# Nitro System for FH2

Fast & Furious style **nitro** for **Forza Horizon 2** (Xbox One build on PC via the Azimuth launcher), built on the **FH2 Mod Platform**.

Hold **LB** (or any button you pick) to burn nitro: your car gets a strong push, blue flames shoot from your tailpipes, the speedometer gets a draining NOS gauge with a glowing tip, the nitro roars, the camera shakes and the controller rumbles.

[<img width="1536" height="864" alt="image" src="https://github.com/user-attachments/assets/ce6448ca-6403-46ca-b6ec-baeee1115d85" />](https://www.youtube.com/watch?v=T0PnPXlaUCg)


---

## Features

- **Nitro on LB, or any button you choose.** Hold the left shoulder button on any connected controller, or remap it to another button, d-pad direction or trigger (see **Nitro Button** below). There is no keyboard key.
- **A real tank.** Nitro drains while you hold the nitro button and recharges when you let go. Run it dry and it locks out until you release the button and have recharged about 2 seconds.
- **Buildup curve.** The push ramps up over a short time instead of snapping on.
- **Only your car.** AI racers and traffic are never boosted.
- **Stays on the road.** The push follows the horizontal direction of the car's nose, so a pitched-up nose doesn't launch you into the sky, and nothing is applied while the car is airborne (or for a moment after landing).
- **Only while driving.** Nitro only works while the game is simulating your car and the speedometer HUD is running, so it stays off in the pause menu, the map and loading screens. The tank doesn't drain or recharge while the game is paused. (Garage and car meet lobbies should also stay off, since the speedometer isn't active there.)
- **NOS gauge on the speedometer.** Both the **analog** and the **digital** speedometers get a four-segment NOS bar that drains as you use nitro, with a glowing tip that follows the leading edge of the fill.
- **Exhaust flames.** Blue flames come out of your car's real tailpipes while nitro is held (the mod drives the game's own backfire effect). Which tailpipes your car has is learned from the game's own backfires, so flames start after the first backfire of a drive or after you switch cars, which normally takes a few seconds.
- **Sound, rumble and camera shake.** A boost sound that kicks in and then holds for as long as the tank lasts, and a stop sound (built into the DLL; put your own `nos_boost.wav` and `nos_stop.wav` in an `FnFNitro` folder next to `azimuth.exe` to replace them; your own boost sound loops from its start), a rumble that ramps up with the boost, and a camera shake while it burns. Each has its own setting below.
- **In-game settings.** Everything below is on **Pause > Settings > MODS**.

### In-game settings

| Setting | Range | Default | What it does |
|---|---|---|---|
| Nitro System | On / Off | On | Turns nitro on or off. |
| Nitro Button | LB, RB, A, B, X, Y, LS Click, RS Click, D-Pad, Back, Start, LT, RT | LB | Which button burns nitro. Applies right away. |
| Nitro Debug Overlay | On / Off | Off | Draws debug info over the game (see below). |
| HUD Theme | Original / Fast & Furious | Fast & Furious | Original is the Forza Horizon 2 HUD with a yellow NOS bar. Fast & Furious has a cyan NOS bar, cyan gears and cyan ABS / TCS / STM icons. **Needs a game restart.** |
| Nitro Boost Multiplier | 1.0x - 2.0x | 1.6x | Strength of the push. |
| Nitro Tank Capacity (sec) | 5 - 60 | 20 | Seconds of nitro in a full tank. |
| Nitro Buildup Time (sec) | 0.1 - 2.0 | 0.5 | How long the push takes to ramp up. |
| Nitro Recharge Rate | 0.0 - 1.0 | 0.2 | Seconds of nitro restored per second while not boosting. |
| Nitro Sound Volume | 0 - 100 | 50 | Volume of the boost and stop sounds. The game's own volume sliders don't change them. Applies from the next boost. |
| Nitro Camera Shake | 0 - 100 | 65 | How much the camera shakes while nitro burns, in the driver, hood, bumper and chase views. 0 turns it off. Applies right away. |
| Backfire on All Cars | On / Off | On | Lets every car backfire, which the exhaust flames need (see below). **Needs a game restart.** |
| Refill Nitro on Race Start | On / Off | On | Fills the tank when a race starts, you quit one, you change car or you fast travel. Applies right away. |
| Refund Nitro on Rewind | On / Off | On | Rewinding gives back the nitro you used since the point you rewind to. Applies right away. |
| Nitro Controller Rumble | On / Off | On | Rumbles the controller while nitro burns. The game's own rumble isn't affected. Applies right away. |
| Drivatar Nitro | On / Off | Off | Drivatars use nitro too, whenever they can, at any speed: they let off only in the air, in a big slide or when the tank runs dry (then, like you, they wait for 2 s of nitro). They don't boost before the race starts: not while your HUD is down, and only once they've been rolling for 2 s off the grid. Once they're more than 100 m ahead of you they coast, so you can catch up; behind you they boost freely. Each has a tank of its own with twice your capacity and recharge, and your boost and buildup. Their exhausts flame (after a Drivatar's first backfire, which is how its tailpipes are learned) and the boost plays through the game's own audio at the car, so you hear it from where it is, fading out by 150 m. The nearest 6 are heard, at the Drivatar Nitro Volume. Applies right away. |
| Drivatar Nitro Volume | 0 - 100 | 25 | Volume of the Drivatars' nitro sound, heard from their cars. 0 silences it. Applies right away. |

Settings are saved to `fh2-mods\FnFNitro.ini`:

```ini
[Settings]
NitroEnabled=1
NitroButton=LB
TorqueScale=1.60
Duration=20.0
BuildupTime=0.50
RechargeRate=0.20
SoundVolume=0.50
DrivatarSoundVolume=0.25
CameraShake=0.65
DebugOverlay=0
BackfireOnAllCars=1
RefillOnRaceStart=1
RefundOnRewind=1
DrivatarNitro=0
Rumble=1
HudTheme=FF
```

A few notes on the file:

- `HudTheme` is `FF` (Fast & Furious, the default) or `Original`. The mod writes the HUD files when it starts and when you change the setting, and the game reads them the next time it starts.

- `NitroButton` picks which controller button burns nitro (default `LB`). An unknown value falls back to `LB`. In the ini you write the name:

  | # | Name | Button | # | Name | Button |
  |---|---|---|---|---|---|
  | 1 | `LB` | Left bumper | 9 | `DPadUp` | D-pad up |
  | 2 | `RB` | Right bumper | 10 | `DPadDown` | D-pad down |
  | 3 | `A` | A | 11 | `DPadLeft` | D-pad left |
  | 4 | `B` | B | 12 | `DPadRight` | D-pad right |
  | 5 | `X` | X | 13 | `Back` | Back / View |
  | 6 | `Y` | Y | 14 | `Start` | Start / Menu |
  | 7 | `LS` | Left stick click | 15 | `LT` | Left trigger |
  | 8 | `RS` | Right stick click | 16 | `RT` | Right trigger |

  Pick one the game doesn't already use while driving, or it will do both at once. Changes on the Mods page apply immediately; edits to the file apply the next time the game starts.

  With a platform older than 2.8.0 the Mods page can't show names, so **Nitro Button** is a slider with the numbers above and **HUD Theme** is a slider where 1 is Original and 2 is Fast & Furious.
- `BackfireOnAllCars` is the **Backfire on All Cars** row on the Mods page. When it is on, the mod makes every car able to backfire by changing the game database at startup, which the flames depend on. Turn it off to leave the game's own backfire settings alone: flames then only work on cars the game already lets backfire. A change only takes effect the next time the game starts.

### Debug overlay

Switch on **Nitro Debug Overlay** to draw a small panel in the top-left corner of the game window: nitro state and fuel, whether the mod thinks you are driving, whether your car has been found (by the game's own car kinds, or by position until those are known), how many Drivatars are boosting, the learned exhaust tips, and the flame counters. It is a transparent window laid over the game, like the FPS counter, so the game must run in **borderless or windowed** mode.

---

## Requirements

- **Azimuth** launcher with Forza Horizon 2.
- The **FH2 Mod Platform** (`DWrite.dll` in the Azimuth folder; tested with platform 2.8.0). The other mods you may already have, such as ShowFPS or Achievements, use the same one.
- **Forza Horizon 2 Extra Options.** The NOS speedometer gauge and its HUD textures, the blue flame effect and the "every car can backfire" database change are all delivered through its `mediaoverride` folder. Without it the nitro push, sound, camera shake and rumble still work, but those won't apply.

---

## Installation

The paths below are examples. Use your own Azimuth folder and the game's `Mount` folder.

### From a release (easiest)

1. Download `FnFNitro-<version>.zip` from the **Releases** page and unzip it.
2. Close the game. Copy the zip's **`azimuth-windows\`** folder into your Azimuth folder (the one with `azimuth.exe`). Say yes to merging.
3. Start the game through Azimuth.

The zip contains the mod (`FnFNitro.dll`, with the HUD files, flame effect and nitro sounds inside it), the FH2 Mod Platform (`DWrite.dll`, which loads mods; if you already have a newer one from another mod, keep yours), the licences, and an `INSTALL.txt` with these same steps.

> [!IMPORTANT]
> `FnFNitro.dll` goes in **`fh2-mods`**, the FH2 Mod Platform's folder. It is **not** an Extra Options mod: don't put it in Extra Options' `mods` folder, where it won't be loaded. If `fh2-mods` doesn't exist yet, the zip creates it.

**Upgrading from 1.2 or older:** those versions had you copy the HUD textures and flame files into `Mount\mediaoverride`. They are inside the DLL now, so you don't need to copy `Mount\` any more. The first time the new version starts it deletes the old copies it recognises (only files that match the old release byte for byte, so anything you changed is left alone) and writes `Removed N old HUD/flame file(s)` to `platform.log`. Leftover `.bak` files next to them are the platform's backups of what was there before; you can delete those yourself.

### From the source tree

The `dist\` folder in this repository mirrors where each file goes:

| From | To |
|---|---|
| `dist\fh2-mods\FnFNitro.dll` | `D:\Games\FH2\azimuth-windows\fh2-mods\` |
| `DWrite.dll` from the platform's [releases page](https://github.com/Trinculo54/fh2-mod-platform/releases) | `D:\Games\FH2\azimuth-windows\` (only if the platform isn't installed yet) |

Start the game through Azimuth. Open `fh2-mods\platform.log` and look for lines like `loaded FnFNitro.dll`, and later `Player car located` and `Vertical axis learned`, which mean the mod found your car.

---

## Building from Source

Requires **Visual Studio 2022** (MSVC C++20) and **CMake 3.20+**.

```powershell
# Configure
cmake -B build -A x64

# Build Release
cmake --build build --config Release

# Package into dist\ and copy everything into your Azimuth and game folders
powershell -ExecutionPolicy Bypass -File scripts\package.ps1 -AzimuthDir "D:\Games\FH2\azimuth-windows"
```

The platform DLL is not built with MSVC: the platform needs llvm-mingw. The release workflow cross-compiles it from the `platform\` submodule and ships it as `DWrite.dll` (the name Azimuth 1.0.1 and later load). To update it, move the submodule to the new tag.

### Local deploy script

`package.ps1` builds, fills `dist\`, and deploys the DLL. The `-AzimuthDir` parameter defaults to the author's folder (`D:\Games\FH2\azimuth-windows`), so pass your own unless yours matches. If the game is running it can't overwrite the DLL, so close the game and run the script again.

---

## Uninstalling

Close the game first. **Don't remove anything else** from `mediaoverride`: the Mods page screens, string tables, car zips and `DefaultCache.bin` there belong to the FH2 Mod Platform and your other mods, and `DWrite.dll` is the platform itself.

**To switch the mod off without uninstalling it,** open **Pause > Settings > MODS** and turn **Nitro System** off (or set `NitroEnabled=0` in the ini).

### 1. Delete the mod (Azimuth folder)

Delete these from `azimuth-windows`:

- `fh2-mods\FnFNitro.dll`
- `fh2-mods\FnFNitro.ini` (your saved settings)
- the `FnFNitro\` folder, if you made one to use your own sounds

### 2. Start the game once

Launch the game through Azimuth. The mod's HUD, flame and database files in `Mount\mediaoverride` are handed out by the FH2 Mod Platform, and it deletes them shortly after startup once no installed mod uses them: the NOS speedometer (`UI\Controls\HUDSpeedometerControl.xaml` and the textures under `UI\textures\Anthem\HUD\Speedometer\`, `UI\textures\data_bound\Anthem\HUD\` and `UI\textures\data_bound\SpireHUD\`), the flame effect (`Particles\Systems\Backfire.xml`) and the "every car can backfire" database copy (`stripped\gamedbRC.slt`, which is only made again if another mod still changes the database). The speedometer, flames and car backfires are then back to stock. Open `fh2-mods\platform.log`: there should be no `[NitroSystem]` lines.

### 3. Only if you installed 1.2 or older by hand

Those versions had you copy files into `Mount\mediaoverride` yourself. 1.3 and later delete the copies they recognise when they start, so this only matters if you never ran a newer version. Delete these if they're still there (add `-WhatIf` to see what would go without deleting anything):

```powershell
$mo = "D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount\mediaoverride"

$targets = @(
  "$mo\UI\textures\Anthem\HUD\HUD_XPBar_TipGlow.swatchbin",
  "$mo\UI\textures\Anthem\HUD\SkillBarTipGlow.swatchbin",
  "$mo\UI\textures\Anthem\HUD\SkillsLargeGlow.swatchbin",
  "$mo\UI\textures\Anthem\HUD\Speedometer",
  "$mo\UI\textures\data_bound\SpireHUD",
  "$mo\UI\textures\data_bound\SpireHUD.zip",
  "$mo\Particles\Materials\backfire.materialbin",
  "$mo\Particles\Materials\backfire_blue.materialbin",
  "$mo\Particles\Materials\backfire_purple.materialbin",
  "$mo\Particles\Materials\Fire\backfire_blue.materialbin",
  "$mo\Particles\Materials\Fire\carBackfire.materialbin",
  "$mo\Particles\Shaders\backfire"
)
foreach ($t in $targets) {
  if (Test-Path $t) { Remove-Item $t -Recurse -Force } else { "Not found (already gone): $t" }
}
```

---

## Known limitations

- **Flames need a first backfire.** After you start driving or switch cars, there are no nitro flames until the game has fired one of its own backfires for that car.
- **No nitro package in the upgrade shop.** The game's upgrade system has no slot for it.

---

## Troubleshooting

Everything the mod says goes to `fh2-mods\platform.log` (starts fresh every launch). Useful lines:

- `loaded FnFNitro.dll`, then `[NitroSystem]` lines for each hook it installed: the mod is running.
- `Player car located` and `Vertical axis learned: Y`: the mod found your car and which way is up. This takes a few seconds of driving after the game loads. Until it has found your car, the mod can't tell your car from AI cars and may push every simulated car, so give it a few seconds before you use nitro next to other cars.
- `Camera view matrices hooked`: the camera shake is in place.
- `Failed to ...` or `Could not hook ...`: a hook didn't install, usually because the game build isn't the one this mod was written for.

If there is no NOS gauge or no flames, check that **Extra Options** is installed and running, and that `platform.log` has `[NitroSystem] replaces media\...` lines for the HUD files and `Backfire.xml`. If it says the mod couldn't replace a file, another mod is already replacing it.

---

## Credits

- **[suderek22](https://github.com/suderek22)**, for [FH2_HonkHonk](https://github.com/suderek22/FH2_HonkHonk). It showed how to play sounds through the game's own FMOD audio engine at a position in the world, which is how the Drivatars' nitro sound is played from their cars. No HonkHonk code is used here: this mod calls the same FMOD functions with its own code.
- **Deuquan**, for FH2-CameraMods. It showed which camera functions build the driver, hood, bumper and chase cameras' view, which is where the nitro camera shake is applied.
