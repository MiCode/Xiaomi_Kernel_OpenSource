/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

static uint32_t mhi_dev_ring_addr2ofst(struct mhi_dev_ring *ring, uint64_t p)
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

/* fetch ring elements from stat->end, take care of wrap-around case */
int mhi_dev_fetch_ring_elements(struct mhi_dev_ring *ring,
					uint32_t start, uint32_t end)
{
	struct mhi_addr host_addr;

	if (ring->mhi_dev->use_ipa) {
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
		mhi_dev_read_from_host(ring->mhi_dev, &host_addr);
	} else if (start > end) {
		/* copy from 'start' to ring end, then ring start to 'end'*/
		host_addr.size = (ring->ring_size-start) *
					sizeof(union mhi_dev_ring_element_type);
		mhi_dev_read_from_host(ring->mhi_dev, &host_addr);
		if (end) {
			/* wrapped around */
			host_addr.device_pa = ring->ring_shadow.device_pa;
			host_addr.device_va = ring->ring_shadow.device_va;
			host_addr.host_pa = ring->ring_shadow.host_pa;
			host_addr.virt_addr = &ring->ring_cache[0];
			host_addr.phy_addr = (ring->ring_cache_dma_handle +
				sizeof(union mhi_dev_ring_element_type) *
				start);
			host_addr.size = (end *
				sizeof(union mhi_dev_ring_element_type));
			mhi_dev_read_from_host(ring->mhi_dev, &host_addr);
		}
	}

	return 0;
}

int mhi_dev_cache_ring(struct mhi_dev_ring *ring, uint32_t wr_offset)
{
	uint32_t old_offset = 0;
	struct mhi_dev *mhi_ctx;

	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return -EINVAL;
	}

	mhi_ctx = ring->mhi_dev;

	if (ring->wr_offset == wr_offset) {
		mhi_log(MHI_MSG_VERBOSE,
			"nothing to cache for ring %d, local wr_ofst %d\n",
			ring->id, ring->wr_offset);
		mhi_log(MHI_MSG_VERBOSE,
			"new wr_offset %d\n", wr_offset);
		return 0;
	}

	old_offset = ring->wr_offset;

	mhi_log(MHI_MSG_VERBOSE,
			"caching - rng size :%d local ofst:%d new ofst: %d\n",
			(uint32_t) ring->ring_size, old_offset,
			ring->wr_offset);

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

	mhi_log(MHI_MSG_VERBOSE, "caching ring %d, start %d, end %d\n",
			ring->id, old_offset, wr_offset);

	if (mhi_dev_fetch_ring_elements(ring, old_offset, wr_offset)) {
		mhi_log(MHI_MSG_ERROR,
		"failed to fetch elements for ring %d, start %d, end %d\n",
		ring->id, old_offset, wr_offset);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_cache_ring);

int mhi_dev_update_wr_offset(struct mhi_dev_ring *ring)
{
	uint64_t wr_offset = 0;
	uint32_t new_wr_offset = 0;
	int32_t rc = 0;

	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return -EINVAL;
	}

	switch (ring->type) {
	case RING_TYPE_CMD:
		rc = mhi_dev_mmio_get_cmd_db(ring, &wr_offset);
		if (rc) {
			pr_err("%s: CMD DB read failed\n", __func__);
			return rc;
		}
		mhi_log(MHI_MSG_VERBOSE,
			"ring %d wr_offset from db 0x%x\n",
			ring->id, (uint32_t) wr_offset);
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
			"ring %d wr_offset from db 0x%x\n",
			ring->id, (uint32_t) wr_offset);
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

int mhi_dev_process_ring_element(struct mhi_dev_ring *ring, uint32_t offset)
{
	union mhi_dev_ring_element_type *el;

	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return -EINVAL;
	}

	/* get the element and invoke the respective callback */
	el = &ring->ring_cache[offset];

	if (ring->ring_cb)
		ring->ring_cb(ring->mhi_dev, el, (void *)ring);
	else
		mhi_log(MHI_MSG_ERROR, "No callback registered for ring %d\n",
				ring->id);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_process_ring_element);

int mhi_dev_process_ring(struct mhi_dev_ring *ring)
{
	int rc = 0;

	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return -EINVAL;
	}

	rc = mhi_dev_update_wr_offset(ring);
	if (rc) {
		mhi_log(MHI_MSG_ERROR,
				"Error updating write-offset for ring %d\n",
				ring->id);
		return rc;
	}

	if (ring->type == RING_TYPE_CH) {
		/* notify the clients that there are elements in the ring */
		rc = mhi_dev_process_ring_element(ring, ring->rd_offset);
		if (rc)
			pr_err("Error fetching elements\n");
		return rc;
	}

	while (ring->rd_offset != ring->wr_offset) {
		rc = mhi_dev_process_ring_element(ring, ring->rd_offset);
		if (rc) {
			mhi_log(MHI_MSG_ERROR,
				"Error processing ring (%d) element (%d)\n",
				ring->id, ring->rd_offset);
			return rc;
		}

		mhi_log(MHI_MSG_VERBOSE,
			"Processing ring (%d) rd_offset:%d, wr_offset:%d\n",
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
				union mhi_dev_ring_element_type *element)
{
	uint32_t old_offset = 0;
	struct mhi_addr host_addr;

	if (!ring || !element) {
		pr_err("%s: Invalid context\n", __func__);
		return -EINVAL;
	}

	mhi_dev_update_wr_offset(ring);

	if ((ring->rd_offset + 1) % ring->ring_size == ring->wr_offset) {
		mhi_log(MHI_MSG_VERBOSE, "ring full to insert element\n");
		return -EINVAL;
	}

	old_offset = ring->rd_offset;

	mhi_dev_ring_inc_index(ring, ring->rd_offset);

	ring->ring_ctx->generic.rp = (ring->rd_offset *
				sizeof(union mhi_dev_ring_element_type)) +
				ring->ring_ctx->generic.rbase;
	/*
	 * Write the element, ring_base has to be the
	 * iomap of the ring_base for memcpy
	 */

	if (ring->mhi_dev->use_ipa)
		host_addr.host_pa = ring->ring_shadow.host_pa +
			sizeof(union mhi_dev_ring_element_type) * old_offset;
	else
		host_addr.device_va = ring->ring_shadow.device_va +
			sizeof(union mhi_dev_ring_element_type) * old_offset;

	host_addr.virt_addr = element;
	host_addr.size = sizeof(union mhi_dev_ring_element_type);

	mhi_log(MHI_MSG_VERBOSE, "adding element to ring (%d)\n", ring->id);
	mhi_log(MHI_MSG_VERBOSE, "rd_ofset %d\n", ring->rd_offset);
	mhi_log(MHI_MSG_VERBOSE, "type %d\n", element->generic.type);

	mhi_dev_write_to_host(ring->mhi_dev, &host_addr);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_add_element);

int mhi_ring_start(struct mhi_dev_ring *ring, union mhi_dev_ring_ctx *ctx,
							struct mhi_dev *mhi)
{
	int rc = 0;
	uint32_t wr_offset = 0;
	uint32_t offset = 0;

	if (!ring || !ctx || !mhi) {
		pr_err("%s: Invalid context\n", __func__);
		return -EINVAL;
	}

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

	ring->ring_cache = dma_alloc_coherent(mhi->dev,
			ring->ring_size *
			sizeof(union mhi_dev_ring_element_type),
			&ring->ring_cache_dma_handle,
			GFP_KERNEL);
	if (!ring->ring_cache)
		return -ENOMEM;

	offset = (uint32_t)(ring->ring_ctx->generic.rbase -
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

	if (ring->type != RING_TYPE_ER) {
		rc = mhi_dev_cache_ring(ring, wr_offset);
		if (rc)
			return rc;
	}

	mhi_log(MHI_MSG_VERBOSE, "ctx ring_base:0x%x, rp:0x%x, wp:0x%x\n",
			(uint32_t)ring->ring_ctx->generic.rbase,
			(uint32_t)ring->ring_ctx->generic.rp,
			(uint32_t)ring->ring_ctx->generic.wp);
	ring->wr_offset = wr_offset;

	return rc;
}
EXPORT_SYMBOL(mhi_ring_start);

void mhi_ring_init(struct mhi_dev_ring *ring, enum mhi_dev_ring_type type,
								int id)
{
	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return;
	}

	ring->id = id;
	ring->state = RING_STATE_UINT;
	ring->ring_cb = NULL;
	ring->type = type;
}
EXPORT_SYMBOL(mhi_ring_init);

void mhi_ring_set_cb(struct mhi_dev_ring *ring,
			void (*ring_cb)(struct mhi_dev *dev,
			union mhi_dev_ring_element_type *el, void *ctx))
{
	if (!ring || !ring_cb) {
		pr_err("%s: Invalid context\n", __func__);
		return;
	}

	ring->ring_cb = ring_cb;
}
EXPORT_SYMBOL(mhi_ring_set_cb);

void mhi_ring_set_state(struct mhi_dev_ring *ring,
				enum mhi_dev_ring_state state)
{
	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return;
	}

	if (state > RING_STATE_PENDING) {
		pr_err("%s: Invalid ring state\n", __func__);
		return;
	}

	ring->state = state;
}
EXPORT_SYMBOL(mhi_ring_set_state);

enum mhi_dev_ring_state mhi_ring_get_state(struct mhi_dev_ring *ring)
{
	if (!ring) {
		pr_err("%s: Invalid ring context\n", __func__);
		return -EINVAL;
	}

	return ring->state;
}
EXPORT_SYMBOL(mhi_ring_get_state);
