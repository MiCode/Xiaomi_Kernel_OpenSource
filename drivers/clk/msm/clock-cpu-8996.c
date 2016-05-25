/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/cpu.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>

#include <asm/cputype.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/kryo-l2-accessors.h>

#include <dt-bindings/clock/msm-clocks-8996.h>

#include "clock.h"
#include "vdd-level-8994.h"

enum {
	APC0_PLL_BASE,
	APC1_PLL_BASE,
	CBF_PLL_BASE,
	APC0_BASE,
	APC1_BASE,
	CBF_BASE,
	EFUSE_BASE,
	DEBUG_BASE,
	NUM_BASES
};

static char *base_names[] = {
	"pwrcl_pll",
	"perfcl_pll",
	"cbf_pll",
	"pwrcl_mux",
	"perfcl_mux",
	"cbf_mux",
	"efuse",
	"debug",
};

static void *vbases[NUM_BASES];
static bool cpu_clocks_v3;
static bool cpu_clocks_pro;

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

/* Power cluster primary PLL */
#define C0_PLL_MODE         0x0
#define C0_PLL_L_VAL        0x4
#define C0_PLL_ALPHA        0x8
#define C0_PLL_USER_CTL    0x10
#define C0_PLL_CONFIG_CTL  0x18
#define C0_PLL_CONFIG_CTL_HI 0x1C
#define C0_PLL_STATUS      0x28
#define C0_PLL_TEST_CTL_LO 0x20
#define C0_PLL_TEST_CTL_HI 0x24

/* Power cluster alt PLL */
#define C0_PLLA_MODE        0x100
#define C0_PLLA_L_VAL       0x104
#define C0_PLLA_ALPHA       0x108
#define C0_PLLA_USER_CTL    0x110
#define C0_PLLA_CONFIG_CTL  0x118
#define C0_PLLA_STATUS      0x128
#define C0_PLLA_TEST_CTL_LO 0x120

/* Perf cluster primary PLL */
#define C1_PLL_MODE         0x0
#define C1_PLL_L_VAL        0x4
#define C1_PLL_ALPHA        0x8
#define C1_PLL_USER_CTL    0x10
#define C1_PLL_CONFIG_CTL  0x18
#define C1_PLL_CONFIG_CTL_HI 0x1C
#define C1_PLL_STATUS      0x28
#define C1_PLL_TEST_CTL_LO 0x20
#define C1_PLL_TEST_CTL_HI 0x24

/* Perf cluster alt PLL */
#define C1_PLLA_MODE        0x100
#define C1_PLLA_L_VAL       0x104
#define C1_PLLA_ALPHA       0x108
#define C1_PLLA_USER_CTL    0x110
#define C1_PLLA_CONFIG_CTL  0x118
#define C1_PLLA_STATUS      0x128
#define C1_PLLA_TEST_CTL_LO 0x120

#define CBF_PLL_MODE         0x0
#define CBF_PLL_L_VAL        0x8
#define CBF_PLL_ALPHA       0x10
#define CBF_PLL_USER_CTL    0x18
#define CBF_PLL_CONFIG_CTL  0x20
#define CBF_PLL_CONFIG_CTL_HI 0x24
#define CBF_PLL_STATUS      0x28
#define CBF_PLL_TEST_CTL_LO 0x30
#define CBF_PLL_TEST_CTL_HI 0x34

#define APC_DIAG_OFFSET	0x48
#define MUX_OFFSET	0x40

#define MDD_DROOP_CODE	0x7C

DEFINE_EXT_CLK(xo_ao, NULL);
DEFINE_CLK_DUMMY(alpha_xo_ao, 19200000);
DEFINE_EXT_CLK(sys_apcsaux_clk_gcc, NULL);
DEFINE_FIXED_SLAVE_DIV_CLK(sys_apcsaux_clk, 2, &sys_apcsaux_clk_gcc.c);

#define L2ACDCR_REG 0x580ULL
#define L2ACDTD_REG 0x581ULL
#define L2ACDDVMRC_REG 0x584ULL
#define L2ACDSSCR_REG 0x589ULL

#define EFUSE_SHIFT	29
#define EFUSE_MASK	0x7

/* ACD static settings */
static int acdtd_val_pwrcl = 0x00006A11;
static int acdtd_val_perfcl = 0x00006A11;
static int dvmrc_val = 0x000E0F0F;
static int acdsscr_val = 0x00000601;
static int acdcr_val_pwrcl = 0x002C5FFD;
module_param(acdcr_val_pwrcl, int, 0444);
static int acdcr_val_perfcl = 0x002C5FFD;
module_param(acdcr_val_perfcl, int, 0444);
int enable_acd = 1;
module_param(enable_acd, int, 0444);

#define WRITE_L2ACDCR(val) \
	set_l2_indirect_reg(L2ACDCR_REG, (val))
#define WRITE_L2ACDTD(val) \
	set_l2_indirect_reg(L2ACDTD_REG, (val))
#define WRITE_L2ACDDVMRC(val) \
	set_l2_indirect_reg(L2ACDDVMRC_REG, (val))
#define WRITE_L2ACDSSCR(val) \
	set_l2_indirect_reg(L2ACDSSCR_REG, (val))

#define READ_L2ACDCR(reg) \
	(reg = get_l2_indirect_reg(L2ACDCR_REG))
#define READ_L2ACDTD(reg) \
	(reg = get_l2_indirect_reg(L2ACDTD_REG))
#define READ_L2ACDDVMRC(reg) \
	(reg = get_l2_indirect_reg(L2ACDDVMRC_REG))
#define READ_L2ACDSSCR(reg) \
	(reg = get_l2_indirect_reg(L2ACDSSCR_REG))

static struct pll_clk perfcl_pll = {
	.mode_reg = (void __iomem *)C1_PLL_MODE,
	.l_reg = (void __iomem *)C1_PLL_L_VAL,
	.alpha_reg = (void __iomem *)C1_PLL_ALPHA,
	.config_reg = (void __iomem *)C1_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)C1_PLL_CONFIG_CTL,
	.config_ctl_hi_reg = (void __iomem *)C1_PLL_CONFIG_CTL_HI,
	.status_reg = (void __iomem *)C1_PLL_MODE,
	.test_ctl_lo_reg = (void __iomem *)C1_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)C1_PLL_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.init_test_ctl = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.post_div_masked = 0x100,
		.pre_div_masked = 0x0,
		.test_ctl_hi_val = 0x00004000,
		.test_ctl_lo_val = 0x04000000,
		.config_ctl_val = 0x200D4AA8,
		.config_ctl_hi_val = 0x002,
	},
	.min_rate =  600000000,
	.max_rate = 3000000000,
	.src_rate = 19200000,
	.base = &vbases[APC1_PLL_BASE],
	.c = {
		.always_on = true,
		.parent = &xo_ao.c,
		.dbg_name = "perfcl_pll",
		.ops = &clk_ops_variable_rate_pll_hwfsm,
		VDD_DIG_FMAX_MAP1(LOW, 3000000000),
		CLK_INIT(perfcl_pll.c),
	},
};

static struct alpha_pll_masks alt_pll_masks = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
	.post_div_mask = 0xf00,
};

static struct alpha_pll_vco_tbl alt_pll_vco_modes[] = {
	VCO(3,  250000000,  500000000),
	VCO(2,  500000000,  750000000),
	VCO(1,  750000000, 1000000000),
	VCO(0, 1000000000, 2150400000),
};

static struct alpha_pll_clk perfcl_alt_pll = {
	.masks = &alt_pll_masks,
	.base = &vbases[APC1_PLL_BASE],
	.offset = C1_PLLA_MODE,
	.vco_tbl = alt_pll_vco_modes,
	.num_vco = ARRAY_SIZE(alt_pll_vco_modes),
	.enable_config = 0x9, /* Main and early outputs */
	.post_div_config = 0x100, /* Div-2 */
	.config_ctl_val = 0x4001051B,
	.offline_bit_workaround = true,
	.no_irq_dis = true,
	.c = {
		.always_on = true,
		.parent = &alpha_xo_ao.c,
		.dbg_name = "perfcl_alt_pll",
		.ops = &clk_ops_alpha_pll_hwfsm,
		CLK_INIT(perfcl_alt_pll.c),
	},
};

static struct pll_clk pwrcl_pll = {
	.mode_reg = (void __iomem *)C0_PLL_MODE,
	.l_reg = (void __iomem *)C0_PLL_L_VAL,
	.alpha_reg = (void __iomem *)C0_PLL_ALPHA,
	.config_reg = (void __iomem *)C0_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)C0_PLL_CONFIG_CTL,
	.config_ctl_hi_reg = (void __iomem *)C0_PLL_CONFIG_CTL_HI,
	.status_reg = (void __iomem *)C0_PLL_MODE,
	.test_ctl_lo_reg = (void __iomem *)C0_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)C0_PLL_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.init_test_ctl = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.post_div_masked = 0x100,
		.pre_div_masked = 0x0,
		.test_ctl_hi_val = 0x00004000,
		.test_ctl_lo_val = 0x04000000,
		.config_ctl_val = 0x200D4AA8,
		.config_ctl_hi_val = 0x002,
	},
	.min_rate =  600000000,
	.max_rate = 3000000000,
	.src_rate = 19200000,
	.base = &vbases[APC0_PLL_BASE],
	.c = {
		.always_on = true,
		.parent = &xo_ao.c,
		.dbg_name = "pwrcl_pll",
		.ops = &clk_ops_variable_rate_pll_hwfsm,
		VDD_DIG_FMAX_MAP1(LOW, 3000000000),
		CLK_INIT(pwrcl_pll.c),
	},
};

static struct alpha_pll_clk pwrcl_alt_pll = {
	.masks = &alt_pll_masks,
	.base = &vbases[APC0_PLL_BASE],
	.offset = C0_PLLA_MODE,
	.vco_tbl = alt_pll_vco_modes,
	.num_vco = ARRAY_SIZE(alt_pll_vco_modes),
	.enable_config = 0x9, /* Main and early outputs */
	.post_div_config = 0x100, /* Div-2 */
	.config_ctl_val = 0x4001051B,
	.offline_bit_workaround = true,
	.no_irq_dis = true,
	.c = {
		.always_on = true,
		.dbg_name = "pwrcl_alt_pll",
		.parent = &alpha_xo_ao.c,
		.ops = &clk_ops_alpha_pll_hwfsm,
		CLK_INIT(pwrcl_alt_pll.c),
	},
};

static DEFINE_SPINLOCK(mux_reg_lock);

DEFINE_FIXED_DIV_CLK(pwrcl_pll_main, 2, &pwrcl_pll.c);
DEFINE_FIXED_DIV_CLK(perfcl_pll_main, 2, &perfcl_pll.c);

static void __cpu_mux_set_sel(struct mux_clk *mux, int sel)
{
	u32 regval;
	unsigned long flags;

	spin_lock_irqsave(&mux_reg_lock, flags);

	if (mux->priv)
		regval = scm_io_read(*(u32 *)mux->priv + mux->offset);
	else
		regval = readl_relaxed(*mux->base + mux->offset);

	regval &= ~(mux->mask << mux->shift);
	regval |= (sel & mux->mask) << mux->shift;

	if (mux->priv)
		scm_io_write(*(u32 *)mux->priv + mux->offset, regval);
	else
		writel_relaxed(regval, *mux->base + mux->offset);

	spin_unlock_irqrestore(&mux_reg_lock, flags);

	/* Ensure switch request goes through before returning */
	mb();
	/* Hardware mandated delay */
	udelay(5);
}

/* It is assumed that the mux enable state is locked in this function */
static int cpu_mux_set_sel(struct mux_clk *mux, int sel)
{
	__cpu_mux_set_sel(mux, sel);

	return 0;
}

static int cpu_mux_get_sel(struct mux_clk *mux)
{
	u32 regval;

	if (mux->priv)
		regval = scm_io_read(*(u32 *)mux->priv + mux->offset);
	else
		regval = readl_relaxed(*mux->base + mux->offset);
	return (regval >> mux->shift) & mux->mask;
}

static int cpu_mux_enable(struct mux_clk *mux)
{
	return 0;
}

static void cpu_mux_disable(struct mux_clk *mux)
{
}

/* It is assumed that the mux enable state is locked in this function */
static int cpu_debug_mux_set_sel(struct mux_clk *mux, int sel)
{
	__cpu_mux_set_sel(mux, sel);

	return 0;
}

static int cpu_debug_mux_get_sel(struct mux_clk *mux)
{
	u32 regval = readl_relaxed(*mux->base + mux->offset);
	return (regval >> mux->shift) & mux->mask;
}

static int cpu_debug_mux_enable(struct mux_clk *mux)
{
	u32 val;

	/* Enable debug clocks */
	val = readl_relaxed(vbases[APC0_BASE] + APC_DIAG_OFFSET);
	val |= BM(11, 8);
	writel_relaxed(val, vbases[APC0_BASE] + APC_DIAG_OFFSET);

	val = readl_relaxed(vbases[APC1_BASE] + APC_DIAG_OFFSET);
	val |= BM(11, 8);
	writel_relaxed(val, vbases[APC1_BASE] + APC_DIAG_OFFSET);

	/* Ensure enable request goes through for correct measurement*/
	mb();
	udelay(5);
	return 0;
}

static void cpu_debug_mux_disable(struct mux_clk *mux)
{
	u32 val;

	/* Disable debug clocks */
	val = readl_relaxed(vbases[APC0_BASE] + APC_DIAG_OFFSET);
	val &= ~BM(11, 8);
	writel_relaxed(val, vbases[APC0_BASE] + APC_DIAG_OFFSET);

	val = readl_relaxed(vbases[APC1_BASE] + APC_DIAG_OFFSET);
	val &= ~BM(11, 8);
	writel_relaxed(val, vbases[APC1_BASE] + APC_DIAG_OFFSET);
}

static struct clk_mux_ops cpu_mux_ops = {
	.enable = cpu_mux_enable,
	.disable = cpu_mux_disable,
	.set_mux_sel = cpu_mux_set_sel,
	.get_mux_sel = cpu_mux_get_sel,
};

static struct clk_mux_ops cpu_debug_mux_ops = {
	.enable = cpu_debug_mux_enable,
	.disable = cpu_debug_mux_disable,
	.set_mux_sel = cpu_debug_mux_set_sel,
	.get_mux_sel = cpu_debug_mux_get_sel,
};

static struct mux_clk pwrcl_lf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &pwrcl_pll_main.c,  1 },
		{ &sys_apcsaux_clk.c, 3 },
	),
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 2,
	.base = &vbases[APC0_BASE],
	.c = {
		.dbg_name = "pwrcl_lf_mux",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(pwrcl_lf_mux.c),
	},
};

static struct mux_clk pwrcl_hf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		/* This may be changed by acd_init to select the ACD leg */
		{ &pwrcl_pll.c,     1 },
		{ &pwrcl_lf_mux.c,  0 },
	),
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 0,
	.base = &vbases[APC0_BASE],
	.c = {
		.dbg_name = "pwrcl_hf_mux",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(pwrcl_hf_mux.c),
	},
};

static struct mux_clk perfcl_lf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &perfcl_pll_main.c, 1 },
		{ &sys_apcsaux_clk.c, 3 },
	),
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 2,
	.base = &vbases[APC1_BASE],
	.c = {
		.dbg_name = "perfcl_lf_mux",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(perfcl_lf_mux.c),
	},
};

static struct mux_clk perfcl_hf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		/* This may be changed by acd_init to select the ACD leg */
		{ &perfcl_pll.c,     1 },
		{ &perfcl_lf_mux.c,  0 },
	),
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 0,
	.base = &vbases[APC1_BASE],
	.c = {
		.dbg_name = "perfcl_hf_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(perfcl_hf_mux.c),
	},
};

static struct clk_src clk_src_perfcl_hf_mux_alt[] =  {
		{ &perfcl_alt_pll.c, 3 },
		{ &perfcl_lf_mux.c,  0 },
	};

static struct clk_src clk_src_pwrcl_hf_mux_alt[] =  {
		{ &pwrcl_alt_pll.c, 3 },
		{ &pwrcl_lf_mux.c,  0 },
	};

static struct clk_src clk_src_perfcl_lf_mux_alt[] =  {
		{ &sys_apcsaux_clk.c, 3 },
	};

static struct clk_src clk_src_pwrcl_lf_mux_alt[] =  {
		{ &sys_apcsaux_clk.c, 3 },
	};

struct cpu_clk_8996 {
	u32 cpu_reg_mask;
	struct clk *alt_pll;
	unsigned long *alt_pll_freqs;
	unsigned long alt_pll_thresh;
	int n_alt_pll_freqs;
	struct clk c;
	bool hw_low_power_ctrl;
	int pm_qos_latency;
	cpumask_t cpumask;
	struct pm_qos_request req;
	bool do_half_rate;
	bool has_acd;
	int postdiv;
};

static inline struct cpu_clk_8996 *to_cpu_clk_8996(struct clk *c)
{
	return container_of(c, struct cpu_clk_8996, c);
}

static enum handoff cpu_clk_8996_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);

	return HANDOFF_DISABLED_CLK;
}

static long cpu_clk_8996_round_rate(struct clk *c, unsigned long rate)
{
	int i;

	for (i = 0; i < c->num_fmax; i++)
		if (rate <= c->fmax[i])
			return clk_round_rate(c->parent, c->fmax[i]);

	return clk_round_rate(c->parent, c->fmax[c->num_fmax - 1]);
}

static unsigned long alt_pll_perfcl_freqs[] = {
	 307200000,
	 556800000,
};

static void do_nothing(void *unused) { }

/* ACD programming */
static struct cpu_clk_8996 perfcl_clk;
static struct cpu_clk_8996 pwrcl_clk;

static void cpu_clock_8996_acd_init(void);

static void cpu_clk_8996_disable(struct clk *c)
{
	struct cpu_clk_8996 *cpuclk = to_cpu_clk_8996(c);

	if (!enable_acd)
		return;

	/* Ensure that we switch to GPLL0 across D5 */
	if (cpuclk == &pwrcl_clk)
		writel_relaxed(0x3C, vbases[APC0_BASE] + MUX_OFFSET);
	else if (cpuclk == &perfcl_clk)
		writel_relaxed(0x3C, vbases[APC1_BASE] + MUX_OFFSET);
}

#define MAX_PLL_MAIN_FREQ 595200000

/*
 * Returns the max safe frequency that will guarantee we switch to main output
 */
unsigned long acd_safe_freq(unsigned long freq)
{
	/*
	 * If we're running at less than double the max PLL main rate,
	 * just return half the rate. This will ensure we switch to
	 * the main output, without violating voltage constraints
	 * that might happen if we choose to go with MAX_PLL_MAIN_FREQ.
	 */
	if (freq > MAX_PLL_MAIN_FREQ && freq <= MAX_PLL_MAIN_FREQ*2)
		return freq/2;

	/*
	 * We're higher than the max main output, and higher than twice
	 * the max main output. Safe to go to the max main output.
	 */
	if (freq > MAX_PLL_MAIN_FREQ)
		return MAX_PLL_MAIN_FREQ;

	/* Shouldn't get here, just return the safest rate possible */
	return sys_apcsaux_clk.c.rate;
}

static int cpu_clk_8996_pre_set_rate(struct clk *c, unsigned long rate)
{
	struct cpu_clk_8996 *cpuclk = to_cpu_clk_8996(c);
	int ret;
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;
	bool on_acd_leg = c->rate > MAX_PLL_MAIN_FREQ;
	bool increase_freq = rate > c->rate;

	/*
	 * If hardware control of the clock tree is enabled during power
	 * collapse, setup a PM QOS request to prevent power collapse and
	 * wake up one of the CPUs in this clock domain, to ensure software
	 * control while the clock rate is being switched.
	 */
	if (hw_low_power_ctrl) {
		memset(&cpuclk->req, 0, sizeof(cpuclk->req));
		cpuclk->req.cpus_affine = cpuclk->cpumask;
		cpuclk->req.type = PM_QOS_REQ_AFFINE_CORES;
		pm_qos_add_request(&cpuclk->req, PM_QOS_CPU_DMA_LATENCY,
				   cpuclk->pm_qos_latency);

		ret = smp_call_function_any(&cpuclk->cpumask, do_nothing,
						NULL, 1);
	}

	/* Select a non-ACD'ed safe source across voltage and freq switches */
	if (enable_acd && cpuclk->has_acd && on_acd_leg && increase_freq) {
		/*
		 * We're on the ACD leg, and the voltage will be
		 * scaled before clk_set_rate. Switch to the main output.
		 */
		return clk_set_rate(c->parent, acd_safe_freq(c->rate));
	}

	return 0;
}

static void cpu_clk_8996_post_set_rate(struct clk *c, unsigned long start_rate)
{
	struct cpu_clk_8996 *cpuclk = to_cpu_clk_8996(c);
	int ret;
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;
	bool on_acd_leg = c->rate > MAX_PLL_MAIN_FREQ;
	bool decrease_freq = start_rate > c->rate;

	if (cpuclk->has_acd && enable_acd && on_acd_leg && decrease_freq) {
		ret = clk_set_rate(c->parent, c->rate);
		if (ret)
			pr_err("Unable to reset parent rate!\n");
	}

	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);
}

static int cpu_clk_8996_set_rate(struct clk *c, unsigned long rate)
{
	struct cpu_clk_8996 *cpuclk = to_cpu_clk_8996(c);
	int ret, err_ret;
	unsigned long alt_pll_prev_rate;
	unsigned long alt_pll_rate;
	unsigned long n_alt_freqs = cpuclk->n_alt_pll_freqs;
	bool on_acd_leg = rate > MAX_PLL_MAIN_FREQ;
	bool decrease_freq = rate < c->rate;

	if (cpuclk->alt_pll && (n_alt_freqs > 0)) {
		alt_pll_prev_rate = cpuclk->alt_pll->rate;
		alt_pll_rate = cpuclk->alt_pll_freqs[0];
		if (rate > cpuclk->alt_pll_thresh)
			alt_pll_rate = cpuclk->alt_pll_freqs[1];
		if (!cpu_clocks_v3)
			mutex_lock(&scm_lmh_lock);
		ret = clk_set_rate(cpuclk->alt_pll, alt_pll_rate);
		if (!cpu_clocks_v3)
			mutex_unlock(&scm_lmh_lock);
		if (ret) {
			pr_err("failed to set rate %lu on alt_pll when setting %lu on %s (%d)\n",
			alt_pll_rate, rate, c->dbg_name, ret);
			goto out;
		}
	}

	/*
	 * Special handling needed for the case when using the div-2 output.
	 * Since the PLL needs to be running at twice the requested rate,
	 * we need to switch the mux first and then change the PLL rate.
	 * Otherwise the current voltage may not suffice with the PLL running at
	 * 2 * rate. Example: switching from 768MHz down to 550Mhz - if we raise
	 * the PLL to 1.1GHz, that's undervolting the system. So, we switch to
	 * the div-2 mux select first (by requesting rate/2), allowing the CPU
	 * to run at 384MHz. Then, we ramp up the PLL to 1.1GHz, allowing the
	 * CPU frequency to ramp up from 384MHz to 550MHz.
	 */
	if (cpuclk->do_half_rate
		&& c->rate > 600000000 && rate < 600000000) {
		if (!cpu_clocks_v3)
			mutex_lock(&scm_lmh_lock);
		ret = clk_set_rate(c->parent, c->rate/cpuclk->postdiv);
		if (!cpu_clocks_v3)
			mutex_unlock(&scm_lmh_lock);
		if (ret) {
			pr_err("failed to set rate %lu on %s (%d)\n",
				c->rate/cpuclk->postdiv, c->dbg_name, ret);
			goto fail;
		}
	}

	if (!cpu_clocks_v3)
		mutex_lock(&scm_lmh_lock);
	ret = clk_set_rate(c->parent, rate);
	if (!cpu_clocks_v3)
		mutex_unlock(&scm_lmh_lock);
	if (ret) {
		pr_err("failed to set rate %lu on %s (%d)\n",
			rate, c->dbg_name, ret);
		goto set_rate_fail;
	}

	/*
	 * If we're on the ACD leg and decreasing freq, voltage will be changed
	 * after this function. Switch to main output.
	 */
	if (enable_acd && cpuclk->has_acd && decrease_freq && on_acd_leg)
		return clk_set_rate(c->parent, acd_safe_freq(c->rate));

	return 0;

set_rate_fail:
	/* Restore parent rate if we halved it */
	if (cpuclk->do_half_rate && c->rate > 600000000 && rate < 600000000) {
		if (!cpu_clocks_v3)
			mutex_lock(&scm_lmh_lock);
		err_ret = clk_set_rate(c->parent, c->rate);
		if (!cpu_clocks_v3)
			mutex_unlock(&scm_lmh_lock);
		if (err_ret)
			pr_err("failed to restore %s rate to %lu\n",
			       c->dbg_name, c->rate);
	}

fail:
	if (cpuclk->alt_pll && (n_alt_freqs > 0)) {
		if (!cpu_clocks_v3)
			mutex_lock(&scm_lmh_lock);
		err_ret = clk_set_rate(cpuclk->alt_pll, alt_pll_prev_rate);
		if (!cpu_clocks_v3)
			mutex_unlock(&scm_lmh_lock);
		if (err_ret)
			pr_err("failed to reset rate to %lu on alt pll after failing to  set %lu on %s (%d)\n",
				alt_pll_prev_rate, rate, c->dbg_name, err_ret);
	}
out:
	return ret;
}

static struct clk_ops clk_ops_cpu_8996 = {
	.disable = cpu_clk_8996_disable,
	.set_rate = cpu_clk_8996_set_rate,
	.pre_set_rate = cpu_clk_8996_pre_set_rate,
	.post_set_rate = cpu_clk_8996_post_set_rate,
	.round_rate = cpu_clk_8996_round_rate,
	.handoff = cpu_clk_8996_handoff,
};

DEFINE_VDD_REGS_INIT(vdd_pwrcl, 1);

#define PERFCL_LATENCY_NO_L2_PC_US (1)
#define PWRCL_LATENCY_NO_L2_PC_US (1)

static struct cpu_clk_8996 pwrcl_clk = {
	.cpu_reg_mask = 0x3,
	.pm_qos_latency = PWRCL_LATENCY_NO_L2_PC_US,
	.do_half_rate = true,
	.postdiv = 2,
	.c = {
		.parent = &pwrcl_hf_mux.c,
		.dbg_name = "pwrcl_clk",
		.ops = &clk_ops_cpu_8996,
		.vdd_class = &vdd_pwrcl,
		CLK_INIT(pwrcl_clk.c),
	},
};

DEFINE_VDD_REGS_INIT(vdd_perfcl, 1);

static struct cpu_clk_8996 perfcl_clk = {
	.cpu_reg_mask = 0x103,
	.alt_pll = &perfcl_alt_pll.c,
	.alt_pll_freqs = alt_pll_perfcl_freqs,
	.alt_pll_thresh = 1190400000,
	.n_alt_pll_freqs = ARRAY_SIZE(alt_pll_perfcl_freqs),
	.pm_qos_latency = PERFCL_LATENCY_NO_L2_PC_US,
	.do_half_rate = true,
	.postdiv = 2,
	.c = {
		.parent = &perfcl_hf_mux.c,
		.dbg_name = "perfcl_clk",
		.ops = &clk_ops_cpu_8996,
		.vdd_class = &vdd_perfcl,
		CLK_INIT(perfcl_clk.c),
	},
};

static DEFINE_SPINLOCK(acd_lock);

static struct clk *mpidr_to_clk(void)
{
	u64 hwid = read_cpuid_mpidr() & 0xFFF;

	if ((hwid | pwrcl_clk.cpu_reg_mask) == pwrcl_clk.cpu_reg_mask)
		return &pwrcl_clk.c;
	if ((hwid | perfcl_clk.cpu_reg_mask) == perfcl_clk.cpu_reg_mask)
		return &perfcl_clk.c;

	return NULL;
}

#define SSSCTL_OFFSET 0x160

/*
 * This *has* to be called on the intended CPU
 */
static void cpu_clock_8996_acd_init(void)
{
	u64 l2acdtd;
	unsigned long flags;

	spin_lock_irqsave(&acd_lock, flags);

	if (!enable_acd) {
		spin_unlock_irqrestore(&acd_lock, flags);
		return;
	}

	READ_L2ACDTD(l2acdtd);

	/* If we have init'ed and the config is still present, return */
	if (mpidr_to_clk() == &pwrcl_clk.c && l2acdtd == acdtd_val_pwrcl) {
		spin_unlock_irqrestore(&acd_lock, flags);
		return;
	} else if (mpidr_to_clk() == &pwrcl_clk.c) {
		WRITE_L2ACDTD(acdtd_val_pwrcl);
	}

	if (mpidr_to_clk() == &perfcl_clk.c && l2acdtd == acdtd_val_perfcl) {
		spin_unlock_irqrestore(&acd_lock, flags);
		return;
	} else if (mpidr_to_clk() == &perfcl_clk.c) {
		WRITE_L2ACDTD(acdtd_val_perfcl);
	}

	/* Initial ACD for *this* cluster */
	WRITE_L2ACDDVMRC(dvmrc_val);
	WRITE_L2ACDSSCR(acdsscr_val);

	if (mpidr_to_clk() == &pwrcl_clk.c) {
		if (vbases[APC0_BASE])
			writel_relaxed(0x0000000F, vbases[APC0_BASE] +
				       SSSCTL_OFFSET);
		/* Ensure SSSCTL config goes through before enabling ACD. */
		mb();
		WRITE_L2ACDCR(acdcr_val_pwrcl);
	} else {
		WRITE_L2ACDCR(acdcr_val_perfcl);
		if (vbases[APC1_BASE])
			writel_relaxed(0x0000000F, vbases[APC1_BASE] +
				       SSSCTL_OFFSET);
		/* Ensure SSSCTL config goes through before enabling ACD. */
		mb();
	}

	spin_unlock_irqrestore(&acd_lock, flags);
}

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node;
	const u32 *cell;
	u64 hwid;
	static struct clk *cpu_clk_map[NR_CPUS];

	if (cpu_clk_map[cpu])
		return cpu_clk_map[cpu];

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		goto fail;

	cell = of_get_property(cpu_node, "reg", NULL);
	if (!cell) {
		pr_err("%s: missing reg property\n", cpu_node->full_name);
		goto fail;
	}

	hwid = of_read_number(cell, of_n_addr_cells(cpu_node));
	if ((hwid | pwrcl_clk.cpu_reg_mask) == pwrcl_clk.cpu_reg_mask) {
		cpu_clk_map[cpu] = &pwrcl_clk.c;
		return &pwrcl_clk.c;
	}
	if ((hwid | perfcl_clk.cpu_reg_mask) == perfcl_clk.cpu_reg_mask) {
		cpu_clk_map[cpu] = &perfcl_clk.c;
		return &perfcl_clk.c;
	}

fail:
	return NULL;
}

static struct pll_clk cbf_pll = {
	.mode_reg = (void __iomem *)CBF_PLL_MODE,
	.l_reg = (void __iomem *)CBF_PLL_L_VAL,
	.alpha_reg = (void __iomem *)CBF_PLL_ALPHA,
	.config_reg = (void __iomem *)CBF_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)CBF_PLL_CONFIG_CTL,
	.config_ctl_hi_reg = (void __iomem *)CBF_PLL_CONFIG_CTL_HI,
	.status_reg = (void __iomem *)CBF_PLL_MODE,
	.test_ctl_lo_reg = (void __iomem *)CBF_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)CBF_PLL_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.init_test_ctl = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.post_div_masked = 0x100,
		.pre_div_masked = 0x0,
		.test_ctl_hi_val = 0x00004000,
		.test_ctl_lo_val = 0x04000000,
		.config_ctl_val = 0x200D4AA8,
		.config_ctl_hi_val = 0x002,
	},
	.min_rate =  600000000,
	.max_rate = 3000000000,
	.src_rate =   19200000,
	.base = &vbases[CBF_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "cbf_pll",
		.ops = &clk_ops_variable_rate_pll_hwfsm,
		VDD_DIG_FMAX_MAP1(LOW, 3000000000),
		CLK_INIT(cbf_pll.c),
	},
};

DEFINE_FIXED_DIV_CLK(cbf_pll_main, 2, &cbf_pll.c);

#define CBF_MUX_OFFSET 0x018

DEFINE_VDD_REGS_INIT(vdd_cbf, 1);

static struct mux_clk cbf_hf_mux = {
	.offset = CBF_MUX_OFFSET,
	MUX_SRC_LIST(
		{ &cbf_pll.c,         1 },
		{ &cbf_pll_main.c,    2 },
		{ &sys_apcsaux_clk.c, 3 },
	),
	.en_mask = 0,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 0,
	.base = &vbases[CBF_BASE],
	.c = {
		.dbg_name = "cbf_hf_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(cbf_hf_mux.c),
	},
};

static struct cpu_clk_8996 cbf_clk = {
	.do_half_rate = true,
	.postdiv = 2,
	.c = {
		.parent = &cbf_hf_mux.c,
		.dbg_name = "cbf_clk",
		.ops = &clk_ops_cpu_8996,
		.vdd_class = &vdd_cbf,
		CLK_INIT(cbf_clk.c),
	},
};

#define APCS_CLK_DIAG 0x78

DEFINE_FIXED_SLAVE_DIV_CLK(pwrcl_div_clk, 16, &pwrcl_clk.c);
DEFINE_FIXED_SLAVE_DIV_CLK(perfcl_div_clk, 16, &perfcl_clk.c);

static struct mux_clk cpu_debug_mux = {
	.offset = APCS_CLK_DIAG,
	MUX_SRC_LIST(
		{ &cbf_clk.c, 0x01 },
		{ &pwrcl_div_clk.c,  0x11 },
		{ &perfcl_div_clk.c, 0x21 },
	),
	MUX_REC_SRC_LIST(
		&cbf_clk.c,
		&pwrcl_div_clk.c,
		&perfcl_div_clk.c,
	),
	.ops = &cpu_debug_mux_ops,
	.mask = 0xFF,
	.shift = 8,
	.base = &vbases[DEBUG_BASE],
	.c = {
		.dbg_name = "cpu_debug_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(cpu_debug_mux.c),
	},
};

static struct clk_lookup cpu_clocks_8996[] = {
	CLK_LIST(pwrcl_clk),
	CLK_LIST(pwrcl_pll),
	CLK_LIST(pwrcl_alt_pll),
	CLK_LIST(pwrcl_pll_main),
	CLK_LIST(pwrcl_hf_mux),
	CLK_LIST(pwrcl_lf_mux),

	CLK_LIST(perfcl_clk),
	CLK_LIST(perfcl_pll),
	CLK_LIST(perfcl_alt_pll),
	CLK_LIST(perfcl_pll_main),
	CLK_LIST(perfcl_hf_mux),
	CLK_LIST(perfcl_lf_mux),

	CLK_LIST(cbf_pll),
	CLK_LIST(cbf_pll_main),
	CLK_LIST(cbf_hf_mux),
	CLK_LIST(cbf_clk),

	CLK_LIST(xo_ao),
	CLK_LIST(sys_apcsaux_clk),

	CLK_LIST(cpu_debug_mux),
};

static int of_get_fmax_vdd_class(struct platform_device *pdev, struct clk *c,
								char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, i;
	struct clk_vdd_class *vdd = c->vdd_class;
	u32 *array;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % 2) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= 2;
	vdd->level_votes = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->level_votes)
		return -ENOMEM;

	vdd->vdd_uv = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->vdd_uv)
		return -ENOMEM;

	c->fmax = devm_kzalloc(&pdev->dev, prop_len * sizeof(unsigned long),
					GFP_KERNEL);
	if (!c->fmax)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * 2, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * 2);
	for (i = 0; i < prop_len; i++) {
		c->fmax[i] = array[2 * i];
		vdd->vdd_uv[i] = array[2 * i + 1];
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	c->num_fmax = prop_len;
	return 0;
}

static int cpu_clock_8996_resources_init(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *c;
	int i;

	for (i = 0; i < ARRAY_SIZE(base_names); i++) {

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   base_names[i]);
		if (!res) {
			dev_err(&pdev->dev,
				 "Unable to get platform resource for %s",
				 base_names[i]);
			return -ENOMEM;
		}

		vbases[i] = devm_ioremap(&pdev->dev, res->start,
					     resource_size(res));
		if (!vbases[i]) {
			dev_err(&pdev->dev, "Unable to map in base %s\n",
				base_names[i]);
			return -ENOMEM;
		}
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (PTR_ERR(vdd_dig.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the CX regulator");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	vdd_pwrcl.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-pwrcl");
	if (IS_ERR(vdd_pwrcl.regulator[0])) {
		if (PTR_ERR(vdd_pwrcl.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the pwrcl vreg\n");
		return PTR_ERR(vdd_pwrcl.regulator[0]);
	}

	vdd_perfcl.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-perfcl");
	if (IS_ERR(vdd_perfcl.regulator[0])) {
		if (PTR_ERR(vdd_perfcl.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the perfcl vreg\n");
		return PTR_ERR(vdd_perfcl.regulator[0]);
	}

	/* Leakage constraints disallow a turbo vote during bootup */
	vdd_perfcl.skip_handoff = true;

	vdd_cbf.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-cbf");
	if (IS_ERR(vdd_cbf.regulator[0])) {
		if (PTR_ERR(vdd_cbf.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the cbf vreg\n");
		return PTR_ERR(vdd_cbf.regulator[0]);
	}

	c = devm_clk_get(&pdev->dev, "xo_ao");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo (rc = %ld)!\n",
				PTR_ERR(c));
		return PTR_ERR(c);
	}
	xo_ao.c.parent = c;

	c = devm_clk_get(&pdev->dev, "aux_clk");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll0 (rc = %ld)!\n",
				PTR_ERR(c));
		return PTR_ERR(c);
	}
	sys_apcsaux_clk_gcc.c.parent = c;

	vdd_perfcl.use_max_uV = true;
	vdd_pwrcl.use_max_uV = true;
	vdd_cbf.use_max_uV = true;

	return 0;
}

static int add_opp(struct clk *c, struct device *dev, unsigned long max_rate)
{
	unsigned long rate = 0;
	int level;
	int uv;
	int *vdd_uv = c->vdd_class->vdd_uv;
	struct regulator *reg = c->vdd_class->regulator[0];
	long ret;
	bool first = true;
	int j = 1;

	while (1) {
		rate = c->fmax[j++];
		level = find_vdd_level(c, rate);
		if (level <= 0) {
			pr_warn("clock-cpu: no corner for %lu.\n", rate);
			return -EINVAL;
		}

		uv = regulator_list_corner_voltage(reg, vdd_uv[level]);
		if (uv < 0) {
			pr_warn("clock-cpu: no uv for %lu.\n", rate);
			return -EINVAL;
		}

		ret = dev_pm_opp_add(dev, rate, uv);
		if (ret) {
			pr_warn("clock-cpu: failed to add OPP for %lu\n", rate);
			return ret;
		}

		/*
		 * Print the OPP pair for the lowest and highest frequency for
		 * each device that we're populating. This is important since
		 * this information will be used by thermal mitigation and the
		 * scheduler.
		 */
		if ((rate >= max_rate) || first) {
			/* one time print at bootup */
			pr_info("clock-cpu-8996: set OPP pair (%lu Hz, %d uv) on %s\n",
				rate, uv, dev_name(dev));
			if (first)
				first = false;
			else
				break;
		}
	}

	return 0;
}

static void populate_opp_table(struct platform_device *pdev)
{
	unsigned long pwrcl_fmax, perfcl_fmax, cbf_fmax;
	struct device_node *cbf_node;
	struct platform_device *cbf_dev;
	int cpu;

	pwrcl_fmax = pwrcl_clk.c.fmax[pwrcl_clk.c.num_fmax - 1];
	perfcl_fmax = perfcl_clk.c.fmax[perfcl_clk.c.num_fmax - 1];
	cbf_fmax = cbf_clk.c.fmax[cbf_clk.c.num_fmax - 1];

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &pwrcl_clk.c) {
			WARN(add_opp(&pwrcl_clk.c, get_cpu_device(cpu),
				     pwrcl_fmax),
			     "Failed to add OPP levels for power cluster\n");
		}
		if (logical_cpu_to_clk(cpu) == &perfcl_clk.c) {
			WARN(add_opp(&perfcl_clk.c, get_cpu_device(cpu),
				     perfcl_fmax),
			     "Failed to add OPP levels for perf cluster\n");
		}
	}

	cbf_node = of_parse_phandle(pdev->dev.of_node, "cbf-dev", 0);
	if (!cbf_node) {
		pr_err("can't find the CBF dt node\n");
		return;
	}

	cbf_dev = of_find_device_by_node(cbf_node);
	if (!cbf_dev) {
		pr_err("can't find the CBF dt device\n");
		return;
	}

	WARN(add_opp(&cbf_clk.c, &cbf_dev->dev, cbf_fmax),
	    "Failed to add OPP levels for CBF\n");
}

static void cpu_clock_8996_pro_fixup(void)
{
	cbf_pll.vals.post_div_masked = 0x300;
	cbf_pll_main.data.max_div = 4;
	cbf_pll_main.data.min_div = 4;
	cbf_pll_main.data.div = 4;
	cbf_clk.postdiv = 4;
}

static int perfclspeedbin;

unsigned long pwrcl_early_boot_rate = 883200000;
unsigned long perfcl_early_boot_rate = 883200000;
unsigned long cbf_early_boot_rate = 614400000;
unsigned long alt_pll_early_boot_rate = 307200000;

static int cpu_clock_8996_driver_probe(struct platform_device *pdev)
{
	int ret, cpu;
	unsigned long pwrclrate, perfclrate, cbfrate;
	int pvs_ver = 0;
	u32 pte_efuse;
	char perfclspeedbinstr[] = "qcom,perfcl-speedbinXX-vXX";
	char pwrclspeedbinstr[] = "qcom,pwrcl-speedbinXX-vXX";
	char cbfspeedbinstr[] = "qcom,cbf-speedbinXX-vXX";

	pwrcl_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	perfcl_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	cbf_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	pwrcl_clk.hw_low_power_ctrl = true;
	perfcl_clk.hw_low_power_ctrl = true;

	ret = cpu_clock_8996_resources_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "resources init failed\n");
		return ret;
	}

	pte_efuse = readl_relaxed(vbases[EFUSE_BASE]);
	perfclspeedbin = ((pte_efuse >> EFUSE_SHIFT) & EFUSE_MASK);
	dev_info(&pdev->dev, "using perf/pwr/cbf speed bin %u and pvs_ver %d\n",
		 perfclspeedbin, pvs_ver);

	snprintf(perfclspeedbinstr, ARRAY_SIZE(perfclspeedbinstr),
			"qcom,perfcl-speedbin%d-v%d", perfclspeedbin, pvs_ver);

	ret = of_get_fmax_vdd_class(pdev, &perfcl_clk.c, perfclspeedbinstr);
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for perfcl. Falling back to zero.\n");
		ret = of_get_fmax_vdd_class(pdev, &perfcl_clk.c,
					    "qcom,perfcl-speedbin0-v0");
		if (ret) {
			dev_err(&pdev->dev, "Unable to retrieve plan for perf. Bailing...\n");
			return ret;
		}
	}

	snprintf(pwrclspeedbinstr, ARRAY_SIZE(pwrclspeedbinstr),
			"qcom,pwrcl-speedbin%d-v%d", perfclspeedbin, pvs_ver);

	ret = of_get_fmax_vdd_class(pdev, &pwrcl_clk.c, pwrclspeedbinstr);
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for pwrcl. Falling back to zero.\n");
		ret = of_get_fmax_vdd_class(pdev, &pwrcl_clk.c,
				    "qcom,pwrcl-speedbin0-v0");
		if (ret) {
			dev_err(&pdev->dev, "Unable to retrieve plan for pwrcl\n");
			return ret;
		}
	}

	snprintf(cbfspeedbinstr, ARRAY_SIZE(cbfspeedbinstr),
			"qcom,cbf-speedbin%d-v%d", perfclspeedbin, pvs_ver);

	ret = of_get_fmax_vdd_class(pdev, &cbf_clk.c, cbfspeedbinstr);
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for cbf. Falling back to zero.\n");
		ret = of_get_fmax_vdd_class(pdev, &cbf_clk.c,
				    "qcom,cbf-speedbin0-v0");
		if (ret) {
			dev_err(&pdev->dev, "Unable to retrieve plan for cbf\n");
			return ret;
		}
	}

	get_online_cpus();

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &pwrcl_clk.c)
			cpumask_set_cpu(cpu, &pwrcl_clk.cpumask);
		if (logical_cpu_to_clk(cpu) == &perfcl_clk.c)
			cpumask_set_cpu(cpu, &perfcl_clk.cpumask);
	}

	ret = of_msm_clock_register(pdev->dev.of_node, cpu_clocks_8996,
				    ARRAY_SIZE(cpu_clocks_8996));
	if (ret) {
		dev_err(&pdev->dev, "Unable to register CPU clocks.\n");
		return ret;
	}

	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(&cbf_clk.c),
			"Failed to enable cbf clock.\n");
		WARN(clk_prepare_enable(logical_cpu_to_clk(cpu)),
			"Failed to enable clock for cpu %d\n", cpu);
	}

	pwrclrate = clk_get_rate(&pwrcl_clk.c);
	perfclrate = clk_get_rate(&perfcl_clk.c);
	cbfrate = clk_get_rate(&cbf_clk.c);

	if (!pwrclrate) {
		dev_err(&pdev->dev, "Unknown pwrcl rate. Setting safe rate\n");
		ret = clk_set_rate(&pwrcl_clk.c, sys_apcsaux_clk.c.rate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on A53.\n");
			return -EINVAL;
		}
		pwrclrate = sys_apcsaux_clk.c.rate;
	}

	if (!perfclrate) {
		dev_err(&pdev->dev, "Unknown perfcl rate. Setting safe rate\n");
		ret = clk_set_rate(&perfcl_clk.c, sys_apcsaux_clk.c.rate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on perf.\n");
			return -EINVAL;
		}
		perfclrate = sys_apcsaux_clk.c.rate;
	}

	if (!cbfrate) {
		dev_err(&pdev->dev, "Unknown CBF rate. Setting safe rate\n");
		cbfrate = sys_apcsaux_clk.c.rate;
		ret = clk_set_rate(&cbf_clk.c, cbfrate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on CBF.\n");
			return -EINVAL;
		}
	}

	/* Permanently enable the cluster PLLs */
	clk_prepare_enable(&perfcl_pll.c);
	clk_prepare_enable(&pwrcl_pll.c);
	clk_prepare_enable(&perfcl_alt_pll.c);
	clk_prepare_enable(&pwrcl_alt_pll.c);
	clk_prepare_enable(&cbf_pll.c);

	/* Set the early boot rate. This may also switch us to the ACD leg */
	clk_set_rate(&pwrcl_clk.c, pwrcl_early_boot_rate);
	clk_set_rate(&perfcl_clk.c, perfcl_early_boot_rate);

	populate_opp_table(pdev);

	put_online_cpus();

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-clock-8996" },
	{ .compatible = "qcom,cpu-clock-8996-v3" },
	{ .compatible = "qcom,cpu-clock-8996-pro" },
	{}
};

static struct platform_driver cpu_clock_8996_driver = {
	.probe = cpu_clock_8996_driver_probe,
	.driver = {
		.name = "cpu-clock-8996",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init cpu_clock_8996_init(void)
{
	return platform_driver_register(&cpu_clock_8996_driver);
}
arch_initcall(cpu_clock_8996_init);

static void __exit cpu_clock_8996_exit(void)
{
	platform_driver_unregister(&cpu_clock_8996_driver);
}
module_exit(cpu_clock_8996_exit);

#define APC0_BASE_PHY 0x06400000
#define APC1_BASE_PHY 0x06480000
#define CBF_BASE_PHY 0x09A11000
#define CBF_PLL_BASE_PHY 0x09A20000
#define AUX_BASE_PHY 0x09820050
#define APCC_RECAL_DLY_BASE 0x099E00C8

#define CLK_CTL_OFFSET 0x44
#define PSCTL_OFFSET 0x164
#define AUTO_CLK_SEL_BIT BIT(8)
#define CBF_AUTO_CLK_SEL_BIT BIT(6)
#define AUTO_CLK_SEL_ALWAYS_ON_MASK BM(5, 4)
#define AUTO_CLK_SEL_ALWAYS_ON_GPLL0_SEL (0x3 << 4)
#define APCC_RECAL_DLY_SIZE 0x10
#define APCC_RECAL_VCTL_OFFSET 0x8
#define APCC_RECAL_CPR_DLY_SETTING 0x00000000
#define APCC_RECAL_VCTL_DLY_SETTING 0x800003ff

#define HF_MUX_MASK 0x3
#define LF_MUX_MASK 0x3
#define LF_MUX_SHIFT 0x2
#define HF_MUX_SEL_EARLY_PLL 0x1
#define HF_MUX_SEL_LF_MUX 0x1
#define LF_MUX_SEL_ALT_PLL 0x1

static int use_alt_pll;
module_param(use_alt_pll, int, 0444);

static int clock_cpu_8996_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	if (!enable_acd)
		return NOTIFY_OK;

	switch (action & ~CPU_TASKS_FROZEN) {

	case CPU_STARTING:
		/* This is needed for the first time that CPUs come up */
		cpu_clock_8996_acd_init();
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata clock_cpu_8996_cpu_notifier = {
	.notifier_call = clock_cpu_8996_cpu_callback,
};

int __init cpu_clock_8996_early_init(void)
{
	int ret = 0;
	void __iomem *auxbase, *acd_recal_base;
	u32 regval;

	if (of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8996-pro")) {
		cpu_clocks_v3 = true;
		cpu_clocks_pro = true;
	} else if (of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8996-v3")) {
		cpu_clocks_v3 = true;
	} else if (!of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8996")) {
		return 0;
	}

	pr_info("clock-cpu-8996: configuring clocks for the perf cluster\n");

	if (cpu_clocks_v3) {
		pwrcl_alt_pll.offline_bit_workaround = false;
		perfcl_alt_pll.offline_bit_workaround = false;
		pwrcl_pll.pgm_test_ctl_enable = false;
		perfcl_pll.pgm_test_ctl_enable = false;
		pwrcl_pll.vals.config_ctl_val = 0x200d4828;
		pwrcl_pll.vals.config_ctl_hi_val = 0x006;
		perfcl_pll.vals.config_ctl_val = 0x200d4828;
		perfcl_pll.vals.config_ctl_hi_val = 0x006;
		cbf_pll.vals.config_ctl_val = 0x200d4828;
		cbf_pll.vals.config_ctl_hi_val = 0x006;
		pwrcl_pll.vals.test_ctl_lo_val = 0x1C000000;
		perfcl_pll.vals.test_ctl_lo_val = 0x1C000000;
		cbf_pll.vals.test_ctl_lo_val = 0x1C000000;
	}

	if (cpu_clocks_pro)
		cpu_clock_8996_pro_fixup();

	/*
	 * We definitely don't want to parse DT here - this is too early and in
	 * the critical path for boot timing. Just ioremap the bases.
	 */
	vbases[APC0_BASE] = ioremap(APC0_BASE_PHY, SZ_4K);
	if (!vbases[APC0_BASE]) {
		WARN(1, "Unable to ioremap power mux base. Can't configure CPU clocks\n");
		ret = -ENOMEM;
		goto fail;
	}

	vbases[APC1_BASE] = ioremap(APC1_BASE_PHY, SZ_4K);
	if (!vbases[APC1_BASE]) {
		WARN(1, "Unable to ioremap perf mux base. Can't configure CPU clocks\n");
		ret = -ENOMEM;
		goto apc1_fail;
	}

	vbases[CBF_BASE] = ioremap(CBF_BASE_PHY, SZ_4K);
	if (!vbases[CBF_BASE]) {
		WARN(1, "Unable to ioremap cbf mux base. Can't configure CPU clocks\n");
		ret = -ENOMEM;
		goto cbf_map_fail;
	}

	vbases[CBF_PLL_BASE] = ioremap(CBF_PLL_BASE_PHY, SZ_4K);
	if (!vbases[CBF_BASE]) {
		WARN(1, "Unable to ioremap cbf pll base. Can't configure CPU clocks\n");
		ret = -ENOMEM;
		goto cbf_pll_map_fail;
	}

	vbases[APC0_PLL_BASE] = vbases[APC0_BASE];
	vbases[APC1_PLL_BASE] = vbases[APC1_BASE];

	auxbase = ioremap(AUX_BASE_PHY, SZ_4K);
	if (!auxbase) {
		WARN(1, "Unable to ioremap aux base. Can't configure CPU clocks\n");
		ret = -ENOMEM;
		goto auxbase_fail;
	}

	acd_recal_base = ioremap(APCC_RECAL_DLY_BASE, APCC_RECAL_DLY_SIZE);
	if (!acd_recal_base) {
		WARN(1, "Unable to ioremap ACD recal base. Can't configure ACD\n");
		ret = -ENOMEM;
		goto acd_recal_base_fail;
	}

	/*
	 * Set GPLL0 divider for div-2 to get 300Mhz. This divider
	 * can be programmed dynamically.
	 */
	regval = readl_relaxed(auxbase);
	regval &= ~BM(17, 16);
	regval |= 0x1 << 16;
	writel_relaxed(regval, auxbase);

	/* Ensure write goes through before selecting the aux clock */
	mb();
	udelay(5);

	/* Select GPLL0 for 300MHz for the perf cluster */
	writel_relaxed(0xC, vbases[APC1_BASE] + MUX_OFFSET);

	/* Select GPLL0 for 300MHz for the power cluster */
	writel_relaxed(0xC, vbases[APC0_BASE] + MUX_OFFSET);

	/* Select GPLL0 for 300MHz on the CBF  */
	writel_relaxed(0x3, vbases[CBF_BASE] + CBF_MUX_OFFSET);

	/* Ensure write goes through before PLLs are reconfigured */
	mb();
	udelay(5);

	/* Set the auto clock sel always-on source to GPLL0/2 (300MHz) */
	regval = readl_relaxed(vbases[APC0_BASE] + MUX_OFFSET);
	regval &= ~AUTO_CLK_SEL_ALWAYS_ON_MASK;
	regval |= AUTO_CLK_SEL_ALWAYS_ON_GPLL0_SEL;
	writel_relaxed(regval, vbases[APC0_BASE] + MUX_OFFSET);

	regval = readl_relaxed(vbases[APC1_BASE] + MUX_OFFSET);
	regval &= ~AUTO_CLK_SEL_ALWAYS_ON_MASK;
	regval |= AUTO_CLK_SEL_ALWAYS_ON_GPLL0_SEL;
	writel_relaxed(regval, vbases[APC1_BASE] + MUX_OFFSET);

	regval = readl_relaxed(vbases[CBF_BASE] + MUX_OFFSET);
	regval &= ~AUTO_CLK_SEL_ALWAYS_ON_MASK;
	regval |= AUTO_CLK_SEL_ALWAYS_ON_GPLL0_SEL;
	writel_relaxed(regval, vbases[CBF_BASE] + MUX_OFFSET);

	/* == Setup PLLs in FSM mode == */

	/* Disable all PLLs (we're already on GPLL0 for both clusters) */
	perfcl_alt_pll.c.ops->disable(&perfcl_alt_pll.c);
	pwrcl_alt_pll.c.ops->disable(&pwrcl_alt_pll.c);
	writel_relaxed(0x0, vbases[APC0_BASE] +
			(unsigned long)pwrcl_pll.mode_reg);
	writel_relaxed(0x0, vbases[APC1_BASE] +
			(unsigned long)perfcl_pll.mode_reg);
	writel_relaxed(0x0, vbases[CBF_PLL_BASE] +
			(unsigned long)cbf_pll.mode_reg);

	/* Let PLLs disable before re-init'ing them */
	mb();

	/* Initialize all the PLLs */
	__variable_rate_pll_init(&perfcl_pll.c);
	__variable_rate_pll_init(&pwrcl_pll.c);
	__variable_rate_pll_init(&cbf_pll.c);
	__init_alpha_pll(&perfcl_alt_pll.c);
	__init_alpha_pll(&pwrcl_alt_pll.c);

	/* Set an appropriate rate on the perf clusters PLLs */
	perfcl_pll.c.ops->set_rate(&perfcl_pll.c, perfcl_early_boot_rate);
	perfcl_alt_pll.c.ops->set_rate(&perfcl_alt_pll.c,
				       alt_pll_early_boot_rate);
	pwrcl_pll.c.ops->set_rate(&pwrcl_pll.c, pwrcl_early_boot_rate);
	pwrcl_alt_pll.c.ops->set_rate(&pwrcl_alt_pll.c,
				      alt_pll_early_boot_rate);

	/* Set an appropriate rate on the CBF PLL */
	cbf_pll.c.ops->set_rate(&cbf_pll.c, cbf_early_boot_rate);

	/*
	 * Enable FSM mode on the primary PLLs.
	 * This should turn on the PLLs as well.
	 */
	writel_relaxed(0x00118000, vbases[APC0_BASE] +
			(unsigned long)pwrcl_pll.mode_reg);
	writel_relaxed(0x00118000, vbases[APC1_BASE] +
			(unsigned long)perfcl_pll.mode_reg);
	/*
	 * Enable FSM mode on the alternate PLLs.
	 * This should turn on the PLLs as well.
	 */
	writel_relaxed(0x00118000, vbases[APC0_BASE] +
			(unsigned long)pwrcl_alt_pll.offset);
	writel_relaxed(0x00118000, vbases[APC1_BASE] +
			(unsigned long)perfcl_alt_pll.offset);

	/*
	 * Enable FSM mode on the CBF PLL.
	 * This should turn on the PLL as well.
	 */
	writel_relaxed(0x00118000, vbases[CBF_PLL_BASE] +
			(unsigned long)cbf_pll.mode_reg);

	/*
	 * If we're on MSM8996 V1, the CBF FSM bits are not present, and
	 * the mode register will read zero at this point. In that case,
	 * just enable the CBF PLL to "simulate" FSM mode.
	 */
	if (!readl_relaxed(vbases[CBF_PLL_BASE] +
		(unsigned long)cbf_pll.mode_reg))
		clk_ops_variable_rate_pll.enable(&cbf_pll.c);

	/* Ensure write goes through before auto clock selection is enabled */
	mb();

	/* Wait for PLL(s) to lock */
	udelay(50);

	/* Enable auto clock selection for both clusters and the CBF */
	regval = readl_relaxed(vbases[APC0_BASE] + CLK_CTL_OFFSET);
	regval |= AUTO_CLK_SEL_BIT;
	writel_relaxed(regval, vbases[APC0_BASE] + CLK_CTL_OFFSET);

	regval = readl_relaxed(vbases[APC1_BASE] + CLK_CTL_OFFSET);
	regval |= AUTO_CLK_SEL_BIT;
	writel_relaxed(regval, vbases[APC1_BASE] + CLK_CTL_OFFSET);

	regval = readl_relaxed(vbases[CBF_BASE] + CBF_MUX_OFFSET);
	regval |= CBF_AUTO_CLK_SEL_BIT;
	writel_relaxed(regval, vbases[CBF_BASE] + CBF_MUX_OFFSET);

	/* Ensure write goes through before muxes are switched */
	mb();
	udelay(5);

	if (use_alt_pll) {
		perfcl_hf_mux.safe_parent = &perfcl_lf_mux.c;
		pwrcl_hf_mux.safe_parent = &pwrcl_lf_mux.c;
		pwrcl_clk.alt_pll = NULL;
		perfcl_clk.alt_pll = NULL;
		pwrcl_clk.do_half_rate = false;
		perfcl_clk.do_half_rate = false;

		perfcl_hf_mux.parents = (struct clk_src *)
					&clk_src_perfcl_hf_mux_alt;
		perfcl_hf_mux.num_parents =
					ARRAY_SIZE(clk_src_perfcl_hf_mux_alt);
		pwrcl_hf_mux.parents =  (struct clk_src *)
					&clk_src_pwrcl_hf_mux_alt;
		pwrcl_hf_mux.num_parents =
					ARRAY_SIZE(clk_src_pwrcl_hf_mux_alt);

		perfcl_lf_mux.parents = (struct clk_src *)
					&clk_src_perfcl_lf_mux_alt;
		perfcl_lf_mux.num_parents =
					ARRAY_SIZE(clk_src_perfcl_lf_mux_alt);
		pwrcl_lf_mux.parents =  (struct clk_src *)
					&clk_src_pwrcl_lf_mux_alt;
		pwrcl_lf_mux.num_parents =
					ARRAY_SIZE(clk_src_pwrcl_lf_mux_alt);

		/* Switch the clusters to use the alternate PLLs */
		writel_relaxed(0x33, vbases[APC0_BASE] + MUX_OFFSET);
		writel_relaxed(0x33, vbases[APC1_BASE] + MUX_OFFSET);
	}

	/* Switch the CBF to use the primary PLL */
	regval = readl_relaxed(vbases[CBF_BASE] + CBF_MUX_OFFSET);
	regval &= ~BM(1, 0);
	regval |= 0x1;
	writel_relaxed(regval, vbases[CBF_BASE] + CBF_MUX_OFFSET);

	if (!cpu_clocks_v3)
		enable_acd = 0;

	if (enable_acd) {
		int i;

		if (use_alt_pll)
			panic("Can't enable ACD on the the alternate PLL\n");

		perfcl_clk.has_acd = true;
		pwrcl_clk.has_acd = true;

		if (cpu_clocks_pro) {
			/*
			 * Configure ACS logic to switch to always-on clock
			 * source during D2-D5 entry. In addition, gate the
			 * limits management clock during certain sleep states.
			 */
			writel_relaxed(0x3, vbases[APC0_BASE] +
							MDD_DROOP_CODE);
			writel_relaxed(0x3, vbases[APC1_BASE] +
							MDD_DROOP_CODE);
			/*
			 * Ensure that the writes go through before going
			 * forward.
			 */
			wmb();

			/*
			 * Program the DLY registers to set a voltage settling
			 * delay time for HW based ACD recalibration.
			 */
			writel_relaxed(APCC_RECAL_CPR_DLY_SETTING,
						acd_recal_base);
			writel_relaxed(APCC_RECAL_VCTL_DLY_SETTING,
						acd_recal_base +
						APCC_RECAL_VCTL_OFFSET);
			/*
			 * Ensure that the writes go through before enabling
			 * ACD.
			 */
			wmb();
		}

		/* Enable ACD on this cluster if necessary */
		cpu_clock_8996_acd_init();

		/* Ensure we never use the non-ACD leg of the GFMUX */
		for (i = 0; i < pwrcl_hf_mux.num_parents; i++)
			if (pwrcl_hf_mux.parents[i].src == &pwrcl_pll.c)
				pwrcl_hf_mux.parents[i].sel = 2;

		for (i = 0; i < perfcl_hf_mux.num_parents; i++)
			if (perfcl_hf_mux.parents[i].src == &perfcl_pll.c)
				perfcl_hf_mux.parents[i].sel = 2;

		BUG_ON(register_hotcpu_notifier(&clock_cpu_8996_cpu_notifier));

		/* Pulse swallower and soft-start settings */
		writel_relaxed(0x00030005, vbases[APC0_BASE] + PSCTL_OFFSET);
		writel_relaxed(0x00030005, vbases[APC1_BASE] + PSCTL_OFFSET);

		/* Ensure all config above goes through before the ACD switch */
		mb();

		/* Switch the clusters to use the ACD leg */
		writel_relaxed(0x32, vbases[APC0_BASE] + MUX_OFFSET);
		writel_relaxed(0x32, vbases[APC1_BASE] + MUX_OFFSET);
	} else {
		/* Switch the clusters to use the primary PLLs */
		writel_relaxed(0x31, vbases[APC0_BASE] + MUX_OFFSET);
		writel_relaxed(0x31, vbases[APC1_BASE] + MUX_OFFSET);
	}

	/*
	 * One time print during boot - this is the earliest time
	 * that Linux configures the CPU clocks. It's critical for
	 * debugging that we know that this configuration completed,
	 * especially when debugging CPU hangs.
	 */
	pr_info("%s: finished CPU clock configuration\n", __func__);

	iounmap(acd_recal_base);
acd_recal_base_fail:
	iounmap(auxbase);
auxbase_fail:
	iounmap(vbases[CBF_PLL_BASE]);
cbf_pll_map_fail:
	iounmap(vbases[CBF_BASE]);
cbf_map_fail:
	if (ret) {
		iounmap(vbases[APC1_BASE]);
		vbases[APC1_BASE] = NULL;
	}
apc1_fail:
	if (ret) {
		iounmap(vbases[APC0_BASE]);
		vbases[APC0_BASE] = NULL;
	}
fail:
	return ret;
}
early_initcall(cpu_clock_8996_early_init);

MODULE_DESCRIPTION("CPU clock driver for msm8996");
MODULE_LICENSE("GPL v2");
