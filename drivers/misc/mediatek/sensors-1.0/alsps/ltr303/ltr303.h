/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _LTR303_H_
#define _LTR303_H_

#include <linux/ioctl.h>

/*LTR303 als sensor register related macro*/
#define LTR303_ALS_CONTR		0x80
#define LTR303_ALS_MEAS_RATE	0x85
#define LTR303_PART_ID	        0x86
#define LTR303_MANUFACTURER_ID	0x87
#define LTR303_INTERRUPT		0x8F
#define LTR303_ALS_THRES_UP_0	0x97
#define LTR303_ALS_THRES_UP_1	0x98
#define LTR303_ALS_THRES_LOW_0	0x99
#define LTR303_ALS_THRES_LOW_1	0x9A
#define LTR303_INTERRUPT_PERSIST 0x9E

/* 303's Read Only Registers */
#define LTR303_ALS_DATA_CH1_0	0x88
#define LTR303_ALS_DATA_CH1_1	0x89
#define LTR303_ALS_DATA_CH0_0	0x8A
#define LTR303_ALS_DATA_CH0_1	0x8B
#define LTR303_ALS_STATUS		0x8C

/* Basic Operating Modes */
#define MODE_ON_Reset			0x02  /*for als reset*/

#define MODE_ALS_Range1			0x00  /*for als gain x1*/
#define MODE_ALS_Range2			0x04  /*for als  gain x2*/
#define MODE_ALS_Range3			0x08  /*for als  gain x4*/
#define MODE_ALS_Range4			0x0C  /*for als gain x8*/
#define MODE_ALS_Range5			0x18  /*for als gain x48*/
#define MODE_ALS_Range6			0x1C  /*for als gain x96*/

#define MODE_ALS_StdBy			0x00

#define ALS_RANGE_64K			1
#define ALS_RANGE_32K			2
#define ALS_RANGE_16K			4
#define ALS_RANGE_8K			8
#define ALS_RANGE_1300			48
#define ALS_RANGE_600			96

/* Power On response time in ms */
#define PON_DELAY		600
#define WAKEUP_DELAY	10
#endif
