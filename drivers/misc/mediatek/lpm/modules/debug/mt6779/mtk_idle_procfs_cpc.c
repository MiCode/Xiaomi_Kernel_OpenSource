// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/seq_file.h>

#include "mtk_cpuidle_cpc.h"
#include "mtk_idle_procfs.h"

static int idle_proc_auto_off_show(struct seq_file *m, void *v)
{
	seq_printf(m, "auto_off mode : %s\n",
			cpc_get_auto_off_sta() ? "Enabled" : "Disabled");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/cpc/auto_off\n");

	return 0;
}

static ssize_t idle_proc_auto_off_write(struct file *filp,
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

	if (enabled)
		cpc_auto_off_en();
	else
		cpc_auto_off_dis();

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_auto_off_thres_show(struct seq_file *m, void *v)
{
	seq_printf(m, "auto_off threshold = %u us\n",
			(unsigned int)cpc_get_auto_off_thres());

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [us] > /proc/cpuidle/cpc/auto_off_thres\n");

	return 0;
}

static ssize_t idle_proc_auto_off_thres_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int threshold_us;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &threshold_us) != 0) {
		ret = -EINVAL;
		goto free;
	}

	cpc_set_auto_off_thres(threshold_us);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

PROC_FOPS(auto_off);
PROC_FOPS(auto_off_thres);
void __init mtk_idle_procfs_cpc_dir_init(struct proc_dir_entry *parent)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	const struct idle_proc_entry entries[] = {
		PROC_ENTRY(auto_off),
		PROC_ENTRY(auto_off_thres)
	};

	dir = proc_mkdir("cpc", parent);

	if (!dir) {
		pr_notice("fail to create procfs @ %s()\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		PROC_CREATE_NODE(dir, entries[i]);
}
