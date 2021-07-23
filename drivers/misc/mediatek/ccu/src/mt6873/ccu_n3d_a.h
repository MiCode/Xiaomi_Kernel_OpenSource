/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
