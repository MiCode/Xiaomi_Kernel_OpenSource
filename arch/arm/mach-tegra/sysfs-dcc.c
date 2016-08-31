/*
 * arch/arm/mach-tegra/sysfs-dcc.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>

#define DCC_TIMEOUT_US	    100000 	/* Delay time for DCC timeout (in uS) */
#define CP14_DSCR_WDTRFULL  0x20000000	/* Write Data Transfer Register Full */
#define SYSFS_DCC_DEBUG_PRINTS 0     	/* Set non-zero to enable debug prints */

#if SYSFS_DCC_DEBUG_PRINTS
#define DEBUG_DCC(x) printk x
#else
#define DEBUG_DCC(x)
#endif

static int DebuggerConnected = 0;  /* -1=not connected, 0=unknown, 1=connected */
static struct kobject *nvdcc_kobj;
static spinlock_t dcc_lock;
static struct list_head dcc_list;

static ssize_t sysfsdcc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t sysfsdcc_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);


static struct kobj_attribute nvdcc_attr =
		__ATTR(dcc0, 0222, sysfsdcc_show, sysfsdcc_store);

static int write_to_dcc(u32 c)
{
	volatile u32 dscr;

	/* Have we already determined that there is no debugger connected? */
	if (DebuggerConnected < 0)
	{
		return -ENXIO;
	}

	/* Read the DSCR. */
	asm volatile ("mrc p14, 0, %0, c0, c1, 0" : "=r" (dscr) : : "cc");

	/* If DSCR Bit 29 (wDTRFull) is set there is data in the write
	 * register. If it stays there for than the DCC_TIMEOUT_US
	 * period, ignore this write and disable further DCC accesses. */
	if (dscr & CP14_DSCR_WDTRFULL)
	{
		ktime_t end = ktime_add_ns(ktime_get(), DCC_TIMEOUT_US * 1000);
		ktime_t now;

		for (;;)
		{
			/* Re-read the DSCR. */
			asm volatile ("mrc p14, 0, %0, c0, c1, 0" : "=r" (dscr) : : "cc");

			/* Previous data still there? */
			if (dscr & CP14_DSCR_WDTRFULL)
			{
				now = ktime_get();

				if (ktime_to_ns(now) >= ktime_to_ns(end))
				{
					goto fail;
				}
			}
			else
			{
				if (DebuggerConnected == 0) {
					/* Debugger connected */
					spin_lock(&dcc_lock);
					DebuggerConnected = 1;
					spin_unlock(&dcc_lock);
				}
				break;
			}
		}
	}

	// Write the data into the DCC output register
	asm volatile ("mcr p14, 0, %0, c0, c5, 0" : : "r" (c) : "cc");
	return 0;

fail:
	/* No debugged connected -- disable DCC */
	spin_lock(&dcc_lock);
	DebuggerConnected = -1;
	spin_unlock(&dcc_lock);
	return -ENXIO;
}


struct tegra_dcc_req {
	struct list_head node;

	const char *pBuf;
	unsigned int size;
};

struct dcc_action {
	struct tegra_dcc_req req;
	struct work_struct work;
	struct list_head node;
};


static void dcc_writer(struct work_struct *work)
{
	struct dcc_action *action = container_of(work, struct dcc_action, work);
	const char *p;

	DEBUG_DCC(("+dcc_writer\n"));

	spin_lock(&dcc_lock);
	list_del(&action->req.node);
	spin_unlock(&dcc_lock);

	p = action->req.pBuf;
	if (p)
		while ((p < &(action->req.pBuf[action->req.size])) && (*p))
			if (write_to_dcc(*p++))
				break;

	kfree(action->req.pBuf);
	kfree(action);

	DEBUG_DCC(("-dcc_writer\n"));
}

static ssize_t sysfsdcc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	DEBUG_DCC(("!sysfsdcc_show\n"));
	return -EACCES;
}

static ssize_t sysfsdcc_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct dcc_action *action;
	char *pBuf;
	ssize_t ret = count;

	DEBUG_DCC(("+sysfsdcc_store: %p, %d\n", buf, count));

	if (!buf || !count) {
		ret = -EINVAL;
		goto fail;
	}

	pBuf = kmalloc(count+1, GFP_KERNEL);
	if (!pBuf) {
		pr_debug("%s: insufficient memory\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	action = kzalloc(sizeof(*action), GFP_KERNEL);
	if (!action) {
		kfree(pBuf);
		pr_debug("%s: insufficient memory\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	strncpy(pBuf, buf, count);
	pBuf[count] = '\0';
	action->req.pBuf = pBuf;
	action->req.size = count;

	INIT_WORK(&action->work, dcc_writer);

	spin_lock(&dcc_lock);
	list_add_tail(&action->req.node, &dcc_list);
	spin_unlock(&dcc_lock);

	/* DCC writes can only be performed from CPU0 */
	schedule_work_on(0, &action->work);

fail:
	DEBUG_DCC(("-sysfsdcc_store: %d\n", count));
	return ret;
}

static int __init sysfsdcc_init(void)
{
	spin_lock_init(&dcc_lock);
	INIT_LIST_HEAD(&dcc_list);

	DEBUG_DCC(("+sysfsdcc_init\n"));
	nvdcc_kobj = kobject_create_and_add("dcc", kernel_kobj);

	if (sysfs_create_file(nvdcc_kobj, &nvdcc_attr.attr))
	{
		DEBUG_DCC(("DCC: sysfs_create_file failed!\n"));
		return -ENXIO;
	}

	DEBUG_DCC(("-sysfsdcc_init\n"));
	return 0;
}

static void __exit sysfsdcc_exit(void)
{
	DEBUG_DCC(("+sysfsdcc_exit\n"));
	sysfs_remove_file(nvdcc_kobj, &nvdcc_attr.attr);
	kobject_del(nvdcc_kobj);
	DEBUG_DCC(("-sysfsdcc_exit\n"));
}

module_init(sysfsdcc_init);
module_exit(sysfsdcc_exit);
MODULE_LICENSE("GPL");
