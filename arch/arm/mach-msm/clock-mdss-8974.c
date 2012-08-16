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

#define MDSS_DSI_PHY_BASE_ADR		0xFD922800
#define DSI_REG_SIZE			2048

#define VCO_CLK				424000000
static unsigned char *mdss_dsi_base;
static int pll_byte_clk_rate;
static int pll_pclk_rate;
static int pll_initialized;
static struct clk *mdss_dsi_ahb_clk;

void __init mdss_clk_ctrl_init(void)
{
	mdss_dsi_base = ioremap(MDSS_DSI_PHY_BASE_ADR,
					DSI_REG_SIZE);
	if (!mdss_dsi_base)
		pr_err("%s:%d unable to remap dsi base",
			       __func__, __LINE__);

	mdss_dsi_ahb_clk = clk_get_sys("mdss_dsi_clk_ctrl", "iface_clk");
	if (!IS_ERR(mdss_dsi_ahb_clk)) {
		clk_prepare(mdss_dsi_ahb_clk);
	} else {
		mdss_dsi_ahb_clk = NULL;
		pr_err("%s:%d unable to get iface clock\n",
			       __func__, __LINE__);
	}
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
	writel_relaxed(0x08, mdss_dsi_base + 0x022c);
	/* Loop filter capacitance values : c1 and c2 */
	writel_relaxed(0x70, mdss_dsi_base + 0x0230);
	writel_relaxed(0x15, mdss_dsi_base + 0x0234);

	writel_relaxed(0x02, mdss_dsi_base + 0x0208); /* ChgPump */
	writel_relaxed(pll_divcfg1, mdss_dsi_base + 0x0204); /* postDiv1 */
	writel_relaxed(pll_divcfg2, mdss_dsi_base + 0x0224); /* postDiv2 */
	writel_relaxed(0x03, mdss_dsi_base + 0x0228); /* postDiv3 */

	writel_relaxed(0x2b, mdss_dsi_base + 0x0278); /* Cal CFG3 */
	writel_relaxed(0x06, mdss_dsi_base + 0x027c); /* Cal CFG4 */
	writel_relaxed(0x05, mdss_dsi_base + 0x0264); /* Cal CFG4 */

	writel_relaxed(0x0a, mdss_dsi_base + 0x023c); /* SDM CFG1 */
	writel_relaxed(0xab, mdss_dsi_base + 0x0240); /* SDM CFG2 */
	writel_relaxed(0x0a, mdss_dsi_base + 0x0244); /* SDM CFG3 */
	writel_relaxed(0x00, mdss_dsi_base + 0x0248); /* SDM CFG4 */

	udelay(10);

	writel_relaxed(0x01, mdss_dsi_base + 0x0200); /* REFCLK CFG */
	writel_relaxed(0x00, mdss_dsi_base + 0x0214); /* PWRGEN CFG */
	writel_relaxed(0x01, mdss_dsi_base + 0x020c); /* VCOLPF CFG */
	writel_relaxed(0x02, mdss_dsi_base + 0x0210); /* VREG CFG */
	writel_relaxed(0x00, mdss_dsi_base + 0x0238); /* SDM CFG0 */

	writel_relaxed(0x5f, mdss_dsi_base + 0x028c); /* CAL CFG8 */
	writel_relaxed(0xa8, mdss_dsi_base + 0x0294); /* CAL CFG10 */
	writel_relaxed(0x01, mdss_dsi_base + 0x0298); /* CAL CFG11 */
	writel_relaxed(0x0a, mdss_dsi_base + 0x026c); /* CAL CFG0 */
	writel_relaxed(0x30, mdss_dsi_base + 0x0284); /* CAL CFG6 */
	writel_relaxed(0x00, mdss_dsi_base + 0x0288); /* CAL CFG7 */
	writel_relaxed(0x00, mdss_dsi_base + 0x0290); /* CAL CFG9 */
	writel_relaxed(0x20, mdss_dsi_base + 0x029c); /* EFUSE CFG */

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
	writel_relaxed(0x01, mdss_dsi_base + 0x0220); /* GLB CFG */
	writel_relaxed(0x05, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(20);
	writel_relaxed(0x07, mdss_dsi_base + 0x0220); /* GLB CFG */
	udelay(20);
	writel_relaxed(0x0f, mdss_dsi_base + 0x0220); /* GLB CFG */

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
