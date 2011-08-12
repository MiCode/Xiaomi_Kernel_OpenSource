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
		ret = clk_enable(clk->depends);
		if (ret) {
			clk_disable(parent);
			goto out;
		}

		if (clk->ops->enable)
			ret = clk->ops->enable(clk);
		if (ret) {
			clk_disable(clk->depends);
			clk_disable(parent);
			goto out;
		}
	} else if (clk->flags & CLKFLAG_HANDOFF_RATE) {
		/*
		 * The clock was already enabled by handoff code so there is no
		 * need to enable it again here. Clearing the handoff flag will
		 * prevent the lateinit handoff code from disabling the clock if
		 * a client driver still has it enabled.
		 */
		clk->flags &= ~CLKFLAG_HANDOFF_RATE;
		goto out;
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
	if (WARN(clk->count == 0, "%s is unbalanced", clk->dbg_name))
		goto out;
	if (clk->count == 1) {
		if (clk->ops->disable)
			clk->ops->disable(clk);
		clk_disable(clk->depends);
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

static struct clock_init_data __initdata *clk_init_data;

void __init msm_clock_init(struct clock_init_data *data)
{
	unsigned n;
	struct clk_lookup *clock_tbl = data->table;
	size_t num_clocks = data->size;

	clk_init_data = data;
	if (clk_init_data->init)
		clk_init_data->init();

	for (n = 0; n < num_clocks; n++) {
		struct clk *clk = clock_tbl[n].clk;
		struct clk *parent = clk_get_parent(clk);
		clk_set_parent(clk, parent);
		if (clk->ops->handoff)
			clk->ops->handoff(clk);
	}

	clkdev_add_table(clock_tbl, num_clocks);
}

/*
 * The bootloader and/or AMSS may have left various clocks enabled.
 * Disable any clocks that have not been explicitly enabled by a
 * clk_enable() call and don't have the CLKFLAG_SKIP_AUTO_OFF flag.
 */
static int __init clock_late_init(void)
{
	unsigned n, count = 0;
	unsigned long flags;
	int ret = 0;

	clock_debug_init(clk_init_data);
	for (n = 0; n < clk_init_data->size; n++) {
		struct clk *clk = clk_init_data->table[n].clk;
		bool handoff = false;

		clock_debug_add(clk);
		if (!(clk->flags & CLKFLAG_SKIP_AUTO_OFF)) {
			spin_lock_irqsave(&clk->lock, flags);
			if (!clk->count && clk->ops->auto_off) {
				count++;
				clk->ops->auto_off(clk);
			}
			if (clk->flags & CLKFLAG_HANDOFF_RATE) {
				clk->flags &= ~CLKFLAG_HANDOFF_RATE;
				handoff = true;
			}
			spin_unlock_irqrestore(&clk->lock, flags);
			/*
			 * Calling clk_disable() outside the lock is safe since
			 * it doesn't need to be atomic with the flag change.
			 */
			if (handoff)
				clk_disable(clk);
		}
	}
	pr_info("clock_late_init() disabled %d unused clocks\n", count);
	if (clk_init_data->late_init)
		ret = clk_init_data->late_init();
	return ret;
}
late_initcall(clock_late_init);
