/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include "clock-mdss-8974.h"

#define REG_R(addr)		readl_relaxed(addr)
#define REG_W(data, addr)	writel_relaxed(data, addr)

#define GDSC_PHYS		0xFD8C2304
#define GDSC_SIZE		0x4

#define DSI_PHY_PHYS		0xFD922800
#define DSI_PHY_SIZE		0x00000800

#define HDMI_PHY_PHYS		0xFD922500
#define HDMI_PHY_SIZE		0x0000007C

#define HDMI_PHY_PLL_PHYS	0xFD922700
#define HDMI_PHY_PLL_SIZE	0x000000D4

/* hdmi phy registers */
#define HDMI_PHY_ANA_CFG0               (0x0000)
#define HDMI_PHY_ANA_CFG1               (0x0004)
#define HDMI_PHY_ANA_CFG2               (0x0008)
#define HDMI_PHY_ANA_CFG3               (0x000C)
#define HDMI_PHY_PD_CTRL0               (0x0010)
#define HDMI_PHY_PD_CTRL1               (0x0014)
#define HDMI_PHY_GLB_CFG                (0x0018)
#define HDMI_PHY_DCC_CFG0               (0x001C)
#define HDMI_PHY_DCC_CFG1               (0x0020)
#define HDMI_PHY_TXCAL_CFG0             (0x0024)
#define HDMI_PHY_TXCAL_CFG1             (0x0028)
#define HDMI_PHY_TXCAL_CFG2             (0x002C)
#define HDMI_PHY_TXCAL_CFG3             (0x0030)
#define HDMI_PHY_BIST_CFG0              (0x0034)
#define HDMI_PHY_BIST_CFG1              (0x0038)
#define HDMI_PHY_BIST_PATN0             (0x003C)
#define HDMI_PHY_BIST_PATN1             (0x0040)
#define HDMI_PHY_BIST_PATN2             (0x0044)
#define HDMI_PHY_BIST_PATN3             (0x0048)
#define HDMI_PHY_STATUS                 (0x005C)

/* hdmi phy unified pll registers */
#define HDMI_UNI_PLL_REFCLK_CFG         (0x0000)
#define HDMI_UNI_PLL_POSTDIV1_CFG       (0x0004)
#define HDMI_UNI_PLL_CHFPUMP_CFG        (0x0008)
#define HDMI_UNI_PLL_VCOLPF_CFG         (0x000C)
#define HDMI_UNI_PLL_VREG_CFG           (0x0010)
#define HDMI_UNI_PLL_PWRGEN_CFG         (0x0014)
#define HDMI_UNI_PLL_GLB_CFG            (0x0020)
#define HDMI_UNI_PLL_POSTDIV2_CFG       (0x0024)
#define HDMI_UNI_PLL_POSTDIV3_CFG       (0x0028)
#define HDMI_UNI_PLL_LPFR_CFG           (0x002C)
#define HDMI_UNI_PLL_LPFC1_CFG          (0x0030)
#define HDMI_UNI_PLL_LPFC2_CFG          (0x0034)
#define HDMI_UNI_PLL_SDM_CFG0           (0x0038)
#define HDMI_UNI_PLL_SDM_CFG1           (0x003C)
#define HDMI_UNI_PLL_SDM_CFG2           (0x0040)
#define HDMI_UNI_PLL_SDM_CFG3           (0x0044)
#define HDMI_UNI_PLL_SDM_CFG4           (0x0048)
#define HDMI_UNI_PLL_SSC_CFG0           (0x004C)
#define HDMI_UNI_PLL_SSC_CFG1           (0x0050)
#define HDMI_UNI_PLL_SSC_CFG2           (0x0054)
#define HDMI_UNI_PLL_SSC_CFG3           (0x0058)
#define HDMI_UNI_PLL_LKDET_CFG0         (0x005C)
#define HDMI_UNI_PLL_LKDET_CFG1         (0x0060)
#define HDMI_UNI_PLL_LKDET_CFG2         (0x0064)
#define HDMI_UNI_PLL_CAL_CFG0           (0x006C)
#define HDMI_UNI_PLL_CAL_CFG1           (0x0070)
#define HDMI_UNI_PLL_CAL_CFG2           (0x0074)
#define HDMI_UNI_PLL_CAL_CFG3           (0x0078)
#define HDMI_UNI_PLL_CAL_CFG4           (0x007C)
#define HDMI_UNI_PLL_CAL_CFG5           (0x0080)
#define HDMI_UNI_PLL_CAL_CFG6           (0x0084)
#define HDMI_UNI_PLL_CAL_CFG7           (0x0088)
#define HDMI_UNI_PLL_CAL_CFG8           (0x008C)
#define HDMI_UNI_PLL_CAL_CFG9           (0x0090)
#define HDMI_UNI_PLL_CAL_CFG10          (0x0094)
#define HDMI_UNI_PLL_CAL_CFG11          (0x0098)
#define HDMI_UNI_PLL_STATUS             (0x00C0)

#define VCO_CLK				424000000
static unsigned char *mdss_dsi_base;
static unsigned char *gdsc_base;
static int pll_byte_clk_rate;
static int pll_pclk_rate;
static int pll_initialized;
static struct clk *mdss_dsi_ahb_clk;
static unsigned long dsi_pll_rate;

static void __iomem *hdmi_phy_base;
static void __iomem *hdmi_phy_pll_base;
static unsigned hdmi_pll_on;

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

	hdmi_phy_base = ioremap(HDMI_PHY_PHYS, HDMI_PHY_SIZE);
	if (!hdmi_phy_base)
		pr_err("%s: unable to ioremap hdmi phy base", __func__);

	hdmi_phy_pll_base = ioremap(HDMI_PHY_PLL_PHYS, HDMI_PHY_PLL_SIZE);
	if (!hdmi_phy_pll_base)
		pr_err("%s: unable to ioremap hdmi phy pll base", __func__);
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

	clk_prepare_enable(mdss_dsi_ahb_clk);
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
	clk_disable_unprepare(mdss_dsi_ahb_clk);

	return pll_initialized;
}

static long mdss_dsi_pll_byte_round_rate(struct clk *c, unsigned long rate)
{
	if (pll_initialized)
		return pll_byte_clk_rate;
	else {
		pr_err("%s: DSI PLL not configured\n",
				__func__);
		return -EINVAL;
	}
}

static long mdss_dsi_pll_pixel_round_rate(struct clk *c, unsigned long rate)
{
	if (pll_initialized)
		return pll_pclk_rate;
	else {
		pr_err("%s: Configure Byte clk first\n",
				__func__);
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
	int pll_divcfg1, pll_divcfg2;
	int half_bitclk_rate;

	pr_debug("%s:\n", __func__);
	if (pll_initialized)
		return 0;

	half_bitclk_rate = rate * 4;

	pll_divcfg1 = (VCO_CLK / half_bitclk_rate) - 2;

	/* Configuring the VCO to 424 Mhz */
	/* Configuring the half rate Bit clk to 212 Mhz */

	pll_divcfg2 = 3; /* ByteClk is 1/4 the half-bitClk rate */

	/* Configure the Loop filter */
	/* Loop filter resistance value */
	REG_W(0x08, mdss_dsi_base + 0x022c);
	/* Loop filter capacitance values : c1 and c2 */
	REG_W(0x70, mdss_dsi_base + 0x0230);
	REG_W(0x15, mdss_dsi_base + 0x0234);

	REG_W(0x02, mdss_dsi_base + 0x0208); /* ChgPump */
	REG_W(pll_divcfg1, mdss_dsi_base + 0x0204); /* postDiv1 */
	REG_W(pll_divcfg2, mdss_dsi_base + 0x0224); /* postDiv2 */
	REG_W(0x05, mdss_dsi_base + 0x0228); /* postDiv3 */

	REG_W(0x2b, mdss_dsi_base + 0x0278); /* Cal CFG3 */
	REG_W(0x66, mdss_dsi_base + 0x027c); /* Cal CFG4 */
	REG_W(0x05, mdss_dsi_base + 0x0264); /* LKDET CFG2 */

	REG_W(0x0a, mdss_dsi_base + 0x023c); /* SDM CFG1 */
	REG_W(0xab, mdss_dsi_base + 0x0240); /* SDM CFG2 */
	REG_W(0x0a, mdss_dsi_base + 0x0244); /* SDM CFG3 */
	REG_W(0x00, mdss_dsi_base + 0x0248); /* SDM CFG4 */

	REG_W(0x01, mdss_dsi_base + 0x0200); /* REFCLK CFG */
	REG_W(0x00, mdss_dsi_base + 0x0214); /* PWRGEN CFG */
	REG_W(0x71, mdss_dsi_base + 0x020c); /* VCOLPF CFG */
	REG_W(0x02, mdss_dsi_base + 0x0210); /* VREG CFG */
	REG_W(0x00, mdss_dsi_base + 0x0238); /* SDM CFG0 */

	REG_W(0x5f, mdss_dsi_base + 0x028c); /* CAL CFG8 */
	REG_W(0xa8, mdss_dsi_base + 0x0294); /* CAL CFG10 */
	REG_W(0x01, mdss_dsi_base + 0x0298); /* CAL CFG11 */
	REG_W(0x0a, mdss_dsi_base + 0x026c); /* CAL CFG0 */
	REG_W(0x30, mdss_dsi_base + 0x0284); /* CAL CFG6 */
	REG_W(0x00, mdss_dsi_base + 0x0288); /* CAL CFG7 */
	REG_W(0x00, mdss_dsi_base + 0x0290); /* CAL CFG9 */
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

static void mdss_dsi_uniphy_pll_lock_detect_setting(void)
{
	REG_W(0x04, mdss_dsi_base + 0x0264); /* LKDetect CFG2 */
	udelay(100);
	REG_W(0x05, mdss_dsi_base + 0x0264); /* LKDetect CFG2 */
	udelay(500);
}

static void mdss_dsi_uniphy_pll_sw_reset(void)
{
	REG_W(0x01, mdss_dsi_base + 0x0268); /* PLL TEST CFG */
	udelay(1);
	REG_W(0x00, mdss_dsi_base + 0x0268); /* PLL TEST CFG */
	udelay(1);
}

static int __mdss_dsi_pll_enable(struct clk *c)
{
	u32 status;
	u32 max_reads, timeout_us;
	int i;

	if (!pll_initialized) {
		if (dsi_pll_rate)
			__mdss_dsi_pll_byte_set_rate(c, dsi_pll_rate);
		else
			pr_err("%s: Calling clk_en before set_rate\n",
						__func__);
	}

	mdss_dsi_uniphy_pll_sw_reset();
	/* PLL power up */
	/* Add HW recommended delay between
	   register writes for the update to propagate */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);
	REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(1000);

	for (i = 0; i < 3; i++) {
		mdss_dsi_uniphy_pll_lock_detect_setting();
		/* poll for PLL ready status */
		max_reads = 5;
		timeout_us = 100;
		if (readl_poll_timeout_noirq((mdss_dsi_base + 0x02c0),
				   status,
				   ((status & 0x01) == 1),
					     max_reads, timeout_us)) {
			pr_debug("%s: DSI PLL status=%x failed to Lock\n",
			       __func__, status);
			pr_debug("%s:Trying to power UP PLL again\n",
			       __func__);
		} else
			break;

		mdss_dsi_uniphy_pll_sw_reset();
		udelay(1000);
		/* Add HW recommended delay between
		   register writes for the update to propagate */
		REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1000);
		REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1000);
		REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1000);
		REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1000);
		REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(1000);
		REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */
		udelay(2000);

	}

	if ((status & 0x01) != 1) {
		pr_err("%s: DSI PLL status=%x failed to Lock\n",
		       __func__, status);
		return -EINVAL;
	}

	pr_debug("%s: **** PLL Lock success\n", __func__);

	return 0;
}

static void __mdss_dsi_pll_disable(void)
{
	writel_relaxed(0x00, mdss_dsi_base + 0x0220); /* GLB CFG */
	pr_debug("%s: **** disable pll Initialize\n", __func__);
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

static enum handoff mdss_dsi_pll_byte_handoff(struct clk *c)
{
	if (mdss_gdsc_enabled() && mdss_dsi_check_pll_lock()) {
		c->rate = 52954560;
		dsi_pll_rate = 52954560;
		pll_byte_clk_rate = 52954560;
		pll_pclk_rate = 105000000;
		dsipll_refcount++;
		return HANDOFF_ENABLED_CLK;
	}

	return HANDOFF_DISABLED_CLK;
}

static enum handoff mdss_dsi_pll_pixel_handoff(struct clk *c)
{
	if (mdss_gdsc_enabled() && mdss_dsi_check_pll_lock()) {
		c->rate = 105000000;
		dsipll_refcount++;
		return HANDOFF_ENABLED_CLK;
	}

	return HANDOFF_DISABLED_CLK;
}

void hdmi_pll_disable(void)
{
	clk_enable(mdss_dsi_ahb_clk);
	REG_W(0x0, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
	udelay(5);
	REG_W(0x0, hdmi_phy_base + HDMI_PHY_GLB_CFG);
	clk_disable(mdss_dsi_ahb_clk);

	hdmi_pll_on = 0;
} /* hdmi_pll_disable */

int hdmi_pll_enable(void)
{
	u32 status;
	u32 max_reads, timeout_us;

	clk_enable(mdss_dsi_ahb_clk);
	/* Global Enable */
	REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
	/* Power up power gen */
	REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
	udelay(350);

	/* PLL Power-Up */
	REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
	udelay(5);
	/* Power up PLL LDO */
	REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
	udelay(350);

	/* PLL Power-Up */
	REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
	udelay(350);

	/* poll for PLL ready status */
	max_reads = 20;
	timeout_us = 100;
	if (readl_poll_timeout_noirq((hdmi_phy_pll_base + HDMI_UNI_PLL_STATUS),
		status, ((status & BIT(0)) == 1), max_reads, timeout_us)) {
		pr_err("%s: hdmi phy pll status=%x failed to Lock\n",
		       __func__, status);
		hdmi_pll_disable();
		clk_disable(mdss_dsi_ahb_clk);
		return -EINVAL;
	}
	pr_debug("%s: hdmi phy pll is locked\n", __func__);

	udelay(350);
	/* poll for PHY ready status */
	max_reads = 20;
	timeout_us = 100;
	if (readl_poll_timeout_noirq((hdmi_phy_base + HDMI_PHY_STATUS),
		status, ((status & BIT(0)) == 1), max_reads, timeout_us)) {
		pr_err("%s: hdmi phy status=%x failed to Lock\n",
		       __func__, status);
		hdmi_pll_disable();
		clk_disable(mdss_dsi_ahb_clk);
		return -EINVAL;
	}
	pr_debug("%s: hdmi phy is locked\n", __func__);
	clk_disable(mdss_dsi_ahb_clk);

	hdmi_pll_on = 1;

	return 0;
} /* hdmi_pll_enable */

int hdmi_pll_set_rate(unsigned long rate)
{
	unsigned int set_power_dwn = 0;

	if (hdmi_pll_on) {
		hdmi_pll_disable();
		set_power_dwn = 1;
	}

	clk_enable(mdss_dsi_ahb_clk);
	pr_debug("%s: rate=%ld\n", __func__, rate);
	switch (rate) {
	case 0:
		/* This case is needed for suspend/resume. */
	break;

	case 25200000:
		/* 640x480p60 */
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0x0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x52, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0xB0, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xF4, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x02, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0x1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0xDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0xD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0x1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	case 27000000:
		/* 576p50/576i50 case */
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0X0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0X0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x54, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x18, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0X1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0x2a, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0X1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0X0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0XDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0XD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0X1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	case 27030000:
		/* 480p60/480i60 case */
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0x0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x54, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x66, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x1D, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0x2A, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0x1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0xDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0xD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0x1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	case 74250000:
		/*
		 * 720p60/720p50/1080i60/1080i50
		 * 1080p24/1080p30/1080p25 case
		 */
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0x0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x52, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x56, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xE6, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x02, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0x1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0xDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0xD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0x1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	case 148500000:
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0x0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x52, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x56, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xE6, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x02, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0x1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0xDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0xD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0x1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	case 268500000:
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0x0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x36, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x61, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0xF6, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0x3E, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0x1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0xDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0xD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0x1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x11, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	case 297000000:
		REG_W(0x81, hdmi_phy_base + HDMI_PHY_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CFG);
		REG_W(0x19, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x0E, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFR_CFG);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC1_CFG);
		REG_W(0x0D, hdmi_phy_pll_base + HDMI_UNI_PLL_LPFC2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x65, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0xAC, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG2);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xCD, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
		REG_W(0x1F, hdmi_phy_base + HDMI_PHY_PD_CTRL0);
		udelay(50);

		REG_W(0x0F, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_PD_CTRL1);
		REG_W(0x10, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0xDB, hdmi_phy_base + HDMI_PHY_ANA_CFG0);
		REG_W(0x43, hdmi_phy_base + HDMI_PHY_ANA_CFG1);
		REG_W(0x06, hdmi_phy_base + HDMI_PHY_ANA_CFG2);
		REG_W(0x03, hdmi_phy_base + HDMI_PHY_ANA_CFG3);
		REG_W(0x04, hdmi_phy_pll_base + HDMI_UNI_PLL_VREG_CFG);
		REG_W(0xD0, hdmi_phy_base + HDMI_PHY_DCC_CFG0);
		REG_W(0x1A, hdmi_phy_base + HDMI_PHY_DCC_CFG1);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG0);
		REG_W(0x00, hdmi_phy_base + HDMI_PHY_TXCAL_CFG1);
		REG_W(0x02, hdmi_phy_base + HDMI_PHY_TXCAL_CFG2);
		REG_W(0x05, hdmi_phy_base + HDMI_PHY_TXCAL_CFG3);
		udelay(200);
	break;

	default:
		pr_err("%s: not supported rate=%ld\n", __func__, rate);
	}

	/* Make sure writes complete before disabling iface clock */
	mb();

	clk_disable(mdss_dsi_ahb_clk);

	if (set_power_dwn)
		hdmi_pll_enable();

	return 0;
} /* hdmi_pll_set_rate */

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
