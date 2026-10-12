@echo off
cd /d "%~dp0"
swift build --build-system native %*
exit /b %errorlevel%
