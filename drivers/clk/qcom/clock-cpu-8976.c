/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/suspend.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>

#include <dt-bindings/clock/msm-clocks-8976.h>

#include "clock.h"

#define APCS_PLL_MODE		0x0
#define APCS_PLL_L_VAL		0x4
#define APCS_PLL_M_VAL		0x8
#define APCS_PLL_N_VAL		0xC
#define APCS_PLL_USER_CTL	0x10
#define APCS_PLL_CONFIG_CTL	0x14
#define APCS_PLL_STATUS		0x1C

enum {
	APCS_C0_PLL_BASE,
	APCS_C1_PLL_BASE,
	APCS_CCI_PLL_BASE,
	APCS0_DBG_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];
struct platform_device *cpu_clock_dev;

static DEFINE_VDD_REGS_INIT(vdd_cpu_a72, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_a53, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_cci, 1);

DEFINE_EXT_CLK(sys_apcsaux_clk_2, NULL);
DEFINE_EXT_CLK(sys_apcsaux_clk_3, NULL);
DEFINE_EXT_CLK(xo_a_clk, NULL);

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

static int vdd_hf_levels[] = {
	0,		RPM_REGULATOR_LEVEL_NONE,	/* VDD_PLL_OFF */
	1800000,	RPM_REGULATOR_LEVEL_SVS,	/* VDD_PLL_SVS */
	1800000,	RPM_REGULATOR_LEVEL_NOM,	/* VDD_PLL_NOM */
	1800000,	RPM_REGULATOR_LEVEL_TURBO,	/* VDD_PLL_TUR */
};

static int vdd_sr_levels[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_PLL_OFF */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_PLL_SVS */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_PLL_NOM */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_PLL_TUR */
};

static DEFINE_VDD_REGULATORS(vdd_hf, VDD_MX_NUM, 2,
				vdd_hf_levels, NULL);

static DEFINE_VDD_REGULATORS(vdd_mx_sr, VDD_MX_NUM, 1,
				vdd_sr_levels, NULL);

#define VDD_MX_HF_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_hf,			\
	.fmax = (unsigned long[VDD_MX_NUM]) {	\
		[VDD_MX_##l1] = (f1),		\
		[VDD_MX_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_MX_NUM

#define VDD_MX_SR_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_mx_sr,			\
	.fmax = (unsigned long[VDD_MX_NUM]) {	\
		[VDD_MX_##l1] = (f1),		\
		[VDD_MX_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_MX_NUM

/* Early output of PLL */
static struct pll_clk a72ss_hf_pll = {
	.mode_reg = (void __iomem *)APCS_PLL_MODE,
	.l_reg = (void __iomem *)APCS_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)APCS_PLL_CONFIG_CTL,
	.status_reg = (void __iomem *)APCS_PLL_STATUS,
	.spm_ctrl = {
		.offset = 0x50,
		.event_bit = 0x4,
	},
	.masks = {
		.vco_mask = BM(29, 28),
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.early_output_mask =  BIT(3),
		.main_output_mask = BIT(0),
	},
	.vals = {
		.config_ctl_val = 0x04E0405D,
		.enable_mn = true,
		.post_div_masked = BVAL(9, 8, (1)),
		.vco_mode_masked = BVAL(21, 20, 1),
	},
	.base = &virt_bases[APCS_C1_PLL_BASE],
	.max_rate = 1843200000,
	.min_rate = 940800000,
	.c = {
		.parent = &xo_a_clk.c,
		.dbg_name = "a72ss_hf_pll",
		.ops = &clk_ops_hf_pll,
		/* MX level of MSM is much higher than of PLL */
		VDD_MX_HF_FMAX_MAP2(SVS, 2000000000, NOM, 2900000000UL),
		CLK_INIT(a72ss_hf_pll.c),
	},
};

DEFINE_FIXED_DIV_CLK(a72ss_hf_pll_main, 2, &a72ss_hf_pll.c);

/* Early output of PLL */
static struct pll_clk a53ss_sr_pll = {
	.mode_reg = (void __iomem *)APCS_PLL_MODE,
	.l_reg = (void __iomem *)APCS_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)APCS_PLL_CONFIG_CTL,
	.status_reg = (void __iomem *)APCS_PLL_STATUS,
	.spm_ctrl = {
		.offset = 0x50,
		.event_bit = 0x4,
	},
	.masks = {
		.vco_mask = BM(21, 20),
		.pre_div_mask = BM(14, 12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.early_output_mask =  BIT(3),
		.main_output_mask = BIT(0),
	},
	.vals = {
		.config_ctl_val = 0x00341600,
		.enable_mn = true,
		.post_div_masked =  BVAL(9, 8, (1)),
	},
	.data = {
		.min_freq = 652800000,
		.max_freq = 902400000,
		.vco_val = BVAL(21, 20, 1),
		.config_ctl_val = 0x00141400,
	},
	.min_rate = 652800000,
	.max_rate = 1478400000,
	.base = &virt_bases[APCS_C0_PLL_BASE],
	.c =  {
		.parent = &xo_a_clk.c,
		.dbg_name = "a53ss_sr_pll",
		.ops = &clk_ops_sr_pll,
		VDD_MX_SR_FMAX_MAP2(SVS, 1000000000, NOM, 2200000000UL),
		CLK_INIT(a53ss_sr_pll.c),
	},
};

DEFINE_FIXED_DIV_CLK(a53ss_sr_pll_main, 2, &a53ss_sr_pll.c);


/* Early output of PLL */
static struct pll_clk cci_sr_pll = {
	.mode_reg = (void __iomem *)APCS_PLL_MODE,
	.l_reg = (void __iomem *)APCS_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_PLL_USER_CTL,
	.config_ctl_reg = (void __iomem *)APCS_PLL_CONFIG_CTL,
	.status_reg = (void __iomem *)APCS_PLL_STATUS,
	.spm_ctrl = {
		.offset = 0x40,
		.event_bit = 0x0,
	},
	.masks = {
		.vco_mask = BM(21, 20),
		.pre_div_mask = BM(14, 12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.early_output_mask =  BIT(3),
		.main_output_mask = BIT(0),
	},
	.vals = {
		.config_ctl_val = 0x00141400,
		.enable_mn = true,
		.post_div_masked = BVAL(9, 8, (1)),
		.vco_mode_masked = BVAL(21, 20, 1),
	},
	.min_rate = 556800000,
	.max_rate = 902400000,
	.base = &virt_bases[APCS_CCI_PLL_BASE],
	.c = {
		.parent = &xo_a_clk.c,
		.dbg_name = "cci_sr_pll",
		.ops = &clk_ops_sr_pll,
		VDD_MX_SR_FMAX_MAP2(SVS, 1000000000, NOM, 2200000000UL),
		CLK_INIT(cci_sr_pll.c),
	},
};

DEFINE_FIXED_DIV_CLK(cci_sr_pll_main, 2, &cci_sr_pll.c);

static const char const *mux_names[] = {"c0", "c1", "cci"};

#define SAFE_NUM	2

#define SRC_SAFE_FREQ(f1, f2)			  \
	.safe_freq = f1,			\
	.safe_freqs = (unsigned long[SAFE_NUM]) {  \
		[0] = (f1),			  \
		[1] = (f2),			  \
	},					  \
	.safe_num = SAFE_NUM

static struct mux_div_clk a72ssmux = {
	.ops = &rcg_mux_div_ops,
	SRC_SAFE_FREQ(400000000, 800000000),
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a72ssmux",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a72ssmux.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &a72ss_hf_pll_main.c,  3 },
		{ &sys_apcsaux_clk_3.c,  4 },
		{ &sys_apcsaux_clk_2.c,  1 },
		{ &a72ss_hf_pll.c,	 5 },
	),
};

static struct mux_div_clk a53ssmux = {
	.ops = &rcg_mux_div_ops,
	SRC_SAFE_FREQ(400000000, 800000000),
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a53ssmux",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a53ssmux.c),
	},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	MUX_SRC_LIST(
		{ &a53ss_sr_pll.c,	 5 },
		{ &a53ss_sr_pll_main.c,	 3 },
		{ &sys_apcsaux_clk_3.c,  4 },
		{ &sys_apcsaux_clk_2.c,  1 },
	),
};

static struct mux_div_clk ccissmux = {
	.ops = &rcg_mux_div_ops,
	.safe_freq = 200000000,
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
		{ &cci_sr_pll_main.c,	 3 },
		{ &sys_apcsaux_clk_3.c,  4 },
		{ &cci_sr_pll.c,	 5 },
		/* SRC - 2 is Tied off */
	),
};

struct cpu_clk_8976 {
	u32 cpu_reg_mask;
	cpumask_t cpumask;
	bool hw_low_power_ctrl;
	struct pm_qos_request req;
	struct clk c;
};

static void do_nothing(void *unused) { }
#define CPU_LATENCY_NO_L2_PC_US (280)

static inline struct cpu_clk_8976 *to_cpu_clk_8976(struct clk *c)
{
	return container_of(c, struct cpu_clk_8976, c);
}

static enum handoff cpu_clk_8976_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	return HANDOFF_DISABLED_CLK;
}

static long cpu_clk_8976_round_rate(struct clk *c, unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static int cpu_clk_8976_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct cpu_clk_8976 *cpuclk = to_cpu_clk_8976(c);
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
				CPU_LATENCY_NO_L2_PC_US);
		smp_call_function_any(&cpuclk->cpumask, do_nothing,
				NULL, 1);
	}

	ret = clk_set_rate(c->parent, rate);

	/* Remove PM QOS request */
	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);

	return ret;
}

static struct clk_ops clk_ops_cpu = {
	.set_rate = cpu_clk_8976_set_rate,
	.round_rate = cpu_clk_8976_round_rate,
	.handoff = cpu_clk_8976_handoff,
};

static struct cpu_clk_8976 a72_clk = {
	.cpu_reg_mask = 0x103,
	.c = {
		.parent = &a72ssmux.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_a72,
		.dbg_name = "a72_clk",
		CLK_INIT(a72_clk.c),
	},
};

static struct cpu_clk_8976 a53_clk = {
	.cpu_reg_mask = 0x3,
	.c = {
		.parent = &a53ssmux.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_a53,
		.dbg_name = "a53_clk",
		CLK_INIT(a53_clk.c),
	},
};

static struct cpu_clk_8976 cci_clk = {
	.c = {
		.parent = &ccissmux.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_cci,
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

static struct clk_lookup cpu_clocks_8976[] = {
	/* PLL */
	CLK_LIST(a53ss_sr_pll),
	CLK_LIST(a53ss_sr_pll_main),
	CLK_LIST(a72ss_hf_pll),
	CLK_LIST(a72ss_hf_pll_main),
	CLK_LIST(cci_sr_pll),
	CLK_LIST(cci_sr_pll_main),

	/* PLL Sources */
	CLK_LIST(sys_apcsaux_clk_2),
	CLK_LIST(sys_apcsaux_clk_3),

	/* Muxes */
	CLK_LIST(a53ssmux),
	CLK_LIST(a72ssmux),
	CLK_LIST(ccissmux),

	/* CPU clocks */
	CLK_LIST(a72_clk),
	CLK_LIST(a53_clk),
	CLK_LIST(cci_clk),

	/* debug clocks */
	CLK_LIST(apc0_m_clk),
	CLK_LIST(apc1_m_clk),
	CLK_LIST(cci_m_clk),
	CLK_LIST(cpu_debug_pri_mux),
};

static struct mux_div_clk *cpussmux[] = { &a53ssmux, &a72ssmux, &ccissmux };
static struct cpu_clk_8976 *cpuclk[] = { &a53_clk, &a72_clk, &cci_clk};

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node = of_get_cpu_node(cpu, NULL);
	u32 reg;

	if (cpu_node && !of_property_read_u32(cpu_node, "reg", &reg)) {
		if ((reg | a53_clk.cpu_reg_mask) == a53_clk.cpu_reg_mask)
			return &a53_clk.c;
		if ((reg | a72_clk.cpu_reg_mask) == a72_clk.cpu_reg_mask)
			return &a72_clk.c;
	}

	return NULL;
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
	int j = 1;

	rcu_read_lock();

	/* Check if the regulator driver has already populated OPP tables */
	oppl = dev_pm_opp_find_freq_exact(vregdev, 2, true);

	rcu_read_unlock();
	if (!IS_ERR_OR_NULL(oppl))
		use_voltages = true;

	do {
		rate = c->fmax[j++];
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
	} while (1);

	return 0;
}

static void print_opp_table(int a53_cpu, int a72_cpu)
{
	struct opp *oppfmax, *oppfmin;
	unsigned long apc0_fmax = a53_clk.c.fmax[a53_clk.c.num_fmax - 1];
	unsigned long apc1_fmax = a72_clk.c.fmax[a72_clk.c.num_fmax - 1];
	unsigned long apc0_fmin = a53_clk.c.fmax[1];
	unsigned long apc1_fmin = a72_clk.c.fmax[1];

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

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a72_cpu), apc1_fmax,
					     true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a72_cpu), apc1_fmin,
					     true);
	pr_info("clock_cpu: a72: OPP voltage for %lu: %lu\n", apc1_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a72: OPP voltage for %lu: %lu\n", apc1_fmax,
		dev_pm_opp_get_voltage(oppfmax));

	rcu_read_unlock();
}

static void populate_opp_table(struct platform_device *pdev)
{
	struct platform_device *apc0_dev, *apc1_dev;
	struct device_node *apc0_node, *apc1_node;
	unsigned long apc0_fmax, apc1_fmax;
	int cpu, a53_cpu, a72_cpu;

	apc0_node = of_parse_phandle(pdev->dev.of_node, "vdd_a53-supply", 0);
	apc1_node = of_parse_phandle(pdev->dev.of_node, "vdd_a72-supply", 0);
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
	apc1_fmax = a72_clk.c.fmax[a72_clk.c.num_fmax - 1];

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_clk.c) {
			a53_cpu = cpu;
			WARN(add_opp(&a53_clk.c, get_cpu_device(cpu),
				     &apc0_dev->dev, apc0_fmax),
				     "Failed to add OPP levels for A53\n");
		}
		if (logical_cpu_to_clk(cpu) == &a72_clk.c) {
			a72_cpu = cpu;
			WARN(add_opp(&a72_clk.c, get_cpu_device(cpu),
				     &apc1_dev->dev, apc1_fmax),
				     "Failed to add OPP levels for A72\n");
		}
	}

	/* One time print during bootup */
	pr_info("clock-cpu-8976: OPP tables populated (cpu %d and %d)\n",
							a53_cpu, a72_cpu);

	print_opp_table(a53_cpu, a72_cpu);
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

	*bin = (pte_efuse >> 2) & 0x7;

	dev_info(&pdev->dev, "Speed bin: %d PVS Version: %d\n", *bin,
								*version);
}

static int cpu_parse_devicetree(struct platform_device *pdev)
{
	struct resource *res;
	int mux_id;
	char rcg_name[] = "xxx-mux";
	char pll_name[] = "xxx-pll";
	struct clk *c;

	for (mux_id = 0; mux_id <= APCS_CCI_PLL_BASE; mux_id++) {
		snprintf(pll_name, ARRAY_SIZE(pll_name), "%s-pll",
						mux_names[mux_id]);
		res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, pll_name);
		if (!res) {
			dev_err(&pdev->dev, "missing %s\n", pll_name);
			return -EINVAL;
		}

		virt_bases[mux_id] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (!virt_bases[mux_id]) {
			dev_err(&pdev->dev, "ioremap failed for %s\n",
								pll_name);
			return -ENOMEM;
		}
	}

	/* HF PLL Analog Supply */
	vdd_hf.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_hf_pll");
	if (IS_ERR(vdd_hf.regulator[0])) {
		if (PTR_ERR(vdd_hf.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_hf_pll regulator!!!\n");
		return PTR_ERR(vdd_hf.regulator[0]);
	}

	/* HF PLL core logic */
	vdd_hf.regulator[1] = devm_regulator_get(&pdev->dev,
							"vdd_mx_hf");
	if (IS_ERR(vdd_hf.regulator[1])) {
		if (PTR_ERR(vdd_hf.regulator[1]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_mx_hf regulator!!!\n");
		return PTR_ERR(vdd_hf.regulator[1]);
	}
	vdd_hf.use_max_uV = true;

	/* SR PLLs core logic */
	vdd_mx_sr.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_mx_sr");
	if (IS_ERR(vdd_mx_sr.regulator[0])) {
		if (PTR_ERR(vdd_mx_sr.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_mx_sr regulator!!!\n");
		return PTR_ERR(vdd_mx_sr.regulator[0]);
	}
	vdd_mx_sr.use_max_uV = true;

	vdd_cpu_a72.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_a72");
	if (IS_ERR(vdd_cpu_a72.regulator[0])) {
		if (PTR_ERR(vdd_cpu_a72.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_a72 regulator!!!\n");
		return PTR_ERR(vdd_cpu_a72.regulator[0]);
	}
	vdd_cpu_a72.use_max_uV = true;

	vdd_cpu_a53.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_a53");
	if (IS_ERR(vdd_cpu_a53.regulator[0])) {
		if (PTR_ERR(vdd_cpu_a53.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_a53 regulator!!!\n");
		return PTR_ERR(vdd_cpu_a53.regulator[0]);
	}
	vdd_cpu_a53.use_max_uV = true;

	vdd_cpu_cci.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_cci");
	if (IS_ERR(vdd_cpu_cci.regulator[0])) {
		if (PTR_ERR(vdd_cpu_cci.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_cci regulator!!!\n");
		return PTR_ERR(vdd_cpu_cci.regulator[0]);
	}
	vdd_cpu_cci.use_max_uV = true;

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

	/* Sources of the PLL */
	c = devm_clk_get(&pdev->dev, "xo_a");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo (rc = %ld)!\n",
				PTR_ERR(c));
		return PTR_ERR(c);
	}
	xo_a_clk.c.parent = c;

	c = devm_clk_get(&pdev->dev, "aux_clk_2");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll4 (rc = %ld)!\n",
				PTR_ERR(c));
		return PTR_ERR(c);
	}
	sys_apcsaux_clk_2.c.parent = c;

	c = devm_clk_get(&pdev->dev, "aux_clk_3");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll0 (rc = %ld)!\n",
				PTR_ERR(c));
		return PTR_ERR(c);
	}
	sys_apcsaux_clk_3.c.parent = c;

	return 0;
}

#define GLB_DIAG	0x0b11101c

static int clock_cpu_probe(struct platform_device *pdev)
{
	int speed_bin, version, rc, cpu, mux_id;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	unsigned long a72rate, a53rate, ccirate;

	a53ss_sr_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	a72ss_hf_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;
	cci_sr_pll_main.c.flags = CLKFLAG_NO_RATE_CACHE;

	get_speed_bin(pdev, &speed_bin, &version);

	rc = cpu_parse_devicetree(pdev);
	if (rc)
		return rc;

	for (mux_id = 0; mux_id < A53SS_MUX_NUM; mux_id++) {
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

	/* Debug Mux */
	virt_bases[APCS0_DBG_BASE] = devm_ioremap(&pdev->dev, GLB_DIAG, SZ_8);
	if (!virt_bases[APCS0_DBG_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap GLB_DIAG registers\n");
		return -ENOMEM;
	}

	rc = of_msm_clock_register(pdev->dev.of_node,
				cpu_clocks_8976, ARRAY_SIZE(cpu_clocks_8976));
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
		WARN(clk_prepare_enable(&cpuclk[cpu/4]->c),
				"Unable to turn on CPU clock");
	}

	a53rate = clk_get_rate(&a53_clk.c);
	if (!a53rate) {
		dev_err(&pdev->dev, "Unknown a53 rate. Setting safe rate, rate %ld\n",
						sys_apcsaux_clk_3.c.rate);
		rc = clk_set_rate(&a53_clk.c, sys_apcsaux_clk_3.c.rate);
		if (rc)
			dev_err(&pdev->dev, "Can't set safe rate\n");
	}

	a72rate = clk_get_rate(&a72_clk.c);
	if (!a72rate) {
		dev_err(&pdev->dev, "Unknown a72 rate. Setting safe rate, rate %ld\n",
						sys_apcsaux_clk_3.c.rate);
		rc = clk_set_rate(&a72_clk.c, sys_apcsaux_clk_3.c.rate);
		if (rc)
			dev_err(&pdev->dev, "Can't set safe rate\n");
	}

	ccirate = clk_get_rate(&cci_clk.c);
	if (!ccirate) {
		dev_err(&pdev->dev, "Unknown cci rate. Setting safe rate, rate %ld\n",
						sys_apcsaux_clk_3.c.rate);
		rc = clk_set_rate(&cci_clk.c, sys_apcsaux_clk_3.c.rate);
		if (rc)
			dev_err(&pdev->dev, "Can't set safe rate\n");
	}

	put_online_cpus();

	populate_opp_table(pdev);

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc)
		return rc;

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_clk.c)
			cpumask_set_cpu(cpu, &a53_clk.cpumask);
		if (logical_cpu_to_clk(cpu) == &a72_clk.c)
			cpumask_set_cpu(cpu, &a72_clk.cpumask);
	}

	a53_clk.hw_low_power_ctrl = true;
	a72_clk.hw_low_power_ctrl = true;

	return 0;
}

static struct of_device_id clock_cpu_match_table[] = {
	{.compatible = "qcom,cpu-clock-8976"},
	{}
};

static struct platform_driver clock_cpu_driver = {
	.probe = clock_cpu_probe,
	.driver = {
		.name = "cpu-clock-8976",
		.of_match_table = clock_cpu_match_table,
		.owner = THIS_MODULE,
	},
};

static int msm_cpu_spm_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm_c0_base");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined for c0\n");
		return -ENOMEM;
	}

	a53ss_sr_pll.spm_ctrl.spm_base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!a53ss_sr_pll.spm_ctrl.spm_base) {
		dev_err(&pdev->dev, "Failed to ioremap c0 spm registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm_c1_base");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined for c1\n");
		return -ENOMEM;
	}

	a72ss_hf_pll.spm_ctrl.spm_base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!a72ss_hf_pll.spm_ctrl.spm_base) {
		dev_err(&pdev->dev, "Failed to ioremap c1 spm registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"spm_cci_base");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined for cci\n");
		return -ENOMEM;
	}

	cci_sr_pll.spm_ctrl.spm_base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!cci_sr_pll.spm_ctrl.spm_base) {
		dev_err(&pdev->dev, "Failed to ioremap cci spm registers\n");
		return -ENOMEM;
	}

	dev_info(&pdev->dev, "Registered CPU SPM clocks\n");

	return 0;
}

static struct of_device_id msm_clock_spm_match_table[] = {
	{ .compatible = "qcom,cpu-spm-8976" },
	{}
};

static struct platform_driver msm_clock_spm_driver = {
	.probe = msm_cpu_spm_probe,
	.driver = {
		.name = "qcom,cpu-spm-8976",
		.of_match_table = msm_clock_spm_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clock_cpu_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&clock_cpu_driver);
	if (!ret)
		ret = platform_driver_register(&msm_clock_spm_driver);
	return ret;
}
arch_initcall(clock_cpu_init);

#define APCS_ALIAS1_CMD_RCGR		0xb011050
#define APCS_ALIAS1_CFG_OFF		0x4
#define APCS_ALIAS1_CORE_CBCR_OFF	0x8
#define SRC_SEL				0x4
#define SRC_DIV				0x1

static int __init cpu_clock_a72_init(void)
{
	void __iomem  *base;
	int regval = 0, count;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
							"qcom,cpu-clock-8976");

	if (!ofnode)
		return 0;

	base = ioremap_nocache(APCS_ALIAS1_CMD_RCGR, SZ_8);
	regval = readl_relaxed(base);

	/* Source GPLL0 and at the rate of GPLL0 */
	regval = (SRC_SEL << 8) | SRC_DIV; /* 0x401 */
	writel_relaxed(regval, base + APCS_ALIAS1_CFG_OFF);
	/* Make sure src sel and src div is set before update bit */
	mb();

	/* update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		if (!(readl_relaxed(base)) & BIT(0))
			break;
		udelay(1);
	}
	if (!(readl_relaxed(base)) & BIT(0))
		panic("A72 RCG configuration didn't update!\n");

	/* Enable the branch */
	regval =  readl_relaxed(base + APCS_ALIAS1_CORE_CBCR_OFF);
	regval |= BIT(0);
	writel_relaxed(regval, base + APCS_ALIAS1_CORE_CBCR_OFF);
	/* Branch enable should be complete */
	mb();
	iounmap(base);

	pr_info("A72 Power clocks configured\n");

	return 0;
}
early_initcall(cpu_clock_a72_init);
