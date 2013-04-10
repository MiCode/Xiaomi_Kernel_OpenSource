/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/iopoll.h>
#include <linux/clk.h>

#include <asm/processor.h>
#include <mach/msm_iomap.h>
#include <mach/clk-provider.h>

#include "clock-mdss-8226.h"

#define REG_R(addr)		readl_relaxed(addr)
#define REG_W(data, addr)	writel_relaxed(data, addr)

#define GDSC_PHYS		0xFD8C2304
#define GDSC_SIZE		0x4

#define DSI_PHY_PHYS		0xFD922800
#define DSI_PHY_SIZE		0x00000800

static unsigned char *mdss_dsi_base;
static unsigned char *gdsc_base;
static int pll_byte_clk_rate;
static int pll_pclk_rate;
static int pll_initialized;
static struct clk *mdss_dsi_ahb_clk;
static unsigned long dsi_pll_rate;

void __init mdss_clk_ctrl_pre_init(struct clk *ahb_clk)
{
	BUG_ON(ahb_clk == NULL);

	gdsc_base = ioremap(GDSC_PHYS, GDSC_SIZE);
	if (!gdsc_base)
		pr_err("%s: unable to remap gdsc base", __func__);

	mdss_dsi_base = ioremap(DSI_PHY_PHYS, DSI_PHY_SIZE);
	if (!mdss_dsi_base)
		pr_err("%s: unable to remap dsi base", __func__);

	mdss_dsi_ahb_clk = ahb_clk;
}

#define PLL_POLL_MAX_READS 10
#define PLL_POLL_TIMEOUT_US 50

static int mdss_gdsc_enabled(void)
{
	if (!gdsc_base)
		return 0;

	return !!(readl_relaxed(gdsc_base) & BIT(31));
}

static int mdss_dsi_check_pll_lock(void)
{
	u32 status;

	/* poll for PLL ready status */
	if (readl_poll_timeout_noirq((mdss_dsi_base + 0x02c0),
				status,
				((status & BIT(0)) == 1),
				PLL_POLL_MAX_READS, PLL_POLL_TIMEOUT_US)) {
		pr_err("%s: DSI PLL status=%x failed to Lock\n",
				__func__, status);
		pll_initialized = 0;
	} else {
		pll_initialized = 1;
	}

	return pll_initialized;
}

static long mdss_dsi_pll_byte_round_rate(struct clk *c, unsigned long rate)
{
	if (pll_initialized) {
		return pll_byte_clk_rate;
	} else {
		pr_err("%s: DSI PLL not configured\n", __func__);
		return -EINVAL;
	}
}

static long mdss_dsi_pll_pixel_round_rate(struct clk *c, unsigned long rate)
{
	if (pll_initialized) {
		return pll_pclk_rate;
	} else {
		pr_err("%s: Configure Byte clk first\n", __func__);
		return -EINVAL;
	}
}

static int mdss_dsi_pll_pixel_set_rate(struct clk *c, unsigned long rate)
{
	if (pll_initialized) {
		pll_pclk_rate = rate;
		pr_debug("%s: pll_pclk_rate=%d\n", __func__, pll_pclk_rate);
		return 0;
	} else {
		pr_err("%s: Configure Byte clk first\n", __func__);
		return -EINVAL;
	}
}

static int __mdss_dsi_pll_byte_set_rate(struct clk *c, unsigned long rate)
{
	pr_debug("%s: rate=%ld\n", __func__, rate);

	if (pll_initialized)
		return 0;

	REG_W(0x70, mdss_dsi_base + 0x0230); /* LPFC1 CFG */
	REG_W(0x08, mdss_dsi_base + 0x022c); /* LPFR CFG */
	REG_W(0x02, mdss_dsi_base + 0x0210); /* VREG CFG */
	REG_W(0x00, mdss_dsi_base + 0x0204); /* postDiv1 */
	REG_W(0x01, mdss_dsi_base + 0x0200); /* REFCLK CFG */
	REG_W(0x03, mdss_dsi_base + 0x0224); /* postDiv2 */
	REG_W(0x00, mdss_dsi_base + 0x0238); /* SDM CFG0 */
	REG_W(0x0b, mdss_dsi_base + 0x023c); /* SDM CFG1 */
	REG_W(0x00, mdss_dsi_base + 0x0240); /* SDM CFG2 */
	REG_W(0x6c, mdss_dsi_base + 0x0244); /* SDM CFG3 */
	REG_W(0x02, mdss_dsi_base + 0x0208); /* ChgPump */
	REG_W(0x31, mdss_dsi_base + 0x020c); /* VCOLPF CFG */
	REG_W(0x15, mdss_dsi_base + 0x0234); /* LPFC2 CFG */

	REG_W(0x30, mdss_dsi_base + 0x0284); /* CAL CFG6 */
	REG_W(0x00, mdss_dsi_base + 0x0288); /* CAL CFG7 */
	REG_W(0x60, mdss_dsi_base + 0x028c); /* CAL CFG8 */
	REG_W(0x00, mdss_dsi_base + 0x0290); /* CAL CFG9 */
	REG_W(0xdd, mdss_dsi_base + 0x0294); /* CAL CFG10 */
	REG_W(0x01, mdss_dsi_base + 0x0298); /* CAL CFG11 */

	REG_W(0x05, mdss_dsi_base + 0x0228); /* postDiv3 */
	REG_W(0x2b, mdss_dsi_base + 0x0278); /* Cal CFG3 */
	REG_W(0x66, mdss_dsi_base + 0x027c); /* Cal CFG4 */
	REG_W(0x05, mdss_dsi_base + 0x0264); /* LKDET CFG2 */
	REG_W(0x00, mdss_dsi_base + 0x0248); /* SDM CFG4 */
	REG_W(0x00, mdss_dsi_base + 0x0214); /* PWRGEN CFG */
	REG_W(0x0a, mdss_dsi_base + 0x026c); /* CAL CFG0 */
	REG_W(0x20, mdss_dsi_base + 0x029c); /* EFUSE CFG */

	dsi_pll_rate = rate;
	pll_byte_clk_rate = rate;

	pr_debug("%s: PLL initialized. bcl=%d\n", __func__, pll_byte_clk_rate);
	pll_initialized = 1;

	return 0;
}

static int mdss_dsi_pll_byte_set_rate(struct clk *c, unsigned long rate)
{
	int ret;

	clk_prepare_enable(mdss_dsi_ahb_clk);
	ret = __mdss_dsi_pll_byte_set_rate(c, rate);
	clk_disable_unprepare(mdss_dsi_ahb_clk);

	return ret;
}

static void mdss_dsi_uniphy_pll_sw_reset(void)
{
	/*
	 * Add hardware recommended delays after toggling the
	 * software reset bit off and back on.
	 */
	REG_W(0x01, mdss_dsi_base + 0x0268); /* PLL TEST CFG */
	udelay(300);
	REG_W(0x00, mdss_dsi_base + 0x0268); /* PLL TEST CFG */
	udelay(300);
}

static void mdss_dsi_pll_enable_casem(void)
{
	int i;

	/*
	 * Add hardware recommended delays between register writes for
	 * the updates to take effect. These delays are necessary for the
	 * PLL to successfully lock.
	 */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);

	for (i = 0; (i < 3) && !mdss_dsi_check_pll_lock(); i++) {
		REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1);

		REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1000);
	}

	if (pll_initialized)
		pr_debug("%s: PLL Locked after %d attempts\n", __func__, i);
	else
		pr_debug("%s: PLL failed to lock\n", __func__);
}

static void mdss_dsi_pll_enable_casef1(void)
{
	/*
	 * Add hardware recommended delays between register writes for
	 * the updates to take effect. These delays are necessary for the
	 * PLL to successfully lock.
	 */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x0d, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);

	if (mdss_dsi_check_pll_lock())
		pr_debug("%s: PLL Locked\n", __func__);
	else
		pr_debug("%s: PLL failed to lock\n", __func__);
}

static void mdss_dsi_pll_enable_cased(void)
{
	/*
	 * Add hardware recommended delays between register writes for
	 * the updates to take effect. These delays are necessary for the
	 * PLL to successfully lock.
	 */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1);
	REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1);
	REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1);

	if (mdss_dsi_check_pll_lock())
		pr_debug("%s: PLL Locked\n", __func__);
	else
		pr_debug("%s: PLL failed to lock\n", __func__);
}

static void mdss_dsi_pll_enable_casec(void)
{
	/*
	 * Add hardware recommended delays between register writes for
	 * the updates to take effect. These delays are necessary for the
	 * PLL to successfully lock.
	 */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);

	if (mdss_dsi_check_pll_lock())
		pr_debug("%s: PLL Locked\n", __func__);
	else
		pr_debug("%s: PLL failed to lock\n", __func__);
}

static void mdss_dsi_pll_enable_casee(void)
{
	/*
	 * Add hardware recommended delays between register writes for
	 * the updates to take effect. These delays are necessary for the
	 * PLL to successfully lock.
	 */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(200);
	REG_W(0x0d, mdss_dsi_base + 0x0220); /* GLB CFG */
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);

	if (mdss_dsi_check_pll_lock())
		pr_debug("%s: PLL Locked\n", __func__);
	else
		pr_debug("%s: PLL failed to lock\n", __func__);
}

static int __mdss_dsi_pll_enable(struct clk *c)
{
	if (!pll_initialized) {
		if (dsi_pll_rate)
			__mdss_dsi_pll_byte_set_rate(c, dsi_pll_rate);
		else
			pr_err("%s: Calling clk_en before set_rate\n",
				__func__);
	}

	/*
	 * Try all PLL power-up sequences one-by-one until
	 * PLL lock is detected
	 */
	mdss_dsi_uniphy_pll_sw_reset();
	mdss_dsi_pll_enable_casem();
	if (pll_initialized)
		goto pll_locked;

	mdss_dsi_uniphy_pll_sw_reset();
	mdss_dsi_pll_enable_cased();
	if (pll_initialized)
		goto pll_locked;

	mdss_dsi_uniphy_pll_sw_reset();
	mdss_dsi_pll_enable_cased();
	if (pll_initialized)
		goto pll_locked;

	mdss_dsi_uniphy_pll_sw_reset();
	mdss_dsi_pll_enable_casef1();
	if (pll_initialized)
		goto pll_locked;

	mdss_dsi_uniphy_pll_sw_reset();
	mdss_dsi_pll_enable_casec();
	if (pll_initialized)
		goto pll_locked;

	mdss_dsi_uniphy_pll_sw_reset();
	mdss_dsi_pll_enable_casee();
	if (pll_initialized)
		goto pll_locked;

	pr_err("%s: DSI PLL failed to Lock\n", __func__);
	return -EINVAL;

pll_locked:
	pr_debug("%s: PLL Lock success\n", __func__);

	return 0;
}

static void __mdss_dsi_pll_disable(void)
{
	writel_relaxed(0x00, mdss_dsi_base + 0x0220); /* GLB CFG */
	pr_debug("%s: PLL disabled\n", __func__);
	pll_initialized = 0;
}

static DEFINE_SPINLOCK(dsipll_lock);
static int dsipll_refcount;

static void mdss_dsi_pll_disable(struct clk *c)
{
	unsigned long flags;

	spin_lock_irqsave(&dsipll_lock, flags);
	if (WARN(dsipll_refcount == 0, "DSI PLL clock is unbalanced"))
		goto out;
	if (dsipll_refcount == 1)
		__mdss_dsi_pll_disable();
	dsipll_refcount--;
out:
	spin_unlock_irqrestore(&dsipll_lock, flags);
}

static int mdss_dsi_pll_enable(struct clk *c)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dsipll_lock, flags);
	if (dsipll_refcount == 0) {
		ret = __mdss_dsi_pll_enable(c);
		if (ret < 0)
			goto out;
	}
	dsipll_refcount++;
out:
	spin_unlock_irqrestore(&dsipll_lock, flags);
	return ret;
}

/* todo: Adjust these values appropriately */
static enum handoff mdss_dsi_pll_byte_handoff(struct clk *c)
{
	if (mdss_gdsc_enabled()) {
		clk_prepare_enable(mdss_dsi_ahb_clk);
		if (mdss_dsi_check_pll_lock()) {
			c->rate = 59000000;
			dsi_pll_rate = 59000000;
			pll_byte_clk_rate = 59000000;
			pll_pclk_rate = 117000000;
			dsipll_refcount++;
			return HANDOFF_ENABLED_CLK;
		}
		clk_disable_unprepare(mdss_dsi_ahb_clk);
	}

	return HANDOFF_DISABLED_CLK;
}

/* todo: Adjust these values appropriately */
static enum handoff mdss_dsi_pll_pixel_handoff(struct clk *c)
{
	if (mdss_gdsc_enabled()) {
		clk_prepare_enable(mdss_dsi_ahb_clk);
		if (mdss_dsi_check_pll_lock()) {
			c->rate = 117000000;
			dsipll_refcount++;
			return HANDOFF_ENABLED_CLK;
		}
		clk_disable_unprepare(mdss_dsi_ahb_clk);
	}

	return HANDOFF_DISABLED_CLK;
}

struct clk_ops clk_ops_dsi_pixel_pll = {
	.enable = mdss_dsi_pll_enable,
	.disable = mdss_dsi_pll_disable,
	.set_rate = mdss_dsi_pll_pixel_set_rate,
	.round_rate = mdss_dsi_pll_pixel_round_rate,
	.handoff = mdss_dsi_pll_pixel_handoff,
};

struct clk_ops clk_ops_dsi_byte_pll = {
	.enable = mdss_dsi_pll_enable,
	.disable = mdss_dsi_pll_disable,
	.set_rate = mdss_dsi_pll_byte_set_rate,
	.round_rate = mdss_dsi_pll_byte_round_rate,
	.handoff = mdss_dsi_pll_byte_handoff,
};
