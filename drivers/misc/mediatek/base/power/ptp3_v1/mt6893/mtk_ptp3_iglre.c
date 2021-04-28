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
 * @file	mkt_ptp3_iglre.c
 * @brief   Driver for iglre
 *
 */

 #define __MTK_IGLRE_C__

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
	#include "mtk_ptp3_iglre.h"
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
#define IGLRE_TAG	 "[xxxxIGLRE]"

#define iglre_info(fmt, args...)	\
	pr_info(IGLRE_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#define iglre_debug(fmt, args...) \
	pr_debug(IGLRE_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

#ifdef PTP3_STATUS_PROBE_DUMP
static char *iglre_buf;
static unsigned long long iglre_mem_size;
void iglre_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	iglre_buf = buf;
	iglre_mem_size = ptp3_mem_size;
}

int iglre_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum IGLRE_TRIGGER_STAGE iglre_tri_stage)
{
	int str_len = 0;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		iglre_info("unable to get free page!\n");
		return -1;
	}
	iglre_info("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* show trigger stage */
	switch (iglre_tri_stage) {
	case IGLRE_TRIGGER_STAGE_PROBE:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Probe]\n");
		break;
	case IGLRE_TRIGGER_STAGE_SUSPEND:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Suspend]\n");
		break;
	case IGLRE_TRIGGER_STAGE_RESUME:
		str_len += snprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Resume]\n");
		break;
	default:
		iglre_debug("illegal IGLRE_TRIGGER_STAGE\n");
		break;
	}
	/* collect dump info */

	/* I-Chang need to fix */


	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	iglre_debug("\n%s", aee_log_buf);
	iglre_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}
#endif

#endif
#endif

/************************************************
 * SMC between kernel and atf
 ************************************************/
static unsigned int iglre_smc_handle(unsigned int key,
	unsigned int val, unsigned int cpu)
{
	unsigned int ret;

	iglre_info("[%s]:key(%d) val(%d) cpu(%d)\n",
		__func__, key, val, cpu);

	/* update atf via smc */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_IGLRE,
		key,
		val,
		cpu);

	return ret;
}

/************************************************
 * static function
 ************************************************/
static void mtk_ig_cfg(unsigned int onOff,
		unsigned int iglre_n)
{
	const unsigned int iglre_key = IG_CFG;
	/* update via atf */
	iglre_smc_handle(iglre_key, onOff, iglre_n);
}

static void mtk_lre_cfg(unsigned int onOff,
		unsigned int iglre_n)
{
	const unsigned int iglre_key = LRE_CFG;
	/* update via atf */
	iglre_smc_handle(iglre_key, onOff, iglre_n);
}

/************************************************
 * set IGLRE status by procfs interface
 ************************************************/
static ssize_t ig_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable, iglre_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		iglre_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++)
		mtk_ig_cfg((enable >> iglre_n) & 0x10, iglre_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ig_cfg_proc_show(struct seq_file *m, void *v)
{
	int status = 0, value, iglre_n = 0;
	const unsigned int iglre_key = IG_R_CFG;

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++) {
		value = iglre_smc_handle(iglre_key, 0, iglre_n);
		status = status | ((value & 0x01) << (iglre_n * 4));
	}

	seq_printf(m, "%08x\n", status << 16);

	return 0;
}

static ssize_t lre_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable, iglre_n;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		iglre_debug("bad argument!! Should be \"0\" ~ \"255\"\n");
		goto out;
	}

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++)
		mtk_lre_cfg((enable >> iglre_n) & 0x10, iglre_n);

out:
	free_page((unsigned long)buf);
	return count;
}

static int lre_cfg_proc_show(struct seq_file *m, void *v)
{
	int status = 0, value, iglre_n = 0;
	const unsigned int iglre_key = LRE_R_CFG;

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++) {
		value = iglre_smc_handle(iglre_key, 0, iglre_n);
		status = status | ((value & 0x01) << (iglre_n * 4));
	}

	seq_printf(m, "%08x\n", status << 16);

	return 0;
}

static int iglre_dump_proc_show(struct seq_file *m, void *v)
{
	struct iglre_class iglre;
	unsigned int *value = (unsigned int *)&iglre;
	unsigned int iglre_n = 0;
	const unsigned int iglre_key = IGLRE_R;

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++) {
		*value = iglre_smc_handle(iglre_key, DBG_CONTROL, iglre_n);

		seq_printf(m, "[IGLRE][CPU%d]", iglre_n + 4);
		seq_printf(m, " mem_ig_en:%d,",
			iglre.mem_ig_en);
		seq_printf(m, " mem_lre_en:%d,\n",
			iglre.mem_lre_en);
	}
	return 0;
}


PROC_FOPS_RW(ig_cfg);
PROC_FOPS_RW(lre_cfg);
PROC_FOPS_RO(iglre_dump);

int iglre_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *iglre_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry iglre_entries[] = {
		PROC_ENTRY(ig_cfg),
		PROC_ENTRY(lre_cfg),
		PROC_ENTRY(iglre_dump),
	};

	iglre_dir = proc_mkdir("iglre", dir);
	if (!iglre_dir) {
		iglre_debug("[%s]: mkdir /proc/iglre failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(iglre_entries); i++) {
		if (!proc_create(iglre_entries[i].name,
			0664,
			iglre_dir,
			iglre_entries[i].fops)) {
			iglre_debug("[%s]: create /proc/%s/iglre/%s failed\n",
				__func__,
				proc_name,
				iglre_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int iglre_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	unsigned int igBitEn, lreBitEn;

	struct device_node *node = NULL;
	int rc = 0;
	unsigned int iglre_n;

	node = pdev->dev.of_node;
	if (!node) {
		iglre_debug("get iglre device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "igBitEn", &igBitEn);
	if (!rc) {
		iglre_debug("[xxxx_iglre] DTree ErrCode(%d), igBitEn(0x%x)\n",
			rc,
			igBitEn);

		for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++)
			mtk_ig_cfg((igBitEn >> iglre_n) & 0x10, iglre_n);
	}

	rc = of_property_read_u32(node, "lreBitEn", &lreBitEn);
	if (!rc) {
		iglre_debug("[xxxx_iglre] DTree ErrCode(%d), lreBitEn(0x%x)\n",
			rc,
			lreBitEn);

		for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++)
			mtk_lre_cfg((lreBitEn >> iglre_n) & 0x10, iglre_n);
	}
#endif /* CONFIG_OF */

#ifdef CONFIG_OF_RESERVED_MEM
#ifdef PTP3_STATUS_PROBE_DUMP
	/* dump reg status into PICACHU dram for DB */
	if (iglre_buf != NULL) {
		iglre_reserve_memory_dump(iglre_buf, iglre_mem_size,
		IGLRE_TRIGGER_STAGE_PROBE);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* PTP3_STATUS_PROBE_DUMP */

#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int iglre_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int iglre_resume(struct platform_device *pdev)
{
	return 0;
}

MODULE_DESCRIPTION("MediaTek IGLRE Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_IGLRE_C__


