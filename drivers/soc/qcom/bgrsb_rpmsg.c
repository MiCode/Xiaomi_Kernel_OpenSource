/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include "bgrsb_rpmsg.h"

static struct bgrsb_rpmsg_dev *pdev;

int bgrsb_rpmsg_tx_msg(void  *msg, size_t len)
{
	int ret;

	if (pdev == NULL || !pdev->chnl_state)
		pr_err("pmsg_device is null, channel is closed\n");

	pdev->message = msg;
	pdev->message_length = len;
	if (pdev->message) {
		ret = rpmsg_send(pdev->channel,
			pdev->message, pdev->message_length);
		if (ret)
			pr_err("rpmsg_send failed: %d\n", ret);

	}
	return ret;
}
EXPORT_SYMBOL(bgrsb_rpmsg_tx_msg);

static int bgrsb_rpmsg_probe(struct rpmsg_device  *rpdev)
{
	int ret;
	void *msg;

	pdev = devm_kzalloc(&rpdev->dev, sizeof(*pdev), GFP_KERNEL);
	if (!pdev)
		return -ENOMEM;

	pdev->channel = rpdev->ept;
	pdev->dev = &rpdev->dev;
	if (pdev->channel == NULL)
		return -ENOMEM;

	pdev->chnl_state = true;
	dev_set_drvdata(&rpdev->dev, pdev);

	/* send a callback to bg-rsb driver*/
	bgrsb_notify_glink_channel_state(true);
	if (pdev->message == NULL)
		ret = bgrsb_rpmsg_tx_msg(msg, 0);
	return 0;
}

static void bgrsb_rpmsg_remove(struct rpmsg_device *rpdev)
{
	pdev->chnl_state = false;
	pdev->message == NULL;
	dev_dbg(&rpdev->dev, "rpmsg client driver is removed\n");
	bgrsb_notify_glink_channel_state(false);
	dev_set_drvdata(&rpdev->dev, NULL);
}

static int bgrsb_rpmsg_cb(struct rpmsg_device *rpdev,
				void *data, int len, void *priv, u32 src)
{
	struct bgrsb_rpmsg_dev *dev =
			dev_get_drvdata(&rpdev->dev);

	if (!dev)
		return -ENODEV;
	bgrsb_rx_msg(data, len);
	return 0;
}

static const struct rpmsg_device_id rpmsg_driver_bgrsb_id_table[] = {
	{ "RSB_CTRL" },
	{},
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_bgrsb_id_table);

static const struct of_device_id rpmsg_driver_bgrsb_of_match[] = {
	{ .compatible = "qcom,bgrsb-rpmsg" },
	{},
};

static struct rpmsg_driver rpmsg_bgrsb_client = {
	.id_table = rpmsg_driver_bgrsb_id_table,
	.probe = bgrsb_rpmsg_probe,
	.callback = bgrsb_rpmsg_cb,
	.remove = bgrsb_rpmsg_remove,
	.drv = {
		.name = "qcom,bg_rsb_rpmsg",
		.of_match_table = rpmsg_driver_bgrsb_of_match,
	},
};
module_rpmsg_driver(rpmsg_bgrsb_client);

MODULE_DESCRIPTION("Interface Driver for BG-RSB and RPMSG");
MODULE_LICENSE("GPL v2");
