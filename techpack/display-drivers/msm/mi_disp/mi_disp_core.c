/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mi_disp_core:[%s:%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/crypto.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/kobject.h>

#include "mi_disp_print.h"
#include "mi_disp_core.h"

static struct disp_core *g_disp_core = NULL;

int mi_disp_cdev_register(const char *name,
			const struct file_operations *fops, struct cdev **cdevp)
{
	int ret = 0;
	dev_t dev_id;
	struct cdev *cdev = NULL;

	ret = alloc_chrdev_region(&dev_id, 0, 1, name);
	if (ret < 0) {
		DISP_ERROR("allocate chrdev region failed for %s\n", name);
		goto err_exit;
	}

	cdev = cdev_alloc();
	if (!cdev) {
		DISP_ERROR("cdev alloc failed for %s\n", name);
		ret = -ENOMEM;
		goto err_chrdev_unreg;
	}

	cdev->owner = fops->owner;
	cdev->ops = fops;
	kobject_set_name(&cdev->kobj, "%s", name);

	ret = cdev_add(cdev, dev_id, 1);
	if (ret < 0) {
		DISP_ERROR("cdev add failed for %s\n", name);
		goto err_cdev_del;
	}

	DISP_INFO("cdev name = %s, dev_id = (%d:%d)", name, MAJOR(dev_id), MINOR(dev_id));
	*cdevp = cdev;
	return 0;

err_cdev_del:
	cdev_del(cdev);
	cdev = NULL;
err_chrdev_unreg:
	unregister_chrdev_region(dev_id, 1);
err_exit:
	return ret;
}

void mi_disp_cdev_unregister(struct cdev *cdev)
{
	unregister_chrdev_region(cdev->dev, 1);
	cdev_del(cdev);
	cdev = NULL;
}

int mi_disp_class_device_register(struct device *dev)
{
	if (!g_disp_core || IS_ERR(g_disp_core->class))
		return -ENOENT;

	dev->class = g_disp_core->class;
	return device_register(dev);
}

void mi_disp_class_device_unregister(struct device *dev)
{
	return device_unregister(dev);
}

struct disp_core * mi_get_disp_core(void)
{
	int ret = 0;

	if (!g_disp_core) {
		ret = mi_disp_core_init();
		if (ret < 0)
			return NULL;
	}

	return g_disp_core;
}

static char *mi_disp_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "%s/%s", MI_DISPLAY_CLASS, dev_name(dev));
}

int mi_disp_core_init(void)
{
	int ret = 0;
	struct disp_core *disp_core = NULL;

	if (g_disp_core) {
		DISP_INFO("mi disp_core already initialized, return!\n");
		return 0;
	}

	disp_core = kzalloc(sizeof(*disp_core), GFP_KERNEL);
	if (!disp_core) {
		DISP_ERROR("can not allocate memory\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	disp_core->class = class_create(THIS_MODULE, MI_DISPLAY_CLASS);
	if (IS_ERR(disp_core->class)) {
		DISP_ERROR("class_create failed for %s\n", MI_DISPLAY_CLASS);
		ret = PTR_ERR(disp_core->class);
		goto err_free_mem;
	}
	disp_core->class->devnode = mi_disp_devnode;

	disp_core->procfs_dir = proc_mkdir(MI_DISPLAY_PROCFS_DIR, NULL);
	if (!disp_core->procfs_dir) {
		DISP_ERROR("proc_mkdir failed for %s\n", MI_DISPLAY_PROCFS_DIR);
		ret = -ENOMEM;
		goto err_class_destroy;
	}

	disp_core->debugfs_dir = debugfs_create_dir(MI_DISPLAY_DEBUGFS_DIR, NULL);
	if (!disp_core->debugfs_dir) {
		DISP_ERROR("debugfs_create_dir failed for %s\n", MI_DISPLAY_DEBUGFS_DIR);
		ret = -ENOMEM;
		goto err_procfs_remove;
	}

	g_disp_core = disp_core;

	DISP_INFO("mi disp_core driver initialized!\n");

	return 0;

err_procfs_remove:
	remove_proc_entry(MI_DISPLAY_PROCFS_DIR, NULL);
err_class_destroy:
	class_destroy(disp_core->class);
err_free_mem:
	kfree(disp_core);
err_exit:
	return ret;
}

void mi_disp_core_deinit(void)
{
	if (!g_disp_core)
		return;
	debugfs_remove_recursive(g_disp_core->debugfs_dir);
	remove_proc_entry(MI_DISPLAY_PROCFS_DIR, NULL);
	class_destroy(g_disp_core->class);
	kfree(g_disp_core);
	g_disp_core = NULL;
}

