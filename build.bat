@echo off
REM ==========================================
REM JP Edge TTS Build Script for Windows
REM ==========================================

setlocal enabledelayedexpansion

REM Default values
set BUILD_TYPE=Release
set BUILD_DIR=build
set GENERATOR="Visual Studio 17 2022"
set ARCH=x64
set ENABLE_GPU=OFF
set ENABLE_TESTS=ON
set ENABLE_EXAMPLES=ON
set USE_MECAB=ON

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :main
if /i "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if /i "%~1"=="--release" (
    set BUILD_TYPE=Release
    shift
    goto :parse_args
)
if /i "%~1"=="--build-dir" (
    set BUILD_DIR=%~2
    shift
    shift
    goto :parse_args
)
if /i "%~1"=="--generator" (
    set GENERATOR=%~2
    shift
    shift
    goto :parse_args
)
if /i "%~1"=="--arch" (
    set ARCH=%~2
    shift
    shift
    goto :parse_args
)
if /i "%~1"=="--enable-gpu" (
    set ENABLE_GPU=ON
    shift
    goto :parse_args
)
if /i "%~1"=="--disable-tests" (
    set ENABLE_TESTS=OFF
    shift
    goto :parse_args
)
if /i "%~1"=="--disable-examples" (
    set ENABLE_EXAMPLES=OFF
    shift
    goto :parse_args
)
if /i "%~1"=="--disable-mecab" (
    set USE_MECAB=OFF
    shift
    goto :parse_args
)
if /i "%~1"=="--clean" (
    echo [INFO] Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    exit /b 0
)
if /i "%~1"=="--help" (
    goto :show_help
)
if /i "%~1"=="-h" (
    goto :show_help
)
echo [ERROR] Unknown option: %~1
exit /b 1

:show_help
echo JP Edge TTS Build Script for Windows
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   --debug              Build in Debug mode
echo   --release            Build in Release mode (default)
echo   --build-dir DIR      Set build directory (default: build)
echo   --generator GEN      Set CMake generator (default: "Visual Studio 17 2022")
echo   --arch ARCH          Set architecture: x64 or Win32 (default: x64)
echo   --enable-gpu         Enable GPU support
echo   --disable-tests      Disable building tests
echo   --disable-examples   Disable building examples
echo   --disable-mecab      Build without MeCab support
echo   --clean              Clean build directory
echo   --help, -h           Show this help message
echo.
echo Examples:
echo   build.bat                        # Build with defaults
echo   build.bat --debug                # Build in debug mode
echo   build.bat --enable-gpu           # Build with GPU support
echo   build.bat --generator "MinGW Makefiles"  # Build with MinGW
echo.
echo Note: For MeCab support on Windows, download from:
echo   https://taku910.github.io/mecab/
echo.
exit /b 0

:main
echo =========================================
echo JP Edge TTS Build Configuration
echo =========================================
echo Build Type:        %BUILD_TYPE%
echo Build Directory:   %BUILD_DIR%
echo Generator:         %GENERATOR%
echo Architecture:      %ARCH%
echo GPU Support:       %ENABLE_GPU%
echo Build Tests:       %ENABLE_TESTS%
echo Build Examples:    %ENABLE_EXAMPLES%
echo MeCab Support:     %USE_MECAB%
echo =========================================

REM Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH.
    echo Please install CMake from: https://cmake.org/download/
    exit /b 1
)

REM Check CMake version
for /f "tokens=3" %%i in ('cmake --version ^| findstr /i "version"') do set CMAKE_VERSION=%%i
echo [INFO] Found CMake version: %CMAKE_VERSION%

REM Check for Git (for submodules)
where git >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Updating git submodules...
    git submodule update --init --recursive
) else (
    echo [WARNING] Git not found. Cannot update submodules.
)

REM Create build directory
echo [INFO] Creating build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure with CMake
echo [INFO] Configuring with CMake...

REM Set architecture for Visual Studio generator
if "%GENERATOR:~0,13%"=="Visual Studio" (
    set CMAKE_GENERATOR_ARGS=-G %GENERATOR% -A %ARCH%
) else (
    set CMAKE_GENERATOR_ARGS=-G %GENERATOR%
)

cmake .. %CMAKE_GENERATOR_ARGS% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DBUILD_SHARED_LIBS=ON ^
    -DBUILD_TESTS=%ENABLE_TESTS% ^
    -DBUILD_EXAMPLES=%ENABLE_EXAMPLES% ^
    -DUSE_GPU=%ENABLE_GPU% ^
    -DUSE_MECAB=%USE_MECAB% ^
    -DENABLE_PROFILING=OFF

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

REM Build
echo [INFO] Building...
cmake --build . --config %BUILD_TYPE% --parallel

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

REM Run tests if enabled
if "%ENABLE_TESTS%"=="ON" (
    echo [INFO] Running tests...
    ctest --config %BUILD_TYPE% --output-on-failure
    if %errorlevel% neq 0 (
        echo [WARNING] Some tests failed
    )
)

echo =========================================
echo Build completed successfully!
echo =========================================
echo.
echo Binaries are located in: %BUILD_DIR%\bin\%BUILD_TYPE%
echo Libraries are located in: %BUILD_DIR%\lib\%BUILD_TYPE%
echo.
echo To run the CLI tool:
echo   %BUILD_DIR%\bin\%BUILD_TYPE%\jp_tts_cli.exe "こんにちは"
echo.
echo To use as a DLL in your project:
echo   1. Add include directory: %CD%\..\include
echo   2. Link against: %BUILD_DIR%\lib\%BUILD_TYPE%\jp_edge_tts_core.lib
echo   3. Copy DLL to your executable directory: jp_edge_tts_core.dll
echo.
echo For Visual Studio integration:
echo   Open the solution file: %BUILD_DIR%\jp_edge_tts.sln
echo.

endlocal