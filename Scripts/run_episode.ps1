# Episode Engine test runs from this checkout (main or a worktree): uncooked -game, CPU auto match, own log / shot dir.
#   .\Scripts\run_episode.ps1 -Name ep_classic -Quit 240 -Players 4 -Arena 0 -Rounds 1 -Extra "-ZoogiEpisode=Balanced"
#   .\Scripts\run_episode.ps1 -Name ep_titanB -Quit 90 -Players 4 -Arena 0 -Rounds 1 -Extra "-ZoogiEpisodeTest=B -ZoogiNoForm"
# Episode screenshots: -ZoogiEpisodeShots=<Saved\Shots\Name> is always passed (Episode presentation frames only).
param([string]$Name = "episode", [int]$Quit = 180, [int]$Players = 4, [int]$Arena = 0, [int]$Rounds = 1, [string]$Extra = "", [int]$TimeoutSec = 900)
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Root = Split-Path -Parent $PSScriptRoot
$shots = "$Root\Saved\Shots\$Name"
New-Item -ItemType Directory -Force $shots | Out-Null
New-Item -ItemType Directory -Force "$Root\Saved\Logs" | Out-Null
$log = "$Root\Saved\Logs\run_$Name.log"
$args = @("$Root\ZoogiRoll.uproject", "-game", "-windowed", "-ResX=1600", "-ResY=900", "-ZoogiAuto", "-ZoogiEpisodeShots=$shots", "-ZoogiQuit=$Quit", "-ZoogiPlayers=$Players", "-ZoogiArena=$Arena", "-ZoogiRounds=$Rounds", "-nosplash", "-NoSound", "-log", "-abslog=$log") + ($Extra -split ' ' | Where-Object { $_ })
$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $args -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; "EXIT=TIMEOUT" | Out-File -Append "$Root\Saved\Logs\run_$Name.exit" }
else { "EXIT=$($p.ExitCode)" | Out-File -Append "$Root\Saved\Logs\run_$Name.exit" }
