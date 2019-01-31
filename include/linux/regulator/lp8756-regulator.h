/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Hsin-Hsiung.Wang <hsin-hsiung.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LP8756_REGISTERS_H__
#define __LP8756_REGISTERS_H__

/* Registers */
#define LP8756_DEV_REV			0x00
#define LP8756_BUCK0_VOUT		0x0A
#define LP8756_BUCK2_VOUT		0x0E

/*
 * Registers bits
 */
/* DEV_REV (addr=0x00) */
#define LP8756_ALL_LAYER_MASK			0x3
#define LP8756_ALL_LAYER_SHIFT			4

/* Voltage Related */
#define LP8756_MIN_MV			500
#define LP8756_RANGE0_MAX_MV		730
#define LP8756_RANGE1_MAX_MV		1400
#define LP8756_MAX_MV			3360
#define LP8756_RANGE0_STEP_MV		10
#define LP8756_RANGE1_STEP_MV		5
#define LP8756_RANGE2_STEP_MV		20

#define LP8756_RAMP_DELAY	10
#define LP8756_TURN_ON_DELAY	70
#endif				/* __LP8756_REGISTERS_H__ */
