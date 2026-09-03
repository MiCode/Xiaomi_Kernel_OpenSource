/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_R8P0_CORE_H
#define _GSP_R8P0_CORE_H

#include <linux/device.h>
#include <linux/list.h>
#include <drm/gsp_cfg.h>
#include "gsp_core.h"
#include "gsp_debug.h"

#define R4P0_DPU_CLOCK_NAME		("clk_dpu_core")
#define R4P0_DPU_CLOCK_PARENT		("clk_dpu_core_src")

#define MIN_POOL_SIZE			(6 * 1024)
#define GSP_COEF_CACHE_MAX		32

struct gsp_r8p0_core {
	struct gsp_core common;
	struct list_head coef_list;
	struct coef_entry coef_cache[GSP_COEF_CACHE_MAX];

	ulong gsp_coef_force_calc;
	uint32_t cache_coef_init_flag;
	char coef_buf_pool[MIN_POOL_SIZE];

	struct clk *dpu_clk;
	struct clk *dpu_clk_parent;
	/* module ctl reg base, virtual	0x63000000 */
	void __iomem *gsp_ctl_reg_base;
};

#define MEM_OPS_ADDR_ALIGN_MASK (0x7UL)

int gsp_r8p0_core_parse_dt(struct gsp_core *core);

int gsp_r8p0_core_copy_cfg(struct gsp_kcfg *kcfg, void *arg, int index);

int gsp_r8p0_core_init(struct gsp_core *core);

int gsp_r8p0_core_alloc(struct gsp_core **core, struct device_node *node);

int gsp_r8p0_core_trigger(struct gsp_core *core);

int gsp_r8p0_core_enable(struct gsp_core *core);

void gsp_r8p0_core_disable(struct gsp_core *core);

int gsp_r8p0_core_release(struct gsp_core *core);

int __user *gsp_r8p0_core_intercept(void __user *arg, int index);
void gsp_r8p0_core_reset(struct gsp_core *core);
void gsp_r8p0_core_dump(struct gsp_core *core);

#endif
