/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _WALLCLK_H
#define _WALLCLK_H

/* wallclock register offset */
#define CTRL_REG_OFFSET			0x0
#define PULSE_CNT_REG_OFFSET		0x4
#define CLK_CNT_SNAPSHOT_REG_OFFSET	0x8
#define CLK_CNT_REG_OFFSET		0xC
#define CLK_BASE_TIME0_OFFSET		0x24
#define CLK_BASE_TIME1_OFFSET		0x28

/* ctrl register bitmap */
#define CTRL_TIME_SRC_POS	0
#define CTRL_TIME_SRC_MASK	0x0000000F
#define CTRL_SW_BITS_POS	4
#define CTRL_SW_BITS_MASK	0x7FFFFFF0
#define CTRL_ENA_DIS_POS	31
#define CTRL_ENABLE_MASK	0x80000000

/* clock rate from time source */
#define CLK_RATE		122880000	/* 122.88 Mhz */
#define PPNS_PULSE		2		/* PP2S */

#define MAX_SFN			1023
#define SFN_PER_SECOND		100

extern int wallclk_set_sfn(u16 sfn);
extern int wallclk_get_sfn(void);
extern int wallclk_set_sfn_ref(u16 sfn);
extern int wallclk_get_sfn_ref(void);
extern int wallclk_reg_read(u32 offset, u32 *p);
extern int wallclk_reg_write(u32 offset, u32 value);

#endif /* _WALLCLK_H */
