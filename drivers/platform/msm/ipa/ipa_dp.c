/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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


#define IPA_LAST_DESC_CNT 0xFFFF
#define POLLING_INACTIVITY_RX 40
#define POLLING_MIN_SLEEP_RX 1010
#define POLLING_MAX_SLEEP_RX 1050
#define POLLING_INACTIVITY_TX 40
#define POLLING_MIN_SLEEP_TX 400
#define POLLING_MAX_SLEEP_TX 500
/* 8K less 1 nominal MTU (1500 bytes) rounded to units of KB */
#define IPA_GENERIC_AGGR_BYTE_LIMIT 6
#define IPA_GENERIC_AGGR_TIME_LIMIT 1
#define IPA_GENERIC_AGGR_PKT_LIMIT 0
#define IPA_GENERIC_RX_POOL_SZ 32
/* 8K less the headroom (NET_SKB_PAD) and skb_shared_info which are implicitly
 * part of the data buffer */
#define IPA_LAN_RX_BUFF_SZ 7936

#define IPA_WLAN_RX_POOL_SZ 100
#define IPA_WLAN_RX_POOL_SZ_LOW_WM 5
#define IPA_WLAN_RX_BUFF_SZ 2048
#define IPA_WLAN_COMM_RX_POOL_LOW 100
#define IPA_WLAN_COMM_RX_POOL_HIGH 900

#define IPA_ODU_RX_BUFF_SZ 2048
#define IPA_ODU_RX_POOL_SZ 32
#define IPA_SIZE_DL_CSUM_META_TRAILER 8

static struct sk_buff *ipa_get_skb_ipa_rx(unsigned int len, gfp_t flags);
static void ipa_replenish_wlan_rx_cache(struct ipa_sys_context *sys);
static void ipa_replenish_rx_cache(struct ipa_sys_context *sys);
static void replenish_rx_work_func(struct work_struct *work);
static void ipa_wq_handle_rx(struct work_struct *work);
static void ipa_wq_handle_tx(struct work_struct *work);
static void ipa_wq_rx_common(struct ipa_sys_context *sys, u32 size);
static void ipa_wlan_wq_rx_common(struct ipa_sys_context *sys,
				u32 size);
static int ipa_assign_policy(struct ipa_sys_connect_params *in,
		struct ipa_sys_context *sys);
static void ipa_cleanup_rx(struct ipa_sys_context *sys);
static void ipa_wq_rx_avail(struct work_struct *work);
static void ipa_alloc_wlan_rx_common_cache(u32 size);
static void ipa_cleanup_wlan_rx_common_cache(void);
static void ipa_wq_repl_rx(struct work_struct *work);

static void ipa_wq_write_done_common(struct ipa_sys_context *sys, u32 cnt)
{
	struct ipa_tx_pkt_wrapper *tx_pkt_expected;
	int i;

	for (i = 0; i < cnt; i++) {
		spin_lock_bh(&sys->spinlock);
		if (unlikely(list_empty(&sys->head_desc_list))) {
			spin_unlock_bh(&sys->spinlock);
			return;
		}
		tx_pkt_expected = list_first_entry(&sys->head_desc_list,
						   struct ipa_tx_pkt_wrapper,
						   link);
		list_del(&tx_pkt_expected->link);
		sys->len--;
		spin_unlock_bh(&sys->spinlock);
		if (!tx_pkt_expected->no_unmap_dma)
			dma_unmap_single(ipa_ctx->pdev,
					tx_pkt_expected->mem.phys_base,
					tx_pkt_expected->mem.size,
					DMA_TO_DEVICE);
		if (tx_pkt_expected->callback)
			tx_pkt_expected->callback(tx_pkt_expected->user1,
					tx_pkt_expected->user2);
		if (tx_pkt_expected->cnt > 1 &&
				tx_pkt_expected->cnt != IPA_LAST_DESC_CNT) {
			if (tx_pkt_expected->cnt == IPA_NUM_DESC_PER_SW_TX) {
				dma_pool_free(ipa_ctx->dma_pool,
					tx_pkt_expected->mult.base,
					tx_pkt_expected->mult.phys_base);
			} else {
				dma_unmap_single(ipa_ctx->pdev,
					tx_pkt_expected->mult.phys_base,
					tx_pkt_expected->mult.size,
					DMA_TO_DEVICE);
				kfree(tx_pkt_expected->mult.base);
			}
		}
		kmem_cache_free(ipa_ctx->tx_pkt_wrapper_cache, tx_pkt_expected);
	}
}

static void ipa_wq_write_done_status(int src_pipe)
{
	struct ipa_tx_pkt_wrapper *tx_pkt_expected;
	struct ipa_sys_context *sys;
	u32 cnt;

	WARN_ON(src_pipe >= IPA_NUM_PIPES);

	if (!ipa_ctx->ep[src_pipe].status.status_en)
		return;

	sys = ipa_ctx->ep[src_pipe].sys;
	if (!sys)
		return;

	spin_lock_bh(&sys->spinlock);
	if (unlikely(list_empty(&sys->head_desc_list))) {
		spin_unlock_bh(&sys->spinlock);
		return;
	}
	tx_pkt_expected = list_first_entry(&sys->head_desc_list,
					   struct ipa_tx_pkt_wrapper,
					   link);
	cnt = tx_pkt_expected->cnt;
	spin_unlock_bh(&sys->spinlock);
	ipa_wq_write_done_common(sys, cnt);
}

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
static void ipa_wq_write_done(struct work_struct *work)
{
	struct ipa_tx_pkt_wrapper *tx_pkt;
	u32 cnt;
	struct ipa_sys_context *sys;

	tx_pkt = container_of(work, struct ipa_tx_pkt_wrapper, work);
	cnt = tx_pkt->cnt;
	sys = tx_pkt->sys;

	ipa_wq_write_done_common(sys, cnt);
}

static int ipa_handle_tx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state)
{
	struct sps_iovec iov;
	int ret;
	int cnt = 0;

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

		ipa_wq_write_done_common(sys, 1);
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

	ret = sps_get_config(sys->ep->ep_hdl, &sys->ep->connect);
	if (ret) {
		IPAERR("sps_get_config() failed %d\n", ret);
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
	queue_delayed_work(sys->wq, &sys->switch_to_intr_work,
			msecs_to_jiffies(1));
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
	struct ipa_sys_context *sys;
	sys = container_of(work, struct ipa_sys_context, work);
	ipa_handle_tx(sys);
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
	int result;
	u16 sps_flags = SPS_IOVEC_FLAG_EOT;
	dma_addr_t dma_address;
	u16 len;
	u32 mem_flag = GFP_ATOMIC;

	if (unlikely(!in_atomic))
		mem_flag = GFP_KERNEL;

	tx_pkt = kmem_cache_zalloc(ipa_ctx->tx_pkt_wrapper_cache, mem_flag);
	if (!tx_pkt) {
		IPAERR("failed to alloc tx wrapper\n");
		goto fail_mem_alloc;
	}

	if (!desc->dma_address_valid) {
		if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0)) {
			WARN_ON(desc->len > 512);

			/*
			 * Due to a HW limitation, we need to make sure that
			 * the packet does not cross a 1KB boundary
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
			dma_address = dma_map_single(ipa_ctx->pdev, desc->pyld,
				desc->len, DMA_TO_DEVICE);
		}
	} else {
		dma_address = desc->dma_address;
		tx_pkt->no_unmap_dma = true;
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

	spin_lock_bh(&sys->spinlock);
	list_add_tail(&tx_pkt->link, &sys->head_desc_list);
	result = sps_transfer_one(sys->ep->ep_hdl, dma_address, len, tx_pkt,
			sps_flags);
	if (result) {
		IPAERR("sps_transfer_one failed rc=%d\n", result);
		goto fail_sps_send;
	}

	spin_unlock_bh(&sys->spinlock);

	return 0;

fail_sps_send:
	list_del(&tx_pkt->link);
	spin_unlock_bh(&sys->spinlock);
	if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0))
		dma_pool_free(ipa_ctx->dma_pool, tx_pkt->bounce,
				dma_address);
	else
		dma_unmap_single(ipa_ctx->pdev, dma_address, desc->len,
				DMA_TO_DEVICE);
fail_dma_map:
	kmem_cache_free(ipa_ctx->tx_pkt_wrapper_cache, tx_pkt);
fail_mem_alloc:
	return -EFAULT;
}

/**
 * ipa_send() - Send multiple descriptors in one HW transaction
 * @sys: system pipe context
 * @num_desc: number of packets
 * @desc: packets to send (may be immediate command or data)
 * @in_atomic:  whether caller is in atomic context
 *
 * This function is used for system-to-bam connection.
 * - SPS driver expect struct sps_transfer which will contain all the data
 *   for a transaction
 * - The sps_transfer struct will be pointing to bounce buffers for
 *   its DMA command (immediate command and data)
 * - ipa_tx_pkt_wrapper will be used for each ipa
 *   descriptor (allocated from wrappers cache)
 * - The wrapper struct will be configured for each ipa-desc payload and will
 *   contain information which will be later used by the user callbacks
 * - each transfer will be made by calling to sps_transfer()
 * - Each packet (command or data) that will be sent will also be saved in
 *   ipa_sys_context for later check that all data was sent
 *
 * Return codes: 0: success, -EFAULT: failure
 */
int ipa_send(struct ipa_sys_context *sys, u32 num_desc, struct ipa_desc *desc,
		bool in_atomic)
{
	struct ipa_tx_pkt_wrapper *tx_pkt;
	struct ipa_tx_pkt_wrapper *next_pkt;
	struct sps_transfer transfer = { 0 };
	struct sps_iovec *iovec;
	dma_addr_t dma_addr;
	int i = 0;
	int j;
	int result;
	int fail_dma_wrap = 0;
	uint size = num_desc * sizeof(struct sps_iovec);
	u32 mem_flag = GFP_ATOMIC;

	if (unlikely(!in_atomic))
		mem_flag = GFP_KERNEL;

	if (num_desc == IPA_NUM_DESC_PER_SW_TX) {
		transfer.iovec = dma_pool_alloc(ipa_ctx->dma_pool, mem_flag,
				&dma_addr);
		if (!transfer.iovec) {
			IPAERR("fail to alloc dma mem for sps xfr buff\n");
			return -EFAULT;
		}
	} else {
		transfer.iovec = kmalloc(size, mem_flag);
		if (!transfer.iovec) {
			IPAERR("fail to alloc mem for sps xfr buff ");
			IPAERR("num_desc = %d size = %d\n", num_desc, size);
			return -EFAULT;
		}
		dma_addr  = dma_map_single(ipa_ctx->pdev,
				transfer.iovec, size, DMA_TO_DEVICE);
		if (!dma_addr) {
			IPAERR("dma_map_single failed for sps xfr buff\n");
			kfree(transfer.iovec);
			return -EFAULT;
		}
	}

	transfer.iovec_phys = dma_addr;
	transfer.iovec_count = num_desc;
	spin_lock_bh(&sys->spinlock);

	for (i = 0; i < num_desc; i++) {
		fail_dma_wrap = 0;
		tx_pkt = kmem_cache_zalloc(ipa_ctx->tx_pkt_wrapper_cache,
					   mem_flag);
		if (!tx_pkt) {
			IPAERR("failed to alloc tx wrapper\n");
			goto failure;
		}
		/*
		 * first desc of set is "special" as it holds the count and
		 * other info
		 */
		if (i == 0) {
			transfer.user = tx_pkt;
			tx_pkt->mult.phys_base = dma_addr;
			tx_pkt->mult.base = transfer.iovec;
			tx_pkt->mult.size = size;
			tx_pkt->cnt = num_desc;
			INIT_WORK(&tx_pkt->work, ipa_wq_write_done);
		}

		iovec = &transfer.iovec[i];
		iovec->flags = 0;

		INIT_LIST_HEAD(&tx_pkt->link);
		tx_pkt->type = desc[i].type;

		tx_pkt->mem.base = desc[i].pyld;
		tx_pkt->mem.size = desc[i].len;

		if (!desc->dma_address_valid) {
			if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0)) {
				WARN_ON(tx_pkt->mem.size > 512);

				/*
				 * Due to a HW limitation, we need to make sure
				 * that the packet does not cross a
				 * 1KB boundary
				 */
				tx_pkt->bounce =
				dma_pool_alloc(ipa_ctx->dma_pool,
					       mem_flag,
					       &tx_pkt->mem.phys_base);
				if (!tx_pkt->bounce) {
					tx_pkt->mem.phys_base = 0;
				} else {
					WARN_ON(!ipa_straddle_boundary(
						(u32)tx_pkt->mem.phys_base,
						(u32)tx_pkt->mem.phys_base +
						tx_pkt->mem.size - 1, 1024));
					memcpy(tx_pkt->bounce, tx_pkt->mem.base,
						tx_pkt->mem.size);
				}
			} else {
				tx_pkt->mem.phys_base =
					dma_map_single(ipa_ctx->pdev,
					tx_pkt->mem.base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			}
		} else {
			tx_pkt->mem.phys_base = desc->dma_address;
			tx_pkt->no_unmap_dma = true;
		}

		if (!tx_pkt->mem.phys_base) {
			IPAERR("failed to alloc tx wrapper\n");
			fail_dma_wrap = 1;
			goto failure;
		}

		tx_pkt->sys = sys;
		tx_pkt->callback = desc[i].callback;
		tx_pkt->user1 = desc[i].user1;
		tx_pkt->user2 = desc[i].user2;

		/*
		 * Point the iovec to the bounce buffer and
		 * add this packet to system pipe context.
		 */
		iovec->addr = tx_pkt->mem.phys_base;
		list_add_tail(&tx_pkt->link, &sys->head_desc_list);

		/*
		 * Special treatment for immediate commands, where the structure
		 * of the descriptor is different
		 */
		if (desc[i].type == IPA_IMM_CMD_DESC) {
			iovec->size = desc[i].opcode;
			iovec->flags |= SPS_IOVEC_FLAG_IMME;
			IPA_DUMP_BUFF(desc[i].pyld,
					tx_pkt->mem.phys_base, desc[i].len);
		} else {
			iovec->size = desc[i].len;
		}

		if (i == (num_desc - 1)) {
			iovec->flags |= SPS_IOVEC_FLAG_EOT;
			/* "mark" the last desc */
			tx_pkt->cnt = IPA_LAST_DESC_CNT;
		}
	}

	result = sps_transfer(sys->ep->ep_hdl, &transfer);
	if (result) {
		IPAERR("sps_transfer failed rc=%d\n", result);
		goto failure;
	}

	spin_unlock_bh(&sys->spinlock);
	return 0;

failure:
	tx_pkt = transfer.user;
	for (j = 0; j < i; j++) {
		next_pkt = list_next_entry(tx_pkt, link);
		list_del(&tx_pkt->link);
		if (unlikely(ipa_ctx->ipa_hw_type == IPA_HW_v1_0))
			dma_pool_free(ipa_ctx->dma_pool,
					tx_pkt->bounce,
					tx_pkt->mem.phys_base);
		else
			dma_unmap_single(ipa_ctx->pdev, tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
		kmem_cache_free(ipa_ctx->tx_pkt_wrapper_cache, tx_pkt);
		tx_pkt = next_pkt;
	}
	if (i < num_desc)
		/* last desc failed */
		if (fail_dma_wrap)
			kmem_cache_free(ipa_ctx->tx_pkt_wrapper_cache, tx_pkt);
	if (transfer.iovec_phys) {
		if (num_desc == IPA_NUM_DESC_PER_SW_TX) {
			dma_pool_free(ipa_ctx->dma_pool, transfer.iovec,
					transfer.iovec_phys);
		} else {
			dma_unmap_single(ipa_ctx->pdev, transfer.iovec_phys,
					size, DMA_TO_DEVICE);
			kfree(transfer.iovec);
		}
	}
	spin_unlock_bh(&sys->spinlock);
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
static void ipa_sps_irq_cmd_ack(void *user1, int user2)
{
	struct ipa_desc *desc = (struct ipa_desc *)user1;

	if (!desc) {
		IPAERR("desc is NULL\n");
		WARN_ON(1);
		return;
	}
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
	struct ipa_desc *desc;
	int result = 0;
	struct ipa_sys_context *sys;
	int ep_idx;

	IPADBG("sending command\n");

	ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	sys = ipa_ctx->ep[ep_idx].sys;

	ipa_inc_client_enable_clks();

	if (num_desc == 1) {
		init_completion(&descr->xfer_done);

		if (descr->callback || descr->user1)
			WARN_ON(1);

		descr->callback = ipa_sps_irq_cmd_ack;
		descr->user1 = descr;
		if (ipa_send_one(sys, descr, true)) {
			IPAERR("fail to send immediate command\n");
			result = -EFAULT;
			goto bail;
		}
		wait_for_completion(&descr->xfer_done);
	} else {
		desc = &descr[num_desc - 1];
		init_completion(&desc->xfer_done);

		if (desc->callback || desc->user1)
			WARN_ON(1);

		desc->callback = ipa_sps_irq_cmd_ack;
		desc->user1 = desc;
		if (ipa_send(sys, num_desc, descr, true)) {
			IPAERR("fail to send multiple immediate command set\n");
			result = -EFAULT;
			goto bail;
		}
		wait_for_completion(&desc->xfer_done);
	}

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
	struct ipa_sys_context *sys = (struct ipa_sys_context *)notify->user;
	int ret;

	IPADBG("event %d notified\n", notify->event_id);

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		if (!atomic_read(&sys->curr_polling_state)) {
			ret = sps_get_config(sys->ep->ep_hdl,
					&sys->ep->connect);
			if (ret) {
				IPAERR("sps_get_config() failed %d\n", ret);
				break;
			}
			sys->ep->connect.options = SPS_O_AUTO_ENABLE |
				SPS_O_ACK_TRANSFERS | SPS_O_POLL;
			ret = sps_set_config(sys->ep->ep_hdl,
					&sys->ep->connect);
			if (ret) {
				IPAERR("sps_set_config() failed %d\n", ret);
				break;
			}
			atomic_set(&sys->curr_polling_state, 1);
			queue_work(sys->wq, &sys->work);
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
		queue_work(tx_pkt->sys->wq, &tx_pkt->work);
		break;
	default:
		IPAERR("recieved unexpected event id %d\n", notify->event_id);
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
static int ipa_handle_rx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state)
{
	struct sps_iovec iov;
	int ret;
	int cnt = 0;

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

		if (IPA_CLIENT_IS_WLAN_CONS(sys->ep->client))
			ipa_wlan_wq_rx_common(sys, iov.size);
		else
			ipa_wq_rx_common(sys, iov.size);

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

	ret = sps_get_config(sys->ep->ep_hdl, &sys->ep->connect);
	if (ret) {
		IPAERR("sps_get_config() failed %d\n", ret);
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
	queue_delayed_work(sys->wq, &sys->switch_to_intr_work,
			msecs_to_jiffies(1));
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
	struct ipa_sys_context *sys = (struct ipa_sys_context *)notify->user;
	int ret;

	IPADBG("event %d notified\n", notify->event_id);

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		if (!atomic_read(&sys->curr_polling_state)) {
			ret = sps_get_config(sys->ep->ep_hdl,
					&sys->ep->connect);
			if (ret) {
				IPAERR("sps_get_config() failed %d\n", ret);
				break;
			}
			sys->ep->connect.options = SPS_O_AUTO_ENABLE |
				SPS_O_ACK_TRANSFERS | SPS_O_POLL;
			ret = sps_set_config(sys->ep->ep_hdl,
					&sys->ep->connect);
			if (ret) {
				IPAERR("sps_set_config() failed %d\n", ret);
				break;
			}
			atomic_set(&sys->curr_polling_state, 1);
			queue_work(sys->wq, &sys->work);
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
	} while (inactive_cycles <= POLLING_INACTIVITY_RX);

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
	struct ipa_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;
	dma_addr_t dma_addr;
	char buff[IPA_RESOURCE_NAME_MAX];

	if (sys_in == NULL || clnt_hdl == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}

	if (sys_in->client >= IPA_CLIENT_MAX || sys_in->desc_fifo_sz == 0) {
		IPAERR("bad parm client:%d fifo_sz:%d\n",
			sys_in->client, sys_in->desc_fifo_sz);
		goto fail_gen;
	}

	ipa_ep_idx = ipa_get_ep_mapping(sys_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		goto fail_gen;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	ipa_inc_client_enable_clks();

	if (ep->valid == 1) {
		if (sys_in->client != IPA_CLIENT_APPS_LAN_WAN_PROD) {
			IPAERR("EP already allocated.\n");
			goto fail_and_disable_clocks;
		} else {
			if (ipa_cfg_ep_hdr(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.hdr)) {
				IPAERR("fail to configure hdr prop of EP.\n");
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			if (ipa_cfg_ep_cfg(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.cfg)) {
				IPAERR("fail to configure cfg prop of EP.\n");
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			IPADBG("client %d (ep: %d) overlay ok sys=%p\n",
					sys_in->client, ipa_ep_idx, ep->sys);
			ep->client_notify = sys_in->notify;
			ep->priv = sys_in->priv;
			*clnt_hdl = ipa_ep_idx;
			if (!ep->keep_ipa_awake)
				ipa_dec_client_disable_clks();

			return 0;
		}
	}

	memset(ep, 0, offsetof(struct ipa_ep_context, sys));

	if (!ep->sys) {
		ep->sys = kzalloc(sizeof(struct ipa_sys_context), GFP_KERNEL);
		if (!ep->sys) {
			IPAERR("failed to sys ctx for client %d\n",
					sys_in->client);
			result = -ENOMEM;
			goto fail_and_disable_clocks;
		}

		ep->sys->ep = ep;
		snprintf(buff, IPA_RESOURCE_NAME_MAX, "ipawq%d",
				sys_in->client);
		ep->sys->wq = alloc_workqueue(buff,
				WQ_MEM_RECLAIM | WQ_UNBOUND, 1);
		if (!ep->sys->wq) {
			IPAERR("failed to create wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq;
		}

		snprintf(buff, IPA_RESOURCE_NAME_MAX, "iparepwq%d",
				sys_in->client);
		ep->sys->repl_wq = alloc_workqueue(buff,
				WQ_MEM_RECLAIM | WQ_UNBOUND, 1);
		if (!ep->sys->repl_wq) {
			IPAERR("failed to create rep wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq2;
		}

		INIT_LIST_HEAD(&ep->sys->head_desc_list);
		spin_lock_init(&ep->sys->spinlock);
	} else {
		memset(ep->sys, 0, offsetof(struct ipa_sys_context, ep));
	}

	ep->skip_ep_cfg = sys_in->skip_ep_cfg;
	if (ipa_assign_policy(sys_in, ep->sys)) {
		IPAERR("failed to sys ctx for client %d\n", sys_in->client);
		result = -ENOMEM;
		goto fail_gen2;
	}

	ep->valid = 1;
	ep->client = sys_in->client;
	ep->client_notify = sys_in->notify;
	ep->priv = sys_in->priv;
	ep->keep_ipa_awake = sys_in->keep_ipa_awake;
	atomic_set(&ep->avail_fifo_desc,
		((sys_in->desc_fifo_sz/sizeof(struct sps_iovec))-1));

	result = ipa_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
				ipa_ep_idx);
		goto fail_gen2;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_gen2;
		}
		if (ipa_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_gen2;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("skipping ep configuration\n");
	}

	/* Default Config */
	ep->ep_hdl = sps_alloc_endpoint();
	if (ep->ep_hdl == NULL) {
		IPAERR("SPS EP allocation failed.\n");
		goto fail_gen2;
	}

	result = sps_get_config(ep->ep_hdl, &ep->connect);
	if (result) {
		IPAERR("fail to get config.\n");
		goto fail_sps_cfg;
	}

	/* Specific Config */
	if (IPA_CLIENT_IS_CONS(sys_in->client)) {
		ep->connect.mode = SPS_MODE_SRC;
		ep->connect.destination = SPS_DEV_HANDLE_MEM;
		ep->connect.source = ipa_ctx->bam_handle;
		ep->connect.dest_pipe_index = ipa_ctx->a5_pipe_index++;
		ep->connect.src_pipe_index = ipa_ep_idx;
	} else {
		ep->connect.mode = SPS_MODE_DEST;
		ep->connect.source = SPS_DEV_HANDLE_MEM;
		ep->connect.destination = ipa_ctx->bam_handle;
		ep->connect.src_pipe_index = ipa_ctx->a5_pipe_index++;
		ep->connect.dest_pipe_index = ipa_ep_idx;
	}

	IPADBG("client:%d ep:%d",
		sys_in->client, ipa_ep_idx);

	IPADBG("dest_pipe_index:%d src_pipe_index:%d\n",
		ep->connect.dest_pipe_index,
		ep->connect.src_pipe_index);

	ep->connect.options = ep->sys->sps_option;
	ep->connect.desc.size = sys_in->desc_fifo_sz;
	ep->connect.desc.base = dma_alloc_coherent(ipa_ctx->pdev,
			ep->connect.desc.size, &dma_addr, 0);
	ep->connect.desc.phys_base = dma_addr;
	if (ep->connect.desc.base == NULL) {
		IPAERR("fail to get DMA desc memory.\n");
		goto fail_sps_cfg;
	}

	ep->connect.event_thresh = IPA_EVENT_THRESHOLD;

	result = ipa_sps_connect_safe(ep->ep_hdl, &ep->connect, sys_in->client);
	if (result) {
		IPAERR("sps_connect fails.\n");
		goto fail_sps_connect;
	}

	ep->sys->event.options = SPS_O_EOT;
	ep->sys->event.mode = SPS_TRIGGER_CALLBACK;
	ep->sys->event.xfer_done = NULL;
	ep->sys->event.user = ep->sys;
	ep->sys->event.callback = ep->sys->sps_callback;
	result = sps_register_event(ep->ep_hdl, &ep->sys->event);
	if (result < 0) {
		IPAERR("register event error %d\n", result);
		goto fail_register_event;
	}

	*clnt_hdl = ipa_ep_idx;

	if (IPA_CLIENT_IS_CONS(sys_in->client))
		ipa_replenish_rx_cache(ep->sys);

	if (IPA_CLIENT_IS_WLAN_CONS(sys_in->client)) {
		ipa_alloc_wlan_rx_common_cache(IPA_WLAN_COMM_RX_POOL_LOW);
		atomic_inc(&ipa_ctx->wc_memb.active_clnt_cnt);
	}

	if (nr_cpu_ids > 1 &&
		(sys_in->client == IPA_CLIENT_APPS_LAN_CONS ||
		 sys_in->client == IPA_CLIENT_APPS_WAN_CONS)) {
		ep->sys->repl.capacity = ep->sys->rx_pool_sz + 1;
		ep->sys->repl.cache = kzalloc(ep->sys->repl.capacity *
				sizeof(void *), GFP_KERNEL);
		if (!ep->sys->repl.cache) {
			IPAERR("ep=%d fail to alloc repl cache\n", ipa_ep_idx);
			ep->sys->repl_hdlr = ipa_replenish_rx_cache;
			ep->sys->repl.capacity = 0;
		} else {
			atomic_set(&ep->sys->repl.head_idx, 0);
			atomic_set(&ep->sys->repl.tail_idx, 0);
			ipa_wq_repl_rx(&ep->sys->repl_work);
		}
	}

	ipa_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(sys_in->client))
		ipa_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		ipa_dec_client_disable_clks();

	IPADBG("client %d (ep: %d) connected sys=%p\n", sys_in->client,
			ipa_ep_idx, ep->sys);

	return 0;

fail_register_event:
	sps_disconnect(ep->ep_hdl);
fail_sps_connect:
	dma_free_coherent(ipa_ctx->pdev, ep->connect.desc.size,
			  ep->connect.desc.base,
			  ep->connect.desc.phys_base);
fail_sps_cfg:
	sps_free_endpoint(ep->ep_hdl);
fail_gen2:
	destroy_workqueue(ep->sys->repl_wq);
fail_wq2:
	destroy_workqueue(ep->sys->wq);
fail_wq:
	kfree(ep->sys);
	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
fail_and_disable_clocks:
	ipa_dec_client_disable_clks();
fail_gen:
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
	struct ipa_ep_context *ep;
	int empty;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		ipa_inc_client_enable_clks();

	ipa_disable_data_path(clnt_hdl);
	ep->valid = 0;

	if (IPA_CLIENT_IS_PROD(ep->client)) {
		do {
			spin_lock_bh(&ep->sys->spinlock);
			empty = list_empty(&ep->sys->head_desc_list);
			spin_unlock_bh(&ep->sys->spinlock);
			if (!empty)
				usleep(100);
			else
				break;
		} while (1);
	}

	flush_workqueue(ep->sys->wq);
	sps_disconnect(ep->ep_hdl);
	dma_free_coherent(ipa_ctx->pdev, ep->connect.desc.size,
			  ep->connect.desc.base,
			  ep->connect.desc.phys_base);
	sps_free_endpoint(ep->ep_hdl);
	if (IPA_CLIENT_IS_CONS(ep->client))
		ipa_cleanup_rx(ep->sys);

	ipa_delete_dflt_flt_rules(clnt_hdl);

	if (IPA_CLIENT_IS_WLAN_CONS(ep->client))
		atomic_dec(&ipa_ctx->wc_memb.active_clnt_cnt);

	memset(&ep->wstats, 0, sizeof(struct ipa_wlan_stats));

	if (!atomic_read(&ipa_ctx->wc_memb.active_clnt_cnt))
		ipa_cleanup_wlan_rx_common_cache();

	ipa_dec_client_disable_clks();

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
static void ipa_tx_comp_usr_notify_release(void *user1, int user2)
{
	struct sk_buff *skb = (struct sk_buff *)user1;
	int ep_idx = user2;

	IPADBG("skb=%p ep=%d\n", skb, ep_idx);

	IPA_STATS_INC_CNT(ipa_ctx->stats.tx_pkts_compl);

	if (ipa_ctx->ep[ep_idx].client_notify)
		ipa_ctx->ep[ep_idx].client_notify(ipa_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)skb);
	else
		dev_kfree_skb_any(skb);
}

static void ipa_tx_cmd_comp(void *user1, int user2)
{
	kfree(user1);
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
	struct ipa_desc desc[2];
	int dst_ep_idx;
	struct ipa_ip_packet_init *cmd;
	struct ipa_sys_context *sys;
	int src_ep_idx;

	memset(desc, 0, 2 * sizeof(struct ipa_desc));

	/*
	 * USB_CONS: PKT_INIT ep_idx = dst pipe
	 * Q6_CONS: PKT_INIT ep_idx = sender pipe
	 * A5_LAN_WAN_PROD: HW path ep_idx = sender pipe
	 *
	 * LAN TX: all PKT_INIT
	 * WAN TX: PKT_INIT (cmd) + HW (data)
	 *
	 */
	if (IPA_CLIENT_IS_CONS(dst)) {
		src_ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD);
		dst_ep_idx = ipa_get_ep_mapping(dst);
	} else {
		src_ep_idx = ipa_get_ep_mapping(dst);
		if (meta && meta->pkt_init_dst_ep_valid)
			dst_ep_idx = meta->pkt_init_dst_ep;
		else
			dst_ep_idx = -1;
	}

	sys = ipa_ctx->ep[src_ep_idx].sys;

	if (!sys->ep->valid) {
		IPAERR("pipe not valid\n");
		goto fail_gen;
	}

	if (dst_ep_idx != -1) {
		/* SW data path */
		cmd = kzalloc(sizeof(struct ipa_ip_packet_init), GFP_ATOMIC);
		if (!cmd) {
			IPAERR("failed to alloc immediate command object\n");
			goto fail_gen;
		}

		cmd->destination_pipe_index = dst_ep_idx;
		if (meta && meta->mbim_stream_id_valid)
			cmd->metadata = meta->mbim_stream_id;
		desc[0].opcode = IPA_IP_PACKET_INIT;
		desc[0].pyld = cmd;
		desc[0].len = sizeof(struct ipa_ip_packet_init);
		desc[0].type = IPA_IMM_CMD_DESC;
		desc[0].callback = ipa_tx_cmd_comp;
		desc[0].user1 = cmd;
		desc[1].pyld = skb->data;
		desc[1].len = skb->len;
		desc[1].type = IPA_DATA_DESC_SKB;
		desc[1].callback = ipa_tx_comp_usr_notify_release;
		desc[1].user1 = skb;
		desc[1].user2 = (meta && meta->pkt_init_dst_ep_valid &&
				meta->pkt_init_dst_ep_remote) ?
				src_ep_idx :
				dst_ep_idx;
		if (meta && meta->dma_address_valid) {
			desc[1].dma_address_valid = true;
			desc[1].dma_address = meta->dma_address;
		}

		if (ipa_send(sys, 2, desc, true)) {
			IPAERR("fail to send immediate command\n");
			goto fail_send;
		}
		IPA_STATS_INC_CNT(ipa_ctx->stats.tx_sw_pkts);
	} else {
		/* HW data path */
		desc[0].pyld = skb->data;
		desc[0].len = skb->len;
		desc[0].type = IPA_DATA_DESC_SKB;
		desc[0].callback = ipa_tx_comp_usr_notify_release;
		desc[0].user1 = skb;
		desc[0].user2 = src_ep_idx;

		if (meta && meta->dma_address_valid) {
			desc[0].dma_address_valid = true;
			desc[0].dma_address = meta->dma_address;
		}

		if (ipa_send_one(sys, &desc[0], true)) {
			IPAERR("fail to send skb\n");
			goto fail_gen;
		}
		IPA_STATS_INC_CNT(ipa_ctx->stats.tx_hw_pkts);
	}

	return 0;

fail_send:
	kfree(cmd);
fail_gen:
	return -EFAULT;
}
EXPORT_SYMBOL(ipa_tx_dp);

static void ipa_wq_handle_rx(struct work_struct *work)
{
	struct ipa_sys_context *sys;
	sys = container_of(work, struct ipa_sys_context, work);
	ipa_handle_rx(sys);
}

static void ipa_wq_repl_rx(struct work_struct *work)
{
	struct ipa_sys_context *sys;
	void *ptr;
	struct ipa_rx_pkt_wrapper *rx_pkt;
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;
	u32 next;
	u32 curr;

	sys = container_of(work, struct ipa_sys_context, repl_work);
	curr = atomic_read(&sys->repl.tail_idx);

begin:
	while (1) {
		next = (curr + 1) % sys->repl.capacity;
		if (next == atomic_read(&sys->repl.head_idx))
			goto fail_kmem_cache_alloc;

		rx_pkt = kmem_cache_zalloc(ipa_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt) {
			IPAERR("failed to alloc rx wrapper\n");
			goto fail_kmem_cache_alloc;
		}

		INIT_LIST_HEAD(&rx_pkt->link);
		INIT_WORK(&rx_pkt->work, ipa_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (rx_pkt->data.dma_addr == 0 ||
				rx_pkt->data.dma_addr == ~0) {
			IPAERR("dma_map_single failure %p for %p\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		sys->repl.cache[curr] = rx_pkt;
		curr = next;
		mb();
		atomic_set(&sys->repl.tail_idx, next);
	}

	return;

fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	if (atomic_read(&sys->repl.tail_idx) ==
			atomic_read(&sys->repl.head_idx)) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS)
			IPA_STATS_INC_CNT(ipa_ctx->stats.wan_repl_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)
			IPA_STATS_INC_CNT(ipa_ctx->stats.lan_repl_rx_empty);
		else
			WARN_ON(1);
		goto begin;
	}
}

static void ipa_replenish_wlan_rx_cache(struct ipa_sys_context *sys)
{
	struct ipa_rx_pkt_wrapper *rx_pkt = NULL;
	struct ipa_rx_pkt_wrapper *tmp;
	int ret;
	u32 rx_len_cached = 0;

	IPADBG("\n");

	spin_lock_bh(&ipa_ctx->wc_memb.wlan_spinlock);
	rx_len_cached = sys->len;

	if (rx_len_cached < sys->rx_pool_sz) {
		list_for_each_entry_safe(rx_pkt, tmp,
			&ipa_ctx->wc_memb.wlan_comm_desc_list, link)
		{
			list_del(&rx_pkt->link);

			if (ipa_ctx->wc_memb.wlan_comm_free_cnt > 0)
				ipa_ctx->wc_memb.wlan_comm_free_cnt--;

			INIT_LIST_HEAD(&rx_pkt->link);
			rx_pkt->len = 0;
			rx_pkt->sys = sys;

			ret = sps_transfer_one(sys->ep->ep_hdl,
				rx_pkt->data.dma_addr,
				IPA_WLAN_RX_BUFF_SZ, rx_pkt, 0);

			if (ret) {
				IPAERR("sps_transfer_one failed %d\n", ret);
				goto fail_sps_transfer;
			}

			list_add_tail(&rx_pkt->link, &sys->head_desc_list);
			rx_len_cached = ++sys->len;

			if (rx_len_cached >= sys->rx_pool_sz) {
				spin_unlock_bh(&ipa_ctx->wc_memb.wlan_spinlock);
				return;
			}
		}
	}
	spin_unlock_bh(&ipa_ctx->wc_memb.wlan_spinlock);

	if (rx_len_cached < sys->rx_pool_sz &&
			ipa_ctx->wc_memb.wlan_comm_total_cnt <
			 IPA_WLAN_COMM_RX_POOL_HIGH) {
		ipa_replenish_rx_cache(sys);
		ipa_ctx->wc_memb.wlan_comm_total_cnt +=
			(sys->rx_pool_sz - rx_len_cached);
	}

	return;

fail_sps_transfer:
	list_del(&rx_pkt->link);
	spin_unlock_bh(&ipa_ctx->wc_memb.wlan_spinlock);

	return;
}

static void ipa_cleanup_wlan_rx_common_cache(void)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;
	struct ipa_rx_pkt_wrapper *tmp;

	list_for_each_entry_safe(rx_pkt, tmp,
		&ipa_ctx->wc_memb.wlan_comm_desc_list, link) {
		list_del(&rx_pkt->link);
		dma_unmap_single(ipa_ctx->pdev, rx_pkt->data.dma_addr,
			IPA_WLAN_COMM_RX_POOL_LOW, DMA_FROM_DEVICE);
		dev_kfree_skb_any(rx_pkt->data.skb);
		kmem_cache_free(ipa_ctx->rx_pkt_wrapper_cache, rx_pkt);
		ipa_ctx->wc_memb.wlan_comm_free_cnt--;
		ipa_ctx->wc_memb.wlan_comm_total_cnt--;
	}
	ipa_ctx->wc_memb.total_tx_pkts_freed = 0;

	if (ipa_ctx->wc_memb.wlan_comm_free_cnt != 0)
		IPAERR("wlan comm buff free cnt: %d\n",
			ipa_ctx->wc_memb.wlan_comm_free_cnt);

	if (ipa_ctx->wc_memb.wlan_comm_total_cnt != 0)
		IPAERR("wlan comm buff total cnt: %d\n",
			ipa_ctx->wc_memb.wlan_comm_total_cnt);

}

static void ipa_alloc_wlan_rx_common_cache(u32 size)
{
	void *ptr;
	struct ipa_rx_pkt_wrapper *rx_pkt;
	int rx_len_cached = 0;
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = ipa_ctx->wc_memb.wlan_comm_total_cnt;
	while (rx_len_cached < size) {
		rx_pkt = kmem_cache_zalloc(ipa_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt) {
			IPAERR("failed to alloc rx wrapper\n");
			goto fail_kmem_cache_alloc;
		}

		INIT_LIST_HEAD(&rx_pkt->link);
		INIT_WORK(&rx_pkt->work, ipa_wq_rx_avail);

		rx_pkt->data.skb =
			ipa_get_skb_ipa_rx(IPA_WLAN_RX_BUFF_SZ,
						flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, IPA_WLAN_RX_BUFF_SZ);
		rx_pkt->data.dma_addr = dma_map_single(NULL, ptr,
				IPA_WLAN_RX_BUFF_SZ, DMA_FROM_DEVICE);
		if (rx_pkt->data.dma_addr == 0 ||
				rx_pkt->data.dma_addr == ~0) {
			IPAERR("dma_map_single failure %p for %p\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		list_add_tail(&rx_pkt->link,
			&ipa_ctx->wc_memb.wlan_comm_desc_list);
		rx_len_cached = ++ipa_ctx->wc_memb.wlan_comm_total_cnt;

		ipa_ctx->wc_memb.wlan_comm_free_cnt++;

	}

	return;

fail_dma_mapping:
	dev_kfree_skb_any(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	return;
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
static void ipa_replenish_rx_cache(struct ipa_sys_context *sys)
{
	void *ptr;
	struct ipa_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = sys->len;

	while (rx_len_cached < sys->rx_pool_sz) {
		rx_pkt = kmem_cache_zalloc(ipa_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt) {
			IPAERR("failed to alloc rx wrapper\n");
			goto fail_kmem_cache_alloc;
		}

		INIT_LIST_HEAD(&rx_pkt->link);
		INIT_WORK(&rx_pkt->work, ipa_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (rx_pkt->data.dma_addr == 0 ||
				rx_pkt->data.dma_addr == ~0) {
			IPAERR("dma_map_single failure %p for %p\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		list_add_tail(&rx_pkt->link, &sys->head_desc_list);
		rx_len_cached = ++sys->len;

		ret = sps_transfer_one(sys->ep->ep_hdl,
			rx_pkt->data.dma_addr, sys->rx_buff_sz, rx_pkt, 0);

		if (ret) {
			IPAERR("sps_transfer_one failed %d\n", ret);
			goto fail_sps_transfer;
		}
	}

	return;

fail_sps_transfer:
	list_del(&rx_pkt->link);
	rx_len_cached = --sys->len;
	dma_unmap_single(ipa_ctx->pdev, rx_pkt->data.dma_addr,
			sys->rx_buff_sz, DMA_FROM_DEVICE);
fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	if (rx_len_cached == 0)
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(10));
}

static void ipa_fast_replenish_rx_cache(struct ipa_sys_context *sys)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	u32 curr;

	rx_len_cached = sys->len;
	curr = atomic_read(&sys->repl.head_idx);

	while (rx_len_cached < sys->rx_pool_sz) {
		if (curr == atomic_read(&sys->repl.tail_idx))
			break;

		rx_pkt = sys->repl.cache[curr];
		list_add_tail(&rx_pkt->link, &sys->head_desc_list);

		ret = sps_transfer_one(sys->ep->ep_hdl,
			rx_pkt->data.dma_addr, sys->rx_buff_sz, rx_pkt, 0);

		if (ret) {
			IPAERR("sps_transfer_one failed %d\n", ret);
			list_del(&rx_pkt->link);
			break;
		}
		rx_len_cached = ++sys->len;
		curr = (curr + 1) % sys->repl.capacity;
		mb();
		atomic_set(&sys->repl.head_idx, curr);
	}

	queue_work(sys->repl_wq, &sys->repl_work);

	if (rx_len_cached == 0) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS)
			IPA_STATS_INC_CNT(ipa_ctx->stats.wan_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)
			IPA_STATS_INC_CNT(ipa_ctx->stats.lan_rx_empty);
		else
			WARN_ON(1);
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
	}

	return;
}

static void replenish_rx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa_sys_context *sys;
	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa_sys_context, replenish_rx_work);
	sys->repl_hdlr(sys);
}

/**
 * ipa_cleanup_rx() - release RX queue resources
 *
 */
static void ipa_cleanup_rx(struct ipa_sys_context *sys)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;
	struct ipa_rx_pkt_wrapper *r;

	list_for_each_entry_safe(rx_pkt, r,
				 &sys->head_desc_list, link) {
		list_del(&rx_pkt->link);
		dma_unmap_single(ipa_ctx->pdev, rx_pkt->data.dma_addr,
			sys->rx_buff_sz, DMA_FROM_DEVICE);
		sys->free_skb(rx_pkt->data.skb);
		kmem_cache_free(ipa_ctx->rx_pkt_wrapper_cache, rx_pkt);
	}
}


static int ipa_lan_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa_sys_context *sys)
{
	int rc = 0;
	struct ipa_hw_pkt_status *status;
	struct sk_buff *skb2;
	int pad_len_byte;
	int len;
	unsigned char *buf;
	bool drop_packet;
	int src_pipe;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);

	if (skb->len == 0) {
		IPAERR("ZLT\n");
		sys->free_skb(skb);
		return rc;
	}

	if (sys->len_partial) {
		IPADBG("len_partial %d\n", sys->len_partial);
		buf = skb_push(skb, sys->len_partial);
		memcpy(buf, sys->prev_skb->data, sys->len_partial);
		sys->len_partial = 0;
		sys->free_skb(sys->prev_skb);
		goto begin;
	}

	/* this pipe has TX comp (status only) + mux-ed LAN RX data
	 * (status+data) */
	if (sys->len_rem) {
		IPADBG("rem %d skb %d pad %d\n", sys->len_rem, skb->len,
				sys->len_pad);
		if (sys->len_rem <= skb->len) {
			if (sys->prev_skb) {
				skb2 = skb_copy_expand(sys->prev_skb, 0,
						sys->len_rem, GFP_KERNEL);
				if (likely(skb2)) {
					memcpy(skb_put(skb2, sys->len_rem),
						skb->data, sys->len_rem);
					skb_trim(skb2,
						skb2->len - sys->len_pad);
					sys->ep->client_notify(sys->ep->priv,
						IPA_RECEIVE,
						(unsigned long)(skb2));
				} else {
					IPAERR("copy expand failed\n");
				}
				dev_kfree_skb_any(sys->prev_skb);
			}
			skb_pull(skb, sys->len_rem);
			sys->prev_skb = NULL;
			sys->len_rem = 0;
			sys->len_pad = 0;
		} else {
			if (sys->prev_skb) {
				skb2 = skb_copy_expand(sys->prev_skb, 0,
					skb->len, GFP_KERNEL);
				if (likely(skb2)) {
					memcpy(skb_put(skb2, skb->len),
						skb->data, skb->len);
				} else {
					IPAERR("copy expand failed\n");
				}
				dev_kfree_skb_any(sys->prev_skb);
				sys->prev_skb = skb2;
			}
			sys->len_rem -= skb->len;
			sys->free_skb(skb);
			return rc;
		}
	}

begin:
	while (skb->len) {
		drop_packet = false;
		IPADBG("LEN_REM %d\n", skb->len);

		if (skb->len < IPA_PKT_STATUS_SIZE) {
			WARN_ON(sys->prev_skb != NULL);
			IPADBG("status straddles buffer\n");
			sys->prev_skb = skb;
			sys->len_partial = skb->len;
			return rc;
		}

		status = (struct ipa_hw_pkt_status *)skb->data;
		IPADBG("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status->status_opcode, status->endp_src_idx,
				status->endp_dest_idx, status->pkt_len);
		if (status->status_opcode !=
			IPA_HW_STATUS_OPCODE_DROPPED_PACKET &&
			status->status_opcode !=
			IPA_HW_STATUS_OPCODE_PACKET &&
			status->status_opcode !=
			IPA_HW_STATUS_OPCODE_SUSPENDED_PACKET) {
			IPAERR("unsupported opcode(%d)\n",
				status->status_opcode);
			skb_pull(skb, IPA_PKT_STATUS_SIZE);
			continue;
		}
		IPA_STATS_EXCP_CNT(status->exception,
				ipa_ctx->stats.rx_excp_pkts);
		if (status->endp_dest_idx >= IPA_NUM_PIPES ||
			status->endp_src_idx >= IPA_NUM_PIPES ||
			status->pkt_len > IPA_GENERIC_AGGR_BYTE_LIMIT * 1024) {
			IPAERR("status fields invalid\n");
			WARN_ON(1);
			BUG();
		}
		if (status->status_mask & IPA_HW_PKT_STATUS_MASK_TAG_VALID) {
			struct ipa_tag_completion *comp;
			IPADBG("TAG packet arrived\n");
			if (status->tag_f_2 == IPA_COOKIE) {
				skb_pull(skb, IPA_PKT_STATUS_SIZE);
				if (skb->len < sizeof(comp)) {
					IPAERR("TAG arrived without packet\n");
					return rc;
				}
				memcpy(&comp, skb->data, sizeof(comp));
				skb_pull(skb, sizeof(comp) +
						IPA_SIZE_DL_CSUM_META_TRAILER);
				complete(&comp->comp);
				if (atomic_dec_return(&comp->cnt) == 0)
					kfree(comp);
				continue;
			} else {
				IPADBG("ignoring TAG with wrong cookie\n");
			}
		}
		if (status->pkt_len == 0) {
			IPADBG("Skip aggr close status\n");
			skb_pull(skb, IPA_PKT_STATUS_SIZE);
			IPA_STATS_INC_CNT(ipa_ctx->stats.aggr_close);
			IPA_STATS_DEC_CNT(
				ipa_ctx->stats.rx_excp_pkts[MAX_NUM_EXCP - 1]);
			continue;
		}
		if (status->endp_dest_idx == (sys->ep - ipa_ctx->ep)) {
			/* RX data */
			src_pipe = status->endp_src_idx;

			/*
			 * A packet which is received back to the AP after
			 * there was no route match.
			 */
			if (!status->exception && !status->route_match)
				drop_packet = true;

			if (skb->len == IPA_PKT_STATUS_SIZE &&
					!status->exception) {
				WARN_ON(sys->prev_skb != NULL);
				IPADBG("Ins header in next buffer\n");
				sys->prev_skb = skb;
				sys->len_partial =	 skb->len;
				return rc;
			}

			pad_len_byte = ((status->pkt_len + 3) & ~3) -
					status->pkt_len;

			len = status->pkt_len + pad_len_byte +
				IPA_SIZE_DL_CSUM_META_TRAILER;
			IPADBG("pad %d pkt_len %d len %d\n", pad_len_byte,
					status->pkt_len, len);

			if (status->exception ==
					IPA_HW_PKT_STATUS_EXCEPTION_DEAGGR) {
				IPADBG("Dropping packet on DeAggr Exception\n");
				skb_pull(skb, len + IPA_PKT_STATUS_SIZE);
				continue;
			}

			skb2 = skb_clone(skb, GFP_KERNEL);
			if (likely(skb2)) {
				if (skb->len < len + IPA_PKT_STATUS_SIZE) {
					IPADBG("SPL skb len %d len %d\n",
							skb->len, len);
					sys->prev_skb = skb2;
					sys->len_rem = len - skb->len +
						IPA_PKT_STATUS_SIZE;
					sys->len_pad = pad_len_byte;
					skb_pull(skb, skb->len);
				} else {
					skb_trim(skb2, status->pkt_len +
							IPA_PKT_STATUS_SIZE);
					IPADBG("rx avail for %d\n",
							status->endp_dest_idx);
					if (drop_packet)
						dev_kfree_skb_any(skb2);
					else {
						sys->ep->client_notify(
							sys->ep->priv,
							IPA_RECEIVE,
							(unsigned long)(skb2));
					}
					skb_pull(skb, len +
						IPA_PKT_STATUS_SIZE);
				}
			} else {
				IPAERR("fail to clone\n");
				if (skb->len < len) {
					sys->prev_skb = NULL;
					sys->len_rem = len - skb->len +
						IPA_PKT_STATUS_SIZE;
					sys->len_pad = pad_len_byte;
					skb_pull(skb, skb->len);
				} else {
					skb_pull(skb, len +
						IPA_PKT_STATUS_SIZE);
				}
			}
			/* TX comp */
			ipa_wq_write_done_status(src_pipe);
			IPADBG("tx comp imp for %d\n", src_pipe);
		} else {
			/* TX comp */
			ipa_wq_write_done_status(status->endp_src_idx);
			IPADBG("tx comp exp for %d\n", status->endp_src_idx);
			skb_pull(skb, IPA_PKT_STATUS_SIZE);
			IPA_STATS_INC_CNT(ipa_ctx->stats.stat_compl);
			IPA_STATS_DEC_CNT(
				ipa_ctx->stats.rx_excp_pkts[MAX_NUM_EXCP - 1]);
		}
	};

	sys->free_skb(skb);
	return rc;
}

static struct sk_buff *join_prev_skb(struct sk_buff *prev_skb,
		struct sk_buff *skb, unsigned int len)
{
	struct sk_buff *skb2;

	skb2 = skb_copy_expand(prev_skb, 0,
			len, GFP_KERNEL);
	if (likely(skb2)) {
		memcpy(skb_put(skb2, len),
			skb->data, len);
	} else {
		IPAERR("copy expand failed\n");
		skb2 = NULL;
	}
	dev_kfree_skb_any(prev_skb);

	return skb2;
}

static void wan_rx_handle_splt_pyld(struct sk_buff *skb,
		struct ipa_sys_context *sys)
{
	struct sk_buff *skb2;

	IPADBG("rem %d skb %d\n", sys->len_rem, skb->len);
	if (sys->len_rem <= skb->len) {
		if (sys->prev_skb) {
			skb2 = join_prev_skb(sys->prev_skb, skb,
					sys->len_rem);
			if (likely(skb2)) {
				IPADBG(
					"removing Status element from skb and sending to WAN client");
				skb_pull(skb2, IPA_PKT_STATUS_SIZE);
				sys->ep->client_notify(sys->ep->priv,
					IPA_RECEIVE,
					(unsigned long)(skb2));
			}
		}
		skb_pull(skb, sys->len_rem);
		sys->prev_skb = NULL;
		sys->len_rem = 0;
	} else {
		if (sys->prev_skb) {
			skb2 = join_prev_skb(sys->prev_skb, skb,
					skb->len);
			sys->prev_skb = skb2;
		}
		sys->len_rem -= skb->len;
		skb_pull(skb, skb->len);
	}
}

static int ipa_wan_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa_sys_context *sys)
{
	int rc = 0;
	struct ipa_hw_pkt_status *status;
	struct sk_buff *skb2;
	u16 pkt_len_with_pad;
	u32 qmap_hdr;
	int checksum_trailer_exists;
	int frame_len;
	int ep_idx;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);
	if (skb->len == 0) {
		IPAERR("ZLT\n");
		goto bail;
	}
	/*
	 * payload splits across 2 buff or more,
	 * take the start of the payload from prev_skb
	 */
	if (sys->len_rem)
		wan_rx_handle_splt_pyld(skb, sys);

	while (skb->len) {
		IPADBG("LEN_REM %d\n", skb->len);
		if (skb->len < IPA_PKT_STATUS_SIZE) {
			IPAERR("status straddles buffer\n");
			WARN_ON(1);
			goto bail;
		}
		status = (struct ipa_hw_pkt_status *)skb->data;
		IPADBG("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status->status_opcode, status->endp_src_idx,
				status->endp_dest_idx, status->pkt_len);
		if (status->status_opcode !=
			IPA_HW_STATUS_OPCODE_DROPPED_PACKET &&
			status->status_opcode !=
			IPA_HW_STATUS_OPCODE_PACKET) {
			IPAERR("unsupported opcode\n");
			skb_pull(skb, IPA_PKT_STATUS_SIZE);
			continue;
		}
		IPA_STATS_INC_CNT(ipa_ctx->stats.rx_pkts);
		if (status->endp_dest_idx >= IPA_NUM_PIPES ||
			status->endp_src_idx >= IPA_NUM_PIPES ||
			status->pkt_len > IPA_GENERIC_AGGR_BYTE_LIMIT * 1024) {
			IPAERR("status fields invalid\n");
			WARN_ON(1);
			goto bail;
		}
		if (status->pkt_len == 0) {
			IPADBG("Skip aggr close status\n");
			skb_pull(skb, IPA_PKT_STATUS_SIZE);
			IPA_STATS_DEC_CNT(ipa_ctx->stats.rx_pkts);
			IPA_STATS_INC_CNT(ipa_ctx->stats.wan_aggr_close);
			continue;
		}
		ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
		if (status->endp_dest_idx != ep_idx) {
			IPAERR("expected endp_dest_idx %d received %d\n",
					ep_idx, status->endp_dest_idx);
			WARN_ON(1);
			goto bail;
		}
		/* RX data */
		if (skb->len == IPA_PKT_STATUS_SIZE) {
			IPAERR("Ins header in next buffer\n");
			WARN_ON(1);
			goto bail;
		}
		qmap_hdr = *(u32 *)(status+1);
		/*
		 * Take the pkt_len_with_pad from the last 2 bytes of the QMAP
		 * header
		 */

		/*QMAP is BE: convert the pkt_len field from BE to LE*/
		pkt_len_with_pad = ntohs((qmap_hdr>>16) & 0xffff);
		IPADBG("pkt_len with pad %d\n", pkt_len_with_pad);
		/*get the CHECKSUM_PROCESS bit*/
		checksum_trailer_exists = status->status_mask &
				IPA_HW_PKT_STATUS_MASK_CKSUM_PROCESS;
		IPADBG("checksum_trailer_exists %d\n",
				checksum_trailer_exists);

		frame_len = IPA_PKT_STATUS_SIZE +
			    IPA_QMAP_HEADER_LENGTH +
			    pkt_len_with_pad;
		if (checksum_trailer_exists)
			frame_len += IPA_DL_CHECKSUM_LENGTH;
		IPADBG("frame_len %d\n", frame_len);

		skb2 = skb_clone(skb, GFP_KERNEL);
		if (likely(skb2)) {
			/*
			 * the len of actual data is smaller than expected
			 * payload split across 2 buff
			 */
			if (skb->len < frame_len) {
				IPADBG("SPL skb len %d len %d\n",
						skb->len, frame_len);
				sys->prev_skb = skb2;
				sys->len_rem = frame_len - skb->len;
				skb_pull(skb, skb->len);
			} else {
				skb_trim(skb2, frame_len);
				IPADBG("rx avail for %d\n",
						status->endp_dest_idx);
				IPADBG(
					"removing Status element from skb and sending to WAN client");
				skb_pull(skb2, IPA_PKT_STATUS_SIZE);
				sys->ep->client_notify(sys->ep->priv,
					IPA_RECEIVE, (unsigned long)(skb2));
				skb_pull(skb, frame_len);
			}
		} else {
			IPAERR("fail to clone\n");
			if (skb->len < frame_len) {
				sys->prev_skb = NULL;
				sys->len_rem = frame_len - skb->len;
				skb_pull(skb, skb->len);
			} else {
				skb_pull(skb, frame_len);
			}
		}
	};
bail:
	sys->free_skb(skb);
	return rc;
}

static int ipa_rx_pyld_hdlr(struct sk_buff *rx_skb, struct ipa_sys_context *sys)
{
	struct ipa_a5_mux_hdr *mux_hdr;
	unsigned int pull_len;
	unsigned int padding;
	struct ipa_ep_context *ep;
	unsigned int src_pipe;

	mux_hdr = (struct ipa_a5_mux_hdr *)rx_skb->data;

	src_pipe = mux_hdr->src_pipe_index;

	IPADBG("RX pkt len=%d IID=0x%x src=%d, flags=0x%x, meta=0x%x\n",
		rx_skb->len, ntohs(mux_hdr->interface_id),
		src_pipe, mux_hdr->flags, ntohl(mux_hdr->metadata));

	IPA_DUMP_BUFF(rx_skb->data, 0, rx_skb->len);

	IPA_STATS_INC_CNT(ipa_ctx->stats.rx_pkts);
	IPA_STATS_EXCP_CNT(mux_hdr->flags, ipa_ctx->stats.rx_excp_pkts);

	/*
	 * Any packets arriving over AMPDU_TX should be dispatched
	 * to the regular WLAN RX data-path.
	 */
	if (unlikely(src_pipe == WLAN_AMPDU_TX_EP))
		src_pipe = WLAN_PROD_TX_EP;

	ep = &ipa_ctx->ep[src_pipe];
	if (unlikely(src_pipe >= IPA_NUM_PIPES ||
		!ep->valid || !ep->client_notify)) {
		IPAERR("drop pipe=%d ep_valid=%d client_notify=%p\n",
		  src_pipe, ep->valid, ep->client_notify);
		dev_kfree_skb_any(rx_skb);
		return 0;
	}

	pull_len = sizeof(struct ipa_a5_mux_hdr);

	/*
	 * IP packet starts on word boundary
	 * remove the MUX header and any padding and pass the frame to
	 * the client which registered a rx callback on the "src pipe"
	 */
	padding = ep->cfg.hdr.hdr_len & 0x3;
	if (padding)
		pull_len += 4 - padding;

	IPADBG("pulling %d bytes from skb\n", pull_len);
	skb_pull(rx_skb, pull_len);
	ep->client_notify(ep->priv, IPA_RECEIVE,
			(unsigned long)(rx_skb));
	return 0;
}

static struct sk_buff *ipa_get_skb_ipa_rx(unsigned int len, gfp_t flags)
{
	return __dev_alloc_skb(len, flags);
}

static void ipa_free_skb_rx(struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
}

void ipa_lan_rx_cb(void *priv, enum ipa_dp_evt_type evt, unsigned long data)
{
	struct sk_buff *rx_skb = (struct sk_buff *)data;
	struct ipa_hw_pkt_status *status;
	struct ipa_ep_context *ep;
	unsigned int src_pipe;
	u32 metadata;

	status = (struct ipa_hw_pkt_status *)rx_skb->data;
	src_pipe = status->endp_src_idx;
	metadata = status->metadata;
	ep = &ipa_ctx->ep[src_pipe];
	if (unlikely(src_pipe >= IPA_NUM_PIPES ||
		!ep->valid ||
		!ep->client_notify)) {
		IPAERR("drop pipe=%d ep_valid=%d client_notify=%p\n",
		  src_pipe, ep->valid, ep->client_notify);
		dev_kfree_skb_any(rx_skb);
		return;
	}
	if (!status->exception)
		skb_pull(rx_skb, IPA_PKT_STATUS_SIZE +
				IPA_LAN_RX_HEADER_LENGTH);
	else
		skb_pull(rx_skb, IPA_PKT_STATUS_SIZE);
	*(u8 *)rx_skb->cb = (metadata >> 16) & 0xFF;
	ep->client_notify(ep->priv, IPA_RECEIVE, (unsigned long)(rx_skb));
}

static void ipa_wq_rx_common(struct ipa_sys_context *sys, u32 size)
{
	struct ipa_rx_pkt_wrapper *rx_pkt_expected;
	struct sk_buff *rx_skb;

	if (unlikely(list_empty(&sys->head_desc_list))) {
		WARN_ON(1);
		return;
	}
	rx_pkt_expected = list_first_entry(&sys->head_desc_list,
					   struct ipa_rx_pkt_wrapper,
					   link);
	list_del(&rx_pkt_expected->link);
	sys->len--;
	if (size)
		rx_pkt_expected->len = size;
	rx_skb = rx_pkt_expected->data.skb;
	dma_unmap_single(ipa_ctx->pdev, rx_pkt_expected->data.dma_addr,
			sys->rx_buff_sz, DMA_FROM_DEVICE);
	skb_set_tail_pointer(rx_skb, rx_pkt_expected->len);
	rx_skb->len = rx_pkt_expected->len;
	rx_skb->truesize = rx_pkt_expected->len + sizeof(struct sk_buff);
	sys->pyld_hdlr(rx_skb, sys);
	sys->repl_hdlr(sys);
	kmem_cache_free(ipa_ctx->rx_pkt_wrapper_cache, rx_pkt_expected);

}

static void ipa_wlan_wq_rx_common(struct ipa_sys_context *sys, u32 size)
{
	struct ipa_rx_pkt_wrapper *rx_pkt_expected;
	struct sk_buff *rx_skb;

	if (unlikely(list_empty(&sys->head_desc_list))) {
		WARN_ON(1);
		return;
	}
	rx_pkt_expected = list_first_entry(&sys->head_desc_list,
					   struct ipa_rx_pkt_wrapper,
					   link);
	list_del(&rx_pkt_expected->link);
	sys->len--;

	if (size)
		rx_pkt_expected->len = size;

	rx_skb = rx_pkt_expected->data.skb;
	skb_set_tail_pointer(rx_skb, rx_pkt_expected->len);
	rx_skb->len = rx_pkt_expected->len;
	rx_skb->truesize = rx_pkt_expected->len + sizeof(struct sk_buff);
	sys->ep->wstats.tx_pkts_rcvd++;
	if (sys->len <= IPA_WLAN_RX_POOL_SZ_LOW_WM) {
		ipa_free_skb(&rx_pkt_expected->data);
		sys->ep->wstats.tx_pkts_dropped++;
	} else {
		sys->ep->wstats.tx_pkts_sent++;
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
				(unsigned long)(&rx_pkt_expected->data));
	}
	ipa_replenish_wlan_rx_cache(sys);
}

static void ipa_wq_rx_avail(struct work_struct *work)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;
	struct ipa_sys_context *sys;

	rx_pkt = container_of(work, struct ipa_rx_pkt_wrapper, work);
	if (unlikely(rx_pkt == NULL))
		WARN_ON(1);
	sys = rx_pkt->sys;
	ipa_wq_rx_common(sys, 0);
}

/**
 * ipa_sps_irq_rx_no_aggr_notify() - Callback function which will be called by
 * the SPS driver after a Rx operation is complete.
 * Called in an interrupt context.
 * @notify:	SPS driver supplied notification struct
 *
 * This function defer the work for this event to a workqueue.
 */
void ipa_sps_irq_rx_no_aggr_notify(struct sps_event_notify *notify)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		rx_pkt = notify->data.transfer.user;
		rx_pkt->len = notify->data.transfer.iovec.size;
		IPADBG("event %d notified sys=%p len=%u\n", notify->event_id,
				notify->user, rx_pkt->len);
		queue_work(rx_pkt->sys->wq, &rx_pkt->work);
		break;
	default:
		IPAERR("recieved unexpected event id %d sys=%p\n",
				notify->event_id, notify->user);
	}
}

static int ipa_odu_rx_pyld_hdlr(struct sk_buff *rx_skb,
	struct ipa_sys_context *sys)
{
	if (sys->ep->client_notify) {
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
			(unsigned long)(rx_skb));
	} else {
		dev_kfree_skb_any(rx_skb);
		WARN_ON(1);
	}

	return 0;
}

static int ipa_assign_policy(struct ipa_sys_connect_params *in,
		struct ipa_sys_context *sys)
{
	if (in->client == IPA_CLIENT_APPS_CMD_PROD) {
		sys->policy = IPA_POLICY_INTR_MODE;
		sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT);
		sys->sps_callback = ipa_sps_irq_tx_no_aggr_notify;
		return 0;
	}

	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_1) {
		if (in->client == IPA_CLIENT_APPS_LAN_WAN_PROD) {
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT |
					SPS_O_ACK_TRANSFERS);
			sys->sps_callback = ipa_sps_irq_tx_notify;
			INIT_WORK(&sys->work, ipa_wq_handle_tx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				switch_to_intr_tx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
		} else if (in->client == IPA_CLIENT_APPS_LAN_CONS) {
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT |
					SPS_O_ACK_TRANSFERS);
			sys->sps_callback = ipa_sps_irq_rx_notify;
			INIT_WORK(&sys->work, ipa_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
					replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz = IPA_RX_SKB_SIZE;
			sys->rx_pool_sz = IPA_RX_POOL_CEIL;
			sys->pyld_hdlr = ipa_rx_pyld_hdlr;
			sys->get_skb = ipa_get_skb_ipa_rx;
			sys->free_skb = ipa_free_skb_rx;
			sys->repl_hdlr = ipa_replenish_rx_cache;
		} else if (IPA_CLIENT_IS_PROD(in->client)) {
			sys->policy = IPA_POLICY_INTR_MODE;
			sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT);
			sys->sps_callback = ipa_sps_irq_tx_no_aggr_notify;
		} else {
			IPAERR("Need to install a RX pipe hdlr\n");
			WARN_ON(1);
			return -EINVAL;
		}
	} else if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
			ipa_ctx->ipa_hw_type == IPA_HW_v2_5) {
		sys->ep->status.status_en = true;
		if (IPA_CLIENT_IS_PROD(in->client)) {
			if (!sys->ep->skip_ep_cfg) {
				sys->policy = IPA_POLICY_NOINTR_MODE;
				sys->sps_option = SPS_O_AUTO_ENABLE;
				sys->sps_callback = NULL;
				sys->ep->status.status_ep = ipa_get_ep_mapping(
						IPA_CLIENT_APPS_LAN_CONS);
			} else {
				sys->policy = IPA_POLICY_INTR_MODE;
				sys->sps_option = (SPS_O_AUTO_ENABLE |
						SPS_O_EOT);
				sys->sps_callback =
					ipa_sps_irq_tx_no_aggr_notify;
			}
		} else {
			if (in->client == IPA_CLIENT_APPS_LAN_CONS ||
			    in->client == IPA_CLIENT_APPS_WAN_CONS) {
				sys->policy = IPA_POLICY_INTR_POLL_MODE;
				sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT
						| SPS_O_ACK_TRANSFERS);
				sys->sps_callback = ipa_sps_irq_rx_notify;
				INIT_WORK(&sys->work, ipa_wq_handle_rx);
				INIT_DELAYED_WORK(&sys->switch_to_intr_work,
					switch_to_intr_rx_work_func);
				INIT_DELAYED_WORK(&sys->replenish_rx_work,
						replenish_rx_work_func);
				INIT_WORK(&sys->repl_work, ipa_wq_repl_rx);
				atomic_set(&sys->curr_polling_state, 0);
				sys->rx_buff_sz = IPA_LAN_RX_BUFF_SZ;
				sys->rx_pool_sz = IPA_GENERIC_RX_POOL_SZ;
				sys->get_skb = ipa_get_skb_ipa_rx;
				sys->free_skb = ipa_free_skb_rx;
				in->ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
				in->ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
				in->ipa_ep_cfg.aggr.aggr_byte_limit =
					IPA_GENERIC_AGGR_BYTE_LIMIT;
				in->ipa_ep_cfg.aggr.aggr_time_limit =
					IPA_GENERIC_AGGR_TIME_LIMIT;
				in->ipa_ep_cfg.aggr.aggr_pkt_limit =
					IPA_GENERIC_AGGR_PKT_LIMIT;
				if (in->client == IPA_CLIENT_APPS_LAN_CONS) {
					sys->pyld_hdlr = ipa_lan_rx_pyld_hdlr;
				} else if (in->client ==
						IPA_CLIENT_APPS_WAN_CONS) {
					sys->pyld_hdlr = ipa_wan_rx_pyld_hdlr;
				}
				if (nr_cpu_ids > 1)
					sys->repl_hdlr =
						ipa_fast_replenish_rx_cache;
				else
					sys->repl_hdlr = ipa_replenish_rx_cache;
			} else if (IPA_CLIENT_IS_WLAN_CONS(in->client)) {
				IPADBG("assigning policy to client:%d",
					in->client);

				sys->ep->status.status_en = false;
				sys->policy = IPA_POLICY_INTR_POLL_MODE;
				sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT
						| SPS_O_ACK_TRANSFERS);
				sys->sps_callback = ipa_sps_irq_rx_notify;
				INIT_WORK(&sys->work, ipa_wq_handle_rx);
				INIT_DELAYED_WORK(&sys->switch_to_intr_work,
					switch_to_intr_rx_work_func);
				INIT_DELAYED_WORK(&sys->replenish_rx_work,
					replenish_rx_work_func);
				atomic_set(&sys->curr_polling_state, 0);
				sys->rx_buff_sz = IPA_WLAN_RX_BUFF_SZ;
				sys->rx_pool_sz = in->desc_fifo_sz/
					sizeof(struct sps_iovec) - 1;
				if (sys->rx_pool_sz > IPA_WLAN_RX_POOL_SZ)
					sys->rx_pool_sz = IPA_WLAN_RX_POOL_SZ;
				sys->pyld_hdlr = NULL;
				sys->get_skb = ipa_get_skb_ipa_rx;
				sys->free_skb = ipa_free_skb_rx;
				in->ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
			} else if (IPA_CLIENT_IS_ODU_CONS(in->client)) {
				IPADBG("assigning policy to client:%d",
					in->client);

				sys->ep->status.status_en = false;
				sys->policy = IPA_POLICY_INTR_POLL_MODE;
				sys->sps_option = (SPS_O_AUTO_ENABLE | SPS_O_EOT
					| SPS_O_ACK_TRANSFERS);
				sys->sps_callback = ipa_sps_irq_rx_notify;
				INIT_WORK(&sys->work, ipa_wq_handle_rx);
				INIT_DELAYED_WORK(&sys->switch_to_intr_work,
					switch_to_intr_rx_work_func);
				INIT_DELAYED_WORK(&sys->replenish_rx_work,
					replenish_rx_work_func);
				atomic_set(&sys->curr_polling_state, 0);
				sys->rx_buff_sz = IPA_ODU_RX_BUFF_SZ;
				sys->rx_pool_sz = IPA_ODU_RX_POOL_SZ;
				sys->pyld_hdlr = ipa_odu_rx_pyld_hdlr;
				sys->get_skb = ipa_get_skb_ipa_rx;
				sys->free_skb = ipa_free_skb_rx;
				sys->repl_hdlr = ipa_replenish_rx_cache;
			} else {
				IPAERR("Need to install a RX pipe hdlr\n");
				WARN_ON(1);
				return -EINVAL;
			}
		}
	} else {
		IPAERR("Unsupported HW type %d\n", ipa_ctx->ipa_hw_type);
		WARN_ON(1);
		return -EINVAL;

	}

	return 0;
}

/**
 * ipa_tx_client_rx_notify_release() - Callback function
 * which will call the user supplied callback function to
 * release the skb, or release it on its own if no callback
 * function was supplied
 *
 * @user1: [in] - Data Descriptor
 * @user2: [in] - endpoint idx
 *
 * This notified callback is for the destination client
 * This function is supplied in ipa_tx_dp_mul
 */
static void ipa_tx_client_rx_notify_release(void *user1, int user2)
{
	struct ipa_tx_data_desc *dd = (struct ipa_tx_data_desc *)user1;
	int ep_idx = user2;

	IPADBG("Received data desc anchor:%p\n", dd);

	atomic_inc(&ipa_ctx->ep[ep_idx].avail_fifo_desc);
	ipa_ctx->ep[ep_idx].wstats.rx_pkts_status_rcvd++;

  /* wlan host driver waits till tx complete before unload */
	IPADBG("ep=%d fifo_desc_free_count=%d\n",
		ep_idx, atomic_read(&ipa_ctx->ep[ep_idx].avail_fifo_desc));
	IPADBG("calling client notify callback with priv:%p\n",
		ipa_ctx->ep[ep_idx].priv);

	if (ipa_ctx->ep[ep_idx].client_notify) {
		ipa_ctx->ep[ep_idx].client_notify(ipa_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)user1);
		ipa_ctx->ep[ep_idx].wstats.rx_hd_reply++;
	}
}
/**
 * ipa_tx_client_rx_pkt_status() - Callback function
 * which will call the user supplied callback function to
 * increase the available fifo descriptor
 *
 * @user1: [in] - Data Descriptor
 * @user2: [in] - endpoint idx
 *
 * This notified callback is for the destination client
 * This function is supplied in ipa_tx_dp_mul
 */
static void ipa_tx_client_rx_pkt_status(void *user1, int user2)
{
	int ep_idx = user2;

	atomic_inc(&ipa_ctx->ep[ep_idx].avail_fifo_desc);
	ipa_ctx->ep[ep_idx].wstats.rx_pkts_status_rcvd++;
}


/**
 * ipa_tx_dp_mul() - Data-path tx handler for multiple packets
 * @src: [in] - Client that is sending data
 * @ipa_tx_data_desc:	[in] data descriptors from wlan
 *
 * this is used for to transfer data descriptors that received
 * from WLAN1_PROD pipe to IPA HW
 *
 * The function will send data descriptors from WLAN1_PROD (one
 * at a time) using sps_transfer_one. Will set EOT flag for last
 * descriptor Once this send was done from SPS point-of-view the
 * IPA driver will get notified by the supplied callback -
 * ipa_sps_irq_tx_no_aggr_notify()
 *
 * ipa_sps_irq_tx_no_aggr_notify will call to the user supplied
 * callback (from ipa_connect)
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_tx_dp_mul(enum ipa_client_type src,
			struct ipa_tx_data_desc *data_desc)
{
	/* The second byte in wlan header holds qmap id */
#define IPA_WLAN_HDR_QMAP_ID_OFFSET 1
	struct ipa_tx_data_desc *entry;
	struct ipa_sys_context *sys;
	struct ipa_desc desc = { 0 };
	u32 num_desc, cnt;
	int ep_idx;

	IPADBG("Received data desc anchor:%p\n", data_desc);

	spin_lock_bh(&ipa_ctx->wc_memb.ipa_tx_mul_spinlock);

	ep_idx = ipa_get_ep_mapping(src);
	if (unlikely(ep_idx == -1)) {
		IPAERR("dest EP does not exist.\n");
		goto fail_send;
	}
	IPADBG("ep idx:%d\n", ep_idx);
	sys = ipa_ctx->ep[ep_idx].sys;

	if (unlikely(ipa_ctx->ep[ep_idx].valid == 0)) {
		IPAERR("dest EP not valid.\n");
		goto fail_send;
	}
	sys->ep->wstats.rx_hd_rcvd++;

	/* Calculate the number of descriptors */
	num_desc = 0;
	list_for_each_entry(entry, &data_desc->link, link) {
		num_desc++;
	}
	IPADBG("Number of Data Descriptors:%d", num_desc);

	if (atomic_read(&sys->ep->avail_fifo_desc) < num_desc) {
		IPAERR("Insufficient data descriptors available\n");
		goto fail_send;
	}

	/* Assign callback only for last data descriptor */
	cnt = 0;
	list_for_each_entry(entry, &data_desc->link, link) {
		IPADBG("Parsing data desc :%d\n", cnt);
		cnt++;
		((u8 *)entry->pyld_buffer)[IPA_WLAN_HDR_QMAP_ID_OFFSET] =
			(u8)sys->ep->cfg.meta.qmap_id;
		desc.pyld = entry->pyld_buffer;
		desc.len = entry->pyld_len;
		desc.type = IPA_DATA_DESC_SKB;
		desc.user1 = data_desc;
		desc.user2 = ep_idx;
		IPADBG("priv:%p pyld_buf:0x%p pyld_len:%d\n",
			entry->priv, desc.pyld, desc.len);

		/* In case of last descriptor populate callback */
		if (cnt == num_desc) {
			IPADBG("data desc:%p\n", data_desc);
			desc.callback = ipa_tx_client_rx_notify_release;
		} else {
			desc.callback = ipa_tx_client_rx_pkt_status;
		}

		IPADBG("calling ipa_send_one()\n");
		if (ipa_send_one(sys, &desc, true)) {
			IPAERR("fail to send skb\n");
			sys->ep->wstats.rx_pkt_leak += (cnt-1);
			sys->ep->wstats.rx_dp_fail++;
			goto fail_send;
		}

		if (atomic_read(&sys->ep->avail_fifo_desc) >= 0)
			atomic_dec(&sys->ep->avail_fifo_desc);

		sys->ep->wstats.rx_pkts_rcvd++;
		IPADBG("ep=%d fifo desc=%d\n",
			ep_idx, atomic_read(&sys->ep->avail_fifo_desc));
	}

	sys->ep->wstats.rx_hd_processed++;
	spin_unlock_bh(&ipa_ctx->wc_memb.ipa_tx_mul_spinlock);
	return 0;

fail_send:
	spin_unlock_bh(&ipa_ctx->wc_memb.ipa_tx_mul_spinlock);
	return -EFAULT;

}
EXPORT_SYMBOL(ipa_tx_dp_mul);

void ipa_free_skb(struct ipa_rx_data *data)
{
	struct ipa_rx_pkt_wrapper *rx_pkt;

	spin_lock_bh(&ipa_ctx->wc_memb.wlan_spinlock);

	ipa_ctx->wc_memb.total_tx_pkts_freed++;
	rx_pkt = container_of(data, struct ipa_rx_pkt_wrapper, data);

	ipa_skb_recycle(rx_pkt->data.skb);
	(void)skb_put(rx_pkt->data.skb, IPA_WLAN_RX_BUFF_SZ);

	list_add_tail(&rx_pkt->link,
		&ipa_ctx->wc_memb.wlan_comm_desc_list);
	ipa_ctx->wc_memb.wlan_comm_free_cnt++;

	spin_unlock_bh(&ipa_ctx->wc_memb.wlan_spinlock);
}
EXPORT_SYMBOL(ipa_free_skb);

/* Functions added to support kernel tests */

int ipa_sys_setup(struct ipa_sys_connect_params *sys_in,
			unsigned long *ipa_bam_hdl,
			u32 *ipa_pipe_num, u32 *clnt_hdl)
{
	struct ipa_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;

	if (sys_in == NULL || clnt_hdl == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}

	if (ipa_bam_hdl == NULL || ipa_pipe_num == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}
	if (sys_in->client >= IPA_CLIENT_MAX) {
		IPAERR("bad parm client:%d\n", sys_in->client);
		goto fail_gen;
	}

	ipa_ep_idx = ipa_get_ep_mapping(sys_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client :%d\n", sys_in->client);
		goto fail_gen;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	ipa_inc_client_enable_clks();

	if (ep->valid == 1) {
		if (sys_in->client != IPA_CLIENT_APPS_LAN_WAN_PROD) {
			IPAERR("EP %d already allocated\n", ipa_ep_idx);
			goto fail_and_disable_clocks;
		} else {
			if (ipa_cfg_ep_hdr(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.hdr)) {
				IPAERR("fail to configure hdr prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			if (ipa_cfg_ep_cfg(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.cfg)) {
				IPAERR("fail to configure cfg prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			IPAERR("client %d (ep: %d) overlay ok sys=%p\n",
					sys_in->client, ipa_ep_idx, ep->sys);
			ep->client_notify = sys_in->notify;
			ep->priv = sys_in->priv;
			*clnt_hdl = ipa_ep_idx;
			if (!ep->keep_ipa_awake)
				ipa_dec_client_disable_clks();

			return 0;
		}
	}

	memset(ep, 0, offsetof(struct ipa_ep_context, sys));

	ep->valid = 1;
	ep->client = sys_in->client;
	ep->client_notify = sys_in->notify;
	ep->priv = sys_in->priv;
	ep->keep_ipa_awake = true;

	result = ipa_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n",
				 result, ipa_ep_idx);
		goto fail_gen2;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_gen2;
		}
		if (ipa_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_gen2;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	*ipa_pipe_num = ipa_ep_idx;
	*ipa_bam_hdl = ipa_ctx->bam_handle;

	if (!ep->keep_ipa_awake)
		ipa_dec_client_disable_clks();

	ipa_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPADBG("client %d (ep: %d) connected sys=%p\n", sys_in->client,
			ipa_ep_idx, ep->sys);

	return 0;

fail_gen2:
fail_and_disable_clocks:
	ipa_dec_client_disable_clks();
fail_gen:
	return result;
}
EXPORT_SYMBOL(ipa_sys_setup);

int ipa_sys_teardown(u32 clnt_hdl)
{
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm(Either endpoint or client hdl invalid)\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		ipa_inc_client_enable_clks();

	ipa_disable_data_path(clnt_hdl);
	ep->valid = 0;

	ipa_dec_client_disable_clks();

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}
EXPORT_SYMBOL(ipa_sys_teardown);
