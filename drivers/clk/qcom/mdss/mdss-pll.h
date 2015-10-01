/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>

#define MDSS_PLL_REG_W(base, offset, data)	\
				writel_relaxed((data), (base) + (offset))
#define MDSS_PLL_REG_R(base, offset)	readl_relaxed((base) + (offset))

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | (((addr1)/4) << 16) | ((data0) << 8) | ((addr0)/4))

#define MDSS_DYN_PLL_REG_W(base, offset, addr0, addr1, data0, data1)   \
		writel_relaxed(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

enum {
	MDSS_DSI_PLL_LPM,
	MDSS_DSI_PLL_HPM,
	MDSS_DSI_PLL_20NM,
	MDSS_EDP_PLL,
	MDSS_HDMI_PLL,
	MDSS_HDMI_PLL_20NM,
	MDSS_UNKNOWN_PLL,
};

enum {
	MDSS_PLL_TARGET_8974,
	MDSS_PLL_TARGET_8994,
	MDSS_PLL_TARGET_8992,
	MDSS_PLL_TARGET_8916,
	MDSS_PLL_TARGET_8939,
	MDSS_PLL_TARGET_8909,
	MDSS_PLL_TARGET_8952,
	MDSS_PLL_TARGET_8976,
};

enum {
	SSC_CENTRE_SPREAD,
	SSC_DOWN_SPREAD,
};

#define DFPS_MAX_NUM_OF_FRAME_RATES 10

struct dfps_panel_info {
	uint32_t enabled;
	uint32_t frame_rate_cnt;
	uint32_t frame_rate[DFPS_MAX_NUM_OF_FRAME_RATES];
};

struct dfps_pll_codes {
	uint32_t pll_codes_1;
	uint32_t pll_codes_2;
};

struct dfps_codes_info {
	uint32_t is_valid;
	uint32_t frame_rate;
	uint32_t clk_rate;
	struct dfps_pll_codes pll_codes;
};

struct dfps_info {
	struct dfps_panel_info panel_dfps;
	struct dfps_codes_info codes_dfps[DFPS_MAX_NUM_OF_FRAME_RATES];
	void *dfps_fb_base;
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
	 * Certain plls' have to change the vco freq range to support
	 * 90 phase difference between bit and byte clock frequency.
	 */
	bool		pll_en_90_phase;

	/*
	 * DSI PLL SSC properties
	 */
	bool		ssc_en;
	uint32_t	ssc_freq;
	uint32_t	ssc_ppm;
	uint32_t	spread_mode;

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
