/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __DSI_PLL_H
#define __DSI_PLL_H

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/regmap.h>
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "dsi_defs.h"

#define DSI_PLL_DBG(p, fmt, ...)	DRM_DEV_DEBUG(NULL, "[msm-dsi-debug]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1,	##__VA_ARGS__)
#define DSI_PLL_ERR(p, fmt, ...)	DRM_DEV_ERROR(NULL, "[msm-dsi-error]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1,	##__VA_ARGS__)
#define DSI_PLL_INFO(p, fmt, ...)	DRM_DEV_INFO(NULL, "[msm-dsi-info]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1,	##__VA_ARGS__)
#define DSI_PLL_WARN(p, fmt, ...)	DRM_WARN("[msm-dsi-warn]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1, ##__VA_ARGS__)

#define DSI_PLL_REG_W(base, offset, data)	\
				writel_relaxed((data), (base) + (offset))
#define DSI_PLL_REG_R(base, offset)	readl_relaxed((base) + (offset))

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1) / 4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0) / 4) & 0xFF))

#define DSI_DYN_PLL_REG_W(base, offset, addr0, addr1, data0, data1)   \
		writel_relaxed(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

#define upper_8_bit(x) ((((x) >> 2) & 0x100) >> 8)

#define DFPS_MAX_NUM_OF_FRAME_RATES 16
#define MAX_DSI_PLL_EN_SEQS	10

/* Register offsets for 5nm PHY PLL */
#define MMSS_DSI_PHY_PLL_PLL_CNTRL		(0x0014)
#define MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN	(0x002C)
#define MMSS_DSI_PHY_PLL_PLLLOCK_CMP_EN		(0x009C)

struct lpfr_cfg {
	unsigned long vco_rate;
	u32 r;
};

enum {
	DSI_PLL_5NM,
	DSI_PLL_10NM,
	DSI_UNKNOWN_PLL,
};

struct dfps_pll_codes {
	uint32_t pll_codes_1;
	uint32_t pll_codes_2;
	uint32_t pll_codes_3;
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

struct dsi_pll_resource {

	/*
	 * dsi base register, phy, gdsc and dynamic refresh
	 * register mapping
	 */
	void __iomem	*pll_base;
	void __iomem	*phy_base;
	void __iomem	*gdsc_base;
	void __iomem	*dyn_pll_base;

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

	u32		pll_revision;


	/* HW recommended delay during configuration of vco clock rate */
	u32		vco_delay;


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
	int		cache_pll_trim_codes[3];

	/*
	 * for maintaining the status of saving trim codes
	 */
	bool		reg_upd;


	/*
	 * PLL index if multiple index are available. Eg. in case of
	 * DSI we have 2 plls.
	 */
	uint32_t index;

	bool ssc_en;	/* share pll with master */
	bool ssc_center;	/* default is down spread */
	u32 ssc_freq;
	u32 ssc_ppm;

	struct dsi_pll_resource *slave;

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

struct dsi_pll_vco_clk {
	struct clk_hw	hw;
	unsigned long	ref_clk_rate;
	u64	min_rate;
	u64	max_rate;
	u32		pll_en_seq_cnt;
	struct lpfr_cfg *lpfr_lut;
	u32		lpfr_lut_size;
	void		*priv;

	int (*pll_enable_seqs[MAX_DSI_PLL_EN_SEQS])
			(struct dsi_pll_resource *pll_res);
};

struct dsi_pll_vco_calc {
	s32 div_frac_start1;
	s32 div_frac_start2;
	s32 div_frac_start3;
	s64 dec_start1;
	s64 dec_start2;
	s64 pll_plllock_cmp1;
	s64 pll_plllock_cmp2;
	s64 pll_plllock_cmp3;
};

static inline bool is_gdsc_disabled(struct dsi_pll_resource *pll_res)
{
	if (!pll_res->gdsc_base) {
		WARN(1, "gdsc_base register is not defined\n");
		return true;
	}
	return readl_relaxed(pll_res->gdsc_base) & BIT(31) ? false : true;
}

static inline int dsi_pll_div_prepare(struct clk_hw *hw)
{
	struct clk_hw *parent_hw = clk_hw_get_parent(hw);
	/* Restore the divider's value */
	return hw->init->ops->set_rate(hw, clk_hw_get_rate(hw),
				clk_hw_get_rate(parent_hw));
}

static inline int dsi_set_mux_sel(void *context, unsigned int reg,
					unsigned int val)
{
	return 0;
}

static inline int dsi_get_mux_sel(void *context, unsigned int reg,
					unsigned int *val)
{
	*val = 0;
	return 0;
}

static inline struct dsi_pll_vco_clk *to_vco_clk_hw(struct clk_hw *hw)
{
	return container_of(hw, struct dsi_pll_vco_clk, hw);
}

int dsi_pll_clock_register_5nm(struct platform_device *pdev,
				  struct dsi_pll_resource *pll_res);

int dsi_pll_clock_register_10nm(struct platform_device *pdev,
				  struct dsi_pll_resource *pll_res);

int dsi_pll_init(struct platform_device *pdev,
				struct dsi_pll_resource **pll_res);
#endif
