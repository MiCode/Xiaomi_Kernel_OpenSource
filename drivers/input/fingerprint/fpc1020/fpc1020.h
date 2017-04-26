/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_H
#define LINUX_SPI_FPC1020_H

struct fpc1020_platform_data {
	int fp_id_gpio;
	int irq_gpio;
	int reset_gpio;
	int cs_gpio;
	int external_supply_mv;
	int txout_boost;
	int force_hwid;
	int use_regulator_for_bezel;
	int use_fpc2050;
	int under_glass;
};

typedef enum {
	FPC1020_MODE_IDLE = 0,
	FPC1020_MODE_WAIT_AND_CAPTURE,
	FPC1020_MODE_SINGLE_CAPTURE,
	FPC1020_MODE_CHECKERBOARD_TEST_NORM,
	FPC1020_MODE_CHECKERBOARD_TEST_INV,
	FPC1020_MODE_BOARD_TEST_ONE,
	FPC1020_MODE_BOARD_TEST_ZERO,
	FPC1020_MODE_WAIT_FINGER_DOWN,
	FPC1020_MODE_WAIT_FINGER_UP,
	FPC1020_MODE_SINGLE_CAPTURE_CAL,
	FPC1020_MODE_CAPTURE_AND_WAIT_FINGER_UP,
} fpc1020_capture_mode_t;

typedef enum {
	FPC1020_CHIP_NONE  = 0,
	FPC1020_CHIP_1020A,
	FPC1020_CHIP_1021A,
	FPC1020_CHIP_1021B,
	FPC1020_CHIP_1021E,
	FPC1020_CHIP_1021F,
	FPC1020_CHIP_1150A,
	FPC1020_CHIP_1150B,
	FPC1020_CHIP_1150F,
	FPC1020_CHIP_1155X,
	FPC1020_CHIP_1140A,
	FPC1020_CHIP_1145X,
	FPC1020_CHIP_1140B,
	FPC1020_CHIP_1140C,
	FPC1020_CHIP_11401,
	FPC1020_CHIP_1025X,
	FPC1020_CHIP_1022X,
	FPC1020_CHIP_1035X,
} fpc1020_chip_t;

#endif

