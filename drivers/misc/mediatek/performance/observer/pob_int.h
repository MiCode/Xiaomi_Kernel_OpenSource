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

#ifndef POB_INT_H
#define POB_INT_H

#include <linux/kobject.h>

#define POB_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

void *pob_alloc_atomic(int i32Size);
void pob_free(void *pvBuf);

void pob_trace(const char *fmt, ...);

/* sysfs */
#define POB_SYSFS_MAX_BUFF_SIZE 1024
#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)

void pob_sysfs_create_file(struct kobject *parent,
			struct kobj_attribute *kobj_attr);
void pob_sysfs_remove_file(struct kobject *parent,
			struct kobj_attribute *kobj_attr);

/* QoS */
int pob_qos_init(struct kobject *pob_kobj);
void pob_qos_exit(struct kobject *pob_kobj);
int pob_qos_ind_client_isemtpy(void);
void pob_qos_tracelog(unsigned long val, void *data);

#endif

