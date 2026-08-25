# build.ps1 - Firmware oder Testsketch nur compilieren
#   .\scripts\build.ps1                 -> Hauptfirmware
#   .\scripts\build.ps1 -Sketch i2c     -> tools\i2c_adc_test
#   .\scripts\build.ps1 -Sketch motor   -> tools\motor_test
param([string]$Sketch = "main")

. "$PSScriptRoot\acli.ps1"

$path = switch ($Sketch) {
    "main"  { Join-Path $Root "firmware\ph_dosieranlage" }
    "i2c"   { Join-Path $Root "tools\i2c_adc_test" }
    "motor" { Join-Path $Root "tools\motor_test" }
    default { throw "Unbekannter Sketch '$Sketch' (main | i2c | motor)" }
}

$cli = Get-ArduinoCli
Write-Host "Compiliere $path" -ForegroundColor Cyan
& $cli compile --fqbn $Fqbn $path
if ($LASTEXITCODE -ne 0) { throw "Compile fehlgeschlagen" }
Write-Host "OK" -ForegroundColor Green
