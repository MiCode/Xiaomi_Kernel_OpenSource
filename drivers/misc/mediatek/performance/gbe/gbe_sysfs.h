/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __GED_SYSFS_H__
#define __GED_SYSFS_H__

#include <linux/kobject.h>

//#include "gbe_type.h"

#define GBE_SYSFS_MAX_BUFF_SIZE 1024

#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)
#define KOBJ_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)

void gbe_sysfs_create_file(struct kobj_attribute *kobj_attr);
void gbe_sysfs_remove_file(struct kobj_attribute *kobj_attr);
void gbe_sysfs_init(void);
void gbe_sysfs_exit(void);

extern struct kobject *kernel_kobj;

#endif
