@echo off
echo Building CLI version...
clang++ cli\main.cpp core\config.cpp core\cursor.cpp core\scheduler.cpp core\utils.cpp core\autofill.cpp -o curator_cli.exe -I deps -ladvapi32 -municode -mwindows

if %ERRORLEVEL% equ 0 (
    echo CLI Build successful: curator_cli.exe
) else (
    echo CLI Build failed.
)

echo Building GUI version...
windres gui\app.rc -o gui\app_res.o
clang++ gui\main.cpp gui\app_res.o core\config.cpp core\cursor.cpp core\scheduler.cpp core\utils.cpp core\autofill.cpp -o curator_gui.exe -I deps -ladvapi32 -lcomctl32 -lole32 -luuid -municode -mwindows

if %ERRORLEVEL% equ 0 (
    echo GUI Build successful: curator_gui.exe
) else (
    echo GUI Build failed.
)
