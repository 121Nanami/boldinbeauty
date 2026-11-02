@echo off
rem Double-click to package the Boldinbeauty tester build for Windows (see docs/windows-build.md).
rem Pass options through, for example:  Package-Windows.bat -ApiUrl https://your-server.example.com
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Package-Windows.ps1" %*
echo.
pause
