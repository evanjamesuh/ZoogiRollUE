# Like Scripts\run_demo.ps1 but for the checkout this script lives in (uncooked -game, CPU demo, -NoSound, log only).
#   .\Scripts\music\run_demo_wt.ps1 -Name music_combo -Quit 150 -Arena 1 -Extra "-ZoogiCombo -ZoogiComboDemo=2 -ZoogiMusicDebug"
param([string]$Name = "demo", [int]$Quit = 120, [int]$Players = 4, [int]$Arena = 0, [int]$Rounds = 2, [string]$Extra = "")
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$shots = "$Root\Saved\Shots\$Name"
New-Item -ItemType Directory -Force $shots, "$Root\Saved\Logs" | Out-Null
$log = "$Root\Saved\Logs\run_$Name.log"
$a = @("$Root\ZoogiRoll.uproject", "-game", "-windowed", "-ResX=1280", "-ResY=720", "-ZoogiAuto", "-ZoogiShots=$shots", "-ZoogiQuit=$Quit", "-ZoogiPlayers=$Players", "-ZoogiArena=$Arena", "-ZoogiRounds=$Rounds", "-nosplash", "-NoSound", "-log", "-abslog=$log") + ($Extra -split ' ' | Where-Object { $_ })
$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $a -PassThru
$p.WaitForExit()
"EXIT=$($p.ExitCode)" | Out-File -Append "$Root\Saved\Logs\run_$Name.exit"
