/* arch/arm/mach-msm/clock.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2013, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/mutex.h>
#include <trace/events/power.h>
#include <mach/clk-provider.h>
#include "clock.h"

struct handoff_clk {
	struct list_head list;
	struct clk *clk;
};
static LIST_HEAD(handoff_list);

struct handoff_vdd {
	struct list_head list;
	struct clk_vdd_class *vdd_class;
};
static LIST_HEAD(handoff_vdd_list);

static DEFINE_MUTEX(msm_clock_init_lock);

/* Find the voltage level required for a given rate. */
int find_vdd_level(struct clk *clk, unsigned long rate)
{
	int level;

	for (level = 0; level < clk->num_fmax; level++)
		if (rate <= clk->fmax[level])
			break;

	if (level == clk->num_fmax) {
		pr_err("Rate %lu for %s is greater than highest Fmax\n", rate,
			clk->dbg_name);
		return -EINVAL;
	}

	return level;
}

/* Update voltage level given the current votes. */
static int update_vdd(struct clk_vdd_class *vdd_class)
{
	int level, rc = 0, i, ignore;
	struct regulator **r = vdd_class->regulator;
	int *uv = vdd_class->vdd_uv;
	int *ua = vdd_class->vdd_ua;
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
		rc = regulator_set_voltage(r[i], uv[new_base + i],
					   uv[max_lvl + i]);
		if (rc)
			goto set_voltage_fail;

		if (ua) {
			rc = regulator_set_optimum_mode(r[i], ua[new_base + i]);
			rc = rc > 0 ? 0 : rc;
			if (rc)
				goto set_mode_fail;
		}
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
	/*
	 * set_optimum_mode could use voltage to derive mode.  Restore
	 * previous voltage setting for r[i] first.
	 */
	if (ua) {
		regulator_set_voltage(r[i], uv[cur_base + i], uv[max_lvl + i]);
		regulator_set_optimum_mode(r[i], ua[cur_base + i]);
	}

set_mode_fail:
	regulator_set_voltage(r[i], uv[cur_base + i], uv[max_lvl + i]);

set_voltage_fail:
	for (i--; i >= 0; i--) {
		regulator_set_voltage(r[i], uv[cur_base + i], uv[max_lvl + i]);
		if (ua)
			regulator_set_optimum_mode(r[i], ua[cur_base + i]);
		if (cur_lvl == 0 || cur_lvl == vdd_class->num_levels)
			regulator_disable(r[i]);
		else if (level == 0)
			ignore = regulator_enable(r[i]);
	}
	return rc;
}

/* Vote for a voltage level. */
int vote_vdd_level(struct clk_vdd_class *vdd_class, int level)
{
	int rc;

	if (level >= vdd_class->num_levels)
		return -EINVAL;

	mutex_lock(&vdd_class->lock);
	vdd_class->level_votes[level]++;
	rc = update_vdd(vdd_class);
	if (rc)
		vdd_class->level_votes[level]--;
	mutex_unlock(&vdd_class->lock);

	return rc;
}

/* Remove vote for a voltage level. */
int unvote_vdd_level(struct clk_vdd_class *vdd_class, int level)
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
	rc = update_vdd(vdd_class);
	if (rc)
		vdd_class->level_votes[level]++;
out:
	mutex_unlock(&vdd_class->lock);
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

/* Check if the rate is within the voltage limits of the clock. */
static bool is_rate_valid(struct clk *clk, unsigned long rate)
{
	int level;

	if (!clk->vdd_class)
		return true;

	level = find_vdd_level(clk, rate);
	return level >= 0;
}

/**
 * __clk_pre_reparent() - Set up the new parent before switching to it and
 * prevent the enable state of the child clock from changing.
 * @c: The child clock that's going to switch parents
 * @new: The new parent that the child clock is going to switch to
 * @flags: Pointer to scratch space to save spinlock flags
 *
 * Cannot be called from atomic context.
 *
 * Use this API to set up the @new parent clock to be able to support the
 * current prepare and enable state of the child clock @c. Once the parent is
 * set up, the child clock can safely switch to it.
 *
 * The caller shall grab the prepare_lock of clock @c before calling this API
 * and only release it after calling __clk_post_reparent() for clock @c (or
 * if this API fails). This is necessary to prevent the prepare state of the
 * child clock @c from changing while the reparenting is in progress. Since
 * this API takes care of grabbing the enable lock of @c, only atomic
 * operation are allowed between calls to __clk_pre_reparent and
 * __clk_post_reparent()
 *
 * The scratch space pointed to by @flags should not be altered before
 * calling __clk_post_reparent() for clock @c.
 *
 * See also: __clk_post_reparent()
 */
int __clk_pre_reparent(struct clk *c, struct clk *new, unsigned long *flags)
{
	int rc;

	if (c->prepare_count) {
		rc = clk_prepare(new);
		if (rc)
			return rc;
	}

	spin_lock_irqsave(&c->lock, *flags);
	if (c->count) {
		rc = clk_enable(new);
		if (rc) {
			spin_unlock_irqrestore(&c->lock, *flags);
			clk_unprepare(new);
			return rc;
		}
	}
	return 0;
}

/**
 * __clk_post_reparent() - Release requirements on old parent after switching
 * away from it and allow changes to the child clock's enable state.
 * @c:   The child clock that switched parents
 * @old: The old parent that the child clock switched away from or the new
 *	 parent of a failed reparent attempt.
 * @flags: Pointer to scratch space where spinlock flags were saved
 *
 * Cannot be called from atomic context.
 *
 * This API works in tandem with __clk_pre_reparent. Use this API to
 * - Remove prepare and enable requirements from the @old parent after
 *   switching away from it
 * - Or, undo the effects of __clk_pre_reparent() after a failed attempt to
 *   change parents
 *
 * The caller shall release the prepare_lock of @c that was grabbed before
 * calling __clk_pre_reparent() only after this API is called (or if
 * __clk_pre_reparent() fails). This is necessary to prevent the prepare
 * state of the child clock @c from changing while the reparenting is in
 * progress. Since this API releases the enable lock of @c, the limit to
 * atomic operations set by __clk_pre_reparent() is no longer present.
 *
 * The scratch space pointed to by @flags shall not be altered since the call
 * to  __clk_pre_reparent() for clock @c.
 *
 * See also: __clk_pre_reparent()
 */
void __clk_post_reparent(struct clk *c, struct clk *old, unsigned long *flags)
{
	if (c->count)
		clk_disable(old);
	spin_unlock_irqrestore(&c->lock, *flags);

	if (c->prepare_count)
		clk_unprepare(old);
}

int clk_prepare(struct clk *clk)
{
	int ret = 0;
	struct clk *parent;

	if (!clk)
		return 0;
	if (IS_ERR(clk))
		return -EINVAL;

	mutex_lock(&clk->prepare_lock);
	if (clk->prepare_count == 0) {
		parent = clk->parent;

		ret = clk_prepare(parent);
		if (ret)
			goto out;
		ret = clk_prepare(clk->depends);
		if (ret)
			goto err_prepare_depends;

		ret = vote_rate_vdd(clk, clk->rate);
		if (ret)
			goto err_vote_vdd;
		if (clk->ops->prepare)
			ret = clk->ops->prepare(clk);
		if (ret)
			goto err_prepare_clock;
	}
	clk->prepare_count++;
out:
	mutex_unlock(&clk->prepare_lock);
	return ret;
err_prepare_clock:
	unvote_rate_vdd(clk, clk->rate);
err_vote_vdd:
	clk_unprepare(clk->depends);
err_prepare_depends:
	clk_unprepare(parent);
	goto out;
}
EXPORT_SYMBOL(clk_prepare);

/*
 * Standard clock functions defined in include/linux/clk.h
 */
int clk_enable(struct clk *clk)
{
	int ret = 0;
	unsigned long flags;
	struct clk *parent;
	const char *name = clk ? clk->dbg_name : NULL;

	if (!clk)
		return 0;
	if (IS_ERR(clk))
		return -EINVAL;

	spin_lock_irqsave(&clk->lock, flags);
	WARN(!clk->prepare_count,
			"%s: Don't call enable on unprepared clocks\n", name);
	if (clk->count == 0) {
		parent = clk->parent;

		ret = clk_enable(parent);
		if (ret)
			goto err_enable_parent;
		ret = clk_enable(clk->depends);
		if (ret)
			goto err_enable_depends;

		trace_clock_enable(name, 1, smp_processor_id());
		if (clk->ops->enable)
			ret = clk->ops->enable(clk);
		if (ret)
			goto err_enable_clock;
	}
	clk->count++;
	spin_unlock_irqrestore(&clk->lock, flags);

	return 0;

err_enable_clock:
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
	const char *name = clk ? clk->dbg_name : NULL;
	unsigned long flags;

	if (IS_ERR_OR_NULL(clk))
		return;

	spin_lock_irqsave(&clk->lock, flags);
	WARN(!clk->prepare_count,
			"%s: Never called prepare or calling disable after unprepare\n",
			name);
	if (WARN(clk->count == 0, "%s is unbalanced", name))
		goto out;
	if (clk->count == 1) {
		struct clk *parent = clk->parent;

		trace_clock_disable(name, 0, smp_processor_id());
		if (clk->ops->disable)
			clk->ops->disable(clk);
		clk_disable(clk->depends);
		clk_disable(parent);
	}
	clk->count--;
out:
	spin_unlock_irqrestore(&clk->lock, flags);
}
EXPORT_SYMBOL(clk_disable);

void clk_unprepare(struct clk *clk)
{
	const char *name = clk ? clk->dbg_name : NULL;

	if (IS_ERR_OR_NULL(clk))
		return;

	mutex_lock(&clk->prepare_lock);
	if (WARN(!clk->prepare_count, "%s is unbalanced (prepare)", name))
		goto out;
	if (clk->prepare_count == 1) {
		struct clk *parent = clk->parent;

		WARN(clk->count,
			"%s: Don't call unprepare when the clock is enabled\n",
			name);

		if (clk->ops->unprepare)
			clk->ops->unprepare(clk);
		unvote_rate_vdd(clk, clk->rate);
		clk_unprepare(clk->depends);
		clk_unprepare(parent);
	}
	clk->prepare_count--;
out:
	mutex_unlock(&clk->prepare_lock);
}
EXPORT_SYMBOL(clk_unprepare);

int clk_reset(struct clk *clk, enum clk_reset_action action)
{
	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;

	if (!clk->ops->reset)
		return -ENOSYS;

	return clk->ops->reset(clk, action);
}
EXPORT_SYMBOL(clk_reset);

unsigned long clk_get_rate(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return 0;

	if (!clk->ops->get_rate)
		return clk->rate;

	return clk->ops->get_rate(clk);
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long start_rate;
	int rc = 0;
	const char *name = clk ? clk->dbg_name : NULL;

	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;

	if (!clk->ops->set_rate)
		return -ENOSYS;

	if (!is_rate_valid(clk, rate))
		return -EINVAL;

	mutex_lock(&clk->prepare_lock);

	/* Return early if the rate isn't going to change */
	if (clk->rate == rate && !(clk->flags & CLKFLAG_NO_RATE_CACHE))
		goto out;

	trace_clock_set_rate(name, rate, raw_smp_processor_id());

	start_rate = clk->rate;

	if (clk->ops->pre_set_rate)
		rc = clk->ops->pre_set_rate(clk, rate);
	if (rc)
		goto out;

	/* Enforce vdd requirements for target frequency. */
	if (clk->prepare_count) {
		rc = vote_rate_vdd(clk, rate);
		if (rc)
			goto err_vote_vdd;
	}

	rc = clk->ops->set_rate(clk, rate);
	if (rc)
		goto err_set_rate;
	clk->rate = rate;

	/* Release vdd requirements for starting frequency. */
	if (clk->prepare_count)
		unvote_rate_vdd(clk, start_rate);

	if (clk->ops->post_set_rate)
		clk->ops->post_set_rate(clk, start_rate);

out:
	mutex_unlock(&clk->prepare_lock);
	return rc;

err_set_rate:
	if (clk->prepare_count)
		unvote_rate_vdd(clk, rate);
err_vote_vdd:
	/* clk->rate is still the old rate. So, pass the new rate instead. */
	if (clk->ops->post_set_rate)
		clk->ops->post_set_rate(clk, rate);
	goto out;
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	long rrate;
	unsigned long fmax = 0, i;

	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;

	if (!clk->ops->round_rate)
		return -ENOSYS;

	for (i = 0; i < clk->num_fmax; i++)
		fmax = max(fmax, clk->fmax[i]);

	if (!fmax)
		fmax = ULONG_MAX;

	rate = min(rate, fmax);
	rrate = clk->ops->round_rate(clk, rate);
	if (rrate > fmax)
		return -EINVAL;
	return rrate;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;

	if (!clk->ops->set_max_rate)
		return -ENOSYS;

	return clk->ops->set_max_rate(clk, rate);
}
EXPORT_SYMBOL(clk_set_max_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int rc = 0;

	if (!clk->ops->set_parent && clk->parent == parent)
		return 0;

	if (!clk->ops->set_parent)
		return -ENOSYS;

	mutex_lock(&clk->prepare_lock);
	if (clk->parent == parent)
		goto out;
	rc = clk->ops->set_parent(clk, parent);
out:
	mutex_unlock(&clk->prepare_lock);

	return rc;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return NULL;

	return clk->parent;
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_flags(struct clk *clk, unsigned long flags)
{
	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;
	if (!clk->ops->set_flags)
		return -ENOSYS;

	return clk->ops->set_flags(clk, flags);
}
EXPORT_SYMBOL(clk_set_flags);

static LIST_HEAD(initdata_list);

static void init_sibling_lists(struct clk_lookup *clock_tbl, size_t num_clocks)
{
	struct clk *clk, *parent;
	unsigned n;

	for (n = 0; n < num_clocks; n++) {
		clk = clock_tbl[n].clk;
		parent = clk->parent;
		if (parent && list_empty(&clk->siblings))
			list_add(&clk->siblings, &parent->children);
	}
}

static void vdd_class_init(struct clk_vdd_class *vdd)
{
	struct handoff_vdd *v;

	if (!vdd)
		return;

	list_for_each_entry(v, &handoff_vdd_list, list) {
		if (v->vdd_class == vdd)
			return;
	}

	pr_debug("voting for vdd_class %s\n", vdd->class_name);
	if (vote_vdd_level(vdd, vdd->num_levels - 1))
		pr_err("failed to vote for %s\n", vdd->class_name);

	v = kmalloc(sizeof(*v), GFP_KERNEL);
	if (!v) {
		pr_err("Unable to kmalloc. %s will be stuck at max.\n",
			vdd->class_name);
		return;
	}

	v->vdd_class = vdd;
	list_add_tail(&v->list, &handoff_vdd_list);
}

static int __handoff_clk(struct clk *clk)
{
	enum handoff state = HANDOFF_DISABLED_CLK;
	struct handoff_clk *h = NULL;
	int rc;

	if (clk == NULL || clk->flags & CLKFLAG_INIT_DONE ||
	    clk->flags & CLKFLAG_SKIP_HANDOFF)
		return 0;

	if (clk->flags & CLKFLAG_INIT_ERR)
		return -ENXIO;

	/* Handoff any 'depends' clock first. */
	rc = __handoff_clk(clk->depends);
	if (rc)
		goto err;

	/*
	 * Handoff functions for the parent must be called before the
	 * children can be handed off. Without handing off the parents and
	 * knowing their rate and state (on/off), it's impossible to figure
	 * out the rate and state of the children.
	 */
	if (clk->ops->get_parent)
		clk->parent = clk->ops->get_parent(clk);

	if (IS_ERR(clk->parent)) {
		rc = PTR_ERR(clk->parent);
		goto err;
	}

	rc = __handoff_clk(clk->parent);
	if (rc)
		goto err;

	if (clk->ops->handoff)
		state = clk->ops->handoff(clk);

	if (state == HANDOFF_ENABLED_CLK) {

		h = kmalloc(sizeof(*h), GFP_KERNEL);
		if (!h) {
			rc = -ENOMEM;
			goto err;
		}

		rc = clk_prepare_enable(clk->parent);
		if (rc)
			goto err;

		rc = clk_prepare_enable(clk->depends);
		if (rc)
			goto err_depends;

		rc = vote_rate_vdd(clk, clk->rate);
		WARN(rc, "%s unable to vote for voltage!\n", clk->dbg_name);

		clk->count = 1;
		clk->prepare_count = 1;
		h->clk = clk;
		list_add_tail(&h->list, &handoff_list);

		pr_debug("Handed off %s rate=%lu\n", clk->dbg_name, clk->rate);
	}

	clk->flags |= CLKFLAG_INIT_DONE;

	return 0;

err_depends:
	clk_disable_unprepare(clk->parent);
err:
	kfree(h);
	clk->flags |= CLKFLAG_INIT_ERR;
	pr_err("%s handoff failed (%d)\n", clk->dbg_name, rc);
	return rc;
}

/**
 * msm_clock_register() - Register additional clock tables
 * @table: Table of clocks
 * @size: Size of @table
 *
 * Upon return, clock APIs may be used to control clocks registered using this
 * function.
 */
int msm_clock_register(struct clk_lookup *table, size_t size)
{
	int n = 0;

	mutex_lock(&msm_clock_init_lock);

	init_sibling_lists(table, size);

	/*
	 * Enable regulators and temporarily set them up at maximum voltage.
	 * Once all the clocks have made their respective vote, remove this
	 * temporary vote. The removing of the temporary vote is done at
	 * late_init, by which time we assume all the clocks would have been
	 * handed off.
	 */
	for (n = 0; n < size; n++)
		vdd_class_init(table[n].clk->vdd_class);

	/*
	 * Detect and preserve initial clock state until clock_late_init() or
	 * a driver explicitly changes it, whichever is first.
	 */
	for (n = 0; n < size; n++)
		__handoff_clk(table[n].clk);

	clkdev_add_table(table, size);

	clock_debug_register(table, size);

	mutex_unlock(&msm_clock_init_lock);

	return 0;
}
EXPORT_SYMBOL(msm_clock_register);

/**
 * msm_clock_init() - Register and initialize a clock driver
 * @data: Driver-specific clock initialization data
 *
 * Upon return from this call, clock APIs may be used to control
 * clocks registered with this API.
 */
int __init msm_clock_init(struct clock_init_data *data)
{
	if (!data)
		return -EINVAL;

	if (data->pre_init)
		data->pre_init();

	mutex_lock(&msm_clock_init_lock);
	if (data->late_init)
		list_add(&data->list, &initdata_list);
	mutex_unlock(&msm_clock_init_lock);

	msm_clock_register(data->table, data->size);

	if (data->post_init)
		data->post_init();

	return 0;
}

static int __init clock_late_init(void)
{
	struct handoff_clk *h, *h_temp;
	struct handoff_vdd *v, *v_temp;
	struct clock_init_data *initdata, *initdata_temp;
	int ret = 0;

	pr_info("%s: Removing enables held for handed-off clocks\n", __func__);

	mutex_lock(&msm_clock_init_lock);

	list_for_each_entry_safe(initdata, initdata_temp,
					&initdata_list, list) {
		ret = initdata->late_init();
		if (ret)
			pr_err("%s: %pS failed late_init.\n", __func__,
				initdata);
	}

	list_for_each_entry_safe(h, h_temp, &handoff_list, list) {
		clk_disable_unprepare(h->clk);
		list_del(&h->list);
		kfree(h);
	}

	list_for_each_entry_safe(v, v_temp, &handoff_vdd_list, list) {
		unvote_vdd_level(v->vdd_class, v->vdd_class->num_levels - 1);
		list_del(&v->list);
		kfree(v);
	}

	mutex_unlock(&msm_clock_init_lock);

	return ret;
}
/* clock_late_init should run only after all deferred probing
 * (excluding DLKM probes) has completed.
 */
late_initcall_sync(clock_late_init);
