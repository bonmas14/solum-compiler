@echo off
setlocal

set "filename=slm"
set "cc=clang-cl"
set "lld=lld-link"

set "bin_dir=bin\"
set "src_dir=src\"
set "obj_out_dir=obj\"

set "linking_deps="

set "defines=/D _CRT_SECURE_NO_WARNINGS /D _UNICODE /D UNICODE"
set "compile_config=/nologo -fansi-escape-codes -fcolor-diagnostics -m64 /c /std:c++14 /utf-8 /Gr /TP /Z7 /W4 /WX- /diagnostics:column /fp:precise /Iinclude /Ideps %defines%"

if "%1"=="Release"  ( goto release )
if "%1"=="Sanitize" ( goto sanitize )
if "%1"=="Debug"    ( goto debug )
if "%1"=="Clear"    ( goto clear )

:: some aliases for help
if "%1"=="Help"  ( goto help )
if "%1"=="help"  ( goto help )
if "%1"=="-h"    ( goto help )
if "%1"=="/?"    ( goto help )

goto debug

:clear
if exist %bin_dir% ( rmdir /s /q %bin_dir% )
if exist %obj_out_dir% ( rmdir /s /q %obj_out_dir% )
goto end

:help
echo:
echo Solum compiler build tool
echo:
echo Targets:
echo     Debug    --- build in debug mode, default target.
echo     Release  --- build in release mode.
echo     Sanitize --- build in verbose debug mode with address sanitizing.
echo     Help     --- display this help menu.
echo     Clear    --- clear the project directories.
echo:
goto end

:debug                                  :: Debug configuration
echo Building: Debug
echo:
set "name=%bin_dir%%filename%-d"
set "obj_path=%obj_out_dir%Debug\\"
set "complie=%compile_config% /JMC /Od /MTd /GS /D DEBUG"
set "link_config=/INCREMENTAL /time"
set "per_target_link_deps="
goto configured

:release                                :: Release configuration
echo Building: Release
echo:
set "name=%bin_dir%%filename%-r"
set "obj_path=%obj_out_dir%Release\\"
set "complie=%compile_config% /Ox /Oi /GF /MT /Gy /D NDEBUG /D RADDBG_MARKUP_STUBS"
set "link_config=/INCREMENTAL:NO /OPT:REF /OPT:ICF /time"
set "per_target_link_deps="
goto configured

:sanitize                               :: Sanitize configuration
echo Building: Sanitize
echo:
set "name=%bin_dir%%filename%-d"
set "obj_path=%obj_out_dir%Debug\\"
set "complie=%compile_config% /fsanitize=address /JMC /Od /MT /GS /D DEBUG /D VERBOSE"
set "link_config=/INCREMENTAL /time"
set "per_target_link_deps=clang_rt.asan_static-x86_64.lib clang_rt.asan-x86_64.lib"
goto configured

:configured

if not exist %bin_dir% ( mkdir %bin_dir% )
if not exist %obj_out_dir% ( mkdir %obj_out_dir% )
if not exist %obj_path% ( mkdir %obj_path% )

echo Compiling...

for %%f in ("%src_dir%*.cpp") do (
    echo File: %%f
    %cc% %complie% /Fo"%obj_path%" "%%f"
)

echo:
echo Linking...

set pdb="%name%.pdb"
set out="%name%.exe" 
set imp="%name%.lib"

set "link=/MACHINE:X64 /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT /DEBUG:FULL %link_config% /OUT:%out% /PDB:%pdb% /IMPLIB:%imp% /DEBUG:FULL"

setlocal enabledelayedexpansion

set "obj_files="
for %%f in ("%obj_path%*.obj") do (
    set "obj_files=!obj_files! %%f"
)

echo %lld% %link% %per_target_link_deps% %linking_deps% !obj_files!
echo:
%lld% %link% %per_target_link_deps% %linking_deps% !obj_files!

echo:
echo Done!

endlocal
:end
endlocal
