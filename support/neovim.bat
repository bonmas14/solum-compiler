@echo off

:: Этот скрипт устанавливает файлы языка (syntax, indent), а затем добавляет ссылку в главный файл конфигурации
:: наверное заменю потом чтобы просто копирование делало свое дело

:: This script copies syntax, indent, lua files for .slm extension.
:: And after that it changes init.lua adding require('solum') at start.

xcopy /E /y neovim %LOCALAPPDATA%\nvim
nvim %LOCALAPPDATA%\nvim\init.lua -c "g/require(.solum.)/d" -c "normal ggOrequire('solum')" -c "wq"