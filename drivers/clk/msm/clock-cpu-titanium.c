/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/suspend.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/uaccess.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>

#include <dt-bindings/clock/msm-clocks-titanium.h>

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

#define UPDATE_CHECK_MAX_LOOPS 5000
#define CCI_RATE(rate)		((rate * 10) / 25)
#define PLL_MODE(x)		(*(x)->base + (unsigned long) (x)->mode_reg)

#define GLB_DIAG		0x0b11101c

enum {
	APCS_C0_PLL_BASE,
	APCS0_DBG_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];
struct platform_device *cpu_clock_dev;

DEFINE_EXT_CLK(xo_a_clk, NULL);
DEFINE_VDD_REGS_INIT(vdd_pwrcl, 1);

enum {
	A53SS_MUX_C0,
	A53SS_MUX_C1,
	A53SS_MUX_CCI,
	A53SS_MUX_NUM,
};

enum vdd_mx_pll_levels {
	VDD_MX_OFF,
	VDD_MX_SVS,
	VDD_MX_NOM,
	VDD_MX_TUR,
	VDD_MX_NUM,
};

static int vdd_pll_levels[] = {
	RPM_REGULATOR_LEVEL_NONE,	/* VDD_PLL_OFF */
	RPM_REGULATOR_LEVEL_SVS,	/* VDD_PLL_SVS */
	RPM_REGULATOR_LEVEL_NOM,	/* VDD_PLL_NOM */
	RPM_REGULATOR_LEVEL_TURBO,	/* VDD_PLL_TUR */
};

static DEFINE_VDD_REGULATORS(vdd_pll, VDD_MX_NUM, 1,
				vdd_pll_levels, NULL);

#define VDD_MX_HF_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_pll,			\
	.fmax = (unsigned long[VDD_MX_NUM]) {	\
		[VDD_MX_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_MX_NUM

static struct clk_ops clk_ops_variable_rate;

/* Early output of PLL */
static struct pll_clk apcs_hf_pll = {
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
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
		.early_output_mask = BIT(3),
		.lock_mask = BIT(31),
	},
	.vals = {
		.post_div_masked = 0x100,
		.pre_div_masked = 0x0,
		.config_ctl_val = 0x200D4828,
		.config_ctl_hi_val = 0x006,
		.test_ctl_hi_val = 0x00004000,
		.test_ctl_lo_val = 0x1C000000,
	},
	.base = &virt_bases[APCS_C0_PLL_BASE],
	.max_rate = 2208000000UL,
	.min_rate = 652800000UL,
	.src_rate =  19200000UL,
	.c = {
		.parent = &xo_a_clk.c,
		.dbg_name = "apcs_hf_pll",
		.ops = &clk_ops_variable_rate,
		/* MX level of MSM is much higher than of PLL */
		VDD_MX_HF_FMAX_MAP1(SVS, 2400000000UL),
		CLK_INIT(apcs_hf_pll.c),
	},
};

static const char const *mux_names[] = {"c0", "c1", "cci"};

/* Perf Cluster */
static struct mux_div_clk a53ssmux_perf = {
	.ops = &rcg_mux_div_ops,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a53ssmux_perf",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a53ssmux_perf.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &apcs_hf_pll.c,	 5 },  /* PLL early */
	),
};

/* Little Cluster */
static struct mux_div_clk a53ssmux_pwr = {
	.ops = &rcg_mux_div_ops,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a53ssmux_pwr",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a53ssmux_pwr.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &apcs_hf_pll.c,	 5 },  /* PLL early */
	),
};

static struct mux_div_clk ccissmux = {
	.ops = &rcg_mux_div_ops,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "ccissmux",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(ccissmux.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &apcs_hf_pll.c,	 5 },  /* PLL early */
	),
};

struct cpu_clk_titanium {
	u32 cpu_reg_mask;
	cpumask_t cpumask;
	bool hw_low_power_ctrl;
	struct pm_qos_request req;
	struct clk c;
	bool set_rate_done;
	s32 cpu_latency_no_l2_pc_us;
};

static void do_nothing(void *unused) { }

static struct cpu_clk_titanium a53_pwr_clk;
static struct cpu_clk_titanium a53_perf_clk;
static struct cpu_clk_titanium cci_clk;

static inline struct cpu_clk_titanium *to_cpu_clk_titanium(struct clk *c)
{
	return container_of(c, struct cpu_clk_titanium, c);
}

static enum handoff cpu_clk_titanium_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	return HANDOFF_DISABLED_CLK;
}

static long cpu_clk_titanium_round_rate(struct clk *c, unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static int cpu_clk_titanium_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct cpu_clk_titanium *cpuclk = to_cpu_clk_titanium(c);
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
					cpuclk->cpu_latency_no_l2_pc_us);
		smp_call_function_any(&cpuclk->cpumask, do_nothing,
				NULL, 1);
	}

	ret = clk_set_rate(c->parent, rate);
	if (!ret) {
		/* update the rates of perf & power cluster */
		if (c == &a53_pwr_clk.c)
			a53_perf_clk.c.rate = rate;
		if (c == &a53_perf_clk.c)
			a53_pwr_clk.c.rate  = rate;
		cci_clk.c.rate = CCI_RATE(rate);
	}

	/* Remove PM QOS request */
	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);

	return ret;
}

static int cpu_clk_cci_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct cpu_clk_titanium *cpuclk = to_cpu_clk_titanium(c);

	if (cpuclk->set_rate_done)
		return ret;

	ret = clk_set_rate(c->parent, rate);
	if (!ret)
		cpuclk->set_rate_done = true;
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

static struct clk_ops clk_ops_cpu = {
	.set_rate = cpu_clk_titanium_set_rate,
	.round_rate = cpu_clk_titanium_round_rate,
	.handoff = cpu_clk_titanium_handoff,
};

static struct clk_ops clk_ops_cci = {
	.set_rate = cpu_clk_cci_set_rate,
	.round_rate = cpu_clk_titanium_round_rate,
	.handoff = cpu_clk_titanium_handoff,
};

static struct cpu_clk_titanium a53_pwr_clk = {
	.cpu_reg_mask = 0x3,
	.cpu_latency_no_l2_pc_us = 280,
	.c = {
		.parent = &a53ssmux_pwr.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_pwrcl,
		.dbg_name = "a53_pwr_clk",
		CLK_INIT(a53_pwr_clk.c),
	},
};

static struct cpu_clk_titanium a53_perf_clk = {
	.cpu_reg_mask = 0x103,
	.cpu_latency_no_l2_pc_us = 280,
	.c = {
		.parent = &a53ssmux_perf.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_pwrcl,
		.dbg_name = "a53_perf_clk",
		CLK_INIT(a53_perf_clk.c),
	},
};

static struct cpu_clk_titanium cci_clk = {
	.c = {
		.parent = &ccissmux.c,
		.ops = &clk_ops_cci,
		.vdd_class = &vdd_pwrcl,
		.dbg_name = "cci_clk",
		CLK_INIT(cci_clk.c),
	},
};

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
	.base = &virt_bases[APCS0_DBG_BASE],
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
	.base = &virt_bases[APCS0_DBG_BASE],
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
	.base = &virt_bases[APCS0_DBG_BASE],
	.c = {
		.dbg_name = "cpu_debug_pri_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(cpu_debug_pri_mux.c),
	},
};

static struct clk_lookup cpu_clocks_titanium[] = {
	/* PLL */
	CLK_LIST(apcs_hf_pll),

	/* Muxes */
	CLK_LIST(a53ssmux_perf),
	CLK_LIST(a53ssmux_pwr),
	CLK_LIST(ccissmux),

	/* CPU clocks */
	CLK_LIST(a53_perf_clk),
	CLK_LIST(a53_pwr_clk),
	CLK_LIST(cci_clk),

	/* debug clocks */
	CLK_LIST(apc0_m_clk),
	CLK_LIST(apc1_m_clk),
	CLK_LIST(cci_m_clk),
	CLK_LIST(cpu_debug_pri_mux),
};

static struct mux_div_clk *cpussmux[] = { &a53ssmux_pwr, &a53ssmux_perf,
						&ccissmux };
static struct cpu_clk_titanium *cpuclk[] = { &a53_pwr_clk, &a53_perf_clk,
						&cci_clk};

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node = of_get_cpu_node(cpu, NULL);
	u32 reg;

	if (cpu_node && !of_property_read_u32(cpu_node, "reg", &reg)) {
		if ((reg | a53_pwr_clk.cpu_reg_mask) ==
						a53_pwr_clk.cpu_reg_mask)
			return &a53_pwr_clk.c;
		if ((reg | a53_perf_clk.cpu_reg_mask) ==
						a53_perf_clk.cpu_reg_mask)
			return &a53_perf_clk.c;
	}

	return NULL;
}

static int add_opp(struct clk *c, struct device *dev, unsigned long max_rate)
{
	unsigned long rate = 0;
	int level;
	int uv;
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

		uv = c->vdd_class->vdd_uv[level];
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
		 * The OPP pair for the lowest and highest frequency for
		 * each device that we're populating. This is important since
		 * this information will be used by thermal mitigation and the
		 * scheduler.
		 */
		if ((rate >= max_rate) || first) {
			if (first)
				first = false;
			else
				break;
		}
	}

	return 0;
}

static void print_opp_table(int a53_pwr_cpu, int a53_perf_cpu)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc0_fmax =
			a53_pwr_clk.c.fmax[a53_pwr_clk.c.num_fmax - 1];
	unsigned long apc0_fmin = a53_pwr_clk.c.fmax[1];

	rcu_read_lock();

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a53_pwr_cpu),
					apc0_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a53_pwr_cpu),
					apc0_fmin, true);
	/*
	 * One time information during boot. Important to know that this looks
	 * sane since it can eventually make its way to the scheduler.
	 */
	pr_info("clock_cpu: a53 C0: OPP voltage for %lu: %ld\n", apc0_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a53 C0: OPP voltage for %lu: %ld\n", apc0_fmax,
		dev_pm_opp_get_voltage(oppfmax));

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a53_perf_cpu),
					apc0_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a53_perf_cpu),
					apc0_fmin, true);
	pr_info("clock_cpu: a53 C1: OPP voltage for %lu: %lu\n", apc0_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a53 C2: OPP voltage for %lu: %lu\n", apc0_fmax,
		dev_pm_opp_get_voltage(oppfmax));

	rcu_read_unlock();
}

static void populate_opp_table(struct platform_device *pdev)
{
	unsigned long apc0_fmax;
	int cpu, a53_pwr_cpu, a53_perf_cpu;

	apc0_fmax = a53_pwr_clk.c.fmax[a53_pwr_clk.c.num_fmax - 1];

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_pwr_clk.c) {
			a53_pwr_cpu = cpu;
			WARN(add_opp(&a53_pwr_clk.c, get_cpu_device(cpu),
					apc0_fmax),
				"Failed to add OPP levels for %d\n", cpu);
		}
		if (logical_cpu_to_clk(cpu) == &a53_perf_clk.c) {
			a53_perf_cpu = cpu;
			WARN(add_opp(&a53_perf_clk.c, get_cpu_device(cpu),
					apc0_fmax),
				"Failed to add OPP levels for %d\n", cpu);
		}
	}

	/* One time print during bootup */
	pr_info("clock-cpu-titanium: OPP tables populated (cpu %d and %d)\n",
						a53_pwr_cpu, a53_perf_cpu);

	print_opp_table(a53_pwr_cpu, a53_perf_cpu);
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

static int cpu_parse_devicetree(struct platform_device *pdev)
{
	struct resource *res;
	int mux_id = 0;
	char rcg_name[] = "xxx-mux";
	char pll_name[] = "xxx-pll";
	struct clk *c;

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "c0-pll");
	if (!res) {
		dev_err(&pdev->dev, "missing %s\n", pll_name);
		return -EINVAL;
	}

	virt_bases[APCS_C0_PLL_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[APCS_C0_PLL_BASE]) {
		dev_err(&pdev->dev, "ioremap failed for %s\n",
				pll_name);
		return -ENOMEM;
	}

	for (mux_id = 0; mux_id < A53SS_MUX_NUM; mux_id++) {
		snprintf(rcg_name, ARRAY_SIZE(rcg_name), "%s-mux",
						mux_names[mux_id]);
		res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, rcg_name);
		if (!res) {
			dev_err(&pdev->dev, "missing %s\n", rcg_name);
			return -EINVAL;
		}

		cpussmux[mux_id]->base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (!cpussmux[mux_id]->base) {
			dev_err(&pdev->dev, "ioremap failed for %s\n",
								rcg_name);
			return -ENOMEM;
		}
	}

	/* PLL core logic */
	vdd_pll.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd-mx");
	if (IS_ERR(vdd_pll.regulator[0])) {
		dev_err(&pdev->dev, "Get vdd-mx regulator!!!\n");
		if (PTR_ERR(vdd_pll.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd-mx regulator!!!\n");
		return PTR_ERR(vdd_pll.regulator[0]);
	}

	vdd_pwrcl.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd-cl");
	if (IS_ERR(vdd_pwrcl.regulator[0])) {
		dev_err(&pdev->dev, "Get vdd-pwrcl regulator!!!\n");
		if (PTR_ERR(vdd_pwrcl.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the cluster vreg\n");
		return PTR_ERR(vdd_pwrcl.regulator[0]);
	}

	/* Sources of the PLL */
	c = devm_clk_get(&pdev->dev, "xo_a");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo (rc = %ld)!\n",
				PTR_ERR(c));
		return PTR_ERR(c);
	}
	xo_a_clk.c.parent = c;

	return 0;
}

static int clock_cpu_probe(struct platform_device *pdev)
{
	int speed_bin, version, rc, cpu, mux_id;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	unsigned long a53rate, ccirate;

	get_speed_bin(pdev, &speed_bin, &version);

	rc = cpu_parse_devicetree(pdev);
	if (rc)
		return rc;

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d-cl",
					speed_bin, version);
	for (mux_id = 0; mux_id < A53SS_MUX_CCI; mux_id++) {
		rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
						prop_name);
		if (rc) {
			dev_err(&pdev->dev, "Loading safe voltage plan %s!\n",
							prop_name);
			snprintf(prop_name, ARRAY_SIZE(prop_name),
						"qcom,speed0-bin-v0-cl");
			rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
								prop_name);
			if (rc) {
				dev_err(&pdev->dev, "safe voltage plan load failed for clusters\n");
				return rc;
			}
		}
	}

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d-cci", speed_bin, version);
	rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c, prop_name);
	if (rc) {
		dev_err(&pdev->dev, "Loading safe voltage plan %s!\n",
							prop_name);
		snprintf(prop_name, ARRAY_SIZE(prop_name),
						"qcom,speed0-bin-v0-cci");
		rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
								prop_name);
		if (rc) {
			dev_err(&pdev->dev, "safe voltage plan load failed for CCI\n");
			return rc;
		}
	}

	/* Debug Mux */
	virt_bases[APCS0_DBG_BASE] = devm_ioremap(&pdev->dev, GLB_DIAG, SZ_8);
	if (!virt_bases[APCS0_DBG_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap GLB_DIAG registers\n");
		return -ENOMEM;
	}

	rc = of_msm_clock_register(pdev->dev.of_node,
			cpu_clocks_titanium, ARRAY_SIZE(cpu_clocks_titanium));
	if (rc) {
		dev_err(&pdev->dev, "msm_clock_register failed\n");
		return rc;
	}

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
		WARN(clk_prepare_enable(&cci_clk.c),
				"Unable to Turn on CCI clock");
		WARN(clk_prepare_enable(logical_cpu_to_clk(cpu)),
				"Unable to turn on CPU clock for %d\n", cpu);
	}

	/* ccirate = HFPLL_rate/(2.5) */
	ccirate = CCI_RATE(apcs_hf_pll.c.rate);
	rc = clk_set_rate(&cci_clk.c, ccirate);
	if (rc)
		dev_err(&pdev->dev, "Can't set safe rate for CCI\n");

	a53rate = clk_get_rate(&a53_pwr_clk.c);
	pr_debug("Rate of A53 Pwr %ld, APCS PLL rate %ld\n", a53rate,
			apcs_hf_pll.c.rate);
	if (!a53rate) {
		dev_err(&pdev->dev, "Unknown a53 rate. Setting safe rate, rate %ld\n",
						apcs_hf_pll.c.rate);
		rc = clk_set_rate(&a53_pwr_clk.c, apcs_hf_pll.c.rate);
		if (rc)
			dev_err(&pdev->dev, "Can't set pwr safe rate\n");
	}

	rc = clk_set_rate(&a53_perf_clk.c, apcs_hf_pll.c.rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set perf safe rate\n");

	put_online_cpus();

	populate_opp_table(pdev);

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc)
		return rc;

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_pwr_clk.c)
			cpumask_set_cpu(cpu, &a53_pwr_clk.cpumask);
		if (logical_cpu_to_clk(cpu) == &a53_perf_clk.c)
			cpumask_set_cpu(cpu, &a53_perf_clk.cpumask);
	}

	a53_pwr_clk.hw_low_power_ctrl = true;
	a53_perf_clk.hw_low_power_ctrl = true;

	return 0;
}

static struct of_device_id clock_cpu_match_table[] = {
	{.compatible = "qcom,cpu-clock-titanium"},
	{}
};

static struct platform_driver clock_cpu_driver = {
	.probe = clock_cpu_probe,
	.driver = {
		.name = "cpu-clock-titanium",
		.of_match_table = clock_cpu_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clock_cpu_init(void)
{
	return platform_driver_register(&clock_cpu_driver);
}
arch_initcall(clock_cpu_init);

#define APCS_HF_PLL_BASE		0xb116000
#define APCS_ALIAS1_CMD_RCGR		0xb011050
#define APCS_ALIAS1_CFG_OFF		0x4
#define APCS_ALIAS1_CORE_CBCR_OFF	0x8
#define SRC_SEL				0x5
#define SRC_DIV				0x1

unsigned long pwrcl_early_boot_rate = 883200000;

static int __init cpu_clock_pwr_init(void)
{
	void __iomem  *base;
	int regval = 0;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
						"qcom,cpu-clock-titanium");
	if (!ofnode)
		return 0;

	/* Initialize the PLLs */
	virt_bases[APCS_C0_PLL_BASE] = ioremap_nocache(APCS_HF_PLL_BASE, SZ_1K);
	clk_ops_variable_rate = clk_ops_variable_rate_pll_hwfsm;
	clk_ops_variable_rate.list_registers = variable_pll_list_registers;

	__variable_rate_pll_init(&apcs_hf_pll.c);
	apcs_hf_pll.c.ops->set_rate(&apcs_hf_pll.c, pwrcl_early_boot_rate);
	clk_ops_variable_rate_pll.enable(&apcs_hf_pll.c);

	base = ioremap_nocache(APCS_ALIAS1_CMD_RCGR, SZ_8);
	regval = readl_relaxed(base);

	/* Source GPLL0 and at the rate of GPLL0 */
	regval = (SRC_SEL << 8) | SRC_DIV; /* 0x501 */
	writel_relaxed(regval, base + APCS_ALIAS1_CFG_OFF);
	/* Make sure src sel and src div is set before update bit */
	mb();

	/* update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);
	/* Make sure src sel and src div is set before update bit */
	mb();

	/* Enable the branch */
	regval =  readl_relaxed(base + APCS_ALIAS1_CORE_CBCR_OFF);
	regval |= BIT(0);
	writel_relaxed(regval, base + APCS_ALIAS1_CORE_CBCR_OFF);
	/* Branch enable should be complete */
	mb();
	iounmap(base);

	pr_info("Power clocks configured\n");

	return 0;
}
early_initcall(cpu_clock_pwr_init);
