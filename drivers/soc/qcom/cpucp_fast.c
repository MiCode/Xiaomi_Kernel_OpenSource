// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched/walt.h>

struct qcom_cpucp_fast {
	uint8_t last_cpu;
	cpumask_t fast_cpus;
	struct mbox_client cl;
	struct mbox_chan *ch;
};

#define FAST_MBOX_CPUMASK 0xFF

static struct qcom_cpucp_fast qcom_cpucp_fast;

static inline
struct qcom_cpucp_fast *to_qcom_cpucp_fast_info(struct mbox_client *cl)
{
	return container_of(cl, struct qcom_cpucp_fast, cl);
}

static void qcom_cpucp_fast_rx(struct mbox_client *cl, void *msg)
{
	struct qcom_cpucp_fast *data = &qcom_cpucp_fast;
	uint64_t mbox_rx_data = *((uint64_t *)msg);
	uint32_t cpu = mbox_rx_data & FAST_MBOX_CPUMASK;

	if (!cpumask_weight(&data->fast_cpus)) {
		dev_dbg(cl->dev, "No CPUS are enabled for FAST\n");
		return;
	}

	if (cpumask_test_cpu(cpu, &data->fast_cpus) ||
			((cpu == FAST_MBOX_CPUMASK) && (data->last_cpu < nr_cpu_ids))) {
		data->last_cpu = cpu;
		sched_walt_oscillate(cpu);
	}
}

static int qcom_cpucp_fast_probe(struct platform_device *pdev)
{
	struct qcom_cpucp_fast *data = &qcom_cpucp_fast;
	struct device *dev = &pdev->dev;
	struct cpufreq_policy *policy = NULL;
	int ret, cpu;

	data->cl.dev = dev;
	data->cl.rx_callback = qcom_cpucp_fast_rx;

	data->ch = mbox_request_channel(&data->cl, 0);
	if (IS_ERR(data->ch)) {
		ret = PTR_ERR(data->ch);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "Error getting mailbox %d\n", ret);
			goto err_ch;
		}
	}

	ret = of_property_read_u32(dev->of_node, "qcom,policy-cpus", &cpu);
	if (ret) {
		dev_err(dev, "Error getting policy%d CPU: %d\n", cpu, ret);
		goto err;
	}

	if (cpu >= nr_cpu_ids || !cpu_present(cpu)) {
		dev_err(dev, "Invalid CPU%d\n", cpu);
		goto err;
	}

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		dev_err(dev, "No policy for CPU:%d. Defer.\n", cpu);
		ret = -EPROBE_DEFER;
		goto err;
	}

	cpumask_copy(&data->fast_cpus, policy->related_cpus);
	cpufreq_cpu_put(policy);

	dev_info(dev, "Probe successful, FAST cpus=0x%lx\n", cpumask_bits(&data->fast_cpus)[0]);
	return 0;

err:
	mbox_free_channel(data->ch);
err_ch:
	return ret;
}

static int qcom_cpucp_fast_remove(struct platform_device *pdev)
{
	struct qcom_cpucp_fast *data = &qcom_cpucp_fast;

	mbox_free_channel(data->ch);

	return 0;
};

static const struct of_device_id qcom_cpucp_fast_match[] = {
	{ .compatible = "qcom,cpucp_fast" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_cpucp_fast_match);

static struct platform_driver qcom_cpucp_fast_driver = {
	.probe = qcom_cpucp_fast_probe,
	.remove = qcom_cpucp_fast_remove,
	.driver = {
		.name = "qcom-cpucp-fast",
		.of_match_table = qcom_cpucp_fast_match,
	},
};
module_platform_driver(qcom_cpucp_fast_driver);

MODULE_DESCRIPTION("QCOM CPUCP FAST Driver");
MODULE_LICENSE("GPL");
