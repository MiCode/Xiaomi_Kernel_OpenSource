/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved. */

#ifndef __MDSS_PLL_H
#define __MDSS_PLL_H

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/regmap.h>
#include "../clk-regmap.h"
#include "../clk-regmap-divider.h"
#include "../clk-regmap-mux.h"

#if defined(CONFIG_DRM)
#include <linux/sde_io_util.h>
#else
#include <linux/mdss_io_util.h>
#endif

#define MDSS_PLL_REG_W(base, offset, data)	\
				writel_relaxed((data), (base) + (offset))
#define MDSS_PLL_REG_R(base, offset)	readl_relaxed((base) + (offset))

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1) / 4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0) / 4) & 0xFF))

#define MDSS_DYN_PLL_REG_W(base, offset, addr0, addr1, data0, data1)   \
		writel_relaxed(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

#define upper_8_bit(x) ((((x) >> 2) & 0x100) >> 8)

enum {
	MDSS_DSI_PLL_10NM,
	MDSS_DP_PLL_10NM,
	MDSS_DSI_PLL_7NM,
	MDSS_DSI_PLL_7NM_V2,
	MDSS_DP_PLL_7NM,
	MDSS_DSI_PLL_28LPM,
	MDSS_DSI_PLL_14NM,
	MDSS_DP_PLL_14NM,
	MDSS_HDMI_PLL_28LPM,
	MDSS_DSI_PLL_12NM,
	MDSS_UNKNOWN_PLL,
};

enum {
	MDSS_PLL_TARGET_8996,
	MDSS_PLL_TARGET_SDM660,
};

#define DFPS_MAX_NUM_OF_FRAME_RATES 16
#ifdef CONFIG_FB_MSM_MDSS
#define PLL_TRIM_CODES_SIZE 2
#else
#define PLL_TRIM_CODES_SIZE 3
#endif

struct dfps_pll_codes {
	uint32_t pll_codes_1;
	uint32_t pll_codes_2;
#ifndef CONFIG_FB_MSM_MDSS
	uint32_t pll_codes_3;
#endif
};

struct dfps_codes_info {
	uint32_t is_valid;
	uint32_t clk_rate;	/* hz */
	struct dfps_pll_codes pll_codes;
};

struct dfps_info {
	uint32_t vco_rate_cnt;
	struct dfps_codes_info codes_dfps[DFPS_MAX_NUM_OF_FRAME_RATES];
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
	void __iomem	*ln_tx0_base;
	void __iomem	*ln_tx1_base;
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
	u32		cached_cfg0;
	u32		cached_cfg1;
	u32		cached_outdiv;

	u32		cached_postdiv1;
	u32		cached_postdiv3;
	u32		cached_vreg_cfg;

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
	int		cache_pll_trim_codes[PLL_TRIM_CODES_SIZE];

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

	/*
	 * for cases where dfps trigger happens before first
	 * suspend/resume and handoff is not finished.
	 */
	bool dfps_trigger;
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
	bool ret = false;

	if (!pll_res->gdsc_base) {
		WARN(1, "gdsc_base register is not defined\n");
		return true;
	}
	if (pll_res->target_id == MDSS_PLL_TARGET_SDM660)
		ret = ((readl_relaxed(pll_res->gdsc_base + 0x4) & BIT(31)) &&
		(!(readl_relaxed(pll_res->gdsc_base) & BIT(0)))) ? false : true;
	else
		ret = readl_relaxed(pll_res->gdsc_base) & BIT(31) ?
			 false : true;
	return ret;
}

static inline int mdss_pll_div_prepare(struct clk_hw *hw)
{
	struct clk_hw *parent_hw = clk_hw_get_parent(hw);
	/* Restore the divider's value */
	return hw->init->ops->set_rate(hw, clk_hw_get_rate(hw),
				clk_hw_get_rate(parent_hw));
}

static inline int mdss_set_mux_sel(void *context, unsigned int reg,
					unsigned int val)
{
	return 0;
}

static inline int mdss_get_mux_sel(void *context, unsigned int reg,
					unsigned int *val)
{
	*val = 0;
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
void mdss_pll_util_parse_dt_dfps(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
struct dss_vreg *mdss_pll_get_mp_by_reg_name(struct mdss_pll_resources *pll_res
		, char *name);
#endif
