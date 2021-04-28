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
 * @file	mkt_cinst.c
 * @brief   Driver for cinst
 *
 */

#define __MTK_CINST_C__

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
#include <linux/topology.h>

#ifdef __KERNEL__
	#include <mt-plat/mtk_chip.h>
	#include <mt-plat/mtk_devinfo.h>
	#include <mt-plat/sync_write.h>
	#include <mt-plat/mtk_secure_api.h>
	#include "mtk_devinfo.h"
	#include "mtk_ptp3_common.h"
	#include "mtk_ptp3_cinst.h"
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

/* #define CINST_DEBUG */
#define CINST_TAG	 "[CINST]"

#define cinst_err(fmt, args...)	\
	pr_info(CINST_TAG"[ERROR][%s():%d]" fmt,\
	__func__, __LINE__, ##args)

#define cinst_msg(fmt, args...)	\
	pr_info(CINST_TAG"[INFO][%s():%d]" fmt,\
	__func__, __LINE__, ##args)

#ifdef CINST_DEBUG
#define cinst_debug(fmt, args...)	\
	pr_debug(CINST_TAG"[DEBUG][%s():%d]" fmt,\
	__func__, __LINE__, ##args)
#else
#define cinst_debug(fmt, args...)
#endif

/************************************************
 * Marco definition
 ************************************************/

/* efuse: PTPOD index */
#define DEVINFO_IDX_0 50

#define REG_INVALID 0xdeadbeef
#define REG_DEFAULT 0x03e403e4

/************************************************
 * static Variable
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF

static unsigned int cinst_doe_ls_enable;
static unsigned int cinst_doe_ls_period;
static unsigned int cinst_doe_ls_credit;
static unsigned int cinst_doe_vx_enable;
static unsigned int cinst_doe_vx_period;
static unsigned int cinst_doe_vx_credit;
static unsigned int cinst_doe_ls_low_en;
static unsigned int cinst_doe_ls_low_period;
static unsigned int cinst_doe_vx_low_en;
static unsigned int cinst_doe_vx_low_period;
static unsigned int cinst_doe_ls_const_en;
static unsigned int cinst_doe_vx_const_en;

#endif /* CONFIG_OF */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

#ifdef PTP3_STATUS_PROBE_DUMP
static char *cinst_buf;
static unsigned long long cinst_mem_size;
void cinst_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	cinst_buf = buf;
	cinst_mem_size = ptp3_mem_size;
}

int cinst_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum CINST_TRIGGER_STAGE cinst_tri_stage)
{
	struct cinst_class cinst_b;
	unsigned int str_len = 0, cinst_n = 0;
	unsigned int *value_b = (unsigned int *)&cinst_b;

	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		cinst_err("unable to get free page!\n");
		return -1;
	}
	cinst_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value_b[0] = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_DRCC, CINST_GROUP_READ,
				CINST_CONTROL_OFFSET, cinst_n);
		value_b[1] = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_DRCC, CINST_GROUP_READ,
				LS_DIDT_CONTROL_OFFSET, cinst_n);
		value_b[2] = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_DRCC, CINST_GROUP_READ,
				VX_DIDT_CONTROL_OFFSET, cinst_n);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"[CINST][CPU%d] ", cinst_n);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"rg_ls_ctrl_en:0x%x, ", cinst_b.cinst_rg_ls_ctrl_en);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"rg_vx_ctrl_en:0x%x, ", cinst_b.cinst_rg_vx_ctrl_en);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"rg_ls_cfg_pipe_issue_sel:0x%x, ",
				cinst_b.cinst_rg_ls_cfg_pipe_issue_sel);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"rg_vx_cfg_pipe_issue_sel:0x%x, ",
				cinst_b.cinst_rg_vx_cfg_pipe_issue_sel);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_cfg_const_en:0x%x, ", cinst_b.cinst_ls_cfg_const_en);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_ctrl_en_local:0x%x, ", cinst_b.cinst_ls_ctrl_en_local);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_low_freq:0x%x, ", cinst_b.cinst_ls_low_freq);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_cfg_low_freq_en:0x%x, ", cinst_b.cinst_ls_cfg_low_freq_en);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_cfg_low_period:0x%x, ", cinst_b.cinst_ls_cfg_low_period);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_cfg_period:0x%x, ", cinst_b.cinst_ls_cfg_period);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"ls_cfg_credit:0x%x, ", cinst_b.cinst_ls_cfg_credit);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_cfg_const_en:0x%x, ", cinst_b.cinst_vx_cfg_const_en);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_ctrl_en_local:0x%x, ", cinst_b.cinst_vx_ctrl_en_local);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_low_freq:0x%x, ", cinst_b.cinst_vx_low_freq);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_cfg_low_freq_en:0x%x, ", cinst_b.cinst_vx_cfg_low_freq_en);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_cfg_low_period:0x%x, ", cinst_b.cinst_vx_cfg_low_period);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_cfg_period:0x%x, ", cinst_b.cinst_vx_cfg_period);
		str_len += snprintf(aee_log_buf + str_len,
				(unsigned long long)cinst_mem_size - str_len,
				"vx_cfg_credit:0x%x\n", cinst_b.cinst_vx_cfg_credit);
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	cinst_debug("\n%s", aee_log_buf);
	cinst_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}
#endif /* PTP3_STATUS_PROBE_DUMP */

#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * SMC between kernel and atf
 ************************************************/
static unsigned int cinst_smc_handle(unsigned int group,
	unsigned int val, unsigned int cpu)
{
	unsigned int ret;

	cinst_msg("[%s]:cpu(%d) param(%d) val(%d)\n",
		__func__, cpu, group, val);

	/* update atf via smc */
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		PTP3_FEATURE_CINST,
		group,
		val,
		cpu);

	return ret;
}

/************************************************
 * IPI between kernel and mcupm/cpu_eb
 ************************************************/
#if 0 //xxx
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
static void cinst_ipi_handle(
	unsigned int cpu, unsigned int param, unsigned int val)
{
	struct ptp3_ipi_data cinst_data;

	cinst_data.cmd = PTP3_IPI_CINST;
	cinst_data.u.cinst.cfg = (param << 4) | cpu;
	cinst_data.u.cinst.val = val;

	cinst_msg("[%s]:cpu(%d) param(%d) val(%d)\n",
		__func__, cpu, param, val);

	/* update mcupm or cpueb via ipi */
	while (ptp3_ipi_handle(&cinst_data) != 0)
		udelay(500);
}
#else
static void cinst_ipi_handle(
	unsigned int cpu, unsigned int param, unsigned int val)
{
	cinst_msg("IPI from kernel to MCUPM not exist\n");
}
#endif
#endif

/************************************************
 * set CINST status by procfs interface
 ************************************************/
static ssize_t cinst_ls_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		cinst_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		cinst_smc_handle(CINST_GROUP_LS_ENABLE, (enable >> cinst_n) & 0x01, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_ls_enable_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;

	value = cinst_smc_handle(CINST_GROUP_LS_ENABLE_R, 0, CINST_CPU_START_ID);

	seq_printf(m, "[CINST] %x\n", value);

	return 0;
}

static ssize_t cinst_ls_credit_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &cinst_n) != 2) {
		cinst_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	cinst_smc_handle(CINST_GROUP_LS_CREDIT, value, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_ls_credit_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_LS_CREDIT_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_ls_period_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &cinst_n) != 2) {
		cinst_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	cinst_smc_handle(CINST_GROUP_LS_PERIOD, value, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_ls_period_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_LS_PERIOD_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

/* VX_DIDT_CONTORL */
static ssize_t cinst_vx_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		cinst_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		cinst_smc_handle(CINST_GROUP_VX_ENABLE, (enable >> cinst_n) & 0x01, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_vx_enable_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;

	value = cinst_smc_handle(CINST_GROUP_VX_ENABLE_R, 0, CINST_CPU_START_ID);

	seq_printf(m, "[CINST] %x\n", value);

	return 0;
}

static ssize_t cinst_vx_credit_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &cinst_n) != 2) {
		cinst_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	cinst_smc_handle(CINST_GROUP_VX_CREDIT, value, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_vx_credit_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_VX_CREDIT_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_vx_period_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &cinst_n) != 2) {
		cinst_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	cinst_smc_handle(CINST_GROUP_VX_PERIOD, value, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_vx_period_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_VX_PERIOD_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_ls_low_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		cinst_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		cinst_smc_handle(CINST_GROUP_LS_LOW_EN, (enable >> cinst_n) & 0x01, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_ls_low_en_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_LS_LOW_EN_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_ls_low_period_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &cinst_n) != 2) {
		cinst_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	cinst_smc_handle(CINST_GROUP_LS_LOW_PERIOD, value, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_ls_low_period_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_LS_LOW_PERIOD_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_vx_low_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		cinst_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		cinst_smc_handle(CINST_GROUP_VX_LOW_EN, (enable >> cinst_n) & 0x01, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_vx_low_en_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_VX_LOW_EN_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_vx_low_period_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &cinst_n) != 2) {
		cinst_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	cinst_smc_handle(CINST_GROUP_VX_LOW_PERIOD, value, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_vx_low_period_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_VX_LOW_PERIOD_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}


static ssize_t cinst_ls_const_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		cinst_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		cinst_smc_handle(CINST_GROUP_LS_CONST_EN, (enable >> cinst_n) & 0x01, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_ls_const_en_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_LS_CONST_EN_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static ssize_t cinst_vx_const_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, cinst_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		cinst_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		cinst_smc_handle(CINST_GROUP_VX_CONST_EN, (enable >> cinst_n) & 0x01, cinst_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int cinst_vx_const_en_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value = cinst_smc_handle(CINST_GROUP_VX_CONST_EN_R, 0, cinst_n);
		seq_printf(m, "[CINST][%d] %x\n", cinst_n, value);
	}

	return 0;
}

static int cinst_cfg_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, cinst_n = 0, temp = 0;

	value = cinst_smc_handle(CINST_GROUP_CFG, 0, CINST_CPU_START_ID);

	for (cinst_n = 0; cinst_n <= CINST_CPU_END_ID; cinst_n++)
		temp |= (((value & (0x1 << cinst_n)) >> cinst_n) << (cinst_n * 4));

	seq_printf(m, "%08x\n", temp);

	return 0;
}

static int cinst_dump_proc_show(struct seq_file *m, void *v)
{
	struct cinst_class cinst_b;
	unsigned int *value_b = (unsigned int *)&cinst_b;
	unsigned int cinst_n = 0;
	const unsigned int cinst_group = CINST_GROUP_READ;

	for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
		value_b[0] = cinst_smc_handle(cinst_group,
			CINST_CONTROL_OFFSET, cinst_n);
		value_b[1] = cinst_smc_handle(cinst_group,
			LS_DIDT_CONTROL_OFFSET, cinst_n);
		value_b[2] = cinst_smc_handle(cinst_group,
			VX_DIDT_CONTROL_OFFSET, cinst_n);
		seq_printf(m, "[CINST][CPU%d] ", cinst_n);
		seq_printf(m, "rg_ls_ctrl_en:0x%x, ",
			cinst_b.cinst_rg_ls_ctrl_en);
		seq_printf(m, "rg_vx_ctrl_en:0x%x, ",
			cinst_b.cinst_rg_vx_ctrl_en);
		seq_printf(m, "rg_ls_cfg_pipe_issue_sel:0x%x, ",
			cinst_b.cinst_rg_ls_cfg_pipe_issue_sel);
		seq_printf(m, "rg_vx_cfg_pipe_issue_sel:0x%x, ",
			cinst_b.cinst_rg_vx_cfg_pipe_issue_sel);
		seq_printf(m, "ls_cfg_const_en:0x%x, ",
			cinst_b.cinst_ls_cfg_const_en);
		seq_printf(m, "ls_ctrl_en_local:0x%x, ",
			cinst_b.cinst_ls_ctrl_en_local);
		seq_printf(m, "ls_low_freq:0x%x, ", cinst_b.cinst_ls_low_freq);
		seq_printf(m, "ls_cfg_low_freq_en:0x%x, ",
			cinst_b.cinst_ls_cfg_low_freq_en);
		seq_printf(m, "ls_cfg_low_period:0x%x, ",
			cinst_b.cinst_ls_cfg_low_period);
		seq_printf(m, "ls_cfg_period:0x%x, ",
			cinst_b.cinst_ls_cfg_period);
		seq_printf(m, "ls_cfg_credit:0x%x, ",
			cinst_b.cinst_ls_cfg_credit);
		seq_printf(m, "vx_cfg_const_en:0x%x, ",
			cinst_b.cinst_vx_cfg_const_en);
		seq_printf(m, "vx_ctrl_en_local:0x%x, ",
			cinst_b.cinst_vx_ctrl_en_local);
		seq_printf(m, "vx_low_freq:0x%x, ",
			cinst_b.cinst_vx_low_freq);
		seq_printf(m, "vx_cfg_low_freq_en:0x%x, ",
			cinst_b.cinst_vx_cfg_low_freq_en);
		seq_printf(m, "vx_cfg_low_period:0x%x, ",
			cinst_b.cinst_vx_cfg_low_period);
		seq_printf(m, "vx_cfg_period:0x%x, ",
			cinst_b.cinst_vx_cfg_period);
		seq_printf(m, "vx_cfg_credit:0x%x\n",
			cinst_b.cinst_vx_cfg_credit);
	}
	return 0;
}

PROC_FOPS_RW(cinst_ls_enable);
PROC_FOPS_RW(cinst_ls_credit);
PROC_FOPS_RW(cinst_ls_period);
PROC_FOPS_RW(cinst_vx_enable);
PROC_FOPS_RW(cinst_vx_credit);
PROC_FOPS_RW(cinst_vx_period);
PROC_FOPS_RW(cinst_ls_low_en);
PROC_FOPS_RW(cinst_ls_low_period);
PROC_FOPS_RW(cinst_vx_low_en);
PROC_FOPS_RW(cinst_vx_low_period);
PROC_FOPS_RW(cinst_ls_const_en);
PROC_FOPS_RW(cinst_vx_const_en);
PROC_FOPS_RO(cinst_cfg);
PROC_FOPS_RO(cinst_dump);

int cinst_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *cinst_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry cinst_entries[] = {
		PROC_ENTRY(cinst_ls_enable),
		PROC_ENTRY(cinst_ls_credit),
		PROC_ENTRY(cinst_ls_period),
		PROC_ENTRY(cinst_vx_enable),
		PROC_ENTRY(cinst_vx_credit),
		PROC_ENTRY(cinst_vx_period),
		PROC_ENTRY(cinst_ls_low_en),
		PROC_ENTRY(cinst_ls_low_period),
		PROC_ENTRY(cinst_vx_low_en),
		PROC_ENTRY(cinst_vx_low_period),
		PROC_ENTRY(cinst_ls_const_en),
		PROC_ENTRY(cinst_vx_const_en),
		PROC_ENTRY(cinst_cfg),
		PROC_ENTRY(cinst_dump),
	};

	cinst_dir = proc_mkdir("cinst", dir);
	if (!cinst_dir) {
		cinst_debug("[%s]: mkdir /proc/cinst failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(cinst_entries); i++) {
		if (!proc_create(cinst_entries[i].name,
			0660,
			cinst_dir,
			cinst_entries[i].fops)) {
			cinst_err(
				"[%s]: create /proc/%s/%s failed\n",
				__func__,
				proc_name,
				cinst_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int cinst_probe(struct platform_device *pdev)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int cinst_n = 0;

	node = pdev->dev.of_node;
	if (!node) {
		cinst_err("get cinst device node err\n");
		return -ENODEV;
	}

	/* ls enable control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_enable", &cinst_doe_ls_enable);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_enable from DTree; rc(%d) cinst_doe_ls_enable(0x%x)\n",
			rc,
			cinst_doe_ls_enable);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_LS_ENABLE,
				(cinst_doe_ls_enable >> cinst_n) & 0x01, cinst_n);
		}
	}
	/* ls credit control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_credit", &cinst_doe_ls_credit);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_credit from DTree; rc(%d) cinst_doe_ls_credit(0x%x)\n",
			rc,
			cinst_doe_ls_credit);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_LS_CREDIT,
				cinst_doe_ls_credit, cinst_n);
		}
	}
	/* ls period control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_period", &cinst_doe_ls_period);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_period from DTree; rc(%d) cinst_doe_ls_period(0x%x)\n",
			rc,
			cinst_doe_ls_period);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_LS_PERIOD,
				cinst_doe_ls_period, cinst_n);
		}
	}

	/* VX enable control */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_enable", &cinst_doe_vx_enable);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_enable from DTree; rc(%d) cinst_doe_vx_enable(0x%x)\n",
			rc,
			cinst_doe_vx_enable);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_VX_ENABLE,
				(cinst_doe_vx_enable >> cinst_n) & 0x01, cinst_n);
		}
	}
	/* ls credit control */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_credit", &cinst_doe_vx_credit);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_credit from DTree; rc(%d) cinst_doe_vx_credit(0x%x)\n",
			rc,
			cinst_doe_vx_credit);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_VX_CREDIT,
				cinst_doe_vx_credit, cinst_n);
		}
	}
	/* ls period control */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_period", &cinst_doe_vx_period);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_period from DTree; rc(%d) cinst_doe_vx_period(0x%x)\n",
			rc,
			cinst_doe_vx_period);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_VX_PERIOD,
				cinst_doe_vx_period, cinst_n);
		}
	}
	/* ls low en control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_low_en", &cinst_doe_ls_low_en);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_low_en from DTree; rc(%d) cinst_doe_ls_low_en(0x%x)\n",
			rc,
			cinst_doe_ls_low_en);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_LS_LOW_EN,
				(cinst_doe_ls_low_en >> cinst_n) & 0x01, cinst_n);
		}
	}
	/* ls low period control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_low_period", &cinst_doe_ls_low_period);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_low_period from DTree; rc(%d) cinst_doe_ls_low_period(0x%x)\n",
			rc,
			cinst_doe_ls_low_period);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_LS_LOW_PERIOD,
				cinst_doe_ls_low_period, cinst_n);
		}
	}
	/* vx low en control */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_low_en", &cinst_doe_vx_low_en);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_low_en from DTree; rc(%d) cinst_doe_vx_low_en(0x%x)\n",
			rc,
			cinst_doe_vx_low_en);
		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_VX_LOW_EN,
				(cinst_doe_vx_low_en >> cinst_n) & 0x01, cinst_n);
		}
	}
	/* vx low period control */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_low_period", &cinst_doe_vx_low_period);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_low_period from DTree; rc(%d) cinst_doe_vx_low_period(0x%x)\n",
			rc,
			cinst_doe_vx_low_period);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_VX_LOW_PERIOD,
				cinst_doe_vx_low_period, cinst_n);
		}
	}
	/* ls const en control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_const_en", &cinst_doe_ls_const_en);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_const_en from DTree; rc(%d) cinst_doe_ls_const_en(0x%x)\n",
			rc,
			cinst_doe_ls_const_en);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_LS_CONST_EN,
				(cinst_doe_ls_const_en >> cinst_n) & 0x01, cinst_n);
		}
	}
	/* vx const en control */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_const_en", &cinst_doe_vx_const_en);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_const_en from DTree; rc(%d) cinst_doe_vx_const_en(0x%x)\n",
			rc,
			cinst_doe_vx_const_en);

		for (cinst_n = CINST_CPU_START_ID; cinst_n <= CINST_CPU_END_ID; cinst_n++) {
			cinst_smc_handle(CINST_GROUP_VX_CONST_EN,
				(cinst_doe_vx_const_en >> cinst_n) & 0x01, cinst_n);
		}
	}

#endif /* CONFIG_OF */

#ifdef CONFIG_OF_RESERVED_MEM
#ifdef PTP3_STATUS_PROBE_DUMP
	/* dump reg status into PICACHU dram for DB */
	if (cinst_buf != NULL) {
		cinst_reserve_memory_dump(
			cinst_buf, cinst_mem_size, CINST_TRIGGER_STAGE_SUSPEND);
	}
#endif /* PTP3_STATUS_PROBE_DUMP */
#endif /* CONFIG_OF_RESERVED_MEM */

#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int cinst_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int cinst_resume(struct platform_device *pdev)
{
	return 0;
}

MODULE_DESCRIPTION("MediaTek CINST Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_CINST_C__
