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
#include <mt-plat/mtk_perfobserver.h>

#include "rs_state.h"
#include "rs_base.h"

static DEFINE_MUTEX(rsu_state_ntf_mutex);
static int is_throttled;
static struct kobject *rss_kobj;

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

static ssize_t rs_state_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int thrm_state = -1;

	rsu_get_state(&thrm_state);

	return scnprintf(buf, PAGE_SIZE, "%d\n", thrm_state);
}
KOBJ_ATTR_RO(rs_state);

int __init rs_state_init(void)
{
	rsu_getstate_fp = rsu_get_state;

	pob_eara_thrm_register_client(&rsu_pob_eara_thrm_notifier);

	if (rs_sysfs_create_dir(NULL, "state", &rss_kobj))
		return -ENODEV;

	rs_sysfs_create_file(rss_kobj, &kobj_attr_rs_state);

	return 0;
}

void __exit rs_state_exit(void)
{
	rs_sysfs_remove_file(rss_kobj, &kobj_attr_rs_state);
	rs_sysfs_remove_dir(&rss_kobj);
}

