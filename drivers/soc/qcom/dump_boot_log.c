// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/memblock.h>
#include <linux/of_address.h>
#include <linux/kthread.h>
#include <linux/slab.h>

static char *xbl_log_buf;
size_t xbl_log_size;

static ssize_t xbl_log_show(struct file *fp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t offset, size_t count)
{
	if (offset < xbl_log_size)
		return scnprintf(buf, count, "%s", xbl_log_buf + offset);

	return 0;
}

static struct bin_attribute attribute =
__BIN_ATTR(xbl_log, 0444, xbl_log_show, NULL, 0);

static int xbl_log_kthread(void *arg)
{
	int err = 1;
	struct module_kobject *mkobj;
	struct device_node *parent = NULL, *node = NULL;
	struct resource res_log = {0,};
	phys_addr_t xbl_log_paddr = 0;
	void *addr = NULL;

	mkobj = kcalloc(1, sizeof(*mkobj), GFP_KERNEL);
	if (!mkobj)
		return 1;

	mkobj->mod = THIS_MODULE;
	mkobj->kobj.kset = module_kset;

	err = kobject_init_and_add(&mkobj->kobj, &module_ktype, NULL, "xbl_log");
	if (err) {
		pr_err("xbl_log: cannot create kobject\n");
		goto kobj_fail;
	}

	kobject_get(&mkobj->kobj);
	if (IS_ERR_OR_NULL(&mkobj->kobj)) {
		err = PTR_ERR(&mkobj->kobj);
		goto kobj_fail;
	}

	err = sysfs_create_bin_file(&mkobj->kobj, &attribute);
	if (err) {
		pr_err("xbl_log: sysfs entry creation failed\n");
		goto kobj_fail;
	}

	parent = of_find_node_by_path("/reserved-memory");
	if (!parent) {
		pr_err("xbl_log: reserved-memory node missing\n");
		goto kobj_fail;
	}

	node = of_find_node_by_name(parent, "uefi_log");
	if (!node) {
		pr_err("xbl_log: uefi_log node missing\n");
		goto node_fail;
	}

	if (of_address_to_resource(node, 0, &res_log))
		goto node_fail;

	xbl_log_paddr = res_log.start;
	xbl_log_size = resource_size(&res_log) - 1;
	pr_debug("xbl_log_addr = %x, size=%d\n", xbl_log_paddr, xbl_log_size);

	addr = memremap(xbl_log_paddr, xbl_log_size, MEMREMAP_WB);
	if (!addr) {
		pr_err("xbl_log: memremap failed\n");
		goto remap_fail;
	}

	xbl_log_buf = kzalloc(xbl_log_size, GFP_KERNEL);
	if (xbl_log_buf) {
		memcpy(xbl_log_buf, addr, xbl_log_size);
		xbl_log_buf[xbl_log_size-1] = '\0';
		memunmap(addr);
		return 0;
	}

	kfree(xbl_log_buf);
remap_fail:
	if (node)
		of_node_put(node);
node_fail:
	if (parent)
		of_node_put(parent);
kobj_fail:
	kobject_del(&mkobj->kobj);
	kfree(mkobj);
	return 1;
}

static int __init dump_boot_log_init(void)
{
	struct task_struct *xbl_log_task =
		kthread_run(xbl_log_kthread, NULL, "xbl_log");

	if (PTR_ERR_OR_ZERO(xbl_log_task))
		return PTR_ERR(xbl_log_task);
	else
		return 0;
}

subsys_initcall(dump_boot_log_init);
MODULE_DESCRIPTION("dump xbl log");
MODULE_LICENSE("GPL v2");
