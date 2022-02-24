// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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

/* #define FLL_DEBUG */
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

/* efuse: PTPOD index */
#define DEVINFO_IDX_0 50


/************************************************
 * static Variable
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF

/* B-DOE use */
static unsigned int fll_doe_pllclken;
static unsigned int fll_doe_bren;
static unsigned int fll_doe_fll05;
static unsigned int fll_doe_fll06;
static unsigned int fll_doe_fll07;
static unsigned int fll_doe_fll08;
static unsigned int fll_doe_fll09;

#endif
#endif

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
	int str_len = 0;
	int cpu, fll_group, reg_value;
	const unsigned int bits = 31;
	const unsigned int shift = 0;
	unsigned int fll_group_bits_shift;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);
	unsigned int fll_cfg =
		(FLL_RW_READ << 15) | (shift << 10) | (bits << 4);

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
		for (fll_group = FLL_GROUP_CONTROL;
			fll_group < NR_FLL_GROUP; fll_group++) {

			fll_group_bits_shift =
				(fll_group << 16) | (bits << 8) | shift;

			reg_value =
				mt_secure_call(
				MTK_SIP_KERNEL_PTP3_CONTROL,
				PTP3_FEATURE_FLL,
				fll_group,
				fll_cfg | cpu,
				0);

			if (fll_group == FLL_GROUP_CONTROL) {
				str_len +=
				 snprintf(aee_log_buf + str_len,
				 ptp3_mem_size - str_len,
				 "CPU%d: FLL_CONTROL = 0x%08x\n",
				 cpu, reg_value);
			} else {
				str_len +=
				 snprintf(aee_log_buf + str_len,
				 ptp3_mem_size - str_len,
				 "CPU%d: FLL0%d = 0x%08x\n",
				 cpu, fll_group, reg_value);
			}
		}
	}

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
 * SMC between kernel and atf
 ************************************************/
static unsigned int fll_smc_handle(
	unsigned int rw, unsigned int cpu, unsigned int group,
	unsigned int bits, unsigned int shift, unsigned int val)
{
	unsigned int ret;
	unsigned int cfg = (rw << 15) | (shift << 10) | (bits << 4) | cpu;

	fll_msg("[%s]:cpu(%d) group(%d) shift(%d) bits(%d) val(%d)\n",
		__func__, cpu, group, shift, bits, val);

	/* update atf via smc */
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		PTP3_FEATURE_FLL,
		group,
		cfg,
		val);

	return ret;
}

/************************************************
 * IPI between kernel and mcupm/cputoeb
 ************************************************/
#if 0
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
#endif

/************************************************
 * static function
 ************************************************/
static void mtk_fll_pllclken(unsigned int fll_pllclken)
{
	unsigned int cpu;
	const unsigned int group = FLL_GROUP_CONTROL;
	const unsigned int bits = FLL_CONTROL_BITS_Pllclken;
	const unsigned int shift = FLL_CONTROL_SHIFT_Pllclken;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		/* update via atf */
		fll_smc_handle(
			FLL_RW_WRITE, cpu, group, bits, shift,
			(fll_pllclken >> cpu) & 1);
#if 0
		fll_ipi_handle(
			cpu, group, bits, shift,
			(fll_pllclken >> cpu) & 1);
#endif
	}
}

static void mtk_fll_bren(unsigned int fll_bren)
{
	unsigned int cpu;
	const unsigned int group = FLL_GROUP_05;
	const unsigned int bits = FLL05_BITS_Bren;
	const unsigned int shift = FLL05_SHIFT_Bren;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		/* update via atf */
		fll_smc_handle(
			FLL_RW_WRITE, cpu, group, bits, shift,
			(fll_bren >> cpu) & 1);
#if 0
		fll_ipi_handle(
			cpu, group, bits, shift,
			(fll_bren >> cpu) & 1);
#endif
	}
}

static void mtk_fll_kpki(unsigned int cpu, unsigned int fll_kp_online,
		unsigned int fll_ki_online,
		unsigned int fll_kp_offline,
		unsigned int fll_ki_offline)
{
	unsigned int fll_kpki;
	const unsigned int group = FLL_GROUP_05;
	const unsigned int bits = 20;
	const unsigned int shift = 0;

	fll_kpki =
		(fll_kp_online << FLL05_SHIFT_KpOnline) |
		(fll_ki_online << FLL05_SHIFT_KiOnline) |
		(fll_kp_offline << FLL05_SHIFT_KpOffline) |
		(fll_ki_offline << FLL05_SHIFT_KiOffline);

	/* update via atf */
	fll_smc_handle(
		FLL_RW_WRITE, cpu, group, bits, shift, fll_kpki);
#if 0
	fll_ipi_handle(
		cpu, group, bits, shift, fll_kpki);
#endif
}

static void mtk_fll(unsigned int cpu, unsigned int fll_group,
	unsigned int bits, unsigned int shift, unsigned int value)
{
	/* update via atf */
	fll_smc_handle(
		FLL_RW_WRITE, cpu, fll_group, bits, shift, value);
#if 0
	fll_ipi_handle(
		cpu, fll_group, bits, shift, value);
#endif
}

/************************************************
 * set FLL status by procfs interface
 ************************************************/
static ssize_t fll_pllclken_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int fll_pllclken = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (kstrtou32((const char *)buf, 0, &fll_pllclken)) {
		fll_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_fll_pllclken((unsigned int)fll_pllclken);

out:
	free_page((unsigned long)buf);
	return count;
}


static int fll_pllclken_proc_show(struct seq_file *m, void *v)
{
	int cpu, fll_pllclken, fll_control;
	const unsigned int fll_group = FLL_GROUP_CONTROL;
	const unsigned int bits = 31;
	const unsigned int shift = 0;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {

		fll_msg("cpu(%d) fll_group(%d) bits(%d) shift(%d)\n",
			cpu, fll_group, bits, shift);

		/* read from atf */
		fll_control =
			fll_smc_handle(
				FLL_RW_READ,
				cpu, fll_group, bits, shift, 0);

		fll_msg(
			"[CPU%d] fll_control=0x%08x\n",
			cpu,
			fll_control);

		fll_pllclken = GET_BITS_VAL(0:0, fll_control);
		seq_printf(m, "CPU%d: FLL_CONTROL_Pllclken = %d\n",
			cpu,
			fll_pllclken);
	}

	return 0;
}

static ssize_t fll_bren_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int fll_bren = 0;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (kstrtou32((const char *)buf, 0, &fll_bren)) {
		fll_err("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_fll_bren((unsigned int)fll_bren);

out:
	free_page((unsigned long)buf);
	return count;

}

static int fll_bren_proc_show(struct seq_file *m, void *v)
{
	int cpu, fll_bren, fll_05;
	const unsigned int fll_group = FLL_GROUP_05;
	const unsigned int bits = 31;
	const unsigned int shift = 0;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {

		fll_msg("cpu(%d) fll_group(%d) bits(%d) shift(%d)\n",
			cpu, fll_group, bits, shift);

		/* read from atf */
		fll_05 =
			fll_smc_handle(
				FLL_RW_READ,
				cpu, fll_group, bits, shift, 0);

		fll_msg(
			"[CPU%d] fll_05=0x%08x\n",
			cpu,
			fll_05);

		fll_bren = GET_BITS_VAL(20:20, fll_05);

		seq_printf(m, "CPU%d: FLL05_Bren = %d\n",
			cpu,
			fll_bren);
	}

	return 0;
}

static ssize_t fll_kpki_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	/* parameter input */
	int cpu, fll_kp_online, fll_kp_offline,
		fll_ki_online, fll_ki_offline;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* parameter check */
	if (sscanf(buf, "%u %u %u %u %u",
		&cpu, &fll_kp_online, &fll_ki_online,
		&fll_kp_offline, &fll_ki_offline) != 5) {

		fll_err("bad argument!! Should input 5 arguments.\n");
		goto out;
	}

	/* sync parameter with trust-zoon */
	mtk_fll_kpki(cpu, (unsigned int)fll_kp_online,
		(unsigned int)fll_ki_online,
		(unsigned int)fll_kp_offline,
		(unsigned int)fll_ki_offline);

out:
	free_page((unsigned long)buf);
	return count;

}

static int fll_kpki_proc_show(struct seq_file *m, void *v)
{
	int cpu, fll_kp_online, fll_kp_offline,
		fll_ki_online, fll_ki_offline, fll_05;
	const unsigned int fll_group = FLL_GROUP_05;
	const unsigned int bits = 31;
	const unsigned int shift = 0;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {

		fll_msg("cpu(%d) fll_group(%d) bits(%d) shift(%d)\n",
			cpu, fll_group, bits, shift);

		/* read from atf */
		fll_05 =
			fll_smc_handle(
				FLL_RW_READ,
				cpu, fll_group, bits, shift, 0);

		fll_msg(
			"[CPU%d] fll_05=0x%08x\n",
			cpu,
			fll_05);

		fll_kp_online = GET_BITS_VAL(19:16, fll_05);
		fll_ki_online = GET_BITS_VAL(15:10, fll_05);
		fll_kp_offline = GET_BITS_VAL(9:6, fll_05);
		fll_ki_offline = GET_BITS_VAL(5:0, fll_05);

		seq_printf(m, "CPU%d: (kp_online,ki_online,kp_offline,ki_offline) = (%d,%d,%d,%d)\n",
			cpu,
			fll_kp_online,
			fll_ki_online,
			fll_kp_offline,
			fll_ki_offline);
	}

	return 0;
}


static ssize_t fll_reg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	/* parameter input */
	char *cpu_str, *fll_group_str, *bits_str, *shift_str, *value_str;
	unsigned int cpu, fll_group, bits, shift, value;

	/* proc template for check */
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* Convert str to hex */
	cpu_str = strsep(&buf, " ");
	fll_group_str = strsep(&buf, " ");
	bits_str = strsep(&buf, " ");
	shift_str = strsep(&buf, " ");
	value_str = strsep(&buf, " ");

	fll_msg(
		"cpu_str(%s) fll_group_str(%s) bits_str(%s) shift_str(%s) value_str(%s)\n",
		cpu_str, fll_group_str, bits_str, shift_str, value_str);

	if ((cpu_str != 0) && (fll_group_str != 0) &&
		(bits_str != 0) && (shift_str != 0) &&
		(value_str != 0)) {
		ret = kstrtou32(
			(const char *)cpu_str, 10, (unsigned int *)&cpu);
		ret = kstrtou32(
			(const char *)fll_group_str, 10,
			(unsigned int *)&fll_group);
		ret = kstrtou32(
			(const char *)bits_str, 10, (unsigned int *)&bits);
		ret = kstrtou32(
			(const char *)shift_str, 10, (unsigned int *)&shift);
		ret = kstrtou32(
			(const char *)value_str, 16, (unsigned int *)&value);

		fll_msg(
			"cpu(%d) fll_group(%d) bits(%d) shift(%d) value(0x%08x)\n",
			cpu, fll_group, bits, shift, value);

		/* sync parameter with trust-zoon */
		mtk_fll((unsigned int)cpu, (unsigned int)fll_group,
		(unsigned int)bits, (unsigned int)shift, (unsigned int)value);
	}

out:
	free_page((unsigned long)buf);
	return count;
}


static int fll_reg_proc_show(struct seq_file *m, void *v)
{
	int cpu, fll_group, reg_value;
	const unsigned int bits = 31;
	const unsigned int shift = 0;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {
		for (fll_group = FLL_GROUP_CONTROL;
			fll_group < NR_FLL_GROUP; fll_group++) {

			fll_msg(
				"cpu(%d) fll_group(%d) bits(%d) shift(%d)\n",
				cpu, fll_group, bits, shift);

			/* read from atf */
			reg_value =
				fll_smc_handle(
					FLL_RW_READ,
					cpu, fll_group, bits, shift, 0);

			if (fll_group == FLL_GROUP_CONTROL) {
				seq_printf(m, "CPU%d: FLL_CONTROL = 0x%08x\n",
					cpu,
					reg_value);
			} else {
				seq_printf(m, "CPU%d: FLL0%d = 0x%08x\n",
					cpu,
					fll_group,
					reg_value);
			}
		}
	}
	return 0;
}

static int fll_bit_en_proc_show(struct seq_file *m, void *v)
{
	unsigned int cpu, fll_bren = 0, fll_pllclken = 0, fll_control, fll_05;
	const unsigned int bits = 31;
	const unsigned int shift = 0;

	for (cpu = FLL_CPU_START_ID; cpu <= FLL_CPU_END_ID; cpu++) {

		/* read from atf */
		fll_control =
			fll_smc_handle(
				FLL_RW_READ,
				cpu, FLL_GROUP_CONTROL, bits, shift, 0);

		/* read from atf */
		fll_05 =
			fll_smc_handle(
				FLL_RW_READ,
				cpu, FLL_GROUP_05, bits, shift, 0);

		fll_msg(
			"[CPU%d] fll_control=0x%08x\n",
			cpu,
			fll_control);

		fll_msg(
			"[CPU%d] fll_05=0x%08x\n",
			cpu,
			fll_05);

		fll_pllclken |= GET_BITS_VAL(0:0, fll_control) << cpu;
		fll_bren |= GET_BITS_VAL(20:20, fll_05) << cpu;

		fll_msg(
			"[CPU%d] fll_pllclken=0x%08x\n",
			cpu,
			fll_pllclken);

		fll_msg(
			"[CPU%d] fll_bren=0x%08x\n",
			cpu,
			fll_bren);

	}

	seq_printf(m, "%d\n", (fll_bren & fll_pllclken));

	return 0;
}


PROC_FOPS_RW(fll_pllclken);
PROC_FOPS_RW(fll_bren);
PROC_FOPS_RW(fll_reg);
PROC_FOPS_RW(fll_kpki);
PROC_FOPS_RO(fll_bit_en);

int fll_create_procfs(const char *proc_name, struct proc_dir_entry *dir)
{
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry fll_entries[] = {
		PROC_ENTRY(fll_pllclken),
		PROC_ENTRY(fll_bren),
		PROC_ENTRY(fll_reg),
		PROC_ENTRY(fll_kpki),
		PROC_ENTRY(fll_bit_en),
	};

	for (i = 0; i < ARRAY_SIZE(fll_entries); i++) {
		if (!proc_create(fll_entries[i].name,
			0660,
			dir,
			fll_entries[i].fops)) {
			fll_err("[%s]: create /proc/%s/%s failed\n",
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
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	int cpu;

	node = pdev->dev.of_node;
	if (!node) {
		fll_err("get fll device node err\n");
		return -ENODEV;
	}

	/*fll05*/
	rc = of_property_read_u32(node,
		"fll_doe_fll05", &fll_doe_fll05);

	if (!rc) {
		fll_msg(
			"fll_doe_fll05 from DTree; rc(%d) fll_doe_fll05(0x%x)\n",
			rc,
			fll_doe_fll05);

		if (fll_doe_fll05 > 0) {
			for (cpu = FLL_CPU_START_ID;
				cpu <= FLL_CPU_END_ID; cpu++) {
				mtk_fll(cpu,
					5,
					20,
					0,
					fll_doe_fll05);
			}
		}
	}

	/*fll06*/
	rc = of_property_read_u32(node,
		"fll_doe_fll06", &fll_doe_fll06);

	if (!rc) {
		fll_msg(
			"fll_doe_fll06 from DTree; rc(%d) fll_doe_fll06(0x%x)\n",
			rc,
			fll_doe_fll06);

		if (fll_doe_fll06 > 0) {
			for (cpu = FLL_CPU_START_ID;
				cpu <= FLL_CPU_END_ID; cpu++) {
				mtk_fll(cpu,
					6,
					15,
					0,
					fll_doe_fll06);
			}
		}
	}

	/*fll07*/
	rc = of_property_read_u32(node,
		"fll_doe_fll07", &fll_doe_fll07);

	if (!rc) {
		fll_msg(
			"fll_doe_fll07 from DTree; rc(%d) fll_doe_fll07(0x%x)\n",
			rc,
			fll_doe_fll07);

		if (fll_doe_fll07 > 0) {
			for (cpu = FLL_CPU_START_ID;
				cpu <= FLL_CPU_END_ID; cpu++) {
				mtk_fll(cpu,
					7,
					11,
					0,
					fll_doe_fll07);
			}
		}
	}

	/*fll08*/
	rc = of_property_read_u32(node,
		"fll_doe_fll08", &fll_doe_fll08);

	if (!rc) {
		fll_msg(
			"fll_doe_fll08 from DTree; rc(%d) fll_doe_fll08(0x%x)\n",
			rc,
			fll_doe_fll08);

		if (fll_doe_fll08 > 0) {
			for (cpu = FLL_CPU_START_ID;
				cpu <= FLL_CPU_END_ID; cpu++) {
				mtk_fll(cpu,
					8,
					12,
					0,
					fll_doe_fll08);
			}
		}
	}

	/*fll09*/
	rc = of_property_read_u32(node,
		"fll_doe_fll09", &fll_doe_fll09);

	if (!rc) {
		fll_msg(
			"fll_doe_fll09 from DTree; rc(%d) fll_doe_fll09(0x%x)\n",
			rc,
			fll_doe_fll09);

		if (fll_doe_fll09 > 0) {
			for (cpu = FLL_CPU_START_ID;
				cpu <= FLL_CPU_END_ID; cpu++) {
				mtk_fll(cpu,
					9,
					13,
					0,
					fll_doe_fll09);
			}
		}
	}

	/* pllclken control */
	rc = of_property_read_u32(node,
		"fll_doe_pllclken", &fll_doe_pllclken);

	if (!rc) {
		fll_msg(
			"fll_doe_pllclken from DTree; rc(%d) fll_doe_pllclken(0x%x)\n",
			rc,
			fll_doe_pllclken);

		if (fll_doe_pllclken < 256)
			mtk_fll_pllclken(fll_doe_pllclken);
	}

	/* bren control */
	rc = of_property_read_u32(node,
		"fll_doe_bren", &fll_doe_bren);

	if (!rc) {
		fll_msg(
			"fll_doe_bren from DTree; rc(%d) fll_doe_bren(0x%x)\n",
			rc,
			fll_doe_bren);

		if (fll_doe_bren < 256)
			mtk_fll_bren(fll_doe_bren);
	}

	/* dump reg status into PICACHU dram for DB */
	fll_reserve_memory_dump(
		fll_buf, fll_mem_size, FLL_TRIGGER_STAGE_PROBE);

	fll_msg("fll probe ok!!\n");
#endif
#endif
	return 0;
}

int fll_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	fll_reserve_memory_dump(
		fll_buf+0x1000, fll_mem_size, FLL_TRIGGER_STAGE_SUSPEND);
#endif
#endif
	return 0;
}

int fll_resume(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* dump reg status into PICACHU dram for DB */
	fll_reserve_memory_dump(
		fll_buf+0x2000, fll_mem_size, FLL_TRIGGER_STAGE_RESUME);
#endif
#endif
	return 0;
}

MODULE_DESCRIPTION("MediaTek FLL Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_FLL_C__
