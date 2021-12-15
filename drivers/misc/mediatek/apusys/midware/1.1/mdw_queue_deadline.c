// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/rbtree.h>
#include <linux/debugfs.h>
#include <apusys_device.h>
#include <linux/ktime.h>

#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "mdw_rsc.h"
#include "mdw_queue.h"

#define CREATE_TRACE_POINTS
#include "apu_sched_events.h"

struct dentry *apusys_dbg_deadline;
static u64 exp_decay_interval = 100;/* ms */

#define EXP_0 (3353) // exp(-100/500)
#define EXP_1 (3706) // exp(-100/1000)
#define EXP_2 (3896) // exp(-100/2000)
#define FIX_1 (4096)

static u64 exp_decay_factor[3] = {EXP_0, EXP_1, EXP_2};
static u64 exp_decay_base = FIX_1;
static u64 exp_boost_threshold = FIX_1;

static void mdw_queue_deadline_tracing(struct work_struct *data)
{
	struct delayed_work *work = to_delayed_work(data);
	struct deadline_root *root =
			container_of(work, struct deadline_root, work);
	struct mdw_queue *mq =
			container_of(root, struct mdw_queue, deadline);
	struct mdw_rsc_tab *tab =
			container_of(mq, struct mdw_rsc_tab, q);

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

static int mdw_queue_deadline_task_start(struct mdw_apu_sc *sc, void *q)
{
	uint64_t load = 0;
	struct mdw_rsc_tab *tab;
	struct deadline_root *root;

	tab = mdw_rsc_get_tab(sc->type);
	if (!tab)
		return 0;

	root = &tab->q.deadline;

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

static int mdw_queue_deadline_task_end(struct mdw_apu_sc *sc, void *q)
{
	uint64_t load = 0;
	struct mdw_rsc_tab *tab;
	struct deadline_root *root;

	tab = mdw_rsc_get_tab(sc->type);
	root = &tab->q.deadline;

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

static struct mdw_apu_sc *mdw_queue_deadline_pop(void *q)
{
	struct rb_node *node;
	struct mdw_apu_sc *entry = NULL;
	struct deadline_root *root = (struct deadline_root *)q;

	mutex_lock(&root->lock);
	node = rb_first_cached(&root->root);
	if (node) {
		entry = rb_entry(node, struct mdw_apu_sc, node);
		rb_erase_cached(node, &root->root);
	}
	if (entry) {
		atomic_dec(&root->cnt);
		mdw_rsc_update_avl_bmp(entry->type);
	}
	mutex_unlock(&root->lock);

	return entry;
}

static int mdw_queue_deadline_insert(struct mdw_apu_sc *sc,
	void *q, int is_front)
{
	struct deadline_root *root = (struct deadline_root *)q;
	struct rb_node **link = &root->root.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct mdw_apu_sc *entry;
	bool leftmost = true;

	mutex_lock(&root->lock);
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct mdw_apu_sc, node);
		if (entry->deadline > sc->deadline)
			link = &parent->rb_left;
		else {
			link = &parent->rb_right;
			leftmost = false;
		}
	}
	rb_link_node(&sc->node, parent, link);
	rb_insert_color_cached(&sc->node, &root->root, leftmost);
	atomic_inc(&root->cnt);
	mdw_rsc_update_avl_bmp(sc->type);
	mutex_unlock(&root->lock);

	return 0;
}

static int mdw_queue_deadline_len(void *q)
{
	struct deadline_root *root = (struct deadline_root *)q;

	return atomic_read(&root->cnt);
}

static int mdw_queue_deadline_delete(struct mdw_apu_sc *sc, void *q)
{
	struct deadline_root *root = (struct deadline_root *)q;

	mutex_lock(&root->lock);
	rb_erase_cached(&sc->node, &root->root);
	atomic_dec(&root->cnt);
	mdw_rsc_update_avl_bmp(sc->type);
	mutex_unlock(&root->lock);

	return -EINVAL;
}

void mdw_queue_deadline_destroy(void *q)
{
	struct deadline_root *root = (struct deadline_root *)q;

	debugfs_remove_recursive(apusys_dbg_deadline);
	cancel_delayed_work(&root->work);
}

int mdw_queue_deadline_init(struct deadline_root *root)
{
	struct dentry *tmp;
	struct mdw_queue *mq =
			container_of(root, struct mdw_queue, deadline);
	struct mdw_rsc_tab *tab =
			container_of(mq, struct mdw_rsc_tab, q);

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
	atomic_set(&root->cnt, 0);
	INIT_DELAYED_WORK(&root->work, mdw_queue_deadline_tracing);

	apusys_dbg_deadline =
		debugfs_create_dir("deadline", mdw_dbg_root);
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

	root->ops.task_start = mdw_queue_deadline_task_start;
	root->ops.task_end = mdw_queue_deadline_task_end;
	root->ops.pop = mdw_queue_deadline_pop;
	root->ops.insert = mdw_queue_deadline_insert;
	root->ops.delete = mdw_queue_deadline_delete;
	root->ops.len = mdw_queue_deadline_len;
	root->ops.destroy = mdw_queue_deadline_destroy;

	tmp = debugfs_create_dir("deadline_queue", tab->dbg_dir);
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
