/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>

#define MAX_AOP_MSG_LEN			96

static void send_aop_ddrss_cmd(struct mbox_chan *aop_mbox)
{
	struct qmp_pkt pkt;
	char mbox_msg[MAX_AOP_MSG_LEN + 1] = {0};
	int rc;

	strlcpy(mbox_msg, "{class: ddr, perfmode: on}", MAX_AOP_MSG_LEN);
	pkt.size = MAX_AOP_MSG_LEN;
	pkt.data = mbox_msg;

	rc = mbox_send_message(aop_mbox, &pkt);

	if (rc < 0)
		pr_err("Failed to send AOP DDRSS command\n");
}

static int aop_ddrss_cmd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mbox_client cl = {0};
	struct mbox_chan *aop_mbox;
	int rc = 0;

	cl.dev = dev;
	cl.tx_block = true;
	cl.tx_tout = 1000;
	cl.knows_txdone = false;
	aop_mbox = mbox_request_channel(&cl, 0);
	if (IS_ERR(aop_mbox)) {
		rc = PTR_ERR(aop_mbox);
		pr_err("Failed to get mailbox channel rc: %d\n", rc);
		return rc;
	}

	send_aop_ddrss_cmd(aop_mbox);
	mbox_free_channel(aop_mbox);

	return rc;
}

static const struct of_device_id of_aop_ddrss_cmd_match_tbl[] = {
	{ .compatible = "qcom,aop-ddrss-cmds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aop_ddrss_cmd_match_tbl);

static struct platform_driver aop_ddrss_cmd_driver = {
	.probe = aop_ddrss_cmd_probe,
	.driver = {
		.name = "aop-ddrss-cmds",
		.of_match_table = of_match_ptr(of_aop_ddrss_cmd_match_tbl),
	},
};
module_platform_driver(aop_ddrss_cmd_driver);

MODULE_DESCRIPTION("QTI AOP DDRSS Commands driver");
MODULE_LICENSE("GPL v2");
