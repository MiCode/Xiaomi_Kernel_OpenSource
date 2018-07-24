// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ipc_router_xprt.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <microvisor/microvisor.h>

#define MODULE_NAME "ipc_router_fifo_xprt"
#define XPRT_NAME_LEN 32

#define FIFO_MAGIC_KEY 0x24495043 /* "$IPC" */
#define FIFO_SIZE 0x4000
#define FIFO_0_START 0x1000
#define FIFO_1_START (FIFO_0_START + FIFO_SIZE)
#define FIFO_MAGIC_IDX 0x0
#define TAIL_0_IDX 0x1
#define HEAD_0_IDX 0x2
#define TAIL_1_IDX 0x3
#define HEAD_1_IDX 0x4

struct msm_ipc_pipe {
	__le32 *tail;
	__le32 *head;

	void *fifo;
	size_t length;
};

/**
 * ipcr_fifo_xprt - IPC Router's FIFO XPRT structure
 * @xprt: IPC Router XPRT structure to contain XPRT specific info.
 * @tx_pipe: TX FIFO specific info.
 * @rx_pipe: RX FIFO specific info.
 * @fifo_xprt_wq: Workqueue to queue read & other XPRT related works.
 * @in_pkt: Pointer to any partially read packet.
 * @read_work: Read Work to perform read operation from SMD.
 * @sft_close_complete: Variable to indicate completion of SSR handling
 *                      by IPC Router.
 * @xprt_version: IPC Router header version supported by this XPRT.
 * @driver: Platform drivers register by this XPRT.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 */
struct ipcr_fifo_xprt {
	struct msm_ipc_router_xprt xprt;
	struct msm_ipc_pipe tx_pipe;
	struct msm_ipc_pipe rx_pipe;
	struct workqueue_struct *xprt_wq;
	struct rr_packet *in_pkt;
	struct delayed_work read_work;
	struct completion sft_close_complete;
	unsigned int xprt_version;
	struct platform_driver driver;
	char xprt_name[XPRT_NAME_LEN];
	void *fifo_base;
	size_t fifo_size;
	int tx_fifo_idx;
	okl4_kcap_t kcap;
};

static void xprt_read_data(struct work_struct *work);
static void ipcr_fifo_raise_virq(struct ipcr_fifo_xprt *xprtp);

static size_t fifo_rx_avail(struct msm_ipc_pipe *pipe)
{
	u32 head;
	u32 tail;

	head = le32_to_cpu(*pipe->head);
	tail = le32_to_cpu(*pipe->tail);

	if (head < tail)
		return pipe->length - tail + head;

	return head - tail;
}

static void fifo_rx_peak(struct msm_ipc_pipe *pipe,
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

static void fifo_rx_advance(struct msm_ipc_pipe *pipe, size_t count)
{
	u32 tail;

	tail = le32_to_cpu(*pipe->tail);

	tail += count;
	if (tail > pipe->length)
		tail -= pipe->length;

	*pipe->tail = cpu_to_le32(tail);
}

static size_t fifo_tx_avail(struct msm_ipc_pipe *pipe)
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

static void fifo_tx_write(struct msm_ipc_pipe *pipe,
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

/**
 * set_xprt_version() - Set IPC Router header version in the transport
 * @xprt: Reference to the transport structure.
 * @version: The version to be set in transport.
 */
static void set_xprt_version(struct msm_ipc_router_xprt *xprt,
			     unsigned int version)
{
	struct ipcr_fifo_xprt *xprtp;

	if (!xprt)
		return;
	xprtp = container_of(xprt, struct ipcr_fifo_xprt, xprt);
	xprtp->xprt_version = version;
}

static int get_xprt_version(struct msm_ipc_router_xprt *xprt)
{
	struct ipcr_fifo_xprt *xprtp;

	if (!xprt)
		return -EINVAL;
	xprtp = container_of(xprt, struct ipcr_fifo_xprt, xprt);
	return (int)xprtp->xprt_version;
}

static int get_xprt_option(struct msm_ipc_router_xprt *xprt)
{
	/* fragmented data is NOT supported */
	return 0;
}

static int xprt_close(struct msm_ipc_router_xprt *xprt)
{
	return 0;
}

static void xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct ipcr_fifo_xprt *xprtp;

	if (!xprt)
		return;

	xprtp = container_of(xprt, struct ipcr_fifo_xprt, xprt);
	complete_all(&xprtp->sft_close_complete);
}

static int xprt_write(void *data, uint32_t len,
		      struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *skb;
	struct ipcr_fifo_xprt *xprtp;

	xprtp = container_of(xprt, struct ipcr_fifo_xprt, xprt);

	if (!pkt)
		return -EINVAL;

	if (!len || pkt->length != len)
		return -EINVAL;

	/* TODO: FIFO write : check if we can write full packet at one shot */
	if (skb_queue_len(pkt->pkt_fragment_q) != 1) {
		pr_err("IPC router core is given fragmented data\n");
		return -EINVAL;
	}
	if (fifo_tx_avail(&xprtp->tx_pipe) < len) {
		pr_err("No Space in FIFO\n");
		return -EAGAIN;
	}

	skb_queue_walk(pkt->pkt_fragment_q, skb) {
		fifo_tx_write(&xprtp->tx_pipe, skb->data, skb->len);
	}

	ipcr_fifo_raise_virq(xprtp);

	return len;
}

static void xprt_read_data(struct work_struct *work)
{
	void *data;
	size_t hdr_len;
	size_t rx_avail;
	size_t pkt_len;
	struct rr_header_v1 hdr;
	struct sk_buff *ipc_rtr_pkt;
	struct ipcr_fifo_xprt *xprtp;
	struct delayed_work *rwork = to_delayed_work(work);

	xprtp = container_of(rwork, struct ipcr_fifo_xprt, read_work);

	hdr_len = sizeof(struct rr_header_v1);
	while (1) {
		rx_avail = fifo_rx_avail(&xprtp->rx_pipe);
		if (!rx_avail)
			break;

		fifo_rx_peak(&xprtp->rx_pipe, &hdr, 0, hdr_len);
		pkt_len = ipc_router_peek_pkt_size((char *)&hdr);

		if (pkt_len < 0) {
			pr_err("%s invalid pkt_len %zu\n", __func__, pkt_len);
			break;
		}
		if (!xprtp->in_pkt) {
			xprtp->in_pkt = create_pkt(NULL);
			if (!xprtp->in_pkt)
				break;
		}
		ipc_rtr_pkt = alloc_skb(pkt_len, GFP_KERNEL);
		if (!ipc_rtr_pkt) {
			release_pkt(xprtp->in_pkt);
			xprtp->in_pkt = NULL;
			break;
		}
		data = skb_put(ipc_rtr_pkt, pkt_len);
		do {
			rx_avail = fifo_rx_avail(&xprtp->rx_pipe);
			if (rx_avail >= pkt_len) {
				fifo_rx_peak(&xprtp->rx_pipe, data, 0, pkt_len);
				fifo_rx_advance(&xprtp->rx_pipe, pkt_len);
				break;
			}
			pr_debug("%s wait for FULL PKT [avail: len][%zu:%zu]\n",
				 __func__, rx_avail, pkt_len);
			/* wait for complete packet written into FIFO */
			msleep(20);
		} while (1);

		skb_queue_tail(xprtp->in_pkt->pkt_fragment_q, ipc_rtr_pkt);
		xprtp->in_pkt->length = pkt_len;
		msm_ipc_router_xprt_notify(&xprtp->xprt,
					   IPC_ROUTER_XPRT_EVENT_DATA,
					   (void *)xprtp->in_pkt);
		release_pkt(xprtp->in_pkt);
		xprtp->in_pkt = NULL;
	}
}

static void ipcr_fifo_raise_virq(struct ipcr_fifo_xprt *xprtp)
{
	okl4_error_t err;
	unsigned long payload = 0xffff;

	err = _okl4_sys_vinterrupt_raise(xprtp->kcap, payload);
}

static irqreturn_t ipcr_fifo_virq_handler(int irq, void *dev_id)
{
	struct ipcr_fifo_xprt *xprtp = dev_id;

	queue_delayed_work(xprtp->xprt_wq, &xprtp->read_work, 0);
	return IRQ_HANDLED;
}

/**
 * ipcr_fifo_config_init() - init FIFO xprt configs
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the FIFO XPRT pointer with
 * the FIFO XPRT configurations either from device tree or static arrays.
 */
static int ipcr_fifo_config_init(struct ipcr_fifo_xprt *xprtp)
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

	xprtp->xprt.link_id = 1;
	xprtp->xprt_version = 1;

	strlcpy(xprtp->xprt_name, "IPCR_FIFO_XPRT", XPRT_NAME_LEN);
	xprtp->xprt.name = xprtp->xprt_name;

	xprtp->xprt.set_version = set_xprt_version;
	xprtp->xprt.get_version = get_xprt_version;
	xprtp->xprt.get_option = get_xprt_option;
	xprtp->xprt.read_avail = NULL;
	xprtp->xprt.read = NULL;
	xprtp->xprt.write_avail = NULL;
	xprtp->xprt.write = xprt_write;
	xprtp->xprt.close = xprt_close;
	xprtp->xprt.sft_close_done = xprt_sft_close_done;
	xprtp->xprt.priv = NULL;

	xprtp->in_pkt = NULL;
	xprtp->xprt_wq = create_singlethread_workqueue(xprtp->xprt_name);
	if (!xprtp->xprt_wq)
		return -EFAULT;

	INIT_DELAYED_WORK(&xprtp->read_work, xprt_read_data);

	msm_ipc_router_xprt_notify(&xprtp->xprt,
				   IPC_ROUTER_XPRT_EVENT_OPEN,
				   NULL);

	if (fifo_rx_avail(&xprtp->rx_pipe))
		queue_delayed_work(xprtp->xprt_wq, &xprtp->read_work, 0);

	return 0;
}

/**
 * ipcr_fifo_xprt_probe() - Probe an FIFO xprt
 *
 * @pdev: Platform device corresponding to FIFO xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to an FIFO transport.
 */
static int ipcr_fifo_xprt_probe(struct platform_device *pdev)
{
	int irq;
	int ret;
	struct resource *r;
	struct device *parent;
	struct ipcr_fifo_xprt *xprtp;
	struct device_node *ipc_irq_np;
	struct device_node *ipc_shm_np;
	struct platform_device *ipc_shm_dev;

	xprtp = devm_kzalloc(&pdev->dev, sizeof(*xprtp), GFP_KERNEL);
	if (IS_ERR_OR_NULL(xprtp))
		return -ENOMEM;

	parent = &pdev->dev;
	ipc_irq_np = parent->of_node;

	irq = platform_get_irq(pdev, 0);

	if (irq >= 0) {
		ret = devm_request_irq(parent, irq, ipcr_fifo_virq_handler,
				       IRQF_TRIGGER_RISING, dev_name(parent),
				       xprtp);
		if (ret < 0)
			return -ENODEV;
	}

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

	ret = ipcr_fifo_config_init(xprtp);
	if (ret) {
		IPC_RTR_ERR("%s init failed ret[%d]\n", __func__, ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id ipcr_fifo_xprt_match_table[] = {
	{ .compatible = "qcom,ipcr-fifo-xprt" },
	{},
};

static struct platform_driver ipcr_fifo_xprt_driver = {
	.probe = ipcr_fifo_xprt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ipcr_fifo_xprt_match_table,
	 },
};

static int __init ipcr_fifo_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&ipcr_fifo_xprt_driver);
	if (rc) {
		IPC_RTR_ERR("%s: driver register failed %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

module_init(ipcr_fifo_xprt_init);
MODULE_DESCRIPTION("IPC Router FIFO XPRT");
MODULE_LICENSE("GPL v2");
