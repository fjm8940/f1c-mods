# Extended Damage Commentary 1.0.3 test gate (2026-10-08 batch E + U2).
# Builds the release DLL (normal + -HostGateTest) and the COMP-04b site test,
# then runs the site test. All build output goes to %TEMP%.
$ErrorActionPreference = 'Stop'
$gcc = 'C:\msys64\mingw32\bin\gcc.exe'
$here = $PSScriptRoot
$sdk = Join-Path $here '..\..\..\..\f1c_ida_decompiled\mods\sdk'
$sdk_tools = Join-Path $here '..\..\..\..\f1c_ida_decompiled\tools\sdk'
$tmp = Join-Path $env:TEMP ('edc_tests_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

& (Join-Path $here 'build.ps1') -OutputDirectory (Join-Path $tmp 'dll')
if ($LASTEXITCODE -ne 0) { throw 'normal build failed' }
& (Join-Path $here 'build.ps1') -OutputDirectory (Join-Path $tmp 'gate') -HostGateTest
if ($LASTEXITCODE -ne 0) { throw 'host-gate build failed' }

$exe = Join-Path $tmp 'site_patch_test.exe'
& $gcc -O2 -Wall -Wextra -Werror -I"$here" -I"$sdk" -I"$sdk_tools" -o $exe (Join-Path $here 'site_patch_test.c') (Join-Path $sdk_tools 'f1c_broker.c') -lkernel32 -lwinmm
if ($LASTEXITCODE -ne 0) { throw 'site test build failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'site test FAILED' }

Write-Output ('EDC 1.0.3 TESTS PASS (artifacts in ' + $tmp + ')')
