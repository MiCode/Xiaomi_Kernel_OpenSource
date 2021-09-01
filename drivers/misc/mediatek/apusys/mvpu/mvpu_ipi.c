// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/rpmsg.h>

#include "mvpu_ipi.h"

/*
 * type0: Distinguish pwr_time, timeout, klog, or CX
 * type1: prepare to C1~C16
 * dir  : Distinguish read or write
 * data : store data
 */

struct mvpu_ipi_data {
	u32 type0;
	u16 dir;
	u64 data;
};
static struct mvpu_ipi_data ipi_tx_recv_buf;
static struct mvpu_ipi_data ipi_rx_send_buf;
static struct mutex mvpu_ipi_mtx;

struct mvpu_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct mvpu_rpmsg_device mvpu_tx_rpm_dev;
static struct mvpu_rpmsg_device mvpu_rx_rpm_dev;

int mvpu_ipi_send(int type_0, u64 val)
{
	struct mvpu_ipi_data ipi_cmd_send;

	if (!mvpu_tx_rpm_dev.ept)
		return 0;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.dir    = MVPU_IPI_WRITE;
	ipi_cmd_send.data   = val;

	mutex_lock(&mvpu_ipi_mtx);

	rpmsg_send(mvpu_tx_rpm_dev.ept, &ipi_cmd_send, sizeof(ipi_cmd_send));

	mutex_unlock(&mvpu_ipi_mtx);

	return 0;
}

int mvpu_ipi_recv(int type_0, u64 *val)
{
	struct mvpu_ipi_data ipi_cmd_send;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.dir    = MVPU_IPI_READ;
	ipi_cmd_send.data   = 0;

	mutex_lock(&mvpu_ipi_mtx);

	rpmsg_send(mvpu_tx_rpm_dev.ept, &ipi_cmd_send, sizeof(ipi_cmd_send));

	if (wait_for_completion_interruptible_timeout(
			&mvpu_tx_rpm_dev.ack,
			msecs_to_jiffies(10)) == 0) {
		mutex_unlock(&mvpu_ipi_mtx);
		pr_info("timeout\n", __func__);
		return -1;
	}

	*val  = (u64)ipi_tx_recv_buf.data;

	mutex_unlock(&mvpu_ipi_mtx);

	return 0;
}

static int mvpu_rpmsg_tx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct mvpu_ipi_data *d = (struct mvpu_ipi_data *)data;

	if (d->dir != MVPU_IPI_READ)
		return 0;

	if (d->type0 == MVPU_IPI_MICROP_MSG) {
		pr_info("Receive uP message -> use the wrong channel!?\n");
	} else {
		ipi_tx_recv_buf.type0  = d->type0;
		ipi_tx_recv_buf.dir    = d->dir;
		ipi_tx_recv_buf.data   = d->data;
		complete(&mvpu_tx_rpm_dev.ack);
	}

	return 0;
}

static void mvpu_ipi_up_msg(u32 type, u64 val)
{
	if (type == MVPU_IPI_MICROP_MSG)
		mvpu_aee_warn("MVPU", "MVPU aee");
}

static int mvpu_rpmsg_rx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct mvpu_ipi_data *d = (struct mvpu_ipi_data *)data;

	if (d->type0 == MVPU_IPI_MICROP_MSG) {

		ipi_rx_send_buf.type0  = d->type0;
		ipi_rx_send_buf.dir    = d->dir;
		ipi_rx_send_buf.data   = d->data;

		rpmsg_send(mvpu_rx_rpm_dev.ept, &ipi_rx_send_buf, sizeof(ipi_rx_send_buf));

		mvpu_ipi_up_msg(d->type0, d->data);
	} else {
		pr_info("Receive command ack -> use the wrong channel!?\n");
	}

	return 0;
}

static int mvpu_rpmsg_tx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
			rpdev->id.name, rpdev->src);

	mvpu_tx_rpm_dev.ept = rpdev->ept;
	mvpu_tx_rpm_dev.rpdev = rpdev;

	return 0;
}

static int mvpu_rpmsg_rx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
			rpdev->id.name, rpdev->src);

	mvpu_rx_rpm_dev.ept = rpdev->ept;
	mvpu_rx_rpm_dev.rpdev = rpdev;

	return 0;
}

static void mvpu_rpmsg_remove(struct rpmsg_device *rpdev)
{
}


static const struct of_device_id mvpu_tx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,mvpu-tx-rpmsg"},
	{},
};

static const struct of_device_id mvpu_rx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,mvpu-rx-rpmsg"},
	{},
};

static struct rpmsg_driver mvpu_rpmsg_tx_drv = {
	.drv = {
		.name = "mvpu-tx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mvpu_tx_rpmsg_of_match,
	},
	.probe = mvpu_rpmsg_tx_probe,
	.callback = mvpu_rpmsg_tx_cb,
	.remove = mvpu_rpmsg_remove,
};

static struct rpmsg_driver mvpu_rpmsg_rx_drv = {
	.drv = {
		.name = "mvpu-rx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mvpu_rx_rpmsg_of_match,
	},
	.probe = mvpu_rpmsg_rx_probe,
	.callback = mvpu_rpmsg_rx_cb,
	.remove = mvpu_rpmsg_remove,
};

int mvpu_ipi_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	init_completion(&mvpu_rx_rpm_dev.ack);
	init_completion(&mvpu_tx_rpm_dev.ack);
	mutex_init(&mvpu_ipi_mtx);

	ret = register_rpmsg_driver(&mvpu_rpmsg_rx_drv);
	if (ret)
		pr_info("failed to register mvpu rx rpmsg\n");

	ret = register_rpmsg_driver(&mvpu_rpmsg_tx_drv);
	if (ret)
		pr_info("failed to register mvpu tx rpmsg\n");

	return 0;
}

void mvpu_ipi_deinit(void)
{
	unregister_rpmsg_driver(&mvpu_rpmsg_tx_drv);
	unregister_rpmsg_driver(&mvpu_rpmsg_rx_drv);
	mutex_destroy(&mvpu_ipi_mtx);
}

