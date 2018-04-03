/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/suspend.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/msm-clocks-8953.h>

#include "clock.h"

#define APCS_PLL_MODE		0x0
#define APCS_PLL_L_VAL		0x8
#define APCS_PLL_ALPHA_VAL	0x10
#define APCS_PLL_USER_CTL	0x18
#define APCS_PLL_CONFIG_CTL_LO	0x20
#define APCS_PLL_CONFIG_CTL_HI	0x24
#define APCS_PLL_STATUS		0x28
#define APCS_PLL_TEST_CTL_LO	0x30
#define APCS_PLL_TEST_CTL_HI	0x34

#define PLL_MODE(x)	(*(x)->base + (unsigned long) (x)->mode_reg)

#define GLB_DIAG	0x0b11101c

static struct clk_ops clk_ops_variable_rate;

DEFINE_EXT_CLK(xo_a_clk, NULL);
static DEFINE_VDD_REGS_INIT(vdd_cpu_perf, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_pwr, 1);
static DEFINE_VDD_REGS_INIT(vdd_cci, 1);

enum {
	APCS_C0_PLL_BASE,
	APCS_C1_PLL_BASE,
	APCS_CCI_PLL_BASE,
	N_PLL_BASES,
};

enum vdd_mx_pll_levels {
	VDD_MX_OFF,
	VDD_MX_MIN,
	VDD_MX_LOWER,
	VDD_MX_SVS,
	VDD_MX_TUR,
	VDD_MX_NUM,
};

static int vdd_pll_levels[] = {
	RPM_REGULATOR_LEVEL_NONE,       /* VDD_PLL_OFF */
	RPM_REGULATOR_LEVEL_MIN_SVS,    /* VDD_PLL_MIN */
	RPM_REGULATOR_LEVEL_LOW_SVS,    /* VDD_PLL_LOW_SVS */
	RPM_REGULATOR_LEVEL_SVS,	/* VDD_PLL_SVS */
	RPM_REGULATOR_LEVEL_TURBO,	/* VDD_PLL_TUR */
};

static DEFINE_VDD_REGULATORS(vdd_mx, VDD_MX_NUM, 1,
					vdd_pll_levels, NULL);

#define VDD_MX_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_mx,                  \
	.fmax = (unsigned long[VDD_MX_NUM]) {   \
		[VDD_MX_##l1] = (f1),           \
		[VDD_MX_##l2] = (f2),           \
	},                                      \
	.num_fmax = VDD_MX_NUM

#define VDD_MX_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_mx,                  \
	.fmax = (unsigned long[VDD_MX_NUM]) {   \
		[VDD_MX_##l1] = (f1),           \
	},                                      \
	.num_fmax = VDD_MX_NUM

static void __iomem *virt_bases[N_PLL_BASES];

/* Power PLL */
static struct pll_clk apcs_c0_pll = {
	.mode_reg = (void __iomem *)APCS_PLL_MODE,
	.l_reg = (void __iomem *)APCS_PLL_L_VAL,
	.alpha_reg = (void __iomem *)APCS_PLL_ALPHA_VAL,
	.config_reg = (void __iomem *)APCS_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)APCS_PLL_CONFIG_CTL_LO,
	.config_ctl_hi_reg = (void __iomem *)APCS_PLL_CONFIG_CTL_HI,
	.test_ctl_lo_reg = (void __iomem *)APCS_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)APCS_PLL_TEST_CTL_HI,
	.status_reg = (void __iomem *)APCS_PLL_MODE,
	.init_test_ctl = true,
	.test_ctl_dbg = true,
	.masks = {
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.lock_mask = BIT(31),
	},
	.vals = {
		.config_ctl_val = 0x200D4828,
		.config_ctl_hi_val = 0x006,
		.test_ctl_hi_val = 0x00004000,
		.test_ctl_lo_val = 0x1C000000,
	},
	.max_rate = 2016000000UL,
	.min_rate = 614400000UL,
	.src_rate =  19200000UL,
	.base = &virt_bases[APCS_C0_PLL_BASE],
	.c = {
		.parent = &xo_a_clk.c,
		.dbg_name = "apcs_c0_pll",
		.ops = &clk_ops_variable_rate,
		VDD_MX_FMAX_MAP2(MIN, 1200000000UL, LOWER, 2016000000UL),
		CLK_INIT(apcs_c0_pll.c),
	},
};

/* Perf PLL */
static struct pll_clk apcs_c1_pll = {
	.mode_reg = (void __iomem *)APCS_PLL_MODE,
	.l_reg = (void __iomem *)APCS_PLL_L_VAL,
	.alpha_reg = (void __iomem *)APCS_PLL_ALPHA_VAL,
	.config_reg = (void __iomem *)APCS_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)APCS_PLL_CONFIG_CTL_LO,
	.config_ctl_hi_reg = (void __iomem *)APCS_PLL_CONFIG_CTL_HI,
	.test_ctl_lo_reg = (void __iomem *)APCS_PLL_TEST_CTL_LO,
	.test_ctl_hi_reg = (void __iomem *)APCS_PLL_TEST_CTL_HI,
	.status_reg = (void __iomem *)APCS_PLL_MODE,
	.init_test_ctl = true,
	.test_ctl_dbg = true,
	.masks = {
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.lock_mask = BIT(31),
	},
	.vals = {
		.config_ctl_val = 0x200D4828,
		.config_ctl_hi_val = 0x006,
		.test_ctl_hi_val = 0x00004000,
		.test_ctl_lo_val = 0x1C000000,
	},
	.max_rate = 2016000000UL,
	.min_rate = 633600000UL,
	.src_rate =  19200000UL,
	.base = &virt_bases[APCS_C1_PLL_BASE],
	.c = {
		.parent = &xo_a_clk.c,
		.dbg_name = "apcs_c1_pll",
		.ops = &clk_ops_variable_rate,
		VDD_MX_FMAX_MAP2(MIN, 1200000000UL, LOWER, 2016000000UL),
		CLK_INIT(apcs_c1_pll.c),
	},
};

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.update_mask = BIT(22),
	.output_mask = 0xf,
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.cal_l_val_mask = BM(31, 16),
};

static struct alpha_pll_vco_tbl apcs_cci_pll_vco[] = {
	VCO(2, 500000000, 1000000000),
};

static struct alpha_pll_clk apcs_cci_pll = {
	.masks = &pll_masks_p,
	.offset = 0x00,
	.vco_tbl = apcs_cci_pll_vco,
	.num_vco = ARRAY_SIZE(apcs_cci_pll_vco),
	.enable_config = 0x8,  /* Early output */
	.slew = true,
	.config_ctl_val = 0x4001055b,
	.cal_l_val = 0x27 << 16,  /* Mid of VCO mode - 748.8MHz */
	.base = &virt_bases[APCS_CCI_PLL_BASE],
	.c = {
		.parent = &xo_a_clk.c,
		.rate = 787200000,
		.dbg_name = "apcs_cci_pll",
		.ops = &clk_ops_dyna_alpha_pll,
		VDD_MX_FMAX_MAP1(SVS, 1000000000UL),
		CLK_INIT(apcs_cci_pll.c),
	},
};

enum {
	A53SS_MUX_PERF,
	A53SS_MUX_PWR,
	A53SS_MUX_CCI,
	A53SS_MUX_NUM,
};

static const char * const pll_names[] = { "c1", "c0", "cci" };
static const char * const mux_names[] = { "c1", "c0", "cci" };

struct a53_cpu_clk {
	u32 cpu_reg_mask;
	cpumask_t cpumask;
	bool hw_low_power_ctrl;
	struct pm_qos_request req;
	struct clk c;
	struct latency_level latency_lvl;
	s32 cpu_latency_no_l2_pc_us;
};

static struct mux_div_clk perf_cpussmux = {
	.ops = &rcg_mux_div_ops,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "perf_cpussmux",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(perf_cpussmux.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &apcs_c1_pll.c, 5},
	),
};

static struct mux_div_clk pwr_cpussmux = {
	.ops = &rcg_mux_div_ops,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "pwr_cpussmux",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(pwr_cpussmux.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &apcs_c0_pll.c, 5},
	),
};

static struct mux_div_clk cci_cpussmux = {
	.ops = &rcg_mux_div_ops,
	.data = {
		.skip_odd_div = true,
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "cci_cpussmux",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(cci_cpussmux.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &apcs_cci_pll.c, 5},
	),
};

static struct a53_cpu_clk pwr_clk;
static struct a53_cpu_clk perf_clk;
static struct a53_cpu_clk cci_clk;

static void do_nothing(void *unused) { }

static inline struct a53_cpu_clk *to_a53_cpu_clk(struct clk *c)
{
	return container_of(c, struct a53_cpu_clk, c);
}

static enum handoff a53_cpu_clk_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	return HANDOFF_DISABLED_CLK;
}

static long a53_cpu_clk_round_rate(struct clk *c, unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static int a53_cpu_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct a53_cpu_clk *cpuclk = to_a53_cpu_clk(c);
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;

	/*
	 * If hardware control of the clock tree is enabled during power
	 * collapse, setup a PM QOS request to prevent power collapse and
	 * wake up one of the CPUs in this clock domain, to ensure software
	 * control while the clock rate is being switched.
	 */
	if (hw_low_power_ctrl) {
		memset(&cpuclk->req, 0, sizeof(cpuclk->req));
		cpumask_copy(&cpuclk->req.cpus_affine,
				(const struct cpumask *)&cpuclk->cpumask);
		cpuclk->req.type = PM_QOS_REQ_AFFINE_CORES;
		pm_qos_add_request(&cpuclk->req, PM_QOS_CPU_DMA_LATENCY,
				cpuclk->cpu_latency_no_l2_pc_us - 1);
		smp_call_function_any(&cpuclk->cpumask, do_nothing,
				NULL, 1);
	}

	ret = clk_set_rate(c->parent, rate);

	/* Remove PM QOS request */
	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);

	return ret;
}

static void __iomem *variable_pll_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct pll_clk *pll = to_pll_clk(c);
	static struct clk_register_data data[] = {
		{"MODE", 0x0},
		{"L", 0x8},
		{"ALPHA", 0x10},
		{"USER_CTL", 0x18},
		{"CONFIG_CTL_LO", 0x20},
		{"CONFIG_CTL_HI", 0x24},
		{"STATUS", 0x28},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return PLL_MODE(pll);
}

static const struct clk_ops clk_ops_cpu = {
	.set_rate = a53_cpu_clk_set_rate,
	.round_rate = a53_cpu_clk_round_rate,
	.handoff = a53_cpu_clk_handoff,
};

static struct a53_cpu_clk perf_clk = {
	.cpu_reg_mask = 0x103,
	.latency_lvl = {
		.affinity_level = LPM_AFF_LVL_L2,
		.reset_level = LPM_RESET_LVL_GDHS,
		.level_name = "perf",
	},
	.cpu_latency_no_l2_pc_us = 280,
	.c = {
		.parent = &perf_cpussmux.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_perf,
		.dbg_name = "perf_clk",
		CLK_INIT(perf_clk.c),
	},
};

static struct a53_cpu_clk pwr_clk = {
	.cpu_reg_mask = 0x3,
	.latency_lvl = {
		.affinity_level = LPM_AFF_LVL_L2,
		.reset_level = LPM_RESET_LVL_GDHS,
		.level_name = "pwr",
	},
	.cpu_latency_no_l2_pc_us = 280,
	.c = {
		.parent = &pwr_cpussmux.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_pwr,
		.dbg_name = "pwr_clk",
		CLK_INIT(pwr_clk.c),
	},
};

static struct a53_cpu_clk cci_clk = {
	.c = {
		.parent = &cci_cpussmux.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cci,
		.dbg_name = "cci_clk",
		CLK_INIT(cci_clk.c),
	},
};

static void __iomem *meas_base;

static struct measure_clk apc0_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc0_m_clk",
		CLK_INIT(apc0_m_clk.c),
	},
};

static struct measure_clk apc1_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc1_m_clk",
		CLK_INIT(apc1_m_clk.c),
	},
};

static struct measure_clk cci_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "cci_m_clk",
		CLK_INIT(cci_m_clk.c),
	},
};

static struct mux_clk cpu_debug_ter_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x3,
	.shift = 8,
	MUX_SRC_LIST(
		{ &apc0_m_clk.c, 0},
		{ &apc1_m_clk.c, 1},
		{ &cci_m_clk.c,  2},
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "cpu_debug_ter_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(cpu_debug_ter_mux.c),
	},
};

static struct mux_clk cpu_debug_sec_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x7,
	.shift = 12,
	MUX_SRC_LIST(
		{ &cpu_debug_ter_mux.c, 0},
	),
	MUX_REC_SRC_LIST(
		&cpu_debug_ter_mux.c,
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "cpu_debug_sec_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(cpu_debug_sec_mux.c),
	},
};

static struct mux_clk cpu_debug_pri_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x3,
	.shift = 16,
	MUX_SRC_LIST(
		{ &cpu_debug_sec_mux.c, 0},
	),
	MUX_REC_SRC_LIST(
		&cpu_debug_sec_mux.c,
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "cpu_debug_pri_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(cpu_debug_pri_mux.c),
	},
};

static struct clk_lookup a53_cpu_clocks[] = {
	/* PLLs */
	CLK_LIST(apcs_c0_pll),
	CLK_LIST(apcs_c1_pll),
	CLK_LIST(apcs_cci_pll),

	/* Muxes */
	CLK_LIST(pwr_cpussmux),
	CLK_LIST(perf_cpussmux),
	CLK_LIST(cci_cpussmux),

	/* CPU clocks */
	CLK_LIST(pwr_clk),
	CLK_LIST(perf_clk),
	CLK_LIST(cci_clk),

	/* debug clocks */
	CLK_LIST(apc0_m_clk),
	CLK_LIST(apc1_m_clk),
	CLK_LIST(cci_m_clk),
	CLK_LIST(cpu_debug_pri_mux),
};

static struct mux_div_clk *a53ssmux[] = { &perf_cpussmux, &pwr_cpussmux,
						&cci_cpussmux };

static struct a53_cpu_clk *cpuclk[] = { &perf_clk, &pwr_clk,
						&cci_clk };

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node = of_get_cpu_node(cpu, NULL);
	u32 reg;

	if (cpu_node && !of_property_read_u32(cpu_node, "reg", &reg)) {
		if ((reg | pwr_clk.cpu_reg_mask) ==
						pwr_clk.cpu_reg_mask)
			return &pwr_clk.c;
		if ((reg | perf_clk.cpu_reg_mask) ==
						perf_clk.cpu_reg_mask)
			return &perf_clk.c;
	}

	return NULL;
}

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
	vdd->level_votes = devm_kzalloc(&pdev->dev,
				prop_len * sizeof(*vdd->level_votes),
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
	vdd->use_max_uV = true;
	c->num_fmax = prop_len;
	return 0;
}

static void get_speed_bin(struct platform_device *pdev, int *bin,
								int *version)
{
	struct resource *res;
	void __iomem *base;
	u32 pte_efuse;

	*bin = 0;
	*version = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		dev_info(&pdev->dev,
			 "No speed/PVS binning available. Defaulting to 0!\n");
		return;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base) {
		dev_warn(&pdev->dev,
			 "Unable to read efuse data. Defaulting to 0!\n");
		return;
	}

	pte_efuse = readl_relaxed(base);
	devm_iounmap(&pdev->dev, base);

	*bin = (pte_efuse >> 8) & 0x7;

	dev_info(&pdev->dev, "Speed bin: %d PVS Version: %d\n", *bin,
								*version);
}

static int cpu_parse_devicetree(struct platform_device *pdev, int mux_id)
{
	struct resource *res;
	char rcg_name[] = "apcs-xxx-rcg-base";
	char vdd_name[] = "vdd-xxx";
	struct regulator *regulator;

	snprintf(rcg_name, ARRAY_SIZE(rcg_name), "apcs-%s-rcg-base",
						mux_names[mux_id]);

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, rcg_name);
	if (!res) {
		dev_err(&pdev->dev, "missing %s\n", rcg_name);
		return -EINVAL;
	}

	a53ssmux[mux_id]->base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!a53ssmux[mux_id]->base) {
		dev_err(&pdev->dev, "ioremap failed for %s\n", rcg_name);
		return -ENOMEM;
	}

	snprintf(vdd_name, ARRAY_SIZE(vdd_name), "vdd-%s", mux_names[mux_id]);
	regulator = devm_regulator_get(&pdev->dev, vdd_name);
	if (IS_ERR(regulator)) {
		if (PTR_ERR(regulator) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "unable to get regulator\n");
		return PTR_ERR(regulator);
	}

	cpuclk[mux_id]->c.vdd_class->regulator[0] = regulator;

	return 0;
}

static int add_opp(struct clk *c, struct device *cpudev,
			unsigned long max_rate)
{
	unsigned long rate = 0;
	int level;
	long ret, uv, corner;
	int j = 1;

	while (1) {
		rate = c->fmax[j++];

		level = find_vdd_level(c, rate);
		if (level <= 0) {
			pr_warn("clock-cpu: no vdd level for %lu.\n", rate);
			return -EINVAL;
		}

		corner = c->vdd_class->vdd_uv[level];
		if (corner < 0)
			return -EINVAL;

		/* Get actual voltage corresponding to each corner */
		uv = regulator_list_corner_voltage(c->vdd_class->regulator[0],
							corner);
		if (uv < 0) {
			pr_warn("%s: no uv for corner %ld - err: %ld\n",
						c->dbg_name, corner, uv);
			return uv;
		}

		/*
		 * Populate both CPU and regulator devices with the
		 * freq-to-corner OPP table to maintain backward
		 * compatibility.
		 */
		ret = dev_pm_opp_add(cpudev, rate, uv);
		if (ret) {
			pr_warn("clock-cpu: couldn't add OPP for %lu\n",
				rate);
			return ret;
		}

		if (rate >= max_rate)
			break;
	}

	return 0;
}

static void print_opp_table(int a53_c0_cpu, int a53_c1_cpu)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc0_fmax, apc1_fmax, apc0_fmin, apc1_fmin;

	apc0_fmax = pwr_clk.c.fmax[pwr_clk.c.num_fmax - 1];
	apc0_fmin = pwr_clk.c.fmax[1];
	apc1_fmax = perf_clk.c.fmax[perf_clk.c.num_fmax - 1];
	apc1_fmin = perf_clk.c.fmax[1];

	rcu_read_lock();
	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a53_c0_cpu),
						apc0_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a53_c0_cpu),
						apc0_fmin, true);
	/*
	 * One time information during boot. Important to know that this
	 * looks sane since it can eventually make its way to the
	 * scheduler.
	 */
	pr_info("clock_cpu: a53_c0: OPP voltage for %lu: %ld\n",
			apc0_fmin, dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a53_c0: OPP voltage for %lu: %ld\n",
			apc0_fmax, dev_pm_opp_get_voltage(oppfmax));

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a53_c1_cpu),
						apc1_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a53_c1_cpu),
						apc1_fmin, true);
	pr_info("clock_cpu: a53_c1: OPP voltage for %lu: %lu\n", apc1_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a53_c1: OPP voltage for %lu: %lu\n", apc1_fmax,
		dev_pm_opp_get_voltage(oppfmax));
	rcu_read_unlock();
}

static void populate_opp_table(struct platform_device *pdev)
{
	unsigned long apc0_fmax, apc1_fmax;
	int cpu, a53_c0_cpu = 0, a53_c1_cpu = 0;
	struct device *dev;

	apc0_fmax = pwr_clk.c.fmax[pwr_clk.c.num_fmax - 1];

	apc1_fmax = perf_clk.c.fmax[perf_clk.c.num_fmax - 1];

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &pwr_clk.c) {
			a53_c0_cpu = cpu;
			dev = get_cpu_device(cpu);
			if (!dev) {
				pr_err("can't find cpu device for attaching OPPs\n");
				return;
			}

			WARN(add_opp(&pwr_clk.c, dev, apc0_fmax),
				"Failed to add OPP levels for %d\n", cpu);
		}

		if (logical_cpu_to_clk(cpu) == &perf_clk.c) {
			a53_c1_cpu = cpu;
			dev = get_cpu_device(cpu);
			if (!dev) {
				pr_err("can't find cpu device for attaching OPPs\n");
				return;
			}

			WARN(add_opp(&perf_clk.c, dev, apc1_fmax),
				"Failed to add OPP levels for %d\n", cpu);
		}
	}
	/* One time print during bootup */
	pr_info("clock-cpu: OPP tables populated (cpu %d and %d)",
		a53_c0_cpu, a53_c1_cpu);

	print_opp_table(a53_c0_cpu, a53_c1_cpu);

}

static int clock_sdm632_pm_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		clk_unprepare(&pwr_clk.c);
		clk_unprepare(&perf_clk.c);
		clk_unprepare(&cci_clk.c);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		clk_prepare(&pwr_clk.c);
		clk_prepare(&perf_clk.c);
		clk_prepare(&cci_clk.c);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block clock_sdm632_pm_notifier = {
	.notifier_call = clock_sdm632_pm_event,
};

/**
 * clock_panic_callback() - panic notification callback function.
 *		This function is invoked when a kernel panic occurs.
 * @nfb:	Notifier block pointer
 * @event:	Value passed unmodified to notifier function
 * @data:	Pointer passed unmodified to notifier function
 *
 * Return: NOTIFY_OK
 */
static int clock_panic_callback(struct notifier_block *nfb,
					unsigned long event, void *data)
{
	unsigned long rate;

	rate  = (perf_clk.c.count) ? perf_clk.c.rate : 0;
	pr_err("%s frequency: %10lu Hz\n", perf_clk.c.dbg_name, rate);

	rate  = (pwr_clk.c.count) ? pwr_clk.c.rate : 0;
	pr_err("%s frequency: %10lu Hz\n", pwr_clk.c.dbg_name, rate);

	return NOTIFY_OK;
}

static struct notifier_block clock_panic_notifier = {
	.notifier_call = clock_panic_callback,
	.priority = 1,
};

/* Configure PLL at Nominal frequency */
static unsigned long pwrcl_early_boot_rate = 1363200000;
static unsigned long perfcl_early_boot_rate = 1401600000;
static unsigned long cci_early_boot_rate = 691200000;

static int clock_a53_probe(struct platform_device *pdev)
{
	int speed_bin, version, rc, cpu, mux_id;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	int mux_num = A53SS_MUX_NUM;
	struct clk *xo_clk;

	get_speed_bin(pdev, &speed_bin, &version);

	xo_clk = devm_clk_get(&pdev->dev, "xo_a");
	if (IS_ERR(xo_clk)) {
		if (PTR_ERR(xo_clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock\n");
		return PTR_ERR(xo_clk);
	}
	xo_a_clk.c.parent = xo_clk;

	/* PLL core logic */
	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd-mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		dev_err(&pdev->dev, "Get vdd-mx regulator!!!\n");
		if (PTR_ERR(vdd_mx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd-mx regulator!!!\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	for (mux_id = 0; mux_id < mux_num; mux_id++) {
		rc = cpu_parse_devicetree(pdev, mux_id);
		if (rc)
			return rc;

		snprintf(prop_name, ARRAY_SIZE(prop_name),
					"qcom,speed%d-bin-v%d-%s",
					speed_bin, version, mux_names[mux_id]);

		rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
								prop_name);
		if (rc) {
			/* Fall back to most conservative PVS table */
			dev_err(&pdev->dev, "Unable to load voltage plan %s!\n",
								prop_name);

			snprintf(prop_name, ARRAY_SIZE(prop_name),
				"qcom,speed0-bin-v0-%s", mux_names[mux_id]);
			rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
								prop_name);
			if (rc) {
				dev_err(&pdev->dev,
					"Unable to load safe voltage plan\n");
				return rc;
			}
			dev_info(&pdev->dev, "Safe voltage plan loaded.\n");
		}
	}

	/* Debug MUX */
	meas_base = devm_ioremap(&pdev->dev, GLB_DIAG, SZ_8);
	if (!meas_base) {
		dev_err(&pdev->dev, "Failed to ioremap GLB_DIAG registers\n");
		return -ENOMEM;
	}

	rc = of_msm_clock_register(pdev->dev.of_node, a53_cpu_clocks,
						ARRAY_SIZE(a53_cpu_clocks));

	if (rc) {
		dev_err(&pdev->dev, "msm_clock_register failed\n");
		return rc;
	}

	/* Force to move to PLL configuartion */
	rc = clk_set_rate(&cci_clk.c, cci_early_boot_rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set CCI PLL rate for CCI\n");

	rc = clk_set_rate(&pwr_clk.c, pwrcl_early_boot_rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set pwr PLL rate for Cluster-0 %ld\n",
					pwrcl_early_boot_rate);

	rc = clk_set_rate(&perf_clk.c, perfcl_early_boot_rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set perf PLL rate for Cluster-1 %ld\n",
					perfcl_early_boot_rate);

	rc = clock_rcgwr_init(pdev);
	if (rc)
		dev_err(&pdev->dev, "Failed to init RCGwR\n");

	/*
	 * We don't want the CPU clocks to be turned off at late init
	 * if CPUFREQ or HOTPLUG configs are disabled. So, bump up the
	 * refcount of these clocks. Any cpufreq/hotplug manager can assume
	 * that the clocks have already been prepared and enabled by the time
	 * they take over.
	 */
	get_online_cpus();
	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(&cpuclk[cpu/4]->c),
				"Unable to turn on CPU clock");
		WARN(clk_prepare_enable(&cci_clk.c),
				"Unable to turn on CCI clock");
	}
	put_online_cpus();

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &perf_clk.c)
			cpumask_set_cpu(cpu, &perf_clk.cpumask);
		if (logical_cpu_to_clk(cpu) == &pwr_clk.c)
			cpumask_set_cpu(cpu, &pwr_clk.cpumask);
	}

	pwr_clk.hw_low_power_ctrl = true;
	perf_clk.hw_low_power_ctrl = true;

	register_pm_notifier(&clock_sdm632_pm_notifier);

	populate_opp_table(pdev);

	atomic_notifier_chain_register(&panic_notifier_list,
						&clock_panic_notifier);

	return 0;
}

static const struct of_device_id clock_a53_match_table[] = {
	{.compatible = "qcom,cpu-clock-sdm632"},
	{}
};

static struct platform_driver clock_a53_driver = {
	.probe = clock_a53_probe,
	.driver = {
		.name = "cpu-clock-sdm632",
		.of_match_table = clock_a53_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clock_a53_init(void)
{
	return platform_driver_register(&clock_a53_driver);
}
arch_initcall(clock_a53_init);

static int __init clock_cpu_lpm_get_latency(void)
{
	int rc = 0;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
					"qcom,cpu-clock-sdm632");

	if (!ofnode)
		return 0;

	rc = lpm_get_latency(&perf_clk.latency_lvl,
			&perf_clk.cpu_latency_no_l2_pc_us);
	if (rc < 0)
		pr_err("Failed to get the L2 PC value for perf\n");

	rc = lpm_get_latency(&pwr_clk.latency_lvl,
			&pwr_clk.cpu_latency_no_l2_pc_us);
	if (rc < 0)
		pr_err("Failed to get the L2 PC value for pwr\n");

	pr_debug("Latency for pwr/perf cluster %d : %d\n",
		pwr_clk.cpu_latency_no_l2_pc_us,
		perf_clk.cpu_latency_no_l2_pc_us);

	return rc;
}
late_initcall(clock_cpu_lpm_get_latency);

#define PWR_PLL_BASE			0xb116000
#define PERF_PLL_BASE			0xb016000
#define CCI_PLL_BASE			0xb1d0000
#define APCS_ALIAS1_CMD_RCGR		0xb011050
#define APCS_ALIAS1_CFG_OFF		0x4
#define APCS_ALIAS1_CORE_CBCR_OFF	0x8
#define SRC_SEL				0x4
#define SRC_DIV				0x1

/* Dummy clock for setting the rate of CCI PLL in early_init*/
DEFINE_CLK_DUMMY(p_clk, 19200000);

static int __init cpu_clock_init(void)
{
	void __iomem  *base;
	int regval = 0, count;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
						"qcom,cpu-clock-sdm632");
	if (!ofnode)
		return 0;

	virt_bases[APCS_C0_PLL_BASE] = ioremap_nocache(PWR_PLL_BASE, SZ_1K);
	virt_bases[APCS_C1_PLL_BASE] = ioremap_nocache(PERF_PLL_BASE, SZ_1K);
	virt_bases[APCS_CCI_PLL_BASE] = ioremap_nocache(CCI_PLL_BASE, SZ_1K);
	clk_ops_variable_rate = clk_ops_variable_rate_pll_hwfsm;
	clk_ops_variable_rate.list_registers = variable_pll_list_registers;

	/* Initialize the PLLs */
	__variable_rate_pll_init(&apcs_c0_pll.c);
	__variable_rate_pll_init(&apcs_c1_pll.c);
	__init_alpha_pll(&apcs_cci_pll.c);

	/* Enable the PLLs */
	apcs_c0_pll.c.ops->set_rate(&apcs_c0_pll.c, pwrcl_early_boot_rate);
	clk_ops_variable_rate_pll.enable(&apcs_c0_pll.c);

	apcs_c1_pll.c.ops->set_rate(&apcs_c1_pll.c, perfcl_early_boot_rate);
	clk_ops_variable_rate_pll.enable(&apcs_c1_pll.c);

	apcs_cci_pll.c.parent = (struct clk *)&p_clk;
	apcs_cci_pll.c.ops->set_rate(&apcs_cci_pll.c, cci_early_boot_rate);
	clk_ops_dyna_alpha_pll.enable(&apcs_cci_pll.c);

	base = ioremap_nocache(APCS_ALIAS1_CMD_RCGR, SZ_8);
	regval = readl_relaxed(base);

	/* Source from GPLL0 */
	regval = (SRC_SEL << 8) | SRC_DIV; /* 0x401 */
	writel_relaxed(regval, base + APCS_ALIAS1_CFG_OFF);
	/* Make sure src sel and src div is set before update bit */
	mb();

	/* update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);
	/* Make sure src sel and src div is set before update bit */
	mb();

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		if (!(readl_relaxed(base)) & BIT(0))
			break;
		udelay(1);
	}

	/* Enable the branch */
	regval =  readl_relaxed(base + APCS_ALIAS1_CORE_CBCR_OFF);
	regval |= BIT(0);
	writel_relaxed(regval, base + APCS_ALIAS1_CORE_CBCR_OFF);

	/* Branch enable should be complete */
	mb();
	iounmap(base);

	pr_info("CPU clocks configured\n");

	return 0;
}
early_initcall(cpu_clock_init);
