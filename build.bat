@echo off
REM ============================================================================
REM KEMGU - Windows CMD build wrapper
REM ============================================================================
REM
REM Kullanim:
REM   build.bat                  - tum hedefi derler (build\kemgu.exe)
REM   build.bat test_tumu        - butun testleri calistirir
REM   build.bat clean
REM
REM MSYS2'yi varsayilan disi bir konuma kurduysaniz:
REM   set MSYS2_ROOT=D:\msys64 && build.bat
REM
REM PowerShell kullanicilari icin build.ps1 var.
REM ============================================================================

setlocal

if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"

set "UCRT64_BIN=%MSYS2_ROOT%\ucrt64\bin"
set "CLANG64_BIN=%MSYS2_ROOT%\clang64\bin"
set "MSYS_BIN=%MSYS2_ROOT%\usr\bin"
set "MINGW_MAKE=%UCRT64_BIN%\mingw32-make.exe"
set "BASH_EXE=%MSYS_BIN%\bash.exe"

if not exist "%MINGW_MAKE%" (
    echo HATA: MSYS2 mingw32-make bulunamadi: %MINGW_MAKE%
    echo.
    echo Cozum:
    echo   1. MSYS2 kurun: https://www.msys2.org/
    echo   2. MSYS2 shell'de:
    echo        pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make ^
    echo                  mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-llvm
    echo   3. Varsayilan disi konuma kurduysaniz:
    echo        set MSYS2_ROOT=D:\msys64
    exit /b 1
)

REM PATH'i sadece bu surec icin gecici set et
REM Clang64 once (ASan), UCRT64 sonra (gcc+make), MSYS son (Bash recipe utils).
set "PATH=%CLANG64_BIN%;%UCRT64_BIN%;%MSYS_BIN%;%PATH%"

REM Makefile recipe'lari Bash sozdizimi kullaniyor. Make default SHELL=cmd.exe;
REM komut satirindan override (forward slash MSYS path donusumu icin guvenli).
set "BASH_SHELL=%BASH_EXE:\=/%"

"%MINGW_MAKE%" "SHELL=%BASH_SHELL%" %*
exit /b %ERRORLEVEL%
