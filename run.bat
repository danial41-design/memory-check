@echo off
setlocal
pushd "%~dp0"
if not exist "bin\Debug\MemoryRoom.exe" (
    call build.bat
    if errorlevel 1 (
        popd
        exit /b 1
    )
)
"bin\Debug\MemoryRoom.exe"
set "MEMORYROOM_RESULT=%ERRORLEVEL%"
popd
exit /b %MEMORYROOM_RESULT%
