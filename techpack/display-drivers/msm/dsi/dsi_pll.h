/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
#include "dsi_hw.h"

#define DSI_PLL_DBG(p, fmt, ...)	DRM_DEV_DEBUG(NULL, "[msm-dsi-debug]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1,	##__VA_ARGS__)
#define DSI_PLL_ERR(p, fmt, ...)	DRM_DEV_ERROR(NULL, "[msm-dsi-error]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1,	##__VA_ARGS__)
#define DSI_PLL_INFO(p, fmt, ...)	DRM_DEV_INFO(NULL, "[msm-dsi-info]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1,	##__VA_ARGS__)
#define DSI_PLL_WARN(p, fmt, ...)	DRM_WARN("[msm-dsi-warn]: DSI_PLL_%d: "\
		fmt, p ? p->index : -1, ##__VA_ARGS__)

#define DSI_PLL_REG_W(base, offset, data) \
	do {\
		pr_debug("[DSI_PLL][%s] - [0x%08x]\n", #offset, (uint32_t)(data)); \
		DSI_GEN_W32(base, offset, data); \
	} while (0)

#define DSI_PLL_REG_R(base, offset)	DSI_GEN_R32(base, offset)

#define DSI_DYN_PLL_REG_W(base, offset, addr0, addr1, data0, data1)   \
		DSI_DYN_REF_REG_W(base, offset, addr0, addr1, data0, data1)

#define upper_8_bit(x) ((((x) >> 2) & 0x100) >> 8)

#define DFPS_MAX_NUM_OF_FRAME_RATES 16
#define MAX_DSI_PLL_EN_SEQS	10

/* Register offsets for 5nm PHY PLL */
#define MMSS_DSI_PHY_PLL_PLL_CNTRL		(0x0014)
#define MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN	(0x002C)
#define MMSS_DSI_PHY_PLL_PLLLOCK_CMP_EN		(0x009C)

/* PLL codes magic id in header */
#define DSI_PLL_TRIM_CODES_MAGIC_ID	(0x5643)

/* PLL codes support version*/
#define DSI_PLL_TRIM_CODES_VERSION	(0x1)

struct lpfr_cfg {
	unsigned long vco_rate;
	u32 r;
};

enum {
	DSI_PLL_5NM,
	DSI_PLL_10NM,
	DSI_UNKNOWN_PLL,
};

enum {
	DISPLAY_PLL_CODEID_DSI0 = 0,
	DISPLAY_PLL_CODEID_DSI1 = 1,
	DISPLAY_PLL_CODEID_MAX
};

#pragma pack(push)
#pragma pack(1)
struct pll_codes_header {
	u16 magic_id;     /* Magic identifier */
	u8  version;      /* Version ID, starting with 1 */
	u8  num_entries;  /* Number of VCO rates in this structure */
	u16 size;         /* Size of the entrie data structure, including header */
	u8  reserved[4];  /* Reserved for future use */
};

struct pll_codes_entry {
	u8  device_id;    /* The PLL ID for this entry, refer to DISPLAY_PLL_CODEID */
	u32 vco_rate;     /* VCO rate of this entry in Hz */
	u8  num_codes;    /* Number of codes stored for this entry */
	u8  pll_codes[8]; /* List of PLL codes */
};

struct pll_codes_info {
	struct pll_codes_header   header;         /* PLL code data header */
	struct pll_codes_entry   *pll_code_data;  /* PLL code data */
};
#pragma pack(pop) // Restore the default packing

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
	s64	vco_ref_clk_rate;
	s64	vco_min_rate;
	s64	vco_rate;
	s64	byteclk_rate;
	s64	pclk_rate;

	u32		pll_revision;


	/* HW recommended delay during configuration of vco clock rate */
	u32		vco_delay;


	/*
	 * caching the pll trim codes in the case of dynamic refresh
	 */
	int		cache_pll_trim_codes[3];


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

	void *priv;

	/*
	 * dynamic refresh pll codes stored in this structure
	 */
	struct dfps_info *dfps;

	/*
	 * DSI pixel depth and lane information
	 */
	int bpp;
	int lanes;

	/*
	 * DSI PHY type DPHY/CPHY
	 */
	enum dsi_phy_type type;
};

struct dsi_pll_clk {
	struct clk_hw hw;
	void *priv;
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

struct dsi_pll_div_table {
	u64 min_hz;
	u64 max_hz;
	int pll_div;
	int phy_div;
};

static inline struct dsi_pll_clk *to_pll_clk_hw(struct clk_hw *hw)
{
	return container_of(hw, struct dsi_pll_clk, hw);
}

int dsi_pll_clock_register_5nm(struct platform_device *pdev,
				  struct dsi_pll_resource *pll_res);

int dsi_pll_init(struct platform_device *pdev,
				struct dsi_pll_resource **pll_res);

void dsi_pll_parse_dfps_data(struct platform_device *pdev, struct dsi_pll_resource *pll_res);

#endif
