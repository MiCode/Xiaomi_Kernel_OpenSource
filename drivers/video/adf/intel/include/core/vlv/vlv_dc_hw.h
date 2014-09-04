/*
 * Copyright (C) 2014, Intel Corporation.
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

#ifndef VLV_DC_HW_H
#define VLV_DC_HW_H

struct hw_context_entry {
	u32 reg;
	u32 value;
};

enum {
	PLANE_A = 0,
	PLANE_B,
	N_PRI_PLANE,
};

#define BIT31   0x80000000
#define BIT30   0x40000000
#define BIT29   0x20000000
#define BIT28   0x10000000
#define BIT27   0x08000000
#define BIT26   0x04000000
#define BIT25   0x02000000
#define BIT24   0x01000000
#define BIT23   0x00800000
#define BIT22   0x00400000
#define BIT21   0x00200000
#define BIT20   0x00100000
#define BIT19   0x00080000
#define BIT18   0x00040000
#define BIT17   0x00020000
#define BIT16   0x00010000
#define BIT15   0x00008000
#define BIT14   0x00004000
#define BIT13   0x00002000
#define BIT12   0x00001000
#define BIT11   0x00000800
#define BIT10   0x00000400
#define BIT9    0x00000200
#define BIT8    0x00000100
#define BIT7    0x00000080
#define BIT6    0x00000040
#define BIT5    0x00000020
#define BIT4    0x00000010
#define BIT3    0x00000008
#define BIT2    0x00000004
#define BIT1    0x00000002
#define BIT0    0x00000001

enum pipestat_reg {
	PIPE_HBLANK_STAT = BIT0,
	FRAMESTART_STAT = BIT1,
	START_OF_VBLANK_STAT = BIT2,
	PIPE_PSR_STAT = BIT6,
	DPST_EVENT_STAT = BIT7,
	VSYNC_STAT = BIT9,
	PLANE_FLIP_DONE_STAT = BIT10,
	SPRITE1_FLIP_DONE_STAT = BIT14,
	SPRITE2_FLIP_DONE_STAT = BIT15,
	PIPE_HBLANK_EN = BIT16,
	FRAMESTART_EN = BIT17,
	START_OF_VBLANK_EN = BIT18,
	SPRITE1_FLIP_DONE_EN = BIT22,
	DPST_EVENT_EN = BIT23,
	VSYNC_EN = BIT25,
	PLANE_FLIP_DONE_EN = BIT26,
	SPRITE2_FLIP_DONE_EN = BIT30,
	FIFO_UNDERRUN_STAT = BIT31,
};

#endif
