/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __GED_SYSFS_H__
#define __GED_SYSFS_H__

#include <linux/kobject.h>

#include "ged_type.h"

#define GED_SYSFS_MAX_BUFF_SIZE 128

#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)
#define KOBJ_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)

GED_ERROR ged_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj);
void ged_sysfs_remove_dir(struct kobject **ppsKobj);
GED_ERROR ged_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
void ged_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
GED_ERROR ged_sysfs_init(void);
void ged_sysfs_exit(void);

extern struct kobject *kernel_kobj;

#endif
