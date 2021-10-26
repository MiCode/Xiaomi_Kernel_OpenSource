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
 * @file	mkt_credit_didt.c
 * @brief   Driver for credit_didt
 *
 */

#define __MTK_CREDIT_DIDT_C__

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

#ifdef __KERNEL__
	#include <linux/topology.h>

	/* local includes (kernel-4.4)*/
	#include <mt-plat/mtk_chip.h>
	/* #include <mt-plat/mtk_gpio.h> */
	#include "mtk_credit_didt.h"
	#include <mt-plat/mtk_devinfo.h>
	#include "mtk_devinfo.h"
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
#define CREDIT_DIDT_DEBUG
#define CREDIT_DIDT_TAG	 "[CREDIT_DIDT]"

#define credit_didt_err(fmt, args...)	\
	pr_info(CREDIT_DIDT_TAG"[ERROR][%s():%d]" fmt,\
	__func__, __LINE__, ##args)

#define credit_didt_msg(fmt, args...)	\
	pr_info(CREDIT_DIDT_TAG"[INFO][%s():%d]" fmt,\
	__func__, __LINE__, ##args)

#ifdef CREDIT_DIDT_DEBUG
#define credit_didt_debug(fmt, args...)	\
	pr_debug(CREDIT_DIDT_TAG"[DEBUG][%s():%d]" fmt,\
	__func__, __LINE__, ##args)
#else
#define credit_didt_debug(fmt, args...)
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
static unsigned int credit_didt_doe_enable;

static unsigned int credit_didt4_doe_ls_period;
static unsigned int credit_didt5_doe_ls_period;
static unsigned int credit_didt6_doe_ls_period;
static unsigned int credit_didt7_doe_ls_period;
static unsigned int credit_didt4_doe_ls_credit;
static unsigned int credit_didt5_doe_ls_credit;
static unsigned int credit_didt6_doe_ls_credit;
static unsigned int credit_didt7_doe_ls_credit;
static unsigned int credit_didt4_doe_ls_low_freq_period;
static unsigned int credit_didt5_doe_ls_low_freq_period;
static unsigned int credit_didt6_doe_ls_low_freq_period;
static unsigned int credit_didt7_doe_ls_low_freq_period;
static unsigned int credit_didt4_doe_ls_low_freq_enable;
static unsigned int credit_didt5_doe_ls_low_freq_enable;
static unsigned int credit_didt6_doe_ls_low_freq_enable;
static unsigned int credit_didt7_doe_ls_low_freq_enable;

static unsigned int credit_didt4_doe_vx_period;
static unsigned int credit_didt5_doe_vx_period;
static unsigned int credit_didt6_doe_vx_period;
static unsigned int credit_didt7_doe_vx_period;
static unsigned int credit_didt4_doe_vx_credit;
static unsigned int credit_didt5_doe_vx_credit;
static unsigned int credit_didt6_doe_vx_credit;
static unsigned int credit_didt7_doe_vx_credit;
static unsigned int credit_didt4_doe_vx_low_freq_period;
static unsigned int credit_didt5_doe_vx_low_freq_period;
static unsigned int credit_didt6_doe_vx_low_freq_period;
static unsigned int credit_didt7_doe_vx_low_freq_period;
static unsigned int credit_didt4_doe_vx_low_freq_enable;
static unsigned int credit_didt5_doe_vx_low_freq_enable;
static unsigned int credit_didt6_doe_vx_low_freq_enable;
static unsigned int credit_didt7_doe_vx_low_freq_enable;

#endif /* CONFIG_OF */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

/* GAT log use */
phys_addr_t credit_didt_mem_base_phys;
phys_addr_t credit_didt_mem_size;
phys_addr_t credit_didt_mem_base_virt;

static int credit_didt_reserve_memory_dump(char *buf, unsigned int log_offset)
{
	int str_len = 0;
	int cpu, ls_vx, cfg, param, cpu_tmp;
	unsigned char credit_didt_info[NR_CREDIT_DIDT_CPU]
		[NR_CREDIT_DIDT_CHANNEL][NR_CREDIT_DIDT_CFG];
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	if (!aee_log_buf) {
		credit_didt_debug("unable to get free page!\n");
		return -1;
	}
	credit_didt_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	credit_didt_debug("log_offset = %d\n", log_offset);
	if (log_offset == 0) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)credit_didt_mem_size - str_len,
		 "\n[Kernel Probe]\n");
	} else if (log_offset == 1) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)credit_didt_mem_size - str_len,
		 "\n[Kernel Suspend]\n");
	} else if (log_offset == 2) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)credit_didt_mem_size - str_len,
		 "\n[Kernel Resume]\n");
	} else {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)credit_didt_mem_size - str_len,
		 "\n[Kernel ??]\n");
	}

	for (cpu = CREDIT_DIDT_CPU_START_ID;
		cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {
		for (ls_vx = 0; ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {
			for (cfg = 0; cfg < NR_CREDIT_DIDT_CFG; cfg++) {
				param = ls_vx * NR_CREDIT_DIDT_CFG + cfg;

				cpu_tmp = cpu-CREDIT_DIDT_CPU_START_ID;
				credit_didt_info[cpu_tmp][ls_vx][cfg] =
				mt_secure_call_credit_didt(
					MTK_SIP_KERNEL_CREDIT_DIDT_CONTROL,
					CREDIT_DIDT_RW_READ,
					cpu,
					param,
					0);
			}

			cpu_tmp = cpu-CREDIT_DIDT_CPU_START_ID;

			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)credit_didt_mem_size
				- str_len,
				"cpu%d %s period=%d credit=%d low_period=%d low_freq_en=%d enable=%d\n",
				cpu, (ls_vx == 0 ? "LS" : "VX"),
				credit_didt_info[cpu_tmp][ls_vx]
					[CREDIT_DIDT_CFG_PERIOD],
				credit_didt_info[cpu_tmp][ls_vx]
					[CREDIT_DIDT_CFG_CREDIT],
				credit_didt_info[cpu_tmp][ls_vx]
					[CREDIT_DIDT_CFG_LOW_PWR_PERIOD],
				credit_didt_info[cpu_tmp][ls_vx]
					[CREDIT_DIDT_CFG_LOW_PWR_ENABLE],
				credit_didt_info[cpu_tmp][ls_vx]
					[CREDIT_DIDT_CFG_ENABLE]);

		}
	}

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	credit_didt_debug("\n%s", aee_log_buf);
	credit_didt_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#define EEM_TEMPSPARE0		0x11278F20
#define credit_didt_read(addr)		__raw_readl((void __iomem *)(addr))
#define credit_didt_write(addr, val)	mt_reg_sync_writel(val, addr)

static void credit_didt_reserve_memory_init(unsigned int log_offset)
{
	char *buf;

	credit_didt_mem_base_virt = 0;
	credit_didt_mem_size = 0x80000;
	credit_didt_mem_base_phys =
		credit_didt_read(ioremap(EEM_TEMPSPARE0, 0));

	if ((char *)credit_didt_mem_base_phys != NULL) {
		credit_didt_mem_base_virt =
			(phys_addr_t)(uintptr_t)ioremap_wc(
			credit_didt_mem_base_phys,
			credit_didt_mem_size);
	}

	credit_didt_debug("[CREDIT_DIDT] phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)credit_didt_mem_base_phys,
		(unsigned long long)credit_didt_mem_size,
		(unsigned long long)credit_didt_mem_base_virt);

	if ((char *)credit_didt_mem_base_virt != NULL) {
		buf = (char *)(uintptr_t)
			(credit_didt_mem_base_virt+0x10000+log_offset*0x1000);

		/* dump credit_didt register status into reserved memory */
		credit_didt_reserve_memory_dump(buf, log_offset);
	} else
		credit_didt_debug("credit_didt_mem_base_virt is null !\n");

}


#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */


/************************************************
 * update CREDIT_DIDT status with ATF
 ************************************************/
static void mtk_credit_didt(unsigned int cpu,
		unsigned int ls_vx, unsigned int cfg, unsigned int value)
{
	unsigned int param = ls_vx * NR_CREDIT_DIDT_CFG + cfg;

	if ((cpu >= CREDIT_DIDT_CPU_START_ID)
		&& (cpu <= CREDIT_DIDT_CPU_END_ID)) {

		mt_secure_call_credit_didt(MTK_SIP_KERNEL_CREDIT_DIDT_CONTROL,
			CREDIT_DIDT_RW_WRITE, cpu, param, value);
	}
}

/************************************************
 * set CREDIT_DIDT status by procfs interface
 ************************************************/
static int credit_didt_proc_show(struct seq_file *m, void *v)
{
	unsigned int cpu, ls_vx, cfg, param, cpu_tmp;
	unsigned char credit_didt_info[NR_CREDIT_DIDT_CPU]
		[NR_CREDIT_DIDT_CHANNEL][NR_CREDIT_DIDT_CFG];

	for (cpu = CREDIT_DIDT_CPU_START_ID;
		cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {

		for (ls_vx = 0; ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {
			for (cfg = 0; cfg < NR_CREDIT_DIDT_CFG; cfg++) {
				param = ls_vx * NR_CREDIT_DIDT_CFG + cfg;

				/* coverity check */
				if (cpu >= CREDIT_DIDT_CPU_START_ID)
					cpu_tmp = cpu-CREDIT_DIDT_CPU_START_ID;
				else {
					credit_didt_err(
						"cpu(%d) is illegal\n", cpu);
					return -1;
				}

				/* coverity check */
				if (cpu_tmp < NR_CREDIT_DIDT_CPU) {
					credit_didt_info[cpu_tmp][ls_vx][cfg] =
					mt_secure_call_credit_didt(
					 MTK_SIP_KERNEL_CREDIT_DIDT_CONTROL,
					 CREDIT_DIDT_RW_READ,
					 cpu,
					 param,
					 0);

					credit_didt_debug(
					 "Get cpu=%d ls_vx=%d cfg=%d value=%d\n",
					 cpu, ls_vx, cfg,
					 credit_didt_info
					  [cpu_tmp][ls_vx][cfg]);
				} else {
					credit_didt_err(
						"cpu_tmp(%d) is illegal\n");
					return -1;
				}
			}
		}
	}

	for (cpu = CREDIT_DIDT_CPU_START_ID;
		cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {
		for (ls_vx = 0; ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {

			/* coverity check */
			if (cpu >= CREDIT_DIDT_CPU_START_ID)
				cpu_tmp = cpu-CREDIT_DIDT_CPU_START_ID;
			else {
				credit_didt_err(
					"cpu(%d) is illegal\n", cpu);
				return -1;
			}

			seq_printf(m,
			"cpu%d %s period=%d credit=%d low_period=%d low_freq_en=%d enable=%d\n",
			cpu, (ls_vx == 0 ? "LS" : "VX"),
			credit_didt_info[cpu_tmp][ls_vx]
				[CREDIT_DIDT_CFG_PERIOD],
			credit_didt_info[cpu_tmp][ls_vx]
				[CREDIT_DIDT_CFG_CREDIT],
			credit_didt_info[cpu_tmp][ls_vx]
				[CREDIT_DIDT_CFG_LOW_PWR_PERIOD],
			credit_didt_info[cpu_tmp][ls_vx]
				[CREDIT_DIDT_CFG_LOW_PWR_ENABLE],
			credit_didt_info[cpu_tmp][ls_vx]
				[CREDIT_DIDT_CFG_ENABLE]
				);
		}
	}

	return 0;
}

static ssize_t credit_didt_proc_write(struct file *file,
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
		credit_didt_debug("bad argument!! Should input 4 arguments.\n");
		goto out;
	}

	if ((cpu < CREDIT_DIDT_CPU_START_ID) || (cpu > CREDIT_DIDT_CPU_END_ID))
		goto out;

	if ((ls_vx < 0) || (ls_vx > 1))
		goto out;

	switch (cfg) {
	case 0: //period
		if ((value < 0) || (value > 7))
			goto out;
		break;
	case 1: //credit
		if ((value < 0) || (value > 16))
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

	mtk_credit_didt((unsigned int)cpu, (unsigned int)ls_vx,
		(unsigned int)cfg, (unsigned int)value);

out:
	free_page((unsigned long)buf);

	return count;
}

static int credit_didt_en_proc_show(struct seq_file *m, void *v)
{
	unsigned int cpu, ls_vx, cfg, param, cpu_tmp;

	unsigned char credit_didt_info[NR_CREDIT_DIDT_CPU]
		[NR_CREDIT_DIDT_CHANNEL][NR_CREDIT_DIDT_CFG];

	for (cpu = CREDIT_DIDT_CPU_START_ID;
		cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {

		for (ls_vx = 0; ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {
			for (cfg = 0; cfg < NR_CREDIT_DIDT_CFG; cfg++) {
				param = ls_vx * NR_CREDIT_DIDT_CFG + cfg;

				/* coverity check */
				if (cpu >= CREDIT_DIDT_CPU_START_ID)
					cpu_tmp = cpu-CREDIT_DIDT_CPU_START_ID;
				else {
					credit_didt_err(
						"cpu(%d) is illegal\n", cpu);
					return -1;
				}

				/* coverity check */
				if (cpu_tmp < NR_CREDIT_DIDT_CPU) {
					credit_didt_info[cpu_tmp][ls_vx][cfg] =
					mt_secure_call_credit_didt(
					 MTK_SIP_KERNEL_CREDIT_DIDT_CONTROL,
					 CREDIT_DIDT_RW_READ,
					 cpu,
					 param,
					 0);

					credit_didt_debug(
					 "Get cpu=%d ls_vx=%d cfg=%d value=%d\n",
					 cpu, ls_vx, cfg,
					 credit_didt_info
					  [cpu_tmp][ls_vx][cfg]);
				} else {
					credit_didt_err(
						"cpu_tmp(%d) is illegal\n");
					return -1;
				}


			}
		}
	}

	for (cpu = CREDIT_DIDT_CPU_START_ID;
		cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {
		for (ls_vx = 0;
			ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {

			/* coverity check */
			if (cpu >= CREDIT_DIDT_CPU_START_ID)
				cpu_tmp = cpu-CREDIT_DIDT_CPU_START_ID;
			else {
				credit_didt_err(
					"cpu(%d) is illegal\n", cpu);
				return -1;
			}

			seq_printf(m,
			"cpu%d %s enable=%d\n",
			cpu, (ls_vx == 0 ? "LS" : "VX"),
			credit_didt_info[cpu_tmp][ls_vx]
				[CREDIT_DIDT_CFG_ENABLE]
				);
		}
	}

	return 0;
}

static ssize_t credit_didt_en_proc_write(struct file *file,
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

	/* coverity check */
	if (!buf)
		return -ENOMEM;

	if (sizeof(buf) >= PAGE_SIZE)
		goto out;

	buf[count] = '\0';

	if (kstrtou32((const char *)buf, 0, &value)) {
		credit_didt_debug("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	for (ls_vx = 0; ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {
		for (cpu = CREDIT_DIDT_CPU_START_ID;
			cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {
			mtk_credit_didt(cpu, ls_vx,
				CREDIT_DIDT_CFG_ENABLE,
				(value >> (ls_vx*
				(CREDIT_DIDT_CPU_END_ID+1)+cpu))&1);
		}
	}

out:
	free_page((unsigned long)buf);

	return count;
}



#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(credit_didt);
PROC_FOPS_RW(credit_didt_en);

static int create_procfs(void)
{
	struct proc_dir_entry *credit_didt_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry credit_didt_entries[] = {
		PROC_ENTRY(credit_didt),
		PROC_ENTRY(credit_didt_en),
	};

	credit_didt_dir = proc_mkdir("credit_didt", NULL);
	if (!credit_didt_dir) {
		credit_didt_debug(
			"mkdir /proc/credit_didt failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(credit_didt_entries); i++) {
		if (!proc_create(credit_didt_entries[i].name,
			0664,
			credit_didt_dir,
			credit_didt_entries[i].fops)) {
			credit_didt_debug(
				"create /proc/credit_didt/%s failed\n",
				credit_didt_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int credit_didt_probe(struct platform_device *pdev)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;
	int cpu, ls_vx;

	node = pdev->dev.of_node;
	if (!node) {
		credit_didt_debug("get credit_didt device node err\n");
		return -ENODEV;
	}

	/* enable control */
	rc = of_property_read_u32(node,
		"credit_didt_doe_enable", &credit_didt_doe_enable);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt_doe_enable from DTree; rc(%d) credit_didt_doe_enable(0x%x)\n",
			rc,
			credit_didt_doe_enable);

		if (credit_didt_doe_enable >= 0) {
			for (ls_vx = 0;
				ls_vx < NR_CREDIT_DIDT_CHANNEL; ls_vx++) {
				for (cpu = CREDIT_DIDT_CPU_START_ID;
					cpu <= CREDIT_DIDT_CPU_END_ID; cpu++) {
					mtk_credit_didt(cpu, ls_vx,
						CREDIT_DIDT_CFG_ENABLE,
						(credit_didt_doe_enable >>
						(ls_vx*
						(CREDIT_DIDT_CPU_END_ID+1)
						+cpu))&1);
				}
			}
		}
	}

	/* cpux, ls_period */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_ls_period", &credit_didt4_doe_ls_period);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_ls_period from DTree; rc(%d) credit_didt4_doe_ls_period(0x%x)\n",
			rc,
			credit_didt4_doe_ls_period);

		if (credit_didt4_doe_ls_period >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt4_doe_ls_period);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_ls_period", &credit_didt5_doe_ls_period);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_ls_period from DTree; rc(%d) credit_didt5_doe_ls_period(0x%x)\n",
			rc,
			credit_didt5_doe_ls_period);

		if (credit_didt5_doe_ls_period >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt5_doe_ls_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_ls_period", &credit_didt6_doe_ls_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_ls_period from DTree; rc(%d) credit_didt6_doe_ls_period(0x%x)\n",
			rc,
			credit_didt6_doe_ls_period);

		if (credit_didt6_doe_ls_period >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt6_doe_ls_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_ls_period", &credit_didt7_doe_ls_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_ls_period from DTree; rc(%d) credit_didt7_doe_ls_period(0x%x)\n",
			rc,
			credit_didt7_doe_ls_period);

		if (credit_didt7_doe_ls_period >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt7_doe_ls_period);
	}

	/* cpux, ls_credit */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_ls_credit", &credit_didt4_doe_ls_credit);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_ls_credit from DTree; rc(%d) credit_didt4_doe_ls_credit(0x%x)\n",
			rc,
			credit_didt4_doe_ls_credit);

		if (credit_didt4_doe_ls_credit >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt4_doe_ls_credit);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_ls_credit", &credit_didt5_doe_ls_credit);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_ls_credit from DTree; rc(%d) credit_didt5_doe_ls_credit(0x%x)\n",
			rc,
			credit_didt5_doe_ls_credit);

		if (credit_didt5_doe_ls_credit >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt5_doe_ls_credit);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_ls_credit", &credit_didt6_doe_ls_credit);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_ls_credit from DTree; rc(%d) credit_didt6_doe_ls_credit(0x%x)\n",
			rc,
			credit_didt6_doe_ls_credit);

		if (credit_didt6_doe_ls_credit >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt6_doe_ls_credit);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_ls_credit", &credit_didt7_doe_ls_credit);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_ls_credit from DTree; rc(%d) credit_didt7_doe_ls_credit(0x%x)\n",
			rc,
			credit_didt7_doe_ls_credit);

		if (credit_didt7_doe_ls_credit >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt7_doe_ls_credit);
	}

	/* cpux, ls_low_freq_period */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_ls_low_freq_period",
		&credit_didt4_doe_ls_low_freq_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_ls_low_freq_period from DTree; rc(%d) credit_didt4_doe_ls_low_freq_period(0x%x)\n",
			rc,
			credit_didt4_doe_ls_low_freq_period);

		if (credit_didt4_doe_ls_low_freq_period >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt4_doe_ls_low_freq_period);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_ls_low_freq_period",
		&credit_didt5_doe_ls_low_freq_period);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_ls_low_freq_period from DTree; rc(%d) credit_didt5_doe_ls_low_freq_period(0x%x)\n",
			rc,
			credit_didt5_doe_ls_low_freq_period);

		if (credit_didt5_doe_ls_low_freq_period >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt5_doe_ls_low_freq_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_ls_low_freq_period",
		&credit_didt6_doe_ls_low_freq_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_ls_low_freq_period from DTree; rc(%d) credit_didt6_doe_ls_low_freq_period(0x%x)\n",
			rc,
			credit_didt6_doe_ls_low_freq_period);

		if (credit_didt6_doe_ls_low_freq_period >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt6_doe_ls_low_freq_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_ls_low_freq_period",
		&credit_didt7_doe_ls_low_freq_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_ls_low_freq_period from DTree; rc(%d) credit_didt7_doe_ls_low_freq_period(0x%x)\n",
			rc,
			credit_didt7_doe_ls_low_freq_period);

		if (credit_didt7_doe_ls_low_freq_period >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt7_doe_ls_low_freq_period);
	}

	/* cpux, ls_low_freq_enable */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_ls_low_freq_enable",
		&credit_didt4_doe_ls_low_freq_enable);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_ls_low_freq_enable from DTree; rc(%d) credit_didt4_doe_ls_low_freq_enable(0x%x)\n",
			rc,
			credit_didt4_doe_ls_low_freq_enable);

		if (credit_didt4_doe_ls_low_freq_enable >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt4_doe_ls_low_freq_enable);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_ls_low_freq_enable",
		&credit_didt5_doe_ls_low_freq_enable);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_ls_low_freq_enable from DTree; rc(%d) credit_didt5_doe_ls_low_freq_enable(0x%x)\n",
			rc,
			credit_didt5_doe_ls_low_freq_enable);

		if (credit_didt5_doe_ls_low_freq_enable >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt5_doe_ls_low_freq_enable);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_ls_low_freq_enable",
		&credit_didt6_doe_ls_low_freq_enable);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_ls_low_freq_enable from DTree; rc(%d) credit_didt6_doe_ls_low_freq_enable(0x%x)\n",
			rc,
			credit_didt6_doe_ls_low_freq_enable);

		if (credit_didt6_doe_ls_low_freq_enable >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt6_doe_ls_low_freq_enable);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_ls_low_freq_enable",
		&credit_didt7_doe_ls_low_freq_enable);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_ls_low_freq_enable from DTree; rc(%d) credit_didt7_doe_ls_low_freq_enable(0x%x)\n",
			rc,
			credit_didt7_doe_ls_low_freq_enable);

		if (credit_didt7_doe_ls_low_freq_enable >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_LS,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt7_doe_ls_low_freq_enable);
	}

	/* cpux, vx_period */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_vx_period", &credit_didt4_doe_vx_period);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_vx_period from DTree; rc(%d) credit_didt4_doe_vx_period(0x%x)\n",
			rc,
			credit_didt4_doe_vx_period);

		if (credit_didt4_doe_vx_period >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt4_doe_vx_period);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_vx_period", &credit_didt5_doe_vx_period);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_vx_period from DTree; rc(%d) credit_didt5_doe_vx_period(0x%x)\n",
			rc,
			credit_didt5_doe_vx_period);

		if (credit_didt5_doe_vx_period >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt5_doe_vx_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_vx_period", &credit_didt6_doe_vx_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_vx_period from DTree; rc(%d) credit_didt6_doe_vx_period(0x%x)\n",
			rc,
			credit_didt6_doe_vx_period);

		if (credit_didt6_doe_vx_period >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt6_doe_vx_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_vx_period", &credit_didt7_doe_vx_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_vx_period from DTree; rc(%d) credit_didt7_doe_vx_period(0x%x)\n",
			rc,
			credit_didt7_doe_vx_period);

		if (credit_didt7_doe_vx_period >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_PERIOD,
				credit_didt7_doe_vx_period);
	}

	/* cpux, vx_credit */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_vx_credit", &credit_didt4_doe_vx_credit);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_vx_credit from DTree; rc(%d) credit_didt4_doe_vx_credit(0x%x)\n",
			rc,
			credit_didt4_doe_vx_credit);

		if (credit_didt4_doe_vx_credit >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt4_doe_vx_credit);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_vx_credit", &credit_didt5_doe_vx_credit);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_vx_credit from DTree; rc(%d) credit_didt5_doe_vx_credit(0x%x)\n",
			rc,
			credit_didt5_doe_vx_credit);

		if (credit_didt5_doe_vx_credit >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt5_doe_vx_credit);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_vx_credit", &credit_didt6_doe_vx_credit);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_vx_credit from DTree; rc(%d) credit_didt6_doe_vx_credit(0x%x)\n",
			rc,
			credit_didt6_doe_vx_credit);

		if (credit_didt6_doe_vx_credit >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt6_doe_vx_credit);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_vx_credit", &credit_didt7_doe_vx_credit);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_vx_credit from DTree; rc(%d) credit_didt7_doe_vx_credit(0x%x)\n",
			rc,
			credit_didt7_doe_vx_credit);

		if (credit_didt7_doe_vx_credit >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_CREDIT,
				credit_didt7_doe_vx_credit);
	}

	/* cpux, vx_low_freq_period */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_vx_low_freq_period",
		&credit_didt4_doe_vx_low_freq_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_vx_low_freq_period from DTree; rc(%d) credit_didt4_doe_vx_low_freq_period(0x%x)\n",
			rc,
			credit_didt4_doe_vx_low_freq_period);

		if (credit_didt4_doe_vx_low_freq_period >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt4_doe_vx_low_freq_period);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_vx_low_freq_period",
		&credit_didt5_doe_vx_low_freq_period);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_vx_low_freq_period from DTree; rc(%d) credit_didt5_doe_vx_low_freq_period(0x%x)\n",
			rc,
			credit_didt5_doe_vx_low_freq_period);

		if (credit_didt5_doe_vx_low_freq_period >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt5_doe_vx_low_freq_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_vx_low_freq_period",
		&credit_didt6_doe_vx_low_freq_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_vx_low_freq_period from DTree; rc(%d) credit_didt6_doe_vx_low_freq_period(0x%x)\n",
			rc,
			credit_didt6_doe_vx_low_freq_period);

		if (credit_didt6_doe_vx_low_freq_period >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt6_doe_vx_low_freq_period);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_vx_low_freq_period",
		&credit_didt7_doe_vx_low_freq_period);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_vx_low_freq_period from DTree; rc(%d) credit_didt7_doe_vx_low_freq_period(0x%x)\n",
			rc,
			credit_didt7_doe_vx_low_freq_period);

		if (credit_didt7_doe_vx_low_freq_period >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
				credit_didt7_doe_vx_low_freq_period);
	}

	/* cpux, vx_low_freq_enable */
	rc = of_property_read_u32(node,
		"credit_didt4_doe_vx_low_freq_enable",
		&credit_didt4_doe_vx_low_freq_enable);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt4_doe_vx_low_freq_enable from DTree; rc(%d) credit_didt4_doe_vx_low_freq_enable(0x%x)\n",
			rc,
			credit_didt4_doe_vx_low_freq_enable);

		if (credit_didt4_doe_vx_low_freq_enable >= 0)
			mtk_credit_didt(4, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt4_doe_vx_low_freq_enable);
	}
	rc = of_property_read_u32(node,
		"credit_didt5_doe_vx_low_freq_enable",
		&credit_didt5_doe_vx_low_freq_enable);

	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt5_doe_vx_low_freq_enable from DTree; rc(%d) credit_didt5_doe_vx_low_freq_enable(0x%x)\n",
			rc,
			credit_didt5_doe_vx_low_freq_enable);

		if (credit_didt5_doe_vx_low_freq_enable >= 0)
			mtk_credit_didt(5, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt5_doe_vx_low_freq_enable);
	}

	rc = of_property_read_u32(node,
		"credit_didt6_doe_vx_low_freq_enable",
		&credit_didt6_doe_vx_low_freq_enable);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt6_doe_vx_low_freq_enable from DTree; rc(%d) credit_didt6_doe_vx_low_freq_enable(0x%x)\n",
			rc,
			credit_didt6_doe_vx_low_freq_enable);

		if (credit_didt6_doe_vx_low_freq_enable >= 0)
			mtk_credit_didt(6, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt6_doe_vx_low_freq_enable);
	}

	rc = of_property_read_u32(node,
		"credit_didt7_doe_vx_low_freq_enable",
		&credit_didt7_doe_vx_low_freq_enable);
	if (!rc) {
		credit_didt_debug("[xxxxcredit_didt] credit_didt7_doe_vx_low_freq_enable from DTree; rc(%d) credit_didt7_doe_vx_low_freq_enable(0x%x)\n",
			rc,
			credit_didt7_doe_vx_low_freq_enable);

		if (credit_didt7_doe_vx_low_freq_enable >= 0)
			mtk_credit_didt(7, CREDIT_DIDT_CHANNEL_VX,
				CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
				credit_didt7_doe_vx_low_freq_enable);
	}

	/* dump register information to picachu buf */
#ifdef CONFIG_OF_RESERVED_MEM
	credit_didt_reserve_memory_init(0);
#endif

	credit_didt_debug("credit_didt probe ok!!\n");
#endif
#endif
	return 0;
}

static int credit_didt_suspend(struct platform_device *pdev, pm_message_t state)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
//	credit_didt_reserve_memory_init(1);
#endif
#endif

	return 0;
}

static int credit_didt_resume(struct platform_device *pdev)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
//	credit_didt_reserve_memory_init(2);
#endif
#endif

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_credit_didt_of_match[] = {
	{ .compatible = "mediatek,credit_didt", },
	{},
};
#endif

static struct platform_driver credit_didt_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= credit_didt_probe,
	.suspend	= credit_didt_suspend,
	.resume		= credit_didt_resume,
	.driver		= {
		.name   = "mt-credit_didt",
#ifdef CONFIG_OF
		.of_match_table = mt_credit_didt_of_match,
#endif
	},
};

static int __init __credit_didt_init(void)
{
	int err = 0;

	create_procfs();

	err = platform_driver_register(&credit_didt_driver);
	if (err) {
		credit_didt_debug("CREDIT_DIDT driver callback register failed..\n");
		return err;
	}

	return 0;
}

static void __exit __credit_didt_exit(void)
{
	credit_didt_debug("credit_didt de-initialization\n");
}


module_init(__credit_didt_init);
module_exit(__credit_didt_exit);

MODULE_DESCRIPTION("MediaTek CREDIT_DIDT Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_CREDIT_DIDT_C__
