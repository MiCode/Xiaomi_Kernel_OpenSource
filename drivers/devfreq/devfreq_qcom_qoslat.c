// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "devfreq-qcom-qoslat: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>
#include <linux/of.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>

struct qoslat_data {
	struct mbox_client		mbox_cl;
	struct mbox_chan		*mbox;
	struct devfreq			*df;
	struct devfreq_dev_profile	profile;
	unsigned int			qos_level;
};

#define QOS_LEVEL_OFF	1
#define QOS_LEVEL_ON	2

#define MAX_MSG_LEN	96
static int update_qos_level(struct device *dev, struct qoslat_data *d)
{
	struct qmp_pkt pkt;
	char mbox_msg[MAX_MSG_LEN + 1] = {0};
	char *qos_msg = "off";
	int ret;

	if (d->qos_level == QOS_LEVEL_ON)
		qos_msg = "on";

	snprintf(mbox_msg, MAX_MSG_LEN, "{class: ddr, perfmode: %s}", qos_msg);
	pkt.size = MAX_MSG_LEN;
	pkt.data = mbox_msg;

	ret = mbox_send_message(d->mbox, &pkt);
	if (ret < 0) {
		dev_err(dev, "Failed to send mbox message: %d\n", ret);
		return ret;
	}

	return 0;
}

static int dev_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct qoslat_data *d = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (!IS_ERR(opp))
		dev_pm_opp_put(opp);
	else
		return PTR_ERR(opp);

	if (*freq == d->qos_level)
		return 0;

	d->qos_level = *freq;

	return update_qos_level(dev, d);
}

static int dev_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct qoslat_data *d = dev_get_drvdata(dev);

	*freq = d->qos_level;

	return 0;
}

static int devfreq_qcom_qoslat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct qoslat_data *d;
	struct devfreq_dev_profile *p;
	const char *gov_name;
	int ret = 0;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	dev_set_drvdata(dev, d);

	if (!of_find_property(dev->of_node, "mboxes", NULL)) {
		dev_err(dev, "Couldn't find AOP mbox\n");
		return -EINVAL;
	}
	d->mbox_cl.dev = dev;
	d->mbox_cl.tx_block = true;
	d->mbox_cl.tx_tout = 1000;
	d->mbox_cl.knows_txdone = false;
	d->mbox = mbox_request_channel(&d->mbox_cl, 0);
	if (IS_ERR(d->mbox)) {
		ret = PTR_ERR(d->mbox);
		dev_err(dev, "Failed to get mailbox channel: %d\n", ret);
		return ret;
	}
	d->qos_level = QOS_LEVEL_OFF;

	p = &d->profile;
	p->target = dev_target;
	p->get_cur_freq = dev_get_cur_freq;
	p->polling_ms = 10;

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0)
		dev_err(dev, "Couldn't parse OPP table: %d\n", ret);

	if (of_property_read_string(dev->of_node, "governor", &gov_name))
		gov_name = "powersave";

	d->df = devfreq_add_device(dev, p, gov_name, NULL);
	if (IS_ERR(d->df)) {
		ret = PTR_ERR(d->df);
		dev_err(dev, "Failed to add devfreq device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id devfreq_qoslat_match_table[] = {
	{ .compatible = "qcom,devfreq-qoslat" },
	{}
};

static struct platform_driver devfreq_qcom_qoslat_driver = {
	.probe = devfreq_qcom_qoslat_probe,
	.driver = {
		.name		= "devfreq-qcom-qoslat",
		.of_match_table = devfreq_qoslat_match_table,
	},
};
module_platform_driver(devfreq_qcom_qoslat_driver);
MODULE_DESCRIPTION("Device driver for setting memory latency qos level");
MODULE_LICENSE("GPL v2");
