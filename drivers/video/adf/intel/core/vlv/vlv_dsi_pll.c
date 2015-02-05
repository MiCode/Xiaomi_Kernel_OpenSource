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
 */

#include <drm/drmP.h>
#include <intel_adf_device.h>
#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pll.h>


#define DSI_HSS_PACKET_SIZE		4
#define DSI_HSE_PACKET_SIZE		4
#define DSI_HSA_PACKET_EXTRA_SIZE	6
#define DSI_HBP_PACKET_EXTRA_SIZE	6
#define DSI_HACTIVE_PACKET_EXTRA_SIZE	6
#define DSI_HFP_PACKET_EXTRA_SIZE	6
#define DSI_EOTP_PACKET_SIZE		4

#define DSI_DRRS_PLL_CONFIG_TIMEOUT_MS	100

static const u32 lfsr_converts[] = {
	426, 469, 234, 373, 442, 221, 110, 311, 411,		/* 62 - 70 */
	461, 486, 243, 377, 188, 350, 175, 343, 427, 213,	/* 71 - 80 */
	106, 53, 282, 397, 454, 227, 113, 56, 284, 142,		/* 81 - 90 */
	71, 35, 273, 136, 324, 418, 465, 488, 500, 506		/* 91 - 100 */
};

/* Get DSI clock from pixel clock */
static u32 vlv_dsi_pll_clk_from_pclk(u32 pclk, int pixel_format, int lane_count)
{
	u32 dsi_clk_khz;
	u32 bpp;

	switch (pixel_format) {
	default:
	case VID_MODE_FORMAT_RGB888:
	case VID_MODE_FORMAT_RGB666_LOOSE:
		bpp = 24;
		break;
	case VID_MODE_FORMAT_RGB666:
		bpp = 18;
		break;
	case VID_MODE_FORMAT_RGB565:
		bpp = 16;
		break;
	}

	/*
	 * DSI data rate = pixel clock * bits per pixel / lane count
	 * pixel clock is converted from KHz to Hz
	 */
	dsi_clk_khz = DIV_ROUND_CLOSEST(pclk * bpp, lane_count);

	return dsi_clk_khz;
}

u32 vlv_dsi_pll_calc_mnp(struct vlv_pll *pll, u32 dsi_clk,
		struct dsi_mnp *dsi_mnp)
{
	u32 m, n, p;
	u32 ref_clk;
	u32 error;
	u32 tmp_error;
	int target_dsi_clk;
	int calc_dsi_clk;
	u32 calc_m;
	u32 calc_p;
	u32 m_seed;
	u32 m_start;
	u32 m_limit;
	u32 n_limit;
	u32 p_limit;

	/* dsi_clk is expected in KHZ */
	if (dsi_clk < 300000 || dsi_clk > 1150000) {
		pr_err("DSI CLK Out of Range\n");
		return -ECHRNG;
	}

	ref_clk = 25000;
	m_start = 62;
	m_limit = 92;
	n_limit = 1;
	p_limit = 6;

	target_dsi_clk = dsi_clk;
	error = 0xFFFFFFFF;
	tmp_error = 0xFFFFFFFF;
	calc_m = m_start;
	calc_p = 0;

	for (m = m_start; m <= m_limit; m++) {
		for (p = 2; p <= p_limit; p++) {
			/*
			 * Find the optimal m and p divisors
			 * with minimal error +/- the required clock
			 */
			calc_dsi_clk = (m * ref_clk) / (p * n_limit);
			if (calc_dsi_clk == target_dsi_clk) {
				calc_m = m;
				calc_p = p;
				error = 0;
				break;
			} else
				tmp_error = abs(target_dsi_clk - calc_dsi_clk);

			if (tmp_error < error) {
				error = tmp_error;
				calc_m = m;
				calc_p = p;
			}
		}

		if (error == 0)
			break;
	}

	m_seed = lfsr_converts[calc_m - 62];
	n = n_limit;
	dsi_mnp->dsi_pll_ctrl = 1 << (DSI_PLL_P1_POST_DIV_SHIFT + calc_p - 2);

	dsi_mnp->dsi_pll_div = (n - 1) << DSI_PLL_N1_DIV_SHIFT |
			m_seed << DSI_PLL_M1_DIV_SHIFT;

	return 0;
}

u32 chv_dsi_pll_calc_mnp(struct vlv_pll *pll, u32 dsi_clk,
		struct dsi_mnp *dsi_mnp)
{
	u32 m, n, p;
	u32 ref_clk;
	u32 error;
	u32 tmp_error;
	int target_dsi_clk;
	int calc_dsi_clk;
	u32 calc_m;
	u32 calc_p;
	u32 m_seed;
	u32 m_start;
	u32 m_limit;
	u32 n_limit;
	u32 p_limit;

	/* dsi_clk is expected in KHZ */
	if (dsi_clk < 300000 || dsi_clk > 1150000) {
		pr_err("DSI CLK Out of Range\n");
		return -ECHRNG;
	}

	ref_clk = 100000;
	m_start = 70;
	m_limit = 96;
	n_limit = 4;
	p_limit = 6;

	target_dsi_clk = dsi_clk;
	error = 0xFFFFFFFF;
	tmp_error = 0xFFFFFFFF;
	calc_m = m_start;
	calc_p = 0;

	for (m = m_start; m <= m_limit; m++) {
		for (p = 2; p <= p_limit; p++) {
			/*
			 * Find the optimal m and p divisors
			 * with minimal error +/- the required clock
			 */
			calc_dsi_clk = (m * ref_clk) / (p * n_limit);
			if (calc_dsi_clk == target_dsi_clk) {
				calc_m = m;
				calc_p = p;
				error = 0;
				break;
			} else
				tmp_error = abs(target_dsi_clk - calc_dsi_clk);

			if (tmp_error < error) {
				error = tmp_error;
				calc_m = m;
				calc_p = p;
			}
		}

		if (error == 0)
			break;
	}

	m_seed = lfsr_converts[calc_m - 62];
	n = n_limit;
	dsi_mnp->dsi_pll_ctrl = 1 << (DSI_PLL_P1_POST_DIV_SHIFT + calc_p - 2);

	dsi_mnp->dsi_pll_div = (n/2) << DSI_PLL_N1_DIV_SHIFT |
			m_seed << DSI_PLL_M1_DIV_SHIFT;

	return 0;
}

/*
 * vlv_dsi_pll_reg_configure:
 *	Function to configure the CCK registers for PLL control and dividers
 *
 * pll		: Pll that is getting configure
 * dsi_mnp	: Struct with divider values
 * pll_enable	: Flag to indicate whether it is a fresh pll enable call or
 *		  call on DRRS purpose
 */
static void vlv_dsi_pll_reg_configure(struct vlv_pll *pll,
				struct dsi_mnp *dsi_mnp, bool pll_enable)
{
	struct dsi_config *intel_dsi = pll->config;
	struct dsi_context *dsi_ctx = &intel_dsi->ctx;

	if (pll_enable) {
		vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, 0);

		/* Enable DSI0 pll for DSI Port A & DSI Dual link*/
		if (dsi_ctx->ports & (1 << PORT_A))
			dsi_mnp->dsi_pll_ctrl |= DSI_PLL_CLK_GATE_DSI0_DSIPLL;

		/* Enable DSI1 pll for DSI Port C & DSI Dual link*/
		else if (dsi_ctx->ports & (1 << PORT_C))
			dsi_mnp->dsi_pll_ctrl |= DSI_PLL_CLK_GATE_DSI1_DSIPLL;
	} else {

		/*
		 * Updating the M1, N1, P1 div values alone on the
		 * CCK registers. these new values are abstracted from
		 * the dsi_mnp struction
		 */
		dsi_mnp->dsi_pll_ctrl =
			(dsi_mnp->dsi_pll_ctrl & DSI_PLL_P1_POST_DIV_MASK) |
			(vlv_cck_read(CCK_REG_DSI_PLL_CONTROL) &
			~DSI_PLL_P1_POST_DIV_MASK);
		dsi_mnp->dsi_pll_div = (dsi_mnp->dsi_pll_div &
			(DSI_PLL_M1_DIV_MASK | DSI_PLL_N1_DIV_MASK)) |
			(vlv_cck_read(CCK_REG_DSI_PLL_DIVIDER)
			& ~(DSI_PLL_M1_DIV_MASK | DSI_PLL_N1_DIV_MASK));
	}

	pr_debug("ADF: %s: dsi_pll: div %08x, ctrl %08x\n", __func__,
				dsi_mnp->dsi_pll_div, dsi_mnp->dsi_pll_ctrl);

	vlv_cck_write(CCK_REG_DSI_PLL_DIVIDER, dsi_mnp->dsi_pll_div);
	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, dsi_mnp->dsi_pll_ctrl);

	return;
}

/*
 * vlv_drrs_configure_dsi_pll:
 *	Function to configure the PLL dividers and bring the new values
 * into effect by power cycling the VCO. This power cycle is supposed
 * to be completed within the vblank period. This is software implementation
 * and depends on the CCK register access. Needs to be tested thoroughly.
 *
 * pipeline	: target pipeline
 * dsi_mnp	: struct with pll divider values
 */
int vlv_drrs_configure_dsi_pll(struct intel_pipeline *pipeline,
						struct dsi_mnp *dsi_mnp)
{
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct vlv_pll *pll = &vlv_pipeline->pll;
	u32 dsl_offset = PIPEDSL(pll->config->pipe), dsl, dsl_end;
	u32 vactive, vtotal, vblank, vblank_30_percent, vblank_70_percent;
	unsigned long timeout;

	vlv_dsi_pll_reg_configure(pll, dsi_mnp, false);

	pr_debug("ADF: %s: dsi_mnp:: ctrl: 0x%X, div: 0x%X\n", __func__,
				dsi_mnp->dsi_pll_ctrl, dsi_mnp->dsi_pll_div);

	dsi_mnp->dsi_pll_ctrl &= (~DSI_PLL_VCO_EN);

	vtotal = REG_READ(VTOTAL(pll->config->pipe));
	vactive = (vtotal & VERTICAL_ACTIVE_DISPLAY_MASK);
	vtotal = (vtotal & VERTICAL_TOTAL_DISPLAY_MASK) >>
					VERTICAL_TOTAL_DISPLAY_OFFSET;
	vblank = vtotal - vactive;
	vblank_30_percent = vactive + DIV_ROUND_UP((vblank * 3), 10);
	vblank_70_percent = vactive + DIV_ROUND_UP((vblank * 7), 10);

	timeout = jiffies + msecs_to_jiffies(DSI_DRRS_PLL_CONFIG_TIMEOUT_MS);

tap_vblank_start:
	do {
		if (time_after(jiffies, timeout)) {
			pr_err("ADF: %s: Timeout at waiting for Vblank\n",
								__func__);
			return -ETIMEDOUT;
		}

		dsl = (REG_READ(dsl_offset) & DSL_LINEMASK_GEN3);

	} while (dsl <= vactive || dsl > vblank_30_percent);

	vlv_cck_mutex_acquire(true);

	dsl_end = REG_READ(dsl_offset) & DSL_LINEMASK_GEN3;

	/*
	 * Did we cross Vblank due to delay in mutex acquirement?
	 * Keeping two scanlines in vblank as buffer for ops.
	 */
	if (dsl_end < vactive || dsl_end > vblank_70_percent) {
		vlv_cck_mutex_acquire(false);
		goto tap_vblank_start;
	}

	/* Toggle the VCO_EN to bring in the new dividers values */
	vlv_cck_write_no_lock(CCK_REG_DSI_PLL_CONTROL, dsi_mnp->dsi_pll_ctrl);
	dsi_mnp->dsi_pll_ctrl |= DSI_PLL_VCO_EN;
	vlv_cck_write_no_lock(CCK_REG_DSI_PLL_CONTROL, dsi_mnp->dsi_pll_ctrl);

	dsl_end = REG_READ(dsl_offset) & DSL_LINEMASK_GEN3;

	vlv_cck_mutex_acquire(false);

	if (wait_for(REG_READ(PIPECONF(pll->config->pipe)) &
					PIPECONF_DSI_PLL_LOCKED, 20)) {
		pr_err("ADF: %s: DSI PLL lock failed\n", __func__);
		return -1;
	}

	pr_debug("ADF: %s: PLL Changed between DSL: %u, %u\n", __func__, dsl,
								dsl_end);
	pr_debug("ADF: %s: DSI PLL locked\n", __func__);
	return 0;
}

/*
 * vlv_dsi_mnp_calculate_for_pclk:
 *	calculates the dsi_mnp values for a given pclk
 *
 * pipeline	: Target pipeline
 * dsi_mnp	: output struct to store divider values
 * pclk		: Input mode clock
 */
int vlv_dsi_mnp_calculate_for_pclk(struct intel_pipeline *pipeline,
				struct dsi_mnp *dsi_mnp, unsigned int pclk)
{
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct vlv_pll *pll = &vlv_pipeline->pll;
	struct dsi_config *intel_dsi = pll->config;
	u32 dsi_clk, ret;

	dsi_clk = vlv_dsi_pll_clk_from_pclk(pclk,
		intel_dsi->ctx.pixel_format, intel_dsi->ctx.lane_count);

	pr_debug("ADF: %s: pclk: %u, dsi_clk: %u\n", __func__, pclk, dsi_clk);

	if (IS_CHERRYVIEW())
		ret = chv_dsi_pll_calc_mnp(pll, dsi_clk, dsi_mnp);
	else
		ret = vlv_dsi_pll_calc_mnp(pll, dsi_clk, dsi_mnp);

	if (ret)
		pr_info("ADF: %s: dsi_calc_mnp failed\n", __func__);
	else
		pr_debug("ADF: %s: dsi_mnp: ctrl : 0x%X, div : 0x%X\n",
					__func__, dsi_mnp->dsi_pll_ctrl,
							dsi_mnp->dsi_pll_div);
	return ret;
}

/*
 * XXX: The muxing and gating is hard coded for now. Need to add support for
 * sharing PLLs with two DSI outputs.
 */
static void vlv_dsi_pll_configure(struct vlv_pll *pll)
{
	u32 ret;
	struct dsi_config *intel_dsi = pll->config;
	struct dsi_mnp dsi_mnp;
	u32 dsi_clk;

	dsi_clk = vlv_dsi_pll_clk_from_pclk(intel_dsi->ctx.pclk,
		intel_dsi->ctx.pixel_format, intel_dsi->ctx.lane_count);

	if (IS_CHERRYVIEW())
		ret = chv_dsi_pll_calc_mnp(pll, dsi_clk, &dsi_mnp);
	else
		ret = vlv_dsi_pll_calc_mnp(pll, dsi_clk, &dsi_mnp);

	if (ret) {
		pr_info("dsi_calc_mnp failed\n");
		return;
	}

	vlv_dsi_pll_reg_configure(pll, &dsi_mnp, true);
	return;
}

u32 vlv_dsi_pll_enable(struct vlv_pll *pll,
		struct drm_mode_modeinfo *mode)
{
	u32 val;
	u32 temp = 0;

	/* Disable DPOunit clock gating, can stall pipe */
	val = REG_READ(pll->offset);
	val |= DPLL_RESERVED_BIT;
	REG_WRITE(pll->offset, val);

	val = REG_READ(DSPCLK_GATE_D);
	val |= VSUNIT_CLOCK_GATE_DISABLE;
	val |= DPOUNIT_CLOCK_GATE_DISABLE;
	REG_WRITE(DSPCLK_GATE_D, val);

	vlv_dsi_pll_configure(pll);

	/* wait at least 0.5 us after ungating before enabling VCO */
	usleep_range(1, 10);

	val = vlv_cck_read(CCK_REG_DSI_PLL_CONTROL);
	val |= DSI_PLL_VCO_EN;

	temp = REG_READ(pll->offset);
	temp |= DPLL_REFA_CLK_ENABLE_VLV;
	REG_WRITE(pll->offset, temp);
	udelay(1000);

	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, val);

	pr_info("DSI PLL locked\n");
	return 0;

}

u32 vlv_dsi_pll_disable(struct vlv_pll *pll)
{
	u32 val;

	/* program the register values */
	val = vlv_cck_read(CCK_REG_DSI_PLL_CONTROL);
	val &= ~DSI_PLL_VCO_EN;
	val |= DSI_PLL_LDO_GATE;
	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, val);

	val = REG_READ(DSPCLK_GATE_D);
	val &= ~DPOUNIT_CLOCK_GATE_DISABLE;
	REG_WRITE(DSPCLK_GATE_D, val);
	return 0;

}

bool vlv_dsi_pll_init(struct vlv_pll *pll, enum pipe pipe_id, enum port port_id)
{
	/* init dsi clock */
	pll->assigned = true;

	pll->offset = DPLL(pipe_id);
	pll->port_id = port_id;
	pll->pll_id = (enum pll) pipe_id;
	return true;
}
