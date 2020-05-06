// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#define L2_REUSE_SMC_ID 0x00200090C

static bool l2_reuse_enable;
static struct kobject *l2_reuse_kobj;

static ssize_t sysfs_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", l2_reuse_enable);
}

static ssize_t sysfs_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct arm_smccc_res res;
	int ret;

	ret = kstrtobool(buf, &l2_reuse_enable);
	if (ret) {
		pr_err("Invalid argument passed\n");
		return ret;
	}

	arm_smccc_smc(L2_REUSE_SMC_ID, l2_reuse_enable, 1, 0, 0, 0, 0, 0, &res);
	return count;
}

struct kobj_attribute l2_reuse_attr = __ATTR(extended_cache_enable, 0660,
		sysfs_show, sysfs_store);

static int __init l2_reuse_driver_init(void)
{
	l2_reuse_kobj = kobject_create_and_add("l2_reuse", power_kobj);

	if (!l2_reuse_kobj) {
		pr_info("kobj creation for l2_reuse failed\n");
		return 0;
	}

	if (sysfs_create_file(l2_reuse_kobj, &l2_reuse_attr.attr))
		kobject_put(l2_reuse_kobj);

	return 0;
}

void __exit l2_reuse_driver_exit(void)
{
	if (l2_reuse_kobj) {
		sysfs_remove_file(power_kobj, &l2_reuse_attr.attr);
		kobject_put(l2_reuse_kobj);
	}
}

module_init(l2_reuse_driver_init);
module_exit(l2_reuse_driver_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc L2 REUSE Module");
MODULE_LICENSE("GPL v2");
