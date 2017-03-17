/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#include "mhi_hwio.h"
#include "mhi_macros.h"

#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/platform_device.h>

static int mhi_init_sync(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int i;

	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
		struct mhi_ring *ring = &mhi_dev_ctxt->mhi_local_chan_ctxt[i];

		mutex_init(&mhi_dev_ctxt->mhi_chan_cfg[i].chan_lock);
		spin_lock_init(&mhi_dev_ctxt->mhi_chan_cfg[i].event_lock);
		spin_lock_init(&ring->ring_lock);
	}

	for (i = 0; i < NR_OF_CMD_RINGS; i++) {
		struct mhi_ring *ring = &mhi_dev_ctxt->mhi_local_cmd_ctxt[i];

		spin_lock_init(&ring->ring_lock);
	}

	return 0;
}

size_t calculate_mhi_space(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	size_t mhi_dev_mem = 0;

	/* Calculate size needed for contexts */
	mhi_dev_mem += (MHI_MAX_CHANNELS * sizeof(struct mhi_chan_ctxt)) +
			(NR_OF_CMD_RINGS * sizeof(struct mhi_chan_ctxt)) +
			(mhi_dev_ctxt->mmio_info.nr_event_rings *
					  sizeof(struct mhi_event_ctxt));
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Reserved %zd bytes for context info\n", mhi_dev_mem);
	/*Calculate size needed for cmd TREs */
	mhi_dev_mem += (CMD_EL_PER_RING * sizeof(union mhi_cmd_pkt));
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Final bytes for MHI device space %zd\n", mhi_dev_mem);
	return mhi_dev_mem;
}

void init_dev_ev_ctxt(struct mhi_event_ctxt *ev_ctxt,
		 dma_addr_t p_base_addr, size_t len)
{
	ev_ctxt->mhi_event_ring_base_addr = p_base_addr;
	ev_ctxt->mhi_event_read_ptr = p_base_addr;
	ev_ctxt->mhi_event_write_ptr = p_base_addr;
	ev_ctxt->mhi_event_ring_len = len;
}

void init_local_ev_ctxt(struct mhi_ring *ev_ctxt,
		 void *v_base_addr, size_t len)
{
	ev_ctxt->base = v_base_addr;
	ev_ctxt->rp = v_base_addr;
	ev_ctxt->wp = v_base_addr;
	ev_ctxt->len = len;
	ev_ctxt->el_size = sizeof(union mhi_event_pkt);
	ev_ctxt->overwrite_en = 0;
}

void init_dev_chan_ctxt(struct mhi_chan_ctxt *chan_ctxt,
		 dma_addr_t p_base_addr, size_t len, int ev_index)
{
	chan_ctxt->mhi_trb_ring_base_addr = p_base_addr;
	chan_ctxt->mhi_trb_read_ptr = p_base_addr;
	chan_ctxt->mhi_trb_write_ptr = p_base_addr;
	chan_ctxt->mhi_trb_ring_len = len;
	/* Prepulate the channel ctxt */
	chan_ctxt->chstate = MHI_CHAN_STATE_ENABLED;
	chan_ctxt->mhi_event_ring_index = ev_index;
}

void init_local_chan_ctxt(struct mhi_ring *chan_ctxt,
		 void *v_base_addr, size_t len)
{
	chan_ctxt->base = v_base_addr;
	chan_ctxt->rp = v_base_addr;
	chan_ctxt->wp = v_base_addr;
	chan_ctxt->len = len;
	chan_ctxt->el_size = sizeof(union mhi_event_pkt);
	chan_ctxt->overwrite_en = 0;
}

/**
 * mhi_cmd_ring_init-  Initialization of the command ring
 *
 * @cmd_ctxt:			command ring context to initialize
 * @trb_list_phy_addr:		Pointer to the dma address of the tre ring
 * @trb_list_virt_addr:		Pointer to the virtual address of the tre ring
 * @ring_size:			Ring size
 * @ring:			Pointer to the shadow command context
 *
 * @Return errno
 */
static int mhi_cmd_ring_init(struct mhi_cmd_ctxt *cmd_ctxt,
				void *trb_list_virt_addr,
				dma_addr_t trb_list_phy_addr,
				size_t ring_size, struct mhi_ring *ring)
{
	cmd_ctxt->mhi_cmd_ring_base_addr = trb_list_phy_addr;
	cmd_ctxt->mhi_cmd_ring_read_ptr = trb_list_phy_addr;
	cmd_ctxt->mhi_cmd_ring_write_ptr = trb_list_phy_addr;
	cmd_ctxt->mhi_cmd_ring_len = ring_size;
	ring[PRIMARY_CMD_RING].wp = trb_list_virt_addr;
	ring[PRIMARY_CMD_RING].rp = trb_list_virt_addr;
	ring[PRIMARY_CMD_RING].base = trb_list_virt_addr;
	ring[PRIMARY_CMD_RING].len = ring_size;
	ring[PRIMARY_CMD_RING].el_size = sizeof(union mhi_cmd_pkt);
	ring[PRIMARY_CMD_RING].overwrite_en = 0;
	ring[PRIMARY_CMD_RING].db_mode.process_db =
		mhi_process_db_brstmode_disable;
	return 0;
}

int init_mhi_dev_mem(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	size_t mhi_mem_index = 0, ring_len;
	void *dev_mem_start;
	dma_addr_t dma_dev_mem_start;
	int i, r;

	mhi_dev_ctxt->dev_space.dev_mem_len =
					calculate_mhi_space(mhi_dev_ctxt);

	mhi_dev_ctxt->dev_space.dev_mem_start =
		dma_alloc_coherent(&mhi_dev_ctxt->plat_dev->dev,
				    mhi_dev_ctxt->dev_space.dev_mem_len,
				   &mhi_dev_ctxt->dev_space.dma_dev_mem_start,
				    GFP_KERNEL);
	if (!mhi_dev_ctxt->dev_space.dev_mem_start) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to allocate memory of size %zd bytes\n",
			mhi_dev_ctxt->dev_space.dev_mem_len);
		return -ENOMEM;
	}
	dev_mem_start = mhi_dev_ctxt->dev_space.dev_mem_start;
	dma_dev_mem_start = mhi_dev_ctxt->dev_space.dma_dev_mem_start;
	memset(dev_mem_start, 0, mhi_dev_ctxt->dev_space.dev_mem_len);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Starting Seg address: virt 0x%p, dma 0x%llx\n",
		dev_mem_start, (u64)dma_dev_mem_start);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Initializing CCABAP at dma 0x%llx\n",
		(u64)dma_dev_mem_start + mhi_mem_index);
	mhi_dev_ctxt->dev_space.ring_ctxt.cc_list = dev_mem_start;
	mhi_dev_ctxt->dev_space.ring_ctxt.dma_cc_list = dma_dev_mem_start;
	mhi_mem_index += MHI_MAX_CHANNELS * sizeof(struct mhi_chan_ctxt);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Initializing CRCBAP at dma 0x%llx\n",
		(u64)dma_dev_mem_start + mhi_mem_index);

	mhi_dev_ctxt->dev_space.ring_ctxt.cmd_ctxt =
						dev_mem_start + mhi_mem_index;
	mhi_dev_ctxt->dev_space.ring_ctxt.dma_cmd_ctxt =
					dma_dev_mem_start + mhi_mem_index;
	mhi_mem_index += NR_OF_CMD_RINGS * sizeof(struct mhi_chan_ctxt);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Initializing ECABAP at dma 0x%llx\n",
		(u64)dma_dev_mem_start + mhi_mem_index);
	mhi_dev_ctxt->dev_space.ring_ctxt.ec_list =
						dev_mem_start + mhi_mem_index;
	mhi_dev_ctxt->dev_space.ring_ctxt.dma_ec_list =
					dma_dev_mem_start + mhi_mem_index;
	mhi_mem_index += mhi_dev_ctxt->mmio_info.nr_event_rings *
					  sizeof(struct mhi_event_ctxt);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Initializing CMD context at dma 0x%llx\n",
		(u64)dma_dev_mem_start + mhi_mem_index);

	/* TODO: Initialize both the local and device cmd context */
	ring_len = (CMD_EL_PER_RING * sizeof(union mhi_cmd_pkt));
	mhi_cmd_ring_init(mhi_dev_ctxt->dev_space.ring_ctxt.cmd_ctxt,
			 dev_mem_start + mhi_mem_index,
			 dma_dev_mem_start + mhi_mem_index,
			 ring_len,
			 mhi_dev_ctxt->mhi_local_cmd_ctxt);
	mhi_mem_index += ring_len;

	/* Initialize both the local and device event contexts */
	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i) {
		dma_addr_t ring_dma_addr = 0;
		void *ring_addr = NULL;

		ring_len = sizeof(union mhi_event_pkt) *
					mhi_dev_ctxt->ev_ring_props[i].nr_desc;
		ring_addr = dma_alloc_coherent(
				&mhi_dev_ctxt->plat_dev->dev,
				ring_len, &ring_dma_addr, GFP_KERNEL);
		if (!ring_addr)
			goto err_ev_alloc;
		init_dev_ev_ctxt(&mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[i],
				ring_dma_addr, ring_len);
		init_local_ev_ctxt(&mhi_dev_ctxt->mhi_local_event_ctxt[i],
				ring_addr, ring_len);
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Initializing EV_%d TRE list at virt 0x%p dma 0x%llx\n",
			i, ring_addr, (u64)ring_dma_addr);
	}
	return 0;

err_ev_alloc:
	for (; i >= 0; --i) {
		struct mhi_event_ctxt *dev_ev_ctxt = NULL;
		struct mhi_ring *ev_ctxt = NULL;

		dev_ev_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[i];
		ev_ctxt = &mhi_dev_ctxt->mhi_local_event_ctxt[i];

		dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
				  ev_ctxt->len,
				  ev_ctxt->base,
				  dev_ev_ctxt->mhi_event_ring_base_addr);
	}
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
			   mhi_dev_ctxt->dev_space.dev_mem_len,
			   mhi_dev_ctxt->dev_space.dev_mem_start,
			   mhi_dev_ctxt->dev_space.dma_dev_mem_start);
	return r;
}

static int mhi_init_events(struct mhi_device_ctxt *mhi_dev_ctxt)
{

	/* Initialize the event which signals M0 */
	mhi_dev_ctxt->mhi_ev_wq.m0_event = kmalloc(sizeof(wait_queue_head_t),
								GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.m0_event) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR, "Failed to init event");
		goto error_state_change_event_handle;
	}
	/* Initialize the event which signals M0 */
	mhi_dev_ctxt->mhi_ev_wq.m3_event = kmalloc(sizeof(wait_queue_head_t),
								GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.m3_event) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR, "Failed to init event");
		goto error_m0_event;
	}
	/* Initialize the event which signals M0 */
	mhi_dev_ctxt->mhi_ev_wq.bhi_event = kmalloc(sizeof(wait_queue_head_t),
								GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.bhi_event) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR, "Failed to init event");
		goto error_bhi_event;
	}

	/* Initialize the event which triggers clients waiting to send */
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	/* Initialize the event which triggers D3hot */
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.bhi_event);

	return 0;
error_bhi_event:
	kfree(mhi_dev_ctxt->mhi_ev_wq.m3_event);
error_m0_event:
	kfree(mhi_dev_ctxt->mhi_ev_wq.m0_event);
error_state_change_event_handle:
	return -ENOMEM;
}

static int mhi_init_state_change_thread_work_queue(
			struct mhi_state_work_queue *q)
{
	bool lock_acquired = 0;
	unsigned long flags;

	if (NULL == q->q_lock) {
		q->q_lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (NULL == q->q_lock)
			return -ENOMEM;
		spin_lock_init(q->q_lock);
	} else {
		spin_lock_irqsave(q->q_lock, flags);
		lock_acquired = 1;
	}
	q->queue_full_cntr = 0;
	q->q_info.base = q->buf;
	q->q_info.rp = q->buf;
	q->q_info.wp = q->buf;
	q->q_info.len = MHI_WORK_Q_MAX_SIZE * sizeof(enum STATE_TRANSITION);
	q->q_info.el_size = sizeof(enum STATE_TRANSITION);
	q->q_info.overwrite_en = 0;
	if (lock_acquired)
		spin_unlock_irqrestore(q->q_lock, flags);

	return 0;
}

static void mhi_init_wakelock(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	wakeup_source_init(&mhi_dev_ctxt->w_lock, "mhi_wakeup_source");
}

/**
 * @brief Main initialization function for a mhi struct device context
 *	 All threads, events mutexes, mhi specific data structures
 *	 are initialized here
 *
 * @param mhi_struct device [IN/OUT] reference to a mhi context to be populated
 *
 * @return errno
 */
int mhi_init_device_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Entered\n");

	r = mhi_populate_event_cfg(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to get event ring properties ret %d\n", r);
		goto error_during_props;
	}
	r = mhi_init_sync(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to initialize mhi sync\n");
		goto error_during_sync;
	}
	r = create_local_ev_ctxt(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to initialize local event ctxt ret %d\n", r);
		goto error_during_local_ev_ctxt;
	}
	r = init_mhi_dev_mem(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to initialize device memory ret %d\n", r);
		goto error_during_dev_mem_init;
	}
	r = mhi_init_events(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to initialize mhi events ret %d\n", r);
		goto error_wq_init;
	}
	r = mhi_reset_all_thread_queues(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to initialize work queues ret %d\n", r);
		goto error_during_thread_init;
	}
	init_event_ctxt_array(mhi_dev_ctxt);
	mhi_dev_ctxt->mhi_state = MHI_STATE_RESET;

	mhi_init_wakelock(mhi_dev_ctxt);

	return r;

error_during_thread_init:
	kfree(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.bhi_event);
error_wq_init:
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
		   mhi_dev_ctxt->dev_space.dev_mem_len,
		   mhi_dev_ctxt->dev_space.dev_mem_start,
		   mhi_dev_ctxt->dev_space.dma_dev_mem_start);
error_during_dev_mem_init:
error_during_local_ev_ctxt:
error_during_sync:
	kfree(mhi_dev_ctxt->ev_ring_props);
error_during_props:
	return r;
}

/**
 * @brief Initialize the channel context and shadow context
 *
 * @cc_list: Context to initialize
 * @trb_list_phy: Physical base address for the TRE ring
 * @trb_list_virt: Virtual base address for the TRE ring
 * @el_per_ring: Number of TREs this ring will contain
 * @chan_type: Type of channel IN/OUT
 * @event_ring: Event ring to be mapped to this channel context
 * @ring: Shadow context to be initialized alongside
 * @chan_state: Channel state
 * @preserve_db_state: Do not reset DB state during resume
 * @Return errno
 */
int mhi_init_chan_ctxt(struct mhi_chan_ctxt *cc_list,
		       uintptr_t trb_list_phy, uintptr_t trb_list_virt,
		       u64 el_per_ring, enum MHI_CHAN_DIR chan_type,
		       u32 event_ring, struct mhi_ring *ring,
		       enum MHI_CHAN_STATE chan_state,
		       bool preserve_db_state,
		       enum MHI_BRSTMODE brstmode)
{
	cc_list->brstmode = brstmode;
	cc_list->chstate = chan_state;
	cc_list->chtype = chan_type;
	cc_list->mhi_event_ring_index = event_ring;
	cc_list->mhi_trb_ring_base_addr = trb_list_phy;
	cc_list->mhi_trb_ring_len =
		((size_t)(el_per_ring)*sizeof(struct mhi_tx_pkt));
	cc_list->mhi_trb_read_ptr = trb_list_phy;
	cc_list->mhi_trb_write_ptr = trb_list_phy;
	ring->rp = (void *)(trb_list_virt);
	ring->ack_rp = ring->rp;
	ring->wp = (void *)(trb_list_virt);
	ring->base = (void *)(trb_list_virt);
	ring->len = ((size_t)(el_per_ring)*sizeof(struct mhi_tx_pkt));
	ring->el_size = sizeof(struct mhi_tx_pkt);
	ring->overwrite_en = 0;
	ring->dir = chan_type;
	ring->ch_state = MHI_CHAN_STATE_DISABLED;
	ring->db_mode.db_mode = 1;
	ring->db_mode.preserve_db_state = (preserve_db_state) ? 1 : 0;
	ring->db_mode.brstmode = brstmode;

	switch (ring->db_mode.brstmode) {
	case MHI_BRSTMODE_ENABLE:
		ring->db_mode.process_db = mhi_process_db_brstmode;
		break;
	case MHI_BRSTMODE_DISABLE:
		ring->db_mode.process_db = mhi_process_db_brstmode_disable;
		break;
	default:
		ring->db_mode.process_db = mhi_process_db;
	}

	/* Flush writes to MMIO */
	wmb();
	return 0;
}

int mhi_reset_all_thread_queues(
		struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val = 0;

	ret_val = mhi_init_state_change_thread_work_queue(
				&mhi_dev_ctxt->state_change_work_item_list);
	if (ret_val)
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to reset STT work queue\n");
	return ret_val;
}

int mhi_reg_notifiers(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 ret_val;

	if (NULL == mhi_dev_ctxt)
		return -EINVAL;
	mhi_dev_ctxt->mhi_cpu_notifier.notifier_call = mhi_cpu_notifier_cb;
	ret_val = register_cpu_notifier(&mhi_dev_ctxt->mhi_cpu_notifier);
	return ret_val;
}
