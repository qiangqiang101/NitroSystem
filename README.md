# Nitro System for FH2

Fast & Furious style **nitro** for **Forza Horizon 2** (Xbox One build on PC via the Azimuth launcher), built on the **FH2 Mod Platform**.

Hold **LB** (or any button you pick) to burn nitro: your car gets a strong push, blue flames shoot from your tailpipes, the speedometer gets a draining NOS gauge with a glowing tip, and the controller rumbles.

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
- **Sound and rumble.** A looping boost sound and a stop sound (`nos_boost.wav`, `nos_stop.wav`), and a rumble that ramps up with the boost.
- **In-game settings.** Everything below is on **Pause > Settings > MODS**.

### In-game settings

| Setting | Range | Default | What it does |
|---|---|---|---|
| Nitro System | On / Off | On | Turns nitro on or off. |
| Nitro Button | 1 - 16 | 1 (LB) | Which button burns nitro. The numbers are listed under the ini notes below. Applies right away. |
| Nitro Debug Overlay | On / Off | Off | Draws debug info over the game (see below). |
| Nitro Boost Multiplier | 1.0x - 2.0x | 1.6x | Strength of the push. |
| Nitro Tank Capacity (sec) | 5 - 60 | 20 | Seconds of nitro in a full tank. |
| Nitro Buildup Time (sec) | 0.1 - 2.0 | 0.5 | How long the push takes to ramp up. |
| Nitro Recharge Rate | 0.0 - 1.0 | 1.0 | Seconds of nitro restored per second while not boosting. |
| Backfire on All Cars | On / Off | On | Lets every car backfire, which the exhaust flames need (see below). **Needs a game restart.** |

Settings are saved to `fh2-mods\FnFNitro.ini`:

```ini
[Settings]
NitroEnabled=1
NitroButton=LB
TorqueScale=1.60
Duration=20.0
BuildupTime=0.50
RechargeRate=1.00
DebugOverlay=0
BackfireOnAllCars=1
```

A few notes on the file:

- `NitroButton` picks which controller button burns nitro (default `LB`). An unknown value falls back to `LB`. The Mods page can only show numbers, so there the **Nitro Button** row is a slider from 1 to 16; in the ini you write the name:

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
- `BackfireOnAllCars` is the **Backfire on All Cars** row on the Mods page. When it is on, the mod makes every car able to backfire by changing the game database at startup, which the flames depend on. Turn it off to leave the game's own backfire settings alone: flames then only work on cars the game already lets backfire. A change only takes effect the next time the game starts.

### Debug overlay

Switch on **Nitro Debug Overlay** to draw a small panel in the top-left corner of the game window: nitro state and fuel, whether the mod thinks you are driving, whether your car has been found, the learned exhaust tips, and the flame counters. It is a transparent window laid over the game, like the FPS counter, so the game must run in **borderless or windowed** mode.

---

## Requirements

- **Azimuth** launcher with Forza Horizon 2.
- The **FH2 Mod Platform** (`d3dcompiler_47.dll` in the Azimuth folder; tested with platform 2.7.1). The other mods you may already have, such as ShowFPS or Achievements, use the same one.
- **Forza Horizon 2 Extra Options.** The NOS speedometer gauge, the blue flame effect and the "every car can backfire" database change are all delivered through its `mediaoverride` folder. Without it the nitro push, sound and rumble still work, but those three won't apply.

---

## Installation

The paths below are examples. Use your own Azimuth folder and the game's `Mount` folder.

### From a release (easiest)

1. Download `FnFNitro-<version>.zip` from the **Releases** page and unzip it.
2. Close the game. Copy the zip's **`azimuth-windows\`** folder into your Azimuth folder (the one with `azimuth.exe`), and its **`Mount\`** folder into the game's `Mount` folder. Say yes to merging.
3. Start the game through Azimuth.

The zip contains the mod (`FnFNitro.dll`), the nitro sounds, the NOS speedometer gauge with its textures, the blue flame effect, the FH2 Mod Platform (`d3dcompiler_47.dll`, which loads mods; if you already have a newer one from another mod, keep yours), the licences, and an `INSTALL.txt` with these same steps.

### From the source tree

The `dist\` folder in this repository mirrors where each file goes:

| From | To |
|---|---|
| `dist\fh2-mods\FnFNitro.dll` | `D:\Games\FH2\azimuth-windows\fh2-mods\` |
| `dist\FnFNitro\` (the two `.wav` files) | `D:\Games\FH2\azimuth-windows\FnFNitro\` |
| `dist\mediaoverride\` (everything inside) | `D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount\mediaoverride\` |
| `dist\d3dcompiler_47.dll` | `D:\Games\FH2\azimuth-windows\` (only if the platform isn't installed yet) |

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
powershell -ExecutionPolicy Bypass -File scripts\package.ps1 -AzimuthDir "D:\Games\FH2\azimuth-windows" -GameMountDir "D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount"
```

### Local deploy script

`package.ps1` builds, fills `dist\`, and deploys the DLL, the sounds and the `mediaoverride` files. Both path parameters default to the author's folders (`D:\Games\FH2\...`), so pass your own unless yours match. If the game is running it can't overwrite the DLL, so close the game and run the script again.

---

## Uninstalling

Close the game first. Everything the mod added is in three places, and **nothing else should be removed**: the Mods page screens, string tables, car zips and `DefaultCache.bin` in `mediaoverride` belong to the FH2 Mod Platform and your other mods, and `d3dcompiler_47.dll` is the platform itself.

**To switch the mod off without uninstalling it,** open **Pause > Settings > MODS** and turn **Nitro System** off (or set `NitroEnabled=0` in the ini).

### 1. The mod and its files (Azimuth folder)

Delete these from `azimuth-windows`:

- `fh2-mods\FnFNitro.dll`
- `fh2-mods\FnFNitro.ini` (your saved settings)
- the whole `FnFNitro\` folder (the sounds)

### 2. The HUD, flame and database overrides (game folder)

Delete these from `Mount\mediaoverride`:

- `UI\Controls\HUDSpeedometerControl.xaml` (the NOS speedometer gauge)
- `UI\textures\Anthem\HUD\HUD_XPBar_TipGlow.swatchbin`, `SkillBarTipGlow.swatchbin` and `SkillsLargeGlow.swatchbin`
- the folder `UI\textures\Anthem\HUD\Speedometer\`
- the folder `UI\textures\data_bound\SpireHUD\` and the file `UI\textures\data_bound\SpireHUD.zip`
- the folder `Particles\` (the blue flame effect)
- `stripped\gamedbRC.slt` (the "every car can backfire" database copy; the platform makes a new one on the next start only if a mod still needs it)

### 3. Or do it all with PowerShell

Edit the first two lines to match your install. Add `-WhatIf` to each `Remove-Item` first to see what would be deleted without deleting anything.

```powershell
$Azimuth = "D:\Games\FH2\azimuth-windows"
$Mount   = "D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount"
$mo      = Join-Path $Mount "mediaoverride"

$targets = @(
  "$Azimuth\fh2-mods\FnFNitro.dll",
  "$Azimuth\fh2-mods\FnFNitro.ini",
  "$Azimuth\FnFNitro",
  "$mo\UI\Controls\HUDSpeedometerControl.xaml",
  "$mo\UI\textures\Anthem\HUD\HUD_XPBar_TipGlow.swatchbin",
  "$mo\UI\textures\Anthem\HUD\SkillBarTipGlow.swatchbin",
  "$mo\UI\textures\Anthem\HUD\SkillsLargeGlow.swatchbin",
  "$mo\UI\textures\Anthem\HUD\Speedometer",
  "$mo\UI\textures\data_bound\SpireHUD",
  "$mo\UI\textures\data_bound\SpireHUD.zip",
  "$mo\Particles",
  "$mo\stripped\gamedbRC.slt"
)
foreach ($t in $targets) {
  if (Test-Path $t) { Remove-Item $t -Recurse -Force } else { "Not found (already gone): $t" }
}
```

### 4. Start the game once

Launch the game through Azimuth. The platform tidies up the files it manages, and the speedometer, flames and car backfire behaviour are back to stock. Open `fh2-mods\platform.log`: there should be no `FnFNitro` lines.

---

## Known limitations

- **Flames at very high speed.** Above about 250 km/h the flames can look thin or vanish when the camera is directly behind the car, and look right from the side. This is the game's particle effect stretching with speed, not something the mod can change.
- **Flames need a first backfire.** After you start driving or switch cars, there are no nitro flames until the game has fired one of its own backfires for that car.
- **No motion blur.** An attempt to add nitro motion blur didn't work with this game build and was removed.
- **No nitro package in the upgrade shop.** The game's upgrade system has no slot for it.

---

## Troubleshooting

Everything the mod says goes to `fh2-mods\platform.log` (starts fresh every launch). Useful lines:

- `loaded FnFNitro.dll`, plus a line for each hook it installed: the mod is running.
- `Player car located` and `Vertical axis learned: Y`: the mod found your car and which way is up. This takes a few seconds of driving after the game loads. Until it has found your car, the mod can't tell your car from AI cars and may push every simulated car, so give it a few seconds before you use nitro next to other cars.
- `Failed to ...` or `Could not hook ...`: a hook didn't install, usually because the game build isn't the one this mod was written for.

If there is no NOS gauge or no flames, check that **Extra Options** is installed and running, and that the `mediaoverride` files from the table above are in the game's `Mount\mediaoverride` folder.
