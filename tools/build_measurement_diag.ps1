<#
.SYNOPSIS
  Build the motors-off measurement dashboard firmware on Windows, using the Pico SDK
  that the VS Code Pico extension installs under %USERPROFILE%\.pico-sdk.

.PARAMETER Motion
  Build the full car dashboard firmware (motion + the Measurements tab) instead of the
  motors-off measurement-only firmware. Same as build_wireless_calibration.sh on Windows.

.PARAMETER Station
  Make the car join an existing Wi-Fi network (the one your computer is on) instead of
  hosting AAI10-Calibrate. Needs station_settings.local.h; falls back to the access point
  if the join fails.

.PARAMETER SettingsDir
  Folder holding station_settings.local.h. Default: firmware/app/robot_firmware/wireless.

.PARAMETER SdkVersion
  Pico SDK version folder under .pico-sdk\sdk. Default 2.3.1.

.PARAMETER SkipEmbed
  Do not regenerate the embedded dashboard pages first.

.EXAMPLE
  .\tools\build_measurement_diag.ps1            # access-point build (AAI10-Calibrate)
  .\tools\build_measurement_diag.ps1 -Station   # join your own Wi-Fi
#>
param(
    [switch]$Motion,
    [switch]$Station,
    [string]$SettingsDir,
    [string]$SdkVersion = '2.3.1',
    [switch]$SkipEmbed
)
# Native tools (cmake, python) write progress to stderr; under 'Stop' Windows PowerShell 5.1
# turns that into a fatal error. Native failures are caught through exit codes (Check) and
# PowerShell cmdlets that matter carry an explicit -ErrorAction Stop.
$ErrorActionPreference = 'Continue'

function Latest([string]$folder, [string]$filter) {
    $item = Get-ChildItem -LiteralPath $folder -Directory -Filter $filter -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $item) { throw "Nothing matching '$filter' in $folder. Install the Pico toolchain with the VS Code Pico extension." }
    return $item.FullName
}
function Check([string]$what) {
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$pico = Join-Path $env:USERPROFILE '.pico-sdk'
$sdk = Join-Path $pico "sdk\$SdkVersion"
if (-not (Test-Path $sdk)) { throw "Pico SDK $SdkVersion not found at $sdk" }
$toolchain = Latest (Join-Path $pico 'toolchain') '*'
$cmakeDir = Join-Path (Latest (Join-Path $pico 'cmake') 'v*') 'bin'
$ninjaDir = Latest (Join-Path $pico 'ninja') 'v*'
$picotoolDir = Join-Path $pico "picotool\$SdkVersion"
$pioasmDir = Join-Path $pico "tools\$SdkVersion\pioasm"
if (-not (Test-Path (Join-Path $pioasmDir 'pioasm.exe'))) { throw "Prebuilt pioasm not found in $pioasmDir" }

$env:PICO_SDK_PATH = $sdk
$env:PICO_TOOLCHAIN_PATH = $toolchain
$env:PATH = "$cmakeDir;$ninjaDir;$(Join-Path $toolchain 'bin');$picotoolDir;$env:PATH"

if (-not $SkipEmbed) {
    # -X utf8 is required: without it Windows decodes the page as cp1252 and corrupts it.
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) { & py -3 -I -X utf8 (Join-Path $repo 'tools\embed_calibration_page.py') }
    else { throw "Python launcher 'py' not found. Install Python 3, or pass -SkipEmbed if the pages are unchanged." }
    Check 'embedding the dashboard pages'
}

$base = if ($Motion) { 'motion-wireless' } else { 'measurement-diag' }
$name = if ($Station) { "$base-station" } else { $base }
$out = Join-Path $repo "build\$name"
if (Test-Path $out) { Remove-Item $out -Recurse -Force -ErrorAction Stop }

$args = @(
    '-S', (Join-Path $repo 'firmware\app\robot_firmware'), '-B', $out, '-G', 'Ninja',
    '-DPICO_BOARD=pico_w', "-Dpioasm_DIR=$($pioasmDir -replace '\\','/')",
    '-DROBOCAR_WIRELESS_CALIBRATION=ON',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_FLAGS=-mcpu=cortex-m0plus -mthumb -Wall -Wextra -Werror'
)
if ($Motion) { $args += '-DROBOCAR_CENTER_LINE=ON' } else { $args += '-DROBOCAR_MEASUREMENT_DIAG=ON' }
if ($Station) {
    $args += '-DROBOCAR_WIFI_STATION=ON'
    if ($SettingsDir) { $args += "-DROBOCAR_STATION_SETTINGS_DIR=$((Resolve-Path $SettingsDir).Path -replace '\\','/')" }
}
& cmake @args
Check 'CMake configure'
& cmake --build $out --parallel 8
Check 'build'

$suffix = if ($Station) { '_station' } else { '' }
$stem = if ($Motion) { 'robocar_motion_dashboard' } else { 'robocar_measurement_v1' }
$uf2 = Join-Path $out "$stem$suffix.uf2"
Copy-Item (Join-Path $out 'robot_firmware.uf2') $uf2 -ErrorAction Stop
Write-Host ""
Write-Host "Built: $uf2"
if ($Station) {
    Write-Host "WARNING: this image contains your Wi-Fi password. Do not commit, publish or share it." -ForegroundColor Yellow
    Write-Host "After flashing, open the serial monitor: the car prints 'open http://<address>/'."
}
