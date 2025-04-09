@echo off

:: Assuming that only msvc sets this variable
if not defined INCLUDE (
    set vc="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    call %vc% 
)


set "cc=clang-cl"
set "lld=lld-link"

set "bin_path=bin\"
set "src_path=src\"
set "obj_main_path=obj\"

set "compile_config=/nologo /c /std:c++14 /Gd /TP /Z7 /W4 /WX- /EHsc /diagnostics:column /fp:precise -m64 /Iinclude /D _CRT_SECURE_NO_WARNINGS /D _UNICODE /D UNICODE"

if "%1"=="Release" (
    set "name=%bin_path%slm-r"
    set "obj_path=%obj_main_path%Release\\"
    set "complie=%compile_config% /Ox /Oi /GF /MT /GS /Gy /D NDEBUG"
    set "link_config=/INCREMENTAL:NO /OPT:REF /OPT:ICF"
) else (
    set "name=%bin_path%slm-d"
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

if not exist %obj_path% (
    mkdir %obj_path%
)

echo:
echo Compiling...


for %%f in ("%src_path%*.cpp") do (
    echo File: %%f
    %cc% %complie% /Fo"%obj_path%" "%%f"
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

%lld% %link% !obj_files!

echo %lld% %link% !obj_files!
echo:
echo Done!

endlocal
