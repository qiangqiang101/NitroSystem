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
static std::atomic<uint64_t> g_last_speedo_tick{0}; // HUD speedometer only ticks while driving with the HUD up

// Telling the player's car from AI racers and traffic. Every simulated car has [+0x178] == 1, so that
// flag can't. The world manager (CWorldCollObjMan::Update, RVA 0x384C00, first argument) holds the
// player car's position at +0x4F0. We learn which offset inside CCarDynamics holds a world position by
// finding the one that keeps landing on it, then treat the nearest car to it as the player's.
static float g_player_pos[3] = {0.0f, 0.0f, 0.0f};
static std::atomic<uint64_t> g_player_pos_tick{0};

typedef void (*MgrUpdateFn)(uint8_t* mgr, float a, float b);
static MgrUpdateFn g_orig_mgr_update = nullptr;

static void detour_mgr_update(uint8_t* mgr, float a, float b) {
    if (g_orig_mgr_update) {
        g_orig_mgr_update(mgr, a, b);
    }
    if (!mgr) return;

    __try {
        const float* p = (const float*)(mgr + 0x4F0);
        if (std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2])) {
            g_player_pos[0] = p[0];
            g_player_pos[1] = p[1];
            g_player_pos[2] = p[2];
            g_player_pos_tick.store(GetTickCount64());
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static const int kScanBytes = 0x400;
static const float kCalibRadius2 = 8.0f * 8.0f;   // player position can lag the car by a frame
static const float kPlayerRadius2 = 25.0f * 25.0f;
static std::atomic<int> g_pos_offset{-1};          // offset of the world position inside CCarDynamics
static std::atomic<int> g_calib_report{0};         // 0 none, 1 learned, 2 failed (logged by the worker)
static std::atomic<int> g_calib_best_hits{0};

struct SeenCar {
    void* ptr;
    uint64_t tick;
};
static SeenCar g_seen[32];

static void note_car(void* car, uint64_t now) {
    SeenCar* slot = &g_seen[0];
    for (auto& s : g_seen) {
        if (s.ptr == car) { slot = &s; break; }
        if (s.tick < slot->tick) slot = &s;
    }
    slot->ptr = car;
    slot->tick = now;
}

static bool car_dist2(const void* car, int off, float* out) {
    const float* v = (const float*)((uintptr_t)car + off);
    float dx = v[0] - g_player_pos[0], dy = v[1] - g_player_pos[1], dz = v[2] - g_player_pos[2];
    float d2 = dx * dx + dy * dy + dz * dz;
    if (!std::isfinite(d2)) return false;
    *out = d2;
    return true;
}

// Only runs on the physics thread: finds the offset whose float triple keeps matching the player position.
static void calibrate_position_offset(void* car, uint64_t now) {
    static uint32_t hits[kScanBytes / 4];
    static int samples = 0;
    if (now - g_player_pos_tick.load() > 200) return;

    for (int i = 0; i < kScanBytes / 4 - 2; ++i) {
        float d2;
        if (car_dist2(car, i * 4, &d2) && d2 < kCalibRadius2) ++hits[i];
    }
    if (++samples < 4000) return;

    int best = 0;
    for (int i = 1; i < kScanBytes / 4 - 2; ++i) {
        if (hits[i] > hits[best]) best = i;
    }
    g_calib_best_hits.store((int)hits[best]);
    if (hits[best] >= 150) {
        g_pos_offset.store(best * 4);
        g_calib_report.store(1);
    } else {
        g_calib_report.store(2);
        samples = 0;
        for (auto& h : hits) h = 0;
    }
}

// True if this car is the one nearest the player position. Until the offset is learned, falls back to
// the old behaviour (any simulated car).
static bool is_player_car(void* car, uint64_t now) {
    int off = g_pos_offset.load();
    if (off < 0) return true;
    if (now - g_player_pos_tick.load() > 200) return false;

    float mine;
    if (!car_dist2(car, off, &mine) || mine > kPlayerRadius2) return false;
    for (const auto& s : g_seen) {
        if (!s.ptr || s.ptr == car || now - s.tick > 150) continue;
        float other;
        if (car_dist2(s.ptr, off, &other) && other < mine) return false;
    }
    return true;
}

// Keeping the boost on the ground. Per-wheel contact isn't known, so: learn which world axis is "up",
// treat a free-fall signature on it as airborne, and push along the horizontal part of the nose only.
static std::atomic<int> g_up_axis{-1};     // 0..2 once learned
static std::atomic<int> g_up_report{0};    // 1 = just learned (logged by the worker)

// The vertical axis is the one the player's position moves least along, since roads are flatter than
// they are long. Vote once a second while the car is covering ground.
static void learn_up_axis(const void* car, uint64_t now) {
    int off = g_pos_offset.load();
    if (off < 0 || g_up_axis.load() >= 0) return;

    static uint64_t win_start = 0;
    static float lo[3], hi[3];
    static int votes[3] = {0, 0, 0};

    const float* p = (const float*)((uintptr_t)car + off);
    if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) return;

    if (win_start == 0 || now - win_start > 1000) {
        if (win_start != 0) {
            float r[3] = {hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]};
            int flattest = 0;
            for (int i = 1; i < 3; ++i) if (r[i] < r[flattest]) flattest = i;
            float mid = 0.0f; // second largest range = the smaller of the two horizontal axes
            for (int i = 0; i < 3; ++i) {
                if (i == flattest) continue;
                if (mid == 0.0f || r[i] < mid) mid = r[i];
            }
            if (mid > 40.0f && r[flattest] < 0.5f * mid) ++votes[flattest];

            int total = votes[0] + votes[1] + votes[2];
            if (total >= 3) {
                int best = 0;
                for (int i = 1; i < 3; ++i) if (votes[i] > votes[best]) best = i;
                if (votes[best] * 3 >= total * 2) {
                    g_up_axis.store(best);
                    g_up_report.store(1);
                }
            }
        }
        win_start = now;
        for (int i = 0; i < 3; ++i) lo[i] = hi[i] = p[i];
    } else {
        for (int i = 0; i < 3; ++i) {
            if (p[i] < lo[i]) lo[i] = p[i];
            if (p[i] > hi[i]) hi[i] = p[i];
        }
    }
}

// Airborne = vertical acceleration near -g for a couple of ticks in a row. Called every tick for the
// player's car, with the velocity as the physics left it (before our own push, which is horizontal).
static bool update_grounded(void* car, const float* vel, float dt, uint64_t now) {
    int up = g_up_axis.load();
    if (up < 0) return true;

    static void* tracked = nullptr;
    static float prev_vy = 0.0f;
    static uint64_t prev_t = 0;
    static int fall_ticks = 0;
    static int grace = 0;

    float vy = vel[up];
    float dt_use = (dt > 0.001f && dt < 0.1f) ? dt : 0.01667f;
    if (car != tracked || now - prev_t > 100) {
        tracked = car;
        fall_ticks = 0;
        grace = 0;
    } else {
        float ay = (vy - prev_vy) / dt_use;
        fall_ticks = (ay < -6.0f) ? fall_ticks + 1 : 0;
    }
    prev_vy = vy;
    prev_t = now;

    if (fall_ticks >= 2) grace = 6;        // airborne; keep off the throttle a moment after landing too
    else if (grace > 0) --grace;
    return fall_ticks < 2 && grace == 0;
}

static bool detour_cardynamics(void* this_ptr, void* rdx, void* r8, uint32_t r9, float dt) {
    bool res = false;
    if (g_orig_cardynamics) {
        res = g_orig_cardynamics(this_ptr, rdx, r8, r9, dt);
    }

    g_physics_frames.fetch_add(1, std::memory_order_relaxed);

    if (!this_ptr) return res;

    __try {
        // In CCarDynamics, [this_ptr + 0x178] == 1 marks a simulated car (the player's, AI racers and traffic)
        int is_sim = *(const int*)((uintptr_t)this_ptr + 0x178);
        if (is_sim == 1) {
            uint64_t now = GetTickCount64();
            g_last_physics_tick.store(now);
            note_car(this_ptr, now);
            if (g_pos_offset.load() < 0) calibrate_position_offset(this_ptr, now);

            bool mine = is_player_car(this_ptr, now);
            if (mine) g_player_cardynamics = this_ptr;

            // In CCarDynamics:
            // [this_ptr + 0x40] = Orientation forward vector (float[3])
            // [this_ptr + 0x50] = Vehicle linear velocity vector (float[3])
            const float* fwd = (const float*)((uintptr_t)this_ptr + 0x40);
            float* vel = (float*)((uintptr_t)this_ptr + 0x50);

            bool grounded = true;
            if (mine) {
                learn_up_axis(this_ptr, now);
                grounded = update_grounded(this_ptr, vel, dt, now);
            }

            if (mine && grounded && g_enabled.load() && g_nitro_active.load()) {
                float boost = g_current_boost.load();
                if (boost > 1.0f) {
                    // Push along the horizontal part of the nose so a pitched-up car isn't thrown skyward
                    float dir[3] = {fwd[0], fwd[1], fwd[2]};
                    bool apply = true;
                    int up = g_up_axis.load();
                    if (up >= 0) {
                        dir[up] = 0.0f;
                        float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
                        if (len < 0.3f) {
                            apply = false; // nose nearly vertical
                        } else {
                            dir[0] /= len; dir[1] /= len; dir[2] /= len;
                        }
                    }

                    if (apply) {
                        float dt_use = (dt > 0.001f && dt < 0.1f) ? dt : 0.01667f;
                        // Substantial acceleration force:
                        // At boost 3.0x: (3.0 - 1.0) * 22.0f * dt ~ 0.73 m/s/tick (~44 m/s^2 or ~4.5 Gs)
                        // At boost 10.0x: (10.0 - 1.0) * 22.0f * dt ~ 3.3 m/s/tick (~198 m/s^2 - extreme nitro rush!)
                        float impulse = (boost - 1.0f) * 22.0f * dt_use;
                        vel[0] += dir[0] * impulse;
                        vel[1] += dir[1] * impulse;
                        vel[2] += dir[2] * impulse;

                        g_boosted_frames.fetch_add(1, std::memory_order_relaxed);
                    }
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

        bool is_target = (car_dyn && car_dyn == g_player_cardynamics);
        if (!is_target && car_dyn && g_pos_offset.load() < 0) {
            // Player's car not identified yet: fall back to any simulated car
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

    g_last_speedo_tick.store(GetTickCount64());

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

        // Active nitrous glowing tip cap (follows the exact leading edge of the draining NOS bar):
        bool active = g_nitro_active.load();
        float glow_scale = 0.0f;
        float leading_angle = 55.87f;

        if (s4 > 0.001f) {
            leading_angle = 118.57f + s4 * 20.26f;
        } else if (s3 > 0.001f) {
            leading_angle = 97.67f + s3 * 20.26f;
        } else if (s2 > 0.001f) {
            leading_angle = 76.77f + s2 * 20.26f;
        } else {
            leading_angle = 55.87f + s1 * 20.26f;
        }

        // The glow passes are narrow RadialBar tracks (AngleAmount sets the width) centered on the
        // rotation angle, so Ratio 1.0 fills the whole track and YPos is the leading edge. The
        // XAML offsets slide each pass back so it ends on the edge.
        // The widest pass trails kGlowTrail degrees behind the edge. Near empty, park the glow where
        // its tail meets the bar start and shrink Ratio so it fades out instead of overhanging.
        const float kBarStart = 55.87f;
        const float kGlowTrail = 12.0f;
        if (active && total_ratio > 0.005f) {
            float room = leading_angle - kBarStart;
            if (room >= kGlowTrail) {
                glow_scale = 1.0f;
            } else {
                glow_scale = room / kGlowTrail;
                leading_angle = kBarStart + kGlowTrail;
            }
        }

        // Digital HUD: one continuous tip glow slides along the whole 245px bar (4 segments of 59px
        // with 3px gaps, so a 62px pitch). Segment 1's YPos carries the glow's right edge as an
        // offset from the bar end, or -1000 to park it off the bar. The analog HUD ignores it.
        const float kBarWidth = 245.0f;
        const float kSegPitch = 62.0f;
        const float kSegWidth = 59.0f;
        float tip_x = -1000.0f;
        if (active && total_ratio > 0.005f) {
            const float seg_fill[4] = {s1, s2, s3, s4};
            int lead = (s4 > 0.001f) ? 3 : (s3 > 0.001f) ? 2 : (s2 > 0.001f) ? 1 : 0;
            tip_x = lead * kSegPitch + kSegWidth * seg_fill[lead] - kBarWidth;
        }

        // Drive the 4 NOS segments via RPMGaugeNumbers[17..20] and the glowing tip cap on [16].
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
                            float x;
                            float y;
                        } targets[5] = {
                            {17, s1, tip_x},                 // Segment 1 (Bottom); Y = digital glow offset
                            {18, s2, 0.0f},                  // Segment 2
                            {19, s3, 0.0f},                  // Segment 3
                            {20, s4, 0.0f},                  // Segment 4 (Top)
                            {16, glow_scale, leading_angle}  // Active Glowing Tip Cap (X=Ratio, Y=Angle)
                        };

                        for (const auto& t : targets) {
                            void* item = items[t.idx];
                            if (!item) continue;

                            float* p_x = (float*)((uintptr_t)item + 0x38);
                            if (fabsf(*p_x - t.x) > 0.0005f) {
                                *p_x = t.x;
                                if (prop_xpos) raise_item_prop(item, prop_xpos);
                            }

                            float* p_y = (float*)((uintptr_t)item + 0x3c);
                            if (fabsf(*p_y - t.y) > 0.005f) {
                                *p_y = t.y;
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

    // Hook CWorldCollObjMan::Update (RVA 0x384C00) to read the player's position. WreckingBall hooks it
    // too; the platform chains hooks on the same function.
    void* mgr_update = (void*)((uintptr_t)base + 0x384C00);
    FH2Hook* hook_mgr = g_api->hook_function(g_mod, "CWorldCollObjMan::Update", mgr_update, (void*)detour_mgr_update, (void**)&g_orig_mgr_update);
    if (hook_mgr && g_api->enable_hooks(g_mod, hook_mgr)) {
        g_api->log(g_mod, "CWorldCollObjMan::Update hooked to find the player's car.");
    } else {
        g_api->log(g_mod, "Failed to hook CWorldCollObjMan::Update; nitro will boost every simulated car.");
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

        // Poll all 4 controller ports for LB (Left Shoulder)
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

        // Only run while the player is actually driving: car physics is simulating (not Pause Menu,
        // Map or Loading) and the HUD speedometer is ticking (not Garage or a car meet lobby).
        uint64_t now_ms = GetTickCount64();
        bool physics_active = (now_ms - g_last_physics_tick.load()) < 200;
        bool driving = physics_active && (now_ms - g_last_speedo_tick.load()) < 200;
        if (!driving) {
            // Nothing is simulated: no nitro, and the tank neither drains nor recharges.
            if (g_nitro_active.load()) {
                g_nitro_active.store(false);
                g_current_boost.store(1.0f);
                set_rumble(0, 0);
                stop_nitro_audio();
                active_duration = 0.0f;
            }
            continue;
        }

        if (g_up_report.exchange(0) == 1) {
            snprintf(log_buf, sizeof(log_buf), "Vertical axis learned: %c. Nitro now pushes horizontally and only on the ground.",
                     "XYZ"[g_up_axis.load()]);
            g_api->log(g_mod, log_buf);
        }

        int report = g_calib_report.exchange(0);
        if (report == 1) {
            snprintf(log_buf, sizeof(log_buf), "Player car located: position at CCarDynamics+0x%X (%d hits). Nitro now only boosts your car.",
                     g_pos_offset.load(), g_calib_best_hits.load());
            g_api->log(g_mod, log_buf);
        } else if (report == 2) {
            snprintf(log_buf, sizeof(log_buf), "Could not locate the player car yet (best offset only %d hits); retrying.",
                     g_calib_best_hits.load());
            g_api->log(g_mod, log_buf);
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
    info.name             = "Fast & Furious NOS in Forza Horizon 2";
    info.version          = FNFNITRO_VERSION;
    info.github_repo      = nullptr;
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = 0;
    info.description      = "Fast & Furious Nitro system ported to Forza Horizon 2 (All Cars - LB to activate)";

    g_mod = api->register_mod(&info);
    if (!g_mod) return 1;

    api->log(g_mod, "Fast & Furious NOS initializing (LB to boost)...");

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
            s_torque.max           = 2.0f;
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
            s_dur.step          = 1.0f;
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
            s_recharge.max           = 1.0f;
            s_recharge.step          = 0.1f;
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

    api->log(g_mod, "Fast & Furious NOS initialized successfully.");
    return 0;
}
