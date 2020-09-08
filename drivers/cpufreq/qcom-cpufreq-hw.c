// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/energy_model.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/interconnect.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define CREATE_TRACE_POINTS
#include <trace/events/dcvsh.h>

#define LUT_MAX_ENTRIES			40U
#define LUT_SRC				GENMASK(31, 30)
#define LUT_L_VAL			GENMASK(7, 0)
#define LUT_CORE_COUNT			GENMASK(18, 16)
#define LUT_VOLT			GENMASK(11, 0)
#define LUT_ROW_SIZE			32
#define CLK_HW_DIV			2
#define EQ_IRQ_STATUS			BIT(0)
#define LT_IRQ_STATUS			BIT(1)
#define MAX_FN_SIZE			12
#define LIMITS_POLLING_DELAY_MS		10

#define CYCLE_CNTR_OFFSET(c, m, acc_count)				\
			(acc_count ? ((c - cpumask_first(m) + 1) * 4) : 0)

enum {
	REG_ENABLE,
	REG_FREQ_LUT,
	REG_VOLT_LUT,
	REG_PERF_STATE,
	REG_CYCLE_CNTR,
	REG_LLM_DCVS_VC_VOTE,
	REG_INTR_EN,
	REG_INTR_CLR,
	REG_INTR_STATUS,

	REG_ARRAY_SIZE,
};

static unsigned long cpu_hw_rate, xo_rate;
static const u16 *offsets;
static unsigned int lut_row_size = LUT_ROW_SIZE;
static unsigned int lut_max_entries = LUT_MAX_ENTRIES;
static bool accumulative_counter;
static struct platform_device *global_pdev;
static bool icc_scaling_enabled;

static int qcom_cpufreq_set_bw(struct cpufreq_policy *policy,
			       unsigned long freq_khz)
{
	unsigned long freq_hz = freq_khz * 1000;
	struct dev_pm_opp *opp;
	struct device *dev;
	int ret;

	dev = get_cpu_device(policy->cpu);
	if (!dev)
		return -ENODEV;

	opp = dev_pm_opp_find_freq_exact(dev, freq_hz, true);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	ret = dev_pm_opp_set_bw(dev, opp);
	dev_pm_opp_put(opp);
	return ret;
}

static int qcom_cpufreq_update_opp(struct device *cpu_dev,
				   unsigned long freq_khz,
				   unsigned long volt)
{
	unsigned long freq_hz = freq_khz * 1000;
	int ret;

	/* Skip voltage update if the opp table is not available */
	if (!icc_scaling_enabled)
		return dev_pm_opp_add(cpu_dev, freq_hz, volt);

	ret = dev_pm_opp_adjust_voltage(cpu_dev, freq_hz, volt, volt, volt);
	if (ret) {
		dev_err(cpu_dev, "Voltage update failed freq=%ld\n", freq_khz);
		return ret;
	}

	return dev_pm_opp_enable(cpu_dev, freq_hz);
}

struct cpufreq_qcom {
	struct cpufreq_frequency_table *table;
	void __iomem *base;
	cpumask_t related_cpus;
	unsigned long dcvsh_freq_limit;
	struct delayed_work freq_poll_work;
	struct mutex dcvsh_lock;
	struct device_attribute freq_limit_attr;
	int dcvsh_irq;
	char dcvsh_irq_name[MAX_FN_SIZE];
	bool is_irq_enabled;
	bool is_irq_requested;
};

struct cpufreq_counter {
	u64 total_cycle_counter;
	u32 prev_cycle_counter;
	spinlock_t lock;
};

static const u16 cpufreq_qcom_std_offsets[REG_ARRAY_SIZE] = {
	[REG_ENABLE]		= 0x0,
	[REG_FREQ_LUT]		= 0x110,
	[REG_VOLT_LUT]		= 0x114,
	[REG_PERF_STATE]	= 0x920,
	[REG_CYCLE_CNTR]	= 0x9c0,
};

static const u16 cpufreq_qcom_epss_std_offsets[REG_ARRAY_SIZE] = {
	[REG_ENABLE]		= 0x0,
	[REG_FREQ_LUT]		= 0x100,
	[REG_VOLT_LUT]		= 0x200,
	[REG_PERF_STATE]	= 0x320,
	[REG_CYCLE_CNTR]	= 0x3c4,
	[REG_LLM_DCVS_VC_VOTE]	= 0x024,
	[REG_INTR_EN]		= 0x304,
	[REG_INTR_CLR]		= 0x308,
	[REG_INTR_STATUS]	= 0x30C,
};

static struct cpufreq_qcom *qcom_freq_domain_map[NR_CPUS];
static struct cpufreq_counter qcom_cpufreq_counter[NR_CPUS];

static ssize_t dcvsh_freq_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpufreq_qcom *c = container_of(attr, struct cpufreq_qcom,
						freq_limit_attr);
	return scnprintf(buf, PAGE_SIZE, "%lu\n", c->dcvsh_freq_limit);
}

static unsigned long limits_mitigation_notify(struct cpufreq_qcom *c)
{
	int i;
	u32 max_vc;

	max_vc = readl_relaxed(c->base + offsets[REG_LLM_DCVS_VC_VOTE]) &
						GENMASK(13, 8);

	for (i = 0; i < lut_max_entries; i++) {
		if (c->table[i].driver_data != max_vc)
			continue;
		else {
			sched_update_cpu_freq_min_max(&c->related_cpus, 0,
					c->table[i].frequency);
			trace_dcvsh_freq(cpumask_first(&c->related_cpus),
						c->table[i].frequency);
			c->dcvsh_freq_limit = c->table[i].frequency;
			return c->table[i].frequency;
		}
	}

	return 0;
}

static void limits_dcvsh_poll(struct work_struct *work)
{
	struct cpufreq_qcom *c = container_of(work, struct cpufreq_qcom,
						freq_poll_work.work);
	struct cpufreq_policy *policy;
	unsigned long freq_limit;
	u32 regval, cpu;

	mutex_lock(&c->dcvsh_lock);

	cpu = cpumask_first(&c->related_cpus);
	policy = cpufreq_cpu_get_raw(cpu);

	freq_limit = limits_mitigation_notify(c);
	if (freq_limit != policy->cpuinfo.max_freq || !freq_limit) {
		mod_delayed_work(system_highpri_wq, &c->freq_poll_work,
				msecs_to_jiffies(LIMITS_POLLING_DELAY_MS));
	} else {
		regval = readl_relaxed(c->base + offsets[REG_INTR_CLR]);
		regval &= ~LT_IRQ_STATUS;
		writel_relaxed(regval, c->base + offsets[REG_INTR_CLR]);

		c->is_irq_enabled = true;
		enable_irq(c->dcvsh_irq);
	}

	mutex_unlock(&c->dcvsh_lock);
}

static irqreturn_t dcvsh_handle_isr(int irq, void *data)
{
	struct cpufreq_qcom *c = data;
	u32 regval;

	regval = readl_relaxed(c->base + offsets[REG_INTR_STATUS]);
	if (!(regval & LT_IRQ_STATUS))
		return IRQ_HANDLED;

	mutex_lock(&c->dcvsh_lock);

	if (c->is_irq_enabled) {
		c->is_irq_enabled = false;
		disable_irq_nosync(c->dcvsh_irq);
		limits_mitigation_notify(c);
		mod_delayed_work(system_highpri_wq, &c->freq_poll_work,
				msecs_to_jiffies(LIMITS_POLLING_DELAY_MS));

	}

	mutex_unlock(&c->dcvsh_lock);

	return IRQ_HANDLED;
}

static u64 qcom_cpufreq_get_cpu_cycle_counter(int cpu)
{
	struct cpufreq_counter *cpu_counter;
	struct cpufreq_policy *policy;
	u64 cycle_counter_ret;
	unsigned long flags;
	u16 offset;
	u32 val;

	policy = cpufreq_cpu_get_raw(cpu);
	if (!policy)
		return 0;

	cpu_counter = &qcom_cpufreq_counter[cpu];
	spin_lock_irqsave(&cpu_counter->lock, flags);

	offset = CYCLE_CNTR_OFFSET(cpu, policy->related_cpus,
					accumulative_counter);
	val = readl_relaxed_no_log(policy->driver_data +
				    offsets[REG_CYCLE_CNTR] + offset);

	if (val < cpu_counter->prev_cycle_counter) {
		/* Handle counter overflow */
		cpu_counter->total_cycle_counter += UINT_MAX -
			cpu_counter->prev_cycle_counter + val;
		cpu_counter->prev_cycle_counter = val;
	} else {
		cpu_counter->total_cycle_counter += val -
			cpu_counter->prev_cycle_counter;
		cpu_counter->prev_cycle_counter = val;
	}
	cycle_counter_ret = cpu_counter->total_cycle_counter;
	spin_unlock_irqrestore(&cpu_counter->lock, flags);

	return cycle_counter_ret;
}

static int
qcom_cpufreq_hw_target_index(struct cpufreq_policy *policy,
			     unsigned int index)
{
	unsigned long freq = policy->freq_table[index].frequency;

	writel_relaxed(index, policy->driver_data + offsets[REG_PERF_STATE]);

	if (icc_scaling_enabled)
		qcom_cpufreq_set_bw(policy, freq);

	arch_set_freq_scale(policy->related_cpus, freq,
			    policy->cpuinfo.max_freq);

	return 0;
}

static unsigned int qcom_cpufreq_hw_get(unsigned int cpu)
{
	struct cpufreq_policy *policy;
	unsigned int index;

	policy = cpufreq_cpu_get_raw(cpu);
	if (!policy)
		return 0;

	index = readl_relaxed(policy->driver_data + offsets[REG_PERF_STATE]);
	index = min(index, lut_max_entries - 1);

	return policy->freq_table[index].frequency;
}

static unsigned int
qcom_cpufreq_hw_fast_switch(struct cpufreq_policy *policy,
			    unsigned int target_freq)
{
	void __iomem *perf_state_reg = policy->driver_data;
	unsigned int index;
	unsigned long freq;

	index = policy->cached_resolved_idx;

	if (qcom_cpufreq_hw_target_index(policy, index))
		return 0;

	return policy->freq_table[index].frequency;
}

static int qcom_cpufreq_hw_cpu_init(struct cpufreq_policy *policy)
{
	struct cpufreq_qcom *c;
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	c = qcom_freq_domain_map[policy->cpu];
	if (!c) {
		pr_err("No scaling support for CPU%d\n", policy->cpu);
		return -ENODEV;
	}

	cpumask_copy(policy->cpus, &c->related_cpus);

	ret = dev_pm_opp_get_opp_count(cpu_dev);
	if (ret <= 0)
		dev_err(cpu_dev, "OPP table is not ready\n");

	policy->freq_table = c->table;
	policy->driver_data = c->base;
	policy->fast_switch_possible = true;
	policy->dvfs_possible_from_any_cpu = true;

	dev_pm_opp_of_register_em(policy->cpus);

	if (c->dcvsh_irq > 0 && !c->is_irq_requested) {
		snprintf(c->dcvsh_irq_name, sizeof(c->dcvsh_irq_name),
					"dcvsh-irq-%d", policy->cpu);
		ret = devm_request_threaded_irq(cpu_dev, c->dcvsh_irq, NULL,
			dcvsh_handle_isr, IRQF_TRIGGER_HIGH | IRQF_ONESHOT |
			IRQF_NO_SUSPEND | IRQF_SHARED, c->dcvsh_irq_name, c);
		if (ret) {
			dev_err(cpu_dev, "Failed to register irq %d\n", ret);
			return ret;
		}

		c->is_irq_requested = true;
		c->is_irq_enabled = true;
		writel_relaxed(LT_IRQ_STATUS, c->base + offsets[REG_INTR_EN]);
		c->freq_limit_attr.attr.name = "dcvsh_freq_limit";
		c->freq_limit_attr.show = dcvsh_freq_limit_show;
		c->freq_limit_attr.attr.mode = 0444;
		c->dcvsh_freq_limit = U32_MAX;
		device_create_file(cpu_dev, &c->freq_limit_attr);
	}

	return 0;
}

static struct freq_attr *qcom_cpufreq_hw_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	&cpufreq_freq_attr_scaling_boost_freqs,
	NULL
};

static void qcom_cpufreq_ready(struct cpufreq_policy *policy)
{
	static struct thermal_cooling_device *cdev[NR_CPUS];
	struct device_node *np;
	unsigned int cpu = policy->cpu;

	if (cdev[cpu])
		return;

	np = of_cpu_device_node_get(cpu);
	if (WARN_ON(!np))
		return;

	/*
	 * For now, just loading the cooling device;
	 * thermal DT code takes care of matching them.
	 */
	if (of_find_property(np, "#cooling-cells", NULL)) {
		cdev[cpu] = of_cpufreq_cooling_register(policy);
		if (IS_ERR(cdev[cpu])) {
			pr_err("running cpufreq for CPU%d without cooling dev: %ld\n",
			       cpu, PTR_ERR(cdev[cpu]));
			cdev[cpu] = NULL;
		}
	}

	of_node_put(np);
}

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
	.ready		= qcom_cpufreq_ready,
};

static int qcom_cpufreq_hw_read_lut(struct platform_device *pdev,
				    struct cpufreq_qcom *c, u32 max_cores)
{
	struct device *dev = &pdev->dev;
	u32 data, src, lval, i, core_count, prev_cc, prev_freq, freq, volt;
	unsigned long cpu;
	struct cpufreq_frequency_table	*table;
	struct dev_pm_opp *opp;
	unsigned long rate;
	int ret;

	c->table = devm_kcalloc(dev, lut_max_entries + 1,
				sizeof(*c->table), GFP_KERNEL);
	if (!c->table)
		return -ENOMEM;

	cpu = cpumask_first(&c->related_cpus);

	for (i = 0; i < lut_max_entries; i++) {
		data = readl_relaxed(c->base + offsets[REG_FREQ_LUT] +
				      i * lut_row_size);

	ret = dev_pm_opp_of_add_table(cpu_dev);
	if (!ret) {
		/* Disable all opps and cross-validate against LUT later */
		icc_scaling_enabled = true;
		for (rate = 0; ; rate++) {
			opp = dev_pm_opp_find_freq_ceil(cpu_dev, &rate);
			if (IS_ERR(opp))
				break;

			dev_pm_opp_put(opp);
			dev_pm_opp_disable(cpu_dev, rate);
		}
	} else if (ret != -ENODEV) {
		dev_err(cpu_dev, "Invalid opp table in device tree\n");
		return ret;
	} else {
		policy->fast_switch_possible = true;
		icc_scaling_enabled = false;
	}

		src = FIELD_GET(LUT_SRC, data);
		lval = FIELD_GET(LUT_L_VAL, data);
		core_count = FIELD_GET(LUT_CORE_COUNT, data);

		data = readl_relaxed(c->base + offsets[REG_VOLT_LUT] +
				      i * lut_row_size);
		volt = FIELD_GET(LUT_VOLT, data) * 1000;

		if (src)
			freq = xo_rate * lval / 1000;
		else
			freq = cpu_hw_rate / 1000;

		if (freq != prev_freq && core_count == max_cores) {
			c->table[i].frequency = freq;
			qcom_cpufreq_update_opp(cpu_dev, freq, volt);
			dev_dbg(dev, "index=%d freq=%d, core_count %d\n",
				i, c->table[i].frequency, core_count);
		} else {
			c->table[i].frequency = CPUFREQ_ENTRY_INVALID;
		}

		/*
		 * Two of the same frequencies with the same core counts means
		 * end of table.
		 */
		if (i > 0 && prev_freq == freq && prev_cc == core_count) {
			struct cpufreq_frequency_table *prev = &c->table[i - 1];

			if (prev_cc != max_cores) {
				prev->frequency = prev_freq;
				prev->flags = CPUFREQ_BOOST_FREQ;
				qcom_cpufreq_update_opp(cpu_dev, prev_freq, volt);
			}

			break;
		}

		prev_cc = core_count;
		prev_freq = freq;

		freq *= 1000;
	}

	c->table[i].frequency = CPUFREQ_TABLE_END;
	dev_pm_opp_set_sharing_cpus(get_cpu_device(cpu), &c->related_cpus);

	return 0;
}

static void qcom_get_related_cpus(int index, struct cpumask *m)
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
}

static int qcom_cpu_resources_init(struct platform_device *pdev,
				   unsigned int cpu, int index,
				   unsigned int max_cores)
{
	struct cpufreq_qcom *c;
	struct resource *res;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	int ret, cpu_r;

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

	if (!of_property_read_bool(dev->of_node, "qcom,skip-enable-check")) {
		/* HW should be in enabled state to proceed */
		if (!(readl_relaxed(base + offsets[REG_ENABLE]) & 0x1)) {
			dev_err(dev, "Domain-%d cpufreq hardware not enabled\n",
				 index);
			return -ENODEV;
		}
	}

	accumulative_counter = !of_property_read_bool(dev->of_node,
						"qcom,no-accumulative-counter");
	c->base = base;

	qcom_get_related_cpus(index, &c->related_cpus);
	if (!cpumask_weight(&c->related_cpus)) {
		dev_err(dev, "Domain-%d failed to get related CPUs\n", index);
		return -ENONET;
	}

	ret = qcom_cpufreq_hw_read_lut(pdev, c, max_cores);
	if (ret) {
		dev_err(dev, "Domain-%d failed to read LUT\n", index);
		return ret;
	}

	if (of_find_property(dev->of_node, "interrupts", NULL)) {
		c->dcvsh_irq = of_irq_get(dev->of_node, index);
		if (c->dcvsh_irq > 0) {
			mutex_init(&c->dcvsh_lock);
			INIT_DEFERRABLE_WORK(&c->freq_poll_work,
					limits_dcvsh_poll);
		}
	}

	for_each_cpu(cpu_r, &c->related_cpus)
		qcom_freq_domain_map[cpu_r] = c;

	dev_pm_opp_of_register_em(cpu_dev, policy->cpus);

	return 0;
}

static int qcom_resources_init(struct platform_device *pdev)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	struct clk *clk;
	unsigned int cpu;
	int ret;

	clk = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	xo_rate = clk_get_rate(clk);
	clk_put(clk);

	clk = devm_clk_get(&pdev->dev, "alternate");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	cpu_hw_rate = clk_get_rate(clk) / CLK_HW_DIV;
	clk_put(clk);

	of_property_read_u32(pdev->dev.of_node, "qcom,lut-row-size",
			      &lut_row_size);

	of_property_read_u32(pdev->dev.of_node, "qcom,lut-max-entries",
			      &lut_max_entries);

	for_each_possible_cpu(cpu) {
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np) {
			dev_dbg(&pdev->dev, "Failed to get cpu %d device\n",
				cpu);
			continue;
		}

		ret = of_parse_phandle_with_args(cpu_np, "qcom,freq-domain",
						  "#freq-domain-cells", 0,
						   &args);
		of_node_put(cpu_np);
		if (ret)
			return ret;

		if (qcom_freq_domain_map[cpu])
			continue;

		ret = qcom_cpu_resources_init(pdev, cpu, args.args[0],
					      args.args[1]);
		if (ret)
			return ret;
	}

	return 0;
}

static int qcom_cpufreq_hw_driver_probe(struct platform_device *pdev)
{
	struct cpu_cycle_counter_cb cycle_counter_cb = {
		.get_cpu_cycle_counter = qcom_cpufreq_get_cpu_cycle_counter,
	};
	int rc, cpu;

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

	for_each_possible_cpu(cpu)
		spin_lock_init(&qcom_cpufreq_counter[cpu].lock);

	rc = register_cpu_cycle_counter_cb(&cycle_counter_cb);
	if (rc) {
		dev_err(&pdev->dev, "cycle counter cb failed to register\n");
		return rc;
	}

	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	dev_dbg(&pdev->dev, "QCOM CPUFreq HW driver initialized\n");

	return 0;
}

static int qcom_cpufreq_hw_driver_remove(struct platform_device *pdev)
{
	return cpufreq_unregister_driver(&cpufreq_qcom_hw_driver);
}

static const struct of_device_id qcom_cpufreq_hw_match[] = {
	{ .compatible = "qcom,cpufreq-hw", .data = &cpufreq_qcom_std_offsets },
	{ .compatible = "qcom,cpufreq-hw-epss",
				   .data = &cpufreq_qcom_epss_std_offsets },
	{}
};

static struct platform_driver qcom_cpufreq_hw_driver = {
	.probe = qcom_cpufreq_hw_driver_probe,
	.remove = qcom_cpufreq_hw_driver_remove,
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

static void __exit qcom_cpufreq_hw_exit(void)
{
	platform_driver_unregister(&qcom_cpufreq_hw_driver);
}
module_exit(qcom_cpufreq_hw_exit);

MODULE_DESCRIPTION("QCOM CPUFREQ HW Driver");
MODULE_LICENSE("GPL v2");
