/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>

#include <mach/clk.h>
#include <mach/socinfo.h>

#include "proc_comm.h"
#include "clock.h"
#include "clock-pcom.h"

/*
 * glue for the proc_comm interface
 */
static int pc_clk_enable(struct clk *clk)
{
	int rc;
	int id = to_pcom_clk(clk)->id;

	/* Ignore clocks that are always on */
	if (id == P_EBI1_CLK || id == P_EBI1_FIXED_CLK)
		return 0;

	rc = msm_proc_comm(PCOM_CLKCTL_RPC_ENABLE, &id, NULL);
	if (rc < 0)
		return rc;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static void pc_clk_disable(struct clk *clk)
{
	int id = to_pcom_clk(clk)->id;

	/* Ignore clocks that are always on */
	if (id == P_EBI1_CLK || id == P_EBI1_FIXED_CLK)
		return;

	msm_proc_comm(PCOM_CLKCTL_RPC_DISABLE, &id, NULL);
}

int pc_clk_reset(unsigned id, enum clk_reset_action action)
{
	int rc;

	if (action == CLK_RESET_ASSERT)
		rc = msm_proc_comm(PCOM_CLKCTL_RPC_RESET_ASSERT, &id, NULL);
	else
		rc = msm_proc_comm(PCOM_CLKCTL_RPC_RESET_DEASSERT, &id, NULL);

	if (rc < 0)
		return rc;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static int pc_reset(struct clk *clk, enum clk_reset_action action)
{
	int id = to_pcom_clk(clk)->id;
	return pc_clk_reset(id, action);
}

static int pc_clk_set_rate(struct clk *clk, unsigned rate)
{
	/* The rate _might_ be rounded off to the nearest KHz value by the
	 * remote function. So a return value of 0 doesn't necessarily mean
	 * that the exact rate was set successfully.
	 */
	int id = to_pcom_clk(clk)->id;
	int rc = msm_proc_comm(PCOM_CLKCTL_RPC_SET_RATE, &id, &rate);
	if (rc < 0)
		return rc;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static int pc_clk_set_min_rate(struct clk *clk, unsigned rate)
{
	int rc;
	int id = to_pcom_clk(clk)->id;
	bool ignore_error = (cpu_is_msm7x27() && id == P_EBI1_CLK &&
				rate >= INT_MAX);

	rc = msm_proc_comm(PCOM_CLKCTL_RPC_MIN_RATE, &id, &rate);
	if (rc < 0)
		return rc;
	else if (ignore_error)
		return 0;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static int pc_clk_set_max_rate(struct clk *clk, unsigned rate)
{
	int id = to_pcom_clk(clk)->id;
	int rc = msm_proc_comm(PCOM_CLKCTL_RPC_MAX_RATE, &id, &rate);
	if (rc < 0)
		return rc;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static int pc_clk_set_flags(struct clk *clk, unsigned flags)
{
	int id = to_pcom_clk(clk)->id;
	int rc = msm_proc_comm(PCOM_CLKCTL_RPC_SET_FLAGS, &id, &flags);
	if (rc < 0)
		return rc;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static int pc_clk_set_ext_config(struct clk *clk, unsigned config)
{
	int id = to_pcom_clk(clk)->id;
	int rc = msm_proc_comm(PCOM_CLKCTL_RPC_SET_EXT_CONFIG, &id, &config);
	if (rc < 0)
		return rc;
	else
		return (int)id < 0 ? -EINVAL : 0;
}

static unsigned pc_clk_get_rate(struct clk *clk)
{
	int id = to_pcom_clk(clk)->id;
	if (msm_proc_comm(PCOM_CLKCTL_RPC_RATE, &id, NULL))
		return 0;
	else
		return id;
}

static int pc_clk_is_enabled(struct clk *clk)
{
	int id = to_pcom_clk(clk)->id;
	if (msm_proc_comm(PCOM_CLKCTL_RPC_ENABLED, &id, NULL))
		return 0;
	else
		return id;
}

static long pc_clk_round_rate(struct clk *clk, unsigned rate)
{

	/* Not really supported; pc_clk_set_rate() does rounding on it's own. */
	return rate;
}

static bool pc_clk_is_local(struct clk *clk)
{
	return false;
}

struct clk_ops clk_ops_pcom = {
	.enable = pc_clk_enable,
	.disable = pc_clk_disable,
	.auto_off = pc_clk_disable,
	.reset = pc_reset,
	.set_rate = pc_clk_set_rate,
	.set_min_rate = pc_clk_set_min_rate,
	.set_max_rate = pc_clk_set_max_rate,
	.set_flags = pc_clk_set_flags,
	.get_rate = pc_clk_get_rate,
	.is_enabled = pc_clk_is_enabled,
	.round_rate = pc_clk_round_rate,
	.is_local = pc_clk_is_local,
};

struct clk_ops clk_ops_pcom_ext_config = {
	.enable = pc_clk_enable,
	.disable = pc_clk_disable,
	.auto_off = pc_clk_disable,
	.reset = pc_reset,
	.set_rate = pc_clk_set_ext_config,
	.set_min_rate = pc_clk_set_min_rate,
	.set_max_rate = pc_clk_set_max_rate,
	.set_flags = pc_clk_set_flags,
	.get_rate = pc_clk_get_rate,
	.is_enabled = pc_clk_is_enabled,
	.round_rate = pc_clk_round_rate,
	.is_local = pc_clk_is_local,
};

