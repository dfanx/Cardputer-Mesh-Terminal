[CmdletBinding()]
param(
    [string]$Port,
    [ValidateRange(115200, 921600)]
    [int]$Baud = 460800,
    [switch]$EraseAll
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$packageRoot = Split-Path -Parent $PSScriptRoot
$firmware = Join-Path $packageRoot 'firmware\firmware-complete.bin'
$esptool = Join-Path $packageRoot 'tools\esptool\esptool.exe'

if (-not (Test-Path -LiteralPath $firmware -PathType Leaf)) {
    throw "Complete firmware not found: $firmware"
}
if (-not (Test-Path -LiteralPath $esptool -PathType Leaf)) {
    throw "Offline esptool not found: $esptool"
}

function Get-CmtSerialPorts {
    $devices = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '\((COM\d+)\)' })
    foreach ($device in $devices) {
        if ($device.Name -match '\((COM\d+)\)') {
            [PSCustomObject]@{
                Port = $Matches[1]
                Name = [string]$device.Name
                Preferred = (([string]$device.PNPDeviceID -match 'VID_303A') -or ([string]$device.Name -match 'JTAG|Serial|Cardputer'))
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    $ports = @(Get-CmtSerialPorts)
    $preferred = @($ports | Where-Object Preferred)
    if ($preferred.Count -eq 1) {
        $Port = $preferred[0].Port
    } elseif ($ports.Count -eq 1) {
        $Port = $ports[0].Port
    } else {
        Write-Host 'Detected serial ports:' -ForegroundColor Cyan
        if ($ports.Count -eq 0) {
            Write-Host '  None. Reconnect Cardputer; if needed hold G0, tap Reset, then release G0.' -ForegroundColor Yellow
        } else {
            $ports | ForEach-Object { Write-Host ("  {0}  {1}" -f $_.Port, $_.Name) }
        }
        $Port = Read-Host 'Enter the Cardputer COM port (for example COM5)'
    }
}

if ($Port -notmatch '^COM\d+$') {
    throw "Invalid COM port: $Port"
}

Write-Host "Cardputer Mesh Terminal v1.1.0-rc1" -ForegroundColor Cyan
Write-Host "Target: $Port, baud: $Baud"
Write-Host 'This overwrites the bootloader, partition table, and application.' -ForegroundColor Yellow

$common = @('--chip', 'esp32s3', '--port', $Port, '--baud', "$Baud",
            '--before', 'default_reset', '--after', 'hard_reset')
if ($EraseAll) {
    $eraseArguments = $common + @('erase_flash')
    & $esptool @eraseArguments
    if ($LASTEXITCODE -ne 0) { throw "Flash erase failed, exit code $LASTEXITCODE" }
}

$writeArguments = $common + @(
    'write_flash', '--flash_mode', 'qio', '--flash_freq', '80m',
    '--flash_size', '8MB', '0x0', $firmware
)
& $esptool @writeArguments
if ($LASTEXITCODE -ne 0) {
    throw "Firmware flash failed, exit code $LASTEXITCODE"
}

Write-Host 'Flash complete. Install the LoRa antenna before entering the four-digit group PIN.' -ForegroundColor Green
