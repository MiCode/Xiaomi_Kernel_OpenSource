/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _MT6885_VPU_REG_H_
#define _MT6885_VPU_REG_H_

#include <sync_write.h>
#include "vpu_cmn.h"

/* Spare Register - Enum */
enum {
	VPU_CMD_DO_EXIT         = 0x00,
	VPU_CMD_DO_LOADER       = 0x01,
	VPU_CMD_DO_DECRYPT      = 0x02,
	VPU_CMD_DO_PASS1_DL     = 0x10,
	VPU_CMD_DO_PASS2_DL     = 0x11,
	VPU_CMD_DO_D2D          = 0x22,
	VPU_CMD_SET_DEBUG       = 0x40,
	VPU_CMD_SET_MPU         = 0x41,
	VPU_CMD_SET_FTRACE_LOG  = 0x42,
	VPU_CMD_GET_SWVER       = 0x81,
	VPU_CMD_GET_ALGO        = 0x82,

	/* Extend for test */
	VPU_CMD_EXT_BUSY        = 0xF0
};

enum {
	VPU_STATE_NOT_READY     = 0x00,
	VPU_STATE_READY         = 0x01,
	VPU_STATE_IDLE          = 0x02,
	VPU_STATE_BUSY          = 0x04,
	VPU_STATE_ERROR         = 0x08,
	VPU_STATE_TERMINATED    = 0x10
};

enum {
	VPU_REQ_DO_CHECK_STATE     = 0x100,
	VPU_REQ_DO_DUMP_LOG        = 0x101,
	VPU_REQ_DO_CLOSED_FILE     = 0x102
};

/* core register offsets */
#define CG_CON		0x100
#define CG_CLR		0x108
#define SW_RST		0x10C
#define DONE_ST		0x90C
#define CTRL		0x910
#define XTENSA_INT	0x200
#define CTL_XTENSA_INT	0x204
#define DEFAULT0	0x93C
#define DEFAULT1	0x940
#define DEFAULT2	0x944
#define XTENSA_INFO00	0x250
#define XTENSA_INFO01	0x254
#define XTENSA_INFO02	0x258
#define XTENSA_INFO03	0x25C
#define XTENSA_INFO04	0x260
#define XTENSA_INFO05	0x264
#define XTENSA_INFO06	0x268
#define XTENSA_INFO07	0x26C
#define XTENSA_INFO08	0x270
#define XTENSA_INFO09	0x274
#define XTENSA_INFO10	0x278
#define XTENSA_INFO11	0x27C
#define XTENSA_INFO12	0x280
#define XTENSA_INFO13	0x284
#define XTENSA_INFO14	0x288
#define XTENSA_INFO15	0x28C
#define XTENSA_INFO16	0x290
#define XTENSA_INFO17	0x294
#define XTENSA_INFO18	0x298
#define XTENSA_INFO19	0x29C
#define XTENSA_INFO20	0x2A0
#define XTENSA_INFO21	0x2A4
#define XTENSA_INFO22	0x2A8
#define XTENSA_INFO23	0x2AC
#define XTENSA_INFO24	0x2B0
#define XTENSA_INFO25	0x2B4
#define XTENSA_INFO26	0x2B8
#define XTENSA_INFO27	0x2BC
#define XTENSA_INFO28	0x2C0
#define XTENSA_INFO29	0x2C4
#define XTENSA_INFO30	0x2C8
#define XTENSA_INFO31	0x2CC
#define DEBUG_INFO00	0x2D0
#define DEBUG_INFO01	0x2D4
#define DEBUG_INFO02	0x2D8
#define DEBUG_INFO03	0x2DC
#define DEBUG_INFO04	0x2E0
#define DEBUG_INFO05	0x2E4
#define DEBUG_INFO06	0x2E8
#define DEBUG_INFO07	0x2EC
#define XTENSA_ALTRESETVEC	0x2F8

/* efuse register related define */
#define EFUSE_VPU_OFFSET	5
#define EFUSE_VPU_MASK		0x7
#define EFUSE_VPU_SHIFT		16

/* mpu protect region definition */
#define MPU_PROCT_REGION	21
#define MPU_PROCT_D0_AP		0
#define MPU_PROCT_D5_APUSYS	5

static inline
unsigned long vpu_reg_base(struct vpu_device *vd)
{
	return (unsigned long)vd->reg.m;
}

static inline
uint32_t vpu_reg_read(struct vpu_device *vd, int offset)
{
	return ioread32((void *) (vpu_reg_base(vd) + offset));
}

static inline
void vpu_reg_write(struct vpu_device *vd, int offset, uint32_t val)
{
	mt_reg_sync_writel(val, (void *) (vpu_reg_base(vd) + offset));
}

static inline
void vpu_reg_clr(struct vpu_device *vd, int offset, uint32_t mask)
{
	vpu_reg_write(vd, offset, vpu_reg_read(vd, offset) & ~mask);
}

static inline
void vpu_reg_set(struct vpu_device *vd, int offset, uint32_t mask)
{
	vpu_reg_write(vd, offset, vpu_reg_read(vd, offset) | mask);
}

#endif
