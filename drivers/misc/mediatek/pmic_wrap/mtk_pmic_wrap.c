/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
/*#include <mach/mt_typedefs.h>*/
#include <linux/timer.h>
#include <mach/mtk_pmic_wrap.h>
#include <linux/syscore_ops.h>
#include <linux/regmap.h>
#include <linux/soc/mediatek/pmic_wrap.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>

#define PMIC_WRAP_DEVICE "pmic_wrap"
#define VERSION     "Revision"

#if !defined CONFIG_MTK_PMIC_WRAP
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

/* ************************************************************************* */
/* --external API for pmic_wrap user---------------------------------------- */
/* ************************************************************************* */
s32 pwrap_wacs2(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	if (mt_wrp.wacs2_hal != NULL)
		return mt_wrp.wacs2_hal(write, adr, wdata, rdata);

	pr_notice("[WRAP] driver need registered!!");
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
/*
 *#if defined PWRAP_TRACE
 *	tracepwrap(adr, wdata);
 *#endif
 */
#ifdef CONFIG_MACH_MT6765
	s32 ret = 0;

	if ((adr == 0x1508) && ((wdata & 0x1) == 0x0)) {
		pr_notice("[PWRAP] Illegal write 0x1508 bit 0 to 0\n");
		pr_notice("[PWRAP] Error: %s, line: %d\n", __func__, __LINE__);
		pr_notice("[PWRAP] addr: 0x%x, wdata: 0x%x\n", adr, wdata);
		return -1;
	} else if (adr == 0x1510 || adr == 0x1512 || adr == 0x1514) {
		ret = pwrap_wacs2(PWRAP_WRITE, 0x1510, 0xB, 0);
		if (ret != 0) {
			pr_notice("[PWRAP] Recovery Fail, ret=%d\n", ret);
			pr_notice("[PWRAP] %s, line: %d\n", __func__, __LINE__);
			pr_notice("[PWRAP] adr:0x%x, wdata:0x%x\n", adr, wdata);
			return ret;
		}
	} else if (adr == 0x1516 || adr == 0x1518 || adr == 0x151a) {
		ret = pwrap_wacs2(PWRAP_WRITE, 0x1516, 0x10A, 0);
		if (ret != 0) {
			pr_notice("[PWRAP] Recovery Fail, ret=%d\n", ret);
			pr_notice("[PWRAP] %s, line: %d\n", __func__, __LINE__);
			pr_notice("[PWRAP] adr:0x%x, wdata:0x%x\n", adr, wdata);
			return ret;
		}
	} else if ((adr == 0x19D0) && ((wdata & 0x1) == 0x0)) {
		pr_notice("[PWRAP] Illegal write 0x19D0 bit 0 to 0\n");
		pr_notice("[PWRAP] Error: %s, line: %d\n", __func__, __LINE__);
		pr_notice("[PWRAP] addr: 0x%x, wdata: 0x%x\n", adr, wdata);
		return -1;
	} else if (adr == 0x19D8 || adr == 0x19DA || adr == 0x19DC) {
		ret = pwrap_wacs2(PWRAP_WRITE, 0x19D8, 0xB, 0);
		if (ret != 0) {
			pr_notice("[PWRAP] Recovery Fail, ret=%d\n", ret);
			pr_notice("[PWRAP] %s, line: %d\n", __func__, __LINE__);
			pr_notice("[PWRAP] adr:0x%x, wdata:0x%x\n", adr, wdata);
			return ret;
		}
	} else if (adr == 0x19DE || adr == 0x19E0 || adr == 0x19E2) {
		ret = pwrap_wacs2(PWRAP_WRITE, 0x19DE, 0xA, 0);
		if (ret != 0) {
			pr_notice("[PWRAP] Recovery Fail, ret=%d\n", ret);
			pr_notice("[PWRAP] %s, line: %d\n", __func__, __LINE__);
			pr_notice("[PWRAP] adr:0x%x, wdata:0x%x\n", adr, wdata);
			return ret;
		}
	} else if ((adr == 0x170E) && (wdata & 0x80)) {
		pr_notice("[PWRAP] Illegal write 0x170E bit 7 to 1\n");
		pr_notice("[PWRAP] Error: %s, line: %d\n", __func__, __LINE__);
		pr_notice("[PWRAP] addr: 0x%x, wdata: 0x%x\n", adr, wdata);
		return -1;
	} else {
		ret = pwrap_wacs2(PWRAP_WRITE, adr, wdata, 0);
		if (ret != 0) {
			pr_notice("[PWRAP] Write Fail, ret=%d\n", ret);
			pr_notice("[PWRAP] %s, line: %d\n", __func__, __LINE__);
			pr_notice("[PWRAP] adr:0x%x, wdata:0x%x\n", adr, wdata);
			return ret;
		}
	}
	return ret;
#else
	return pwrap_wacs2(PWRAP_WRITE, adr, wdata, 0);
#endif
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

	return snprintf(buf, PAGE_SIZE, "%s\n", "[WRAP]driver need register!!");
}

static ssize_t mt_pwrap_store(struct device_driver *driver,
					const char *buf, size_t count)
{
	if (mt_wrp.store_hal != NULL)
		return mt_wrp.store_hal(buf, count);

	pr_notice("[WRAP]driver need registered!!");
	return count;
}

DRIVER_ATTR(pwrap, 0664, mt_pwrap_show, mt_pwrap_store);
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
		pr_notice("[WRAP]Fail to register mt_wrp");
	ret = driver_create_file(&mt_wrp.driver, &driver_attr_pwrap);
	if (ret)
		pr_notice("[WRAP]Fail to create mt_wrp sysfs files");
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
#else

static struct regmap *pmic_regmap;
static spinlock_t   wrp_lock = __SPIN_LOCK_UNLOCKED(lock);

s32 pwrap_read(u32 adr, u32 *rdata)
{
	int ret = 0;
	unsigned long flags = 0;

	if (pmic_regmap) {
		spin_lock_irqsave(&wrp_lock, flags);
		ret = regmap_read(pmic_regmap, adr, rdata);
		spin_unlock_irqrestore(&wrp_lock, flags);
	} else
		pr_notice("%s %d Error.\n", __func__, __LINE__);
	return ret;
}
EXPORT_SYMBOL(pwrap_read);

s32 pwrap_write(u32 adr, u32 wdata)
{
	int ret = 0;
	unsigned long flags = 0;

	if (pmic_regmap) {
		spin_lock_irqsave(&wrp_lock, flags);
		ret = regmap_write(pmic_regmap, adr, wdata);
		spin_unlock_irqrestore(&wrp_lock, flags);
	} else
		pr_notice("%s %d Error.\n", __func__, __LINE__);
	return ret;
}
EXPORT_SYMBOL(pwrap_write);

static int __init mt_pwrap_init(void)
{
	struct device_node *node, *pwrap_node;

	pr_info("%s\n", __func__);
	node = of_find_compatible_node(NULL, NULL, "mediatek,pwraph");
	pwrap_node = of_parse_phandle(node, "mediatek,pwrap-regmap", 0);
	if (pwrap_node) {
		pmic_regmap = pwrap_node_to_regmap(pwrap_node);
		if (IS_ERR(pmic_regmap)) {
			pr_notice("%s %d Error.\n", __func__, __LINE__);
			return PTR_ERR(pmic_regmap);
		}
	} else {
		pr_notice("%s %d Error.\n", __func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}
subsys_initcall(mt_pwrap_init);
#endif

MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("pmic_wrapper Driver  Revision");
MODULE_LICENSE("GPL");
