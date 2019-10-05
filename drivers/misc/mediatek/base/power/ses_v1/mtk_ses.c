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
 * @file	mkt_ses.c
 * @brief   Driver for SupplEyeScan
 *
 */

#define __MTK_SES_C__

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
	#include <mt-plat/mtk_devinfo.h>
	#include "mtk_ses.h"
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
 * Marco definition
 ************************************************/


/************************************************
 * bit operation
 ************************************************/
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))
#define GET_BITS_VAL(_bits_, _val_)   \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

/************************************************
 * LOG
 ************************************************/
#define SES_TAG	 "[CPU_SES] "
#define ses_debug(fmt, args...)		pr_info(SES_TAG	fmt, ##args)

/************************************************
 * REG ACCESS
 ************************************************/
#ifdef __KERNEL__
	#define ses_read(addr)	__raw_readl((void __iomem *)(addr))
	#define ses_read_field(addr, range)	\
		((ses_read(addr) & BITMASK(range)) >> LSB(range))
	#define ses_write(addr, val)	mt_reg_sync_writel(val, addr)
#endif
/**
 * Write a field of a register.
 * @addr:	Address of the register
 * @range:	The field bit range in the form of MSB:LSB
 * @val:	The value to be written to the field
 */
#define ses_write_field(addr, range, val)	\
	ses_write(addr, (ses_read(addr) & ~BITMASK(range)) | BITS(range, val))

/************************************************
 * static Variable
 ************************************************/
static unsigned int sesNum = 9;
#ifdef CONFIG_OF
static unsigned int state;
static unsigned int ses_ByteSel = 1;
static unsigned int ses0_reg3;
static unsigned int ses1_reg3;
static unsigned int ses2_reg3;
static unsigned int ses3_reg3;
static unsigned int ses4_reg3;
static unsigned int ses5_reg3;
static unsigned int ses6_reg3;
static unsigned int ses7_reg3;
static unsigned int ses8_reg3;
static unsigned int ses0_reg2;
static unsigned int ses1_reg2;
static unsigned int ses2_reg2;
static unsigned int ses3_reg2;
static unsigned int ses4_reg2;
static unsigned int ses5_reg2;
static unsigned int ses6_reg2;
static unsigned int ses7_reg2;
static unsigned int ses8_reg2;
//static unsigned int ses0_drphipct;
//static unsigned int ses1_drphipct;
//static unsigned int ses2_drphipct;
//static unsigned int ses3_drphipct;
//static unsigned int ses4_drphipct;
//static unsigned int ses5_drphipct;
//static unsigned int ses6_drphipct;
//static unsigned int ses7_drphipct;
//static unsigned int ses8_drphipct;
//static unsigned int ses0_drplopct;
//static unsigned int ses1_drplopct;
//static unsigned int ses2_drplopct;
//static unsigned int ses3_drplopct;
//static unsigned int ses4_drplopct;
//static unsigned int ses5_drplopct;
//static unsigned int ses6_drplopct;
//static unsigned int ses7_drplopct;
//static unsigned int ses8_drplopct;

#endif

/************************************************
 * Global function definition
 ************************************************/

//int mtk_ses_feature_enabled_check(void)
//{
//	unsigned int status = 0, i = 0;
//
//	for (i = 0; i < sesNum; i++) {
//		status |= (mt_secure_call_ses(
//				MTK_SIP_KERNEL_SES_READ,
//				SES_AO_REG_BASE + ((u64)0x200 * i),
//				0,
//				0,
//				0) &
//				0x01) <<
//				i;
//	}
//
//	return status;
//}

void mtk_ses_init(void)
{
	mt_secure_call_ses(MTK_SIP_KERNEL_SES_INIT,
		0, 0, 0, 0);
}

void mtk_ses_enable(unsigned int onOff,
		unsigned int ses_node)
{
	mt_secure_call_ses(MTK_SIP_KERNEL_SES_ENABLE,
		onOff, ses_node, 0, 0);
}

unsigned int mtk_ses_event_count(unsigned int ByteSel,
		unsigned int ses_node)
{
	return mt_secure_call_ses(MTK_SIP_KERNEL_SES_COUNT,
			ByteSel, ses_node, 0, 0);
}

void mtk_ses_hwgatepct(unsigned int HwGateSel,
		unsigned int value,
		unsigned int ses_node)
{
	mt_secure_call_ses(MTK_SIP_KERNEL_SES_HWGATEPCT,
		HwGateSel, value, ses_node, 0);
}

void mtk_ses_stepstart(unsigned int HwGateSel,
		unsigned int ses_node)
{
	mt_secure_call_ses(MTK_SIP_KERNEL_SES_STEPSTART,
		HwGateSel, ses_node, 0, 0);
}

void mtk_ses_steptime(unsigned int value,
		unsigned int ses_node)
{
	mt_secure_call_ses(MTK_SIP_KERNEL_SES_STEPTIME,
		value, ses_node, 0, 0);
}
void mtk_ses_dly_filt(unsigned int value,
		unsigned int ses_node)
{
	mt_secure_call_ses(MTK_SIP_KERNEL_SES_DLYFILT,
		value, ses_node, 0, 0);
}


/************************************************
 * set SES status by procfs interface
 ************************************************/
static int ses_enable_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0, value, ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mt_secure_call_ses(
			MTK_SIP_KERNEL_SES_STATUS,
			SESV6_REG2 + (0x200 * ses_node),
			0,
			0,
			0);
		status = status | ((value & 0x01) << ses_node);
	}
	value = mt_secure_call_ses(
		MTK_SIP_KERNEL_SES_STATUS,
		SESV6_DSU_REG2,
		0,
		0,
		0);
	status = status | ((value & 0x01) << ses_node);

	seq_printf(m, "%u\n", status);

	return 0;
}

static ssize_t ses_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, ses_node;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		ses_debug("bad argument!! Should be \"0\" ~ \"511\"\n");
		goto out;
	}

	for (ses_node = 0; ses_node < sesNum; ses_node++)
		mtk_ses_enable((enable >> ses_node) & 0x01, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_count_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0;
	unsigned int ses_node = 0;

	for (ses_node = 0; ses_node < sesNum; ses_node++) {
		status = mtk_ses_event_count(ses_ByteSel,
		ses_node);

		seq_printf(m, "ses_%u ByteSel_%u count = %u\n",
			ses_node, ses_ByteSel, status);
	}
	return 0;
}

static ssize_t ses_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &ses_ByteSel)) {
		ses_debug("bad argument!! Should input 0~4.\n");
		goto out;
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_hwgatepct_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;
	unsigned int ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mt_secure_call_ses(
			MTK_SIP_KERNEL_SES_STATUS,
			SESV6_REG3 + (0x200 * ses_node),
			0,
			0,
			0);
		seq_printf(m, "ses_%u, hwgatepct[0,1,2,3]= %x %x %x %x\n",
			ses_node,
			GET_BITS_VAL(11:9, value),
			GET_BITS_VAL(14:12, value),
			GET_BITS_VAL(17:15, value),
			GET_BITS_VAL(20:18, value));
	}

	value = mt_secure_call_ses(
		MTK_SIP_KERNEL_SES_STATUS,
		SESV6_DSU_REG3,
		0,
		0,
		0);
	seq_printf(m, "ses_%u, hwgatepct[0,1,2,3]= %x %x %x %x\n",
		ses_node,
		GET_BITS_VAL(11:9, value),
		GET_BITS_VAL(14:12, value),
		GET_BITS_VAL(17:15, value),
		GET_BITS_VAL(20:18, value));

	return 0;
}

static ssize_t ses_hwgatepct_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int HwGateSel = 0, value = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u", &HwGateSel, &value, &ses_node) != 3) {
		ses_debug("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	mtk_ses_hwgatepct(HwGateSel, value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_stepstart_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;
	unsigned int ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mt_secure_call_ses(
			MTK_SIP_KERNEL_SES_STATUS,
			SESV6_REG3 + (0x200 * ses_node),
			0,
			0,
			0);
		seq_printf(m, "ses_%u, stepstart from hwgatepct[%x]\n",
			ses_node,
			GET_BITS_VAL(8:7, value));

	}

	value = mt_secure_call_ses(
		MTK_SIP_KERNEL_SES_STATUS,
		SESV6_DSU_REG3,
		0,
		0,
		0);
	seq_printf(m, "ses_%u, stepstart from hwgatepct[%x]\n",
		ses_node,
		GET_BITS_VAL(8:7, value));

	return 0;
}

static ssize_t ses_stepstart_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int HwGateSel = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &HwGateSel, &ses_node) != 2) {
		ses_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ses_stepstart(HwGateSel, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}


static int ses_steptime_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mt_secure_call_ses(
			MTK_SIP_KERNEL_SES_STATUS,
			SESV6_REG3 + (0x200 * ses_node),
			0,
			0,
			0);
		seq_printf(m, "ses_%u, steptime = %x\n",
			ses_node,
			GET_BITS_VAL(6:0, value));

	}

	value = mt_secure_call_ses(
		MTK_SIP_KERNEL_SES_STATUS,
		SESV6_DSU_REG3,
		0,
		0,
		0);
	seq_printf(m, "ses_%u, steptime = %x\n",
		ses_node,
		GET_BITS_VAL(6:0, value));

	return 0;
}

static ssize_t ses_steptime_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ses_node) != 2) {
		ses_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ses_steptime(value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}


static int ses_dly_filt_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mt_secure_call_ses(
			MTK_SIP_KERNEL_SES_STATUS,
			SESV6_REG2 + (0x200 * ses_node),
			0,
			0,
			0);
		seq_printf(m, "ses_%u, dly= %x, filt[hi:lo]= %x,%x\n",
			ses_node,
			GET_BITS_VAL(6:2, value),
			GET_BITS_VAL(16:12, value),
			GET_BITS_VAL(11:7, value));
	}

	value = mt_secure_call_ses(
		MTK_SIP_KERNEL_SES_STATUS,
		SESV6_DSU_REG2,
		0,
		0,
		0);
	seq_printf(m, "ses_%u, dly= %x, filt[hi:lo]= %x,%x\n",
		ses_node,
		GET_BITS_VAL(6:2, value),
		GET_BITS_VAL(16:12, value),
		GET_BITS_VAL(11:7, value));

	return 0;
}

static ssize_t ses_dly_filt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ses_node) != 2) {
		ses_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ses_dly_filt(value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}



static int ses_status_dump_proc_show(struct seq_file *m, void *v)
{
	unsigned int i, value[sesNum][5], ses_node = 0;

	for (ses_node = 0; ses_node < sesNum; ses_node++) {
		for (i = 0; i < 5; i++)
			value[ses_node][i] = mt_secure_call_ses(
				MTK_SIP_KERNEL_SES_STATUS,
				SESV6_REG0 + (0x200 * ses_node) + (i * 4),
				0,
				0,
				0);
		seq_printf(m, "CPU(%d), ses_reg :", ses_node);
		for (i = 0; i < 5; i++)
			seq_printf(m, "\t0x%llx = 0x%x",
				SESV6_REG0 + (0x200 * ses_node) + (i * 4),
				value[ses_node][i]);
		seq_printf(m, "    .%u\n", i);
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

PROC_FOPS_RW(ses_enable);
PROC_FOPS_RW(ses_count);
PROC_FOPS_RW(ses_hwgatepct);
PROC_FOPS_RW(ses_stepstart);
PROC_FOPS_RW(ses_steptime);
PROC_FOPS_RW(ses_dly_filt);
PROC_FOPS_RO(ses_status_dump);

static int create_procfs(void)
{
	struct proc_dir_entry *ses_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry ses_entries[] = {
		PROC_ENTRY(ses_enable),
		PROC_ENTRY(ses_count),
		PROC_ENTRY(ses_hwgatepct),
		PROC_ENTRY(ses_stepstart),
		PROC_ENTRY(ses_steptime),
		PROC_ENTRY(ses_dly_filt),
		PROC_ENTRY(ses_status_dump),
	};

	ses_dir = proc_mkdir("ses", NULL);
	if (!ses_dir) {
		ses_debug("[%s]: mkdir /proc/ses failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(ses_entries); i++) {
		if (!proc_create(ses_entries[i].name,
			0664,
			ses_dir,
			ses_entries[i].fops)) {
			ses_debug("[%s]: create /proc/ses/%s failed\n",
				__func__,
				ses_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int ses_probe(struct platform_device *pdev)
{
	#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int ses_node = 0;

	node = pdev->dev.of_node;
	if (!node) {
		ses_debug("get ses device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "state", &state);
	if (!rc) {
		ses_debug("state from DTree; rc(%d) state(0x%x)\n",
			rc,
			state);

		for (ses_node = 0; ses_node < sesNum; ses_node++)
			mtk_ses_enable((state >> ses_node) & 0x01, ses_node);
	}

	rc = of_property_read_u32(node, "ses0_reg3", &ses0_reg3);
	if (!rc) {
		ses_debug("ses0_reg3 from DTree; rc(%d) ses0_reg3(0x%x)\n",
			rc,
			ses0_reg3);

		if (ses0_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses0_reg3), 0);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses0_reg3), 0);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses0_reg3), 0);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses0_reg3), 0);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses0_reg3), 0);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses0_reg3), 0);
		}
	}

	rc = of_property_read_u32(node, "ses1_reg3", &ses1_reg3);
	if (!rc) {
		ses_debug("ses1_reg3 from DTree; rc(%d) ses1_reg3(0x%x)\n",
			rc,
			ses1_reg3);

		if (ses1_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses1_reg3), 1);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses1_reg3), 1);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses1_reg3), 1);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses1_reg3), 1);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses1_reg3), 1);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses1_reg3), 1);
		}
	}

	rc = of_property_read_u32(node, "ses2_reg3", &ses2_reg3);
	if (!rc) {
		ses_debug("ses2_reg3 from DTree; rc(%d) ses2_reg3(0x%x)\n",
			rc,
			ses2_reg3);

		if (ses2_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses2_reg3), 2);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses2_reg3), 2);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses2_reg3), 2);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses2_reg3), 2);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses2_reg3), 2);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses2_reg3), 2);
		}
	}

	rc = of_property_read_u32(node, "ses3_reg3", &ses3_reg3);
	if (!rc) {
		ses_debug("ses3_reg3 from DTree; rc(%d) ses3_reg3(0x%x)\n",
			rc,
			ses3_reg3);

		if (ses3_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses3_reg3), 3);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses3_reg3), 3);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses3_reg3), 3);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses3_reg3), 3);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses3_reg3), 3);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses3_reg3), 3);
		}
	}

	rc = of_property_read_u32(node, "ses4_reg3", &ses4_reg3);
	if (!rc) {
		ses_debug("ses4_reg3 from DTree; rc(%d) ses4_reg3(0x%x)\n",
			rc,
			ses4_reg3);

		if (ses4_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses4_reg3), 4);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses4_reg3), 4);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses4_reg3), 4);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses4_reg3), 4);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses4_reg3), 4);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses4_reg3), 4);
		}
	}

	rc = of_property_read_u32(node, "ses5_reg3", &ses5_reg3);
	if (!rc) {
		ses_debug("ses5_reg3 from DTree; rc(%d) ses5_reg3(0x%x)\n",
			rc,
			ses5_reg3);

		if (ses5_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses5_reg3), 5);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses5_reg3), 5);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses5_reg3), 5);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses5_reg3), 5);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses5_reg3), 5);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses5_reg3), 5);
		}
	}

	rc = of_property_read_u32(node, "ses6_reg3", &ses6_reg3);
	if (!rc) {
		ses_debug("ses6_reg3 from DTree; rc(%d) ses6_reg3(0x%x)\n",
			rc,
			ses6_reg3);

		if (ses6_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses6_reg3), 6);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses6_reg3), 6);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses6_reg3), 6);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses6_reg3), 6);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses6_reg3), 6);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses6_reg3), 6);
		}
	}

	rc = of_property_read_u32(node, "ses7_reg3", &ses7_reg3);
	if (!rc) {
		ses_debug("ses7_reg3 from DTree; rc(%d) ses7_reg3(0x%x)\n",
			rc,
			ses7_reg3);

		if (ses7_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses7_reg3), 7);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses7_reg3), 7);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses7_reg3), 7);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses7_reg3), 7);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses7_reg3), 7);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses7_reg3), 7);
		}
	}

	rc = of_property_read_u32(node, "ses8_reg3", &ses8_reg3);
	if (!rc) {
		ses_debug("ses8_reg3 from DTree; rc(%d) ses8_reg3(0x%x)\n",
			rc,
			ses8_reg3);

		if (ses8_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses8_reg3), 8);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses8_reg3), 8);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses8_reg3), 8);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses8_reg3), 8);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses8_reg3), 8);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses8_reg3), 8);
		}
	}

	rc = of_property_read_u32(node, "ses0_reg2", &ses0_reg2);
	if (!rc) {
		ses_debug("ses0_reg2 from DTree; rc(%d) ses0_reg2(0x%x)\n",
			rc,
			ses0_reg2);

		if (ses0_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses0_reg2), 0);
	}

	rc = of_property_read_u32(node, "ses1_reg2", &ses1_reg2);
	if (!rc) {
		ses_debug("ses1_reg2 from DTree; rc(%d) ses1_reg2(0x%x)\n",
			rc,
			ses1_reg2);

		if (ses1_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses1_reg2), 1);
	}

	rc = of_property_read_u32(node, "ses2_reg2", &ses2_reg2);
	if (!rc) {
		ses_debug("ses2_reg2 from DTree; rc(%d) ses2_reg2(0x%x)\n",
			rc,
			ses2_reg2);

		if (ses2_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses2_reg2), 2);
	}

	rc = of_property_read_u32(node, "ses3_reg2", &ses3_reg2);
	if (!rc) {
		ses_debug("ses3_reg2 from DTree; rc(%d) ses3_reg2(0x%x)\n",
			rc,
			ses3_reg2);

		if (ses3_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses3_reg2), 3);
	}

	rc = of_property_read_u32(node, "ses4_reg2", &ses4_reg2);
	if (!rc) {
		ses_debug("ses4_reg2 from DTree; rc(%d) ses4_reg2(0x%x)\n",
			rc,
			ses4_reg2);

		if (ses4_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses4_reg2), 4);
	}


	rc = of_property_read_u32(node, "ses5_reg2", &ses5_reg2);
	if (!rc) {
		ses_debug("ses5_reg2 from DTree; rc(%d) ses5_reg2(0x%x)\n",
			rc,
			ses5_reg2);

		if (ses5_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses5_reg2), 5);
	}


	rc = of_property_read_u32(node, "ses6_reg2", &ses6_reg2);
	if (!rc) {
		ses_debug("ses6_reg2 from DTree; rc(%d) ses6_reg2(0x%x)\n",
			rc,
			ses6_reg2);

		if (ses6_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses6_reg2), 6);
	}


	rc = of_property_read_u32(node, "ses7_reg2", &ses7_reg2);
	if (!rc) {
		ses_debug("ses7_reg2 from DTree; rc(%d) ses7_reg2(0x%x)\n",
			rc,
			ses7_reg2);

		if (ses7_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses7_reg2), 7);
	}


	rc = of_property_read_u32(node, "ses8_reg2", &ses8_reg2);
	if (!rc) {
		ses_debug("ses8_reg2 from DTree; rc(%d) ses8_reg2(0x%x)\n",
			rc,
			ses8_reg2);

		if (ses8_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses8_reg2), 8);
	}



	#endif

	#ifdef CONFIG_MTK_RAM_CONSOLE
	//_mt_ses_aee_init();
	#endif
	ses_debug("ses probe ok!!\n");

	return 0;
}

static int ses_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ses_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_ses_of_match[] = {
	{ .compatible = "mediatek,ses", },
	{},
};
#endif

static struct platform_driver ses_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= ses_probe,
	.suspend	= ses_suspend,
	.resume		= ses_resume,
	.driver		= {
		.name   = "mt-ses",
#ifdef CONFIG_OF
	.of_match_table = mt_ses_of_match,
#endif
	},
};

static int __init ses_init(void)
{
	int err = 0;

#ifdef MTK_SES_ON
	mtk_ses_init();
#endif
	create_procfs();

	err = platform_driver_register(&ses_driver);
	if (err) {
		ses_debug("SES driver callback register failed..\n");
		return err;
	}
	return 0;
}

static void __exit ses_exit(void)
{
	ses_debug("ses de-initialization\n");
}

late_initcall(ses_init);

MODULE_DESCRIPTION("MediaTek SES Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_SES_C__
