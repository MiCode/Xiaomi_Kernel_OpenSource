/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <mach/debug_mm.h>
#include <mach/msm_smd.h>

#include "adsp.h"

#define MAX_LEN 64
#ifdef CONFIG_DEBUG_FS
static struct dentry *adsp_dentry;
#endif
static char l_buf[MAX_LEN];
static unsigned int crash_enable;

static ssize_t q5_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	MM_DBG("q5 debugfs opened\n");
	return 0;
}

static ssize_t q5_debug_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int len;

	if (count < 0)
		return 0;
	len = count > (MAX_LEN - 1) ? (MAX_LEN - 1) : count;
	if (copy_from_user(l_buf, buf, len)) {
		MM_INFO("Unable to copy data from user space\n");
		return -EFAULT;
	}
	l_buf[len] = 0;
	if (l_buf[len - 1] == '\n') {
		l_buf[len - 1] = 0;
		len--;
	}
	if (!strncmp(l_buf, "boom", MAX_LEN)) {
		q5audio_dsp_not_responding();
	} else if (!strncmp(l_buf, "enable", MAX_LEN)) {
		crash_enable = 1;
		MM_INFO("Crash enabled : %d\n", crash_enable);
	} else if (!strncmp(l_buf, "disable", MAX_LEN)) {
		crash_enable = 0;
		MM_INFO("Crash disabled : %d\n", crash_enable);
	} else
		MM_INFO("Unknown Command\n");

	return count;
}

static const struct file_operations q5_debug_fops = {
	.write = q5_debug_write,
	.open = q5_debug_open,
};

static int __init q5_debug_init(void)
{
#ifdef CONFIG_DEBUG_FS
	adsp_dentry = debugfs_create_file("q5_debug", S_IFREG | S_IRUGO,
				NULL, (void *) NULL, &q5_debug_fops);
#endif /* CONFIG_DEBUG_FS */
	return 0;
}
device_initcall(q5_debug_init);

