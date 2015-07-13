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
#include "mhi_hwio.h"
#include "mhi_trace.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

static inline void mhi_set_m_state(struct mhi_device_ctxt *mhi_dev_ctxt,
					enum MHI_STATE new_state)
{
	if (MHI_STATE_RESET == new_state) {
		mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRL,
			MHICTRL_RESET_MASK,
			MHICTRL_RESET_SHIFT,
			1);
	} else {
		mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRL,
			MHICTRL_MHISTATE_MASK,
			MHICTRL_MHISTATE_SHIFT,
			new_state);
	}
	mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRL);
}

static void conditional_chan_db_write(
				struct mhi_device_ctxt *mhi_dev_ctxt, u32 chan)
{
	u64 db_value;
	unsigned long flags;

	mhi_dev_ctxt->mhi_chan_db_order[chan] = 0;
	spin_lock_irqsave(&mhi_dev_ctxt->db_write_lock[chan], flags);
	if (0 == mhi_dev_ctxt->mhi_chan_db_order[chan]) {
		db_value = virt_to_dma(NULL,
				mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp);
		mhi_process_db(mhi_dev_ctxt,
			       mhi_dev_ctxt->mmio_info.chan_db_addr,
			       chan, db_value);
	}
	mhi_dev_ctxt->mhi_chan_db_order[chan] = 0;
	spin_unlock_irqrestore(&mhi_dev_ctxt->db_write_lock[chan], flags);
}

static void ring_all_chan_dbs(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 i = 0;
	struct mhi_ring *local_ctxt = NULL;

	mhi_log(MHI_MSG_VERBOSE, "Ringing chan dbs\n");
	for (i = 0; i < MHI_MAX_CHANNELS; ++i)
		if (VALID_CHAN_NR(i)) {
			local_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[i];
			if (IS_HARDWARE_CHANNEL(i))
				mhi_dev_ctxt->flags.db_mode[i] = 1;
			if ((local_ctxt->wp != local_ctxt->rp) ||
			   ((local_ctxt->wp != local_ctxt->rp) &&
			    (local_ctxt->dir == MHI_IN)))
				conditional_chan_db_write(mhi_dev_ctxt, i);
		}
}

static void ring_all_cmd_dbs(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	struct mutex *cmd_mutex = NULL;
	u64 db_value;
	u64 rp = 0;
	struct mhi_ring *local_ctxt = NULL;

	mhi_log(MHI_MSG_VERBOSE, "Ringing chan dbs\n");
	cmd_mutex = &mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING];
	mhi_dev_ctxt->cmd_ring_order = 0;
	mutex_lock(cmd_mutex);
	local_ctxt = &mhi_dev_ctxt->mhi_local_cmd_ctxt[PRIMARY_CMD_RING];
	rp = virt_to_dma(NULL, local_ctxt->rp);
	db_value = virt_to_dma(NULL, mhi_dev_ctxt->mhi_local_cmd_ctxt[0].wp);
	if (0 == mhi_dev_ctxt->cmd_ring_order && rp != db_value)
		mhi_process_db(mhi_dev_ctxt,
			       mhi_dev_ctxt->mmio_info.cmd_db_addr,
							0, db_value);
	mhi_dev_ctxt->cmd_ring_order = 0;
	mutex_unlock(cmd_mutex);
}

static void ring_all_ev_dbs(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 i;
	u64 db_value = 0;
	struct mhi_event_ctxt *event_ctxt = NULL;
	struct mhi_control_seg *mhi_ctrl = NULL;
	spinlock_t *lock = NULL;
	unsigned long flags;
	mhi_ctrl = mhi_dev_ctxt->mhi_ctrl_seg;

	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i) {
		lock = &mhi_dev_ctxt->mhi_ev_spinlock_list[i];
		mhi_dev_ctxt->mhi_ev_db_order[i] = 0;
		spin_lock_irqsave(lock, flags);
		event_ctxt = &mhi_ctrl->mhi_ec_list[i];
		db_value = virt_to_dma(NULL,
				mhi_dev_ctxt->mhi_local_event_ctxt[i].wp);
		if (0 == mhi_dev_ctxt->mhi_ev_db_order[i]) {
			mhi_process_db(mhi_dev_ctxt,
				       mhi_dev_ctxt->mmio_info.event_db_addr,
				       i, db_value);
		}
		mhi_dev_ctxt->mhi_ev_db_order[i] = 0;
		spin_unlock_irqrestore(lock, flags);
	}
}

static enum MHI_STATUS process_m0_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	unsigned long flags;
	int ret_val;
	mhi_log(MHI_MSG_INFO, "Entered\n");

	if (mhi_dev_ctxt->mhi_state == MHI_STATE_M2) {
		mhi_dev_ctxt->counters.m2_m0++;
	} else if (mhi_dev_ctxt->mhi_state == MHI_STATE_M3) {
			mhi_dev_ctxt->counters.m3_m0++;
	} else if (mhi_dev_ctxt->mhi_state == MHI_STATE_READY) {
		mhi_log(MHI_MSG_INFO,
			"Transitioning from READY.\n");
	} else if (mhi_dev_ctxt->mhi_state == MHI_STATE_M1) {
		mhi_log(MHI_MSG_INFO,
			"Transitioning from M1.\n");
	} else {
		mhi_log(MHI_MSG_INFO,
			"MHI State %d link state %d. Quitting\n",
			mhi_dev_ctxt->mhi_state, mhi_dev_ctxt->flags.link_up);
	}

	read_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->mhi_state = MHI_STATE_M0;
	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	mhi_assert_device_wake(mhi_dev_ctxt);
	read_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

	if (mhi_dev_ctxt->flags.mhi_initialized) {
		ring_all_ev_dbs(mhi_dev_ctxt);
		ring_all_chan_dbs(mhi_dev_ctxt);
		ring_all_cmd_dbs(mhi_dev_ctxt);
	}
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	ret_val  = mhi_set_bus_request(mhi_dev_ctxt, 1);
	if (ret_val)
		mhi_log(MHI_MSG_CRITICAL,
			"Could not set bus frequency ret: %d\n",
			ret_val);
	mhi_dev_ctxt->flags.pending_M0 = 0;
	if (atomic_read(&mhi_dev_ctxt->flags.pending_powerup)) {
		atomic_set(&mhi_dev_ctxt->flags.pending_ssr, 0);
		atomic_set(&mhi_dev_ctxt->flags.pending_powerup, 0);
	}
	wake_up_interruptible(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	if (!mhi_dev_ctxt->flags.pending_M3 &&
	     mhi_dev_ctxt->flags.link_up &&
	     mhi_dev_ctxt->flags.mhi_initialized)
		mhi_deassert_device_wake(mhi_dev_ctxt);
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

	mhi_log(MHI_MSG_INFO, "Exited\n");
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS process_m1_transition(
		struct mhi_device_ctxt  *mhi_dev_ctxt,
		enum STATE_TRANSITION cur_work_item)
{
	unsigned long flags = 0;
	int ret_val = 0;
	int r = 0;
	mhi_log(MHI_MSG_INFO,
			"Processing M1 state transition from state %d\n",
			mhi_dev_ctxt->mhi_state);

	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	if (!mhi_dev_ctxt->flags.pending_M3) {
		mhi_log(MHI_MSG_INFO, "Setting M2 Transition flag\n");
		atomic_inc(&mhi_dev_ctxt->flags.m2_transition);
		mhi_dev_ctxt->mhi_state = MHI_STATE_M2;
		mhi_log(MHI_MSG_INFO, "Allowing transition to M2\n");
		mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_M2);
		mhi_dev_ctxt->counters.m1_m2++;
	}
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	ret_val = mhi_set_bus_request(mhi_dev_ctxt, 0);
	if (ret_val)
		mhi_log(MHI_MSG_INFO, "Failed to update bus request\n");

	mhi_log(MHI_MSG_INFO, "Debouncing M2\n");
	msleep(MHI_M2_DEBOUNCE_TMR_MS);

	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_log(MHI_MSG_INFO, "Pending acks %d\n",
		atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	if (atomic_read(&mhi_dev_ctxt->counters.outbound_acks) ||
			 mhi_dev_ctxt->flags.pending_M3) {
		mhi_assert_device_wake(mhi_dev_ctxt);
	} else {
		pm_runtime_mark_last_busy(
				&mhi_dev_ctxt->dev_info->plat_dev->dev);
		r = pm_request_autosuspend(
				&mhi_dev_ctxt->dev_info->plat_dev->dev);
		if (r) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to remove counter ret %d\n", r);
			BUG_ON(mhi_dev_ctxt->dev_info->
				plat_dev->dev.power.runtime_error);
		}
	}
	atomic_set(&mhi_dev_ctxt->flags.m2_transition, 0);
	mhi_log(MHI_MSG_INFO, "M2 transition complete.\n");
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	BUG_ON(atomic_read(&mhi_dev_ctxt->outbound_acks) < 0);

	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS process_m3_transition(
		struct mhi_device_ctxt *mhi_dev_ctxt,
		enum STATE_TRANSITION cur_work_item)
{
	unsigned long flags;
	mhi_log(MHI_MSG_INFO,
			"Processing M3 state transition\n");
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->mhi_state = MHI_STATE_M3;
	mhi_dev_ctxt->flags.pending_M3 = 0;
	wake_up_interruptible(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->counters.m0_m3++;
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS mhi_process_link_down(
		struct mhi_device_ctxt *mhi_dev_ctxt)
{
	unsigned long flags;
	int r;
	mhi_log(MHI_MSG_INFO, "Entered.\n");
	if (NULL == mhi_dev_ctxt)
		return MHI_STATUS_ERROR;

	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->flags.mhi_initialized = 0;
	mhi_dev_ctxt->mhi_state = MHI_STATE_RESET;
	mhi_deassert_device_wake(mhi_dev_ctxt);
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

	mhi_dev_ctxt->flags.stop_threads = 1;

	while (!mhi_dev_ctxt->flags.ev_thread_stopped) {
		wake_up_interruptible(mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq);
		mhi_log(MHI_MSG_INFO,
			"Waiting for threads to SUSPEND EVT: %d, STT: %d\n",
			mhi_dev_ctxt->flags.st_thread_stopped,
			mhi_dev_ctxt->flags.ev_thread_stopped);
		msleep(20);
	}

	r = mhi_set_bus_request(mhi_dev_ctxt, 0);
	if (r)
		mhi_log(MHI_MSG_INFO,
			"Failed to scale bus request to sleep set.\n");
	mhi_turn_off_pcie_link(mhi_dev_ctxt);
	mhi_dev_ctxt->dev_info->link_down_cntr++;
	atomic_set(&mhi_dev_ctxt->flags.data_pending, 0);
	mhi_log(MHI_MSG_INFO, "Exited.\n");

	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS process_link_down_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	mhi_log(MHI_MSG_INFO, "Entered\n");
	if (MHI_STATUS_SUCCESS !=
			mhi_process_link_down(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to process link down\n");
	}
	mhi_log(MHI_MSG_INFO, "Exited.\n");
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS process_wake_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	int r = 0;

	mhi_log(MHI_MSG_INFO, "Entered\n");
	__pm_stay_awake(&mhi_dev_ctxt->w_lock);

	if (atomic_read(&mhi_dev_ctxt->flags.pending_ssr)) {
		mhi_log(MHI_MSG_CRITICAL,
			"Pending SSR, Ignoring.\n");
		goto exit;
	}

	if (mhi_dev_ctxt->flags.mhi_initialized) {
		r = pm_request_resume(&mhi_dev_ctxt->dev_info->plat_dev->dev);
		mhi_log(MHI_MSG_VERBOSE,
			"MHI is initialized, transitioning to M0, ret %d\n", r);
	}

	if (!mhi_dev_ctxt->flags.mhi_initialized) {
		mhi_log(MHI_MSG_INFO,
			"MHI is not initialized transitioning to base.\n");
		ret_val = init_mhi_base_state(mhi_dev_ctxt);
		if (MHI_STATUS_SUCCESS != ret_val)
			mhi_log(MHI_MSG_CRITICAL,
				"Failed to transition to base state %d.\n",
				ret_val);
	}

exit:
	__pm_relax(&mhi_dev_ctxt->w_lock);
	mhi_log(MHI_MSG_INFO, "Exited.\n");
	return ret_val;

}

static enum MHI_STATUS process_bhi_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	mhi_turn_on_pcie_link(mhi_dev_ctxt);
	mhi_log(MHI_MSG_INFO, "Entered\n");
	mhi_dev_ctxt->mhi_state = MHI_STATE_BHI;
	wake_up_interruptible(mhi_dev_ctxt->mhi_ev_wq.bhi_event);
	mhi_log(MHI_MSG_INFO, "Exited\n");
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS process_ready_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_log(MHI_MSG_INFO, "Processing READY state transition\n");
	mhi_dev_ctxt->mhi_state = MHI_STATE_READY;

	ret_val = mhi_reset_all_thread_queues(mhi_dev_ctxt);

	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_ERROR,
			"Failed to reset thread queues\n");

	/* Initialize MMIO */
	if (MHI_STATUS_SUCCESS != mhi_init_mmio(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_ERROR,
			"Failure during MMIO initialization\n");
		return MHI_STATUS_ERROR;
	}
	ret_val = mhi_add_elements_to_event_rings(mhi_dev_ctxt,
				cur_work_item);

	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_log(MHI_MSG_ERROR,
			"Failure during event ring init\n");
		return MHI_STATUS_ERROR;
	}

	mhi_dev_ctxt->flags.stop_threads = 0;
	mhi_assert_device_wake(mhi_dev_ctxt);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRL,
			MHICTRL_MHISTATE_MASK,
			MHICTRL_MHISTATE_SHIFT,
			MHI_STATE_M0);
	return MHI_STATUS_SUCCESS;
}

static void mhi_reset_chan_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt,
				int chan)
{
	struct mhi_chan_ctxt *chan_ctxt =
			&mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	struct mhi_ring *local_chan_ctxt =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	chan_ctxt->mhi_trb_read_ptr = chan_ctxt->mhi_trb_ring_base_addr;
	chan_ctxt->mhi_trb_write_ptr = chan_ctxt->mhi_trb_ring_base_addr;
	local_chan_ctxt->rp = local_chan_ctxt->base;
	local_chan_ctxt->wp = local_chan_ctxt->base;
	local_chan_ctxt->ack_rp = local_chan_ctxt->base;
}

static enum MHI_STATUS process_reset_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	u32 i = 0;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	unsigned long flags = 0;

	mhi_log(MHI_MSG_INFO, "Processing RESET state transition\n");
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->mhi_state = MHI_STATE_RESET;
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->counters.mhi_reset_cntr++;
	mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_PBL;
	ret_val = mhi_test_for_device_reset(mhi_dev_ctxt);
	ret_val = mhi_test_for_device_ready(mhi_dev_ctxt);
	switch (ret_val) {
	case MHI_STATUS_SUCCESS:
		break;
	case MHI_STATUS_LINK_DOWN:
		mhi_log(MHI_MSG_CRITICAL, "Link down detected\n");
		break;
	case MHI_STATUS_DEVICE_NOT_READY:
		ret_val = mhi_init_state_transition(mhi_dev_ctxt,
					STATE_TRANSITION_RESET);
		if (MHI_STATUS_SUCCESS != ret_val)
			mhi_log(MHI_MSG_CRITICAL,
				"Failed to initiate 0x%x state trans\n",
				STATE_TRANSITION_RESET);
		break;
	default:
		mhi_log(MHI_MSG_CRITICAL,
			"Unexpected ret code detected for\n");
		break;
	}
	for (i = 0; i < NR_OF_CMD_RINGS; ++i) {
		mhi_dev_ctxt->mhi_local_cmd_ctxt[i].rp =
				mhi_dev_ctxt->mhi_local_cmd_ctxt[i].base;
		mhi_dev_ctxt->mhi_local_cmd_ctxt[i].wp =
				mhi_dev_ctxt->mhi_local_cmd_ctxt[i].base;
		mhi_dev_ctxt->mhi_ctrl_seg->mhi_cmd_ctxt_list[i].
						mhi_cmd_ring_read_ptr =
				virt_to_dma(NULL,
				mhi_dev_ctxt->mhi_local_cmd_ctxt[i].rp);
	}
	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i)
		mhi_reset_ev_ctxt(mhi_dev_ctxt, i);

	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
		if (VALID_CHAN_NR(i))
			mhi_reset_chan_ctxt(mhi_dev_ctxt, i);
	}
	ret_val = mhi_init_state_transition(mhi_dev_ctxt,
				STATE_TRANSITION_READY);
	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_CRITICAL,
		"Failed to initiate 0x%x state trans\n",
		STATE_TRANSITION_READY);
	return ret_val;
}

static enum MHI_STATUS process_syserr_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_log(MHI_MSG_CRITICAL, "Received SYS ERROR. Resetting MHI\n");
	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_log(MHI_MSG_CRITICAL, "Failed to reset mhi\n");
		return MHI_STATUS_ERROR;
	}
	mhi_dev_ctxt->mhi_state = MHI_STATE_RESET;
	if (MHI_STATUS_SUCCESS != mhi_init_state_transition(mhi_dev_ctxt,
				STATE_TRANSITION_RESET))
		mhi_log(MHI_MSG_ERROR,
			"Failed to init state transition to RESET.\n");
	return ret_val;
}

enum MHI_STATUS start_chan_sync(struct mhi_client_handle *client_handle)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	int chan = client_handle->chan_info.chan_nr;
	int r = 0;

	init_completion(&client_handle->chan_open_complete);
	ret_val = mhi_send_cmd(client_handle->mhi_dev_ctxt,
			       MHI_COMMAND_START_CHAN,
			       chan);
	if (ret_val != MHI_STATUS_SUCCESS) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to send start command for chan %d ret %d\n",
			chan, ret_val);
		return ret_val;
	}
	r = wait_for_completion_timeout(
			&client_handle->chan_open_complete,
			msecs_to_jiffies(MHI_MAX_CMD_TIMEOUT));
	if (!r) {
		mhi_log(MHI_MSG_ERROR,
			   "Timed out waiting for chan %d start completion\n",
			    chan);
		ret_val = MHI_STATUS_ERROR;
	}
	return ret_val;
}

static void enable_clients(struct mhi_device_ctxt *mhi_dev_ctxt,
					enum MHI_EXEC_ENV exec_env)
{
	struct mhi_client_handle *client_handle = NULL;
	struct mhi_cb_info cb_info;
	int i;
	struct mhi_chan_info chan_info;
	int r;

	cb_info.cb_reason = MHI_CB_MHI_ENABLED;

	mhi_log(MHI_MSG_INFO, "Enabling Clients, exec env %d.\n", exec_env);
	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
		if (!VALID_CHAN_NR(i))
			continue;
		client_handle = mhi_dev_ctxt->client_handle_list[i];
		r = get_chan_props(mhi_dev_ctxt, i, &chan_info);
		if (!r && client_handle &&
		    exec_env == GET_CHAN_PROPS(CHAN_BRINGUP_STAGE,
						chan_info.flags))
			mhi_notify_client(client_handle, MHI_CB_MHI_ENABLED);
	}
	if (exec_env == MHI_EXEC_ENV_AMSS)
		mhi_deassert_device_wake(mhi_dev_ctxt);
	mhi_log(MHI_MSG_INFO, "Done.\n");
}

static enum MHI_STATUS process_sbl_transition(
				struct mhi_device_ctxt *mhi_dev_ctxt,
				enum STATE_TRANSITION cur_work_item)
{
	int r;
	mhi_log(MHI_MSG_INFO, "Processing SBL state transition\n");

	pm_runtime_set_autosuspend_delay(&mhi_dev_ctxt->dev_info->plat_dev->dev,
					 MHI_RPM_AUTOSUSPEND_TMR_VAL_MS);
	pm_runtime_use_autosuspend(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	r = pm_runtime_set_active(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	if (r) {
		mhi_log(MHI_MSG_ERROR,
		"Failed to activate runtime pm ret %d\n", r);
	}
	pm_runtime_enable(&mhi_dev_ctxt->dev_info->plat_dev->dev);
	mhi_log(MHI_MSG_INFO, "Enabled runtime pm\n");
	mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_SBL;
	wmb();
	enable_clients(mhi_dev_ctxt, mhi_dev_ctxt->dev_exec_env);
	return MHI_STATUS_SUCCESS;
}

static enum MHI_STATUS process_amss_transition(
				struct mhi_device_ctxt *mhi_dev_ctxt,
				enum STATE_TRANSITION cur_work_item)
{
	enum MHI_STATUS ret_val;
	struct mhi_client_handle *client_handle = NULL;
	int i = 0;

	mhi_log(MHI_MSG_INFO, "Processing AMSS state transition\n");
	mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_AMSS;
	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	mhi_assert_device_wake(mhi_dev_ctxt);
	if (0 == mhi_dev_ctxt->flags.mhi_initialized) {
		ret_val = mhi_add_elements_to_event_rings(mhi_dev_ctxt,
					cur_work_item);
		if (MHI_STATUS_SUCCESS != ret_val)
			return MHI_STATUS_ERROR;
		mhi_dev_ctxt->flags.mhi_initialized = 1;
		if (MHI_STATUS_SUCCESS != ret_val)
			mhi_log(MHI_MSG_CRITICAL,
				"Failed to set local chan state\n");
			ring_all_chan_dbs(mhi_dev_ctxt);
			mhi_log(MHI_MSG_INFO,
				"Notifying clients that MHI is enabled\n");
		if (ret_val != MHI_STATUS_SUCCESS)
			mhi_log(MHI_MSG_CRITICAL,
				"Failed to probe MHI CORE clients, ret 0x%x\n",
				ret_val);
		enable_clients(mhi_dev_ctxt, mhi_dev_ctxt->dev_exec_env);
	} else {
		mhi_log(MHI_MSG_INFO, "MHI is initialized\n");
		for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
			client_handle = mhi_dev_ctxt->client_handle_list[i];
			if (client_handle && client_handle->chan_status)
				ret_val = start_chan_sync(client_handle);
				if (ret_val)
					mhi_log(MHI_MSG_ERROR,
					"Failed to start chan %d ret %d\n",
					i, ret_val);

		}
		ring_all_chan_dbs(mhi_dev_ctxt);
	}
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	if (!mhi_dev_ctxt->flags.pending_M3 &&
	     mhi_dev_ctxt->flags.link_up)
		mhi_deassert_device_wake(mhi_dev_ctxt);
	mhi_log(MHI_MSG_INFO, "Exited\n");
	return MHI_STATUS_SUCCESS;
}

enum MHI_STATUS mhi_trigger_reset(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	enum MHI_STATUS ret_val;
	unsigned long flags = 0;

	mhi_log(MHI_MSG_INFO, "Entered\n");
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->mhi_state = MHI_STATE_SYS_ERR;
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

	mhi_log(MHI_MSG_INFO, "Setting RESET to MDM.\n");
	mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_RESET);
	mhi_log(MHI_MSG_INFO, "Transitioning state to RESET\n");
	ret_val = mhi_init_state_transition(mhi_dev_ctxt,
					    STATE_TRANSITION_RESET);
	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to initiate 0x%x state trans ret %d\n",
			STATE_TRANSITION_RESET, ret_val);
	mhi_log(MHI_MSG_INFO, "Exiting\n");
	return ret_val;
}

static enum MHI_STATUS process_stt_work_item(
			struct mhi_device_ctxt  *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;

	mhi_log(MHI_MSG_INFO, "Transitioning to %d\n",
				(int)cur_work_item);
	trace_mhi_state(cur_work_item);
	switch (cur_work_item) {
	case STATE_TRANSITION_BHI:
		ret_val = process_bhi_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_RESET:
		ret_val = process_reset_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_READY:
		ret_val = process_ready_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_SBL:
		ret_val = process_sbl_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_AMSS:
		ret_val = process_amss_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_M0:
		ret_val = process_m0_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_M1:
		ret_val = process_m1_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_M3:
		ret_val = process_m3_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_SYS_ERR:
		ret_val = process_syserr_transition(mhi_dev_ctxt,
						   cur_work_item);
		break;
	case STATE_TRANSITION_LINK_DOWN:
		ret_val = process_link_down_transition(mhi_dev_ctxt,
							cur_work_item);
		break;
	case STATE_TRANSITION_WAKE:
		ret_val = process_wake_transition(mhi_dev_ctxt, cur_work_item);
		break;
	default:
		mhi_log(MHI_MSG_ERROR,
				"Unrecongized state: %d\n", cur_work_item);
		break;
	}
	return ret_val;
}

int mhi_state_change_thread(void *ctxt)
{
	int r = 0;
	unsigned long flags = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = (struct mhi_device_ctxt *)ctxt;
	enum STATE_TRANSITION cur_work_item;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	struct mhi_state_work_queue *work_q =
			&mhi_dev_ctxt->state_change_work_item_list;
	struct mhi_ring *state_change_q = &work_q->q_info;

	if (NULL == mhi_dev_ctxt) {
		mhi_log(MHI_MSG_ERROR, "Got bad context, quitting\n");
		return -EIO;
	}
	for (;;) {
		r = wait_event_interruptible(
				*mhi_dev_ctxt->mhi_ev_wq.state_change_event,
				((work_q->q_info.rp != work_q->q_info.wp) &&
				 !mhi_dev_ctxt->flags.st_thread_stopped));
		if (r) {
			mhi_log(MHI_MSG_INFO,
				"Caught signal %d, quitting\n", r);
			return 0;
		}

		if (mhi_dev_ctxt->flags.kill_threads) {
			mhi_log(MHI_MSG_INFO,
				"Caught exit signal, quitting\n");
			return 0;
		}
		mhi_dev_ctxt->flags.st_thread_stopped = 0;
		spin_lock_irqsave(work_q->q_lock, flags);
		cur_work_item = *(enum STATE_TRANSITION *)(state_change_q->rp);
		ret_val = ctxt_del_element(&work_q->q_info, NULL);
		MHI_ASSERT(ret_val == MHI_STATUS_SUCCESS,
			"Failed to delete element from STT workqueue\n");
		spin_unlock_irqrestore(work_q->q_lock, flags);
		ret_val = process_stt_work_item(mhi_dev_ctxt, cur_work_item);
	}
	return 0;
}

/**
 * mhi_init_state_transition - Add a new state transition work item to
 *			the state transition thread work item list.
 *
 * @mhi_dev_ctxt	The mhi_dev_ctxt context
 * @new_state		The state we wish to transition to
 *
 */
enum MHI_STATUS mhi_init_state_transition(struct mhi_device_ctxt *mhi_dev_ctxt,
		enum STATE_TRANSITION new_state)
{
	unsigned long flags = 0;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	enum STATE_TRANSITION *cur_work_item = NULL;
	s32 nr_avail_work_items = 0;
	struct mhi_ring *stt_ring =
		&mhi_dev_ctxt->state_change_work_item_list.q_info;
	struct mhi_state_work_queue *work_q =
			&mhi_dev_ctxt->state_change_work_item_list;

	spin_lock_irqsave(work_q->q_lock, flags);
	nr_avail_work_items = get_nr_avail_ring_elements(stt_ring);

	if (0 >= nr_avail_work_items) {
		mhi_log(MHI_MSG_CRITICAL, "No Room left on STT work queue\n");
		return MHI_STATUS_ERROR;
	}
	mhi_log(MHI_MSG_VERBOSE,
		"Processing state transition %x\n",
		new_state);
	*(enum STATE_TRANSITION *)stt_ring->wp = new_state;
	ret_val = ctxt_add_element(stt_ring, (void **)&cur_work_item);
	wmb();
	MHI_ASSERT(MHI_STATUS_SUCCESS == ret_val,
			"Failed to add selement to STT workqueue\n");
	spin_unlock_irqrestore(work_q->q_lock, flags);
	wake_up_interruptible(mhi_dev_ctxt->mhi_ev_wq.state_change_event);
	return ret_val;
}

int mhi_initiate_m0(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;
	unsigned long flags;

	mhi_log(MHI_MSG_INFO,
		"Entered MHI state %d, Pending M0 %d Pending M3 %d\n",
		mhi_dev_ctxt->mhi_state, mhi_dev_ctxt->flags.pending_M0,
					mhi_dev_ctxt->flags.pending_M3);
	mutex_lock(&mhi_dev_ctxt->pm_lock);
	mhi_log(MHI_MSG_INFO,
		"Waiting for M0 M1 or M3. Currently %d...\n",
					mhi_dev_ctxt->mhi_state);

	r = wait_event_interruptible_timeout(*mhi_dev_ctxt->mhi_ev_wq.m3_event,
			mhi_dev_ctxt->mhi_state == MHI_STATE_M3 ||
			mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
			mhi_dev_ctxt->mhi_state == MHI_STATE_M1,
		msecs_to_jiffies(MHI_MAX_SUSPEND_TIMEOUT));
	switch (r) {
	case 0:
		mhi_log(MHI_MSG_CRITICAL,
			"Timeout: State %d after %d ms\n",
				mhi_dev_ctxt->mhi_state,
				MHI_MAX_SUSPEND_TIMEOUT);
		mhi_dev_ctxt->counters.m0_event_timeouts++;
		r = -ETIME;
		goto exit;
		break;
	case -ERESTARTSYS:
		mhi_log(MHI_MSG_CRITICAL,
			"Going Down...\n");
		goto exit;
		break;
	default:
		mhi_log(MHI_MSG_INFO,
			"Wait complete state: %d\n", mhi_dev_ctxt->mhi_state);
		r = 0;
		break;
	}
	if (mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
	    mhi_dev_ctxt->mhi_state == MHI_STATE_M1) {
		mhi_assert_device_wake(mhi_dev_ctxt);
		mhi_log(MHI_MSG_INFO,
				"MHI state %d, done\n",
					mhi_dev_ctxt->mhi_state);
		goto exit;
	} else {
		if (MHI_STATUS_SUCCESS != mhi_turn_on_pcie_link(mhi_dev_ctxt)) {
			mhi_log(MHI_MSG_CRITICAL,
					"Failed to resume link\n");
			r = -EIO;
			goto exit;
		}

		write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
		mhi_log(MHI_MSG_VERBOSE, "Setting M0 ...\n");
		if (mhi_dev_ctxt->flags.pending_M3) {
			mhi_log(MHI_MSG_INFO,
				"Pending M3 detected, aborting M0 procedure\n");
			write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock,
								flags);
			r = -EPERM;
			goto exit;
		}
		if (mhi_dev_ctxt->flags.link_up) {
			mhi_dev_ctxt->flags.pending_M0 = 1;
			mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_M0);
		}
		write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
		r = wait_event_interruptible_timeout(
				*mhi_dev_ctxt->mhi_ev_wq.m0_event,
				mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
				mhi_dev_ctxt->mhi_state == MHI_STATE_M1,
				msecs_to_jiffies(MHI_MAX_RESUME_TIMEOUT));
		WARN_ON(!r || -ERESTARTSYS == r);
		if (!r || -ERESTARTSYS == r)
			mhi_log(MHI_MSG_ERROR,
				"Failed to get M0 event ret %d\n", r);
		r = 0;
	}
exit:
	mutex_unlock(&mhi_dev_ctxt->pm_lock);
	mhi_log(MHI_MSG_INFO, "Exited...\n");
	return r;
}

int mhi_initiate_m3(struct mhi_device_ctxt *mhi_dev_ctxt)
{

	unsigned long flags;
	int r = 0;
	int abort_m3 = 0;

	mhi_log(MHI_MSG_INFO,
		"Entered MHI state %d, Pending M0 %d Pending M3 %d\n",
		mhi_dev_ctxt->mhi_state, mhi_dev_ctxt->flags.pending_M0,
					mhi_dev_ctxt->flags.pending_M3);
	mutex_lock(&mhi_dev_ctxt->pm_lock);
	switch (mhi_dev_ctxt->mhi_state) {
	case MHI_STATE_RESET:
		mhi_log(MHI_MSG_INFO,
				"MHI in RESET turning link off and quitting\n");
		mhi_turn_off_pcie_link(mhi_dev_ctxt);
		r = mhi_set_bus_request(mhi_dev_ctxt, 0);
		if (r)
			mhi_log(MHI_MSG_INFO,
					"Failed to set bus freq ret %d\n", r);
		goto exit;
		break;
	case MHI_STATE_M0:
	case MHI_STATE_M1:
	case MHI_STATE_M2:
		mhi_log(MHI_MSG_INFO,
			"Triggering wake out of M2\n");
		write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
		mhi_dev_ctxt->flags.pending_M3 = 1;
		if ((atomic_read(&mhi_dev_ctxt->flags.m2_transition)) == 0) {
			mhi_log(MHI_MSG_INFO,
				"M2 transition not set\n");
			mhi_assert_device_wake(mhi_dev_ctxt);
		}
		write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
		r = wait_event_interruptible_timeout(
				*mhi_dev_ctxt->mhi_ev_wq.m0_event,
				mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
				mhi_dev_ctxt->mhi_state == MHI_STATE_M1,
				msecs_to_jiffies(MHI_MAX_RESUME_TIMEOUT));
		if (0 == r || -ERESTARTSYS == r) {
			mhi_log(MHI_MSG_INFO,
				"MDM failed to come out of M2.\n");
			r = -EAGAIN;
			goto exit;
		}
		break;
	case MHI_STATE_M3:
		mhi_log(MHI_MSG_INFO,
			"MHI state %d, link state %d.\n",
				mhi_dev_ctxt->mhi_state,
				mhi_dev_ctxt->flags.link_up);
		if (mhi_dev_ctxt->flags.link_up)
			r = -EAGAIN;
		else
			r = 0;
		goto exit;
	default:
		mhi_log(MHI_MSG_INFO,
			"MHI state %d, link state %d.\n",
				mhi_dev_ctxt->mhi_state,
				mhi_dev_ctxt->flags.link_up);
		break;
	}
	while (atomic_read(&mhi_dev_ctxt->counters.outbound_acks)) {
		mhi_log(MHI_MSG_INFO,
			"There are still %d acks pending from device\n",
			atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
			__pm_stay_awake(&mhi_dev_ctxt->w_lock);
			__pm_relax(&mhi_dev_ctxt->w_lock);
		abort_m3 = 1;
		r = -EAGAIN;
		goto exit;
	}

	if (atomic_read(&mhi_dev_ctxt->flags.data_pending)) {
		abort_m3 = 1;
		r = -EAGAIN;
		goto exit;
	}
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	if (mhi_dev_ctxt->flags.pending_M0) {
		write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
		r = -EAGAIN;
		goto exit;
	}
	mhi_dev_ctxt->flags.pending_M3 = 1;

	mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_M3);
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);

	mhi_log(MHI_MSG_INFO,
			"Waiting for M3 completion.\n");
	r = wait_event_interruptible_timeout(*mhi_dev_ctxt->mhi_ev_wq.m3_event,
			mhi_dev_ctxt->mhi_state == MHI_STATE_M3,
		msecs_to_jiffies(MHI_MAX_SUSPEND_TIMEOUT));
	switch (r) {
	case 0:
		mhi_log(MHI_MSG_CRITICAL,
			"MDM failed to suspend after %d ms\n",
			MHI_MAX_SUSPEND_TIMEOUT);
		mhi_dev_ctxt->counters.m3_event_timeouts++;
		mhi_dev_ctxt->flags.pending_M3 = 0;
		goto exit;
		break;

	default:
		mhi_log(MHI_MSG_INFO,
			"M3 completion received\n");
		break;
	}
	mhi_turn_off_pcie_link(mhi_dev_ctxt);
	r = mhi_set_bus_request(mhi_dev_ctxt, 0);
	if (r)
		mhi_log(MHI_MSG_INFO, "Failed to set bus freq ret %d\n", r);
exit:
	if (abort_m3) {
		write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
		atomic_inc(&mhi_dev_ctxt->flags.data_pending);
		write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
		ring_all_chan_dbs(mhi_dev_ctxt);
		ring_all_cmd_dbs(mhi_dev_ctxt);
		atomic_dec(&mhi_dev_ctxt->flags.data_pending);
		mhi_deassert_device_wake(mhi_dev_ctxt);
	}
	mhi_dev_ctxt->flags.pending_M3 = 0;
	mutex_unlock(&mhi_dev_ctxt->pm_lock);
	return r;
}
