# Switching Nitro System's NOS off from another mod

Another FH2 Mod Platform mod can switch the player's NOS off while it needs to, and turn it on again:

```c
const NitroSystemControl* nos =
    (const NitroSystemControl*)api->get_interface(NITROSYSTEM_CONTROL_NAME, NITROSYSTEM_CONTROL_VERSION);
if (nos) nos->set_blocked("MyMod", 1);   // NOS off
if (nos) nos->set_blocked("MyMod", 0);   // NOS on again
```

## Setup

1. Copy [`include/NitroSystemControl.h`](../include/NitroSystemControl.h) into your mod and include it.
2. Look the interface up **from your own thread**, after every mod has started. Mods start in name order, so Nitro System may not have published it yet while your `fh2mod_init` runs, and `wait_for_mods` returns straight away if you call it from there.

```c
#include "NitroSystemControl.h"

static const NitroSystemControl* g_nos;   /* NULL: Nitro System isn't installed */

static DWORD WINAPI find_nitro_system(LPVOID unused) {
    (void)unused;
    g_api->wait_for_mods(10000);
    g_nos = (const NitroSystemControl*)g_api->get_interface(NITROSYSTEM_CONTROL_NAME, NITROSYSTEM_CONTROL_VERSION);
    return 0;
}

/* in fh2mod_init: CreateThread(NULL, 0, find_nitro_system, NULL, 0, NULL); */
```

`get_interface` returns `NULL` when Nitro System isn't installed, or is a version without the switch. Check for it and carry on without NOS control.

## What it does

| Function | What it does |
|---|---|
| `set_blocked(who, 1)` | Switches the player's NOS off on behalf of `who`, your mod's name (up to 31 characters). |
| `set_blocked(who, 0)` | Takes your block away again. |
| `is_blocked()` | 1 while any mod blocks NOS. |
| `is_active()` | 1 while the player's NOS is burning. |
| `nitro_percent()` | How full the player's tank is, 0 to 100. |

- **Blocks are per mod.** NOS stays off while at least one mod blocks it, so two mods can't undo each other. Use the same `who` to block and to unblock.
- **While blocked:** the NOS gauge is hidden, the NOS button does nothing, and NOS that was burning stops at once. The tank still recharges. Drivatars aren't affected.
- **Blocks aren't saved.** They're gone when the game closes, so block again after a restart if you still need to.
- **Threads:** every function can be called from any thread, at any time.

`platform.log` shows `NOS blocked by MyMod.` and `NOS no longer blocked by MyMod.`, and Nitro System's debug overlay lists the mods currently blocking NOS.

## Without the platform's interfaces

`FnFNitro.dll` exports the same functions:

```c
typedef void (*SetBlockedFn)(const char* who, int blocked);
HMODULE m = GetModuleHandleW(L"FnFNitro.dll");
SetBlockedFn set_blocked = m ? (SetBlockedFn)GetProcAddress(m, "FnFNitro_SetBlocked") : NULL;
if (set_blocked) set_blocked("MyMod", 1);
```

| Export | Same as |
|---|---|
| `FnFNitro_SetBlocked(const char* who, int blocked)` | `set_blocked` |
| `FnFNitro_IsBlocked()` | `is_blocked` |
| `FnFNitro_IsActive()` | `is_active` |
| `FnFNitro_GetNitroPercent()` | `nitro_percent` |

## ANNA Drone Mode

Drone Mode needs none of this. Nitro System asks Drone Mode whether the drone is flying (its `ANNADroneMode` interface, `is_flying`) and blocks NOS under the name `ANNADroneMode` while it does. `platform.log` says `ANNA Drone Mode found: no NOS while the drone flies.` at startup when it's installed.

## Versions

The interface is published as `NitroSystem.Control`, version 1. Later versions only add functions at the end of the struct, so code written for version 1 keeps working.
