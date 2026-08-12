[CmdletBinding()]
param(
    [string]$Version = 'v1.1.0-rc1',
    [Parameter(Mandatory = $true)]
    [string]$EsptoolDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dist = Join-Path $repo 'dist'
$packageName = "Cardputer-Mesh-Terminal-$Version-windows-x64"
$stage = Join-Path $dist $packageName
$zip = Join-Path $dist "$packageName.zip"

if (-not $stage.StartsWith($dist, [StringComparison]::OrdinalIgnoreCase)) {
    throw '拒絕處理 dist 以外的 staging 目錄。'
}
if (Test-Path -LiteralPath $stage) { Remove-Item -Recurse -Force -LiteralPath $stage }
if (Test-Path -LiteralPath $zip) { Remove-Item -Force -LiteralPath $zip }

$build = Join-Path $repo '.pio\build\cardputer-adv'
$required = @(
    (Join-Path $build 'firmware.bin'),
    (Join-Path $build 'bootloader.bin'),
    (Join-Path $build 'partitions.bin'),
    (Join-Path $repo 'deploy\firmware-complete.bin'),
    (Join-Path $EsptoolDirectory 'esptool.exe')
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "缺少打包必要檔案：$path"
    }
}

$directories = @('deploy', 'firmware', 'tools\esptool', 'source')
foreach ($directory in $directories) {
    New-Item -ItemType Directory -Force -Path (Join-Path $stage $directory) | Out-Null
}

Copy-Item -LiteralPath (Join-Path $repo 'deploy\flash-windows.ps1') -Destination (Join-Path $stage 'deploy')
Copy-Item -LiteralPath (Join-Path $repo 'deploy\flash-windows.cmd') -Destination (Join-Path $stage 'deploy')
Copy-Item -LiteralPath (Join-Path $repo 'deploy\README-DEPLOY.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $repo 'deploy\PACKAGE-SCOPE.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $repo 'THIRD_PARTY_NOTICES.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $repo 'deploy\firmware-complete.bin') -Destination (Join-Path $stage 'firmware')
Copy-Item -LiteralPath (Join-Path $build 'firmware.bin') -Destination (Join-Path $stage 'firmware')
Copy-Item -LiteralPath (Join-Path $build 'bootloader.bin') -Destination (Join-Path $stage 'firmware')
Copy-Item -LiteralPath (Join-Path $build 'partitions.bin') -Destination (Join-Path $stage 'firmware')
Copy-Item -LiteralPath (Join-Path $repo '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin') -Destination (Join-Path $stage 'firmware')
Copy-Item -Recurse -Force -Path (Join-Path $EsptoolDirectory '*') -Destination (Join-Path $stage 'tools\esptool')

$sourceItems = @('include', 'src', 'test', 'data', 'docs', 'tools', 'deploy',
                 'README.md', 'CHANGELOG.md', 'CLAUDE.md', 'platformio.ini',
                 'requirements-dev.txt', 'THIRD_PARTY_NOTICES.md')
foreach ($item in $sourceItems) {
    Copy-Item -Recurse -Force -LiteralPath (Join-Path $repo $item) -Destination (Join-Path $stage 'source')
}
$duplicateFirmware = Join-Path $stage 'source\deploy\firmware-complete.bin'
if (Test-Path -LiteralPath $duplicateFirmware) {
    Remove-Item -Force -LiteralPath $duplicateFirmware
}
Set-Content -LiteralPath (Join-Path $stage 'VERSION.txt') -Value $Version -Encoding ASCII

$flashCommand = @'
esptool --chip esp32s3 --port <PORT> --baud 460800 --before default_reset --after hard_reset write_flash --flash_mode qio --flash_freq 80m --flash_size 8MB 0x0 firmware-complete.bin
'@
Set-Content -LiteralPath (Join-Path $stage 'firmware\FLASH-COMMAND.txt') -Value $flashCommand -Encoding UTF8

$artifacts = Get-ChildItem -File -LiteralPath (Join-Path $stage 'firmware')
$manifest = foreach ($artifact in $artifacts) {
    [PSCustomObject]@{
        file = $artifact.Name
        bytes = $artifact.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.FullName).Hash
    }
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'firmware\SHA256SUMS.json') -Encoding UTF8

Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal
$zipItem = Get-Item -LiteralPath $zip
[PSCustomObject]@{
    Path = $zipItem.FullName
    Bytes = $zipItem.Length
    SHA256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipItem.FullName).Hash
} | Format-List
