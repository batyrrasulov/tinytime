@echo off
REM ============================================================
REM TinyTime FPGA — One-Click Build Script (Windows Lab PC)
REM
REM This script does EVERYTHING:
REM   1. Copies custom RTL + IP into the Quartus project
REM   2. Runs the Qsys modification script
REM   3. Regenerates Computer_System HDL
REM   4. Replaces the top-level Verilog
REM   5. Compiles the full Quartus project
REM   6. Generates the .rbf file for flashing
REM
REM Usage:
REM   build_all.bat C:\path\to\DE10_Standard_Computer\verilog
REM
REM Output:
REM   DE10_Standard_Computer.rbf in the project directory
REM ============================================================

echo.
echo ==========================================
echo  TinyTime FPGA — Full Build
echo ==========================================
echo.

REM --- Get the project path ---
if "%~1"=="" (
    echo Drag-and-drop the DE10_Standard_Computer\verilog folder
    echo onto this script, or type the path:
    echo.
    set /p GHRD_PATH="Path to DE10_Standard_Computer\verilog: "
) else (
    set GHRD_PATH=%~1
)

REM --- Verify the path ---
if not exist "%GHRD_PATH%\DE10_Standard_Computer.qpf" (
    echo.
    echo ERROR: Could not find DE10_Standard_Computer.qpf in:
    echo   %GHRD_PATH%
    echo Make sure you point to the "verilog" folder.
    pause
    exit /b 1
)

echo Found Quartus project at: %GHRD_PATH%
echo.

REM --- Get our script's directory ---
set SCRIPT_DIR=%~dp0

REM ============================================================
REM STEP 1: Copy RTL and IP files
REM ============================================================
echo [1/6] Copying custom RTL and IP files...
if not exist "%GHRD_PATH%\ip\tinytime" mkdir "%GHRD_PATH%\ip\tinytime"

copy /Y "%SCRIPT_DIR%rtl\bcd_to_7seg.v"           "%GHRD_PATH%\ip\tinytime\"
copy /Y "%SCRIPT_DIR%rtl\hex_decoder.v"            "%GHRD_PATH%\ip\tinytime\"
copy /Y "%SCRIPT_DIR%rtl\key_debouncer.v"          "%GHRD_PATH%\ip\tinytime\"
copy /Y "%SCRIPT_DIR%ip\hex_decoder_hw.tcl"        "%GHRD_PATH%\ip\tinytime\"
copy /Y "%SCRIPT_DIR%ip\key_debouncer_hw.tcl"      "%GHRD_PATH%\ip\tinytime\"
echo    Done.
echo.

REM ============================================================
REM STEP 2: Backup original files
REM ============================================================
echo [2/6] Backing up original files...
if not exist "%GHRD_PATH%\backup" mkdir "%GHRD_PATH%\backup"
if not exist "%GHRD_PATH%\backup\DE10_Standard_Computer.v" (
    copy /Y "%GHRD_PATH%\DE10_Standard_Computer.v" "%GHRD_PATH%\backup\"
)
if not exist "%GHRD_PATH%\backup\Computer_System.qsys" (
    copy /Y "%GHRD_PATH%\Computer_System.qsys" "%GHRD_PATH%\backup\"
)
echo    Done.
echo.

REM ============================================================
REM STEP 3: Copy the Qsys modification script
REM ============================================================
echo [3/6] Modifying Platform Designer system...
copy /Y "%SCRIPT_DIR%quartus\modify_qsys.tcl" "%GHRD_PATH%\ip\tinytime\"

REM Run the Qsys modification script
pushd "%GHRD_PATH%"
qsys-script --search-path="ip/tinytime,$$" --script=ip/tinytime/modify_qsys.tcl
if errorlevel 1 (
    echo.
    echo ERROR: qsys-script failed!
    echo.
    echo MANUAL FALLBACK: Open Platform Designer manually and follow
    echo the steps in LAB_PC_STEPS.txt.
    echo.
    echo Restoring backup...
    copy /Y "backup\Computer_System.qsys" "Computer_System.qsys"
    popd
    pause
    exit /b 1
)
echo    Done.
echo.

REM ============================================================
REM STEP 4: Regenerate Computer_System HDL
REM ============================================================
echo [4/6] Regenerating Computer_System HDL (this takes a few minutes)...
qsys-generate Computer_System.qsys --synthesis=VERILOG
if errorlevel 1 (
    echo.
    echo ERROR: qsys-generate failed!
    echo Try opening Platform Designer manually.
    popd
    pause
    exit /b 1
)
echo    Done.
echo.

REM ============================================================
REM STEP 5: Replace top-level Verilog
REM ============================================================
echo [5/6] Replacing top-level Verilog with modified version...
copy /Y "%SCRIPT_DIR%quartus\DE10_Standard_Computer_modified.v" "%GHRD_PATH%\DE10_Standard_Computer.v"
echo    Done.
echo.

REM ============================================================
REM STEP 6: Full Quartus compilation
REM ============================================================
echo [6/6] Starting Quartus compilation (10-30 minutes)...
echo    This will take a while. Go get coffee.
echo.
quartus_sh --flow compile DE10_Standard_Computer
if errorlevel 1 (
    echo.
    echo ERROR: Quartus compilation failed!
    echo Check the output above for error messages.
    echo Common fix: open Quartus GUI, check the error details.
    popd
    pause
    exit /b 1
)
echo    Done.
echo.

REM ============================================================
REM STEP 7: Generate .rbf
REM ============================================================
echo Generating .rbf file...
quartus_cpf -c output_files/DE10_Standard_Computer.sof DE10_Standard_Computer.rbf
if errorlevel 1 (
    echo.
    echo WARNING: Could not auto-generate .rbf
    echo Do it manually: File ^> Convert Programming Files in Quartus
) else (
    echo    Done: DE10_Standard_Computer.rbf
)
popd
echo.

echo ==========================================
echo  BUILD COMPLETE!
echo ==========================================
echo.
echo The .rbf file is at:
echo   %GHRD_PATH%\DE10_Standard_Computer.rbf
echo.
echo To flash it to the board:
echo   1. Copy .rbf to USB drive
echo   2. SSH into the board: ssh root@192.168.1.123
echo   3. Copy: scp DE10_Standard_Computer.rbf root@192.168.1.123:/root/
echo   4. Flash:
echo      cp /root/DE10_Standard_Computer.rbf /media/fat_partition/soc_system.rbf
echo      reboot
echo.
echo Then run the app:
echo   ./tinytime --custom-fpga --use-class-addrs
echo.
pause
