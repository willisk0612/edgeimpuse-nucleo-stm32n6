/**
 ******************************************************************************
 * @file    app_config.h
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
#ifndef APP_CONFIG
#define APP_CONFIG

#define USE_DCACHE

#define VDDCORE_OVERDRIVE               1               // Use Overdrive mode (quick clocks) or not (normal clocks)

#define USE_UART_BAUDRATE               9600
#define USE_EXTERNAL_MEMORY_DEVICES     1

#define USE_NPU_CACHE           // Used to open RISAFs for the NPU cache

#endif
