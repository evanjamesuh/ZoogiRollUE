# music2 pipeline: gate -> build -> gate -> import -> gate -> smoke runs (combo / classic / cards). Status: Saved\Logs\pipeline.status
param([switch]$SkipBuild, [switch]$SkipImport, [string[]]$Only)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$L = "$Root\Saved\Logs"; New-Item -ItemType Directory -Force $L | Out-Null
$st = "$L\pipeline.status"
function S($m) { "$(Get-Date -Format 'hh:mm:ss tt') $m" | Out-File -Append $st }
function Gate($n) { $r = & "$Root\Scripts\waitclear.ps1"; S "gate($n): $r"; return ($r -like 'READY*') }
if (-not $SkipBuild) { if (-not (Gate 'build')) { S 'ABORT'; exit 1 }; & "$Root\Scripts\build.ps1"; S "build: $((Get-Content "$L\build.log" -Tail 1))" ; if ((Get-Content "$L\build.log" -Tail 1) -ne 'BUILD_EXIT=0') { S 'PIPELINE_FAIL'; exit 1 } }
if (-not $SkipImport) { if (-not (Gate 'import')) { S 'ABORT'; exit 1 }; & "$Root\Scripts\music\run_python_wt.ps1" Scripts\import_music.py -LogName music_import; S "import done: $((Get-Content "$L\music_import.log" -Tail 1))" }
$runs = @(
  @{N='music_combo';   Q=150; A=1; X='-ZoogiCombo -ZoogiComboDemo=2 -ZoogiMusicDebug'},
  @{N='music_classic'; Q=120; A=2; X='-ZoogiMusicDebug'},
  @{N='music_shift';   Q=90;  A=1; X='-ZoogiCombo -ZoogiComboDemo=2 -ZoogiMusicForceShift=12 -ZoogiMusicDebug'},
  @{N='music_cards';   Q=150; A=1; X='-ZoogiCombo -ZoogiComboDemo=2 -ZoogiCards -ZoogiMusicDebug'})
foreach ($r in $runs) {
  if ($Only -and ($Only -notcontains $r.N)) { continue }
  if (-not (Gate $r.N)) { S 'ABORT'; exit 1 }
  Remove-Item -ErrorAction SilentlyContinue "$L\run_$($r.N).exit"
  & "$Root\Scripts\music\run_demo_wt.ps1" -Name $r.N -Quit $r.Q -Arena $r.A -Extra $r.X
  S "run $($r.N): $((Get-Content "$L\run_$($r.N).exit" -Tail 1))"
}
S 'PIPELINE_DONE'
