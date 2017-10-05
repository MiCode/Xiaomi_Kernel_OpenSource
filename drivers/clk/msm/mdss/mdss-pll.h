/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_PLL_H
#define __MDSS_PLL_H

#include <linux/mdss_io_util.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/io.h>

#define MDSS_PLL_REG_W(base, offset, data)	\
				writel_relaxed((data), (base) + (offset))
#define MDSS_PLL_REG_R(base, offset)	readl_relaxed((base) + (offset))

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1) / 4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0) / 4) & 0xFF))

#define MDSS_DYN_PLL_REG_W(base, offset, addr0, addr1, data0, data1)   \
		writel_relaxed(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

enum {
	MDSS_DSI_PLL_8996,
	MDSS_DSI_PLL_8998,
	MDSS_DP_PLL_8998,
	MDSS_HDMI_PLL_8996,
	MDSS_HDMI_PLL_8996_V2,
	MDSS_HDMI_PLL_8996_V3,
	MDSS_HDMI_PLL_8996_V3_1_8,
	MDSS_HDMI_PLL_8998_3_3,
	MDSS_HDMI_PLL_8998_1_8,
	MDSS_UNKNOWN_PLL,
};

enum {
	MDSS_PLL_TARGET_8996,
};

#define DFPS_MAX_NUM_OF_FRAME_RATES 20

struct dfps_panel_info {
	uint32_t enabled;
	uint32_t frame_rate_cnt;
	uint32_t frame_rate[DFPS_MAX_NUM_OF_FRAME_RATES]; /* hz */
};

struct dfps_pll_codes {
	uint32_t pll_codes_1;
	uint32_t pll_codes_2;
};

struct dfps_codes_info {
	uint32_t is_valid;
	uint32_t frame_rate;	/* hz */
	uint32_t clk_rate;	/* hz */
	struct dfps_pll_codes pll_codes;
};

struct dfps_info {
	struct dfps_panel_info panel_dfps;
	struct dfps_codes_info codes_dfps[DFPS_MAX_NUM_OF_FRAME_RATES];
	void *dfps_fb_base;
	uint32_t chip_serial;
};

struct mdss_pll_resources {

	/* Pll specific resources like GPIO, power supply, clocks, etc*/
	struct dss_module_power mp;

	/*
	 * dsi/edp/hmdi plls' base register, phy, gdsc and dynamic refresh
	 * register mapping
	 */
	void __iomem	*pll_base;
	void __iomem	*phy_base;
	void __iomem	*gdsc_base;
	void __iomem	*dyn_pll_base;

	bool	is_init_locked;
	s64	vco_current_rate;
	s64	vco_locking_rate;
	s64	vco_ref_clk_rate;

	/*
	 * Certain pll's needs to update the same vco rate after resume in
	 * suspend/resume scenario. Cached the vco rate for such plls.
	 */
	unsigned long	vco_cached_rate;

	/* dsi/edp/hmdi pll interface type */
	u32		pll_interface_type;

	/*
	 * Target ID. Used in pll_register API for valid target check before
	 * registering the PLL clocks.
	 */
	u32		target_id;

	/* HW recommended delay during configuration of vco clock rate */
	u32		vco_delay;

	/* Ref-count of the PLL resources */
	u32		resource_ref_cnt;

	/*
	 * Keep track to resource status to avoid updating same status for the
	 * pll from different paths
	 */
	bool		resource_enable;

	/*
	 * Certain plls' do not allow vco rate update if it is on. Keep track of
	 * status for them to turn on/off after set rate success.
	 */
	bool		pll_on;

	/*
	 * handoff_status is true of pll is already enabled by bootloader with
	 * continuous splash enable case. Clock API will call the handoff API
	 * to enable the status. It is disabled if continuous splash
	 * feature is disabled.
	 */
	bool		handoff_resources;

	/*
	 * caching the pll trim codes in the case of dynamic refresh
	 */
	int		cache_pll_trim_codes[2];

	/*
	 * for maintaining the status of saving trim codes
	 */
	bool		reg_upd;

	/*
	 * Notifier callback for MDSS gdsc regulator events
	 */
	struct notifier_block gdsc_cb;

	/*
	 * Worker function to call PLL off event
	 */
	struct work_struct pll_off;

	/*
	 * PLL index if multiple index are available. Eg. in case of
	 * DSI we have 2 plls.
	 */
	uint32_t index;

	bool ssc_en;	/* share pll with master */
	bool ssc_center;	/* default is down spread */
	u32 ssc_freq;
	u32 ssc_ppm;

	struct mdss_pll_resources *slave;

	/*
	 * target pll revision information
	 */
	int		revision;

	void *priv;

	/*
	 * dynamic refresh pll codes stored in this structure
	 */
	struct dfps_info *dfps;

};

struct mdss_pll_vco_calc {
	s32 div_frac_start1;
	s32 div_frac_start2;
	s32 div_frac_start3;
	s64 dec_start1;
	s64 dec_start2;
	s64 pll_plllock_cmp1;
	s64 pll_plllock_cmp2;
	s64 pll_plllock_cmp3;
};

static inline bool is_gdsc_disabled(struct mdss_pll_resources *pll_res)
{
	if (!pll_res->gdsc_base) {
		WARN(1, "gdsc_base register is not defined\n");
		return true;
	}

	return ((readl_relaxed(pll_res->gdsc_base + 0x4) & BIT(31)) &&
		(!(readl_relaxed(pll_res->gdsc_base) & BIT(0)))) ? false : true;
}

static inline int mdss_pll_div_prepare(struct clk *c)
{
	struct div_clk *div = to_div_clk(c);
	/* Restore the divider's value */
	return div->ops->set_div(div, div->data.div);
}

static inline int mdss_set_mux_sel(struct mux_clk *clk, int sel)
{
	return 0;
}

static inline int mdss_get_mux_sel(struct mux_clk *clk)
{
	return 0;
}

int mdss_pll_resource_enable(struct mdss_pll_resources *pll_res, bool enable);
int mdss_pll_util_resource_init(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res);
void mdss_pll_util_resource_deinit(struct platform_device *pdev,
					 struct mdss_pll_resources *pll_res);
void mdss_pll_util_resource_release(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res);
int mdss_pll_util_resource_enable(struct mdss_pll_resources *pll_res,
								bool enable);
int mdss_pll_util_resource_parse(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
struct dss_vreg *mdss_pll_get_mp_by_reg_name(struct mdss_pll_resources *pll_res
		, char *name);
#endif
