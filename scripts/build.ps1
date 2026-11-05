[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PicoSdkPath,
    [Parameter(Mandatory=$true)][string]$PicoPioUsbPath,
    [Parameter(Mandatory=$true)][string]$ToolchainPath,
    [Parameter(Mandatory=$true)][string]$NinjaPath,
    [string]$BuildDir,
    [string]$CmakeExe='cmake',
    [string]$PythonExe,
    [string]$PioasmDir,
    [string]$PicotoolDir
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$BuildDir) { $BuildDir=Join-Path $root 'build/fruit-jam' }
$lock=Get-Content (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json
foreach ($dependency in @(@($PicoSdkPath,$lock.'pico-sdk'.commit),@($PicoPioUsbPath,$lock.'Pico-PIO-USB'.commit),@((Join-Path $PicoSdkPath 'lib/tinyusb'),$lock.tinyusb.commit))) {
    $revision=& git -C $dependency[0] rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $revision -ne $dependency[1]) { throw "Dependency revision mismatch: $($dependency[0])" }
    $changed=& git -C $dependency[0] status --porcelain --untracked-files=no
    if ($LASTEXITCODE -ne 0 -or $changed) { throw "Dependency is modified: $($dependency[0])" }
}
$arguments=@('-S',$root,'-B',[IO.Path]::GetFullPath($BuildDir),'-G','Ninja',
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=$([IO.Path]::GetFullPath($NinjaPath))",
    "-DPICO_SDK_PATH=$([IO.Path]::GetFullPath($PicoSdkPath))",
    "-DPICO_PIO_USB_PATH=$([IO.Path]::GetFullPath($PicoPioUsbPath))",
    "-DPICO_TOOLCHAIN_PATH=$([IO.Path]::GetFullPath($ToolchainPath))",
    '-DPICO_PLATFORM=rp2350-arm-s','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
if($PythonExe){$arguments+="-DPython3_EXECUTABLE:FILEPATH=$([IO.Path]::GetFullPath($PythonExe))"}
if($PioasmDir){$arguments+="-Dpioasm_DIR=$([IO.Path]::GetFullPath($PioasmDir))"}
if($PicotoolDir){$arguments+="-Dpicotool_DIR=$([IO.Path]::GetFullPath($PicotoolDir))"}
& $CmakeExe @arguments
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
& $CmakeExe --build $BuildDir --target picoUzeboxCartridgeRuntime --parallel 8
if($LASTEXITCODE -ne 0){throw 'Build failed'}
Get-FileHash -Algorithm SHA256 (Join-Path $BuildDir 'picoUzeboxCartridgeRuntime.uf2')
