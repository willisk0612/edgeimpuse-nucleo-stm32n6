# example-standalone-st-stm32n6
Example standalone for NUCLEO-N657X0-Q

Aton version is: atonn-v1.1.1-14-ge619e860

## Prerequisites

Flashing the device requires STM32 Programmer installed min version 2.18, download from https://www.st.com/en/development-tools/stm32cubeprog.html
STM32Programmer should be added to your path.

## How to run

### Flash the board
Make sure the switch BOOT1 is on the right!
Launch the script for your os, argument can be:
- all : flash weights and firmware
- firmware : flash the firmware
- weights : flash weights (network_data.hex)
- bootloader : flash the fsbl (ai_fsbl_cut_2_0.hex)

If no argument is provided, 'all' is the default one, which flash the weights and then the firmware.

After flashing is finished, switch BOOT1 to left.
BOOT0 should be left to left postion.

NOTE: The bootloader should be flashed just once!

### Run GUI
`python gui/gui_handwriting.py`

## Update your model

### Edge Impulse: Model running on NPU
Deploy your model as ST Neural-ART library.
Copy model.c and model_data.hex into the Model folder.
Copy model-paramets and edge-impulse-sdk folders into edgeimpulse.

### Model running on MCU
Deploy your model as C++ library, copy the extracted folder into the edgeimpulse folder.

NOTE: If you are testing a model running on MCU, you don't need to flash the weights.

