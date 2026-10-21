# Builds the release zip from an already-built tree.
#   scripts\package-release.ps1 -Tag v1.0.0
# The zip mirrors where the files go, so a player copies two folders:
#   azimuth-windows\*  ->  their Azimuth folder
#   Mount\*            ->  the game's Mount folder
# Run it after:  cmake -B build -A x64 ; cmake --build build --config Release
param(
    [Parameter(Mandatory = $true)] [string]$Tag,
    [string]$BuildDir = "build",
    [string]$OutDir = "release"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $root

$dll         = Join-Path $BuildDir "Release\FnFNitro.dll"
$platformDll = Join-Path $BuildDir "platform\Release\d3dcompiler_47.dll"
$sounds      = @("dist\FnFNitro\nos_boost.wav", "dist\FnFNitro\nos_stop.wav")
$override    = "dist\mediaoverride"

# Everything the zip needs must exist, or the release fails loudly instead of shipping a broken zip
$required = @($dll, $platformDll, $override, "README.md", "platform\LICENSE", "platform\third_party\minhook\LICENSE.txt") + $sounds
foreach ($f in $required) {
    if (-not (Test-Path $f)) { throw "Missing required file: $f" }
}

$safeTag = ($Tag -replace '[^A-Za-z0-9._-]', '-')
$name    = "FnFNitro-$safeTag"
$stage   = Join-Path $OutDir $name
$zip     = Join-Path $OutDir "$name.zip"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip)   { Remove-Item $zip -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

# --- Azimuth folder: the mod, its sounds, and the platform it runs on
$az = Join-Path $stage "azimuth-windows"
New-Item -ItemType Directory -Force -Path (Join-Path $az "fh2-mods"), (Join-Path $az "FnFNitro") | Out-Null
Copy-Item $dll         (Join-Path $az "fh2-mods\FnFNitro.dll")
Copy-Item $sounds      (Join-Path $az "FnFNitro")
Copy-Item $platformDll (Join-Path $az "d3dcompiler_47.dll")

# --- Game folder: HUD gauge, textures, flame particles. The database copy is left out on purpose:
# it is a copy of the game's own database, and the platform rebuilds it at runtime from the mod's SQL.
$mo = Join-Path $stage "Mount\mediaoverride"
New-Item -ItemType Directory -Force -Path $mo | Out-Null
$overrideRoot = (Resolve-Path $override).Path
Get-ChildItem $overrideRoot -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($overrideRoot.Length).TrimStart('\')
    if ($rel -ieq "stripped\gamedbRC.slt") { return }
    $dest = Join-Path $mo $rel
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item $_.FullName $dest
}

# --- Licences (the platform asks for LICENSE-MinHook.txt to travel with d3dcompiler_47.dll) and docs
$lic = Join-Path $stage "licenses"
New-Item -ItemType Directory -Force -Path $lic | Out-Null
Copy-Item "platform\LICENSE"                          (Join-Path $lic "LICENSE-FH2-Mod-Platform.txt")
Copy-Item "platform\third_party\minhook\LICENSE.txt"  (Join-Path $lic "LICENSE-MinHook.txt")
Copy-Item "README.md" (Join-Path $stage "README.md")

@"
FnFNitro $Tag - Fast & Furious nitro for Forza Horizon 2 (Azimuth)
====================================================================

Close the game first, then copy two folders from this zip:

  1. azimuth-windows\   ->  into your Azimuth folder (the one with azimuth.exe)
                            e.g. D:\Games\FH2\azimuth-windows\
                            Say yes to merging folders.

  2. Mount\             ->  into the game's Mount folder, next to forza_durango_release_final.exe
                            e.g. D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount\
                            Say yes to merging folders.

What is in azimuth-windows\:
  fh2-mods\FnFNitro.dll    the mod
  FnFNitro\                the nitro sounds
  d3dcompiler_47.dll       the FH2 Mod Platform that loads mods. If you already have a platform
                           from another mod, keep the newer one: skip this file if yours is newer.

What is in Mount\mediaoverride\: the NOS speedometer gauge, its textures and the blue flame effect.
These need Forza Horizon 2 Extra Options to be installed.

Start the game through Azimuth. Hold LB while driving. Settings: Pause > Settings > MODS.
fh2-mods\platform.log should show "loaded FnFNitro.dll".

To uninstall, see README.md (the "Uninstalling" section).
licenses\ holds the licences for the platform and for MinHook, which it contains.
"@ | Set-Content -Path (Join-Path $stage "INSTALL.txt") -Encoding UTF8

# --- Zip it (forward-slash entries, one top folder), then show what went in
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    (Resolve-Path $stage).Path,
    (Join-Path (Resolve-Path $OutDir).Path "$name.zip"),
    [System.IO.Compression.CompressionLevel]::Optimal,
    $true)

$z = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $zip).Path)
try {
    Write-Host "Contents of $zip :"
    foreach ($e in $z.Entries) { if ($e.Name) { Write-Host ("  {0}  ({1:N0} bytes)" -f $e.FullName, $e.Length) } }
} finally { $z.Dispose() }
Write-Host ("Created {0} ({1:N0} bytes)" -f $zip, (Get-Item $zip).Length)
