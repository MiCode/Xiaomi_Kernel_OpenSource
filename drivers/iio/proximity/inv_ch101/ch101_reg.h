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

#ifndef _CH101REG_H_
#define _CH101REG_H_

/* CH-101 common definitions */

#define CH101_COMMON_REG_OPMODE		0x01
#define CH101_COMMON_REG_TICK_INTERVAL	0x02
#define CH101_COMMON_REG_PERIOD		0x05
#define CH101_COMMON_REG_MAX_RANGE	0x07
#define CH101_COMMON_REG_TIME_PLAN	0x09
#define CH101_COMMON_REG_STAT_RANGE	0x12
#define CH101_COMMON_REG_STAT_COEFF	0x13
#define CH101_COMMON_REG_READY		0x14
#define CH101_COMMON_REG_TOF_SF		0x16
#define CH101_COMMON_REG_TOF		0x18
#define CH101_COMMON_REG_AMPLITUDE	0x1A
#define CH101_COMMON_REG_CAL_TRIG	0x06
#define CH101_COMMON_REG_CAL_RESULT	0x0A
#define CH101_COMMON_REG_DATA		0x1C

/* Programming interface register addresses */
#define CH_PROG_REG_PING  0x00	/*!< Read-only register used for ping device.*/
#define CH_PROG_REG_CPU	  0x42	/*!< Processor control register address. */
#define CH_PROG_REG_STAT  0x43	/*!< Processor status register address. */
#define CH_PROG_REG_CTL   0x44	/*!< Data transfer control register address. */
#define CH_PROG_REG_ADDR  0x05	/*!< Data transfer starting register address.*/
#define CH_PROG_REG_CNT   0x07	/*!< Data transfer size register address. */
#define CH_PROG_REG_DATA  0x06	/*!< Data transfer value register address. */

#define CH101_SIG_BYTE_0	(0x0a)	/*  Signature bytes in sensor */
#define CH101_SIG_BYTE_1	(0x02)

#endif /* _CH101REG_H_ */
