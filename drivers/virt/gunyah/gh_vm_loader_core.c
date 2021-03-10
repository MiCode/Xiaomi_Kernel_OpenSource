// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <linux/gunyah/gh_vm.h>

static long gh_vm_loader_dev_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;

	switch (cmd) {
	case GH_VM_GET_API_VERSION:
		return GH_VM_API_VERSION;
	default:
		pr_err("Invalid ioctl\n");
		return ret;
	};

	return ret;
}

static const struct file_operations gh_vm_loader_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gh_vm_loader_dev_ioctl,
	.llseek = noop_llseek,
};

static struct miscdevice gh_vm_loader_dev = {
	.name = "gh_vm",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &gh_vm_loader_dev_fops,
};

static int __init gh_vm_loader_init(void)
{
	return misc_register(&gh_vm_loader_dev);
}

static void __exit gh_vm_loader_exit(void)
{
	misc_deregister(&gh_vm_loader_dev);
}

module_init(gh_vm_loader_init);
module_exit(gh_vm_loader_exit);

MODULE_LICENSE("GPL v2");
