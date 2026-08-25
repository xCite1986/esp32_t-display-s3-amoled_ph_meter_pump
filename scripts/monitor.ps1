# monitor.ps1 - serielle Konsole oeffnen (115200 Baud)
#   Beenden mit Strg+C
param([string]$Port = "COM3")

. "$PSScriptRoot\acli.ps1"

$cli  = Get-ArduinoCli
$dest = Get-EspPort -Preferred $Port

Write-Host "Monitor auf $dest, 115200 Baud. 'help' eingeben. Beenden mit Strg+C." -ForegroundColor Cyan
# ohne --fqbn, damit derselbe Monitor fuer C3 (COM3) und Panel (COM6) passt
& $cli monitor -p $dest --config baudrate=115200
