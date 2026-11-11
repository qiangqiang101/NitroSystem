/* NitroSystemControl.h - let another FH2 mod switch Nitro System's NOS off for the player, and read its state.
   Copy this file into your mod. Plain C, works from C and C++.

   Through the FH2 Mod Platform (recommended): Nitro System publishes this struct as "NitroSystem.Control".
   Mods start in alphabetical order, so look it up from your own thread once every mod has started:

       api->wait_for_mods(10000);
       const NitroSystemControl* nos =
           (const NitroSystemControl*)api->get_interface(NITROSYSTEM_CONTROL_NAME, NITROSYSTEM_CONTROL_VERSION);
       if (nos) nos->set_blocked("ANNADroneMode", 1);    // drone flying: no NOS for the player
       ...
       if (nos) nos->set_blocked("ANNADroneMode", 0);    // back in the car: NOS allowed again

   NULL means Nitro System isn't installed (or is too old): carry on without it.

   Without the platform's interfaces: the same functions are exported by FnFNitro.dll as
   FnFNitro_SetBlocked, FnFNitro_IsBlocked, FnFNitro_IsActive and FnFNitro_GetNitroPercent
   (GetModuleHandleW(L"FnFNitro.dll") + GetProcAddress).

   Blocking is per caller: each mod passes its own name (`who`, up to 31 characters), and NOS stays off while at
   least one of them blocks it, so two mods can't undo each other. While blocked, the NOS gauge is hidden, the NOS button does nothing and
   NOS that was burning stops at once; the tank still recharges, and Drivatars aren't affected. Blocks aren't saved:
   they're gone when the game closes. Every function may be called from any thread, at any time. */
#ifndef NITROSYSTEM_CONTROL_H
#define NITROSYSTEM_CONTROL_H

#include <stdint.h>

#define NITROSYSTEM_CONTROL_NAME    "NitroSystem.Control"
#define NITROSYSTEM_CONTROL_VERSION 1u

typedef struct NitroSystemControl {
    uint32_t size;   /* sizeof(NitroSystemControl) of the Nitro System that published it */

    /* ---- version 1 ---- */
    /* Block (1) or allow again (0) the player's NOS on behalf of `who`. */
    void (*set_blocked)(const char* who, int blocked);
    /* 1 while any mod blocks the player's NOS, else 0. */
    int (*is_blocked)(void);
    /* 1 while the player's NOS is burning, else 0. */
    int (*is_active)(void);
    /* How full the player's tank is, 0 to 100. */
    int (*nitro_percent)(void);

    /* Later versions only add members here, at the end. */
} NitroSystemControl;

#endif
