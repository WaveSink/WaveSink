@echo off

set EXE=.\build\Release\WaveSink.exe
set COMPILE_SCRIPT=.\compile.bat

call "%COMPILE_SCRIPT%"

start "" "%EXE%"
