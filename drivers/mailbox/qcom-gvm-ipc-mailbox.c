/*
 * Copyright (c) 2020, The Linux Foundation, All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>

#define QCOM_GVM_CHNL	32

struct qcom_gvm_ipc {
	struct mbox_controller mbox;
	struct mbox_chan mbox_chans[QCOM_GVM_CHNL];

	void __iomem *reg;
	unsigned long offset;
};

static int qcom_gvm_ipc_send_data(struct mbox_chan *chan, void *data)
{
	struct qcom_gvm_ipc *gvm_ipc = container_of(chan->mbox,
						  struct qcom_gvm_ipc, mbox);

	__raw_writel(1, gvm_ipc->reg);

	return 0;
}

static const struct mbox_chan_ops qcom_gvm_ipc_ops = {
	.send_data = qcom_gvm_ipc_send_data,
};

static int qcom_gvm_ipc_probe(struct platform_device *pdev)
{
	struct qcom_gvm_ipc *gvm_ipc;
	struct resource *res;
	void __iomem *base;
	unsigned long i;
	int ret;

	gvm_ipc = devm_kzalloc(&pdev->dev, sizeof(*gvm_ipc), GFP_KERNEL);
	if (!gvm_ipc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(base))
		return PTR_ERR(base);

	gvm_ipc->reg = base;
	/* Initialize channel identifiers */
	for (i = 0; i < ARRAY_SIZE(gvm_ipc->mbox_chans); i++)
		gvm_ipc->mbox_chans[i].con_priv = (void *)i;

	gvm_ipc->mbox.dev = &pdev->dev;
	gvm_ipc->mbox.ops = &qcom_gvm_ipc_ops;
	gvm_ipc->mbox.chans = gvm_ipc->mbox_chans;
	gvm_ipc->mbox.num_chans = ARRAY_SIZE(gvm_ipc->mbox_chans);

	ret = mbox_controller_register(&gvm_ipc->mbox);
	if (ret) {
		dev_err(&pdev->dev, "failed to register GVM IPC controller\n");
		return ret;
	}

	platform_set_drvdata(pdev, gvm_ipc);

	return 0;
}

static int qcom_gvm_ipc_remove(struct platform_device *pdev)
{
	struct qcom_gvm_ipc *gvm_ipc = platform_get_drvdata(pdev);

	mbox_controller_unregister(&gvm_ipc->mbox);

	return 0;
}

/* .data is the offset of the ipc register within the global block */
static const struct of_device_id qcom_gvm_ipc_of_match[] = {
	{ .compatible = "qcom,sm8150-apcs-hmss-global" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_gvm_ipc_of_match);

static struct platform_driver qcom_gvm_ipc_driver = {
	.probe = qcom_gvm_ipc_probe,
	.remove = qcom_gvm_ipc_remove,
	.driver = {
		.name = "qcom_gvm_ipc",
		.of_match_table = qcom_gvm_ipc_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init qcom_gvm_ipc_init(void)
{
	return platform_driver_register(&qcom_gvm_ipc_driver);
}
postcore_initcall(qcom_gvm_ipc_init);

static void __exit qcom_gvm_ipc_exit(void)
{
	platform_driver_unregister(&qcom_gvm_ipc_driver);
}
module_exit(qcom_gvm_ipc_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. GVM IPC driver");
MODULE_LICENSE("GPL v2");
