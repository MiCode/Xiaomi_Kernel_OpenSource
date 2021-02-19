// SPDX-License-Identifier: GPL-2.0-only
/* driver/misc/qrc/qrc_uart.c
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/serdev.h>
#include <linux/types.h>
#include<linux/slab.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include "qrc_core.h"

#define QRC_RX_FIFO_SIZE 0x400
#define QRC_TX_BUFF_SIZE 0x400
#define QRCUART_DRV_NAME "qrcuart"
#define QRC_DRV_VERSION "0.1.0"


static int qrcuart_setup(struct qrc_dev *dev);

static int
qrc_uart_receive(struct serdev_device *serdev, const unsigned char *data,
		size_t count)
{
	struct qrcuart *qrc = serdev_device_get_drvdata(serdev);
	struct qrc_dev *qrc_dev = qrc->qrc_dev;
	int ret;

	/* check count */
	ret = kfifo_avail(&qrc->qrc_rx_fifo);
	if (!ret)
		return 0;

	if (count > ret)
		count = ret;

	ret = kfifo_in(&qrc->qrc_rx_fifo, data, count);
	if (!ret)
		return 0;

	wake_up_interruptible(&qrc_dev->r_wait);

	return count;
}

/* Write out any remaining transmit buffer. Scheduled when tty is writable */
static void qrcuart_transmit(struct work_struct *work)
{
	struct qrcuart *qrc = container_of(work, struct qrcuart, tx_work);
	int written;

	spin_lock_bh(&qrc->lock);

	if (qrc->tx_left <= 0) {
		/* Now serial buffer is almost free & we can start
		 * transmission of another packet
		 */
		spin_unlock_bh(&qrc->lock);
		return;
	}

	written = serdev_device_write_buf(qrc->serdev, qrc->tx_head,
					  qrc->tx_left);
	if (written > 0) {
		qrc->tx_left -= written;
		qrc->tx_head += written;
	}
	spin_unlock_bh(&qrc->lock);
}

/* Called by the driver when there's room for more data.
 * Schedule the transmit.
 */
static void qrc_uart_wakeup(struct serdev_device *serdev)
{
	struct qrcuart *qrc = serdev_device_get_drvdata(serdev);

	schedule_work(&qrc->tx_work);
}

static struct serdev_device_ops qrc_serdev_ops = {
	.receive_buf = qrc_uart_receive,
	.write_wakeup = qrc_uart_wakeup,
};

/*----------------Interface to QRC core -----------------------------*/

static int qrcuart_open(struct qrc_dev *dev)
{
	return 0;
}

static int qrcuart_close(struct qrc_dev *dev)
{
	struct qrcuart *qrc = qrc_get_data(dev);

	flush_work(&qrc->tx_work);

	spin_lock_bh(&qrc->lock);
	qrc->tx_left = 0;
	spin_unlock_bh(&qrc->lock);

	return 0;
}

static int qrcuart_init(struct qrc_dev *dev)
{
	struct qrcuart *qrc = qrc_get_data(dev);
	size_t len;
	int ret;

	/* Finish setting up the device info. */
	len = QRC_TX_BUFF_SIZE;
	qrc->tx_buffer = devm_kmalloc(&qrc->serdev->dev, len, GFP_KERNEL);

	if (!qrc->tx_buffer)
		return -ENOMEM;

	qrc->tx_head = qrc->tx_buffer;
	qrc->tx_left = 0;

	ret = kfifo_alloc(&qrc->qrc_rx_fifo, QRC_RX_FIFO_SIZE,
					  GFP_KERNEL);
	if (ret)
		return -ENOMEM;

	return 0;
}
static void qrcuart_uninit(struct qrc_dev *dev)
{
	struct qrcuart *qrc = qrc_get_data(dev);

	kfifo_free(&qrc->qrc_rx_fifo);
}

/*put data from kfifo to qrc fifo */
static int qrcuart_receive(struct qrc_dev *dev, char __user *buf,
			       size_t count)
{
	struct qrcuart *qrc = qrc_get_data(dev);
	u32 fifo_len, trans_len;

	if (!kfifo_is_empty(&qrc->qrc_rx_fifo)) {
		fifo_len = kfifo_len(&qrc->qrc_rx_fifo);
		if (count > fifo_len)
			count = fifo_len;
		if (kfifo_to_user(&qrc->qrc_rx_fifo,
				(void *)buf, count, &trans_len))
			return -EFAULT;
		return trans_len;
	}
	return 0;
}

static int qrcuart_data_status(struct qrc_dev *dev)
{
	struct qrcuart *qrc = qrc_get_data(dev);

	return kfifo_len(&qrc->qrc_rx_fifo);
}

static void qrcuart_data_clean(struct qrc_dev *dev)
{
	struct qrcuart *qrc = qrc_get_data(dev);

	kfifo_reset(&qrc->qrc_rx_fifo);
}


static enum qrcdev_tx qrcuart_xmit(const char __user  *buf,
				size_t data_length, struct qrc_dev *dev)
{
	struct qrcuart *qrc = qrc_get_data(dev);
	struct qrc_device_stats *n_stats = &dev->stats;
	size_t written;
	u8 *pos;

	WARN_ON(qrc->tx_left);

	pos = qrc->tx_buffer + qrc->tx_left;
	if ((data_length + qrc->tx_left) > QRC_TX_BUFF_SIZE) {
		pr_err("qrcuart transmit date overflow %d\n", data_length);
		return __QRCDEV_TX_MIN;
	}

	if (copy_from_user(pos, buf, data_length))
		return __QRCDEV_TX_MIN;

	pos += data_length;

	spin_lock(&qrc->lock);

	written = serdev_device_write_buf(qrc->serdev, qrc->tx_buffer,
					  pos - qrc->tx_buffer);
	if (written > 0) {
		qrc->tx_left = (pos - qrc->tx_buffer) - written;
		qrc->tx_head = qrc->tx_buffer + written;
		n_stats->tx_bytes += written;
	}

	spin_unlock(&qrc->lock);

	return QRCDEV_TX_OK;
}

static int qrcuart_config(struct qrc_dev *dev)
{
	//baudrate,wordlength ... config
	return 0;
}

static const struct qrc_device_ops qrcuart_qrc_ops = {
	.qrcops_open = qrcuart_open,
	.qrcops_close = qrcuart_close,
	.qrcops_init = qrcuart_init,
	.qrcops_uninit = qrcuart_uninit,
	.qrcops_xmit = qrcuart_xmit,
	.qrcops_receive = qrcuart_receive,
	.qrcops_config = qrcuart_config,
	.qrcops_setup = qrcuart_setup,
	.qrcops_data_status = qrcuart_data_status,
	.qrcops_data_clean = qrcuart_data_clean,
};

static int qrcuart_setup(struct qrc_dev *dev)
{
	dev->qrc_ops = &qrcuart_qrc_ops;
	return 0;
}

static int qrc_uart_probe(struct serdev_device *serdev)
{
	struct qrc_dev *qdev;
	struct qrcuart *qrc;
	const char *mac;
	u32 speed = 115200;
	int ret;

	qdev = kmalloc(sizeof(*qdev), GFP_KERNEL);
	qrc = kmalloc(sizeof(*qrc), GFP_KERNEL);
	if ((!qrc) || (!qdev)) {
		pr_err("qrc_uart: Fail to retrieve private structure\n");
		goto free;
	}
	qrc_set_data(qdev, qrc);

	qrc->qrc_dev = qdev;
	qrc->serdev = serdev;
	spin_lock_init(&qrc->lock);
	INIT_WORK(&qrc->tx_work, qrcuart_transmit);
	qrcuart_setup(qdev);
	ret = qrcuart_init(qdev);
	if (ret) {
		qrcuart_uninit(qdev);
		pr_err("qrcuart: Fail to init qrc structure\n");
		return ret;
	}
	serdev_device_set_drvdata(serdev, qrc);
	serdev_device_set_client_ops(serdev, &qrc_serdev_ops);

	ret = serdev_device_open(serdev);
	if (ret) {
		pr_err("qrcuart :Unable to open device\n");
		ret = -ENOMEM;
		goto free;
	}

	speed = serdev_device_set_baudrate(serdev, speed);
	serdev_device_set_flow_control(serdev, false);

	ret = qrc_register_device(qdev, &serdev->dev);

	if (ret) {
		pr_err("qrcuart: Unable to register qrc device %s\n");
		serdev_device_close(serdev);
		cancel_work_sync(&qrc->tx_work);
		goto free;
	}
	dev_info(&serdev->dev, "qrcuart drv probed\n");

	return 0;

free:
	kfree(qdev);
	kfree(qrc);
	return ret;
}

static void qrc_uart_remove(struct serdev_device *serdev)
{
	struct qrcuart *qrc = serdev_device_get_drvdata(serdev);

	serdev_device_close(serdev);
	qrcuart_uninit(qrc->qrc_dev);
	cancel_work_sync(&qrc->tx_work);
	qrc_unregister(qrc->qrc_dev);
	kfree(qrc->qrc_dev);
	kfree(qrc);
	dev_info(&serdev->dev, "qrcuart drv removed\n");
}

static const struct of_device_id qrc_uart_of_match[] = {
	{
	 .compatible = "qcom,qrc-uart",
	},
	{}
};
MODULE_DEVICE_TABLE(of, qrc_of_match);


static struct serdev_device_driver qrc_uart_driver = {
	.probe = qrc_uart_probe,
	.remove = qrc_uart_remove,
	.driver = {
		.name = QRCUART_DRV_NAME,
		.of_match_table = of_match_ptr(qrc_uart_of_match),
	},
};

module_serdev_device_driver(qrc_uart_driver);

/**********************************************/

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. QRC Uart Driver");
MODULE_LICENSE("GPL v2");
