// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/msm_gsi.h>
#include <net/sock.h>
#include "ipa_i.h"
#include "ipa_trace.h"
#include "ipahal/ipahal.h"
#include "ipahal/ipahal_fltrt.h"

#define IPA_WAN_AGGR_PKT_CNT 1
#define IPA_WAN_NAPI_MAX_FRAMES (NAPI_WEIGHT / IPA_WAN_AGGR_PKT_CNT)
#define IPA_WAN_PAGE_ORDER 3
#define IPA_LAN_AGGR_PKT_CNT 5
#define IPA_LAN_NAPI_MAX_FRAMES (NAPI_WEIGHT / IPA_LAN_AGGR_PKT_CNT)
#define IPA_LAST_DESC_CNT 0xFFFF
#define POLLING_INACTIVITY_RX 40
#define POLLING_MIN_SLEEP_RX 1010
#define POLLING_MAX_SLEEP_RX 1050
#define POLLING_INACTIVITY_TX 40
#define POLLING_MIN_SLEEP_TX 400
#define POLLING_MAX_SLEEP_TX 500
#define SUSPEND_MIN_SLEEP_RX 1000
#define SUSPEND_MAX_SLEEP_RX 1005
/* 8K less 1 nominal MTU (1500 bytes) rounded to units of KB */
#define IPA_MTU 1500
#define IPA_GENERIC_AGGR_BYTE_LIMIT 6
#define IPA_GENERIC_AGGR_TIME_LIMIT 500 /* 0.5msec */
#define IPA_GENERIC_AGGR_PKT_LIMIT 0

#define IPA_GSB_AGGR_BYTE_LIMIT 14
#define IPA_GSB_RX_BUFF_BASE_SZ 16384

#define IPA_GENERIC_RX_BUFF_BASE_SZ 8192
#define IPA_REAL_GENERIC_RX_BUFF_SZ(X) (SKB_DATA_ALIGN(\
		(X) + NET_SKB_PAD) +\
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))
#define IPA_GENERIC_RX_BUFF_SZ(X) ((X) -\
		(IPA_REAL_GENERIC_RX_BUFF_SZ(X) - (X)))
#define IPA_GENERIC_RX_BUFF_LIMIT (\
		IPA_REAL_GENERIC_RX_BUFF_SZ(\
		IPA_GENERIC_RX_BUFF_BASE_SZ) -\
		IPA_GENERIC_RX_BUFF_BASE_SZ)

/* less 1 nominal MTU (1500 bytes) rounded to units of KB */
#define IPA_ADJUST_AGGR_BYTE_LIMIT(X) (((X) - IPA_MTU)/1000)

#define IPA_RX_BUFF_CLIENT_HEADROOM 256

#define IPA_WLAN_RX_POOL_SZ 100
#define IPA_WLAN_RX_POOL_SZ_LOW_WM 5
#define IPA_WLAN_RX_BUFF_SZ 2048
#define IPA_WLAN_COMM_RX_POOL_LOW 100
#define IPA_WLAN_COMM_RX_POOL_HIGH 900

#define IPA_ODU_RX_BUFF_SZ 2048
#define IPA_ODU_RX_POOL_SZ 64

#define IPA_ODL_RX_BUFF_SZ (16 * 1024)

#define IPA_GSI_MAX_CH_LOW_WEIGHT 15
#define IPA_GSI_EVT_RING_INT_MODT (16) /* 0.5ms under 32KHz clock */
#define IPA_GSI_EVT_RING_INT_MODC (20)

#define IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC 10
/* The below virtual channel cannot be used by any entity */
#define IPA_GSI_CH_20_WA_VIRT_CHAN 29

#define IPA_DEFAULT_SYS_YELLOW_WM 32
#define IPA_REPL_XFER_THRESH 20
#define IPA_REPL_XFER_MAX 36

#define IPA_TX_SEND_COMPL_NOP_DELAY_NS (2 * 1000 * 1000)

#define IPA_APPS_BW_FOR_PM 700

#define IPA_SEND_MAX_DESC (20)

#define IPA_EOT_THRESH 32

#define IPA_QMAP_ID_BYTE 0

#define IPA_TX_MAX_DESC (50)

static struct sk_buff *ipa3_get_skb_ipa_rx(unsigned int len, gfp_t flags);
static void ipa3_replenish_wlan_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_replenish_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_replenish_rx_work_func(struct work_struct *work);
static void ipa3_fast_replenish_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_replenish_rx_page_cache(struct ipa3_sys_context *sys);
static void ipa3_wq_page_repl(struct work_struct *work);
static void ipa3_replenish_rx_page_recycle(struct ipa3_sys_context *sys);
static struct ipa3_rx_pkt_wrapper *ipa3_alloc_rx_pkt_page(gfp_t flag,
	bool is_tmp_alloc);
static void ipa3_wq_handle_rx(struct work_struct *work);
static void ipa3_wq_rx_common(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify);
static void ipa3_rx_napi_chain(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify, uint32_t num);
static void ipa3_wlan_wq_rx_common(struct ipa3_sys_context *sys,
				struct gsi_chan_xfer_notify *notify);
static int ipa3_assign_policy(struct ipa_sys_connect_params *in,
		struct ipa3_sys_context *sys);
static void ipa3_cleanup_rx(struct ipa3_sys_context *sys);
static void ipa3_wq_rx_avail(struct work_struct *work);
static void ipa3_alloc_wlan_rx_common_cache(u32 size);
static void ipa3_cleanup_wlan_rx_common_cache(void);
static void ipa3_wq_repl_rx(struct work_struct *work);
static void ipa3_dma_memcpy_notify(struct ipa3_sys_context *sys);
static int ipa_gsi_setup_coal_def_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep, struct ipa3_ep_context *coal_ep);
static int ipa_gsi_setup_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep);
static int ipa_gsi_setup_event_ring(struct ipa3_ep_context *ep,
	u32 ring_size, gfp_t mem_flag);
static int ipa_gsi_setup_transfer_ring(struct ipa3_ep_context *ep,
	u32 ring_size, struct ipa3_sys_context *user_data, gfp_t mem_flag);
static int ipa3_teardown_coal_def_pipe(u32 clnt_hdl);
static int ipa_populate_tag_field(struct ipa3_desc *desc,
		struct ipa3_tx_pkt_wrapper *tx_pkt,
		struct ipahal_imm_cmd_pyld **tag_pyld_ret);
static int ipa_poll_gsi_pkt(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify);
static int ipa_poll_gsi_n_pkt(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify, int expected_num,
	int *actual_num);
static unsigned long tag_to_pointer_wa(uint64_t tag);
static uint64_t pointer_to_tag_wa(struct ipa3_tx_pkt_wrapper *tx_pkt);

static u32 ipa_adjust_ra_buff_base_sz(u32 aggr_byte_limit);

static void ipa3_wq_write_done_common(struct ipa3_sys_context *sys,
				struct ipa3_tx_pkt_wrapper *tx_pkt)
{
	struct ipa3_tx_pkt_wrapper *next_pkt;
	int i, cnt;

	if (unlikely(tx_pkt == NULL)) {
		IPAERR("tx_pkt is NULL\n");
		return;
	}

	cnt = tx_pkt->cnt;
	IPADBG_LOW("cnt: %d\n", cnt);
	for (i = 0; i < cnt; i++) {
		spin_lock_bh(&sys->spinlock);
		if (unlikely(list_empty(&sys->head_desc_list))) {
			spin_unlock_bh(&sys->spinlock);
			return;
		}
		next_pkt = list_next_entry(tx_pkt, link);
		list_del(&tx_pkt->link);
		sys->len--;
		spin_unlock_bh(&sys->spinlock);
		if (!tx_pkt->no_unmap_dma) {
			if (tx_pkt->type != IPA_DATA_DESC_SKB_PAGED) {
				dma_unmap_single(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			} else {
				dma_unmap_page(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			}
		}
		if (tx_pkt->callback)
			tx_pkt->callback(tx_pkt->user1, tx_pkt->user2);

		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);
		tx_pkt = next_pkt;
	}
}

static void ipa3_wq_write_done_status(int src_pipe,
			struct ipa3_tx_pkt_wrapper *tx_pkt)
{
	struct ipa3_sys_context *sys;

	WARN_ON(src_pipe >= ipa3_ctx->ipa_num_pipes);

	if (!ipa3_ctx->ep[src_pipe].status.status_en)
		return;

	sys = ipa3_ctx->ep[src_pipe].sys;
	if (!sys)
		return;

	ipa3_wq_write_done_common(sys, tx_pkt);
}

static void ipa3_tasklet_schd_work(struct work_struct *work)
{
	struct ipa3_sys_context *sys;

	sys = container_of(work, struct ipa3_sys_context, tasklet_work);
	if (atomic_read(&sys->xmit_eot_cnt))
		tasklet_schedule(&sys->tasklet);
}

/**
 * ipa_write_done() - this function will be (eventually) called when a Tx
 * operation is complete
 * @data: user pointer point to the ipa3_sys_context
 *
 * Will be called in deferred context.
 * - invoke the callback supplied by the client who sent this command
 * - iterate over all packets and validate that
 *   the order for sent packet is the same as expected
 * - delete all the tx packet descriptors from the system
 *   pipe context (not needed anymore)
 */
static void ipa3_tasklet_write_done(unsigned long data)
{
	struct ipa3_sys_context *sys;
	struct ipa3_tx_pkt_wrapper *this_pkt;
	bool xmit_done = false;
	unsigned int max_tx_pkt = 0;

	sys = (struct ipa3_sys_context *)data;
	spin_lock_bh(&sys->spinlock);
	while (atomic_add_unless(&sys->xmit_eot_cnt, -1, 0)) {
		while (!list_empty(&sys->head_desc_list)) {
			this_pkt = list_first_entry(&sys->head_desc_list,
				struct ipa3_tx_pkt_wrapper, link);
			xmit_done = this_pkt->xmit_done;
			spin_unlock_bh(&sys->spinlock);
			ipa3_wq_write_done_common(sys, this_pkt);
			spin_lock_bh(&sys->spinlock);
			max_tx_pkt++;
			if (xmit_done)
				break;
		}
		/* If TX packets processing continuously in tasklet other
		 * softirqs are not able to run on that core which is leading
		 * to watchdog bark. For avoiding these scenarios exit from
		 * tasklet after reaching max limit.
		 */
		if (max_tx_pkt >= IPA_TX_MAX_DESC)
			break;
	}
	spin_unlock_bh(&sys->spinlock);

	if (max_tx_pkt >= IPA_TX_MAX_DESC)
		queue_work(sys->tasklet_wq, &sys->tasklet_work);
}


static void ipa3_send_nop_desc(struct work_struct *work)
{
	struct ipa3_sys_context *sys = container_of(work,
		struct ipa3_sys_context, work);
	struct gsi_xfer_elem nop_xfer;
	struct ipa3_tx_pkt_wrapper *tx_pkt;

	IPADBG_LOW("gsi send NOP for ch: %lu\n", sys->ep->gsi_chan_hdl);

	if (atomic_read(&sys->workqueue_flushed))
		return;

	tx_pkt = kmem_cache_zalloc(ipa3_ctx->tx_pkt_wrapper_cache, GFP_KERNEL);
	if (!tx_pkt) {
		queue_work(sys->wq, &sys->work);
		return;
	}

	INIT_LIST_HEAD(&tx_pkt->link);
	tx_pkt->cnt = 1;
	tx_pkt->no_unmap_dma = true;
	tx_pkt->sys = sys;
	spin_lock_bh(&sys->spinlock);
	if (unlikely(!sys->nop_pending)) {
		spin_unlock_bh(&sys->spinlock);
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);
		return;
	}
	list_add_tail(&tx_pkt->link, &sys->head_desc_list);
	sys->nop_pending = false;

	memset(&nop_xfer, 0, sizeof(nop_xfer));
	nop_xfer.type = GSI_XFER_ELEM_NOP;
	nop_xfer.flags = GSI_XFER_FLAG_EOT;
	nop_xfer.xfer_user_data = tx_pkt;
	if (gsi_queue_xfer(sys->ep->gsi_chan_hdl, 1, &nop_xfer, true)) {
		spin_unlock_bh(&sys->spinlock);
		IPAERR("gsi_queue_xfer for ch:%lu failed\n",
			sys->ep->gsi_chan_hdl);
		queue_work(sys->wq, &sys->work);
		return;
	}
	spin_unlock_bh(&sys->spinlock);

	/* make sure TAG process is sent before clocks are gated */
	ipa3_ctx->tag_process_before_gating = true;

}


/**
 * ipa3_send() - Send multiple descriptors in one HW transaction
 * @sys: system pipe context
 * @num_desc: number of packets
 * @desc: packets to send (may be immediate command or data)
 * @in_atomic:  whether caller is in atomic context
 *
 * This function is used for GPI connection.
 * - ipa3_tx_pkt_wrapper will be used for each ipa
 *   descriptor (allocated from wrappers cache)
 * - The wrapper struct will be configured for each ipa-desc payload and will
 *   contain information which will be later used by the user callbacks
 * - Each packet (command or data) that will be sent will also be saved in
 *   ipa3_sys_context for later check that all data was sent
 *
 * Return codes: 0: success, -EFAULT: failure
 */
int ipa3_send(struct ipa3_sys_context *sys,
		u32 num_desc,
		struct ipa3_desc *desc,
		bool in_atomic)
{
	struct ipa3_tx_pkt_wrapper *tx_pkt, *tx_pkt_first = NULL;
	struct ipahal_imm_cmd_pyld *tag_pyld_ret = NULL;
	struct ipa3_tx_pkt_wrapper *next_pkt;
	struct gsi_xfer_elem gsi_xfer[IPA_SEND_MAX_DESC];
	int i = 0;
	int j;
	int result;
	u32 mem_flag = GFP_ATOMIC;
	const struct ipa_gsi_ep_config *gsi_ep_cfg;
	bool send_nop = false;
	unsigned int max_desc;

	if (unlikely(!in_atomic))
		mem_flag = GFP_KERNEL;

	gsi_ep_cfg = ipa3_get_gsi_ep_info(sys->ep->client);
	if (unlikely(!gsi_ep_cfg)) {
		IPAERR("failed to get gsi EP config for client=%d\n",
			sys->ep->client);
		return -EFAULT;
	}
	if (unlikely(num_desc > IPA_SEND_MAX_DESC)) {
		IPAERR("max descriptors reached need=%d max=%d\n",
			num_desc, IPA_SEND_MAX_DESC);
		WARN_ON(1);
		return -EPERM;
	}

	max_desc = gsi_ep_cfg->ipa_if_tlv;
	if (gsi_ep_cfg->prefetch_mode == GSI_SMART_PRE_FETCH ||
		gsi_ep_cfg->prefetch_mode == GSI_FREE_PRE_FETCH)
		max_desc -= gsi_ep_cfg->prefetch_threshold;

	if (unlikely(num_desc > max_desc)) {
		IPAERR("Too many chained descriptors need=%d max=%d\n",
			num_desc, max_desc);
		WARN_ON(1);
		return -EPERM;
	}

	/* initialize only the xfers we use */
	memset(gsi_xfer, 0, sizeof(gsi_xfer[0]) * num_desc);

	spin_lock_bh(&sys->spinlock);

	for (i = 0; i < num_desc; i++) {
		tx_pkt = kmem_cache_zalloc(ipa3_ctx->tx_pkt_wrapper_cache,
					   GFP_ATOMIC);
		if (unlikely(!tx_pkt)) {
			IPAERR("failed to alloc tx wrapper\n");
			result = -ENOMEM;
			goto failure;
		}
		INIT_LIST_HEAD(&tx_pkt->link);

		if (i == 0) {
			tx_pkt_first = tx_pkt;
			tx_pkt->cnt = num_desc;
		}

		/* populate tag field */
		if (desc[i].is_tag_status) {
			if (unlikely(ipa_populate_tag_field(&desc[i], tx_pkt,
				&tag_pyld_ret))) {
				IPAERR("Failed to populate tag field\n");
				result = -EFAULT;
				goto failure_dma_map;
			}
		}

		tx_pkt->type = desc[i].type;

		if (desc[i].type != IPA_DATA_DESC_SKB_PAGED) {
			tx_pkt->mem.base = desc[i].pyld;
			tx_pkt->mem.size = desc[i].len;

			if (!desc[i].dma_address_valid) {
				tx_pkt->mem.phys_base =
					dma_map_single(ipa3_ctx->pdev,
					tx_pkt->mem.base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			} else {
				tx_pkt->mem.phys_base =
					desc[i].dma_address;
				tx_pkt->no_unmap_dma = true;
			}
		} else {
			tx_pkt->mem.base = desc[i].frag;
			tx_pkt->mem.size = desc[i].len;

			if (!desc[i].dma_address_valid) {
				tx_pkt->mem.phys_base =
					skb_frag_dma_map(ipa3_ctx->pdev,
					desc[i].frag,
					0, tx_pkt->mem.size,
					DMA_TO_DEVICE);
			} else {
				tx_pkt->mem.phys_base =
					desc[i].dma_address;
				tx_pkt->no_unmap_dma = true;
			}
		}
		if (unlikely(dma_mapping_error(ipa3_ctx->pdev,
			tx_pkt->mem.phys_base))) {
			IPAERR("failed to do dma map.\n");
			result = -EFAULT;
			goto failure_dma_map;
		}

		tx_pkt->sys = sys;
		tx_pkt->callback = desc[i].callback;
		tx_pkt->user1 = desc[i].user1;
		tx_pkt->user2 = desc[i].user2;
		tx_pkt->xmit_done = false;

		list_add_tail(&tx_pkt->link, &sys->head_desc_list);

		gsi_xfer[i].addr = tx_pkt->mem.phys_base;

		/*
		 * Special treatment for immediate commands, where
		 * the structure of the descriptor is different
		 */
		if (desc[i].type == IPA_IMM_CMD_DESC) {
			gsi_xfer[i].len = desc[i].opcode;
			gsi_xfer[i].type =
				GSI_XFER_ELEM_IMME_CMD;
		} else {
			gsi_xfer[i].len = desc[i].len;
			gsi_xfer[i].type =
				GSI_XFER_ELEM_DATA;
		}

		if (i == (num_desc - 1)) {
			if (!sys->use_comm_evt_ring ||
			    (sys->pkt_sent % IPA_EOT_THRESH == 0)) {
				gsi_xfer[i].flags |=
					GSI_XFER_FLAG_EOT;
				gsi_xfer[i].flags |=
					GSI_XFER_FLAG_BEI;
			} else {
				send_nop = true;
			}
			gsi_xfer[i].xfer_user_data =
				tx_pkt_first;
		} else {
			gsi_xfer[i].flags |=
				GSI_XFER_FLAG_CHAIN;
		}
	}

	IPADBG_LOW("ch:%lu queue xfer\n", sys->ep->gsi_chan_hdl);
	result = gsi_queue_xfer(sys->ep->gsi_chan_hdl, num_desc,
			gsi_xfer, true);
	if (unlikely(result != GSI_STATUS_SUCCESS)) {
		IPAERR_RL("GSI xfer failed.\n");
		result = -EFAULT;
		goto failure;
	}

	if (send_nop && !sys->nop_pending)
		sys->nop_pending = true;
	else
		send_nop = false;

	sys->pkt_sent++;
	spin_unlock_bh(&sys->spinlock);

	/* set the timer for sending the NOP descriptor */
	if (send_nop) {

		ktime_t time = ktime_set(0, IPA_TX_SEND_COMPL_NOP_DELAY_NS);

		IPADBG_LOW("scheduling timer for ch %lu\n",
			sys->ep->gsi_chan_hdl);
		hrtimer_start(&sys->db_timer, time, HRTIMER_MODE_REL);
	}

	/* make sure TAG process is sent before clocks are gated */
	ipa3_ctx->tag_process_before_gating = true;

	return 0;

failure_dma_map:
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);

failure:
	ipahal_destroy_imm_cmd(tag_pyld_ret);
	tx_pkt = tx_pkt_first;
	for (j = 0; j < i; j++) {
		next_pkt = list_next_entry(tx_pkt, link);
		list_del(&tx_pkt->link);

		if (!tx_pkt->no_unmap_dma) {
			if (desc[j].type != IPA_DATA_DESC_SKB_PAGED) {
				dma_unmap_single(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size, DMA_TO_DEVICE);
			} else {
				dma_unmap_page(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			}
		}
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);
		tx_pkt = next_pkt;
	}

	spin_unlock_bh(&sys->spinlock);
	return result;
}

/**
 * ipa3_send_one() - Send a single descriptor
 * @sys:	system pipe context
 * @desc:	descriptor to send
 * @in_atomic:  whether caller is in atomic context
 *
 * - Allocate tx_packet wrapper
 * - transfer data to the IPA
 * - after the transfer was done the SPS will
 *   notify the sending user via ipa_sps_irq_comp_tx()
 *
 * Return codes: 0: success, -EFAULT: failure
 */
int ipa3_send_one(struct ipa3_sys_context *sys, struct ipa3_desc *desc,
	bool in_atomic)
{
	return ipa3_send(sys, 1, desc, in_atomic);
}

/**
 * ipa3_transport_irq_cmd_ack - callback function which will be called by
 * the transport driver after an immediate command is complete.
 * @user1:	pointer to the descriptor of the transfer
 * @user2:
 *
 * Complete the immediate commands completion object, this will release the
 * thread which waits on this completion object (ipa3_send_cmd())
 */
static void ipa3_transport_irq_cmd_ack(void *user1, int user2)
{
	struct ipa3_desc *desc = (struct ipa3_desc *)user1;

	if (WARN(!desc, "desc is NULL"))
		return;

	IPADBG_LOW("got ack for cmd=%d\n", desc->opcode);
	complete(&desc->xfer_done);
}

/**
 * ipa3_transport_irq_cmd_ack_free - callback function which will be
 * called by the transport driver after an immediate command is complete.
 * This function will also free the completion object once it is done.
 * @tag_comp: pointer to the completion object
 * @ignored: parameter not used
 *
 * Complete the immediate commands completion object, this will release the
 * thread which waits on this completion object (ipa3_send_cmd())
 */
static void ipa3_transport_irq_cmd_ack_free(void *tag_comp, int ignored)
{
	struct ipa3_tag_completion *comp = tag_comp;

	if (!comp) {
		IPAERR("comp is NULL\n");
		return;
	}

	complete(&comp->comp);
	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);
}

/**
 * ipa3_send_cmd - send immediate commands
 * @num_desc:	number of descriptors within the desc struct
 * @descr:	descriptor structure
 *
 * Function will block till command gets ACK from IPA HW, caller needs
 * to free any resources it allocated after function returns
 * The callback in ipa3_desc should not be set by the caller
 * for this function.
 */
int ipa3_send_cmd(u16 num_desc, struct ipa3_desc *descr)
{
	struct ipa3_desc *desc;
	int i, result = 0;
	struct ipa3_sys_context *sys;
	int ep_idx;

	for (i = 0; i < num_desc; i++)
		IPADBG("sending imm cmd %d\n", descr[i].opcode);

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	if (-1 == ep_idx) {
		IPAERR("Client %u is not mapped\n",
			IPA_CLIENT_APPS_CMD_PROD);
		return -EFAULT;
	}

	sys = ipa3_ctx->ep[ep_idx].sys;
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (num_desc == 1) {
		init_completion(&descr->xfer_done);

		if (descr->callback || descr->user1)
			WARN_ON(1);

		descr->callback = ipa3_transport_irq_cmd_ack;
		descr->user1 = descr;
		if (ipa3_send_one(sys, descr, true)) {
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

		desc->callback = ipa3_transport_irq_cmd_ack;
		desc->user1 = desc;
		if (ipa3_send(sys, num_desc, descr, true)) {
			IPAERR("fail to send multiple immediate command set\n");
			result = -EFAULT;
			goto bail;
		}
		wait_for_completion(&desc->xfer_done);
	}

bail:
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return result;
}

/**
 * ipa3_send_cmd_timeout - send immediate commands with limited time
 *	waiting for ACK from IPA HW
 * @num_desc:	number of descriptors within the desc struct
 * @descr:	descriptor structure
 * @timeout:	millisecond to wait till get ACK from IPA HW
 *
 * Function will block till command gets ACK from IPA HW or timeout.
 * Caller needs to free any resources it allocated after function returns
 * The callback in ipa3_desc should not be set by the caller
 * for this function.
 */
int ipa3_send_cmd_timeout(u16 num_desc, struct ipa3_desc *descr, u32 timeout)
{
	struct ipa3_desc *desc;
	int i, result = 0;
	struct ipa3_sys_context *sys;
	int ep_idx;
	int completed;
	struct ipa3_tag_completion *comp;

	for (i = 0; i < num_desc; i++)
		IPADBG("sending imm cmd %d\n", descr[i].opcode);

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	if (-1 == ep_idx) {
		IPAERR("Client %u is not mapped\n",
			IPA_CLIENT_APPS_CMD_PROD);
		return -EFAULT;
	}

	comp = kzalloc(sizeof(*comp), GFP_ATOMIC);
	if (!comp)
		return -ENOMEM;

	init_completion(&comp->comp);

	/* completion needs to be released from both here and in ack callback */
	atomic_set(&comp->cnt, 2);

	sys = ipa3_ctx->ep[ep_idx].sys;

	if (num_desc == 1) {
		if (descr->callback || descr->user1)
			WARN_ON(1);

		descr->callback = ipa3_transport_irq_cmd_ack_free;
		descr->user1 = comp;
		if (ipa3_send_one(sys, descr, true)) {
			IPAERR("fail to send immediate command\n");
			kfree(comp);
			result = -EFAULT;
			goto bail;
		}
	} else {
		desc = &descr[num_desc - 1];

		if (desc->callback || desc->user1)
			WARN_ON(1);

		desc->callback = ipa3_transport_irq_cmd_ack_free;
		desc->user1 = comp;
		if (ipa3_send(sys, num_desc, descr, true)) {
			IPAERR("fail to send multiple immediate command set\n");
			kfree(comp);
			result = -EFAULT;
			goto bail;
		}
	}

	completed = wait_for_completion_timeout(
		&comp->comp, msecs_to_jiffies(timeout));
	if (!completed) {
		IPADBG("timeout waiting for imm-cmd ACK\n");
		result = -EBUSY;
	}

	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);

bail:
	return result;
}

/**
 * ipa3_handle_rx_core() - The core functionality of packet reception. This
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
static int ipa3_handle_rx_core(struct ipa3_sys_context *sys, bool process_all,
		bool in_poll_state)
{
	int ret;
	int cnt = 0;
	struct gsi_chan_xfer_notify notify = { 0 };

	while ((in_poll_state ? atomic_read(&sys->curr_polling_state) :
		!atomic_read(&sys->curr_polling_state))) {
		if (cnt && !process_all)
			break;

		ret = ipa_poll_gsi_pkt(sys, &notify);
		if (ret)
			break;

		if (IPA_CLIENT_IS_MEMCPY_DMA_CONS(sys->ep->client))
			ipa3_dma_memcpy_notify(sys);
		else if (IPA_CLIENT_IS_WLAN_CONS(sys->ep->client))
			ipa3_wlan_wq_rx_common(sys, &notify);
		else
			ipa3_wq_rx_common(sys, &notify);

		++cnt;
	}
	return cnt;
}

/**
 * __ipa3_update_curr_poll_state -> update current polling for default wan and
 *                                  coalescing pipe.
 * In RSC/RSB enabled cases using common event ring, so both the pipe
 * polling state should be in sync.
 */
void __ipa3_update_curr_poll_state(enum ipa_client_type client, int state)
{
	int ep_idx = IPA_EP_NOT_ALLOCATED;

	if (client == IPA_CLIENT_APPS_WAN_COAL_CONS)
		ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	if (client == IPA_CLIENT_APPS_WAN_CONS)
		ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);

	if (ep_idx != IPA_EP_NOT_ALLOCATED && ipa3_ctx->ep[ep_idx].sys)
		atomic_set(&ipa3_ctx->ep[ep_idx].sys->curr_polling_state,
									state);
}

/**
 * ipa3_rx_switch_to_intr_mode() - Operate the Rx data path in interrupt mode
 */
static int ipa3_rx_switch_to_intr_mode(struct ipa3_sys_context *sys)
{
	int ret;

	atomic_set(&sys->curr_polling_state, 0);
	__ipa3_update_curr_poll_state(sys->ep->client, 0);

	ipa3_dec_release_wakelock();
	ret = gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
		GSI_CHAN_MODE_CALLBACK);
	if ((ret != GSI_STATUS_SUCCESS) &&
		!atomic_read(&sys->curr_polling_state)) {
		if (ret == -GSI_STATUS_PENDING_IRQ) {
			ipa3_inc_acquire_wakelock();
			atomic_set(&sys->curr_polling_state, 1);
			__ipa3_update_curr_poll_state(sys->ep->client, 1);
		} else {
			IPAERR("Failed to switch to intr mode %d ch_id %d\n",
			 sys->curr_polling_state, sys->ep->gsi_chan_hdl);
		}
	}

	return ret;
}

/**
 * ipa3_handle_rx() - handle packet reception. This function is executed in the
 * context of a work queue.
 * @work: work struct needed by the work queue
 *
 * ipa3_handle_rx_core() is run in polling mode. After all packets has been
 * received, the driver switches back to interrupt mode.
 */
static void ipa3_handle_rx(struct ipa3_sys_context *sys)
{
	int inactive_cycles;
	int cnt;
	int ret;

	ipa_pm_activate_sync(sys->pm_hdl);
start_poll:
	inactive_cycles = 0;
	do {
		cnt = ipa3_handle_rx_core(sys, true, true);
		if (cnt == 0)
			inactive_cycles++;
		else
			inactive_cycles = 0;

		trace_idle_sleep_enter3(sys->ep->client);
		usleep_range(POLLING_MIN_SLEEP_RX, POLLING_MAX_SLEEP_RX);
		trace_idle_sleep_exit3(sys->ep->client);

		/*
		 * if pipe is out of buffers there is no point polling for
		 * completed descs; release the worker so delayed work can
		 * run in a timely manner
		 */
		if (sys->len == 0)
			break;

	} while (inactive_cycles <= POLLING_INACTIVITY_RX);

	trace_poll_to_intr3(sys->ep->client);
	ret = ipa3_rx_switch_to_intr_mode(sys);
	if (ret == -GSI_STATUS_PENDING_IRQ)
		goto start_poll;

	ipa_pm_deferred_deactivate(sys->pm_hdl);
}

static void ipa3_switch_to_intr_rx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa3_sys_context *sys;

	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa3_sys_context, switch_to_intr_work);

	if (sys->napi_obj) {
		/* interrupt mode is done in ipa3_rx_poll context */
		ipa_assert();
	} else
		ipa3_handle_rx(sys);
}

enum hrtimer_restart ipa3_ring_doorbell_timer_fn(struct hrtimer *param)
{
	struct ipa3_sys_context *sys = container_of(param,
		struct ipa3_sys_context, db_timer);

	queue_work(sys->wq, &sys->work);
	return HRTIMER_NORESTART;
}

static void ipa_pm_sys_pipe_cb(void *p, enum ipa_pm_cb_event event)
{
	struct ipa3_sys_context *sys = (struct ipa3_sys_context *)p;

	switch (event) {
	case IPA_PM_CLIENT_ACTIVATED:
		/*
		 * this event is ignored as the sync version of activation
		 * will be used.
		 */
		break;
	case IPA_PM_REQUEST_WAKEUP:
		/*
		 * pipe will be unsuspended as part of
		 * enabling IPA clocks
		 */
		IPADBG("calling wakeup for client %d\n", sys->ep->client);
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_WAN");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_WAN");
		} else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_LAN");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_LAN");
		} else if (sys->ep->client == IPA_CLIENT_ODL_DPL_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_ODL");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_ODL");
		} else if (sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_COAL");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_COAL");
		} else
			IPAERR("Unexpected event %d\n for client %d\n",
				event, sys->ep->client);
		break;
	default:
		IPAERR("Unexpected event %d\n for client %d\n",
			event, sys->ep->client);
		WARN_ON(1);
		return;
	}
}

/**
 * ipa3_setup_sys_pipe() - Setup an IPA GPI pipe and perform
 * IPA EP configuration
 * @sys_in:	[in] input needed to setup the pipe and configure EP
 * @clnt_hdl:	[out] client handle
 *
 *  - configure the end-point registers with the supplied
 *    parameters from the user.
 *  - Creates a GPI connection with IPA.
 *  - allocate descriptor FIFO
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int i, ipa_ep_idx, wan_handle, coal_ep_id;
	int result = -EINVAL;
	struct ipahal_reg_coal_qmap_cfg qmap_cfg;
	struct ipahal_reg_coal_evict_lru evict_lru;
	char buff[IPA_RESOURCE_NAME_MAX];
	struct ipa_ep_cfg ep_cfg_copy;

	if (sys_in == NULL || clnt_hdl == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}

	if (sys_in->client >= IPA_CLIENT_MAX || sys_in->desc_fifo_sz == 0) {
		IPAERR("bad parm client:%d fifo_sz:%d\n",
			sys_in->client, sys_in->desc_fifo_sz);
		goto fail_gen;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(sys_in->client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("Invalid client.\n");
		goto fail_gen;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid == 1) {
		IPAERR("EP %d already allocated.\n", ipa_ep_idx);
		goto fail_gen;
	}

	coal_ep_id = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
	/* save the input config parameters */
	if (sys_in->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
		ep_cfg_copy = sys_in->ipa_ep_cfg;

	IPA_ACTIVE_CLIENTS_INC_EP(sys_in->client);
	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));

	if (!ep->sys) {
		struct ipa_pm_register_params pm_reg;

		memset(&pm_reg, 0, sizeof(pm_reg));
		ep->sys = kzalloc(sizeof(struct ipa3_sys_context), GFP_KERNEL);
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
				WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS, 1);

		if (!ep->sys->wq) {
			IPAERR("failed to create wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq;
		}

		snprintf(buff, IPA_RESOURCE_NAME_MAX, "iparepwq%d",
				sys_in->client);
		ep->sys->repl_wq = alloc_workqueue(buff,
				WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS, 1);
		if (!ep->sys->repl_wq) {
			IPAERR("failed to create rep wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq2;
		}

		snprintf(buff, IPA_RESOURCE_NAME_MAX, "ipataskletwq%d",
				sys_in->client);
		ep->sys->tasklet_wq = alloc_workqueue(buff,
				WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS, 1);
		if (!ep->sys->tasklet_wq) {
			IPAERR("failed to create rep wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq3;
		}
		INIT_LIST_HEAD(&ep->sys->head_desc_list);
		INIT_LIST_HEAD(&ep->sys->rcycl_list);
		spin_lock_init(&ep->sys->spinlock);
		hrtimer_init(&ep->sys->db_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
		ep->sys->db_timer.function = ipa3_ring_doorbell_timer_fn;

		/* create IPA PM resources for handling polling mode */
		if (sys_in->client == IPA_CLIENT_APPS_WAN_CONS &&
			coal_ep_id != IPA_EP_NOT_ALLOCATED &&
			ipa3_ctx->ep[coal_ep_id].valid == 1) {
			/* Use coalescing pipe PM handle for default pipe also*/
			ep->sys->pm_hdl = ipa3_ctx->ep[coal_ep_id].sys->pm_hdl;
		} else if (IPA_CLIENT_IS_CONS(sys_in->client)) {
			pm_reg.name = ipa_clients_strings[sys_in->client];
			pm_reg.callback = ipa_pm_sys_pipe_cb;
			pm_reg.user_data = ep->sys;
			pm_reg.group = IPA_PM_GROUP_APPS;
			result = ipa_pm_register(&pm_reg, &ep->sys->pm_hdl);
			if (result) {
				IPAERR("failed to create IPA PM client %d\n",
					result);
				goto fail_pm;
			}

			if (IPA_CLIENT_IS_APPS_CONS(sys_in->client)) {
				result = ipa_pm_associate_ipa_cons_to_client(
					ep->sys->pm_hdl, sys_in->client);
				if (result) {
					IPAERR("failed to associate\n");
					goto fail_gen2;
				}
			}

			result = ipa_pm_set_throughput(ep->sys->pm_hdl,
				IPA_APPS_BW_FOR_PM);
			if (result) {
				IPAERR("failed to set profile IPA PM client\n");
				goto fail_gen2;
			}
		}
	} else {
		memset(ep->sys, 0, offsetof(struct ipa3_sys_context, ep));
	}

	atomic_set(&ep->sys->xmit_eot_cnt, 0);
	tasklet_init(&ep->sys->tasklet, ipa3_tasklet_write_done,
			(unsigned long) ep->sys);
	INIT_WORK(&ep->sys->tasklet_work,
		ipa3_tasklet_schd_work);
	ep->skip_ep_cfg = sys_in->skip_ep_cfg;
	if (ipa3_assign_policy(sys_in, ep->sys)) {
		IPAERR("failed to sys ctx for client %d\n", sys_in->client);
		result = -ENOMEM;
		goto fail_gen2;
	}

	ep->valid = 1;
	ep->client = sys_in->client;
	ep->client_notify = sys_in->notify;
	ep->sys->napi_obj = sys_in->napi_obj;
	ep->priv = sys_in->priv;
	ep->keep_ipa_awake = sys_in->keep_ipa_awake;
	atomic_set(&ep->avail_fifo_desc,
		((sys_in->desc_fifo_sz / IPA_FIFO_ELEMENT_SIZE) - 1));

	if (ep->status.status_en && IPA_CLIENT_IS_CONS(ep->client) &&
	    ep->sys->status_stat == NULL) {
		ep->sys->status_stat =
			kzalloc(sizeof(struct ipa3_status_stats), GFP_KERNEL);
		if (!ep->sys->status_stat)
			goto fail_gen2;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_gen2;
		}
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_gen2;
		}
		IPADBG("ep %d configuration successful\n", ipa_ep_idx);
	} else {
		IPADBG("skipping ep %d configuration\n", ipa_ep_idx);
	}

	result = ipa_gsi_setup_channel(sys_in, ep);
	if (result) {
		IPAERR("Failed to setup GSI channel\n");
		goto fail_gen2;
	}

	*clnt_hdl = ipa_ep_idx;

	if (ep->sys->repl_hdlr == ipa3_fast_replenish_rx_cache) {
		ep->sys->repl = kzalloc(sizeof(*ep->sys->repl), GFP_KERNEL);
		if (!ep->sys->repl) {
			IPAERR("failed to alloc repl for client %d\n",
					sys_in->client);
			result = -ENOMEM;
			goto fail_gen2;
		}
		atomic_set(&ep->sys->repl->pending, 0);
		ep->sys->repl->capacity = ep->sys->rx_pool_sz + 1;

		ep->sys->repl->cache = kcalloc(ep->sys->repl->capacity,
				sizeof(void *), GFP_KERNEL);
		if (!ep->sys->repl->cache) {
			IPAERR("ep=%d fail to alloc repl cache\n", ipa_ep_idx);
			ep->sys->repl_hdlr = ipa3_replenish_rx_cache;
			ep->sys->repl->capacity = 0;
		} else {
			atomic_set(&ep->sys->repl->head_idx, 0);
			atomic_set(&ep->sys->repl->tail_idx, 0);
			ipa3_wq_repl_rx(&ep->sys->repl_work);
		}
	}

	if (ep->sys->repl_hdlr == ipa3_replenish_rx_page_recycle) {
		ep->sys->page_recycle_repl = kzalloc(
			sizeof(*ep->sys->page_recycle_repl), GFP_KERNEL);
		if (!ep->sys->page_recycle_repl) {
			IPAERR("failed to alloc repl for client %d\n",
					sys_in->client);
			result = -ENOMEM;
			goto fail_gen2;
		}
		atomic_set(&ep->sys->page_recycle_repl->pending, 0);
		ep->sys->page_recycle_repl->capacity =
				(ep->sys->rx_pool_sz + 1) * 2;

		ep->sys->page_recycle_repl->cache =
				kcalloc(ep->sys->page_recycle_repl->capacity,
				sizeof(void *), GFP_KERNEL);
		atomic_set(&ep->sys->page_recycle_repl->head_idx, 0);
		atomic_set(&ep->sys->page_recycle_repl->tail_idx, 0);
		ep->sys->repl = kzalloc(sizeof(*ep->sys->repl), GFP_KERNEL);
		if (!ep->sys->repl) {
			IPAERR("failed to alloc repl for client %d\n",
				   sys_in->client);
			result = -ENOMEM;
			goto fail_page_recycle_repl;
		}
		ep->sys->repl->capacity = (ep->sys->rx_pool_sz + 1);

		atomic_set(&ep->sys->repl->pending, 0);
		ep->sys->repl->cache = kcalloc(ep->sys->repl->capacity,
				sizeof(void *), GFP_KERNEL);
		atomic_set(&ep->sys->repl->head_idx, 0);
		atomic_set(&ep->sys->repl->tail_idx, 0);

		ipa3_replenish_rx_page_cache(ep->sys);
		ipa3_wq_page_repl(&ep->sys->repl_work);
	}

	if (IPA_CLIENT_IS_CONS(sys_in->client)) {
		if (IPA_CLIENT_IS_WAN_CONS(sys_in->client) &&
			ipa3_ctx->ipa_wan_skb_page) {
			ipa3_replenish_rx_page_recycle(ep->sys);
		} else
			ipa3_replenish_rx_cache(ep->sys);
		for (i = 0; i < GSI_VEID_MAX; i++)
			INIT_LIST_HEAD(&ep->sys->pending_pkts[i]);
	}

	if (IPA_CLIENT_IS_WLAN_CONS(sys_in->client)) {
		ipa3_alloc_wlan_rx_common_cache(IPA_WLAN_COMM_RX_POOL_LOW);
		atomic_inc(&ipa3_ctx->wc_memb.active_clnt_cnt);
	}

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(sys_in->client)) {
		if (ipa3_ctx->modem_cfg_emb_pipe_flt &&
			sys_in->client == IPA_CLIENT_APPS_WAN_PROD)
			IPADBG("modem cfg emb pipe flt\n");
		else
			ipa3_install_dflt_flt_rules(ipa_ep_idx);
	}

	result = ipa3_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d ep=%d.\n", result,
			ipa_ep_idx);
		goto fail_repl;
	}

	result = gsi_start_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_start_channel failed res=%d ep=%d.\n", result,
			ipa_ep_idx);
		goto fail_gen3;
	}

	IPADBG("client %d (ep: %d) connected sys=%pK\n", sys_in->client,
			ipa_ep_idx, ep->sys);

	/* configure the registers and setup the default pipe */
	if (sys_in->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
		evict_lru.coal_vp_lru_thrshld = 0;
		evict_lru.coal_eviction_en = true;
		ipahal_write_reg_fields(IPA_COAL_EVICT_LRU, &evict_lru);

		qmap_cfg.mux_id_byte_sel = IPA_QMAP_ID_BYTE;
		ipahal_write_reg_fields(IPA_COAL_QMAP_CFG, &qmap_cfg);

		sys_in->client = IPA_CLIENT_APPS_WAN_CONS;
		sys_in->ipa_ep_cfg = ep_cfg_copy;
		result = ipa3_setup_sys_pipe(sys_in, &wan_handle);
		if (result) {
			IPAERR("failed to setup default coalescing pipe\n");
			goto fail_repl;
		}
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ep->client);

	return 0;

fail_gen3:
	ipa3_disable_data_path(ipa_ep_idx);
fail_repl:
	ep->sys->repl_hdlr = ipa3_replenish_rx_cache;
	ep->sys->repl->capacity = 0;
	kfree(ep->sys->repl);
fail_page_recycle_repl:
	if (ep->sys->page_recycle_repl) {
		ep->sys->page_recycle_repl->capacity = 0;
		kfree(ep->sys->page_recycle_repl);
	}
fail_gen2:
	ipa_pm_deregister(ep->sys->pm_hdl);
fail_pm:
	destroy_workqueue(ep->sys->tasklet_wq);
fail_wq3:
	destroy_workqueue(ep->sys->repl_wq);
fail_wq2:
	destroy_workqueue(ep->sys->wq);
fail_wq:
	kfree(ep->sys);
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
fail_and_disable_clocks:
	IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);
fail_gen:
	return result;
}

/**
 * ipa3_teardown_sys_pipe() - Teardown the GPI pipe and cleanup IPA EP
 * @clnt_hdl:	[in] the handle obtained from ipa3_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_teardown_sys_pipe(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int empty;
	int result;
	int i;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_disable_data_path(clnt_hdl);

	if (IPA_CLIENT_IS_PROD(ep->client)) {
		do {
			spin_lock_bh(&ep->sys->spinlock);
			empty = list_empty(&ep->sys->head_desc_list);
			spin_unlock_bh(&ep->sys->spinlock);
			if (!empty)
				usleep_range(95, 105);
			else
				break;
		} while (1);
	}

	/* channel stop might fail on timeout if IPA is busy */
	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		result = ipa3_stop_gsi_channel(clnt_hdl);
		if (result == GSI_STATUS_SUCCESS)
			break;

		if (result != -GSI_STATUS_AGAIN &&
			result != -GSI_STATUS_TIMED_OUT)
			break;
	}

	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("GSI stop chan err: %d.\n", result);
		ipa_assert();
		return result;
	}

	do {
		usleep_range(95, 105);
	} while (atomic_read(&ep->sys->curr_polling_state));

	if (IPA_CLIENT_IS_CONS(ep->client))
		cancel_delayed_work_sync(&ep->sys->replenish_rx_work);
	flush_workqueue(ep->sys->wq);
	if (IPA_CLIENT_IS_PROD(ep->client))
		atomic_set(&ep->sys->workqueue_flushed, 1);

	/* tear down the default pipe before we reset the channel*/
	if (ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
		i = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);

		if (i == IPA_EP_NOT_ALLOCATED) {
			IPAERR("failed to get idx");
			return i;
		}

		result = ipa3_teardown_coal_def_pipe(i);
		if (result) {
			IPAERR("failed to teardown default coal pipe\n");
			return result;
		}
	}

	result = ipa3_reset_gsi_channel(clnt_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to reset chan: %d.\n", result);
		ipa_assert();
		return result;
	}
	dma_free_coherent(ipa3_ctx->pdev,
		ep->gsi_mem_info.chan_ring_len,
		ep->gsi_mem_info.chan_ring_base_vaddr,
		ep->gsi_mem_info.chan_ring_base_addr);
	result = gsi_dealloc_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to dealloc chan: %d.\n", result);
		ipa_assert();
		return result;
	}

	/* free event ring only when it is present */
	if (ep->sys->use_comm_evt_ring) {
		ipa3_ctx->gsi_evt_comm_ring_rem +=
			ep->gsi_mem_info.chan_ring_len;
	} else if (ep->gsi_evt_ring_hdl != ~0) {
		result = gsi_reset_evt_ring(ep->gsi_evt_ring_hdl);
		if (WARN(result != GSI_STATUS_SUCCESS, "reset evt %d", result))
			return result;

		dma_free_coherent(ipa3_ctx->pdev,
			ep->gsi_mem_info.evt_ring_len,
			ep->gsi_mem_info.evt_ring_base_vaddr,
			ep->gsi_mem_info.evt_ring_base_addr);
		result = gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
		if (WARN(result != GSI_STATUS_SUCCESS, "deall evt %d", result))
			return result;
	}
	if (ep->sys->repl_wq)
		flush_workqueue(ep->sys->repl_wq);
	if (ep->sys->tasklet_wq)
		flush_workqueue(ep->sys->tasklet_wq);
	if (IPA_CLIENT_IS_CONS(ep->client))
		ipa3_cleanup_rx(ep->sys);

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(ep->client)) {
		if (ipa3_ctx->modem_cfg_emb_pipe_flt &&
			ep->client == IPA_CLIENT_APPS_WAN_PROD)
			IPADBG("modem cfg emb pipe flt\n");
		else
			ipa3_delete_dflt_flt_rules(clnt_hdl);
	}

	if (IPA_CLIENT_IS_WLAN_CONS(ep->client))
		atomic_dec(&ipa3_ctx->wc_memb.active_clnt_cnt);

	memset(&ep->wstats, 0, sizeof(struct ipa3_wlan_stats));

	if (!atomic_read(&ipa3_ctx->wc_memb.active_clnt_cnt))
		ipa3_cleanup_wlan_rx_common_cache();

	ep->valid = 0;

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}

/**
 * ipa3_teardown_coal_def_pipe() - Teardown the APPS_WAN_COAL_CONS
 *				   default GPI pipe and cleanup IPA EP
 *				   called after the coalesced pipe is destroyed.
 * @clnt_hdl:	[in] the handle obtained from ipa3_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
static int ipa3_teardown_coal_def_pipe(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result;
	int i;

	ep = &ipa3_ctx->ep[clnt_hdl];

	ipa3_disable_data_path(clnt_hdl);

	/* channel stop might fail on timeout if IPA is busy */
	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		result = ipa3_stop_gsi_channel(clnt_hdl);
		if (result == GSI_STATUS_SUCCESS)
			break;

		if (result != -GSI_STATUS_AGAIN &&
		    result != -GSI_STATUS_TIMED_OUT)
			break;
	}

	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("GSI stop chan err: %d.\n", result);
		ipa_assert();
		return result;
	}
	result = ipa3_reset_gsi_channel(clnt_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to reset chan: %d.\n", result);
		ipa_assert();
		return result;
	}
	dma_free_coherent(ipa3_ctx->pdev,
		ep->gsi_mem_info.chan_ring_len,
		ep->gsi_mem_info.chan_ring_base_vaddr,
		ep->gsi_mem_info.chan_ring_base_addr);
	result = gsi_dealloc_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to dealloc chan: %d.\n", result);
		ipa_assert();
		return result;
	}

	if (IPA_CLIENT_IS_CONS(ep->client))
		cancel_delayed_work_sync(&ep->sys->replenish_rx_work);

	flush_workqueue(ep->sys->wq);

	if (ep->sys->repl_wq)
		flush_workqueue(ep->sys->repl_wq);
	if (IPA_CLIENT_IS_CONS(ep->client))
		ipa3_cleanup_rx(ep->sys);

	ep->valid = 0;

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}

/**
 * ipa3_tx_comp_usr_notify_release() - Callback function which will call the
 * user supplied callback function to release the skb, or release it on
 * its own if no callback function was supplied.
 * @user1
 * @user2
 *
 * This notified callback is for the destination client.
 */
static void ipa3_tx_comp_usr_notify_release(void *user1, int user2)
{
	struct sk_buff *skb = (struct sk_buff *)user1;
	int ep_idx = user2;

	IPADBG_LOW("skb=%pK ep=%d\n", skb, ep_idx);

	IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_pkts_compl);

	if (ipa3_ctx->ep[ep_idx].client_notify)
		ipa3_ctx->ep[ep_idx].client_notify(ipa3_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)skb);
	else
		dev_kfree_skb_any(skb);
}

void ipa3_tx_cmd_comp(void *user1, int user2)
{
	ipahal_destroy_imm_cmd(user1);
}

/**
 * ipa3_tx_dp() - Data-path tx handler
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
 * Once this send was done from transport point-of-view the IPA driver will
 * get notified by the supplied callback.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *meta)
{
	struct ipa3_desc *desc;
	struct ipa3_desc _desc[3];
	int dst_ep_idx;
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	struct ipa3_sys_context *sys;
	int src_ep_idx;
	int num_frags, f;
	const struct ipa_gsi_ep_config *gsi_ep;
	int data_idx;
	unsigned int max_desc;

	if (unlikely(!ipa3_ctx)) {
		IPAERR("IPA3 driver was not initialized\n");
		return -EINVAL;
	}

	if (unlikely(skb->len == 0)) {
		IPAERR("packet size is 0\n");
		return -EINVAL;
	}

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
		src_ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_PROD);
		if (unlikely(-1 == src_ep_idx)) {
			IPAERR("Client %u is not mapped\n",
				IPA_CLIENT_APPS_LAN_PROD);
			goto fail_gen;
		}
		dst_ep_idx = ipa3_get_ep_mapping(dst);
	} else {
		src_ep_idx = ipa3_get_ep_mapping(dst);
		if (unlikely(-1 == src_ep_idx)) {
			IPAERR("Client %u is not mapped\n", dst);
			goto fail_gen;
		}
		if (meta && meta->pkt_init_dst_ep_valid)
			dst_ep_idx = meta->pkt_init_dst_ep;
		else
			dst_ep_idx = -1;
	}

	sys = ipa3_ctx->ep[src_ep_idx].sys;

	if (unlikely(!sys || !sys->ep->valid)) {
		IPAERR_RL("pipe %d not valid\n", src_ep_idx);
		goto fail_pipe_not_valid;
	}

	num_frags = skb_shinfo(skb)->nr_frags;
	/*
	 * make sure TLV FIFO supports the needed frags.
	 * 2 descriptors are needed for IP_PACKET_INIT and TAG_STATUS.
	 * 1 descriptor needed for the linear portion of skb.
	 */
	gsi_ep = ipa3_get_gsi_ep_info(ipa3_ctx->ep[src_ep_idx].client);
	if (unlikely(gsi_ep == NULL)) {
		IPAERR("failed to get EP %d GSI info\n", src_ep_idx);
		goto fail_gen;
	}
	max_desc =  gsi_ep->ipa_if_tlv;
	if (gsi_ep->prefetch_mode == GSI_SMART_PRE_FETCH ||
		gsi_ep->prefetch_mode == GSI_FREE_PRE_FETCH)
		max_desc -= gsi_ep->prefetch_threshold;
	if (num_frags + 3 > max_desc) {
		if (unlikely(skb_linearize(skb))) {
			IPAERR("Failed to linear skb with %d frags\n",
				num_frags);
			goto fail_gen;
		}
		num_frags = 0;
	}
	if (num_frags) {
		/* 1 desc for tag to resolve status out-of-order issue;
		 * 1 desc is needed for the linear portion of skb;
		 * 1 desc may be needed for the PACKET_INIT;
		 * 1 desc for each frag
		 */
		desc = kzalloc(sizeof(*desc) * (num_frags + 3), GFP_ATOMIC);
		if (unlikely(!desc)) {
			IPAERR("failed to alloc desc array\n");
			goto fail_gen;
		}
	} else {
		memset(_desc, 0, 3 * sizeof(struct ipa3_desc));
		desc = &_desc[0];
	}

	if (dst_ep_idx != -1) {
		int skb_idx;

		/* SW data path */
		data_idx = 0;
		if (sys->policy == IPA_POLICY_NOINTR_MODE) {
			/*
			 * For non-interrupt mode channel (where there is no
			 * event ring) TAG STATUS are used for completion
			 * notification. IPA will generate a status packet with
			 * tag info as a result of the TAG STATUS command.
			 */
			desc[data_idx].is_tag_status = true;
			data_idx++;
		}
		desc[data_idx].opcode = ipa3_ctx->pkt_init_imm_opcode;
		desc[data_idx].dma_address_valid = true;
		desc[data_idx].dma_address = ipa3_ctx->pkt_init_imm[dst_ep_idx];
		desc[data_idx].type = IPA_IMM_CMD_DESC;
		desc[data_idx].callback = NULL;
		data_idx++;
		desc[data_idx].pyld = skb->data;
		desc[data_idx].len = skb_headlen(skb);
		desc[data_idx].type = IPA_DATA_DESC_SKB;
		desc[data_idx].callback = ipa3_tx_comp_usr_notify_release;
		desc[data_idx].user1 = skb;
		desc[data_idx].user2 = (meta && meta->pkt_init_dst_ep_valid &&
				meta->pkt_init_dst_ep_remote) ?
				src_ep_idx :
				dst_ep_idx;
		if (meta && meta->dma_address_valid) {
			desc[data_idx].dma_address_valid = true;
			desc[data_idx].dma_address = meta->dma_address;
		}

		skb_idx = data_idx;
		data_idx++;

		for (f = 0; f < num_frags; f++) {
			desc[data_idx + f].frag = &skb_shinfo(skb)->frags[f];
			desc[data_idx + f].type = IPA_DATA_DESC_SKB_PAGED;
			desc[data_idx + f].len =
				skb_frag_size(desc[data_idx + f].frag);
		}
		/* don't free skb till frag mappings are released */
		if (num_frags) {
			desc[data_idx + f - 1].callback =
				desc[skb_idx].callback;
			desc[data_idx + f - 1].user1 = desc[skb_idx].user1;
			desc[data_idx + f - 1].user2 = desc[skb_idx].user2;
			desc[skb_idx].callback = NULL;
		}

		if (unlikely(ipa3_send(sys, num_frags + data_idx,
		    desc, true))) {
			IPAERR_RL("fail to send skb %pK num_frags %u SWP\n",
				skb, num_frags);
			goto fail_send;
		}
		IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_sw_pkts);
	} else {
		/* HW data path */
		data_idx = 0;
		if (sys->policy == IPA_POLICY_NOINTR_MODE) {
			/*
			 * For non-interrupt mode channel (where there is no
			 * event ring) TAG STATUS are used for completion
			 * notification. IPA will generate a status packet with
			 * tag info as a result of the TAG STATUS command.
			 */
			desc[data_idx].is_tag_status = true;
			data_idx++;
		}
		desc[data_idx].pyld = skb->data;
		desc[data_idx].len = skb_headlen(skb);
		desc[data_idx].type = IPA_DATA_DESC_SKB;
		desc[data_idx].callback = ipa3_tx_comp_usr_notify_release;
		desc[data_idx].user1 = skb;
		desc[data_idx].user2 = src_ep_idx;

		if (meta && meta->dma_address_valid) {
			desc[data_idx].dma_address_valid = true;
			desc[data_idx].dma_address = meta->dma_address;
		}
		if (num_frags == 0) {
			if (unlikely(ipa3_send(sys, data_idx + 1,
				 desc, true))) {
				IPAERR("fail to send skb %pK HWP\n", skb);
				goto fail_mem;
			}
		} else {
			for (f = 0; f < num_frags; f++) {
				desc[data_idx+f+1].frag =
					&skb_shinfo(skb)->frags[f];
				desc[data_idx+f+1].type =
					IPA_DATA_DESC_SKB_PAGED;
				desc[data_idx+f+1].len =
					skb_frag_size(desc[data_idx+f+1].frag);
			}
			/* don't free skb till frag mappings are released */
			desc[data_idx+f].callback = desc[data_idx].callback;
			desc[data_idx+f].user1 = desc[data_idx].user1;
			desc[data_idx+f].user2 = desc[data_idx].user2;
			desc[data_idx].callback = NULL;

			if (unlikely(ipa3_send(sys, num_frags + data_idx + 1,
			    desc, true))) {
				IPAERR("fail to send skb %pK num_frags %u\n",
					skb, num_frags);
				goto fail_mem;
			}
		}
		IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_hw_pkts);
	}

	if (num_frags) {
		kfree(desc);
		IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_non_linear);
	}
	return 0;

fail_send:
	ipahal_destroy_imm_cmd(cmd_pyld);
fail_mem:
	if (num_frags)
		kfree(desc);
fail_gen:
	return -EFAULT;
fail_pipe_not_valid:
	return -EPIPE;
}

static void ipa3_wq_handle_rx(struct work_struct *work)
{
	struct ipa3_sys_context *sys;

	sys = container_of(work, struct ipa3_sys_context, work);

	if (sys->napi_obj) {
		ipa_pm_activate_sync(sys->pm_hdl);
		napi_schedule(sys->napi_obj);
		IPA_STATS_INC_CNT(sys->napi_sch_cnt);
	} else
		ipa3_handle_rx(sys);
}

static void ipa3_wq_repl_rx(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	gfp_t flag = GFP_KERNEL;
	u32 next;
	u32 curr;

	sys = container_of(work, struct ipa3_sys_context, repl_work);
	atomic_set(&sys->repl->pending, 0);
	curr = atomic_read(&sys->repl->tail_idx);

begin:
	while (1) {
		next = (curr + 1) % sys->repl->capacity;
		if (unlikely(next == atomic_read(&sys->repl->head_idx)))
			goto fail_kmem_cache_alloc;

		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (unlikely(!rx_pkt))
			goto fail_kmem_cache_alloc;

		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (unlikely(rx_pkt->data.skb == NULL))
			goto fail_skb_alloc;

		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(ipa3_ctx->pdev,
		    rx_pkt->data.dma_addr))) {
			pr_err_ratelimited("%s dma map fail %pK for %pK sys=%pK\n",
			       __func__, (void *)rx_pkt->data.dma_addr,
			       ptr, sys);
			goto fail_dma_mapping;
		}

		sys->repl->cache[curr] = rx_pkt;
		curr = next;
		/* ensure write is done before setting tail index */
		mb();
		atomic_set(&sys->repl->tail_idx, next);
	}

	return;

fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	if (atomic_read(&sys->repl->tail_idx) ==
			atomic_read(&sys->repl->head_idx)) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS ||
			sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_repl_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_repl_rx_empty);
		pr_err_ratelimited("%s sys=%pK repl ring empty\n",
				__func__, sys);
		goto begin;
	}
}

static struct ipa3_rx_pkt_wrapper *ipa3_alloc_rx_pkt_page(
	gfp_t flag, bool is_tmp_alloc)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;

	flag |= __GFP_NOMEMALLOC;
	rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
		flag);
	if (unlikely(!rx_pkt))
		return NULL;
	rx_pkt->len = PAGE_SIZE << IPA_WAN_PAGE_ORDER;
	rx_pkt->page_data.page = __dev_alloc_pages(flag,
		IPA_WAN_PAGE_ORDER);
	if (unlikely(!rx_pkt->page_data.page))
		goto fail_page_alloc;

	rx_pkt->page_data.dma_addr = dma_map_page(ipa3_ctx->pdev,
			rx_pkt->page_data.page, 0,
			rx_pkt->len, DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(ipa3_ctx->pdev,
		rx_pkt->page_data.dma_addr))) {
		pr_err_ratelimited("%s dma map fail %pK for %pK\n",
			__func__, (void *)rx_pkt->page_data.dma_addr,
			rx_pkt->page_data.page);
		goto fail_dma_mapping;
	}
	if (is_tmp_alloc)
		rx_pkt->page_data.is_tmp_alloc = true;
	else
		rx_pkt->page_data.is_tmp_alloc = false;
	return rx_pkt;

fail_dma_mapping:
	__free_pages(rx_pkt->page_data.page, IPA_WAN_PAGE_ORDER);
fail_page_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
	return NULL;
}

static void ipa3_replenish_rx_page_cache(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	u32 curr;

	for (curr = 0; curr < sys->page_recycle_repl->capacity; curr++) {
		rx_pkt = ipa3_alloc_rx_pkt_page(GFP_KERNEL, false);
		if (unlikely(!rx_pkt)) {
			IPAERR("ipa3_alloc_rx_pkt_page fails\n");
			ipa_assert();
			break;
		}
		rx_pkt->sys = sys;
		sys->page_recycle_repl->cache[curr] = rx_pkt;
	}

	return;

}

static void ipa3_wq_page_repl(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	u32 next;
	u32 curr;

	sys = container_of(work, struct ipa3_sys_context, repl_work);
	atomic_set(&sys->repl->pending, 0);
	curr = atomic_read(&sys->repl->tail_idx);

begin:
	while (1) {
		next = (curr + 1) % sys->repl->capacity;
		if (unlikely(next == atomic_read(&sys->repl->head_idx)))
			goto fail_kmem_cache_alloc;
		rx_pkt = ipa3_alloc_rx_pkt_page(GFP_KERNEL, true);
		if (unlikely(!rx_pkt)) {
			IPAERR("ipa3_alloc_rx_pkt_page fails\n");
			break;
		}
		rx_pkt->sys = sys;
		sys->repl->cache[curr] = rx_pkt;
		curr = next;
		/* ensure write is done before setting tail index */
		mb();
		atomic_set(&sys->repl->tail_idx, next);
	}

	return;

fail_kmem_cache_alloc:
	if (atomic_read(&sys->repl->tail_idx) ==
			atomic_read(&sys->repl->head_idx)) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS ||
			sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_repl_rx_empty);
		pr_err_ratelimited("%s sys=%pK wq_repl ring empty\n",
				__func__, sys);
		goto begin;
	}

}

static inline void __trigger_repl_work(struct ipa3_sys_context *sys)
{
	int tail, head, avail;

	if (atomic_read(&sys->repl->pending))
		return;

	tail = atomic_read(&sys->repl->tail_idx);
	head = atomic_read(&sys->repl->head_idx);
	avail = (tail - head) % sys->repl->capacity;

	if (avail < sys->repl->capacity / 4) {
		atomic_set(&sys->repl->pending, 1);
		queue_work(sys->repl_wq, &sys->repl_work);
	}
}


static void ipa3_replenish_rx_page_recycle(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	u32 curr;
	u32 curr_wq;
	int idx = 0;
	struct page *cur_page;
	u32 stats_i = 0;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;
	stats_i = (sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) ? 0 : 1;

	spin_lock_bh(&sys->spinlock);
	rx_len_cached = sys->len;
	curr = atomic_read(&sys->page_recycle_repl->head_idx);
	curr_wq = atomic_read(&sys->repl->head_idx);

	while (rx_len_cached < sys->rx_pool_sz) {
		cur_page = sys->page_recycle_repl->cache[curr]->page_data.page;
		/* Found an idle page that can be used */
		if (page_ref_count(cur_page) == 1) {
			page_ref_inc(cur_page);
			rx_pkt = sys->page_recycle_repl->cache[curr];
			curr = (++curr == sys->page_recycle_repl->capacity) ?
								0 : curr;
		} else {
			/*
			 * Could not find idle page at curr index.
			 * Allocate a new one.
			 */
			if (curr_wq == atomic_read(&sys->repl->tail_idx))
				break;
			ipa3_ctx->stats.page_recycle_stats[stats_i].tmp_alloc++;
			rx_pkt = sys->repl->cache[curr_wq];
			curr_wq = (++curr_wq == sys->repl->capacity) ?
								 0 : curr_wq;
		}

		dma_sync_single_for_device(ipa3_ctx->pdev,
			rx_pkt->page_data.dma_addr,
			rx_pkt->len, DMA_FROM_DEVICE);
		gsi_xfer_elem_array[idx].addr = rx_pkt->page_data.dma_addr;
		gsi_xfer_elem_array[idx].len = rx_pkt->len;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		rx_len_cached++;
		idx++;
		ipa3_ctx->stats.page_recycle_stats[stats_i].total_replenished++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_THRESH.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (unlikely(ret != GSI_STATUS_SUCCESS)) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				ipa_assert();
				break;
			}
			idx = 0;
		}
	}
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
			gsi_xfer_elem_array, true);
	if (likely(ret == GSI_STATUS_SUCCESS)) {
		/* ensure write is done before setting head index */
		mb();
		atomic_set(&sys->repl->head_idx, curr_wq);
		atomic_set(&sys->page_recycle_repl->head_idx, curr);
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		ipa_assert();
	}
	spin_unlock_bh(&sys->spinlock);
	__trigger_repl_work(sys);

	if (rx_len_cached <= IPA_DEFAULT_SYS_YELLOW_WM) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS ||
			sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_rx_empty);
		else
			WARN_ON(1);
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
	}

	return;
}

static void ipa3_replenish_wlan_rx_cache(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt = NULL;
	struct ipa3_rx_pkt_wrapper *tmp;
	int ret;
	struct gsi_xfer_elem gsi_xfer_elem_one;
	u32 rx_len_cached = 0;

	IPADBG_LOW("\n");

	spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
	rx_len_cached = sys->len;

	if (rx_len_cached < sys->rx_pool_sz) {
		list_for_each_entry_safe(rx_pkt, tmp,
			&ipa3_ctx->wc_memb.wlan_comm_desc_list, link) {
			list_del(&rx_pkt->link);

			if (ipa3_ctx->wc_memb.wlan_comm_free_cnt > 0)
				ipa3_ctx->wc_memb.wlan_comm_free_cnt--;

			rx_pkt->len = 0;
			rx_pkt->sys = sys;

			memset(&gsi_xfer_elem_one, 0,
				sizeof(gsi_xfer_elem_one));
			gsi_xfer_elem_one.addr = rx_pkt->data.dma_addr;
			gsi_xfer_elem_one.len = IPA_WLAN_RX_BUFF_SZ;
			gsi_xfer_elem_one.flags |= GSI_XFER_FLAG_EOT;
			gsi_xfer_elem_one.flags |= GSI_XFER_FLAG_EOB;
			gsi_xfer_elem_one.type = GSI_XFER_ELEM_DATA;
			gsi_xfer_elem_one.xfer_user_data = rx_pkt;

			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, 1,
				&gsi_xfer_elem_one, true);

			if (unlikely(ret)) {
				IPAERR("failed to provide buffer: %d\n", ret);
				goto fail_provide_rx_buffer;
			}

			rx_len_cached = ++sys->len;

			if (rx_len_cached >= sys->rx_pool_sz) {
				spin_unlock_bh(
					&ipa3_ctx->wc_memb.wlan_spinlock);
				return;
			}
		}
	}
	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	if (rx_len_cached < sys->rx_pool_sz &&
			ipa3_ctx->wc_memb.wlan_comm_total_cnt <
			 IPA_WLAN_COMM_RX_POOL_HIGH) {
		ipa3_replenish_rx_cache(sys);
		ipa3_ctx->wc_memb.wlan_comm_total_cnt +=
			(sys->rx_pool_sz - rx_len_cached);
	}

	return;

fail_provide_rx_buffer:
	list_del(&rx_pkt->link);
	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
}

static void ipa3_cleanup_wlan_rx_common_cache(void)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct ipa3_rx_pkt_wrapper *tmp;

	spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	list_for_each_entry_safe(rx_pkt, tmp,
		&ipa3_ctx->wc_memb.wlan_comm_desc_list, link) {
		list_del(&rx_pkt->link);
		dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
				IPA_WLAN_RX_BUFF_SZ, DMA_FROM_DEVICE);
		dev_kfree_skb_any(rx_pkt->data.skb);
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
		ipa3_ctx->wc_memb.wlan_comm_free_cnt--;
		ipa3_ctx->wc_memb.wlan_comm_total_cnt--;
	}
	ipa3_ctx->wc_memb.total_tx_pkts_freed = 0;

	if (ipa3_ctx->wc_memb.wlan_comm_free_cnt != 0)
		IPAERR("wlan comm buff free cnt: %d\n",
			ipa3_ctx->wc_memb.wlan_comm_free_cnt);

	if (ipa3_ctx->wc_memb.wlan_comm_total_cnt != 0)
		IPAERR("wlan comm buff total cnt: %d\n",
			ipa3_ctx->wc_memb.wlan_comm_total_cnt);

	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

}

static void ipa3_alloc_wlan_rx_common_cache(u32 size)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int rx_len_cached = 0;
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = ipa3_ctx->wc_memb.wlan_comm_total_cnt;
	while (rx_len_cached < size) {
		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt)
			goto fail_kmem_cache_alloc;

		INIT_LIST_HEAD(&rx_pkt->link);
		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);

		rx_pkt->data.skb =
			ipa3_get_skb_ipa_rx(IPA_WLAN_RX_BUFF_SZ,
						flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, IPA_WLAN_RX_BUFF_SZ);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
				IPA_WLAN_RX_BUFF_SZ, DMA_FROM_DEVICE);
		if (dma_mapping_error(ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			IPAERR("dma_map_single failure %pK for %pK\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
		list_add_tail(&rx_pkt->link,
			&ipa3_ctx->wc_memb.wlan_comm_desc_list);
		rx_len_cached = ++ipa3_ctx->wc_memb.wlan_comm_total_cnt;

		ipa3_ctx->wc_memb.wlan_comm_free_cnt++;
		spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	}

	return;

fail_dma_mapping:
	dev_kfree_skb_any(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	return;
}


/**
 * ipa3_replenish_rx_cache() - Replenish the Rx packets cache.
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
 */
static void ipa3_replenish_rx_cache(struct ipa3_sys_context *sys)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int idx = 0;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = sys->len;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;


	while (rx_len_cached < sys->rx_pool_sz) {
		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt)
			goto fail_kmem_cache_alloc;

		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (dma_mapping_error(ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			IPAERR("dma_map_single failure %pK for %pK\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		idx++;
		rx_len_cached++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_MAX.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	goto done;

fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	if (rx_len_cached == 0)
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
done:
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
		gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}
}

static void ipa3_replenish_rx_cache_recycle(struct ipa3_sys_context *sys)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int idx = 0;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;

	rx_len_cached = sys->len;

	while (rx_len_cached < sys->rx_pool_sz) {
		if (list_empty(&sys->rcycl_list)) {
			rx_pkt = kmem_cache_zalloc(
				ipa3_ctx->rx_pkt_wrapper_cache, flag);
			if (!rx_pkt)
				goto fail_kmem_cache_alloc;

			INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
			rx_pkt->sys = sys;

			rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
			if (rx_pkt->data.skb == NULL) {
				IPAERR("failed to alloc skb\n");
				kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache,
					rx_pkt);
				goto fail_kmem_cache_alloc;
			}
			ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
			rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev,
				ptr, sys->rx_buff_sz, DMA_FROM_DEVICE);
			if (dma_mapping_error(ipa3_ctx->pdev,
				rx_pkt->data.dma_addr)) {
				IPAERR("dma_map_single failure %pK for %pK\n",
					(void *)rx_pkt->data.dma_addr, ptr);
				goto fail_dma_mapping;
			}
		} else {
			spin_lock_bh(&sys->spinlock);
			rx_pkt = list_first_entry(&sys->rcycl_list,
				struct ipa3_rx_pkt_wrapper, link);
			list_del(&rx_pkt->link);
			spin_unlock_bh(&sys->spinlock);
			ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
			rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev,
				ptr, sys->rx_buff_sz, DMA_FROM_DEVICE);
			if (dma_mapping_error(ipa3_ctx->pdev,
				rx_pkt->data.dma_addr)) {
				IPAERR("dma_map_single failure %pK for %pK\n",
					(void *)rx_pkt->data.dma_addr, ptr);
				goto fail_dma_mapping;
			}
		}

		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		idx++;
		rx_len_cached++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_MAX.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	goto done;
fail_dma_mapping:
	spin_lock_bh(&sys->spinlock);
	list_add_tail(&rx_pkt->link, &sys->rcycl_list);
	INIT_LIST_HEAD(&rx_pkt->link);
	spin_unlock_bh(&sys->spinlock);
fail_kmem_cache_alloc:
	if (rx_len_cached == 0)
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
		msecs_to_jiffies(1));
done:
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
		gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}
}

static void ipa3_fast_replenish_rx_cache(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	u32 curr;
	int idx = 0;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;

	spin_lock_bh(&sys->spinlock);
	rx_len_cached = sys->len;
	curr = atomic_read(&sys->repl->head_idx);

	while (rx_len_cached < sys->rx_pool_sz) {
		if (curr == atomic_read(&sys->repl->tail_idx))
			break;
		rx_pkt = sys->repl->cache[curr];
		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		rx_len_cached++;
		curr = (++curr == sys->repl->capacity) ? 0 : curr;
		idx++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_THRESH.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
			gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		/* ensure write is done before setting head index */
		mb();
		atomic_set(&sys->repl->head_idx, curr);
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}

	spin_unlock_bh(&sys->spinlock);

	__trigger_repl_work(sys);

	if (rx_len_cached <= IPA_DEFAULT_SYS_YELLOW_WM) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS ||
			sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_rx_empty);
		else
			WARN_ON(1);
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
	}
}

static void ipa3_replenish_rx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa3_sys_context *sys;

	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa3_sys_context, replenish_rx_work);
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	sys->repl_hdlr(sys);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
}

/**
 * free_rx_pkt() - function to free the skb and rx_pkt_wrapper
 *
 * @chan_user_data: ipa_sys_context used for skb size and skb_free func
 * @xfer_uder_data: rx_pkt wrapper to be freed
 *
 */
static void free_rx_pkt(void *chan_user_data, void *xfer_user_data)
{

	struct ipa3_rx_pkt_wrapper *rx_pkt = (struct ipa3_rx_pkt_wrapper *)
		xfer_user_data;
	struct ipa3_sys_context *sys = (struct ipa3_sys_context *)
		chan_user_data;

	dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
		sys->rx_buff_sz, DMA_FROM_DEVICE);
	sys->free_skb(rx_pkt->data.skb);
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
}

/**
 * free_rx_page() - function to free the page and rx_pkt_wrapper
 *
 * @chan_user_data: ipa_sys_context used for skb size and skb_free func
 * @xfer_uder_data: rx_pkt wrapper to be freed
 *
 */
static void free_rx_page(void *chan_user_data, void *xfer_user_data)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt = (struct ipa3_rx_pkt_wrapper *)
		xfer_user_data;
	struct ipa3_sys_context *sys = rx_pkt->sys;
	int i;

	for (i = 0; i < sys->page_recycle_repl->capacity; i++)
		if (sys->page_recycle_repl->cache[i] == rx_pkt)
			break;
	if (i < sys->page_recycle_repl->capacity) {
		page_ref_dec(rx_pkt->page_data.page);
		sys->page_recycle_repl->cache[i] = NULL;
	}
	dma_unmap_page(ipa3_ctx->pdev, rx_pkt->page_data.dma_addr,
		rx_pkt->len, DMA_FROM_DEVICE);
	__free_pages(rx_pkt->page_data.page,
		IPA_WAN_PAGE_ORDER);
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
}

/**
 * ipa3_cleanup_rx() - release RX queue resources
 *
 */
static void ipa3_cleanup_rx(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct ipa3_rx_pkt_wrapper *r;
	u32 head;
	u32 tail;
	int i;

	/*
	 * buffers not consumed by gsi are cleaned up using cleanup callback
	 * provided to gsi
	 */

	spin_lock_bh(&sys->spinlock);
	list_for_each_entry_safe(rx_pkt, r,
				 &sys->rcycl_list, link) {
		list_del(&rx_pkt->link);
		if (rx_pkt->data.dma_addr)
			dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
				sys->rx_buff_sz, DMA_FROM_DEVICE);
		else
			IPADBG("DMA address already freed\n");
		sys->free_skb(rx_pkt->data.skb);
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
	}
	spin_unlock_bh(&sys->spinlock);

	if (sys->repl) {
		head = atomic_read(&sys->repl->head_idx);
		tail = atomic_read(&sys->repl->tail_idx);
		while (head != tail) {
			rx_pkt = sys->repl->cache[head];
			if (sys->repl_hdlr != ipa3_replenish_rx_page_recycle) {
				dma_unmap_single(ipa3_ctx->pdev,
					rx_pkt->data.dma_addr,
					sys->rx_buff_sz,
					DMA_FROM_DEVICE);
				sys->free_skb(rx_pkt->data.skb);
			} else {
				dma_unmap_page(ipa3_ctx->pdev,
					rx_pkt->page_data.dma_addr,
					rx_pkt->len,
					DMA_FROM_DEVICE);
				__free_pages(rx_pkt->page_data.page,
					IPA_WAN_PAGE_ORDER);
			}
			kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache,
				rx_pkt);
			head = (head + 1) % sys->repl->capacity;
		}

		kfree(sys->repl->cache);
		kfree(sys->repl);
	}
	if (sys->page_recycle_repl) {
		for (i = 0; i < sys->page_recycle_repl->capacity; i++) {
			rx_pkt = sys->page_recycle_repl->cache[i];
			if (rx_pkt) {
				dma_unmap_page(ipa3_ctx->pdev,
					rx_pkt->page_data.dma_addr,
					rx_pkt->len,
					DMA_FROM_DEVICE);
				__free_pages(rx_pkt->page_data.page,
					IPA_WAN_PAGE_ORDER);
				kmem_cache_free(
					ipa3_ctx->rx_pkt_wrapper_cache,
					rx_pkt);
			}
		}
		kfree(sys->page_recycle_repl->cache);
		kfree(sys->page_recycle_repl);
	}
}

static struct sk_buff *ipa3_skb_copy_for_client(struct sk_buff *skb, int len)
{
	struct sk_buff *skb2 = NULL;

	if (!ipa3_ctx->lan_rx_napi_enable)
		skb2 = __dev_alloc_skb(len + IPA_RX_BUFF_CLIENT_HEADROOM,
					GFP_KERNEL);
	else
		skb2 = __dev_alloc_skb(len + IPA_RX_BUFF_CLIENT_HEADROOM,
					GFP_ATOMIC);

	if (likely(skb2)) {
		/* Set the data pointer */
		skb_reserve(skb2, IPA_RX_BUFF_CLIENT_HEADROOM);
		memcpy(skb2->data, skb->data, len);
		skb2->len = len;
		skb_set_tail_pointer(skb2, len);
	}

	return skb2;
}

static int ipa3_lan_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	int rc = 0;
	struct ipahal_pkt_status status;
	u32 pkt_status_sz;
	struct sk_buff *skb2;
	int pad_len_byte;
	int len;
	unsigned char *buf;
	int src_pipe;
	unsigned int used = *(unsigned int *)skb->cb;
	unsigned int used_align = ALIGN(used, 32);
	unsigned long unused = IPA_GENERIC_RX_BUFF_BASE_SZ - used;
	struct ipa3_tx_pkt_wrapper *tx_pkt = NULL;
	unsigned long ptr;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);

	if (skb->len == 0) {
		IPAERR("ZLT packet arrived to AP\n");
		goto out;
	}

	if (sys->len_partial) {
		IPADBG_LOW("len_partial %d\n", sys->len_partial);
		buf = skb_push(skb, sys->len_partial);
		memcpy(buf, sys->prev_skb->data, sys->len_partial);
		sys->len_partial = 0;
		sys->free_skb(sys->prev_skb);
		sys->prev_skb = NULL;
		goto begin;
	}

	/* this pipe has TX comp (status only) + mux-ed LAN RX data
	 * (status+data)
	 */
	if (sys->len_rem) {
		IPADBG_LOW("rem %d skb %d pad %d\n", sys->len_rem, skb->len,
				sys->len_pad);
		if (sys->len_rem <= skb->len) {
			if (sys->prev_skb) {
				if (!ipa3_ctx->lan_rx_napi_enable)
					skb2 = skb_copy_expand(sys->prev_skb,
						0, sys->len_rem, GFP_KERNEL);
				else
					skb2 = skb_copy_expand(sys->prev_skb,
						0, sys->len_rem, GFP_ATOMIC);
				if (likely(skb2)) {
					memcpy(skb_put(skb2, sys->len_rem),
						skb->data, sys->len_rem);
					skb_trim(skb2,
						skb2->len - sys->len_pad);
					skb2->truesize = skb2->len +
						sizeof(struct sk_buff);
					if (sys->drop_packet)
						dev_kfree_skb_any(skb2);
					else
						sys->ep->client_notify(
							sys->ep->priv,
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
				if (!ipa3_ctx->lan_rx_napi_enable)
					skb2 = skb_copy_expand(sys->prev_skb, 0,
						skb->len, GFP_KERNEL);
				else
					skb2 = skb_copy_expand(sys->prev_skb, 0,
						skb->len, GFP_ATOMIC);
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
			goto out;
		}
	}

begin:
	pkt_status_sz = ipahal_pkt_status_get_size();
	while (skb->len) {
		sys->drop_packet = false;
		IPADBG_LOW("LEN_REM %d\n", skb->len);

		if (skb->len < pkt_status_sz) {
			WARN_ON(sys->prev_skb != NULL);
			IPADBG_LOW("status straddles buffer\n");
			if (!ipa3_ctx->lan_rx_napi_enable)
				sys->prev_skb = skb_copy(skb, GFP_KERNEL);
			else
				sys->prev_skb = skb_copy(skb, GFP_ATOMIC);
			sys->len_partial = skb->len;
			goto out;
		}

		ipahal_pkt_status_parse(skb->data, &status);
		IPADBG_LOW("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.pkt_len);
		if (sys->status_stat) {
			sys->status_stat->status[sys->status_stat->curr] =
				status;
			sys->status_stat->curr++;
			if (sys->status_stat->curr == IPA_MAX_STATUS_STAT_NUM)
				sys->status_stat->curr = 0;
		}

		switch (status.status_opcode) {
		case IPAHAL_PKT_STATUS_OPCODE_DROPPED_PACKET:
		case IPAHAL_PKT_STATUS_OPCODE_PACKET:
		case IPAHAL_PKT_STATUS_OPCODE_SUSPENDED_PACKET:
		case IPAHAL_PKT_STATUS_OPCODE_PACKET_2ND_PASS:
			break;
		case IPAHAL_PKT_STATUS_OPCODE_NEW_FRAG_RULE:
			IPAERR_RL("Frag packets received on lan consumer\n");
			IPAERR_RL("STATUS opcode=%d src=%d dst=%d src ip=%x\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.src_ip_addr);
			skb_pull(skb, pkt_status_sz);
			continue;
		default:
			IPAERR_RL("unsupported opcode(%d)\n",
				status.status_opcode);
			skb_pull(skb, pkt_status_sz);
			continue;
		}

		IPA_STATS_EXCP_CNT(status.exception,
				ipa3_ctx->stats.rx_excp_pkts);
		if (status.endp_dest_idx >= ipa3_ctx->ipa_num_pipes ||
			status.endp_src_idx >= ipa3_ctx->ipa_num_pipes) {
			IPAERR_RL("status fields invalid\n");
			IPAERR_RL("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.pkt_len);
			WARN_ON(1);
			/* HW gave an unexpected status */
			ipa_assert();
		}
		if (IPAHAL_PKT_STATUS_MASK_FLAG_VAL(
			IPAHAL_PKT_STATUS_MASK_TAG_VALID_SHFT, &status)) {
			struct ipa3_tag_completion *comp;

			IPADBG_LOW("TAG packet arrived\n");
			if (status.tag_info == IPA_COOKIE) {
				skb_pull(skb, pkt_status_sz);
				if (skb->len < sizeof(comp)) {
					IPAERR("TAG arrived without packet\n");
					goto out;
				}
				memcpy(&comp, skb->data, sizeof(comp));
				skb_pull(skb, sizeof(comp));
				complete(&comp->comp);
				if (atomic_dec_return(&comp->cnt) == 0)
					kfree(comp);
				continue;
			} else {
				ptr = tag_to_pointer_wa(status.tag_info);
				tx_pkt = (struct ipa3_tx_pkt_wrapper *)ptr;
				IPADBG_LOW("tx_pkt recv = %pK\n", tx_pkt);
			}
		}
		if (status.pkt_len == 0) {
			IPADBG_LOW("Skip aggr close status\n");
			skb_pull(skb, pkt_status_sz);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.aggr_close);
			IPA_STATS_DEC_CNT(ipa3_ctx->stats.rx_excp_pkts
				[IPAHAL_PKT_STATUS_EXCEPTION_NONE]);
			continue;
		}

		if (status.endp_dest_idx == (sys->ep - ipa3_ctx->ep)) {
			/* RX data */
			src_pipe = status.endp_src_idx;

			/*
			 * A packet which is received back to the AP after
			 * there was no route match.
			 */
			if (status.exception ==
				IPAHAL_PKT_STATUS_EXCEPTION_NONE &&
				ipahal_is_rule_miss_id(status.rt_rule_id))
				sys->drop_packet = true;

			if (skb->len == pkt_status_sz &&
				status.exception ==
				IPAHAL_PKT_STATUS_EXCEPTION_NONE) {
				WARN_ON(sys->prev_skb != NULL);
				IPADBG_LOW("Ins header in next buffer\n");
				if (!ipa3_ctx->lan_rx_napi_enable)
					sys->prev_skb = skb_copy(skb,
						GFP_KERNEL);
				else
					sys->prev_skb = skb_copy(skb,
						GFP_ATOMIC);
				sys->len_partial = skb->len;
				goto out;
			}

			pad_len_byte = ((status.pkt_len + 3) & ~3) -
					status.pkt_len;
			len = status.pkt_len + pad_len_byte;
			IPADBG_LOW("pad %d pkt_len %d len %d\n", pad_len_byte,
					status.pkt_len, len);

			if (status.exception ==
					IPAHAL_PKT_STATUS_EXCEPTION_DEAGGR) {
				IPADBG_LOW(
					"Dropping packet on DeAggr Exception\n");
				sys->drop_packet = true;
			}

			skb2 = ipa3_skb_copy_for_client(skb,
				min(status.pkt_len + pkt_status_sz, skb->len));
			if (likely(skb2)) {
				if (skb->len < len + pkt_status_sz) {
					IPADBG_LOW("SPL skb len %d len %d\n",
							skb->len, len);
					sys->prev_skb = skb2;
					sys->len_rem = len - skb->len +
						pkt_status_sz;
					sys->len_pad = pad_len_byte;
					skb_pull(skb, skb->len);
				} else {
					skb_trim(skb2, status.pkt_len +
							pkt_status_sz);
					IPADBG_LOW("rx avail for %d\n",
							status.endp_dest_idx);
					if (sys->drop_packet) {
						dev_kfree_skb_any(skb2);
					} else if (status.pkt_len >
						   IPA_GENERIC_AGGR_BYTE_LIMIT *
						   1024) {
						IPAERR("packet size invalid\n");
						IPAERR("STATUS opcode=%d\n",
							status.status_opcode);
						IPAERR("src=%d dst=%d len=%d\n",
							status.endp_src_idx,
							status.endp_dest_idx,
							status.pkt_len);
						/* Unexpected HW status */
						ipa_assert();
					} else {
						skb2->truesize = skb2->len +
						sizeof(struct sk_buff) +
						(ALIGN(len +
						pkt_status_sz, 32) *
						unused / used_align);
						sys->ep->client_notify(
							sys->ep->priv,
							IPA_RECEIVE,
							(unsigned long)(skb2));
					}
					skb_pull(skb, len + pkt_status_sz);
				}
			} else {
				IPAERR("fail to alloc skb\n");
				if (skb->len < len) {
					sys->prev_skb = NULL;
					sys->len_rem = len - skb->len +
						pkt_status_sz;
					sys->len_pad = pad_len_byte;
					skb_pull(skb, skb->len);
				} else {
					skb_pull(skb, len + pkt_status_sz);
				}
			}
			/* TX comp */
			ipa3_wq_write_done_status(src_pipe, tx_pkt);
			IPADBG_LOW("tx comp imp for %d\n", src_pipe);
		} else {
			/* TX comp */
			ipa3_wq_write_done_status(status.endp_src_idx, tx_pkt);
			IPADBG_LOW("tx comp exp for %d\n",
				status.endp_src_idx);
			skb_pull(skb, pkt_status_sz);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.stat_compl);
			IPA_STATS_DEC_CNT(ipa3_ctx->stats.rx_excp_pkts
				[IPAHAL_PKT_STATUS_EXCEPTION_NONE]);
		}
		tx_pkt = NULL;
	}

out:
	ipa3_skb_recycle(skb);
	return rc;
}

static struct sk_buff *ipa3_join_prev_skb(struct sk_buff *prev_skb,
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

static void ipa3_wan_rx_handle_splt_pyld(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	struct sk_buff *skb2;

	IPADBG_LOW("rem %d skb %d\n", sys->len_rem, skb->len);
	if (sys->len_rem <= skb->len) {
		if (sys->prev_skb) {
			skb2 = ipa3_join_prev_skb(sys->prev_skb, skb,
					sys->len_rem);
			if (likely(skb2)) {
				IPADBG_LOW(
					"removing Status element from skb and sending to WAN client");
				skb_pull(skb2, ipahal_pkt_status_get_size());
				skb2->truesize = skb2->len +
					sizeof(struct sk_buff);
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
			skb2 = ipa3_join_prev_skb(sys->prev_skb, skb,
					skb->len);
			sys->prev_skb = skb2;
		}
		sys->len_rem -= skb->len;
		skb_pull(skb, skb->len);
	}
}

static int ipa3_wan_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	struct ipahal_pkt_status status;
	unsigned char *skb_data;
	u32 pkt_status_sz;
	struct sk_buff *skb2;
	u16 pkt_len_with_pad;
	u32 qmap_hdr;
	int checksum_trailer_exists;
	int frame_len;
	int ep_idx;
	unsigned int used = *(unsigned int *)skb->cb;
	unsigned int used_align = ALIGN(used, 32);
	unsigned long unused = IPA_GENERIC_RX_BUFF_BASE_SZ - used;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);
	if (skb->len == 0) {
		IPAERR("ZLT\n");
		goto bail;
	}

	if (ipa3_ctx->ipa_client_apps_wan_cons_agg_gro) {
		sys->ep->client_notify(sys->ep->priv,
			IPA_RECEIVE, (unsigned long)(skb));
		return 0;
	}
	if (sys->repl_hdlr == ipa3_replenish_rx_cache_recycle) {
		IPAERR("Recycle should enable only with GRO Aggr\n");
		ipa_assert();
	}

	/*
	 * payload splits across 2 buff or more,
	 * take the start of the payload from prev_skb
	 */
	if (sys->len_rem)
		ipa3_wan_rx_handle_splt_pyld(skb, sys);

	pkt_status_sz = ipahal_pkt_status_get_size();
	while (skb->len) {
		IPADBG_LOW("LEN_REM %d\n", skb->len);
		if (skb->len < pkt_status_sz) {
			IPAERR("status straddles buffer\n");
			WARN_ON(1);
			goto bail;
		}
		ipahal_pkt_status_parse(skb->data, &status);
		skb_data = skb->data;
		IPADBG_LOW("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.pkt_len);

		if (sys->status_stat) {
			sys->status_stat->status[sys->status_stat->curr] =
				status;
			sys->status_stat->curr++;
			if (sys->status_stat->curr == IPA_MAX_STATUS_STAT_NUM)
				sys->status_stat->curr = 0;
		}

		if ((status.status_opcode !=
			IPAHAL_PKT_STATUS_OPCODE_DROPPED_PACKET) &&
			(status.status_opcode !=
			IPAHAL_PKT_STATUS_OPCODE_PACKET) &&
			(status.status_opcode !=
			IPAHAL_PKT_STATUS_OPCODE_PACKET_2ND_PASS)) {
			IPAERR("unsupported opcode(%d)\n",
				status.status_opcode);
			skb_pull(skb, pkt_status_sz);
			continue;
		}

		IPA_STATS_INC_CNT(ipa3_ctx->stats.rx_pkts);
		if (status.endp_dest_idx >= ipa3_ctx->ipa_num_pipes ||
			status.endp_src_idx >= ipa3_ctx->ipa_num_pipes) {
			IPAERR("status fields invalid\n");
			WARN_ON(1);
			goto bail;
		}
		if (status.pkt_len == 0) {
			IPADBG_LOW("Skip aggr close status\n");
			skb_pull(skb, pkt_status_sz);
			IPA_STATS_DEC_CNT(ipa3_ctx->stats.rx_pkts);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_aggr_close);
			continue;
		}
		ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
		if (status.endp_dest_idx != ep_idx) {
			IPAERR("expected endp_dest_idx %d received %d\n",
					ep_idx, status.endp_dest_idx);
			WARN_ON(1);
			goto bail;
		}
		/* RX data */
		if (skb->len == pkt_status_sz) {
			IPAERR("Ins header in next buffer\n");
			WARN_ON(1);
			goto bail;
		}
		qmap_hdr = *(u32 *)(skb_data + pkt_status_sz);
		/*
		 * Take the pkt_len_with_pad from the last 2 bytes of the QMAP
		 * header
		 */

		/*QMAP is BE: convert the pkt_len field from BE to LE*/
		pkt_len_with_pad = ntohs((qmap_hdr>>16) & 0xffff);
		IPADBG_LOW("pkt_len with pad %d\n", pkt_len_with_pad);
		/*get the CHECKSUM_PROCESS bit*/
		checksum_trailer_exists = IPAHAL_PKT_STATUS_MASK_FLAG_VAL(
			IPAHAL_PKT_STATUS_MASK_CKSUM_PROCESS_SHFT, &status);
		IPADBG_LOW("checksum_trailer_exists %d\n",
				checksum_trailer_exists);

		frame_len = pkt_status_sz + IPA_QMAP_HEADER_LENGTH +
			    pkt_len_with_pad;
		if (checksum_trailer_exists)
			frame_len += IPA_DL_CHECKSUM_LENGTH;
		IPADBG_LOW("frame_len %d\n", frame_len);

		skb2 = skb_clone(skb, GFP_KERNEL);
		if (likely(skb2)) {
			/*
			 * the len of actual data is smaller than expected
			 * payload split across 2 buff
			 */
			if (skb->len < frame_len) {
				IPADBG_LOW("SPL skb len %d len %d\n",
						skb->len, frame_len);
				sys->prev_skb = skb2;
				sys->len_rem = frame_len - skb->len;
				skb_pull(skb, skb->len);
			} else {
				skb_trim(skb2, frame_len);
				IPADBG_LOW("rx avail for %d\n",
						status.endp_dest_idx);
				IPADBG_LOW(
					"removing Status element from skb and sending to WAN client");
				skb_pull(skb2, pkt_status_sz);
				skb2->truesize = skb2->len +
					sizeof(struct sk_buff) +
					(ALIGN(frame_len, 32) *
					 unused / used_align);
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
	}
bail:
	sys->free_skb(skb);
	return 0;
}

static struct sk_buff *ipa3_get_skb_ipa_rx(unsigned int len, gfp_t flags)
{
	return __dev_alloc_skb(len, flags);
}

static void ipa3_free_skb_rx(struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
}

void ipa3_lan_rx_cb(void *priv, enum ipa_dp_evt_type evt, unsigned long data)
{
	struct sk_buff *rx_skb = (struct sk_buff *)data;
	struct ipahal_pkt_status_thin status;
	struct ipa3_ep_context *ep;
	unsigned int src_pipe;
	u32 metadata;
	u8 ucp;
	void (*client_notify)(void *client_priv, enum ipa_dp_evt_type evt,
		       unsigned long data);
	void *client_priv;

	ipahal_pkt_status_parse_thin(rx_skb->data, &status);
	src_pipe = status.endp_src_idx;
	metadata = status.metadata;
	ucp = status.ucp;
	ep = &ipa3_ctx->ep[src_pipe];
	if (unlikely(src_pipe >= ipa3_ctx->ipa_num_pipes) ||
		unlikely(atomic_read(&ep->disconnect_in_progress))) {
		IPAERR("drop pipe=%d\n", src_pipe);
		dev_kfree_skb_any(rx_skb);
		return;
	}
	if (status.exception == IPAHAL_PKT_STATUS_EXCEPTION_NONE)
		skb_pull(rx_skb, ipahal_pkt_status_get_size() +
				IPA_LAN_RX_HEADER_LENGTH);
	else
		skb_pull(rx_skb, ipahal_pkt_status_get_size());

	/* Metadata Info
	 *  ------------------------------------------
	 *  |   3     |   2     |    1        |  0   |
	 *  | fw_desc | vdev_id | qmap mux id | Resv |
	 *  ------------------------------------------
	 */
	*(u16 *)rx_skb->cb = ((metadata >> 16) & 0xFFFF);
	*(u8 *)(rx_skb->cb + 4) = ucp;
	IPADBG_LOW("meta_data: 0x%x cb: 0x%x\n",
			metadata, *(u32 *)rx_skb->cb);
	IPADBG_LOW("ucp: %d\n", *(u8 *)(rx_skb->cb + 4));

	spin_lock(&ipa3_ctx->disconnect_lock);
	if (likely((!atomic_read(&ep->disconnect_in_progress)) &&
				ep->valid && ep->client_notify)) {
		client_notify = ep->client_notify;
		client_priv = ep->priv;
		spin_unlock(&ipa3_ctx->disconnect_lock);
		client_notify(client_priv, IPA_RECEIVE,
				(unsigned long)(rx_skb));
	} else {
		spin_unlock(&ipa3_ctx->disconnect_lock);
		dev_kfree_skb_any(rx_skb);
	}

}

static void ipa3_recycle_rx_wrapper(struct ipa3_rx_pkt_wrapper *rx_pkt)
{
	rx_pkt->data.dma_addr = 0;
	/* skb recycle was moved to pyld_hdlr */
	INIT_LIST_HEAD(&rx_pkt->link);
	spin_lock_bh(&rx_pkt->sys->spinlock);
	list_add_tail(&rx_pkt->link, &rx_pkt->sys->rcycl_list);
	spin_unlock_bh(&rx_pkt->sys->spinlock);
}

static void ipa3_recycle_rx_page_wrapper(struct ipa3_rx_pkt_wrapper *rx_pkt)
{
	struct ipa_rx_page_data rx_page;

	rx_page = rx_pkt->page_data;

	/* Free rx_wrapper only for tmp alloc pages*/
	if (rx_page.is_tmp_alloc)
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
}

/**
 * handle_skb_completion()- Handle event completion EOB or EOT and prep the skb
 *
 * if eob: Set skb values, put rx_pkt at the end of the list and return NULL
 *
 * if eot: Set skb values, put skb at the end of the list. Then update the
 * length and chain the skbs together while also freeing and unmapping the
 * corresponding rx pkt. Once finished return the head_skb to be sent up the
 * network stack.
 */
static struct sk_buff *handle_skb_completion(struct gsi_chan_xfer_notify
		*notify, bool update_truesize)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt, *tmp;
	struct sk_buff *rx_skb, *next_skb = NULL;
	struct list_head *head;
	struct ipa3_sys_context *sys;

	sys = (struct ipa3_sys_context *) notify->chan_user_data;
	rx_pkt = (struct ipa3_rx_pkt_wrapper *) notify->xfer_user_data;

	spin_lock_bh(&rx_pkt->sys->spinlock);
	rx_pkt->sys->len--;
	spin_unlock_bh(&rx_pkt->sys->spinlock);

	if (notify->bytes_xfered)
		rx_pkt->len = notify->bytes_xfered;

	/*Drop packets when WAN consumer channel receive EOB event*/
	if ((notify->evt_id == GSI_CHAN_EVT_EOB ||
		sys->skip_eot) &&
		sys->ep->client == IPA_CLIENT_APPS_WAN_CONS) {
		dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
			sys->rx_buff_sz, DMA_FROM_DEVICE);
		sys->free_skb(rx_pkt->data.skb);
		sys->free_rx_wrapper(rx_pkt);
		sys->eob_drop_cnt++;
		if (notify->evt_id == GSI_CHAN_EVT_EOB) {
			IPADBG("EOB event on WAN consumer channel, drop\n");
			sys->skip_eot = true;
		} else {
			IPADBG("Reset skip eot flag.\n");
			sys->skip_eot = false;
		}
		return NULL;
	}

	rx_skb = rx_pkt->data.skb;
	skb_set_tail_pointer(rx_skb, rx_pkt->len);
	rx_skb->len = rx_pkt->len;

	if (update_truesize) {
		*(unsigned int *)rx_skb->cb = rx_skb->len;
		rx_skb->truesize = rx_pkt->len + sizeof(struct sk_buff);
	}

	if (unlikely(notify->veid >= GSI_VEID_MAX)) {
		WARN_ON(1);
		return NULL;
	}

	head = &rx_pkt->sys->pending_pkts[notify->veid];

	INIT_LIST_HEAD(&rx_pkt->link);
	list_add_tail(&rx_pkt->link, head);

	/* Check added for handling LAN consumer packet without EOT flag */
	if (notify->evt_id == GSI_CHAN_EVT_EOT ||
		sys->ep->client == IPA_CLIENT_APPS_LAN_CONS) {
	/* go over the list backward to save computations on updating length */
		list_for_each_entry_safe_reverse(rx_pkt, tmp, head, link) {
			rx_skb = rx_pkt->data.skb;

			list_del(&rx_pkt->link);
			dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
				sys->rx_buff_sz, DMA_FROM_DEVICE);
			sys->free_rx_wrapper(rx_pkt);

			if (next_skb) {
				skb_shinfo(rx_skb)->frag_list = next_skb;
				rx_skb->len += next_skb->len;
				rx_skb->data_len += next_skb->len;
			}
			next_skb = rx_skb;
		}
	} else {
		return NULL;
	}
	return rx_skb;
}

/**
 * handle_page_completion()- Handle event completion EOB or EOT
 * and prep the skb
 *
 * if eob: Set skb values, put rx_pkt at the end of the list and return NULL
 *
 * if eot: Set skb values, put skb at the end of the list. Then update the
 * length and put the page together to the frags while also
 * freeing and unmapping the corresponding rx pkt. Once finished
 * return the head_skb to be sent up the network stack.
 */
static struct sk_buff *handle_page_completion(struct gsi_chan_xfer_notify
		*notify, bool update_truesize)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt, *tmp;
	struct sk_buff *rx_skb;
	struct list_head *head;
	struct ipa3_sys_context *sys;
	struct ipa_rx_page_data rx_page;

	sys = (struct ipa3_sys_context *) notify->chan_user_data;
	rx_pkt = (struct ipa3_rx_pkt_wrapper *) notify->xfer_user_data;
	rx_page = rx_pkt->page_data;

	spin_lock_bh(&rx_pkt->sys->spinlock);
	rx_pkt->sys->len--;
	spin_unlock_bh(&rx_pkt->sys->spinlock);

	/* TODO: truesize handle for EOB */
	if (update_truesize)
		IPAERR("update_truesize not supported\n");

	if (notify->veid >= GSI_VEID_MAX) {
		IPAERR("notify->veid > GSI_VEID_MAX\n");
		if (!rx_page.is_tmp_alloc) {
			init_page_count(rx_page.page);
		} else {
			dma_unmap_page(ipa3_ctx->pdev, rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
			__free_pages(rx_pkt->page_data.page,
							IPA_WAN_PAGE_ORDER);
		}
		rx_pkt->sys->free_rx_wrapper(rx_pkt);
		IPA_STATS_INC_CNT(ipa3_ctx->stats.rx_page_drop_cnt);
		return NULL;
	}

	head = &rx_pkt->sys->pending_pkts[notify->veid];

	INIT_LIST_HEAD(&rx_pkt->link);
	list_add_tail(&rx_pkt->link, head);

	/* Check added for handling LAN consumer packet without EOT flag */
	if (notify->evt_id == GSI_CHAN_EVT_EOT ||
		sys->ep->client == IPA_CLIENT_APPS_LAN_CONS) {
		rx_skb = alloc_skb(0, GFP_ATOMIC);
		if (unlikely(!rx_skb)) {
			IPAERR("skb alloc failure\n");
			list_del(&rx_pkt->link);
			if (!rx_page.is_tmp_alloc) {
				init_page_count(rx_page.page);
			} else {
				dma_unmap_page(ipa3_ctx->pdev, rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
				__free_pages(rx_pkt->page_data.page,
							IPA_WAN_PAGE_ORDER);
			}
			rx_pkt->sys->free_rx_wrapper(rx_pkt);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.rx_page_drop_cnt);
			return NULL;
		}
	/* go over the list backward to save computations on updating length */
		list_for_each_entry_safe_reverse(rx_pkt, tmp, head, link) {
			rx_page = rx_pkt->page_data;

			list_del(&rx_pkt->link);
			if (rx_page.is_tmp_alloc)
				dma_unmap_page(ipa3_ctx->pdev, rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
			else
				dma_sync_single_for_cpu(ipa3_ctx->pdev,
					rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
			rx_pkt->sys->free_rx_wrapper(rx_pkt);

			skb_add_rx_frag(rx_skb,
				skb_shinfo(rx_skb)->nr_frags,
				rx_page.page, 0,
				notify->bytes_xfered,
				PAGE_SIZE << IPA_WAN_PAGE_ORDER);
		}
	} else {
		return NULL;
	}
	return rx_skb;
}

static void ipa3_wq_rx_common(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify)
{
	struct sk_buff *rx_skb;
	struct ipa3_sys_context *coal_sys;
	int ipa_ep_idx;

	if (unlikely(!notify)) {
		IPAERR_RL("gsi_chan_xfer_notify is null\n");
		return;
	}
	rx_skb = handle_skb_completion(notify, true);

	if (likely(rx_skb)) {
		sys->pyld_hdlr(rx_skb, sys);

		/* For coalescing, we have 2 transfer rings to replenish */
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
			ipa_ep_idx = ipa3_get_ep_mapping(
					IPA_CLIENT_APPS_WAN_CONS);

			if (unlikely(ipa_ep_idx == IPA_EP_NOT_ALLOCATED)) {
				IPAERR("Invalid client.\n");
				return;
			}

			coal_sys = ipa3_ctx->ep[ipa_ep_idx].sys;
			coal_sys->repl_hdlr(coal_sys);
		}

		sys->repl_hdlr(sys);
	}
}

static void ipa3_rx_napi_chain(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify, uint32_t num)
{
	struct ipa3_sys_context *wan_def_sys;
	int i, ipa_ep_idx;
	struct sk_buff *rx_skb, *first_skb = NULL, *prev_skb = NULL;

	/* non-coalescing case (SKB chaining enabled) */
	if (sys->ep->client != IPA_CLIENT_APPS_WAN_COAL_CONS) {
		for (i = 0; i < num; i++) {
			if (!ipa3_ctx->ipa_wan_skb_page)
				rx_skb = handle_skb_completion(
					&notify[i], false);
			else
				rx_skb = handle_page_completion(
					&notify[i], false);

			/* this is always true for EOTs */
			if (rx_skb) {
				if (!first_skb)
					first_skb = rx_skb;

				if (prev_skb)
					skb_shinfo(prev_skb)->frag_list =
						rx_skb;

				prev_skb = rx_skb;
			}
		}
		if (prev_skb) {
			skb_shinfo(prev_skb)->frag_list = NULL;
			sys->pyld_hdlr(first_skb, sys);
		}
	} else {
		if (!ipa3_ctx->ipa_wan_skb_page) {
			/* TODO: add chaining for coal case */
			for (i = 0; i < num; i++) {
				rx_skb = handle_skb_completion(
					&notify[i], false);
				if (rx_skb) {
					sys->pyld_hdlr(rx_skb, sys);
					/*
					 * For coalescing, we have 2 transfer
					 * rings to replenish
					 */
					ipa_ep_idx = ipa3_get_ep_mapping(
						IPA_CLIENT_APPS_WAN_CONS);
					if (unlikely(ipa_ep_idx ==
						IPA_EP_NOT_ALLOCATED)) {
						IPAERR("Invalid client.\n");
						return;
					}
					wan_def_sys =
						ipa3_ctx->ep[ipa_ep_idx].sys;
					wan_def_sys->repl_hdlr(wan_def_sys);
					sys->repl_hdlr(sys);
				}
			}
		} else {
			for (i = 0; i < num; i++) {
				rx_skb = handle_page_completion(
					&notify[i], false);

				/* this is always true for EOTs */
				if (rx_skb) {
					if (!first_skb)
						first_skb = rx_skb;

					if (prev_skb)
						skb_shinfo(prev_skb)->frag_list
							= rx_skb;

					prev_skb = rx_skb;
				}
			}
			if (prev_skb) {
				skb_shinfo(prev_skb)->frag_list = NULL;
				sys->pyld_hdlr(first_skb, sys);
				/*
				 * For coalescing, we have 2 transfer
				 * rings to replenish
				 */
				ipa_ep_idx = ipa3_get_ep_mapping(
						IPA_CLIENT_APPS_WAN_CONS);
				if (unlikely(ipa_ep_idx ==
					IPA_EP_NOT_ALLOCATED)) {
					IPAERR("Invalid client.\n");
					return;
				}
				wan_def_sys =
					ipa3_ctx->ep[ipa_ep_idx].sys;
				wan_def_sys->repl_hdlr(wan_def_sys);
			}
		}
	}
}

static void ipa3_wlan_wq_rx_common(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt_expected;
	struct sk_buff *rx_skb;

	rx_pkt_expected = (struct ipa3_rx_pkt_wrapper *) notify->xfer_user_data;

	sys->len--;

	if (notify->bytes_xfered)
		rx_pkt_expected->len = notify->bytes_xfered;

	rx_skb = rx_pkt_expected->data.skb;
	skb_set_tail_pointer(rx_skb, rx_pkt_expected->len);
	rx_skb->len = rx_pkt_expected->len;
	rx_skb->truesize = rx_pkt_expected->len + sizeof(struct sk_buff);
	sys->ep->wstats.tx_pkts_rcvd++;
	if (sys->len <= IPA_WLAN_RX_POOL_SZ_LOW_WM) {
		ipa3_free_skb(&rx_pkt_expected->data);
		sys->ep->wstats.tx_pkts_dropped++;
	} else {
		sys->ep->wstats.tx_pkts_sent++;
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
				(unsigned long)(&rx_pkt_expected->data));
	}
	ipa3_replenish_wlan_rx_cache(sys);
}

static void ipa3_dma_memcpy_notify(struct ipa3_sys_context *sys)
{
	IPADBG_LOW("ENTER.\n");
	if (unlikely(list_empty(&sys->head_desc_list))) {
		IPAERR("descriptor list is empty!\n");
		WARN_ON(1);
		return;
	}
	sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE, 0);
	IPADBG_LOW("EXIT\n");
}

static void ipa3_wq_rx_avail(struct work_struct *work)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct ipa3_sys_context *sys;

	rx_pkt = container_of(work, struct ipa3_rx_pkt_wrapper, work);
	WARN(unlikely(rx_pkt == NULL), "rx pkt is null");
	sys = rx_pkt->sys;
	ipa3_wq_rx_common(sys, 0);
}

static int ipa3_odu_rx_pyld_hdlr(struct sk_buff *rx_skb,
	struct ipa3_sys_context *sys)
{
	if (sys->ep->client_notify) {
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
			(unsigned long)(rx_skb));
	} else {
		dev_kfree_skb_any(rx_skb);
		WARN(1, "client notify is null");
	}

	return 0;
}

static int ipa3_odl_dpl_rx_pyld_hdlr(struct sk_buff *rx_skb,
	struct ipa3_sys_context *sys)
{
	if (WARN(!sys->ep->client_notify, "sys->ep->client_notify is NULL\n")) {
		dev_kfree_skb_any(rx_skb);
	} else {
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
			(unsigned long)(rx_skb));
		/*Recycle the SKB before reusing it*/
		ipa3_skb_recycle(rx_skb);
	}

	return 0;
}
static void ipa3_free_rx_wrapper(struct ipa3_rx_pkt_wrapper *rk_pkt)
{
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rk_pkt);
}

static void ipa3_set_aggr_limit(struct ipa_sys_connect_params *in,
		struct ipa3_sys_context *sys)
{
	u32 *aggr_byte_limit = &in->ipa_ep_cfg.aggr.aggr_byte_limit;
	u32 adjusted_sz = ipa_adjust_ra_buff_base_sz(*aggr_byte_limit);

	IPADBG("get close-by %u\n", adjusted_sz);
	IPADBG("set rx_buff_sz %lu\n", (unsigned long)
		IPA_GENERIC_RX_BUFF_SZ(adjusted_sz));

	/* disable ipa_status */
	sys->ep->status.status_en = false;
	sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(adjusted_sz);

	if (in->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
		in->ipa_ep_cfg.aggr.aggr_hard_byte_limit_en = 1;

	*aggr_byte_limit = sys->rx_buff_sz < *aggr_byte_limit ?
		IPA_ADJUST_AGGR_BYTE_LIMIT(sys->rx_buff_sz) :
		IPA_ADJUST_AGGR_BYTE_LIMIT(*aggr_byte_limit);

	IPADBG("set aggr_limit %lu\n", (unsigned long) *aggr_byte_limit);
}

static int ipa3_assign_policy(struct ipa_sys_connect_params *in,
		struct ipa3_sys_context *sys)
{
	bool apps_wan_cons_agg_gro_flag;
	unsigned long aggr_byte_limit;

	if (in->client == IPA_CLIENT_APPS_CMD_PROD) {
		sys->policy = IPA_POLICY_INTR_MODE;
		sys->use_comm_evt_ring = false;
		return 0;
	}

	if (in->client == IPA_CLIENT_APPS_WAN_PROD) {
		sys->policy = IPA_POLICY_INTR_MODE;
		sys->use_comm_evt_ring = true;
		INIT_WORK(&sys->work, ipa3_send_nop_desc);
		atomic_set(&sys->workqueue_flushed, 0);

		/*
		 * enable source notification status for exception packets
		 * (i.e. QMAP commands) to be routed to modem.
		 */
		sys->ep->status.status_en = true;
		sys->ep->status.status_ep =
			ipa3_get_ep_mapping(IPA_CLIENT_Q6_WAN_CONS);
		return 0;
	}

	if (IPA_CLIENT_IS_MEMCPY_DMA_PROD(in->client)) {
		sys->policy = IPA_POLICY_NOINTR_MODE;
		return 0;
	}

	apps_wan_cons_agg_gro_flag =
		ipa3_ctx->ipa_client_apps_wan_cons_agg_gro;
	aggr_byte_limit = in->ipa_ep_cfg.aggr.aggr_byte_limit;

	if (IPA_CLIENT_IS_PROD(in->client)) {
		if (sys->ep->skip_ep_cfg) {
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			sys->use_comm_evt_ring = true;
			atomic_set(&sys->curr_polling_state, 0);
		} else {
			sys->policy = IPA_POLICY_INTR_MODE;
			sys->use_comm_evt_ring = true;
			INIT_WORK(&sys->work, ipa3_send_nop_desc);
			atomic_set(&sys->workqueue_flushed, 0);
		}
	} else {
		if (in->client == IPA_CLIENT_APPS_LAN_CONS ||
		    in->client == IPA_CLIENT_APPS_WAN_CONS ||
		    in->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
			sys->ep->status.status_en = true;
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
					ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(
				IPA_GENERIC_RX_BUFF_BASE_SZ);
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa3_free_skb_rx;
			in->ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
			if (in->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
				in->ipa_ep_cfg.aggr.aggr = IPA_COALESCE;
			else
				in->ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
			in->ipa_ep_cfg.aggr.aggr_time_limit =
				IPA_GENERIC_AGGR_TIME_LIMIT;
			if (in->client == IPA_CLIENT_APPS_LAN_CONS) {
				INIT_WORK(&sys->repl_work, ipa3_wq_repl_rx);
				sys->pyld_hdlr = ipa3_lan_rx_pyld_hdlr;
				sys->repl_hdlr =
					ipa3_replenish_rx_cache_recycle;
				sys->free_rx_wrapper =
					ipa3_recycle_rx_wrapper;
				sys->rx_pool_sz =
					ipa3_ctx->lan_rx_ring_size;
				in->ipa_ep_cfg.aggr.aggr_byte_limit =
				IPA_GENERIC_AGGR_BYTE_LIMIT;
				in->ipa_ep_cfg.aggr.aggr_pkt_limit =
				IPA_GENERIC_AGGR_PKT_LIMIT;
			} else if (in->client == IPA_CLIENT_APPS_WAN_CONS ||
				in->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
				if (ipa3_ctx->ipa_wan_skb_page
					&& in->napi_obj) {
					INIT_WORK(&sys->repl_work,
							ipa3_wq_page_repl);
					sys->pyld_hdlr = ipa3_wan_rx_pyld_hdlr;
					sys->free_rx_wrapper =
						ipa3_recycle_rx_page_wrapper;
					sys->repl_hdlr =
						ipa3_replenish_rx_page_recycle;
					sys->rx_pool_sz =
						ipa3_ctx->wan_rx_ring_size;
				} else {
					INIT_WORK(&sys->repl_work,
						ipa3_wq_repl_rx);
					sys->pyld_hdlr = ipa3_wan_rx_pyld_hdlr;
					sys->free_rx_wrapper =
						ipa3_free_rx_wrapper;
					sys->rx_pool_sz =
						ipa3_ctx->wan_rx_ring_size;
					if (nr_cpu_ids > 1) {
						sys->repl_hdlr =
						ipa3_fast_replenish_rx_cache;
					} else {
						sys->repl_hdlr =
						ipa3_replenish_rx_cache;
					}
					if (in->napi_obj && in->recycle_enabled)
						sys->repl_hdlr =
						ipa3_replenish_rx_cache_recycle;
				}
				in->ipa_ep_cfg.aggr.aggr_sw_eof_active
						= true;
				if (apps_wan_cons_agg_gro_flag)
					ipa3_set_aggr_limit(in, sys);
				else {
					in->ipa_ep_cfg.aggr.aggr_byte_limit
						= IPA_GENERIC_AGGR_BYTE_LIMIT;
					in->ipa_ep_cfg.aggr.aggr_pkt_limit
						= IPA_GENERIC_AGGR_PKT_LIMIT;
				}
			}
		} else if (IPA_CLIENT_IS_WLAN_CONS(in->client)) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
				ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz = IPA_WLAN_RX_BUFF_SZ;
			sys->rx_pool_sz = in->desc_fifo_sz /
				IPA_FIFO_ELEMENT_SIZE - 1;
			if (sys->rx_pool_sz > IPA_WLAN_RX_POOL_SZ)
				sys->rx_pool_sz = IPA_WLAN_RX_POOL_SZ;
			sys->pyld_hdlr = NULL;
			sys->repl_hdlr = ipa3_replenish_wlan_rx_cache;
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa3_free_skb_rx;
			sys->free_rx_wrapper = ipa3_free_rx_wrapper;
			in->ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
		} else if (IPA_CLIENT_IS_ODU_CONS(in->client)) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
				ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_pool_sz = in->desc_fifo_sz /
				IPA_FIFO_ELEMENT_SIZE - 1;
			if (sys->rx_pool_sz > IPA_ODU_RX_POOL_SZ)
				sys->rx_pool_sz = IPA_ODU_RX_POOL_SZ;
			sys->pyld_hdlr = ipa3_odu_rx_pyld_hdlr;
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa3_free_skb_rx;
			/* recycle skb for GSB use case */
			if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
				sys->free_rx_wrapper =
					ipa3_free_rx_wrapper;
				sys->repl_hdlr =
					ipa3_replenish_rx_cache;
				/* Overwrite buffer size & aggr limit for GSB */
				sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(
					IPA_GSB_RX_BUFF_BASE_SZ);
				in->ipa_ep_cfg.aggr.aggr_byte_limit =
					IPA_GSB_AGGR_BYTE_LIMIT;
			} else {
				sys->free_rx_wrapper =
					ipa3_free_rx_wrapper;
				sys->repl_hdlr = ipa3_replenish_rx_cache;
				sys->rx_buff_sz = IPA_ODU_RX_BUFF_SZ;
			}
		} else if (in->client ==
				IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
		} else if (in->client ==
				IPA_CLIENT_MEMCPY_DMA_SYNC_CONS) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_NOINTR_MODE;
		}  else if (in->client == IPA_CLIENT_ODL_DPL_CONS) {
			IPADBG("assigning policy to ODL client:%d\n",
				in->client);
			/* Status enabling is needed for DPLv2 with
			 * IPA versions < 4.5.
			 * Dont enable ipa_status for APQ, since MDM IPA
			 * has IPA >= 4.5 with DPLv3.
			 */
			if ((ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ &&
				ipa3_is_mhip_offload_enabled()) ||
				(ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5))
				sys->ep->status.status_en = false;
			else
				sys->ep->status.status_en = true;
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
				ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz =
				IPA_GENERIC_RX_BUFF_SZ(IPA_ODL_RX_BUFF_SZ);
			sys->pyld_hdlr = ipa3_odl_dpl_rx_pyld_hdlr;
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa3_free_skb_rx;
			sys->free_rx_wrapper = ipa3_recycle_rx_wrapper;
			sys->repl_hdlr = ipa3_replenish_rx_cache_recycle;
			sys->rx_pool_sz = in->desc_fifo_sz /
					IPA_FIFO_ELEMENT_SIZE - 1;
		} else {
			WARN(1, "Need to install a RX pipe hdlr\n");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * ipa3_tx_client_rx_notify_release() - Callback function
 * which will call the user supplied callback function to
 * release the skb, or release it on its own if no callback
 * function was supplied
 *
 * @user1: [in] - Data Descriptor
 * @user2: [in] - endpoint idx
 *
 * This notified callback is for the destination client
 * This function is supplied in ipa3_tx_dp_mul
 */
static void ipa3_tx_client_rx_notify_release(void *user1, int user2)
{
	struct ipa_tx_data_desc *dd = (struct ipa_tx_data_desc *)user1;
	int ep_idx = user2;

	IPADBG_LOW("Received data desc anchor:%pK\n", dd);

	atomic_inc(&ipa3_ctx->ep[ep_idx].avail_fifo_desc);
	ipa3_ctx->ep[ep_idx].wstats.rx_pkts_status_rcvd++;

  /* wlan host driver waits till tx complete before unload */
	IPADBG_LOW("ep=%d fifo_desc_free_count=%d\n",
		ep_idx, atomic_read(&ipa3_ctx->ep[ep_idx].avail_fifo_desc));
	IPADBG_LOW("calling client notify callback with priv:%pK\n",
		ipa3_ctx->ep[ep_idx].priv);

	if (ipa3_ctx->ep[ep_idx].client_notify) {
		ipa3_ctx->ep[ep_idx].client_notify(ipa3_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)user1);
		ipa3_ctx->ep[ep_idx].wstats.rx_hd_reply++;
	}
}
/**
 * ipa3_tx_client_rx_pkt_status() - Callback function
 * which will call the user supplied callback function to
 * increase the available fifo descriptor
 *
 * @user1: [in] - Data Descriptor
 * @user2: [in] - endpoint idx
 *
 * This notified callback is for the destination client
 * This function is supplied in ipa3_tx_dp_mul
 */
static void ipa3_tx_client_rx_pkt_status(void *user1, int user2)
{
	int ep_idx = user2;

	atomic_inc(&ipa3_ctx->ep[ep_idx].avail_fifo_desc);
	ipa3_ctx->ep[ep_idx].wstats.rx_pkts_status_rcvd++;
}


/**
 * ipa3_tx_dp_mul() - Data-path tx handler for multiple packets
 * @src: [in] - Client that is sending data
 * @ipa_tx_data_desc:	[in] data descriptors from wlan
 *
 * this is used for to transfer data descriptors that received
 * from WLAN1_PROD pipe to IPA HW
 *
 * The function will send data descriptors from WLAN1_PROD (one
 * at a time). Will set EOT flag for last descriptor Once this send was done
 * from transport point-of-view the IPA driver will get notified by the
 * supplied callback - ipa_gsi_irq_tx_notify_cb()
 *
 * ipa_gsi_irq_tx_notify_cb will call to the user supplied callback
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_tx_dp_mul(enum ipa_client_type src,
			struct ipa_tx_data_desc *data_desc)
{
	/* The second byte in wlan header holds qmap id */
#define IPA_WLAN_HDR_QMAP_ID_OFFSET 1
	struct ipa_tx_data_desc *entry;
	struct ipa3_sys_context *sys;
	struct ipa3_desc desc[2];
	u32 num_desc, cnt;
	int ep_idx;

	IPADBG_LOW("Received data desc anchor:%pK\n", data_desc);

	spin_lock_bh(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);

	ep_idx = ipa3_get_ep_mapping(src);
	if (unlikely(ep_idx == -1)) {
		IPAERR("dest EP does not exist.\n");
		goto fail_send;
	}
	IPADBG_LOW("ep idx:%d\n", ep_idx);
	sys = ipa3_ctx->ep[ep_idx].sys;

	if (unlikely(ipa3_ctx->ep[ep_idx].valid == 0)) {
		IPAERR("dest EP not valid.\n");
		goto fail_send;
	}
	sys->ep->wstats.rx_hd_rcvd++;

	/* Calculate the number of descriptors */
	num_desc = 0;
	list_for_each_entry(entry, &data_desc->link, link) {
		num_desc++;
	}
	IPADBG_LOW("Number of Data Descriptors:%d", num_desc);

	if (atomic_read(&sys->ep->avail_fifo_desc) < num_desc) {
		IPAERR("Insufficient data descriptors available\n");
		goto fail_send;
	}

	/* Assign callback only for last data descriptor */
	cnt = 0;
	list_for_each_entry(entry, &data_desc->link, link) {
		memset(desc, 0, 2 * sizeof(struct ipa3_desc));

		IPADBG_LOW("Parsing data desc :%d\n", cnt);
		cnt++;
		((u8 *)entry->pyld_buffer)[IPA_WLAN_HDR_QMAP_ID_OFFSET] =
			(u8)sys->ep->cfg.meta.qmap_id;

		/* the tag field will be populated in ipa3_send() function */
		desc[0].is_tag_status = true;
		desc[1].pyld = entry->pyld_buffer;
		desc[1].len = entry->pyld_len;
		desc[1].type = IPA_DATA_DESC_SKB;
		desc[1].user1 = data_desc;
		desc[1].user2 = ep_idx;
		IPADBG_LOW("priv:%pK pyld_buf:0x%pK pyld_len:%d\n",
			entry->priv, desc[1].pyld, desc[1].len);

		/* In case of last descriptor populate callback */
		if (cnt == num_desc) {
			IPADBG_LOW("data desc:%pK\n", data_desc);
			desc[1].callback = ipa3_tx_client_rx_notify_release;
		} else {
			desc[1].callback = ipa3_tx_client_rx_pkt_status;
		}

		IPADBG_LOW("calling ipa3_send()\n");
		if (ipa3_send(sys, 2, desc, true)) {
			IPAERR("fail to send skb\n");
			sys->ep->wstats.rx_pkt_leak += (cnt-1);
			sys->ep->wstats.rx_dp_fail++;
			goto fail_send;
		}

		if (atomic_read(&sys->ep->avail_fifo_desc) >= 0)
			atomic_dec(&sys->ep->avail_fifo_desc);

		sys->ep->wstats.rx_pkts_rcvd++;
		IPADBG_LOW("ep=%d fifo desc=%d\n",
			ep_idx, atomic_read(&sys->ep->avail_fifo_desc));
	}

	sys->ep->wstats.rx_hd_processed++;
	spin_unlock_bh(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	return 0;

fail_send:
	spin_unlock_bh(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	return -EFAULT;

}

void ipa3_free_skb(struct ipa_rx_data *data)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;

	spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	ipa3_ctx->wc_memb.total_tx_pkts_freed++;
	rx_pkt = container_of(data, struct ipa3_rx_pkt_wrapper, data);

	ipa3_skb_recycle(rx_pkt->data.skb);
	(void)skb_put(rx_pkt->data.skb, IPA_WLAN_RX_BUFF_SZ);

	list_add_tail(&rx_pkt->link,
		&ipa3_ctx->wc_memb.wlan_comm_desc_list);
	ipa3_ctx->wc_memb.wlan_comm_free_cnt++;

	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
}

/* Functions added to support kernel tests */

int ipa3_sys_setup(struct ipa_sys_connect_params *sys_in,
			unsigned long *ipa_transport_hdl,
			u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;

	if (sys_in == NULL || clnt_hdl == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}

	if (ipa_transport_hdl == NULL || ipa_pipe_num == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}
	if (sys_in->client >= IPA_CLIENT_MAX) {
		IPAERR("bad parm client:%d\n", sys_in->client);
		goto fail_gen;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(sys_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client :%d\n", sys_in->client);
		goto fail_gen;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	IPA_ACTIVE_CLIENTS_INC_EP(sys_in->client);

	if (ep->valid == 1) {
		if (sys_in->client != IPA_CLIENT_APPS_WAN_PROD) {
			IPAERR("EP %d already allocated\n", ipa_ep_idx);
			goto fail_and_disable_clocks;
		} else {
			if (ipa3_cfg_ep_hdr(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.hdr)) {
				IPAERR("fail to configure hdr prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			if (ipa3_cfg_ep_hdr_ext(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.hdr_ext)) {
				IPAERR("fail config hdr_ext prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			if (ipa3_cfg_ep_cfg(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.cfg)) {
				IPAERR("fail to configure cfg prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			IPAERR("client %d (ep: %d) overlay ok sys=%pK\n",
					sys_in->client, ipa_ep_idx, ep->sys);
			ep->client_notify = sys_in->notify;
			ep->priv = sys_in->priv;
			*clnt_hdl = ipa_ep_idx;
			if (!ep->keep_ipa_awake)
				IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);

			return 0;
		}
	}

	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));

	ep->valid = 1;
	ep->client = sys_in->client;
	ep->client_notify = sys_in->notify;
	ep->priv = sys_in->priv;
	ep->keep_ipa_awake = true;
	if (en_status) {
		ep->status.status_en = true;
		ep->status.status_ep = ipa_ep_idx;
	}

	result = ipa3_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n",
				 result, ipa_ep_idx);
		goto fail_gen2;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_gen2;
		}
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_gen2;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	*ipa_pipe_num = ipa_ep_idx;
	*ipa_transport_hdl = ipa3_ctx->gsi_dev_hdl;

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPADBG("client %d (ep: %d) connected sys=%pK\n", sys_in->client,
			ipa_ep_idx, ep->sys);

	return 0;

fail_gen2:
fail_and_disable_clocks:
	IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);
fail_gen:
	return result;
}

int ipa3_sys_teardown(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm(Either endpoint or client hdl invalid)\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_disable_data_path(clnt_hdl);
	ep->valid = 0;

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}

int ipa3_sys_update_gsi_hdls(u32 clnt_hdl, unsigned long gsi_ch_hdl,
	unsigned long gsi_ev_hdl)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm(Either endpoint or client hdl invalid)\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	ep->gsi_chan_hdl = gsi_ch_hdl;
	ep->gsi_evt_ring_hdl = gsi_ev_hdl;

	return 0;
}

static void ipa_gsi_evt_ring_err_cb(struct gsi_evt_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_EVT_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_EVT_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_EVT_EVT_RING_EMPTY_ERR:
		IPAERR("Got GSI_EVT_EVT_RING_EMPTY_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
}

static void ipa_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_CHAN_INVALID_TRE_ERR:
		IPAERR("Got GSI_CHAN_INVALID_TRE_ERR\n");
		break;
	case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
		IPAERR("Got GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_CHAN_HWO_1_ERR:
		IPAERR("Got GSI_CHAN_HWO_1_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
}

static void ipa_gsi_irq_tx_notify_cb(struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_tx_pkt_wrapper *tx_pkt;

	IPADBG_LOW("event %d notified\n", notify->evt_id);

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		tx_pkt = notify->xfer_user_data;
		tx_pkt->xmit_done = true;
		atomic_inc(&tx_pkt->sys->xmit_eot_cnt);
		tasklet_schedule(&tx_pkt->sys->tasklet);
		break;
	default:
		IPAERR("received unexpected event id %d\n", notify->evt_id);
	}
}

void __ipa_gsi_irq_rx_scedule_poll(struct ipa3_sys_context *sys)
{
	bool clk_off;

	atomic_set(&sys->curr_polling_state, 1);
	__ipa3_update_curr_poll_state(sys->ep->client, 1);

	ipa3_inc_acquire_wakelock();

	/*
	 * pm deactivate is done in wq context
	 * or after NAPI poll
	 */

	clk_off = ipa_pm_activate(sys->pm_hdl);
	if (!clk_off && sys->napi_obj) {
		napi_schedule(sys->napi_obj);
		IPA_STATS_INC_CNT(sys->napi_sch_cnt);
		return;
	}
	queue_work(sys->wq, &sys->work);
	return;

}

static void ipa_gsi_irq_rx_notify_cb(struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_sys_context *sys;

	if (unlikely(!notify)) {
		IPAERR("gsi notify is NULL.\n");
		return;
	}
	IPADBG_LOW("event %d notified\n", notify->evt_id);

	sys = (struct ipa3_sys_context *)notify->chan_user_data;

	/*
	 * In suspend just before stopping the channel possible to receive
	 * the IEOB interrupt and xfer pointer will not be processed in this
	 * mode and moving channel poll mode. In resume after starting the
	 * channel will receive the IEOB interrupt and xfer pointer will be
	 * overwritten. To avoid this process all data in polling context.
	 */
	sys->ep->xfer_notify_valid = false;
	sys->ep->xfer_notify = *notify;

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
	case GSI_CHAN_EVT_EOB:
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		if (!atomic_read(&sys->curr_polling_state)) {
			/* put the gsi channel into polling mode */
			gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
				GSI_CHAN_MODE_POLL);
			__ipa_gsi_irq_rx_scedule_poll(sys);
		}
		break;
	default:
		IPAERR("received unexpected event id %d\n", notify->evt_id);
	}
}

static void ipa_dma_gsi_irq_rx_notify_cb(struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_sys_context *sys;

	if (unlikely(!notify)) {
		IPAERR("gsi notify is NULL.\n");
		return;
	}
	IPADBG_LOW("event %d notified\n", notify->evt_id);

	sys = (struct ipa3_sys_context *)notify->chan_user_data;
	if (sys->ep->client == IPA_CLIENT_MEMCPY_DMA_SYNC_CONS) {
		IPAERR("IRQ_RX Callback was called for DMA_SYNC_CONS.\n");
		return;
	}

	sys->ep->xfer_notify_valid = false;
	sys->ep->xfer_notify = *notify;

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
		if (!atomic_read(&sys->curr_polling_state)) {
			/* put the gsi channel into polling mode */
			gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
				GSI_CHAN_MODE_POLL);
			ipa3_inc_acquire_wakelock();
			atomic_set(&sys->curr_polling_state, 1);
			queue_work(sys->wq, &sys->work);
		}
		break;
	default:
		IPAERR("received unexpected event id %d\n", notify->evt_id);
	}
}

int ipa3_alloc_common_event_ring(void)
{
	struct gsi_evt_ring_props gsi_evt_ring_props;
	dma_addr_t evt_dma_addr;
	int result;

	memset(&gsi_evt_ring_props, 0, sizeof(gsi_evt_ring_props));
	gsi_evt_ring_props.intf = GSI_EVT_CHTYPE_GPI_EV;
	gsi_evt_ring_props.intr = GSI_INTR_IRQ;
	gsi_evt_ring_props.re_size = GSI_EVT_RING_RE_SIZE_16B;

	gsi_evt_ring_props.ring_len = IPA_COMMON_EVENT_RING_SIZE;

	gsi_evt_ring_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev,
		gsi_evt_ring_props.ring_len, &evt_dma_addr, GFP_KERNEL);
	if (!gsi_evt_ring_props.ring_base_vaddr) {
		IPAERR("fail to dma alloc %u bytes\n",
			gsi_evt_ring_props.ring_len);
		return -ENOMEM;
	}
	gsi_evt_ring_props.ring_base_addr = evt_dma_addr;
	gsi_evt_ring_props.int_modt = 0;
	gsi_evt_ring_props.int_modc = 1; /* moderation comes from channel*/
	gsi_evt_ring_props.rp_update_addr = 0;
	gsi_evt_ring_props.exclusive = false;
	gsi_evt_ring_props.err_cb = ipa_gsi_evt_ring_err_cb;
	gsi_evt_ring_props.user_data = NULL;

	result = gsi_alloc_evt_ring(&gsi_evt_ring_props,
		ipa3_ctx->gsi_dev_hdl, &ipa3_ctx->gsi_evt_comm_hdl);
	if (result) {
		IPAERR("gsi_alloc_evt_ring failed %d\n", result);
		return result;
	}
	ipa3_ctx->gsi_evt_comm_ring_rem = IPA_COMMON_EVENT_RING_SIZE;

	return 0;
}

static int ipa_gsi_setup_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep)
{
	u32 ring_size;
	int result;
	gfp_t mem_flag = GFP_KERNEL;
	u32 coale_ep_idx;

	if (in->client == IPA_CLIENT_APPS_WAN_CONS ||
		in->client == IPA_CLIENT_APPS_WAN_COAL_CONS ||
		in->client == IPA_CLIENT_APPS_WAN_PROD)
		mem_flag = GFP_ATOMIC;

	if (!ep) {
		IPAERR("EP context is empty\n");
		return -EINVAL;
	}
	coale_ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
	/*
	 * GSI ring length is calculated based on the desc_fifo_sz
	 * which was meant to define the BAM desc fifo. GSI descriptors
	 * are 16B as opposed to 8B for BAM.
	 */
	ring_size = 2 * in->desc_fifo_sz;
	ep->gsi_evt_ring_hdl = ~0;
	if (ep->sys->use_comm_evt_ring) {
		if (ipa3_ctx->gsi_evt_comm_ring_rem < ring_size) {
			IPAERR("not enough space in common event ring\n");
			IPAERR("available: %d needed: %d\n",
				ipa3_ctx->gsi_evt_comm_ring_rem,
				ring_size);
			WARN_ON(1);
			return -EFAULT;
		}
		ipa3_ctx->gsi_evt_comm_ring_rem -= (ring_size);
		ep->gsi_evt_ring_hdl = ipa3_ctx->gsi_evt_comm_hdl;
	} else if (in->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
		result = ipa_gsi_setup_event_ring(ep,
				IPA_COMMON_EVENT_RING_SIZE, mem_flag);
		if (result)
			goto fail_setup_event_ring;

	} else if (in->client == IPA_CLIENT_APPS_WAN_CONS &&
			coale_ep_idx != IPA_EP_NOT_ALLOCATED &&
			ipa3_ctx->ep[coale_ep_idx].valid == 1) {
		IPADBG("Wan consumer pipe configured\n");
		result = ipa_gsi_setup_coal_def_channel(in, ep,
					&ipa3_ctx->ep[coale_ep_idx]);
		if (result) {
			IPAERR("Failed to setup default coal GSI channel\n");
			goto fail_setup_event_ring;
		}
		return result;
	} else if (ep->sys->policy != IPA_POLICY_NOINTR_MODE ||
			IPA_CLIENT_IS_CONS(ep->client)) {
		result = ipa_gsi_setup_event_ring(ep, ring_size, mem_flag);
		if (result)
			goto fail_setup_event_ring;
	}
	result = ipa_gsi_setup_transfer_ring(ep, ring_size,
		ep->sys, mem_flag);
	if (result)
		goto fail_setup_transfer_ring;

	if (ep->client == IPA_CLIENT_MEMCPY_DMA_SYNC_CONS)
		gsi_config_channel_mode(ep->gsi_chan_hdl,
				GSI_CHAN_MODE_POLL);
	return 0;

fail_setup_transfer_ring:
	if (ep->gsi_mem_info.evt_ring_base_vaddr)
		dma_free_coherent(ipa3_ctx->pdev, ep->gsi_mem_info.evt_ring_len,
			ep->gsi_mem_info.evt_ring_base_vaddr,
			ep->gsi_mem_info.evt_ring_base_addr);
fail_setup_event_ring:
	IPAERR("Return with err: %d\n", result);
	return result;
}

static int ipa_gsi_setup_event_ring(struct ipa3_ep_context *ep,
	u32 ring_size, gfp_t mem_flag)
{
	struct gsi_evt_ring_props gsi_evt_ring_props;
	dma_addr_t evt_dma_addr;
	int result;

	evt_dma_addr = 0;
	memset(&gsi_evt_ring_props, 0, sizeof(gsi_evt_ring_props));
	gsi_evt_ring_props.intf = GSI_EVT_CHTYPE_GPI_EV;
	gsi_evt_ring_props.intr = GSI_INTR_IRQ;
	gsi_evt_ring_props.re_size = GSI_EVT_RING_RE_SIZE_16B;
	gsi_evt_ring_props.ring_len = ring_size;
	gsi_evt_ring_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, gsi_evt_ring_props.ring_len,
		&evt_dma_addr, mem_flag);
	if (!gsi_evt_ring_props.ring_base_vaddr) {
		IPAERR("fail to dma alloc %u bytes\n",
			gsi_evt_ring_props.ring_len);
		return -ENOMEM;
	}
	gsi_evt_ring_props.ring_base_addr = evt_dma_addr;

	/* copy mem info */
	ep->gsi_mem_info.evt_ring_len = gsi_evt_ring_props.ring_len;
	ep->gsi_mem_info.evt_ring_base_addr =
		gsi_evt_ring_props.ring_base_addr;
	ep->gsi_mem_info.evt_ring_base_vaddr =
		gsi_evt_ring_props.ring_base_vaddr;

	if (ep->sys->napi_obj) {
		gsi_evt_ring_props.int_modt = IPA_GSI_EVT_RING_INT_MODT;
		gsi_evt_ring_props.int_modc = IPA_GSI_EVT_RING_INT_MODC;
	} else {
		gsi_evt_ring_props.int_modt = IPA_GSI_EVT_RING_INT_MODT;
		gsi_evt_ring_props.int_modc = 1;
	}

	IPADBG("client=%d moderation threshold cycles=%u cnt=%u\n",
		ep->client,
		gsi_evt_ring_props.int_modt,
		gsi_evt_ring_props.int_modc);
	gsi_evt_ring_props.rp_update_addr = 0;
	gsi_evt_ring_props.exclusive = true;
	gsi_evt_ring_props.err_cb = ipa_gsi_evt_ring_err_cb;
	gsi_evt_ring_props.user_data = NULL;

	result = gsi_alloc_evt_ring(&gsi_evt_ring_props,
		ipa3_ctx->gsi_dev_hdl, &ep->gsi_evt_ring_hdl);
	if (result != GSI_STATUS_SUCCESS)
		goto fail_alloc_evt_ring;

	return 0;

fail_alloc_evt_ring:
	if (ep->gsi_mem_info.evt_ring_base_vaddr)
		dma_free_coherent(ipa3_ctx->pdev, ep->gsi_mem_info.evt_ring_len,
			ep->gsi_mem_info.evt_ring_base_vaddr,
			ep->gsi_mem_info.evt_ring_base_addr);
	IPAERR("Return with err: %d\n", result);
	return result;
}

static int ipa_gsi_setup_transfer_ring(struct ipa3_ep_context *ep,
	u32 ring_size, struct ipa3_sys_context *user_data, gfp_t mem_flag)
{
	dma_addr_t dma_addr;
	union __packed gsi_channel_scratch ch_scratch;
	struct gsi_chan_props gsi_channel_props;
	const struct ipa_gsi_ep_config *gsi_ep_info;
	int result;

	memset(&gsi_channel_props, 0, sizeof(gsi_channel_props));
	if (ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
		gsi_channel_props.prot = GSI_CHAN_PROT_GCI;
	else
		gsi_channel_props.prot = GSI_CHAN_PROT_GPI;
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		gsi_channel_props.dir = GSI_CHAN_DIR_TO_GSI;
	} else {
		gsi_channel_props.dir = GSI_CHAN_DIR_FROM_GSI;
		gsi_channel_props.max_re_expected = ep->sys->rx_pool_sz;
	}

	gsi_ep_info = ipa3_get_gsi_ep_info(ep->client);
	if (!gsi_ep_info) {
		IPAERR("Failed getting GSI EP info for client=%d\n",
		       ep->client);
		result = -EINVAL;
		goto fail_get_gsi_ep_info;
	} else {
		gsi_channel_props.ch_id = gsi_ep_info->ipa_gsi_chan_num;
	}

	gsi_channel_props.evt_ring_hdl = ep->gsi_evt_ring_hdl;
	gsi_channel_props.re_size = GSI_CHAN_RE_SIZE_16B;
	gsi_channel_props.ring_len = ring_size;

	gsi_channel_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, gsi_channel_props.ring_len,
			&dma_addr, mem_flag);
	if (!gsi_channel_props.ring_base_vaddr) {
		IPAERR("fail to dma alloc %u bytes\n",
			gsi_channel_props.ring_len);
		result = -ENOMEM;
		goto fail_alloc_channel_ring;
	}
	gsi_channel_props.ring_base_addr = dma_addr;

	/* copy mem info */
	ep->gsi_mem_info.chan_ring_len = gsi_channel_props.ring_len;
	ep->gsi_mem_info.chan_ring_base_addr =
		gsi_channel_props.ring_base_addr;
	ep->gsi_mem_info.chan_ring_base_vaddr =
		gsi_channel_props.ring_base_vaddr;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		gsi_channel_props.use_db_eng = GSI_CHAN_DIRECT_MODE;
	else
		gsi_channel_props.use_db_eng = GSI_CHAN_DB_MODE;
	gsi_channel_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	if (ep->client == IPA_CLIENT_APPS_CMD_PROD)
		gsi_channel_props.low_weight = IPA_GSI_MAX_CH_LOW_WEIGHT;
	else
		gsi_channel_props.low_weight = 1;
	gsi_channel_props.prefetch_mode = gsi_ep_info->prefetch_mode;
	gsi_channel_props.empty_lvl_threshold = gsi_ep_info->prefetch_threshold;
	gsi_channel_props.chan_user_data = user_data;
	gsi_channel_props.err_cb = ipa_gsi_chan_err_cb;
	if (IPA_CLIENT_IS_PROD(ep->client))
		gsi_channel_props.xfer_cb = ipa_gsi_irq_tx_notify_cb;
	else
		gsi_channel_props.xfer_cb = ipa_gsi_irq_rx_notify_cb;
	if (IPA_CLIENT_IS_MEMCPY_DMA_CONS(ep->client))
		gsi_channel_props.xfer_cb = ipa_dma_gsi_irq_rx_notify_cb;

	if (IPA_CLIENT_IS_CONS(ep->client))
		gsi_channel_props.cleanup_cb = free_rx_pkt;

	/* overwrite the cleanup_cb for page recycling */
	if (ipa3_ctx->ipa_wan_skb_page &&
		(IPA_CLIENT_IS_WAN_CONS(ep->client)))
		gsi_channel_props.cleanup_cb = free_rx_page;

	result = gsi_alloc_channel(&gsi_channel_props, ipa3_ctx->gsi_dev_hdl,
		&ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to alloc GSI chan.\n");
		goto fail_alloc_channel;
	}

	memset(&ch_scratch, 0, sizeof(ch_scratch));
	/*
	 * Update scratch for MCS smart prefetch:
	 * Starting IPA4.5, smart prefetch implemented by H/W.
	 * At IPA 4.0/4.1/4.2, we do not use MCS smart prefetch
	 *  so keep the fields zero.
	 */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		ch_scratch.gpi.max_outstanding_tre =
			gsi_ep_info->ipa_if_tlv * GSI_CHAN_RE_SIZE_16B;
		ch_scratch.gpi.outstanding_threshold =
			2 * GSI_CHAN_RE_SIZE_16B;
	}
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
		ch_scratch.gpi.dl_nlo_channel = 0;
	result = gsi_write_channel_scratch(ep->gsi_chan_hdl, ch_scratch);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("failed to write scratch %d\n", result);
		goto fail_write_channel_scratch;
	}
	return 0;

fail_write_channel_scratch:
	if (gsi_dealloc_channel(ep->gsi_chan_hdl)
		!= GSI_STATUS_SUCCESS) {
		IPAERR("Failed to dealloc GSI chan.\n");
		WARN_ON(1);
	}
fail_alloc_channel:
	dma_free_coherent(ipa3_ctx->pdev, ep->gsi_mem_info.chan_ring_len,
			ep->gsi_mem_info.chan_ring_base_vaddr,
			ep->gsi_mem_info.chan_ring_base_addr);
fail_alloc_channel_ring:
fail_get_gsi_ep_info:
	if (ep->gsi_evt_ring_hdl != ~0) {
		gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
		ep->gsi_evt_ring_hdl = ~0;
	}
	return result;
}

static int ipa_gsi_setup_coal_def_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep, struct ipa3_ep_context *coal_ep)
{
	u32 ring_size;
	int result;

	ring_size = 2 * in->desc_fifo_sz;

	/* copy event ring handle */
	ep->gsi_evt_ring_hdl = coal_ep->gsi_evt_ring_hdl;

	result = ipa_gsi_setup_transfer_ring(ep, ring_size,
		coal_ep->sys, GFP_ATOMIC);
	if (result) {
		if (ep->gsi_mem_info.evt_ring_base_vaddr)
			dma_free_coherent(ipa3_ctx->pdev,
					ep->gsi_mem_info.chan_ring_len,
					ep->gsi_mem_info.chan_ring_base_vaddr,
					ep->gsi_mem_info.chan_ring_base_addr);
		IPAERR("Destroying WAN_COAL_CONS evt_ring");
		if (ep->gsi_evt_ring_hdl != ~0) {
			gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
			ep->gsi_evt_ring_hdl = ~0;
		}
		IPAERR("Return with err: %d\n", result);
		return result;
	}
	return 0;
}

static int ipa_populate_tag_field(struct ipa3_desc *desc,
		struct ipa3_tx_pkt_wrapper *tx_pkt,
		struct ipahal_imm_cmd_pyld **tag_pyld_ret)
{
	struct ipahal_imm_cmd_pyld *tag_pyld;
	struct ipahal_imm_cmd_ip_packet_tag_status tag_cmd = {0};

	/* populate tag field only if it is NULL */
	if (desc->pyld == NULL) {
		tag_cmd.tag = pointer_to_tag_wa(tx_pkt);
		tag_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_IP_PACKET_TAG_STATUS, &tag_cmd, true);
		if (unlikely(!tag_pyld)) {
			IPAERR("Failed to construct ip_packet_tag_status\n");
			return -EFAULT;
		}
		/*
		 * This is for 32-bit pointer, will need special
		 * handling if 64-bit pointer is used
		 */
		IPADBG_LOW("tx_pkt sent in tag: 0x%pK\n", tx_pkt);
		desc->pyld = tag_pyld->data;
		desc->opcode = tag_pyld->opcode;
		desc->len = tag_pyld->len;
		desc->user1 = tag_pyld;
		desc->type = IPA_IMM_CMD_DESC;
		desc->callback = ipa3_tag_destroy_imm;

		*tag_pyld_ret = tag_pyld;
	}
	return 0;
}

static int ipa_poll_gsi_pkt(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify)
{
	int unused_var;

	return ipa_poll_gsi_n_pkt(sys, notify, 1, &unused_var);
}


static int ipa_poll_gsi_n_pkt(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify,
		int expected_num, int *actual_num)
{
	int ret;
	int idx = 0;
	int poll_num = 0;

	if (!actual_num || expected_num <= 0 ||
		expected_num > IPA_WAN_NAPI_MAX_FRAMES) {
		IPAERR("bad params actual_num=%pK expected_num=%d\n",
			actual_num, expected_num);
		return GSI_STATUS_INVALID_PARAMS;
	}

	if (sys->ep->xfer_notify_valid) {
		*notify = sys->ep->xfer_notify;
		sys->ep->xfer_notify_valid = false;
		idx++;
	}
	if (expected_num == idx) {
		*actual_num = idx;
		return GSI_STATUS_SUCCESS;
	}

	ret = gsi_poll_n_channel(sys->ep->gsi_chan_hdl,
		&notify[idx], expected_num - idx, &poll_num);
	if (ret == GSI_STATUS_POLL_EMPTY) {
		if (idx) {
			*actual_num = idx;
			return GSI_STATUS_SUCCESS;
		}
		*actual_num = 0;
		return ret;
	} else if (ret != GSI_STATUS_SUCCESS) {
		if (idx) {
			*actual_num = idx;
			return GSI_STATUS_SUCCESS;
		}
		*actual_num = 0;
		IPAERR("Poll channel err: %d\n", ret);
		return ret;
	}

	*actual_num = idx + poll_num;
	return ret;
}
/**
 * ipa3_lan_rx_poll() - Poll the LAN rx packets from IPA HW.
 * This function is executed in the softirq context
 *
 * if input budget is zero, the driver switches back to
 * interrupt mode.
 *
 * return number of polled packets, on error 0(zero)
 */
int ipa3_lan_rx_poll(u32 clnt_hdl, int weight)
{
	struct ipa3_ep_context *ep;
	int ret;
	int cnt = 0;
	int remain_aggr_weight;
	struct gsi_chan_xfer_notify notify;

	if (unlikely(clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0)) {
		IPAERR("bad param 0x%x\n", clnt_hdl);
		return cnt;
	}
	remain_aggr_weight = weight / IPA_LAN_AGGR_PKT_CNT;
	if (unlikely(remain_aggr_weight > IPA_LAN_NAPI_MAX_FRAMES)) {
		IPAERR("NAPI weight is higher than expected\n");
		IPAERR("expected %d got %d\n",
			IPA_LAN_NAPI_MAX_FRAMES, remain_aggr_weight);
		return cnt;
	}
	ep = &ipa3_ctx->ep[clnt_hdl];

start_poll:
	while (remain_aggr_weight > 0 &&
			atomic_read(&ep->sys->curr_polling_state)) {
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		ret = ipa_poll_gsi_pkt(ep->sys, &notify);
		if (ret)
			break;

		if (IPA_CLIENT_IS_MEMCPY_DMA_CONS(ep->client))
			ipa3_dma_memcpy_notify(ep->sys);
		else if (IPA_CLIENT_IS_WLAN_CONS(ep->client))
			ipa3_wlan_wq_rx_common(ep->sys, &notify);
		else
			ipa3_wq_rx_common(ep->sys, &notify);

		remain_aggr_weight--;
		if (ep->sys->len == 0) {
			if (remain_aggr_weight == 0)
				cnt--;
			break;
		}
	}
	cnt += weight - remain_aggr_weight * IPA_LAN_AGGR_PKT_CNT;
	if (cnt < weight) {
		napi_complete(ep->sys->napi_obj);
		ret = ipa3_rx_switch_to_intr_mode(ep->sys);
		if (ret == -GSI_STATUS_PENDING_IRQ &&
				napi_reschedule(ep->sys->napi_obj))
			goto start_poll;

		ipa_pm_deferred_deactivate(ep->sys->pm_hdl);
	}

	return cnt;
}

/**
 * ipa3_rx_poll() - Poll the WAN rx packets from IPA HW. This
 * function is exectued in the softirq context
 *
 * if input budget is zero, the driver switches back to
 * interrupt mode.
 *
 * return number of polled packets, on error 0(zero)
 */
int ipa3_rx_poll(u32 clnt_hdl, int weight)
{
	struct ipa3_ep_context *ep;
	struct ipa3_sys_context *wan_def_sys;
	int ret;
	int cnt = 0;
	int num = 0;
	int remain_aggr_weight;
	int ipa_ep_idx;
	struct ipa_active_client_logging_info log;
	struct gsi_chan_xfer_notify notify[IPA_WAN_NAPI_MAX_FRAMES];

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log, "NAPI");

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm 0x%x\n", clnt_hdl);
		return cnt;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("Invalid client.\n");
		return cnt;
	}

	wan_def_sys = ipa3_ctx->ep[ipa_ep_idx].sys;
	remain_aggr_weight = weight / IPA_WAN_AGGR_PKT_CNT;

	if (remain_aggr_weight > IPA_WAN_NAPI_MAX_FRAMES) {
		IPAERR("NAPI weight is higher than expected\n");
		IPAERR("expected %d got %d\n",
			IPA_WAN_NAPI_MAX_FRAMES, remain_aggr_weight);
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
start_poll:
	while (remain_aggr_weight > 0 &&
			atomic_read(&ep->sys->curr_polling_state)) {
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		if (ipa3_ctx->enable_napi_chain) {
			ret = ipa_poll_gsi_n_pkt(ep->sys, notify,
				remain_aggr_weight, &num);
		} else {
			ret = ipa_poll_gsi_n_pkt(ep->sys, notify,
				1, &num);
		}
		if (ret)
			break;

		trace_ipa3_rx_poll_num(num);
		ipa3_rx_napi_chain(ep->sys, notify, num);
		remain_aggr_weight -= num;

		trace_ipa3_rx_poll_cnt(ep->sys->len);
		if (ep->sys->len == 0) {
			if (remain_aggr_weight == 0)
				cnt--;
			break;
		}
	}
	cnt += weight - remain_aggr_weight * IPA_WAN_AGGR_PKT_CNT;
	/* call repl_hdlr before napi_reschedule / napi_complete */
	ep->sys->repl_hdlr(ep->sys);

	/* When not able to replenish enough descriptors, keep in polling
	 * mode, wait for napi-poll and replenish again.
	 */
	if (cnt < weight && ep->sys->len > IPA_DEFAULT_SYS_YELLOW_WM &&
		wan_def_sys->len > IPA_DEFAULT_SYS_YELLOW_WM) {
		napi_complete(ep->sys->napi_obj);
		IPA_STATS_INC_CNT(ep->sys->napi_comp_cnt);
		ret = ipa3_rx_switch_to_intr_mode(ep->sys);
		if (ret == -GSI_STATUS_PENDING_IRQ &&
				napi_reschedule(ep->sys->napi_obj))
			goto start_poll;
		ipa_pm_deferred_deactivate(ep->sys->pm_hdl);
	} else {
		cnt = weight;
		IPADBG_LOW("Client = %d not replenished free descripotrs\n",
				ep->client);
	}
	return cnt;
}

static unsigned long tag_to_pointer_wa(uint64_t tag)
{
	return 0xFFFF000000000000 | (unsigned long) tag;
}

static uint64_t pointer_to_tag_wa(struct ipa3_tx_pkt_wrapper *tx_pkt)
{
	u16 temp;
	/* Add the check but it might have throughput issue */
	if (BITS_PER_LONG == 64) {
		temp = (u16) (~((unsigned long) tx_pkt &
			0xFFFF000000000000) >> 48);
		if (temp) {
			IPAERR("The 16 prefix is not all 1s (%pK)\n",
			tx_pkt);
			/*
			 * We need all addresses starting at 0xFFFF to
			 * pass it to HW.
			 */
			ipa_assert();
		}
	}
	return (unsigned long)tx_pkt & 0x0000FFFFFFFFFFFF;
}

/**
 * ipa_gsi_ch20_wa() - software workaround for IPA GSI channel 20
 *
 * A hardware limitation requires to avoid using GSI physical channel 20.
 * This function allocates GSI physical channel 20 and holds it to prevent
 * others to use it.
 *
 * Return codes: 0 on success, negative on failure
 */
int ipa_gsi_ch20_wa(void)
{
	struct gsi_chan_props gsi_channel_props;
	dma_addr_t dma_addr;
	int result;
	int i;
	unsigned long chan_hdl[IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC];
	unsigned long chan_hdl_to_keep;


	memset(&gsi_channel_props, 0, sizeof(gsi_channel_props));
	gsi_channel_props.prot = GSI_CHAN_PROT_GPI;
	gsi_channel_props.dir = GSI_CHAN_DIR_TO_GSI;
	gsi_channel_props.evt_ring_hdl = ~0;
	gsi_channel_props.re_size = GSI_CHAN_RE_SIZE_16B;
	gsi_channel_props.ring_len = 4 * gsi_channel_props.re_size;
	gsi_channel_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, gsi_channel_props.ring_len,
		&dma_addr, 0);
	gsi_channel_props.ring_base_addr = dma_addr;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		gsi_channel_props.use_db_eng = GSI_CHAN_DIRECT_MODE;
	else
		gsi_channel_props.use_db_eng = GSI_CHAN_DB_MODE;

	gsi_channel_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	gsi_channel_props.low_weight = 1;
	gsi_channel_props.err_cb = ipa_gsi_chan_err_cb;
	gsi_channel_props.xfer_cb = ipa_gsi_irq_tx_notify_cb;

	/* first allocate channels up to channel 20 */
	for (i = 0; i < IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC; i++) {
		gsi_channel_props.ch_id = i;
		result = gsi_alloc_channel(&gsi_channel_props,
			ipa3_ctx->gsi_dev_hdl,
			&chan_hdl[i]);
		if (result != GSI_STATUS_SUCCESS) {
			IPAERR("failed to alloc channel %d err %d\n",
				i, result);
			return result;
		}
	}

	/* allocate channel 20 */
	gsi_channel_props.ch_id = IPA_GSI_CH_20_WA_VIRT_CHAN;
	result = gsi_alloc_channel(&gsi_channel_props, ipa3_ctx->gsi_dev_hdl,
		&chan_hdl_to_keep);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("failed to alloc channel %d err %d\n",
			i, result);
		return result;
	}

	/* release all other channels */
	for (i = 0; i < IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC; i++) {
		result = gsi_dealloc_channel(chan_hdl[i]);
		if (result != GSI_STATUS_SUCCESS) {
			IPAERR("failed to dealloc channel %d err %d\n",
				i, result);
			return result;
		}
	}

	/* DMA memory shall not be freed as it is used by channel 20 */
	return 0;
}

/**
 * ipa_adjust_ra_buff_base_sz()
 *
 * Return value: the largest power of two which is smaller
 * than the input value
 */
static u32 ipa_adjust_ra_buff_base_sz(u32 aggr_byte_limit)
{
	aggr_byte_limit += IPA_MTU;
	aggr_byte_limit += IPA_GENERIC_RX_BUFF_LIMIT;
	aggr_byte_limit--;
	aggr_byte_limit |= aggr_byte_limit >> 1;
	aggr_byte_limit |= aggr_byte_limit >> 2;
	aggr_byte_limit |= aggr_byte_limit >> 4;
	aggr_byte_limit |= aggr_byte_limit >> 8;
	aggr_byte_limit |= aggr_byte_limit >> 16;
	aggr_byte_limit++;
	return aggr_byte_limit >> 1;
}
