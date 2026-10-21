@echo off
title Zoogi Roll - phone stream
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-ZoogiPhone.ps1" %*
pause
