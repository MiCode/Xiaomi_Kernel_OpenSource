// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/bitfield.h>
#include <linux/cpufreq.h>
#include <linux/energy_model.h>
#include <linux/init.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>

#define LUT_MAX_ENTRIES			32U
#define LUT_FREQ			GENMASK(11, 0)
#define LUT_ROW_SIZE			0x4
#define CPUFREQ_HW_STATUS		BIT(0)
#define POLL_USEC			1000
#define TIMEOUT_USEC			300000
#define DT_STRING_LEN			15

enum {
	REG_FREQ_LUT_TABLE,
	REG_FREQ_ENABLE,
	REG_FREQ_PERF_STATE,
	REG_FREQ_HW_STATE,
	REG_EM_POWER_TBL,

	REG_ARRAY_SIZE,
};

struct cpufreq_mtk {
	struct cpufreq_frequency_table *table;
	void __iomem *reg_bases[REG_ARRAY_SIZE];
	int nr_opp;
	cpumask_t related_cpus;
};

static const u16 cpufreq_mtk_offsets[REG_ARRAY_SIZE] = {
	[REG_FREQ_LUT_TABLE]		= 0x0,
	[REG_FREQ_ENABLE]		= 0x84,
	[REG_FREQ_PERF_STATE]		= 0x88,
	[REG_FREQ_HW_STATE]		= 0x8c,
	[REG_EM_POWER_TBL]		= 0x0,
};

static struct cpufreq_mtk *mtk_freq_domain_map[NR_CPUS];

static int mtk_cpufreq_get_cpu_power(unsigned long *power, unsigned long *KHz, int cpu)
{
	struct device *cpu_dev = get_cpu_device(cpu);
	struct cpufreq_mtk *c = mtk_freq_domain_map[cpu];
	unsigned long Hz;
	int i;

	if (!cpu_dev) {
		pr_info("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	/* Get the power cost of the performance domain. */
	Hz = *KHz * 1000;
	for (i = 0; i < c->nr_opp; i++) {
		if (c->table[i].frequency < Hz)
			break;
	}
	i--;
	*KHz = c->table[i].frequency / 1000;

	/* The EM framework specifies the frequency in KHz. */
	*power = readl_relaxed(c->reg_bases[REG_EM_POWER_TBL] + i * LUT_ROW_SIZE) / 1000;

	return 0;
}

static int mtk_cpufreq_hw_target_index(struct cpufreq_policy *policy,
				       unsigned int index)
{
	struct cpufreq_mtk *c = policy->driver_data;

	writel_relaxed(index, c->reg_bases[REG_FREQ_PERF_STATE]);
	arch_set_freq_scale(policy->related_cpus,
			    policy->freq_table[index].frequency,
			    policy->cpuinfo.max_freq);

	return 0;
}

static unsigned int mtk_cpufreq_hw_get(unsigned int cpu)
{
	struct cpufreq_mtk *c;
	struct cpufreq_policy *policy;
	unsigned int index;

	policy = cpufreq_cpu_get_raw(cpu);
	if (!policy)
		return 0;

	c = policy->driver_data;

	index = readl_relaxed(c->reg_bases[REG_FREQ_PERF_STATE]);
	index = min(index, LUT_MAX_ENTRIES - 1);

	return policy->freq_table[index].frequency;
}

static int mtk_cpufreq_hw_cpu_init(struct cpufreq_policy *policy)
{
	struct em_data_callback em_cb = EM_DATA_CB(mtk_cpufreq_get_cpu_power);
	struct cpufreq_mtk *c;
	struct device *cpu_dev;
	int ret, sig;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev)
		return -ENODEV;

	c = mtk_freq_domain_map[policy->cpu];
	if (!c)
		return -ENODEV;

	cpumask_copy(policy->cpus, &c->related_cpus);

	policy->freq_table = c->table;
	policy->driver_data = c;

	writel_relaxed(0x1, c->reg_bases[REG_FREQ_ENABLE]);

	/* HW should be in enabled state to proceed now */
	if (readl_poll_timeout((c->reg_bases[REG_FREQ_HW_STATE]),
	    sig, sig & CPUFREQ_HW_STATUS, POLL_USEC, TIMEOUT_USEC)) {
		pr_info("cpufreq hardware of CPU%d is not enabled\n",
			policy->cpu);
		return -ENODEV;
	}

	em_register_perf_domain(policy->cpus, c->nr_opp, &em_cb);

	return 0;
}

static struct freq_attr *mtk_cpufreq_hw_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL
};

static struct cpufreq_driver cpufreq_mtk_hw_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
			  CPUFREQ_HAVE_GOVERNOR_PER_POLICY | CPUFREQ_IS_COOLING_DEV,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= mtk_cpufreq_hw_target_index,
	.get		= mtk_cpufreq_hw_get,
	.init		= mtk_cpufreq_hw_cpu_init,
	.name		= "mtk-cpufreq-hw",
	.attr		= mtk_cpufreq_hw_attr,
};

static int mtk_cpufreq_hw_opp_create(struct platform_device *pdev,
				     struct cpufreq_mtk *c)
{
	struct device *dev = &pdev->dev;
	void __iomem *base_table;
	u32 data, i, freq, volt, prev_freq = 0;

	c->table = devm_kcalloc(dev, LUT_MAX_ENTRIES + 1,
				sizeof(*c->table), GFP_KERNEL);
	if (!c->table)
		return -ENOMEM;

	base_table = c->reg_bases[REG_FREQ_LUT_TABLE];

	for (i = 0; i < LUT_MAX_ENTRIES; i++) {
		data = readl_relaxed(base_table + (i * LUT_ROW_SIZE));
		freq = FIELD_GET(LUT_FREQ, data) * 1000;
		c->table[i].frequency = freq;

		if (freq == prev_freq)
			break;

		prev_freq = freq;
	}

	c->table[i].frequency = CPUFREQ_TABLE_END;
	c->nr_opp = i;

	return 0;
}

static int mtk_get_related_cpus(int index, struct cpumask *m)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np)
			continue;

		ret = of_parse_phandle_with_args(cpu_np, "mtk-freq-domain",
						 "#freq-domain-cells", 0,
						 &args);
		of_node_put(cpu_np);
		if (ret < 0)
			continue;

		if (index == args.args[0])
			cpumask_set_cpu(cpu, m);
	}

	return 0;
}

static int mtk_cpu_resources_init(struct platform_device *pdev,
				  unsigned int cpu, int index)
{
	struct cpufreq_mtk *c;
	struct resource *res;
	struct device *dev = &pdev->dev;
	const u16 *offsets;
	int ret, i, cpu_r, uindex;
	void __iomem *base, *ubase;
	char unode[DT_STRING_LEN];

	if (mtk_freq_domain_map[cpu])
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

	snprintf(unode, sizeof(unode), "%s%d", "em-domain", index);
	uindex = of_property_match_string(pdev->dev.of_node, "reg-names", unode);
	if (IS_ERR(uindex))
		return PTR_ERR(uindex);

	res = platform_get_resource(pdev, IORESOURCE_MEM, uindex);
	ubase = devm_ioremap_resource(dev, res);
	if (IS_ERR(ubase))
		return PTR_ERR(ubase);

	for (i = REG_FREQ_LUT_TABLE; i < REG_ARRAY_SIZE; i++) {
		if (i >= REG_EM_POWER_TBL)
			c->reg_bases[i] = ubase + offsets[i];
		else
			c->reg_bases[i] = base + offsets[i];
	}

	ret = mtk_get_related_cpus(index, &c->related_cpus);
	if (ret)
		return ret;

	ret = mtk_cpufreq_hw_opp_create(pdev, c);
	if (ret)
		return ret;

	for_each_cpu(cpu_r, &c->related_cpus)
		mtk_freq_domain_map[cpu_r] = c;

	return 0;
}

static int mtk_resources_init(struct platform_device *pdev)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	unsigned int cpu;
	int ret;

	for_each_possible_cpu(cpu) {
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np)
			continue;

		ret = of_parse_phandle_with_args(cpu_np, "mtk-freq-domain",
				"#freq-domain-cells", 0, &args);
		if (ret < 0)
			return ret;

		ret = mtk_cpu_resources_init(pdev, cpu, args.args[0]);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_cpufreq_hw_driver_probe(struct platform_device *pdev)
{
	int ret;

	/* Get the bases of cpufreq for domains */
	ret = mtk_resources_init(pdev);
	if (ret)
		return ret;

	ret = cpufreq_register_driver(&cpufreq_mtk_hw_driver);
	if (ret)
		return ret;

	pr_info("Mediatek CPUFreq HW driver initialized\n");
	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	return 0;
}

static const struct of_device_id mtk_cpufreq_hw_match[] = {
	{ .compatible = "mediatek,cpufreq-hw", .data = &cpufreq_mtk_offsets },
	{}
};

static struct platform_driver mtk_cpufreq_hw_driver = {
	.probe = mtk_cpufreq_hw_driver_probe,
	.driver = {
		.name = "mtk-cpufreq-hw",
		.of_match_table = mtk_cpufreq_hw_match,
	},
};

static int __init mtk_cpufreq_hw_init(void)
{
	return platform_driver_register(&mtk_cpufreq_hw_driver);
}
subsys_initcall(mtk_cpufreq_hw_init);

MODULE_DESCRIPTION("mtk CPUFREQ HW Driver");
MODULE_LICENSE("GPL v2");
