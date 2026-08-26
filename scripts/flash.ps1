# flash.ps1 - compilieren und auf das T-Display S3 AMOLED an COM6 uebertragen
#   .\scripts\flash.ps1                 -> Hauptfirmware
#   .\scripts\flash.ps1 -Sketch i2c     -> Phase-1-Test
#   .\scripts\flash.ps1 -Sketch motor   -> Phase-2-Test
#   .\scripts\flash.ps1 -Port COM5      -> anderer Port
param(
    [string]$Sketch = "main",
    [string]$Port   = "COM6"
)

$TargetPort = $Port          # vor dem Dot-Sourcing sichern, sonst
                             # ueberschreibt acli.ps1 den Parameter
. "$PSScriptRoot\acli.ps1"

$path = switch ($Sketch) {
    "main"  { Join-Path $Root "firmware\ph_dosieranlage_s3" }
    "i2c"   { Join-Path $Root "tools\i2c_adc_test" }
    "motor" { Join-Path $Root "tools\motor_test" }
    default { throw "Unbekannter Sketch '$Sketch' (main | i2c | motor)" }
}

$cli  = Get-ArduinoCli
$dest = Get-EspPort -Preferred $TargetPort

Write-Host "Compiliere $path" -ForegroundColor Cyan
# --jobs 1 ist Absicht: die parallele Bibliothekserkennung von arduino-cli
# bleibt bei diesem Bibliothekssatz haengen (siehe docs/BEDIENPANEL.md).
& $cli compile --jobs 1 --fqbn $Fqbn $path
if ($LASTEXITCODE -ne 0) { throw "Compile fehlgeschlagen" }

Write-Host "Uebertrage auf $dest" -ForegroundColor Cyan
& $cli upload -p $dest --fqbn $Fqbn $path
if ($LASTEXITCODE -ne 0) {
    Write-Host "Upload fehlgeschlagen. Falls der Port belegt ist: seriellen Monitor schliessen." -ForegroundColor Yellow
    Write-Host "Notfalls den S3 in den Bootloader zwingen: BOOT halten, RESET tippen, BOOT loslassen." -ForegroundColor Yellow
    throw "Upload fehlgeschlagen"
}
Write-Host "Fertig. Monitor starten mit: .\scripts\monitor.ps1" -ForegroundColor Green
