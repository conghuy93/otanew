@echo off
cd /d "C:\Users\congh\Downloads\Compressed\xiaozhi-esp32-2.0.3otto2\xiaozhi-esp32-2.0.3"
call "C:\Espressif\tools\export_paths.bat"
call "C:\Users\congh\esp\v5.5\esp-idf\export.bat"
idf.py -B build_otto build
pause