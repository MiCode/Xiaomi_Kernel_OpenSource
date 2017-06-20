/*
 * Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "devfreq-simple-dev: " fmt

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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/devfreq.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <trace/events/power.h>

struct dev_data {
	struct clk *clk;
	struct devfreq *df;
	struct devfreq_dev_profile profile;
	bool freq_in_khz;
};

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

static int dev_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);
	unsigned long rfreq;

	find_freq(&d->profile, freq, flags);

	rfreq = clk_round_rate(d->clk, d->freq_in_khz ? *freq * 1000 : *freq);
	if (IS_ERR_VALUE(rfreq)) {
		dev_err(dev, "devfreq: Cannot find matching frequency for %lu\n",
			*freq);
		return rfreq;
	}

	return clk_set_rate(d->clk, rfreq);
}

static int dev_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct dev_data *d = dev_get_drvdata(dev);
	unsigned long f;

	f = clk_get_rate(d->clk);
	if (IS_ERR_VALUE(f))
		return f;
	*freq = d->freq_in_khz ? f / 1000 : f;
	return 0;
}

#define PROP_TBL "freq-tbl-khz"
static int parse_freq_table(struct device *dev, struct dev_data *d)
{
	struct devfreq_dev_profile *p = &d->profile;
	int ret, len, i, j;
	u32 *data;
	unsigned long f;

	if (!of_find_property(dev->of_node, PROP_TBL, &len)) {
		if (dev_pm_opp_get_opp_count(dev) <= 0)
			return -EPROBE_DEFER;
		return 0;
	}

	d->freq_in_khz = true;
	len /= sizeof(*data);
	data = devm_kzalloc(dev, len * sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	p->freq_table = devm_kzalloc(dev, len * sizeof(*p->freq_table),
				     GFP_KERNEL);
	if (!p->freq_table)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, PROP_TBL, data, len);
	if (ret)
		return ret;

	j = 0;
	for (i = 0; i < len; i++) {
		f = clk_round_rate(d->clk, data[i] * 1000);
		if (IS_ERR_VALUE(f))
			dev_warn(dev, "Unable to find dev rate for %d KHz",
				 data[i]);
		else
			p->freq_table[j++] = f / 1000;
	}
	p->max_state = j;
	devm_kfree(dev, data);

	if (p->max_state == 0) {
		dev_err(dev, "Error parsing property %s!\n", PROP_TBL);
		return -EINVAL;
	}

	return 0;
}

static int devfreq_clock_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	u32 poll;
	const char *gov_name;
	int ret;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	platform_set_drvdata(pdev, d);

	d->clk = devm_clk_get(dev, "devfreq_clk");
	if (IS_ERR(d->clk))
		return PTR_ERR(d->clk);

	ret = parse_freq_table(dev, d);
	if (ret)
		return ret;

	p = &d->profile;
	p->target = dev_target;
	p->get_cur_freq = dev_get_cur_freq;
	ret = dev_get_cur_freq(dev, &p->initial_freq);
	if (ret)
		return ret;

	p->polling_ms = 50;
	if (!of_property_read_u32(dev->of_node, "polling-ms", &poll))
		p->polling_ms = poll;

	if (of_property_read_string(dev->of_node, "governor", &gov_name))
		gov_name = "performance";

	if (of_property_read_bool(dev->of_node, "qcom,prepare-clk")) {
		ret = clk_prepare(d->clk);
		if (ret)
			return ret;
	}

	d->df = devfreq_add_device(dev, p, gov_name, NULL);
	if (IS_ERR(d->df)) {
		ret = PTR_ERR(d->df);
		goto add_err;
	}

	return 0;
add_err:
	if (of_property_read_bool(dev->of_node, "qcom,prepare-clk"))
		clk_unprepare(d->clk);
	return ret;
}

static int devfreq_clock_remove(struct platform_device *pdev)
{
	struct dev_data *d = platform_get_drvdata(pdev);

	devfreq_remove_device(d->df);

	return 0;
}

static const struct of_device_id devfreq_simple_match_table[] = {
	{ .compatible = "devfreq-simple-dev" },
	{}
};

static struct platform_driver devfreq_clock_driver = {
	.probe = devfreq_clock_probe,
	.remove = devfreq_clock_remove,
	.driver = {
		.name = "devfreq-simple-dev",
		.of_match_table = devfreq_simple_match_table,
	},
};
module_platform_driver(devfreq_clock_driver);
MODULE_DESCRIPTION("Devfreq driver for setting generic device clock frequency");
MODULE_LICENSE("GPL v2");
