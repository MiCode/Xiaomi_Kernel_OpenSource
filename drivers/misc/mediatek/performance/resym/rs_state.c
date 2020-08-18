/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/module.h>

#include <mt-plat/mtk_perfobserver.h>

#include "rs_state.h"


#define RSU_DEBUGFS_ENTRY(name) \
static int rsu_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, rsu_##name##_show, i->i_private); \
} \
\
static const struct file_operations rsu_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = rsu_##name##_open, \
	.read = seq_read, \
	.write = rsu_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static DEFINE_MUTEX(rsu_state_ntf_mutex);
static int is_throttled;

void rsu_get_state(int *throttled)
{
	mutex_lock(&rsu_state_ntf_mutex);
	*throttled = is_throttled;
	mutex_unlock(&rsu_state_ntf_mutex);
}

static int rsu_pob_eara_thrm_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	switch (val) {
	case POB_EARA_THRM_THROTTLED:
		mutex_lock(&rsu_state_ntf_mutex);
		is_throttled = 1;
		mutex_unlock(&rsu_state_ntf_mutex);
		break;
	case POB_EARA_THRM_UNTHROTTLED:
		mutex_lock(&rsu_state_ntf_mutex);
		is_throttled = 0;
		mutex_unlock(&rsu_state_ntf_mutex);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rsu_pob_eara_thrm_notifier = {
	.notifier_call = rsu_pob_eara_thrm_cb,
};

static int rsu_xState_show(struct seq_file *m, void *unused)
{
	int thrm_state = -1;

	rsu_get_state(&thrm_state);
	seq_printf(m, "thermal state: %d\n", thrm_state);

	return 0;
}

static ssize_t rsu_xState_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	uint32_t val;
	int ret;

	ret = kstrtou32_from_user(ubuf, cnt, 16, &val);
	if (ret)
		return ret;

	return cnt;
}

RSU_DEBUGFS_ENTRY(xState);

int __init rs_state_init(struct dentry *rs_debugfs_dir)
{
	struct dentry *rs_state_debugfs_dir = NULL;

	rs_state_debugfs_dir = debugfs_create_dir("state", rs_debugfs_dir);

	if (!rs_state_debugfs_dir)
		return -ENODEV;

	if (rs_state_debugfs_dir) {
		debugfs_create_file("xState",
				    0644,
				    rs_state_debugfs_dir,
				    NULL,
				    &rsu_xState_fops);
	}

	rsu_getstate_fp = rsu_get_state;

	pob_eara_thrm_register_client(&rsu_pob_eara_thrm_notifier);

	return 0;
}

void rs_state_exit(void)
{

}

