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
static unsigned int drcc0_Code;
static unsigned int drcc1_Code;
static unsigned int drcc2_Code;
static unsigned int drcc3_Code;
static unsigned int drcc4_Code;
static unsigned int drcc5_Code;
static unsigned int drcc6_Code;
static unsigned int drcc7_Code;
static unsigned int drcc4_EdgeSel;
static unsigned int drcc5_EdgeSel;
static unsigned int drcc6_EdgeSel;
static unsigned int drcc7_EdgeSel;

#endif
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

#ifdef PTP3_STATUS_PROBE_DUMP
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
	struct drcc_b_class drcc_b;
	struct drcc_l_class drcc_l;
	unsigned int str_len = 0, drcc_n = 0, i = 0;
	unsigned int *value_b = (unsigned int *)&drcc_b;
	unsigned int *value_l = (unsigned int *)&drcc_l;

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
		if (drcc_n < DRCC_L_NUM) {
			*value_l = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_DRCC, DRCC_GROUP_READ,
				DRCC_L_AOREG_OFFSET, drcc_n);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"[DRCC][CPU%d] ", drcc_n);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"drcc_enable:0x%x, ", drcc_l.drcc_enable);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"drcc_code:0x%x, ", drcc_l.drcc_code);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"drcc_hwgatepct:0x%x, ", drcc_l.drcc_hwgatepct);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"drcc_verffilt:0x%x, ", drcc_l.drcc_verffilt);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"drcc_autocalibdelay:0x%x\n", drcc_l.drcc_autocalibdelay);
		} else {
			for (i = 0; i < DRCC_REG_CNT ; i++) {
				value_b[i] = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
					PTP3_FEATURE_DRCC, DRCC_GROUP_READ,
					DRFC_V101_OFFSET + (i * 4), drcc_n);
			}
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"[DRCC][CPU%d] ", drcc_n);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"SamplerEn:0x%x, ", drcc_b.drcc_SamplerEn);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccGateEn:0x%x, ", drcc_b.drcc_DrccGateEn);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"ConfigComplete:0x%x, ", drcc_b.drcc_ConfigComplete);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccCode:0x%x, ", drcc_b.drcc_DrccCode);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccVrefFilt:0x%x, ", drcc_b.drcc_DrccVrefFilt);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccClockEdgeSel:0x%x, ", drcc_b.drcc_DrccClockEdgeSel);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccForceTrim:0x%x, ", drcc_b.drcc_DrccForceTrim);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"ForceTrimEn:0x%x, ", drcc_b.drcc_ForceTrimEn);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"AutoCalibDelay:0x%x, ", drcc_b.drcc_AutoCalibDelay);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccCMP:0x%x, ", drcc_b.drcc_DrccCMP);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccAutoCalibDone:0x%x, ", drcc_b.drcc_DrccAutoCalibDone);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccAutoCalibError:0x%x, ", drcc_b.drcc_DrccAutoCalibError);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"DrccAutoCalibTrim:0x%x, ", drcc_b.drcc_DrccAutoCalibTrim);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventSource0:0x%x, ", drcc_b.drcc_EventSource0);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventType0:0x%x, ", drcc_b.drcc_EventType0);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventSource1:0x%x, ", drcc_b.drcc_EventSource1);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventType1:0x%x, ", drcc_b.drcc_EventType1);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventFreeze:0x%x, ", drcc_b.drcc_EventFreeze);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventCount0:0x%x, ", drcc_b.drcc_EventCount0);
			str_len += snprintf(aee_log_buf + str_len,
				drcc_mem_size - str_len,
				"EventCount1:0x%x,\n", drcc_b.drcc_EventCount1);
		}
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len + 1);

	drcc_debug("\n%s", aee_log_buf);
	drcc_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}
#endif

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
static void mtk_drcc_DrccEnable(unsigned int onOff,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_DrccEnable;
	/* update via atf */
	drcc_smc_handle(drcc_group, onOff, drcc_n);
	/* update via mcupm or cpu_eb */
	// drcc_ipi_handle(drcc_group, onoff, cpu);
}

static void mtk_drcc_Hwgatepct(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_Hwgatepct;

	value = (value > 7) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_DrccCode(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_DrccCode;

	value = (value > 63) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_DrccVrefFilt(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_DrccVrefFilt;

	value = (value > 7) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_DrccClockEdgeSel(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_DrccClockEdgeSel;

	value = (value > 1) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_DrccForceTrim(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_DrccForceTrim;

	value = (value > 127) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_ForceTrimEn(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_ForceTrimEn;

	value = (value > 127) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_AutoCalibDelay(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_AutoCalibDelay;

	value = (value > 15) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_EventSource0(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_EventSource0;

	value = (value > 7) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_EventType0(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_EventType0;

	value = (value > 3) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_EventSource1(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_EventSource1;

	value = (value > 7) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_EventType1(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_EventType1;

	value = (value > 3) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}

static void mtk_drcc_EventFreeze(unsigned int value,
		unsigned int drcc_n)
{
	const unsigned int drcc_group = DRCC_GROUP_EventFreeze;

	value = (value > 1) ? 0 : value;

	drcc_smc_handle(drcc_group, value, drcc_n);
}
/************************************************
 * set DRCC status by procfs interface
 ************************************************/
static ssize_t DrccEnable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, drcc_n;
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
		mtk_drcc_DrccEnable((enable >> drcc_n) & 0x01, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int DrccEnable_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, drcc_n = 0;

	value = drcc_smc_handle(DRCC_GROUP_CFG, 0, drcc_n);

	seq_printf(m, "[DRCC]%x\n", value);

	return 0;
}

static ssize_t Hwgatepct_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_Hwgatepct(value, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int Hwgatepct_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_Hwgatepct_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], Hwgatepct:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t DrccCode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_DrccCode(value, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int DrccCode_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_DrccCode_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], code:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t DrccVrefFilt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_DrccVrefFilt(value, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int DrccVrefFilt_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_DrccVrefFilt_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], DrccVrefFilt:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t DrccClockEdgeSel_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_DrccClockEdgeSel(value, drcc_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int DrccClockEdgeSel_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_DrccClockEdgeSel_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], DrccClockEdgeSel:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t DrccForceTrim_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_DrccForceTrim(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int DrccForceTrim_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_DrccForceTrim_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], DrccForceTrim:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t ForceTrimEn_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_ForceTrimEn(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int ForceTrimEn_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_ForceTrimEn_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], ForceTrimEn:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t AutoCalibDelay_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_AutoCalibDelay(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int AutoCalibDelay_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_AutoCalibDelay_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], AutoCalibDelay:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t EventSource0_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_EventSource0(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int EventSource0_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_EventSource0_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], EventSource0:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t EventType0_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_EventType0(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int EventType0_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_EventType0_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], EventType0:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t EventSource1_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_EventSource1(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int EventSource1_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_EventSource1_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], EventSource1:%x\n",
			drcc_n, status);
	}

	return 0;
}


static ssize_t EventType1_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_EventType1(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int EventType1_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_EventType1_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], EventType1:%x\n",
			drcc_n, status);
	}

	return 0;
}

static ssize_t EventFreeze_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, drcc_n = 0;
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

	mtk_drcc_EventFreeze(value, drcc_n); //xxxx

out:
	free_page((unsigned long)buf);
	return count;
}

static int EventFreeze_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		status = drcc_smc_handle(DRCC_GROUP_EventFreeze_R, 0, drcc_n);

		seq_printf(m, "[DRCC][CPU%d], EventFreeze:%x\n",
			drcc_n, status);
	}

	return 0;
}

static int EventCount_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, drcc_n = 0;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		value = drcc_smc_handle(DRCC_GROUP_EventCount0_R, 0, drcc_n);
		seq_printf(m, "[DRCC][CPU%d][EventCount0] 0x%x\n",
			drcc_n, value);
		value = drcc_smc_handle(DRCC_GROUP_EventCount1_R, 0, drcc_n);
		seq_printf(m, "[DRCC][CPU%d][EventCount1] 0x%x\n",
			drcc_n, value);
	}

	return 0;
}

static int drcc_cfg_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;

	value = drcc_smc_handle(DRCC_GROUP_CFG, 0, 0);

	seq_printf(m, "%08x\n", value);

	return 0;
}

static int drcc_dump_proc_show(struct seq_file *m, void *v)
{
	struct drcc_b_class drcc_b;
	struct drcc_l_class drcc_l;
	unsigned int *value_b = (unsigned int *)&drcc_b;
	unsigned int *value_l = (unsigned int *)&drcc_l;
	unsigned int i, drcc_n = 0;
	const unsigned int drcc_group = DRCC_GROUP_READ;

	for (drcc_n = 0; drcc_n < DRCC_NUM; drcc_n++) {
		if (drcc_n < DRCC_L_NUM) {
			*value_l = drcc_smc_handle(drcc_group, DRCC_L_AOREG_OFFSET,
					drcc_n);
				seq_printf(m, "[DRCC][CPU%d] ", drcc_n);
				seq_printf(m, "drcc_enable:0x%x, ",
					drcc_l.drcc_enable);
				seq_printf(m, "drcc_code:0x%x, ",
					drcc_l.drcc_code);
				seq_printf(m, "drcc_hwgatepct:0x%x, ",
					drcc_l.drcc_hwgatepct);
				seq_printf(m, "drcc_verffilt:0x%x, ",
					drcc_l.drcc_verffilt);
				seq_printf(m, "drcc_autocalibdelay:0x%x\n",
					drcc_l.drcc_autocalibdelay);
		} else {
			// DRFC_V101~V106
			for (i = 0; i < DRCC_REG_CNT; i++)
				value_b[i] = drcc_smc_handle(drcc_group,
							DRFC_V101_OFFSET + (i * 4),
							drcc_n);
			seq_printf(m, "[DRCC][CPU%d] ", drcc_n);
			seq_printf(m, "SamplerEn:0x%x, ",
				drcc_b.drcc_SamplerEn);
			seq_printf(m, "DrccGateEn:0x%x, ",
				drcc_b.drcc_DrccGateEn);
			seq_printf(m, "ConfigComplete:0x%x, ",
				drcc_b.drcc_ConfigComplete);
			seq_printf(m, "DrccCode:0x%x, ",
				drcc_b.drcc_DrccCode);
			seq_printf(m, "DrccVrefFilt:0x%x, ",
				drcc_b.drcc_DrccVrefFilt);
			seq_printf(m, "DrccClockEdgeSel:0x%x, ",
				drcc_b.drcc_DrccClockEdgeSel);
			seq_printf(m, "DrccForceTrim:0x%x, ",
				drcc_b.drcc_DrccForceTrim);
			seq_printf(m, "ForceTrimEn:0x%x, ",
				drcc_b.drcc_ForceTrimEn);
			seq_printf(m, "AutoCalibDelay:0x%x, ",
				drcc_b.drcc_AutoCalibDelay);
			seq_printf(m, "DrccCMP:0x%x, ",
				drcc_b.drcc_DrccCMP);
			seq_printf(m, "DrccAutoCalibDone:0x%x, ",
				drcc_b.drcc_DrccAutoCalibDone);
			seq_printf(m, "DrccAutoCalibError:0x%x, ",
				drcc_b.drcc_DrccAutoCalibError);
			seq_printf(m, "DrccAutoCalibTrim:0x%x, ",
				drcc_b.drcc_DrccAutoCalibTrim);
			seq_printf(m, "EventSource0:0x%x, ",
				drcc_b.drcc_EventSource0);
			seq_printf(m, "EventType0:0x%x, ",
				drcc_b.drcc_EventType0);
			seq_printf(m, "EventSource1:0x%x, ",
				drcc_b.drcc_EventSource1);
			seq_printf(m, "EventType1:0x%x, ",
				drcc_b.drcc_EventType1);
			seq_printf(m, "EventFreeze:0x%x, ",
				drcc_b.drcc_EventFreeze);
			seq_printf(m, "EventCount0:0x%x, ",
				drcc_b.drcc_EventCount0);
			seq_printf(m, "EventCount1:0x%x,\n",
				drcc_b.drcc_EventCount1);
		}
	}
	return 0;
}

PROC_FOPS_RW(DrccEnable);
PROC_FOPS_RW(Hwgatepct);
PROC_FOPS_RW(DrccCode);
PROC_FOPS_RW(DrccVrefFilt);
PROC_FOPS_RW(DrccClockEdgeSel);
PROC_FOPS_RW(DrccForceTrim);
PROC_FOPS_RW(ForceTrimEn);
PROC_FOPS_RW(AutoCalibDelay);
PROC_FOPS_RW(EventSource0);
PROC_FOPS_RW(EventType0);
PROC_FOPS_RW(EventSource1);
PROC_FOPS_RW(EventType1);
PROC_FOPS_RW(EventFreeze);
PROC_FOPS_RO(EventCount);
PROC_FOPS_RO(drcc_dump);
PROC_FOPS_RO(drcc_cfg);

int drcc_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *drcc_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry drcc_entries[] = {
		PROC_ENTRY(DrccEnable),
		PROC_ENTRY(Hwgatepct),
		PROC_ENTRY(DrccCode),
		PROC_ENTRY(DrccVrefFilt),
		PROC_ENTRY(DrccClockEdgeSel),
		PROC_ENTRY(DrccForceTrim),
		PROC_ENTRY(ForceTrimEn),
		PROC_ENTRY(AutoCalibDelay),
		PROC_ENTRY(EventSource0),
		PROC_ENTRY(EventType0),
		PROC_ENTRY(EventSource1),
		PROC_ENTRY(EventType1),
		PROC_ENTRY(EventFreeze),
		PROC_ENTRY(EventCount),
		PROC_ENTRY(drcc_dump),
		PROC_ENTRY(drcc_cfg),
	};

	drcc_dir = proc_mkdir("drcc", dir);
	if (!drcc_dir) {
		drcc_debug("[%s]: mkdir /proc/drcc failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(drcc_entries); i++) {
		if (!proc_create(drcc_entries[i].name,
			0660,
			drcc_dir,
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
			mtk_drcc_DrccEnable((drcc_state >> drcc_n) & 0x01, drcc_n);
	}

	rc = of_property_read_u32(node, "drcc0_Vref", &drcc0_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc0_Vref from DTree; rc(%d) drcc0_Vref(0x%x)\n",
			rc,
			drcc0_Vref);

		if (drcc0_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc0_Vref, 0);
	}

	rc = of_property_read_u32(node, "drcc1_Vref", &drcc1_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc1_Vref from DTree; rc(%d) drcc1_Vref(0x%x)\n",
			rc,
			drcc1_Vref);

		if (drcc1_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc1_Vref, 1);
	}

	rc = of_property_read_u32(node, "drcc2_Vref", &drcc2_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc2_Vref from DTree; rc(%d) drcc2_Vref(0x%x)\n",
			rc,
			drcc2_Vref);

		if (drcc2_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc2_Vref, 2);
	}

	rc = of_property_read_u32(node, "drcc3_Vref", &drcc3_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc3_Vref from DTree; rc(%d) drcc3_Vref(0x%x)\n",
			rc,
			drcc3_Vref);

		if (drcc3_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc3_Vref, 3);
	}

	rc = of_property_read_u32(node, "drcc4_Vref", &drcc4_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc4_Vref from DTree; rc(%d) drcc4_Vref(0x%x)\n",
			rc,
			drcc4_Vref);

		if (drcc4_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc4_Vref, 4);
	}

	rc = of_property_read_u32(node, "drcc5_Vref", &drcc5_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc5_Vref from DTree; rc(%d) drcc5_Vref(0x%x)\n",
			rc,
			drcc5_Vref);

		if (drcc5_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc5_Vref, 5);
	}

	rc = of_property_read_u32(node, "drcc6_Vref", &drcc6_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc6_Vref from DTree; rc(%d) drcc6_Vref(0x%x)\n",
			rc,
			drcc6_Vref);

		if (drcc6_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc6_Vref, 6);
	}

	rc = of_property_read_u32(node, "drcc7_Vref", &drcc7_Vref);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc7_Vref from DTree; rc(%d) drcc7_Vref(0x%x)\n",
			rc,
			drcc7_Vref);

		if (drcc7_Vref <= 7)
			mtk_drcc_DrccVrefFilt(drcc7_Vref, 7);
	}

	rc = of_property_read_u32(node, "drcc0_Code", &drcc0_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc0_Code from DTree; rc(%d) drcc0_Code(0x%x)\n",
			rc,
			drcc0_Code);

		if (drcc0_Code <= 63)
			mtk_drcc_DrccCode(drcc0_Code, 0);
	}

	rc = of_property_read_u32(node, "drcc1_Code", &drcc1_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc1_Code from DTree; rc(%d) drcc1_Code(0x%x)\n",
			rc,
			drcc1_Code);

		if (drcc1_Code <= 63)
			mtk_drcc_DrccCode(drcc1_Code, 1);
	}

	rc = of_property_read_u32(node, "drcc2_Code", &drcc2_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc2_Code from DTree; rc(%d) drcc2_Code(0x%x)\n",
			rc,
			drcc2_Code);

		if (drcc2_Code <= 63)
			mtk_drcc_DrccCode(drcc2_Code, 2);
	}

	rc = of_property_read_u32(node, "drcc3_Code", &drcc3_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc3_Code from DTree; rc(%d) drcc3_Code(0x%x)\n",
			rc,
			drcc3_Code);

		if (drcc3_Code <= 63)
			mtk_drcc_DrccCode(drcc3_Code, 3);
	}

	rc = of_property_read_u32(node, "drcc4_Code", &drcc4_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc4_Code from DTree; rc(%d) drcc4_Code(0x%x)\n",
			rc,
			drcc4_Code);

		if (drcc4_Code <= 63)
			mtk_drcc_DrccCode(drcc4_Code, 4);
	}

	rc = of_property_read_u32(node, "drcc5_Code", &drcc5_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc5_Code from DTree; rc(%d) drcc5_Code(0x%x)\n",
			rc,
			drcc5_Code);

		if (drcc5_Code <= 63)
			mtk_drcc_DrccCode(drcc5_Code, 5);
	}

	rc = of_property_read_u32(node, "drcc6_Code", &drcc6_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc6_Code from DTree; rc(%d) drcc6_Code(0x%x)\n",
			rc,
			drcc6_Code);

		if (drcc6_Code <= 63)
			mtk_drcc_DrccCode(drcc6_Code, 6);
	}

	rc = of_property_read_u32(node, "drcc7_Code", &drcc7_Code);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc7_Code from DTree; rc(%d) drcc7_Code(0x%x)\n",
			rc,
			drcc7_Code);

		if (drcc7_Code <= 63)
			mtk_drcc_DrccCode(drcc7_Code, 7);
	}

	rc = of_property_read_u32(node, "drcc4_EdgeSel", &drcc4_EdgeSel);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc4_EdgeSel from DTree; rc(%d) drcc4_EdgeSel(0x%x)\n",
			rc,
			drcc4_EdgeSel);

		if (drcc4_EdgeSel <= 1)
			mtk_drcc_DrccClockEdgeSel(drcc4_EdgeSel, 4);
	}

	rc = of_property_read_u32(node, "drcc5_EdgeSel", &drcc5_EdgeSel);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc5_EdgeSel from DTree; rc(%d) drcc5_EdgeSel(0x%x)\n",
			rc,
			drcc5_EdgeSel);

		if (drcc5_EdgeSel <= 1)
			mtk_drcc_DrccClockEdgeSel(drcc5_EdgeSel, 5);
	}

	rc = of_property_read_u32(node, "drcc6_EdgeSel", &drcc6_EdgeSel);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc6_EdgeSel from DTree; rc(%d) drcc6_EdgeSel(0x%x)\n",
			rc,
			drcc6_EdgeSel);

		if (drcc6_EdgeSel <= 1)
			mtk_drcc_DrccClockEdgeSel(drcc6_EdgeSel, 6);
	}

	rc = of_property_read_u32(node, "drcc7_EdgeSel", &drcc7_EdgeSel);
	if (!rc) {
		drcc_debug("[xxxxdrcc] drcc7_EdgeSel from DTree; rc(%d) drcc7_EdgeSel(0x%x)\n",
			rc,
			drcc7_EdgeSel);

		if (drcc7_EdgeSel <= 1)
			mtk_drcc_DrccClockEdgeSel(drcc7_EdgeSel, 7);
	}
#endif /* CONFIG_OF */

#ifdef CONFIG_OF_RESERVED_MEM
#ifdef PTP3_STATUS_PROBE_DUMP
	/* dump reg status into PICACHU dram for DB */
	if (drcc_buf != NULL) {
		drcc_reserve_memory_dump(drcc_buf, drcc_mem_size,
			DRCC_TRIGGER_STAGE_PROBE);
	}
#endif /* PTP3_STATUS_PROBE_DUMP */
#endif /* CONFIG_OF_RESERVED_MEM */

#endif /* CONFIG_FPGA_EARLY_PORTING */
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
