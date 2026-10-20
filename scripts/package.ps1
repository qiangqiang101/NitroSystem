param(
    [string]$AzimuthDir = "D:\Games\FH2\azimuth-windows",
    [string]$GameMountDir = "D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount"
)

$ErrorActionPreference = "Stop"

Write-Host "Building FnFNitro (Release)..." -ForegroundColor Cyan
cmake --build build --config Release

$dllSource = "build\Release\FnFNitro.dll"
$platformSource = "build\platform\Release\d3dcompiler_47.dll"
$soundSource = "dist\FnFNitro\nos_boost.wav"

if (!(Test-Path $dllSource)) {
    Write-Error "Build failed or $dllSource not found!"
    exit 1
}

# 1. Package into dist\
$distDir = "dist"
$distModsDir = Join-Path $distDir "fh2-mods"
$distDataDir = Join-Path $distDir "FnFNitro"
New-Item -ItemType Directory -Force -Path $distModsDir | Out-Null
New-Item -ItemType Directory -Force -Path $distDataDir | Out-Null

Copy-Item $platformSource -Destination $distDir -Force
Copy-Item $dllSource -Destination $distModsDir -Force

Write-Host "Distributable packaged in $distDir\" -ForegroundColor Green

# 2. Deploy to active Azimuth installation
if (Test-Path $AzimuthDir) {
    # Deploy DLL
    $targetModsDir = Join-Path $AzimuthDir "fh2-mods"
    if (Test-Path $targetModsDir) {
        $targetFile = Join-Path $targetModsDir "FnFNitro.dll"
        try {
            Copy-Item $dllSource -Destination $targetFile -Force
            Write-Host "Successfully installed FnFNitro.dll into $targetModsDir" -ForegroundColor Green
        } catch {
            Write-Warning "Could not overwrite $targetFile because Azimuth / Forza Horizon 2 is currently running. Please close the game, then re-run scripts\package.ps1 to copy the latest build."
        }
    }

    # Deploy Sounds
    $targetDataDir = Join-Path $AzimuthDir "FnFNitro"
    New-Item -ItemType Directory -Force -Path $targetDataDir | Out-Null
    Copy-Item "dist\FnFNitro\*" -Destination $targetDataDir -Force
    Write-Host "Deployed NOS audio (boost + stop sounds) into $targetDataDir" -ForegroundColor Green
}

# 3. Deploy mediaoverride assets to Game Mount
if (Test-Path $GameMountDir) {
    $srcOverride = Join-Path $distDir "mediaoverride"
    $dstOverride = Join-Path $GameMountDir "mediaoverride"
    if (Test-Path $srcOverride) {
        Copy-Item -Path "$srcOverride\*" -Destination $dstOverride -Recurse -Force
        Write-Host "Deployed HUD gauge and blue flame overrides into $dstOverride" -ForegroundColor Green
    }
}
