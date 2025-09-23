@echo off
REM ===================================================================
REM VS2022 Solution Generator for JP Edge TTS
REM Creates a Visual Studio 2022 solution with proper configuration
REM ===================================================================

echo.
echo ========================================
echo Generating VS2022 Solution for JP Edge TTS
echo ========================================
echo.

REM Check if build directory exists, create if not
if not exist "build_vs2022" (
    echo Creating build_vs2022 directory...
    mkdir build_vs2022
) else (
    echo Build directory already exists, cleaning...
    REM Optional: clean the directory for fresh build
    REM rd /s /q build_vs2022
    REM mkdir build_vs2022
)

REM Navigate to build directory
cd build_vs2022

REM Generate the Visual Studio 2022 solution
REM Using the Visual Studio 17 2022 generator for VS2022
echo Running CMake to generate VS2022 solution...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    ..

REM Check if CMake succeeded
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo ERROR: CMake generation failed!
    echo Please check the error messages above.
    echo ========================================
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo VS2022 Solution generated successfully!
echo Solution file: build_vs2022\jp_edge_tts.sln
echo.
echo You can now:
echo   1. Open the solution in Visual Studio 2022
echo   2. Build using: cmake --build . --config Release
echo   3. Or open VS2022 and build from the IDE
echo ========================================
echo.

REM Optional: Ask if user wants to open the solution
choice /C YN /M "Do you want to open the solution in VS2022 now"
if %ERRORLEVEL% EQU 1 (
    echo Opening solution in Visual Studio 2022...
    start jp_edge_tts.sln
)

cd ..