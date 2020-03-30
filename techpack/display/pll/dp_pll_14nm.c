// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

/*
 ***************************************************************************
 ******** Display Port PLL driver block diagram for branch clocks **********
 ***************************************************************************

			+--------------------------+
			|       DP_VCO_CLK         |
			|			   |
			|  +-------------------+   |
			|  |   (DP PLL/VCO)    |   |
			|  +---------+---------+   |
			|	     v		   |
			| +----------+-----------+ |
			| | hsclk_divsel_clk_src | |
			| +----------+-----------+ |
			+--------------------------+
				     |
				     v
	   +------------<------------|------------>-------------+
	   |                         |                          |
+----------v----------+	  +----------v----------+    +----------v----------+
|   dp_link_2x_clk    |	  | vco_divided_clk_src	|    | vco_divided_clk_src |
|     divsel_five     |	  |			|    |			   |
v----------+----------v	  |	divsel_two	|    |	   divsel_four	   |
	   |		  +----------+----------+    +----------+----------+
	   |                         |                          |
	   v			     v				v
				     |	+---------------------+	|
  Input to MMSSCC block		     |	|    (aux_clk_ops)    |	|
  for link clk, crypto clk	     +-->   vco_divided_clk   <-+
  and interface clock			|	_src_mux      |
					+----------+----------+
						   |
						   v
					 Input to MMSSCC block
					 for DP pixel clock

 ******************************************************************************
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/usb/usbpd.h>

#include <dt-bindings/clock/mdss-14nm-pll-clk.h>

#include "pll_drv.h"
#include "dp_pll.h"
#include "dp_pll_14nm.h"

static struct dp_pll_db dp_pdb;
static struct clk_ops mux_clk_ops;

static struct regmap_config dp_pll_14nm_cfg = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register = 0x910,
};

static struct regmap_bus dp_pixel_mux_regmap_ops = {
	.reg_write = dp_mux_set_parent_14nm,
	.reg_read = dp_mux_get_parent_14nm,
};

/* Op structures */
static const struct clk_ops dp_14nm_vco_clk_ops = {
	.recalc_rate = dp_vco_recalc_rate_14nm,
	.set_rate = dp_vco_set_rate_14nm,
	.round_rate = dp_vco_round_rate_14nm,
	.prepare = dp_vco_prepare_14nm,
	.unprepare = dp_vco_unprepare_14nm,
};

static struct dp_pll_vco_clk dp_vco_clk = {
	.min_rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000,
	.max_rate = DP_VCO_HSCLK_RATE_5400MHZDIV1000,
	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_clk",
		.parent_names = (const char *[]){ "xo_board" },
		.num_parents = 1,
		.ops = &dp_14nm_vco_clk_ops,
	},
};

static struct clk_fixed_factor dp_phy_pll_link_clk = {
	.div = 10,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_phy_pll_link_clk",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_two_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_two_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_four_clk_src = {
	.div = 4,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_four_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE),
		.ops = &clk_fixed_factor_ops,
	},
};

static int clk_mux_determine_rate(struct clk_hw *hw,
				     struct clk_rate_request *req)
{
	int ret = 0;

	ret = __clk_mux_determine_rate_closest(hw, req);
	if (ret)
		return ret;

	/* Set the new parent of mux if there is a new valid parent */
	if (hw->clk && req->best_parent_hw->clk)
		clk_set_parent(hw->clk, req->best_parent_hw->clk);

	return 0;
}


static unsigned long mux_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk *div_clk = NULL, *vco_clk = NULL;
	struct dp_pll_vco_clk *vco = NULL;

	div_clk = clk_get_parent(hw->clk);
	if (!div_clk)
		return 0;

	vco_clk = clk_get_parent(div_clk);
	if (!vco_clk)
		return 0;

	vco = to_dp_vco_hw(__clk_get_hw(vco_clk));
	if (!vco)
		return 0;

	if (vco->rate == DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		return (vco->rate / 4);
	else
		return (vco->rate / 2);
}

static struct clk_regmap_mux dp_phy_pll_vco_div_clk = {
	.reg = 0x64,
	.shift = 0,
	.width = 1,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dp_phy_pll_vco_div_clk",
			.parent_names =
				(const char *[]){"dp_vco_divsel_two_clk_src",
					"dp_vco_divsel_four_clk_src"},
			.num_parents = 2,
			.ops = &mux_clk_ops,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		},
	},
};

static struct clk_hw *mdss_dp_pllcc_14nm[] = {
	[DP_VCO_CLK] = &dp_vco_clk.hw,
	[DP_PHY_PLL_LINK_CLK] = &dp_phy_pll_link_clk.hw,
	[DP_VCO_DIVSEL_FOUR_CLK_SRC] = &dp_vco_divsel_four_clk_src.hw,
	[DP_VCO_DIVSEL_TWO_CLK_SRC] = &dp_vco_divsel_two_clk_src.hw,
	[DP_PHY_PLL_VCO_DIV_CLK] = &dp_phy_pll_vco_div_clk.clkr.hw,
};


int dp_mux_set_parent_14nm(void *context, unsigned int reg, unsigned int val)
{
	struct mdss_pll_resources *dp_res = context;
	int rc;
	u32 auxclk_div;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP PLL resources\n");
		return rc;
	}

	auxclk_div = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_VCO_DIV);
	auxclk_div &= ~0x03;	/* bits 0 to 1 */

	if (val == 0) /* mux parent index = 0 */
		auxclk_div |= 1;
	else if (val == 1) /* mux parent index = 1 */
		auxclk_div |= 2;

	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_VCO_DIV, auxclk_div);
	/* Make sure the PHY registers writes are done */
	wmb();
	pr_debug("mux=%d auxclk_div=%x\n", val, auxclk_div);

	mdss_pll_resource_enable(dp_res, false);

	return 0;
}

int dp_mux_get_parent_14nm(void *context, unsigned int reg, unsigned int *val)
{
	int rc;
	u32 auxclk_div = 0;
	struct mdss_pll_resources *dp_res = context;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable dp_res resources\n");
		return rc;
	}

	auxclk_div = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_VCO_DIV);
	auxclk_div &= 0x03;

	if (auxclk_div == 1) /* Default divider */
		*val = 0;
	else if (auxclk_div == 2)
		*val = 1;

	mdss_pll_resource_enable(dp_res, false);

	pr_debug("auxclk_div=%d, val=%d\n", auxclk_div, *val);

	return 0;
}

static int dp_vco_pll_init_db_14nm(struct dp_pll_db *pdb,
		unsigned long rate)
{
	struct mdss_pll_resources *dp_res = pdb->pll;
	u32 spare_value = 0;

	spare_value = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_SPARE0);
	pdb->lane_cnt = spare_value & 0x0F;
	pdb->orientation = (spare_value & 0xF0) >> 4;

	pr_debug("spare_value=0x%x, ln_cnt=0x%x, orientation=0x%x\n",
			spare_value, pdb->lane_cnt, pdb->orientation);

	switch (rate) {
	case DP_VCO_HSCLK_RATE_1620MHZDIV1000:
		pdb->hsclk_sel = 0x2c;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start1_mode0 = 0x00;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0xbf;
		pdb->lock_cmp2_mode0 = 0x21;
		pdb->lock_cmp3_mode0 = 0x00;
		pdb->phy_vco_div = 0x1;
		pdb->lane_mode_1 = 0xc6;
		break;
	case DP_VCO_HSCLK_RATE_2700MHZDIV1000:
		pdb->hsclk_sel = 0x24;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start1_mode0 = 0x00;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x3f;
		pdb->lock_cmp2_mode0 = 0x38;
		pdb->lock_cmp3_mode0 = 0x00;
		pdb->phy_vco_div = 0x1;
		pdb->lane_mode_1 = 0xc4;
		break;
	case DP_VCO_HSCLK_RATE_5400MHZDIV1000:
		pdb->hsclk_sel = 0x20;
		pdb->dec_start_mode0 = 0x8c;
		pdb->div_frac_start1_mode0 = 0x00;
		pdb->div_frac_start2_mode0 = 0x00;
		pdb->div_frac_start3_mode0 = 0x0a;
		pdb->lock_cmp1_mode0 = 0x7f;
		pdb->lock_cmp2_mode0 = 0x70;
		pdb->lock_cmp3_mode0 = 0x00;
		pdb->phy_vco_div = 0x2;
		pdb->lane_mode_1 = 0xc4;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int dp_config_vco_rate_14nm(struct dp_pll_vco_clk *vco,
		unsigned long rate)
{
	u32 res = 0;
	struct mdss_pll_resources *dp_res = vco->priv;
	struct dp_pll_db *pdb = (struct dp_pll_db *)dp_res->priv;

	res = dp_vco_pll_init_db_14nm(pdb, rate);
	if (res) {
		pr_err("VCO Init DB failed\n");
		return res;
	}

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_PD_CTL, 0x3d);

	/* Make sure the PHY register writes are done */
	wmb();

	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_SVS_MODE_CLK_SEL, 0x01);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_SYSCLK_EN_SEL, 0x37);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CLK_SELECT, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_SYS_CLK_CTRL, 0x06);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x3f);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CLK_ENABLE1, 0x0e);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_BG_CTRL, 0x0f);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_SYSCLK_BUF_ENABLE, 0x06);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CLK_SELECT, 0x30);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_PLL_IVCO, 0x0f);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_PLL_CCTRL_MODE0, 0x28);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_PLL_RCTRL_MODE0, 0x16);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CP_CTRL_MODE0, 0x0b);

	/* Parameters dependent on vco clock frequency */
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_HSCLK_SEL, pdb->hsclk_sel);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DEC_START_MODE0, pdb->dec_start_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DIV_FRAC_START1_MODE0, pdb->div_frac_start1_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DIV_FRAC_START2_MODE0, pdb->div_frac_start2_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DIV_FRAC_START3_MODE0, pdb->div_frac_start3_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_LOCK_CMP1_MODE0, pdb->lock_cmp1_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_LOCK_CMP2_MODE0, pdb->lock_cmp2_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_LOCK_CMP3_MODE0, pdb->lock_cmp3_mode0);

	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_INTEGLOOP_GAIN0_MODE0, 0x40);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_INTEGLOOP_GAIN1_MODE0, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_VCO_TUNE_MAP, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_BG_TIMER, 0x08);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CORECLK_DIV, 0x05);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_VCO_TUNE1_MODE0, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_VCO_TUNE2_MODE0, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	wmb(); /* make sure write happens */

	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CORE_CLK_EN, 0x0f);
	wmb(); /* make sure write happens */

	if (pdb->orientation == ORIENTATION_CC2)
		MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_MODE, 0xc9);
	else
		MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_MODE, 0xd9);
	wmb(); /* make sure write happens */

	/* TX Lane configuration */
	MDSS_PLL_REG_W(dp_res->phy_base,
		DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base,
		DP_PHY_TX2_TX3_LANE_CTL, 0x05);

	/* TX-0 register configuration */
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TRANSCEIVER_BIAS_EN, 0x1a);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_VMODE_CTRL1, 0x40);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_INTERFACE_SELECT, 0x3d);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_CLKBUF_ENABLE, 0x0f);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TX_EMP_POST1_LVL, 0x2b);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TX_DRV_LVL, 0x2f);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TX_BAND, 0x4);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_RES_CODE_LANE_OFFSET_TX, 0x12);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_RES_CODE_LANE_OFFSET_RX, 0x12);

	/* TX-1 register configuration */
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TRANSCEIVER_BIAS_EN, 0x1a);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_VMODE_CTRL1, 0x40);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_INTERFACE_SELECT, 0x3d);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_CLKBUF_ENABLE, 0x0f);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TX_EMP_POST1_LVL, 0x2b);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TX_DRV_LVL, 0x2f);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TX_BAND, 0x4);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_RES_CODE_LANE_OFFSET_TX, 0x12);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_RES_CODE_LANE_OFFSET_RX, 0x12);
	wmb(); /* make sure write happens */

	/* PHY VCO divider programming */
	MDSS_PLL_REG_W(dp_res->phy_base,
		DP_PHY_VCO_DIV, pdb->phy_vco_div);
	wmb(); /* make sure write happens */

	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CMN_CONFIG, 0x02);
	wmb(); /* make sure write happens */

	return res;
}

static bool dp_14nm_pll_lock_status(struct mdss_pll_resources *dp_res)
{
	u32 status;
	bool pll_locked;

	/* poll for PLL lock status */
	if (readl_poll_timeout_atomic((dp_res->pll_base +
			QSERDES_COM_C_READY_STATUS),
			status,
			((status & BIT(0)) > 0),
			DP_PLL_POLL_SLEEP_US,
			DP_PLL_POLL_TIMEOUT_US)) {
		pr_err("C_READY status is not high. Status=%x\n", status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}

static bool dp_14nm_phy_rdy_status(struct mdss_pll_resources *dp_res)
{
	u32 status;
	bool phy_ready = true;

	/* poll for PHY ready status */
	if (readl_poll_timeout_atomic((dp_res->phy_base +
			DP_PHY_STATUS),
			status,
			((status & (BIT(1) | BIT(0))) > 0),
			DP_PHY_POLL_SLEEP_US,
			DP_PHY_POLL_TIMEOUT_US)) {
		pr_err("Phy_ready is not high. Status=%x\n", status);
		phy_ready = false;
	}

	return phy_ready;
}

static int dp_pll_enable_14nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x01);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x01);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x09);
	wmb(); /* Make sure the PHY register writes are done */

	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_RESETSM_CNTRL, 0x20);
	wmb();	/* Make sure the PLL register writes are done */

	udelay(900); /* hw recommended delay for full PU */

	if (!dp_14nm_pll_lock_status(dp_res)) {
		rc = -EINVAL;
		goto lock_err;
	}

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x19);
	wmb();	/* Make sure the PHY register writes are done */

	udelay(10); /* hw recommended delay */

	if (!dp_14nm_phy_rdy_status(dp_res)) {
		rc = -EINVAL;
		goto lock_err;
	}

	pr_debug("PLL is locked\n");

	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TRANSCEIVER_BIAS_EN, 0x3f);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_HIGHZ_DRVR_EN, 0x10);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TRANSCEIVER_BIAS_EN, 0x3f);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_HIGHZ_DRVR_EN, 0x10);

	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX0_OFFSET + TXn_TX_POL_INV, 0x0a);
	MDSS_PLL_REG_W(dp_res->phy_base,
		QSERDES_TX1_OFFSET + TXn_TX_POL_INV, 0x0a);

	/*
	 * Switch DP Mainlink clock (cc_dpphy_link_clk) from DP
	 * controller side with final frequency
	 */
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x18);
	wmb();	/* Make sure the PHY register writes are done */
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x19);
	wmb();	/* Make sure the PHY register writes are done */

lock_err:
	return rc;
}

static int dp_pll_disable_14nm(struct clk_hw *hw)
{
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;

	/* Assert DP PHY power down */
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_PD_CTL, 0x2);
	/*
	 * Make sure all the register writes to disable PLL are
	 * completed before doing any other operation
	 */
	wmb();

	return 0;
}


int dp_vco_prepare_14nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;

	pr_debug("rate=%ld\n", vco->rate);
	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP pll resources\n");
		goto error;
	}

	if ((dp_res->vco_cached_rate != 0)
		&& (dp_res->vco_cached_rate == vco->rate)) {
		rc = vco->hw.init->ops->set_rate(hw,
			dp_res->vco_cached_rate, dp_res->vco_cached_rate);
		if (rc) {
			pr_err("index=%d vco_set_rate failed. rc=%d\n",
				rc, dp_res->index);
			mdss_pll_resource_enable(dp_res, false);
			goto error;
		}
	}

	rc = dp_pll_enable_14nm(hw);
	if (rc) {
		mdss_pll_resource_enable(dp_res, false);
		pr_err("ndx=%d failed to enable dp pll\n",
					dp_res->index);
		goto error;
	}

	mdss_pll_resource_enable(dp_res, false);
error:
	return rc;
}

void dp_vco_unprepare_14nm(struct clk_hw *hw)
{
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;

	if (!dp_res) {
		pr_err("Invalid input parameter\n");
		return;
	}

	if (!dp_res->pll_on &&
		mdss_pll_resource_enable(dp_res, true)) {
		pr_err("pll resource can't be enabled\n");
		return;
	}
	dp_res->vco_cached_rate = vco->rate;
	dp_pll_disable_14nm(hw);

	dp_res->handoff_resources = false;
	mdss_pll_resource_enable(dp_res, false);
	dp_res->pll_on = false;
}

int dp_vco_set_rate_14nm(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;
	int rc;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	pr_debug("DP lane CLK rate=%ld\n", rate);

	rc = dp_config_vco_rate_14nm(vco, rate);
	if (rc)
		pr_err("Failed to set clk rate\n");

	mdss_pll_resource_enable(dp_res, false);

	vco->rate = rate;

	return 0;
}

unsigned long dp_vco_recalc_rate_14nm(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	int rc;
	u32 div, hsclk_div;
	u64 vco_rate;
	struct mdss_pll_resources *dp_res = vco->priv;

	if (is_gdsc_disabled(dp_res))
		return 0;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP pll=%d\n", dp_res->index);
		return rc;
	}

	div = MDSS_PLL_REG_R(dp_res->pll_base, QSERDES_COM_HSCLK_SEL);
	div &= 0x0f;

	if (div == 12)
		hsclk_div = 5; /* Default */
	else if (div == 4)
		hsclk_div = 3;
	else if (div == 0)
		hsclk_div = 2;
	else {
		pr_debug("unknown divider. forcing to default\n");
		hsclk_div = 5;
	}

	if (hsclk_div == 5)
		vco_rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000;
	else if (hsclk_div == 3)
		vco_rate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
	else
		vco_rate = DP_VCO_HSCLK_RATE_5400MHZDIV1000;

	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(dp_res, false);

	dp_res->vco_cached_rate = vco->rate = vco_rate;
	return (unsigned long)vco_rate;
}

long dp_vco_round_rate_14nm(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	unsigned long rrate = rate;
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);

	if (rate <= vco->min_rate)
		rrate = vco->min_rate;
	else if (rate <= DP_VCO_HSCLK_RATE_2700MHZDIV1000)
		rrate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
	else
		rrate = vco->max_rate;

	pr_debug("rrate=%ld\n", rrate);

	*parent_rate = rrate;
	return rrate;
}

int dp_pll_clock_register_14nm(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP, i = 0;
	struct clk_onecell_data *clk_data;
	struct clk *clk;
	struct regmap *regmap;
	int num_clks = ARRAY_SIZE(mdss_dp_pllcc_14nm);

	clk_data = devm_kzalloc(&pdev->dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kcalloc(&pdev->dev, num_clks,
				sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	pll_res->priv = &dp_pdb;
	dp_pdb.pll = pll_res;

	/* Set client data for vco, mux and div clocks */
	regmap = devm_regmap_init(&pdev->dev, &dp_pixel_mux_regmap_ops,
			pll_res, &dp_pll_14nm_cfg);
	dp_phy_pll_vco_div_clk.clkr.regmap = regmap;
	mux_clk_ops = clk_regmap_mux_closest_ops;
	mux_clk_ops.determine_rate = clk_mux_determine_rate;
	mux_clk_ops.recalc_rate = mux_recalc_rate;

	dp_vco_clk.priv = pll_res;

	for (i = DP_VCO_CLK; i <= DP_PHY_PLL_VCO_DIV_CLK; i++) {
		pr_debug("reg clk: %d index: %d\n", i, pll_res->index);
		clk = devm_clk_register(&pdev->dev,
				mdss_dp_pllcc_14nm[i]);
		if (IS_ERR(clk)) {
			pr_err("clk registration failed for DP: %d\n",
					pll_res->index);
			rc = -EINVAL;
			goto clk_reg_fail;
		}
		clk_data->clks[i] = clk;
	}

	rc = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, clk_data);
	if (rc) {
		pr_err("Clock register failed rc=%d\n", rc);
		rc = -EPROBE_DEFER;
	} else {
		pr_debug("SUCCESS\n");
	}
	return 0;
clk_reg_fail:
	return rc;
}
