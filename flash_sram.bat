@echo off
setlocal
REM Programs SRAM on NUCLEO board

set PROG=STM32_Programmer_CLI.exe

set FILE="./build/Project.elf"

%PROG% -c port=SWD mode=Normal reset=SWrst -d %FILE% -g 0x34000000

endlocal
