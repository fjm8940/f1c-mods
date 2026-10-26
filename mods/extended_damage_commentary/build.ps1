param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'build'),
    [switch]$HostGateTest
)
$ErrorActionPreference = 'Stop'
$gcc = 'C:\msys64\mingw32\bin\gcc.exe'
$windres = 'C:\msys64\mingw32\bin\windres.exe'
$source = Join-Path $PSScriptRoot 'extended_damage_commentary.c'
$sdk = Join-Path $env:F1C_SDK_DIR '..\..\mods\sdk'
$sdk_tools = $env:F1C_SDK_DIR
$dll = Join-Path $OutputDirectory 'extended_damage_commentary.dll'
$resource = Join-Path $OutputDirectory 'meta.o'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
Push-Location $PSScriptRoot
& $windres -O coff -o $resource 'extended_damage_commentary_meta.rc'
Pop-Location
if ($LASTEXITCODE -ne 0) { throw 'windres failed' }
$flags = @('-shared','-O2','-s','-Wall','-Wextra','-Werror','-fno-ident','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-fno-unwind-tables','-fno-asynchronous-unwind-tables')
$testFlag = @()
if ($HostGateTest) { $testFlag = @('-DEDC_HOST_GATE_TEST') }
& $gcc @flags @testFlag '-I' $sdk '-I' $sdk_tools -o $dll $source (Join-Path $sdk_tools 'f1c_broker.c') $resource -lkernel32 -lwinmm '-Wl,--gc-sections,--strip-all'
if ($LASTEXITCODE -ne 0) { throw 'build failed' }
Get-FileHash -Algorithm SHA256 -LiteralPath $dll
