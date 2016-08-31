/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_ipc.h"
#include "nvshm_iobuf.h"
#include "nvshm_queue.h"
/*
 * really simple allocator: data is divided of chunk of equal size
 *  since iobuf are mainly for tty/net traffic which is well below 8k
 */

#define NVSHM_DEFAULT_OFFSET 32
#define NVSHM_MAX_FREE_PENDING (16)

struct nvshm_allocator {
	spinlock_t lock;
	/* AP free iobufs */
	struct nvshm_iobuf *free_pool_head;
	struct nvshm_iobuf *free_pool_tail;
	/* Freed BBC iobuf to be returned */
	struct nvshm_iobuf *bbc_pool_head;
	struct nvshm_iobuf *bbc_pool_tail;
	int nbuf;
	int free_count;
};

static struct nvshm_allocator alloc;

static const char *give_pointer_location(struct nvshm_handle *handle, void *ptr)
{
	if (!ptr)
		return "null";

	ptr = NVSHM_B2A(handle, ptr);

	if (ADDR_OUTSIDE(ptr, handle->desc_base_virt, handle->desc_size)
	    && ADDR_OUTSIDE(ptr, handle->data_base_virt, handle->data_size)) {
		if (ADDR_OUTSIDE(ptr, handle->ipc_base_virt, handle->ipc_size))
			return "Err";
		else
			return "BBC";
	}

	return "AP";
}

/* Accumulate BBC freed iobuf to return them later at end of rx processing */
/* This saves a lot of CPU/memory cycles on both sides */
static void bbc_free(struct nvshm_handle *handle, struct nvshm_iobuf *iob)
{
	unsigned long f;

	spin_lock_irqsave(&alloc.lock, f);
	alloc.free_count++;
	if (alloc.bbc_pool_head) {
		alloc.bbc_pool_tail->next = NVSHM_A2B(handle, iob);
		alloc.bbc_pool_tail = iob;
	} else {
		alloc.bbc_pool_head = alloc.bbc_pool_tail = iob;
	}
	spin_unlock_irqrestore(&alloc.lock, f);
	if (alloc.free_count > NVSHM_MAX_FREE_PENDING)
		nvshm_iobuf_bbc_free(handle);
}

/* Effectively free all iobufs accumulated */
void nvshm_iobuf_bbc_free(struct nvshm_handle *handle)
{
	struct nvshm_iobuf *iob = NULL;
	unsigned long f;

	spin_lock_irqsave(&alloc.lock, f);
	if (alloc.bbc_pool_head) {
		alloc.free_count = 0;
		iob = alloc.bbc_pool_head;
		alloc.bbc_pool_head = alloc.bbc_pool_tail = NULL;
	}
	spin_unlock_irqrestore(&alloc.lock, f);
	if (iob) {
		nvshm_queue_put(handle, iob);
		nvshm_generate_ipc(handle);
	}
}

struct nvshm_iobuf *nvshm_iobuf_alloc(struct nvshm_channel *chan, int size)
{
	struct nvshm_handle *handle = nvshm_get_handle();
	struct nvshm_iobuf *desc = NULL;
	unsigned long f;

	spin_lock_irqsave(&alloc.lock, f);
	if (alloc.free_pool_head) {
		int check = nvshm_iobuf_check(alloc.free_pool_head);

		if (check) {
			spin_unlock_irqrestore(&alloc.lock, f);
			pr_err("%s: iobuf check ret %d\n", __func__, check);
			return NULL;
		}
		if (size > (alloc.free_pool_head->total_length -
			    NVSHM_DEFAULT_OFFSET)) {
			spin_unlock_irqrestore(&alloc.lock, f);
			pr_err("%s: requested size (%d > %d) too big\n",
			       __func__,
			       size,
			       alloc.free_pool_head->total_length -
			       NVSHM_DEFAULT_OFFSET);
			if (chan->ops) {
				chan->ops->error_event(chan,
						       NVSHM_IOBUF_ERROR);
			}
			return desc;
		}
		desc = alloc.free_pool_head;
		alloc.free_pool_head = desc->next;
		if (alloc.free_pool_head) {
			alloc.free_pool_head = NVSHM_B2A(handle,
							 alloc.free_pool_head);
		} else {
			pr_debug("%s end of alloc queue - clearing tail\n",
				__func__);
			alloc.free_pool_tail = NULL;
		}
		desc->length = 0;
		desc->flags = 0;
		desc->data_offset = NVSHM_DEFAULT_OFFSET;
		desc->sg_next = NULL;
		desc->next = NULL;
		desc->ref = 1;

	} else {
		spin_unlock_irqrestore(&alloc.lock, f);
		pr_err("%s: no more alloc space\n", __func__);
		/* No error since it's only Xoff situation */
		return desc;
	}

	spin_unlock_irqrestore(&alloc.lock, f);

	return desc;
}

/** Returned iobuf are already freed - just process them */
void nvshm_iobuf_process_freed(struct nvshm_iobuf *desc)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	unsigned long f;

	while (desc) {
		int callback = 0, chan;
		struct nvshm_iobuf *next = desc->next;

		if (desc->ref != 0) {
			pr_err("%s: BBC returned an non freed iobuf (0x%x)\n",
			       __func__,
			       (unsigned int)desc);
			return;
		}

		chan = desc->chan;
		spin_lock_irqsave(&alloc.lock, f);
		/* update rate counter */
		if ((chan >= 0) &&
		    (chan < NVSHM_MAX_CHANNELS)) {
			if ((priv->chan[chan].rate_counter++ ==
			     NVSHM_RATE_LIMIT_TRESHOLD)
			    && (priv->chan[chan].xoff)) {
				priv->chan[chan].xoff = 0;
				callback = 1;
			}
		}
		desc->sg_next = NULL;
		desc->next = NULL;
		desc->length = 0;
		desc->flags = 0;
		desc->data_offset = 0;
		desc->chan = 0;
		if (alloc.free_pool_tail) {
			alloc.free_pool_tail->next = NVSHM_A2B(priv,
							       desc);
			alloc.free_pool_tail = desc;
		} else {
			alloc.free_pool_head = desc;
				alloc.free_pool_tail = desc;
		}
		spin_unlock_irqrestore(&alloc.lock, f);
		if (callback)
			nvshm_start_tx(&priv->chan[chan]);
		if (next) {
			desc = NVSHM_B2A(priv, next);
		} else {
			desc = next;
		}
	}
}

/** Single iobuf free - do not follow iobuf links */
void nvshm_iobuf_free(struct nvshm_iobuf *desc)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	int callback = 0, chan;
	unsigned long f;

	if (desc->ref == 0) {
		pr_err("%s: freeing an already freed iobuf (0x%x)\n",
		       __func__,
		       (unsigned int)desc);
		return;
	}
	spin_lock_irqsave(&alloc.lock, f);
	pr_debug("%s: free 0x%p ref %d pool %x\n", __func__,
		 desc, desc->ref, desc->pool_id);
	desc->ref--;
	chan = desc->chan;
	if (desc->ref == 0) {
		if (desc->pool_id >= NVSHM_AP_POOL_ID) {
			/* update rate counter */
			if ((chan >= 0) &&
			    (chan < NVSHM_MAX_CHANNELS)) {
				if ((priv->chan[chan].rate_counter++ ==
				     NVSHM_RATE_LIMIT_TRESHOLD)
				    && (priv->chan[chan].xoff)) {
					priv->chan[chan].xoff = 0;
					callback = 1;
				}
			}
			desc->sg_next = NULL;
			desc->next = NULL;
			desc->length = 0;
			desc->flags = 0;
			desc->data_offset = 0;
			desc->chan = 0;
			if (alloc.free_pool_tail) {
				alloc.free_pool_tail->next = NVSHM_A2B(priv,
								       desc);
				alloc.free_pool_tail = desc;
			} else {
				alloc.free_pool_head = desc;
				alloc.free_pool_tail = desc;
			}
		} else {
			/* iobuf belongs to other side */
			pr_debug("%s: re-queue freed buffer\n", __func__);
			desc->sg_next = NULL;
			desc->next = NULL;
			desc->length = 0;
			desc->data_offset = 0;
			spin_unlock_irqrestore(&alloc.lock, f);
			bbc_free(priv, desc);
			return;
		}
	}
	spin_unlock_irqrestore(&alloc.lock, f);
	if (callback)
		nvshm_start_tx(&priv->chan[chan]);
}

void nvshm_iobuf_free_cluster(struct nvshm_iobuf *list)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	struct nvshm_iobuf *_phy_list, *_to_free, *leaf;
	int n = 0;

	_phy_list = list;
	while (_phy_list) {
		_to_free = list;
		if (list->sg_next) {
			_phy_list = list->sg_next;
			if (_phy_list) {
				leaf = NVSHM_B2A(priv, _phy_list);
				leaf->next = list->next;
			}
		} else {
			_phy_list = list->next;
		}
		list = NVSHM_B2A(priv, _phy_list);
		n++;
		nvshm_iobuf_free(_to_free);
	}
}

int nvshm_iobuf_ref(struct nvshm_iobuf *iob)
{
	int ref;
	unsigned long f;

	spin_lock_irqsave(&alloc.lock, f);
	ref = iob->ref++;
	spin_unlock_irqrestore(&alloc.lock, f);
	return ref;
}

int nvshm_iobuf_unref(struct nvshm_iobuf *iob)
{
	int ref;
	unsigned long f;

	spin_lock_irqsave(&alloc.lock, f);
	ref = iob->ref--;
	spin_unlock_irqrestore(&alloc.lock, f);
	return ref;
}

int nvshm_iobuf_ref_cluster(struct nvshm_iobuf *iob)
{
	int ref, ret = 0;
	struct nvshm_iobuf *_phy_list, *_phy_leaf;
	struct nvshm_handle *handle = nvshm_get_handle();

	_phy_list = iob;
	while (_phy_list) {
		_phy_leaf = _phy_list;
		while (_phy_leaf) {
			ref = nvshm_iobuf_ref(_phy_leaf);
			ret = (ref > ret) ? ref : ret;
			if (_phy_leaf->sg_next) {
				_phy_leaf = NVSHM_B2A(handle,
						      _phy_leaf->sg_next);
			} else {
				_phy_leaf = NULL;
			}
		}
		if (_phy_list->next)
			_phy_list = NVSHM_B2A(handle, _phy_list->next);
		else
			_phy_list = NULL;
	}
	return ret;
}

int nvshm_iobuf_unref_cluster(struct nvshm_iobuf *iob)
{
	int ref, ret = 0;
	struct nvshm_iobuf *_phy_list, *_phy_leaf;
	struct nvshm_handle *handle = nvshm_get_handle();

	_phy_list = iob;
	while (_phy_list) {
		_phy_leaf = _phy_list;
		while (_phy_leaf) {
			ref = nvshm_iobuf_unref(_phy_leaf);
			ret = (ref > ret) ? ref : ret;
			if (_phy_leaf->sg_next) {
				_phy_leaf = NVSHM_B2A(handle,
						      _phy_leaf->sg_next);
			} else {
				_phy_leaf = NULL;
			}
		}
		if (_phy_list->next)
			_phy_list = NVSHM_B2A(handle, _phy_list->next);
		else
			_phy_list = NULL;
	}

	return ret;
}

int nvshm_iobuf_flags(struct nvshm_iobuf *iob,
		      unsigned int set,
		      unsigned int clear)
{
	iob->flags &= ~(clear & 0xFFFF);
	iob->flags |= set & 0xFFFF;
	return 0;
}

void nvshm_iobuf_dump(struct nvshm_iobuf *iob)
{
	struct nvshm_handle *priv = nvshm_get_handle();

	pr_err("iobuf (0x%p) dump:\n", NVSHM_A2B(priv, iob));
	pr_err("\t data      = 0x%p (%s)\n", iob->npdu_data,
	       give_pointer_location(priv, iob->npdu_data));
	pr_err("\t length    = %d\n", iob->length);
	pr_err("\t offset    = %d\n", iob->data_offset);
	pr_err("\t total_len = %d\n", iob->total_length);
	pr_err("\t ref       = %d\n", iob->ref);
	pr_err("\t pool_id   = %d (%s)\n", iob->pool_id,
	       (iob->pool_id < NVSHM_AP_POOL_ID) ? "BBC" : "AP");
	pr_err("\t next      = 0x%p (%s)\n", iob->next,
	       give_pointer_location(priv, iob->next));
	pr_err("\t sg_next   = 0x%p (%s)\n", iob->sg_next,
	       give_pointer_location(priv, iob->sg_next));
	pr_err("\t flags     = 0x%x\n", iob->flags);
	pr_err("\t _size     = %d\n", iob->_size);
	pr_err("\t _handle   = 0x%p\n", iob->_handle);
	pr_err("\t _reserved = 0x%x\n", iob->_reserved);
	pr_err("\t qnext     = 0x%p (%s)\n", iob->qnext,
	       give_pointer_location(priv, iob->qnext));
	pr_err("\t chan      = 0x%x\n", iob->chan);
	pr_err("\t qflags    = 0x%x\n", iob->qflags);
}

int nvshm_iobuf_check(struct nvshm_iobuf *iob)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	struct nvshm_iobuf *bbiob;
	int ret = 0;

	/* Check iobuf is in IPC space */
	if (ADDR_OUTSIDE(iob, priv->ipc_base_virt, priv->ipc_size)) {
		pr_err("%s: iob @ check failed 0x%lx\n",
		       __func__,
		       (long)iob);
		return -1;
	}

	bbiob = NVSHM_A2B(priv, iob);

	if (ADDR_OUTSIDE(iob->npdu_data, NVSHM_IPC_BB_BASE, priv->ipc_size)) {
		pr_err("%s 0x%lx: npduData @ check failed 0x%lx\n",
		       __func__,
		       (long)bbiob,
		       (long)iob->npdu_data);
		ret = -2;
		goto dump;
	}
	if (ADDR_OUTSIDE(iob->npdu_data + iob->data_offset,
			NVSHM_IPC_BB_BASE, priv->ipc_size)) {
		pr_err("%s 0x%lx: npduData + offset @ check failed 0x%lx/0x%lx\n",
		       __func__, (long)bbiob,
		       (long)iob->npdu_data, (long)iob->data_offset);
		ret = -3;
		goto dump;
	}
	if (iob->next) {
		if (ADDR_OUTSIDE(iob->next,
				NVSHM_IPC_BB_BASE, priv->ipc_size)) {
			pr_err("%s 0x%lx: next @ check failed 0x%lx\n",
			       __func__,
			       (long)bbiob,
			       (long)iob->next);
			ret = -4;
			goto dump;
		}
	}
	if (iob->sg_next) {
		if (ADDR_OUTSIDE(iob->sg_next,
				NVSHM_IPC_BB_BASE, priv->ipc_size)) {
			pr_err("%s 0x%lx:sg_next @ check failed 0x%lx\n",
			       __func__, (long)bbiob, (long)iob->sg_next);
			ret = -5;
			goto dump;
		}
	}
	if (iob->qnext) {
		if (ADDR_OUTSIDE(iob->qnext,
				NVSHM_IPC_BB_BASE, priv->ipc_size)) {
			pr_err("%s 0x%lx:qnext @ check failed 0x%lx\n",
			       __func__, (long)bbiob, (long)iob->qnext);
			ret = -6;
			goto dump;
		}
	}

	return ret;
dump:
	nvshm_iobuf_dump(iob);
	return ret;
}

int nvshm_iobuf_init(struct nvshm_handle *handle)
{
	struct nvshm_iobuf *iob;
	int ndesc, desc, datasize;
	unsigned char *dataptr;

	pr_debug("%s instance %d\n", __func__, handle->instance);

	spin_lock_init(&alloc.lock);
	/* Clear BBC free list */
	alloc.bbc_pool_head = alloc.bbc_pool_tail = NULL;
	alloc.free_count = 0;
	ndesc = handle->desc_size / sizeof(struct nvshm_iobuf) ;
	alloc.nbuf = ndesc;
	alloc.free_count = 0;
	datasize =  handle->data_size / ndesc;
	spin_lock(&alloc.lock);
	if (handle->shared_queue_tail != handle->desc_base_virt) {
		pr_err("%s initial tail != desc_base_virt not supported yet\n",
		       __func__);
	}
	iob = (struct nvshm_iobuf *)handle->desc_base_virt;

	dataptr = handle->data_base_virt;
	/* Invalidate all data region */
	INV_CPU_DCACHE(dataptr, handle->data_size);
	/* Clear all desc region */
	memset(handle->desc_base_virt, 0, handle->desc_size);
	/* Dummy queue element */
	iob->npdu_data = NVSHM_A2B(handle, dataptr);
	dataptr += datasize;
	iob->data_offset = NVSHM_DEFAULT_OFFSET;
	iob->total_length = datasize;
	iob->chan = -1;
	iob->next = NULL;
	iob->pool_id = NVSHM_AP_POOL_ID;
	iob->ref = 1;
	alloc.free_pool_head = ++iob;
	for (desc = 1; desc < (ndesc-1); desc++) {
		iob->npdu_data = NVSHM_A2B(handle, dataptr);
		dataptr += datasize;
		iob->data_offset = NVSHM_DEFAULT_OFFSET;
		iob->total_length = datasize;
		iob->next = NVSHM_A2B(handle, (void *)iob +
				      sizeof(struct nvshm_iobuf));
		iob->pool_id = NVSHM_AP_POOL_ID;
		iob++;
	}
	/* Untied last */
	iob->npdu_data = NVSHM_A2B(handle, dataptr);
	iob->data_offset = NVSHM_DEFAULT_OFFSET;
	iob->total_length = datasize;
	iob->pool_id = NVSHM_AP_POOL_ID;
	iob->next = NULL;

	alloc.free_pool_tail = iob;
	/* Flush all descriptor region */
	FLUSH_CPU_DCACHE(handle->desc_base_virt,
			 (long)handle->desc_size);
	spin_unlock(&alloc.lock);
	return 0;
}
