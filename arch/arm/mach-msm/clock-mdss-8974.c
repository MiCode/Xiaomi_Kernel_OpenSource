/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "clock.h"
#include "clock-mdss-8974.h"

#define REG_R(addr)		readl_relaxed(addr)
#define REG_W(data, addr)	writel_relaxed(data, addr)

#define DSI_PHY_PHYS		0xFD922800
#define DSI_PHY_SIZE		0x00000800

#define HDMI_PHY_PHYS		0xFD922500
#define HDMI_PHY_SIZE		0x0000007C

#define HDMI_PHY_PLL_PHYS	0xFD922700
#define HDMI_PHY_PLL_SIZE	0x000000D4

/* hdmi phy registers */
#define HDMI_PHY_PD_CTRL0		(0x0010)
#define HDMI_PHY_GLB_CFG		(0x0018)
#define HDMI_PHY_STATUS			(0x005C)

/* hdmi phy unified pll registers */
#define	 HDMI_UNI_PLL_REFCLK_CF		(0x0000)
#define	 HDMI_UNI_PLL_POSTDIV1_CFG	(0x0004)
#define	 HDMI_UNI_PLL_VCOLPF_CFG	(0x000C)
#define	 HDMI_UNI_PLL_GLB_CFG		(0x0020)
#define	 HDMI_UNI_PLL_POSTDIV2_CFG	(0x0024)
#define	 HDMI_UNI_PLL_POSTDIV3_CFG	(0x0028)
#define	 HDMI_UNI_PLL_SDM_CFG0		(0x0038)
#define	 HDMI_UNI_PLL_SDM_CFG1		(0x003C)
#define	 HDMI_UNI_PLL_SDM_CFG2		(0x0040)
#define	 HDMI_UNI_PLL_SDM_CFG3		(0x0044)
#define	 HDMI_UNI_PLL_SDM_CFG4		(0x0048)
#define	 HDMI_UNI_PLL_LKDET_CFG0	(0x005C)
#define	 HDMI_UNI_PLL_LKDET_CFG1	(0x0060)
#define	 HDMI_UNI_PLL_LKDET_CFG2	(0x0064)
#define	 HDMI_UNI_PLL_CAL_CFG8		(0x008C)
#define	 HDMI_UNI_PLL_CAL_CFG9		(0x0090)
#define	 HDMI_UNI_PLL_CAL_CFG10		(0x0094)
#define	 HDMI_UNI_PLL_CAL_CFG11		(0x0098)
#define  HDMI_UNI_PLL_STATUS		(0x00C0)

#define VCO_CLK				424000000
static unsigned char *mdss_dsi_base;
static int pll_byte_clk_rate;
static int pll_pclk_rate;
static int pll_initialized;
static struct clk *mdss_dsi_ahb_clk;

static void __iomem *hdmi_phy_base;
static void __iomem *hdmi_phy_pll_base;
static unsigned hdmi_pll_on;

void __init mdss_clk_ctrl_init(void)
{
	mdss_dsi_base = ioremap(DSI_PHY_PHYS, DSI_PHY_SIZE);
	if (!mdss_dsi_base)
		pr_err("%s: unable to remap dsi base", __func__);

	mdss_dsi_ahb_clk = clk_get_sys("mdss_dsi_clk_ctrl", "iface_clk");
	if (!IS_ERR(mdss_dsi_ahb_clk)) {
		clk_prepare(mdss_dsi_ahb_clk);
	} else {
		mdss_dsi_ahb_clk = NULL;
		pr_err("%s:%d unable to get dsi iface clock\n",
			       __func__, __LINE__);
	}

	hdmi_phy_base = ioremap(HDMI_PHY_PHYS, HDMI_PHY_SIZE);
	if (!hdmi_phy_base)
		pr_err("%s: unable to ioremap hdmi phy base", __func__);

	hdmi_phy_pll_base = ioremap(HDMI_PHY_PLL_PHYS, HDMI_PHY_PLL_SIZE);
	if (!hdmi_phy_pll_base)
		pr_err("%s: unable to ioremap hdmi phy pll base", __func__);
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
	if (pll_initialized)
		return 0;
	else {
		pr_err("%s: Configure Byte clk first\n",
				__func__);
		return -EINVAL;
	}
}

static int mdss_dsi_pll_byte_set_rate(struct clk *c, unsigned long rate)
{
	int pll_divcfg1, pll_divcfg2;
	int half_bitclk_rate;

	if (pll_initialized)
		return 0;

	if (!mdss_dsi_ahb_clk) {
		pr_err("%s: mdss_dsi_ahb_clk not initialized\n",
				__func__);
		return -EINVAL;
	}

	clk_enable(mdss_dsi_ahb_clk);

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
	REG_W(0x03, mdss_dsi_base + 0x0228); /* postDiv3 */

	REG_W(0x2b, mdss_dsi_base + 0x0278); /* Cal CFG3 */
	REG_W(0x06, mdss_dsi_base + 0x027c); /* Cal CFG4 */
	REG_W(0x05, mdss_dsi_base + 0x0264); /* Cal CFG4 */

	REG_W(0x0a, mdss_dsi_base + 0x023c); /* SDM CFG1 */
	REG_W(0xab, mdss_dsi_base + 0x0240); /* SDM CFG2 */
	REG_W(0x0a, mdss_dsi_base + 0x0244); /* SDM CFG3 */
	REG_W(0x00, mdss_dsi_base + 0x0248); /* SDM CFG4 */

	udelay(10);

	REG_W(0x01, mdss_dsi_base + 0x0200); /* REFCLK CFG */
	REG_W(0x00, mdss_dsi_base + 0x0214); /* PWRGEN CFG */
	REG_W(0x01, mdss_dsi_base + 0x020c); /* VCOLPF CFG */
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

	pll_byte_clk_rate = 53000000;
	pll_pclk_rate = 105000000;

	clk_disable(mdss_dsi_ahb_clk);
	pll_initialized = 1;

	return 0;
}

static int mdss_dsi_pll_enable(struct clk *c)
{
	u32 status;
	u32 max_reads, timeout_us;
	static int pll_enabled;

	if (pll_enabled)
		return 0;

	if (!mdss_dsi_ahb_clk) {
		pr_err("%s: mdss_dsi_ahb_clk not initialized\n",
				__func__);
		return -EINVAL;
	}

	clk_enable(mdss_dsi_ahb_clk);
	/* PLL power up */
	REG_W(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	REG_W(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(20);
	REG_W(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(20);
	REG_W(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */

	/* poll for PLL ready status */
	max_reads = 20;
	timeout_us = 100;
	if (readl_poll_timeout_noirq((mdss_dsi_base + 0x02c0),
			   status,
			   ((status & 0x01) == 1),
				     max_reads, timeout_us)) {
		pr_err("%s: DSI PLL status=%x failed to Lock\n",
		       __func__, status);
		clk_disable(mdss_dsi_ahb_clk);
		return -EINVAL;
	}
	clk_disable(mdss_dsi_ahb_clk);
	pll_enabled = 1;

	return 0;
}

static void mdss_dsi_pll_disable(struct clk *c)
{
	if (!mdss_dsi_ahb_clk)
		pr_err("%s: mdss_dsi_ahb_clk not initialized\n",
				__func__);

	clk_enable(mdss_dsi_ahb_clk);
	writel_relaxed(0x00, mdss_dsi_base + 0x0220); /* GLB CFG */
	clk_disable(mdss_dsi_ahb_clk);
}

void hdmi_pll_disable(void)
{
	REG_W(0x0, hdmi_phy_pll_base + HDMI_UNI_PLL_GLB_CFG);
	udelay(5);
	REG_W(0x0, hdmi_phy_base + HDMI_PHY_GLB_CFG);

	hdmi_pll_on = 0;
} /* hdmi_pll_disable */

int hdmi_pll_enable(void)
{
	u32 status;
	u32 max_reads, timeout_us;

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
		return -EINVAL;
	}
	pr_debug("%s: hdmi phy is locked\n", __func__);

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

	pr_debug("%s: rate=%ld\n", __func__, rate);
	switch (rate) {
	case 0:
		/* This case is needed for suspend/resume. */
	break;

	case 25200000:
		/* 640x480p60 */
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CF);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x36, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x4C, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xFC, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
	break;

	case 27030000:
		/* 480p60/480i60 case */
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CF);
		REG_W(0x18, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x36, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x14, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x63, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x1D, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0x2A, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x03, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
	break;

	case 74250000:
		/*
		 * 720p60/720p50/1080i60/1080i50
		 * 1080p24/1080p30/1080p25 case
		 */
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CF);
		REG_W(0x20, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x36, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x52, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0xFD, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x55, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0x73, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
	break;

	case 148500000:
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CF);
		REG_W(0x18, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x36, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x52, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0xFD, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0x55, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xE6, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x02, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
	break;

	case 297000000:
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_REFCLK_CF);
		REG_W(0x18, hdmi_phy_pll_base + HDMI_UNI_PLL_VCOLPF_CFG);
		REG_W(0x36, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG0);
		REG_W(0x65, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG1);
		REG_W(0x01, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG2);
		REG_W(0xAC, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG3);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_SDM_CFG4);
		REG_W(0x10, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG0);
		REG_W(0x1A, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG1);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_LKDET_CFG2);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV1_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV2_CFG);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_POSTDIV3_CFG);
		REG_W(0x60, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG8);
		REG_W(0x00, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG9);
		REG_W(0xCD, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG10);
		REG_W(0x05, hdmi_phy_pll_base + HDMI_UNI_PLL_CAL_CFG11);
	break;

	case 27000000:
		/* 576p50/576i50 case */
	default:
		pr_err("%s: not supported rate=%ld\n", __func__, rate);
	}

	/* Make sure writes complete before disabling iface clock */
	mb();

	if (set_power_dwn)
		hdmi_pll_enable();

	return 0;
} /* hdmi_pll_set_rate */

struct clk_ops clk_ops_dsi_pixel_pll = {
	.enable = mdss_dsi_pll_enable,
	.disable = mdss_dsi_pll_disable,
	.set_rate = mdss_dsi_pll_pixel_set_rate,
	.round_rate = mdss_dsi_pll_pixel_round_rate,
};

struct clk_ops clk_ops_dsi_byte_pll = {
	.enable = mdss_dsi_pll_enable,
	.disable = mdss_dsi_pll_disable,
	.set_rate = mdss_dsi_pll_byte_set_rate,
	.round_rate = mdss_dsi_pll_byte_round_rate,
};
