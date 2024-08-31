// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 */
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>

extern struct dentry *ispv4_debugfs;

static struct dentry *fdev_file_dir;
static u32 ispv4_mb_fake_reg[4];
static const char *reg_name[] = { "sdata", "info", "rdata", "ctrl" };
static struct debugfs_u32_array ispv4_mb_dregs;

DEFINE_DEBUGFS_ATTRIBUTE(ispv4_mbox_fakedev_fops, NULL, NULL, "%llu\n");

static struct resource fake_resource[] = { DEFINE_RES_MEM(
	(phys_addr_t)ispv4_mb_fake_reg, sizeof(ispv4_mb_fake_reg)) };

static void fake_dev_release(struct device *dev)
{
}

static struct platform_device fake_dev = {
	.name = "xm-ispv4-mbox",
	.id = -1,
	.num_resources = ARRAY_SIZE(fake_resource),
	.resource = fake_resource,
	.dev.release = fake_dev_release,
};

static int __init ispv4_mbox_fakedev_init(void)
{
	int i;

	ispv4_mb_dregs.array = ispv4_mb_fake_reg;
	ispv4_mb_dregs.n_elements = ARRAY_SIZE(ispv4_mb_fake_reg);

	fdev_file_dir = debugfs_create_dir("ispv4_mb_fake", ispv4_debugfs);
	if (IS_ERR_OR_NULL(fdev_file_dir)) {
		pr_info("ispv4: fake mailbox debugfs failed\n");
		return -1;
	}

	for (i = 0; i < 4; i++) {
		debugfs_create_u32(reg_name[i], 0666, fdev_file_dir,
				   &ispv4_mb_fake_reg[i]);
	}

	if (platform_device_register(&fake_dev) != 0) {
		pr_info("ispv4: device register failed\n");
		debugfs_remove(fdev_file_dir);
		return -1;
	}

	return 0;
}

static void __exit ispv4_mbox_fakedev_exit(void)
{
	debugfs_remove(fdev_file_dir);
	platform_device_unregister(&fake_dev);
}

module_init(ispv4_mbox_fakedev_init);
module_exit(ispv4_mbox_fakedev_exit);

MODULE_AUTHOR("Chenhonglin <chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4 mailbox Fake device");
MODULE_LICENSE("GPL v2");
