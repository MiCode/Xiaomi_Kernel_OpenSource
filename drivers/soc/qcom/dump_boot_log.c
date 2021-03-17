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
static size_t xbl_log_size;
static struct kobject *kobj;
static struct device_node *parent, *node;

static struct kobj_type xbl_log_kobj_type = {
	.sysfs_ops = &kobj_sysfs_ops,
};
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

static void free_xbl_log_buf(phys_addr_t paddr, size_t size)
{
	unsigned long pfn_start = 0, pfn_end = 0, pfn_idx = 0;

	memblock_free(paddr, size);
	pfn_start = paddr >> PAGE_SHIFT;
	pfn_end = (paddr + size) >> PAGE_SHIFT;
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));
}

static int __init dump_boot_log_init(void)
{
	int err = 1;
	struct resource res_log = {0,};
	phys_addr_t xbl_log_paddr = 0;
	void *addr = NULL;

	kobj = kcalloc(1, sizeof(*kobj), GFP_KERNEL);
	if (!kobj)
		return 1;


	err = kobject_init_and_add(kobj, &xbl_log_kobj_type, kernel_kobj, "xbl_log");
	if (err) {
		pr_err("xbl_log: cannot create kobject\n");
		goto kobj_fail;
	}

	kobject_get(kobj);
	if (IS_ERR_OR_NULL(kobj)) {
		err = PTR_ERR(kobj);

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

	err = sysfs_create_bin_file(kobj, &attribute);
	if (err) {
		pr_err("xbl_log: sysfs entry creation failed\n");
		goto kobj_fail;
	}
	xbl_log_buf = kzalloc(xbl_log_size, GFP_KERNEL);
	if (xbl_log_buf) {
		memcpy(xbl_log_buf, addr, xbl_log_size);
		xbl_log_buf[xbl_log_size-1] = '\0';
		memunmap(addr);
		if (xbl_log_size)
			free_xbl_log_buf(xbl_log_paddr, xbl_log_size);
		return 0;
	}

	kfree(xbl_log_buf);
kobj_fail:
	kobject_del(kobj);
	kfree(kobj);
remap_fail:
	if (node)
		of_node_put(node);
node_fail:
	if (parent)
		of_node_put(parent);
	return 1;
}

static void __exit dump_boot_log_exit(void)
{

	kfree(xbl_log_buf);
	kobject_del(kobj);
	kfree(kobj);
	if (node)
		of_node_put(node);
	if (parent)
		of_node_put(parent);
}

module_init(dump_boot_log_init);
module_exit(dump_boot_log_exit);
MODULE_DESCRIPTION("dump xbl log");
MODULE_LICENSE("GPL v2");
