param(
    [string]$AzimuthDir = "D:\Games\FH2\azimuth-windows"
)

$ErrorActionPreference = "Stop"

Write-Host "Building FnFNitro (Release)..." -ForegroundColor Cyan
cmake --build build --config Release --target FnFNitro

$dllSource = "build\Release\FnFNitro.dll"

if (!(Test-Path $dllSource)) {
    Write-Error "Build failed or $dllSource not found!"
    exit 1
}

# 1. Package into dist\
$distDir = "dist"
$distModsDir = Join-Path $distDir "fh2-mods"
New-Item -ItemType Directory -Force -Path $distModsDir | Out-Null

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
}
