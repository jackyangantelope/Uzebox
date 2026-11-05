param([Parameter(Mandatory=$true)][string]$Destination)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$lock = Get-Content (Join-Path $PSScriptRoot '../dependencies.lock.json') -Raw | ConvertFrom-Json
$root = [IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $root | Out-Null
foreach ($name in @('pico-sdk','Pico-PIO-USB')) {
    $path=Join-Path $root $name
    if (Test-Path -LiteralPath $path) { throw "Destination exists; preserve it: $path" }
    $dependency=$lock.$name
    & git clone --no-checkout $dependency.url $path
    if ($LASTEXITCODE -ne 0) { throw "Clone failed: $name" }
    & git -C $path checkout --detach $dependency.commit
    if ($LASTEXITCODE -ne 0) { throw "Checkout failed: $name" }
}
& git -C (Join-Path $root 'pico-sdk') submodule update --init lib/tinyusb
if ($LASTEXITCODE -ne 0) { throw 'TinyUSB initialization failed' }
Write-Host "Dependencies ready: $root"
