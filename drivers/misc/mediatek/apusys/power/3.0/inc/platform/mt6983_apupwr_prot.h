/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6983_APUPWR_PROT_H__
#define __MT6983_APUPWR_PROT_H__

#include "apu_top.h"
#include "mt6983_apupwr.h"

#define ACX0_LIMIT_OPP_REG      SPARE0_MBOX_DUMMY_0_ADDR
#define ACX1_LIMIT_OPP_REG      SPARE0_MBOX_DUMMY_1_ADDR
#define DEV_OPP_SYNC_REG        SPARE0_MBOX_DUMMY_2_ADDR
#define HW_RES_SYNC_REG         SPARE0_MBOX_DUMMY_3_ADDR

enum apu_opp_limit_type {
	OPP_LIMIT_THERMAL = 0,	// limit by power API
	OPP_LIMIT_HAL,		// limit by i/o ctl
	OPP_LIMIT_DEBUG,	// limit by i/o ctl
};

struct device_opp_limit {
	int8_t vpu_max:4,
	       vpu_min:4;
	int8_t dla_max:4,
	       dla_min:4;
	int8_t lmt_type; // limit reason
};

struct cluster_dev_opp_info {
	uint32_t opp_lmt_reg;
	struct device_opp_limit dev_opp_lmt;
};

struct hw_resource_status {
	int32_t vapu_opp:4,
		vsram_opp:4,
		vcore_opp:4,
		fconn_opp:4,
		fvpu_opp:4,
		fdla_opp:4,
		fup_opp:4,
		reserved:4;
};

void aputop_opp_limit(struct aputop_func_param *aputop,
		enum apu_opp_limit_type type);
void aputop_curr_status(struct aputop_func_param *aputop);
void aputop_pwr_cfg(struct aputop_func_param *aputop);
void aputop_pwr_ut(struct aputop_func_param *aputop);

#if IS_ENABLED(CONFIG_DEBUG_FS)
int mt6983_apu_top_dbg_open(struct inode *inode, struct file *file);
ssize_t mt6983_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos);
#endif

int aputop_opp_limiter_init(void __iomem *reg_base);

#endif
