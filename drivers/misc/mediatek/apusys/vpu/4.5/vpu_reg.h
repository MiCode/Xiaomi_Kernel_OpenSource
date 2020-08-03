/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __VPU_REG_H__
#define __VPU_REG_H__

#include <linux/io.h>
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

#define vpu_reg_base(vd)  ((unsigned long)vd->reg.m)

#define vpu_reg_read(vd, r) \
	ioread32((void *) (vpu_reg_base(vd) + (vd_reg(vd)->r)))

#define vpu_reg_write(vd, r, val) \
	iowrite32(val, (void *) (vpu_reg_base(vd) + (vd_reg(vd)->r)))

#define vpu_reg_clr(vd, r, mask) \
	vpu_reg_write(vd, r, vpu_reg_read(vd, r) & ~mask)

#define vpu_reg_set(vd, r, mask) \
	vpu_reg_write(vd, r, vpu_reg_read(vd, r) | mask)

#define vpu_reg_read_ofs(vd, ofs) \
	ioread32((void *) (vpu_reg_base(vd) + ofs))

#endif
