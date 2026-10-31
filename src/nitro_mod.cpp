#include "fh2mod.h"
#include "fh2game.h"
#include "legacy_files.h"

#include <windows.h>
#include <intrin.h>
#include <mmsystem.h>
#include <xinput.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winmm.lib")

// Configuration & State
static const FH2ModApi* g_api = nullptr;
static FH2Mod* g_mod = nullptr;
static void* g_game_base = nullptr;

static std::atomic<bool> g_enabled{true};
static std::atomic<bool> g_debug{false};          // on-screen debug overlay (Mods page)
static std::atomic<bool> g_backfire_all_cars{true};  // "Backfire on All Cars": applied to the database at startup (needs a restart)
static std::atomic<bool> g_rumble{true};             // "Controller Rumble" while nitro burns
static std::atomic<bool> g_refund_on_rewind{true};   // "Refund Nitro on Rewind": a rewind gives back the nitro used since the point it returns to
static std::atomic<bool> g_refill_on_start{true};    // "Refill Nitro on Race Start": full tank when the car is put somewhere else
static std::atomic<bool> g_drivatar_nitro{false};    // "Drivatar Nitro": Drivatars burn nitro too, from tanks of their own
static std::atomic<int> g_hud_theme{1};              // "HUD Theme": 0 = Original (yellow NOS bar), 1 = Fast & Furious (cyan); needs a restart
static std::atomic<float> g_torque_scale{1.6f};      // Spire default TorqueScale = 1.6
static std::atomic<float> g_duration{20.0f};         // Spire default Duration = 20s
static std::atomic<float> g_buildup_time{0.5f};      // Spire default BuildupTime = 0.5s
static std::atomic<float> g_recharge_rate{0.2f};     // 0.2 seconds of nitro recharged per second idle
static std::atomic<float> g_camera_shake{0.65f};     // camera shake while nitro burns, 0..1
static std::atomic<float> g_sound_volume{0.5f};      // boost and stop sounds, 0..1 (the game's own volume sliders don't reach them)
static std::atomic<float> g_ai_sound_volume{0.25f};  // the Drivatars' boost and stop sounds, 0..1

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

// Nitro button binding (NitroButton in the ini). Triggers are analog, so they get their own flags.
// `label` is what the Mods page's choice row shows.
struct ButtonName { const wchar_t* name; WORD mask; const char* label; };
static constexpr WORD BTN_LT = 0x8001;
static constexpr WORD BTN_RT = 0x8002;
static const ButtonName k_buttons[] = {
    {L"LB", XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"}, {L"RB", XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"},
    {L"A", XINPUT_GAMEPAD_A, "A"}, {L"B", XINPUT_GAMEPAD_B, "B"}, {L"X", XINPUT_GAMEPAD_X, "X"}, {L"Y", XINPUT_GAMEPAD_Y, "Y"},
    {L"LS", XINPUT_GAMEPAD_LEFT_THUMB, "LS Click"}, {L"RS", XINPUT_GAMEPAD_RIGHT_THUMB, "RS Click"},
    {L"DPadUp", XINPUT_GAMEPAD_DPAD_UP, "D-Pad Up"}, {L"DPadDown", XINPUT_GAMEPAD_DPAD_DOWN, "D-Pad Down"},
    {L"DPadLeft", XINPUT_GAMEPAD_DPAD_LEFT, "D-Pad Left"}, {L"DPadRight", XINPUT_GAMEPAD_DPAD_RIGHT, "D-Pad Right"},
    {L"Back", XINPUT_GAMEPAD_BACK, "Back"}, {L"Start", XINPUT_GAMEPAD_START, "Start"},
    {L"LT", BTN_LT, "LT"}, {L"RT", BTN_RT, "RT"},
};
static constexpr int k_button_count = (int)(sizeof(k_buttons) / sizeof(k_buttons[0]));
static std::atomic<WORD> g_nitro_button{XINPUT_GAMEPAD_LEFT_SHOULDER};

static int button_index() {
    for (int i = 0; i < k_button_count; ++i)
        if (k_buttons[i].mask == g_nitro_button.load()) return i;
    return 0;
}

static bool nitro_button_down(const XINPUT_GAMEPAD& pad) {
    WORD b = g_nitro_button.load();
    if (b == BTN_LT) return pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    if (b == BTN_RT) return pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    return (pad.wButtons & b) != 0;
}

// INI Persistence
static void load_config() {
    if (g_ini_path[0] == L'\0') return;

    wchar_t btn[32];
    GetPrivateProfileStringW(L"Settings", L"NitroButton", L"LB", btn, 32, g_ini_path);
    g_nitro_button.store(XINPUT_GAMEPAD_LEFT_SHOULDER);   // unknown names fall back to LB
    for (const auto& b : k_buttons)
        if (_wcsicmp(btn, b.name) == 0) g_nitro_button.store(b.mask);

    int en = GetPrivateProfileIntW(L"Settings", L"NitroEnabled", 1, g_ini_path);
    g_enabled.store(en != 0);
    g_debug.store(GetPrivateProfileIntW(L"Settings", L"DebugOverlay", 0, g_ini_path) != 0);
    g_backfire_all_cars.store(GetPrivateProfileIntW(L"Settings", L"BackfireOnAllCars", 1, g_ini_path) != 0);
    g_refill_on_start.store(GetPrivateProfileIntW(L"Settings", L"RefillOnRaceStart", 1, g_ini_path) != 0);
    g_refund_on_rewind.store(GetPrivateProfileIntW(L"Settings", L"RefundOnRewind", 1, g_ini_path) != 0);
    g_drivatar_nitro.store(GetPrivateProfileIntW(L"Settings", L"DrivatarNitro", 0, g_ini_path) != 0);
    g_rumble.store(GetPrivateProfileIntW(L"Settings", L"Rumble", 1, g_ini_path) != 0);

    wchar_t theme[32];
    GetPrivateProfileStringW(L"Settings", L"HudTheme", L"FF", theme, 32, g_ini_path);
    g_hud_theme.store(_wcsicmp(theme, L"Original") == 0 ? 0 : 1);   // unknown values fall back to FF

    wchar_t buf[64];
    if (GetPrivateProfileStringW(L"Settings", L"TorqueScale", L"1.6", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val > 2.0f) val = 2.0f;                       // same ceiling as the Mods page slider
        if (val >= 1.0f) g_torque_scale.store(val);
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
    if (GetPrivateProfileStringW(L"Settings", L"RechargeRate", L"0.2", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 0.0f && val <= 5.0f) g_recharge_rate.store(val);
    }
    if (GetPrivateProfileStringW(L"Settings", L"SoundVolume", L"0.5", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 0.0f && val <= 1.0f) g_sound_volume.store(val);
    }
    if (GetPrivateProfileStringW(L"Settings", L"DrivatarSoundVolume", L"0.25", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 0.0f && val <= 1.0f) g_ai_sound_volume.store(val);
    }
    if (GetPrivateProfileStringW(L"Settings", L"CameraShake", L"0.65", buf, 64, g_ini_path) > 0) {
        float val = wcstof(buf, nullptr);
        if (val >= 0.0f && val <= 1.0f) g_camera_shake.store(val);
    }
}

static void save_config() {
    if (g_ini_path[0] == L'\0') return;

    wchar_t buf[64];
    for (const auto& b : k_buttons)
        if (b.mask == g_nitro_button.load()) WritePrivateProfileStringW(L"Settings", L"NitroButton", b.name, g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"NitroEnabled", g_enabled.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"DebugOverlay", g_debug.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"BackfireOnAllCars", g_backfire_all_cars.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"RefillOnRaceStart", g_refill_on_start.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"RefundOnRewind", g_refund_on_rewind.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"DrivatarNitro", g_drivatar_nitro.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"Rumble", g_rumble.load() ? L"1" : L"0", g_ini_path);
    WritePrivateProfileStringW(L"Settings", L"HudTheme", g_hud_theme.load() == 0 ? L"Original" : L"FF", g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_torque_scale.load());
    WritePrivateProfileStringW(L"Settings", L"TorqueScale", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.1f", g_duration.load());
    WritePrivateProfileStringW(L"Settings", L"Duration", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_buildup_time.load());
    WritePrivateProfileStringW(L"Settings", L"BuildupTime", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_recharge_rate.load());
    WritePrivateProfileStringW(L"Settings", L"RechargeRate", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_sound_volume.load());
    WritePrivateProfileStringW(L"Settings", L"SoundVolume", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_ai_sound_volume.load());
    WritePrivateProfileStringW(L"Settings", L"DrivatarSoundVolume", buf, g_ini_path);
    swprintf_s(buf, 64, L"%.2f", g_camera_shake.load());
    WritePrivateProfileStringW(L"Settings", L"CameraShake", buf, g_ini_path);
}

// HUD themes. The HUD files live in this DLL (src\hud.rc) and are written to mediaoverride through the platform.
// The speedometer XAML and the NOS outlines are needed by both themes. The gears and the ABS/TCR/STM "on" icons
// are Fast & Furious art, so with the Original theme they aren't replaced and the game reads its own.
struct HudFile {
    const char* resource;    // name in hud.rc
    const char* media_path;  // relative to media
    bool ff_only;
};
#define HUD_GEAR(n) {"HUD_Dial_Digital_Gear_" #n, "ui/textures/Anthem/HUD/Speedometer/HUD_Dial_Digital_Gear_" #n ".swatchbin", true}
#define HUD_ICON(kind, what) {"HUD_Dial_Assist_" kind "_" what "_On", "ui/textures/data_bound/Anthem/HUD/HUD_Dial_Assist_" kind "_" what "_On.swatchbin", true}
#define HUD_OUTLINE(name) {"HUD_Nos_Outline_" name "_Bar", "ui/textures/data_bound/SpireHUD/HUD_Nos_Outline_" name "_Bar.swatchbin", false}
static const HudFile k_hud_xaml = {"HUDSpeedometerControl", "ui/Controls/HUDSpeedometerControl.xaml", false};
static const HudFile k_hud_files[] = {
    HUD_OUTLINE("Digital"), HUD_OUTLINE("Radial"),
    HUD_GEAR(1), HUD_GEAR(2), HUD_GEAR(3), HUD_GEAR(4), HUD_GEAR(5), HUD_GEAR(6), HUD_GEAR(7), HUD_GEAR(8),
    HUD_ICON("Analogue", "ABS"), HUD_ICON("Analogue", "TCR"), HUD_ICON("Analogue", "STM"),
    HUD_ICON("Digital", "ABS"),  HUD_ICON("Digital", "TCR"),  HUD_ICON("Digital", "STM"),
};

// A file embedded in this DLL (src\hud.rc). The pointer stays valid for as long as the DLL is loaded.
static const void* embedded_file(const char* name, DWORD* size) {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&embedded_file), &self))
        return nullptr;
    HRSRC res = FindResourceA(self, name, MAKEINTRESOURCEA(10));   // 10 = RT_RCDATA
    if (!res) return nullptr;
    HGLOBAL mem = LoadResource(self, res);
    const void* data = mem ? LockResource(mem) : nullptr;
    if (data && size) *size = SizeofResource(self, res);
    return data;
}

static bool load_hud_resource(const char* name, std::string& out) {
    DWORD size = 0;
    const void* data = embedded_file(name, &size);
    if (!data) return false;
    out.assign(static_cast<const char*>(data), size);
    return true;
}

static void replace_all(std::string& s, const char* from, const char* to) {
    const size_t n = strlen(from);
    for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += strlen(to)) s.replace(pos, n, to);
}

static void apply_hud_theme() {
    if (!g_api || !g_mod || g_api->api_version < 2) return;
    const bool ff = g_hud_theme.load() != 0;

    std::string xaml;
    if (!load_hud_resource(k_hud_xaml.resource, xaml)) {
        g_api->log(g_mod, "HUD: the speedometer XAML is missing from the DLL.");
        return;
    }
    if (!ff) {
        // The XAML is written with the Fast & Furious cyan; Original turns the NOS bar yellow.
        replace_all(xaml, "#FF2DB2FF", "#FFF4F444");   // fill: the yellow of the stock gear box
        replace_all(xaml, "#2AEAFAFF", "#2AFFFFB0");   // tip glow
        replace_all(xaml, "#22EAF8FF", "#22FFFFB0");   // tip glow, radial
    }
    if (!g_api->set_file(g_mod, k_hud_xaml.media_path, xaml.data(), xaml.size()))
        g_api->log(g_mod, "HUD: couldn't replace the speedometer XAML (see platform.log).");

    for (const auto& f : k_hud_files) {
        if (f.ff_only && !ff) {
            g_api->clear_file(g_mod, f.media_path);
            continue;
        }
        std::string data;
        if (!load_hud_resource(f.resource, data) || !g_api->set_file(g_mod, f.media_path, data.data(), data.size())) {
            char msg[256];
            snprintf(msg, sizeof(msg), "HUD: couldn't replace %s (see platform.log).", f.media_path);
            g_api->log(g_mod, msg);
        }
    }
    g_api->log(g_mod, ff ? "HUD theme: Fast & Furious" : "HUD theme: Original (yellow NOS bar)");
}

// Older versions had players copy these files into mediaoverride by hand. The platform doesn't know about them, so
// it never removes them: stale cyan gears would show with the Original theme, and the old Spire flame shaders would
// stay. A file is deleted only when it is byte-for-byte one we know (or our own embedded copy), so nothing a player
// or another mod changed is touched.
static uint64_t fnv1a(const void* data, size_t size) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < size; ++i) h = (h ^ static_cast<const unsigned char*>(data)[i]) * 0x100000001b3ULL;
    return h;
}

static bool read_whole_file(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (64 << 20);
    if (ok) {
        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD got = 0;
        ok = ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size();
    }
    CloseHandle(f);
    return ok;
}

static std::wstring override_path(const wchar_t* relative) {
    return std::wstring(g_api->game_dir) + L"\\mediaoverride\\" + relative;
}

// Delete `path` if its content is exactly `size` bytes with hash `hash`; then remove its folders if they're empty.
static bool delete_if_known(const std::wstring& path, uint64_t size, uint64_t hash) {
    std::string have;
    if (!read_whole_file(path, have) || have.size() != size || fnv1a(have.data(), have.size()) != hash) return false;
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!DeleteFileW(path.c_str())) return false;
    const std::wstring root = std::wstring(g_api->game_dir) + L"\\mediaoverride";
    for (std::wstring dir = path.substr(0, path.find_last_of(L'\\'));
         dir.size() > root.size() && RemoveDirectoryW(dir.c_str()); dir = dir.substr(0, dir.find_last_of(L'\\'))) {
    }
    return true;
}

static void remove_legacy_files() {
    if (!g_api || !g_mod || !g_api->game_dir || !g_api->game_dir[0]) return;
    int removed = 0;
    for (const auto& f : k_legacy_files)
        if (delete_if_known(override_path(f.path), f.size, f.hash)) ++removed;
    // The Fast & Furious gears and icons, left behind by hand-copying or by an older version, when the theme is Original.
    if (g_hud_theme.load() == 0) {
        for (const auto& f : k_hud_files) {
            if (!f.ff_only) continue;
            std::string wanted;
            std::wstring rel(f.media_path, f.media_path + strlen(f.media_path));
            for (auto& c : rel) if (c == L'/') c = L'\\';
            if (load_hud_resource(f.resource, wanted) &&
                delete_if_known(override_path(rel.c_str()), wanted.size(), fnv1a(wanted.data(), wanted.size())))
                ++removed;
        }
    }
    if (removed) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Removed %d old HUD/flame file(s) from mediaoverride (now built into the mod).", removed);
        g_api->log(g_mod, msg);
    }
}

// The exhaust flame: the game's own Backfire effect with more and longer-lived particles, drawn with its stock
// blue backfire material. Both themes use it.
static void apply_backfire_effect() {
    if (!g_api || !g_mod || g_api->api_version < 2) return;
    std::string xml;
    if (!load_hud_resource("Backfire", xml) || !g_api->set_file(g_mod, "Particles/Systems/Backfire.xml", xml.data(), xml.size()))
        g_api->log(g_mod, "Couldn't replace the backfire effect (see platform.log); the exhaust flames will look stock.");
}

// Physics Hook: CCarDynamics::UpdateDynamics (RVA 0x12C1EA0 in Anthem)
// Main per-tick vehicle simulation loop for all vehicles in the game
typedef bool (*CCarDynamics_UpdateDynamicsFn)(void* this_ptr, void* rdx, void* r8, uint32_t r9, float dt);
static CCarDynamics_UpdateDynamicsFn g_orig_cardynamics = nullptr;

static std::atomic<uint64_t> g_physics_frames{0};
static std::atomic<DWORD> g_phys_tid{0};   // physics hook thread (debug overlay)
static std::atomic<DWORD> g_frame_tid{0};  // game logic frame-step thread (debug overlay)
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

static void drive_backfire_from_game_thread();   // defined with the other backfire code below

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
static const float kFlameRadius2 = 6.0f * 6.0f;     // presentation car "is the player" radius when finding it
static const float kStickyRadius2 = 40.0f * 40.0f;  // ...and when it is already known: at 250+ km/h the two positions lag apart
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

// Car kinds. ForzaCar knows what it is (FH2_GAME_FORZACAR_KIND: 0 the player's, 1 a Drivatar, 2 traffic).
// Its setup (0xF67270) has the physics world (ForzaCar+0x3A8, slot 0x80: 0x12BFEE0) make its CCarDynamics and
// keeps what that returns at ForzaCar+0x4C08: the dynamics' interface at +0x3A0 inside it. Its presentation is
// at ForzaCar+0x4D38 (the setup at 0xF65AD0 stores it, ForzaCar::ApplyCollidable reads it): a
// CCarPresentationDeferred, which queues each call for the real CarPresentation11 it holds at +8 (its slot 0x3A8
// returns that, and its queued calls are made on it). Each is checked by its vtable before it's used, and until
// a car of kind 0 turns up the player's car is still found by position (is_player_car above).
static const uintptr_t kForzaCarDynOffset = 0x4C08;
static const uintptr_t kDynInterfaceOffset = 0x3A0;
static const uintptr_t kForzaCarPresOffset = 0x4D38;
static const uintptr_t kDeferredPresOffset = 0x8;
static const uintptr_t kDeferredPresVtable = 0x2EBEF68;   // TRefCounted<CCarPresentationDeferred>, at its +0x10
static const uintptr_t kCarPresVtables[] = {0x2EC1450, 0x2EC2B70};   // CarPresentation, its TRefCounted wrapper
static const uintptr_t kCarDynVtables[] = {0x2EB4B08, 0x2EB5D68};    // CCarDynamics, its TRefCounted wrapper
static const uint64_t kKindFreshMs = 500;

typedef void (*ForzaCarUpdateFn)(void* car, float dt, float b, uint32_t flag);
typedef int (*ForzaCarKindFn)(void* car);
static ForzaCarUpdateFn g_orig_forzacar_update = nullptr;
static ForzaCarKindFn g_forzacar_kind = nullptr;

struct KnownCar {
    std::atomic<void*> dyn;        // written by the game thread, read by the physics thread
    std::atomic<uintptr_t> pres;   // its CarPresentation11
    std::atomic<int> kind;
    std::atomic<uint64_t> tick;
};
static KnownCar g_cars[64];
static std::atomic<void*> g_kind_player_dyn{nullptr};      // the car of kind 0, with its presentation
static std::atomic<uintptr_t> g_kind_player_pres{0};
static std::atomic<uint64_t> g_kind_player_tick{0};
static std::atomic<uint32_t> g_link_ok{0};                 // ForzaCar updates that led to a CCarDynamics
static std::atomic<uint32_t> g_link_bad{0};                // ...and that didn't
static std::atomic<uintptr_t> g_link_bad_pres_vt{0};       // what was found instead (game+offset), for the log
static std::atomic<uintptr_t> g_link_bad_dyn_vt{0};

static bool vtable_is(uintptr_t obj, const uintptr_t* rvas, int n) {
    const uintptr_t vt = *(const uintptr_t*)obj - (uintptr_t)g_game_base;
    for (int i = 0; i < n; ++i)
        if (vt == rvas[i]) return true;
    return false;
}

static void remember_car(void* dyn, uintptr_t pres, int kind, uint64_t now) {
    KnownCar* slot = &g_cars[0];
    for (auto& c : g_cars) {
        if (c.dyn.load() == dyn) { slot = &c; break; }
        if (c.tick.load() < slot->tick.load()) slot = &c;
    }
    if (slot->dyn.load() != dyn) {
        slot->dyn.store(nullptr);   // a reader mid-lookup mustn't pair the old car with the new kind
        slot->pres.store(pres);
        slot->kind.store(kind);
        slot->tick.store(now);
        slot->dyn.store(dyn);
    } else {
        slot->pres.store(pres);
        slot->kind.store(kind);
        slot->tick.store(now);
    }
}

// The kind and presentation of the car with this CCarDynamics, if the game updated it lately.
static bool car_kind(void* dyn, uint64_t now, int* kind, uintptr_t* pres) {
    for (auto& c : g_cars) {
        if (c.dyn.load() != dyn) continue;
        int k = c.kind.load();
        uintptr_t p = c.pres.load();
        uint64_t t = c.tick.load();
        if (c.dyn.load() != dyn || now - t > kKindFreshMs) return false;
        *kind = k;
        *pres = p;
        return true;
    }
    return false;
}

static bool player_known_by_kind(uint64_t now) {
    return now - g_kind_player_tick.load() < kKindFreshMs;
}

static void detour_forzacar_update(void* car, float dt, float b, uint32_t flag) {
    if (g_orig_forzacar_update) g_orig_forzacar_update(car, dt, b, flag);
    if (!car || !g_forzacar_kind || !g_game_base) return;

    __try {
        uintptr_t iface = *(const uintptr_t*)((uintptr_t)car + kForzaCarDynOffset);
        if (!iface) return;   // not simulated (yet)
        uintptr_t dyn = iface - kDynInterfaceOffset;
        if (!vtable_is(dyn, kCarDynVtables, 2)) {
            g_link_bad_dyn_vt.store(*(const uintptr_t*)dyn - (uintptr_t)g_game_base);
            g_link_bad.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        // The presentation is only needed for the flames: without it the car still gets its kind
        uintptr_t pres = *(const uintptr_t*)((uintptr_t)car + kForzaCarPresOffset);
        if (pres && vtable_is(pres, &kDeferredPresVtable, 1)) pres = *(const uintptr_t*)(pres + kDeferredPresOffset);
        if (pres && !vtable_is(pres, kCarPresVtables, 2)) {
            g_link_bad_pres_vt.store(*(const uintptr_t*)pres - (uintptr_t)g_game_base);
            pres = 0;
        }
        g_link_ok.fetch_add(1, std::memory_order_relaxed);

        int kind = g_forzacar_kind(car);
        uint64_t now = GetTickCount64();
        remember_car((void*)dyn, pres, kind, now);
        if (kind == 0) {
            g_kind_player_dyn.store((void*)dyn);
            g_kind_player_pres.store(pres);
            g_kind_player_tick.store(now);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
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

// Push along the horizontal part of the nose so a pitched-up car isn't thrown skyward. False if the nose is
// nearly vertical.
static bool push_forward(const float* fwd, float* vel, float boost, float dt) {
    float dir[3] = {fwd[0], fwd[1], fwd[2]};
    int up = g_up_axis.load();
    if (up >= 0) {
        dir[up] = 0.0f;
        float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        if (len < 0.3f) return false;
        dir[0] /= len; dir[1] /= len; dir[2] /= len;
    }
    float dt_use = (dt > 0.001f && dt < 0.1f) ? dt : 0.01667f;
    // Substantial acceleration force:
    // At boost 3.0x: (3.0 - 1.0) * 22.0f * dt ~ 0.73 m/s/tick (~44 m/s^2 or ~4.5 Gs)
    // At boost 10.0x: (10.0 - 1.0) * 22.0f * dt ~ 3.3 m/s/tick (~198 m/s^2 - extreme nitro rush!)
    float impulse = (boost - 1.0f) * 22.0f * dt_use;
    vel[0] += dir[0] * impulse;
    vel[1] += dir[1] * impulse;
    vel[2] += dir[2] * impulse;
    return true;
}

// Drivatar nitro. Each Drivatar has a tank of its own, twice the player's capacity and recharge (they can't time
// it as well as a player, and would otherwise run dry a minute into a race), with the player's boost and buildup.
// It boosts whenever it can, at any speed: it only lets off in the air, in a big slide or when the tank runs dry,
// and like the player it then waits for 2 s of nitro before going again. (Letting off for corners was tried: a
// car's turn and braking can't be read reliably while it boosts, and it lost the Drivatars too much.)
// Nothing happens while the player isn't driving (the HUD speedometer is down, as in the pre-race intro), and a
// car that has stood still (on the grid through the countdown) has to really roll for a while before it boosts:
// its speed is taken from how far it moves, since a car held on the grid can still report a velocity.
// One that has got more than 100 m ahead of the player doesn't boost (it coasts and refills), so the player can
// catch it; behind the player, or near, it boosts freely. Only the physics thread touches these.
struct AiNitro {
    void* dyn;
    uint64_t seen;
    float fuel;
    float burned;       // seconds into the current burst
    float cooldown;     // seconds until the next burst may start
    bool burning;
    bool depleted;      // ran dry: waits for 2 s of nitro
    bool tracked;       // the readings below are from its last tick
    float last_pos[3];
    float real_speed;   // m/s from how far it moved, smoothed
    float rolling;      // seconds it has been covering ground since it last stood still
    float speed_kmh;    // for the debug overlay
    const char* state;  // ...and what it's doing
};
static AiNitro g_ai[32];
static uint32_t g_ai_rng = 0x9E3779B9u;

// What a burning Drivatar's flames and sound need, published by the physics thread for the game threads that
// draw and play them. Same index as g_ai.
struct AiFx {
    std::atomic<bool> burning;
    std::atomic<uintptr_t> pres;   // the car's CarPresentation11
    std::atomic<float> pos[3];     // its world position
    std::atomic<uint64_t> tick;    // last physics tick that saw it; stale when the game pauses or the car goes
    std::atomic<uint64_t> since;   // when the current burst began
};

// Why bursts end, for the log's summary (physics thread counts, worker logs)
enum AiLetOff { kOffDry, kOffStopped, kOffAhead, kOffAir, kOffLong, kOffSlide, kOffCount };
static const char* const kAiLetOffNames[kOffCount] = {"ran dry", "stopped", "too far ahead", "air", "long burst", "slide"};
static std::atomic<uint32_t> g_ai_letoffs[kOffCount];
static std::atomic<uint32_t> g_ai_bursts{0};
static std::atomic<float> g_ai_burst_time{0.0f};   // seconds of all the bursts that ended
static AiFx g_ai_fx[32];
static const uint64_t kAiFxFreshMs = 150;
static const float kAiNitroRange2 = 100.0f * 100.0f;   // how far ahead of the player a Drivatar may still boost
static const float kAiTankScale = 2.0f;       // a Drivatar's tank against the player's
static const float kAiRechargeScale = 2.0f;   // ...and its recharge
static const float kAiMaxBurst = 60.0f;       // the built-in boost sound is 63 s long
static const float kAiLaunchTime = 2.0f;      // rolling this long after a standstill before boosting
static float ai_tank() { return g_duration.load() * kAiTankScale; }
static float g_player_heading[3] = {0.0f, 0.0f, 0.0f};  // where the player's car is going (physics thread)

static float ai_rand() {   // 0..1
    g_ai_rng ^= g_ai_rng << 13;
    g_ai_rng ^= g_ai_rng >> 17;
    g_ai_rng ^= g_ai_rng << 5;
    return (g_ai_rng >> 8) * (1.0f / 16777216.0f);
}

static AiNitro* ai_state(void* dyn, uint64_t now) {
    AiNitro* slot = &g_ai[0];
    for (auto& s : g_ai) {
        if (s.dyn == dyn) { s.seen = now; return &s; }
        if (s.seen < slot->seen) slot = &s;
    }
    *slot = AiNitro{};
    slot->dyn = dyn;
    slot->seen = now;
    slot->fuel = ai_tank();
    slot->cooldown = ai_rand();
    slot->state = "new";
    g_ai_fx[slot - g_ai].burning.store(false);
    return slot;
}

static float horizontal_len(const float* v, int up) {
    float h[3] = {v[0], v[1], v[2]};
    h[up] = 0.0f;
    return sqrtf(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
}

static void drivatar_nitro(void* dyn, uintptr_t pres, const float* fwd, float* vel, float dt, uint64_t now) {
    // Up is learned from the player's driving, which can take a while; it has always come out Y, so until then
    // take that
    const int learned_up = g_up_axis.load();
    const int up = learned_up >= 0 ? learned_up : 1;
    AiNitro* s = ai_state(dyn, now);
    AiFx& fx = g_ai_fx[s - g_ai];
    const float dt_use = (dt > 0.001f && dt < 0.1f) ? dt : 0.01667f;
    const float smooth = dt_use / (0.15f + dt_use);        // ~0.15 s of smoothing
    const float full = ai_tank();
    const int off = g_pos_offset.load();
    bool held_back = false;   // got away ahead of the player
    if (off >= 0) {
        const float* p = (const float*)((uintptr_t)dyn + off);
        for (int i = 0; i < 3; ++i) fx.pos[i].store(p[i]);
        float d2;
        if (now - g_player_pos_tick.load() < 200 && car_dist2(dyn, off, &d2) && d2 >= kAiNitroRange2) {
            const float ahead = (p[0] - g_player_pos[0]) * g_player_heading[0] + (p[1] - g_player_pos[1]) * g_player_heading[1] +
                                (p[2] - g_player_pos[2]) * g_player_heading[2];
            held_back = ahead > 0.0f;
        }
    }
    fx.pres.store(pres);
    fx.tick.store(now);

    float hv[3] = {vel[0], vel[1], vel[2]}, hf[3] = {fwd[0], fwd[1], fwd[2]};
    hv[up] = 0.0f;
    hf[up] = 0.0f;
    const float speed = sqrtf(hv[0] * hv[0] + hv[1] * hv[1] + hv[2] * hv[2]);
    const float flen = sqrtf(hf[0] * hf[0] + hf[1] * hf[1] + hf[2] * hf[2]);
    const float align = (speed > 1.0f && flen > 0.3f) ? (hv[0] * hf[0] + hv[1] * hf[1] + hv[2] * hf[2]) / (speed * flen) : 0.0f;
    const bool grounded = fabsf(vel[up]) < 3.0f;
    s->speed_kmh = speed * 3.6f;

    // How fast it's really moving: from how far it went since the last tick
    if (s->tracked && off >= 0) {
        const float* p = (const float*)((uintptr_t)dyn + off);
        float moved[3] = {p[0] - s->last_pos[0], p[1] - s->last_pos[1], p[2] - s->last_pos[2]};
        const float dist = horizontal_len(moved, up);
        if (dist > 30.0f) {
            // Put somewhere else (a race start, a reset): a fresh car with a full tank
            s->tracked = false;
            s->rolling = 0.0f;
            s->fuel = full;
            s->depleted = false;
        } else {
            s->real_speed += (dist / dt_use - s->real_speed) * smooth;
        }
    } else if (off < 0) {
        s->real_speed = speed;   // where cars are isn't known yet: take their word for it
    }
    if (!s->tracked) {
        s->real_speed = 0.0f;
    }
    s->tracked = true;
    if (off >= 0)
        for (int i = 0; i < 3; ++i) s->last_pos[i] = ((const float*)((uintptr_t)dyn + off))[i];

    // Through the pre-race intro the player isn't driving; on the grid, through the countdown, the car doesn't move
    const bool driving = now - g_last_speedo_tick.load() < 200;
    if (!driving || s->real_speed < 1.5f) s->rolling = 0.0f;
    else s->rolling += dt_use;
    const bool launched = s->rolling >= kAiLaunchTime;

    if (s->burning) {
        const bool spent = s->fuel <= 0.0f || s->burned >= kAiMaxBurst;
        if (!launched || held_back || !grounded || align < 0.9f || spent) {
            const AiLetOff why = s->fuel <= 0.0f ? kOffDry : !launched ? kOffStopped : held_back ? kOffAhead
                               : !grounded ? kOffAir : spent ? kOffLong : kOffSlide;
            g_ai_letoffs[why].fetch_add(1, std::memory_order_relaxed);
            g_ai_bursts.fetch_add(1, std::memory_order_relaxed);
            g_ai_burst_time.store(g_ai_burst_time.load() + s->burned);
            s->burning = false;
            s->depleted = s->fuel <= 0.0f;
            // A moment to settle, so a bump doesn't flicker it off and on
            s->cooldown = 0.5f + ai_rand();
            s->state = kAiLetOffNames[why];
            fx.burning.store(false);
            return;
        }
        s->state = "BOOST";
        s->burned += dt_use;
        s->fuel = (std::max)(0.0f, s->fuel - dt_use);
        const float ramp_time = g_buildup_time.load();
        const float ramp = (ramp_time > 0.001f) ? (std::min)(1.0f, s->burned / ramp_time) : 1.0f;
        push_forward(fwd, vel, 1.0f + (g_torque_scale.load() - 1.0f) * ramp, dt);
        return;
    }

    s->fuel = (std::min)(full, s->fuel + dt_use * g_recharge_rate.load() * kAiRechargeScale);
    if (s->cooldown > 0.0f) s->cooldown -= dt_use;
    if (s->depleted && s->fuel >= 2.0f) s->depleted = false;
    // Under way, going forwards and not sliding much: whenever it can
    if (s->cooldown <= 0.0f && !s->depleted && s->fuel > 0.0f && !held_back && grounded && launched &&
        speed > 2.0f && align > 0.95f) {
        s->burned = 0.0f;
        s->burning = true;
        s->state = "BOOST";
        fx.since.store(now);
        fx.burning.store(true);
    } else {
        s->state = s->depleted ? "refilling to 2s" : !driving ? "waiting: not racing" : !launched ? "waiting: launch"
                 : s->cooldown > 0.0f ? "settling" : held_back ? "too far ahead" : !grounded ? "in the air"
                 : "sliding";
    }
}

static bool detour_cardynamics(void* this_ptr, void* rdx, void* r8, uint32_t r9, float dt) {
    bool res = false;
    if (g_orig_cardynamics) {
        res = g_orig_cardynamics(this_ptr, rdx, r8, r9, dt);
    }

    g_physics_frames.fetch_add(1, std::memory_order_relaxed);
    g_phys_tid.store(GetCurrentThreadId(), std::memory_order_relaxed);

    if (!this_ptr) return res;

    __try {
        // In CCarDynamics, [this_ptr + 0x178] == 1 marks a simulated car (the player's, AI racers and traffic)
        int is_sim = *(const int*)((uintptr_t)this_ptr + 0x178);
        if (is_sim == 1) {
            uint64_t now = GetTickCount64();
            g_last_physics_tick.store(now);
            note_car(this_ptr, now);
            if (g_pos_offset.load() < 0) calibrate_position_offset(this_ptr, now);

            // The car of kind 0 when the game has said which that is; the one nearest the player position until then.
            bool mine = player_known_by_kind(now) ? this_ptr == g_kind_player_dyn.load() : is_player_car(this_ptr, now);
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
                // Where the car is going: its velocity, or its nose when it's barely moving
                float sp2 = vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2];
                const float* dir = sp2 > 25.0f ? vel : fwd;
                float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
                if (len > 0.001f)
                    for (int i = 0; i < 3; ++i) g_player_heading[i] = dir[i] / len;
            }

            if (mine && grounded && g_enabled.load() && g_nitro_active.load()) {
                float boost = g_current_boost.load();
                if (boost > 1.0f && push_forward(fwd, vel, boost, dt))
                    g_boosted_frames.fetch_add(1, std::memory_order_relaxed);
            }

            int kind;
            uintptr_t pres;
            if (!mine && g_drivatar_nitro.load() && g_enabled.load() && car_kind(this_ptr, now, &kind, &pres) && kind == 1)
                drivatar_nitro(this_ptr, pres, fwd, vel, dt, now);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safe protection
    }
    return res;
}

// Backfire Hook: Call at RVA 0x13B0DE6 in Anthem (CarPresentation11::UpdateBackfire)
typedef void (*BackfirePrepFn)(void* r15);
static BackfirePrepFn g_orig_backfire_prep = nullptr;
static std::atomic<uint32_t> g_bf_driven{0};   // routine calls we made ourselves
static std::atomic<uint32_t> g_bf_total{0};    // every hook call, nitro or not
static std::atomic<uint32_t> g_bf_calls{0};    // hook calls while nitro is on
static std::atomic<uint32_t> g_bf_matched{0};  // ...that were the player's car
static std::atomic<int> g_bf_tips{0};          // real exhaust tips on the player car
static char g_tip_diag[16] = "";               // per-tip status for the debug overlay
static std::atomic<uint32_t> g_real_mask{0};    // tips the game itself flagged in its own backfire events for the player
static std::atomic<uint32_t> g_real_events{0};  // how many such events were seen
static std::atomic<bool> g_in_drive{false};     // true while our own driver is calling the routine
static std::atomic<uintptr_t> g_mask_pres{0};   // the presentation (car) the learned tip mask belongs to

// The game only runs its backfire routine when a cooldown timer on the car's presentation has expired
// (about every 40 s for the player), so a nitro burst would get at most one flame. The player's
// presentation is the one with the player's position stored at +0x50; remember it, and while nitro is
// on keep that timer expired so the game runs the routine (and our flame hook) every frame.
static const uintptr_t kPresPosOffset = 0x50;
static const uintptr_t kTipsOffset = 0x15a0;      // 8 exhaust tip entries of 0x60 bytes in the presentation
static const uintptr_t kBackfireObjOffset = 0x18a0;   // timer object inside the presentation (r15)
static const uintptr_t kBackfireExpiryOffset = 0x18;  // next-fire time inside it
static std::atomic<uintptr_t> g_player_pres{0};
static std::atomic<uint64_t> g_player_pres_tick{0};

// The player's presentation: the one of the car of kind 0 when that's known, else the one at the player position.
static bool is_player_pres(uintptr_t pres, uint64_t now) {
    if (player_known_by_kind(now) && g_kind_player_pres.load()) return pres == g_kind_player_pres.load();
    float d2;
    return g_pos_offset.load() >= 0 && now - g_player_pos_tick.load() < 200 &&
           car_dist2((const void*)pres, (int)kPresPosOffset, &d2) &&
           (d2 < kFlameRadius2 || (pres == g_player_pres.load() && d2 < kStickyRadius2));
}

// The presentation our driver is calling the routine for, and the tips to flag on it.
static std::atomic<uintptr_t> g_drive_pres{0};
static std::atomic<uint32_t> g_drive_mask{0};
static std::atomic<uint32_t> g_ai_flames{0};   // routine calls we made for Drivatars

static void detour_backfire_prep(void* r15) {
    if (g_orig_backfire_prep) {
        g_orig_backfire_prep(r15);
    }

    g_bf_total.fetch_add(1, std::memory_order_relaxed);
    if (!r15) return;

    // r15 is CarPresentation11 + 0x18a0
    uintptr_t car_pres = (uintptr_t)r15 - kBackfireObjOffset;

    // Is this the player's presentation?
    bool is_target = false;
    __try {
        uint64_t now = GetTickCount64();
        if (is_player_pres(car_pres, now)) {
            is_target = true;
            g_player_pres.store(car_pres);
            g_player_pres_tick.store(now);
        } else if (g_pos_offset.load() < 0 && !player_known_by_kind(now)) {
            // Player not identified yet: fall back to any simulated car
            void* car_dyn = *(void**)(car_pres + 0x10);
            if (car_dyn && *(const int*)((uintptr_t)car_dyn + 0x178) == 1) is_target = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    if (g_enabled.load() && g_nitro_active.load()) {
        g_bf_calls.fetch_add(1, std::memory_order_relaxed);
        if (is_target) g_bf_matched.fetch_add(1, std::memory_order_relaxed);
    }

    __try {
        // When the game fires its own backfire event its message already carries the right tips: leave it alone.
        // When our driver is calling, flag the tips the game has flagged for this car in its own events (the
        // routine then spawns a "BackFireInstance<N>" effect for each flagged tip whose handle slot,
        // car_pres + 0x1498 + 0x10 * N, is empty). 8 is only the size of the tip array: a slot the car doesn't
        // really have would get a flame in mid-air, so there is no fallback. Leave the handles alone: the game
        // keeps the running effect attached to the exhaust through them.
        if (g_in_drive.load() && car_pres == g_drive_pres.load()) {
            uintptr_t exhaust_start = car_pres + kTipsOffset;
            int total = (int)(((uintptr_t)r15 - exhaust_start) / 0x60);
            if (total > 8) total = 8;

            uint32_t mask = g_drive_mask.load() & 0xFF;
            char diag[16];
            int nvalid = 0;
            for (int i = 0; i < total; ++i) {
                bool on = ((mask >> i) & 1) != 0;
                *(uint8_t*)(exhaust_start + (uintptr_t)i * 0x60 + 0x40) = on ? 1 : 0;
                diag[i] = on ? 'M' : '-';
                if (on) ++nvalid;
            }
            if (is_target) {
                diag[total] = 0;
                memcpy(g_tip_diag, diag, (size_t)total + 1);
                g_bf_tips.store(nvalid, std::memory_order_relaxed);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safe protection
    }
}

// Entry of the game's backfire routine (RVA 0x13B0D70, vtable slot 76 of CarPresentation11). The game calls it
// when the engine sim sends a backfire message. We note which thread that is, and learn which exhaust tips each
// car really has from the tips the game itself flags for it.
typedef uint64_t (*BackfireEntryFn)(void* a, void* b, void* c, void* d);
static BackfireEntryFn g_orig_backfire_entry = nullptr;
static std::atomic<DWORD> g_bf_tid{0};   // thread the game runs its backfire routine on

// The tips learned for the other cars (the Drivatars' flames need them). Only the backfire thread uses it.
struct TipMask {
    uintptr_t pres;
    uint32_t mask;
    uint64_t tick;
};
static TipMask g_tip_masks[48];

static void learn_tips(uintptr_t pres, uint32_t mask, uint64_t now) {
    TipMask* slot = &g_tip_masks[0];
    for (auto& t : g_tip_masks) {
        if (t.pres == pres) { slot = &t; break; }
        if (t.tick < slot->tick) slot = &t;
    }
    if (slot->pres != pres) *slot = TipMask{pres, 0, 0};   // a reused slot starts over
    slot->mask |= mask;
    slot->tick = now;
}

static uint32_t learned_tips(uintptr_t pres) {
    for (const auto& t : g_tip_masks)
        if (t.pres == pres) return t.mask & 0xFF;
    return 0;
}

static uint64_t detour_backfire_entry(void* a, void* b, void* c, void* d) {
    g_bf_tid.store(GetCurrentThreadId());

    // A real backfire event: the message (b) carries the flag byte (+0x40) of each tip the game wants to fire,
    // and the game only flags tips the car really has. Remember them.
    if (!g_in_drive.load() && a && b) {
        __try {
            uint32_t mask = 0;
            for (int i = 0; i < 8; ++i) {
                if (*((const uint8_t*)b + i * 0x60 + 0x40) != 0) mask |= 1u << i;
            }
            uint64_t now = GetTickCount64();
            if (is_player_pres((uintptr_t)a, now)) {
                if ((uintptr_t)a != g_mask_pres.load()) {
                    // A different car (or a reloaded one): what was learned no longer applies
                    g_real_mask.store(0);
                    g_real_events.store(0);
                    g_mask_pres.store((uintptr_t)a);
                }
                g_real_mask.fetch_or(mask);
                g_real_events.fetch_add(1);
            } else if (mask) {
                learn_tips((uintptr_t)a, mask, now);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    return g_orig_backfire_entry ? g_orig_backfire_entry(a, b, c, d) : 0;
}

// Runs the game's backfire routine for one presentation, flagging `mask`'s tips.
static void drive_backfire(uintptr_t pres, uint32_t mask) {
    alignas(16) static uint8_t message[0x400];
    __try {
        // The routine copies the 8 tip entries (0x60 bytes each) from this array into the presentation's own tips
        // before it spawns the flames, then destroys the array. An empty array would wipe the tips' positions
        // (flames spawn nowhere), so send the tips' current contents. Each entry holds a ref-counted pointer
        // (+0x48 object, +0x50 control block with the use count at +8); the destroy step releases one
        // reference per entry, so take one first, like the game's own copy does.
        memset(message, 0, sizeof(message));
        memcpy(message, (const void*)(pres + kTipsOffset), 8 * 0x60);
        for (int i = 0; i < 8; ++i) {
            uintptr_t block = *(uintptr_t*)(message + i * 0x60 + 0x50);
            if (block) _InterlockedIncrement((volatile long*)(block + 8));
        }
        g_drive_pres.store(pres);
        g_drive_mask.store(mask);
        g_in_drive.store(true);
        g_orig_backfire_entry((void*)pres, message, nullptr, nullptr);
        g_in_drive.store(false);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_in_drive.store(false);
    }
}

// The game only calls its backfire routine when the engine sim sends a backfire message (rare), so nitro
// would get at most one flame. While nitro is on, call the routine for the player's presentation every few
// frames ourselves, with the presentation's own tips as the message; the flame hook then flags the real tips.
// The same for each Drivatar burning nitro. Runs from the game's per-frame step, and only on the same thread
// the game calls the routine on.
static void drive_backfire_from_game_thread() {
    if (!g_orig_backfire_entry || !g_enabled.load()) return;

    DWORD tid = GetCurrentThreadId();
    DWORD bf_tid = g_bf_tid.load();

    if (bf_tid == 0 || tid != bf_tid) return;

    static uint64_t last_drive = 0;
    uint64_t now = GetTickCount64();
    if (now - last_drive < 40) return;
    last_drive = now;

    // Wait until the game has shown which tips the car really has (its own backfire events carry that).
    // No flames until then rather than flames on slots the car doesn't have.
    uintptr_t pres = g_player_pres.load();
    if (g_nitro_active.load() && pres && (g_real_mask.load() & 0xFF) != 0 && g_mask_pres.load() == pres &&
        now - g_player_pres_tick.load() <= 120000 && is_player_pres(pres, now)) {
        drive_backfire(pres, g_real_mask.load() & 0xFF);
        g_bf_driven.fetch_add(1, std::memory_order_relaxed);
    }

    if (!g_drivatar_nitro.load()) return;
    for (auto& fx : g_ai_fx) {
        if (!fx.burning.load() || now - fx.tick.load() > kAiFxFreshMs) continue;
        uintptr_t ai_pres = fx.pres.load();
        uint32_t mask = ai_pres ? learned_tips(ai_pres) : 0;
        if (!mask) continue;
        drive_backfire(ai_pres, mask);
        g_ai_flames.fetch_add(1, std::memory_order_relaxed);
    }
}

// Per-frame step of the game's logic thread (RVA 0xFA78A0; this is the thread the backfire routine runs on).
// Found from the call chain of a real backfire call: thread entry -> update loop -> this -> subsystem updates
// -> backfire message handler. Takes (this, bool); no floating-point arguments.
typedef uint64_t (*GameFrameFn)(void* self, void* flag);
static GameFrameFn g_orig_game_frame = nullptr;

static uint64_t detour_game_frame(void* self, void* flag) {
    uint64_t r = g_orig_game_frame ? g_orig_game_frame(self, flag) : 0;
    g_frame_tid.store(GetCurrentThreadId(), std::memory_order_relaxed);
    drive_backfire_from_game_thread();
    return r;
}

// Camera Shake
// While nitro burns, the driver, hood, bumper and chase cameras shake. Each camera's view matrix (3 rows of
// right/up/forward, with the translation in the 4th column) comes from its vtable slot +0x50: 0x1034360 for the
// three mounted cameras, 0x1034430 for the three chase cameras (found through Deuquan's FH2-CameraMods, which
// shakes the same matrix). The functions are hooked rather than the slots, so the camera mod's own slot hooks
// still call through to ours. Other cameras that share the functions are left alone.
typedef void* (*ViewMatrixFn)(void* camera, float* out);
static ViewMatrixFn g_orig_view_mounted = nullptr;
static ViewMatrixFn g_orig_view_chase = nullptr;
static const uintptr_t k_shake_camera_vtables[] = {0x2E78120, 0x2E77EA8, 0x2E703B0, 0x2E783A0, 0x2E78610, 0x2E78880};

static void shake_view_matrix(void* camera, float* m) {
    const float strength = g_camera_shake.load();
    if (!camera || !m || !g_game_base || strength <= 0.0f) return;
    const uintptr_t vtable = *static_cast<uintptr_t*>(camera) - reinterpret_cast<uintptr_t>(g_game_base);
    bool ours = false;
    for (uintptr_t v : k_shake_camera_vtables) ours |= v == vtable;
    if (!ours) return;

    // Fade in over ~0.2s and out over ~0.4s. The shake is a function of the time, so a matrix asked for twice in
    // one frame comes out the same.
    static LARGE_INTEGER freq{}, last{};
    static float envelope = 0.0f, t = 0.0f;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    float dt = last.QuadPart ? static_cast<float>(now.QuadPart - last.QuadPart) / static_cast<float>(freq.QuadPart) : 0.0f;
    last = now;
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    const float target = g_nitro_active.load() ? 1.0f : 0.0f;
    envelope += (target - envelope) * (std::min)(1.0f, dt * (target > envelope ? 5.0f : 2.5f));
    t = fmodf(t + dt, 1000.0f);
    if (envelope < 0.001f) return;

    for (int i = 0; i < 12; ++i)
        if (!std::isfinite(m[i])) return;
    float* r0 = m; float* r1 = m + 4; float* r2 = m + 8;
    const float d02 = r0[0] * r2[0] + r0[1] * r2[1] + r0[2] * r2[2];
    const float d12 = r1[0] * r2[0] + r1[1] * r2[1] + r1[2] * r2[2];
    const float l2 = r2[0] * r2[0] + r2[1] * r2[1] + r2[2] * r2[2];
    if (fabsf(d02) > 0.05f || fabsf(d12) > 0.05f || l2 < 0.9f || l2 > 1.1f) return;   // not a rotation: leave it

    // A slower wobble under a fast buzz, at frequencies that don't line up. At 100 the peaks are about half a degree.
    constexpr float tau = 6.2831853f;
    const float a = envelope * strength * 0.008f;
    const float pitch = a * (0.55f * sinf(tau * 4.3f * t) + 0.30f * sinf(tau * 13.7f * t + 1.1f) + 0.15f * sinf(tau * 23.1f * t + 2.3f));
    const float yaw   = a * (0.35f * sinf(tau * 3.1f * t + 0.7f) + 0.20f * sinf(tau * 17.3f * t + 0.4f));
    const float roll  = a * (0.60f * sinf(tau * 5.9f * t + 1.9f) + 0.35f * sinf(tau * 19.7f * t + 0.2f));

    auto turn = [](float* x, float* y, float angle) {   // rotates row x towards row y
        const float c = cosf(angle), s = sinf(angle);
        for (int k = 0; k < 4; ++k) {
            const float nx = c * x[k] + s * y[k], ny = c * y[k] - s * x[k];
            x[k] = nx;
            y[k] = ny;
        }
    };
    turn(r2, r0, yaw);
    turn(r2, r1, pitch);
    turn(r0, r1, roll);
}

static void* detour_view_mounted(void* camera, float* out) {
    void* r = g_orig_view_mounted(camera, out);
    shake_view_matrix(camera, out);
    return r;
}

static void* detour_view_chase(void* camera, float* out) {
    void* r = g_orig_view_chase(camera, out);
    shake_view_matrix(camera, out);
    return r;
}

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
static void set_rumble(WORD left_motor, WORD right_motor);

// Nitro Button and HUD Theme are choice rows (0-based) on platform 2.8.0 and up, sliders (1-based) before.
static void set_nitro_button(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= k_button_count) idx = k_button_count - 1;
    g_nitro_button.store(k_buttons[idx].mask);
    save_config();
    char buf[128];
    snprintf(buf, sizeof(buf), "NitroButton set to %ls (saved)", k_buttons[idx].name);
    if (g_api && g_mod) g_api->log(g_mod, buf);
}

static void set_hud_theme(int idx) {
    g_hud_theme.store(idx >= 1 ? 1 : 0);
    save_config();
    apply_hud_theme();   // the files are written now; the game picks them up when it restarts
    char buf[128];
    snprintf(buf, sizeof(buf), "HudTheme set to %s (saved, applies after a restart)", g_hud_theme.load() ? "Fast & Furious" : "Original");
    if (g_api && g_mod) g_api->log(g_mod, buf);
}

static void on_setting_changed(void* user, const char* key, int value) {
    if (strcmp(key, "NitroButton") == 0) {
        set_nitro_button(value);
        return;
    }
    if (strcmp(key, "HudTheme") == 0) {
        set_hud_theme(value);
        return;
    }
    if (strcmp(key, "DrivatarNitro") == 0) {
        g_drivatar_nitro.store(value != 0);
        save_config();
        if (g_api && g_mod) g_api->log(g_mod, value ? "Drivatar nitro on" : "Drivatar nitro off");
        return;
    }
    if (strcmp(key, "DebugOverlay") == 0) {
        g_debug.store(value != 0);
        save_config();
        return;
    }
    if (strcmp(key, "Rumble") == 0) {
        g_rumble.store(value != 0);
        if (!value) set_rumble(0, 0);   // stop a rumble that's already going
        save_config();
        return;
    }
    if (strcmp(key, "RefundOnRewind") == 0) {
        g_refund_on_rewind.store(value != 0);
        save_config();
        return;
    }
    if (strcmp(key, "RefillOnRaceStart") == 0) {
        g_refill_on_start.store(value != 0);
        save_config();
        return;
    }
    if (strcmp(key, "BackfireOnAllCars") == 0) {
        g_backfire_all_cars.store(value != 0);
        save_config();
        if (g_api && g_mod) g_api->log(g_mod, value ? "Backfire on all cars on (applies after a restart)" : "Backfire on all cars off (applies after a restart)");
        return;
    }
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
    if (strcmp(key, "NitroButton") == 0) {
        set_nitro_button((int)(value + 0.5f) - 1);
    } else if (strcmp(key, "HudTheme") == 0) {
        set_hud_theme((int)(value + 0.5f) - 1);
    } else if (strcmp(key, "TorqueScale") == 0) {
        g_torque_scale.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "TorqueScale set to %.2fx (saved)", value);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "Duration") == 0) {
        g_duration.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "Nitro Duration set to %.1fs (saved)", value);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "SoundVolume") == 0) {
        g_sound_volume.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "SoundVolume set to %.0f%% (saved)", value * 100.0f);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "DrivatarSoundVolume") == 0) {
        g_ai_sound_volume.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "DrivatarSoundVolume set to %.0f%% (saved)", value * 100.0f);
        if (g_api && g_mod) g_api->log(g_mod, buf);
    } else if (strcmp(key, "CameraShake") == 0) {
        g_camera_shake.store(value);
        save_config();
        snprintf(buf, sizeof(buf), "CameraShake set to %.0f%% (saved)", value * 100.0f);
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
// With Controller Rumble off only the "stop" (0, 0) goes through, so the game's own rumble is left alone.
static void set_rumble(WORD left_motor, WORD right_motor) {
    if (!g_rumble.load() && (left_motor || right_motor)) return;
    XINPUT_VIBRATION vib{};
    vib.wLeftMotorSpeed = left_motor;
    vib.wRightMotorSpeed = right_motor;
    for (DWORD i = 0; i < 4; ++i) {
        XInputSetState(i, &vib);
    }
}

// Audio Control
// The boost sound is mixed here from the FF layers embedded in the DLL: the start burst over a bed of the two
// sustain loops taking turns, with a short crossfade between them. PlaySound can only loop a whole sound, so the
// mix is made a little longer than a full tank: the start never plays again, however big the tank is. The stop
// sound is played as it is. A nos_boost.wav / nos_stop.wav in the FnFNitro folder next to azimuth.exe replaces
// them, for players who want their own (a custom boost sound loops whole). PlaySound has no volume of its own,
// so the samples are scaled by the Sound Volume slider. A sound is only rebuilt when something it's made from
// changes, and its buffer has to stay alive while PlaySound is still playing it.
struct NitroSound {
    const wchar_t* custom_path;
    const char* embedded_name;   // nullptr: mixed from the layers
    std::vector<char> wav;
    float volume = -1.0f;
    float seconds = -1.0f;
    bool custom = false;
};
static NitroSound g_boost_sound{g_sound_boost_path, nullptr};
static NitroSound g_stop_sound{g_sound_stop_path, "nos_stop"};

// Where a 16-bit PCM wav's samples are. samples is nullptr for anything else.
struct Pcm16 {
    int16_t* samples = nullptr;
    size_t frames = 0;
    int channels = 0;
    uint32_t rate = 0;
};

static Pcm16 find_pcm16(char* wav, size_t size) {
    Pcm16 pcm;
    if (size < 12 || memcmp(wav, "RIFF", 4) != 0 || memcmp(wav + 8, "WAVE", 4) != 0) return pcm;
    bool pcm16 = false;
    for (size_t pos = 12; pos + 8 <= size;) {
        uint32_t len;
        memcpy(&len, wav + pos + 4, 4);
        char* body = wav + pos + 8;
        const size_t avail = (std::min)(static_cast<size_t>(len), size - pos - 8);
        if (memcmp(wav + pos, "fmt ", 4) == 0 && avail >= 16) {
            uint16_t tag, channels, bits, sub = 0;
            memcpy(&tag, body, 2);
            memcpy(&channels, body + 2, 2);
            memcpy(&pcm.rate, body + 4, 4);
            memcpy(&bits, body + 14, 2);
            if (tag == 0xFFFE && avail >= 26) memcpy(&sub, body + 24, 2);   // WAVE_FORMAT_EXTENSIBLE: the real tag starts the GUID
            pcm16 = bits == 16 && channels >= 1 && channels <= 2 && (tag == 1 || (tag == 0xFFFE && sub == 1));
            pcm.channels = channels;
        } else if (memcmp(wav + pos, "data", 4) == 0) {
            if (pcm16) {
                pcm.samples = reinterpret_cast<int16_t*>(body);
                pcm.frames = avail / (2 * pcm.channels);
            }
            return pcm;
        }
        pos += 8 + static_cast<size_t>(len) + (len & 1);
    }
    return pcm;
}

static void scale_wav(std::vector<char>& wav, float volume) {
    Pcm16 pcm = find_pcm16(wav.data(), wav.size());
    for (size_t i = 0; pcm.samples && i < pcm.frames * pcm.channels; ++i)
        pcm.samples[i] = static_cast<int16_t>(lroundf(pcm.samples[i] * volume));
}

static Pcm16 embedded_pcm16(const char* name) {
    DWORD size = 0;
    const void* data = embedded_file(name, &size);
    return data ? find_pcm16(static_cast<char*>(const_cast<void*>(data)), size) : Pcm16{};
}

static constexpr uint32_t kBoostRate = 48000;   // the rate of the built-in sounds

// The start burst over the sustains, `seconds` long: 48 kHz stereo, interleaved, in 16-bit units (it can go past
// what 16 bits hold). Empty if the built-in sounds aren't as expected.
static std::vector<float> mix_boost_pcm(float seconds) {
    constexpr uint32_t rate = kBoostRate;
    constexpr size_t fade = rate / 50;   // 20 ms crossfade between sustains
    const Pcm16 start = embedded_pcm16("nos_start");
    const Pcm16 sustains[2] = {embedded_pcm16("nos_sustain_1"), embedded_pcm16("nos_sustain_2")};
    for (const Pcm16& p : {start, sustains[0], sustains[1]})
        if (!p.samples || p.rate != rate || p.frames <= 2 * fade) return {};

    const size_t frames = static_cast<size_t>(seconds * rate);
    std::vector<float> mix(frames * 2, 0.0f);
    for (size_t pos = 0, n = 0; pos < frames; pos += sustains[n++ % 2].frames - fade) {
        const Pcm16& s = sustains[n % 2];
        for (size_t j = 0; j < s.frames && pos + j < frames; ++j) {
            float keep = 0.0f, add = 1.0f;
            if (n > 0 && j < fade) {   // equal-power crossfade from the sustain before
                const float a = (j + 0.5f) / fade * 1.5707963f;
                keep = cosf(a);
                add = sinf(a);
            }
            for (int c = 0; c < 2; ++c) {
                float& out = mix[(pos + j) * 2 + c];
                out = out * keep + s.samples[j * s.channels + (s.channels == 2 ? c : 0)] * add;
            }
        }
    }
    for (size_t j = 0; j < start.frames && j < frames; ++j)
        for (int c = 0; c < 2; ++c) mix[j * 2 + c] += start.samples[j * start.channels + (start.channels == 2 ? c : 0)];
    return mix;
}

// Interleaved samples (in 16-bit units) as a 16-bit wav, times `volume`. Clipped samples crackle, so samples that
// would go past what 16 bits hold are turned down to fit; `scaled`, if given, gets how much (1 = not at all), for
// a player that can make up for it.
static std::vector<char> wav16(const std::vector<float>& samples, int channels, uint32_t rate, float volume,
                               float* scaled = nullptr) {
    float peak = 0.0f;
    for (float v : samples) peak = (std::max)(peak, fabsf(v * volume));
    if (peak > 32767.0f) volume *= 32767.0f / peak;
    if (scaled) *scaled = peak > 32767.0f ? 32767.0f / peak : 1.0f;

    const uint32_t data_size = static_cast<uint32_t>(samples.size() * 2);
    std::vector<char> wav(44 + data_size);
    auto put32 = [&](size_t at, uint32_t v) { memcpy(wav.data() + at, &v, 4); };
    auto put16 = [&](size_t at, uint16_t v) { memcpy(wav.data() + at, &v, 2); };
    memcpy(wav.data(), "RIFF", 4);
    put32(4, 36 + data_size);
    memcpy(wav.data() + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);                                    // PCM
    put16(22, static_cast<uint16_t>(channels));
    put32(24, rate);
    put32(28, rate * 2 * channels);                  // bytes per second
    put16(32, static_cast<uint16_t>(2 * channels));  // bytes per frame
    put16(34, 16);                                   // bits
    memcpy(wav.data() + 36, "data", 4);
    put32(40, data_size);
    int16_t* out = reinterpret_cast<int16_t*>(wav.data() + 44);
    for (size_t i = 0; i < samples.size(); ++i)
        out[i] = static_cast<int16_t>(lroundf((std::max)(-32768.0f, (std::min)(32767.0f, samples[i] * volume))));
    return wav;
}

// The start burst over the sustains, `seconds` long, as a 48 kHz 16-bit stereo wav.
static std::vector<char> mix_boost(float seconds, float volume, float* scaled = nullptr) {
    const std::vector<float> mix = mix_boost_pcm(seconds);
    return mix.empty() ? std::vector<char>{} : wav16(mix, 2, kBoostRate, volume, scaled);
}

// Interleaved samples down to mono at another rate, through a windowed-sinc filter (a cheap resampler, which FMOD
// may use when it has to convert, leaves a gritty crackle on sounds this bright).
static std::vector<float> mono_at(const float* in, size_t frames, int channels, uint32_t from, uint32_t to) {
    std::vector<float> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) sum += in[i * channels + c];
        mono[i] = sum / channels;
    }
    if (from == to || from == 0 || to == 0) return mono;

    constexpr int kTaps = 16;   // each side
    const double pi = 3.14159265358979;
    const double cutoff = (std::min)(1.0, static_cast<double>(to) / from);   // below both Nyquists
    // Output frame i sits at input frame i * from / to: its fraction is one of to / gcd steps, and those repeat,
    // so each one's filter is worked out once.
    uint32_t a = from, b = to;
    while (b) { const uint32_t t = a % b; a = b; b = t; }
    const uint32_t phases = to / a;
    if (phases > 4096) return mono;   // an odd rate: leave it to FMOD
    std::vector<float> table(static_cast<size_t>(phases) * 2 * kTaps);
    for (uint32_t p = 0; p < phases; ++p) {
        const double frac = static_cast<double>(p) / phases;
        float* w = &table[static_cast<size_t>(p) * 2 * kTaps];
        double total = 0.0;
        for (int k = 0; k < 2 * kTaps; ++k) {
            const double x = frac - (k - kTaps + 1);   // distance from input frame centre + k - kTaps + 1
            const double sx = x * cutoff * pi;
            const double sinc = fabs(sx) < 1e-9 ? 1.0 : sin(sx) / sx;
            w[k] = static_cast<float>(sinc * (0.5 + 0.5 * cos(pi * x / kTaps)));   // Hann window
            total += w[k];
        }
        for (int k = 0; k < 2 * kTaps; ++k) w[k] = static_cast<float>(w[k] / total);   // unity gain
    }

    const size_t out_frames = static_cast<size_t>(static_cast<uint64_t>(frames) * to / from);
    std::vector<float> out(out_frames);
    for (size_t i = 0; i < out_frames; ++i) {
        const uint64_t pos = static_cast<uint64_t>(i) * from;   // in 1/to of an input frame
        const long long centre = static_cast<long long>(pos / to);
        const float* w = &table[static_cast<size_t>((pos % to) / a) * 2 * kTaps];
        float sum = 0.0f;
        for (int k = 0; k < 2 * kTaps; ++k) {
            const long long n = centre + k - kTaps + 1;
            if (n >= 0 && n < static_cast<long long>(frames)) sum += mono[static_cast<size_t>(n)] * w[k];
        }
        out[i] = sum;
    }
    return out;
}

static void play_nitro_sound(NitroSound& sound, DWORD flags) {
    const float volume = g_sound_volume.load();
    if (volume <= 0.0f) return;
    const bool custom = sound.custom_path[0] != L'\0' && GetFileAttributesW(sound.custom_path) != INVALID_FILE_ATTRIBUTES;
    const float seconds = sound.embedded_name ? 0.0f : g_duration.load() + 3.0f;
    if (sound.wav.empty() || sound.volume != volume || sound.custom != custom || (!custom && sound.seconds != seconds)) {
        sound.wav.clear();
        if (custom) {
            HANDLE f = CreateFileW(sound.custom_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER size{};
                DWORD got = 0;
                if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < 256 * 1024 * 1024) {
                    sound.wav.resize(static_cast<size_t>(size.QuadPart));
                    if (!ReadFile(f, sound.wav.data(), static_cast<DWORD>(sound.wav.size()), &got, nullptr) || got != sound.wav.size())
                        sound.wav.clear();
                }
                CloseHandle(f);
            }
            if (volume < 1.0f) scale_wav(sound.wav, volume);
        } else if (!sound.embedded_name) {
            sound.wav = mix_boost(seconds, volume);
        } else {
            DWORD size = 0;
            if (const void* wav = embedded_file(sound.embedded_name, &size))
                sound.wav.assign(static_cast<const char*>(wav), static_cast<const char*>(wav) + size);
            if (volume < 1.0f) scale_wav(sound.wav, volume);
        }
        sound.volume = volume;
        sound.seconds = seconds;
        sound.custom = custom;
    }
    if (!sound.wav.empty())
        PlaySoundW(reinterpret_cast<LPCWSTR>(sound.wav.data()), NULL, flags | SND_MEMORY | SND_NODEFAULT);
}

static void start_nitro_audio() {
    PlaySoundW(NULL, NULL, 0);   // nothing may still be playing from a buffer that's about to be rebuilt
    play_nitro_sound(g_boost_sound, SND_ASYNC | SND_LOOP);
}

static void stop_nitro_audio() {
    PlaySoundW(NULL, NULL, 0);
    play_nitro_sound(g_stop_sound, SND_ASYNC);
}

// Drivatar nitro sound goes through the game's own FMOD instead: each burning Drivatar gets a channel at its car,
// so the game's listener gives it distance and direction. These are FMOD's API functions; each names itself in
// FMOD's error-trace string ("System::createSound", "ChannelControl::set3DAttributes", ...). Playing sounds this
// way comes from HonkHonk (github.com/suderek22/FH2_HonkHonk). Every FMOD call is made from inside the game's
// own Studio::System::update, on the thread it drives FMOD from.
static const uintptr_t kFmodStudioUpdate = 0x1A9C7B0;      // Studio::System::update
static const uintptr_t kFmodStudioGetLowLevel = 0x1A98080; // Studio::System::getLowLevelSystem
static const uintptr_t kFmodCreateSound = 0x1997740;       // System::createSound
static const uintptr_t kFmodPlaySound = 0x199B0F0;         // System::playSound
static const uintptr_t kFmodGetListener = 0x1997C00;       // System::get3DListenerAttributes
static const uintptr_t kFmodSet3DAttributes = 0x19A3F50;   // ChannelControl::set3DAttributes
static const uintptr_t kFmodSet3DMinMax = 0x19A4BC0;       // ChannelControl::set3DMinMaxDistance
static const uintptr_t kFmodSetVolume = 0x19A6470;         // ChannelControl::setVolume
static const uintptr_t kFmodSetPaused = 0x19A5E50;         // ChannelControl::setPaused
static const uintptr_t kFmodStop = 0x19A6730;              // ChannelControl::stop
static const uintptr_t kFmodSetPriority = 0x19AD070;       // Channel::setPriority
static const uintptr_t kFmodGetSoftwareFormat = 0x1999F70; // System::getSoftwareFormat
static const uintptr_t kFmodSetVolumeRamp = 0x19A6610;     // ChannelControl::setVolumeRamp

// FMOD_MODE bits, and the size of the game's FMOD_CREATESOUNDEXINFO
static const unsigned kFmodLoopOff = 0x1, kFmodLoopNormal = 0x2, kFmod3D = 0x10, kFmodCreateSample = 0x100,
                      kFmodOpenMemory = 0x800, kFmod3DLinearSquareRolloff = 0x400000;
static const unsigned kFmodExInfoSize = 0xD8;
static const int kMaxAiVoices = 6;
static const float kAiHearMin = 5.0f;     // full volume within this many metres...
static const float kAiHearMax = 150.0f;   // ...fading to silence here (an inverse rolloff never gets there)
static const float kAiFadeTime = 0.08f;   // fade in and out, rather than cutting the sound in mid-wave (a click)
static const uint64_t kAiStaleMs = 400;   // a car the physics hasn't updated for this long has gone (or the game paused)

struct FmodVec { float x, y, z; };
typedef int (*FmodStudioUpdateFn)(void* studio);
typedef int (*FmodGetLowLevelFn)(void* studio, void** core);
typedef int (*FmodCreateSoundFn)(void* core, const void* data, unsigned mode, void* exinfo, void** sound);
typedef int (*FmodPlaySoundFn)(void* core, void* sound, void* group, bool paused, void** channel);
typedef int (*FmodGetListenerFn)(void* core, int listener, FmodVec* pos, FmodVec* vel, FmodVec* fwd, FmodVec* up);
typedef int (*FmodSet3DFn)(void* channel, const FmodVec* pos, const FmodVec* vel, const FmodVec* alt_pan);
typedef int (*FmodMinMaxFn)(void* channel, float min_distance, float max_distance);
typedef int (*FmodSetVolumeFn)(void* channel, float volume);
typedef int (*FmodSetPausedFn)(void* channel, bool paused);
typedef int (*FmodStopFn)(void* channel);
typedef int (*FmodSetPriorityFn)(void* channel, int priority);
typedef int (*FmodGetSoftwareFormatFn)(void* core, int* rate, int* speaker_mode, int* raw_speakers);
typedef int (*FmodSetVolumeRampFn)(void* channel, bool ramp);

static FmodStudioUpdateFn g_orig_studio_update = nullptr;
static void* g_fmod_core = nullptr;
static void* g_fmod_boost = nullptr;     // made the first time a Drivatar burns
static float g_fmod_boost_gain = 1.0f;   // makes up for a sound being turned down to fit 16 bits
static float g_fmod_stop_gain = 1.0f;
static void* g_fmod_stop = nullptr;
static bool g_fmod_sounds_made = false;
static void* g_ai_channel[32];           // same index as g_ai_fx
static float g_ai_gain[32];              // each channel's fade, 0..1
static bool g_ai_was_burning[32];        // the last update's fx.burning, to hear a burst end
static std::atomic<int> g_ai_voices{0};
static bool g_fmod_play_logged = false;

template <typename Fn> static Fn fmod_fn(uintptr_t rva) {
    return reinterpret_cast<Fn>((uintptr_t)g_game_base + rva);
}

static int fmod_low_level(void* studio, void** core) {
    __try {
        return fmod_fn<FmodGetLowLevelFn>(kFmodStudioGetLowLevel)(studio, core);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1000;
    }
}

// A 3D sample from a whole sound file in memory (FMOD copies it).
static int fmod_create(const void* data, size_t size, unsigned loop, void** sound) {
    __try {
        unsigned char info[kFmodExInfoSize] = {};
        *(unsigned*)info = kFmodExInfoSize;
        *(unsigned*)(info + 4) = (unsigned)size;   // length
        return fmod_fn<FmodCreateSoundFn>(kFmodCreateSound)(
            g_fmod_core, data, loop | kFmod3D | kFmodCreateSample | kFmodOpenMemory | kFmod3DLinearSquareRolloff, info, sound);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1000;
    }
}

// The rate FMOD mixes at, or 0.
static int fmod_mix_rate() {
    __try {
        int rate = 0, mode = 0, raw = 0;
        return fmod_fn<FmodGetSoftwareFormatFn>(kFmodGetSoftwareFormat)(g_fmod_core, &rate, &mode, &raw) == 0 ? rate : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static bool fmod_listener(FmodVec* pos, FmodVec* vel) {
    __try {
        FmodVec fwd, up;
        return fmod_fn<FmodGetListenerFn>(kFmodGetListener)(g_fmod_core, 0, pos, vel, &fwd, &up) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Plays `sound` at `pos`. It's given the listener's velocity, so it has no Doppler shift. The channel, or nullptr.
// At the top priority: with the game's own sounds about, FMOD runs short of real voices and would otherwise keep
// switching ours off and back on (heard as crackling); and with volume ramping, so volume changes are smooth.
static void* fmod_play(void* sound, const FmodVec& pos, const FmodVec& vel, float volume) {
    __try {
        void* channel = nullptr;
        if (fmod_fn<FmodPlaySoundFn>(kFmodPlaySound)(g_fmod_core, sound, nullptr, true, &channel) != 0 || !channel)
            return nullptr;
        fmod_fn<FmodSetPriorityFn>(kFmodSetPriority)(channel, 0);
        fmod_fn<FmodSetVolumeRampFn>(kFmodSetVolumeRamp)(channel, true);
        fmod_fn<FmodSet3DFn>(kFmodSet3DAttributes)(channel, &pos, &vel, nullptr);
        fmod_fn<FmodMinMaxFn>(kFmodSet3DMinMax)(channel, kAiHearMin, kAiHearMax);
        fmod_fn<FmodSetVolumeFn>(kFmodSetVolume)(channel, volume);
        fmod_fn<FmodSetPausedFn>(kFmodSetPaused)(channel, false);
        return channel;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static void fmod_move(void* channel, const FmodVec& pos, const FmodVec& vel, float volume) {
    __try {
        fmod_fn<FmodSet3DFn>(kFmodSet3DAttributes)(channel, &pos, &vel, nullptr);
        fmod_fn<FmodSetVolumeFn>(kFmodSetVolume)(channel, volume);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static void fmod_stop(void* channel) {
    __try {
        fmod_fn<FmodStopFn>(kFmodStop)(channel);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// The player's sounds (nos_boost.wav / nos_stop.wav in the FnFNitro folder if they're there, else the built-in
// ones), at full scale: the Drivatar Nitro Volume slider sets the channels' volume. The built-in boost is mixed a little
// longer than the biggest tank (60 s), so a Drivatar emptying one never hears it start over; a custom one loops
// whole, as the player's does. Each is made mono (FMOD places 3D sounds itself) at the rate FMOD mixes at, so it
// plays as it is rather than through FMOD's resampler; a custom one that isn't a 16-bit wav goes to FMOD as it is.
static int fmod_create_wav(const char* data, size_t size, unsigned loop, int mix_rate, void** sound, float* gain) {
    *gain = 1.0f;
    const Pcm16 pcm = find_pcm16(const_cast<char*>(data), size);
    if (!pcm.samples || mix_rate <= 0) return fmod_create(data, size, loop, sound);
    const std::vector<float> in(pcm.samples, pcm.samples + pcm.frames * pcm.channels);
    float scaled = 1.0f;
    const std::vector<char> wav =
        wav16(mono_at(in.data(), pcm.frames, pcm.channels, pcm.rate, (uint32_t)mix_rate), 1, (uint32_t)mix_rate, 1.0f, &scaled);
    *gain = 1.0f / scaled;
    return fmod_create(wav.data(), wav.size(), loop, sound);
}

static void make_fmod_sounds() {
    g_fmod_sounds_made = true;
    const int mix_rate = fmod_mix_rate();
    const uint32_t rate = mix_rate > 0 ? (uint32_t)mix_rate : kBoostRate;
    char msg[200];
    std::string file;
    void* sound = nullptr;
    int r;
    if (read_whole_file(g_sound_boost_path, file)) {
        r = fmod_create_wav(file.data(), file.size(), kFmodLoopNormal, mix_rate, &sound, &g_fmod_boost_gain);
    } else {
        const std::vector<float> mix = mix_boost_pcm(63.0f);
        float scaled = 1.0f;
        const std::vector<char> wav =
            mix.empty() ? std::vector<char>{} : wav16(mono_at(mix.data(), mix.size() / 2, 2, kBoostRate, rate), 1, rate, 1.0f, &scaled);
        g_fmod_boost_gain = 1.0f / scaled;
        r = wav.empty() ? -1 : fmod_create(wav.data(), wav.size(), kFmodLoopOff, &sound);
    }
    if (r == 0) g_fmod_boost = sound;

    sound = nullptr;
    DWORD size = 0;
    const void* data = nullptr;
    if (read_whole_file(g_sound_stop_path, file)) {
        data = file.data();
        size = (DWORD)file.size();
    } else {
        data = embedded_file("nos_stop", &size);
    }
    int r2 = data ? fmod_create_wav((const char*)data, size, kFmodLoopOff, mix_rate, &sound, &g_fmod_stop_gain) : -1;
    if (r2 == 0) g_fmod_stop = sound;

    snprintf(msg, sizeof(msg), "Drivatar nitro sound: boost %s (%d), stop %s (%d); FMOD mixes at %d Hz.",
             g_fmod_boost ? "ready" : "failed", r, g_fmod_stop ? "ready" : "failed", r2, mix_rate);
    g_api->log(g_mod, msg);
}

// Where a Drivatar's sound goes: its car as drawn (the presentation's position), which moves in step with the
// listener; the physics position runs on its own clock, and the difference jitters the sound's volume and panning
// many times a second. The physics position if the presentation's can't be read or is far off.
static FmodVec ai_sound_pos(const AiFx& fx) {
    const FmodVec phys{fx.pos[0].load(), fx.pos[1].load(), fx.pos[2].load()};
    const uintptr_t pres = fx.pres.load();
    if (!pres) return phys;
    __try {
        const float* p = (const float*)(pres + kPresPosOffset);
        const float dx = p[0] - phys.x, dy = p[1] - phys.y, dz = p[2] - phys.z;
        if (std::isfinite(dx + dy + dz) && dx * dx + dy * dy + dz * dz < 20.0f * 20.0f) return FmodVec{p[0], p[1], p[2]};
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return phys;
}

// Starts, moves and stops each burning Drivatar's channel, and plays the stop sound where a burst ends. Only the
// nearest burning Drivatars in earshot get a voice; each fades in and out, so nothing starts or stops with a click.
static void drivatar_sound_tick() {
    static uint64_t last_tick = 0;
    const uint64_t now = GetTickCount64();
    const float step = (std::min)(0.1f, (now - last_tick) * 0.001f) / kAiFadeTime;
    last_tick = now;
    const float volume = g_ai_sound_volume.load();
    const bool on = g_drivatar_nitro.load() && g_enabled.load() && volume > 0.0f && g_pos_offset.load() >= 0;
    FmodVec lpos{}, lvel{};
    const bool listener = fmod_listener(&lpos, &lvel);

    // Who should be heard: burning, in earshot, and among the nearest
    float dist2[32];
    bool want[32];
    for (int i = 0; i < 32; ++i) {
        AiFx& fx = g_ai_fx[i];
        const float dx = fx.pos[0].load() - lpos.x, dy = fx.pos[1].load() - lpos.y, dz = fx.pos[2].load() - lpos.z;
        dist2[i] = dx * dx + dy * dy + dz * dz;
        want[i] = on && listener && now - fx.tick.load() <= kAiStaleMs && fx.burning.load() &&
                  dist2[i] < kAiHearMax * kAiHearMax;
    }
    for (int i = 0; i < 32; ++i) {
        if (!want[i]) continue;
        int nearer = 0;
        for (int j = 0; j < 32; ++j)
            if (j != i && want[j] && (dist2[j] < dist2[i] || (dist2[j] == dist2[i] && j < i))) ++nearer;
        if (nearer >= kMaxAiVoices) want[i] = false;
    }

    for (int i = 0; i < 32; ++i) {
        AiFx& fx = g_ai_fx[i];
        void*& channel = g_ai_channel[i];
        float& gain = g_ai_gain[i];
        const FmodVec pos = ai_sound_pos(fx);
        const bool burning = fx.burning.load();
        const bool fresh = now - fx.tick.load() <= kAiStaleMs;

        // A burst just ended where it can be heard: the stop sound there (fades out on its own)
        if (g_ai_was_burning[i] && !burning && fresh && on && listener && g_fmod_stop && channel &&
            dist2[i] < kAiHearMax * kAiHearMax && now - fx.since.load() >= 1000)
            fmod_play(g_fmod_stop, pos, lvel, volume * gain * g_fmod_stop_gain);
        g_ai_was_burning[i] = burning;

        if (want[i] && !channel) {
            if (!g_fmod_sounds_made) make_fmod_sounds();
            if (!g_fmod_boost || g_ai_voices.load() >= kMaxAiVoices) continue;
            gain = 0.0f;
            channel = fmod_play(g_fmod_boost, pos, lvel, 0.0f);
            if (!channel) continue;
            g_ai_voices.fetch_add(1);
            if (!g_fmod_play_logged) {
                // The sound is placed in the physics' world: the listener should be near the player's car
                g_fmod_play_logged = true;
                char msg[200];
                snprintf(msg, sizeof(msg), "Drivatar nitro sound at %.0f %.0f %.0f; listener at %.0f %.0f %.0f, your car at %.0f %.0f %.0f.",
                         pos.x, pos.y, pos.z, lpos.x, lpos.y, lpos.z, g_player_pos[0], g_player_pos[1], g_player_pos[2]);
                g_api->log(g_mod, msg);
            }
        }
        if (!channel) continue;

        gain = want[i] ? (std::min)(1.0f, gain + step) : (std::max)(0.0f, gain - step);
        if (gain > 0.0f || want[i]) {
            fmod_move(channel, pos, lvel, volume * gain * g_fmod_boost_gain);
        } else {
            fmod_stop(channel);   // already silent
            channel = nullptr;
            g_ai_voices.fetch_sub(1);
        }
    }
}

static int detour_studio_update(void* studio) {
    int r = g_orig_studio_update ? g_orig_studio_update(studio) : 0;
    if (!g_fmod_core && studio) {
        void* core = nullptr;
        if (fmod_low_level(studio, &core) == 0 && core) {
            g_fmod_core = core;
            g_api->log(g_mod, "FMOD found (Drivatar nitro sound plays through the game's own audio).");
        }
    }
    if (g_fmod_core) drivatar_sound_tick();
    return r;
}

// Fill the tank. One cooldown for every trigger below, since a single transition can set off several.
static void refill_nitro(const char* why, uint64_t now) {
    static uint64_t last_refill = 0;
    if (now - last_refill < 3000) return;
    last_refill = now;
    float full = g_duration.load();
    if (g_current_fuel.load() < full - 0.05f || g_depleted_lockout.load()) {
        g_current_fuel.store(full);
        g_depleted_lockout.store(false);
        char msg[128];
        snprintf(msg, sizeof(msg), "Nitro tank refilled (%s).", why);
        g_api->log(g_mod, msg);
    }
}

// Starting or leaving a race, quitting one, changing car, fast travelling and rewinding all hide the HUD speedometer
// while the car keeps simulating (a pause doesn't: physics stops with it, and on resuming the speedometer comes
// back before physics does). So: arm when the speedometer has been off for 2 s with physics running, or when
// physics resumes with it off, and act when it comes back.
//
// What to do depends on where the car ends up (seen in the logs):
//   - a rewind replays the car's recorded path backwards for several seconds, ending 40 m or more from where the
//     speedometer went off, on a spot the car passed a few seconds earlier: give back the nitro used since then;
//   - anything else (race start, quit, car change, fast travel) fills the tank.
// The path is recorded while the speedometer is on, and frozen while it is off.
struct FuelSample {
    float pos[3];
    float fuel;
    uint64_t tick;
};

static float dist3(const float* a, const float* b) {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void nitro_hud_events(bool physics, bool speedo, uint64_t now) {
    static std::vector<FuelSample> path;   // newest last; one minute at 10 samples a second
    static float freeze_pos[3] = {0.0f, 0.0f, 0.0f};
    static bool prev_physics = false;
    static bool prev_speedo = false;
    static uint64_t off_since = 0;
    static bool armed = false;

    float cur[3] = {g_player_pos[0], g_player_pos[1], g_player_pos[2]};
    bool pos_fresh = (now - g_player_pos_tick.load()) < 200;

    if (speedo && physics && pos_fresh && !armed && (path.empty() || now - path.back().tick >= 100)) {
        path.push_back({{cur[0], cur[1], cur[2]}, g_current_fuel.load(), now});
        if (path.size() > 600) path.erase(path.begin());
    }

    if (prev_speedo && !speedo) {   // the speedometer just went off: where the car was
        freeze_pos[0] = cur[0]; freeze_pos[1] = cur[1]; freeze_pos[2] = cur[2];
    }
    if (physics && !speedo) {
        if (off_since == 0) off_since = now;
        if (!prev_physics || now - off_since >= 2000) armed = true;
    } else {
        off_since = 0;
    }

    if (speedo && physics && armed) {
        armed = false;
        int best = -1;
        float best_d = 25.0f;
        if (dist3(cur, freeze_pos) > 40.0f) {
            for (int i = 0; i < (int)path.size(); ++i) {
                float d = dist3(cur, path[i].pos);
                if (d < best_d) { best_d = d; best = i; }
            }
        }
        char msg[128];
        if (best >= 0) {   // a rewind
            float then = path[best].fuel;
            path.resize((size_t)best + 1);   // what came after that point was undone
            if (g_refund_on_rewind.load() && then > g_current_fuel.load() + 0.05f) {
                g_current_fuel.store(then);
                if (then >= 2.0f) g_depleted_lockout.store(false);
                snprintf(msg, sizeof(msg), "Rewind: nitro tank restored to %.1f s (back %.0f m, matched within %.0f m).", then,
                         dist3(cur, freeze_pos), best_d);
                g_api->log(g_mod, msg);
            }
        } else {
            path.clear();   // somewhere new: the old path no longer leads here
            if (g_refill_on_start.load()) refill_nitro("race start / quit / car change / fast travel", now);
        }
    }
    prev_physics = physics;
    prev_speedo = speedo;
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

    char log_buf[640];
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

    void* bf_entry = (void*)((uintptr_t)base + 0x13B0D70);
    FH2Hook* hook_bfe = g_api->hook_function(g_mod, "CarPresentation11::UpdateBackfire", bf_entry, (void*)detour_backfire_entry, (void**)&g_orig_backfire_entry);
    if (hook_bfe && g_api->enable_hooks(g_mod, hook_bfe)) {
        g_api->log(g_mod, "UpdateBackfire entry hooked (learns which exhaust tips are real).");
    } else {
        g_api->log(g_mod, "Could not hook the UpdateBackfire entry.");
    }

    void* frame_fn = (void*)((uintptr_t)base + 0xFA78A0);
    FH2Hook* hook_frame = g_api->hook_function(g_mod, "GameLogic::FrameStep", frame_fn, (void*)detour_game_frame, (void**)&g_orig_game_frame);
    if (hook_frame && g_api->enable_hooks(g_mod, hook_frame)) {
        g_api->log(g_mod, "Game logic frame step hooked (drives the exhaust flames during nitro).");
    } else {
        g_api->log(g_mod, "Could not hook the game logic frame step; flames will only fire on the game's own events.");
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

    // ForzaCar's per-frame update, to learn each car's kind (the player's, a Drivatar, traffic). The kind
    // function is called, so check it's what the game has on disk first.
    const char* game = (const char*)base;
    if (memcmp(game + FH2_GAME_FORZACAR_KIND, FH2_GAME_FORZACAR_KIND_BYTES, sizeof FH2_GAME_FORZACAR_KIND_BYTES - 1) == 0) {
        g_forzacar_kind = (ForzaCarKindFn)(game + FH2_GAME_FORZACAR_KIND);
        FH2Hook* hook_car = g_api->hook_function(g_mod, "ForzaCar::Update", (void*)(game + FH2_GAME_FORZACAR_UPDATE),
                                                 (void*)detour_forzacar_update, (void**)&g_orig_forzacar_update);
        if (hook_car && g_api->enable_hooks(g_mod, hook_car)) {
            g_api->log(g_mod, "ForzaCar::Update hooked (tells the player's car from Drivatars and traffic).");
        } else {
            g_api->log(g_mod, "Could not hook ForzaCar::Update; the player's car is found by position and Drivatars get no nitro.");
        }
    } else {
        g_api->log(g_mod, "ForzaCar's kind function isn't as expected; the player's car is found by position and Drivatars get no nitro.");
    }

    FH2Hook* hook_fmod = g_api->hook_function(g_mod, "FMOD Studio::System::update", (void*)(game + kFmodStudioUpdate),
                                              (void*)detour_studio_update, (void**)&g_orig_studio_update);
    if (!hook_fmod || !g_api->enable_hooks(g_mod, hook_fmod))
        g_api->log(g_mod, "Could not hook FMOD's update; Drivatar nitro has no sound.");

    void* view_mounted = (void*)((uintptr_t)base + 0x1034360);
    void* view_chase = (void*)((uintptr_t)base + 0x1034430);
    FH2Hook* hook_vm = g_api->hook_function(g_mod, "CameraMounted::ViewMatrix", view_mounted, (void*)detour_view_mounted, (void**)&g_orig_view_mounted);
    FH2Hook* hook_vc = g_api->hook_function(g_mod, "CameraFollow::ViewMatrix", view_chase, (void*)detour_view_chase, (void**)&g_orig_view_chase);
    bool shake_mounted = hook_vm && g_api->enable_hooks(g_mod, hook_vm);
    bool shake_chase = hook_vc && g_api->enable_hooks(g_mod, hook_vc);
    g_api->log(g_mod, shake_mounted && shake_chase ? "Camera view matrices hooked (camera shake during nitro)."
                      : shake_mounted || shake_chase ? "Only one camera view matrix could be hooked; camera shake works in some views."
                      : "Could not hook the camera view matrices; no camera shake.");

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

        // Poll all 4 controller ports for the nitro button (LB by default)
        bool button_pressed = false;
        for (DWORD user_idx = 0; user_idx < 4; ++user_idx) {
            XINPUT_STATE state{};
            if (XInputGetState(user_idx, &state) == ERROR_SUCCESS) {
                if (nitro_button_down(state.Gamepad)) {
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

        if (g_refund_on_rewind.load() || g_refill_on_start.load())
            nitro_hud_events(physics_active, (now_ms - g_last_speedo_tick.load()) < 200, now_ms);

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

        static bool link_reported = false;
        if (!link_reported) {
            if (player_known_by_kind(now_ms)) {
                link_reported = true;
                g_api->log(g_mod, "Car kinds found: the player's car is now told apart from Drivatars and traffic by the game's own kind.");
            } else if (g_link_ok.load() == 0 && g_link_bad.load() > 600) {
                link_reported = true;
                snprintf(log_buf, sizeof(log_buf), "Car kinds unavailable (presentation vtable game+0x%llX, simulation vtable game+0x%llX); "
                         "finding the player's car by position, no Drivatar nitro.",
                         (unsigned long long)g_link_bad_pres_vt.load(), (unsigned long long)g_link_bad_dyn_vt.load());
                g_api->log(g_mod, log_buf);
            }
        }

        // How the Drivatars used their nitro, once a minute while they do
        static uint64_t ai_report_tick = 0;
        if (ai_report_tick == 0) ai_report_tick = now_ms;
        if (now_ms - ai_report_tick >= 60000) {
            ai_report_tick = now_ms;
            const uint32_t bursts = g_ai_bursts.exchange(0);
            const float total = g_ai_burst_time.exchange(0.0f);
            if (bursts) {
                int n = snprintf(log_buf, sizeof(log_buf), "Drivatar nitro, last minute: %u bursts, %.1f s on average; let off for",
                                 bursts, total / bursts);
                for (int i = 0; i < kOffCount && n > 0 && n < (int)sizeof(log_buf); ++i) {
                    const uint32_t c = g_ai_letoffs[i].exchange(0);
                    if (c) n += snprintf(log_buf + n, sizeof(log_buf) - n, " %s %u,", kAiLetOffNames[i], c);
                }
                if (n > 0 && n < (int)sizeof(log_buf)) log_buf[n - 1] = '.';
                g_api->log(g_mod, log_buf);
            }
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

// On-screen debug overlay: a transparent, click-through window laid over the game window, drawn with GDI.
// Switched on from the Mods page. Needs the game in borderless or windowed mode, like ShowFPS.
static char g_overlay_lines[24][180];
static int g_overlay_count = 0;
static HWND g_overlay_hwnd = nullptr;

static void overlay_line(const char* fmt, ...) {
    if (g_overlay_count >= 24) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_overlay_lines[g_overlay_count], sizeof(g_overlay_lines[0]), fmt, ap);
    va_end(ap);
    ++g_overlay_count;
}

static void build_overlay_lines() {
    g_overlay_count = 0;
    uint64_t now = GetTickCount64();
    uint64_t phys_age = now - g_last_physics_tick.load();
    uint64_t speedo_age = now - g_last_speedo_tick.load();
    uint64_t pos_age = now - g_player_pos_tick.load();
    bool driving = phys_age < 200 && speedo_age < 200;
    int up = g_up_axis.load();
    int off = g_pos_offset.load();

    overlay_line("FnFNitro debug");
    overlay_line("nitro: enabled=%d active=%d lockout=%d fuel=%.1f/%.0f boost=%.2f",
                 g_enabled.load() ? 1 : 0, g_nitro_active.load() ? 1 : 0, g_depleted_lockout.load() ? 1 : 0,
                 g_current_fuel.load(), g_duration.load(), g_current_boost.load());
    overlay_line("driving=%d  physics age=%llums  speedometer age=%llums", driving ? 1 : 0,
                 (unsigned long long)phys_age, (unsigned long long)speedo_age);
    if (off >= 0) {
        overlay_line("player car: position offset=+0x%X  up axis=%c  position age=%llums", off,
                     up >= 0 ? "XYZ"[up] : '?', (unsigned long long)pos_age);
    } else {
        overlay_line("player car: not located yet (drive for a few seconds)  up axis=%c", up >= 0 ? "XYZ"[up] : '?');
    }
    int kinds[3] = {0, 0, 0};
    for (const auto& c : g_cars) {
        int k = c.kind.load();
        if (c.dyn.load() && now - c.tick.load() < kKindFreshMs && k >= 0 && k < 3) ++kinds[k];
    }
    int burning = 0;
    for (const auto& s : g_ai)
        if (s.burning && now - s.seen < kKindFreshMs) ++burning;
    overlay_line("car kinds: link ok=%u bad=%u | player by %s | cars: player=%d drivatar=%d traffic=%d",
                 g_link_ok.load(), g_link_bad.load(), player_known_by_kind(now) ? "kind" : "position",
                 kinds[0], kinds[1], kinds[2]);
    overlay_line("drivatar nitro: %s | boosting=%d | flames driven=%u | sounds playing=%d/%d (FMOD %s)",
                 !g_drivatar_nitro.load() ? "off" : kinds[1] ? "on" : "on, no Drivatars seen", burning,
                 g_ai_flames.load(), g_ai_voices.load(), kMaxAiVoices, g_fmod_core ? "found" : "not found");
    for (int i = 0; i < 32 && g_overlay_count < 24; ++i) {
        const AiNitro& s = g_ai[i];
        if (!s.dyn || now - s.seen >= kKindFreshMs) continue;
        const uintptr_t pres = g_ai_fx[i].pres.load();
        overlay_line("  drivatar %2d: %-19s fuel %4.1f/%.0f  %3.0f km/h (moving %3.0f)  tips %02X%s  sound %s",
                     i, s.state ? s.state : "?", s.fuel, ai_tank(), s.speed_kmh, s.real_speed * 3.6f,
                     pres ? learned_tips(pres) : 0,
                     pres ? "" : " (no presentation)", g_ai_channel[i] ? "on" : "off");
    }
    overlay_line("exhaust tips: %s  (M=flagged by game events, -=not) | events seen=%u mask=%02X (needs one real backfire event)",
                 g_tip_diag[0] ? g_tip_diag : "(none yet)", g_real_events.load(), g_real_mask.load() & 0xFF);
    overlay_line("flames: calls=%u matched=%u tips=%d | routine ran=%u | driven=%u",
                 g_bf_calls.load(), g_bf_matched.load(), g_bf_tips.load(), g_bf_total.load(), g_bf_driven.load());
    DWORD ft = g_frame_tid.load(), bt = g_bf_tid.load();
    overlay_line("threads: game frame=%lu backfire=%lu %s | physics=%lu", (unsigned long)ft, (unsigned long)bt,
                 (ft != 0 && ft == bt) ? "(same: flame driver runs)" : "(different: flame driver blocked)",
                 (unsigned long)g_phys_tid.load());
}

static LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH key = CreateSolidBrush(RGB(255, 0, 255));      // colour-keyed: becomes see-through
        FillRect(dc, &rc, key);
        DeleteObject(key);

        const int line_h = 17;
        int box_h = g_overlay_count * line_h + 10;
        RECT box = {0, 0, rc.right, box_h};
        HBRUSH bg = CreateSolidBrush(RGB(14, 14, 14));
        FillRect(dc, &box, bg);
        DeleteObject(bg);

        HFONT font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(90, 255, 130));
        for (int i = 0; i < g_overlay_count; ++i) {
            TextOutA(dc, 6, 5 + i * line_h, g_overlay_lines[i], (int)strlen(g_overlay_lines[i]));
        }
        SelectObject(dc, old);
        DeleteObject(font);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

struct FindGame {
    DWORD pid;
    HWND best;
    long best_area;
};

static BOOL CALLBACK find_game_window(HWND hwnd, LPARAM lp) {
    FindGame* f = (FindGame*)lp;
    if (hwnd == g_overlay_hwnd || !IsWindowVisible(hwnd)) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != f->pid) return TRUE;
    if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    RECT rc;
    GetClientRect(hwnd, &rc);
    long area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > f->best_area) {
        f->best_area = area;
        f->best = hwnd;
    }
    return TRUE;
}

static void debug_overlay_thread() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = overlay_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FnFNitroDebugOverlay";
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    if (!RegisterClassExW(&wc)) return;

    HWND hw = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              wc.lpszClassName, L"FnFNitro debug", WS_POPUP, 0, 0, 100, 100,
                              nullptr, nullptr, wc.hInstance, nullptr);
    if (!hw) return;
    g_overlay_hwnd = hw;
    SetLayeredWindowAttributes(hw, RGB(255, 0, 255), 0, LWA_COLORKEY);

    bool shown = false;
    for (;;) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!g_debug.load()) {
            if (shown) { ShowWindow(hw, SW_HIDE); shown = false; }
            continue;
        }

        FindGame f{GetCurrentProcessId(), nullptr, 0};
        EnumWindows(find_game_window, (LPARAM)&f);
        static HWND logged_for = (HWND)1;
        if (f.best != logged_for && g_api && g_mod) {
            logged_for = f.best;
            char b[200];
            if (f.best) {
                RECT r0;
                GetClientRect(f.best, &r0);
                snprintf(b, sizeof(b), "debug overlay: game window found (%ldx%ld client)", r0.right, r0.bottom);
            } else {
                snprintf(b, sizeof(b), "debug overlay: no game window found, nothing to draw on");
            }
            g_api->log(g_mod, b);
        }
        if (!f.best) continue;
        RECT cr;
        GetClientRect(f.best, &cr);
        POINT origin = {0, 0};
        ClientToScreen(f.best, &origin);

        build_overlay_lines();
        int h = g_overlay_count * 17 + 10;
        MoveWindow(hw, origin.x + 12, origin.y + 12, 1100, h, TRUE);
        SetWindowPos(hw, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        shown = true;
        InvalidateRect(hw, nullptr, TRUE);
    }
}

// The tank as 0-100. Exported so other mods in the process (VirtualDashboard) can read it.
static int nitro_percent() {
    float duration = g_duration.load();
    float pct = (duration > 0.001f) ? g_current_fuel.load() / duration * 100.0f : 0.0f;
    return static_cast<int>(std::lround((std::min)(100.0f, (std::max)(0.0f, pct))));
}

extern "C" __declspec(dllexport) int FnFNitro_GetNitroPercent() { return nitro_percent(); }

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    g_api = api;

    FH2ModInfo info{};
    info.size             = sizeof(info);
    info.name             = "NitroSystem";
    info.version          = "1.5.0";
    info.github_repo      = "qiangqiang101/NitroSystem";
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = 1;
    info.description      = "Fast & Furious Nitro system ported to Forza Horizon 2 (All Cars - LB to activate)";
    info.offline_only     = 1;
    
    g_mod = api->register_mod(&info);
    if (!g_mod) return 1;

    api->log(g_mod, "NitroSystem initializing (LB to boost)...");

    // Initialize paths & load persistent settings from FnFNitro.ini
    init_paths();
    load_config();
    apply_hud_theme();   // before the game first reads the HUD files
    apply_backfire_effect();
    remove_legacy_files();

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

        FH2Setting s_debug{};
        s_debug.size        = sizeof(s_debug);
        s_debug.key         = "DebugOverlay";
        s_debug.label       = "Nitro Debug Overlay";
        s_debug.description = "Draws nitro and flame debug info over the game (needs borderless/windowed mode).";
        s_debug.value       = g_debug.load() ? 1 : 0;
        s_debug.on_change   = on_setting_changed;
        api->add_setting(g_mod, &s_debug);

        FH2Setting s_backfire{};
        s_backfire.size          = sizeof(s_backfire);
        s_backfire.key           = "BackfireOnAllCars";
        s_backfire.label         = "Backfire on All Cars";
        s_backfire.description   = "Lets every car backfire, which the nitro exhaust flames need. Off leaves the game's own backfire settings alone. Takes effect after a restart.";
        s_backfire.value         = g_backfire_all_cars.load() ? 1 : 0;
        s_backfire.needs_restart = 1;
        s_backfire.on_change     = on_setting_changed;
        api->add_setting(g_mod, &s_backfire);

        FH2Setting s_refill{};
        s_refill.size        = sizeof(s_refill);
        s_refill.key         = "RefillOnRaceStart";
        s_refill.label       = "Refill Nitro on Race Start";
        s_refill.description = "Fills the nitro tank when a race starts, you quit one, you change car or you fast travel.";
        s_refill.value       = g_refill_on_start.load() ? 1 : 0;
        s_refill.on_change   = on_setting_changed;
        api->add_setting(g_mod, &s_refill);

        FH2Setting s_rewind{};
        s_rewind.size        = sizeof(s_rewind);
        s_rewind.key         = "RefundOnRewind";
        s_rewind.label       = "Refund Nitro on Rewind";
        s_rewind.description = "Rewinding gives back the nitro you used since the point you rewind to.";
        s_rewind.value       = g_refund_on_rewind.load() ? 1 : 0;
        s_rewind.on_change   = on_setting_changed;
        api->add_setting(g_mod, &s_rewind);

        FH2Setting s_rumble{};
        s_rumble.size        = sizeof(s_rumble);
        s_rumble.key         = "Rumble";
        s_rumble.label       = "Nitro Controller Rumble";
        s_rumble.description = "Rumbles the controller while Nitro is active. The game's own rumble isn't affected.";
        s_rumble.value       = g_rumble.load() ? 1 : 0;
        s_rumble.on_change   = on_setting_changed;
        api->add_setting(g_mod, &s_rumble);

        FH2Setting s_drivatar{};
        s_drivatar.size        = sizeof(s_drivatar);
        s_drivatar.key         = "DrivatarNitro";
        s_drivatar.label       = "Drivatar Nitro";
        s_drivatar.description = "Drivatars use nitro too, whenever they can (not before the race starts), from "
                                 "a tank with twice your capacity and recharge, with flames and sound. Not once they're over "
                                 "100 m ahead of you, so you can catch up. Applies right away.";
        s_drivatar.value       = g_drivatar_nitro.load() ? 1 : 0;
        s_drivatar.on_change   = on_setting_changed;
        api->add_setting(g_mod, &s_drivatar);

        // Platform 2.8.0 shows the button and the theme by name; older ones only have sliders, so there they're
        // picked by number (listed in the description).
        static const char* button_choices[k_button_count];
        for (int i = 0; i < k_button_count; ++i) button_choices[i] = k_buttons[i].label;
        static const char* const theme_choices[] = {"Original", "Fast & Furious"};
        const bool choices = api->api_version >= 9;

        if (choices) {
            FH2Setting s_button{};
            s_button.size         = sizeof(s_button);
            s_button.key          = "NitroButton";
            s_button.label        = "Nitro Button";
            s_button.description  = "Which button burns nitro. Pick one the game doesn't use while driving. Applies right away.";
            s_button.kind         = FH2_SETTING_CHOICE;
            s_button.choices      = button_choices;
            s_button.choice_count = k_button_count;
            s_button.value        = button_index();
            s_button.on_change    = on_setting_changed;
            api->add_setting(g_mod, &s_button);

            FH2Setting s_hud{};
            s_hud.size          = sizeof(s_hud);
            s_hud.key           = "HudTheme";
            s_hud.label         = "HUD Theme";
            s_hud.description   = "Original: the game's HUD with a yellow NOS bar. Fast & Furious: cyan NOS bar, gears and assist icons.";
            s_hud.kind          = FH2_SETTING_CHOICE;
            s_hud.choices       = theme_choices;
            s_hud.choice_count  = 2;
            s_hud.value         = g_hud_theme.load() ? 1 : 0;
            s_hud.needs_restart = 1;
            s_hud.on_change     = on_setting_changed;
            api->add_setting(g_mod, &s_hud);
        } else if (api->api_version >= 5) {
            FH2Setting s_button{};
            s_button.size          = sizeof(s_button);
            s_button.key           = "NitroButton";
            s_button.label         = "Nitro Button";
            s_button.description   = "1 LB, 2 RB, 3 A, 4 B, 5 X, 6 Y, 7 LS click, 8 RS click, 9 D-Pad Up, 10 D-Pad Down, "
                                     "11 D-Pad Left, 12 D-Pad Right, 13 Back, 14 Start, 15 LT, 16 RT. Pick one the game "
                                     "doesn't use while driving. Applies right away.";
            s_button.kind          = FH2_SETTING_SLIDER;
            s_button.min           = 1.0f;
            s_button.max           = (float)k_button_count;
            s_button.step          = 1.0f;
            s_button.slider_value  = (float)(button_index() + 1);
            s_button.display_scale = 1.0f;
            s_button.decimals      = 0;
            s_button.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_button);

            FH2Setting s_hud{};
            s_hud.size          = sizeof(s_hud);
            s_hud.key           = "HudTheme";
            s_hud.label         = "HUD Theme";
            s_hud.description   = "1 Original (yellow NOS bar), 2 Fast & Furious (cyan NOS bar, gears and assist icons).";
            s_hud.kind          = FH2_SETTING_SLIDER;
            s_hud.min           = 1.0f;
            s_hud.max           = 2.0f;
            s_hud.step          = 1.0f;
            s_hud.slider_value  = g_hud_theme.load() ? 2.0f : 1.0f;
            s_hud.display_scale = 1.0f;
            s_hud.decimals      = 0;
            s_hud.needs_restart = 1;
            s_hud.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_hud);
        }

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

            FH2Setting s_volume{};
            s_volume.size          = sizeof(s_volume);
            s_volume.key           = "SoundVolume";
            s_volume.label         = "Nitro Sound Volume";
            s_volume.description   = "Volume of the boost and stop sounds. The game's own volume sliders don't change them.";
            s_volume.kind          = FH2_SETTING_SLIDER;
            s_volume.min           = 0.0f;
            s_volume.max           = 1.0f;
            s_volume.step          = 0.05f;
            s_volume.slider_value  = g_sound_volume.load();
            s_volume.display_scale = 100.0f;
            s_volume.decimals      = 0;
            s_volume.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_volume);

            FH2Setting s_ai_volume{};
            s_ai_volume.size          = sizeof(s_ai_volume);
            s_ai_volume.key           = "DrivatarSoundVolume";
            s_ai_volume.label         = "Drivatar Nitro Volume";
            s_ai_volume.description   = "Volume of the Drivatars' nitro, heard from their cars (with Drivatar Nitro on). 0 silences it.";
            s_ai_volume.kind          = FH2_SETTING_SLIDER;
            s_ai_volume.min           = 0.0f;
            s_ai_volume.max           = 1.0f;
            s_ai_volume.step          = 0.05f;
            s_ai_volume.slider_value  = g_ai_sound_volume.load();
            s_ai_volume.display_scale = 100.0f;
            s_ai_volume.decimals      = 0;
            s_ai_volume.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_ai_volume);

            FH2Setting s_shake{};
            s_shake.size          = sizeof(s_shake);
            s_shake.key           = "CameraShake";
            s_shake.label         = "Nitro Camera Shake";
            s_shake.description   = "How much the camera shakes while Nitro is active, in the driver, hood, bumper and chase views. 0 turns it off.";
            s_shake.kind          = FH2_SETTING_SLIDER;
            s_shake.min           = 0.0f;
            s_shake.max           = 1.0f;
            s_shake.step          = 0.05f;
            s_shake.slider_value  = g_camera_shake.load();
            s_shake.display_scale = 100.0f;
            s_shake.decimals      = 0;
            s_shake.on_slide      = on_slider_changed;
            api->add_setting(g_mod, &s_shake);

        }
    }

    // Ensure all cars support exhaust backfire flames in gamedbRC (BackfireOnAllCars=0 in the ini turns this off;
    // the nitro flames then only work on cars the game already lets backfire)
    if (g_backfire_all_cars.load() && api->api_version >= 2 && api->add_sql) {
        api->add_sql(g_mod, "UPDATE BackfireLevels SET BackfireLevel = 3;");
    }

    // Launch background worker thread
    std::thread(nitro_worker_thread).detach();
    std::thread(debug_overlay_thread).detach();

    api->log(g_mod, "Fast & Furious NOS initialized successfully.");
    return 0;
}
