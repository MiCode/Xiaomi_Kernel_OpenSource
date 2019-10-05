/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_debugfs.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_lbat_service.h"

/*-------pmic_dbg_level global variable-------*/
unsigned int gPMICDbgLvl;
unsigned int gPMICHKDbgLvl;
unsigned int gPMICIRQDbgLvl;
unsigned int gPMICREGDbgLvl;
unsigned int gPMICCOMDbgLvl; /* so far no used */

#define DUMP_ALL_REG 0

int pmic_pre_wdt_reset(void)
{
	int ret = 0;
/* remove dump exception status before wdt, since we will recore
 * it at next boot preloader
 */
#if 0
	preempt_disable();
	local_irq_disable();
	pr_info(PMICTAG "[%s][pmic_boot_status]\n", __func__);
	pmic_dump_exception_reg();
#if DUMP_ALL_REG
	pmic_dump_register();
#endif
#endif
	return ret;

}

int pmic_pre_condition1(void)
{
	return 0;
}
int pmic_pre_condition2(void)
{
	return 0;
}
int pmic_pre_condition3(void)
{
	return 0;
}
int pmic_post_condition1(void)
{
	return 0;
}
int pmic_post_condition2(void)
{
	return 0;
}
int pmic_post_condition3(void)
{
	return 0;
}

/*****************************************************************************
 * mt-pmic dev_attr APIs
 ******************************************************************************/
unsigned int g_reg_value;
static ssize_t show_pmic_access(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_reg_value);
	return sprintf(buf, "0x%x\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		pr_info("[%s] size is %d, buf is %s\n"
			, __func__, (int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			pr_info("[%s] write PMU reg 0x%x with value 0x%x !\n"
				, __func__, reg_address, reg_value);
			ret = pmic_config_interface(
					reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = pmic_read_interface(
					reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_info("[%s] read PMU reg 0x%x with value 0x%x !\n"
				, __func__, reg_address, g_reg_value);
			pr_info("[%s] use \"cat pmic_access\" to get value\n"
				,  __func__);
		}
	}
	return size;
}

/*664*/
static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);

/*
 * PMIC dump exception
 */
static int pmic_dump_exception_show(struct seq_file *s, void *unused)
{
	both_dump_exception_reg(s);
	return 0;
}
static int pmic_dump_exception_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_dump_exception_show, NULL);
}
const struct file_operations pmic_dump_exception_operations = {
	.open    = pmic_dump_exception_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*
 * PMIC debug level
 */

/*-------pmic_dbg_level-------*/
static ssize_t pmic_dbg_level_write(struct file *file,
				    const char __user *buf,
				    size_t size,
				    loff_t *ppos)
{
	char info[10];
	unsigned int value = 0;
	int ret = 0;

	memset(info, 0, 10);

	ret = simple_write_to_buffer(info, sizeof(info) - 1, ppos, buf, size);
	if (ret < 0)
		return ret;

	ret = kstrtou32(info, 16, (unsigned int *)&value);
	if (ret)
		return ret;

	pr_info("[%s] value=0x%x\n", __func__, value);
	if (value != 0xFFFF) {
		pmic_dbg_level_set(value);
		pr_info("D %d, HK %d, IRQ %d, REG %d, COM %d\n",
			gPMICDbgLvl, gPMICHKDbgLvl, gPMICIRQDbgLvl,
			gPMICREGDbgLvl, gPMICCOMDbgLvl);
	} else {
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		pmic_ipi_test_code();
		pr_info("pmic_ipi_test_code\n");
#endif
	}

	return size;
}

static int pmic_dbg_level_show(struct seq_file *s, void *unused)
{
	seq_puts(s, "4:PMIC_LOG_DBG\n");
	seq_puts(s, "3:PMIC_LOG_INFO\n");
	seq_puts(s, "2:PMIC_LOG_NOT\n");
	seq_puts(s, "1:PMIC_LOG_WARN\n");
	seq_puts(s, "0:PMIC_LOG_ERR\n");
	seq_printf(s, "PMIC_Dbg_Lvl = %d\n", gPMICDbgLvl);
	seq_printf(s, "PMIC_HK_Dbg_Lvl = %d\n", gPMICHKDbgLvl);
	seq_printf(s, "PMIC_IRQ_Dbg_Lvl = %d\n", gPMICIRQDbgLvl);
	seq_printf(s, "PMIC_REG_Dbg_Lvl = %d\n", gPMICREGDbgLvl);
	seq_printf(s, "PMIC_COM_Dbg_Lvl = %d\n", gPMICCOMDbgLvl);
	return 0;
}

static int pmic_dbg_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_dbg_level_show, NULL);
}

static const struct file_operations pmic_dbg_level_operations = {
	.open    = pmic_dbg_level_open,
	.read    = seq_read,
	.write   = pmic_dbg_level_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*
 * PMIC reg dump log
 */
/*----------------pmic reg dump machanism-----------------------*/
static int proc_dump_register_show(struct seq_file *m, void *v)
{
	seq_puts(m, "********** dump PMIC registers**********\n");
	pmic_dump_register(m);

	return 0;
}

static int proc_dump_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dump_register_show, NULL);
}

static const struct file_operations pmic_dump_register_proc_fops = {
	.open = proc_dump_register_open,
	.read = seq_read,
};

int __attribute__ ((weak)) pmic_irq_debug_init(struct dentry *debug_dir)
{
	return 0;
}

int pmic_debug_init(struct platform_device *dev)
{
	struct dentry *mtk_pmic_dir;
	int ret_device_file = 0;

	mtk_pmic_dir = debugfs_create_dir("mtk_pmic", NULL);
	if (!mtk_pmic_dir) {
		pr_notice(PMICTAG "fail to mkdir /sys/kernel/debug/mtk_pmic\n");
		return -ENOMEM;
	}

	/*--/sys/kernel/debug/mtk_pmic--*/
	debugfs_create_file("dump_pmic_reg", 0644,
				mtk_pmic_dir, NULL,
				&pmic_dump_register_proc_fops);
	debugfs_create_file("pmic_dump_exception", (S_IFREG | 0444),
				mtk_pmic_dir, NULL,
				&pmic_dump_exception_operations);
	debugfs_create_file("pmic_dbg_level", (S_IFREG | 0444),
				mtk_pmic_dir, NULL,
				&pmic_dbg_level_operations);

	pmic_regulator_debug_init(dev, mtk_pmic_dir);

	pmic_throttling_dlpt_debug_init(dev, mtk_pmic_dir);

	pmic_irq_debug_init(mtk_pmic_dir);

	lbat_debug_init(mtk_pmic_dir);

	PMICLOG("%s debugfs done\n", __func__);

	/*--/sys/devices/platform/mt-pmic/ --*/
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_pmic_access);
	PMICLOG("%s dev attr done\n", __func__);

	return 0;
}

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MT PMIC DEBUGFS Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0_M");
