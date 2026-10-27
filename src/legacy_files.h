// Files an older FnFNitro (1.2 and before) copied into mediaoverride by hand, from the repo's dist\mediaoverride.
// This version doesn't use them: the HUD and the flame effect are written from the DLL. remove_legacy_files() deletes
// one only when its size and FNV-1a hash match, so a file you or another mod changed is never touched.
// Generated; the gears and the files the mod still writes aren't listed.
#pragma once
#include <cstdint>

struct LegacyFile { const wchar_t* path; uint64_t size; uint64_t hash; };
static const LegacyFile k_legacy_files[] = {
    {L"Particles\\Materials\\Fire\\backfire_blue.materialbin", 352, 0x93a1d358cefe3c64ULL},
    {L"Particles\\Materials\\Fire\\carBackfire.materialbin", 352, 0x93a1d358cefe3c64ULL},
    {L"Particles\\Materials\\backfire.materialbin", 352, 0x4fdfbbe1a997a105ULL},
    {L"Particles\\Materials\\backfire_blue.materialbin", 352, 0x93a1d358cefe3c64ULL},
    {L"Particles\\Materials\\backfire_purple.materialbin", 352, 0xcd54be5d8eafee75ULL},
    {L"Particles\\Shaders\\backfire\\Backfire.shaderbin", 788, 0x3a8a4aa6ef317357ULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLightScenario.durango.pso", 10548, 0xb2a99406b4eabab4ULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLightScenario.durango.vso", 28452, 0x406dadf48f7dc5c4ULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLightScenario.pc.pso", 11020, 0x012999dd66105d6dULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLightScenario.pc.vso", 31580, 0xd2e9e4ad578e4839ULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLowResLightScenario.durango.pso", 11316, 0xc6b3b8e420556c75ULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLowResLightScenario.durango.vso", 28460, 0x44b361f22047286bULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLowResLightScenario.pc.pso", 11692, 0x416f1a3b116cbe82ULL},
    {L"Particles\\Shaders\\backfire\\backfireParticleLowResLightScenario.pc.vso", 31580, 0xd2e9e4ad578e4839ULL},
    {L"ui\\textures\\Anthem\\HUD\\HUD_XPBar_TipGlow.swatchbin", 1107, 0x7b2f89c2413ad764ULL},
    {L"ui\\textures\\Anthem\\HUD\\SkillBarTipGlow.swatchbin", 4179, 0xe86615d5aff61822ULL},
    {L"ui\\textures\\Anthem\\HUD\\SkillsLargeGlow.swatchbin", 65619, 0x98f09b4c3b4527bdULL},
    {L"ui\\textures\\data_bound\\SpireHUD\\HUD_Dial_Assist_Analogue_NOS_Off.swatchbin", 32851, 0xb6aa372f8cdca9e4ULL},
    {L"ui\\textures\\data_bound\\SpireHUD\\HUD_Dial_Assist_Analogue_NOS_On.swatchbin", 32851, 0x8f9b982e3dbe73ceULL},
    {L"ui\\textures\\data_bound\\SpireHUD\\HUD_Dial_Assist_Digital_NOS_Off.swatchbin", 32851, 0x39f758561b7ea44cULL},
    {L"ui\\textures\\data_bound\\SpireHUD\\HUD_Dial_Assist_Digital_NOS_On.swatchbin", 32851, 0x64c65c52b7db3ae1ULL},
};
