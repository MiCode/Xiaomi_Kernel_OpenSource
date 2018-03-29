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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <asm/setup.h>
#include "devinfo.h"

static u32 *g_devinfo_data;
static u32 g_devinfo_size;
static struct cdev devinfo_cdev;
static struct class *devinfo_class;
static dev_t devinfo_dev;
static struct dentry *devinfo_segment_root;
static char devinfo_segment_buff[128];

/*****************************************************************************
*FUNCTION DEFINITION
*****************************************************************************/
static int devinfo_open(struct inode *inode, struct file *filp);
static int devinfo_release(struct inode *inode, struct file *filp);
static long devinfo_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t devinfo_segment_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);

/**************************************************************************
*EXTERN FUNCTION
**************************************************************************/
u32 devinfo_get_size(void)
{
	return g_devinfo_size;
}

u32 get_devinfo_with_index(u32 index)
{
	int size = devinfo_get_size();
	u32 ret = 0;

	if (((index >= 0) && (index < size)) && (g_devinfo_data != NULL))
		ret = g_devinfo_data[index];
	else {
		pr_err("devinfo data index out of range:%d\n", index);
		pr_err("devinfo data size:%d\n", size);
		ret = 0xFFFFFFFF;
	}

	return ret;
}
/**************************************************************************
*STATIC FUNCTION
**************************************************************************/

static const struct file_operations devinfo_fops = {
	.open = devinfo_open,
	.release = devinfo_release,
	.unlocked_ioctl   = devinfo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = devinfo_ioctl,
#endif
	.owner = THIS_MODULE,
};

static int devinfo_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int devinfo_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations devinfo_segment_fops = {
	.owner = THIS_MODULE,
	.read = devinfo_segment_read,
};

static ssize_t devinfo_segment_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	return simple_read_from_buffer(buf, len, ppos, devinfo_segment_buff, strlen(devinfo_segment_buff));
}

/**************************************************************************
*  DEV DRIVER IOCTL
**************************************************************************/
static long devinfo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 index = 0;
	int err   = 0;
	int ret   = 0;
	u32 data_read = 0;
	int size = devinfo_get_size();

	/* ---------------------------------- */
	/* IOCTL							  */
	/* ---------------------------------- */
	if (_IOC_TYPE(cmd) != DEV_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEV_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	/* ---------------------------------- */
	/* get dev info data				  */
	/* ---------------------------------- */
	case READ_DEV_DATA:
		if (copy_from_user((void *)&index, (void __user *)arg, sizeof(u32)))
			return -1;
		if (index < size) {
			data_read = get_devinfo_with_index(index);
			ret = copy_to_user((void __user *)arg, (void *)&(data_read), sizeof(u32));
		} else {
			pr_warn("%s Error! Data index larger than data size. index:%d, size:%d\n", MODULE_NAME,
				index, size);
			return -2;
		}
		break;
	}

	return 0;
}

/******************************************************************************
 * devinfo_init
 *
 * DESCRIPTION:
 *   Init the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   0 for success
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/
static int __init devinfo_init(void)
{
	int ret = 0;
	struct device *device;

	devinfo_dev = MKDEV(MAJOR_DEV_NUM, 0);
	pr_debug("[%s]init\n", MODULE_NAME);
	ret = register_chrdev_region(devinfo_dev, 1, DEV_NAME);
	if (ret) {
		pr_warn("[%s] register device failed, ret:%d\n", MODULE_NAME, ret);
		return ret;
	}
	/*create class*/
	devinfo_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(devinfo_class)) {
		ret = PTR_ERR(devinfo_class);
		pr_warn("[%s] register class failed, ret:%d\n", MODULE_NAME, ret);
		unregister_chrdev_region(devinfo_dev, 1);
		return ret;
	}
	/* initialize the device structure and register the device  */
	cdev_init(&devinfo_cdev, &devinfo_fops);
	devinfo_cdev.owner = THIS_MODULE;

	ret = cdev_add(&devinfo_cdev, devinfo_dev, 1);
	if (ret < 0) {
		pr_warn("[%s] could not allocate chrdev for the device, ret:%d\n", MODULE_NAME, ret);
		class_destroy(devinfo_class);
		unregister_chrdev_region(devinfo_dev, 1);
		return ret;
	}
	/*create device*/
	device = device_create(devinfo_class, NULL, devinfo_dev, NULL, "devmap");
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_warn("[%s]device create fail\n", MODULE_NAME);
		cdev_del(&devinfo_cdev);
		class_destroy(devinfo_class);
		unregister_chrdev_region(devinfo_dev, 1);
		return ret;
	}

	devinfo_segment_root = debugfs_create_dir("devinfo", NULL);
	if (!devinfo_segment_root)
		return -ENOMEM;

	if (!debugfs_create_file("segcode", 0444, devinfo_segment_root, NULL, &devinfo_segment_fops))
		return -ENOMEM;

	return 0;
}

#ifdef CONFIG_OF
static int __init devinfo_parse_dt(unsigned long node, const char *uname, int depth, void *data)
{
	struct devinfo_tag *tags;
	u32 size = 0;

	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	tags = (struct devinfo_tag *) of_get_flat_dt_prop(node, "atag,devinfo", NULL);
	if (tags) {
		size = tags->data_size;

		g_devinfo_data = kmalloc(sizeof(struct devinfo_tag) + (size * sizeof(u32)), GFP_KERNEL);
		g_devinfo_size = size;

		BUG_ON(size > 300); /* for size integer too big protection */

		memcpy(g_devinfo_data, tags->data, (size * sizeof(u32)));

		/* print chip id for debugging purpose */
		pr_debug("tag_devinfo_data size:%d\n", size);

		sprintf(devinfo_segment_buff, "segment code=0x%x\n", g_devinfo_data[DEVINFO_SEGCODE_INDEX]);

		pr_err("[devinfo][SegCode] Segment Code=0x%x\n", g_devinfo_data[DEVINFO_SEGCODE_INDEX]);

	} else
		sprintf(devinfo_segment_buff, "segment code=[Fail in parsing DT]\n");

	return 1;
}

static int __init devinfo_of_init(void)
{
	of_scan_flat_dt(devinfo_parse_dt, NULL);
	return 0;
}
#endif
/******************************************************************************
 * devinfo_exit
 *
 * DESCRIPTION:
 *   Free the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   None
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/
static void __exit devinfo_exit(void)
{
	debugfs_remove_recursive(devinfo_segment_root);
	cdev_del(&devinfo_cdev);
	class_destroy(devinfo_class);
	unregister_chrdev_region(devinfo_dev, 1);
}
#ifdef CONFIG_OF
early_initcall(devinfo_of_init);
#endif
module_init(devinfo_init);
module_exit(devinfo_exit);
MODULE_LICENSE("GPL");


