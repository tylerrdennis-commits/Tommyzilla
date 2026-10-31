@echo off
rem Build Tommyzilla with Visual Studio 2022 (MSVC, x64). Run from a normal or "Developer" command prompt.
rem Requires: Visual Studio 2022 with "Desktop development with C++" (includes CMake and the Windows SDK).
setlocal
cd /d "%~dp0"
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail
cmake --build build --config %CONFIG% --parallel
if errorlevel 1 goto :fail
if not exist dist mkdir dist
copy /Y "build\%CONFIG%\Tommyzilla.exe" dist\ >nul
xcopy /E /I /Y data dist\data >nul
rem models mapped in data\models.json (+ textures, asset licences) -> dist\assets
if exist dist\assets rmdir /S /Q dist\assets
cmake -DSRC_DIR="%CD%" -DDEST="%CD%\dist" -P tools\copy_models.cmake
if errorlevel 1 goto :fail
rem Also put the exe in the project root (next to data\) so double-clicking Tommyzilla.exe there just works.
copy /Y "build\%CONFIG%\Tommyzilla.exe" Tommyzilla.exe >nul
echo.
echo Built dist\Tommyzilla.exe and .\Tommyzilla.exe  (%CONFIG%)
echo Debug builds (build_windows.bat Debug) enable the D3D12 debug layer.
goto :eof
:fail
echo Build failed.
exit /b 1
