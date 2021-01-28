// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "gbe_sysfs.h"

#define GBE_SYSFS_DIR_NAME "gbe"

static struct kobject *gbe_kobj;
// --------------------------------------------------
void gbe_sysfs_create_file(struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL || gbe_kobj == NULL) {
		pr_debug("Failed to create '%s' sysfs file kobj_attr=NULL\n");
		return;
	}

	if (sysfs_create_file(gbe_kobj, &(kobj_attr->attr))) {
		pr_debug("Failed to create sysfs file\n");
		return;
	}

}
// --------------------------------------------------
void gbe_sysfs_remove_file(struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL || gbe_kobj == NULL)
		return;

	sysfs_remove_file(gbe_kobj, &(kobj_attr->attr));
}
// --------------------------------------------------
void gbe_sysfs_init(void)
{
	gbe_kobj = kobject_create_and_add(GBE_SYSFS_DIR_NAME,
			kernel_kobj);
	if (!gbe_kobj) {
		pr_debug("Failed to create '%s' sysfs root directory\n",
				GBE_SYSFS_DIR_NAME);
	}

}
// --------------------------------------------------
void gbe_sysfs_exit(void)
{
	kobject_put(gbe_kobj);
	gbe_kobj = NULL;
}
