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
#include <linux/pm_qos.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-alpha-pll.h>

#include <dt-bindings/clock/msm-clocks-8994.h>
#include <dt-bindings/clock/msm-clocks-8992.h>

#include "clock.h"
#include "vdd-level-8994.h"

#define A53OFFSET (56)

enum {
	C0_PLL_BASE,
	C1_PLL_BASE,
	CCI_PLL_BASE,
	ALIAS0_GLB_BASE,
	ALIAS1_GLB_BASE,
	CCI_BASE,
	EFUSE_BASE,
	NUM_BASES
};

static char *base_names[] = {
	"c0_pll",
	"c1_pll",
	"cci_pll",
	"c0_mux",
	"c1_mux",
	"cci_mux",
	"efuse",
};

static void *vbases[NUM_BASES];
u32 cci_phys_base = 0xF9112000;
static void sanity_check_clock_tree(u32 regval, struct mux_clk *mux);

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

#define C0_PLL_MODE        0x0
#define C0_PLL_L_VAL       0x4
#define C0_PLL_ALPHA       0x8
#define C0_PLL_USER_CTL   0x10
#define C0_PLL_CONFIG_CTL 0x14
#define C0_PLL_STATUS     0x1C
#define C0_PLL_TEST_CTL_LO 0x20
#define C0_PLL_TEST_CTL_HI 0x24

#define C0_PLLA_MODE       0x40
#define C0_PLLA_L_VAL      0x44
#define C0_PLLA_ALPHA      0x48
#define C0_PLLA_USER_CTL   0x50
#define C0_PLLA_CONFIG_CTL 0x54
#define C0_PLLA_STATUS     0x5C
#define C0_PLLA_TEST_CTL_LO 0x60
#define C0_PLLA_TEST_CTL_HI 0x64

#define C1_PLL_MODE        0x0
#define C1_PLL_L_VAL       0x4
#define C1_PLL_ALPHA       0x8
#define C1_PLL_USER_CTL   0x10
#define C1_PLL_CONFIG_CTL 0x14
#define C1_PLL_STATUS     0x1C
#define C1_PLL_TEST_CTL_LO 0x20
#define C1_PLL_TEST_CTL_HI 0x24

#define C1_PLLA_MODE       0x40
#define C1_PLLA_L_VAL      0x44
#define C1_PLLA_ALPHA      0x48
#define C1_PLLA_USER_CTL   0x50
#define C1_PLLA_CONFIG_CTL 0x54
#define C1_PLLA_STATUS     0x5C
#define C1_PLLA_TEST_CTL_LO 0x60
#define C1_PLLA_TEST_CTL_HI 0x64

#define GLB_CLK_DIAG	0x1C
#define MUX_OFFSET	0x54

/* 8994 V1 clock tree - Note that aux source is 600 Mhz, not 300.
 *  ___________		            ____
 * |	       |		   |    \
 * |	       |==================>|3   |
 * | a5*c_pll0 |		   |    |
 * |    early  |	==========>|2   |=======================> CPU
 * |___________|	|	   |    |
 *			|  =======>|1   |
 *			|  |       |    |
 *  ____________	|  |   ===>|0   |
 * | GPLL0(aux) |========  |   |   |___/
 * |_(600 Mhz)__|	   |   |
 *			   |   |
 *  ___________	           |   |
 * |	       |	   |   |
 * |	       |	   |   |
 * | a5*c_pll1 |============   |
 * |  early    |	       |
 * |___________|	       |
 *			     <=====================
 *						  ^
 *						  |
 *						  |
 *  ___________			    ____	  |
 * |	       |     [div1/div2]   |    \	  |
 * |	       |==================>|2   |	  |
 * | a5*c_pll0 |		   |    |	  |
 * |   main    |		   |    |	  |
 * | (early/2) |	==========>|0   |==========>
 * |___________|	|	   |    |
 *			|    =====>|1   |
 *			|    |     |    |
 *  ___________		|    |     |___/
 * |	       |	|    |
 * |    CXO    |=========    |
 * |___________|	     |
 *			     |
 *  ___________	             |
 * |	       |	     |
 * |	       | [div1/div2] |
 * | a5*c_pll1 |==============
 * |    main   |
 * | (early/2) |
 * |___________|
 *
 */

DEFINE_CLK_DUMMY(a53_safe_parent, 199200000);
DEFINE_CLK_DUMMY(a57_safe_parent, 199200000);

DEFINE_FIXED_SLAVE_DIV_CLK(a53_safe_clk, 1, &a53_safe_parent.c);
DEFINE_FIXED_SLAVE_DIV_CLK(a57_safe_clk, 1, &a57_safe_parent.c);

DEFINE_EXT_CLK(xo_ao, NULL);
DEFINE_EXT_CLK(sys_apcsaux_clk, NULL);

static bool msm8994_v2;
static bool msm8992;

static struct pll_clk a57_pll0 = {
	.mode_reg = (void __iomem *)C1_PLL_MODE,
	.l_reg = (void __iomem *)C1_PLL_L_VAL,
	.alpha_reg = (void __iomem *)C1_PLL_ALPHA,
	.config_reg = (void __iomem *)C1_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)C1_PLL_CONFIG_CTL,
	.status_reg = (void __iomem *)C1_PLL_MODE,
	.alt_status_reg = (void __iomem *)C1_PLL_STATUS,
	.test_ctl_lo_reg = (void __iomem *)C1_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)C1_PLL_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.pre_div_masked = 0x0,
		.config_ctl_val = 0x000D6968,
		.test_ctl_lo_val = 0x00010000,
	},
	.min_rate = 1209600000,
	.max_rate = 1996800000,
	.base = &vbases[C1_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "a57_pll0",
		.ops = &clk_ops_variable_rate_pll,
		VDD_DIG_FMAX_MAP2(LOW, 1593600000, NOMINAL, 1996800000),
		CLK_INIT(a57_pll0.c),
	},
};

static struct pll_clk a57_pll1 = {
	.mode_reg = (void __iomem *)C1_PLLA_MODE,
	.l_reg = (void __iomem *)C1_PLLA_L_VAL,
	.alpha_reg = (void __iomem *)C1_PLLA_ALPHA,
	.config_reg = (void __iomem *)C1_PLLA_USER_CTL,
	.config_ctl_reg = (void __iomem *)C1_PLLA_CONFIG_CTL,
	.status_reg = (void __iomem *)C1_PLLA_MODE,
	.alt_status_reg = (void __iomem *)C1_PLLA_STATUS,
	.test_ctl_lo_reg = (void __iomem *)C1_PLLA_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)C1_PLLA_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.pre_div_masked = 0x0,
		.config_ctl_val = 0x000D6968,
		.test_ctl_lo_val = 0x00010000,
	},
	/* Necessary since we'll be setting a rate before handoff on V1 */
	.src_rate = 19200000,
	.min_rate = 1209600000,
	.max_rate = 1996800000,
	.base = &vbases[C1_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "a57_pll1",
		.ops = &clk_ops_variable_rate_pll,
		VDD_DIG_FMAX_MAP2(LOW, 1593600000, NOMINAL, 1996800000),
		CLK_INIT(a57_pll1.c),
	},
};

static struct pll_clk a53_pll0 = {
	.mode_reg = (void __iomem *)C0_PLL_MODE,
	.l_reg = (void __iomem *)C0_PLL_L_VAL,
	.alpha_reg = (void __iomem *)C0_PLL_ALPHA,
	.config_reg = (void __iomem *)C0_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)C0_PLL_CONFIG_CTL,
	.status_reg = (void __iomem *)C0_PLL_MODE,
	.alt_status_reg = (void __iomem *)C0_PLL_STATUS,
	.test_ctl_lo_reg = (void __iomem *)C0_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)C0_PLL_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.pre_div_masked = 0x0,
		.config_ctl_val = 0x000D6968,
		.test_ctl_lo_val = 0x00010000,
	},
	.min_rate = 1209600000,
	.max_rate = 1996800000,
	.base = &vbases[C0_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "a53_pll0",
		.ops = &clk_ops_variable_rate_pll,
		VDD_DIG_FMAX_MAP2(LOW, 1593600000, NOMINAL, 1996800000),
		CLK_INIT(a53_pll0.c),
	},
};

static struct pll_clk a53_pll1 = {
	.mode_reg = (void __iomem *)C0_PLLA_MODE,
	.l_reg = (void __iomem *)C0_PLLA_L_VAL,
	.alpha_reg = (void __iomem *)C0_PLLA_ALPHA,
	.config_reg = (void __iomem *)C0_PLLA_USER_CTL,
	.config_ctl_reg = (void __iomem *)C0_PLLA_CONFIG_CTL,
	.status_reg = (void __iomem *)C0_PLLA_MODE,
	.alt_status_reg = (void __iomem *)C0_PLLA_STATUS,
	.test_ctl_lo_reg = (void __iomem *)C0_PLLA_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)C0_PLLA_TEST_CTL_HI,
	.pgm_test_ctl_enable = true,
	.masks = {
		.pre_div_mask = BIT(12),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.apc_pdn_mask = BIT(24),
		.lock_mask = BIT(31),
	},
	.vals = {
		.pre_div_masked = 0x0,
		.config_ctl_val = 0x000D6968,
		.test_ctl_lo_val = 0x00010000,
	},
	/* Necessary since we'll be setting a rate before handoff on V1 */
	.src_rate = 19200000,
	.min_rate = 1209600000,
	.max_rate = 1996800000,
	.base = &vbases[C0_PLL_BASE],
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "a53_pll1",
		.ops = &clk_ops_variable_rate_pll,
		VDD_DIG_FMAX_MAP2(LOW, 1593600000, NOMINAL, 1996800000),
		CLK_INIT(a53_pll1.c),
	},
};

static DEFINE_SPINLOCK(mux_reg_lock);

static int cpudiv_get_div(struct div_clk *divclk)
{
	u32 regval;

	if (divclk->priv)
		regval = scm_io_read(*(u32 *)divclk->priv + divclk->offset);
	else
		regval = readl_relaxed(*divclk->base + divclk->offset);

	regval &= (divclk->mask << divclk->shift);
	regval >>= divclk->shift;

	return regval + 1;
}

static void __cpudiv_set_div(struct div_clk *divclk, int div)
{
	u32 regval;
	unsigned long flags;

	spin_lock_irqsave(&mux_reg_lock, flags);

	if (divclk->priv)
		regval = scm_io_read(*(u32 *)divclk->priv + divclk->offset);
	else
		regval = readl_relaxed(*divclk->base + divclk->offset);

	regval &= ~(divclk->mask << divclk->shift);
	regval |= ((div - 1) & divclk->mask) << divclk->shift;

	if (divclk->priv)
		scm_io_write(*(u32 *)divclk->priv + divclk->offset, regval);
	else
		writel_relaxed(regval, *divclk->base + divclk->offset);

	/* Ensure switch request goes through before returning */
	mb();
	spin_unlock_irqrestore(&mux_reg_lock, flags);
}

static int cpudiv_set_div(struct div_clk *divclk, int div)
{
	unsigned long flags;

	spin_lock_irqsave(&divclk->c.lock, flags);

	/*
	 * Cache the divider here. If the divider is re-enabled before data.div
	 * is updated, we need to restore the divider to the latest value.
	 */
	divclk->data.cached_div = div;

	/*
	 * Only set the divider if the clock is enabled. If the rate of the
	 * clock is being changed when the clock is off, the clock may be
	 * at a lower rate than requested, which is OK, since the block being
	 * clocked is "offline" or the clock output is off. This works because
	 * mux_div_disable sets the max possible divider.
	 */
	if (divclk->c.count)
		__cpudiv_set_div(divclk, div);

	spin_unlock_irqrestore(&divclk->c.lock, flags);

	return 0;
}

static int cpudiv_enable(struct div_clk *divclk)
{
	__cpudiv_set_div(divclk, divclk->data.cached_div);
	return 0;
}

static void cpudiv_disable(struct div_clk *divclk)
{
	__cpudiv_set_div(divclk, divclk->data.max_div);
}

static struct clk_div_ops cpu_div_ops = {
	.set_div = cpudiv_set_div,
	.get_div = cpudiv_get_div,
	.enable = cpudiv_enable,
	.disable = cpudiv_disable,
};

DEFINE_FIXED_DIV_CLK(a53_pll0_main, 2, &a53_pll0.c);
DEFINE_FIXED_DIV_CLK(a57_pll0_main, 2, &a57_pll0.c);
DEFINE_FIXED_DIV_CLK(a53_pll1_main, 2, &a53_pll1.c);
DEFINE_FIXED_DIV_CLK(a57_pll1_main, 2, &a57_pll1.c);

#define DEFINE_LF_MUX_DIV(name, _base, _parent)	\
	static struct div_clk name = {		\
		.data = {			\
			.min_div = 1,		\
			.max_div = 2,		\
		},				\
		.ops = &cpu_div_ops,		\
		.base = &vbases[_base],	\
		.offset = MUX_OFFSET,		\
		.mask = 0x1,			\
		.shift = 5,			\
		.c = {				\
			.parent = _parent,	\
			.flags = CLKFLAG_NO_RATE_CACHE, \
			.dbg_name = #name,	\
			.ops = &clk_ops_div,	\
			CLK_INIT(name.c),	\
		},				\
	};

DEFINE_LF_MUX_DIV(a53_lf_mux_pll0_div, ALIAS0_GLB_BASE,
		  &a53_pll0_main.c);
DEFINE_LF_MUX_DIV(a57_lf_mux_pll0_div, ALIAS1_GLB_BASE,
		  &a57_pll0_main.c);

static struct mux_clk a53_lf_mux;
static struct mux_clk a53_hf_mux;
static struct mux_clk a57_lf_mux;
static struct mux_clk a57_hf_mux;

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

	sanity_check_clock_tree(regval, mux);

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
	 * Don't switch the mux if it isn't enabled. However, if this is a
	 * request to select the safe source or low power source do it
	 * unconditionally. This is to allow the safe source to be selected
	 * during frequency switches even if the mux is disabled (specifically
	 * on 8994 V1, the LFMUX may be disabled).
	 */
	if (!mux->c.count && sel != mux->low_power_sel)
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
	__cpu_mux_set_sel(mux, mux->low_power_sel);
}

static struct clk_mux_ops cpu_mux_ops = {
	.enable = cpu_mux_enable,
	.disable = cpu_mux_disable,
	.set_mux_sel = cpu_mux_set_sel,
	.get_mux_sel = cpu_mux_get_sel,
};

static struct mux_clk a53_lf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &a53_lf_mux_pll0_div.c, 2 },
		{ &a53_safe_clk.c,        1 },
		{ &xo_ao.c,		  0 },
	),
	.safe_parent = &a53_safe_clk.c,
	.low_power_sel = 1,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 1,
	.base = &vbases[ALIAS0_GLB_BASE],
	.c = {
		.dbg_name = "a53_lf_mux",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a53_lf_mux.c),
	},
};

static struct mux_clk a53_hf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &a53_pll0.c,	      3 },
		{ &a53_lf_mux.c,      0 },
		{ &sys_apcsaux_clk.c, 2 },
	),
	.safe_parent = &a53_lf_mux.c,
	.low_power_sel = 0,
	.safe_freq = 199200000,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 3,
	.base = &vbases[ALIAS0_GLB_BASE],
	.c = {
		.dbg_name = "a53_hf_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a53_hf_mux.c),
	},
};

static struct mux_clk a57_lf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &a57_lf_mux_pll0_div.c, 2 },
		{ &a57_safe_clk.c,        1 },
		{ &xo_ao.c,               0 },
	),
	.safe_parent = &a57_safe_clk.c,
	.low_power_sel = 1,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 1,
	.base = &vbases[ALIAS1_GLB_BASE],
	.c = {
		.dbg_name = "a57_lf_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(a57_lf_mux.c),
	},
};

static struct mux_clk a57_hf_mux = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &a57_pll0.c,	      3 },
		{ &a57_lf_mux.c,      0 },
		{ &sys_apcsaux_clk.c, 2 },
	),
	.safe_parent = &a57_lf_mux.c,
	.low_power_sel = 0,
	.safe_freq = 199200000,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 3,
	.base = &vbases[ALIAS1_GLB_BASE],
	.c = {
		.dbg_name = "a57_hf_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a57_hf_mux.c),
	},
};

/* === 8994 V2 clock tree begins here === */

static int plldiv_get_div(struct div_clk *divclk)
{
	u32 regval;
	int div = 0;

	regval = readl_relaxed(*divclk->base + divclk->offset);
	regval &= (divclk->mask << divclk->shift);
	regval >>= divclk->shift;

	switch (regval) {
	case  0:
		div = 1;
	break;
	case 1:
		div = 2;
	break;
	case 3:
		div = 4;
	break;
	default:
		pr_err("Invalid divider value programmed for %s\n",
			divclk->c.dbg_name);
		BUG();
	break;
	};

	return div;
}

static void __plldiv_set_div(struct div_clk *divclk, int div)
{
	u32 regval;
	unsigned long flags;
	u32 program_div = 0;

	spin_lock_irqsave(&mux_reg_lock, flags);
	switch (div) {
	case 2:
		program_div = 1;
	break;
	case 4:
		program_div = 3;
	break;
	default:
		WARN(1, "Unknown divider for %s\n", divclk->c.dbg_name);
		program_div = 3;
	break;
	};

	regval = readl_relaxed(*divclk->base + divclk->offset);
	regval &= ~(divclk->mask << divclk->shift);
	regval |= (program_div & divclk->mask) << divclk->shift;
	writel_relaxed(regval, *divclk->base + divclk->offset);

	/* Ensure switch request goes through before returning */
	mb();
	udelay(5);
	spin_unlock_irqrestore(&mux_reg_lock, flags);
}

static int plldiv_set_div(struct div_clk *divclk, int div)
{
	/*
	 * Only set the divider if the clock is disabled.
	 */
	if (!divclk->c.count)
		__plldiv_set_div(divclk, div);
	else
		WARN(1, "Attempting to set divider when PLL may be on!\n");

	return 0;
}

static struct clk_div_ops pll_div_ops = {
	.set_div = plldiv_set_div,
	.get_div = plldiv_get_div,
};

#define DEFINE_PLL_MUX_DIV(name, _base, _parent, _offset)\
	static struct div_clk name = {		\
		.data = {			\
			.min_div = 2,		\
			.max_div = 4,		\
			.skip_odd_div = true,	\
		},				\
		.ops = &pll_div_ops,		\
		.base = &vbases[_base],		\
		.offset = _offset,		\
		.mask = 0x3,			\
		.shift = 8,			\
		.c = {				\
			.parent = _parent,	\
			.flags = CLKFLAG_NO_RATE_CACHE, \
			.dbg_name = #name,	\
			.ops = &clk_ops_div,	\
			CLK_INIT(name.c),	\
		},				\
	};

DEFINE_PLL_MUX_DIV(a53_pll0div_main, C0_PLL_BASE, &a53_pll0.c, C0_PLL_USER_CTL);
DEFINE_PLL_MUX_DIV(a53_pll1div_main, C0_PLL_BASE, &a53_pll1.c,
		   C0_PLLA_USER_CTL);
DEFINE_PLL_MUX_DIV(a57_pll0div_main, C1_PLL_BASE, &a57_pll0.c, C1_PLL_USER_CTL);
DEFINE_PLL_MUX_DIV(a57_pll1div_main, C1_PLL_BASE, &a57_pll1.c,
		   C1_PLLA_USER_CTL);

static struct mux_clk a53_lf_mux_v2 = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &xo_ao.c,           0 },
		{ &a53_pll1div_main.c,   1 },
		{ &a53_pll0div_main.c,   2 },
		{ &sys_apcsaux_clk.c, 3 },
	),
	.en_mask = 3,
	.low_power_sel = 3,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 1,
	.try_new_parent = true,
	.try_get_rate = true,
	.base = &vbases[ALIAS0_GLB_BASE],
	.c = {
		.dbg_name = "a53_lf_mux_v2",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a53_lf_mux_v2.c),
	},
};

static struct mux_clk a53_hf_mux_v2 = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &a53_pll1.c,       1 },
		{ &a53_pll0.c,       3 },
		{ &a53_lf_mux_v2.c,  0 },
	),
	.en_mask = 0,
	.low_power_sel = 0,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 3,
	.try_new_parent = true,
	.try_get_rate = true,
	.base = &vbases[ALIAS0_GLB_BASE],
	.c = {
		.dbg_name = "a53_hf_mux_v2",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a53_hf_mux_v2.c),
	},
};

static struct mux_clk a57_lf_mux_v2 = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &xo_ao.c,            0 },
		{ &a57_pll1div_main.c, 1 },
		{ &a57_pll0div_main.c, 2 },
		{ &sys_apcsaux_clk.c,  3 },
	),
	.low_power_sel = 3,
	.en_mask = 3,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 1,
	.try_new_parent = true,
	.try_get_rate = true,
	.base = &vbases[ALIAS1_GLB_BASE],
	.c = {
		.dbg_name = "a57_lf_mux_v2",
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a57_lf_mux_v2.c),
	},
};

static struct mux_clk a57_hf_mux_v2 = {
	.offset = MUX_OFFSET,
	MUX_SRC_LIST(
		{ &a57_lf_mux_v2.c,  0 },
		{ &a57_pll1.c,       1 },
		{ &a57_pll0.c,       3 },
	),
	.low_power_sel = 0,
	.en_mask = 0,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 3,
	.try_new_parent = true,
	.try_get_rate = true,
	.base = &vbases[ALIAS1_GLB_BASE],
	.c = {
		.dbg_name = "a57_hf_mux_v2",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a57_hf_mux_v2.c),
	},
};

struct cpu_clk_8994 {
	u32 cpu_reg_mask;
	cpumask_t cpumask;
	bool hw_low_power_ctrl;
	struct clk c;
	struct pm_qos_request req;
};

static inline struct cpu_clk_8994 *to_cpu_clk_8994(struct clk *c)
{
	return container_of(c, struct cpu_clk_8994, c);
}

static enum handoff cpu_clk_8994_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);

	return HANDOFF_DISABLED_CLK;
}

static long cpu_clk_8994_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long fmax = c->fmax[c->num_fmax - 1];
	unsigned long fmin = c->fmax[1];
	int i = 1;

	if (rate <= fmin)
		return fmin;
	if (rate >= fmax)
		return fmax;
	while ((c->fmax[i++] < rate) && (i < c->num_fmax))
		;

	return c->fmax[i-1];
}

static void do_nothing(void *unused) { }

#define CPU_LATENCY_NO_L2_PC_US (800 - 1)

static int cpu_clk_8994_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	struct cpu_clk_8994 *cpuclk = to_cpu_clk_8994(c);
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;

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
				   CPU_LATENCY_NO_L2_PC_US);

		ret = smp_call_function_any(&cpuclk->cpumask, do_nothing,
						NULL, 1);
	}

	ret = clk_set_rate(c->parent, rate);

	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);

	return ret;
}

static struct clk_ops clk_ops_cpu_8994 = {
	.set_rate = cpu_clk_8994_set_rate,
	.round_rate = cpu_clk_8994_round_rate,
	.handoff = cpu_clk_8994_handoff,
};

DEFINE_VDD_REGS_INIT(vdd_a53, 1);

static struct cpu_clk_8994 a53_clk = {
	.cpu_reg_mask = 0x3,
	.c = {
		.parent = &a53_hf_mux.c,
		.dbg_name = "a53_clk",
		.ops = &clk_ops_cpu_8994,
		.vdd_class = &vdd_a53,
		CLK_INIT(a53_clk.c),
	},
};

DEFINE_VDD_REGS_INIT(vdd_a57, 1);

static struct cpu_clk_8994 a57_clk = {
	.cpu_reg_mask = 0x103,
	.c = {
		.parent = &a57_hf_mux.c,
		.dbg_name = "a57_clk",
		.ops = &clk_ops_cpu_8994,
		.vdd_class = &vdd_a57,
		CLK_INIT(a57_clk.c),
	},
};

#define LFMUX_MASK 0x6
#define HFMUX_MASK 0x18
#define LFDIV_MASK 0x20

#define LFMUX_SHIFT 1
#define HFMUX_SHIFT 3

#define LFMUX_SEL 0
#define PLL0_MAIN_SEL 2
#define PLL1_MAIN_SEL 1
#define PLL0_EARLY_SEL 3
#define PLL1_EARLY_SEL 1
#define AUX_CLK_SEL 3

void sanity_check_clock_tree(u32 muxval, struct mux_clk *mux)
{
	int level;
	u32 hfmux_sel = (muxval & HFMUX_MASK) >> HFMUX_SHIFT;
	u32 lfmux_sel = (muxval & LFMUX_MASK) >> LFMUX_SHIFT;
	int div = 0;
	void *base = NULL;
	unsigned long rate;
	struct clk *c;
	int cur_uv, req_uv;
	int *uv;

	if (!(msm8994_v2 || msm8992))
		return;

	if (mux->base == &vbases[ALIAS0_GLB_BASE]) {
		base = vbases[C0_PLL_BASE];
		c = &a53_clk.c;
	}
	if (mux->base == &vbases[ALIAS1_GLB_BASE]) {
		base = vbases[C1_PLL_BASE];
		c = &a57_clk.c;
	}

	if (!base)
		return;

	uv = c->vdd_class->vdd_uv;
	level = c->vdd_class->cur_level;

	/* Possibly hotplugged out */
	if (!level || !uv[level])
		return;

	switch (hfmux_sel) {
	case LFMUX_SEL:
		switch (lfmux_sel) {
		case PLL0_MAIN_SEL:
			rate = readl_relaxed(base + C0_PLL_L_VAL);
			div = readl_relaxed(base + C0_PLL_USER_CTL);
			div &= 0x300;
			div >>= 8;
			if (div == 1)
				div = 2;
			else if (div == 3)
				div = 4;
			else
				WARN(1, "bad div on %s pll0\n", c->dbg_name);
			rate *= xo_ao.c.rate;
			rate /= div;
		break;

		case PLL1_MAIN_SEL:
			rate = readl_relaxed(base + C0_PLLA_L_VAL);
			div = readl_relaxed(base + C0_PLLA_USER_CTL);
			div &= 0x300;
			div >>= 8;
			if (div == 1)
				div = 2;
			else if (div == 3)
				div = 4;
			else
				WARN(1, "bad div on %s pll1\n", c->dbg_name);
			rate *= xo_ao.c.rate;
			rate /= div;
		break;

		case AUX_CLK_SEL:
			rate = sys_apcsaux_clk.c.rate;
		break;
		};
	break;
	case PLL0_EARLY_SEL:
		rate = readl_relaxed(base + C0_PLL_L_VAL);
		rate *= xo_ao.c.rate;
	break;
	case PLL1_EARLY_SEL:
		rate = readl_relaxed(base + C0_PLLA_L_VAL);
		rate *= xo_ao.c.rate;
	break;
	};

	/* One regulator */
	cur_uv = uv[level];
	req_uv = uv[find_vdd_level(c, rate)];

	if (cur_uv < req_uv) {
		pr_err("%s: rate is %lu, uv is %d, req uv is %d\n", c->dbg_name,
			rate, cur_uv, req_uv);
		BUG();
	}
}

DEFINE_FIXED_SLAVE_DIV_CLK(a53_div_clk, 1, &a53_clk.c);
DEFINE_FIXED_SLAVE_DIV_CLK(a57_div_clk, 1, &a57_clk.c);

static struct div_clk cci_clk;

#define APCS_ALIAS1_CORE_CBCR 0x58

static struct mux_clk a53_debug_mux = {
	.offset = GLB_CLK_DIAG,
	.en_offset = APCS_ALIAS1_CORE_CBCR,
	.en_mask = 0x1,
	.ops = &mux_reg_ops,
	.mask = 0x3F,
	.shift = 12,
	MUX_REC_SRC_LIST(
		&a53_div_clk.c,
	),
	MUX_SRC_LIST(
		{&a53_div_clk.c, 0},
		{&cci_clk.c,     1},
	),
	.base = &vbases[ALIAS0_GLB_BASE],
	.c = {
		.dbg_name = "a53_debug_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a53_debug_mux.c),
	},
};

#define APCS_ALIAS0_CORE_CBCR 0x58

static struct mux_clk a57_debug_mux = {
	.offset = GLB_CLK_DIAG,
	.en_offset = APCS_ALIAS0_CORE_CBCR,
	.en_mask = 0x1,
	.ops = &mux_reg_ops,
	.mask = 0x3F,
	.shift = 12,
	MUX_REC_SRC_LIST(
		&a57_div_clk.c,
	),
	MUX_SRC_LIST(
		{&a57_div_clk.c, 0},
	),
	.base = &vbases[ALIAS1_GLB_BASE],
	.c = {
		.dbg_name = "a57_debug_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(a57_debug_mux.c),
	},
};

static struct mux_clk cpu_debug_mux = {
	.offset = 0x120,
	.ops = &cpu_mux_ops,
	.mask = 0x1,
	.shift = 0,
	MUX_SRC_LIST(
		{&a53_debug_mux.c, 0},
		{&a57_debug_mux.c, 1},
	),
	MUX_REC_SRC_LIST(
		&a53_debug_mux.c,
		&a57_debug_mux.c,
	),
	.priv = &cci_phys_base,
	.base = &vbases[CCI_BASE],
	.c = {
		.dbg_name = "cpu_debug_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(cpu_debug_mux.c),
	},
};

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node = of_get_cpu_node(cpu, NULL);
	u32 reg;

	if (cpu_node && !of_property_read_u32(cpu_node, "reg", &reg)) {
		if ((reg | a53_clk.cpu_reg_mask) == a53_clk.cpu_reg_mask)
			return &a53_clk.c;
		if ((reg | a57_clk.cpu_reg_mask) == a57_clk.cpu_reg_mask)
			return &a57_clk.c;
	}

	return NULL;
}

static struct alpha_pll_masks alpha_pll_masks_20nm_p = {
	.lock_mask = BIT(31),
	.update_mask = BIT(22),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xF,
	.post_div_mask = 0xF00,
};

static struct alpha_pll_vco_tbl alpha_pll_vco_20nm_p[] = {
	VCO(2,  500000000, 1000000000),
};

static struct alpha_pll_clk cci_pll = {
	.masks = &alpha_pll_masks_20nm_p,
	.base = &vbases[CCI_PLL_BASE],
	.vco_tbl = alpha_pll_vco_20nm_p,
	.num_vco = ARRAY_SIZE(alpha_pll_vco_20nm_p),
	.enable_config = 0x9, /* Main and early outputs */
	.post_div_config = 0x100, /* Div-2 */
	.c = {
		.parent = &xo_ao.c,
		.dbg_name = "cci_pll",
		.ops = &clk_ops_alpha_pll,
		VDD_DIG_FMAX_MAP1(LOW, 1000000000),
		CLK_INIT(cci_pll.c),
	},
};

DEFINE_FIXED_DIV_CLK(cci_pll_main, 2, &cci_pll.c);

#define CCI_MUX_OFFSET 0x11C

static struct mux_clk cci_lf_mux = {
	.offset = CCI_MUX_OFFSET,
	MUX_SRC_LIST(
		{ &sys_apcsaux_clk.c, 1 },
		{ &xo_ao.c, 0 },
	),
	.en_mask = 1,
	.safe_parent = &sys_apcsaux_clk.c,
	.low_power_sel = 1,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 1,
	.priv = &cci_phys_base,
	.base = &vbases[CCI_BASE],
	.c = {
		.dbg_name = "cci_lf_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(cci_lf_mux.c),
	},
};

static struct mux_clk cci_hf_mux = {
	.offset = CCI_MUX_OFFSET,
	MUX_SRC_LIST(
		{ &cci_lf_mux.c,   1 },
		{ &cci_pll.c,      3 },
		{ &cci_pll_main.c, 2 },
	),
	.en_mask = 1,
	.safe_parent = &cci_lf_mux.c,
	.low_power_sel = 1,
	.safe_freq = 600000000,
	.ops = &cpu_mux_ops,
	.mask = 0x3,
	.shift = 3,
	.priv = &cci_phys_base,
	.base = &vbases[CCI_BASE],
	.c = {
		.dbg_name = "cci_hf_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(cci_hf_mux.c),
	},
};

DEFINE_VDD_REGS_INIT(vdd_cci, 1);

static struct div_clk cci_clk = {
	.data = {
		.min_div = 1,
		.max_div = 4,
	},
	.safe_freq = 600000000,
	.ops = &cpu_div_ops,
	.base = &vbases[CCI_BASE],
	.offset = CCI_MUX_OFFSET,
	.mask = 0x3,
	.shift = 5,
	.priv = &cci_phys_base,
	.c = {
		.parent = &cci_hf_mux.c,
		.vdd_class = &vdd_cci,
		.dbg_name = "cci_clk",
		.ops = &clk_ops_div,
		CLK_INIT(cci_clk.c),
	},
};

static struct clk_lookup cpu_clocks_8994[] = {
	CLK_LIST(a53_pll0),
	CLK_LIST(a53_pll1),

	CLK_LIST(a57_pll0),
	CLK_LIST(a57_pll1),

	CLK_LIST(a53_hf_mux),
	CLK_LIST(a53_lf_mux),

	CLK_LIST(a57_hf_mux),
	CLK_LIST(a57_lf_mux),

	CLK_LIST(a53_lf_mux_pll0_div),
	CLK_LIST(a53_pll0_main),

	CLK_LIST(sys_apcsaux_clk),
	CLK_LIST(a53_clk),
	CLK_LIST(a57_clk),
	CLK_LIST(cci_clk),
	CLK_LIST(cci_pll),
	CLK_LIST(cci_hf_mux),
	CLK_LIST(cci_lf_mux),
	CLK_LIST(xo_ao),
	CLK_LIST(sys_apcsaux_clk),

	CLK_LIST(a53_debug_mux),
	CLK_LIST(a57_debug_mux),
	CLK_LIST(cpu_debug_mux),
};

/* List of clocks applicable to both 8994v2 and 8992 */

static struct clk_lookup cpu_clocks_8994_v2[] = {
	CLK_LIST(a53_clk),
	CLK_LIST(a53_pll0),
	CLK_LIST(a53_pll1),
	CLK_LIST(a53_pll0_main),
	CLK_LIST(a53_pll1_main),
	CLK_LIST(a53_hf_mux_v2),
	CLK_LIST(a53_lf_mux_v2),

	CLK_LIST(a57_clk),
	CLK_LIST(a57_pll0),
	CLK_LIST(a57_pll1),
	CLK_LIST(a57_pll0_main),
	CLK_LIST(a57_pll1_main),
	CLK_LIST(a57_hf_mux_v2),
	CLK_LIST(a57_lf_mux_v2),

	CLK_LIST(cci_clk),
	CLK_LIST(cci_pll),
	CLK_LIST(cci_hf_mux),
	CLK_LIST(cci_lf_mux),

	CLK_LIST(xo_ao),
	CLK_LIST(sys_apcsaux_clk),

	CLK_LIST(a53_debug_mux),
	CLK_LIST(a57_debug_mux),
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

static int cpu_clock_8994_resources_init(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *c;
	int i;

	for (i = 0; i < ARRAY_SIZE(base_names); i++) {

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   base_names[i]);
		if (!res) {
			dev_info(&pdev->dev,
				 "Unable to get platform resource for %s",
				 base_names[i]);
			return -ENOMEM;
		}

		vbases[i] = devm_ioremap(&pdev->dev, res->start,
					     resource_size(res));
		if (!vbases[i]) {
			dev_warn(&pdev->dev, "Unable to map in base %s\n",
				base_names[i]);
			return -ENOMEM;
		}
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the CX regulator");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	vdd_a53.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-a53");
	if (IS_ERR(vdd_a53.regulator[0])) {
		if (PTR_ERR(vdd_a53.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the a53 vreg\n");
		return PTR_ERR(vdd_a53.regulator[0]);
	}

	vdd_a57.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-a57");
	if (IS_ERR(vdd_a57.regulator[0])) {
		if (PTR_ERR(vdd_a57.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the a57 vreg\n");
		return PTR_ERR(vdd_a57.regulator[0]);
	}

	/* Leakage constraints disallow a turbo vote during bootup */
	vdd_a57.skip_handoff = true;

	vdd_cci.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-cci");
	if (IS_ERR(vdd_cci.regulator[0])) {
		if (PTR_ERR(vdd_cci.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the cci vreg\n");
		return PTR_ERR(vdd_cci.regulator[0]);
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
	sys_apcsaux_clk.c.parent = c;

	vdd_a53.use_max_uV = true;
	vdd_cci.use_max_uV = true;

	return 0;
}

static void perform_v1_fixup(void)
{
	u32 regval;

	/*
	 * For 8994 V1, always configure the secondary PLL to 200MHz.
	 * 0. Use the aux source first (done above).
	 * 1. Set the PLL divider on the main output to 0x4.
	 * 2. Set the divider on the PLL LF mux input to 0x2.
	 * 3. Configure the PLL to generate 1.5936 GHz.
	 */
	a53_pll1.c.ops->disable(&a53_pll1.c);

	/* Set the divider on the PLL1 input to the A53 LF MUX (div 2) */
	regval = readl_relaxed(vbases[ALIAS0_GLB_BASE] + MUX_OFFSET);
	regval |= BIT(6);
	writel_relaxed(regval, vbases[ALIAS0_GLB_BASE] + MUX_OFFSET);

	a53_pll1.c.ops->set_rate(&a53_pll1.c, 1593600000);

	a53_pll1.c.rate = 1593600000;

	/* Enable the A53 secondary PLL */
	a53_pll1.c.ops->enable(&a53_pll1.c);

	/* Select the "safe" parent on the secondary mux */
	__cpu_mux_set_sel(&a53_lf_mux, 1);
}

static long corner_to_voltage(unsigned long corner, struct device *dev)
{
	struct opp *oppl;
	long uv;

	rcu_read_lock();
	oppl = dev_pm_opp_find_freq_exact(dev, corner, true);
	rcu_read_unlock();
	if (IS_ERR_OR_NULL(oppl))
		return -EINVAL;

	rcu_read_lock();
	uv = dev_pm_opp_get_voltage(oppl);
	rcu_read_unlock();

	return uv;
}

static int add_opp(struct clk *c, struct device *cpudev, struct device *vregdev,
			unsigned long max_rate)
{
	unsigned long rate = 0;
	int level;
	long ret, uv, corner;
	bool use_voltages = false;
	struct opp *oppl;

	rcu_read_lock();
	/* Check if the regulator driver has already populated OPP tables */
	oppl = dev_pm_opp_find_freq_exact(vregdev, 2, true);
	rcu_read_unlock();
	if (!IS_ERR_OR_NULL(oppl))
		use_voltages = true;

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
		uv = corner = c->vdd_class->vdd_uv[level];
		/*
		 * If corner to voltage mapping is available, populate the OPP
		 * table with the voltages rather than corners.
		 */
		if (use_voltages) {
			uv = corner_to_voltage(corner, vregdev);
			if (uv < 0) {
				pr_warn("clock-cpu: no uv for corner %lu\n",
					 corner);
				return uv;
			}
			ret = dev_pm_opp_add(cpudev, rate, uv);
			if (ret) {
				pr_warn("clock-cpu: couldn't add OPP for %lu\n",
					 rate);
				return ret;
			}
		} else {
			/*
			 * Populate both CPU and regulator devices with the
			 * freq-to-corner OPP table to maintain backward
			 * compatibility.
			 */
			ret = dev_pm_opp_add(cpudev, rate, corner);
			if (ret) {
				pr_warn("clock-cpu: couldn't add OPP for %lu\n",
					rate);
				return ret;
			}
			ret = dev_pm_opp_add(vregdev, rate, corner);
			if (ret) {
				pr_warn("clock-cpu: couldn't add OPP for %lu\n",
					rate);
				return ret;
			}
		}
		if (rate >= max_rate)
			break;
	}

	return 0;
}

static void print_opp_table(int a53_cpu, int a57_cpu)
{
	struct opp *oppfmax, *oppfmin;
	unsigned long apc0_fmax = a53_clk.c.fmax[a53_clk.c.num_fmax - 1];
	unsigned long apc1_fmax = a57_clk.c.fmax[a57_clk.c.num_fmax - 1];
	unsigned long apc0_fmin = a53_clk.c.fmax[1];
	unsigned long apc1_fmin = a57_clk.c.fmax[1];

	rcu_read_lock();
	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a53_cpu), apc0_fmax,
					     true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a53_cpu), apc0_fmin,
					     true);
	/*
	 * One time information during boot. Important to know that this looks
	 * sane since it can eventually make its way to the scheduler.
	 */
	pr_info("clock_cpu: a53: OPP voltage for %lu: %ld\n", apc0_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a53: OPP voltage for %lu: %ld\n", apc0_fmax,
		dev_pm_opp_get_voltage(oppfmax));

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a57_cpu), apc1_fmax,
					     true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a57_cpu), apc1_fmin,
					     true);
	pr_info("clock_cpu: a57: OPP voltage for %lu: %lu\n", apc1_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a57: OPP voltage for %lu: %lu\n", apc1_fmax,
		dev_pm_opp_get_voltage(oppfmax));
	rcu_read_unlock();
}

static void populate_opp_table(struct platform_device *pdev)
{
	struct platform_device *apc0_dev, *apc1_dev;
	struct device_node *apc0_node, *apc1_node;
	unsigned long apc0_fmax, apc1_fmax;
	int cpu, a53_cpu, a57_cpu;

	apc0_node = of_parse_phandle(pdev->dev.of_node, "vdd-a53-supply", 0);
	apc1_node = of_parse_phandle(pdev->dev.of_node, "vdd-a57-supply", 0);
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

	apc0_fmax = a53_clk.c.fmax[a53_clk.c.num_fmax - 1];
	apc1_fmax = a57_clk.c.fmax[a57_clk.c.num_fmax - 1];

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_clk.c) {
			a53_cpu = cpu;
			WARN(add_opp(&a53_clk.c, get_cpu_device(cpu),
				     &apc0_dev->dev, apc0_fmax),
				     "Failed to add OPP levels for A53\n");
		}
		if (logical_cpu_to_clk(cpu) == &a57_clk.c) {
			a57_cpu = cpu;
			WARN(add_opp(&a57_clk.c, get_cpu_device(cpu),
				     &apc1_dev->dev, apc1_fmax),
				     "Failed to add OPP levels for A57\n");
		}
	}

	/* One time print during bootup */
	pr_info("clock-cpu-8994: OPP tables populated (cpu %d and %d)\n",
		a53_cpu, a57_cpu);

	print_opp_table(a53_cpu, a57_cpu);
}

static void init_v2_data(void)
{
	a53_pll0.vals.config_ctl_val = 0x004D6968;
	a53_pll1.vals.config_ctl_val = 0x004D6968;
	a57_pll0.vals.config_ctl_val = 0x004D6968;
	a57_pll1.vals.config_ctl_val = 0x004D6968;
	a53_pll0.vals.test_ctl_hi_val = 0x1;
	a53_pll1.vals.test_ctl_hi_val = 0x1;
	a57_pll0.vals.test_ctl_hi_val = 0x1;
	a57_pll1.vals.test_ctl_hi_val = 0x1;
	a53_pll0.vals.test_ctl_lo_val = 0x80000000;
	a53_pll1.vals.test_ctl_lo_val = 0x80000000;
	a57_pll0.vals.test_ctl_lo_val = 0x80000000;
	a57_pll1.vals.test_ctl_lo_val = 0x80000000;
	a53_pll0.init_test_ctl = true;
	a53_pll1.init_test_ctl = true;
	a57_pll0.init_test_ctl = true;
	a57_pll1.init_test_ctl = true;
	a53_pll0.pgm_test_ctl_enable = false;
	a53_pll1.pgm_test_ctl_enable = false;
	a57_pll0.pgm_test_ctl_enable = false;
	a57_pll1.pgm_test_ctl_enable = false;
	a57_clk.c.parent = &a57_hf_mux_v2.c;
	a53_clk.c.parent = &a53_hf_mux_v2.c;
	a53_div_clk.data.min_div = 8;
	a53_div_clk.data.max_div = 8;
	a53_div_clk.data.div = 8;
	a57_div_clk.data.min_div = 8;
	a57_div_clk.data.max_div = 8;
	a57_div_clk.data.div = 8;
	a57_pll0.no_prepared_reconfig = true;
	a57_pll1.no_prepared_reconfig = true;
	a53_pll0.no_prepared_reconfig = true;
	a53_pll1.no_prepared_reconfig = true;
	a53_clk.hw_low_power_ctrl = true;
}

static int a57speedbin;
static int a53speedbin;
struct platform_device *cpu_clock_8994_dev;

/* Low power mux code begins here */
#define EVENT_WAIT_US 1
#define WAIT_IPI_HANDLER_BEGIN_US 50
#define LOW_POWER_IPI_WAIT_US 100
#define WARN_ON_SLOW_SYNC_EVENT_ITERS 500000

/*
 * Low power mux switch feature flag. Cannot be switched at runtime.
 * Set this on the kernel commandline.
 */
static int clk_low_power_mux_switch = 1;
module_param(clk_low_power_mux_switch, int, 0444);

enum {
	CSD_LF_MUX,
	CSD_HF_MUX,
	CSD_N,
};

struct clkcpu_8994_idle_data {
	struct call_single_data csd[CSD_N];
	spinlock_t idle_lock;
	spinlock_t *exit_idle_lock;
	bool idle;
	bool low_power_mux_switch;

	/* Debug */
	u64 ipi_sent_time;
	u64 ipi_notsent_time;
	u64 ipi_started_time;
	u64 ipi_exit_time;
	u64 idle_start_time;
	u64 idle_exit_time;
};

static DEFINE_SPINLOCK(a57_exit_idle_lock);

struct mux_priv_data {
	cpumask_t cpumask;
	spinlock_t *exit_idle_lock;
	int csd_idx;
};

static struct mux_priv_data a57_hf_mux_priv_data = {
	.exit_idle_lock = &a57_exit_idle_lock,
	.csd_idx = CSD_HF_MUX,
};
static struct mux_priv_data a57_lf_mux_priv_data = {
	.exit_idle_lock = &a57_exit_idle_lock,
	.csd_idx = CSD_LF_MUX,
};
static DEFINE_PER_CPU(struct clkcpu_8994_idle_data, idle_data_clk_8994);

static void do_low_power_poll(void *unused)
{
	int cpu = smp_processor_id();
	struct clkcpu_8994_idle_data *idle_data;

	idle_data = &per_cpu(idle_data_clk_8994, cpu);
	idle_data->ipi_started_time = sched_clock();
	udelay(LOW_POWER_IPI_WAIT_US);
}

static inline void __init_idle_data(struct clkcpu_8994_idle_data *id)
{
	id->ipi_sent_time = 0ULL;
	id->ipi_notsent_time = 0ULL;
	id->ipi_started_time = 0ULL;
	id->ipi_exit_time = 0ULL;
	/*
	 * This is in case we use the fact that these flags are cleared here
	 * as a serialization mechanism in the idle notifiers. Better to put
	 * in this memory barrier now rather than forget about it then.
	 */
	mb();
}

static void __low_power_pre_mux_switch(struct mux_clk *mux)
{
	struct clkcpu_8994_idle_data *idle_data, *this_idle_data;
	int cpu, this_cpu = smp_processor_id();
	struct mux_priv_data *data = (struct mux_priv_data *)mux->priv;

	/*
	 * If we somehow ended up here in the idle thread (when the low power
	 * mode code calls clk_enable/disable), do not attempt to schedule
	 * IPIs since those may deadlock with IPIs sent by other entities in
	 * the cpufreq thread.
	 */
	this_idle_data = &per_cpu(idle_data_clk_8994, this_cpu);
	if (this_idle_data->idle &&
	    cpumask_test_cpu(this_cpu, &data->cpumask))
			return;

	/*
	 * Prevent non-idle CPUs from entering low power modes. This is to
	 * a) Ensure that we don't hit the clk_enable/disable on other CPUs
	 *    in the low power code, the IPI would only be processed after the
	 *    mux switch and a sleep and wakeup cycle since we're already
	 *    holding the mux reg lock at this point.
	 * b) Prevent CPUs from sleeping just to have them wake up immediately
	      and process the IPI.
	 */
	for_each_cpu(cpu, &data->cpumask)
		per_cpu_idle_poll_ctrl(cpu, true);
	spin_lock(data->exit_idle_lock);

	for_each_online_cpu(cpu) {
		if (cpu == smp_processor_id() ||
			!cpumask_test_cpu(cpu, &data->cpumask))
			continue;
		idle_data = &per_cpu(idle_data_clk_8994, cpu);
		if (!idle_data->idle) {
			__init_idle_data(idle_data);
			idle_data->ipi_sent_time = sched_clock();
			/*
			 * send IPI to core not in idle. It is assumed that the
			 * caller (probably cpufreq) has ensured that hotplug
			 * is not possible here.
			 */
			__smp_call_function_single(cpu,
				&idle_data->csd[data->csd_idx], 0);
		} else {
			idle_data->ipi_notsent_time = sched_clock();
		}
	}

	/* Wait for IPI to begin */
	udelay(WAIT_IPI_HANDLER_BEGIN_US);
}

static void __low_power_post_mux_switch(struct mux_clk *mux)
{
	struct clkcpu_8994_idle_data *this_idle_data;
	int cpu, this_cpu = smp_processor_id();
	struct mux_priv_data *data = (struct mux_priv_data *)mux->priv;

	/*
	 * If we ended up here in the idle thread (when the low power
	 * mode code calls clk_enable/disable), do not attempt to schedule
	 * IPIs since those may deadlock with IPIs sent by other entities in
	 * the cpufreq thread.
	 */
	this_idle_data = &per_cpu(idle_data_clk_8994, this_cpu);
	if (this_idle_data->idle &&
	    cpumask_test_cpu(this_cpu, &data->cpumask))
			return;

	spin_unlock(data->exit_idle_lock);
	for_each_cpu(cpu, &data->cpumask)
		per_cpu_idle_poll_ctrl(cpu, false);
}

/*
 * One CPU may be switching LF CPU mux, while the other is enabling or disabling
 * the HFMUX. Serialize those operations.
 */
static DEFINE_SPINLOCK(low_power_mux_lock);

static void __low_power_mux_set_sel(struct mux_clk *mux, int sel)
{
	u32 regval;
	unsigned long flags;

	spin_lock(&low_power_mux_lock);
	__low_power_pre_mux_switch(mux);

	spin_lock_irqsave(&mux_reg_lock, flags);
	regval = readl_relaxed(*mux->base + mux->offset);
	regval &= ~(mux->mask << mux->shift);
	regval |= (sel & mux->mask) << mux->shift;
	sanity_check_clock_tree(regval, mux);
	writel_relaxed(regval, *mux->base + mux->offset);
	/* Ensure switch request goes through before returning */
	mb();
	/* Hardware mandated delay */
	udelay(5);
	spin_unlock_irqrestore(&mux_reg_lock, flags);

	__low_power_post_mux_switch(mux);
	spin_unlock(&low_power_mux_lock);
}

/* It is assumed that the mux enable state is locked in this function */
static int low_power_mux_set_sel(struct mux_clk *mux, int sel)
{
	mux->en_mask = sel;

	/*
	 * Don't switch the mux if it isn't enabled.
	 * However, if this is a request to select the safe source
	 * do it unconditionally. This is to allow the safe source
	 * to be selected during frequency switches even if the mux
	 * is disabled (specifically on 8994 V1, the LFMUX may be
	 * disabled).
	 */
	if (!mux->c.count && sel != mux->low_power_sel)
		return 0;

	__low_power_mux_set_sel(mux, mux->en_mask);

	return 0;
}

static int low_power_mux_get_sel(struct mux_clk *mux)
{
	u32 regval = readl_relaxed(*mux->base + mux->offset);
	return (regval >> mux->shift) & mux->mask;
}

static int low_power_mux_enable(struct mux_clk *mux)
{
	__low_power_mux_set_sel(mux, mux->en_mask);
	return 0;
}

static void low_power_mux_disable(struct mux_clk *mux)
{
	__low_power_mux_set_sel(mux, mux->low_power_sel);
}

static int clock_cpu_8994_idle_notifier(struct notifier_block *nb,
					     unsigned long val,
					     void *data)
{
	int cpu = smp_processor_id();
	struct clkcpu_8994_idle_data *id = &per_cpu(idle_data_clk_8994, cpu);

	if (!id->low_power_mux_switch)
		return 0;

	switch (val) {
	case IDLE_START:
		id->idle_start_time = sched_clock();
		id->idle = true;
		/*
		 * Don't allow re-ordering of the idle flag with
		 * rest of the idle thread.
		 */
		mb();
		break;
	case IDLE_END:
		id->idle = false;
		/*
		 * Don't allow re-ordering of the idle flag with
		 * rest of the idle thread and the exit_idle_lock
		 * below..
		 */
		mb();
		id->idle_exit_time = sched_clock();
		spin_lock(id->exit_idle_lock);
		spin_unlock(id->exit_idle_lock);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block clock_cpu_8994_idle_nb = {
	.notifier_call = clock_cpu_8994_idle_notifier,
};

static struct clk_mux_ops low_power_mux_ops = {
	.set_mux_sel = low_power_mux_set_sel,
	.get_mux_sel = low_power_mux_get_sel,
	.enable = low_power_mux_enable,
	.disable = low_power_mux_disable,
};

static void low_power_mux_init(void)
{
	int cpu;

	if (!clk_low_power_mux_switch)
		return;

	a57_hf_mux.ops = a57_hf_mux_v2.ops = &low_power_mux_ops;
	a57_lf_mux.ops = a57_lf_mux_v2.ops = &low_power_mux_ops;

	a57_hf_mux.priv = a57_hf_mux_v2.priv = &a57_hf_mux_priv_data;
	a57_lf_mux.priv = a57_lf_mux_v2.priv = &a57_lf_mux_priv_data;

	for_each_possible_cpu(cpu) {
		struct clkcpu_8994_idle_data *id =
					&per_cpu(idle_data_clk_8994, cpu);
		if (logical_cpu_to_clk(cpu) == &a57_clk.c) {
			cpumask_set_cpu(cpu, &a57_hf_mux_priv_data.cpumask);
			cpumask_set_cpu(cpu, &a57_lf_mux_priv_data.cpumask);
			id->exit_idle_lock = &a57_exit_idle_lock;
			id->csd[CSD_LF_MUX].func = do_low_power_poll;
			id->csd[CSD_HF_MUX].func = do_low_power_poll;
			id->idle = false;
			spin_lock_init(&id->idle_lock);
			id->low_power_mux_switch = true;
		}
	}

	idle_notifier_register(&clock_cpu_8994_idle_nb);
}

static int cpu_clock_8994_driver_probe(struct platform_device *pdev)
{
	int ret, cpu;
	unsigned long a53rate, a57rate, ccirate;
	bool v2;
	int pvs_ver = 0;
	u64 pte_efuse;
	char a57speedbinstr[] = "qcom,a57-speedbinXX-vXX";
	char a53speedbinstr[] = "qcom,a53-speedbinXX-vXX";

	v2 = msm8994_v2 | msm8992;

	a53_pll0_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	a57_pll0_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	a53_pll1_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	a57_pll1_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	cci_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;

	if (v2)
		init_v2_data();

	ret = cpu_clock_8994_resources_init(pdev);
	if (ret)
		return ret;

	if (!v2)
		perform_v1_fixup();

	if (msm8992) {
		pte_efuse = readq_relaxed(vbases[EFUSE_BASE]);
		a53speedbin = (pte_efuse >> A53OFFSET) & 0x3;
		dev_info(&pdev->dev, "using A53 speed bin %u and pvs_ver %d\n",
			 a53speedbin, pvs_ver);

		snprintf(a53speedbinstr, ARRAY_SIZE(a53speedbinstr),
			"qcom,a53-speedbin%d-v%d", a53speedbin, pvs_ver);
	} else if (v2)
		pte_efuse = readl_relaxed(vbases[EFUSE_BASE]);

	snprintf(a53speedbinstr, ARRAY_SIZE(a53speedbinstr),
			"qcom,a53-speedbin%d-v%d", a53speedbin, pvs_ver);

	ret = of_get_fmax_vdd_class(pdev, &a53_clk.c, a53speedbinstr);
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for a53. Falling back to zero.\n");
		ret = of_get_fmax_vdd_class(pdev, &a53_clk.c,
					    "qcom,a53-speedbin0-v0");
		if (ret) {
			dev_err(&pdev->dev, "Unable to retrieve plan for A53. Bailing...\n");
			return ret;
		}
	}

	if (v2) {
		a57speedbin = pte_efuse & 0x7;
		dev_info(&pdev->dev, "using A57 speed bin %u and pvs_ver %d\n",
			 a57speedbin, pvs_ver);
	}

	snprintf(a57speedbinstr, ARRAY_SIZE(a57speedbinstr),
			"qcom,a57-speedbin%d-v%d", a57speedbin, pvs_ver);

	ret = of_get_fmax_vdd_class(pdev, &a57_clk.c, a57speedbinstr);
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for a57. Falling back to zero.\n");
		ret = of_get_fmax_vdd_class(pdev, &a57_clk.c,
					    "qcom,a57-speedbin0-v0");
		if (ret) {
			dev_err(&pdev->dev, "Unable to retrieve plan for A57. Bailing...\n");
			return ret;
		}
	}

	ret = of_get_fmax_vdd_class(pdev, &cci_clk.c, "qcom,cci-speedbin0-v0");
	if (ret) {
		dev_err(&pdev->dev, "Can't get speed bin for cci\n");
		return ret;
	}

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_clk.c)
			cpumask_set_cpu(cpu, &a53_clk.cpumask);
		if (logical_cpu_to_clk(cpu) == &a57_clk.c)
			cpumask_set_cpu(cpu, &a57_clk.cpumask);
	}

	low_power_mux_init();

	get_online_cpus();

	if (!v2)
		ret = of_msm_clock_register(pdev->dev.of_node, cpu_clocks_8994,
				 ARRAY_SIZE(cpu_clocks_8994));
	else
		ret = of_msm_clock_register(pdev->dev.of_node,
					    cpu_clocks_8994_v2,
					    ARRAY_SIZE(cpu_clocks_8994_v2));
	if (ret) {
		dev_err(&pdev->dev, "Unable to register CPU clocks.\n");
		return ret;
	}

	/* Keep the secondary PLLs enabled forever on V1 */
	if (!v2) {
		clk_prepare_enable(&a53_pll1.c);
		clk_prepare_enable(&a57_pll1.c);
	}

	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(&cci_clk.c),
			"Failed to enable cci clock.\n");
		WARN(clk_prepare_enable(logical_cpu_to_clk(cpu)),
			"Failed to enabled clock for cpu %d\n", cpu);
	}

	a53rate = clk_get_rate(&a53_clk.c);
	a57rate = clk_get_rate(&a57_clk.c);
	ccirate = clk_get_rate(&cci_clk.c);

	if (!a53rate) {
		dev_err(&pdev->dev, "Unknown a53 rate. Setting safe rate\n");
		ret = clk_set_rate(&a53_clk.c, a53_safe_parent.c.rate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on A53.\n");
			return -EINVAL;
		}
		a53rate = a53_safe_parent.c.rate;
	}

	if (!a57rate) {
		dev_err(&pdev->dev, "Unknown a57 rate. Setting safe rate\n");
		ret = clk_set_rate(&a57_clk.c, a57_safe_parent.c.rate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on A57.\n");
			return -EINVAL;
		}
		a57rate = a57_safe_parent.c.rate;
	}

	if (!ccirate) {
		dev_err(&pdev->dev, "Unknown CCI rate. Setting safe rate\n");
		ccirate = cci_hf_mux.safe_freq/cci_clk.data.max_div;
		ret = clk_set_rate(&cci_clk.c, ccirate);
		if (ret) {
			dev_err(&pdev->dev, "Can't set a safe rate on CCI.\n");
			return -EINVAL;
		}
	}

	/* Set low frequencies until thermal/cpufreq probe. */
	if (!v2) {
		clk_set_rate(&a53_clk.c, 384000000);
		clk_set_rate(&a57_clk.c, 199200000);
		clk_set_rate(&cci_clk.c, 150000000);
	} else {
		clk_set_rate(&a53_clk.c, 600000000);
		clk_set_rate(&a57_clk.c, 384000000);
		clk_set_rate(&cci_clk.c, 300000000);
	}


	/*
	 * For the A53s, prepare and enable the HFMUX. During hotplug, this
	 * ensures that the clk_disable/clk_unprepare do not get propagated
	 * beyond the a53_clk, allowing the PLL to stay on. The PLL voltage
	 * vote is active-set-only anyway.
	 */
	if (v2)
		clk_prepare_enable(&a53_hf_mux_v2.c);

	put_online_cpus();

	cpu_clock_8994_dev = pdev;
	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-clock-8994" },
	{ .compatible = "qcom,cpu-clock-8994-v2" },
	{ .compatible = "qcom,cpu-clock-8992" },
	{}
};

static struct platform_driver cpu_clock_8994_driver = {
	.probe = cpu_clock_8994_driver_probe,
	.driver = {
		.name = "cpu-clock-8994",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

/* CPU devices are not currently available in arch_initcall */
static int __init cpu_clock_8994_init_opp(void)
{
	if (cpu_clock_8994_dev)
		populate_opp_table(cpu_clock_8994_dev);
	return 0;
}
module_init(cpu_clock_8994_init_opp);

static int __init cpu_clock_8994_init(void)
{
	return platform_driver_register(&cpu_clock_8994_driver);
}
arch_initcall(cpu_clock_8994_init);

static void __exit cpu_clock_8994_exit(void)
{
	platform_driver_unregister(&cpu_clock_8994_driver);
}
module_exit(cpu_clock_8994_exit);

#define ALIAS0_GLB_BASE_PHY 0xF900D000
#define ALIAS1_GLB_BASE_PHY 0xF900F000
#define C1_PLL_BASE_PHY 0xF9016000
#define C0_PLL_BASE_PHY 0xF9015000

int __init cpu_clock_8994_init_a57_v2(void)
{
	int ret = 0;

	pr_info("%s: configuring clocks for the A57 cluster\n",
		msm8992 ? "msm8992" : "msm8994-v2");

	vbases[ALIAS0_GLB_BASE] = ioremap(ALIAS0_GLB_BASE_PHY, SZ_4K);
	if (!vbases[ALIAS0_GLB_BASE]) {
		WARN(1, "Unable to ioremap A57 mux base. Can't configure A57 clocks.\n");
		ret = -ENOMEM;
		goto fail;
	}

	vbases[ALIAS1_GLB_BASE] = ioremap(ALIAS1_GLB_BASE_PHY, SZ_4K);
	if (!vbases[ALIAS1_GLB_BASE]) {
		WARN(1, "Unable to ioremap A57 mux base. Can't configure A57 clocks.\n");
		ret = -ENOMEM;
		goto iomap_fail;
	}

	vbases[C0_PLL_BASE] = ioremap(C0_PLL_BASE_PHY, SZ_4K);
	if (!vbases[C0_PLL_BASE]) {
		WARN(1, "Unable to ioremap A53 pll base. Can't configure A53 clocks.\n");
		ret = -ENOMEM;
		goto iomap_c0_pll_fail;
	}

	vbases[C1_PLL_BASE] = ioremap(C1_PLL_BASE_PHY, SZ_4K);
	if (!vbases[C1_PLL_BASE]) {
		WARN(1, "Unable to ioremap A57 pll base. Can't configure A57 clocks.\n");
		ret = -ENOMEM;
		goto iomap_c1_pll_fail;
	}

	/* Select GPLL0 for 600MHz on the A57s */
	writel_relaxed(0x6, vbases[ALIAS1_GLB_BASE] + MUX_OFFSET);
	/* Select GPLL0 for 600MHz on the A53s */
	writel_relaxed(0x6, vbases[ALIAS0_GLB_BASE] + MUX_OFFSET);

	/* Ensure write goes through before we disable PLLs below. */
	mb();
	udelay(5);

	/*
	 * Disable the PLLs in order to allow early rate setting to work.
	 * The PLL ping-pong scheme needs the PLL to refuse round_rate
	 * requests if prepare. However handoff will set the PLL ref count
	 * to one thus preventing PLL ping-ponging to work correctly before
	 * late_init.
	 */
	writel_relaxed(0x0,  vbases[C0_PLL_BASE] + C0_PLL_MODE);
	writel_relaxed(0x0,  vbases[C0_PLL_BASE] + C0_PLLA_MODE);
	writel_relaxed(0x0,  vbases[C1_PLL_BASE] + C1_PLL_MODE);
	writel_relaxed(0x0,  vbases[C1_PLL_BASE] + C1_PLLA_MODE);

	/* Ensure writes go through before divider config below */
	mb();
	udelay(5);

	/* Setup dividers and outputs */
	writel_relaxed(0x109, vbases[C0_PLL_BASE] + C0_PLLA_USER_CTL);
	writel_relaxed(0x109, vbases[C1_PLL_BASE] + C1_PLL_USER_CTL);
	writel_relaxed(0x109, vbases[C1_PLL_BASE] + C1_PLLA_USER_CTL);

	/* Ensure writes go through before clock driver probe */
	mb();
	udelay(5);

	pr_cont("%s: finished configuring A57 cluster clocks.\n",
		msm8992 ? "msm8992" : "msm8994-v2");

iomap_c1_pll_fail:
	iounmap(vbases[C0_PLL_BASE]);
iomap_c0_pll_fail:
	iounmap(vbases[ALIAS1_GLB_BASE]);
iomap_fail:
	iounmap(vbases[ALIAS0_GLB_BASE]);
fail:
	return ret;
}

/* Setup the A57 clocks before _this_ driver probes, before smp_init */
int __init cpu_clock_8994_init_a57(void)
{
	u32 regval;
	int xo_sel, lfmux_sel, safe_sel;
	struct device_node *ofnode;

	ofnode = of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8994-v2");
	if (ofnode)
		msm8994_v2 = true;

	ofnode = of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8992");
	if (ofnode)
		msm8992 = true;

	if (msm8994_v2 || msm8992)
		return cpu_clock_8994_init_a57_v2();

	ofnode = of_find_compatible_node(NULL, NULL,
					 "qcom,cpu-clock-8994");
	if (!ofnode)
		return 0;

	/*
	 * One time configuration message. This is extremely important to know
	 * if the boot-time configuration has't hung the CPU(s).
	 */
	pr_info("clock-cpu-8994: configuring clocks for the A57 cluster\n");

	vbases[ALIAS1_GLB_BASE] = ioremap(ALIAS1_GLB_BASE_PHY, SZ_4K);
	if (!vbases[ALIAS1_GLB_BASE]) {
		WARN(1, "Unable to ioremap A57 mux base. Can't configure A57 clocks.\n");
		return -ENOMEM;
	}

	vbases[C1_PLL_BASE] = ioremap(C1_PLL_BASE_PHY, SZ_4K);
	if (!vbases[C1_PLL_BASE]) {
		WARN(1, "Unable to ioremap A57 pll base. Can't configure A57 clocks.\n");
		return -ENOMEM;
	}

	xo_sel = parent_to_src_sel(a57_lf_mux.parents, a57_lf_mux.num_parents,
				   &xo_ao.c);
	lfmux_sel = parent_to_src_sel(a57_hf_mux.parents,
					a57_hf_mux.num_parents, &a57_lf_mux.c);
	safe_sel = parent_to_src_sel(a57_lf_mux.parents, a57_lf_mux.num_parents,
					&a57_safe_clk.c);

	__cpu_mux_set_sel(&a57_lf_mux, xo_sel);
	__cpu_mux_set_sel(&a57_hf_mux, lfmux_sel);

	a57_pll1.c.ops->disable(&a57_pll1.c);

	/* Set the main/aux output divider on the A57 primary PLL to 4 */
	regval = readl_relaxed(vbases[C1_PLL_BASE] + C1_PLLA_USER_CTL);
	regval &= ~BM(9, 8);
	regval |= (0x3 << 8);
	writel_relaxed(regval, vbases[C1_PLL_BASE] + C1_PLLA_USER_CTL);

	a57_pll1.c.ops->set_rate(&a57_pll1.c, 1593600000);

	/* Set the divider on the PLL1 input to the A57 LF MUX (div 2) */
	regval = readl_relaxed(vbases[ALIAS1_GLB_BASE] + MUX_OFFSET);
	regval |= BIT(6);
	writel_relaxed(regval, vbases[ALIAS1_GLB_BASE] + MUX_OFFSET);

	a57_pll1.c.ops->enable(&a57_pll1.c);

	__cpu_mux_set_sel(&a57_lf_mux, safe_sel);

	/* Set the cached mux selections to match what was programmed above. */
	a57_lf_mux.en_mask = safe_sel;
	a57_hf_mux.en_mask = lfmux_sel;

	iounmap(vbases[ALIAS1_GLB_BASE]);
	iounmap(vbases[C1_PLL_BASE]);

	pr_cont("clock-cpu-8994: finished configuring A57 cluster clocks.\n");

	return 0;
}
early_initcall(cpu_clock_8994_init_a57);

MODULE_DESCRIPTION("CPU clock driver for 8994");
MODULE_LICENSE("GPL v2");
