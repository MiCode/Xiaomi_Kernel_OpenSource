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
 * @file	mkt_fll.c
 * @brief   Driver for fll
 *
 */

#define __MTK_FLL_C__

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
#include <linux/topology.h>

#ifdef __KERNEL__
	#include <mt-plat/mtk_chip.h>
	#include <mt-plat/mtk_devinfo.h>
	#include <mt-plat/sync_write.h>
	#include <mt-plat/mtk_secure_api.h>
	#include "mtk_devinfo.h"
	#include "mtk_ptp3_common.h"
	#include "mtk_ptp3_fll.h"
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

#define FLL_DEBUG
#define FLL_TAG	 "[FLL]"

#define fll_err(fmt, args...)	\
	pr_info(FLL_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define fll_msg(fmt, args...)	\
	pr_info(FLL_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef FLL_DEBUG
#define fll_debug(fmt, args...)	\
	pr_debug(FLL_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define fll_debug(fmt, args...)
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
static unsigned int fll_doe_fllCtrl;

#endif
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

static char *fll_buf;
static unsigned long long fll_mem_size;
void fll_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	fll_buf = buf;
	fll_mem_size = ptp3_mem_size;
}

int fll_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum FLL_TRIGGER_STAGE fll_tri_stage)
{
	int str_len = 0;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);
	unsigned int cpu;

	/* check free page valid or not */
	if (!aee_log_buf) {
		fll_msg("unable to get free page!\n");
		return -1;
	}
	fll_msg("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* show trigger stage */
	switch (fll_tri_stage) {
	case FLL_TRIGGER_STAGE_PROBE:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Probe]\n");
		break;
	case FLL_TRIGGER_STAGE_SUSPEND:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Suspend]\n");
		break;
	case FLL_TRIGGER_STAGE_RESUME:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Resume]\n");
		break;
	default:
		fll_err("illegal FLL_TRIGGER_STAGE\n");
		break;
	}


	/* collect dump info */
	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		/* fill data to aee_log_buf */
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	fll_debug("\n%s", aee_log_buf);
	fll_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif
#endif

/************************************************
 * SMC between kernel and atf
 ************************************************/

/************************************************
 * IPI between kernel and mcupm/cpu_eb
 ************************************************/
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
static void fll_ipi_handle(unsigned int cpu, unsigned int group,
	unsigned int bits, unsigned int shift, unsigned int val)
{
	struct ptp3_ipi_data fll_data;

	fll_data.cmd = PTP3_IPI_FLL;
	fll_data.u.fll.cfg = (cpu << 24) | (group << 16) | (shift << 8) | bits;
	fll_data.u.fll.val = val;

	fll_msg("[%s]:cpu(%d) group(%d) shift(%d) bits(%d) val(%d)\n",
		__func__, cpu, group, shift, bits, val);

	/* update mcupm or cpueb via ipi */
	while (ptp3_ipi_handle(&fll_data) != 0)
		udelay(500);
}
#else
static void fll_ipi_handle(unsigned int cpu, unsigned int group,
	unsigned int bits, unsigned int shift, unsigned int val)
{
	fll_msg("IPI from kernel to MCUPM not exist\n");
}
#endif

/************************************************
 * static function
 ************************************************/

/************************************************
 * set FLL status by procfs interface
 ************************************************/
static unsigned int _proc_write_allocate_buf(
	const char __user *buffer, size_t count, char *buf)
{
	/* proc template for check */
	buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		fll_err("buf(%d) is illegal\n");
		return 0;
	}

	if (count >= PAGE_SIZE) {
		fll_err("count(%d) >= PAGE_SIZE\n");
		return 0;
	}

	if (copy_from_user(buf, buffer, count)) {
		fll_err("buffer copy fail\n");
		return 0;
	}

	buf[count] = '\0';

	return 1;
}

unsigned int option_r;
static ssize_t fll_ctrl_r_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int ret = 0;
	char *buf = 0;

	/* allocate buf for proc_write */
	ret = _proc_write_allocate_buf(buffer, count, buf);

	if (ret) {
		/* parameter check */
		if (kstrtou32((const char *)buf, 0, &option_r)) {
			fll_err("bad argument!! Should input 1 arguments.\n");
			goto out;
		}
	}

out:
	free_page((unsigned long)buf);
	return count;

}

static int fll_ctrl_r_proc_show(struct seq_file *m, void *v)
{
	unsigned int value, cpu;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {

		/* update via atf */
		value = ptp3_smc_handle(
			FLL_RW_READ,
			cpu,
			option_r,
			value);

		seq_printf(m, FLL_TAG"[CPU%d] FLL_CTRL(%d):%d\n",
			cpu,
			FLL_LIST_NAME[option_r],
			value);
	}
	return 0;
}

static ssize_t fll_ctrl_w_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cpu, option, value;
	unsigned int ret = 0;
	char *buf = 0;

	/* allocate buf for proc_write */
	ret = _proc_write_allocate_buf(buffer, count, buf);

	if (ret) {
		/* parameter check */
		if (sscanf(buf, "%u %u %u",
			&cpu, &option, &value) != 3) {

			fll_err("bad argument!! Should input 3 arguments.\n");
			goto out;
		}

		/* update via atf */
		ptp3_smc_handle(
			FLL_RW_WRITE,
			cpu,
			option,
			value);

		/* update via mcupm or cpu_eb */
		fll_ipi_handle(0, 0, 0, 0, 0);

	}

out:
	free_page((unsigned long)buf);
	return count;

}

static int fll_ctrl_w_proc_show(struct seq_file *m, void *v)
{
	return 0;
}


static int fll_list_proc_show(struct seq_file *m, void *v)
{
	unsigned int list_num;

	for (list_num = 0; list_num < NR_FLL_LIST; list_num++)
		seq_printf(m, "%d.%s\n", list_num, FLL_LIST_NAME[list_num]);

	return 0;
}

PROC_FOPS_RW(fll_ctrl_r);
PROC_FOPS_RW(fll_ctrl_w);
PROC_FOPS_RO(fll_list);

int fll_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry fll_entries[] = {
		PROC_ENTRY(fll_ctrl_r),
		PROC_ENTRY(fll_ctrl_w),
		PROC_ENTRY(fll_list),
	};

	for (i = 0; i < ARRAY_SIZE(fll_entries); i++) {
		if (!proc_create(fll_entries[i].name,
			0664,
			dir,
			fll_entries[i].fops)) {
			fll_err("[%s]: create /proc/%s/%s failed\n",
				__func__,
				proc_name,
				fll_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int fll_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int cpu, option, value;

	node = pdev->dev.of_node;
	if (!node) {
		fll_err("get fll device node err\n");
		return -ENODEV;
	}

	/* fll_doe_fllCtrl */
	rc = of_property_read_u32(node,
		"fll_doe_fllCtrl", &fll_doe_fllCtrl);

	if (!rc) {
		fll_msg(
			"fll_doe_fllCtrl from DTree; rc(%d) fll_doe_fllCtrl(0x%x)\n",
			rc,
			fll_doe_fllCtrl);

		cpu = (fll_doe_fllCtrl & 0xF0000000) >> 28;
		option = (fll_doe_fllCtrl & 0xFFF0000) >> 16;
		value = fll_doe_fllCtrl & 0xFFFF;

		if (fll_doe_fllCtrl != 0xFFFFFFFF) {
			/* update via atf */
			ptp3_smc_handle(
				FLL_RW_WRITE,
				cpu,
				option,
				value);
		}
	}

	/* dump reg status into PICACHU dram for DB */
	fll_reserve_memory_dump(
		fll_buf, fll_mem_size, FLL_TRIGGER_STAGE_PROBE);

	fll_msg("fll probe ok!!\n");
#endif
#endif
	return 0;
}

int fll_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	fll_reserve_memory_dump(
		fll_buf+0x1000, fll_mem_size, FLL_TRIGGER_STAGE_SUSPEND);
#endif
#endif
	return 0;
}

int fll_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	fll_reserve_memory_dump(
		fll_buf+0x2000, fll_mem_size, FLL_TRIGGER_STAGE_RESUME);
#endif
#endif
	return 0;
}

MODULE_DESCRIPTION("MediaTek FLL Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_FLL_C__
