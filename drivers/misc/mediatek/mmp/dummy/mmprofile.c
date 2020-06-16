// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/io.h>

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/uaccess.h>

#include <mmprofile_internal.h>
/* #pragma GCC optimize ("O0") */
#define MMP_DEVNAME "mmp"

void mmprofile_start(int start)
{
}

void mmprofile_enable(int enable)
{
}

/* Exposed APIs begin */
mmp_event mmprofile_register_event(mmp_event parent, const char *name)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_register_event);

mmp_event mmprofile_find_event(mmp_event parent, const char *name)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_find_event);

void mmprofile_enable_event(mmp_event event, long enable)
{
}
EXPORT_SYMBOL(mmprofile_enable_event);

void mmprofile_enable_event_recursive(mmp_event event, long enable)
{
}
EXPORT_SYMBOL(mmprofile_enable_event_recursive);

long mmprofile_query_enable(mmp_event event)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_query_enable);

void mmprofile_log_ex(mmp_event event, enum mmp_log_type type,
	unsigned long data1, unsigned long data2)
{
}
EXPORT_SYMBOL(mmprofile_log_ex);

void mmprofile_log(mmp_event event, enum mmp_log_type type)
{
}
EXPORT_SYMBOL(mmprofile_log);

long mmprofile_log_meta(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_log_meta);

long mmprofile_log_meta_structure(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_structure_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_log_meta_structure);

long mmprofile_log_meta_string_ex(mmp_event event, enum mmp_log_type type,
	unsigned long data1, unsigned long data2, const char *str)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_log_meta_string_ex);

long mmprofile_log_meta_string(mmp_event event, enum mmp_log_type type,
	const char *str)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_log_meta_string);

long mmprofile_log_meta_bitmap(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_bitmap_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_log_meta_bitmap);

long mmprofile_log_meta_yuv_bitmap(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_bitmap_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(mmprofile_log_meta_yuv_bitmap);

/* Exposed APIs end */

/* Driver specific begin */
#ifdef MMP_USE
static dev_t mmprofile_devno;
static struct cdev *mmprofile_cdev;
static struct class *mmprofile_class;
#endif
static int mmprofile_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmprofile_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mmprofile_read(struct file *file, char __user *data,
	size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t mmprofile_write(struct file *file, const char __user *data,
	size_t len,
			       loff_t *ppos)
{
	return 0;
}

static long mmprofile_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return 0;
}

static int mmprofile_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EINVAL;
}

const struct file_operations mmprofile_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmprofile_ioctl,
	.open = mmprofile_open,
	.release = mmprofile_release,
	.read = mmprofile_read,
	.write = mmprofile_write,
	.mmap = mmprofile_mmap,
};

static int __init mmprofile_init(void)
{
	return 0;
}

static void __exit mmprofile_exit(void)
{

}

/* Driver specific end */

module_init(mmprofile_init);
module_exit(mmprofile_exit);
MODULE_AUTHOR("Tianshu Qiu <tianshu.qiu@mediatek.com>");
MODULE_DESCRIPTION("MMProfile Driver");
MODULE_LICENSE("GPL");
