@echo off
cd /d "%~dp0"
swift build --build-system native --product cmplayer %*
exit /b %errorlevel%
