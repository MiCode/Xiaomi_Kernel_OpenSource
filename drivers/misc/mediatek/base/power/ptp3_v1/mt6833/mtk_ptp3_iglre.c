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
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		PTP3_FEATURE_IGLRE,
		key,
		val,
		cpu);

	return ret;
}

/************************************************
 * IPI between kernel and mcupm/cpu_eb
 ************************************************/
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
static void iglre_ipi_handle(unsigned int cfg, unsigned int val)
{
	struct ptp3_ipi_data iglre_data;

	iglre_data.cmd = PTP3_IPI_IGLRE;
	iglre_data.u.iglre.cfg = cfg;
	iglre_data.u.iglre.val = val;

	iglre_info("[%s]: cfg(%d) val(%d)\n",
		__func__, cfg, val);

	/* update mcupm or cpueb via ipi */
	while (ptp3_ipi_handle(&iglre_data) != 0)
		udelay(500);
}
#else
static void iglre_ipi_handle(unsigned int param, unsigned int val)
{
	iglre_info("IPI from kernel to MCUPM not exist\n");
}
#endif

/************************************************
 * static function
 ************************************************/
static void mtk_ig_cfg(unsigned int onOff)
{
	const unsigned int iglre_key = IG_CFG;

	iglre_ipi_handle(iglre_key, onOff);
}

static void mtk_lre_cfg(unsigned int onOff)
{
	const unsigned int iglre_key = LRE_CFG;

	iglre_ipi_handle(iglre_key, onOff);
}

static void mtk_byte_cfg(unsigned int onOff)
{
	const unsigned int iglre_key = BYTE_CFG;

	iglre_ipi_handle(iglre_key, onOff);
}

static void mtk_rcan_cfg(unsigned int onOff)
{
	const unsigned int iglre_key = RCAN_CFG;

	iglre_ipi_handle(iglre_key, onOff);
}

/************************************************
 * set IGLRE status by procfs interface
 ************************************************/
static ssize_t ig_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable;
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

	if (enable > 0)
		mtk_ig_cfg((enable % 2) ? 1 : 0);

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

	seq_printf(m, "%08x\n", status << 24); /* add 6L status */

	return 0;
}

static ssize_t lre_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable;
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

	if (enable > 0)
		mtk_lre_cfg((enable % 2) ? 1 : 0);

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
		status = status | (value << (iglre_n * 4));
	}

	seq_printf(m, "%08x\n", status << 24);

	return 0;
}

static ssize_t byte_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable;
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

	if (enable > 0)
		mtk_byte_cfg((enable % 2) ? 1 : 0);

out:
	free_page((unsigned long)buf);
	return count;
}

static int byte_cfg_proc_show(struct seq_file *m, void *v)
{
	int status = 0, value, iglre_n = 0;
	const unsigned int iglre_key = BYTE_R_CFG;

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++) {
		value = iglre_smc_handle(iglre_key, 0, iglre_n);
		status = status | ((value & 0x01) << (iglre_n * 4));
	}

	seq_printf(m, "%08x\n", status << 24);

	return 0;
}

static ssize_t rcan_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable;
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

	if (enable > 0)
		mtk_rcan_cfg((enable % 2) ? 1 : 0);

out:
	free_page((unsigned long)buf);
	return count;
}

static int rcan_cfg_proc_show(struct seq_file *m, void *v)
{
	int status = 0, value, iglre_n = 0;
	const unsigned int iglre_key = RCAN_R_CFG;

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++) {
		value = iglre_smc_handle(iglre_key, 0, iglre_n);
		status = status | ((value & 0x01) << (iglre_n * 4));
	}

	seq_printf(m, "%08x\n", status << 24);

	return 0;
}

static int iglre_dump_proc_show(struct seq_file *m, void *v)
{
	struct iglre_class iglre;
	unsigned int *value = (unsigned int *)&iglre;
	unsigned int iglre_n = 0;
	const unsigned int iglre_key = IGLRE_R;

	for (iglre_n = 0; iglre_n < IGLRE_NUM; iglre_n++) {
		value[0] = iglre_smc_handle(iglre_key, IGLRE_CONTROL, iglre_n);
		value[1] = iglre_smc_handle(iglre_key, SPWR_CONTROL, iglre_n);

		seq_printf(m, "[IGLRE][CPU%d]", iglre_n + 6);

		seq_printf(m, " mem_ig_en:%d,",
			iglre.mem_ig_en);
		seq_printf(m, " mem_lre_en_if_itag:%d,",
			iglre.mem_lre_en_if_itag);
		seq_printf(m, " mem_lre_en_ls_pf_pht:%d,",
			iglre.mem_lre_en_ls_pf_pht);
		seq_printf(m, " mem_lre_en_ls_data:%d,",
			iglre.mem_lre_en_ls_data);
		seq_printf(m, " mem_lre_en_ls_tag:%d,",
			iglre.mem_lre_en_ls_tag);

		seq_printf(m, " sram_ig_low_pwr_sel:%d,",
			iglre.sram_ig_low_pwr_sel);
		seq_printf(m, " l1data_sram_ig_en:%d,",
			iglre.l1data_sram_ig_en);
		seq_printf(m, " l2tag_sram_ig_en:%d,",
			iglre.l2tag_sram_ig_en);
		seq_printf(m, " sram_ig_en_ctrl:%d,",
			iglre.sram_ig_en_ctrl);
		seq_printf(m, " sram_byte_en:%d,",
			iglre.sram_byte_en);
		seq_printf(m, " sram_rcan_en:%d\n",
			iglre.sram_rcan_en);
	}
	return 0;
}


PROC_FOPS_RW(ig_cfg);
PROC_FOPS_RW(lre_cfg);
PROC_FOPS_RW(byte_cfg);
PROC_FOPS_RW(rcan_cfg);
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
		PROC_ENTRY(byte_cfg),
		PROC_ENTRY(rcan_cfg),
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
	unsigned int igEn, lreEn, byteEn, rcanEn;

	struct device_node *node = NULL;
	int rc = 0;

	node = pdev->dev.of_node;
	if (!node) {
		iglre_debug("get iglre device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "igEn", &igEn);
	if (!rc) {
		iglre_debug("[xxxx_iglre] DTree ErrCode(%d), igEn(0x%x)\n",
			rc,
			igEn);

		if (igEn > 0)
			mtk_ig_cfg((igEn % 2) ? 1 : 0);
	}

	rc = of_property_read_u32(node, "lreEn", &lreEn);
	if (!rc) {
		iglre_debug("[xxxx_iglre] DTree ErrCode(%d), lreEn(0x%x)\n",
			rc,
			lreEn);

		if (lreEn > 0)
			mtk_lre_cfg((lreEn % 2) ? 1 : 0);
	}

	rc = of_property_read_u32(node, "byteEn", &byteEn);
	if (!rc) {
		iglre_debug("[xxxx_iglre] DTree ErrCode(%d), byteEn(0x%x)\n",
			rc,
			byteEn);

		if (byteEn > 0)
			mtk_byte_cfg((byteEn % 2) ? 1 : 0);
	}

	rc = of_property_read_u32(node, "rcanEn", &rcanEn);
	if (!rc) {
		iglre_debug("[xxxx_iglre] DTree ErrCode(%d), rcanEn(0x%x)\n",
			rc,
			rcanEn);

		if (rcanEn > 0)
			mtk_rcan_cfg((rcanEn % 2) ? 1 : 0);
	}
#endif /* CONFIG_OF */
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (iglre_buf != NULL) {
		iglre_reserve_memory_dump(iglre_buf, iglre_mem_size,
		IGLRE_TRIGGER_STAGE_PROBE);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int iglre_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (iglre_buf != NULL) {
		iglre_reserve_memory_dump(iglre_buf+0x1000, iglre_mem_size,
		IGLRE_TRIGGER_STAGE_SUSPEND);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

int iglre_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	if (iglre_buf != NULL) {
		iglre_reserve_memory_dump(iglre_buf+0x2000, iglre_mem_size,
		IGLRE_TRIGGER_STAGE_RESUME);
	}
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
	return 0;
}

MODULE_DESCRIPTION("MediaTek IGLRE Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_IGLRE_C__


