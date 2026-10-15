@echo off
setlocal
set ROOT=%~dp0
if not defined VSINSTALL set VSINSTALL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%ROOT%"
if not exist bin mkdir bin
rem Stop running copies so their exe can be overwritten (an elevated one needs build.bat run as admin).
taskkill /f /im iliz_resmon.exe >nul 2>&1
taskkill /f /im iliz_resmon.old.exe >nul 2>&1
taskkill /f /im iliz_resmon_config.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul
del /q bin\iliz_resmon.old.exe 2>nul
rem Still locked (elevated instance, build not elevated): move it aside instead.
if exist bin\iliz_resmon.exe 2>nul (call;>>bin\iliz_resmon.exe) || ren bin\iliz_resmon.exe iliz_resmon.old.exe
rem VERSIONINFO (ProductVersion is what iliz_updater compares with the release tag).
rc /nologo /Isrc /foiliz_resmon.res src\app.rc || exit /b 1
rem No CRT: own entry point and memset/memcpy/memcmp in src\nocrt.cpp. /Zl: no default lib names in .obj.
cl /nologo /O1 /Os /GS- /GR- /EHs-c- /Zl /utf-8 /DUNICODE /Febin\iliz_resmon.exe src\*.cpp iliz_resmon.res /link /NODEFAULTLIB /ENTRY:WinMainEntry /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /MANIFEST:NO kernel32.lib user32.lib gdi32.lib shell32.lib advapi32.lib iphlpapi.lib || (del /q *.obj *.res 2>nul & exit /b 1)
del /q *.obj *.res 2>nul
rem Configurator: own sources in config\, shares src\settings.h and the CRT-free entry from src\nocrt.cpp.
rc /nologo /Isrc /foiliz_resmon_config.res config\config.rc || exit /b 1
cl /nologo /O1 /Os /GS- /GR- /EHs-c- /Zl /utf-8 /DUNICODE /Febin\iliz_resmon_config.exe config\config.cpp src\nocrt.cpp iliz_resmon_config.res /link /NODEFAULTLIB /ENTRY:WinMainEntry /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /MANIFEST:NO kernel32.lib user32.lib advapi32.lib || (del /q *.obj *.res 2>nul & exit /b 1)
del /q *.obj *.res 2>nul
rem Runtime companions: updater config, and the updater exe if the sibling project has it.
copy /y iliz_updater.json bin\ >nul
if not defined ILIZ_UPDATER_EXE set ILIZ_UPDATER_EXE=%ROOT%..\iliz_updater\bin\iliz_updater.exe
if exist "%ILIZ_UPDATER_EXE%" copy /y "%ILIZ_UPDATER_EXE%" bin\iliz_updater.exe >nul
echo Built: %ROOT%bin\iliz_resmon.exe, %ROOT%bin\iliz_resmon_config.exe
