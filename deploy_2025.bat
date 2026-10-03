@echo off
pushd "%~dp0"
powershell -ExecutionPolicy Bypass -File ".\deploy_2025.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Deploy 2025 failed!
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo [SUCCESS] Deploy 2025 completed.
pause
popd
