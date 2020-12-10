/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include "apu_devfreq.h"
#include "apu_clk.h"
#include "apu_regulator.h"

#define APMIX_PLL_NODE		"apmix-pll"
#define TOP_PLL_NODE		"top-pll"
#define TOPMUX_NODE	"top-mux"
#define SYSMUX_NODE	"sys-mux"
#define SYSMUX_PARENT_NODE	"sys-mux-parent"

/**
 * of_apu_clk_get - lookup and obtain a reference to a clock producer.
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Returns a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the clock consumer, and thereby
 * the clock producer.  (IOW, @id may be identical strings, but
 * clk_get may return different clock producers depending on @dev.)
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clk_get should not be called from within interrupt context.
 */
int of_apu_clk_get(struct device *dev, const char *id, struct apu_clk **dst);

/**
 * of_apu_clk_put - release apu clk resources.
 * @dst: struct apu_clk to release
 *
 */
void of_apu_clk_put(struct apu_clk **dst);

int of_apu_cg_get(struct device *dev,	struct apu_cgs **dst);

void of_apu_cg_put(struct apu_cgs **dst);

int of_apu_link(struct device *dev, struct device_node *con_np, struct device_node *sup_np,
			      u32 dl_flags);

void of_apu_regulator_put(struct apu_regulator *rgul);
int of_apu_regulator_get(struct device *dev,
		struct apu_regulator *rgul, unsigned long volt, ulong freq);
