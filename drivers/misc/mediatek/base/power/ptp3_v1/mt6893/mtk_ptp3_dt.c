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
 * @file	mkt_ptp3_dt.c
 * @brief   Driver for dt
 *
 */

#define __MTK_DT_C__

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
	#include "mtk_ptp3_pdp.h"
	#include "mtk_ptp3_dt.h"
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
#define ARMDT_DEBUG
#define DT_TAG	 "[DT]"

#define dt_err(fmt, args...)	\
	pr_info(DT_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define dt_msg(fmt, args...)	\
	pr_info(DT_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef ARMDT_DEBUG
#define dt_debug(fmt, args...) \
	pr_debug(DT_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define dt_debug(fmt, args...)
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
/* B-DOE use: dt func. */
static unsigned int dt_state;
static unsigned int dt_state_pinctl;

#endif
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

static char *dt_buf;
static unsigned long long dt_mem_size;
void dt_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	dt_buf = buf;
	dt_mem_size = ptp3_mem_size;
}
/* xxxx merged from dt driver */
int dt_reserve_memory_dump(char *buf,  unsigned long long ptp3_mem_size,
	enum DT_TRIGGER_STAGE dt_tri_stage)
{
	int str_len = 0;
	unsigned int dt_n, reg_value, pinctl_reg_value, dt_thr = 0;

	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		dt_msg("unable to get free page!\n");
		return -1;
	}
	dt_msg("[DT]buf: 0x%llx, [DT]aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* collect dump info */
		for (dt_n = DT_B_START_ID; dt_n < DT_NUM; dt_n++) {
			reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
						PTP3_FEATURE_DT,
						DT_RW_READ,
						0,
						dt_n);
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ptp3_mem_size - str_len,
				"[DT][CPU%d]DT_EN : %d\n",
				dt_n,
				reg_value);
			}
			reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
						PTP3_FEATURE_DT,
						DT_RW_READ,
						0,
						DT_PINCTL_BIT);
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ptp3_mem_size - str_len,
				"[DT]DT_PINCTL_EN : %d\n",
				reg_value);
		for (dt_n = DT_THR_START_BIT ; dt_n >= DT_THR_END_BIT ; dt_n++) {
			reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
						PTP3_FEATURE_DT,
						DT_RW_READ,
						0,
						dt_n);
			dt_thr |= reg_value << dt_n;
			}
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ptp3_mem_size - str_len,
				"DT_THR = %d\n",
				dt_thr);
		for (dt_n = DT_B_START_ID; dt_n < DT_NUM; dt_n++) {
			pinctl_reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
						PTP3_FEATURE_DT,
						DT_RW_PINCTL_READ,
						0,
						dt_n);
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ptp3_mem_size - str_len,
				"[DT][CPU%d]DT_THR : %d\n",
				dt_n,
				pinctl_reg_value);
			}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	dt_debug("\n%s", aee_log_buf);
	dt_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif
#endif

/************************************************
 * set DT status by procfs interface
 ************************************************/
static ssize_t dt_dump_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int input, dt_n, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &input)) {
		dt_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}
		for (dt_n = 0; dt_n < DT_NUM; dt_n++) {
			value = (input >> dt_n) & 0x1;
			ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_WRITE, value, dt_n);
		}

out:
	free_page((unsigned long)buf);
	return count;
}

static int dt_dump_proc_show(struct seq_file *m, void *v)
{

	unsigned int status = 0, dt_n, dt_thr = 0, cpu_n;

	for (cpu_n = DT_B_START_ID ; cpu_n < DT_NUM ; cpu_n++) {
		for (dt_n = DT_THR_START_BIT ; dt_n <= DT_THR_END_BIT ; dt_n++) {
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, dt_n);
			dt_thr |= status << dt_n;
		}
		seq_printf(m, "[DT][CPU%d]", cpu_n);
		dt_debug("[DT][CPU%d]", cpu_n);
	switch (dt_thr) {
	case p0_disp_red:
		seq_printf(m, "THR : %d (0%% dispatch reduction(inactive)), ", dt_thr);
		dt_debug("THR : %d (0%% dispatch reduction(inactive)), ", dt_thr);
		break;
	case p12_disp_red:
		seq_printf(m, "THR : %d (12%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (12%% dispatch reduction), ", dt_thr);
		break;
	case p25_disp_red:
		seq_printf(m, "THR : %d (25%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (25%% dispatch reduction), ", dt_thr);
		break;
	case p30_disp_red:
		seq_printf(m, "THR : %d (30%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (30%% dispatch reduction), ", dt_thr);
		break;
	case p35_disp_red:
		seq_printf(m, "THR : %d (35%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (35%% dispatch reduction), ", dt_thr);
		break;
	case p40_disp_red:
		seq_printf(m, "THR : %d (40%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (40%% dispatch reduction), ", dt_thr);
		break;
	case p75_disp_red:
		seq_printf(m, "THR : %d (75%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (75%% dispatch reduction), ", dt_thr);
		break;
	case p75_disp_red_pinctl_is_p100:
		seq_printf(m, "THR : %d (75%% dispatch reduction), ", dt_thr);
		dt_debug("THR : %d (75%% dispatch reduction), ", dt_thr);
		break;
	default:
		break;
		}
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, DT_PINCTL_BIT);
			seq_printf(m, "PINCTL : %d, ", status);
			dt_debug("PINCTL : %d, ", status);
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, cpu_n);
			seq_printf(m, "DT_EN : %d, ", status);
			dt_debug("DT_EN : %d, ", status);
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_PINCTL_READ, 0, cpu_n);
		switch (status) {
		case p0_disp_red:
	seq_printf(m, "PINCTL_THR : %d (0%% dispatch reduction(inactive))\n", status);
			dt_debug("PINCTL_THR : %d (0%% dispatch reduction(inactive))\n", status);
			break;
		case p12_disp_red:
			seq_printf(m, "PINCTL_THR : %d (12%% dispatch reduction)\n", status);
			dt_debug("PINCTL_THR : %d (12%% dispatch reduction)\n", status);
			break;
		case p25_disp_red:
			seq_printf(m, "PINCTL_THR : %d (25%% dispatch reduction)\n", status);
			dt_debug("PINCTL_THR : %d (25%% dispatch reduction)\n", status);
			break;
		case p30_disp_red:
			seq_printf(m, "PINCTL_THR : %d (30%% dispatch reduction)\n", status);
			dt_debug("PINCTL_THR : %d (30%% dispatch reduction)\n", status);
			break;
		case p35_disp_red:
			seq_printf(m, "PINCTL_THR : %d (35%% dispatch reduction)\n", status);
			dt_debug("PINCTL_THR : %d (35%% dispatch reduction)\n", status);
			break;
		case p40_disp_red:
			seq_printf(m, "PINCTL_THR : %d (40%% dispatch reduction)\n", status);
			dt_debug("PINCTL_THR : %d (40%% dispatch reduction)\n", status);
			break;
		case p75_disp_red:
			seq_printf(m, "PINCTL_THR : %d (75%% dispatch reduction)\n", status);
			dt_debug("PINCTL_THR : %d (75%% dispatch reduction)\n", status);
			break;
		case p75_disp_red_pinctl_is_p100:
			seq_printf(m, "PINCTL_THR : %d (100%% drive DT)\n", status);
			dt_debug("PINCTL_THR : %d (100%% drive DT)\n", status);
			break;
	}
		}
#if 0
	for (dt_n = DT_NUM ; dt_n < DT_DEBUG_BIT ; dt_n++) {
		status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, dt_n);
		seq_printf(m, "DT Debug Bits%d: %d\n", dt_n, status);
	}
#endif
	return 0;
}
static int dt_cfg_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0, pinctl_status = 0, dt_thr = 0, dt_n;
	unsigned int cpu_n, dt_cfg = 0, dt_cfg_flag = 0;

	for (cpu_n = DT_B_START_ID ; cpu_n < DT_NUM ; cpu_n++) {
		for (dt_n = DT_THR_START_BIT ; dt_n <= DT_THR_END_BIT ; dt_n++) {
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, dt_n);
			dt_thr |= status << dt_n;
		}
		pinctl_status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, DT_PINCTL_BIT);
		if (pinctl_status == 1) {
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_PINCTL_READ, 0, cpu_n);
			if (status != 0)
				dt_cfg |= status << (cpu_n * 4);
				/* seq_printf(m, "%d",status); */
			else
				dt_cfg |= 0 << (cpu_n * 4);
				/* seq_printf(m, "0"); */
		} else {
			status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, cpu_n);
			if (status == 1)
				dt_cfg |= dt_thr << (cpu_n * 4);
				/* seq_printf(m, "%d",dt_thr); */
			else
				dt_cfg |= 0 << (cpu_n * 4);
				/* seq_printf(m, "0"); */
		}
		dt_cfg_flag = 1;

	}
		if (dt_cfg_flag == 1)
			seq_printf(m, "%08x\n", dt_cfg);
		else
			seq_puts(m, "XXXX0000\n");
	return 0;
}


static ssize_t dt_pinctl_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int dt_n, input;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &input)) {
		dt_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (dt_n = DT_B_START_ID ; dt_n < DT_NUM ; dt_n++)
		ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_PINCTL_WRITE, input, dt_n);


out:
	free_page((unsigned long)buf);
	return count;
}

static int dt_pinctl_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0, dt_n;

	for (dt_n = DT_B_START_ID; dt_n <= DT_END_ID; dt_n++) {
		status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_READ, 0, DT_PINCTL_BIT);
		seq_printf(m, "[DT][CPU%d]PINCTL : %d, ", dt_n, status);
		dt_debug("[DT][CPU%d]PINCTL : %d, ", dt_n, status);

		status = ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_PINCTL_READ, 0, dt_n);
	switch (status) {
	case p0_disp_red:
		seq_printf(m, "THR : %d (0%% dispatch reduction(inactive))\n", status);
		dt_debug("THR : %d (0%% dispatch reduction(inactive))\n", status);
		break;
	case p12_disp_red:
		seq_printf(m, "THR : %d (12%% dispatch reduction)\n", status);
		dt_debug("THR : %d (12%% dispatch reduction)\n", status);
		break;
	case p25_disp_red:
		seq_printf(m, "THR : %d (25%% dispatch reduction)\n", status);
		dt_debug("THR : %d (25%% dispatch reduction)\n", status);
		break;
	case p30_disp_red:
		seq_printf(m, "THR : %d (30%% dispatch reduction)\n", status);
		dt_debug("THR : %d (30%% dispatch reduction)\n", status);
		break;
	case p35_disp_red:
		seq_printf(m, "THR : %d (35%% dispatch reduction)\n", status);
		dt_debug("THR : %d (35%% dispatch reduction)\n", status);
		break;
	case p40_disp_red:
		seq_printf(m, "THR : %d (40%% dispatch reduction)\n", status);
		dt_debug("THR : %d (40%% dispatch reduction)\n", status);
		break;
	case p75_disp_red:
		seq_printf(m, "THR : %d (75%% dispatch reduction)\n", status);
		dt_debug("THR : %d (75%% dispatch reduction)\n", status);
		break;
	case p75_disp_red_pinctl_is_p100:
		seq_printf(m, "THR : %d (100%% drive DT)\n", status);
		dt_debug("THR : %d (100%% drive DT)\n", status);
		break;
	default:
		break;
	}
		}

	return 0;
}


PROC_FOPS_RW(dt_dump);
PROC_FOPS_RO(dt_cfg);
PROC_FOPS_RW(dt_pinctl);



int dt_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *dt_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry dt_entries[] = {
		PROC_ENTRY(dt_dump),
		PROC_ENTRY(dt_cfg),
		PROC_ENTRY(dt_pinctl),

	};

	dt_dir = proc_mkdir("dt", dir);
	if (!dt_dir) {
		dt_err("[%s]: mkdir /proc/dt failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dt_entries); i++) {
		if (!proc_create(dt_entries[i].name,
			0660,
			dt_dir,
			dt_entries[i].fops)) {
			dt_err("[%s]: create /proc/%s/dt/%s failed\n",
				__func__,
				proc_name,
				dt_entries[i].name);
			return -3;
		}
	}
	return 0;
}
int dt_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int dt_n = 0;

	node = pdev->dev.of_node;
	if (!node) {
		dt_err("get dt device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "dt_state", &dt_state);
	if (!rc) {
		dt_debug("[xxxxdt] state from DTree; rc(%d) dt_state(0x%x)\n",
			rc,
			dt_state);

		for (dt_n = 0; dt_n < DT_NUM; dt_n++)
			ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_WRITE,
				(dt_state >> dt_n) & 0x1, dt_n);
	}
	rc = of_property_read_u32(node, "dt_state_pinctl", &dt_state_pinctl);
	if (!rc) {
		dt_debug("[xxxxdt] state from DTree; rc(%d) dt_state_pinctl(0x%x)\n",
			rc,
			dt_state_pinctl);

		for (dt_n = 0; dt_n < DT_NUM; dt_n++)
			ptp3_smc_handle(PTP3_FEATURE_DT, DT_RW_PINCTL_WRITE,
				(dt_state_pinctl >> dt_n) & 0x1, dt_n);
	}
#endif /* CONFIG_OF */
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (dt_buf != NULL) {
		dt_reserve_memory_dump(dt_buf+0x1000, dt_mem_size,
			DT_TRIGGER_STAGE_PROBE);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}
int dt_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (dt_buf != NULL) {
		dt_reserve_memory_dump(dt_buf+0x1000, dt_mem_size,
			DT_TRIGGER_STAGE_SUSPEND);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int dt_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (dt_buf != NULL) {
		dt_reserve_memory_dump(dt_buf+0x2000, dt_mem_size,
			DT_TRIGGER_STAGE_RESUME);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

MODULE_DESCRIPTION("MediaTek DT Driver v1p1");
MODULE_LICENSE("GPL");

#undef __MTK_DT_C__
