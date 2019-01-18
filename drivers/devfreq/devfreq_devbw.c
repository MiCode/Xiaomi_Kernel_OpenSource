// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2014, 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "devbw: " fmt

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
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>

/* Has to be ULL to prevent overflow where this macro is used. */
#define MBYTE (1ULL << 20)
#define MAX_PATHS	2
#define DBL_BUF		2

struct dev_data {
	struct msm_bus_vectors vectors[MAX_PATHS * DBL_BUF];
	struct msm_bus_paths bw_levels[DBL_BUF];
	struct msm_bus_scale_pdata bw_data;
	int num_paths;
	u32 bus_client;
	int cur_idx;
	int cur_ab;
	int cur_ib;
	long gov_ab;
	struct devfreq *df;
	struct devfreq_dev_profile dp;
};

static int set_bw(struct device *dev, int new_ib, int new_ab)
{
	struct dev_data *d = dev_get_drvdata(dev);
	int i, ret;

	if (d->cur_ib == new_ib && d->cur_ab == new_ab)
		return 0;

	i = (d->cur_idx + 1) % DBL_BUF;

	d->bw_levels[i].vectors[0].ib = new_ib * MBYTE;
	d->bw_levels[i].vectors[0].ab = new_ab / d->num_paths * MBYTE;
	d->bw_levels[i].vectors[1].ib = new_ib * MBYTE;
	d->bw_levels[i].vectors[1].ab = new_ab / d->num_paths * MBYTE;

	dev_dbg(dev, "BW MBps: AB: %d IB: %d\n", new_ab, new_ib);

	ret = msm_bus_scale_client_update_request(d->bus_client, i);
	if (ret) {
		dev_err(dev, "bandwidth request failed (%d)\n", ret);
	} else {
		d->cur_idx = i;
		d->cur_ib = new_ib;
		d->cur_ab = new_ab;
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

static int devbw_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);

	find_freq(&d->dp, freq, flags);
	return set_bw(dev, *freq, d->gov_ab);
}

static int devbw_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct dev_data *d = dev_get_drvdata(dev);

	stat->private_data = &d->gov_ab;
	return 0;
}

#define PROP_PORTS "qcom,src-dst-ports"
#define PROP_TBL "qcom,bw-tbl"
#define PROP_ACTIVE "qcom,active-only"

int devfreq_add_devbw(struct device *dev)
{
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	u32 *data, ports[MAX_PATHS * 2];
	const char *gov_name;
	int ret, len, i, num_paths;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	dev_set_drvdata(dev, d);

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

	d->bw_levels[0].vectors = &d->vectors[0];
	d->bw_levels[1].vectors = &d->vectors[MAX_PATHS];
	d->bw_data.usecase = d->bw_levels;
	d->bw_data.num_usecases = ARRAY_SIZE(d->bw_levels);
	d->bw_data.name = dev_name(dev);
	d->bw_data.active_only = of_property_read_bool(dev->of_node,
							PROP_ACTIVE);

	for (i = 0; i < num_paths; i++) {
		d->bw_levels[0].vectors[i].src = ports[2 * i];
		d->bw_levels[0].vectors[i].dst = ports[2 * i + 1];
		d->bw_levels[1].vectors[i].src = ports[2 * i];
		d->bw_levels[1].vectors[i].dst = ports[2 * i + 1];
	}
	d->bw_levels[0].num_paths = num_paths;
	d->bw_levels[1].num_paths = num_paths;
	d->num_paths = num_paths;

	p = &d->dp;
	p->polling_ms = 50;
	p->target = devbw_target;
	p->get_dev_status = devbw_get_dev_status;

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

	d->bus_client = msm_bus_scale_register_client(&d->bw_data);
	if (!d->bus_client) {
		dev_err(dev, "Unable to register bus client\n");
		return -ENODEV;
	}

	if (of_property_read_string(dev->of_node, "governor", &gov_name))
		gov_name = "performance";

	d->df = devfreq_add_device(dev, p, gov_name, NULL);
	if (IS_ERR(d->df)) {
		msm_bus_scale_unregister_client(d->bus_client);
		return PTR_ERR(d->df);
	}

	return 0;
}

static int devfreq_devbw_probe(struct platform_device *pdev)
{
	return devfreq_add_devbw(&pdev->dev);
}

int devfreq_remove_devbw(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	msm_bus_scale_unregister_client(d->bus_client);
	devfreq_remove_device(d->df);
	return 0;
}

static int devfreq_devbw_remove(struct platform_device *pdev)
{
	return devfreq_remove_devbw(&pdev->dev);
}

static const struct of_device_id devbw_match_table[] = {
	{ .compatible = "qcom,devbw" },
	{}
};

static struct platform_driver devbw_driver = {
	.probe = devfreq_devbw_probe,
	.remove = devfreq_devbw_remove,
	.driver = {
		.name = "devbw",
		.of_match_table = devbw_match_table,
	},
};

module_platform_driver(devbw_driver);
MODULE_DESCRIPTION("Device DDR bandwidth voting driver MSM SoCs");
MODULE_LICENSE("GPL v2");
