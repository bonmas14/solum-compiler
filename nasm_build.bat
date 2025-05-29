@echo off
setlocal

nasm -g -f win64 output.nasm -O0 -o .\obj\output.obj
pushd bin
lld-link /MACHINE:X64 /DYNAMICBASE /SUBSYSTEM:CONSOLE /DEBUG:FULL /ENTRY:mainCRTStartup /OUT:test.exe ..\obj\output.obj
popd

endlocal
