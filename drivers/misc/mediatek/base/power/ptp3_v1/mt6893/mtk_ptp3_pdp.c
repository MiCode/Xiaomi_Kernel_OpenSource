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
 * @file	mkt_ptp3_pdp.c
 * @brief   Driver for pdp
 *
 */

#define __MTK_PDP_C__

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
#define PDP_DEBUG
#define PDP_TAG	 "[PDP]"

#define pdp_err(fmt, args...)	\
	pr_info(PDP_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define pdp_msg(fmt, args...)	\
	pr_info(PDP_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef PDP_DEBUG
#define pdp_debug(fmt, args...) \
	pr_debug(PDP_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define pdp_debug(fmt, args...)
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
/* B-DOE use: pdp func. */
static unsigned int pdp_state;
static unsigned int pdp_state_pinctl;

#endif
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

static char *pdp_buf;
static unsigned long long pdp_mem_size;
void pdp_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	pdp_buf = buf;
	pdp_mem_size = ptp3_mem_size;
}
/* xxxx merged from pdp driver */
int pdp_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum PDP_TRIGGER_STAGE pdp_tri_stage)
{
	int str_len = 0;
	unsigned int pdp_n, reg_value, pinctl_reg_value;

	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		pdp_msg("unable to get free page!\n");
		return -1;
	}
	pdp_msg("[PDP]buf: 0x%llx, [PDP]aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* collect dump info */
		for (pdp_n = PDP_B_START_ID-1; pdp_n < PDP_NUM; pdp_n++) {
			reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
						PTP3_FEATURE_PDP,
						PDP_RW_READ,
						0,
						pdp_n);
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ptp3_mem_size - str_len,
				"[PDP][CPU%d]PDP_EN : %d\n",
				pdp_n,
				reg_value);
			}
		for (pdp_n = PDP_B_START_ID; pdp_n < PDP_NUM; pdp_n++) {
			pinctl_reg_value = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
						PTP3_FEATURE_PDP,
						PDP_RW_PINCTL_READ,
						0,
						pdp_n);
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ptp3_mem_size - str_len,
				"[PDP][CPU%d]PDP_PINCTL_EN : %d\n",
				pdp_n,
				pinctl_reg_value);
			}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	pdp_debug("\n%s", aee_log_buf);
	pdp_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif
#endif

/************************************************
 * set PDP status by procfs interface
 ************************************************/
static ssize_t pdp_dump_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int input, pdp_n, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &input)) {
		pdp_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}
		for (pdp_n = 0; pdp_n < PDP_NUM; pdp_n++) {
			value = (input >> pdp_n) & 0x1;
			ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_WRITE, value, pdp_n);
		}

out:
	free_page((unsigned long)buf);
	return count;
}

static int pdp_dump_proc_show(struct seq_file *m, void *v)
{

	unsigned int status = 0, pdp_n, cpu_n;

	for (cpu_n = PDP_B_START_ID; cpu_n < PDP_NUM; cpu_n++) {
		seq_printf(m, "[PDP][CPU%d]", cpu_n);
		pdp_debug("[PDP][CPU%d] ", cpu_n);
		for (pdp_n = 0; pdp_n < PDP_B_START_ID; pdp_n++) {
			status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, pdp_n);
		switch (pdp_n) {
		case PDP_DIS_IQ:
			seq_printf(m, "DIS_IQ : %d, ", status);
			pdp_debug("DIS_IQ : %d, ", status);
			break;
		case PDP_DIS_LSINH:
			seq_printf(m, "DIS_LSINH : %d, ", status);
			pdp_debug("DIS_LSINH : %d, ", status);
			break;
		case PDP_DIS_SPEC:
			seq_printf(m, "DIS_SPEC : %d, ", status);
			pdp_debug("DIS_SPEC : %d, ", status);
			break;
		case PDPPINCTL:
			seq_printf(m, "PINCTL : %d, ", status);
			pdp_debug("PINCTL : %d, ", status);
			status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, cpu_n);
			seq_printf(m, "PDP_EN : %d, ", status);
			pdp_debug("PDP_EN : %d, ", status);
			status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_PINCTL_READ, 0, cpu_n);
			seq_printf(m, "PDP_PINCTL_EN : %d\n", status);
			pdp_debug("PDP_PINCTL_EN : %d\n", status);
			break;
		default:
			break;
				}
			}
		}

#if 1
	for (pdp_n = PDP_NUM; pdp_n < PDP_DEBUG_BIT; pdp_n++) {
		status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, pdp_n);
		seq_printf(m, "PDP Debug Bits%d: %d\n", pdp_n, status);
	}
#endif
	return 0;
}

static int pdp_cfg_proc_show(struct seq_file *m, void *v)
{

	unsigned int pdp_n, status = 0, cpu_n, pinctl_status = 0, pdp_subfeature_en = 0;
	unsigned int pdp_config = 0, pdp_cfg = 0, pdp_cfg_flag = 1;

	for (pdp_n = 0 ; pdp_n < 3 ; pdp_n++) {
		status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, pdp_n);
		pdp_subfeature_en |= status << pdp_n;
		}
	for (cpu_n = PDP_B_START_ID; cpu_n < PDP_NUM; cpu_n++) {
		pinctl_status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, PDP_PINCTL_BIT);
		if (pinctl_status == 1) {
			status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_PINCTL_READ, 0, cpu_n);
			if (status == 1) {
				pdp_config = PDP_ALL_ON;
				pdp_cfg |= pdp_config << (cpu_n * 4);
				//seq_printf(m, "%d",pdp_config);
			} else {
				pdp_config = PDP_ALL_OFF;
				pdp_cfg |= pdp_config << (cpu_n * 4);
				//seq_printf(m, "%d",pdp_config);
				}

		} else {
			status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, cpu_n);
			if (status == 1) {
				if (pdp_subfeature_en == PDP_ALLFEATURE_ON) {
					pdp_config = PDP_ALL_ON;
					pdp_cfg |= pdp_config << (cpu_n * 4);
					//seq_printf(m, "%d",pdp_config);
				} else if (pdp_subfeature_en == PDP_DISABLE_DIS_LSINH) {
					pdp_config = PDP_DISABLE_SUB2;
					pdp_cfg |= pdp_config << (cpu_n * 4);
					//seq_printf(m, "%d",pdp_config);
				} else {
					pdp_cfg_flag = 0;
					break;
				}

			} else {
				pdp_config = PDP_ALL_OFF;
				pdp_cfg |= pdp_config << (cpu_n * 4);
				//seq_printf(m, "%d",pdp_config);
			}

		}
	}
	if (pdp_cfg_flag == 1) {
		seq_printf(m, "%08x\n", pdp_cfg);
		pdp_debug("%08x\n", pdp_cfg);
	} else {
		seq_puts(m, "XXXX0000\n");
		pdp_debug("XXXX0000\n");
	}


	return 0;
}

static ssize_t pdp_pinctl_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int pdp_n, value, input;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &input)) {
		pdp_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}
		for (pdp_n = 0; pdp_n < PDP_NUM; pdp_n++) {
			value = (input >> pdp_n) & 0x1;
			ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_PINCTL_WRITE, value, pdp_n);
			}

out:
	free_page((unsigned long)buf);
	return count;
}

static int pdp_pinctl_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0, pdp_n;

	for (pdp_n = PDP_B_START_ID; pdp_n <= PDP_END_ID; pdp_n++) {
		status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_READ, 0, PDP_PINCTL_BIT);
		seq_printf(m, "[PDP][CPU%d] PDP_ARM_PINCTL_EN : %d, ", pdp_n, status);
		pdp_debug("[PDP][CPU%d] PDP_ARM_PINCTL_EN : %d, ", pdp_n, status);
		status = ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_PINCTL_READ, 0, pdp_n);
		seq_printf(m, "PDP_PINCTL_EN : %d\n", status);
		pdp_debug("PDP_PINCTL_EN : %d\n", status);
		}

	return 0;
}


PROC_FOPS_RW(pdp_dump);
PROC_FOPS_RO(pdp_cfg);
PROC_FOPS_RW(pdp_pinctl);


int pdp_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *pdp_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry pdp_entries[] = {
		PROC_ENTRY(pdp_dump),
		PROC_ENTRY(pdp_cfg),
		PROC_ENTRY(pdp_pinctl),

	};

	pdp_dir = proc_mkdir("pdp", dir);
	if (!pdp_dir) {
		pdp_err("[%s]: mkdir /proc/pdp failed\n",
			__func__);
			return -1;
	}

	for (i = 0; i < ARRAY_SIZE(pdp_entries); i++) {
		if (!proc_create(pdp_entries[i].name,
			0660,
			pdp_dir,
			pdp_entries[i].fops)) {
			pdp_err("[%s]: create /proc/%s/pdp/%s failed\n",
				__func__,
				proc_name,
				pdp_entries[i].name);
			return -3;
		}
	}
	return 0;
}
int pdp_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int pdp_n = 0;

	node = pdev->dev.of_node;
	if (!node) {
		pdp_err("get pdp device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "pdp_state", &pdp_state);
	if (!rc) {
		pdp_debug("[xxxxpdp] state from DTree; rc(%d) pdp_state(0x%x)\n",
			rc,
			pdp_state);

		for (pdp_n = 0; pdp_n < PDP_NUM; pdp_n++)
			ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_WRITE,
			(pdp_state >> pdp_n) & 0x1, pdp_n);
	}

	rc = of_property_read_u32(node, "pdp_state_pinctl", &pdp_state_pinctl);
	if (!rc) {
		pdp_debug("[xxxxpdp] state from DTree; rc(%d) pdp_state_pinctl(0x%x)\n",
			rc,
			pdp_state_pinctl);

		for (pdp_n = 0; pdp_n < PDP_NUM; pdp_n++)
			ptp3_smc_handle(PTP3_FEATURE_PDP, PDP_RW_PINCTL_WRITE,
			(pdp_state_pinctl >> pdp_n) & 0x1, pdp_n);
	}
#endif /* CONFIG_OF */
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (pdp_buf != NULL) {
		pdp_reserve_memory_dump(pdp_buf, pdp_mem_size,
				PDP_TRIGGER_STAGE_PROBE);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;

}
int pdp_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (pdp_buf != NULL) {
		pdp_reserve_memory_dump(pdp_buf+0x1000, pdp_mem_size,
				PDP_TRIGGER_STAGE_SUSPEND);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int pdp_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (pdp_buf != NULL) {
		pdp_reserve_memory_dump(pdp_buf+0x2000, pdp_mem_size,
				PDP_TRIGGER_STAGE_RESUME);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

MODULE_DESCRIPTION("MediaTek PDP Driver v1p1");
MODULE_LICENSE("GPL");

#undef __MTK_PDP_C__
