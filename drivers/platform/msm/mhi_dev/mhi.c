/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/msm_ep_pcie.h>
#include <linux/ipa_mhi.h>
#include <linux/vmalloc.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/boot_stats.h>

#include "mhi.h"
#include "mhi_hwio.h"
#include "mhi_sm.h"

/* Wait time on the device for Host to set M0 state */
#define MHI_DEV_M0_MAX_CNT		30
/* Wait time before suspend/resume is complete */
#define MHI_SUSPEND_MIN			100
#define MHI_SUSPEND_TIMEOUT		600
/* Wait time on the device for Host to set BHI_INTVEC */
#define MHI_BHI_INTVEC_MAX_CNT			200
#define MHI_BHI_INTVEC_WAIT_MS		50
#define MHI_WAKEUP_TIMEOUT_CNT		20
#define MHI_MASK_CH_EV_LEN		32
#define MHI_RING_CMD_ID			0
#define MHI_RING_PRIMARY_EVT_ID		1
#define MHI_1K_SIZE			0x1000
/* Updated Specification for event start is NER - 2 and end - NER -1 */
#define MHI_HW_ACC_EVT_RING_END		1

#define MHI_HOST_REGION_NUM             2

#define MHI_MMIO_CTRL_INT_STATUS_A7_MSK	0x1
#define MHI_MMIO_CTRL_CRDB_STATUS_MSK	0x2

#define HOST_ADDR(lsb, msb)		((lsb) | ((uint64_t)(msb) << 32))
#define HOST_ADDR_LSB(addr)		(addr & 0xFFFFFFFF)
#define HOST_ADDR_MSB(addr)		((addr >> 32) & 0xFFFFFFFF)

#define MHI_IPC_LOG_PAGES		(100)
#define MHI_REGLEN			0x100
#define MHI_INIT			0
#define MHI_REINIT			1

#define TR_RING_ELEMENT_SZ	sizeof(struct mhi_dev_transfer_ring_element)
#define RING_ELEMENT_TYPE_SZ	sizeof(union mhi_dev_ring_element_type)

#define MHI_DEV_CH_CLOSE_TIMEOUT_MIN	5000
#define MHI_DEV_CH_CLOSE_TIMEOUT_MAX	5100
#define MHI_DEV_CH_CLOSE_TIMEOUT_COUNT	30

uint32_t bhi_imgtxdb;
enum mhi_msg_level mhi_msg_lvl = MHI_MSG_ERROR;
enum mhi_msg_level mhi_ipc_msg_lvl = MHI_MSG_VERBOSE;
void *mhi_ipc_log;

static struct mhi_dev *mhi_ctx;
static void mhi_hwc_cb(void *priv, enum ipa_mhi_event_type event,
	unsigned long data);
static void mhi_ring_init_cb(void *user_data);
static void mhi_update_state_info(enum mhi_ctrl_info info);
static void mhi_update_state_info_ch(uint32_t ch_id, enum mhi_ctrl_info info);
static int mhi_deinit(struct mhi_dev *mhi);
static void mhi_dev_resume_init_with_link_up(struct ep_pcie_notify *notify);
static int mhi_dev_pcie_notify_event;
static void mhi_dev_transfer_completion_cb(void *mreq);
static int mhi_dev_alloc_evt_buf_evt_req(struct mhi_dev *mhi,
		struct mhi_dev_channel *ch, struct mhi_dev_ring *evt_ring);
static struct mhi_dev_uevent_info channel_state_info[MHI_MAX_CHANNELS];
static DECLARE_COMPLETION(read_from_host);
static DECLARE_COMPLETION(write_to_host);
static DECLARE_COMPLETION(transfer_host_to_device);
static DECLARE_COMPLETION(transfer_device_to_host);

/*
 * mhi_dev_ring_cache_completion_cb () - Call back function called
 * by IPA driver when ring element cache is done
 *
 * @req : ring cache request
 */
static void mhi_dev_ring_cache_completion_cb(void *req)
{
	struct ring_cache_req *ring_req = req;

	if (ring_req)
		complete(ring_req->done);
	else
		mhi_log(MHI_MSG_ERROR, "ring cache req is NULL\n");
}

static void mhi_dev_edma_sync_cb(void *done)
{
	complete((struct completion *)done);
}

/**
 * mhi_dev_read_from_host_ipa - memcpy equivalent API to transfer data
 *		from host to device.
 * @mhi:	MHI dev structure.
 * @transfer:	Host and device address details.
 */
void mhi_dev_read_from_host_ipa(struct mhi_dev *mhi, struct mhi_addr *transfer)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	struct ring_cache_req ring_req;

	DECLARE_COMPLETION(done);

	ring_req.done = &done;

	if (WARN_ON(!mhi))
		return;

	if (mhi->config_iatu) {
		offset = (uint64_t) transfer->host_pa - mhi->ctrl_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->ctrl_base.device_pa + offset;
	} else {
		host_addr_pa = transfer->host_pa | bit_40;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"device 0x%llx <<-- host 0x%llx, size %d\n",
		transfer->phy_addr, host_addr_pa,
		(int) transfer->size);
	rc = ipa_dma_async_memcpy((u64)transfer->phy_addr, host_addr_pa,
			(int)transfer->size,
			mhi_dev_ring_cache_completion_cb, &ring_req);
	if (rc)
		pr_err("error while reading from host:%d\n", rc);

	wait_for_completion(&done);
}

/**
 * mhi_dev_write_to_host_ipa - Transfer data from device to host.
 *		Based on support available, either DMA or memcpy is used.
 * @mhi:	MHI dev structure.
 * @transfer:	Host and device address details.
 * @ereq:	event_req structure.
 * @tr_type:	Transfer type.
 */
void mhi_dev_write_to_host_ipa(struct mhi_dev *mhi, struct mhi_addr *transfer,
		struct event_req *ereq, enum mhi_dev_transfer_type tr_type)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	dma_addr_t dma;

	if (WARN_ON(!mhi))
		return;

	if (mhi->config_iatu) {
		offset = (uint64_t) transfer->host_pa - mhi->ctrl_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->ctrl_base.device_pa + offset;
	} else {
		host_addr_pa = transfer->host_pa | bit_40;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"device 0x%llx --> host 0x%llx, size %d\n",
		(uint64_t) mhi->cache_dma_handle, host_addr_pa,
		(int) transfer->size);
	if (tr_type == MHI_DEV_DMA_ASYNC) {
		/*
		 * Event read pointer memory is dma_alloc_coherent memory
		 * don't need to dma_map. Assigns the physical address in
		 * phy_addr.
		 */
		if (transfer->phy_addr)
			dma = transfer->phy_addr;
		else
			dma = dma_map_single(&mhi->pdev->dev,
				transfer->virt_addr, transfer->size,
				DMA_TO_DEVICE);
		if (ereq->event_type == SEND_EVENT_BUFFER) {
			ereq->dma = dma;
			ereq->dma_len = transfer->size;
		} else if (ereq->event_type == SEND_EVENT_RD_OFFSET) {
			/*
			 * Event read pointer memory is dma_alloc_coherent
			 * memory. Don't need to dma_unmap.
			 */
			if (transfer->phy_addr)
				ereq->event_rd_dma = 0;
			else
				ereq->event_rd_dma = dma;
		}
		rc = ipa_dma_async_memcpy(host_addr_pa, (uint64_t) dma,
				(int)transfer->size,
				ereq->client_cb, ereq);
		if (rc)
			pr_err("error while writing to host:%d\n", rc);
	} else if (tr_type == MHI_DEV_DMA_SYNC) {
		/* Copy the device content to a local device
		 * physical address.
		 */
		memcpy(mhi->dma_cache, transfer->virt_addr,
				transfer->size);
		rc = ipa_dma_sync_memcpy(host_addr_pa,
				(u64) mhi->cache_dma_handle,
				(int) transfer->size);
		if (rc)
			pr_err("error while writing to host:%d\n", rc);
	}
}

/*
 * mhi_dev_event_buf_completion_cb() - CB function called by IPA driver
 * when transfer completion event buffer copy to host is done.
 *
 * @req -  event_req structure
 */
static void mhi_dev_event_buf_completion_cb(void *req)
{
	struct event_req *ereq = req;

	if (ereq)
		dma_unmap_single(&mhi_ctx->pdev->dev, ereq->dma,
			ereq->dma_len, DMA_TO_DEVICE);
	else
		mhi_log(MHI_MSG_ERROR, "event req is null\n");
}

/*
 * mhi_dev_event_rd_offset_completion_cb() - CB function called by IPA driver
 * when event ring rd_offset transfer is done.
 *
 * @req -  event_req structure
 */
static void mhi_dev_event_rd_offset_completion_cb(void *req)
{
	union mhi_dev_ring_ctx *ctx;
	int rc;
	struct event_req *ereq = req;
	struct mhi_dev_channel *ch = ereq->context;
	struct mhi_dev *mhi = ch->ring->mhi_dev;
	unsigned long flags;

	if (ereq->event_rd_dma)
		dma_unmap_single(&mhi_ctx->pdev->dev, ereq->event_rd_dma,
			sizeof(uint64_t), DMA_TO_DEVICE);
	/* rp update in host memory should be flushed before sending an MSI */
	wmb();
	ctx = (union mhi_dev_ring_ctx *)&mhi->ev_ctx_cache[ereq->event_ring];
	if (mhi_ctx->use_ipa) {
		rc = ep_pcie_trigger_msi(mhi_ctx->phandle, ctx->ev.msivec);
		if (rc)
			pr_err("%s: error sending in msi\n", __func__);
	}

	/* Add back the flushed events space to the event buffer */
	ch->evt_buf_wp = ereq->start + ereq->num_events;
	if (ch->evt_buf_wp == ch->evt_buf_size)
		ch->evt_buf_wp = 0;
	/* Return the event req to the list */
	spin_lock_irqsave(&mhi->lock, flags);
	if (ch->curr_ereq == NULL)
		ch->curr_ereq = ereq;
	else
		list_add_tail(&ereq->list, &ch->event_req_buffers);
	spin_unlock_irqrestore(&mhi->lock, flags);
}

static void msi_trigger_completion_cb(void *data)
{
	mhi_log(MHI_MSG_VERBOSE,
			"%s invoked\n", __func__);
}

static int mhi_trigger_msi_edma(struct mhi_dev_ring *ring, u32 idx)
{
	struct dma_async_tx_descriptor *descriptor;
	struct ep_pcie_msi_config cfg;
	struct msi_buf_cb_data *msi_buf;
	int rc;
	unsigned long flags;

	if (!mhi_ctx->msi_lower) {
		rc = ep_pcie_get_msi_config(mhi_ctx->phandle, &cfg);
		if (rc) {
			pr_err("Error retrieving pcie msi logic\n");
			return rc;
		}

		mhi_ctx->msi_data = cfg.data;
		mhi_ctx->msi_lower = cfg.lower;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"Trigger MSI via edma, MSI lower:%x IRQ:%d idx:%d\n",
		mhi_ctx->msi_lower, mhi_ctx->msi_data + idx, idx);

	spin_lock_irqsave(&mhi_ctx->msi_lock, flags);

	msi_buf = &ring->msi_buf;
	msi_buf->buf[0] = (mhi_ctx->msi_data + idx);

	descriptor = dmaengine_prep_dma_memcpy(mhi_ctx->tx_dma_chan,
				(dma_addr_t)(mhi_ctx->msi_lower),
				msi_buf->dma_addr,
				sizeof(u32),
				DMA_PREP_INTERRUPT);
	if (!descriptor) {
		pr_err("%s(): desc is null, MSI to Host failed\n", __func__);
		spin_unlock_irqrestore(&mhi_ctx->msi_lock, flags);
		return -EFAULT;
	}

	descriptor->callback_param = msi_buf;
	descriptor->callback = msi_trigger_completion_cb;
	dma_async_issue_pending(mhi_ctx->tx_dma_chan);

	spin_unlock_irqrestore(&mhi_ctx->msi_lock, flags);

	return 0;
}

static int mhi_dev_send_multiple_tr_events(struct mhi_dev *mhi, int evnt_ring,
		struct event_req *ereq, uint32_t evt_len)
{
	int rc = 0;
	uint64_t evnt_ring_idx = mhi->ev_ring_start + evnt_ring;
	struct mhi_dev_ring *ring = &mhi->ring[evnt_ring_idx];
	union mhi_dev_ring_ctx *ctx;
	struct mhi_addr transfer_addr;
	struct mhi_dev_channel *ch;

	if (!ereq) {
		pr_err("%s(): invalid event req\n", __func__);
		return -EINVAL;
	}

	if (evnt_ring_idx > mhi->cfg.event_rings) {
		pr_err("Invalid event ring idx: %lld\n", evnt_ring_idx);
		return -EINVAL;
	}

	ctx = (union mhi_dev_ring_ctx *)&mhi->ev_ctx_cache[evnt_ring];

	if (mhi_ring_get_state(ring) == RING_STATE_UINT) {
		rc = mhi_ring_start(ring, ctx, mhi);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"error starting event ring %d\n", evnt_ring);
			return rc;
		}
	}
	ch = ereq->context;
	/* Check the limits of the buffer to be flushed */
	if (ereq->tr_events < ch->tr_events ||
		(ereq->tr_events + ereq->num_events) >
		(ch->tr_events + ch->evt_buf_size)) {
		pr_err("%s: Invalid completion event buffer!\n", __func__);
		mhi_log(MHI_MSG_ERROR,
			"Invalid cmpl evt buf - start %pK, end %pK\n",
			ereq->tr_events, ereq->tr_events + ereq->num_events);
		return -EINVAL;
	}

	mutex_lock(&ring->event_lock);
	mhi_log(MHI_MSG_VERBOSE, "Flushing %d cmpl events of ch %d\n",
			ereq->num_events, ch->ch_id);
	/* add the events */
	ereq->client_cb = mhi_dev_event_buf_completion_cb;
	ereq->event_type = SEND_EVENT_BUFFER;
	rc = mhi_dev_add_element(ring, ereq->tr_events, ereq, evt_len);
	if (rc) {
		pr_err("%s(): error in adding element rc %d\n", __func__, rc);
		mutex_unlock(&ring->event_lock);
		return rc;
	}
	ring->ring_ctx_shadow->ev.rp = (ring->rd_offset *
		sizeof(union mhi_dev_ring_element_type)) +
		ring->ring_ctx->generic.rbase;

	mhi_log(MHI_MSG_VERBOSE, "ev.rp = %llx for %lld\n",
		ring->ring_ctx_shadow->ev.rp, evnt_ring_idx);

	if (MHI_USE_DMA(mhi)) {
		transfer_addr.host_pa = (mhi->ev_ctx_shadow.host_pa +
		sizeof(struct mhi_dev_ev_ctx) *
		evnt_ring) + (size_t)&ring->ring_ctx->ev.rp -
		(size_t)ring->ring_ctx;
		/*
		 * As ev_ctx_cache memory is dma_alloc_coherent, dma_map_single
		 * should not be called. Pass physical address to write to host.
		 */
		transfer_addr.phy_addr = (mhi->ev_ctx_cache_dma_handle +
			sizeof(struct mhi_dev_ev_ctx) * evnt_ring) +
			(size_t)&ring->ring_ctx->ev.rp -
			(size_t)ring->ring_ctx;
	} else {
		transfer_addr.device_va = (mhi->ev_ctx_shadow.device_va +
		sizeof(struct mhi_dev_ev_ctx) *
		evnt_ring) + (size_t)&ring->ring_ctx->ev.rp -
		(size_t)ring->ring_ctx;
	}

	transfer_addr.virt_addr = &ring->ring_ctx_shadow->ev.rp;
	transfer_addr.size = sizeof(uint64_t);
	ereq->event_type = SEND_EVENT_RD_OFFSET;
	ereq->client_cb = mhi_dev_event_rd_offset_completion_cb;
	ereq->event_ring = evnt_ring;
	mhi_ctx->write_to_host(mhi, &transfer_addr, ereq, MHI_DEV_DMA_ASYNC);
	mutex_unlock(&ring->event_lock);

	if (mhi_ctx->use_edma) {
		rc = mhi_trigger_msi_edma(ring, ctx->ev.msivec);
		if (rc)
			pr_err("%s: error sending in msi\n", __func__);
	}

	return rc;
}

static int mhi_dev_flush_transfer_completion_events(struct mhi_dev *mhi,
		struct mhi_dev_channel *ch)
{
	int rc = 0;
	unsigned long flags;
	struct event_req *flush_ereq;

	/*
	 * Channel got stopped or closed with transfers pending
	 * Do not send completion events to host
	 */
	if (ch->state == MHI_DEV_CH_CLOSED ||
		ch->state == MHI_DEV_CH_STOPPED) {
		mhi_log(MHI_MSG_DBG, "Ch %d closed with %d writes pending\n",
			ch->ch_id, ch->pend_wr_count + 1);
		return -ENODEV;
	}

	do {
		spin_lock_irqsave(&mhi->lock, flags);
		if (list_empty(&ch->flush_event_req_buffers)) {
			spin_unlock_irqrestore(&mhi->lock, flags);
			break;
		}
		flush_ereq = container_of(ch->flush_event_req_buffers.next,
					struct event_req, list);
		list_del_init(&flush_ereq->list);
		spin_unlock_irqrestore(&mhi->lock, flags);

		mhi_log(MHI_MSG_DBG, "Flush called for ch %d\n", ch->ch_id);
		rc = mhi_dev_send_multiple_tr_events(mhi,
				mhi->ch_ctx_cache[ch->ch_id].err_indx,
				flush_ereq,
				(flush_ereq->num_events *
				sizeof(union mhi_dev_ring_element_type)));
		if (rc) {
			mhi_log(MHI_MSG_ERROR, "failed to send compl evts\n");
			break;
		}
	} while (true);

	return rc;
}

static bool mhi_dev_is_full_compl_evt_buf(struct mhi_dev_channel *ch)
{
	if (((ch->evt_buf_rp + 1) % ch->evt_buf_size) == ch->evt_buf_wp)
		return true;

	return false;
}

static void mhi_dev_rollback_compl_evt(struct mhi_dev_channel *ch)
{
	if (ch->evt_buf_rp)
		ch->evt_buf_rp--;
	else
		ch->evt_buf_rp = ch->evt_buf_size - 1;
}

/*
 * mhi_dev_queue_transfer_completion() - Queues a transfer completion
 * event to the event buffer (where events are stored until they get
 * flushed to host). Also determines when the completion events are
 * to be flushed (sent) to host.
 *
 * @req -  event_req structure
 * @flush - Set to true when completion events are to be flushed.
 */

static int mhi_dev_queue_transfer_completion(struct mhi_req *mreq, bool *flush)
{
	union mhi_dev_ring_element_type *compl_ev;
	struct mhi_dev_channel *ch = mreq->client->channel;
	unsigned long flags;

	if (mhi_dev_is_full_compl_evt_buf(ch) || ch->curr_ereq == NULL) {
		mhi_log(MHI_MSG_VERBOSE, "Ran out of %s\n",
			(ch->curr_ereq ? "compl evts" : "ereqs"));
		return -EBUSY;
	}

	if (mreq->el->tre.ieot) {
		compl_ev = ch->tr_events + ch->evt_buf_rp;
		compl_ev->evt_tr_comp.chid = ch->ch_id;
		compl_ev->evt_tr_comp.type =
			MHI_DEV_RING_EL_TRANSFER_COMPLETION_EVENT;
		compl_ev->evt_tr_comp.len = mreq->transfer_len;
		compl_ev->evt_tr_comp.code = MHI_CMD_COMPL_CODE_EOT;
		compl_ev->evt_tr_comp.ptr = ch->ring->ring_ctx->generic.rbase +
			mreq->rd_offset * TR_RING_ELEMENT_SZ;
		ch->evt_buf_rp++;
		if (ch->evt_buf_rp == ch->evt_buf_size)
			ch->evt_buf_rp = 0;
		ch->curr_ereq->num_events++;
		/*
		 * It is not necessary to flush when we need to wrap-around, if
		 * we do have free space in the buffer upon wrap-around.
		 * But when we really need to flush, we need a separate dma op
		 * anyway for the current chunk (from flush_start to the
		 * physical buffer end) since the buffer is circular. So we
		 * might as well flush on wrap-around.
		 * Also, we flush when we hit the threshold as well. The flush
		 * threshold is based on the channel's event ring size.
		 *
		 * In summary, completion event buffer flush is done if
		 *    * Client requests it (snd_cmpl was set to 1) OR
		 *    * Physical end of the event buffer is reached OR
		 *    * Flush threshold is reached for the current ereq
		 *
		 * When events are to be flushed, the current ereq is moved to
		 * the flush list, and the flush param is set to true for the
		 * second and third cases above. The actual flush of the events
		 * is done in the write_to_host API (for the write path) or
		 * in the transfer completion callback (for the read path).
		 */
		if (ch->evt_buf_rp == 0 ||
			ch->curr_ereq->num_events >=
			MHI_CMPL_EVT_FLUSH_THRSHLD(ch->evt_buf_size)
			|| mreq->snd_cmpl) {
			if (flush)
				*flush = true;

			if (!mreq->snd_cmpl)
				mreq->snd_cmpl = 1;

			ch->curr_ereq->tr_events = ch->tr_events +
				ch->curr_ereq->start;
			ch->curr_ereq->context = ch;

			/* Move current event req to flush list*/
			spin_lock_irqsave(&mhi_ctx->lock, flags);
			list_add_tail(&ch->curr_ereq->list,
				&ch->flush_event_req_buffers);

			if (!list_empty(&ch->event_req_buffers)) {
				ch->curr_ereq =
					container_of(ch->event_req_buffers.next,
						struct event_req, list);
				list_del_init(&ch->curr_ereq->list);
				ch->curr_ereq->num_events = 0;
				ch->curr_ereq->start = ch->evt_buf_rp;
			} else {
				pr_err("%s evt req buffers empty\n", __func__);
				mhi_log(MHI_MSG_ERROR,
						"evt req buffers empty\n");
				ch->curr_ereq = NULL;
			}
			spin_unlock_irqrestore(&mhi_ctx->lock, flags);
		}
		return 0;
	}

	mhi_log(MHI_MSG_ERROR, "ieot is not valid\n");
	return -EINVAL;
}

/**
 * mhi_transfer_host_to_device_ipa - memcpy equivalent API to transfer data
 *					from host to the device.
 * @dev:	Physical destination virtual address.
 * @host_pa:	Source physical address.
 * @len:	Numer of bytes to be transferred.
 * @mhi:	MHI dev structure.
 * @mreq:	mhi_req structure
 */
int mhi_transfer_host_to_device_ipa(void *dev, uint64_t host_pa, uint32_t len,
		struct mhi_dev *mhi, struct mhi_req *mreq)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	struct mhi_dev_ring *ring = NULL;
	struct mhi_dev_channel *ch;

	if (WARN_ON(!mhi || !dev || !host_pa || !mreq))
		return -EINVAL;

	if (mhi->config_iatu) {
		offset = (uint64_t)host_pa - mhi->data_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->data_base.device_pa + offset;
	} else {
		host_addr_pa = host_pa | bit_40;
	}

	mhi_log(MHI_MSG_VERBOSE, "device 0x%llx <-- host 0x%llx, size %d\n",
		(uint64_t) mhi->read_dma_handle, host_addr_pa, (int) len);

	if (mreq->mode == DMA_SYNC) {
		rc = ipa_dma_sync_memcpy((u64) mhi->read_dma_handle,
				host_addr_pa, (int) len);
		if (rc) {
			pr_err("error while reading chan using sync:%d\n", rc);
			return rc;
		}
		memcpy(dev, mhi->read_handle, len);
	} else if (mreq->mode == DMA_ASYNC) {
		ch = mreq->client->channel;
		ring = ch->ring;
		mreq->dma = dma_map_single(&mhi->pdev->dev, dev, len,
				DMA_FROM_DEVICE);
		mhi_dev_ring_inc_index(ring, ring->rd_offset);

		if (ring->rd_offset == ring->wr_offset) {
			mhi_log(MHI_MSG_VERBOSE,
				"Setting snd_cmpl to 1 for ch %d\n", ch->ch_id);
			mreq->snd_cmpl = 1;
		}

		/* Queue the completion event for the current transfer */
		rc = mhi_dev_queue_transfer_completion(mreq, NULL);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to queue completion for ch %d, rc %d\n",
				ch->ch_id, rc);
			return rc;
		}

		rc = ipa_dma_async_memcpy(mreq->dma, host_addr_pa,
				(int) len, mhi_dev_transfer_completion_cb,
				mreq);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"DMA read error %d for ch %d\n", rc, ch->ch_id);
			/* Roll back the completion event that we wrote above */
			mhi_dev_rollback_compl_evt(ch);
			/* Unmap the buffer */
			dma_unmap_single(&mhi_ctx->pdev->dev, mreq->dma,
							len, DMA_FROM_DEVICE);
			return rc;
		}
	}
	return rc;
}

/**
 * mhi_transfer_device_to_host_ipa - memcpy equivalent API to transfer data
 *		from device to the host.
 * @host_addr:	Physical destination address.
 * @dev:	Source virtual address.
 * @len:	Number of bytes to be transferred.
 * @mhi:	MHI dev structure.
 * @req:	mhi_req structure
 */
int mhi_transfer_device_to_host_ipa(uint64_t host_addr, void *dev, uint32_t len,
		struct mhi_dev *mhi, struct mhi_req *req)
{
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	struct mhi_dev_ring *ring = NULL;
	bool flush = false;
	struct mhi_dev_channel *ch;
	u32 snd_cmpl;
	int rc;

	if (WARN_ON(!mhi || !dev || !req  || !host_addr))
		return -EINVAL;

	if (mhi->config_iatu) {
		offset = (uint64_t)host_addr - mhi->data_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->data_base.device_pa + offset;
	} else {
		host_addr_pa = host_addr | bit_40;
	}
	mhi_log(MHI_MSG_VERBOSE, "device 0x%llx ---> host 0x%llx, size %d\n",
				(uint64_t) mhi->write_dma_handle,
				host_addr_pa, (int) len);

	if (req->mode == DMA_SYNC) {
		memcpy(mhi->write_handle, dev, len);
		return ipa_dma_sync_memcpy(host_addr_pa,
				(u64) mhi->write_dma_handle, (int) len);
	} else if (req->mode == DMA_ASYNC) {
		ch = req->client->channel;

		req->dma = dma_map_single(&mhi->pdev->dev, req->buf,
				req->len, DMA_TO_DEVICE);

		ring = ch->ring;
		mhi_dev_ring_inc_index(ring, ring->rd_offset);
		if (ring->rd_offset == ring->wr_offset)
			req->snd_cmpl = 1;
		snd_cmpl = req->snd_cmpl;

		/* Queue the completion event for the current transfer */
		rc = mhi_dev_queue_transfer_completion(req, &flush);
		if (rc) {
			pr_err("Failed to queue completion: %d\n", rc);
			return rc;
		}

		rc = ipa_dma_async_memcpy(host_addr_pa,
				(uint64_t) req->dma, (int) len,
				mhi_dev_transfer_completion_cb, req);
		if (rc) {
			mhi_log(MHI_MSG_ERROR, "Error sending data to host\n");
			/* Roll back the completion event that we wrote above */
			mhi_dev_rollback_compl_evt(ch);
			/* Unmap the buffer */
			dma_unmap_single(&mhi_ctx->pdev->dev, req->dma,
				req->len, DMA_TO_DEVICE);
			return rc;
		}
		if (snd_cmpl || flush) {
			rc = mhi_dev_flush_transfer_completion_events(mhi, ch);
			if (rc) {
				mhi_log(MHI_MSG_ERROR,
					"Failed to flush write completions to host\n");
				return rc;
			}
		}
	}
	return 0;
}

/**
 * mhi_dev_read_from_host_edma - memcpy equivalent API to transfer data
 *		from host to device.
 * @mhi:	MHI dev structure.
 * @transfer:	Host and device address details.
 */
void mhi_dev_read_from_host_edma(struct mhi_dev *mhi, struct mhi_addr *transfer)
{
	uint64_t host_addr_pa = 0, offset = 0;
	struct dma_async_tx_descriptor *descriptor;

	reinit_completion(&read_from_host);

	if (mhi->config_iatu) {
		offset = (uint64_t) transfer->host_pa - mhi->ctrl_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->ctrl_base.device_pa + offset;
	} else {
		host_addr_pa = transfer->host_pa;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"device 0x%llx <<-- host 0x%llx, size %d\n",
		transfer->phy_addr, host_addr_pa,
		(int) transfer->size);

	descriptor = dmaengine_prep_dma_memcpy(mhi->rx_dma_chan,
				transfer->phy_addr, host_addr_pa,
				(int)transfer->size, DMA_PREP_INTERRUPT);
	if (!descriptor) {
		pr_err("%s(): descriptor is null\n", __func__);
		return;
	}
	descriptor->callback_param = &read_from_host;
	descriptor->callback = mhi_dev_edma_sync_cb;
	dma_async_issue_pending(mhi->rx_dma_chan);

	if (!wait_for_completion_timeout
			(&read_from_host, msecs_to_jiffies(1000)))
		mhi_log(MHI_MSG_ERROR, "read from host timed out\n");
}

/**
 * mhi_dev_write_to_host_edma - Transfer data from device to host using eDMA.
 * @mhi:	MHI dev structure.
 * @transfer:	Host and device address details.
 * @ereq:	event_req structure.
 * @tr_type:	Transfer type.
 */
void mhi_dev_write_to_host_edma(struct mhi_dev *mhi, struct mhi_addr *transfer,
		struct event_req *ereq, enum mhi_dev_transfer_type tr_type)
{
	uint64_t host_addr_pa = 0, offset = 0;
	dma_addr_t dma;
	struct dma_async_tx_descriptor *descriptor;

	if (mhi->config_iatu) {
		offset = (uint64_t) transfer->host_pa - mhi->ctrl_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->ctrl_base.device_pa + offset;
	} else {
		host_addr_pa = transfer->host_pa;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"device 0x%llx --> host 0x%llx, size %d, type = %d\n",
		mhi->cache_dma_handle, host_addr_pa,
		(int) transfer->size, tr_type);
	if (tr_type == MHI_DEV_DMA_ASYNC) {
		/*
		 * Event read pointer memory is dma_alloc_coherent memory
		 * don't need to dma_map. Assigns the physical address in
		 * phy_addr.
		 */
		if (transfer->phy_addr) {
			dma = transfer->phy_addr;
		} else {
			dma = dma_map_single(&mhi->pdev->dev,
				transfer->virt_addr, transfer->size,
				DMA_TO_DEVICE);
			if (dma_mapping_error(&mhi->pdev->dev, dma)) {
				pr_err("%s(): dma mapping failed\n", __func__);
				return;
			}
		}

		if (ereq->event_type == SEND_EVENT_BUFFER) {
			ereq->dma = dma;
			ereq->dma_len = transfer->size;
		} else {
			/*
			 * Event read pointer memory is dma_alloc_coherent
			 * memory. Don't need to dma_unmap.
			 */
			if (transfer->phy_addr)
				ereq->event_rd_dma = 0;
			else
				ereq->event_rd_dma = dma;
		}

		descriptor = dmaengine_prep_dma_memcpy(
				mhi->tx_dma_chan, host_addr_pa,
				dma, (int)transfer->size,
				DMA_PREP_INTERRUPT);
		if (!descriptor) {
			pr_err("%s(): descriptor is null\n", __func__);
			dma_unmap_single(&mhi->pdev->dev,
				(size_t)transfer->virt_addr, transfer->size,
				DMA_TO_DEVICE);
			return;
		}
		descriptor->callback_param = ereq;
		descriptor->callback = ereq->client_cb;
		dma_async_issue_pending(mhi->tx_dma_chan);
	} else if (tr_type == MHI_DEV_DMA_SYNC) {
		reinit_completion(&write_to_host);

		/* Copy the device content to local device physical address */
		memcpy(mhi->dma_cache, transfer->virt_addr, transfer->size);

		descriptor = dmaengine_prep_dma_memcpy(
				mhi->tx_dma_chan, host_addr_pa,
				mhi->cache_dma_handle,
				(int)transfer->size,
				DMA_PREP_INTERRUPT);
		if (!descriptor) {
			pr_err("%s(): descriptor is null\n", __func__);
			return;
		}

		descriptor->callback_param = &write_to_host;
		descriptor->callback = mhi_dev_edma_sync_cb;
		dma_async_issue_pending(mhi->tx_dma_chan);
		if (!wait_for_completion_timeout
			(&write_to_host, msecs_to_jiffies(1000)))
			mhi_log(MHI_MSG_ERROR, "write to host timed out\n");
	}
}

/**
 * mhi_transfer_host_to_device_edma - memcpy equivalent API to transfer data
 *		from host to the device.
 * @dev:	Physical destination virtual address.
 * @host_pa:	Source physical address.
 * @len:	Number of bytes to be transferred.
 * @mhi:	MHI dev structure.
 * @mreq:	mhi_req structure
 */
int mhi_transfer_host_to_device_edma(void *dev, uint64_t host_pa, uint32_t len,
		struct mhi_dev *mhi, struct mhi_req *mreq)
{
	uint64_t host_addr_pa = 0, offset = 0;
	struct mhi_dev_ring *ring;
	struct dma_async_tx_descriptor *descriptor;
	struct mhi_dev_channel *ch;
	int rc;

	if (mhi->config_iatu) {
		offset = (uint64_t)host_pa - mhi->data_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->data_base.device_pa + offset;
	} else {
		host_addr_pa = host_pa;
	}

	mhi_log(MHI_MSG_VERBOSE, "device 0x%llx <-- host 0x%llx, size %d\n",
		mhi->read_dma_handle, host_addr_pa, (int) len);

	if (mreq->mode == DMA_SYNC) {
		reinit_completion(&transfer_host_to_device);

		descriptor = dmaengine_prep_dma_memcpy(
				mhi->rx_dma_chan,
				mhi->read_dma_handle,
				host_addr_pa, (int)len,
				DMA_PREP_INTERRUPT);
		if (!descriptor) {
			pr_err("%s(): descriptor is null\n", __func__);
			return -EFAULT;
		}
		descriptor->callback_param = &transfer_host_to_device;
		descriptor->callback = mhi_dev_edma_sync_cb;
		dma_async_issue_pending(mhi->rx_dma_chan);
		if (!wait_for_completion_timeout
			(&transfer_host_to_device, msecs_to_jiffies(1000))) {
			mhi_log(MHI_MSG_ERROR,
				"transfer host to device timed out\n");
			return -ETIMEDOUT;
		}

		memcpy(dev, mhi->read_handle, len);
	} else if (mreq->mode == DMA_ASYNC) {
		ch = mreq->client->channel;
		ring = ch->ring;
		mreq->dma = dma_map_single(&mhi->pdev->dev, dev, len,
				DMA_FROM_DEVICE);
		if (dma_mapping_error(&mhi->pdev->dev, mreq->dma)) {
			pr_err("%s(): dma map single failed\n", __func__);
			return -ENOMEM;
		}

		mhi_dev_ring_inc_index(ring, ring->rd_offset);

		if (ring->rd_offset == ring->wr_offset) {
			mhi_log(MHI_MSG_VERBOSE,
				"Setting snd_cmpl to 1 for ch %d\n", ch->ch_id);
			mreq->snd_cmpl = 1;
		}

		/* Queue the completion event for the current transfer */
		rc = mhi_dev_queue_transfer_completion(mreq, NULL);
		if (rc) {
			pr_err("Failed to queue completion: %d\n", rc);
			return rc;
		}

		descriptor = dmaengine_prep_dma_memcpy(
				mhi->rx_dma_chan, mreq->dma,
				host_addr_pa, (int)len,
				DMA_PREP_INTERRUPT);
		if (!descriptor) {
			pr_err("%s(): descriptor is null\n", __func__);
			/* Roll back the completion event that we wrote above */
			mhi_dev_rollback_compl_evt(ch);
			dma_unmap_single(&mhi->pdev->dev, (size_t)dev, len,
							DMA_FROM_DEVICE);
			return -EFAULT;
		}
		descriptor->callback_param = mreq;
		descriptor->callback =
			mhi_dev_transfer_completion_cb;
		dma_async_issue_pending(mhi->rx_dma_chan);
	}
	return 0;
}

/**
 * mhi_transfer_device_to_host_edma - memcpy equivalent API to transfer data
 *		from device to the host.
 * @host_addr:	Physical destination address.
 * @dev:	Source virtual address.
 * @len:	Numer of bytes to be transferred.
 * @mhi:	MHI dev structure.
 * @req:	mhi_req structure
 */
int mhi_transfer_device_to_host_edma(uint64_t host_addr, void *dev,
		uint32_t len, struct mhi_dev *mhi, struct mhi_req *req)
{
	uint64_t host_addr_pa = 0, offset = 0;
	struct mhi_dev_ring *ring;
	struct dma_async_tx_descriptor *descriptor;
	bool flush = false;
	struct mhi_dev_channel *ch;
	int rc;

	if (mhi->config_iatu) {
		offset = (uint64_t)host_addr - mhi->data_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->data_base.device_pa + offset;
	} else {
		host_addr_pa = host_addr;
	}
	mhi_log(MHI_MSG_VERBOSE, "device 0x%llx ---> host 0x%llx, size %d\n",
				mhi->write_dma_handle,
				host_addr_pa, (int) len);

	if (req->mode == DMA_SYNC) {
		reinit_completion(&transfer_device_to_host);

		descriptor = dmaengine_prep_dma_memcpy(mhi->tx_dma_chan,
			host_addr_pa, mhi->write_dma_handle,
			(int)len, DMA_PREP_INTERRUPT);
		if (!descriptor) {
			pr_err("%s(): descriptor is null\n", __func__);
			return -EFAULT;
		}
		descriptor->callback_param = &transfer_device_to_host;
		descriptor->callback = mhi_dev_edma_sync_cb;
		dma_async_issue_pending(mhi->tx_dma_chan);

		if (!wait_for_completion_timeout
			(&transfer_device_to_host, msecs_to_jiffies(1000))) {
			mhi_log(MHI_MSG_ERROR,
					"transfer device to host timed out\n");
			return -ETIMEDOUT;
		}
	} else if (req->mode == DMA_ASYNC) {
		ch = req->client->channel;
		req->dma = dma_map_single(&mhi->pdev->dev, req->buf,
				req->len, DMA_TO_DEVICE);
		if (dma_mapping_error(&mhi->pdev->dev, req->dma)) {
			pr_err("%s(): dma map single failed\n", __func__);
			return -ENOMEM;
		}

		ring = ch->ring;
		mhi_dev_ring_inc_index(ring, ring->rd_offset);
		if (ring->rd_offset == ring->wr_offset)
			req->snd_cmpl = 1;

		/* Queue the completion event for the current transfer */
		rc = mhi_dev_queue_transfer_completion(req, &flush);
		if (rc) {
			pr_err("Failed to queue completion: %d\n", rc);
			return rc;
		}

		descriptor = dmaengine_prep_dma_memcpy(mhi->tx_dma_chan,
			host_addr_pa, req->dma, (int) len,
			DMA_PREP_INTERRUPT);
		if (!descriptor) {
			pr_err("%s(): descriptor is null\n", __func__);
			/* Roll back the completion event that we wrote above */
			mhi_dev_rollback_compl_evt(ch);
			/* Unmap the buffer */
			dma_unmap_single(&mhi->pdev->dev, (size_t)req->buf,
				req->len, DMA_TO_DEVICE);
			return -EFAULT;
		}
		descriptor->callback_param = req;
		descriptor->callback = mhi_dev_transfer_completion_cb;

		dma_async_issue_pending(mhi->tx_dma_chan);

		if (flush) {
			rc = mhi_dev_flush_transfer_completion_events(mhi, ch);
			if (rc) {
				mhi_log(MHI_MSG_ERROR,
					"Failed to flush write completions to host\n");
				return rc;
			}
		}
	}
	return 0;
}

int mhi_dev_is_list_empty(void)
{
	if (list_empty(&mhi_ctx->event_ring_list) &&
			list_empty(&mhi_ctx->process_ring_list))
		return 0;

	return 1;
}
EXPORT_SYMBOL(mhi_dev_is_list_empty);

static void mhi_dev_get_erdb_db_cfg(struct mhi_dev *mhi,
				struct ep_pcie_db_config *erdb_cfg)
{
	if (mhi->cfg.event_rings == NUM_CHANNELS) {
		erdb_cfg->base = HW_CHANNEL_BASE;
		erdb_cfg->end = HW_CHANNEL_END;
	} else {
		erdb_cfg->base = mhi->cfg.event_rings -
					(mhi->cfg.hw_event_rings);
		erdb_cfg->end =  mhi->cfg.event_rings -
					MHI_HW_ACC_EVT_RING_END;
	}
}

int mhi_pcie_config_db_routing(struct mhi_dev *mhi)
{
	struct ep_pcie_db_config chdb_cfg, erdb_cfg;

	if (WARN_ON(!mhi))
		return -EINVAL;

	/* Configure Doorbell routing */
	chdb_cfg.base = HW_CHANNEL_BASE;
	chdb_cfg.end = HW_CHANNEL_END;
	chdb_cfg.tgt_addr = (uint32_t) mhi->ipa_uc_mbox_crdb;

	mhi_dev_get_erdb_db_cfg(mhi, &erdb_cfg);

	mhi_log(MHI_MSG_VERBOSE,
		"Event rings 0x%x => er_base 0x%x, er_end %d\n",
		mhi->cfg.event_rings, erdb_cfg.base, erdb_cfg.end);
	erdb_cfg.tgt_addr = (uint32_t) mhi->ipa_uc_mbox_erdb;
	ep_pcie_config_db_routing(mhi_ctx->phandle, chdb_cfg, erdb_cfg);

	return 0;
}
EXPORT_SYMBOL(mhi_pcie_config_db_routing);

static int mhi_enable_int(void)
{
	int rc = 0;

	mhi_log(MHI_MSG_VERBOSE,
		"Enable chdb, ctrl and cmdb interrupts\n");

	rc = mhi_dev_mmio_enable_chdb_interrupts(mhi_ctx);
	if (rc) {
		pr_err("Failed to enable channel db: %d\n", rc);
		return rc;
	}

	rc = mhi_dev_mmio_enable_ctrl_interrupt(mhi_ctx);
	if (rc) {
		pr_err("Failed to enable control interrupt: %d\n", rc);
		return rc;
	}

	rc = mhi_dev_mmio_enable_cmdb_interrupt(mhi_ctx);
	if (rc) {
		pr_err("Failed to enable command db: %d\n", rc);
		return rc;
	}
	mhi_update_state_info(MHI_STATE_CONNECTED);
	if (!mhi_ctx->mhi_int)
		ep_pcie_mask_irq_event(mhi_ctx->phandle,
				EP_PCIE_INT_EVT_MHI_A7, true);
	return 0;
}

static int mhi_hwc_init(struct mhi_dev *mhi)
{
	int rc = 0;
	struct ep_pcie_msi_config cfg;
	struct ipa_mhi_init_params ipa_init_params;
	struct ep_pcie_db_config erdb_cfg;

	if (mhi_ctx->use_edma) {
		/*
		 * Interrupts are enabled during the IPA callback
		 * once the IPA HW is ready. Callback is not triggerred
		 * in case of edma, hence enable interrupts.
		 */
		rc = mhi_enable_int();
		if (rc)
			pr_err("Error configuring interrupts: rc = %d\n", rc);
		return rc;
	}

	/* Call IPA HW_ACC Init with MSI Address and db routing info */
	rc = ep_pcie_get_msi_config(mhi_ctx->phandle, &cfg);
	if (rc) {
		pr_err("Error retrieving pcie msi logic\n");
		return rc;
	}

	rc = mhi_pcie_config_db_routing(mhi);
	if (rc) {
		pr_err("Error configuring DB routing\n");
		return rc;
	}

	mhi_dev_get_erdb_db_cfg(mhi, &erdb_cfg);
	mhi_log(MHI_MSG_VERBOSE,
		"Event rings 0x%x => er_base 0x%x, er_end %d\n",
		mhi->cfg.event_rings, erdb_cfg.base, erdb_cfg.end);

	erdb_cfg.tgt_addr = (uint32_t) mhi->ipa_uc_mbox_erdb;
	memset(&ipa_init_params, 0, sizeof(ipa_init_params));
	ipa_init_params.msi.addr_hi = cfg.upper;
	ipa_init_params.msi.addr_low = cfg.lower;
	ipa_init_params.msi.data = cfg.data;
	ipa_init_params.msi.mask = ((1 << cfg.msg_num) - 1);
	ipa_init_params.first_er_idx = erdb_cfg.base;
	ipa_init_params.first_ch_idx = HW_CHANNEL_BASE;

	if (mhi_ctx->config_iatu)
		ipa_init_params.mmio_addr =
			((uint32_t) mhi_ctx->mmio_base_pa_addr) + MHI_REGLEN;
	else
		ipa_init_params.mmio_addr =
			((uint32_t) mhi_ctx->mmio_base_pa_addr);

	if (!mhi_ctx->config_iatu)
		ipa_init_params.assert_bit40 = true;

	mhi_log(MHI_MSG_VERBOSE,
		"MMIO Addr 0x%x, MSI config: U:0x%x L: 0x%x D: 0x%x\n",
		ipa_init_params.mmio_addr, cfg.upper, cfg.lower, cfg.data);
	ipa_init_params.notify = mhi_hwc_cb;
	ipa_init_params.priv = mhi;

	return ipa_mhi_init(&ipa_init_params);
}

static int mhi_hwc_start(struct mhi_dev *mhi)
{
	struct ipa_mhi_start_params ipa_start_params;

	memset(&ipa_start_params, 0, sizeof(ipa_start_params));

	if (mhi->config_iatu) {
		ipa_start_params.host_ctrl_addr = mhi->ctrl_base.device_pa;
		ipa_start_params.host_data_addr = mhi->data_base.device_pa;
	} else {
		ipa_start_params.channel_context_array_addr =
				mhi->ch_ctx_shadow.host_pa;
		ipa_start_params.event_context_array_addr =
				mhi->ev_ctx_shadow.host_pa;
	}

	return ipa_mhi_start(&ipa_start_params);
}

static void mhi_hwc_cb(void *priv, enum ipa_mhi_event_type event,
	unsigned long data)
{
	int rc = 0;

	switch (event) {
	case IPA_MHI_EVENT_READY:
		mhi_log(MHI_MSG_INFO,
			"HW Channel uC is ready event=0x%X\n", event);
		rc = mhi_hwc_start(mhi_ctx);
		if (rc) {
			pr_err("hwc_init start failed with %d\n", rc);
			return;
		}

		rc = mhi_enable_int();
		if (rc) {
			pr_err("Error configuring interrupts, rc = %d\n", rc);
			return;
		}

		mhi_log(MHI_MSG_CRITICAL, "Device in M0 State\n");
		update_marker("MHI - Device in M0 State\n");
		break;
	case IPA_MHI_EVENT_DATA_AVAILABLE:
		rc = mhi_dev_notify_sm_event(MHI_DEV_EVENT_HW_ACC_WAKEUP);
		if (rc) {
			pr_err("Event HW_ACC_WAKEUP failed with %d\n", rc);
			return;
		}
		break;
	default:
		pr_err("HW Channel uC unknown event 0x%X\n", event);
		break;
	}
}

static int mhi_hwc_chcmd(struct mhi_dev *mhi, uint chid,
				enum mhi_dev_ring_element_type_id type)
{
	int rc = -EINVAL;
	struct ipa_mhi_connect_params connect_params;

	memset(&connect_params, 0, sizeof(connect_params));

	switch (type) {
	case MHI_DEV_RING_EL_RESET:
	case MHI_DEV_RING_EL_STOP:
		if ((chid-HW_CHANNEL_BASE) > NUM_HW_CHANNELS) {
			pr_err("Invalid Channel ID = 0x%X\n", chid);
			return -EINVAL;
		}

		rc = ipa_mhi_disconnect_pipe(
			mhi->ipa_clnt_hndl[chid-HW_CHANNEL_BASE]);
		if (rc)
			pr_err("Stopping HW Channel%d failed 0x%X\n",
							chid, rc);
		break;
	case MHI_DEV_RING_EL_START:
		connect_params.channel_id = chid;
		connect_params.sys.skip_ep_cfg = true;

		if (chid > HW_CHANNEL_END) {
			pr_err("Channel DB for %d not enabled\n", chid);
			return -EINVAL;
		}

		if ((chid-HW_CHANNEL_BASE) > NUM_HW_CHANNELS) {
			pr_err("Invalid Channel = 0x%X\n", chid);
			return -EINVAL;
		}

		rc = ipa_mhi_connect_pipe(&connect_params,
			&mhi->ipa_clnt_hndl[chid-HW_CHANNEL_BASE]);
		if (rc)
			pr_err("HW Channel%d start failed : %d\n",
							chid, rc);
		break;
	case MHI_DEV_RING_EL_INVALID:
	default:
		pr_err("Invalid Ring Element type = 0x%X\n", type);
		break;
	}

	return rc;
}

static void mhi_dev_core_ack_ctrl_interrupts(struct mhi_dev *dev,
							uint32_t *int_value)
{
	int rc = 0;

	rc = mhi_dev_mmio_read(dev, MHI_CTRL_INT_STATUS_A7, int_value);
	if (rc) {
		pr_err("Failed to read A7 status\n");
		return;
	}

	rc = mhi_dev_mmio_write(dev, MHI_CTRL_INT_CLEAR_A7, *int_value);
	if (rc) {
		pr_err("Failed to clear A7 status\n");
		return;
	}
}

static void mhi_dev_fetch_ch_ctx(struct mhi_dev *mhi, uint32_t ch_id)
{
	struct mhi_addr data_transfer;

	if (MHI_USE_DMA(mhi)) {
		data_transfer.host_pa = mhi->ch_ctx_shadow.host_pa +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;
		data_transfer.phy_addr = mhi->ch_ctx_cache_dma_handle +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;
	}

	data_transfer.size  = sizeof(struct mhi_dev_ch_ctx);
	/* Fetch the channel ctx (*dst, *src, size) */
	mhi_ctx->read_from_host(mhi, &data_transfer);
}

int mhi_dev_syserr(struct mhi_dev *mhi)
{
	if (WARN_ON(!mhi))
		return -EINVAL;

	pr_err("MHI dev sys error\n");

	return mhi_dev_dump_mmio(mhi);
}
EXPORT_SYMBOL(mhi_dev_syserr);

int mhi_dev_send_event(struct mhi_dev *mhi, int evnt_ring,
					union mhi_dev_ring_element_type *el)
{
	int rc = 0;
	uint64_t evnt_ring_idx = mhi->ev_ring_start + evnt_ring;
	struct mhi_dev_ring *ring = &mhi->ring[evnt_ring_idx];
	union mhi_dev_ring_ctx *ctx;
	struct ep_pcie_msi_config cfg;
	struct mhi_addr transfer_addr;

	rc = ep_pcie_get_msi_config(mhi->phandle, &cfg);
	if (rc) {
		pr_err("Error retrieving pcie msi logic\n");
		return rc;
	}

	if (evnt_ring_idx > mhi->cfg.event_rings) {
		pr_err("Invalid event ring idx: %lld\n", evnt_ring_idx);
		return -EINVAL;
	}

	ctx = (union mhi_dev_ring_ctx *)&mhi->ev_ctx_cache[evnt_ring];
	if (mhi_ring_get_state(ring) == RING_STATE_UINT) {
		rc = mhi_ring_start(ring, ctx, mhi);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"error starting event ring %d\n", evnt_ring);
			return rc;
		}
	}

	mutex_lock(&mhi->mhi_event_lock);
	/* add the ring element */
	mhi_dev_add_element(ring, el, NULL, 0);

	ring->ring_ctx_shadow->ev.rp =  (ring->rd_offset *
				sizeof(union mhi_dev_ring_element_type)) +
				ring->ring_ctx->generic.rbase;

	mhi_log(MHI_MSG_VERBOSE, "ev.rp = %llx for %lld\n",
				ring->ring_ctx_shadow->ev.rp, evnt_ring_idx);

	if (MHI_USE_DMA(mhi))
		transfer_addr.host_pa = (mhi->ev_ctx_shadow.host_pa +
			sizeof(struct mhi_dev_ev_ctx) *
			evnt_ring) + (size_t) &ring->ring_ctx->ev.rp -
			(size_t) ring->ring_ctx;
	else
		transfer_addr.device_va = (mhi->ev_ctx_shadow.device_va +
			sizeof(struct mhi_dev_ev_ctx) *
			evnt_ring) + (size_t) &ring->ring_ctx->ev.rp -
			(size_t) ring->ring_ctx;

	transfer_addr.virt_addr = &ring->ring_ctx_shadow->ev.rp;
	transfer_addr.size = sizeof(uint64_t);
	transfer_addr.phy_addr = 0;

	mhi_ctx->write_to_host(mhi, &transfer_addr, NULL, MHI_DEV_DMA_SYNC);
	/*
	 * rp update in host memory should be flushed
	 * before sending a MSI to the host
	 */
	wmb();

	mutex_unlock(&mhi->mhi_event_lock);
	mhi_log(MHI_MSG_VERBOSE, "event sent:\n");
	mhi_log(MHI_MSG_VERBOSE, "evnt ptr : 0x%llx\n", el->evt_tr_comp.ptr);
	mhi_log(MHI_MSG_VERBOSE, "evnt len : 0x%x\n", el->evt_tr_comp.len);
	mhi_log(MHI_MSG_VERBOSE, "evnt code :0x%x\n", el->evt_tr_comp.code);
	mhi_log(MHI_MSG_VERBOSE, "evnt type :0x%x\n", el->evt_tr_comp.type);
	mhi_log(MHI_MSG_VERBOSE, "evnt chid :0x%x\n", el->evt_tr_comp.chid);

	if (mhi_ctx->use_edma)
		rc = mhi_trigger_msi_edma(ring, ctx->ev.msivec);
	else
		rc = ep_pcie_trigger_msi(mhi_ctx->phandle, ctx->ev.msivec);

	return rc;
}

static int mhi_dev_send_completion_event_async(struct mhi_dev_channel *ch,
			size_t rd_ofst, uint32_t len,
			enum mhi_dev_cmd_completion_code code,
			struct mhi_req *mreq)
{
	int rc;
	struct mhi_dev *mhi = ch->ring->mhi_dev;

	mhi_log(MHI_MSG_VERBOSE, "Ch %d\n", ch->ch_id);

	/* Queue the completion event for the current transfer */
	mreq->snd_cmpl = 1;
	rc = mhi_dev_queue_transfer_completion(mreq, NULL);
	if (rc) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to queue completion for ch %d, rc %d\n",
			ch->ch_id, rc);
		return rc;
	}

	mhi_log(MHI_MSG_VERBOSE, "Calling flush for ch %d\n", ch->ch_id);
	rc = mhi_dev_flush_transfer_completion_events(mhi, ch);
	if (rc) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to flush read completions to host\n");
		return rc;
	}

	return 0;
}

static int mhi_dev_send_completion_event(struct mhi_dev_channel *ch,
			size_t rd_ofst, uint32_t len,
			enum mhi_dev_cmd_completion_code code)
{
	union mhi_dev_ring_element_type compl_event;
	struct mhi_dev *mhi = ch->ring->mhi_dev;

	compl_event.evt_tr_comp.chid = ch->ch_id;
	compl_event.evt_tr_comp.type =
				MHI_DEV_RING_EL_TRANSFER_COMPLETION_EVENT;
	compl_event.evt_tr_comp.len = len;
	compl_event.evt_tr_comp.code = code;
	compl_event.evt_tr_comp.ptr = ch->ring->ring_ctx->generic.rbase +
			rd_ofst * sizeof(struct mhi_dev_transfer_ring_element);

	return mhi_dev_send_event(mhi,
			mhi->ch_ctx_cache[ch->ch_id].err_indx, &compl_event);
}

int mhi_dev_send_state_change_event(struct mhi_dev *mhi,
						enum mhi_dev_state state)
{
	union mhi_dev_ring_element_type event;

	event.evt_state_change.type = MHI_DEV_RING_EL_MHI_STATE_CHG;
	event.evt_state_change.mhistate = state;

	return mhi_dev_send_event(mhi, 0, &event);
}
EXPORT_SYMBOL(mhi_dev_send_state_change_event);

int mhi_dev_send_ee_event(struct mhi_dev *mhi, enum mhi_dev_execenv exec_env)
{
	union mhi_dev_ring_element_type event;

	event.evt_ee_state.type = MHI_DEV_RING_EL_EE_STATE_CHANGE_NOTIFY;
	event.evt_ee_state.execenv = exec_env;

	return mhi_dev_send_event(mhi, 0, &event);
}
EXPORT_SYMBOL(mhi_dev_send_ee_event);

static void mhi_dev_trigger_cb(enum mhi_client_channel ch_id)
{
	struct mhi_dev_ready_cb_info *info;
	enum mhi_ctrl_info state_data;

	/* Currently no clients register for HW channel notification */
	if (ch_id >= MHI_MAX_SOFTWARE_CHANNELS)
		return;

	list_for_each_entry(info, &mhi_ctx->client_cb_list, list)
		if (info->cb && info->cb_data.channel == ch_id) {
			mhi_ctrl_state_info(info->cb_data.channel, &state_data);
			info->cb_data.ctrl_info = state_data;
			info->cb(&info->cb_data);
		}
}

int mhi_dev_trigger_hw_acc_wakeup(struct mhi_dev *mhi)
{
	/*
	 * Expected usage is when there is HW ACC traffic IPA uC notifes
	 * Q6 -> IPA A7 -> MHI core -> MHI SM
	 */
	return mhi_dev_notify_sm_event(MHI_DEV_EVENT_HW_ACC_WAKEUP);
}
EXPORT_SYMBOL(mhi_dev_trigger_hw_acc_wakeup);

static int mhi_dev_send_cmd_comp_event(struct mhi_dev *mhi,
				enum mhi_dev_cmd_completion_code code)
{
	union mhi_dev_ring_element_type event;

	if (code > MHI_CMD_COMPL_CODE_RES) {
		mhi_log(MHI_MSG_ERROR,
			"Invalid cmd compl code: %d\n", code);
		return -EINVAL;
	}

	/* send the command completion event to the host */
	event.evt_cmd_comp.ptr = mhi->cmd_ctx_cache->rbase
			+ (mhi->ring[MHI_RING_CMD_ID].rd_offset *
			(sizeof(union mhi_dev_ring_element_type)));
	mhi_log(MHI_MSG_VERBOSE, "evt cmd comp ptr :%lx\n",
			(size_t) event.evt_cmd_comp.ptr);
	event.evt_cmd_comp.type = MHI_DEV_RING_EL_CMD_COMPLETION_EVT;
	event.evt_cmd_comp.code = code;
	return mhi_dev_send_event(mhi, 0, &event);
}

static int mhi_dev_process_stop_cmd(struct mhi_dev_ring *ring, uint32_t ch_id,
							struct mhi_dev *mhi)
{
	struct mhi_addr data_transfer;

	if (ring->rd_offset != ring->wr_offset &&
		mhi->ch_ctx_cache[ch_id].ch_type ==
				MHI_DEV_CH_TYPE_OUTBOUND_CHANNEL) {
		mhi_log(MHI_MSG_INFO, "Pending outbound transaction\n");
		return 0;
	} else if (mhi->ch_ctx_cache[ch_id].ch_type ==
			MHI_DEV_CH_TYPE_INBOUND_CHANNEL &&
			(mhi->ch[ch_id].pend_wr_count > 0)) {
		mhi_log(MHI_MSG_INFO, "Pending inbound transaction\n");
		return 0;
	}

	/* set the channel to stop */
	mhi->ch_ctx_cache[ch_id].ch_state = MHI_DEV_CH_STATE_STOP;
	mhi->ch[ch_id].state = MHI_DEV_CH_STOPPED;

	if (MHI_USE_DMA(mhi)) {
		data_transfer.host_pa = mhi->ch_ctx_shadow.host_pa +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
	} else {
		data_transfer.device_va = mhi->ch_ctx_shadow.device_va +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
		data_transfer.device_pa = mhi->ch_ctx_shadow.device_pa +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
	}
	data_transfer.size = sizeof(enum mhi_dev_ch_ctx_state);
	data_transfer.virt_addr = &mhi->ch_ctx_cache[ch_id].ch_state;

	/* update the channel state in the host */
	mhi_ctx->write_to_host(mhi, &data_transfer, NULL, MHI_DEV_DMA_SYNC);

	/* send the completion event to the host */
	return mhi_dev_send_cmd_comp_event(mhi,
					MHI_CMD_COMPL_CODE_SUCCESS);
}

static void mhi_dev_process_cmd_ring(struct mhi_dev *mhi,
			union mhi_dev_ring_element_type *el, void *ctx)
{
	int rc = 0;
	uint32_t ch_id = 0;
	union mhi_dev_ring_element_type event;
	struct mhi_addr host_addr;
	struct mhi_dev_channel *ch;
	struct mhi_dev_ring *ring;
	union mhi_dev_ring_ctx *evt_ctx;

	ch_id = el->generic.chid;
	mhi_log(MHI_MSG_VERBOSE, "for channel:%d and cmd:%d\n",
		ch_id, el->generic.type);

	switch (el->generic.type) {
	case MHI_DEV_RING_EL_START:
		mhi_log(MHI_MSG_VERBOSE, "recived start cmd for channel %d\n",
								ch_id);
		if (ch_id >= (HW_CHANNEL_BASE)) {
			rc = mhi_hwc_chcmd(mhi, ch_id, el->generic.type);
			if (rc) {
				mhi_log(MHI_MSG_ERROR,
					"Error with HW channel cmd %d\n", rc);
				rc = mhi_dev_send_cmd_comp_event(mhi,
						MHI_CMD_COMPL_CODE_UNDEFINED);
				if (rc)
					mhi_log(MHI_MSG_ERROR,
						"Error with compl event\n");
				return;
			}
			goto send_start_completion_event;
		}

		/* fetch the channel context from host */
		mhi_dev_fetch_ch_ctx(mhi, ch_id);

		/* Initialize and configure the corresponding channel ring */
		rc = mhi_ring_start(&mhi->ring[mhi->ch_ring_start + ch_id],
			(union mhi_dev_ring_ctx *)&mhi->ch_ctx_cache[ch_id],
			mhi);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"start ring failed for ch %d\n", ch_id);
			rc = mhi_dev_send_cmd_comp_event(mhi,
						MHI_CMD_COMPL_CODE_UNDEFINED);
			if (rc)
				mhi_log(MHI_MSG_ERROR,
					"Error with compl event\n");
			return;
		}

		mhi->ring[mhi->ch_ring_start + ch_id].state =
						RING_STATE_PENDING;

		/* set the channel to running */
		mhi->ch_ctx_cache[ch_id].ch_state = MHI_DEV_CH_STATE_RUNNING;
		mhi->ch[ch_id].state = MHI_DEV_CH_STARTED;
		mhi->ch[ch_id].ch_id = ch_id;
		mhi->ch[ch_id].ring = &mhi->ring[mhi->ch_ring_start + ch_id];
		mhi->ch[ch_id].ch_type = mhi->ch_ctx_cache[ch_id].ch_type;

		/* enable DB for event ring */
		rc = mhi_dev_mmio_enable_chdb_a7(mhi, ch_id);
		if (rc) {
			pr_err("Failed to enable channel db\n");
			rc = mhi_dev_send_cmd_comp_event(mhi,
						MHI_CMD_COMPL_CODE_UNDEFINED);
			if (rc)
				mhi_log(MHI_MSG_ERROR,
					"Error with compl event\n");
			return;
		}

		if (mhi->use_edma || mhi->use_ipa) {
			uint32_t evnt_ring_idx = mhi->ev_ring_start +
					mhi->ch_ctx_cache[ch_id].err_indx;
			struct mhi_dev_ring *evt_ring =
				&mhi->ring[evnt_ring_idx];
			evt_ctx = (union mhi_dev_ring_ctx *)&mhi->ev_ctx_cache
				[mhi->ch_ctx_cache[ch_id].err_indx];
			if (mhi_ring_get_state(evt_ring) == RING_STATE_UINT) {
				rc = mhi_ring_start(evt_ring, evt_ctx, mhi);
				if (rc) {
					mhi_log(MHI_MSG_ERROR,
					"error starting event ring %d\n",
					mhi->ch_ctx_cache[ch_id].err_indx);
					return;
				}
			}
			mhi_dev_alloc_evt_buf_evt_req(mhi, &mhi->ch[ch_id],
					evt_ring);
		}

		if (MHI_USE_DMA(mhi))
			host_addr.host_pa = mhi->ch_ctx_shadow.host_pa +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;
		else
			host_addr.device_va = mhi->ch_ctx_shadow.device_va +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;

		host_addr.virt_addr = &mhi->ch_ctx_cache[ch_id].ch_state;
		host_addr.size = sizeof(enum mhi_dev_ch_ctx_state);

		mhi_ctx->write_to_host(mhi, &host_addr, NULL, MHI_DEV_DMA_SYNC);

send_start_completion_event:
		rc = mhi_dev_send_cmd_comp_event(mhi,
						MHI_CMD_COMPL_CODE_SUCCESS);
		if (rc)
			pr_err("Error sending command completion event\n");

		mhi_update_state_info_ch(ch_id, MHI_STATE_CONNECTED);
		/* Trigger callback to clients */
		mhi_dev_trigger_cb(ch_id);
		mhi_uci_chan_state_notify(mhi, ch_id, MHI_STATE_CONNECTED);
		break;
	case MHI_DEV_RING_EL_STOP:
		if (ch_id >= HW_CHANNEL_BASE) {
			rc = mhi_hwc_chcmd(mhi, ch_id, el->generic.type);
			if (rc)
				mhi_log(MHI_MSG_ERROR,
					"send channel stop cmd event failed\n");

			/* send the completion event to the host */
			event.evt_cmd_comp.ptr = mhi->cmd_ctx_cache->rbase +
				(mhi->ring[MHI_RING_CMD_ID].rd_offset *
				(sizeof(union mhi_dev_ring_element_type)));
			event.evt_cmd_comp.type =
					MHI_DEV_RING_EL_CMD_COMPLETION_EVT;
			if (rc == 0)
				event.evt_cmd_comp.code =
					MHI_CMD_COMPL_CODE_SUCCESS;
			else
				event.evt_cmd_comp.code =
					MHI_CMD_COMPL_CODE_UNDEFINED;

			rc = mhi_dev_send_event(mhi, 0, &event);
			if (rc) {
				pr_err("stop event send failed\n");
				return;
			}
		} else {
			/*
			 * Check if there are any pending transactions for the
			 * ring associated with the channel. If no, proceed to
			 * write disable the channel state else send stop
			 * channel command to check if one can suspend the
			 * command.
			 */
			ring = &mhi->ring[ch_id + mhi->ch_ring_start];
			if (ring->state == RING_STATE_UINT) {
				pr_err("Channel not opened for %d\n", ch_id);
				return;
			}

			ch = &mhi->ch[ch_id];

			mutex_lock(&ch->ch_lock);

			mhi->ch[ch_id].state = MHI_DEV_CH_PENDING_STOP;
			rc = mhi_dev_process_stop_cmd(
				&mhi->ring[mhi->ch_ring_start + ch_id],
				ch_id, mhi);
			if (rc)
				pr_err("stop event send failed\n");

			mutex_unlock(&ch->ch_lock);
			mhi_update_state_info_ch(ch_id, MHI_STATE_DISCONNECTED);
			/* Trigger callback to clients */
			mhi_dev_trigger_cb(ch_id);
			mhi_uci_chan_state_notify(mhi, ch_id,
					MHI_STATE_DISCONNECTED);
		}
		break;
	case MHI_DEV_RING_EL_RESET:
		mhi_log(MHI_MSG_VERBOSE,
			"received reset cmd for channel %d\n", ch_id);
		if (ch_id >= HW_CHANNEL_BASE) {
			rc = mhi_hwc_chcmd(mhi, ch_id, el->generic.type);
			if (rc)
				mhi_log(MHI_MSG_ERROR,
					"send channel stop cmd event failed\n");

			/* send the completion event to the host */
			event.evt_cmd_comp.ptr = mhi->cmd_ctx_cache->rbase +
				(mhi->ring[MHI_RING_CMD_ID].rd_offset *
				(sizeof(union mhi_dev_ring_element_type)));
			event.evt_cmd_comp.type =
					MHI_DEV_RING_EL_CMD_COMPLETION_EVT;
			if (rc == 0)
				event.evt_cmd_comp.code =
					MHI_CMD_COMPL_CODE_SUCCESS;
			else
				event.evt_cmd_comp.code =
					MHI_CMD_COMPL_CODE_UNDEFINED;

			rc = mhi_dev_send_event(mhi, 0, &event);
			if (rc) {
				pr_err("stop event send failed\n");
				return;
			}
		} else {

			mhi_log(MHI_MSG_VERBOSE,
					"received reset cmd for channel %d\n",
					ch_id);

			ring = &mhi->ring[ch_id + mhi->ch_ring_start];
			if (ring->state == RING_STATE_UINT) {
				pr_err("Channel not opened for %d\n", ch_id);
				return;
			}

			ch = &mhi->ch[ch_id];

			mutex_lock(&ch->ch_lock);

			/* hard stop and set the channel to stop */
			mhi->ch_ctx_cache[ch_id].ch_state =
						MHI_DEV_CH_STATE_DISABLED;
			mhi->ch[ch_id].state = MHI_DEV_CH_STOPPED;
			if (MHI_USE_DMA(mhi))
				host_addr.host_pa =
					mhi->ch_ctx_shadow.host_pa +
					(sizeof(struct mhi_dev_ch_ctx) * ch_id);
			else
				host_addr.device_va =
					mhi->ch_ctx_shadow.device_va +
					(sizeof(struct mhi_dev_ch_ctx) * ch_id);

			host_addr.virt_addr =
					&mhi->ch_ctx_cache[ch_id].ch_state;
			host_addr.size = sizeof(enum mhi_dev_ch_ctx_state);

			/* update the channel state in the host */
			mhi_ctx->write_to_host(mhi, &host_addr, NULL,
					MHI_DEV_DMA_SYNC);

			/* send the completion event to the host */
			rc = mhi_dev_send_cmd_comp_event(mhi,
						MHI_CMD_COMPL_CODE_SUCCESS);
			if (rc)
				pr_err("Error sending command completion event\n");
			mutex_unlock(&ch->ch_lock);
			mhi_update_state_info_ch(ch_id, MHI_STATE_DISCONNECTED);
			mhi_dev_trigger_cb(ch_id);
			mhi_uci_chan_state_notify(mhi, ch_id,
					MHI_STATE_DISCONNECTED);
		}
		break;
	default:
		pr_err("%s: Invalid command:%d\n", __func__, el->generic.type);
		break;
	}
}

static void mhi_dev_process_tre_ring(struct mhi_dev *mhi,
			union mhi_dev_ring_element_type *el, void *ctx)
{
	struct mhi_dev_ring *ring = (struct mhi_dev_ring *)ctx;
	struct mhi_dev_channel *ch;
	struct mhi_dev_client_cb_reason reason;

	if (ring->id < mhi->ch_ring_start) {
		mhi_log(MHI_MSG_VERBOSE,
			"invalid channel ring id (%d), should be < %lu\n",
			ring->id, mhi->ch_ring_start);
		return;
	}

	ch = &mhi->ch[ring->id - mhi->ch_ring_start];
	reason.ch_id = ch->ch_id;
	reason.reason = MHI_DEV_TRE_AVAILABLE;

	/* Invoke a callback to let the client know its data is ready.
	 * Copy this event to the clients context so that it can be
	 * sent out once the client has fetch the data. Update the rp
	 * before sending the data as part of the event completion
	 */
	if (ch->active_client && ch->active_client->event_trigger != NULL)
		ch->active_client->event_trigger(&reason);
}

static void mhi_dev_process_ring_pending(struct work_struct *work)
{
	struct mhi_dev *mhi = container_of(work,
				struct mhi_dev, pending_work);
	struct list_head *cp, *q;
	struct mhi_dev_ring *ring;
	struct mhi_dev_channel *ch;
	int rc = 0;

	mutex_lock(&mhi_ctx->mhi_lock);
	rc = mhi_dev_process_ring(&mhi->ring[mhi->cmd_ring_idx]);
	if (rc) {
		mhi_log(MHI_MSG_ERROR, "error processing command ring\n");
		goto exit;
	}

	list_for_each_safe(cp, q, &mhi->process_ring_list) {
		ring = list_entry(cp, struct mhi_dev_ring, list);
		list_del(cp);
		mhi_log(MHI_MSG_VERBOSE, "processing ring %d\n", ring->id);
		rc = mhi_dev_process_ring(ring);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"error processing ring %d\n", ring->id);
			goto exit;
		}

		if (ring->id < mhi->ch_ring_start) {
			mhi_log(MHI_MSG_ERROR,
				"ring (%d) is not a channel ring\n", ring->id);
			goto exit;
		}

		ch = &mhi->ch[ring->id - mhi->ch_ring_start];
		rc = mhi_dev_mmio_enable_chdb_a7(mhi, ch->ch_id);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
			"error enabling chdb interrupt for %d\n", ch->ch_id);
			goto exit;
		}
	}

exit:
	mutex_unlock(&mhi_ctx->mhi_lock);
}

static int mhi_dev_get_event_notify(enum mhi_dev_state state,
						enum mhi_dev_event *event)
{
	int rc = 0;

	switch (state) {
	case MHI_DEV_M0_STATE:
		*event = MHI_DEV_EVENT_M0_STATE;
		break;
	case MHI_DEV_M1_STATE:
		*event = MHI_DEV_EVENT_M1_STATE;
		break;
	case MHI_DEV_M2_STATE:
		*event = MHI_DEV_EVENT_M2_STATE;
		break;
	case MHI_DEV_M3_STATE:
		*event = MHI_DEV_EVENT_M3_STATE;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static void mhi_dev_queue_channel_db(struct mhi_dev *mhi,
					uint32_t chintr_value, uint32_t ch_num)
{
	struct mhi_dev_ring *ring;
	int rc = 0;

	for (; chintr_value; ch_num++, chintr_value >>= 1) {
		if (chintr_value & 1) {
			ring = &mhi->ring[ch_num + mhi->ch_ring_start];
			if (ring->state == RING_STATE_UINT) {
				pr_debug("Channel not opened for %d\n", ch_num);
				continue;
			}
			mhi_ring_set_state(ring, RING_STATE_PENDING);
			list_add(&ring->list, &mhi->process_ring_list);
			rc = mhi_dev_mmio_disable_chdb_a7(mhi, ch_num);
			if (rc) {
				pr_err("Error disabling chdb\n");
				return;
			}
			queue_work(mhi->pending_ring_wq, &mhi->pending_work);
		}
	}
}

static void mhi_dev_check_channel_interrupt(struct mhi_dev *mhi)
{
	int i, rc = 0;
	uint32_t chintr_value = 0, ch_num = 0;

	rc = mhi_dev_mmio_read_chdb_status_interrupts(mhi);
	if (rc) {
		pr_err("Read channel db\n");
		return;
	}

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		ch_num = i * MHI_MASK_CH_EV_LEN;
		/* Process channel status whose mask is enabled */
		chintr_value = (mhi->chdb[i].status & mhi->chdb[i].mask);
		if (chintr_value) {
			mhi_log(MHI_MSG_VERBOSE,
				"processing id: %d, ch interrupt 0x%x\n",
							i, chintr_value);
			mhi_dev_queue_channel_db(mhi, chintr_value, ch_num);
			rc = mhi_dev_mmio_write(mhi, MHI_CHDB_INT_CLEAR_A7_n(i),
							mhi->chdb[i].status);
			if (rc) {
				pr_err("Error writing interrupt clear for A7\n");
				return;
			}
		}
	}
}

static void mhi_update_state_info_all(enum mhi_ctrl_info info)
{
	int i;
	struct mhi_dev_client_cb_reason reason;

	mhi_ctx->ctrl_info = info;
	for (i = 0; i < MHI_MAX_SOFTWARE_CHANNELS; ++i) {
		/*
		 * Skip channel state info change
		 * if channel is already in the desired state.
		 */
		if (channel_state_info[i].ctrl_info == info ||
		    (info == MHI_STATE_DISCONNECTED &&
		    channel_state_info[i].ctrl_info == MHI_STATE_CONFIGURED))
			continue;
		channel_state_info[i].ctrl_info = info;
		/* Notify kernel clients */
		mhi_dev_trigger_cb(i);
	}

	/* For legacy reasons for QTI client */
	reason.reason = MHI_DEV_CTRL_UPDATE;
	uci_ctrl_update(&reason);
}

static int mhi_dev_abort(struct mhi_dev *mhi)
{
	struct mhi_dev_channel *ch;
	struct mhi_dev_ring *ring;
	int ch_id = 0, rc = 0;

	/* Hard stop all the channels */
	for (ch_id = 0; ch_id < mhi->cfg.channels; ch_id++) {
		ring = &mhi->ring[ch_id + mhi->ch_ring_start];
		if (ring->state == RING_STATE_UINT)
			continue;

		ch = &mhi->ch[ch_id];
		mutex_lock(&ch->ch_lock);
		mhi->ch[ch_id].state = MHI_DEV_CH_STOPPED;
		mutex_unlock(&ch->ch_lock);
	}

	/* Update channel state and notify clients */
	mhi_update_state_info_all(MHI_STATE_DISCONNECTED);
	mhi_uci_chan_state_notify_all(mhi, MHI_STATE_DISCONNECTED);

	flush_workqueue(mhi->ring_init_wq);
	flush_workqueue(mhi->pending_ring_wq);

	/* Clean up initialized channels */
	rc = mhi_deinit(mhi);
	if (rc) {
		pr_err("Error during mhi_deinit with %d\n", rc);
		return rc;
	}

	rc = mhi_dev_mmio_mask_chdb_interrupts(mhi_ctx);
	if (rc) {
		pr_err("Failed to enable channel db\n");
		return rc;
	}

	rc = mhi_dev_mmio_disable_ctrl_interrupt(mhi_ctx);
	if (rc) {
		pr_err("Failed to enable control interrupt\n");
		return rc;
	}

	rc = mhi_dev_mmio_disable_cmdb_interrupt(mhi_ctx);
	if (rc) {
		pr_err("Failed to enable command db\n");
		return rc;
	}


	atomic_set(&mhi_ctx->re_init_done, 0);

	mhi_log(MHI_MSG_INFO,
			"Register a PCIe callback during re-init\n");
	mhi_ctx->event_reg.events = EP_PCIE_EVENT_LINKUP;
	mhi_ctx->event_reg.user = mhi_ctx;
	mhi_ctx->event_reg.mode = EP_PCIE_TRIGGER_CALLBACK;
	mhi_ctx->event_reg.callback = mhi_dev_resume_init_with_link_up;
	mhi_ctx->event_reg.options = MHI_REINIT;

	rc = ep_pcie_register_event(mhi_ctx->phandle,
					&mhi_ctx->event_reg);
	if (rc) {
		pr_err("Failed to register for events from PCIe\n");
		return rc;
	}

	/* Set RESET field to 0 */
	mhi_dev_mmio_reset(mhi_ctx);

	return rc;
}

static void mhi_dev_transfer_completion_cb(void *mreq)
{
	int rc = 0;
	struct mhi_req *req = mreq;
	struct mhi_dev_channel *ch = req->client->channel;
	u32 snd_cmpl = req->snd_cmpl;

	if (mhi_ctx->ch_ctx_cache[ch->ch_id].ch_type ==
			MHI_DEV_CH_TYPE_INBOUND_CHANNEL)
		ch->pend_wr_count--;

	dma_unmap_single(&mhi_ctx->pdev->dev, req->dma,
			req->len, DMA_FROM_DEVICE);

	/*
	 * Channel got stopped or closed with transfers pending
	 * Do not trigger callback or send cmpl to host
	 */
	if (ch->state == MHI_DEV_CH_CLOSED ||
		ch->state == MHI_DEV_CH_STOPPED) {
		mhi_log(MHI_MSG_DBG,
			"Ch %d not in started state, %d writes pending\n",
			ch->ch_id, ch->pend_wr_count + 1);
		return;
	}

	/* Trigger client call back */
	req->client_cb(req);

	/* Flush read completions to host */
	if (snd_cmpl && mhi_ctx->ch_ctx_cache[ch->ch_id].ch_type ==
				MHI_DEV_CH_TYPE_OUTBOUND_CHANNEL) {
		mhi_log(MHI_MSG_DBG, "Calling flush for ch %d\n", ch->ch_id);
		rc = mhi_dev_flush_transfer_completion_events(mhi_ctx, ch);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to flush read completions to host\n");
		}
	}

	if (ch->state == MHI_DEV_CH_PENDING_STOP) {
		ch->state = MHI_DEV_CH_STOPPED;
		rc = mhi_dev_process_stop_cmd(ch->ring, ch->ch_id, mhi_ctx);
		if (rc)
			mhi_log(MHI_MSG_ERROR,
			"Error while stopping channel (%d)\n", ch->ch_id);
	}
}

static void mhi_dev_scheduler(struct work_struct *work)
{
	struct mhi_dev *mhi = container_of(work,
				struct mhi_dev, chdb_ctrl_work);
	int rc = 0;
	uint32_t int_value = 0;
	struct mhi_dev_ring *ring;
	enum mhi_dev_state state;
	enum mhi_dev_event event = 0;
	u32 mhi_reset;

	mutex_lock(&mhi_ctx->mhi_lock);
	/* Check for interrupts */
	mhi_dev_core_ack_ctrl_interrupts(mhi, &int_value);

	if (int_value & MHI_MMIO_CTRL_INT_STATUS_A7_MSK) {
		mhi_log(MHI_MSG_VERBOSE,
			"processing ctrl interrupt with %d\n", int_value);

		rc = mhi_dev_mmio_read(mhi, BHI_IMGTXDB, &bhi_imgtxdb);
		mhi_log(MHI_MSG_DBG, "BHI_IMGTXDB = 0x%x\n", bhi_imgtxdb);

		rc = mhi_dev_mmio_get_mhi_state(mhi, &state, &mhi_reset);
		if (rc) {
			pr_err("%s: get mhi state failed\n", __func__);
			mutex_unlock(&mhi_ctx->mhi_lock);
			return;
		}

		if (mhi_reset) {
			mhi_log(MHI_MSG_VERBOSE,
				"processing mhi device reset\n");
			rc = mhi_dev_abort(mhi);
			if (rc)
				pr_err("device reset failed:%d\n", rc);
			mutex_unlock(&mhi_ctx->mhi_lock);
			queue_work(mhi->ring_init_wq, &mhi->re_init);
			return;
		}

		rc = mhi_dev_get_event_notify(state, &event);
		if (rc) {
			pr_err("unsupported state :%d\n", state);
			goto fail;
		}

		rc = mhi_dev_notify_sm_event(event);
		if (rc) {
			pr_err("error sending SM event\n");
			goto fail;
		}
	}

	if (int_value & MHI_MMIO_CTRL_CRDB_STATUS_MSK) {
		mhi_log(MHI_MSG_VERBOSE,
			"processing cmd db interrupt with %d\n", int_value);
		ring = &mhi->ring[MHI_RING_CMD_ID];
		ring->state = RING_STATE_PENDING;
		queue_work(mhi->pending_ring_wq, &mhi->pending_work);
	}

	/* get the specific channel interrupts */
	mhi_dev_check_channel_interrupt(mhi);

fail:
	mutex_unlock(&mhi_ctx->mhi_lock);

	if (mhi->config_iatu || mhi->mhi_int)
		enable_irq(mhi->mhi_irq);
	else
		ep_pcie_mask_irq_event(mhi->phandle,
				EP_PCIE_INT_EVT_MHI_A7, true);
}

void mhi_dev_notify_a7_event(struct mhi_dev *mhi)
{

	if (!atomic_read(&mhi->mhi_dev_wake)) {
		pm_stay_awake(mhi->dev);
		atomic_set(&mhi->mhi_dev_wake, 1);
	}
	mhi_log(MHI_MSG_VERBOSE, "acquiring mhi wakelock\n");

	schedule_work(&mhi->chdb_ctrl_work);
	mhi_log(MHI_MSG_VERBOSE, "mhi irq triggered\n");
}
EXPORT_SYMBOL(mhi_dev_notify_a7_event);

static irqreturn_t mhi_dev_isr(int irq, void *dev_id)
{
	struct mhi_dev *mhi = dev_id;

	if (!atomic_read(&mhi->mhi_dev_wake)) {
		pm_stay_awake(mhi->dev);
		atomic_set(&mhi->mhi_dev_wake, 1);
		mhi_log(MHI_MSG_VERBOSE, "acquiring mhi wakelock in ISR\n");
	}

	disable_irq_nosync(mhi->mhi_irq);
	schedule_work(&mhi->chdb_ctrl_work);
	mhi_log(MHI_MSG_VERBOSE, "mhi irq triggered\n");

	return IRQ_HANDLED;
}

int mhi_dev_config_outbound_iatu(struct mhi_dev *mhi)
{
	struct ep_pcie_iatu control, data;
	struct ep_pcie_iatu entries[MHI_HOST_REGION_NUM];

	data.start = mhi->data_base.device_pa;
	data.end = mhi->data_base.device_pa + mhi->data_base.size - 1;
	data.tgt_lower = HOST_ADDR_LSB(mhi->data_base.host_pa);
	data.tgt_upper = HOST_ADDR_MSB(mhi->data_base.host_pa);

	control.start = mhi->ctrl_base.device_pa;
	control.end = mhi->ctrl_base.device_pa + mhi->ctrl_base.size - 1;
	control.tgt_lower = HOST_ADDR_LSB(mhi->ctrl_base.host_pa);
	control.tgt_upper = HOST_ADDR_MSB(mhi->ctrl_base.host_pa);

	entries[0] = data;
	entries[1] = control;

	return ep_pcie_config_outbound_iatu(mhi_ctx->phandle, entries,
					MHI_HOST_REGION_NUM);
}
EXPORT_SYMBOL(mhi_dev_config_outbound_iatu);

static int mhi_dev_cache_host_cfg(struct mhi_dev *mhi)
{
	int rc = 0;
	struct platform_device *pdev;
	uint64_t addr1 = 0;
	struct mhi_addr data_transfer;

	pdev = mhi->pdev;

	/* Get host memory region configuration */
	mhi_dev_get_mhi_addr(mhi);

	mhi->ctrl_base.host_pa  = HOST_ADDR(mhi->host_addr.ctrl_base_lsb,
						mhi->host_addr.ctrl_base_msb);
	mhi->data_base.host_pa  = HOST_ADDR(mhi->host_addr.data_base_lsb,
						mhi->host_addr.data_base_msb);

	addr1 = HOST_ADDR(mhi->host_addr.ctrl_limit_lsb,
					mhi->host_addr.ctrl_limit_msb);
	mhi->ctrl_base.size = addr1 - mhi->ctrl_base.host_pa;
	addr1 = HOST_ADDR(mhi->host_addr.data_limit_lsb,
					mhi->host_addr.data_limit_msb);
	mhi->data_base.size = addr1 - mhi->data_base.host_pa;

	if (mhi->config_iatu) {
		if (mhi->ctrl_base.host_pa > mhi->data_base.host_pa) {
			mhi->data_base.device_pa = mhi->device_local_pa_base;
			mhi->ctrl_base.device_pa = mhi->device_local_pa_base +
				mhi->ctrl_base.host_pa - mhi->data_base.host_pa;
		} else {
			mhi->ctrl_base.device_pa = mhi->device_local_pa_base;
			mhi->data_base.device_pa = mhi->device_local_pa_base +
				mhi->data_base.host_pa - mhi->ctrl_base.host_pa;
		}

		if (!mhi->use_ipa || !mhi->use_edma) {
			mhi->ctrl_base.device_va =
				(uintptr_t) devm_ioremap_nocache(&pdev->dev,
				mhi->ctrl_base.device_pa,
				mhi->ctrl_base.size);
			if (!mhi->ctrl_base.device_va) {
				pr_err("io remap failed for mhi address\n");
				return -EINVAL;
			}
		}
	}

	if (mhi->config_iatu) {
		rc = mhi_dev_config_outbound_iatu(mhi);
		if (rc) {
			pr_err("Configuring iATU failed\n");
			return rc;
		}
	}

	/* Get Channel, event and command context base pointer */
	rc = mhi_dev_mmio_get_chc_base(mhi);
	if (rc) {
		pr_err("Fetching channel context failed\n");
		return rc;
	}

	rc = mhi_dev_mmio_get_erc_base(mhi);
	if (rc) {
		pr_err("Fetching event ring context failed\n");
		return rc;
	}

	rc = mhi_dev_mmio_get_crc_base(mhi);
	if (rc) {
		pr_err("Fetching command ring context failed\n");
		return rc;
	}

	rc = mhi_dev_update_ner(mhi);
	if (rc) {
		pr_err("Fetching NER failed\n");
		return rc;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"Number of Event rings : %d, HW Event rings : %d\n",
			mhi->cfg.event_rings, mhi->cfg.hw_event_rings);

	mhi->cmd_ctx_shadow.size = sizeof(struct mhi_dev_cmd_ctx);
	mhi->ev_ctx_shadow.size = sizeof(struct mhi_dev_ev_ctx) *
					mhi->cfg.event_rings;
	mhi->ch_ctx_shadow.size = sizeof(struct mhi_dev_ch_ctx) *
					mhi->cfg.channels;

	mhi->cmd_ctx_cache = dma_alloc_coherent(&pdev->dev,
				sizeof(struct mhi_dev_cmd_ctx),
				&mhi->cmd_ctx_cache_dma_handle,
				GFP_KERNEL);
	if (!mhi->cmd_ctx_cache) {
		pr_err("no memory while allocating cmd ctx\n");
		return -ENOMEM;
	}
	memset(mhi->cmd_ctx_cache, 0, sizeof(struct mhi_dev_cmd_ctx));

	mhi->ev_ctx_cache = dma_alloc_coherent(&pdev->dev,
				sizeof(struct mhi_dev_ev_ctx) *
				mhi->cfg.event_rings,
				&mhi->ev_ctx_cache_dma_handle,
				GFP_KERNEL);
	if (!mhi->ev_ctx_cache)
		return -ENOMEM;
	memset(mhi->ev_ctx_cache, 0, sizeof(struct mhi_dev_ev_ctx) *
						mhi->cfg.event_rings);

	mhi->ch_ctx_cache = dma_alloc_coherent(&pdev->dev,
				sizeof(struct mhi_dev_ch_ctx) *
				mhi->cfg.channels,
				&mhi->ch_ctx_cache_dma_handle,
				GFP_KERNEL);
	if (!mhi->ch_ctx_cache)
		return -ENOMEM;
	memset(mhi->ch_ctx_cache, 0, sizeof(struct mhi_dev_ch_ctx) *
						mhi->cfg.channels);

	if (MHI_USE_DMA(mhi)) {
		data_transfer.phy_addr = mhi->cmd_ctx_cache_dma_handle;
		data_transfer.host_pa = mhi->cmd_ctx_shadow.host_pa;
	}

	data_transfer.size = mhi->cmd_ctx_shadow.size;

	/* Cache the command and event context */
	mhi_ctx->read_from_host(mhi, &data_transfer);

	if (MHI_USE_DMA(mhi)) {
		data_transfer.phy_addr = mhi->ev_ctx_cache_dma_handle;
		data_transfer.host_pa = mhi->ev_ctx_shadow.host_pa;
	}

	data_transfer.size = mhi->ev_ctx_shadow.size;

	mhi_ctx->read_from_host(mhi, &data_transfer);

	mhi_log(MHI_MSG_VERBOSE,
			"cmd ring_base:0x%llx, rp:0x%llx, wp:0x%llx\n",
					mhi->cmd_ctx_cache->rbase,
					mhi->cmd_ctx_cache->rp,
					mhi->cmd_ctx_cache->wp);
	mhi_log(MHI_MSG_VERBOSE,
			"ev ring_base:0x%llx, rp:0x%llx, wp:0x%llx\n",
					mhi_ctx->ev_ctx_cache->rbase,
					mhi->ev_ctx_cache->rp,
					mhi->ev_ctx_cache->wp);

	return mhi_ring_start(&mhi->ring[0],
			(union mhi_dev_ring_ctx *)mhi->cmd_ctx_cache, mhi);
}

void mhi_dev_pm_relax(void)
{
	atomic_set(&mhi_ctx->mhi_dev_wake, 0);
	pm_relax(mhi_ctx->dev);
	mhi_log(MHI_MSG_VERBOSE, "releasing mhi wakelock\n");
}
EXPORT_SYMBOL(mhi_dev_pm_relax);

int mhi_dev_suspend(struct mhi_dev *mhi)
{
	int ch_id = 0, rc = 0;
	struct mhi_addr data_transfer;

	mutex_lock(&mhi_ctx->mhi_write_test);
	atomic_set(&mhi->is_suspended, 1);

	for (ch_id = 0; ch_id < mhi->cfg.channels; ch_id++) {
		if (mhi->ch_ctx_cache[ch_id].ch_state !=
						MHI_DEV_CH_STATE_RUNNING)
			continue;

		mhi->ch_ctx_cache[ch_id].ch_state = MHI_DEV_CH_STATE_SUSPENDED;

		if (MHI_USE_DMA(mhi)) {
			data_transfer.host_pa = mhi->ch_ctx_shadow.host_pa +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
		} else {
			data_transfer.device_va = mhi->ch_ctx_shadow.device_va +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
			data_transfer.device_pa = mhi->ch_ctx_shadow.device_pa +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
		}

		data_transfer.size = sizeof(enum mhi_dev_ch_ctx_state);
		data_transfer.virt_addr = &mhi->ch_ctx_cache[ch_id].ch_state;

		/* update the channel state in the host */
		mhi_ctx->write_to_host(mhi, &data_transfer, NULL,
				MHI_DEV_DMA_SYNC);

	}

	mutex_unlock(&mhi_ctx->mhi_write_test);

	return rc;
}
EXPORT_SYMBOL(mhi_dev_suspend);

int mhi_dev_resume(struct mhi_dev *mhi)
{
	int ch_id = 0, rc = 0;
	struct mhi_addr data_transfer;

	for (ch_id = 0; ch_id < mhi->cfg.channels; ch_id++) {
		if (mhi->ch_ctx_cache[ch_id].ch_state !=
				MHI_DEV_CH_STATE_SUSPENDED)
			continue;

		mhi->ch_ctx_cache[ch_id].ch_state = MHI_DEV_CH_STATE_RUNNING;
		if (MHI_USE_DMA(mhi)) {
			data_transfer.host_pa = mhi->ch_ctx_shadow.host_pa +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
		} else {
			data_transfer.device_va = mhi->ch_ctx_shadow.device_va +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
			data_transfer.device_pa = mhi->ch_ctx_shadow.device_pa +
				sizeof(struct mhi_dev_ch_ctx) * ch_id;
		}

		data_transfer.size = sizeof(enum mhi_dev_ch_ctx_state);
		data_transfer.virt_addr = &mhi->ch_ctx_cache[ch_id].ch_state;

		/* update the channel state in the host */
		mhi_ctx->write_to_host(mhi, &data_transfer, NULL,
				MHI_DEV_DMA_SYNC);
	}
	mhi_update_state_info(MHI_STATE_CONNECTED);

	atomic_set(&mhi->is_suspended, 0);

	return rc;
}
EXPORT_SYMBOL(mhi_dev_resume);

static int mhi_dev_ring_init(struct mhi_dev *dev)
{
	int i = 0;

	mhi_log(MHI_MSG_INFO, "initializing all rings");
	dev->cmd_ring_idx = 0;
	dev->ev_ring_start = 1;
	dev->ch_ring_start = dev->ev_ring_start + dev->cfg.event_rings;

	/* Initialize CMD ring */
	mhi_ring_init(&dev->ring[dev->cmd_ring_idx],
				RING_TYPE_CMD, dev->cmd_ring_idx);

	mhi_ring_set_cb(&dev->ring[dev->cmd_ring_idx],
				mhi_dev_process_cmd_ring);

	/* Initialize Event ring */
	for (i = dev->ev_ring_start; i < (dev->cfg.event_rings
					+ dev->ev_ring_start); i++)
		mhi_ring_init(&dev->ring[i], RING_TYPE_ER, i);

	/* Initialize CH */
	for (i = dev->ch_ring_start; i < (dev->cfg.channels
					+ dev->ch_ring_start); i++) {
		mhi_ring_init(&dev->ring[i], RING_TYPE_CH, i);
		mhi_ring_set_cb(&dev->ring[i], mhi_dev_process_tre_ring);
	}

	return 0;
}

static uint32_t mhi_dev_get_evt_ring_size(struct mhi_dev *mhi, uint32_t ch_id)
{
	uint32_t info;
	int rc;

	/* If channel was started by host, get event ring size */
	rc = mhi_ctrl_state_info(ch_id, &info);
	if (rc || (info != MHI_STATE_CONNECTED))
		return NUM_TR_EVENTS_DEFAULT;

	return mhi->ring[mhi->ev_ring_start +
		mhi->ch_ctx_cache[ch_id].err_indx].ring_size;
}

static int mhi_dev_alloc_evt_buf_evt_req(struct mhi_dev *mhi,
		struct mhi_dev_channel *ch, struct mhi_dev_ring *evt_ring)
{
	int rc;
	uint32_t size, i;

	if (evt_ring)
		size = evt_ring->ring_size;
	else
		size = mhi_dev_get_evt_ring_size(mhi, ch->ch_id);

	if (!size) {
		mhi_log(MHI_MSG_ERROR,
			"Evt buf size is 0 for channel %d", ch->ch_id);
		return -EINVAL;
	}

	/* Previous allocated evt buf size matches requested size */
	if (size == ch->evt_buf_size)
		return 0;

	/*
	 * Either evt buf and evt reqs were not allocated yet or
	 * they were allocated with a different size
	 */
	if (ch->evt_buf_size) {
		kfree(ch->ereqs);
		kfree(ch->tr_events);
	}
	/*
	 * Set number of event flush req buffers equal to size of
	 * event buf since in the worst case we may need to flush
	 * every event ring element individually
	 */
	ch->evt_buf_size = size;
	ch->evt_req_size = size;

	mhi_log(MHI_MSG_INFO,
		"Channel %d evt buf size is %d\n", ch->ch_id, ch->evt_buf_size);

	/* Allocate event requests */
	ch->ereqs = kcalloc(ch->evt_req_size, sizeof(*ch->ereqs), GFP_KERNEL);
	if (!ch->ereqs)
		return -ENOMEM;

	/* Allocate buffers to queue transfer completion events */
	ch->tr_events = kcalloc(ch->evt_buf_size, sizeof(*ch->tr_events),
			GFP_KERNEL);
	if (!ch->tr_events) {
		rc = -ENOMEM;
		goto free_ereqs;
	}

	/* Organize event flush requests into a linked list */
	INIT_LIST_HEAD(&ch->event_req_buffers);
	INIT_LIST_HEAD(&ch->flush_event_req_buffers);
	for (i = 0; i < ch->evt_req_size; ++i)
		list_add_tail(&ch->ereqs[i].list, &ch->event_req_buffers);

	ch->curr_ereq =
		container_of(ch->event_req_buffers.next,
					struct event_req, list);
	list_del_init(&ch->curr_ereq->list);
	ch->curr_ereq->start = 0;

	/*
	 * Initialize cmpl event buffer indexes - evt_buf_rp and
	 * evt_buf_wp point to the first and last free index available.
	 */
	ch->evt_buf_rp = 0;
	ch->evt_buf_wp = ch->evt_buf_size - 1;

	return 0;

free_ereqs:
	kfree(ch->ereqs);
	ch->ereqs = NULL;
	ch->evt_buf_size = 0;
	ch->evt_req_size = 0;

	return rc;
}

int mhi_dev_open_channel(uint32_t chan_id,
			struct mhi_dev_client **handle_client,
			void (*mhi_dev_client_cb_reason)
			(struct mhi_dev_client_cb_reason *cb))
{
	int rc = 0;
	struct mhi_dev_channel *ch;
	struct platform_device *pdev;

	pdev = mhi_ctx->pdev;
	ch = &mhi_ctx->ch[chan_id];

	mutex_lock(&ch->ch_lock);

	if (ch->active_client) {
		mhi_log(MHI_MSG_ERROR,
			"Channel (%d) already opened by client\n", chan_id);
		rc = -EINVAL;
		goto exit;
	}

	/* Initialize the channel, client and state information */
	*handle_client = kzalloc(sizeof(struct mhi_dev_client), GFP_KERNEL);
	if (!(*handle_client)) {
		dev_err(&pdev->dev, "can not allocate mhi_dev memory\n");
		rc = -ENOMEM;
		goto exit;
	}

	rc = mhi_dev_alloc_evt_buf_evt_req(mhi_ctx, ch, NULL);
	if (rc)
		goto free_client;

	ch->active_client = (*handle_client);
	(*handle_client)->channel = ch;
	(*handle_client)->event_trigger = mhi_dev_client_cb_reason;

	if (ch->state == MHI_DEV_CH_UNINT) {
		ch->ring = &mhi_ctx->ring[chan_id + mhi_ctx->ch_ring_start];
		ch->state = MHI_DEV_CH_PENDING_START;
	} else if (ch->state == MHI_DEV_CH_CLOSED)
		ch->state = MHI_DEV_CH_STARTED;
	else if (ch->state == MHI_DEV_CH_STOPPED)
		ch->state = MHI_DEV_CH_PENDING_START;

	goto exit;

free_client:
	kfree(*handle_client);
	*handle_client = NULL;

exit:
	mutex_unlock(&ch->ch_lock);
	return rc;
}
EXPORT_SYMBOL(mhi_dev_open_channel);

int mhi_dev_channel_isempty(struct mhi_dev_client *handle)
{
	struct mhi_dev_channel *ch;
	int rc;

	if (!handle) {
		mhi_log(MHI_MSG_ERROR, "Invalid channel access\n");
		return -EINVAL;
	}

	ch = handle->channel;
	if (!ch)
		return -EINVAL;

	rc = ch->ring->rd_offset == ch->ring->wr_offset;

	return rc;
}
EXPORT_SYMBOL(mhi_dev_channel_isempty);

bool mhi_dev_channel_has_pending_write(struct mhi_dev_client *handle)
{
	struct mhi_dev_channel *ch;

	if (!handle) {
		mhi_log(MHI_MSG_ERROR, "Invalid channel access\n");
		return -EINVAL;
	}

	ch = handle->channel;
	if (!ch)
		return -EINVAL;

	return ch->pend_wr_count ? true : false;
}
EXPORT_SYMBOL(mhi_dev_channel_has_pending_write);

void mhi_dev_close_channel(struct mhi_dev_client *handle)
{
	struct mhi_dev_channel *ch;
	int count = 0;

	if (!handle) {
		mhi_log(MHI_MSG_ERROR, "Invalid channel access:%d\n", -ENODEV);
		return;
	}
	ch = handle->channel;

	do {
		if (ch->pend_wr_count) {
			usleep_range(MHI_DEV_CH_CLOSE_TIMEOUT_MIN,
					MHI_DEV_CH_CLOSE_TIMEOUT_MAX);
		} else
			break;
	} while (++count < MHI_DEV_CH_CLOSE_TIMEOUT_COUNT);

	mutex_lock(&ch->ch_lock);

	if (ch->pend_wr_count)
		mhi_log(MHI_MSG_ERROR, "%d writes pending for channel %d\n",
			ch->pend_wr_count, ch->ch_id);

	if (ch->state != MHI_DEV_CH_PENDING_START)
		if ((ch->ch_type == MHI_DEV_CH_TYPE_OUTBOUND_CHANNEL &&
			!mhi_dev_channel_isempty(handle)) || ch->tre_loc)
			mhi_log(MHI_MSG_DBG,
				"Trying to close an active channel (%d)\n",
				ch->ch_id);

	ch->state = MHI_DEV_CH_CLOSED;
	ch->active_client = NULL;
	kfree(ch->ereqs);
	kfree(ch->tr_events);
	ch->evt_buf_size = 0;
	ch->evt_req_size = 0;
	ch->ereqs = NULL;
	ch->tr_events = NULL;
	kfree(handle);

	mutex_unlock(&ch->ch_lock);
	return;
}
EXPORT_SYMBOL(mhi_dev_close_channel);

static int mhi_dev_check_tre_bytes_left(struct mhi_dev_channel *ch,
		struct mhi_dev_ring *ring, union mhi_dev_ring_element_type *el,
		struct mhi_req *mreq)
{
	uint32_t td_done = 0;

	/*
	 * A full TRE worth of data was consumed.
	 * Check if we are at a TD boundary.
	 */
	if (ch->tre_bytes_left == 0) {
		if (el->tre.chain) {
			if (el->tre.ieob)
				mhi_dev_send_completion_event_async(ch,
				ring->rd_offset, el->tre.len,
				MHI_CMD_COMPL_CODE_EOB, mreq);
			mreq->chain = 1;
		} else {
			if (el->tre.ieot)
				mhi_dev_send_completion_event_async(
				ch, ring->rd_offset, el->tre.len,
				MHI_CMD_COMPL_CODE_EOT, mreq);
			td_done = 1;
			mreq->chain = 0;
		}
		mhi_dev_ring_inc_index(ring, ring->rd_offset);
		ch->tre_bytes_left = 0;
		ch->tre_loc = 0;
	}

	return td_done;
}

int mhi_dev_read_channel(struct mhi_req *mreq)
{
	struct mhi_dev_channel *ch;
	struct mhi_dev_ring *ring;
	union mhi_dev_ring_element_type *el;
	size_t bytes_to_read, addr_offset;
	uint64_t read_from_loc;
	ssize_t bytes_read = 0;
	size_t write_to_loc = 0;
	uint32_t usr_buf_remaining;
	int td_done = 0, rc = 0;
	struct mhi_dev_client *handle_client;

	if (WARN_ON(!mreq))
		return -ENXIO;

	if (mhi_ctx->ctrl_info != MHI_STATE_CONNECTED) {
		pr_err("Channel not connected:%d\n", mhi_ctx->ctrl_info);
		return -ENODEV;
	}

	if (!mreq->client) {
		mhi_log(MHI_MSG_ERROR, "invalid mhi request\n");
		return -ENXIO;
	}
	handle_client = mreq->client;
	ch = handle_client->channel;
	usr_buf_remaining = mreq->len;
	ring = ch->ring;
	mreq->chain = 0;

	mutex_lock(&ch->ch_lock);

	do {
		el = &ring->ring_cache[ring->rd_offset];
		mhi_log(MHI_MSG_VERBOSE, "evtptr : 0x%llx\n",
						el->tre.data_buf_ptr);
		mhi_log(MHI_MSG_VERBOSE, "evntlen : 0x%x, offset:%lu\n",
						el->tre.len, ring->rd_offset);

		if (ch->tre_loc) {
			bytes_to_read = min(usr_buf_remaining,
						ch->tre_bytes_left);
			mreq->chain = 1;
			mhi_log(MHI_MSG_VERBOSE,
				"remaining buffered data size %d\n",
				(int) ch->tre_bytes_left);
		} else {
			if (ring->rd_offset == ring->wr_offset) {
				mhi_log(MHI_MSG_VERBOSE,
					"nothing to read, returning\n");
				bytes_read = 0;
				goto exit;
			}

			if (ch->state == MHI_DEV_CH_STOPPED) {
				mhi_log(MHI_MSG_VERBOSE,
					"channel (%d) already stopped\n",
					mreq->chan);
				bytes_read = -1;
				goto exit;
			}

			ch->tre_loc = el->tre.data_buf_ptr;
			ch->tre_size = el->tre.len;
			ch->tre_bytes_left = ch->tre_size;

			mhi_log(MHI_MSG_VERBOSE,
			"user_buf_remaining %d, ch->tre_size %d\n",
			usr_buf_remaining, ch->tre_size);
			bytes_to_read = min(usr_buf_remaining, ch->tre_size);
		}

		bytes_read += bytes_to_read;
		addr_offset = ch->tre_size - ch->tre_bytes_left;
		read_from_loc = ch->tre_loc + addr_offset;
		write_to_loc = (size_t) mreq->buf +
			(mreq->len - usr_buf_remaining);
		ch->tre_bytes_left -= bytes_to_read;
		mreq->el = el;
		mreq->transfer_len = bytes_to_read;
		mreq->rd_offset = ring->rd_offset;
		mhi_log(MHI_MSG_VERBOSE, "reading %lu bytes from chan %d\n",
				bytes_to_read, mreq->chan);
		rc = mhi_ctx->host_to_device((void *) write_to_loc,
				read_from_loc, bytes_to_read, mhi_ctx, mreq);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
					"Error while reading chan (%d) rc %d\n",
					mreq->chan, rc);
			mutex_unlock(&ch->ch_lock);
			return rc;
		}
		usr_buf_remaining -= bytes_to_read;

		if (mreq->mode == DMA_ASYNC) {
			ch->tre_bytes_left = 0;
			ch->tre_loc = 0;
			goto exit;
		} else {
			td_done = mhi_dev_check_tre_bytes_left(ch, ring,
					el, mreq);
		}
	} while (usr_buf_remaining  && !td_done);
	if (td_done && ch->state == MHI_DEV_CH_PENDING_STOP) {
		ch->state = MHI_DEV_CH_STOPPED;
		rc = mhi_dev_process_stop_cmd(ring, mreq->chan, mhi_ctx);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
					"Error while stopping channel (%d)\n",
					mreq->chan);
			bytes_read = -EIO;
		}
	}
exit:
	mutex_unlock(&ch->ch_lock);
	return bytes_read;
}
EXPORT_SYMBOL(mhi_dev_read_channel);

static void skip_to_next_td(struct mhi_dev_channel *ch)
{
	struct mhi_dev_ring *ring = ch->ring;
	union mhi_dev_ring_element_type *el;
	uint32_t td_boundary_reached = 0;

	ch->skip_td = 1;
	el = &ring->ring_cache[ring->rd_offset];
	while (ring->rd_offset != ring->wr_offset) {
		if (td_boundary_reached) {
			ch->skip_td = 0;
			break;
		}
		if (!el->tre.chain)
			td_boundary_reached = 1;
		mhi_dev_ring_inc_index(ring, ring->rd_offset);
		el = &ring->ring_cache[ring->rd_offset];
	}
}

int mhi_dev_write_channel(struct mhi_req *wreq)
{
	struct mhi_dev_channel *ch;
	struct mhi_dev_ring *ring;
	struct mhi_dev_client *handle_client;
	union mhi_dev_ring_element_type *el;
	enum mhi_dev_cmd_completion_code code = MHI_CMD_COMPL_CODE_INVALID;
	int rc = 0;
	uint64_t skip_tres = 0, write_to_loc;
	size_t read_from_loc;
	uint32_t usr_buf_remaining;
	size_t usr_buf_offset = 0;
	size_t bytes_to_write = 0;
	size_t bytes_written = 0;
	uint32_t tre_len = 0, suspend_wait_timeout = 0;
	bool async_wr_sched = false;
	enum mhi_ctrl_info info;

	if (WARN_ON(!wreq || !wreq->client || !wreq->buf)) {
		pr_err("%s: invalid parameters\n", __func__);
		return -ENXIO;
	}

	if (mhi_ctx->ctrl_info != MHI_STATE_CONNECTED) {
		pr_err("Channel not connected:%d\n", mhi_ctx->ctrl_info);
		return -ENODEV;
	}

	usr_buf_remaining =  wreq->len;
	mutex_lock(&mhi_ctx->mhi_write_test);

	if (atomic_read(&mhi_ctx->is_suspended)) {
		/*
		 * Expected usage is when there is a write
		 * to the MHI core -> notify SM.
		 */
		rc = mhi_dev_notify_sm_event(MHI_DEV_EVENT_CORE_WAKEUP);
		if (rc) {
			pr_err("error sending core wakeup event\n");
			mutex_unlock(&mhi_ctx->mhi_write_test);
			return rc;
		}
	}

	while (atomic_read(&mhi_ctx->is_suspended) &&
			suspend_wait_timeout < MHI_WAKEUP_TIMEOUT_CNT) {
		/* wait for the suspend to finish */
		msleep(MHI_SUSPEND_MIN);
		suspend_wait_timeout++;
	}

	if (suspend_wait_timeout >= MHI_WAKEUP_TIMEOUT_CNT ||
				mhi_ctx->ctrl_info != MHI_STATE_CONNECTED) {
		pr_err("Failed to wake up core\n");
		mutex_unlock(&mhi_ctx->mhi_write_test);
		return -ENODEV;
	}

	handle_client = wreq->client;
	ch = handle_client->channel;

	ring = ch->ring;

	mutex_lock(&ch->ch_lock);

	rc = mhi_ctrl_state_info(ch->ch_id, &info);
	if (rc || (info != MHI_STATE_CONNECTED)) {
		mhi_log(MHI_MSG_ERROR, "Channel %d not started by host\n",
				ch->ch_id);
		mutex_unlock(&ch->ch_lock);
		return -ENODEV;
	}

	ch->pend_wr_count++;
	if (ch->state == MHI_DEV_CH_STOPPED) {
		mhi_log(MHI_MSG_ERROR,
			"channel %d already stopped\n", wreq->chan);
		bytes_written = -1;
		goto exit;
	}

	if (ch->state == MHI_DEV_CH_PENDING_STOP) {
		if (mhi_dev_process_stop_cmd(ring, wreq->chan, mhi_ctx) < 0)
			bytes_written = -1;
		goto exit;
	}

	if (ch->skip_td)
		skip_to_next_td(ch);

	do {
		if (ring->rd_offset == ring->wr_offset) {
			mhi_log(MHI_MSG_ERROR,
					"%s():rd & wr offsets are equal\n",
					__func__);
			mhi_log(MHI_MSG_INFO, "No TREs available\n");
			break;
		}

		el = &ring->ring_cache[ring->rd_offset];
		tre_len = el->tre.len;
		if (wreq->len > tre_len) {
			pr_err("%s(): rlen = %lu, tlen = %d: client buf > tre len\n",
					__func__, wreq->len, tre_len);
			bytes_written = -ENOMEM;
			goto exit;
		}

		bytes_to_write = min(usr_buf_remaining, tre_len);
		usr_buf_offset = wreq->len - bytes_to_write;
		read_from_loc = (size_t) wreq->buf + usr_buf_offset;
		write_to_loc = el->tre.data_buf_ptr;
		wreq->rd_offset = ring->rd_offset;
		wreq->el = el;
		wreq->transfer_len = bytes_to_write;
		rc = mhi_ctx->device_to_host(write_to_loc,
						(void *) read_from_loc,
						bytes_to_write,
						mhi_ctx, wreq);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
					"Error while writing chan (%d) rc %d\n",
					wreq->chan, rc);
			goto exit;
		} else if (wreq->mode == DMA_ASYNC)
			async_wr_sched = true;
		bytes_written += bytes_to_write;
		usr_buf_remaining -= bytes_to_write;

		if (usr_buf_remaining) {
			if (!el->tre.chain)
				code = MHI_CMD_COMPL_CODE_OVERFLOW;
			else if (el->tre.ieob)
				code = MHI_CMD_COMPL_CODE_EOB;
		} else {
			if (el->tre.chain)
				skip_tres = 1;
			code = MHI_CMD_COMPL_CODE_EOT;
		}
		if (wreq->mode == DMA_SYNC) {
			rc = mhi_dev_send_completion_event(ch,
					ring->rd_offset, bytes_to_write, code);
			if (rc)
				mhi_log(MHI_MSG_VERBOSE,
						"err in snding cmpl evt ch:%d\n",
						wreq->chan);
			 mhi_dev_ring_inc_index(ring, ring->rd_offset);
		}

		if (ch->state == MHI_DEV_CH_PENDING_STOP)
			break;

	} while (!skip_tres && usr_buf_remaining);

	if (skip_tres)
		skip_to_next_td(ch);

	if (ch->state == MHI_DEV_CH_PENDING_STOP) {
		rc = mhi_dev_process_stop_cmd(ring, wreq->chan, mhi_ctx);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"channel %d stop failed\n", wreq->chan);
		}
	}
exit:
	if (wreq->mode == DMA_SYNC || !async_wr_sched)
		ch->pend_wr_count--;
	mutex_unlock(&ch->ch_lock);
	mutex_unlock(&mhi_ctx->mhi_write_test);
	return bytes_written;
}
EXPORT_SYMBOL(mhi_dev_write_channel);

static int mhi_dev_recover(struct mhi_dev *mhi)
{
	int rc = 0;
	uint32_t syserr, max_cnt = 0, bhi_intvec = 0, bhi_max_cnt = 0;
	u32 mhi_reset;
	enum mhi_dev_state state;

	/* Check if MHI is in syserr */
	mhi_dev_mmio_masked_read(mhi, MHISTATUS,
				MHISTATUS_SYSERR_MASK,
				MHISTATUS_SYSERR_SHIFT, &syserr);

	mhi_log(MHI_MSG_VERBOSE, "mhi_syserr = 0x%X\n", syserr);
	if (syserr) {
		/* Poll for the host to set the reset bit */
		rc = mhi_dev_mmio_get_mhi_state(mhi, &state, &mhi_reset);
		if (rc) {
			pr_err("%s: get mhi state failed\n", __func__);
			return rc;
		}

		mhi_log(MHI_MSG_VERBOSE, "mhi_state = 0x%X, reset = %d\n",
				state, mhi_reset);

		rc = mhi_dev_mmio_read(mhi, BHI_INTVEC, &bhi_intvec);
		if (rc)
			return rc;

		while (bhi_intvec == 0xffffffff &&
				bhi_max_cnt < MHI_BHI_INTVEC_MAX_CNT) {
			/* Wait for Host to set the bhi_intvec */
			msleep(MHI_BHI_INTVEC_WAIT_MS);
			mhi_log(MHI_MSG_VERBOSE,
					"Wait for Host to set BHI_INTVEC\n");
			rc = mhi_dev_mmio_read(mhi, BHI_INTVEC, &bhi_intvec);
			if (rc) {
				pr_err("%s: Get BHI_INTVEC failed\n", __func__);
				return rc;
			}
			bhi_max_cnt++;
		}

		if (bhi_max_cnt == MHI_BHI_INTVEC_MAX_CNT) {
			mhi_log(MHI_MSG_ERROR,
					"Host failed to set BHI_INTVEC\n");
			return -EINVAL;
		}

		if (bhi_intvec != 0xffffffff) {
			/* Indicate the host that the device is ready */
			rc = ep_pcie_trigger_msi(mhi->phandle, bhi_intvec);
			if (rc) {
				pr_err("%s: error sending msi\n", __func__);
				return rc;
			}
		}

		/* Poll for the host to set the reset bit */
		rc = mhi_dev_mmio_get_mhi_state(mhi, &state, &mhi_reset);
		if (rc) {
			pr_err("%s: get mhi state failed\n", __func__);
			return rc;
		}

		mhi_log(MHI_MSG_VERBOSE, "mhi_state = 0x%X, reset = %d\n",
				state, mhi_reset);

		while (mhi_reset != 0x1 && max_cnt < MHI_SUSPEND_TIMEOUT) {
			/* Wait for Host to set the reset */
			msleep(MHI_SUSPEND_MIN);
			rc = mhi_dev_mmio_get_mhi_state(mhi, &state,
								&mhi_reset);
			if (rc) {
				pr_err("%s: get mhi state failed\n", __func__);
				return rc;
			}
			max_cnt++;
		}

		if (!mhi_reset) {
			mhi_log(MHI_MSG_VERBOSE, "Host failed to set reset\n");
			return -EINVAL;
		}
	}
	/*
	 * Now mask the interrupts so that the state machine moves
	 * only after IPA is ready
	 */
	mhi_dev_mmio_mask_interrupts(mhi);
	return 0;
}

static void mhi_dev_enable(struct work_struct *work)
{
	int rc = 0;
	struct ep_pcie_msi_config msi_cfg;
	struct mhi_dev *mhi = container_of(work,
				struct mhi_dev, ring_init_cb_work);
	u32 mhi_reset;
	enum mhi_dev_state state;
	uint32_t max_cnt = 0, bhi_intvec = 0;

	if (mhi->use_ipa) {
		rc = ipa_dma_init();
		if (rc) {
			pr_err("ipa dma init failed\n");
			return;
		}

		rc = ipa_dma_enable();
		if (rc) {
			pr_err("ipa enable failed\n");
			return;
		}
	}

	rc = mhi_dev_ring_init(mhi);
	if (rc) {
		pr_err("MHI dev ring init failed\n");
		return;
	}

	rc = mhi_dev_mmio_read(mhi, BHI_INTVEC, &bhi_intvec);
	if (rc)
		return;

	if (bhi_intvec != 0xffffffff) {
		/* Indicate the host that the device is ready */
		rc = ep_pcie_get_msi_config(mhi->phandle, &msi_cfg);
		if (!rc) {
			rc = ep_pcie_trigger_msi(mhi_ctx->phandle, bhi_intvec);
			if (rc) {
				pr_err("%s: error sending msi\n", __func__);
				return;
			}
		} else {
			pr_err("MHI: error geting msi configs\n");
		}
	}

	rc = mhi_dev_mmio_get_mhi_state(mhi, &state, &mhi_reset);
	if (rc) {
		pr_err("%s: get mhi state failed\n", __func__);
		return;
	}

	while (state != MHI_DEV_M0_STATE && max_cnt < MHI_SUSPEND_TIMEOUT) {
		/* Wait for Host to set the M0 state */
		msleep(MHI_SUSPEND_MIN);
		rc = mhi_dev_mmio_get_mhi_state(mhi, &state, &mhi_reset);
		if (rc) {
			pr_err("%s: get mhi state failed\n", __func__);
			return;
		}
		max_cnt++;
	}

	mhi_log(MHI_MSG_INFO, "state:%d\n", state);

	if (state == MHI_DEV_M0_STATE) {
		rc = mhi_dev_cache_host_cfg(mhi);
		if (rc) {
			pr_err("Failed to cache the host config\n");
			return;
		}

		rc = mhi_dev_mmio_set_env(mhi, MHI_ENV_VALUE);
		if (rc) {
			pr_err("%s: env setting failed\n", __func__);
			return;
		}
	} else {
		pr_err("MHI device failed to enter M0\n");
		return;
	}

	rc = mhi_hwc_init(mhi_ctx);
	if (rc) {
		pr_err("error during hwc_init\n");
		return;
	}

	if (mhi_ctx->config_iatu || mhi_ctx->mhi_int) {
		mhi_ctx->mhi_int_en = true;
		enable_irq(mhi_ctx->mhi_irq);
	}

	/*
	 * ctrl_info might already be set to CONNECTED state in the
	 * callback function mhi_hwc_cb triggered from IPA when mhi_hwc_init
	 * is called above, so set to CONFIGURED state only when it
	 * is not already set to CONNECTED
	 */
	if (mhi_ctx->ctrl_info != MHI_STATE_CONNECTED)
		mhi_update_state_info(MHI_STATE_CONFIGURED);

	/*Enable MHI dev network stack Interface*/
	rc = mhi_dev_net_interface_init();
	if (rc)
		pr_err("%s Failed to initialize mhi_dev_net iface\n", __func__);
}

static void mhi_ring_init_cb(void *data)
{
	struct mhi_dev *mhi = data;

	if (WARN_ON(!mhi))
		return;

	queue_work(mhi->ring_init_wq, &mhi->ring_init_cb_work);
}

int mhi_register_state_cb(void (*mhi_state_cb)
				(struct mhi_dev_client_cb_data *cb_data),
				void *data, enum mhi_client_channel channel)
{
	struct mhi_dev_ready_cb_info *cb_info = NULL;

	if (WARN_ON(!mhi_ctx))
		return -ENXIO;

	if (channel >= MHI_MAX_SOFTWARE_CHANNELS) {
		pr_err("Invalid channel :%d\n", channel);
		return -EINVAL;
	}

	mutex_lock(&mhi_ctx->mhi_lock);
	cb_info = kmalloc(sizeof(*cb_info), GFP_KERNEL);
	if (!cb_info) {
		mutex_unlock(&mhi_ctx->mhi_lock);
		return -ENOMEM;
	}

	cb_info->cb = mhi_state_cb;
	cb_info->cb_data.user_data = data;
	cb_info->cb_data.channel = channel;

	list_add_tail(&cb_info->list, &mhi_ctx->client_cb_list);

	/**
	 * If channel is open during registration, no callback is issued.
	 * Instead return -EEXIST to notify the client. Clients request
	 * is added to the list to notify future state change notification.
	 * Channel struct may not be allocated yet if this function is called
	 * early during boot - add an explicit check for non-null "ch".
	 */
	if (mhi_ctx->ch && (mhi_ctx->ch[channel].state == MHI_DEV_CH_STARTED)) {
		mutex_unlock(&mhi_ctx->mhi_lock);
		return -EEXIST;
	}

	mutex_unlock(&mhi_ctx->mhi_lock);

	return 0;
}
EXPORT_SYMBOL(mhi_register_state_cb);

static void mhi_update_state_info_ch(uint32_t ch_id, enum mhi_ctrl_info info)
{
	struct mhi_dev_client_cb_reason reason;

	/* Currently no clients register for HW channel notification */
	if (ch_id >= MHI_MAX_SOFTWARE_CHANNELS)
		return;

	channel_state_info[ch_id].ctrl_info = info;
	if (ch_id == MHI_CLIENT_QMI_OUT || ch_id == MHI_CLIENT_QMI_IN) {
		/* For legacy reasons for QTI client */
		reason.reason = MHI_DEV_CTRL_UPDATE;
		uci_ctrl_update(&reason);
	}
}


static void mhi_update_state_info(enum mhi_ctrl_info info)
{
	mhi_ctx->ctrl_info = info;
}

int mhi_ctrl_state_info(uint32_t idx, uint32_t *info)
{
	if (idx == MHI_DEV_UEVENT_CTRL)
		*info = mhi_ctx->ctrl_info;
	else
		if (idx < MHI_MAX_SOFTWARE_CHANNELS)
			*info = channel_state_info[idx].ctrl_info;
		else
			return -EINVAL;

	mhi_log(MHI_MSG_VERBOSE, "idx:%d, ctrl:%d", idx, *info);

	return 0;
}
EXPORT_SYMBOL(mhi_ctrl_state_info);

static int get_device_tree_data(struct platform_device *pdev)
{
	struct mhi_dev *mhi;
	int rc = 0;
	struct resource *res_mem = NULL;

	mhi = devm_kzalloc(&pdev->dev,
			sizeof(struct mhi_dev), GFP_KERNEL);
	if (!mhi)
		return -ENOMEM;

	mhi->pdev = pdev;
	mhi->dev = &pdev->dev;
	res_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "mhi_mmio_base");
	if (!res_mem) {
		pr_err("Request MHI MMIO physical memory region failed\n");
		return -EINVAL;
	}

	mhi->mmio_base_pa_addr = res_mem->start;
	mhi->mmio_base_addr = ioremap_nocache(res_mem->start, MHI_1K_SIZE);
	if (!mhi->mmio_base_addr) {
		pr_err("Failed to IO map MMIO registers\n");
		return -EINVAL;
	}

	mhi->use_ipa = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-ipa-software-channel");
	if (mhi->use_ipa) {
		res_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "ipa_uc_mbox_crdb");
		if (!res_mem) {
			pr_err("Request IPA_UC_MBOX CRDB physical region failed\n");
			rc = -EINVAL;
			goto err;
		}

		mhi->ipa_uc_mbox_crdb = res_mem->start;

		res_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "ipa_uc_mbox_erdb");
		if (!res_mem) {
			pr_err("Request IPA_UC_MBOX ERDB physical region failed\n");
			rc = -EINVAL;
			goto err;
		}

		mhi->ipa_uc_mbox_erdb = res_mem->start;
	}

	mhi_ctx = mhi;

	rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-ifc-id",
				&mhi_ctx->ifc_id);
	if (rc) {
		pr_err("qcom,mhi-ifc-id does not exist\n");
		goto err;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-ep-msi",
				&mhi_ctx->mhi_ep_msi_num);
	if (rc) {
		pr_err("qcom,mhi-ep-msi does not exist\n");
		goto err;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-version",
				&mhi_ctx->mhi_version);
	if (rc) {
		pr_err("qcom,mhi-version does not exist\n");
		goto err;
	}

	mhi->use_edma = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-pcie-edma");

	mhi_ctx->config_iatu = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,mhi-config-iatu");

	if (mhi_ctx->config_iatu) {
		rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-local-pa-base",
				&mhi_ctx->device_local_pa_base);
		if (rc) {
			pr_err("qcom,mhi-local-pa-base does not exist\n");
			goto err;
		}
	}

	mhi_ctx->mhi_int = of_property_read_bool((&pdev->dev)->of_node,
					"qcom,mhi-interrupt");

	if (mhi->config_iatu || mhi_ctx->mhi_int) {
		mhi->mhi_irq = platform_get_irq_byname(pdev, "mhi-device-inta");
		if (mhi->mhi_irq < 0) {
			pr_err("Invalid MHI device interrupt\n");
			rc = mhi->mhi_irq;
			goto err;
		}
	}

	device_init_wakeup(mhi->dev, true);
	/* MHI device will be woken up from PCIe event */
	device_set_wakeup_capable(mhi->dev, false);
	/* Hold a wakelock until completion of M0 */
	pm_stay_awake(mhi->dev);
	atomic_set(&mhi->mhi_dev_wake, 1);

	mhi->enable_m2 = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,enable-m2");

	mhi_log(MHI_MSG_VERBOSE, "acquiring wakelock\n");

	return 0;
err:
	iounmap(mhi->mmio_base_addr);
	return rc;
}

static int mhi_deinit(struct mhi_dev *mhi)
{
	int i = 0, ring_id = 0;
	struct mhi_dev_ring *ring;
	struct platform_device *pdev = mhi->pdev;

	ring_id = mhi->cfg.channels + mhi->cfg.event_rings + 1;

	for (i = 0; i < ring_id; i++) {
		ring = &mhi->ring[i];
		if (ring->state == RING_STATE_UINT)
			continue;

		dma_free_coherent(mhi->dev, ring->ring_size *
			sizeof(union mhi_dev_ring_element_type),
			ring->ring_cache,
			ring->ring_cache_dma_handle);

		if (mhi->use_edma)
			dma_free_coherent(mhi->dev, sizeof(u32),
				ring->msi_buf.buf,
				ring->msi_buf.dma_addr);
	}

	devm_kfree(&pdev->dev, mhi->mmio_backup);
	devm_kfree(&pdev->dev, mhi->ring);

	mhi_dev_sm_exit(mhi);

	mhi->mmio_initialized = false;

	return 0;
}

static int mhi_init(struct mhi_dev *mhi)
{
	int rc = 0, i = 0;
	struct platform_device *pdev = mhi->pdev;

	rc = mhi_dev_mmio_init(mhi);
	if (rc) {
		pr_err("Failed to update the MMIO init\n");
		return rc;
	}

	mhi->ring = devm_kzalloc(&pdev->dev,
			(sizeof(struct mhi_dev_ring) *
			(mhi->cfg.channels + mhi->cfg.event_rings + 1)),
			GFP_KERNEL);
	if (!mhi->ring)
		return -ENOMEM;

	/*
	 * mhi_init is also called during device reset, in
	 * which case channel mem will already be allocated.
	 */
	if (!mhi->ch) {
		mhi->ch = devm_kzalloc(&pdev->dev,
			(sizeof(struct mhi_dev_channel) *
			(mhi->cfg.channels)), GFP_KERNEL);
		if (!mhi->ch)
			return -ENOMEM;

		for (i = 0; i < mhi->cfg.channels; i++) {
			mhi->ch[i].ch_id = i;
			mutex_init(&mhi->ch[i].ch_lock);
			}
	}

	spin_lock_init(&mhi->lock);
	spin_lock_init(&mhi->msi_lock);
	mhi->mmio_backup = devm_kzalloc(&pdev->dev,
			MHI_DEV_MMIO_RANGE, GFP_KERNEL);
	if (!mhi->mmio_backup)
		return -ENOMEM;

	return 0;
}

static int mhi_dev_resume_mmio_mhi_reinit(struct mhi_dev *mhi_ctx)
{
	int rc = 0;

	mutex_lock(&mhi_ctx->mhi_lock);
	if (atomic_read(&mhi_ctx->re_init_done)) {
		mhi_log(MHI_MSG_INFO, "Re_init done, return\n");
		mutex_unlock(&mhi_ctx->mhi_lock);
		return 0;
	}

	rc = mhi_init(mhi_ctx);
	if (rc) {
		pr_err("Error initializing MHI MMIO with %d\n", rc);
		goto fail;
	}

	mhi_ctx->event_reg.events = EP_PCIE_EVENT_PM_D3_HOT |
		EP_PCIE_EVENT_PM_D3_COLD |
		EP_PCIE_EVENT_PM_D0 |
		EP_PCIE_EVENT_PM_RST_DEAST |
		EP_PCIE_EVENT_L1SUB_TIMEOUT |
		EP_PCIE_EVENT_L1SUB_TIMEOUT_EXIT |
		EP_PCIE_EVENT_LINKDOWN;
	if (!mhi_ctx->mhi_int)
		mhi_ctx->event_reg.events |= EP_PCIE_EVENT_MHI_A7;
	mhi_ctx->event_reg.user = mhi_ctx;
	mhi_ctx->event_reg.mode = EP_PCIE_TRIGGER_CALLBACK;
	mhi_ctx->event_reg.callback = mhi_dev_sm_pcie_handler;

	rc = ep_pcie_register_event(mhi_ctx->phandle, &mhi_ctx->event_reg);
	if (rc) {
		pr_err("Failed to register for events from PCIe\n");
		goto fail;
	}

	if (mhi_ctx->use_ipa) {
		rc = ipa_register_ipa_ready_cb(mhi_ring_init_cb, mhi_ctx);
		if (rc < 0) {
			if (rc == -EEXIST) {
				mhi_ring_init_cb(mhi_ctx);
			} else {
				pr_err("Error calling IPA cb with %d\n", rc);
				goto fail;
			}
		}
	}

	/* Invoke MHI SM when device is in RESET state */
	rc = mhi_dev_sm_init(mhi_ctx);
	if (rc) {
		pr_err("%s: Error during SM init\n", __func__);
		goto fail;
	}

	/* set the env before setting the ready bit */
	rc = mhi_dev_mmio_set_env(mhi_ctx, MHI_ENV_VALUE);
	if (rc) {
		pr_err("%s: env setting failed\n", __func__);
		goto fail;
	}

	/* All set, notify the host */
	rc = mhi_dev_sm_set_ready();
	if (rc) {
		pr_err("%s: unable to set ready bit\n", __func__);
		goto fail;
	}

	if (mhi_ctx->use_edma)
		mhi_ring_init_cb(mhi_ctx);

	atomic_set(&mhi_ctx->is_suspended, 0);
fail:
	atomic_set(&mhi_ctx->re_init_done, 1);
	mutex_unlock(&mhi_ctx->mhi_lock);
	return rc;
}

static void mhi_dev_reinit(struct work_struct *work)
{
	struct mhi_dev *mhi_ctx = container_of(work,
				struct mhi_dev, re_init);
	enum ep_pcie_link_status link_state;
	int rc = 0;

	link_state = ep_pcie_get_linkstatus(mhi_ctx->phandle);
	if (link_state == EP_PCIE_LINK_ENABLED) {
		/* PCIe link is up with BME set */
		rc = mhi_dev_resume_mmio_mhi_reinit(mhi_ctx);
		if (rc) {
			pr_err("Failed to register for events from PCIe\n");
			return;
		}
	}

	mhi_log(MHI_MSG_VERBOSE, "Wait for PCIe linkup\n");
}

static int mhi_dev_resume_mmio_mhi_init(struct mhi_dev *mhi_ctx)
{
	struct platform_device *pdev;
	int rc = 0;

	/*
	 * There could be multiple calls to this function if device gets
	 * multiple link-up events (bme irqs).
	 */
	if (mhi_ctx->init_done) {
		mhi_log(MHI_MSG_INFO, "mhi init already done, returning\n");
		return 0;
	}

	pdev = mhi_ctx->pdev;

	INIT_WORK(&mhi_ctx->chdb_ctrl_work, mhi_dev_scheduler);

	mhi_ctx->pending_ring_wq = alloc_workqueue("mhi_pending_wq",
							WQ_HIGHPRI, 0);
	if (!mhi_ctx->pending_ring_wq) {
		rc = -ENOMEM;
		return rc;
	}

	INIT_WORK(&mhi_ctx->pending_work, mhi_dev_process_ring_pending);

	INIT_WORK(&mhi_ctx->ring_init_cb_work, mhi_dev_enable);

	INIT_WORK(&mhi_ctx->re_init, mhi_dev_reinit);

	mhi_ctx->ring_init_wq = alloc_workqueue("mhi_ring_init_cb_wq",
							WQ_HIGHPRI, 0);
	if (!mhi_ctx->ring_init_wq) {
		rc = -ENOMEM;
		return rc;
	}

	INIT_LIST_HEAD(&mhi_ctx->event_ring_list);
	INIT_LIST_HEAD(&mhi_ctx->process_ring_list);
	mutex_init(&mhi_ctx->mhi_event_lock);
	mutex_init(&mhi_ctx->mhi_write_test);

	mhi_ctx->phandle = ep_pcie_get_phandle(mhi_ctx->ifc_id);
	if (!mhi_ctx->phandle) {
		pr_err("PCIe driver get handle failed.\n");
		return -EINVAL;
	}

	rc = mhi_dev_recover(mhi_ctx);
	if (rc) {
		pr_err("%s: get mhi state failed\n", __func__);
		return rc;
	}

	rc = mhi_init(mhi_ctx);
	if (rc)
		return rc;

	mhi_ctx->dma_cache = dma_alloc_coherent(&pdev->dev,
			(TRB_MAX_DATA_SIZE * 4),
			&mhi_ctx->cache_dma_handle, GFP_KERNEL);
	if (!mhi_ctx->dma_cache)
		return -ENOMEM;

	mhi_ctx->read_handle = dma_alloc_coherent(&pdev->dev,
			(TRB_MAX_DATA_SIZE * 4),
			&mhi_ctx->read_dma_handle,
			GFP_KERNEL);
	if (!mhi_ctx->read_handle)
		return -ENOMEM;

	mhi_ctx->write_handle = dma_alloc_coherent(&pdev->dev,
			(TRB_MAX_DATA_SIZE * 24),
			&mhi_ctx->write_dma_handle,
			GFP_KERNEL);
	if (!mhi_ctx->write_handle)
		return -ENOMEM;

	rc = mhi_dev_mmio_write(mhi_ctx, MHIVER, mhi_ctx->mhi_version);
	if (rc) {
		pr_err("Failed to update the MHI version\n");
		return rc;
	}
	mhi_ctx->event_reg.events = EP_PCIE_EVENT_PM_D3_HOT |
		EP_PCIE_EVENT_PM_D3_COLD |
		EP_PCIE_EVENT_PM_D0 |
		EP_PCIE_EVENT_PM_RST_DEAST |
		EP_PCIE_EVENT_L1SUB_TIMEOUT |
		EP_PCIE_EVENT_L1SUB_TIMEOUT_EXIT |
		EP_PCIE_EVENT_LINKDOWN;
	if (!mhi_ctx->mhi_int)
		mhi_ctx->event_reg.events |= EP_PCIE_EVENT_MHI_A7;
	mhi_ctx->event_reg.user = mhi_ctx;
	mhi_ctx->event_reg.mode = EP_PCIE_TRIGGER_CALLBACK;
	mhi_ctx->event_reg.callback = mhi_dev_sm_pcie_handler;

	rc = ep_pcie_register_event(mhi_ctx->phandle, &mhi_ctx->event_reg);
	if (rc) {
		pr_err("Failed to register for events from PCIe\n");
		return rc;
	}

	if (mhi_ctx->use_ipa) {
		pr_err("Registering with IPA\n");

		rc = ipa_register_ipa_ready_cb(mhi_ring_init_cb, mhi_ctx);
		if (rc < 0) {
			if (rc == -EEXIST) {
				mhi_ring_init_cb(mhi_ctx);
			} else {
				pr_err("Error calling IPA cb with %d\n", rc);
				return rc;
			}
		}
	}

	/* Invoke MHI SM when device is in RESET state */
	rc = mhi_dev_sm_init(mhi_ctx);
	if (rc) {
		pr_err("%s: Error during SM init\n", __func__);
		return rc;
	}

	/* set the env before setting the ready bit */
	rc = mhi_dev_mmio_set_env(mhi_ctx, MHI_ENV_VALUE);
	if (rc) {
		pr_err("%s: env setting failed\n", __func__);
		return rc;
	}

	/* All set, notify the host */
	mhi_dev_sm_set_ready();

	if (mhi_ctx->config_iatu || mhi_ctx->mhi_int) {
		rc = devm_request_irq(&pdev->dev, mhi_ctx->mhi_irq, mhi_dev_isr,
			IRQF_TRIGGER_HIGH, "mhi_isr", mhi_ctx);
		if (rc) {
			dev_err(&pdev->dev, "request mhi irq failed %d\n", rc);
			return -EINVAL;
		}

		disable_irq(mhi_ctx->mhi_irq);
	}

	if (mhi_ctx->use_edma)
		mhi_ring_init_cb(mhi_ctx);

	mhi_ctx->init_done = true;

	return 0;
}

static void mhi_dev_resume_init_with_link_up(struct ep_pcie_notify *notify)
{
	if (!notify || !notify->user) {
		pr_err("Null argument for notify\n");
		return;
	}

	mhi_ctx = notify->user;
	mhi_dev_pcie_notify_event = notify->options;
	mhi_log(MHI_MSG_INFO,
			"PCIe event=0x%x\n", notify->options);
	queue_work(mhi_ctx->pcie_event_wq, &mhi_ctx->pcie_event);
}

static void mhi_dev_pcie_handle_event(struct work_struct *work)
{
	struct mhi_dev *mhi_ctx = container_of(work, struct mhi_dev,
								pcie_event);
	int rc = 0;

	if (mhi_dev_pcie_notify_event == MHI_INIT) {
		rc = mhi_dev_resume_mmio_mhi_init(mhi_ctx);
		if (rc) {
			pr_err("Error during MHI device initialization\n");
			return;
		}
	} else if (mhi_dev_pcie_notify_event == MHI_REINIT) {
		rc = mhi_dev_resume_mmio_mhi_reinit(mhi_ctx);
		if (rc) {
			pr_err("Error during MHI device re-initialization\n");
			return;
		}
	}
}

static int mhi_edma_init(struct device *dev)
{
	mhi_ctx->tx_dma_chan = dma_request_slave_channel(dev, "tx");
	if (IS_ERR_OR_NULL(mhi_ctx->tx_dma_chan)) {
		pr_err("%s(): request for TX chan failed\n", __func__);
		return -EIO;
	}

	mhi_log(MHI_MSG_VERBOSE, "request for TX chan returned :%pK\n",
			mhi_ctx->tx_dma_chan);

	mhi_ctx->rx_dma_chan = dma_request_slave_channel(dev, "rx");
	if (IS_ERR_OR_NULL(mhi_ctx->rx_dma_chan)) {
		pr_err("%s(): request for RX chan failed\n", __func__);
		return -EIO;
	}
	mhi_log(MHI_MSG_VERBOSE, "request for RX chan returned :%pK\n",
			mhi_ctx->rx_dma_chan);
	return 0;
}

static int mhi_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	if (pdev->dev.of_node) {
		rc = get_device_tree_data(pdev);
		if (rc) {
			pr_err("Error reading MHI Dev DT\n");
			return rc;
		}
		mhi_ipc_log = ipc_log_context_create(MHI_IPC_LOG_PAGES,
								"mhi", 0);
		if (mhi_ipc_log == NULL) {
			dev_err(&pdev->dev,
				"Failed to create IPC logging context\n");
		}
		/*
		 * The below list and mutex should be initialized
		 * before calling mhi_uci_init to avoid crash in
		 * mhi_register_state_cb when accessing these.
		 */
		INIT_LIST_HEAD(&mhi_ctx->client_cb_list);
		mutex_init(&mhi_ctx->mhi_lock);

		mhi_uci_init();
		mhi_update_state_info(MHI_STATE_CONFIGURED);
	}

	if (mhi_ctx->use_edma) {
		rc = mhi_edma_init(&pdev->dev);
		if (rc) {
			pr_err("MHI: mhi edma init failed, rc = %d\n", rc);
			return rc;
		}

		rc = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
		if (rc) {
			pr_err("Error set MHI DMA mask: rc = %d\n", rc);
			return rc;
		}
	}

	if (mhi_ctx->use_edma) {
		mhi_ctx->read_from_host = mhi_dev_read_from_host_edma;
		mhi_ctx->write_to_host = mhi_dev_write_to_host_edma;
		mhi_ctx->host_to_device = mhi_transfer_host_to_device_edma;
		mhi_ctx->device_to_host = mhi_transfer_device_to_host_edma;
	} else {
		mhi_ctx->read_from_host = mhi_dev_read_from_host_ipa;
		mhi_ctx->write_to_host = mhi_dev_write_to_host_ipa;
		mhi_ctx->host_to_device = mhi_transfer_host_to_device_ipa;
		mhi_ctx->device_to_host = mhi_transfer_device_to_host_ipa;
	}

	INIT_WORK(&mhi_ctx->pcie_event, mhi_dev_pcie_handle_event);
	mhi_ctx->pcie_event_wq = alloc_workqueue("mhi_dev_pcie_event_wq",
							WQ_HIGHPRI, 0);
	if (!mhi_ctx->pcie_event_wq) {
		pr_err("no memory\n");
		rc = -ENOMEM;
		return rc;
	}

	mhi_ctx->phandle = ep_pcie_get_phandle(mhi_ctx->ifc_id);
	if (mhi_ctx->phandle) {
		/* PCIe link is already up */
		rc = mhi_dev_resume_mmio_mhi_init(mhi_ctx);
		if (rc) {
			pr_err("Error during MHI device initialization\n");
			return rc;
		}
	} else {
		pr_debug("Register a PCIe callback\n");
		mhi_ctx->event_reg.events = EP_PCIE_EVENT_LINKUP;
		mhi_ctx->event_reg.user = mhi_ctx;
		mhi_ctx->event_reg.mode = EP_PCIE_TRIGGER_CALLBACK;
		mhi_ctx->event_reg.callback = mhi_dev_resume_init_with_link_up;
		mhi_ctx->event_reg.options = MHI_INIT;

		rc = ep_pcie_register_event(mhi_ctx->phandle,
							&mhi_ctx->event_reg);
		if (rc) {
			pr_err("Failed to register for events from PCIe\n");
			return rc;
		}
	}

	return 0;
}

static int mhi_dev_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mhi_dev_match_table[] = {
	{	.compatible = "qcom,msm-mhi-dev" },
	{}
};

static struct platform_driver mhi_dev_driver = {
	.driver		= {
		.name	= "qcom,msm-mhi-dev",
		.of_match_table = mhi_dev_match_table,
	},
	.probe		= mhi_dev_probe,
	.remove		= mhi_dev_remove,
};

module_param(mhi_msg_lvl, uint, 0644);
module_param(mhi_ipc_msg_lvl, uint, 0644);

MODULE_PARM_DESC(mhi_msg_lvl, "mhi msg lvl");
MODULE_PARM_DESC(mhi_ipc_msg_lvl, "mhi ipc msg lvl");

static int __init mhi_dev_init(void)
{
	return platform_driver_register(&mhi_dev_driver);
}
subsys_initcall(mhi_dev_init);

static void __exit mhi_dev_exit(void)
{
	platform_driver_unregister(&mhi_dev_driver);
}
module_exit(mhi_dev_exit);

MODULE_DESCRIPTION("MHI device driver");
MODULE_LICENSE("GPL v2");
