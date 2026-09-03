// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sipc.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#if IS_ENABLED(CONFIG_UNISOC_SIPC)
#include "../drivers/unisoc_platform/sysdump/sysdump.h"
#endif
#include "sipc_priv.h"

#if defined(CONFIG_DEBUG_FS)
void sipc_debug_putline(struct seq_file *m, char c, int n)
{
	char buf[300];
	int i, max, len;

	/* buf will end with '\n' and 0 */
	max = ARRAY_SIZE(buf) - 2;
	len = (n > max) ? max : n;

	for (i = 0; i < len; i++)
		buf[i] = c;

	buf[i] = '\n';
	buf[i + 1] = 0;

	seq_puts(m, buf);
}
EXPORT_SYMBOL_GPL(sipc_debug_putline);
#endif

static u64 sprd_u32_to_u64(u32 *mssg)
{
	u64 msg_64 = 0;

	msg_64 = mssg[1];
	msg_64 = msg_64 << 32;
	msg_64 |= mssg[0];
	return msg_64;
}

static void sprd_rx_callback(struct mbox_client *client, void *message)
{

	struct smsg_ipc *ipc  = dev_get_drvdata(client->dev);
	struct smsg *msg = NULL;
	struct device *dev = client->dev;
	u64 data;

	data = sprd_u32_to_u64(message);
	if (!data) {
		dev_err(dev, "receive data is null !\n");
	} else {
		msg = (struct smsg *)&data;
		smsg_msg_process(ipc, msg, true);
	}
}

static void sprd_sensor_rx_callback(struct mbox_client *client, void *message)
{

	struct smsg_ipc *ipc  = dev_get_drvdata(client->dev);
	struct smsg *msg = NULL;
	struct device *dev = client->dev;
	u64 data;

	data = sprd_u32_to_u64(message);
	if (!data) {
		dev_err(dev, "receive data is null !\n");
	} else {
		msg = (struct smsg *)&data;
		smsg_msg_process(ipc, msg, false);
	}
}

static void sprd_wcn_mbox_rx_callback(struct mbox_client *client, void *message)
{
	struct smsg_ipc *ipc  = dev_get_drvdata(client->dev);
	struct smsg *msg = NULL;
	struct device *dev = client->dev;
	u64 data;

	data = sprd_u32_to_u64(message);
	if (!data) {
		dev_err(dev, "receive data is null !\n");
	} else {
		msg = (struct smsg *)&data;
		if (msg->channel == SMSG_CH_DATA0)
			return smsg_msg_process(ipc, msg, false);
		else
			return smsg_msg_process(ipc, msg, true);
		}
}

static int sprd_get_smem_info(struct device *dev,
			      struct smsg_ipc *ipc, struct device_node *np)
{
	struct smem_item *smem_ptr;
	int i, count;
	const __be32 *list;

	list = of_get_property(np, "sprd,smem-info", &count);
	if (!list || !count) {
		pr_err("no smem-info\n");
		return -ENODEV;
	}

	count = count / sizeof(*smem_ptr);
	smem_ptr = kcalloc(count, sizeof(*smem_ptr), GFP_KERNEL);
	if (!smem_ptr)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		smem_ptr[i].smem_base = be32_to_cpu(*list++);
		smem_ptr[i].dst_smem_base = be32_to_cpu(*list++);
		smem_ptr[i].smem_size = be32_to_cpu(*list++);
		dev_info(dev, "smem count=%d, base=0x%x, dstbase=0x%x, size=0x%x\n",
			 i,
			 smem_ptr[i].smem_base,
			 smem_ptr[i].dst_smem_base,
			 smem_ptr[i].smem_size);
	}

	ipc->smem_cnt = count;
	ipc->smem_ptr = smem_ptr;

	/* default mem */
	ipc->smem_base = smem_ptr[0].smem_base;
	ipc->dst_smem_base = smem_ptr[0].dst_smem_base;
	ipc->smem_size = smem_ptr[0].smem_size;

	return 0;
}

static int sprd_ipc_parse_dt(struct device *dev,
			     struct device_node *np, struct smsg_ipc *ipc)
{
	u32 value;
	int err;

	/* Get sipc label */
	err = of_property_read_string(np, "label", &ipc->name);
	if (err)
		return err;
	dev_info(dev, "label  =%s\n", ipc->name);

	/* Get sipc dst */
	err = of_property_read_u32(np, "reg", &value);
	if (err)
		return err;
	ipc->dst = (u8)value;
	dev_info(dev, "dst    =%d\n", ipc->dst);

	/* default mailbox */
	ipc->type = SIPC_BASE_MBOX;

	/* get smem info */
	err = sprd_get_smem_info(dev, ipc, np);
	if (err) {
		dev_err(dev, "sipc: parse smem info failed.\n");
		return err;
	}
	return 0;
}

static int sprd_ipc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct smsg_ipc *ipc;
	int ret;

	ipc = devm_kzalloc(&pdev->dev,
			   sizeof(struct smsg_ipc), GFP_KERNEL);
	if (!ipc)
		return -ENOMEM;

	if (sprd_ipc_parse_dt(&pdev->dev, np, ipc)) {
		dev_err(dev, "failed to parse dt!\n");
		return -ENODEV;
	}

	smsg_ipc_create(ipc);
	platform_set_drvdata(pdev, ipc);

	/* mailbox request */
	ipc->cl.dev = &pdev->dev;
	ipc->cl.tx_block = false;
	ipc->cl.knows_txdone = false;
	/* Immediately Submit next message
	 * Not notify the client,so not use tx_done
	 */
	ipc->cl.tx_done = NULL;
	if (ipc->dst == SIPC_ID_WCN) {
		ipc->cl.rx_callback = sprd_wcn_mbox_rx_callback;
		dev_info(dev, "register wcn mbox rx callback\n");
	} else {
		ipc->cl.rx_callback = sprd_rx_callback;
	}
	ipc->chan = mbox_request_channel(&ipc->cl, 0);
	if (IS_ERR(ipc->chan)) {
		dev_err(dev, "failed to sipc mailbox, dst = %d\n", ipc->dst);
		ret = PTR_ERR(ipc->chan);
		goto out;
	}

	/* request sensor mailbox channel */
	if ((ipc->dst == SIPC_ID_PM_SYS) || (ipc->dst == SIPC_ID_CH)) {
		/* mailbox request */
		ipc->sensor_cl = ipc->cl;
		ipc->sensor_cl.rx_callback = sprd_sensor_rx_callback;
		ipc->sensor_chan = mbox_request_channel(&ipc->sensor_cl, 1);
		if (IS_ERR(ipc->sensor_chan))
			dev_err(dev, "failed to sipc sensor mailbox\n");
		else
			ipc->sensor_core = (uintptr_t)ipc->sensor_chan->con_priv;
	}

	init_waitqueue_head(&ipc->suspend_wait);
	spin_lock_init(&ipc->suspend_pinlock);
	spin_lock_init(&ipc->txpinlock);

	dev_info(dev, "sprd ipc probe success\n");

	/* populating sub-devices */
	ret = devm_of_platform_populate(dev);
	if (ret) {
		dev_err(dev, "Failed to populate sub-devices\n");
		return ret;
	}

	return 0;
out:
	if (!IS_ERR(ipc->chan))
		mbox_free_channel(ipc->chan);
	if (!IS_ERR(ipc->sensor_chan))
		mbox_free_channel(ipc->sensor_chan);
	return ret;
}

static int sprd_ipc_remove(struct platform_device *pdev)
{
	struct smsg_ipc *ipc = platform_get_drvdata(pdev);

	smsg_ipc_destroy(ipc);
	kfree(ipc->smem_ptr);

	devm_kfree(&pdev->dev, ipc);
	return 0;
}

static const struct of_device_id sprd_ipc_match_table[] = {
	{ .compatible = "sprd,sipc", },
	{ },
};

static struct platform_driver sprd_ipc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-sipc",
		.of_match_table = sprd_ipc_match_table,
	},
	.probe = sprd_ipc_probe,
	.remove = sprd_ipc_remove,
};

static int __init sprd_ipc_init(void)
{
	smsg_init_wakeup();
	smsg_init_channel2index();
#if IS_ENABLED(CONFIG_UNISOC_SIPC)
	sysdump_callback_register(smsg_senddie);
#endif
#if defined(CONFIG_DEBUG_FS)
	smem_init_debug();
	smsg_init_debug();
	sbuf_init_debug();
	sblock_init_debug();
#endif
	return platform_driver_register(&sprd_ipc_driver);
}

static void __exit sprd_ipc_exit(void)
{
	smsg_remove_wakeup();
#if defined(CONFIG_DEBUG_FS)
	smem_destroy_debug();
	smsg_destroy_debug();
	sbuf_destroy_debug();
	sblock_destroy_debug();
#endif
#if IS_ENABLED(CONFIG_UNISOC_SIPC)
	sysdump_callback_unregister();
#endif
	platform_driver_unregister(&sprd_ipc_driver);
}

subsys_initcall_sync(sprd_ipc_init);
module_exit(sprd_ipc_exit);

MODULE_AUTHOR("Wenping Zhou <wenping.zhou@unisoc.com>");
MODULE_AUTHOR("Orson Zhai <orson.zhai@unisoc.com>");
MODULE_AUTHOR("Haidong Yao <haidong.yao@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum Inter Remote Processors Communication driver");
MODULE_LICENSE("GPL v2");
