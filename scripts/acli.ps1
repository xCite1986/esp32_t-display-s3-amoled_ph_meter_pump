# acli.ps1 - gemeinsame Einstellungen fuer alle Build-/Flash-Skripte
#
# Es ist keine separate arduino-cli-Installation noetig: die Arduino IDE 2
# bringt eine mit. Falls arduino-cli im PATH liegt, wird die bevorzugt.

$ErrorActionPreference = "Stop"

$script:Fqbn = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB"
$script:DefaultPort = "COM6"   # NICHT $Port nennen: kollidiert mit param($Port)
$script:Root = Split-Path -Parent $PSScriptRoot

function Get-ArduinoCli {
    $inPath = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($inPath) { return $inPath.Source }

    $bundled = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    if (Test-Path $bundled) { return $bundled }

    throw "arduino-cli nicht gefunden. Arduino IDE 2 installieren oder arduino-cli in den PATH legen."
}

function Get-EspPort {
    param([string]$Preferred = $script:DefaultPort)

    $cli = Get-ArduinoCli
    $list = & $cli board list --format json 2>$null | ConvertFrom-Json

    # Bevorzugten Port nehmen, wenn er da ist
    foreach ($p in $list.detected_ports) {
        if ($p.port.address -eq $Preferred) { return $Preferred }
    }
    # sonst den ersten seriellen Port mit ESP32-Kern
    foreach ($p in $list.detected_ports) {
        if ($p.port.protocol -eq "serial" -and $p.matching_boards) {
            Write-Host "Hinweis: $Preferred nicht gefunden, benutze $($p.port.address)" -ForegroundColor Yellow
            return $p.port.address
        }
    }
    throw "Kein ESP32 an einem seriellen Port gefunden. USB-Kabel pruefen (Datenkabel, nicht nur Ladekabel)."
}
