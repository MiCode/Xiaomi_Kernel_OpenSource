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
#include <linux/interrupt.h>
#include <linux/irqreturn.h>

#include "mhi_sys.h"
#include "mhi_trace.h"

irqreturn_t mhi_msi_handlr(int irq_number, void *dev_id)
{
	struct device *mhi_device = dev_id;
	struct mhi_device_ctxt *mhi_dev_ctxt = mhi_device->platform_data;

	if (!mhi_dev_ctxt) {
		mhi_log(MHI_MSG_ERROR, "Failed to get a proper context\n");
		return IRQ_HANDLED;
	}
	mhi_dev_ctxt->counters.msi_counter[
			IRQ_TO_MSI(mhi_dev_ctxt, irq_number)]++;
	mhi_log(MHI_MSG_VERBOSE,
		"Got MSI 0x%x\n", IRQ_TO_MSI(mhi_dev_ctxt, irq_number));
	trace_mhi_msi(IRQ_TO_MSI(mhi_dev_ctxt, irq_number));
	atomic_inc(&mhi_dev_ctxt->flags.events_pending);
	wake_up_interruptible(
		mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq);
	return IRQ_HANDLED;
}

irqreturn_t mhi_msi_ipa_handlr(int irq_number, void *dev_id)
{
	struct device *mhi_device = dev_id;
	u32 client_index;
	struct mhi_device_ctxt *mhi_dev_ctxt = mhi_device->platform_data;
	struct mhi_client_handle *client_handle;
	struct mhi_client_info_t *client_info;
	struct mhi_cb_info cb_info;
	int msi_num = (IRQ_TO_MSI(mhi_dev_ctxt, irq_number));

	mhi_dev_ctxt->counters.msi_counter[msi_num]++;
	mhi_log(MHI_MSG_VERBOSE, "Got MSI 0x%x\n", msi_num);
	trace_mhi_msi(msi_num);
	client_index = MHI_MAX_CHANNELS -
			(mhi_dev_ctxt->mmio_info.nr_event_rings - msi_num);
	client_handle = mhi_dev_ctxt->client_handle_list[client_index];
	client_info = &client_handle->client_info;
	if (likely(NULL != client_handle)) {
		client_handle->result.user_data =
				client_handle->user_data;
	if (likely(NULL != &client_info->mhi_client_cb)) {
			cb_info.result = &client_handle->result;
			cb_info.cb_reason = MHI_CB_XFER;
			cb_info.chan = client_handle->chan_info.chan_nr;
			cb_info.result->transaction_status =
					MHI_STATUS_SUCCESS;
			client_info->mhi_client_cb(&cb_info);
		}
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
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	struct mhi_event_ctxt *ev_ctxt = NULL;
	union mhi_cmd_pkt *cmd_pkt = NULL;
	union mhi_event_pkt *ev_ptr = NULL;
	struct mhi_ring *local_ev_ctxt =
		&mhi_dev_ctxt->mhi_local_event_ctxt[ev_index];
	u32 event_code;

	ev_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[ev_index];

	device_rp = (union mhi_event_pkt *)mhi_p2v_addr(
					mhi_dev_ctxt,
					MHI_RING_TYPE_EVENT_RING,
					ev_index,
					ev_ctxt->mhi_event_read_ptr);

	local_rp = (union mhi_event_pkt *)local_ev_ctxt->rp;

	BUG_ON(validate_ev_el_addr(local_ev_ctxt, (uintptr_t)device_rp));

	while ((local_rp != device_rp) && (event_quota > 0) &&
			(device_rp != NULL) && (local_rp != NULL)) {
		event_to_process = *local_rp;
		ev_ptr = &event_to_process;
		event_code = get_cmd_pkt(mhi_dev_ctxt,
					ev_ptr, &cmd_pkt, ev_index);
		if (((MHI_TRB_READ_INFO(EV_TRB_TYPE, (&event_to_process)) ==
		    MHI_PKT_TYPE_CMD_COMPLETION_EVENT)) &&
		    (event_code == MHI_EVENT_CC_SUCCESS)) {
			mhi_log(MHI_MSG_INFO, "Command Completion event\n");
			if ((MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt) ==
			     MHI_PKT_TYPE_RESET_CHAN_CMD)) {
				mhi_log(MHI_MSG_INFO, "First Reset CC event\n");
				MHI_TRB_SET_INFO(CMD_TRB_TYPE, cmd_pkt,
					MHI_PKT_TYPE_RESET_CHAN_DEFER_CMD);
				ret_val = MHI_STATUS_CMD_PENDING;
				break;
			} else if ((MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt)
				    == MHI_PKT_TYPE_RESET_CHAN_DEFER_CMD)) {
				MHI_TRB_SET_INFO(CMD_TRB_TYPE, cmd_pkt,
						 MHI_PKT_TYPE_RESET_CHAN_CMD);
				mhi_log(MHI_MSG_INFO,
					"Processing Reset CC event\n");
			}
		}
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
			ret_val = parse_cmd_event(mhi_dev_ctxt,
					&event_to_process, ev_index);
			break;
		case MHI_PKT_TYPE_TX_EVENT:
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			parse_xfer_event(mhi_dev_ctxt,
						&event_to_process, ev_index);
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
		case MHI_PKT_TYPE_SYS_ERR_EVENT:
			mhi_log(MHI_MSG_INFO,
			   "MHI System Error Detected. Triggering Reset\n");
			BUG();
			if (!mhi_trigger_reset(mhi_dev_ctxt))
				mhi_log(MHI_MSG_ERROR,
				"Failed to reset for SYSERR recovery\n");
		break;
		default:
			mhi_log(MHI_MSG_ERROR,
				"Unsupported packet type code 0x%x\n",
				MHI_TRB_READ_INFO(EV_TRB_TYPE,
					&event_to_process));
			break;
		}
		local_rp = (union mhi_event_pkt *)local_ev_ctxt->rp;
		device_rp = (union mhi_event_pkt *)mhi_p2v_addr(
						mhi_dev_ctxt,
						MHI_RING_TYPE_EVENT_RING,
						ev_index,
						ev_ctxt->mhi_event_read_ptr);
		ret_val = MHI_STATUS_SUCCESS;
		--event_quota;
	}
	return ret_val;
}

int parse_event_thread(void *ctxt)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = ctxt;
	u32 i = 0;
	int ret_val = 0;
	int ret_val_process_event = 0;
	atomic_t *ev_pen_ptr = &mhi_dev_ctxt->flags.events_pending;

	/* Go through all event rings */
	for (;;) {
		ret_val =
			wait_event_interruptible(
				*mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq,
				((atomic_read(
				&mhi_dev_ctxt->flags.events_pending) > 0) &&
					!mhi_dev_ctxt->flags.stop_threads) ||
				mhi_dev_ctxt->flags.kill_threads ||
				(mhi_dev_ctxt->flags.stop_threads &&
				!mhi_dev_ctxt->flags.ev_thread_stopped));

		switch (ret_val) {
		case -ERESTARTSYS:
			return 0;
		default:
			if (mhi_dev_ctxt->flags.kill_threads) {
				mhi_log(MHI_MSG_INFO,
					"Caught exit signal, quitting\n");
				return 0;
			}
			if (mhi_dev_ctxt->flags.stop_threads) {
				mhi_dev_ctxt->flags.ev_thread_stopped = 1;
				continue;
			}
			break;
		}
		mhi_dev_ctxt->flags.ev_thread_stopped = 0;
		atomic_dec(&mhi_dev_ctxt->flags.events_pending);
		for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i) {
			if (mhi_dev_ctxt->mhi_state == MHI_STATE_SYS_ERR) {
				mhi_log(MHI_MSG_INFO,
				   "SYS_ERR detected, not processing events\n");
				atomic_set(&mhi_dev_ctxt->flags.events_pending,
					   0);
				break;
			}
			if (GET_EV_PROPS(EV_MANAGED,
					mhi_dev_ctxt->ev_ring_props[i].flags)){
				ret_val_process_event =
				    mhi_process_event_ring(mhi_dev_ctxt, i,
				     mhi_dev_ctxt->ev_ring_props[i].nr_desc);
				if (ret_val_process_event ==
					MHI_STATUS_CMD_PENDING)
					atomic_inc(ev_pen_ptr);
			}
		}
	}
	return ret_val;
}

struct mhi_result *mhi_poll(struct mhi_client_handle *client_handle)
{
	enum MHI_STATUS ret_val;

	client_handle->result.buf_addr = NULL;
	client_handle->result.bytes_xferd = 0;
	client_handle->result.transaction_status = 0;
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
