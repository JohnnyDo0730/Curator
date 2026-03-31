@echo off
echo Building CLI version...
clang++ cli\main.cpp core\config.cpp core\cursor.cpp core\scheduler.cpp core\utils.cpp -o curator_cli.exe -ladvapi32 -municode -mwindows
if %ERRORLEVEL% equ 0 (
    echo Build successful: curator_cli.exe
) else (
    echo Build failed.
)
