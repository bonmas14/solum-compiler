@echo off

:: Этот скрипт устанавливает файлы языка (syntax, indent), а затем добавляет ссылку в главный файл конфигурации

:: This script copies syntax, indent, lua files for .slm extension.

xcopy /E /y neovim %LOCALAPPDATA%\nvim
