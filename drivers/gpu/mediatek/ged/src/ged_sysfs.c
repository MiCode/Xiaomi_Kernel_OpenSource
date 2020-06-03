// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "ged_sysfs.h"
#include "ged_base.h"

#define GED_SYSFS_DIR_NAME "ged"

static struct kobject *ged_kobj;

GED_ERROR ged_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj)
{
	struct kobject *psKobj = NULL;

	if (name == NULL || ppsKobj == NULL)
		return GED_ERROR_INVALID_PARAMS;

	parent = (parent != NULL) ? parent : ged_kobj;
	psKobj = kobject_create_and_add(name, parent);
	if (!psKobj) {
		GED_LOGE("Failed to create '%s' sysfs directory\n",
				name);
		return GED_ERROR_FAIL;
	}
	*ppsKobj = psKobj;

	return GED_OK;
}
// --------------------------------------------------
void ged_sysfs_remove_dir(struct kobject **ppsKobj)
{
	if (ppsKobj == NULL)
		return;
	kobject_put(*ppsKobj);
	*ppsKobj = NULL;
}
// --------------------------------------------------
GED_ERROR ged_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return GED_ERROR_INVALID_PARAMS;

	parent = (parent != NULL) ? parent : ged_kobj;
	if (sysfs_create_file(parent, &(kobj_attr->attr))) {
		GED_LOGE("Failed to create sysfs file\n");
		return GED_ERROR_FAIL;
	}

	return GED_OK;
}
// --------------------------------------------------
void ged_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;
	parent = (parent != NULL) ? parent : ged_kobj;
	sysfs_remove_file(parent, &(kobj_attr->attr));
}
// --------------------------------------------------
GED_ERROR ged_sysfs_init(void)
{
	ged_kobj = kobject_create_and_add(GED_SYSFS_DIR_NAME,
			kernel_kobj);
	if (!ged_kobj) {
		GED_LOGE("Failed to create '%s' sysfs root directory\n",
				GED_SYSFS_DIR_NAME);
		return GED_ERROR_FAIL;
	}

	return GED_OK;
}
// --------------------------------------------------
void ged_sysfs_exit(void)
{
	kobject_put(ged_kobj);
	ged_kobj = NULL;
}
