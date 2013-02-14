/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/atomic.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <mach/debug_mm.h>
#include <mach/msm_iomap.h>

#include "dsp_debug.h"

static wait_queue_head_t dsp_wait;
static int dsp_has_crashed;
static int dsp_wait_count;

static atomic_t dsp_crash_count = ATOMIC_INIT(0);
static dsp_state_cb cb_ptr;

#define MAX_LEN 64
#define HDR_LEN 20
#define NUM_DSP_RAM_BANKS 3

static char l_buf[MAX_LEN];
#ifdef CONFIG_DEBUG_FS
static struct dentry *dsp_dentry;
#endif

void q5audio_dsp_not_responding(void)
{
	if (cb_ptr)
		cb_ptr(DSP_STATE_CRASHED);

	MM_DBG("entered q5audio_dsp_not_responding\n");
	if (atomic_add_return(1, &dsp_crash_count) != 1) {
		MM_ERR("q5audio_dsp_not_responding() \
			- parking additional crasher...\n");
		for (;;)
			msleep(1000);
	}
	if (dsp_wait_count) {
		dsp_has_crashed = 1;
		wake_up(&dsp_wait);

		while (dsp_has_crashed != 2)
			wait_event(dsp_wait, dsp_has_crashed == 2);
	} else {
		MM_ERR("q5audio_dsp_not_responding() - no waiter?\n");
	}

	if (cb_ptr)
		cb_ptr(DSP_STATE_CRASH_DUMP_DONE);
}

static int dsp_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t dsp_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *pos)
{
	char cmd[32];

	if (count >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buf, count))
		return -EFAULT;
	cmd[count] = 0;

	if ((count > 1) && (cmd[count-1] == '\n'))
		cmd[count-1] = 0;

	if (!strncmp(cmd, "wait-for-crash", sizeof("wait-for-crash"))) {
		while (!dsp_has_crashed) {
			int res;
			dsp_wait_count++;
			res = wait_event_interruptible(dsp_wait,
							dsp_has_crashed);
			if (res < 0) {
				dsp_wait_count--;
				return res;
			}
		}
	} else if (!strncmp(cmd, "boom", sizeof("boom"))) {
		q5audio_dsp_not_responding();
	} else if (!strncmp(cmd, "continue-crash", sizeof("continue-crash"))) {
		dsp_has_crashed = 2;
		wake_up(&dsp_wait);
	} else {
		MM_ERR("[%s:%s] unknown dsp_debug command: %s\n", __MM_FILE__,
				__func__, cmd);
	}

	return count;
}

static ssize_t dsp_read(struct file *file, char __user *buf,
			size_t count, loff_t *pos)
{
	size_t actual = 0;
	static void *dsp_addr;
	static unsigned copy_ok_count;

	MM_INFO("pos = %lld\n", *pos);
	if (*pos >= DSP_RAM_SIZE * NUM_DSP_RAM_BANKS)
		return 0;

	if (*pos == 0)
		dsp_addr = (*pos + RAMA_BASE);
	else if (*pos == DSP_RAM_SIZE)
		dsp_addr = RAMB_BASE;
	else if (*pos >= DSP_RAM_SIZE * 2)
		dsp_addr = RAMC_BASE;

	MM_INFO("dsp_addr = %p\n", dsp_addr);
	while (count >= PAGE_SIZE) {
		if (copy_to_user(buf, dsp_addr, PAGE_SIZE)) {
			MM_ERR("[%s:%s] copy error @ %p\n", __MM_FILE__,
					__func__, buf);
			return -EFAULT;
		}
		copy_ok_count += PAGE_SIZE;
		dsp_addr = (char *)dsp_addr + PAGE_SIZE;
		buf += PAGE_SIZE;
		actual += PAGE_SIZE;
		count -= PAGE_SIZE;
	}

	*pos += actual;
	return actual;
}

static int dsp_release(struct inode *inode, struct file *file)
{
	return 0;
}

int dsp_debug_register(dsp_state_cb ptr)
{
	if (ptr == NULL)
		return -EINVAL;

	cb_ptr = ptr;

	return 0;
}

static const struct file_operations dsp_fops = {
	.owner		= THIS_MODULE,
	.open		= dsp_open,
	.read		= dsp_read,
	.write		= dsp_write,
	.release	= dsp_release,
};

#ifdef CONFIG_DEBUG_FS
static struct miscdevice dsp_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "dsp_debug",
	.fops	= &dsp_fops,
};
#endif

static ssize_t dsp_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	MM_DBG("adsp debugfs opened\n");
	return 0;
}

static ssize_t dsp_debug_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	int len;

	if (count < 0)
		return 0;
	len = count > (MAX_LEN - 1) ? (MAX_LEN - 1) : count;
	if (copy_from_user(l_buf + HDR_LEN, buf, len)) {
		MM_ERR("Unable to copy data from user space\n");
		return -EFAULT;
	}
	l_buf[len + HDR_LEN] = 0;
	if (l_buf[len + HDR_LEN - 1] == '\n') {
		l_buf[len + HDR_LEN - 1] = 0;
		len--;
	}
	if (!strncmp(l_buf + HDR_LEN, "boom", 64)) {
		q5audio_dsp_not_responding();
	} else if (!strncmp(l_buf + HDR_LEN, "continue-crash",
				sizeof("continue-crash"))) {
		dsp_has_crashed = 2;
		wake_up(&dsp_wait);
	} else
		MM_ERR("Unknown command\n");

	return count;
}
static const struct file_operations dsp_debug_fops = {
	.write = dsp_debug_write,
	.open = dsp_debug_open,
};

static int __init dsp_init(void)
{
	init_waitqueue_head(&dsp_wait);
#ifdef CONFIG_DEBUG_FS
	dsp_dentry = debugfs_create_file("dsp_debug", S_IFREG | S_IRUGO,
			NULL, (void *) NULL, &dsp_debug_fops);

	return misc_register(&dsp_misc);
#else
	return 0;
#endif /* CONFIG_DEBUG_FS */
}

device_initcall(dsp_init);
