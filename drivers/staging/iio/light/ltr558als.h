/* Lite-On LTR-558ALS Linux Driver
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _LTR558_H
#define _LTR558_H


/* LTR-558 Registers */
#define LTR558_ALS_CONTR	0x80
#define LTR558_PS_CONTR		0x81
#define LTR558_PS_LED		0x82
#define LTR558_PS_N_PULSES	0x83
#define LTR558_PS_MEAS_RATE	0x84
#define LTR558_ALS_MEAS_RATE	0x85
#define LTR558_MANUFACTURER_ID	0x87

#define LTR558_INTERRUPT	0x8F
#define LTR558_PS_THRES_UP_0	0x90
#define LTR558_PS_THRES_UP_1	0x91
#define LTR558_PS_THRES_LOW_0	0x92
#define LTR558_PS_THRES_LOW_1	0x93

#define LTR558_ALS_THRES_UP_0	0x97
#define LTR558_ALS_THRES_UP_1	0x98
#define LTR558_ALS_THRES_LOW_0	0x99
#define LTR558_ALS_THRES_LOW_1	0x9A

#define LTR558_INTERRUPT_PERSIST 0x9E

/* 558's Read Only Registers */
#define LTR558_ALS_DATA_CH1_0	0x88
#define LTR558_ALS_DATA_CH1_1	0x89
#define LTR558_ALS_DATA_CH0_0	0x8A
#define LTR558_ALS_DATA_CH0_1	0x8B
#define LTR558_ALS_PS_STATUS	0x8C
#define LTR558_PS_DATA_0	0x8D
#define LTR558_PS_DATA_1	0x8E

/* ALS PS STATUS 0x8C */
#define STATUS_ALS_GAIN_RANGE1	0x10
#define STATUS_ALS_INT_TRIGGER	0x08
#define STATUS_ALS_NEW_DATA		0x04
#define STATUS_PS_INT_TRIGGER	0x02
#define STATUS_PS_NEW_DATA		0x01

/* Basic Operating Modes */
#define MODE_ALS_ON_Range1	0x0B
#define MODE_ALS_ON_Range2	0x03
#define MODE_ALS_StdBy		0x00

#define MODE_PS_ON_Gain1	0x03
#define MODE_PS_ON_Gain4	0x07
#define MODE_PS_ON_Gain8	0x0B
#define MODE_PS_ON_Gain16	0x0F
#define MODE_PS_StdBy		0x00

#define PS_RANGE1 	1
#define PS_RANGE4	2
#define PS_RANGE8 	4
#define PS_RANGE16	8

#define ALS_RANGE1_320	1
#define ALS_RANGE2_64K 	2

/* Power On response time in ms */
#define PON_DELAY	600
#define WAKEUP_DELAY	10

#endif
