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
};

static void *vbases[NUM_BASES];

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

#define C0_PLL_MODE         0x0
#define C0_PLL_L_VAL        0x4
#define C0_PLL_ALPHA        0x8
#define C0_PLL_USER_CTL    0x10
#define C0_PLL_CONFIG_CTL  0x18
#define C0_PLL_STATUS      0x28
#define C0_PLL_TEST_CTL_LO 0x20

#define C0_PLLA_MODE        0x100
#define C0_PLLA_L_VAL       0x104
#define C0_PLLA_ALPHA       0x108
#define C0_PLLA_USER_CTL    0x110
#define C0_PLLA_CONFIG_CTL  0x118
#define C0_PLLA_STATUS      0x128
#define C0_PLLA_TEST_CTL_LO 0x120

#define C1_PLL_MODE         0x0
#define C1_PLL_L_VAL        0x4
#define C1_PLL_ALPHA        0x8
#define C1_PLL_USER_CTL    0x10
#define C1_PLL_CONFIG_CTL  0x18
#define C1_PLL_STATUS      0x28
#define C1_PLL_TEST_CTL_LO 0x20

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
#define CBF_PLL_STATUS      0x28
#define CBF_PLL_TEST_CTL_LO 0x30

#define GLB_CLK_DIAG	0x1C
#define MUX_OFFSET	0x40

DEFINE_EXT_CLK(xo_ao, NULL);
DEFINE_EXT_CLK(sys_apcsaux_clk_gcc, NULL);
DEFINE_FIXED_SLAVE_DIV_CLK(sys_apcsaux_clk, 2, &sys_apcsaux_clk_gcc.c);

static struct pll_clk perfcl_pll = {
	.mode_reg = (void __iomem *)C1_PLL_MODE,
	.l_reg = (void __iomem *)C1_PLL_L_VAL,
	.alpha_reg = (void __iomem *)C1_PLL_ALPHA,
	.config_reg = (void __iomem *)C1_PLL_USER_CTL,
	.status_reg = (void __iomem *)C1_PLL_MODE,
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
	},
	.min_rate =  600000000,
	.max_rate = 3000000000,
	.base = &vbases[APC1_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "perfcl_pll",
		.ops = &clk_ops_variable_rate_pll,
		VDD_DIG_FMAX_MAP1(LOW, 3000000000),
		CLK_INIT(perfcl_pll.c),
	},
};

static struct pll_clk pwrcl_pll = {
	.mode_reg = (void __iomem *)C0_PLL_MODE,
	.l_reg = (void __iomem *)C0_PLL_L_VAL,
	.alpha_reg = (void __iomem *)C0_PLL_ALPHA,
	.config_reg = (void __iomem *)C0_PLL_USER_CTL,
	.status_reg = (void __iomem *)C0_PLL_MODE,
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
	},
	.min_rate =  600000000,
	.max_rate = 3000000000,
	.base = &vbases[APC0_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "pwrcl_pll",
		.ops = &clk_ops_variable_rate_pll,
		VDD_DIG_FMAX_MAP1(LOW, 3000000000),
		CLK_INIT(pwrcl_pll.c),
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
	mux->en_mask = sel;

	/*
	 * Don't switch the mux if it isn't enabled.
	 * However, if this is a request to select the safe source
	 * do it unconditionally. This is to allow the safe source
	 * to be selected during frequency switches even if the mux
	 * is disabled (specifically on 8996 V1, the LFMUX may be
	 * disabled).
	 */
	if (!mux->c.count && sel != mux->safe_sel)
		return 0;

	__cpu_mux_set_sel(mux, mux->en_mask);

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
	__cpu_mux_set_sel(mux, mux->en_mask);
	return 0;
}

static void cpu_mux_disable(struct mux_clk *mux)
{
	__cpu_mux_set_sel(mux, mux->safe_sel);
}

static struct clk_mux_ops cpu_mux_ops = {
	.enable = cpu_mux_enable,
	.disable = cpu_mux_disable,
	.set_mux_sel = cpu_mux_set_sel,
	.get_mux_sel = cpu_mux_get_sel,
};

static struct mux_clk pwrcl_lf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &xo_ao.c,           0 },
		{ &pwrcl_pll_main.c,  1 },
		{ &sys_apcsaux_clk.c, 3 },
	),
	.en_mask = 3,
	.safe_parent = &sys_apcsaux_clk.c,
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
	.en_mask = 0,
	.safe_parent = &pwrcl_lf_mux.c,
	.safe_freq = 300000000,
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
		{ &xo_ao.c,           0 },
		{ &perfcl_pll_main.c, 1 },
		{ &sys_apcsaux_clk.c, 3 },
	),
	.safe_parent = &sys_apcsaux_clk.c,
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
	.safe_parent = &perfcl_lf_mux.c,
	.safe_freq = 300000000,
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

static int cpu_clk_8996_set_rate(struct clk *c, unsigned long rate)
{
	return clk_set_rate(c->parent, rate);
}

static struct clk_ops clk_ops_cpu_8996 = {
	.set_rate = cpu_clk_8996_set_rate,
	.round_rate = cpu_clk_8996_round_rate,
	.handoff = cpu_clk_8996_handoff,
};

DEFINE_VDD_REGS_INIT(vdd_pwrcl, 1);

static struct cpu_clk_8996 pwrcl_clk = {
	.cpu_reg_mask = 0x3,
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
	.c = {
		.parent = &perfcl_hf_mux.c,
		.dbg_name = "perfcl_clk",
		.ops = &clk_ops_cpu_8996,
		.vdd_class = &vdd_perfcl,
		CLK_INIT(perfcl_clk.c),
	},
};

DEFINE_FIXED_SLAVE_DIV_CLK(pwrcl_div_clk, 1, &pwrcl_clk.c);
DEFINE_FIXED_SLAVE_DIV_CLK(perfcl_div_clk, 1, &perfcl_clk.c);

#define APCS_ALIAS1_CORE_CBCR 0x58

static struct mux_clk pwrcl_debug_mux = {
	.offset = GLB_CLK_DIAG,
	.en_offset = APCS_ALIAS1_CORE_CBCR,
	.en_mask = 0x1,
	.ops = &mux_reg_ops,
	.mask = 0x3F,
	.shift = 12,
	MUX_REC_SRC_LIST(
		&pwrcl_div_clk.c,
	),
	MUX_SRC_LIST(
		{&pwrcl_div_clk.c, 0},
	),
	.base = &vbases[APC0_BASE],
	.c = {
		.dbg_name = "pwrcl_debug_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(pwrcl_debug_mux.c),
	},
};

#define APCS_ALIAS0_CORE_CBCR 0x58

static struct mux_clk perfcl_debug_mux = {
	.offset = GLB_CLK_DIAG,
	.en_offset = APCS_ALIAS0_CORE_CBCR,
	.en_mask = 0x1,
	.ops = &mux_reg_ops,
	.mask = 0x3F,
	.shift = 12,
	MUX_REC_SRC_LIST(
		&perfcl_div_clk.c,
	),
	MUX_SRC_LIST(
		{&perfcl_div_clk.c, 0},
	),
	.base = &vbases[APC1_BASE],
	.c = {
		.dbg_name = "perfcl_debug_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(perfcl_debug_mux.c),
	},
};

static struct mux_clk cpu_debug_mux = {
	.offset = 0x120,
	.ops = &cpu_mux_ops,
	.mask = 0x1,
	.shift = 0,
	MUX_SRC_LIST(
		{&pwrcl_debug_mux.c, 0},
		{&perfcl_debug_mux.c, 1},
	),
	MUX_REC_SRC_LIST(
		&pwrcl_debug_mux.c,
		&perfcl_debug_mux.c,
	),
	.base = &vbases[CBF_BASE],
	.c = {
		.dbg_name = "cpu_debug_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(cpu_debug_mux.c),
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
	.status_reg = (void __iomem *)CBF_PLL_MODE,
	.test_ctl_lo_reg = (void __iomem *)CBF_PLL_TEST_CTL_LO,
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
		.config_ctl_val = 0x000D6968,
		.test_ctl_lo_val = 0x00010000,
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

static struct mux_clk cbf_lf_mux = {
	.offset = CBF_MUX_OFFSET,
	MUX_SRC_LIST(
		{ &xo_ao.c, 0 },
	),
	.en_mask = 0,
	.safe_parent = &xo_ao.c,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 2,
	.base = &vbases[CBF_BASE],
	.c = {
		.dbg_name = "cbf_lf_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(cbf_lf_mux.c),
	},
};

DEFINE_VDD_REGS_INIT(vdd_cbf, 1);

static struct mux_clk cbf_hf_mux = {
	.offset = CBF_MUX_OFFSET,
	MUX_SRC_LIST(
		{ &cbf_lf_mux.c,      0 },
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

static struct clk_lookup cpu_clocks_8996[] = {
	CLK_LIST(pwrcl_clk),
	CLK_LIST(pwrcl_pll),
	CLK_LIST(pwrcl_pll_main),
	CLK_LIST(pwrcl_hf_mux),
	CLK_LIST(pwrcl_lf_mux),

	CLK_LIST(perfcl_clk),
	CLK_LIST(perfcl_pll),
	CLK_LIST(perfcl_pll_main),
	CLK_LIST(perfcl_hf_mux),
	CLK_LIST(perfcl_lf_mux),

	CLK_LIST(cbf_pll),
	CLK_LIST(cbf_hf_mux),
	CLK_LIST(cbf_lf_mux),

	CLK_LIST(xo_ao),
	CLK_LIST(sys_apcsaux_clk),

	CLK_LIST(pwrcl_debug_mux),
	CLK_LIST(perfcl_debug_mux),
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

int __init cpu_clock_8996_init_perfcl(void)
{
	int ret = 0;
	void __iomem *auxbase;
	u32 regval;

	pr_info("clock-cpu-8996: configuring clocks for the perf cluster\n");

	vbases[APC1_BASE] = ioremap(APC1_BASE_PHY, SZ_4K);
	if (!vbases[APC1_BASE]) {
		WARN(1, "Unable to ioremap perf mux base. Can't configure perf cluster clocks.\n");
		ret = -ENOMEM;
		goto fail;
	}

	auxbase = ioremap(AUX_BASE_PHY, SZ_4K);
	if (!auxbase) {
		WARN(1, "Unable to ioremap aux base. Can't set safe source freq.\n");
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

	/* Ensure write goes through before selecting the aux clock. */
	mb();
	udelay(5);

	/* Select GPLL0 for 300MHz for the perf cluster */
	writel_relaxed(0xC, vbases[APC1_BASE] + MUX_OFFSET);

	/* Ensure write goes through before CPUs are brought up. */
	mb();
	udelay(5);

	pr_cont("clock-cpu-8996: finished configuring perf cluster clocks.\n");

	iounmap(auxbase);
auxbase_fail:
	iounmap(vbases[APC1_BASE]);
fail:
	return ret;
}
early_initcall(cpu_clock_8996_init_perfcl);

MODULE_DESCRIPTION("CPU clock driver for msm8996");
MODULE_LICENSE("GPL v2");
