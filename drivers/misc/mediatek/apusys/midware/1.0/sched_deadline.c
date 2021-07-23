/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/rbtree.h>
#include <linux/debugfs.h>
#include <apusys_device.h>
#include <linux/ktime.h>
#include "sched_deadline.h"
#include "resource_mgt.h"
#define CREATE_TRACE_POINTS
#include "apu_sched_events.h"

#define MAX_BOOST (100)

struct dentry *apusys_dbg_deadline;
static u64 exp_decay_interval = 100;/* ms */

#define EXP_0 (3353) // exp(-100/500)
#define EXP_1 (3706) // exp(-100/1000)
#define EXP_2 (3896) // exp(-100/2000)
#define FIX_1 (4096)

static u64 exp_decay_factor[3] = {EXP_0, EXP_1, EXP_2};
static u64 exp_decay_base = FIX_1;
static u64 exp_boost_threshold = FIX_1;

static void deadline_load_tracing(struct work_struct *data)
{
	struct delayed_work *work = to_delayed_work(data);
	struct deadline_root *root =
			container_of(work, struct deadline_root, work);
	struct apusys_res_table *tab =
			container_of(root, struct apusys_res_table, deadline_q);

	u64 *f = exp_decay_factor;
	u64 b = exp_decay_base;
	u64 *l = root->avg_load;
	u64 n = root->total_subcmd;
	u64 boost_threshold = tab->dev_num * exp_decay_base;

	l[0] = (l[0] * f[0]) / b + n * (b - f[0]);
	l[1] = (l[1] * f[1]) / b + n * (b - f[1]);
	l[2] = (l[2] * f[2]) / b + n * (b - f[2]);

	mutex_lock(&root->lock);
	if (l[0] > boost_threshold ||
		l[1] > boost_threshold ||
		l[2] > boost_threshold)
		root->trace_boost = true;
	else
		root->trace_boost = false;

	mdw_drv_debug("%s: %llu, %llu, %llu, boost: %llu\n",
		__func__,
		root->avg_load[0],
		root->avg_load[1],
		root->avg_load[2],
		boost_threshold);

	trace_deadline_load(tab->name, root->avg_load);

	/* Disable load tracing timer */
	if (l[0] == 0 && l[1] == 0 && l[2] == 0) {
		root->need_timer = true;
		root->trace_boost = false;
	} else
		schedule_delayed_work(work,
				msecs_to_jiffies(exp_decay_interval));
	mutex_unlock(&root->lock);
}

int deadline_queue_init(int type)
{
	struct dentry *tmp;
	struct apusys_res_table *normal_tab;
	struct apusys_res_table *tab = res_get_table(type);
	struct deadline_root *root = &tab->deadline_q;

	if (type < APUSYS_DEVICE_RT)
		return 0;

	root->root = RB_ROOT_CACHED;
	root->total_period = 0;
	root->total_runtime = 0;
	root->total_subcmd = 0;
	root->avg_load[0] = 0;
	root->avg_load[1] = 0;
	root->avg_load[2] = 0;
	root->need_timer = true;
	root->load_boost = 0;
	root->trace_boost = 0;
	mutex_init(&root->lock);
	INIT_DELAYED_WORK(&root->work, deadline_load_tracing);


	apusys_dbg_deadline =
		debugfs_create_dir("deadline", apusys_dbg_root);
	debugfs_create_u64("exp_decay_interval",
		0644, apusys_dbg_deadline, &exp_decay_interval);
	debugfs_create_u64("exp_decay_factor_0",
		0644, apusys_dbg_deadline, &exp_decay_factor[0]);
	debugfs_create_u64("exp_decay_factor_1",
		0644, apusys_dbg_deadline, &exp_decay_factor[1]);
	debugfs_create_u64("exp_decay_factor_2",
		0644, apusys_dbg_deadline, &exp_decay_factor[2]);
	debugfs_create_u64("exp_decay_base",
		0644, apusys_dbg_deadline, &exp_decay_base);
	debugfs_create_u64("exp_boost_threshold",
		0644, apusys_dbg_deadline, &exp_boost_threshold);


	normal_tab = res_get_table(type - APUSYS_DEVICE_RT);
	tmp = normal_tab->dbg_dir;

	debugfs_create_bool("load_boost", 0444, tmp, &root->load_boost);
	debugfs_create_bool("trace_boost", 0444, tmp, &root->trace_boost);
	debugfs_create_u64("total_runtime", 0444, tmp, &root->total_runtime);
	debugfs_create_u64("total_period", 0444, tmp, &root->total_period);
	debugfs_create_u64("total_subcmd", 0444, tmp, &root->total_subcmd);
	debugfs_create_u64("avg_load_0", 0444, tmp, &root->avg_load[0]);
	debugfs_create_u64("avg_load_1", 0444, tmp, &root->avg_load[1]);
	debugfs_create_u64("avg_load_2", 0444, tmp, &root->avg_load[2]);

	return 0;
}

void deadline_queue_destroy(int type)
{
	struct apusys_res_table *tab = res_get_table(type);
	struct deadline_root *root = &tab->deadline_q;

	debugfs_remove_recursive(apusys_dbg_deadline);
	cancel_delayed_work(&root->work);
}

int deadline_task_insert(struct apusys_subcmd *sc)
{
	struct apusys_res_table *tab = res_get_table(sc->type);
	struct deadline_root *root = &tab->deadline_q;
	struct rb_node **link = &root->root.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct apusys_subcmd *entry;
	bool leftmost = true;

	mutex_lock(&root->lock);
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct apusys_subcmd, node);
		if (entry->deadline > sc->deadline)
			link = &parent->rb_left;
		else {
			link = &parent->rb_right;
			leftmost = false;
		}
	}
	rb_link_node(&sc->node, parent, link);
	rb_insert_color_cached(&sc->node, &root->root, leftmost);
	mutex_unlock(&root->lock);
	return 0;
}

bool deadline_task_empty(int type)
{
	struct apusys_res_table *tab = res_get_table(type);
	struct deadline_root *root = &tab->deadline_q;

	if (rb_first_cached(&root->root))
		return false;
	else
		return true;
}

struct apusys_subcmd *deadline_task_pop(int type)
{
	struct rb_node *node;
	struct apusys_subcmd *entry = NULL;
	struct apusys_res_table *tab = res_get_table(type);
	struct deadline_root *root = &tab->deadline_q;

	if (type < APUSYS_DEVICE_RT)
		return NULL;

	mutex_lock(&root->lock);
	node = rb_first_cached(&root->root);
	if (node) {
		entry = rb_entry(node, struct apusys_subcmd, node);
		rb_erase_cached(node, &root->root);
	}
	mutex_unlock(&root->lock);
	return entry;
}

int deadline_task_remove(struct apusys_subcmd *sc)
{
	struct apusys_res_table *tab;
	struct deadline_root *root;

	if (sc == NULL)
		return 0;

	tab = res_get_table(sc->type);
	root = &tab->deadline_q;

	mutex_lock(&root->lock);
	if (sc)
		rb_erase_cached(&sc->node, &root->root);
	mutex_unlock(&root->lock);

	return 0;
}

int deadline_task_start(struct apusys_subcmd *sc)
{
	uint64_t load = 0;
	struct apusys_res_table *tab;
	struct deadline_root *root;

	if (sc == NULL)
		return 0;

	tab = res_get_table(sc->type);
	root = &tab->deadline_q;

	if (sc->period == 0 || tab == NULL) {/* Not a deadline task*/
		struct deadline_root *rt_root;
		struct apusys_res_table *rt_tab;

		if (sc->type < APUSYS_DEVICE_RT) {
			rt_tab = res_get_table(sc->type + APUSYS_DEVICE_RT);
			rt_root = &rt_tab->deadline_q;
			if (rt_tab != NULL && rt_root->need_timer)
				sc->cluster_size = INT_MAX;
		}
		return 0;
	}


	mutex_lock(&root->lock);
	root->total_period += sc->period;
	root->total_runtime += sc->runtime;
	root->total_subcmd++;
	load = (root->total_subcmd * root->total_runtime);

	if (unlikely(root->total_period == 0))
		load = 0;
	else
		load /= root->total_period;

	if (tab->dev_num <= load && root->load_boost == 0) {
		root->load_boost = 1;
		mdw_drv_debug("load_boost: %llu, %llu, %llu, %llu\n",
			sc->runtime, sc->period, root->total_period,
			root->total_runtime);
	}
	/* Enable load tracing timer */
	if (root->need_timer) {
		schedule_delayed_work(&root->work, 0);
		root->need_timer = false;
	}
	trace_deadline_task(tab->name, true, load);
	mutex_unlock(&root->lock);
	return 0;
}

int deadline_task_end(struct apusys_subcmd *sc)
{
	uint64_t load = 0;
	struct apusys_res_table *tab;
	struct deadline_root *root;

	if (sc == NULL)
		return 0;

	tab = res_get_table(sc->type);
	root = &tab->deadline_q;

	if (sc->period == 0 || tab == NULL) /* Not a deadline task*/
		return 0;

	mutex_lock(&root->lock);
	root->total_period -= sc->period;
	root->total_runtime -= sc->runtime;
	root->total_subcmd--;
	load = (root->total_subcmd * root->total_runtime);

	if (unlikely(root->total_period == 0))
		load = 0;
	else
		load /= root->total_period;

	if (tab->dev_num > load && root->load_boost == 1) {
		root->load_boost = 0;
		mdw_drv_debug("load_boost: %llu, %llu, %llu, %llu\n",
			sc->runtime, sc->period, root->total_period,
			root->total_runtime);
	}
	trace_deadline_task(tab->name, false, load);
	mutex_unlock(&root->lock);
	return 0;
}

int deadline_task_boost(struct apusys_subcmd *sc)
{
	struct apusys_res_table *tab;
	struct deadline_root *root;
	unsigned int suggest_time;

	if (sc == NULL)
		return 0;

	suggest_time = sc->c_hdr->cmn.suggest_time * 1000;

	tab = res_get_table(sc->type);
	root = &tab->deadline_q;


	if (sc->c_hdr->cmn.suggest_time != 0) {
		if (sc->c_hdr->cmn.driver_time < suggest_time)
			sc->boost_val -= 10;
		else if (sc->c_hdr->cmn.driver_time > suggest_time)
			sc->boost_val += 10;

		if (sc->boost_val > 100)
			sc->boost_val = 100;
		if (sc->boost_val < 0)
			sc->boost_val = 0;
	}

	if (root->load_boost || root->trace_boost)
		return MAX_BOOST;
	else
		return sc->boost_val;
}
