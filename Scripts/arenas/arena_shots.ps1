# Arena A/B screenshots for the Buzzy re-skin: for each arena index, one -game CPU run with -ZoogiArenaShots (fixed views,
# then the match continues for in-match shots). Waits for a free machine before each run (16 GB).
#   .\Scripts\arenas\arena_shots.ps1 -Tag before -Arenas 0,2,1 -Quit 75
param([string]$Tag = "after", [int[]]$Arenas = @(0), [int]$Quit = 75, [string]$Extra = "")
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$slugs = @('meadow','frozen','volcanic','cosmic','arabian','tomb','neon')
foreach ($a in $Arenas) {
  $w = & "$Root\Scripts\waitclear.ps1" -Minutes 60
  "$w"
  if ($w -notmatch 'READY') { "SKIP arena $a (busy)"; continue }
  $name = "arena_$($slugs[$a])_$Tag"
  Remove-Item -Recurse "$Root\Saved\Shots\$name" -EA SilentlyContinue
  $t = Get-Date
  & "$Root\Scripts\run_demo.ps1" -Name $name -Quit $Quit -Players 4 -Arena $a -Rounds 1 -Extra "-ZoogiArenaShots -ZoogiAnime=Off $Extra"
  $n = (Get-ChildItem "$Root\Saved\Shots\$name" -Filter *.png -EA SilentlyContinue).Count
  "RUN_DONE $name shots=$n in $([int]((Get-Date)-$t).TotalSeconds)s"
  Select-String -Path "$Root\Saved\Logs\run_$name.log" -Pattern 'Arena showcase: starting|Error|Fatal|Built arena|missing' | Select -First 12 | % { $_.Line.Substring(0,[Math]::Min(220,$_.Line.Length)) }
}
