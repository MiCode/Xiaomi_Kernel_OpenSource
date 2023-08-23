/*
 * Copyright (c) 2013-2014, 2018-2019, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
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

static int devbw_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (!IS_ERR(opp))
		dev_pm_opp_put(opp);

	return set_bw(dev, *freq, d->gov_ab);
}

static int devbw_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct dev_data *d = dev_get_drvdata(dev);

	stat->private_data = &d->gov_ab;
	return 0;
}

#define PROP_OPERATING_POINTS_V2 "operating-points-v2"

static int add_opp_prop_from_child(struct device *dev,
				struct device_node *of_child)
{
	struct property *prop;
	int len = 0, ret = 0;
	void *value;
	const void *p_val;

	p_val = of_get_property(of_child, PROP_OPERATING_POINTS_V2, &len);
	if (!p_val)
		return -ENODEV;
	value = devm_kzalloc(dev, len, GFP_KERNEL);
	if (!value)
		return -ENOMEM;
	memcpy(value, p_val, len);
	prop = devm_kzalloc(dev, sizeof(*prop), GFP_KERNEL);
	if (!prop) {
		devm_kfree(dev, value);
		return -ENOMEM;
	}
	prop->name = "operating-points-v2";
	prop->value = value;
	prop->length = len;
	ret = of_add_property(dev->of_node, prop);
	if (ret) {
		devm_kfree(dev, value);
		devm_kfree(dev, prop);
		dev_err(dev, "failed to add property: %d\n", ret);
	}

	return ret;
}

static int parse_child_nodes_for_opp(struct device *dev)
{
	struct device_node *of_child;
	int ddr_type_of = -1;
	int ddr_type = of_fdt_get_ddrtype();
	int ret = -EINVAL;

	for_each_child_of_node(dev->of_node, of_child) {
		ret = of_property_read_u32(of_child, "qcom,ddr-type",
						&ddr_type_of);
		if (!ret && (ddr_type == ddr_type_of)) {
			dev_dbg(dev, "ddr-type = %d, is matching DT entry\n",
						ddr_type_of);
			ret = add_opp_prop_from_child(dev, of_child);
			if (ret)
				return ret;
			return dev_pm_opp_of_add_table(dev);
		}
		ret = -ENODEV;
	}
	return ret;
}

#define PROP_PORTS "qcom,src-dst-ports"
#define PROP_ACTIVE "qcom,active-only"

int devfreq_add_devbw(struct device *dev)
{
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	u32 ports[MAX_PATHS * 2];
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
	if (of_get_child_count(dev->of_node))
		ret = parse_child_nodes_for_opp(dev);
	else
		ret = dev_pm_opp_of_add_table(dev);

	if (ret)
		dev_err(dev, "Couldn't parse OPP table:%d\n", ret);

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

int devfreq_remove_devbw(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	msm_bus_scale_unregister_client(d->bus_client);
	devfreq_remove_device(d->df);
	return 0;
}

int devfreq_suspend_devbw(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	return devfreq_suspend_device(d->df);
}

int devfreq_resume_devbw(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	return devfreq_resume_device(d->df);
}

static int devfreq_devbw_probe(struct platform_device *pdev)
{
	return devfreq_add_devbw(&pdev->dev);
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
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(devbw_driver);
MODULE_DESCRIPTION("Device DDR bandwidth voting driver MSM SoCs");
MODULE_LICENSE("GPL v2");
