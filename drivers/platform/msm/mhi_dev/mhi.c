/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/wakelock.h>

#include "mhi.h"
#include "mhi_hwio.h"
#include "mhi_sm.h"

/* Wait time on the device for Host to set M0 state */
#define MHI_DEV_M0_MAX_CNT		30
/* Wait time before suspend/resume is complete */
#define MHI_SUSPEND_MIN			100
#define MHI_SUSPEND_TIMEOUT		600
#define MHI_WAKEUP_TIMEOUT_CNT		20
#define MHI_MASK_CH_EV_LEN		32
#define MHI_RING_CMD_ID			0
#define MHI_RING_PRIMARY_EVT_ID		1
#define MHI_1K_SIZE			0x1000
/* Updated Specification for event start is NER - 2 and end - NER -1 */
#define MHI_HW_ACC_EVT_RING_START	2
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

enum mhi_msg_level mhi_msg_lvl = MHI_MSG_ERROR;
enum mhi_msg_level mhi_ipc_msg_lvl = MHI_MSG_VERBOSE;
void *mhi_ipc_log;

static struct mhi_dev *mhi_ctx;
static void mhi_hwc_cb(void *priv, enum ipa_mhi_event_type event,
	unsigned long data);
static void mhi_ring_init_cb(void *user_data);
static void mhi_update_state_info(uint32_t uevent_idx, enum mhi_ctrl_info info);
static int mhi_deinit(struct mhi_dev *mhi);
static void mhi_dev_resume_init_with_link_up(struct ep_pcie_notify *notify);
static int mhi_dev_pcie_notify_event;
static void mhi_dev_transfer_completion_cb(void *mreq);
static struct mhi_dev_uevent_info channel_state_info[MHI_MAX_CHANNELS];

/*
 * mhi_dev_ring_cache_completion_cb () - Call back function called
 * by IPA driver when ring element cache is done
 *
 * @req : ring cache request
 */
static void  mhi_dev_ring_cache_completion_cb(void *req)
{
	struct ring_cache_req *ring_req = NULL;

	if (req)
		ring_req = (struct ring_cache_req *)req;
	else {
		pr_err("%s():ring cache req data is NULL\n", __func__);
		return;
	}
	complete(ring_req->done);
}

void mhi_dev_read_from_host(struct mhi_dev *mhi, struct mhi_addr *transfer)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	struct ring_cache_req ring_req;

	DECLARE_COMPLETION(done);

	ring_req.done = &done;

	if (!mhi) {
		pr_err("invalid MHI ctx\n");
		return;
	}

	if (mhi->config_iatu) {
		offset = (uint64_t)transfer->host_pa - mhi->ctrl_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->ctrl_base.device_pa + offset;
	} else {
		host_addr_pa = transfer->host_pa | bit_40;
	}

	mhi_log(MHI_MSG_VERBOSE,
		"device 0x%x <<-- host 0x%llx, size %d\n",
		transfer->phy_addr, host_addr_pa,
		(int) transfer->size);
	rc = ipa_dma_async_memcpy((u64)transfer->phy_addr, host_addr_pa,
			(int)transfer->size,
			mhi_dev_ring_cache_completion_cb, &ring_req);
	if (rc)
		pr_err("error while reading from host:%d\n", rc);

	wait_for_completion(&done);

}
EXPORT_SYMBOL(mhi_dev_read_from_host);

void mhi_dev_write_to_host(struct mhi_dev *mhi, struct mhi_addr *transfer,
		struct event_req *ereq, enum mhi_dev_transfer_type tr_type)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	dma_addr_t dma;

	if (!mhi) {
		pr_err("invalid MHI ctx\n");
		return;
	}
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
		dma = dma_map_single(&mhi->pdev->dev,
				transfer->virt_addr, transfer->size,
				DMA_TO_DEVICE);
		if (ereq->event_type == SEND_EVENT_BUFFER) {
			ereq->dma = dma;
			ereq->dma_len = transfer->size;
		} else if (ereq->event_type == SEND_EVENT_RD_OFFSET) {
			ereq->event_rd_dma = dma;
		}
		rc = ipa_dma_async_memcpy(host_addr_pa, (uint64_t) dma,
				(int)transfer->size,
				ereq->client_cb, ereq);
		if (rc)
			pr_err("error while writing to host:%d\n", rc);
	} else if (tr_type == MHI_DEV_DMA_SYNC) {
		/* Copy the device content to a local device
		 * physical address */
		memcpy(mhi->dma_cache, transfer->virt_addr,
				transfer->size);
		rc = ipa_dma_sync_memcpy(host_addr_pa,
				(u64) mhi->cache_dma_handle,
				(int) transfer->size);
		if (rc)
			pr_err("error while writing to host:%d\n", rc);
	}
}
EXPORT_SYMBOL(mhi_dev_write_to_host);

int mhi_transfer_host_to_device(void *dev, uint64_t host_pa, uint32_t len,
		struct mhi_dev *mhi, struct mhi_req *mreq)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	struct mhi_dev_ring *ring = NULL;


	if (!mhi || !dev || !host_pa || !mreq) {
		pr_err("%s():Invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (mhi->config_iatu) {
		offset = (uint64_t)host_pa - mhi->data_base.host_pa;
		/* Mapping the translated physical address on the device */
		host_addr_pa = (uint64_t) mhi->data_base.device_pa + offset;
	} else {
		host_addr_pa = host_pa | bit_40;
	}

	mhi_log(MHI_MSG_VERBOSE, "device 0x%llx <-- host 0x%llx, size %d\n",
		(uint64_t) mhi->read_dma_handle, host_addr_pa, (int) len);

	if (mreq->mode == IPA_DMA_SYNC) {
		rc = ipa_dma_sync_memcpy((u64) mhi->read_dma_handle,
				host_addr_pa, (int) len);
		if (rc) {
			pr_err("error while reading chan using sync:%d\n", rc);
			return rc;
		}
		memcpy(dev, mhi->read_handle, len);
	} else if (mreq->mode == IPA_DMA_ASYNC) {
		ring = mreq->client->channel->ring;
		mreq->dma = dma_map_single(&mhi->pdev->dev, dev, len,
				DMA_FROM_DEVICE);
		mhi_dev_ring_inc_index(ring, ring->rd_offset);

		if (ring->rd_offset == ring->wr_offset)
			mreq->snd_cmpl = 1;
		else
			mreq->snd_cmpl = 0;
		rc = ipa_dma_async_memcpy(mreq->dma, host_addr_pa,
				(int) len, mhi_dev_transfer_completion_cb,
				mreq);
		if (rc) {
			pr_err("error while reading chan using async:%d\n", rc);
			return rc;
		}
	}
	return rc;
}
EXPORT_SYMBOL(mhi_transfer_host_to_device);

int mhi_transfer_device_to_host(uint64_t host_addr, void *dev, uint32_t len,
		struct mhi_dev *mhi, struct mhi_req *req)
{
	int rc = 0;
	uint64_t bit_40 = ((u64) 1) << 40, host_addr_pa = 0, offset = 0;
	struct mhi_dev_ring *ring = NULL;

	if (!mhi || !dev || !req  || !host_addr) {
		pr_err("%sInvalid parameters\n", __func__);
		return -EINVAL;
	}

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

	if (req->mode == IPA_DMA_SYNC) {
		memcpy(mhi->write_handle, dev, len);
		rc = ipa_dma_sync_memcpy(host_addr_pa,
				(u64) mhi->write_dma_handle, (int) len);
	} else if (req->mode == IPA_DMA_ASYNC) {
		req->dma = dma_map_single(&mhi->pdev->dev, req->buf,
				req->len, DMA_TO_DEVICE);
		ring = req->client->channel->ring;
		mhi_dev_ring_inc_index(ring, ring->rd_offset);
		if (ring->rd_offset == ring->wr_offset)
			req->snd_cmpl = 1;
		rc = ipa_dma_async_memcpy(host_addr_pa,
				(uint64_t) req->dma, (int) len,
				mhi_dev_transfer_completion_cb, req);
	}
	return rc;
}
EXPORT_SYMBOL(mhi_transfer_device_to_host);

int mhi_dev_is_list_empty(void)
{

	if (list_empty(&mhi_ctx->event_ring_list) &&
			list_empty(&mhi_ctx->process_ring_list))
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(mhi_dev_is_list_empty);

static void mhi_dev_get_erdb_db_cfg(struct mhi_dev *mhi,
				struct ep_pcie_db_config *erdb_cfg)
{
	switch (mhi->cfg.event_rings) {
	case NUM_CHANNELS:
		erdb_cfg->base = HW_CHANNEL_BASE;
		erdb_cfg->end = HW_CHANNEL_END;
		break;
	default:
		erdb_cfg->base = mhi->cfg.event_rings -
					MHI_HW_ACC_EVT_RING_START;
		erdb_cfg->end =  mhi->cfg.event_rings -
					MHI_HW_ACC_EVT_RING_END;
		break;
	}
}

int mhi_pcie_config_db_routing(struct mhi_dev *mhi)
{
	int rc = 0;
	struct ep_pcie_db_config chdb_cfg, erdb_cfg;

	if (!mhi) {
		pr_err("Invalid MHI context\n");
		return -EINVAL;
	}

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

	return rc;
}
EXPORT_SYMBOL(mhi_pcie_config_db_routing);

static int mhi_hwc_init(struct mhi_dev *mhi)
{
	int rc = 0;
	struct ep_pcie_msi_config cfg;
	struct ipa_mhi_init_params ipa_init_params;
	struct ep_pcie_db_config erdb_cfg;

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

	rc = ipa_mhi_init(&ipa_init_params);
	if (rc) {
		pr_err("Error initializing IPA\n");
		return rc;
	}

	return rc;
}

static int mhi_hwc_start(struct mhi_dev *mhi)
{
	int rc = 0;
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

	rc = ipa_mhi_start(&ipa_start_params);
	if (rc)
		pr_err("Error starting IPA (rc = 0x%X)\n", rc);

	return rc;
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

		rc = mhi_dev_mmio_enable_chdb_interrupts(mhi_ctx);
		if (rc) {
			pr_err("Failed to enable channel db\n");
			return;
		}

		rc = mhi_dev_mmio_enable_ctrl_interrupt(mhi_ctx);
		if (rc) {
			pr_err("Failed to enable control interrupt\n");
			return;
		}

		rc = mhi_dev_mmio_enable_cmdb_interrupt(mhi_ctx);

		if (rc) {
			pr_err("Failed to enable command db\n");
			return;
		}

		mhi_update_state_info(MHI_DEV_UEVENT_CTRL, MHI_STATE_CONNECTED);

		ep_pcie_mask_irq_event(mhi_ctx->phandle,
				EP_PCIE_INT_EVT_MHI_A7, true);
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
	int rc = 0;
	struct ipa_mhi_connect_params connect_params;

	memset(&connect_params, 0, sizeof(connect_params));

	switch (type) {
	case MHI_DEV_RING_EL_RESET:
	case MHI_DEV_RING_EL_STOP:
		rc = ipa_mhi_disconnect_pipe(
			mhi->ipa_clnt_hndl[chid-HW_CHANNEL_BASE]);
		if (rc)
			pr_err("Stopping HW Channel%d failed 0x%X\n",
							chid, rc);
		break;
	case MHI_DEV_RING_EL_START:
		connect_params.channel_id = chid;
		connect_params.sys.skip_ep_cfg = true;
		if ((chid % 2) == 0x0)
			connect_params.sys.client = IPA_CLIENT_MHI_PROD;
		else
			connect_params.sys.client = IPA_CLIENT_MHI_CONS;

		rc = ipa_mhi_connect_pipe(&connect_params,
			&mhi->ipa_clnt_hndl[chid-HW_CHANNEL_BASE]);
		if (rc)
			pr_err("HW Channel%d start failed 0x%X\n",
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

	mhi_dev_mmio_write(dev, MHI_CTRL_INT_CLEAR_A7, *int_value);
	if (rc) {
		pr_err("Failed to clear A7 status\n");
		return;
	}
}

static void mhi_dev_fetch_ch_ctx(struct mhi_dev *mhi, uint32_t ch_id)
{
	struct mhi_addr data_transfer;

	if (mhi->use_ipa) {
		data_transfer.host_pa = mhi->ch_ctx_shadow.host_pa +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;
		data_transfer.phy_addr = mhi->ch_ctx_cache_dma_handle +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;
	}

	data_transfer.size  = sizeof(struct mhi_dev_ch_ctx);
	/* Fetch the channel ctx (*dst, *src, size) */
	mhi_dev_read_from_host(mhi, &data_transfer);
}

int mhi_dev_syserr(struct mhi_dev *mhi)
{

	if (!mhi) {
		pr_err("%s: Invalid MHI ctx\n", __func__);
		return -EINVAL;
	}

	mhi_dev_dump_mmio(mhi);
	pr_err("MHI dev sys error\n");

	return 0;
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

	if (mhi->use_ipa)
		transfer_addr.host_pa = (mhi->ev_ctx_shadow.host_pa +
			sizeof(struct mhi_dev_ev_ctx) *
			evnt_ring) + (uint32_t) &ring->ring_ctx->ev.rp -
			(uint32_t) ring->ring_ctx;
	else
		transfer_addr.device_va = (mhi->ev_ctx_shadow.device_va +
			sizeof(struct mhi_dev_ev_ctx) *
			evnt_ring) + (uint32_t) &ring->ring_ctx->ev.rp -
			(uint32_t) ring->ring_ctx;

	transfer_addr.virt_addr = &ring->ring_ctx_shadow->ev.rp;
	transfer_addr.size = sizeof(uint64_t);

	mhi_dev_write_to_host(mhi, &transfer_addr, NULL, MHI_DEV_DMA_SYNC);
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
	rc = ep_pcie_trigger_msi(mhi_ctx->phandle, ctx->ev.msivec);
	if (rc) {
		pr_err("%s: error sending msi\n", __func__);
		return rc;
	}
	return rc;
}

/*
 * mhi_dev_event_buf_completion_cb() -Cb function called by IPA driver
 * when transfer completion event buffer copy is done.
 *
 * @req -  event_req structure
 */

static void mhi_dev_event_buf_completion_cb(void *req)
{
	struct event_req *ereq = NULL;

	if (req) {
		ereq = (struct event_req *)req;
	} else {
		pr_err("%s():event req data is invalid\n", __func__);
		return;
	}
	dma_unmap_single(&mhi_ctx->pdev->dev, ereq->dma,
			ereq->dma_len, DMA_TO_DEVICE);
}

/**
 * mhi_dev_event_rd_offset_completion_cb() -CB function called by IPA driver
 * when event rd_offset transfer is done.
 *
 * @req -  event_req structure
 */

static void mhi_dev_event_rd_offset_completion_cb(void *req)
{
	union mhi_dev_ring_ctx *ctx;
	int rc = 0;
	struct event_req *ereq = (struct event_req *)req;
	struct mhi_dev_channel *ch = ereq->context;
	struct mhi_dev *mhi = ch->ring->mhi_dev;
	unsigned long flags;

	dma_unmap_single(&mhi_ctx->pdev->dev, ereq->event_rd_dma,
			sizeof(uint64_t), DMA_TO_DEVICE);
	ctx = (union mhi_dev_ring_ctx *)&mhi->ev_ctx_cache[ereq->event_ring];
	rc = ep_pcie_trigger_msi(mhi_ctx->phandle, ctx->ev.msivec);
	if (rc)
		pr_err("%s: error sending in msi\n", __func__);

	/* return the event req to pre allocated pooled list */
	spin_lock_irqsave(&mhi->lock, flags);
	list_add_tail(&ereq->list, &ch->event_req_buffers);
	spin_unlock_irqrestore(&mhi->lock, flags);
}

static int mhi_dev_send_multiple_tr_events(struct mhi_dev *mhi, int evnt_ring,
		struct event_req *ereq, uint32_t evt_len)
{
	int rc = 0;
	uint64_t evnt_ring_idx = mhi->ev_ring_start + evnt_ring;
	struct mhi_dev_ring *ring = &mhi->ring[evnt_ring_idx];
	union mhi_dev_ring_ctx *ctx;
	struct mhi_addr transfer_addr;
	static int count;

	if (!ereq) {
		pr_err("%s(): invalid event req\n", __func__);
		return -EINVAL;
	}

	if (count == 0) {
		rc = ep_pcie_get_msi_config(mhi->phandle, &mhi->msi_cfg);
		if (rc) {
			pr_err("Error retrieving pcie msi logic\n");
			return rc;
		}
		count++;
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

	/* add the ring element */
	ereq->client_cb = mhi_dev_event_buf_completion_cb;
	ereq->event_type = SEND_EVENT_BUFFER;
	rc = mhi_dev_add_element(ring, ereq->tr_events, ereq, evt_len);
	if (rc) {
		pr_err("%s(): error in adding element rc %d\n", __func__, rc);
		return rc;
	}
	ring->ring_ctx_shadow->ev.rp = (ring->rd_offset *
		sizeof(union mhi_dev_ring_element_type)) +
		ring->ring_ctx->generic.rbase;

	mhi_log(MHI_MSG_VERBOSE, "ev.rp = %llx for %lld\n",
		ring->ring_ctx_shadow->ev.rp, evnt_ring_idx);

	if (mhi->use_ipa)
		transfer_addr.host_pa = (mhi->ev_ctx_shadow.host_pa +
		sizeof(struct mhi_dev_ev_ctx) *
		evnt_ring) + (uint32_t)&ring->ring_ctx->ev.rp -
		(uint32_t)ring->ring_ctx;
	else
		transfer_addr.device_va = (mhi->ev_ctx_shadow.device_va +
		sizeof(struct mhi_dev_ev_ctx) *
		evnt_ring) + (uint32_t)&ring->ring_ctx->ev.rp -
		(uint32_t)ring->ring_ctx;

	transfer_addr.virt_addr = &ring->ring_ctx_shadow->ev.rp;
	transfer_addr.size = sizeof(uint64_t);
	ereq->event_type = SEND_EVENT_RD_OFFSET;
	ereq->client_cb = mhi_dev_event_rd_offset_completion_cb;
	ereq->event_ring = evnt_ring;
	mhi_dev_write_to_host(mhi, &transfer_addr, ereq, MHI_DEV_DMA_ASYNC);
	return rc;
}

static int mhi_dev_send_completion_event(struct mhi_dev_channel *ch,
			uint32_t rd_ofst, uint32_t len,
			enum mhi_dev_cmd_completion_code code)
{
	int rc = 0;
	union mhi_dev_ring_element_type compl_event;
	struct mhi_dev *mhi = ch->ring->mhi_dev;

	compl_event.evt_tr_comp.chid = ch->ch_id;
	compl_event.evt_tr_comp.type =
				MHI_DEV_RING_EL_TRANSFER_COMPLETION_EVENT;
	compl_event.evt_tr_comp.len = len;
	compl_event.evt_tr_comp.code = code;
	compl_event.evt_tr_comp.ptr = ch->ring->ring_ctx->generic.rbase +
			rd_ofst * sizeof(struct mhi_dev_transfer_ring_element);

	rc = mhi_dev_send_event(mhi,
			mhi->ch_ctx_cache[ch->ch_id].err_indx, &compl_event);

	return rc;
}

int mhi_dev_send_state_change_event(struct mhi_dev *mhi,
						enum mhi_dev_state state)
{
	union mhi_dev_ring_element_type event;
	int rc = 0;

	event.evt_state_change.type = MHI_DEV_RING_EL_MHI_STATE_CHG;
	event.evt_state_change.mhistate = state;

	rc = mhi_dev_send_event(mhi, 0, &event);
	if (rc) {
		pr_err("Sending state change event failed\n");
		return rc;
	}

	return rc;
}
EXPORT_SYMBOL(mhi_dev_send_state_change_event);

int mhi_dev_send_ee_event(struct mhi_dev *mhi, enum mhi_dev_execenv exec_env)
{
	union mhi_dev_ring_element_type event;
	int rc = 0;

	event.evt_ee_state.type = MHI_DEV_RING_EL_EE_STATE_CHANGE_NOTIFY;
	event.evt_ee_state.execenv = exec_env;

	rc = mhi_dev_send_event(mhi, 0, &event);
	if (rc) {
		pr_err("Sending EE change event failed\n");
		return rc;
	}

	return rc;
}
EXPORT_SYMBOL(mhi_dev_send_ee_event);

static void mhi_dev_trigger_cb(void)
{
	struct mhi_dev_ready_cb_info *info;
	enum mhi_ctrl_info state_data;

	list_for_each_entry(info, &mhi_ctx->client_cb_list, list)
		if (info->cb) {
			mhi_ctrl_state_info(info->cb_data.channel, &state_data);
			info->cb_data.ctrl_info = state_data;
			info->cb(&info->cb_data);
		}
}

int mhi_dev_trigger_hw_acc_wakeup(struct mhi_dev *mhi)
{
	int rc = 0;

	/*
	 * Expected usuage is when there is HW ACC traffic IPA uC notifes
	 * Q6 -> IPA A7 -> MHI core -> MHI SM
	 */
	rc = mhi_dev_notify_sm_event(MHI_DEV_EVENT_HW_ACC_WAKEUP);
	if (rc) {
		pr_err("error sending SM event\n");
		return rc;
	}

	return rc;
}
EXPORT_SYMBOL(mhi_dev_trigger_hw_acc_wakeup);

static int mhi_dev_send_cmd_comp_event(struct mhi_dev *mhi)
{
	int rc = 0;
	union mhi_dev_ring_element_type event;

	/* send the command completion event to the host */
	event.evt_cmd_comp.ptr = mhi->cmd_ctx_cache->rbase
			+ (mhi->ring[MHI_RING_CMD_ID].rd_offset *
			(sizeof(union mhi_dev_ring_element_type)));
	mhi_log(MHI_MSG_VERBOSE, "evt cmd comp ptr :%d\n",
			(uint32_t) event.evt_cmd_comp.ptr);
	event.evt_cmd_comp.type = MHI_DEV_RING_EL_CMD_COMPLETION_EVT;
	event.evt_cmd_comp.code = MHI_CMD_COMPL_CODE_SUCCESS;

	rc = mhi_dev_send_event(mhi, 0, &event);
	if (rc)
		pr_err("channel start command faied\n");

	return rc;
}

static int mhi_dev_process_stop_cmd(struct mhi_dev_ring *ring, uint32_t ch_id,
							struct mhi_dev *mhi)
{
	int rc = 0;
	struct mhi_addr data_transfer;

	if (ring->rd_offset != ring->wr_offset &&
		mhi->ch_ctx_cache[ch_id].ch_type ==
				MHI_DEV_CH_TYPE_OUTBOUND_CHANNEL) {
		mhi_log(MHI_MSG_INFO, "Pending outbound transaction\n");
		return 0;
	} else if (mhi->ch_ctx_cache[ch_id].ch_type ==
			MHI_DEV_CH_TYPE_INBOUND_CHANNEL &&
			mhi->ch[ch_id].wr_request_active) {
		mhi_log(MHI_MSG_INFO, "Pending inbound transaction\n");
		return 0;
	}

	/* set the channel to stop */
	mhi->ch_ctx_cache[ch_id].ch_state = MHI_DEV_CH_STATE_STOP;
	mhi->ch[ch_id].state = MHI_DEV_CH_STOPPED;

	if (mhi->use_ipa) {
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
	mhi_dev_write_to_host(mhi, &data_transfer, NULL, MHI_DEV_DMA_SYNC);

	/* send the completion event to the host */
	rc = mhi_dev_send_cmd_comp_event(mhi);
	if (rc)
		pr_err("Error sending command completion event\n");

	return rc;
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
	char *connected[2] = { "MHI_CHANNEL_STATE_12=CONNECTED", NULL};
	char *disconnected[2] = { "MHI_CHANNEL_STATE_12=DISCONNECTED", NULL};

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
				pr_err("Error with HW channel cmd :%d\n", rc);
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
			return;
		}

		if (mhi->use_ipa)
			host_addr.host_pa = mhi->ch_ctx_shadow.host_pa +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;
		else
			host_addr.device_va = mhi->ch_ctx_shadow.device_va +
					sizeof(struct mhi_dev_ch_ctx) * ch_id;

		host_addr.virt_addr = &mhi->ch_ctx_cache[ch_id].ch_state;
		host_addr.size = sizeof(enum mhi_dev_ch_ctx_state);

		mhi_dev_write_to_host(mhi, &host_addr, NULL, MHI_DEV_DMA_SYNC);

send_start_completion_event:
		rc = mhi_dev_send_cmd_comp_event(mhi);
		if (rc)
			pr_err("Error sending command completion event\n");

		mhi_update_state_info(ch_id, MHI_STATE_CONNECTED);
		/* Trigger callback to clients */
		mhi_dev_trigger_cb();
		if (ch_id == MHI_CLIENT_MBIM_OUT) {
			rc = kobject_uevent_env(&mhi_ctx->dev->kobj,
						KOBJ_CHANGE, connected);
			if (rc)
				pr_err("Error sending uevent %d\n", rc);
		}
		break;
	case MHI_DEV_RING_EL_STOP:
		if (ch_id >= HW_CHANNEL_BASE) {
			rc = mhi_hwc_chcmd(mhi, ch_id, el->generic.type);
			if (rc) {
				mhi_log(MHI_MSG_ERROR,
					"send channel stop cmd event failed\n");
				return;
			}

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
			mhi_update_state_info(ch_id, MHI_STATE_DISCONNECTED);
			if (ch_id == MHI_CLIENT_MBIM_OUT) {
				rc = kobject_uevent_env(&mhi_ctx->dev->kobj,
						KOBJ_CHANGE, disconnected);
				if (rc)
					pr_err("Error sending uevent %d\n", rc);
			}
		}
		break;
	case MHI_DEV_RING_EL_RESET:
		mhi_log(MHI_MSG_VERBOSE,
			"received reset cmd for channel %d\n", ch_id);
		if (ch_id >= HW_CHANNEL_BASE) {
			rc = mhi_hwc_chcmd(mhi, ch_id, el->generic.type);
			if (rc) {
				mhi_log(MHI_MSG_ERROR,
					"send channel stop cmd event failed\n");
				return;
			}

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
			if (mhi->use_ipa)
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
			mhi_dev_write_to_host(mhi, &host_addr, NULL,
					MHI_DEV_DMA_SYNC);

			/* send the completion event to the host */
			rc = mhi_dev_send_cmd_comp_event(mhi);
			if (rc)
				pr_err("Error sending command completion event\n");
			mutex_unlock(&ch->ch_lock);
			mhi_update_state_info(ch_id, MHI_STATE_DISCONNECTED);
			if (ch_id == MHI_CLIENT_MBIM_OUT) {
				rc = kobject_uevent_env(&mhi_ctx->dev->kobj,
						KOBJ_CHANGE, disconnected);
				if (rc)
					pr_err("Error sending uevent %d\n", rc);
			}
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
			"invalid channel ring id (%d), should be < %d\n",
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
				break;
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

static int mhi_dev_abort(struct mhi_dev *mhi)
{
	struct mhi_dev_channel *ch;
	struct mhi_dev_ring *ring;
	int ch_id = 0, rc = 0;
	char *disconnected_12[2] = { "MHI_CHANNEL_STATE_12=DISCONNECTED", NULL};
	char *disconnected_14[2] = { "MHI_CHANNEL_STATE_14=DISCONNECTED", NULL};

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

	/* Update ctrl node */
	mhi_update_state_info(MHI_DEV_UEVENT_CTRL, MHI_STATE_DISCONNECTED);
	mhi_update_state_info(MHI_CLIENT_MBIM_OUT, MHI_STATE_DISCONNECTED);
	mhi_update_state_info(MHI_CLIENT_QMI_OUT, MHI_STATE_DISCONNECTED);
	rc = kobject_uevent_env(&mhi_ctx->dev->kobj,
				KOBJ_CHANGE, disconnected_12);
	if (rc)
		pr_err("Error sending uevent:%d\n", rc);

	rc = kobject_uevent_env(&mhi_ctx->dev->kobj,
				KOBJ_CHANGE, disconnected_14);
	if (rc)
		pr_err("Error sending uevent:%d\n", rc);

	flush_workqueue(mhi->ring_init_wq);
	flush_workqueue(mhi->pending_ring_wq);

	/* Initiate MHI IPA reset */
	ipa_mhi_destroy();

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
	struct mhi_dev_channel *ch;
	struct mhi_dev_client *client;
	union mhi_dev_ring_element_type *el;
	int rc = 0;
	struct mhi_req *req = (struct mhi_req *)mreq;
	struct mhi_req *local_req = NULL;
	union mhi_dev_ring_element_type *compl_ev = NULL;
	struct mhi_dev *mhi = NULL;
	unsigned long flags;

	client = req->client;
	ch = client->channel;
	mhi = ch->ring->mhi_dev;
	el = req->el;
	local_req = req;
	ch->curr_ereq->context = ch;

	dma_unmap_single(&mhi_ctx->pdev->dev, req->dma,
			req->len, DMA_FROM_DEVICE);

	/* Trigger client call back */
	req->client_cb(req);

	if (el->tre.ieot) {
		compl_ev = ch->curr_ereq->tr_events + ch->curr_ereq->num_events;
		compl_ev->evt_tr_comp.chid = ch->ch_id;
		compl_ev->evt_tr_comp.type =
				MHI_DEV_RING_EL_TRANSFER_COMPLETION_EVENT;
		compl_ev->evt_tr_comp.len = el->tre.len;
		compl_ev->evt_tr_comp.code = MHI_CMD_COMPL_CODE_EOT;
		compl_ev->evt_tr_comp.ptr = ch->ring->ring_ctx->generic.rbase +
				local_req->rd_offset * TR_RING_ELEMENT_SZ;
		ch->curr_ereq->num_events++;

		if (ch->curr_ereq->num_events >= MAX_TR_EVENTS ||
				local_req->snd_cmpl){
			mhi_log(MHI_MSG_VERBOSE,
					"num of tr events %d for ch %d\n",
					ch->curr_ereq->num_events, ch->ch_id);
			rc = mhi_dev_send_multiple_tr_events(mhi,
				mhi->ch_ctx_cache[ch->ch_id].err_indx,
				ch->curr_ereq, (ch->curr_ereq->num_events*
				sizeof(union mhi_dev_ring_element_type)));
			if (rc)
				mhi_log(MHI_MSG_ERROR,
						"failed to send compl evts\n");
			if (!list_empty(&ch->event_req_buffers)) {
				ch->curr_ereq =
					container_of(ch->event_req_buffers.next,
							struct event_req, list);
				spin_lock_irqsave(&mhi->lock, flags);
				list_del_init(&ch->curr_ereq->list);
				spin_unlock_irqrestore(&mhi->lock, flags);
				ch->curr_ereq->num_events = 0;
			} else
				pr_err("%s evt req buffers empty\n", __func__);
		}
	} else
		mhi_log(MHI_MSG_ERROR, "ieot is not valid\n");

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
	bool mhi_reset = false;
	uint32_t bhi_imgtxdb = 0;

	mutex_lock(&mhi_ctx->mhi_lock);
	/* Check for interrupts */
	mhi_dev_core_ack_ctrl_interrupts(mhi, &int_value);

	if (int_value & MHI_MMIO_CTRL_INT_STATUS_A7_MSK) {
		mhi_log(MHI_MSG_VERBOSE,
			"processing ctrl interrupt with %d\n", int_value);
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

		rc = mhi_dev_mmio_read(mhi, BHI_IMGTXDB, &bhi_imgtxdb);
		mhi_log(MHI_MSG_VERBOSE,
			"BHI_IMGTXDB = 0x%x\n", bhi_imgtxdb);
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
	int rc = 0;
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

	rc = ep_pcie_config_outbound_iatu(mhi_ctx->phandle, entries,
					MHI_HOST_REGION_NUM);
	if (rc) {
		pr_err("error configure iATU\n");
		return rc;
	}

	return 0;
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

		if (!mhi->use_ipa) {
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
	if (!mhi_ctx->ch_ctx_cache)
		return -ENOMEM;
	memset(mhi->ch_ctx_cache, 0, sizeof(struct mhi_dev_ch_ctx) *
						mhi->cfg.channels);

	if (mhi->use_ipa) {
		data_transfer.phy_addr = mhi->cmd_ctx_cache_dma_handle;
		data_transfer.host_pa = mhi->cmd_ctx_shadow.host_pa;
	}

	data_transfer.size = mhi->cmd_ctx_shadow.size;

	/* Cache the command and event context */
	mhi_dev_read_from_host(mhi, &data_transfer);

	if (mhi->use_ipa) {
		data_transfer.phy_addr = mhi->ev_ctx_cache_dma_handle;
		data_transfer.host_pa = mhi->ev_ctx_shadow.host_pa;
	}

	data_transfer.size = mhi->ev_ctx_shadow.size;

	mhi_dev_read_from_host(mhi, &data_transfer);

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

	rc = mhi_ring_start(&mhi->ring[0],
			(union mhi_dev_ring_ctx *)mhi->cmd_ctx_cache, mhi);
	if (rc) {
		pr_err("error in ring start\n");
		return rc;
	}

	return 0;
}

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

		if (mhi->use_ipa) {
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
		mhi_dev_write_to_host(mhi, &data_transfer, NULL,
				MHI_DEV_DMA_SYNC);

	}

	atomic_set(&mhi->mhi_dev_wake, 0);
	pm_relax(mhi->dev);
	mhi_log(MHI_MSG_VERBOSE, "releasing mhi wakelock\n");

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
		if (mhi->use_ipa) {
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
		mhi_dev_write_to_host(mhi, &data_transfer, NULL,
				MHI_DEV_DMA_SYNC);
	}
	mhi_update_state_info(MHI_DEV_UEVENT_CTRL, MHI_STATE_CONNECTED);

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

exit:
	mutex_unlock(&ch->ch_lock);
	return rc;
}
EXPORT_SYMBOL(mhi_dev_open_channel);

int mhi_dev_channel_isempty(struct mhi_dev_client *handle)
{
	struct mhi_dev_channel *ch;
	int rc;

	ch = handle->channel;

	rc = ch->ring->rd_offset == ch->ring->wr_offset;

	return rc;
}
EXPORT_SYMBOL(mhi_dev_channel_isempty);

int mhi_dev_close_channel(struct mhi_dev_client *handle)
{
	struct mhi_dev_channel *ch;
	int rc = 0;

	ch = handle->channel;

	mutex_lock(&ch->ch_lock);
	if (ch->state != MHI_DEV_CH_PENDING_START) {
		if (ch->ch_type == MHI_DEV_CH_TYPE_OUTBOUND_CHANNEL &&
					!mhi_dev_channel_isempty(handle)) {
			mhi_log(MHI_MSG_ERROR,
				"Trying to close an active channel (%d)\n",
				ch->ch_id);
			mutex_unlock(&ch->ch_lock);
			rc = -EAGAIN;
			goto exit;
		} else if (ch->tre_loc) {
			mhi_log(MHI_MSG_ERROR,
				"Trying to close channel (%d) when a TRE is active",
				ch->ch_id);
			mutex_unlock(&ch->ch_lock);
			rc = -EAGAIN;
			goto exit;
		}
	}

	ch->state = MHI_DEV_CH_CLOSED;
	ch->active_client = NULL;
	kfree(handle);
exit:
	mutex_unlock(&ch->ch_lock);
	return rc;
}
EXPORT_SYMBOL(mhi_dev_close_channel);

static int mhi_dev_check_tre_bytes_left(struct mhi_dev_channel *ch,
		struct mhi_dev_ring *ring, union mhi_dev_ring_element_type *el,
		uint32_t *chain)
{
	uint32_t td_done = 0;

	/*
	 * A full TRE worth of data was consumed.
	 * Check if we are at a TD boundary.
	 */
	if (ch->tre_bytes_left == 0) {
		if (el->tre.chain) {
			if (el->tre.ieob)
				mhi_dev_send_completion_event(ch,
					ring->rd_offset, el->tre.len,
					MHI_CMD_COMPL_CODE_EOB);
				*chain = 1;
		} else {
			if (el->tre.ieot)
				mhi_dev_send_completion_event(
					ch, ring->rd_offset, el->tre.len,
					MHI_CMD_COMPL_CODE_EOT);
				td_done = 1;
				*chain = 0;
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
	uint32_t write_to_loc = 0;
	size_t usr_buf_remaining;
	int td_done = 0, rc = 0;
	struct mhi_dev_client *handle_client;

	if (!mreq) {
		mhi_log(MHI_MSG_ERROR, "invalid mhi request\n");
		return -ENXIO;
	}

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
		write_to_loc = (uint32_t) mreq->buf +
			(mreq->len - usr_buf_remaining);
		ch->tre_bytes_left -= bytes_to_read;
		mreq->el = el;
		mreq->actual_len = bytes_read;
		mreq->rd_offset = ring->rd_offset;
		mhi_log(MHI_MSG_VERBOSE, "reading %d bytes from chan %d\n",
				bytes_to_read, mreq->chan);
		rc = mhi_transfer_host_to_device((void *) write_to_loc,
				read_from_loc, bytes_to_read, mhi_ctx, mreq);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
					"Error while reading chan (%d) rc %d\n",
					mreq->chan, rc);
			mutex_unlock(&ch->ch_lock);
			return rc;
		}
		usr_buf_remaining -= bytes_to_read;

		if (mreq->mode == IPA_DMA_ASYNC) {
			ch->tre_bytes_left = 0;
			ch->tre_loc = 0;
			goto exit;
		} else {
			td_done = mhi_dev_check_tre_bytes_left(ch, ring,
					el, &mreq->chain);
		}
	} while (usr_buf_remaining  && !td_done);
	if (td_done && ch->state == MHI_DEV_CH_PENDING_STOP) {
		ch->state = MHI_DEV_CH_STOPPED;
		rc = mhi_dev_process_stop_cmd(ring, mreq->chan , mhi_ctx);
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
	uint32_t read_from_loc;
	size_t usr_buf_remaining;
	size_t usr_buf_offset = 0;
	size_t bytes_to_write = 0;
	size_t bytes_written = 0;
	uint32_t tre_len = 0, suspend_wait_timeout = 0;

	if (!wreq || !wreq->client || !wreq->buf) {
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
	ch->wr_request_active = true;

	ring = ch->ring;

	mutex_lock(&ch->ch_lock);

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
			pr_err("%s(): rlen = %d, tlen = %d: client buf > tre len\n",
					__func__, wreq->len, tre_len);
			bytes_written = -ENOMEM;
			goto exit;
		}

		bytes_to_write = min(usr_buf_remaining, tre_len);
		usr_buf_offset = wreq->len - bytes_to_write;
		read_from_loc = (uint32_t) wreq->buf + usr_buf_offset;
		write_to_loc = el->tre.data_buf_ptr;
		wreq->rd_offset = ring->rd_offset;
		wreq->el = el;
		rc = mhi_transfer_device_to_host(write_to_loc,
						(void *) read_from_loc,
						bytes_to_write,
						mhi_ctx, wreq);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
					"Error while writing chan (%d) rc %d\n",
					wreq->chan, rc);
			goto exit;
		}
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
		if (wreq->mode == IPA_DMA_SYNC) {
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
	ch->wr_request_active = false;
	mutex_unlock(&ch->ch_lock);
	mutex_unlock(&mhi_ctx->mhi_write_test);
	return bytes_written;
}
EXPORT_SYMBOL(mhi_dev_write_channel);

static void mhi_dev_enable(struct work_struct *work)
{
	int rc = 0;
	struct ep_pcie_msi_config msi_cfg;
	struct mhi_dev *mhi = container_of(work,
				struct mhi_dev, ring_init_cb_work);
	bool mhi_reset;
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

	/*Enable MHI dev network stack Interface*/
	rc = mhi_dev_net_interface_init();
	if (rc)
		pr_err("%s Failed to initialize mhi_dev_net iface\n", __func__);

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

	mhi_update_state_info(MHI_DEV_UEVENT_CTRL,
						MHI_STATE_CONFIGURED);
}

static void mhi_ring_init_cb(void *data)
{
	struct mhi_dev *mhi = data;

	if (!mhi) {
		pr_err("Invalid MHI ctx\n");
		return;
	}

	queue_work(mhi->ring_init_wq, &mhi->ring_init_cb_work);
}

int mhi_register_state_cb(void (*mhi_state_cb)
				(struct mhi_dev_client_cb_data *cb_data),
				void *data, enum mhi_client_channel channel)
{
	struct mhi_dev_ready_cb_info *cb_info = NULL;

	if (!mhi_ctx) {
		pr_err("MHI device not ready\n");
		return -ENXIO;
	}

	if (channel > MHI_MAX_CHANNELS) {
		pr_err("Invalid channel :%d\n", channel);
		return -EINVAL;
	}

	mutex_lock(&mhi_ctx->mhi_lock);
	cb_info = kmalloc(sizeof(struct mhi_dev_ready_cb_info), GFP_KERNEL);
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

static void mhi_update_state_info(uint32_t uevent_idx, enum mhi_ctrl_info info)
{
	struct mhi_dev_client_cb_reason reason;

	if (uevent_idx == MHI_DEV_UEVENT_CTRL)
		mhi_ctx->ctrl_info = info;

	channel_state_info[uevent_idx].ctrl_info = info;

	if (uevent_idx == MHI_CLIENT_QMI_OUT ||
			uevent_idx == MHI_CLIENT_QMI_IN) {
		/* For legacy reasons for QTI client */
		reason.reason = MHI_DEV_CTRL_UPDATE;
		uci_ctrl_update(&reason);
	}
}

int mhi_ctrl_state_info(uint32_t idx, uint32_t *info)
{
	if (!info) {
		pr_err("Invalid info\n");
		return -EINVAL;
	}

	if (idx == MHI_DEV_UEVENT_CTRL)
		*info = mhi_ctx->ctrl_info;
	else
		*info = channel_state_info[idx].ctrl_info;

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
		rc = -EINVAL;
		pr_err("Request MHI MMIO physical memory region failed\n");
		return rc;
	}

	mhi->mmio_base_pa_addr = res_mem->start;
	mhi->mmio_base_addr = ioremap_nocache(res_mem->start, MHI_1K_SIZE);
	if (!mhi->mmio_base_addr) {
		pr_err("Failed to IO map MMIO registers.\n");
		rc = -EINVAL;
		return rc;
	}

	res_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ipa_uc_mbox_crdb");
	if (!res_mem) {
		rc = -EINVAL;
		pr_err("Request IPA_UC_MBOX CRDB physical region failed\n");
		return rc;
	}

	mhi->ipa_uc_mbox_crdb = res_mem->start;

	res_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ipa_uc_mbox_erdb");
	if (!res_mem) {
		rc = -EINVAL;
		pr_err("Request IPA_UC_MBOX ERDB physical region failed\n");
		return rc;
	}

	mhi->ipa_uc_mbox_erdb = res_mem->start;
	mhi_ctx = mhi;

	rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-ifc-id",
				&mhi_ctx->ifc_id);
	if (rc) {
		pr_err("qcom,mhi-ifc-id does not exist.\n");
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-ep-msi",
				&mhi_ctx->mhi_ep_msi_num);
	if (rc) {
		pr_err("qcom,mhi-ep-msi does not exist.\n");
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-version",
				&mhi_ctx->mhi_version);
	if (rc) {
		pr_err("qcom,mhi-version does not exist.\n");
		return rc;
	}

	mhi_ctx->use_ipa = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-ipa-software-channel");

	mhi_ctx->config_iatu = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,mhi-config-iatu");

	if (mhi_ctx->config_iatu) {
		rc = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,mhi-local-pa-base",
				&mhi_ctx->device_local_pa_base);
		if (rc) {
			pr_err("qcom,mhi-local-pa-base does not exist\n");
			return rc;
		}
	}

	mhi_ctx->mhi_int = of_property_read_bool((&pdev->dev)->of_node,
					"qcom,mhi-interrupt");

	if (mhi->config_iatu || mhi_ctx->mhi_int) {
		mhi->mhi_irq = platform_get_irq_byname(pdev, "mhi-device-inta");
		if (mhi->mhi_irq < 0) {
			pr_err("Invalid MHI device interrupt\n");
			rc = mhi->mhi_irq;
			return rc;
		}
	}

	device_init_wakeup(mhi->dev, true);
	/* MHI device will be woken up from PCIe event */
	device_set_wakeup_capable(mhi->dev, false);
	/* Hold a wakelock until completion of M0 */
	pm_stay_awake(mhi->dev);
	atomic_set(&mhi->mhi_dev_wake, 1);

	mhi_log(MHI_MSG_VERBOSE, "acquiring wakelock\n");

	return 0;
}

static int mhi_deinit(struct mhi_dev *mhi)
{
	int rc = 0, i = 0, ring_id = 0;
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
	}

	for (i = 0; i < mhi->cfg.channels; i++)
		mutex_destroy(&mhi->ch[i].ch_lock);

	devm_kfree(&pdev->dev, mhi->mmio_backup);
	devm_kfree(&pdev->dev, mhi->ch);
	devm_kfree(&pdev->dev, mhi->ring);

	mhi_dev_sm_exit(mhi);

	mhi->mmio_initialized = false;

	return rc;
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

	mhi->ch = devm_kzalloc(&pdev->dev,
			(sizeof(struct mhi_dev_channel) *
			(mhi->cfg.channels)), GFP_KERNEL);
	if (!mhi->ch)
		return -ENOMEM;


	for (i = 0; i < mhi->cfg.channels; i++) {
		mutex_init(&mhi->ch[i].ch_lock);
		if (i == MHI_CLIENT_IP_SW_4_OUT || i == MHI_CLIENT_IP_SW_4_IN) {
			int nreq = 0;

			INIT_LIST_HEAD(&mhi->ch[i].event_req_buffers);
			while (nreq < MHI_MAX_EVT_REQ) {
				struct event_req *ereq;
				/* Pre allocate event requests */
				ereq = kzalloc(sizeof(struct event_req),
						GFP_KERNEL);
				if (!ereq)
					return -ENOMEM;

				/* pre allocate buffers to queue
				 * transfer completion events
				 */
				ereq->tr_events = kzalloc(RING_ELEMENT_TYPE_SZ*
						MAX_TR_EVENTS, GFP_KERNEL);
				if (!ereq->tr_events) {
					kfree(ereq);
					return -ENOMEM;
				}
				list_add_tail(&ereq->list,
						&mhi->ch[i].event_req_buffers);
				nreq++;
			}
			mhi->ch[i].curr_ereq =
				container_of(mhi->ch[i].event_req_buffers.next,
						struct event_req, list);
			list_del_init(&mhi->ch[i].curr_ereq->list);
		}
	}

	spin_lock_init(&mhi->lock);
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
		EP_PCIE_EVENT_MHI_A7 |
		EP_PCIE_EVENT_LINKDOWN;
	mhi_ctx->event_reg.user = mhi_ctx;
	mhi_ctx->event_reg.mode = EP_PCIE_TRIGGER_CALLBACK;
	mhi_ctx->event_reg.callback = mhi_dev_sm_pcie_handler;

	rc = ep_pcie_register_event(mhi_ctx->phandle, &mhi_ctx->event_reg);
	if (rc) {
		pr_err("Failed to register for events from PCIe\n");
		goto fail;
	}

	rc = ipa_register_ipa_ready_cb(mhi_ring_init_cb, mhi_ctx);
	if (rc < 0) {
		if (rc == -EEXIST) {
			mhi_ring_init_cb(mhi_ctx);
		} else {
			pr_err("Error calling IPA cb with %d\n", rc);
			goto fail;
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

	mhi_ctx->phandle = ep_pcie_get_phandle(mhi_ctx->ifc_id);
	if (!mhi_ctx->phandle) {
		pr_err("PCIe driver get handle failed.\n");
		return -EINVAL;
	}

	mhi_ctx->event_reg.events = EP_PCIE_EVENT_PM_D3_HOT |
		EP_PCIE_EVENT_PM_D3_COLD |
		EP_PCIE_EVENT_PM_D0 |
		EP_PCIE_EVENT_PM_RST_DEAST |
		EP_PCIE_EVENT_MHI_A7 |
		EP_PCIE_EVENT_LINKDOWN;
	mhi_ctx->event_reg.user = mhi_ctx;
	mhi_ctx->event_reg.mode = EP_PCIE_TRIGGER_CALLBACK;
	mhi_ctx->event_reg.callback = mhi_dev_sm_pcie_handler;

	rc = ep_pcie_register_event(mhi_ctx->phandle, &mhi_ctx->event_reg);
	if (rc) {
		pr_err("Failed to register for events from PCIe\n");
		return rc;
	}

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

	return 0;
}

void mhi_dev_resume_init_with_link_up(struct ep_pcie_notify *notify)
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
		mhi_update_state_info(MHI_DEV_UEVENT_CTRL,
						MHI_STATE_CONFIGURED);
	}

	INIT_WORK(&mhi_ctx->pcie_event, mhi_dev_pcie_handle_event);
	mhi_ctx->pcie_event_wq = alloc_workqueue("mhi_dev_pcie_event_wq",
							WQ_HIGHPRI, 0);
	if (!mhi_ctx->pcie_event_wq) {
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

module_param(mhi_msg_lvl, uint, S_IRUGO | S_IWUSR);
module_param(mhi_ipc_msg_lvl, uint, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(mhi_msg_lvl, "mhi msg lvl");
MODULE_PARM_DESC(mhi_ipc_msg_lvl, "mhi ipc msg lvl");

static int __init mhi_dev_init(void)
{
	return platform_driver_register(&mhi_dev_driver);
}
module_init(mhi_dev_init);

static void __exit mhi_dev_exit(void)
{
	platform_driver_unregister(&mhi_dev_driver);
}
module_exit(mhi_dev_exit);

MODULE_DESCRIPTION("MHI device driver");
MODULE_LICENSE("GPL v2");
