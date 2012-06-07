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
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <mach/proc_comm.h>
#include <mach/debug_mm.h>
#include <mach/qdsp6v2/dsp_debug.h>

static wait_queue_head_t dsp_wait;
static int dsp_has_crashed;
static int dsp_wait_count;

static atomic_t dsp_crash_count = ATOMIC_INIT(0);
dsp_state_cb cb_ptr;

void q6audio_dsp_not_responding(void)
{
	if (cb_ptr)
		cb_ptr(DSP_STATE_CRASHED);
	if (atomic_add_return(1, &dsp_crash_count) != 1) {
		pr_err("q6audio_dsp_not_responding() \
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
		pr_err("q6audio_dsp_not_responding() - no waiter?\n");
	}
	if (cb_ptr)
		cb_ptr(DSP_STATE_CRASH_DUMP_DONE);
}

static int dsp_open(struct inode *inode, struct file *file)
{
	return 0;
}

#define DSP_NMI_ADDR 0x28800010

static ssize_t dsp_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *pos)
{
	char cmd[32];
	void __iomem *ptr;
	void *mem_buffer;

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
			res = wait_event_interruptible(dsp_wait,
							dsp_has_crashed);
			if (res < 0) {
				dsp_wait_count--;
				return res;
			}
		}
		/* assert DSP NMI */
		mem_buffer = ioremap(DSP_NMI_ADDR, 0x16);
		if (IS_ERR((void *)mem_buffer)) {
			pr_err("%s:map_buffer failed, error = %ld\n", __func__,
				   PTR_ERR((void *)mem_buffer));
			return -ENOMEM;
		}
		ptr = mem_buffer;
		if (!ptr) {
			pr_err("Unable to map DSP NMI\n");
			return -EFAULT;
		}
		writel(0x1, (void *)ptr);
		iounmap(mem_buffer);
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

static unsigned copy_ok_count;
static uint32_t dsp_ram_size;
static uint32_t dsp_ram_base;

static ssize_t dsp_read(struct file *file, char __user *buf,
			size_t count, loff_t *pos)
{
	size_t actual = 0;
	size_t mapsize = PAGE_SIZE;
	unsigned addr;
	void __iomem *ptr;
	void *mem_buffer;

	if ((dsp_ram_base == 0) || (dsp_ram_size == 0)) {
		pr_err("[%s:%s] Memory Invalid or not initialized, Base = 0x%x,"
			   " size = 0x%x\n", __MM_FILE__,
				__func__, dsp_ram_base, dsp_ram_size);
		return -EINVAL;
	}

	if (*pos >= dsp_ram_size)
		return 0;

	if (*pos & (PAGE_SIZE - 1))
		return -EINVAL;

	addr = (*pos + dsp_ram_base);

	/* don't blow up if we're unaligned */
	if (addr & (PAGE_SIZE - 1))
		mapsize *= 2;

	while (count >= PAGE_SIZE) {
		mem_buffer = ioremap(addr, mapsize);
		if (IS_ERR((void *)mem_buffer)) {
			pr_err("%s:map_buffer failed, error = %ld\n",
				__func__, PTR_ERR((void *)mem_buffer));
			return -ENOMEM;
		}
		ptr = mem_buffer;
		if (!ptr) {
			pr_err("[%s:%s] map error @ %x\n", __MM_FILE__,
					__func__, addr);
			return -EFAULT;
		}
		if (copy_to_user(buf, ptr, PAGE_SIZE)) {
			iounmap(mem_buffer);
			pr_err("[%s:%s] copy error @ %p\n", __MM_FILE__,
					__func__, buf);
			return -EFAULT;
		}
		copy_ok_count += PAGE_SIZE;
		iounmap(mem_buffer);
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

int dsp_debug_register(dsp_state_cb ptr)
{
	if (ptr == NULL)
		return -EINVAL;
	cb_ptr = ptr;

	return 0;
}

static int dspcrashd_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	int *pdata;

	pdata = pdev->dev.platform_data;
	res = platform_get_resource_byname(pdev, IORESOURCE_DMA,
						"msm_dspcrashd");
	if (!res) {
		pr_err("%s: failed to get resources for dspcrashd\n", __func__);
		return -ENODEV;
	}

	dsp_ram_base = res->start;
	dsp_ram_size = res->end - res->start;
	pr_info("%s: Platform driver values: Base = 0x%x, Size = 0x%x,"
		 "pdata = 0x%x\n", __func__,
		dsp_ram_base, dsp_ram_size, *pdata);
	return rc;
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

static struct platform_driver dspcrashd_driver = {
	.probe = dspcrashd_probe,
	.driver = { .name = "msm_dspcrashd"}
};

static int __init dsp_init(void)
{
	int rc = 0;
	init_waitqueue_head(&dsp_wait);
	rc = platform_driver_register(&dspcrashd_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register for dspcrashd failed\n",
			__func__);
	}
	return misc_register(&dsp_misc);
}

static int __exit dsp_exit(void)
{
	platform_driver_unregister(&dspcrashd_driver);
	return 0;
}

device_initcall(dsp_init);
