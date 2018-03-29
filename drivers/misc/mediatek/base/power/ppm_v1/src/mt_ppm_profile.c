/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/string.h>

#include "mt_ppm_internal.h"

#define TOTAL_STATE_NUM	(NR_PPM_POWER_STATE + 1)

struct ppm_profile {
	bool is_profiling;
	enum ppm_power_state cur_state;
	struct mutex lock;
	/* state transition profiling */
	unsigned long total_trans_time;
	ktime_t last_time;
	unsigned long long *time_in_state;
	unsigned int *trans_tbl;
	/* client execution time profiling */
	unsigned long long max_client_exec_time_us[NR_PPM_CLIENTS];
	unsigned long long avg_client_exec_time_us[NR_PPM_CLIENTS];
} ppm_profile_data;


static void ppm_profile_update_state_time(enum ppm_power_state state)
{
	ktime_t cur_time = ktime_get();
	unsigned long long delta_time;

	delta_time = ktime_to_ms(ktime_sub(cur_time, ppm_profile_data.last_time));
	ppm_profile_data.time_in_state[state] += delta_time;
	ppm_profile_data.last_time = cur_time;

	ppm_dbg(TIME_PROFILE, "@%s: state = %s, delta_time = %lld\n",
		__func__, ppm_get_power_state_name(state), delta_time);
}

static void ppm_profile_dump_time_in_state(struct seq_file *m)
{
	int i;

	seq_puts(m, "\n==========================================\n");
	seq_puts(m, "Time in each PPM state (ms)");
	seq_puts(m, "\n==========================================\n");

	/* ppm_profile_update_state_time(ppm_profile_data.cur_state); */

	seq_printf(m, "%-10s%-10s\n", "State", "Time");
	for (i = 0; i < TOTAL_STATE_NUM; i++)
		seq_printf(m, "%-10s%-10lld\n",
			ppm_get_power_state_name(i),
			ppm_profile_data.time_in_state[i]);
}

static void ppm_profile_dump_trans_tbl(struct seq_file *m)
{
	int i, j;

	seq_puts(m, "\n==========================================\n");
	seq_puts(m, "PPM state traslation table");
	seq_puts(m, "\n==========================================\n");

	seq_printf(m, "%-10s", "From/To");
	for (i = 0; i < TOTAL_STATE_NUM; i++)
		seq_printf(m, "%-10s", ppm_get_power_state_name(i));
	seq_puts(m, "\n");

	for (i = 0; i < TOTAL_STATE_NUM; i++) {
		seq_printf(m, "%-10s", ppm_get_power_state_name(i));
		for (j = 0; j < TOTAL_STATE_NUM; j++)
			if (i == j)
				seq_printf(m, "%-10s", "N/A");
			else
				seq_printf(m, "%-10d",
					ppm_profile_data.trans_tbl[i * TOTAL_STATE_NUM + j]);
		seq_puts(m, "\n");
	}

	seq_printf(m, "Total translation times = %ld\n", ppm_profile_data.total_trans_time);
}

static void ppm_profile_dump_client_exec_time(struct seq_file *m)
{
	int i;

	seq_puts(m, "\n==========================================\n");
	seq_puts(m, "PPM Client Execution Time (us)");
	seq_puts(m, "\n==========================================\n");

	seq_printf(m, "%-10s%-10s%-10s\n", "Client", "Avg Time", "Max Time");
	for_each_ppm_clients(i) {
		seq_printf(m, "%-10s%-10lld%-10lld\n", ppm_main_info.client_info[i].name,
			ppm_profile_data.avg_client_exec_time_us[i],
			ppm_profile_data.max_client_exec_time_us[i]);
	}
}

void ppm_profile_update_client_exec_time(enum ppm_client client, unsigned long long time)
{
	ppm_lock(&ppm_profile_data.lock);

	if (!ppm_profile_data.is_profiling)
		goto end;

	if (time > ppm_profile_data.max_client_exec_time_us[client])
		ppm_profile_data.max_client_exec_time_us[client] = time;
	ppm_profile_data.avg_client_exec_time_us[client] =
		(ppm_profile_data.avg_client_exec_time_us[client] + time) / 2;

	ppm_dbg(TIME_PROFILE, "@%s: client = %s, time = %lld\n",
		__func__, ppm_main_info.client_info[client].name, time);

end:
	ppm_unlock(&ppm_profile_data.lock);
}

void ppm_profile_state_change_notify(enum ppm_power_state old_state, enum ppm_power_state new_state)
{
	ppm_lock(&ppm_profile_data.lock);

	ppm_profile_data.cur_state = new_state;

	if (!ppm_profile_data.is_profiling)
		goto end;

	ppm_profile_update_state_time(old_state);
	ppm_profile_data.total_trans_time++;
	ppm_profile_data.trans_tbl[old_state * TOTAL_STATE_NUM + new_state]++;
end:
	ppm_unlock(&ppm_profile_data.lock);
}

static int ppm_dump_info_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_time_in_state(m);
	ppm_profile_dump_trans_tbl(m);
	ppm_profile_dump_client_exec_time(m);
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}

static int ppm_time_in_state_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_time_in_state(m);
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}

static int ppm_trans_tbl_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_trans_tbl(m);
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}

static int ppm_client_exec_time_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_client_exec_time(m);
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}

static int ppm_profile_on_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ppm_profile_data.is_profiling);

	return 0;
}

static ssize_t ppm_profile_on_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	int enable;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &enable)) {
		ppm_lock(&ppm_profile_data.lock);

		if ((ppm_profile_data.is_profiling && enable)
			|| (!ppm_profile_data.is_profiling && !enable))
			goto end;

		/* on: record current time and clear all data */
		/* off: update time_in_state */
		if (!enable) {
			ppm_info("profiling stop!\n");
			ppm_profile_data.is_profiling = false;
			ppm_profile_update_state_time(ppm_profile_data.cur_state);
		} else {
			int i, j;

			ppm_info("profiling start!\n");

			/* clear data */
			ppm_profile_data.total_trans_time = 0;

			for (i = 0; i < TOTAL_STATE_NUM; i++) {
				ppm_profile_data.time_in_state[i] = 0;

				for (j = 0; j < TOTAL_STATE_NUM; j++)
					ppm_profile_data.trans_tbl[i * TOTAL_STATE_NUM + j] = 0;
			}

			for_each_ppm_clients(i) {
				ppm_profile_data.max_client_exec_time_us[i] = 0;
				ppm_profile_data.avg_client_exec_time_us[i] = 0;
			}

			/* record current time and start profiling */
			ppm_profile_data.last_time = ktime_get();
			ppm_profile_data.is_profiling = true;
		}

end:
		ppm_unlock(&ppm_profile_data.lock);
	} else
		ppm_err("echo [0/1] > /proc/ppm/profile/profile_on\n");

	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RO(dump_info);
PROC_FOPS_RO(time_in_state);
PROC_FOPS_RO(trans_tbl);
PROC_FOPS_RO(client_exec_time);
PROC_FOPS_RW(profile_on);

int ppm_profile_init(void)
{
	int i, ret = 0;
	unsigned int alloc_size;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dump_info),
		PROC_ENTRY(time_in_state),
		PROC_ENTRY(trans_tbl),
		PROC_ENTRY(client_exec_time),
		PROC_ENTRY(profile_on),
	};

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, profile_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/profile/%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	/* allocate mem */
	alloc_size = (NR_PPM_POWER_STATE + 1) * sizeof(unsigned long long) +
		(NR_PPM_POWER_STATE + 1) * (NR_PPM_POWER_STATE + 1) * sizeof(unsigned int);
	ppm_profile_data.time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!ppm_profile_data.time_in_state) {
		ret = -ENOMEM;
		goto out;
	}
	ppm_profile_data.trans_tbl = (unsigned int *)
		(ppm_profile_data.time_in_state + (NR_PPM_POWER_STATE + 1));

	mutex_init(&ppm_profile_data.lock);

	ppm_profile_data.cur_state = PPM_POWER_STATE_NONE;

	ppm_info("%s done\n", __func__);

out:
	return ret;
}

void ppm_profile_exit(void)
{
	kfree(ppm_profile_data.time_in_state);
}

