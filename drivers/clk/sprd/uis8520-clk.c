// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc UIS8520 clock driver
 *
 * Copyright (C) 2023 Unisoc, Inc.
 * Author: yao.zhang3 <yao.zhang3@unisoc.com>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,uis8520-clk.h>
#include <dt-bindings/reset/sprd,uis8520-reset.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"
#include "reset.h"

#define UIS8520_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* pll gate clock */
static CLK_FIXED_FACTOR_FW_NAME(clk_26m_aud, "clk-26m-aud", "ext-26m", 1, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_13m, "clk-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_6m5, "clk-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_4m3, "clk-4m3", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_4m, "clk-4m", "ext-26m", 13, 2, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_2m, "clk-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_1m, "clk-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_250k, "clk-250k", "ext-26m", 104, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_200k, "clk-200k", "ext-26m", 130, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_160k, "clk-160k", "ext-26m", 325, 2, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_16k, "clk-16k", "ext-26m", 1625, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_100m_25m, "rco-100m-25m", "rco-100m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_100m_20m, "rco-100m-20m", "rco-100m", 5, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_100m_4m, "rco-100m-4m", "rco-100m", 25, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_100m_2m, "rco-100m-2m", "rco-100m", 50, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_60m_20m, "rco-60m-20m", "rco-60m", 3, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_60m_4m, "rco-60m-4m", "rco-60m", 15, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_60m_2m, "rco-60m-2m", "rco-60m", 30, 1, 0);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(phyr8pll_gate, "phyr8pll-gate", "ext-26m", 0xa30,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(psr8pll_gate, "psr8pll-gate", "ext-26m", 0xa34,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(v4nrpll_gate, "v4nrpll-gate", "ext-26m", 0xa44,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(rpll_gate, "rpll-gate", "ext-26m", 0xa0c,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(tgpll_gate, "tgpll-gate", "ext-26m", 0xa40,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(mplll_gate, "mplll-gate", "ext-26m", 0x9f8,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(mplls_gate, "mplls-gate", "ext-26m", 0x9f4,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(dpll0_gate, "dpll0-gate", "ext-26m", 0xa14,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(dpll1_gate, "dpll1-gate", "ext-26m", 0xa18,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(dpll2_gate, "dpll2-gate", "ext-26m", 0xa1c,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(audpll_gate, "audpll-gate", "ext-26m", 0xa48,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(pcie0pllv_gate, "pcie0pllv-gate", "ext-26m", 0xa50,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(pcie1pllv_gate, "pcie1pllv-gate", "ext-26m", 0xa54,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(tsnpll_gate, "tsnpll-gate", "ext-26m", 0xa60,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(ethpll_gate, "ethpll-gate", "ext-26m", 0xa64,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *uis8520_pmu_gate_clks[] = {
	/* address base is 0x64910000 */
	&phyr8pll_gate.common,
	&psr8pll_gate.common,
	&v4nrpll_gate.common,
	&rpll_gate.common,
	&tgpll_gate.common,
	&mplll_gate.common,
	&mplls_gate.common,
	&dpll0_gate.common,
	&dpll1_gate.common,
	&dpll2_gate.common,
	&audpll_gate.common,
	&pcie0pllv_gate.common,
	&pcie1pllv_gate.common,
	&tsnpll_gate.common,
	&ethpll_gate.common,
};

static struct clk_hw_onecell_data uis8520_pmu_gate_hws = {
	.hws	= {
		[CLK_26M_AUD]		= &clk_26m_aud.hw,
		[CLK_13M]		= &clk_13m.hw,
		[CLK_6M5]		= &clk_6m5.hw,
		[CLK_4M3]		= &clk_4m3.hw,
		[CLK_4M]		= &clk_4m.hw,
		[CLK_2M]		= &clk_2m.hw,
		[CLK_1M]		= &clk_1m.hw,
		[CLK_250K]		= &clk_250k.hw,
		[CLK_200K]		= &clk_200k.hw,
		[CLK_160K]		= &clk_160k.hw,
		[CLK_16K]		= &clk_16k.hw,
		[CLK_RCO_100M_25M]	= &rco_100m_25m.hw,
		[CLK_RCO_100m_20M]      = &rco_100m_20m.hw,
		[CLK_RCO_100m_4M]	= &rco_100m_4m.hw,
		[CLK_RCO_100m_2M]	= &rco_100m_2m.hw,
		[CLK_RCO_60m_20M]	= &rco_60m_20m.hw,
		[CLK_RCO_60m_4M]	= &rco_60m_4m.hw,
		[CLK_RCO_60m_2M]	= &rco_60m_2m.hw,
		[CLK_PHYR8PLL_GATE]	= &phyr8pll_gate.common.hw,
		[CLK_PSR8PLL_GATE]	= &psr8pll_gate.common.hw,
		[CLK_V4NRPLL_GATE]	= &v4nrpll_gate.common.hw,
		[CLK_TGPLL_GATE]	= &tgpll_gate.common.hw,
		[CLK_MPLLL_GATE]	= &mplll_gate.common.hw,
		[CLK_MPLLS_GATE]	= &mplls_gate.common.hw,
		[CLK_DPLL0_GATE]	= &dpll0_gate.common.hw,
		[CLK_DPLL1_GATE]	= &dpll1_gate.common.hw,
		[CLK_DPLL2_GATE]	= &dpll2_gate.common.hw,
		[CLK_AUDPLL_GATE]	= &audpll_gate.common.hw,
		[CLK_PCIE0PLLV_GATE]	= &pcie0pllv_gate.common.hw,
		[CLK_PCIE1PLLV_GATE]	= &pcie1pllv_gate.common.hw,
		[CLK_TSNPLL_GATE]	= &tsnpll_gate.common.hw,
		[CLK_ETHPLL_GATE]	= &ethpll_gate.common.hw,
	},
	.num = CLK_PMU_GATE_NUM,
};

static struct sprd_reset_map uis8520_pmu_apb_resets[] = {
	[RESET_PMU_APB_AUD_CEVA_SOFT_RST]	= { 0x0b88, BIT(0), 0x1000 },
	[RESET_PMU_APB_AP_SOFT_RST]		= { 0x0b98, BIT(0), 0x1000 },
	[RESET_PMU_APB_APCPU_TOP_SOFT_RST]	= { 0x0b98, BIT(1), 0x1000 },
	[RESET_PMU_APB_PS_CP_SOFT_RST]		= { 0x0b98, BIT(11), 0x1000 },
	[RESET_PMU_APB_PHY_CP_SOFT_RST]		= { 0x0b98, BIT(14), 0x1000 },
	[RESET_PMU_APB_AUDIO_SOFT_RST]		= { 0x0b98, BIT(22), 0x1000 },
	[RESET_PMU_APB_IPA_SOFT_RST]		= { 0x0b98, BIT(24), 0x1000 },
	[RESET_PMU_APB_PCIE_0_SOFT_RST]		= { 0x0b98, BIT(25), 0x1000 },
	[RESET_PMU_APB_PCIE_1_SOFT_RST]		= { 0x0b98, BIT(26), 0x1000 },
	[RESET_PMU_APB_ETHER_XGE_SOFT_RST]	= { 0x0b98, BIT(27), 0x1000 },
	[RESET_PMU_APB_CS_SOFT_RST]		= { 0x0b98, BIT(29), 0x1000 },
	[RESET_PMU_APB_AON_SOFT_RST]		= { 0x0b98, BIT(30), 0x1000 },
	[RESET_PMU_APB_TIANSHAN_CP_SOFT_RST]	= { 0x0b98, BIT(31), 0x1000 },
	[RESET_PMU_APB_SP_SOFT_RST]		= { 0x0b9c, BIT(0), 0x1000 },
	[RESET_PMU_APB_PUB_SOFT_RST]		= { 0x0b9c, BIT(4), 0x1000 },
};

static struct sprd_clk_desc uis8520_pmu_gate_desc = {
	.clk_clks	= uis8520_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(uis8520_pmu_gate_clks),
	.hw_clks	= &uis8520_pmu_gate_hws,
	.resets		= uis8520_pmu_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_pmu_apb_resets),
};

/* pll clock at g1 */
static struct freq_table rpll_ftable[4] = {
	{ .ibias = 1, .max_freq = 2000000000ULL, .vco_sel = 0 },
	{ .ibias = 2, .max_freq = 2800000000ULL, .vco_sel = 0 },
	{ .ibias = 3, .max_freq = 3200000000ULL, .vco_sel = 0 },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ, .vco_sel = INVALID_MAX_VCO_SEL},
};

static struct clk_bit_field f_rpll[PLL_FACT_MAX] = {
	{ .shift = 18,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 4 },	/* icp		*/
	{ .shift = 7,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 8 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 66,	.width = 2 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

static SPRD_PLL_HW(rpll, "rpll", &clk_26m_aud.hw, 0x10,
				   3, rpll_ftable, f_rpll, 240,
				   1000, 1000, 1, 1560000000);
static CLK_FIXED_FACTOR_HW(rpll_390m, "rpll-390m", &rpll.common.hw, 2, 1, 0);
static CLK_FIXED_FACTOR_HW(rpll_260m, "rpll-260m", &rpll.common.hw, 3, 1, 0);
static CLK_FIXED_FACTOR_HW(rpll_26m, "rpll-26m", &rpll.common.hw, 30, 1, 0);

static struct sprd_clk_common *uis8520_g1_pll_clks[] = {
	/* address base is 0x64304000 */
	&rpll.common,
};

static struct clk_hw_onecell_data uis8520_g1_pll_hws = {
	.hws    = {
		[CLK_RPLL]		= &rpll.common.hw,
		[CLK_RPLL_390M]		= &rpll_390m.hw,
		[CLK_RPLL_260M]		= &rpll_260m.hw,
		[CLK_RPLL_26M]		= &rpll_26m.hw,
	},
	.num    = CLK_ANLG_PHY_G1_NUM,
};

static struct sprd_clk_desc uis8520_g1_pll_desc = {
	.clk_clks	= uis8520_g1_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(uis8520_g1_pll_clks),
	.hw_clks	= &uis8520_g1_pll_hws,
};

/* pll at g2 */
static struct freq_table tsnpll_ftable[4] = {
	{ .ibias = 1, .max_freq = 2000000000ULL, .vco_sel = 0 },
	{ .ibias = 2, .max_freq = 2800000000ULL, .vco_sel = 0 },
	{ .ibias = 3, .max_freq = 3200000000ULL, .vco_sel = 0 },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ, .vco_sel = INVALID_MAX_VCO_SEL},
};

static struct clk_bit_field f_tsnpll[PLL_FACT_MAX] = {
	{ .shift = 84,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 56,	.width = 3 },	/* icp		*/
	{ .shift = 20,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 8 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 66,	.width = 2 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

static SPRD_PLL_FW_NAME(tsnpll, "tsnpll", "ext-26m", 0x8,
				   3, tsnpll_ftable, f_tsnpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR_HW(tsnpll_1300m, "tsnpll-1300m", &tsnpll.common.hw, 2, 1, 0);
static CLK_FIXED_FACTOR_HW(tsnpll_1040m, "tsnpll-1040m", &tsnpll.common.hw, 5, 2, 0);
static CLK_FIXED_FACTOR_HW(tsnpll_100m, "tsnpll-100m", &tsnpll.common.hw, 26, 1, 0);

static struct clk_bit_field f_dftpll[PLL_FACT_MAX] = {
	{ .shift = 80,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 52,	.width = 3 },	/* icp		*/
	{ .shift = 12,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0 },	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 24,	.width = 1 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};
#define dftpll_ftable tsnpll_ftable
static SPRD_PLL_FW_NAME(dftpll, "dftpll", "ext-26m", 0x14,
				   3, dftpll_ftable, f_dftpll, 240,
				   1000, 1000, 1, 1600000000);

static struct clk_bit_field f_psr8pll[PLL_FACT_MAX] = {
	{ .shift = 112,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 84,	.width = 3 },	/* icp		*/
	{ .shift = 40,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0 },	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 52,	.width = 1 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

#define psr8pll_ftable tsnpll_ftable
static SPRD_PLL_HW(psr8pll, "psr8pll", &psr8pll_gate.common.hw, 0x1c,
				   4, psr8pll_ftable, f_psr8pll, 240,
				   1000, 1000, 1, 1600000000);

#define f_phyr8pll f_psr8pll
#define phyr8pll_ftable tsnpll_ftable
static SPRD_PLL_HW(phyr8pll, "phyr8pll", &phyr8pll_gate.common.hw, 0x28,
				   4, phyr8pll_ftable, f_phyr8pll, 240,
				   1000, 1000, 1, 1600000000);

static struct clk_bit_field f_v4nrpll[PLL_FACT_MAX] = {
	{ .shift = 156,	.width = 1 },	/* lock_done	*/
	{ .shift = 33,	.width = 1 },	/* div_s	*/
	{ .shift = 63,	.width = 1 },	/* mod_en	*/
	{ .shift = 108,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 116,	.width = 3 },	/* icp		*/
	{ .shift = 64,	.width = 11 },	/* n		*/
	{ .shift = 76,	.width = 7 },	/* nint		*/
	{ .shift = 36,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 84,	.width = 1 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

#define v4nrpll_ftable tsnpll_ftable
static SPRD_PLL_FW_NAME(v4nrpll, "v4nrpll", "ext-26m", 0x34,
				   5, v4nrpll_ftable, f_v4nrpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR_HW(v4nrpll_38m4, "v4nrpll-38m4", &v4nrpll.common.hw, 64, 1, 0);
static CLK_FIXED_FACTOR_HW(v4nrpll_245m76, "v4nrpll-245m76", &v4nrpll.common.hw, 10, 1, 0);
static CLK_FIXED_FACTOR_HW(v4nrpll_409m6, "v4nrpll-409m6", &v4nrpll.common.hw, 6, 1, 0);
static CLK_FIXED_FACTOR_HW(v4nrpll_614m4, "v4nrpll-614m4", &v4nrpll.common.hw, 4, 1, 0);
static CLK_FIXED_FACTOR_HW(v4nrpll_819m2, "v4nrpll-819m2", &v4nrpll.common.hw, 3, 1, 0);
static CLK_FIXED_FACTOR_HW(v4nrpll_1228m8, "v4nrpll-1228m8", &v4nrpll.common.hw, 2, 1, 0);

static struct clk_bit_field f_audpll[PLL_FACT_MAX] = {
	{ .shift = 156,	.width = 1 },	/* lock_done	*/
	{ .shift = 12,	.width = 1 },	/* div_s	*/
	{ .shift = 59,	.width = 1 },	/* mod_en	*/
	{ .shift = 108,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 116,	.width = 3 },	/* icp		*/
	{ .shift = 64,	.width = 11 },	/* n		*/
	{ .shift = 76,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 84,	.width = 1 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};
#define audpll_ftable tsnpll_ftable
static SPRD_PLL_FW_NAME(audpll, "audpll", "ext-26m", 0x48,
				   5, audpll_ftable, f_audpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR_HW(audpll_38m4, "audpll-38m4", &audpll.common.hw, 32, 1, 0);
static CLK_FIXED_FACTOR_HW(audpll_24m57, "audpll-24m57", &audpll.common.hw, 50, 1, 0);
static CLK_FIXED_FACTOR_HW(audpll_19m2, "audpll-19m2", &audpll.common.hw, 64, 1, 0);
static CLK_FIXED_FACTOR_HW(audpll_12m28, "audpll-12m28", &audpll.common.hw, 100, 1, 0);

static struct clk_bit_field f_tgpll[PLL_FACT_MAX] = {
	{ .shift = 156,	.width = 1 },	/* lock_done	*/
	{ .shift = 13,	.width = 1 },	/* div_s	*/
	{ .shift = 59,	.width = 1 },	/* mod_en	*/
	{ .shift = 108,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 116,	.width = 3 },	/* icp		*/
	{ .shift = 64,	.width = 11 },	/* n		*/
	{ .shift = 76,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 84,	.width = 1 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};
#define tgpll_ftable tsnpll_ftable
static SPRD_PLL_FW_NAME(tgpll, "tgpll", "ext-26m", 0x5c,
				   3, tgpll_ftable, f_tgpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR_HW(tgpll_12m, "tgpll-12m", &tgpll.common.hw, 128, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_24m, "tgpll-24m", &tgpll.common.hw, 64, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_38m4, "tgpll-38m4", &tgpll.common.hw, 40, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_48m, "tgpll-48m", &tgpll.common.hw, 32, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_51m2, "tgpll-51m2", &tgpll.common.hw, 30, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_64m, "tgpll-64m", &tgpll.common.hw, 24, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_76m8, "tgpll-76m8", &tgpll.common.hw, 20, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_96m, "tgpll-96m", &tgpll.common.hw, 16, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_128m, "tgpll-128m", &tgpll.common.hw, 12, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_153m6, "tgpll-153m6", &tgpll.common.hw, 10, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_170m6, "tgpll-170m6", &tgpll.common.hw, 9, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_192m, "tgpll-192m", &tgpll.common.hw, 8, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_219m4, "tgpll-219m4", &tgpll.common.hw, 7, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_256m, "tgpll-256m", &tgpll.common.hw, 6, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_307m2, "tgpll-307m2", &tgpll.common.hw, 5, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_384m, "tgpll-384m", &tgpll.common.hw, 4, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_512m, "tgpll-512m", &tgpll.common.hw, 3, 1, 0);
static CLK_FIXED_FACTOR_HW(tgpll_614m4, "tgpll-614m4", &tgpll.common.hw, 5, 2, 0);
static CLK_FIXED_FACTOR_HW(tgpll_768m, "tgpll-768m", &tgpll.common.hw, 2, 1, 0);

static struct sprd_clk_common *uis8520_g2_pll_clks[] = {
	/* address base is 0x64320000 */
	&tsnpll.common,
	&dftpll.common,
	&psr8pll.common,
	&phyr8pll.common,
	&v4nrpll.common,
	&audpll.common,
	&tgpll.common,
};

static struct clk_hw_onecell_data uis8520_g2_pll_hws = {
	.hws    = {
		[CLK_TSNPLL]		= &tsnpll.common.hw,
		[CLK_TSNPLL_1300M]	= &tsnpll_1300m.hw,
		[CLK_TSNPLL_1040M]	= &tsnpll_1040m.hw,
		[CLK_TSNPLL_100M]	= &tsnpll_100m.hw,
		[CLK_DFTPLL]		= &dftpll.common.hw,
		[CLK_PSR8PLL]		= &psr8pll.common.hw,
		[CLK_PHYR8PLL]		= &phyr8pll.common.hw,
		[CLK_V4NRPLL]		= &v4nrpll.common.hw,
		[CLK_V4NRPLL_38M4]	= &v4nrpll_38m4.hw,
		[CLK_V4NRPLL_245M76]	= &v4nrpll_245m76.hw,
		[CLK_V4NRPLL_409M6]	= &v4nrpll_409m6.hw,
		[CLK_V4NRPLL_614M4]	= &v4nrpll_614m4.hw,
		[CLK_V4NRPLL_819M2]	= &v4nrpll_819m2.hw,
		[CLK_V4NRPLL_1228M8]	= &v4nrpll_1228m8.hw,
		[CLK_AUDPLL]		= &audpll.common.hw,
		[CLK_AUDPLL_38M4]	= &audpll_38m4.hw,
		[CLK_AUDPLL_24M57]	= &audpll_24m57.hw,
		[CLK_AUDPLL_19M2]	= &audpll_19m2.hw,
		[CLK_AUDPLL_12M28]	= &audpll_12m28.hw,
		[CLK_TGPLL]		= &tgpll.common.hw,
		[CLK_TGPLL_12M]		= &tgpll_12m.hw,
		[CLK_TGPLL_24M]		= &tgpll_24m.hw,
		[CLK_TGPLL_38M4]	= &tgpll_38m4.hw,
		[CLK_TGPLL_48M]		= &tgpll_48m.hw,
		[CLK_TGPLL_51M2]	= &tgpll_51m2.hw,
		[CLK_TGPLL_64M]		= &tgpll_64m.hw,
		[CLK_TGPLL_76M8]	= &tgpll_76m8.hw,
		[CLK_TGPLL_96M]		= &tgpll_96m.hw,
		[CLK_TGPLL_128M]	= &tgpll_128m.hw,
		[CLK_TGPLL_153M6]	= &tgpll_153m6.hw,
		[CLK_TGPLL_170M6]	= &tgpll_170m6.hw,
		[CLK_TGPLL_192M]	= &tgpll_192m.hw,
		[CLK_TGPLL_219M4]	= &tgpll_219m4.hw,
		[CLK_TGPLL_256M]	= &tgpll_256m.hw,
		[CLK_TGPLL_307M2]	= &tgpll_307m2.hw,
		[CLK_TGPLL_384M]	= &tgpll_384m.hw,
		[CLK_TGPLL_512M]	= &tgpll_512m.hw,
		[CLK_TGPLL_614M4]	= &tgpll_614m4.hw,
		[CLK_TGPLL_768M]	= &tgpll_768m.hw,
	},
	.num    = CLK_ANLG_PHY_G2_NUM,
};

static struct sprd_clk_desc uis8520_g2_pll_desc = {
	.clk_clks	= uis8520_g2_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(uis8520_g2_pll_clks),
	.hw_clks	= &uis8520_g2_pll_hws,
};

/* pll at g10 */
static struct freq_table mplll_ftable[4] = {
	{ .ibias = 7,  .max_freq = 2000000000ULL,  .vco_sel = 0 },
	{ .ibias = 10,  .max_freq = 3000000000ULL,  .vco_sel = 0 },
	{ .ibias = 12,  .max_freq = 3200000000ULL,  .vco_sel = 0 },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ, .vco_sel = INVALID_MAX_VCO_SEL},
};

static struct clk_bit_field f_mplll[PLL_FACT_MAX] = {
	{ .shift = 15,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 4 },	/* icp		*/
	{ .shift = 4,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0 },	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 68,	.width = 4 },	/* postdiv	*/
	{ .shift = 67,	.width = 1 },	/* refdiv	*/
	{ .shift = 96,	.width = 1 },	/* vco_sel	*/
};

static SPRD_PLL_FW_NAME(mplll, "mplll", "ext-26m", 0x0,
				   4, mplll_ftable, f_mplll, 240,
				   1000, 1000, 1, 1200000000);

static struct clk_bit_field f_mplls[PLL_FACT_MAX] = {
	{ .shift = 15,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 4 },	/* icp		*/
	{ .shift = 4,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 68,	.width = 4 },	/* postdiv	*/
	{ .shift = 72,	.width = 1 },	/* refdiv	*/
	{ .shift = 96,	.width = 1 },	/* vco_sel	*/
};

#define mplls_ftable mplll_ftable
static SPRD_PLL_FW_NAME(mplls, "mplls", "ext-26m", 0x20,
				   4, mplls_ftable, f_mplls, 240,
				   1000, 1000, 1, 1200000000);

static struct sprd_clk_common *uis8520_g10_pll_clks[] = {
	/* address base is 0x64334000 */
	&mplll.common,
	&mplls.common,
};

static struct clk_hw_onecell_data uis8520_g10_pll_hws = {
	.hws    = {
		[CLK_MPLLL]		= &mplll.common.hw,
		[CLK_MPLLS]		= &mplls.common.hw,
	},
	.num    = CLK_ANLG_PHY_G10_NUM,
};

static struct sprd_clk_desc uis8520_g10_pll_desc = {
	.clk_clks	= uis8520_g10_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(uis8520_g10_pll_clks),
	.hw_clks	= &uis8520_g10_pll_hws,
};

/* pll at g11 */
static struct freq_table dpll_ftable[4] = {
	{ .ibias = 1, .max_freq = 2000000000ULL, .vco_sel = 0 },
	{ .ibias = 2, .max_freq = 2800000000ULL, .vco_sel = 0 },
	{ .ibias = 3, .max_freq = 3200000000ULL, .vco_sel = 0 },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ, .vco_sel = INVALID_MAX_VCO_SEL},
};

static struct clk_bit_field f_dpll[PLL_FACT_MAX] = {
	{ .shift = 19,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 4,	.width = 4 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 56,	.width = 8 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 68,	.width = 4 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

static SPRD_PLL_FW_NAME(dpll0, "dpll0", "ext-26m", 0x4,
				   3, dpll_ftable, f_dpll, 240,
				   1000, 1000, 1, 1500000000);

static SPRD_PLL_FW_NAME(dpll1, "dpll1", "ext-26m", 0x24,
				   3, dpll_ftable, f_dpll, 240,
				   1000, 1000, 1, 1500000000);

static SPRD_PLL_FW_NAME(dpll2, "dpll2", "ext-26m", 0x44,
				   3, dpll_ftable, f_dpll, 240,
				   1000, 1000, 1, 1500000000);

static struct sprd_clk_common *uis8520_g11_pll_clks[] = {
	/* address base is 0x64308000 */
	&dpll0.common,
	&dpll1.common,
	&dpll2.common,
};

static struct clk_hw_onecell_data uis8520_g11_pll_hws = {
	.hws    = {
		[CLK_DPLL0]		= &dpll0.common.hw,
		[CLK_DPLL1]		= &dpll1.common.hw,
		[CLK_DPLL2]		= &dpll2.common.hw,
	},
	.num    = CLK_ANLG_PHY_G11_NUM,
};

static struct sprd_clk_desc uis8520_g11_pll_desc = {
	.clk_clks	= uis8520_g11_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(uis8520_g11_pll_clks),
	.hw_clks	= &uis8520_g11_pll_hws,
};

/* ap apb gates */
static SPRD_SC_GATE_CLK_FW_NAME(iis0_eb, "iis0-eb", "ext-26m", 0x0,
			0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(iis1_eb, "iis1-eb", "ext-26m", 0x0,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(iis2_eb, "iis2-eb", "ext-26m", 0x0,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi0_eb, "spi0-eb", "ext-26m", 0x0,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi1_eb, "spi1-eb", "ext-26m", 0x0,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi2_eb, "spi2-eb", "ext-26m", 0x0,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart3_eb, "uart3-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart0_eb, "uart0-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart1_eb, "uart1-eb", "ext-26m", 0x0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart2_eb, "uart2-eb", "ext-26m", 0x0,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi0_lfin_eb, "spi0-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi1_lfin_eb, "spi1-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi2_lfin_eb, "spi2-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi3_lfin_eb, "spi3-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi4_lfin_eb, "spi4-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi5_lfin_eb, "spi5-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ce_sec_eb, "ce-sec-eb", "ext-26m", 0x0,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ce_pub_eb, "ce-pub-eb", "ext-26m", 0x0,
			0x1000, BIT(31), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nic400_eb, "nic400-eb", "ext-26m", 0x10,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart4_eb, "uart4-eb", "ext-26m", 0x14,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart5_eb, "uart5-eb", "ext-26m", 0x14,
			0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart6_eb, "uart6-eb", "ext-26m", 0x14,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart7_eb, "uart7-eb", "ext-26m", 0x14,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart8_eb, "uart8-eb", "ext-26m", 0x14,
			0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart9_eb, "uart9-eb", "ext-26m", 0x14,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart10_eb, "uart10-eb", "ext-26m", 0x14,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi3_eb, "spi3-eb", "ext-26m", 0x14,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi4_eb, "spi4-eb", "ext-26m", 0x14,
			0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi5_eb, "spi5-eb", "ext-26m", 0x14,
			0x1000, BIT(9), 0, 0);

static struct sprd_clk_common *uis8520_apapb_gate[] = {
	/* address base is 0x20100000 */
	&iis0_eb.common,
	&iis1_eb.common,
	&iis2_eb.common,
	&spi0_eb.common,
	&spi1_eb.common,
	&spi2_eb.common,
	&uart3_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&uart2_eb.common,
	&spi0_lfin_eb.common,
	&spi1_lfin_eb.common,
	&spi2_lfin_eb.common,
	&spi3_lfin_eb.common,
	&spi4_lfin_eb.common,
	&spi5_lfin_eb.common,
	&ce_sec_eb.common,
	&ce_pub_eb.common,
	&nic400_eb.common,
	&uart4_eb.common,
	&uart5_eb.common,
	&uart6_eb.common,
	&uart7_eb.common,
	&uart8_eb.common,
	&uart9_eb.common,
	&uart10_eb.common,
	&spi3_eb.common,
	&spi4_eb.common,
	&spi5_eb.common,
};

static struct clk_hw_onecell_data uis8520_apapb_gate_hws = {
	.hws	= {
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_SPI1_EB]		= &spi1_eb.common.hw,
		[CLK_SPI2_EB]		= &spi2_eb.common.hw,
		[CLK_UART3_EB]		= &uart3_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_UART2_EB]		= &uart2_eb.common.hw,
		[CLK_SPI0_LFIN_EB]	= &spi0_lfin_eb.common.hw,
		[CLK_SPI1_LFIN_EB]	= &spi1_lfin_eb.common.hw,
		[CLK_SPI2_LFIN_EB]	= &spi2_lfin_eb.common.hw,
		[CLK_SPI3_LFIN_EB]	= &spi3_lfin_eb.common.hw,
		[CLK_SPI4_LFIN_EB]	= &spi4_lfin_eb.common.hw,
		[CLK_SPI5_LFIN_EB]	= &spi5_lfin_eb.common.hw,
		[CLK_CE_SEC_EB]		= &ce_sec_eb.common.hw,
		[CLK_CE_PUB_EB]		= &ce_pub_eb.common.hw,
		[CLK_NIC400_EB]		= &nic400_eb.common.hw,
		[CLK_UART4_EB]		= &uart4_eb.common.hw,
		[CLK_UART5_EB]		= &uart5_eb.common.hw,
		[CLK_UART6_EB]		= &uart6_eb.common.hw,
		[CLK_UART7_EB]		= &uart7_eb.common.hw,
		[CLK_UART8_EB]		= &uart8_eb.common.hw,
		[CLK_UART9_EB]		= &uart9_eb.common.hw,
		[CLK_UART10_EB]		= &uart10_eb.common.hw,
		[CLK_SPI3_EB]		= &spi3_eb.common.hw,
		[CLK_SPI4_EB]		= &spi4_eb.common.hw,
		[CLK_SPI5_EB]		= &spi5_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static struct sprd_reset_map uis8520_ap_apb_resets[] = {
	[RESET_AP_APB_IIS0_SOFT_RST]	= { 0x0004, BIT(1), 0x1000 },
	[RESET_AP_APB_IIS1_SOFT_RST]	= { 0x0004, BIT(2), 0x1000 },
	[RESET_AP_APB_IIS2_SOFT_RST]	= { 0x0004, BIT(3), 0x1000 },
	[RESET_AP_APB_SPI0_SOFT_RST]	= { 0x0004, BIT(4), 0x1000 },
	[RESET_AP_APB_SPI1_SOFT_RST]	= { 0x0004, BIT(5), 0x1000 },
	[RESET_AP_APB_SPI2_SOFT_RST]	= { 0x0004, BIT(6), 0x1000 },
	[RESET_AP_APB_UART3_SOFT_RST]	= { 0x0004, BIT(7), 0x1000 },
	[RESET_AP_APB_UART0_SOFT_RST]	= { 0x0004, BIT(13), 0x1000 },
	[RESET_AP_APB_UART1_SOFT_RST]	= { 0x0004, BIT(14), 0x1000 },
	[RESET_AP_APB_UART2_SOFT_RST]	= { 0x0004, BIT(15), 0x1000 },
	[RESET_AP_APB_CE_SEC_SOFT_RST]	= { 0x0004, BIT(20), 0x1000 },
	[RESET_AP_APB_CE_PUB_SOFT_RST]	= { 0x0004, BIT(21), 0x1000 },
	[RESET_AP_APB_AP_DVFS_SOFT_RST]	= { 0x0004, BIT(22), 0x1000 },
	[RESET_AP_APB_SPI3_SOFT_RST]	= { 0x0018, BIT(0), 0x1000 },
	[RESET_AP_APB_SPI4_SOFT_RST]	= { 0x0018, BIT(1), 0x1000 },
	[RESET_AP_APB_SPI5_SOFT_RST]	= { 0x0018, BIT(2), 0x1000 },
	[RESET_AP_APB_UART4_SOFT_RST]	= { 0x0018, BIT(3), 0x1000 },
	[RESET_AP_APB_UART5_SOFT_RST]	= { 0x0018, BIT(4), 0x1000 },
	[RESET_AP_APB_UART6_SOFT_RST]	= { 0x0018, BIT(5), 0x1000 },
	[RESET_AP_APB_UART7_SOFT_RST]	= { 0x0018, BIT(6), 0x1000 },
	[RESET_AP_APB_UART8_SOFT_RST]	= { 0x0018, BIT(7), 0x1000 },
	[RESET_AP_APB_UART9_SOFT_RST]	= { 0x0018, BIT(8), 0x1000 },
	[RESET_AP_APB_UART10_SOFT_RST]	= { 0x0018, BIT(9), 0x1000 },
};

static struct sprd_clk_desc uis8520_apapb_gate_desc = {
	.clk_clks	= uis8520_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_apapb_gate),
	.hw_clks	= &uis8520_apapb_gate_hws,
	.resets		= uis8520_ap_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_ap_apb_resets),
};

/* ap ahb gates */
/* ap related gate clocks configure CLK_IGNORE_UNUSED because they are
 * configured as enabled state to support display working during uboot phase.
 * if their clocks are gated during kernel phase, it will affect the normal
 * working of display..
 */
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_eb, "sdio0-eb", "ext-26m", 0x0,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio1_eb, "sdio1-eb", "ext-26m", 0x0,
			0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio2_eb, "sdio2-eb", "ext-26m", 0x0,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_eb, "emmc-eb", "ext-26m", 0x0,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dma_pub_eb, "dma-pub-eb", "ext-26m", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ufs_eb, "ufs-eb", "ext-26m", 0x0,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ckg_eb, "ckg-eb", "ext-26m", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(busmon_clk_eb, "busmon-clk-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap2emc_eb, "ap2emc-eb", "ext-26m", 0x0,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c0_eb, "i2c0-eb", "ext-26m", 0x0,
			0x1000, BIT(10), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c1_eb, "i2c1-eb", "ext-26m", 0x0,
			0x1000, BIT(11), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c2_eb, "i2c2-eb", "ext-26m", 0x0,
			0x1000, BIT(12), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c3_eb, "i2c3-eb", "ext-26m", 0x0,
			0x1000, BIT(13), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c4_eb, "i2c4-eb", "ext-26m", 0x0,
			0x1000, BIT(14), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c5_eb, "i2c5-eb", "ext-26m", 0x0,
			0x1000, BIT(15), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c6_eb, "i2c6-eb", "ext-26m", 0x0,
			0x1000, BIT(16), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c7_eb, "i2c7-eb", "ext-26m", 0x0,
			0x1000, BIT(17), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c8_eb, "i2c8-eb", "ext-26m", 0x0,
			0x1000, BIT(18), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c9_eb, "i2c9-eb", "ext-26m", 0x0,
			0x1000, BIT(19), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c0_eb_sec, "i2c0-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(20), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c1_eb_sec, "i2c1-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(21), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c2_eb_sec, "i2c2-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(22), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c3_eb_sec, "i2c3-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(23), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c4_eb_sec, "i2c4-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(24), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c5_eb_sec, "i2c5-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(25), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c6_eb_sec, "i2c6-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(26), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c7_eb_sec, "i2c7-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(27), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c8_eb_sec, "i2c8-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(28), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c9_eb_sec, "i2c9-eb-sec", "ext-26m", 0x0,
			0x1000, BIT(29), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_eb, "nandc-eb", "ext-26m", 0x34,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c_s0_eb_sec, "i2c-s0-eb-sec", "ext-26m", 0x34,
			0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c_s1_eb_sec, "i2c-s1-eb-sec", "ext-26m", 0x34,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c_s0_eb_non_sec, "i2c-s0-eb-non-sec", "ext-26m", 0x34,
			0x1000, BIT(10), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c_s1_eb_non_sec, "i2c-s1-eb-non-sec", "ext-26m", 0x34,
			0x1000, BIT(11), 0, 0);
static struct sprd_clk_common *uis8520_apahb_gate[] = {
	/* address base is 0x20000000 */
	&sdio0_eb.common,
	&sdio1_eb.common,
	&sdio2_eb.common,
	&emmc_eb.common,
	&dma_pub_eb.common,
	&ufs_eb.common,
	&ckg_eb.common,
	&busmon_clk_eb.common,
	&ap2emc_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&i2c3_eb.common,
	&i2c4_eb.common,
	&i2c5_eb.common,
	&i2c6_eb.common,
	&i2c7_eb.common,
	&i2c8_eb.common,
	&i2c9_eb.common,
	&i2c0_eb_sec.common,
	&i2c1_eb_sec.common,
	&i2c2_eb_sec.common,
	&i2c3_eb_sec.common,
	&i2c4_eb_sec.common,
	&i2c5_eb_sec.common,
	&i2c6_eb_sec.common,
	&i2c7_eb_sec.common,
	&i2c8_eb_sec.common,
	&i2c9_eb_sec.common,
	&nandc_eb.common,
	&i2c_s0_eb_sec.common,
	&i2c_s1_eb_sec.common,
	&i2c_s0_eb_non_sec.common,
	&i2c_s1_eb_non_sec.common,
};

static struct clk_hw_onecell_data uis8520_apahb_gate_hws = {
	.hws	= {
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO2_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_DMA_PUB_EB]	= &dma_pub_eb.common.hw,
		[CLK_UFS_EB]		= &ufs_eb.common.hw,
		[CLK_CKG_EB]		= &ckg_eb.common.hw,
		[CLK_BUSMON_CLK_EB]	= &busmon_clk_eb.common.hw,
		[CLK_AP2EMC_EB]		= &ap2emc_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_I2C3_EB]		= &i2c3_eb.common.hw,
		[CLK_I2C4_EB]		= &i2c4_eb.common.hw,
		[CLK_I2C5_EB]		= &i2c5_eb.common.hw,
		[CLK_I2C6_EB]		= &i2c6_eb.common.hw,
		[CLK_I2C7_EB]		= &i2c7_eb.common.hw,
		[CLK_I2C8_EB]		= &i2c8_eb.common.hw,
		[CLK_I2C9_EB]		= &i2c9_eb.common.hw,
		[CLK_I2C0_EB_SEC]	= &i2c0_eb_sec.common.hw,
		[CLK_I2C1_EB_SEC]	= &i2c1_eb_sec.common.hw,
		[CLK_I2C2_EB_SEC]	= &i2c2_eb_sec.common.hw,
		[CLK_I2C3_EB_SEC]	= &i2c3_eb_sec.common.hw,
		[CLK_I2C4_EB_SEC]	= &i2c4_eb_sec.common.hw,
		[CLK_I2C5_EB_SEC]	= &i2c5_eb_sec.common.hw,
		[CLK_I2C6_EB_SEC]	= &i2c6_eb_sec.common.hw,
		[CLK_I2C7_EB_SEC]	= &i2c7_eb_sec.common.hw,
		[CLK_I2C8_EB_SEC]	= &i2c8_eb_sec.common.hw,
		[CLK_I2C9_EB_SEC]	= &i2c9_eb_sec.common.hw,
		[CLK_NANDC_EB]		= &nandc_eb.common.hw,
		[CLK_I2C_S0_EB_SEC]	= &i2c_s0_eb_sec.common.hw,
		[CLK_I2C_S1_EB_SEC]	= &i2c_s1_eb_sec.common.hw,
		[CLK_I2C_S0_EB_NON_SEC]	= &i2c_s0_eb_non_sec.common.hw,
		[CLK_I2C_S1_EB_NON_SEC]	= &i2c_s1_eb_non_sec.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static struct sprd_reset_map uis8520_ap_ahb_resets[] = {
	[RESET_AP_AHB_SDIO_SOFT_RST]	= { 0x0004, BIT(0), 0x1000 },
	[RESET_AP_AHB_EMMC_SOFT_RST]	= { 0x0004, BIT(3), 0x1000 },
	[RESET_AP_AHB_DMA_SOFT_RST]	= { 0x0004, BIT(8), 0x1000 },
	[RESET_AP_AHB_I2C0_SOFT_RST]	= { 0x0004, BIT(16), 0x1000 },
	[RESET_AP_AHB_I2C1_SOFT_RST]	= { 0x0004, BIT(17), 0x1000 },
	[RESET_AP_AHB_I2C2_SOFT_RST]	= { 0x0004, BIT(18), 0x1000 },
	[RESET_AP_AHB_I2C3_SOFT_RST]	= { 0x0004, BIT(19), 0x1000 },
	[RESET_AP_AHB_I2C4_SOFT_RST]	= { 0x0004, BIT(20), 0x1000 },
	[RESET_AP_AHB_I2C5_SOFT_RST]	= { 0x0004, BIT(21), 0x1000 },
	[RESET_AP_AHB_I2C6_SOFT_RST]	= { 0x0004, BIT(22), 0x1000 },
	[RESET_AP_AHB_I2C7_SOFT_RST]	= { 0x0004, BIT(23), 0x1000 },
	[RESET_AP_AHB_I2C8_SOFT_RST]	= { 0x0004, BIT(24), 0x1000 },
	[RESET_AP_AHB_I2C9_SOFT_RST]	= { 0x0004, BIT(25), 0x1000 },
	[RESET_AP_AHB_NANDC_SOFT_RST]	= { 0x0038, BIT(7), 0x1000 },
	[RESET_AP_AHB_I2C_S0_SOFT_RST]	= { 0x0038, BIT(10), 0x1000 },
	[RESET_AP_AHB_I2C_S1_SOFT_RST]	= { 0x0038, BIT(11), 0x1000 },
};

static struct sprd_clk_desc uis8520_apahb_gate_desc = {
	.clk_clks	= uis8520_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_apahb_gate),
	.hw_clks	= &uis8520_apahb_gate_hws,
	.resets		= uis8520_ap_ahb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_ap_ahb_resets),
};

/* ap clks */
static const struct clk_parent_data ap_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_64m.hw },
	{ .hw = &tgpll_96m.hw },
	{ .hw = &tgpll_128m.hw },
};
static SPRD_MUX_CLK_DATA(ap_apb, "ap-apb", ap_apb_parents, 0x28,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data ap_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_76m8.hw },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_256m.hw },
};
static SPRD_MUX_CLK_DATA(ap_axi, "ap-axi", ap_axi_parents, 0x34,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data nandc_ecc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_256m.hw },
	{ .hw = &tgpll_307m2.hw },
};

static SPRD_COMP_CLK_DATA_OFFSET(nandc_ecc, "nandc-ecc", nandc_ecc_parents,
			    0x40, 0, 2, 0, 3, 0);

static const struct clk_parent_data ap_uart_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_51m2.hw },
	{ .hw = &tgpll_96m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart0, "ap-uart0", ap_uart_parents,
			    0x4c, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart1, "ap-uart1", ap_uart_parents,
			    0x58, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart2, "ap-uart2", ap_uart_parents,
			    0x64, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart3, "ap-uart3", ap_uart_parents,
			    0x70, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart4, "ap-uart4", ap_uart_parents,
			    0x7c, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart5, "ap-uart5", ap_uart_parents,
			    0x88, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart6, "ap-uart6", ap_uart_parents,
			    0x94, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart7, "ap-uart7", ap_uart_parents,
			    0xa0, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart8, "ap-uart8", ap_uart_parents,
			    0xac, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart9, "ap-uart9", ap_uart_parents,
			    0xb8, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_uart10, "ap-uart10", ap_uart_parents,
			    0xc4, 0, 2, 0, 3, 0);


static const struct clk_parent_data i2c_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_51m2.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c0, "ap-i2c0", i2c_parents,
			    0xd8, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c1, "ap-i2c1", i2c_parents,
			    0xdc, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c2, "ap-i2c2", i2c_parents,
			    0xe8, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c3, "ap-i2c3", i2c_parents,
			    0xf4, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c4, "ap-i2c4", i2c_parents,
			    0x100, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c5, "ap-i2c5", i2c_parents,
			    0x10c, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c6, "ap-i2c6", i2c_parents,
			    0x118, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c7, "ap-i2c7", i2c_parents,
			    0x124, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c8, "ap-i2c8", i2c_parents,
			    0x130, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_i2c9, "ap-i2c9", i2c_parents,
			    0x13c, 0, 2, 0, 3, 0);

static const struct clk_parent_data i2c_slv_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_38m4.hw },
};

static SPRD_COMP_CLK_DATA_OFFSET(i2c_slv1, "i2c-slv1", i2c_slv_parents,
			    0x148, 0, 1, 0, 2, 0);

static SPRD_COMP_CLK_DATA_OFFSET(i2c_slv2, "i2c-slv2", i2c_slv_parents,
			    0x154, 0, 1, 0, 2, 0);

static const struct clk_parent_data iis_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_192m.hw },
	{ .hw = &tgpll_256m.hw },
	{ .hw = &tgpll_512m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(ap_iis0, "ap-iis0", iis_parents,
			    0x160, 0, 3, 0, 6, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_iis1, "ap-iis1", iis_parents,
			    0x16c, 0, 3, 0, 6, 0);
static SPRD_COMP_CLK_DATA_OFFSET(ap_iis2, "ap-iis2", iis_parents,
			    0x178, 0, 3, 0, 6, 0);

static const struct clk_parent_data ap_ce_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_96m.hw },
	{ .hw = &tgpll_192m.hw },
	{ .hw = &tgpll_256m.hw },
};
static SPRD_MUX_CLK_DATA(ap_ce, "ap-ce", ap_ce_parents, 0x190,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data sdio_parents[] = {
	{ .hw = &clk_1m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_307m2.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &rpll_390m.hw },
	{ .hw = &v4nrpll_409m6.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(emmc_2x, "emmc-2x", sdio_parents,
			    0x19c, 0, 3, 0, 11, 0);
static SPRD_DIV_CLK_HW(emmc_1x, "emmc-1x", &emmc_2x.common.hw, 0x1a4,
		    0, 1, 0);

static const struct clk_parent_data nandc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_170m6.hw },
	{ .hw = &tgpll_219m4.hw },
	{ .hw = &v4nrpll_245m76.hw },
	{ .hw = &rpll_260m.hw },
	{ .hw = &tgpll_307m2.hw },
	{ .hw = &rpll_390m.hw },
};

static SPRD_COMP_CLK_DATA_OFFSET(nandc_2x, "nandc-2x", nandc_parents,
			    0x1b4, 0, 4, 0, 4, 0);

static SPRD_DIV_CLK_HW(nandc_1x, "nandc-1x", &nandc_2x.common.hw, 0x1bc,
			    0, 1, 0);

static const struct clk_parent_data disp_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_96m.hw },
	{ .hw = &tgpll_128m.hw },
};

static SPRD_COMP_CLK_DATA_OFFSET(disp, "disp", disp_parents,
			    0x1cc, 0, 2, 0, 7, 0);

static struct sprd_clk_common *uis8520_ap_clks[] = {
	/* address base is 0x20010000 */
	&ap_apb.common,
	&ap_axi.common,
	&nandc_ecc.common,
	&ap_uart0.common,
	&ap_uart1.common,
	&ap_uart2.common,
	&ap_uart3.common,
	&ap_uart4.common,
	&ap_uart5.common,
	&ap_uart6.common,
	&ap_uart7.common,
	&ap_uart8.common,
	&ap_uart9.common,
	&ap_uart10.common,
	&ap_i2c0.common,
	&ap_i2c1.common,
	&ap_i2c2.common,
	&ap_i2c3.common,
	&ap_i2c4.common,
	&ap_i2c5.common,
	&ap_i2c6.common,
	&ap_i2c7.common,
	&ap_i2c8.common,
	&ap_i2c9.common,
	&i2c_slv1.common,
	&i2c_slv2.common,
	&ap_iis0.common,
	&ap_iis1.common,
	&ap_iis2.common,
	&ap_ce.common,
	&emmc_2x.common,
	&emmc_1x.common,
	&nandc_2x.common,
	&nandc_1x.common,
	&disp.common,
};

static struct clk_hw_onecell_data uis8520_ap_clk_hws = {
	.hws	= {
		[CLK_AP_APB]		= &ap_apb.common.hw,
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_NANDC_ECC]		= &nandc_ecc.common.hw,
		[CLK_AP_UART0]		= &ap_uart0.common.hw,
		[CLK_AP_UART1]		= &ap_uart1.common.hw,
		[CLK_AP_UART2]		= &ap_uart2.common.hw,
		[CLK_AP_UART3]		= &ap_uart3.common.hw,
		[CLK_AP_UART4]		= &ap_uart4.common.hw,
		[CLK_AP_UART5]		= &ap_uart5.common.hw,
		[CLK_AP_UART6]		= &ap_uart6.common.hw,
		[CLK_AP_UART7]		= &ap_uart7.common.hw,
		[CLK_AP_UART8]		= &ap_uart8.common.hw,
		[CLK_AP_UART9]		= &ap_uart9.common.hw,
		[CLK_AP_UART10]		= &ap_uart10.common.hw,
		[CLK_AP_I2C0]		= &ap_i2c0.common.hw,
		[CLK_AP_I2C1]		= &ap_i2c1.common.hw,
		[CLK_AP_I2C2]		= &ap_i2c2.common.hw,
		[CLK_AP_I2C3]		= &ap_i2c3.common.hw,
		[CLK_AP_I2C4]		= &ap_i2c4.common.hw,
		[CLK_AP_I2C5]		= &ap_i2c5.common.hw,
		[CLK_AP_I2C6]		= &ap_i2c6.common.hw,
		[CLK_AP_I2C7]		= &ap_i2c7.common.hw,
		[CLK_AP_I2C8]		= &ap_i2c8.common.hw,
		[CLK_AP_I2C9]		= &ap_i2c9.common.hw,
		[CLK_I2C_SLV1]		= &i2c_slv1.common.hw,
		[CLK_I2C_SLV2]		= &i2c_slv2.common.hw,
		[CLK_AP_IIS0]		= &ap_iis0.common.hw,
		[CLK_AP_IIS1]		= &ap_iis1.common.hw,
		[CLK_AP_IIS2]		= &ap_iis2.common.hw,
		[CLK_AP_CE]		= &ap_ce.common.hw,
		[CLK_EMMC_2X]		= &emmc_2x.common.hw,
		[CLK_EMMC_1X]		= &emmc_1x.common.hw,
		[CLK_NANDC_2X]		= &nandc_2x.common.hw,
		[CLK_NANDC_1X]		= &nandc_1x.common.hw,
		[CLK_DISP]		= &disp.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static struct sprd_clk_desc uis8520_ap_clk_desc = {
	.clk_clks	= uis8520_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(uis8520_ap_clks),
	.hw_clks	= &uis8520_ap_clk_hws,
};

/* aon apb gates */
static SPRD_SC_GATE_CLK_FW_NAME(rc100m_cal_eb, "rc100m-cal-eb", "ext-26m", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rfti_eb, "rfti-eb", "ext-26m", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(djtag_eb, "djtag-eb", "ext-26m", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux0_eb, "aux0-eb", "ext-26m", 0x0,
			0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux1_eb, "aux1-eb", "ext-26m", 0x0,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux2_eb, "aux2-eb", "ext-26m", 0x0,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(probe_eb, "probe-eb", "ext-26m", 0x0,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_dap_eb, "apcpu-dap-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_cssys_eb, "aon-cssys-eb", "ext-26m", 0x0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cssys_apb_eb, "cssys-apb-eb", "ext-26m", 0x0,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cssys_pub_eb, "cssys-pub-eb", "ext-26m", 0x0,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux4_eb, "aux4-eb", "ext-26m", 0x0,
			0x1000, BIT(24), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux5_eb, "aux5-eb", "ext-26m", 0x0,
			0x1000, BIT(25), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux6_eb, "aux6-eb", "ext-26m", 0x0,
			0x1000, BIT(26), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(efuse_eb, "efuse-eb", "ext-26m", 0x4,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gpio_eb, "gpio-eb", "ext-26m", 0x4,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mbox_eb, "mbox-eb", "ext-26m", 0x4,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(kpd_eb, "kpd-eb", "ext-26m", 0x4,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_syst_eb, "aon-syst-eb", "ext-26m", 0x4,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_syst_eb, "ap-syst-eb", "ext-26m", 0x4,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_tmr_eb, "aon-tmr-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_dvfs_top_eb, "aon-dvfs-top-eb", "ext-26m", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_usb2_top_eb, "aon-usb2-top-eb", "ext-26m", 0x4,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(otg_phy_eb, "otg-phy-eb", "ext-26m", 0x4,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(splk_eb, "splk-eb", "ext-26m", 0x4,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pin_eb, "pin-eb", "ext-26m", 0x4,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ana_eb, "ana-eb", "ext-26m", 0x4,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_busmon_eb,  "apcpu-busmon-eb", "ext-26m", 0x4,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_ts0_eb, "apcpu-ts0-eb", "ext-26m", 0x4,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apb_busmon_eb,  "apb-busmon-eb", "ext-26m", 0x4,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_iis_eb, "aon-iis-eb", "ext-26m", 0x4,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(scc_eb, "scc-eb", "ext-26m", 0x4,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux3_eb, "aux3-eb", "ext-26m", 0x4,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(thm0_eb, "thm0-eb", "ext-26m", 0x8,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(thm1_eb, "thm1-eb", "ext-26m", 0x8,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(audcp_intc_eb, "audcp-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pmu_eb, "pmu-eb", "ext-26m", 0x8,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(adi_eb, "adi-eb", "ext-26m", 0x8,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_eb, "eic-eb", "ext-26m", 0x8,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc0_eb, "ap-intc0-eb", "ext-26m", 0x8,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc1_eb, "ap-intc1-eb", "ext-26m", 0x8,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc2_eb, "ap-intc2-eb", "ext-26m", 0x8,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc3_eb, "ap-intc3-eb", "ext-26m", 0x8,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc4_eb, "ap-intc4-eb", "ext-26m", 0x8,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc5_eb, "ap-intc5-eb", "ext-26m", 0x8,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc6_eb, "ap-intc6-eb", "ext-26m", 0x8,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc7_eb, "ap-intc7-eb", "ext-26m", 0x8,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pscp_intc_eb, "pscp-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(phycp_intc_eb, "phycp-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr0_eb, "ap-tmr0-eb", "ext-26m", 0x8,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr1_eb, "ap-tmr1-eb", "ext-26m", 0x8,
			0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr2_eb, "ap-tmr2-eb", "ext-26m", 0x8,
			0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_uart_eb, "ap-uart-eb", "ext-26m", 0x8,
			0x1000, BIT(28), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_wdg_eb, "ap-wdg-eb", "ext-26m", 0x8,
			0x1000, BIT(29), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_wdg_eb, "apcpu-wdg-eb", "ext-26m", 0x8,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(arch_rtc_eb, "arch-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(kpd_rtc_eb, "kpd-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_syst_rtc_eb, "ap-syst-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_rtc_eb, "eic-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_rtcdv5_eb, "eic-rtcdv5-eb", "ext-26m", 0x18,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ac_wdg_rtc_eb, "ac-wdg-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dcxo_lc_rtc_eb, "dcxo-lc-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(bb_cal_rtc_eb, "bb-cal-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(djtag_tck_en, "djtag-tck-en", "ext-26m", 0x138,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dmc_ref_eb, "dmc-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(otg_ref_eb, "otg-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rc100m_ref_eb, "rc100m-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rc100m_fdk_eb, "rc100m-fdk-eb", "ext-26m", 0x138,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(debounce_eb, "debounce-eb", "ext-26m", 0x138,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(det_32k_eb, "det-32k-eb", "ext-26m", 0x138,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(debug_ts_en, "debug-ts-en", "ext-26m", 0x13c,
			0x1000, BIT(18), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(access_aud_en, "access-aud-en", "ext-26m", 0x14c,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_access_xge_en, "ap-access-xge-en", "ext-26m", 0xcd8,
			0x1000, BIT(8), 0, 0);

static const struct clk_parent_data aux_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .fw_name = "clk-52m-a2d" },
	{ .hw = &audpll_19m2.hw },
	{ .hw = &audpll_12m28.hw },
	{ .hw = &audpll_24m57.hw },
	{ .hw = &tgpll_48m.hw },
	{ .fw_name = "tgpll-32m" },
	{ .fw_name = "tgpll-16m" },
	{ .hw = &tgpll_12m.hw },
	{ .fw_name = "tgpll-8m" },
	{ .fw_name = "tgpll-4m" },
	{ .hw = &clk_13m.hw },
	{ .fw_name = "clk-26m-a2d" },
	{ .fw_name = "phyr8pll-38m18" },
	{ .fw_name = "psr8pll-38m18" },
	{ .fw_name = "ethpll-fout0-31m25" },
	{ .fw_name = "ethpll-fout1-19m5" },
	{ .fw_name = "ethpll-fout2-12m5" },
	{ .fw_name = "ethmplla-word-32m2" },
	{ .fw_name = "ethrxo-32m2" },
	{ .fw_name = "tsnpll-50m" },
	{ .fw_name = "tsnpll-40m62" },
	{ .fw_name = "dftpll-30m87" },
	{ .hw = &v4nrpll_38m4.hw },
	{ .fw_name = "lvdsrfpll-58m5" },
	{ .fw_name = "lvdsrf-rx-58m5" },
	{ .fw_name = "lvdsrf-tx-58m5" },
	{ .hw = &rpll_26m.hw },
	{ .fw_name = "mplllit-24m98" },
	{ .fw_name = "mplls-32m5" },
};

static SPRD_COMP_CLK_DATA(aux0_clk, "aux0-clk", aux_parents, 0x240,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(aux1_clk, "aux1-clk", aux_parents, 0x244,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(aux2_clk, "aux2-clk", aux_parents, 0x248,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(probe_clk, "probe-clk", aux_parents, 0x24c,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(aux3_clk, "aux3-clk", aux_parents, 0xd20,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(aux4_clk, "aux4-clk", aux_parents, 0x260,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(aux5_clk, "aux5-clk", aux_parents, 0x264,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK_DATA(aux6_clk, "aux6-clk", aux_parents, 0x268,
		    6, 6, 0, 6, 0);

static struct sprd_clk_common *uis8520_aon_gate[] = {
	/* address base is 0x64900000 */
	&rc100m_cal_eb.common,
	&rfti_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&apcpu_dap_eb.common,
	&aon_cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&aux4_eb.common,
	&aux5_eb.common,
	&aux6_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&aon_dvfs_top_eb.common,
	&aon_usb2_top_eb.common,
	&otg_phy_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&apcpu_busmon_eb.common,
	&apcpu_ts0_eb.common,
	&apb_busmon_eb.common,
	&aon_iis_eb.common,
	&scc_eb.common,
	&aux3_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&audcp_intc_eb.common,
	&pmu_eb.common,
	&adi_eb.common,
	&eic_eb.common,
	&ap_intc0_eb.common,
	&ap_intc1_eb.common,
	&ap_intc2_eb.common,
	&ap_intc3_eb.common,
	&ap_intc4_eb.common,
	&ap_intc5_eb.common,
	&ap_intc6_eb.common,
	&ap_intc7_eb.common,
	&pscp_intc_eb.common,
	&phycp_intc_eb.common,
	&ap_tmr0_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&ap_uart_eb.common,
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&arch_rtc_eb.common,
	&kpd_rtc_eb.common,
	&aon_syst_rtc_eb.common,
	&ap_syst_rtc_eb.common,
	&aon_tmr_rtc_eb.common,
	&eic_rtc_eb.common,
	&eic_rtcdv5_eb.common,
	&ap_wdg_rtc_eb.common,
	&ac_wdg_rtc_eb.common,
	&ap_tmr0_rtc_eb.common,
	&ap_tmr1_rtc_eb.common,
	&ap_tmr2_rtc_eb.common,
	&dcxo_lc_rtc_eb.common,
	&bb_cal_rtc_eb.common,
	&djtag_tck_en.common,
	&dmc_ref_eb.common,
	&otg_ref_eb.common,
	&rc100m_ref_eb.common,
	&rc100m_fdk_eb.common,
	&debounce_eb.common,
	&det_32k_eb.common,
	&debug_ts_en.common,
	&access_aud_en.common,
	&ap_access_xge_en.common,
	&aux0_clk.common,
	&aux1_clk.common,
	&aux2_clk.common,
	&probe_clk.common,
	&aux3_clk.common,
	&aux4_clk.common,
	&aux5_clk.common,
	&aux6_clk.common,
};

static struct clk_hw_onecell_data uis8520_aon_gate_hws = {
	.hws	= {
		[CLK_RC100M_CAL_EB]	= &rc100m_cal_eb.common.hw,
		[CLK_RFTI_EB]		= &rfti_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_APCPU_DAP_EB]	= &apcpu_dap_eb.common.hw,
		[CLK_AON_CSSYS_EB]	= &aon_cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB]	= &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB]	= &cssys_pub_eb.common.hw,
		[CLK_AUX4_EB]		= &aux4_eb.common.hw,
		[CLK_AUX5_EB]		= &aux5_eb.common.hw,
		[CLK_AUX6_EB]		= &aux6_eb.common.hw,
		[CLK_EFUSE_EB]		= &efuse_eb.common.hw,
		[CLK_GPIO_EB]		= &gpio_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_KPD_EB]		= &kpd_eb.common.hw,
		[CLK_AON_SYST_EB]	= &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB]	= &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB]	= &aon_tmr_eb.common.hw,
		[CLK_AON_DVFS_TOP_EB]	= &aon_dvfs_top_eb.common.hw,
		[CLK_AON_USB2_TOP_EB]	= &aon_usb2_top_eb.common.hw,
		[CLK_OTG_PHY_EB]	= &otg_phy_eb.common.hw,
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_PIN_EB]		= &pin_eb.common.hw,
		[CLK_ANA_EB]		= &ana_eb.common.hw,
		[CLK_APCPU_BUSMON_EB]	= &apcpu_busmon_eb.common.hw,
		[CLK_APCPU_TS0_EB]	= &apcpu_ts0_eb.common.hw,
		[CLK_APB_BUSMON_EB]	= &apb_busmon_eb.common.hw,
		[CLK_AON_IIS_EB]	= &aon_iis_eb.common.hw,
		[CLK_SCC_EB]		= &scc_eb.common.hw,
		[CLK_AUX3_EB]		= &aux3_eb.common.hw,
		[CLK_THM0_EB]		= &thm0_eb.common.hw,
		[CLK_THM1_EB]		= &thm1_eb.common.hw,
		[CLK_AUDCP_INTC_EB]	= &audcp_intc_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_ADI_EB]		= &adi_eb.common.hw,
		[CLK_EIC_EB]		= &eic_eb.common.hw,
		[CLK_AP_INTC0_EB]	= &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB]	= &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB]	= &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB]	= &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB]	= &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB]	= &ap_intc5_eb.common.hw,
		[CLK_AP_INTC6_EB]	= &ap_intc6_eb.common.hw,
		[CLK_AP_INTC7_EB]	= &ap_intc7_eb.common.hw,
		[CLK_PSCP_INTC_EB]	= &pscp_intc_eb.common.hw,
		[CLK_PHYCP_INTC_EB]	= &phycp_intc_eb.common.hw,
		[CLK_AP_TMR0_EB]	= &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_AP_UART_EB]	= &ap_uart_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB]	= &apcpu_wdg_eb.common.hw,
		[CLK_ARCH_RTC_EB]	= &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB]	= &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB]	= &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB]	= &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB]	= &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB]	= &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB]	= &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB]	= &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB]	= &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB]	= &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB]	= &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB]	= &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB]	= &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB]	= &bb_cal_rtc_eb.common.hw,
		[CLK_DJTAG_TCK_EN]	= &djtag_tck_en.common.hw,
		[CLK_DMC_REF_EB]	= &dmc_ref_eb.common.hw,
		[CLK_OTG_REF_EB]	= &otg_ref_eb.common.hw,
		[CLK_RC100M_REF_EB]	= &rc100m_ref_eb.common.hw,
		[CLK_RC100M_FDK_EB]	= &rc100m_fdk_eb.common.hw,
		[CLK_DEBOUNCE_EB]	= &debounce_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_DEBUG_TS_EN]	= &debug_ts_en.common.hw,
		[CLK_ACCESS_AUD_EN]	= &access_aud_en.common.hw,
		[CLK_AP_ACCESS_XGE_EN]	= &ap_access_xge_en.common.hw,
		[CLK_AUX0]		= &aux0_clk.common.hw,
		[CLK_AUX1]		= &aux1_clk.common.hw,
		[CLK_AUX2]		= &aux2_clk.common.hw,
		[CLK_PROBE]		= &probe_clk.common.hw,
		[CLK_AUX3]		= &aux3_clk.common.hw,
		[CLK_AUX4]		= &aux4_clk.common.hw,
		[CLK_AUX5]		= &aux5_clk.common.hw,
		[CLK_AUX6]		= &aux6_clk.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static struct sprd_reset_map uis8520_aon_apb_resets[] = {
	[RESET_AON_APB_RC100M_CAL_SOFT_RST]		= { 0x000c, BIT(0), 0x1000 },
	[RESET_AON_APB_DCXO_LC_SOFT_RST]		= { 0x000c, BIT(2), 0x1000 },
	[RESET_AON_APB_DAP_MTX_SOFT_RST]		= { 0x000c, BIT(6), 0x1000 },
	[RESET_AON_APB_RC60M_CAL_SOFT_RST]		= { 0x000c, BIT(12), 0x1000 },
	[RESET_AON_APB_RC6M_CAL_SOFT_RST]		= { 0x000C, BIT(13), 0x1000 },
	[RESET_AON_APB_AON_SW_RFFE_SOFT_RST]		= { 0x000c, BIT(14), 0x1000 },
	[RESET_AON_APB_LVDS_PHY_SOFT_RST]		= { 0x000c, BIT(15), 0x1000 },
	[RESET_AON_APB_LCM_RST]				= { 0x000c, BIT(16), 0x1000 },
	[RESET_AON_APB_EFUSE_SOFT_RST]			= { 0x0010, BIT(0), 0x1000 },
	[RESET_AON_APB_GPIO_SOFT_RST]			= { 0x0010, BIT(1), 0x1000 },
	[RESET_AON_APB_MBOX_SOFT_RST]			= { 0x0010, BIT(2), 0x1000 },
	[RESET_AON_APB_KPD_SOFT_RST]			= { 0x0010, BIT(3), 0x1000 },
	[RESET_AON_APB_AON_SYST_SOFT_RST]		= { 0x0010, BIT(4), 0x1000 },
	[RESET_AON_APB_AP_SYST_SOFT_RST]		= { 0x0010, BIT(5), 0x1000 },
	[RESET_AON_APB_DVFS_TOP_SOFT_RST]		= { 0x0010, BIT(7), 0x1000 },
	[RESET_AON_APB_OTG_UTMI_SOFT_RST]		= { 0x0010, BIT(8), 0x1000 },
	[RESET_AON_APB_OTG_PHY_SOFT_RST]		= { 0x0010, BIT(9), 0x1000 },
	[RESET_AON_APB_SPLK_SOFT_RST]			= { 0x0010, BIT(10), 0x1000 },
	[RESET_AON_APB_PIN_SOFT_RST]			= { 0x0010, BIT(11), 0x1000 },
	[RESET_AON_APB_ANA_SOFT_RST]			= { 0x0010, BIT(12), 0x1000 },
	[RESET_AON_APB_CKG_SOFT_RST]			= { 0x0010, BIT(13), 0x1000 },
	[RESET_AON_APB_APCPU_TS0_SOFT_RST]		= { 0x0010, BIT(17), 0x1000 },
	[RESET_AON_APB_DEBUG_FILTER_SOFT_RST]		= { 0x0010, BIT(18), 0x1000 },
	[RESET_AON_APB_AON_IIS_SOFT_RST]		= { 0x0010, BIT(19), 0x1000 },
	[RESET_AON_APB_SCC_SOFT_RST]			= { 0x0010, BIT(20), 0x1000 },
	[RESET_AON_APB_THM0_OVERHEAT_SOFT_RST]		= { 0x0010, BIT(23), 0x1000 },
	[RESET_AON_APB_THM1_OVERHEAT_SOFT_RST]		= { 0x0010, BIT(24), 0x1000 },
	[RESET_AON_APB_THM0_SOFT_RST]			= { 0x0014, BIT(0), 0x1000 },
	[RESET_AON_APB_THM1_SOFT_RST]			= { 0x0014, BIT(1), 0x1000 },
	[RESET_AON_APB_PSCP_SIM0_AON_TOP_SOFT_RST]	= { 0x0014, BIT(4), 0x1000 },
	[RESET_AON_APB_PSCP_SIM1_AON_TOP_SOFT_RST]	= { 0x0014, BIT(5), 0x1000 },
	[RESET_AON_APB_AON_ASE_SOFT_RST]		= { 0x0014, BIT(6), 0x1000 },
	[RESET_AON_APB_LP_AUDCP_INTC_SOFT_RST]		= { 0x0014, BIT(7), 0x1000 },
	[RESET_AON_APB_PMU_SOFT_RST]			= { 0x0014, BIT(8), 0x1000 },
	[RESET_AON_APB_ADI_SOFT_RST]			= { 0x0014, BIT(9), 0x1000 },
	[RESET_AON_APB_EIC_SOFT_RST]			= { 0x0014, BIT(10), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC0_SOFT_RST]		= { 0x0014, BIT(11), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC1_SOFT_RST]		= { 0x0014, BIT(12), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC2_SOFT_RST]		= { 0x0014, BIT(13), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC3_SOFT_RST]		= { 0x0014, BIT(14), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC4_SOFT_RST]		= { 0x0014, BIT(15), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC5_SOFT_RST]		= { 0x0014, BIT(16), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC6_SOFT_RST]		= { 0x0014, BIT(17), 0x1000 },
	[RESET_AON_APB_LP_AP_INTC7_SOFT_RST]		= { 0x0014, BIT(18), 0x1000 },
	[RESET_AON_APB_LP_PSCP_INTC_SOFT_RST]		= { 0x0014, BIT(19), 0x1000 },
	[RESET_AON_APB_LP_PHYCP_INTC_SOFT_RST]		= { 0x0014, BIT(20), 0x1000 },
	[RESET_AON_APB_AP_WDG_SOFT_RST]			= { 0x0014, BIT(29), 0x1000 },
	[RESET_AON_APB_APCPU_WDG_SOFT_RST]		= { 0x0014, BIT(30), 0x1000 },
	[RESET_AON_APB_AP_UART_SOFT_RST]		= { 0x0014, BIT(31), 0x1000 },
	[RESET_AON_APB_DJTAG_SOFT_RST]			= { 0x0130, BIT(15), 0x1000 },
};

static struct sprd_clk_desc uis8520_aon_gate_desc = {
	.clk_clks	= uis8520_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_aon_gate),
	.hw_clks	= &uis8520_aon_gate_hws,
	.resets		= uis8520_aon_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_aon_apb_resets),
};

/* aon apb clks */
static const struct clk_parent_data aon_apb_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &clk_4m3.hw },
	{ .hw = &clk_13m.hw },
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(aon_apb, "aon-apb", aon_apb_parents, 0x28,
			    0, 3, 0, 2, 0);

static const struct clk_parent_data adi_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_38m4.hw },
	{ .hw = &tgpll_51m2.hw },
};
static SPRD_MUX_CLK_DATA(adi, "adi", adi_parents, 0x34,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data pwm_timer_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .fw_name = "rco-60m" },
	{ .fw_name = "rco-100m" },
	{ .hw = &tsnpll_100m.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(pwm_timer, "pwm_timer", pwm_timer_parents, 0x40,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data efuse_parents[] = {
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(efuse, "efuse", efuse_parents, 0x4c,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data uart_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_51m2.hw },
	{ .hw = &tgpll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
};
static SPRD_MUX_CLK_DATA(uart0, "uart0", uart_parents, 0x58,
		    0, 3, UIS8520_MUX_FLAG);
static SPRD_MUX_CLK_DATA(uart1, "uart1", uart_parents, 0x64,
		    0, 3, UIS8520_MUX_FLAG);
static SPRD_MUX_CLK_DATA(ap_uart, "ap-uart", uart_parents, 0x70,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data thm_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_250k.hw },
};
static SPRD_MUX_CLK_DATA(thm0, "thm0", thm_parents, 0xa0,
		    0, 1, UIS8520_MUX_FLAG);
static SPRD_MUX_CLK_DATA(thm1, "thm1", thm_parents, 0xac,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data sp_i3c_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_51m2.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_192m.hw },
};
static SPRD_MUX_CLK_DATA(sp_i3c0, "sp-i3c0", sp_i3c_parents, 0xb8,
		    0, 3, UIS8520_MUX_FLAG);
static SPRD_MUX_CLK_DATA(sp_i3c1, "sp-i3c1", sp_i3c_parents, 0xc4,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data sp_i2c_slv_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_38m4.hw },
};
static SPRD_MUX_CLK_DATA(sp_i2c0_slv, "sp-i2c0-slv", sp_i2c_slv_parents, 0xd0,
		    0, 1, UIS8520_MUX_FLAG);
static SPRD_MUX_CLK_DATA(sp_i2c1_slv, "sp-i2c1-slv", sp_i2c_slv_parents, 0xdc,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data sp_i2c_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_51m2.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(sp_i2c0, "sp-i2c0", sp_i2c_parents, 0xe8,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(sp_i2c1, "sp-i2c1", sp_i2c_parents, 0xf4,
			    0, 2, 0, 3, 0);

static const struct clk_parent_data sp_spi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw},
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_192m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(sp_spi0, "sp-spi0", sp_spi_parents, 0x100,
			    0, 3, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(sp_spi1, "sp-spi1", sp_spi_parents, 0x10c,
			    0, 3, 0, 3, 0);

static const struct clk_parent_data scc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
	{ .hw = &tgpll_51m2.hw },
	{ .hw = &tgpll_96m.hw },
};
static SPRD_MUX_CLK_DATA(scc, "scc", scc_parents, 0x118,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data apcpu_dap_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &tgpll_76m8.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(apcpu_dap, "apcpu-dap", apcpu_dap_parents, 0x124,
		    0, 3, UIS8520_MUX_FLAG);


static const struct clk_parent_data apcpu_ts_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(apcpu_ts, "apcpu-ts", apcpu_ts_parents, 0x130,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data debug_ts_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_76m8.hw },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_192m.hw },
};
static SPRD_MUX_CLK_DATA(debug_ts, "debug-ts", debug_ts_parents, 0x13c,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data rffe_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(rffe, "rffe", rffe_parents, 0x148,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data pri_sbi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .fw_name = "ext-52m" },
	{ .hw = &tgpll_96m.hw },
};
static SPRD_MUX_CLK_DATA(pri_sbi, "pri-sbi", pri_sbi_parents, 0x154,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data xo_sel_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .fw_name = "ext-52m" },
};
static SPRD_MUX_CLK_DATA(xo_sel, "xo-sel", xo_sel_parents, 0x160,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data rfti_lth_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .fw_name = "ext-52m" },
};
static SPRD_MUX_CLK_DATA(rfti_lth, "rfti-lth", rfti_lth_parents, 0x16c,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data afc_lth_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .fw_name = "ext-52m" },
};
static SPRD_MUX_CLK_DATA(afc_lth, "afc-lth", afc_lth_parents, 0x178,
		    0, 1, UIS8520_MUX_FLAG);

static SPRD_DIV_CLK_FW_NAME(rco100m_fdk, "rco100m-fdk", "rco-100m", 0x18c,
		    0, 6, 0);

static const struct clk_parent_data djtag_tck_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(djtag_tck, "djtag-tck", djtag_tck_parents, 0x19c,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data aon_tmr_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(aon_tmr, "aon-tmr", aon_tmr_parents, 0x1b4,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data aon_pmu_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &clk_4m3.hw },
	{ .hw = &rco_60m_4m.hw },
};
static SPRD_MUX_CLK_DATA(aon_pmu, "aon-pmu", aon_pmu_parents, 0x1cc,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data debounce_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &rco_60m_4m.hw },
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &rco_60m_20m.hw },
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(debounce, "debounce", debounce_parents, 0x1d8,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data apcpu_pmu_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .fw_name = "rco-60m" },
	{ .hw = &tgpll_76m8.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
};
static SPRD_MUX_CLK_DATA(apcpu_pmu, "apcpu-pmu", apcpu_pmu_parents, 0x1e4,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data top_dvfs_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
};
static SPRD_MUX_CLK_DATA(top_dvfs, "top-dvfs", top_dvfs_parents, 0x1f0,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data pmu_26m_parents[] = {
	{ .hw = &rco_100m_20m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &rco_60m_20m.hw },
};
static SPRD_MUX_CLK_DATA(pmu_26m, "pmu-26m", aon_pmu_parents, 0x1fc,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data tzpc_parents[] = {
	{ .hw = &rco_100m_4m.hw },
	{ .hw = &clk_4m3.hw },
	{ .hw = &clk_13m.hw },
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(tzpc, "tzpc", tzpc_parents, 0x208,
			    0, 3, 0, 2, 0);

static const struct clk_parent_data otg_ref_parents[] = {
	{ .hw = &tgpll_12m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(otg_ref, "otg-ref", otg_ref_parents, 0x220,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data cssys_parents[] = {
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &tgpll_512m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(cssys, "cssys", cssys_parents, 0x22c,
			    0, 3, 0, 2, 0);
static SPRD_DIV_CLK_HW(cssys_apb, "cssys-apb", &cssys.common.hw, 0x234,
		    0, 2, 0);

static const struct clk_parent_data sdio_2x_parents[] = {
	{ .hw = &clk_1m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_307m2.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &rpll_390m.hw },
	{ .hw = &v4nrpll_409m6.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(sdio0_2x, "sdio0-2x", sdio_2x_parents, 0x244,
		    0, 3, 0, 11, 0);
static SPRD_DIV_CLK_HW(sdio0_1x, "sdio0-1x", &sdio0_2x.common.hw, 0x24c,
		    0, 1, 0);

static SPRD_COMP_CLK_DATA_OFFSET(sdio2_2x, "sdio2-2x", sdio_2x_parents, 0x25c,
			    0, 3, 0, 11, 0);
static SPRD_DIV_CLK_HW(sdio2_1x, "sdio2-1x", &sdio2_2x.common.hw, 0x264,
		    0, 1, 0);

static const struct clk_parent_data spi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_192m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(spi0, "spi0", spi_parents, 0x274,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(spi1, "spi1", spi_parents, 0x280,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(spi2, "spi2", spi_parents, 0x28c,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(spi3, "spi3", spi_parents, 0x298,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(spi4, "spi4", spi_parents, 0x2a4,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_DATA_OFFSET(spi5, "spi5", spi_parents, 0x2b0,
			    0, 2, 0, 3, 0);

static const struct clk_parent_data spi_pscp_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_192m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(spi_pscp, "spi-pscp", spi_pscp_parents, 0x2bc,
			    0, 2, 0, 3, 0);

static const struct clk_parent_data analog_io_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_48m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(analog_io_apb, "analog-io-apb",
			    analog_io_apb_parents, 0x2c8, 0, 1, 0, 2, 0);

static const struct clk_parent_data dmc_ref_parents[] = {
	{ .hw = &clk_6m5.hw },
	{ .hw = &clk_13m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(dmc_ref, "dmc-ref", dmc_ref_parents, 0x2d4,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data usb_parents[] = {
	{ .hw = &rco_100m_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_76m8.hw },
	{ .hw = &tgpll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tgpll_128m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(usb, "usb", usb_parents, 0x2e0,
			    0, 3, 0, 2, 0);

static const struct clk_parent_data usb_suspend_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_160k.hw },
	{ .hw = &clk_200k.hw },
	{ .hw = &clk_1m.hw },
};
static SPRD_MUX_CLK_DATA(usb_suspend, "usb-suspend", usb_suspend_parents, 0x2f8,
		    0, 2, UIS8520_MUX_FLAG);

static SPRD_DIV_CLK_FW_NAME(rco60m_fdk, "rco60m-fdk", "rco-60m", 0x414,
		    0, 5, 0);

static const struct clk_parent_data rco6m_ref_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_2m.hw },
};
static SPRD_COMP_CLK_DATA_OFFSET(rco6m_ref, "rco6m-ref", rco6m_ref_parents, 0x424,
			    0, 1, 0, 5, 0);
static SPRD_DIV_CLK_FW_NAME(rco6m_fdk, "rco6m-fdk", "rco-6m", 0x42c,
		    0, 11, 0);

static const struct clk_parent_data aon_iis_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(aon_iis, "aon-iis", aon_iis_parents, 0x454,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data tsn_timer_parents[] = {
	{ .fw_name = "ext-52m" },
	{ .hw = &tgpll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &tsnpll_100m.hw },
};
static SPRD_MUX_CLK_DATA(tsn_timer, "tsn-timer", tsn_timer_parents, 0x478,
		    0, 2, UIS8520_MUX_FLAG);

static struct sprd_clk_common *uis8520_aonapb_clk[] = {
	/* address base is 0x64060000 */
	&aon_apb.common,
	&adi.common,
	&pwm_timer.common,
	&efuse.common,
	&uart0.common,
	&uart1.common,
	&ap_uart.common,
	&thm0.common,
	&thm1.common,
	&sp_i3c0.common,
	&sp_i3c1.common,
	&sp_i2c0_slv.common,
	&sp_i2c1_slv.common,
	&sp_i2c0.common,
	&sp_i2c1.common,
	&sp_spi0.common,
	&sp_spi1.common,
	&scc.common,
	&apcpu_dap.common,
	&apcpu_ts.common,
	&debug_ts.common,
	&rffe.common,
	&pri_sbi.common,
	&xo_sel.common,
	&rfti_lth.common,
	&afc_lth.common,
	&rco100m_fdk.common,
	&djtag_tck.common,
	&aon_tmr.common,
	&aon_pmu.common,
	&debounce.common,
	&apcpu_pmu.common,
	&top_dvfs.common,
	&pmu_26m.common,
	&tzpc.common,
	&otg_ref.common,
	&cssys.common,
	&cssys_apb.common,
	&sdio0_2x.common,
	&sdio0_1x.common,
	&sdio2_2x.common,
	&sdio2_1x.common,
	&spi0.common,
	&spi1.common,
	&spi2.common,
	&spi3.common,
	&spi4.common,
	&spi5.common,
	&spi_pscp.common,
	&analog_io_apb.common,
	&dmc_ref.common,
	&usb.common,
	&usb_suspend.common,
	&rco60m_fdk.common,
	&rco6m_ref.common,
	&rco6m_fdk.common,
	&aon_iis.common,
	&tsn_timer.common,
};

static struct clk_hw_onecell_data uis8520_aonapb_clk_hws = {
	.hws	= {
		[CLK_AON_APB]		= &aon_apb.common.hw,
		[CLK_ADI]		= &adi.common.hw,
		[CLK_PWM_TIMER]		= &pwm_timer.common.hw,
		[CLK_EFUSE]		= &efuse.common.hw,
		[CLK_UART0]		= &uart0.common.hw,
		[CLK_UART1]		= &uart1.common.hw,
		[CLK_AP_UART]		= &ap_uart.common.hw,
		[CLK_THM0]		= &thm0.common.hw,
		[CLK_THM1]		= &thm1.common.hw,
		[CLK_SP_I3C0]		= &sp_i3c0.common.hw,
		[CLK_SP_I3C1]		= &sp_i3c1.common.hw,
		[CLK_SP_I2C0_SLV]	= &sp_i2c0_slv.common.hw,
		[CLK_SP_I2C1_SLV]	= &sp_i2c1_slv.common.hw,
		[CLK_SP_I2C0]		= &sp_i2c0.common.hw,
		[CLK_SP_I2C1]		= &sp_i2c1.common.hw,
		[CLK_SP_SPI0]		= &sp_spi0.common.hw,
		[CLK_SP_SPI1]		= &sp_spi1.common.hw,
		[CLK_SCC]		= &scc.common.hw,
		[CLK_APCPU_DAP]		= &apcpu_dap.common.hw,
		[CLK_APCPU_TS]		= &apcpu_ts.common.hw,
		[CLK_DEBUG_TS]		= &debug_ts.common.hw,
		[CLK_RFFE]		= &rffe.common.hw,
		[CLK_PRI_SBI]		= &pri_sbi.common.hw,
		[CLK_XO_SEL]		= &xo_sel.common.hw,
		[CLK_RFTI_LTH]		= &rfti_lth.common.hw,
		[CLK_AFC_LTH]		= &afc_lth.common.hw,
		[CLK_RCO100M_FDK]	= &rco100m_fdk.common.hw,
		[CLK_DJTAG_TCK]		= &djtag_tck.common.hw,
		[CLK_AON_TMR]		= &aon_tmr.common.hw,
		[CLK_AON_PMU]		= &aon_pmu.common.hw,
		[CLK_DEBOUNCE]		= &debounce.common.hw,
		[CLK_APCPU_PMU]		= &apcpu_pmu.common.hw,
		[CLK_TOP_DVFS]		= &top_dvfs.common.hw,
		[CLK_PMU_26M]		= &pmu_26m.common.hw,
		[CLK_TZPC]		= &tzpc.common.hw,
		[CLK_OTG_REF]		= &otg_ref.common.hw,
		[CLK_CSSYS]		= &cssys.common.hw,
		[CLK_CSSYS_APB]		= &cssys_apb.common.hw,
		[CLK_SDIO0_2X]		= &sdio0_2x.common.hw,
		[CLK_SDIO0_1X]		= &sdio0_1x.common.hw,
		[CLK_SDIO2_2X]		= &sdio2_2x.common.hw,
		[CLK_SDIO2_1X]		= &sdio2_1x.common.hw,
		[CLK_SPI0]		= &spi0.common.hw,
		[CLK_SPI1]		= &spi1.common.hw,
		[CLK_SPI2]		= &spi2.common.hw,
		[CLK_SPI3]		= &spi3.common.hw,
		[CLK_SPI4]		= &spi4.common.hw,
		[CLK_SPI5]		= &spi5.common.hw,
		[CLK_SPI_PSCP]		= &spi_pscp.common.hw,
		[CLK_ANALOG_IO_APB]	= &analog_io_apb.common.hw,
		[CLK_DMC_REF]		= &dmc_ref.common.hw,
		[CLK_USB]		= &usb.common.hw,
		[CLK_USB_SUSPEND]	= &usb_suspend.common.hw,
		[CLK_RCO60M_FDK]	= &rco60m_fdk.common.hw,
		[CLK_RCO6M_REF]		= &rco6m_ref.common.hw,
		[CLK_RCO6M_FDK]		= &rco6m_fdk.common.hw,
		[CLK_AON_IIS]		= &aon_iis.common.hw,
		[CLK_TSN_TIMER]		= &tsn_timer.common.hw,
	},
	.num	= CLK_AON_APB_NUM,
};

static struct sprd_clk_desc uis8520_aonapb_clk_desc = {
	.clk_clks	= uis8520_aonapb_clk,
	.num_clk_clks	= ARRAY_SIZE(uis8520_aonapb_clk),
	.hw_clks	= &uis8520_aonapb_clk_hws,
};

/* top dvfs clk */
static const struct clk_parent_data lit_core_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_256m.hw },
	{ .hw = &tgpll_768m.hw },
	{ .hw = &v4nrpll_1228m8.hw },
	{ .hw = &tgpll.common.hw },
	{ .hw = &mplll.common.hw },
};
static SPRD_COMP_CLK_DATA(core0, "core0", lit_core_parents, 0xe08,
		     0, 3, 3, 1, 0);
static SPRD_COMP_CLK_DATA(core1, "core1", lit_core_parents, 0xe08,
		     4, 3, 7, 1, 0);
static SPRD_COMP_CLK_DATA(core2, "core2", lit_core_parents, 0xe08,
		     8, 3, 11, 1, 0);
static SPRD_COMP_CLK_DATA(core3, "core3", lit_core_parents, 0xe08,
		     12, 3, 15, 1, 0);

static const struct clk_parent_data scu_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_512m.hw },
	{ .hw = &tgpll_768m.hw },
	{ .hw = &v4nrpll_819m2.hw },
	{ .hw = &mplls.common.hw },
};
static SPRD_COMP_CLK_DATA(scu, "scu", scu_parents, 0xe0c,
		     0, 3, 3, 1, 0);
static SPRD_DIV_CLK_HW(ace, "ace", &scu.common.hw, 0xe0c,
		    24, 1, 0);

static const struct clk_parent_data atb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &tgpll_512m.hw },
};
static SPRD_COMP_CLK_DATA(atb, "atb", atb_parents, 0xe0c,
		     4, 2, 6, 3, 0);
static SPRD_DIV_CLK_HW(debug_apb, "debug_apb", &atb.common.hw, 0xe0c,
		    25, 2, 0);

static const struct clk_parent_data cps_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &v4nrpll_614m4.hw },
};
static SPRD_COMP_CLK_DATA(cps, "cps", cps_parents, 0xe0c,
		     9, 2, 11, 3, 0);

static const struct clk_parent_data gic_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_153m6.hw  },
	{ .hw = &tgpll_384m.hw  },
	{ .hw = &tgpll_512m.hw  },
};
static SPRD_COMP_CLK_DATA(gic, "gic", gic_parents, 0xe0c,
		     14, 2, 16, 3, 0);

static const struct clk_parent_data periph_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_153m6.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &tgpll_512m.hw },
};
static SPRD_COMP_CLK_DATA(periph, "periph", periph_parents, 0xe0c,
		     19, 2, 21, 3, 0);

static struct sprd_clk_common *uis8520_topdvfs_clk[] = {
	/* address base is 0x64080000 */
	&core0.common,
	&core1.common,
	&core2.common,
	&core3.common,
	&scu.common,
	&ace.common,
	&atb.common,
	&debug_apb.common,
	&cps.common,
	&gic.common,
	&periph.common,
};

static struct clk_hw_onecell_data uis8520_topdvfs_clk_hws = {
	.hws    = {
		[CLK_CORE0]		= &core0.common.hw,
		[CLK_CORE1]		= &core1.common.hw,
		[CLK_CORE2]		= &core2.common.hw,
		[CLK_CORE3]		= &core3.common.hw,
		[CLK_SCU]		= &scu.common.hw,
		[CLK_ACE]		= &ace.common.hw,
		[CLK_ATB]		= &atb.common.hw,
		[CLK_DEBUG_APB]		= &debug_apb.common.hw,
		[CLK_CPS]		= &cps.common.hw,
		[CLK_GIC]		= &gic.common.hw,
		[CLK_PERIPH]		= &periph.common.hw,
	},
	.num    = CLK_TOPDVFS_CLK_NUM,
};

static struct sprd_clk_desc uis8520_topdvfs_clk_desc = {
	.clk_clks	= uis8520_topdvfs_clk,
	.num_clk_clks	= ARRAY_SIZE(uis8520_topdvfs_clk),
	.hw_clks	= &uis8520_topdvfs_clk_hws,
};

/* ipa apb gate clocks */
/* ipa apb related gate clocks configure CLK_IGNORE_UNUSED because their
 * power domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK_FW_NAME(usb_eb, "usb-eb", "ext-26m", 0x4,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb_suspend_eb, "usb-suspend-eb", "ext-26m", 0x4,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb_ref_eb, "usb-ref-eb", "ext-26m", 0x4,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_eb, "ipa-eb", "ext-26m", 0x4,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pam_usb_eb, "pam-usb-eb", "ext-26m", 0x4,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(tft_eb, "tft-eb", "ext-26m", 0x4,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pam_wifi_eb, "pam-wifi-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_tca_eb, "ipa-tca-eb", "ext-26m", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_usb31pll_eb, "ipa-usb31pll-eb", "ext-26m", 0x4,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_ckg_eb, "ipa-ckg-eb", "ext-26m", 0x4,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_access_pcie0_en, "ipa-access-pcie0-en", "ext-26m",
			0x11c, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_access_pcie1_en, "ipa-access-pcie1-en", "ext-26m",
			0x11c, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_access_eth_en, "ipa-access-eth-en", "ext-26m",
			0x11c, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *uis8520_ipaapb_gate[] = {
	/* address base is 0x25000000 */
	&usb_eb.common,
	&usb_suspend_eb.common,
	&usb_ref_eb.common,
	&ipa_eb.common,
	&pam_usb_eb.common,
	&tft_eb.common,
	&pam_wifi_eb.common,
	&ipa_tca_eb.common,
	&ipa_usb31pll_eb.common,
	&ipa_ckg_eb.common,
	&ipa_access_pcie0_en.common,
	&ipa_access_pcie1_en.common,
	&ipa_access_eth_en.common,
};

static struct clk_hw_onecell_data uis8520_ipaapb_gate_hws = {
	.hws	= {
		[CLK_USB_EB]			= &usb_eb.common.hw,
		[CLK_USB_SUSPEND_EB]		= &usb_suspend_eb.common.hw,
		[CLK_USB_REF_EB]		= &usb_ref_eb.common.hw,
		[CLK_IPA_EB]			= &ipa_eb.common.hw,
		[CLK_PAM_USB_EB]		= &pam_usb_eb.common.hw,
		[CLK_TFT_EB]			= &tft_eb.common.hw,
		[CLK_PAM_WIFI_EB]		= &pam_wifi_eb.common.hw,
		[CLK_IPA_TCA_EB]		= &ipa_tca_eb.common.hw,
		[CLK_IPA_USB31PLL_EB]		= &ipa_usb31pll_eb.common.hw,
		[CLK_IPA_CKG_EB]		= &ipa_ckg_eb.common.hw,
		[CLK_IPA_ACCESS_PCIE0_EN]	= &ipa_access_pcie0_en.common.hw,
		[CLK_IPA_ACCESS_PCIE1_EN]	= &ipa_access_pcie1_en.common.hw,
		[CLK_IPA_ACCESS_ETH_EN]		= &ipa_access_eth_en.common.hw,
	},
	.num	= CLK_IPAAPB_GATE_NUM,
};

static struct sprd_reset_map uis8520_ipa_apb_resets[] = {
	[RESET_IPA_APB_USB_SOFT_RST]			= { 0x0000, BIT(0), 0x1000 },
	[RESET_IPA_APB_PAM_U3_SOFT_RST]			= { 0x0000, BIT(1), 0x1000 },
	[RESET_IPA_APB_NIC_400_CFG_SOFT_RST]		= { 0x0000, BIT(2), 0x1000 },
	[RESET_IPA_APB_PAM_WIFI_SOFT_RST]		= { 0x0000, BIT(3), 0x1000 },
	[RESET_IPA_APB_IPA_SOFT_RST]			= { 0x0000, BIT(4), 0x1000 },
	[RESET_IPA_APB_PHY_SOFT_RST]			= { 0x0000, BIT(5), 0x1000 },
	[RESET_IPA_APB_TCA_SOFT_RST]			= { 0x0000, BIT(6), 0x1000 },
	[RESET_IPA_APB_BUSMON_PERF_PAM_U3_SOFT_RST]	= { 0x0000, BIT(7), 0x1000 },
	[RESET_IPA_APB_BUSMON_PERF_UPA_WIFI_SOFT_RST]	= { 0x0000, BIT(8), 0x1000 },
	[RESET_IPA_APB_BUSMON_PERF_IPA_M0_SOFT_RST]	= { 0x0000, BIT(9), 0x1000 },
};

static struct sprd_clk_desc uis8520_ipaapb_gate_desc = {
	.clk_clks	= uis8520_ipaapb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_ipaapb_gate),
	.hw_clks	= &uis8520_ipaapb_gate_hws,
	.resets		= uis8520_ipa_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_ipa_apb_resets),
};

/* ipa clocks*/
static const struct clk_parent_data ipa_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_192m.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &v4nrpll_409m6.hw },
};
static SPRD_MUX_CLK_DATA(ipa_axi, "ipa-axi", ipa_axi_parents, 0x28,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data ipa_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_192m.hw },
};
static SPRD_MUX_CLK_DATA(ipa_apb, "ipa-apb", ipa_apb_parents, 0x34,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data usb_ref_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &tgpll_24m.hw },
};
static SPRD_MUX_CLK_DATA(usb_ref, "usb-ref", usb_ref_parents, 0x4c,
		    0, 1, UIS8520_MUX_FLAG);

static const struct clk_parent_data ipa_otg_ref_parents[] = {
	{ .hw = &tgpll_12m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(ipa_otg_ref, "ipa-otg-ref", ipa_otg_ref_parents, 0x58,
		    0, 1, UIS8520_MUX_FLAG);

static struct sprd_clk_common *uis8520_ipa_clk[] = {
	/* address base is 0x25010000 */
	&ipa_axi.common,
	&ipa_apb.common,
	&usb_ref.common,
	&ipa_otg_ref.common,
};

static struct clk_hw_onecell_data uis8520_ipa_clk_hws = {
	.hws	= {
		[CLK_IPA_AXI]		= &ipa_axi.common.hw,
		[CLK_IPA_APB]		= &ipa_apb.common.hw,
		[CLK_USB_REF]		= &usb_ref.common.hw,
		[CLK_IPA_OTG_REF]	= &ipa_otg_ref.common.hw
	},
	.num	= CLK_IPA_CLK_NUM,
};

static struct sprd_clk_desc uis8520_ipa_clk_desc = {
	.clk_clks	= uis8520_ipa_clk,
	.num_clk_clks	= ARRAY_SIZE(uis8520_ipa_clk),
	.hw_clks	= &uis8520_ipa_clk_hws,
};

/* pcie0 apb gate clocks */
/* pcie0 related gate clocks configure CLK_IGNORE_UNUSED because their power
 * domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK_FW_NAME(pcie3_aux_0_eb, "pcie3-aux-0-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie3_0_eb, "pcie3-0-eb", "ext-26m", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nic400_tranmon_0_eb, "nic400-tranmon-0-eb", "ext-26m",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nic400_cfg_0_eb, "nic400-cfg-0-eb", "ext-26m", 0x4,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie3_phy_0_eb, "pcie3-phy-0-eb", "ext-26m", 0x4,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *uis8520_pcie0apb_gate[] = {
	/* address base is 0x26000000 */
	&pcie3_aux_0_eb.common,
	&pcie3_0_eb.common,
	&nic400_tranmon_0_eb.common,
	&nic400_cfg_0_eb.common,
	&pcie3_phy_0_eb.common,
};

static struct clk_hw_onecell_data uis8520_pcie0apb_gate_hws = {
	.hws	= {
		[CLK_PCIE3_AUX_0_EB]		= &pcie3_aux_0_eb.common.hw,
		[CLK_PCIE3_0_EB]		= &pcie3_0_eb.common.hw,
		[CLK_NIC400_TRANMON_0_EB]	= &nic400_tranmon_0_eb.common.hw,
		[CLK_NIC400_CFG_0_EB]		= &nic400_cfg_0_eb.common.hw,
		[CLK_PCIE3_PHY_0_EB]		= &pcie3_phy_0_eb.common.hw,
	},
	.num	= CLK_PCIE0APB_GATE_NUM,
};

static struct sprd_reset_map uis8520_pcie0_apb_resets[] = {
	[RESET_PCIE_APB_PCIE3_SOFT_0_RST]	= { 0x0000, BIT(5), 0x1000 },
	[RESET_PCIE_APB_NIC400_CFG_SOFT_0_RST]	= { 0x0000, BIT(6), 0x1000 },
	[RESET_PCIE_APB_PCIE_ANLG_SOFT_0_RST]	= { 0x0000, BIT(7), 0x1000 },
};

static struct sprd_clk_desc uis8520_pcie0apb_gate_desc = {
	.clk_clks	= uis8520_pcie0apb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_pcie0apb_gate),
	.hw_clks	= &uis8520_pcie0apb_gate_hws,
	.resets		= uis8520_pcie0_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_pcie0_apb_resets),
};

/* pcie0 clocks*/
static const struct clk_parent_data pcie0_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_192m.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &tgpll_512m.hw },
};
static SPRD_MUX_CLK_DATA(pcie0_axi, "pcie0-axi", pcie0_axi_parents, 0x28,
		    0, 2, CLK_SET_RATE_NO_REPARENT);
static SPRD_DIV_CLK_HW(pcie0_apb, "pcie0-apb", &pcie0_axi.common.hw, 0x30,
		    0, 3, 0);

static const struct clk_parent_data pcie0_aux_parents[] = {
	{ .hw = &clk_2m.hw },
	{ .hw = &rco_100m_2m.hw },
};
static SPRD_MUX_CLK_DATA(pcie0_aux, "pcie0-aux", pcie0_aux_parents, 0x40,
		    0, 1, UIS8520_MUX_FLAG);

static struct sprd_clk_common *uis8520_pcie0_clk[] = {
	/* address base is 0x26004000 */
	&pcie0_axi.common,
	&pcie0_apb.common,
	&pcie0_aux.common,
};

static struct clk_hw_onecell_data uis8520_pcie0_clk_hws = {
	.hws	= {
		[CLK_PCIE0_AXI]		= &pcie0_axi.common.hw,
		[CLK_PCIE0_APB]		= &pcie0_apb.common.hw,
		[CLK_PCIE0_AUX]		= &pcie0_aux.common.hw,
	},
	.num	= CLK_PCIE0_CLK_NUM,
};

static struct sprd_clk_desc uis8520_pcie0_clk_desc = {
	.clk_clks	= uis8520_pcie0_clk,
	.num_clk_clks	= ARRAY_SIZE(uis8520_pcie0_clk),
	.hw_clks	= &uis8520_pcie0_clk_hws,
};

/* pcie1 apb gate clocks */
/* pcie1 related gate clocks configure CLK_IGNORE_UNUSED because their power
 * domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK_FW_NAME(pcie3_aux_1_eb, "pcie3-aux-1-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie3_1_eb, "pcie3-1-eb", "ext-26m", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nic400_tranmon_1_eb, "nic400-tranmon-1-eb", "ext-26m",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nic400_cfg_1_eb, "nic400-cfg-1-eb", "ext-26m", 0x4,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie3_phy_1_eb, "pcie3-phy-1-eb", "ext-26m", 0x4,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *uis8520_pcie1apb_gate[] = {
	/* address base is 0x26800000 */
	&pcie3_aux_1_eb.common,
	&pcie3_1_eb.common,
	&nic400_tranmon_1_eb.common,
	&nic400_cfg_1_eb.common,
	&pcie3_phy_1_eb.common,
};

static struct clk_hw_onecell_data uis8520_pcie1apb_gate_hws = {
	.hws	= {
		[CLK_PCIE3_AUX_1_EB]		= &pcie3_aux_1_eb.common.hw,
		[CLK_PCIE3_1_EB]		= &pcie3_1_eb.common.hw,
		[CLK_NIC400_TRANMON_1_EB]	= &nic400_tranmon_1_eb.common.hw,
		[CLK_NIC400_CFG_1_EB]		= &nic400_cfg_1_eb.common.hw,
		[CLK_PCIE3_PHY_1_EB]		= &pcie3_phy_1_eb.common.hw,
	},
	.num	= CLK_PCIE1APB_GATE_NUM,
};

static struct sprd_reset_map uis8520_pcie1_apb_resets[] = {
	[RESET_PCIE_APB_PCIE3_SOFT_1_RST]	= { 0x0000, BIT(5), 0x1000 },
	[RESET_PCIE_APB_NIC400_CFG_SOFT_1_RST]	= { 0x0000, BIT(6), 0x1000 },
	[RESET_PCIE_APB_PCIE_ANLG_SOFT_1_RST]	= { 0x0000, BIT(7), 0x1000 },
};

static struct sprd_clk_desc uis8520_pcie1apb_gate_desc = {
	.clk_clks	= uis8520_pcie1apb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_pcie1apb_gate),
	.hw_clks	= &uis8520_pcie1apb_gate_hws,
	.resets		= uis8520_pcie1_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_pcie1_apb_resets),
};

/* pcie1 clocks*/
static const struct clk_parent_data pcie1_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_192m.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &tgpll_512m.hw },
};
static SPRD_MUX_CLK_DATA(pcie1_axi, "pcie1-axi", pcie1_axi_parents, 0x28,
		    0, 2, CLK_SET_RATE_NO_REPARENT);
static SPRD_DIV_CLK_HW(pcie1_apb, "pcie1-apb", &pcie1_axi.common.hw, 0x30,
		    0, 3, 0);

static const struct clk_parent_data pcie1_aux_parents[] = {
	{ .hw = &clk_2m.hw },
	{ .hw = &rco_100m_2m.hw },
};
static SPRD_MUX_CLK_DATA(pcie1_aux, "pcie1-aux", pcie1_aux_parents, 0x40,
		    0, 1, UIS8520_MUX_FLAG);

static struct sprd_clk_common *uis8520_pcie1_clk[] = {
	/* address base is 0x26804000 */
	&pcie1_axi.common,
	&pcie1_apb.common,
	&pcie1_aux.common,
};

static struct clk_hw_onecell_data uis8520_pcie1_clk_hws = {
	.hws    = {
		[CLK_PCIE1_AXI]		= &pcie1_axi.common.hw,
		[CLK_PCIE1_APB]		= &pcie1_apb.common.hw,
		[CLK_PCIE1_AUX]		= &pcie1_aux.common.hw,
	},
	.num    = CLK_PCIE1_CLK_NUM,
};

static struct sprd_clk_desc uis8520_pcie1_clk_desc = {
	.clk_clks	= uis8520_pcie1_clk,
	.num_clk_clks	= ARRAY_SIZE(uis8520_pcie1_clk),
	.hw_clks	= &uis8520_pcie1_clk_hws,
};

/* ether xge global gates */
static SPRD_SC_GATE_CLK_HW(xgmac_eb, "xgmac-eb", &ap_access_xge_en.common.hw,
			0x0, 0x1000, BIT(1), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(xge_clk_rf_eb, "xge-clk-rf-eb", &ap_access_xge_en.common.hw,
			0x0, 0x1000, BIT(3), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(nic400_transmon_eb, "nic400-transmon-eb", &ap_access_xge_en.common.hw,
			0x0, 0x1000, BIT(4), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(nic400_cfg_eb, "nic400-cfg-eb", &ap_access_xge_en.common.hw,
			0x0, 0x1000, BIT(5), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(phy_eb, "phy-eb", &ap_access_xge_en.common.hw,
			0x4, 0x1000, BIT(6), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(tsn_timer_eb, "tsn-timer-eb", &ap_access_xge_en.common.hw,
			0x4, 0x1000, BIT(7), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);

static struct sprd_clk_common *uis8520_eth_gate[] = {
	/* address base is 0x28000000 */
	&xgmac_eb.common,
	&xge_clk_rf_eb.common,
	&nic400_transmon_eb.common,
	&nic400_cfg_eb.common,
	&phy_eb.common,
	&tsn_timer_eb.common,
};

static struct clk_hw_onecell_data uis8520_eth_gate_hws = {
	.hws	= {
		[CLK_XGMAC_EB]			= &xgmac_eb.common.hw,
		[CLK_XGE_CLK_RF_EB]		= &xge_clk_rf_eb.common.hw,
		[CLK_NIC400_TRANSMON_EB]	= &nic400_transmon_eb.common.hw,
		[CLK_NIC400_CFG_EB]		= &nic400_cfg_eb.common.hw,
		[CLK_PHY_EB]			= &phy_eb.common.hw,
		[CLK_TSN_TIMER_EB]		= &tsn_timer_eb.common.hw,
	},
	.num	= CLK_ETH_GATE_NUM,
};

static struct sprd_reset_map uis8520_eth_resets[] = {
	[RESET_XGMAC_SOFT_RST]		= { 0x0004, BIT(0), 0x1000 },
	[RESET_XPCS_SOFT_RST]		= { 0x0004, BIT(1), 0x1000 },
	[RESET_PHY_SOFT_RST]		= { 0x0004, BIT(2), 0x1000 },
	[RESET_NIC400_CFG_SOFT_RST]	= { 0x0004, BIT(3), 0x1000 },
	[RESET_ETH_PLL_SSMOD_SOFT_RST]	= { 0x0004, BIT(4), 0x1000 },
};

static const struct sprd_clk_desc uis8520_eth_gate_desc = {
	.clk_clks	= uis8520_eth_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_eth_gate),
	.hw_clks	= &uis8520_eth_gate_hws,
	.resets		= uis8520_eth_resets,
	.num_resets	= ARRAY_SIZE(uis8520_eth_resets),
};

/*eth xge clock*/
static const struct clk_parent_data xge_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_256m.hw },
	{ .hw = &tgpll_384m.hw },
	{ .hw = &tgpll_512m.hw },
};

static SPRD_MUX_CLK_DATA(xge_axi, "xge-axi", xge_axi_parents, 0x28,
		    0, 3, UIS8520_MUX_FLAG);

static const struct clk_parent_data xge_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_128m.hw },
	{ .hw = &tgpll_192m.hw },
};

static SPRD_MUX_CLK_DATA(xge_apb, "xge-apb", xge_apb_parents, 0x34,
		    0, 2, UIS8520_MUX_FLAG);

static const struct clk_parent_data phy_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &tgpll_96m.hw },
};

static SPRD_MUX_CLK_DATA(phy_apb, "phy-apb", phy_apb_parents, 0x40,
		    0, 1, UIS8520_MUX_FLAG);

static struct sprd_clk_common *uis8520_eth_clk[] = {
	/* address base is 0x28010000 */
	&xge_axi.common,
	&xge_apb.common,
	&phy_apb.common,
};

static struct clk_hw_onecell_data uis8520_eth_clk_hws = {
	.hws    = {
		[CLK_XGE_AXI]		= &xge_axi.common.hw,
		[CLK_XGE_APB]		= &xge_apb.common.hw,
		[CLK_PHY_APB]		= &phy_apb.common.hw,
	},
	.num    = CLK_ETH_CLK_NUM,
};

static struct sprd_clk_desc uis8520_eth_clk_desc = {
	.clk_clks	= uis8520_eth_clk,
	.num_clk_clks	= ARRAY_SIZE(uis8520_eth_clk),
	.hw_clks	= &uis8520_eth_clk_hws,
};

/* audcp global clock gates */
/* Audcp global clocks configure CLK_IGNORE_UNUSED because these clocks may be
 * controlled by audcp sys at the same time. It may be cause an execption if
 * kernel gates these clock.
 */
static SPRD_SC_GATE_CLK_HW(audcp_iis0_eb, "audcp-iis0-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(0), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_iis1_eb, "audcp-iis1-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(1), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_iis2_eb, "audcp-iis2-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(2), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_uart_eb, "audcp-uart-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(4), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_dma_cp_eb, "audcp-dma-cp-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(5), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_dma_ap_eb, "audcp-dma-ap-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(6), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_glb_vad_eb, "audcp-glb-vad-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(7), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_glb_pdm_eb, "audcp-glb-pdm-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(8), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_glb_pdm_iis_eb, "audcp-glb-pdm-iis-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(9), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_src48k_eb, "audcp-src48k-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(10), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_mcdt_eb, "audcp-mcdt-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(12), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vbc_eb, "audcp-vbc-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(15), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_splk_eb, "audcp-splk-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(17), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_icu_eb, "audcp-icu-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(18), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(dma_ap_ashb_eb, "dma-ap-ashb-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(19), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(dma_cp_ashb_eb, "dma-cp-ashb-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(20), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_aud_eb, "audcp-aud-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(21), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audif_ckg_auto_en, "audif-ckg-auto-en", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(22), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vbc_24m_eb, "audcp-vbc-24m-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(23), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_tmr_26m_eb, "audcp-tmr-26m-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(24), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_dvfs_ashb_eb, "audcp-dvfs-ashb-eb",
			&access_aud_en.common.hw, 0x0, 0x1000, BIT(25),
			CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_matrix_cfg_en, "audcp-matrix-cfg-en",
			&access_aud_en.common.hw, 0x0, 0x1000, BIT(26),
			CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_tdm_hf_eb, "audcp-tdm-hf-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(27), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_tdm_eb, "audcp-tdm-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(28), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_glb_audif_eb, "audcp-glb-audif-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(30), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_glb_pdm_ap_eb, "audcp-glb-pdm-ap-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(16), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vbc_ap_eb, "audcp-vbc-ap-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_mcdt_ap_eb, "audcp-mcdt-ap-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_aud_ap_eb, "audcp-aud-ap-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(19), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vad_pclk_eb, "audcp-vad-pclk-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(31), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);

static struct sprd_clk_common *uis8520_audcpglb_gate[] = {
	/* address base is 0x56200000 */
	&audcp_iis0_eb.common,
	&audcp_iis1_eb.common,
	&audcp_iis2_eb.common,
	&audcp_uart_eb.common,
	&audcp_dma_cp_eb.common,
	&audcp_dma_ap_eb.common,
	&audcp_glb_vad_eb.common,
	&audcp_glb_pdm_eb.common,
	&audcp_glb_pdm_iis_eb.common,
	&audcp_src48k_eb.common,
	&audcp_mcdt_eb.common,
	&audcp_vbc_eb.common,
	&audcp_splk_eb.common,
	&audcp_icu_eb.common,
	&dma_ap_ashb_eb.common,
	&dma_cp_ashb_eb.common,
	&audcp_aud_eb.common,
	&audif_ckg_auto_en.common,
	&audcp_vbc_24m_eb.common,
	&audcp_tmr_26m_eb.common,
	&audcp_dvfs_ashb_eb.common,
	&audcp_matrix_cfg_en.common,
	&audcp_tdm_hf_eb.common,
	&audcp_tdm_eb.common,
	&audcp_glb_audif_eb.common,
	&audcp_glb_pdm_ap_eb.common,
	&audcp_vbc_ap_eb.common,
	&audcp_mcdt_ap_eb.common,
	&audcp_aud_ap_eb.common,
	&audcp_vad_pclk_eb.common,
};

static struct clk_hw_onecell_data uis8520_audcpglb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_IIS0_EB]		= &audcp_iis0_eb.common.hw,
		[CLK_AUDCP_IIS1_EB]		= &audcp_iis1_eb.common.hw,
		[CLK_AUDCP_IIS2_EB]		= &audcp_iis2_eb.common.hw,
		[CLK_AUDCP_UART_EB]		= &audcp_uart_eb.common.hw,
		[CLK_AUDCP_DMA_CP_EB]		= &audcp_dma_cp_eb.common.hw,
		[CLK_AUDCP_DMA_AP_EB]		= &audcp_dma_ap_eb.common.hw,
		[CLK_AUDCP_GLB_VAD_EB]		= &audcp_glb_vad_eb.common.hw,
		[CLK_AUDCP_GLB_PDM_EB]		= &audcp_glb_pdm_eb.common.hw,
		[CLK_AUDCP_GLB_PDM_IIS_EB]	= &audcp_glb_pdm_iis_eb.common.hw,
		[CLK_AUDCP_SRC48K_EB]		= &audcp_src48k_eb.common.hw,
		[CLK_AUDCP_MCDT_EB]		= &audcp_mcdt_eb.common.hw,
		[CLK_AUDCP_VBC_EB]		= &audcp_vbc_eb.common.hw,
		[CLK_AUDCP_SPLK_EB]		= &audcp_splk_eb.common.hw,
		[CLK_AUDCP_ICU_EB]		= &audcp_icu_eb.common.hw,
		[CLK_AUDCP_DMA_AP_ASHB_EB]	= &dma_ap_ashb_eb.common.hw,
		[CLK_AUDCP_DMA_CP_ASHB_EB]	= &dma_cp_ashb_eb.common.hw,
		[CLK_AUDCP_AUD_EB]		= &audcp_aud_eb.common.hw,
		[CLK_AUDIF_CKG_AUTO_EN]		= &audif_ckg_auto_en.common.hw,
		[CLK_AUDCP_VBC_24M_EB]		= &audcp_vbc_24m_eb.common.hw,
		[CLK_AUDCP_TMR_26M_EB]		= &audcp_tmr_26m_eb.common.hw,
		[CLK_AUDCP_DVFS_ASHB_EB]	= &audcp_dvfs_ashb_eb.common.hw,
		[CLK_AUDCP_MATRIX_CFG_EN]	= &audcp_matrix_cfg_en.common.hw,
		[CLK_AUDCP_TDM_HF_EB]		= &audcp_tdm_hf_eb.common.hw,
		[CLK_AUDCP_TDM_EB]		= &audcp_tdm_eb.common.hw,
		[CLK_AUDCP_GLB_AUDIF_EB]	= &audcp_glb_audif_eb.common.hw,
		[CLK_AUDCP_GLB_PDM_AP_EB]	= &audcp_glb_pdm_ap_eb.common.hw,
		[CLK_AUDCP_VBC_AP_EB]		= &audcp_vbc_ap_eb.common.hw,
		[CLK_AUDCP_MCDT_AP_EB]		= &audcp_mcdt_ap_eb.common.hw,
		[CLK_AUDCP_AUD_AP_EB]		= &audcp_aud_ap_eb.common.hw,
		[CLK_AUDCP_VAD_PCLK_EB]		= &audcp_vad_pclk_eb.common.hw,
	},
	.num	= CLK_AUDCP_GLB_GATE_NUM,
};

static struct sprd_reset_map uis8520_audcp_glb_resets[] = {
	[RESET_AUDCP_GLB_VBS_24M_SOFT_RST]	= { 0x0008, BIT(0), 0x1000 },
	[RESET_AUDCP_GLB_DMA_AP_SOFT_RST]	= { 0x0008, BIT(1), 0x1000 },
	[RESET_AUDCP_GLB_VAD_SOFT_RST]		= { 0x0008, BIT(2), 0x1000 },
	[RESET_AUDCP_GLB_SRC48K_SOFT_RST]	= { 0x0008, BIT(5), 0x1000 },
	[RESET_AUDCP_GLB_MCDT_SOFT_RST]		= { 0x0008, BIT(7), 0x1000 },
	[RESET_AUDCP_GLB_VBC_SOFT_RST]		= { 0x0008, BIT(9), 0x1000 },
	[RESET_AUDCP_GLB_SPINLOCK_SOFT_RST]	= { 0x0008, BIT(10), 0x1000 },
	[RESET_AUDCP_GLB_DMA_CP_SOFT_RST]	= { 0x0008, BIT(11), 0x1000 },
	[RESET_AUDCP_GLB_IIS0_SOFT_RST]		= { 0x0008, BIT(12), 0x1000 },
	[RESET_AUDCP_GLB_IIS1_SOFT_RST]		= { 0x0008, BIT(13), 0x1000 },
	[RESET_AUDCP_GLB_IIS2_SOFT_RST]		= { 0x0008, BIT(14), 0x1000 },
	[RESET_AUDCP_GLB_UART_SOFT_RST]		= { 0x0008, BIT(16), 0x1000 },
	[RESET_AUDCP_GLB_AUD_SOFT_RST]		= { 0x0008, BIT(25), 0x1000 },
	[RESET_AUDCP_GLB_TDM_SOFT_RST]		= { 0x0008, BIT(27), 0x1000 },
	[RESET_AUDCP_GLB_MATRIX_CFG_SOFT_RST]	= { 0x0008, BIT(28), 0x1000 },
	[RESET_AUDCP_GLB_TDM_HF_SOFT_RST]	= { 0x0008, BIT(29), 0x1000 },
	[RESET_AUDCP_GLB_PDM_SOFT_RST]		= { 0x0008, BIT(30), 0x1000 },
	[RESET_AUDCP_GLB_PDM_IIS_SOFT_RST]	= { 0x0008, BIT(31), 0x1000 },
};

static const struct sprd_clk_desc uis8520_audcpglb_gate_desc = {
	.clk_clks	= uis8520_audcpglb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_audcpglb_gate),
	.hw_clks	= &uis8520_audcpglb_gate_hws,
	.resets		= uis8520_audcp_glb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_audcp_glb_resets),
};

/* audcp aon apb gates */
/* Audcp aon aphb clocks configure CLK_IGNORE_UNUSED because these clocks may be
 * controlled by audcp sys at the same time. It may be cause an execption if
 * kernel gates these clock.
 */
static SPRD_SC_GATE_CLK_HW(audcp_vad_eb, "audcp-vad-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(0), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_pdm_eb, "audcp-pdm-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(1), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_audif_eb, "audcp-audif-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(3), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_pdm_iis_eb, "audcp-pdm-iis-eb", &access_aud_en.common.hw,
			0x0, 0x1000, BIT(4), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vad_apb_eb, "audcp-vad-apb-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(0), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_pdm_ap_eb, "audcp-pdm-ap-eb", &access_aud_en.common.hw,
			0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);

static struct sprd_clk_common *uis8520_audcpapb_gate[] = {
	/* address base is 0x56390000 */
	&audcp_vad_eb.common,
	&audcp_pdm_eb.common,
	&audcp_audif_eb.common,
	&audcp_pdm_iis_eb.common,
	&audcp_vad_apb_eb.common,
	&audcp_pdm_ap_eb.common,
};

static struct clk_hw_onecell_data uis8520_audcpapb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_VAD_EB]	= &audcp_vad_eb.common.hw,
		[CLK_AUDCP_PDM_EB]	= &audcp_pdm_eb.common.hw,
		[CLK_AUDCP_AUDIF_EB]	= &audcp_audif_eb.common.hw,
		[CLK_AUDCP_PDM_IIS_EB]	= &audcp_pdm_iis_eb.common.hw,
		[CLK_AUDCP_VAD_APB_EB]	= &audcp_vad_apb_eb.common.hw,
		[CLK_AUDCP_PDM_AP_EB]	= &audcp_pdm_ap_eb.common.hw,
	},
	.num	= CLK_AUDCP_APB_GATE_NUM,
};

static struct sprd_reset_map uis8520_audcp_aon_apb_resets[] = {
	[RESET_AUDCP_AON_APB_VAD_SOFT_RST]	= { 0x0008, BIT(0), 0x1000 },
	[RESET_AUDCP_AON_APB_PDM_SOFT_RST]	= { 0x0008, BIT(1), 0x1000 },
	[RESET_AUDCP_AON_APB_PDM_IIS_SOFT_RST]	= { 0x0008, BIT(2), 0x1000 },
	[RESET_AUDCP_AON_APB_DVFS_SOFT_RST]	= { 0x0008, BIT(3), 0x1000 },
};

static const struct sprd_clk_desc uis8520_audcpapb_gate_desc = {
	.clk_clks	= uis8520_audcpapb_gate,
	.num_clk_clks	= ARRAY_SIZE(uis8520_audcpapb_gate),
	.hw_clks	= &uis8520_audcpapb_gate_hws,
	.resets		= uis8520_audcp_aon_apb_resets,
	.num_resets	= ARRAY_SIZE(uis8520_audcp_aon_apb_resets),
};

static const struct of_device_id sprd_uis8520_clk_ids[] = {
	{ .compatible = "sprd,uis8520-pmu-gate",	/* 0x64910000 */
	  .data = &uis8520_pmu_gate_desc },
	{ .compatible = "sprd,uis8520-g1-pll",		/* 0x64304000 */
	  .data = &uis8520_g1_pll_desc },
	{ .compatible = "sprd,uis8520-g2-pll",		/* 0x64320000 */
	  .data = &uis8520_g2_pll_desc },
	{ .compatible = "sprd,uis8520-g10-pll",		/* 0x64334000 */
	  .data = &uis8520_g10_pll_desc },
	{ .compatible = "sprd,uis8520-g11-pll",		/* 0x64308000 */
	  .data = &uis8520_g11_pll_desc },
	{ .compatible = "sprd,uis8520-apapb-gate",	/* 0x20100000 */
	  .data = &uis8520_apapb_gate_desc },
	{ .compatible = "sprd,uis8520-ap-clk",		/* 0x20010000 */
	  .data = &uis8520_ap_clk_desc },
	{ .compatible = "sprd,uis8520-apahb-gate",	/* 0x20000000 */
	  .data = &uis8520_apahb_gate_desc },
	{ .compatible = "sprd,uis8520-aon-gate",	/* 0x64900000 */
	  .data = &uis8520_aon_gate_desc },
	{ .compatible = "sprd,uis8520-aonapb-clk",	/* 0x64060000 */
	  .data = &uis8520_aonapb_clk_desc },
	{ .compatible = "sprd,uis8520-topdvfs-clk",	/* 0x64080000 */
	  .data = &uis8520_topdvfs_clk_desc },
	{ .compatible = "sprd,uis8520-ipaapb-gate",	/* 0x25000000 */
	  .data = &uis8520_ipaapb_gate_desc },
	{ .compatible = "sprd,uis8520-ipa-clk",		/* 0x25010000 */
	  .data = &uis8520_ipa_clk_desc },
	{ .compatible = "sprd,uis8520-pcie0apb-gate",	/* 0x26000000 */
	  .data = &uis8520_pcie0apb_gate_desc },
	{ .compatible = "sprd,uis8520-pcie1apb-gate",	/* 0x26800000 */
	  .data = &uis8520_pcie1apb_gate_desc },
	{ .compatible = "sprd,uis8520-pcie0-clk",	/* 0x26004000 */
	  .data = &uis8520_pcie0_clk_desc },
	{ .compatible = "sprd,uis8520-pcie1-clk",	/* 0x26804000 */
	  .data = &uis8520_pcie1_clk_desc },
	{ .compatible = "sprd,uis8520-eth-gate",	/* 0x28000000 */
	  .data = &uis8520_eth_gate_desc },
	{ .compatible = "sprd,uis8520-eth-clk",		/* 0x28010000 */
	  .data = &uis8520_eth_clk_desc },
	{ .compatible = "sprd,uis8520-audcpglb-gate",	/* 0x56200000 */
	  .data = &uis8520_audcpglb_gate_desc },
	{ .compatible = "sprd,uis8520-audcpapb-gate",	/* 0x56390000 */
	  .data = &uis8520_audcpapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_uis8520_clk_ids);

static int uis8520_clk_probe(struct platform_device *pdev)
{
	const struct sprd_clk_desc *desc;
	struct sprd_reset *reset;
	int ret;

	desc = device_get_match_data(&pdev->dev);
	if (!desc)
		return -ENODEV;

	sprd_clk_regmap_init(pdev, desc);

	if (desc->num_resets > 0) {
		reset = devm_kzalloc(&pdev->dev, sizeof(*reset), GFP_KERNEL);
		if (!reset)
			return -ENOMEM;

		spin_lock_init(&reset->lock);
		reset->rcdev.of_node = pdev->dev.of_node;
		reset->rcdev.ops = &sprd_sc_reset_ops;
		reset->rcdev.nr_resets = desc->num_resets;
		reset->reset_map = desc->resets;
		reset->regmap = platform_get_drvdata(pdev);

		ret = devm_reset_controller_register(&pdev->dev, &reset->rcdev);
		if (ret)
			dev_err(&pdev->dev, "Failed to register reset controller\n");
	}

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver uis8520_clk_driver = {
	.probe	= uis8520_clk_probe,
	.driver	= {
		.name	= "uis8520-clk",
		.of_match_table	= sprd_uis8520_clk_ids,
	},
};
module_platform_driver(uis8520_clk_driver);

MODULE_DESCRIPTION("Unisoc UIS8520 Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:uis8520-clk");


