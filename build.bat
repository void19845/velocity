@echo off
rem Compile VELOCITY avec MSVC (trouve automatiquement via vswhere)
cd /d "%~dp0"
where cl >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vspath.txt"
set /p VSPATH=<"%TEMP%\vspath.txt"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
:build
rem VELOCITY_BUILD : numero de build donne par la CI de release (bouton MISE A JOUR) ; absent = build 0 (dev)
set "VBUILD="
if defined VELOCITY_BUILD set "VBUILD=/DVELOCITY_BUILD=%VELOCITY_BUILD%"
cl /nologo /O2 /W3 /std:c11 %VBUILD% /Fe:velocity.exe main.c /link /SUBSYSTEM:WINDOWS
