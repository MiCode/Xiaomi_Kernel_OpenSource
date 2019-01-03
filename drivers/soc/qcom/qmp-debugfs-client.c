// SPDX-License-Identifier: GPL-2.0-only

/* Copyright (c) 2017,2018, The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mailbox_client.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/mailbox/qmp.h>
#include <linux/uaccess.h>
#include <linux/mailbox_controller.h>

#define MAX_MSG_SIZE 96 /* Imposed by the remote*/

struct qmp_debugfs_data {
	struct qmp_pkt pkt;
	char buf[MAX_MSG_SIZE + 1];
};

static struct qmp_debugfs_data data_pkt[MBOX_TX_QUEUE_LEN];
static struct mbox_chan *chan;
static struct mbox_client *cl;

static DEFINE_MUTEX(qmp_debugfs_mutex);

static ssize_t aop_msg_write(struct file *file, const char __user *userstr,
		size_t len, loff_t *pos)
{
	static int count;
	int rc;

	if (!len || (len > MAX_MSG_SIZE))
		return len;

	mutex_lock(&qmp_debugfs_mutex);

	if (count >= MBOX_TX_QUEUE_LEN)
		count = 0;

	memset(&(data_pkt[count]), 0, sizeof(data_pkt[count]));
	rc  = copy_from_user(data_pkt[count].buf, userstr, len);
	if (rc) {
		pr_err("%s copy from user failed, rc=%d\n", __func__, rc);
		mutex_unlock(&qmp_debugfs_mutex);
		return len;
	}

	/*
	 * Controller expects a 4 byte aligned buffer
	 */
	data_pkt[count].pkt.size = (len + 0x3) & ~0x3;
	data_pkt[count].pkt.data = data_pkt[count].buf;

	if (mbox_send_message(chan, &(data_pkt[count].pkt)) < 0)
		pr_err("Failed to send qmp request\n");
	else
		count++;

	mutex_unlock(&qmp_debugfs_mutex);
	return len;
}

static const struct file_operations aop_msg_fops = {
	.write = aop_msg_write,
};

static int qmp_msg_probe(struct platform_device *pdev)
{
	struct dentry *file;

	cl = devm_kzalloc(&pdev->dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	cl->dev = &pdev->dev;
	cl->tx_block = true;
	cl->tx_tout = 1000;
	cl->knows_txdone = false;

	chan = mbox_request_channel(cl, 0);
	if (IS_ERR(chan)) {
		dev_err(&pdev->dev, "Failed to mbox channel\n");
		return PTR_ERR(chan);
	}

	file = debugfs_create_file("aop_send_message", 0220, NULL, NULL,
			&aop_msg_fops);
	if (!file)
		goto err;
	return 0;
err:
	mbox_free_channel(chan);
	chan = NULL;
	return -ENOMEM;
}

static const struct of_device_id aop_qmp_match_tbl[] = {
	{.compatible = "qcom,debugfs-qmp-client"},
	{},
};

static struct platform_driver aop_qmp_msg_driver = {
	.probe = qmp_msg_probe,
	.driver = {
		.name = "debugfs-qmp-client",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
		.of_match_table = aop_qmp_match_tbl,
	},
};
builtin_platform_driver(aop_qmp_msg_driver);
