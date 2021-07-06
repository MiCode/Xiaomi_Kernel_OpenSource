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
#include "vpu_cfg.h"
#include "vpu_cmn.h"

/* Spare Register - Enum */
enum {
	VPU_CMD_DO_EXIT         = 0x00,
	VPU_CMD_DO_LOADER       = 0x01,
	VPU_CMD_DO_D2D          = 0x22,
	VPU_CMD_DO_D2D_EXT      = 0x24,
	VPU_CMD_SET_DEBUG       = 0x40,
	VPU_CMD_SET_FTRACE_LOG  = 0x42,

	/* Extend for test */
	VPU_CMD_EXT_BUSY        = 0xF0,
	VPU_CMD_DO_D2D_EXT_TEST = 0x80000024
};

/* host side state */
enum {
	VPU_STATE_NOT_READY     = 0x00,
	VPU_STATE_READY         = 0x01,
	VPU_STATE_IDLE          = 0x02,
	VPU_STATE_BUSY          = 0x04,
	VPU_STATE_ERROR         = 0x08,
	VPU_STATE_TERMINATED    = 0x10,
	VPU_STATE_ABORT         = 0xFF  /* Aborted by driver */
};

/* device to host request */
enum {
	VPU_REQ_NONE               = 0,
	VPU_REQ_DO_CHECK_STATE     = 0x100,
	VPU_REQ_DO_DUMP_LOG        = 0x101,
	VPU_REQ_DO_CLOSED_FILE     = 0x102,
	VPU_REQ_MAX                = 0xFFFF,
};

/* device state, INFO17 b24..16 */
enum {
	DS_DSP_RDY = 0x010000,       /* boot-up done */
	DS_DBG_RDY = 0x020000,       /* set-debug done */
	DS_ALG_RDY = 0x040000,       /* do-loader done */
	DS_ALG_DONE = 0x080000,      /* d2d done */
	DS_ALG_GOT = 0x100000,       /* get-algo done */
	DS_PREEMPT_RDY = 0x200000,   /* context switch done */
	DS_PREEMPT_DONE = 0x400000,  /* d2d-ext done */
	DS_FTRACE_RDY = 0x800000,    /* set-ftrace done */
};

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
	iowrite32(val, (void *) (vpu_reg_base(vd) + offset));
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
