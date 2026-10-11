@echo off
setlocal
set ROOT=%~dp0
if not defined VSINSTALL set VSINSTALL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%ROOT%"
cl /nologo /O1 /Os /GS- /GR- /EHs-c- /utf-8 /DUNICODE /Fecputray.exe src\*.cpp /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /MANIFEST:NO kernel32.lib user32.lib gdi32.lib shell32.lib advapi32.lib iphlpapi.lib || (del /q *.obj 2>nul & exit /b 1)
del /q *.obj 2>nul
echo Built: %ROOT%cputray.exe
