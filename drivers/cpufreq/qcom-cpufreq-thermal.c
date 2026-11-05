// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct qcom_cpufreq_thermal_domain {
	struct mbox_client cl;
	struct mbox_chan *ch;
	struct cpufreq_policy *policy;

	struct device_attribute freq_limit_attr;
	unsigned long freq_limit;
};

struct qcom_cpufreq_thermal {
	struct qcom_cpufreq_thermal_domain *domains;
	int num_domains;
};

static struct qcom_cpufreq_thermal qcom_cpufreq_thermal;

static inline
struct qcom_cpufreq_thermal_domain *to_qcom_cpufreq_thermal_domain(struct mbox_client *cl)
{
	return container_of(cl, struct qcom_cpufreq_thermal_domain, cl);
}

static void qcom_cpufreq_thermal_rx(struct mbox_client *cl, void *msg)
{
	struct qcom_cpufreq_thermal_domain *domain = to_qcom_cpufreq_thermal_domain(cl);
	unsigned int cpu = cpumask_first(domain->policy->related_cpus);
	unsigned long throttled_freq = *((unsigned long *)msg);

	dev_dbg(cl->dev, "cpu%u thermal limit: %lu\n", cpu, throttled_freq);

	domain->freq_limit = throttled_freq;

	arch_update_thermal_pressure(domain->policy->related_cpus, throttled_freq);
}

static ssize_t dcvsh_freq_limit_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct qcom_cpufreq_thermal_domain *domain;

	domain = container_of(attr, struct qcom_cpufreq_thermal_domain, freq_limit_attr);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", domain->freq_limit);
}

static int qcom_cpufreq_thermal_driver_probe(struct platform_device *pdev)
{
	struct qcom_cpufreq_thermal *data = &qcom_cpufreq_thermal;
	struct qcom_cpufreq_thermal_domain *domain;
	struct device *dev = &pdev->dev;
	struct device *cpu_dev;
	struct device_node *np = dev->of_node;
	int ret, cpu, i;

	data->num_domains = of_property_count_u32_elems(np, "qcom,policy-cpus");
	if (data->num_domains < 0) {
		dev_err(dev, "Error getting number of policies: %d\n", data->num_domains);
		return data->num_domains;
	}

	data->domains = devm_kzalloc(dev, sizeof(*domain) * data->num_domains, GFP_KERNEL);
	if (!data->domains)
		return -ENOMEM;

	for (i = 0; i < data->num_domains; i++) {
		domain = &data->domains[i];

		ret = of_property_read_u32_index(np, "qcom,policy-cpus", i, &cpu);
		if (ret) {
			dev_err(dev, "Error getting policy%d CPU: %d\n", i, ret);
			goto err;
		}

		domain->policy = cpufreq_cpu_get(cpu);
		if (!domain->policy) {
			dev_dbg(dev, "Error getting policy for CPU%d\n", i);
			ret = -EPROBE_DEFER;
			goto err;
		}

		domain->cl.dev = dev;
		domain->cl.rx_callback = qcom_cpufreq_thermal_rx;

		domain->ch = mbox_request_channel(&domain->cl, i);
		if (IS_ERR(domain->ch)) {
			ret = PTR_ERR(domain->ch);
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Error getting mailbox %d: %d\n", i, ret);
			goto err;
		}

		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			dev_err(dev, "Error getting CPU%d device\n", i);
			ret = -EINVAL;
			goto err;
		}

		sysfs_attr_init(&domain->freq_limit_attr.attr);
		domain->freq_limit_attr.attr.name = "dcvsh_freq_limit";
		domain->freq_limit_attr.show = dcvsh_freq_limit_show;
		domain->freq_limit_attr.attr.mode = 0444;
		domain->freq_limit = U32_MAX;
		device_create_file(cpu_dev, &domain->freq_limit_attr);
	}

	dev_info(dev, "Probe successful\n");
	return 0;

err:
	for (i = 0; i < data->num_domains; i++) {
		domain = &data->domains[i];
		if (domain->policy)
			cpufreq_cpu_put(domain->policy);
		if (!IS_ERR_OR_NULL(domain->ch))
			mbox_free_channel(domain->ch);
	}

	return ret;
}

static void qcom_cpufreq_thermal_driver_remove(struct platform_device *pdev)
{
	struct qcom_cpufreq_thermal *data = &qcom_cpufreq_thermal;
	struct qcom_cpufreq_thermal_domain *domain;
	struct device *cpu_dev;
	int i;

	for (i = 0; i < data->num_domains; i++) {
		domain = &data->domains[i];

		mbox_free_channel(domain->ch);
		cpu_dev = get_cpu_device(domain->policy->cpu);
		device_remove_file(cpu_dev, &domain->freq_limit_attr);
		cpufreq_cpu_put(domain->policy);
	}
}

static const struct of_device_id qcom_cpufreq_thermal_match[] = {
	{ .compatible = "qcom,cpufreq-thermal" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_cpufreq_thermal_match);

static struct platform_driver qcom_cpufreq_thermal_driver = {
	.probe = qcom_cpufreq_thermal_driver_probe,
	.remove_new = qcom_cpufreq_thermal_driver_remove,
	.driver = {
		.name = "qcom-cpufreq-thermal",
		.of_match_table = qcom_cpufreq_thermal_match,
	},
};

static int __init qcom_cpufreq_thermal_init(void)
{
	return platform_driver_register(&qcom_cpufreq_thermal_driver);
}
postcore_initcall(qcom_cpufreq_thermal_init);

MODULE_DESCRIPTION("QCOM CPUFREQ Thermal Driver");
MODULE_LICENSE("GPL");
