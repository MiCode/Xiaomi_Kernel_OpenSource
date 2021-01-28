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
 * @file	mkt_brisket2.c
 * @brief   Driver for brisket2
 *
 */

#define __MTK_BRISKET2_C__

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
	#include "mtk_ptp3_brisket2.h"
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

#define BRISKET2_DEBUG
#define BRISKET2_TAG	 "[BRISKET2]"

#define brisket2_err(fmt, args...)	\
	pr_info(BRISKET2_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define brisket2_msg(fmt, args...)	\
	pr_info(BRISKET2_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef BRISKET2_DEBUG
#define brisket2_debug(fmt, args...)	\
	pr_debug(BRISKET2_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define brisket2_debug(fmt, args...)
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
static unsigned int brisket2_doe_brisket2Ctrl;

#endif
#endif

static const char BRISKET2_LIST_NAME[][40] = {
	SamplerEn,
	DrccGateEn,
	ConfigComplete,
	FllEn,
	FllClkOutSelect,
	FllSlowReqEn,
	FllSlowReqGateEn,
	CttEn,
	TestMode,
	GlobalEventEn,
	SafeFreqReqOverride
};

static const char BRISKET2_RW_REG_NAME[][5] = {
	V101
};

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

static char *brisket2_buf;
static unsigned long long brisket2_mem_size;
void brisket2_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	brisket2_buf = buf;
	brisket2_mem_size = ptp3_mem_size;
}

int brisket2_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum BRISKET2_TRIGGER_STAGE brisket2_tri_stage)
{
	int str_len = 0;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);
	unsigned int cpu;

	/* check free page valid or not */
	if (!aee_log_buf) {
		brisket2_msg("unable to get free page!\n");
		return -1;
	}
	brisket2_msg("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* show trigger stage */
	switch (brisket2_tri_stage) {
	case BRISKET2_TRIGGER_STAGE_PROBE:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Probe]\n");
		break;
	case BRISKET2_TRIGGER_STAGE_SUSPEND:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Suspend]\n");
		break;
	case BRISKET2_TRIGGER_STAGE_RESUME:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Resume]\n");
		break;
	default:
		brisket2_err("illegal BRISKET2_TRIGGER_STAGE\n");
		break;
	}


	/* collect dump info */
	for (cpu = BRISKET2_CPU_START_ID; cpu <= BRISKET2_CPU_END_ID; cpu++) {
		/* fill data to aee_log_buf */
		break;
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	brisket2_debug("\n%s", aee_log_buf);
	brisket2_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif
#endif

/************************************************
 * IPI between kernel and mcupm/cpu_eb
 ************************************************/
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
static void brisket2_ipi_handle(unsigned int cpu, unsigned int group,
	unsigned int bits, unsigned int shift, unsigned int val)
{
	struct ptp3_ipi_data brisket2_data;

	brisket2_data.cmd = PTP3_IPI_BRISKET2;
	brisket2_data.u.brisket2.cfg = (cpu << 24) | (group << 16) | (shift << 8) | bits;
	brisket2_data.u.brisket2.val = val;

	brisket2_msg("[%s]:cpu(%d) group(%d) shift(%d) bits(%d) val(%d)\n",
		__func__, cpu, group, shift, bits, val);

	/* update mcupm or cpueb via ipi */
	while (ptp3_ipi_handle(&brisket2_data) != 0)
		udelay(500);
}
#else
static void brisket2_ipi_handle(unsigned int cpu, unsigned int group,
	unsigned int bits, unsigned int shift, unsigned int val)
{
	brisket2_msg("IPI from kernel to MCUPM not exist\n");
}
#endif

/************************************************
 * static function
 ************************************************/

/************************************************
 * set BRISKET2 status by procfs interface
 ************************************************/
static ssize_t brisket2_ctrl_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cpu, option, value;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		brisket2_err("buf(%d) is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		brisket2_err("count(%d) >= PAGE_SIZE\n");
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		brisket2_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %u",
		&cpu, &option, &value) != 3) {

		brisket2_err("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	/* encode cfg */
	/*
	 *	cfg[15:8] option
	 *	cfg[31:28] cpu
	 */
	cfg = (option << BRISKET2_CFG_OFFSET_OPTION) & BRISKET2_CFG_BITMASK_OPTION;
	cfg |= (cpu << BRISKET2_CFG_OFFSET_CPU) & BRISKET2_CFG_BITMASK_CPU;

	/* update via atf */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_BRISKET2,
		BRISKET2_NODE_LIST_WRITE,
		cfg,
		value);

	if (ret < 0) {
		brisket2_err("ret(%d). access atf fail\n", ret);
		return ret;
	}

	/* update via mcupm or cpu_eb */
	brisket2_ipi_handle(0, 0, 0, 0, 0);

out:
	free_page((unsigned long)buf);
	return count;

}

static int brisket2_ctrl_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int brisket2_list_proc_show(struct seq_file *m, void *v)
{
	unsigned int list_num;

	for (list_num = 0; list_num < NR_BRISKET2_LIST; list_num++)
		seq_printf(m, "%d.%s\n", list_num, BRISKET2_LIST_NAME[list_num]);

	return 0;
}

static int brisket2_info_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value, option;

	for (cpu = BRISKET2_CPU_START_ID; cpu <= BRISKET2_CPU_END_ID; cpu++) {
		seq_printf(m, BRISKET2_TAG"[CPU%d]", cpu);

		for (option = 0; option < NR_BRISKET2_LIST; option++) {
			/* encode cfg */
			/*
			 *	cfg[15:8] option
			 *	cfg[31:28] cpu
			 */
			cfg = (option << BRISKET2_CFG_OFFSET_OPTION) & BRISKET2_CFG_BITMASK_OPTION;
			cfg |= (cpu << BRISKET2_CFG_OFFSET_CPU) & BRISKET2_CFG_BITMASK_CPU;

			/* update via atf */
			value = ptp3_smc_handle(
				PTP3_FEATURE_BRISKET2,
				BRISKET2_NODE_LIST_READ,
				cfg,
				0);

			if (option != NR_BRISKET2_LIST-1)
				seq_printf(m, " %s:0x%x,",
					BRISKET2_LIST_NAME[option], value);
			else
				seq_printf(m, " %s:0x%x;\n",
					BRISKET2_LIST_NAME[option], value);

		}
	}
	return 0;
}

static ssize_t brisket2_reg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cpu, group, value;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		brisket2_err("buf(%d) is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		brisket2_err("count(%d) >= PAGE_SIZE\n");
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		brisket2_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %u",
		&cpu, &group, &value) != 3) {

		brisket2_err("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	/* encode cfg */
	/*
	 *	cfg[15:8] option
	 *	cfg[31:28] cpu
	 */
	cfg = (group << BRISKET2_CFG_OFFSET_OPTION) & BRISKET2_CFG_BITMASK_OPTION;
	cfg |= (cpu << BRISKET2_CFG_OFFSET_CPU) & BRISKET2_CFG_BITMASK_CPU;

	/* update via atf */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_BRISKET2,
		BRISKET2_NODE_RW_REG_WRITE,
		cfg,
		value);

	if (ret < 0) {
		brisket2_err("ret(%d). access atf fail\n", ret);
		goto out;
	}

	/* update via mcupm or cpu_eb */
	brisket2_ipi_handle(0, 0, 0, 0, 0);

out:
	free_page((unsigned long)buf);
	return count;

}


static int brisket2_reg_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value, option;

	for (cpu = BRISKET2_CPU_START_ID; cpu <= BRISKET2_CPU_END_ID; cpu++) {
		seq_printf(m, BRISKET2_TAG"[CPU%d]", cpu);

		for (option = 0; option < NR_BRISKET2_RW_GROUP; option++) {

			/* encode cfg */
			/*
			 *	cfg[15:8] option
			 *	cfg[31:28] cpu
			 */
			cfg = (option << BRISKET2_CFG_OFFSET_OPTION) &
				BRISKET2_CFG_BITMASK_OPTION;
			cfg |= (cpu << BRISKET2_CFG_OFFSET_CPU) & BRISKET2_CFG_BITMASK_CPU;

			/* update via atf */
			value = ptp3_smc_handle(
				PTP3_FEATURE_BRISKET2,
				BRISKET2_NODE_RW_REG_READ,
				cfg,
				0);

			if (option != NR_BRISKET2_RW_GROUP-1)
				seq_printf(m, " %s:0x%08x,",
					BRISKET2_RW_REG_NAME[option], value);
			else
				seq_printf(m, " %s:0x%08x;\n",
					BRISKET2_RW_REG_NAME[option], value);
		}
	}

	return 0;
}

static int brisket2_cfg_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value;
	unsigned int bitmap = 0;

	for (cpu = BRISKET2_CPU_START_ID; cpu <= BRISKET2_CPU_END_ID; cpu++) {
		/* encode cfg */
		/*
		 *	cfg[15:8] option
		 *	cfg[31:28] cpu
		 */
		cfg = (BRISKET2_LIST_FllEn << BRISKET2_CFG_OFFSET_OPTION) &
			BRISKET2_CFG_BITMASK_OPTION;
		cfg |= (cpu << BRISKET2_CFG_OFFSET_CPU) & BRISKET2_CFG_BITMASK_CPU;

		/* update via atf */
		value = ptp3_smc_handle(
			PTP3_FEATURE_BRISKET2,
			BRISKET2_NODE_LIST_READ,
			cfg,
			0);

		bitmap |= value << (cpu * 4);
	}

	/* output bitmap result */
	seq_printf(m, "%08x\n", bitmap);

	return 0;
}

PROC_FOPS_RW(brisket2_ctrl);
PROC_FOPS_RO(brisket2_list);
PROC_FOPS_RO(brisket2_info);
PROC_FOPS_RW(brisket2_reg);
PROC_FOPS_RO(brisket2_cfg);

int brisket2_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry brisket2_entries[] = {
		PROC_ENTRY(brisket2_ctrl),
		PROC_ENTRY(brisket2_list),
		PROC_ENTRY(brisket2_info),
		PROC_ENTRY(brisket2_reg),
		PROC_ENTRY(brisket2_cfg),
	};

	for (i = 0; i < ARRAY_SIZE(brisket2_entries); i++) {
		if (!proc_create(brisket2_entries[i].name,
			0664,
			dir,
			brisket2_entries[i].fops)) {
			brisket2_err("[%s]: create /proc/%s/%s failed\n",
				__func__,
				proc_name,
				brisket2_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int brisket2_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int cpu, option, value;

	node = pdev->dev.of_node;
	if (!node) {
		brisket2_err("get brisket2 device node err\n");
		return -ENODEV;
	}

	/* brisket2_doe_brisket2Ctrl */
	rc = of_property_read_u32(node,
		"brisket2_doe_brisket2Ctrl", &brisket2_doe_brisket2Ctrl);

	if (!rc) {
		brisket2_msg(
			"brisket2_doe_brisket2Ctrl from DTree; rc(%d) brisket2_doe_brisket2Ctrl(0x%x)\n",
			rc,
			brisket2_doe_brisket2Ctrl);

		cpu = (brisket2_doe_brisket2Ctrl & 0xF0000000) >> 28;
		option = (brisket2_doe_brisket2Ctrl & 0xFFF0000) >> 16;
		value = brisket2_doe_brisket2Ctrl & 0xFFFF;

		if (brisket2_doe_brisket2Ctrl != 0xFFFFFFFF) {
			/* update via atf */
			ptp3_smc_handle(
				BRISKET2_NODE_LIST_WRITE,
				cpu,
				option,
				value);
		}
	}

	/* dump reg status into PICACHU dram for DB */
	brisket2_reserve_memory_dump(
		brisket2_buf, brisket2_mem_size, BRISKET2_TRIGGER_STAGE_PROBE);

	brisket2_msg("brisket2 probe ok!!\n");
#endif
#endif
	return 0;
}

int brisket2_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	brisket2_reserve_memory_dump(
		brisket2_buf+0x1000, brisket2_mem_size, BRISKET2_TRIGGER_STAGE_SUSPEND);
#endif
#endif
	return 0;
}

int brisket2_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	brisket2_reserve_memory_dump(
		brisket2_buf+0x2000, brisket2_mem_size, BRISKET2_TRIGGER_STAGE_RESUME);
#endif
#endif
	return 0;
}

MODULE_DESCRIPTION("MediaTek BRISKET2 Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_BRISKET2_C__
