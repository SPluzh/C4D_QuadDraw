@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy_2026.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Deployment failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
