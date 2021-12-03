// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2014-2015 ARM Limited
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/devfreq.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/thermal.h>

#define DEVFREQ_VDD_CDEV_DRIVER "devfreq-vdd-cdev"

struct devfreq_vdd_cdev {
	char dev_name[THERMAL_NAME_LENGTH];
	struct thermal_cooling_device *cdev;
	struct devfreq *devfreq;
	unsigned long cur_state;
	u32 *freq_table;
	size_t freq_table_size;
};

static int partition_enable_opps(struct devfreq_vdd_cdev *dfc,
				 unsigned long cdev_state)
{
	int i;
	struct device *dev = dfc->devfreq->dev.parent;

	for (i = 0; i < dfc->freq_table_size; i++) {
		struct dev_pm_opp *opp;
		int ret = 0;
		unsigned int freq = dfc->freq_table[i];
		bool want_enable = i <= cdev_state;

		opp = dev_pm_opp_find_freq_exact(dev, freq, !want_enable);

		if (PTR_ERR(opp) == -ERANGE)
			continue;
		else if (IS_ERR(opp))
			return PTR_ERR(opp);

		dev_pm_opp_put(opp);

		if (want_enable)
			ret = dev_pm_opp_enable(dev, freq);
		else
			ret = dev_pm_opp_disable(dev, freq);

		if (ret)
			return ret;
	}

	return 0;
}

static int devfreq_vdd_cdev_get_max_state(struct thermal_cooling_device *cdev,
					 unsigned long *state)
{
	struct devfreq_vdd_cdev *dfc = cdev->devdata;

	*state = dfc->freq_table_size - 1;

	return 0;
}

static int devfreq_vdd_cdev_get_min_state(struct thermal_cooling_device *cdev,
					 unsigned long *state)
{
	struct devfreq_vdd_cdev *dfc = cdev->devdata;

	*state = dfc->cur_state;

	return 0;
}

static int devfreq_vdd_cdev_set_min_state(struct thermal_cooling_device *cdev,
					 unsigned long state)
{
	struct devfreq_vdd_cdev *dfc = cdev->devdata;
	struct devfreq *df = dfc->devfreq;
	struct device *dev = df->dev.parent;
	int ret;

	if (state == dfc->cur_state)
		return 0;

	dev_dbg(dev, "Setting cooling min state %lu\n", state);

	if (state >= dfc->freq_table_size)
		return -EINVAL;

	/* Request Max - state for min state */
	ret = partition_enable_opps(dfc,
			dfc->freq_table_size - state - 1);
	if (ret)
		return ret;

	dfc->cur_state = state;

	return 0;
}

static struct thermal_cooling_device_ops devfreq_vdd_cdev_ops = {
	.get_max_state = devfreq_vdd_cdev_get_max_state,
	.get_cur_state = devfreq_vdd_cdev_get_min_state,
	.set_cur_state = devfreq_vdd_cdev_set_min_state,
};

static int devfreq_vdd_cdev_gen_tables(struct platform_device *pdev,
			struct devfreq_vdd_cdev *dfc)
{
	struct devfreq *df = dfc->devfreq;
	struct device *dev = df->dev.parent;
	int ret, num_opps;
	unsigned long freq;
	u32 *freq_table;
	int i;

	num_opps = dev_pm_opp_get_opp_count(dev);

	freq_table = devm_kcalloc(&pdev->dev, num_opps, sizeof(*freq_table),
			     GFP_KERNEL);
	if (!freq_table) {
		ret = -ENOMEM;
		return ret;
	}

	for (i = 0, freq = ULONG_MAX; i < num_opps; i++, freq--) {
		struct dev_pm_opp *opp;

		opp = dev_pm_opp_find_freq_floor(dev, &freq);
		if (IS_ERR(opp)) {
			ret = PTR_ERR(opp);
			goto free_tables;
		}
		dev_pm_opp_put(opp);
		freq_table[i] = freq;
	}

	dfc->freq_table = freq_table;
	dfc->freq_table_size = num_opps;

	return 0;

free_tables:
	kfree(freq_table);

	return ret;
}

static int devfreq_vdd_cdev_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_vdd_cdev *dfc = NULL;
	struct device_node *np = pdev->dev.of_node;

	dfc = devm_kzalloc(&pdev->dev, sizeof(*dfc), GFP_KERNEL);
	if (!dfc)
		return -ENOMEM;

	dfc->devfreq = devfreq_get_devfreq_by_phandle(&pdev->dev, 0);
	if (IS_ERR_OR_NULL(dfc->devfreq)) {
		ret = PTR_ERR(dfc->devfreq);
		dev_err(&pdev->dev,
			"Failed to get devfreq for min state cdev (%d)\n",
			ret);

		return ret;
	}

	ret = devfreq_vdd_cdev_gen_tables(pdev, dfc);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to get creat table for min state cdev (%d)\n",
				ret);
		return ret;
	}

	strlcpy(dfc->dev_name, np->name, THERMAL_NAME_LENGTH);
	dfc->cdev = thermal_of_cooling_device_register(np, dfc->dev_name, dfc,
						  &devfreq_vdd_cdev_ops);
	if (IS_ERR(dfc->cdev)) {
		ret = PTR_ERR(dfc->cdev);
		dev_err(&pdev->dev,
			"Failed to register devfreq cooling device (%d)\n",
			ret);
		dfc->cdev = NULL;
		return ret;
	}
	dev_set_drvdata(&pdev->dev, dfc);

	return 0;
}

static int devfreq_vdd_cdev_remove(struct platform_device *pdev)
{
	struct devfreq_vdd_cdev *dfc =
		(struct devfreq_vdd_cdev *)dev_get_drvdata(&pdev->dev);
	if (dfc->cdev)
		thermal_cooling_device_unregister(dfc->cdev);

	return 0;
};

static const struct of_device_id devfreq_vdd_cdev_match[] = {
	{ .compatible = "qcom,devfreq-vdd-cooling-device", },
	{},
};

static struct platform_driver devfreq_vdd_cdev_driver = {
	.probe		= devfreq_vdd_cdev_probe,
	.remove         = devfreq_vdd_cdev_remove,
	.driver		= {
		.name = DEVFREQ_VDD_CDEV_DRIVER,
		.of_match_table = devfreq_vdd_cdev_match,
	},
};

module_platform_driver(devfreq_vdd_cdev_driver);
MODULE_LICENSE("GPL v2");
