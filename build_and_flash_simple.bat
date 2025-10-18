@echo off
cd /d "C:\Users\congh\Downloads\Compressed\xiaozhi-esp32-2.0.3otto2\xiaozhi-esp32-2.0.3"
call "C:\Users\congh\esp\v5.5\esp-idf\export.bat"
echo Building Otto Robot firmware...
idf.py build
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Flashing...
    idf.py flash monitor
) else (
    echo Build failed!
    pause
)
