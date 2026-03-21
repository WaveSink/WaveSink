@echo off

set EXE=.\build\Release\AudioMan.exe
set COMPILE_SCRIPT=.\compile.bat

call "%COMPILE_SCRIPT%"

start "" "%EXE%"
