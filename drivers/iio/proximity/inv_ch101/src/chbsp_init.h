/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef CHBSPINIT_H_
#define CHBSPINIT_H_

#include "system.h"
#include "chirp_board_config.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define CHIRP_RST	1
#define CHIRP_RST_PS	2

#define CHIRP0_PROG_0	10
#define CHIRP0_PROG_1	11
#define CHIRP0_PROG_2	12
#define CHIRP1_PROG_0	13
#define CHIRP1_PROG_1	14
#define CHIRP1_PROG_2	15
#define CHIRP2_PROG_0	16
#define CHIRP2_PROG_1	17
#define CHIRP2_PROG_2	18

#define CHIRP0_INT_0	20
#define CHIRP0_INT_1	21
#define CHIRP0_INT_2	22
#define CHIRP1_INT_0	23
#define CHIRP1_INT_1	24
#define CHIRP1_INT_2	25
#define CHIRP2_INT_0	26
#define CHIRP2_INT_1	27
#define CHIRP2_INT_2	28

/* Standard symbols used in BSP - use values from config header */
#define CHBSP_MAX_DEVICES	CHIRP_MAX_NUM_SENSORS
#define CHBSP_NUM_I2C_BUSES	CHIRP_NUM_I2C_BUSES

/* RTC hardware pulse pin */
#define CHBSP_RTC_CAL_PULSE_PIN		1
/* Length of real-time clock calibration pulse, in milliseconds */
/* Length of pulse applied to sensor INT line during clock cal */
#define CHBSP_RTC_CAL_PULSE_MS		63

/* I2C Address assignments for each possible device */
#define CHIRP_I2C_ADDRS		{45, 43, 44, 52, 53, 54} //, 62, 63, 64}
#define CHIRP_I2C_BUSES         {0, 0, 0, 1, 1, 1} //, 2, 2, 2}

extern u8 chirp_i2c_addrs[CHBSP_MAX_DEVICES];
extern u8 chirp_i2c_buses[CHBSP_MAX_DEVICES];

extern u32 chirp_pin_enabled[CHBSP_MAX_DEVICES];
extern u32 chirp_pin_prog[CHBSP_MAX_DEVICES];
extern u32 chirp_pin_io[CHBSP_MAX_DEVICES];
extern u32 chirp_pin_io_irq[CHBSP_MAX_DEVICES];

#endif /* CHBSPINIT_H_ */
