[CmdletBinding()]
param(
    [string]$Port,
    [switch]$Monitor
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$pio = Join-Path $repoRoot 'tools\pio.cmd'
$venvPython = Join-Path $repoRoot '.venv\Scripts\python.exe'

if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    throw @"
找不到 $venvPython
先在 repo 根目錄建立開發環境：
  python -m venv .venv
  .\.venv\Scripts\python -m pip install -r requirements-dev.txt
"@
}

function Get-CmtSerialPorts {
    $devices = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '\((COM\d+)\)' })
    foreach ($device in $devices) {
        if ($device.Name -match '\((COM\d+)\)') {
            [PSCustomObject]@{
                Port = $Matches[1]
                Name = [string]$device.Name
                Preferred = (([string]$device.PNPDeviceID -match 'VID_303A') -or ([string]$device.Name -match 'JTAG|Cardputer'))
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

Write-Host "Cardputer Mesh Terminal - build and flash from source" -ForegroundColor Cyan
Write-Host "Target: $Port"
Write-Host 'This does a full chip erase, then writes the bootloader, partition table, and application.' -ForegroundColor Yellow
Write-Host 'The erase wipes NVS: saved callsign and pairing state are lost, not just the app image.' -ForegroundColor Yellow

# Always a full erase before flashing: the logfs partition (ADR-008) sits at a
# fixed offset baked into the current partition table, so a plain overwrite
# can leave stale bytes from a differently-laid-out prior flash instead of a
# clean filesystem. A full erase is the only way this script can guarantee
# what's on the chip afterward matches what was just built.
& $pio run -e cardputer-adv -t erase --upload-port $Port
if ($LASTEXITCODE -ne 0) { throw "Flash erase failed, exit code $LASTEXITCODE" }

& $pio run -e cardputer-adv -t upload --upload-port $Port
if ($LASTEXITCODE -ne 0) {
    throw "Build or flash failed, exit code $LASTEXITCODE"
}

Write-Host 'Flash complete. Install the LoRa antenna before entering the four-digit group PIN.' -ForegroundColor Green

if ($Monitor) {
    & $pio device monitor -e cardputer-adv --port $Port
}
