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

#define __MTK_DRCC_C__

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
	#include "mtk_ptp3_drcc.h"
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
#define DRCC_DEBUG
#define DRCC_TAG	 "[DRCC]"

#define drcc_err(fmt, args...)	\
	pr_info(DRCC_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define drcc_msg(fmt, args...)	\
	pr_info(DRCC_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef DRCC_DEBUG
#define drcc_debug(fmt, args...) \
	pr_debug(DRCC_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define drcc_debug(fmt, args...)
#endif

/************************************************
 * DRCC spinlock
 ************************************************/
static DEFINE_SPINLOCK(drcc_spinlock);
#ifdef TIME_LOG
static long long drcc_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}
#endif

static void mtk_drcc_lock(unsigned long *flags)
{
#ifdef __KERNEL__
	spin_lock_irqsave(&drcc_spinlock, *flags);
	#ifdef TIME_LOG
	drcc_pTime_us = drcc_get_current_time_us();
	#endif
#endif
}

static void mtk_drcc_unlock(unsigned long *flags)
{
#ifdef __KERNEL__
	#ifdef TIME_LOG
	drcc_cTime_us = drcc_get_current_time_us();
	DRCC_IS_TOO_LONG();
	#endif
	spin_unlock_irqrestore(&drcc_spinlock, *flags);
#endif
}


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
/* B-DOE use: drcc func. */
static unsigned int drcc_state;
static unsigned int drcc0_Vref;
static unsigned int drcc1_Vref;
static unsigned int drcc2_Vref;
static unsigned int drcc3_Vref;
static unsigned int drcc4_Vref;
static unsigned int drcc5_Vref;
static unsigned int drcc6_Vref;
static unsigned int drcc7_Vref;
static unsigned int drcc0_Hwgatepct;
static unsigned int drcc1_Hwgatepct;
static unsigned int drcc2_Hwgatepct;
static unsigned int drcc3_Hwgatepct;
static unsigned int drcc4_Hwgatepct;
static unsigned int drcc5_Hwgatepct;
static unsigned int drcc6_Hwgatepct;
static unsigned int drcc7_Hwgatepct;
static unsigned int drcc0_Code;
static unsigned int drcc1_Code;
static unsigned int drcc2_Code;
static unsigned int drcc3_Code;
static unsigned int drcc4_Code;
static unsigned int drcc5_Code;
static unsigned int drcc6_Code;
static unsigned int drcc7_Code;

#endif
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

static char *drcc_buf;
static unsigned long long drcc_mem_size;
void drcc_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	drcc_buf = buf;
	drcc_mem_size = ptp3_mem_size;
}

/* xxxx merged from drcc driver */
int drcc_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum DRCC_TRIGGER_STAGE drcc_tri_stage)
{
	int str_len = 0;
	unsigned int i, value, drcc_n = 0, reg_value;

	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		drcc_msg("unable to get free page!\n");
		return -1;
	}
	drcc_msg("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* collect dump info */
	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
					PTP3_FEATURE_DRCC,
					DRCC_GROUP_READ,
					CPU0_DRCC_A0_CONFIG
						+ ((u64)0x200 * drcc_n),
					0);
		str_len +=
			snprintf(aee_log_buf + str_len,
			(unsigned long long)drcc_mem_size - str_len,
			"CPU(%d)_DRCC_AO_CONFIG reg=0x%llx,\t value=0x%x\n",
			drcc_n,
			CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n),
			reg_value);

		str_len +=
			snprintf(aee_log_buf + str_len,
			(unsigned long long)drcc_mem_size - str_len,
			"CPU(%d), drcc_reg:", drcc_n);

		for (i = 0; i < 4; i++) {
			value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
					PTP3_FEATURE_DRCC,
					DRCC_GROUP_READ,
					CPU0_DRCC_CFG_REG0 +
						(drcc_n * (u64)0x800) + (i * 4),
					0);
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)drcc_mem_size - str_len,
				"\t0x%llx = 0x%x",
				CPU0_DRCC_CFG_REG0 +
				(drcc_n * (u64)0x800) + (i * 4),
				value);
		}

		str_len +=
			snprintf(aee_log_buf + str_len,
			(unsigned long long)drcc_mem_size - str_len,
			"    .%d\n", i);
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	drcc_debug("\n%s", aee_log_buf);
	drcc_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif
#endif

/************************************************
 * SMC between kernel and atf
 ************************************************/
static unsigned int drcc_smc_handle(unsigned int group,
	unsigned int val, unsigned int cpu)
{
	unsigned int ret;

	drcc_msg("[%s]:cpu(%d) group(%d) val(%d)\n",
		__func__, cpu, group, val);

	/* update atf via smc */
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		PTP3_FEATURE_DRCC,
		group,
		val,
		cpu);

	return ret;
}

/************************************************
 * static function
 ************************************************/
static void mtk_drcc_enable(unsigned int onOff,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_ENABLE;
	/* update via atf */
	drcc_smc_handle(drcc_group, onOff, drcc_n);
	/* update via mcupm or cpu_eb */
	// drcc_ipi_handle(drcc_group, onoff, cpu);
}

static void mtk_drcc_cg_count(unsigned int onOff,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_CG_CNT_EN;

	onOff = (onOff) ? 1 : 0;

	drcc_smc_handle(drcc_group, onOff, drcc_n);
}

static void mtk_drcc_cmp_count(unsigned int onOff,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_CMP_CNT_EN;

	onOff = (onOff) ? 1 : 0;

	drcc_smc_handle(drcc_group, onOff, drcc_n);
}

static void mtk_drcc_mode(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_MODE;

	value = (value > 4) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_code(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_CODE;

	value = (value > 63) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_hwgatepct(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_HWGATEOCT;

	value = (value > 7) ? 3 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_vreffilt(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_VREFFILT;

	value = (value > 7) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_autocalibdelay(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_AUTOCALIBDELAY;

	value = (value > 15) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

/************************************************
 * set DRCC status by procfs interface
 ************************************************/
static ssize_t drcc_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable, drcc_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		drcc_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++)
		mtk_drcc_enable((enable >> drcc_n) & 0x01, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_enable_proc_show(struct seq_file *m, void *v)
{
	int status = 0, value, drcc_n = 0;
	const unsigned int drcc_group = DRCC_GROUP_READ;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		value = drcc_smc_handle(drcc_group,
			CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n), drcc_n);
		status = status | ((value & 0x01) << drcc_n);
	}

	seq_printf(m, "%d\n", status);

	return 0;
}

static ssize_t drcc_code_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, drcc_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_code((unsigned int)value, (unsigned int)drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_code_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n), 0);

		seq_printf(m, "drcc_%d, code = %x\n",
			drcc_n,
			(status >> 4) & 0x3F);
	}

	return 0;
}

static ssize_t drcc_hwgatepct_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, drcc_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_hwgatepct((unsigned int)value, (unsigned int)drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_hwgatepct_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n), 0);

		seq_printf(m, "drcc_%d, hwgatepct = %x\n",
			drcc_n,
			(status >> 12) & 0x07);
	}

	return 0;
}

static ssize_t drcc_vreffilt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, drcc_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_vreffilt((unsigned int)value, (unsigned int)drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_vreffilt_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n),
			0);

		seq_printf(m, "drcc_%d, vreffilt = %x\n",
			drcc_n,
			(status >> 16) & 0x07);
	}

	return 0;
}


static ssize_t drcc_cg_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0, drcc_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &enable, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	mtk_drcc_cg_count(enable, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_cg_count_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_CFG_REG0 + (drcc_n * (u64)0x800),
			0);

		seq_printf(m, "drcc_%d count =  %s, %s\n",
			drcc_n,
			(status & 0x40) ? "enable" : "disable",
			(status & 0x80) ? "comparator" : "clock gate");
	}
	return 0;
}

static ssize_t drcc_cmp_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0, drcc_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &enable, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	mtk_drcc_cmp_count(enable, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_cmp_count_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_CFG_REG0 + (drcc_n * (u64)0x800),
			0);

		seq_printf(m, "drcc_%d count =  %s, %s\n",
			drcc_n,
			(status & 0x40) ? "enable" : "disable",
			(status & 0x80) ? "comparator" : "clock gate");
	}
	return 0;
}

static ssize_t drcc_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, drcc_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_mode((unsigned int)value, (unsigned int)drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_mode_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_CFG_REG0 + (drcc_n * (u64)0x800),
			0);

		seq_printf(m, "drcc_%d mode = %x\n",
			drcc_n,
			(status >> 12) & 0x07);
	}
	return 0;
}

static ssize_t drcc_autocalibdelay_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, drcc_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &drcc_n) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_autocalibdelay((unsigned int)value, (unsigned int)drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_autocalibdelay_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(
			DRCC_GROUP_READ,
			CPU0_DRCC_CFG_REG0 + ((u64)0x200 * drcc_n),
			0);

		seq_printf(m, "drcc_%d, autocalibdelay = %x\n",
			drcc_n,
			(status >> 20) & 0x0F);
	}

	return 0;
}

static int drcc_reg_dump_proc_show(struct seq_file *m, void *v)
{
	unsigned long flags;
	unsigned int i, value[DRCC_NUM][4], drcc_n = 0;
	const unsigned int drcc_group = DRCC_GROUP_READ;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		mtk_drcc_lock(&flags);
		seq_printf(m, "CPU(%d)_DRCC_AO_CONFIG reg=0x%llx,\t value=0x%x\n",
			drcc_n,
			CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n),
			drcc_smc_handle(
				drcc_group,
				CPU0_DRCC_A0_CONFIG + ((u64)0x200 * drcc_n),
				drcc_n));

		for (i = 0; i < 4; i++)
			value[drcc_n][i] = drcc_smc_handle(
				drcc_group,
				CPU0_DRCC_CFG_REG0 + (drcc_n * (u64)0x800)
				 + (i * 4),
				drcc_n);
		mtk_drcc_unlock(&flags);

		seq_printf(m, "CPU(%d), drcc_reg :", drcc_n);
		for (i = 0; i < 4; i++)
			seq_printf(m, "\t0x%llx = 0x%x",
				CPU0_DRCC_CFG_REG0 + (drcc_n * (u64)0x800)
				 + (i * 4),
				value[drcc_n][i]);
		seq_printf(m, "    .%d\n", i);
	}
	return 0;
}

PROC_FOPS_RW(drcc_enable);
PROC_FOPS_RO(drcc_reg_dump);
PROC_FOPS_RW(drcc_cg_count);
PROC_FOPS_RW(drcc_cmp_count);
PROC_FOPS_RW(drcc_mode);
PROC_FOPS_RW(drcc_code);
PROC_FOPS_RW(drcc_hwgatepct);
PROC_FOPS_RW(drcc_vreffilt);
PROC_FOPS_RW(drcc_autocalibdelay);

int drcc_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry drcc_entries[] = {
		PROC_ENTRY(drcc_enable),
		PROC_ENTRY(drcc_cg_count),
		PROC_ENTRY(drcc_cmp_count),
		PROC_ENTRY(drcc_mode),
		PROC_ENTRY(drcc_code),
		PROC_ENTRY(drcc_hwgatepct),
		PROC_ENTRY(drcc_vreffilt),
		PROC_ENTRY(drcc_autocalibdelay),
		PROC_ENTRY(drcc_reg_dump),
	};

	for (i = 0; i < ARRAY_SIZE(drcc_entries); i++) {
		if (!proc_create(drcc_entries[i].name,
			0660,
			dir,
			drcc_entries[i].fops)) {
			drcc_err("[%s]: create /proc/%s/%s failed\n",
				__func__,
				proc_name,
				drcc_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int drcc_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int drcc_n = 0;

	node = pdev->dev.of_node;
	if (!node) {
		drcc_err("get drcc device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "drcc_state", &drcc_state);
	if (!rc) {
		drcc_debug("[xxxxdrcc] state from DTree; rc(%d) drcc_state(0x%x)\n",
			rc,
			drcc_state);

		for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++)
			mtk_drcc_enable((drcc_state >> drcc_n) & 0x01, drcc_n);
	}

	rc = of_property_read_u32(node, "drcc0_Vref", &drcc0_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc0_Vref from DTree; rc(%d) drcc0_Vref(0x%x)\n",
			rc,
			drcc0_Vref);

		if (drcc0_Vref <= 7)
			mtk_drcc_vreffilt(drcc0_Vref, 0);
	}

	rc = of_property_read_u32(node, "drcc1_Vref", &drcc1_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc1_Vref from DTree; rc(%d) drcc1_Vref(0x%x)\n",
			rc,
			drcc1_Vref);

		if (drcc1_Vref <= 7)
			mtk_drcc_vreffilt(drcc1_Vref, 1);
	}

	rc = of_property_read_u32(node, "drcc2_Vref", &drcc2_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc2_Vref from DTree; rc(%d) drcc2_Vref(0x%x)\n",
			rc,
			drcc2_Vref);

		if (drcc2_Vref <= 7)
			mtk_drcc_vreffilt(drcc2_Vref, 2);
	}

	rc = of_property_read_u32(node, "drcc3_Vref", &drcc3_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc3_Vref from DTree; rc(%d) drcc3_Vref(0x%x)\n",
			rc,
			drcc3_Vref);

		if (drcc3_Vref <= 7)
			mtk_drcc_vreffilt(drcc3_Vref, 3);
	}

	rc = of_property_read_u32(node, "drcc4_Vref", &drcc4_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc4_Vref from DTree; rc(%d) drcc4_Vref(0x%x)\n",
			rc,
			drcc4_Vref);

		if (drcc4_Vref <= 7)
			mtk_drcc_vreffilt(drcc4_Vref, 4);
	}

	rc = of_property_read_u32(node, "drcc5_Vref", &drcc5_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc5_Vref from DTree; rc(%d) drcc5_Vref(0x%x)\n",
			rc,
			drcc5_Vref);

		if (drcc5_Vref <= 7)
			mtk_drcc_vreffilt(drcc5_Vref, 5);
	}

	rc = of_property_read_u32(node, "drcc6_Vref", &drcc6_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc6_Vref from DTree; rc(%d) drcc6_Vref(0x%x)\n",
			rc,
			drcc6_Vref);

		if (drcc6_Vref <= 7)
			mtk_drcc_vreffilt(drcc6_Vref, 6);
	}

	rc = of_property_read_u32(node, "drcc7_Vref", &drcc7_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc7_Vref from DTree; rc(%d) drcc7_Vref(0x%x)\n",
			rc,
			drcc7_Vref);

		if (drcc7_Vref <= 7)
			mtk_drcc_vreffilt(drcc7_Vref, 7);
	}

	rc = of_property_read_u32(node, "drcc0_Hwgatepct", &drcc0_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc0_Hwgatepct from DTree; rc(%d) drcc0_Hwgatepct(0x%x)\n",
			rc,
			drcc0_Hwgatepct);

		if (drcc0_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc0_Hwgatepct, 0);
	}

	rc = of_property_read_u32(node, "drcc1_Hwgatepct", &drcc1_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc1_Hwgatepct from DTree; rc(%d) drcc1_Hwgatepct(0x%x)\n",
			rc,
			drcc1_Hwgatepct);

		if (drcc1_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc1_Hwgatepct, 1);
	}

	rc = of_property_read_u32(node, "drcc2_Hwgatepct", &drcc2_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc2_Hwgatepct from DTree; rc(%d) drcc2_Hwgatepct(0x%x)\n",
			rc,
			drcc2_Hwgatepct);

		if (drcc2_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc2_Hwgatepct, 2);
	}

	rc = of_property_read_u32(node, "drcc3_Hwgatepct", &drcc3_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc3_Hwgatepct from DTree; rc(%d) drcc3_Hwgatepct(0x%x)\n",
			rc,
			drcc3_Hwgatepct);

		if (drcc3_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc3_Hwgatepct, 3);
	}

	rc = of_property_read_u32(node, "drcc4_Hwgatepct", &drcc4_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc4_Hwgatepct from DTree; rc(%d) drcc4_Hwgatepct(0x%x)\n",
			rc,
			drcc4_Hwgatepct);

		if (drcc4_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc4_Hwgatepct, 4);
	}

	rc = of_property_read_u32(node, "drcc5_Hwgatepct", &drcc5_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc5_Hwgatepct from DTree; rc(%d) drcc5_Hwgatepct(0x%x)\n",
			rc,
			drcc5_Hwgatepct);

		if (drcc5_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc5_Hwgatepct, 5);
	}

	rc = of_property_read_u32(node, "drcc6_Hwgatepct", &drcc6_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc6_Hwgatepct from DTree; rc(%d) drcc6_Hwgatepct(0x%x)\n",
			rc,
			drcc6_Hwgatepct);

		if (drcc6_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc6_Hwgatepct, 6);
	}

	rc = of_property_read_u32(node, "drcc7_Hwgatepct", &drcc7_Hwgatepct);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc7_Hwgatepct from DTree; rc(%d) drcc7_Hwgatepct(0x%x)\n",
			rc,
			drcc7_Hwgatepct);

		if (drcc7_Hwgatepct <= 7)
			mtk_drcc_hwgatepct(drcc7_Hwgatepct, 7);
	}

	rc = of_property_read_u32(node, "drcc0_Code", &drcc0_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc0_Code from DTree; rc(%d) drcc0_Code(0x%x)\n",
			rc,
			drcc0_Code);

		if (drcc0_Code <= 63)
			mtk_drcc_code(drcc0_Code, 0);
	}

	rc = of_property_read_u32(node, "drcc1_Code", &drcc1_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc1_Code from DTree; rc(%d) drcc1_Code(0x%x)\n",
			rc,
			drcc1_Code);

		if (drcc1_Code <= 63)
			mtk_drcc_code(drcc1_Code, 1);
	}

	rc = of_property_read_u32(node, "drcc2_Code", &drcc2_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc2_Code from DTree; rc(%d) drcc2_Code(0x%x)\n",
			rc,
			drcc2_Code);

		if (drcc2_Code <= 63)
			mtk_drcc_code(drcc2_Code, 2);
	}

	rc = of_property_read_u32(node, "drcc3_Code", &drcc3_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc3_Code from DTree; rc(%d) drcc3_Code(0x%x)\n",
			rc,
			drcc3_Code);

		if (drcc3_Code <= 63)
			mtk_drcc_code(drcc3_Code, 3);
	}

	rc = of_property_read_u32(node, "drcc4_Code", &drcc4_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc4_Code from DTree; rc(%d) drcc4_Code(0x%x)\n",
			rc,
			drcc4_Code);

		if (drcc4_Code <= 63)
			mtk_drcc_code(drcc4_Code, 4);
	}

	rc = of_property_read_u32(node, "drcc5_Code", &drcc5_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc5_Code from DTree; rc(%d) drcc5_Code(0x%x)\n",
			rc,
			drcc5_Code);

		if (drcc5_Code <= 63)
			mtk_drcc_code(drcc5_Code, 5);
	}

	rc = of_property_read_u32(node, "drcc6_Code", &drcc6_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc6_Code from DTree; rc(%d) drcc6_Code(0x%x)\n",
			rc,
			drcc6_Code);

		if (drcc6_Code <= 63)
			mtk_drcc_code(drcc6_Code, 6);
	}

	rc = of_property_read_u32(node, "drcc7_Code", &drcc7_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc7_Code from DTree; rc(%d) drcc7_Code(0x%x)\n",
			rc,
			drcc7_Code);

		if (drcc7_Code <= 63)
			mtk_drcc_code(drcc7_Code, 7);
	}
	/* dump reg status into PICACHU dram for DB */
	if (drcc_buf != NULL) {
		drcc_reserve_memory_dump(drcc_buf, drcc_mem_size,
			DRCC_TRIGGER_STAGE_PROBE);
	}

	drcc_msg("drcc probe ok!!\n");
#endif
#endif
	return 0;
}

int drcc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int drcc_resume(struct platform_device *pdev)
{
	return 0;
}

MODULE_DESCRIPTION("MediaTek DRCC Driver v1p1");
MODULE_LICENSE("GPL");

#undef __MTK_DRCC_C__
