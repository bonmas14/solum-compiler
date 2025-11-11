@echo off
setlocal

set "slm=..\bin\slm-r"
set "bin_dir=bin\"
if exist %bin_dir% ( rmdir /s /q %bin_dir% )
mkdir %bin_dir%

for %%f in ("*.slm") do (
    echo File: %%f
    %slm% %%f --output bin\%%~nf %1 > "%%f.json"
)


endlocal
