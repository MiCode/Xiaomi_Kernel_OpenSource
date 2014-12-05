/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>

#include "mhi_sys.h"
#include "mhi_trace.h"


irqreturn_t mhi_msi_handlr(int irq_number, void *dev_id)
{
	struct device *mhi_device = dev_id;
	u32 client_index;
	struct mhi_device_ctxt *mhi_dev_ctxt = mhi_device->platform_data;
	struct mhi_client_handle *client_handle;
	struct mhi_client_info_t *client_info;
	struct mhi_cb_info cb_info;

	if (NULL == mhi_dev_ctxt) {
		mhi_log(MHI_MSG_ERROR, "Failed to get a proper context\n");
		return IRQ_HANDLED;
	}
	mhi_dev_ctxt->msi_counter[IRQ_TO_MSI(mhi_dev_ctxt, irq_number)]++;
	mhi_log(MHI_MSG_VERBOSE,
		"Got MSI 0x%x\n", IRQ_TO_MSI(mhi_dev_ctxt, irq_number));
	trace_mhi_msi(IRQ_TO_MSI(mhi_dev_ctxt, irq_number));
	switch (IRQ_TO_MSI(mhi_dev_ctxt, irq_number)) {
	case 0:
	case 1:
	case 2:
		atomic_inc(&mhi_dev_ctxt->flags.events_pending);
		wake_up_interruptible(mhi_dev_ctxt->event_handle);
		break;
	case 3:
		client_index =
			mhi_dev_ctxt->alloced_ev_rings[IPA_IN_EV_RING];
		client_handle = mhi_dev_ctxt->client_handle_list[client_index];
		client_info = &client_handle->client_info;

		if (likely(NULL != client_handle)) {
			client_handle->result.user_data =
					client_handle->user_data;
			if (likely(NULL != &client_info->mhi_client_cb)) {
				cb_info.result = &client_handle->result;
				cb_info.cb_reason = MHI_CB_XFER;
				cb_info.chan = client_handle->chan;
				cb_info.result->transaction_status =
						MHI_STATUS_SUCCESS;
				client_info->mhi_client_cb(&cb_info);
			}
		}
		break;
	}
	return IRQ_HANDLED;
}

static enum MHI_STATUS mhi_process_event_ring(
		struct mhi_device_ctxt *mhi_dev_ctxt,
		u32 ev_index,
		u32 event_quota)
{
	union mhi_event_pkt *local_rp = NULL;
	union mhi_event_pkt *device_rp = NULL;
	union mhi_event_pkt event_to_process;
	struct mhi_event_ctxt *ev_ctxt = NULL;
	struct mhi_ring *local_ev_ctxt =
		&mhi_dev_ctxt->mhi_local_event_ctxt[ev_index];

	ev_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_ec_list[ev_index];

	device_rp =
		(union mhi_event_pkt *)mhi_p2v_addr(
					mhi_dev_ctxt->mhi_ctrl_seg_info,
					ev_ctxt->mhi_event_read_ptr);
	local_rp = (union mhi_event_pkt *)local_ev_ctxt->rp;


	if (unlikely(MHI_STATUS_SUCCESS != validate_ev_el_addr(local_ev_ctxt,
				(uintptr_t)device_rp)))
		mhi_log(MHI_MSG_ERROR,
				"Failed to validate event ring element 0x%p\n",
				device_rp);

	while ((local_rp != device_rp) && (event_quota > 0) &&
			(device_rp != NULL) && (local_rp != NULL)) {
		event_to_process = *local_rp;
		if (unlikely(MHI_STATUS_SUCCESS !=
					recycle_trb_and_ring(mhi_dev_ctxt,
						local_ev_ctxt,
						MHI_RING_TYPE_EVENT_RING,
						ev_index)))
			mhi_log(MHI_MSG_ERROR, "Failed to recycle ev pkt\n");
		switch (MHI_TRB_READ_INFO(EV_TRB_TYPE, (&event_to_process))) {
		case MHI_PKT_TYPE_CMD_COMPLETION_EVENT:
			mhi_log(MHI_MSG_INFO,
					"MHI CCE received ring 0x%x\n",
					ev_index);
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			__pm_relax(&mhi_dev_ctxt->w_lock);
			parse_cmd_event(mhi_dev_ctxt,
					&event_to_process);
			break;
		case MHI_PKT_TYPE_TX_EVENT:
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			parse_xfer_event(mhi_dev_ctxt, &event_to_process);
			__pm_relax(&mhi_dev_ctxt->w_lock);
			break;
		case MHI_PKT_TYPE_STATE_CHANGE_EVENT:
		{
			enum STATE_TRANSITION new_state;
			new_state = MHI_READ_STATE(&event_to_process);
			mhi_log(MHI_MSG_INFO,
					"MHI STE received ring 0x%x\n",
					ev_index);
			mhi_init_state_transition(mhi_dev_ctxt, new_state);
			break;
		}
		case MHI_PKT_TYPE_EE_EVENT:
		{
			enum STATE_TRANSITION new_state;
			mhi_log(MHI_MSG_INFO,
					"MHI EEE received ring 0x%x\n",
					ev_index);
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			__pm_relax(&mhi_dev_ctxt->w_lock);
			switch (MHI_READ_EXEC_ENV(&event_to_process)) {
			case MHI_EXEC_ENV_SBL:
				new_state = STATE_TRANSITION_SBL;
				mhi_init_state_transition(mhi_dev_ctxt,
								new_state);
				break;
			case MHI_EXEC_ENV_AMSS:
				new_state = STATE_TRANSITION_AMSS;
				mhi_init_state_transition(mhi_dev_ctxt,
								new_state);
				break;
			}
			break;
		}
		default:
			mhi_log(MHI_MSG_ERROR,
				"Unsupported packet type code 0x%x\n",
				MHI_TRB_READ_INFO(EV_TRB_TYPE,
					&event_to_process));
			break;
		}
		local_rp = (union mhi_event_pkt *)local_ev_ctxt->rp;
		device_rp = (union mhi_event_pkt *)mhi_p2v_addr(
					mhi_dev_ctxt->mhi_ctrl_seg_info,
					(u64)ev_ctxt->mhi_event_read_ptr);
		--event_quota;
	}
	return MHI_STATUS_SUCCESS;
}

int parse_event_thread(void *ctxt)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = ctxt;
	u32 i = 0;
	u32 ev_poll_en = 0;
	int ret_val = 0;

	/* Go through all event rings */
	for (;;) {
		ret_val =
			wait_event_interruptible(*mhi_dev_ctxt->event_handle,
				((atomic_read(
				&mhi_dev_ctxt->flags.events_pending) > 0) &&
					!mhi_dev_ctxt->flags.stop_threads) ||
				mhi_dev_ctxt->flags.kill_threads ||
				(mhi_dev_ctxt->flags.stop_threads &&
				!mhi_dev_ctxt->ev_thread_stopped));

		switch (ret_val) {
		case -ERESTARTSYS:
			return 0;
			break;
		default:
			if (mhi_dev_ctxt->flags.kill_threads) {
				mhi_log(MHI_MSG_INFO,
					"Caught exit signal, quitting\n");
				return 0;
			}
			if (mhi_dev_ctxt->flags.stop_threads) {
				mhi_dev_ctxt->ev_thread_stopped = 1;
				continue;
			}
			break;
		}
		mhi_dev_ctxt->ev_thread_stopped = 0;
		atomic_dec(&mhi_dev_ctxt->flags.events_pending);

		for (i = 0; i < EVENT_RINGS_ALLOCATED; ++i) {
			MHI_GET_EVENT_RING_INFO(EVENT_RING_POLLING,
					mhi_dev_ctxt->ev_ring_props[i],
					ev_poll_en)
			if (ev_poll_en) {
				mhi_process_event_ring(mhi_dev_ctxt,
				 mhi_dev_ctxt->alloced_ev_rings[i],
				 EV_EL_PER_RING);
			}
		}
	}
	return 0;
}

struct mhi_result *mhi_poll(struct mhi_client_handle *client_handle)
{
	enum MHI_STATUS ret_val;
	client_handle->result.payload_buf = 0;
	client_handle->result.bytes_xferd = 0;
	ret_val = mhi_process_event_ring(client_handle->mhi_dev_ctxt,
				client_handle->event_ring_index,
				1);
	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_INFO, "NAPI failed to process event ring\n");
	return &(client_handle->result);
}

void mhi_mask_irq(struct mhi_client_handle *client_handle)
{
	disable_irq_nosync(MSI_TO_IRQ(client_handle->mhi_dev_ctxt,
					client_handle->msi_vec));
	client_handle->mhi_dev_ctxt->counters.msi_disable_cntr++;
	if (client_handle->mhi_dev_ctxt->counters.msi_disable_cntr >
		   (client_handle->mhi_dev_ctxt->counters.msi_enable_cntr + 1))
		mhi_log(MHI_MSG_INFO, "No nested IRQ disable Allowed\n");
}

void mhi_unmask_irq(struct mhi_client_handle *client_handle)
{
	client_handle->mhi_dev_ctxt->counters.msi_enable_cntr++;
	enable_irq(MSI_TO_IRQ(client_handle->mhi_dev_ctxt,
			client_handle->msi_vec));
	if (client_handle->mhi_dev_ctxt->counters.msi_enable_cntr >
		   client_handle->mhi_dev_ctxt->counters.msi_disable_cntr)
		mhi_log(MHI_MSG_INFO, "No nested IRQ enable Allowed\n");
}
