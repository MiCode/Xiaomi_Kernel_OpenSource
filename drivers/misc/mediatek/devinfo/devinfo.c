// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 *
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
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#include <linux/of.h>
#endif
#include <linux/atomic.h>
#include <asm/setup.h>
#include <mt-plat/mtk_devinfo.h>
#include "devinfo.h"

enum {
	DEVINFO_UNINIT = 0,
	DEVINFO_INITIALIZED = 1
} DEVINFO_INIT_STATE;

static u32 *g_devinfo_data;
static u32 g_devinfo_size;
static u32 g_hrid_size = HRID_DEFAULT_SIZE;
static struct cdev devinfo_cdev;
static struct class *devinfo_class;
static dev_t devinfo_dev;
static struct dentry *devinfo_segment_root;
static char devinfo_segment_buff[128];
static atomic_t g_devinfo_init_status = ATOMIC_INIT(DEVINFO_UNINIT);
static atomic_t g_devinfo_init_errcnt = ATOMIC_INIT(0);
static struct device_node *chosen_node;

/*****************************************************************************
 *FUNCTION DEFINITION
 *****************************************************************************/
static int devinfo_open(struct inode *inode, struct file *filp);
static int devinfo_release(struct inode *inode, struct file *filp);
static long devinfo_ioctl(struct file *file, u32 cmd, unsigned long arg);
static ssize_t devinfo_segment_read(struct file *filp, char __user *buf,
		size_t len, loff_t *ppos);
static void init_devinfo_exclusive(void);
static void devinfo_parse_dt(void);

/**************************************************************************
 *EXTERN FUNCTION
 **************************************************************************/
u32 devinfo_get_size(void)
{
	return g_devinfo_size;
}
EXPORT_SYMBOL(devinfo_get_size);

u32 devinfo_ready(void)
{
	if (devinfo_get_size() > 0)
		return 1;
	return 0;
}
EXPORT_SYMBOL(devinfo_ready);

u32 get_devinfo_with_index(u32 index)
{
	int size = devinfo_get_size();
	u32 ret = 0;

#ifdef CONFIG_OF
	if (size == 0) {
		/* Devinfo API users may call this API earlier than devinfo
		 * data is ready from dt. If the earlier API users found,
		 * make the devinfo data init earlier at that time.
		 */
		init_devinfo_exclusive();
		size = devinfo_get_size();
	}
#endif

	if (((index >= 0) && (index < size)) && (g_devinfo_data != NULL))
		ret = g_devinfo_data[index];
	else {
		pr_info("%s data index %d is larger than total size %d\n",
				MODULE_NAME, index, size);
		ret = 0xFFFFFFFF;
	}

	return ret;
}
EXPORT_SYMBOL(get_devinfo_with_index);

u32 get_hrid_size(void)
{
#ifdef CONFIG_OF
	if (devinfo_get_size() == 0)
		init_devinfo_exclusive();
#endif

	return g_hrid_size;
}
EXPORT_SYMBOL(get_hrid_size);

u32 get_hrid(unsigned char *rid, unsigned char *rid_sz)
{
	u32 ret = E_SUCCESS;
	u32 i, j;
	u32 reg_val = 0;
	u32 rid_temp_val = 0;

#ifdef CONFIG_OF
	if (devinfo_get_size() == 0)
		init_devinfo_exclusive();
#endif

	if (rid_sz == NULL)
		return E_BUF_SIZE_ZERO_OR_NULL;

	if (rid == NULL)
		return E_BUF_ZERO_OR_NULL;

	if (*rid_sz < (g_hrid_size * 4))
		return E_BUF_NOT_ENOUGH;

	for (i = 0; i < g_hrid_size; i++) {
		reg_val = get_devinfo_with_index(12 + i);
		for (j = 0; j < 4; j++) {
			rid_temp_val = (reg_val & (0xff << (8 * j))) >> (8 * j);
			*(rid + i * 4 + j) = rid_temp_val;
		}
	}

	*rid_sz = g_hrid_size * 4;

	return ret;
}
EXPORT_SYMBOL(get_hrid);

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

static ssize_t devinfo_segment_read(struct file *filp, char __user *buf,
		size_t len, loff_t *ppos)
{
	return simple_read_from_buffer(buf, len, ppos, devinfo_segment_buff,
			strlen(devinfo_segment_buff));
}

/**************************************************************************
 *  DEV DRIVER IOCTL
 **************************************************************************/
static long devinfo_ioctl(struct file *file, u32 cmd, unsigned long arg)
{
	u32 index = 0;
	int err   = 0;
	int ret   = 0;
	u32 data_read = 0;
	int size = devinfo_get_size();

	/* IOCTL */
	if (_IOC_TYPE(cmd) != DEV_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEV_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				_IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				_IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	/* get dev info data */
	case READ_DEV_DATA:
		if (copy_from_user((void *)&index, (void __user *)arg,
					sizeof(u32)))
			return -1;
		if (index < size) {
			data_read = get_devinfo_with_index(index);
			ret = copy_to_user((void __user *)arg,
				(void *)&(data_read), sizeof(u32));
		} else {
			pr_info("%s Error! Index %d is larger than size %d\n",
					MODULE_NAME, index, size);
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
 *****************************************************************************/
static int __init devinfo_init(void)
{
	int ret = 0;
	struct device *device;

	devinfo_dev = MKDEV(MAJOR_DEV_NUM, 0);
	pr_debug("[%s]init\n", MODULE_NAME);
	ret = register_chrdev_region(devinfo_dev, 1, DEV_NAME);
	if (ret) {
		pr_info("[%s] register device failed, ret:%d\n",
				MODULE_NAME, ret);
		return ret;
	}
	/*create class*/
	devinfo_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(devinfo_class)) {
		ret = PTR_ERR(devinfo_class);
		pr_info("[%s] register class failed, ret:%d\n",
				MODULE_NAME, ret);
		unregister_chrdev_region(devinfo_dev, 1);
		return ret;
	}
	/* initialize the device structure and register the device  */
	cdev_init(&devinfo_cdev, &devinfo_fops);
	devinfo_cdev.owner = THIS_MODULE;

	ret = cdev_add(&devinfo_cdev, devinfo_dev, 1);
	if (ret < 0) {
		pr_info("[%s] could not allocate chrdev for the device, ret:%d\n",
				MODULE_NAME, ret);
		class_destroy(devinfo_class);
		unregister_chrdev_region(devinfo_dev, 1);
		return ret;
	}
	/*create device*/
	device = device_create(devinfo_class, NULL, devinfo_dev, NULL,
			"devmap");
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_info("[%s]device create fail\n", MODULE_NAME);
		cdev_del(&devinfo_cdev);
		class_destroy(devinfo_class);
		unregister_chrdev_region(devinfo_dev, 1);
		return ret;
	}

	devinfo_segment_root = debugfs_create_dir("devinfo", NULL);
	if (!devinfo_segment_root)
		return -ENOMEM;

	if (!debugfs_create_file("segcode", 0444, devinfo_segment_root, NULL,
				&devinfo_segment_fops))
		return -ENOMEM;

	return 0;
}

#ifdef CONFIG_OF
static void devinfo_parse_dt(void)
{
	struct devinfo_tag *tags;
	u32 size = 0;
	u32 hrid_magic_and_size = 0;
	u32 hrid_magic = 0;
	u32 hrid_tmp_size = 0;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node) {
		chosen_node = of_find_node_by_path("/chosen@0");
		if (!chosen_node) {
			pr_info("chosen node is not found!!\n");
			return;
		}
	}

	tags = (struct devinfo_tag *) of_get_property(chosen_node,
			"atag,devinfo",	NULL);
	if (tags) {
		size = tags->data_size;

		g_devinfo_data = kmalloc(sizeof(struct devinfo_tag) +
				(size * sizeof(u32)), GFP_KERNEL);
		g_devinfo_size = size;

		WARN_ON(size > 300); /* for size integer too big protection */

		memcpy(g_devinfo_data, tags->data,
				(size * sizeof(u32)));

		if (size >= HRID_SIZE_INDEX) {
			hrid_magic_and_size = g_devinfo_data[HRID_SIZE_INDEX];
			hrid_magic = (hrid_magic_and_size & 0xFFFF0000);
			hrid_tmp_size = (hrid_magic_and_size & 0x0000FFFF);
			if (hrid_magic == HRID_SIZE_MAGIC_NUM) {
				if (hrid_tmp_size > HRID_MAX_ALLOWED_SIZE)
					g_hrid_size = HRID_MAX_ALLOWED_SIZE;
				else if (hrid_tmp_size < HRID_MIN_ALLOWED_SIZE)
					g_hrid_size = HRID_MIN_ALLOWED_SIZE;
				else
					g_hrid_size = hrid_tmp_size;
			} else
				g_hrid_size = HRID_DEFAULT_SIZE;
		} else
			g_hrid_size = HRID_DEFAULT_SIZE;

		pr_info("tag_devinfo_data size:%d, HRID size:%d\n",
				size, g_hrid_size);

		sprintf(devinfo_segment_buff, "segment code=0x%x\n",
				g_devinfo_data[DEVINFO_SEGCODE_INDEX]);

		pr_info("[devinfo][SegCode] Segment Code=0x%x\n",
				g_devinfo_data[DEVINFO_SEGCODE_INDEX]);

	} else {
		sprintf(devinfo_segment_buff,
				"segment code=[Fail in parsing DT]\n");

		pr_info("'atag,devinfo' is not found\n");
	}

}

static void init_devinfo_exclusive(void)
{
	if (atomic_read(&g_devinfo_init_status) == DEVINFO_INITIALIZED) {
		atomic_inc(&g_devinfo_init_errcnt);
		pr_info("%s Already init done earlier. Extra times:%d.\n",
			MODULE_NAME, atomic_read(&g_devinfo_init_errcnt));
		return;
	}

	if (atomic_read(&g_devinfo_init_status) == DEVINFO_UNINIT)
		atomic_set(&g_devinfo_init_status, DEVINFO_INITIALIZED);
	else
		return;

	devinfo_parse_dt();
}

static int __init devinfo_of_init(void)
{
	init_devinfo_exclusive();
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
 *****************************************************************************/
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


