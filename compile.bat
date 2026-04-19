@echo off
setlocal

set QT_PATH=Qt\6.10.3\msvc2022_64
set PROJECT_NAME=WaveSink
set BUILD_DIR=build
set BUILD_NINJA=build-ninja
set EXE_PATH=%BUILD_DIR%\Release\%PROJECT_NAME%.exe

if "%~1"=="clean" (
    rmdir /s /Q build >nul
)

echo "[1/4] Configuring CMake"
cmake -B "%BUILD_DIR%" -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH="%QT_PATH%"

if %ERRORLEVEL% neq 0 (
    echo "FAILED"
    exit 1
)

echo "[2/4] Generating compilation database"
cmake -S . -B "%BUILD_NINJA%" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH="%QT_PATH%" >nul
copy "%BUILD_NINJA%\compile_commands.json" . >nul
rmdir /s /Q "%BUILD_NINJA%" >nul

if %ERRORLEVEL% neq 0 (
    echo "Couldn't generate compilation database. Skipping.."
)

echo "[3/4] Compiling"
cmake --build "%BUILD_DIR%" --config Release

if %ERRORLEVEL% neq 0 (
    echo "FAILED"
    exit 1
)

echo "[4/4] Deploying"
"%QT_PATH%\bin\windeployqt.exe" "%EXE_PATH%"

if %ERRORLEVEL% neq 0 (
    echo "FAILED"
    exit 1
)
