/*
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Standard functionality for the common clock API.  See Documentation/clk.txt
 */

#define pr_fmt(fmt) "clk: " fmt

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/clk-conf.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/clkdev.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include "clk.h"

#if defined(CONFIG_COMMON_CLK)

static DEFINE_SPINLOCK(enable_lock);
static DEFINE_MUTEX(prepare_lock);

static struct task_struct *prepare_owner;
static struct task_struct *enable_owner;

static int prepare_refcnt;
static int enable_refcnt;

static HLIST_HEAD(clk_root_list);
static HLIST_HEAD(clk_orphan_list);
static LIST_HEAD(clk_notifier_list);

struct clk_handoff_vdd {
	struct list_head list;
	struct clk_vdd_class *vdd_class;
};

static LIST_HEAD(clk_handoff_vdd_list);

/***    private data structures    ***/

struct clk_core {
	const char		*name;
	const struct clk_ops	*ops;
	struct clk_hw		*hw;
	struct module		*owner;
	struct clk_core		*parent;
	const char		**parent_names;
	struct clk_core		**parents;
	u8			num_parents;
	u8			new_parent_index;
	unsigned long		rate;
	unsigned long		req_rate;
	unsigned long		new_rate;
	struct clk_core		*new_parent;
	struct clk_core		*new_child;
	unsigned long		flags;
	bool			orphan;
	unsigned int		enable_count;
	unsigned int		prepare_count;
	bool			need_handoff_enable;
	bool			need_handoff_prepare;
	unsigned long		min_rate;
	unsigned long		max_rate;
	unsigned long		accuracy;
	int			phase;
	struct hlist_head	children;
	struct hlist_node	child_node;
	struct hlist_head	clks;
	unsigned int		notifier_count;
#ifdef CONFIG_DEBUG_FS
	struct dentry		*dentry;
	struct hlist_node	debug_node;
#endif
	struct kref		ref;
	struct clk_vdd_class	*vdd_class;
	unsigned long		*rate_max;
	int			num_rate_max;
};

#define CREATE_TRACE_POINTS
#include <trace/events/clk.h>

struct clk {
	struct clk_core	*core;
	const char *dev_id;
	const char *con_id;
	unsigned long min_rate;
	unsigned long max_rate;
	struct hlist_node clks_node;
};

/***           locking             ***/
static void clk_prepare_lock(void)
{
	if (!mutex_trylock(&prepare_lock)) {
		if (prepare_owner == current) {
			prepare_refcnt++;
			return;
		}
		mutex_lock(&prepare_lock);
	}
	WARN_ON_ONCE(prepare_owner != NULL);
	WARN_ON_ONCE(prepare_refcnt != 0);
	prepare_owner = current;
	prepare_refcnt = 1;
}

static void clk_prepare_unlock(void)
{
	WARN_ON_ONCE(prepare_owner != current);
	WARN_ON_ONCE(prepare_refcnt == 0);

	if (--prepare_refcnt)
		return;
	prepare_owner = NULL;
	mutex_unlock(&prepare_lock);
}

static unsigned long clk_enable_lock(void)
	__acquires(enable_lock)
{
	unsigned long flags;

	if (!spin_trylock_irqsave(&enable_lock, flags)) {
		if (enable_owner == current) {
			enable_refcnt++;
			__acquire(enable_lock);
			return flags;
		}
		spin_lock_irqsave(&enable_lock, flags);
	}
	WARN_ON_ONCE(enable_owner != NULL);
	WARN_ON_ONCE(enable_refcnt != 0);
	enable_owner = current;
	enable_refcnt = 1;
	return flags;
}

static void clk_enable_unlock(unsigned long flags)
	__releases(enable_lock)
{
	WARN_ON_ONCE(enable_owner != current);
	WARN_ON_ONCE(enable_refcnt == 0);

	if (--enable_refcnt) {
		__release(enable_lock);
		return;
	}
	enable_owner = NULL;
	spin_unlock_irqrestore(&enable_lock, flags);
}

static bool clk_core_is_prepared(struct clk_core *core)
{
	/*
	 * .is_prepared is optional for clocks that can prepare
	 * fall back to software usage counter if it is missing
	 */
	if (!core->ops->is_prepared)
		return core->prepare_count;

	return core->ops->is_prepared(core->hw);
}

static bool clk_core_is_enabled(struct clk_core *core)
{
	/*
	 * .is_enabled is only mandatory for clocks that gate
	 * fall back to software usage counter if .is_enabled is missing
	 */
	if (!core->ops->is_enabled)
		return core->enable_count;

	return core->ops->is_enabled(core->hw);
}

/***    helper functions   ***/

const char *__clk_get_name(const struct clk *clk)
{
	return !clk ? NULL : clk->core->name;
}
EXPORT_SYMBOL_GPL(__clk_get_name);

const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return hw->core->name;
}
EXPORT_SYMBOL_GPL(clk_hw_get_name);

struct clk_hw *__clk_get_hw(struct clk *clk)
{
	return !clk ? NULL : clk->core->hw;
}
EXPORT_SYMBOL_GPL(__clk_get_hw);

unsigned int clk_hw_get_num_parents(const struct clk_hw *hw)
{
	return hw->core->num_parents;
}
EXPORT_SYMBOL_GPL(clk_hw_get_num_parents);

struct clk_hw *clk_hw_get_parent(const struct clk_hw *hw)
{
	return hw->core->parent ? hw->core->parent->hw : NULL;
}
EXPORT_SYMBOL_GPL(clk_hw_get_parent);

static struct clk_core *__clk_lookup_subtree(const char *name,
					     struct clk_core *core)
{
	struct clk_core *child;
	struct clk_core *ret;

	if (!strcmp(core->name, name))
		return core;

	hlist_for_each_entry(child, &core->children, child_node) {
		ret = __clk_lookup_subtree(name, child);
		if (ret)
			return ret;
	}

	return NULL;
}

static struct clk_core *clk_core_lookup(const char *name)
{
	struct clk_core *root_clk;
	struct clk_core *ret;

	if (!name)
		return NULL;

	/* search the 'proper' clk tree first */
	hlist_for_each_entry(root_clk, &clk_root_list, child_node) {
		ret = __clk_lookup_subtree(name, root_clk);
		if (ret)
			return ret;
	}

	/* if not found, then search the orphan tree */
	hlist_for_each_entry(root_clk, &clk_orphan_list, child_node) {
		ret = __clk_lookup_subtree(name, root_clk);
		if (ret)
			return ret;
	}

	return NULL;
}

static struct clk_core *clk_core_get_parent_by_index(struct clk_core *core,
							 u8 index)
{
	if (!core || index >= core->num_parents)
		return NULL;

	if (!core->parents[index])
		core->parents[index] =
				clk_core_lookup(core->parent_names[index]);

	return core->parents[index];
}

struct clk_hw *
clk_hw_get_parent_by_index(const struct clk_hw *hw, unsigned int index)
{
	struct clk_core *parent;

	parent = clk_core_get_parent_by_index(hw->core, index);

	return !parent ? NULL : parent->hw;
}
EXPORT_SYMBOL_GPL(clk_hw_get_parent_by_index);

unsigned int __clk_get_enable_count(struct clk *clk)
{
	return !clk ? 0 : clk->core->enable_count;
}

static unsigned long clk_core_get_rate_nolock(struct clk_core *core)
{
	unsigned long ret;

	if (!core) {
		ret = 0;
		goto out;
	}

	ret = core->rate;

	if (!core->num_parents)
		goto out;

	if (!core->parent)
		ret = 0;

out:
	return ret;
}

unsigned long clk_hw_get_rate(const struct clk_hw *hw)
{
	return clk_core_get_rate_nolock(hw->core);
}
EXPORT_SYMBOL_GPL(clk_hw_get_rate);

static unsigned long __clk_get_accuracy(struct clk_core *core)
{
	if (!core)
		return 0;

	return core->accuracy;
}

unsigned long __clk_get_flags(struct clk *clk)
{
	return !clk ? 0 : clk->core->flags;
}
EXPORT_SYMBOL_GPL(__clk_get_flags);

unsigned long clk_hw_get_flags(const struct clk_hw *hw)
{
	return hw->core->flags;
}
EXPORT_SYMBOL_GPL(clk_hw_get_flags);

bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return clk_core_is_prepared(hw->core);
}

bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return clk_core_is_enabled(hw->core);
}

bool __clk_is_enabled(struct clk *clk)
{
	if (!clk)
		return false;

	return clk_core_is_enabled(clk->core);
}
EXPORT_SYMBOL_GPL(__clk_is_enabled);

static bool mux_is_better_rate(unsigned long rate, unsigned long now,
			   unsigned long best, unsigned long flags)
{
	if (flags & CLK_MUX_ROUND_CLOSEST)
		return abs(now - rate) < abs(best - rate);

	return now <= rate && now > best;
}

static int
clk_mux_determine_rate_flags(struct clk_hw *hw, struct clk_rate_request *req,
			     unsigned long flags)
{
	struct clk_core *core = hw->core, *parent, *best_parent = NULL;
	int i, num_parents, ret;
	unsigned long best = 0;
	struct clk_rate_request parent_req = *req;

	/* if NO_REPARENT flag set, pass through to current parent */
	if (core->flags & CLK_SET_RATE_NO_REPARENT) {
		parent = core->parent;
		if (core->flags & CLK_SET_RATE_PARENT) {
			ret = __clk_determine_rate(parent ? parent->hw : NULL,
						   &parent_req);
			if (ret)
				return ret;

			best = parent_req.rate;
		} else if (parent) {
			best = clk_core_get_rate_nolock(parent);
		} else {
			best = clk_core_get_rate_nolock(core);
		}

		goto out;
	}

	/* find the parent that can provide the fastest rate <= rate */
	num_parents = core->num_parents;
	for (i = 0; i < num_parents; i++) {
		parent = clk_core_get_parent_by_index(core, i);
		if (!parent)
			continue;

		if (core->flags & CLK_SET_RATE_PARENT) {
			parent_req = *req;
			ret = __clk_determine_rate(parent->hw, &parent_req);
			if (ret)
				continue;
		} else {
			parent_req.rate = clk_core_get_rate_nolock(parent);
		}

		if (mux_is_better_rate(req->rate, parent_req.rate,
				       best, flags)) {
			best_parent = parent;
			best = parent_req.rate;
		}
	}

	if (!best_parent)
		return -EINVAL;

out:
	if (best_parent)
		req->best_parent_hw = best_parent->hw;
	req->best_parent_rate = best;
	req->rate = best;

	return 0;
}

struct clk *__clk_lookup(const char *name)
{
	struct clk_core *core = clk_core_lookup(name);

	return !core ? NULL : core->hw->clk;
}

static void clk_core_get_boundaries(struct clk_core *core,
				    unsigned long *min_rate,
				    unsigned long *max_rate)
{
	struct clk *clk_user;

	*min_rate = core->min_rate;
	*max_rate = core->max_rate;

	hlist_for_each_entry(clk_user, &core->clks, clks_node)
		*min_rate = max(*min_rate, clk_user->min_rate);

	hlist_for_each_entry(clk_user, &core->clks, clks_node)
		*max_rate = min(*max_rate, clk_user->max_rate);
}

void clk_hw_set_rate_range(struct clk_hw *hw, unsigned long min_rate,
			   unsigned long max_rate)
{
	hw->core->min_rate = min_rate;
	hw->core->max_rate = max_rate;
}
EXPORT_SYMBOL_GPL(clk_hw_set_rate_range);

/*
 * Aggregate the rate of all child nodes which are enabled and exclude the
 * child node which requests for clk_aggregate_rate.
 */
unsigned long clk_aggregate_rate(struct clk_hw *hw,
					const struct clk_core *parent)
{
	struct clk_core *child;
	unsigned long aggre_rate = 0;

	hlist_for_each_entry(child, &parent->children, child_node) {
		if (child->enable_count &&
				strcmp(child->name, hw->init->name))
			aggre_rate = max(child->rate, aggre_rate);
	}

	return aggre_rate;
}
EXPORT_SYMBOL_GPL(clk_aggregate_rate);

/*
 * Helper for finding best parent to provide a given frequency. This can be used
 * directly as a determine_rate callback (e.g. for a mux), or from a more
 * complex clock that may combine a mux with other operations.
 */
int __clk_mux_determine_rate(struct clk_hw *hw,
			     struct clk_rate_request *req)
{
	return clk_mux_determine_rate_flags(hw, req, 0);
}
EXPORT_SYMBOL_GPL(__clk_mux_determine_rate);

int __clk_mux_determine_rate_closest(struct clk_hw *hw,
				     struct clk_rate_request *req)
{
	return clk_mux_determine_rate_flags(hw, req, CLK_MUX_ROUND_CLOSEST);
}
EXPORT_SYMBOL_GPL(__clk_mux_determine_rate_closest);

/*
 *  Find the voltage level required for a given clock rate.
 */
static int clk_find_vdd_level(struct clk_core *clk, unsigned long rate)
{
	int level;

	/*
	 * For certain PLLs, due to the limitation in the bits allocated for
	 * programming the fractional divider, the actual rate of the PLL will
	 * be slightly higher than the requested rate (in the order of several
	 * Hz). To accommodate this difference, convert the FMAX rate and the
	 * clock frequency to KHz and use that for deriving the voltage level.
	 */
	for (level = 0; level < clk->num_rate_max; level++)
		if (DIV_ROUND_CLOSEST(rate, 1000) <=
				DIV_ROUND_CLOSEST(clk->rate_max[level], 1000))
			break;

	if (level == clk->num_rate_max) {
		pr_err("Rate %lu for %s is greater than highest Fmax\n", rate,
				clk->name);
		return -EINVAL;
	}

	return level;
}

/*
 * Update voltage level given the current votes.
 */
static int clk_update_vdd(struct clk_vdd_class *vdd_class)
{
	int level, rc = 0, i, ignore;
	struct regulator **r = vdd_class->regulator;
	int *uv = vdd_class->vdd_uv;
	int n_reg = vdd_class->num_regulators;
	int cur_lvl = vdd_class->cur_level;
	int max_lvl = vdd_class->num_levels - 1;
	int cur_base = cur_lvl * n_reg;
	int new_base;

	/* aggregate votes */
	for (level = max_lvl; level > 0; level--)
		if (vdd_class->level_votes[level])
			break;

	if (level == cur_lvl)
		return 0;

	max_lvl = max_lvl * n_reg;
	new_base = level * n_reg;

	for (i = 0; i < vdd_class->num_regulators; i++) {
		pr_debug("Set Voltage level Min %d, Max %d\n", uv[new_base + i],
				uv[max_lvl + i]);
		rc = regulator_set_voltage(r[i], uv[new_base + i],
			vdd_class->use_max_uV ? INT_MAX : uv[max_lvl + i]);
		if (rc)
			goto set_voltage_fail;

		if (cur_lvl == 0 || cur_lvl == vdd_class->num_levels)
			rc = regulator_enable(r[i]);
		else if (level == 0)
			rc = regulator_disable(r[i]);
		if (rc)
			goto enable_disable_fail;
	}

	if (vdd_class->set_vdd && !vdd_class->num_regulators)
		rc = vdd_class->set_vdd(vdd_class, level);

	if (!rc)
		vdd_class->cur_level = level;

	return rc;

enable_disable_fail:
	regulator_set_voltage(r[i], uv[cur_base + i],
			vdd_class->use_max_uV ? INT_MAX : uv[max_lvl + i]);

set_voltage_fail:
	for (i--; i >= 0; i--) {
		regulator_set_voltage(r[i], uv[cur_base + i],
		       vdd_class->use_max_uV ? INT_MAX : uv[max_lvl + i]);
		if (cur_lvl == 0 || cur_lvl == vdd_class->num_levels)
			regulator_disable(r[i]);
		else if (level == 0)
			ignore = regulator_enable(r[i]);
	}

	return rc;
}

/*
 *  Vote for a voltage level.
 */
static int clk_vote_vdd_level(struct clk_vdd_class *vdd_class, int level)
{
	int rc = 0;

	if (level >= vdd_class->num_levels)
		return -EINVAL;

	mutex_lock(&vdd_class->lock);

	vdd_class->level_votes[level]++;

	rc = clk_update_vdd(vdd_class);
	if (rc)
		vdd_class->level_votes[level]--;

	mutex_unlock(&vdd_class->lock);

	return rc;
}

/*
 * Remove vote for a voltage level.
 */
static int clk_unvote_vdd_level(struct clk_vdd_class *vdd_class, int level)
{
	int rc = 0;

	if (level >= vdd_class->num_levels)
		return -EINVAL;

	mutex_lock(&vdd_class->lock);

	if (WARN(!vdd_class->level_votes[level],
				"Reference counts are incorrect for %s level %d\n",
				vdd_class->class_name, level))
		goto out;

	vdd_class->level_votes[level]--;

	rc = clk_update_vdd(vdd_class);
	if (rc)
		vdd_class->level_votes[level]++;

out:
	mutex_unlock(&vdd_class->lock);
	return rc;
}

/*
 * Vote for a voltage level corresponding to a clock's rate.
 */
static int clk_vote_rate_vdd(struct clk_core *core, unsigned long rate)
{
	int level;

	if (!core->vdd_class)
		return 0;

	level = clk_find_vdd_level(core, rate);
	if (level < 0)
		return level;

	return clk_vote_vdd_level(core->vdd_class, level);
}

/*
 * Remove vote for a voltage level corresponding to a clock's rate.
 */
static void clk_unvote_rate_vdd(struct clk_core *core, unsigned long rate)
{
	int level;

	if (!core->vdd_class)
		return;

	level = clk_find_vdd_level(core, rate);
	if (level < 0)
		return;

	clk_unvote_vdd_level(core->vdd_class, level);
}

static bool clk_is_rate_level_valid(struct clk_core *core, unsigned long rate)
{
	int level;

	if (!core->vdd_class)
		return true;

	level = clk_find_vdd_level(core, rate);

	return level >= 0;
}

static int clk_vdd_class_init(struct clk_vdd_class *vdd)
{
	struct clk_handoff_vdd *v;

	if (vdd->skip_handoff)
		return 0;

	list_for_each_entry(v, &clk_handoff_vdd_list, list) {
		if (v->vdd_class == vdd)
			return 0;
	}

	pr_debug("voting for vdd_class %s\n", vdd->class_name);

	if (clk_vote_vdd_level(vdd, vdd->num_levels - 1))
		pr_err("failed to vote for %s\n", vdd->class_name);

	v = kmalloc(sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;

	v->vdd_class = vdd;

	list_add_tail(&v->list, &clk_handoff_vdd_list);

	return 0;
}

/***        clk api        ***/

static void clk_core_unprepare(struct clk_core *core)
{
	lockdep_assert_held(&prepare_lock);

	if (!core)
		return;

	if (WARN_ON(core->prepare_count == 0))
		return;

	if (WARN_ON(core->prepare_count == 1 && core->flags & CLK_IS_CRITICAL))
		return;

	if (--core->prepare_count > 0)
		return;

	WARN_ON(core->enable_count > 0);

	trace_clk_unprepare(core);

	if (core->ops->unprepare)
		core->ops->unprepare(core->hw);

	trace_clk_unprepare_complete(core);

	clk_unvote_rate_vdd(core, core->rate);

	clk_core_unprepare(core->parent);
}

static void clk_core_unprepare_lock(struct clk_core *core)
{
	clk_prepare_lock();
	clk_core_unprepare(core);
	clk_prepare_unlock();
}

/**
 * clk_unprepare - undo preparation of a clock source
 * @clk: the clk being unprepared
 *
 * clk_unprepare may sleep, which differentiates it from clk_disable.  In a
 * simple case, clk_unprepare can be used instead of clk_disable to gate a clk
 * if the operation may sleep.  One example is a clk which is accessed over
 * I2c.  In the complex case a clk gate operation may require a fast and a slow
 * part.  It is this reason that clk_unprepare and clk_disable are not mutually
 * exclusive.  In fact clk_disable must be called before clk_unprepare.
 */
void clk_unprepare(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	clk_core_unprepare_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_unprepare);

static int clk_core_prepare(struct clk_core *core)
{
	int ret = 0;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	if (core->prepare_count == 0) {
		ret = clk_core_prepare(core->parent);
		if (ret)
			return ret;

		trace_clk_prepare(core);

		ret = clk_vote_rate_vdd(core, core->rate);
		if (ret) {
			clk_core_unprepare(core->parent);
			return ret;
		}

		if (core->ops->prepare)
			ret = core->ops->prepare(core->hw);

		trace_clk_prepare_complete(core);

		if (ret) {
			clk_unvote_rate_vdd(core, core->rate);
			clk_core_unprepare(core->parent);
			return ret;
		}
	}

	core->prepare_count++;

	return 0;
}

static int clk_core_prepare_lock(struct clk_core *core)
{
	int ret;

	clk_prepare_lock();
	ret = clk_core_prepare(core);
	clk_prepare_unlock();

	return ret;
}

/**
 * clk_prepare - prepare a clock source
 * @clk: the clk being prepared
 *
 * clk_prepare may sleep, which differentiates it from clk_enable.  In a simple
 * case, clk_prepare can be used instead of clk_enable to ungate a clk if the
 * operation may sleep.  One example is a clk which is accessed over I2c.  In
 * the complex case a clk ungate operation may require a fast and a slow part.
 * It is this reason that clk_prepare and clk_enable are not mutually
 * exclusive.  In fact clk_prepare must be called before clk_enable.
 * Returns 0 on success, -EERROR otherwise.
 */
int clk_prepare(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_prepare_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_prepare);

static void clk_core_disable(struct clk_core *core)
{
	lockdep_assert_held(&enable_lock);

	if (!core)
		return;

	if (WARN_ON(core->enable_count == 0))
		return;

	if (WARN_ON(core->enable_count == 1 && core->flags & CLK_IS_CRITICAL))
		return;

	if (--core->enable_count > 0)
		return;

	trace_clk_disable_rcuidle(core);

	if (core->ops->disable)
		core->ops->disable(core->hw);

	trace_clk_disable_complete_rcuidle(core);

	clk_core_disable(core->parent);
}

static void clk_core_disable_lock(struct clk_core *core)
{
	unsigned long flags;

	flags = clk_enable_lock();
	clk_core_disable(core);
	clk_enable_unlock(flags);
}

/**
 * clk_disable - gate a clock
 * @clk: the clk being gated
 *
 * clk_disable must not sleep, which differentiates it from clk_unprepare.  In
 * a simple case, clk_disable can be used instead of clk_unprepare to gate a
 * clk if the operation is fast and will never sleep.  One example is a
 * SoC-internal clk which is controlled via simple register writes.  In the
 * complex case a clk gate operation may require a fast and a slow part.  It is
 * this reason that clk_unprepare and clk_disable are not mutually exclusive.
 * In fact clk_disable must be called before clk_unprepare.
 */
void clk_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	clk_core_disable_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_disable);

static int clk_core_enable(struct clk_core *core)
{
	int ret = 0;

	lockdep_assert_held(&enable_lock);

	if (!core)
		return 0;

	if (WARN_ON(core->prepare_count == 0))
		return -ESHUTDOWN;

	if (core->enable_count == 0) {
		ret = clk_core_enable(core->parent);

		if (ret)
			return ret;

		trace_clk_enable_rcuidle(core);

		if (core->ops->enable)
			ret = core->ops->enable(core->hw);

		trace_clk_enable_complete_rcuidle(core);

		if (ret) {
			clk_core_disable(core->parent);
			return ret;
		}
	}

	core->enable_count++;
	return 0;
}

static int clk_core_enable_lock(struct clk_core *core)
{
	unsigned long flags;
	int ret;

	flags = clk_enable_lock();
	ret = clk_core_enable(core);
	clk_enable_unlock(flags);

	return ret;
}

/**
 * clk_enable - ungate a clock
 * @clk: the clk being ungated
 *
 * clk_enable must not sleep, which differentiates it from clk_prepare.  In a
 * simple case, clk_enable can be used instead of clk_prepare to ungate a clk
 * if the operation will never sleep.  One example is a SoC-internal clk which
 * is controlled via simple register writes.  In the complex case a clk ungate
 * operation may require a fast and a slow part.  It is this reason that
 * clk_enable and clk_prepare are not mutually exclusive.  In fact clk_prepare
 * must be called before clk_enable.  Returns 0 on success, -EERROR
 * otherwise.
 */
int clk_enable(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_enable_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_enable);

static int clk_core_prepare_enable(struct clk_core *core)
{
	int ret;

	ret = clk_core_prepare_lock(core);
	if (ret)
		return ret;

	ret = clk_core_enable_lock(core);
	if (ret)
		clk_core_unprepare_lock(core);

	return ret;
}

static void clk_core_disable_unprepare(struct clk_core *core)
{
	clk_core_disable_lock(core);
	clk_core_unprepare_lock(core);
}

static void clk_unprepare_unused_subtree(struct clk_core *core)
{
	struct clk_core *child;

	lockdep_assert_held(&prepare_lock);

	hlist_for_each_entry(child, &core->children, child_node)
		clk_unprepare_unused_subtree(child);

	/*
	 * setting CLK_ENABLE_HAND_OFF flag triggers this conditional
	 *
	 * need_handoff_prepare implies this clk was already prepared by
	 * __clk_init. now we have a proper user, so unset the flag in our
	 * internal bookkeeping. See CLK_ENABLE_HAND_OFF flag in clk-provider.h
	 * for details.
	 */
	if (core->need_handoff_prepare) {
		core->need_handoff_prepare = false;
		clk_core_unprepare(core);
	}

	if (core->prepare_count)
		return;

	if (core->flags & CLK_IGNORE_UNUSED)
		return;

	if (clk_core_is_prepared(core)) {
		trace_clk_unprepare(core);
		if (core->ops->unprepare_unused)
			core->ops->unprepare_unused(core->hw);
		else if (core->ops->unprepare)
			core->ops->unprepare(core->hw);
		trace_clk_unprepare_complete(core);
	}
}

static void clk_disable_unused_subtree(struct clk_core *core)
{
	struct clk_core *child;
	unsigned long flags;

	lockdep_assert_held(&prepare_lock);

	hlist_for_each_entry(child, &core->children, child_node)
		clk_disable_unused_subtree(child);

	/*
	 * setting CLK_ENABLE_HAND_OFF flag triggers this conditional
	 *
	 * need_handoff_enable implies this clk was already enabled by
	 * __clk_init. now we have a proper user, so unset the flag in our
	 * internal bookkeeping. See CLK_ENABLE_HAND_OFF flag in clk-provider.h
	 * for details.
	 */
	if (core->need_handoff_enable) {
		core->need_handoff_enable = false;
		flags = clk_enable_lock();
		clk_core_disable(core);
		clk_enable_unlock(flags);
	}

	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_prepare_enable(core->parent);

	flags = clk_enable_lock();

	if (core->enable_count)
		goto unlock_out;

	if (core->flags & CLK_IGNORE_UNUSED)
		goto unlock_out;

	/*
	 * some gate clocks have special needs during the disable-unused
	 * sequence.  call .disable_unused if available, otherwise fall
	 * back to .disable
	 */
	if (clk_core_is_enabled(core)) {
		trace_clk_disable(core);
		if (core->ops->disable_unused)
			core->ops->disable_unused(core->hw);
		else if (core->ops->disable)
			core->ops->disable(core->hw);
		trace_clk_disable_complete(core);
	}

unlock_out:
	clk_enable_unlock(flags);
	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_disable_unprepare(core->parent);
}

static bool clk_ignore_unused;
static int __init clk_ignore_unused_setup(char *__unused)
{
	clk_ignore_unused = true;
	return 1;
}
__setup("clk_ignore_unused", clk_ignore_unused_setup);

static int clk_disable_unused(void)
{
	struct clk_core *core;
	struct clk_handoff_vdd *v, *v_temp;

	if (clk_ignore_unused) {
		pr_warn("clk: Not disabling unused clocks\n");
		return 0;
	}

	clk_prepare_lock();

	hlist_for_each_entry(core, &clk_root_list, child_node)
		clk_disable_unused_subtree(core);

	hlist_for_each_entry(core, &clk_orphan_list, child_node)
		clk_disable_unused_subtree(core);

	hlist_for_each_entry(core, &clk_root_list, child_node)
		clk_unprepare_unused_subtree(core);

	hlist_for_each_entry(core, &clk_orphan_list, child_node)
		clk_unprepare_unused_subtree(core);

	list_for_each_entry_safe(v, v_temp, &clk_handoff_vdd_list, list) {
		clk_unvote_vdd_level(v->vdd_class,
				v->vdd_class->num_levels - 1);
		list_del(&v->list);
		kfree(v);
	};

	clk_prepare_unlock();

	return 0;
}
late_initcall_sync(clk_disable_unused);

static int clk_core_round_rate_nolock(struct clk_core *core,
				      struct clk_rate_request *req)
{
	struct clk_core *parent;
	long rate;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	parent = core->parent;
	if (parent) {
		req->best_parent_hw = parent->hw;
		req->best_parent_rate = parent->rate;
	} else {
		req->best_parent_hw = NULL;
		req->best_parent_rate = 0;
	}

	if (core->ops->determine_rate) {
		return core->ops->determine_rate(core->hw, req);
	} else if (core->ops->round_rate) {
		rate = core->ops->round_rate(core->hw, req->rate,
					     &req->best_parent_rate);
		if (rate < 0)
			return rate;

		req->rate = rate;
	} else if (core->flags & CLK_SET_RATE_PARENT) {
		return clk_core_round_rate_nolock(parent, req);
	} else {
		req->rate = core->rate;
	}

	return 0;
}

/**
 * __clk_determine_rate - get the closest rate actually supported by a clock
 * @hw: determine the rate of this clock
 * @req: target rate request
 *
 * Useful for clk_ops such as .set_rate and .determine_rate.
 */
int __clk_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	if (!hw) {
		req->rate = 0;
		return 0;
	}

	return clk_core_round_rate_nolock(hw->core, req);
}
EXPORT_SYMBOL_GPL(__clk_determine_rate);

unsigned long clk_hw_round_rate(struct clk_hw *hw, unsigned long rate)
{
	int ret;
	struct clk_rate_request req;

	clk_core_get_boundaries(hw->core, &req.min_rate, &req.max_rate);
	req.rate = rate;

	ret = clk_core_round_rate_nolock(hw->core, &req);
	if (ret)
		return 0;

	return req.rate;
}
EXPORT_SYMBOL_GPL(clk_hw_round_rate);

/**
 * clk_round_rate - round the given rate for a clk
 * @clk: the clk for which we are rounding a rate
 * @rate: the rate which is to be rounded
 *
 * Takes in a rate as input and rounds it to a rate that the clk can actually
 * use which is then returned.  If clk doesn't support round_rate operation
 * then the parent rate is returned.
 */
long clk_round_rate(struct clk *clk, unsigned long rate)
{
	struct clk_rate_request req;
	int ret;

	if (!clk)
		return 0;

	clk_prepare_lock();

	clk_core_get_boundaries(clk->core, &req.min_rate, &req.max_rate);
	req.rate = rate;

	ret = clk_core_round_rate_nolock(clk->core, &req);
	clk_prepare_unlock();

	if (ret)
		return ret;

	return req.rate;
}
EXPORT_SYMBOL_GPL(clk_round_rate);

/**
 * __clk_notify - call clk notifier chain
 * @core: clk that is changing rate
 * @msg: clk notifier type (see include/linux/clk.h)
 * @old_rate: old clk rate
 * @new_rate: new clk rate
 *
 * Triggers a notifier call chain on the clk rate-change notification
 * for 'clk'.  Passes a pointer to the struct clk and the previous
 * and current rates to the notifier callback.  Intended to be called by
 * internal clock code only.  Returns NOTIFY_DONE from the last driver
 * called if all went well, or NOTIFY_STOP or NOTIFY_BAD immediately if
 * a driver returns that.
 */
static int __clk_notify(struct clk_core *core, unsigned long msg,
		unsigned long old_rate, unsigned long new_rate)
{
	struct clk_notifier *cn;
	struct clk_notifier_data cnd;
	int ret = NOTIFY_DONE;

	cnd.old_rate = old_rate;
	cnd.new_rate = new_rate;

	list_for_each_entry(cn, &clk_notifier_list, node) {
		if (cn->clk->core == core) {
			cnd.clk = cn->clk;
			ret = srcu_notifier_call_chain(&cn->notifier_head, msg,
					&cnd);
		}
	}

	return ret;
}

/**
 * __clk_recalc_accuracies
 * @core: first clk in the subtree
 *
 * Walks the subtree of clks starting with clk and recalculates accuracies as
 * it goes.  Note that if a clk does not implement the .recalc_accuracy
 * callback then it is assumed that the clock will take on the accuracy of its
 * parent.
 */
static void __clk_recalc_accuracies(struct clk_core *core)
{
	unsigned long parent_accuracy = 0;
	struct clk_core *child;

	lockdep_assert_held(&prepare_lock);

	if (core->parent)
		parent_accuracy = core->parent->accuracy;

	if (core->ops->recalc_accuracy)
		core->accuracy = core->ops->recalc_accuracy(core->hw,
							  parent_accuracy);
	else
		core->accuracy = parent_accuracy;

	hlist_for_each_entry(child, &core->children, child_node)
		__clk_recalc_accuracies(child);
}

static long clk_core_get_accuracy(struct clk_core *core)
{
	unsigned long accuracy;

	clk_prepare_lock();
	if (core && (core->flags & CLK_GET_ACCURACY_NOCACHE))
		__clk_recalc_accuracies(core);

	accuracy = __clk_get_accuracy(core);
	clk_prepare_unlock();

	return accuracy;
}

/**
 * clk_get_accuracy - return the accuracy of clk
 * @clk: the clk whose accuracy is being returned
 *
 * Simply returns the cached accuracy of the clk, unless
 * CLK_GET_ACCURACY_NOCACHE flag is set, which means a recalc_rate will be
 * issued.
 * If clk is NULL then returns 0.
 */
long clk_get_accuracy(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_get_accuracy(clk->core);
}
EXPORT_SYMBOL_GPL(clk_get_accuracy);

static unsigned long clk_recalc(struct clk_core *core,
				unsigned long parent_rate)
{
	if (core->ops->recalc_rate)
		return core->ops->recalc_rate(core->hw, parent_rate);
	return parent_rate;
}

/**
 * __clk_recalc_rates
 * @core: first clk in the subtree
 * @msg: notification type (see include/linux/clk.h)
 *
 * Walks the subtree of clks starting with clk and recalculates rates as it
 * goes.  Note that if a clk does not implement the .recalc_rate callback then
 * it is assumed that the clock will take on the rate of its parent.
 *
 * clk_recalc_rates also propagates the POST_RATE_CHANGE notification,
 * if necessary.
 */
static void __clk_recalc_rates(struct clk_core *core, unsigned long msg)
{
	unsigned long old_rate;
	unsigned long parent_rate = 0;
	struct clk_core *child;

	lockdep_assert_held(&prepare_lock);

	old_rate = core->rate;

	if (core->parent)
		parent_rate = core->parent->rate;

	core->rate = clk_recalc(core, parent_rate);

	/*
	 * ignore NOTIFY_STOP and NOTIFY_BAD return values for POST_RATE_CHANGE
	 * & ABORT_RATE_CHANGE notifiers
	 */
	if (core->notifier_count && msg)
		__clk_notify(core, msg, old_rate, core->rate);

	hlist_for_each_entry(child, &core->children, child_node)
		__clk_recalc_rates(child, msg);
}

static unsigned long clk_core_get_rate(struct clk_core *core)
{
	unsigned long rate;

	clk_prepare_lock();

	if (core && (core->flags & CLK_GET_RATE_NOCACHE))
		__clk_recalc_rates(core, 0);

	rate = clk_core_get_rate_nolock(core);
	clk_prepare_unlock();

	return rate;
}

/**
 * clk_get_rate - return the rate of clk
 * @clk: the clk whose rate is being returned
 *
 * Simply returns the cached rate of the clk, unless CLK_GET_RATE_NOCACHE flag
 * is set, which means a recalc_rate will be issued.
 * If clk is NULL then returns 0.
 */
unsigned long clk_get_rate(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_get_rate(clk->core);
}
EXPORT_SYMBOL_GPL(clk_get_rate);

static int clk_fetch_parent_index(struct clk_core *core,
				  struct clk_core *parent)
{
	int i;

	if (!parent)
		return -EINVAL;

	for (i = 0; i < core->num_parents; i++)
		if (clk_core_get_parent_by_index(core, i) == parent)
			return i;

	return -EINVAL;
}

/*
 * Update the orphan status of @core and all its children.
 */
static void clk_core_update_orphan_status(struct clk_core *core, bool is_orphan)
{
	struct clk_core *child;

	core->orphan = is_orphan;

	hlist_for_each_entry(child, &core->children, child_node)
		clk_core_update_orphan_status(child, is_orphan);
}

static void clk_reparent(struct clk_core *core, struct clk_core *new_parent)
{
	bool was_orphan = core->orphan;

	hlist_del(&core->child_node);

	if (new_parent) {
		bool becomes_orphan = new_parent->orphan;

		/* avoid duplicate POST_RATE_CHANGE notifications */
		if (new_parent->new_child == core)
			new_parent->new_child = NULL;

		hlist_add_head(&core->child_node, &new_parent->children);

		if (was_orphan != becomes_orphan)
			clk_core_update_orphan_status(core, becomes_orphan);
	} else {
		hlist_add_head(&core->child_node, &clk_orphan_list);
		if (!was_orphan)
			clk_core_update_orphan_status(core, true);
	}

	core->parent = new_parent;
}

static struct clk_core *__clk_set_parent_before(struct clk_core *core,
					   struct clk_core *parent)
{
	unsigned long flags;
	struct clk_core *old_parent = core->parent;

	/*
	 * 1. enable parents for CLK_OPS_PARENT_ENABLE clock
	 *
	 * 2. Migrate prepare state between parents and prevent race with
	 * clk_enable().
	 *
	 * If the clock is not prepared, then a race with
	 * clk_enable/disable() is impossible since we already have the
	 * prepare lock (future calls to clk_enable() need to be preceded by
	 * a clk_prepare()).
	 *
	 * If the clock is prepared, migrate the prepared state to the new
	 * parent and also protect against a race with clk_enable() by
	 * forcing the clock and the new parent on.  This ensures that all
	 * future calls to clk_enable() are practically NOPs with respect to
	 * hardware and software states.
	 *
	 * See also: Comment for clk_set_parent() below.
	 */

	/* enable old_parent & parent if CLK_OPS_PARENT_ENABLE is set */
	if (core->flags & CLK_OPS_PARENT_ENABLE) {
		clk_core_prepare_enable(old_parent);
		clk_core_prepare_enable(parent);
	}

	/* migrate prepare count if > 0 */
	if (core->prepare_count) {
		clk_core_prepare_enable(parent);
		clk_core_enable_lock(core);
	}

	/* update the clk tree topology */
	flags = clk_enable_lock();
	clk_reparent(core, parent);
	clk_enable_unlock(flags);

	return old_parent;
}

static void __clk_set_parent_after(struct clk_core *core,
				   struct clk_core *parent,
				   struct clk_core *old_parent)
{
	/*
	 * Finish the migration of prepare state and undo the changes done
	 * for preventing a race with clk_enable().
	 */
	if (core->prepare_count) {
		clk_core_disable_lock(core);
		clk_core_disable_unprepare(old_parent);
	}

	/* re-balance ref counting if CLK_OPS_PARENT_ENABLE is set */
	if (core->flags & CLK_OPS_PARENT_ENABLE) {
		clk_core_disable_unprepare(parent);
		clk_core_disable_unprepare(old_parent);
	}
}

static int __clk_set_parent(struct clk_core *core, struct clk_core *parent,
			    u8 p_index)
{
	unsigned long flags;
	int ret = 0;
	struct clk_core *old_parent;

	old_parent = __clk_set_parent_before(core, parent);

	trace_clk_set_parent(core, parent);

	/* change clock input source */
	if (parent && core->ops->set_parent)
		ret = core->ops->set_parent(core->hw, p_index);

	trace_clk_set_parent_complete(core, parent);

	if (ret) {
		flags = clk_enable_lock();
		clk_reparent(core, old_parent);
		clk_enable_unlock(flags);
		__clk_set_parent_after(core, old_parent, parent);

		return ret;
	}

	__clk_set_parent_after(core, parent, old_parent);

	return 0;
}

/**
 * __clk_speculate_rates
 * @core: first clk in the subtree
 * @parent_rate: the "future" rate of clk's parent
 *
 * Walks the subtree of clks starting with clk, speculating rates as it
 * goes and firing off PRE_RATE_CHANGE notifications as necessary.
 *
 * Unlike clk_recalc_rates, clk_speculate_rates exists only for sending
 * pre-rate change notifications and returns early if no clks in the
 * subtree have subscribed to the notifications.  Note that if a clk does not
 * implement the .recalc_rate callback then it is assumed that the clock will
 * take on the rate of its parent.
 */
static int __clk_speculate_rates(struct clk_core *core,
				 unsigned long parent_rate)
{
	struct clk_core *child;
	unsigned long new_rate;
	int ret = NOTIFY_DONE;

	lockdep_assert_held(&prepare_lock);

	new_rate = clk_recalc(core, parent_rate);

	/* abort rate change if a driver returns NOTIFY_BAD or NOTIFY_STOP */
	if (core->notifier_count)
		ret = __clk_notify(core, PRE_RATE_CHANGE, core->rate, new_rate);

	if (ret & NOTIFY_STOP_MASK) {
		pr_debug("%s: clk notifier callback for clock %s aborted with error %d\n",
				__func__, core->name, ret);
		goto out;
	}

	hlist_for_each_entry(child, &core->children, child_node) {
		ret = __clk_speculate_rates(child, new_rate);
		if (ret & NOTIFY_STOP_MASK)
			break;
	}

out:
	return ret;
}

static void clk_calc_subtree(struct clk_core *core, unsigned long new_rate,
			     struct clk_core *new_parent, u8 p_index)
{
	struct clk_core *child;

	core->new_rate = new_rate;
	core->new_parent = new_parent;
	core->new_parent_index = p_index;
	/* include clk in new parent's PRE_RATE_CHANGE notifications */
	core->new_child = NULL;
	if (new_parent && new_parent != core->parent)
		new_parent->new_child = core;

	hlist_for_each_entry(child, &core->children, child_node) {
		child->new_rate = clk_recalc(child, new_rate);
		clk_calc_subtree(child, child->new_rate, NULL, 0);
	}
}

/*
 * calculate the new rates returning the topmost clock that has to be
 * changed.
 */
static struct clk_core *clk_calc_new_rates(struct clk_core *core,
					   unsigned long rate)
{
	struct clk_core *top = core;
	struct clk_core *old_parent, *parent;
	unsigned long best_parent_rate = 0;
	unsigned long new_rate;
	unsigned long min_rate;
	unsigned long max_rate;
	int p_index = 0;
	long ret;

	/* sanity */
	if (IS_ERR_OR_NULL(core))
		return NULL;

	/* save parent rate, if it exists */
	parent = old_parent = core->parent;
	if (parent)
		best_parent_rate = parent->rate;

	clk_core_get_boundaries(core, &min_rate, &max_rate);

	/* find the closest rate and parent clk/rate */
	if (core->ops->determine_rate) {
		struct clk_rate_request req;

		req.rate = rate;
		req.min_rate = min_rate;
		req.max_rate = max_rate;
		if (parent) {
			req.best_parent_hw = parent->hw;
			req.best_parent_rate = parent->rate;
		} else {
			req.best_parent_hw = NULL;
			req.best_parent_rate = 0;
		}

		ret = core->ops->determine_rate(core->hw, &req);
		if (ret < 0)
			return NULL;

		best_parent_rate = req.best_parent_rate;
		new_rate = req.rate;
		parent = req.best_parent_hw ? req.best_parent_hw->core : NULL;
	} else if (core->ops->round_rate) {
		ret = core->ops->round_rate(core->hw, rate,
					    &best_parent_rate);
		if (ret < 0)
			return NULL;

		new_rate = ret;
		if (new_rate < min_rate || new_rate > max_rate)
			return NULL;
	} else if (!parent || !(core->flags & CLK_SET_RATE_PARENT)) {
		/* pass-through clock without adjustable parent */
		core->new_rate = core->rate;
		return NULL;
	} else {
		/* pass-through clock with adjustable parent */
		top = clk_calc_new_rates(parent, rate);
		new_rate = parent->new_rate;
		goto out;
	}

	/* some clocks must be gated to change parent */
	if (parent != old_parent &&
	    (core->flags & CLK_SET_PARENT_GATE) && core->prepare_count) {
		pr_debug("%s: %s not gated but wants to reparent\n",
			 __func__, core->name);
		return NULL;
	}

	/* try finding the new parent index */
	if (parent && core->num_parents > 1) {
		p_index = clk_fetch_parent_index(core, parent);
		if (p_index < 0) {
			pr_debug("%s: clk %s can not be parent of clk %s\n",
				 __func__, parent->name, core->name);
			return NULL;
		}
	}

	/*
	 * The Fabia PLLs only have 16 bits to program the fractional divider.
	 * Hence the programmed rate might be slightly different than the
	 * requested one.
	 */
	if ((core->flags & CLK_SET_RATE_PARENT) && parent &&
		(DIV_ROUND_CLOSEST(best_parent_rate, 1000) !=
			DIV_ROUND_CLOSEST(parent->rate, 1000)))
		top = clk_calc_new_rates(parent, best_parent_rate);

out:
	if (!clk_is_rate_level_valid(core, rate))
		return NULL;

	clk_calc_subtree(core, new_rate, parent, p_index);

	return top;
}

/*
 * Notify about rate changes in a subtree. Always walk down the whole tree
 * so that in case of an error we can walk down the whole tree again and
 * abort the change.
 */
static struct clk_core *clk_propagate_rate_change(struct clk_core *core,
						  unsigned long event)
{
	struct clk_core *child, *tmp_clk, *fail_clk = NULL;
	int ret = NOTIFY_DONE;

	if (core->rate == core->new_rate)
		return NULL;

	if (core->notifier_count) {
		ret = __clk_notify(core, event, core->rate, core->new_rate);
		if (ret & NOTIFY_STOP_MASK)
			fail_clk = core;
	}

	hlist_for_each_entry(child, &core->children, child_node) {
		/* Skip children who will be reparented to another clock */
		if (child->new_parent && child->new_parent != core)
			continue;
		tmp_clk = clk_propagate_rate_change(child, event);
		if (tmp_clk)
			fail_clk = tmp_clk;
	}

	/* handle the new child who might not be in core->children yet */
	if (core->new_child) {
		tmp_clk = clk_propagate_rate_change(core->new_child, event);
		if (tmp_clk)
			fail_clk = tmp_clk;
	}

	return fail_clk;
}

/*
 * walk down a subtree and set the new rates notifying the rate
 * change on the way
 */
static int clk_change_rate(struct clk_core *core)
{
	struct clk_core *child;
	struct hlist_node *tmp;
	unsigned long old_rate;
	unsigned long best_parent_rate = 0;
	bool skip_set_rate = false;
	struct clk_core *old_parent;
	struct clk_core *parent = NULL;
	int rc = 0;

	old_rate = core->rate;

	if (core->new_parent) {
		parent = core->new_parent;
		best_parent_rate = core->new_parent->rate;
	} else if (core->parent) {
		parent = core->parent;
		best_parent_rate = core->parent->rate;
	}

	if (core->flags & CLK_SET_RATE_UNGATE) {
		unsigned long flags;

		clk_core_prepare(core);
		flags = clk_enable_lock();
		clk_core_enable(core);
		clk_enable_unlock(flags);
	}

	trace_clk_set_rate(core, core->new_rate);

	/* Enforce vdd requirements for new frequency. */
	if (core->prepare_count) {
		rc = clk_vote_rate_vdd(core, core->new_rate);
		if (rc)
			goto out;
	}

	if (core->new_parent && core->new_parent != core->parent) {
		old_parent = __clk_set_parent_before(core, core->new_parent);
		trace_clk_set_parent(core, core->new_parent);

		if (core->ops->set_rate_and_parent) {
			skip_set_rate = true;
			core->ops->set_rate_and_parent(core->hw, core->new_rate,
					best_parent_rate,
					core->new_parent_index);
		} else if (core->ops->set_parent) {
			core->ops->set_parent(core->hw, core->new_parent_index);
		}

		trace_clk_set_parent_complete(core, core->new_parent);
		__clk_set_parent_after(core, core->new_parent, old_parent);
	}

	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_prepare_enable(parent);

	if (!skip_set_rate && core->ops->set_rate) {
		rc = core->ops->set_rate(core->hw, core->new_rate,
						best_parent_rate);
		if (rc)
			goto err_set_rate;
	}

	trace_clk_set_rate_complete(core, core->new_rate);

	/* Release vdd requirements for old frequency. */
	if (core->prepare_count)
		clk_unvote_rate_vdd(core, old_rate);

	core->rate = clk_recalc(core, best_parent_rate);

	if (core->flags & CLK_SET_RATE_UNGATE) {
		unsigned long flags;

		flags = clk_enable_lock();
		clk_core_disable(core);
		clk_enable_unlock(flags);
		clk_core_unprepare(core);
	}

	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_disable_unprepare(parent);

	if (core->notifier_count && old_rate != core->rate)
		__clk_notify(core, POST_RATE_CHANGE, old_rate, core->rate);

	if (core->flags & CLK_RECALC_NEW_RATES)
		(void)clk_calc_new_rates(core, core->new_rate);

	if (core->flags & CLK_CHILD_NO_RATE_PROP)
		return rc;
	/*
	 * Use safe iteration, as change_rate can actually swap parents
	 * for certain clock types.
	 */
	hlist_for_each_entry_safe(child, tmp, &core->children, child_node) {
		/* Skip children who will be reparented to another clock */
		if (child->new_parent && child->new_parent != core)
			continue;
		rc = clk_change_rate(child);
		if (rc)
			return rc;
	}

	/* handle the new child who might not be in core->children yet */
	if (core->new_child)
		rc = clk_change_rate(core->new_child);

	return rc;

err_set_rate:
	if (core->prepare_count)
		clk_unvote_rate_vdd(core, core->new_rate);
out:
	trace_clk_set_rate_complete(core, core->new_rate);

	return rc;
}

static int clk_core_set_rate_nolock(struct clk_core *core,
				    unsigned long req_rate)
{
	struct clk_core *top, *fail_clk;
	unsigned long rate = req_rate;
	int ret = 0;

	if (!core)
		return 0;

	/* bail early if nothing to do */
	if (rate == clk_core_get_rate_nolock(core))
		return 0;

	if ((core->flags & CLK_SET_RATE_GATE) && core->prepare_count)
		return -EBUSY;

	/* calculate new rates and get the topmost changed clock */
	top = clk_calc_new_rates(core, rate);
	if (!top)
		return -EINVAL;

	/* notify that we are about to change rates */
	fail_clk = clk_propagate_rate_change(top, PRE_RATE_CHANGE);
	if (fail_clk) {
		pr_debug("%s: failed to set %s clock to run at %lu\n", __func__,
				fail_clk->name, req_rate);
		clk_propagate_rate_change(top, ABORT_RATE_CHANGE);
		return -EBUSY;
	}

	/* change the rates */
	ret = clk_change_rate(top);
	if (ret) {
		pr_err("%s: failed to set %s clock to run at %lu\n", __func__,
				top->name, req_rate);
		clk_propagate_rate_change(top, ABORT_RATE_CHANGE);
		return ret;
	}

	core->req_rate = req_rate;

	return ret;
}

/**
 * clk_set_rate - specify a new rate for clk
 * @clk: the clk whose rate is being changed
 * @rate: the new rate for clk
 *
 * In the simplest case clk_set_rate will only adjust the rate of clk.
 *
 * Setting the CLK_SET_RATE_PARENT flag allows the rate change operation to
 * propagate up to clk's parent; whether or not this happens depends on the
 * outcome of clk's .round_rate implementation.  If *parent_rate is unchanged
 * after calling .round_rate then upstream parent propagation is ignored.  If
 * *parent_rate comes back with a new rate for clk's parent then we propagate
 * up to clk's parent and set its rate.  Upward propagation will continue
 * until either a clk does not support the CLK_SET_RATE_PARENT flag or
 * .round_rate stops requesting changes to clk's parent_rate.
 *
 * Rate changes are accomplished via tree traversal that also recalculates the
 * rates for the clocks and fires off POST_RATE_CHANGE notifiers.
 *
 * Returns 0 on success, -EERROR otherwise.
 */
int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;

	if (!clk)
		return 0;

	/* prevent racing with updates to the clock topology */
	clk_prepare_lock();

	ret = clk_core_set_rate_nolock(clk->core, rate);

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate);

/**
 * clk_set_rate_range - set a rate range for a clock source
 * @clk: clock source
 * @min: desired minimum clock rate in Hz, inclusive
 * @max: desired maximum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clk_set_rate_range(struct clk *clk, unsigned long min, unsigned long max)
{
	int ret = 0;

	if (!clk)
		return 0;

	if (min > max) {
		pr_err("%s: clk %s dev %s con %s: invalid range [%lu, %lu]\n",
		       __func__, clk->core->name, clk->dev_id, clk->con_id,
		       min, max);
		return -EINVAL;
	}

	clk_prepare_lock();

	if (min != clk->min_rate || max != clk->max_rate) {
		clk->min_rate = min;
		clk->max_rate = max;
		ret = clk_core_set_rate_nolock(clk->core, clk->core->req_rate);
	}

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate_range);

/**
 * clk_set_min_rate - set a minimum clock rate for a clock source
 * @clk: clock source
 * @rate: desired minimum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clk_set_min_rate(struct clk *clk, unsigned long rate)
{
	if (!clk)
		return 0;

	return clk_set_rate_range(clk, rate, clk->max_rate);
}
EXPORT_SYMBOL_GPL(clk_set_min_rate);

/**
 * clk_set_max_rate - set a maximum clock rate for a clock source
 * @clk: clock source
 * @rate: desired maximum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	if (!clk)
		return 0;

	return clk_set_rate_range(clk, clk->min_rate, rate);
}
EXPORT_SYMBOL_GPL(clk_set_max_rate);

/**
 * clk_get_parent - return the parent of a clk
 * @clk: the clk whose parent gets returned
 *
 * Simply returns clk->parent.  Returns NULL if clk is NULL.
 */
struct clk *clk_get_parent(struct clk *clk)
{
	struct clk *parent;

	if (!clk)
		return NULL;

	clk_prepare_lock();
	/* TODO: Create a per-user clk and change callers to call clk_put */
	parent = !clk->core->parent ? NULL : clk->core->parent->hw->clk;
	clk_prepare_unlock();

	return parent;
}
EXPORT_SYMBOL_GPL(clk_get_parent);

static struct clk_core *__clk_init_parent(struct clk_core *core)
{
	u8 index = 0;

	if (core->num_parents > 1 && core->ops->get_parent)
		index = core->ops->get_parent(core->hw);

	return clk_core_get_parent_by_index(core, index);
}

static void clk_core_reparent(struct clk_core *core,
				  struct clk_core *new_parent)
{
	clk_reparent(core, new_parent);
	__clk_recalc_accuracies(core);
	__clk_recalc_rates(core, POST_RATE_CHANGE);
}

void clk_hw_reparent(struct clk_hw *hw, struct clk_hw *new_parent)
{
	if (!hw)
		return;

	clk_core_reparent(hw->core, !new_parent ? NULL : new_parent->core);
}

/**
 * clk_has_parent - check if a clock is a possible parent for another
 * @clk: clock source
 * @parent: parent clock source
 *
 * This function can be used in drivers that need to check that a clock can be
 * the parent of another without actually changing the parent.
 *
 * Returns true if @parent is a possible parent for @clk, false otherwise.
 */
bool clk_has_parent(struct clk *clk, struct clk *parent)
{
	struct clk_core *core, *parent_core;
	unsigned int i;

	/* NULL clocks should be nops, so return success if either is NULL. */
	if (!clk || !parent)
		return true;

	core = clk->core;
	parent_core = parent->core;

	/* Optimize for the case where the parent is already the parent. */
	if (core->parent == parent_core)
		return true;

	for (i = 0; i < core->num_parents; i++)
		if (strcmp(core->parent_names[i], parent_core->name) == 0)
			return true;

	return false;
}
EXPORT_SYMBOL_GPL(clk_has_parent);

static int clk_core_set_parent(struct clk_core *core, struct clk_core *parent)
{
	int ret = 0;
	int p_index = 0;
	unsigned long p_rate = 0;

	if (!core)
		return 0;

	/* prevent racing with updates to the clock topology */
	clk_prepare_lock();

	if (core->parent == parent && !(core->flags & CLK_IS_MEASURE))
		goto out;

	/* verify ops for for multi-parent clks */
	if ((core->num_parents > 1) && (!core->ops->set_parent)) {
		ret = -ENOSYS;
		goto out;
	}

	/* check that we are allowed to re-parent if the clock is in use */
	if ((core->flags & CLK_SET_PARENT_GATE) && core->prepare_count) {
		ret = -EBUSY;
		goto out;
	}

	/* try finding the new parent index */
	if (parent) {
		p_index = clk_fetch_parent_index(core, parent);
		if (p_index < 0) {
			pr_debug("%s: clk %s can not be parent of clk %s\n",
					__func__, parent->name, core->name);
			ret = p_index;
			goto out;
		}
		p_rate = parent->rate;
	}

	/* propagate PRE_RATE_CHANGE notifications */
	ret = __clk_speculate_rates(core, p_rate);

	/* abort if a driver objects */
	if (ret & NOTIFY_STOP_MASK)
		goto out;

	/* do the re-parent */
	ret = __clk_set_parent(core, parent, p_index);

	/* propagate rate an accuracy recalculation accordingly */
	if (ret) {
		__clk_recalc_rates(core, ABORT_RATE_CHANGE);
	} else {
		__clk_recalc_rates(core, POST_RATE_CHANGE);
		__clk_recalc_accuracies(core);
	}

out:
	clk_prepare_unlock();

	return ret;
}

/**
 * clk_set_parent - switch the parent of a mux clk
 * @clk: the mux clk whose input we are switching
 * @parent: the new input to clk
 *
 * Re-parent clk to use parent as its new input source.  If clk is in
 * prepared state, the clk will get enabled for the duration of this call. If
 * that's not acceptable for a specific clk (Eg: the consumer can't handle
 * that, the reparenting is glitchy in hardware, etc), use the
 * CLK_SET_PARENT_GATE flag to allow reparenting only when clk is unprepared.
 *
 * After successfully changing clk's parent clk_set_parent will update the
 * clk topology, sysfs topology and propagate rate recalculation via
 * __clk_recalc_rates.
 *
 * Returns 0 on success, -EERROR otherwise.
 */
int clk_set_parent(struct clk *clk, struct clk *parent)
{
	if (!clk)
		return 0;

	return clk_core_set_parent(clk->core, parent ? parent->core : NULL);
}
EXPORT_SYMBOL_GPL(clk_set_parent);

/**
 * clk_set_phase - adjust the phase shift of a clock signal
 * @clk: clock signal source
 * @degrees: number of degrees the signal is shifted
 *
 * Shifts the phase of a clock signal by the specified
 * degrees. Returns 0 on success, -EERROR otherwise.
 *
 * This function makes no distinction about the input or reference
 * signal that we adjust the clock signal phase against. For example
 * phase locked-loop clock signal generators we may shift phase with
 * respect to feedback clock signal input, but for other cases the
 * clock phase may be shifted with respect to some other, unspecified
 * signal.
 *
 * Additionally the concept of phase shift does not propagate through
 * the clock tree hierarchy, which sets it apart from clock rates and
 * clock accuracy. A parent clock phase attribute does not have an
 * impact on the phase attribute of a child clock.
 */
int clk_set_phase(struct clk *clk, int degrees)
{
	int ret = -EINVAL;

	if (!clk)
		return 0;

	/* sanity check degrees */
	degrees %= 360;
	if (degrees < 0)
		degrees += 360;

	clk_prepare_lock();

	trace_clk_set_phase(clk->core, degrees);

	if (clk->core->ops->set_phase)
		ret = clk->core->ops->set_phase(clk->core->hw, degrees);

	trace_clk_set_phase_complete(clk->core, degrees);

	if (!ret)
		clk->core->phase = degrees;

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_phase);

static int clk_core_get_phase(struct clk_core *core)
{
	int ret;

	clk_prepare_lock();
	ret = core->phase;
	clk_prepare_unlock();

	return ret;
}

/**
 * clk_get_phase - return the phase shift of a clock signal
 * @clk: clock signal source
 *
 * Returns the phase shift of a clock node in degrees, otherwise returns
 * -EERROR.
 */
int clk_get_phase(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_get_phase(clk->core);
}
EXPORT_SYMBOL_GPL(clk_get_phase);

/**
 * clk_is_match - check if two clk's point to the same hardware clock
 * @p: clk compared against q
 * @q: clk compared against p
 *
 * Returns true if the two struct clk pointers both point to the same hardware
 * clock node. Put differently, returns true if struct clk *p and struct clk *q
 * share the same struct clk_core object.
 *
 * Returns false otherwise. Note that two NULL clks are treated as matching.
 */
bool clk_is_match(const struct clk *p, const struct clk *q)
{
	/* trivial case: identical struct clk's or both NULL */
	if (p == q)
		return true;

	/* true if clk->core pointers match. Avoid dereferencing garbage */
	if (!IS_ERR_OR_NULL(p) && !IS_ERR_OR_NULL(q))
		if (p->core == q->core)
			return true;

	return false;
}
EXPORT_SYMBOL_GPL(clk_is_match);

int clk_set_flags(struct clk *clk, unsigned long flags)
{
	if (!clk)
		return 0;

	if (!clk->core->ops->set_flags)
		return -EINVAL;

	return clk->core->ops->set_flags(clk->core->hw, flags);
}
EXPORT_SYMBOL_GPL(clk_set_flags);

unsigned long clk_list_frequency(struct clk *clk, unsigned int index)
{
	int ret = 0;

	if (!clk || !clk->core->ops->list_rate)
		return -EINVAL;

	clk_prepare_lock();
	ret = clk->core->ops->list_rate(clk->core->hw, index, ULONG_MAX);
	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_list_frequency);

/***        debugfs support        ***/

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

static struct dentry *rootdir;
static int inited = 0;
static u32 debug_suspend;
static DEFINE_MUTEX(clk_debug_lock);
static HLIST_HEAD(clk_debug_list);

static struct hlist_head *all_lists[] = {
	&clk_root_list,
	&clk_orphan_list,
	NULL,
};

static struct hlist_head *orphan_list[] = {
	&clk_orphan_list,
	NULL,
};

static void clk_state_subtree(struct clk_core *c)
{
	int vdd_level = 0;
	struct clk_core *child;

	if (!c)
		return;

	if (c->vdd_class) {
		vdd_level = clk_find_vdd_level(c, c->rate);
		if (vdd_level < 0)
			vdd_level = 0;
	}

	trace_clk_state(c->name, c->prepare_count, c->enable_count,
						c->rate, vdd_level);

	hlist_for_each_entry(child, &c->children, child_node)
		clk_state_subtree(child);
}

static int clk_state_show(struct seq_file *s, void *data)
{
	struct clk_core *c;
	struct hlist_head **lists = (struct hlist_head **)s->private;

	clk_prepare_lock();

	for (; *lists; lists++)
		hlist_for_each_entry(c, *lists, child_node)
			clk_state_subtree(c);

	clk_prepare_unlock();

	return 0;
}


static int clk_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, clk_state_show, inode->i_private);
}

static const struct file_operations clk_state_fops = {
	.open		= clk_state_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void clk_summary_show_one(struct seq_file *s, struct clk_core *c,
				 int level)
{
	if (!c)
		return;

	seq_printf(s, "%*s%-*s %11d %12d %11lu %10lu %-3d\n",
		   level * 3 + 1, "",
		   30 - level * 3, c->name,
		   c->enable_count, c->prepare_count, clk_core_get_rate(c),
		   clk_core_get_accuracy(c), clk_core_get_phase(c));
}

static void clk_summary_show_subtree(struct seq_file *s, struct clk_core *c,
				     int level)
{
	struct clk_core *child;

	if (!c)
		return;

	clk_summary_show_one(s, c, level);

	hlist_for_each_entry(child, &c->children, child_node)
		clk_summary_show_subtree(s, child, level + 1);
}

static int clk_summary_show(struct seq_file *s, void *data)
{
	struct clk_core *c;
	struct hlist_head **lists = (struct hlist_head **)s->private;

	seq_puts(s, "   clock                         enable_cnt  prepare_cnt        rate   accuracy   phase\n");
	seq_puts(s, "----------------------------------------------------------------------------------------\n");

	clk_prepare_lock();

	for (; *lists; lists++)
		hlist_for_each_entry(c, *lists, child_node)
			clk_summary_show_subtree(s, c, 0);

	clk_prepare_unlock();

	return 0;
}


static int clk_summary_open(struct inode *inode, struct file *file)
{
	return single_open(file, clk_summary_show, inode->i_private);
}

static const struct file_operations clk_summary_fops = {
	.open		= clk_summary_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void clk_dump_one(struct seq_file *s, struct clk_core *c, int level)
{
	if (!c)
		return;

	/* This should be JSON format, i.e. elements separated with a comma */
	seq_printf(s, "\"%s\": { ", c->name);
	seq_printf(s, "\"enable_count\": %d,", c->enable_count);
	seq_printf(s, "\"prepare_count\": %d,", c->prepare_count);
	seq_printf(s, "\"rate\": %lu,", clk_core_get_rate(c));
	seq_printf(s, "\"accuracy\": %lu,", clk_core_get_accuracy(c));
	seq_printf(s, "\"phase\": %d", clk_core_get_phase(c));
}

static void clk_dump_subtree(struct seq_file *s, struct clk_core *c, int level)
{
	struct clk_core *child;

	if (!c)
		return;

	clk_dump_one(s, c, level);

	hlist_for_each_entry(child, &c->children, child_node) {
		seq_printf(s, ",");
		clk_dump_subtree(s, child, level + 1);
	}

	seq_printf(s, "}");
}

static int clk_dump(struct seq_file *s, void *data)
{
	struct clk_core *c;
	bool first_node = true;
	struct hlist_head **lists = (struct hlist_head **)s->private;

	seq_printf(s, "{");

	clk_prepare_lock();

	for (; *lists; lists++) {
		hlist_for_each_entry(c, *lists, child_node) {
			if (!first_node)
				seq_puts(s, ",");
			first_node = false;
			clk_dump_subtree(s, c, 0);
		}
	}

	clk_prepare_unlock();

	seq_puts(s, "}\n");
	return 0;
}


static int clk_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, clk_dump, inode->i_private);
}

static const struct file_operations clk_dump_fops = {
	.open		= clk_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int clock_debug_rate_set(void *data, u64 val)
{
	struct clk_core *core = data;
	int ret;

	ret = clk_set_rate(core->hw->clk, val);
	if (ret)
		pr_err("clk_set_rate(%lu) failed (%d)\n",
				(unsigned long)val, ret);

	return ret;
}

static int clock_debug_rate_get(void *data, u64 *val)
{
	struct clk_core *core = data;

	*val = core->hw->core->rate;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_rate_fops, clock_debug_rate_get,
			clock_debug_rate_set, "%llu\n");

static ssize_t clock_parent_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	char name[256] = {0};
	struct clk_core *core = filp->private_data;
	struct clk_core *p = core->hw->core->parent;

	snprintf(name, sizeof(name), "%s\n", p ? p->name : "None\n");

	return simple_read_from_buffer(ubuf, cnt, ppos, name, strlen(name));
}

static const struct file_operations clock_parent_fops = {
	.open	= simple_open,
	.read	= clock_parent_read,
};

static int clock_debug_enable_set(void *data, u64 val)
{
	struct clk_core *core = data;
	int rc = 0;

	if (val)
		rc = clk_prepare_enable(core->hw->clk);
	else
		clk_disable_unprepare(core->hw->clk);

	return rc;
}

static int clock_debug_enable_get(void *data, u64 *val)
{
	struct clk_core *core = data;
	int enabled = 0;

	enabled = core->enable_count;

	*val = enabled;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_enable_fops, clock_debug_enable_get,
			clock_debug_enable_set, "%lld\n");

#define clock_debug_output(m, c, fmt, ...)		\
do {							\
	if (m)						\
		seq_printf(m, fmt, ##__VA_ARGS__);	\
	else if (c)					\
		pr_cont(fmt, ##__VA_ARGS__);		\
	else						\
		pr_info(fmt, ##__VA_ARGS__);		\
} while (0)

/*
 * clock_debug_print_enabled_debug_suspend() - Print names of enabled clocks
 * during suspend.
 */
static void clock_debug_print_enabled_debug_suspend(struct seq_file *s)
{
	struct clk_core *core;
	int cnt = 0;

	if (!mutex_trylock(&clk_debug_lock))
		return;

	clock_debug_output(s, 0, "Enabled clocks:\n");

	hlist_for_each_entry(core, &clk_debug_list, debug_node) {
		if (!core || !core->prepare_count)
			continue;

		if (core->vdd_class)
			clock_debug_output(s, 0, " %s:%u:%u [%ld, %d]",
					core->name, core->prepare_count,
					core->enable_count, core->rate,
					clk_find_vdd_level(core, core->rate));

		else
			clock_debug_output(s, 0, " %s:%u:%u [%ld]",
					core->name, core->prepare_count,
					core->enable_count, core->rate);
		cnt++;
	}

	mutex_unlock(&clk_debug_lock);

	if (cnt)
		clock_debug_output(s, 0, "Enabled clock count: %d\n", cnt);
	else
		clock_debug_output(s, 0, "No clocks enabled.\n");
}

static int clock_debug_print_clock(struct clk_core *c, struct seq_file *s)
{
	char *start = "";
	struct clk *clk;

	if (!c || !c->prepare_count)
		return 0;

	clk = c->hw->clk;

	clock_debug_output(s, 0, "\t");

	do {
		if (clk->core->vdd_class)
			clock_debug_output(s, 1, "%s%s:%u:%u [%ld, %d]", start,
					clk->core->name,
					clk->core->prepare_count,
					clk->core->enable_count,
					clk->core->rate,
				clk_find_vdd_level(clk->core, clk->core->rate));
		else
			clock_debug_output(s, 1, "%s%s:%u:%u [%ld]", start,
					clk->core->name,
					clk->core->prepare_count,
					clk->core->enable_count,
					clk->core->rate);
		start = " -> ";
	} while ((clk = clk_get_parent(clk)));

	clock_debug_output(s, 1, "\n");

	return 1;
}

/*
 * clock_debug_print_enabled_clocks() - Print names of enabled clocks
 */
static void clock_debug_print_enabled_clocks(struct seq_file *s)
{
	struct clk_core *core;
	int cnt = 0;

	if (!mutex_trylock(&clk_debug_lock))
		return;

	clock_debug_output(s, 0, "Enabled clocks:\n");

	hlist_for_each_entry(core, &clk_debug_list, debug_node)
		cnt += clock_debug_print_clock(core, s);

	mutex_unlock(&clk_debug_lock);

	if (cnt)
		clock_debug_output(s, 0, "Enabled clock count: %d\n", cnt);
	else
		clock_debug_output(s, 0, "No clocks enabled.\n");
}

static int enabled_clocks_show(struct seq_file *s, void *unused)
{
	clock_debug_print_enabled_clocks(s);

	return 0;
}

static int enabled_clocks_open(struct inode *inode, struct file *file)
{
	return single_open(file, enabled_clocks_show, inode->i_private);
}

static const struct file_operations clk_enabled_list_fops = {
	.open		= enabled_clocks_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

void clk_debug_print_hw(struct clk_core *clk, struct seq_file *f)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	clk_debug_print_hw(clk->parent, f);

	clock_debug_output(f, false, "%s\n", clk->name);

	if (!clk->ops->list_registers)
		return;

	clk->ops->list_registers(f, clk->hw);
}

static int print_hw_show(struct seq_file *m, void *unused)
{
	struct clk_core *c = m->private;

	clk_debug_print_hw(c, m);

	return 0;
}

static int print_hw_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_hw_show, inode->i_private);
}

static const struct file_operations clock_print_hw_fops = {
	.open		= print_hw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int list_rates_show(struct seq_file *s, void *unused)
{
	struct clk_core *core = s->private;
	int level = 0, i = 0;
	unsigned long rate, rate_max = 0;

	/* Find max frequency supported within voltage constraints. */
	if (!core->vdd_class) {
		rate_max = ULONG_MAX;
	} else {
		for (level = 0; level < core->num_rate_max; level++)
			if (core->rate_max[level])
				rate_max = core->rate_max[level];
	}

	/*
	 * List supported frequencies <= rate_max. Higher frequencies may
	 * appear in the frequency table, but are not valid and should not
	 * be listed.
	 */
	while (!IS_ERR_VALUE(rate =
			core->ops->list_rate(core->hw, i++, rate_max))) {
		if (rate <= 0)
			break;
		if (rate <= rate_max)
			seq_printf(s, "%lu\n", rate);
	}

	return 0;
}

static int list_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, list_rates_show, inode->i_private);
}

static const struct file_operations list_rates_fops = {
	.open		= list_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static void clock_print_rate_max_by_level(struct seq_file *s, int level)
{
	struct clk_core *core = s->private;
	struct clk_vdd_class *vdd_class = core->vdd_class;
	int off, i, vdd_level, nregs = vdd_class->num_regulators;

	vdd_level = clk_find_vdd_level(core, core->rate);

	seq_printf(s, "%2s%10lu", vdd_level == level ? "[" : "",
		core->rate_max[level]);

	for (i = 0; i < nregs; i++) {
		off = nregs*level + i;
		if (vdd_class->vdd_uv)
			seq_printf(s, "%10u", vdd_class->vdd_uv[off]);
	}

	if (vdd_level == level)
		seq_puts(s, "]");

	seq_puts(s, "\n");
}

static int rate_max_show(struct seq_file *s, void *unused)
{
	struct clk_core *core = s->private;
	struct clk_vdd_class *vdd_class = core->vdd_class;
	int level = 0, i, nregs = vdd_class->num_regulators;
	char reg_name[10];

	int vdd_level = clk_find_vdd_level(core, core->rate);

	if (vdd_level < 0) {
		seq_printf(s, "could not find_vdd_level for %s, %ld\n",
			core->name, core->rate);
		return 0;
	}

	seq_printf(s, "%12s", "");
	for (i = 0; i < nregs; i++) {
		snprintf(reg_name, ARRAY_SIZE(reg_name), "reg %d", i);
		seq_printf(s, "%10s", reg_name);
	}

	seq_printf(s, "\n%12s", "freq");
	for (i = 0; i < nregs; i++)
		seq_printf(s, "%10s", "uV");

	seq_puts(s, "\n");

	for (level = 0; level < core->num_rate_max; level++)
		clock_print_rate_max_by_level(s, level);

	return 0;
}

static int rate_max_open(struct inode *inode, struct file *file)
{
	return single_open(file, rate_max_show, inode->i_private);
}

static const struct file_operations rate_max_fops = {
	.open		= rate_max_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int clk_debug_create_one(struct clk_core *core, struct dentry *pdentry)
{
	struct dentry *d;
	int ret = -ENOMEM;

	if (!core || !pdentry) {
		ret = -EINVAL;
		goto out;
	}

	d = debugfs_create_dir(core->name, pdentry);
	if (!d)
		goto out;

	core->dentry = d;

	d = debugfs_create_file("clk_rate", 0444, core->dentry, core,
			&clock_rate_fops);
	if (!d)
		goto err_out;

	if (core->ops->list_rate) {
		if (!debugfs_create_file("clk_list_rates",
				0444, core->dentry, core, &list_rates_fops))
			goto err_out;
	}

	if (core->vdd_class && !debugfs_create_file("clk_rate_max",
				0444, core->dentry, core, &rate_max_fops))
		goto err_out;

	d = debugfs_create_u32("clk_accuracy", 0444, core->dentry,
			(u32 *)&core->accuracy);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_phase", 0444, core->dentry,
			(u32 *)&core->phase);
	if (!d)
		goto err_out;

	d = debugfs_create_x32("clk_flags", 0444, core->dentry,
			(u32 *)&core->flags);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_prepare_count", 0444, core->dentry,
			(u32 *)&core->prepare_count);
	if (!d)
		goto err_out;

	d = debugfs_create_file("clk_enable_count", 0444, core->dentry,
			core, &clock_enable_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_notifier_count", 0444, core->dentry,
			(u32 *)&core->notifier_count);
	if (!d)
		goto err_out;

	d = debugfs_create_file("clk_parent", 0444, core->dentry, core,
			&clock_parent_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file("clk_print_regs", 0444, core->dentry,
			core, &clock_print_hw_fops);
	if (!d)
		goto err_out;

	if (core->ops->debug_init) {
		ret = core->ops->debug_init(core->hw, core->dentry);
		if (ret)
			goto err_out;
	}

	ret = 0;
	goto out;

err_out:
	debugfs_remove_recursive(core->dentry);
	core->dentry = NULL;
out:
	return ret;
}

/**
 * clk_debug_register - add a clk node to the debugfs clk directory
 * @core: the clk being added to the debugfs clk directory
 *
 * Dynamically adds a clk to the debugfs clk directory if debugfs has been
 * initialized.  Otherwise it bails out early since the debugfs clk directory
 * will be created lazily by clk_debug_init as part of a late_initcall.
 */
static int clk_debug_register(struct clk_core *core)
{
	int ret = 0;

	mutex_lock(&clk_debug_lock);
	hlist_add_head(&core->debug_node, &clk_debug_list);

	if (!inited)
		goto unlock;

	ret = clk_debug_create_one(core, rootdir);
unlock:
	mutex_unlock(&clk_debug_lock);

	return ret;
}

 /**
 * clk_debug_unregister - remove a clk node from the debugfs clk directory
 * @core: the clk being removed from the debugfs clk directory
 *
 * Dynamically removes a clk and all its child nodes from the
 * debugfs clk directory if clk->dentry points to debugfs created by
 * clk_debug_register in __clk_core_init.
 */
static void clk_debug_unregister(struct clk_core *core)
{
	mutex_lock(&clk_debug_lock);
	hlist_del_init(&core->debug_node);
	debugfs_remove_recursive(core->dentry);
	core->dentry = NULL;
	mutex_unlock(&clk_debug_lock);
}

struct dentry *clk_debugfs_add_file(struct clk_hw *hw, char *name, umode_t mode,
				void *data, const struct file_operations *fops)
{
	struct dentry *d = NULL;

	if (hw->core->dentry)
		d = debugfs_create_file(name, mode, hw->core->dentry, data,
					fops);

	return d;
}
EXPORT_SYMBOL_GPL(clk_debugfs_add_file);

/*
 * Print the names of all enabled clocks and their parents if
 * debug_suspend is set from debugfs along with print_parent flag set to 1.
 * Otherwise if print_parent set to 0, print only enabled clocks
 *
 */
void clock_debug_print_enabled(bool print_parent)
{
	if (likely(!debug_suspend))
		return;

	if (print_parent)
		clock_debug_print_enabled_clocks(NULL);
	else
		clock_debug_print_enabled_debug_suspend(NULL);
}
EXPORT_SYMBOL_GPL(clock_debug_print_enabled);

/**
 * clk_debug_init - lazily populate the debugfs clk directory
 *
 * clks are often initialized very early during boot before memory can be
 * dynamically allocated and well before debugfs is setup. This function
 * populates the debugfs clk directory once at boot-time when we know that
 * debugfs is setup. It should only be called once at boot-time, all other clks
 * added dynamically will be done so with clk_debug_register.
 */
static int __init clk_debug_init(void)
{
	struct clk_core *core;
	struct dentry *d;

	rootdir = debugfs_create_dir("clk", NULL);

	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("clk_summary", 0444, rootdir, &all_lists,
				&clk_summary_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("clk_dump", 0444, rootdir, &all_lists,
				&clk_dump_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("clk_orphan_summary", 0444, rootdir,
				&orphan_list, &clk_summary_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("clk_orphan_dump", 0444, rootdir,
				&orphan_list, &clk_dump_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("clk_enabled_list", 0444, rootdir,
				&clk_debug_list, &clk_enabled_list_fops);
	if (!d)
		return -ENOMEM;


	d = debugfs_create_u32("debug_suspend", 0644, rootdir, &debug_suspend);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("trace_clocks", 0444, rootdir, &all_lists,
				&clk_state_fops);
	if (!d)
		return -ENOMEM;

	mutex_lock(&clk_debug_lock);
	hlist_for_each_entry(core, &clk_debug_list, debug_node)
		clk_debug_create_one(core, rootdir);

	inited = 1;
	mutex_unlock(&clk_debug_lock);

	return 0;
}
late_initcall(clk_debug_init);
#else
static inline int clk_debug_register(struct clk_core *core) { return 0; }
static inline void clk_debug_reparent(struct clk_core *core,
				      struct clk_core *new_parent)
{
}
static inline void clk_debug_unregister(struct clk_core *core)
{
}
#endif

/**
 * __clk_core_init - initialize the data structures in a struct clk_core
 * @core:	clk_core being initialized
 *
 * Initializes the lists in struct clk_core, queries the hardware for the
 * parent and rate and sets them both.
 */
static int __clk_core_init(struct clk_core *core)
{
	int i, ret = 0;
	struct clk_core *orphan;
	struct hlist_node *tmp2;
	unsigned long rate;

	if (!core)
		return -EINVAL;

	clk_prepare_lock();

	/* check to see if a clock with this name is already registered */
	if (clk_core_lookup(core->name)) {
		pr_debug("%s: clk %s already initialized\n",
				__func__, core->name);
		ret = -EEXIST;
		goto out;
	}

	/* check that clk_ops are sane.  See Documentation/clk.txt */
	if (core->ops->set_rate &&
	    !((core->ops->round_rate || core->ops->determine_rate) &&
	      core->ops->recalc_rate)) {
		pr_err("%s: %s must implement .round_rate or .determine_rate in addition to .recalc_rate\n",
		       __func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->ops->set_parent && !core->ops->get_parent) {
		pr_err("%s: %s must implement .get_parent & .set_parent\n",
		       __func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->num_parents > 1 && !core->ops->get_parent) {
		pr_err("%s: %s must implement .get_parent as it has multi parents\n",
		       __func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->ops->set_rate_and_parent &&
			!(core->ops->set_parent && core->ops->set_rate)) {
		pr_err("%s: %s must implement .set_parent & .set_rate\n",
				__func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	/* throw a WARN if any entries in parent_names are NULL */
	for (i = 0; i < core->num_parents; i++)
		WARN(!core->parent_names[i],
				"%s: invalid NULL in %s's .parent_names\n",
				__func__, core->name);

	core->parent = __clk_init_parent(core);

	/*
	 * Populate core->parent if parent has already been clk_core_init'd. If
	 * parent has not yet been clk_core_init'd then place clk in the orphan
	 * list.  If clk doesn't have any parents then place it in the root
	 * clk list.
	 *
	 * Every time a new clk is clk_init'd then we walk the list of orphan
	 * clocks and re-parent any that are children of the clock currently
	 * being clk_init'd.
	 */
	if (core->parent) {
		hlist_add_head(&core->child_node,
				&core->parent->children);
		core->orphan = core->parent->orphan;
	} else if (!core->num_parents) {
		hlist_add_head(&core->child_node, &clk_root_list);
		core->orphan = false;
	} else {
		hlist_add_head(&core->child_node, &clk_orphan_list);
		core->orphan = true;
	}

	/*
	 * Set clk's accuracy.  The preferred method is to use
	 * .recalc_accuracy. For simple clocks and lazy developers the default
	 * fallback is to use the parent's accuracy.  If a clock doesn't have a
	 * parent (or is orphaned) then accuracy is set to zero (perfect
	 * clock).
	 */
	if (core->ops->recalc_accuracy)
		core->accuracy = core->ops->recalc_accuracy(core->hw,
					__clk_get_accuracy(core->parent));
	else if (core->parent)
		core->accuracy = core->parent->accuracy;
	else
		core->accuracy = 0;

	/*
	 * Set clk's phase.
	 * Since a phase is by definition relative to its parent, just
	 * query the current clock phase, or just assume it's in phase.
	 */
	if (core->ops->get_phase)
		core->phase = core->ops->get_phase(core->hw);
	else
		core->phase = 0;

	/*
	 * Set clk's rate.  The preferred method is to use .recalc_rate.  For
	 * simple clocks and lazy developers the default fallback is to use the
	 * parent's rate.  If a clock doesn't have a parent (or is orphaned)
	 * then rate is set to zero.
	 */
	if (core->ops->recalc_rate)
		rate = core->ops->recalc_rate(core->hw,
				clk_core_get_rate_nolock(core->parent));
	else if (core->parent)
		rate = core->parent->rate;
	else
		rate = 0;
	core->rate = core->req_rate = rate;

	/*
	 * walk the list of orphan clocks and reparent any that newly finds a
	 * parent.
	 */
	hlist_for_each_entry_safe(orphan, tmp2, &clk_orphan_list, child_node) {
		struct clk_core *parent = __clk_init_parent(orphan);

		/*
		 * we could call __clk_set_parent, but that would result in a
		 * redundant call to the .set_rate op, if it exists
		 */
		if (parent) {
			__clk_set_parent_before(orphan, parent);
			__clk_set_parent_after(orphan, parent, NULL);
			__clk_recalc_accuracies(orphan);
			__clk_recalc_rates(orphan, 0);
		}
	}

	/*
	 * optional platform-specific magic
	 *
	 * The .init callback is not used by any of the basic clock types, but
	 * exists for weird hardware that must perform initialization magic.
	 * Please consider other ways of solving initialization problems before
	 * using this callback, as its use is discouraged.
	 */
	if (core->ops->init)
		core->ops->init(core->hw);

	if (core->flags & CLK_IS_CRITICAL) {
		unsigned long flags;

		clk_core_prepare(core);

		flags = clk_enable_lock();
		clk_core_enable(core);
		clk_enable_unlock(flags);
	}

	/*
	 * enable clocks with the CLK_ENABLE_HAND_OFF flag set
	 *
	 * This flag causes the framework to enable the clock at registration
	 * time, which is sometimes necessary for clocks that would cause a
	 * system crash when gated (e.g. cpu, memory, etc). The prepare_count
	 * is migrated over to the first clk consumer to call clk_prepare().
	 * Similarly the clk's enable_count is migrated to the first consumer
	 * to call clk_enable().
	 */
	if (core->flags & CLK_ENABLE_HAND_OFF) {
		unsigned long flags;

		/*
		 * Few clocks might have hardware gating which would be
		 * required to be ON before prepare/enabling the clocks. So
		 * check if the clock has been turned ON earlier and we should
		 * prepare/enable those clocks.
		 */
		if (clk_core_is_enabled(core)) {
			core->need_handoff_prepare = true;
			core->need_handoff_enable = true;
			ret = clk_core_prepare(core);
			if (ret)
				goto out;
			flags = clk_enable_lock();
			clk_core_enable(core);
			clk_enable_unlock(flags);
		}
	}

	kref_init(&core->ref);
out:
	clk_prepare_unlock();

	if (!ret)
		clk_debug_register(core);

	return ret;
}

struct clk *__clk_create_clk(struct clk_hw *hw, const char *dev_id,
			     const char *con_id)
{
	struct clk *clk;

	/* This is to allow this function to be chained to others */
	if (IS_ERR_OR_NULL(hw))
		return ERR_CAST(hw);

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	clk->core = hw->core;
	clk->dev_id = dev_id;
	clk->con_id = con_id;
	clk->max_rate = ULONG_MAX;

	clk_prepare_lock();
	hlist_add_head(&clk->clks_node, &hw->core->clks);
	clk_prepare_unlock();

	return clk;
}

void __clk_free_clk(struct clk *clk)
{
	clk_prepare_lock();
	hlist_del(&clk->clks_node);
	clk_prepare_unlock();

	kfree(clk);
}

/**
 * clk_register - allocate a new clock, register it and return an opaque cookie
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * clk_register is the primary interface for populating the clock tree with new
 * clock nodes.  It returns a pointer to the newly allocated struct clk which
 * cannot be dereferenced by driver code but may be used in conjunction with the
 * rest of the clock API.  In the event of an error clk_register will return an
 * error code; drivers must test for an error code after calling clk_register.
 */
struct clk *clk_register(struct device *dev, struct clk_hw *hw)
{
	int i, ret;
	struct clk_core *core;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core) {
		ret = -ENOMEM;
		goto fail_out;
	}

	core->name = kstrdup_const(hw->init->name, GFP_KERNEL);
	if (!core->name) {
		ret = -ENOMEM;
		goto fail_name;
	}
	core->ops = hw->init->ops;
	if (dev && dev->driver)
		core->owner = dev->driver->owner;
	core->hw = hw;
	core->flags = hw->init->flags;
	core->num_parents = hw->init->num_parents;
	core->min_rate = 0;
	core->max_rate = ULONG_MAX;
	core->vdd_class = hw->init->vdd_class;
	core->rate_max = hw->init->rate_max;
	core->num_rate_max = hw->init->num_rate_max;
	hw->core = core;

	if (core->vdd_class) {
		ret = clk_vdd_class_init(core->vdd_class);
		if (ret) {
			pr_err("Failed to initialize vdd class\n");
			goto fail_parent_names;
		}
	}

	/* allocate local copy in case parent_names is __initdata */
	core->parent_names = kcalloc(core->num_parents, sizeof(char *),
					GFP_KERNEL);

	if (!core->parent_names) {
		ret = -ENOMEM;
		goto fail_parent_names;
	}


	/* copy each string name in case parent_names is __initdata */
	for (i = 0; i < core->num_parents; i++) {
		core->parent_names[i] = kstrdup_const(hw->init->parent_names[i],
						GFP_KERNEL);
		if (!core->parent_names[i]) {
			ret = -ENOMEM;
			goto fail_parent_names_copy;
		}
	}

	/* avoid unnecessary string look-ups of clk_core's possible parents. */
	core->parents = kcalloc(core->num_parents, sizeof(*core->parents),
				GFP_KERNEL);
	if (!core->parents) {
		ret = -ENOMEM;
		goto fail_parents;
	};

	INIT_HLIST_HEAD(&core->clks);

	hw->clk = __clk_create_clk(hw, NULL, NULL);
	if (IS_ERR(hw->clk)) {
		ret = PTR_ERR(hw->clk);
		goto fail_parents;
	}

	ret = __clk_core_init(core);
	if (!ret)
		return hw->clk;

	__clk_free_clk(hw->clk);
	hw->clk = NULL;

fail_parents:
	kfree(core->parents);
fail_parent_names_copy:
	while (--i >= 0)
		kfree_const(core->parent_names[i]);
	kfree(core->parent_names);
fail_parent_names:
	kfree_const(core->name);
fail_name:
	kfree(core);
fail_out:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(clk_register);

/**
 * clk_hw_register - register a clk_hw and return an error code
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * clk_hw_register is the primary interface for populating the clock tree with
 * new clock nodes. It returns an integer equal to zero indicating success or
 * less than zero indicating failure. Drivers must test for an error code after
 * calling clk_hw_register().
 */
int clk_hw_register(struct device *dev, struct clk_hw *hw)
{
	return PTR_ERR_OR_ZERO(clk_register(dev, hw));
}
EXPORT_SYMBOL_GPL(clk_hw_register);

/* Free memory allocated for a clock. */
static void __clk_release(struct kref *ref)
{
	struct clk_core *core = container_of(ref, struct clk_core, ref);
	int i = core->num_parents;

	lockdep_assert_held(&prepare_lock);

	kfree(core->parents);
	while (--i >= 0)
		kfree_const(core->parent_names[i]);

	kfree(core->parent_names);
	kfree_const(core->name);
	kfree(core);
}

/*
 * Empty clk_ops for unregistered clocks. These are used temporarily
 * after clk_unregister() was called on a clock and until last clock
 * consumer calls clk_put() and the struct clk object is freed.
 */
static int clk_nodrv_prepare_enable(struct clk_hw *hw)
{
	return -ENXIO;
}

static void clk_nodrv_disable_unprepare(struct clk_hw *hw)
{
	WARN_ON_ONCE(1);
}

static int clk_nodrv_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	return -ENXIO;
}

static int clk_nodrv_set_parent(struct clk_hw *hw, u8 index)
{
	return -ENXIO;
}

static const struct clk_ops clk_nodrv_ops = {
	.enable		= clk_nodrv_prepare_enable,
	.disable	= clk_nodrv_disable_unprepare,
	.prepare	= clk_nodrv_prepare_enable,
	.unprepare	= clk_nodrv_disable_unprepare,
	.set_rate	= clk_nodrv_set_rate,
	.set_parent	= clk_nodrv_set_parent,
};

/**
 * clk_unregister - unregister a currently registered clock
 * @clk: clock to unregister
 */
void clk_unregister(struct clk *clk)
{
	unsigned long flags;

	if (!clk || WARN_ON_ONCE(IS_ERR(clk)))
		return;

	clk_debug_unregister(clk->core);

	clk_prepare_lock();

	if (clk->core->ops == &clk_nodrv_ops) {
		pr_err("%s: unregistered clock: %s\n", __func__,
		       clk->core->name);
		goto unlock;
	}
	/*
	 * Assign empty clock ops for consumers that might still hold
	 * a reference to this clock.
	 */
	flags = clk_enable_lock();
	clk->core->ops = &clk_nodrv_ops;
	clk_enable_unlock(flags);

	if (!hlist_empty(&clk->core->children)) {
		struct clk_core *child;
		struct hlist_node *t;

		/* Reparent all children to the orphan list. */
		hlist_for_each_entry_safe(child, t, &clk->core->children,
					  child_node)
			clk_core_set_parent(child, NULL);
	}

	hlist_del_init(&clk->core->child_node);

	if (clk->core->prepare_count)
		pr_warn("%s: unregistering prepared clock: %s\n",
					__func__, clk->core->name);
	kref_put(&clk->core->ref, __clk_release);
unlock:
	clk_prepare_unlock();
}
EXPORT_SYMBOL_GPL(clk_unregister);

/**
 * clk_hw_unregister - unregister a currently registered clk_hw
 * @hw: hardware-specific clock data to unregister
 */
void clk_hw_unregister(struct clk_hw *hw)
{
	clk_unregister(hw->clk);
}
EXPORT_SYMBOL_GPL(clk_hw_unregister);

static void devm_clk_release(struct device *dev, void *res)
{
	clk_unregister(*(struct clk **)res);
}

static void devm_clk_hw_release(struct device *dev, void *res)
{
	clk_hw_unregister(*(struct clk_hw **)res);
}

#define MAX_LEN_OPP_HANDLE	50
#define LEN_OPP_HANDLE		16

static int derive_device_list(struct device **device_list,
				struct clk_core *core,
				struct device_node *np,
				char *clk_handle_name, int count)
{
	int j;
	struct platform_device *pdev;
	struct device_node *dev_node;

	for (j = 0; j < count; j++) {
		device_list[j] = NULL;
		dev_node = of_parse_phandle(np, clk_handle_name, j);
		if (!dev_node) {
			pr_err("Unable to get device_node pointer for %s opp-handle (%s)\n",
					core->name, clk_handle_name);
			return -ENODEV;
		}

		pdev = of_find_device_by_node(dev_node);
		if (!pdev) {
			pr_err("Unable to find platform_device node for %s opp-handle\n",
						core->name);
			return -ENODEV;
		}
		device_list[j] = &pdev->dev;
	}
	return 0;
}

static int clk_get_voltage(struct clk_core *core, unsigned long rate, int n)
{
	struct clk_vdd_class *vdd;
	int level, corner;

	/* Use the first regulator in the vdd class for the OPP table. */
	vdd = core->vdd_class;
	if (vdd->num_regulators > 1) {
		corner = vdd->vdd_uv[vdd->num_regulators * n];
	} else {
		level = clk_find_vdd_level(core, rate);
		if (level < 0) {
			pr_err("Could not find vdd level\n");
			return -EINVAL;
		}
		corner = vdd->vdd_uv[level];
	}

	if (!corner) {
		pr_err("%s: Unable to find vdd level for rate %lu\n",
					core->name, rate);
		return -EINVAL;
	}

	return corner;
}

static int clk_add_and_print_opp(struct clk_hw *hw,
				struct device **device_list, int count,
				unsigned long rate, int uv, int n)
{
	struct clk_core *core = hw->core;
	int j, ret = 0;

	for (j = 0; j < count; j++) {
		ret = dev_pm_opp_add(device_list[j], rate, uv);
		if (ret) {
			pr_err("%s: couldn't add OPP for %lu - err: %d\n",
						core->name, rate, ret);
			return ret;
		}

		if (n == 0 || n == core->num_rate_max - 1 ||
					rate == clk_hw_round_rate(hw, INT_MAX))
			pr_info("%s: set OPP pair(%lu Hz: %u uV) on %s\n",
						core->name, rate, uv,
						dev_name(device_list[j]));
	}
	return ret;
}

static void clk_populate_clock_opp_table(struct device_node *np,
						struct clk_hw *hw)
{
	struct device **device_list;
	struct clk_core *core = hw->core;
	char clk_handle_name[MAX_LEN_OPP_HANDLE];
	int n, len, count, uv;
	unsigned long rate = 0, ret = 0;

	if (!core || !core->num_rate_max)
		return;

	if (strlen(core->name) + LEN_OPP_HANDLE < MAX_LEN_OPP_HANDLE) {
		ret = snprintf(clk_handle_name, ARRAY_SIZE(clk_handle_name),
				"qcom,%s-opp-handle", core->name);
		if (ret < strlen(core->name) + LEN_OPP_HANDLE) {
			pr_err("%s: Failed to hold clk_handle_name\n",
							core->name);
			return;
		}
	} else {
		pr_err("clk name (%s) too large to fit in clk_handle_name\n",
							core->name);
		return;
	}

	if (of_find_property(np, clk_handle_name, &len)) {
		count = len/sizeof(u32);

		device_list = kmalloc_array(count, sizeof(struct device *),
							GFP_KERNEL);
		if (!device_list)
			return;

		ret = derive_device_list(device_list, core, np,
					clk_handle_name, count);
		if (ret < 0) {
			pr_err("Failed to fill device_list for %s\n",
						clk_handle_name);
			goto err_derive_device_list;
		}
	} else {
		pr_debug("Unable to find %s\n", clk_handle_name);
		return;
	}

	for (n = 0; ; n++) {
		ret = clk_hw_round_rate(hw, rate + 1);
		if (ret < 0) {
			pr_err("clk_round_rate failed for %s\n",
							core->name);
			goto err_derive_device_list;
		}

		/*
		 * If clk_hw_round_rate gives the same value on consecutive
		 * iterations, exit the loop since we're at the maximum clock
		 * frequency.
		 */
		if (rate == ret)
			break;
		rate = ret;

		uv = clk_get_voltage(core, rate, n);
		if (uv < 0)
			goto err_derive_device_list;

		ret = clk_add_and_print_opp(hw, device_list, count,
							rate, uv, n);
		if (ret)
			goto err_derive_device_list;
	}

err_derive_device_list:
	kfree(device_list);
}

/**
 * devm_clk_register - resource managed clk_register()
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * Managed clk_register(). Clocks returned from this function are
 * automatically clk_unregister()ed on driver detach. See clk_register() for
 * more information.
 */
struct clk *devm_clk_register(struct device *dev, struct clk_hw *hw)
{
	struct clk *clk;
	struct clk **clkp;

	clkp = devres_alloc(devm_clk_release, sizeof(*clkp), GFP_KERNEL);
	if (!clkp)
		return ERR_PTR(-ENOMEM);

	clk = clk_register(dev, hw);
	if (!IS_ERR(clk)) {
		*clkp = clk;
		devres_add(dev, clkp);
	} else {
		devres_free(clkp);
	}

	clk_populate_clock_opp_table(dev->of_node, hw);
	return clk;
}
EXPORT_SYMBOL_GPL(devm_clk_register);

/**
 * devm_clk_hw_register - resource managed clk_hw_register()
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * Managed clk_hw_register(). Clocks registered by this function are
 * automatically clk_hw_unregister()ed on driver detach. See clk_hw_register()
 * for more information.
 */
int devm_clk_hw_register(struct device *dev, struct clk_hw *hw)
{
	struct clk_hw **hwp;
	int ret;

	hwp = devres_alloc(devm_clk_hw_release, sizeof(*hwp), GFP_KERNEL);
	if (!hwp)
		return -ENOMEM;

	ret = clk_hw_register(dev, hw);
	if (!ret) {
		*hwp = hw;
		devres_add(dev, hwp);
	} else {
		devres_free(hwp);
	}

	clk_populate_clock_opp_table(dev->of_node, hw);
	return ret;
}
EXPORT_SYMBOL_GPL(devm_clk_hw_register);

static int devm_clk_match(struct device *dev, void *res, void *data)
{
	struct clk *c = res;
	if (WARN_ON(!c))
		return 0;
	return c == data;
}

static int devm_clk_hw_match(struct device *dev, void *res, void *data)
{
	struct clk_hw *hw = res;

	if (WARN_ON(!hw))
		return 0;
	return hw == data;
}

/**
 * devm_clk_unregister - resource managed clk_unregister()
 * @clk: clock to unregister
 *
 * Deallocate a clock allocated with devm_clk_register(). Normally
 * this function will not need to be called and the resource management
 * code will ensure that the resource is freed.
 */
void devm_clk_unregister(struct device *dev, struct clk *clk)
{
	WARN_ON(devres_release(dev, devm_clk_release, devm_clk_match, clk));
}
EXPORT_SYMBOL_GPL(devm_clk_unregister);

/**
 * devm_clk_hw_unregister - resource managed clk_hw_unregister()
 * @dev: device that is unregistering the hardware-specific clock data
 * @hw: link to hardware-specific clock data
 *
 * Unregister a clk_hw registered with devm_clk_hw_register(). Normally
 * this function will not need to be called and the resource management
 * code will ensure that the resource is freed.
 */
void devm_clk_hw_unregister(struct device *dev, struct clk_hw *hw)
{
	WARN_ON(devres_release(dev, devm_clk_hw_release, devm_clk_hw_match,
				hw));
}
EXPORT_SYMBOL_GPL(devm_clk_hw_unregister);

/*
 * clkdev helpers
 */
int __clk_get(struct clk *clk)
{
	struct clk_core *core = !clk ? NULL : clk->core;

	if (core) {
		if (!try_module_get(core->owner))
			return 0;

		kref_get(&core->ref);
	}
	return 1;
}

void __clk_put(struct clk *clk)
{
	struct module *owner;

	if (!clk || WARN_ON_ONCE(IS_ERR(clk)))
		return;

	clk_prepare_lock();

	hlist_del(&clk->clks_node);
	if (clk->min_rate > clk->core->req_rate ||
	    clk->max_rate < clk->core->req_rate)
		clk_core_set_rate_nolock(clk->core, clk->core->req_rate);

	owner = clk->core->owner;
	kref_put(&clk->core->ref, __clk_release);

	clk_prepare_unlock();

	module_put(owner);

	kfree(clk);
}

/***        clk rate change notifiers        ***/

/**
 * clk_notifier_register - add a clk rate change notifier
 * @clk: struct clk * to watch
 * @nb: struct notifier_block * with callback info
 *
 * Request notification when clk's rate changes.  This uses an SRCU
 * notifier because we want it to block and notifier unregistrations are
 * uncommon.  The callbacks associated with the notifier must not
 * re-enter into the clk framework by calling any top-level clk APIs;
 * this will cause a nested prepare_lock mutex.
 *
 * In all notification cases (pre, post and abort rate change) the original
 * clock rate is passed to the callback via struct clk_notifier_data.old_rate
 * and the new frequency is passed via struct clk_notifier_data.new_rate.
 *
 * clk_notifier_register() must be called from non-atomic context.
 * Returns -EINVAL if called with null arguments, -ENOMEM upon
 * allocation failure; otherwise, passes along the return value of
 * srcu_notifier_chain_register().
 */
int clk_notifier_register(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn;
	int ret = -ENOMEM;

	if (!clk || !nb)
		return -EINVAL;

	clk_prepare_lock();

	/* search the list of notifiers for this clk */
	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			break;

	/* if clk wasn't in the notifier list, allocate new clk_notifier */
	if (cn->clk != clk) {
		cn = kzalloc(sizeof(struct clk_notifier), GFP_KERNEL);
		if (!cn)
			goto out;

		cn->clk = clk;
		srcu_init_notifier_head(&cn->notifier_head);

		list_add(&cn->node, &clk_notifier_list);
	}

	ret = srcu_notifier_chain_register(&cn->notifier_head, nb);

	clk->core->notifier_count++;

out:
	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_notifier_register);

/**
 * clk_notifier_unregister - remove a clk rate change notifier
 * @clk: struct clk *
 * @nb: struct notifier_block * with callback info
 *
 * Request no further notification for changes to 'clk' and frees memory
 * allocated in clk_notifier_register.
 *
 * Returns -EINVAL if called with null arguments; otherwise, passes
 * along the return value of srcu_notifier_chain_unregister().
 */
int clk_notifier_unregister(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn = NULL;
	int ret = -EINVAL;

	if (!clk || !nb)
		return -EINVAL;

	clk_prepare_lock();

	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			break;

	if (cn->clk == clk) {
		ret = srcu_notifier_chain_unregister(&cn->notifier_head, nb);

		clk->core->notifier_count--;

		/* XXX the notifier code should handle this better */
		if (!cn->notifier_head.head) {
			srcu_cleanup_notifier_head(&cn->notifier_head);
			list_del(&cn->node);
			kfree(cn);
		}

	} else {
		ret = -ENOENT;
	}

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_notifier_unregister);

#endif /* CONFIG_COMMON_CLK */

#ifdef CONFIG_OF
/**
 * struct of_clk_provider - Clock provider registration structure
 * @link: Entry in global list of clock providers
 * @node: Pointer to device tree node of clock provider
 * @get: Get clock callback.  Returns NULL or a struct clk for the
 *       given clock specifier
 * @data: context pointer to be passed into @get callback
 */
struct of_clk_provider {
	struct list_head link;

	struct device_node *node;
	struct clk *(*get)(struct of_phandle_args *clkspec, void *data);
	struct clk_hw *(*get_hw)(struct of_phandle_args *clkspec, void *data);
	void *data;
};

static const struct of_device_id __clk_of_table_sentinel
	__used __section(__clk_of_table_end);

static LIST_HEAD(of_clk_providers);
static DEFINE_MUTEX(of_clk_mutex);

struct clk *of_clk_src_simple_get(struct of_phandle_args *clkspec,
				     void *data)
{
	return data;
}
EXPORT_SYMBOL_GPL(of_clk_src_simple_get);

struct clk_hw *of_clk_hw_simple_get(struct of_phandle_args *clkspec, void *data)
{
	return data;
}
EXPORT_SYMBOL_GPL(of_clk_hw_simple_get);

#if defined(CONFIG_COMMON_CLK)

struct clk *of_clk_src_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk_onecell_data *clk_data = data;
	unsigned int idx = clkspec->args[0];

	if (idx >= clk_data->clk_num) {
		pr_err("%s: invalid clock index %u\n", __func__, idx);
		return ERR_PTR(-EINVAL);
	}

	return clk_data->clks[idx];
}
EXPORT_SYMBOL_GPL(of_clk_src_onecell_get);

struct clk_hw *
of_clk_hw_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk_hw_onecell_data *hw_data = data;
	unsigned int idx = clkspec->args[0];

	if (idx >= hw_data->num) {
		pr_err("%s: invalid index %u\n", __func__, idx);
		return ERR_PTR(-EINVAL);
	}

	return hw_data->hws[idx];
}
EXPORT_SYMBOL_GPL(of_clk_hw_onecell_get);

#endif /* CONFIG_COMMON_CLK */

/**
 * of_clk_del_provider() - Remove a previously registered clock provider
 * @np: Device node pointer associated with clock provider
 */
void of_clk_del_provider(struct device_node *np)
{
	struct of_clk_provider *cp;

	mutex_lock(&of_clk_mutex);
	list_for_each_entry(cp, &of_clk_providers, link) {
		if (cp->node == np) {
			list_del(&cp->link);
			of_node_put(cp->node);
			kfree(cp);
			break;
		}
	}
	mutex_unlock(&of_clk_mutex);
}
EXPORT_SYMBOL_GPL(of_clk_del_provider);

/**
 * of_clk_add_provider() - Register a clock provider for a node
 * @np: Device node pointer associated with clock provider
 * @clk_src_get: callback for decoding clock
 * @data: context pointer for @clk_src_get callback.
 */
int of_clk_add_provider(struct device_node *np,
			struct clk *(*clk_src_get)(struct of_phandle_args *clkspec,
						   void *data),
			void *data)
{
	struct of_clk_provider *cp;
	int ret;

	cp = kzalloc(sizeof(struct of_clk_provider), GFP_KERNEL);
	if (!cp)
		return -ENOMEM;

	cp->node = of_node_get(np);
	cp->data = data;
	cp->get = clk_src_get;

	mutex_lock(&of_clk_mutex);
	list_add(&cp->link, &of_clk_providers);
	mutex_unlock(&of_clk_mutex);
	pr_debug("Added clock from %s\n", np->full_name);

	ret = of_clk_set_defaults(np, true);
	if (ret < 0)
		of_clk_del_provider(np);

	return ret;
}
EXPORT_SYMBOL_GPL(of_clk_add_provider);

/**
 * of_clk_add_hw_provider() - Register a clock provider for a node
 * @np: Device node pointer associated with clock provider
 * @get: callback for decoding clk_hw
 * @data: context pointer for @get callback.
 */
int of_clk_add_hw_provider(struct device_node *np,
			   struct clk_hw *(*get)(struct of_phandle_args *clkspec,
						 void *data),
			   void *data)
{
	struct of_clk_provider *cp;
	int ret;

	cp = kzalloc(sizeof(*cp), GFP_KERNEL);
	if (!cp)
		return -ENOMEM;

	cp->node = of_node_get(np);
	cp->data = data;
	cp->get_hw = get;

	mutex_lock(&of_clk_mutex);
	list_add(&cp->link, &of_clk_providers);
	mutex_unlock(&of_clk_mutex);
	pr_debug("Added clk_hw provider from %s\n", np->full_name);

	ret = of_clk_set_defaults(np, true);
	if (ret < 0)
		of_clk_del_provider(np);

	return ret;
}
EXPORT_SYMBOL_GPL(of_clk_add_hw_provider);

static struct clk_hw *
__of_clk_get_hw_from_provider(struct of_clk_provider *provider,
			      struct of_phandle_args *clkspec)
{
	struct clk *clk;

	if (provider->get_hw)
		return provider->get_hw(clkspec, provider->data);

	clk = provider->get(clkspec, provider->data);
	if (IS_ERR(clk))
		return ERR_CAST(clk);
	return __clk_get_hw(clk);
}

struct clk *__of_clk_get_from_provider(struct of_phandle_args *clkspec,
				       const char *dev_id, const char *con_id)
{
	struct of_clk_provider *provider;
	struct clk *clk = ERR_PTR(-EPROBE_DEFER);
	struct clk_hw *hw;

	if (!clkspec)
		return ERR_PTR(-EINVAL);

	/* Check if we have such a provider in our array */
	mutex_lock(&of_clk_mutex);
	list_for_each_entry(provider, &of_clk_providers, link) {
		if (provider->node == clkspec->np) {
			hw = __of_clk_get_hw_from_provider(provider, clkspec);
			clk = __clk_create_clk(hw, dev_id, con_id);
		}

		if (!IS_ERR(clk)) {
			if (!__clk_get(clk)) {
				__clk_free_clk(clk);
				clk = ERR_PTR(-ENOENT);
			}

			break;
		}
	}
	mutex_unlock(&of_clk_mutex);

	return clk;
}

/**
 * of_clk_get_from_provider() - Lookup a clock from a clock provider
 * @clkspec: pointer to a clock specifier data structure
 *
 * This function looks up a struct clk from the registered list of clock
 * providers, an input is a clock specifier data structure as returned
 * from the of_parse_phandle_with_args() function call.
 */
struct clk *of_clk_get_from_provider(struct of_phandle_args *clkspec)
{
	return __of_clk_get_from_provider(clkspec, NULL, __func__);
}
EXPORT_SYMBOL_GPL(of_clk_get_from_provider);

/**
 * of_clk_get_parent_count() - Count the number of clocks a device node has
 * @np: device node to count
 *
 * Returns: The number of clocks that are possible parents of this node
 */
unsigned int of_clk_get_parent_count(struct device_node *np)
{
	int count;

	count = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (count < 0)
		return 0;

	return count;
}
EXPORT_SYMBOL_GPL(of_clk_get_parent_count);

const char *of_clk_get_parent_name(struct device_node *np, int index)
{
	struct of_phandle_args clkspec;
	struct property *prop;
	const char *clk_name;
	const __be32 *vp;
	u32 pv;
	int rc;
	int count;
	struct clk *clk;

	rc = of_parse_phandle_with_args(np, "clocks", "#clock-cells", index,
					&clkspec);
	if (rc)
		return NULL;

	index = clkspec.args_count ? clkspec.args[0] : 0;
	count = 0;

	/* if there is an indices property, use it to transfer the index
	 * specified into an array offset for the clock-output-names property.
	 */
	of_property_for_each_u32(clkspec.np, "clock-indices", prop, vp, pv) {
		if (index == pv) {
			index = count;
			break;
		}
		count++;
	}
	/* We went off the end of 'clock-indices' without finding it */
	if (prop && !vp)
		return NULL;

	if (of_property_read_string_index(clkspec.np, "clock-output-names",
					  index,
					  &clk_name) < 0) {
		/*
		 * Best effort to get the name if the clock has been
		 * registered with the framework. If the clock isn't
		 * registered, we return the node name as the name of
		 * the clock as long as #clock-cells = 0.
		 */
		clk = of_clk_get_from_provider(&clkspec);
		if (IS_ERR(clk)) {
			if (clkspec.args_count == 0)
				clk_name = clkspec.np->name;
			else
				clk_name = NULL;
		} else {
#if defined(CONFIG_COMMON_CLK)
			clk_name = __clk_get_name(clk);
			clk_put(clk);
#endif
		}
	}


	of_node_put(clkspec.np);
	return clk_name;
}
EXPORT_SYMBOL_GPL(of_clk_get_parent_name);

/**
 * of_clk_parent_fill() - Fill @parents with names of @np's parents and return
 * number of parents
 * @np: Device node pointer associated with clock provider
 * @parents: pointer to char array that hold the parents' names
 * @size: size of the @parents array
 *
 * Return: number of parents for the clock node.
 */
int of_clk_parent_fill(struct device_node *np, const char **parents,
		       unsigned int size)
{
	unsigned int i = 0;

	while (i < size && (parents[i] = of_clk_get_parent_name(np, i)) != NULL)
		i++;

	return i;
}
EXPORT_SYMBOL_GPL(of_clk_parent_fill);

#if defined(CONFIG_COMMON_CLK)

struct clock_provider {
	of_clk_init_cb_t clk_init_cb;
	struct device_node *np;
	struct list_head node;
};

/*
 * This function looks for a parent clock. If there is one, then it
 * checks that the provider for this parent clock was initialized, in
 * this case the parent clock will be ready.
 */
static int parent_ready(struct device_node *np)
{
	int i = 0;

	while (true) {
		struct clk *clk = of_clk_get(np, i);

		/* this parent is ready we can check the next one */
		if (!IS_ERR(clk)) {
			clk_put(clk);
			i++;
			continue;
		}

		/* at least one parent is not ready, we exit now */
		if (PTR_ERR(clk) == -EPROBE_DEFER)
			return 0;

		/*
		 * Here we make assumption that the device tree is
		 * written correctly. So an error means that there is
		 * no more parent. As we didn't exit yet, then the
		 * previous parent are ready. If there is no clock
		 * parent, no need to wait for them, then we can
		 * consider their absence as being ready
		 */
		return 1;
	}
}

/**
 * of_clk_detect_critical() - set CLK_IS_CRITICAL flag from Device Tree
 * @np: Device node pointer associated with clock provider
 * @index: clock index
 * @flags: pointer to clk_core->flags
 *
 * Detects if the clock-critical property exists and, if so, sets the
 * corresponding CLK_IS_CRITICAL flag.
 *
 * Do not use this function. It exists only for legacy Device Tree
 * bindings, such as the one-clock-per-node style that are outdated.
 * Those bindings typically put all clock data into .dts and the Linux
 * driver has no clock data, thus making it impossible to set this flag
 * correctly from the driver. Only those drivers may call
 * of_clk_detect_critical from their setup functions.
 *
 * Return: error code or zero on success
 */
int of_clk_detect_critical(struct device_node *np,
					  int index, unsigned long *flags)
{
	struct property *prop;
	const __be32 *cur;
	uint32_t idx;

	if (!np || !flags)
		return -EINVAL;

	of_property_for_each_u32(np, "clock-critical", prop, cur, idx)
		if (index == idx)
			*flags |= CLK_IS_CRITICAL;

	return 0;
}

/**
 * of_clk_init() - Scan and init clock providers from the DT
 * @matches: array of compatible values and init functions for providers.
 *
 * This function scans the device tree for matching clock providers
 * and calls their initialization functions. It also does it by trying
 * to follow the dependencies.
 */
void __init of_clk_init(const struct of_device_id *matches)
{
	const struct of_device_id *match;
	struct device_node *np;
	struct clock_provider *clk_provider, *next;
	bool is_init_done;
	bool force = false;
	LIST_HEAD(clk_provider_list);

	if (!matches)
		matches = &__clk_of_table;

	/* First prepare the list of the clocks providers */
	for_each_matching_node_and_match(np, matches, &match) {
		struct clock_provider *parent;

		if (!of_device_is_available(np))
			continue;

		parent = kzalloc(sizeof(*parent), GFP_KERNEL);
		if (!parent) {
			list_for_each_entry_safe(clk_provider, next,
						 &clk_provider_list, node) {
				list_del(&clk_provider->node);
				of_node_put(clk_provider->np);
				kfree(clk_provider);
			}
			of_node_put(np);
			return;
		}

		parent->clk_init_cb = match->data;
		parent->np = of_node_get(np);
		list_add_tail(&parent->node, &clk_provider_list);
	}

	while (!list_empty(&clk_provider_list)) {
		is_init_done = false;
		list_for_each_entry_safe(clk_provider, next,
					&clk_provider_list, node) {
			if (force || parent_ready(clk_provider->np)) {

				/* Don't populate platform devices */
				of_node_set_flag(clk_provider->np,
						 OF_POPULATED);

				clk_provider->clk_init_cb(clk_provider->np);
				of_clk_set_defaults(clk_provider->np, true);

				list_del(&clk_provider->node);
				of_node_put(clk_provider->np);
				kfree(clk_provider);
				is_init_done = true;
			}
		}

		/*
		 * We didn't manage to initialize any of the
		 * remaining providers during the last loop, so now we
		 * initialize all the remaining ones unconditionally
		 * in case the clock parent was not mandatory
		 */
		if (!is_init_done)
			force = true;
	}
}

#endif /* CONFIG_COMMON_CLK */

#endif
