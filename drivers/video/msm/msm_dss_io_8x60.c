/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <mach/clk.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "hdmi_msm.h"
#include <mach/msm_iomap.h>

/* multimedia sub system clock control */
char *mmss_cc_base = MSM_MMSS_CLK_CTL_BASE;
/* multimedia sub system sfpb */
char *mmss_sfpb_base;
void  __iomem *periph_base;

static struct dsi_clk_desc dsicore_clk;
static struct dsi_clk_desc dsi_pclk;

static struct clk *dsi_byte_div_clk;
static struct clk *dsi_esc_clk;
static struct clk *dsi_m_pclk;
static struct clk *dsi_s_pclk;

static struct clk *amp_pclk;
int mipi_dsi_clk_on;

int mipi_dsi_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	amp_pclk = clk_get(dev, "arb_clk");
	if (IS_ERR_OR_NULL(amp_pclk)) {
		pr_err("can't find amp_pclk\n");
		amp_pclk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_m_pclk = clk_get(dev, "master_iface_clk");
	if (IS_ERR_OR_NULL(dsi_m_pclk)) {
		pr_err("can't find dsi_m_pclk\n");
		dsi_m_pclk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_s_pclk = clk_get(dev, "slave_iface_clk");
	if (IS_ERR_OR_NULL(dsi_s_pclk)) {
		pr_err("can't find dsi_s_pclk\n");
		dsi_s_pclk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_byte_div_clk = clk_get(dev, "byte_clk");
	if (IS_ERR_OR_NULL(dsi_byte_div_clk)) {
		pr_err("can't find dsi_byte_div_clk\n");
		dsi_byte_div_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_esc_clk = clk_get(dev, "esc_clk");
	if (IS_ERR_OR_NULL(dsi_esc_clk)) {
		printk(KERN_ERR "can't find dsi_esc_clk\n");
		dsi_esc_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	return 0;

mipi_dsi_clk_err:
	mipi_dsi_clk_deinit(NULL);
	return -EPERM;
}

void mipi_dsi_clk_deinit(struct device *dev)
{
	if (amp_pclk)
		clk_put(amp_pclk);
	if (dsi_m_pclk)
		clk_put(dsi_m_pclk);
	if (dsi_s_pclk)
		clk_put(dsi_s_pclk);
	if (dsi_byte_div_clk)
		clk_put(dsi_byte_div_clk);
	if (dsi_esc_clk)
		clk_put(dsi_esc_clk);
}

static void mipi_dsi_clk_ctrl(struct dsi_clk_desc *clk, int clk_en)
{
	char	*cc, *ns, *md;
	int	pmxo_sel = 0;
	char	mnd_en = 1, root_en = 1;
	uint32	data, val;

	cc = mmss_cc_base + 0x004c;
	md = mmss_cc_base + 0x0050;
	ns = mmss_cc_base + 0x0054;

	if (clk_en) {
		if (clk->mnd_mode == 0) {
			data  = clk->pre_div_func << 14;
			data |= clk->src;
			MIPI_OUTP_SECURE(ns, data);
			MIPI_OUTP_SECURE(cc, ((pmxo_sel << 8)
						| (clk->mnd_mode << 6)
						| (root_en << 2) | clk_en));
		} else {
			val = clk->d * 2;
			data = (~val) & 0x0ff;
			data |= clk->m << 8;
			MIPI_OUTP_SECURE(md, data);

			val = clk->n - clk->m;
			data = (~val) & 0x0ff;
			data <<= 24;
			data |= clk->src;
			MIPI_OUTP_SECURE(ns, data);

			MIPI_OUTP_SECURE(cc, ((pmxo_sel << 8)
					      | (clk->mnd_mode << 6)
					      | (mnd_en << 5)
					      | (root_en << 2) | clk_en));
		}

	} else
		MIPI_OUTP_SECURE(cc, 0);

	wmb();
}

static void mipi_dsi_sfpb_cfg(void)
{
	char *sfpb;
	int data;

	sfpb = mmss_sfpb_base + 0x058;

	data = MIPI_INP(sfpb);
	data |= 0x01800;
	MIPI_OUTP(sfpb, data);
	wmb();
}

static void mipi_dsi_pclk_ctrl(struct dsi_clk_desc *clk, int clk_en)
{
	char	*cc, *ns, *md;
	char	mnd_en = 1, root_en = 1;
	uint32	data, val;

	cc = mmss_cc_base + 0x0130;
	md = mmss_cc_base + 0x0134;
	ns = mmss_cc_base + 0x0138;

	if (clk_en) {
		if (clk->mnd_mode == 0) {
			data  = clk->pre_div_func << 12;
			data |= clk->src;
			MIPI_OUTP_SECURE(ns, data);
			MIPI_OUTP_SECURE(cc, ((clk->mnd_mode << 6)
					      | (root_en << 2) | clk_en));
		} else {
			val = clk->d * 2;
			data = (~val) & 0x0ff;
			data |= clk->m << 8;
			MIPI_OUTP_SECURE(md, data);

			val = clk->n - clk->m;
			data = (~val) & 0x0ff;
			data <<= 24;
			data |= clk->src;
			MIPI_OUTP_SECURE(ns, data);

			MIPI_OUTP_SECURE(cc, ((clk->mnd_mode << 6)
					      | (mnd_en << 5)
					      | (root_en << 2) | clk_en));
		}

	} else
		MIPI_OUTP_SECURE(cc, 0);

	wmb();
}

static void mipi_dsi_ahb_en(void)
{
	char	*ahb;

	ahb = mmss_cc_base + 0x08;

	pr_debug("%s: ahb=%x %x\n",
		__func__, (int) ahb, MIPI_INP_SECURE(ahb));
}

static void mipi_dsi_calibration(void)
{
	uint32 data;

	MIPI_OUTP(MIPI_DSI_BASE + 0xf4, 0x0000ff11); /* cal_ctrl */
	MIPI_OUTP(MIPI_DSI_BASE + 0xf0, 0x01); /* cal_hw_trigger */

	while (1) {
		data = MIPI_INP(MIPI_DSI_BASE + 0xfc); /* cal_status */
		if ((data & 0x10000000) == 0)
			break;

		udelay(10);
	}
}

#define PREF_DIV_RATIO 27
struct dsiphy_pll_divider_config pll_divider_config;


int mipi_dsi_phy_pll_config(u32 clk_rate)
{
	struct dsiphy_pll_divider_config *dividers;
	u32 fb_divider, tmp;
	dividers = &pll_divider_config;

	/* DSIPHY_PLL_CTRL_x:    1     2     3     8     9     10 */
	/* masks               0xff  0x07  0x3f  0x0f  0xff  0xff */

	/* DSIPHY_PLL_CTRL_1 */
	fb_divider = ((dividers->fb_divider) / 2) - 1;
	MIPI_OUTP(MIPI_DSI_BASE + 0x204, fb_divider & 0xff);

	/* DSIPHY_PLL_CTRL_2 */
	tmp = MIPI_INP(MIPI_DSI_BASE + 0x208);
	tmp &= ~0x07;
	tmp |= (fb_divider >> 8) & 0x07;
	MIPI_OUTP(MIPI_DSI_BASE + 0x208, tmp);

	/* DSIPHY_PLL_CTRL_3 */
	tmp = MIPI_INP(MIPI_DSI_BASE + 0x20c);
	tmp &= ~0x3f;
	tmp |= (dividers->ref_divider_ratio - 1) & 0x3f;
	MIPI_OUTP(MIPI_DSI_BASE + 0x20c, tmp);

	/* DSIPHY_PLL_CTRL_8 */
	tmp = MIPI_INP(MIPI_DSI_BASE + 0x220);
	tmp &= ~0x0f;
	tmp |= (dividers->bit_clk_divider - 1) & 0x0f;
	MIPI_OUTP(MIPI_DSI_BASE + 0x220, tmp);

	/* DSIPHY_PLL_CTRL_9 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x224, (dividers->byte_clk_divider - 1));

	/* DSIPHY_PLL_CTRL_10 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x228, (dividers->dsi_clk_divider - 1));

	return 0;
}

int mipi_dsi_clk_div_config(uint8 bpp, uint8 lanes,
			    uint32 *expected_dsi_pclk)
{
	u32 fb_divider, rate, vco;
	u32 div_ratio = 0;
	struct dsi_clk_mnd_table const *mnd_entry = mnd_table;
	if (pll_divider_config.clk_rate == 0)
		pll_divider_config.clk_rate = 454000000;

	rate = pll_divider_config.clk_rate / 1000000; /* In Mhz */

	if (rate < 125) {
		vco = rate * 8;
		div_ratio = 8;
	} else if (rate < 250) {
		vco = rate * 4;
		div_ratio = 4;
	} else if (rate < 500) {
		vco = rate * 2;
		div_ratio = 2;
	} else {
		vco = rate * 1;
		div_ratio = 1;
	}

	/* find the mnd settings from mnd_table entry */
	for (; mnd_entry != mnd_table + ARRAY_SIZE(mnd_table); ++mnd_entry) {
		if (((mnd_entry->lanes) == lanes) &&
			((mnd_entry->bpp) == bpp))
			break;
	}

	if (mnd_entry == mnd_table + ARRAY_SIZE(mnd_table)) {
		pr_err("%s: requested Lanes, %u & BPP, %u, not supported\n",
			__func__, lanes, bpp);
		return -EINVAL;
	}
	fb_divider = ((vco * PREF_DIV_RATIO) / 27);
	pll_divider_config.fb_divider = fb_divider;
	pll_divider_config.ref_divider_ratio = PREF_DIV_RATIO;
	pll_divider_config.bit_clk_divider = div_ratio;
	pll_divider_config.byte_clk_divider =
			pll_divider_config.bit_clk_divider * 8;
	pll_divider_config.dsi_clk_divider =
			(mnd_entry->dsiclk_div) * div_ratio;

	if ((mnd_entry->dsiclk_d == 0)
		|| (mnd_entry->dsiclk_m == 1)) {
		dsicore_clk.mnd_mode = 0;
		dsicore_clk.src = 0x3;
		dsicore_clk.pre_div_func = (mnd_entry->dsiclk_n - 1);
	} else {
		dsicore_clk.mnd_mode = 2;
		dsicore_clk.src = 0x3;
		dsicore_clk.m = mnd_entry->dsiclk_m;
		dsicore_clk.n = mnd_entry->dsiclk_n;
		dsicore_clk.d = mnd_entry->dsiclk_d;
	}

	if ((mnd_entry->pclk_d == 0)
		|| (mnd_entry->pclk_m == 1)) {
		dsi_pclk.mnd_mode = 0;
		dsi_pclk.src = 0x3;
		dsi_pclk.pre_div_func = (mnd_entry->pclk_n - 1);
		*expected_dsi_pclk = ((vco * 1000000) /
					((pll_divider_config.dsi_clk_divider)
					* (mnd_entry->pclk_n)));
	} else {
		dsi_pclk.mnd_mode = 2;
		dsi_pclk.src = 0x3;
		dsi_pclk.m = mnd_entry->pclk_m;
		dsi_pclk.n = mnd_entry->pclk_n;
		dsi_pclk.d = mnd_entry->pclk_d;
		*expected_dsi_pclk = ((vco * 1000000 * dsi_pclk.m) /
					((pll_divider_config.dsi_clk_divider)
					* (mnd_entry->pclk_n)));
	}
	return 0;
}

void mipi_dsi_phy_init(int panel_ndx, struct msm_panel_info const *panel_info,
	int target_type)
{
	struct mipi_dsi_phy_ctrl *pd;
	int i, off;

	MIPI_OUTP(MIPI_DSI_BASE + 0x128, 0x0001);/* start phy sw reset */
	msleep(100);
	MIPI_OUTP(MIPI_DSI_BASE + 0x128, 0x0000);/* end phy w reset */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2cc, 0x0003);/* regulator_ctrl_0 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2d0, 0x0001);/* regulator_ctrl_1 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2d4, 0x0001);/* regulator_ctrl_2 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2d8, 0x0000);/* regulator_ctrl_3 */
#ifdef DSI_POWER
	MIPI_OUTP(MIPI_DSI_BASE + 0x2dc, 0x0100);/* regulator_ctrl_4 */
#endif

	pd = (panel_info->mipi).dsi_phy_db;

	off = 0x02cc;	/* regulator ctrl 0 */
	for (i = 0; i < 4; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->regulator[i]);
		wmb();
		off += 4;
	}

	off = 0x0260;	/* phy timig ctrl 0 */
	for (i = 0; i < 11; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	off = 0x0290;	/* ctrl 0 */
	for (i = 0; i < 4; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->ctrl[i]);
		wmb();
		off += 4;
	}

	off = 0x02a0;	/* strength 0 */
	for (i = 0; i < 4; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->strength[i]);
		wmb();
		off += 4;
	}

	mipi_dsi_calibration();

	off = 0x0204;	/* pll ctrl 1, skip 0 */
	for (i = 1; i < 21; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->pll[i]);
		wmb();
		off += 4;
	}

	if (panel_info)
		mipi_dsi_phy_pll_config(panel_info->clk_rate);

	/* pll ctrl 0 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x200, pd->pll[0]);
	wmb();
}

void cont_splash_clk_ctrl(int enable)
{
}

void mipi_dsi_prepare_clocks(void)
{
	clk_prepare(amp_pclk);
	clk_prepare(dsi_m_pclk);
	clk_prepare(dsi_s_pclk);
	clk_prepare(dsi_byte_div_clk);
	clk_prepare(dsi_esc_clk);
}

void mipi_dsi_unprepare_clocks(void)
{
	clk_unprepare(dsi_esc_clk);
	clk_unprepare(dsi_byte_div_clk);
	clk_unprepare(dsi_m_pclk);
	clk_unprepare(dsi_s_pclk);
	clk_unprepare(amp_pclk);
}

void mipi_dsi_ahb_ctrl(u32 enable)
{
	static int ahb_ctrl_done;
	if (enable) {
		if (ahb_ctrl_done) {
			pr_info("%s: ahb clks already ON\n", __func__);
			return;
		}
		clk_enable(amp_pclk); /* clock for AHB-master to AXI */
		clk_enable(dsi_m_pclk);
		clk_enable(dsi_s_pclk);
		mipi_dsi_ahb_en();
		mipi_dsi_sfpb_cfg();
		ahb_ctrl_done = 1;
	} else {
		if (ahb_ctrl_done == 0) {
			pr_info("%s: ahb clks already OFF\n", __func__);
			return;
		}
		clk_disable(dsi_m_pclk);
		clk_disable(dsi_s_pclk);
		clk_disable(amp_pclk); /* clock for AHB-master to AXI */
		ahb_ctrl_done = 0;
	}
}

void mipi_dsi_clk_enable(void)
{
	u32 pll_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x0200);
	if (mipi_dsi_clk_on) {
		pr_info("%s: mipi_dsi_clks already ON\n", __func__);
		return;
	}
	MIPI_OUTP(MIPI_DSI_BASE + 0x0200, pll_ctrl | 0x01);
	mb();

	if (clk_set_rate(dsi_byte_div_clk, 1) < 0)	/* divided by 1 */
		pr_err("%s: clk_set_rate failed\n",	__func__);
	mipi_dsi_pclk_ctrl(&dsi_pclk, 1);
	mipi_dsi_clk_ctrl(&dsicore_clk, 1);
	clk_enable(dsi_byte_div_clk);
	clk_enable(dsi_esc_clk);
	mipi_dsi_clk_on = 1;
}

void mipi_dsi_clk_disable(void)
{
	if (mipi_dsi_clk_on == 0) {
		pr_info("%s: mipi_dsi_clks already OFF\n", __func__);
		return;
	}
	clk_disable(dsi_esc_clk);
	clk_disable(dsi_byte_div_clk);

	mipi_dsi_pclk_ctrl(&dsi_pclk, 0);
	mipi_dsi_clk_ctrl(&dsicore_clk, 0);
	/* DSIPHY_PLL_CTRL_0, disable dsi pll */
	MIPI_OUTP(MIPI_DSI_BASE + 0x0200, 0x40);
	mipi_dsi_clk_on = 0;
}

void mipi_dsi_phy_ctrl(int on)
{
	if (on) {
		/* DSIPHY_PLL_CTRL_5 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0214, 0x050);

		/* DSIPHY_TPA_CTRL_1 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0258, 0x00f);

		/* DSIPHY_TPA_CTRL_2 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x025c, 0x000);
	} else {
		/* DSIPHY_PLL_CTRL_5 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0214, 0x05f);

		/* DSIPHY_TPA_CTRL_1 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0258, 0x08f);

		/* DSIPHY_TPA_CTRL_2 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x025c, 0x001);

		/* DSIPHY_REGULATOR_CTRL_0 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x02cc, 0x02);

		/* DSIPHY_CTRL_0 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0290, 0x00);

		/* DSIPHY_CTRL_1 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0294, 0x7f);

		/* disable dsi clk */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0118, 0);
	}
}

#ifdef CONFIG_FB_MSM_HDMI_COMMON
#define SW_RESET BIT(2)
void hdmi_phy_reset(void)
{
	unsigned int phy_reset_polarity = 0x0;
	unsigned int val = HDMI_INP_ND(0x2D4);

	phy_reset_polarity = val >> 3 & 0x1;

	if (phy_reset_polarity == 0)
		HDMI_OUTP(0x2D4, val | SW_RESET);
	else
		HDMI_OUTP(0x2D4, val & (~SW_RESET));

	msleep(100);

	if (phy_reset_polarity == 0)
		HDMI_OUTP(0x2D4, val & (~SW_RESET));
	else
		HDMI_OUTP(0x2D4, val | SW_RESET);
}

void hdmi_msm_reset_core(void)
{
	hdmi_msm_set_mode(FALSE);
	hdmi_msm_clk(0);
	udelay(5);
	hdmi_msm_clk(1);

	clk_reset(hdmi_msm_state->hdmi_app_clk, CLK_RESET_ASSERT);
	clk_reset(hdmi_msm_state->hdmi_m_pclk, CLK_RESET_ASSERT);
	clk_reset(hdmi_msm_state->hdmi_s_pclk, CLK_RESET_ASSERT);
	udelay(20);
	clk_reset(hdmi_msm_state->hdmi_app_clk, CLK_RESET_DEASSERT);
	clk_reset(hdmi_msm_state->hdmi_m_pclk, CLK_RESET_DEASSERT);
	clk_reset(hdmi_msm_state->hdmi_s_pclk, CLK_RESET_DEASSERT);
}

void hdmi_msm_init_phy(int video_format)
{
	uint32 offset;
	/* De-serializer delay D/C for non-lbk mode
	 * PHY REG0 = (DESER_SEL(0) | DESER_DEL_CTRL(3)
	 * | AMUX_OUT_SEL(0))
	 */
	HDMI_OUTP_ND(0x0300, 0x0C); /*0b00001100*/

	if (video_format == HDMI_VFRMT_720x480p60_16_9) {
		/* PHY REG1 = DTEST_MUX_SEL(5) | PLL_GAIN_SEL(0)
		 * | OUTVOL_SWING_CTRL(3)
		 */
		HDMI_OUTP_ND(0x0304, 0x53); /*0b01010011*/
	} else {
		/* If the freq. is less than 120MHz, use low gain 0
		 * for board with termination
		 * PHY REG1 = DTEST_MUX_SEL(5) | PLL_GAIN_SEL(0)
		 * | OUTVOL_SWING_CTRL(4)
		 */
		HDMI_OUTP_ND(0x0304, 0x54); /*0b01010100*/
	}

	/* No matter what, start from the power down mode
	 * PHY REG2 = PD_PWRGEN | PD_PLL | PD_DRIVE_4 | PD_DRIVE_3
	 * | PD_DRIVE_2 | PD_DRIVE_1 | PD_DESER
	 */
	HDMI_OUTP_ND(0x0308, 0x7F); /*0b01111111*/

	/* Turn PowerGen on
	 * PHY REG2 = PD_PLL | PD_DRIVE_4 | PD_DRIVE_3
	 * | PD_DRIVE_2 | PD_DRIVE_1 | PD_DESER
	 */
	HDMI_OUTP_ND(0x0308, 0x3F); /*0b00111111*/

	/* Turn PLL power on
	 * PHY REG2 = PD_DRIVE_4 | PD_DRIVE_3
	 * | PD_DRIVE_2 | PD_DRIVE_1 | PD_DESER
	 */
	HDMI_OUTP_ND(0x0308, 0x1F); /*0b00011111*/

	/* Write to HIGH after PLL power down de-assert
	 * PHY REG3 = PLL_ENABLE
	 */
	HDMI_OUTP_ND(0x030C, 0x01);
	/* ASIC power on; PHY REG9 = 0 */
	HDMI_OUTP_ND(0x0324, 0x00);
	/* Enable PLL lock detect, PLL lock det will go high after lock
	 * Enable the re-time logic
	 * PHY REG12 = PLL_LOCK_DETECT_EN | RETIMING_ENABLE
	 */
	HDMI_OUTP_ND(0x0330, 0x03); /*0b00000011*/

	/* Drivers are on
	 * PHY REG2 = PD_DESER
	 */
	HDMI_OUTP_ND(0x0308, 0x01); /*0b00000001*/
	/* If the RX detector is needed
	 * PHY REG2 = RCV_SENSE_EN | PD_DESER
	 */
	HDMI_OUTP_ND(0x0308, 0x81); /*0b10000001*/

	offset = 0x0310;
	while (offset <= 0x032C) {
		HDMI_OUTP(offset, 0x0);
		offset += 0x4;
	}

	/* If we want to use lock enable based on counting
	 * PHY REG12 = FORCE_LOCK | PLL_LOCK_DETECT_EN | RETIMING_ENABLE
	 */
	HDMI_OUTP_ND(0x0330, 0x13); /*0b00010011*/
}

void hdmi_msm_powerdown_phy(void)
{
	/* Assert RESET PHY from controller */
	HDMI_OUTP_ND(0x02D4, 0x4);
	udelay(10);
	/* De-assert RESET PHY from controller */
	HDMI_OUTP_ND(0x02D4, 0x0);
	/* Turn off Driver */
	HDMI_OUTP_ND(0x0308, 0x1F);
	udelay(10);
	/* Disable PLL */
	HDMI_OUTP_ND(0x030C, 0x00);
	/* Power down PHY */
	HDMI_OUTP_ND(0x0308, 0x7F); /*0b01111111*/
}

void hdmi_frame_ctrl_cfg(const struct hdmi_disp_mode_timing_type *timing)
{
	/*  0x02C8 HDMI_FRAME_CTRL
	 *  31 INTERLACED_EN   Interlaced or progressive enable bit
	 *    0: Frame in progressive
	 *    1: Frame is interlaced
	 *  29 HSYNC_HDMI_POL  HSYNC polarity fed to HDMI core
	 *     0: Active Hi Hsync, detect the rising edge of hsync
	 *     1: Active lo Hsync, Detect the falling edge of Hsync
	 *  28 VSYNC_HDMI_POL  VSYNC polarity fed to HDMI core
	 *     0: Active Hi Vsync, detect the rising edge of vsync
	 *     1: Active Lo Vsync, Detect the falling edge of Vsync
	 *  12 RGB_MUX_SEL     ALPHA mdp4 input is RGB, mdp4 input is BGR
	 */
	HDMI_OUTP(0x02C8,
		  ((timing->interlaced << 31) & 0x80000000)
		| ((timing->active_low_h << 29) & 0x20000000)
		| ((timing->active_low_v << 28) & 0x10000000)
		| (1 << 12));
}

void hdmi_msm_phy_status_poll(void)
{
	unsigned int phy_ready;
	phy_ready = 0x1 & HDMI_INP_ND(0x33c);
	if (phy_ready) {
		pr_debug("HDMI Phy Status bit is set and ready\n");
	} else {
		pr_debug("HDMI Phy Status bit is not set,"
			"waiting for ready status\n");
		do {
			phy_ready = 0x1 & HDMI_INP_ND(0x33c);
		} while (!phy_ready);
	}
}
#endif
