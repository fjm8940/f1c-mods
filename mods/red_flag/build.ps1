# Red Flag 1.1.6-rc.1: the shipped 1.1.5 base plus the consent-gated per-session
# usage summary (rf_usage.h on the shared f1c_usage.h core), the shared crash VEH
# (installed after successful init, removed on unload), and the broker ABI15
# session-end flush. No gameplay change. build.ps1 compiles the hardened source
# with -Igenerated (for ../src includes) and -I mods/sdk (for f1c_usage.h).
param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'build'))
$ErrorActionPreference = 'Stop'
$gcc = 'C:\msys64\mingw32\bin\gcc.exe'
$windres = 'C:\msys64\mingw32\bin\windres.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sdk = (Resolve-Path (Join-Path $env:F1C_SDK_DIR '..\..\mods\sdk')).Path
$output = Join-Path $OutputDirectory 'redflag.dll'
$resource = Join-Path $OutputDirectory 'redflag_meta.o'
if (Test-Path -LiteralPath $output) { throw "Refusing to overwrite $output" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
Push-Location $PSScriptRoot
try {
    & $windres '-I.' 'redflag_meta.rc' '-O' 'coff' '-o' $resource
    if ($LASTEXITCODE -ne 0) { throw 'Metadata resource build failed.' }
} finally { Pop-Location }
Push-Location $project
try {
    & $gcc '-shared' '-O2' '-s' '-DAUTO_REDFLAG_ENABLED=1' '-fvisibility=hidden' `
        '-fno-asynchronous-unwind-tables' '-fno-unwind-tables' '-ffunction-sections' `
        '-fdata-sections' '-static' '-static-libgcc' '-Wl,--gc-sections' `
        '-Wl,--exclude-all-symbols' '-Wl,--no-insert-timestamp' '-Wl,--image-base,0x68000000' `
        '-DRF_RELEASE' '-Igenerated' '-I' $sdk '-I' (Join-Path $env:F1C_SDK_DIR '..\..') '-o' $output (Join-Path $PSScriptRoot 'redflag.hardened.c') $resource `
        '-lkernel32' '-luser32' '-lwinmm' '-lgdi32'
    if ($LASTEXITCODE -ne 0) { throw "Red Flag build failed: $LASTEXITCODE" }
} finally { Pop-Location }
Get-FileHash -Algorithm SHA256 -LiteralPath $output
