:: @echo off
:: powershell.exe -noe -c "&{Import-Module """C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"""; Enter-VsDevShell 8b14fce0; cd """C:\Users\bonmas14\projects\solum-compiler"""; msbuild """.\solum.sln""" '/p:Configuration=Debug' '/p:Platform=x64'; exit; }"

@echo off

set "config=Debug"
if "%1"=="Release" (
    set "config=Release"
)

powershell.exe -noe -c "&{ Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'; Enter-VsDevShell 8b14fce0; cd 'C:\Users\bonmas14\projects\solum-compiler'; msbuild '.\solum.sln' '/p:Configuration=%config%' '/p:Platform=x64'; exit; }"
