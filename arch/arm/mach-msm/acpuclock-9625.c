/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/iopoll.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/rpm-regulator.h>
#include <mach/clk-provider.h>
#include <mach/rpm-regulator-smd.h>

#include "acpuclock.h"

#define RCG_SRC_DIV_MASK		BM(7, 0)
#define RCG_CONFIG_PGM_DATA_BIT		BIT(11)
#define RCG_CONFIG_PGM_ENA_BIT		BIT(10)
#define POLL_INTERVAL_US		1
#define APCS_RCG_UPDATE_TIMEOUT_US	20
#define GPLL0_TO_A5_ALWAYS_ENABLE	BIT(18)

#define MAX_VDD_MEM			1050000
#define MAX_VDD_CPU			1050000

/* Corner type vreg VDD values */
#define LVL_NONE        RPM_REGULATOR_CORNER_NONE
#define LVL_LOW         RPM_REGULATOR_CORNER_SVS_SOC
#define LVL_NOM         RPM_REGULATOR_CORNER_NORMAL
#define LVL_HIGH        RPM_REGULATOR_CORNER_SUPER_TURBO

enum clk_src {
	CXO,
	PLL0,
	ACPUPLL,
	NUM_SRC,
};

struct src_clock {
	struct clk *clk;
	const char *name;
};

static struct src_clock src_clocks[NUM_SRC] = {
	[PLL0].name = "pll0",
	[ACPUPLL].name = "pll14",
};

struct clkctl_acpu_speed {
	bool use_for_scaling;
	unsigned int khz;
	int src;
	unsigned int src_sel;
	unsigned int src_div;
	unsigned int vdd_cpu;
	unsigned int vdd_mem;
	unsigned int bw_level;
};

struct acpuclk_drv_data {
	struct mutex			lock;
	struct clkctl_acpu_speed	*current_speed;
	void __iomem			*apcs_rcg_config;
	void __iomem			*apcs_cpu_pwr_ctl;
	struct regulator		*vdd_cpu;
	struct regulator		*vdd_mem;
};

static struct acpuclk_drv_data drv_data = {
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
	[0] =  BW_MBPS(152), /* At least 19 MHz on bus. */
	[1] =  BW_MBPS(264), /* At least 33 MHz on bus. */
	[2] =  BW_MBPS(528), /* At least 66 MHz on bus. */
	[3] =  BW_MBPS(664), /* At least 83 MHz on bus. */
	[4] = BW_MBPS(1064), /* At least 133 MHz on bus. */
	[5] = BW_MBPS(1328), /* At least 166 MHz on bus. */
	[6] = BW_MBPS(2128), /* At least 266 MHz on bus. */
	[7] = BW_MBPS(2664), /* At least 333 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclock",
};

static uint32_t bus_perf_client;

/* TODO:
 * 1) Update MX voltage when they are avaiable
 * 2) Update bus bandwidth
 */
static struct clkctl_acpu_speed acpu_freq_tbl[] = {
	{ 0,  19200, CXO,     0, 0,   LVL_LOW,    950000, 0 },
	{ 1, 300000, PLL0,    1, 2,   LVL_LOW,    950000, 4 },
	{ 1, 600000, PLL0,    1, 0,   LVL_NOM,    950000, 4 },
	{ 1, 748800, ACPUPLL, 5, 0,   LVL_HIGH,  1050000, 7 },
	{ 1, 998400, ACPUPLL, 5, 0,   LVL_HIGH,  1050000, 7 },
	{ 0 }
};

/* Update the bus bandwidth request. */
static void set_bus_bw(unsigned int bw)
{
	int ret;

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

	/* Increase vdd_mem before vdd_cpu. vdd_mem should be >= vdd_cpu. */
	rc = regulator_set_voltage(drv_data.vdd_mem, vdd_mem, MAX_VDD_MEM);
	if (rc) {
		pr_err("vdd_mem increase failed (%d)\n", rc);
		return rc;
	}

	rc = regulator_set_voltage(drv_data.vdd_cpu, vdd_cpu, MAX_VDD_CPU);
	if (rc)
		pr_err("vdd_cpu increase failed (%d)\n", rc);

	return rc;
}

/* Apply any per-cpu voltage decreases. */
static void decrease_vdd(unsigned int vdd_cpu, unsigned int vdd_mem)
{
	int ret;

	/* Update CPU voltage. */
	ret = regulator_set_voltage(drv_data.vdd_cpu, vdd_cpu, MAX_VDD_CPU);
	if (ret) {
		pr_err("vdd_cpu decrease failed (%d)\n", ret);
		return;
	}

	/* Decrease vdd_mem after vdd_cpu. vdd_mem should be >= vdd_cpu. */
	ret = regulator_set_voltage(drv_data.vdd_mem, vdd_mem, MAX_VDD_MEM);
	if (ret)
		pr_err("vdd_mem decrease failed (%d)\n", ret);
}

static void select_clk_source_div(struct clkctl_acpu_speed *s)
{
	u32 regval, rc, src_div;
	void __iomem *apcs_rcg_config = drv_data.apcs_rcg_config;

	src_div = s->src_div ? ((2 * s->src_div) - 1) : s->src_div;

	regval = readl_relaxed(apcs_rcg_config);
	regval &= ~RCG_SRC_DIV_MASK;
	regval |= BVAL(2, 0, s->src_sel) | BVAL(7, 3, src_div);
	writel_relaxed(regval, apcs_rcg_config);

	/*
	 * Make sure writing of src and div finishes before update
	 * the configuration
	 */
	mb();

	/* Update the configruation */
	regval = readl_relaxed(apcs_rcg_config);
	regval |= RCG_CONFIG_PGM_DATA_BIT | RCG_CONFIG_PGM_ENA_BIT;
	writel_relaxed(regval, apcs_rcg_config);

	/* Wait for update to take effect */
	rc = readl_poll_timeout(apcs_rcg_config, regval,
		   !(regval & RCG_CONFIG_PGM_DATA_BIT),
		   POLL_INTERVAL_US,
		   APCS_RCG_UPDATE_TIMEOUT_US);
	if (rc)
		pr_warn("acpu rcg didn't update its configuration\n");
}

static int set_speed(struct clkctl_acpu_speed *tgt_s)
{
	int rc = 0;
	unsigned int tgt_freq_hz = tgt_s->khz * 1000;
	struct clkctl_acpu_speed *strt_s = drv_data.current_speed;
	struct clkctl_acpu_speed *cxo_s = &acpu_freq_tbl[0];
	struct clk *strt = src_clocks[strt_s->src].clk;
	struct clk *tgt = src_clocks[tgt_s->src].clk;

	if (strt_s->src == ACPUPLL && tgt_s->src == ACPUPLL) {
		/* Switch to another always on src */
		select_clk_source_div(cxo_s);

		/* Re-program acpu pll */
		clk_disable(tgt);
		rc = clk_set_rate(tgt, tgt_freq_hz);
		if (rc)
			pr_err("Failed to set ACPU PLL to %u\n", tgt_freq_hz);
		BUG_ON(clk_enable(tgt));

		/* Switch back to acpu pll */
		select_clk_source_div(tgt_s);
	} else if (strt_s->src != ACPUPLL && tgt_s->src == ACPUPLL) {
		rc = clk_set_rate(tgt, tgt_freq_hz);
		if (rc) {
			pr_err("Failed to set ACPU PLL to %u\n", tgt_freq_hz);
			return rc;
		}

		rc = clk_enable(tgt);
		if (rc) {
			pr_err("ACPU PLL enable failed\n");
			return rc;
		}

		select_clk_source_div(tgt_s);

		clk_disable(strt);
	} else {
		rc = clk_enable(tgt);
		if (rc) {
			pr_err("%s enable failed\n",
					src_clocks[tgt_s->src].name);
			return rc;
		}

		select_clk_source_div(tgt_s);

		clk_disable(strt);
	}

	return rc;
}

static int acpuclk_9625_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int rc = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_data.lock);

	strt_s = drv_data.current_speed;

	/* Return early if rate didn't change */
	if (rate == strt_s->khz)
		goto out;

	/* Find target frequency */
	for (tgt_s = acpu_freq_tbl; tgt_s->khz != 0; tgt_s++)
		if (tgt_s->khz == rate)
			break;
	if (tgt_s->khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	/* Increase VDD levels if needed */
	if ((reason == SETRATE_CPUFREQ || reason == SETRATE_INIT)
			&& (tgt_s->khz > strt_s->khz)) {
		rc = increase_vdd(tgt_s->vdd_cpu, tgt_s->vdd_mem);
		if (rc)
			goto out;
	}

	pr_debug("Switching from CPU rate %u KHz -> %u KHz\n",
		strt_s->khz, tgt_s->khz);

	/* Switch CPU speed. */
	rc = set_speed(tgt_s);
	if (rc)
		goto out;

	drv_data.current_speed = tgt_s;
	pr_debug("CPU speed change complete\n");

	/* Nothing else to do for SWFI or power-collapse. */
	if (reason == SETRATE_SWFI || reason == SETRATE_PC)
		goto out;

	/* Update bus bandwith request */
	set_bus_bw(tgt_s->bw_level);

	/* Drop VDD levels if we can. */
	if (tgt_s->khz < strt_s->khz)
		decrease_vdd(tgt_s->vdd_cpu, tgt_s->vdd_mem);

out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_data.lock);
	return rc;
}

static unsigned long acpuclk_9625_get_rate(int cpu)
{
	return drv_data.current_speed->khz;
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[30];

static void __init cpufreq_table_init(void)
{
	int i, freq_cnt = 0;

	/* Construct the freq_table tables from acpu_freq_tbl. */
	for (i = 0; acpu_freq_tbl[i].khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table); i++) {
		if (!acpu_freq_tbl[i].use_for_scaling)
			continue;
		freq_table[freq_cnt].index = freq_cnt;
		freq_table[freq_cnt].frequency = acpu_freq_tbl[i].khz;
		freq_cnt++;
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

static struct acpuclk_data acpuclk_9625_data = {
	.set_rate = acpuclk_9625_set_rate,
	.get_rate = acpuclk_9625_get_rate,
	.power_collapse_khz = 19200,
	.wait_for_irq_khz = 19200,
};

static int __init acpuclk_9625_probe(struct platform_device *pdev)
{
	unsigned long max_cpu_khz = 0;
	struct resource *res;
	int i;
	u32 regval;

	mutex_init(&drv_data.lock);

	bus_perf_client = msm_bus_scale_register_client(&bus_client_pdata);
	if (!bus_perf_client) {
		pr_err("Unable to register bus client\n");
		BUG();
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rcg_base");
	if (!res)
		return -EINVAL;

	drv_data.apcs_rcg_config = ioremap(res->start, resource_size(res));
	if (!drv_data.apcs_rcg_config)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwr_base");
	if (!res)
		return -EINVAL;

	drv_data.apcs_cpu_pwr_ctl = ioremap(res->start, resource_size(res));
	if (!drv_data.apcs_cpu_pwr_ctl)
		return -ENOMEM;

	drv_data.vdd_cpu = regulator_get(&pdev->dev, "a5_cpu");
	if (IS_ERR(drv_data.vdd_cpu)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a5_cpu");
		return PTR_ERR(drv_data.vdd_cpu);
	}

	drv_data.vdd_mem = regulator_get(&pdev->dev, "a5_mem");
	if (IS_ERR(drv_data.vdd_mem)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a5_mem");
		return PTR_ERR(drv_data.vdd_mem);
	}

	/* Disable hardware gating of gpll0 to A5SS */
	regval = readl_relaxed(drv_data.apcs_cpu_pwr_ctl);
	regval |= GPLL0_TO_A5_ALWAYS_ENABLE;
	writel_relaxed(regval, drv_data.apcs_cpu_pwr_ctl);

	for (i = 0; i < NUM_SRC; i++) {
		if (!src_clocks[i].name)
			continue;
		src_clocks[i].clk = clk_get(&pdev->dev, src_clocks[i].name);
		BUG_ON(IS_ERR(src_clocks[i].clk));
		/*
		 * Prepare the PLLs because we enable/disable them
		 * in atomic context during power collapse/restore.
		 */
		BUG_ON(clk_prepare(src_clocks[i].clk));
	}

	/* Improve boot time by ramping up CPU immediately */
	for (i = 0; acpu_freq_tbl[i].khz != 0 &&
				acpu_freq_tbl[i].use_for_scaling; i++)
		max_cpu_khz = acpu_freq_tbl[i].khz;

	acpuclk_9625_set_rate(smp_processor_id(), max_cpu_khz, SETRATE_INIT);

	acpuclk_register(&acpuclk_9625_data);
	cpufreq_table_init();

	return 0;
}

static struct of_device_id acpuclk_9625_match_table[] = {
	{.compatible = "qcom,acpuclk-9625"},
	{}
};

static struct platform_driver acpuclk_9625_driver = {
	.driver = {
		.name = "acpuclk-9625",
		.of_match_table = acpuclk_9625_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_9625_init(void)
{
	return platform_driver_probe(&acpuclk_9625_driver, acpuclk_9625_probe);
}
device_initcall(acpuclk_9625_init);
