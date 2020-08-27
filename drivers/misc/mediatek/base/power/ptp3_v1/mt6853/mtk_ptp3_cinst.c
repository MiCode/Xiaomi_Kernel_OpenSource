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
static unsigned int cinst_doe_enable;
static unsigned int cinst_doe_const_mode;
static unsigned int cinst_doe_ls_idx_sel;

static unsigned int cinst_doe_ls_period;
static unsigned int cinst_doe_ls_credit;
static unsigned int cinst_doe_ls_low_freq_period;
static unsigned int cinst_doe_ls_low_freq_enable;

static unsigned int cinst_doe_vx_period;
static unsigned int cinst_doe_vx_credit;
static unsigned int cinst_doe_vx_low_freq_period;
static unsigned int cinst_doe_vx_low_freq_enable;

#endif /* CONFIG_OF */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

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
	int str_len = 0;
	int cpu, cpu_tmp;
	unsigned char cinst_info[NR_CINST_CPU]
		[NR_CINST_CHANNEL][NR_CINST_CFG];
	unsigned int cinst_bcpu_cfg[NR_CINST_CPU];
	unsigned int cinst_ao_cfg[NR_CINST_CPU];
	unsigned char cinst_const_mode[NR_CINST_CPU];
	unsigned char cinst_ls_idx_sel[NR_CINST_CPU];
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);
	unsigned int cinst_cfg = CINST_RW_REG_READ << 4;
	unsigned char is_reg_status_valid;

	/* check free page valid or not */
	if (!aee_log_buf) {
		cinst_err("unable to get free page!\n");
		return -1;
	}
	cinst_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* show trigger stage */
	switch (cinst_tri_stage) {
	case CINST_TRIGGER_STAGE_PROBE:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Probe]\n");
		break;
	case CINST_TRIGGER_STAGE_SUSPEND:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Suspend]\n");
		break;
	case CINST_TRIGGER_STAGE_RESUME:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Resume]\n");
		break;
	default:
		cinst_err("illegal CINST_TRIGGER_STAGE\n");
		break;
	}

	/* collect dump info */
	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {

		cpu_tmp = cpu-CINST_CPU_START_ID;
		is_reg_status_valid = 0;

		/* Get BCPU reg status */
		do {
			cinst_bcpu_cfg[cpu_tmp] =
				mt_secure_call(
					MTK_SIP_KERNEL_PTP3_CONTROL,
					PTP3_FEATURE_CINST,
					CINST_CPU_BASE[cpu_tmp]+
					CINST_DIDT_CONTROL,
					cinst_cfg,
					0);
			is_reg_status_valid++;
			if (is_reg_status_valid == 100)
				break;
		} while (
			(cinst_bcpu_cfg[cpu_tmp] == REG_INVALID) ||
			(cinst_bcpu_cfg[cpu_tmp] == REG_DEFAULT)
			);

		/* Get AO reg status */
		cinst_ao_cfg[cpu_tmp] =
			mt_secure_call(
				MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_CINST,
				CINST_CPU_AO_BASE[cpu_tmp],
				cinst_cfg,
				0);

		cinst_debug(
			"[CPU%d] bcpu_value=0x%08x, ao_value=0x%08x\n",
			cpu,
			cinst_bcpu_cfg[cpu_tmp],
			cinst_ao_cfg[cpu_tmp]);

		/* if BCPUx in core off, return default status */
		if ((cinst_bcpu_cfg[cpu_tmp] == REG_INVALID)
			|| (cinst_bcpu_cfg[cpu_tmp] == REG_DEFAULT)) {
			/* LS */
			cinst_info[cpu_tmp][0][CINST_CFG_PERIOD] =
				GET_BITS_VAL(7:5, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_CREDIT] =
				GET_BITS_VAL(4:0, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(10:8, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(11:11, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_ENABLE] = 0;

			/* VX */
			cinst_info[cpu_tmp][1][CINST_CFG_PERIOD] =
				GET_BITS_VAL(23:21, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_CREDIT] =
				GET_BITS_VAL(20:16, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(26:24, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(27:27, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_ENABLE] = 0;

			/* Const mode */
			cinst_const_mode[cpu_tmp] =
				GET_BITS_VAL(31:31, REG_DEFAULT);

			/* LS index select */
			cinst_ls_idx_sel[cpu_tmp] = 0;
		} else {
			cinst_info[cpu_tmp][0][CINST_CFG_PERIOD] =
				GET_BITS_VAL(7:5, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_CREDIT] =
				GET_BITS_VAL(4:0, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(10:8, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(11:11, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_ENABLE] =
				GET_BITS_VAL(15:15, cinst_ao_cfg[cpu_tmp]);

			/* VX */
			cinst_info[cpu_tmp][1][CINST_CFG_PERIOD] =
				GET_BITS_VAL(23:21, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_CREDIT] =
				GET_BITS_VAL(20:16, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(26:24, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(27:27, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_ENABLE] =
				GET_BITS_VAL(18:18, cinst_ao_cfg[cpu_tmp]);

			/* Const mode */
			cinst_const_mode[cpu_tmp] =
				GET_BITS_VAL(31:31, cinst_bcpu_cfg[cpu_tmp]);

			/* LS index select */
			cinst_ls_idx_sel[cpu_tmp] =
				GET_BITS_VAL(17:17, cinst_ao_cfg[cpu_tmp]);
		}

		str_len +=
			snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"cpu%d %s period=%d credit=%d low_period=%d low_freq_en=%d enable=%d\n",
			cpu, "LS",
			cinst_info[cpu_tmp][0]
				[CINST_CFG_PERIOD],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_CREDIT],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_LOW_PWR_PERIOD],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_LOW_PWR_ENABLE],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_ENABLE]);

		str_len +=
			snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"cpu%d %s period=%d credit=%d low_period=%d low_freq_en=%d enable=%d\n",
			cpu, "VX",
			cinst_info[cpu_tmp][1]
				[CINST_CFG_PERIOD],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_CREDIT],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_LOW_PWR_PERIOD],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_LOW_PWR_ENABLE],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_ENABLE]);

		str_len +=
			snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"cpu%d const_mode=%d ls_index_select=%d\n",
			cpu,
			cinst_const_mode[cpu_tmp],
			cinst_ls_idx_sel[cpu_tmp]);
	}


	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	cinst_msg("\n%s", aee_log_buf);
	cinst_msg("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * SMC between kernel and atf
 ************************************************/
static unsigned int cinst_smc_handle(
	unsigned int rw, unsigned int cpu, unsigned int param, unsigned int val)
{
	unsigned int ret;
	unsigned int cinst_cfg =
		(rw << 4) | (param << 6) | cpu;

	cinst_msg("[%s]:cpu(%d) param(%d) val(%d)\n",
		__func__, cpu, param, val);

	/* update atf via smc */
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		PTP3_FEATURE_CINST,
		0,
		cinst_cfg,
		val);

	return ret;
}

/************************************************
 * IPI between kernel and mcupm/cpu_eb
 ************************************************/
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

/************************************************
 * update CINST status with ATF
 ************************************************/
static void mtk_cinst_const_mode(unsigned int cpu, unsigned int value)
{
	/* update bren via atf */
	cinst_smc_handle(CINST_RW_WRITE, cpu, CINST_PARAM_CONST_MODE, value);
}

static void mtk_cinst_ls_idx_sel(unsigned int cpu, unsigned int value)
{
	/* update bren via atf */
	cinst_smc_handle(CINST_RW_WRITE, cpu, CINST_PARAM_LS_IDX_SEL, value);
}

static void mtk_cinst(unsigned int cpu,
		unsigned int ls_vx, unsigned int cfg, unsigned int value)
{
	unsigned int param = ls_vx * NR_CINST_CFG + cfg;

	/* update bren via atf */
	cinst_smc_handle(CINST_RW_WRITE, cpu, param, value);

	/* update via mcupm or cpu_eb */
	cinst_ipi_handle(cpu, param, value);
}

/************************************************
 * set CINST status by procfs interface
 ************************************************/
static int cinst_proc_show(struct seq_file *m, void *v)
{
	int cpu, cpu_tmp;
	unsigned int cinst_info[NR_CINST_CPU]
		[NR_CINST_CHANNEL][NR_CINST_CFG];
	unsigned int cinst_bcpu_cfg[NR_CINST_CPU];
	unsigned int cinst_ao_cfg[NR_CINST_CPU];
	unsigned char cinst_const_mode[NR_CINST_CPU];
	unsigned char cinst_ls_idx_sel[NR_CINST_CPU];
	unsigned int cinst_cfg = CINST_RW_REG_READ << 4;
	unsigned char is_reg_status_valid;

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {

		cpu_tmp = cpu-CINST_CPU_START_ID;
		is_reg_status_valid = 0;

		/* Get BCPU reg status */
		do {
			cinst_bcpu_cfg[cpu_tmp] =
				mt_secure_call(
					MTK_SIP_KERNEL_PTP3_CONTROL,
					PTP3_FEATURE_CINST,
					CINST_CPU_BASE[cpu_tmp]+
					CINST_DIDT_CONTROL,
					cinst_cfg,
					0);
			is_reg_status_valid++;
			if (is_reg_status_valid == 100)
				break;
		} while (
			(cinst_bcpu_cfg[cpu_tmp] == REG_INVALID) ||
			(cinst_bcpu_cfg[cpu_tmp] == REG_DEFAULT)
			);

		/* Get AO reg status */
		cinst_ao_cfg[cpu_tmp] =
			mt_secure_call(
				MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_CINST,
				CINST_CPU_AO_BASE[cpu_tmp],
				cinst_cfg,
				0);

		cinst_msg(
			"[CPU%d] bcpu_value=0x%08x, ao_value=0x%08x\n",
			cpu,
			cinst_bcpu_cfg[cpu_tmp],
			cinst_ao_cfg[cpu_tmp]);

		/* if BCPUx in core off, return default status */
		if ((cinst_bcpu_cfg[cpu_tmp] == REG_INVALID)
			|| (cinst_bcpu_cfg[cpu_tmp] == REG_DEFAULT)) {
			/* LS */
			cinst_info[cpu_tmp][0][CINST_CFG_PERIOD] =
				GET_BITS_VAL(7:5, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_CREDIT] =
				GET_BITS_VAL(4:0, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(10:8, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(11:11, REG_DEFAULT);
			cinst_info[cpu_tmp][0][CINST_CFG_ENABLE] = 0;

			/* VX */
			cinst_info[cpu_tmp][1][CINST_CFG_PERIOD] =
				GET_BITS_VAL(23:21, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_CREDIT] =
				GET_BITS_VAL(20:16, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(26:24, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(27:27, REG_DEFAULT);
			cinst_info[cpu_tmp][1][CINST_CFG_ENABLE] = 0;

			/* Const mode */
			cinst_const_mode[cpu_tmp] =
				GET_BITS_VAL(31:31, REG_DEFAULT);

			/* LS index select */
			cinst_ls_idx_sel[cpu_tmp] = 0;
		} else {
			cinst_info[cpu_tmp][0][CINST_CFG_PERIOD] =
				GET_BITS_VAL(7:5, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_CREDIT] =
				GET_BITS_VAL(4:0, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(10:8, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(11:11, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][0][CINST_CFG_ENABLE] =
				GET_BITS_VAL(15:15, cinst_ao_cfg[cpu_tmp]);

			/* VX */
			cinst_info[cpu_tmp][1][CINST_CFG_PERIOD] =
				GET_BITS_VAL(23:21, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_CREDIT] =
				GET_BITS_VAL(20:16, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_PERIOD] =
				GET_BITS_VAL(26:24, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_LOW_PWR_ENABLE] =
				GET_BITS_VAL(27:27, cinst_bcpu_cfg[cpu_tmp]);
			cinst_info[cpu_tmp][1][CINST_CFG_ENABLE] =
				GET_BITS_VAL(18:18, cinst_ao_cfg[cpu_tmp]);

			/* Const mode */
			cinst_const_mode[cpu_tmp] =
				GET_BITS_VAL(31:31, cinst_bcpu_cfg[cpu_tmp]);

			/* LS index select */
			cinst_ls_idx_sel[cpu_tmp] =
				GET_BITS_VAL(17:17, cinst_ao_cfg[cpu_tmp]);
		}
		seq_printf(m,
			"cpu%d LS period=%d credit=%d low_period=%d low_freq_en=%d enable=%d\n",
			cpu,
			cinst_info[cpu_tmp][0]
				[CINST_CFG_PERIOD],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_CREDIT],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_LOW_PWR_PERIOD],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_LOW_PWR_ENABLE],
			cinst_info[cpu_tmp][0]
				[CINST_CFG_ENABLE]);

		seq_printf(m,
			"cpu%d VX period=%d credit=%d low_period=%d low_freq_en=%d enable=%d\n",
			cpu,
			cinst_info[cpu_tmp][1]
				[CINST_CFG_PERIOD],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_CREDIT],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_LOW_PWR_PERIOD],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_LOW_PWR_ENABLE],
			cinst_info[cpu_tmp][1]
				[CINST_CFG_ENABLE]);

		seq_printf(m,
			"cpu%d const_mode=%d ls_index_select=%d\n",
			cpu,
			cinst_const_mode[cpu_tmp],
			cinst_ls_idx_sel[cpu_tmp]);

	}

	return 0;
}

static ssize_t cinst_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cpu, ls_vx, cfg, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u %u", &cpu, &ls_vx, &cfg, &value) != 4) {
		cinst_err("bad argument!! Should input 4 arguments.\n");
		goto out;
	}

	if ((cpu < CINST_CPU_START_ID) || (cpu > CINST_CPU_END_ID))
		goto out;

	if ((ls_vx < 0) || (ls_vx > 1))
		goto out;

	switch (cfg) {
	case 0: //period
		if ((value < 0) || (value > 7))
			goto out;
		break;
	case 1: //credit
		if ((value < 0) || (value > 31))
			goto out;
		break;
	case 2: //low_period
		if ((value < 0) || (value > 7))
			goto out;
		break;
	case 3: //low_freq_en
		if ((value < 0) || (value > 1))
			goto out;
		break;
	case 4: //enable
		if ((value < 0) || (value > 1))
			goto out;
		break;
	default:
		goto out;
	}

	mtk_cinst((unsigned int)cpu, (unsigned int)ls_vx,
		(unsigned int)cfg, (unsigned int)value);

out:
	free_page((unsigned long)buf);

	return count;
}

static int cinst_en_proc_show(struct seq_file *m, void *v)
{
	int cpu, ls_vx, cfg, param, cpu_tmp;

	unsigned char cinst_info[NR_CINST_CPU]
		[NR_CINST_CHANNEL][NR_CINST_CFG];

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {

		for (ls_vx = 0; ls_vx < NR_CINST_CHANNEL; ls_vx++) {
			for (cfg = 0; cfg < NR_CINST_CFG; cfg++) {
				param = ls_vx * NR_CINST_CFG + cfg;

				cpu_tmp = cpu-CINST_CPU_START_ID;

				/* read from atf */
				cinst_info[cpu_tmp][ls_vx][cfg] =
					cinst_smc_handle(
						CINST_RW_READ,
						cpu,
						param,
						0);

				cinst_msg(
					"Get cpu=%d ls_vx=%d cfg=%d value=%d\n",
					cpu, ls_vx, cfg,
					cinst_info[cpu_tmp][ls_vx][cfg]);
			}
		}
	}

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {
		for (ls_vx = 0;
			ls_vx < NR_CINST_CHANNEL; ls_vx++) {

			cpu_tmp = cpu-CINST_CPU_START_ID;

			seq_printf(m,
			"cpu%d %s enable=%d\n",
			cpu, (ls_vx == 0 ? "LS" : "VX"),
			cinst_info[cpu_tmp][ls_vx]
				[CINST_CFG_ENABLE]
				);
		}
	}

	return 0;
}

static ssize_t cinst_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cpu, ls_vx, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtou32((const char *)buf, 0, &value)) {
		cinst_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	for (ls_vx = 0; ls_vx < NR_CINST_CHANNEL; ls_vx++) {
		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {
			mtk_cinst(cpu, ls_vx,
				CINST_CFG_ENABLE,
				(value >> (ls_vx*
				(CINST_CPU_END_ID+1)+cpu))&1);
		}
	}

out:
	free_page((unsigned long)buf);

	return count;
}

static int cinst_const_mode_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cinst_const_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cpu, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtou32((const char *)buf, 0, &value)) {
		cinst_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {
		mtk_cinst_const_mode(cpu, (value >> cpu) & 1);
	}

out:
	free_page((unsigned long)buf);

	return count;
}

static int cinst_ls_idx_sel_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cinst_ls_idx_sel_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cpu, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtou32((const char *)buf, 0, &value)) {
		cinst_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {
		mtk_cinst_ls_idx_sel(cpu, (value >> cpu) & 1);
	}

out:
	free_page((unsigned long)buf);

	return count;
}

static int cinst_bit_en_proc_show(struct seq_file *m, void *v)
{
	int cpu, ls_vx, cfg, param, cpu_tmp;
	unsigned int cinst_bit_en = 0;
	unsigned int cinst_bit_ls_en, cinst_bit_vx_en;

	unsigned char cinst_info[NR_CINST_CPU]
		[NR_CINST_CHANNEL][NR_CINST_CFG];

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {

		for (ls_vx = 0; ls_vx < NR_CINST_CHANNEL; ls_vx++) {
			for (cfg = 0; cfg < NR_CINST_CFG; cfg++) {
				param = ls_vx * NR_CINST_CFG + cfg;

				cpu_tmp = cpu-CINST_CPU_START_ID;

				/* read from atf */
				cinst_info[cpu_tmp][ls_vx][cfg] =
					cinst_smc_handle(
						CINST_RW_READ,
						cpu,
						param,
						0);

				cinst_msg(
					"Get cpu=%d ls_vx=%d cfg=%d value=%d\n",
					cpu, ls_vx, cfg,
					cinst_info[cpu_tmp][ls_vx][cfg]);
			}
		}
	}

	for (cpu = CINST_CPU_START_ID;
		cpu <= CINST_CPU_END_ID; cpu++) {

		cpu_tmp = cpu-CINST_CPU_START_ID;

		cinst_bit_ls_en =
			cinst_info[cpu_tmp]
				[CINST_CHANNEL_LS][CINST_CFG_ENABLE];
		cinst_bit_vx_en =
			cinst_info[cpu_tmp]
				[CINST_CHANNEL_VX][CINST_CFG_ENABLE];
		cinst_bit_en |= (cinst_bit_ls_en | cinst_bit_vx_en) << cpu;

		cinst_msg(
			"cinst_bit_ls_en(%d) cinst_bit_vx_en(%d) cinst_bit_en(%d)\n",
			cinst_bit_ls_en, cinst_bit_vx_en, cinst_bit_en);
	}

	seq_printf(m, "%d\n", cinst_bit_en);

	return 0;
}


PROC_FOPS_RW(cinst);
PROC_FOPS_RW(cinst_en);
PROC_FOPS_RW(cinst_const_mode);
PROC_FOPS_RW(cinst_ls_idx_sel);
PROC_FOPS_RO(cinst_bit_en);

int cinst_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry cinst_entries[] = {
		PROC_ENTRY(cinst),
		PROC_ENTRY(cinst_en),
		PROC_ENTRY(cinst_const_mode),
		PROC_ENTRY(cinst_ls_idx_sel),
		PROC_ENTRY(cinst_bit_en),
	};

	for (i = 0; i < ARRAY_SIZE(cinst_entries); i++) {
		if (!proc_create(cinst_entries[i].name,
			0660,
			dir,
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
	int cpu, ls_vx;

	node = pdev->dev.of_node;
	if (!node) {
		cinst_err("get cinst device node err\n");
		return -ENODEV;
	}

	/* enable control */
	rc = of_property_read_u32(node,
		"cinst_doe_enable", &cinst_doe_enable);

	if (!rc) {
		cinst_msg(
			"cinst_doe_enable from DTree; rc(%d) cinst_doe_enable(0x%x)\n",
			rc,
			cinst_doe_enable);

		if (cinst_doe_enable < 65536) {
			for (ls_vx = 0;
				ls_vx < NR_CINST_CHANNEL; ls_vx++) {
				for (cpu = CINST_CPU_START_ID;
					cpu <= CINST_CPU_END_ID; cpu++) {
					mtk_cinst(cpu, ls_vx,
						CINST_CFG_ENABLE,
						(cinst_doe_enable >>
						(ls_vx*
						(CINST_CPU_END_ID+1)
						+cpu))&1);
				}
			}
		}
	}

	/* constant mode control */
	rc = of_property_read_u32(node,
		"cinst_doe_const_mode", &cinst_doe_const_mode);

	if (!rc) {
		cinst_msg(
			"cinst_doe_const_mode from DTree; rc(%d) cinst_doe_const_mode(0x%x)\n",
			rc,
			cinst_doe_const_mode);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_const_mode < 256)
				mtk_cinst_const_mode(
				cpu,
				(cinst_doe_const_mode >> cpu) & 1);
		}
	}

	/* ls_idx_sel control */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_idx_sel", &cinst_doe_ls_idx_sel);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_idx_sel from DTree; rc(%d) cinst_doe_ls_idx_sel(0x%x)\n",
			rc,
			cinst_doe_ls_idx_sel);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_ls_idx_sel < 256)
				mtk_cinst_ls_idx_sel(
				cpu,
				(cinst_doe_ls_idx_sel >> cpu) & 1);
		}
	}

	/* cpux, ls_period */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_period", &cinst_doe_ls_period);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_period from DTree; rc(%d) cinst_doe_ls_period(0x%x)\n",
			rc,
			cinst_doe_ls_period);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_ls_period < 8)
				mtk_cinst(cpu, CINST_CHANNEL_LS,
					CINST_CFG_PERIOD,
					cinst_doe_ls_period);
		}
	}

	/* cpux, ls_credit */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_credit", &cinst_doe_ls_credit);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_credit from DTree; rc(%d) cinst_doe_ls_credit(0x%x)\n",
			rc,
			cinst_doe_ls_credit);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_ls_credit < 32)
				mtk_cinst(cpu, CINST_CHANNEL_LS,
					CINST_CFG_CREDIT,
					cinst_doe_ls_credit);
		}
	}

	/* cpux, ls_low_freq_period */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_low_freq_period",
		&cinst_doe_ls_low_freq_period);
	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_low_freq_period from DTree; rc(%d) cinst_doe_ls_low_freq_period(0x%x)\n",
			rc,
			cinst_doe_ls_low_freq_period);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_ls_low_freq_period < 8)
				mtk_cinst(cpu, CINST_CHANNEL_LS,
					CINST_CFG_LOW_PWR_PERIOD,
					cinst_doe_ls_low_freq_period);
		}
	}

	/* cpux, ls_low_freq_enable */
	rc = of_property_read_u32(node,
		"cinst_doe_ls_low_freq_enable",
		&cinst_doe_ls_low_freq_enable);

	if (!rc) {
		cinst_msg(
			"cinst_doe_ls_low_freq_enable from DTree; rc(%d) cinst_doe_ls_low_freq_enable(0x%x)\n",
			rc,
			cinst_doe_ls_low_freq_enable);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_ls_low_freq_enable < 2)
				mtk_cinst(cpu, CINST_CHANNEL_LS,
					CINST_CFG_LOW_PWR_ENABLE,
					cinst_doe_ls_low_freq_enable);
		}
	}

	/* cpux, vx_period */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_period", &cinst_doe_vx_period);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_period from DTree; rc(%d) cinst_doe_vx_period(0x%x)\n",
			rc,
			cinst_doe_vx_period);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_vx_period < 8)
				mtk_cinst(cpu, CINST_CHANNEL_VX,
					CINST_CFG_PERIOD,
					cinst_doe_vx_period);
		}
	}

	/* cpux, vx_credit */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_credit", &cinst_doe_vx_credit);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_credit from DTree; rc(%d) cinst_doe_vx_credit(0x%x)\n",
			rc,
			cinst_doe_vx_credit);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_vx_credit < 32)
				mtk_cinst(cpu, CINST_CHANNEL_VX,
					CINST_CFG_CREDIT,
					cinst_doe_vx_credit);
		}
	}

	/* cpux, vx_low_freq_period */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_low_freq_period",
		&cinst_doe_vx_low_freq_period);
	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_low_freq_period from DTree; rc(%d) cinst_doe_vx_low_freq_period(0x%x)\n",
			rc,
			cinst_doe_vx_low_freq_period);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_vx_low_freq_period < 8)
				mtk_cinst(cpu, CINST_CHANNEL_VX,
					CINST_CFG_LOW_PWR_PERIOD,
					cinst_doe_vx_low_freq_period);
		}
	}

	/* cpux, vx_low_freq_enable */
	rc = of_property_read_u32(node,
		"cinst_doe_vx_low_freq_enable",
		&cinst_doe_vx_low_freq_enable);

	if (!rc) {
		cinst_msg(
			"cinst_doe_vx_low_freq_enable from DTree; rc(%d) cinst_doe_vx_low_freq_enable(0x%x)\n",
			rc,
			cinst_doe_vx_low_freq_enable);

		for (cpu = CINST_CPU_START_ID;
			cpu <= CINST_CPU_END_ID; cpu++) {

			if (cinst_doe_vx_low_freq_enable < 2)
				mtk_cinst(cpu, CINST_CHANNEL_VX,
					CINST_CFG_LOW_PWR_ENABLE,
					cinst_doe_vx_low_freq_enable);
		}
	}

	/* dump reg status into PICACHU dram for DB */
	cinst_reserve_memory_dump(
		cinst_buf, cinst_mem_size, CINST_TRIGGER_STAGE_PROBE);

	cinst_msg("cinst probe ok!!\n");
#endif
#endif
	return 0;
}

int cinst_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	cinst_reserve_memory_dump(
		cinst_buf+0x1000, cinst_mem_size, CINST_TRIGGER_STAGE_SUSPEND);
#endif
#endif
	return 0;
}

int cinst_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	cinst_reserve_memory_dump(
		cinst_buf+0x2000, cinst_mem_size, CINST_TRIGGER_STAGE_RESUME);
#endif
#endif
	return 0;
}

MODULE_DESCRIPTION("MediaTek CINST Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_CINST_C__
