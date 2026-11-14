@echo off
echo %time% >> C:\dev\ios\runanc_hb.txt
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" C:\dev\ios\git_save.py
