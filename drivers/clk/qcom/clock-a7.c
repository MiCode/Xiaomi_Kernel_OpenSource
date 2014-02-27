/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>

DEFINE_VDD_REGS_INIT(vdd_cpu, 1);

static struct mux_div_clk a7ssmux = {
	.ops = &rcg_mux_div_ops,
	.safe_freq = 300000000,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a7ssmux",
		.ops = &clk_ops_mux_div_clk,
		.vdd_class = &vdd_cpu,
		CLK_INIT(a7ssmux.c),
	},
	.parents = (struct clk_src[8]) {},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
};

static struct clk_lookup clock_tbl_a7[] = {
	CLK_LOOKUP("cpu0_clk",	a7ssmux.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu1_clk",	a7ssmux.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu2_clk",	a7ssmux.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu3_clk",	a7ssmux.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu0_clk",	a7ssmux.c, "fe805664.qcom,pm"),
	CLK_LOOKUP("cpu1_clk",	a7ssmux.c, "fe805664.qcom,pm"),
	CLK_LOOKUP("cpu2_clk",	a7ssmux.c, "fe805664.qcom,pm"),
	CLK_LOOKUP("cpu3_clk",	a7ssmux.c, "fe805664.qcom,pm"),
	CLK_LOOKUP("cpu0_clk",   a7ssmux.c, "8600664.qcom,pm"),
	CLK_LOOKUP("cpu1_clk",   a7ssmux.c, "8600664.qcom,pm"),
	CLK_LOOKUP("cpu2_clk",   a7ssmux.c, "8600664.qcom,pm"),
	CLK_LOOKUP("cpu3_clk",   a7ssmux.c, "8600664.qcom,pm"),
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

static void get_speed_bin(struct platform_device *pdev, int *bin, int *version)
{
	struct resource *res;
	void __iomem *base;
	u32 pte_efuse, redundant_sel, valid;

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

	redundant_sel = (pte_efuse >> 24) & 0x7;
	*bin = pte_efuse & 0x7;
	valid = (pte_efuse >> 3) & 0x1;
	*version = (pte_efuse >> 4) & 0x3;

	if (redundant_sel == 1)
		*bin = (pte_efuse >> 27) & 0x7;

	if (!valid) {
		dev_info(&pdev->dev, "Speed bin not set. Defaulting to 0!\n");
		*bin = 0;
	} else {
		dev_info(&pdev->dev, "Speed bin: %d\n", *bin);
	}

	dev_info(&pdev->dev, "PVS version: %d\n", *version);

	return;
}

static void get_speed_bin_b(struct platform_device *pdev, int *bin,
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

static int of_get_clk_src(struct platform_device *pdev, struct clk_src *parents)
{
	struct device_node *of = pdev->dev.of_node;
	int num_parents, i, j, index;
	struct clk *c;
	char clk_name[] = "clk-x";

	num_parents = of_property_count_strings(of, "clock-names");
	if (num_parents <= 0 || num_parents > 8) {
		dev_err(&pdev->dev, "missing clock-names\n");
		return -EINVAL;
	}

	j = 0;
	for (i = 0; i < 8; i++) {
		snprintf(clk_name, ARRAY_SIZE(clk_name), "clk-%d", i);
		index = of_property_match_string(of, "clock-names", clk_name);
		if (IS_ERR_VALUE(index))
			continue;

		parents[j].sel = i;
		parents[j].src = c = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(c)) {
			if (c != ERR_PTR(-EPROBE_DEFER))
				dev_err(&pdev->dev, "clk_get: %s\n fail",
						clk_name);
			return PTR_ERR(c);
		}
		j++;
	}

	return num_parents;
}

static int clock_a7_probe(struct platform_device *pdev)
{
	struct resource *res;
	int speed_bin = 0, version = 0, rc, cpu;
	unsigned long rate, aux_rate;
	struct clk *aux_clk, *main_pll;
	char prop_name[] = "qcom,speedX-bin-vX";
	const void *prop;
	bool compat_bin = false;

	compat_bin = of_device_is_compatible(pdev->dev.of_node,
						"qcom,clock-a53-8916");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rcg-base");
	if (!res) {
		dev_err(&pdev->dev, "missing rcg-base\n");
		return -EINVAL;
	}
	a7ssmux.base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!a7ssmux.base) {
		dev_err(&pdev->dev, "ioremap failed for rcg-base\n");
		return -ENOMEM;
	}

	vdd_cpu.regulator[0] = devm_regulator_get(&pdev->dev, "cpu-vdd");
	if (IS_ERR(vdd_cpu.regulator[0])) {
		if (PTR_ERR(vdd_cpu.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "unable to get regulator\n");
		return PTR_ERR(vdd_cpu.regulator[0]);
	}

	rc = of_get_clk_src(pdev, a7ssmux.parents);
	if (IS_ERR_VALUE(rc))
		return rc;

	a7ssmux.num_parents = rc;

	/* Override the existing safe operating frequency */
	prop = of_get_property(pdev->dev.of_node, "qcom,safe-freq", NULL);
	if (prop)
		a7ssmux.safe_freq = of_read_ulong(prop, 1);

	if (compat_bin)
		get_speed_bin_b(pdev, &speed_bin, &version);
	else
		get_speed_bin(pdev, &speed_bin, &version);

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d", speed_bin, version);
	rc = of_get_fmax_vdd_class(pdev, &a7ssmux.c, prop_name);
	if (rc) {
		/* Fall back to most conservative PVS table */
		dev_err(&pdev->dev, "Unable to load voltage plan %s!\n",
								prop_name);
		rc = of_get_fmax_vdd_class(pdev, &a7ssmux.c,
						"qcom,speed0-bin-v0");
		if (rc) {
			dev_err(&pdev->dev,
					"Unable to load safe voltage plan\n");
			return rc;
		}
		dev_info(&pdev->dev, "Safe voltage plan loaded.\n");
	}

	rc = msm_clock_register(clock_tbl_a7, ARRAY_SIZE(clock_tbl_a7));
	if (rc) {
		dev_err(&pdev->dev, "msm_clock_register failed\n");
		return rc;
	}

	/* Force a PLL reconfiguration */
	aux_clk = a7ssmux.parents[0].src;
	main_pll = a7ssmux.parents[1].src;

	aux_rate = clk_get_rate(aux_clk);
	rate = clk_get_rate(&a7ssmux.c);
	clk_set_rate(&a7ssmux.c, aux_rate);
	clk_set_rate(main_pll, clk_round_rate(main_pll, 1));
	clk_set_rate(&a7ssmux.c, rate);

	/*
	 * We don't want the CPU clocks to be turned off at late init
	 * if CPUFREQ or HOTPLUG configs are disabled. So, bump up the
	 * refcount of these clocks. Any cpufreq/hotplug manager can assume
	 * that the clocks have already been prepared and enabled by the time
	 * they take over.
	 */
	get_online_cpus();
	for_each_online_cpu(cpu)
		WARN(clk_prepare_enable(&a7ssmux.c),
			"Unable to turn on CPU clock");
	put_online_cpus();
	return 0;
}

static struct of_device_id clock_a7_match_table[] = {
	{.compatible = "qcom,clock-a7-8226"},
	{.compatible = "qcom,clock-a7-krypton"},
	{.compatible = "qcom,clock-a7-9630"},
	{.compatible = "qcom,clock-a53-8916"},
	{}
};

static struct platform_driver clock_a7_driver = {
	.probe = clock_a7_probe,
	.driver = {
		.name = "clock-a7",
		.of_match_table = clock_a7_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clock_a7_init(void)
{
	return platform_driver_register(&clock_a7_driver);
}
arch_initcall(clock_a7_init);
