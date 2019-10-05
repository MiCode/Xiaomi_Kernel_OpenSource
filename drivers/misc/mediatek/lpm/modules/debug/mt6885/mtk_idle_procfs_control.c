// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/seq_file.h>

#include "mtk_cpuidle_status.h"
#include "mtk_idle_procfs.h"


static int idle_proc_log_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU idle log : %s\n",
		mtk_cpuidle_ctrl_log_sta_get() ?
		"Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/control/log\n");

	return 0;
}

static ssize_t idle_proc_log_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_ctrl_log_en(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_timer_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Limit sleep timer : %s\n",
		mtk_cpuidle_ctrl_timer_sta_get() ?
		"Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/control/timer\n");

	return 0;
}

static ssize_t idle_proc_timer_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_ctrl_timer_en(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_stress_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU idle stress : %s\n",
		mtk_cpuidle_get_stress_status() ?
		"Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/control/stress\n");

	return 0;
}

static ssize_t idle_proc_stress_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_set_stress_test(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_stress_time_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU idle stress interval time : %u\n",
		mtk_cpuidle_get_stress_time());


	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [us] > /proc/cpuidle/control/stress_time\n");

	return 0;
}

static ssize_t idle_proc_stress_time_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int interval_us;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &interval_us) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_set_stress_time(interval_us);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}


PROC_FOPS(log);
PROC_FOPS(timer);
PROC_FOPS(stress);
PROC_FOPS(stress_time);
void __init mtk_idle_procfs_control_dir_init(struct proc_dir_entry *parent)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	const struct idle_proc_entry entries[] = {
		PROC_ENTRY(log),
		PROC_ENTRY(timer),
		PROC_ENTRY(stress),
		PROC_ENTRY(stress_time)
	};

	dir = proc_mkdir("control", parent);

	if (!dir) {
		pr_notice("fail to create procfs @ %s()\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		PROC_CREATE_NODE(dir, entries[i]);
}
