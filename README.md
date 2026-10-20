# FnFNitroInFH2

Fast & Furious Nitro system ported to **Forza Horizon 2** (Xbox One on PC via Azimuth Launcher) using the **FH2 Mod Platform**.

---

## Features

- **Nitro State Machine**: Implements continuous boost with duration tank, buildup curve, and automatic recharge.
- **Tuned to Match F&F (Spire)**:
  - `TorqueScale`: Default `1.6x` engine torque boost (configurable).
  - `Duration`: Default `20.0s` capacity (configurable).
  - `BuildupTime`: Default `0.5s` linear ramp (configurable).
- **In-Game Settings**: Integrated directly into **Pause > Settings > MODS**:
  - **Nitro System** (Toggle On/Off)
  - **Nitro Torque Multiplier** (Slider: 1.0x – 3.0x)
  - **Nitro Tank Capacity** (Slider: 5s – 60s)
  - **Nitro Buildup Time** (Slider: 0.1s – 2.0s)
  - **Nitro Recharge Rate** (Slider: 0.0x – 5.0x)
- **Controller Support**: Default activation via **(A) Button** while accelerating (`RT`).

---

## Building from Source

Requires **Visual Studio 2022** (MSVC C++20) and **CMake 3.20+**.

```powershell
# Configure
cmake -B build -A x64

# Build Release
cmake --build build --config Release

# Package and copy to Azimuth folder
powershell -ExecutionPolicy Bypass -File scripts\package.ps1
```

---

## Installation

1. Copy `FnFNitro.dll` into your Azimuth launcher's `fh2-mods\` folder:
   ```
   D:\Games\FH2\azimuth-windows\fh2-mods\FnFNitro.dll
   ```
2. Ensure `d3dcompiler_47.dll` from the platform is in your `azimuth-windows\` root folder.
3. Launch Forza Horizon 2 via Azimuth. Check `fh2-mods\platform.log` to confirm `FnFNitro` has loaded.
