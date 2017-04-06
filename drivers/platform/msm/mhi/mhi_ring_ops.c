/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mhi_sys.h"
#include "mhi.h"

static int add_element(struct mhi_ring *ring, void **rp,
			void **wp, void **assigned_addr)
{
	uintptr_t d_wp = 0, d_rp = 0, ring_size = 0;
	int r;

	if (NULL == ring || 0 == ring->el_size
		|| NULL == ring->base || 0 == ring->len) {
		return -EINVAL;
	}

	r = get_element_index(ring, *rp, &d_rp);
	if (r)
		return r;
	r = get_element_index(ring, *wp, &d_wp);

	if (r)
		return r;

	ring_size = ring->len / ring->el_size;

	if ((d_wp + 1) % ring_size == d_rp) {
		if (ring->overwrite_en) {
			ctxt_del_element(ring, NULL);
		} else {
			return -ENOSPC;
		}
	}
	if (NULL != assigned_addr)
		*assigned_addr = (char *)ring->wp;
	*wp = (void *)(((d_wp + 1) % ring_size) * ring->el_size +
						(uintptr_t)ring->base);

	/* force update visible to other cores */
	smp_wmb();
	return 0;
}

inline int ctxt_add_element(struct mhi_ring *ring,
						void **assigned_addr)
{
	return add_element(ring, &ring->rp, &ring->wp, assigned_addr);
}
inline int ctxt_del_element(struct mhi_ring *ring,
						void **assigned_addr)
{
	return delete_element(ring, &ring->rp, &ring->wp, assigned_addr);
}

/**
 * delete_element - Moves the read pointer of the transfer ring to
 * the next element of the transfer ring,
 *
 * ring location of local ring data structure
 * @rp ring read pointer
 * @wp ring write pointer
 * @assigned_addr location of the element just deleted
 */
int delete_element(struct mhi_ring *ring, void **rp,
			void **wp, void **assigned_addr)
{
	uintptr_t d_wp = 0, d_rp = 0, ring_size = 0;
	int r;

	if (NULL == ring || 0 == ring->el_size ||
		NULL == ring->base || 0 == ring->len)
		return -EINVAL;

	ring_size = ring->len / ring->el_size;
	r = get_element_index(ring, *rp, &d_rp);
	if (r)
		return r;
	r = get_element_index(ring, *wp, &d_wp);
	if (r)
		return r;
	if (d_wp == d_rp) {
		if (NULL != assigned_addr)
			*assigned_addr = NULL;
		return -ENODATA;
	}

	if (NULL != assigned_addr)
		*assigned_addr = (void *)ring->rp;

	*rp = (void *)(((d_rp + 1) % ring_size) * ring->el_size +
						(uintptr_t)ring->base);

	/* force update visible to other cores */
	smp_wmb();
	return 0;
}

int mhi_get_free_desc(struct mhi_client_handle *client_handle)
{
	u32 chan;
	struct mhi_client_config *client_config;
	struct mhi_device_ctxt *ctxt;
	int bb_ring, ch_ring;

	if (!client_handle)
		return -EINVAL;
	client_config = client_handle->client_config;
	ctxt = client_config->mhi_dev_ctxt;
	chan = client_config->chan_info.chan_nr;

	bb_ring = get_nr_avail_ring_elements(ctxt, &ctxt->chan_bb_list[chan]);
	ch_ring = get_nr_avail_ring_elements(ctxt,
					     &ctxt->mhi_local_chan_ctxt[chan]);

	return min(bb_ring, ch_ring);
}
EXPORT_SYMBOL(mhi_get_free_desc);

int get_nr_avail_ring_elements(struct mhi_device_ctxt *mhi_dev_ctxt,
			       struct mhi_ring *ring)
{
	u32 nr_el = 0;
	uintptr_t ring_size = 0;
	int ret_val = 0;

	ring_size = ring->len / ring->el_size;
	ret_val = get_nr_enclosed_el(ring, ring->rp, ring->wp, &nr_el);
	if (ret_val != 0) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to get enclosed el ret %d.\n", ret_val);
		return 0;
	}
	return ring_size - nr_el - 1;
}

int get_nr_enclosed_el(struct mhi_ring *ring, void *rp,
						void *wp, u32 *nr_el)
{
	uintptr_t index_rp = 0;
	uintptr_t index_wp = 0;
	uintptr_t ring_size = 0;
	int r = 0;

	if (NULL == ring || 0 == ring->el_size ||
		NULL == ring->base || 0 == ring->len) {
		return -EINVAL;
	}
	r = get_element_index(ring, rp, &index_rp);
	if (r)
		return r;
	r = get_element_index(ring, wp, &index_wp);
	if (r)
		return r;
	ring_size = ring->len / ring->el_size;

	if (index_rp < index_wp)
		*nr_el = index_wp - index_rp;
	else if (index_rp > index_wp)
		*nr_el = ring_size - (index_rp - index_wp);
	else
		*nr_el = 0;
	return 0;
}

int get_element_index(struct mhi_ring *ring,
				void *address, uintptr_t *index)
{
	int r = validate_ring_el_addr(ring, (uintptr_t)address);

	if (r)
		return r;
	*index = ((uintptr_t)address - (uintptr_t)ring->base) / ring->el_size;
	return r;
}

int get_element_addr(struct mhi_ring *ring,
				uintptr_t index, void **address)
{
	uintptr_t ring_size = 0;

	if (NULL == ring || NULL == address)
		return -EINVAL;
	ring_size = ring->len / ring->el_size;
	*address = (void *)((uintptr_t)ring->base +
			(index % ring_size) * ring->el_size);
	return 0;
}
