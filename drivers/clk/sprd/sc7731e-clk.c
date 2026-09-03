// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc SC7731E clock driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Xiongpeng Wu <xiongpeng.wu@unisoc.com>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,sc7731e-clk.h>
#include <dt-bindings/reset/sprd,sc7731e-reset.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"
#include "reset.h"

#define SC7731E_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* pmu apb pll gates clock */
static CLK_FIXED_FACTOR_FW_NAME(clk_26m_aud, "clk-26m-aud", "ext-26m", 1, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_13m, "clk-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_6m5, "clk-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_4m3, "clk-4m3", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_2m, "clk-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_1m, "clk-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_250k, "clk-250k", "ext-26m", 104, 1, 0);

static SPRD_PLL_SC_GATE_CLK_FW_NAME(cpll_gate, "cpll-gate", "ext-26m", 0x88,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(gpll_gate, "gpll-gate", "ext-26m", 0x90,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(mpll_gate, "mpll-gate", "ext-26m", 0x94,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(dpll_gate, "dpll-gate", "ext-26m", 0x98,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(bbpll_gate, "bbpll-gate", "ext-26m", 0x344,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sc7731e_pmu_gate_clks[] = {
	/* address base is 0x402b0000 */
	&cpll_gate.common,
	&gpll_gate.common,
	&mpll_gate.common,
	&dpll_gate.common,
	&bbpll_gate.common,
};

static struct clk_hw_onecell_data sc7731e_pmu_gate_hws = {
	.hws	= {
		[CLK_26M_AUD]		= &clk_26m_aud.hw,
		[CLK_13M]		= &clk_13m.hw,
		[CLK_6M5]		= &clk_6m5.hw,
		[CLK_4M3]		= &clk_4m3.hw,
		[CLK_2M]		= &clk_2m.hw,
		[CLK_1M]		= &clk_1m.hw,
		[CLK_250K]		= &clk_250k.hw,
		[CLK_CPLL_GATE]		= &cpll_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
		[CLK_MPLL_GATE]		= &mpll_gate.common.hw,
		[CLK_DPLL_GATE]		= &dpll_gate.common.hw,
		[CLK_BBPLL_GATE]	= &bbpll_gate.common.hw,
	},
	.num	= CLK_PMU_APB_NUM,
};

static const struct sprd_clk_desc sc7731e_pmu_gate_desc = {
	.clk_clks	= sc7731e_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_pmu_gate_clks),
	.hw_clks        = &sc7731e_pmu_gate_hws,
};

/* 0x402e0000 aon apb, pll register */
static const struct freq_table ftable[5] = {
	{ .ibias = 0, .max_freq = 951000000ULL, .vco_sel = 0 },
	{ .ibias = 1, .max_freq = 1131000000ULL, .vco_sel = 0 },
	{ .ibias = 2, .max_freq = 1145000000ULL, .vco_sel = 0 },
	{ .ibias = 3, .max_freq = 1600000000ULL, .vco_sel = 0 },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ, .vco_sel = INVALID_MAX_VCO_SEL},
};

static const struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 20,	.width = 1 },	/* div_s	*/
	{ .shift = 19,	.width = 1 },	/* mod_en	*/
	{ .shift = 18,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 11,	.width = 2 },	/* ibias	*/
	{ .shift = 0,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

#define f_mpll f_twpll
static SPRD_PLL_HW(mpll, "mpll", &mpll_gate.common.hw, 0x44,
		   2, ftable, f_mpll, 240,
		   1000, 1000, 0, 0);
#define f_dpll f_twpll
static SPRD_PLL_HW(dpll, "dpll", &dpll_gate.common.hw, 0x4c,
		   2, ftable, f_dpll, 240,
		   1000, 1000, 0, 0);

static SPRD_PLL_FW_NAME(twpll, "twpll", "ext-26m", 0x54,
			2, ftable, f_twpll, 240,
			1000, 1000, 0, 0);

static CLK_FIXED_FACTOR_HW(twpll_768m, "twpll-768m", &twpll.common.hw, 2, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_384m, "twpll-384m", &twpll.common.hw, 4, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_192m, "twpll-192m", &twpll.common.hw, 8, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_96m, "twpll-96m", &twpll.common.hw, 16, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_48m, "twpll-48m", &twpll.common.hw, 32, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_24m, "twpll-24m", &twpll.common.hw, 64, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_12m, "twpll-12m", &twpll.common.hw, 128, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_512m, "twpll-512m", &twpll.common.hw, 3, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_256m, "twpll-256m", &twpll.common.hw, 6, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_128m, "twpll-128m", &twpll.common.hw, 12, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_64m, "twpll-64m", &twpll.common.hw, 24, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_307m2, "twpll-307m2", &twpll.common.hw, 5, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_219m4, "twpll-219m4", &twpll.common.hw, 7, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_170m6, "twpll-170m6", &twpll.common.hw, 9, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_153m6, "twpll-153m6", &twpll.common.hw, 10, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_76m8, "twpll-76m8", &twpll.common.hw, 20, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_51m2, "twpll-51m2", &twpll.common.hw, 30, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_38m4, "twpll-38m4", &twpll.common.hw, 40, 1, 0);
static CLK_FIXED_FACTOR_HW(twpll_19m2, "twpll-19m2", &twpll.common.hw, 80, 1, 0);

#define f_cpll f_twpll
static SPRD_PLL_HW(cpll, "cpll", &cpll_gate.common.hw, 0x150,
		   2, ftable, f_cpll, 240,
		   1000, 1000, 0, 0);

static CLK_FIXED_FACTOR_HW(cpll_800m, "cpll-800m", &cpll.common.hw, 2, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_533m, "cpll-533m", &cpll.common.hw, 3, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_400m, "cpll-400m", &cpll.common.hw, 4, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_320m, "cpll-320m", &cpll.common.hw, 5, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_266m67, "cpll-266m67", &cpll.common.hw, 6, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_228m57, "cpll-228m57", &cpll.common.hw, 7, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_200m, "cpll-200m", &cpll.common.hw, 8, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_160m, "cpll-160m", &cpll.common.hw, 10, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_133m34, "cpll-133m34", &cpll.common.hw, 12, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_100m, "cpll-100m", &cpll.common.hw, 16, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_50m, "cpll-50m", &cpll.common.hw, 32, 1, 0);
static CLK_FIXED_FACTOR_HW(cpll_40m, "cpll-40m", &cpll.common.hw, 40, 1, 0);

static const struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 20,	.width = 1 },	/* div_s	*/
	{ .shift = 19,	.width = 1 },	/* mod_en	*/
	{ .shift = 18,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 11,	.width = 2 },	/* ibias	*/
	{ .shift = 0,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 13,	.width = 1 },	/* postdiv	*/
	{ .shift = 0,	.width = 0 },	/* refdiv	*/
	{ .shift = 0,	.width = 0 },	/* vco_sel	*/
};

static SPRD_PLL_HW(gpll, "gpll", &gpll_gate.common.hw, 0x158,
		   2, ftable, f_gpll, 240,
		   1000, 1000, 1, 600000000);

static CLK_FIXED_FACTOR_FW_NAME(bbpll_416m, "bbpll-416m", "bbpll",
			   3, 1, 0);

static struct sprd_clk_common *sc7731e_pll_clks[] = {
	/* address base is 0x402e0000 */
	&twpll.common,
	&cpll.common,
	&mpll.common,
	&dpll.common,
	&gpll.common,
};

static struct clk_hw_onecell_data sc7731e_pll_hws = {
	.hws	= {
		[CLK_TWPLL]		= &twpll.common.hw,
		[CLK_TWPLL_768M]	= &twpll_768m.hw,
		[CLK_TWPLL_384M]	= &twpll_384m.hw,
		[CLK_TWPLL_192M]	= &twpll_192m.hw,
		[CLK_TWPLL_96M]		= &twpll_96m.hw,
		[CLK_TWPLL_48M]		= &twpll_48m.hw,
		[CLK_TWPLL_24M]		= &twpll_24m.hw,
		[CLK_TWPLL_12M]		= &twpll_12m.hw,
		[CLK_TWPLL_512M]	= &twpll_512m.hw,
		[CLK_TWPLL_256M]	= &twpll_256m.hw,
		[CLK_TWPLL_128M]	= &twpll_128m.hw,
		[CLK_TWPLL_64M]		= &twpll_64m.hw,
		[CLK_TWPLL_307M2]	= &twpll_307m2.hw,
		[CLK_TWPLL_219M4]	= &twpll_219m4.hw,
		[CLK_TWPLL_170M6]	= &twpll_170m6.hw,
		[CLK_TWPLL_153M6]	= &twpll_153m6.hw,
		[CLK_TWPLL_76M8]	= &twpll_76m8.hw,
		[CLK_TWPLL_51M2]	= &twpll_51m2.hw,
		[CLK_TWPLL_38M4]	= &twpll_38m4.hw,
		[CLK_TWPLL_19M2]	= &twpll_19m2.hw,
		[CLK_CPLL]		= &cpll.common.hw,
		[CLK_CPPLL_800M]	= &cpll_800m.hw,
		[CLK_CPPLL_533M]	= &cpll_533m.hw,
		[CLK_CPPLL_400M]	= &cpll_400m.hw,
		[CLK_CPPLL_320M]	= &cpll_320m.hw,
		[CLK_CPPLL_266M]	= &cpll_266m67.hw,
		[CLK_CPPLL_228M]	= &cpll_228m57.hw,
		[CLK_CPPLL_200M]	= &cpll_200m.hw,
		[CLK_CPPLL_160M]	= &cpll_160m.hw,
		[CLK_CPPLL_133M]	= &cpll_133m34.hw,
		[CLK_CPPLL_100M]	= &cpll_100m.hw,
		[CLK_CPPLL_50M]		= &cpll_50m.hw,
		[CLK_CPPLL_40M]		= &cpll_40m.hw,
		[CLK_MPLL]		= &mpll.common.hw,
		[CLK_DPLL]		= &dpll.common.hw,
		[CLK_GPLL]		= &gpll.common.hw,
		[CLK_BBPLL_416M]	= &bbpll_416m.hw,

	},
	.num	= CLK_AON_PLL_NUM,
};

static const struct sprd_clk_desc sc7731e_pll_desc = {
	.clk_clks	= sc7731e_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_pll_clks),
	.hw_clks        = &sc7731e_pll_hws,
};

/* 0x20e00000 ap-ahb clocks */
static SPRD_SC_GATE_CLK_FW_NAME(dsi_eb, "dsi-eb", "ext-26m", 0x0,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dispc_eb, "dispc-eb", "ext-26m", 0x0,
				0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gsp_eb, "gsp-eb", "ext-26m", 0x0,
				0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(otg_eb, "otg-eb", "ext-26m", 0x0,
				0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dma_eb, "dma-eb", "ext-26m", 0x0,
				0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ce_eb, "ce-eb", "ext-26m", 0x0,
				0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_eb, "sdio0-eb", "ext-26m", 0x0,
				0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_eb, "nandc-eb", "ext-26m", 0x0,
				0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_eb, "emmc-eb", "ext-26m", 0x0,
				0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ce_sec_eb, "ce-sec-eb", "ext-26m", 0x0,
				0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_32k_eb, "emmc-32k-eb", "ext-26m", 0x0,
				0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_32k_eb, "sdio0-32k-eb", "ext-26m", 0x0,
				0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_ecc_eb, "nandc-ecc-eb", "ext-26m", 0x0,
				0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data mcu_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_512m.hw },
	{ .hw = &twpll_768m.hw },
	{ .hw = &dpll.common.hw },
	{ .hw = &cpll.common.hw },
	{ .hw = &twpll.common.hw },
	{ .hw = &mpll.common.hw },
};

static SPRD_COMP_CLK_DATA(mcu, "mcu", mcu_parents, 0x54,
		     0, 3, 4, 3, 0);
static SPRD_DIV_CLK_HW(ca7_axi, "ca7-axi", &mcu.common.hw,
		       0x54, 8, 3, 0);
static SPRD_DIV_CLK_HW(ca7_dbg, "ca7-dbg", &mcu.common.hw,
		       0x54, 16, 3, 0);

static struct sprd_clk_common *sc7731e_apahb_gate[] = {
	&dsi_eb.common,
	&dispc_eb.common,
	&gsp_eb.common,
	&otg_eb.common,
	&dma_eb.common,
	&ce_eb.common,
	&sdio0_eb.common,
	&nandc_eb.common,
	&emmc_eb.common,
	&ce_sec_eb.common,
	&emmc_32k_eb.common,
	&sdio0_32k_eb.common,
	&nandc_ecc_eb.common,
	&mcu.common,
	&ca7_axi.common,
	&ca7_dbg.common,
};

static struct clk_hw_onecell_data sc7731e_apahb_gate_hws = {
	.hws	= {
		[CLK_DSI_EB]		= &dsi_eb.common.hw,
		[CLK_DISPC_EB]		= &dispc_eb.common.hw,
		[CLK_GSP_EB]		= &gsp_eb.common.hw,
		[CLK_OTG_EB]		= &otg_eb.common.hw,
		[CLK_DMA_EB]		= &dma_eb.common.hw,
		[CLK_CE_EB]		= &ce_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_NANDC_EB]		= &nandc_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_CE_SEC_EB]		= &ce_sec_eb.common.hw,
		[CLK_EMMC_32K_EB]	= &emmc_32k_eb.common.hw,
		[CLK_SDIO0_32K_EB]	= &sdio0_32k_eb.common.hw,
		[CLK_NANDC_ECC_EB]	= &nandc_ecc_eb.common.hw,
		[CLK_MCU]		= &mcu.common.hw,
		[CLK_CA7_AXI]		= &ca7_axi.common.hw,
		[CLK_CA7_DBG]		= &ca7_dbg.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_apahb_gate_desc = {
	.clk_clks	= sc7731e_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_apahb_gate),
	.hw_clks	= &sc7731e_apahb_gate_hws,
};

/* 0x21500000 ap clocks */
static const struct clk_parent_data ap_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_76m8.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &cpll_266m67.hw },
};
static SPRD_MUX_CLK_DATA(ap_axi, "ap-axi", ap_axi_parents,
			 0x20, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data ap_ahb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_76m8.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_192m.hw },
	{ .hw = &cpll_200m.hw },
};
static SPRD_MUX_CLK_DATA(ap_ahb, "ap-ahb", ap_ahb_parents,
			 0x24, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data ap_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_64m.hw },
	{ .hw = &twpll_96m.hw },
	{ .hw = &cpll_100m.hw },
	{ .hw = &twpll_128m.hw },
};
static SPRD_MUX_CLK_DATA(ap_apb, "ap-apb", ap_apb_parents,
			 0x28, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data gsp_parents[] = {
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &twpll_307m2.hw },
	{ .hw = &cpll_320m.hw },
	{ .hw = &twpll_384m.hw },
};
static SPRD_MUX_CLK_DATA(gsp, "gsp", gsp_parents,
			 0x2c, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data dispc0_parents[] = {
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_192m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &cpll_320m.hw },
	{ .hw = &twpll_384m.hw },
};
static SPRD_MUX_CLK_DATA(dispc0, "dispc0", dispc0_parents,
			 0x30, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data dispc0_dpi_parents[] = {
	{ .hw = &twpll_96m.hw },
	{ .hw = &cpll_100m.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_192m.hw },
};
static SPRD_COMP_CLK_DATA(dispc0_dpi, "dispc0-dpi", dispc0_dpi_parents,
			  0x34, 0, 3, 8, 4, 0);

static SPRD_GATE_CLK_FW_NAME(dsi_rxsec_en, "dsi-rxsec-en", "ext-26m",
			     0x38, BIT(16), 0, 0);
static SPRD_GATE_CLK_FW_NAME(dlanebyte_en, "dlanebyte-en", "ext-26m",
			     0x3c, BIT(16), 0, 0);
static SPRD_GATE_CLK_FW_NAME(otg_utmi_en, "otg-utmi-en", "ext-26m",
			     0x4c, BIT(16), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data ap_uart_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_48m.hw },
	{ .hw = &twpll_51m2.hw },
	{ .hw = &twpll_96m.hw },
};
static SPRD_COMP_CLK_DATA(ap_uart0, "ap-uart0", ap_uart_parents,
			  0x50, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_uart1, "ap-uart1", ap_uart_parents,
			  0x54, 0, 2, 8, 3, 0);

static const struct clk_parent_data i2c_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_48m.hw },
	{ .hw = &twpll_51m2.hw },
	{ .hw = &twpll_153m6.hw },
};
static SPRD_COMP_CLK_DATA(ap_i2c0, "ap-i2c0", i2c_parents,
			  0x58, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_i2c1, "ap-i2c1", i2c_parents,
			  0x5c, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_i2c2, "ap-i2c2", i2c_parents,
			  0x60, 0, 2, 8, 3, 0);

static const struct clk_parent_data spi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_100m.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_192m.hw },
};
static SPRD_COMP_CLK_DATA(ap_spi0, "ap-spi0", spi_parents,
			  0x64, 0, 3, 8, 3, 0);

static const struct clk_parent_data iis_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_153m6.hw },
};
static SPRD_COMP_CLK_DATA(ap_iis0, "ap-iis0", iis_parents,
			  0x68, 0, 2, 8, 3, 0);

static const struct clk_parent_data ap_ce_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_96m.hw },
	{ .hw = &cpll_100m.hw },
	{ .hw = &twpll_192m.hw },
	{ .hw = &twpll_256m.hw },
};
static SPRD_MUX_CLK_DATA(ap_ce, "ap-ce", ap_ce_parents,
			 0x6c, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data nandc_ecc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_200m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &twpll_307m2.hw },
};
static SPRD_MUX_CLK_DATA(nandc_ecc, "nandc-ecc", nandc_ecc_parents,
			 0x78, 0, 2, SC7731E_MUX_FLAG);

static struct sprd_clk_common *sc7731e_ap_clks[] = {
	&ap_axi.common,
	&ap_ahb.common,
	&ap_apb.common,
	&gsp.common,
	&dispc0.common,
	&dispc0_dpi.common,
	&dsi_rxsec_en.common,
	&dlanebyte_en.common,
	&otg_utmi_en.common,
	&ap_uart0.common,
	&ap_uart1.common,
	&ap_i2c0.common,
	&ap_i2c1.common,
	&ap_i2c2.common,
	&ap_spi0.common,
	&ap_iis0.common,
	&ap_ce.common,
	&nandc_ecc.common,
};

static struct clk_hw_onecell_data sc7731e_ap_clk_hws = {
	.hws	= {
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_AP_AHB]		= &ap_ahb.common.hw,
		[CLK_AP_APB]		= &ap_apb.common.hw,
		[CLK_GSP]		= &gsp.common.hw,
		[CLK_DISPC0]		= &dispc0.common.hw,
		[CLK_DISPC0_DPI]	= &dispc0_dpi.common.hw,
		[CLK_DSI_RXSEC]		= &dsi_rxsec_en.common.hw,
		[CLK_DLANEBYTE]		= &dlanebyte_en.common.hw,
		[CLK_OTG_UTMI]		= &otg_utmi_en.common.hw,
		[CLK_AP_UART0]		= &ap_uart0.common.hw,
		[CLK_AP_UART1]		= &ap_uart1.common.hw,
		[CLK_AP_I2C0]		= &ap_i2c0.common.hw,
		[CLK_AP_I2C1]		= &ap_i2c1.common.hw,
		[CLK_AP_I2C2]		= &ap_i2c2.common.hw,
		[CLK_AP_SPI0]		= &ap_spi0.common.hw,
		[CLK_AP_IIS0]		= &ap_iis0.common.hw,
		[CLK_AP_CE]		= &ap_ce.common.hw,
		[CLK_NANDC_ECC]		= &nandc_ecc.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc sc7731e_ap_clk_desc = {
	.clk_clks	= sc7731e_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_ap_clks),
	.hw_clks	= &sc7731e_ap_clk_hws,
};

/* 0x402d0000 aon clocks */
static const struct clk_parent_data emc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_384m.hw },
	{ .hw = &twpll_512m.hw },
	{ .hw = &twpll_768m.hw },
	{ .hw = &cpll_800m.hw },
	{ .hw = &dpll.common.hw },
};
static SPRD_COMP_CLK_DATA(emc, "emc", emc_parents,
			  0x220, 0, 3, 8, 2, 0);

static const struct clk_parent_data aon_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_100m.hw },
	{ .hw = &twpll_128m.hw },
};
static SPRD_COMP_CLK_DATA(aon_apb, "aon-apb", aon_apb_parents,
			  0x230, 0, 2, 8, 2, 0);

static const struct clk_parent_data adi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_50m.hw },
	{ .hw = &twpll_51m2.hw },
};
static SPRD_MUX_CLK_DATA(adi, "adi", adi_parents,
			 0x234, 0, 2, SC7731E_MUX_FLAG);

static const struct clk_parent_data pwm_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_26m_aud.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_40m.hw },
	{ .hw = &twpll_48m.hw },
};
static SPRD_MUX_CLK_DATA(pwm0, "pwm0", pwm_parents,
			 0x248, 0, 3, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK_DATA(pwm1, "pwm1", pwm_parents,
			 0x24c, 0, 3, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK_DATA(pwm2, "pwm2", pwm_parents,
			 0x250, 0, 3, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK_DATA(pwm3, "pwm3", pwm_parents,
			 0x254, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data aon_thm_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_250k.hw },
};
static SPRD_MUX_CLK_DATA(aon_thm, "aon-thm", aon_thm_parents,
			 0x268, 0, 1, SC7731E_MUX_FLAG);

static const struct clk_parent_data aon_i2c_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_48m.hw },
	{ .hw = &twpll_51m2.hw },
	{ .hw = &cpll_133m34.hw },
	{ .hw = &twpll_153m6.hw },
};
static SPRD_COMP_CLK_DATA(aon_i2c0, "aon-i2c0", aon_i2c_parents,
			  0x26c, 0, 2, 8, 3, 0);

static const struct clk_parent_data avs_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &twpll_48m.hw },
	{ .hw = &cpll_50m.hw },
	{ .hw = &twpll_51m2.hw },
	{ .hw = &twpll_96m.hw },
};
static SPRD_MUX_CLK_DATA(avs, "avs", avs_parents,
			 0x270, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data audif_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_38m4.hw },
	{ .hw = &cpll_50m.hw },
	{ .hw = &twpll_51m2.hw },
};
static SPRD_MUX_CLK_DATA(audif, "audif", audif_parents,
			 0x278, 0, 2, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(iis_da0_en, "iis-da0-en", "ext-26m",
			     0x280, BIT(16), 0, 0);
static SPRD_GATE_CLK_FW_NAME(iis0_ad0_en, "iis0-ad0-en", "ext-26m",
			     0x284, BIT(16), 0, 0);
static SPRD_GATE_CLK_FW_NAME(iis1_ad0_en, "iis1-ad0-en", "ext-26m",
			     0x288, BIT(16), 0, 0);

static const struct clk_parent_data cpu_dap_parents[] = {
	{ .hw = &clk_26m_aud.hw },
	{ .hw = &twpll_76m8.hw },
	{ .hw = &cpll_100m.hw },
	{ .hw = &twpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(cpu_dap, "cpu-dap", cpu_dap_parents,
			 0x28c, 0, 2, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(cdap_mtck_en, "cdap-mtck-en", "ext-26m",
			     0x290, BIT(16), 0, 0);

static const struct clk_parent_data cpu_ts_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_128m.hw },
	{ .hw = &cpll_133m34.hw },
	{ .hw = &twpll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(cpu_ts, "cpu-ts", cpu_ts_parents,
			 0x294, 0, 3, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(djtag_tck_en, "djtag-tck-en", "ext-26m",
			     0x298, BIT(16), 0, 0);

static const struct clk_parent_data emc_ref_parents[] = {
	{ .hw = &clk_6m5.hw },
	{ .hw = &clk_13m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(emc_ref, "emc-ref", emc_ref_parents,
			 0x2a8, 0, 2, SC7731E_MUX_FLAG);

static const struct clk_parent_data cssys_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_96m.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &cpll_266m67.hw },
	{ .hw = &twpll_384m.hw },
	{ .hw = &twpll_512m.hw },
};
static SPRD_COMP_CLK_DATA(cssys, "cssys-clk", cssys_parents,
			  0x2ac, 0, 3, 8, 2, 0);

static SPRD_DIV_CLK_HW(cssys_ca7, "cssys-ca7", &cssys.common.hw,
		       0x2b0, 8, 1, 0);

static const struct clk_parent_data sdio_parents[] = {
	{ .hw = &clk_1m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_307m2.hw },
	{ .hw = &twpll_384m.hw },
	{ .hw = &cpll_400m.hw },
	{ .hw = &bbpll_416m.hw },
};
static SPRD_MUX_CLK_DATA(sdio0_2x, "sdio0-2x", sdio_parents,
			 0x2bc, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data nandc_parents[] = {
	{ .hw = &clk_1m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_170m6.hw },
	{ .hw = &cpll_200m.hw },
	{ .hw = &twpll_219m4.hw },
	{ .hw = &cpll_228m57.hw },
	{ .hw = &cpll_266m67.hw },
};
static SPRD_MUX_CLK_DATA(nandc_2x, "nandc-2x", nandc_parents,
			 0x2c4, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data emmc_parents[] = {
	{ .hw = &clk_1m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_307m2.hw },
	{ .hw = &twpll_384m.hw },
	{ .hw = &cpll_400m.hw },
	{ .hw = &bbpll_416m.hw },
};
static SPRD_MUX_CLK_DATA(emmc_2x, "emmc-2x", emmc_parents,
			 0x2cc, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data ap_hs_spi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_128m.hw },
	{ .hw = &cpll_133m34.hw },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_192m.hw },
};
static SPRD_COMP_CLK_DATA(ap_hs_spi, "ap-hs-spi", ap_hs_spi_parents,
			  0x2e0, 0, 3, 8, 3, 0);

static const struct clk_parent_data sdphy_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_40m.hw },
	{ .hw = &twpll_48m.hw },
};
static SPRD_MUX_CLK_DATA(sdphy_apb, "sdphy-apb", sdphy_apb_parents,
			 0x2e4, 0, 2, SC7731E_MUX_FLAG);

static const struct clk_parent_data analog_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_40m.hw },
	{ .hw = &twpll_48m.hw },
};
static SPRD_MUX_CLK_DATA(analog_apb, "analog-apb", analog_apb_parents,
			 0x2f0, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data io_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &cpll_40m.hw },
	{ .hw = &twpll_48m.hw },
};
static SPRD_COMP_CLK_DATA(io_apb, "io-apb", io_apb_parents,
			  0x2f4, 0, 2, 8, 2, 0);

static SPRD_GATE_CLK_FW_NAME(dtck_hw_en, "dtck-hw-en", "ext-26m",
			     0x2f8, BIT(16), 0, 0);

static struct sprd_clk_common *sc7731e_aon_clks[] = {
	&emc.common,
	&aon_apb.common,
	&adi.common,
	&pwm0.common,
	&pwm1.common,
	&pwm2.common,
	&pwm3.common,
	&aon_thm.common,
	&aon_i2c0.common,
	&avs.common,
	&audif.common,
	&iis_da0_en.common,
	&iis0_ad0_en.common,
	&iis1_ad0_en.common,
	&cpu_dap.common,
	&cdap_mtck_en.common,
	&cpu_ts.common,
	&djtag_tck_en.common,
	&emc_ref.common,
	&cssys.common,
	&cssys_ca7.common,
	&sdio0_2x.common,
	&nandc_2x.common,
	&emmc_2x.common,
	&ap_hs_spi.common,
	&sdphy_apb.common,
	&analog_apb.common,
	&io_apb.common,
	&dtck_hw_en.common,
};

static struct clk_hw_onecell_data sc7731e_aon_clk_hws = {
	.hws	= {
		[CLK_EMC]		= &emc.common.hw,
		[CLK_AON_APB]		= &aon_apb.common.hw,
		[CLK_ADI]		= &adi.common.hw,
		[CLK_PWM0]		= &pwm0.common.hw,
		[CLK_PWM1]		= &pwm1.common.hw,
		[CLK_PWM2]		= &pwm2.common.hw,
		[CLK_PWM3]		= &pwm3.common.hw,
		[CLK_AON_THM]		= &aon_thm.common.hw,
		[CLK_AON_I2C0]		= &aon_i2c0.common.hw,
		[CLK_AVS]		= &avs.common.hw,
		[CLK_AUDIF]		= &audif.common.hw,
		[CLK_IIS_DA0]		= &iis_da0_en.common.hw,
		[CLK_IIS0_AD0]		= &iis0_ad0_en.common.hw,
		[CLK_IIS1_AD0]		= &iis1_ad0_en.common.hw,
		[CLK_CPU_DAP]		= &cpu_dap.common.hw,
		[CLK_CDAP_MTCK]		= &cdap_mtck_en.common.hw,
		[CLK_CPU_TS]		= &cpu_ts.common.hw,
		[CLK_DJTAG_TCK]		= &djtag_tck_en.common.hw,
		[CLK_EMC_REF]		= &emc_ref.common.hw,
		[CLK_CSSYS]		= &cssys.common.hw,
		[CLK_CSSYS_CA7]		= &cssys_ca7.common.hw,
		[CLK_SDIO0_2X]		= &sdio0_2x.common.hw,
		[CLK_NANDC_2X]		= &nandc_2x.common.hw,
		[CLK_EMMC_2X]		= &emmc_2x.common.hw,
		[CLK_AP_HS_SPI]		= &ap_hs_spi.common.hw,
		[CLK_SDPHY_APB]		= &sdphy_apb.common.hw,
		[CLK_ANALOG_APB]	= &analog_apb.common.hw,
		[CLK_IO_APB]		= &io_apb.common.hw,
		[CLK_DTCK_HW]		= &dtck_hw_en.common.hw,
	},
	.num	= CLK_AON_CLK_NUM,
};

static const struct sprd_clk_desc sc7731e_aon_clk_desc = {
	.clk_clks	= sc7731e_aon_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_aon_clks),
	.hw_clks	= &sc7731e_aon_clk_hws,
};

/* 0x402e0000 aon gate clocks */
static SPRD_SC_GATE_CLK_FW_NAME(gpio_eb, "gpio-eb", "ext-26m", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm0_eb, "pwm0-eb", "ext-26m", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm1_eb, "pwm1-eb", "ext-26m", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm2_eb, "pwm2-eb", "ext-26m", 0x0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm3_eb, "pwm3-eb", "ext-26m", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(kpd_eb,	 "kpd-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_syst_eb, "aon-syst-eb", "ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_syst_eb, "ap-syst-eb", "ext-26m", 0x0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_tmr_eb, "aon-tmr-eb", "ext-26m", 0x0,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr0_eb, "ap-tmr0-eb", "ext-26m", 0x0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(efuse_eb, "efuse-eb", "ext-26m", 0x0,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_eb,	 "eic-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(intc_eb, "intc-eb", "ext-26m", 0x0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(adi_eb,	 "adi-eb", "ext-26m", 0x0,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(audif_eb, "audif-eb", "ext-26m", 0x0,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aud_eb,	 "aud-eb", "ext-26m", 0x0,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(vbc_eb,	 "vbc-eb", "ext-26m", 0x0,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pin_eb,	 "pin-eb", "ext-26m", 0x0,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(splk_eb, "splk-eb", "ext-26m", 0x0,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_wdg_eb, "ap-wdg-eb", "ext-26m", 0x0,
			0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mm_eb, "mm-eb", "ext-26m", 0x0,
			0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_apb_ckg_eb, "aon-apb-ckg-eb", "ext-26m", 0x0,
			0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gpu_eb, "gpu-eb", "ext-26m", 0x0,
			0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ca53_ts0_eb, "ca53-ts0-eb", "ext-26m", 0x0,
			0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ca53_ts1_eb, "ca53-ts1-eb", "ext-26m", 0x0,
			0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ca53_dap_eb, "ca53-dap-eb", "ext-26m", 0x0,
			0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c_eb, "i2c-eb", "ext-26m", 0x0,
			0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pmu_eb, "pmu-eb", "ext-26m",
			0x4, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(thm_eb, "thm-eb", "ext-26m",
			0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux0_eb, "aux0-eb", "ext-26m",
			0x4, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux1_eb, "aux1-eb", "ext-26m",
			0x4, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux2_eb, "aux2-eb", "ext-26m",
			0x4, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(probe_eb, "probe-eb", "ext-26m",
			0x4, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(avs_eb, "avs-eb", "ext-26m",
			0x4, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emc_ref_eb, "emc-ref-eb", "ext-26m",
			0x4, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ca53_wdg_eb, "ca53-wdg-eb", "ext-26m",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr1_eb, "ap-tmr1-eb", "ext-26m",
			0x4, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr2_eb, "ap-tmr2-eb", "ext-26m",
			0x4, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(disp_emc_eb, "disp-emc-eb", "ext-26m",
			0x4, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gsp_emc_eb, "gsp-emc-eb", "ext-26m",
			0x4, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mm_vsp_eb, "mm-vsp-eb", "ext-26m",
			0x4, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mdar_eb, "mdar-eb", "ext-26m",
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rtc4m0_cal_eb, "rtc4m0-cal-eb", "ext-26m",
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(djtag_eb, "djtag-eb", "ext-26m",
			0x4, 0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mbox_eb, "mbox-eb", "ext-26m",
			0x4, 0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_dma_eb, "aon-dma-eb", "ext-26m",
			0x4, 0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cm4_djtag_eb, "cm4-djtag-eb", "ext-26m",
			0x4, 0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(wcn_eb, "wcn-eb", "ext-26m",
			0x4, 0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_apb_def_eb, "aon-apb-def-eb", "ext-26m",
			0x4, 0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dbg_eb, "dbg-eb", "ext-26m",
			0x4, 0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dbg_emc_eb, "dbg-emc-eb", "ext-26m",
			0x4, 0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cross_trig_eb, "cross-trig-eb",	"ext-26m",
			0x4, 0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(serdes_dphy_eb, "serdes-dphy-eb", "ext-26m",
			0x4, 0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(arch_rtc_eb, "arch-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(kpd_rtc_eb, "kpd-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_syst_rtc_eb, "ap-syst-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_rtc_eb, "eic-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_rtcdv5_eb, "eic-rtcdv5-eb", "ext-26m",
			0x10, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ca53_wdg_rtc_eb, "ca53-wdg-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(thm_rtc_eb, "thm-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(athma_rtc_eb, "athma-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gthma_rtc_eb, "gthma-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(athma_rtc_a_eb, "athma-rtc-a-eb", "ext-26m",
			0x10, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gthma_rtc_a_eb, "gthma-rtc-a-eb", "ext-26m",
			0x10, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dxco_lc_rtc_eb, "dxco-lc-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(bb_cal_rtc_eb, "bb-cal-rtc-eb", "ext-26m",
			0x10, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(audio_gate, "audio-gate", "ext-26m", 0x14,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data aux_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_26m_aud.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_COMP_CLK_DATA(aux0, "aux0", aux_parents, 0x88,
			  0, 2, 16, 4, 0);

static SPRD_SC_GATE_CLK_FW_NAME(cssys_eb, "cssys-eb", "ext-26m", 0xb0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dmc_eb, "dmc-eb", "ext-26m", 0xb0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pub_reg_eb, "pub-reg-eb", "ext-26m", 0xb0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rosc_eb, "rosc-eb", "ext-26m", 0xb0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(s_d_cfg_eb, "s-d-cfg-eb", "ext-26m", 0xb0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(s_d_ref_eb, "s-d-ref-eb", "ext-26m", 0xb0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(b_dma_eb, "b-dma-eb", "ext-26m", 0xb0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(anlg_eb, "anlg-eb", "ext-26m", 0xb0,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pin_apb_eb, "pin-apb-eb", "ext-26m", 0xb0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(anlg_apb_eb, "anlg-apb-eb", "ext-26m", 0xb0,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(bsmtmr_eb, "bsmtmr-eb", "ext-26m", 0xb0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_dap_eb, "ap-dap-eb", "ext-26m", 0xb0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(emmc_1x_eb, "emmc-1x-eb", "ext-26m", 0x134,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_2x_eb, "emmc-2x-eb", "ext-26m", 0x134,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_1x_eb, "sdio0-1x-eb", "ext-26m", 0x134,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_2x_eb, "sdio0-2x-eb", "ext-26m", 0x134,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio1_1x_eb, "sdio1-1x-eb", "ext-26m", 0x134,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio1_2x_eb, "sdio1-2x-eb", "ext-26m", 0x134,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_1x_eb, "nandc-1x-eb", "ext-26m", 0x134,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_2x_eb, "nandc-2x-eb", "ext-26m", 0x134,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cssys_ca7_eb, "cssys-ca7-eb", "ext-26m", 0x134,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_hs_spi_eb, "ap-hs-spi-eb", "ext-26m", 0x134,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(det_32k_eb, "det-32k-eb", "ext-26m", 0x134,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(tmr_eb, "tmr-eb", "ext-26m", 0x134,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apll_test_eb, "apll-test-eb", "ext-26m", 0x134,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc7731e_aonapb_gate_clks[] = {
	&gpio_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&ap_tmr0_eb.common,
	&efuse_eb.common,
	&eic_eb.common,
	&intc_eb.common,
	&adi_eb.common,
	&audif_eb.common,
	&aud_eb.common,
	&vbc_eb.common,
	&pin_eb.common,
	&splk_eb.common,
	&ap_wdg_eb.common,
	&mm_eb.common,
	&aon_apb_ckg_eb.common,
	&gpu_eb.common,
	&ca53_ts0_eb.common,
	&ca53_ts1_eb.common,
	&ca53_dap_eb.common,
	&i2c_eb.common,
	&pmu_eb.common,
	&thm_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&avs_eb.common,
	&emc_ref_eb.common,
	&ca53_wdg_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&disp_emc_eb.common,
	&gsp_emc_eb.common,
	&mm_vsp_eb.common,
	&mdar_eb.common,
	&rtc4m0_cal_eb.common,
	&djtag_eb.common,
	&mbox_eb.common,
	&aon_dma_eb.common,
	&cm4_djtag_eb.common,
	&wcn_eb.common,
	&aon_apb_def_eb.common,
	&dbg_eb.common,
	&dbg_emc_eb.common,
	&cross_trig_eb.common,
	&serdes_dphy_eb.common,
	&arch_rtc_eb.common,
	&kpd_rtc_eb.common,
	&aon_syst_rtc_eb.common,
	&ap_syst_rtc_eb.common,
	&aon_tmr_rtc_eb.common,
	&ap_tmr0_rtc_eb.common,
	&eic_rtc_eb.common,
	&eic_rtcdv5_eb.common,
	&ap_wdg_rtc_eb.common,
	&ca53_wdg_rtc_eb.common,
	&thm_rtc_eb.common,
	&athma_rtc_eb.common,
	&gthma_rtc_eb.common,
	&athma_rtc_a_eb.common,
	&gthma_rtc_a_eb.common,
	&ap_tmr1_rtc_eb.common,
	&ap_tmr2_rtc_eb.common,
	&dxco_lc_rtc_eb.common,
	&bb_cal_rtc_eb.common,
	&audio_gate.common,
	&aux0.common,
	&cssys_eb.common,
	&dmc_eb.common,
	&pub_reg_eb.common,
	&rosc_eb.common,
	&s_d_cfg_eb.common,
	&s_d_ref_eb.common,
	&b_dma_eb.common,
	&anlg_eb.common,
	&pin_apb_eb.common,
	&anlg_apb_eb.common,
	&bsmtmr_eb.common,
	&ap_dap_eb.common,
	&emmc_1x_eb.common,
	&emmc_2x_eb.common,
	&sdio0_1x_eb.common,
	&sdio0_2x_eb.common,
	&sdio1_1x_eb.common,
	&sdio1_2x_eb.common,
	&nandc_1x_eb.common,
	&nandc_2x_eb.common,
	&cssys_ca7_eb.common,
	&ap_hs_spi_eb.common,
	&det_32k_eb.common,
	&tmr_eb.common,
	&apll_test_eb.common,
};

static struct clk_hw_onecell_data sc7731e_aonapb_gate_hws = {
	.hws	= {
		[CLK_GPIO_EB]		= &gpio_eb.common.hw,
		[CLK_PWM0_EB]		= &pwm0_eb.common.hw,
		[CLK_PWM1_EB]		= &pwm1_eb.common.hw,
		[CLK_PWM2_EB]		= &pwm2_eb.common.hw,
		[CLK_PWM3_EB]		= &pwm3_eb.common.hw,
		[CLK_KPD_EB]		= &kpd_eb.common.hw,
		[CLK_AON_SYST_EB]	= &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB]	= &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB]	= &aon_tmr_eb.common.hw,
		[CLK_AP_TMR0_EB]	= &ap_tmr0_eb.common.hw,
		[CLK_EFUSE_EB]		= &efuse_eb.common.hw,
		[CLK_EIC_EB]		= &eic_eb.common.hw,
		[CLK_INTC_EB]		= &intc_eb.common.hw,
		[CLK_ADI_EB]		= &adi_eb.common.hw,
		[CLK_AUDIF_EB]		= &audif_eb.common.hw,
		[CLK_AUD_EB]		= &aud_eb.common.hw,
		[CLK_VBC_EB]		= &vbc_eb.common.hw,
		[CLK_PIN_EB]		= &pin_eb.common.hw,
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_AON_APB_CKG_EB]	= &aon_apb_ckg_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_CA53_TS0_EB]	= &ca53_ts0_eb.common.hw,
		[CLK_CA53_TS1_EB]	= &ca53_ts1_eb.common.hw,
		[CLK_CA53_DAP_EB]	= &ca53_dap_eb.common.hw,
		[CLK_I2C_EB]		= &i2c_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_THM_EB]		= &thm_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_AVS_EB]		= &avs_eb.common.hw,
		[CLK_EMC_REF_EB]	= &emc_ref_eb.common.hw,
		[CLK_CA53_WDG_EB]	= &ca53_wdg_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_DISP_EMC_EB]	= &disp_emc_eb.common.hw,
		[CLK_GSP_EMC_EB]	= &gsp_emc_eb.common.hw,
		[CLK_MM_VSP_EB]		= &mm_vsp_eb.common.hw,
		[CLK_MDAR_EB]		= &mdar_eb.common.hw,
		[CLK_RTC4M0_CAL_EB]	= &rtc4m0_cal_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_AON_DMA_EB]	= &aon_dma_eb.common.hw,
		[CLK_CM4_DJTAG_EB]	= &cm4_djtag_eb.common.hw,
		[CLK_WCN_EB]		= &wcn_eb.common.hw,
		[CLK_AON_APB_DEF_EB]	= &aon_apb_def_eb.common.hw,
		[CLK_DBG_EB]		= &dbg_eb.common.hw,
		[CLK_DBG_EMC_EB]	= &dbg_emc_eb.common.hw,
		[CLK_CROSS_TRIG_EB]	= &cross_trig_eb.common.hw,
		[CLK_SERDES_DPHY_EB]	= &serdes_dphy_eb.common.hw,
		[CLK_ARCH_RTC_EB]	= &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB]	= &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB]	= &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB]	= &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB]	= &aon_tmr_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB]	= &ap_tmr0_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB]	= &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB]	= &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB]	= &ap_wdg_rtc_eb.common.hw,
		[CLK_CA53_WDG_RTC_EB]	= &ca53_wdg_rtc_eb.common.hw,
		[CLK_THM_RTC_EB]	= &thm_rtc_eb.common.hw,
		[CLK_ATHMA_RTC_EB]	= &athma_rtc_eb.common.hw,
		[CLK_GTHMA_RTC_EB]	= &gthma_rtc_eb.common.hw,
		[CLK_ATHMA_RTC_A_EB]	= &athma_rtc_a_eb.common.hw,
		[CLK_GTHMA_RTC_A_EB]	= &gthma_rtc_a_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB]	= &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB]	= &ap_tmr2_rtc_eb.common.hw,
		[CLK_DXCO_LC_RTC_EB]	= &dxco_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB]	= &bb_cal_rtc_eb.common.hw,
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
		[CLK_AUX0]		= &aux0.common.hw,
		[CLK_CSSYS_EB]		= &cssys_eb.common.hw,
		[CLK_DMC_EB]		= &dmc_eb.common.hw,
		[CLK_PUB_REG_EB]	= &pub_reg_eb.common.hw,
		[CLK_ROSC_EB]		= &rosc_eb.common.hw,
		[CLK_S_D_CFG_EB]	= &s_d_cfg_eb.common.hw,
		[CLK_S_D_REF_EB]	= &s_d_ref_eb.common.hw,
		[CLK_B_DMA_EB]		= &b_dma_eb.common.hw,
		[CLK_ANLG_EB]		= &anlg_eb.common.hw,
		[CLK_PIN_APB_EB]	= &pin_apb_eb.common.hw,
		[CLK_ANLG_APB_EB]	= &anlg_apb_eb.common.hw,
		[CLK_BSMTMR_EB]		= &bsmtmr_eb.common.hw,
		[CLK_AP_DAP_EB]		= &ap_dap_eb.common.hw,
		[CLK_EMMC_1X_EB]	= &emmc_1x_eb.common.hw,
		[CLK_EMMC_2X_EB]	= &emmc_2x_eb.common.hw,
		[CLK_SDIO0_1X_EB]	= &sdio0_1x_eb.common.hw,
		[CLK_SDIO0_2X_EB]	= &sdio0_2x_eb.common.hw,
		[CLK_SDIO1_1X_EB]	= &sdio1_1x_eb.common.hw,
		[CLK_SDIO1_2X_EB]	= &sdio1_2x_eb.common.hw,
		[CLK_NANDC_1X_EB]	= &nandc_1x_eb.common.hw,
		[CLK_NANDC_2X_EB]	= &nandc_2x_eb.common.hw,
		[CLK_CSSYS_CA7_EB]	= &cssys_ca7_eb.common.hw,
		[CLK_AP_HS_SPI_EB]	= &ap_hs_spi_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_TMR_EB]		= &tmr_eb.common.hw,
		[CLK_APLL_TEST_EB]	= &apll_test_eb.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_aonapb_gate_desc = {
	.clk_clks	= sc7731e_aonapb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_aonapb_gate_clks),
	.hw_clks	= &sc7731e_aonapb_gate_hws,
};

/* 0x60100000 gpu clocks */
static const struct clk_parent_data gpu_parents[] = {
	{ .hw = &twpll_153m6.hw },
	{ .hw = &twpll_192m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &twpll_307m2.hw },
	{ .hw = &twpll_384m.hw },
	{ .hw = &twpll_512m.hw },
	{ .hw = &gpll.common.hw },
};
static SPRD_COMP_CLK_DATA(gpu, "gpu", gpu_parents,
			  0x4, 0, 3, 4, 2, 0);

static struct sprd_clk_common *sc7731e_gpu_clk[] = {
	&gpu.common,
};

static struct clk_hw_onecell_data sc7731e_gpu_clk_hws = {
	.hws	= {
		[CLK_GPU] = &gpu.common.hw,
	},
	.num	= CLK_GPU_CLK_NUM,
};

static struct sprd_clk_desc sc7731e_gpu_clk_desc = {
	.clk_clks	= sc7731e_gpu_clk,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_gpu_clk),
	.hw_clks	= &sc7731e_gpu_clk_hws,
};

/* 0x60d00000 mm gate clocks */
static SPRD_SC_GATE_CLK_FW_NAME(dcam_eb, "dcam-eb", "ext-26m", 0x0,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(isp_eb, "isp-eb", "ext-26m", 0x0,
				0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(vsp_eb, "vsp-eb", "ext-26m", 0x0,
				0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(csi_eb, "csi-eb", "ext-26m", 0x0,
				0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(jpg_eb, "jpg-eb", "ext-26m", 0x0,
				0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mm_ckg_eb, "mm-ckg-eb", "ext-26m", 0x0,
				0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(vsp_mq_eb, "vsp-mq-eb", "ext-26m", 0x0,
				0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(mcphy_cfg_eb, "mcphy-cfg-eb", "ext-26m",
			     0x8, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK_FW_NAME(msensor0_eb, "msensor0-eb", "ext-26m",
			     0x8, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK_FW_NAME(misp_axi_eb, "misp-axi-eb", "ext-26m",
			     0x8, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK_FW_NAME(mdcam_axi_eb, "mdcam-axi-eb", "ext-26m",
			     0x8, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK_FW_NAME(mmipi_csi_eb, "mmipi-csi-eb", "ext-26m",
			     0x8, BIT(4), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc7731e_mm_gate_clks[] = {
	&dcam_eb.common,
	&isp_eb.common,
	&vsp_eb.common,
	&csi_eb.common,
	&jpg_eb.common,
	&mm_ckg_eb.common,
	&vsp_mq_eb.common,
	&mcphy_cfg_eb.common,
	&msensor0_eb.common,
	&misp_axi_eb.common,
	&mdcam_axi_eb.common,
	&mmipi_csi_eb.common,
};

static struct clk_hw_onecell_data sc7731e_mm_gate_hws = {
	.hws	= {
		[CLK_DCAM_EB]		= &dcam_eb.common.hw,
		[CLK_ISP_EB]		= &isp_eb.common.hw,
		[CLK_VSP_EB]		= &vsp_eb.common.hw,
		[CLK_CSI_EB]		= &csi_eb.common.hw,
		[CLK_JPG_EB]		= &jpg_eb.common.hw,
		[CLK_MM_CKG_EB]		= &mm_ckg_eb.common.hw,
		[CLK_VSP_MQ_EB]		= &vsp_mq_eb.common.hw,
		[CLK_MCPHY_CFG_EB]	= &mcphy_cfg_eb.common.hw,
		[CLK_MSENSOR0_EB]	= &msensor0_eb.common.hw,
		[CLK_MISP_AXI_EB]	= &misp_axi_eb.common.hw,
		[CLK_MDCAM_AXI_EB]	= &mdcam_axi_eb.common.hw,
		[CLK_MMIPI_CSI_EB]	= &mmipi_csi_eb.common.hw,
	},
	.num	= CLK_MM_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_mm_gate_desc = {
	.clk_clks	= sc7731e_mm_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_mm_gate_clks),
	.hw_clks	= &sc7731e_mm_gate_hws,
};

/* 0x60e00000 mm domain clocks */
static const struct clk_parent_data mm_ahb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_96m.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_153m6.hw },
	{ .hw = &cpll_160m.hw },
};
static SPRD_MUX_CLK_DATA(mm_ahb, "mm-ahb", mm_ahb_parents,
			 0x20, 0, 3, SC7731E_MUX_FLAG);

static const struct clk_parent_data sensor_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &twpll_48m.hw },
	{ .hw = &twpll_76m8.hw },
	{ .hw = &twpll_96m.hw },
};
static SPRD_COMP_CLK_DATA(sensor0, "sensor0", sensor_parents,
			  0x24, 0, 2, 8, 3, 0);

static const struct clk_parent_data dcam_if_parents[] = {
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &twpll_307m2.hw },
	{ .hw = &cpll_320m.hw },
};
static SPRD_MUX_CLK_DATA(dcam_if, "dcam-if", dcam_if_parents,
			 0x28, 0, 2, SC7731E_MUX_FLAG);

static const struct clk_parent_data vsp_parents[] = {
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &twpll_307m2.hw },
	{ .hw = &cpll_320m.hw },
};
static SPRD_MUX_CLK_DATA(vsp, "vsp", vsp_parents,
			 0x2c, 0, 2, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK_DATA(isp, "isp", vsp_parents,
			 0x30, 0, 2, SC7731E_MUX_FLAG);

static const struct clk_parent_data jpg_parents[] = {
	{ .hw = &twpll_76m8.hw },
	{ .hw = &twpll_128m.hw },
	{ .hw = &twpll_256m.hw },
	{ .hw = &cpll_266m67.hw },
};
static SPRD_MUX_CLK_DATA(jpg, "jpg", jpg_parents,
			 0x34, 0, 2, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(mipi_csi_en, "mipi-csi-en", "ext-26m",
			     0x38, BIT(16), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data dcam_axi_parents[] = {
	{ .hw = &twpll_307m2.hw },
	{ .hw = &cpll_320m.hw },
	{ .hw = &twpll_512m.hw },
	{ .hw = &cpll_533m.hw },
};
static SPRD_MUX_CLK_DATA(dcam_axi, "dcam-axi", dcam_axi_parents,
			 0x40, 0, 2, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK_DATA(isp_axi, "isp-axi", dcam_axi_parents,
			 0x44, 0, 2, SC7731E_MUX_FLAG);

static struct sprd_clk_common *sc7731e_mm_clk_clks[] = {
	&mm_ahb.common,
	&sensor0.common,
	&dcam_if.common,
	&vsp.common,
	&isp.common,
	&jpg.common,
	&mipi_csi_en.common,
	&dcam_axi.common,
	&isp_axi.common,
};

static struct clk_hw_onecell_data sc7731e_mm_clk_hws = {
	.hws	= {
		[CLK_MM_AHB]	= &mm_ahb.common.hw,
		[CLK_SENSOR0]	= &sensor0.common.hw,
		[CLK_DCAM_IF]	= &dcam_if.common.hw,
		[CLK_VSP]	= &vsp.common.hw,
		[CLK_ISP]	= &isp.common.hw,
		[CLK_JPG]	= &jpg.common.hw,
		[CLK_MIPI_CSI]	= &mipi_csi_en.common.hw,
		[CLK_DCAM_AXI]	= &dcam_axi.common.hw,
		[CLK_ISP_AXI]	= &isp_axi.common.hw,
	},
	.num	= CLK_MM_CLK_NUM,
};

static const struct sprd_clk_desc sc7731e_mm_clk_desc = {
	.clk_clks	= sc7731e_mm_clk_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_mm_clk_clks),
	.hw_clks	= &sc7731e_mm_clk_hws,
};

/* 0x71300000 ap-apb clocks */
static SPRD_SC_GATE_CLK_FW_NAME(sim0_eb, "sim0-eb", "ext-26m", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(iis0_eb, "iis0-eb", "ext-26m", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi0_eb, "spi0-eb", "ext-26m", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c0_eb, "i2c0-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c1_eb, "i2c1-eb", "ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(i2c2_eb, "i2c2-eb", "ext-26m", 0x0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart0_eb, "uart0-eb", "ext-26m", 0x0,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(uart1_eb, "uart1-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sim0_32k_eb, "sim0_32k-eb", "ext-26m", 0x0,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(intc0_eb, "intc0-eb", "ext-26m", 0x0,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(intc1_eb, "intc1-eb", "ext-26m", 0x0,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(intc2_eb, "intc2-eb", "ext-26m", 0x0,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(intc3_eb, "intc3-eb", "ext-26m", 0x0,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc7731e_apapb_gate[] = {
	&sim0_eb.common,
	&iis0_eb.common,
	&spi0_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&sim0_32k_eb.common,
	&intc0_eb.common,
	&intc1_eb.common,
	&intc2_eb.common,
	&intc3_eb.common,
};

static struct clk_hw_onecell_data sc7731e_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_SIM0_32K_EB]	= &sim0_32k_eb.common.hw,
		[CLK_INTC0_EB]		= &intc0_eb.common.hw,
		[CLK_INTC1_EB]		= &intc1_eb.common.hw,
		[CLK_INTC2_EB]		= &intc2_eb.common.hw,
		[CLK_INTC3_EB]		= &intc3_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static struct sprd_reset_map sc7731e_ap_apb_resets[] = {
	[RESET_AP_APB_SIM0_SOFT_RST]		= { 0x0004, BIT(0), 0x1000 },
	[RESET_AP_APB_IIS0_SOFT_RST]		= { 0x0004, BIT(1), 0x1000 },
	[RESET_AP_APB_SPI0_SOFT_RST]		= { 0x0004, BIT(5), 0x1000 },
	[RESET_AP_APB_SPI1_SOFT_RST]		= { 0x0004, BIT(6), 0x1000 },
	[RESET_AP_APB_I2C0_SOFT_RST]		= { 0x0004, BIT(8), 0x1000 },
	[RESET_AP_APB_I2C1_SOFT_RST]		= { 0x0004, BIT(9), 0x1000 },
	[RESET_AP_APB_I2C2_SOFT_RST]		= { 0x0004, BIT(10), 0x1000 },
	[RESET_AP_APB_UART0_SOFT_RST]		= { 0x0004, BIT(13), 0x1000 },
	[RESET_AP_APB_UART1_SOFT_RST]		= { 0x0004, BIT(14), 0x1000 },
	[RESET_AP_APB_INTC0_SOFT_RST]		= { 0x0004, BIT(19), 0x1000 },
	[RESET_AP_APB_INTC1_SOFT_RST]		= { 0x0004, BIT(20), 0x1000 },
	[RESET_AP_APB_INTC2_SOFT_RST]		= { 0x0004, BIT(21), 0x1000 },
	[RESET_AP_APB_INTC3_SOFT_RST]		= { 0x0004, BIT(22), 0x1000 },
};

static const struct sprd_clk_desc sc7731e_apapb_gate_desc = {
	.clk_clks	= sc7731e_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_apapb_gate),
	.hw_clks	= &sc7731e_apapb_gate_hws,
	.resets		= sc7731e_ap_apb_resets,
	.num_resets	= ARRAY_SIZE(sc7731e_ap_apb_resets),
};

static const struct of_device_id sprd_sc7731e_clk_ids[] = {
	{ .compatible = "sprd,sc7731e-pmu-gate",	/* 0x402b0000 */
	  .data = &sc7731e_pmu_gate_desc },
	{ .compatible = "sprd,sc7731e-pll",		/* 0x402e0000 */
	  .data = &sc7731e_pll_desc },
	{ .compatible = "sprd,sc7731e-apahb-gate",	/* 0x20e00000 */
	  .data = &sc7731e_apahb_gate_desc },
	{ .compatible = "sprd,sc7731e-ap-clk",		/* 0x21500000 */
	  .data = &sc7731e_ap_clk_desc },
	{ .compatible = "sprd,sc7731e-aon-clk",		/* 0x402d0000 */
	  .data = &sc7731e_aon_clk_desc },
	{ .compatible = "sprd,sc7731e-aonapb-gate",	/* 0x402e0000 */
	  .data = &sc7731e_aonapb_gate_desc },
	{ .compatible = "sprd,sc7731e-gpu-clk",		/* 0x60100000 */
	  .data = &sc7731e_gpu_clk_desc },
	{ .compatible = "sprd,sc7731e-mm-gate",		/* 0x60d00000 */
	  .data = &sc7731e_mm_gate_desc },
	{ .compatible = "sprd,sc7731e-mm-clk",		/* 0x60e00000 */
	  .data = &sc7731e_mm_clk_desc },
	{ .compatible = "sprd,sc7731e-apapb-gate",	/* 0x71300000 */
	  .data = &sc7731e_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sc7731e_clk_ids);

static int sc7731e_clk_probe(struct platform_device *pdev)
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

static struct platform_driver sc7731e_clk_driver = {
	.probe	= sc7731e_clk_probe,
	.driver	= {
		.name	= "sc7731e-clk",
		.of_match_table	= sprd_sc7731e_clk_ids,
	},
};
module_platform_driver(sc7731e_clk_driver);

MODULE_DESCRIPTION("Unisoc SC7731E Clock Driver");
MODULE_LICENSE("GPL v2");
