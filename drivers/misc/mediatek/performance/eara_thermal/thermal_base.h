/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __EARA_THERMAL_BASE_H__
#define __EARA_THERMAL_BASE_H__

#include <linux/kobject.h>

#define EARA_SYSFS_MAX_BUFF_SIZE 1024

#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)
#define KOBJ_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)

void eara_thrm_tracelog(const char *fmt, ...);
void eara_thrm_systrace(pid_t pid, int val, const char *fmt, ...);
void eara_thrm_sysfs_create_file(struct kobj_attribute *kobj_attr);
void eara_thrm_sysfs_remove_file(struct kobj_attribute *kobj_attr);
int eara_thrm_base_init(void);
void eara_thrm_base_exit(void);

#endif
