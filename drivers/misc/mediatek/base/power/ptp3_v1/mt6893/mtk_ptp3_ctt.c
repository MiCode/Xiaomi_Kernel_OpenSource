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
 * @file	mkt_ptp3_ctt.c
 * @brief   Driver for ctt
 *
 */

#define __MTK_CTT_C__

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
	#include "mtk_ptp3_ctt.h"
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
#define CTT_TAG	 "[xxxxCTT]"

#define ctt_info(fmt, args...)	\
	pr_info(CTT_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#define ctt_debug(fmt, args...) \
	pr_debug(CTT_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)

/************************************************
 * CTT spinlock
 ************************************************/
#define LOG_INTERVAL    (1LL * NSEC_PER_SEC)

#ifdef TIME_LOG
/* Get time stmp to known the time period */
static unsigned long long ctt_pTime_us, ctt_cTime_us, ctt_diff_us;
#ifdef __KERNEL__
#define TIME_TH_US 3000
#define CTT_IS_TOO_LONG()      \
	do {                    \
		ctt_diff_us = ctt_cTime_us - ctt_pTime_us;           \
		if (ctt_diff_us > TIME_TH_US) {                        \
			pr_debug(CTT_TAG "caller_addr %p: %llu us\n",  \
			__builtin_return_address(0), ctt_diff_us);     \
		} else if (ctt_diff_us < 0) {                          \
			pr_debug(CTT_TAG "E: misuse caller_addr %p\n", \
			__builtin_return_address(0));                   \
		}
	} while (0)
#endif
#endif

/************************************************
 * static Variable
 ************************************************/

/************************************************
 * SMC between kernel and atf
 ************************************************/
static unsigned int ctt_smc_handle(unsigned int key,
	unsigned int val, unsigned int cpu)
{
	unsigned int ret;

	ctt_info("[%s]:key(%d) val(%d) cpu(%d)\n",
		__func__, key, val, cpu);

	/* update atf via smc */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_CTT,
		key,
		val,
		cpu);

	return ret;
}

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

#ifdef PTP3_STATUS_PROBE_DUMP
static char *ctt_buf;
static unsigned long long ctt_mem_size;
void ctt_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	ctt_buf = buf;
	ctt_mem_size = ptp3_mem_size;
}

int ctt_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum CTT_TRIGGER_STAGE ctt_tri_stage)
{
	int str_len = 0;
	struct ctt_class ctt;
	unsigned int *value = (unsigned int *)&ctt;
	unsigned int i, ctt_n = 0;
	const unsigned int ctt_key = CTT_R;

	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		ctt_info("unable to get free page!\n");
		return -1;
	}
	ctt_info("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* show trigger stage */
	switch (ctt_tri_stage) {
	case CTT_TRIGGER_STAGE_PROBE:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Probe]\n");
		break;
	case CTT_TRIGGER_STAGE_SUSPEND:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Suspend]\n");
		break;
	case CTT_TRIGGER_STAGE_RESUME:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Resume]\n");
		break;
	default:
		ctt_debug("illegal CTT_TRIGGER_STAGE\n");
		break;
	}
	/* collect dump info */
	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		value[0] = ctt_smc_handle(ctt_key, CTT_V101, ctt_n);
		for (i = 1; i < 11; i++) {
			value[i] = ctt_smc_handle(ctt_key,
						CTT_V127 + (i * 4),
						ctt_n);
		}

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"[CTT][CPU%d]", ctt_n + 4);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" SafeFreqReqOverride:%d,",
			ctt.safeFreqReqOverride);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" GlobalEventEn:%d,",
			ctt.globalEventEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" TestMode:%d,",
			ctt.testMode);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttEn:%d,",
			ctt.cttEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" FllSlowReqGateEn:%d,",
			ctt.fllSlowReqGateEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" FllSlowReqEn:%d,",
			ctt.fllSlowReqEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" FllClkOutSelect:%d,",
			ctt.fllClkOutSelect);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" FllEn:%d,",
			ctt.fllEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" ConfigComplete:%d,",
			ctt.configComplete);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" DrccGateEn:%d,",
			ctt.drccGateEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" SamplerEn:%d,",
			ctt.samplerEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTmax:%d,",
			ctt.CttTmax);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttImax:%d,",
			ctt.CttImax);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttVoltage:%d,",
			ctt.CttVoltage);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttDynScale:%d,",
			ctt.CttDynScale);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttLkgScale:%d,",
			ctt.CttLkgScale);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttVcoef:%d,",
			ctt.CttVcoef);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTcoef2:%d,",
			ctt.CttTcoef2);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTcoef1:%d,",
			ctt.CttTcoef1);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttLkgIrefTrim:%d,",
			ctt.CttLkgIrefTrim);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTcal:%d,",
			ctt.CttTcal);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttVcal:%d,",
			ctt.CttVcal);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttLcal:%d,",
			ctt.CttLcal);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttFllNR:%d,",
			ctt.CttFllNR);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttMpmmEn:%d,",
			ctt.CttMpmmEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttImaxDisableEn:%d,",
			ctt.CttImaxDisableEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttIRQStatus:%d,",
			ctt.CttIRQStatus);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttIRQClear:%d,",
			ctt.CttIRQClear);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttIRQEn:%d,",
			ctt.CttIRQEn);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttVoltageMin:%d,",
			ctt.CttVoltageMin);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTargetScaleMin:%d,",
			ctt.CttTargetScaleMin);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTmaxIncPwr2:%d,",
			ctt.CttTmaxIncPwr2);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttImaxIncPwr2:%d,",
			ctt.CttImaxIncPwr2);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTmaxDelta:%d,",
			ctt.CttTmaxDelta);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttImaxDelta:%d,",
			ctt.CttImaxDelta);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttState:%d,",
			ctt.CttState);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttLkgCode_r:%d,",
			ctt.CttLkgCode_r);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" LKG_INCR:%d,",
			ctt.LKG_INCR);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttStartupDone:%d,",
			ctt.CttStartupDone);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTargetScaleOut:%d,",
			ctt.CttTargetScaleOut);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttLcode:%d,",
			ctt.CttLcode);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttTemp:%d,",
			ctt.CttTemp);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttCurrent:%d,",
			ctt.CttCurrent);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttMpmmGear1:%d,",
			ctt.CttMpmmGear1);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttMpmmGear0:%d,",
			ctt.CttMpmmGear0);

		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			" CttMpmmGear2:%d,",
			ctt.CttMpmmGear2);
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	ctt_debug("\n%s", aee_log_buf);
	ctt_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}
#endif
#endif
#endif

/************************************************
 * static function
 ************************************************/
static void mtk_ctt_cfg(unsigned int onOff,
		unsigned int ctt_n)
{
	const unsigned int ctt_key = CTT_CFG;
	/* update via atf */
	ctt_smc_handle(ctt_key, onOff, ctt_n);
}

static void mtk_ctt_Imax(unsigned int value,
		unsigned int ctt_n)
{
	const unsigned int ctt_key = CTT_IMAX;
	/* update via atf */
	ctt_smc_handle(ctt_key, value, ctt_n);
}

static void mtk_ctt_Tmax(unsigned int value,
		unsigned int ctt_n)
{
	const unsigned int ctt_key = CTT_TMAX;
	/* update via atf */
	ctt_smc_handle(ctt_key, value, ctt_n);
}

static void mtk_ctt_ImaxDelta(unsigned int value,
		unsigned int ctt_n)
{
	const unsigned int ctt_key = CTT_IMAX_DELTA;
	/* update via atf */
	ctt_smc_handle(ctt_key, value, ctt_n);
}

static void mtk_ctt_TmaxDelta(unsigned int value,
		unsigned int ctt_n)
{
	const unsigned int ctt_key = CTT_TMAX_DELTA;
	/* update via atf */
	ctt_smc_handle(ctt_key, value, ctt_n);
}

/************************************************
 * set CTT status by procfs interface
 ************************************************/
static ssize_t ctt_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable, ctt_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		ctt_debug("bad argument!! Should be \"0\" ~ \"16\"\n");
		goto out;
	}

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++)
		mtk_ctt_cfg((enable >> ctt_n) & 0x01, ctt_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ctt_cfg_proc_show(struct seq_file *m, void *v)
{
	int status = 0, value, ctt_n = 0;
	const unsigned int ctt_key = CTT_R_CFG;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		value = ctt_smc_handle(ctt_key,	0, ctt_n);
		status = status | ((value & 0x01) << (ctt_n * 4));
	}

	seq_printf(m, "%08x\n", status << 16);

	return 0;
}

static ssize_t ctt_imax_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, ctt_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ctt_n) != 2) {
		ctt_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ctt_Imax((unsigned int)value, (unsigned int)ctt_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ctt_imax_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_IMAX, 0, ctt_n);

		seq_printf(m, "ctt_%d, Imax = %d\n",
			ctt_n, status);
	}

	return 0;
}

static ssize_t ctt_tmax_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, ctt_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ctt_n) != 2) {
		ctt_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ctt_Tmax((unsigned int)value, (unsigned int)ctt_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ctt_tmax_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_TMAX, 0, ctt_n);

		seq_printf(m, "ctt_%d, Tmax = %d\n",
			ctt_n, status);
	}

	return 0;
}

static ssize_t ctt_imax_delta_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, ctt_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ctt_n) != 2) {
		ctt_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ctt_ImaxDelta((unsigned int)value, (unsigned int)ctt_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ctt_imax_delta_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_IMAX_DELTA, 0, ctt_n);

		seq_printf(m, "ctt_%d, ImaxDelta = %d\n",
			ctt_n, status);
	}

	return 0;
}

static ssize_t ctt_tmax_delta_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0, ctt_n = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ctt_n) != 2) {
		ctt_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ctt_TmaxDelta((unsigned int)value, (unsigned int)ctt_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ctt_tmax_delta_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_TMAX_DELTA, 0, ctt_n);

		seq_printf(m, "ctt_%d, TmaxDelta = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_target_scale_out_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_TARGETSCALEOUT, 0, ctt_n);

		seq_printf(m, "ctt_%d, TargetScaleOut = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_startup_done_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_STARTUPDONE, 0, ctt_n);

		seq_printf(m, "ctt_%d, StartupDone = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_state_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_STATE, 0, ctt_n);

		seq_printf(m, "ctt_%d, State = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_current_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_CURRENT, 0, ctt_n);

		seq_printf(m, "ctt_%d, Current = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_temp_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_TEMP, 0, ctt_n);

		seq_printf(m, "ctt_%d, Temp = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_lcode_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int ctt_n = 0;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		status = ctt_smc_handle(CTT_R_LCODE, 0, ctt_n);

		seq_printf(m, "ctt_%d, Lcode = %d\n",
			ctt_n, status);
	}

	return 0;
}

static int ctt_dump_proc_show(struct seq_file *m, void *v)
{
	struct ctt_class ctt;
	unsigned int *value = (unsigned int *)&ctt;
	unsigned int i, ctt_n = 0;
	const unsigned int ctt_key = CTT_R;

	for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++) {
		value[0] = ctt_smc_handle(ctt_key, CTT_V101, ctt_n);
		for (i = 1; i < 17; i++) {
			value[i] = ctt_smc_handle(ctt_key,
						CTT_V127 + (i * 4),
						ctt_n);
		}

		seq_printf(m, "[CTT][CPU%d]", ctt_n + 4);
		seq_printf(m, " SafeFreqReqOverride:%d,",
			ctt.safeFreqReqOverride);
		seq_printf(m, " GlobalEventEn:%d,",
			ctt.globalEventEn);
		seq_printf(m, " TestMode:%d,",
			ctt.testMode);
		seq_printf(m, " CttEn:%d,",
			ctt.cttEn);
		seq_printf(m, " FllSlowReqGateEn:%d,",
			ctt.fllSlowReqGateEn);
		seq_printf(m, " FllSlowReqEn:%d,",
			ctt.fllSlowReqEn);
		seq_printf(m, " FllClkOutSelect:%d,",
			ctt.fllClkOutSelect);
		seq_printf(m, " FllEn:%d,",
			ctt.fllEn);
		seq_printf(m, " ConfigComplete:%d,",
			ctt.configComplete);
		seq_printf(m, " DrccGateEn:%d,",
			ctt.drccGateEn);
		seq_printf(m, " SamplerEn:%d,",
			ctt.samplerEn);
		seq_printf(m, " CttTmax:%d,",
			ctt.CttTmax);
		seq_printf(m, " CttImax:%d,",
			ctt.CttImax);
		seq_printf(m, " CttVoltage:%d,",
			ctt.CttVoltage);
		seq_printf(m, " CttDynScale:%d,",
			ctt.CttDynScale);
		seq_printf(m, " CttLkgScale:%d,",
			ctt.CttLkgScale);
		seq_printf(m, " CttVcoef:%d,",
			ctt.CttVcoef);
		seq_printf(m, " CttTcoef2:%d,",
			ctt.CttTcoef2);
		seq_printf(m, " CttTcoef1:%d,",
			ctt.CttTcoef1);
		seq_printf(m, " CttLkgIrefTrim:%d,",
			ctt.CttLkgIrefTrim);
		seq_printf(m, " CttTcal:%d,",
			ctt.CttTcal);
		seq_printf(m, " CttVcal:%d,",
			ctt.CttVcal);
		seq_printf(m, " CttLcal:%d,",
			ctt.CttLcal);
		seq_printf(m, " CttFllNR:%d,",
			ctt.CttFllNR);
		seq_printf(m, " CttMpmmEn:%d,",
			ctt.CttMpmmEn);
		seq_printf(m, " CttImaxDisableEn:%d,",
			ctt.CttImaxDisableEn);
		seq_printf(m, " CttIRQStatus:%d,",
			ctt.CttIRQStatus);
		seq_printf(m, " CttIRQClear:%d,",
			ctt.CttIRQClear);
		seq_printf(m, " CttIRQEn:%d,",
			ctt.CttIRQEn);
		seq_printf(m, " CttVoltageMin:%d,",
			ctt.CttVoltageMin);
		seq_printf(m, " CttTargetScaleMin:%d,",
			ctt.CttTargetScaleMin);
		seq_printf(m, " CttTmaxIncPwr2:%d,",
			ctt.CttTmaxIncPwr2);
		seq_printf(m, " CttImaxIncPwr2:%d,",
			ctt.CttImaxIncPwr2);
		seq_printf(m, " CttTmaxDelta:%d,",
			ctt.CttTmaxDelta);
		seq_printf(m, " CttImaxDelta:%d,",
			ctt.CttImaxDelta);
		seq_printf(m, " CttState:%d,",
			ctt.CttState);
		seq_printf(m, " CttLkgCode_r:%d,",
			ctt.CttLkgCode_r);
		seq_printf(m, " LKG_INCR:%d,",
			ctt.LKG_INCR);
		seq_printf(m, " CttStartupDone:%d,",
			ctt.CttStartupDone);
		seq_printf(m, " CttTargetScaleOut:%d,",
			ctt.CttTargetScaleOut);
		seq_printf(m, " CttLcode:%d,",
			ctt.CttLcode);
		seq_printf(m, " CttTemp:%d,",
			ctt.CttTemp);
		seq_printf(m, " CttCurrent:%d,",
			ctt.CttCurrent);
		seq_printf(m, " CttMpmmGear1:%d,",
			ctt.CttMpmmGear1);
		seq_printf(m, " CttMpmmGear0:%d,",
			ctt.CttMpmmGear0);
		seq_printf(m, " CttMpmmGear2:%d,",
			ctt.CttMpmmGear2);
		seq_printf(m, " CttLkgCode_w:%d,",
			ctt.CttLkgCode_w);
		seq_printf(m, " CttLkgRst:%d,",
			ctt.CttLkgRst);
		seq_printf(m, " CttLkgTestEn:%d,",
			ctt.CttLkgTestEn);
		seq_printf(m, " CttLkgClk:%d,",
			ctt.CttLkgClk);
		seq_printf(m, " CttLkgStartup:%d,",
			ctt.CttLkgStartup);
		seq_printf(m, " CttLkgEnable:%d,",
			ctt.CttLkgEnable);
		seq_printf(m, " CttLkgControl:%d,",
			ctt.CttLkgControl);
		seq_printf(m, " CttDacBistCode:%d,",
			ctt.CttDacBistCode);
		seq_printf(m, " CttDacBistSingle:%d,",
			ctt.CttDacBistSingle);
		seq_printf(m, " CttDacBistLoopPwr2:%d,",
			ctt.CttDacBistLoopPwr2);
		seq_printf(m, " CttIrefBistLoopPwr2:%d,",
			ctt.CttIrefBistLoopPwr2);
		seq_printf(m, " CttCapBistLoopCount:%d,",
			ctt.CttCapBistLoopCount);
		seq_printf(m, " CttDacBistTarget:%d,",
			ctt.CttDacBistTarget);
		seq_printf(m, " CttIrefBistTarget:%d,",
			ctt.CttIrefBistTarget);
		seq_printf(m, " CttDacBistZone0Target:%d,",
			ctt.CttDacBistZone0Target);
		seq_printf(m, " CttDacBistZone1Target:%d,",
			ctt.CttDacBistZone1Target);
		seq_printf(m, " CttDacBistZone2Target:%d,",
			ctt.CttDacBistZone2Target);
		seq_printf(m, " CttDacBistZone3Target:%d,",
			ctt.CttDacBistZone3Target);
		seq_printf(m, " CttBistDone:%d,",
			ctt.CttBistDone);
		seq_printf(m, " CttDacBistFailCount:%d,",
			ctt.CttDacBistFailCount);
		seq_printf(m, " CttDacBistMaxFail:%d,",
			ctt.CttDacBistMaxFail);
		seq_printf(m, " CttDacBistZeroFail:%d,",
			ctt.CttDacBistZeroFail);
		seq_printf(m, " CttDacBistDone:%d,",
			ctt.CttDacBistDone);
		seq_printf(m, " CttIrefBistTrim:%d,",
			ctt.CttIrefBistTrim);
		seq_printf(m, " CttIrefBistMaxFail:%d,",
			ctt.CttIrefBistMaxFail);
		seq_printf(m, " CttIrefBistMinFail:%d,",
			ctt.CttIrefBistMinFail);
		seq_printf(m, " CttIrefBistMinFail:%d,",
			ctt.CttIrefBistMinFail);
		seq_printf(m, " CttIrefBistDone:%d,",
			ctt.CttIrefBistDone);
		seq_printf(m, " CttCapBistMaxFail:%d,",
			ctt.CttCapBistMaxFail);
		seq_printf(m, " CttCapBistZeroFail:%d,",
			ctt.CttCapBistZeroFail);
		seq_printf(m, " CttCapBistDone:%d,",
			ctt.CttCapBistDone);
		seq_printf(m, " CttDacBistCount:%d,",
			ctt.CttDacBistCount);
		seq_printf(m, " CttCapBistCount:%d,\n",
			ctt.CttCapBistCount);
	}
	return 0;
}

PROC_FOPS_RW(ctt_cfg);
PROC_FOPS_RW(ctt_imax);
PROC_FOPS_RW(ctt_tmax);
PROC_FOPS_RW(ctt_imax_delta);
PROC_FOPS_RW(ctt_tmax_delta);
PROC_FOPS_RO(ctt_target_scale_out);
PROC_FOPS_RO(ctt_startup_done);
PROC_FOPS_RO(ctt_state);
PROC_FOPS_RO(ctt_current);
PROC_FOPS_RO(ctt_temp);
PROC_FOPS_RO(ctt_lcode);
PROC_FOPS_RO(ctt_dump);

int ctt_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *ctt_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry ctt_entries[] = {
		PROC_ENTRY(ctt_cfg),
		PROC_ENTRY(ctt_imax),
		PROC_ENTRY(ctt_tmax),
		PROC_ENTRY(ctt_imax_delta),
		PROC_ENTRY(ctt_tmax_delta),
		PROC_ENTRY(ctt_target_scale_out),
		PROC_ENTRY(ctt_startup_done),
		PROC_ENTRY(ctt_state),
		PROC_ENTRY(ctt_current),
		PROC_ENTRY(ctt_temp),
		PROC_ENTRY(ctt_lcode),
		PROC_ENTRY(ctt_dump),
	};

	ctt_dir = proc_mkdir("ctt", dir);
	if (!ctt_dir) {
		ctt_debug("[%s]: mkdir /proc/ctt failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(ctt_entries); i++) {
		if (!proc_create(ctt_entries[i].name,
			0660,
			ctt_dir,
			ctt_entries[i].fops)) {
			ctt_debug("[%s]: create /proc/%s/ctt/%s failed\n",
				__func__,
				proc_name,
				ctt_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int ctt_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	unsigned int cttBitEn;
	unsigned int ctt_Imax[CTT_NUM];
	unsigned int ctt_Tmax[CTT_NUM];
	unsigned int ctt_ImaxDelta[CTT_NUM];
	unsigned int ctt_TmaxDelta[CTT_NUM];

	struct device_node *node = NULL;
	int rc = 0;
	unsigned int ctt_n = 0;

	node = pdev->dev.of_node;
	if (!node) {
		ctt_debug("get ctt device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "cttBitEn", &cttBitEn);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttBitEn(0x%x)\n",
			rc,
			cttBitEn);

		for (ctt_n = 0; ctt_n < CTT_NUM; ctt_n++)
			mtk_ctt_cfg((cttBitEn >> ctt_n) & 0x01, ctt_n);
	}

	rc = of_property_read_u32(node, "cttImax_0", &ctt_Imax[0]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImax_0(0x%x)\n",
			rc,
			ctt_Imax[0]);

		if (ctt_Imax[0] <= 8191)
			mtk_ctt_Imax(ctt_Imax[0], 0);
	}

	rc = of_property_read_u32(node, "cttImax_1", &ctt_Imax[1]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImax_1(0x%x)\n",
			rc,
			ctt_Imax[1]);

		if (ctt_Imax[1] <= 8191)
			mtk_ctt_Imax(ctt_Imax[1], 1);
	}

	rc = of_property_read_u32(node, "cttImax_2", &ctt_Imax[2]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImax_2(0x%x)\n",
			rc,
			ctt_Imax[2]);

		if (ctt_Imax[2] <= 8191)
			mtk_ctt_Imax(ctt_Imax[2], 2);
	}

	rc = of_property_read_u32(node, "cttImax_3", &ctt_Imax[3]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImax_3(0x%x)\n",
			rc,
			ctt_Imax[3]);

		if (ctt_Imax[3] <= 8191)
			mtk_ctt_Imax(ctt_Imax[3], 3);
	}

	rc = of_property_read_u32(node, "cttTmax_0", &ctt_Tmax[0]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmax_0(0x%x)\n",
			rc,
			ctt_Tmax[0]);

		if (ctt_Tmax[0] <= 255)
			mtk_ctt_Tmax(ctt_Tmax[0], 0);
	}

	rc = of_property_read_u32(node, "cttTmax_1", &ctt_Tmax[1]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmax_1(0x%x)\n",
			rc,
			ctt_Tmax[1]);

		if (ctt_Tmax[1] <= 255)
			mtk_ctt_Tmax(ctt_Tmax[1], 1);
	}

	rc = of_property_read_u32(node, "cttTmax_2", &ctt_Tmax[2]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmax_2(0x%x)\n",
			rc,
			ctt_Tmax[2]);

		if (ctt_Tmax[2] <= 255)
			mtk_ctt_Tmax(ctt_Tmax[2], 2);
	}

	rc = of_property_read_u32(node, "cttTmax_3", &ctt_Tmax[3]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmax_3(0x%x)\n",
			rc,
			ctt_Tmax[3]);

		if (ctt_Tmax[3] <= 255)
			mtk_ctt_Tmax(ctt_Tmax[3], 3);
	}

	rc = of_property_read_u32(node, "cttImaxDelta_0", &ctt_ImaxDelta[0]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImaxDelta_0(0x%x)\n",
			rc,
			ctt_ImaxDelta[0]);

		if (ctt_ImaxDelta[0] <= 2047)
			mtk_ctt_ImaxDelta(ctt_ImaxDelta[0], 0);
	}

	rc = of_property_read_u32(node, "cttImaxDelta_1", &ctt_ImaxDelta[1]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImaxDelta_1(0x%x)\n",
			rc,
			ctt_ImaxDelta[1]);

		if (ctt_ImaxDelta[1] <= 2047)
			mtk_ctt_ImaxDelta(ctt_ImaxDelta[1], 1);
	}

	rc = of_property_read_u32(node, "cttImaxDelta_2", &ctt_ImaxDelta[2]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImaxDelta_2(0x%x)\n",
			rc,
			ctt_ImaxDelta[2]);

		if (ctt_ImaxDelta[2] <= 2047)
			mtk_ctt_ImaxDelta(ctt_ImaxDelta[2], 2);
	}

	rc = of_property_read_u32(node, "cttImaxDelta_3", &ctt_ImaxDelta[3]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttImaxDelta_3(0x%x)\n",
			rc,
			ctt_ImaxDelta[3]);

		if (ctt_ImaxDelta[3] <= 2047)
			mtk_ctt_ImaxDelta(ctt_ImaxDelta[3], 3);
	}

	rc = of_property_read_u32(node, "cttTmaxDelta_0", &ctt_TmaxDelta[0]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmaxDelta_0(0x%x)\n",
			rc,
			ctt_TmaxDelta[0]);

		if (ctt_TmaxDelta[0] <= 31)
			mtk_ctt_TmaxDelta(ctt_TmaxDelta[0], 0);
	}

	rc = of_property_read_u32(node, "cttTmaxDelta_1", &ctt_TmaxDelta[1]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmaxDelta_1(0x%x)\n",
			rc,
			ctt_TmaxDelta[1]);

		if (ctt_TmaxDelta[1] <= 31)
			mtk_ctt_TmaxDelta(ctt_TmaxDelta[1], 1);
	}

	rc = of_property_read_u32(node, "cttTmaxDelta_2", &ctt_TmaxDelta[2]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmaxDelta_2(0x%x)\n",
			rc,
			ctt_TmaxDelta[2]);

		if (ctt_TmaxDelta[2] <= 31)
			mtk_ctt_TmaxDelta(ctt_TmaxDelta[2], 2);
	}

	rc = of_property_read_u32(node, "cttTmaxDelta_3", &ctt_TmaxDelta[3]);
	if (!rc) {
		ctt_debug("[xxxx_ctt] DTree ErrCode(%d), cttTmaxDelta_3(0x%x)\n",
			rc,
			ctt_TmaxDelta[3]);

		if (ctt_TmaxDelta[3] <= 31)
			mtk_ctt_TmaxDelta(ctt_TmaxDelta[3], 3);
	}
#endif /* CONFIG_OF */

#ifdef CONFIG_OF_RESERVED_MEM
#ifdef PTP3_STATUS_PROBE_DUMP
	/* dump reg status into PICACHU dram for DB */
	if (ctt_buf != NULL) {
		ctt_reserve_memory_dump(ctt_buf, ctt_mem_size,
			CTT_TRIGGER_STAGE_PROBE);
	}
#endif /* PTP3_STATUS_PROBE_DUMP */
#endif /* CONFIG_OF_RESERVED_MEM */

#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int ctt_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int ctt_resume(struct platform_device *pdev)
{
	return 0;
}

MODULE_DESCRIPTION("MediaTek CTT Driver v0.1");
MODULE_LICENSE("GPL");
#undef __MTK_CTT_C__
