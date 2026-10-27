# Builds the release zip from an already-built tree.
#   scripts\package-release.ps1 -Tag v1.0.0
# The zip mirrors where the files go, so a player copies one folder:
#   azimuth-windows\*  ->  their Azimuth folder
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
$platformDll = "dist\d3dcompiler_47.dll"   # the platform's official release build; it isn't built here

# Everything the zip needs must exist, or the release fails loudly instead of shipping a broken zip
$required = @($dll, $platformDll, "README.md", "platform\LICENSE", "platform\third_party\minhook\LICENSE.txt")
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

# --- Azimuth folder: the mod and the platform it runs on
$az = Join-Path $stage "azimuth-windows"
New-Item -ItemType Directory -Force -Path (Join-Path $az "fh2-mods") | Out-Null
Copy-Item $dll         (Join-Path $az "fh2-mods\FnFNitro.dll")
Copy-Item $platformDll (Join-Path $az "d3dcompiler_47.dll")

# The HUD, the flame effect and the database change are all inside FnFNitro.dll; the platform writes them to
# the game's mediaoverride folder itself, so the zip has no Mount folder.

# --- Licences (the platform asks for LICENSE-MinHook.txt to travel with d3dcompiler_47.dll) and docs
$lic = Join-Path $stage "licenses"
New-Item -ItemType Directory -Force -Path $lic | Out-Null
Copy-Item "platform\LICENSE"                          (Join-Path $lic "LICENSE-FH2-Mod-Platform.txt")
Copy-Item "platform\third_party\minhook\LICENSE.txt"  (Join-Path $lic "LICENSE-MinHook.txt")
Copy-Item "README.md" (Join-Path $stage "README.md")

@"
FnFNitro $Tag - Fast & Furious nitro for Forza Horizon 2 (Azimuth)
====================================================================

Close the game first, then copy the azimuth-windows\ folder from this zip into your Azimuth folder
(the one with azimuth.exe), e.g. D:\Games\FH2\azimuth-windows\ . Say yes to merging folders.

What is in azimuth-windows\:
  fh2-mods\FnFNitro.dll    the mod
  d3dcompiler_47.dll       the FH2 Mod Platform that loads mods. If you already have a platform
                           from another mod, keep the newer one: skip this file if yours is newer.

The NOS speedometer gauge, the HUD textures and the blue flame effect are inside FnFNitro.dll. The platform
writes them to the game's mediaoverride folder, which needs Forza Horizon 2 Extra Options to be installed.

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
