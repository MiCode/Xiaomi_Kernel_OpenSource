// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include "linux/kernel.h"
#include "linux/slab.h"
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#define SD_SIZE 512 * 1024

extern struct dentry *ispv4_debugfs;

static struct resource xm_ispv4_fake_resource[] = {
	DEFINE_RES_DMA_NAMED(0, "sram"),
	DEFINE_RES_DMA_NAMED(0, "ddr"),
};

static int ispv4_fake_dev_inited = 0;

static void ispv4_fake_devrelease(struct device *dev)
{
	pr_info("ispv4: release.\n");
}

static struct platform_device ispv4_fake_dev = {
	.name = "xm-ispv4-fake",
	.id = -1,
	.num_resources = ARRAY_SIZE(xm_ispv4_fake_resource),
	.resource = xm_ispv4_fake_resource,
	.dev.release = ispv4_fake_devrelease,
};

static int ispv4_add_device(void)
{
	int rc;

	pr_info("ispv4: add device!\n");
	if (ispv4_fake_dev_inited != 0) {
		pr_err("ispv4: device is not clear!\n");
		return 0;
	}
	ispv4_fake_dev_inited = -1;

	rc = platform_device_register(&ispv4_fake_dev);
	if (rc != 0) {
		pr_info("ispv4: device add failed %d!\n", rc);
		return 0;
	}
	pr_info("ispv4: device add success!\n");
	ispv4_fake_dev_inited = 1;
	return 0;
}

static ssize_t ispv4_debugfs_deivce_add(struct file *f, const char __user *data,
					size_t len, loff_t *off)
{
	char k_buf[16];
	int rc;

	if (len > 16) {
		return 0;
	}

	(void)copy_from_user(k_buf, data, len);
	k_buf[15] = 0;

	if (!strcmp(k_buf, "add\n")) {
		rc = ispv4_add_device();
		if (rc != 0)
			return 0;
	}
	return len;
}

static const struct file_operations ispv4_add_ops = {
	.write = ispv4_debugfs_deivce_add,
	.open = simple_open,
	.llseek = generic_file_llseek,
};

struct dentry *xm_fake_add_entry;

static int fake_init(void)
{
	void *kma_sram, *kma_ddr;
	int rc;

	kma_sram = kmalloc(SD_SIZE, GFP_KERNEL);
	if (kma_sram == NULL) {
		rc = -1;
		goto sram_failed;
	}
	xm_ispv4_fake_resource[0].start = virt_to_phys(kma_sram);

	kma_ddr = kmalloc(SD_SIZE, GFP_KERNEL);
	if (kma_ddr == NULL) {
		rc = -1;
		goto ddr_failed;
	}
	xm_ispv4_fake_resource[1].start = virt_to_phys(kma_ddr);

	xm_fake_add_entry = debugfs_create_file(
		"ispv4_rproc_fake", 0666, ispv4_debugfs, NULL, &ispv4_add_ops);

	if (xm_fake_add_entry == NULL) {
		rc = -1;
		goto debugfs_failed;
	}

	xm_ispv4_fake_resource[0].end =
		xm_ispv4_fake_resource[0].start + SD_SIZE - 1;
	xm_ispv4_fake_resource[1].end =
		xm_ispv4_fake_resource[1].start + SD_SIZE - 1;

	pr_info("ispv4 fake dev: sram:0x%llx(0x%llx), ddr:0x%llx(0x%llx)\n",
		xm_ispv4_fake_resource[0].start, kma_sram,
		xm_ispv4_fake_resource[1].start, kma_ddr);
	pr_info("ispv4 fake dev: size : %d\n", SD_SIZE);

	return 0;

debugfs_failed:
	kfree(phys_to_virt(xm_ispv4_fake_resource[1].start));
	xm_ispv4_fake_resource[1].start = 0;
ddr_failed:
	kfree(phys_to_virt(xm_ispv4_fake_resource[0].start));
	xm_ispv4_fake_resource[0].start = 0;
sram_failed:
	return rc;
}

static void fake_deinit(void)
{
	if (xm_fake_add_entry != NULL) {
		debugfs_remove(xm_fake_add_entry);
	}
	if (ispv4_fake_dev_inited == 1)
		platform_device_unregister(&ispv4_fake_dev);
	if (xm_ispv4_fake_resource[1].start != 0)
		kfree(phys_to_virt(xm_ispv4_fake_resource[1].start));
	if (xm_ispv4_fake_resource[0].start != 0)
		kfree(phys_to_virt(xm_ispv4_fake_resource[0].start));
}

module_init(fake_init);
module_exit(fake_deinit);

MODULE_AUTHOR("ChenHonglin<chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4.");
MODULE_LICENSE("GPL v2");
