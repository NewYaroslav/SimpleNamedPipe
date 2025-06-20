@echo off
setlocal enabledelayedexpansion

REM Set path to the library source folder
set "SRC=MQL5\Include\SimpleNamedPipe"

REM Check if source exists
if not exist "%SRC%" (
    echo [×] Source folder not found: %SRC%
    goto end
)

echo [*] Scanning for installed MetaTrader terminals...

REM Set base directory where MT5 terminals are located
set "BASE=%APPDATA%\MetaQuotes\Terminal"

REM Iterate over subdirectories (terminals)
for /D %%T in ("%BASE%\*") do (
    set "DST=%%T\MQL5\Include\SimpleNamedPipe"

    REM Check if destination MQL5\Include folder exists
    if exist "%%T\MQL5\Include" (
        echo [→] Installing to: !DST!

        REM Remove existing version
        if exist "!DST!" (
            echo     [!] Removing old version...
            rmdir /s /q "!DST!"
        )

        REM Copy new version
        xcopy /e /i /y "%SRC%" "!DST!" >nul

        if errorlevel 1 (
            echo     [✓] Installed successfully to: !DST!
        ) else (
            echo     [×] Copy failed for: !DST!
        )
    ) else (
        echo [×] Skipping %%T — No MQL5\Include folder
    )
)

:end
echo.
pause
endlocal
