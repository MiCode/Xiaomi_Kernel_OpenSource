/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/scm.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-alpha-pll.h>

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

DEFINE_EXT_CLK(xo_ao, NULL);
DEFINE_CLK_DUMMY(alpha_xo_ao, 19200000);
DEFINE_EXT_CLK(sys_apcsaux_clk_gcc, NULL);
DEFINE_FIXED_SLAVE_DIV_CLK(sys_apcsaux_clk, 2, &sys_apcsaux_clk_gcc.c);

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
	VCO(0, 1000000000, 2000000000),
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

struct cpu_clk_8996 {
	u32 cpu_reg_mask;
	struct clk *alt_pll;
	unsigned long *alt_pll_freqs;
	int n_alt_pll_freqs;
	struct clk c;
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
	return clk_round_rate(c->parent, rate);
}

static unsigned long alt_pll_pwrcl_freqs[] = {
	 268800000,
	 480000000,
	 883200000,
};

static unsigned long alt_pll_perfcl_freqs[] = {
	 268800000,
	 403200000,
	 576000000,
};

static int cpu_clk_8996_set_rate(struct clk *c, unsigned long rate)
{
	struct cpu_clk_8996 *cpuclk = to_cpu_clk_8996(c);
	int ret, err_ret, i;
	unsigned long alt_pll_prev_rate = cpuclk->alt_pll->rate;
	unsigned long alt_pll_rate;
	unsigned long n_alt_freqs = cpuclk->n_alt_pll_freqs;

	alt_pll_rate = cpuclk->alt_pll_freqs[0];
	if (rate >= cpuclk->alt_pll_freqs[n_alt_freqs - 1])
		alt_pll_rate = cpuclk->alt_pll_freqs[n_alt_freqs - 1];

	for (i = 0; i < n_alt_freqs - 1; i++)
		if (cpuclk->alt_pll_freqs[i] < rate &&
		    cpuclk->alt_pll_freqs[i+1] >= rate)
			alt_pll_rate = cpuclk->alt_pll_freqs[i];

	ret = clk_set_rate(cpuclk->alt_pll, alt_pll_rate);
	if (ret) {
		pr_err("failed to set rate %lu on alt_pll when setting %lu on %s (%d)\n",
			alt_pll_rate, rate, c->dbg_name, ret);
		goto out;
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
	if (c->rate > 600000000 && rate < 600000000) {
		ret = clk_set_rate(c->parent, c->rate/2);
		if (ret) {
			pr_err("failed to set rate %lu on %s (%d)\n",
				c->rate/2, c->dbg_name, ret);
			goto fail;
		}
	}

	ret = clk_set_rate(c->parent, rate);
	if (ret) {
		pr_err("failed to set rate %lu on %s (%d)\n",
			rate, c->dbg_name, ret);
		goto set_rate_fail;
	}

	return 0;

set_rate_fail:
	/* Restore parent rate if we halved it */
	if (c->rate > 600000000 && rate < 600000000) {
		err_ret = clk_set_rate(c->parent, c->rate);
		if (err_ret)
			pr_err("failed to restore %s rate to %lu\n",
			       c->dbg_name, c->rate);
	}

fail:
	err_ret = clk_set_rate(cpuclk->alt_pll, alt_pll_prev_rate);
	if (err_ret)
		pr_err("failed to reset rate to %lu on alt pll after failing to  set %lu on %s (%d)\n",
			alt_pll_prev_rate, rate, c->dbg_name, err_ret);
out:
	return ret;
}

static struct clk_ops clk_ops_cpu_8996 = {
	.set_rate = cpu_clk_8996_set_rate,
	.round_rate = cpu_clk_8996_round_rate,
	.handoff = cpu_clk_8996_handoff,
};

DEFINE_VDD_REGS_INIT(vdd_pwrcl, 1);

static struct cpu_clk_8996 pwrcl_clk = {
	.cpu_reg_mask = 0x3,
	.alt_pll = &pwrcl_alt_pll.c,
	.alt_pll_freqs = alt_pll_pwrcl_freqs,
	.n_alt_pll_freqs = ARRAY_SIZE(alt_pll_pwrcl_freqs),
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
	.n_alt_pll_freqs = ARRAY_SIZE(alt_pll_perfcl_freqs),
	.c = {
		.parent = &perfcl_hf_mux.c,
		.dbg_name = "perfcl_clk",
		.ops = &clk_ops_cpu_8996,
		.vdd_class = &vdd_perfcl,
		CLK_INIT(perfcl_clk.c),
	},
};

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node;
	const u32 *cell;
	u64 hwid;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		goto fail;

	cell = of_get_property(cpu_node, "reg", NULL);
	if (!cell) {
		pr_err("%s: missing reg property\n", cpu_node->full_name);
		goto fail;
	}

	hwid = of_read_number(cell, of_n_addr_cells(cpu_node));
	if ((hwid | pwrcl_clk.cpu_reg_mask) == pwrcl_clk.cpu_reg_mask)
		return &pwrcl_clk.c;
	if ((hwid | perfcl_clk.cpu_reg_mask) == perfcl_clk.cpu_reg_mask)
		return &perfcl_clk.c;

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
		.config_ctl_val = 0x200D4AA8,
		.config_ctl_hi_val = 0x002,
	},
	.min_rate =  600000000,
	.max_rate = 3000000000,
	.base = &vbases[CBF_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "cbf_pll",
		.ops = &clk_ops_variable_rate_pll,
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
	.safe_parent = &sys_apcsaux_clk.c,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 0,
	.base = &vbases[CBF_BASE],
	.c = {
		.dbg_name = "cbf_hf_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.vdd_class = &vdd_cbf,
		CLK_INIT(cbf_hf_mux.c),
	},
};

#define APCS_CLK_DIAG 0x78

DEFINE_FIXED_SLAVE_DIV_CLK(pwrcl_div_clk, 16, &pwrcl_clk.c);
DEFINE_FIXED_SLAVE_DIV_CLK(perfcl_div_clk, 16, &perfcl_clk.c);

static struct mux_clk cpu_debug_mux = {
	.offset = APCS_CLK_DIAG,
	MUX_SRC_LIST(
		{ &cbf_hf_mux.c, 0x01 },
		{ &pwrcl_div_clk.c,  0x11 },
		{ &perfcl_div_clk.c, 0x21 },
	),
	MUX_REC_SRC_LIST(
		&cbf_hf_mux.c,
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
	CLK_LIST(cbf_hf_mux),

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
	long ret;

	while (1) {
		ret = clk_round_rate(c, rate + 1);
		if (ret < 0) {
			pr_warn("clock-cpu: round_rate failed at %lu\n", rate);
			return ret;
		}
		rate = ret;
		level = find_vdd_level(c, rate);
		if (level <= 0) {
			pr_warn("clock-cpu: no uv for %lu.\n", rate);
			return -EINVAL;
		}
		ret = dev_pm_opp_add(dev, rate, c->vdd_class->vdd_uv[level]);
		if (ret) {
			pr_warn("clock-cpu: failed to add OPP for %lu\n", rate);
			return ret;
		}
		if (rate >= max_rate)
			break;
	}

	return 0;
}

static void populate_opp_table(struct platform_device *pdev)
{
	struct platform_device *apc0_dev, *apc1_dev;
	struct device_node *apc0_node, *apc1_node;
	unsigned long apc0_fmax, apc1_fmax;

	apc0_node = of_parse_phandle(pdev->dev.of_node, "vdd-pwrcl-supply", 0);
	apc1_node = of_parse_phandle(pdev->dev.of_node, "vdd-perfcl-supply", 0);

	if (!apc0_node) {
		pr_err("can't find the apc0 dt node.\n");
		return;
	}

	if (!apc1_node) {
		pr_err("can't find the apc1 dt node.\n");
		return;
	}

	apc0_dev = of_find_device_by_node(apc0_node);
	apc1_dev = of_find_device_by_node(apc1_node);

	if (!apc0_dev) {
		pr_err("can't find the apc0 device node.\n");
		return;
	}

	if (!apc1_dev) {
		pr_err("can't find the apc1 device node.\n");
		return;
	}

	apc0_fmax = pwrcl_clk.c.fmax[pwrcl_clk.c.num_fmax - 1];
	apc1_fmax = perfcl_clk.c.fmax[perfcl_clk.c.num_fmax - 1];

	WARN(add_opp(&pwrcl_clk.c, &apc0_dev->dev, apc0_fmax),
		"Failed to add OPP levels for A53\n");
	WARN(add_opp(&perfcl_clk.c, &apc1_dev->dev, apc1_fmax),
		"Failed to add OPP levels for perf\n");

	/* One time print during bootup */
	pr_info("clock-cpu-8996: OPP tables populated.\n");
}

static int perfclspeedbin;

static int cpu_clock_8996_driver_probe(struct platform_device *pdev)
{
	int ret, cpu;
	unsigned long pwrclrate, perfclrate, cbfrate;
	int pvs_ver = 0;
	u32 pte_efuse;
	char perfclspeedbinstr[] = "qcom,perfcl-speedbinXX-vXX";

	pwrcl_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	perfcl_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;

	ret = cpu_clock_8996_resources_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "resources init failed\n");
		return ret;
	}

	ret = of_get_fmax_vdd_class(pdev, &pwrcl_clk.c,
				    "qcom,pwrcl-speedbin0-v0");
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for pwrcl\n");
		return ret;
	}

	pte_efuse = readl_relaxed(vbases[EFUSE_BASE]);
	perfclspeedbin = pte_efuse & 0x7;
	dev_info(&pdev->dev, "using perf speed bin %u and pvs_ver %d\n",
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

	ret = of_get_fmax_vdd_class(pdev, &cbf_hf_mux.c,
				    "qcom,cbf-speedbin0-v0");
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for cbf\n");
		return ret;
	}

	get_online_cpus();

	ret = of_msm_clock_register(pdev->dev.of_node, cpu_clocks_8996,
				    ARRAY_SIZE(cpu_clocks_8996));
	if (ret) {
		dev_err(&pdev->dev, "Unable to register CPU clocks.\n");
		return ret;
	}

	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(&cbf_hf_mux.c),
			"Failed to enable cbf clock.\n");
		WARN(clk_prepare_enable(logical_cpu_to_clk(cpu)),
			"Failed to enabled clock for cpu %d\n", cpu);
	}

	pwrclrate = clk_get_rate(&pwrcl_clk.c);
	perfclrate = clk_get_rate(&perfcl_clk.c);
	cbfrate = clk_get_rate(&cbf_hf_mux.c);

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
		ret = clk_set_rate(&cbf_hf_mux.c, cbfrate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on CBF.\n");
			return -EINVAL;
		}
	}

	/* Set low frequencies until thermal/cpufreq probe. */
	clk_set_rate(&pwrcl_clk.c, 768000000);
	clk_set_rate(&perfcl_clk.c, 300000000);
	clk_set_rate(&cbf_hf_mux.c, 595200000);

	/* Permanently enable the cluster PLLs */
	clk_prepare_enable(&perfcl_pll.c);
	clk_prepare_enable(&pwrcl_pll.c);
	clk_prepare_enable(&perfcl_alt_pll.c);
	clk_prepare_enable(&pwrcl_alt_pll.c);

	populate_opp_table(pdev);

	put_online_cpus();

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-clock-8996" },
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
#define AUX_BASE_PHY 0x09820050

#define CLK_CTL_OFFSET 0x44
#define AUTO_CLK_SEL_BIT BIT(8)
#define AUTO_CLK_SEL_ALWAYS_ON_MASK BM(5, 4)
#define AUTO_CLK_SEL_ALWAYS_ON_GPLL0_SEL (0x3 << 4)

#define HF_MUX_MASK 0x3
#define LF_MUX_MASK 0x3
#define LF_MUX_SHIFT 0x2
#define HF_MUX_SEL_EARLY_PLL 0x1
#define HF_MUX_SEL_LF_MUX 0x1
#define LF_MUX_SEL_ALT_PLL 0x1

int __init cpu_clock_8996_early_init(void)
{
	int ret = 0;
	void __iomem *auxbase;
	u32 regval;
	struct device_node *ofnode;

	ofnode = of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8996");
	if (!ofnode)
		return 0;

	pr_info("clock-cpu-8996: configuring clocks for the perf cluster\n");

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

	vbases[APC0_PLL_BASE] = vbases[APC0_BASE];
	vbases[APC1_PLL_BASE] = vbases[APC1_BASE];

	auxbase = ioremap(AUX_BASE_PHY, SZ_4K);
	if (!auxbase) {
		WARN(1, "Unable to ioremap aux base. Can't configure CPU clocks\n");
		ret = -ENOMEM;
		goto auxbase_fail;
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

	/* == Setup PLLs in FSM mode == */

	/* Disable all PLLs (we're already on GPLL0 for both clusters) */
	perfcl_alt_pll.c.ops->disable(&perfcl_alt_pll.c);
	pwrcl_alt_pll.c.ops->disable(&pwrcl_alt_pll.c);
	writel_relaxed(0x0, vbases[APC0_BASE] +
			(unsigned long)pwrcl_pll.mode_reg);
	writel_relaxed(0x0, vbases[APC1_BASE] +
			(unsigned long)perfcl_pll.mode_reg);

	/* Let PLLs disable before re-init'ing them */
	mb();

	/* Initialize all the PLLs */
	__variable_rate_pll_init(&perfcl_pll.c);
	__variable_rate_pll_init(&pwrcl_pll.c);
	__init_alpha_pll(&perfcl_alt_pll.c);
	__init_alpha_pll(&pwrcl_alt_pll.c);

	/* Set an appropriate rate on the perf cluster's PLLs */
	perfcl_pll.c.ops->set_rate(&perfcl_pll.c, 614400000);
	perfcl_alt_pll.c.ops->set_rate(&perfcl_alt_pll.c, 307200000);

	/* Set an appropriate rate on the power cluster's alternate PLL */
	pwrcl_alt_pll.c.ops->set_rate(&pwrcl_alt_pll.c, 307200000);

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

	/* Ensure write goes through before auto clock selection is enabled */
	mb();

	/* Wait for PLL(s) to lock */
	udelay(50);

	/* Enable auto clock selection for both clusters */
	regval = readl_relaxed(vbases[APC0_BASE] + CLK_CTL_OFFSET);
	regval |= AUTO_CLK_SEL_BIT;
	writel_relaxed(regval, vbases[APC0_BASE] + CLK_CTL_OFFSET);

	regval = readl_relaxed(vbases[APC1_BASE] + CLK_CTL_OFFSET);
	regval |= AUTO_CLK_SEL_BIT;
	writel_relaxed(regval, vbases[APC1_BASE] + CLK_CTL_OFFSET);

	/* Ensure write goes through before muxes are switched */
	mb();
	udelay(5);

	/* Switch the power cluster to use the primary PLL again */
	writel_relaxed(0x34, vbases[APC0_BASE] + MUX_OFFSET);

	/*
	 * One time print during boot - this is the earliest time
	 * that Linux configures the CPU clocks. It's critical for
	 * debugging that we know that this configuration completed,
	 * especially when debugging CPU hangs.
	 */
	pr_info("%s: finished CPU clock configuration\n", __func__);

	iounmap(auxbase);
auxbase_fail:
	iounmap(vbases[APC1_BASE]);
apc1_fail:
	iounmap(vbases[APC0_BASE]);
fail:
	return ret;
}
early_initcall(cpu_clock_8996_early_init);

MODULE_DESCRIPTION("CPU clock driver for msm8996");
MODULE_LICENSE("GPL v2");
