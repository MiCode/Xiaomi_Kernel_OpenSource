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
#include <linux/interrupt.h>
#include <linux/irqreturn.h>

#include "mhi_sys.h"
#include "mhi_trace.h"

static int mhi_process_event_ring(
		struct mhi_device_ctxt *mhi_dev_ctxt,
		u32 ev_index,
		u32 event_quota)
{
	union mhi_event_pkt *local_rp = NULL;
	union mhi_event_pkt *device_rp = NULL;
	union mhi_event_pkt event_to_process;
	int count = 0;
	struct mhi_event_ctxt *ev_ctxt = NULL;
	unsigned long flags;
	struct mhi_ring *local_ev_ctxt =
		&mhi_dev_ctxt->mhi_local_event_ctxt[ev_index];

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Enter ev_index:%u\n", ev_index);
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	if (unlikely(MHI_EVENT_ACCESS_INVALID(mhi_dev_ctxt->mhi_pm_state))) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"No event access, PM_STATE:0x%x\n",
			mhi_dev_ctxt->mhi_pm_state);
		read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
		return -EIO;
	}
	mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, false);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	ev_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[ev_index];

	spin_lock_irqsave(&local_ev_ctxt->ring_lock, flags);
	device_rp = (union mhi_event_pkt *)mhi_p2v_addr(
					mhi_dev_ctxt,
					MHI_RING_TYPE_EVENT_RING,
					ev_index,
					ev_ctxt->mhi_event_read_ptr);

	local_rp = (union mhi_event_pkt *)local_ev_ctxt->rp;
	spin_unlock_irqrestore(&local_ev_ctxt->ring_lock, flags);
	BUG_ON(validate_ev_el_addr(local_ev_ctxt, (uintptr_t)device_rp));

	while ((local_rp != device_rp) && (event_quota > 0) &&
			(device_rp != NULL) && (local_rp != NULL)) {

		spin_lock_irqsave(&local_ev_ctxt->ring_lock, flags);
		event_to_process = *local_rp;
		recycle_trb_and_ring(mhi_dev_ctxt,
				     local_ev_ctxt,
				     MHI_RING_TYPE_EVENT_RING,
				     ev_index);
		spin_unlock_irqrestore(&local_ev_ctxt->ring_lock, flags);

		switch (MHI_TRB_READ_INFO(EV_TRB_TYPE, &event_to_process)) {
		case MHI_PKT_TYPE_CMD_COMPLETION_EVENT:
		{
			union mhi_cmd_pkt *cmd_pkt;
			u32 chan;
			struct mhi_chan_cfg *cfg;
			unsigned long flags;
			struct mhi_ring *cmd_ring = &mhi_dev_ctxt->
				mhi_local_cmd_ctxt[PRIMARY_CMD_RING];
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			__pm_relax(&mhi_dev_ctxt->w_lock);
			get_cmd_pkt(mhi_dev_ctxt,
				    &event_to_process,
				    &cmd_pkt, ev_index);
			MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
			cfg = &mhi_dev_ctxt->mhi_chan_cfg[chan];
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"MHI CCE received ring 0x%x chan:%u\n",
				ev_index, chan);
			spin_lock_irqsave(&cfg->event_lock, flags);
			cfg->cmd_pkt = *cmd_pkt;
			cfg->cmd_event_pkt =
				event_to_process.cmd_complete_event_pkt;
			complete(&cfg->cmd_complete);
			spin_unlock_irqrestore(&cfg->event_lock, flags);
			spin_lock_irqsave(&cmd_ring->ring_lock,
					  flags);
			ctxt_del_element(cmd_ring, NULL);
			spin_unlock_irqrestore(&cmd_ring->ring_lock,
					       flags);
			break;
		}
		case MHI_PKT_TYPE_TX_EVENT:
		{
			u32 chan;
			struct mhi_ring *ring;
			unsigned long flags;

			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			chan = MHI_EV_READ_CHID(EV_CHID, &event_to_process);
			if (unlikely(!VALID_CHAN_NR(chan))) {
				mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
					"Invalid chan:%d\n", chan);
				break;
			}
			ring = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
			spin_lock_irqsave(&ring->ring_lock, flags);
			if (ring->ch_state == MHI_CHAN_STATE_ENABLED)
				parse_xfer_event(mhi_dev_ctxt,
						 &event_to_process,
						 ev_index);
			spin_unlock_irqrestore(&ring->ring_lock, flags);
			__pm_relax(&mhi_dev_ctxt->w_lock);
			event_quota--;
			break;
		}
		case MHI_PKT_TYPE_STATE_CHANGE_EVENT:
		{
			enum STATE_TRANSITION new_state;
			unsigned long flags;
			new_state = MHI_READ_STATE(&event_to_process);
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"MHI STE received ring 0x%x State:%s\n",
				ev_index, state_transition_str(new_state));

			switch (new_state) {
			case STATE_TRANSITION_M0:
				process_m0_transition(mhi_dev_ctxt);
				break;
			case STATE_TRANSITION_M1:
				write_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock,
						   flags);
				mhi_dev_ctxt->mhi_state =
					mhi_get_m_state(mhi_dev_ctxt);
				if (mhi_dev_ctxt->mhi_state == MHI_STATE_M1) {
					enum MHI_PM_STATE state;

					state = mhi_tryset_pm_state
						(mhi_dev_ctxt, MHI_PM_M1);
					if (state == MHI_PM_M1) {
						mhi_dev_ctxt->counters.m0_m1++;
						schedule_work
							(&mhi_dev_ctxt->
							 process_m1_worker);
					}
				}
				write_unlock_irqrestore(&mhi_dev_ctxt->
							pm_xfer_lock, flags);
				break;
			case STATE_TRANSITION_M3:
				process_m3_transition(mhi_dev_ctxt);
				break;
			case STATE_TRANSITION_SYS_ERR:
			{
				enum MHI_PM_STATE new_state;
				unsigned long flags;

				mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
					"MHI System Error Detected\n");
				write_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock,
						   flags);
				new_state = mhi_tryset_pm_state
					(mhi_dev_ctxt, MHI_PM_SYS_ERR_DETECT);
				write_unlock_irqrestore
					(&mhi_dev_ctxt->pm_xfer_lock, flags);
				if (new_state == MHI_PM_SYS_ERR_DETECT)
					schedule_work(&mhi_dev_ctxt->
						      process_sys_err_worker);
				break;
			}
			default:
				mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
					"Unsupported STE received ring 0x%x State:%s\n",
					ev_index,
					state_transition_str(new_state));
			}
			break;
		}
		case MHI_PKT_TYPE_EE_EVENT:
		{
			enum STATE_TRANSITION new_state = 0;
			enum MHI_EXEC_ENV event =
				MHI_READ_EXEC_ENV(&event_to_process);

			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"MHI EE received ring 0x%x event:0x%x\n",
				ev_index, event);
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			__pm_relax(&mhi_dev_ctxt->w_lock);
			switch (event) {
			case MHI_EXEC_ENV_SBL:
				new_state = STATE_TRANSITION_SBL;
				break;
			case MHI_EXEC_ENV_AMSS:
				new_state = STATE_TRANSITION_AMSS;
				break;
			case MHI_EXEC_ENV_BHIE:
				new_state = STATE_TRANSITION_BHIE;
				break;
			case MHI_EXEC_ENV_RDDM:
				new_state = STATE_TRANSITION_RDDM;
				break;
			default:
				mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
					"Invalid EE Event 0x%x received\n",
					event);
			}
			if (new_state)
				mhi_init_state_transition(mhi_dev_ctxt,
							  new_state);
			break;
		}
		case MHI_PKT_TYPE_STALE_EVENT:
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Stale Event received for chan:%u\n",
				MHI_EV_READ_CHID(EV_CHID, local_rp));
			break;
		default:
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Unsupported packet type code 0x%x\n",
				MHI_TRB_READ_INFO(EV_TRB_TYPE,
					&event_to_process));
			break;
		}
		spin_lock_irqsave(&local_ev_ctxt->ring_lock, flags);
		local_rp = (union mhi_event_pkt *)local_ev_ctxt->rp;
		device_rp = (union mhi_event_pkt *)mhi_p2v_addr(
						mhi_dev_ctxt,
						MHI_RING_TYPE_EVENT_RING,
						ev_index,
						ev_ctxt->mhi_event_read_ptr);
		spin_unlock_irqrestore(&local_ev_ctxt->ring_lock, flags);
		count++;
	}
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "exit ev_index:%u\n", ev_index);
	return count;
}

void mhi_ev_task(unsigned long data)
{
	struct mhi_ring *mhi_ring = (struct mhi_ring *)data;
	struct mhi_device_ctxt *mhi_dev_ctxt =
		mhi_ring->mhi_dev_ctxt;
	int ev_index = mhi_ring->index;
	const int CTRL_EV = 0; /* event ring for ctrl events */
	int ret;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Enter\n");

	/* Process event ring */
	ret = mhi_process_event_ring(mhi_dev_ctxt, ev_index, U32_MAX);
	/*
	 * If we received MSI for primary event ring with no events to process
	 * check status register to see if device enter SYSERR status
	 */
	if (ev_index == CTRL_EV && !ret) {
		bool in_sys_err = false;
		unsigned long flags;
		enum MHI_PM_STATE new_state;

		read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
		if (MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state))
			in_sys_err = mhi_in_sys_err(mhi_dev_ctxt);
		read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);

		if (in_sys_err) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"MHI System Error Detected\n");
			write_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
			new_state = mhi_tryset_pm_state(mhi_dev_ctxt,
							MHI_PM_SYS_ERR_DETECT);
			write_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock,
						flags);
			if (new_state == MHI_PM_SYS_ERR_DETECT)
				schedule_work(&mhi_dev_ctxt->
					      process_sys_err_worker);
		}
	}

	enable_irq(MSI_TO_IRQ(mhi_dev_ctxt, ev_index));
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Exit\n");
}

void process_event_ring(struct work_struct *work)
{
	struct mhi_ring *mhi_ring =
		container_of(work, struct mhi_ring, ev_worker);
	struct mhi_device_ctxt *mhi_dev_ctxt =
		mhi_ring->mhi_dev_ctxt;
	int ev_index = mhi_ring->index;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Enter\n");
	/* Process event ring */
	mhi_process_event_ring(mhi_dev_ctxt, ev_index, U32_MAX);

	enable_irq(MSI_TO_IRQ(mhi_dev_ctxt, ev_index));
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Exit\n");
}

struct mhi_result *mhi_poll(struct mhi_client_handle *client_handle)
{
	int ret_val;
	struct mhi_client_config *client_config = client_handle->client_config;

	client_config->result.buf_addr = NULL;
	client_config->result.bytes_xferd = 0;
	client_config->result.transaction_status = 0;
	ret_val = mhi_process_event_ring(client_config->mhi_dev_ctxt,
					 client_config->event_ring_index,
					 1);
	if (ret_val < 0)
		mhi_log(client_config->mhi_dev_ctxt, MHI_MSG_INFO,
			"NAPI failed to process event ring\n");
	return &(client_config->result);
}

void mhi_mask_irq(struct mhi_client_handle *client_handle)
{
	struct mhi_client_config *client_config = client_handle->client_config;
	struct mhi_device_ctxt *mhi_dev_ctxt =
		client_config->mhi_dev_ctxt;
	struct mhi_ring *ev_ring = &mhi_dev_ctxt->
		mhi_local_event_ctxt[client_config->event_ring_index];

	disable_irq_nosync(MSI_TO_IRQ(mhi_dev_ctxt, client_config->msi_vec));
	ev_ring->msi_disable_cntr++;
}

void mhi_unmask_irq(struct mhi_client_handle *client_handle)
{
	struct mhi_client_config *client_config = client_handle->client_config;
	struct mhi_device_ctxt *mhi_dev_ctxt =
		client_config->mhi_dev_ctxt;
	struct mhi_ring *ev_ring = &mhi_dev_ctxt->
		mhi_local_event_ctxt[client_config->event_ring_index];

	ev_ring->msi_enable_cntr++;
	enable_irq(MSI_TO_IRQ(mhi_dev_ctxt, client_config->msi_vec));
}

irqreturn_t mhi_msi_handlr(int irq_number, void *dev_id)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = dev_id;
	int msi = IRQ_TO_MSI(mhi_dev_ctxt, irq_number);
	struct mhi_ring *mhi_ring = &mhi_dev_ctxt->mhi_local_event_ctxt[msi];
	struct mhi_event_ring_cfg *ring_props =
		&mhi_dev_ctxt->ev_ring_props[msi];

	mhi_dev_ctxt->counters.msi_counter[
			IRQ_TO_MSI(mhi_dev_ctxt, irq_number)]++;
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Got MSI 0x%x\n", msi);
	trace_mhi_msi(IRQ_TO_MSI(mhi_dev_ctxt, irq_number));
	disable_irq_nosync(irq_number);
	if (ring_props->priority <= MHI_EV_PRIORITY_TASKLET)
		tasklet_schedule(&mhi_ring->ev_task);
	else
		schedule_work(&mhi_ring->ev_worker);

	return IRQ_HANDLED;
}

irqreturn_t mhi_msi_ipa_handlr(int irq_number, void *dev_id)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = dev_id;
	struct mhi_event_ring_cfg *ev_ring_props;
	struct mhi_client_handle *client_handle;
	struct mhi_client_config *client_config;
	struct mhi_client_info_t *client_info;
	struct mhi_cb_info cb_info;
	int msi_num = (IRQ_TO_MSI(mhi_dev_ctxt, irq_number));

	mhi_dev_ctxt->counters.msi_counter[msi_num]++;
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Got MSI 0x%x\n", msi_num);
	trace_mhi_msi(msi_num);

	/* Obtain client config from MSI */
	ev_ring_props = &mhi_dev_ctxt->ev_ring_props[msi_num];
	client_handle = mhi_dev_ctxt->client_handle_list[ev_ring_props->chan];
	if (unlikely(!client_handle)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Recv MSI for unreg chan:%u\n", ev_ring_props->chan);
		return IRQ_HANDLED;
	}

	client_config = client_handle->client_config;
	client_info = &client_config->client_info;
	client_config->result.user_data =
				client_config->user_data;
	cb_info.result = &client_config->result;
	cb_info.cb_reason = MHI_CB_XFER;
	cb_info.chan = client_config->chan_info.chan_nr;
	cb_info.result->transaction_status = 0;
	client_info->mhi_client_cb(&cb_info);

	return IRQ_HANDLED;
}
