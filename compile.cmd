@echo off
setlocal

set ROOT=%~dp0

echo Compiling Fidelis...

gcc -I"%ROOT%include" "%ROOT%src\main.c" "%ROOT%src\lexer.c" "%ROOT%src\helper.c" -o "%ROOT%fidelis.exe"

if errorlevel 1 (
    echo Compilation failed.
    exit /b %errorlevel%
)

echo Build succeeded: %ROOT%fidelis.exe
echo Run with: .\fidelis.exe
