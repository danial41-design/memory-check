@echo off
setlocal
pushd "%~dp0"
set "MEMORYROOM_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MEMORYROOM_MSBUILD="
if exist "%MEMORYROOM_VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%MEMORYROOM_VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "MEMORYROOM_MSBUILD=%%I"
)
if not defined MEMORYROOM_MSBUILD (
    echo MSBuild was not found. Open MemoryRoom.sln in Visual Studio with C++ tools.
    popd
    exit /b 1
)
"%MEMORYROOM_MSBUILD%" "MemoryRoom.sln" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
set "MEMORYROOM_RESULT=%ERRORLEVEL%"
popd
exit /b %MEMORYROOM_RESULT%
