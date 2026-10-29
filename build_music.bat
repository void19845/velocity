@echo off
rem Compile musique.exe (ecoute du design musical, music.h) avec MSVC
cd /d "%~dp0"
where cl >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vspath.txt"
set /p VSPATH=<"%TEMP%\vspath.txt"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
:build
cl /nologo /O2 /W3 /std:c11 /Fe:musique.exe musique.c
