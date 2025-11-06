@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
setlocal
REM go to the folder where this bat script is located
cd /d %~dp0

REM Find the directory where STM32_Programmer_CLI is located
for /f "delims=" %%i in ('where STM32_Programmer_CLI') do set PROGRAMMER_PATH=%%i
for %%i in ("%PROGRAMMER_PATH%") do set PROGRAMMER_DIR=%%~dpi

set FLASH_FIRMWARE=firmware
set FLASH_WEIGHTS=weights
set FLASH_BOOTLOADER=bootloader
set FLASH_ERASE=erase
set FLASHER=STM32_Programmer_CLI
set EL=%PROGRAMMER_DIR%ExternalLoader\MX25UM51245G_STM32N6570-NUCLEO.stldr

set TARGET=%1

if not defined TARGET SET TARGET=all

if "%TARGET%" NEQ "firmware" if "%TARGET%" NEQ "weights" if "%TARGET%" NEQ "bootloader" if "%TARGET%" NEQ "all" if "%TARGET%" NEQ "erase" goto INVALIDTARGET

echo Flashing %TARGET%

IF "%TARGET%" == "%FLASH_WEIGHTS%" goto :FLASH_W
IF "%TARGET%" == "%FLASH_BOOTLOADER%" goto :FLASH_B
IF "%TARGET%" == "%FLASH_FIRMWARE%" goto :FLASH_F
IF "%TARGET%" == "%FLASH_ERASE%" goto :FLASH_E

%FLASHER% -c port=SWD mode=HOTPLUG ap=1 -el %EL% -hardRst -w Model\network_data.hex
%FLASHER% -c port=SWD mode=HOTPLUG ap=1 -el %EL% -hardRst -w build\Project.bin 0x70080000

goto :COMMON_EXIT

:FLASH_F
    %FLASHER% -c port=SWD mode=HOTPLUG ap=1 -el %EL% -hardRst -w build\Project.bin 0x70080000
goto :COMMON_EXIT

:FLASH_W
    %FLASHER% -c port=SWD mode=HOTPLUG ap=1 -el %EL% -hardRst -w Model\network_data.hex
goto :COMMON_EXIT

:FLASH_B
    %FLASHER% -c port=SWD mode=HOTPLUG ap=1 -el %EL% -hardRst -w ai_fsbl_cut_2_0.hex
goto :COMMON_EXIT

REM erase bootloader sectors
:FLASH_E
    %FLASHER% -c port=SWD mode=HOTPLUG ap=1 -el %EL% --erase [0 7] -hardRst
goto :COMMON_EXIT

:INVALIDTARGET

echo %TARGET% is an invalid target!
exit \b 1


:COMMON_EXIT
