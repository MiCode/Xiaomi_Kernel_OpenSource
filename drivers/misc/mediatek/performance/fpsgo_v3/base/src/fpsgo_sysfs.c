// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "fpsgo_sysfs.h"
#include "fpsgo_base.h"

#define FPSGO_SYSFS_DIR_NAME "fpsgo"

static struct kobject *fpsgo_kobj;

int fpsgo_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj)
{
	struct kobject *psKobj = NULL;

	if (name == NULL || ppsKobj == NULL) {
		FPSGO_LOGE("Failed to create sysfs directory %p %p\n",
				name, ppsKobj);
		return -1;
	}

	parent = (parent != NULL) ? parent : fpsgo_kobj;
	psKobj = kobject_create_and_add(name, parent);
	if (!psKobj) {
		FPSGO_LOGE("Failed to create '%s' sysfs directory\n",
				name);
		return -1;
	}
	*ppsKobj = psKobj;

	return 0;
}
// --------------------------------------------------
void fpsgo_sysfs_remove_dir(struct kobject **ppsKobj)
{
	if (ppsKobj == NULL)
		return;
	kobject_put(*ppsKobj);
	*ppsKobj = NULL;
}
// --------------------------------------------------
void fpsgo_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL) {
		FPSGO_LOGE("Failed to create '%s' sysfs file kobj_attr=NULL\n");
		return;
	}

	parent = (parent != NULL) ? parent : fpsgo_kobj;
	if (sysfs_create_file(parent, &(kobj_attr->attr))) {
		FPSGO_LOGE("Failed to create '%s' sysfs file\n",
				(kobj_attr->attr).name);
		return;
	}

}
// --------------------------------------------------
void fpsgo_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;
	parent = (parent != NULL) ? parent : fpsgo_kobj;
	sysfs_remove_file(parent, &(kobj_attr->attr));
}
// --------------------------------------------------
void fpsgo_sysfs_init(void)
{
	fpsgo_kobj = kobject_create_and_add(FPSGO_SYSFS_DIR_NAME,
			kernel_kobj);
	if (!fpsgo_kobj) {
		FPSGO_LOGE("Failed to create '%s' sysfs root directory\n",
				FPSGO_SYSFS_DIR_NAME);
	}

}
// --------------------------------------------------
void fpsgo_sysfs_exit(void)
{
	kobject_put(fpsgo_kobj);
	fpsgo_kobj = NULL;
}
