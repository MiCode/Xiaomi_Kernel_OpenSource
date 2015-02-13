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

#define VCO_540K	540000
#define VCO_620K	620000
#define VCO_648K	648000

#define MHZ(x)		((x) * 1000000)
#define M2_FRACTION_MASK	(0x3FFFFF)
#define BYTE0 (0xFF)
#define BYTE1 (BYTE0 << 8)
#define BYTE2 (BYTE0 << 16)
#define BYTE3 (BYTE0 << 24)

#define SPARE_1	(1 << 30)
#define SPARE_2	(1 << 31)

struct edp_vswing_preemp {
	u32 deemp;
	u32 transcale;
	u32 downscale;
	u32 deempscale;
};

struct edp_vswing_preemp edp_values[] = {
	{0x02020000, 0x001a9800, 0x041a0000, 0x80800000}, /* 200mV 0db */
	{0x02020000, 0x00269800, 0x04260000, 0x70700000}, /* 200mV 3.5db */
	{0x02020000, 0x00309800, 0x04300000, 0x60600000}, /* 200mV 6db */
	{0x02020000, 0x00369800, 0x04360000, 0x45450000}, /* 200mV 9.5db */
	{0x02020000, 0x00209800, 0x04200000, 0x80800000}, /* 250mV 0db */
	{0x02020000, 0x00309800, 0x04300000, 0x68680000}, /* 250mV 3.5db */
	{0x02020000, 0x00369800, 0x04360000, 0x55550000}, /* 250mV 6db */
	{0x0       , 0x0       , 0x0       , 0x0       }, /* invalid */
	{0x02020000, 0x00269800, 0x04260000, 0x80800000}, /* 300mV 0db */
	{0x02020000, 0x00369800, 0x04360000, 0x65650000}, /* 300mV 3.5db */
	{0x0       , 0x0       , 0x0       , 0x0       }, /* invalid */
	{0x0       , 0x0       , 0x0       , 0x0       }, /* invalid */
	{0x02020000, 0x00309800, 0x04300000, 0x80800000}, /* 350mV 0db */
	{0x0       , 0x0       , 0x0       , 0x0       }, /* invalid */
	{0x0       , 0x0       , 0x0       , 0x0       }, /* invalid */
	{0x0       , 0x0       , 0x0       , 0x0       }, /* invalid */
};

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

u32 hdmi_swing_values[2][5] = {
	{0x2020000, 0x809800, 0x4800000, 0x80800000, 0},
	{0x2020000, 0xa09800, 0x4a00000, 0x60600000, 0x80800000},
};

/* Physical Access Coding Sub-Layer register values for differnt frequencies */
u32 ps0_values[5] = {0x61f42, 0x61f44, 0x61f47, 0x61f4d, 0x61f58};
u32 ps1_values[5] = {0x571f42, 0x571f44, 0x571f47, 0x571f4d, 0x571f58};

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
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
	enum port port_id = PORT_A;

	switch (pipeline->type) {
	case INTEL_PIPE_DSI:
		dsi_pipe = &pipeline->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port, dsi_ctx->ports)
			port_id = pipeline->port.dsi_port[port].port_id;
		break;
	case INTEL_PIPE_HDMI:
		port_id = pipeline->port.hdmi_port.port_id;
		break;
	case INTEL_PIPE_DP:
	case INTEL_PIPE_EDP:
		port_id = pipeline->port.dp_port.port_id;
		break;
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
	enum dpio_channel ch = vlv_pipe_to_channel(disp->pipe.pipe_id);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	u32 val, temp;

	mutex_lock(&config->dpio_lock);

	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW14(ch));
	val &= ~(DPIO_DCLKP_EN);
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW14(ch), val);

	temp = ((clock->p1 & 0x7) << 5) | (clock->p2 & 0x1f);
	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW13(ch));
	val &= ~(BYTE1);
	val |= ((temp & BYTE0) << DPIO_CHV_P2_DIV_SHIFT);
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW13(ch), val);

	temp = (clock->m2 >> 24);
	val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW0(ch));
	val = (val  & (~BYTE0)) | temp;
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW0(ch), val);

	temp = (clock->n & BYTE0) << DPIO_CHV_N_DIV_SHIFT;
	val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW1(ch));
	val = (val  & (~BYTE1)) | temp;
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW1(ch), val);

	if (clock->m2 & M2_FRACTION_MASK) {
		val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW2(ch));
		val = (val & BYTE3) | (clock->m2 & M2_FRACTION_MASK);
		vlv_dpio_write(disp->dpio_id, CHV_PLL_DW2(ch), val);

		val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW3(ch));
		val  |= (DPIO_CHV_FRAC_DIV_EN);
		vlv_dpio_write(disp->dpio_id, CHV_PLL_DW3(ch), val);

		val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW9(ch));
		val = ((val & (0xFFFFFFF0)) | (0x5 << 1));
		vlv_dpio_write(disp->dpio_id, CHV_PLL_DW9(ch), val);

	} else {
		val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW3(ch));
		val  &= ~(DPIO_CHV_FRAC_DIV_EN);
		vlv_dpio_write(disp->dpio_id, CHV_PLL_DW3(ch), val);

		val = vlv_dpio_read(disp->dpio_id, VLV_PLL_DW9(ch));
		val = ((val & (0xFFFFFFF0)) | (0x5 << 1) | 1);
		vlv_dpio_write(disp->dpio_id, VLV_PLL_DW9(ch), val);
	}

	if (clock->vco == VCO_540K)
		temp = 0x10803;
	else if (clock->vco <= VCO_620K)
		temp = 0x30B05;
	else if (clock->vco <= VCO_648K)
		temp = 0x30904;
	val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW6(ch));
	val = (val & BYTE3) | temp;
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW6(ch), val);

	val = vlv_dpio_read(disp->dpio_id, CHV_PLL_DW8(ch));
	val &= ~(BYTE0);
	if (clock->vco <= VCO_620K)
		val = val | 0x9;
	else if (clock->vco <= VCO_648K)
		val = val | 0x8;
	vlv_dpio_write(disp->dpio_id, CHV_PLL_DW8(ch), val);

	val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW14(ch));
	val  |= (DPIO_DCLKP_EN | DPIO_AFC_RECAL);
	vlv_dpio_write(disp->dpio_id, CHV_CMN_DW14(ch), val);

	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_enable_staggering(struct intel_pipeline *pipeline, u32 dotclock)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	enum dpio_channel ch = vlv_disp_to_port(disp);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	u32 pcs0 = 0, pcs1 = 0;
	u32 pcs0_offset = 0, pcs1_offset = 0;
	u32 tx0 = 0, tx1 = 0, tx2 = 0;

	pcs0_offset = VLV_PCS01_DW12(ch);
	pcs1_offset = VLV_PCS23_DW12(ch);

	tx0 = CHV_TX0_DW15(ch);
	tx1 = CHV_TX2_DW15(ch);
	tx2 = CHV_TX3_DW15(ch);

	if (dotclock >= MHZ(25) && dotclock <= MHZ(33)) {
		pcs0 = ps0_values[0];
		pcs1 = ps1_values[0];
	} else if (dotclock <= MHZ(67)) {
		pcs0 = ps0_values[1];
		pcs1 = ps1_values[1];
	} else if (dotclock <= MHZ(135)) {
		pcs0 = ps0_values[2];
		pcs1 = ps1_values[2];
	} else if (dotclock <= MHZ(270)) {
		pcs0 = ps0_values[3];
		pcs1 = ps1_values[3];
	} else if (dotclock <= MHZ(540)) {
		pcs0 = ps0_values[4];
		pcs1 = ps1_values[4];
	}

	mutex_lock(&config->dpio_lock);

	vlv_dpio_write(disp->dpio_id, VLV_PCS01_DW12(ch), pcs0);
	vlv_dpio_write(disp->dpio_id, VLV_PCS23_DW12(ch), pcs1);

	/* skew */
	vlv_dpio_write(disp->dpio_id, tx0, SPARE_1);
	vlv_dpio_write(disp->dpio_id, tx1, SPARE_1);
	vlv_dpio_write(disp->dpio_id, tx2, SPARE_1);

	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_update_channel(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	enum dpio_channel ch = vlv_disp_to_port(disp);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	u32 val = 0;
	u32 port = 0;
	u32 pll = disp->pll.pll_id;

	if (disp->type == INTEL_PIPE_HDMI)
		port = disp->port.hdmi_port.port_id;
	else
		port = disp->port.dp_port.port_id;

	mutex_lock(&config->dpio_lock);

	/* Need to program for cross linking alone, otherwise use default */
	if ((port == PORT_B) && (pll == PLL_B)) {
		val = vlv_dpio_read(disp->dpio_id, VLV_PCS_DW8(ch));
		val = (val  & (~BYTE2)) | (0x30 << 16);
		vlv_dpio_write(disp->dpio_id, VLV_PCS_DW8(ch), val);

		val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW19(ch));
		val = (val  & (~BYTE1)) | (0xA3 << 8);
		vlv_dpio_write(disp->dpio_id, CHV_CMN_DW19(ch), val);

	} else if ((port == PORT_C) && (pll == PLL_A)) {
		val = vlv_dpio_read(disp->dpio_id, VLV_PCS_DW8(ch));
		val = (val  & (~BYTE2)) | (0x10 << 16);
		vlv_dpio_write(disp->dpio_id, VLV_PCS_DW8(ch), val);

		val = vlv_dpio_read(disp->dpio_id, CHV_CMN_DW19(ch));
		val = (val  & (~BYTE1)) | (0x83 << 8);
		vlv_dpio_write(disp->dpio_id, CHV_CMN_DW19(ch), val);
	}

	/*
	 * Default values should be enough
	 * assuming phy was ungated for other combinations
	 */
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

void chv_dpio_signal_levels(struct intel_pipeline *pipeline,
	u32 margin, u32 deemp)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);
	u32 trigreg;
	u32 reglist[3] = {0};
	u32 *vallist;

	trigreg = CHV_PCS_DW10(ch);
	reglist[0] = VLV_TX_DW2(ch);
	reglist[1] = VLV_TX_DW3(ch);
	reglist[2] = VLV_TX_DW4(ch);

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

void chv_dpio_edp_signal_levels(struct intel_pipeline *pipeline, u32 v, u32 p)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);
	u32 reg_list[3] = {0};
	u32 val_list[3] = {0};
	u32 trigreg = 0;
	u32 i = 0;
	u32 count = 0;
	trigreg = CHV_PCS_DW10(ch);
	reg_list[0] = VLV_TX_DW2(ch);
	reg_list[1] = VLV_TX_DW3(ch);
	reg_list[2] = VLV_TX_DW4(ch);

	/* select the appropriate row */
	count = 4 * v + p;

	val_list[0] = edp_values[count].transcale;
	val_list[1] = edp_values[count].downscale;
	val_list[2] = edp_values[count].deempscale;

	mutex_lock(&config->dpio_lock);

	vlv_dpio_write(disp->dpio_id, trigreg, edp_values[count].deemp);

	for (i = 0; i < 3; i++)
		vlv_dpio_write(disp->dpio_id, reg_list[i], val_list[i]);

	vlv_dpio_write(disp->dpio_id, trigreg, 0);
	mutex_unlock(&config->dpio_lock);
}

void chv_dpio_hdmi_swing_levels(struct intel_pipeline *pipeline, u32 dotclock)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dc_config *config = get_vlv_dc_config(disp);
	enum dpio_channel ch = vlv_disp_to_port(disp);
	u32 deemp, transscale, preemp, vswing, tx3clock;

	if (dotclock <= MHZ(162)) {
		deemp = hdmi_swing_values[0][0];
		transscale = hdmi_swing_values[0][1];
		preemp = hdmi_swing_values[0][2];
		vswing = hdmi_swing_values[0][3];
		tx3clock = hdmi_swing_values[0][4];
	} else {
		deemp = hdmi_swing_values[1][0];
		transscale = hdmi_swing_values[1][1];
		preemp = hdmi_swing_values[1][2];
		vswing = hdmi_swing_values[1][3];
		tx3clock = hdmi_swing_values[1][4];
	}

	mutex_lock(&config->dpio_lock);

	vlv_dpio_write(disp->dpio_id, CHV_PCS_DW10(ch), deemp);
	vlv_dpio_write(disp->dpio_id, VLV_TX_DW2(ch), transscale);
	vlv_dpio_write(disp->dpio_id, VLV_TX_DW3(ch), preemp);
	vlv_dpio_write(disp->dpio_id, VLV_TX_DW4(ch), vswing);

	if (dotclock > MHZ(162))
		vlv_dpio_write(disp->dpio_id, VLV_TX3_DW4(ch), tx3clock);

	vlv_dpio_write(disp->dpio_id, CHV_PCS_DW10(ch), 0);

	mutex_unlock(&config->dpio_lock);
}
