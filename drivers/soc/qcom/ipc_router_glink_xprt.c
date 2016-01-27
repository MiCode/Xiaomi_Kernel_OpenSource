/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

/*
 * IPC ROUTER GLINK XPRT module.
 */
#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/ipc_router_xprt.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <soc/qcom/glink.h>
#include <soc/qcom/subsystem_restart.h>

static int ipc_router_glink_xprt_debug_mask;
module_param_named(debug_mask, ipc_router_glink_xprt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(DEBUG)
#define D(x...) do { \
if (ipc_router_glink_xprt_debug_mask) \
	pr_info(x); \
} while (0)
#else
#define D(x...) do { } while (0)
#endif

#define MIN_FRAG_SZ (IPC_ROUTER_HDR_SIZE + sizeof(union rr_control_msg))
#define IPC_RTR_XPRT_NAME_LEN (2 * GLINK_NAME_SIZE)
#define PIL_SUBSYSTEM_NAME_LEN 32
#define DEFAULT_NUM_INTENTS 5
#define DEFAULT_RX_INTENT_SIZE 2048
/**
 * ipc_router_glink_xprt - IPC Router's GLINK XPRT structure
 * @list: IPC router's GLINK XPRT list.
 * @ch_name: GLink Channel Name.
 * @edge: Edge between the local node and the remote node.
 * @transport: Physical Transport Name as identified by Glink.
 * @pil_edge: Edge name understood by PIL.
 * @ipc_rtr_xprt_name: XPRT Name to be registered with IPC Router.
 * @xprt: IPC Router XPRT structure to contain XPRT specific info.
 * @ch_hndl: Opaque Channel handle returned by GLink.
 * @xprt_wq: Workqueue to queue read & other XPRT related works.
 * @ss_reset_rwlock: Read-Write lock to protect access to the ss_reset flag.
 * @ss_reset: flag used to check SSR state.
 * @pil: pil handle to the remote subsystem
 * @sft_close_complete: Variable to indicate completion of SSR handling
 *                      by IPC Router.
 * @xprt_version: IPC Router header version supported by this XPRT.
 * @xprt_option: XPRT specific options to be handled by IPC Router.
 * @disable_pil_loading: Disable PIL Loading of the subsystem.
 */
struct ipc_router_glink_xprt {
	struct list_head list;
	char ch_name[GLINK_NAME_SIZE];
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
	char pil_edge[PIL_SUBSYSTEM_NAME_LEN];
	char ipc_rtr_xprt_name[IPC_RTR_XPRT_NAME_LEN];
	struct msm_ipc_router_xprt xprt;
	void *ch_hndl;
	struct workqueue_struct *xprt_wq;
	struct rw_semaphore ss_reset_rwlock;
	int ss_reset;
	void *pil;
	struct completion sft_close_complete;
	unsigned xprt_version;
	unsigned xprt_option;
	bool disable_pil_loading;
};

struct ipc_router_glink_xprt_work {
	struct ipc_router_glink_xprt *glink_xprtp;
	struct work_struct work;
};

struct queue_rx_intent_work {
	struct ipc_router_glink_xprt *glink_xprtp;
	size_t intent_size;
	struct work_struct work;
};

struct read_work {
	struct ipc_router_glink_xprt *glink_xprtp;
	void *iovec;
	size_t iovec_size;
	void * (*vbuf_provider)(void *iovec, size_t offset, size_t *size);
	void * (*pbuf_provider)(void *iovec, size_t offset, size_t *size);
	struct work_struct work;
};

static void glink_xprt_read_data(struct work_struct *work);
static void glink_xprt_open_event(struct work_struct *work);
static void glink_xprt_close_event(struct work_struct *work);

/**
 * ipc_router_glink_xprt_config - Config. Info. of each GLINK XPRT
 * @ch_name:		Name of the GLINK endpoint exported by GLINK driver.
 * @edge:		Edge between the local node and remote node.
 * @transport:		Physical Transport Name as identified by GLINK.
 * @pil_edge:		Edge name understood by PIL.
 * @ipc_rtr_xprt_name:	XPRT Name to be registered with IPC Router.
 * @link_id:		Network Cluster ID to which this XPRT belongs to.
 * @xprt_version:	IPC Router header version supported by this XPRT.
 * @disable_pil_loading:Disable PIL Loading of the subsystem.
 */
struct ipc_router_glink_xprt_config {
	char ch_name[GLINK_NAME_SIZE];
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
	char ipc_rtr_xprt_name[IPC_RTR_XPRT_NAME_LEN];
	char pil_edge[PIL_SUBSYSTEM_NAME_LEN];
	uint32_t link_id;
	unsigned xprt_version;
	unsigned xprt_option;
	bool disable_pil_loading;
};

#define MODULE_NAME "ipc_router_glink_xprt"
static DEFINE_MUTEX(glink_xprt_list_lock_lha1);
static LIST_HEAD(glink_xprt_list);

static struct workqueue_struct *glink_xprt_wq;

static void glink_xprt_link_state_cb(struct glink_link_state_cb_info *cb_info,
				     void *priv);
static struct glink_link_info glink_xprt_link_info = {
			NULL, NULL, glink_xprt_link_state_cb};
static void *glink_xprt_link_state_notif_handle;

struct xprt_state_work_info {
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
	uint32_t link_state;
	struct work_struct work;
};

#define OVERFLOW_ADD_UNSIGNED(type, a, b) \
	(((type)~0 - (a)) < (b) ? true : false)

static void *glink_xprt_vbuf_provider(void *iovec, size_t offset,
				      size_t *buf_size)
{
	struct rr_packet *pkt = (struct rr_packet *)iovec;
	struct sk_buff *skb;
	size_t temp_size = 0;

	if (unlikely(!pkt || !buf_size))
		return NULL;

	*buf_size = 0;
	skb_queue_walk(pkt->pkt_fragment_q, skb) {
		if (unlikely(OVERFLOW_ADD_UNSIGNED(size_t, temp_size,
						   skb->len)))
			break;

		temp_size += skb->len;
		if (offset >= temp_size)
			continue;

		*buf_size = temp_size - offset;
		return (void *)skb->data + skb->len - *buf_size;
	}
	return NULL;
}

/**
 * ipc_router_glink_xprt_set_version() - Set the IPC Router version in transport
 * @xprt:	Reference to the transport structure.
 * @version:	The version to be set in transport.
 */
static void ipc_router_glink_xprt_set_version(
	struct msm_ipc_router_xprt *xprt, unsigned version)
{
	struct ipc_router_glink_xprt *glink_xprtp;

	if (!xprt)
		return;
	glink_xprtp = container_of(xprt, struct ipc_router_glink_xprt, xprt);
	glink_xprtp->xprt_version = version;
}

static int ipc_router_glink_xprt_get_version(
	struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_glink_xprt *glink_xprtp;
	if (!xprt)
		return -EINVAL;
	glink_xprtp = container_of(xprt, struct ipc_router_glink_xprt, xprt);

	return (int)glink_xprtp->xprt_version;
}

static int ipc_router_glink_xprt_get_option(
	struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_glink_xprt *glink_xprtp;
	if (!xprt)
		return -EINVAL;
	glink_xprtp = container_of(xprt, struct ipc_router_glink_xprt, xprt);

	return (int)glink_xprtp->xprt_option;
}

static int ipc_router_glink_xprt_write(void *data, uint32_t len,
				       struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct rr_packet *temp_pkt;
	int ret;
	struct ipc_router_glink_xprt *glink_xprtp =
		container_of(xprt, struct ipc_router_glink_xprt, xprt);

	if (!pkt)
		return -EINVAL;

	if (!len || pkt->length != len)
		return -EINVAL;

	temp_pkt = clone_pkt(pkt);
	if (!temp_pkt) {
		IPC_RTR_ERR("%s: Error cloning packet while tx\n", __func__);
		return -ENOMEM;
	}

	down_read(&glink_xprtp->ss_reset_rwlock);
	if (glink_xprtp->ss_reset) {
		release_pkt(temp_pkt);
		IPC_RTR_ERR("%s: %s chnl reset\n", __func__, xprt->name);
		ret = -ENETRESET;
		goto out_write_data;
	}

	D("%s: Ready to write %d bytes\n", __func__, len);
	ret = glink_txv(glink_xprtp->ch_hndl, (void *)glink_xprtp,
			(void *)temp_pkt, len, glink_xprt_vbuf_provider,
			NULL, true);
	if (ret < 0) {
		release_pkt(temp_pkt);
		IPC_RTR_ERR("%s: Error %d while tx\n", __func__, ret);
		goto out_write_data;
	}
	ret = len;
	D("%s:%s: TX Complete for %d bytes @ %p\n", __func__,
	  glink_xprtp->ipc_rtr_xprt_name, len, temp_pkt);

out_write_data:
	up_read(&glink_xprtp->ss_reset_rwlock);
	return ret;
}

static int ipc_router_glink_xprt_close(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_glink_xprt *glink_xprtp =
		container_of(xprt, struct ipc_router_glink_xprt, xprt);

	down_write(&glink_xprtp->ss_reset_rwlock);
	glink_xprtp->ss_reset = 1;
	up_write(&glink_xprtp->ss_reset_rwlock);
	return glink_close(glink_xprtp->ch_hndl);
}

static void glink_xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_glink_xprt *glink_xprtp =
		container_of(xprt, struct ipc_router_glink_xprt, xprt);

	complete_all(&glink_xprtp->sft_close_complete);
}

static struct rr_packet *glink_xprt_copy_data(struct read_work *rx_work)
{
	void *buf, *pbuf, *dest_buf;
	size_t buf_size;
	struct rr_packet *pkt;
	struct sk_buff *skb;

	pkt = create_pkt(NULL);
	if (!pkt) {
		IPC_RTR_ERR("%s: Couldn't alloc rr_packet\n", __func__);
		return NULL;
	}

	do {
		buf_size = 0;
		if (rx_work->vbuf_provider) {
			buf = rx_work->vbuf_provider(rx_work->iovec,
						pkt->length, &buf_size);
		} else {
			pbuf = rx_work->pbuf_provider(rx_work->iovec,
						pkt->length, &buf_size);
			buf = phys_to_virt((unsigned long)pbuf);
		}
		if (!buf_size || !buf)
			break;

		skb = alloc_skb(buf_size, GFP_KERNEL);
		if (!skb) {
			IPC_RTR_ERR("%s: Couldn't alloc skb of size %zu\n",
				    __func__, buf_size);
			release_pkt(pkt);
			return NULL;
		}
		dest_buf = skb_put(skb, buf_size);
		memcpy(dest_buf, buf, buf_size);
		skb_queue_tail(pkt->pkt_fragment_q, skb);
		pkt->length += buf_size;
	} while (buf && buf_size);
	return pkt;
}

static void glink_xprt_read_data(struct work_struct *work)
{
	struct rr_packet *pkt;
	struct read_work *rx_work =
		container_of(work, struct read_work, work);
	struct ipc_router_glink_xprt *glink_xprtp = rx_work->glink_xprtp;
	bool reuse_intent = false;

	down_read(&glink_xprtp->ss_reset_rwlock);
	if (glink_xprtp->ss_reset) {
		IPC_RTR_ERR("%s: %s channel reset\n",
			__func__, glink_xprtp->xprt.name);
		goto out_read_data;
	}

	D("%s %zu bytes @ %p\n", __func__, rx_work->iovec_size, rx_work->iovec);
	if (rx_work->iovec_size <= DEFAULT_RX_INTENT_SIZE)
		reuse_intent = true;

	pkt = glink_xprt_copy_data(rx_work);
	if (!pkt) {
		IPC_RTR_ERR("%s: Error copying data\n", __func__);
		goto out_read_data;
	}

	msm_ipc_router_xprt_notify(&glink_xprtp->xprt,
				   IPC_ROUTER_XPRT_EVENT_DATA, pkt);
	release_pkt(pkt);
out_read_data:
	glink_rx_done(glink_xprtp->ch_hndl, rx_work->iovec, reuse_intent);
	kfree(rx_work);
	up_read(&glink_xprtp->ss_reset_rwlock);
}

static void glink_xprt_open_event(struct work_struct *work)
{
	struct ipc_router_glink_xprt_work *xprt_work =
		container_of(work, struct ipc_router_glink_xprt_work, work);
	struct ipc_router_glink_xprt *glink_xprtp = xprt_work->glink_xprtp;
	int i;

	msm_ipc_router_xprt_notify(&glink_xprtp->xprt,
				IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	  __func__, glink_xprtp->xprt.name);
	for (i = 0; i < DEFAULT_NUM_INTENTS; i++)
		glink_queue_rx_intent(glink_xprtp->ch_hndl, (void *)glink_xprtp,
				      DEFAULT_RX_INTENT_SIZE);
	kfree(xprt_work);
}

static void glink_xprt_close_event(struct work_struct *work)
{
	struct ipc_router_glink_xprt_work *xprt_work =
		container_of(work, struct ipc_router_glink_xprt_work, work);
	struct ipc_router_glink_xprt *glink_xprtp = xprt_work->glink_xprtp;

	init_completion(&glink_xprtp->sft_close_complete);
	msm_ipc_router_xprt_notify(&glink_xprtp->xprt,
				IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
	   __func__, glink_xprtp->xprt.name);
	wait_for_completion(&glink_xprtp->sft_close_complete);
	kfree(xprt_work);
}

static void glink_xprt_qrx_intent_worker(struct work_struct *work)
{
	struct queue_rx_intent_work *qrx_intent_work =
		container_of(work, struct queue_rx_intent_work, work);
	struct ipc_router_glink_xprt *glink_xprtp =
					qrx_intent_work->glink_xprtp;

	glink_queue_rx_intent(glink_xprtp->ch_hndl, (void *)glink_xprtp,
			      qrx_intent_work->intent_size);
	kfree(qrx_intent_work);
}

static void msm_ipc_unload_subsystem(struct ipc_router_glink_xprt *glink_xprtp)
{
	if (glink_xprtp->pil) {
		subsystem_put(glink_xprtp->pil);
		glink_xprtp->pil = NULL;
	}
}

static void *msm_ipc_load_subsystem(struct ipc_router_glink_xprt *glink_xprtp)
{
	void *pil = NULL;

	if (!glink_xprtp->disable_pil_loading) {
		pil = subsystem_get(glink_xprtp->pil_edge);
		if (IS_ERR(pil)) {
			pr_err("%s: Failed to load %s err = [0x%ld]\n",
				__func__, glink_xprtp->pil_edge, PTR_ERR(pil));
			pil = NULL;
		}
	}
	return pil;
}

static void glink_xprt_notify_rxv(void *handle, const void *priv,
	const void *pkt_priv, void *ptr, size_t size,
	void * (*vbuf_provider)(void *iovec, size_t offset, size_t *size),
	void * (*pbuf_provider)(void *iovec, size_t offset, size_t *size))
{
	struct ipc_router_glink_xprt *glink_xprtp =
		(struct ipc_router_glink_xprt *)priv;
	struct read_work *rx_work;

	rx_work = kmalloc(sizeof(*rx_work), GFP_ATOMIC);
	if (!rx_work) {
		IPC_RTR_ERR("%s: couldn't allocate read_work\n", __func__);
		glink_rx_done(glink_xprtp->ch_hndl, ptr, true);
		return;
	}

	rx_work->glink_xprtp = glink_xprtp;
	rx_work->iovec = ptr;
	rx_work->iovec_size = size;
	rx_work->vbuf_provider = vbuf_provider;
	rx_work->pbuf_provider = pbuf_provider;
	INIT_WORK(&rx_work->work, glink_xprt_read_data);
	queue_work(glink_xprtp->xprt_wq, &rx_work->work);
}

static void glink_xprt_notify_tx_done(void *handle, const void *priv,
				      const void *pkt_priv, const void *ptr)
{
	struct ipc_router_glink_xprt *glink_xprtp =
		(struct ipc_router_glink_xprt *)priv;
	struct rr_packet *temp_pkt = (struct rr_packet *)ptr;

	D("%s:%s: @ %p\n", __func__, glink_xprtp->ipc_rtr_xprt_name, ptr);
	release_pkt(temp_pkt);
}

static bool glink_xprt_notify_rx_intent_req(void *handle, const void *priv,
					    size_t sz)
{
	struct queue_rx_intent_work *qrx_intent_work;
	struct ipc_router_glink_xprt *glink_xprtp =
		(struct ipc_router_glink_xprt *)priv;

	if (sz <= DEFAULT_RX_INTENT_SIZE)
		return true;

	qrx_intent_work = kmalloc(sizeof(struct queue_rx_intent_work),
				  GFP_ATOMIC);
	if (!qrx_intent_work) {
		IPC_RTR_ERR("%s: Couldn't queue rx_intent of %zu bytes\n",
			    __func__, sz);
		return false;
	}
	qrx_intent_work->glink_xprtp = glink_xprtp;
	qrx_intent_work->intent_size = sz;
	INIT_WORK(&qrx_intent_work->work, glink_xprt_qrx_intent_worker);
	queue_work(glink_xprtp->xprt_wq, &qrx_intent_work->work);
	return true;
}

static void glink_xprt_notify_state(void *handle, const void *priv,
				    unsigned event)
{
	struct ipc_router_glink_xprt_work *xprt_work;
	struct ipc_router_glink_xprt *glink_xprtp =
		(struct ipc_router_glink_xprt *)priv;

	D("%s: %s:%s - State %d\n",
	  __func__, glink_xprtp->edge, glink_xprtp->transport, event);
	switch (event) {
	case GLINK_CONNECTED:
		if (IS_ERR_OR_NULL(glink_xprtp->ch_hndl))
			glink_xprtp->ch_hndl = handle;
		down_write(&glink_xprtp->ss_reset_rwlock);
		glink_xprtp->ss_reset = 0;
		up_write(&glink_xprtp->ss_reset_rwlock);
		xprt_work = kmalloc(sizeof(struct ipc_router_glink_xprt_work),
				    GFP_ATOMIC);
		if (!xprt_work) {
			IPC_RTR_ERR(
			"%s: Couldn't notify %d event to IPC Router\n",
				__func__, event);
			return;
		}
		xprt_work->glink_xprtp = glink_xprtp;
		INIT_WORK(&xprt_work->work, glink_xprt_open_event);
		queue_work(glink_xprtp->xprt_wq, &xprt_work->work);
		break;

	case GLINK_LOCAL_DISCONNECTED:
	case GLINK_REMOTE_DISCONNECTED:
		down_write(&glink_xprtp->ss_reset_rwlock);
		if (glink_xprtp->ss_reset) {
			up_write(&glink_xprtp->ss_reset_rwlock);
			break;
		}
		glink_xprtp->ss_reset = 1;
		up_write(&glink_xprtp->ss_reset_rwlock);
		xprt_work = kmalloc(sizeof(struct ipc_router_glink_xprt_work),
				    GFP_ATOMIC);
		if (!xprt_work) {
			IPC_RTR_ERR(
			"%s: Couldn't notify %d event to IPC Router\n",
				__func__, event);
			return;
		}
		xprt_work->glink_xprtp = glink_xprtp;
		INIT_WORK(&xprt_work->work, glink_xprt_close_event);
		queue_work(glink_xprtp->xprt_wq, &xprt_work->work);
		break;
	}
}

static void glink_xprt_ch_open(struct ipc_router_glink_xprt *glink_xprtp)
{
	struct glink_open_config open_cfg = {0};

	if (!IS_ERR_OR_NULL(glink_xprtp->ch_hndl))
		return;

	open_cfg.transport = glink_xprtp->transport;
	open_cfg.options |= GLINK_OPT_INITIAL_XPORT;
	open_cfg.edge = glink_xprtp->edge;
	open_cfg.name = glink_xprtp->ch_name;
	open_cfg.notify_rx = NULL;
	open_cfg.notify_rxv = glink_xprt_notify_rxv;
	open_cfg.notify_tx_done = glink_xprt_notify_tx_done;
	open_cfg.notify_state = glink_xprt_notify_state;
	open_cfg.notify_rx_intent_req = glink_xprt_notify_rx_intent_req;
	open_cfg.priv = glink_xprtp;

	glink_xprtp->pil = msm_ipc_load_subsystem(glink_xprtp);
	glink_xprtp->ch_hndl =  glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(glink_xprtp->ch_hndl)) {
		IPC_RTR_ERR("%s:%s:%s %s: unable to open channel\n",
			    open_cfg.transport, open_cfg.edge,
			    open_cfg.name, __func__);
			msm_ipc_unload_subsystem(glink_xprtp);
	}
}

/**
 * glink_xprt_link_state_worker() - Function to handle link state updates
 * @work: Pointer to the work item in the link_state_work_info.
 *
 * This worker function is scheduled when there is a link state update. Since
 * the loopback server registers for all transports, it receives all link state
 * updates about all transports that get registered in the system.
 */
static void glink_xprt_link_state_worker(struct work_struct *work)
{
	struct xprt_state_work_info *xs_info =
		container_of(work, struct xprt_state_work_info, work);
	struct ipc_router_glink_xprt *glink_xprtp;

	if (xs_info->link_state == GLINK_LINK_STATE_UP) {
		D("%s: LINK_STATE_UP %s:%s\n",
		  __func__, xs_info->edge, xs_info->transport);
		mutex_lock(&glink_xprt_list_lock_lha1);
		list_for_each_entry(glink_xprtp, &glink_xprt_list, list) {
			if (strcmp(glink_xprtp->edge, xs_info->edge) ||
			    strcmp(glink_xprtp->transport, xs_info->transport))
				continue;
			glink_xprt_ch_open(glink_xprtp);
		}
		mutex_unlock(&glink_xprt_list_lock_lha1);
	} else if (xs_info->link_state == GLINK_LINK_STATE_DOWN) {
		D("%s: LINK_STATE_DOWN %s:%s\n",
		  __func__, xs_info->edge, xs_info->transport);
		mutex_lock(&glink_xprt_list_lock_lha1);
		list_for_each_entry(glink_xprtp, &glink_xprt_list, list) {
			if (strcmp(glink_xprtp->edge, xs_info->edge) ||
			    strcmp(glink_xprtp->transport, xs_info->transport)
			    || IS_ERR_OR_NULL(glink_xprtp->ch_hndl))
				continue;
			glink_close(glink_xprtp->ch_hndl);
			glink_xprtp->ch_hndl = NULL;
			msm_ipc_unload_subsystem(glink_xprtp);
		}
		mutex_unlock(&glink_xprt_list_lock_lha1);

	}
	kfree(xs_info);
	return;
}

/**
 * glink_xprt_link_state_cb() - Callback to receive link state updates
 * @cb_info: Information containing link & its state.
 * @priv: Private data passed during the link state registration.
 *
 * This function is called by the GLINK core to notify the IPC Router
 * regarding the link state updates. This function is registered with the
 * GLINK core by IPC Router during glink_register_link_state_cb().
 */
static void glink_xprt_link_state_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv)
{
	struct xprt_state_work_info *xs_info;

	if (!cb_info)
		return;

	D("%s: %s:%s\n", __func__, cb_info->edge, cb_info->transport);
	xs_info = kmalloc(sizeof(*xs_info), GFP_KERNEL);
	if (!xs_info) {
		IPC_RTR_ERR("%s: Error allocating xprt state info\n", __func__);
		return;
	}

	strlcpy(xs_info->edge, cb_info->edge, GLINK_NAME_SIZE);
	strlcpy(xs_info->transport, cb_info->transport, GLINK_NAME_SIZE);
	xs_info->link_state = cb_info->link_state;
	INIT_WORK(&xs_info->work, glink_xprt_link_state_worker);
	queue_work(glink_xprt_wq, &xs_info->work);
}

/**
 * ipc_router_glink_config_init() - init GLINK xprt configs
 *
 * @glink_xprt_config: pointer to GLINK Channel configurations.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the GLINK XPRT pointer with
 * the GLINK XPRT configurations either from device tree or static arrays.
 */
static int ipc_router_glink_config_init(
		struct ipc_router_glink_xprt_config *glink_xprt_config)
{
	struct ipc_router_glink_xprt *glink_xprtp;
	char xprt_wq_name[GLINK_NAME_SIZE];

	glink_xprtp = kzalloc(sizeof(struct ipc_router_glink_xprt), GFP_KERNEL);
	if (IS_ERR_OR_NULL(glink_xprtp)) {
		IPC_RTR_ERR("%s:%s:%s:%s glink_xprtp alloc failed\n",
			    __func__, glink_xprt_config->ch_name,
			    glink_xprt_config->edge,
			    glink_xprt_config->transport);
		return -ENOMEM;
	}

	glink_xprtp->xprt.link_id = glink_xprt_config->link_id;
	glink_xprtp->xprt_version = glink_xprt_config->xprt_version;
	glink_xprtp->xprt_option = glink_xprt_config->xprt_option;
	glink_xprtp->disable_pil_loading =
				glink_xprt_config->disable_pil_loading;

	if (!glink_xprtp->disable_pil_loading)
		strlcpy(glink_xprtp->pil_edge, glink_xprt_config->pil_edge,
				PIL_SUBSYSTEM_NAME_LEN);
	strlcpy(glink_xprtp->ch_name, glink_xprt_config->ch_name,
		GLINK_NAME_SIZE);
	strlcpy(glink_xprtp->edge, glink_xprt_config->edge, GLINK_NAME_SIZE);
	strlcpy(glink_xprtp->transport,
		glink_xprt_config->transport, GLINK_NAME_SIZE);
	strlcpy(glink_xprtp->ipc_rtr_xprt_name,
		glink_xprt_config->ipc_rtr_xprt_name, IPC_RTR_XPRT_NAME_LEN);
	glink_xprtp->xprt.name = glink_xprtp->ipc_rtr_xprt_name;

	glink_xprtp->xprt.get_version =	ipc_router_glink_xprt_get_version;
	glink_xprtp->xprt.set_version =	ipc_router_glink_xprt_set_version;
	glink_xprtp->xprt.get_option = ipc_router_glink_xprt_get_option;
	glink_xprtp->xprt.read_avail = NULL;
	glink_xprtp->xprt.read = NULL;
	glink_xprtp->xprt.write_avail = NULL;
	glink_xprtp->xprt.write = ipc_router_glink_xprt_write;
	glink_xprtp->xprt.close = ipc_router_glink_xprt_close;
	glink_xprtp->xprt.sft_close_done = glink_xprt_sft_close_done;
	glink_xprtp->xprt.priv = NULL;

	init_rwsem(&glink_xprtp->ss_reset_rwlock);
	glink_xprtp->ss_reset = 0;

	scnprintf(xprt_wq_name, GLINK_NAME_SIZE, "%s_%s_%s",
			glink_xprtp->ch_name, glink_xprtp->edge,
			glink_xprtp->transport);
	glink_xprtp->xprt_wq = create_singlethread_workqueue(xprt_wq_name);
	if (IS_ERR_OR_NULL(glink_xprtp->xprt_wq)) {
		IPC_RTR_ERR("%s:%s:%s:%s wq alloc failed\n",
			    __func__, glink_xprt_config->ch_name,
			    glink_xprt_config->edge,
			    glink_xprt_config->transport);
		kfree(glink_xprtp);
		return -EFAULT;
	}

	mutex_lock(&glink_xprt_list_lock_lha1);
	list_add(&glink_xprtp->list, &glink_xprt_list);
	mutex_unlock(&glink_xprt_list_lock_lha1);

	glink_xprt_link_info.edge = glink_xprt_config->edge;
	glink_xprt_link_state_notif_handle = glink_register_link_state_cb(
						&glink_xprt_link_info, NULL);
	return 0;
}

/**
 * parse_devicetree() - parse device tree binding
 *
 * @node: pointer to device tree node
 * @glink_xprt_config: pointer to GLINK XPRT configurations
 *
 * @return: 0 on success, -ENODEV on failure.
 */
static int parse_devicetree(struct device_node *node,
		struct ipc_router_glink_xprt_config *glink_xprt_config)
{
	int ret;
	int link_id;
	int version;
	char *key;
	const char *ch_name;
	const char *edge;
	const char *transport;
	const char *pil_edge;

	key = "qcom,ch-name";
	ch_name = of_get_property(node, key, NULL);
	if (!ch_name)
		goto error;
	strlcpy(glink_xprt_config->ch_name, ch_name, GLINK_NAME_SIZE);

	key = "qcom,xprt-remote";
	edge = of_get_property(node, key, NULL);
	if (!edge)
		goto error;
	strlcpy(glink_xprt_config->edge, edge, GLINK_NAME_SIZE);

	key = "qcom,glink-xprt";
	transport = of_get_property(node, key, NULL);
	if (!transport)
		goto error;
	strlcpy(glink_xprt_config->transport, transport,
		GLINK_NAME_SIZE);

	key = "qcom,xprt-linkid";
	ret = of_property_read_u32(node, key, &link_id);
	if (ret)
		goto error;
	glink_xprt_config->link_id = link_id;

	key = "qcom,xprt-version";
	ret = of_property_read_u32(node, key, &version);
	if (ret)
		goto error;
	glink_xprt_config->xprt_version = version;

	key = "qcom,fragmented-data";
	glink_xprt_config->xprt_option = of_property_read_bool(node, key);

	key = "qcom,pil-label";
	pil_edge = of_get_property(node, key, NULL);
	if (pil_edge) {
		strlcpy(glink_xprt_config->pil_edge,
				pil_edge, PIL_SUBSYSTEM_NAME_LEN);
		glink_xprt_config->disable_pil_loading = false;
	} else {
		glink_xprt_config->disable_pil_loading = true;
	}
	scnprintf(glink_xprt_config->ipc_rtr_xprt_name, IPC_RTR_XPRT_NAME_LEN,
		  "%s_%s", edge, ch_name);

	return 0;

error:
	IPC_RTR_ERR("%s: missing key: %s\n", __func__, key);
	return -ENODEV;
}

/**
 * ipc_router_glink_xprt_probe() - Probe a GLINK xprt
 *
 * @pdev: Platform device corresponding to GLINK xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to a GLINK transport.
 */
static int ipc_router_glink_xprt_probe(struct platform_device *pdev)
{
	int ret;
	struct ipc_router_glink_xprt_config glink_xprt_config;

	if (pdev) {
		if (pdev->dev.of_node) {
			ret = parse_devicetree(pdev->dev.of_node,
							&glink_xprt_config);
			if (ret) {
				IPC_RTR_ERR("%s: Failed to parse device tree\n",
					    __func__);
				return ret;
			}

			ret = ipc_router_glink_config_init(&glink_xprt_config);
			if (ret) {
				IPC_RTR_ERR("%s init failed\n", __func__);
				return ret;
			}
		}
	}
	return 0;
}

static struct of_device_id ipc_router_glink_xprt_match_table[] = {
	{ .compatible = "qcom,ipc_router_glink_xprt" },
	{},
};

static struct platform_driver ipc_router_glink_xprt_driver = {
	.probe = ipc_router_glink_xprt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ipc_router_glink_xprt_match_table,
	 },
};

static int __init ipc_router_glink_xprt_init(void)
{
	int rc;

	glink_xprt_wq = create_singlethread_workqueue("glink_xprt_wq");
	if (IS_ERR_OR_NULL(glink_xprt_wq)) {
		pr_err("%s: create_singlethread_workqueue failed\n", __func__);
		return -EFAULT;
	}

	rc = platform_driver_register(&ipc_router_glink_xprt_driver);
	if (rc) {
		IPC_RTR_ERR(
		"%s: ipc_router_glink_xprt_driver register failed %d\n",
		__func__, rc);
		return rc;
	}

	return 0;
}

module_init(ipc_router_glink_xprt_init);
MODULE_DESCRIPTION("IPC Router GLINK XPRT");
MODULE_LICENSE("GPL v2");
