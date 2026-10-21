#include "fh2mod.h"

#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

#pragma comment(lib, "winmm.lib")

// Configuration & State
static const FH2ModApi* g_api = nullptr;
static FH2Mod* g_mod = nullptr;

static std::atomic<bool> g_enabled{true};
static std::atomic<float> g_torque_scale{1.6f};      // Spire default TorqueScale = 1.6
static std::atomic<float> g_duration{20.0f};         // Spire default Duration = 20s
static std::atomic<float> g_buildup_time{0.5f};      // Spire default BuildupTime = 0.5s
static std::atomic<float> g_recharge_rate{1.0f};     // 1 second of nitro recharged per second idle

static std::atomic<bool> g_nitro_active{false};
static std::atomic<bool> g_depleted_lockout{false};
static std::atomic<float> g_current_fuel{20.0f};
static std::atomic<float> g_current_boost{1.0f};

static wchar_t g_ini_path[MAX_PATH] = {0};
static wchar_t g_sound_boost_path[MAX_PATH] = {0};
static wchar_t g_sound_stop_path[MAX_PATH] = {0};

// File paths initialization
static void init_paths() {
    if (g_api && g_api->azimuth_dir) {
        swprintf_s(g_ini_path, MAX_PATH, L"%s\\fh2-mods\\FnFNitro.ini", g_api->azimuth_dir);
        swprintf_s(g_sound_boost_path, MAX_PATH, L"%s\\FnFNitro\\nos_boost.wav", g_api->azimuth_dir);
        swprintf_s(g_sound_stop_path, MAX_PATH, L"%s\\FnFNitro\\nos_stop.wav", g_api->azimuth_dir);
    }
}

// INI Persistence
static void load_config() {
    if (g_ini_path[0] == L'\0') return;

    int en = GetPrivateProfileIntW(L"Settings", L"NitroEnabled", 1, g_ini_path);
    g_enabled.store(en != 0);

    wchar_t buf[64];
    if (GetPrivateProfileStringW(L"Settings", L"TorqueScale", L"1.6", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 1.0f && val <= 3.0f) g_torque_scale.store(val);
    }
    if (GetPrivateProfileStringW(L"Settings", L"Duration", L"20.0", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 5.0f && val <= 60.0f) {
            g_duration.store(val);
            g_current_fuel.store(val);
        }
    }
    if (GetPrivateProfileStringW(L"Settings", L"BuildupTime", L"0.5", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 0.1f && val <= 2.0f) g_buildup_time.store(val);
    }
    if (GetPrivateProfileStringW(L"Settings", L"RechargeRate", L"1.0", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 0.0f && val <= 5.0f) g_recharge_rate.store(val);
    }
}

static void save_config() {
    if (g_ini_path[0] == L'\0') return;

    wchar_t buf[64];
    WritePrivateProfileStringW(L"Settings", L"NitroEnabled", g_enabled.load() ? L"1" : L"0", g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_torque_scale.load());
    WritePrivateProfileStringW(L"Settings", L"TorqueScale", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.1f", g_duration.load());
    WritePrivateProfileStringW(L"Settings", L"Duration", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_buildup_time.load());
    WritePrivateProfileStringW(L"Settings", L"BuildupTime", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_recharge_rate.load());
    WritePrivateProfileStringW(L"Settings", L"RechargeRate", buf, g_ini_path);
}

// Physics Hook: CCarDynamics::UpdateDynamics (RVA 0x12C1EA0 in Anthem)
// Main per-tick vehicle simulation loop for all vehicles in the game
typedef bool (*CCarDynamics_UpdateDynamicsFn)(void* this_ptr, void* rdx, void* r8, uint32_t r9, float dt);
static CCarDynamics_UpdateDynamicsFn g_orig_cardynamics = nullptr;

static std::atomic<uint64_t> g_physics_frames{0};
static std::atomic<uint64_t> g_boosted_frames{0};
static std::atomic<uint64_t> g_last_physics_tick{0};
static void* g_player_cardynamics = nullptr;

static bool detour_cardynamics(void* this_ptr, void* rdx, void* r8, uint32_t r9, float dt) {
    bool res = false;
    if (g_orig_cardynamics) {
        res = g_orig_cardynamics(this_ptr, rdx, r8, r9, dt);
    }

    g_physics_frames.fetch_add(1, std::memory_order_relaxed);

    if (!this_ptr) return res;

    __try {
        // In CCarDynamics, [this_ptr + 0x178] == 1 identifies the local player car
        int is_player = *(const int*)((uintptr_t)this_ptr + 0x178);
        if (is_player == 1) {
            g_player_cardynamics = this_ptr;
            g_last_physics_tick.store(GetTickCount64());

            if (g_enabled.load() && g_nitro_active.load()) {
                float boost = g_current_boost.load();
                if (boost > 1.0f) {
                // In CCarDynamics:
                // [this_ptr + 0x40] = Orientation forward vector (float[3])
                // [this_ptr + 0x50] = Vehicle linear velocity vector (float[3])
                const float* fwd = (const float*)((uintptr_t)this_ptr + 0x40);
                float* vel = (float*)((uintptr_t)this_ptr + 0x50);

                float dt_use = (dt > 0.001f && dt < 0.1f) ? dt : 0.01667f;
                // Substantial acceleration force:
                // At boost 3.0x: (3.0 - 1.0) * 22.0f * dt ~ 0.73 m/s/tick (~44 m/s^2 or ~4.5 Gs)
                // At boost 10.0x: (10.0 - 1.0) * 22.0f * dt ~ 3.3 m/s/tick (~198 m/s^2 - extreme nitro rush!)
                float impulse = (boost - 1.0f) * 22.0f * dt_use;
                vel[0] += fwd[0] * impulse;
                vel[1] += fwd[1] * impulse;
                vel[2] += fwd[2] * impulse;

                g_boosted_frames.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safe protection
    }
    return res;
}

// Backfire Hook: Call at RVA 0x13B0DE6 in Anthem (CarPresentation11::UpdateBackfire)
typedef void (*BackfirePrepFn)(void* r15);
static BackfirePrepFn g_orig_backfire_prep = nullptr;

static void detour_backfire_prep(void* r15) {
    if (g_orig_backfire_prep) {
        g_orig_backfire_prep(r15);
    }

    if (!g_enabled.load() || !g_nitro_active.load() || !r15) {
        return;
    }

    __try {
        // r15 is CarPresentation11 + 0x18a0
        uintptr_t car_pres = (uintptr_t)r15 - 0x18a0;
        void* car_dyn = *(void**)(car_pres + 0x10);

        bool is_target = (!g_player_cardynamics || car_dyn == g_player_cardynamics);
        if (!is_target && car_dyn) {
            int flag = *(const int*)((uintptr_t)car_dyn + 0x178);
            if (flag == 1) is_target = true;
        }

        if (is_target) {
            static uint32_t flame_tick = 0;
            flame_tick++;
            uintptr_t exhaust_start = car_pres + 0x15a0;
            int tip_idx = 0;
            for (uintptr_t tip = exhaust_start; tip < (uintptr_t)r15; tip += 0x60, ++tip_idx) {
                *(uint8_t*)(tip + 0x40) = 1; // Ignite exhaust backfire flame
                // Clear existing particle handle every 3 ticks so continuous burst fires
                if ((flame_tick % 3) == 0) {
                    *(uint64_t*)(car_pres + 0x1498 + tip_idx * 0x10) = 0;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safe protection
    }
}

static void* g_game_base = nullptr;

// Speedometer Gauge Hook: HUDStatusSpeedometer tick (RVA 0xAD6070 in Anthem)
// rcx = HUDStatusSpeedometer* speedo, xmm1 = float current_rpm
typedef void (*SpeedoTickFn)(void* speedo, float rpm);
static SpeedoTickFn g_orig_speedo_tick = nullptr;

static void detour_speedo_tick(void* speedo, float rpm) {
    if (g_orig_speedo_tick) {
        g_orig_speedo_tick(speedo, rpm);
    }

    if (!speedo || !g_game_base) return;

    __try {
        float fuel = g_current_fuel.load();
        float duration = g_duration.load();
        float total_ratio = (duration > 0.001f) ? (fuel / duration) : 0.0f;
        if (total_ratio < 0.0f) total_ratio = 0.0f;
        if (total_ratio > 1.0f) total_ratio = 1.0f;

        // Sequential segments (Top segment drains first, down to bottom segment):
        // Segment 1 (bottom: 0.00 to 0.25)
        // Segment 2:         0.25 to 0.50
        // Segment 3:         0.50 to 0.75
        // Segment 4 (top:    0.75 to 1.00)
        auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
        float s1 = clamp01((total_ratio - 0.00f) / 0.25f);
        float s2 = clamp01((total_ratio - 0.25f) / 0.25f);
        float s3 = clamp01((total_ratio - 0.50f) / 0.25f);
        float s4 = clamp01((total_ratio - 0.75f) / 0.25f);

        // Tip glow must remain 0.0f because TipGlowFade > 0 triggers a Durango radial shader
        // defect where the entire 300x300 rotated bounding quad renders as solid white.
        float g1 = 0.0f, g2 = 0.0f, g3 = 0.0f, g4 = 0.0f;

        // NOTE: We deliberately do NOT touch [speedo + 0x14c] (GaugeSmallDashAmount).
        // GaugeSmallDashAmount and GaugeLargeDashAmount are authentic RPM tick mark intervals
        // calculated by the game engine for the gauge mesh dividers.

        // Drive the 4 NOS segments (and active tip glow) via RPMGaugeNumbers[17..20] XPos/YPos.
        // Elements 17..20 are unused by the RPM dial (cars don't rev to 17k+ RPM) and their
        // XPos and YPos properties are native AVUI floats that bind flawlessly in XAML.
        void* arr_wrapper = *(void**)((uintptr_t)speedo + 0x150);
        if (arr_wrapper) {
            void* p50 = *(void**)((uintptr_t)arr_wrapper + 0x50);
            if (p50) {
                void* p28 = *(void**)((uintptr_t)p50 + 0x28);
                if (p28) {
                    void** items = *(void***)((uintptr_t)p28 + 0x28);
                    if (items) {
                        typedef void* (*GetPropFn)();
                        typedef void (*RaiseItemPropFn)(void* item, void* prop);

                        GetPropFn get_xpos_prop = (GetPropFn)((uintptr_t)g_game_base + 0xADD560);
                        GetPropFn get_ypos_prop = (GetPropFn)((uintptr_t)g_game_base + 0xADD620);
                        RaiseItemPropFn raise_item_prop = (RaiseItemPropFn)((uintptr_t)g_game_base + 0xAD1A80);

                        void* prop_xpos = get_xpos_prop();
                        void* prop_ypos = get_ypos_prop();

                        struct SegTarget {
                            int idx;
                            float ratio;
                            float glow;
                        } targets[4] = {
                            {17, s1, g1}, // Segment 1 (Bottom)
                            {18, s2, g2}, // Segment 2
                            {19, s3, g3}, // Segment 3
                            {20, s4, g4}  // Segment 4 (Top)
                        };

                        for (const auto& t : targets) {
                            void* item = items[t.idx];
                            if (!item) continue;

                            float* p_x = (float*)((uintptr_t)item + 0x38);
                            if (fabsf(*p_x - t.ratio) > 0.0005f) {
                                *p_x = t.ratio;
                                if (prop_xpos) raise_item_prop(item, prop_xpos);
                            }

                            float* p_y = (float*)((uintptr_t)item + 0x3c);
                            if (fabsf(*p_y - t.glow) > 0.005f) {
                                *p_y = t.glow;
                                if (prop_ypos) raise_item_prop(item, prop_ypos);
                            }
                        }
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}


// Settings callbacks
static void on_setting_changed(void* user, const char* key, int value) {
    (void)user;
    if (strcmp(key, "NitroEnabled") == 0) {
        g_enabled.store(value != 0);
        save_config();
        if (g_api && g_mod) {
            g_api->log(g_mod, value ? "Nitro enabled" : "Nitro disabled");
        }
    }
}

static void on_slider_changed(void* user, const char* key, float value) {
    (void)user;
    char buf[128];
    if (strcmp(key, "TorqueScale") == 0) {
        g_torque_scale.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "TorqueScale set to %.2fx (saved)", value);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "Duration") == 0) {
        g_duration.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "Nitro Duration set to %.1fs (saved)", value);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "BuildupTime") == 0) {
        g_buildup_time.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "BuildupTime set to %.2fs (saved)", value);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "RechargeRate") == 0) {
        g_recharge_rate.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "RechargeRate set to %.2fx (saved)", value);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    }
}

// Controller Haptics
static void set_rumble(WORD left_motor, WORD right_motor) {
    XINPUT_VIBRATION vib{};
    vib.wLeftMotorSpeed = left_motor;
    vib.wRightMotorSpeed = right_motor;
    for (DWORD i = 0; i < 4; ++i) {
        XInputSetState(i, &vib);
    }
}

// Audio Control
static void start_nitro_audio() {
    if (g_sound_boost_path[0] != L'\0') {
        PlaySoundW(g_sound_boost_path, NULL, SND_ASYNC | SND_FILENAME | SND_LOOP | SND_NODEFAULT);
    }
}

static void stop_nitro_audio() {
    PlaySoundW(NULL, NULL, 0);
    if (g_sound_stop_path[0] != L'\0') {
        PlaySoundW(g_sound_stop_path, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
    }
}

// Background thread: handles gamepad input polling, state-machine, audio & haptics
static void nitro_worker_thread() {
    if (!g_api || !g_mod) return;

    g_api->log(g_mod, "Worker thread waiting for game code...");
    void* base = g_api->wait_for_code(0xFFFFFFFF);
    if (!base) {
        g_api->log(g_mod, "Game code not found (timed out).");
        return;
    }

    char log_buf[128];
    snprintf(log_buf, sizeof(log_buf), "Game mapped at base %p. Installing physics boost hook...", base);
    g_api->log(g_mod, log_buf);
    g_game_base = base;

    // Hook CCarDynamics::UpdateDynamics (RVA 0x12C1EA0)
    void* target_func = (void*)((uintptr_t)base + 0x12C1EA0);
    FH2Hook* hook = g_api->hook_function(g_mod, "CCarDynamics::UpdateDynamics", target_func, (void*)detour_cardynamics, (void**)&g_orig_cardynamics);
    if (hook) {
        if (g_api->enable_hooks(g_mod, hook)) {
            g_api->log(g_mod, "CCarDynamics::UpdateDynamics successfully hooked for physical nitro acceleration!");
        } else {
            g_api->log(g_mod, "Failed to enable CCarDynamics hook.");
        }
    } else {
        g_api->log(g_mod, "Failed to create CCarDynamics hook.");
    }

    // Hook CarPresentation11 exhaust backfire prep (RVA 0x13B0DE6)
    void* backfire_call = (void*)((uintptr_t)base + 0x13B0DE6);
    FH2Hook* hook_bf = g_api->hook_call(g_mod, "CarPresentation11::BackfirePrep", backfire_call, (void*)detour_backfire_prep, (void**)&g_orig_backfire_prep);
    if (hook_bf) {
        if (g_api->enable_hooks(g_mod, hook_bf)) {
            g_api->log(g_mod, "CarPresentation11 exhaust backfire flame hook enabled successfully!");
        } else {
            g_api->log(g_mod, "Failed to enable CarPresentation11 backfire hook.");
        }
    } else {
        g_api->log(g_mod, "Failed to create CarPresentation11 backfire hook.");
    }

    // Hook HUDStatusSpeedometer gauge update (RVA 0xAD6070) for dynamic NOS gauge fuel display
    void* speedo_call = (void*)((uintptr_t)base + 0xad6070);
    FH2Hook* hook_sp = g_api->hook_function(g_mod, "HUDStatusSpeedometer::UpdateGauges", speedo_call, (void*)detour_speedo_tick, (void**)&g_orig_speedo_tick);
    if (hook_sp) {
        if (g_api->enable_hooks(g_mod, hook_sp)) {
            g_api->log(g_mod, "HUDStatusSpeedometer hook enabled for dynamic NOS gauge decrease/refill!");
        } else {
            g_api->log(g_mod, "Failed to enable HUDStatusSpeedometer hook.");
        }
    } else {
        g_api->log(g_mod, "Failed to create HUDStatusSpeedometer hook.");
    }

    auto last_time = std::chrono::steady_clock::now();
    float active_duration = 0.0f;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz tick

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;
        if (dt > 0.1f) dt = 0.1f; // Clamp delta time spike

        if (!g_enabled.load()) {
            if (g_nitro_active.load()) {
                g_nitro_active.store(false);
                set_rumble(0, 0);
                stop_nitro_audio();
            }
            g_current_boost.store(1.0f);
            continue;
        }

        // Poll all 4 controller ports + Keyboard for LB (Left Shoulder)
        bool button_pressed = false;
        for (DWORD user_idx = 0; user_idx < 4; ++user_idx) {
            XINPUT_STATE state{};
            if (XInputGetState(user_idx, &state) == ERROR_SUCCESS) {
                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0) {
                    button_pressed = true;
                    break;
                }
            }
        }

        // Support Left Shift or 'N' for keyboard players
        if (!button_pressed) {
            if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState('N') & 0x8000)) {
                button_pressed = true;
            }
        }

        // Only allow nitro activation while car physics is actively simulating
        // (Prevents nitro triggering in Pause Menu, Map, Garage, or Loading screens)
        uint64_t now_ms = GetTickCount64();
        bool physics_active = (now_ms - g_last_physics_tick.load()) < 200;
        if (!physics_active) {
            button_pressed = false;
        }

        float fuel = g_current_fuel.load();
        float max_duration = g_duration.load();
        float target_scale = g_torque_scale.load();
        float ramp_time = g_buildup_time.load();

        if (g_depleted_lockout.load()) {
            // Require releasing LB AND recharging at least 2.0s before clearing lockout
            if (!button_pressed && fuel >= 2.0f) {
                g_depleted_lockout.store(false);
            }
        }

        if (button_pressed && !g_depleted_lockout.load() && fuel > 0.0f) {
            // Activating / Active
            if (!g_nitro_active.load()) {
                g_nitro_active.store(true);
                active_duration = 0.0f;
                start_nitro_audio();
                g_api->log(g_mod, "Fast & Furious Nitro engaged (LB pressed)!");
            }

            active_duration += dt;
            fuel = (std::max)(0.0f, fuel - dt);
            g_current_fuel.store(fuel);

            // Linear ramp-up from 1.0 to target_scale over buildup_time
            float ramp = (ramp_time > 0.001f) ? (std::min)(1.0f, active_duration / ramp_time) : 1.0f;
            float boost = 1.0f + (target_scale - 1.0f) * ramp;
            g_current_boost.store(boost);

            // Haptic rumble
            WORD rumble_strength = static_cast<WORD>(28000 * ramp);
            WORD high_freq = static_cast<WORD>(42000 * ramp);
            set_rumble(rumble_strength, high_freq);

            if (fuel <= 0.0f) {
                g_depleted_lockout.store(true);
                g_nitro_active.store(false);
                g_current_boost.store(1.0f);
                set_rumble(0, 0);
                stop_nitro_audio();
                g_api->log(g_mod, "Nitro tank depleted.");
            }
        } else {
            // Idle / Recharging
            if (g_nitro_active.load()) {
                g_nitro_active.store(false);
                g_current_boost.store(1.0f);
                set_rumble(0, 0);
                stop_nitro_audio();
                active_duration = 0.0f;
            }

            // Recharge over time
            float rate = g_recharge_rate.load();
            if (fuel < max_duration && rate > 0.0f) {
                fuel = (std::min)(max_duration, fuel + dt * rate);
                g_current_fuel.store(fuel);
            }
        }
    }
}

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    g_api = api;

    FH2ModInfo info{};
    info.size             = sizeof(info);
    info.name             = "FnFNitro";
    info.version          = FNFNITRO_VERSION;
    info.github_repo      = nullptr;
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = 0;
    info.description      = "Fast & Furious Nitro system ported to Forza Horizon 2 (All Cars - LB to activate)";

    g_mod = api->register_mod(&info);
    if (!g_mod) return 1;

    api->log(g_mod, "FnFNitro initializing (LB to boost)...");

    // Initialize paths & load persistent settings from FnFNitro.ini
    init_paths();
    load_config();

    // Register In-Game Settings (Pause > Settings > MODS) with loaded values
    if (api->api_version >= 4) {
        FH2Setting s_enable{};
        s_enable.size        = sizeof(s_enable);
        s_enable.key         = "NitroEnabled";
        s_enable.label       = "Nitro System (LB)";
        s_enable.description = "Enable or disable Fast & Furious Nitro. Press LB while driving to boost!";
        s_enable.value       = g_enabled.load() ? 1 : 0;
        s_enable.on_change   = on_setting_changed;
        api->add_setting(g_mod, &s_enable);

        if (api->api_version >= 5) {
            FH2Setting s_torque{};
            s_torque.size          = sizeof(s_torque);
            s_torque.key           = "TorqueScale";
            s_torque.label         = "Nitro Boost Multiplier";
            s_torque.description   = "Multiplies forward acceleration while Nitro is active (Spire default: 1.6x).";
            s_torque.kind          = FH2_SETTING_SLIDER;
            s_torque.min           = 1.0f;
            s_torque.max           = 3.0f;
            s_torque.step          = 0.1f;
            s_torque.slider_value  = g_torque_scale.load();
            s_torque.display_scale = 1.0f;
            s_torque.decimals      = 1;
            s_torque.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_torque);

            FH2Setting s_dur{};
            s_dur.size          = sizeof(s_dur);
            s_dur.key           = "Duration";
            s_dur.label         = "Nitro Tank Capacity (sec)";
            s_dur.description   = "Total seconds of continuous boost available in a full tank (Spire default: 20s).";
            s_dur.kind          = FH2_SETTING_SLIDER;
            s_dur.min           = 5.0f;
            s_dur.max           = 60.0f;
            s_dur.step          = 5.0f;
            s_dur.slider_value  = g_duration.load();
            s_dur.display_scale = 1.0f;
            s_dur.decimals      = 0;
            s_dur.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_dur);

            FH2Setting s_buildup{};
            s_buildup.size          = sizeof(s_buildup);
            s_buildup.key           = "BuildupTime";
            s_buildup.label         = "Nitro Buildup Time (sec)";
            s_buildup.description   = "Time taken to ramp up to full boost torque (Spire default: 0.5s).";
            s_buildup.kind          = FH2_SETTING_SLIDER;
            s_buildup.min           = 0.1f;
            s_buildup.max           = 2.0f;
            s_buildup.step          = 0.1f;
            s_buildup.slider_value  = g_buildup_time.load();
            s_buildup.display_scale = 1.0f;
            s_buildup.decimals      = 1;
            s_buildup.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_buildup);

            FH2Setting s_recharge{};
            s_recharge.size          = sizeof(s_recharge);
            s_recharge.key           = "RechargeRate";
            s_recharge.label         = "Nitro Recharge Rate";
            s_recharge.description   = "Seconds of nitro restored per second while not boosting.";
            s_recharge.kind          = FH2_SETTING_SLIDER;
            s_recharge.min           = 0.0f;
            s_recharge.max           = 5.0f;
            s_recharge.step          = 0.2f;
            s_recharge.slider_value  = g_recharge_rate.load();
            s_recharge.display_scale = 1.0f;
            s_recharge.decimals      = 1;
            s_recharge.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_recharge);
        }
    }

    // Ensure all cars support exhaust backfire flames in gamedbRC
    if (api->api_version >= 2 && api->add_sql) {
        api->add_sql(g_mod, "UPDATE BackfireLevels SET BackfireLevel = 3;");
    }

    // Launch background worker thread
    std::thread(nitro_worker_thread).detach();

    api->log(g_mod, "FnFNitro initialized successfully.");
    return 0;
}
