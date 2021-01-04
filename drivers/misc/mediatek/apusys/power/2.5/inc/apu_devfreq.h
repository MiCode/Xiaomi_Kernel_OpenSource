/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_DEVFREQ_H__
#define __APU_DEVFREQ_H__
#include <linux/devfreq.h>
#include <linux/device.h>
#include <linux/pm_opp.h>

#include "apusys_power_user.h"

#define BOOST_MAX	100


/**
 * struct apu_dev - device struct represent each apu engine.
 *
 * @dev:     struct device
 * @df: devfreq struct
 * @p_oppt:  point to this device's opp table.
 * @opp_div: divider of opp
 * @node:    used for hook all struct apu_dev
 * @user:    enum dvfs_user
 * @regs:    the register base of this apu engine
 * @prc:     the rpc register base (for wakeup/suspend)
 * @cgs:     CGs of this apu device
 * @clock:   clk groups need for this apu device
 * @
 * This struct describes platform operations define for different platform.
 */
struct apu_dev {
	const char *name;

	struct device *dev;

	/* apu devices info */
	struct list_head node;
	enum DVFS_USER	user;

	/* clk info */
	struct apu_clk_gp *aclk;

	/* regulator info */
	struct apu_regulator_gp *argul;

	/* platform operations */
	struct apu_plat_ops *plat_ops;

	/* devfreq info */
	struct devfreq	 *df;
	struct opp_table *oppt;
	unsigned int opp_div;
};

static inline int devfreq_dummy_target(struct device *dev, unsigned long *rate,
				u32 flags) { return 0; }
#endif
