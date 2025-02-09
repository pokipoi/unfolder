@echo off
if exist build rmdir /s /q build
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

copy /Y Release\unfolder.exe ..\unfolder.exe
cd ..