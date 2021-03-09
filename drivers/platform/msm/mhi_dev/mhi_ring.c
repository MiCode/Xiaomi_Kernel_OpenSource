// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.*/

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>

#include "mhi.h"

static struct event_req dummy_ereq;

static void mhi_dev_event_buf_completion_dummy_cb(void *req)
{
	mhi_log(MHI_MSG_VERBOSE, "%s invoked\n", __func__);
}

static size_t mhi_dev_ring_addr2ofst(struct mhi_dev_ring *ring, uint64_t p)
{
	uint64_t rbase;

	rbase = ring->ring_ctx->generic.rbase;

	return (p - rbase)/sizeof(union mhi_dev_ring_element_type);
}

static uint32_t mhi_dev_ring_num_elems(struct mhi_dev_ring *ring)
{
	return ring->ring_ctx->generic.rlen/
			sizeof(union mhi_dev_ring_element_type);
}

int mhi_dev_fetch_ring_elements(struct mhi_dev_ring *ring,
					size_t start, size_t end)
{
	struct mhi_addr host_addr;
	struct mhi_dev *mhi_ctx;

	mhi_ctx = ring->mhi_dev;

	/* fetch ring elements from start->end, take care of wrap-around case */
	if (MHI_USE_DMA(mhi_ctx)) {
		host_addr.host_pa = ring->ring_shadow.host_pa
			+ sizeof(union mhi_dev_ring_element_type) * start;
		host_addr.phy_addr = ring->ring_cache_dma_handle +
			(sizeof(union mhi_dev_ring_element_type) * start);
	} else {
		host_addr.device_va = ring->ring_shadow.device_va
			+ sizeof(union mhi_dev_ring_element_type) * start;
		host_addr.virt_addr = &ring->ring_cache[start];
	}
	host_addr.size = (end-start) * sizeof(union mhi_dev_ring_element_type);
	if (start < end) {
		mhi_ctx->read_from_host(ring->mhi_dev, &host_addr);
	} else if (start > end) {
		/* copy from 'start' to ring end, then ring start to 'end'*/
		host_addr.size = (ring->ring_size-start) *
					sizeof(union mhi_dev_ring_element_type);
		mhi_ctx->read_from_host(ring->mhi_dev, &host_addr);
		if (end) {
			/* wrapped around */
			host_addr.device_pa = ring->ring_shadow.device_pa;
			host_addr.device_va = ring->ring_shadow.device_va;
			host_addr.host_pa = ring->ring_shadow.host_pa;
			host_addr.virt_addr = &ring->ring_cache[0];
			host_addr.phy_addr = ring->ring_cache_dma_handle;
			host_addr.size = (end *
				sizeof(union mhi_dev_ring_element_type));
			mhi_ctx->read_from_host(ring->mhi_dev,
							&host_addr);
		}
	}
	return 0;
}

int mhi_dev_cache_ring(struct mhi_dev_ring *ring, size_t wr_offset)
{
	size_t old_offset = 0;
	struct mhi_dev *mhi_ctx;

	if (WARN_ON(!ring))
		return -EINVAL;

	mhi_ctx = ring->mhi_dev;

	if (ring->wr_offset == wr_offset) {
		mhi_log(MHI_MSG_VERBOSE,
			"nothing to cache for ring %d, local wr_ofst %lu\n",
			ring->id, ring->wr_offset);
		mhi_log(MHI_MSG_VERBOSE,
			"new wr_offset %lu\n", wr_offset);
		return 0;
	}

	old_offset = ring->wr_offset;

	/*
	 * copy the elements starting from old_offset to wr_offset
	 * take in to account wrap around case event rings are not
	 * cached, not required
	 */
	if (ring->id >= mhi_ctx->ev_ring_start &&
		ring->id < (mhi_ctx->ev_ring_start +
				mhi_ctx->cfg.event_rings)) {
		mhi_log(MHI_MSG_VERBOSE,
				"not caching event ring %d\n", ring->id);
		return 0;
	}

	mhi_log(MHI_MSG_VERBOSE, "caching ring %d, start %lu, end %lu\n",
			ring->id, old_offset, wr_offset);

	if (mhi_dev_fetch_ring_elements(ring, old_offset, wr_offset)) {
		mhi_log(MHI_MSG_ERROR,
		"failed to fetch elements for ring %d, start %lu, end %lu\n",
		ring->id, old_offset, wr_offset);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_cache_ring);

int mhi_dev_update_wr_offset(struct mhi_dev_ring *ring)
{
	uint64_t wr_offset = 0;
	size_t new_wr_offset = 0;
	int32_t rc = 0;

	if (WARN_ON(!ring))
		return -EINVAL;

	switch (ring->type) {
	case RING_TYPE_CMD:
		rc = mhi_dev_mmio_get_cmd_db(ring, &wr_offset);
		if (rc) {
			pr_err("%s: CMD DB read failed\n", __func__);
			return rc;
		}
		mhi_log(MHI_MSG_VERBOSE,
			"ring %d wr_offset from db 0x%lx\n",
			ring->id, (size_t) wr_offset);
		break;
	case RING_TYPE_ER:
		rc = mhi_dev_mmio_get_erc_db(ring, &wr_offset);
		if (rc) {
			pr_err("%s: EVT DB read failed\n", __func__);
			return rc;
		}
		break;
	case RING_TYPE_CH:
		rc = mhi_dev_mmio_get_ch_db(ring, &wr_offset);
		if (rc) {
			pr_err("%s: CH DB read failed\n", __func__);
			return rc;
		}
		mhi_log(MHI_MSG_VERBOSE,
			"ring %d wr_offset from db 0x%lx\n",
			ring->id, (size_t) wr_offset);
		break;
	default:
		mhi_log(MHI_MSG_ERROR, "invalid ring type\n");
		return -EINVAL;
	}

	new_wr_offset = mhi_dev_ring_addr2ofst(ring, wr_offset);

	mhi_dev_cache_ring(ring, new_wr_offset);

	ring->wr_offset = new_wr_offset;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_update_wr_offset);

int mhi_dev_process_ring_element(struct mhi_dev_ring *ring, size_t offset)
{
	union mhi_dev_ring_element_type *el;

	if (WARN_ON(!ring))
		return -EINVAL;

	/* get the element and invoke the respective callback */
	el = &ring->ring_cache[offset];

	mhi_log(MHI_MSG_VERBOSE, "evnt ptr : 0x%llx\n", el->tre.data_buf_ptr);
	mhi_log(MHI_MSG_VERBOSE, "evnt len : 0x%x, offset:%lu\n",
						el->tre.len, offset);

	if (ring->ring_cb)
		return ring->ring_cb(ring->mhi_dev, el, (void *)ring);
	else
		mhi_log(MHI_MSG_ERROR, "No callback registered for ring %d\n",
				ring->id);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_process_ring_element);

int mhi_dev_process_ring(struct mhi_dev_ring *ring)
{
	int rc = 0;
	union mhi_dev_ring_element_type *el;

	if (WARN_ON(!ring))
		return -EINVAL;

	mhi_log(MHI_MSG_VERBOSE,
			"Before wr update ring_id (%d) element (%lu) with wr:%lu\n",
			ring->id, ring->rd_offset, ring->wr_offset);

	rc = mhi_dev_update_wr_offset(ring);
	if (rc) {
		mhi_log(MHI_MSG_ERROR,
				"Error updating write-offset for ring %d\n",
				ring->id);
		return rc;
	}

	/* get the element and invoke the respective callback */
	el = &ring->ring_cache[ring->wr_offset];

	mhi_log(MHI_MSG_VERBOSE, "evnt ptr : 0x%llx\n", el->tre.data_buf_ptr);
	mhi_log(MHI_MSG_VERBOSE, "evnt len : 0x%x, wr_offset:%lu\n",
						el->tre.len, ring->wr_offset);

	if (ring->type == RING_TYPE_CH) {
		/* notify the clients that there are elements in the ring */
		rc = mhi_dev_process_ring_element(ring, ring->rd_offset);
		if (rc)
			pr_err("Error fetching elements\n");
		return rc;
	}
	mhi_log(MHI_MSG_VERBOSE,
			"After ring update ring_id (%d) element (%lu) with wr:%lu\n",
			ring->id, ring->rd_offset, ring->wr_offset);

	while (ring->rd_offset != ring->wr_offset) {
		rc = mhi_dev_process_ring_element(ring, ring->rd_offset);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"Error processing ring (%d) element (%lu)\n",
				ring->id, ring->rd_offset);
			return rc;
		}

		mhi_log(MHI_MSG_VERBOSE,
			"Processing ring (%d) rd_offset:%lu, wr_offset:%lu\n",
			ring->id, ring->rd_offset, ring->wr_offset);

		mhi_dev_ring_inc_index(ring, ring->rd_offset);
	}

	if (!(ring->rd_offset == ring->wr_offset)) {
		mhi_log(MHI_MSG_ERROR,
				"Error with the rd offset/wr offset\n");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_process_ring);

int mhi_dev_add_element(struct mhi_dev_ring *ring,
				union mhi_dev_ring_element_type *element,
				struct event_req *ereq, int size)
{
	size_t old_offset = 0;
	struct mhi_addr host_addr;
	uint32_t num_elem = 1;
	uint32_t num_free_elem;
	struct mhi_dev *mhi_ctx;
	uint32_t i;

	if (WARN_ON(!ring || !element))
		return -EINVAL;
	mhi_ctx = ring->mhi_dev;

	mhi_dev_update_wr_offset(ring);

	if (ereq)
		num_elem = size / (sizeof(union mhi_dev_ring_element_type));

	if (ring->rd_offset < ring->wr_offset)
		num_free_elem = ring->wr_offset - ring->rd_offset - 1;
	else
		num_free_elem = ring->ring_size - ring->rd_offset +
				ring->wr_offset - 1;

	if (num_free_elem < num_elem) {
		mhi_log(MHI_MSG_ERROR, "No space to add %d elem in ring (%d)\n",
			num_elem, ring->id);
		return -EINVAL;
	}

	old_offset = ring->rd_offset;

	if (ereq) {
		ring->rd_offset += num_elem;
		if (ring->rd_offset >= ring->ring_size)
			ring->rd_offset -= ring->ring_size;
	} else
		mhi_dev_ring_inc_index(ring, ring->rd_offset);

	mhi_log(MHI_MSG_VERBOSE,
		"Writing %d elements, ring old 0x%x, new 0x%x\n",
		num_elem, old_offset, ring->rd_offset);

	ring->ring_ctx->generic.rp = (ring->rd_offset *
		sizeof(union mhi_dev_ring_element_type)) +
		ring->ring_ctx->generic.rbase;
	/*
	 * Write the element, ring_base has to be the
	 * iomap of the ring_base for memcpy
	 */

	if (MHI_USE_DMA(mhi_ctx))
		host_addr.host_pa = ring->ring_shadow.host_pa +
			sizeof(union mhi_dev_ring_element_type) * old_offset;
	else
		host_addr.device_va = ring->ring_shadow.device_va +
			sizeof(union mhi_dev_ring_element_type) * old_offset;

	if (!ereq) {
		/* We're adding only a single ring element */
		host_addr.virt_addr = element;
		host_addr.size = sizeof(union mhi_dev_ring_element_type);

		mhi_log(MHI_MSG_VERBOSE, "adding element to ring (%d)\n",
					ring->id);
		mhi_log(MHI_MSG_VERBOSE, "rd_ofset %lu\n", ring->rd_offset);
		mhi_log(MHI_MSG_VERBOSE, "type %d\n", element->generic.type);

		mhi_ctx->write_to_host(ring->mhi_dev, &host_addr,
			NULL, MHI_DEV_DMA_SYNC);
		return 0;
	}

	// Log elements added to ring
	for (i = 0; i < num_elem; ++i) {
		mhi_log(MHI_MSG_VERBOSE, "evnt ptr : 0x%llx\n",
			(element + i)->evt_tr_comp.ptr);
		mhi_log(MHI_MSG_VERBOSE, "evnt len : 0x%x\n",
			(element + i)->evt_tr_comp.len);
		mhi_log(MHI_MSG_VERBOSE, "evnt code :0x%x\n",
			(element + i)->evt_tr_comp.code);
		mhi_log(MHI_MSG_VERBOSE, "evnt type :0x%x\n",
			(element + i)->evt_tr_comp.type);
		mhi_log(MHI_MSG_VERBOSE, "evnt chid :0x%x\n",
			(element + i)->evt_tr_comp.chid);
	}
	/* Adding multiple ring elements */
	if (ring->rd_offset == 0 || (ring->rd_offset > old_offset)) {
		/* No wrap-around case */
		host_addr.virt_addr = element;
		host_addr.size = size;
		host_addr.phy_addr = 0;
		mhi_ctx->write_to_host(ring->mhi_dev, &host_addr,
			ereq, MHI_DEV_DMA_ASYNC);
	} else {
		mhi_log(MHI_MSG_VERBOSE, "Wrap around case\n");
		/* Wrap-around case - first chunk uses dma sync */
		host_addr.virt_addr = element;
		host_addr.size = (ring->ring_size - old_offset) *
			sizeof(union mhi_dev_ring_element_type);

		if (mhi_ctx->use_ipa) {
			mhi_ctx->write_to_host(ring->mhi_dev, &host_addr,
				NULL, MHI_DEV_DMA_SYNC);
		} else {
			dummy_ereq.event_type = SEND_EVENT_BUFFER;
			host_addr.phy_addr = 0;
			/* Nothing to do in the callback */
			dummy_ereq.client_cb =
				mhi_dev_event_buf_completion_dummy_cb;
			mhi_ctx->write_to_host(ring->mhi_dev, &host_addr,
					&dummy_ereq, MHI_DEV_DMA_ASYNC);
		}

		/* Copy remaining elements */
		if (MHI_USE_DMA(mhi_ctx))
			host_addr.host_pa = ring->ring_shadow.host_pa;
		else
			host_addr.device_va = ring->ring_shadow.device_va;
		host_addr.virt_addr = element + (ring->ring_size - old_offset);
		host_addr.size = ring->rd_offset *
			sizeof(union mhi_dev_ring_element_type);
		host_addr.phy_addr = 0;
		mhi_ctx->write_to_host(ring->mhi_dev, &host_addr,
			ereq, MHI_DEV_DMA_ASYNC);
	}
	return 0;
}
EXPORT_SYMBOL(mhi_dev_add_element);

static int mhi_dev_ring_alloc_msi_buf(struct mhi_dev_ring *ring)
{
	if (ring->msi_buffer.buf) {
		mhi_log(MHI_MSG_INFO, "MSI buf already allocated\n");
		return 0;
	}

	ring->msi_buffer.buf = dma_alloc_coherent(&ring->mhi_dev->pdev->dev,
				sizeof(u32),
				&ring->msi_buffer.dma_addr,
				GFP_KERNEL);

	if (!ring->msi_buffer.buf)
		return -ENOMEM;

	return 0;
}

int mhi_ring_start(struct mhi_dev_ring *ring, union mhi_dev_ring_ctx *ctx,
							struct mhi_dev *mhi)
{
	int rc = 0;
	size_t wr_offset = 0;
	size_t offset = 0;

	if (WARN_ON(!ring || !ctx || !mhi))
		return -EINVAL;

	ring->ring_ctx = ctx;
	ring->ring_size = mhi_dev_ring_num_elems(ring);
	ring->rd_offset = mhi_dev_ring_addr2ofst(ring,
					ring->ring_ctx->generic.rp);
	ring->wr_offset = mhi_dev_ring_addr2ofst(ring,
					ring->ring_ctx->generic.rp);
	ring->mhi_dev = mhi;

	mhi_ring_set_state(ring, RING_STATE_IDLE);

	wr_offset = mhi_dev_ring_addr2ofst(ring,
					ring->ring_ctx->generic.wp);

	if (!ring->ring_cache) {
		ring->ring_cache = dma_alloc_coherent(mhi->dev,
				ring->ring_size *
				sizeof(union mhi_dev_ring_element_type),
				&ring->ring_cache_dma_handle,
				GFP_KERNEL);
		if (!ring->ring_cache) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to allocate ring cache\n");
			return -ENOMEM;
		}
	}

	if (ring->type == RING_TYPE_ER) {
		if (!ring->evt_rp_cache) {
			ring->evt_rp_cache = dma_alloc_coherent(mhi->dev,
				sizeof(uint64_t) * ring->ring_size,
				&ring->evt_rp_cache_dma_handle,
				GFP_KERNEL);
			if (!ring->evt_rp_cache) {
				mhi_log(MHI_MSG_ERROR,
					"Failed to allocate evt rp cache\n");
				rc = -ENOMEM;
				goto cleanup;
			}
		}
		if (!ring->msi_buf) {
			ring->msi_buf = dma_alloc_coherent(mhi->dev,
				sizeof(uint32_t),
				&ring->msi_buf_dma_handle,
				GFP_KERNEL);
			if (!ring->msi_buf) {
				mhi_log(MHI_MSG_ERROR,
					"Failed to allocate msi buf\n");
				rc = -ENOMEM;
				goto cleanup;
			}
		}
	}

	offset = (size_t)(ring->ring_ctx->generic.rbase -
					mhi->ctrl_base.host_pa);

	ring->ring_shadow.device_pa = mhi->ctrl_base.device_pa + offset;
	ring->ring_shadow.device_va = mhi->ctrl_base.device_va + offset;
	ring->ring_shadow.host_pa = mhi->ctrl_base.host_pa + offset;

	if (ring->type == RING_TYPE_ER)
		ring->ring_ctx_shadow =
		(union mhi_dev_ring_ctx *) (mhi->ev_ctx_shadow.device_va +
			(ring->id - mhi->ev_ring_start) *
			sizeof(union mhi_dev_ring_ctx));
	else if (ring->type == RING_TYPE_CMD)
		ring->ring_ctx_shadow =
		(union mhi_dev_ring_ctx *) mhi->cmd_ctx_shadow.device_va;
	else if (ring->type == RING_TYPE_CH)
		ring->ring_ctx_shadow =
		(union mhi_dev_ring_ctx *) (mhi->ch_ctx_shadow.device_va +
		(ring->id - mhi->ch_ring_start)*sizeof(union mhi_dev_ring_ctx));

	ring->ring_ctx_shadow = ring->ring_ctx;

	if (ring->type != RING_TYPE_ER && ring->type != RING_TYPE_CH) {
		rc = mhi_dev_cache_ring(ring, wr_offset);
		if (rc)
			return rc;
	}

	mhi_log(MHI_MSG_VERBOSE, "ctx ring_base:0x%lx, rp:0x%lx, wp:0x%lx\n",
			(size_t)ring->ring_ctx->generic.rbase,
			(size_t)ring->ring_ctx->generic.rp,
			(size_t)ring->ring_ctx->generic.wp);
	ring->wr_offset = wr_offset;

	if (mhi->use_edma) {
		rc = mhi_dev_ring_alloc_msi_buf(ring);
		if (rc)
			return rc;
	}
	return rc;

cleanup:
	dma_free_coherent(mhi->dev,
		ring->ring_size *
		sizeof(union mhi_dev_ring_element_type),
		ring->ring_cache,
		ring->ring_cache_dma_handle);
	ring->ring_cache = NULL;
	if (ring->evt_rp_cache) {
		dma_free_coherent(mhi->dev,
			sizeof(uint64_t) * ring->ring_size,
			ring->evt_rp_cache,
			ring->evt_rp_cache_dma_handle);
		ring->evt_rp_cache = NULL;
	}
	return rc;
}
EXPORT_SYMBOL(mhi_ring_start);

void mhi_ring_init(struct mhi_dev_ring *ring, enum mhi_dev_ring_type type,
								int id)
{
	if (WARN_ON(!ring))
		return;

	ring->id = id;
	ring->state = RING_STATE_UINT;
	ring->ring_cb = NULL;
	ring->type = type;
	mutex_init(&ring->event_lock);
}
EXPORT_SYMBOL(mhi_ring_init);

void mhi_ring_set_cb(struct mhi_dev_ring *ring,
			int (*ring_cb)(struct mhi_dev *dev,
			union mhi_dev_ring_element_type *el, void *ctx))
{
	if (WARN_ON(!ring || !ring_cb))
		return;

	ring->ring_cb = ring_cb;
}
EXPORT_SYMBOL(mhi_ring_set_cb);

void mhi_ring_set_state(struct mhi_dev_ring *ring,
				enum mhi_dev_ring_state state)
{
	if (WARN_ON(!ring))
		return;

	if (state > RING_STATE_PENDING) {
		pr_err("%s: Invalid ring state\n", __func__);
		return;
	}

	ring->state = state;
}
EXPORT_SYMBOL(mhi_ring_set_state);

enum mhi_dev_ring_state mhi_ring_get_state(struct mhi_dev_ring *ring)
{
	if (WARN_ON(!ring))
		return -EINVAL;

	return ring->state;
}
EXPORT_SYMBOL(mhi_ring_get_state);
