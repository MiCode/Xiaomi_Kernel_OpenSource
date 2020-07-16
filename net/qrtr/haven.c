// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/haven/hh_rm_drv.h>
#include <linux/haven/hh_dbl.h>
#include <soc/qcom/secure_buffer.h>
#include "qrtr.h"

#define HAVEN_MAGIC_KEY	0x24495043 /* "$IPC" */
#define FIFO_SIZE	0x4000
#define FIFO_0_START	0x1000
#define FIFO_1_START	(FIFO_0_START + FIFO_SIZE)
#define HAVEN_MAGIC_IDX	0x0
#define TAIL_0_IDX	0x1
#define HEAD_0_IDX	0x2
#define TAIL_1_IDX	0x3
#define HEAD_1_IDX	0x4
#define QRTR_DBL_MASK	0x1

struct haven_pipe {
	__le32 *tail;
	__le32 *head;

	void *fifo;
	size_t length;
};

/**
 * qrtr_haven_dev - qrtr haven transport structure
 * @ep: qrtr endpoint specific info.
 * @dev: device from platform_device.
 * @buf: buf for reading from fifo.
 * @res: resource of reserved mem region
 * @memparcel: memparcel handle returned from sharing mem
 * @base: Base of the shared fifo.
 * @size: fifo size.
 * @master: primary vm indicator.
 * @peer_name: name of vm peer.
 * @rm_nb: notifier block for vm status from rm
 * @label: label for haven resources
 * @tx_dbl: doorbell for tx notifications.
 * @rx_dbl: doorbell for rx notifications.
 * @tx_pipe: TX haven specific info.
 * @rx_pipe: RX haven specific info.
 */
struct qrtr_haven_dev {
	struct qrtr_endpoint ep;
	struct device *dev;
	void *buf;

	struct resource res;
	u32 memparcel;
	void *base;
	size_t size;
	bool master;
	u32 peer_name;
	struct notifier_block rm_nb;

	u32 label;
	void *tx_dbl;
	void *rx_dbl;
	struct work_struct work;

	struct haven_pipe tx_pipe;
	struct haven_pipe rx_pipe;
};

static void qrtr_haven_read(struct qrtr_haven_dev *qdev);

static void qrtr_haven_kick(struct qrtr_haven_dev *qdev)
{
	hh_dbl_flags_t dbl_mask = QRTR_DBL_MASK;
	int ret;

	ret = hh_dbl_send(qdev->tx_dbl, &dbl_mask, HH_DBL_NONBLOCK);
	if (ret) {
		dev_err(qdev->dev, "failed to raise doorbell %d\n", ret);
		if (!qdev->master)
			schedule_work(&qdev->work);
	}
}

static void qrtr_haven_retry_work(struct work_struct *work)
{
	struct qrtr_haven_dev *qdev = container_of(work, struct qrtr_haven_dev,
						   work);
	hh_dbl_flags_t dbl_mask = QRTR_DBL_MASK;

	hh_dbl_send(qdev->tx_dbl, &dbl_mask, 0);
}

static void qrtr_haven_cb(int irq, void *data)
{
	qrtr_haven_read((struct qrtr_haven_dev *)data);
}

static size_t haven_rx_avail(struct haven_pipe *pipe)
{
	u32 head;
	u32 tail;

	head = le32_to_cpu(*pipe->head);
	tail = le32_to_cpu(*pipe->tail);

	if (head < tail)
		return pipe->length - tail + head;

	return head - tail;
}

static void haven_rx_peak(struct haven_pipe *pipe, void *data,
			  unsigned int offset, size_t count)
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

static void haven_rx_advance(struct haven_pipe *pipe, size_t count)
{
	u32 tail;

	tail = le32_to_cpu(*pipe->tail);

	tail += count;
	if (tail > pipe->length)
		tail -= pipe->length;

	*pipe->tail = cpu_to_le32(tail);
}

static size_t haven_tx_avail(struct haven_pipe *pipe)
{
	u32 avail;
	u32 head;
	u32 tail;

	head = le32_to_cpu(*pipe->head);
	tail = le32_to_cpu(*pipe->tail);

	if (tail <= head)
		avail = pipe->length - head + tail;
	else
		avail = tail - head;

	return avail;
}

static void haven_tx_write(struct haven_pipe *pipe,
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
	smp_wmb();

	*pipe->head = cpu_to_le32(head);
}

/* from qrtr to haven */
static int qrtr_haven_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_haven_dev *qdev;
	int rc;

	qdev = container_of(ep, struct qrtr_haven_dev, ep);

	rc = skb_linearize(skb);
	if (rc) {
		kfree_skb(skb);
		return rc;
	}

	if (haven_tx_avail(&qdev->tx_pipe) < skb->len) {
		pr_err("No Space in haven\n");
		return -EAGAIN;
	}

	haven_tx_write(&qdev->tx_pipe, skb->data, skb->len);
	kfree_skb(skb);

	qrtr_haven_kick(qdev);

	return 0;
}

static void qrtr_haven_read(struct qrtr_haven_dev *qdev)
{
	size_t rx_avail;
	size_t pkt_len;
	u32 hdr[8];
	int rc;
	size_t hdr_len = sizeof(hdr);

	while (haven_rx_avail(&qdev->rx_pipe)) {
		haven_rx_peak(&qdev->rx_pipe, &hdr, 0, hdr_len);
		pkt_len = qrtr_peek_pkt_size((void *)&hdr);
		if ((int)pkt_len < 0) {
			dev_err(qdev->dev, "invalid pkt_len %zu\n", pkt_len);
			break;
		}

		rx_avail = haven_rx_avail(&qdev->rx_pipe);
		if (rx_avail < pkt_len) {
			pr_err_ratelimited("Not FULL pkt in haven %zu %zu\n",
					   rx_avail, pkt_len);
			break;
		}
		haven_rx_peak(&qdev->rx_pipe, qdev->buf, 0, pkt_len);
		haven_rx_advance(&qdev->rx_pipe, pkt_len);

		rc = qrtr_endpoint_post(&qdev->ep, qdev->buf, pkt_len);
		if (rc == -EINVAL)
			dev_err(qdev->dev, "invalid ipcrouter packet\n");
	}
}

static int qrtr_haven_share_mem(struct qrtr_haven_dev *qdev,
				hh_vmid_t self, hh_vmid_t peer)
{
	u32 src_vmlist[1] = {self};
	int dst_vmlist[2] = {self, peer};
	int dst_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct hh_acl_desc *acl;
	struct hh_sgl_desc *sgl;
	int ret;

	ret = hyp_assign_phys(qdev->res.start, resource_size(&qdev->res),
			      src_vmlist, 1,
			      dst_vmlist, dst_perms, 2);
	if (ret) {
		pr_err("%s: hyp_assign_phys failed addr=%x size=%u err=%d\n",
		       __func__, qdev->res.start, qdev->size, ret);
		return ret;
	}

	acl = kzalloc(offsetof(struct hh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;
	sgl = kzalloc(offsetof(struct hh_sgl_desc, sgl_entries[1]), GFP_KERNEL);
	if (!sgl) {
		kfree(acl);
		return -ENOMEM;
	}
	acl->n_acl_entries = 2;
	acl->acl_entries[0].vmid = (u16)self;
	acl->acl_entries[0].perms = HH_RM_ACL_R | HH_RM_ACL_W;
	acl->acl_entries[1].vmid = (u16)peer;
	acl->acl_entries[1].perms = HH_RM_ACL_R | HH_RM_ACL_W;

	sgl->n_sgl_entries = 1;
	sgl->sgl_entries[0].ipa_base = qdev->res.start;
	sgl->sgl_entries[0].size = resource_size(&qdev->res);
	ret = hh_rm_mem_qcom_lookup_sgl(HH_RM_MEM_TYPE_NORMAL,
					qdev->label,
					acl, sgl, NULL,
					&qdev->memparcel);
	kfree(acl);
	kfree(sgl);

	return ret;
}

static int qrtr_haven_rm_cb(struct notifier_block *nb, unsigned long cmd,
			    void *data)
{
	struct hh_rm_notif_vm_status_payload *vm_status_payload;
	struct qrtr_haven_dev *qdev;
	hh_vmid_t peer_vmid;
	hh_vmid_t self_vmid;

	qdev = container_of(nb, struct qrtr_haven_dev, rm_nb);

	if (cmd != HH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	vm_status_payload = data;
	if (vm_status_payload->vm_status != HH_RM_VM_STATUS_READY)
		return NOTIFY_DONE;
	if (hh_rm_get_vmid(qdev->peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (hh_rm_get_vmid(HH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	if (qrtr_haven_share_mem(qdev, self_vmid, peer_vmid))
		pr_err("%s: failed to share memory\n", __func__);

	return NOTIFY_DONE;
}

/**
 * qrtr_haven_fifo_init() - init haven xprt configs
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the haven XPRT pointer with
 * the haven XPRT configurations either from device tree or static arrays.
 */
static void qrtr_haven_fifo_init(struct qrtr_haven_dev *qdev)
{
	__le32 *descs;

	if (qdev->master)
		memset(qdev->base, 0, sizeof(*descs) * 10);

	descs = qdev->base;
	descs[HAVEN_MAGIC_IDX] = HAVEN_MAGIC_KEY;

	if (qdev->master) {
		qdev->tx_pipe.tail = &descs[TAIL_0_IDX];
		qdev->tx_pipe.head = &descs[HEAD_0_IDX];
		qdev->tx_pipe.fifo = qdev->base + FIFO_0_START;
		qdev->tx_pipe.length = FIFO_SIZE;

		qdev->rx_pipe.tail = &descs[TAIL_1_IDX];
		qdev->rx_pipe.head = &descs[HEAD_1_IDX];
		qdev->rx_pipe.fifo = qdev->base + FIFO_1_START;
		qdev->rx_pipe.length = FIFO_SIZE;
	} else {
		qdev->tx_pipe.tail = &descs[TAIL_1_IDX];
		qdev->tx_pipe.head = &descs[HEAD_1_IDX];
		qdev->tx_pipe.fifo = qdev->base + FIFO_1_START;
		qdev->tx_pipe.length = FIFO_SIZE;

		qdev->rx_pipe.tail = &descs[TAIL_0_IDX];
		qdev->rx_pipe.head = &descs[HEAD_0_IDX];
		qdev->rx_pipe.fifo = qdev->base + FIFO_0_START;
		qdev->rx_pipe.length = FIFO_SIZE;
	}

	/* Reset respective index */
	*qdev->tx_pipe.head = 0;
	*qdev->rx_pipe.tail = 0;
}

static struct device_node *qrtr_haven_svm_of_parse(struct qrtr_haven_dev *qdev)
{
	const char *compat = "qcom,qrtr-haven-gen";
	struct device_node *np = NULL;
	struct device_node *shm_np;
	u32 label;
	int ret;

	while ((np = of_find_compatible_node(np, NULL, compat))) {
		ret = of_property_read_u32(np, "qcom,label", &label);
		if (ret) {
			of_node_put(np);
			continue;
		}
		if (label == qdev->label)
			break;

		of_node_put(np);
	}
	if (!np)
		return NULL;

	shm_np = of_parse_phandle(np, "memory-region", 0);
	if (!shm_np)
		dev_err(qdev->dev, "cant parse svm shared mem node!\n");

	of_node_put(np);
	return shm_np;
}

static int qrtr_haven_map_memory(struct qrtr_haven_dev *qdev)
{
	struct device *dev = qdev->dev;
	struct device_node *np;
	resource_size_t size;
	int ret;

	np = of_parse_phandle(dev->of_node, "shared-buffer", 0);
	if (!np) {
		np = qrtr_haven_svm_of_parse(qdev);
		if (!np) {
			dev_err(dev, "cant parse shared mem node!\n");
			return -EINVAL;
		}
	}

	ret = of_address_to_resource(np, 0, &qdev->res);
	of_node_put(np);
	if (ret) {
		dev_err(dev, "of_address_to_resource failed!\n");
		return -EINVAL;
	}
	size = resource_size(&qdev->res);

	qdev->base = devm_ioremap_nocache(dev, qdev->res.start, size);
	if (!qdev->base) {
		dev_err(dev, "ioremap failed!\n");
		return -ENXIO;
	}
	qdev->size = size;

	return 0;
}

/**
 * qrtr_haven_probe() - Probe a haven xprt
 *
 * @pdev: Platform device corresponding to haven xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to a haven transport.
 */
static int qrtr_haven_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct qrtr_haven_dev *qdev;
	enum hh_dbl_label dbl_label;
	int ret;

	qdev = devm_kzalloc(&pdev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;
	qdev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, qdev);

	qdev->buf = devm_kzalloc(&pdev->dev, FIFO_SIZE, GFP_KERNEL);
	if (!qdev->buf)
		return -ENOMEM;

	ret = of_property_read_u32(node, "haven-label", &qdev->label);
	if (ret) {
		dev_err(qdev->dev, "failed to read label info %d\n", ret);
		return ret;
	}
	qdev->master = of_property_read_bool(node, "qcom,master");

	ret = qrtr_haven_map_memory(qdev);
	if (ret)
		return ret;

	qrtr_haven_fifo_init(qdev);

	if (qdev->master) {
		ret = of_property_read_u32(node, "peer-name", &qdev->peer_name);
		if (ret)
			qdev->peer_name = HH_SELF_VM;

		qdev->rm_nb.notifier_call = qrtr_haven_rm_cb;
		qdev->rm_nb.priority = INT_MAX;
		hh_rm_register_notifier(&qdev->rm_nb);
	}

	dbl_label = qdev->label;
	qdev->tx_dbl = hh_dbl_tx_register(dbl_label);
	if (IS_ERR_OR_NULL(qdev->tx_dbl)) {
		ret = PTR_ERR(qdev->tx_dbl);
		dev_err(qdev->dev, "failed to get haven tx dbl %d\n", ret);
		return ret;
	}
	INIT_WORK(&qdev->work, qrtr_haven_retry_work);

	qdev->rx_dbl = hh_dbl_rx_register(dbl_label, qrtr_haven_cb, qdev);
	if (IS_ERR_OR_NULL(qdev->rx_dbl)) {
		ret = PTR_ERR(qdev->rx_dbl);
		dev_err(qdev->dev, "failed to get haven rx dbl %d\n", ret);
		goto fail_rx_dbl;
	}

	qdev->ep.xmit = qrtr_haven_send;
	ret = qrtr_endpoint_register(&qdev->ep, QRTR_EP_NET_ID_AUTO, false);
	if (ret)
		goto register_fail;

	if (haven_rx_avail(&qdev->rx_pipe))
		qrtr_haven_read(qdev);

	return 0;

register_fail:
	hh_dbl_rx_unregister(qdev->rx_dbl);
fail_rx_dbl:
	cancel_work_sync(&qdev->work);
	hh_dbl_tx_unregister(qdev->tx_dbl);

	return ret;
}

static int qrtr_haven_remove(struct platform_device *pdev)
{
	struct qrtr_haven_dev *qdev = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&qdev->work);
	hh_dbl_tx_unregister(qdev->tx_dbl);
	hh_dbl_rx_unregister(qdev->rx_dbl);

	return 0;
}

static const struct of_device_id qrtr_haven_match_table[] = {
	{ .compatible = "qcom,qrtr-haven" },
	{}
};

static struct platform_driver qrtr_haven_driver = {
	.driver = {
		.name = "qcom_haven_qrtr",
		.of_match_table = qrtr_haven_match_table,
	 },
	.probe = qrtr_haven_probe,
	.remove = qrtr_haven_remove,
};
module_platform_driver(qrtr_haven_driver);

MODULE_DESCRIPTION("QTI IPC-Router Haven interface driver");
MODULE_LICENSE("GPL v2");
