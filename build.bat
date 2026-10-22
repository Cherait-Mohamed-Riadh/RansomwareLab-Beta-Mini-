@echo off
echo Compiling Ransomware Lab...

cd /d "%~dp0"

set "MSYS2_PATH=%~dp0build_deps\openssl\mingw64"

g++ main.cpp -o main.exe -std=c++17 ^
    -I"%MSYS2_PATH%\include" ^
    -L"%MSYS2_PATH%\lib" ^
    -lssl -lcrypto -lws2_32 -lcrypt32 -lgdi32 -lbcrypt -static

IF %ERRORLEVEL% EQU 0 (
    echo.
    echo =========================================
    echo [SUCCESS] main.exe has been created!
    echo =========================================
) ELSE (
    echo.
    echo =========================================
    echo [ERROR] Compilation failed. Check the errors above.
    echo =========================================
)

pause