// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/topology.h>
#include <trace/hooks/topology.h>
#include <trace/events/sched.h>
#include <trace/hooks/sched.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <sched/sched.h>
#include "cpuqos_sys_common.h"

#define CREATE_TRACE_POINTS

static struct attribute *cpuqos_attrs[] = {
	&show_cpuqos_status_attr.attr,
	&set_cache_size_attr.attr,
	NULL,
};

static struct attribute_group cpuqos_attr_group = {
	.attrs = cpuqos_attrs,
};

static struct kobject *kobj;
int init_cpuqos_common_sysfs(void)
{
	int ret = 0;

	kobj = kobject_create_and_add("cpuqos",
				&cpu_subsys.dev_root->kobj);
	if (!kobj) {
		pr_info("cpuqos folder create failed\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(kobj, &cpuqos_attr_group);
	if (ret)
		goto error;
	kobject_uevent(kobj, KOBJ_ADD);

	return 0;

error:
	kobject_put(kobj);
	kobj = NULL;
	return ret;
}

void cleanup_cpuqos_common_sysfs(void)
{
	if (kobj) {
		sysfs_remove_group(kobj, &cpuqos_attr_group);
		kobject_put(kobj);
		kobj = NULL;
	}
}
