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
	#include "mtk_devinfo.h"
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

#ifdef CONFIG_OF_RESERVED_MEM
	#include <of_reserved_mem.h>
#endif

/************************************************
 * Debug print
 ************************************************/

#define BRISKET_DEBUG
#define BRISKET_TAG	 "[BRISKET]"

#define brisket_err(fmt, args...)	\
	pr_info(BRISKET_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define brisket_msg(fmt, args...)	\
	pr_info(BRISKET_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef BRISKET_DEBUG
#define brisket_debug(fmt, args...)	\
	pr_debug(BRISKET_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define brisket_debug(fmt, args...)
#endif


/************************************************
 * Marco definition
 ************************************************/

/* efuse: PTPOD index */
#define DEVINFO_IDX_0 50


/************************************************
 * static Variable
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF

/* B-DOE use */
static unsigned int brisket_doe_pllclken;
static unsigned int brisket_doe_bren;
static unsigned int brisket_doe_brisket05;
static unsigned int brisket_doe_brisket06;
static unsigned int brisket_doe_brisket07;
static unsigned int brisket_doe_brisket08;
static unsigned int brisket_doe_brisket09;

#endif
#endif

/************************************************
 * static function
 ************************************************/

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

/* GAT log use */
phys_addr_t brisket_mem_base_phys;
phys_addr_t brisket_mem_size;
phys_addr_t brisket_mem_base_virt;

static int brisket_reserve_memory_dump(char *buf, unsigned int log_offset)
{
	int str_len = 0;
	int cpu, brisket_group, reg_value;
	const unsigned int bits = 31;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	if (!aee_log_buf) {
		brisket_msg("unable to get free page!\n");
		return -1;
	}
	brisket_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	brisket_debug("log_offset = %d\n", log_offset);
	if (log_offset == 0) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)brisket_mem_size - str_len,
		 "\n[Kernel Probe]\n");
	} else if (log_offset == 1) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)brisket_mem_size - str_len,
		 "\n[Kernel Suspend]\n");
	} else if (log_offset == 2) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)brisket_mem_size - str_len,
		 "\n[Kernel Resume]\n");
	} else {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)brisket_mem_size - str_len,
		 "\n[Kernel ??]\n");
	}

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {
		for (brisket_group = BRISKET_GROUP_CONTROL;
			brisket_group < NR_BRISKET_GROUP; brisket_group++) {

			brisket_group_bits_shift =
				(brisket_group << 16) | (bits << 8) | shift;

			reg_value =
				mt_secure_call_brisket(
				MTK_SIP_KERNEL_BRISKET_CONTROL,
				BRISKET_RW_READ,
				cpu,
				brisket_group_bits_shift,
				0);

			if (brisket_group == BRISKET_GROUP_CONTROL) {
				str_len +=
				 snprintf(aee_log_buf + str_len,
				 (unsigned long long)brisket_mem_size - str_len,
				 "CPU%d: BRISKET_CONTROL = 0x%08x\n",
				 cpu, reg_value);
			} else {
				str_len +=
				 snprintf(aee_log_buf + str_len,
				 (unsigned long long)brisket_mem_size - str_len,
				 "CPU%d: BRISKET0%d = 0x%08x\n",
				 cpu, brisket_group, reg_value);
			}
		}
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	brisket_debug("\n%s", aee_log_buf);
	brisket_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#define EEM_TEMPSPARE0		0x11278F20
#define brisket_read(addr)		__raw_readl((void __iomem *)(addr))
#define brisket_write(addr, val)	mt_reg_sync_writel(val, addr)

static void brisket_reserve_memory_init(unsigned int log_offset)
{
	char *buf;

	if (log_offset == 0) {
		brisket_mem_base_virt = 0;
		brisket_mem_size = 0x80000;
		brisket_mem_base_phys =
			brisket_read(ioremap(EEM_TEMPSPARE0, 0));

		if ((char *)brisket_mem_base_phys != NULL) {
			brisket_mem_base_virt =
				(phys_addr_t)(uintptr_t)ioremap_wc(
				brisket_mem_base_phys,
				brisket_mem_size);
		}
	}
	brisket_msg("[BRISKET] phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)brisket_mem_base_phys,
		(unsigned long long)brisket_mem_size,
		(unsigned long long)brisket_mem_base_virt);

	if ((char *)brisket_mem_base_virt != NULL) {
		buf = (char *)(uintptr_t)
			(brisket_mem_base_virt+log_offset*0x1000);

		/* dump brisket register status into reserved memory */
		brisket_reserve_memory_dump(buf, log_offset);
	} else
		brisket_err("brisket_mem_base_virt is null !\n");

}

#endif
#endif


void mtk_brisket_pllclken(unsigned int brisket_pllclken)
{
	unsigned int cpu;
	const unsigned int brisket_group = BRISKET_GROUP_CONTROL;
	const unsigned int bits = 1;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift =
		(brisket_group << 16) | (bits << 8) | shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {
		mt_secure_call_brisket(MTK_SIP_KERNEL_BRISKET_CONTROL,
			BRISKET_RW_WRITE,
			cpu,
			brisket_group_bits_shift,
			(brisket_pllclken >> cpu) & 1);
	}

}

void mtk_brisket_bren(unsigned int brisket_bren)
{
	unsigned int cpu;
	const unsigned int brisket_group = BRISKET_GROUP_05;
	const unsigned int bits = 1;
	const unsigned int shift = 20;
	unsigned int brisket_group_bits_shift =
		(brisket_group << 16) | (bits << 8) | shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {
		mt_secure_call_brisket(MTK_SIP_KERNEL_BRISKET_CONTROL,
			BRISKET_RW_WRITE,
			cpu,
			brisket_group_bits_shift,
			(brisket_bren >> cpu) & 1);
	}
}

void mtk_brisket_kpki(unsigned int cpu, unsigned int brisket_kp_online,
		unsigned int brisket_ki_online,
		unsigned int brisket_kp_offline,
		unsigned int brisket_ki_offline)
{
	unsigned int brisket_kpki;
	const unsigned int brisket_group = BRISKET_GROUP_05;
	const unsigned int bits = 20;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift =
		(brisket_group << 16) | (bits << 8) | shift;

	brisket_kpki =
		(brisket_kp_online << 16) |
		(brisket_ki_online << 10) |
		(brisket_kp_offline << 6) |
		(brisket_ki_offline << 0);

	mt_secure_call_brisket(MTK_SIP_KERNEL_BRISKET_CONTROL,
		BRISKET_RW_WRITE,
		cpu,
		brisket_group_bits_shift,
		brisket_kpki);
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
	int brisket_pllclken = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	/* coverity check */
	if (!buf)
		return -ENOMEM;

	if (sizeof(buf) >= PAGE_SIZE)
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (kstrtou32((const char *)buf, 0, &brisket_pllclken)) {
		brisket_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_brisket_pllclken((unsigned int)brisket_pllclken);

out:
	free_page((unsigned long)buf);
	return count;
}


static int brisket_pllclken_proc_show(struct seq_file *m, void *v)
{
	int cpu, brisket_pllclken, brisket_control;
	const unsigned int brisket_group = BRISKET_GROUP_CONTROL;
	const unsigned int bits = 31;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {

		brisket_group_bits_shift =
			(brisket_group << 16) | (bits << 8) | shift;

		brisket_msg("cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
			cpu, brisket_group, bits, shift);

		do {
			brisket_control =
				mt_secure_call_brisket(
				MTK_SIP_KERNEL_BRISKET_CONTROL,
				BRISKET_RW_READ,
				cpu,
				brisket_group_bits_shift,
				0);
		} while (brisket_control == 0xdeadbeef);

		brisket_msg(
			"[CPU%d] brisket_control=0x%08x\n",
			cpu,
			brisket_control);

		brisket_pllclken = GET_BITS_VAL(0:0, brisket_control);
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
	int brisket_bren = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	/* coverity check */
	if (!buf)
		return -ENOMEM;

	if (sizeof(buf) >= PAGE_SIZE)
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (kstrtou32((const char *)buf, 0, &brisket_bren)) {
		brisket_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_brisket_bren((unsigned int)brisket_bren);

out:
	free_page((unsigned long)buf);
	return count;

}

static int brisket_bren_proc_show(struct seq_file *m, void *v)
{
	int cpu, brisket_bren, brisket_05;
	const unsigned int brisket_group = BRISKET_GROUP_05;
	const unsigned int bits = 31;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {

		brisket_group_bits_shift =
			(brisket_group << 16) | (bits << 8) | shift;

		brisket_msg("cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
			cpu, brisket_group, bits, shift);

		do {
			brisket_05 =
				mt_secure_call_brisket(
				MTK_SIP_KERNEL_BRISKET_CONTROL,
				BRISKET_RW_READ,
				cpu,
				brisket_group_bits_shift,
				0);
		} while (brisket_05 == 0xdeadbeef);

		brisket_msg(
			"[CPU%d] brisket_05=0x%08x\n",
			cpu,
			brisket_05);

		brisket_bren = GET_BITS_VAL(20:20, brisket_05);

		seq_printf(m, "CPU%d: BRISKET05_Bren = %d\n",
			cpu,
			brisket_bren);
	}

	return 0;
}

static ssize_t brisket_kpki_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int cpu, brisket_kp_online, brisket_kp_offline,
		brisket_ki_online, brisket_ki_offline;

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
		&cpu, &brisket_kp_online, &brisket_ki_online,
		&brisket_kp_offline, &brisket_ki_offline) != 5) {

		brisket_err("bad argument!! Should input 5 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_brisket_kpki(cpu, (unsigned int)brisket_kp_online,
		(unsigned int)brisket_ki_online,
		(unsigned int)brisket_kp_offline,
		(unsigned int)brisket_ki_offline);

out:
	free_page((unsigned long)buf);
	return count;

}

static int brisket_kpki_proc_show(struct seq_file *m, void *v)
{
	int cpu, brisket_kp_online, brisket_kp_offline,
		brisket_ki_online, brisket_ki_offline, brisket_05;
	const unsigned int brisket_group = BRISKET_GROUP_05;
	const unsigned int bits = 31;
	const unsigned int shift = 0;
	unsigned int brisket_group_bits_shift;

	for (cpu = BRISKET_CPU_START_ID; cpu <= BRISKET_CPU_END_ID; cpu++) {

		brisket_group_bits_shift =
			(brisket_group << 16) | (bits << 8) | shift;

		brisket_msg("cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
			cpu, brisket_group, bits, shift);

		do {
			brisket_05 =
				mt_secure_call_brisket(
				MTK_SIP_KERNEL_BRISKET_CONTROL,
				BRISKET_RW_READ,
				cpu,
				brisket_group_bits_shift,
				0);
		} while (brisket_05 == 0xdeadbeef);

		brisket_msg(
			"[CPU%d] brisket_05=0x%08x\n",
			cpu,
			brisket_05);

		brisket_kp_online = GET_BITS_VAL(19:16, brisket_05);
		brisket_ki_online = GET_BITS_VAL(15:10, brisket_05);
		brisket_kp_offline = GET_BITS_VAL(9:6, brisket_05);
		brisket_ki_offline = GET_BITS_VAL(5:0, brisket_05);

		seq_printf(m, "CPU%d: (kp_online,ki_online,kp_offline,ki_offline) = (%d,%d,%d,%d)\n",
			cpu,
			brisket_kp_online,
			brisket_ki_online,
			brisket_kp_offline,
			brisket_ki_offline);
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

		brisket_err("bad argument!! Should input 5 arguments.\n");
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

			brisket_msg(
				"cpu(%d) brisket_group(%d) bits(%d) shift(%d)\n",
				cpu, brisket_group, bits, shift);

			reg_value =
				mt_secure_call_brisket(
				MTK_SIP_KERNEL_BRISKET_CONTROL,
				BRISKET_RW_READ,
				cpu,
				brisket_group_bits_shift,
				0);

			if (brisket_group == BRISKET_GROUP_CONTROL) {
				seq_printf(m, "CPU%d: BRISKET_CONTROL = 0x%08x\n",
					cpu,
					reg_value);
			} else {
				seq_printf(m, "CPU%d: BRISKET0%d = 0x%08x\n",
					cpu,
					brisket_group,
					reg_value);
			}
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
PROC_FOPS_RW(brisket_kpki);

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
		PROC_ENTRY(brisket_kpki),
	};

	brisket_dir = proc_mkdir("brisket", NULL);
	if (!brisket_dir) {
		brisket_err("[%s]: mkdir /proc/brisket failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(brisket_entries); i++) {
		if (!proc_create(brisket_entries[i].name,
			0664,
			brisket_dir,
			brisket_entries[i].fops)) {
			brisket_err("[%s]: create /proc/brisket/%s failed\n",
				__func__,
				brisket_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int brisket_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	int cpu;

	node = pdev->dev.of_node;
	if (!node) {
		brisket_err("get brisket device node err\n");
		return -ENODEV;
	}

	/*brisket05*/
	rc = of_property_read_u32(node,
		"brisket_doe_brisket05", &brisket_doe_brisket05);

	if (!rc) {
		brisket_msg(
			"brisket_doe_brisket05 from DTree; rc(%d) brisket_doe_brisket05(0x%x)\n",
			rc,
			brisket_doe_brisket05);

		if (brisket_doe_brisket05 > 0) {
			for (cpu = BRISKET_CPU_START_ID;
				cpu <= BRISKET_CPU_END_ID; cpu++) {
				mtk_brisket(cpu,
							5,
							20,
							0,
							brisket_doe_brisket05);
			}
		}
	}

	/*brisket06*/
	rc = of_property_read_u32(node,
		"brisket_doe_brisket06", &brisket_doe_brisket06);

	if (!rc) {
		brisket_msg(
			"brisket_doe_brisket06 from DTree; rc(%d) brisket_doe_brisket06(0x%x)\n",
			rc,
			brisket_doe_brisket06);

		if (brisket_doe_brisket06 > 0) {
			for (cpu = BRISKET_CPU_START_ID;
				cpu <= BRISKET_CPU_END_ID; cpu++) {
				mtk_brisket(cpu,
							6,
							15,
							0,
							brisket_doe_brisket06);
			}
		}
	}

	/*brisket07*/
	rc = of_property_read_u32(node,
		"brisket_doe_brisket07", &brisket_doe_brisket07);

	if (!rc) {
		brisket_msg(
			"brisket_doe_brisket07 from DTree; rc(%d) brisket_doe_brisket07(0x%x)\n",
			rc,
			brisket_doe_brisket07);

		if (brisket_doe_brisket07 > 0) {
			for (cpu = BRISKET_CPU_START_ID;
				cpu <= BRISKET_CPU_END_ID; cpu++) {
				mtk_brisket(cpu,
							7,
							11,
							0,
							brisket_doe_brisket07);
			}
		}
	}

	/*brisket08*/
	rc = of_property_read_u32(node,
		"brisket_doe_brisket08", &brisket_doe_brisket08);

	if (!rc) {
		brisket_msg(
			"brisket_doe_brisket08 from DTree; rc(%d) brisket_doe_brisket08(0x%x)\n",
			rc,
			brisket_doe_brisket08);

		if (brisket_doe_brisket08 > 0) {
			for (cpu = BRISKET_CPU_START_ID;
				cpu <= BRISKET_CPU_END_ID; cpu++) {
				mtk_brisket(cpu,
							8,
							12,
							0,
							brisket_doe_brisket08);
			}
		}
	}

	/*brisket09*/
	rc = of_property_read_u32(node,
		"brisket_doe_brisket09", &brisket_doe_brisket09);

	if (!rc) {
		brisket_msg(
			"brisket_doe_brisket09 from DTree; rc(%d) brisket_doe_brisket09(0x%x)\n",
			rc,
			brisket_doe_brisket09);

		if (brisket_doe_brisket09 > 0) {
			for (cpu = BRISKET_CPU_START_ID;
				cpu <= BRISKET_CPU_END_ID; cpu++) {
				mtk_brisket(cpu,
							9,
							13,
							0,
							brisket_doe_brisket09);
			}
		}
	}

	/* pllclken control */
	rc = of_property_read_u32(node,
		"brisket_doe_pllclken", &brisket_doe_pllclken);

	if (!rc) {
		brisket_msg(
			"brisket_doe_pllclken from DTree; rc(%d) brisket_doe_pllclken(0x%x)\n",
			rc,
			brisket_doe_pllclken);

		if (brisket_doe_pllclken < 256)
			mtk_brisket_pllclken(brisket_doe_pllclken);
	}

	/* bren control */
	rc = of_property_read_u32(node,
		"brisket_doe_bren", &brisket_doe_bren);

	if (!rc) {
		brisket_msg(
			"brisket_doe_bren from DTree; rc(%d) brisket_doe_bren(0x%x)\n",
			rc,
			brisket_doe_bren);

		if (brisket_doe_bren < 256)
			mtk_brisket_bren(brisket_doe_bren);
	}

	/* dump register information to picachu buf */
#ifdef CONFIG_OF_RESERVED_MEM
	brisket_reserve_memory_init(0);
#endif

	brisket_msg("brisket probe ok!!\n");
#endif
#endif
	return 0;
}

static int brisket_suspend(struct platform_device *pdev, pm_message_t state)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
//	brisket_reserve_memory_init(1);
#endif
#endif

	return 0;
}

static int brisket_resume(struct platform_device *pdev)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
//	brisket_reserve_memory_init(2);
#endif
#endif

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
		brisket_err("BRISKET driver callback register failed..\n");
		return err;
	}

	return 0;
}

static void __exit __brisket_exit(void)
{
	brisket_msg("brisket de-initialization\n");
}


module_init(__brisket_init);
module_exit(__brisket_exit);

MODULE_DESCRIPTION("MediaTek BRISKET Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_BRISKET_C__
