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

//#define FLL_DEBUG
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

/************************************************
 * static Variable
 ************************************************/
static const char FLL_LIST_NAME[][40] = {
	FllFastKpOnline,
	FllFastKiOnline,
	FllSlowKpOnline,
	FllSlowKiOnline,
	FllKpOffline,
	FllKiOffline,
	FllCttTargetScaleDisable,
	FllTargetScale,
	FllInFreqOverrideVal,
	FllFreqErrWtOnline,
	FllFreqErrWtOffline,
	FllPhaseErrWt,
	FllFreqErrCapOnline,
	FllFreqErrCapOffline,
	FllPhaseErrCap,
	FllStartInPong,
	FllPingMaxThreshold,
	FllPongMinThreshold,
	FllDccEn,
	FllDccClkShaperCalin,
	FllClk26mDetDis,
	FllClk26mEn,
	FllControl,
	FllPhaselockThresh,
	FllPhaselockCycles,
	FllFreqlockRatio,
	FllFreqlockCycles,
	FllFreqUnlock,
	FllTst_cctrl,
	FllTst_fctrl,
	FllTst_sctrl,
	FllSlowReqCode,
	FllSlowReqResponseSelectMode,
	FllSlowReqFastResponseCycles,
	FllSlowReqErrorMask,
	FllEventSource0,
	FllEventSource1,
	FllEventType0,
	FllEventType1,
	FllEventSourceThresh0,
	FllEventSourceThresh1,
	FllEventFreeze,
	FllWGTriggerSource,
	FllWGTriggerCaptureDelay,
	FllWGTriggerEdge,
	FllWGTriggerVal,
	FllInFreq,
	FllOutFreq,
	FllCalFreq,
	FllStatus,
	FllPhaseErr,
	FllPhaseLockDet,
	FllFreqLockDet,
	FllClk26mDet,
	FllErrOnline,
	FllErrOffline,
	Fllfsm_state_sr,
	Fllfsm_state,
	FllSctrl_pong,
	FllCctrl_ping,
	FllFctrl_ping,
	FllSctrl_ping,
	FllCctrl_pong,
	FllFctrl_pong,
	FllEventCount0,
	FllEventCount1
};

static const char FLL_RW_REG_NAME[][5] = {
	V111,
	V112,
	V113,
	V114,
	V115,
	V116,
	V117,
	V118
};

static const char FLL_RO_REG_NAME[][5] = {
	V119,
	V120,
	V121,
	V122,
	V123,
	V124,
	V125,
	V126
};

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
	unsigned int cfg = 0;
	unsigned int cpu, value, group;
	int str_len = 0;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		fll_err("unable to get free page!\n");
		return -1;
	}
	fll_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
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
		str_len += snprintf(
			aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			FLL_TAG"[CPU%d]", cpu);
		for (group = 0; group < NR_FLL_RW_GROUP; group++) {

			/* encode cfg */
			/*
			 *	cfg[15:8] option
			 *	cfg[31:28] cpu
			 */
			cfg = (group << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
			cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

			/* update via atf */
			value = ptp3_smc_handle(
				PTP3_FEATURE_FLL,
				FLL_NODE_RW_REG_READ,
				cfg,
				0);

			if (group != NR_FLL_RW_GROUP-1)
				str_len += snprintf(
					aee_log_buf + str_len,
					ptp3_mem_size - str_len,
					" %s:0x%08x,", FLL_RW_REG_NAME[group], value);
			else
				str_len += snprintf(
					aee_log_buf + str_len,
					ptp3_mem_size - str_len,
					" %s:0x%08x\n", FLL_RW_REG_NAME[group], value);
		}
	}

	/* fill data to aee_log_buf */
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
static ssize_t fll_ctrl_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cpu, option, value;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		fll_err("buf is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		fll_err("count(%u) >= PAGE_SIZE\n", (unsigned int)count);
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		fll_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %x",
		&cpu, &option, &value) != 3) {

		fll_err("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	/* encode cfg */
	/*
	 *	cfg[15:8] option
	 *	cfg[31:28] cpu
	 */
	cfg = (option << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
	cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

	/* update via atf */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_FLL,
		FLL_NODE_LIST_WRITE,
		cfg,
		value);

	if (ret < 0) {
		fll_err("ret(%d). access atf fail\n", ret);
		goto out;
	}

	/* update via mcupm or cpu_eb */
	fll_ipi_handle(0, 0, 0, 0, 0);

out:
	free_page((unsigned long)buf);
	return count;

}

static int fll_ctrl_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int fll_list_proc_show(struct seq_file *m, void *v)
{
	unsigned int list_num;

	for (list_num = 0; list_num <= FLL_LIST_FllWGTriggerVal; list_num++)
		seq_printf(m, "%d.%s\n", list_num, FLL_LIST_NAME[list_num]);

	return 0;
}

static int fll_dump_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value, option;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		seq_printf(m, FLL_TAG"[CPU%d]", cpu);

		for (option = 0; option < NR_FLL_LIST; option++) {

			switch (option) {
			case FLL_LIST_FllFastKpOnline:
			case FLL_LIST_FllFastKiOnline:
			case FLL_LIST_FllSlowKpOnline:
			case FLL_LIST_FllSlowKiOnline:
			case FLL_LIST_FllKpOffline:
			case FLL_LIST_FllKiOffline:
			case FLL_LIST_FllCttTargetScaleDisable:
			case FLL_LIST_FllTargetScale:
			case FLL_LIST_FllInFreqOverrideVal:
			case FLL_LIST_FllStartInPong:
			case FLL_LIST_FllPingMaxThreshold:
			case FLL_LIST_FllPongMinThreshold:
			case FLL_LIST_FllDccEn:
			case FLL_LIST_FllDccClkShaperCalin:
			case FLL_LIST_FllFreqlockRatio:
			case FLL_LIST_FllFreqlockCycles:
			case FLL_LIST_FllPhaselockThresh:
			case FLL_LIST_FllPhaselockCycles:
			case FLL_LIST_FllFreqUnlock:
			case FLL_LIST_FllTst_cctrl:
			case FLL_LIST_FllTst_fctrl:
			case FLL_LIST_FllTst_sctrl:
			case FLL_LIST_FllSlowReqCode:
			case FLL_LIST_FllSlowReqResponseSelectMode:
			case FLL_LIST_FllSlowReqFastResponseCycles:
			case FLL_LIST_FllSlowReqErrorMask:
			case FLL_LIST_FllInFreq:
			case FLL_LIST_FllOutFreq:
			case FLL_LIST_FllCalFreq:
			case FLL_LIST_FllPhaseLockDet:
			case FLL_LIST_FllFreqLockDet:
			case FLL_LIST_Fllfsm_state_sr:
			case FLL_LIST_Fllfsm_state:
			case FLL_LIST_FllSctrl_pong:
			case FLL_LIST_FllCctrl_ping:
			case FLL_LIST_FllFctrl_ping:
			case FLL_LIST_FllSctrl_ping:
			case FLL_LIST_FllCctrl_pong:
			case FLL_LIST_FllFctrl_pong:
				/* encode cfg */
				/*
				 *	cfg[15:8] option
				 *	cfg[31:28] cpu
				 */
				cfg = (option << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
				cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

				/* update via atf */
				value = ptp3_smc_handle(
					PTP3_FEATURE_FLL,
					FLL_NODE_LIST_READ,
					cfg,
					0);

				if (option != FLL_LIST_FllFctrl_pong)
					seq_printf(m, " %s:0x%x,",
						FLL_LIST_NAME[option], value);
				else /* last one */
					seq_printf(m, " %s:0x%x;\n",
						FLL_LIST_NAME[option], value);
				break;
			case FLL_LIST_FllFreqErrWtOnline:
			case FLL_LIST_FllFreqErrWtOffline:
			case FLL_LIST_FllPhaseErrWt:
			case FLL_LIST_FllFreqErrCapOnline:
			case FLL_LIST_FllFreqErrCapOffline:
			case FLL_LIST_FllPhaseErrCap:
			case FLL_LIST_FllClk26mDetDis:
			case FLL_LIST_FllClk26mEn:
			case FLL_LIST_FllControl:
			case FLL_LIST_FllWGTriggerSource:
			case FLL_LIST_FllWGTriggerCaptureDelay:
			case FLL_LIST_FllWGTriggerEdge:
			case FLL_LIST_FllWGTriggerVal:
			case FLL_LIST_FllStatus:
			case FLL_LIST_FllPhaseErr:
			case FLL_LIST_FllClk26mDet:
			case FLL_LIST_FllErrOnline:
			case FLL_LIST_FllErrOffline:
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

static ssize_t fll_reg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cpu, group, value;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		fll_err("buf is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		fll_err("count(%u) >= PAGE_SIZE\n", (unsigned int)count);
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		fll_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %x",
		&cpu, &group, &value) != 3) {

		fll_err("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	/* check group if illegal */
	if (((group - 111) >= FLL_RW_GROUP_V111)
		&& ((group - 111) <= FLL_RW_GROUP_V118)) {

		fll_err("group(%d) is illegal\n", group);
		goto out;
	}

	/* encode cfg */
	/*
	 *	cfg[15:8] option
	 *	cfg[31:28] cpu
	 */
	cfg = (group << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
	cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

	/* update via atf */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_FLL,
		FLL_NODE_RW_REG_WRITE,
		cfg,
		value);

	if (ret < 0) {
		fll_err("ret(%d). access atf fail\n", ret);
		goto out;
	}

	/* update via mcupm or cpu_eb */
	fll_ipi_handle(0, 0, 0, 0, 0);

out:
	free_page((unsigned long)buf);
	return count;

}


static int fll_reg_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value, group;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		seq_printf(m, FLL_TAG"[CPU%d]", cpu);

		for (group = 0; group < NR_FLL_RW_GROUP; group++) {

			/* encode cfg */
			/*
			 *	cfg[15:8] option
			 *	cfg[31:28] cpu
			 */
			cfg = (group << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
			cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

			/* update via atf */
			value = ptp3_smc_handle(
				PTP3_FEATURE_FLL,
				FLL_NODE_RW_REG_READ,
				cfg,
				0);

			seq_printf(m, " %s:0x%08x,",
				FLL_RW_REG_NAME[group], value);
		}

		for (group = 0; group < NR_FLL_RO_GROUP; group++) {

			/* encode cfg */
			/*
			 *	cfg[15:8] option
			 *	cfg[31:28] cpu
			 */
			cfg = (group << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
			cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

			/* update via atf */
			value = ptp3_smc_handle(
				PTP3_FEATURE_FLL,
				FLL_NODE_RO_REG_READ,
				cfg,
				0);

			if (group != NR_FLL_RO_GROUP-1)
				seq_printf(m, " %s:0x%08x,",
					FLL_RO_REG_NAME[group], value);
			else
				seq_printf(m, " %s:0x%08x;\n",
					FLL_RO_REG_NAME[group], value);
		}
	}
	return 0;
}

static ssize_t fll_eventCount_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cntSel, src, type, thres;
	unsigned int cpu;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		fll_err("buf is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		fll_err("count(%u) >= PAGE_SIZE\n", (unsigned int)count);
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		fll_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %u %u %u",
		&cpu, &cntSel, &src, &type, &thres) != 5) {

		fll_err("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	/* check for cnt0 or cnt1 */
	if (cntSel == 0) {
		/* set EventSource */
		cfg = (FLL_LIST_FllEventSource0 << FLL_CFG_OFFSET_OPTION) &
			FLL_CFG_BITMASK_OPTION;
		cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

		/* update via atf */
		ret = ptp3_smc_handle(
			PTP3_FEATURE_FLL,
			FLL_NODE_LIST_WRITE,
			cfg,
			src);

		/* set EventType */
		cfg = (FLL_LIST_FllEventType0 << FLL_CFG_OFFSET_OPTION) &
			FLL_CFG_BITMASK_OPTION;
		cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

		/* update via atf */
		ret = ptp3_smc_handle(
			PTP3_FEATURE_FLL,
			FLL_NODE_LIST_WRITE,
			cfg,
			type);

		/* set EventSourceThresh */
		cfg = (FLL_LIST_FllEventSourceThresh0 << FLL_CFG_OFFSET_OPTION) &
			FLL_CFG_BITMASK_OPTION;
		cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

		/* update via atf */
		ret = ptp3_smc_handle(
			PTP3_FEATURE_FLL,
			FLL_NODE_LIST_WRITE,
			cfg,
			thres);
	} else {
		/* set EventSource */
		cfg = (FLL_LIST_FllEventSource1 << FLL_CFG_OFFSET_OPTION) &
			FLL_CFG_BITMASK_OPTION;
		cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

		/* update via atf */
		ret = ptp3_smc_handle(
			PTP3_FEATURE_FLL,
			FLL_NODE_LIST_WRITE,
			cfg,
			src);

		/* set EventType */
		cfg = (FLL_LIST_FllEventType1 << FLL_CFG_OFFSET_OPTION) &
			FLL_CFG_BITMASK_OPTION;
		cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

		/* update via atf */
		ret = ptp3_smc_handle(
			PTP3_FEATURE_FLL,
			FLL_NODE_LIST_WRITE,
			cfg,
			type);

		/* set EventSourceThresh */
		cfg = (FLL_LIST_FllEventSourceThresh1 << FLL_CFG_OFFSET_OPTION) &
			FLL_CFG_BITMASK_OPTION;
		cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

		/* update via atf */
		ret = ptp3_smc_handle(
			PTP3_FEATURE_FLL,
			FLL_NODE_LIST_WRITE,
			cfg,
			thres);
	}

	if (ret < 0) {
		fll_err("ret(%d). access atf fail\n", ret);
		goto out;
	}

	/* update via mcupm or cpu_eb */
	fll_ipi_handle(0, 0, 0, 0, 0);

out:
	free_page((unsigned long)buf);
	return count;

}


static int fll_eventCount_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value, option;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		seq_printf(m, FLL_TAG"[CPU%d]", cpu);

		for (option = 0; option < NR_FLL_LIST; option++) {

			switch (option) {
			case FLL_LIST_FllEventSource0:
			case FLL_LIST_FllEventSource1:
			case FLL_LIST_FllEventType0:
			case FLL_LIST_FllEventType1:
			case FLL_LIST_FllEventSourceThresh0:
			case FLL_LIST_FllEventSourceThresh1:
			case FLL_LIST_FllEventFreeze:
			case FLL_LIST_FllEventCount0:
			case FLL_LIST_FllEventCount1:
				/* encode cfg */
				/*
				 *	cfg[15:8] option
				 *	cfg[31:28] cpu
				 */
				cfg = (option << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
				cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

				/* update via atf */
				value = ptp3_smc_handle(
					PTP3_FEATURE_FLL,
					FLL_NODE_LIST_READ,
					cfg,
					0);

				if (option != FLL_LIST_FllEventCount1)
					seq_printf(m, " %s:0x%x,",
						FLL_LIST_NAME[option], value);
				else /* the last one */
					seq_printf(m, " %s:0x%x;\n",
						FLL_LIST_NAME[option], value);
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

static ssize_t fll_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cpu, value, fllEn;
	char *fllEn_str;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		fll_err("buf is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		fll_err("count(%u) >= PAGE_SIZE\n", (unsigned int)count);
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		fll_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* Convert str to hex */
	fllEn_str = strsep(&buf, " ");
	if (fllEn_str)
		ret = kstrtou32(fllEn_str, 16, (unsigned int *)&fllEn);

	for (cpu = BRISKET2_CPU_START_ID; cpu <= BRISKET2_CPU_END_ID; cpu++) {
		value = (fllEn >> cpu*4) & 0xF;
		cfg = (BRISKET2_LIST_FllEn << BRISKET2_CFG_OFFSET_OPTION)
			& BRISKET2_CFG_BITMASK_OPTION;
		cfg |= (cpu << BRISKET2_CFG_OFFSET_CPU) & BRISKET2_CFG_BITMASK_CPU;
		/* update via atf */
		ptp3_smc_handle(
			PTP3_FEATURE_BRISKET2,
			BRISKET2_NODE_LIST_WRITE,
			cfg,
			value);
	}

	if (ret < 0) {
		fll_err("ret(%d). access atf fail\n", ret);
		goto out;
	}

out:
	free_page((unsigned long)buf);
	return count;

}


static int fll_cfg_proc_show(struct seq_file *m, void *v)
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
		cfg = (BRISKET2_LIST_FllEn << BRISKET2_CFG_OFFSET_OPTION)
			& BRISKET2_CFG_BITMASK_OPTION;
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

static ssize_t fll_eventFreeze_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	unsigned int cfg = 0;
	unsigned int cpu, value;
	int ret = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf) {
		fll_err("buf is illegal\n");
		goto out;
	}

	if (count >= PAGE_SIZE) {
		fll_err("count(%u) >= PAGE_SIZE\n", (unsigned int)count);
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		fll_err("buffer copy fail\n");
		goto out;
	}

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u",
		&cpu, &value) != 2) {

		fll_err("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	/* encode cfg */
	/*
	 *	cfg[15:8] option
	 *	cfg[31:28] cpu
	 */
	cfg = (FLL_LIST_FllEventFreeze << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
	cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

	/* update via atf */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_FLL,
		FLL_NODE_LIST_WRITE,
		cfg,
		value);

	if (ret < 0) {
		fll_err("ret(%d). access atf fail\n", ret);
		goto out;
	}

	/* update via mcupm or cpu_eb */
	fll_ipi_handle(0, 0, 0, 0, 0);

out:
	free_page((unsigned long)buf);
	return count;

}

static int fll_eventFreeze_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int fll_freq_proc_show(struct seq_file *m, void *v)
{
	unsigned int cfg = 0;
	unsigned int cpu, value, option;

	for (cpu = BRISKET2_CPU_START_ID; cpu <= BRISKET2_CPU_END_ID; cpu++) {
		seq_printf(m, FLL_TAG"[CPU%d]", cpu);

		for (option = FLL_LIST_FllInFreq; option <= FLL_LIST_FllOutFreq; option++) {
			/* encode cfg */
			/*
			 *	cfg[15:8] option
			 *	cfg[31:28] cpu
			 */
			cfg = (option << FLL_CFG_OFFSET_OPTION) & FLL_CFG_BITMASK_OPTION;
			cfg |= (cpu << FLL_CFG_OFFSET_CPU) & FLL_CFG_BITMASK_CPU;

			/* update via atf */
			value = ptp3_smc_handle(
				PTP3_FEATURE_FLL,
				FLL_NODE_LIST_READ,
				cfg,
				0);

			if (option != FLL_LIST_FllOutFreq)
				seq_printf(m, " %s:0x%x,",
					FLL_LIST_NAME[option], value);
			else /* last one */
				seq_printf(m, " %s:0x%x;\n",
					FLL_LIST_NAME[option], value);
		}
	}
	return 0;
}


PROC_FOPS_RW(fll_ctrl);
PROC_FOPS_RO(fll_list);
PROC_FOPS_RO(fll_dump);
PROC_FOPS_RW(fll_reg);
PROC_FOPS_RW(fll_eventCount);
PROC_FOPS_RW(fll_cfg);
PROC_FOPS_RW(fll_eventFreeze);
PROC_FOPS_RO(fll_freq);

int fll_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *fll_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry fll_entries[] = {
		PROC_ENTRY(fll_ctrl),
		PROC_ENTRY(fll_list),
		PROC_ENTRY(fll_dump),
		PROC_ENTRY(fll_reg),
		PROC_ENTRY(fll_eventCount),
		PROC_ENTRY(fll_cfg),
		PROC_ENTRY(fll_eventFreeze),
		PROC_ENTRY(fll_freq),
	};

	fll_dir = proc_mkdir("fll", dir);
	if (!fll_dir) {
		fll_debug("[%s]: mkdir /proc/fll failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(fll_entries); i++) {
		if (!proc_create(fll_entries[i].name,
			0660,
			fll_dir,
			fll_entries[i].fops)) {
			fll_debug("[%s]: create /proc/%s/fll/%s failed\n",
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
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (fll_buf != NULL) {
		fll_reserve_memory_dump(
			fll_buf, fll_mem_size, FLL_TRIGGER_STAGE_PROBE);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int fll_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (fll_buf != NULL) {
		fll_reserve_memory_dump(
			fll_buf+0x1000, fll_mem_size, FLL_TRIGGER_STAGE_SUSPEND);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int fll_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (fll_buf != NULL) {
		fll_reserve_memory_dump(
			fll_buf+0x2000, fll_mem_size, FLL_TRIGGER_STAGE_RESUME);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

MODULE_DESCRIPTION("MediaTek FLL Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_FLL_C__
