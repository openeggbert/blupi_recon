@echo off
rem Starts the Free Direct server on TCP port 51325 (README.txt). Extra arguments are
rem passed on, for example: start-server.bat --port 51400
cd /d "%~dp0"
if exist password.txt (
    free-direct-server.exe --password-file password.txt %*
) else (
    free-direct-server.exe %*
)
pause
