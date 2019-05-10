/*
 * Copyright (c) 2016, Linaro Ltd
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <microvisor/microvisor.h>

#include "qrtr.h"

#define FIFO_MAGIC_KEY 0x24495043 /* "$IPC" */
#define FIFO_SIZE 0x4000
#define FIFO_0_START 0x1000
#define FIFO_1_START (FIFO_0_START + FIFO_SIZE)
#define FIFO_MAGIC_IDX 0x0
#define TAIL_0_IDX 0x1
#define HEAD_0_IDX 0x2
#define TAIL_1_IDX 0x3
#define HEAD_1_IDX 0x4

struct fifo_pipe {
	__le32 *tail;
	__le32 *head;

	void *fifo;
	size_t length;
};

/**
 * qrtr_fifo_xprt - qrtr FIFO transport structure
 * @ep: qrtr endpoint specific info.
 * @tx_pipe: TX FIFO specific info.
 * @rx_pipe: RX FIFO specific info.
 * @fifo_base: Base of the shared FIFO.
 * @fifo_size: FIFO Size.
 * @tx_fifo_idx: TX FIFO index.
 * @kcap: Register info to raise irq to other VM.
 */
struct qrtr_fifo_xprt {
	struct qrtr_endpoint ep;
	struct fifo_pipe tx_pipe;
	struct fifo_pipe rx_pipe;
	void *fifo_base;
	size_t fifo_size;
	int tx_fifo_idx;
	okl4_kcap_t kcap;
};

static void qrtr_fifo_raise_virq(struct qrtr_fifo_xprt *xprtp);

static size_t fifo_rx_avail(struct fifo_pipe *pipe)
{
	u32 head;
	u32 tail;

	head = le32_to_cpu(*pipe->head);
	tail = le32_to_cpu(*pipe->tail);

	if (head < tail)
		return pipe->length - tail + head;

	return head - tail;
}

static void fifo_rx_peak(struct fifo_pipe *pipe,
			 void *data, unsigned int offset, size_t count)
{
	size_t len;
	u32 tail;

	tail = le32_to_cpu(*pipe->tail);
	tail += offset;
	if (tail >= pipe->length)
		tail -= pipe->length;

	len = min_t(size_t, count, pipe->length - tail);
	if (len)
		memcpy_fromio(data, pipe->fifo + tail, len);

	if (len != count)
		memcpy_fromio(data + len, pipe->fifo, (count - len));
}

static void fifo_rx_advance(struct fifo_pipe *pipe, size_t count)
{
	u32 tail;

	tail = le32_to_cpu(*pipe->tail);

	tail += count;
	if (tail > pipe->length)
		tail -= pipe->length;

	*pipe->tail = cpu_to_le32(tail);
}

static size_t fifo_tx_avail(struct fifo_pipe *pipe)
{
	u32 head;
	u32 tail;
	u32 avail;

	 head = le32_to_cpu(*pipe->head);
	 tail = le32_to_cpu(*pipe->tail);

	if (tail <= head)
		avail = pipe->length - head + tail;
	else
		avail = tail - head;

	return avail;
}

static void fifo_tx_write(struct fifo_pipe *pipe,
			  const void *data, size_t count)
{
	size_t len;
	u32 head;

	head = le32_to_cpu(*pipe->head);

	len = min_t(size_t, count, pipe->length - head);
	if (len)
		memcpy_toio(pipe->fifo + head, data, len);

	if (len != count)
		memcpy_toio(pipe->fifo, data + len, count - len);

	head += count;
	if (head >= pipe->length)
		head -= pipe->length;

	/* Ensure ordering of fifo and head update */
	wmb();

	*pipe->head = cpu_to_le32(head);
}

/* from qrtr to FIFO */
static int xprt_write(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_fifo_xprt *xprtp;
	int rc;

	xprtp = container_of(ep, struct qrtr_fifo_xprt, ep);

	rc = skb_linearize(skb);
	if (rc) {
		kfree_skb(skb);
		return rc;
	}

	/* TODO: FIFO write : check if we can write full packet at one shot */
	if (fifo_tx_avail(&xprtp->tx_pipe) < skb->len) {
		pr_err("No Space in FIFO\n");
		return -EAGAIN;
	}

	fifo_tx_write(&xprtp->tx_pipe, skb->data, skb->len);
	kfree_skb(skb);

	qrtr_fifo_raise_virq(xprtp);

	return 0;
}

static void xprt_read_data(struct qrtr_fifo_xprt *xprtp)
{
	int rc;
	u32 hdr[8];
	void *data;
	size_t pkt_len;
	size_t rx_avail;
	size_t hdr_len = sizeof(hdr);

	while (fifo_rx_avail(&xprtp->rx_pipe)) {
		fifo_rx_peak(&xprtp->rx_pipe, &hdr, 0, hdr_len);
		pkt_len = qrtr_peek_pkt_size((void *)&hdr);

		if (pkt_len < 0) {
			pr_err("%s invalid pkt_len %zu\n", __func__, pkt_len);
			break;
		}

		rx_avail = fifo_rx_avail(&xprtp->rx_pipe);
		if (rx_avail < pkt_len) {
			pr_err("%s Not FULL pkt in FIFO %zu %zu\n",
			       __func__, rx_avail, pkt_len);
			break;
		}

		data = kzalloc(pkt_len, GFP_ATOMIC);
		if (!data)
			break;

		fifo_rx_peak(&xprtp->rx_pipe, data, 0, pkt_len);
		fifo_rx_advance(&xprtp->rx_pipe, pkt_len);

		rc = qrtr_endpoint_post(&xprtp->ep, data, pkt_len);
		if (rc == -EINVAL)
			pr_err("%s invalid ipcrouter packet\n", __func__);
		kfree(data);
		data = NULL;
	}
}

static void qrtr_fifo_raise_virq(struct qrtr_fifo_xprt *xprtp)
{
	okl4_error_t err;
	unsigned long payload = 0xffff;

	err = _okl4_sys_vinterrupt_raise(xprtp->kcap, payload);
}

static irqreturn_t qrtr_fifo_virq_handler(int irq, void *dev_id)
{
	xprt_read_data((struct qrtr_fifo_xprt *)dev_id);
	return IRQ_HANDLED;
}

/**
 * qrtr_fifo_config_init() - init FIFO xprt configs
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the FIFO XPRT pointer with
 * the FIFO XPRT configurations either from device tree or static arrays.
 */
static int qrtr_fifo_config_init(struct qrtr_fifo_xprt *xprtp)
{
	__le32 *descs;

	descs = xprtp->fifo_base;
	descs[FIFO_MAGIC_IDX] = FIFO_MAGIC_KEY;

	if (xprtp->tx_fifo_idx) {
		xprtp->tx_pipe.tail = &descs[TAIL_0_IDX];
		xprtp->tx_pipe.head = &descs[HEAD_0_IDX];
		xprtp->tx_pipe.fifo = xprtp->fifo_base + FIFO_0_START;
		xprtp->tx_pipe.length = FIFO_SIZE;

		xprtp->rx_pipe.tail = &descs[TAIL_1_IDX];
		xprtp->rx_pipe.head = &descs[HEAD_1_IDX];
		xprtp->rx_pipe.fifo = xprtp->fifo_base + FIFO_1_START;
		xprtp->rx_pipe.length = FIFO_SIZE;
	} else {
		xprtp->tx_pipe.tail = &descs[TAIL_1_IDX];
		xprtp->tx_pipe.head = &descs[HEAD_1_IDX];
		xprtp->tx_pipe.fifo = xprtp->fifo_base + FIFO_1_START;
		xprtp->tx_pipe.length = FIFO_SIZE;

		xprtp->rx_pipe.tail = &descs[TAIL_0_IDX];
		xprtp->rx_pipe.head = &descs[HEAD_0_IDX];
		xprtp->rx_pipe.fifo = xprtp->fifo_base + FIFO_0_START;
		xprtp->rx_pipe.length = FIFO_SIZE;
	}

	/* Reset respective index */
	*xprtp->tx_pipe.head = 0;
	*xprtp->rx_pipe.tail = 0;

	return 0;
}

/**
 * qrtr_fifo_xprt_probe() - Probe an FIFO xprt
 *
 * @pdev: Platform device corresponding to FIFO xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to an FIFO transport.
 */
static int qrtr_fifo_xprt_probe(struct platform_device *pdev)
{
	int irq;
	int ret;
	struct resource *r;
	struct device *parent;
	struct qrtr_fifo_xprt *xprtp;
	struct device_node *ipc_irq_np;
	struct device_node *ipc_shm_np;
	struct platform_device *ipc_shm_dev;

	xprtp = devm_kzalloc(&pdev->dev, sizeof(*xprtp), GFP_KERNEL);
	if (!xprtp)
		return -ENOMEM;

	parent = &pdev->dev;
	ipc_irq_np = parent->of_node;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	ret = devm_request_irq(parent, irq, qrtr_fifo_virq_handler,
			       IRQF_TRIGGER_RISING, dev_name(parent),
			       xprtp);
	if (ret < 0)
		return -ENODEV;

	/* this kcap is required to raise VIRQ */
	ret = of_property_read_u32(ipc_irq_np, "reg", &xprtp->kcap);
	if (ret < 0)
		return -ENODEV;

	ipc_shm_np = of_parse_phandle(ipc_irq_np, "qcom,ipc-shm", 0);
	if (!ipc_shm_np)
		return -ENODEV;

	ipc_shm_dev = of_find_device_by_node(ipc_shm_np);
	if (!ipc_shm_dev)
		return -ENODEV;

	r = platform_get_resource(ipc_shm_dev, IORESOURCE_MEM, 0);
	if (!r) {
		pr_err("%s failed to get shared FIFO\n", __func__);
		return -ENODEV;
	}

	xprtp->tx_fifo_idx = of_property_read_bool(ipc_shm_np,
						   "qcom,tx-is-first");

	xprtp->fifo_size = resource_size(r);
	xprtp->fifo_base = devm_ioremap_nocache(&pdev->dev, r->start,
						resource_size(r));
	if (!xprtp->fifo_base) {
		pr_err("%s ioreamp_nocache() failed\n", __func__);
		return -ENOMEM;
	}

	ret = qrtr_fifo_config_init(xprtp);
	if (ret) {
		pr_err("%s init failed ret[%d]\n", __func__, ret);
		return ret;
	}

	xprtp->ep.xmit = xprt_write;
	ret = qrtr_endpoint_register(&xprtp->ep, QRTR_EP_NID_AUTO, false);
	if (ret)
		return ret;

	if (fifo_rx_avail(&xprtp->rx_pipe))
		xprt_read_data(xprtp);

	return 0;
}

static const struct of_device_id qrtr_fifo_xprt_match_table[] = {
	{ .compatible = "qcom,ipcr-fifo-xprt" },
	{},
};

static struct platform_driver qrtr_fifo_xprt_driver = {
	.probe = qrtr_fifo_xprt_probe,
	.driver = {
		.name = "qcom_fifo_qrtr",
		.owner = THIS_MODULE,
		.of_match_table = qrtr_fifo_xprt_match_table,
	 },
};

static int __init qrtr_fifo_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&qrtr_fifo_xprt_driver);
	if (rc) {
		pr_err("%s: driver register failed %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

module_init(qrtr_fifo_xprt_init);
MODULE_DESCRIPTION("QTI IPC-router FIFO XPRT");
MODULE_LICENSE("GPL v2");
