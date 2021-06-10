// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "generic_debugfs.h"

#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(1000)

static int data_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
	int i, ret;

	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}
	/* read transfer */
	if (!di->io_read)
		return -EPERM;
	ret = di->io_read(di->io_drvdata, d->reg, d->data_buffer, d->size);
	if (ret < 0)
		return ret;
	pdata = d->data_buffer;
	seq_puts(s, "0x");
	for (i = 0; i < d->size; i++)
		seq_printf(s, "%02x,", *(pdata + i));
	seq_puts(s, "\n");
	return 0;
}

static int data_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, data_debug_show, inode->i_private);
}

static ssize_t data_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct seq_file *seq = file->private_data;
	struct dbg_info *di = seq->private;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
	int val_cnt = 0, ret;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	if (copy_from_user(buf, user_buf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	/* buffer size check */
	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}
	/* data parsing */
	cur = buf;
	pdata = d->data_buffer;
	while ((token = strsep(&cur, ",\n")) != NULL) {
		if (!*token)
			break;
		if (val_cnt++ >= d->size)
			break;
		if (kstrtou8(token, 16, pdata++))
			return -EINVAL;
	}
	if (val_cnt != d->size)
		return -EINVAL;
	/* write transfer */
	if (!di->io_write)
		return -EPERM;
	ret = di->io_write(di->io_drvdata, d->reg, d->data_buffer, d->size);
	return (ret < 0) ? ret : cnt;
}

static const struct file_operations data_debug_fops = {
	.open = data_debug_open,
	.read = seq_read,
	.write = data_debug_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int type_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;

	seq_printf(s, "%s,%s\n", di->typestr, di->devname);
	return 0;
}

static int type_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_debug_show, inode->i_private);
}

static const struct file_operations type_debug_fops = {
	.open = type_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t lock_debug_read(struct file *file,
			       char __user *user_buf, size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	char buf[10];
	bool lock;

	mutex_lock(&d->io_lock);
	lock = d->access_lock;
	mutex_unlock(&d->io_lock);

	snprintf(buf, sizeof(buf), "%d\n", lock);
	return simple_read_from_buffer(user_buf, cnt, loff, buf, strlen(buf));
}

static ssize_t lock_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	u32 lock;
	int ret;

	ret = kstrtou32_from_user(user_buf, cnt, 0, &lock);
	if (ret < 0)
		return ret;
	mutex_lock(&d->io_lock);
	if (!!lock == d->access_lock)
		ret = -EFAULT;
	d->access_lock = !!lock;
	mutex_unlock(&d->io_lock);
	return (ret < 0) ? ret : cnt;
}

static const struct file_operations lock_debug_fops = {
	.open = simple_open,
	.read = lock_debug_read,
	.write = lock_debug_write,
};

int generic_debugfs_init(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	/* valid check */
	if (!di->dirname || !di->devname || !di->typestr)
		return -EINVAL;
	d->data_buffer_size = PREALLOC_RBUFFER_SIZE;
	/* for MTK engineer setting */
	d->size = 1;
	d->data_buffer = kzalloc(PREALLOC_RBUFFER_SIZE, GFP_KERNEL);
	if (!d->data_buffer)
		return -ENOMEM;
	/* create debugfs */
	d->rt_root = debugfs_lookup("ext_dev_io", NULL);
	if (!d->rt_root) {
		d->rt_root = debugfs_create_dir("ext_dev_io", NULL);
		if (!d->rt_root)
			return -ENODEV;
		d->rt_dir_create = true;
	}
	mutex_init(&d->io_lock);
	d->ic_root = debugfs_create_dir(di->dirname, d->rt_root);
	if (!d->ic_root)
		goto err_cleanup_rt;
	debugfs_create_u16("reg", 0644, d->ic_root, &d->reg);
	debugfs_create_u16("size", 0644, d->ic_root, &d->size);
	if (!debugfs_create_file("data", 0644,
				 d->ic_root, di, &data_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("type", 0444,
				 d->ic_root, di, &type_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("lock", 0644,
				 d->ic_root, di, &lock_debug_fops))
		goto err_cleanup_ic;
	return 0;
err_cleanup_ic:
	debugfs_remove_recursive(d->ic_root);
err_cleanup_rt:
	mutex_destroy(&d->io_lock);
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(generic_debugfs_init);

void generic_debugfs_exit(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	debugfs_remove_recursive(d->ic_root);
	mutex_destroy(&d->io_lock);
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
}
EXPORT_SYMBOL_GPL(generic_debugfs_exit);

static int __init generic_debugfs_init(void)
{
	return 0;
}

static void __exit generic_debugfs_exit(void)
{
}

subsys_initcall(generic_debugfs_init);
module_exit(generic_debugfs_exit);

MODULE_DESCRIPTION("Generic Debugfs");
MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_LICENSE("GPL");
