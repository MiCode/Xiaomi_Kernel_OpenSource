/* arch/arm/mach-msm/qdsp6/dsp_dump.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/io.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <asm/atomic.h>

#include <mach/proc_comm.h>
#include <mach/debug_mm.h>

static wait_queue_head_t dsp_wait;
static int dsp_has_crashed;
static int dsp_wait_count;

static atomic_t dsp_crash_count = ATOMIC_INIT(0);

void q6audio_dsp_not_responding(void)
{

	if (atomic_add_return(1, &dsp_crash_count) != 1) {
		pr_err("q6audio_dsp_not_responding() - parking additional crasher...\n");
		for (;;)
			msleep(1000);
	}
	if (dsp_wait_count) {
		dsp_has_crashed = 1;
		wake_up(&dsp_wait);

		while (dsp_has_crashed != 2)
			wait_event(dsp_wait, dsp_has_crashed == 2);
	} else {
		pr_err("q6audio_dsp_not_responding() - no waiter?\n");
	}
	BUG();
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

	if (!strcmp(cmd, "wait-for-crash")) {
		while (!dsp_has_crashed) {
			int res;
			dsp_wait_count++;
			res = wait_event_interruptible(dsp_wait, dsp_has_crashed);
			if (res < 0) {
				dsp_wait_count--;
				return res;
			}
		}
#if defined(CONFIG_MACH_MAHIMAHI)
		/* assert DSP NMI */
		msm_proc_comm(PCOM_CUSTOMER_CMD1, 0, 0);
		msleep(250);
#endif
	} else if (!strcmp(cmd, "boom")) {
		q6audio_dsp_not_responding();
	} else if (!strcmp(cmd, "continue-crash")) {
		dsp_has_crashed = 2;
		wake_up(&dsp_wait);
	} else {
		pr_err("[%s:%s] unknown dsp_debug command: %s\n", __MM_FILE__,
				__func__, cmd);
	}

	return count;
}

#define DSP_RAM_BASE 0x2E800000
#define DSP_RAM_SIZE 0x01800000

static unsigned copy_ok_count;

static ssize_t dsp_read(struct file *file, char __user *buf,
			size_t count, loff_t *pos)
{
	size_t actual = 0;
	size_t mapsize = PAGE_SIZE;
	unsigned addr;
	void __iomem *ptr;

	if (*pos >= DSP_RAM_SIZE)
		return 0;

	if (*pos & (PAGE_SIZE - 1))
		return -EINVAL;

	addr = (*pos + DSP_RAM_BASE);

	/* don't blow up if we're unaligned */
	if (addr & (PAGE_SIZE - 1))
		mapsize *= 2;

	while (count >= PAGE_SIZE) {
		ptr = ioremap(addr, mapsize);
		if (!ptr) {
			pr_err("[%s:%s] map error @ %x\n", __MM_FILE__,
					__func__, addr);
			return -EFAULT;
		}
		if (copy_to_user(buf, ptr, PAGE_SIZE)) {
			iounmap(ptr);
			pr_err("[%s:%s] copy error @ %p\n", __MM_FILE__,
					__func__, buf);
			return -EFAULT;
		}
		copy_ok_count += PAGE_SIZE;
		iounmap(ptr);
		addr += PAGE_SIZE;
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

static const struct file_operations dsp_fops = {
	.owner		= THIS_MODULE,
	.open		= dsp_open,
	.read		= dsp_read,
	.write		= dsp_write,
	.release	= dsp_release,
};

static struct miscdevice dsp_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "dsp_debug",
	.fops	= &dsp_fops,
};


static int __init dsp_init(void)
{
	init_waitqueue_head(&dsp_wait);
	return misc_register(&dsp_misc);
}

device_initcall(dsp_init);
