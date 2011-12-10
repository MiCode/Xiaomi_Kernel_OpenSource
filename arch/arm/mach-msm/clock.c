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

/* Find the voltage level required for a given rate. */
static int find_vdd_level(struct clk *clk, unsigned long rate)
{
	int level;

	for (level = 0; level < ARRAY_SIZE(clk->fmax); level++)
		if (rate <= clk->fmax[level])
			break;

	if (level == ARRAY_SIZE(clk->fmax)) {
		pr_err("Rate %lu for %s is greater than highest Fmax\n", rate,
			clk->dbg_name);
		return -EINVAL;
	}

	return level;
}

/* Update voltage level given the current votes. */
static int update_vdd(struct clk_vdd_class *vdd_class)
{
	int level, rc;

	for (level = ARRAY_SIZE(vdd_class->level_votes)-1; level > 0; level--)
		if (vdd_class->level_votes[level])
			break;

	if (level == vdd_class->cur_level)
		return 0;

	rc = vdd_class->set_vdd(vdd_class, level);
	if (!rc)
		vdd_class->cur_level = level;

	return rc;
}

/* Vote for a voltage level. */
int vote_vdd_level(struct clk_vdd_class *vdd_class, int level)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&vdd_class->lock, flags);
	vdd_class->level_votes[level]++;
	rc = update_vdd(vdd_class);
	if (rc)
		vdd_class->level_votes[level]--;
	spin_unlock_irqrestore(&vdd_class->lock, flags);

	return rc;
}

/* Remove vote for a voltage level. */
int unvote_vdd_level(struct clk_vdd_class *vdd_class, int level)
{
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&vdd_class->lock, flags);
	if (WARN(!vdd_class->level_votes[level],
			"Reference counts are incorrect for %s level %d\n",
			vdd_class->class_name, level))
		goto out;
	vdd_class->level_votes[level]--;
	rc = update_vdd(vdd_class);
	if (rc)
		vdd_class->level_votes[level]++;
out:
	spin_unlock_irqrestore(&vdd_class->lock, flags);
	return rc;
}

/* Vote for a voltage level corresponding to a clock's rate. */
static int vote_rate_vdd(struct clk *clk, unsigned long rate)
{
	int level;

	if (!clk->vdd_class)
		return 0;

	level = find_vdd_level(clk, rate);
	if (level < 0)
		return level;

	return vote_vdd_level(clk->vdd_class, level);
}

/* Remove vote for a voltage level corresponding to a clock's rate. */
static void unvote_rate_vdd(struct clk *clk, unsigned long rate)
{
	int level;

	if (!clk->vdd_class)
		return;

	level = find_vdd_level(clk, rate);
	if (level < 0)
		return;

	unvote_vdd_level(clk->vdd_class, level);
}

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
			goto err_enable_parent;
		ret = clk_enable(clk->depends);
		if (ret)
			goto err_enable_depends;

		ret = vote_rate_vdd(clk, clk->rate);
		if (ret)
			goto err_vote_vdd;
		if (clk->ops->enable)
			ret = clk->ops->enable(clk);
		if (ret)
			goto err_enable_clock;
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

	return 0;

err_enable_clock:
	unvote_rate_vdd(clk, clk->rate);
err_vote_vdd:
	clk_disable(clk->depends);
err_enable_depends:
	clk_disable(parent);
err_enable_parent:
	spin_unlock_irqrestore(&clk->lock, flags);
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if (!clk)
		return;

	spin_lock_irqsave(&clk->lock, flags);
	if (WARN(clk->count == 0, "%s is unbalanced", clk->dbg_name))
		goto out;
	if (clk->count == 1) {
		struct clk *parent = clk_get_parent(clk);

		if (clk->ops->disable)
			clk->ops->disable(clk);
		unvote_rate_vdd(clk, clk->rate);
		clk_disable(clk->depends);
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
	unsigned long start_rate, flags;
	int rc;

	if (!clk->ops->set_rate)
		return -ENOSYS;

	spin_lock_irqsave(&clk->lock, flags);
	if (clk->count) {
		start_rate = clk->rate;
		/* Enforce vdd requirements for target frequency. */
		rc = vote_rate_vdd(clk, rate);
		if (rc)
			goto err_vote_vdd;
		rc = clk->ops->set_rate(clk, rate);
		if (rc)
			goto err_set_rate;
		/* Release vdd requirements for starting frequency. */
		unvote_rate_vdd(clk, start_rate);
	} else {
		rc = clk->ops->set_rate(clk, rate);
	}

	if (!rc)
		clk->rate = rate;

	spin_unlock_irqrestore(&clk->lock, flags);
	return rc;

err_set_rate:
	unvote_rate_vdd(clk, rate);
err_vote_vdd:
	spin_unlock_irqrestore(&clk->lock, flags);
	return rc;
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!clk->ops->round_rate)
		return -ENOSYS;

	return clk->ops->round_rate(clk, rate);
}
EXPORT_SYMBOL(clk_round_rate);

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
	struct clk_lookup *clock_tbl;
	size_t num_clocks;

	clk_init_data = data;
	if (clk_init_data->init)
		clk_init_data->init();

	clock_tbl = data->table;
	num_clocks = data->size;

	for (n = 0; n < num_clocks; n++) {
		struct clk *clk = clock_tbl[n].clk;
		struct clk *parent = clk_get_parent(clk);
		clk_set_parent(clk, parent);
		if (clk->ops->handoff && !(clk->flags & CLKFLAG_HANDOFF_RATE)) {
			if (clk->ops->handoff(clk)) {
				clk->flags |= CLKFLAG_HANDOFF_RATE;
				clk_enable(clk);
			}
		}
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
