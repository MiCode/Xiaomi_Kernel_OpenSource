/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define DEBUG_RH850	0
#if DEBUG_RH850 == 1
#define LOGDI(...) dev_info(&priv_data->spidev->dev, __VA_ARGS__)
#define LOGDE(...) dev_err(&priv_data->spidev->dev, __VA_ARGS__)
#define LOGNI(...) netdev_info(priv_data->netdev, __VA_ARGS__)
#define LOGNNI(...) netdev_info(netdev, __VA_ARGS__)
#define LOGNE(...) netdev_err(priv_data->netdev, __VA_ARGS__)
#else
#define LOGDI(...)
#define LOGDE(...)
#define LOGNI(...)
#define LOGNNI(...)
#define LOGNE(...)
#endif

#define MAX_TX_BUFFERS		1
#define XFER_BUFFER_SIZE	64
#define RX_ASSEMBLY_BUFFER_SIZE	128
#define RH850_CLOCK	80000000

struct rh850_can {
	struct can_priv		can;
	struct net_device	*netdev;
	struct spi_device	*spidev;

	struct mutex spi_lock; /* SPI device lock */

	struct workqueue_struct *tx_wq;
	char *tx_buf, *rx_buf;
	int xfer_length;
	atomic_t msg_seq;

	char *assembly_buffer;
	u8 assembly_buffer_size;
	atomic_t netif_queue_stop;
};

struct rh850_tx_work {
	struct work_struct work;
	struct rh850_can *priv_data;
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

#define CMD_GET_FW_VERSION	0x81
#define CMD_CAN_SEND_FRAME	0x82
#define CMD_CAN_ADD_FILTER	0x83
#define CMD_CAN_REMOVE_FILTER	0x84
#define CMD_CAN_RECEIVE_FRAME	0x85

struct can_fw_resp {
	u8 maj;
	u8 min;
	u8 ver[32];
} __packed;

struct can_write_req {
	u8 can_if;
	u32 mid;
	u8 dlc;
	u8 data[];
} __packed;

struct can_write_resp {
	u8 err;
} __packed;

struct can_add_filter_req {
	u8 can_if;
	u32 mid;
	u32 mask;
} __packed;

struct can_add_filter_resp {
	u8 err;
} __packed;

struct can_remove_filter_req {
	u8 can_if;
	u32 mid;
	u32 mask;
} __packed;

struct can_receive_frame {
	u8 can_if;
	u32 ts;
	u32 mid;
	u8 dlc;
	u8 data[];
} __packed;

static struct can_bittiming_const rh850_bittiming_const = {
	.name = "rh850",
	.tseg1_min = 4,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 4,
	.brp_max = 1023,
	.brp_inc = 1,
};

static int rh850_rx_message(struct rh850_can *priv_data);

static irqreturn_t rh850_irq(int irq, void *priv)
{
	struct rh850_can *priv_data = priv;

	LOGDI("rh850_irq\n");
	rh850_rx_message(priv_data);
	return IRQ_HANDLED;
}

static void rh850_receive_frame(struct rh850_can *priv_data,
				struct can_receive_frame *frame)
{
	struct can_frame *cf;
	struct sk_buff *skb;
	struct skb_shared_hwtstamps *skt;
	struct timeval tv;
	static int msec;
	struct net_device *netdev;
	int i;

	netdev = priv_data->netdev;
	skb = alloc_can_skb(priv_data->netdev, &cf);
	if (skb == NULL) {
		pr_err("skb failed..frame->can %d", 0);/*(frame->can);*/
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
	priv_data->netdev->stats.rx_packets++;
}

static void rh850_process_response(struct rh850_can *priv_data,
				   struct spi_miso *resp, int length)
{
	LOGDE("<%x %2d [%d]\n", resp->cmd, resp->len, resp->seq);
	if (resp->cmd == CMD_CAN_RECEIVE_FRAME) {
		struct can_receive_frame *frame =
				(struct can_receive_frame *)&resp->data;
		if (resp->len > length) {
			LOGDE("This should never happen");
			LOGDE("process_response: Saving %d bytes\n",
			      length);
			memcpy(priv_data->assembly_buffer, (char *)resp,
			       length);
			priv_data->assembly_buffer_size = length;
		} else {
			rh850_receive_frame(priv_data, frame);
		}
	} else if (resp->cmd  == CMD_GET_FW_VERSION) {
		struct can_fw_resp *fw_resp = (struct can_fw_resp *)resp->data;
			LOGDI("data %x %d %d\n",
			      resp->cmd, resp->len, resp->seq);
		dev_info(&priv_data->spidev->dev, "fw %d.%d",
			 fw_resp->maj, fw_resp->min);
		dev_info(&priv_data->spidev->dev, "fw string %s",
			 fw_resp->ver);
	}
}

static void rh850_process_rx(struct rh850_can *priv_data, char *rx_buf)
{
	struct spi_miso *resp;
	int length_processed = 0, actual_length = priv_data->xfer_length;

	while (length_processed < actual_length) {
		int length_left = actual_length - length_processed;
		int length = 0; /* length of consumed chunk */
		void *data;

		if (priv_data->assembly_buffer_size > 0) {
			LOGDI("callback: Reassembling %d bytes\n",
			      priv_data->assembly_buffer_size);
			/* should copy just 1 byte instead, since cmd should */
			/* already been copied as being first byte */
			memcpy(priv_data->assembly_buffer +
			       priv_data->assembly_buffer_size,
			       rx_buf, 2);
			data = priv_data->assembly_buffer;
			resp = (struct spi_miso *)data;
			length = resp->len - priv_data->assembly_buffer_size;
			if (length > 0)
				memcpy(priv_data->assembly_buffer +
				       priv_data->assembly_buffer_size,
				       rx_buf, length);
			length_left += priv_data->assembly_buffer_size;
			priv_data->assembly_buffer_size = 0;
		} else {
			data = rx_buf + length_processed;
			resp = (struct spi_miso *)data;
			if (resp->cmd == 0) {
				/* special case. ignore cmd==0 */
				length_processed += 1;
				continue;
			}
			length = resp->len + sizeof(struct spi_miso);
		}
		LOGDI("processing. p %d -> l %d (t %d)\n",
		      length_processed, length_left, priv_data->xfer_length);
		length_processed += length;
		if (length_left >= sizeof(*resp) &&
		    resp->len <= length_left) {
			struct spi_miso *resp =
					(struct spi_miso *)data;
			if (resp->len < sizeof(struct spi_miso)) {
				LOGDE("Error resp->len is %d). Abort.\n",
				      resp->len);
				break;
			}
			rh850_process_response(priv_data, resp, length_left);
		} else if (length_left > 0) {
			/* Not full message. Store however much we have for */
			/* later assembly */
			LOGDI("callback: Storing %d bytes of response\n",
			      length_left);
			memcpy(priv_data->assembly_buffer, data, length_left);
			priv_data->assembly_buffer_size = length_left;
			break;
		}
	}
}

static int rh850_do_spi_transaction(struct rh850_can *priv_data)
{
	struct spi_device *spi;
	struct spi_transfer *xfer;
	struct spi_message *msg;
	int ret;

	spi = priv_data->spidev;
	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (xfer == 0 || msg == 0)
		return -ENOMEM;
	spi_message_init(msg);
	spi_message_add_tail(xfer, msg);
	xfer->tx_buf = priv_data->tx_buf;
	xfer->rx_buf = priv_data->rx_buf;
	xfer->len = priv_data->xfer_length;
	ret = spi_sync(spi, msg);
	LOGDI("spi_sync ret %d\n", ret);
	if (ret == 0)
		rh850_process_rx(priv_data, priv_data->rx_buf);
	kfree(msg);
	kfree(xfer);
	return ret;
}

static int rh850_rx_message(struct rh850_can *priv_data)
{
	char *tx_buf, *rx_buf;
	int ret;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	ret = rh850_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int rh850_query_firmware_version(struct rh850_can *priv_data)
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

	ret = rh850_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int rh850_can_write(struct rh850_can *priv_data, struct can_frame *cf)
{
	char *tx_buf, *rx_buf;
	int ret, i;
	struct spi_mosi *req;
	struct can_write_req *req_d;

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
	req_d->can_if = 0;
	req_d->mid = cf->can_id;
	req_d->dlc = cf->can_dlc;
	for (i = 0; i < cf->can_dlc; i++)
		req_d->data[i] = cf->data[i];

	ret = rh850_do_spi_transaction(priv_data);
	priv_data->netdev->stats.tx_packets++;
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int rh850_netdev_open(struct net_device *netdev)
{
	int err;

	LOGNNI("Open");
	err = open_candev(netdev);
	if (err)
		return err;

	netif_start_queue(netdev);

	return 0;
}

static int rh850_netdev_close(struct net_device *netdev)
{
	LOGNNI("Close");

	netif_stop_queue(netdev);
	close_candev(netdev);
	return 0;
}

static void rh850_send_can_frame(struct work_struct *ws)
{
	struct rh850_tx_work *tx_work;
	struct can_frame *cf;
	struct rh850_can *priv_data;

	tx_work = container_of(ws, struct rh850_tx_work, work);
	priv_data = tx_work->priv_data;
	LOGDI("send_can_frame ws %p\n", ws);
	LOGDI("send_can_frame tx %p\n", tx_work);

	cf = (struct can_frame *)tx_work->skb->data;
	rh850_can_write(priv_data, cf);

	dev_kfree_skb(tx_work->skb);
	kfree(tx_work);
}

static netdev_tx_t rh850_netdev_start_xmit(
		struct sk_buff *skb, struct net_device *netdev)
{
	struct rh850_can *priv_data = netdev_priv(netdev);
	struct rh850_tx_work *tx_work;

	LOGNI("netdev_start_xmit");
	if (can_dropped_invalid_skb(netdev, skb)) {
		pr_err("Dropping invalid can frame");
		return NETDEV_TX_OK;
	}
	tx_work = kzalloc(sizeof(*tx_work), GFP_ATOMIC);
	if (tx_work == 0)
		return NETDEV_TX_OK;
	INIT_WORK(&tx_work->work, rh850_send_can_frame);
	tx_work->netdev = netdev;
	tx_work->skb = skb;
	tx_work->priv_data = priv_data;
	queue_work(priv_data->tx_wq, &tx_work->work);

	return NETDEV_TX_OK;
}

static const struct net_device_ops rh850_netdev_ops = {
		.ndo_open = rh850_netdev_open,
		.ndo_stop = rh850_netdev_close,
		.ndo_start_xmit = rh850_netdev_start_xmit,
};

static int rh850_probe(struct spi_device *spi)
{
	int err;
	struct net_device *netdev;
	struct rh850_can *priv_data;

	err = spi_setup(spi);
	dev_info(&spi->dev, "rh850_probe");

	netdev = alloc_candev(sizeof(struct rh850_can), MAX_TX_BUFFERS);
	if (!netdev) {
		dev_err(&spi->dev, "Couldn't alloc candev\n");
		err = -ENOMEM;
		goto cleanup_candev;
	}

	priv_data = netdev_priv(netdev);
	priv_data->spidev = spi;
	priv_data->netdev = netdev;
	priv_data->assembly_buffer = kzalloc(RX_ASSEMBLY_BUFFER_SIZE,
			GFP_KERNEL);
	if (!priv_data->assembly_buffer) {
		err = -ENOMEM;
		goto cleanup_candev;
	}

	spi_set_drvdata(spi, priv_data);

	netdev->netdev_ops = &rh850_netdev_ops;

	priv_data->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES |
				      CAN_CTRLMODE_LISTENONLY;
	priv_data->can.bittiming_const = &rh850_bittiming_const;
	priv_data->can.clock.freq = RH850_CLOCK;

	priv_data->tx_buf = kzalloc(XFER_BUFFER_SIZE, GFP_KERNEL);
	priv_data->rx_buf = kzalloc(XFER_BUFFER_SIZE, GFP_KERNEL);
	if (!priv_data->tx_buf || !priv_data->rx_buf) {
		dev_err(&spi->dev, "Couldn't alloc tx or rx buffers\n");
		err = -ENOMEM;
		goto cleanup_privdata;
	}
	priv_data->xfer_length = 0;

	mutex_init(&priv_data->spi_lock);
	atomic_set(&priv_data->msg_seq, 0);

	SET_NETDEV_DEV(netdev, &spi->dev);

	priv_data->tx_wq = alloc_workqueue("rh850_tx_wq", 0, 0);
	if (!priv_data->tx_wq) {
		dev_err(&spi->dev, "Couldn't alloc workqueue\n");
		err = -ENOMEM;
		goto cleanup_privdata;
	}

	err = register_candev(netdev);
	if (err) {
		dev_err(&spi->dev, "Failed to reg.CAN device: %d", err);
		goto cleanup_privdata;
	}

	err = request_threaded_irq(spi->irq, NULL, rh850_irq,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "rh850", priv_data);
	if (err) {
		dev_err(&spi->dev, "Failed to request irq: %d", err);
		goto unregister_candev;
	}
	dev_info(&spi->dev, "Request irq %d ret %d\n", spi->irq, err);

	rh850_query_firmware_version(priv_data);
	return 0;

unregister_candev:
	unregister_candev(priv_data->netdev);
cleanup_privdata:
	if (priv_data->tx_wq)
		destroy_workqueue(priv_data->tx_wq);
	kfree(priv_data->rx_buf);
	kfree(priv_data->tx_buf);
	kfree(priv_data->assembly_buffer);
cleanup_candev:
	if (priv_data) {
		if (priv_data->netdev)
			free_candev(priv_data->netdev);
		kfree(priv_data);
	}
	return err;
}

static int rh850_remove(struct spi_device *spi)
{
	struct rh850_can *priv_data = spi_get_drvdata(spi);

	LOGDI("rh850_remove\n");
	unregister_candev(priv_data->netdev);
	destroy_workqueue(priv_data->tx_wq);
	kfree(priv_data->assembly_buffer);
	kfree(priv_data->rx_buf);
	kfree(priv_data->tx_buf);
	kfree(priv_data);
	return 0;
}

static const struct of_device_id rh850_match_table[] = {
	{ .compatible = "renesas,rh850" },
	{ }
};

static struct spi_driver rh850_driver = {
	.driver = {
		.name = "rh850",
		.of_match_table = rh850_match_table,
		.owner = THIS_MODULE,
	},
	.probe = rh850_probe,
	.remove = rh850_remove,
};
module_spi_driver(rh850_driver);

MODULE_DESCRIPTION("RH850 SPI-CAN module");
MODULE_LICENSE("GPL v2");
