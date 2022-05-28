// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/rpmsg.h>

#include "aps_ipi.h"
#include "aps_utils.h"

/*
 * type0: Distinguish pwr_time, timeout, klog, or CX
 * type1: prepare to C1~C16
 * dir  : Distinguish read or write
 * data : store data
 */

struct aps_ipi_data {
	u32 type0;
	u16 dir;
	u64 data;
};
static struct aps_ipi_data ipi_tx_recv_buf;
static struct aps_ipi_data ipi_rx_send_buf;
static struct mutex aps_ipi_mtx;

struct aps_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct aps_rpmsg_device aps_tx_rpm_dev;
static struct aps_rpmsg_device aps_rx_rpm_dev;

int aps_ipi_send(int type_0, u64 val)
{
	struct aps_ipi_data ipi_cmd_send;

	if (!aps_tx_rpm_dev.ept)
		return 0;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.dir    = APS_IPI_WRITE;
	ipi_cmd_send.data   = val;

	mutex_lock(&aps_ipi_mtx);

	rpmsg_send(aps_tx_rpm_dev.ept, &ipi_cmd_send, sizeof(ipi_cmd_send));

	mutex_unlock(&aps_ipi_mtx);

	return 0;
}

int aps_ipi_recv(int type_0, u64 *val)
{
	struct aps_ipi_data ipi_cmd_send;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.dir    = APS_IPI_READ;
	ipi_cmd_send.data   = 0;

	mutex_lock(&aps_ipi_mtx);

	rpmsg_send(aps_tx_rpm_dev.ept, &ipi_cmd_send, sizeof(ipi_cmd_send));

	if (wait_for_completion_interruptible_timeout(
			&aps_tx_rpm_dev.ack,
			msecs_to_jiffies(10)) == 0) {
		mutex_unlock(&aps_ipi_mtx);
		APS_ERR("%s timeout\n", __func__);
		return -1;
	}

	*val  = (u64)ipi_tx_recv_buf.data;

	mutex_unlock(&aps_ipi_mtx);

	return 0;
}

static int aps_rpmsg_tx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct aps_ipi_data *d = (struct aps_ipi_data *)data;

	if (d->dir != APS_IPI_READ)
		return 0;

	if (d->type0 == APS_IPI_MICROP_MSG) {
		APS_ERR("Receive uP message -> use the wrong channel!?\n");
	} else {
		ipi_tx_recv_buf.type0  = d->type0;
		ipi_tx_recv_buf.dir    = d->dir;
		ipi_tx_recv_buf.data   = d->data;
		complete(&aps_tx_rpm_dev.ack);
	}

	return 0;
}

static void aps_ipi_up_msg(u32 type, u64 val)
{
	if (type == APS_IPI_MICROP_MSG)
		aps_aee_warn("aps", "aps aee");
}

static int aps_rpmsg_rx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct aps_ipi_data *d = (struct aps_ipi_data *)data;

	if (d->type0 == APS_IPI_MICROP_MSG) {

		ipi_rx_send_buf.type0  = d->type0;
		ipi_rx_send_buf.dir    = d->dir;
		ipi_rx_send_buf.data   = d->data;

		rpmsg_send(aps_rx_rpm_dev.ept, &ipi_rx_send_buf, sizeof(ipi_rx_send_buf));

		aps_ipi_up_msg(d->type0, d->data);
	} else {
		APS_ERR("Receive command ack -> use the wrong channel!?\n");
	}

	return 0;
}

static int aps_rpmsg_tx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
			rpdev->id.name, rpdev->src);

	aps_tx_rpm_dev.ept = rpdev->ept;
	aps_tx_rpm_dev.rpdev = rpdev;

	return 0;
}

static int aps_rpmsg_rx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
			rpdev->id.name, rpdev->src);

	aps_rx_rpm_dev.ept = rpdev->ept;
	aps_rx_rpm_dev.rpdev = rpdev;

	return 0;
}

static void aps_rpmsg_remove(struct rpmsg_device *rpdev)
{
}


static const struct of_device_id aps_tx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,aps-tx-rpmsg"},
	{},
};

static const struct of_device_id aps_rx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,aps-rx-rpmsg"},
	{},
};

static struct rpmsg_driver aps_rpmsg_tx_drv = {
	.drv = {
		.name = "aps-tx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = aps_tx_rpmsg_of_match,
	},
	.probe = aps_rpmsg_tx_probe,
	.callback = aps_rpmsg_tx_cb,
	.remove = aps_rpmsg_remove,
};

static struct rpmsg_driver aps_rpmsg_rx_drv = {
	.drv = {
		.name = "aps-rx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = aps_rx_rpmsg_of_match,
	},
	.probe = aps_rpmsg_rx_probe,
	.callback = aps_rpmsg_rx_cb,
	.remove = aps_rpmsg_remove,
};

int aps_ipi_init(void)
{
	int ret;

	init_completion(&aps_rx_rpm_dev.ack);
	init_completion(&aps_tx_rpm_dev.ack);
	mutex_init(&aps_ipi_mtx);

	ret = register_rpmsg_driver(&aps_rpmsg_rx_drv);
	if (ret)
		APS_ERR("failed to register aps rx rpmsg\n");

	ret = register_rpmsg_driver(&aps_rpmsg_tx_drv);
	if (ret)
		APS_ERR("failed to register aps tx rpmsg\n");

	return 0;
}

void aps_ipi_deinit(void)
{
	unregister_rpmsg_driver(&aps_rpmsg_tx_drv);
	unregister_rpmsg_driver(&aps_rpmsg_rx_drv);
	mutex_destroy(&aps_ipi_mtx);
}

