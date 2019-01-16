/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_H
#define LINUX_SPI_FPC1020_H


#define DEBUG
//#define GPIO_POWER
#define MTK_PLATFORM

#ifdef DEBUG
#define LOG_FUNCTION_ENTRY()                {printk("[fp][CT] %s +++ \n", __func__);}
#define LOG_FUNCTION_EXIT()                 {printk("[fp][CT] %s --- \n", __func__);}
#define LOG_FUNCTION_RETURN_WITH_RET( ... ) {printk("[fp][CT] %s --- return %d \n", __func__, __VA_ARGS__);}
#else
#define LOG_FUNCTION_ENTRY()                {do {} while(0);}
#define LOG_FUNCTION_EXIT()                 {do {} while(0);}
#define LOG_FUNCTION_RETURN_WITH_RET( ... ) {do {} while(0);}
#endif

struct fpc1020_platform_data {
	u32 irq_gpio;
	u32 reset_gpio;
	u32 cs_gpio;
	int external_supply_mv;
	int txout_boost;
	int force_hwid;
	int use_regulator_for_bezel;
	int use_fpc2050;
};

typedef enum {
	FPC1020_MODE_IDLE			= 0,
	FPC1020_MODE_WAIT_AND_CAPTURE		= 1,
	FPC1020_MODE_SINGLE_CAPTURE		= 2,
	FPC1020_MODE_CHECKERBOARD_TEST_NORM	= 3,
	FPC1020_MODE_CHECKERBOARD_TEST_INV	= 4,
	FPC1020_MODE_BOARD_TEST_ONE		= 5,
	FPC1020_MODE_BOARD_TEST_ZERO		= 6,
	FPC1020_MODE_WAIT_FINGER_DOWN		= 7,
	FPC1020_MODE_WAIT_FINGER_UP		= 8,
	FPC1020_MODE_SINGLE_CAPTURE_CAL		= 9,
	FPC1020_MODE_CAPTURE_AND_WAIT_FINGER_UP	= 10,
} fpc1020_capture_mode_t;

typedef enum {
	FPC1020_CHIP_NONE  = 0,
	FPC1020_CHIP_1020A = 1,
	FPC1020_CHIP_1021A = 2,
	FPC1020_CHIP_1021B = 3,
	FPC1020_CHIP_1150A = 4,
	FPC1020_CHIP_1150B = 5,
	FPC1020_CHIP_1150F = 6,
	FPC1020_CHIP_1155X = 7,
	FPC1020_CHIP_1140A = 8,
	FPC1020_CHIP_1145X = 9,
	FPC1020_CHIP_1140B = 10,
	FPC1020_CHIP_1025X = 11,
	FPC1020_CHIP_1022A = 12,
	FPC1020_CHIP_1035A = 13,
	FPC1020_CHIP_1022B = 14,
	FPC1020_CHIP_1035B = 15,
} fpc1020_chip_t;

#ifdef GPIO_POWER
typedef enum {
	FPC1020_POWER_STATE_OFF = 0,
	FPC1020_POWER_STATE_ON  = 1,
} fpc_power_state_t;
#endif

#endif
