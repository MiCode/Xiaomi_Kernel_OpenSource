// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/sizes.h>
#include <linux/socket.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <net/sock.h>
#include <net/af_vsock.h>

/* auto-bind range */
#define GHVST_MIN_SOCKET 0x4000
#define GHVST_MAX_SOCKET 0x7fff

#define GHVST_PROTO_VER_1 1

#define MAX_PKT_SZ  SZ_64K

/* list of ghvst devices */
static LIST_HEAD(ghvst_devs);
/* lock for qrtr_all_epts */
static DECLARE_RWSEM(ghvst_devs_lock);
/* local port allocation management */
static DEFINE_IDR(ghvst_ports);
static DEFINE_SPINLOCK(ghvst_port_lock);

enum ghvst_pkt_type {
	GHVST_TYPE_DATA = 1,
};

/* gh_transport_buf: gunyah transport buffer
 * @buf: buffer saved
 * @lock: lock for the buffer
 * @len: hdrlen + packet size
 * @copied: size of buffer copied
 * @remaining: size needed to complete the pkt_len
 * @hdr_received: true if the header is already saved, else false
 */
struct gh_transport_buf {
	void *buf;
	/* @lock: lock for the buffer */
	struct mutex lock;
	size_t len;
	size_t copied;
	size_t remaining;
	bool hdr_received;
};

/* gh_transport_device: vm devices attached to this transport
 * @dev: device from platform_device.
 * @cid: local cid
 * @peer_name: remote cid
 * @master: primary vm indicator
 * @msgq_label: msgq label
 * @msgq_hdl: msgq handle
 * @rm_nb: notifier block for vm status from rm
 * @item: list item of all vm devices
 * @tx_lock: tx lock to queue only one packet at a time
 * @rx_thread: rx thread to receive incoming packets
 */
struct gh_transport_device {
	struct device *dev;
	unsigned int cid;
	unsigned int peer_name;
	bool master;

	enum gh_msgq_label msgq_label;
	void *msgq_hdl;

	struct notifier_block rm_nb;

	struct list_head item;

	/* @tx_lock: tx lock to queue only one packet at a time */
	struct mutex tx_lock;

	struct task_struct *rx_thread;
	struct gh_transport_buf gbuf;
};

/**
 * struct ghvst_hdr - ghvst packet header
 * @version: protocol version
 * @type: packet type; one of ghvst_TYPE_*
 * @flags: Reserved for future use
 * @optlen: length of optional header data
 * @size: length of packet, excluding this header and optlen
 * @src_node_id: source cid, reserved
 * @src_port_id: source port
 * @dst_node_id: destination cid, reserved
 * @dst_port_id: destination port
 */
struct ghvst_hdr {
	u8 version;
	u8 type;
	u8 flags;
	u8 optlen;
	__le32 size;
	__le32 src_rsvd;
	__le32 src_port_id;
	__le32 dst_rsvd;
	__le32 dst_port_id;
};

struct ghvst_cb {
	u32 src_node;
	u32 src_port;
	u32 dst_node;
	u32 dst_port;

	u8 type;
};

static int ghvst_socket_init(struct vsock_sock *vsk, struct vsock_sock *psk)
{
	int rc;

	down_read(&ghvst_devs_lock);
	rc = list_empty(&ghvst_devs);
	up_read(&ghvst_devs_lock);
	if (rc) {
		pr_err("%s: Transport not available\n", __func__);
		return -ENODEV;
	}

	vsk->local_addr.svm_cid = VMADDR_CID_HOST;

	return 0;
}

static void ghvst_destruct(struct vsock_sock *vsk)
{
}

static void ghvst_port_remove(struct vsock_sock *vsk)
{
	int port = vsk->local_addr.svm_port;
	unsigned long flags;

	sock_put(sk_vsock(vsk));

	spin_lock_irqsave(&ghvst_port_lock, flags);
	idr_remove(&ghvst_ports, port);
	spin_unlock_irqrestore(&ghvst_port_lock, flags);
}

static void ghvst_release(struct vsock_sock *vsk)
{
	struct sock *sk = sk_vsock(vsk);

	if (!sk)
		return;

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);

	if (!sock_flag(sk, SOCK_ZAPPED))
		ghvst_port_remove(vsk);
}

static int ghvst_port_assign(struct vsock_sock *vsk, int *port)
{
	int rc;

	if (!*port || *port < 0) {
		rc = idr_alloc_cyclic(&ghvst_ports, vsk, GHVST_MIN_SOCKET,
				      GHVST_MAX_SOCKET + 1, GFP_ATOMIC);
		if (rc >= 0)
			*port = rc;
	} else if (*port < GHVST_MIN_SOCKET && !capable(CAP_NET_ADMIN)) {
		rc = -EACCES;
	} else {
		rc = idr_alloc_cyclic(&ghvst_ports, vsk, *port, *port + 1,
				      GFP_ATOMIC);
		if (rc >= 0)
			*port = rc;
	}

	if (rc == -ENOSPC) {
		pr_err("%s: Failed: EADDRINUSE\n", __func__);
		return -EADDRINUSE;
	} else if (rc < 0) {
		pr_err("%s: Failed: rc: %d\n", __func__, rc);
		return rc;
	}

	sock_hold(sk_vsock(vsk));
	return 0;
}

static int __ghvst_bind(struct vsock_sock *vsk,
			struct sockaddr_vm *addr,
			int zapped)
{
	struct sock *sk = sk_vsock(vsk);
	unsigned long flags;
	int port;
	int rc;

	/* rebinding ok */
	if (!zapped && addr->svm_port == vsk->local_addr.svm_port)
		return 0;

	spin_lock_irqsave(&ghvst_port_lock, flags);
	port = addr->svm_port;
	rc = ghvst_port_assign(vsk, &port);
	spin_unlock_irqrestore(&ghvst_port_lock, flags);
	pr_debug("%s: port: 0x%x\n", __func__, port);
	if (rc)
		return rc;

	if (!zapped)
		ghvst_port_remove(vsk);

	vsock_addr_init(&vsk->local_addr, VMADDR_CID_HOST, port);
	sock_reset_flag(sk, SOCK_ZAPPED);

	return 0;
}

int ghvst_dgram_bind(struct vsock_sock *vsk, struct sockaddr_vm *addr)
{
	struct sock *sk = sk_vsock(vsk);
	int rc;

	if (addr->svm_family != AF_VSOCK) {
		pr_err("%s: Failed: Invalid AF family: %d\n", __func__,
		       addr->svm_family);
		return -EINVAL;
	}

	rc = __ghvst_bind(vsk, addr, sock_flag(sk, SOCK_ZAPPED));

	return rc;
}

static struct vsock_sock *ghvst_port_lookup(int port)
{
	struct vsock_sock *vsk;
	unsigned long flags;

	spin_lock_irqsave(&ghvst_port_lock, flags);
	vsk = idr_find(&ghvst_ports, port);
	if (vsk)
		sock_hold(sk_vsock(vsk));
	spin_unlock_irqrestore(&ghvst_port_lock, flags);

	return vsk;
}

static int ghvst_dgram_post(struct gh_transport_device *gdev)
{
	struct gh_transport_buf *gbuf;
	struct vsock_sock *vsk;
	struct ghvst_hdr *hdr;
	struct sk_buff *skb;
	struct ghvst_cb *cb;
	unsigned int size;
	unsigned int len;
	u64 pl_buf = 0;
	void *data;
	int rc;

	gbuf = &gdev->gbuf;
	if (gbuf->len < sizeof(*hdr)) {
		pr_err("%s: len: %d < hdr size\n", __func__, gbuf->len);
		return -EINVAL;
	}

	len = gbuf->len - sizeof(*hdr);
	data = gbuf->buf;
	if (len <= 0 || !data) {
		pr_err("%s: EINVAL: len: %d\n", __func__, len);
		return -EINVAL;
	}

	skb = alloc_skb_with_frags(sizeof(*hdr), len, 0, &rc, GFP_ATOMIC);
	if (!skb) {
		pr_err("%s: Unable to get skb with len:%lu\n", __func__, len);
		return -ENOMEM;
	}

	skb_reserve(skb, sizeof(*hdr));
	cb = (struct ghvst_cb *)skb->cb;

	hdr = (struct ghvst_hdr *)data;

	cb->type = le32_to_cpu(hdr->type);
	cb->src_port = le32_to_cpu(hdr->src_port_id);
	cb->dst_port = le32_to_cpu(hdr->dst_port_id);
	size = le32_to_cpu(hdr->size);

	skb->data_len = size;
	skb->len = gbuf->len;
	skb_store_bits(skb, 0, data + sizeof(*hdr), size);

	skb_copy_bits(skb, 0, &pl_buf, sizeof(pl_buf));
	pr_debug("%s: RX DATA: Len:0x%x src[0x%x] dst[0x%x] [%08x %08x]\n",
		 __func__, skb->len, cb->src_port, cb->dst_port,
		(unsigned int)pl_buf, (unsigned int)(pl_buf >> 32));

	vsk = ghvst_port_lookup(cb->dst_port);
	if (!vsk) {
		pr_err("%s: no vsk for port:0x%x\n", __func__, cb->dst_port);
		goto err;
	}

	if (sock_queue_rcv_skb(sk_vsock(vsk), skb)) {
		pr_err("%s: sock_queue_rcv_skb failed\n", __func__);
		sock_put(sk_vsock(vsk));
		goto err;
	}

	sock_put(sk_vsock(vsk));
	return 0;

err:
	kfree_skb(skb);
	return -EINVAL;
}

static void reset_buf(struct gh_transport_buf *gbuf)
{
	memset(gbuf->buf, 0, MAX_PKT_SZ);
	gbuf->hdr_received = false;
	gbuf->copied = 0;
	gbuf->remaining = 0;
	gbuf->len = 0;
}

static void update_buf(void *buf, struct gh_transport_buf *gbuf, size_t len)
{
	memcpy(gbuf->buf, buf, len);
	gbuf->copied += len;
	gbuf->buf += len;
}

static void check_rx_complete(struct gh_transport_device *gdev)
{
	if (gdev->gbuf.copied == gdev->gbuf.len) {
		gdev->gbuf.buf -= gdev->gbuf.len;
		ghvst_dgram_post(gdev);
		reset_buf(&gdev->gbuf);
	}
}

static void copy_data(struct gh_transport_device *gdev, void *buf,
		      struct gh_transport_buf *gbuf, size_t len)
{
	size_t copy_len;

	copy_len = (len > gbuf->remaining) ? gbuf->remaining : len;
	update_buf(buf, gbuf, copy_len);
	gbuf->remaining = gbuf->len - gbuf->copied;
	check_rx_complete(gdev);
}

static void ghvst_process_msg(struct gh_transport_device *gdev,
			      void *buf, size_t len)
{
	struct gh_transport_buf *gbuf;
	struct ghvst_hdr hdr;
	void *head;

	gbuf = &gdev->gbuf;
	if (!gbuf)
		return;

	mutex_lock(&gbuf->lock);
	if (gbuf->hdr_received) {
		copy_data(gdev, buf, gbuf, len);
		mutex_unlock(&gbuf->lock);
		return;
	}

	update_buf(buf, gbuf, len);
	if (gbuf->copied >= sizeof(hdr)) {
		head = gbuf->buf - gbuf->copied;
		memcpy(&hdr, head, sizeof(hdr));
		if (hdr.version != GHVST_PROTO_VER_1 &&
		    hdr.type != GHVST_TYPE_DATA) {
			pr_err("%s: Incorrect info ver:%d; type:%d\n",
			       __func__, hdr.version, hdr.type);
			reset_buf(gbuf);
			mutex_unlock(&gbuf->lock);
			return;
		}
		gbuf->len = sizeof(hdr) + hdr.size;
		gbuf->hdr_received = true;
		gbuf->remaining = gbuf->len - gbuf->copied;
		check_rx_complete(gdev);
		mutex_unlock(&gbuf->lock);
		return;
	}
	mutex_unlock(&gbuf->lock);
}

static int ghvst_msgq_recv(void *data)
{
	struct gh_transport_device *gdev = data;
	struct gh_transport_buf *gbuf;
	size_t size;
	void *buf;
	int rc;

	buf = kzalloc(GH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	gbuf = &gdev->gbuf;
	if (!gbuf)
		return -EINVAL;

	while (!kthread_should_stop()) {
		rc = gh_msgq_recv(gdev->msgq_hdl, buf,
				  GH_MSGQ_MAX_MSG_SIZE_BYTES,
				  &size, GH_MSGQ_TX_PUSH);
		if (rc)
			continue;

		if (size <= 0)
			continue;

		ghvst_process_msg(gdev, buf, size);
	}
	kfree(buf);
	return 0;
}

/**
 * ghvst_dgram_dequeue() - post incoming data
 */
static int ghvst_dgram_dequeue(struct vsock_sock *vsk,
			       struct msghdr *msg, size_t len,
			       int flags)
{
	DECLARE_SOCKADDR(struct sockaddr_vm *, vm_addr, msg->msg_name);
	struct sock *sk = sk_vsock(vsk);
	struct sk_buff *skb;
	struct ghvst_cb *cb;
	size_t payload_len;
	int noblock;
	int rc = 0;

	if (sock_flag(sk, SOCK_ZAPPED)) {
		pr_err("%s: Invalid addr error\n", __func__);
		return -EADDRNOTAVAIL;
	}

	noblock = flags & MSG_DONTWAIT;

	/* Retrieve the head sk_buff from the socket's receive queue. */
	skb = skb_recv_datagram(&vsk->sk, flags & ~MSG_DONTWAIT, noblock, &rc);
	if (!skb)
		return rc;

	lock_sock(sk);
	cb = (struct ghvst_cb *)skb->cb;
	if (!cb) {
		rc = -ENOMEM;
		goto out;
	}

	payload_len = skb->data_len;
	/* Ensure the sk_buff matches the payload size claimed in the packet. */
	if (payload_len != skb->len - sizeof(struct ghvst_hdr)) {
		rc = -EINVAL;
		pr_err("%s: payload_len:%d; skb->len:%d\n",
		       __func__, payload_len, skb->len);
		goto out;
	}

	if (payload_len > len) {
		payload_len = len;
		msg->msg_flags |= MSG_TRUNC;
	}

	/* Place the datagram payload in the user's iovec. */
	rc = skb_copy_datagram_msg(skb, 0, msg, payload_len);
	if (rc < 0) {
		pr_err("%s: skb_copy_datagram_msg failed: %d\n", __func__, rc);
		goto out;
	}
	rc = payload_len;

	if (msg->msg_name) {
		vsock_addr_init(vm_addr, VMADDR_CID_HOST, cb->src_port);
		msg->msg_namelen = sizeof(*vm_addr);
	}
out:
	skb_free_datagram(&vsk->sk, skb);
	release_sock(sk);

	return rc;
}

static int ghvst_sendmsg(struct gh_transport_device *gdev,
			 void *buf, size_t len)
{
	size_t tx_len;
	int rc;

	if (!gdev || !gdev->msgq_hdl) {
		pr_err("%s: ENODEV err\n", __func__);
		return -ENODEV;
	}

	if (len <= 0 || !buf) {
		pr_err("%s: EINVAL err\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&gdev->tx_lock);
	while (len > 0) {
		tx_len = (len > GH_MSGQ_MAX_MSG_SIZE_BYTES ?
			  GH_MSGQ_MAX_MSG_SIZE_BYTES : len);

		rc = gh_msgq_send(gdev->msgq_hdl, buf, tx_len, GH_MSGQ_TX_PUSH);
		if (rc) {
			pr_err("%s: gh_msgq_send failed: %d\n", __func__, rc);
			mutex_unlock(&gdev->tx_lock);
			return rc;
		}
		len -= tx_len;
		buf = buf + tx_len;
	}
	mutex_unlock(&gdev->tx_lock);

	return rc;
}

struct gh_transport_device *get_ghvst(unsigned int cid)
{
	struct gh_transport_device *gdev = NULL;
	struct gh_transport_device *temp;

	down_read(&ghvst_devs_lock);
	list_for_each_entry(temp, &ghvst_devs, item) {
		if (temp->cid == cid) {
			gdev = temp;
			break;
		}
	}
	up_read(&ghvst_devs_lock);

	return gdev;
}

static int ghvst_dgram_enqueue(struct vsock_sock *vsk,
			       struct sockaddr_vm *remote,
			       struct msghdr *msg, size_t len)
{
	DECLARE_SOCKADDR(struct sockaddr_vm *, addr, msg->msg_name);
	struct gh_transport_device *gdev;
	struct sockaddr_vm *local_addr;
	struct ghvst_hdr *hdr;
	char *buf;
	int rc;

	local_addr = &vsk->local_addr;
	gdev = get_ghvst(local_addr->svm_cid);
	if (!gdev) {
		pr_err("%s: no gunyah transport device for [0x%x:0x%x]\n",
		       __func__, local_addr->svm_cid, local_addr->svm_port);
		return -ENXIO;
	}

	if (msg->msg_flags & MSG_DONTWAIT) {
		pr_err("%s: No support for non-blocking flag: %lu\n",
		       __func__, msg->msg_flags);
		return -EINVAL;
	}

	if (len > MAX_PKT_SZ - sizeof(*hdr)) {
		pr_err("%s: Invalid pk size: len: %lu\n", __func__, len);
		return -EMSGSIZE;
	}

	if (addr) {
		if (msg->msg_namelen < sizeof(*addr)) {
			pr_err("%s: Invalid addr\n", __func__);
			return -EINVAL;
		}

		if (addr->svm_family != AF_VSOCK) {
			pr_err("%s: Invalid sock family\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("%s: No addr\n", __func__);
		return -ENOTCONN;
	}

	/* Allocate a buffer for the user's message and our packet header. */
	buf = kmalloc(len + sizeof(*hdr), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* Populate Header */
	hdr = (struct ghvst_hdr *)buf;
	hdr->version = GHVST_PROTO_VER_1;
	hdr->type = GHVST_TYPE_DATA;
	hdr->flags = 0;
	hdr->optlen = 0;
	hdr->size = len;
	hdr->src_rsvd = 0;
	hdr->src_port_id = vsk->local_addr.svm_port;
	hdr->dst_rsvd = 0;
	hdr->dst_port_id = remote->svm_port;
	rc = memcpy_from_msg((void *)buf + sizeof(*hdr), msg, len);
	if (rc) {
		pr_err("%s failed: memcpy_from_msg rc: %d\n", __func__, rc);
		return rc;
	}

	pr_debug("TX DATA: Len:0x%x src[0x%x] dst[0x%x]\n",
		 len, hdr->src_port_id, hdr->dst_port_id);
	rc = ghvst_sendmsg(gdev, buf, len + sizeof(*hdr));
	if (rc < 0) {
		pr_err("%s: failed to send msg rc: %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static bool ghvst_allow_rsvd_cid(u32 cid)
{
	/* Allowing for cid 0 as of now as af_vsock sends 0 if no cid is
	 * passed by the client.
	 */
	if (cid == 0)
		return true;

	return false;
}

static bool ghvst_dgram_allow(u32 cid, u32 port)
{
	struct gh_transport_device *gdev = get_ghvst(cid);

	if (gdev)
		return true;

	if (ghvst_allow_rsvd_cid(cid))
		return true;

	pr_err("%s: dgram not allowed for cid 0x%x\n", __func__, cid);

	return false;
}

static int ghvst_shutdown(struct vsock_sock *vsk, int mode)
{
	return 0;
}

static u32 ghvst_get_local_cid(void)
{
	return VMADDR_CID_HOST;
}

static const struct vsock_transport gunyah_transport = {
	/* Initialize/tear-down socket. */
	.init                  = ghvst_socket_init,
	.destruct              = ghvst_destruct,
	.release               = ghvst_release,

	/* DGRAM. */
	.dgram_bind            = ghvst_dgram_bind,
	.dgram_dequeue         = ghvst_dgram_dequeue,
	.dgram_enqueue         = ghvst_dgram_enqueue,
	.dgram_allow           = ghvst_dgram_allow,

	/* Shutdown. */
	.shutdown              = ghvst_shutdown,

	/* Addressing. */
	.get_local_cid         = ghvst_get_local_cid,
};

static int ghvst_rm_cb(struct notifier_block *nb, unsigned long cmd, void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload;
	struct gh_transport_device *gdev;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;

	gdev = container_of(nb, struct gh_transport_device, rm_nb);

	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	vm_status_payload = data;
	if (vm_status_payload->vm_status != GH_RM_VM_STATUS_READY)
		return NOTIFY_DONE;

	if (gh_rm_get_vmid(gdev->peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	if (gdev->msgq_hdl) {
		dev_err(gdev->dev, "%s: already have msgq handle!\n", __func__);
		return NOTIFY_DONE;
	}

	gdev->msgq_hdl = gh_msgq_register(gdev->msgq_label);
	if (IS_ERR(gdev->msgq_hdl)) {
		dev_err(gdev->dev, "%s: msgq registration failed: err:%d\n",
			__func__, PTR_ERR(gdev->msgq_hdl));
	}

	return NOTIFY_DONE;
}

static int gunyah_transport_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct gh_transport_device *gdev;
	struct device *dev = &pdev->dev;
	struct gh_transport_buf *gbuf;
	int rc;

	gdev = devm_kzalloc(dev, sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return -ENOMEM;

	gdev->dev = dev;
	mutex_init(&gdev->tx_lock);

	gbuf = &gdev->gbuf;
	gbuf->buf = devm_kzalloc(dev, MAX_PKT_SZ, GFP_KERNEL);
	if (!gbuf->buf)
		return -ENOMEM;

	mutex_init(&gbuf->lock);
	gbuf->len = 0;
	gbuf->copied = 0;
	gbuf->remaining = 0;
	gbuf->hdr_received = false;

	gdev->cid = VMADDR_CID_HOST;

	rc = of_property_read_u32(node, "msgq-label", &gdev->msgq_label);
	if (rc) {
		dev_err(dev, "failed to read msgq-label info %d\n", rc);
		return rc;
	}

	dev_set_drvdata(&pdev->dev, gdev);

	gdev->master = of_property_read_bool(node, "qcom,master");
	if (gdev->master) {
		rc = of_property_read_u32(node, "peer-name", &gdev->peer_name);
		if (rc) {
			dev_err(dev, "failed to read peer-name info %d\n", rc);
			return rc;
		}
		gdev->rm_nb.notifier_call = ghvst_rm_cb;
		gh_rm_register_notifier(&gdev->rm_nb);
	} else {
		gdev->msgq_hdl = gh_msgq_register(gdev->msgq_label);
		if (IS_ERR(gdev->msgq_hdl)) {
			rc = PTR_ERR(gdev->msgq_hdl);
			dev_err(dev, "msgq register failed rc:%d\n", rc);
			return rc;
		}
	}

	gdev->rx_thread = kthread_create(ghvst_msgq_recv, gdev, "ghvst_rx");
	if (IS_ERR(gdev->rx_thread)) {
		rc = PTR_ERR(gdev->rx_thread);
		dev_err(dev, "Failed to create receiver thread rc:%d\n", rc);
		return rc;
	}

	down_write(&ghvst_devs_lock);
	list_add(&gdev->item, &ghvst_devs);
	up_write(&ghvst_devs_lock);

	wake_up_process(gdev->rx_thread);

	return rc;
}

static int gunyah_transport_remove(struct platform_device *pdev)
{
	struct gh_transport_device *gdev = dev_get_drvdata(&pdev->dev);

	if (gdev->master)
		gh_rm_unregister_notifier(&gdev->rm_nb);

	if (gdev->rx_thread)
		kthread_stop(gdev->rx_thread);

	return 0;
}

static const struct of_device_id gunyah_transport_match_table[] = {
	{ .compatible = "qcom,gunyah-vsock" },
	{}
};
MODULE_DEVICE_TABLE(of, gunyah_transport_match_table);

static struct platform_driver gunyah_vsock_driver = {
	.driver = {
		.name = "gunyah_vsock",
		.of_match_table = gunyah_transport_match_table,
	 },
	.probe = gunyah_transport_probe,
	.remove = gunyah_transport_remove,
};

static int __init gunyah_vsock_init(void)
{
	int rc;

	rc = vsock_core_register(&gunyah_transport, VSOCK_TRANSPORT_F_DGRAM);
	if (rc)
		return rc;

	platform_driver_register(&gunyah_vsock_driver);

	return 0;
}

static void __exit gunyah_vsock_exit(void)
{
	vsock_core_unregister(&gunyah_transport);
	platform_driver_unregister(&gunyah_vsock_driver);
}
module_init(gunyah_vsock_init);
module_exit(gunyah_vsock_exit);

MODULE_DESCRIPTION("Gunyah Transport driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_NETPROTO(PF_VSOCK);
