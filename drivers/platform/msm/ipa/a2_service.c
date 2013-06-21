/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 *  A2 service component
 */

#include <net/ip.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <mach/sps.h>
#include <mach/msm_smsm.h>
#include <mach/socinfo.h>
#include <mach/ipa.h>
#include "ipa_i.h"

#define A2_NUM_PIPES				6
#define A2_SUMMING_THRESHOLD			4096
#define BUFFER_SIZE				2048
#define NUM_BUFFERS				32
#define BAM_CH_LOCAL_OPEN			0x1
#define BAM_CH_REMOTE_OPEN			0x2
#define BAM_CH_IN_RESET				0x4
#define BAM_MUX_HDR_MAGIC_NO			0x33fc
#define BAM_MUX_HDR_CMD_DATA			0
#define BAM_MUX_HDR_CMD_OPEN			1
#define BAM_MUX_HDR_CMD_CLOSE			2
#define BAM_MUX_HDR_CMD_STATUS			3
#define BAM_MUX_HDR_CMD_OPEN_NO_A2_PC		4
#define LOW_WATERMARK				2
#define HIGH_WATERMARK				4
#define A2_MUX_COMPLETION_TIMEOUT		(60*HZ)
#define ENABLE_DISCONNECT_ACK			0x1
#define A2_MUX_PADDING_LENGTH(len)		(4 - ((len) & 0x3))

struct bam_ch_info {
	u32			status;
	a2_mux_notify_cb	notify_cb;
	void			*user_data;
	spinlock_t		lock;
	int			num_tx_pkts;
	int			use_wm;
	u32			v4_hdr_hdl;
	u32			v6_hdr_hdl;
};
struct tx_pkt_info {
	struct sk_buff		*skb;
	char			is_cmd;
	u32			len;
	struct list_head	list_node;
	unsigned		ts_sec;
	unsigned long		ts_nsec;
};
struct bam_mux_hdr {
	u16			magic_num;
	u8			reserved;
	u8			cmd;
	u8			pad_len;
	u8			ch_id;
	u16			pkt_len;
};

struct a2_mux_context_type {
	u32 tethered_prod;
	u32 tethered_cons;
	u32 embedded_prod;
	u32 embedded_cons;
	int a2_mux_apps_pc_enabled;
	struct work_struct kickoff_ul_wakeup;
	struct work_struct kickoff_ul_power_down;
	struct work_struct kickoff_ul_request_resource;
	struct	bam_ch_info bam_ch[A2_MUX_NUM_CHANNELS];
	struct list_head bam_tx_pool;
	spinlock_t bam_tx_pool_spinlock;
	struct workqueue_struct *a2_mux_tx_workqueue;
	struct workqueue_struct *a2_mux_rx_workqueue;
	int a2_mux_initialized;
	bool bam_is_connected;
	bool bam_connect_in_progress;
	int a2_mux_send_power_vote_on_init_once;
	int a2_mux_sw_bridge_is_connected;
	bool a2_mux_dl_wakeup;
	u32 a2_device_handle;
	struct mutex wakeup_lock;
	struct completion ul_wakeup_ack_completion;
	struct completion bam_connection_completion;
	struct completion request_resource_completion;
	struct completion dl_wakeup_completion;
	rwlock_t ul_wakeup_lock;
	int wait_for_ack;
	struct wake_lock bam_wakelock;
	int a2_pc_disabled;
	spinlock_t wakelock_reference_lock;
	int wakelock_reference_count;
	int a2_pc_disabled_wakelock_skipped;
	int disconnect_ack;
	struct mutex smsm_cb_lock;
	int bam_dmux_uplink_vote;
};
static struct a2_mux_context_type *a2_mux_ctx;

static void handle_a2_mux_cmd(struct sk_buff *rx_skb);

static bool bam_ch_is_open(int index)
{
	return a2_mux_ctx->bam_ch[index].status ==
		(BAM_CH_LOCAL_OPEN | BAM_CH_REMOTE_OPEN);
}

static bool bam_ch_is_local_open(int index)
{
	return a2_mux_ctx->bam_ch[index].status &
		BAM_CH_LOCAL_OPEN;
}

static bool bam_ch_is_remote_open(int index)
{
	return a2_mux_ctx->bam_ch[index].status &
		BAM_CH_REMOTE_OPEN;
}

static bool bam_ch_is_in_reset(int index)
{
	return a2_mux_ctx->bam_ch[index].status &
		BAM_CH_IN_RESET;
}

static void set_tx_timestamp(struct tx_pkt_info *pkt)
{
	unsigned long long t_now;

	t_now = sched_clock();
	pkt->ts_nsec = do_div(t_now, 1000000000U);
	pkt->ts_sec = (unsigned)t_now;
}

static void verify_tx_queue_is_empty(const char *func)
{
	unsigned long flags;
	struct tx_pkt_info *info;
	int reported = 0;

	spin_lock_irqsave(&a2_mux_ctx->bam_tx_pool_spinlock, flags);
	list_for_each_entry(info, &a2_mux_ctx->bam_tx_pool, list_node) {
		if (!reported) {
			IPADBG("%s: tx pool not empty\n", func);
			reported = 1;
		}
		IPADBG("%s: node=%p ts=%u.%09lu\n", __func__,
			&info->list_node, info->ts_sec, info->ts_nsec);
	}
	spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock, flags);
}

static void grab_wakelock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&a2_mux_ctx->wakelock_reference_lock, flags);
	IPADBG("%s: ref count = %d\n",
		__func__,
		a2_mux_ctx->wakelock_reference_count);
	if (a2_mux_ctx->wakelock_reference_count == 0)
		wake_lock(&a2_mux_ctx->bam_wakelock);
	++a2_mux_ctx->wakelock_reference_count;
	spin_unlock_irqrestore(&a2_mux_ctx->wakelock_reference_lock, flags);
}

static void release_wakelock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&a2_mux_ctx->wakelock_reference_lock, flags);
	if (a2_mux_ctx->wakelock_reference_count == 0) {
		IPAERR("%s: bam_dmux wakelock not locked\n", __func__);
		dump_stack();
		spin_unlock_irqrestore(&a2_mux_ctx->wakelock_reference_lock,
				       flags);
		return;
	}
	IPADBG("%s: ref count = %d\n",
		__func__,
		a2_mux_ctx->wakelock_reference_count);
	--a2_mux_ctx->wakelock_reference_count;
	if (a2_mux_ctx->wakelock_reference_count == 0)
		wake_unlock(&a2_mux_ctx->bam_wakelock);
	spin_unlock_irqrestore(&a2_mux_ctx->wakelock_reference_lock, flags);
}

static void toggle_apps_ack(void)
{
	static unsigned int clear_bit; /* 0 = set the bit, else clear bit */

	IPADBG("%s: apps ack %d->%d\n", __func__,
			clear_bit & 0x1, ~clear_bit & 0x1);
	smsm_change_state(SMSM_APPS_STATE,
				clear_bit & SMSM_A2_POWER_CONTROL_ACK,
				~clear_bit & SMSM_A2_POWER_CONTROL_ACK);
	IPA_STATS_INC_CNT(ipa_ctx->stats.a2_power_apps_acks);
	clear_bit = ~clear_bit;
}

static void power_vote(int vote)
{
	IPADBG("%s: curr=%d, vote=%d\n",
		__func__,
		a2_mux_ctx->bam_dmux_uplink_vote, vote);
	if (a2_mux_ctx->bam_dmux_uplink_vote == vote)
		IPADBG("%s: warning - duplicate power vote\n", __func__);
	a2_mux_ctx->bam_dmux_uplink_vote = vote;
	if (vote) {
		smsm_change_state(SMSM_APPS_STATE, 0, SMSM_A2_POWER_CONTROL);
		IPA_STATS_INC_CNT(ipa_ctx->stats.a2_power_on_reqs_out);
	} else {
		smsm_change_state(SMSM_APPS_STATE, SMSM_A2_POWER_CONTROL, 0);
		IPA_STATS_INC_CNT(ipa_ctx->stats.a2_power_off_reqs_out);
	}
}

static inline void ul_powerdown(void)
{
	IPADBG("%s: powerdown\n", __func__);
	verify_tx_queue_is_empty(__func__);
	if (a2_mux_ctx->a2_pc_disabled)
		release_wakelock();
	else {
		a2_mux_ctx->wait_for_ack = 1;
		INIT_COMPLETION(a2_mux_ctx->ul_wakeup_ack_completion);
		power_vote(0);
	}
}

static void ul_wakeup(void)
{
	int ret;

	mutex_lock(&a2_mux_ctx->wakeup_lock);
	if (a2_mux_ctx->bam_is_connected &&
				!a2_mux_ctx->bam_connect_in_progress) {
		IPADBG("%s Already awake\n", __func__);
		mutex_unlock(&a2_mux_ctx->wakeup_lock);
		return;
	}
	if (a2_mux_ctx->a2_pc_disabled) {
		/*
		 * don't grab the wakelock the first time because it is
		 * already grabbed when a2 powers on
		 */
		if (likely(a2_mux_ctx->a2_pc_disabled_wakelock_skipped))
			grab_wakelock();
		else
			a2_mux_ctx->a2_pc_disabled_wakelock_skipped = 1;
		mutex_unlock(&a2_mux_ctx->wakeup_lock);
		return;
	}
	/*
	 * must wait for the previous power down request to have been acked
	 * chances are it already came in and this will just fall through
	 * instead of waiting
	 */
	if (a2_mux_ctx->wait_for_ack) {
		IPADBG("%s waiting for previous ack\n", __func__);
		ret = wait_for_completion_timeout(
					&a2_mux_ctx->ul_wakeup_ack_completion,
					A2_MUX_COMPLETION_TIMEOUT);
		a2_mux_ctx->wait_for_ack = 0;
		if (unlikely(ret == 0)) {
			IPAERR("%s previous ack from modem timed out\n",
				__func__);
			goto bail;
		}
	}
	INIT_COMPLETION(a2_mux_ctx->ul_wakeup_ack_completion);
	power_vote(1);
	IPADBG("%s waiting for wakeup ack\n", __func__);
	ret = wait_for_completion_timeout(&a2_mux_ctx->ul_wakeup_ack_completion,
					A2_MUX_COMPLETION_TIMEOUT);
	if (unlikely(ret == 0)) {
		IPAERR("%s wakup ack from modem timed out\n", __func__);
		goto bail;
	}
	INIT_COMPLETION(a2_mux_ctx->bam_connection_completion);
	if (!a2_mux_ctx->a2_mux_sw_bridge_is_connected) {
		ret = wait_for_completion_timeout(
			&a2_mux_ctx->bam_connection_completion,
			A2_MUX_COMPLETION_TIMEOUT);
		if (unlikely(ret == 0)) {
			IPAERR("%s modem power on timed out\n", __func__);
			goto bail;
		}
	}
	IPADBG("%s complete\n", __func__);
	mutex_unlock(&a2_mux_ctx->wakeup_lock);
	return;
bail:
	mutex_unlock(&a2_mux_ctx->wakeup_lock);
	BUG();
	return;
}

static void a2_mux_write_done(bool is_tethered, struct sk_buff *skb)
{
	struct tx_pkt_info *info;
	enum a2_mux_logical_channel_id lcid;
	unsigned long event_data;
	unsigned long flags;

	spin_lock_irqsave(&a2_mux_ctx->bam_tx_pool_spinlock, flags);
	info = list_first_entry(&a2_mux_ctx->bam_tx_pool,
			struct tx_pkt_info, list_node);
	if (unlikely(info->skb != skb)) {
		struct tx_pkt_info *errant_pkt;

		IPAERR("tx_pool mismatch next=%p list_node=%p, ts=%u.%09lu\n",
				a2_mux_ctx->bam_tx_pool.next,
				&info->list_node,
				info->ts_sec, info->ts_nsec
				);

		list_for_each_entry(errant_pkt,
				    &a2_mux_ctx->bam_tx_pool, list_node) {
			IPAERR("%s: node=%p ts=%u.%09lu\n", __func__,
			&errant_pkt->list_node, errant_pkt->ts_sec,
			errant_pkt->ts_nsec);
			if (errant_pkt->skb == skb)
				info = errant_pkt;

		}
		spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock,
				       flags);
		BUG();
	}
	list_del(&info->list_node);
	spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock, flags);
	if (info->is_cmd) {
		dev_kfree_skb_any(info->skb);
		kfree(info);
		return;
	}
	skb = info->skb;
	kfree(info);
	event_data = (unsigned long)(skb);
	if (is_tethered)
		lcid = A2_MUX_TETHERED_0;
	else {
		struct bam_mux_hdr *hdr = (struct bam_mux_hdr *)skb->data;
		lcid = (enum a2_mux_logical_channel_id) hdr->ch_id;
	}
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	a2_mux_ctx->bam_ch[lcid].num_tx_pkts--;
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	if (a2_mux_ctx->bam_ch[lcid].notify_cb)
		a2_mux_ctx->bam_ch[lcid].notify_cb(
			a2_mux_ctx->bam_ch[lcid].user_data, A2_MUX_WRITE_DONE,
							event_data);
	else
		dev_kfree_skb_any(skb);
}

static bool a2_mux_kickoff_ul_power_down(void)

{
	bool is_connected;

	write_lock(&a2_mux_ctx->ul_wakeup_lock);
	if (a2_mux_ctx->bam_connect_in_progress) {
		a2_mux_ctx->bam_is_connected = false;
		is_connected = true;
	} else {
		is_connected = a2_mux_ctx->bam_is_connected;
		a2_mux_ctx->bam_is_connected = false;
		if (is_connected) {
			a2_mux_ctx->bam_connect_in_progress = true;
			queue_work(a2_mux_ctx->a2_mux_tx_workqueue,
				&a2_mux_ctx->kickoff_ul_power_down);
		}
	}
	write_unlock(&a2_mux_ctx->ul_wakeup_lock);
	return is_connected;
}

static bool a2_mux_kickoff_ul_wakeup(void)
{
	bool is_connected;

	write_lock(&a2_mux_ctx->ul_wakeup_lock);
	if (a2_mux_ctx->bam_connect_in_progress) {
		a2_mux_ctx->bam_is_connected = true;
		is_connected = false;
	} else {
		is_connected = a2_mux_ctx->bam_is_connected;
		a2_mux_ctx->bam_is_connected = true;
		if (!is_connected) {
			a2_mux_ctx->bam_connect_in_progress = true;
			queue_work(a2_mux_ctx->a2_mux_tx_workqueue,
				&a2_mux_ctx->kickoff_ul_wakeup);
		}
	}
	write_unlock(&a2_mux_ctx->ul_wakeup_lock);
	return is_connected;
}

static void kickoff_ul_power_down_func(struct work_struct *work)
{
	bool is_connected;

	IPADBG("%s: UL active - forcing powerdown\n", __func__);
	ul_powerdown();
	write_lock(&a2_mux_ctx->ul_wakeup_lock);
	is_connected = a2_mux_ctx->bam_is_connected;
	a2_mux_ctx->bam_is_connected = false;
	a2_mux_ctx->bam_connect_in_progress = false;
	write_unlock(&a2_mux_ctx->ul_wakeup_lock);
	if (is_connected)
		a2_mux_kickoff_ul_wakeup();
	else
		ipa_rm_notify_completion(IPA_RM_RESOURCE_RELEASED,
						IPA_RM_RESOURCE_A2_CONS);
}

static void kickoff_ul_wakeup_func(struct work_struct *work)
{
	bool is_connected;
	int ret;

	ul_wakeup();
	write_lock(&a2_mux_ctx->ul_wakeup_lock);
	is_connected = a2_mux_ctx->bam_is_connected;
	a2_mux_ctx->bam_is_connected = true;
	a2_mux_ctx->bam_connect_in_progress = false;
	write_unlock(&a2_mux_ctx->ul_wakeup_lock);
	if (is_connected)
		ipa_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
				IPA_RM_RESOURCE_A2_CONS);
	INIT_COMPLETION(a2_mux_ctx->dl_wakeup_completion);
	if (!a2_mux_ctx->a2_mux_dl_wakeup) {
		ret = wait_for_completion_timeout(
			&a2_mux_ctx->dl_wakeup_completion,
			A2_MUX_COMPLETION_TIMEOUT);
		if (unlikely(ret == 0)) {
			IPAERR("%s timeout waiting for A2 PROD granted\n",
				__func__);
			BUG();
			return;
		}
	}
	if (!is_connected)
		a2_mux_kickoff_ul_power_down();
}

static void kickoff_ul_request_resource_func(struct work_struct *work)
{
	int ret;

	INIT_COMPLETION(a2_mux_ctx->request_resource_completion);
	ret = ipa_rm_request_resource(IPA_RM_RESOURCE_A2_PROD);
	if (ret < 0 && ret != -EINPROGRESS) {
		IPAERR("%s: ipa_rm_request_resource failed %d\n", __func__,
		       ret);
		return;
	}
	if (ret == -EINPROGRESS) {
		ret = wait_for_completion_timeout(
			&a2_mux_ctx->request_resource_completion,
			A2_MUX_COMPLETION_TIMEOUT);
		if (unlikely(ret == 0)) {
			IPAERR("%s timeout waiting for A2 PROD granted\n",
				__func__);
			BUG();
			return;
		}
	}
	toggle_apps_ack();
	a2_mux_ctx->a2_mux_dl_wakeup = true;
	complete_all(&a2_mux_ctx->dl_wakeup_completion);
}

static void ipa_embedded_notify(void *priv,
				enum ipa_dp_evt_type evt,
				unsigned long data)
{
	switch (evt) {
	case IPA_RECEIVE:
		handle_a2_mux_cmd((struct sk_buff *)data);
		break;
	case IPA_WRITE_DONE:
		a2_mux_write_done(false, (struct sk_buff *)data);
		break;
	default:
		IPAERR("%s: Unknown event %d\n", __func__, evt);
		break;
	}
}

static void ipa_tethered_notify(void *priv,
				enum ipa_dp_evt_type evt,
				unsigned long data)
{
	IPADBG("%s: event = %d\n", __func__, evt);
	switch (evt) {
	case IPA_RECEIVE:
		if (a2_mux_ctx->bam_ch[A2_MUX_TETHERED_0].notify_cb)
			a2_mux_ctx->bam_ch[A2_MUX_TETHERED_0].notify_cb(
				a2_mux_ctx->bam_ch[A2_MUX_TETHERED_0].user_data,
				A2_MUX_RECEIVE,
				data);
		break;
	case IPA_WRITE_DONE:
		a2_mux_write_done(true, (struct sk_buff *)data);
		break;
	default:
		IPAERR("%s: Unknown event %d\n", __func__, evt);
		break;
	}
}

static int connect_to_bam(void)
{
	int ret;
	struct ipa_sys_connect_params connect_params;

	IPAERR("%s:\n", __func__);
	if (a2_mux_ctx->a2_mux_sw_bridge_is_connected) {
		IPAERR("%s: SW bridge is already UP\n",
				__func__);
		return -EFAULT;
	}
	if (sps_ctrl_bam_dma_clk(true))
		WARN_ON(1);
	memset(&connect_params, 0, sizeof(struct ipa_sys_connect_params));
	connect_params.client = IPA_CLIENT_A2_TETHERED_CONS;
	connect_params.notify = ipa_tethered_notify;
	connect_params.desc_fifo_sz = 0x800;
	ret = ipa_bridge_setup(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_TETHERED,
			&connect_params,
			&a2_mux_ctx->tethered_prod);
	if (ret) {
		IPAERR("%s: IPA bridge tethered UL failed to connect: %d\n",
				__func__, ret);
		goto bridge_tethered_ul_failed;
	}
	memset(&connect_params, 0, sizeof(struct ipa_sys_connect_params));
	connect_params.ipa_ep_cfg.mode.mode = IPA_DMA;
	connect_params.ipa_ep_cfg.mode.dst = IPA_CLIENT_USB_CONS;
	connect_params.client = IPA_CLIENT_A2_TETHERED_PROD;
	connect_params.notify = ipa_tethered_notify;
	connect_params.desc_fifo_sz = 0x800;
	ret = ipa_bridge_setup(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_TETHERED,
			&connect_params,
			&a2_mux_ctx->tethered_cons);
	if (ret) {
		IPAERR("%s: IPA bridge tethered DL failed to connect: %d\n",
				__func__, ret);
		goto bridge_tethered_dl_failed;
	}
	memset(&connect_params, 0, sizeof(struct ipa_sys_connect_params));
	connect_params.ipa_ep_cfg.hdr.hdr_len = sizeof(struct bam_mux_hdr);
	connect_params.ipa_ep_cfg.hdr.hdr_ofst_pkt_size_valid = 1;
	connect_params.ipa_ep_cfg.hdr.hdr_ofst_pkt_size = 6;
	connect_params.client = IPA_CLIENT_A2_EMBEDDED_CONS;
	connect_params.notify = ipa_embedded_notify;
	connect_params.desc_fifo_sz = 0x800;
	ret = ipa_bridge_setup(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_EMBEDDED,
			&connect_params,
			&a2_mux_ctx->embedded_prod);
	if (ret) {
		IPAERR("%s: IPA bridge embedded UL failed to connect: %d\n",
				__func__, ret);
		goto bridge_embedded_ul_failed;
	}
	memset(&connect_params, 0, sizeof(struct ipa_sys_connect_params));
	connect_params.ipa_ep_cfg.hdr.hdr_len = sizeof(struct bam_mux_hdr);
	connect_params.ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	connect_params.ipa_ep_cfg.hdr.hdr_ofst_metadata = 4;
	connect_params.client = IPA_CLIENT_A2_EMBEDDED_PROD;
	connect_params.notify = ipa_embedded_notify;
	connect_params.desc_fifo_sz = 0x800;
	ret = ipa_bridge_setup(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_EMBEDDED,
			&connect_params,
			&a2_mux_ctx->embedded_cons);
	if (ret) {
		IPAERR("%s: IPA bridge embedded DL failed to connect: %d\n",
		       __func__, ret);
		goto bridge_embedded_dl_failed;
	}
	a2_mux_ctx->a2_mux_sw_bridge_is_connected = 1;
	complete_all(&a2_mux_ctx->bam_connection_completion);
	return 0;

bridge_embedded_dl_failed:
	ipa_bridge_teardown(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_EMBEDDED,
			a2_mux_ctx->embedded_prod);
bridge_embedded_ul_failed:
	ipa_bridge_teardown(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_TETHERED,
			a2_mux_ctx->tethered_cons);
bridge_tethered_dl_failed:
	ipa_bridge_teardown(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_TETHERED,
			a2_mux_ctx->tethered_prod);
bridge_tethered_ul_failed:
	if (sps_ctrl_bam_dma_clk(false))
		WARN_ON(1);
	return ret;
}

static int disconnect_to_bam(void)
{
	int ret;

	IPAERR("%s\n", __func__);
	if (!a2_mux_ctx->a2_mux_sw_bridge_is_connected) {
		IPAERR("%s: SW bridge is already DOWN\n",
				__func__);
		return -EFAULT;
	}
	ret = ipa_bridge_teardown(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_TETHERED,
			a2_mux_ctx->tethered_prod);
	if (ret) {
		IPAERR("%s: IPA bridge tethered UL failed to disconnect: %d\n",
				__func__, ret);
		return ret;
	}
	ret = ipa_bridge_teardown(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_TETHERED,
			a2_mux_ctx->tethered_cons);
	if (ret) {
		IPAERR("%s: IPA bridge tethered DL failed to disconnect: %d\n",
				__func__, ret);
		return ret;
	}
	ret = ipa_bridge_teardown(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_EMBEDDED,
			a2_mux_ctx->embedded_prod);
	if (ret) {
		IPAERR("%s: IPA bridge embedded UL failed to disconnect: %d\n",
				__func__, ret);
		return ret;
	}
	ret = ipa_bridge_teardown(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_EMBEDDED,
			a2_mux_ctx->embedded_cons);
	if (ret) {
		IPAERR("%s: IPA bridge embedded DL failed to disconnect: %d\n",
				__func__, ret);
		return ret;
	}
	if (sps_ctrl_bam_dma_clk(false))
		WARN_ON(1);
	verify_tx_queue_is_empty(__func__);
	(void) ipa_rm_release_resource(IPA_RM_RESOURCE_A2_PROD);
	if (a2_mux_ctx->disconnect_ack)
		toggle_apps_ack();
	a2_mux_ctx->a2_mux_dl_wakeup = false;
	a2_mux_ctx->a2_mux_sw_bridge_is_connected = 0;
	complete_all(&a2_mux_ctx->bam_connection_completion);
	return 0;
}

static void a2_mux_smsm_cb(void *priv,
		u32 old_state,
		u32 new_state)
{
	static int last_processed_state;

	mutex_lock(&a2_mux_ctx->smsm_cb_lock);
	IPADBG("%s: 0x%08x -> 0x%08x\n", __func__, old_state,
			new_state);
	if (last_processed_state == (new_state & SMSM_A2_POWER_CONTROL)) {
		IPADBG("%s: already processed this state\n", __func__);
		mutex_unlock(&a2_mux_ctx->smsm_cb_lock);
		return;
	}
	last_processed_state = new_state & SMSM_A2_POWER_CONTROL;
	if (new_state & SMSM_A2_POWER_CONTROL) {
		IPADBG("%s: MODEM PWR CTRL 1\n", __func__);
		IPA_STATS_INC_CNT(ipa_ctx->stats.a2_power_on_reqs_in);
		grab_wakelock();
		(void) connect_to_bam();
		queue_work(a2_mux_ctx->a2_mux_rx_workqueue,
			   &a2_mux_ctx->kickoff_ul_request_resource);
	} else if (!(new_state & SMSM_A2_POWER_CONTROL)) {
		IPADBG("%s: MODEM PWR CTRL 0\n", __func__);
		IPA_STATS_INC_CNT(ipa_ctx->stats.a2_power_off_reqs_in);
		(void) disconnect_to_bam();
		release_wakelock();
	} else {
		IPAERR("%s: unsupported state change\n", __func__);
	}
	mutex_unlock(&a2_mux_ctx->smsm_cb_lock);
}

static void a2_mux_smsm_ack_cb(void *priv, u32 old_state,
						u32 new_state)
{
	IPADBG("%s: 0x%08x -> 0x%08x\n", __func__, old_state,
			new_state);
	IPA_STATS_INC_CNT(ipa_ctx->stats.a2_power_modem_acks);
	complete_all(&a2_mux_ctx->ul_wakeup_ack_completion);
}

static int a2_mux_pm_rm_request_resource(void)
{
	int result = 0;
	bool is_connected;

	is_connected = a2_mux_kickoff_ul_wakeup();
	if (!is_connected)
		result = -EINPROGRESS;
	return result;
}

static int a2_mux_pm_rm_release_resource(void)
{
	int result = 0;
	bool is_connected;

	is_connected = a2_mux_kickoff_ul_power_down();
	if (is_connected)
		result = -EINPROGRESS;
	return result;
}

static void a2_mux_pm_rm_notify_cb(void *user_data,
		enum ipa_rm_event event,
		unsigned long data)
{
	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPADBG("%s: PROD GRANTED CB\n", __func__);
		complete_all(&a2_mux_ctx->request_resource_completion);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		IPADBG("%s: PROD RELEASED CB\n", __func__);
		break;
	default:
		return;
	}
}
static int a2_mux_pm_initialize_rm(void)
{
	struct ipa_rm_create_params create_params;
	int result;

	memset(&create_params, 0, sizeof(create_params));
	create_params.name = IPA_RM_RESOURCE_A2_PROD;
	create_params.reg_params.notify_cb = &a2_mux_pm_rm_notify_cb;
	result = ipa_rm_create_resource(&create_params);
	if (result)
		goto bail;
	memset(&create_params, 0, sizeof(create_params));
	create_params.name = IPA_RM_RESOURCE_A2_CONS;
	create_params.release_resource = &a2_mux_pm_rm_release_resource;
	create_params.request_resource = &a2_mux_pm_rm_request_resource;
	result = ipa_rm_create_resource(&create_params);
bail:
	return result;
}

static void a2_mux_process_data(struct sk_buff *rx_skb)
{
	unsigned long flags;
	struct bam_mux_hdr *rx_hdr;
	unsigned long event_data;

	rx_hdr = (struct bam_mux_hdr *)rx_skb->data;
	rx_skb->data = (unsigned char *)(rx_hdr + 1);
	rx_skb->tail = rx_skb->data + rx_hdr->pkt_len;
	rx_skb->len = rx_hdr->pkt_len;
	rx_skb->truesize = rx_hdr->pkt_len + sizeof(struct sk_buff);
	event_data = (unsigned long)(rx_skb);
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[rx_hdr->ch_id].lock, flags);
	if (a2_mux_ctx->bam_ch[rx_hdr->ch_id].notify_cb)
		a2_mux_ctx->bam_ch[rx_hdr->ch_id].notify_cb(
			a2_mux_ctx->bam_ch[rx_hdr->ch_id].user_data,
			A2_MUX_RECEIVE,
			event_data);
	else
		dev_kfree_skb_any(rx_skb);
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[rx_hdr->ch_id].lock,
			       flags);
}

static void handle_a2_mux_cmd_open(struct bam_mux_hdr *rx_hdr)
{
	unsigned long flags;

	spin_lock_irqsave(&a2_mux_ctx->bam_ch[rx_hdr->ch_id].lock, flags);
	a2_mux_ctx->bam_ch[rx_hdr->ch_id].status |= BAM_CH_REMOTE_OPEN;
	a2_mux_ctx->bam_ch[rx_hdr->ch_id].num_tx_pkts = 0;
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[rx_hdr->ch_id].lock,
			       flags);
}

static void handle_a2_mux_cmd(struct sk_buff *rx_skb)
{
	unsigned long flags;
	struct bam_mux_hdr *rx_hdr;

	rx_hdr = (struct bam_mux_hdr *)rx_skb->data;
	IPADBG("%s: magic %x reserved %d cmd %d pad %d ch %d len %d\n",
			__func__,
			rx_hdr->magic_num, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
	rx_hdr->magic_num = ntohs(rx_hdr->magic_num);
	rx_hdr->pkt_len = ntohs(rx_hdr->pkt_len);
	IPADBG("%s: converted to host order magic_num=%d, pkt_len=%d\n",
	    __func__, rx_hdr->magic_num, rx_hdr->pkt_len);
	if (rx_hdr->magic_num != BAM_MUX_HDR_MAGIC_NO) {
		IPAERR("bad hdr magic %x rvd %d cmd %d pad %d ch %d len %d\n",
		       rx_hdr->magic_num, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
		dev_kfree_skb_any(rx_skb);
		return;
	}
	if (rx_hdr->ch_id >= A2_MUX_NUM_CHANNELS) {
		IPAERR("bad LCID %d rsvd %d cmd %d pad %d ch %d len %d\n",
			rx_hdr->ch_id, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
		dev_kfree_skb_any(rx_skb);
		return;
	}
	switch (rx_hdr->cmd) {
	case BAM_MUX_HDR_CMD_DATA:
		a2_mux_process_data(rx_skb);
		break;
	case BAM_MUX_HDR_CMD_OPEN:
		IPADBG("%s: opening cid %d PC enabled\n", __func__,
				rx_hdr->ch_id);
		handle_a2_mux_cmd_open(rx_hdr);
		if (!(rx_hdr->reserved & ENABLE_DISCONNECT_ACK)) {
			IPADBG("%s: deactivating disconnect ack\n",
								__func__);
			a2_mux_ctx->disconnect_ack = 0;
		}
		dev_kfree_skb_any(rx_skb);
		if (a2_mux_ctx->a2_mux_send_power_vote_on_init_once) {
			kickoff_ul_wakeup_func(NULL);
			a2_mux_ctx->a2_mux_send_power_vote_on_init_once = 0;
		}
		break;
	case BAM_MUX_HDR_CMD_OPEN_NO_A2_PC:
		IPADBG("%s: opening cid %d PC disabled\n", __func__,
				rx_hdr->ch_id);
		if (!a2_mux_ctx->a2_pc_disabled) {
			a2_mux_ctx->a2_pc_disabled = 1;
			ul_wakeup();
		}
		handle_a2_mux_cmd_open(rx_hdr);
		dev_kfree_skb_any(rx_skb);
		break;
	case BAM_MUX_HDR_CMD_CLOSE:
		/* probably should drop pending write */
		IPADBG("%s: closing cid %d\n", __func__,
				rx_hdr->ch_id);
		spin_lock_irqsave(&a2_mux_ctx->bam_ch[rx_hdr->ch_id].lock,
				  flags);
		a2_mux_ctx->bam_ch[rx_hdr->ch_id].status &=
			~BAM_CH_REMOTE_OPEN;
		spin_unlock_irqrestore(
			&a2_mux_ctx->bam_ch[rx_hdr->ch_id].lock, flags);
		dev_kfree_skb_any(rx_skb);
		break;
	default:
		IPAERR("bad hdr.magic %x rvd %d cmd %d pad %d ch %d len %d\n",
			rx_hdr->magic_num, rx_hdr->reserved,
			rx_hdr->cmd, rx_hdr->pad_len, rx_hdr->ch_id,
			rx_hdr->pkt_len);
		dev_kfree_skb_any(rx_skb);
		return;
	}
}

static int a2_mux_write_cmd(void *data, u32 len)
{
	int rc;
	struct tx_pkt_info *pkt;
	unsigned long flags;

	pkt = kmalloc(sizeof(struct tx_pkt_info), GFP_ATOMIC);
	if (pkt == NULL) {
		IPAERR("%s: mem alloc for tx_pkt_info failed\n", __func__);
		return -ENOMEM;
	}
	pkt->skb = __dev_alloc_skb(len, GFP_NOWAIT | __GFP_NOWARN);
	if (pkt->skb == NULL) {
		IPAERR("%s: unable to alloc skb\n\n", __func__);
		kfree(pkt);
		return -ENOMEM;
	}
	memcpy(skb_put(pkt->skb, len), data, len);
	kfree(data);
	pkt->len = len;
	pkt->is_cmd = 1;
	set_tx_timestamp(pkt);
	spin_lock_irqsave(&a2_mux_ctx->bam_tx_pool_spinlock, flags);
	list_add_tail(&pkt->list_node, &a2_mux_ctx->bam_tx_pool);
	rc = ipa_tx_dp(IPA_CLIENT_A2_EMBEDDED_CONS, pkt->skb, NULL);
	if (rc) {
		IPAERR("%s ipa_tx_dp failed rc=%d\n",
			__func__, rc);
		list_del(&pkt->list_node);
		spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock,
				       flags);
		dev_kfree_skb_any(pkt->skb);
		kfree(pkt);
	} else {
		spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock,
				       flags);
	}
	return rc;
}

/**
 * a2_mux_get_tethered_client_handles() - provide the tethred
 *		pipe handles for post setup configuration
 * @lcid: logical channel ID
 * @clnt_cons_handle: [out] consumer pipe handle
 * @clnt_prod_handle: [out] producer pipe handle
 *
 * Returns: 0 on success, negative on failure
 */
int a2_mux_get_tethered_client_handles(enum a2_mux_logical_channel_id lcid,
		unsigned int *clnt_cons_handle,
		unsigned int *clnt_prod_handle)
{
	if (!a2_mux_ctx->a2_mux_initialized || lcid != A2_MUX_TETHERED_0)
		return -ENODEV;
	if (!clnt_cons_handle || !clnt_prod_handle)
		return -EINVAL;
	*clnt_prod_handle = a2_mux_ctx->tethered_prod;
	*clnt_cons_handle = a2_mux_ctx->tethered_cons;
	return 0;
}

/**
 * a2_mux_write() - send the packet to A2,
 *		add MUX header acc to lcid provided
 * @id: logical channel ID
 * @skb: SKB to write
 *
 * Returns: 0 on success, negative on failure
 */
int a2_mux_write(enum a2_mux_logical_channel_id id, struct sk_buff *skb)
{
	int rc = 0;
	struct bam_mux_hdr *hdr;
	unsigned long flags;
	struct sk_buff *new_skb = NULL;
	struct tx_pkt_info *pkt;
	bool is_connected;

	if (id >= A2_MUX_NUM_CHANNELS)
		return -EINVAL;
	if (!skb)
		return -EINVAL;
	if (!a2_mux_ctx->a2_mux_initialized)
		return -ENODEV;
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[id].lock, flags);
	if (!bam_ch_is_open(id)) {
		spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[id].lock, flags);
		IPAERR("%s: port not open: %d\n",
		       __func__,
		       a2_mux_ctx->bam_ch[id].status);
		return -ENODEV;
	}
	if (a2_mux_ctx->bam_ch[id].use_wm &&
	    (a2_mux_ctx->bam_ch[id].num_tx_pkts >= HIGH_WATERMARK)) {
		spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[id].lock, flags);
		IPAERR("%s: watermark exceeded: %d\n", __func__, id);
		return -EAGAIN;
	}
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[id].lock, flags);
	read_lock(&a2_mux_ctx->ul_wakeup_lock);
	is_connected = a2_mux_ctx->bam_is_connected &&
					!a2_mux_ctx->bam_connect_in_progress;
	read_unlock(&a2_mux_ctx->ul_wakeup_lock);
	if (!is_connected)
		return -ENODEV;
	if (id != A2_MUX_TETHERED_0) {
		/*
		 * if skb do not have any tailroom for padding
		 * copy the skb into a new expanded skb
		 */
		if ((skb->len & 0x3) &&
		    (skb_tailroom(skb) < A2_MUX_PADDING_LENGTH(skb->len))) {
			new_skb = skb_copy_expand(skb, skb_headroom(skb),
					A2_MUX_PADDING_LENGTH(skb->len),
					GFP_ATOMIC);
			if (new_skb == NULL) {
				IPAERR("%s: cannot allocate skb\n", __func__);
				rc = -ENOMEM;
				goto write_fail;
			}
			dev_kfree_skb_any(skb);
			skb = new_skb;
		}
		hdr = (struct bam_mux_hdr *)skb_push(
					skb, sizeof(struct bam_mux_hdr));
		/*
		 * caller should allocate for hdr and padding
		 * hdr is fine, padding is tricky
		 */
		hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
		hdr->cmd = BAM_MUX_HDR_CMD_DATA;
		hdr->reserved = 0;
		hdr->ch_id = id;
		hdr->pkt_len = skb->len - sizeof(struct bam_mux_hdr);
		if (skb->len & 0x3)
			skb_put(skb, A2_MUX_PADDING_LENGTH(skb->len));
		hdr->pad_len = skb->len - (sizeof(struct bam_mux_hdr) +
					   hdr->pkt_len);
		IPADBG("data %p, tail %p skb len %d pkt len %d pad len %d\n",
		    skb->data, skb->tail, skb->len,
		    hdr->pkt_len, hdr->pad_len);
		hdr->magic_num = htons(hdr->magic_num);
		hdr->pkt_len = htons(hdr->pkt_len);
		IPADBG("convert to network order magic_num=%d, pkt_len=%d\n",
		    hdr->magic_num, hdr->pkt_len);
	}
	pkt = kmalloc(sizeof(struct tx_pkt_info), GFP_ATOMIC);
	if (pkt == NULL) {
		IPAERR("%s: mem alloc for tx_pkt_info failed\n", __func__);
		rc = -ENOMEM;
		goto write_fail2;
	}
	pkt->skb = skb;
	pkt->is_cmd = 0;
	set_tx_timestamp(pkt);
	spin_lock_irqsave(&a2_mux_ctx->bam_tx_pool_spinlock, flags);
	list_add_tail(&pkt->list_node, &a2_mux_ctx->bam_tx_pool);
	if (id == A2_MUX_TETHERED_0)
		rc = ipa_tx_dp(IPA_CLIENT_A2_TETHERED_CONS, skb, NULL);
	else
		rc = ipa_tx_dp(IPA_CLIENT_A2_EMBEDDED_CONS, skb, NULL);
	if (rc) {
		IPAERR("%s ipa_tx_dp failed rc=%d\n",
			__func__, rc);
		list_del(&pkt->list_node);
		spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock,
				       flags);
		goto write_fail3;
	} else {
		spin_unlock_irqrestore(&a2_mux_ctx->bam_tx_pool_spinlock,
				       flags);
		spin_lock_irqsave(&a2_mux_ctx->bam_ch[id].lock, flags);
		a2_mux_ctx->bam_ch[id].num_tx_pkts++;
		spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[id].lock, flags);
	}
	return 0;

write_fail3:
	kfree(pkt);
write_fail2:
	if (new_skb)
		dev_kfree_skb_any(new_skb);
write_fail:
	return rc;
}

/**
 * a2_mux_add_hdr() - called when MUX header should
 *		be added
 * @lcid: logical channel ID
 *
 * Returns: 0 on success, negative on failure
 */
static int a2_mux_add_hdr(enum a2_mux_logical_channel_id lcid)
{
	struct ipa_ioc_add_hdr *hdrs;
	struct ipa_hdr_add *ipv4_hdr;
	struct ipa_hdr_add *ipv6_hdr;
	struct bam_mux_hdr *dmux_hdr;
	int rc;

	IPADBG("%s: ch %d\n", __func__, lcid);

	if (lcid < A2_MUX_WWAN_0 || lcid > A2_MUX_WWAN_7) {
		IPAERR("%s: non valid lcid passed: %d\n", __func__, lcid);
		return -EINVAL;
	}


	hdrs = kzalloc(sizeof(struct ipa_ioc_add_hdr) +
		       2 * sizeof(struct ipa_hdr_add), GFP_KERNEL);
	if (!hdrs) {
		IPAERR("%s: hdr allocation fail for ch %d\n", __func__, lcid);
		return -ENOMEM;
	}

	ipv4_hdr = &hdrs->hdr[0];
	ipv6_hdr = &hdrs->hdr[1];

	dmux_hdr = (struct bam_mux_hdr *)ipv4_hdr->hdr;
	snprintf(ipv4_hdr->name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF, lcid);
	dmux_hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
	dmux_hdr->cmd = BAM_MUX_HDR_CMD_DATA;
	dmux_hdr->reserved = 0;
	dmux_hdr->ch_id = lcid;

	/* Packet lenght is added by IPA */
	dmux_hdr->pkt_len = 0;
	dmux_hdr->pad_len = 0;

	dmux_hdr->magic_num = htons(dmux_hdr->magic_num);
	IPADBG("converted to network order magic_num=%d\n",
		    dmux_hdr->magic_num);

	ipv4_hdr->hdr_len = sizeof(struct bam_mux_hdr);
	ipv4_hdr->is_partial = 0;

	dmux_hdr = (struct bam_mux_hdr *)ipv6_hdr->hdr;
	snprintf(ipv6_hdr->name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V6_PREF, lcid);
	dmux_hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
	dmux_hdr->cmd = BAM_MUX_HDR_CMD_DATA;
	dmux_hdr->reserved = 0;
	dmux_hdr->ch_id = lcid;

	/* Packet lenght is added by IPA */
	dmux_hdr->pkt_len = 0;
	dmux_hdr->pad_len = 0;

	dmux_hdr->magic_num = htons(dmux_hdr->magic_num);
	IPADBG("converted to network order magic_num=%d\n",
		    dmux_hdr->magic_num);

	ipv6_hdr->hdr_len = sizeof(struct bam_mux_hdr);
	ipv6_hdr->is_partial = 0;

	hdrs->commit = 1;
	hdrs->num_hdrs = 2;

	rc = ipa_add_hdr(hdrs);
	if (rc) {
		IPAERR("Fail on Header-Insertion(%d)\n", rc);
		goto bail;
	}

	if (ipv4_hdr->status) {
		IPAERR("Fail on Header-Insertion ipv4(%d)\n",
				ipv4_hdr->status);
		rc = ipv4_hdr->status;
		goto bail;
	}

	if (ipv6_hdr->status) {
		IPAERR("%s: Fail on Header-Insertion ipv4(%d)\n", __func__,
				ipv6_hdr->status);
		rc = ipv6_hdr->status;
		goto bail;
	}

	a2_mux_ctx->bam_ch[lcid].v4_hdr_hdl = ipv4_hdr->hdr_hdl;
	a2_mux_ctx->bam_ch[lcid].v6_hdr_hdl = ipv6_hdr->hdr_hdl;

	rc = 0;
bail:
	kfree(hdrs);
	return rc;
}

/**
 * a2_mux_del_hdr() - called when MUX header should
 *		be removed
 * @lcid: logical channel ID
 *
 * Returns: 0 on success, negative on failure
 */
static int a2_mux_del_hdr(enum a2_mux_logical_channel_id lcid)
{
	struct ipa_ioc_del_hdr *hdrs;
	struct ipa_hdr_del *ipv4_hdl;
	struct ipa_hdr_del *ipv6_hdl;
	int rc;

	IPADBG("%s: ch %d\n", __func__, lcid);

	if (lcid < A2_MUX_WWAN_0 || lcid > A2_MUX_WWAN_7) {
		IPAERR("invalid lcid passed: %d\n", lcid);
		return -EINVAL;
	}


	hdrs = kzalloc(sizeof(struct ipa_ioc_del_hdr) +
		       2 * sizeof(struct ipa_hdr_del), GFP_KERNEL);
	if (!hdrs) {
		IPAERR("hdr alloc fail for ch %d\n", lcid);
		return -ENOMEM;
	}

	ipv4_hdl = &hdrs->hdl[0];
	ipv6_hdl = &hdrs->hdl[1];

	ipv4_hdl->hdl = a2_mux_ctx->bam_ch[lcid].v4_hdr_hdl;
	ipv6_hdl->hdl = a2_mux_ctx->bam_ch[lcid].v6_hdr_hdl;

	hdrs->commit = 1;
	hdrs->num_hdls = 2;

	rc = ipa_del_hdr(hdrs);
	if (rc) {
		IPAERR("Fail on Del Header-Insertion(%d)\n", rc);
		goto bail;
	}

	if (ipv4_hdl->status) {
		IPAERR("Fail on Del Header-Insertion ipv4(%d)\n",
				ipv4_hdl->status);
		rc = ipv4_hdl->status;
		goto bail;
	}
	a2_mux_ctx->bam_ch[lcid].v4_hdr_hdl = 0;

	if (ipv6_hdl->status) {
		IPAERR("Fail on Del Header-Insertion ipv4(%d)\n",
				ipv6_hdl->status);
		rc = ipv6_hdl->status;
		goto bail;
	}
	a2_mux_ctx->bam_ch[lcid].v6_hdr_hdl = 0;

	rc = 0;
bail:
	kfree(hdrs);
	return rc;

}

/**
 * a2_mux_open_channel() - opens logical channel
 *		to A2
 * @lcid: logical channel ID
 * @user_data: user provided data for below CB
 * @notify_cb: user provided notification CB
 *
 * Returns: 0 on success, negative on failure
 */
int a2_mux_open_channel(enum a2_mux_logical_channel_id lcid,
			void *user_data,
			a2_mux_notify_cb notify_cb)
{
	struct bam_mux_hdr *hdr;
	unsigned long flags;
	int rc = 0;
	bool is_connected;

	IPADBG("%s: opening ch %d\n", __func__, lcid);
	if (!a2_mux_ctx->a2_mux_initialized) {
		IPAERR("%s: not inititialized\n", __func__);
		return -ENODEV;
	}
	if (lcid >= A2_MUX_NUM_CHANNELS || lcid < 0) {
		IPAERR("%s: invalid channel id %d\n", __func__, lcid);
		return -EINVAL;
	}
	if (notify_cb == NULL) {
		IPAERR("%s: notify function is NULL\n", __func__);
		return -EINVAL;
	}
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	if (bam_ch_is_open(lcid)) {
		IPAERR("%s: Already opened %d\n", __func__, lcid);
		spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
		goto open_done;
	}
	if (!bam_ch_is_remote_open(lcid)) {
		IPAERR("%s: Remote not open; ch: %d\n", __func__, lcid);
		spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
		return -ENODEV;
	}
	a2_mux_ctx->bam_ch[lcid].notify_cb = notify_cb;
	a2_mux_ctx->bam_ch[lcid].user_data = user_data;
	a2_mux_ctx->bam_ch[lcid].status |= BAM_CH_LOCAL_OPEN;
	a2_mux_ctx->bam_ch[lcid].num_tx_pkts = 0;
	a2_mux_ctx->bam_ch[lcid].use_wm = 0;
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	read_lock(&a2_mux_ctx->ul_wakeup_lock);
	is_connected = a2_mux_ctx->bam_is_connected &&
					!a2_mux_ctx->bam_connect_in_progress;
	read_unlock(&a2_mux_ctx->ul_wakeup_lock);
	if (!is_connected)
		return -ENODEV;
	if (lcid != A2_MUX_TETHERED_0) {
		hdr = kmalloc(sizeof(struct bam_mux_hdr), GFP_KERNEL);
		if (hdr == NULL) {
			IPAERR("%s: hdr kmalloc failed. ch: %d\n",
			       __func__, lcid);
			return -ENOMEM;
		}
		hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
		if (a2_mux_ctx->a2_mux_apps_pc_enabled) {
			hdr->cmd = BAM_MUX_HDR_CMD_OPEN;
		} else {
			IPAERR("%s: PC DISABLED BY A5 SW BY INTENTION\n",
					__func__);
			a2_mux_ctx->a2_pc_disabled = 1;
			hdr->cmd = BAM_MUX_HDR_CMD_OPEN_NO_A2_PC;
		}
		hdr->reserved = 0;
		hdr->ch_id = lcid;
		hdr->pkt_len = 0;
		hdr->pad_len = 0;
		hdr->magic_num = htons(hdr->magic_num);
		hdr->pkt_len = htons(hdr->pkt_len);
		IPADBG("convert to network order magic_num=%d, pkt_len=%d\n",
		    hdr->magic_num, hdr->pkt_len);
		rc = a2_mux_write_cmd((void *)hdr,
				       sizeof(struct bam_mux_hdr));
		if (rc) {
			IPAERR("%s: bam_mux_write_cmd failed %d; ch: %d\n",
			       __func__, rc, lcid);
			kfree(hdr);
			return rc;
		}
		rc = a2_mux_add_hdr(lcid);
		if (rc) {
			IPAERR("a2_mux_add_hdr failed %d; ch: %d\n",
			       rc, lcid);
			return rc;
		}
	}

open_done:
	IPADBG("%s: opened ch %d\n", __func__, lcid);
	return rc;
}

/**
 * a2_mux_close_channel() - closes logical channel
 *		to A2
 * @lcid: logical channel ID
 *
 * Returns: 0 on success, negative on failure
 */
int a2_mux_close_channel(enum a2_mux_logical_channel_id lcid)
{
	struct bam_mux_hdr *hdr;
	unsigned long flags;
	int rc = 0;
	bool is_connected;

	if (lcid >= A2_MUX_NUM_CHANNELS || lcid < 0)
		return -EINVAL;
	IPADBG("%s: closing ch %d\n", __func__, lcid);
	if (!a2_mux_ctx->a2_mux_initialized)
		return -ENODEV;
	read_lock(&a2_mux_ctx->ul_wakeup_lock);
	is_connected = a2_mux_ctx->bam_is_connected &&
					!a2_mux_ctx->bam_connect_in_progress;
	read_unlock(&a2_mux_ctx->ul_wakeup_lock);
	if (!is_connected && !bam_ch_is_in_reset(lcid))
		return -ENODEV;
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	a2_mux_ctx->bam_ch[lcid].notify_cb = NULL;
	a2_mux_ctx->bam_ch[lcid].user_data = NULL;
	a2_mux_ctx->bam_ch[lcid].status &= ~BAM_CH_LOCAL_OPEN;
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	if (bam_ch_is_in_reset(lcid)) {
		a2_mux_ctx->bam_ch[lcid].status &= ~BAM_CH_IN_RESET;
		return 0;
	}
	if (lcid != A2_MUX_TETHERED_0) {
		hdr = kmalloc(sizeof(struct bam_mux_hdr), GFP_ATOMIC);
		if (hdr == NULL) {
			IPAERR("%s: hdr kmalloc failed. ch: %d\n",
			       __func__, lcid);
			return -ENOMEM;
		}
		hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
		hdr->cmd = BAM_MUX_HDR_CMD_CLOSE;
		hdr->reserved = 0;
		hdr->ch_id = lcid;
		hdr->pkt_len = 0;
		hdr->pad_len = 0;
		hdr->magic_num = htons(hdr->magic_num);
		hdr->pkt_len = htons(hdr->pkt_len);
		IPADBG("convert to network order magic_num=%d, pkt_len=%d\n",
		    hdr->magic_num, hdr->pkt_len);
		rc = a2_mux_write_cmd((void *)hdr, sizeof(struct bam_mux_hdr));
		if (rc) {
			IPAERR("%s: bam_mux_write_cmd failed %d; ch: %d\n",
			       __func__, rc, lcid);
			kfree(hdr);
			return rc;
		}

		rc = a2_mux_del_hdr(lcid);
		if (rc) {
			IPAERR("a2_mux_del_hdr failed %d; ch: %d\n",
			       rc, lcid);
			return rc;
		}
	}
	IPADBG("%s: closed ch %d\n", __func__, lcid);
	return 0;
}

/**
 * a2_mux_is_ch_full() - checks if channel is above predefined WM,
 *		used for flow control implementation
 * @lcid: logical channel ID
 *
 * Returns: true if the channel is above predefined WM,
 *		false otherwise
 */
int a2_mux_is_ch_full(enum a2_mux_logical_channel_id lcid)
{
	unsigned long flags;
	int ret;

	if (lcid >= A2_MUX_NUM_CHANNELS ||
			lcid < 0)
		return -EINVAL;
	if (!a2_mux_ctx->a2_mux_initialized)
		return -ENODEV;
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	a2_mux_ctx->bam_ch[lcid].use_wm = 1;
	ret = a2_mux_ctx->bam_ch[lcid].num_tx_pkts >= HIGH_WATERMARK;
	IPADBG("%s: ch %d num tx pkts=%d, HWM=%d\n", __func__,
	     lcid, a2_mux_ctx->bam_ch[lcid].num_tx_pkts, ret);
	if (!bam_ch_is_local_open(lcid)) {
		ret = -ENODEV;
		IPAERR("%s: port not open: %d\n", __func__,
		       a2_mux_ctx->bam_ch[lcid].status);
	}
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	return ret;
}

/**
 * a2_mux_is_ch_low() - checks if channel is below predefined WM,
 *		used for flow control implementation
 * @lcid: logical channel ID
 *
 * Returns: true if the channel is below predefined WM,
 *		false otherwise
 */
int a2_mux_is_ch_low(enum a2_mux_logical_channel_id lcid)
{
	unsigned long flags;
	int ret;

	if (lcid >= A2_MUX_NUM_CHANNELS ||
			lcid < 0)
		return -EINVAL;
	if (!a2_mux_ctx->a2_mux_initialized)
		return -ENODEV;
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	a2_mux_ctx->bam_ch[lcid].use_wm = 1;
	ret = a2_mux_ctx->bam_ch[lcid].num_tx_pkts <= LOW_WATERMARK;
	IPADBG("%s: ch %d num tx pkts=%d, LWM=%d\n", __func__,
	     lcid, a2_mux_ctx->bam_ch[lcid].num_tx_pkts, ret);
	if (!bam_ch_is_local_open(lcid)) {
		ret = -ENODEV;
		IPAERR("%s: port not open: %d\n", __func__,
		       a2_mux_ctx->bam_ch[lcid].status);
	}
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	return ret;
}

/**
 * a2_mux_is_ch_empty() - checks if channel is empty.
 * @lcid: logical channel ID
 *
 * Returns: true if the channel is empty,
 *		false otherwise
 */
int a2_mux_is_ch_empty(enum a2_mux_logical_channel_id lcid)
{
	unsigned long flags;
	int ret;

	if (lcid >= A2_MUX_NUM_CHANNELS ||
			lcid < 0)
		return -EINVAL;
	if (!a2_mux_ctx->a2_mux_initialized)
		return -ENODEV;
	spin_lock_irqsave(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	a2_mux_ctx->bam_ch[lcid].use_wm = 1;
	ret = a2_mux_ctx->bam_ch[lcid].num_tx_pkts == 0;
	if (!bam_ch_is_local_open(lcid)) {
		ret = -ENODEV;
		IPAERR("%s: port not open: %d\n", __func__,
		       a2_mux_ctx->bam_ch[lcid].status);
	}
	spin_unlock_irqrestore(&a2_mux_ctx->bam_ch[lcid].lock, flags);
	return ret;
}

static int a2_mux_initialize_context(int handle)
{
	int i;

	a2_mux_ctx->a2_mux_apps_pc_enabled = 1;
	a2_mux_ctx->a2_device_handle = handle;
	INIT_WORK(&a2_mux_ctx->kickoff_ul_wakeup, kickoff_ul_wakeup_func);
	INIT_WORK(&a2_mux_ctx->kickoff_ul_power_down,
		  kickoff_ul_power_down_func);
	INIT_WORK(&a2_mux_ctx->kickoff_ul_request_resource,
		  kickoff_ul_request_resource_func);
	INIT_LIST_HEAD(&a2_mux_ctx->bam_tx_pool);
	spin_lock_init(&a2_mux_ctx->bam_tx_pool_spinlock);
	mutex_init(&a2_mux_ctx->wakeup_lock);
	rwlock_init(&a2_mux_ctx->ul_wakeup_lock);
	spin_lock_init(&a2_mux_ctx->wakelock_reference_lock);
	a2_mux_ctx->disconnect_ack = 1;
	mutex_init(&a2_mux_ctx->smsm_cb_lock);
	for (i = 0; i < A2_MUX_NUM_CHANNELS; ++i)
		spin_lock_init(&a2_mux_ctx->bam_ch[i].lock);
	init_completion(&a2_mux_ctx->ul_wakeup_ack_completion);
	init_completion(&a2_mux_ctx->bam_connection_completion);
	init_completion(&a2_mux_ctx->request_resource_completion);
	init_completion(&a2_mux_ctx->dl_wakeup_completion);
	wake_lock_init(&a2_mux_ctx->bam_wakelock,
		       WAKE_LOCK_SUSPEND, "a2_mux_wakelock");
	a2_mux_ctx->a2_mux_initialized = 1;
	a2_mux_ctx->a2_mux_send_power_vote_on_init_once = 1;
	a2_mux_ctx->a2_mux_tx_workqueue =
		create_singlethread_workqueue("a2_mux_tx");
	if (!a2_mux_ctx->a2_mux_tx_workqueue) {
		IPAERR("%s: a2_mux_tx_workqueue alloc failed\n",
		       __func__);
		return -ENOMEM;
	}
	a2_mux_ctx->a2_mux_rx_workqueue =
		create_singlethread_workqueue("a2_mux_rx");
	if (!a2_mux_ctx->a2_mux_rx_workqueue) {
		IPAERR("%s: a2_mux_rx_workqueue alloc failed\n",
			__func__);
		return -ENOMEM;
	}
	return 0;
}

/**
 * a2_mux_init() - initialize A2 MUX component
 *
 * Returns: 0 on success, negative otherwise
 */
int a2_mux_init(void)
{
	int rc;
	u32 h;
	void *a2_virt_addr;
	u32 a2_bam_mem_base;
	u32 a2_bam_mem_size;
	u32 a2_bam_irq;
	struct sps_bam_props a2_props;


	IPADBG("%s A2 MUX\n", __func__);
	rc = ipa_get_a2_mux_bam_info(&a2_bam_mem_base,
				     &a2_bam_mem_size,
				     &a2_bam_irq);
	if (rc) {
		IPAERR("%s: ipa_get_a2_mux_bam_info failed\n", __func__);
		rc = -EFAULT;
		goto bail;
	}
	a2_virt_addr = ioremap_nocache((unsigned long)(a2_bam_mem_base),
							a2_bam_mem_size);
	if (!a2_virt_addr) {
		IPAERR("%s: ioremap failed\n", __func__);
		rc = -ENOMEM;
		goto bail;
	}
	memset(&a2_props, 0, sizeof(a2_props));
	a2_props.phys_addr		= a2_bam_mem_base;
	a2_props.virt_addr		= a2_virt_addr;
	a2_props.virt_size		= a2_bam_mem_size;
	a2_props.irq			= a2_bam_irq;
	a2_props.options		= SPS_BAM_OPT_IRQ_WAKEUP;
	a2_props.num_pipes		= A2_NUM_PIPES;
	a2_props.summing_threshold	= A2_SUMMING_THRESHOLD;
	a2_props.manage                 = SPS_BAM_MGR_DEVICE_REMOTE;
	/* need to free on tear down */
	rc = sps_register_bam_device(&a2_props, &h);
	if (rc < 0) {
		IPAERR("%s: register bam error %d\n", __func__, rc);
		goto register_bam_failed;
	}
	a2_mux_ctx = kzalloc(sizeof(*a2_mux_ctx), GFP_KERNEL);
	if (!a2_mux_ctx) {
		IPAERR("%s: a2_mux_ctx alloc failed, rc: %d\n", __func__, rc);
		rc = -ENOMEM;
		goto register_bam_failed;
	}
	rc = a2_mux_initialize_context(h);
	if (rc) {
		IPAERR("%s: a2_mux_initialize_context failed, rc: %d\n",
		       __func__, rc);
		goto ctx_alloc_failed;
	}
	rc = a2_mux_pm_initialize_rm();
	if (rc) {
		IPAERR("%s: a2_mux_pm_initialize_rm failed, rc: %d\n",
		       __func__, rc);
		goto ctx_alloc_failed;
	}
	rc = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_A2_POWER_CONTROL,
					a2_mux_smsm_cb, NULL);
	if (rc) {
		IPAERR("%s: smsm cb register failed, rc: %d\n", __func__, rc);
		rc = -ENOMEM;
		goto ctx_alloc_failed;
	}
	rc = smsm_state_cb_register(SMSM_MODEM_STATE,
				    SMSM_A2_POWER_CONTROL_ACK,
				    a2_mux_smsm_ack_cb, NULL);
	if (rc) {
		IPAERR("%s: smsm ack cb register failed, rc: %d\n",
		       __func__, rc);
		rc = -ENOMEM;
		goto smsm_ack_cb_reg_failed;
	}
	if (smsm_get_state(SMSM_MODEM_STATE) & SMSM_A2_POWER_CONTROL)
		a2_mux_smsm_cb(NULL, 0, smsm_get_state(SMSM_MODEM_STATE));

	/*
	 * Set remote channel open for tethered channel since there is
	 *  no actual remote tethered channel
	 */
	a2_mux_ctx->bam_ch[A2_MUX_TETHERED_0].status |= BAM_CH_REMOTE_OPEN;

	rc = 0;
	goto bail;

smsm_ack_cb_reg_failed:
	smsm_state_cb_deregister(SMSM_MODEM_STATE,
				SMSM_A2_POWER_CONTROL,
				a2_mux_smsm_cb, NULL);
ctx_alloc_failed:
	kfree(a2_mux_ctx);
register_bam_failed:
	iounmap(a2_virt_addr);
bail:
	return rc;
}

/**
 * a2_mux_exit() - destroy A2 MUX component
 *
 * Returns: 0 on success, negative otherwise
 */
int a2_mux_exit(void)
{
	smsm_state_cb_deregister(SMSM_MODEM_STATE,
			SMSM_A2_POWER_CONTROL_ACK,
			a2_mux_smsm_ack_cb,
			NULL);
	smsm_state_cb_deregister(SMSM_MODEM_STATE,
				SMSM_A2_POWER_CONTROL,
				a2_mux_smsm_cb,
				NULL);
	if (a2_mux_ctx->a2_mux_tx_workqueue)
		destroy_workqueue(a2_mux_ctx->a2_mux_tx_workqueue);
	if (a2_mux_ctx->a2_mux_rx_workqueue)
		destroy_workqueue(a2_mux_ctx->a2_mux_rx_workqueue);
	return 0;
}
