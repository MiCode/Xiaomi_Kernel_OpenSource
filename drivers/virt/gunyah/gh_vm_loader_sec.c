// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <linux/gunyah/gh_vm.h>

#include "gh_vm_loader_private.h"

/* Structure per VM device node */
struct gh_sec_vm_dev {
	struct list_head list;
	const char *vm_name;
	struct device *dev;
};

/* Structure per VM: Binds struct gh_vm_struct and struct gh_sec_vm_dev */
struct gh_sec_vm_struct {
	struct gh_vm_struct *vm_struct;
	struct gh_sec_vm_dev *vm_dev;
	struct mutex vm_lock;
};

static DEFINE_SPINLOCK(gh_sec_vm_devs_lock);
static LIST_HEAD(gh_sec_vm_devs);

static struct gh_sec_vm_dev *get_sec_vm_dev_by_name(const char *vm_name)
{
	struct gh_sec_vm_dev *sec_vm_dev;

	spin_lock(&gh_sec_vm_devs_lock);

	list_for_each_entry(sec_vm_dev, &gh_sec_vm_devs, list) {
		if (!strcmp(sec_vm_dev->vm_name, vm_name)) {
			spin_unlock(&gh_sec_vm_devs_lock);
			return sec_vm_dev;
		}
	}

	spin_unlock(&gh_sec_vm_devs_lock);

	return NULL;
}

static void gh_vm_loader_sec_rm_notifier(struct gh_vm_struct *vm_struct,
					unsigned long cmd, void *data)
{
}

static long gh_vm_loader_sec_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	default:
		pr_err("Invalid IOCTL for secure loader\n");
		break;
	}

	return -EINVAL;
}

static int gh_vm_loader_sec_release(struct inode *inode, struct file *file)
{
	struct gh_vm_struct *vm_struct = file->private_data;
	struct gh_sec_vm_struct *sec_vm_struct;

	sec_vm_struct = gh_vm_loader_get_loader_data(vm_struct);
	if (sec_vm_struct) {
		gh_vm_loader_set_loader_data(vm_struct, NULL);
		mutex_destroy(&sec_vm_struct->vm_lock);
		kfree(sec_vm_struct);
	}

	/* vm_struct should not be accessed after this */
	gh_vm_loader_destroy_vm(vm_struct);

	return 0;
}

static const struct file_operations gh_vm_loader_sec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gh_vm_loader_sec_ioctl,
	.release = gh_vm_loader_sec_release,
};

static int gh_vm_loader_sec_vm_init(struct gh_vm_struct *vm_struct)
{
	struct gh_sec_vm_struct *sec_vm_struct;
	struct gh_sec_vm_dev *vm_dev;
	const char *vm_name;

	vm_name = gh_vm_loader_get_name(vm_struct);
	vm_dev = get_sec_vm_dev_by_name(vm_name);
	if (!vm_dev) {
		pr_err("Unable to find a registered secure VM by name: %s\n",
			vm_name);
		return -ENODEV;
	}

	sec_vm_struct = kzalloc(sizeof(*sec_vm_struct), GFP_KERNEL);
	if (!sec_vm_struct)
		return -ENOMEM;

	mutex_init(&sec_vm_struct->vm_lock);

	sec_vm_struct->vm_struct = vm_struct;
	sec_vm_struct->vm_dev = vm_dev;

	gh_vm_loader_set_loader_data(vm_struct, sec_vm_struct);

	return 0;
}

static int gh_vm_loader_sec_probe(struct platform_device *pdev)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	int ret;

	sec_vm_dev = devm_kzalloc(&pdev->dev,
				sizeof(*sec_vm_dev), GFP_KERNEL);
	if (!sec_vm_dev)
		return -ENOMEM;

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &sec_vm_dev->vm_name);
	if (ret)
		return ret;

	sec_vm_dev->dev = &pdev->dev;

	spin_lock(&gh_sec_vm_devs_lock);
	list_add(&sec_vm_dev->list, &gh_sec_vm_devs);
	spin_unlock(&gh_sec_vm_devs_lock);

	platform_set_drvdata(pdev, sec_vm_dev);

	return 0;
}

static int gh_vm_loader_sec_remove(struct platform_device *pdev)
{
	struct gh_sec_vm_dev *sec_vm_dev;

	sec_vm_dev = platform_get_drvdata(pdev);

	spin_lock(&gh_sec_vm_devs_lock);
	list_del(&sec_vm_dev->list);
	spin_unlock(&gh_sec_vm_devs_lock);

	return 0;
}

static const struct of_device_id gh_vm_loader_sec_match_table[] = {
	{ .compatible = "qcom,gh-vm-loader-sec" },
	{},
};

static struct platform_driver gh_vm_loader_sec_drv = {
	.probe = gh_vm_loader_sec_probe,
	.remove = gh_vm_loader_sec_remove,
	.driver = {
		.name = "gh_vm_loader_sec",
		.of_match_table = gh_vm_loader_sec_match_table,
	},
};

int gh_vm_loader_sec_init(void)
{
	return platform_driver_register(&gh_vm_loader_sec_drv);
}

void gh_vm_loader_sec_exit(void)
{
	platform_driver_unregister(&gh_vm_loader_sec_drv);
}

struct gh_vm_loader_info gh_vm_sec_loader_info = {
	.vm_fops = &gh_vm_loader_sec_fops,
	.gh_vm_loader_vm_init = gh_vm_loader_sec_vm_init,
	.gh_vm_loader_init = gh_vm_loader_sec_init,
	.gh_vm_loader_exit = gh_vm_loader_sec_exit,
	.gh_vm_loader_rm_notifier = gh_vm_loader_sec_rm_notifier,
};
