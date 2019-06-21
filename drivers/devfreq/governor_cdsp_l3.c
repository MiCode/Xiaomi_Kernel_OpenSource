// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "governor_cdspl3: " fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/soc/qcom/cdsprm.h>

#include "governor.h"

struct cdspl3 {
	struct device_node *of_node;
	struct devfreq *df;
	unsigned int l3_freq_hz;
};

static struct cdspl3 p_me;

static int cdsp_l3_request_callback(unsigned int freq_khz)
{
	if (p_me.df) {
		mutex_lock(&p_me.df->lock);
		p_me.l3_freq_hz = freq_khz * 1000;
		update_devfreq(p_me.df);
		mutex_unlock(&p_me.df->lock);
	} else {
		pr_err("CDSP L3 request for %dKHz not served\n", freq_khz);
		return -ENODEV;
	}
	return 0;
}

static struct cdsprm_l3 cdsprm = {
	.set_l3_freq = cdsp_l3_request_callback,
};

static int devfreq_get_target_freq(struct devfreq *df,
			unsigned long *freq)
{
	if (freq)
		*freq = (unsigned long)p_me.l3_freq_hz;
	return 0;
}

static int gov_start(struct devfreq *df)
{
	int ret = 0;

	if (p_me.of_node != df->dev.parent->of_node) {
		dev_err(df->dev.parent,
		"Device match error in CDSP L3 frequency governor\n");
		return -ENODEV;
	}

	p_me.df = df;
	p_me.l3_freq_hz = 0;
	/*
	 * Trigger an update to set the target frequency
	 */
	mutex_lock(&df->lock);
	ret = update_devfreq(df);
	mutex_unlock(&df->lock);
	/*
	 * Send governor start message to CDSP RM driver
	 */
	cdsprm_register_cdspl3gov(&cdsprm);

	return ret;
}

static int gov_stop(struct devfreq *df)
{
	p_me.df = 0;
	p_me.l3_freq_hz = 0;
	/*
	 * Send governor stop message to CDSP RM driver
	 */
	cdsprm_unregister_cdspl3gov();
	return 0;
}

static int devfreq_event_handler(struct devfreq *df,
			unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = gov_start(df);
		if (ret)
			return ret;
		dev_info(df->dev.parent,
			"Successfully started CDSP L3 governor\n");
		break;
	case DEVFREQ_GOV_STOP:
		dev_info(df->dev.parent,
			"Received stop CDSP L3 governor event\n");
		ret = gov_stop(df);
		if (ret)
			return ret;
		break;
	default:
		break;
	}
	return 0;
}

static struct devfreq_governor cdsp_l3_gov = {
	.name = "cdspl3",
	.get_target_freq = devfreq_get_target_freq,
	.event_handler = devfreq_event_handler,
};

static int cdsp_l3_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	p_me.of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!p_me.of_node) {
		dev_err(dev, "Couldn't find a target device\n");
		return -ENODEV;
	}
	ret = devfreq_add_governor(&cdsp_l3_gov);
	if (ret)
		dev_err(dev, "Failed registering CDSP L3 requests %d\n",
			ret);
	return ret;
}

static const struct of_device_id cdsp_l3_match_table[] = {
	{ .compatible = "qcom,cdsp-l3" },
	{}
};

static struct platform_driver cdsp_l3 = {
	.probe = cdsp_l3_driver_probe,
	.driver = {
		.name = "cdsp-l3",
		.of_match_table = cdsp_l3_match_table,
	}
};

static int __init cdsp_l3_gov_module_init(void)
{
	return platform_driver_register(&cdsp_l3);

}
module_init(cdsp_l3_gov_module_init);

static void __exit cdsp_l3_gov_module_exit(void)
{
	devfreq_remove_governor(&cdsp_l3_gov);
	platform_driver_unregister(&cdsp_l3);
}
module_exit(cdsp_l3_gov_module_exit);
MODULE_LICENSE("GPL v2");
