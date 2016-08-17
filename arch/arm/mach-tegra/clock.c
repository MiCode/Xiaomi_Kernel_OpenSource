/*
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (c) 2019-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/bug.h>
#include <trace/events/power.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/hardware.h>
#include <mach/edp.h>

#include "board.h"
#include "clock.h"
#include "dvfs.h"
#include "timer.h"

#define DISABLE_BOOT_CLOCKS 1

/*
 * Locking:
 *
 * Each struct clk has a lock.  Depending on the cansleep flag, that lock
 * may be a spinlock or a mutex.  For most clocks, the spinlock is sufficient,
 * and using the spinlock allows the clock to be manipulated from an interrupt
 * or while holding a spinlock.  Some clocks may need to adjust a regulator
 * in order to maintain the required voltage for a new frequency.  Those
 * clocks set the cansleep flag, and take a mutex so that the regulator api
 * can be used while holding the lock.
 *
 * To avoid AB-BA locking problems, locks must always be traversed from child
 * clock to parent clock.  For example, when enabling a clock, the clock's lock
 * is taken, and then clk_enable is called on the parent, which take's the
 * parent clock's lock.  There are two exceptions to this ordering:
 *  1. When setting a clock as cansleep, in which case the entire list of clocks
 *     is traversed to set the children as cansleep as well.  This must occur
 *     during init, before any calls to clk_get, so no other clock locks can
 *     get taken.
 *  2. When dumping the clock tree through debugfs.  In this case, clk_lock_all
 *     is called, which attemps to iterate through the entire list of clocks
 *     and take every clock lock.  If any call to clk_trylock fails, a locked
 *     clocks are unlocked, and the process is retried.  When all the locks
 *     are held, the only clock operation that can be called is
 *     clk_get_rate_all_locked.
 *
 * Within a single clock, no clock operation can call another clock operation
 * on itself, except for clk_xxx_locked.  Any clock operation can call any other
 * clock operation on any of it's possible parents.
 *
 * clk_set_cansleep is used to mark a clock as sleeping.  It is called during
 * dvfs (Dynamic Voltage and Frequency Scaling) init on any clock that has a
 * dvfs requirement, and propagated to all possible children of sleeping clock.
 *
 * An additional mutex, clock_list_lock, is used to protect the list of all
 * clocks.
 *
 * The clock operations must lock internally to protect against
 * read-modify-write on registers that are shared by multiple clocks
 */

/* FIXME: remove and never ignore overclock */
#define IGNORE_PARENT_OVERCLOCK 0

static DEFINE_MUTEX(clock_list_lock);
static LIST_HEAD(clocks);
static unsigned long osc_freq;

struct clk *tegra_get_clock_by_name(const char *name)
{
	struct clk *c;
	struct clk *ret = NULL;
	mutex_lock(&clock_list_lock);
	list_for_each_entry(c, &clocks, node) {
		if (strcmp(c->name, name) == 0) {
			ret = c;
			break;
		}
	}
	mutex_unlock(&clock_list_lock);
	return ret;
}
EXPORT_SYMBOL(tegra_get_clock_by_name);

static void clk_stats_update(struct clk *c)
{
	u64 cur_jiffies = get_jiffies_64();

	if (c->refcnt) {
		c->stats.time_on = c->stats.time_on +
			(jiffies64_to_cputime64(cur_jiffies) -
			 (c->stats.last_update));
	}

	c->stats.last_update = cur_jiffies;
}

/* Must be called with clk_lock(c) held */
static unsigned long clk_predict_rate_from_parent(struct clk *c, struct clk *p)
{
	u64 rate;

	rate = clk_get_rate(p);

	if (c->mul != 0 && c->div != 0) {
		rate *= c->mul;
		rate += c->div - 1; /* round up */
		do_div(rate, c->div);
	}

	return rate;
}

unsigned long clk_get_max_rate(struct clk *c)
{
		return c->max_rate;
}

unsigned long clk_get_min_rate(struct clk *c)
{
		return c->min_rate;
}

/* Must be called with clk_lock(c) held */
unsigned long clk_get_rate_locked(struct clk *c)
{
	unsigned long rate;

	if (c->parent)
		rate = clk_predict_rate_from_parent(c, c->parent);
	else
		rate = c->rate;

	return rate;
}

unsigned long clk_get_rate(struct clk *c)
{
	unsigned long flags;
	unsigned long rate;

	clk_lock_save(c, &flags);

	rate = clk_get_rate_locked(c);

	clk_unlock_restore(c, &flags);

	return rate;
}
EXPORT_SYMBOL(clk_get_rate);

static void __clk_set_cansleep(struct clk *c)
{
	struct clk *child;
	int i;
	BUG_ON(mutex_is_locked(&c->mutex));
	BUG_ON(spin_is_locked(&c->spinlock));

	/* Make sure that all possible descendants of sleeping clock are
	   marked as sleeping (to eliminate "sleeping parent - non-sleeping
	   child" relationship */
	list_for_each_entry(child, &clocks, node) {
		bool possible_parent = (child->parent == c);

		if (!possible_parent && child->inputs) {
			for (i = 0; child->inputs[i].input; i++) {
				if ((child->inputs[i].input == c) &&
				    tegra_clk_is_parent_allowed(child, c)) {
					possible_parent = true;
					break;
				}
			}
		}

		if (possible_parent)
			__clk_set_cansleep(child);
	}

	c->cansleep = true;
}

/* Must be called before any clk_get calls */
void clk_set_cansleep(struct clk *c)
{

	mutex_lock(&clock_list_lock);
	__clk_set_cansleep(c);
	mutex_unlock(&clock_list_lock);
}

int clk_reparent(struct clk *c, struct clk *parent)
{
	c->parent = parent;
	return 0;
}

void clk_init(struct clk *c)
{
	clk_lock_init(c);

	if (c->ops && c->ops->init)
		c->ops->init(c);

	if (!c->ops || !c->ops->enable) {
		c->refcnt++;
		c->set = true;
		if (c->parent)
			c->state = c->parent->state;
		else
			c->state = ON;
	}
	c->stats.last_update = get_jiffies_64();

	mutex_lock(&clock_list_lock);
	list_add(&c->node, &clocks);
	mutex_unlock(&clock_list_lock);
}

static int clk_enable_locked(struct clk *c)
{
	int ret = 0;

	if (clk_is_auto_dvfs(c)) {
		ret = tegra_dvfs_set_rate(c, clk_get_rate_locked(c));
		if (ret)
			return ret;
	}

	if (c->refcnt == 0) {
		if (c->parent) {
			ret = tegra_clk_prepare_enable(c->parent);
			if (ret)
				return ret;
		}

		if (c->ops && c->ops->enable) {
			ret = c->ops->enable(c);
			trace_clock_enable(c->name, 1, 0);
			if (ret) {
				if (c->parent)
					tegra_clk_disable_unprepare(c->parent);
				return ret;
			}
			c->state = ON;
			c->set = true;
		}
		clk_stats_update(c);
	}
	c->refcnt++;

	return ret;
}

static void clk_disable_locked(struct clk *c)
{
	if (c->refcnt == 0) {
		WARN(1, "Attempting to disable clock %s with refcnt 0", c->name);
		return;
	}
	if (c->refcnt == 1) {
		if (c->ops && c->ops->disable) {
			trace_clock_disable(c->name, 0, 0);
			c->ops->disable(c);
		}
		if (c->parent)
			tegra_clk_disable_unprepare(c->parent);

		c->state = OFF;
		clk_stats_update(c);
	}
	c->refcnt--;

	if (clk_is_auto_dvfs(c) && c->refcnt == 0)
		tegra_dvfs_set_rate(c, 0);
}

#ifdef CONFIG_HAVE_CLK_PREPARE
/*
 * The clk_enable/clk_disable may be called in atomic context, so they must not
 * hold mutex. On the other hand clk_prepare/clk_unprepare can hold a mutex,
 * as these APIs are called only in non-atomic context. Since tegra clock have
 * "cansleep" attributte that indicates if clock requires preparation, we can
 * split the interfaces respectively: do all work on sleeping clocks only in
 * clk_prepare/clk_unprepare, and do all work for non-sleeping clocks only in
 * clk_enable/clk_disable APIs. Calling "empty" APIs on either type of clocks
 * is allowed as well, and actually expected, since clients may not know the
 * clock attributes. However, calling clk_enable on non-prepared sleeping clock
 * would fail.
 */
int clk_prepare(struct clk *c)
{
	int ret = 0;
	unsigned long flags;

	if (!clk_cansleep(c))
		return 0;

	clk_lock_save(c, &flags);
	ret = clk_enable_locked(c);
	clk_unlock_restore(c, &flags);
	return ret;
}
EXPORT_SYMBOL(clk_prepare);

int clk_enable(struct clk *c)
{
	int ret = 0;
	unsigned long flags;

	if (clk_cansleep(c)) {
		if (WARN_ON(c->refcnt == 0))
			return -ENOSYS;
		return 0;
	}

	clk_lock_save(c, &flags);
	ret = clk_enable_locked(c);
	clk_unlock_restore(c, &flags);
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_unprepare(struct clk *c)
{
	unsigned long flags;

	if (!clk_cansleep(c))
		return;

	clk_lock_save(c, &flags);
	clk_disable_locked(c);
	clk_unlock_restore(c, &flags);
}
EXPORT_SYMBOL(clk_unprepare);

void clk_disable(struct clk *c)
{
	unsigned long flags;

	if (clk_cansleep(c))
		return;

	clk_lock_save(c, &flags);
	clk_disable_locked(c);
	clk_unlock_restore(c, &flags);
}
EXPORT_SYMBOL(clk_disable);
#else
int clk_enable(struct clk *c)
{
	int ret = 0;
	unsigned long flags;

	clk_lock_save(c, &flags);
	ret = clk_enable_locked(c);
	clk_unlock_restore(c, &flags);
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *c)
{
	unsigned long flags;

	clk_lock_save(c, &flags);
	clk_disable_locked(c);
	clk_unlock_restore(c, &flags);
}
EXPORT_SYMBOL(clk_disable);
#endif

int clk_rate_change_notify(struct clk *c, unsigned long rate)
{
	if (!c->rate_change_nh)
		return -ENOSYS;
	return raw_notifier_call_chain(c->rate_change_nh, rate, NULL);
}

int clk_set_parent_locked(struct clk *c, struct clk *parent)
{
	int ret = 0;
	unsigned long new_rate;
	unsigned long old_rate;
	bool disable = false;

	if (!c->ops || !c->ops->set_parent) {
		ret = -ENOSYS;
		goto out;
	}

	if (!tegra_clk_is_parent_allowed(c, parent)) {
		ret = -EINVAL;
		goto out;
	}

	new_rate = clk_predict_rate_from_parent(c, parent);
	old_rate = clk_get_rate_locked(c);

	if ((new_rate > clk_get_max_rate(c)) &&
		(!parent->ops || !parent->ops->shared_bus_update)) {

		pr_err("Failed to set parent %s for %s (violates clock limit"
		       " %lu)\n", parent->name, c->name, clk_get_max_rate(c));
#if !IGNORE_PARENT_OVERCLOCK
		ret = -EINVAL;
		goto out;
#endif
	}

	/* The new clock control register setting does not take effect if
	 * clock is disabled. Later, when the clock is enabled it would run
	 * for several cycles on the old parent, which may hang h/w if the
	 * parent is already disabled. To guarantee h/w switch to the new
	 * setting enable clock while setting parent.
	 */
	if ((c->refcnt == 0) && (c->flags & MUX)) {
		pr_debug("Setting parent of clock %s with refcnt 0\n", c->name);
		ret = clk_enable_locked(c);
		if (ret)
			goto out;
		disable = true;
	}

	if (clk_is_auto_dvfs(c) && c->refcnt > 0 &&
			(!c->parent || new_rate > old_rate)) {
		ret = tegra_dvfs_set_rate(c, new_rate);
		if (ret)
			goto out;
	}

	ret = c->ops->set_parent(c, parent);
	if (ret)
		goto out;

	trace_clock_set_parent(c->name, parent->name);

	if (clk_is_auto_dvfs(c) && c->refcnt > 0 &&
			new_rate < old_rate)
		ret = tegra_dvfs_set_rate(c, new_rate);

	if (new_rate != old_rate)
		clk_rate_change_notify(c, new_rate);

out:
	if (disable)
		clk_disable_locked(c);
	return ret;
}


int clk_set_parent(struct clk *c, struct clk *parent)
{
	int ret = 0;
	unsigned long flags;

	clk_lock_save(c, &flags);
	ret = clk_set_parent_locked(c, parent);
	clk_unlock_restore(c, &flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *c)
{
	return c->parent;
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_rate_locked(struct clk *c, unsigned long rate)
{
	int ret = 0;
	unsigned long old_rate, max_rate;
	long new_rate;
	bool disable = false;

	if (!c->ops || !c->ops->set_rate)
		return -ENOSYS;

	old_rate = clk_get_rate_locked(c);

	max_rate = clk_get_max_rate(c);
	if (rate > max_rate)
		rate = max_rate;

	if (c->ops && c->ops->round_rate) {
		new_rate = c->ops->round_rate(c, rate);

		if (new_rate < 0) {
			ret = new_rate;
			return ret;
		}

		rate = new_rate;
	}

	/* The new clock control register setting does not take effect if
	 * clock is disabled. Later, when the clock is enabled it would run
	 * for several cycles on the old rate, which may over-clock module
	 * at given voltage. To guarantee h/w switch to the new setting
	 * enable clock while setting rate.
	 */
	if ((c->refcnt == 0) && (c->flags & (DIV_U71 | DIV_U16)) &&
		clk_is_auto_dvfs(c)) {
		pr_debug("Setting rate of clock %s with refcnt 0\n", c->name);
		ret = clk_enable_locked(c);
		if (ret)
			goto out;
		disable = true;
	}

	if (clk_is_auto_dvfs(c) && rate > old_rate && c->refcnt > 0) {
		ret = tegra_dvfs_set_rate(c, rate);
		if (ret)
			goto out;
	}

	trace_clock_set_rate(c->name, rate, 0);
	ret = c->ops->set_rate(c, rate);
	if (ret)
		goto out;

	if (clk_is_auto_dvfs(c) && rate < old_rate && c->refcnt > 0)
		ret = tegra_dvfs_set_rate(c, rate);

	if (rate != old_rate)
		clk_rate_change_notify(c, rate);

out:
	if (disable)
		clk_disable_locked(c);
	return ret;
}

int clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long flags;
	int ret;

	if (!c->ops || !c->ops->set_rate)
		return -ENOSYS;

	clk_lock_save(c, &flags);

	ret = clk_set_rate_locked(c, rate);

	clk_unlock_restore(c, &flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

/* Must be called with clocks lock and all indvidual clock locks held */
unsigned long clk_get_rate_all_locked(struct clk *c)
{
	u64 rate;
	int mul = 1;
	int div = 1;
	struct clk *p = c;

	while (p) {
		c = p;
		if (c->mul != 0 && c->div != 0) {
			mul *= c->mul;
			div *= c->div;
		}
		p = c->parent;
	}

	rate = c->rate;
	rate *= mul;
	do_div(rate, div);

	return rate;
}

long clk_round_rate_locked(struct clk *c, unsigned long rate)
{
	unsigned long max_rate;
	long ret;

	if (!c->ops || !c->ops->round_rate) {
		ret = -ENOSYS;
		goto out;
	}

	max_rate = clk_get_max_rate(c);
	if (rate > max_rate)
		rate = max_rate;

	ret = c->ops->round_rate(c, rate);

out:
	return ret;
}

long clk_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long flags;
	long ret;

	clk_lock_save(c, &flags);
	ret = clk_round_rate_locked(c, rate);
	clk_unlock_restore(c, &flags);
	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

static int tegra_clk_clip_rate_for_parent(struct clk *c, struct clk *p)
{
	unsigned long flags, max_rate, old_rate, new_rate;

	clk_lock_save(c, &flags);

	max_rate = clk_get_max_rate(c);
	new_rate = clk_predict_rate_from_parent(c, p);
	old_rate = clk_get_rate_locked(c);

	clk_unlock_restore(c, &flags);

	if (new_rate > max_rate) {
		u64 rate = max_rate;
		rate *= old_rate;
		do_div(rate, new_rate);

		return clk_set_rate(c, (unsigned long)rate);
	}
	return 0;
}

static int tegra_clk_init_one_from_table(struct tegra_clk_init_table *table)
{
	struct clk *c;
	struct clk *p;

	int ret = 0;

	c = tegra_get_clock_by_name(table->name);

	if (!c) {
		pr_warning("Unable to initialize clock %s\n",
			table->name);
		return -ENODEV;
	}

	if (table->enabled) {
		ret = tegra_clk_prepare_enable(c);
		if (ret) {
			pr_warning("Unable to enable clock %s: %d\n",
				table->name, ret);
			return -EINVAL;
		}
	}

	if (table->parent) {
		p = tegra_get_clock_by_name(table->parent);
		if (!p) {
			pr_warning("Unable to find parent %s of clock %s\n",
				table->parent, table->name);
			return -ENODEV;
		}

		if (c->parent != p) {
			ret = tegra_clk_clip_rate_for_parent(c, p);
			if (ret) {
				pr_warning("Unable to clip rate for parent %s"
					   " of clock %s: %d\n",
					   table->parent, table->name, ret);
				return -EINVAL;
			}

			ret = clk_set_parent(c, p);
			if (ret) {
				pr_warning("Unable to set parent %s of clock %s: %d\n",
					table->parent, table->name, ret);
				return -EINVAL;
			}
		}
	}

	if (table->rate && table->rate != clk_get_rate(c)) {
		ret = clk_set_rate(c, table->rate);
		if (ret) {
			pr_warning("Unable to set clock %s to rate %lu: %d\n",
				table->name, table->rate, ret);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * If table refer pll directly it can be scaled only if all its children are OFF
 */
static bool tegra_can_scale_pll_direct(struct clk *pll)
{
	bool can_scale = true;
	struct clk *c;

	mutex_lock(&clock_list_lock);

	list_for_each_entry(c, &clocks, node) {
		if ((clk_get_parent(c) == pll) && (c->state == ON)) {
			WARN(1, "tegra: failed initialize %s: in use by %s\n",
			     pll->name, c->name);
			can_scale = false;
			break;
		}
	}
	mutex_unlock(&clock_list_lock);
	return can_scale;
}

/*
 * If table entry refer pll as cbus parent it can be scaled as long as all its
 * children are cbus users (that will be switched to cbus backup during scaling)
 */
static bool tegra_can_scale_pll_cbus(struct clk *pll)
{
	bool can_scale = true;
	struct clk *c;

	mutex_lock(&clock_list_lock);

	list_for_each_entry(c, &clocks, node) {
		if ((clk_get_parent(c) == pll) &&
		    !(c->flags & PERIPH_ON_CBUS)) {
			WARN(1, "tegra: failed initialize %s: in use by %s\n",
			     pll->name, c->name);
			can_scale = false;
			break;
		}
	}
	mutex_unlock(&clock_list_lock);
	return can_scale;
}

static int tegra_clk_init_cbus_pll_one(struct tegra_clk_init_table *table)
{
	bool can_scale = true;
	struct clk *pll;
	struct clk *c = tegra_get_clock_by_name(table->name);
	if (!c)
		return tegra_clk_init_one_from_table(table);

	if (c->flags & PERIPH_ON_CBUS) {
		/* table entry refer pllc/c2/c3 indirectly as cbus parent */
		pll = clk_get_parent(c);
		can_scale = tegra_can_scale_pll_cbus(pll);
	} else if (c->state == ON) {
		/* table entry refer pllc/c2/c3 directly, and it is ON */
		pll = c;
		can_scale = tegra_can_scale_pll_direct(pll);
	}

	if (can_scale)
		return tegra_clk_init_one_from_table(table);
	return -EBUSY;
}

void tegra_clk_init_cbus_plls_from_table(struct tegra_clk_init_table *table)
{
	for (; table->name; table++)
		tegra_clk_init_cbus_pll_one(table);
}

void tegra_clk_init_from_table(struct tegra_clk_init_table *table)
{
	for (; table->name; table++)
		tegra_clk_init_one_from_table(table);
}
EXPORT_SYMBOL(tegra_clk_init_from_table);

void tegra_periph_reset_deassert(struct clk *c)
{
	BUG_ON(!c->ops->reset);
	c->ops->reset(c, false);
}
EXPORT_SYMBOL(tegra_periph_reset_deassert);

void tegra_periph_reset_assert(struct clk *c)
{
	BUG_ON(!c->ops->reset);
	c->ops->reset(c, true);
}
EXPORT_SYMBOL(tegra_periph_reset_assert);

int tegra_is_clk_enabled(struct clk *c)
{
	return c->refcnt;
}
EXPORT_SYMBOL(tegra_is_clk_enabled);

int tegra_clk_shared_bus_update(struct clk *c)
{
	int ret = 0;
	unsigned long flags;

	clk_lock_save(c, &flags);

	if (c->ops && c->ops->shared_bus_update)
		ret = c->ops->shared_bus_update(c);

	clk_unlock_restore(c, &flags);
	return ret;
}

/* dvfs initialization may lower default maximum rate */
void tegra_init_max_rate(struct clk *c, unsigned long max_rate)
{
	struct clk *shared_bus_user;

	if (c->max_rate <= max_rate)
		return;

	/* skip message if shared bus user */
	if (!c->parent || !c->parent->ops || !c->parent->ops->shared_bus_update)
		pr_info("Lowering %s maximum rate from %lu to %lu\n",
			c->name, c->max_rate, max_rate);

	c->max_rate = max_rate;
	list_for_each_entry(shared_bus_user,
			    &c->shared_bus_list, u.shared_bus_user.node) {
		if (shared_bus_user->u.shared_bus_user.rate > max_rate)
			shared_bus_user->u.shared_bus_user.rate = max_rate;
		tegra_init_max_rate(shared_bus_user, max_rate);
	}
}

/* Use boot rate as emc monitor output until actual monitoring starts */
void __init tegra_clk_preset_emc_monitor(unsigned long rate)
{
	struct clk *c = tegra_get_clock_by_name("mon.emc");

	if (c) {
		c->u.shared_bus_user.rate = rate;
		clk_enable(c);
	}
}

static void __init tegra_clk_verify_rates(void)
{
	struct clk *c;
	unsigned long rate;

	mutex_lock(&clock_list_lock);

	list_for_each_entry(c, &clocks, node) {
		rate = clk_get_rate(c);
		if (rate > clk_get_max_rate(c))
			WARN(1, "tegra: %s boot rate %lu exceeds max rate %lu\n",
			     c->name, rate, clk_get_max_rate(c));
		c->boot_rate = rate;
	}
	mutex_unlock(&clock_list_lock);
}

void __init tegra_common_init_clock(void)
{
	tegra_cpu_timer_init();
	tegra_clk_verify_rates();
}

void __init tegra_clk_verify_parents(void)
{
	struct clk *c;
	struct clk *p;

	mutex_lock(&clock_list_lock);

	list_for_each_entry(c, &clocks, node) {
		p = clk_get_parent(c);
		if (!tegra_clk_is_parent_allowed(c, p))
			WARN(1, "tegra: parent %s is not allowed for %s\n",
			     p->name, c->name);
	}
	mutex_unlock(&clock_list_lock);
}

static bool tegra_keep_boot_clocks = false;
static int __init tegra_keep_boot_clocks_setup(char *__unused)
{
	tegra_keep_boot_clocks = true;
	return 1;
}
__setup("tegra_keep_boot_clocks", tegra_keep_boot_clocks_setup);

/*
 * Bootloader may not match kernel restrictions on CPU clock sources.
 * Make sure CPU clock is sourced from either main or backup parent.
 */
static int __init tegra_sync_cpu_clock(void)
{
	int ret;
	unsigned long rate;
	struct clk *c = tegra_get_clock_by_name("cpu");

	BUG_ON(!c);
	rate = clk_get_rate(c);
	ret = clk_set_rate(c, rate);
	if (ret)
		pr_err("%s: Failed to sync CPU at rate %lu\n", __func__, rate);
	else
		pr_info("CPU rate: %lu MHz\n", clk_get_rate(c) / 1000000);
	return ret;
}

/*
 * Iterate through all clocks, disabling any for which the refcount is 0
 * but the clock init detected the bootloader left the clock on.
 */
static int __init tegra_init_disable_boot_clocks(void)
{
#if DISABLE_BOOT_CLOCKS
	unsigned long flags;
	struct clk *c;

	mutex_lock(&clock_list_lock);

	list_for_each_entry(c, &clocks, node) {
		clk_lock_save(c, &flags);
		if (c->refcnt == 0 && c->state == ON &&
				c->ops && c->ops->disable) {
			pr_warn_once("%s clocks left on by bootloader:\n",
				tegra_keep_boot_clocks ?
					"Prevented disabling" :
					"Disabling");

			pr_warn("   %s\n", c->name);

			if (!tegra_keep_boot_clocks) {
				c->ops->disable(c);
				c->state = OFF;
			}
		}
		clk_unlock_restore(c, &flags);
	}

	mutex_unlock(&clock_list_lock);
#endif
	return 0;
}

/* Get ready DFLL clock source (if available) for CPU */
static int __init tegra_dfll_cpu_start(void)
{
	unsigned long flags;
	struct clk *c = tegra_get_clock_by_name("cpu");
	struct clk *dfll_cpu = tegra_get_clock_by_name("dfll_cpu");

	BUG_ON(!c);
	clk_lock_save(c, &flags);

	if (dfll_cpu && dfll_cpu->ops && dfll_cpu->ops->init)
		dfll_cpu->ops->init(dfll_cpu);

	clk_unlock_restore(c, &flags);
	return 0;
}

static int __init tegra_clk_late_init(void)
{
	tegra_init_disable_boot_clocks(); /* must before dvfs late init */
	if (!tegra_dvfs_late_init())
		tegra_dfll_cpu_start();	/* after successful dvfs init only */
	tegra_sync_cpu_clock();		/* after attempt to get dfll ready */
	tegra_recalculate_cpu_edp_limits();
	return 0;
}
late_initcall(tegra_clk_late_init);


/* Several extended clock configuration bits (e.g., clock routing, clock
 * phase control) are included in PLL and peripheral clock source
 * registers. */
int tegra_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	int ret = 0;
	unsigned long flags;

	clk_lock_save(c, &flags);

	if (!c->ops || !c->ops->clk_cfg_ex) {
		ret = -ENOSYS;
		goto out;
	}
	ret = c->ops->clk_cfg_ex(c, p, setting);

out:
	clk_unlock_restore(c, &flags);
	return ret;
}

int tegra_register_clk_rate_notifier(struct clk *c, struct notifier_block *nb)
{
	int ret;
	unsigned long flags;

	if (!c->rate_change_nh)
		return -ENOSYS;

	clk_lock_save(c, &flags);
	ret = raw_notifier_chain_register(c->rate_change_nh, nb);
	clk_unlock_restore(c, &flags);
	return ret;
}

void tegra_unregister_clk_rate_notifier(
	struct clk *c, struct notifier_block *nb)
{
	unsigned long flags;

	if (!c->rate_change_nh)
		return;

	clk_lock_save(c, &flags);
	raw_notifier_chain_unregister(c->rate_change_nh, nb);
	clk_unlock_restore(c, &flags);
}

#define OSC_FREQ_DET			0x58
#define OSC_FREQ_DET_TRIG		BIT(31)

#define OSC_FREQ_DET_STATUS		0x5C
#define OSC_FREQ_DET_BUSY		BIT(31)
#define OSC_FREQ_DET_CNT_MASK		0xFFFF

unsigned long tegra_clk_measure_input_freq(void)
{
	u32 clock_autodetect;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

	if (osc_freq)
		return osc_freq;

	writel(OSC_FREQ_DET_TRIG | 1, (u32)clk_base + OSC_FREQ_DET);
	do {} while (readl((u32)clk_base + OSC_FREQ_DET_STATUS) & OSC_FREQ_DET_BUSY);

	clock_autodetect = readl((u32)clk_base + OSC_FREQ_DET_STATUS);
	if (clock_autodetect >= 732 - 3 && clock_autodetect <= 732 + 3) {
		osc_freq = 12000000;
	} else if (clock_autodetect >= 794 - 3 && clock_autodetect <= 794 + 3) {
		osc_freq = 13000000;
	} else if (clock_autodetect >= 1172 - 3 && clock_autodetect <= 1172 + 3) {
		osc_freq = 19200000;
	} else if (clock_autodetect >= 1587 - 3 && clock_autodetect <= 1587 + 3) {
		osc_freq = 26000000;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	} else if (clock_autodetect >= 1025 - 3 && clock_autodetect <= 1025 + 3) {
		osc_freq = 16800000;
	} else if (clock_autodetect >= 2344 - 3 && clock_autodetect <= 2344 + 3) {
		osc_freq = 38400000;
	} else if (clock_autodetect >= 2928 - 3 && clock_autodetect <= 2928 + 3) {
		osc_freq = 48000000;
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	} else if (tegra_revision == TEGRA_REVISION_QT) {
		if (clock_autodetect >= 2 && clock_autodetect <= 9)
			osc_freq = 115200;
		else if (clock_autodetect >= 13 && clock_autodetect <= 15)
			osc_freq = 230400;
#endif
#endif
	} else {
		pr_err("%s: Unexpected clock autodetect value %d", __func__, clock_autodetect);
	}

	BUG_ON(osc_freq == 0);

	return osc_freq;
}

#ifdef CONFIG_DEBUG_FS

/*
 * Attempt to lock all the clocks that are marked cansleep
 * Must be called with irqs enabled
 */
static int __clk_lock_all_mutexes(void)
{
	struct clk *c;

	might_sleep();

	list_for_each_entry(c, &clocks, node)
		if (clk_cansleep(c))
			if (!mutex_trylock(&c->mutex))
				goto unlock_mutexes;

	return 0;

unlock_mutexes:
	list_for_each_entry_continue_reverse(c, &clocks, node)
		if (clk_cansleep(c))
			mutex_unlock(&c->mutex);

	return -EAGAIN;
}

/*
 * Attempt to lock all the clocks that are not marked cansleep
 * Must be called with irqs disabled
 */
static int __clk_lock_all_spinlocks(void)
{
	struct clk *c;

	list_for_each_entry(c, &clocks, node)
		if (!clk_cansleep(c))
			if (!spin_trylock(&c->spinlock))
				goto unlock_spinlocks;

	return 0;

unlock_spinlocks:
	list_for_each_entry_continue_reverse(c, &clocks, node)
		if (!clk_cansleep(c))
			spin_unlock(&c->spinlock);

	return -EAGAIN;
}

static void __clk_unlock_all_mutexes(void)
{
	struct clk *c;

	list_for_each_entry_reverse(c, &clocks, node)
		if (clk_cansleep(c))
			mutex_unlock(&c->mutex);
}

static void __clk_unlock_all_spinlocks(void)
{
	struct clk *c;

	list_for_each_entry_reverse(c, &clocks, node)
		if (!clk_cansleep(c))
			spin_unlock(&c->spinlock);
}

/*
 * This function retries until it can take all locks, and may take
 * an arbitrarily long time to complete.
 * Must be called with irqs enabled, returns with irqs disabled
 * Must be called with clock_list_lock held
 */
static void clk_lock_all(void)
{
	int ret;
retry:
	ret = __clk_lock_all_mutexes();
	if (ret)
		goto failed_mutexes;

	local_irq_disable();

	ret = __clk_lock_all_spinlocks();
	if (ret)
		goto failed_spinlocks;

	/* All locks taken successfully, return */
	return;

failed_spinlocks:
	local_irq_enable();
	__clk_unlock_all_mutexes();
failed_mutexes:
	msleep(1);
	goto retry;
}

/*
 * Unlocks all clocks after a clk_lock_all
 * Must be called with irqs disabled, returns with irqs enabled
 * Must be called with clock_list_lock held
 */
static void clk_unlock_all(void)
{
	__clk_unlock_all_spinlocks();

	local_irq_enable();

	__clk_unlock_all_mutexes();
}

static struct dentry *clk_debugfs_root;

static void dvfs_show_one(struct seq_file *s, struct dvfs *d, int level)
{
	seq_printf(s, "%*s  %-*s%21s%d mV\n",
			level * 3 + 1, "",
			30 - level * 3, d->dvfs_rail->reg_id,
			"",
			d->cur_millivolts);
}

static void clock_tree_show_one(struct seq_file *s, struct clk *c, int level)
{
	struct clk *child;
	const char *state = "uninit";
	char div[8] = {0};
	unsigned long rate = clk_get_rate_all_locked(c);
	unsigned long max_rate = clk_get_max_rate(c);;

	if (c->state == ON)
		state = "on";
	else if (c->state == OFF)
		state = "off";

	if (c->mul != 0 && c->div != 0) {
		if (c->mul > c->div) {
			int mul = c->mul / c->div;
			int mul2 = (c->mul * 10 / c->div) % 10;
			int mul3 = (c->mul * 10) % c->div;
			if (mul2 == 0 && mul3 == 0)
				snprintf(div, sizeof(div), "x%d", mul);
			else if (mul3 == 0)
				snprintf(div, sizeof(div), "x%d.%d", mul, mul2);
			else
				snprintf(div, sizeof(div), "x%d.%d..", mul, mul2);
		} else {
			snprintf(div, sizeof(div), "%d%s", c->div / c->mul,
				(c->div % c->mul) ? ".5" : "");
		}
	}

	seq_printf(s, "%*s%c%c%-*s%c %-6s %-3d %-8s %-10lu",
		level * 3 + 1, "",
		rate > max_rate ? '!' : ' ',
		!c->set ? '*' : ' ',
		30 - level * 3, c->name,
		c->cansleep ? '$' : ' ',
		state, c->refcnt, div, rate);
	if (c->parent && !list_empty(&c->parent->shared_bus_list))
		seq_printf(s, " (%lu%s)", c->u.shared_bus_user.rate,
			   c->u.shared_bus_user.mode == SHARED_CEILING ? "^" :
			   c->u.shared_bus_user.mode == SHARED_ISO_BW ? "+" :
			   c->u.shared_bus_user.mode == SHARED_BW ? "+" : "");
	seq_printf(s, "\n");

	if (c->dvfs)
		dvfs_show_one(s, c->dvfs, level + 1);

	list_for_each_entry(child, &clocks, node) {
		if (child->parent != c)
			continue;

		clock_tree_show_one(s, child, level + 1);
	}
}

static int clock_tree_show(struct seq_file *s, void *data)
{
	struct clk *c;
	seq_printf(s, "   clock                          state  ref div      rate       (shared rate)\n");
	seq_printf(s, "------------------------------------------------------------------------------\n");

	mutex_lock(&clock_list_lock);
#ifndef CONFIG_TEGRA_FPGA_PLATFORM
	clk_lock_all();
#endif
	list_for_each_entry(c, &clocks, node)
		if (c->parent == NULL)
			clock_tree_show_one(s, c, 0);
#ifndef CONFIG_TEGRA_FPGA_PLATFORM
	clk_unlock_all();
#endif
	mutex_unlock(&clock_list_lock);
	return 0;
}

static int clock_tree_open(struct inode *inode, struct file *file)
{
	return single_open(file, clock_tree_show, inode->i_private);
}

static const struct file_operations clock_tree_fops = {
	.open		= clock_tree_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void syncevent_one(struct clk *c)
{
	struct clk *child;

	if (c->state == ON)
		trace_clock_enable(c->name, 1, smp_processor_id());
	else
		trace_clock_disable(c->name, 0, smp_processor_id());

	trace_clock_set_rate(c->name, clk_get_rate_all_locked(c),
				smp_processor_id());

	list_for_each_entry(child, &clocks, node) {
		if (child->parent != c)
			continue;

		syncevent_one(child);
	}
}

static int syncevent_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct clk *c;
	char buffer[40];
	int buf_size;

	memset(buffer, 0, sizeof(buffer));
	buf_size = min(count, (sizeof(buffer)-1));

	if (copy_from_user(buffer, user_buf, buf_size))
		return -EFAULT;

	if (!strnicmp("all", buffer, 3)) {
		mutex_lock(&clock_list_lock);

		clk_lock_all();

		list_for_each_entry(c, &clocks, node) {
			if (c->parent == NULL)
				syncevent_one(c);
		}

		clk_unlock_all();

		mutex_unlock(&clock_list_lock);
	}

	return count;
}

static const struct file_operations syncevent_fops = {
	.write		= syncevent_write,
};

static int possible_parents_show(struct seq_file *s, void *data)
{
	struct clk *c = s->private;
	int i;
	bool first = true;

	for (i = 0; c->inputs[i].input; i++) {
		if (tegra_clk_is_parent_allowed(c, c->inputs[i].input)) {
			seq_printf(s, "%s%s", first ? "" : " ",
				   c->inputs[i].input->name);
			first = false;
		}
	}
	seq_printf(s, "\n");
	return 0;
}

static int possible_parents_open(struct inode *inode, struct file *file)
{
	return single_open(file, possible_parents_show, inode->i_private);
}

static const struct file_operations possible_parents_fops = {
	.open		= possible_parents_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int parent_show(struct seq_file *s, void *data)
{
	struct clk *c = s->private;
	struct clk *p = clk_get_parent(c);

	seq_printf(s, "%s\n", p ? p->name : "clk_root");
	return 0;
}

static int parent_open(struct inode *inode, struct file *file)
{
	return single_open(file, parent_show, inode->i_private);
}

static int rate_get(void *data, u64 *val)
{
	struct clk *c = (struct clk *)data;
	*val = (u64)clk_get_rate(c);
	return 0;
}

static int state_get(void *data, u64 *val)
{
	struct clk *c = (struct clk *)data;
	*val = (u64)((c->state == ON) ? 1 : 0);
	return 0;
}

#ifdef CONFIG_TEGRA_CLOCK_DEBUG_WRITE

static const mode_t parent_rate_mode =  S_IRUGO | S_IWUSR;

static ssize_t parent_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct clk *c = s->private;
	struct clk *p = NULL;
	char buf[32];

	if (sizeof(buf) <= count)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count]='\0';
	strim(buf);

	p = tegra_get_clock_by_name(buf);
	if (!p)
		return -EINVAL;

	if (clk_set_parent(c, p))
		return -EINVAL;

	return count;
}

static const struct file_operations parent_fops = {
	.open		= parent_open,
	.read		= seq_read,
	.write		= parent_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int rate_set(void *data, u64 val)
{
	struct clk *c = (struct clk *)data;
	return clk_set_rate(c, (unsigned long)val);
}
DEFINE_SIMPLE_ATTRIBUTE(rate_fops, rate_get, rate_set, "%llu\n");

static int state_set(void *data, u64 val)
{
	struct clk *c = (struct clk *)data;

	if (val)
		return tegra_clk_prepare_enable(c);
	else {
		tegra_clk_disable_unprepare(c);
		return 0;
	}
}
DEFINE_SIMPLE_ATTRIBUTE(state_fops, state_get, state_set, "%llu\n");

#else

static const mode_t parent_rate_mode =  S_IRUGO;

static const struct file_operations parent_fops = {
	.open		= parent_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

DEFINE_SIMPLE_ATTRIBUTE(rate_fops, rate_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(state_fops, state_get, NULL, "%llu\n");
#endif

static int time_on_get(void *data, u64 *val)
{
	unsigned long flags;
	struct clk *c = (struct clk *)data;

	clk_lock_save(c, &flags);
	clk_stats_update(c);
	*val = cputime64_to_clock_t(c->stats.time_on);
	clk_unlock_restore(c, &flags);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(time_on_fops, time_on_get, NULL, "%llu\n");

static int possible_rates_show(struct seq_file *s, void *data)
{
	struct clk *c = s->private;
	long rate = 0;

	/* shared bus clock must round up, unless top of range reached */
	while (rate <= c->max_rate) {
		long rounded_rate = c->ops->round_rate(c, rate);
		if (IS_ERR_VALUE(rounded_rate) || (rounded_rate <= rate))
			break;

		rate = rounded_rate + 2000;	/* 2kHz resolution */
		seq_printf(s, "%ld ", rounded_rate / 1000);
	}
	seq_printf(s, "(kHz)\n");
	return 0;
}

static int possible_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, possible_rates_show, inode->i_private);
}

static const struct file_operations possible_rates_fops = {
	.open		= possible_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int clk_debugfs_register_one(struct clk *c)
{
	struct dentry *d;

	d = debugfs_create_dir(c->name, clk_debugfs_root);
	if (!d)
		return -ENOMEM;
	c->dent = d;

	d = debugfs_create_u8("refcnt", S_IRUGO, c->dent, (u8 *)&c->refcnt);
	if (!d)
		goto err_out;

	d = debugfs_create_x32("flags", S_IRUGO, c->dent, (u32 *)&c->flags);
	if (!d)
		goto err_out;

	d = debugfs_create_u32(
		"max", parent_rate_mode, c->dent, (u32 *)&c->max_rate);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("min", S_IRUGO, c->dent, (u32 *)&c->min_rate);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"parent", parent_rate_mode, c->dent, c, &parent_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"rate", parent_rate_mode, c->dent, c, &rate_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"state", parent_rate_mode, c->dent, c, &state_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"time_on", S_IRUGO, c->dent, c, &time_on_fops);
	if (!d)
		goto err_out;

	if (c->inputs) {
		d = debugfs_create_file("possible_parents", S_IRUGO, c->dent,
			c, &possible_parents_fops);
		if (!d)
			goto err_out;
	}

	if (c->ops && c->ops->round_rate && c->ops->shared_bus_update) {
		d = debugfs_create_file("possible_rates", S_IRUGO, c->dent,
			c, &possible_rates_fops);
		if (!d)
			goto err_out;
	}

	return 0;

err_out:
	debugfs_remove_recursive(c->dent);
	return -ENOMEM;
}

static int clk_debugfs_register(struct clk *c)
{
	int err;
	struct clk *pa = c->parent;

	if (pa && !pa->dent) {
		err = clk_debugfs_register(pa);
		if (err)
			return err;
	}

	if (!c->dent) {
		err = clk_debugfs_register_one(c);
		if (err)
			return err;
	}
	return 0;
}

static int __init clk_debugfs_init(void)
{
	struct clk *c;
	struct dentry *d;
	int err = -ENOMEM;

	d = debugfs_create_dir("clock", NULL);
	if (!d)
		return -ENOMEM;
	clk_debugfs_root = d;

	d = debugfs_create_file("clock_tree", S_IRUGO, clk_debugfs_root, NULL,
		&clock_tree_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file("syncevents", S_IRUGO|S_IWUSR, clk_debugfs_root, NULL,
		&syncevent_fops);

	if (!d || dvfs_debugfs_init(clk_debugfs_root))
		goto err_out;

	list_for_each_entry(c, &clocks, node) {
		err = clk_debugfs_register(c);
		if (err)
			goto err_out;
	}
	return 0;
err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return err;
}

late_initcall(clk_debugfs_init);
#endif
