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

#include <linux/pm.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "mt-plat/mtk_rtc.h"

/*********************************************************************
 * FUNCTION DEFINATIONS
 ********************************************************************/
/* todo: recover clock related code later if MET needs it */
#if 0
#define TAG	"[pm_init] "

#define clk_err(fmt, args...)	pr_err(TAG fmt, ##args)
#define clk_warn(fmt, args...)	pr_warn(TAG fmt, ##args)
#define clk_info(fmt, args...)	pr_notice(TAG fmt, ##args)
#define clk_dbg(fmt, args...)	pr_info(TAG fmt, ##args)
#define clk_ver(fmt, args...)	pr_debug(TAG fmt, ##args)

#ifndef BIT
#define BIT(_bit_)		(u32)(1U << (_bit_))
#endif

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	do { writel(val, addr); dsb(); } while (0)
#define clk_setl(addr, val)	clk_writel(addr, clk_readl(addr) | (val))
#define clk_clrl(addr, val)	clk_writel(addr, clk_readl(addr) & ~(val))
#define clk_writel_mask(addr, h, l, v)
	clk_writel(addr, (clk_readl(addr) & ~GENMASK(h, l)) | ((v) << (l)));

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

static void __iomem *topckgen_base;	/* 0x10000000 */
static void __iomem *infrasys_base;	/* 0x10001000 */
static void __iomem *mcucfg_base;	/* 0x10200000 */

#define TOPCKGEN_REG(ofs)	(topckgen_base + ofs)
#define INFRA_REG(ofs)		(infrasys_base + ofs)
#define MCUCFG_REG(ofs)		(mcucfg_base + ofs)

#define CLK_CFG_8		TOPCKGEN_REG(0x100)
#define CLK_CFG_9		TOPCKGEN_REG(0x104)
#define CLK_MISC_CFG_1		TOPCKGEN_REG(0x214)
#define CLK_MISC_CFG_2		TOPCKGEN_REG(0x218)
#define CLK26CALI_0		TOPCKGEN_REG(0x220)
#define CLK26CALI_1		TOPCKGEN_REG(0x224)
#define CLK26CALI_2		TOPCKGEN_REG(0x228)
#define INFRA_TOPCKGEN_DCMCTL	INFRA_REG(0x0010)
#define MCU_26C			MCUCFG_REG(0x026C)
#define ARMPLL_JIT_CTRL		MCUCFG_REG(0x064C)

enum FMETER_TYPE {
	ABIST,
	CKGEN
};

enum ABIST_CLK {
	CLKPH_MCK_O			= 24,
	ARMPLL_OCC_MON			= 46,
};

enum CKGEN_CLK {
	HF_FAXI_CK			= 1,
	HF_FMM_CK			= 5,
	HF_FMFG_CK			= 9,
};

static void set_fmeter_divider_ca7(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 15, 8, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

static void set_fmeter_divider_ca15(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_2);

	v = ALT_BITS(v, 7, 0, k1);
	clk_writel(CLK_MISC_CFG_2, v);
}

static void set_fmeter_divider(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 7, 0, k1);
	v = ALT_BITS(v, 31, 24, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

static bool wait_fmeter_done(u32 tri_bit)
{
	static int max_wait_count;
	int wait_count = (max_wait_count > 0) ? (max_wait_count * 2 + 2) : 100;
	int i;

	/* wait fmeter */
	for (i = 0; i < wait_count && (clk_readl(CLK26CALI_0) & tri_bit); i++)
		udelay(20);

	if (!(clk_readl(CLK26CALI_0) & tri_bit)) {
		max_wait_count = max(max_wait_count, i);
		return true;
	}

	return false;
}

static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 cksw_ckgen[] = {21, 16, clk};
	u32 cksw_abist[] = {14, 8, clk};
	void __iomem *clk_cfg_reg =
				(type == CKGEN) ? CLK_CFG_9	: CLK_CFG_8;
	void __iomem *cnt_reg =	(type == CKGEN) ? CLK26CALI_2	: CLK26CALI_1;
	u32 *cksw_hlv =		(type == CKGEN) ? cksw_ckgen	: cksw_abist;
	u32 tri_bit =		(type == CKGEN) ? BIT(4)	: BIT(0);
	u32 clk_exc =		(type == CKGEN) ? BIT(5)	: BIT(2);

	u32 clk_misc_cfg_1;
	u32 clk_misc_cfg_2;
	u32 clk_cfg_val;
	u32 freq = 0;

	/* setup fmeter */
	clk_setl(CLK26CALI_0, BIT(7));			/* enable fmeter_en */
	clk_clrl(CLK26CALI_0, clk_exc);			/* set clk_exc */

	/* load_cnt = 1023 */
	clk_writel_mask(cnt_reg, 25, 16, 1023);

	clk_misc_cfg_1 = clk_readl(CLK_MISC_CFG_1);	/* backup reg value */
	clk_misc_cfg_2 = clk_readl(CLK_MISC_CFG_2);
	clk_cfg_val = clk_readl(clk_cfg_reg);

	set_fmeter_divider(k1);				/* set div (0 = /1) */
	set_fmeter_divider_ca7(k1);
	set_fmeter_divider_ca15(k1);
	clk_writel_mask(clk_cfg_reg, cksw_hlv[0], cksw_hlv[1], cksw_hlv[2]);

	clk_setl(CLK26CALI_0, tri_bit);			/* start fmeter */

	if (wait_fmeter_done(tri_bit)) {
		u32 cnt = clk_readl(cnt_reg) & 0xFFFF;

		/* freq = counter * 26M / 1024 (KHz) */
		freq = (cnt * 26000) * (k1 + 1) / 1024;
	}

	/* restore register settings */
	clk_writel(clk_cfg_reg, clk_cfg_val);
	clk_writel(CLK_MISC_CFG_2, clk_misc_cfg_2);
	clk_writel(CLK_MISC_CFG_1, clk_misc_cfg_1);

	return freq;
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1,
		int clk)
{
	u32 last_freq = 0;
	u32 freq;
	u32 maxfreq;

	u32 (*fmeter)(enum FMETER_TYPE, int, int);

	fmeter = fmeter_freq;
	freq = fmeter(type, k1, clk);
	maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static u32 measure_abist_freq(enum ABIST_CLK clk)
{
	return measure_stable_fmeter_freq(ABIST, 0, clk);
}

static u32 measure_ckgen_freq(enum CKGEN_CLK clk)
{
	return measure_stable_fmeter_freq(CKGEN, 0, clk);
}

static u32 measure_armpll_freq(u32 jit_ctrl)
{
	u32 freq;
	u32 mcu26c = clk_readl(MCU_26C);
	u32 armpll_jit_ctrl = clk_readl(ARMPLL_JIT_CTRL);
	u32 top_dcmctl = clk_readl(INFRA_TOPCKGEN_DCMCTL);

	clk_setl(MCU_26C, 0x8);
	clk_setl(ARMPLL_JIT_CTRL, jit_ctrl);
	clk_clrl(INFRA_TOPCKGEN_DCMCTL, 0x700);

	freq = measure_stable_fmeter_freq(ABIST, 1, ARMPLL_OCC_MON);

	clk_writel(INFRA_TOPCKGEN_DCMCTL, top_dcmctl);
	clk_writel(ARMPLL_JIT_CTRL, armpll_jit_ctrl);
	clk_writel(MCU_26C, mcu26c);

	return freq;
}

static u32 measure_ca53_freq(void)
{
	return measure_armpll_freq(0x01);
}

static u32 measure_ca57_freq(void)
{
	return measure_armpll_freq(0x11);
}

unsigned int mt_get_emi_freq(void)
{
	return measure_abist_freq(CLKPH_MCK_O);
}
EXPORT_SYMBOL(mt_get_emi_freq);

unsigned int mt_get_bus_freq(void)
{
	return measure_ckgen_freq(HF_FAXI_CK);
}
EXPORT_SYMBOL(mt_get_bus_freq);

unsigned int mt_get_smallcpu_freq(void)
{
	return measure_ca53_freq();
}
EXPORT_SYMBOL(mt_get_smallcpu_freq);

unsigned int mt_get_bigcpu_freq(void)
{
	return measure_ca57_freq();
}
EXPORT_SYMBOL(mt_get_bigcpu_freq);

unsigned int mt_get_mmclk_freq(void)
{
	return measure_ckgen_freq(HF_FMM_CK);
}
EXPORT_SYMBOL(mt_get_mmclk_freq);

unsigned int mt_get_mfgclk_freq(void)
{
	return measure_ckgen_freq(HF_FMFG_CK);
}
EXPORT_SYMBOL(mt_get_mfgclk_freq);

static int __init get_base_from_node(const char *cmp, void __iomem **pbase)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node) {
		clk_err("node '%s' not found!\n", cmp);
		return -1;
	}

	*pbase = of_iomap(node, 0);
	clk_info("%s base: 0x%p\n", cmp, *pbase);

	return 0;
}

static void __init init_iomap(void)
{
	get_base_from_node("mediatek,mt8173-topckgen", &topckgen_base);
	get_base_from_node("mediatek,mt8173-infrasys", &infrasys_base);
	get_base_from_node("mediatek,MCUCFG", &mcucfg_base);
}
#endif

static int __init mt_power_management_init(void)
{
	pm_power_off = mt_power_off;
	return 0;
}
arch_initcall(mt_power_management_init);

static int __init mt_pm_module_init(void)
{
	/* recover clock related code later if MET needs it */
	/* init_iomap(); */
	return 0;
}
module_init(mt_pm_module_init);

static int __init mt_pm_late_init(void)
{
	return 0;
}
late_initcall(mt_pm_late_init);


MODULE_DESCRIPTION("MTK Power Management Init Driver");
MODULE_LICENSE("GPL");
