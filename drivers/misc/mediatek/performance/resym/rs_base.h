/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RS_BASE_H_
#define _RS_BASE_H_

#include <linux/kobject.h>

extern unsigned int mt_cpufreq_get_freq_by_idx(int id, int idx);

#define RS_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

void *rs_alloc_atomic(int i32Size);
void rs_free(void *pvBuf);

/* sysfs */
#define RS_SYSFS_MAX_BUFF_SIZE 1024
extern struct kobject *rs_kobj;

#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)
#define KOBJ_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)

int rs_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj);
void rs_sysfs_remove_dir(struct kobject **ppsKobj);
void rs_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
void rs_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);

#endif
