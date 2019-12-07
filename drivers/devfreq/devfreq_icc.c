// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2014, 2018, 2019, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
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

static int icc_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (!IS_ERR(opp))
		dev_pm_opp_put(opp);

	return set_bw(dev, *freq, d->gov_ab);
}

static int icc_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct dev_data *d = dev_get_drvdata(dev);

	stat->private_data = &d->gov_ab;
	return 0;
}

#define PROP_ACTIVE	"qcom,active-only"
#define ACTIVE_ONLY_TAG	0x3

int devfreq_add_icc(struct device *dev)
{
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	const char *gov_name;
	int ret;
	struct opp_table *opp_table;
	u32 version;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	dev_set_drvdata(dev, d);

	p = &d->dp;
	p->polling_ms = 50;
	p->target = icc_target;
	p->get_dev_status = icc_get_dev_status;

	if (of_device_is_compatible(dev->of_node, "qcom,devfreq-icc-ddr")) {
		version = (1 << of_fdt_get_ddrtype());
		opp_table = dev_pm_opp_set_supported_hw(dev, &version, 1);
		if (IS_ERR(opp_table)) {
			dev_err(dev, "Failed to set supported hardware\n");
			return PTR_ERR(opp_table);
		}
	}

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0)
		dev_err(dev, "Couldn't parse OPP table:%d\n", ret);

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
	{ .compatible = "qcom,devfreq-icc-llcc" },
	{ .compatible = "qcom,devfreq-icc-ddr" },
	{ .compatible = "qcom,devfreq-icc" },
	{}
};

static struct platform_driver devfreq_icc_driver = {
	.probe = devfreq_icc_probe,
	.remove = devfreq_icc_remove,
	.driver = {
		.name = "devfreq-icc",
		.of_match_table = devfreq_icc_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(devfreq_icc_driver);
MODULE_DESCRIPTION("Device DDR bandwidth voting driver MSM SoCs");
MODULE_LICENSE("GPL v2");
