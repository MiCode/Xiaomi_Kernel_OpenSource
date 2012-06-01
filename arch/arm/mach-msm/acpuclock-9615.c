/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>

#include <asm/cpu.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/rpm-regulator.h>

#include "acpuclock.h"

#define REG_CLKSEL_0	(MSM_APCS_GLB_BASE + 0x08)
#define REG_CLKDIV_0	(MSM_APCS_GLB_BASE + 0x0C)
#define REG_CLKSEL_1	(MSM_APCS_GLB_BASE + 0x10)
#define REG_CLKDIV_1	(MSM_APCS_GLB_BASE + 0x14)
#define REG_CLKOUTSEL	(MSM_APCS_GLB_BASE + 0x18)

#define MAX_VDD_MEM	1150000

enum clk_src {
	SRC_CXO,
	SRC_PLL0,
	SRC_PLL8,
	SRC_PLL9,
	NUM_SRC,
};

struct src_clock {
	struct clk *clk;
	const char *name;
};

static struct src_clock clocks[NUM_SRC] = {
	[SRC_PLL0].name = "pll0",
	[SRC_PLL8].name = "pll8",
	[SRC_PLL9].name = "pll9",
};

struct clkctl_acpu_speed {
	bool		use_for_scaling;
	unsigned int	khz;
	int		src;
	unsigned int	src_sel;
	unsigned int	src_div;
	unsigned int	vdd_cpu;
	unsigned int	vdd_mem;
	unsigned int	bw_level;
};

struct acpuclk_state {
	struct mutex			lock;
	struct clkctl_acpu_speed	*current_speed;
};

static struct acpuclk_state drv_state = {
	.current_speed = &(struct clkctl_acpu_speed){ 0 },
};

/* Instantaneous bandwidth requests in MB/s. */
#define BW_MBPS(_bw) \
	{ \
		.vectors = &(struct msm_bus_vectors){ \
			.src = MSM_BUS_MASTER_AMPSS_M0, \
			.dst = MSM_BUS_SLAVE_EBI_CH0, \
			.ib = (_bw) * 1000000UL, \
			.ab = 0, \
		}, \
		.num_paths = 1, \
	}
static struct msm_bus_paths bw_level_tbl[] = {
	[0] =  BW_MBPS(152), /* At least  19 MHz on bus. */
	[1] =  BW_MBPS(368), /* At least  46 MHz on bus. */
	[2] =  BW_MBPS(552), /* At least  69 MHz on bus. */
	[3] =  BW_MBPS(736), /* At least  92 MHz on bus. */
	[4] = BW_MBPS(1064), /* At least 133 MHz on bus. */
	[5] = BW_MBPS(1536), /* At least 192 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclock",
};

static uint32_t bus_perf_client;

static struct clkctl_acpu_speed acpu_freq_tbl[] = {
	{ 0,  19200, SRC_CXO,  0, 0, RPM_VREG_CORNER_LOW,     1050000, 0 },
	{ 1, 138000, SRC_PLL0, 6, 1, RPM_VREG_CORNER_LOW,     1050000, 2 },
	{ 1, 276000, SRC_PLL0, 6, 0, RPM_VREG_CORNER_NOMINAL, 1050000, 2 },
	{ 1, 384000, SRC_PLL8, 3, 0, RPM_VREG_CORNER_HIGH,    1150000, 4 },
	/* The row below may be changed at runtime depending on hw rev. */
	{ 1, 440000, SRC_PLL9, 2, 0, RPM_VREG_CORNER_HIGH,    1150000, 4 },
	{ 0 }
};

static void select_clk_source_div(struct clkctl_acpu_speed *s)
{
	static void * __iomem const sel_reg[] = {REG_CLKSEL_0, REG_CLKSEL_1};
	static void * __iomem const div_reg[] = {REG_CLKDIV_0, REG_CLKDIV_1};
	uint32_t next_bank;

	next_bank = !(readl_relaxed(REG_CLKOUTSEL) & 1);
	writel_relaxed(s->src_sel, sel_reg[next_bank]);
	writel_relaxed(s->src_div, div_reg[next_bank]);
	writel_relaxed(next_bank, REG_CLKOUTSEL);

	/* Wait for switch to complete. */
	mb();
	udelay(1);
}

/* Update the bus bandwidth request. */
static void set_bus_bw(unsigned int bw)
{
	int ret;

	/* Bounds check. */
	if (bw >= ARRAY_SIZE(bw_level_tbl)) {
		pr_err("invalid bandwidth request (%d)\n", bw);
		return;
	}

	/* Update bandwidth if request has changed. This may sleep. */
	ret = msm_bus_scale_client_update_request(bus_perf_client, bw);
	if (ret)
		pr_err("bandwidth request failed (%d)\n", ret);

	return;
}

/* Apply any per-cpu voltage increases. */
static int increase_vdd(unsigned int vdd_cpu, unsigned int vdd_mem)
{
	int rc = 0;

	/*
	 * Increase vdd_mem active-set before vdd_cpu.
	 * vdd_mem should be >= vdd_cpu.
	 */
	rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8018_L9, RPM_VREG_VOTER1,
				  vdd_mem, MAX_VDD_MEM, 0);
	if (rc) {
		pr_err("vdd_mem increase failed (%d)\n", rc);
		return rc;
	}

	rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8018_VDD_DIG_CORNER,
			RPM_VREG_VOTER1, vdd_cpu, RPM_VREG_CORNER_HIGH, 0);
	if (rc)
		pr_err("vdd_cpu increase failed (%d)\n", rc);

	return rc;
}

/* Apply any per-cpu voltage decreases. */
static void decrease_vdd(unsigned int vdd_cpu, unsigned int vdd_mem)
{
	int ret;

	/* Update CPU voltage. */
	ret = rpm_vreg_set_voltage(RPM_VREG_ID_PM8018_VDD_DIG_CORNER,
		RPM_VREG_VOTER1, vdd_cpu, RPM_VREG_CORNER_HIGH, 0);

	if (ret) {
		pr_err("vdd_cpu decrease failed (%d)\n", ret);
		return;
	}

	/*
	 * Decrease vdd_mem active-set after vdd_cpu.
	 * vdd_mem should be >= vdd_cpu.
	 */
	ret = rpm_vreg_set_voltage(RPM_VREG_ID_PM8018_L9, RPM_VREG_VOTER1,
				  vdd_mem, MAX_VDD_MEM, 0);
	if (ret)
		pr_err("vdd_mem decrease failed (%d)\n", ret);
}

static int acpuclk_9615_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int rc = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed;

	/* Return early if rate didn't change. */
	if (rate == strt_s->khz)
		goto out;

	/* Find target frequency. */
	for (tgt_s = acpu_freq_tbl; tgt_s->khz != 0; tgt_s++)
		if (tgt_s->khz == rate)
			break;
	if (tgt_s->khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	/* Increase VDD levels if needed. */
	if ((reason == SETRATE_CPUFREQ || reason == SETRATE_INIT)
			&& (tgt_s->khz > strt_s->khz)) {
		rc = increase_vdd(tgt_s->vdd_cpu, tgt_s->vdd_mem);
		if (rc)
			goto out;
	}

	pr_debug("Switching from CPU rate %u KHz -> %u KHz\n",
		strt_s->khz, tgt_s->khz);

	/* Switch CPU speed. */
	clk_enable(clocks[tgt_s->src].clk);
	select_clk_source_div(tgt_s);
	clk_disable(clocks[strt_s->src].clk);

	drv_state.current_speed = tgt_s;
	pr_debug("CPU speed change complete\n");

	/* Nothing else to do for SWFI or power-collapse. */
	if (reason == SETRATE_SWFI || reason == SETRATE_PC)
		goto out;

	/* Update bus bandwith request. */
	set_bus_bw(tgt_s->bw_level);

	/* Drop VDD levels if we can. */
	if (tgt_s->khz < strt_s->khz)
		decrease_vdd(tgt_s->vdd_cpu, tgt_s->vdd_mem);

out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static unsigned long acpuclk_9615_get_rate(int cpu)
{
	return drv_state.current_speed->khz;
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[30];

static void __init cpufreq_table_init(void)
{
	int i, freq_cnt = 0;

	/* Construct the freq_table tables from acpu_freq_tbl. */
	for (i = 0; acpu_freq_tbl[i].khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table); i++) {
		if (acpu_freq_tbl[i].use_for_scaling) {
			freq_table[freq_cnt].index = freq_cnt;
			freq_table[freq_cnt].frequency
				= acpu_freq_tbl[i].khz;
			freq_cnt++;
		}
	}
	/* freq_table not big enough to store all usable freqs. */
	BUG_ON(acpu_freq_tbl[i].khz != 0);

	freq_table[freq_cnt].index = freq_cnt;
	freq_table[freq_cnt].frequency = CPUFREQ_TABLE_END;

	pr_info("CPU: %d scaling frequencies supported.\n", freq_cnt);

	/* Register table with CPUFreq. */
	cpufreq_frequency_table_get_attr(freq_table, smp_processor_id());
}
#else
static void __init cpufreq_table_init(void) {}
#endif

static struct acpuclk_data acpuclk_9615_data = {
	.set_rate = acpuclk_9615_set_rate,
	.get_rate = acpuclk_9615_get_rate,
	.power_collapse_khz = 19200,
	.wait_for_irq_khz = 19200,
};

static int __init acpuclk_9615_init(struct acpuclk_soc_data *soc_data)
{
	unsigned long max_cpu_khz = 0;
	int i;

	mutex_init(&drv_state.lock);

	bus_perf_client = msm_bus_scale_register_client(&bus_client_pdata);
	if (!bus_perf_client) {
		pr_err("Unable to register bus client\n");
		BUG();
	}

	for (i = 0; i < NUM_SRC; i++) {
		if (clocks[i].name) {
			clocks[i].clk = clk_get_sys("acpu", clocks[i].name);
			BUG_ON(IS_ERR(clocks[i].clk));
			/*
			 * Prepare the PLLs because we enable/disable them
			 * in atomic context during power collapse/restore.
			 */
			BUG_ON(clk_prepare(clocks[i].clk));
		}
	}

	/* Determine the rate of PLL9 and fixup tables accordingly */
	if (clk_get_rate(clocks[SRC_PLL9].clk) == 550000000) {
		for (i = 0; i < ARRAY_SIZE(acpu_freq_tbl); i++)
			if (acpu_freq_tbl[i].src == SRC_PLL9) {
				acpu_freq_tbl[i].khz = 550000;
				acpu_freq_tbl[i].bw_level = 5;
			}
	}

	/* Improve boot time by ramping up CPU immediately. */
	for (i = 0; acpu_freq_tbl[i].khz != 0; i++)
		max_cpu_khz = acpu_freq_tbl[i].khz;
	acpuclk_9615_set_rate(smp_processor_id(), max_cpu_khz, SETRATE_INIT);

	acpuclk_register(&acpuclk_9615_data);
	cpufreq_table_init();

	return 0;
}

struct acpuclk_soc_data acpuclk_9615_soc_data __initdata = {
	.init = acpuclk_9615_init,
};
