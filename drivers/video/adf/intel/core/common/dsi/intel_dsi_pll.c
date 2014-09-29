/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Shobhit Kumar <shobhit.kumar@intel.com>
 *	Yogesh Mohan Marimuthu <yogesh.mohan.marimuthu@intel.com>
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include <drm/i915_adf_wrapper.h>
#include <intel_adf_device.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include "intel_dsi.h"

#define DSI_HSS_PACKET_SIZE		4
#define DSI_HSE_PACKET_SIZE		4
#define DSI_HSA_PACKET_EXTRA_SIZE	6
#define DSI_HBP_PACKET_EXTRA_SIZE	6
#define DSI_HACTIVE_PACKET_EXTRA_SIZE	6
#define DSI_HFP_PACKET_EXTRA_SIZE	6
#define DSI_EOTP_PACKET_SIZE		4

struct dsi_mnp {
	u32 dsi_pll_ctrl;
	u32 dsi_pll_div;
};

static const u32 lfsr_converts[] = {
	426, 469, 234, 373, 442, 221, 110, 311, 411,		/* 62 - 70 */
	461, 486, 243, 377, 188, 350, 175, 343, 427, 213,	/* 71 - 80 */
	106, 53, 282, 397, 354, 227, 113, 56, 284, 142,		/* 81 - 90 */
	71, 35							/* 91 - 92 */
};

/* Get DSI clock from pixel clock */
static u32 dsi_clk_from_pclk(u32 pclk, int pixel_format, int lane_count)
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

	/* DSI data rate = pixel clock * bits per pixel / lane count
	   pixel clock is converted from KHz to Hz */
	dsi_clk_khz = DIV_ROUND_CLOSEST(pclk * bpp, lane_count);

	return dsi_clk_khz;
}

static int dsi_calc_mnp(u32 dsi_clk, struct dsi_mnp *dsi_mnp)
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

	/* dsi_clk is expected in KHZ */
	if (dsi_clk < 300000 || dsi_clk > 1150000) {
		pr_err("DSI CLK Out of Range\n");
		return -ECHRNG;
	}

	ref_clk = 25000;
	target_dsi_clk = dsi_clk;
	error = 0xFFFFFFFF;
	tmp_error = 0xFFFFFFFF;
	calc_m = 0;
	calc_p = 0;

	for (m = 62; m <= 92; m++) {
		for (p = 2; p <= 6; p++) {
			/* Find the optimal m and p divisors
			   with minimal error +/- the required clock */
			calc_dsi_clk = (m * ref_clk) / p;
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
	n = 1;
	dsi_mnp->dsi_pll_ctrl = 1 << (DSI_PLL_P1_POST_DIV_SHIFT + calc_p - 2);
	dsi_mnp->dsi_pll_div = (n - 1) << DSI_PLL_N1_DIV_SHIFT |
		m_seed << DSI_PLL_M1_DIV_SHIFT;

	return 0;
}

/*
 * XXX: The muxing and gating is hard coded for now. Need to add support for
 * sharing PLLs with two DSI outputs.
 */
static void vlv_configure_dsi_pll(struct dsi_config *config)
{
	struct dsi_context *intel_dsi = &config->ctx;
	int ret;
	struct dsi_mnp dsi_mnp;
	u32 dsi_clk;

	dsi_clk = dsi_clk_from_pclk(intel_dsi->pclk, intel_dsi->pixel_format,
				    intel_dsi->lane_count);

	ret = dsi_calc_mnp(dsi_clk, &dsi_mnp);
	if (ret) {
		pr_info("dsi_calc_mnp failed\n");
		return;
	}

	dsi_mnp.dsi_pll_ctrl |= DSI_PLL_CLK_GATE_DSI0_DSIPLL;

	pr_info("dsi pll div %08x, ctrl %08x\n",
		      dsi_mnp.dsi_pll_div, dsi_mnp.dsi_pll_ctrl);

	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, 0);
	vlv_cck_write(CCK_REG_DSI_PLL_DIVIDER, dsi_mnp.dsi_pll_div);
	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, dsi_mnp.dsi_pll_ctrl);
}

void vlv_enable_dsi_pll(struct dsi_config *config)
{
	u32 tmp;
	int pipe = config->pipe;

	vlv_configure_dsi_pll(config);

	/* wait at least 0.5 us after ungating before enabling VCO */
	usleep_range(1, 10);

	tmp = vlv_cck_read(CCK_REG_DSI_PLL_CONTROL);
	tmp |= DSI_PLL_VCO_EN;
	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, tmp);

	if (wait_for(REG_READ(PIPECONF(pipe)) & PIPECONF_DSI_PLL_LOCKED, 20)) {
		pr_err("DSI PLL lock failed\n");
		return;
	}

	pr_info("DSI PLL locked\n");
}

void vlv_disable_dsi_pll(struct dsi_config *config)
{
	u32 tmp;
	tmp = vlv_cck_read(CCK_REG_DSI_PLL_CONTROL);
	tmp &= ~DSI_PLL_VCO_EN;
	tmp |= DSI_PLL_LDO_GATE;
	vlv_cck_write(CCK_REG_DSI_PLL_CONTROL, tmp);
}
