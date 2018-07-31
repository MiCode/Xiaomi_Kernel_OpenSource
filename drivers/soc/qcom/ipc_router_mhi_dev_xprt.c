/* Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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

#include <linux/ipc_router_xprt.h>
#include <linux/module.h>
#include <linux/msm_mhi_dev.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/types.h>

static int ipc_router_mhi_dev_xprt_debug_mask;
module_param_named(debug_mask, ipc_router_mhi_dev_xprt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define D(x...) do { \
if (ipc_router_mhi_dev_xprt_debug_mask) \
	pr_info(x); \
} while (0)

#define MODULE_NAME "ipc_router_mhi_dev_xprt"
#define XPRT_NAME_LEN 32
#define IPC_ROUTER_MHI_XPRT_MAX_PKT_SIZE 8192
#define MHI_IPCR_ASYNC_TIMEOUT msecs_to_jiffies(100)
#define MAX_IPCR_WR_REQ 128

/**
 * ipc_router_mhi_dev_channel - MHI Channel related information
 * @out_chan_id: Out channel ID for use by IPC ROUTER enumerated in MHI driver.
 * @out_handle: MHI Output channel handle.
 * @in_chan_id: In channel ID for use by IPC ROUTER enumerated in MHI driver.
 * @in_handle: MHI Input channel handle.
 * @state_lock: Lock to protect access to the state information.
 * @out_chan_enabled: State of the outgoing channel.
 * @in_chan_enabled: State of the incoming channel.
 * @max_packet_size: Possible maximum packet size.
 * @mhi_xprtp: Pointer to IPC Router MHI XPRT.
 */
struct ipc_router_mhi_dev_channel {
	enum mhi_client_channel out_chan_id;
	struct mhi_dev_client *out_handle;
	enum mhi_client_channel in_chan_id;
	struct mhi_dev_client *in_handle;
	struct mutex state_lock;
	bool out_chan_enabled;
	bool in_chan_enabled;
	size_t max_packet_size;
	void *mhi_xprtp;
};

/**
 * ipc_router_mhi_dev_xprt - IPC Router's MHI XPRT structure
 * @list: IPC router's MHI XPRTs list.
 * @ch_hndl: Data Structure to hold MHI Channel information.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 * @xprt: IPC Router XPRT structure to contain MHI XPRT specific info.
 * @wq: Workqueue to queue read & other XPRT related works.
 * @read_work: Read Work to perform read operation from MHI Driver.
 * @write_wait_q: Wait Queue to handle the write events.
 * @xprt_version: IPC Router header version supported by this XPRT.
 * @xprt_option: XPRT specific options to be handled by IPC Router.
 * @wr_req_lock: Lock for write request list
 * @wreqs: List of write requests
 * @wr_req_list: Head of the list of available write requests
 * @read_done: Completion for async read.
 */
struct ipc_router_mhi_dev_xprt {
	struct list_head list;
	struct ipc_router_mhi_dev_channel ch_hndl;
	char xprt_name[XPRT_NAME_LEN];
	struct msm_ipc_router_xprt xprt;
	struct workqueue_struct *wq;
	struct work_struct read_work;
	wait_queue_head_t write_wait_q;
	unsigned xprt_version;
	unsigned xprt_option;
	spinlock_t wr_req_lock;
	struct mhi_req *wreqs;
	struct list_head wr_req_list;
	struct completion read_done;
};

/**
 * ipc_router_mhi_dev_xprt_config - Config. Info. of each MHI XPRT
 * @out_chan_id: Out channel ID for use by IPC ROUTER enumerated in MHI driver.
 * @in_chan_id: In channel ID for use by IPC ROUTER enumerated in MHI driver.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 * @link_id: Network Cluster ID to which this XPRT belongs to.
 * @xprt_version: IPC Router header version supported by this XPRT.
 */
struct ipc_router_mhi_dev_xprt_config {
	enum mhi_client_channel out_chan_id;
	enum mhi_client_channel in_chan_id;
	char xprt_name[XPRT_NAME_LEN];
	uint32_t link_id;
	uint32_t xprt_version;
	uint32_t xprt_option;
};

static struct ipc_router_mhi_dev_xprt *mhi_xprtp;

/*
 * ipc_router_mhi_dev_release_pkt() - Release a cloned IPC Router packet
 * @ref: Reference to the kref object in the IPC Router packet.
 */
static void ipc_router_mhi_dev_release_pkt(struct kref *ref)
{
	struct rr_packet *pkt = container_of(ref, struct rr_packet, ref);

	release_pkt(pkt);
}

/**
* ipc_router_mhi_dev_set_xprt_version() - Set the IPC Router version in transport
* @xprt:      Reference to the transport structure.
* @version:   The version to be set in transport.
*/
static void ipc_router_mhi_dev_set_xprt_version(
	struct msm_ipc_router_xprt *xprt,
	unsigned version)
{
	struct ipc_router_mhi_dev_xprt *mhi_xprtp;

	if (!xprt)
		return;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_dev_xprt, xprt);
	mhi_xprtp->xprt_version = version;
}

/**
 * ipc_router_mhi_dev_get_xprt_version() - Get IPC Router header version
 *				       supported by the XPRT
 * @xprt: XPRT for which the version information is required.
 *
 * @return: IPC Router header version supported by the XPRT.
 */
static int ipc_router_mhi_dev_get_xprt_version(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_mhi_dev_xprt *mhi_xprtp;

	if (!xprt)
		return -EINVAL;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_dev_xprt, xprt);

	return (int)mhi_xprtp->xprt_version;
}

/**
 * ipc_router_mhi_dev_get_xprt_option() - Get XPRT options
 * @xprt: XPRT for which the option information is required.
 *
 * @return: Options supported by the XPRT.
 */
static int ipc_router_mhi_dev_get_xprt_option(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_mhi_dev_xprt *mhi_xprtp;

	if (!xprt)
		return -EINVAL;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_dev_xprt, xprt);

	return (int)mhi_xprtp->xprt_option;
}

static void mhi_xprt_write_completion_cb(void *req)
{
	struct mhi_req *ureq = req;
	struct rr_packet *pkt = ureq->context;
	unsigned long flags;

	if (pkt)
		kref_put(&pkt->ref, ipc_router_mhi_dev_release_pkt);

	spin_lock_irqsave(&mhi_xprtp->wr_req_lock, flags);
	list_add_tail(&ureq->list, &mhi_xprtp->wr_req_list);
	spin_unlock_irqrestore(&mhi_xprtp->wr_req_lock, flags);
}

static void mhi_xprt_read_completion_cb(void *req)
{
	struct mhi_req *ureq = req;
	struct ipc_router_mhi_dev_xprt *mhi_xprtp;

	mhi_xprtp = (struct ipc_router_mhi_dev_xprt *)ureq->context;
	complete(&mhi_xprtp->read_done);
}

static void mhi_xprt_event_notifier(struct mhi_dev_client_cb_reason *reason)
{
	if (reason->reason == MHI_DEV_TRE_AVAILABLE) {
		if (reason->ch_id == mhi_xprtp->ch_hndl.in_chan_id)
			wake_up(&mhi_xprtp->write_wait_q);
		else if (reason->ch_id == mhi_xprtp->ch_hndl.out_chan_id)
			queue_work(mhi_xprtp->wq, &mhi_xprtp->read_work);
	}
}

/*
 * mhi_xprt_read_data() - Read data from the XPRT
 */
static void mhi_xprt_read_data(struct work_struct *work)
{
	struct sk_buff *skb;
	struct rr_packet *in_pkt;
	struct mhi_req mreq = {0};
	int data_sz;
	struct ipc_router_mhi_dev_xprt *mhi_xprtp =
		container_of(work, struct ipc_router_mhi_dev_xprt, read_work);
	unsigned long compl_ret;

	while (!mhi_dev_channel_isempty(mhi_xprtp->ch_hndl.out_handle)) {
		in_pkt = create_pkt(NULL);
		if (!in_pkt) {
			IPC_RTR_ERR("%s: Couldn't alloc rr_packet\n",
				    __func__);
			return;
		}
		D("%s: Allocated rr_packet\n", __func__);

		skb = alloc_skb(mhi_xprtp->ch_hndl.max_packet_size, GFP_KERNEL);
		if (!skb) {
			IPC_RTR_ERR("%s: Could not allocate SKB\n", __func__);
			goto exit_free_pkt;
		}

		mreq.client = mhi_xprtp->ch_hndl.out_handle;
		mreq.context = mhi_xprtp;
		mreq.buf = skb->data;
		mreq.len = mhi_xprtp->ch_hndl.max_packet_size;
		mreq.chan = mhi_xprtp->ch_hndl.out_chan_id;
		mreq.mode = IPA_DMA_ASYNC;
		mreq.client_cb = mhi_xprt_read_completion_cb;

		reinit_completion(&mhi_xprtp->read_done);

		data_sz = mhi_dev_read_channel(&mreq);
		if (data_sz < 0) {
			IPC_RTR_ERR("%s: Failed to queue TRB into MHI\n",
				    __func__);
			goto exit_free_skb;
		} else if (!data_sz) {
			D("%s: No data available\n", __func__);
			goto exit_free_skb;
		}

		compl_ret =
			wait_for_completion_interruptible_timeout(
				&mhi_xprtp->read_done,
				MHI_IPCR_ASYNC_TIMEOUT);

		if (compl_ret == -ERESTARTSYS) {
			IPC_RTR_ERR("%s: Exit signal caught\n", __func__);
			goto exit_free_skb;
		} else if (compl_ret == 0) {
			IPC_RTR_ERR("%s: Read timed out\n", __func__);
			goto exit_free_skb;
		}

		if (mreq.transfer_len != ipc_router_peek_pkt_size(skb->data)) {
			IPC_RTR_ERR("%s: Incomplete packet, drop it\n",
				    __func__);
			goto exit_free_skb;
		}

		skb_put(skb, mreq.transfer_len);
		in_pkt->length = mreq.transfer_len;
		skb_queue_tail(in_pkt->pkt_fragment_q, skb);

		D("%s: Packet size read %d\n", __func__, in_pkt->length);
		msm_ipc_router_xprt_notify(&mhi_xprtp->xprt,
					   IPC_ROUTER_XPRT_EVENT_DATA,
					   (void *)in_pkt);
		release_pkt(in_pkt);
	}

	return;

exit_free_skb:
	kfree_skb(skb);
exit_free_pkt:
	release_pkt(in_pkt);
}

/**
 * ipc_router_mhi_dev_write_skb() - Write a single SKB onto the XPRT
 * @mhi_xprtp: XPRT in which the SKB has to be written.
 * @skb: SKB to be written.
 * @pkt: IPC packet, for reference count purposes
 *
 * @return: return number of bytes written on success,
 *          standard Linux error codes on failure.
 */
static int ipc_router_mhi_dev_write_skb(
	struct ipc_router_mhi_dev_xprt *mhi_xprtp,
	struct sk_buff *skb, struct rr_packet *pkt)
{
	struct mhi_req *wreq;

	if (skb->len > mhi_xprtp->ch_hndl.max_packet_size) {
		IPC_RTR_ERR("%s: Packet size is above the limit\n", __func__);
		return -EINVAL;
	}

	wait_event_interruptible(mhi_xprtp->write_wait_q,
		!mhi_dev_channel_isempty(mhi_xprtp->ch_hndl.in_handle) ||
					 !mhi_xprtp->ch_hndl.in_chan_enabled);
	mutex_lock(&mhi_xprtp->ch_hndl.state_lock);
	if (!mhi_xprtp->ch_hndl.in_chan_enabled) {
		mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);
		IPC_RTR_ERR("%s: %s chnl reset\n",
			    __func__, mhi_xprtp->xprt_name);
		return -ENETRESET;
	}

	spin_lock_irq(&mhi_xprtp->wr_req_lock);
	if (list_empty(&mhi_xprtp->wr_req_list)) {
		IPC_RTR_ERR("%s: Write request pool empty\n", __func__);
		spin_unlock_irq(&mhi_xprtp->wr_req_lock);
		mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);
		return -ENOMEM;
	}
	wreq = container_of(mhi_xprtp->wr_req_list.next,
			    struct mhi_req, list);
	list_del_init(&wreq->list);

	kref_get(&pkt->ref);

	wreq->client = mhi_xprtp->ch_hndl.in_handle;
	wreq->context = pkt;
	wreq->buf = skb->data;
	wreq->len = skb->len;
	if (list_empty(&wreq->list))
		wreq->snd_cmpl = 1;
	else
		wreq->snd_cmpl = 0;
	spin_unlock_irq(&mhi_xprtp->wr_req_lock);

	if (mhi_dev_write_channel(wreq) < 0) {
		spin_lock_irq(&mhi_xprtp->wr_req_lock);
		list_add_tail(&wreq->list, &mhi_xprtp->wr_req_list);
		spin_unlock_irq(&mhi_xprtp->wr_req_lock);
		kref_put(&pkt->ref, ipc_router_mhi_dev_release_pkt);
		mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);
		IPC_RTR_ERR("%s: Error queueing mhi_xfer\n", __func__);
		return -EFAULT;
	}

	mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);

	return skb->len;
}

/**
 * ipc_router_mhi_dev_write() - Write to XPRT
 * @data: Data to be written to the XPRT.
 * @len: Length of the data to be written.
 * @xprt: XPRT to which the data has to be written.
 *
 * @return: Data Length on success, standard Linux error codes on failure.
 */
static int ipc_router_mhi_dev_write(void *data,
	uint32_t len, struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *ipc_rtr_pkt;
	struct rr_packet *cloned_pkt;
	int rc = 0;
	struct ipc_router_mhi_dev_xprt *mhi_xprtp =
		container_of(xprt, struct ipc_router_mhi_dev_xprt, xprt);

	if (!pkt)
		return -EINVAL;

	if (!len || pkt->length != len)
		return -EINVAL;

	cloned_pkt = clone_pkt(pkt);
	if (!cloned_pkt) {
		pr_err("%s: Error in cloning packet while tx\n", __func__);
		return -ENOMEM;
	}
	D("%s: Ready to write %d bytes\n", __func__, len);
	skb_queue_walk(cloned_pkt->pkt_fragment_q, ipc_rtr_pkt) {
		rc = ipc_router_mhi_dev_write_skb(mhi_xprtp, ipc_rtr_pkt,
						  cloned_pkt);
		if (rc < 0) {
			IPC_RTR_ERR("%s: Error writing SKB %d\n",
				    __func__, rc);
			break;
		}
	}

	kref_put(&cloned_pkt->ref, ipc_router_mhi_dev_release_pkt);
	if (rc < 0)
		return rc;
	else
		return len;
}

/**
 * ipc_router_mhi_dev_close() - Close the XPRT
 * @xprt: XPRT which needs to be closed.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int ipc_router_mhi_dev_close(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_mhi_dev_xprt *mhi_xprtp;

	if (!xprt)
		return -EINVAL;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_dev_xprt, xprt);

	mutex_lock(&mhi_xprtp->ch_hndl.state_lock);
	mhi_xprtp->ch_hndl.out_chan_enabled = false;
	mhi_xprtp->ch_hndl.in_chan_enabled = false;
	mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);
	flush_workqueue(mhi_xprtp->wq);

	return 0;
}

static int mhi_dev_xprt_alloc_write_reqs(
	struct ipc_router_mhi_dev_xprt *mhi_xprtp)
{
	int i;

	mhi_xprtp->wreqs = kcalloc(MAX_IPCR_WR_REQ,
				   sizeof(struct mhi_req),
				   GFP_KERNEL);
	if (!mhi_xprtp->wreqs) {
		IPC_RTR_ERR("%s: Write reqs alloc failed\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&mhi_xprtp->wr_req_list);
	for (i = 0; i < MAX_IPCR_WR_REQ; ++i) {
		mhi_xprtp->wreqs[i].chan = mhi_xprtp->ch_hndl.in_chan_id;
		mhi_xprtp->wreqs[i].mode = IPA_DMA_ASYNC;
		mhi_xprtp->wreqs[i].client_cb = mhi_xprt_write_completion_cb;
		list_add_tail(&mhi_xprtp->wreqs[i].list,
			      &mhi_xprtp->wr_req_list);
	}

	D("%s: write reqs allocation successful\n", __func__);
	return 0;
}

/**
 * ipc_router_mhi_dev_driver_register() - register for MHI channels
 *
 * @mhi_xprtp: pointer to IPC router mhi xprt structure.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when a new XPRT is added and the MHI
 * channel is ready.
 */
static int ipc_router_mhi_dev_driver_register(
	struct ipc_router_mhi_dev_xprt *mhi_xprtp)
{
	int rc;

	rc = mhi_dev_open_channel(mhi_xprtp->ch_hndl.out_chan_id,
		&mhi_xprtp->ch_hndl.out_handle, mhi_xprt_event_notifier);
	if (rc) {
		IPC_RTR_ERR("%s Failed to open chan 0x%x, rc %d\n",
			__func__, mhi_xprtp->ch_hndl.out_chan_id, rc);
		goto exit;
	}
	mutex_lock(&mhi_xprtp->ch_hndl.state_lock);
	mhi_xprtp->ch_hndl.out_chan_enabled = true;
	mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);

	rc = mhi_dev_open_channel(mhi_xprtp->ch_hndl.in_chan_id,
		&mhi_xprtp->ch_hndl.in_handle, mhi_xprt_event_notifier);
	if (rc) {
		IPC_RTR_ERR("%s Failed to open chan 0x%x, rc %d\n",
			__func__, mhi_xprtp->ch_hndl.in_chan_id, rc);
		goto exit;
	}
	mutex_lock(&mhi_xprtp->ch_hndl.state_lock);
	mhi_xprtp->ch_hndl.in_chan_enabled = true;
	mutex_unlock(&mhi_xprtp->ch_hndl.state_lock);

	rc = mhi_dev_xprt_alloc_write_reqs(mhi_xprtp);
	if (rc)
		goto exit;

	/* Register the XPRT before receiving any data */
	msm_ipc_router_xprt_notify(&mhi_xprtp->xprt,
			   IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	  __func__, mhi_xprtp->xprt.name);

exit:
	return rc;
}

/**
 * mhi_xprt_enable_cb() - Enable the MHI link for communication
 */
static void mhi_dev_xprt_state_cb(struct mhi_dev_client_cb_data *cb_data)
{
	int rc;
	struct ipc_router_mhi_dev_xprt *mhi_xprtp =
		(struct ipc_router_mhi_dev_xprt *)cb_data->user_data;

	/* Channel already enabled */
	if (mhi_xprtp->ch_hndl.in_chan_enabled)
		return;

	rc = ipc_router_mhi_dev_driver_register(mhi_xprtp);
	if (rc)
		IPC_RTR_ERR("%s: Failed to regiter for MHI channels\n",
			    __func__);
}

/**
 * ipc_router_mhi_dev_config_init() - init MHI xprt configs
 *
 * @mhi_xprt_config: pointer to MHI xprt configurations.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the MHI XPRT pointer with
 * the MHI XPRT configurations from device tree.
 */
static int ipc_router_mhi_dev_config_init(
	struct ipc_router_mhi_dev_xprt_config *mhi_xprt_config,
	struct device *dev)
{
	char wq_name[XPRT_NAME_LEN];
	int rc = 0;

	mhi_xprtp = devm_kzalloc(dev, sizeof(struct ipc_router_mhi_dev_xprt),
				 GFP_KERNEL);
	if (IS_ERR_OR_NULL(mhi_xprtp)) {
		IPC_RTR_ERR("%s: devm_kzalloc() failed for mhi_xprtp:%s\n",
			__func__, mhi_xprt_config->xprt_name);
		return -ENOMEM;
	}

	scnprintf(wq_name, XPRT_NAME_LEN, "MHI_DEV_XPRT%x:%x",
		  mhi_xprt_config->out_chan_id, mhi_xprt_config->in_chan_id);
	mhi_xprtp->wq = create_singlethread_workqueue(wq_name);
	if (!mhi_xprtp->wq) {
		IPC_RTR_ERR("%s: %s create WQ failed\n",
			__func__, mhi_xprt_config->xprt_name);
		kfree(mhi_xprtp);
		return -EFAULT;
	}

	INIT_WORK(&mhi_xprtp->read_work, mhi_xprt_read_data);
	init_waitqueue_head(&mhi_xprtp->write_wait_q);
	mhi_xprtp->xprt_version = mhi_xprt_config->xprt_version;
	mhi_xprtp->xprt_option = mhi_xprt_config->xprt_option;
	strlcpy(mhi_xprtp->xprt_name, mhi_xprt_config->xprt_name,
		XPRT_NAME_LEN);

	/* Initialize XPRT operations and parameters registered with IPC RTR */
	mhi_xprtp->xprt.link_id = mhi_xprt_config->link_id;
	mhi_xprtp->xprt.name = mhi_xprtp->xprt_name;
	mhi_xprtp->xprt.get_version = ipc_router_mhi_dev_get_xprt_version;
	mhi_xprtp->xprt.set_version = ipc_router_mhi_dev_set_xprt_version;
	mhi_xprtp->xprt.get_option = ipc_router_mhi_dev_get_xprt_option;
	mhi_xprtp->xprt.read_avail = NULL;
	mhi_xprtp->xprt.read = NULL;
	mhi_xprtp->xprt.write_avail = NULL;
	mhi_xprtp->xprt.write = ipc_router_mhi_dev_write;
	mhi_xprtp->xprt.close = ipc_router_mhi_dev_close;
	mhi_xprtp->xprt.sft_close_done = NULL;
	mhi_xprtp->xprt.priv = NULL;

	/* Initialize channel handle parameters */
	mhi_xprtp->ch_hndl.out_chan_id = mhi_xprt_config->out_chan_id;
	mhi_xprtp->ch_hndl.in_chan_id = mhi_xprt_config->in_chan_id;
	mutex_init(&mhi_xprtp->ch_hndl.state_lock);
	mhi_xprtp->ch_hndl.max_packet_size = IPC_ROUTER_MHI_XPRT_MAX_PKT_SIZE;
	mhi_xprtp->ch_hndl.mhi_xprtp = mhi_xprtp;
	init_completion(&mhi_xprtp->read_done);
	spin_lock_init(&mhi_xprtp->wr_req_lock);

	/* Register callback to mhi_dev */
	rc = mhi_register_state_cb(mhi_dev_xprt_state_cb, mhi_xprtp,
		mhi_xprtp->ch_hndl.in_chan_id);
	if (rc == -EEXIST) {
		rc = ipc_router_mhi_dev_driver_register(mhi_xprtp);
		if (rc)
			IPC_RTR_ERR("%s: Failed to regiter for MHI channels\n",
				    __func__);
	}

	return rc;
}

/**
 * parse_devicetree() - parse device tree binding
 *
 * @node: pointer to device tree node
 * @mhi_xprt_config: pointer to MHI XPRT configurations
 *
 * @return: 0 on success, -ENODEV on failure.
 */
static int parse_devicetree(struct device_node *node,
		struct ipc_router_mhi_dev_xprt_config *mhi_xprt_config)
{
	int rc;
	uint32_t out_chan_id;
	uint32_t in_chan_id;
	const char *remote_ss;
	uint32_t link_id;
	uint32_t version;
	char *key;

	key = "qcom,out-chan-id";
	rc = of_property_read_u32(node, key, &out_chan_id);
	if (rc)
		goto error;
	mhi_xprt_config->out_chan_id = out_chan_id;

	key = "qcom,in-chan-id";
	rc = of_property_read_u32(node, key, &in_chan_id);
	if (rc)
		goto error;
	mhi_xprt_config->in_chan_id = in_chan_id;

	key = "qcom,xprt-remote";
	remote_ss = of_get_property(node, key, NULL);
	if (!remote_ss)
		goto error;

	key = "qcom,xprt-linkid";
	rc = of_property_read_u32(node, key, &link_id);
	if (rc)
		goto error;
	mhi_xprt_config->link_id = link_id;

	key = "qcom,xprt-version";
	rc = of_property_read_u32(node, key, &version);
	if (rc)
		goto error;
	mhi_xprt_config->xprt_version = version;

	mhi_xprt_config->xprt_option = 0;

	scnprintf(mhi_xprt_config->xprt_name, XPRT_NAME_LEN, "%s_IPCRTR",
		  remote_ss);

	return 0;
error:
	IPC_RTR_ERR("%s: missing key: %s\n", __func__, key);
	return -ENODEV;
}

/**
 * ipc_router_mhi_dev_xprt_probe() - Probe an MHI xprt
 * @pdev: Platform device corresponding to MHI xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to an MHI transport.
 */
static int ipc_router_mhi_dev_xprt_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct ipc_router_mhi_dev_xprt_config mhi_xprt_config;

	if (pdev && pdev->dev.of_node) {
		rc = parse_devicetree(pdev->dev.of_node, &mhi_xprt_config);
		if (rc) {
			IPC_RTR_ERR("%s: failed to parse device tree\n",
				    __func__);
			return rc;
		}

		rc = ipc_router_mhi_dev_config_init(&mhi_xprt_config,
						    &pdev->dev);
		if (rc) {
			if (rc == -ENXIO)
				return -EPROBE_DEFER;
			IPC_RTR_ERR("%s: init failed\n", __func__);
			return rc;
		}
	}
	return rc;
}

static struct of_device_id ipc_router_mhi_dev_xprt_match_table[] = {
	{ .compatible = "qcom,ipc-router-mhi-dev-xprt" },
	{},
};

static struct platform_driver ipc_router_mhi_dev_xprt_driver = {
	.probe = ipc_router_mhi_dev_xprt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ipc_router_mhi_dev_xprt_match_table,
	},
};

static int __init ipc_router_mhi_dev_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&ipc_router_mhi_dev_xprt_driver);
	if (rc) {
		IPC_RTR_ERR(
			"%s: ipc_router_mhi_dev_xprt_driver reg. failed %d\n",
			__func__, rc);
		return rc;
	}
	return 0;
}

module_init(ipc_router_mhi_dev_xprt_init);
MODULE_DESCRIPTION("IPC Router MHI_DEV XPRT");
MODULE_LICENSE("GPL v2");
