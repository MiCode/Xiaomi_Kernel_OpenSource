// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/seq_file.h>

#include "mtk_cpuidle_cpc.h"
#include "mtk_cpuidle_status.h"
#include "mtk_idle_procfs.h"

static int idle_proc_kernel_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", __func__);

	return 0;
}

static ssize_t idle_proc_kernel_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	/* Start/Stop kernel profile */

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_target_state_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", __func__);

	return 0;
}

static ssize_t idle_proc_target_state_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	int state = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 10, &state) != 0) {
		ret = -EINVAL;
		goto free;
	}

	/* Set target state */

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_ratio_show(struct seq_file *m, void *v)
{
	mtk_cpuidle_prof_ratio_dump(m);

	return 0;
}

static ssize_t idle_proc_ratio_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	if (enabled)
		mtk_cpuidle_prof_ratio_start();
	else
		mtk_cpuidle_prof_ratio_stop();

free:
	mtk_idle_procfs_free(buf);

	return ret;
}


static int idle_proc_cpc_show(struct seq_file *m, void *v)
{
	mtk_cpc_prof_lat_dump(m);

	return 0;
}

static ssize_t idle_proc_cpc_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	if (enabled)
		mtk_cpc_prof_start();
	else
		mtk_cpc_prof_stop();

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

PROC_FOPS(kernel);
PROC_FOPS(target_state);
PROC_FOPS(ratio);
PROC_FOPS(cpc);
void __init mtk_idle_procfs_profile_dir_init(struct proc_dir_entry *parent)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	const struct idle_proc_entry entries[] = {
		PROC_ENTRY(kernel),
		PROC_ENTRY(target_state),
		PROC_ENTRY(ratio),
		PROC_ENTRY(cpc)
	};

	dir = proc_mkdir("profile", parent);

	if (!dir) {
		pr_notice("fail to create procfs @ %s()\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		PROC_CREATE_NODE(dir, entries[i]);
}
