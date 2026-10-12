@echo off
cd /d "%~dp0"
swift build --build-system native --product cmplayer -Xlinker Resources\cmplayer.res %*
exit /b %errorlevel%
