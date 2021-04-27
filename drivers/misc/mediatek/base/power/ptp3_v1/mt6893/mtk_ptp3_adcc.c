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
 * @file	mkt_ptp3_adcc.c
 * @brief   Driver for adcc
 *
 */

#define __MTK_ADCC_C__

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
	#include "mtk_ptp3_adcc.h"
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
/*#define ADCC_DEBUG_ON */

#define ADCC_TAG	"[ADCC]"

#define adcc_info(fmt, args...)	\
	pr_info(ADCC_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef ADCC_DEBUG_ON
#define adcc_debug(fmt, args...) \
	pr_debug(ADCC_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define adcc_debug(fmt, args...)
#endif


/************************************************
 * static marco
 ************************************************/
#undef  BIT
#define BIT(_bit_)                    (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_)           ((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_)               (((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_)   (((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

/************************************************
 * SMC between kernel and atf
 ************************************************/
static unsigned int adcc_smc_handle(unsigned int key,
	unsigned int core, unsigned int val)
{
	unsigned int ret;

	adcc_debug("[%s]:key(%d) core(%d) val(%d)\n",
		__func__, key, core, val);

	/* update atf via smc */
	ret = ptp3_smc_handle(
		PTP3_FEATURE_ADCC,
		key,
		core,
		val);

	return ret;
}

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

#ifdef PTP3_STATUS_PROBE_DUMP
static char *adcc_buf;
static unsigned long long adcc_mem_size;
void adcc_save_memory_info(char *buf, unsigned long long ptp3_mem_size)
{
	adcc_buf = buf;
	adcc_mem_size = ptp3_mem_size;
}

int adcc_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum ADCC_TRIGGER_STAGE adcc_tri_stage)
{
	int str_len = 0;
	unsigned int core, temp, dump_efuse;
	unsigned int dump_set, dump_PLL, dump_FLL;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	/* check free page valid or not */
	if (!aee_log_buf) {
		adcc_info("unable to get free page!\n");
		return -1;
	}
	adcc_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	/* show trigger stage */
	switch (adcc_tri_stage) {
	case ADCC_TRIGGER_STAGE_PROBE:
		str_len += scnprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Probe]\n");
		break;
	case ADCC_TRIGGER_STAGE_SUSPEND:
		str_len += scnprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Suspend]\n");
		break;
	case ADCC_TRIGGER_STAGE_RESUME:
		str_len += scnprintf(aee_log_buf + str_len,
			ptp3_mem_size - str_len,
			"\n[Kernel Resume]\n");
		break;
	default:
		adcc_debug("illegal ADCC_TRIGGER_STAGE\n");
		break;
	}
	/* collect dump info */
	for (core = ADCC_CPU_START_ID; core <= ADCC_CPU_END_ID; core++) {
		dump_set = adcc_smc_handle(ADCC_DUMP_INFO, core, 8);
		dump_PLL = adcc_smc_handle(ADCC_DUMP_INFO, core, 9);
		dump_FLL = adcc_smc_handle(ADCC_DUMP_INFO, core, 10);
		dump_efuse = adcc_smc_handle(ADCC_DUMP_INFO, core, 3);

		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			ADCC_TAG"[CPU%d]", core);

		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" Shaper:0x%x,", GET_BITS_VAL(20:17, dump_set));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" SW_nFlag:0x%x,", GET_BITS_VAL(16:16, dump_set));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" DcdSlect:0x%x,", GET_BITS_VAL(15:8, dump_set));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" DcTarget:0x%x,", GET_BITS_VAL(7:0, dump_set));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FllCalDone:0x%x,", GET_BITS_VAL(5:5, dump_FLL));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FllCalin:0x%x,", GET_BITS_VAL(4:0, dump_FLL));

		if (GET_BITS_VAL(16:16, dump_FLL) == 1) {
			temp = GET_BITS_VAL(15:6, dump_FLL);
			str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FLL_integrator:%d,", temp);
			if (temp >= 512)
				str_len += scnprintf(aee_log_buf + str_len,
				(unsigned long long)adcc_mem_size - str_len,
				" FllDuty:%d%%%%,", ((512-(temp-512))*5000)/512);
			else
				str_len += scnprintf(aee_log_buf + str_len,
				(unsigned long long)adcc_mem_size - str_len,
				" FllDuty:%d%%%%,", ((512+(512-temp))*5000)/512);
		}

		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" PLLCalDone:0x%x,",
			GET_BITS_VAL(5:5, dump_PLL));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" PLLCalin:0x%x,",
			GET_BITS_VAL(4:0, dump_PLL));

		if (GET_BITS_VAL(16:16, dump_PLL) == 1) {
			temp = GET_BITS_VAL(15:6, dump_PLL);
			str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" PLL_integrator:%d,", temp);
			if (temp >= 512)
				str_len += scnprintf(aee_log_buf + str_len,
				(unsigned long long)adcc_mem_size - str_len,
				" PLLDuty:%d%%%%", ((512-(temp-512))*5000)/512);
			else
				str_len += scnprintf(aee_log_buf + str_len,
				(unsigned long long)adcc_mem_size - str_len,
				" PLLDuty:%d%%%%", ((512+(512-temp))*5000)/512);
		}

		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" PLL_efuse:0x%x,", GET_BITS_VAL(3:0, dump_efuse));
		str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FLL_efuse:0x%x,", GET_BITS_VAL(7:4, dump_efuse));
		if (core == 4)
			str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(12:8, dump_efuse));
		else if (core == 5)
			str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(17:13, dump_efuse));
		else if (core == 6)
			str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(22:18, dump_efuse));
		else if (core == 7)
			str_len += scnprintf(aee_log_buf + str_len,
			(unsigned long long)adcc_mem_size - str_len,
			" FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(27:23, dump_efuse));

	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	adcc_debug("\n%s", aee_log_buf);
	adcc_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#endif
#endif
#endif

/************************************************
 * static function
 ************************************************/


/************************************************
 * set ADCC status by procfs interface
 ************************************************/
static ssize_t adcc_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int core, enable, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtouint(buf, 16, &value)) {
		adcc_info("bad argument!! Should be XXXXXXXX\n");
		goto out;
	}

	for (core = ADCC_CPU_START_ID; core <= ADCC_CPU_END_ID; core++) {
		enable = (value >> (core * 4)) & 0xF;
		adcc_smc_handle(ADCC_ENABLE, core, enable);
	}
out:
	free_page((unsigned long)buf);
	return count;
}

static int adcc_cfg_proc_show(struct seq_file *m, void *v)
{
	unsigned int core, dump_set;
	unsigned int status = 0;

	for (core = ADCC_CPU_START_ID; core <= ADCC_CPU_END_ID; core++) {
		dump_set = adcc_smc_handle(ADCC_DUMP_INFO, core, 8);
		status = status | ((GET_BITS_VAL(20:17, dump_set) & 0xF) << (core * 4));
	}
	seq_printf(m, "%08x\n", status);

	return 0;
}

static ssize_t adcc_set_Calin_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int core, shaper, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u", &core, &shaper, &value) != 3) {
		adcc_info("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	if ((core < ADCC_CPU_START_ID) || (core > ADCC_CPU_END_ID)) {
		adcc_info("core(%d) is illegal\n", core);
		goto out;
	}

	if (shaper > 3) {
		adcc_info("shaper(%d) is illegal\n", core);
		goto out;
	}

	adcc_smc_handle(ADCC_SET_SHAPER, core, shaper);
	adcc_smc_handle(ADCC_SET_CALIN, core, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int adcc_set_Calin_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t adcc_set_DcdSelect_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int core, shaper, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u", &core, &shaper, &value) != 3) {
		adcc_info("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	if ((core < ADCC_CPU_START_ID) || (core > ADCC_CPU_END_ID)) {
		adcc_info("core(%d) is illegal\n", core);
		goto out;
	}

	adcc_smc_handle(ADCC_SET_SHAPER, core, shaper);
	adcc_smc_handle(ADCC_SET_DCDSELECT, core, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int adcc_set_DcdSelect_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t adcc_set_DcTarget_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int core, shaper, value;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u", &core, &shaper, &value) != 3) {
		adcc_info("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	if ((core < ADCC_CPU_START_ID) || (core > ADCC_CPU_END_ID)) {
		adcc_info("core(%d) is illegal\n", core);
		goto out;
	}

	adcc_smc_handle(ADCC_SET_SHAPER, core, shaper);
	adcc_smc_handle(ADCC_SET_DCTARGET, core, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int adcc_set_DcTarget_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int adcc_dump_proc_show(struct seq_file *m, void *v)
{
	unsigned int dump_set, dump_PLL, dump_FLL, core;
	unsigned int temp, dump_efuse;

	for (core = ADCC_CPU_START_ID; core <= ADCC_CPU_END_ID; core++) {
		dump_set = adcc_smc_handle(ADCC_DUMP_INFO, core, 8);
		dump_PLL = adcc_smc_handle(ADCC_DUMP_INFO, core, 9);
		dump_FLL = adcc_smc_handle(ADCC_DUMP_INFO, core, 10);
		dump_efuse = adcc_smc_handle(ADCC_DUMP_INFO, core, 3);

		seq_printf(m, ADCC_TAG"[CPU%d]", core);
		seq_printf(m, " Shaper:0x%x,", GET_BITS_VAL(20:17, dump_set));
		seq_printf(m, " SW_nFlag:0x%x,", GET_BITS_VAL(16:16, dump_set));
		seq_printf(m, " DcdSlect:0x%x,", GET_BITS_VAL(15:8, dump_set));
		seq_printf(m, " DcTarget:0x%x,", GET_BITS_VAL(7:0, dump_set));

		seq_printf(m, " FllCalDone:0x%x,", GET_BITS_VAL(5:5, dump_FLL));
		seq_printf(m, " FllCalin:0x%x,", GET_BITS_VAL(4:0, dump_FLL));


		if (GET_BITS_VAL(16:16, dump_FLL) == 1) {
			temp = GET_BITS_VAL(15:6, dump_FLL);
			if (temp >= 512)
				seq_printf(m, " FllDuty:%d%%%%,",
					((512-(temp-512))*5000)/512);
			else
				seq_printf(m, " FllDuty:%d%%%%,",
					((512+(512-temp))*5000)/512);
		}

		seq_printf(m, " PllCalDone:0x%x,", GET_BITS_VAL(5:5, dump_PLL));
		seq_printf(m, " PllCalin:0x%x,", GET_BITS_VAL(4:0, dump_PLL));

		if (GET_BITS_VAL(16:16, dump_PLL) == 1) {
			temp = GET_BITS_VAL(15:6, dump_PLL);
			if (temp >= 512)
				seq_printf(m, " PllDuty:%d%%%%",
					((512-(temp-512))*5000)/512);
			else
				seq_printf(m, " PllDuty:%d%%%%",
					((512+(512-temp))*5000)/512);
		}

		seq_printf(m, " PLL_efuse:0x%x,", GET_BITS_VAL(3:0, dump_efuse));
		seq_printf(m, " FLL_efuse:0x%x,", GET_BITS_VAL(7:4, dump_efuse));
		if (core == 4)
			seq_printf(m, " FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(12:8, dump_efuse));
		else if (core == 5)
			seq_printf(m, " FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(17:13, dump_efuse));
		else if (core == 6)
			seq_printf(m, " FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(22:18, dump_efuse));
		else if (core == 7)
			seq_printf(m, " FLL_efuse_calout:0x%x\n",
				GET_BITS_VAL(27:23, dump_efuse));

	}

	return 0;
}

static int adcc_dump_reg_proc_show(struct seq_file *m, void *v)
{
	unsigned int core;

	for (core = ADCC_CPU_START_ID; core <= ADCC_CPU_END_ID; core++) {
		seq_printf(m, ADCC_TAG"[CPU%d]", core);
		seq_printf(m, "SET:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 0));
		seq_printf(m, "PLL:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 1));
		seq_printf(m, "FLL:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 2));
		seq_printf(m, "ATE:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 3));
		seq_printf(m, "109:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 4));
		seq_printf(m, "110:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 5));
		seq_printf(m, "114:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 6));
		seq_printf(m, "DCD:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 7));
		seq_printf(m, "DOS:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 8));
		seq_printf(m, "DOP:0x%x,", adcc_smc_handle(ADCC_DUMP_INFO, core, 9));
		seq_printf(m, "DOF:0x%x\n", adcc_smc_handle(ADCC_DUMP_INFO, core, 10));
	}

	return 0;
}


PROC_FOPS_RW(adcc_cfg);
PROC_FOPS_RW(adcc_set_Calin);
PROC_FOPS_RW(adcc_set_DcdSelect);
PROC_FOPS_RW(adcc_set_DcTarget);
PROC_FOPS_RO(adcc_dump);
PROC_FOPS_RO(adcc_dump_reg);

int adcc_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;
	struct proc_dir_entry *adcc_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry adcc_entries[] = {
		PROC_ENTRY(adcc_cfg),
		PROC_ENTRY(adcc_set_Calin),
		PROC_ENTRY(adcc_set_DcdSelect),
		PROC_ENTRY(adcc_set_DcTarget),
		PROC_ENTRY(adcc_dump),
		PROC_ENTRY(adcc_dump_reg),
	};

	adcc_dir = proc_mkdir("adcc", dir);
	if (!adcc_dir) {
		adcc_debug("[%s]: mkdir /proc/adcc failed\n",
			__func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(adcc_entries); i++) {
		if (!proc_create(adcc_entries[i].name,
			0660,
			adcc_dir,
			adcc_entries[i].fops)) {
			adcc_debug("[%s]: create /proc/%s/adcc/%s failed\n",
				__func__,
				proc_name,
				adcc_entries[i].name);
			return -3;
		}
	}
	return 0;
}

int adcc_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
#ifdef PTP3_STATUS_PROBE_DUMP
	/* dump reg status into PICACHU dram for DB */
	if (adcc_buf != NULL) {
		adcc_reserve_memory_dump(adcc_buf, adcc_mem_size,
			ADCC_TRIGGER_STAGE_PROBE);
	}
#endif
#endif
#endif
	return 0;
}

int adcc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int adcc_resume(struct platform_device *pdev)
{
	return 0;
}

MODULE_DESCRIPTION("MediaTek ADCC Driver v0.1");
MODULE_LICENSE("GPL");
#undef __MTK_ADCC_C__
