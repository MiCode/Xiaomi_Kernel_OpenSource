/* drivers/misc/last_prink.c
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Ling Zhu <zhuling@xiaomi.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

extern char *backup_log_buf;

#define BUF_SIZE (1 << CONFIG_LOG_BUF_SHIFT)

static int last_printk_read(struct file *file, char __user * buf,
		size_t len, loff_t * offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= BUF_SIZE)
		return 0;

	count = min(len, (size_t) (BUF_SIZE - pos));
	if (copy_to_user(buf, backup_log_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_printk_file_ops = {
	.owner = THIS_MODULE,
	.read = last_printk_read,
};

int __init last_printk_init(void)
{
	struct proc_dir_entry *proc_entry;

	if (backup_log_buf) {
		proc_entry =
		create_proc_entry("last_kmsg", S_IFREG | S_IRUGO, NULL);
		proc_entry->proc_fops = &last_printk_file_ops;
		proc_entry->size = BUF_SIZE;
		proc_entry->data = (void *)1;
	}

	return 0;
}

module_init(last_printk_init);
