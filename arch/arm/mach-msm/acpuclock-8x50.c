/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/mfd/tps65023.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>

#include "acpuclock.h"
#include "avs.h"

#define SHOT_SWITCH 4
#define HOP_SWITCH 5
#define SIMPLE_SLEW 6
#define COMPLEX_SLEW 7

#define SPSS_CLK_CNTL_ADDR (MSM_CSR_BASE + 0x100)
#define SPSS_CLK_SEL_ADDR (MSM_CSR_BASE + 0x104)

/* Scorpion PLL registers */
#define SCPLL_CTL_ADDR         (MSM_SCPLL_BASE + 0x4)
#define SCPLL_STATUS_ADDR      (MSM_SCPLL_BASE + 0x18)
#define SCPLL_FSM_CTL_EXT_ADDR (MSM_SCPLL_BASE + 0x10)

#ifdef CONFIG_QSD_SVS
#define TPS65023_MAX_DCDC1	1600
#else
#define TPS65023_MAX_DCDC1	CONFIG_QSD_PMIC_DEFAULT_DCDC1
#endif

enum {
	ACPU_PLL_TCXO	= -1,
	ACPU_PLL_0	= 0,
	ACPU_PLL_1,
	ACPU_PLL_2,
	ACPU_PLL_3,
	ACPU_PLL_END,
};

struct clkctl_acpu_speed {
	unsigned int     use_for_scaling;
	unsigned int     acpuclk_khz;
	int              pll;
	unsigned int     acpuclk_src_sel;
	unsigned int     acpuclk_src_div;
	unsigned int     ahbclk_khz;
	unsigned int     ahbclk_div;
	unsigned int     axiclk_khz;
	unsigned int     sc_core_src_sel_mask;
	unsigned int     sc_l_value;
	int              vdd;
	unsigned long    lpj; /* loops_per_jiffy */
};

struct clkctl_acpu_speed acpu_freq_tbl_998[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 0, 0, 14000, 0, 0, 1000},
	{ 0, 128000, ACPU_PLL_1, 1, 5, 0, 0, 14000, 2, 0, 1000},
	{ 1, 245760, ACPU_PLL_0, 4, 0, 0, 0, 29000, 0, 0, 1000},
	/* Update AXI_S and PLL0_S macros if above row numbers change. */
	{ 1, 384000, ACPU_PLL_3, 0, 0, 0, 0, 58000, 1, 0xA, 1000},
	{ 0, 422400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xB, 1000},
	{ 0, 460800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xC, 1000},
	{ 0, 499200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xD, 1050},
	{ 0, 537600, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xE, 1050},
	{ 1, 576000, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xF, 1050},
	{ 0, 614400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x10, 1075},
	{ 0, 652800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x11, 1100},
	{ 0, 691200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x12, 1125},
	{ 0, 729600, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x13, 1150},
	{ 1, 768000, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x14, 1150},
	{ 0, 806400, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x15, 1175},
	{ 0, 844800, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x16, 1225},
	{ 0, 883200, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x17, 1250},
	{ 0, 921600, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x18, 1300},
	{ 0, 960000, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x19, 1300},
	{ 1, 998400, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x1A, 1300},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

struct clkctl_acpu_speed acpu_freq_tbl_768[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 0, 0, 14000, 0, 0, 1000},
	{ 0, 128000, ACPU_PLL_1, 1, 5, 0, 0, 14000, 2, 0, 1000},
	{ 1, 245760, ACPU_PLL_0, 4, 0, 0, 0, 29000, 0, 0, 1000},
	/* Update AXI_S and PLL0_S macros if above row numbers change. */
	{ 1, 384000, ACPU_PLL_3, 0, 0, 0, 0, 58000, 1, 0xA, 1075},
	{ 0, 422400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xB, 1100},
	{ 0, 460800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xC, 1125},
	{ 0, 499200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xD, 1150},
	{ 0, 537600, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xE, 1150},
	{ 1, 576000, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xF, 1150},
	{ 0, 614400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x10, 1175},
	{ 0, 652800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x11, 1200},
	{ 0, 691200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x12, 1225},
	{ 0, 729600, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x13, 1250},
	{ 1, 768000, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x14, 1250},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static struct clkctl_acpu_speed *acpu_freq_tbl = acpu_freq_tbl_998;
#define AXI_S	(&acpu_freq_tbl[1])
#define PLL0_S	(&acpu_freq_tbl[2])

/* Use 128MHz for PC since ACPU will auto-switch to AXI (128MHz) before
 * coming back up. This allows detection of return-from-PC, since 128MHz
 * is only used for power collapse. */
#define POWER_COLLAPSE_KHZ	128000
/* Use 245MHz (not 128MHz) for SWFI to avoid unnecessary steps between
 * 128MHz<->245MHz. Jumping to high frequencies from 128MHz directly
 * is not allowed. */
#define WAIT_FOR_IRQ_KHZ	245760

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[20];

static void __init cpufreq_table_init(void)
{
	unsigned int i;
	unsigned int freq_cnt = 0;

	/* Construct the freq_table table from acpu_freq_tbl since the
	 * freq_table values need to match frequencies specified in
	 * acpu_freq_tbl and acpu_freq_tbl needs to be fixed up during init.
	 */
	for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table)-1; i++) {
		if (acpu_freq_tbl[i].use_for_scaling) {
			freq_table[freq_cnt].index = freq_cnt;
			freq_table[freq_cnt].frequency
				= acpu_freq_tbl[i].acpuclk_khz;
			freq_cnt++;
		}
	}

	/* freq_table not big enough to store all usable freqs. */
	BUG_ON(acpu_freq_tbl[i].acpuclk_khz != 0);

	freq_table[freq_cnt].index = freq_cnt;
	freq_table[freq_cnt].frequency = CPUFREQ_TABLE_END;

	pr_info("%d scaling frequencies supported.\n", freq_cnt);
}
#endif

struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	struct clk			*ebi1_clk;
	int (*acpu_set_vdd) (int mvolts);
};

static struct clock_state drv_state = { 0 };

static void scpll_set_freq(uint32_t lval, unsigned freq_switch)
{
	uint32_t regval;

	if (lval > 33)
		lval = 33;
	if (lval < 10)
		lval = 10;

	/* wait for any calibrations or frequency switches to finish */
	while (readl(SCPLL_STATUS_ADDR) & 0x3)
		;

	/* write the new L val and switch mode */
	regval = readl(SCPLL_FSM_CTL_EXT_ADDR);
	regval &= ~(0x3f << 3);
	regval |= (lval << 3);
	if (freq_switch == SIMPLE_SLEW)
		regval |= (0x1 << 9);

	regval &= ~(0x3 << 0);
	regval |= (freq_switch << 0);
	writel(regval, SCPLL_FSM_CTL_EXT_ADDR);

	dmb();

	/* put in normal mode */
	regval = readl(SCPLL_CTL_ADDR);
	regval |= 0x7;
	writel(regval, SCPLL_CTL_ADDR);

	dmb();

	/* wait for frequency switch to finish */
	while (readl(SCPLL_STATUS_ADDR) & 0x1)
		;

	/* status bit seems to clear early, using
	 * 100us to handle the worst case. */
	udelay(100);
}

static void scpll_apps_enable(bool state)
{
	uint32_t regval;

	if (state)
		pr_debug("Enabling PLL 3\n");
	else
		pr_debug("Disabling PLL 3\n");

	/* Wait for any frequency switches to finish. */
	while (readl(SCPLL_STATUS_ADDR) & 0x1)
		;

	/* put the pll in standby mode */
	regval = readl(SCPLL_CTL_ADDR);
	regval &= ~(0x7);
	regval |= (0x2);
	writel(regval, SCPLL_CTL_ADDR);

	dmb();

	if (state) {
		/* put the pll in normal mode */
		regval = readl(SCPLL_CTL_ADDR);
		regval |= (0x7);
		writel(regval, SCPLL_CTL_ADDR);
		udelay(200);
	} else {
		/* put the pll in power down mode */
		regval = readl(SCPLL_CTL_ADDR);
		regval &= ~(0x7);
		writel(regval, SCPLL_CTL_ADDR);
	}
	udelay(62);

	if (state)
		pr_debug("PLL 3 Enabled\n");
	else
		pr_debug("PLL 3 Disabled\n");
}

static void scpll_init(void)
{
	uint32_t regval;
#define L_VAL_384MHZ	0xA
#define L_VAL_768MHZ	0x14

	pr_debug("Initializing PLL 3\n");

	/* power down scpll */
	writel(0x0, SCPLL_CTL_ADDR);

	dmb();

	/* set bypassnl, put into standby */
	writel(0x00400002, SCPLL_CTL_ADDR);

	/* set bypassnl, reset_n, full calibration */
	writel(0x00600004, SCPLL_CTL_ADDR);

	/* Ensure register write to initiate calibration has taken
	effect before reading status flag */
	dmb();

	/* wait for cal_all_done */
	while (readl(SCPLL_STATUS_ADDR) & 0x2)
		;

	/* Start: Set of experimentally derived steps
	 * to work around a h/w bug. */

	/* Put the pll in normal mode */
	scpll_apps_enable(1);

	/* SHOT switch to 384 MHz */
	regval = readl(SCPLL_FSM_CTL_EXT_ADDR);
	regval &= ~(0x3f << 3);
	regval |= (L_VAL_384MHZ << 3);

	regval &= ~0x7;
	regval |= SHOT_SWITCH;
	writel(regval, SCPLL_FSM_CTL_EXT_ADDR);

	/* Trigger the freq switch by putting pll in normal mode. */
	regval = readl(SCPLL_CTL_ADDR);
	regval |= (0x7);
	writel(regval, SCPLL_CTL_ADDR);

	/* Wait for frequency switch to finish */
	while (readl(SCPLL_STATUS_ADDR) & 0x1)
		;

	/* Status bit seems to clear early, using
	 * 800 microseconds for the worst case. */
	udelay(800);

	/* HOP switch to 768 MHz. */
	regval = readl(SCPLL_FSM_CTL_EXT_ADDR);
	regval &= ~(0x3f << 3);
	regval |= (L_VAL_768MHZ << 3);

	regval &= ~0x7;
	regval |= HOP_SWITCH;
	writel(regval, SCPLL_FSM_CTL_EXT_ADDR);

	/* Trigger the freq switch by putting pll in normal mode. */
	regval = readl(SCPLL_CTL_ADDR);
	regval |= (0x7);
	writel(regval, SCPLL_CTL_ADDR);

	/* Wait for frequency switch to finish */
	while (readl(SCPLL_STATUS_ADDR) & 0x1)
		;

	/* Status bit seems to clear early, using
	 * 100 microseconds for the worst case. */
	udelay(100);

	/* End: Work around for h/w bug */

	/* Power down scpll */
	scpll_apps_enable(0);
}

static void config_pll(struct clkctl_acpu_speed *s)
{
	uint32_t regval;

	if (s->pll == ACPU_PLL_3)
		scpll_set_freq(s->sc_l_value, HOP_SWITCH);
	/* Configure the PLL divider mux if we plan to use it. */
	else if (s->sc_core_src_sel_mask == 0) {
		/* get the current clock source selection */
		regval = readl(SPSS_CLK_SEL_ADDR) & 0x1;

		/* configure the other clock source, then switch to it,
		 * using the glitch free mux */
		switch (regval) {
		case 0x0:
			regval = readl(SPSS_CLK_CNTL_ADDR);
			regval &= ~(0x7 << 4 | 0xf);
			regval |= (s->acpuclk_src_sel << 4);
			regval |= (s->acpuclk_src_div << 0);
			writel(regval, SPSS_CLK_CNTL_ADDR);

			regval = readl(SPSS_CLK_SEL_ADDR);
			regval |= 0x1;
			writel(regval, SPSS_CLK_SEL_ADDR);
			break;

		case 0x1:
			regval = readl(SPSS_CLK_CNTL_ADDR);
			regval &= ~(0x7 << 12 | 0xf << 8);
			regval |= (s->acpuclk_src_sel << 12);
			regval |= (s->acpuclk_src_div << 8);
			writel(regval, SPSS_CLK_CNTL_ADDR);

			regval = readl(SPSS_CLK_SEL_ADDR);
			regval &= ~0x1;
			writel(regval, SPSS_CLK_SEL_ADDR);
			break;
		}
		dmb();
	}

	regval = readl(SPSS_CLK_SEL_ADDR);
	regval &= ~(0x3 << 1);
	regval |= (s->sc_core_src_sel_mask << 1);
	writel(regval, SPSS_CLK_SEL_ADDR);
}

static int acpuclk_set_vdd_level(int vdd)
{
	if (drv_state.acpu_set_vdd) {
		pr_debug("Switching VDD to %d mV\n", vdd);
		return drv_state.acpu_set_vdd(vdd);
	} else {
		/* Assume that the PMIC supports scaling the processor
		 * to its maximum frequency at its default voltage.
		 */
		return 0;
	}
}

static int acpuclk_8x50_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int res, rc = 0;
	int freq_index = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed;

	if (rate == strt_s->acpuclk_khz)
		goto out;

	for (tgt_s = acpu_freq_tbl; tgt_s->acpuclk_khz != 0; tgt_s++) {
		if (tgt_s->acpuclk_khz == rate)
			break;
		freq_index++;
	}

	if (tgt_s->acpuclk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ) {
#ifdef CONFIG_MSM_CPU_AVS
		/* Notify avs before changing frequency */
		rc = avs_adjust_freq(freq_index, 1);
		if (rc) {
			pr_err("Unable to increase ACPU vdd (%d)\n", rc);
			goto out;
		}
#endif
		/* Increase VDD if needed. */
		if (tgt_s->vdd > strt_s->vdd) {
			rc = acpuclk_set_vdd_level(tgt_s->vdd);
			if (rc) {
				pr_err("Unable to increase ACPU vdd (%d)\n",
					rc);
				goto out;
			}
		}
	} else if (reason == SETRATE_PC
		&& rate != POWER_COLLAPSE_KHZ) {
		/* Returning from PC. ACPU is running on AXI source.
		 * Step up to PLL0 before ramping up higher. */
		config_pll(PLL0_S);
	}

	pr_debug("Switching from ACPU rate %u KHz -> %u KHz\n",
		strt_s->acpuclk_khz, tgt_s->acpuclk_khz);

	if (strt_s->pll != ACPU_PLL_3 && tgt_s->pll != ACPU_PLL_3) {
		config_pll(tgt_s);
	} else if (strt_s->pll != ACPU_PLL_3 && tgt_s->pll == ACPU_PLL_3) {
		scpll_apps_enable(1);
		config_pll(tgt_s);
	} else if (strt_s->pll == ACPU_PLL_3 && tgt_s->pll != ACPU_PLL_3) {
		config_pll(tgt_s);
		scpll_apps_enable(0);
	} else {
		/* Temporarily switch to PLL0 while reconfiguring PLL3. */
		config_pll(PLL0_S);
		config_pll(tgt_s);
	}

	/* Update the driver state with the new clock freq */
	drv_state.current_speed = tgt_s;

	/* Re-adjust lpj for the new clock speed. */
	loops_per_jiffy = tgt_s->lpj;

	/* Nothing else to do for SWFI. */
	if (reason == SETRATE_SWFI)
		goto out;

	if (strt_s->axiclk_khz != tgt_s->axiclk_khz) {
		res = clk_set_rate(drv_state.ebi1_clk,
				tgt_s->axiclk_khz * 1000);
		if (res < 0)
			pr_warning("Setting AXI min rate failed (%d)\n", res);
	}

	/* Nothing else to do for power collapse */
	if (reason == SETRATE_PC)
		goto out;

#ifdef CONFIG_MSM_CPU_AVS
	/* notify avs after changing frequency */
	res = avs_adjust_freq(freq_index, 0);
	if (res)
		pr_warning("Unable to drop ACPU vdd (%d)\n", res);
#endif

	/* Drop VDD level if we can. */
	if (tgt_s->vdd < strt_s->vdd) {
		res = acpuclk_set_vdd_level(tgt_s->vdd);
		if (res)
			pr_warning("Unable to drop ACPU vdd (%d)\n", res);
	}

	pr_debug("ACPU speed change complete\n");
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __init acpuclk_hw_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel, regval;
	int res;

	/* Determine the source of the Scorpion clock. */
	regval = readl(SPSS_CLK_SEL_ADDR);
	switch ((regval & 0x6) >> 1) {
	case 0: /* raw source clock */
	case 3: /* low jitter PLL1 (768Mhz) */
		if (regval & 0x1) {
			sel = ((readl(SPSS_CLK_CNTL_ADDR) >> 4) & 0x7);
			div = ((readl(SPSS_CLK_CNTL_ADDR) >> 0) & 0xf);
		} else {
			sel = ((readl(SPSS_CLK_CNTL_ADDR) >> 12) & 0x7);
			div = ((readl(SPSS_CLK_CNTL_ADDR) >> 8) & 0xf);
		}

		/* Find the matching clock rate. */
		for (speed = acpu_freq_tbl; speed->acpuclk_khz != 0; speed++) {
			if (speed->acpuclk_src_sel == sel &&
			    speed->acpuclk_src_div == div)
				break;
		}
		break;

	case 1: /* unbuffered scorpion pll (384Mhz to 998.4Mhz) */
		sel = ((readl(SCPLL_FSM_CTL_EXT_ADDR) >> 3) & 0x3f);

		/* Find the matching clock rate. */
		for (speed = acpu_freq_tbl; speed->acpuclk_khz != 0; speed++) {
			if (speed->sc_l_value == sel &&
			    speed->sc_core_src_sel_mask == 1)
				break;
		}
		break;

	case 2: /* AXI bus clock (128Mhz) */
		speed = AXI_S;
		break;
	default:
		BUG();
	}

	/* Initialize scpll only if it wasn't already initialized by the boot
	 * loader. If the CPU is already running on scpll, then the scpll was
	 * initialized by the boot loader. */
	if (speed->pll != ACPU_PLL_3)
		scpll_init();

	if (speed->acpuclk_khz == 0) {
		pr_err("Error - ACPU clock reports invalid speed\n");
		return;
	}

	drv_state.current_speed = speed;
	res = clk_set_rate(drv_state.ebi1_clk, speed->axiclk_khz * 1000);
	if (res < 0)
		pr_warning("Setting AXI min rate failed (%d)\n", res);
	res = clk_enable(drv_state.ebi1_clk);
	if (res < 0)
		pr_warning("Enabling AXI clock failed (%d)\n", res);

	pr_info("ACPU running at %d KHz\n", speed->acpuclk_khz);
}

static unsigned long acpuclk_8x50_get_rate(int cpu)
{
	return drv_state.current_speed->acpuclk_khz;
}

/* Spare register populated with efuse data on max ACPU freq. */
#define CT_CSR_PHYS		0xA8700000
#define TCSR_SPARE2_ADDR	(ct_csr_base + 0x60)

#define PLL0_M_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x308)

static void __init acpu_freq_tbl_fixup(void)
{
	void __iomem *ct_csr_base;
	uint32_t tcsr_spare2, pll0_m_val;
	unsigned int max_acpu_khz;
	unsigned int i;

	ct_csr_base = ioremap(CT_CSR_PHYS, PAGE_SIZE);
	BUG_ON(ct_csr_base == NULL);

	tcsr_spare2 = readl(TCSR_SPARE2_ADDR);

	/* Check if the register is supported and meaningful. */
	if ((tcsr_spare2 & 0xF000) != 0xA000) {
		pr_info("Efuse data on Max ACPU freq not present.\n");
		goto skip_efuse_fixup;
	}

	switch (tcsr_spare2 & 0xF0) {
	case 0x70:
		acpu_freq_tbl = acpu_freq_tbl_768;
		max_acpu_khz = 768000;
		break;
	case 0x30:
	case 0x00:
		max_acpu_khz = 998400;
		break;
	case 0x10:
		max_acpu_khz = 1267200;
		break;
	default:
		pr_warning("Invalid efuse data (%x) on Max ACPU freq!\n",
				tcsr_spare2);
		goto skip_efuse_fixup;
	}

	pr_info("Max ACPU freq from efuse data is %d KHz\n", max_acpu_khz);

	for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0; i++) {
		if (acpu_freq_tbl[i].acpuclk_khz > max_acpu_khz) {
			acpu_freq_tbl[i].acpuclk_khz = 0;
			break;
		}
	}

skip_efuse_fixup:
	iounmap(ct_csr_base);

	/* pll0_m_val will be 36 when PLL0 is run at 235MHz
	 * instead of the usual 245MHz. */
	pll0_m_val = readl(PLL0_M_VAL_ADDR) & 0x7FFFF;
	if (pll0_m_val == 36)
		PLL0_S->acpuclk_khz = 235930;

	for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0; i++) {
		if (acpu_freq_tbl[i].vdd > TPS65023_MAX_DCDC1) {
			acpu_freq_tbl[i].acpuclk_khz = 0;
			break;
		}
	}
}

/* Initalize the lpj field in the acpu_freq_tbl. */
static void __init lpj_init(void)
{
	int i;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;
	for (i = 0; acpu_freq_tbl[i].acpuclk_khz; i++) {
		acpu_freq_tbl[i].lpj = cpufreq_scale(loops_per_jiffy,
						base_clk->acpuclk_khz,
						acpu_freq_tbl[i].acpuclk_khz);
	}
}

#ifdef CONFIG_MSM_CPU_AVS
static int __init acpu_avs_init(int (*set_vdd) (int), int khz)
{
	int i;
	int freq_count = 0;
	int freq_index = -1;

	for (i = 0; acpu_freq_tbl[i].acpuclk_khz; i++) {
		freq_count++;
		if (acpu_freq_tbl[i].acpuclk_khz == khz)
			freq_index = i;
	}

	return avs_init(set_vdd, freq_count, freq_index);
}
#endif

static int qsd8x50_tps65023_set_dcdc1(int mVolts)
{
	int rc = 0;
#ifdef CONFIG_QSD_SVS
	rc = tps65023_set_dcdc1_level(mVolts);
	/*
	 * By default the TPS65023 will be initialized to 1.225V.
	 * So we can safely switch to any frequency within this
	 * voltage even if the device is not probed/ready.
	 */
	if (rc == -ENODEV && mVolts <= CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = 0;
#else
	/*
	 * Disallow frequencies not supported in the default PMIC
	 * output voltage.
	 */
	if (mVolts > CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = -EFAULT;
#endif
	return rc;
}

static struct acpuclk_data acpuclk_8x50_data = {
	.set_rate = acpuclk_8x50_set_rate,
	.get_rate = acpuclk_8x50_get_rate,
	.power_collapse_khz = POWER_COLLAPSE_KHZ,
	.wait_for_irq_khz = WAIT_FOR_IRQ_KHZ,
	.switch_time_us = 20,
};

static int __init acpuclk_8x50_init(struct acpuclk_soc_data *soc_data)
{
	mutex_init(&drv_state.lock);
	drv_state.acpu_set_vdd = qsd8x50_tps65023_set_dcdc1;

	drv_state.ebi1_clk = clk_get(NULL, "ebi1_acpu_clk");
	BUG_ON(IS_ERR(drv_state.ebi1_clk));

	acpu_freq_tbl_fixup();
	acpuclk_hw_init();
	lpj_init();
	/* Set a lower bound for ACPU rate for boot. This limits the
	 * maximum frequency hop caused by the first CPUFREQ switch. */
	if (drv_state.current_speed->acpuclk_khz < PLL0_S->acpuclk_khz)
		acpuclk_set_rate(0, PLL0_S->acpuclk_khz, SETRATE_CPUFREQ);

	acpuclk_register(&acpuclk_8x50_data);

#ifdef CONFIG_CPU_FREQ_MSM
	cpufreq_table_init();
	cpufreq_frequency_table_get_attr(freq_table, smp_processor_id());
#endif
#ifdef CONFIG_MSM_CPU_AVS
	if (!acpu_avs_init(drv_state.acpu_set_vdd,
		drv_state.current_speed->acpuclk_khz)) {
		/* avs init successful. avs will handle voltage changes */
		drv_state.acpu_set_vdd = NULL;
	}
#endif
	return 0;
}

struct acpuclk_soc_data acpuclk_8x50_soc_data __initdata = {
	.init = acpuclk_8x50_init,
};
