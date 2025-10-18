@echo off
cd /d "C:\Users\congh\Downloads\Compressed\xiaozhi-esp32-2.0.3otto2\xiaozhi-esp32-2.0.3"
call "C:\Users\congh\esp\v5.5\esp-idf\export.bat"
idf.py -B build_otto flash monitor
