@echo off
setlocal

REM 获取当前批处理文件所在的目录
set "current_dir=%~dp0"

REM 设置注册表项，支持多选
reg add "HKCR\Directory\shell\Unfolder" /ve /d "Unfolder" /f
reg add "HKCR\Directory\shell\Unfolder" /v "Icon" /d "%current_dir%unfolder.exe" /f
reg add "HKCR\Directory\shell\Unfolder" /v "MultiSelectModel" /d "Player" /f
reg add "HKCR\Directory\shell\Unfolder\command" /ve /d "\"%current_dir%unfolder.exe\" %%*" /f

echo Registry key added successfully.
pause