/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>

#define TAG "[LT]"

struct LT_USER_DATA {
	void (*fn)(int loading);
	unsigned long polling_ms;
	u64 *prev_idle_time;
	u64 *prev_wall_time;
	struct hlist_node user_list_node;
	struct LT_WORK_DATA *link2work;
};

struct LT_WORK_DATA {
	struct LT_USER_DATA *link2user;
	struct delayed_work s_work;
	ktime_t target_expire;
};

static int nr_cpus;

static struct workqueue_struct *ps_lk_wq;
static HLIST_HEAD(lt_user_list);
DEFINE_MUTEX(lt_mlock);
static void lt_work_fn(struct work_struct *ps_work);

static inline void lt_lock(const char *tag)
{
	mutex_lock(&lt_mlock);
}

static inline void lt_unlock(const char *tag)
{
	mutex_unlock(&lt_mlock);
}

static inline void lt_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&lt_mlock));
}

static void *new_lt_work(struct LT_USER_DATA *lt_user)
{
	struct LT_WORK_DATA *new_work;

	lt_lockprove(__func__);
	new_work = kzalloc(sizeof(*new_work), GFP_KERNEL);
	if (!new_work)
		goto new_lt_work_out;

	new_work->link2user = lt_user;
	INIT_DELAYED_WORK(&new_work->s_work, lt_work_fn);
	new_work->target_expire =
		ktime_add_ms(ktime_get(), lt_user->polling_ms);

new_lt_work_out:
	return new_work;
}

static inline void free_lt_work(struct LT_WORK_DATA *lt_work)
{
	lt_lockprove(__func__);
	kfree(lt_work);
}

static int lt_update_loading(struct LT_USER_DATA *lt_data)
{
	int ret = -EOVERFLOW;
	int cpu;
	u64 cur_idle_time_i, cur_wall_time_i;
	u64 cpu_idle_time = 0, cpu_wall_time = 0;

	lt_lockprove(__func__);
	for_each_possible_cpu(cpu) {
		cur_idle_time_i = get_cpu_idle_time(cpu, &cur_wall_time_i, 1);
		cpu_idle_time += cur_idle_time_i - lt_data->prev_idle_time[cpu];
		cpu_wall_time += cur_wall_time_i - lt_data->prev_wall_time[cpu];
		lt_data->prev_idle_time[cpu] = cur_idle_time_i;
		lt_data->prev_wall_time[cpu] = cur_wall_time_i;
	}

	if (cpu_wall_time > 0 && cpu_wall_time >= cpu_idle_time)
		ret =
			div_u64((cpu_wall_time - cpu_idle_time) * 100,
				cpu_wall_time);

	return ret;
}

static void lt_work_fn(struct work_struct *ps_work)
{
	struct delayed_work *dwork;
	struct LT_WORK_DATA *lt_work;
	struct LT_USER_DATA *lt_user;
	ktime_t ktime_now;

	lt_lock(__func__);
	dwork = container_of(ps_work, struct delayed_work, work);
	lt_work = container_of(dwork, struct LT_WORK_DATA, s_work);
	lt_user = lt_work->link2user;
	if (lt_user) {
		lt_user->fn(lt_update_loading(lt_user));

		ktime_now = ktime_get();
		do {
			lt_work->target_expire =
				ktime_add_ms(lt_work->target_expire,
					lt_user->polling_ms);
		} while (!ktime_after(lt_work->target_expire, ktime_now));

		queue_delayed_work(ps_lk_wq, &lt_work->s_work,
			msecs_to_jiffies(ktime_ms_delta(
				lt_work->target_expire, ktime_now)));
	} else
		free_lt_work(lt_work);

	lt_unlock(__func__);
}

static void *new_lt_user(void (*fn)(int loading),
	unsigned long polling_ms)
{
	struct LT_USER_DATA *new_lt;
	int cpu;

	lt_lockprove(__func__);
	new_lt                 = kzalloc(sizeof(*new_lt), GFP_KERNEL);
	if (!new_lt)
		goto new_lt_alloc_err;

	new_lt->fn             = fn;
	new_lt->polling_ms     = polling_ms;
	new_lt->prev_idle_time =
		kcalloc(nr_cpus, sizeof(u64), GFP_KERNEL);
	if (!new_lt->prev_idle_time)
		goto new_lt_idle_alloc_err;

	new_lt->prev_wall_time =
		kcalloc(nr_cpus, sizeof(u64), GFP_KERNEL);
	if (!new_lt->prev_wall_time)
		goto new_lt_wall_alloc_err;

	for_each_possible_cpu(cpu)
		new_lt->prev_idle_time[cpu] =
			get_cpu_idle_time(cpu, &new_lt->prev_wall_time[cpu], 1);

	hlist_add_head(&new_lt->user_list_node, &lt_user_list);

	return new_lt;

new_lt_wall_alloc_err:
	kfree(new_lt->prev_idle_time);
new_lt_idle_alloc_err:
	kfree(new_lt);
new_lt_alloc_err:
	return NULL;
}

static void free_lt_user(struct LT_USER_DATA *node)
{
	lt_lockprove(__func__);
	if (node->link2work)
		node->link2work->link2user = NULL;

	hlist_del(&node->user_list_node);
	kfree(node->prev_idle_time);
	kfree(node->prev_wall_time);
	kfree(node);
}

static void lt_cleanup(void)
{
	struct LT_USER_DATA *ltiter = NULL;
	struct hlist_node *t;

	lt_lock(__func__);
	hlist_for_each_entry_safe(ltiter, t, &lt_user_list, user_list_node)
		free_lt_user(ltiter);

	lt_unlock(__func__);
}

int reg_loading_tracking_sp(void (*fn)(int loading), unsigned long polling_ms,
	const char *caller)
{
	struct LT_USER_DATA *ltiter = NULL, *new_user;
	struct LT_WORK_DATA *new_work;
	int ret = 0;

	might_sleep();

	lt_lock(__func__);
	if (!fn || polling_ms == 0 || !ps_lk_wq) {
		ret = -EINVAL;
		goto reg_loading_tracking_out;
	}

	hlist_for_each_entry(ltiter, &lt_user_list, user_list_node)
		if (ltiter->fn == fn)
			break;

	if (ltiter) {
		ret = -EINVAL;
		goto reg_loading_tracking_out;
	}

	new_user = new_lt_user(fn, polling_ms);
	if (!new_user) {
		ret = -ENOMEM;
		goto reg_loading_tracking_out;
	}

	new_work = new_lt_work(new_user);
	if (!new_work)
		goto reg_loading_tracking_new_work_err;

	new_user->link2work = new_work;

	queue_delayed_work(ps_lk_wq, &new_work->s_work,
		msecs_to_jiffies(new_user->polling_ms));

	pr_debug(TAG"%s %s success\n", __func__, caller);

	goto reg_loading_tracking_out;

reg_loading_tracking_new_work_err:
	free_lt_user(new_user);
reg_loading_tracking_out:
	lt_unlock(__func__);

	return ret;
}


int unreg_loading_tracking_sp(void (*fn)(int loading), const char *caller)
{
	struct LT_USER_DATA *ltiter = NULL;
	int ret = 0;

	might_sleep();

	lt_lock(__func__);
	if (!fn || !ps_lk_wq) {
		ret = -EINVAL;
		goto unreg_loading_tracking_out;
	}

	hlist_for_each_entry(ltiter, &lt_user_list, user_list_node)
		if (ltiter->fn == fn)
			break;

	if (!ltiter) {
		ret = -EINVAL;
		goto unreg_loading_tracking_out;
	}

	free_lt_user(ltiter);
	pr_debug(TAG"%s %s success\n", __func__, caller);

unreg_loading_tracking_out:
	lt_unlock(__func__);

	return ret;
}

static int __init load_track_init(void)
{
	nr_cpus = num_possible_cpus();
	ps_lk_wq = create_workqueue("lt_wq");
	if (!ps_lk_wq) {
		return -EFAULT;
		pr_debug(TAG"%s OOM\n", __func__);
	}

	return 0;
}

static void __exit load_track_exit(void)
{
	lt_cleanup();
	flush_workqueue(ps_lk_wq);
	destroy_workqueue(ps_lk_wq);
}

module_init(load_track_init);
module_exit(load_track_exit);
