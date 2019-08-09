/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*
 * @file    mtk_udi.c
 * @brief   Driver for UDI interface
 *
 */
/* system includes */
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#else
#include <common.h> /* for printf */
#endif

/* local includes */
#include <sync_write.h>
#include "mtk_udi_internal.h"

/*-----------------------------------------*/
/* Reused code start                       */
/*-----------------------------------------*/

#ifdef __KERNEL__
#define udi_read(addr)			readl(addr)
#define udi_write(addr, val)	mt_reg_sync_writel((val), ((void *)addr))
#endif

/*
 * LOG
 */
#define	UDI_TAG	  "[mt_udi] "
#ifdef __KERNEL__
#ifdef USING_XLOG
#include <linux/xlog.h>
#define udi_info(fmt, args...)	\
	pr_info(ANDROID_LOG_INFO, UDI_TAG, fmt, ##args)
#else
#define udi_info(fmt, args...)	\
	pr_info(UDI_TAG	fmt, ##args)
#endif
#else
#define udi_info(fmt, args...)	\
	printf(UDI_TAG fmt, ##args)
#endif

#ifdef __KERNEL__
/* Device infrastructure */
static int udi_remove(struct platform_device *pdev)
{
	return 0;
}

static int udi_probe(struct platform_device *pdev)
{
	udi_info("UDI Initial.\n");

	return 0;
}

static int udi_suspend(struct platform_device *pdev, pm_message_t state)
{
	udi_info("UDI suspend\n");
	return 0;
}

static int udi_resume(struct platform_device *pdev)
{
	udi_info("UDI resume\n");
	return 0;
}

struct platform_device udi_pdev = {
	.name   = "mt_udi",
	.id     = -1,
};

static struct platform_driver udi_pdrv = {
	.remove     = udi_remove,
	.shutdown   = NULL,
	.probe      = udi_probe,
	.suspend    = udi_suspend,
	.resume     = udi_resume,
	.driver     = {
		.name   = "mt_udi",
	},
};


#ifdef CONFIG_PROC_FS

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (buf == NULL)
		return NULL;
	if (count >= PAGE_SIZE)
		goto out0;
	if (copy_from_user(buf, buffer, count) != 0U)
		goto out0;

	buf[count] = '\0';
	return buf;

out0:
	free_page((unsigned long)buf);
	return NULL;
}

/* udi_pinmux_switch */
static int udi_pinmux_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU UDI pinmux reg[0x%p] = 0x%x.\n",
		UDIPIN_UDI_MUX1, udi_read(UDIPIN_UDI_MUX1));
	seq_printf(m, "CPU UDI pinmux reg[0x%p] = 0x%x.\n",
		UDIPIN_UDI_MUX2, udi_read(UDIPIN_UDI_MUX2));
	return 0;
}

static ssize_t udi_pinmux_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int pin_switch = 0U;

	if (buf == NULL)
		return -EINVAL;

	if (kstrtoint(buf, 10, &pin_switch) == 0) {
		if (pin_switch == 1U) {
			udi_write(UDIPIN_UDI_MUX1, UDIPIN_UDI_MUX1_VALUE);
			udi_write(UDIPIN_UDI_MUX2, UDIPIN_UDI_MUX2_VALUE);
		}
	} else
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_debug\n");

	free_page((unsigned long)buf);
	return (long)count;
}

#define PROC_FOPS_RW(name)	\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)	\
	{	\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));	\
	}	\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,	\
		.open           = name ## _proc_open,	\
		.read           = seq_read,	\
		.llseek         = seq_lseek,	\
		.release        = single_release,	\
		.write          = name ## _proc_write,	\
	}

#define PROC_FOPS_RO(name)	\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)	\
	{	\
		return single_open(file, name ## _proc_show, \
		PDE_DATA(inode));\
	}	\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,	\
		.open           = name ## _proc_open,	\
		.read           = seq_read,	\
		.llseek         = seq_lseek,	\
		.release        = single_release,	\
	}

#define PROC_ENTRY(name)    {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(udi_pinmux);		/* for udi pinmux switch */

static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(udi_pinmux),
	};

	dir = proc_mkdir("udi", NULL);

	if (dir == NULL) {
		udi_info("fail to create /proc/udi @ %s()\n", __func__);
		return -ENOMEM;
	}

	if (proc_create(entries[0].name, 0664, dir, entries[0].fops) == NULL)
		udi_info("create /proc/udi/udi_pinmux failed\n");


	return 0;
}

#endif /* CONFIG_PROC_FS */

/*
 * Module driver
 */
static int __init udi_init(void)
{
	int err = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, DEVICE_GPIO);
	if (node == NULL)
		udi_info("error: cannot find node UDI_NODE!\n");

	/* Setup IO addresses and printf */
	udipin_base = of_iomap(node, 0); /* UDI pinmux reg */
	udi_info("udipin_base = 0x%lx.\n", (unsigned long)udipin_base);
	if (udipin_base == NULL)
		udi_info("udi pinmux get some base NULL.\n");


	/* register platform device/driver */
	err = platform_device_register(&udi_pdev);

	if (err != 0) {
		udi_info("fail to register UDI device @ %s()\n", __func__);
		goto out2;
	}

	err = platform_driver_register(&udi_pdrv);
	if (err != 0) {
		udi_info("%s(), UDI driver callback register failed..\n",
			__func__);
		return err;
	}

#ifdef CONFIG_PROC_FS
	/* init proc */
	if (_create_procfs() != 0) {
		err = -ENOMEM;
		goto out2;
	}
#endif /* CONFIG_PROC_FS */

out2:
	return err;
}

static void __exit udi_exit(void)
{
	udi_info("UDI de-initialization\n");
	platform_driver_unregister(&udi_pdrv);
	platform_device_unregister(&udi_pdev);
}

module_init(udi_init);
module_exit(udi_exit);

MODULE_DESCRIPTION("MediaTek UDI Driver v0.1");
MODULE_LICENSE("GPL");
#endif /* __KERNEL__ */
