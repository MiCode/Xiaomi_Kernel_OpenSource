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
#include <linux/cpu_pm.h>
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

#include <dt-bindings/clock/msm-clocks-8953.h>

#include "clock.h"

#define APCS_PLL_MODE			0x0
#define APCS_PLL_L_VAL			0x8
#define APCS_PLL_ALPHA_VAL		0x10
#define APCS_PLL_USER_CTL		0x18
#define APCS_PLL_CONFIG_CTL_LO		0x20
#define APCS_PLL_CONFIG_CTL_HI		0x24
#define APCS_PLL_STATUS			0x28
#define APCS_PLL_TEST_CTL_LO		0x30
#define APCS_PLL_TEST_CTL_HI		0x34

#define FREQ_BOOST_MODE_EN		0x200
#define FREQ_BOOST_FSM_CTRL		0x204
#define FREQ_PLL_CLK_SRC		0x208
#define FREQ_BOOST_RATIO_REGS_CORE0	0x20C
#define FREQ_BOOST_SAFE_PLL_L_VAL	0x21C
#define FREQ_BOOST_TIMER1_CNT_VAL	0x224
#define FREQ_BOOST_TIMER2_CNT_VAL	0x228
#define FREQ_BOOST_TIMER3_CNT_VAL	0x22C
#define FREQ_BOOST_TIMER4_CNT_VAL	0x230
#define FREQ_BOOST_STATUS_REGISTER	0x234
#define HW_MODE_CTRL			0x300
#define CORE_COUNT_THRESHOLD		0x304
#define HYST_TIMEOUT			0x308
#define HYST_TIMER_CONFIG		0x30C
#define PERF_BOOST_L_VALUE		0x310
#define VMIN_STATUS_REGISTER		0x314
#define PERF_BOOST_SH_STATUS_REG	0x31C
#define PERF_BOOST_DEEP_STATUS_REG	0x320

#define APCS0_CPR_CORE_ADJ_MODE_REG	0x0b018798
#define APCS0_CPR_MARGIN_ADJ_CTL_REG	0x0b0187F8
#define GLB_DIAG			0x0b11101c

#define VMIN_FEATURE_EN_BIT		BIT(0)
#define PERF_BOOST_FEATURE_EN_BIT	BIT(1)
#define SW_DCVS_EN_BIT			BIT(6)
#define CPR_MODE_CHANGE_BIT		BIT(0)
#define CPR_MODE_CHANGE_READY_BIT	BIT(31)

#define CC_UNIFIED_UPPER_LIMIT_MSB	15
#define CC_UNIFIED_UPPER_LIMIT_LSB	12
#define CC_UNIFIED_THRESHOLD_MSB	11
#define CC_UNIFIED_THRESHOLD_LSB	 8
#define CC_SPLIT_UPPER_LIMIT_MSB	 7
#define CC_SPLIT_UPPER_LIMIT_LSB	 4
#define CC_SPLIT_THRESHOLD_MSB		 3
#define CC_SPLIT_THRESHOLD_LSB		 0

#define UNIFIED_CPR_MUX_SEL_MSB		 9
#define UNIFIED_CPR_MUX_SEL_LSB		 8

#define CPR_MARGIN_CORE_ADJ_EN		 1
#define CPR_MARGIN_TEMP_ADJ_EN		 2

#define BOTH_FSM			 2
#define PLL_CLK_SRC			 1

#define SPLIT_UPL			 4
#define SPLIT_THRESHOLD			 3

#define MAX_L_VAL			156
#define MAX_SAFE_L_VAL			104
#define TIMER3_DELAY_VAL		0x4
#define TIMER3_E3_DELAY_VAL		0x1809
#define CCI_AFFINITY_LEVEL		2

#define UPDATE_CHECK_MAX_LOOPS	5000
#define MAX_DEBUG_BUF_LEN	500
#define CCI_RATE(rate)		(div_u64((rate * 10ULL), 25))
#define PLL_MODE(x)		(*(x)->base + (unsigned long) (x)->mode_reg)
#define VALUE(val, msb, lsb)	((val & BM(msb, lsb)) >> lsb)

#define B_VAL(msb, lsb, val, reg)     { \
	reg &= ~BM(msb, lsb); \
	reg |= (((val) << lsb) & BM(msb, lsb)); \
}

#define HZ_TO_LVAL(val, fmax, src_rate) { \
	val = val * 1000000; \
	val = val + fmax; \
	val = val / src_rate; \
}

static DEFINE_MUTEX(debug_buf_mutex);
static char debug_buf[MAX_DEBUG_BUF_LEN];


enum {
	APCS_C0_PLL_BASE,
	APCS_C1_PLL_BASE,
	APCS0_DBG_BASE,
	APCS0_CPR_CORE_BASE,
	APCS0_CPR_MARGIN_BASE,
	N_BASES,
};

enum vmin_boost_mode {
	VMIN,
	PBOOST,
	VMIN_BOOST_NONE,
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

struct core_count_thresh {
	u8 both_thres;
	u8 split_upl;
	u8 split_thres;
	u8 unified_upl;
	u8 unified_thres;
};

struct apcs_pll_ctrl {
	int *boost_safe_l_val;
	u32 timer3_cnt_val;
	u32 hw_mode_ctrl;
	u32 pll_clk_src;

	u32 pboost_l_val;

	/* Boot time enable flags for modes */
	bool vmin_enable;
	bool perfb_enable;

	struct core_count_thresh vmin_cc;
	struct core_count_thresh pb_cc;
};

struct cpu_clk_8953 {
	u32 cpu_reg_mask;
	cpumask_t cpumask;
	bool hw_low_power_ctrl;
	bool set_rate_done;
	enum vmin_boost_mode curr_mode;
	s32 cpu_latency_no_l2_pc_us;
	struct pm_qos_request req;
	struct clk c;
	struct apcs_pll_ctrl ctrl;
};

static struct cpu_clk_8953 a53_pwr_clk;
static struct cpu_clk_8953 a53_perf_clk;
static struct cpu_clk_8953 cci_clk;

static inline struct cpu_clk_8953 *to_cpu_clk_8953(struct clk *c)
{
	return container_of(c, struct cpu_clk_8953, c);
}

static enum handoff cpu_clk_8953_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	return HANDOFF_DISABLED_CLK;
}

static long cpu_clk_8953_round_rate(struct clk *c, unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static int cpr_mode_check(bool enable)
{
	u32 regval, rval_core, count;

	if (!virt_bases[APCS0_CPR_MARGIN_BASE] ||
			!virt_bases[APCS0_CPR_CORE_BASE])
		return -EINVAL;

	regval = readl_relaxed(virt_bases[APCS0_CPR_CORE_BASE]);

	/* Enable == 0 clear cpr mode bit */
	if (!enable) {
		if (regval) {
			regval &= ~CPR_MODE_CHANGE_BIT;
			writel_relaxed(regval, virt_bases[APCS0_CPR_CORE_BASE]);
		}
		return 0;
	}

	rval_core = readl_relaxed(virt_bases[APCS0_CPR_MARGIN_BASE]);
	rval_core &= BM(CPR_MARGIN_TEMP_ADJ_EN, CPR_MARGIN_CORE_ADJ_EN);
	if (!rval_core) {
		pr_debug("Check the TEMP_ADJ_EN & CORE_ADJ_EN bits %d!!!\n",
							rval_core);
		return -EINVAL;
	}

	/* Enable == 1 set cpr mode bit */
	regval |= CPR_MODE_CHANGE_BIT;
	writel_relaxed(regval, virt_bases[APCS0_CPR_CORE_BASE]);

	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		regval = readl_relaxed(virt_bases[APCS0_CPR_CORE_BASE]);
		regval &= CPR_MODE_CHANGE_READY_BIT;
		if (regval) {
			pr_debug("CPR Mode Ready 0x%x\n",
			readl_relaxed(virt_bases[APCS0_CPR_CORE_BASE]));
			break;
		}
		udelay(1);
	}

	BUG_ON(count == 0);

	return 0;
}

static void sw_dcvs_enable(void)
{
	u32 regval;

	regval = readl_relaxed(virt_bases[APCS_C0_PLL_BASE] + HW_MODE_CTRL);
	regval &= ~SW_DCVS_EN_BIT;
	writel_relaxed(regval, virt_bases[APCS_C0_PLL_BASE] + HW_MODE_CTRL);

	/* Make sure the registers are update before enable */
	mb();
}

static void sw_dcvs_disable(void)
{
	u32 regval;

	regval = readl_relaxed(virt_bases[APCS_C0_PLL_BASE] + HW_MODE_CTRL);
	regval |= SW_DCVS_EN_BIT;
	writel_relaxed(regval, virt_bases[APCS_C0_PLL_BASE] + HW_MODE_CTRL);

	/* Make sure the registers are update before enable */
	mb();
}

static int update_mode(bool enable, enum vmin_boost_mode boost_mode)
{
	u32 regval_c0, regval_c1;

	regval_c0 = readl_relaxed(virt_bases[APCS_C0_PLL_BASE] + HW_MODE_CTRL);
	regval_c1 = readl_relaxed(virt_bases[APCS_C1_PLL_BASE] + HW_MODE_CTRL);

	switch (boost_mode) {
	case VMIN:
		if (enable) {
			regval_c0 |= VMIN_FEATURE_EN_BIT;
			regval_c1 |= VMIN_FEATURE_EN_BIT;
		} else {
			regval_c0 &= ~VMIN_FEATURE_EN_BIT;
			regval_c1 &= ~VMIN_FEATURE_EN_BIT;
		}
		break;
	case PBOOST:
		if (enable) {
			regval_c0 |= PERF_BOOST_FEATURE_EN_BIT;
			regval_c1 |= PERF_BOOST_FEATURE_EN_BIT;
		} else {
			regval_c0 &= ~PERF_BOOST_FEATURE_EN_BIT;
			regval_c1 &= ~PERF_BOOST_FEATURE_EN_BIT;
		}
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(regval_c0, virt_bases[APCS_C0_PLL_BASE] + HW_MODE_CTRL);
	writel_relaxed(regval_c1, virt_bases[APCS_C1_PLL_BASE] + HW_MODE_CTRL);

	/* Make sure the registers are updated */
	mb();

	return 0;
}

static int fsm_hardware_init(struct cpu_clk_8953 *cpuclk,
	struct core_count_thresh mode_cc, u32 l_val,
	enum vmin_boost_mode boost_mode)
{
	int regval = 0, i = 0;
	void __iomem *base;

	switch (boost_mode) {
	case VMIN:
		for (i = 0; i < APCS0_DBG_BASE; i++) {
			base = virt_bases[i];
			writel_relaxed(l_val,
					base + FREQ_BOOST_SAFE_PLL_L_VAL);
		}
		break;
	case PBOOST:
		for (i = 0; i < APCS0_DBG_BASE; i++) {
			base = virt_bases[i];
			writel_relaxed(l_val,
					base + FREQ_BOOST_SAFE_PLL_L_VAL);
			writel_relaxed(cpuclk->ctrl.pboost_l_val,
					base + PERF_BOOST_L_VALUE);
		}
		break;
	default:
		return -EINVAL;
	};

	/* Make sure the registers are updated */
	mb();

	/* configure the core count for C0 & C1 */
	for (i = 0; i < APCS0_DBG_BASE; i++) {
		base = virt_bases[i];

		regval = readl_relaxed(base + CORE_COUNT_THRESHOLD);
		B_VAL(CC_UNIFIED_UPPER_LIMIT_MSB, CC_UNIFIED_UPPER_LIMIT_LSB,
				mode_cc.unified_upl, regval);
		B_VAL(CC_UNIFIED_THRESHOLD_MSB, CC_UNIFIED_THRESHOLD_LSB,
				mode_cc.unified_thres, regval);
		B_VAL(CC_SPLIT_UPPER_LIMIT_MSB, CC_SPLIT_UPPER_LIMIT_LSB,
				mode_cc.split_upl, regval);
		B_VAL(CC_SPLIT_THRESHOLD_MSB, CC_SPLIT_THRESHOLD_LSB,
				mode_cc.split_thres, regval);
		writel_relaxed(regval, base + CORE_COUNT_THRESHOLD);

		regval = readl_relaxed(base + HW_MODE_CTRL);
		B_VAL(UNIFIED_CPR_MUX_SEL_MSB, UNIFIED_CPR_MUX_SEL_LSB,
				cpuclk->ctrl.hw_mode_ctrl, regval);
		writel_relaxed(regval, base + HW_MODE_CTRL);

		regval = readl_relaxed(base + FREQ_PLL_CLK_SRC);
		regval |= cpuclk->ctrl.pll_clk_src;
		writel_relaxed(regval, base + FREQ_PLL_CLK_SRC);

		regval = TIMER3_DELAY_VAL;
		writel_relaxed(regval, base + FREQ_BOOST_TIMER3_CNT_VAL);
		cpuclk->ctrl.timer3_cnt_val = readl_relaxed(base +
						FREQ_BOOST_TIMER3_CNT_VAL);

		/* Make sure the registers are updated */
		mb();
	};

	return 0;
}

static int cpu_clk_8953_pre_rate(struct clk *c, unsigned long new_rate)
{
	struct cpu_clk_8953 *cpuclk = to_cpu_clk_8953(c);
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;
	unsigned long fmax = c->fmax[c->num_fmax - 1];

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
	}

	cpr_mode_check(true);

	switch (cpuclk->curr_mode) {
	case VMIN:
	case PBOOST:
		sw_dcvs_disable();
		update_mode(false, cpuclk->curr_mode);
		if (new_rate == fmax || !cpuclk->ctrl.vmin_enable)
			cpuclk->curr_mode = VMIN_BOOST_NONE;
		break;
	default:
		break;
	}

	return 0;
}

static int cpu_clk_8953_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct cpu_clk_8953 *cpuclk = to_cpu_clk_8953(c);
	unsigned long fmax = c->fmax[c->num_fmax - 1];
	int level = find_vdd_level(c, rate);
	u32 safe_l = 0;

	ret = clk_set_rate(c->parent, rate);
	if (!ret) {
		/* update the rates of perf & power cluster */
		if (c == &a53_pwr_clk.c)
			a53_perf_clk.c.rate = rate;
		if (c == &a53_perf_clk.c)
			a53_pwr_clk.c.rate  = rate;
		cci_clk.c.rate = CCI_RATE(rate);
	}

	if (cpuclk->ctrl.vmin_enable && (rate != fmax)) {
		safe_l = cpuclk->ctrl.boost_safe_l_val[level];
		if (!safe_l) {
			cpuclk->curr_mode = VMIN_BOOST_NONE;
			goto out;
		}

		fsm_hardware_init(cpuclk, cpuclk->ctrl.vmin_cc, safe_l, VMIN);
		update_mode(true, VMIN);
		sw_dcvs_enable();
		cpuclk->curr_mode = VMIN;
	} else if (cpuclk->ctrl.perfb_enable && (rate == fmax)) {
		safe_l = cpuclk->ctrl.boost_safe_l_val[level];
		if (!safe_l || !cpuclk->ctrl.pboost_l_val) {
			cpuclk->curr_mode = VMIN_BOOST_NONE;
			goto out;
		}

		fsm_hardware_init(cpuclk, cpuclk->ctrl.pb_cc, safe_l, PBOOST);
		update_mode(true, PBOOST);
		sw_dcvs_enable();
		cpuclk->curr_mode = PBOOST;
	}

out:
	return ret;
}

void cpu_clk_8953_post_rate(struct clk *c, unsigned long old_rate)
{
	struct cpu_clk_8953 *cpuclk = to_cpu_clk_8953(c);
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;

	cpr_mode_check(false);

	/* Remove PM QOS request */
	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);
}

static int cpu_clk_cci_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct cpu_clk_8953 *cpuclk = to_cpu_clk_8953(c);

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
	.pre_set_rate = cpu_clk_8953_pre_rate,
	.set_rate = cpu_clk_8953_set_rate,
	.post_set_rate = cpu_clk_8953_post_rate,
	.round_rate = cpu_clk_8953_round_rate,
	.handoff = cpu_clk_8953_handoff,
};

static struct clk_ops clk_ops_cci = {
	.set_rate = cpu_clk_cci_set_rate,
	.round_rate = cpu_clk_8953_round_rate,
	.handoff = cpu_clk_8953_handoff,
};

static struct cpu_clk_8953 a53_pwr_clk = {
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

static struct cpu_clk_8953 a53_perf_clk = {
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

static struct cpu_clk_8953 cci_clk = {
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

static struct clk_lookup cpu_clocks_8953[] = {
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
static struct cpu_clk_8953 *cpuclk[] = { &a53_pwr_clk, &a53_perf_clk,
						&cci_clk};
/* Fast forward declaration */
static void initialize_mode_control(enum vmin_boost_mode bmode, u8 uni_upl,
		u8 uni_thres, enum vmin_boost_mode cur_mode);

/* Debugfs support */
static ssize_t boost_mode_get(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	struct cpu_clk_8953 *clk;
	int rc = 0, output = 0;
	u32 mode = 0;

	if (IS_ERR(file) || file == NULL) {
		pr_err("Function Input Error %ld\n", PTR_ERR(file));
		return -ENOMEM;
	}

	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	mode = clk->curr_mode;

	output = snprintf(debug_buf, MAX_DEBUG_BUF_LEN-1, "Current Mode: %s\n",
		(mode) ? (mode == VMIN_BOOST_NONE) ? "VMIN_BOOST_NONE" :
						"PBOOST" : "VMIN");

	rc = simple_read_from_buffer((void __user *) buf, output, ppos,
					(void *) debug_buf, output);
	return rc;
}

static const struct file_operations boost_mode_fops = {
	.read = boost_mode_get,
	.open = simple_open,
};

static int boost_enable_set(void *data, u64 val)
{
	struct clk clock;
	struct cpu_clk_8953 *clk;
	unsigned long numfmax;
	u32 mode = 0;

	clock = cpuclk[0]->c;
	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	mode = clk->curr_mode;
	numfmax = clock.num_fmax - 1;

	if (mode == PBOOST) {
		pr_err("pboost registers can't be configured in current mode %s\n",
			(mode) ? (mode == VMIN_BOOST_NONE) ? "VMIN_BOOST_NONE" :
							"PBOOST" : "VMIN");
		return -EINVAL;
	}

	if (!val) {
		clk->ctrl.perfb_enable = false;
		pr_info("Perf Boost feature disabled\n");
		return -EINVAL;
	}

	if (!clk->ctrl.boost_safe_l_val[numfmax]) {
		pr_err("safe_L value can't be 'zero'\n");
		return -EINVAL;
	}

	if (!clk->ctrl.pboost_l_val) {
		pr_err("boost_L value can't be 'zero'\n");
		return -EINVAL;
	}

	if (!(clk->ctrl.pb_cc.unified_upl) ||
			!(clk->ctrl.pb_cc.unified_thres)) {
		pr_err("Unified limit/threshold can't be 'zero'\n");
		return -EINVAL;
	}

	initialize_mode_control(PBOOST, clk->ctrl.pb_cc.unified_upl,
			clk->ctrl.pb_cc.unified_thres, mode);

	if (clk->ctrl.perfb_enable)
		pr_info("Perf Boost feature enabled\n");

	return 0;
}

static int boost_enable_get(void *data, u64 *val)
{
	struct cpu_clk_8953 *clk;
	u32 enable = 0;

	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	enable = clk->ctrl.perfb_enable;

	*val = enable;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(boost_enable_fops, boost_enable_get,
				boost_enable_set, "%lld\n");

static int boost_safe_l_set(void *data, u64 val)
{
	struct clk clock;
	struct cpu_clk_8953 *clk;
	unsigned long numfmax;
	u32 mode = 0;

	clock = cpuclk[0]->c;
	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	mode = clk->curr_mode;
	numfmax = clock.num_fmax - 1;

	if (!val) {
		pr_info("Perf Boost safe_L value can't be 'zero'\n");
		return -EINVAL;
	}

	if (val > MAX_SAFE_L_VAL) {
		pr_info("Perf Boost safe_L value can't be > than %d\n",
				MAX_SAFE_L_VAL);
		return -EINVAL;
	}

	if (mode == PBOOST) {
		pr_err("pboost registers can't be configured in current mode %s\n",
			(mode) ? (mode == VMIN_BOOST_NONE) ? "VMIN_BOOST_NONE" :
							"PBOOST" : "VMIN");
		return -EINVAL;
	}

	clk->ctrl.boost_safe_l_val[numfmax] = val;

	return 0;
}

static int boost_safe_l_get(void *data, u64 *val)
{
	struct clk clock;
	struct cpu_clk_8953 *clk;
	unsigned long numfmax;

	clock = cpuclk[0]->c;
	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	numfmax = clock.num_fmax - 1;

	*val = clk->ctrl.boost_safe_l_val[numfmax];

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(boost_safe_l_fops, boost_safe_l_get,
				boost_safe_l_set, "%lld\n");

static int boost_l_set(void *data, u64 val)
{
	struct clk clock;
	struct cpu_clk_8953 *clk;
	unsigned long numfmax;
	u32 mode = 0;

	clock = cpuclk[0]->c;
	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	mode = clk->curr_mode;
	numfmax = clock.num_fmax - 1;

	if (!val) {
		pr_info("Perf Boost boost-l value can't be 'zero'\n");
		return -EINVAL;
	}

	if (mode == PBOOST) {
		pr_err("pboost registers can't be configured in current mode %s\n",
			(mode) ? (mode == VMIN_BOOST_NONE) ? "VMIN_BOOST_NONE" :
							"PBOOST" : "VMIN");
		return -EINVAL;
	}

	if (val > MAX_L_VAL) {
		pr_err("boost_L value can't be > than %x\n", MAX_L_VAL);
		return -EINVAL;
	}

	clk->ctrl.pboost_l_val = val;

	return 0;
}

static int boost_l_get(void *data, u64 *val)
{
	struct cpu_clk_8953 *clk;

	clk = to_cpu_clk_8953(&cpuclk[0]->c);

	*val = clk->ctrl.pboost_l_val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(boost_l_fops, boost_l_get,
				boost_l_set, "%lld\n");

static ssize_t num_cores_set(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	int filled;
	struct cpu_clk_8953 *clk;
	u32 upl = 0, thres = 0, mode = 0;

	if (IS_ERR(file) || file == NULL) {
		pr_err("Function Input Error %ld\n", PTR_ERR(file));
		return -ENOMEM;
	}

	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	mode = clk->curr_mode;

	if (mode == PBOOST) {
		pr_err("pboost registers can't be configured in current mode %s\n",
			(mode) ? (mode == VMIN_BOOST_NONE) ? "VMIN_BOOST_NONE" :
							"PBOOST" : "VMIN");
		return -EINVAL;
	}


	if (count < MAX_DEBUG_BUF_LEN) {
		if (copy_from_user(debug_buf, (void __user *) buf, count))
			return -EFAULT;

		debug_buf[count] = '\0';
		filled = sscanf(debug_buf, "%u %u", &upl, &thres);

		/* check that user entered two numbers */
		if (filled < 2) {
			pr_err("Error: 'echo \"UNIFIED_UPPER_LIMIT UNIFIED_THRESHOLD > num_cores\n");
			return -EINVAL;
		} else if (upl == 0 || thres == 0) {
			pr_err("Error, values can not be 0\n");
			return -EINVAL;
		}
	}

	clk->ctrl.pb_cc.unified_upl = upl;
	clk->ctrl.pb_cc.unified_thres = thres;

	return count;
}

static ssize_t num_cores_get(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	struct cpu_clk_8953 *clk;
	int rc = 0, output = 0;
	u32 cores = 0;

	if (IS_ERR(file) || file == NULL) {
		pr_err("Function Input Error %ld\n", PTR_ERR(file));
		return -ENOMEM;
	}

	clk = to_cpu_clk_8953(&cpuclk[0]->c);

	cores = clk->ctrl.pb_cc.unified_upl - clk->ctrl.pb_cc.unified_thres;

	output = snprintf(debug_buf, MAX_DEBUG_BUF_LEN-1,
		"Max no. of cores in Perf Boost Mode: %d\n", cores);

	rc = simple_read_from_buffer((void __user *) buf, output, ppos,
					(void *) debug_buf, output);
	return rc;
}

static const struct file_operations max_cores_fops = {
	.write = num_cores_set,
	.read  = num_cores_get,
	.open  = simple_open,
};

static ssize_t hardware_status_get(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	struct cpu_clk_8953 *clk;
	u32 mode = 0, regval = 0, feature = 0, fsm_state = 0;
	int rc = 0, output = 0;


	if (IS_ERR(file) || file == NULL) {
		pr_err("Function Input Error %ld\n", PTR_ERR(file));
		return -ENOMEM;
	}

	clk = to_cpu_clk_8953(&cpuclk[0]->c);
	mode = clk->curr_mode;

	if (mode == VMIN) {
		regval = readl_relaxed(virt_bases[APCS_C0_PLL_BASE] +
				VMIN_STATUS_REGISTER);
		/* 0 --> HW Indicates feature is enabled, SW indicates as 1 */
		feature = !(VALUE(regval, 19, 19));
		fsm_state = VALUE(regval, 6, 4);
		output = snprintf(debug_buf, MAX_DEBUG_BUF_LEN-1,
		"Feature enabled: %d fsm_state %d max_core_allowed %d\n",
			feature, fsm_state, VALUE(regval, 12, 10));

	} else if (mode == PBOOST) {
		regval = readl_relaxed(virt_bases[APCS_C0_PLL_BASE] +
				PERF_BOOST_SH_STATUS_REG);
		fsm_state = VALUE(regval, 7, 4);
		regval = readl_relaxed(virt_bases[APCS_C0_PLL_BASE] +
				PERF_BOOST_DEEP_STATUS_REG);
		/* 0 --> HW Indicates feature is enabled, SW indicates as 1 */
		feature = !(VALUE(regval, 27, 27));
		output = snprintf(debug_buf, MAX_DEBUG_BUF_LEN-1,
		"Feature enabled: %d fsm_state %d\n", feature, fsm_state);

	} else
		output = snprintf(debug_buf, MAX_DEBUG_BUF_LEN-1,
				"Currently mode is VMIN_BOOST_NONE\n");

	rc = simple_read_from_buffer((void __user *) buf, output, ppos,
					(void *) debug_buf, output);
	return rc;
}

static const struct file_operations hardware_status_fops = {
	.read = hardware_status_get,
	.open = simple_open,
};

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
	pr_info("clock-cpu-8953: OPP tables populated (cpu %d and %d)\n",
						a53_pwr_cpu, a53_perf_cpu);

	print_opp_table(a53_pwr_cpu, a53_perf_cpu);
}

static int of_get_fmax_vdd_class(struct platform_device *pdev, struct clk *c,
		char *prop_name, bool vmin, bool pboost, u32 boost_index)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, i, len, temp = 0, rc = 0;
	struct clk_vdd_class *vdd = c->vdd_class;
	struct cpu_clk_8953 *cpuclk = to_cpu_clk_8953(c);
	u32 *array, delta;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	if (vmin || pboost)
		len = vdd->num_regulators + 2;
	else
		len = vdd->num_regulators + 1;

	prop_len /= sizeof(u32);
	if (prop_len % len) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= len;
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

	if (vmin || pboost) {
		cpuclk->ctrl.boost_safe_l_val = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(unsigned long), GFP_KERNEL);
		if (!cpuclk->ctrl.boost_safe_l_val)
			return -ENOMEM;
	}

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * len, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * len);
	for (i = 0; i < prop_len; i++) {
		c->fmax[i] = array[len * i];
		vdd->vdd_uv[i] = array[len * i + 1];
		if (vmin) {
			temp = array[len * i + 2];
			if (temp > 0 && (i < (prop_len - 1)))
				cpuclk->ctrl.boost_safe_l_val[i] = temp;
		}
	}

	if (pboost) {
		temp = array[(len * --i) + 2];
		if (temp > 0)
			cpuclk->ctrl.boost_safe_l_val[i] = temp;

		rc = of_property_read_u32_index(of, "qcom,pboost-delta",
				boost_index, &delta);
		if (!rc) {
			HZ_TO_LVAL(delta, c->fmax[i], apcs_hf_pll.src_rate);
			if (delta <= MAX_L_VAL)
				cpuclk->ctrl.pboost_l_val = delta;
		} else
			dev_err(&pdev->dev, "No Boost L Val defined\n");
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	vdd->use_max_uV = true;
	c->num_fmax = prop_len;

	return 0;
}

static void get_speed_bin(struct platform_device *pdev, int *bin,
					int *version, int *pboost_freq)
{
	struct resource *res;
	void __iomem *base;
	u32 pte_efuse;

	*bin = 0;
	*version = 0;
	*pboost_freq = 0;

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

	*pboost_freq = (pte_efuse >> 11) & 0x3;

	dev_info(&pdev->dev, "Speed bin: %d PVS Version: %d Boost: %d\n", *bin,
							*version, *pboost_freq);
}

static int cpu_parse_devicetree(struct platform_device *pdev)
{
	struct resource *res;
	int mux_id = 0;
	char rcg_name[] = "xxx-mux";
	struct clk *c;

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "c0-pll");
	if (!res) {
		dev_err(&pdev->dev, "missing c0-pll\n");
		return -EINVAL;
	}

	virt_bases[APCS_C0_PLL_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[APCS_C0_PLL_BASE]) {
		dev_err(&pdev->dev, "ioremap failed for c0-pll\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "c1-pll");
	if (!res) {
		dev_err(&pdev->dev, "missing c1-pll\n");
		return -EINVAL;
	}

	virt_bases[APCS_C1_PLL_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[APCS_C1_PLL_BASE]) {
		dev_err(&pdev->dev, "ioremap failed for c1-pll\n");
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

static void initialize_mode_control(enum vmin_boost_mode bmode, u8 uni_upl,
		u8 uni_thres, enum vmin_boost_mode cur_mode)
{
	int mux_id;
	struct cpu_clk_8953 *clk;

	for (mux_id = 0; mux_id < A53SS_MUX_CCI; mux_id++) {
		clk = to_cpu_clk_8953(&cpuclk[mux_id]->c);

		if (bmode == VMIN) {
			clk->ctrl.vmin_cc.split_upl = SPLIT_UPL;
			clk->ctrl.vmin_cc.split_thres = SPLIT_THRESHOLD;
			clk->ctrl.vmin_cc.unified_upl = uni_upl;
			clk->ctrl.vmin_cc.unified_thres = uni_thres;
			clk->ctrl.vmin_enable = true;
		}

		if (bmode == PBOOST) {
			clk->ctrl.pb_cc.split_upl = SPLIT_UPL;
			clk->ctrl.pb_cc.split_thres = SPLIT_THRESHOLD;
			clk->ctrl.pb_cc.unified_upl = uni_upl;
			clk->ctrl.pb_cc.unified_thres = uni_thres;
			clk->ctrl.perfb_enable = true;
		}

		clk->ctrl.hw_mode_ctrl = BOTH_FSM;
		clk->ctrl.pll_clk_src = PLL_CLK_SRC;
		clk->curr_mode = cur_mode;
	}
}

static void debugfs_init(struct platform_device *pdev)
{
	static struct dentry *debugfs_base;

	/* Debugfs for Perf Boost */
	debugfs_base = debugfs_create_dir("hardware_boost", NULL);
	if (debugfs_base) {
		if (!debugfs_create_file("curr_mode", S_IRUGO,
				debugfs_base, NULL, &boost_mode_fops))
			goto debugfs_fail;

		if (!debugfs_create_file("safe_L", S_IRUGO, debugfs_base,
					NULL, &boost_safe_l_fops))
			goto debugfs_fail;

		if (!debugfs_create_file("boost_L", S_IRUGO, debugfs_base,
					NULL, &boost_l_fops))
			goto debugfs_fail;

		if (!debugfs_create_file("max_cores", S_IRUGO, debugfs_base,
					NULL, &max_cores_fops))
			goto debugfs_fail;

		if (!debugfs_create_file("boost_en", S_IRUGO, debugfs_base,
					NULL, &boost_enable_fops))
			goto debugfs_fail;

		if (!debugfs_create_file("hw_status", S_IRUGO, debugfs_base,
					NULL, &hardware_status_fops))
			goto debugfs_fail;
	} else
		dev_err(&pdev->dev, "Failed to create debugfs entry\n");

	return;

debugfs_fail:
	dev_err(&pdev->dev, "Remove debugfs as failed debugfs entry\n");
	debugfs_remove_recursive(debugfs_base);
}

static int cpuclk_notifier(struct notifier_block *self, unsigned long cmd,
			void *aff_level)
{
	struct cpu_clk_8953 *cclk;
	int i;
	void __iomem *base;
	u32 mode_ctrl = 0;

	if (!((unsigned long)aff_level == CCI_AFFINITY_LEVEL))
		return NOTIFY_OK;

	pr_debug("Notification received for aff level %ld, cmd %lu\n",
			(unsigned long)aff_level, cmd);

	mode_ctrl = a53_pwr_clk.ctrl.hw_mode_ctrl;
	mode_ctrl &= BM(UNIFIED_CPR_MUX_SEL_MSB, UNIFIED_CPR_MUX_SEL_LSB);

	if (mode_ctrl != BOTH_FSM)
		return NOTIFY_OK;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		for (i = 0; i < APCS0_DBG_BASE; i++) {
			base = virt_bases[i];
			writel_relaxed(TIMER3_E3_DELAY_VAL,
					base + FREQ_BOOST_TIMER3_CNT_VAL);
		}
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		for (i = 0; i < APCS0_DBG_BASE; i++) {
			base = virt_bases[i];
			cclk = to_cpu_clk_8953(&cpuclk[i]->c);
			writel_relaxed(cclk->ctrl.timer3_cnt_val,
					base + FREQ_BOOST_TIMER3_CNT_VAL);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpuclk_notifier_block = {
	.notifier_call = cpuclk_notifier,
};

static int clock_cpu_probe(struct platform_device *pdev)
{
	int speed_bin, version, rc, cpu, mux_id, pboost_freq;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	unsigned long ccirate, pwrcl_boot_rate = 883200000;
	bool vmin_en = false, pboost_en = false;

	get_speed_bin(pdev, &speed_bin, &version, &pboost_freq);

	vmin_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,enable-vmin");

	if ((of_property_read_bool(pdev->dev.of_node, "qcom,enable-boost"))
		       && (pboost_freq > 0))
		pboost_en = true;

	rc = cpu_parse_devicetree(pdev);
	if (rc)
		return rc;

	snprintf(prop_name, ARRAY_SIZE(prop_name),
				"qcom,speed%d-bin-v%d-cl",
					speed_bin, version);
	for (mux_id = 0; mux_id < A53SS_MUX_CCI; mux_id++) {
		rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
				prop_name, vmin_en, pboost_en, pboost_freq);
		if (rc) {
			dev_err(&pdev->dev, "Loading safe voltage plan %s!\n",
							prop_name);
			snprintf(prop_name, ARRAY_SIZE(prop_name),
						"qcom,speed0-bin-v0-cl");
			rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
				prop_name, vmin_en, pboost_en, pboost_freq);
			if (rc) {
				dev_err(&pdev->dev, "safe voltage plan load failed for clusters\n");
				return rc;
			}
		}
	}

	if (vmin_en)
		initialize_mode_control(VMIN, 0x4, 0x1, VMIN_BOOST_NONE);

	if (pboost_en)
		initialize_mode_control(PBOOST, 0x8, 0x4, VMIN_BOOST_NONE);

	if (!vmin_en || !pboost_en) {
		a53_pwr_clk.curr_mode = VMIN_BOOST_NONE;
		a53_perf_clk.curr_mode = VMIN_BOOST_NONE;
	}

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d-cci", speed_bin, version);
	rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c, prop_name, false,
								false, 0);
	if (rc) {
		dev_err(&pdev->dev, "Loading safe voltage plan %s!\n",
							prop_name);
		snprintf(prop_name, ARRAY_SIZE(prop_name),
						"qcom,speed0-bin-v0-cci");
		rc = of_get_fmax_vdd_class(pdev, &cpuclk[mux_id]->c,
						prop_name, false, false, 0);
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

	/* CPR Core Adjust */
	virt_bases[APCS0_CPR_CORE_BASE] = devm_ioremap(&pdev->dev,
					APCS0_CPR_CORE_ADJ_MODE_REG, SZ_4);
	if (!virt_bases[APCS0_CPR_CORE_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap CPR_ADJ Mode register\n");
		return -ENOMEM;
	}

	/* CPR Core Adjust */
	virt_bases[APCS0_CPR_MARGIN_BASE] = devm_ioremap(&pdev->dev,
					APCS0_CPR_MARGIN_ADJ_CTL_REG, SZ_4);
	if (!virt_bases[APCS0_CPR_MARGIN_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap CPR_ADJ Ctl register\n");
		return -ENOMEM;
	}

	rc = of_msm_clock_register(pdev->dev.of_node,
			cpu_clocks_8953, ARRAY_SIZE(cpu_clocks_8953));
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
		WARN(clk_prepare_enable(&a53_pwr_clk.c),
				"Unable to turn on CPU clock for %d\n", cpu);
	}

	/* ccirate = HFPLL_rate/(2.5) */
	ccirate = CCI_RATE(apcs_hf_pll.c.rate);
	rc = clk_set_rate(&cci_clk.c, ccirate);
	if (rc)
		dev_err(&pdev->dev, "Can't set safe rate for CCI\n");

	rc = clk_set_rate(&a53_pwr_clk.c, apcs_hf_pll.c.rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set pwr safe rate\n");

	rc = clk_set_rate(&a53_perf_clk.c, apcs_hf_pll.c.rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set perf safe rate\n");

	/* Move to higher boot frequency */
	rc = clk_set_rate(&a53_pwr_clk.c, pwrcl_boot_rate);
	if (rc)
		dev_err(&pdev->dev, "Can't set pwr rate %ld\n",
					pwrcl_boot_rate);
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

	if (of_property_read_bool(pdev->dev.of_node, "qcom,enable-qos")) {
		a53_pwr_clk.hw_low_power_ctrl = true;
		a53_perf_clk.hw_low_power_ctrl = true;
	}

	debugfs_init(pdev);

	dev_info(&pdev->dev, "CPU clock driver initialized Vmin %s, HW Perf Boost %s\n",
			vmin_en ? "yes" : "no",
			pboost_en ? "yes" : "no");

	/* Register for cpu_cluster notification */
	cpu_pm_register_notifier(&cpuclk_notifier_block);

	return 0;
}

static struct of_device_id clock_cpu_match_table[] = {
	{.compatible = "qcom,cpu-clock-8953"},
	{}
};

static struct platform_driver clock_cpu_driver = {
	.probe = clock_cpu_probe,
	.driver = {
		.name = "cpu-clock-8953",
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

/* Configure PLL at Low frequency */
unsigned long pwrcl_early_boot_rate = 652800000;

static int __init cpu_clock_pwr_init(void)
{
	void __iomem  *base;
	int regval = 0;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
						"qcom,cpu-clock-8953");
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
