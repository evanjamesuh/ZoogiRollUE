# Runs the game (uncooked, -game) in CPU demo mode and saves screenshots.
#   .\Scripts\run_demo.ps1 -Name demo -Quit 120 -Players 4 -Arena 0 -Rounds 2 -Extra "-ZoogiMenuShot -ZoogiHumanTest"
param([string]$Name = "demo", [int]$Quit = 120, [int]$Players = 4, [int]$Arena = 0, [int]$Rounds = 2, [string]$Extra = "")
$UE = "C:\Program Files\Epic Games\UE_5.6"
$shots = "C:\dev\ZoogiRollUnreal\Saved\Shots\$Name"
New-Item -ItemType Directory -Force $shots | Out-Null
$log = "C:\dev\ZoogiRollUnreal\Saved\Logs\run_$Name.log"
$args = @("C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject", "-game", "-windowed", "-ResX=1600", "-ResY=900", "-ZoogiAuto", "-ZoogiShots=$shots", "-ZoogiQuit=$Quit", "-ZoogiPlayers=$Players", "-ZoogiArena=$Arena", "-ZoogiRounds=$Rounds", "-nosplash", "-NoSound", "-log", "-abslog=$log") + ($Extra -split ' ' | Where-Object { $_ })
$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $args -PassThru
$p.WaitForExit()
"EXIT=$($p.ExitCode)" | Out-File -Append "C:\dev\ZoogiRollUnreal\Saved\Logs\run_$Name.exit"
