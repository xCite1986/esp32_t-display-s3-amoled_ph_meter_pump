# ota.ps1 - Firmware ueber WLAN uebertragen, ohne USB-Kabel
#   .\scripts\ota.ps1                      -> an ph-dosierung.local
#   .\scripts\ota.ps1 -Target 192.168.0.61
#   .\scripts\ota.ps1 -Password geheim     -> falls ein Web-Passwort gesetzt ist
#
# Warum nicht "arduino-cli upload --protocol network"?
# Das ruft espota ohne festen lokalen Port auf. espota arbeitet so, dass sich
# das Board zum PC ZURUECK verbindet - diese eingehende Verbindung blockt die
# Windows-Firewall, wenn fuer das jeweilige espota.exe keine Freigabe besteht.
# Die Freigaben haengen am Pfad und damit an der Core-Version: nach einem
# Core-Update fehlen sie wieder. Ein fester Port macht das reproduzierbar.
param(
    [string]$Target   = "ph-dosierung.local",
    [int]$DevicePort  = 3232,
    [int]$LocalPort   = 3233,
    [string]$Password = ""
)

. "$PSScriptRoot\acli.ps1"

$sketch = Join-Path $Root "firmware\ph_dosieranlage_s3"
$outDir = Join-Path $env:TEMP "ph_ota_build"

$cli = Get-ArduinoCli
Write-Host "Compiliere $sketch" -ForegroundColor Cyan
& $cli compile --jobs 1 --fqbn $Fqbn --output-dir $outDir $sketch
if ($LASTEXITCODE -ne 0) { throw "Compile fehlgeschlagen" }

$bin = Join-Path $outDir "ph_dosieranlage_s3.ino.bin"
if (-not (Test-Path $bin)) { throw "Firmware-Datei nicht gefunden: $bin" }

# espota liegt beim ESP32-Core; die Version kann sich aendern
$core = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\hardware\esp32" -Directory -EA SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
if (-not $core) { throw "ESP32-Core nicht gefunden" }
$espota = Join-Path $core.FullName "tools\espota.exe"
if (-not (Test-Path $espota)) { throw "espota.exe nicht gefunden: $espota" }

$args = @("-r","-i",$Target,"-p",$DevicePort,"-P",$LocalPort,"-f",$bin)
if ($Password) { $args += @("-a",$Password) }

Write-Host "Uebertrage per OTA an $Target" -ForegroundColor Cyan
Write-Host "Falls Windows nach einer Firewall-Freigabe fragt: zulassen." -ForegroundColor Yellow

# espota schreibt seinen Fortschritt auf stderr. Mit ErrorActionPreference
# "Stop" aus acli.ps1 wuerde PowerShell das als Abbruch werten, obwohl die
# Uebertragung laeuft - also hier gezielt aussetzen und nur den Exitcode
# auswerten.
$prev = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $espota @args 2>&1 | ForEach-Object { Write-Host $_ }
$rc = $LASTEXITCODE
$ErrorActionPreference = $prev

if ($rc -ne 0) {
    Write-Host "" -ForegroundColor Yellow
    Write-Host "Bleibt es bei 'No response from device', blockt die Firewall den" -ForegroundColor Yellow
    Write-Host "Rueckkanal. Freigabe fuer diese Datei setzen:" -ForegroundColor Yellow
    Write-Host "  $espota" -ForegroundColor Yellow
    throw "OTA fehlgeschlagen"
}

Write-Host "Fertig. Das Geraet startet neu." -ForegroundColor Green
Write-Host "Hinweis: Vor dem Update wird ein Not-Halt ausgeloest - die Pumpe" -ForegroundColor Green
Write-Host "steht also waehrend der Uebertragung garantiert still." -ForegroundColor Green
