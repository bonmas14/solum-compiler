@echo off
set "exe_name=slm"
set "cc=clang-cl"
set "lld=lld-link"

set "log_path=log\"
set "bin_path=bin\"
set "src_path=src\"
set "obj_main_path=obj\"

set "global_defines=/D _CRT_SECURE_NO_WARNINGS /D _UNICODE /D UNICODE"
set "compile_config=/nologo /c /std:c++14 /utf-8 /Gr /TP /Z7 /W4 /WX- /diagnostics:column /fp:precise /Iinclude %global_defines%"

if "%1"=="Release" (
    set "name=%bin_path%%exe_name%-r"
    set "obj_path=%obj_main_path%Release\\"
    set "complie=%compile_config% /Ox /Oi /GF /MT /GS /Gy /D NDEBUG"
    set "link_config=/INCREMENTAL:NO /OPT:REF /OPT:ICF /time"
) else (
    set "name=%bin_path%%exe_name%-d"
    set "obj_path=%obj_main_path%Debug\\"
    set "complie=%compile_config% /JMC /Od /MTd /GS /D DEBUG"
    set "link_config=/INCREMENTAL /time"
)

if not exist "%obj_main_path%" (
    mkdir %obj_main_path%
)

if not exist %bin_path% (
    mkdir %bin_path%
)

if not exist %log_path% (
    mkdir %log_path%
)

if not exist %obj_path% (
    mkdir %obj_path%
)

echo Compiling...

for %%f in ("%src_path%*.cpp") do (
    echo File: %%f
    start "Compile %%f" /B %cc% %complie% /Fo"%obj_path%" "%%f" > %log_path%\log_%%~nxf.txt 2>&1
)

:wait 
tasklist | find "%cc%.exe" > nul
if %errorlevel% == 0 (
    timeout /t 0 > nul
    goto wait
)

echo:
echo log output:

:: Logging out everything so we dont have race conditions on stdout
for %%f in ("%src_path%*.cpp") do (
    cat %log_path%\log_%%~nxf.txt
)


echo:
echo Linking...

set pdb="%name%.pdb"
set out="%name%.exe" 
set imp="%name%.lib"

set "deps="
set "link=/MACHINE:X64 /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT /DEBUG:FULL %link_config% /OUT:%out% /PDB:%pdb% /IMPLIB:%imp% /DEBUG:FULL %deps%"

setlocal enabledelayedexpansion

set "obj_files="
for %%f in ("%obj_path%*.obj") do (
    set "obj_files=!obj_files! %%f"
)

echo %lld% %link% !obj_files!
echo:
%lld% %link% !obj_files!

echo:
echo Done!

endlocal
