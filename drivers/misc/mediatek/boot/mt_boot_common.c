/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "["KBUILD_MODNAME"] " fmt
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <linux/atomic.h>
#include <mt-plat/mt_boot_common.h>


enum {
	BM_UNINIT = 0,
	BM_INITIALIZING = 1,
	BM_INITIALIZED = 2,
} BM_INIT_STATE;

enum boot_mode_t g_boot_mode __nosavedata = UNKNOWN_BOOT;
static atomic_t g_boot_init = ATOMIC_INIT(BM_UNINIT);
static atomic_t g_boot_errcnt = ATOMIC_INIT(0);
static atomic_t g_boot_status = ATOMIC_INIT(0);

#ifdef CONFIG_OF
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
};
static int __init dt_get_boot_common(unsigned long node, const char *uname, int depth, void *data)
{
	struct tag_bootmode *tags = NULL;

	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	tags = (struct tag_bootmode *)of_get_flat_dt_prop(node, "atag,boot", NULL);

	if (tags) {
		g_boot_mode = tags->bootmode;
		atomic_set(&g_boot_status, 1);
	} else {
		pr_warn("'atag,boot' is not found\n");
	}

	/* break now */
	return 1;
}
#endif


void init_boot_common(unsigned int line)
{
#ifdef CONFIG_OF
	int rc;

	if (BM_INITIALIZING == atomic_read(&g_boot_init)) {
		pr_warn("%s (%d) state(%d,%d)\n", __func__, line, atomic_read(&g_boot_init),
			g_boot_mode);
		atomic_inc(&g_boot_errcnt);
		return;
	}

	if (BM_UNINIT == atomic_read(&g_boot_init))
		atomic_set(&g_boot_init, BM_INITIALIZING);
	else
		return;

	if (UNKNOWN_BOOT != g_boot_mode) {
		atomic_set(&g_boot_init, BM_INITIALIZED);
		pr_warn("%s (%d) boot_mode = %d\n", __func__, line, g_boot_mode);
		return;
	}

	pr_debug("%s %d %d %d\n", __func__, line, g_boot_mode, atomic_read(&g_boot_init));
	rc = of_scan_flat_dt(dt_get_boot_common, NULL);
	if (0 != rc)
		atomic_set(&g_boot_init, BM_INITIALIZED);
	else
		pr_warn("of_scan_flat_dt() = %d", rc);
	pr_debug("%s %d %d %d\n", __func__, line, g_boot_mode, atomic_read(&g_boot_init));
#endif
}

/* return boot mode */
unsigned int get_boot_mode(void)
{
	init_boot_common(__LINE__);
	return g_boot_mode;
}
EXPORT_SYMBOL(get_boot_mode);

/* for convenience, simply check is meta mode or not */
bool is_meta_mode(void)
{
	init_boot_common(__LINE__);

	if (g_boot_mode == META_BOOT)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_meta_mode);

bool is_advanced_meta_mode(void)
{
	init_boot_common(__LINE__);

	if (g_boot_mode == ADVMETA_BOOT)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_advanced_meta_mode);

static ssize_t boot_show(struct kobject *kobj, struct attribute *a, char *buf)
{
	return sprintf(buf, "%d\n", get_boot_mode());
}

static ssize_t boot_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count)
{
	return count;
}

/* boot object */
static struct kobject boot_kobj;
static const struct sysfs_ops boot_sysfs_ops = {
	.show = boot_show,
	.store = boot_store,
};

/* boot attribute */
struct attribute boot_attr = { BOOT_SYSFS_ATTR, 0644 };

static struct attribute *boot_attrs[] = {
	&boot_attr,
	NULL
};

/* boot type */
static struct kobj_type boot_ktype = {
	.sysfs_ops = &boot_sysfs_ops,
	.default_attrs = boot_attrs
};

/* boot device node */
static dev_t boot_dev_num;
static struct cdev boot_cdev;
static const struct file_operations boot_fops = {
	.owner = THIS_MODULE,
	.open = NULL,
	.release = NULL,
	.write = NULL,
	.read = NULL,
	.unlocked_ioctl = NULL
};

/* boot device class */
static struct class *boot_class;
static struct device *boot_device;

static int __init create_sysfs(void)
{
	int ret;

	/* allocate device major number */
	if (alloc_chrdev_region(&boot_dev_num, 0, 1, BOOT_DEV_NAME) < 0) {
		pr_warn("fail to register chrdev\n");
		return -1;
	}

	/* add character driver */
	cdev_init(&boot_cdev, &boot_fops);
	ret = cdev_add(&boot_cdev, boot_dev_num, 1);
	if (ret < 0) {
		pr_warn("fail to add cdev\n");
		return ret;
	}

	/* create class (device model) */
	boot_class = class_create(THIS_MODULE, BOOT_DEV_NAME);
	if (IS_ERR(boot_class)) {
		pr_warn("fail to create class\n");
		return -1;
	}

	boot_device = device_create(boot_class, NULL, boot_dev_num, NULL, BOOT_DEV_NAME);
	if (IS_ERR(boot_device)) {
		pr_warn("fail to create device\n");
		return -1;
	}

	/* add kobject */
	ret = kobject_init_and_add(&boot_kobj, &boot_ktype, &(boot_device->kobj), BOOT_SYSFS);
	if (ret < 0) {
		pr_warn("fail to add kobject\n");
		return ret;
	}

	return 0;
}

static void __exit destroy_sysfs(void)
{
	cdev_del(&boot_cdev);
}

static int boot_mode_proc_show(struct seq_file *p, void *v)
{
	seq_puts(p, "\n\rMTK BOOT MODE : ");
	switch (g_boot_mode) {
	case NORMAL_BOOT:
		seq_puts(p, "NORMAL BOOT\n");
		break;
	case META_BOOT:
		seq_puts(p, "META BOOT\n");
		break;
	case ADVMETA_BOOT:
		seq_puts(p, "Advanced META BOOT\n");
		break;
	case ATE_FACTORY_BOOT:
		seq_puts(p, "ATE_FACTORY BOOT\n");
		break;
	case ALARM_BOOT:
		seq_puts(p, "ALARM BOOT\n");
		break;
	default:
		seq_puts(p, "UNKNOWN BOOT\n");
		break;
	}

	return 0;
}

static int boot_mode_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_mode_proc_show, NULL);
}

static const struct file_operations boot_mode_proc_fops = {
	.open = boot_mode_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init boot_common_core(void)
{
	init_boot_common(__LINE__);
	/* create proc entry at /proc/boot_mode */
	if (NULL == proc_create_data("boot_mode", S_IRUGO, NULL, &boot_mode_proc_fops, NULL))
		pr_warn("create procfs fail");

	/* create sysfs entry at /sys/class/BOOT/BOOT/boot */
	create_sysfs();
	return 0;
}

static int __init boot_common_init(void)
{
	pr_debug("boot_mode = %d, state(%d,%d,%d)", g_boot_mode,
		 atomic_read(&g_boot_init), atomic_read(&g_boot_errcnt),
		 atomic_read(&g_boot_status));
	return 0;
}

core_initcall(boot_common_core);
module_init(boot_common_init);
MODULE_DESCRIPTION("MTK Boot Information Common Driver");
MODULE_LICENSE("GPL");
