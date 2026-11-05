// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include "walt.h"

struct gclk_counter {
	u64 total_cycle_counter;
	u64 prev_cycle_counter;
	spinlock_t lock;
};

static struct gclk_counter walt_gclk_counter[MAX_CLUSTERS];

struct walt_ncc_data {
	void __iomem *base;
};

static struct walt_ncc_data ncc_data[MAX_CLUSTERS];

u64 walt_get_ncc_gclk_cycle_counter(int cpu, u64 wc)
{
	struct gclk_counter *ncc_counter;
	struct walt_ncc_data *data;
	u64 cycle_counter_ret;
	unsigned long flags;
	int index;
	u64 val;

	index = topology_cluster_id(cpu);

	data = &ncc_data[index];

	ncc_counter = &walt_gclk_counter[index];
	spin_lock_irqsave(&ncc_counter->lock, flags);

	val = readq_relaxed(data->base);

	if (val < ncc_counter->prev_cycle_counter) {
		/* Handle counter overflow.
		 * Most likely will not occur
		 * for 64 bit counter, but
		 * handling for completeness.
		 */
		ncc_counter->total_cycle_counter += U64_MAX -
			ncc_counter->prev_cycle_counter + val;
		ncc_counter->prev_cycle_counter = val;
	} else {
		ncc_counter->total_cycle_counter += val -
			ncc_counter->prev_cycle_counter;
		ncc_counter->prev_cycle_counter = val;
	}
	cycle_counter_ret = ncc_counter->total_cycle_counter;
	spin_unlock_irqrestore(&ncc_counter->lock, flags);

	pr_debug("CPU %u, cluster-id %d\n", cpu, index);

	return cycle_counter_ret;
}

static int walt_gclk_cycle_counter_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	void __iomem *base;
	int ret = -ENODEV, index;
	struct walt_sched_cluster *cluster;

	for_each_sched_cluster(cluster) {
		index = topology_cluster_id(cpumask_first(&cluster->cpus));
		res = platform_get_resource(pdev, IORESOURCE_MEM, index);
		if (!res) {
			dev_err(dev, "failed to get mem resource %d\n", index);
			return -ENODEV;
		}

		if (!devm_request_mem_region(dev, res->start, resource_size(res), res->name)) {
			dev_err(dev, "failed to request resource %pR\n", res);
			return -EBUSY;
		}

		base = devm_ioremap(dev, res->start, resource_size(res));
		if (!base) {
			dev_err(dev, "failed to map resource %pR\n", res);
			return -ENOMEM;
		}
		ncc_data[index].base = base;
	}

	if (!walt_get_cycle_counts_cb) {
		for (int i = 0; i < MAX_CLUSTERS; i++)
			spin_lock_init(&walt_gclk_counter[i].lock);
		walt_get_cycle_counts_cb = walt_get_ncc_gclk_cycle_counter;
		use_cycle_counter = true;
		complete(&walt_get_cycle_counts_cb_completion);
		return 0;
	}

	return ret;
}

static int walt_gclk_cycle_counter_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id walt_gclk_cycle_counter_match[] = {
		{ .compatible = "qcom,gclk" },
		{}
};

static struct platform_driver walt_gclk_cycle_counter_driver = {
	.driver = {
		.name = "walt-gclk-cycle-counter",
		.of_match_table = walt_gclk_cycle_counter_match
	},
	.probe = walt_gclk_cycle_counter_driver_probe,
	.remove = walt_gclk_cycle_counter_driver_remove,
};

int walt_gclk_cycle_counter_driver_register(void)
{
	return platform_driver_register(&walt_gclk_cycle_counter_driver);
}
