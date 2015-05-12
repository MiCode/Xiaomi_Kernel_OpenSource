/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/completion.h>

enum MHI_STATUS mhi_clean_init_stage(struct mhi_device_ctxt *mhi_dev_ctxt,
		enum MHI_INIT_ERROR_STAGE cleanup_stage)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	switch (cleanup_stage) {
	case MHI_INIT_ERROR_STAGE_UNWIND_ALL:
	case MHI_INIT_ERROR_TIMERS:
	case MHI_INIT_ERROR_STAGE_DEVICE_CTRL:
		mhi_freememregion(mhi_dev_ctxt->mhi_ctrl_seg_info);
	case MHI_INIT_ERROR_STAGE_THREAD_QUEUES:
	case MHI_INIT_ERROR_STAGE_THREADS:
		kfree(mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq);
		kfree(mhi_dev_ctxt->mhi_ev_wq.state_change_event);
		kfree(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	case MHI_INIT_ERROR_STAGE_EVENTS:
		kfree(mhi_dev_ctxt->mhi_ctrl_seg_info);
	case MHI_INIT_ERROR_STAGE_MEM_ZONES:
		kfree(mhi_dev_ctxt->mhi_cmd_mutex_list);
		kfree(mhi_dev_ctxt->mhi_chan_mutex);
		kfree(mhi_dev_ctxt->mhi_ev_spinlock_list);
	case MHI_INIT_ERROR_STAGE_SYNC:
		kfree(mhi_dev_ctxt->ev_ring_props);
		break;
	default:
		ret_val = MHI_STATUS_ERROR;
		break;
	}
	return ret_val;
}

static enum MHI_STATUS mhi_init_sync(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 i = 0;

	mhi_dev_ctxt->mhi_ev_spinlock_list = kmalloc(sizeof(spinlock_t) *
					mhi_dev_ctxt->mmio_info.nr_event_rings,
					GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_spinlock_list)
		goto ev_mutex_free;
	mhi_dev_ctxt->mhi_chan_mutex = kmalloc(sizeof(struct mutex) *
						MHI_MAX_CHANNELS, GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_chan_mutex)
		goto chan_mutex_free;
	mhi_dev_ctxt->mhi_cmd_mutex_list = kmalloc(sizeof(struct mutex) *
						NR_OF_CMD_RINGS, GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_cmd_mutex_list)
		goto cmd_mutex_free;

	mhi_dev_ctxt->db_write_lock = kmalloc(sizeof(spinlock_t) *
						MHI_MAX_CHANNELS, GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->db_write_lock)
		goto db_write_lock_free;
	for (i = 0; i < MHI_MAX_CHANNELS; ++i)
		mutex_init(&mhi_dev_ctxt->mhi_chan_mutex[i]);
	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i)
		spin_lock_init(&mhi_dev_ctxt->mhi_ev_spinlock_list[i]);
	for (i = 0; i < NR_OF_CMD_RINGS; ++i)
		mutex_init(&mhi_dev_ctxt->mhi_cmd_mutex_list[i]);
	for (i = 0; i < MHI_MAX_CHANNELS; ++i)
		spin_lock_init(&mhi_dev_ctxt->db_write_lock[i]);
	rwlock_init(&mhi_dev_ctxt->xfer_lock);
	mutex_init(&mhi_dev_ctxt->mhi_link_state);
	mutex_init(&mhi_dev_ctxt->pm_lock);
	atomic_set(&mhi_dev_ctxt->flags.m2_transition, 0);
	return MHI_STATUS_SUCCESS;

db_write_lock_free:
	kfree(mhi_dev_ctxt->mhi_cmd_mutex_list);
cmd_mutex_free:
	kfree(mhi_dev_ctxt->mhi_chan_mutex);
chan_mutex_free:
	kfree(mhi_dev_ctxt->mhi_ev_spinlock_list);
ev_mutex_free:
	return MHI_STATUS_ALLOC_ERROR;
}

static enum MHI_STATUS mhi_init_ctrl_zone(struct mhi_pcie_dev_info *dev_info,
				struct mhi_device_ctxt *mhi_dev_ctxt)
{
	mhi_dev_ctxt->mhi_ctrl_seg_info = kmalloc(sizeof(struct mhi_meminfo),
							GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ctrl_seg_info)
		return MHI_STATUS_ALLOC_ERROR;
	mhi_dev_ctxt->mhi_ctrl_seg_info->dev = &dev_info->pcie_device->dev;
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_init_events(struct mhi_device_ctxt *mhi_dev_ctxt)
{

	mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq = kmalloc(
						sizeof(wait_queue_head_t),
						GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq) {
		mhi_log(MHI_MSG_ERROR, "Failed to init event");
		return MHI_STATUS_ERROR;
	}
	mhi_dev_ctxt->mhi_ev_wq.state_change_event =
				kmalloc(sizeof(wait_queue_head_t), GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.state_change_event) {
		mhi_log(MHI_MSG_ERROR, "Failed to init event");
		goto error_event_handle_alloc;
	}
	/* Initialize the event which signals M0 */
	mhi_dev_ctxt->mhi_ev_wq.m0_event = kmalloc(sizeof(wait_queue_head_t),
								GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.m0_event) {
		mhi_log(MHI_MSG_ERROR, "Failed to init event");
		goto error_state_change_event_handle;
	}
	/* Initialize the event which signals M0 */
	mhi_dev_ctxt->mhi_ev_wq.m3_event = kmalloc(sizeof(wait_queue_head_t),
								GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.m3_event) {
		mhi_log(MHI_MSG_ERROR, "Failed to init event");
		goto error_m0_event;
	}
	/* Initialize the event which signals M0 */
	mhi_dev_ctxt->mhi_ev_wq.bhi_event = kmalloc(sizeof(wait_queue_head_t),
								GFP_KERNEL);
	if (NULL == mhi_dev_ctxt->mhi_ev_wq.bhi_event) {
		mhi_log(MHI_MSG_ERROR, "Failed to init event");
		goto error_bhi_event;
	}
	/* Initialize the event which starts the event parsing thread */
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq);
	/* Initialize the event which starts the state change thread */
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.state_change_event);
	/* Initialize the event which triggers clients waiting to send */
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	/* Initialize the event which triggers D3hot */
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	init_waitqueue_head(mhi_dev_ctxt->mhi_ev_wq.bhi_event);

	return MHI_STATUS_SUCCESS;
error_bhi_event:
	kfree(mhi_dev_ctxt->mhi_ev_wq.m3_event);
error_m0_event:
	kfree(mhi_dev_ctxt->mhi_ev_wq.m0_event);
error_state_change_event_handle:
	kfree(mhi_dev_ctxt->mhi_ev_wq.state_change_event);
error_event_handle_alloc:
	kfree(mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq);
	return MHI_STATUS_ERROR;
}

static enum MHI_STATUS mhi_init_state_change_thread_work_queue(
			struct mhi_state_work_queue *q)
{
	bool lock_acquired = 0;
	unsigned long flags;

	if (NULL == q->q_lock) {
		q->q_lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (NULL == q->q_lock)
			return MHI_STATUS_ALLOC_ERROR;
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

	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_init_device_ctrl(struct mhi_device_ctxt
								*mhi_dev_ctxt)
{
	size_t ctrl_seg_size = 0;
	size_t ctrl_seg_offset = 0;
	int i = 0;
	u32 align_len = sizeof(u64) * 2;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;

	if (NULL == mhi_dev_ctxt || NULL == mhi_dev_ctxt->mhi_ctrl_seg_info ||
			NULL == mhi_dev_ctxt->mhi_ctrl_seg_info->dev)
		return MHI_STATUS_ERROR;

	mhi_dev_ctxt->enable_lpm = 1;
	mhi_dev_ctxt->flags.mhi_initialized = 0;

	mhi_log(MHI_MSG_INFO, "Allocating control segment.\n");
	ctrl_seg_size += sizeof(struct mhi_control_seg);
	/* Calculate the size of the control segment needed */

	ctrl_seg_size += align_len - (ctrl_seg_size % align_len);

	ret_val = mhi_mallocmemregion(mhi_dev_ctxt->mhi_ctrl_seg_info,
							ctrl_seg_size);
	if (MHI_STATUS_SUCCESS != ret_val)
		return MHI_STATUS_ERROR;
	mhi_dev_ctxt->mhi_ctrl_seg =
			mhi_get_virt_addr(mhi_dev_ctxt->mhi_ctrl_seg_info);

	if (!mhi_dev_ctxt->mhi_ctrl_seg)
		return MHI_STATUS_ALLOC_ERROR;

	/* Set the channel contexts, event contexts and cmd context */
	ctrl_seg_offset = (uintptr_t)mhi_dev_ctxt->mhi_ctrl_seg +
						sizeof(struct mhi_control_seg);

	/* Set the channel direction and state */
	ctrl_seg_offset += align_len - (ctrl_seg_offset % align_len);
		for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
			mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[i].
						mhi_chan_type = (i % 2) + 1;
			mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[i].
						mhi_chan_state =
							MHI_CHAN_STATE_ENABLED;
		}
	return MHI_STATUS_SUCCESS;
}
/**
 * mhi_cmd_ring_init-  Initialization of the command ring
 *
 * @cmd_ctxt:			command ring context to initialize
 * @trb_list_phy_addr:		Pointer to the pysical address of the tre ring
 * @trb_list_virt_addr:		Pointer to the virtual address of the tre ring
 * @el_per_ring:		Number of elements in this command ring
 * @ring:			Pointer to the shadow command context
 *
 * @Return MHI_STATUS
 */
static enum MHI_STATUS mhi_cmd_ring_init(struct mhi_cmd_ctxt *cmd_ctxt,
				uintptr_t trb_list_phy_addr,
				uintptr_t trb_list_virt_addr,
				size_t el_per_ring, struct mhi_ring *ring)
{
	cmd_ctxt->mhi_cmd_ring_base_addr = trb_list_phy_addr;
	cmd_ctxt->mhi_cmd_ring_read_ptr = trb_list_phy_addr;
	cmd_ctxt->mhi_cmd_ring_write_ptr = trb_list_phy_addr;
	cmd_ctxt->mhi_cmd_ring_len =
		(size_t)el_per_ring*sizeof(union mhi_cmd_pkt);
	ring[PRIMARY_CMD_RING].wp = (void *)trb_list_virt_addr;
	ring[PRIMARY_CMD_RING].rp = (void *)trb_list_virt_addr;
	ring[PRIMARY_CMD_RING].base = (void *)trb_list_virt_addr;
	ring[PRIMARY_CMD_RING].len =
		(size_t)el_per_ring*sizeof(union mhi_cmd_pkt);
	ring[PRIMARY_CMD_RING].el_size = sizeof(union mhi_cmd_pkt);
	ring[PRIMARY_CMD_RING].overwrite_en = 0;
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_init_timers(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_init_wakelock(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	wakeup_source_init(&mhi_dev_ctxt->w_lock, "mhi_wakeup_source");
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_init_contexts(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;
	struct mhi_control_seg *mhi_ctrl = mhi_dev_ctxt->mhi_ctrl_seg;

	r = init_event_ctxt_array(mhi_dev_ctxt);
	if (r)
		return MHI_STATUS_ERROR;

	/* Init Command Ring */
	mhi_cmd_ring_init(&mhi_ctrl->mhi_cmd_ctxt_list[PRIMARY_CMD_RING],
			virt_to_dma(NULL,
				mhi_ctrl->cmd_trb_list[PRIMARY_CMD_RING]),
			(uintptr_t)mhi_ctrl->cmd_trb_list[PRIMARY_CMD_RING],
			CMD_EL_PER_RING,
			&mhi_dev_ctxt->mhi_local_cmd_ctxt[PRIMARY_CMD_RING]);

	mhi_dev_ctxt->mhi_state = MHI_STATE_RESET;
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_spawn_threads(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	mhi_dev_ctxt->event_thread_handle = kthread_run(parse_event_thread,
							mhi_dev_ctxt,
							"mhi_ev_thrd");
	if (-ENOMEM == (int)mhi_dev_ctxt->event_thread_handle)
		return MHI_STATUS_ERROR;
	mhi_dev_ctxt->st_thread_handle = kthread_run(mhi_state_change_thread,
							mhi_dev_ctxt,
							"mhi_st_thrd");
	if (-ENOMEM == (int)mhi_dev_ctxt->event_thread_handle)
		return MHI_STATUS_ERROR;
	return MHI_STATUS_SUCCESS;
}

/**
 * @brief Main initialization function for a mhi struct device context
 *	 All threads, events mutexes, mhi specific data structures
 *	 are initialized here
 *
 * @param dev_info [IN ] pcie struct device information structure to
 which this mhi context belongs
 * @param mhi_struct device [IN/OUT] reference to a mhi context to be populated
 *
 * @return MHI_STATUS
 */
enum MHI_STATUS mhi_init_device_ctxt(struct mhi_pcie_dev_info *dev_info,
		struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;
	if (NULL == dev_info || NULL == mhi_dev_ctxt)
		return MHI_STATUS_ERROR;
	mhi_log(MHI_MSG_VERBOSE, "mhi_init_device_ctxt>Init MHI dev ctxt\n");

	mhi_dev_ctxt->dev_info = dev_info;
	mhi_dev_ctxt->dev_props = &dev_info->core;

	r = mhi_populate_event_cfg(mhi_dev_ctxt);
	if (r) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to get event ring properties ret %d\n", r);
		mhi_clean_init_stage(mhi_dev_ctxt, MHI_INIT_ERROR_STAGE_SYNC);
		return MHI_STATUS_ERROR;
	}

	if (MHI_STATUS_SUCCESS != mhi_init_sync(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to initialize mhi sync\n");
		mhi_clean_init_stage(mhi_dev_ctxt, MHI_INIT_ERROR_STAGE_SYNC);
		return MHI_STATUS_ERROR;
	}

	if (MHI_STATUS_SUCCESS != mhi_init_ctrl_zone(dev_info, mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to initialize  memory zones\n");
		mhi_clean_init_stage(mhi_dev_ctxt,
					MHI_INIT_ERROR_STAGE_MEM_ZONES);
		return MHI_STATUS_ERROR;
	}
	if (MHI_STATUS_SUCCESS != mhi_init_events(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to initialize mhi events\n");
		mhi_clean_init_stage(mhi_dev_ctxt, MHI_INIT_ERROR_STAGE_EVENTS);
		return MHI_STATUS_ERROR;
	}
	if (MHI_STATUS_SUCCESS != mhi_reset_all_thread_queues(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to initialize work queues\n");
		mhi_clean_init_stage(mhi_dev_ctxt,
					MHI_INIT_ERROR_STAGE_THREAD_QUEUES);
		return MHI_STATUS_ERROR;
	}
	if (MHI_STATUS_SUCCESS != mhi_init_device_ctrl(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to initialize ctrl seg\n");
		mhi_clean_init_stage(mhi_dev_ctxt,
					MHI_INIT_ERROR_STAGE_THREAD_QUEUES);
		return MHI_STATUS_ERROR;
	}
	create_ev_rings(mhi_dev_ctxt);

	if (MHI_STATUS_SUCCESS != mhi_init_contexts(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed initializing contexts\n");
		mhi_clean_init_stage(mhi_dev_ctxt,
					MHI_INIT_ERROR_STAGE_DEVICE_CTRL);
		return MHI_STATUS_ERROR;
	}
	if (MHI_STATUS_SUCCESS != mhi_spawn_threads(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to spawn threads\n");
		return MHI_STATUS_ERROR;
	}
	if (MHI_STATUS_SUCCESS != mhi_init_timers(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed initializing timers\n");
		mhi_clean_init_stage(mhi_dev_ctxt,
					MHI_INIT_ERROR_STAGE_DEVICE_CTRL);
		return MHI_STATUS_ERROR;
	}
	if (MHI_STATUS_SUCCESS != mhi_init_wakelock(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to initialize wakelock\n");
		mhi_clean_init_stage(mhi_dev_ctxt,
					MHI_INIT_ERROR_STAGE_DEVICE_CTRL);
		return MHI_STATUS_ERROR;
	}
	return MHI_STATUS_SUCCESS;
}

/**
 * @brief Initialize the channel context and shadow context
 *
 * @cc_list:		Context to initialize
 * @trb_list_phy:	Physical base address for the TRE ring
 * @trb_list_virt:	Virtual base address for the TRE ring
 * @el_per_ring:	Number of TREs this ring will contain
 * @chan_type:		Type of channel IN/OUT
 * @event_ring:	 Event ring to be mapped to this channel context
 * @ring:		 Shadow context to be initialized alongside
 *
 * @Return MHI_STATUS
 */
enum MHI_STATUS mhi_init_chan_ctxt(struct mhi_chan_ctxt *cc_list,
		uintptr_t trb_list_phy, uintptr_t trb_list_virt,
		u64 el_per_ring, enum MHI_CHAN_TYPE chan_type,
		u32 event_ring, struct mhi_ring *ring,
		enum MHI_CHAN_STATE chan_state)
{
	cc_list->mhi_chan_state = chan_state;
	cc_list->mhi_chan_type = chan_type;
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
	/* Flush writes to MMIO */
	wmb();
	return MHI_STATUS_SUCCESS;
}

enum MHI_STATUS mhi_reset_all_thread_queues(
		struct mhi_device_ctxt *mhi_dev_ctxt)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;

	mhi_init_state_change_thread_work_queue(
				&mhi_dev_ctxt->state_change_work_item_list);
	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_log(MHI_MSG_ERROR, "Failed to reset STT work queue\n");
		return ret_val;
	}
	return ret_val;
}

enum MHI_STATUS mhi_reg_notifiers(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 ret_val;
	if (NULL == mhi_dev_ctxt)
		return MHI_STATUS_ERROR;
	mhi_dev_ctxt->mhi_cpu_notifier.notifier_call = mhi_cpu_notifier_cb;
	ret_val = register_cpu_notifier(&mhi_dev_ctxt->mhi_cpu_notifier);
	if (ret_val)
		return MHI_STATUS_ERROR;
	else
		return MHI_STATUS_SUCCESS;
}

