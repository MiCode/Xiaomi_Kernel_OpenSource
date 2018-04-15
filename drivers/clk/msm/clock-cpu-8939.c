/*
 * Copyright (c) 2014-2016, 2018, The Linux Foundation. All rights reserved.
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

#include "clock.h"
#include <dt-bindings/clock/msm-cpu-clocks-8939.h>

DEFINE_VDD_REGS_INIT(vdd_cpu_bc, 1);
DEFINE_VDD_REGS_INIT(vdd_cpu_lc, 1);
DEFINE_VDD_REGS_INIT(vdd_cpu_cci, 1);

enum {
	A53SS_MUX_BC,
	A53SS_MUX_LC,
	A53SS_MUX_CCI,
	A53SS_MUX_NUM,
};

static const char * const mux_names[] = { "c1", "c0", "cci"};

struct cpu_clk_8939 {
	u32 cpu_reg_mask;
	cpumask_t cpumask;
	bool hw_low_power_ctrl;
	struct pm_qos_request req;
	struct clk c;
	struct latency_level latency_lvl;
	s32 cpu_latency_no_l2_pc_us;
};

static struct mux_div_clk a53ssmux_bc = {
	.ops = &rcg_mux_div_ops,
	.safe_freq = 400000000,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a53ssmux_bc",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a53ssmux_bc.c),
	},
	.parents = (struct clk_src[8]) {},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
};

static struct mux_div_clk a53ssmux_lc = {
	.ops = &rcg_mux_div_ops,
	.safe_freq = 200000000,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a53ssmux_lc",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a53ssmux_lc.c),
	},
	.parents = (struct clk_src[8]) {},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
};

static struct mux_div_clk a53ssmux_cci = {
	.ops = &rcg_mux_div_ops,
	.safe_freq = 200000000,
	.data = {
		.max_div = 32,
		.min_div = 2,
		.is_half_divider = true,
	},
	.c = {
		.dbg_name = "a53ssmux_cci",
		.ops = &clk_ops_mux_div_clk,
		CLK_INIT(a53ssmux_cci.c),
	},
	.parents = (struct clk_src[8]) {},
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
};

static void do_nothing(void *unused) { }

static inline struct cpu_clk_8939 *to_cpu_clk_8939(struct clk *c)
{
	return container_of(c, struct cpu_clk_8939, c);
}

static enum handoff cpu_clk_8939_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	return HANDOFF_DISABLED_CLK;
}

static long cpu_clk_8939_round_rate(struct clk *c, unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static int cpu_clk_8939_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct cpu_clk_8939 *cpuclk = to_cpu_clk_8939(c);
	bool hw_low_power_ctrl = cpuclk->hw_low_power_ctrl;

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

	if (hw_low_power_ctrl)
		pm_qos_remove_request(&cpuclk->req);

	return ret;
}

static struct clk_ops clk_ops_cpu = {
	.set_rate = cpu_clk_8939_set_rate,
	.round_rate = cpu_clk_8939_round_rate,
	.handoff = cpu_clk_8939_handoff,
};

static struct cpu_clk_8939 a53_bc_clk = {
	.cpu_reg_mask = 0x3,
	.latency_lvl = {
		.affinity_level = LPM_AFF_LVL_L2,
		.reset_level = LPM_RESET_LVL_GDHS,
		.level_name = "perf",
	},
	.cpu_latency_no_l2_pc_us = 300,
	.c = {
		.parent = &a53ssmux_bc.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_bc,
		.dbg_name = "a53_bc_clk",
		CLK_INIT(a53_bc_clk.c),
	},
};

static struct cpu_clk_8939 a53_lc_clk = {
	.cpu_reg_mask = 0x103,
	.latency_lvl = {
		.affinity_level = LPM_AFF_LVL_L2,
		.reset_level = LPM_RESET_LVL_GDHS,
		.level_name = "pwr",
	},
	.cpu_latency_no_l2_pc_us = 300,
	.c = {
		.parent = &a53ssmux_lc.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_lc,
		.dbg_name = "a53_lc_clk",
		CLK_INIT(a53_lc_clk.c),
	},
};

static struct cpu_clk_8939 cci_clk = {
	.c = {
		.parent = &a53ssmux_cci.c,
		.ops = &clk_ops_cpu,
		.vdd_class = &vdd_cpu_cci,
		.dbg_name = "cci_clk",
		CLK_INIT(cci_clk.c),
	},
};

static struct clk_lookup cpu_clocks_8939[] = {
	CLK_LIST(a53ssmux_lc),
	CLK_LIST(a53ssmux_bc),
	CLK_LIST(a53ssmux_cci),
	CLK_LIST(a53_bc_clk),
	CLK_LIST(a53_lc_clk),
	CLK_LIST(cci_clk),
};

static struct clk_lookup cpu_clocks_8939_single_cluster[] = {
	CLK_LIST(a53ssmux_bc),
	CLK_LIST(a53_bc_clk),
};


static struct mux_div_clk *a53ssmux[] = {&a53ssmux_bc,
						&a53ssmux_lc, &a53ssmux_cci};

static struct cpu_clk_8939 *cpuclk[] = { &a53_bc_clk, &a53_lc_clk, &cci_clk};

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node;
	const u32 *cell;
	u64 hwid;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		goto fail;

	cell = of_get_property(cpu_node, "reg", NULL);
	if (!cell) {
		pr_err("%s: missing reg property\n", cpu_node->full_name);
		goto fail;
	}

	/*
	 * CPU 0/1/2/3 --> a53_bc_clk and mask = 0x103
	 * CPU 4/5/6/7 --> a53_lc_clk and mask = 0x3
	 */
	hwid = of_read_number(cell, of_n_addr_cells(cpu_node));
	if ((hwid | a53_bc_clk.cpu_reg_mask) == a53_bc_clk.cpu_reg_mask)
		return &a53_lc_clk.c;
	if ((hwid | a53_lc_clk.cpu_reg_mask) == a53_lc_clk.cpu_reg_mask)
		return &a53_bc_clk.c;

fail:
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
	void __iomem *base, *base1, *base2;
	u32 pte_efuse, pte_efuse1, pte_efuse2;

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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse1");
	if (!res) {
		dev_info(&pdev->dev,
			 "No PVS version available. Defaulting to 0!\n");
		goto out;
	}

	base1 = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base1) {
		dev_warn(&pdev->dev,
			 "Unable to read efuse1 data. Defaulting to 0!\n");
		goto out;
	}

	pte_efuse1 = readl_relaxed(base1);
	devm_iounmap(&pdev->dev, base1);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse2");
	if (!res) {
		dev_info(&pdev->dev,
			 "No PVS version available. Defaulting to 0!\n");
		goto out;
	}

	base2 = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base2) {
		dev_warn(&pdev->dev,
			 "Unable to read efuse2 data. Defaulting to 0!\n");
		goto out;
	}

	pte_efuse2 = readl_relaxed(base2);
	devm_iounmap(&pdev->dev, base2);

	*version = ((pte_efuse1 >> 29 & 0x1) | ((pte_efuse2 >> 18 & 0x3) << 1));

out:
	dev_info(&pdev->dev, "Speed bin: %d PVS Version: %d\n", *bin,
								*version);
}

static int of_get_clk_src(struct platform_device *pdev,
				struct clk_src *parents, int mux_id)
{
	struct device_node *of = pdev->dev.of_node;
	int mux_parents, i, j, index;
	struct clk *c;
	char clk_name[] = "clk-xxx-x";

	mux_parents = of_property_count_strings(of, "clock-names");
	if (mux_parents <= 0) {
		dev_err(&pdev->dev, "missing clock-names\n");
		return -EINVAL;
	}
	j = 0;

	for (i = 0; i < 8; i++) {
		snprintf(clk_name, ARRAY_SIZE(clk_name), "clk-%s-%d",
							mux_names[mux_id], i);
		index = of_property_match_string(of, "clock-names", clk_name);
		if (index < 0)
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

	return j;
}

static int cpu_parse_devicetree(struct platform_device *pdev, int mux_id)
{
	struct resource *res;
	int rc;
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

	rc = of_get_clk_src(pdev, a53ssmux[mux_id]->parents, mux_id);
	if (rc < 0)
		return rc;

	a53ssmux[mux_id]->num_parents = rc;

	return 0;
}

static long corner_to_voltage(unsigned long corner, struct device *dev)
{
	struct dev_pm_opp *oppl;
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
	struct dev_pm_opp *oppl;
	int j = 1;

	rcu_read_lock();
	/* Check if the regulator driver has already populated OPP tables */
	oppl = dev_pm_opp_find_freq_exact(vregdev, 2, true);
	rcu_read_unlock();
	if (!IS_ERR_OR_NULL(oppl))
		use_voltages = true;

	while (1) {
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
	}

	return 0;
}

static void print_opp_table(int a53_c0_cpu, int a53_c1_cpu, bool single_cluster)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc0_fmax, apc1_fmax, apc0_fmin, apc1_fmin;

	if (!single_cluster) {
		apc0_fmax = a53_lc_clk.c.fmax[a53_lc_clk.c.num_fmax - 1];
		apc0_fmin = a53_lc_clk.c.fmax[1];
	}
	apc1_fmax = a53_bc_clk.c.fmax[a53_bc_clk.c.num_fmax - 1];
	apc1_fmin = a53_bc_clk.c.fmax[1];

	rcu_read_lock();
	if (!single_cluster) {
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
	}
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

static void populate_opp_table(struct platform_device *pdev,
					bool single_cluster)
{
	struct platform_device *apc0_dev = 0, *apc1_dev;
	struct device_node *apc0_node = NULL, *apc1_node;
	unsigned long apc0_fmax = 0, apc1_fmax = 0;
	int cpu, a53_c0_cpu = 0, a53_c1_cpu = 0;

	if (!single_cluster)
		apc0_node = of_parse_phandle(pdev->dev.of_node,
						"vdd-c0-supply", 0);
	apc1_node = of_parse_phandle(pdev->dev.of_node, "vdd-c1-supply", 0);
	if (!apc0_node && !single_cluster) {
		pr_err("can't find the apc0 dt node.\n");
		return;
	}
	if (!apc1_node) {
		pr_err("can't find the apc1 dt node.\n");
		return;
	}
	if (!single_cluster)
		apc0_dev = of_find_device_by_node(apc0_node);

	apc1_dev = of_find_device_by_node(apc1_node);
	if (!apc1_dev && !single_cluster) {
		pr_err("can't find the apc0 device node.\n");
		return;
	}
	if (!apc1_dev) {
		pr_err("can't find the apc1 device node.\n");
		return;
	}

	if (!single_cluster)
		apc0_fmax = a53_lc_clk.c.fmax[a53_lc_clk.c.num_fmax - 1];

	apc1_fmax = a53_bc_clk.c.fmax[a53_bc_clk.c.num_fmax - 1];

	for_each_possible_cpu(cpu) {
		pr_debug("the CPU number is : %d\n", cpu);
		if (cpu/4 == 0) {
			a53_c1_cpu = cpu;
			WARN(add_opp(&a53_bc_clk.c, get_cpu_device(cpu),
				     &apc1_dev->dev, apc1_fmax),
				     "Failed to add OPP levels for A53 big cluster\n");
		} else if (cpu/4 == 1 && !single_cluster) {
			a53_c0_cpu = cpu;
			WARN(add_opp(&a53_lc_clk.c, get_cpu_device(cpu),
				     &apc0_dev->dev, apc0_fmax),
				     "Failed to add OPP levels for A53 little cluster\n");
		}
	}

	/* One time print during bootup */
	pr_info("clock-cpu-8939: OPP tables populated (cpu %d and %d)",
		a53_c0_cpu, a53_c1_cpu);

	print_opp_table(a53_c0_cpu, a53_c1_cpu, single_cluster);

}

static void config_pll(int mux_id)
{
	unsigned long rate, aux_rate;
	struct clk *aux_clk, *main_pll;

	aux_clk = a53ssmux[mux_id]->parents[0].src;
	main_pll = a53ssmux[mux_id]->parents[1].src;

	aux_rate = clk_get_rate(aux_clk);
	rate = clk_get_rate(&a53ssmux[mux_id]->c);
	clk_set_rate(&a53ssmux[mux_id]->c, aux_rate);
	clk_set_rate(main_pll, clk_round_rate(main_pll, 1));
	clk_set_rate(&a53ssmux[mux_id]->c, rate);

}

static int clock_8939_pm_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		clk_unprepare(&a53_lc_clk.c);
		clk_unprepare(&a53_bc_clk.c);
		clk_unprepare(&cci_clk.c);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		clk_prepare(&a53_lc_clk.c);
		clk_prepare(&a53_bc_clk.c);
		clk_prepare(&cci_clk.c);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int clock_8939_pm_event_single_cluster(struct notifier_block *this,
						unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		clk_unprepare(&a53_bc_clk.c);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		clk_prepare(&a53_bc_clk.c);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block clock_8939_pm_notifier = {
	.notifier_call = clock_8939_pm_event,
};

static struct notifier_block clock_8939_pm_notifier_single_cluster = {
	.notifier_call = clock_8939_pm_event_single_cluster,
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
	bool single_cluster = 0;
	unsigned long rate;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
							"qcom,cpu-clock-8939");
	if (!ofnode)
		ofnode = of_find_compatible_node(NULL, NULL,
						"qcom,cpu-clock-8917");
	if (ofnode)
		single_cluster = of_property_read_bool(ofnode,
							"qcom,num-cluster");

	rate  = (a53_bc_clk.c.count) ? a53_bc_clk.c.rate : 0;
	pr_err("%s frequency: %10lu Hz\n", a53_bc_clk.c.dbg_name, rate);

	if (!single_cluster) {
		rate  = (a53_lc_clk.c.count) ? a53_lc_clk.c.rate : 0;
		pr_err("%s frequency: %10lu Hz\n", a53_lc_clk.c.dbg_name, rate);
	}

	return NOTIFY_OK;
}

static struct notifier_block clock_panic_notifier = {
	.notifier_call = clock_panic_callback,
	.priority = 1,
};

static int clock_a53_probe(struct platform_device *pdev)
{
	int speed_bin, version, rc, cpu, mux_id, rate;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	int mux_num;
	bool single_cluster;

	single_cluster = of_property_read_bool(pdev->dev.of_node,
						"qcom,num-cluster");

	get_speed_bin(pdev, &speed_bin, &version);

	mux_num = single_cluster ? A53SS_MUX_LC:A53SS_MUX_NUM;

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
	if (single_cluster)
		rc = of_msm_clock_register(pdev->dev.of_node,
				cpu_clocks_8939_single_cluster,
				ARRAY_SIZE(cpu_clocks_8939_single_cluster));
	else
		rc = of_msm_clock_register(pdev->dev.of_node,
				cpu_clocks_8939, ARRAY_SIZE(cpu_clocks_8939));

	if (rc) {
		dev_err(&pdev->dev, "msm_clock_register failed\n");
		return rc;
	}

	if (!single_cluster) {
		rate = clk_get_rate(&cci_clk.c);
		clk_set_rate(&cci_clk.c, rate);
	}

	for (mux_id = 0; mux_id < mux_num; mux_id++) {
		/* Force a PLL reconfiguration */
		config_pll(mux_id);
	}

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
		if (!single_cluster)
			clk_prepare_enable(&cci_clk.c);
	}
	put_online_cpus();

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &a53_bc_clk.c)
			cpumask_set_cpu(cpu, &a53_bc_clk.cpumask);
		if (logical_cpu_to_clk(cpu) == &a53_lc_clk.c)
			cpumask_set_cpu(cpu, &a53_lc_clk.cpumask);
	}

	a53_lc_clk.hw_low_power_ctrl = true;
	a53_bc_clk.hw_low_power_ctrl = true;

	if (single_cluster)
		register_pm_notifier(&clock_8939_pm_notifier_single_cluster);
	else
		register_pm_notifier(&clock_8939_pm_notifier);

	populate_opp_table(pdev, single_cluster);

	atomic_notifier_chain_register(&panic_notifier_list,
						&clock_panic_notifier);

	return 0;
}

static const struct of_device_id clock_a53_match_table[] = {
	{.compatible = "qcom,cpu-clock-8939"},
	{.compatible = "qcom,cpu-clock-8917"},
	{}
};

static struct platform_driver clock_a53_driver = {
	.probe = clock_a53_probe,
	.driver = {
		.name = "cpu-clock-8939",
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
	bool single_cluster;
	int rc = 0;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
					"qcom,cpu-clock-8939");

	if (!ofnode)
		ofnode = of_find_compatible_node(NULL, NULL,
					"qcom,cpu-clock-gold");

	if (!ofnode)
		return 0;

	single_cluster = of_property_read_bool(ofnode,
					"qcom,num-cluster");

	rc = lpm_get_latency(&a53_bc_clk.latency_lvl,
			&a53_bc_clk.cpu_latency_no_l2_pc_us);
	if (rc < 0)
		pr_err("Failed to get the L2 PC value for perf\n");

	if (!single_cluster) {
		rc = lpm_get_latency(&a53_lc_clk.latency_lvl,
				&a53_lc_clk.cpu_latency_no_l2_pc_us);
		if (rc < 0)
			pr_err("Failed to get the L2 PC value for pwr\n");

		pr_debug("Latency for pwr/perf cluster %d : %d\n",
			a53_lc_clk.cpu_latency_no_l2_pc_us,
			a53_bc_clk.cpu_latency_no_l2_pc_us);
	} else {
		pr_debug("Latency for perf cluster %d\n",
			a53_bc_clk.cpu_latency_no_l2_pc_us);
	}

	return rc;
}
late_initcall(clock_cpu_lpm_get_latency);

#define APCS_C0_PLL			0xb116000
#define C0_PLL_MODE			0x0
#define C0_PLL_L_VAL			0x4
#define C0_PLL_M_VAL			0x8
#define C0_PLL_N_VAL			0xC
#define C0_PLL_USER_CTL			0x10
#define C0_PLL_CONFIG_CTL		0x14

#define APCS_ALIAS0_CMD_RCGR		0xb111050
#define APCS_ALIAS0_CFG_OFF		0x4
#define APCS_ALIAS0_CORE_CBCR_OFF	0x8
#define SRC_SEL				0x4
#define SRC_DIV				0x3

static void __init configure_enable_sr2_pll(void __iomem *base)
{
	/* Disable Mode */
	writel_relaxed(0x0, base + C0_PLL_MODE);

	/* Configure L/M/N values */
	writel_relaxed(0x34, base + C0_PLL_L_VAL);
	writel_relaxed(0x0,  base + C0_PLL_M_VAL);
	writel_relaxed(0x1,  base + C0_PLL_N_VAL);

	/* Configure USER_CTL and CONFIG_CTL value */
	writel_relaxed(0x0100000f, base + C0_PLL_USER_CTL);
	writel_relaxed(0x4c015765, base + C0_PLL_CONFIG_CTL);

	/* Enable PLL now */
	writel_relaxed(0x2, base + C0_PLL_MODE);
	udelay(2);
	writel_relaxed(0x6, base + C0_PLL_MODE);
	udelay(50);
	writel_relaxed(0x7, base + C0_PLL_MODE);
	/* Ensure that the writes go through before enabling
	 * PLL
	 */
	mb();
}

static int __init cpu_clock_a53_init_little(void)
{
	void __iomem  *base;
	int regval = 0, count;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
							"qcom,cpu-clock-8939");
	if (!ofnode)
		return 0;

	base = ioremap_nocache(APCS_C0_PLL, SZ_32);
	configure_enable_sr2_pll(base);
	iounmap(base);

	base = ioremap_nocache(APCS_ALIAS0_CMD_RCGR, SZ_8);
	regval = readl_relaxed(base);
	/* Source GPLL0 and 1/2 the rate of GPLL0 */
	regval = (SRC_SEL << 8) | SRC_DIV; /* 0x403 */
	writel_relaxed(regval, base + APCS_ALIAS0_CFG_OFF);
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

	/* Enable the branch */
	regval =  readl_relaxed(base + APCS_ALIAS0_CORE_CBCR_OFF);
	regval |= BIT(0);
	writel_relaxed(regval, base + APCS_ALIAS0_CORE_CBCR_OFF);
	/* Branch enable should be complete */
	mb();
	iounmap(base);

	pr_info("A53 Power clocks configured\n");

	return 0;
}
early_initcall(cpu_clock_a53_init_little);
