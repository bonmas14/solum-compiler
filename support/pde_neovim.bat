@echo off

if exist temp (
    rmdir /s /q temp
)

mkdir temp
pushd temp

git clone https://github.com/wbthomason/packer.nvim %LOCALAPPDATA%\nvim-data\site\pack\packer\start\packer.nvim
git clone https://github.com/bonmas14/dotfiles.git

pushd dotfiles
call install_nvim.bat
nvim --headless -c "luafile %LOCALAPPDATA%\nvim\lua\bonmas\packer.lua" -c "autocmd User PackerComplete quitall" -c "PackerSync"
popd

popd temp
rmdir /s /q temp