// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#define LUT_MAX_ENTRIES			40U
#define CORE_COUNT_VAL(val)		(((val) & (GENMASK(18, 16))) >> 16)
#define LUT_ROW_SIZE			4
#define CLK_HW_DIV			2

enum {
	REG_ENABLE,
	REG_LUT_TABLE,
	REG_PERF_STATE,

	REG_ARRAY_SIZE,
};

struct cpufreq_qcom {
	struct cpufreq_frequency_table *table;
	void __iomem *reg_bases[REG_ARRAY_SIZE];
	cpumask_t related_cpus;
	unsigned int max_cores;
	unsigned long xo_rate;
	unsigned long cpu_hw_rate;
};

static const u16 cpufreq_qcom_std_offsets[REG_ARRAY_SIZE] = {
	[REG_ENABLE]		= 0x0,
	[REG_LUT_TABLE]		= 0x100,
	[REG_PERF_STATE]	= 0x320,
};

static struct cpufreq_qcom *qcom_freq_domain_map[NR_CPUS];

static int
qcom_cpufreq_hw_target_index(struct cpufreq_policy *policy,
			     unsigned int index)
{
	struct cpufreq_qcom *c = policy->driver_data;

	writel_relaxed(index, c->reg_bases[REG_PERF_STATE]);

	return 0;
}

static unsigned int qcom_cpufreq_hw_get(unsigned int cpu)
{
	struct cpufreq_qcom *c;
	struct cpufreq_policy *policy;
	unsigned int index;

	policy = cpufreq_cpu_get_raw(cpu);
	if (!policy)
		return 0;

	c = policy->driver_data;

	index = readl_relaxed(c->reg_bases[REG_PERF_STATE]);
	index = min(index, LUT_MAX_ENTRIES - 1);

	return policy->freq_table[index].frequency;
}

static unsigned int
qcom_cpufreq_hw_fast_switch(struct cpufreq_policy *policy,
			    unsigned int target_freq)
{
	struct cpufreq_qcom *c = policy->driver_data;
	int index;

	index = policy->cached_resolved_idx;
	if (index < 0)
		return 0;

	writel_relaxed(index, c->reg_bases[REG_PERF_STATE]);

	return policy->freq_table[index].frequency;
}

static int qcom_cpufreq_hw_cpu_init(struct cpufreq_policy *policy)
{
	struct cpufreq_qcom *c;

	c = qcom_freq_domain_map[policy->cpu];
	if (!c) {
		pr_err("No scaling support for CPU%d\n", policy->cpu);
		return -ENODEV;
	}

	cpumask_copy(policy->cpus, &c->related_cpus);

	policy->fast_switch_possible = true;
	policy->freq_table = c->table;
	policy->driver_data = c;

	return 0;
}

static struct freq_attr *qcom_cpufreq_hw_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	&cpufreq_freq_attr_scaling_boost_freqs,
	NULL
};

static struct cpufreq_driver cpufreq_qcom_hw_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
			  CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= qcom_cpufreq_hw_target_index,
	.get		= qcom_cpufreq_hw_get,
	.init		= qcom_cpufreq_hw_cpu_init,
	.fast_switch    = qcom_cpufreq_hw_fast_switch,
	.name		= "qcom-cpufreq-hw",
	.attr		= qcom_cpufreq_hw_attr,
	.boost_enabled	= true,
};

static int qcom_cpufreq_hw_read_lut(struct platform_device *pdev,
				    struct cpufreq_qcom *c)
{
	struct device *dev = &pdev->dev;
	void __iomem *base;
	u32 data, src, lval, i, core_count, prev_cc, prev_freq, cur_freq;

	c->table = devm_kcalloc(dev, LUT_MAX_ENTRIES + 1,
				sizeof(*c->table), GFP_KERNEL);
	if (!c->table)
		return -ENOMEM;

	base = c->reg_bases[REG_LUT_TABLE];

	for (i = 0; i < LUT_MAX_ENTRIES; i++) {
		data = readl_relaxed(base + i * LUT_ROW_SIZE);
		src = (data & GENMASK(31, 30)) >> 30;
		lval = data & GENMASK(7, 0);
		core_count = CORE_COUNT_VAL(data);

		if (src)
			c->table[i].frequency = c->xo_rate * lval / 1000;
		else
			c->table[i].frequency = c->cpu_hw_rate / 1000;

		cur_freq = c->table[i].frequency;

		dev_dbg(dev, "index=%d freq=%d, core_count %d\n",
			i, c->table[i].frequency, core_count);

		if (core_count != c->max_cores)
			cur_freq = CPUFREQ_ENTRY_INVALID;

		/*
		 * Two of the same frequencies with the same core counts means
		 * end of table.
		 */
		if (i > 0 && c->table[i - 1].frequency ==
		   c->table[i].frequency && prev_cc == core_count) {
			struct cpufreq_frequency_table *prev = &c->table[i - 1];

			if (prev_freq == CPUFREQ_ENTRY_INVALID)
				prev->flags = CPUFREQ_BOOST_FREQ;
			break;
		}
		prev_cc = core_count;
		prev_freq = cur_freq;
	}

	c->table[i].frequency = CPUFREQ_TABLE_END;

	return 0;
}

static int qcom_get_related_cpus(int index, struct cpumask *m)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np)
			continue;

		ret = of_parse_phandle_with_args(cpu_np, "qcom,freq-domain",
				"#freq-domain-cells", 0, &args);
		of_node_put(cpu_np);
		if (ret < 0)
			continue;

		if (index == args.args[0])
			cpumask_set_cpu(cpu, m);
	}

	return 0;
}

static int qcom_cpu_resources_init(struct platform_device *pdev,
				   unsigned int cpu, int index,
				   unsigned int max_cores,
				   unsigned long xo_rate,
				   unsigned long cpu_hw_rate)
{
	struct cpufreq_qcom *c;
	struct resource *res;
	struct device *dev = &pdev->dev;
	const u16 *offsets;
	int ret, i, cpu_r;
	void __iomem *base;

	if (qcom_freq_domain_map[cpu])
		return 0;

	c = devm_kzalloc(dev, sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	offsets = of_device_get_match_data(&pdev->dev);
	if (!offsets)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	for (i = REG_ENABLE; i < REG_ARRAY_SIZE; i++)
		c->reg_bases[i] = base + offsets[i];

	/* HW should be in enabled state to proceed */
	if (!(readl_relaxed(c->reg_bases[REG_ENABLE]) & 0x1)) {
		dev_err(dev, "Domain-%d cpufreq hardware not enabled\n", index);
		return -ENODEV;
	}

	ret = qcom_get_related_cpus(index, &c->related_cpus);
	if (ret) {
		dev_err(dev, "Domain-%d failed to get related CPUs\n", index);
		return ret;
	}

	c->max_cores = max_cores;
	if (!c->max_cores)
		return -ENOENT;

	c->xo_rate = xo_rate;
	c->cpu_hw_rate = cpu_hw_rate;

	ret = qcom_cpufreq_hw_read_lut(pdev, c);
	if (ret) {
		dev_err(dev, "Domain-%d failed to read LUT\n", index);
		return ret;
	}

	for_each_cpu(cpu_r, &c->related_cpus)
		qcom_freq_domain_map[cpu_r] = c;

	return 0;
}

static int qcom_resources_init(struct platform_device *pdev)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	struct clk *clk;
	unsigned int cpu;
	unsigned long xo_rate, cpu_hw_rate;
	int ret;

	clk = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	xo_rate = clk_get_rate(clk);

	devm_clk_put(&pdev->dev, clk);

	clk = devm_clk_get(&pdev->dev, "cpu_clk");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	cpu_hw_rate = clk_get_rate(clk) / CLK_HW_DIV;

	devm_clk_put(&pdev->dev, clk);

	for_each_possible_cpu(cpu) {
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np) {
			dev_dbg(&pdev->dev, "Failed to get cpu %d device\n",
				cpu);
			continue;
		}

		ret = of_parse_phandle_with_args(cpu_np, "qcom,freq-domain",
				"#freq-domain-cells", 0, &args);
		if (ret < 0)
			return ret;

		ret = qcom_cpu_resources_init(pdev, cpu, args.args[0],
					      args.args[1], xo_rate,
					      cpu_hw_rate);
		if (ret)
			return ret;
	}

	return 0;
}

static int qcom_cpufreq_hw_driver_probe(struct platform_device *pdev)
{
	int rc;

	/* Get the bases of cpufreq for domains */
	rc = qcom_resources_init(pdev);
	if (rc) {
		dev_err(&pdev->dev, "CPUFreq resource init failed\n");
		return rc;
	}

	rc = cpufreq_register_driver(&cpufreq_qcom_hw_driver);
	if (rc) {
		dev_err(&pdev->dev, "CPUFreq HW driver failed to register\n");
		return rc;
	}

	dev_dbg(&pdev->dev, "QCOM CPUFreq HW driver initialized\n");

	return 0;
}

static const struct of_device_id qcom_cpufreq_hw_match[] = {
	{ .compatible = "qcom,cpufreq-hw", .data = &cpufreq_qcom_std_offsets },
	{}
};

static struct platform_driver qcom_cpufreq_hw_driver = {
	.probe = qcom_cpufreq_hw_driver_probe,
	.driver = {
		.name = "qcom-cpufreq-hw",
		.of_match_table = qcom_cpufreq_hw_match,
	},
};

static int __init qcom_cpufreq_hw_init(void)
{
	return platform_driver_register(&qcom_cpufreq_hw_driver);
}
subsys_initcall(qcom_cpufreq_hw_init);

MODULE_DESCRIPTION("QCOM firmware-based CPU Frequency driver");
