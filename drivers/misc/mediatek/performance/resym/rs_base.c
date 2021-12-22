// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "rs_base.h"

#define LOGSIZE 32

void *rs_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void rs_free(void *pvBuf)
{
	kvfree(pvBuf);
}

int rs_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj)
{
	struct kobject *psKobj = NULL;

	if (name == NULL || ppsKobj == NULL)
		return -1;

	parent = (parent != NULL) ? parent : rs_kobj;
	if (parent == NULL)
		return -1;

	psKobj = kobject_create_and_add(name, parent);
	if (!psKobj)
		return -1;

	*ppsKobj = psKobj;

	return 0;
}

void rs_sysfs_remove_dir(struct kobject **ppsKobj)
{
	if (ppsKobj == NULL)
		return;

	kobject_put(*ppsKobj);
	*ppsKobj = NULL;
}

void rs_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	int ret;

	if (kobj_attr == NULL)
		return;

	parent = (parent != NULL) ? parent : rs_kobj;
	if (parent == NULL)
		return;

	ret = sysfs_create_file(parent, &(kobj_attr->attr));
	if (ret)
		pr_debug("Failed to create sysfs file\n");
}

void rs_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;

	parent = (parent != NULL) ? parent : rs_kobj;
	if (parent == NULL)
		return;

	sysfs_remove_file(parent, &(kobj_attr->attr));
}

