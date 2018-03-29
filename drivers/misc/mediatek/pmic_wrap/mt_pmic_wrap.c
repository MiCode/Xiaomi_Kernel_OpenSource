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

/******************************************************************************
 * pmic_wrapper.c - Linux pmic_wrapper Driver
 *
 *
 * DESCRIPTION:
 *     This file provid the other drivers PMIC wrapper relative functions
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
/*#include <mach/mt_typedefs.h>*/
#include <linux/timer.h>
#include <linux/slab.h>
#include <mt_pmic_wrap.h>
#include <linux/syscore_ops.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#define PMIC_WRAP_DEVICE "pmic_wrap"
#define VERSION     "Revision"

static struct mt_pmic_wrap_driver mt_wrp = {
	.driver = {
		   .name = "pmic_wrap",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
};

struct mt_pmic_wrap_driver *get_mt_pmic_wrap_drv(void)
{
	return &mt_wrp;
}
/*this function only used for ROME plus*/
int check_pmic_wrap_init(void)
{
	if (mt_wrp.wacs2_hal == NULL)
		return -1;
	else
		return 0;
}

/* ****************************************************************************** */
/* --external API for pmic_wrap user------------------------------------------------- */
/* ****************************************************************************** */
s32 pwrap_wacs2(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	if (mt_wrp.wacs2_hal != NULL)
		return mt_wrp.wacs2_hal(write, adr, wdata, rdata);

	pr_err("[WRAP]" "driver need registered!!");
	return -5;

}
EXPORT_SYMBOL(pwrap_wacs2);
s32 pwrap_read(u32 adr, u32 *rdata)
{
	return pwrap_wacs2(PWRAP_READ, adr, 0, rdata);
}
EXPORT_SYMBOL(pwrap_read);

s32 pwrap_write(u32 adr, u32 wdata)
{
#if defined PWRAP_TRACE
	tracepwrap(adr, wdata);
#endif
	return pwrap_wacs2(PWRAP_WRITE, adr, wdata, 0);
}
EXPORT_SYMBOL(pwrap_write);
/********************************************************************/
/********************************************************************/
/* return value : EINT_STA: [0]: CPU IRQ status in PMIC1 */
/* [1]: MD32 IRQ status in PMIC1 */
/* [2]: CPU IRQ status in PMIC2 */
/* [3]: RESERVED */
/********************************************************************/
u32 pmic_wrap_eint_status(void)
{
	return mt_pmic_wrap_eint_status();
}
EXPORT_SYMBOL(pmic_wrap_eint_status);

/********************************************************************/
/* set value(W1C) : EINT_CLR:       [0]: CPU IRQ status in PMIC1 */
/* [1]: MD32 IRQ status in PMIC1 */
/* [2]: CPU IRQ status in PMIC2 */
/* [3]: RESERVED */
/* para: offset is shift of clear bit which needs to clear */
/********************************************************************/
void pmic_wrap_eint_clr(int offset)
{
	mt_pmic_wrap_eint_clr(offset);
}
EXPORT_SYMBOL(pmic_wrap_eint_clr);
/************************************************************************/
static ssize_t mt_pwrap_show(struct device_driver *driver, char *buf)
{
	if (mt_wrp.show_hal != NULL)
		return mt_wrp.show_hal(buf);

	return snprintf(buf, PAGE_SIZE, "%s\n", "[WRAP]driver need registered!! ");
}

static ssize_t mt_pwrap_store(struct device_driver *driver, const char *buf, size_t count)
{
	if (mt_wrp.store_hal != NULL)
		return mt_wrp.store_hal(buf, count);

	pr_err("[WRAP]" "driver need registered!!");
	return count;
}

DRIVER_ATTR(pwrap, 0664, mt_pwrap_show, mt_pwrap_store);
/************************************************************************/
unsigned int gPWRAPHCK;
unsigned int gPWRAPDBGADDR;
/*-------set function-------*/
static unsigned int pwrap_trace_level_set(unsigned int level, unsigned int addr)
{
	gPWRAPHCK = level > PWRAP_HCK_LEVEL ? PWRAP_HCK_LEVEL : level;
	gPWRAPDBGADDR = addr;
	return 0;
}

/*-------pwrap_trace-------*/
static ssize_t pwrap_trace_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char *info, *pvalue, *paddr;
	unsigned int value = 0;
	unsigned int addr = 0;
	int ret = 0;

	info = kmalloc_array(size, sizeof(char), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	memset(info, 0, size);

	if (copy_from_user(info, buf, size))
		return -EFAULT;

	info[size-1] = '\0';

	if (size != 0) {
		if (size > 2) {
			pvalue = strsep(&info, " ");
			ret = kstrtou32(pvalue, 16, (unsigned int *)&value);
		}

		if (size > 2) {
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16);*/
			/*pvalue = (char *)buf + 1;*/
			paddr =  strsep(&info, " ");
			ret = kstrtou32(paddr, 16, (unsigned int *)&addr);
		}
	}

	pwrap_trace_level_set(value, addr);

	kfree(info);

	return size;
}

static int pwrap_trace_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "pwrap_trace_write_level = %d\n", gPWRAPHCK);
	seq_printf(s, "pwrap_trace_write_addr = 0x%x\n", gPWRAPDBGADDR);
	return 0;
}

static int pwrap_trace_open(struct inode *inode, struct file *file)
{
	return single_open(file, pwrap_trace_show, NULL);
}

static const struct file_operations pwrap_trace_operations = {
	.open    = pwrap_trace_open,
	.read    = seq_read,
	.write   = pwrap_trace_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init pwrap_debugfs_init(void)
{
	struct dentry *mt_pwrap;

	mt_pwrap = debugfs_create_dir("mt_pwrap", NULL);
	if (!mt_pwrap)
		pr_err("create dir mt_pwrap fail\n");

	debugfs_create_file("pwrap_trace", (S_IFREG | S_IRUGO), mt_pwrap, NULL, &pwrap_trace_operations);

	return 0;
}
late_initcall(pwrap_debugfs_init);
/*-----suspend/resume for pmic_wrap-------------------------------------------*/
/* infra power down while suspend,pmic_wrap will gate clock after suspend. */
/* so,need to init PPB when resume. */
/* only affect PWM and I2C */
static int pwrap_suspend(void)
{
	/* PWRAPLOG("pwrap_suspend\n"); */
	if (mt_wrp.suspend != NULL)
		return mt_wrp.suspend();
	return 0;
}

static void pwrap_resume(void)
{
	if (mt_wrp.resume != NULL)
		mt_wrp.resume();
}

static struct syscore_ops pwrap_syscore_ops = {
	.resume = pwrap_resume,
	.suspend = pwrap_suspend,
};

static int __init mt_pwrap_init(void)
{
	u32 ret = 0;

	ret = driver_register(&mt_wrp.driver);
	if (ret)
		pr_err("[WRAP]" "Fail to register mt_wrp");
	ret = driver_create_file(&mt_wrp.driver, &driver_attr_pwrap);
	if (ret)
		pr_err("[WRAP]" "Fail to create mt_wrp sysfs files");
	/* PWRAPLOG("pwrap_init_ops\n"); */
	register_syscore_ops(&pwrap_syscore_ops);
	return ret;

}
postcore_initcall(mt_pwrap_init);
/* device_initcall(mt_pwrap_init); */
/* ---------------------------------------------------------------------*/
/* static void __exit mt_pwrap_exit(void) */
/* { */
/* platform_driver_unregister(&mt_pwrap_driver); */
/* return; */
/* } */
/* ---------------------------------------------------------------------*/
/* postcore_initcall(mt_pwrap_init); */
/* module_exit(mt_pwrap_exit); */
/* #define PWRAP_EARLY_PORTING */
/*-----suspend/resume for pmic_wrap-------------------------------------------*/
/* infra power down while suspend,pmic_wrap will gate clock after suspend. */
/* so,need to init PPB when resume. */
/* only affect PWM and I2C */

/* static struct syscore_ops pwrap_syscore_ops = { */
/* .resume   = pwrap_resume, */
/* .suspend  = pwrap_suspend, */
/* }; */
/*  */
/* static int __init pwrap_init_ops(void) */
/* { */
/* PWRAPLOG("pwrap_init_ops\n"); */
/* register_syscore_ops(&pwrap_syscore_ops); */
/* return 0; */
/* } */
/* device_initcall(pwrap_init_ops); */

MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("pmic_wrapper Driver  Revision");
MODULE_LICENSE("GPL");
