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

#include <linux/pm_runtime.h>
#include <mhi_sys.h>
#include <mhi.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/esoc_client.h>

static int mhi_ssr_notify_cb(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct mhi_device_ctxt *mhi_dev_ctxt =
		container_of(nb, struct mhi_device_ctxt, mhi_ssr_nb);
	enum MHI_PM_STATE cur_state;
	struct notif_data *notif_data = (struct notif_data *)data;
	bool crashed = notif_data->crashed;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Received ESOC notifcation:%lu crashed:%d\n", action, crashed);
	switch (action) {
	case SUBSYS_AFTER_SHUTDOWN:

		/* Disable internal state, no more communication */
		write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		cur_state = mhi_tryset_pm_state(mhi_dev_ctxt,
						MHI_PM_LD_ERR_FATAL_DETECT);
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		if (unlikely(cur_state != MHI_PM_LD_ERR_FATAL_DETECT))
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Failed to transition to state 0x%x from 0x%x\n",
				MHI_PM_LD_ERR_FATAL_DETECT, cur_state);
		if (mhi_dev_ctxt->mhi_pm_state != MHI_PM_DISABLE)
			process_disable_transition(MHI_PM_SHUTDOWN_PROCESS,
						   mhi_dev_ctxt);
		mutex_lock(&mhi_dev_ctxt->pm_lock);
		write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		cur_state = mhi_tryset_pm_state(mhi_dev_ctxt,
						MHI_PM_SSR_PENDING);
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		mutex_unlock(&mhi_dev_ctxt->pm_lock);
		if (unlikely(cur_state != MHI_PM_SSR_PENDING))
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Failed to transition to state 0x%x from 0x%x\n",
				MHI_PM_SSR_PENDING, cur_state);
		break;
	default:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Not handling esoc notification:%lu\n", action);
		break;
	}
	return NOTIFY_OK;
}

int mhi_esoc_register(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val = 0;
	struct device_node *np;
	struct device *dev = &mhi_dev_ctxt->pcie_device->dev;

	np = dev->of_node;
	mhi_dev_ctxt->esoc_handle = devm_register_esoc_client(dev, "mdm");
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Of table of pcie struct device property is dev->of_node %p\n",
		np);
	if (IS_ERR_OR_NULL(mhi_dev_ctxt->esoc_handle)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to register for SSR, ret %lx\n",
			(uintptr_t)mhi_dev_ctxt->esoc_handle);
		return -EIO;
	}
	mhi_dev_ctxt->mhi_ssr_nb.notifier_call = mhi_ssr_notify_cb;
	mhi_dev_ctxt->esoc_ssr_handle = subsys_notif_register_notifier(
					mhi_dev_ctxt->esoc_handle->name,
					&mhi_dev_ctxt->mhi_ssr_nb);
	if (IS_ERR_OR_NULL(mhi_dev_ctxt->esoc_ssr_handle)) {
		ret_val = PTR_RET(mhi_dev_ctxt->esoc_ssr_handle);
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Can't find esoc desc ret 0x%lx\n",
			(uintptr_t)mhi_dev_ctxt->esoc_ssr_handle);
	}

	return ret_val;
}

/* handles sys_err, and shutdown transition */
void process_disable_transition(enum MHI_PM_STATE transition_state,
				struct mhi_device_ctxt *mhi_dev_ctxt)
{
	enum MHI_PM_STATE cur_state, prev_state;
	struct mhi_client_handle *client_handle;
	struct mhi_ring *ch_ring, *bb_ring, *cmd_ring;
	struct mhi_cmd_ctxt *cmd_ctxt;
	struct mhi_chan_cfg *chan_cfg;
	rwlock_t *pm_xfer_lock = &mhi_dev_ctxt->pm_xfer_lock;
	enum MHI_CB_REASON reason;
	u32 timeout = mhi_dev_ctxt->poll_reset_timeout_ms;
	int i;
	int ret;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Enter with pm_state:0x%x MHI_STATE:%s transition_state:0x%x\n",
		mhi_dev_ctxt->mhi_pm_state,
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state),
		transition_state);

	mutex_lock(&mhi_dev_ctxt->pm_lock);
	write_lock_irq(pm_xfer_lock);
	prev_state = mhi_dev_ctxt->mhi_pm_state;
	cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, transition_state);
	if (cur_state == transition_state) {
		mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_DISABLE_TRANSITION;
		mhi_dev_ctxt->flags.mhi_initialized = false;
	}
	write_unlock_irq(pm_xfer_lock);

	/* Not handling sys_err, could be middle of shut down */
	if (unlikely(cur_state != transition_state)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Failed to transition to state 0x%x from 0x%x\n",
			transition_state, cur_state);
		mutex_unlock(&mhi_dev_ctxt->pm_lock);
		return;
	}

	/*
	 * If we're shutting down trigger device into MHI reset
	 * so we can gurantee device will not access host DDR
	 * during reset
	 */
	if (cur_state == MHI_PM_SHUTDOWN_PROCESS &&
	    MHI_REG_ACCESS_VALID(prev_state)) {
		read_lock_bh(pm_xfer_lock);
		mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_RESET);
		read_unlock_bh(pm_xfer_lock);
		mhi_test_for_device_reset(mhi_dev_ctxt);
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Waiting for all pending event ring processing to complete\n");
	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; i++) {
		tasklet_kill(&mhi_dev_ctxt->mhi_local_event_ctxt[i].ev_task);
		flush_work(&mhi_dev_ctxt->mhi_local_event_ctxt[i].ev_worker);
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Notifying all clients and resetting channels\n");

	if (cur_state == MHI_PM_SHUTDOWN_PROCESS)
		reason = MHI_CB_MHI_SHUTDOWN;
	else
		reason = MHI_CB_SYS_ERROR;
	ch_ring = mhi_dev_ctxt->mhi_local_chan_ctxt;
	chan_cfg = mhi_dev_ctxt->mhi_chan_cfg;
	bb_ring = mhi_dev_ctxt->chan_bb_list;
	for (i = 0; i < MHI_MAX_CHANNELS;
	     i++, ch_ring++, chan_cfg++, bb_ring++) {
		enum MHI_CHAN_STATE ch_state;

		client_handle = mhi_dev_ctxt->client_handle_list[i];
		if (client_handle)
			mhi_notify_client(client_handle, reason);

		mutex_lock(&chan_cfg->chan_lock);
		spin_lock_irq(&ch_ring->ring_lock);
		ch_state = ch_ring->ch_state;
		ch_ring->ch_state = MHI_CHAN_STATE_DISABLED;
		spin_unlock_irq(&ch_ring->ring_lock);

		/* Reset channel and free ring */
		if (ch_state == MHI_CHAN_STATE_ENABLED) {
			mhi_reset_chan(mhi_dev_ctxt, i);
			free_tre_ring(mhi_dev_ctxt, i);
			bb_ring->rp = bb_ring->base;
			bb_ring->wp = bb_ring->base;
			bb_ring->ack_rp = bb_ring->base;
		}
		mutex_unlock(&chan_cfg->chan_lock);
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Finished notifying clients\n");

	/* Release lock and wait for all pending threads to complete */
	mutex_unlock(&mhi_dev_ctxt->pm_lock);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Waiting for all pending threads to complete\n");
	complete(&mhi_dev_ctxt->cmd_complete);
	flush_work(&mhi_dev_ctxt->process_m1_worker);
	flush_work(&mhi_dev_ctxt->st_thread_worker);
	if (mhi_dev_ctxt->bhi_ctxt.manage_boot)
		flush_work(&mhi_dev_ctxt->bhi_ctxt.fw_load_work);
	if (cur_state == MHI_PM_SHUTDOWN_PROCESS)
		flush_work(&mhi_dev_ctxt->process_sys_err_worker);

	mutex_lock(&mhi_dev_ctxt->pm_lock);

	/*
	 * Shutdown has higher priority than sys_err and can be called
	 * middle of sys error, check current state to confirm state
	 * was not changed.
	 */
	if (mhi_dev_ctxt->mhi_pm_state != cur_state) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"PM State transitioned to 0x%x while processing 0x%x\n",
			mhi_dev_ctxt->mhi_pm_state, transition_state);
		mutex_unlock(&mhi_dev_ctxt->pm_lock);
		return;
	}

	/* Check all counts to make sure 0 */
	WARN_ON(atomic_read(&mhi_dev_ctxt->counters.device_wake));
	WARN_ON(atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	if (mhi_dev_ctxt->core.pci_master)
		WARN_ON(atomic_read(&mhi_dev_ctxt->pcie_device->dev.
				   power.usage_count));

	/* Reset Event rings and CMD rings  */
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Resetting ev ctxt and cmd ctxt\n");

	cmd_ring = mhi_dev_ctxt->mhi_local_cmd_ctxt;
	cmd_ctxt = mhi_dev_ctxt->dev_space.ring_ctxt.cmd_ctxt;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, cmd_ring++) {
		cmd_ring->rp = cmd_ring->base;
		cmd_ring->wp = cmd_ring->base;
		cmd_ctxt->mhi_cmd_ring_read_ptr =
			cmd_ctxt->mhi_cmd_ring_base_addr;
		cmd_ctxt->mhi_cmd_ring_write_ptr =
			cmd_ctxt->mhi_cmd_ring_base_addr;
	}
	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; i++)
		mhi_reset_ev_ctxt(mhi_dev_ctxt, i);

	/*
	 * If we're the bus master disable runtime suspend
	 * we will enable it back again during AMSS transition
	 */
	if (mhi_dev_ctxt->core.pci_master)
		pm_runtime_forbid(&mhi_dev_ctxt->pcie_device->dev);

	if (cur_state == MHI_PM_SYS_ERR_PROCESS) {
		bool trigger_reset = false;

		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Triggering device reset\n");
		reinit_completion(&mhi_dev_ctxt->cmd_complete);
		write_lock_irq(pm_xfer_lock);
		/* Link can go down while processing SYS_ERR */
		if (MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
			mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_RESET);
			mhi_init_state_transition(mhi_dev_ctxt,
						  STATE_TRANSITION_RESET);
			trigger_reset = true;
		}
		write_unlock_irq(pm_xfer_lock);

		if (trigger_reset) {
			/*
			 * Keep the MHI state in Active (M0) state until host
			 * enter AMSS/RDDM state.  Otherwise modem would error
			 * fatal if host try to enter M1 before reaching
			 * AMSS\RDDM state.
			 */
			read_lock_bh(pm_xfer_lock);
			mhi_assert_device_wake(mhi_dev_ctxt, false);
			read_unlock_bh(pm_xfer_lock);

			/* Wait till we enter AMSS/RDDM Exec env.*/
			ret = wait_for_completion_timeout
				(&mhi_dev_ctxt->cmd_complete,
				 msecs_to_jiffies(timeout));
			if (!ret || (mhi_dev_ctxt->dev_exec_env !=
				     MHI_EXEC_ENV_AMSS &&
				     mhi_dev_ctxt->dev_exec_env !=
				     MHI_EXEC_ENV_RDDM)) {

				/*
				 * device did not reset properly, notify bus
				 * master
				 */
				if (!mhi_dev_ctxt->core.pci_master) {
					mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
						"Notifying bus master Sys Error Status\n");
					mhi_dev_ctxt->status_cb(
						MHI_CB_SYS_ERROR,
						mhi_dev_ctxt->priv_data);
				}
				mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
			}
		}
	} else {
		write_lock_irq(pm_xfer_lock);
		cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, MHI_PM_DISABLE);
		write_unlock_irq(pm_xfer_lock);
		if (unlikely(cur_state != MHI_PM_DISABLE))
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Error transition from state:0x%x to 0x%x\n",
				cur_state, MHI_PM_DISABLE);

		if (mhi_dev_ctxt->core.pci_master &&
		    cur_state == MHI_PM_DISABLE)
			mhi_turn_off_pcie_link(mhi_dev_ctxt,
					MHI_REG_ACCESS_VALID(prev_state));
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Exit with pm_state:0x%x exec_env:0x%x mhi_state:%s\n",
		mhi_dev_ctxt->mhi_pm_state, mhi_dev_ctxt->dev_exec_env,
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));

	mutex_unlock(&mhi_dev_ctxt->pm_lock);
}

void mhi_sys_err_worker(struct work_struct *work)
{
	struct mhi_device_ctxt *mhi_dev_ctxt =
		container_of(work, struct mhi_device_ctxt,
			     process_sys_err_worker);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Enter with pm_state:0x%x MHI_STATE:%s\n",
		mhi_dev_ctxt->mhi_pm_state,
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));

	process_disable_transition(MHI_PM_SYS_ERR_PROCESS, mhi_dev_ctxt);
}
