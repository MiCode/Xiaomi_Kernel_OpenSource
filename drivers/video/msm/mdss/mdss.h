/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_H
#define MDSS_H

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#define MDSS_REG_WRITE(addr, val) writel_relaxed(val, mdss_reg_base + addr)
#define MDSS_REG_READ(addr) readl_relaxed(mdss_reg_base + addr)

extern unsigned char *mdss_reg_base;

enum mdss_mdp_clk_type {
	MDSS_CLK_AHB,
	MDSS_CLK_AXI,
	MDSS_CLK_MDP_SRC,
	MDSS_CLK_MDP_CORE,
	MDSS_CLK_MDP_LUT,
	MDSS_CLK_MDP_VSYNC,
	MDSS_MAX_CLK
};

struct mdss_res_type {
	u32 rev;
	u32 mdp_rev;
	struct clk *mdp_clk[MDSS_MAX_CLK];
	struct regulator *fs;

	struct workqueue_struct *clk_ctrl_wq;
	struct delayed_work clk_ctrl_worker;

	u32 irq;
	u32 irq_mask;
	u32 irq_ena;
	u32 irq_buzy;

	u32 clk_ena;
	u32 suspend;
	u32 timeout;

	u32 fs_ena;
	u32 vsync_ena;

	u32 intf;
	u32 eintf_ena;
	u32 prim_ptype;
	u32 res_init;
	u32 pdev_lcnt;
	u32 bus_hdl;

	u32 smp_mb_cnt;
	u32 smp_mb_size;
	u32 *pipe_type_map;
	u32 *mixer_type_map;
};
extern struct mdss_res_type *mdss_res;
#endif /* MDSS_H */
