@echo off
setlocal
rem Rebuilds the ASI in this directory. Requires Python 3, x64 clang-cl and lld-link on PATH.
where clang-cl >nul 2>nul
if errorlevel 1 (echo ERROR: clang-cl is not on PATH. & exit /b 1)
where lld-link >nul 2>nul
if errorlevel 1 (echo ERROR: lld-link is not on PATH. & exit /b 1)
set "PYTHON="
where py >nul 2>nul
if not errorlevel 1 set "PYTHON=py -3"
if not defined PYTHON (
    where python >nul 2>nul
    if errorlevel 1 (echo ERROR: Python 3 is not on PATH. & exit /b 1)
    set "PYTHON=python"
)
set "ROOT=%~dp0"
set "BUILD=%ROOT%.build"
if not exist "%BUILD%" mkdir "%BUILD%"
if not exist "%BUILD%" exit /b 1
%PYTHON% "%ROOT%tools\make_version_resource.py" "%ROOT%source\Version.h" "%BUILD%\version.res"
if errorlevel 1 exit /b 1
lld-link /lib /def:"%ROOT%source\kernel32.def" /machine:x64 /out:"%BUILD%\kernel32.lib"
if errorlevel 1 exit /b 1
clang-cl /nologo /c /O2 /Oi- /GS- /GR- /Zl /W4 /WX /std:c++17 /clang:-fno-builtin /Fo"%BUILD%\ShutUpAndLetMePlay.obj" "%ROOT%source\ShutUpAndLetMePlay.cpp"
if errorlevel 1 exit /b 1
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib /noimplib /dynamicbase /nxcompat /highentropyva /opt:ref /opt:icf /timestamp:0 /out:"%ROOT%ShutUpAndLetMePlay.asi" /map:"%BUILD%\ShutUpAndLetMePlay.map" "%BUILD%\ShutUpAndLetMePlay.obj" "%BUILD%\kernel32.lib" "%BUILD%\version.res"
if errorlevel 1 exit /b 1
echo Built: "%ROOT%ShutUpAndLetMePlay.asi"
exit /b 0
