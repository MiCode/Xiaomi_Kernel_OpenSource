/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/reboot.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/slab.h>

#define MAX_AOP_MSG_LEN			96
#define AOP_DDR_TARG_FREQ_MHZ		300

struct mbox_data {
	struct mbox_client cl;
	struct mbox_chan *mbox;
	struct notifier_block reboot_notif_blk;
};

static void send_aop_ddr_freq_msg(struct mbox_data *aop_mbox, int freq_mhz)
{
	struct qmp_pkt pkt;
	char mbox_msg[MAX_AOP_MSG_LEN + 1] = {0};
	int rc;

	scnprintf(mbox_msg, MAX_AOP_MSG_LEN,
		  "{class: ddr, res: fixed, val: %d}", freq_mhz);
	pkt.size = MAX_AOP_MSG_LEN;
	pkt.data = mbox_msg;

	rc = mbox_send_message(aop_mbox->mbox, &pkt);

	if (rc < 0)
		pr_err("Failed to send AOP DDR freq msg: %d rc: %d\n", freq_mhz,
		       rc);
}

static int aop_ddr_freq_msg_handler(struct notifier_block *this,
				    unsigned long event, void *ptr)
{
	struct mbox_data *aop_mbox = container_of(this, struct mbox_data,
						  reboot_notif_blk);

	if (event == SYS_HALT || event == SYS_POWER_OFF)
		send_aop_ddr_freq_msg(aop_mbox, AOP_DDR_TARG_FREQ_MHZ);

	return NOTIFY_DONE;
}

static int aop_ddr_msgs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc;
	struct mbox_data *aop_mbox = devm_kzalloc(dev, sizeof(*aop_mbox),
						  GFP_KERNEL);

	if (!aop_mbox)
		return -ENOMEM;

	aop_mbox->cl.dev = dev;
	aop_mbox->cl.tx_block = true;
	aop_mbox->cl.tx_tout = 1000;
	aop_mbox->cl.knows_txdone = false;
	aop_mbox->mbox = mbox_request_channel(&aop_mbox->cl, 0);
	if (IS_ERR(aop_mbox->mbox)) {
		rc = PTR_ERR(aop_mbox->mbox);
		pr_err("Failed to get mailbox channel rc: %d\n", rc);
		return rc;
	}

	aop_mbox->reboot_notif_blk.notifier_call = aop_ddr_freq_msg_handler;
	platform_set_drvdata(pdev, aop_mbox);
	rc = register_reboot_notifier(&aop_mbox->reboot_notif_blk);
	if (rc < 0) {
		pr_err("Failed to register to the reboot notifier rc: %d\n",
		       rc);
		mbox_free_channel(aop_mbox->mbox);
		platform_set_drvdata(pdev, NULL);
	}
	return rc;
}

static int aop_ddr_msgs_remove(struct platform_device *pdev)
{
	struct mbox_data *aop_mbox = platform_get_drvdata(pdev);

	mbox_free_channel(aop_mbox->mbox);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id of_aop_ddr_match_tbl[] = {
	{ .compatible = "qcom,aop-ddr-msgs", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aop_ddr_match_tbl);

static struct platform_driver aop_ddr_msgs_driver = {
	.probe = aop_ddr_msgs_probe,
	.remove = aop_ddr_msgs_remove,
	.driver = {
		.name = "aop-ddr-msgs",
		.of_match_table = of_match_ptr(of_aop_ddr_match_tbl),
	},
};
module_platform_driver(aop_ddr_msgs_driver);

MODULE_DESCRIPTION("Qualcomm Technologies Inc AOP DDR Messaging driver");
MODULE_LICENSE("GPL v2");
