@echo off
rem Builds iliz_resmon.exe and packs dist\iliz_resmon-<ver>.zip + dist\SHA256SUMS for a GitHub release.
rem The zip holds iliz_resmon.exe, iliz_resmon_config.exe, iliz_updater.exe, iliz_updater.json, LICENSE. Never iliz_resmon.ini (user settings, also in "preserve").
rem iliz_updater.exe is taken from ..\iliz_updater\bin (override with ILIZ_UPDATER_EXE).
setlocal EnableDelayedExpansion
set ROOT=%~dp0
if not defined ILIZ_UPDATER_EXE set ILIZ_UPDATER_EXE=%ROOT%..\iliz_updater\bin\iliz_updater.exe
if not exist "%ILIZ_UPDATER_EXE%" (
  echo Missing iliz_updater.exe: %ILIZ_UPDATER_EXE%
  exit /b 1
)

call "%ROOT%build.bat" norun || exit /b 1

set VER=
for /f "tokens=3" %%a in ('findstr /C:"#define APP_VERSION_A" "%ROOT%src\version.h"') do set VER=%%~a
if "!VER!"=="" (
  echo Could not read APP_VERSION_A from src\version.h
  exit /b 1
)

set OUT=%ROOT%dist
set NAME=iliz_resmon-!VER!
set STAGE=%OUT%\%NAME%
set ZIP=%OUT%\%NAME%.zip
if not exist "%OUT%" mkdir "%OUT%"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
if exist "%ZIP%" del /q "%ZIP%"
mkdir "%STAGE%"
copy /y "%ROOT%bin\iliz_resmon.exe" "%STAGE%\" >nul || exit /b 1
copy /y "%ROOT%bin\iliz_resmon_config.exe" "%STAGE%\" >nul || exit /b 1
copy /y "%ILIZ_UPDATER_EXE%" "%STAGE%\iliz_updater.exe" >nul || exit /b 1
copy /y "%ROOT%bin\iliz_updater.json" "%STAGE%\" >nul || exit /b 1
copy /y "%ROOT%LICENSE" "%STAGE%\" >nul || exit /b 1

powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -Force" || exit /b 1
rmdir /s /q "%STAGE%"
powershell -NoProfile -Command "$h=(Get-FileHash -Algorithm SHA256 '%ZIP%').Hash.ToLower(); [IO.File]::WriteAllText('%OUT%\SHA256SUMS', $h + '  %NAME%.zip' + [char]10)" || exit /b 1

echo Packed: %ZIP%
echo Sums:   %OUT%\SHA256SUMS
echo Release: gh release create v!VER! "%ZIP%" "%OUT%\SHA256SUMS" --title "v!VER!" --notes "..."
