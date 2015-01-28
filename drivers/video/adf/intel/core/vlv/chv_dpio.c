/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created on 15 Dec 2014
 * Author: Sivakumar Thulasimani <sivakumar.thulasimani@intel.com>
 */

#include <drm/drmP.h>
#include <core/vlv/dpio.h>

enum dpio_channel {
	DPIO_CH0,
	DPIO_CH1,
};

u32 dpio_signal_levels[4][4][5] = {
	{
		{0x00000000, 0x00349800, 0x04340000, 0x80800000, 0x02020000},
		{0x02020000, 0x004d9800, 0x044d0000, 0x80800000, 0x00000000},
		{0x00000000, 0x00669800, 0x04660000, 0x80800000, 0x02020000},
		{0x02020000, 0x009a9800, 0x049a0000, 0x80800000, 0x00000000}
	},
	{
		{0x00000000, 0x004e9800, 0x044e0000, 0x55550000, 0x02020000},
		{0x02020000, 0x00749800, 0x04740000, 0x55550000, 0x00000000},
		{0x00000000, 0x009a9800, 0x049a0000, 0x55550000, 0x02020000}
	},
	{
		{0x02020000, 0x00689800, 0x04680000, 0x40400000, 0x00000000},
		{0x00000000, 0x009a9800, 0x049a0000, 0x39390000, 0x02020000}
	},
	{
		{0x00000000, 0x009a9800, 0x049a0000, 0x2b2b0000, 0x02020000}
	}
};

static inline struct vlv_dc_config *get_vlv_dc_config(
	struct vlv_pipeline *disp)
{
	/* find the first pipeline object */
	while (disp->disp_no != 0)
		disp--;

	return container_of(disp, struct vlv_dc_config, pipeline[0]);
}

int vlv_disp_to_port(struct vlv_pipeline *pipeline)
{
	enum port port_id;

	switch (pipeline->type) {
	case INTEL_PIPE_DSI:
		port_id = pipeline->port.dsi_port.port_id;
		break;
	case INTEL_PIPE_HDMI:
		port_id = pipeline->port.hdmi_port.port_id;
		break;
	case INTEL_PIPE_DP:
	case INTEL_PIPE_EDP:
	default:
		BUG();
		return -EINVAL;
	}

	pr_info("ADF: %s port=%d", __func__, (int)port_id);

	switch (port_id) {
	case PORT_B:
	case PORT_D:
		return DPIO_CH0;
	case PORT_C:
		return DPIO_CH1;
	default:
		pr_err("%s: invalid port passed for channel retrieval\n",
			__func__);
		BUG();
		break;
	}

	/* should never reach here */
	return -EINVAL;
}

int vlv_pipe_to_channel(enum pipe pipe)
{
	switch (pipe) {
	case PIPE_A:
	case PIPE_C:
		return DPIO_CH0;
	case PIPE_B:
		return DPIO_CH1;
	default:
		pr_err("%s: invalid pipe passed for channel retrieval\n",
			__func__);
		BUG();
	}
}

void chv_dpio_update_clock(struct intel_pipeline *pipeline,
		struct intel_clock *clock)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	enum dpio_channel port = vlv_disp_to_port(disp);
	u32 bestn, bestm1, bestm2, bestp1, bestp2, bestm2_frac;
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	u32  loopfilter;
	u32 old_val = 0, val = 0;
	bestn = clock->n;
	bestm1 = clock->m1;
	bestm2 = clock->m2 >> 22;
	bestm2_frac = clock->m2 & 0x3fffff;
	bestp1 = clock->p1;
	bestp2 = clock->p2;

	mutex_lock(&config->dpio_lock);

	/* p1 and p2 divider */
	val = 5 << DPIO_CHV_S1_DIV_SHIFT |
		bestp1 << DPIO_CHV_P1_DIV_SHIFT |
		bestp2 << DPIO_CHV_P2_DIV_SHIFT |
		1 << DPIO_CHV_K_DIV_SHIFT;
	old_val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW13(port));
	pr_err("old = %x new = %x\n", old_val, val);
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW13(port), val);

	/* Feedback post-divider - m2 */
	old_val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW0(port));
	pr_err("old = %x new = %x\n", old_val, bestm2);
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW0(port), bestm2);

	/* Feedback refclk divider - n and m1 */
	val = DPIO_CHV_M1_DIV_BY_2 |
		1 << DPIO_CHV_N_DIV_SHIFT;
	old_val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW1(port));
	pr_err("old = %x new = %x\n", old_val, val);
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW1(port), val);

	/* M2 fraction division */
	old_val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW2(port));
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW2(port), bestm2_frac);
	pr_err("old = %x new = %x\n", old_val, bestm2_frac);

	/* M2 fraction division enable */
	val = DPIO_CHV_FRAC_DIV_EN |
		(2 << DPIO_CHV_FEEDFWD_GAIN_SHIFT);
	old_val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW3(port));
	pr_err("old = %x new = %x\n", old_val, val);
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW3(port), val);

	/* Loop filter */
	loopfilter = 5 << DPIO_CHV_PROP_COEFF_SHIFT |
		2 << DPIO_CHV_GAIN_CTRL_SHIFT;

	loopfilter |= 11 << DPIO_CHV_INT_COEFF_SHIFT;
	old_val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW6(port));
	pr_err("old = %x new = %x\n", old_val, val);
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW6(port), loopfilter);

	/* AFC Recal */
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW14(port),
			vlv_dpio_read(disp->dpio_id, CHV_CMN_DW14(port)) |
			DPIO_AFC_RECAL);

	mutex_unlock(&config->dpio_lock);
}


void chv_dpio_update_channel(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);
	u32 val = 0;
	mutex_lock(&config->dpio_lock);

	/* program left/right clock distribution */
	if (disp->pipe.pipe_id != PIPE_B) {
		val = vlv_dpio_read(disp->dpio_id, _CHV_CMN_DW5_CH0);
		val &= ~(CHV_BUFLEFTENA1_MASK | CHV_BUFRIGHTENA1_MASK);
		if (ch == DPIO_CH0)
			val |= CHV_BUFLEFTENA1_FORCE;
		if (ch == DPIO_CH1)
			val |= CHV_BUFRIGHTENA1_FORCE;
		vlv_dpio_write(disp->dpio_id, _CHV_CMN_DW5_CH0, val);
	} else {
		val = vlv_dpio_read(disp->dpio_id, _CHV_CMN_DW1_CH1);
		val &= ~(CHV_BUFLEFTENA2_MASK | CHV_BUFRIGHTENA2_MASK);
		if (ch == DPIO_CH0)
			val |= CHV_BUFLEFTENA2_FORCE;
		if (ch == DPIO_CH1)
			val |= CHV_BUFRIGHTENA2_FORCE;
		vlv_dpio_write(disp->dpio_id, _CHV_CMN_DW1_CH1, val);
	}

	/* program clock channel usage */
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW8(ch));
	val |= CHV_PCS_USEDCLKCHANNEL_OVRRIDE;
	if (disp->pipe.pipe_id != PIPE_B)
		val &= ~CHV_PCS_USEDCLKCHANNEL;
	else
		val |= CHV_PCS_USEDCLKCHANNEL;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW8(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW8(ch));
	val |= CHV_PCS_USEDCLKCHANNEL_OVRRIDE;
	if (disp->pipe.pipe_id != PIPE_B)
		val &= ~CHV_PCS_USEDCLKCHANNEL;
	else
		val |= CHV_PCS_USEDCLKCHANNEL;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW8(ch), val);

	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW19(ch));
	if (disp->pipe.pipe_id != PIPE_B)
		val &= ~CHV_CMN_USEDCLKCHANNEL;
	else
		val |= CHV_CMN_USEDCLKCHANNEL;
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW19(ch), val);

	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW14(ch));
	val |= DPIO_DCLKP_EN;
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW14(ch), val);

	mutex_unlock(&config->dpio_lock);

	udelay(1);
}

void chv_dpio_pre_port_enable(struct intel_pipeline *pipeline)
{
	int i = 0, val = 0, data = 0;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);

	mutex_lock(&config->dpio_lock);

	/* allow hardware to manage TX FIFO reset source */
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW11(ch));
	val &= ~DPIO_LANEDESKEW_STRAP_OVRD;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW11(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW11(ch));
	val &= ~DPIO_LANEDESKEW_STRAP_OVRD;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW11(ch), val);

	/* Deassert soft data lane reset*/
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW1(ch));
	val |= CHV_PCS_REQ_SOFTRESET_EN;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW1(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW1(ch));
	val |= CHV_PCS_REQ_SOFTRESET_EN;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW1(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW0(ch));
	val |= (DPIO_PCS_TX_LANE2_RESET | DPIO_PCS_TX_LANE1_RESET);
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW0(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW0(ch));
	val |= (DPIO_PCS_TX_LANE2_RESET | DPIO_PCS_TX_LANE1_RESET);
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW0(ch), val);

	/* Program Tx lane latency optimal setting*/
	for (i = 0; i < 4; i++) {
		/* Set the latency optimal bit */
		data = (i == 1) ? 0x0 : 0x6;
		vlv_dpio_write(disp->dpio_id, CHV_TX_DW11(ch, i),
				data << DPIO_FRC_LATENCY_SHFIT);

		/* Set the upar bit */
		data = (i == 1) ? 0x0 : 0x1;
		vlv_dpio_write(disp->dpio_id, CHV_TX_DW14(ch, i),
				data << DPIO_UPAR_SHIFT);
	}

	if (disp->type != INTEL_PIPE_HDMI) {
		mutex_unlock(&config->dpio_lock);
		return;
	}

	/* FIXME: Fix up value only after power analysis */

	/* Data lane stagger programming */

	/* Clear calc init */
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW10(ch));
	val &= ~(DPIO_PCS_SWING_CALC_TX0_TX2 | DPIO_PCS_SWING_CALC_TX1_TX3);
	val &= ~(DPIO_PCS_TX1DEEMP_MASK | DPIO_PCS_TX2DEEMP_MASK);
	val |= DPIO_PCS_TX1DEEMP_9P5 | DPIO_PCS_TX2DEEMP_9P5;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW10(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW10(ch));
	val &= ~(DPIO_PCS_SWING_CALC_TX0_TX2 | DPIO_PCS_SWING_CALC_TX1_TX3);
	val &= ~(DPIO_PCS_TX1DEEMP_MASK | DPIO_PCS_TX2DEEMP_MASK);
	val |= DPIO_PCS_TX1DEEMP_9P5 | DPIO_PCS_TX2DEEMP_9P5;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW10(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW9(ch));
	val &= ~(DPIO_PCS_TX1MARGIN_MASK | DPIO_PCS_TX2MARGIN_MASK);
	val |= DPIO_PCS_TX1MARGIN_000 | DPIO_PCS_TX2MARGIN_000;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW9(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW9(ch));
	val &= ~(DPIO_PCS_TX1MARGIN_MASK | DPIO_PCS_TX2MARGIN_MASK);
	val |= DPIO_PCS_TX1MARGIN_000 | DPIO_PCS_TX2MARGIN_000;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW9(ch), val);

	/* FIXME: Program the support xxx V-dB */
	/* Use 800mV-0dB */
	for (i = 0; i < 4; i++) {
		val = vlv_dpio_read(disp->dpio_id, CHV_TX_DW4(ch, i));
		val &= ~DPIO_SWING_DEEMPH9P5_MASK;
		val |= 128 << DPIO_SWING_DEEMPH9P5_SHIFT;
		vlv_dpio_write(disp->dpio_id, CHV_TX_DW4(ch, i), val);
	}

	for (i = 0; i < 4; i++) {
		val = vlv_dpio_read(disp->dpio_id, CHV_TX_DW2(ch, i));
		val &= ~DPIO_SWING_MARGIN000_MASK;
		val |= 102 << DPIO_SWING_MARGIN000_SHIFT;
		vlv_dpio_write(disp->dpio_id, CHV_TX_DW2(ch, i), val);
	}

	/* Disable unique transition scale */
	for (i = 0; i < 4; i++) {
		val = vlv_dpio_read(disp->dpio_id, CHV_TX_DW3(ch, i));
		val &= ~DPIO_TX_UNIQ_TRANS_SCALE_EN;
		vlv_dpio_write(disp->dpio_id, CHV_TX_DW3(ch, i), val);
	}

	/* Additional steps for 1200mV-0dB */
	/* Start swing calculation */
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW10(ch));
	val |= DPIO_PCS_SWING_CALC_TX0_TX2 | DPIO_PCS_SWING_CALC_TX1_TX3;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW10(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW10(ch));
	val |= DPIO_PCS_SWING_CALC_TX0_TX2 | DPIO_PCS_SWING_CALC_TX1_TX3;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW10(ch), val);

	/* LRC Bypass */
	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW30);
	val |= DPIO_LRC_BYPASS;
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW30, val);

	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_lane_reset_en(struct intel_pipeline *pipeline, bool enable)
{
	int val = 0, add = 0;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);

	mutex_lock(&config->dpio_lock);

	/* Propagate soft reset to data lane reset */
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW1(ch));
	val |= (CHV_PCS_REQ_SOFTRESET_EN | DPIO_PCS_CLK_SOFT_RESET);
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW1(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW1(ch));
	val |= (CHV_PCS_REQ_SOFTRESET_EN | DPIO_PCS_CLK_SOFT_RESET);
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW1(ch), val);

	if (enable)
		add = (DPIO_PCS_TX_LANE2_RESET | DPIO_PCS_TX_LANE1_RESET);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW0(ch));
	val  = add;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW0(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW0(ch));
	val = add;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW0(ch), val);

	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_post_disable(struct intel_pipeline *pipeline)
{
	int val = 0;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);

	mutex_lock(&config->dpio_lock);

	/* Propagate soft reset to data lane reset */
	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW1(ch));
	val |= CHV_PCS_REQ_SOFTRESET_EN;
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW1(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW1(ch));
	val |= CHV_PCS_REQ_SOFTRESET_EN;
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW1(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS01_DW0(ch));
	val &= ~(DPIO_PCS_TX_LANE2_RESET | DPIO_PCS_TX_LANE1_RESET);
	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW0(ch), val);

	val = vlv_dpio_read(disp->dpio_id, VLV_PCS23_DW0(ch));
	val &= ~(DPIO_PCS_TX_LANE2_RESET | DPIO_PCS_TX_LANE1_RESET);
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW0(ch), val);

	mutex_unlock(&config->dpio_lock);
}


void chv_dpio_post_pll_disable(struct intel_pipeline *pipeline)
{
	int val = 0;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);

	mutex_lock(&config->dpio_lock);

	/* Disable 10bit clock to display controller */
	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW14(ch));
	val &= ~DPIO_DCLKP_EN;
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW14(ch), val);

	/* disable left/right clock distribution */
	if (disp->pipe.pipe_id != PIPE_B) {
		val = vlv_dpio_read(disp->dpio_id, _CHV_CMN_DW5_CH0);
		val &= ~(CHV_BUFLEFTENA1_MASK | CHV_BUFRIGHTENA1_MASK);
		vlv_dpio_write(disp->dpio_id, _CHV_CMN_DW5_CH0, val);
	} else {
		val = vlv_dpio_read(disp->dpio_id, _CHV_CMN_DW1_CH1);
		val &= ~(CHV_BUFLEFTENA2_MASK | CHV_BUFRIGHTENA2_MASK);
		vlv_dpio_write(disp->dpio_id, _CHV_CMN_DW1_CH1, val);
	}

	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_lane_reset(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);

	mutex_lock(&config->dpio_lock);

	vlv_dpio_write(disp->dpio_id, 0x8200, 0x00000000);
	vlv_dpio_write(disp->dpio_id, 0x8204, 0x00e00060);

	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_signal_levels(struct intel_pipeline *pipeline,
	u32 deemp, u32 margin)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);

	u32 trigreg = 0x8228;
	u32 reglist[3] = {0};
	u32 *vallist;
	reglist[0] = 0x8288;
	reglist[1] = 0x828C;
	reglist[2] = 0x8290;

	vallist = dpio_signal_levels[deemp][margin];

	mutex_lock(&config->dpio_lock);

	vlv_dpio_write(disp->dpio_id, trigreg, vallist[0]);
	vlv_dpio_write(disp->dpio_id, reglist[0], vallist[1]);
	vlv_dpio_write(disp->dpio_id, reglist[1], vallist[2]);
	vlv_dpio_write(disp->dpio_id, reglist[2], vallist[3]);
	vlv_dpio_write(disp->dpio_id, trigreg, vallist[4]);

	mutex_unlock(&config->dpio_lock);
}

void vlv_dpio_signal_levels(struct intel_pipeline *pipeline,
	u32 deemp, u32 margin)
{
	/* FIXME: to be implemetned */
	BUG();
}
