# E2E 1.0.5 (copied from 1.0.3): the release ZIP through the real Lightloader 2.2.2 manager code (reflection),
# into scratch game folders in %TEMP%. A: fresh install. B: update over 1.0.0 (its SDT stays,
# player INI kept, 35 engineer WAVs added). C: 2.2.1 manager also installs the WAVs.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$zip = [IO.File]::ReadAllBytes((Join-Path $here 'mechanical_failures-v1.0.7-lightloader.zip'))
$old = [IO.File]::ReadAllBytes((Join-Path $here '..\v1.0.0-rc.1\mechanical_failures-v1.0.0-lightloader.zip'))
$lw = '%USERPROFILE%\Desktop\F1\development\lightloader\v2.0.0-hub-dev.1'
$vanilla = '%USERPROFILE%\Documents\VM\F1 Challenge V10_clean\Audio\Commentary\Pws\PC_RE_MAIN.SDT'
$dll = (Get-FileHash -Algorithm SHA256 (Join-Path $here 'build\mechanical_failures.dll')).Hash
$fails = 0
function Check($ok, $name) { if ($ok) { "PASS $name" } else { "FAIL $name"; $script:fails++ } }
function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash }
function NewGame() {
    $g = Join-Path $env:TEMP ("mf_e2e101_" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path (Join-Path $g 'Audio\Commentary\Pws'), (Join-Path $g 'plugins\lightloader') | Out-Null
    Copy-Item -LiteralPath $vanilla -Destination (Join-Path $g 'Audio\Commentary\Pws\PC_RE_MAIN.SDT')
    return $g
}
function Wavs($g) { @(Get-ChildItem (Join-Path $g 'Audio\MechanicalFailures') -Filter '*.wav' | Where-Object { $_.Name -cmatch '^MF_[A-Z]' }).Count }
$asm = [Reflection.Assembly]::LoadFile((Join-Path $lw 'release_2.2.2_2026-10-06\build\manager\LightloaderManager.exe'))
$install = $asm.GetType('Lightloader.Core').GetMethod('InstallPackage')

$g = NewGame
$s = $install.Invoke($null, @([string]$g, $zip, $null)); "--- A:`n$s"
Check ((Sha (Join-Path $g 'plugins\lightloader\mechanical_failures.dll')) -eq $dll) 'A: 1.0.3 DLL installed'
Check ((Wavs $g) -eq 35) 'A: 35 engineer WAVs in Audio\MechanicalFailures'
Check (Test-Path (Join-Path $g 'Audio\MechanicalFailures\mf_engine_blow.wav')) 'A: blow sound'
Check (Test-Path (Join-Path $g 'Audio\MechanicalFailures\mf_backfire_3.wav')) 'A: own backfire pops'
Check ((Sha (Join-Path $g 'Audio\Commentary\Pws\PC_RE_MAIN.SDT')) -like '1730F4CD*') 'A: original SDT untouched'
Check ((Get-Content -Raw (Join-Path $g 'plugins\lightloader\mechanical_failures.ini')) -match 'VoiceVolume=100') 'A: INI with VoiceVolume'

$g = NewGame
$null = $install.Invoke($null, @([string]$g, $old, $null))
Set-Content -LiteralPath (Join-Path $g 'plugins\lightloader\mechanical_failures.ini') -Value "[Engine]`r`nAIFailures=0"
$s = $install.Invoke($null, @([string]$g, $zip, $null)); "--- B:`n$s"
Check ((Sha (Join-Path $g 'plugins\lightloader\mechanical_failures.dll')) -eq $dll) 'B: update installs 1.0.3 DLL'
Check ((Get-Content -Raw (Join-Path $g 'plugins\lightloader\mechanical_failures.ini')) -match 'AIFailures=0') 'B: player INI kept'
Check ((Wavs $g) -eq 35) 'B: engineer WAVs added on update'
Check ((Sha (Join-Path $g 'Audio\Commentary\Pws\PC_RE_MAIN.SDT')) -like '9C734314*') 'B: 1.0.0 SDT left as it was (harmless)'

$asm2 = [Reflection.Assembly]::LoadFile((Join-Path $lw 'release_2.2.1_2026-10-04\build\manager\LightloaderManager.exe'))
$g = NewGame
$null = $asm2.GetType('Lightloader.Core').GetMethod('InstallPackage').Invoke($null, @([string]$g, $zip, $null))
Check ((Wavs $g) -eq 35) 'C: 2.2.1 manager installs the WAVs'
"FAILS: $fails"
exit $fails
