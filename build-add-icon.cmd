@echo off
cd /d "%~dp0"
swift build -Xlinker Resources\cmplayer.res %*
exit /b %errorlevel%
