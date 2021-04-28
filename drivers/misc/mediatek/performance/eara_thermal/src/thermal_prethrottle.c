// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */
#include "mtk_thermal.h"
#include "mtk_thermal_monitor.h"
#include "thermal_internal.h"
#include "thermal_base.h"
#include "fpsgo_common.h"
#include "fstb.h"

#define EARA_MAX_COUNT 10

static int eara_enable;
static int condition;
static DECLARE_WAIT_QUEUE_HEAD(eara_thrm_q);
static DEFINE_MUTEX(pre_lock);

static int eara_fg_pid;
static int reset_flag;

static void set_tfps_diff(int max_cnt, int *pid, unsigned long long *buf_id, int *diff)
{
	int i;

	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	for (i = 0; i < max_cnt; i++) {
		if (pid[i] == 0)
			break;
		pr_debug("EARA set %d %llu: %d\n", pid[i], buf_id[i], diff[i]);
		eara2fstb_tfps_mdiff(pid[i], buf_id[i], diff[i]);
	}

	mutex_unlock(&pre_lock);
}

static void get_tfps_pair(int max_cnt, int *pid, unsigned long long *buf_id, int *tfps,
	char name[][16])
{
	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	memset(pid, 0, max_cnt * sizeof(int));
	memset(buf_id, 0, max_cnt * sizeof(unsigned long long));
	memset(tfps, 0, max_cnt * sizeof(int));
	eara2fstb_get_tfps(max_cnt, pid, buf_id, tfps, name);

	mutex_unlock(&pre_lock);
}

static void switch_eara(int enable)
{
	mutex_lock(&pre_lock);

	eara_enable = enable;

	mutex_unlock(&pre_lock);
}

static void query_info(int *reset)
{
	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	mutex_unlock(&pre_lock);

	wait_event_interruptible(eara_thrm_q, condition);

	mutex_lock(&pre_lock);
	*reset = reset_flag;
	reset_flag = 0;
	mutex_unlock(&pre_lock);
}

static void active_event(int is_active)
{
	int to_activate = 0;

	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	if (!condition && is_active)
		to_activate = 1;

	condition = is_active;

	mutex_unlock(&pre_lock);

	if (to_activate)
		wake_up_interruptible(&eara_thrm_q);
}

static ssize_t eara_fg_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int val = -1;

	mutex_lock(&pre_lock);
	val = eara_fg_pid;
	mutex_unlock(&pre_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t eara_fg_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[EARA_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < EARA_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, EARA_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val < 0)
		return count;

	mutex_lock(&pre_lock);
	if (eara_fg_pid != val) {
		eara_fg_pid = val;
		reset_flag = 1;
		pr_debug("EARA reset %d %d\n", reset_flag, eara_fg_pid);
	}
	mutex_unlock(&pre_lock);

	return count;
}

KOBJ_ATTR_RW(eara_fg_pid);

void __exit eara_thrm_pre_exit(void)
{
	eara_enable_fp = NULL;
	eara_query_info_fp = NULL;
	eara_set_tfps_diff_fp = NULL;
	eara_get_tfps_pair_fp = NULL;
	eara_pre_active_fp = NULL;

	eara_thrm_sysfs_remove_file(&kobj_attr_eara_fg_pid);
}

int __init eara_thrm_pre_init(void)
{
	eara_enable_fp = switch_eara;
	eara_query_info_fp = query_info;
	eara_set_tfps_diff_fp = set_tfps_diff;
	eara_get_tfps_pair_fp = get_tfps_pair;
	eara_pre_active_fp = active_event;

	eara_thrm_sysfs_create_file(&kobj_attr_eara_fg_pid);

	return 0;
}

