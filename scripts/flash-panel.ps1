# flash-panel.ps1 - Bedienpanel (T-Display S3 AMOLED) compilieren und flashen
#   .\scripts\flash-panel.ps1              -> COM6
#   .\scripts\flash-panel.ps1 -Port COM7
#   .\scripts\flash-panel.ps1 -BuildOnly
param(
    [string]$Port = "COM6",
    [switch]$BuildOnly
)

$TargetPort = $Port          # vor dem Dot-Sourcing sichern, sonst
                             # ueberschreibt acli.ps1 den Parameter
. "$PSScriptRoot\acli.ps1"

$PanelFqbn = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB"
$path = Join-Path $Root "firmware\ph_panel_s3amoled"

$cli = Get-ArduinoCli
Write-Host "Compiliere $path (dauert beim ersten Mal ~20 min wegen LVGL)" -ForegroundColor Cyan
# --jobs 1 ist Absicht: die parallele Bibliothekserkennung von arduino-cli
# bleibt bei diesem Bibliothekssatz reproduzierbar haengen (siehe docs/BEDIENPANEL.md).
& $cli compile --jobs 1 --fqbn $PanelFqbn $path
if ($LASTEXITCODE -ne 0) { throw "Compile fehlgeschlagen" }

if ($BuildOnly) { Write-Host "OK (nur Build)" -ForegroundColor Green; return }

Write-Host "Uebertrage auf $TargetPort" -ForegroundColor Cyan
& $cli upload -p $TargetPort --fqbn $PanelFqbn $path
if ($LASTEXITCODE -ne 0) {
    Write-Host "Upload fehlgeschlagen. Seriellen Monitor schliessen." -ForegroundColor Yellow
    Write-Host "Notfalls Bootloader erzwingen: BOOT halten, RESET tippen, BOOT loslassen." -ForegroundColor Yellow
    throw "Upload fehlgeschlagen"
}
Write-Host "Fertig. Monitor: .\scripts\monitor.ps1 -Port $TargetPort" -ForegroundColor Green
