param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM[1-9][0-9]*$')]
    [string]$Port
)

$ErrorActionPreference = 'Stop'
$files = @(
    'bootloader.bin',
    'partitions.bin',
    'boot_app0.bin',
    'firmware.bin'
)
foreach ($file in $files) {
    $path = Join-Path $PSScriptRoot $file
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required flash image not found: $path"
    }
}

Write-Host "Target: ESP32-S3 on $Port"
Write-Host 'This writes bootloader, partition table, OTA metadata and app image.'
Write-Host 'No whole-chip erase is run; sectors in the listed image ranges may be rewritten.'
Write-Host 'The separate NVS partition is not in the write ranges.'
$confirmation = Read-Host 'Type FLASH to continue'
if ($confirmation -cne 'FLASH') {
    throw 'Flash cancelled.'
}

$esptoolArgs = @(
    '-3', '-m', 'esptool',
    '--chip', 'esp32s3',
    '--port', $Port,
    '--baud', '460800',
    'write_flash',
    '--flash_mode', 'qio',
    '--flash_freq', '80m',
    '--flash_size', '8MB',
    '0x0000', (Join-Path $PSScriptRoot 'bootloader.bin'),
    '0x8000', (Join-Path $PSScriptRoot 'partitions.bin'),
    '0xe000', (Join-Path $PSScriptRoot 'boot_app0.bin'),
    '0x10000', (Join-Path $PSScriptRoot 'firmware.bin')
)
& py @esptoolArgs
if ($LASTEXITCODE -ne 0) {
    throw "esptool failed with exit code $LASTEXITCODE"
}
Write-Host 'Flash complete. Reset the StickS3 and verify boot/interaction manually.'
