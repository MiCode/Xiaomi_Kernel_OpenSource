/* include/linux/cm3217.h
 *
 * Copyright (C) 2011 Capella Microsystems Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 * Author: Frank Hsieh <pengyueh@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_CM3217_H
#define __LINUX_CM3217_H

#define CM3217_I2C_NAME			"cm3217"

#define	ALS_W_CMD1_addr			(0x20 >> 1)
#define	ALS_W_CMD2_addr			(0x22 >> 1)
#define	ALS_R_MSB_addr			(0x21 >> 1)
#define	ALS_R_LSB_addr			(0x23 >> 1)

#define ALS_CALIBRATED			0x6E93

/* cm3217 */

/* for ALS command 20h */
#define CM3217_ALS_BIT5_Default_1	(1 << 5)
#define CM3217_ALS_IT_HALF_T		(0 << 2)
#define CM3217_ALS_IT_1_T		(1 << 2)
#define CM3217_ALS_IT_2_T		(2 << 2)
#define CM3217_ALS_IT_4_T		(4 << 2)
#define CM3217_ALS_WDM_DEFAULT_1	(1 << 1)
#define CM3217_ALS_SD			(1 << 0)

/* for ALS command 22h */
#define CM3217_ALS_IT_800ms		(0 << 5)
#define CM3217_ALS_IT_400ms		(1 << 5)
#define CM3217_ALS_IT_266ms		(2 << 5)
#define CM3217_ALS_IT_200ms		(3 << 5)
#define CM3217_ALS_IT_130ms		(4 << 5)
#define CM3217_ALS_IT_100ms		(5 << 5)
#define CM3217_ALS_IT_80ms		(6 << 5)
#define CM3217_ALS_IT_66ms		(7 << 5)

#define CM3217_NUM_LEVELS	10

struct cm3217_platform_data {
	u32 levels[CM3217_NUM_LEVELS];
	u32 golden_adc;
	int (*power) (int, uint8_t);	/* power to the chip */
	uint16_t ALS_slave_address;
};

#define LS_PWR_ON (1 << 0)

#endif
