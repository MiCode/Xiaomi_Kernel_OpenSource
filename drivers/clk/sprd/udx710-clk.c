// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc udx710 clock driver
 *
 * Copyright (C) 2018 Unisoc, Inc.
 * Author: Xiaolong Zhang <xiaolong.zhang@unisoc.com>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,udx710-clk.h>
#include <dt-bindings/reset/sprd,udx710-reset.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"
#include "reset.h"

#define UDX710_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* pmu gates clock */
static SPRD_PLL_SC_GATE_CLK_FW_NAME(mpll0_gate, "mpll0-gate", "ext-26m", 0x190,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(mpll1_gate, "mpll1-gate", "ext-26m", 0x194,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *udx710_pmu_gate_clks[] = {
	/* address base is 0x64010000 */
	&mpll0_gate.common,
	&mpll1_gate.common,
};

static struct clk_hw_onecell_data udx710_pmu_gate_hws = {
	.hws	= {
		[CLK_MPLL0_GATE]  = &mpll0_gate.common.hw,
		[CLK_MPLL1_GATE]  = &mpll1_gate.common.hw,
	},
	.num	= CLK_PMU_GATE_NUM,
};

static struct sprd_reset_map udx710_pmu_apb_resets[] = {
	[RESET_PMU_APB_V3_MODEM_SOFT_RST]		= { 0x00B0, BIT(0), 0x1000 },
	[RESET_PMU_APB_PSCP_SOFT_RST]			= { 0x00B0, BIT(1), 0x1000 },
	[RESET_PMU_APB_V3_MODEM_PS_SOFT_RST]		= { 0x00B0, BIT(2), 0x1000 },
	[RESET_PMU_APB_V3_MODEM_PHY_SOFT_RST]		= { 0x00B0, BIT(3), 0x1000 },
	[RESET_PMU_APB_GPU_SOFT_RST]			= { 0x00B0, BIT(4), 0x1000 },
	[RESET_PMU_APB_AP_SOFT_RST]			= { 0x00B0, BIT(5), 0x1000 },
	[RESET_PMU_APB_PUB_SOFT_RST]			= { 0x00B0, BIT(6), 0x1000 },
	[RESET_PMU_APB_APCPU_SOFT_RST]			= { 0x00B0, BIT(7), 0x1000 },
	[RESET_PMU_APB_SP_SYS_SOFT_RST]			= { 0x00B0, BIT(8), 0x1000 },
	[RESET_PMU_APB_AUDCP_SYS_SOFT_RST]		= { 0x00B0, BIT(9), 0x1000 },
	[RESET_PMU_APB_AUDCP_AUDDSP_SOFT_RST]		= { 0x00B0, BIT(10), 0x1000 },
	[RESET_PMU_APB_AP_VDSP_SOFT_RST]		= { 0x00B0, BIT(11), 0x1000 },
	[RESET_PMU_APB_V3_MODEM_AON_SOFT_RST]		= { 0x00B0, BIT(12), 0x1000 },
	[RESET_PMU_APB_NRCP_AON_SOFT_RST]		= { 0x00B0, BIT(13), 0x1000 },
	[RESET_PMU_APB_NRCP_DSP_0_SOFT_RST]		= { 0x00B0, BIT(14), 0x1000 },
	[RESET_PMU_APB_NRCP_DSP_1_SOFT_RST]		= { 0x00B0, BIT(15), 0x1000 },
	[RESET_PMU_APB_AP_VSP_SOFT_RST]			= { 0x00B0, BIT(16), 0x1000 },
	[RESET_PMU_APB_NRCP_SYS_SOFT_RST]		= { 0x00B0, BIT(17), 0x1000 },
};

static struct sprd_clk_desc udx710_pmu_gate_desc = {
	.clk_clks	= udx710_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(udx710_pmu_gate_clks),
	.hw_clks	= &udx710_pmu_gate_hws,
	.resets		= udx710_pmu_apb_resets,
	.num_resets	= ARRAY_SIZE(udx710_pmu_apb_resets),
};

/* pll clock at g3 */
static CLK_FIXED_FACTOR_FW_NAME(v3rpll, "v3rpll", "ext-26m", 1, 15, 0);
static CLK_FIXED_FACTOR_FW_NAME(v3rpll_195m, "v3rpll-195m", "v3rpll", 2, 1, 0);

static struct freq_table v3pll_ftable[6] = {
	{ .ibias = 2, .max_freq = 900000000ULL },
	{ .ibias = 3, .max_freq = 1100000000ULL },
	{ .ibias = 4, .max_freq = 1300000000ULL },
	{ .ibias = 5, .max_freq = 1500000000ULL },
	{ .ibias = 6, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_v3pll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_FW_NAME(v3pll, "v3pll", "ext-26m", 0x14,
			       2, v3pll_ftable, f_v3pll, 240,
			       1000, 1000, 0, 0);
static CLK_FIXED_FACTOR_HW(v3pll_768m, "v3pll-768m", &v3pll.common.hw, 2, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_384m, "v3pll-384m", &v3pll.common.hw, 4, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_192m, "v3pll-192m", &v3pll.common.hw, 8, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_96m, "v3pll-96m", &v3pll.common.hw, 16, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_48m, "v3pll-48m", &v3pll.common.hw, 32, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_24m, "v3pll-24m", &v3pll.common.hw, 64, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_12m, "v3pll-12m", &v3pll.common.hw, 128, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_512m, "v3pll-512m", &v3pll.common.hw, 3, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_256m, "v3pll-256m", &v3pll.common.hw, 6, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_128m, "v3pll-128m", &v3pll.common.hw, 12, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_64m, "v3pll-64m", &v3pll.common.hw, 24, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_307m2, "v3pll-307m2", &v3pll.common.hw, 5, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_219m4, "v3pll-219m4", &v3pll.common.hw, 7, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_170m6, "v3pll-170m6", &v3pll.common.hw, 9, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_153m6, "v3pll-153m6", &v3pll.common.hw, 10, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_76m8, "v3pll-76m8", &v3pll.common.hw, 20, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_51m2, "v3pll-51m2", &v3pll.common.hw, 30, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_38m4, "v3pll-38m4", &v3pll.common.hw, 40, 1, 0);
static CLK_FIXED_FACTOR_HW(v3pll_19m2, "v3pll-19m2", &v3pll.common.hw, 80, 1, 0);

static struct freq_table mpll_ftable[7] = {
	{ .ibias = 1, .max_freq = 1400000000ULL },
	{ .ibias = 2, .max_freq = 1600000000ULL },
	{ .ibias = 3, .max_freq = 1800000000ULL },
	{ .ibias = 4, .max_freq = 2000000000ULL },
	{ .ibias = 5, .max_freq = 2200000000ULL },
	{ .ibias = 6, .max_freq = 2500000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_mpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 1,	.width = 0 },	/* mod_en	*/
	{ .shift = 2,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 46,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_HW(mpll0, "mpll0", &mpll0_gate.common.hw, 0x9c,
				   2, mpll_ftable, f_mpll, 240,
				   1000, 1000, 1, 1200000000ULL);
static SPRD_PLL_HW(mpll1, "mpll1", &mpll1_gate.common.hw, 0xb0,
				   2, mpll_ftable, f_mpll, 240,
				   1000, 1000, 1, 1200000000ULL);

static struct sprd_clk_common *udx710_g3_pll_clks[] = {
	/* address base is 0x634b0000 */
	&v3pll.common,
	&mpll0.common,
	&mpll1.common,
};

static struct clk_hw_onecell_data udx710_g3_pll_hws = {
	.hws	= {
		[CLK_V3RPLL] = &v3rpll.hw,
		[CLK_V3RPLL_195M] = &v3rpll_195m.hw,
		[CLK_V3PLL] = &v3pll.common.hw,
		[CLK_TWPLL_768M] = &v3pll_768m.hw,
		[CLK_TWPLL_384M] = &v3pll_384m.hw,
		[CLK_TWPLL_192M] = &v3pll_192m.hw,
		[CLK_TWPLL_96M] = &v3pll_96m.hw,
		[CLK_TWPLL_48M] = &v3pll_48m.hw,
		[CLK_TWPLL_24M] = &v3pll_24m.hw,
		[CLK_TWPLL_12M] = &v3pll_12m.hw,
		[CLK_TWPLL_512M] = &v3pll_512m.hw,
		[CLK_TWPLL_256M] = &v3pll_256m.hw,
		[CLK_TWPLL_128M] = &v3pll_128m.hw,
		[CLK_TWPLL_64M] = &v3pll_64m.hw,
		[CLK_TWPLL_307M2] = &v3pll_307m2.hw,
		[CLK_TWPLL_219M4] = &v3pll_219m4.hw,
		[CLK_TWPLL_170M6] = &v3pll_170m6.hw,
		[CLK_TWPLL_153M6] = &v3pll_153m6.hw,
		[CLK_TWPLL_76M8] = &v3pll_76m8.hw,
		[CLK_TWPLL_51M2] = &v3pll_51m2.hw,
		[CLK_TWPLL_38M4] = &v3pll_38m4.hw,
		[CLK_TWPLL_19M2] = &v3pll_19m2.hw,
		[CLK_MPLL0] = &mpll0.common.hw,
		[CLK_MPLL1] = &mpll1.common.hw,
	},
	.num	= CLK_ANLG_PHY_G3_NUM,
};

static struct sprd_clk_desc udx710_g3_pll_desc = {
	.clk_clks	= udx710_g3_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(udx710_g3_pll_clks),
	.hw_clks	= &udx710_g3_pll_hws,
};

/* aon clocks */
static CLK_FIXED_FACTOR_FW_NAME(clk_13m, "clk-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_6m5, "clk-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_4m,	"clk-4m", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_2m,	"clk-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_1m,	"clk-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_250k, "clk-250k", "ext-26m", 104, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_25m, "rco-25m", "rco-100m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_20m, "rco-20m", "rco-100m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_4m,	"rco-4m", "rco-100m", 25, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(rco_2m,	"rco-2m", "rco-100m", 50, 1, 0);

static const struct clk_parent_data aon_apb_parents[] = {
	{ .hw = &rco_4m.hw },
	{ .hw = &clk_4m.hw },
	{ .hw = &clk_13m.hw },
	{ .hw = &rco_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_128m.hw },
};
static SPRD_COMP_CLK_DATA(aon_apb_clk, "aon-apb-clk", aon_apb_parents, 0x220,
		     0, 3, 8, 2, 0);

static const struct clk_parent_data adi_parents[] = {
	{ .hw = &rco_4m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &rco_25m.hw },
	{ .hw = &v3pll_38m4.hw },
	{ .hw = &v3pll_51m2.hw },
};
static SPRD_MUX_CLK_DATA(adi_clk, "adi-clk", adi_parents, 0x228,
			0, 3, UDX710_MUX_FLAG);

static const struct clk_parent_data aon_uart_parents[] = {
	{ .hw = &rco_4m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_48m.hw },
	{ .hw = &v3pll_51m2.hw },
	{ .hw = &v3pll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_128m.hw },
};
static SPRD_MUX_CLK_DATA(aon_uart0_clk, "aon-uart0-clk", aon_uart_parents, 0x22c,
		    0, 3, UDX710_MUX_FLAG);

static const struct clk_parent_data aon_i2c_parents[] = {
	{ .hw = &rco_4m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_48m.hw },
	{ .hw = &v3pll_51m2.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(aon_i2c_clk, "aon-i2c-clk", aon_i2c_parents, 0x230,
		    0, 3, UDX710_MUX_FLAG);

static const struct clk_parent_data efuse_parents[] = {
	{ .hw = &rco_25m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(efuse_clk, "efuse-clk", efuse_parents, 0x234,
		    0, 1, UDX710_MUX_FLAG);

static const struct clk_parent_data tmr_parents[] = {
	{ .hw = &rco_4m.hw },
	{ .hw = &rco_25m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(tmr_clk, "tmr-clk", tmr_parents, 0x238,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data thm_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_250k.hw },
};
static SPRD_MUX_CLK_DATA(thm0_clk, "thm0-clk", thm_parents, 0x23c,
		    0, 1, UDX710_MUX_FLAG);
static SPRD_MUX_CLK_DATA(thm1_clk, "thm1-clk", thm_parents, 0x240,
		    0, 1, UDX710_MUX_FLAG);
static SPRD_MUX_CLK_DATA(thm2_clk, "thm2-clk", thm_parents, 0x244,
		    0, 1, UDX710_MUX_FLAG);

static const struct clk_parent_data pmu_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &rco_4m.hw },
	{ .hw = &clk_4m.hw },
};
static SPRD_MUX_CLK_DATA(pmu_clk, "pmu-clk", pmu_parents, 0x250,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data apcpu_pmu_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_128m.hw },
};
static SPRD_MUX_CLK_DATA(apcpu_pmu_clk, "apcpu-pmu-clk", pmu_parents, 0x254,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data  aux_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .fw_name = "ext-26m" },
};
static SPRD_COMP_CLK_DATA(aux0_clk, "aux0-clk", aux_parents, 0x260,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK_DATA(aux1_clk, "aux1-clk", aux_parents, 0x264,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK_DATA(aux2_clk, "aux2-clk", aux_parents, 0x268,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK_DATA(probe_clk, "probe-clk", aux_parents, 0x26c,
		     0, 1, 8, 4, 0);

static const struct clk_parent_data  apcpu_dap_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &rco_4m.hw },
	{ .hw = &v3pll_76m8.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_128m.hw },
	{ .hw = &v3pll_153m6.hw },
};
static SPRD_MUX_CLK_DATA(apcpu_dap_clk, "apcpu-dap-clk", thm_parents, 0x27c,
		    0, 3, UDX710_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(apcpu_dap_mtck, "apcpu-dap-mtck", "ext-26m", 0x280,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data debug_ts_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_76m8.hw },
	{ .hw = &v3pll_128m.hw },
	{ .hw = &v3pll_192m.hw },
};
static SPRD_MUX_CLK_DATA(debug_ts_clk, "debug-ts-clk", debug_ts_parents, 0x288,
		    0, 2, UDX710_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(dsi0_test_clk, "dsi0-test-clk", "ext-26m", 0x28c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(dsi1_test_clk, "dsi1-test-clk", "ext-26m", 0x290,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(dsi2_test_clk, "dsi2-test-clk", "ext-26m", 0x294,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data djtag_tck_parents[] = {
	{ .hw = &rco_4m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(djtag_tck_clk, "djtag-tck-clk", djtag_tck_parents, 0x2a4,
		    0, 1, UDX710_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(djtag_tck_hw, "djtag-tck-hw", "ext-26m", 0x2a8,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data debounce_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &rco_4m.hw },
	{ .hw = &rco_25m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(debounce_clk, "debounce-clk", debounce_parents, 0x2b0,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data scc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_48m.hw },
	{ .hw = &v3pll_51m2.hw },
	{ .hw = &v3pll_96m.hw },
};
static SPRD_MUX_CLK_DATA(scc_clk, "scc-clk", scc_parents, 0x2b8,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data top_dvfs_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_96m.hw },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_128m.hw },
};
static SPRD_MUX_CLK_DATA(top_dvfs_clk, "top-dvfs-clk", top_dvfs_parents, 0x2bc,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data sdio_2x_parents[] = {
	{ .hw = &clk_1m.hw },
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_307m2.hw },
	{ .hw = &v3rpll.hw },
};
static SPRD_COMP_CLK_DATA(sdio2_2x_clk, "sdio2-2x-clk", sdio_2x_parents, 0x2c4,
		     0, 2, 16, 11, 0);
static SPRD_DIV_CLK_HW(sdio2_1x_clk, "sdio2-1x-clk", &sdio2_2x_clk.common.hw, 0x2c8,
		    8, 1, 0);

static const struct clk_parent_data cssys_parents[] = {
	{ .hw = &rco_25m.hw },
	{ .fw_name = "ext-26m" },
	{ .fw_name = "rco-100m" },
	{ .hw = &v3pll_256m.hw },
	{ .hw = &v3pll_307m2.hw },
	{ .hw = &v3pll_384m.hw },
};
static SPRD_COMP_CLK_DATA(cssys_clk, "cssys-clk", cssys_parents, 0x2cc,
		     0, 3, 8, 2, 0);
static SPRD_DIV_CLK_HW(cssys_apb_clk, "cssys-apb-clk", &cssys_clk.common.hw, 0x2d0,
		    8, 2, 0);

static const struct clk_parent_data apcpu_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_76m8.hw },
	{ .hw = &v3pll_128m.hw },
	{ .hw = &v3pll_256m.hw },
};
static SPRD_MUX_CLK_DATA(apcpu_axi_clk, "apcpu-axi-clk", apcpu_axi_parents, 0x2d4,
		    0, 2, UDX710_MUX_FLAG);

static SPRD_COMP_CLK_DATA(sdio1_2x_clk, "sdio1-2x-clk", sdio_2x_parents, 0x2d8,
		     0, 2, 16, 11, 0);
static SPRD_DIV_CLK_HW(sdio1_1x_clk, "sdio1-1x-clk", &sdio1_2x_clk.common.hw, 0x2dc,
		    8, 1, 0);

static SPRD_GATE_CLK_FW_NAME(sdio0_slv_clk, "sdio0-slv-clk", "ext-26m", 0x2e0,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_COMP_CLK_DATA(emmc_2x_clk, "emmc-2x-clk", sdio_2x_parents, 0x2e4,
		     0, 2, 16, 11, 0);
static SPRD_DIV_CLK_HW(emmc_1x_clk, "emmc-1x-clk", &emmc_2x_clk.common.hw, 0x2e8,
		    8, 1, 0);

static const struct clk_parent_data nandc_2x_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_153m6.hw },
	{ .hw = &v3pll_170m6.hw },
	{ .hw = &v3rpll_195m.hw },
	{ .hw = &v3pll_256m.hw },
	{ .hw = &v3pll_307m2.hw },
	{ .hw = &v3rpll.hw },
};
static SPRD_COMP_CLK_DATA(nandc_2x_clk, "nandc-2x-clk", nandc_2x_parents, 0x2ec,
		     0, 2, 8, 11, 0);
static SPRD_DIV_CLK_HW(nandc_1x_clk, "nandc-1x-clk", &nandc_2x_clk.common.hw, 0x2f0,
		    8, 1, 0);

static const struct clk_parent_data ap_spi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_128m.hw },
	{ .hw = &v3pll_153m6.hw },
	{ .hw = &v3pll_192m.hw },
};
static SPRD_COMP_CLK_DATA(ap_spi0_clk, "ap-spi0-clk", ap_spi_parents, 0x2f4,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_spi1_clk, "ap-spi1-clk", ap_spi_parents, 0x2f8,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_spi2_clk, "ap-spi2-clk", ap_spi_parents, 0x2fc,
		     0, 2, 8, 3, 0);

static const struct clk_parent_data otg2_ref_parents[] = {
	{ .fw_name = "clk-12m" },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(otg2a_ref_clk, "otg2a-ref-clk", otg2_ref_parents, 0x300,
		    0, 1, UDX710_MUX_FLAG);

static const struct clk_parent_data usb3_ref_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &clk_1m.hw },
};
static SPRD_MUX_CLK_DATA(u3a_suspend_ref, "u3a-suspend-ref", usb3_ref_parents, 0x308,
		    0, 1, UDX710_MUX_FLAG);

static SPRD_MUX_CLK_DATA(otg2b_ref_clk, "otg2b-ref-clk", otg2_ref_parents, 0x30c,
		    0, 1, UDX710_MUX_FLAG);

static SPRD_MUX_CLK_DATA(u3b_suspend_ref, "u3b-suspend-ref", usb3_ref_parents, 0x308,
		    0, 1, UDX710_MUX_FLAG);

static const struct clk_parent_data analog_io_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_48m.hw },
};
static SPRD_COMP_CLK_DATA(analog_io_clk, "analog-io-clk", analog_io_parents, 0x328,
		     0, 1, 8, 2, 0);

static const struct clk_parent_data dmc_ref_parents[] = {
	{ .hw = &clk_6m5.hw },
	{ .hw = &clk_13m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(dmc_ref_clk, "dmc-ref-clk", dmc_ref_parents, 0x32c,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data emc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_384m.hw },
	{ .hw = &v3pll_512m.hw },
	{ .hw = &v3pll_768m.hw },
	{ .hw = &v3pll.common.hw },
};
static SPRD_MUX_CLK_DATA(emc_clk, "emc-clk", emc_parents, 0x330,
		    0, 3, UDX710_MUX_FLAG);

static const struct clk_parent_data sc_cc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &rco_25m.hw },
	{ .hw = &v3pll_96m.hw },
};
static SPRD_MUX_CLK_DATA(sc_cc_clk, "sc-cc-clk", sc_cc_parents, 0x398,
		    0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data pmu_26m_parents[] = {
	{ .hw = &rco_20m.hw },
	{ .fw_name = "ext-26m" },
};
static SPRD_MUX_CLK_DATA(pmu_26m_clk, "pmu-26m-clk", pmu_26m_parents, 0x39c,
		    0, 1, UDX710_MUX_FLAG);

static struct sprd_clk_common *udx710_aon_clks[] = {
	/* address base is 0x63170000 */
	&aon_apb_clk.common,
	&adi_clk.common,
	&aon_uart0_clk.common,
	&aon_i2c_clk.common,
	&efuse_clk.common,
	&tmr_clk.common,
	&thm0_clk.common,
	&thm1_clk.common,
	&thm2_clk.common,
	&pmu_clk.common,
	&apcpu_pmu_clk.common,
	&aux0_clk.common,
	&aux1_clk.common,
	&aux2_clk.common,
	&probe_clk.common,
	&apcpu_dap_clk.common,
	&apcpu_dap_mtck.common,
	&debug_ts_clk.common,
	&dsi0_test_clk.common,
	&dsi1_test_clk.common,
	&dsi2_test_clk.common,
	&djtag_tck_clk.common,
	&djtag_tck_hw.common,
	&debounce_clk.common,
	&scc_clk.common,
	&top_dvfs_clk.common,
	&sdio2_2x_clk.common,
	&sdio2_1x_clk.common,
	&cssys_clk.common,
	&cssys_apb_clk.common,
	&apcpu_axi_clk.common,
	&sdio1_2x_clk.common,
	&sdio1_1x_clk.common,
	&sdio0_slv_clk.common,
	&emmc_2x_clk.common,
	&emmc_1x_clk.common,
	&nandc_2x_clk.common,
	&nandc_1x_clk.common,
	&ap_spi0_clk.common,
	&ap_spi1_clk.common,
	&ap_spi2_clk.common,
	&otg2a_ref_clk.common,
	&u3a_suspend_ref.common,
	&otg2b_ref_clk.common,
	&u3b_suspend_ref.common,
	&analog_io_clk.common,
	&dmc_ref_clk.common,
	&emc_clk.common,
	&sc_cc_clk.common,
	&pmu_26m_clk.common,
};

static struct clk_hw_onecell_data udx710_aon_clk_hws = {
	.hws	= {
		[CLK_13M]  = &clk_13m.hw,
		[CLK_6M5]  = &clk_6m5.hw,
		[CLK_4M]   = &clk_4m.hw,
		[CLK_2M]   = &clk_2m.hw,
		[CLK_1M]   = &clk_1m.hw,
		[CLK_250K] = &clk_250k.hw,
		[CLK_RCO25M] = &rco_25m.hw,
		[CLK_RCO20M] = &rco_20m.hw,
		[CLK_RCO4M] = &rco_4m.hw,
		[CLK_RCO2M] = &rco_2m.hw,
		[CLK_AON_APB] = &aon_apb_clk.common.hw,
		[CLK_ADI] = &adi_clk.common.hw,
		[CLK_AON_UART0] = &aon_uart0_clk.common.hw,
		[CLK_AON_I2C] = &aon_i2c_clk.common.hw,
		[CLK_EFUSE] = &efuse_clk.common.hw,
		[CLK_TMR] = &tmr_clk.common.hw,
		[CLK_THM0] = &thm0_clk.common.hw,
		[CLK_THM1] = &thm1_clk.common.hw,
		[CLK_THM2] = &thm2_clk.common.hw,
		[CLK_PMU] = &pmu_clk.common.hw,
		[CLK_APCPU_PMU] = &apcpu_pmu_clk.common.hw,
		[CLK_AUX0] = &aux0_clk.common.hw,
		[CLK_AUX1] = &aux1_clk.common.hw,
		[CLK_AUX2] = &aux2_clk.common.hw,
		[CLK_PROBE] = &probe_clk.common.hw,
		[CLK_APCPU_DAP] = &apcpu_dap_clk.common.hw,
		[CLK_APCPU_DAP_MTCK] = &apcpu_dap_mtck.common.hw,
		[CLK_DEBUG_TS] = &debug_ts_clk.common.hw,
		[CLK_DSI0_TEST] = &dsi0_test_clk.common.hw,
		[CLK_DSI1_TEST] = &dsi1_test_clk.common.hw,
		[CLK_DSI2_TEST] = &dsi2_test_clk.common.hw,
		[CLK_DJTAG_TCK] = &djtag_tck_clk.common.hw,
		[CLK_DJTAG_TCK_HW] = &djtag_tck_hw.common.hw,
		[CLK_DEBOUNCE] = &debounce_clk.common.hw,
		[CLK_SCC] = &scc_clk.common.hw,
		[CLK_TOP_DVFS] = &top_dvfs_clk.common.hw,
		[CLK_SDIO2_2X] = &sdio2_2x_clk.common.hw,
		[CLK_SDIO2_1X] = &sdio2_1x_clk.common.hw,
		[CLK_CSSYS] = &cssys_clk.common.hw,
		[CLK_CSSYS_APB] = &cssys_apb_clk.common.hw,
		[CLK_APCPU_AXI] = &apcpu_axi_clk.common.hw,
		[CLK_SDIO1_2X] = &sdio1_2x_clk.common.hw,
		[CLK_SDIO1_1X] = &sdio1_1x_clk.common.hw,
		[CLK_SDIO0_SLV] = &sdio0_slv_clk.common.hw,
		[CLK_EMMC_2X] = &emmc_2x_clk.common.hw,
		[CLK_EMMC_1X] = &emmc_1x_clk.common.hw,
		[CLK_NANDC_2X] = &nandc_2x_clk.common.hw,
		[CLK_NANDC_1X] = &nandc_1x_clk.common.hw,
		[CLK_AP_SPI0] = &ap_spi0_clk.common.hw,
		[CLK_AP_SPI1] = &ap_spi1_clk.common.hw,
		[CLK_AP_SPI2] = &ap_spi2_clk.common.hw,
		[CLK_OTG2A_REF] = &otg2a_ref_clk.common.hw,
		[CLK_U3A_SUSPEND_REF] = &u3a_suspend_ref.common.hw,
		[CLK_OTG2B_REF] = &otg2b_ref_clk.common.hw,
		[CLK_U3B_SUSPEND_REF] = &u3b_suspend_ref.common.hw,
		[CLK_ANALOG_IO] = &analog_io_clk.common.hw,
		[CLK_DMC_REF] = &dmc_ref_clk.common.hw,
		[CLK_EMC] = &emc_clk.common.hw,
		[CLK_SC_CC] = &sc_cc_clk.common.hw,
		[CLK_PMU_26M] = &pmu_26m_clk.common.hw,
	},
	.num	= CLK_AON_CLK_NUM,
};

static const struct sprd_clk_desc udx710_aon_clk_desc = {
	.clk_clks	= udx710_aon_clks,
	.num_clk_clks	= ARRAY_SIZE(udx710_aon_clks),
	.hw_clks	= &udx710_aon_clk_hws,
};

/* aon gates */
static SPRD_SC_GATE_CLK_FW_NAME(rc100_cal_eb, "rc100-cal-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_spi_eb, "aon-spi-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(djtag_tck_eb, "djtag-tck-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(djtag_eb, "djtag-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux0_eb, "aux0-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux1_eb, "aux1-eb", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aux2_eb, "aux2-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(probe_eb, "probe-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(bsm_tmr_eb, "bsm-tmr-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_apb_bm_eb, "aon-apb-bm-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pmu_apb_bm_eb, "pmu-apb-bm-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_cssys_eb, "apcpu-cssys-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(debug_filter_eb, "debug-filter-eb", "ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_dap_eb, "apcpu-dap-eb", "ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cssys_eb, "cssys-eb", "ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cssys_apb_eb, "cssys-apb-eb", "ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(cssys_pub_eb, "cssys-pub-eb", "ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd0_cfg_eb, "sd0-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd0_ref_eb, "sd0-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd1_cfg_eb, "sd1-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd1_ref_eb, "sd1-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd2_cfg_eb, "sd2-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd2_ref_eb, "sd2-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(serdes0_eb, "serdes0-eb", "ext-26m", 0x0,
		     0x1000, BIT(25), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(serdes1_eb, "serdes1-eb", "ext-26m", 0x0,
		     0x1000, BIT(26), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(serdes2_eb, "serdes2-eb", "ext-26m", 0x0,
		     0x1000, BIT(27), 0, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rtm_eb, "rtm-eb", "ext-26m", 0x0,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rtm_atb_eb, "rtm-atb-eb", "ext-26m", 0x0,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_nr_spi_eb, "aon-nr-spi-eb", "ext-26m", 0x0,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_bm_s5_eb, "aon-bm-s5-eb", "ext-26m", 0x0,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(efuse_eb, "efuse-eb", "ext-26m", 0x4,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(gpio_eb, "gpio-eb", "ext-26m", 0x4,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(mbox_eb, "mbox-eb", "ext-26m", 0x4,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(kpd_eb, "kpd-eb", "ext-26m", 0x4,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_syst_eb, "aon-syst-eb", "ext-26m", 0x4,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_syst_eb, "ap-syst-eb", "ext-26m", 0x4,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_tmr_eb, "aon-tmr-eb", "ext-26m", 0x4,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dvfs_top_eb, "dvfs-top-eb", "ext-26m", 0x4,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_clk_eb, "apcpu-clk-rf-eb", "ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(splk_eb, "splk-eb", "ext-26m", 0x4,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pin_eb, "pin-eb", "ext-26m", 0x4,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ana_eb, "ana-eb", "ext-26m", 0x4,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_ckg_eb, "aon-ckg-eb", "ext-26m", 0x4,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(djtag_ctrl_eb, "djtag-ctrl-eb", "ext-26m", 0x4,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_ts0_eb, "apcpu-ts0-eb", "ext-26m", 0x4,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nic400_aon_eb, "nic400-aon-eb", "ext-26m", 0x4,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(scc_eb, "scc-eb", "ext-26m", 0x4,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_spi0_eb, "ap-spi0-eb", "ext-26m", 0x4,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_spi1_eb, "ap-spi1-eb", "ext-26m", 0x4,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_spi2_eb, "ap-spi2-eb", "ext-26m", 0x4,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_bm_s3_eb, "aon-bm-s3-eb", "ext-26m", 0x4,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sc_cc_eb, "sc-cc-eb", "ext-26m", 0x4,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(thm0_eb, "thm0-eb", "ext-26m", 0x8,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(thm1_eb, "thm1-eb", "ext-26m", 0x8,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_sim_eb, "ap-sim-eb", "ext-26m", 0x8,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_i2c_eb, "aon-i2c-eb", "ext-26m", 0x8,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pmu_eb, "pmu-eb", "ext-26m", 0x8,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(adi_eb, "adi-eb", "ext-26m", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_eb, "eic-eb", "ext-26m", 0x8,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc0_eb, "ap-intc0-eb", "ext-26m", 0x8,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc1_eb, "ap-intc1-eb", "ext-26m", 0x8,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc2_eb, "ap-intc2-eb", "ext-26m", 0x8,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc3_eb, "ap-intc3-eb", "ext-26m", 0x8,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc4_eb, "ap-intc4-eb", "ext-26m", 0x8,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_intc5_eb, "ap-intc5-eb", "ext-26m", 0x8,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(audcp_intc_eb, "audcp-intc-eb", "ext-26m", 0x8,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr0_eb, "ap-tmr0-eb", "ext-26m", 0x8,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr1_eb, "ap-tmr1-eb", "ext-26m", 0x8,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr2_eb, "ap-tmr2-eb", "ext-26m", 0x8,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_wdg_eb, "ap-wdg-eb", "ext-26m", 0x8,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(apcpu_wdg_eb, "apcpu-wdg-eb", "ext-26m", 0x8,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(thm2_eb, "thm2-eb", "ext-26m", 0x8,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(arch_rtc_eb, "arch-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(kpd_rtc_eb, "kpd-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_syst_rtc_eb, "ap-syst-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_rtc_eb, "eic-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(eic_rtcdv5_eb, "eic-rtcdv5-eb", "ext-26m", 0xc,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ac_wdg_rtc_eb, "ac-wdg-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dcxo_lc_rtc_eb, "dcxo-lc-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(bb_cal_rtc_eb, "bb-cal-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(dsi0_test_eb, "dsi0-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dsi1_test_eb, "dsi1-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dsi2_test_eb, "dsi2-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dmc_ref_en, "dmc-ref-eb", "ext-26m", 0x20,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(tsen_en, "tsen-en", "ext-26m", 0x20,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(tmr_en, "tmr-en", "ext-26m", 0x20,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rc100_ref_en, "rc100-ref-en", "ext-26m", 0x20,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(rc100_fdk_en, "rc100-fdk-en", "ext-26m", 0x20,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(debounce_en, "debounce-en", "ext-26m", 0x20,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(det_32k_eb, "det-32k-eb", "ext-26m", 0x20,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(cssys_en, "cssys-en", "ext-26m", 0x24,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_1x_en, "nandc-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_2x_en, "nandc-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(all_pll_test_eb, "all-pll-test-eb", "ext-26m", 0x24,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(aapc_test_eb, "aapc-test-eb", "ext-26m", 0x24,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(debug_ts_eb, "debug-ts-eb", "ext-26m", 0x24,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK_FW_NAME(u2_0_ref_en, "u2-0-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(u2_1_ref_en, "u2-1-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(u3_0_ref_en, "u3-0-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(u3_0_suspend_en, "u3-0-suspend-en", "ext-26m", 0x564,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(u3_1_ref_en, "u3-1-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(u3_1_suspend_en, "u3-1-suspend-en", "ext-26m", 0x564,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dsi0_ref_en, "dsi0-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dsi1_ref_en, "dsi1-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dsi2_ref_en, "dsi2-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie_ref_en, "pcie-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(access_aud_en, "access-aud-en", "ext-26m", 0x648,
		     0x1000, BIT(0), 0, 0);

static struct sprd_clk_common *udx710_aon_gate[] = {
	/* address base is 0x64020000 */
	&rc100_cal_eb.common,
	&aon_spi_eb.common,
	&djtag_tck_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&bsm_tmr_eb.common,
	&aon_apb_bm_eb.common,
	&pmu_apb_bm_eb.common,
	&apcpu_cssys_eb.common,
	&debug_filter_eb.common,
	&apcpu_dap_eb.common,
	&cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&sd0_cfg_eb.common,
	&sd0_ref_eb.common,
	&sd1_cfg_eb.common,
	&sd1_ref_eb.common,
	&sd2_cfg_eb.common,
	&sd2_ref_eb.common,
	&serdes0_eb.common,
	&serdes1_eb.common,
	&serdes2_eb.common,
	&rtm_eb.common,
	&rtm_atb_eb.common,
	&aon_nr_spi_eb.common,
	&aon_bm_s5_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&dvfs_top_eb.common,
	&apcpu_clk_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&aon_ckg_eb.common,
	&djtag_ctrl_eb.common,
	&apcpu_ts0_eb.common,
	&nic400_aon_eb.common,
	&scc_eb.common,
	&ap_spi0_eb.common,
	&ap_spi1_eb.common,
	&ap_spi2_eb.common,
	&aon_bm_s3_eb.common,
	&sc_cc_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&ap_sim_eb.common,
	&aon_i2c_eb.common,
	&pmu_eb.common,
	&adi_eb.common,
	&eic_eb.common,
	&ap_intc0_eb.common,
	&ap_intc1_eb.common,
	&ap_intc2_eb.common,
	&ap_intc3_eb.common,
	&ap_intc4_eb.common,
	&ap_intc5_eb.common,
	&audcp_intc_eb.common,
	&ap_tmr0_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&thm2_eb.common,
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
	&dsi0_test_eb.common,
	&dsi1_test_eb.common,
	&dsi2_test_eb.common,
	&dmc_ref_en.common,
	&tsen_en.common,
	&tmr_en.common,
	&rc100_ref_en.common,
	&rc100_fdk_en.common,
	&debounce_en.common,
	&det_32k_eb.common,
	&cssys_en.common,
	&sdio0_2x_en.common,
	&sdio0_1x_en.common,
	&sdio1_2x_en.common,
	&sdio1_1x_en.common,
	&sdio2_2x_en.common,
	&sdio2_1x_en.common,
	&emmc_1x_en.common,
	&emmc_2x_en.common,
	&nandc_1x_en.common,
	&nandc_2x_en.common,
	&all_pll_test_eb.common,
	&aapc_test_eb.common,
	&debug_ts_eb.common,
	&u2_0_ref_en.common,
	&u2_1_ref_en.common,
	&u3_0_ref_en.common,
	&u3_0_suspend_en.common,
	&u3_1_ref_en.common,
	&u3_1_suspend_en.common,
	&dsi0_ref_en.common,
	&dsi1_ref_en.common,
	&dsi2_ref_en.common,
	&pcie_ref_en.common,
	&access_aud_en.common,
};

static struct clk_hw_onecell_data udx710_aon_gate_hws = {
	.hws	= {
		[CLK_RC100_CAL_EB] = &rc100_cal_eb.common.hw,
		[CLK_AON_SPI_EB] = &aon_spi_eb.common.hw,
		[CLK_DJTAG_TCK_EB] = &djtag_tck_eb.common.hw,
		[CLK_DJTAG_EB] = &djtag_eb.common.hw,
		[CLK_AUX0_EB] = &aux0_eb.common.hw,
		[CLK_AUX1_EB] = &aux1_eb.common.hw,
		[CLK_AUX2_EB] = &aux2_eb.common.hw,
		[CLK_PROBE_EB] = &probe_eb.common.hw,
		[CLK_BSM_TMR_EB] = &bsm_tmr_eb.common.hw,
		[CLK_AON_APB_BM_EB] = &aon_apb_bm_eb.common.hw,
		[CLK_PMU_APB_BM_EB] = &pmu_apb_bm_eb.common.hw,
		[CLK_APCPU_CSSYS_EB] = &apcpu_cssys_eb.common.hw,
		[CLK_DEBUG_FILTER_EB] = &debug_filter_eb.common.hw,
		[CLK_APCPU_DAP_EB] = &apcpu_dap_eb.common.hw,
		[CLK_CSSYS_EB] = &cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB] = &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB] = &cssys_pub_eb.common.hw,
		[CLK_SD0_CFG_EB] = &sd0_cfg_eb.common.hw,
		[CLK_SD0_REF_EB] = &sd0_ref_eb.common.hw,
		[CLK_SD1_CFG_EB] = &sd1_cfg_eb.common.hw,
		[CLK_SD1_REF_EB] = &sd1_ref_eb.common.hw,
		[CLK_SD2_CFG_EB] = &sd2_cfg_eb.common.hw,
		[CLK_SD2_REF_EB] = &sd2_ref_eb.common.hw,
		[CLK_SERDES0_EB] = &serdes0_eb.common.hw,
		[CLK_SERDES1_EB] = &serdes1_eb.common.hw,
		[CLK_SERDES2_EB] = &serdes2_eb.common.hw,
		[CLK_RTM_EB] = &rtm_eb.common.hw,
		[CLK_RTM_ATB_EB] = &rtm_atb_eb.common.hw,
		[CLK_AON_NR_SPI_EB] = &aon_nr_spi_eb.common.hw,
		[CLK_AON_BM_S5_EB] = &aon_bm_s5_eb.common.hw,
		[CLK_EFUSE_EB] = &efuse_eb.common.hw,
		[CLK_GPIO_EB] = &gpio_eb.common.hw,
		[CLK_MBOX_EB] = &mbox_eb.common.hw,
		[CLK_KPD_EB] = &kpd_eb.common.hw,
		[CLK_AON_SYST_EB] = &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB] = &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB] = &aon_tmr_eb.common.hw,
		[CLK_DVFS_TOP_EB] = &dvfs_top_eb.common.hw,
		[CLK_APCPU_CLK_EB] = &apcpu_clk_eb.common.hw,
		[CLK_SPLK_EB] = &splk_eb.common.hw,
		[CLK_PIN_EB] = &pin_eb.common.hw,
		[CLK_ANA_EB] = &ana_eb.common.hw,
		[CLK_AON_CKG_EB] = &aon_ckg_eb.common.hw,
		[CLK_DJTAG_CTRL_EB] = &djtag_ctrl_eb.common.hw,
		[CLK_APCPU_TS0_EB] = &apcpu_ts0_eb.common.hw,
		[CLK_NIC400_AON_EB] = &nic400_aon_eb.common.hw,
		[CLK_SCC_EB] = &scc_eb.common.hw,
		[CLK_AP_SPI0_EB] = &ap_spi0_eb.common.hw,
		[CLK_AP_SPI1_EB] = &ap_spi1_eb.common.hw,
		[CLK_AP_SPI2_EB] = &ap_spi2_eb.common.hw,
		[CLK_AON_BM_S3_EB] = &aon_bm_s3_eb.common.hw,
		[CLK_SC_CC_EB] = &sc_cc_eb.common.hw,
		[CLK_THM0_EB] = &thm0_eb.common.hw,
		[CLK_THM1_EB] = &thm1_eb.common.hw,
		[CLK_AP_SIM_EB] = &ap_sim_eb.common.hw,
		[CLK_AON_I2C_EB] = &aon_i2c_eb.common.hw,
		[CLK_PMU_EB] = &pmu_eb.common.hw,
		[CLK_ADI_EB] = &adi_eb.common.hw,
		[CLK_EIC_EB] = &eic_eb.common.hw,
		[CLK_AP_INTC0_EB] = &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB] = &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB] = &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB] = &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB] = &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB] = &ap_intc5_eb.common.hw,
		[CLK_AUDCP_INTC_EB] = &audcp_intc_eb.common.hw,
		[CLK_AP_TMR0_EB] = &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB] = &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB] = &ap_tmr2_eb.common.hw,
		[CLK_AP_WDG_EB] = &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB] = &apcpu_wdg_eb.common.hw,
		[CLK_THM2_EB] = &thm2_eb.common.hw,
		[CLK_ARCH_RTC_EB] = &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB] = &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB] = &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB] = &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB] = &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB] = &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB] = &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB] = &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB] = &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB] = &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB] = &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB] = &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB] = &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB] = &bb_cal_rtc_eb.common.hw,
		[CLK_DSI0_TEST_EB] = &dsi0_test_eb.common.hw,
		[CLK_DSI1_TEST_EB] = &dsi1_test_eb.common.hw,
		[CLK_DSI2_TEST_EB] = &dsi2_test_eb.common.hw,
		[CLK_DMC_REF_EN] = &dmc_ref_en.common.hw,
		[CLK_TSEN_EN] = &tsen_en.common.hw,
		[CLK_TMR_EN] = &tmr_en.common.hw,
		[CLK_RC100_REF_EN] = &rc100_ref_en.common.hw,
		[CLK_RC100_FDK_EN] = &rc100_fdk_en.common.hw,
		[CLK_DEBOUNCE_EN] = &debounce_en.common.hw,
		[CLK_DET_32K_EB] = &det_32k_eb.common.hw,
		[CLK_CSSYS_EN] = &cssys_en.common.hw,
		[CLK_SDIO0_2X_EN] = &sdio0_2x_en.common.hw,
		[CLK_SDIO0_1X_EN] = &sdio0_1x_en.common.hw,
		[CLK_SDIO1_2X_EN] = &sdio1_2x_en.common.hw,
		[CLK_SDIO1_1X_EN] = &sdio1_1x_en.common.hw,
		[CLK_SDIO2_2X_EN] = &sdio2_2x_en.common.hw,
		[CLK_SDIO2_1X_EN] = &sdio2_1x_en.common.hw,
		[CLK_EMMC_1X_EN] = &emmc_1x_en.common.hw,
		[CLK_EMMC_2X_EN] = &emmc_2x_en.common.hw,
		[CLK_NANDC_1X_EN] = &nandc_1x_en.common.hw,
		[CLK_NANDC_2X_EN] = &nandc_2x_en.common.hw,
		[CLK_ALL_PLL_TEST_EB] = &all_pll_test_eb.common.hw,
		[CLK_AAPC_TEST_EB] = &aapc_test_eb.common.hw,
		[CLK_DEBUG_TS_EB] = &debug_ts_eb.common.hw,
		[CLK_U2_0_REF_EN] = &u2_0_ref_en.common.hw,
		[CLK_U2_1_REF_EN] = &u2_1_ref_en.common.hw,
		[CLK_U3_0_REF_EN] = &u3_0_ref_en.common.hw,
		[CLK_U3_0_SUSPEND_EN] = &u3_0_suspend_en.common.hw,
		[CLK_U3_1_REF_EN] = &u3_1_ref_en.common.hw,
		[CLK_U3_1_SUSPEND_EN] = &u3_1_suspend_en.common.hw,
		[CLK_DSI0_REF_EN] = &dsi0_ref_en.common.hw,
		[CLK_DSI1_REF_EN] = &dsi1_ref_en.common.hw,
		[CLK_DSI2_REF_EN] = &dsi2_ref_en.common.hw,
		[CLK_PCIE_REF_EN] = &pcie_ref_en.common.hw,
		[CLK_ACCESS_AUD_EN] = &access_aud_en.common.hw,
	},
	.num	= CLK_AON_GATE_NUM,
};

static struct sprd_reset_map udx710_aon_apb_resets[] = {
	[RESET_AON_APB_RC100M_CAL_SOFT_RST]			= { 0x0200, BIT(0), 0x1000 },
	[RESET_AON_APB_AON_SPI_SOFT_RST]			= { 0x0200, BIT(1), 0x1000 },
	[RESET_AON_APB_DCXO_LC_SOFT_RST]			= { 0x0200, BIT(2), 0x1000 },
	[RESET_AON_APB_BB_CAL_SOFT_RST]				= { 0x0200, BIT(3), 0x1000 },
	[RESET_AON_APB_AON_NR_SPI_SOFT_RST]			= { 0x0200, BIT(4), 0x1000 },
	[RESET_AON_APB_DAP_MTX_SOFT_RST]			= { 0x0200, BIT(6), 0x1000 },
	[RESET_AON_APB_SERDES_DPHY0_SOFT_RST]			= { 0x0200, BIT(8), 0x1000 },
	[RESET_AON_APB_SERDES_DPHY0_APB_SOFT_RST]		= { 0x0200, BIT(9), 0x1000 },
	[RESET_AON_APB_SERDES_DPHY1_SOFT_RST]			= { 0x0200, BIT(10), 0x1000 },
	[RESET_AON_APB_SERDES_DPHY1_APB_SOFT_RST]		= { 0x0200, BIT(11), 0x1000 },
	[RESET_AON_APB_SERDES_DPHY2_SOFT_RST]			= { 0x0200, BIT(12), 0x1000 },
	[RESET_AON_APB_SERDES_DPHY2_APB_SOFT_RST]		= { 0x0200, BIT(13), 0x1000 },
	[RESET_AON_APB_SERDES0_SOFT_RST]			= { 0x0200, BIT(14), 0x1000 },
	[RESET_AON_APB_SERDES1_SOFT_RST]			= { 0x0200, BIT(15), 0x1000 },
	[RESET_AON_APB_SERDES2_SOFT_RST]			= { 0x0200, BIT(16), 0x1000 },
	[RESET_AON_APB_RTM_SOFT_RST]				= { 0x0200, BIT(17), 0x1000 },
	[RESET_AON_APB_WCN_ADDA_TEST_SOFT_RST]			= { 0x0200, BIT(18), 0x1000 },
	[RESET_AON_APB_V3PS_SIM0_AON_TOP_SOFT_RST]		= { 0x0200, BIT(19), 0x1000 },
	[RESET_AON_APB_V3PS_SIM1_AON_TOP_SOFT_RST]		= { 0x0200, BIT(20), 0x1000 },
	[RESET_AON_APB_V3PS_SIM2_AON_TOP_SOFT_RST]		= { 0x0200, BIT(21), 0x1000 },
	[RESET_AON_APB_EFUSE_SOFT_RST]				= { 0x0204, BIT(0), 0x1000 },
	[RESET_AON_APB_GPIO_SOFT_RST]				= { 0x0204, BIT(1), 0x1000 },
	[RESET_AON_APB_MBOX_SOFT_RST]				= { 0x0204, BIT(2), 0x1000 },
	[RESET_AON_APB_KPD_SOFT_RST]				= { 0x0204, BIT(3), 0x1000 },
	[RESET_AON_APB_AON_SYST_SOFT_RST]			= { 0x0204, BIT(4), 0x1000 },
	[RESET_AON_APB_AP_SYST_SOFT_RST]			= { 0x0204, BIT(5), 0x1000 },
	[RESET_AON_APB_AON_TMR_SOFT_RST]			= { 0x0204, BIT(6), 0x1000 },
	[RESET_AON_APB_DVFS_TOP_SOFT_RST]			= { 0x0204, BIT(7), 0x1000 },
	[RESET_AON_APB_APCPU_CLK_RF_SOFT_RST]			= { 0x0204, BIT(8), 0x1000 },
	[RESET_AON_APB_SPLK_SOFT_RST]				= { 0x0204, BIT(10), 0x1000 },
	[RESET_AON_APB_PIN_SOFT_RST]				= { 0x0204, BIT(11), 0x1000 },
	[RESET_AON_APB_ANA_SOFT_RST]				= { 0x0204, BIT(12), 0x1000 },
	[RESET_AON_APB_CKG_SOFT_RST]				= { 0x0204, BIT(13), 0x1000 },
	[RESET_AON_APB_SEC_32K_DET_SOFT_RST]			= { 0x0204, BIT(14), 0x1000 },
	[RESET_AON_APB_APCPU_TS0_SOFT_RST]			= { 0x0204, BIT(17), 0x1000 },
	[RESET_AON_APB_APB_DIRECT_ACC_PROT_SOFT_RST]		= { 0x0204, BIT(18), 0x1000 },
	[RESET_AON_APB_SCC_SOFT_RST]				= { 0x0204, BIT(20), 0x1000 },
	[RESET_AON_APB_APB_DIRECT_ACC_PROT_PMU_APB_SOFT_RST]	= { 0x0204, BIT(21), 0x1000 },
	[RESET_AON_APB_APB_DIRECT_ACC_PROT_AON_APB_SOFT_RST]	= { 0x0204, BIT(22), 0x1000 },
	[RESET_AON_APB_BSMTMR_SOFT_RST]				= { 0x0204, BIT(23), 0x1000 },
	[RESET_AON_APB_CSSYS_SOFT_RST]				= { 0x0204, BIT(24), 0x1000 },
	[RESET_AON_APB_AON_APB_BUSMON_SOFT_RST]			= { 0x0204, BIT(25), 0x1000 },
	[RESET_AON_APB_PMU_APB_BUSMON_SOFT_RST]			= { 0x0204, BIT(26), 0x1000 },
	[RESET_AON_APB_AON_RFSPI_SLV_V3_SOFT_RST]		= { 0x0204, BIT(27), 0x1000 },
	[RESET_AON_APB_AON_RFSPI_SLV_NR_SOFT_RST]		= { 0x0204, BIT(28), 0x1000 },
	[RESET_AON_APB_THM0_SOFT_RST]				= { 0x0208, BIT(0), 0x1000 },
	[RESET_AON_APB_THM1_SOFT_RST]				= { 0x0208, BIT(1), 0x1000 },
	[RESET_AON_APB_AP_SIM_AON_TOP_SOFT_RST]			= { 0x0208, BIT(2), 0x1000 },
	[RESET_AON_APB_PSCP_SIM0_AON_TOP_SOFT_RST]		= { 0x0208, BIT(3), 0x1000 },
	[RESET_AON_APB_PSCP_SIM1_AON_TOP_SOFT_RST]		= { 0x0208, BIT(4), 0x1000 },
	[RESET_AON_APB_PSCP_SIM2_AON_TOP_SOFT_RST]		= { 0x0208, BIT(5), 0x1000 },
	[RESET_AON_APB_I2C_SOFT_RST]				= { 0x0208, BIT(6), 0x1000 },
	[RESET_AON_APB_PMU_SOFT_RST]				= { 0x0208, BIT(7), 0x1000 },
	[RESET_AON_APB_ADI_SOFT_RST]				= { 0x0208, BIT(8), 0x1000 },
	[RESET_AON_APB_EIC_SOFT_RST]				= { 0x0208, BIT(9), 0x1000 },
	[RESET_AON_APB_AP_INTC0_SOFT_RST]			= { 0x0208, BIT(10), 0x1000 },
	[RESET_AON_APB_AP_INTC1_SOFT_RST]			= { 0x0208, BIT(11), 0x1000 },
	[RESET_AON_APB_AP_INIC2_SOFT_RST]			= { 0x0208, BIT(12), 0x1000 },
	[RESET_AON_APB_AP_INTC3_SOFT_RST]			= { 0x0208, BIT(13), 0x1000 },
	[RESET_AON_APB_AP_INTC4_CAL_SOFT_RST]			= { 0x0208, BIT(14), 0x1000 },
	[RESET_AON_APB_AP_INTC5_CAL_SOFT_RST]			= { 0x0208, BIT(15), 0x1000 },
	[RESET_AON_APB_AUDCP_INTC_SOFT_RST]			= { 0x0208, BIT(16), 0x1000 },
	[RESET_AON_APB_PSCP_INTC_SOFT_RST]			= { 0x0208, BIT(17), 0x1000 },
	[RESET_AON_APB_V3MODEM_INTC_SOFT_RST]			= { 0x0208, BIT(18), 0x1000 },
	[RESET_AON_APB_V3PS_INTC_SOFT_RST]			= { 0x0208, BIT(19), 0x1000 },
	[RESET_AON_APB_V3PHY_INTC_SOFT_RST]			= { 0x0208, BIT(20), 0x1000 },
	[RESET_AON_APB_NR_CR8_INTC_SOFT_RST]			= { 0x0208, BIT(21), 0x1000 },
	[RESET_AON_APB_NR_DSP0_INTC_SOFT_RST]			= { 0x0208, BIT(22), 0x1000 },
	[RESET_AON_APB_NR_DSP1_INTC_SOFT_RST]			= { 0x0208, BIT(23), 0x1000 },
	[RESET_AON_APB_NR_INTC_SOFT_RST]			= { 0x0208, BIT(24), 0x1000 },
	[RESET_AON_APB_THM2_SOFT_RST]				= { 0x0208, BIT(25), 0x1000 },
	[RESET_AON_APB_AP_TMR0_SOFT_RST]			= { 0x0208, BIT(27), 0x1000 },
	[RESET_AON_APB_AP_TMR1_SOFT_RST]			= { 0x0208, BIT(28), 0x1000 },
	[RESET_AON_APB_AP_TMR2_SOFT_RST]			= { 0x0208, BIT(29), 0x1000 },
	[RESET_AON_APB_AP_WDG_SOFT_RST]				= { 0x0208, BIT(30), 0x1000 },
	[RESET_AON_APB_APCPU_WDG_SOFT_RST]			= { 0x0208, BIT(31), 0x1000 },
	[RESET_AON_APB_DJTAG_AP_SOFT_RST]			= { 0x020C, BIT(0), 0x1000 },
	[RESET_AON_APB_DJTAG_APCPU_SOFT_RST]			= { 0x020C, BIT(1), 0x1000 },
	[RESET_AON_APB_DJTAG_NRCP_SOFT_RST]			= { 0x020C, BIT(4), 0x1000 },
	[RESET_AON_APB_DJTAG_PSCP_SOFT_RST]			= { 0x020C, BIT(5), 0x1000 },
	[RESET_AON_APB_DJTAG_V3_SOFT_RST]			= { 0x020C, BIT(6), 0x1000 },
	[RESET_AON_APB_DJTAG_AUDCP_SOFT_RST]			= { 0x020C, BIT(7), 0x1000 },
	[RESET_AON_APB_DJTAG_AON_SOFT_RST]			= { 0x020C, BIT(8), 0x1000 },
	[RESET_AON_APB_DJTAG_PUB0_SOFT_RST]			= { 0x020C, BIT(9), 0x1000 },
	[RESET_AON_APB_DJTAG_SOFT_RST]				= { 0x020C, BIT(10), 0x1000 },
};

static const struct sprd_clk_desc udx710_aon_gate_desc = {
	.clk_clks	= udx710_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(udx710_aon_gate),
	.hw_clks	= &udx710_aon_gate_hws,
	.resets		= udx710_aon_apb_resets,
	.num_resets	= ARRAY_SIZE(udx710_aon_apb_resets),
};

/* apcpu clocks */
static const struct clk_parent_data core_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_512m.hw },
	{ .hw = &v3pll_768m.hw },
	{ .hw = &mpll0.common.hw },
};
static SPRD_COMP_CLK_DATA(core0_clk, "core0-clk", core_parents,
		     0x20, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(core1_clk, "core1-clk", core_parents,
		     0x24, 0, 2, 8, 3, 0);

static const struct clk_parent_data scu_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_512m.hw },
	{ .hw = &v3pll_768m.hw },
	{ .hw = &mpll1.common.hw },
};
static SPRD_COMP_CLK_DATA(scu_clk, "scu-clk", scu_parents,
		     0x28, 0, 2, 8, 3, 0);
static SPRD_DIV_CLK_HW(ace_clk, "ace-clk", &scu_clk.common.hw, 0x2c,
		    8, 3, 0);

static const struct clk_parent_data gic_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_153m6.hw },
	{ .hw = &v3pll_384m.hw },
	{ .hw = &v3pll_512m.hw },
};
static SPRD_COMP_CLK_DATA(gic_clk, "gic-clk", gic_parents,
		     0x38, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(periph_clk, "periph-clk", gic_parents,
		     0x3c, 0, 2, 8, 3, 0);

static struct sprd_clk_common *udx710_apcpu_clk_clks[] = {
	/* address base is 0x63970000 */
	&core0_clk.common,
	&core1_clk.common,
	&scu_clk.common,
	&ace_clk.common,
	&gic_clk.common,
	&periph_clk.common,
};

static struct clk_hw_onecell_data udx710_apcpu_clk_hws = {
	.hws	= {
		[CLK_CORE0_CLK] = &core0_clk.common.hw,
		[CLK_CORE1_CLK] = &core1_clk.common.hw,
		[CLK_SCU_CLK] = &scu_clk.common.hw,
		[CLK_ACE_CLK] = &ace_clk.common.hw,
		[CLK_GIC_CLK] = &gic_clk.common.hw,
		[CLK_PERIPH_CLK] = &periph_clk.common.hw,
	},
	.num	= CLK_APCPU_CLK_NUM,
};

static struct sprd_clk_desc udx710_apcpu_clk_desc = {
	.clk_clks	= udx710_apcpu_clk_clks,
	.num_clk_clks	= ARRAY_SIZE(udx710_apcpu_clk_clks),
	.hw_clks	= &udx710_apcpu_clk_hws,
};

/* ap ahb gates */
static SPRD_SC_GATE_CLK_FW_NAME(apahb_ckg_eb, "apahb-ckg-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_eb, "nandc-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_ecc_eb, "nandc-ecc-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(nandc_26m_eb, "nandc-26m-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dma_eb, "dma-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(dma_eb2, "dma-eb2", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb0_eb, "usb0-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb0_suspend_eb, "usb0-suspend-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb0_ref_eb, "usb0-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio_mst_eb, "sdio-mst-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio_mst_32k_eb, "sdio-mst-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_eb, "emmc-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(emmc_32k_eb, "emmc-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *udx710_apahb_gate[] = {
	/* address base is 0x21000000 */
	&apahb_ckg_eb.common,
	&nandc_eb.common,
	&nandc_ecc_eb.common,
	&nandc_26m_eb.common,
	&dma_eb.common,
	&dma_eb2.common,
	&usb0_eb.common,
	&usb0_suspend_eb.common,
	&usb0_ref_eb.common,
	&sdio_mst_eb.common,
	&sdio_mst_32k_eb.common,
	&emmc_eb.common,
	&emmc_32k_eb.common,
};

static struct clk_hw_onecell_data udx710_apahb_gate_hws = {
	.hws	= {
		[CLK_APAHB_CKG_EB] = &apahb_ckg_eb.common.hw,
		[CLK_NANDC_EB] = &nandc_eb.common.hw,
		[CLK_NANDC_ECC_EB] = &nandc_ecc_eb.common.hw,
		[CLK_NANDC_26M_EB] = &nandc_26m_eb.common.hw,
		[CLK_DMA_EB] = &dma_eb.common.hw,
		[CLK_DMA_EB2] = &dma_eb2.common.hw,
		[CLK_USB0_EB] = &usb0_eb.common.hw,
		[CLK_USB0_SUSPEND_EB] = &usb0_suspend_eb.common.hw,
		[CLK_USB0_REF_EB] = &usb0_ref_eb.common.hw,
		[CLK_SDIO_MST_EB] = &sdio_mst_eb.common.hw,
		[CLK_SDIO_MST_32K_EB] = &sdio_mst_32k_eb.common.hw,
		[CLK_EMMC_EB] = &emmc_eb.common.hw,
		[CLK_EMMC_32K_EB] = &emmc_32k_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static struct sprd_reset_map udx710_ap_ahb_resets[] = {
	[RESET_AP_AHB_NANDC_SOFT_RST]		= { 0x0004, BIT(0), 0x1000 },
	[RESET_AP_AHB_USB0_SOFT_RST]		= { 0x0004, BIT(1), 0x1000 },
	[RESET_AP_AHB_DMA_SOFT_RST]		= { 0x0004, BIT(2), 0x1000 },
	[RESET_AP_AHB_SDIO_MST_SOFT_RST]	= { 0x0004, BIT(3), 0x1000 },
	[RESET_AP_AHB_EMMC_SOFT_RST]		= { 0x0004, BIT(4), 0x1000 },
};

static const struct sprd_clk_desc udx710_apahb_gate_desc = {
	.clk_clks	= udx710_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(udx710_apahb_gate),
	.hw_clks	= &udx710_apahb_gate_hws,
	.resets		= udx710_ap_ahb_resets,
	.num_resets	= ARRAY_SIZE(udx710_ap_ahb_resets),
};

/* ap clocks */
#define UDX710_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

static const struct clk_parent_data ap_axi_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_64m.hw },
	{ .hw = &v3pll_96m.hw },
	{ .hw = &v3pll_128m.hw },
	{ .hw = &v3pll_256m.hw },
	{ .hw = &v3rpll.hw },
};
static SPRD_MUX_CLK_DATA(ap_axi_clk, "ap-axi-clk", ap_axi_parents, 0x20,
			0, 3, UDX710_MUX_FLAG);

static const struct clk_parent_data peri_apb_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_64m.hw },
	{ .hw = &v3pll_96m.hw },
	{ .hw = &v3pll_128m.hw },
};
static SPRD_MUX_CLK_DATA(peri_apb_clk, "peri-apb-clk", peri_apb_parents, 0x24,
			0, 2, UDX710_MUX_FLAG);

static const struct clk_parent_data nandc_ecc_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_256m.hw },
	{ .hw = &v3pll_307m2.hw },
};
static SPRD_COMP_CLK_DATA(nandc_ecc_clk, "nandc-ecc-clk", nandc_ecc_parents,
		     0x28, 0, 2, 8, 3, 0);

static const struct clk_parent_data usb_ref_parents[] = {
	{ .fw_name = "ext-32k" },
	{ .hw = &v3pll_24m.hw },
};
static SPRD_MUX_CLK_DATA(usb0_ref_clk, "usb0-ref-clk", usb_ref_parents, 0x3c,
			0, 1, UDX710_MUX_FLAG);
static SPRD_MUX_CLK_DATA(usb1_ref_clk, "usb1-ref-clk", usb_ref_parents, 0x40,
			0, 1, UDX710_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(pcie_aux_clk, "pcie-aux-clk", "ext-26m", 0x44,
			BIT(0), CLK_IGNORE_UNUSED, 0);

static const struct clk_parent_data ap_uart_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_48m.hw },
	{ .hw = &v3pll_51m2.hw },
	{ .hw = &v3pll_96m.hw },
};
static SPRD_COMP_CLK_DATA(ap_uart0_clk, "ap-uart0-clk", ap_uart_parents,
		     0x48, 0, 2, 8, 3, 0);

static const struct clk_parent_data ap_i2c_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_48m.hw },
	{ .hw = &v3pll_51m2.hw },
	{ .hw = &v3pll_153m6.hw },
};
static SPRD_COMP_CLK_DATA(ap_i2c0_clk, "ap-i2c0-clk", ap_i2c_parents,
		     0x4c, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_i2c1_clk, "ap-i2c1-clk", ap_i2c_parents,
		     0x50, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_i2c2_clk, "ap-i2c2-clk", ap_i2c_parents,
		     0x54, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_i2c3_clk, "ap-i2c3-clk", ap_i2c_parents,
		     0x58, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK_DATA(ap_i2c4_clk, "ap-i2c4-clk", ap_i2c_parents,
		     0x5c, 0, 2, 8, 3, 0);

static const struct clk_parent_data ap_sim_parents[] = {
	{ .fw_name = "ext-26m" },
	{ .hw = &v3pll_51m2.hw },
	{ .hw = &v3pll_64m.hw },
	{ .hw = &v3pll_96m.hw },
	{ .hw = &v3pll_128m.hw },
};
static SPRD_COMP_CLK_DATA(ap_sim_clk, "ap-sim-clk", ap_sim_parents,
		     0x64, 0, 3, 8, 3, 0);

static const struct clk_parent_data pwm_parents[] = {
	{ .fw_name = "clk-32k" },
	{ .fw_name = "ext-26m" },
	{ .hw = &rco_4m.hw },
	{ .hw = &rco_25m.hw },
	{ .hw = &v3pll_48m.hw },
};
static SPRD_MUX_CLK_DATA(pwm0_clk, "pwm0-clk", pwm_parents, 0x68,
		    0, 3, UDX710_MUX_FLAG);
static SPRD_MUX_CLK_DATA(pwm1_clk, "pwm1-clk", pwm_parents, 0x6c,
		    0, 3, UDX710_MUX_FLAG);
static SPRD_MUX_CLK_DATA(pwm2_clk, "pwm2-clk", pwm_parents, 0x70,
		    0, 3, UDX710_MUX_FLAG);
static SPRD_MUX_CLK_DATA(pwm3_clk, "pwm3-clk", pwm_parents, 0x74,
		    0, 3, UDX710_MUX_FLAG);

static SPRD_GATE_CLK_FW_NAME(usb0_pipe_clk, "usb0-pipe-clk", "ext-26m", 0x78,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(usb0_utmi_clk, "usb0-utmi-clk", "ext-26m", 0x7c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(usb1_pipe_clk, "usb1-pipe-clk", "ext-26m", 0x80,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(usb1_utmi_clk, "usb1-utmi-clk", "ext-26m", 0x84,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK_FW_NAME(pcie_pipe_clk, "pcie-pipe-clk", "ext-26m", 0x88,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *udx710_ap_clks[] = {
	/* address base is 0x21100000 */
	&ap_axi_clk.common,
	&peri_apb_clk.common,
	&nandc_ecc_clk.common,
	&usb0_ref_clk.common,
	&usb1_ref_clk.common,
	&pcie_aux_clk.common,
	&ap_uart0_clk.common,
	&ap_i2c0_clk.common,
	&ap_i2c1_clk.common,
	&ap_i2c2_clk.common,
	&ap_i2c3_clk.common,
	&ap_i2c4_clk.common,
	&ap_sim_clk.common,
	&pwm0_clk.common,
	&pwm1_clk.common,
	&pwm2_clk.common,
	&pwm3_clk.common,
	&usb0_pipe_clk.common,
	&usb0_utmi_clk.common,
	&usb1_pipe_clk.common,
	&usb1_utmi_clk.common,
	&pcie_pipe_clk.common,
};

static struct clk_hw_onecell_data udx710_ap_clk_hws = {
	.hws	= {
		[CLK_AP_AXI] = &ap_axi_clk.common.hw,
		[CLK_PERI_APB] = &peri_apb_clk.common.hw,
		[CLK_NANDC_ECC] = &nandc_ecc_clk.common.hw,
		[CLK_USB0_REF] = &usb0_ref_clk.common.hw,
		[CLK_USB1_REF] = &usb1_ref_clk.common.hw,
		[CLK_PCIE_AUX] = &pcie_aux_clk.common.hw,
		[CLK_AP_UART0] = &ap_uart0_clk.common.hw,
		[CLK_AP_I2C0] = &ap_i2c0_clk.common.hw,
		[CLK_AP_I2C1] = &ap_i2c1_clk.common.hw,
		[CLK_AP_I2C2] = &ap_i2c2_clk.common.hw,
		[CLK_AP_I2C3] = &ap_i2c3_clk.common.hw,
		[CLK_AP_I2C4] = &ap_i2c4_clk.common.hw,
		[CLK_AP_SIM] = &ap_sim_clk.common.hw,
		[CLK_PWM0] = &pwm0_clk.common.hw,
		[CLK_PWM1] = &pwm1_clk.common.hw,
		[CLK_PWM2] = &pwm2_clk.common.hw,
		[CLK_PWM3] = &pwm3_clk.common.hw,
		[CLK_USB0_PIPE] = &usb0_pipe_clk.common.hw,
		[CLK_USB0_UTMI] = &usb0_utmi_clk.common.hw,
		[CLK_USB1_PIPE] = &usb1_pipe_clk.common.hw,
		[CLK_USB1_UTMI] = &usb1_utmi_clk.common.hw,
		[CLK_PCIE_PIPE] = &pcie_pipe_clk.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc udx710_ap_clk_desc = {
	.clk_clks	= udx710_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(udx710_ap_clks),
	.hw_clks	= &udx710_ap_clk_hws,
};

/* ap apb gates */
static SPRD_SC_GATE_CLK_FW_NAME(apapb_reg_eb, "apapb-reg-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_uart0_eb, "ap-uart0-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_i2c0_eb, "ap-i2c0-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_i2c1_eb, "ap-i2c1-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_i2c2_eb, "ap-i2c2-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_i2c3_eb2, "ap-i2c3-eb2", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_i2c4_eb, "ap-i2c4-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_apb_spi0_eb, "ap-apb-spi0-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi0_lf_in_eb, "spi0-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_apb_spi1_eb, "ap-apb-spi1-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi1_lf_in_eb, "spi1-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ap_apb_spi2_eb, "ap-apb-spi2-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(spi2_lf_in_eb, "spi2-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm0_eb, "pwm0-eb", "ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm1_eb, "pwm1-eb", "ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm2_eb, "pwm2-eb", "ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pwm3_eb, "pwm3-eb", "ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sim0_eb, "sim0-eb", "ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sim0_32k_eb, "sim0-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *udx710_apapb_gate[] = {
	/* address base is 0x24000000 */
	&apapb_reg_eb.common,
	&ap_uart0_eb.common,
	&ap_i2c0_eb.common,
	&ap_i2c1_eb.common,
	&ap_i2c2_eb.common,
	&ap_i2c3_eb2.common,
	&ap_i2c4_eb.common,
	&ap_apb_spi0_eb.common,
	&spi0_lf_in_eb.common,
	&ap_apb_spi1_eb.common,
	&spi1_lf_in_eb.common,
	&ap_apb_spi2_eb.common,
	&spi2_lf_in_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&sim0_eb.common,
	&sim0_32k_eb.common,
};

static struct clk_hw_onecell_data udx710_apapb_gate_hws = {
	.hws	= {
		[CLK_APAPB_REG_EB] = &apapb_reg_eb.common.hw,
		[CLK_AP_UART0_EB] = &ap_uart0_eb.common.hw,
		[CLK_AP_I2C0_EB] = &ap_i2c0_eb.common.hw,
		[CLK_AP_I2C1_EB] = &ap_i2c1_eb.common.hw,
		[CLK_AP_I2C2_EB] = &ap_i2c2_eb.common.hw,
		[CLK_AP_I2C3_EB] = &ap_i2c3_eb2.common.hw,
		[CLK_AP_I2C4_EB] = &ap_i2c4_eb.common.hw,
		[CLK_AP_APB_SPI0_EB] = &ap_apb_spi0_eb.common.hw,
		[CLK_SPI0_LF_IN_EB] = &spi0_lf_in_eb.common.hw,
		[CLK_AP_APB_SPI1_EB] = &ap_apb_spi1_eb.common.hw,
		[CLK_SPI1_IF_IN_EB] = &spi1_lf_in_eb.common.hw,
		[CLK_AP_APB_SPI2_EB] = &ap_apb_spi2_eb.common.hw,
		[CLK_SPI2_IF_IN_EB] = &spi2_lf_in_eb.common.hw,
		[CLK_PWM0_EB] = &pwm0_eb.common.hw,
		[CLK_PWM1_EB] = &pwm1_eb.common.hw,
		[CLK_PWM2_EB] = &pwm2_eb.common.hw,
		[CLK_PWM3_EB] = &pwm3_eb.common.hw,
		[CLK_SIM0_EB] = &sim0_eb.common.hw,
		[CLK_SIM0_32K_EB] = &sim0_32k_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static struct sprd_reset_map udx710_ap_apb_resets[] = {
	[RESET_AP_APB_UART0_SOFT_RST]		= { 0x0004, BIT(0), 0x1000 },
	[RESET_AP_APB_I2C0_SOFT_RST]		= { 0x0004, BIT(1), 0x1000 },
	[RESET_AP_APB_I2C1_SOFT_RST]		= { 0x0004, BIT(2), 0x1000 },
	[RESET_AP_APB_I2C2_SOFT_RST]		= { 0x0004, BIT(3), 0x1000 },
	[RESET_AP_APB_I2C3_SOFT_RST]		= { 0x0004, BIT(4), 0x1000 },
	[RESET_AP_APB_I2C4_SOFT_RST]		= { 0x0004, BIT(5), 0x1000 },
	[RESET_AP_APB_SPI0_SOFT_RST]		= { 0x0004, BIT(6), 0x1000 },
	[RESET_AP_APB_SPI1_SOFT_RST]		= { 0x0004, BIT(7), 0x1000 },
	[RESET_AP_APB_SPI2_SOFT_RST]		= { 0x0004, BIT(8), 0x1000 },
	[RESET_AP_APB_PWM0_SOFT_RST]		= { 0x0004, BIT(9), 0x1000 },
	[RESET_AP_APB_PWM1_SOFT_RST]		= { 0x0004, BIT(10), 0x1000 },
	[RESET_AP_APB_PWM2_SOFT_RST]		= { 0x0004, BIT(11), 0x1000 },
	[RESET_AP_APB_PWM3_SOFT_RST]		= { 0x0004, BIT(12), 0x1000 },
	[RESET_AP_APB_SIM0_SOFT_RST]		= { 0x0004, BIT(13), 0x1000 },
};

static const struct sprd_clk_desc udx710_apapb_gate_desc = {
	.clk_clks	= udx710_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(udx710_apapb_gate),
	.hw_clks	= &udx710_apapb_gate_hws,
	.resets		= udx710_ap_apb_resets,
	.num_resets	= ARRAY_SIZE(udx710_ap_apb_resets),
};

/* ap ipa gate */
static SPRD_SC_GATE_CLK_FW_NAME(ipa_usb1_eb, "ipa-usb1-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb1_suspend_eb, "usb1-suspend-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_usb1_ref_eb, "ipa-usb1-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sdio_slv_eb, "sdio-slv-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(sd_slv_frun_eb, "sd-slv-frun-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie_eb, "pcie-eb", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie_aux_eb, "pcie-aux-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(ipa_eb, "ipa-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(usb_pam_eb, "usb-pam-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK_FW_NAME(pcie_sel, "pcie-sel", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *udx710_ipa_gate_clks[] = {
	/* address base is 0x29000000 */
	&ipa_usb1_eb.common,
	&usb1_suspend_eb.common,
	&ipa_usb1_ref_eb.common,
	&sdio_slv_eb.common,
	&sd_slv_frun_eb.common,
	&pcie_eb.common,
	&pcie_aux_eb.common,
	&ipa_eb.common,
	&usb_pam_eb.common,
	&pcie_sel.common,
};

static struct clk_hw_onecell_data udx710_ipa_gate_clk_hws = {
	.hws	= {
		[CLK_IPA_USB1_EB] = &ipa_usb1_eb.common.hw,
		[CLK_USB1_SUSPEND_EB] = &usb1_suspend_eb.common.hw,
		[CLK_IPA_USB1_REF_EB] = &ipa_usb1_ref_eb.common.hw,
		[CLK_SDIO_SLV_EB] = &sdio_slv_eb.common.hw,
		[CLK_SD_SLV_FRUN_EB] = &sd_slv_frun_eb.common.hw,
		[CLK_PCIE_EB] = &pcie_eb.common.hw,
		[CLK_PCIE_AUX_EB] = &pcie_aux_eb.common.hw,
		[CLK_IPA_EB] = &ipa_eb.common.hw,
		[CLK_USB_PAM_EB] = &usb_pam_eb.common.hw,
		[CLK_PCIE_SEL] = &pcie_sel.common.hw,
	},
	.num	= CLK_IPA_GATE_NUM,
};

static struct sprd_reset_map udx710_ipa_resets[] = {
	[RESET_IPA_USB1_SOFT_RST]		= { 0x0004, BIT(0), 0x1000 },
	[RESET_IPA_PCIE_BUT_SOFT_RST]		= { 0x0004, BIT(1), 0x1000 },
	[RESET_IPA_SDIO_SLV_SOFT_RST]		= { 0x0004, BIT(2), 0x1000 },
	[RESET_IPA_PCIE_SOFT_RST]		= { 0x0004, BIT(3), 0x1000 },
	[RESET_IPA_IPA_SOFT_RST]		= { 0x0004, BIT(4), 0x1000 },
	[RESET_IPA_USB_PAM_SOFT_RST]		= { 0x0004, BIT(5), 0x1000 },
};

static struct sprd_clk_desc udx710_ipa_gate_desc = {
	.clk_clks	= udx710_ipa_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(udx710_ipa_gate_clks),
	.hw_clks	= &udx710_ipa_gate_clk_hws,
	.resets		= udx710_ipa_resets,
	.num_resets	= ARRAY_SIZE(udx710_ipa_resets),
};

/* audcp apb gates */
static SPRD_SC_GATE_CLK_HW(audcp_wdg_eb, "audcp-wdg-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(1), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_rtc_wdg_eb, "audcp-rtc-wdg-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(2), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_tmr0_eb, "audcp-tmr0-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(5), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_tmr1_eb, "audcp-tmr1-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(6), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);

static struct sprd_clk_common *udx710_audcpapb_gate[] = {
	/* address base is 0x3350d000 */
	&audcp_wdg_eb.common,
	&audcp_rtc_wdg_eb.common,
	&audcp_tmr0_eb.common,
	&audcp_tmr1_eb.common,
};

static struct clk_hw_onecell_data udx710_audcpapb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_WDG_EB]	= &audcp_wdg_eb.common.hw,
		[CLK_AUDCP_RTC_WDG_EB]	= &audcp_rtc_wdg_eb.common.hw,
		[CLK_AUDCP_TMR0_EB]	= &audcp_tmr0_eb.common.hw,
		[CLK_AUDCP_TMR1_EB]	= &audcp_tmr1_eb.common.hw,
	},
	.num	= CLK_AUDCP_APB_GATE_NUM,
};

static struct sprd_reset_map udx710_audcpapb_resets[] = {
	[RESET_AUDCP_APB_WDG_SOFT_RST]		= { 0x0004, BIT(1), 0x100 },
	[RESET_AUDCP_APB_TMR0_SOFT_RST]		= { 0x0004, BIT(2), 0x100 },
	[RESET_AUDCP_APB_TMR1_SOFT_RST]		= { 0x0004, BIT(3), 0x100 },
};

static const struct sprd_clk_desc udx710_audcpapb_gate_desc = {
	.clk_clks	= udx710_audcpapb_gate,
	.num_clk_clks	= ARRAY_SIZE(udx710_audcpapb_gate),
	.hw_clks	= &udx710_audcpapb_gate_hws,
	.resets		= udx710_audcpapb_resets,
	.num_resets	= ARRAY_SIZE(udx710_audcpapb_resets),
};

/* audcp ahb gates */
static SPRD_SC_GATE_CLK_HW(audcp_iis0_eb, "audcp-iis0-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(0), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_iis1_eb, "audcp-iis1-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(1), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_iis2_eb, "audcp-iis2-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(2), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_iis3_eb, "audcp-iis3-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(3), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_uart_eb, "audcp-uart-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(4), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_dma_cp_eb, "audcp-dma-cp-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(5), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_dma_ap_eb, "audcp-dma-ap-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(6), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_src48k_eb, "audcp-src48k-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(10), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_mcdt_eb,  "audcp-mcdt-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(12), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vbcifd_eb, "audcp-vbcifd-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(13), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vbc_eb,   "audcp-vbc-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(14), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_splk_eb,  "audcp-splk-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(15), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_icu_eb,   "audcp-icu-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(16), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(dma_ap_ashb_eb, "dma-ap-ashb-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(17), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(dma_cp_ashb_eb, "dma-cp-ashb-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(18), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_aud_eb,   "audcp-aud-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(19), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_vbc_24m_eb, "audcp-vbc-24m-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(21), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_tmr_26m_eb, "audcp-tmr-26m-eb", &access_aud_en.common.hw, 0x0,
		     0x100, BIT(22), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);
static SPRD_SC_GATE_CLK_HW(audcp_dvfs_ashb_eb, "audcp-dvfs-ashb-eb", &access_aud_en.common.hw,
		     0x0, 0x100, BIT(23), CLK_IGNORE_UNUSED, SPRD_GATE_NON_AON);

static struct sprd_clk_common *udx710_audcpahb_gate[] = {
	/* address base is 0x335e0000 */
	&audcp_iis0_eb.common,
	&audcp_iis1_eb.common,
	&audcp_iis2_eb.common,
	&audcp_iis3_eb.common,
	&audcp_uart_eb.common,
	&audcp_dma_cp_eb.common,
	&audcp_dma_ap_eb.common,
	&audcp_src48k_eb.common,
	&audcp_mcdt_eb.common,
	&audcp_vbcifd_eb.common,
	&audcp_vbc_eb.common,
	&audcp_splk_eb.common,
	&audcp_icu_eb.common,
	&dma_ap_ashb_eb.common,
	&dma_cp_ashb_eb.common,
	&audcp_aud_eb.common,
	&audcp_vbc_24m_eb.common,
	&audcp_tmr_26m_eb.common,
	&audcp_dvfs_ashb_eb.common,
};

static struct clk_hw_onecell_data udx710_audcpahb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_IIS0_EB]		= &audcp_iis0_eb.common.hw,
		[CLK_AUDCP_IIS1_EB]		= &audcp_iis1_eb.common.hw,
		[CLK_AUDCP_IIS2_EB]		= &audcp_iis2_eb.common.hw,
		[CLK_AUDCP_IIS3_EB]		= &audcp_iis3_eb.common.hw,
		[CLK_AUDCP_UART_EB]		= &audcp_uart_eb.common.hw,
		[CLK_AUDCP_DMA_CP_EB]		= &audcp_dma_cp_eb.common.hw,
		[CLK_AUDCP_DMA_AP_EB]		= &audcp_dma_ap_eb.common.hw,
		[CLK_AUDCP_SRC48K_EB]		= &audcp_src48k_eb.common.hw,
		[CLK_AUDCP_MCDT_EB]		= &audcp_mcdt_eb.common.hw,
		[CLK_AUDCP_VBCIFD_EB]		= &audcp_vbcifd_eb.common.hw,
		[CLK_AUDCP_VBC_EB]		= &audcp_vbc_eb.common.hw,
		[CLK_AUDCP_SPLK_EB]		= &audcp_splk_eb.common.hw,
		[CLK_AUDCP_ICU_EB]		= &audcp_icu_eb.common.hw,
		[CLK_AUDCP_DMA_AP_ASHB_EB]	= &dma_ap_ashb_eb.common.hw,
		[CLK_AUDCP_DMA_CP_ASHB_EB]	= &dma_cp_ashb_eb.common.hw,
		[CLK_AUDCP_AUD_EB]		= &audcp_aud_eb.common.hw,
		[CLK_AUDCP_VBC_24M_EB]		= &audcp_vbc_24m_eb.common.hw,
		[CLK_AUDCP_TMR_26M_EB]		= &audcp_tmr_26m_eb.common.hw,
		[CLK_AUDCP_DVFS_ASHB_EB]	= &audcp_dvfs_ashb_eb.common.hw,
	},
	.num	= CLK_AUDCP_AHB_GATE_NUM,
};

static struct sprd_reset_map udx710_audcpahb_resets[] = {
	[RESET_AUDCP_AHB_VBC_24M_SOFT_RST]		= { 0x0008, BIT(0), 0x100 },
	[RESET_AUDCP_AHB_DMA_AP_SOFT_RST]		= { 0x0008, BIT(1), 0x100 },
	[RESET_AUDCP_AHB_SRC48K_SOFT_RST]		= { 0x0008, BIT(5), 0x100 },
	[RESET_AUDCP_AHB_MCDT_SOFT_RST]			= { 0x0008, BIT(7), 0x100 },
	[RESET_AUDCP_AHB_VBCIFD_SOFT_RST]		= { 0x0008, BIT(8), 0x100 },
	[RESET_AUDCP_AHB_VBC_SOFT_RST]			= { 0x0008, BIT(9), 0x100 },
	[RESET_AUDCP_AHB_SPINLOCK_SOFT_RST]		= { 0x0008, BIT(10), 0x100 },
	[RESET_AUDCP_AHB_DMA_CP_SOFT_RST]		= { 0x0008, BIT(11), 0x100 },
	[RESET_AUDCP_AHB_IIS0_SOFT_RST]			= { 0x0008, BIT(12), 0x100 },
	[RESET_AUDCP_AHB_IIS1_SOFT_RST]			= { 0x0008, BIT(13), 0x100 },
	[RESET_AUDCP_AHB_IIS2_SOFT_RST]			= { 0x0008, BIT(14), 0x100 },
	[RESET_AUDCP_AHB_UART_SOFT_RST]			= { 0x0008, BIT(16), 0x100 },
	[RESET_AUDCP_AHB_AUD_SOFT_RST]			= { 0x0008, BIT(25), 0x100 },
	[RESET_AUDCP_AHB_DVFS_SOFT_RST]			= { 0x0008, BIT(26), 0x100 },
};

static const struct sprd_clk_desc udx710_audcpahb_gate_desc = {
	.clk_clks	= udx710_audcpahb_gate,
	.num_clk_clks	= ARRAY_SIZE(udx710_audcpahb_gate),
	.hw_clks	= &udx710_audcpahb_gate_hws,
	.resets		= udx710_audcpahb_resets,
	.num_resets	= ARRAY_SIZE(udx710_audcpahb_resets),
};

/* sp ahb gates */
static SPRD_SC_GATE_CLK_FW_NAME(sp_uart0_eb, "sp-uart0-eb", "ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *udx710_spahb_gate[] = {
	/* address base is 0x62010000 */
	&sp_uart0_eb.common,
};

static struct clk_hw_onecell_data udx710_spahb_gate_hws = {
	.hws    = {
		[CLK_SP_UART0_EB] = &sp_uart0_eb.common.hw,
	},
	.num	= CLK_SP_AHB_GATE_NUM,
};

static struct sprd_reset_map udx710_spahb_resets[] = {
	[RESET_SP_AHB_ARCH_SOFT_RST]			= { 0x0004, BIT(0), 0x1000 },
	[RESET_SP_AHB_IMC_SOFT_RST]			= { 0x0004, BIT(2), 0x1000 },
	[RESET_SP_AHB_EIC_SOFT_RST]			= { 0x0004, BIT(5), 0x1000 },
	[RESET_SP_AHB_WDG_SOFT_RST]			= { 0x0004, BIT(6), 0x1000 },
	[RESET_SP_AHB_SYST_SOFT_RST]			= { 0x0004, BIT(7), 0x1000 },
	[RESET_SP_AHB_TMR_SOFT_RST]			= { 0x0004, BIT(8), 0x1000 },
	[RESET_SP_AHB_UART0_SOFT_RST]			= { 0x0004, BIT(9), 0x1000 },
	[RESET_SP_AHB_EIC1_SOFT_RST]			= { 0x0004, BIT(10), 0x1000 },
};

static const struct sprd_clk_desc udx710_spahb_gate_desc = {
	.clk_clks	= udx710_spahb_gate,
	.num_clk_clks	= ARRAY_SIZE(udx710_spahb_gate),
	.hw_clks	= &udx710_spahb_gate_hws,
	.resets		= udx710_spahb_resets,
	.num_resets	= ARRAY_SIZE(udx710_spahb_resets),
};

static const struct of_device_id sprd_udx710_clk_ids[] = {
	{ .compatible = "sprd,udx710-apahb-gate",		/* 0x21000000 */
	  .data = &udx710_apahb_gate_desc },
	{ .compatible = "sprd,udx710-ap-clk",			/* 0x21100000 */
	  .data = &udx710_ap_clk_desc },
	{ .compatible = "sprd,udx710-apapb-gate",		/* 0x24000000 */
	  .data = &udx710_apapb_gate_desc },
	{ .compatible = "sprd,udx710-ipa-gate",			/* 0x29000000 */
	  .data = &udx710_ipa_gate_desc },
	{ .compatible = "sprd,udx710-audcpapb-gate",		/* 0x3350d000 */
	  .data = &udx710_audcpapb_gate_desc },
	{ .compatible = "sprd,udx710-audcpahb-gate",		/* 0x335e0000 */
	  .data = &udx710_audcpahb_gate_desc },
	{ .compatible = "sprd,udx710-aon-clk",			/* 0x63170000 */
	  .data = &udx710_aon_clk_desc },
	{ .compatible = "sprd,udx710-g3-pll",			/* 0x634b0000 */
	  .data = &udx710_g3_pll_desc },
	{ .compatible = "sprd,udx710-apcpu-clk",		/* 0x63970000 */
	  .data = &udx710_apcpu_clk_desc },
	{ .compatible = "sprd,udx710-pmu-gate",			/* 0x64010000 */
	  .data = &udx710_pmu_gate_desc },
	{ .compatible = "sprd,udx710-aon-gate",			/* 0x64020000 */
	  .data = &udx710_aon_gate_desc },
	{ .compatible = "sprd,udx710-spahb-gate",		/* 0x62010000 */
	  .data = &udx710_spahb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_udx710_clk_ids);

static int udx710_clk_probe(struct platform_device *pdev)
{
	const struct sprd_clk_desc *desc;
	struct sprd_reset *reset;
	int ret;

	desc = device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

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

static struct platform_driver udx710_clk_driver = {
	.probe	= udx710_clk_probe,
	.driver	= {
		.name = "udx710-clk",
		.of_match_table = sprd_udx710_clk_ids,
	},
};
module_platform_driver(udx710_clk_driver);

MODULE_DESCRIPTION("Spreadtrum UDX710 Clock Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:udx710-clk");
