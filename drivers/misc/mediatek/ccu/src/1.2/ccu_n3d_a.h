/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __CCU_N3D_A_H__
#define __CCU_N3D_A_H__

enum N3DA_REGS_OFFSET {
	OFFSET_CTL = 0x100,
	OFFSET_TRIG = 0x108,
	OFFSET_INT = 0x10C,
};

extern u32 n3d_a_readw(unsigned long n3d_a_base, u32 offset);
extern void n3d_a_writew(u32 value, unsigned long n3d_a_base, u32 offset);


#endif
