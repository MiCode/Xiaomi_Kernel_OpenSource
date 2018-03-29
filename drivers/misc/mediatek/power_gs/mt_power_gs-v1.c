/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>

#include "mt_power_gs_array.h"

#if defined CONFIG_ARCH_MT6797
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#endif

#define gs_read(addr) (*(volatile u32 *)(addr))

struct proc_dir_entry *mt_power_gs_dir = NULL;

#define DEBUG_BUF_SIZE 200
static char buf[DEBUG_BUF_SIZE] = { 0 };

#if defined CONFIG_ARCH_MT6797
static int is_checking_md;
static int mt_power_gs_md_setting_read(struct seq_file *m, void *v)
{
	seq_printf(m, "is_checking_md= %d\n", is_checking_md);
	return 0;
}


static ssize_t mt_power_gs_md_setting_write(struct file *file, const char __user *buffer,
					   size_t count, loff_t *data)
{
	char desc[32];
	int temp;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';


	if (kstrtoint(desc, 10, &temp) == 0) {
		is_checking_md = temp;
		return count;
	}

	return -EINVAL;
}
#endif

static u16 gs_pmic_read(u16 reg)
{
	u32 ret = 0;
	u32 reg_val = 0;

	ret = pmic_read_interface_nolock(reg, &reg_val, 0xFFFF, 0x0);

	return (u16)reg_val;
}

static void mt_power_gs_compare(char *scenario, char *pmic_name,
				const unsigned int *pmic_gs, unsigned int pmic_gs_len)
{
	unsigned int i, k, val1, val2, diff;
	char *p;

	/*pr_warn("Scenario - PMIC - Addr  - Value  - Mask   - Golden - Wrong Bit\n"); for log reduction*/

	for (i = 0; i < pmic_gs_len; i += 3) {
		val1 = gs_pmic_read(pmic_gs[i]) & pmic_gs[i + 1];
		val2 = pmic_gs[i + 2] & pmic_gs[i + 1];

		if (val1 != val2) {
			p = buf;
			p += snprintf(p, sizeof(buf), "%s - %s - 0x%x - 0x%04x - 0x%04x - 0x%04x -",
				     scenario, pmic_name, pmic_gs[i], gs_pmic_read(pmic_gs[i]),
				     pmic_gs[i + 1], pmic_gs[i + 2]);

			for (k = 0, diff = val1 ^ val2; diff != 0; k++, diff >>= 1) {
				if ((diff % 2) != 0)
					p += snprintf(p, sizeof(buf) - (p - buf), " %d", k);
			}
			pr_warn("%s\n", buf);
		}
	}
}

void mt_power_gs_dump_suspend(void)
{
#ifdef CONFIG_ARCH_MT6580
	mt_power_gs_compare("Suspend ", "6325",
			    MT6325_PMIC_REG_gs_flightmode_suspend_mode,
			    MT6325_PMIC_REG_gs_flightmode_suspend_mode_len);
#elif defined CONFIG_ARCH_MT6735 || defined CONFIG_ARCH_MT6735M || defined CONFIG_ARCH_MT6753
	mt_power_gs_compare("Suspend ", "6328",
			    MT6328_PMIC_REG_gs_flightmode_suspend_mode,
			    MT6328_PMIC_REG_gs_flightmode_suspend_mode_len);
#elif defined CONFIG_ARCH_MT6755
#if defined CONFIG_MTK_PMIC_CHIP_MT6353
	mt_power_gs_compare("Suspend ", "6353",
			    MT6353_PMIC_REG_gs_flightmode_suspend_mode,
			    MT6353_PMIC_REG_gs_flightmode_suspend_mode_len);
#else
	mt_power_gs_compare("Suspend ", "6351",
			    MT6351_PMIC_REG_gs_flightmode_suspend_mode,
			    MT6351_PMIC_REG_gs_flightmode_suspend_mode_len);
#endif
#elif defined CONFIG_ARCH_MT6797
	if (is_checking_md)
		mt_power_gs_compare("Suspend ", "6351",
				MT6351_PMIC_REG_gs_flightmode_suspend_mode,
				MT6351_PMIC_REG_gs_flightmode_suspend_mode_len);
	else
		mt_power_gs_compare("Suspend ", "6351",
				MT6351_PMIC_REG_gs_suspend_mode,
				MT6351_PMIC_REG_gs_suspend_mode_len);
#endif
}
EXPORT_SYMBOL(mt_power_gs_dump_suspend);

void mt_power_gs_dump_dpidle(void)
{
#if defined CONFIG_ARCH_MT6755 || defined CONFIG_ARCH_MT6797
#if defined CONFIG_MTK_PMIC_CHIP_MT6353
	mt_power_gs_compare("DPIdle  ", "6353",
			    MT6353_PMIC_REG_gs_early_suspend_deep_idle_mode,
			    MT6353_PMIC_REG_gs_early_suspend_deep_idle_mode_len);
#else
	mt_power_gs_compare("DPIdle  ", "6351",
			    MT6351_PMIC_REG_gs_early_suspend_deep_idle_mode,
			    MT6351_PMIC_REG_gs_early_suspend_deep_idle_mode_len);
#endif
#endif
}
EXPORT_SYMBOL(mt_power_gs_dump_dpidle);

#if defined CONFIG_ARCH_MT6797
static int mt_power_gs_md_setting_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_power_gs_md_setting_read, NULL);
}

static const struct file_operations mt_power_gs_md_setting_fops = {
	.owner = THIS_MODULE,
	.open = mt_power_gs_md_setting_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_power_gs_md_setting_write,
	.release = single_release,
};
#endif

static void __exit mt_power_gs_exit(void)
{
}

static int __init mt_power_gs_init(void)
{
	mt_power_gs_dir = proc_mkdir("mt_power_gs", NULL);

	if (!mt_power_gs_dir)
		pr_err("[%s]: mkdir /proc/mt_power_gs failed\n", __func__);

#if defined CONFIG_ARCH_MT6797
	proc_create("check_md_setting", S_IRUGO | S_IWUSR, mt_power_gs_dir,
			&mt_power_gs_md_setting_fops);
#endif
	return 0;
}

module_init(mt_power_gs_init);
module_exit(mt_power_gs_exit);

MODULE_DESCRIPTION("MT Low Power Golden Setting");

