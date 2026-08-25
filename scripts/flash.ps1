# flash.ps1 - compilieren und auf den ESP32-C3 an COM3 uebertragen
#   .\scripts\flash.ps1                 -> Hauptfirmware
#   .\scripts\flash.ps1 -Sketch i2c     -> Phase-1-Test
#   .\scripts\flash.ps1 -Sketch motor   -> Phase-2-Test
#   .\scripts\flash.ps1 -Port COM5      -> anderer Port
param(
    [string]$Sketch = "main",
    [string]$Port   = "COM3"
)

. "$PSScriptRoot\acli.ps1"

$path = switch ($Sketch) {
    "main"  { Join-Path $Root "firmware\ph_dosieranlage" }
    "i2c"   { Join-Path $Root "tools\i2c_adc_test" }
    "motor" { Join-Path $Root "tools\motor_test" }
    default { throw "Unbekannter Sketch '$Sketch' (main | i2c | motor)" }
}

$cli  = Get-ArduinoCli
$dest = Get-EspPort -Preferred $Port

Write-Host "Compiliere $path" -ForegroundColor Cyan
& $cli compile --fqbn $Fqbn $path
if ($LASTEXITCODE -ne 0) { throw "Compile fehlgeschlagen" }

Write-Host "Uebertrage auf $dest" -ForegroundColor Cyan
& $cli upload -p $dest --fqbn $Fqbn $path
if ($LASTEXITCODE -ne 0) {
    Write-Host "Upload fehlgeschlagen. Falls der Port belegt ist: seriellen Monitor schliessen." -ForegroundColor Yellow
    Write-Host "Notfalls den ESP32-C3 in den Bootloader zwingen: BOOT halten, RESET tippen, BOOT loslassen." -ForegroundColor Yellow
    throw "Upload fehlgeschlagen"
}
Write-Host "Fertig. Monitor starten mit: .\scripts\monitor.ps1" -ForegroundColor Green
