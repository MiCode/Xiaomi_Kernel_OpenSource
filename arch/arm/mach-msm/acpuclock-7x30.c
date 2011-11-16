/*
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>

#include "smd_private.h"
#include "clock.h"
#include "acpuclock.h"
#include "spm.h"

#define SCSS_CLK_CTL_ADDR	(MSM_ACC_BASE + 0x04)
#define SCSS_CLK_SEL_ADDR	(MSM_ACC_BASE + 0x08)

#define PLL2_L_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x33C)
#define PLL2_M_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x340)
#define PLL2_N_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x344)
#define PLL2_CONFIG_ADDR	(MSM_CLK_CTL_BASE + 0x34C)

#define VREF_SEL     1	/* 0: 0.625V (50mV step), 1: 0.3125V (25mV step). */
#define V_STEP       (25 * (2 - VREF_SEL)) /* Minimum voltage step size. */
#define VREG_DATA    (VREG_CONFIG | (VREF_SEL << 5))
#define VREG_CONFIG  (BIT(7) | BIT(6)) /* Enable VREG, pull-down if disabled. */
/* Cause a compile error if the voltage is not a multiple of the step size. */
#define MV(mv)      ((mv) / (!((mv) % V_STEP)))
/* mv = (750mV + (raw * 25mV)) * (2 - VREF_SEL) */
#define VDD_RAW(mv) (((MV(mv) / V_STEP) - 30) | VREG_DATA)

#define MAX_AXI_KHZ 192000

struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	struct clk			*ebi1_clk;
};

struct pll {
	unsigned int l;
	unsigned int m;
	unsigned int n;
	unsigned int pre_div;
};

struct clkctl_acpu_speed {
	unsigned int	use_for_scaling;
	unsigned int	acpu_clk_khz;
	int		src;
	unsigned int	acpu_src_sel;
	unsigned int	acpu_src_div;
	unsigned int	axi_clk_hz;
	unsigned int	vdd_mv;
	unsigned int	vdd_raw;
	struct pll	*pll_rate;
	unsigned long	lpj; /* loops_per_jiffy */
};

static struct clock_state drv_state = { 0 };

/* Switch to this when reprogramming PLL2 */
static struct clkctl_acpu_speed *backup_s;

static struct pll pll2_tbl[] = {
	{  42, 0, 1, 0 }, /*  806 MHz */
	{  53, 1, 3, 0 }, /* 1024 MHz */
	{ 125, 0, 1, 1 }, /* 1200 MHz */
	{  73, 0, 1, 0 }, /* 1401 MHz */
};

/* Use negative numbers for sources that can't be enabled/disabled */

enum acpuclk_source {
	LPXO	= -2,
	AXI	= -1,
	PLL_0	=  0,
	PLL_1,
	PLL_2,
	PLL_3,
	MAX_SOURCE
};

static struct clk *acpuclk_sources[MAX_SOURCE];

/*
 * Each ACPU frequency has a certain minimum MSMC1 voltage requirement
 * that is implicitly met by voting for a specific minimum AXI frequency.
 * Do NOT change the AXI frequency unless you are _absoulutely_ sure you
 * know all the h/w requirements.
 */
static struct clkctl_acpu_speed acpu_freq_tbl[] = {
	{ 0, 24576,  LPXO, 0, 0,  30720000,  900, VDD_RAW(900) },
	{ 0, 61440,  PLL_3,    5, 11, 61440000,  900, VDD_RAW(900) },
	{ 1, 122880, PLL_3,    5, 5,  61440000,  900, VDD_RAW(900) },
	{ 0, 184320, PLL_3,    5, 4,  61440000,  900, VDD_RAW(900) },
	{ 0, MAX_AXI_KHZ, AXI, 1, 0, 61440000, 900, VDD_RAW(900) },
	{ 1, 245760, PLL_3,    5, 2,  61440000,  900, VDD_RAW(900) },
	{ 1, 368640, PLL_3,    5, 1,  122800000, 900, VDD_RAW(900) },
	/* AXI has MSMC1 implications. See above. */
	{ 1, 768000, PLL_1,    2, 0,  153600000, 1050, VDD_RAW(1050) },
	/*
	 * AXI has MSMC1 implications. See above.
	 */
	{ 1, 806400,  PLL_2, 3, 0, UINT_MAX, 1100, VDD_RAW(1100), &pll2_tbl[0]},
	{ 1, 1024000, PLL_2, 3, 0, UINT_MAX, 1200, VDD_RAW(1200), &pll2_tbl[1]},
	{ 1, 1200000, PLL_2, 3, 0, UINT_MAX, 1200, VDD_RAW(1200), &pll2_tbl[2]},
	{ 1, 1401600, PLL_2, 3, 0, UINT_MAX, 1250, VDD_RAW(1250), &pll2_tbl[3]},
	{ 0 }
};

static int acpuclk_set_acpu_vdd(struct clkctl_acpu_speed *s)
{
	int ret = msm_spm_set_vdd(0, s->vdd_raw);
	if (ret)
		return ret;

	/* Wait for voltage to stabilize. */
	udelay(62);
	return 0;
}

/* Assumes PLL2 is off and the acpuclock isn't sourced from PLL2 */
static void acpuclk_config_pll2(struct pll *pll)
{
	uint32_t config = readl_relaxed(PLL2_CONFIG_ADDR);

	/* Make sure write to disable PLL_2 has completed
	 * before reconfiguring that PLL. */
	mb();
	writel_relaxed(pll->l, PLL2_L_VAL_ADDR);
	writel_relaxed(pll->m, PLL2_M_VAL_ADDR);
	writel_relaxed(pll->n, PLL2_N_VAL_ADDR);
	if (pll->pre_div)
		config |= BIT(15);
	else
		config &= ~BIT(15);
	writel_relaxed(config, PLL2_CONFIG_ADDR);
	/* Make sure PLL is programmed before returning. */
	mb();
}

/* Set clock source and divider given a clock speed */
static void acpuclk_set_src(const struct clkctl_acpu_speed *s)
{
	uint32_t reg_clksel, reg_clkctl, src_sel;

	reg_clksel = readl_relaxed(SCSS_CLK_SEL_ADDR);

	/* CLK_SEL_SRC1NO */
	src_sel = reg_clksel & 1;

	/* Program clock source and divider. */
	reg_clkctl = readl_relaxed(SCSS_CLK_CTL_ADDR);
	reg_clkctl &= ~(0xFF << (8 * src_sel));
	reg_clkctl |= s->acpu_src_sel << (4 + 8 * src_sel);
	reg_clkctl |= s->acpu_src_div << (0 + 8 * src_sel);
	writel_relaxed(reg_clkctl, SCSS_CLK_CTL_ADDR);

	/* Toggle clock source. */
	reg_clksel ^= 1;

	/* Program clock source selection. */
	writel_relaxed(reg_clksel, SCSS_CLK_SEL_ADDR);

	/* Make sure switch to new source is complete. */
	mb();
}

static int acpuclk_7x30_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int res, rc = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed;

	if (rate == strt_s->acpu_clk_khz)
		goto out;

	for (tgt_s = acpu_freq_tbl; tgt_s->acpu_clk_khz != 0; tgt_s++) {
		if (tgt_s->acpu_clk_khz == rate)
			break;
	}
	if (tgt_s->acpu_clk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ) {
		/* Increase VDD if needed. */
		if (tgt_s->vdd_mv > strt_s->vdd_mv) {
			rc = acpuclk_set_acpu_vdd(tgt_s);
			if (rc < 0) {
				pr_err("ACPU VDD increase to %d mV failed "
					"(%d)\n", tgt_s->vdd_mv, rc);
				goto out;
			}
		}
	}

	pr_debug("Switching from ACPU rate %u KHz -> %u KHz\n",
	       strt_s->acpu_clk_khz, tgt_s->acpu_clk_khz);

	/* Increase the AXI bus frequency if needed. This must be done before
	 * increasing the ACPU frequency, since voting for high AXI rates
	 * implicitly takes care of increasing the MSMC1 voltage, as needed. */
	if (tgt_s->axi_clk_hz > strt_s->axi_clk_hz) {
		rc = clk_set_rate(drv_state.ebi1_clk, tgt_s->axi_clk_hz);
		if (rc < 0) {
			pr_err("Setting AXI min rate failed (%d)\n", rc);
			goto out;
		}
	}

	/* Move off of PLL2 if we're reprogramming it */
	if (tgt_s->src == PLL_2 && strt_s->src == PLL_2) {
		clk_enable(acpuclk_sources[backup_s->src]);
		acpuclk_set_src(backup_s);
		clk_disable(acpuclk_sources[strt_s->src]);
	}

	/* Reconfigure PLL2 if we're moving to it */
	if (tgt_s->src == PLL_2)
		acpuclk_config_pll2(tgt_s->pll_rate);

	/* Make sure target PLL is on. */
	if ((strt_s->src != tgt_s->src && tgt_s->src >= 0) ||
	    (tgt_s->src == PLL_2 && strt_s->src == PLL_2)) {
		pr_debug("Enabling PLL %d\n", tgt_s->src);
		clk_enable(acpuclk_sources[tgt_s->src]);
	}

	/* Perform the frequency switch */
	acpuclk_set_src(tgt_s);
	drv_state.current_speed = tgt_s;
	loops_per_jiffy = tgt_s->lpj;

	if (tgt_s->src == PLL_2 && strt_s->src == PLL_2)
		clk_disable(acpuclk_sources[backup_s->src]);

	/* Nothing else to do for SWFI. */
	if (reason == SETRATE_SWFI)
		goto out;

	/* Turn off previous PLL if not used. */
	if (strt_s->src != tgt_s->src && strt_s->src >= 0) {
		pr_debug("Disabling PLL %d\n", strt_s->src);
		clk_disable(acpuclk_sources[strt_s->src]);
	}

	/* Decrease the AXI bus frequency if we can. */
	if (tgt_s->axi_clk_hz < strt_s->axi_clk_hz) {
		res = clk_set_rate(drv_state.ebi1_clk, tgt_s->axi_clk_hz);
		if (res < 0)
			pr_warning("Setting AXI min rate failed (%d)\n", res);
	}

	/* Nothing else to do for power collapse. */
	if (reason == SETRATE_PC)
		goto out;

	/* Drop VDD level if we can. */
	if (tgt_s->vdd_mv < strt_s->vdd_mv) {
		res = acpuclk_set_acpu_vdd(tgt_s);
		if (res)
			pr_warning("ACPU VDD decrease to %d mV failed (%d)\n",
					tgt_s->vdd_mv, res);
	}

	pr_debug("ACPU speed change complete\n");
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);

	return rc;
}

static unsigned long acpuclk_7x30_get_rate(int cpu)
{
	WARN_ONCE(drv_state.current_speed == NULL,
		  "acpuclk_get_rate: not initialized\n");
	if (drv_state.current_speed)
		return drv_state.current_speed->acpu_clk_khz;
	else
		return 0;
}

/*----------------------------------------------------------------------------
 * Clock driver initialization
 *---------------------------------------------------------------------------*/

static void __init acpuclk_hw_init(void)
{
	struct clkctl_acpu_speed *s;
	uint32_t div, sel, src_num;
	uint32_t reg_clksel, reg_clkctl;
	int res;
	u8 pll2_l = readl_relaxed(PLL2_L_VAL_ADDR) & 0xFF;

	drv_state.ebi1_clk = clk_get(NULL, "ebi1_clk");
	BUG_ON(IS_ERR(drv_state.ebi1_clk));

	reg_clksel = readl_relaxed(SCSS_CLK_SEL_ADDR);

	/* Determine the ACPU clock rate. */
	switch ((reg_clksel >> 1) & 0x3) {
	case 0:	/* Running off the output of the raw clock source mux. */
		reg_clkctl = readl_relaxed(SCSS_CLK_CTL_ADDR);
		src_num = reg_clksel & 0x1;
		sel = (reg_clkctl >> (12 - (8 * src_num))) & 0x7;
		div = (reg_clkctl >> (8 -  (8 * src_num))) & 0xF;

		/* Check frequency table for matching sel/div pair. */
		for (s = acpu_freq_tbl; s->acpu_clk_khz != 0; s++) {
			if (s->acpu_src_sel == sel && s->acpu_src_div == div)
				break;
		}
		if (s->acpu_clk_khz == 0) {
			pr_err("Error - ACPU clock reports invalid speed\n");
			return;
		}
		break;
	case 2:	/* Running off of the SCPLL selected through the core mux. */
		/* Switch to run off of the SCPLL selected through the raw
		 * clock source mux. */
		for (s = acpu_freq_tbl; s->acpu_clk_khz != 0
			&& s->src != PLL_2 && s->acpu_src_div == 0; s++)
			;
		if (s->acpu_clk_khz != 0) {
			/* Program raw clock source mux. */
			acpuclk_set_src(s);

			/* Switch to raw clock source input of the core mux. */
			reg_clksel = readl_relaxed(SCSS_CLK_SEL_ADDR);
			reg_clksel &= ~(0x3 << 1);
			writel_relaxed(reg_clksel, SCSS_CLK_SEL_ADDR);
			break;
		}
		/* else fall through */
	default:
		pr_err("Error - ACPU clock reports invalid source\n");
		return;
	}

	/* Look at PLL2's L val to determine what speed PLL2 is running at */
	if (s->src == PLL_2)
		for ( ; s->acpu_clk_khz; s++)
			if (s->pll_rate && s->pll_rate->l == pll2_l)
				break;

	/* Set initial ACPU VDD. */
	acpuclk_set_acpu_vdd(s);

	drv_state.current_speed = s;

	/* Initialize current PLL's reference count. */
	if (s->src >= 0)
		clk_enable(acpuclk_sources[s->src]);

	res = clk_set_rate(drv_state.ebi1_clk, s->axi_clk_hz);
	if (res < 0)
		pr_warning("Setting AXI min rate failed!\n");

	pr_info("ACPU running at %d KHz\n", s->acpu_clk_khz);

	return;
}

/* Initalize the lpj field in the acpu_freq_tbl. */
static void __init lpj_init(void)
{
	int i;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;

	for (i = 0; acpu_freq_tbl[i].acpu_clk_khz; i++) {
		acpu_freq_tbl[i].lpj = cpufreq_scale(loops_per_jiffy,
						base_clk->acpu_clk_khz,
						acpu_freq_tbl[i].acpu_clk_khz);
	}
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table cpufreq_tbl[ARRAY_SIZE(acpu_freq_tbl)];

static void setup_cpufreq_table(void)
{
	unsigned i = 0;
	const struct clkctl_acpu_speed *speed;

	for (speed = acpu_freq_tbl; speed->acpu_clk_khz; speed++)
		if (speed->use_for_scaling) {
			cpufreq_tbl[i].index = i;
			cpufreq_tbl[i].frequency = speed->acpu_clk_khz;
			i++;
		}
	cpufreq_tbl[i].frequency = CPUFREQ_TABLE_END;

	cpufreq_frequency_table_get_attr(cpufreq_tbl, smp_processor_id());
}
#else
static inline void setup_cpufreq_table(void) { }
#endif

/*
 * Truncate the frequency table at the current PLL2 rate and determine the
 * backup PLL to use when scaling PLL2.
 */
void __init pll2_fixup(void)
{
	struct clkctl_acpu_speed *speed = acpu_freq_tbl;
	u8 pll2_l = readl_relaxed(PLL2_L_VAL_ADDR) & 0xFF;

	for ( ; speed->acpu_clk_khz; speed++) {
		if (speed->src != PLL_2)
			backup_s = speed;
		if (speed->pll_rate && speed->pll_rate->l == pll2_l) {
			speed++;
			speed->acpu_clk_khz = 0;
			return;
		}
	}

	pr_err("Unknown PLL2 lval %d\n", pll2_l);
	BUG();
}

#define RPM_BYPASS_MASK	(1 << 3)
#define PMIC_MODE_MASK	(1 << 4)

static void __init populate_plls(void)
{
	acpuclk_sources[PLL_1] = clk_get_sys("acpu", "pll1_clk");
	BUG_ON(IS_ERR(acpuclk_sources[PLL_1]));
	acpuclk_sources[PLL_2] = clk_get_sys("acpu", "pll2_clk");
	BUG_ON(IS_ERR(acpuclk_sources[PLL_2]));
	acpuclk_sources[PLL_3] = clk_get_sys("acpu", "pll3_clk");
	BUG_ON(IS_ERR(acpuclk_sources[PLL_3]));
}

static struct acpuclk_data acpuclk_7x30_data = {
	.set_rate = acpuclk_7x30_set_rate,
	.get_rate = acpuclk_7x30_get_rate,
	.power_collapse_khz = MAX_AXI_KHZ,
	.wait_for_irq_khz = MAX_AXI_KHZ,
	.switch_time_us = 50,
};

static int __init acpuclk_7x30_init(struct acpuclk_soc_data *soc_data)
{
	pr_info("%s()\n", __func__);

	mutex_init(&drv_state.lock);
	pll2_fixup();
	populate_plls();
	acpuclk_hw_init();
	lpj_init();
	setup_cpufreq_table();
	acpuclk_register(&acpuclk_7x30_data);

	return 0;
}

struct acpuclk_soc_data acpuclk_7x30_soc_data __initdata = {
	.init = acpuclk_7x30_init,
};
