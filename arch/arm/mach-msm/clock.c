/* arch/arm/mach-msm/clock.c
 *
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
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

#include "clock.h"

/*
 * Standard clock functions defined in include/linux/clk.h
 */
int clk_enable(struct clk *clk)
{
	int ret = 0;
	unsigned long flags;
	struct clk *parent;

	if (!clk)
		return 0;

	spin_lock_irqsave(&clk->lock, flags);
	if (clk->count == 0) {
		parent = clk_get_parent(clk);
		ret = clk_enable(parent);
		if (ret)
			goto out;

		if (clk->ops->enable)
			ret = clk->ops->enable(clk);
		if (ret) {
			clk_disable(parent);
			goto out;
		}
	}
	clk->count++;
out:
	spin_unlock_irqrestore(&clk->lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;
	struct clk *parent;

	if (!clk)
		return;

	spin_lock_irqsave(&clk->lock, flags);
	if (WARN_ON(clk->count == 0))
		goto out;
	if (clk->count == 1) {
		if (clk->ops->disable)
			clk->ops->disable(clk);
		parent = clk_get_parent(clk);
		clk_disable(parent);
	}
	clk->count--;
out:
	spin_unlock_irqrestore(&clk->lock, flags);
}
EXPORT_SYMBOL(clk_disable);

int clk_reset(struct clk *clk, enum clk_reset_action action)
{
	if (!clk->ops->reset)
		return -ENOSYS;

	return clk->ops->reset(clk, action);
}
EXPORT_SYMBOL(clk_reset);

unsigned long clk_get_rate(struct clk *clk)
{
	if (!clk->ops->get_rate)
		return 0;

	return clk->ops->get_rate(clk);
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	if (!clk->ops->set_rate)
		return -ENOSYS;

	return clk->ops->set_rate(clk, rate);
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!clk->ops->round_rate)
		return -ENOSYS;

	return clk->ops->round_rate(clk, rate);
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_min_rate(struct clk *clk, unsigned long rate)
{
	if (!clk->ops->set_min_rate)
		return -ENOSYS;

	return clk->ops->set_min_rate(clk, rate);
}
EXPORT_SYMBOL(clk_set_min_rate);

int clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	if (!clk->ops->set_max_rate)
		return -ENOSYS;

	return clk->ops->set_max_rate(clk, rate);
}
EXPORT_SYMBOL(clk_set_max_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	if (!clk->ops->set_parent)
		return 0;

	return clk->ops->set_parent(clk, parent);
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	if (!clk->ops->get_parent)
		return NULL;

	return clk->ops->get_parent(clk);
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_flags(struct clk *clk, unsigned long flags)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;
	if (!clk->ops->set_flags)
		return -ENOSYS;

	return clk->ops->set_flags(clk, flags);
}
EXPORT_SYMBOL(clk_set_flags);

static struct clk_lookup *msm_clocks;
static unsigned msm_num_clocks;

void __init msm_clock_init(struct clk_lookup *clock_tbl, size_t num_clocks)
{
	unsigned n;

	for (n = 0; n < num_clocks; n++) {
		struct clk *clk = clock_tbl[n].clk;
		struct clk *parent = clk_get_parent(clk);
		clk_set_parent(clk, parent);
	}

	clkdev_add_table(clock_tbl, num_clocks);
	msm_clocks = clock_tbl;
	msm_num_clocks = num_clocks;
}

/*
 * The bootloader and/or AMSS may have left various clocks enabled.
 * Disable any clocks that have not been explicitly enabled by a
 * clk_enable() call and don't have the CLKFLAG_SKIP_AUTO_OFF flag.
 */
static int __init clock_late_init(void)
{
	unsigned n;
	unsigned long flags;
	unsigned count = 0;

	clock_debug_init(msm_clocks, msm_num_clocks);
	for (n = 0; n < msm_num_clocks; n++) {
		struct clk *clk = msm_clocks[n].clk;

		clock_debug_add(clk);
		if (!(clk->flags & CLKFLAG_SKIP_AUTO_OFF)) {
			spin_lock_irqsave(&clk->lock, flags);
			if (!clk->count && clk->ops->auto_off) {
				count++;
				clk->ops->auto_off(clk);
			}
			spin_unlock_irqrestore(&clk->lock, flags);
		}
	}
	pr_info("clock_late_init() disabled %d unused clocks\n", count);
	return 0;
}
late_initcall(clock_late_init);
