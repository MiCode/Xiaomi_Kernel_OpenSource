// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2014, 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "devfreq-icc: " fmt

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
#include <linux/platform_device.h>
#include <linux/interconnect.h>
#include <soc/qcom/devfreq_icc.h>

/* Has to be ULL to prevent overflow where this macro is used. */
#define MBYTE (1ULL << 20)

struct dev_data {
	struct icc_path			*icc_path;
	u32				cur_ab;
	u32				cur_ib;
	unsigned long			gov_ab;
	struct devfreq			*df;
	struct devfreq_dev_profile	dp;
};

static int set_bw(struct device *dev, u32 new_ib, u32 new_ab)
{
	struct dev_data *d = dev_get_drvdata(dev);
	int ret;

	if (d->cur_ib == new_ib && d->cur_ab == new_ab)
		return 0;

	dev_dbg(dev, "BW MBps: AB: %d IB: %d\n", new_ab, new_ib);

	ret = icc_set_bw(d->icc_path, Bps_to_icc(new_ab * MBYTE),
				Bps_to_icc(new_ib * MBYTE));
	if (ret < 0) {
		dev_err(dev, "icc set bandwidth request failed (%d)\n", ret);
	} else {
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

static int icc_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);

	find_freq(&d->dp, freq, flags);
	return set_bw(dev, *freq, d->gov_ab);
}

static int icc_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct dev_data *d = dev_get_drvdata(dev);

	stat->private_data = &d->gov_ab;
	return 0;
}

#define PROP_TBL	"qcom,bw-tbl"
#define PROP_ACTIVE	"qcom,active-only"
#define ACTIVE_ONLY_TAG	0x3

int devfreq_add_icc(struct device *dev)
{
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	u32 *data;
	const char *gov_name;
	int ret, len, i;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	dev_set_drvdata(dev, d);

	p = &d->dp;
	p->polling_ms = 50;
	p->target = icc_target;
	p->get_dev_status = icc_get_dev_status;

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
		if (ret < 0)
			return ret;

		for (i = 0; i < len; i++)
			p->freq_table[i] = data[i];
		p->max_state = len;
	}

	d->icc_path = of_icc_get(dev, NULL);
	if (IS_ERR(d->icc_path)) {
		ret = PTR_ERR(d->icc_path);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Unable to register icc path: %d\n", ret);
		return ret;
	}

	if (of_property_read_bool(dev->of_node, PROP_ACTIVE))
		icc_set_tag(d->icc_path, ACTIVE_ONLY_TAG);

	if (of_property_read_string(dev->of_node, "governor", &gov_name))
		gov_name = "performance";

	d->df = devfreq_add_device(dev, p, gov_name, NULL);
	if (IS_ERR(d->df)) {
		icc_put(d->icc_path);
		return PTR_ERR(d->df);
	}

	return 0;
}

int devfreq_remove_icc(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	icc_put(d->icc_path);
	devfreq_remove_device(d->df);
	return 0;
}

int devfreq_suspend_icc(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	return devfreq_suspend_device(d->df);
}

int devfreq_resume_icc(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);

	return devfreq_resume_device(d->df);
}

static int devfreq_icc_probe(struct platform_device *pdev)
{
	return devfreq_add_icc(&pdev->dev);
}

static int devfreq_icc_remove(struct platform_device *pdev)
{
	return devfreq_remove_icc(&pdev->dev);
}

static const struct of_device_id devfreq_icc_match_table[] = {
	{ .compatible = "qcom,devfreq-icc" },
	{}
};

static struct platform_driver devfreq_icc_driver = {
	.probe = devfreq_icc_probe,
	.remove = devfreq_icc_remove,
	.driver = {
		.name = "devfreq-icc",
		.of_match_table = devfreq_icc_match_table,
	},
};

module_platform_driver(devfreq_icc_driver);
MODULE_DESCRIPTION("Device DDR bandwidth voting driver MSM SoCs");
MODULE_LICENSE("GPL v2");
