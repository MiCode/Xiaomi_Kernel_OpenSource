/*
 * Copyright (C) 2016 MediaTek Inc.
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

/**
 * @file	mkt_brisket.c
 * @brief   Driver for brisket
 *
 */

#define __MTK_BRISKET_C__

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef __KERNEL__
	#include <linux/topology.h>

	/* local includes (kernel-4.4)*/
	#include <mt-plat/mtk_chip.h>
	/* #include <mt-plat/mtk_gpio.h> */
	#include "mtk_brisket.h"
	#include <mt-plat/mtk_devinfo.h>
#endif

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/cpu_pm.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

/************************************************
 * Debug print
 ************************************************/

//#define BRISKET_DEBUG
#define BRISKET_TAG	 "[BRISKET]"

#ifdef BRISKET_DEBUG
#define brisket_debug(fmt, args...)	\
	pr_info(BRISKET_TAG"[%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define brisket_debug(fmt, args...)
#endif


/************************************************
 * Marco definition
 ************************************************/
#ifdef TIME_LOG
/* Get time stmp to known the time period */
static unsigned long long brisket_pTime_us, brisket_cTime_us, brisket_diff_us;
#ifdef __KERNEL__
#define TIME_TH_US 3000
#define BRISKET_IS_TOO_LONG()	\
	do {	\
		brisket_diff_us = brisket_cTime_us - brisket_pTime_us;	\
		if (brisket_diff_us > TIME_TH_US) {	\
			pr_debug(BRISKET_TAG \
			"caller_addr %p: %llu us\n",	\
			__builtin_return_address(0), brisket_diff_us);	\
		} else if (brisket_diff_us < 0) {	\
			pr_debug(BRISKET_TAG \
			"E: misuse caller_addr %p\n",	\
			__builtin_return_address(0));	\
		}	\
	} while (0)
#endif
#endif

/************************************************
 * static Variable
 ************************************************/
#if 0
static DEFINE_SPINLOCK(brisket_spinlock);
#endif

#ifdef CONFIG_OF
static unsigned int brisket4_doe_pllclken;
static unsigned int brisket5_doe_pllclken;
static unsigned int brisket6_doe_pllclken;
static unsigned int brisket7_doe_pllclken;
static unsigned int brisket4_doe_bren;
static unsigned int brisket5_doe_bren;
static unsigned int brisket6_doe_bren;
static unsigned int brisket7_doe_bren;
#endif

/************************************************
 * static function
 ************************************************/


#ifdef TIME_LOG
static long long brisket_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}
#endif

#if 0
static void mtk_brisket_lock(unsigned long *flags)
{
#ifdef __KERNEL__
	spin_lock_irqsave(&brisket_spinlock, *flags);
	#ifdef TIME_LOG
	brisket_pTime_us = brisket_get_current_time_us();
	#endif
#endif
}

static void mtk_brisket_unlock(unsigned long *flags)
{
#ifdef __KERNEL__
	#ifdef TIME_LOG
	brisket_cTime_us = brisket_get_current_time_us();
	BRISKET_IS_TOO_LONG();
	#endif
	spin_unlock_irqrestore(&brisket_spinlock, *flags);
#endif
}
#endif

void mtk_brisket_pllclken(unsigned int cpu,
		unsigned int brisket_pllclken)
{
	const unsigned int brisket_group = BRISKET_GROUP_CONTROL;
	const unsigned int bits = 1;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift =
		(brisket_group << 16) | (bits << 8) | shift;

	mt_secure_call_brisket(MTK_SIP_KERNEL_BRISKET_CONTROL,
		BRISKET_RW_WRITE,
		cpu,
		brisket_group_bits_shift,
		brisket_pllclken);
}

void mtk_brisket_bren(unsigned int cpu,
		unsigned int brisket_bren)
{
	const unsigned int brisket_group = BRISKET_GROUP_05;
	const unsigned int bits = 1;
	const unsigned int shift = 20;
	unsigned int brisket_group_bits_shift =
		(brisket_group << 16) | (bits << 8) | shift;

	mt_secure_call_brisket(MTK_SIP_KERNEL_BRISKET_CONTROL,
		BRISKET_RW_WRITE,
		cpu,
		brisket_group_bits_shift,
		brisket_bren);
}

static void mtk_brisket(unsigned int cpu, unsigned int brisket_group,
	unsigned int bits, unsigned int shift, unsigned int value)
{
	unsigned int brisket_group_bits_shift =
		(brisket_group << 16) | (bits << 8) | shift;

	mt_secure_call_brisket(MTK_SIP_KERNEL_BRISKET_CONTROL,
		BRISKET_RW_WRITE,
		cpu,
		brisket_group_bits_shift,
		value);
}

/************************************************
 * set BRISKET status by procfs interface
 ************************************************/
static ssize_t brisket_pllclken_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int cpu, brisket_pllclken;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u", &cpu, &brisket_pllclken) != 2) {
		brisket_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_brisket_pllclken((unsigned int)cpu, (unsigned int)brisket_pllclken);

out:
	free_page((unsigned long)buf);
	return count;
}


static int brisket_pllclken_proc_show(struct seq_file *m, void *v)
{
	int cpu, brisket_pllclken;
	const unsigned int brisket_group = BRISKET_GROUP_CONTROL;
	const unsigned int bits = 1;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {

		brisket_group_bits_shift =
			(brisket_group << 16) | (bits << 8) | shift;

		brisket_debug("cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
			cpu, brisket_group, bits, shift);

		brisket_pllclken =
			mt_secure_call_brisket(
			MTK_SIP_KERNEL_BRISKET_CONTROL,
			BRISKET_RW_READ,
			cpu,
			brisket_group_bits_shift,
			0);

		seq_printf(m, "CPU%d: BRISKET_CONTROL_Pllclken = %d\n",
			cpu,
			brisket_pllclken);
	}

	return 0;
}

static ssize_t brisket_bren_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int cpu, brisket_bren;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u", &cpu, &brisket_bren) != 2) {
		brisket_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_brisket_bren((unsigned int)cpu, (unsigned int)brisket_bren);

out:
	free_page((unsigned long)buf);
	return count;

}

static int brisket_bren_proc_show(struct seq_file *m, void *v)
{
	int cpu, brisket_bren;
	const unsigned int brisket_group = BRISKET_GROUP_05;
	const unsigned int bits = 1;
	const unsigned int shift = 20;
	unsigned int brisket_group_bits_shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {

		brisket_group_bits_shift =
			(brisket_group << 16) | (bits << 8) | shift;

		brisket_debug("cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
			cpu, brisket_group, bits, shift);

		brisket_bren =
			mt_secure_call_brisket(
			MTK_SIP_KERNEL_BRISKET_CONTROL,
			BRISKET_RW_READ,
			cpu,
			brisket_group_bits_shift,
			0);

		seq_printf(m, "CPU%d: BRISKET05_Bren = %d\n",
			cpu,
			brisket_bren);
	}
	return 0;
}

static ssize_t brisket_reg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int cpu, brisket_group, bits, shift, value;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %u %u %u",
		&cpu, &brisket_group, &bits, &shift, &value) != 5) {

		brisket_debug("bad argument!! Should input 5 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_brisket((unsigned int)cpu, (unsigned int)brisket_group,
	(unsigned int)bits, (unsigned int)shift, (unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}


static int brisket_reg_proc_show(struct seq_file *m, void *v)
{
	int cpu, brisket_group, reg_value;
	const unsigned int bits = 31;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {
		for (brisket_group = BRISKET_GROUP_CONTROL;
			brisket_group < NR_BRISKET_GROUP; brisket_group++) {

			brisket_group_bits_shift =
				(brisket_group << 16) | (bits << 8) | shift;

			brisket_debug("cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
				cpu, brisket_group, bits, shift);

			reg_value =
				mt_secure_call_brisket(
				MTK_SIP_KERNEL_BRISKET_CONTROL,
				BRISKET_RW_READ,
				cpu,
				brisket_group_bits_shift,
				0);

			seq_printf(m, "CPU%d: BRISKET0%d = 0x%08x\n",
				cpu,
				brisket_group,
				reg_value);
		}
	}
	return 0;
}


#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(brisket_pllclken);
PROC_FOPS_RW(brisket_bren);
PROC_FOPS_RW(brisket_reg);

static int create_procfs(void)
{
	struct proc_dir_entry *brisket_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry brisket_entries[] = {
		PROC_ENTRY(brisket_pllclken),
		PROC_ENTRY(brisket_bren),
		PROC_ENTRY(brisket_reg),
	};

	brisket_dir = proc_mkdir("brisket", NULL);
	if (!brisket_dir) {
		brisket_debug("[%s]: mkdir /proc/brisket failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(brisket_entries); i++) {
		if (!proc_create(brisket_entries[i].name,
			0664,
			brisket_dir,
			brisket_entries[i].fops)) {
			brisket_debug("[%s]: create /proc/brisket/%s failed\n",
				__func__,
				brisket_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int brisket_probe(struct platform_device *pdev)
{
	#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;

	node = pdev->dev.of_node;
	if (!node) {
		brisket_debug("get brisket device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node,
		"brisket4_doe_pllclken", &brisket4_doe_pllclken);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket4_doe_pllclken from DTree; rc(%d) brisket4_doe_pllclken(0x%x)\n",
			rc,
			brisket4_doe_pllclken);

		if (brisket4_doe_pllclken >= 0)
			mtk_brisket_pllclken(4, brisket4_doe_pllclken);
	}

	rc = of_property_read_u32(node,
		"brisket5_doe_pllclken", &brisket5_doe_pllclken);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket5_doe_pllclken from DTree; rc(%d) brisket5_doe_pllclken(0x%x)\n",
			rc,
			brisket5_doe_pllclken);

		if (brisket5_doe_pllclken >= 0)
			mtk_brisket_pllclken(5, brisket5_doe_pllclken);
	}

	rc = of_property_read_u32(node,
		"brisket6_doe_pllclken", &brisket6_doe_pllclken);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket6_doe_pllclken from DTree; rc(%d) brisket6_doe_pllclken(0x%x)\n",
			rc,
			brisket6_doe_pllclken);

		if (brisket6_doe_pllclken >= 0)
			mtk_brisket_pllclken(6, brisket6_doe_pllclken);
	}

	rc = of_property_read_u32(node,
		"brisket7_doe_pllclken", &brisket7_doe_pllclken);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket7_doe_pllclken from DTree; rc(%d) brisket7_doe_pllclken(0x%x)\n",
			rc,
			brisket7_doe_pllclken);

		if (brisket7_doe_pllclken >= 0)
			mtk_brisket_pllclken(7, brisket7_doe_pllclken);
	}

	rc = of_property_read_u32(node,
		"brisket4_doe_bren", &brisket4_doe_bren);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket4_doe_bren from DTree; rc(%d) brisket4_doe_bren(0x%x)\n",
			rc,
			brisket4_doe_bren);

		if (brisket4_doe_bren >= 0)
			mtk_brisket_bren(4, brisket4_doe_bren);
	}

	rc = of_property_read_u32(node,
		"brisket5_doe_bren", &brisket5_doe_bren);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket5_doe_bren from DTree; rc(%d) brisket5_doe_bren(0x%x)\n",
			rc,
			brisket5_doe_bren);

		if (brisket5_doe_bren >= 0)
			mtk_brisket_bren(5, brisket5_doe_bren);
	}

	rc = of_property_read_u32(node,
		"brisket6_doe_bren", &brisket6_doe_bren);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket6_doe_bren from DTree; rc(%d) brisket6_doe_bren(0x%x)\n",
			rc,
			brisket6_doe_bren);

		if (brisket6_doe_bren >= 0)
			mtk_brisket_bren(6, brisket6_doe_bren);
	}

	rc = of_property_read_u32(node,
		"brisket7_doe_bren", &brisket7_doe_bren);
	if (!rc) {
		brisket_debug("[xxxxbrisket] brisket7_doe_bren from DTree; rc(%d) brisket7_doe_bren(0x%x)\n",
			rc,
			brisket7_doe_bren);

		if (brisket7_doe_bren >= 0)
			mtk_brisket_bren(7, brisket7_doe_bren);
	}

	brisket_debug("brisket probe ok!!\n");
	#endif

	return 0;
}

static int brisket_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int brisket_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_brisket_of_match[] = {
	{ .compatible = "mediatek,brisket", },
	{},
};
#endif

static struct platform_driver brisket_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= brisket_probe,
	.suspend	= brisket_suspend,
	.resume		= brisket_resume,
	.driver		= {
		.name   = "mt-brisket",
#ifdef CONFIG_OF
	.of_match_table = mt_brisket_of_match,
#endif
	},
};

static int __init __brisket_init(void)
{
	int err = 0;

	create_procfs();

	err = platform_driver_register(&brisket_driver);
	if (err) {
		brisket_debug("BRISKET driver callback register failed..\n");
		return err;
	}
	return 0;
}

static void __exit __brisket_exit(void)
{
	brisket_debug("brisket de-initialization\n");
}


module_init(__brisket_init);
module_exit(__brisket_exit);

MODULE_DESCRIPTION("MediaTek BRISKET Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_BRISKET_C__
