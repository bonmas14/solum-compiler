@echo off

set "exe_name=slm"
set "cc=clang-cl"
set "lld=lld-link"

set "log_path=log\"
set "bin_path=bin\"
set "src_path=src\"
set "obj_main_path=obj\"

set "compile_config=/nologo /c /std:c++14 /Gd /TP /Z7 /W4 /WX- /EHsc /diagnostics:column /fp:precise -m64 /Iinclude /D _CRT_SECURE_NO_WARNINGS /D _UNICODE /D UNICODE"

if "%1"=="Release" (
    set "name=%bin_path%%exe_name%-r"
    set "obj_path=%obj_main_path%Release\\"
    set "complie=%compile_config% /Ox /Oi /GF /MT /GS /Gy /D NDEBUG"
    set "link_config=/INCREMENTAL:NO /OPT:REF /OPT:ICF"
) else (
    set "name=%bin_path%%exe_name%-d"
    set "obj_path=%obj_main_path%Debug\\"
    set "complie=%compile_config% /JMC /Od /MTd /GS /D DEBUG"
    set "link_config=/INCREMENTAL"
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

set manifest="level='asInvoker' uiAccess='false'"
set "deps=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib"
set "link=%link_config% /MACHINE:X64 /OUT:%out% /PDB:%pdb% /IMPLIB:%imp% /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT /MANIFEST /MANIFESTUAC:%manifest% /manifest:embed /DEBUG:FULL %deps%"

setlocal enabledelayedexpansion

set "obj_files="
for %%f in ("%obj_path%*.obj") do (
    set "obj_files=!obj_files! %%f"
)

echo %lld% %link% !obj_files!
%lld% %link% !obj_files!

echo:
echo Done!

endlocal
