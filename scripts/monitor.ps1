# monitor.ps1 - serielle Konsole oeffnen (115200 Baud)
#   Beenden mit Strg+C
param([string]$Port = "COM6")

$TargetPort = $Port          # vor dem Dot-Sourcing sichern, sonst
                             # ueberschreibt acli.ps1 den Parameter
. "$PSScriptRoot\acli.ps1"

$cli  = Get-ArduinoCli
$dest = Get-EspPort -Preferred $TargetPort

Write-Host "Monitor auf $dest, 115200 Baud. 'help' eingeben. Beenden mit Strg+C." -ForegroundColor Cyan
# ohne --fqbn: der Monitor braucht das Board nicht zu kennen
& $cli monitor -p $dest --config baudrate=115200
