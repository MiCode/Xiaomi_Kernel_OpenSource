/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include "dp_catalog.h"
#include "dp_reg.h"

#define dp_read(offset) readl_relaxed((offset))
#define dp_write(offset, data) writel_relaxed((data), (offset))

#define dp_catalog_get_priv_v420(x) ({ \
	struct dp_catalog *dp_catalog; \
	dp_catalog = container_of(x, struct dp_catalog, x); \
	dp_catalog->priv.data; \
})

#define MAX_VOLTAGE_LEVELS 4
#define MAX_PRE_EMP_LEVELS 4

static u8 const vm_pre_emphasis[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x00, 0x0B, 0x12, 0xFF},       /* pe0, 0 db */
	{0x00, 0x0A, 0x12, 0xFF},       /* pe1, 3.5 db */
	{0x00, 0x0C, 0xFF, 0xFF},       /* pe2, 6.0 db */
	{0xFF, 0xFF, 0xFF, 0xFF}        /* pe3, 9.5 db */
};

/* voltage swing, 0.2v and 1.0v are not support */
static u8 const vm_voltage_swing[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x07, 0x0F, 0x14, 0xFF}, /* sw0, 0.4v  */
	{0x11, 0x1D, 0x1F, 0xFF}, /* sw1, 0.6 v */
	{0x18, 0x1F, 0xFF, 0xFF}, /* sw1, 0.8 v */
	{0xFF, 0xFF, 0xFF, 0xFF}  /* sw1, 1.2 v, optional */
};

struct dp_catalog_private_v420 {
	struct device *dev;
	struct dp_io *io;
};

static void dp_catalog_aux_setup_v420(struct dp_catalog_aux *aux,
		struct dp_aux_cfg *cfg)
{
	struct dp_catalog_private_v420 *catalog;
	int i = 0;

	if (!aux || !cfg) {
		pr_err("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v420(aux);

	dp_write(catalog->io->phy_io.base + DP_PHY_PD_CTL, 0x7D);
	wmb(); /* make sure PD programming happened */

	/* Turn on BIAS current for PHY/PLL */
	dp_write(catalog->io->dp_pll_io.base +
		QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x3d);

	/* DP AUX CFG register programming */
	for (i = 0; i < PHY_AUX_CFG_MAX; i++) {
		pr_debug("%s: offset=0x%08x, value=0x%08x\n",
			dp_phy_aux_config_type_to_string(i),
			cfg[i].offset, cfg[i].lut[cfg[i].current_index]);
		dp_write(catalog->io->phy_io.base + cfg[i].offset,
			cfg[i].lut[cfg[i].current_index]);
	}

	dp_write(catalog->io->phy_io.base + DP_PHY_AUX_INTERRUPT_MASK_V420,
		0x1F);
}

static void dp_catalog_ctrl_config_msa_v420(struct dp_catalog_ctrl *ctrl,
					u32 rate, u32 stream_rate_khz,
					bool fixed_nvid)
{
	u32 pixel_m, pixel_n;
	u32 mvid, nvid;
	u64 mvid_calc;
	u32 const nvid_fixed = 0x8000;
	u32 const link_rate_hbr2 = 540000;
	u32 const link_rate_hbr3 = 810000;
	struct dp_catalog_private_v420 *catalog;
	void __iomem *base_cc, *base_ctrl;

	if (!ctrl || !rate) {
		pr_err("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v420(ctrl);
	if (fixed_nvid) {
		pr_debug("use fixed NVID=0x%x\n", nvid_fixed);
		nvid = nvid_fixed;

		pr_debug("link rate=%dkbps, stream_rate_khz=%uKhz",
			rate, stream_rate_khz);

		/*
		 * For intermediate results, use 64 bit arithmetic to avoid
		 * loss of precision.
		 */
		mvid_calc = (u64) stream_rate_khz * nvid;
		mvid_calc = div_u64(mvid_calc, rate);

		/*
		 * truncate back to 32 bits as this final divided value will
		 * always be within the range of a 32 bit unsigned int.
		 */
		mvid = (u32) mvid_calc;
	} else {
		base_cc = catalog->io->dp_cc_io.base;

		pixel_m = dp_read(base_cc + MMSS_DP_PIXEL_M_V420);
		pixel_n = dp_read(base_cc + MMSS_DP_PIXEL_N_V420);
		pr_debug("pixel_m=0x%x, pixel_n=0x%x\n", pixel_m, pixel_n);

		mvid = (pixel_m & 0xFFFF) * 5;
		nvid = (0xFFFF & (~pixel_n)) + (pixel_m & 0xFFFF);

		pr_debug("rate = %d\n", rate);

		if (link_rate_hbr2 == rate)
			nvid *= 2;

		if (link_rate_hbr3 == rate)
			nvid *= 3;
	}

	base_ctrl = catalog->io->ctrl_io.base;
	pr_debug("mvid=0x%x, nvid=0x%x\n", mvid, nvid);
	dp_write(base_ctrl + DP_SOFTWARE_MVID, mvid);
	dp_write(base_ctrl + DP_SOFTWARE_NVID, nvid);
}

static void dp_catalog_ctrl_phy_lane_cfg_v420(struct dp_catalog_ctrl *ctrl,
		bool flipped, u8 ln_cnt)
{
	u32 info = 0x0;
	struct dp_catalog_private_v420 *catalog;
	u8 orientation = BIT(!!flipped);

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v420(ctrl);

	info |= (ln_cnt & 0x0F);
	info |= ((orientation & 0x0F) << 4);
	pr_debug("Shared Info = 0x%x\n", info);

	dp_write(catalog->io->phy_io.base + DP_PHY_SPARE0_V420, info);
}

static void dp_catalog_ctrl_update_vx_px_v420(struct dp_catalog_ctrl *ctrl,
		u8 v_level, u8 p_level)
{
	struct dp_catalog_private_v420 *catalog;
	void __iomem *base0, *base1;
	u8 value0, value1;

	if (!ctrl || !((v_level < MAX_VOLTAGE_LEVELS)
		&& (p_level < MAX_PRE_EMP_LEVELS))) {
		pr_err("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v420(ctrl);
	base0 = catalog->io->ln_tx0_io.base;
	base1 = catalog->io->ln_tx1_io.base;

	pr_debug("hw: v=%d p=%d\n", v_level, p_level);

	value0 = vm_voltage_swing[v_level][p_level];
	value1 = vm_pre_emphasis[v_level][p_level];

	/* program default setting first */
	dp_write(base0 + TXn_TX_DRV_LVL_V420, 0x2A);
	dp_write(base1 + TXn_TX_DRV_LVL_V420, 0x2A);
	dp_write(base0 + TXn_TX_EMP_POST1_LVL, 0x20);
	dp_write(base1 + TXn_TX_EMP_POST1_LVL, 0x20);

	/* Enable MUX to use Cursor values from these registers */
	value0 |= BIT(5);
	value1 |= BIT(5);

	/* Configure host and panel only if both values are allowed */
	if (value0 != 0xFF && value1 != 0xFF) {
		dp_write(base0 + TXn_TX_DRV_LVL_V420, value0);
		dp_write(base1 + TXn_TX_DRV_LVL_V420, value0);
		dp_write(base0 + TXn_TX_EMP_POST1_LVL, value1);
		dp_write(base1 + TXn_TX_EMP_POST1_LVL, value1);

		pr_debug("hw: vx_value=0x%x px_value=0x%x\n",
			value0, value1);
	} else {
		pr_err("invalid vx (0x%x=0x%x), px (0x%x=0x%x\n",
			v_level, value0, p_level, value1);
	}
}

static void dp_catalog_put_v420(struct dp_catalog *catalog)
{
	struct dp_catalog_private_v420 *catalog_priv;

	if (!catalog || !catalog->priv.data)
		return;

	catalog_priv = catalog->priv.data;
	devm_kfree(catalog_priv->dev, catalog_priv);
}

int dp_catalog_get_v420(struct device *dev, struct dp_catalog *catalog,
			struct dp_parser *parser)
{
	struct dp_catalog_private_v420 *catalog_priv;

	if (!dev || !catalog || !parser) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	catalog_priv = devm_kzalloc(dev, sizeof(*catalog_priv), GFP_KERNEL);
	if (!catalog_priv)
		return -ENOMEM;

	catalog_priv->dev = dev;
	catalog_priv->io = &parser->io;
	catalog->priv.data = catalog_priv;

	catalog->priv.put          = dp_catalog_put_v420;
	catalog->aux.setup         = dp_catalog_aux_setup_v420;
	catalog->ctrl.config_msa   = dp_catalog_ctrl_config_msa_v420;
	catalog->ctrl.phy_lane_cfg = dp_catalog_ctrl_phy_lane_cfg_v420;
	catalog->ctrl.update_vx_px = dp_catalog_ctrl_update_vx_px_v420;

	return 0;
}
