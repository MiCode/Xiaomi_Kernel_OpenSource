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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include "ipa_i.h"

#define list_next_entry(pos, member) \
	list_entry(pos->member.next, typeof(*pos), member)
#define POLLING_INACTIVITY_RX 40
#define POLLING_MIN_SLEEP_RX 2350
#define POLLING_MAX_SLEEP_RX 2450
#define POLLING_INACTIVITY_TX 10
#define POLLING_MIN_SLEEP_TX 100
#define POLLING_MAX_SLEEP_TX 200
#define RX_MAX_IND 40

static void replenish_rx_work_func(struct work_struct *work);
static struct delayed_work replenish_rx_work;
static void ipa_wq_handle_rx(struct work_struct *work);
static DECLARE_WORK(rx_work, ipa_wq_handle_rx);
static void ipa_wq_handle_tx(struct work_struct *work);
static DECLARE_WORK(tx_work, ipa_wq_handle_tx);

/**
 * ipa_write_done() - this function will be (eventually) called when a Tx
 * operation is complete
 * * @work:	work_struct used by the work queue
 *
 * Will be called in deferred context.
 * - invoke the callback supplied by the client who sent this command
 * - iterate over all packets and validate that
 *   the order for sent packet is the same as expected
 * - delete all the tx packet descriptors from the system
 *   pipe context (not needed anymore)
 * - return the tx buffer back to dma_pool
 */
void ipa_wq_write_done(struct work_struct *work)
{
	struct ipa_tx_pkt_wrapper *tx_pkt;
	struct ipa_tx_pkt_wrapper *tx_pkt_expected;
	unsigned long irq_flags;

	tx_pkt = container_of(work, struct ipa_tx_pkt_wrapper, work);

	if (unlikely(tx_pkt == NULL))
		WARN_ON(1);
	WARN_ON(tx_pkt->cnt != 1);

	spin_lock_irqsave(&tx_pkt->sys->spinlock, irq_flags);
	tx_pkt_expected = list_first_entry(&tx_pkt->sys->head_desc_list,
					   struct ipa_tx_pkt_wrapper,
					   link);
	if (unlikely(tx_pkt != tx_pkt_expected)) {
		spin_unlock_irqrestore(&tx_pkt->sys->spinlock,
				irq_flags);
		WARN_ON(1);
	}
	list_del(&tx_pkt->link);
	spin_unlock_irqrestore(&tx_pkt->sys->spinlock, irq_flags);
	if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0)) {
		dma_pool_free(ipa_ctx->dma_pool,
				tx_pkt->bounce,
				tx_pkt->mem.phys_base);
	} else {
		dma_unmap_single(NULL, tx_pkt->mem.phys_base,
				tx_pkt->mem.size,
				DMA_TO_DEVICE);
	}

	if (tx_pkt->callback)
		tx_pkt->callback(tx_pkt->user1, tx_pkt->user2);

	kfree(tx_pkt);
}

int ipa_handle_tx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state)
{
	struct ipa_tx_pkt_wrapper *tx_pkt;
	struct sps_iovec iov;
	int ret;
	int cnt = 0;
	unsigned long irq_flags;

	while ((in_poll_state ? atomic_read(&sys->curr_polling_state) :
				!atomic_read(&sys->curr_polling_state))) {
		if (cnt && !process_all)
			break;
		ret = sps_get_iovec(sys->ep->ep_hdl, &iov);
		if (ret) {
			IPAERR("sps_get_iovec failed %d\n", ret);
			break;
		}

		if (iov.addr == 0)
			break;

		if (unlikely(list_empty(&sys->head_desc_list)))
			continue;

		spin_lock_irqsave(&sys->spinlock, irq_flags);
		tx_pkt = list_first_entry(&sys->head_desc_list,
					  struct ipa_tx_pkt_wrapper, link);

		sys->len--;
		list_del(&tx_pkt->link);
		spin_unlock_irqrestore(&sys->spinlock, irq_flags);

		IPADBG("--curr_cnt=%d\n", sys->len);

		if (tx_pkt->callback) {
			dma_unmap_single(NULL, tx_pkt->mem.phys_base,
					tx_pkt->mem.size, DMA_TO_DEVICE);
			tx_pkt->callback(tx_pkt->user1, tx_pkt->user2);
		}

		kfree(tx_pkt);
		cnt++;
	};

	return cnt;
}

/**
 * ipa_tx_switch_to_intr_mode() - Operate the Tx data path in interrupt mode
 */
static void ipa_tx_switch_to_intr_mode(struct ipa_sys_context *sys)
{
	int ret;

	if (!atomic_read(&sys->curr_polling_state)) {
		IPAERR("already in intr mode\n");
		goto fail;
	}

	sys->event.options = SPS_O_EOT;
	ret = sps_register_event(sys->ep->ep_hdl, &sys->event);
	if (ret) {
		IPAERR("sps_register_event() failed %d\n", ret);
		goto fail;
	}
	sys->ep->connect.options =
		SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_EOT;
	ret = sps_set_config(sys->ep->ep_hdl, &sys->ep->connect);
	if (ret) {
		IPAERR("sps_set_config() failed %d\n", ret);
		goto fail;
	}
	atomic_set(&sys->curr_polling_state, 0);
	ipa_handle_tx_core(sys, true, false);
	return;

fail:
	IPA_STATS_INC_CNT(ipa_ctx->stats.x_intr_repost_tx);
	schedule_delayed_work(&sys->switch_to_intr_work, msecs_to_jiffies(1));
	return;
}

static void ipa_handle_tx(struct ipa_sys_context *sys)
{
	int inactive_cycles = 0;
	int cnt;

	ipa_inc_client_enable_clks();
	do {
		cnt = ipa_handle_tx_core(sys, true, true);
		if (cnt == 0) {
			inactive_cycles++;
			usleep_range(POLLING_MIN_SLEEP_TX,
					POLLING_MAX_SLEEP_TX);
		} else {
			inactive_cycles = 0;
		}
	} while (inactive_cycles <= POLLING_INACTIVITY_TX);

	ipa_tx_switch_to_intr_mode(sys);
	ipa_dec_client_disable_clks();
}

static void ipa_wq_handle_tx(struct work_struct *work)
{
	ipa_handle_tx(&ipa_ctx->sys[IPA_A5_LAN_WAN_OUT]);
}

/**
 * ipa_send_one() - Send a single descriptor
 * @sys:	system pipe context
 * @desc:	descriptor to send
 * @in_atomic:  whether caller is in atomic context
 *
 * - Allocate tx_packet wrapper
 * - Allocate a bounce buffer due to HW constrains
 *   (This buffer will be used for the DMA command)
 * - Copy the data (desc->pyld) to the bounce buffer
 * - transfer data to the IPA
 * - after the transfer was done the SPS will
 *   notify the sending user via ipa_sps_irq_comp_tx()
 *
 * Return codes: 0: success, -EFAULT: failure
 */
int ipa_send_one(struct ipa_sys_context *sys, struct ipa_desc *desc,
		bool in_atomic)
{
	struct ipa_tx_pkt_wrapper *tx_pkt;
	unsigned long irq_flags;
	int result;
	u16 sps_flags = SPS_IOVEC_FLAG_EOT;
	dma_addr_t dma_address;
	u16 len;
	u32 mem_flag = GFP_ATOMIC;

	if (unlikely(!in_atomic))
		mem_flag = GFP_KERNEL;

	tx_pkt = kmalloc(sizeof(struct ipa_tx_pkt_wrapper), mem_flag);
	if (!tx_pkt) {
		IPAERR("failed to alloc tx wrapper\n");
		goto fail_mem_alloc;
	}

	if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0)) {
		WARN_ON(desc->len > 512);

		/*
		 * Due to a HW limitation, we need to make sure that the packet
		 * does not cross a 1KB boundary
		 */
		tx_pkt->bounce = dma_pool_alloc(
					ipa_ctx->dma_pool,
					mem_flag, &dma_address);
		if (!tx_pkt->bounce) {
			dma_address = 0;
		} else {
			WARN_ON(!ipa_straddle_boundary
		       ((u32)dma_address,
				(u32)dma_address + desc->len - 1,
				1024));
			memcpy(tx_pkt->bounce, desc->pyld, desc->len);
		}
	} else {
		dma_address = dma_map_single(NULL, desc->pyld, desc->len,
				DMA_TO_DEVICE);
	}
	if (!dma_address) {
		IPAERR("failed to DMA wrap\n");
		goto fail_dma_map;
	}

	INIT_LIST_HEAD(&tx_pkt->link);
	tx_pkt->type = desc->type;
	tx_pkt->cnt = 1;    /* only 1 desc in this "set" */

	tx_pkt->mem.phys_base = dma_address;
	tx_pkt->mem.base = desc->pyld;
	tx_pkt->mem.size = desc->len;
	tx_pkt->sys = sys;
	tx_pkt->callback = desc->callback;
	tx_pkt->user1 = desc->user1;
	tx_pkt->user2 = desc->user2;

	/*
	 * Special treatment for immediate commands, where the structure of the
	 * descriptor is different
	 */
	if (desc->type == IPA_IMM_CMD_DESC) {
		sps_flags |= SPS_IOVEC_FLAG_IMME;
		len = desc->opcode;
		IPADBG("sending cmd=%d pyld_len=%d sps_flags=%x\n",
				desc->opcode, desc->len, sps_flags);
		IPA_DUMP_BUFF(desc->pyld, dma_address, desc->len);
	} else {
		len = desc->len;
	}

	INIT_WORK(&tx_pkt->work, ipa_wq_write_done);

	spin_lock_irqsave(&sys->spinlock, irq_flags);
	list_add_tail(&tx_pkt->link, &sys->head_desc_list);
	result = sps_transfer_one(sys->ep->ep_hdl, dma_address, len, tx_pkt,
			sps_flags);
	if (result) {
		IPAERR("sps_transfer_one failed rc=%d\n", result);
		goto fail_sps_send;
	}

	spin_unlock_irqrestore(&sys->spinlock, irq_flags);

	return 0;

fail_sps_send:
	list_del(&tx_pkt->link);
	spin_unlock_irqrestore(&sys->spinlock, irq_flags);
	if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0))
		dma_pool_free(ipa_ctx->dma_pool, tx_pkt->bounce,
				dma_address);
	else
		dma_unmap_single(NULL, dma_address, desc->len, DMA_TO_DEVICE);
fail_dma_map:
	kfree(tx_pkt);
fail_mem_alloc:
	return -EFAULT;
}

/**
 * ipa_sps_irq_cmd_ack - callback function which will be called by SPS driver after an
 * immediate command is complete.
 * @user1:	pointer to the descriptor of the transfer
 * @user2:
 *
 * Complete the immediate commands completion object, this will release the
 * thread which waits on this completion object (ipa_send_cmd())
 */
static void ipa_sps_irq_cmd_ack(void *user1, void *user2)
{
	struct ipa_desc *desc = (struct ipa_desc *)user1;

	if (!desc)
		WARN_ON(1);
	IPADBG("got ack for cmd=%d\n", desc->opcode);
	complete(&desc->xfer_done);
}

/**
 * ipa_send_cmd - send immediate commands
 * @num_desc:	number of descriptors within the desc struct
 * @descr:	descriptor structure
 *
 * Function will block till command gets ACK from IPA HW, caller needs
 * to free any resources it allocated after function returns
 * The callback in ipa_desc should not be set by the caller
 * for this function.
 */
int ipa_send_cmd(u16 num_desc, struct ipa_desc *descr)
{
	int result = 0;

	ipa_inc_client_enable_clks();

	if (num_desc == 1) {
		init_completion(&descr->xfer_done);

		if (descr->callback || descr->user1)
			WARN_ON(1);

		descr->callback = ipa_sps_irq_cmd_ack;
		descr->user1 = descr;
		if (ipa_send_one(&ipa_ctx->sys[IPA_A5_CMD], descr, false)) {
			IPAERR("fail to send immediate command\n");
			result = -EFAULT;
			goto bail;
		}
		wait_for_completion(&descr->xfer_done);
	} else {
		IPAERR("unsupported chaining multiple IC\n");
			result = -EFAULT;
			goto bail;
		}

	IPA_STATS_INC_IC_CNT(num_desc, descr, ipa_ctx->stats.imm_cmds);
bail:
	ipa_dec_client_disable_clks();
	return result;
}

/**
 * ipa_sps_irq_tx_notify() - Callback function which will be called by
 * the SPS driver to start a Tx poll operation.
 * Called in an interrupt context.
 * @notify:	SPS driver supplied notification struct
 *
 * This function defer the work for this event to the tx workqueue.
 */
static void ipa_sps_irq_tx_notify(struct sps_event_notify *notify)
{
	struct ipa_sys_context *sys = &ipa_ctx->sys[IPA_A5_LAN_WAN_OUT];
	int ret;

	IPADBG("event %d notified\n", notify->event_id);

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		if (!atomic_read(&sys->curr_polling_state)) {
			sys->ep->connect.options = SPS_O_AUTO_ENABLE |
				SPS_O_ACK_TRANSFERS | SPS_O_POLL;
			ret = sps_set_config(sys->ep->ep_hdl,
					&sys->ep->connect);
			if (ret) {
				IPAERR("sps_set_config() failed %d\n", ret);
				break;
			}
			atomic_set(&sys->curr_polling_state, 1);
			queue_work(ipa_ctx->tx_wq, &tx_work);
		}
		break;
	default:
		IPAERR("recieved unexpected event id %d\n", notify->event_id);
	}
}

/**
 * ipa_sps_irq_tx_no_aggr_notify() - Callback function which will be called by
 * the SPS driver after a Tx operation is complete.
 * Called in an interrupt context.
 * @notify:	SPS driver supplied notification struct
 *
 * This function defer the work for this event to the tx workqueue.
 * This event will be later handled by ipa_write_done.
 */
static void ipa_sps_irq_tx_no_aggr_notify(struct sps_event_notify *notify)
{
	struct ipa_tx_pkt_wrapper *tx_pkt;

	IPADBG("event %d notified\n", notify->event_id);

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		tx_pkt = notify->data.transfer.user;
		queue_work(ipa_ctx->tx_wq, &tx_pkt->work);
		break;
	default:
		IPAERR("recieved unexpected event id %d\n", notify->event_id);
	}
}

static void ipa_handle_tag_rsp(struct ipa_a5_mux_hdr *mux_hdr)
{
	struct completion *compl;
	struct ipa_tree_node *node;

	/* retrieve the compl object from tag value */
	mux_hdr++;
	compl = (struct completion *) ntohl(*((u32 *)mux_hdr));
	IPADBG("%x %x %p\n", *(u32 *)mux_hdr, *((u32 *)mux_hdr + 1), compl);

	mutex_lock(&ipa_ctx->lock);
	node = ipa_search(&ipa_ctx->tag_tree, (u32)compl);
	if (node) {
		complete_all(compl);
		rb_erase(&node->node, &ipa_ctx->tag_tree);
		kmem_cache_free(ipa_ctx->tree_node_cache, node);
	} else {
		WARN_ON(1);
	}
	mutex_unlock(&ipa_ctx->lock);
}

static void ipa_dejitter(bool limit)
{
	struct sk_buff *skb;
	int len = skb_queue_len(&ipa_ctx->rx_list);
	int i;
	void *cookie;
	ipa_notify_cb cb;

	if (limit && len >= RX_MAX_IND)
		len = RX_MAX_IND;

	for (i = len; i > 0; i--) {
		skb = __skb_dequeue(&ipa_ctx->rx_list);
		cb = (ipa_notify_cb)*(u32 *)&(skb->cb[0]);
		cookie = (void *)*(u32 *)&(skb->cb[4]);
		cb(cookie, IPA_RECEIVE, (unsigned long)skb);
	}
}

/**
 * ipa_handle_rx_core() - The core functionality of packet reception. This
 * function is read from multiple code paths.
 *
 * All the packets on the Rx data path are received on the IPA_A5_LAN_WAN_IN
 * endpoint. The function runs as long as there are packets in the pipe.
 * For each packet:
 *  - Disconnect the packet from the system pipe linked list
 *  - Unmap the packets skb, make it non DMAable
 *  - Free the packet from the cache
 *  - Prepare a proper skb
 *  - Call the endpoints notify function, passing the skb in the parameters
 *  - Replenish the rx cache
 */
int ipa_handle_rx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state)
{
	struct ipa_a5_mux_hdr *mux_hdr;
	struct ipa_rx_pkt_wrapper *rx_pkt;
	struct sk_buff *rx_skb;
	struct sps_iovec iov;
	int ret;
	struct ipa_ep_context *ep;
	int cnt = 0;
	unsigned int src_pipe;

	while ((in_poll_state ? atomic_read(&sys->curr_polling_state) :
				!atomic_read(&sys->curr_polling_state))) {
		if (cnt && !process_all)
			break;

		ret = sps_get_iovec(sys->ep->ep_hdl, &iov);
		if (ret) {
			IPAERR("sps_get_iovec failed %d\n", ret);
			break;
		}

		if (iov.addr == 0)
			break;

		if (unlikely(list_empty(&sys->head_desc_list)))
			continue;

		rx_pkt = list_first_entry(&sys->head_desc_list,
					  struct ipa_rx_pkt_wrapper, link);

		rx_pkt->len = iov.size;
		sys->len--;
		list_del(&rx_pkt->link);

		IPADBG("--curr_cnt=%d\n", sys->len);

		rx_skb = rx_pkt->skb;
		dma_unmap_single(NULL, rx_pkt->dma_address, IPA_RX_SKB_SIZE,
				 DMA_FROM_DEVICE);

		/*
		 * make it look like a real skb, "data" was already set at
		 * alloc time
		 */
		rx_skb->tail = rx_skb->data + rx_pkt->len;
		rx_skb->len = rx_pkt->len;
		rx_skb->truesize = rx_pkt->len + sizeof(struct sk_buff);
		kfree(rx_pkt);

		mux_hdr = (struct ipa_a5_mux_hdr *)rx_skb->data;

		src_pipe = mux_hdr->src_pipe_index;

		IPADBG("RX pkt len=%d IID=0x%x src=%d, flags=0x%x, meta=0x%x\n",
			rx_skb->len, ntohs(mux_hdr->interface_id),
			src_pipe, mux_hdr->flags, ntohl(mux_hdr->metadata));

		IPA_DUMP_BUFF(rx_skb->data, 0, rx_skb->len);

		IPA_STATS_INC_CNT(ipa_ctx->stats.rx_pkts);
		IPA_STATS_EXCP_CNT(mux_hdr->flags, ipa_ctx->stats.rx_excp_pkts);

		if (mux_hdr->flags & IPA_A5_MUX_HDR_EXCP_FLAG_TAG) {
			if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_VIRTUAL)
				ipa_handle_tag_rsp(mux_hdr);
			dev_kfree_skb(rx_skb);
			ipa_replenish_rx_cache();
			++cnt;
			continue;
		}

		/*
		 * Any packets arriving over AMPDU_TX should be dispatched
		 * to the regular WLAN RX data-path.
		 */
		if (src_pipe == WLAN_AMPDU_TX_EP)
			src_pipe = WLAN_PROD_TX_EP;

		WARN_ON(src_pipe >= IPA_NUM_PIPES);

		ep = &ipa_ctx->ep[src_pipe];
		IPADBG("pulling %d bytes from skb\n", ep->pull_len);
		skb_pull(rx_skb, ep->pull_len);
		ipa_replenish_rx_cache();
		if (ep->client_notify) {
			__skb_queue_tail(&ipa_ctx->rx_list, rx_skb);
			*(u32 *)&(rx_skb->cb[0]) = (u32)ep->client_notify;
			*(u32 *)&(rx_skb->cb[4]) = (u32)ep->priv;
		} else {
			dev_kfree_skb(rx_skb);
		}
		cnt++;
	};

	return cnt;
}

/**
 * ipa_rx_switch_to_intr_mode() - Operate the Rx data path in interrupt mode
 */
static void ipa_rx_switch_to_intr_mode(struct ipa_sys_context *sys)
{
	int ret;

	if (!atomic_read(&sys->curr_polling_state)) {
		IPAERR("already in intr mode\n");
		goto fail;
	}

	sys->event.options = SPS_O_EOT;
	ret = sps_register_event(sys->ep->ep_hdl, &sys->event);
	if (ret) {
		IPAERR("sps_register_event() failed %d\n", ret);
		goto fail;
	}
	sys->ep->connect.options =
		SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_EOT;
	ret = sps_set_config(sys->ep->ep_hdl, &sys->ep->connect);
	if (ret) {
		IPAERR("sps_set_config() failed %d\n", ret);
		goto fail;
	}
	atomic_set(&sys->curr_polling_state, 0);
	ipa_handle_rx_core(sys, true, false);
	return;

fail:
	IPA_STATS_INC_CNT(ipa_ctx->stats.x_intr_repost);
	schedule_delayed_work(&sys->switch_to_intr_work, msecs_to_jiffies(1));
}


/**
 * ipa_rx_notify() - Callback function which is called by the SPS driver when a
 * a packet is received
 * @notify:	SPS driver supplied notification information
 *
 * Called in an interrupt context, therefore the majority of the work is
 * deffered using a work queue.
 *
 * After receiving a packet, the driver goes to polling mode and keeps pulling
 * packets until the rx buffer is empty, then it goes back to interrupt mode.
 * This comes to prevent the CPU from handling too many interrupts when the
 * throughput is high.
 */
static void ipa_sps_irq_rx_notify(struct sps_event_notify *notify)
{
	struct ipa_sys_context *sys = &ipa_ctx->sys[IPA_A5_LAN_WAN_IN];
	int ret;

	IPADBG("event %d notified\n", notify->event_id);

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		if (!atomic_read(&sys->curr_polling_state)) {
			sys->ep->connect.options = SPS_O_AUTO_ENABLE |
				SPS_O_ACK_TRANSFERS | SPS_O_POLL;
			ret = sps_set_config(sys->ep->ep_hdl,
					&sys->ep->connect);
			if (ret) {
				IPAERR("sps_set_config() failed %d\n", ret);
				break;
			}
			atomic_set(&sys->curr_polling_state, 1);
			queue_work(ipa_ctx->rx_wq, &rx_work);
		}
		break;
	default:
		IPAERR("recieved unexpected event id %d\n", notify->event_id);
	}
}

static void switch_to_intr_tx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa_sys_context *sys;
	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa_sys_context, switch_to_intr_work);
	ipa_handle_tx(sys);
}

/**
 * ipa_handle_rx() - handle packet reception. This function is executed in the
 * context of a work queue.
 * @work: work struct needed by the work queue
 *
 * ipa_handle_rx_core() is run in polling mode. After all packets has been
 * received, the driver switches back to interrupt mode.
 */
static void ipa_handle_rx(struct ipa_sys_context *sys)
{
	int inactive_cycles = 0;
	int cnt;

	ipa_inc_client_enable_clks();
	do {
		cnt = ipa_handle_rx_core(sys, true, true);
		if (cnt == 0) {
			inactive_cycles++;
			usleep_range(POLLING_MIN_SLEEP_RX,
					POLLING_MAX_SLEEP_RX);
		} else {
			inactive_cycles = 0;
		}
		ipa_dejitter(true);
	} while (inactive_cycles <= POLLING_INACTIVITY_RX);

	ipa_dejitter(false);
	ipa_rx_switch_to_intr_mode(sys);
	ipa_dec_client_disable_clks();
}

static void switch_to_intr_rx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa_sys_context *sys;
	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa_sys_context, switch_to_intr_work);
	ipa_handle_rx(sys);
}

/**
 * ipa_setup_sys_pipe() - Setup an IPA end-point in system-BAM mode and perform
 * IPA EP configuration
 * @sys_in:	[in] input needed to setup BAM pipe and configure EP
 * @clnt_hdl:	[out] client handle
 *
 *  - configure the end-point registers with the supplied
 *    parameters from the user.
 *  - call SPS APIs to create a system-to-bam connection with IPA.
 *  - allocate descriptor FIFO
 *  - register callback function(ipa_sps_irq_rx_notify or
 *    ipa_sps_irq_tx_notify - depends on client type) in case the driver is
 *    not configured to pulling mode
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl)
{
	int ipa_ep_idx;
	int sys_idx = -1;
	int result = -EFAULT;
	dma_addr_t dma_addr;

	if (sys_in == NULL || clnt_hdl == NULL ||
	    sys_in->client >= IPA_CLIENT_MAX || sys_in->desc_fifo_sz == 0) {
		IPAERR("bad parm.\n");
		result = -EINVAL;
		goto fail_bad_param;
	}

	ipa_ep_idx = ipa_get_ep_mapping(ipa_ctx->mode, sys_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		goto fail_bad_param;
	}

	if (ipa_ctx->ep[ipa_ep_idx].valid == 1) {
		IPAERR("EP already allocated.\n");
		goto fail_bad_param;
	}

	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));

	ipa_ctx->ep[ipa_ep_idx].valid = 1;
	ipa_ctx->ep[ipa_ep_idx].client = sys_in->client;
	ipa_ctx->ep[ipa_ep_idx].client_notify = sys_in->notify;
	ipa_ctx->ep[ipa_ep_idx].priv = sys_in->priv;

	if (ipa_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
		IPAERR("fail to configure EP.\n");
		goto fail_sps_api;
	}

	/* Default Config */
	ipa_ctx->ep[ipa_ep_idx].ep_hdl = sps_alloc_endpoint();

	if (ipa_ctx->ep[ipa_ep_idx].ep_hdl == NULL) {
		IPAERR("SPS EP allocation failed.\n");
		goto fail_sps_api;
	}

	result = sps_get_config(ipa_ctx->ep[ipa_ep_idx].ep_hdl,
			&ipa_ctx->ep[ipa_ep_idx].connect);
	if (result) {
		IPAERR("fail to get config.\n");
		goto fail_mem_alloc;
	}

	/* Specific Config */
	if (IPA_CLIENT_IS_CONS(sys_in->client)) {
		ipa_ctx->ep[ipa_ep_idx].connect.mode = SPS_MODE_SRC;
		ipa_ctx->ep[ipa_ep_idx].connect.destination =
			SPS_DEV_HANDLE_MEM;
		ipa_ctx->ep[ipa_ep_idx].connect.source = ipa_ctx->bam_handle;
		ipa_ctx->ep[ipa_ep_idx].connect.dest_pipe_index =
			ipa_ctx->a5_pipe_index++;
		ipa_ctx->ep[ipa_ep_idx].connect.src_pipe_index = ipa_ep_idx;
		ipa_ctx->ep[ipa_ep_idx].connect.options = SPS_O_ACK_TRANSFERS |
			SPS_O_NO_DISABLE;
	} else {
		ipa_ctx->ep[ipa_ep_idx].connect.mode = SPS_MODE_DEST;
		ipa_ctx->ep[ipa_ep_idx].connect.source = SPS_DEV_HANDLE_MEM;
		ipa_ctx->ep[ipa_ep_idx].connect.destination =
			ipa_ctx->bam_handle;
		ipa_ctx->ep[ipa_ep_idx].connect.src_pipe_index =
			ipa_ctx->a5_pipe_index++;
		ipa_ctx->ep[ipa_ep_idx].connect.dest_pipe_index = ipa_ep_idx;
		if (sys_in->client == IPA_CLIENT_A5_LAN_WAN_PROD)
			ipa_ctx->ep[ipa_ep_idx].connect.options |=
				SPS_O_ACK_TRANSFERS;
	}

	ipa_ctx->ep[ipa_ep_idx].connect.options |= (SPS_O_AUTO_ENABLE |
		SPS_O_EOT);
	if (ipa_ctx->polling_mode)
		ipa_ctx->ep[ipa_ep_idx].connect.options |= SPS_O_POLL;

	ipa_ctx->ep[ipa_ep_idx].connect.desc.size = sys_in->desc_fifo_sz;
	ipa_ctx->ep[ipa_ep_idx].connect.desc.base =
	   dma_alloc_coherent(NULL, ipa_ctx->ep[ipa_ep_idx].connect.desc.size,
			   &dma_addr, 0);
	ipa_ctx->ep[ipa_ep_idx].connect.desc.phys_base = dma_addr;
	if (ipa_ctx->ep[ipa_ep_idx].connect.desc.base == NULL) {
		IPAERR("fail to get DMA desc memory.\n");
		goto fail_mem_alloc;
	}

	ipa_ctx->ep[ipa_ep_idx].connect.event_thresh = IPA_EVENT_THRESHOLD;

	result = sps_connect(ipa_ctx->ep[ipa_ep_idx].ep_hdl,
			&ipa_ctx->ep[ipa_ep_idx].connect);
	if (result) {
		IPAERR("sps_connect fails.\n");
		goto fail_sps_connect;
	}

	switch (ipa_ep_idx) {
	case 1:
		sys_idx = ipa_ep_idx;
		break;
	case 2:
		sys_idx = ipa_ep_idx;
		ipa_ctx->sys[sys_idx].max_len = sys_in->desc_fifo_sz / 8 - 2;
		INIT_DELAYED_WORK(&ipa_ctx->sys[sys_idx].switch_to_intr_work,
				switch_to_intr_tx_work_func);
		break;
	case 3:
		sys_idx = ipa_ep_idx;
		INIT_DELAYED_WORK(&replenish_rx_work, replenish_rx_work_func);
		INIT_DELAYED_WORK(&ipa_ctx->sys[sys_idx].switch_to_intr_work,
				switch_to_intr_rx_work_func);
		break;
	case WLAN_AMPDU_TX_EP:
		sys_idx = IPA_A5_WLAN_AMPDU_OUT;
		break;
	default:
		IPAERR("Invalid EP index.\n");
		result = -EFAULT;
		goto fail_register_event;
	}

	if (!ipa_ctx->polling_mode) {

		ipa_ctx->sys[sys_idx].event.options = SPS_O_EOT;
		ipa_ctx->sys[sys_idx].event.mode = SPS_TRIGGER_CALLBACK;
		ipa_ctx->sys[sys_idx].event.xfer_done = NULL;
		ipa_ctx->sys[sys_idx].event.user =
			&ipa_ctx->sys[sys_idx];
		ipa_ctx->sys[sys_idx].event.callback =
				IPA_CLIENT_IS_CONS(sys_in->client) ?
					ipa_sps_irq_rx_notify :
					(sys_in->client ==
					 IPA_CLIENT_A5_LAN_WAN_PROD ?
					ipa_sps_irq_tx_notify :
					ipa_sps_irq_tx_no_aggr_notify);
		result = sps_register_event(ipa_ctx->ep[ipa_ep_idx].ep_hdl,
					  &ipa_ctx->sys[sys_idx].event);
		if (result < 0) {
			IPAERR("register event error %d\n", result);
			goto fail_register_event;
		}
	}

	*clnt_hdl = ipa_ep_idx;

	IPADBG("client %d (ep: %d) connected\n", sys_in->client, ipa_ep_idx);

	return 0;

fail_register_event:
	sps_disconnect(ipa_ctx->ep[ipa_ep_idx].ep_hdl);
fail_sps_connect:
	dma_free_coherent(NULL, ipa_ctx->ep[ipa_ep_idx].connect.desc.size,
			  ipa_ctx->ep[ipa_ep_idx].connect.desc.base,
			  ipa_ctx->ep[ipa_ep_idx].connect.desc.phys_base);
fail_mem_alloc:
	sps_free_endpoint(ipa_ctx->ep[ipa_ep_idx].ep_hdl);
fail_sps_api:
	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
fail_bad_param:
	return result;
}
EXPORT_SYMBOL(ipa_setup_sys_pipe);

/**
 * ipa_teardown_sys_pipe() - Teardown the system-BAM pipe and cleanup IPA EP
 * @clnt_hdl:	[in] the handle obtained from ipa_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_teardown_sys_pipe(u32 clnt_hdl)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	sps_disconnect(ipa_ctx->ep[clnt_hdl].ep_hdl);
	dma_free_coherent(NULL, ipa_ctx->ep[clnt_hdl].connect.desc.size,
			  ipa_ctx->ep[clnt_hdl].connect.desc.base,
			  ipa_ctx->ep[clnt_hdl].connect.desc.phys_base);
	sps_free_endpoint(ipa_ctx->ep[clnt_hdl].ep_hdl);
	memset(&ipa_ctx->ep[clnt_hdl], 0, sizeof(struct ipa_ep_context));

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}
EXPORT_SYMBOL(ipa_teardown_sys_pipe);

/**
 * ipa_tx_comp_usr_notify_release() - Callback function which will call the
 * user supplied callback function to release the skb, or release it on
 * its own if no callback function was supplied.
 * @user1
 * @user2
 *
 * This notified callback is for the destination client.
 * This function is supplied in ipa_connect.
 */
static void ipa_tx_comp_usr_notify_release(void *user1, void *user2)
{
	struct sk_buff *skb = (struct sk_buff *)user1;
	u32 ep_idx = (u32)user2;

	IPADBG("skb=%p ep=%d\n", skb, ep_idx);

	IPA_STATS_INC_TX_CNT(ep_idx, ipa_ctx->stats.tx_sw_pkts,
			ipa_ctx->stats.tx_hw_pkts);

	if (ipa_ctx->ep[ep_idx].client_notify)
		ipa_ctx->ep[ep_idx].client_notify(ipa_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)skb);
	else
		dev_kfree_skb(skb);
}

static int ipa_send_two(struct sk_buff *skb, struct ipa_sys_context *sys,
		int dst_ep_idx)
{
	struct ipa_tx_pkt_wrapper *tx_pktc;
	struct ipa_tx_pkt_wrapper *tx_pktd;
	struct ipa_ep_context *ep = &ipa_ctx->ep[dst_ep_idx];
	unsigned long irq_flags;
	dma_addr_t dma_addrd;
	int rc = -ENOMEM;

	tx_pktc = kmalloc(sizeof(struct ipa_tx_pkt_wrapper), GFP_ATOMIC);
	if (!tx_pktc) {
		IPAERR("failed to alloc tx wrapper C\n");
		goto fail_mem_alloc_c;
	}

	tx_pktd = kmalloc(sizeof(struct ipa_tx_pkt_wrapper), GFP_ATOMIC);
	if (!tx_pktd) {
		IPAERR("failed to alloc tx wrapper D\n");
		goto fail_mem_alloc_d;
	}

	dma_addrd = dma_map_single(NULL, skb->data, skb->len, DMA_TO_DEVICE);
	if (!dma_addrd) {
		IPAERR("failed to DMA wrap\n");
		goto fail_dma_map_d;
	}

	INIT_LIST_HEAD(&tx_pktc->link);
	tx_pktc->callback = NULL;

	INIT_LIST_HEAD(&tx_pktd->link);
	tx_pktd->mem.phys_base = dma_addrd;
	tx_pktd->mem.base = skb->data;
	tx_pktd->mem.size = skb->len;
	tx_pktd->callback = ipa_tx_comp_usr_notify_release;
	tx_pktd->user1 = skb;
	tx_pktd->user2 = (void *)dst_ep_idx;

	spin_lock_irqsave(&sys->spinlock, irq_flags);
	if (sys->len >= sys->max_len)
		goto fail_oom;
	list_add_tail(&tx_pktc->link, &sys->head_desc_list);
	if (sps_transfer_one(sys->ep->ep_hdl, ep->dma_addr, IPA_IP_PACKET_INIT,
			tx_pktc, SPS_IOVEC_FLAG_IMME |
			SPS_IOVEC_FLAG_NO_SUBMIT))
		IPAERR("sps_transfer_one 0 failed\n");
	list_add_tail(&tx_pktd->link, &sys->head_desc_list);
	if (sps_transfer_one(sys->ep->ep_hdl, dma_addrd, skb->len, tx_pktd,
			SPS_IOVEC_FLAG_EOT | SPS_IOVEC_FLAG_INT))
		IPAERR("sps_transfer_one 1 failed\n");
	sys->len += 2;
	spin_unlock_irqrestore(&sys->spinlock, irq_flags);
	return 0;

fail_oom:
	dma_unmap_single(NULL, dma_addrd, skb->len, DMA_TO_DEVICE);
fail_dma_map_d:
	kfree(tx_pktd);
fail_mem_alloc_d:
	kfree(tx_pktc);
fail_mem_alloc_c:
	return rc;
}

static int ipa_send_data_hw_path(struct sk_buff *skb,
		struct ipa_sys_context *sys, int dst_ep_idx)
{
	struct ipa_desc desc;

	desc.pyld = skb->data;
	desc.len = skb->len;
	desc.type = IPA_DATA_DESC_SKB;
	desc.callback = ipa_tx_comp_usr_notify_release;
	desc.user1 = skb;
	desc.user2 = (void *)dst_ep_idx;

	if (ipa_send_one(sys, &desc, true))
		return -EFAULT;

	return 0;
}

/**
 * ipa_tx_dp() - Data-path tx handler
 * @dst:	[in] which IPA destination to route tx packets to
 * @skb:	[in] the packet to send
 * @metadata:	[in] TX packet meta-data
 *
 * Data-path tx handler, this is used for both SW data-path which by-passes most
 * IPA HW blocks AND the regular HW data-path for WLAN AMPDU traffic only. If
 * dst is a "valid" CONS type, then SW data-path is used. If dst is the
 * WLAN_AMPDU PROD type, then HW data-path for WLAN AMPDU is used. Anything else
 * is an error. For errors, client needs to free the skb as needed. For success,
 * IPA driver will later invoke client callback if one was supplied. That
 * callback should free the skb. If no callback supplied, IPA driver will free
 * the skb internally
 *
 * The function will use two descriptors for this send command
 * (for A5_WLAN_AMPDU_PROD only one desciprtor will be sent),
 * the first descriptor will be used to inform the IPA hardware that
 * apps need to push data into the IPA (IP_PACKET_INIT immediate command).
 * Once this send was done from SPS point-of-view the IPA driver will
 * get notified by the supplied callback - ipa_sps_irq_tx_comp()
 *
 * ipa_sps_irq_tx_comp will call to the user supplied
 * callback (from ipa_connect)
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *meta)
{
	int ipa_ep_idx;

	ipa_ep_idx = ipa_get_ep_mapping(ipa_ctx->mode, dst);
	if (unlikely(ipa_ep_idx == -1)) {
		IPAERR("dest EP does not exist.\n");
		goto fail;
	}

	if (unlikely(ipa_ctx->ep[ipa_ep_idx].valid == 0)) {
		IPAERR("dest EP not valid.\n");
		goto fail;
	}

	if (IPA_CLIENT_IS_CONS(dst)) {
		if (ipa_send_two(skb, &ipa_ctx->sys[IPA_A5_LAN_WAN_OUT],
					ipa_ep_idx)) {
			IPAERR("fail to send pkt_init+skb dst=%d skb=%p\n",
					dst, skb);
			goto fail;
		}
		IPA_STATS_INC_CNT(ipa_ctx->stats.imm_cmds[IPA_IP_PACKET_INIT]);
	} else if (dst == IPA_CLIENT_A5_WLAN_AMPDU_PROD) {
		if (ipa_send_data_hw_path(skb,
					&ipa_ctx->sys[IPA_A5_WLAN_AMPDU_OUT],
					ipa_ep_idx)) {
			IPAERR("fail to send skb dst=%d skb=%p\n", dst, skb);
			goto fail;
		}
	} else {
		IPAERR("%d PROD is not supported.\n", dst);
		goto fail;
	}

	return 0;

fail:
	return -EFAULT;
}
EXPORT_SYMBOL(ipa_tx_dp);

static void ipa_wq_handle_rx(struct work_struct *work)
{
	ipa_handle_rx(&ipa_ctx->sys[IPA_A5_LAN_WAN_IN]);
}

/**
 * ipa_replenish_rx_cache() - Replenish the Rx packets cache.
 *
 * The function allocates buffers in the rx_pkt_wrapper_cache cache until there
 * are IPA_RX_POOL_CEIL buffers in the cache.
 *   - Allocate a buffer in the cache
 *   - Initialized the packets link
 *   - Initialize the packets work struct
 *   - Allocate the packets socket buffer (skb)
 *   - Fill the packets skb with data
 *   - Make the packet DMAable
 *   - Add the packet to the system pipe linked list
 *   - Initiate a SPS transfer so that SPS driver will use this packet later.
 */
void ipa_replenish_rx_cache(void)
{
	void *ptr;
	struct ipa_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	struct ipa_sys_context *sys = &ipa_ctx->sys[IPA_A5_LAN_WAN_IN];
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = sys->len;

	while (rx_len_cached < IPA_RX_POOL_CEIL) {
		rx_pkt = kmalloc(sizeof(struct ipa_rx_pkt_wrapper), flag);
		if (!rx_pkt) {
			IPAERR("failed to alloc rx wrapper\n");
			goto fail_kmem_cache_alloc;
		}

		INIT_LIST_HEAD(&rx_pkt->link);

		rx_pkt->skb = __dev_alloc_skb(IPA_RX_SKB_SIZE, flag);
		if (rx_pkt->skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->skb, IPA_RX_SKB_SIZE);
		rx_pkt->dma_address = dma_map_single(NULL, ptr,
						     IPA_RX_SKB_SIZE,
						     DMA_FROM_DEVICE);
		if (rx_pkt->dma_address == 0 || rx_pkt->dma_address == ~0) {
			IPAERR("dma_map_single failure %p for %p\n",
			       (void *)rx_pkt->dma_address, ptr);
			goto fail_dma_mapping;
		}

		list_add_tail(&rx_pkt->link, &sys->head_desc_list);
		rx_len_cached = ++sys->len;

		ret = sps_transfer_one(sys->ep->ep_hdl, rx_pkt->dma_address,
				       IPA_RX_SKB_SIZE, rx_pkt,
				       0);

		if (ret) {
			IPAERR("sps_transfer_one failed %d\n", ret);
			goto fail_sps_transfer;
		}
	}

	ipa_ctx->stats.rx_q_len = sys->len;

	return;

fail_sps_transfer:
	list_del(&rx_pkt->link);
	rx_len_cached = --sys->len;
	dma_unmap_single(NULL, rx_pkt->dma_address, IPA_RX_SKB_SIZE,
			 DMA_FROM_DEVICE);
fail_dma_mapping:
	dev_kfree_skb(rx_pkt->skb);
fail_skb_alloc:
	kfree(rx_pkt);
fail_kmem_cache_alloc:
	if (rx_len_cached == 0) {
		IPA_STATS_INC_CNT(ipa_ctx->stats.rx_repl_repost);
		schedule_delayed_work(&replenish_rx_work,
				msecs_to_jiffies(100));
	}
	ipa_ctx->stats.rx_q_len = sys->len;
	return;
}

static void replenish_rx_work_func(struct work_struct *work)
{
	ipa_replenish_rx_cache();
}

/**
 * ipa_cleanup_rx() - release RX queue resources
 *
 */
void ipa_cleanup_rx(void)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;
	struct ipa_rx_pkt_wrapper *r;
	struct ipa_sys_context *sys = &ipa_ctx->sys[IPA_A5_LAN_WAN_IN];

	list_for_each_entry_safe(rx_pkt, r,
				 &sys->head_desc_list, link) {
		list_del(&rx_pkt->link);
		dma_unmap_single(NULL, rx_pkt->dma_address, IPA_RX_SKB_SIZE,
				 DMA_FROM_DEVICE);
		dev_kfree_skb(rx_pkt->skb);
		kfree(rx_pkt);
	}
}

