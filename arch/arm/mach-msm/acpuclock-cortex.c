/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include "acpuclock-cortex.h"

#define POLL_INTERVAL_US		1
#define APCS_RCG_UPDATE_TIMEOUT_US	20

static struct acpuclk_drv_data *acpuclk_init_data;
static uint32_t bus_perf_client;

/* Update the bus bandwidth request. */
static void set_bus_bw(unsigned int bw)
{
	int ret;

	if (bw >= acpuclk_init_data->bus_scale->num_usecases) {
		pr_err("invalid bandwidth request (%d)\n", bw);
		return;
	}

	/* Update bandwidth if request has changed. This may sleep. */
	ret = msm_bus_scale_client_update_request(bus_perf_client, bw);
	if (ret)
		pr_err("bandwidth request failed (%d)\n", ret);

	return;
}

/* Apply any voltage increases. */
static int increase_vdd(unsigned int vdd_cpu, unsigned int vdd_mem)
{
	int rc = 0;

	/* Increase vdd_mem before vdd_cpu. vdd_mem should be >= vdd_cpu. */
	rc = regulator_set_voltage(acpuclk_init_data->vdd_mem, vdd_mem,
		acpuclk_init_data->vdd_max_mem);
	if (rc) {
		pr_err("vdd_mem increase failed (%d)\n", rc);
		return rc;
	}

	rc = regulator_set_voltage(acpuclk_init_data->vdd_cpu, vdd_cpu,
		acpuclk_init_data->vdd_max_cpu);
	if (rc)
		pr_err("vdd_cpu increase failed (%d)\n", rc);

	return rc;
}

/* Apply any per-cpu voltage decreases. */
static void decrease_vdd(unsigned int vdd_cpu, unsigned int vdd_mem)
{
	int ret;

	/* Update CPU voltage. */
	ret = regulator_set_voltage(acpuclk_init_data->vdd_cpu, vdd_cpu,
		acpuclk_init_data->vdd_max_cpu);
	if (ret) {
		pr_err("vdd_cpu decrease failed (%d)\n", ret);
		return;
	}

	/* Decrease vdd_mem after vdd_cpu. vdd_mem should be >= vdd_cpu. */
	ret = regulator_set_voltage(acpuclk_init_data->vdd_mem, vdd_mem,
		acpuclk_init_data->vdd_max_mem);
	if (ret)
		pr_err("vdd_mem decrease failed (%d)\n", ret);
}

static void select_clk_source_div(struct acpuclk_drv_data *drv_data,
	struct clkctl_acpu_speed *s)
{
	u32 regval, rc, src_div;
	void __iomem *apcs_rcg_config = drv_data->apcs_rcg_config;
	void __iomem *apcs_rcg_cmd = drv_data->apcs_rcg_cmd;
	struct acpuclk_reg_data *r = &drv_data->reg_data;

	src_div = s->src_div ? ((2 * s->src_div) - 1) : s->src_div;

	regval = readl_relaxed(apcs_rcg_config);
	regval &= ~r->cfg_src_mask;
	regval |= s->src_sel << r->cfg_src_shift;
	regval &= ~r->cfg_div_mask;
	regval |= src_div << r->cfg_div_shift;
	writel_relaxed(regval, apcs_rcg_config);

	/* Update the configuration */
	regval = readl_relaxed(apcs_rcg_cmd);
	regval |= r->update_mask;
	writel_relaxed(regval, apcs_rcg_cmd);

	/* Wait for the update to take effect */
	rc = readl_poll_timeout(apcs_rcg_cmd, regval,
		   !(regval & r->poll_mask),
		   POLL_INTERVAL_US,
		   APCS_RCG_UPDATE_TIMEOUT_US);
	if (rc)
		pr_warn("acpu rcg didn't update its configuration\n");
}

/*
 * This function can be called in both atomic and nonatomic context.
 * Since regulator APIS can sleep, we cannot always use the clk prepare
 * unprepare API.
 */
static int set_speed(struct clkctl_acpu_speed *tgt_s, bool atomic)
{
	int rc = 0;
	unsigned int tgt_freq_hz = tgt_s->khz * 1000;
	struct clkctl_acpu_speed *strt_s = acpuclk_init_data->current_speed;
	struct clkctl_acpu_speed *cxo_s = &acpuclk_init_data->freq_tbl[0];
	struct clk *strt = acpuclk_init_data->src_clocks[strt_s->src].clk;
	struct clk *tgt = acpuclk_init_data->src_clocks[tgt_s->src].clk;

	if (strt_s->src == ACPUPLL && tgt_s->src == ACPUPLL) {
		/* Switch to another always on src */
		select_clk_source_div(acpuclk_init_data, cxo_s);

		/* Re-program acpu pll */
		if (atomic)
			clk_disable(tgt);
		else
			clk_disable_unprepare(tgt);

		rc = clk_set_rate(tgt, tgt_freq_hz);
		if (rc)
			pr_err("Failed to set ACPU PLL to %u\n", tgt_freq_hz);

		if (atomic)
			BUG_ON(clk_enable(tgt));
		else
			BUG_ON(clk_prepare_enable(tgt));

		/* Switch back to acpu pll */
		select_clk_source_div(acpuclk_init_data, tgt_s);

	} else if (strt_s->src != ACPUPLL && tgt_s->src == ACPUPLL) {
		rc = clk_set_rate(tgt, tgt_freq_hz);
		if (rc) {
			pr_err("Failed to set ACPU PLL to %u\n", tgt_freq_hz);
			return rc;
		}

		if (atomic)
			clk_enable(tgt);
		else
			clk_prepare_enable(tgt);

		if (rc) {
			pr_err("ACPU PLL enable failed\n");
			return rc;
		}

		select_clk_source_div(acpuclk_init_data, tgt_s);

		if (atomic)
			clk_disable(strt);
		else
			clk_disable_unprepare(strt);

	} else {
		if (atomic)
			clk_enable(tgt);
		else
			clk_prepare_enable(tgt);

		if (rc) {
			pr_err("%s enable failed\n",
				acpuclk_init_data->src_clocks[tgt_s->src].name);
			return rc;
		}

		select_clk_source_div(acpuclk_init_data, tgt_s);

		if (atomic)
			clk_disable(strt);
		else
			clk_disable_unprepare(strt);

	}

	return rc;
}

static int acpuclk_cortex_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int rc = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&acpuclk_init_data->lock);

	strt_s = acpuclk_init_data->current_speed;

	/* Return early if rate didn't change */
	if (rate == strt_s->khz)
		goto out;

	/* Find target frequency */
	for (tgt_s = acpuclk_init_data->freq_tbl; tgt_s->khz != 0; tgt_s++)
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

	/* Switch CPU speed. Flag indicates atomic context */
	if (reason == SETRATE_CPUFREQ || reason == SETRATE_INIT)
		rc = set_speed(tgt_s, false);
	else
		rc = set_speed(tgt_s, true);

	if (rc)
		goto out;

	acpuclk_init_data->current_speed = tgt_s;
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
		mutex_unlock(&acpuclk_init_data->lock);
	return rc;
}

static unsigned long acpuclk_cortex_get_rate(int cpu)
{
	return acpuclk_init_data->current_speed->khz;
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[30];

static void __init cpufreq_table_init(void)
{
	int i, freq_cnt = 0;

	/* Construct the freq_table tables from acpuclk_init_data->freq_tbl. */
	for (i = 0; acpuclk_init_data->freq_tbl[i].khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table); i++) {
		if (!acpuclk_init_data->freq_tbl[i].use_for_scaling)
			continue;
		freq_table[freq_cnt].index = freq_cnt;
		freq_table[freq_cnt].frequency =
			acpuclk_init_data->freq_tbl[i].khz;
		freq_cnt++;
	}
	/* freq_table not big enough to store all usable freqs. */
	BUG_ON(acpuclk_init_data->freq_tbl[i].khz != 0);

	freq_table[freq_cnt].index = freq_cnt;
	freq_table[freq_cnt].frequency = CPUFREQ_TABLE_END;

	pr_info("CPU: %d scaling frequencies supported.\n", freq_cnt);

	/* Register table with CPUFreq. */
	for_each_possible_cpu(i)
		cpufreq_frequency_table_get_attr(freq_table, i);
}
#else
static void __init cpufreq_table_init(void) {}
#endif

static struct acpuclk_data acpuclk_cortex_data = {
	.set_rate = acpuclk_cortex_set_rate,
	.get_rate = acpuclk_cortex_get_rate,
	.power_collapse_khz = 19200,
	.wait_for_irq_khz = 19200,
};

int __init acpuclk_cortex_init(struct platform_device *pdev,
	struct acpuclk_drv_data *data)
{
	unsigned long max_cpu_khz = 0;
	int i, rc;

	acpuclk_init_data = data;
	mutex_init(&acpuclk_init_data->lock);

	bus_perf_client = msm_bus_scale_register_client(
		acpuclk_init_data->bus_scale);
	if (!bus_perf_client) {
		pr_err("Unable to register bus client\n");
		BUG();
	}

	for (i = 0; i < NUM_SRC; i++) {
		if (!acpuclk_init_data->src_clocks[i].name)
			continue;
		acpuclk_init_data->src_clocks[i].clk =
			clk_get(&pdev->dev,
				acpuclk_init_data->src_clocks[i].name);
		BUG_ON(IS_ERR(acpuclk_init_data->src_clocks[i].clk));
	}

	/* Improve boot time by ramping up CPU immediately */
	for (i = 0; acpuclk_init_data->freq_tbl[i].khz != 0; i++)
		if (acpuclk_init_data->freq_tbl[i].use_for_scaling)
			max_cpu_khz = acpuclk_init_data->freq_tbl[i].khz;

	/* Initialize regulators */
	rc = increase_vdd(acpuclk_init_data->vdd_max_cpu,
		acpuclk_init_data->vdd_max_mem);
	if (rc)
		goto err_vdd;

	rc = regulator_enable(acpuclk_init_data->vdd_mem);
	if (rc) {
		dev_err(&pdev->dev, "regulator_enable for mem failed\n");
		goto err_vdd;
	}

	rc = regulator_enable(acpuclk_init_data->vdd_cpu);
	if (rc) {
		dev_err(&pdev->dev, "regulator_enable for cpu failed\n");
		goto err_vdd_cpu;
	}

	/*
	 * Select a state which is always a valid transition to align SW with
	 * the HW configuration set by the bootloaders.
	 */
	acpuclk_cortex_set_rate(0, acpuclk_cortex_data.power_collapse_khz,
		SETRATE_INIT);
	acpuclk_cortex_set_rate(0, max_cpu_khz, SETRATE_INIT);

	acpuclk_register(&acpuclk_cortex_data);
	cpufreq_table_init();

	return 0;

err_vdd_cpu:
	regulator_disable(acpuclk_init_data->vdd_mem);
err_vdd:
	regulator_put(acpuclk_init_data->vdd_mem);
	regulator_put(acpuclk_init_data->vdd_cpu);

	for (i = 0; i < NUM_SRC; i++) {
		if (!acpuclk_init_data->src_clocks[i].name)
			continue;
		clk_put(acpuclk_init_data->src_clocks[i].clk);
	}
	return rc;
}
