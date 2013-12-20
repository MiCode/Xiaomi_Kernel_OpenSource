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

#define pr_fmt(fmt) "cpubw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/devfreq.h>
#include <linux/of.h>
#include <trace/events/power.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

/* Has to be ULL to prevent overflow where this macro is used. */
#define MBYTE (1ULL << 20)
#define MAX_PATHS	2

static struct msm_bus_vectors vectors[MAX_PATHS * 2];
static struct msm_bus_paths bw_levels[] = {
	{ .vectors = &vectors[0] },
	{ .vectors = &vectors[MAX_PATHS] },
};
static struct msm_bus_scale_pdata bw_data = {
	.usecase = bw_levels,
	.num_usecases = ARRAY_SIZE(bw_levels),
	.name = "devfreq_cpubw",
	.active_only = 1,
};
static int num_paths;
static u32 bus_client;

static int set_bw(int new_ib, int new_ab)
{
	static int cur_idx, cur_ab, cur_ib;
	int i, ret;

	if (cur_ib == new_ib && cur_ab == new_ab)
		return 0;

	i = (cur_idx + 1) % ARRAY_SIZE(bw_levels);

	bw_levels[i].vectors[0].ib = new_ib * MBYTE;
	bw_levels[i].vectors[0].ab = new_ab / num_paths * MBYTE;
	bw_levels[i].vectors[1].ib = new_ib * MBYTE;
	bw_levels[i].vectors[1].ab = new_ab / num_paths * MBYTE;

	pr_debug("BW MBps: AB: %d IB: %d\n", new_ab, new_ib);

	ret = msm_bus_scale_client_update_request(bus_client, i);
	if (ret) {
		pr_err("bandwidth request failed (%d)\n", ret);
	} else {
		cur_idx = i;
		cur_ib = new_ib;
		cur_ab = new_ab;
	}

	return ret;
}

static void find_freq(struct devfreq_dev_profile *p, unsigned long *freq,
			u32 flags)
{
	int i;
	unsigned long atmost, atleast, f;

	atmost = p->freq_table[0];
	atleast = p->freq_table[p->max_state-1];
	for (i = 0; i < p->max_state; i++) {
		f = p->freq_table[i];
		if (f <= *freq)
			atmost = max(f, atmost);
		if (f >= *freq)
			atleast = min(f, atleast);
	}

	if (flags & DEVFREQ_FLAG_LEAST_UPPER_BOUND)
		*freq = atmost;
	else
		*freq = atleast;
}

struct devfreq_dev_profile cpubw_profile;
static long gov_ab;

int cpubw_target(struct device *dev, unsigned long *freq, u32 flags)
{
	find_freq(&cpubw_profile, freq, flags);
	return set_bw(*freq, gov_ab);
}

static struct devfreq_governor_data gov_data[] = {
	{ .name = "performance" },
	{ .name = "powersave" },
	{ .name = "userspace" },
};
struct devfreq_dev_profile cpubw_profile = {
	.polling_ms = 50,
	.target = cpubw_target,
	.governor_data = gov_data,
	.num_governor_data = ARRAY_SIZE(gov_data),
};

#define PROP_PORTS "qcom,cpu-mem-ports"
#define PROP_TBL "qcom,bw-tbl"

static int __init cpubw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct devfreq_dev_profile *p = &cpubw_profile;
	struct devfreq *df;
	u32 *data, ports[MAX_PATHS * 2];
	int ret, len, i;

	if (of_find_property(dev->of_node, PROP_PORTS, &len)) {
		len /= sizeof(ports[0]);
		if (len % 2 || len > ARRAY_SIZE(ports)) {
			dev_err(dev, "Unexpected number of ports\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(dev->of_node, PROP_PORTS,
						 ports, len);
		if (ret)
			return ret;

		num_paths = len / 2;
	} else {
		return -EINVAL;
	}

	for (i = 0; i < num_paths; i++) {
		bw_levels[0].vectors[i].src = ports[2 * i];
		bw_levels[0].vectors[i].dst = ports[2 * i + 1];
		bw_levels[1].vectors[i].src = ports[2 * i];
		bw_levels[1].vectors[i].dst = ports[2 * i + 1];
	}
	bw_levels[0].num_paths = num_paths;
	bw_levels[1].num_paths = num_paths;

	if (of_find_property(dev->of_node, PROP_TBL, &len)) {
		len /= sizeof(*data);
		data = devm_kzalloc(dev, len * sizeof(*data), GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		p->freq_table = devm_kzalloc(dev,
					     len * sizeof(*p->freq_table),
					     GFP_KERNEL);
		if (!p->freq_table)
			return -ENOMEM;

		ret = of_property_read_u32_array(dev->of_node, PROP_TBL,
						 data, len);
		if (ret)
			return ret;

		for (i = 0; i < len; i++)
			p->freq_table[i] = data[i];
		p->max_state = len;
	}

	bus_client = msm_bus_scale_register_client(&bw_data);
	if (!bus_client) {
		dev_err(dev, "Unable to register bus client\n");
		return -ENODEV;
	}

	df = devfreq_add_device(dev, &cpubw_profile, "powersave", NULL);
	if (IS_ERR(df)) {
		msm_bus_scale_unregister_client(bus_client);
		return PTR_ERR(df);
	}

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpubw" },
	{}
};

static struct platform_driver cpubw_driver = {
	.driver = {
		.name = "cpubw",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init cpubw_init(void)
{
	platform_driver_probe(&cpubw_driver, cpubw_probe);
	return 0;
}
device_initcall(cpubw_init);

MODULE_DESCRIPTION("CPU DDR bandwidth voting driver MSM CPUs");
MODULE_LICENSE("GPL v2");
