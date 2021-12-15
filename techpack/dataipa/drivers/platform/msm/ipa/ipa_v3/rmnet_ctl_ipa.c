// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/ipa.h>
#include <uapi/linux/msm_rmnet.h>
#include "ipa_i.h"

enum ipa_rmnet_ctl_state {
	IPA_RMNET_CTL_NOT_REG,
	IPA_RMNET_CTL_REGD, /* rmnet_ctl register */
	IPA_RMNET_CTL_PIPE_READY, /* sys pipe setup */
	IPA_RMNET_CTL_START, /* rmnet_ctl register + pipe setup */
};

#define IPA_RMNET_CTL_PIPE_NOT_READY (0)
#define IPA_RMNET_CTL_PIPE_TX_READY (1 << 0)
#define IPA_RMNET_CTL_PIPE_RX_READY (1 << 1)
#define IPA_RMNET_CTL_PIPE_READY_ALL (IPA_RMNET_CTL_PIPE_TX_READY | \
	IPA_RMNET_CTL_PIPE_RX_READY) /* TX Ready + RX ready */


#define IPA_WWAN_CONS_DESC_FIFO_SZ 256
#define RMNET_CTRL_QUEUE_MAX (2 * IPA_WWAN_CONS_DESC_FIFO_SZ)

struct ipa3_rmnet_ctl_cb_info {
	ipa_rmnet_ctl_ready_cb ready_cb;
	ipa_rmnet_ctl_stop_cb stop_cb;
	ipa_rmnet_ctl_rx_notify_cb rx_notify_cb;
	void *ready_cb_user_data;
	void *stop_cb_user_data;
	void *rx_notify_cb_user_data;
};

struct ipa3_rmnet_ctl_stats {
	atomic_t outstanding_pkts;
	u32 tx_pkt_sent;
	u32 rx_pkt_rcvd;
	u64 tx_byte_sent;
	u64 rx_byte_rcvd;
	u32 tx_pkt_dropped;
	u32 rx_pkt_dropped;
	u64 tx_byte_dropped;
	u64 rx_byte_dropped;
};

struct rmnet_ctl_ipa3_context {
	struct ipa3_rmnet_ctl_stats stats;
	enum ipa_rmnet_ctl_state state;
	u8 pipe_state;
	struct ipa_sys_connect_params apps_to_ipa_low_lat_ep_cfg;
	struct ipa_sys_connect_params ipa_to_apps_low_lat_ep_cfg;
	u32 apps_to_ipa3_low_lat_hdl;
	u32 ipa3_to_apps_low_lat_hdl;
	spinlock_t tx_lock;
	struct ipa3_rmnet_ctl_cb_info cb_info;
	struct sk_buff_head tx_queue;
	u32 rmnet_ctl_pm_hdl;
	struct mutex lock;
	struct workqueue_struct *wq;
};

static struct rmnet_ctl_ipa3_context *rmnet_ctl_ipa3_ctx;

static void rmnet_ctl_wakeup_ipa(struct work_struct *work);
static DECLARE_DELAYED_WORK(rmnet_ctl_wakeup_work,
	rmnet_ctl_wakeup_ipa);
static void apps_rmnet_ctl_tx_complete_notify(void *priv,
	enum ipa_dp_evt_type evt, unsigned long data);
static void apps_rmnet_ctl_receive_notify(void *priv,
	enum ipa_dp_evt_type evt, unsigned long data);
static int ipa3_rmnet_ctl_register_pm_client(void);
static void ipa3_rmnet_ctl_deregister_pm_client(void);

int ipa3_rmnet_ctl_init(void)
{
	char buff[IPA_RESOURCE_NAME_MAX];

	if (!ipa3_ctx) {
		IPAERR("ipa3_ctx was not initialized\n");
		return -EINVAL;
	}

	if (ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_LOW_LAT_PROD) == -1 ||
		ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_LOW_LAT_CONS) == -1)
	{
		IPAERR("invalid low lat endpoints\n");
		return -EINVAL;
	}

	rmnet_ctl_ipa3_ctx = kzalloc(sizeof(*rmnet_ctl_ipa3_ctx),
			GFP_KERNEL);

	if (!rmnet_ctl_ipa3_ctx)
		return -ENOMEM;

	snprintf(buff, IPA_RESOURCE_NAME_MAX, "rmnet_ctlwq");
	rmnet_ctl_ipa3_ctx->wq = alloc_workqueue(buff,
		WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS, 1);
	if (!rmnet_ctl_ipa3_ctx->wq) {
		kfree(rmnet_ctl_ipa3_ctx);
		return -ENOMEM;
	}
	memset(&rmnet_ctl_ipa3_ctx->apps_to_ipa_low_lat_ep_cfg, 0,
		sizeof(struct ipa_sys_connect_params));
	memset(&rmnet_ctl_ipa3_ctx->ipa_to_apps_low_lat_ep_cfg, 0,
		sizeof(struct ipa_sys_connect_params));
	skb_queue_head_init(&rmnet_ctl_ipa3_ctx->tx_queue);
	rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_NOT_REG;
	mutex_init(&rmnet_ctl_ipa3_ctx->lock);
	spin_lock_init(&rmnet_ctl_ipa3_ctx->tx_lock);
	rmnet_ctl_ipa3_ctx->pipe_state = IPA_RMNET_CTL_PIPE_NOT_READY;
	return 0;
}

int ipa3_register_rmnet_ctl_cb(
	void (*ipa_rmnet_ctl_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ctl_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ctl_rx_notify_cb)(
	void *user_data3, void *rx_data),
	void *user_data3)
{
	/* check ipa3_ctx existed or not */
	if (!ipa3_ctx) {
		IPADBG("rmnet_ctl_ctx haven't initialized\n");
		return -EAGAIN;
	}

	if (!ipa3_ctx->rmnet_ctl_enable) {
		IPAERR("low lat pipes are not supported");
		return -ENXIO;
	}

	if (!rmnet_ctl_ipa3_ctx) {
		IPADBG("rmnet_ctl_ctx haven't initialized\n");
		return -EAGAIN;
	}

	mutex_lock(&rmnet_ctl_ipa3_ctx->lock);
	if (rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_NOT_REG &&
		rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_PIPE_READY) {
		IPADBG("rmnet_ctl registered already\n", __func__);
		mutex_unlock(&rmnet_ctl_ipa3_ctx->lock);
		return -EEXIST;
	}
	rmnet_ctl_ipa3_ctx->cb_info.ready_cb = ipa_rmnet_ctl_ready_cb;
	rmnet_ctl_ipa3_ctx->cb_info.ready_cb_user_data = user_data1;
	rmnet_ctl_ipa3_ctx->cb_info.stop_cb = ipa_rmnet_ctl_stop_cb;
	rmnet_ctl_ipa3_ctx->cb_info.stop_cb_user_data = user_data2;
	rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb = ipa_rmnet_ctl_rx_notify_cb;
	rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb_user_data = user_data3;
	if (rmnet_ctl_ipa3_ctx->state == IPA_RMNET_CTL_NOT_REG) {
		rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_REGD;
	} else {
		(*ipa_rmnet_ctl_ready_cb)(user_data1);
		rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_START;
	}
	ipa3_rmnet_ctl_register_pm_client();
	mutex_unlock(&rmnet_ctl_ipa3_ctx->lock);
	IPADBG("rmnet_ctl registered successfually\n");
	return 0;
}

int ipa3_unregister_rmnet_ctl_cb(void)
{
	/* check ipa3_ctx existed or not */
	if (!ipa3_ctx) {
		IPADBG("IPA driver haven't initialized\n");
		return -EAGAIN;
	}

	if (!ipa3_ctx->rmnet_ctl_enable) {
		IPAERR("low lat pipe is disabled");
		return -ENXIO;
	}

	if (!rmnet_ctl_ipa3_ctx) {
		IPADBG("rmnet_ctl_ctx haven't initialized\n");
		return -EAGAIN;
	}

	mutex_lock(&rmnet_ctl_ipa3_ctx->lock);
	if (rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_REGD &&
		rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_START) {
		IPADBG("rmnet_ctl unregistered already\n", __func__);
		mutex_unlock(&rmnet_ctl_ipa3_ctx->lock);
		return 0;
	}
	rmnet_ctl_ipa3_ctx->cb_info.ready_cb = NULL;
	rmnet_ctl_ipa3_ctx->cb_info.ready_cb_user_data = NULL;
	rmnet_ctl_ipa3_ctx->cb_info.stop_cb = NULL;
	rmnet_ctl_ipa3_ctx->cb_info.stop_cb_user_data = NULL;
	rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb = NULL;
	rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb_user_data = NULL;
	if (rmnet_ctl_ipa3_ctx->state == IPA_RMNET_CTL_REGD)
		rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_NOT_REG;
	else
		rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_PIPE_READY;

	ipa3_rmnet_ctl_deregister_pm_client();
	mutex_unlock(&rmnet_ctl_ipa3_ctx->lock);

	IPADBG("rmnet_ctl unregistered successfually\n");
	return 0;
}

int ipa3_setup_apps_low_lat_cons_pipe(void)
{
	struct ipa_sys_connect_params *ipa_low_lat_ep_cfg;
	int ret = 0;
	int ep_idx;

	if (!ipa3_ctx->rmnet_ctl_enable) {
		IPAERR("low lat pipe is disabled");
		return -ENXIO;
	}
	ep_idx = ipa_get_ep_mapping(
		IPA_CLIENT_APPS_WAN_LOW_LAT_CONS);
	if (ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPADBG("Low lat datapath not supported\n");
		return -ENXIO;
	}
	if (rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_NOT_REG &&
		rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_REGD) {
		IPADBG("rmnet_ctl in bad state %d\n",
			rmnet_ctl_ipa3_ctx->state);
		return -ENXIO;
	}
	ipa_low_lat_ep_cfg =
		&rmnet_ctl_ipa3_ctx->ipa_to_apps_low_lat_ep_cfg;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en =
		IPA_ENABLE_CS_DL_QMAP;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.aggr.aggr_byte_limit =
		0;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.aggr.aggr_pkt_limit =
		0;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 8;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata_valid
		= 1;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata
		= 1;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size_valid
		= 1;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size
		= 2;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid
		= true;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad
		= 0;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding
		= true;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset
		= 0;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_little_endian
		= 0;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.metadata_mask.metadata_mask
		= 0xFF000000;
	ipa_low_lat_ep_cfg->client = IPA_CLIENT_APPS_WAN_LOW_LAT_CONS;
	ipa_low_lat_ep_cfg->notify = apps_rmnet_ctl_receive_notify;
	ipa_low_lat_ep_cfg->priv = NULL;
	ipa_low_lat_ep_cfg->desc_fifo_sz =
		IPA_WWAN_CONS_DESC_FIFO_SZ * IPA_FIFO_ELEMENT_SIZE;
	ret = ipa_setup_sys_pipe(
		&rmnet_ctl_ipa3_ctx->ipa_to_apps_low_lat_ep_cfg,
		&rmnet_ctl_ipa3_ctx->ipa3_to_apps_low_lat_hdl);
	if (ret) {
		IPADBG("Low lat pipe setup fails\n");
		return ret;
	}
	rmnet_ctl_ipa3_ctx->pipe_state |= IPA_RMNET_CTL_PIPE_RX_READY;
	if (rmnet_ctl_ipa3_ctx->cb_info.ready_cb) {
		(*(rmnet_ctl_ipa3_ctx->cb_info.ready_cb))
			(rmnet_ctl_ipa3_ctx->cb_info.ready_cb_user_data);
	}
	/*
	 * if no ready_cb yet, which means rmnet_ctl not
	 * register to IPA, we will move state to pipe
	 * ready and will wait for register event
	 * coming and move to start state.
	 * The ready_cb will called from regsiter itself.
	 */
	mutex_lock(&rmnet_ctl_ipa3_ctx->lock);
	if (rmnet_ctl_ipa3_ctx->state == IPA_RMNET_CTL_NOT_REG)
		rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_PIPE_READY;
	else
		rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_START;
	mutex_unlock(&rmnet_ctl_ipa3_ctx->lock);

	return 0;
}

int ipa3_setup_apps_low_lat_prod_pipe(void)
{
	struct ipa_sys_connect_params *ipa_low_lat_ep_cfg;
	int ret = 0;
	int ep_idx;

	if (!ipa3_ctx->rmnet_ctl_enable) {
		IPAERR("Low lat pipe is disabled");
		return -ENXIO;
	}
	ep_idx = ipa_get_ep_mapping(
		IPA_CLIENT_APPS_WAN_LOW_LAT_PROD);
	if (ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("low lat pipe not supported\n");
		return -EFAULT;
	}
	ipa_low_lat_ep_cfg =
		&rmnet_ctl_ipa3_ctx->apps_to_ipa_low_lat_ep_cfg;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 8;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en =
		IPA_ENABLE_CS_OFFLOAD_UL;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.cfg.cs_metadata_hdr_offset
		= 1;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.aggr.aggr_en =
		IPA_BYPASS_AGGR;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	/* modem want offset at 0! */
	ipa_low_lat_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata = 0;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.mode.dst =
		IPA_CLIENT_Q6_WAN_CONS;
	ipa_low_lat_ep_cfg->ipa_ep_cfg.mode.mode =
		IPA_DMA;
	ipa_low_lat_ep_cfg->client =
		IPA_CLIENT_APPS_WAN_LOW_LAT_PROD;
	ipa_low_lat_ep_cfg->notify =
		apps_rmnet_ctl_tx_complete_notify;
	ipa_low_lat_ep_cfg->desc_fifo_sz =
		IPA_SYS_TX_DATA_DESC_FIFO_SZ;
	ipa_low_lat_ep_cfg->priv = NULL;

	ret = ipa_setup_sys_pipe(ipa_low_lat_ep_cfg,
		&rmnet_ctl_ipa3_ctx->apps_to_ipa3_low_lat_hdl);
	if (ret) {
		IPAERR("failed to config apps low lat prod pipe\n");
		return ret;
	}
	rmnet_ctl_ipa3_ctx->pipe_state |= IPA_RMNET_CTL_PIPE_TX_READY;
	return 0;
}

int ipa3_teardown_apps_low_lat_pipes(void)
{
	int ret = 0;

	if (rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_PIPE_READY &&
		rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_START &&
		rmnet_ctl_ipa3_ctx->pipe_state == IPA_RMNET_CTL_PIPE_NOT_READY) {
		IPAERR("rmnet_ctl in bad state %d\n",
			rmnet_ctl_ipa3_ctx->state);
		return -EFAULT;
	}
	if (rmnet_ctl_ipa3_ctx->pipe_state == IPA_RMNET_CTL_PIPE_READY ||
		rmnet_ctl_ipa3_ctx->state == IPA_RMNET_CTL_START) {
		if (rmnet_ctl_ipa3_ctx->cb_info.stop_cb) {
			(*(rmnet_ctl_ipa3_ctx->cb_info.stop_cb))
				(rmnet_ctl_ipa3_ctx->cb_info.stop_cb_user_data);
		} else {
			IPAERR("Invalid stop_cb\n");
			return -EFAULT;
		}
		if (rmnet_ctl_ipa3_ctx->state == IPA_RMNET_CTL_PIPE_READY)
			rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_NOT_REG;
		else
			rmnet_ctl_ipa3_ctx->state = IPA_RMNET_CTL_REGD;
	}
	if (rmnet_ctl_ipa3_ctx->pipe_state & IPA_RMNET_CTL_PIPE_RX_READY) {
		ret = ipa3_teardown_sys_pipe(
			rmnet_ctl_ipa3_ctx->ipa3_to_apps_low_lat_hdl);
		if (ret < 0) {
			IPAERR("Failed to teardown APPS->IPA low lat pipe\n");
			return ret;
		}
		rmnet_ctl_ipa3_ctx->ipa3_to_apps_low_lat_hdl = -1;
		rmnet_ctl_ipa3_ctx->pipe_state &= ~IPA_RMNET_CTL_PIPE_RX_READY;
	}

	if (rmnet_ctl_ipa3_ctx->pipe_state & IPA_RMNET_CTL_PIPE_TX_READY) {
		ret = ipa3_teardown_sys_pipe(
			rmnet_ctl_ipa3_ctx->apps_to_ipa3_low_lat_hdl);
		if (ret < 0) {
			return ret;
			IPAERR("Failed to teardown APPS->IPA low lat pipe\n");
		}
		rmnet_ctl_ipa3_ctx->apps_to_ipa3_low_lat_hdl = -1;
		rmnet_ctl_ipa3_ctx->pipe_state &= ~IPA_RMNET_CTL_PIPE_TX_READY;
	}
	return ret;
}

int ipa3_rmnet_ctl_xmit(struct sk_buff *skb)
{
	int ret;
	int len;
	unsigned long flags;

	if (!ipa3_ctx->rmnet_ctl_enable) {
		IPAERR("low lat pipe not supported\n");
		kfree_skb(skb);
		return 0;
	}

	spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
	/* we cannot infinitely queue the packet */
	if (skb_queue_len(&rmnet_ctl_ipa3_ctx->tx_queue) >=
		RMNET_CTRL_QUEUE_MAX) {
		IPAERR("rmnet_ctl tx queue full\n");
		rmnet_ctl_ipa3_ctx->stats.tx_pkt_dropped++;
		rmnet_ctl_ipa3_ctx->stats.tx_byte_dropped +=
			skb->len;
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		kfree_skb(skb);
		return -EAGAIN;
	}

	if (rmnet_ctl_ipa3_ctx->state != IPA_RMNET_CTL_START) {
		IPAERR("bad rmnet_ctl state %d\n",
			rmnet_ctl_ipa3_ctx->state);
		rmnet_ctl_ipa3_ctx->stats.tx_pkt_dropped++;
		rmnet_ctl_ipa3_ctx->stats.tx_byte_dropped +=
			skb->len;
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		kfree_skb(skb);
		return 0;
	}

	/* if queue is not empty, means we still have pending wq */
	if (skb_queue_len(&rmnet_ctl_ipa3_ctx->tx_queue) != 0) {
		skb_queue_tail(&rmnet_ctl_ipa3_ctx->tx_queue, skb);
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		return 0;
	}

	/* rmnet_ctl is calling from atomic context */
	ret = ipa_pm_activate(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
	if (ret == -EINPROGRESS) {
		skb_queue_tail(&rmnet_ctl_ipa3_ctx->tx_queue, skb);
		/*
		 * delayed work is required here since we need to
		 * reschedule in the same workqueue context on error
		 */
		queue_delayed_work(rmnet_ctl_ipa3_ctx->wq,
			&rmnet_ctl_wakeup_work, 0);
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		return 0;
	} else if (ret) {
		IPAERR("[%s] fatal: ipa pm activate failed %d\n",
			__func__, ret);
		rmnet_ctl_ipa3_ctx->stats.tx_pkt_dropped++;
		rmnet_ctl_ipa3_ctx->stats.tx_byte_dropped +=
			skb->len;
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		kfree_skb(skb);
		return 0;
	}
	spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock, flags);

	len = skb->len;
	/*
	 * both data packets and command will be routed to
	 * IPA_CLIENT_Q6_WAN_CONS based on DMA settings
	 */
	ret = ipa3_tx_dp(IPA_CLIENT_APPS_WAN_LOW_LAT_PROD, skb, NULL);
	if (ret) {
		if (ret == -EPIPE) {
			IPAERR("Low lat fatal: pipe is not valid\n");
			spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock,
				flags);
			rmnet_ctl_ipa3_ctx->stats.tx_pkt_dropped++;
			rmnet_ctl_ipa3_ctx->stats.tx_byte_dropped +=
				skb->len;
			spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
				flags);
			kfree_skb(skb);
			return 0;
		}
		spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
		skb_queue_head(&rmnet_ctl_ipa3_ctx->tx_queue, skb);
		ret = 0;
		goto out;
	}

	spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
	atomic_inc(&rmnet_ctl_ipa3_ctx->stats.outstanding_pkts);
	rmnet_ctl_ipa3_ctx->stats.tx_pkt_sent++;
	rmnet_ctl_ipa3_ctx->stats.tx_byte_sent += len;
	ret = 0;

out:
	if (atomic_read(
		&rmnet_ctl_ipa3_ctx->stats.outstanding_pkts)
		== 0)
		ipa_pm_deferred_deactivate(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
	spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
	return ret;
}

static void rmnet_ctl_wakeup_ipa(struct work_struct *work)
{
	int ret;
	unsigned long flags;
	struct sk_buff *skb;
	int len = 0;

	/* calling from WQ */
	ret = ipa_pm_activate_sync(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
	if (ret) {
		IPAERR("[%s] fatal: ipa pm activate failed %d\n",
			__func__, ret);
		queue_delayed_work(rmnet_ctl_ipa3_ctx->wq,
			&rmnet_ctl_wakeup_work,
			msecs_to_jiffies(1));
		return;
	}

	spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
	/* dequeue the skb */
	while (skb_queue_len(&rmnet_ctl_ipa3_ctx->tx_queue) > 0) {
		skb = skb_dequeue(&rmnet_ctl_ipa3_ctx->tx_queue);
		if (skb == NULL)
			continue;
		len = skb->len;
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
		/*
		 * both data packets and command will be routed to
		 * IPA_CLIENT_Q6_WAN_CONS based on DMA settings
		 */
		ret = ipa3_tx_dp(IPA_CLIENT_APPS_WAN_LOW_LAT_PROD, skb, NULL);
		if (ret) {
			if (ret == -EPIPE) {
				/* try to drain skb from queue if pipe teardown */
				IPAERR_RL("Low lat fatal: pipe is not valid\n");
				spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock,
					flags);
				rmnet_ctl_ipa3_ctx->stats.tx_pkt_dropped++;
				rmnet_ctl_ipa3_ctx->stats.tx_byte_dropped +=
					skb->len;
				kfree_skb(skb);
				continue;
			}
			spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
			skb_queue_head(&rmnet_ctl_ipa3_ctx->tx_queue, skb);
			spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
			goto delayed_work;
		}

		atomic_inc(&rmnet_ctl_ipa3_ctx->stats.outstanding_pkts);
		spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
		rmnet_ctl_ipa3_ctx->stats.tx_pkt_sent++;
		rmnet_ctl_ipa3_ctx->stats.tx_byte_sent += len;
	}
	spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock, flags);
	goto out;

delayed_work:
	queue_delayed_work(rmnet_ctl_ipa3_ctx->wq,
		&rmnet_ctl_wakeup_work,
		msecs_to_jiffies(1));
out:
	if (atomic_read(
		&rmnet_ctl_ipa3_ctx->stats.outstanding_pkts)
		== 0) {
		ipa_pm_deferred_deactivate(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
	}

}

/**
 * apps_rmnet_ctl_tx_complete_notify() - Rx notify
 *
 * @priv: driver context
 * @evt: event type
 * @data: data provided with event
 *
 * Check that the packet is the one we sent and release it
 * This function will be called in defered context in IPA wq.
 */
static void apps_rmnet_ctl_tx_complete_notify(void *priv,
	enum ipa_dp_evt_type evt, unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	unsigned long flags;

	if (evt != IPA_WRITE_DONE) {
		IPAERR("unsupported evt on Tx callback, Drop the packet\n");
		spin_lock_irqsave(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		rmnet_ctl_ipa3_ctx->stats.tx_pkt_dropped++;
		rmnet_ctl_ipa3_ctx->stats.tx_byte_dropped +=
			skb->len;
		spin_unlock_irqrestore(&rmnet_ctl_ipa3_ctx->tx_lock,
			flags);
		kfree_skb(skb);
		return;
	}

	atomic_dec(&rmnet_ctl_ipa3_ctx->stats.outstanding_pkts);

	if (atomic_read(
		&rmnet_ctl_ipa3_ctx->stats.outstanding_pkts) == 0)
		ipa_pm_deferred_deactivate(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);

	kfree_skb(skb);
}

/**
 * apps_rmnet_ctl_receive_notify() - Rmnet_ctl RX notify
 *
 * @priv: driver context
 * @evt: event type
 * @data: data provided with event
 *
 * IPA will pass a packet to the Linux network stack with skb->data
 */
static void apps_rmnet_ctl_receive_notify(void *priv,
	enum ipa_dp_evt_type evt, unsigned long data)
{
	void *rx_notify_cb_rx_data;
	struct sk_buff *low_lat_data;
	int len;

	low_lat_data = (struct sk_buff *)data;
	if (low_lat_data == NULL) {
		IPAERR("Rx packet is invalid");
		return;
	}
	len = low_lat_data->len;
	if (evt == IPA_RECEIVE) {
		IPADBG_LOW("Rx packet was received");
		rx_notify_cb_rx_data = (void *)data;
		if (rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb) {
			(*(rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb))(
			rmnet_ctl_ipa3_ctx->cb_info.rx_notify_cb_user_data,
			rx_notify_cb_rx_data);
		} else
			goto fail;
		rmnet_ctl_ipa3_ctx->stats.rx_pkt_rcvd++;
		rmnet_ctl_ipa3_ctx->stats.rx_byte_rcvd +=
			len;
	} else {
		IPAERR("Invalid evt %d received in rmnet_ctl\n", evt);
		goto fail;
	}
	return;

fail:
	kfree_skb(low_lat_data);
	rmnet_ctl_ipa3_ctx->stats.rx_pkt_dropped++;
}


static int ipa3_rmnet_ctl_register_pm_client(void)
{
	int result;
	struct ipa_pm_register_params pm_reg;

	memset(&pm_reg, 0, sizeof(pm_reg));
	pm_reg.name = "rmnet_ctl";
	pm_reg.group = IPA_PM_GROUP_APPS;
	result = ipa_pm_register(&pm_reg, &rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
	if (result) {
		IPAERR("failed to create IPA PM client %d\n", result);
		return result;
	}

	IPAERR("%s register done\n", pm_reg.name);

	return 0;
}

static void ipa3_rmnet_ctl_deregister_pm_client(void)
{
	ipa_pm_deactivate_sync(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
	ipa_pm_deregister(rmnet_ctl_ipa3_ctx->rmnet_ctl_pm_hdl);
}

