@echo off

net session >nul 2>&1
if %errorlevel% neq 0 (
    C:\Windows\System32\WindowsPowerShell\v1.0\powershell -Command "Start-Process cmd -ArgumentList '/c cd %~dp0 && %~nx0' -Verb RunAs" 2>nul   
    exit /b
)

set "envs=HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"

reg add "%envs%" /v SOLUM_MODULES /t REG_SZ /d "%cd%\modules\\" /f > nul
