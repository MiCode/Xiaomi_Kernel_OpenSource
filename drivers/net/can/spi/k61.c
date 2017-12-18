/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/spi/spi.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/pm.h>

#define DEBUG_K61	0
#if DEBUG_K61 == 1
#define LOGDI(...) dev_info(&priv_data->spidev->dev, __VA_ARGS__)
#define LOGNI(...) netdev_info(netdev, __VA_ARGS__)
#else
#define LOGDI(...)
#define LOGNI(...)
#endif
#define LOGDE(...) dev_err(&priv_data->spidev->dev, __VA_ARGS__)
#define LOGNE(...) netdev_err(netdev, __VA_ARGS__)

#define MAX_TX_BUFFERS			1
#define XFER_BUFFER_SIZE		64
#define K61_CLOCK			120000000
#define K61_MAX_CHANNELS		1
#define K61_FW_QUERY_RETRY_COUNT	3

struct k61_can {
	struct net_device	*netdev;
	struct spi_device	*spidev;

	struct mutex spi_lock; /* SPI device lock */

	struct workqueue_struct *tx_wq;
	char *tx_buf, *rx_buf;
	int xfer_length;
	atomic_t msg_seq;

	atomic_t netif_queue_stop;
	struct completion response_completion;
	int reset;
	int wait_cmd;
	int cmd_result;
	int bits_per_word;
	int reset_delay_msec;
};

struct k61_netdev_privdata {
	struct can_priv can;
	struct k61_can *k61_can;
};

struct k61_tx_work {
	struct work_struct work;
	struct sk_buff *skb;
	struct net_device *netdev;
};

/* Message definitions */
struct spi_mosi { /* TLV for MOSI line */
	u8 cmd;
	u8 len;
	u16 seq;
	u8 data[];
} __packed;

struct spi_miso { /* TLV for MISO line */
	u8 cmd;
	u8 len;
	u16 seq; /* should match seq field from request, or 0 for unsols */
	u8 data[];
} __packed;

#define CMD_GET_FW_VERSION		0x81
#define CMD_CAN_SEND_FRAME		0x82
#define CMD_CAN_ADD_FILTER		0x83
#define CMD_CAN_REMOVE_FILTER		0x84
#define CMD_CAN_RECEIVE_FRAME		0x85
#define CMD_CAN_DATA_BUFF_ADD		0x87
#define CMD_CAN_DATA_BUFF_REMOVE	0x88
#define CMD_CAN_RELEASE_BUFFER          0x89
#define CMD_CAN_DATA_BUFF_REMOVE_ALL	0x8A

#define IOCTL_RELEASE_CAN_BUFFER	(SIOCDEVPRIVATE + 0)
#define IOCTL_ENABLE_BUFFERING		(SIOCDEVPRIVATE + 1)
#define IOCTL_ADD_FRAME_FILTER		(SIOCDEVPRIVATE + 2)
#define IOCTL_REMOVE_FRAME_FILTER	(SIOCDEVPRIVATE + 3)
#define IOCTL_DISABLE_BUFFERING		(SIOCDEVPRIVATE + 5)
#define IOCTL_DISABLE_ALL_BUFFERING	(SIOCDEVPRIVATE + 6)

struct can_fw_resp {
	u8 maj;
	u8 min;
	u8 ver;
} __packed;

struct can_write_req {
	u32 ts;
	u32 mid;
	u8 dlc;
	u8 data[];
} __packed;

struct can_write_resp {
	u8 err;
} __packed;

struct can_receive_frame {
	u32 ts;
	u32 mid;
	u8 dlc;
	u8 data[];
} __packed;

struct can_add_filter_req {
	u8 can_if;
	u32 mid;
	u32 mask;
	u8 type;
} __packed;

static struct can_bittiming_const k61_bittiming_const = {
	.name = "k61",
	.tseg1_min = 4,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 4,
	.brp_max = 1023,
	.brp_inc = 1,
};

struct k61_add_can_buffer {
	u8 can_if;
	u32 mid;
	u32 mask;
} __packed;

struct k61_delete_can_buffer {
	u8 can_if;
	u32 mid;
	u32 mask;
} __packed;

static int k61_rx_message(struct k61_can *priv_data);

static irqreturn_t k61_irq(int irq, void *priv)
{
	struct k61_can *priv_data = priv;

	LOGDI("k61_irq\n");
	k61_rx_message(priv_data);
	return IRQ_HANDLED;
}

static void k61_frame_error(struct k61_can *priv_data,
			    struct can_receive_frame *frame)
{
	struct can_frame *cf;
	struct sk_buff *skb;
	struct net_device *netdev;

	netdev = priv_data->netdev;
	skb = alloc_can_err_skb(netdev, &cf);
	if (skb == NULL) {
		LOGDE("skb alloc failed\n");
		return;
	}

	cf->can_id |= CAN_ERR_BUSERROR;
	cf->data[2] |= CAN_ERR_PROT_FORM;
	netdev->stats.rx_errors++;
	netif_rx(skb);
	netdev->stats.rx_packets++;
	netdev->stats.rx_bytes += cf->can_dlc;
}

static void k61_receive_frame(struct k61_can *priv_data,
			      struct can_receive_frame *frame)
{
	struct can_frame *cf;
	struct sk_buff *skb;
	struct skb_shared_hwtstamps *skt;
	struct timeval tv;
	static int msec;
	struct net_device *netdev;
	int i;

	if (frame->dlc > 8) {
		LOGDE("can rx frame error\n");
		k61_frame_error(priv_data, frame);
		return;
	}

	netdev = priv_data->netdev;
	skb = alloc_can_skb(netdev, &cf);
	if (skb == NULL) {
		LOGDE("skb alloc failed\n");
		return;
	}

	LOGDI("rcv frame %d %x %d %x %x %x %x %x %x %x %x\n",
	      frame->ts, frame->mid, frame->dlc, frame->data[0],
	      frame->data[1], frame->data[2], frame->data[3], frame->data[4],
	      frame->data[5], frame->data[6], frame->data[7]);
	cf->can_id = le32_to_cpu(frame->mid);
	cf->can_dlc = get_can_dlc(frame->dlc);

	for (i = 0; i < cf->can_dlc; i++)
		cf->data[i] = frame->data[i];

	msec = le32_to_cpu(frame->ts);
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec - tv.tv_sec * 1000) * 1000;
	skt = skb_hwtstamps(skb);
	skt->hwtstamp = timeval_to_ktime(tv);
	LOGDI("  hwtstamp %lld\n", ktime_to_ms(skt->hwtstamp));
	skb->tstamp = timeval_to_ktime(tv);
	netif_rx(skb);
	netdev->stats.rx_packets++;
	netdev->stats.rx_bytes += cf->can_dlc;
}

static void k61_process_response(struct k61_can *priv_data,
				 struct spi_miso *resp)
{
	int ret = 0;
	LOGDI("<%x %2d [%d]\n", resp->cmd, resp->len, resp->seq);
	if (resp->cmd == CMD_CAN_RECEIVE_FRAME) {
		struct can_receive_frame *frame =
				(struct can_receive_frame *)&resp->data;
		k61_receive_frame(priv_data, frame);
	} else if (resp->cmd  == CMD_GET_FW_VERSION) {
		struct can_fw_resp *fw_resp = (struct can_fw_resp *)resp->data;

		dev_info(&priv_data->spidev->dev, "fw %d.%d.%d",
			 fw_resp->maj, fw_resp->min, fw_resp->ver);
	}

	if (resp->cmd == priv_data->wait_cmd) {
		priv_data->cmd_result = ret;
		complete(&priv_data->response_completion);
	}
}

static void k61_process_rx(struct k61_can *priv_data, char *rx_buf)
{
	struct spi_miso *resp;
	int length_processed = 0, actual_length = priv_data->xfer_length;

	while (length_processed < actual_length) {
		int length_left = actual_length - length_processed;
		int length = 0; /* length of consumed chunk */
		void *data;

		data = rx_buf + length_processed;
		resp = (struct spi_miso *)data;

		if (resp->cmd == 0) {
			/* special case. ignore cmd==0 */
			length_processed += 1;
			continue;
		}

		LOGDI("processing. p %d -> l %d (t %d)\n",
		      length_processed, length_left, priv_data->xfer_length);
		length = resp->len + sizeof(*resp);

		if (length <= length_left) {
			k61_process_response(priv_data, resp);
			length_processed += length;
		} else {
			/* Incomplete command */
			break;
		}
	}
}

static int k61_do_spi_transaction(struct k61_can *priv_data)
{
	struct spi_device *spi;
	struct spi_transfer *xfer;
	struct spi_message *msg;
	int ret;

	spi = priv_data->spidev;
	msg = devm_kzalloc(&spi->dev, sizeof(*msg), GFP_KERNEL);
	xfer = devm_kzalloc(&spi->dev, sizeof(*xfer), GFP_KERNEL);
	if (xfer == 0 || msg == 0)
		return -ENOMEM;
	spi_message_init(msg);

	spi_message_add_tail(xfer, msg);
	xfer->tx_buf = priv_data->tx_buf;
	xfer->rx_buf = priv_data->rx_buf;
	xfer->len = XFER_BUFFER_SIZE;
	xfer->bits_per_word = priv_data->bits_per_word;

	ret = spi_sync(spi, msg);
	LOGDI("spi_sync ret %d\n", ret);

	if (ret == 0) {
		devm_kfree(&spi->dev, msg);
		devm_kfree(&spi->dev, xfer);
		k61_process_rx(priv_data, priv_data->rx_buf);
	}
	return ret;
}

static int k61_rx_message(struct k61_can *priv_data)
{
	char *tx_buf, *rx_buf;
	int ret;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	ret = k61_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int k61_query_firmware_version(struct k61_can *priv_data)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_GET_FW_VERSION;
	req->len = 0;
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	priv_data->wait_cmd = CMD_GET_FW_VERSION;
	priv_data->cmd_result = -1;
	reinit_completion(&priv_data->response_completion);

	ret = k61_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0) {
		wait_for_completion_interruptible_timeout(
				&priv_data->response_completion, 0.001 * HZ);
		ret = priv_data->cmd_result;
	}

	return ret;
}

static int k61_can_write(struct k61_can *priv_data, struct can_frame *cf)
{
	char *tx_buf, *rx_buf;
	int ret, i;
	struct spi_mosi *req;
	struct can_write_req *req_d;
	struct net_device *netdev;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_CAN_SEND_FRAME;
	req->len = sizeof(struct can_write_req) + 8;
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	req_d = (struct can_write_req *)req->data;
	req_d->mid = cf->can_id;
	req_d->dlc = cf->can_dlc;
	for (i = 0; i < cf->can_dlc; i++)
		req_d->data[i] = cf->data[i];

	ret = k61_do_spi_transaction(priv_data);
	netdev = priv_data->netdev;
	netdev->stats.tx_packets++;
	netdev->stats.tx_bytes += cf->can_dlc;
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int k61_netdev_open(struct net_device *netdev)
{
	int err;

	LOGNI("Open");
	err = open_candev(netdev);
	if (err)
		return err;

	netif_start_queue(netdev);

	return 0;
}

static int k61_netdev_close(struct net_device *netdev)
{
	LOGNI("Close");

	netif_stop_queue(netdev);
	close_candev(netdev);
	return 0;
}

static void k61_send_can_frame(struct work_struct *ws)
{
	struct k61_tx_work *tx_work;
	struct can_frame *cf;
	struct k61_can *priv_data;
	struct net_device *netdev;
	struct k61_netdev_privdata *netdev_priv_data;

	tx_work = container_of(ws, struct k61_tx_work, work);
	netdev = tx_work->netdev;
	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->k61_can;
	LOGDI("send_can_frame ws %p\n", ws);
	LOGDI("send_can_frame tx %p\n", tx_work);

	cf = (struct can_frame *)tx_work->skb->data;
	k61_can_write(priv_data, cf);

	dev_kfree_skb(tx_work->skb);
	kfree(tx_work);
}

static int k61_frame_filter(struct net_device *netdev,
			    struct ifreq *ifr, int cmd)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;
	struct can_add_filter_req *add_filter;
	struct can_add_filter_req *filter_request;
	struct k61_can *priv_data;
	struct k61_netdev_privdata *netdev_priv_data;
	struct spi_device *spi;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->k61_can;
	spi = priv_data->spidev;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	if (ifr == NULL)
		return -EINVAL;

	filter_request =
		devm_kzalloc(&spi->dev, sizeof(struct can_add_filter_req),
			     GFP_KERNEL);
	if (!filter_request)
		return -ENOMEM;

	if (copy_from_user(filter_request, ifr->ifr_data,
			   sizeof(struct can_add_filter_req))) {
		devm_kfree(&spi->dev, filter_request);
		return -EFAULT;
	}

	req = (struct spi_mosi *)tx_buf;
	if (IOCTL_ADD_FRAME_FILTER == cmd)
		req->cmd = CMD_CAN_ADD_FILTER;
	else
		req->cmd = CMD_CAN_REMOVE_FILTER;

	req->len = sizeof(struct can_add_filter_req);
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	add_filter = (struct can_add_filter_req *)req->data;
	add_filter->can_if = filter_request->can_if;
	add_filter->mid = filter_request->mid;
	add_filter->mask = filter_request->mask;

	ret = k61_do_spi_transaction(priv_data);
	devm_kfree(&spi->dev, filter_request);
	mutex_unlock(&priv_data->spi_lock);
	return ret;
}

static netdev_tx_t k61_netdev_start_xmit(
		struct sk_buff *skb, struct net_device *netdev)
{
	struct k61_netdev_privdata *netdev_priv_data = netdev_priv(netdev);
	struct k61_can *priv_data = netdev_priv_data->k61_can;
	struct k61_tx_work *tx_work;

	LOGNI("netdev_start_xmit");
	if (can_dropped_invalid_skb(netdev, skb)) {
		LOGNE("Dropping invalid can frame\n");
		return NETDEV_TX_OK;
	}
	tx_work = kzalloc(sizeof(*tx_work), GFP_ATOMIC);
	if (tx_work == 0)
		return NETDEV_TX_OK;
	INIT_WORK(&tx_work->work, k61_send_can_frame);
	tx_work->netdev = netdev;
	tx_work->skb = skb;
	queue_work(priv_data->tx_wq, &tx_work->work);

	return NETDEV_TX_OK;
}

static int k61_send_release_can_buffer_cmd(struct net_device *netdev)
{
	struct k61_can *priv_data;
	struct k61_netdev_privdata *netdev_priv_data;
	struct spi_device *spi;
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->k61_can;
	spi = priv_data->spidev;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_CAN_RELEASE_BUFFER;
	req->len = 0;
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	ret = k61_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);
	return ret;
}

static int k61_remove_all_buffering(struct net_device *netdev)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;
	struct k61_can *priv_data;
	struct k61_netdev_privdata *netdev_priv_data;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->k61_can;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_CAN_DATA_BUFF_REMOVE_ALL;
	req->len = 0;
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	priv_data->wait_cmd = req->cmd;
	priv_data->cmd_result = -1;
	reinit_completion(&priv_data->response_completion);

	ret = k61_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0) {
		LOGDI("k61_do_blocking_ioctl ready to wait for response\n");
		/* Flash write may take some time. Hence give 2s as
		 * wait duration in the worst case. This wait time should
		 * increase if more number of frame IDs are stored in flash.
		 */
		ret = wait_for_completion_interruptible_timeout(
				&priv_data->response_completion, 2 * HZ);
		ret = priv_data->cmd_result;
	}

	return ret;
}

static int k61_convert_ioctl_cmd_to_spi_cmd(int ioctl_cmd)
{
	switch (ioctl_cmd) {
	case IOCTL_ENABLE_BUFFERING:
		return CMD_CAN_DATA_BUFF_ADD;
	case IOCTL_DISABLE_BUFFERING:
		return CMD_CAN_DATA_BUFF_REMOVE;
	}
	return -EINVAL;
}

static int k61_data_buffering(struct net_device *netdev,
			      struct ifreq *ifr, int cmd)
{
	int spi_cmd, ret;
	char *tx_buf, *rx_buf;
	struct k61_can *priv_data;
	struct spi_mosi *req;
	struct k61_netdev_privdata *netdev_priv_data;
	struct k61_add_can_buffer *enable_buffering;
	struct k61_add_can_buffer *add_request;
	struct spi_device *spi;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->k61_can;
	spi = priv_data->spidev;

	mutex_lock(&priv_data->spi_lock);
	spi_cmd = k61_convert_ioctl_cmd_to_spi_cmd(cmd);
	if (spi_cmd < 0) {
		LOGDE("k61_do_blocking_ioctl wrong command %d\n", cmd);
		return spi_cmd;
	}

	if (ifr == NULL)
		return -EINVAL;

	add_request = devm_kzalloc(&spi->dev, sizeof(struct k61_add_can_buffer),
				   GFP_KERNEL);
	if (!add_request)
		return -ENOMEM;

	if (copy_from_user(add_request, ifr->ifr_data,
			   sizeof(struct k61_add_can_buffer))) {
		devm_kfree(&spi->dev, add_request);
		return -EFAULT;
	}

	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = spi_cmd;
	req->len = sizeof(struct k61_add_can_buffer);
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	enable_buffering = (struct k61_add_can_buffer *)req->data;
	enable_buffering->can_if = add_request->can_if;
	enable_buffering->mid = add_request->mid;
	enable_buffering->mask = add_request->mask;

	priv_data->wait_cmd = spi_cmd;
	priv_data->cmd_result = -1;
	reinit_completion(&priv_data->response_completion);

	ret = k61_do_spi_transaction(priv_data);
	devm_kfree(&spi->dev, add_request);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0) {
		LOGDI("k61_do_blocking_ioctl ready to wait for response\n");
		/* Flash write may take some time. Hence give 400ms as
		 * wait duration in the worst case.
		 */
		ret = wait_for_completion_interruptible_timeout(
				&priv_data->response_completion, 0.4 * HZ);
		ret = priv_data->cmd_result;
	}
	return ret;
}

static int k61_netdev_do_ioctl(struct net_device *netdev,
			       struct ifreq *ifr, int cmd)
{
	struct k61_can *priv_data;
	struct k61_netdev_privdata *netdev_priv_data;
	int ret = -EINVAL;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->k61_can;
	LOGDI("k61_netdev_do_ioctl %x\n", cmd);

	switch (cmd) {
	case IOCTL_ADD_FRAME_FILTER:
	case IOCTL_REMOVE_FRAME_FILTER:
		ret = k61_frame_filter(netdev, ifr, cmd);
		break;
	case IOCTL_ENABLE_BUFFERING:
	case IOCTL_DISABLE_BUFFERING:
		ret = k61_data_buffering(netdev, ifr, cmd);
		break;
	case IOCTL_DISABLE_ALL_BUFFERING:
		ret = k61_remove_all_buffering(netdev);
		break;
	case IOCTL_RELEASE_CAN_BUFFER:
		ret = k61_send_release_can_buffer_cmd(netdev);
		break;
	}
	return ret;
}

static const struct net_device_ops k61_netdev_ops = {
		.ndo_open = k61_netdev_open,
		.ndo_stop = k61_netdev_close,
		.ndo_start_xmit = k61_netdev_start_xmit,
		.ndo_do_ioctl = k61_netdev_do_ioctl,
};

static int k61_create_netdev(struct spi_device *spi,
			     struct k61_can *priv_data)
{
	struct net_device *netdev;
	struct k61_netdev_privdata *netdev_priv_data;

	LOGDI("k61_create_netdev\n");
	netdev = alloc_candev(sizeof(*netdev_priv_data), MAX_TX_BUFFERS);
	if (!netdev) {
		LOGDE("Couldn't alloc candev\n");
		return -ENOMEM;
	}

	netdev_priv_data = netdev_priv(netdev);
	netdev_priv_data->k61_can = priv_data;

	priv_data->netdev = netdev;

	netdev->netdev_ops = &k61_netdev_ops;
	SET_NETDEV_DEV(netdev, &spi->dev);
	netdev_priv_data->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES |
						   CAN_CTRLMODE_LISTENONLY;
	netdev_priv_data->can.bittiming_const = &k61_bittiming_const;
	netdev_priv_data->can.clock.freq = K61_CLOCK;

	return 0;
}

static struct k61_can *k61_create_priv_data(struct spi_device *spi)
{
	struct k61_can *priv_data;
	int err;
	struct device *dev;

	dev = &spi->dev;
	priv_data = devm_kzalloc(dev, sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data) {
		dev_err(dev, "Couldn't alloc k61_can\n");
		return 0;
	}
	spi_set_drvdata(spi, priv_data);
	atomic_set(&priv_data->netif_queue_stop, 0);
	priv_data->spidev = spi;

	priv_data->tx_wq = alloc_workqueue("k61_tx_wq", 0, 0);
	if (!priv_data->tx_wq) {
		dev_err(dev, "Couldn't alloc workqueue\n");
		err = -ENOMEM;
		goto cleanup_privdata;
	}

	priv_data->tx_buf = devm_kzalloc(dev, XFER_BUFFER_SIZE,
								GFP_KERNEL);
	priv_data->rx_buf = devm_kzalloc(dev, XFER_BUFFER_SIZE,
								GFP_KERNEL);
	if (!priv_data->tx_buf || !priv_data->rx_buf) {
		dev_err(dev, "Couldn't alloc tx or rx buffers\n");
		err = -ENOMEM;
		goto cleanup_privdata;
	}
	priv_data->xfer_length = 0;

	mutex_init(&priv_data->spi_lock);
	atomic_set(&priv_data->msg_seq, 0);
	init_completion(&priv_data->response_completion);
	return priv_data;

cleanup_privdata:
	if (priv_data) {
		if (priv_data->tx_wq)
			destroy_workqueue(priv_data->tx_wq);
	}
	return 0;
}

static int k61_probe(struct spi_device *spi)
{
	int err, retry = 0, query_err = -1;
	struct k61_can *priv_data;
	struct device *dev;

	dev = &spi->dev;
	dev_dbg(dev, "k61_probe");

	err = spi_setup(spi);
	if (err) {
		dev_err(dev, "spi_setup failed: %d", err);
		return err;
	}

	priv_data = k61_create_priv_data(spi);
	if (!priv_data) {
		dev_err(dev, "Failed to create k61_can priv_data\n");
		err = -ENOMEM;
		return err;
	}
	dev_dbg(dev, "k61_probe created priv_data");

	err = of_property_read_u32(spi->dev.of_node, "bits-per-word",
				   &priv_data->bits_per_word);
	if (err)
		priv_data->bits_per_word = 16;

	err = of_property_read_u32(spi->dev.of_node, "reset-delay-msec",
				   &priv_data->reset_delay_msec);
	if (err)
		priv_data->reset_delay_msec = 1;

	priv_data->reset = of_get_named_gpio(spi->dev.of_node, "reset-gpio", 0);
	if (gpio_is_valid(priv_data->reset)) {
		err = gpio_request(priv_data->reset, "k61-reset");
		if (err < 0) {
			dev_err(&spi->dev,
				"failed to request gpio %d: %d\n",
				priv_data->reset, err);
			goto cleanup_candev;
		}

		gpio_direction_output(priv_data->reset, 0);
		udelay(1);
		gpio_direction_output(priv_data->reset, 1);
		msleep(priv_data->reset_delay_msec);
	}

	err = k61_create_netdev(spi, priv_data);
	if (err) {
		dev_err(dev, "Failed to create CAN device: %d", err);
		goto cleanup_candev;
	}

	err = register_candev(priv_data->netdev);
	if (err) {
		dev_err(dev, "Failed to register CAN device: %d", err);
		goto unregister_candev;
	}

	err = request_threaded_irq(spi->irq, NULL, k61_irq,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "k61", priv_data);
	if (err) {
		dev_err(dev, "Failed to request irq: %d", err);
		goto unregister_candev;
	}
	dev_dbg(dev, "Request irq %d ret %d\n", spi->irq, err);

	while ((query_err != 0) && (retry < K61_FW_QUERY_RETRY_COUNT)) {
		query_err = k61_query_firmware_version(priv_data);
		retry++;
	}

	if (query_err) {
		dev_info(dev, "K61 probe failed\n");
		err = -ENODEV;
		goto free_irq;
	}
	return 0;

free_irq:
	free_irq(spi->irq, priv_data);
unregister_candev:
	unregister_candev(priv_data->netdev);
cleanup_candev:
	if (priv_data) {
		if (priv_data->netdev)
			free_candev(priv_data->netdev);
		if (priv_data->tx_wq)
			destroy_workqueue(priv_data->tx_wq);
	}
	return err;
}

static int k61_remove(struct spi_device *spi)
{
	struct k61_can *priv_data = spi_get_drvdata(spi);

	LOGDI("k61_remove\n");
	unregister_candev(priv_data->netdev);
	free_candev(priv_data->netdev);
	destroy_workqueue(priv_data->tx_wq);
	return 0;
}

static const struct of_device_id k61_match_table[] = {
	{ .compatible = "fsl,k61" },
	{ .compatible = "nxp,mpc5746c" },
	{ }
};

#ifdef CONFIG_PM
static int k61_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);

	enable_irq_wake(spi->irq);
	return 0;
}

static int k61_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct k61_can *priv_data = spi_get_drvdata(spi);

	disable_irq_wake(spi->irq);
	k61_rx_message(priv_data);
	return 0;
}

static const struct dev_pm_ops k61_dev_pm_ops = {
	.suspend	= k61_suspend,
	.resume		= k61_resume,
};
#endif

static struct spi_driver k61_driver = {
	.driver = {
		.name = "k61",
		.of_match_table = k61_match_table,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &k61_dev_pm_ops,
#endif
	},
	.probe = k61_probe,
	.remove = k61_remove,
};
module_spi_driver(k61_driver);

MODULE_DESCRIPTION("Freescale K61 SPI-CAN module");
MODULE_LICENSE("GPL v2");
