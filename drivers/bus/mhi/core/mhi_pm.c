// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved. */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/mhi.h>
#include "mhi_internal.h"

static void mhi_special_events_pending(struct mhi_controller *mhi_cntrl);

/*
 * Not all MHI states transitions are sync transitions. Linkdown, SSR, and
 * shutdown can happen anytime asynchronously. This function will transition to
 * new state only if we're allowed to transitions.
 *
 * Priority increase as we go down, example while in any states from L0, start
 * state from L1, L2, or L3 can be set.  Notable exception to this rule is state
 * DISABLE.  From DISABLE state we can transition to only POR or state.  Also
 * for example while in L2 state, user cannot jump back to L1 or L0 states.
 * Valid transitions:
 * L0: DISABLE <--> POR
 *     POR <--> POR
 *     POR -> M0 -> M2 --> M0
 *     POR -> FW_DL_ERR
 *     FW_DL_ERR <--> FW_DL_ERR
 *     M0 <--> M0
 *     M0 -> FW_DL_ERR
 *     M0 -> M3_ENTER -> M3 -> M3_EXIT --> M0
 * L1: DEVICE_ERR_DETECT -> SYS_ERR_DETECT
 *     SYS_ERR_DETECT -> SYS_ERR_PROCESS --> POR
 * L2: SHUTDOWN_PROCESS -> LD_ERR_FATAL_DETECT
 *     SHUTDOWN_PROCESS -> DISABLE
 * L3: LD_ERR_FATAL_DETECT <--> LD_ERR_FATAL_DETECT
 *     LD_ERR_FATAL_DETECT -> SHUTDOWN_NO_ACCESS
 *     SHUTDOWN_NO_ACCESS -> DISABLE
 */
static struct mhi_pm_transitions const mhi_state_transitions[] = {
	/* L0 States */
	{
		MHI_PM_DISABLE,
		MHI_PM_POR
	},
	{
		MHI_PM_POR,
		MHI_PM_POR | MHI_PM_DISABLE | MHI_PM_M0 |
		MHI_PM_DEVICE_ERR_DETECT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_FW_DL_ERR | MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_M0,
		MHI_PM_M0 | MHI_PM_M2 | MHI_PM_M3_ENTER |
		MHI_PM_DEVICE_ERR_DETECT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_FW_DL_ERR | MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_M2,
		MHI_PM_M0 | MHI_PM_DEVICE_ERR_DETECT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_M3_ENTER,
		MHI_PM_M3 | MHI_PM_DEVICE_ERR_DETECT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_M3,
		MHI_PM_M3_EXIT | MHI_PM_DEVICE_ERR_DETECT |
		MHI_PM_SYS_ERR_DETECT | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_M3_EXIT,
		MHI_PM_M0 | MHI_PM_DEVICE_ERR_DETECT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_FW_DL_ERR,
		MHI_PM_FW_DL_ERR | MHI_PM_DEVICE_ERR_DETECT |
		MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_SHUTDOWN_NO_ACCESS
	},
	/* L1 States */
	{
		MHI_PM_DEVICE_ERR_DETECT,
		MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_SYS_ERR_DETECT,
		MHI_PM_DEVICE_ERR_DETECT | MHI_PM_SYS_ERR_PROCESS |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT |
		MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_SYS_ERR_PROCESS,
		MHI_PM_POR | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_SHUTDOWN_NO_ACCESS
	},
	/* L2 States */
	{
		MHI_PM_SHUTDOWN_PROCESS,
		MHI_PM_DISABLE | MHI_PM_LD_ERR_FATAL_DETECT
	},
	/* L3 States */
	{
		MHI_PM_LD_ERR_FATAL_DETECT,
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_SHUTDOWN_NO_ACCESS
	},
	{
		MHI_PM_SHUTDOWN_NO_ACCESS,
		MHI_PM_DISABLE
	},
};

enum MHI_PM_STATE __must_check mhi_tryset_pm_state(
				struct mhi_controller *mhi_cntrl,
				enum MHI_PM_STATE state)
{
	unsigned long cur_state = mhi_cntrl->pm_state;
	int index = find_last_bit(&cur_state, 32);

	if (unlikely(index >= ARRAY_SIZE(mhi_state_transitions))) {
		MHI_CRITICAL("cur_state:%s is not a valid pm_state\n",
			     to_mhi_pm_state_str(cur_state));
		return cur_state;
	}

	if (unlikely(mhi_state_transitions[index].from_state != cur_state)) {
		MHI_ERR("index:%u cur_state:%s != actual_state: %s\n",
			index, to_mhi_pm_state_str(cur_state),
			to_mhi_pm_state_str
			(mhi_state_transitions[index].from_state));
		return cur_state;
	}

	if (unlikely(!(mhi_state_transitions[index].to_states & state))) {
		MHI_LOG(
			"Not allowing pm state transition from:%s to:%s state\n",
			to_mhi_pm_state_str(cur_state),
			to_mhi_pm_state_str(state));
		return cur_state;
	}

	MHI_VERB("Transition to pm state from:%s to:%s\n",
		 to_mhi_pm_state_str(cur_state), to_mhi_pm_state_str(state));

	mhi_cntrl->pm_state = state;
	return mhi_cntrl->pm_state;
}

void mhi_set_mhi_state(struct mhi_controller *mhi_cntrl,
		       enum mhi_dev_state state)
{
	if (state == MHI_STATE_RESET) {
		mhi_write_reg_field(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
				    MHICTRL_RESET_MASK, MHICTRL_RESET_SHIFT, 1);
		mhi_cntrl->initiate_mhi_reset = true;
	} else {
		mhi_cntrl->write_reg(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
				(state << MHICTRL_MHISTATE_SHIFT));
	}
}

/* nop for backward compatibility, allowed to ring db registers in M2 state */
static void mhi_toggle_dev_wake_nop(struct mhi_controller *mhi_cntrl)
{
}

static void mhi_toggle_dev_wake(struct mhi_controller *mhi_cntrl)
{
	mhi_cntrl->wake_get(mhi_cntrl, false);
	mhi_cntrl->wake_put(mhi_cntrl, true);
}

/* set device wake */
void mhi_assert_dev_wake(struct mhi_controller *mhi_cntrl, bool force)
{
	unsigned long flags;

	/* if set, regardless of count set the bit if not set */
	if (unlikely(force)) {
		spin_lock_irqsave(&mhi_cntrl->wlock, flags);
		atomic_inc(&mhi_cntrl->dev_wake);
		if (MHI_WAKE_DB_FORCE_SET_VALID(mhi_cntrl->pm_state) &&
		    !mhi_cntrl->wake_set) {
			mhi_write_db(mhi_cntrl, mhi_cntrl->wake_db, 1);
			mhi_cntrl->wake_set = true;
		}
		spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
	} else {
		/* if resources requested already, then increment and exit */
		spin_lock_irqsave(&mhi_cntrl->wlock, flags);
		if (likely(atomic_add_unless(&mhi_cntrl->dev_wake, 1, 0))) {
			spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
			return;
		}

		if ((atomic_inc_return(&mhi_cntrl->dev_wake) == 1) &&
		    MHI_WAKE_DB_SET_VALID(mhi_cntrl->pm_state) &&
		    !mhi_cntrl->wake_set) {
			mhi_write_db(mhi_cntrl, mhi_cntrl->wake_db, 1);
			mhi_cntrl->wake_set = true;
		}
		spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
	}
}

/* clear device wake */
void mhi_deassert_dev_wake(struct mhi_controller *mhi_cntrl, bool override)
{
	unsigned long flags;

	MHI_ASSERT((mhi_is_active(mhi_cntrl->mhi_dev) &&
		   atomic_read(&mhi_cntrl->dev_wake) == 0), "dev_wake == 0");

	/* resources not dropping to 0, decrement and exit */
	spin_lock_irqsave(&mhi_cntrl->wlock, flags);
	if (likely(atomic_add_unless(&mhi_cntrl->dev_wake, -1, 1))) {
		if (!override)
			mhi_cntrl->ignore_override = true;
		spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
		return;
	}

	if ((atomic_dec_return(&mhi_cntrl->dev_wake) == 0) &&
	    MHI_WAKE_DB_CLEAR_VALID(mhi_cntrl->pm_state) && (!override ||
	    mhi_cntrl->ignore_override) && mhi_cntrl->wake_set) {
		mhi_write_db(mhi_cntrl, mhi_cntrl->wake_db, 0);
		mhi_cntrl->wake_set = false;
		mhi_cntrl->ignore_override = false;
	}
	spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
}

int mhi_ready_state_transition(struct mhi_controller *mhi_cntrl)
{
	void __iomem *base = mhi_cntrl->regs;
	u32 reset = 1, ready = 0;
	struct mhi_event *mhi_event;
	enum MHI_PM_STATE cur_state;
	int ret = -EIO, i;

	MHI_CNTRL_LOG("Waiting to enter READY state\n");

	/* wait for RESET to be cleared and READY bit to be set */
	wait_event_timeout(mhi_cntrl->state_event,
			   MHI_PM_IN_FATAL_STATE(mhi_cntrl->pm_state) ||
			   mhi_read_reg_field(mhi_cntrl, base, MHICTRL,
					      MHICTRL_RESET_MASK,
					      MHICTRL_RESET_SHIFT, &reset) ||
			   mhi_read_reg_field(mhi_cntrl, base, MHISTATUS,
					      MHISTATUS_READY_MASK,
					      MHISTATUS_READY_SHIFT, &ready) ||
			   (!reset && ready),
			   msecs_to_jiffies(mhi_cntrl->timeout_ms));

	/* device enter into error state */
	if (MHI_PM_IN_FATAL_STATE(mhi_cntrl->pm_state))
		goto error_ready;

	/* device did not transition to ready state */
	if (reset || !ready) {
		ret = -ETIMEDOUT;
		goto error_ready;
	}

	MHI_CNTRL_LOG("Device in READY State\n");
	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_POR);
	mhi_cntrl->dev_state = MHI_STATE_READY;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	if (cur_state != MHI_PM_POR) {
		MHI_CNTRL_ERR("Error moving to state %s from %s\n",
				to_mhi_pm_state_str(MHI_PM_POR),
				to_mhi_pm_state_str(cur_state));
		goto error_ready;
	}
	read_lock_bh(&mhi_cntrl->pm_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		goto error_mmio;

	ret = mhi_init_mmio(mhi_cntrl);
	if (ret) {
		MHI_CNTRL_ERR("Error programming mmio registers\n");
		ret = -EIO;
		goto error_mmio;
	}

	/* add elements to all sw event rings */
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		if (mhi_event->offload_ev || mhi_event->hw_ring)
			continue;

		ring->wp = ring->base + ring->len - ring->el_size;
		*ring->ctxt_wp = ring->iommu_base + ring->len - ring->el_size;
		/* needs to update to all cores */
		smp_wmb();

		/* ring the db for event rings */
		spin_lock_irq(&mhi_event->lock);
		mhi_ring_er_db(mhi_event);
		spin_unlock_irq(&mhi_event->lock);
	}

	/* set device into M0 state */
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M0);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	return 0;

error_mmio:
	read_unlock_bh(&mhi_cntrl->pm_lock);

error_ready:
	mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data,
			     MHI_CB_BOOTUP_TIMEOUT);

	return ret;
}

int mhi_pm_m0_transition(struct mhi_controller *mhi_cntrl)
{
	enum MHI_PM_STATE cur_state;
	struct mhi_chan *mhi_chan;
	int i;

	MHI_LOG("Entered With State:%s PM_STATE:%s\n",
		TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		to_mhi_pm_state_str(mhi_cntrl->pm_state));

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->dev_state = MHI_STATE_M0;
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M0);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	if (unlikely(cur_state != MHI_PM_M0)) {
		MHI_ERR("Failed to transition to state %s from %s\n",
			to_mhi_pm_state_str(MHI_PM_M0),
			to_mhi_pm_state_str(cur_state));
		return -EIO;
	}
	mhi_cntrl->M0++;
	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_get(mhi_cntrl, true);

	/* ring all event rings and CMD ring only if we're in mission mode */
	if (MHI_IN_MISSION_MODE(mhi_cntrl->ee)) {
		struct mhi_event *mhi_event = mhi_cntrl->mhi_event;
		struct mhi_cmd *mhi_cmd =
			&mhi_cntrl->mhi_cmd[PRIMARY_CMD_RING];

		for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
			if (mhi_event->offload_ev)
				continue;

			spin_lock_irq(&mhi_event->lock);
			mhi_ring_er_db(mhi_event);
			spin_unlock_irq(&mhi_event->lock);
		}

		/* only ring primary cmd ring */
		spin_lock_irq(&mhi_cmd->lock);
		if (mhi_cmd->ring.rp != mhi_cmd->ring.wp)
			mhi_ring_cmd_db(mhi_cntrl, mhi_cmd);
		spin_unlock_irq(&mhi_cmd->lock);
	}

	/* ring channel db registers */
	mhi_chan = mhi_cntrl->mhi_chan;
	for (i = 0; i < mhi_cntrl->max_chan; i++, mhi_chan++) {
		struct mhi_ring *tre_ring = &mhi_chan->tre_ring;

		if (mhi_chan->db_cfg.reset_req) {
			write_lock_irq(&mhi_chan->lock);
			mhi_chan->db_cfg.db_mode = true;
			write_unlock_irq(&mhi_chan->lock);
		}

		read_lock_irq(&mhi_chan->lock);
		/* only ring DB if ring is not empty */
		if (tre_ring->base && tre_ring->wp  != tre_ring->rp)
			mhi_ring_chan_db(mhi_cntrl, mhi_chan);
		read_unlock_irq(&mhi_chan->lock);
	}

	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);
	wake_up_all(&mhi_cntrl->state_event);
	MHI_VERB("Exited\n");

	return 0;
}

void mhi_pm_m1_transition(struct mhi_controller *mhi_cntrl)
{
	enum MHI_PM_STATE state;

	write_lock_irq(&mhi_cntrl->pm_lock);
	/* Just check if we are racing with device_wake assertion */
	if (atomic_read(&mhi_cntrl->dev_wake))
		MHI_VERB("M2 transition request post dev_wake:%d\n",
			 atomic_read(&mhi_cntrl->dev_wake));

	/* if it fails, means we transition to M3 */
	state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M2);
	if (state != MHI_PM_M2) {
		/* Nothing to be done, handle M3 transition later */
		write_unlock_irq(&mhi_cntrl->pm_lock);
		return;
	}

	MHI_VERB("Entered M2 State\n");
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M2);
	mhi_cntrl->dev_state = MHI_STATE_M2;
	mhi_cntrl->M2++;

	write_unlock_irq(&mhi_cntrl->pm_lock);
	wake_up_all(&mhi_cntrl->state_event);

	/* transfer pending, exit M2 immediately */
	if (unlikely(atomic_read(&mhi_cntrl->pending_pkts) ||
		     atomic_read(&mhi_cntrl->dev_wake))) {
		MHI_VERB(
			 "Exiting M2 Immediately, pending_pkts:%d dev_wake:%d\n",
			 atomic_read(&mhi_cntrl->pending_pkts),
			 atomic_read(&mhi_cntrl->dev_wake));
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_get(mhi_cntrl, true);
		mhi_cntrl->wake_put(mhi_cntrl, true);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return;
	}

	mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data, MHI_CB_IDLE);
}

int mhi_pm_m3_transition(struct mhi_controller *mhi_cntrl)
{
	enum MHI_PM_STATE state;

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->dev_state = MHI_STATE_M3;
	state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	if (state != MHI_PM_M3) {
		MHI_ERR("Failed to transition to state %s from %s\n",
			to_mhi_pm_state_str(MHI_PM_M3),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}
	wake_up_all(&mhi_cntrl->state_event);
	mhi_cntrl->M3++;

	MHI_LOG("Entered mhi_state:%s pm_state:%s\n",
		TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		to_mhi_pm_state_str(mhi_cntrl->pm_state));
	return 0;
}

static int mhi_pm_mission_mode_transition(struct mhi_controller *mhi_cntrl)
{
	int i, ret;
	enum mhi_ee ee = 0;
	struct mhi_event *mhi_event;

	MHI_CNTRL_LOG("Processing Mission Mode Transition\n");

	write_lock_irq(&mhi_cntrl->pm_lock);
	if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		ee = mhi_get_exec_env(mhi_cntrl);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	if (!MHI_IN_MISSION_MODE(ee)) {
		MHI_CNTRL_ERR("Invalid EE:%s\n", TO_MHI_EXEC_STR(ee));
		return -EIO;
	}

	mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data,
			     MHI_CB_EE_MISSION_MODE);
	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->ee = ee;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	wake_up_all(&mhi_cntrl->state_event);

	/* offload register write if supported */
	if (mhi_cntrl->offload_wq) {
		mhi_reset_reg_write_q(mhi_cntrl);
		mhi_cntrl->write_reg = mhi_write_reg_offload;
	}

	/* force MHI to be in M0 state before continuing */
	ret = __mhi_device_get_sync(mhi_cntrl);
	if (ret)
		return ret;

	read_lock_bh(&mhi_cntrl->pm_lock);

	/* add elements to all HW event rings */
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		ret = -EIO;
		goto error_mission_mode;
	}

	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		if (mhi_event->offload_ev || !mhi_event->hw_ring)
			continue;

		ring->wp = ring->base + ring->len - ring->el_size;
		*ring->ctxt_wp = ring->iommu_base + ring->len - ring->el_size;
		/* all ring updates must get updated immediately */
		smp_wmb();

		spin_lock_irq(&mhi_event->lock);
		if (MHI_DB_ACCESS_VALID(mhi_cntrl))
			mhi_ring_er_db(mhi_event);
		spin_unlock_irq(&mhi_event->lock);

	}

	read_unlock_bh(&mhi_cntrl->pm_lock);

	/* setup support for additional features */
	mhi_init_sfr(mhi_cntrl);

	if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		mhi_timesync_log(mhi_cntrl);

	mhi_special_events_pending(mhi_cntrl);

	/* setup sysfs nodes for userspace votes */
	mhi_create_sysfs(mhi_cntrl);

	MHI_CNTRL_LOG("Adding new devices\n");

	/* add supported devices */
	mhi_create_devices(mhi_cntrl);

	read_lock_bh(&mhi_cntrl->pm_lock);

error_mission_mode:
	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	MHI_CNTRL_LOG("Exit with ret:%d\n", ret);

	return ret;
}

/* handles both sys_err and shutdown transitions */
static void mhi_pm_disable_transition(struct mhi_controller *mhi_cntrl,
				      enum MHI_PM_STATE transition_state)
{
	enum MHI_PM_STATE cur_state, prev_state;
	struct mhi_event *mhi_event;
	struct mhi_cmd_ctxt *cmd_ctxt;
	struct mhi_cmd *mhi_cmd;
	struct mhi_event_ctxt *er_ctxt;
	struct mhi_sfr_info *sfr_info = mhi_cntrl->mhi_sfr;
	int ret, i;

	MHI_CNTRL_LOG(
		"Enter with from pm_state:%s MHI_STATE:%s to pm_state:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		to_mhi_pm_state_str(transition_state));

	/* restore async write call back */
	mhi_cntrl->write_reg = mhi_write_reg;

	if (mhi_cntrl->offload_wq)
		mhi_reset_reg_write_q(mhi_cntrl);

	/* We must notify MHI control driver so it can clean up first */
	if (transition_state == MHI_PM_SYS_ERR_PROCESS)
		mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data,
				     MHI_CB_SYS_ERROR);

	mutex_lock(&mhi_cntrl->pm_mutex);
	write_lock_irq(&mhi_cntrl->pm_lock);
	prev_state = mhi_cntrl->pm_state;
	cur_state = mhi_tryset_pm_state(mhi_cntrl, transition_state);
	if (cur_state == transition_state) {
		mhi_cntrl->ee = MHI_EE_DISABLE_TRANSITION;
		mhi_cntrl->dev_state = MHI_STATE_RESET;
	}
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* wake up any threads waiting for state transitions */
	wake_up_all(&mhi_cntrl->state_event);

	/* not handling sys_err, could be middle of shut down */
	if (cur_state != transition_state) {
		MHI_CNTRL_LOG("Failed to transition to state:0x%x from:0x%x\n",
				transition_state, cur_state);
		mutex_unlock(&mhi_cntrl->pm_mutex);
		return;
	}

	/* trigger MHI RESET so device will not access host ddr */
	if (MHI_REG_ACCESS_VALID(prev_state)) {
		unsigned long timeout = msecs_to_jiffies(mhi_cntrl->timeout_ms);

		MHI_CNTRL_LOG("Trigger device into MHI_RESET\n");

		write_lock_irq(&mhi_cntrl->pm_lock);
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_RESET);
		write_unlock_irq(&mhi_cntrl->pm_lock);

		/* wait for reset to be cleared */
		ret = wait_event_timeout(mhi_cntrl->state_event,
				!mhi_cntrl->initiate_mhi_reset, timeout);
		if (!ret && cur_state == MHI_PM_SYS_ERR_PROCESS) {
			MHI_CRITICAL("Device failed to exit RESET state\n");
			mutex_unlock(&mhi_cntrl->pm_mutex);
			return;
		}

		/*
		 * device cleares INTVEC as part of RESET processing,
		 * re-program it
		 */
		if (!mhi_cntrl->initiate_mhi_reset)
			mhi_cntrl->write_reg(mhi_cntrl, mhi_cntrl->bhi,
				BHI_INTVEC, 0);

		mhi_cntrl->initiate_mhi_reset = false;
	}

	MHI_CNTRL_LOG(
		"Waiting for all pending event ring processing to complete\n");
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (!mhi_event->request_irq)
			continue;
		tasklet_kill(&mhi_event->task);
	}

	mutex_unlock(&mhi_cntrl->pm_mutex);

	MHI_CNTRL_LOG("Reset all active channels and remove mhi devices\n");
	device_for_each_child(mhi_cntrl->dev, NULL, mhi_destroy_device);

	MHI_LOG("Finish resetting channels\n");

	/* remove support for userspace votes */
	mhi_destroy_sysfs(mhi_cntrl);

	MHI_CNTRL_LOG("Waiting for all pending threads to complete\n");
	wake_up_all(&mhi_cntrl->state_event);
	flush_work(&mhi_cntrl->special_work);

	if (sfr_info && sfr_info->buf_addr) {
		mhi_free_coherent(mhi_cntrl, sfr_info->len, sfr_info->buf_addr,
				  sfr_info->dma_addr);
		sfr_info->buf_addr = NULL;
	}

	mutex_lock(&mhi_cntrl->pm_mutex);

	MHI_ASSERT(atomic_read(&mhi_cntrl->dev_wake), "dev_wake != 0");
	MHI_ASSERT(atomic_read(&mhi_cntrl->pending_pkts), "pending_pkts != 0");

	/* reset the ev rings and cmd rings */
	MHI_CNTRL_LOG("Resetting EV CTXT and CMD CTXT\n");
	mhi_cmd = mhi_cntrl->mhi_cmd;
	cmd_ctxt = mhi_cntrl->mhi_ctxt->cmd_ctxt;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, mhi_cmd++, cmd_ctxt++) {
		struct mhi_ring *ring = &mhi_cmd->ring;

		ring->rp = ring->base;
		ring->wp = ring->base;
		cmd_ctxt->rp = cmd_ctxt->rbase;
		cmd_ctxt->wp = cmd_ctxt->rbase;
	}

	mhi_event = mhi_cntrl->mhi_event;
	er_ctxt = mhi_cntrl->mhi_ctxt->er_ctxt;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, er_ctxt++,
		     mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		/* do not touch offload er */
		if (mhi_event->offload_ev)
			continue;

		ring->rp = ring->base;
		ring->wp = ring->base;
		er_ctxt->rp = er_ctxt->rbase;
		er_ctxt->wp = er_ctxt->rbase;
	}

	if (cur_state == MHI_PM_SYS_ERR_PROCESS) {
		mhi_ready_state_transition(mhi_cntrl);
	} else {
		/* move to disable state */
		write_lock_irq(&mhi_cntrl->pm_lock);
		cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_DISABLE);
		write_unlock_irq(&mhi_cntrl->pm_lock);
		if (unlikely(cur_state != MHI_PM_DISABLE))
			MHI_CNTRL_ERR(
				"Error moving from pm state:%s to state:%s\n",
				to_mhi_pm_state_str(cur_state),
				to_mhi_pm_state_str(MHI_PM_DISABLE));
	}

	MHI_CNTRL_LOG("Exit with pm_state:%s mhi_state:%s\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state),
			TO_MHI_STATE_STR(mhi_cntrl->dev_state));

	mutex_unlock(&mhi_cntrl->pm_mutex);
}

int mhi_debugfs_trigger_soc_reset(void *data, u64 val)
{
	struct mhi_controller *mhi_cntrl = data;

	MHI_LOG("Trigger MHI SOC Reset\n");

	mhi_perform_soc_reset(mhi_cntrl);

	return 0;
}

int mhi_debugfs_trigger_reset(void *data, u64 val)
{
	struct mhi_controller *mhi_cntrl = data;
	enum MHI_PM_STATE cur_state;
	int ret;

	MHI_CNTRL_LOG("Trigger MHI Reset\n");

	/* exit lpm first */
	mhi_cntrl->runtime_get(mhi_cntrl, mhi_cntrl->priv_data);
	mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M0 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		MHI_CNTRL_ERR(
			"Did not enter M0 state, cur_state:%s pm_state:%s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_SYS_ERR_DETECT);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	if (cur_state == MHI_PM_SYS_ERR_DETECT)
		mhi_process_sys_err(mhi_cntrl);

	return 0;
}

/* queue disable transition work item */
int mhi_queue_disable_transition(struct mhi_controller *mhi_cntrl,
				 enum MHI_PM_STATE pm_state)
{
	struct state_transition *item = kmalloc(sizeof(*item), GFP_ATOMIC);
	unsigned long flags;

	if (!item)
		return -ENOMEM;

	item->pm_state = pm_state;
	item->state = MHI_ST_TRANSITION_DISABLE;
	spin_lock_irqsave(&mhi_cntrl->transition_lock, flags);
	list_add_tail(&item->node, &mhi_cntrl->transition_list);
	spin_unlock_irqrestore(&mhi_cntrl->transition_lock, flags);

	queue_work(mhi_cntrl->wq, &mhi_cntrl->st_worker);

	return 0;
}

/* queue a new work item and scheduler work */
int mhi_queue_state_transition(struct mhi_controller *mhi_cntrl,
			       enum MHI_ST_TRANSITION state)
{
	struct state_transition *item = kmalloc(sizeof(*item), GFP_ATOMIC);
	unsigned long flags;

	if (!item)
		return -ENOMEM;

	item->state = state;
	spin_lock_irqsave(&mhi_cntrl->transition_lock, flags);
	list_add_tail(&item->node, &mhi_cntrl->transition_list);
	spin_unlock_irqrestore(&mhi_cntrl->transition_lock, flags);

	queue_work(mhi_cntrl->wq, &mhi_cntrl->st_worker);

	return 0;
}

static void mhi_special_events_pending(struct mhi_controller *mhi_cntrl)
{
	struct mhi_event *mhi_event;

	list_for_each_entry(mhi_event, &mhi_cntrl->sp_ev_rings, node) {
		struct mhi_event_ctxt *er_ctxt =
			&mhi_cntrl->mhi_ctxt->er_ctxt[mhi_event->er_index];
		struct mhi_ring *ev_ring = &mhi_event->ring;

		spin_lock_bh(&mhi_event->lock);
		if (ev_ring->rp != mhi_to_virtual(ev_ring, er_ctxt->rp)) {
			queue_work(mhi_cntrl->wq, &mhi_cntrl->special_work);
			spin_unlock_bh(&mhi_event->lock);
			break;
		}
		spin_unlock_bh(&mhi_event->lock);
	}
}

void mhi_special_purpose_work(struct work_struct *work)
{
	struct mhi_controller *mhi_cntrl = container_of(work,
							struct mhi_controller,
							special_work);
	struct mhi_event *mhi_event;

	MHI_VERB("Enter with pm_state:%s MHI_STATE:%s ee:%s\n",
		 to_mhi_pm_state_str(mhi_cntrl->pm_state),
		 TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		 TO_MHI_EXEC_STR(mhi_cntrl->ee));

	if (unlikely(MHI_EVENT_ACCESS_INVALID(mhi_cntrl->pm_state)))
		return;

	/* check special purpose event rings and process events */
	list_for_each_entry(mhi_event, &mhi_cntrl->sp_ev_rings, node)
		mhi_event->process_event(mhi_cntrl, mhi_event, U32_MAX);
}

void mhi_process_sys_err(struct mhi_controller *mhi_cntrl)
{
	/*
	 * if controller supports rddm, we do not process sys error state,
	 * instead we will jump directly to rddm state
	 */
	if (mhi_cntrl->rddm_supported) {
		MHI_CNTRL_LOG(
			"Controller supports RDDM, skipping SYS_ERR_PROCESS\n");
		return;
	}

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->power_down = true;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	mhi_queue_disable_transition(mhi_cntrl, MHI_PM_SYS_ERR_PROCESS);
}

void mhi_pm_st_worker(struct work_struct *work)
{
	struct state_transition *itr, *tmp;
	LIST_HEAD(head);
	struct mhi_controller *mhi_cntrl = container_of(work,
							struct mhi_controller,
							st_worker);
	spin_lock_irq(&mhi_cntrl->transition_lock);
	list_splice_tail_init(&mhi_cntrl->transition_list, &head);
	spin_unlock_irq(&mhi_cntrl->transition_lock);

	list_for_each_entry_safe(itr, tmp, &head, node) {
		list_del(&itr->node);
		MHI_CNTRL_LOG("Transition to state:%s\n",
				TO_MHI_STATE_TRANS_STR(itr->state));

		switch (itr->state) {
		case MHI_ST_TRANSITION_PBL:
			write_lock_irq(&mhi_cntrl->pm_lock);
			if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
				mhi_cntrl->ee = mhi_get_exec_env(mhi_cntrl);
			write_unlock_irq(&mhi_cntrl->pm_lock);
			mhi_fw_load_handler(mhi_cntrl);
			break;
		case MHI_ST_TRANSITION_SBL:
			mhi_create_devices(mhi_cntrl);
			break;
		case MHI_ST_TRANSITION_MISSION_MODE:
			mhi_pm_mission_mode_transition(mhi_cntrl);
			break;
		case MHI_ST_TRANSITION_READY:
			mhi_ready_state_transition(mhi_cntrl);
			break;
		case MHI_ST_TRANSITION_DISABLE:
			mhi_pm_disable_transition(mhi_cntrl, itr->pm_state);
			break;
		default:
			break;
		}
		kfree(itr);
	}
}

int mhi_async_power_up(struct mhi_controller *mhi_cntrl)
{
	int ret;
	u32 val;
	enum mhi_ee current_ee;
	enum MHI_ST_TRANSITION next_state;
	struct mhi_device *mhi_dev = mhi_cntrl->mhi_dev;

	MHI_CNTRL_LOG("Requested to power on\n");

	if (mhi_cntrl->msi_allocated < mhi_cntrl->total_ev_rings)
		return -EINVAL;

	/* set to default wake if any one is not set */
	if (!mhi_cntrl->wake_get || !mhi_cntrl->wake_put ||
	    !mhi_cntrl->wake_toggle) {
		mhi_cntrl->wake_get = mhi_assert_dev_wake;
		mhi_cntrl->wake_put = mhi_deassert_dev_wake;
		mhi_cntrl->wake_toggle = (mhi_cntrl->db_access & MHI_PM_M2) ?
			mhi_toggle_dev_wake_nop : mhi_toggle_dev_wake;
	}

	/* clear votes before proceeding for power up */
	atomic_set(&mhi_dev->dev_vote, 0);
	atomic_set(&mhi_dev->bus_vote, 0);

	mutex_lock(&mhi_cntrl->pm_mutex);
	mhi_cntrl->pm_state = MHI_PM_DISABLE;

	if (!mhi_cntrl->pre_init) {
		/* setup device context */
		ret = mhi_init_dev_ctxt(mhi_cntrl);
		if (ret) {
			MHI_CNTRL_ERR("Error setting dev_context\n");
			goto error_dev_ctxt;
		}
	}

	ret = mhi_init_irq_setup(mhi_cntrl);
	if (ret) {
		MHI_CNTRL_ERR("Error setting up irq\n");
		goto error_setup_irq;
	}

	/* setup bhi offset & intvec */
	write_lock_irq(&mhi_cntrl->pm_lock);
	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, BHIOFF, &val);
	if (ret) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		MHI_ERR("Error getting bhi offset\n");
		goto error_bhi_offset;
	}

	if (val >= mhi_cntrl->len) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		MHI_ERR("Invalid bhi offset:%x\n", val);
		ret = -EINVAL;
		goto error_bhi_offset;
	}

	mhi_cntrl->bhi = mhi_cntrl->regs + val;

	/* setup bhie offset if not set */
	if (mhi_cntrl->fbc_download && !mhi_cntrl->bhie) {
		ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, BHIEOFF, &val);
		if (ret) {
			write_unlock_irq(&mhi_cntrl->pm_lock);
			MHI_CNTRL_ERR("Error getting bhie offset\n");
			goto error_bhi_offset;
		}

		if (val >= mhi_cntrl->len) {
			write_unlock_irq(&mhi_cntrl->pm_lock);
			MHI_ERR("Invalid bhie offset:%x\n", val);
			ret = -EINVAL;
			goto error_bhi_offset;
		}

		mhi_cntrl->bhie = mhi_cntrl->regs + val;
	}

	mhi_cntrl->write_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_INTVEC, 0);
	mhi_cntrl->power_down = false;
	mhi_cntrl->pm_state = MHI_PM_POR;
	mhi_cntrl->ee = MHI_EE_MAX;
	current_ee = mhi_get_exec_env(mhi_cntrl);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* confirm device is in valid exec env */
	if (!MHI_IN_PBL(current_ee) && current_ee != MHI_EE_AMSS) {
		MHI_CNTRL_ERR("Not a valid EE for power on:%s\n",
				TO_MHI_EXEC_STR(current_ee));
		ret = -EIO;
		goto error_bhi_offset;
	}

	/* transition to next state */
	next_state = MHI_IN_PBL(current_ee) ?
		MHI_ST_TRANSITION_PBL : MHI_ST_TRANSITION_READY;

	mhi_queue_state_transition(mhi_cntrl, next_state);

	mhi_init_debugfs(mhi_cntrl);

	mutex_unlock(&mhi_cntrl->pm_mutex);

	MHI_CNTRL_LOG("Power on setup success\n");

	return 0;

error_bhi_offset:
	mhi_cntrl->power_down = true;
	mhi_deinit_free_irq(mhi_cntrl);

error_setup_irq:
	if (!mhi_cntrl->pre_init)
		mhi_deinit_dev_ctxt(mhi_cntrl);

error_dev_ctxt:
	mutex_unlock(&mhi_cntrl->pm_mutex);

	return ret;
}
EXPORT_SYMBOL(mhi_async_power_up);

/* Transition MHI into error state and notify critical clients */
void mhi_control_error(struct mhi_controller *mhi_cntrl)
{
	enum MHI_PM_STATE cur_state, transition_state;
	struct mhi_sfr_info *sfr_info = mhi_cntrl->mhi_sfr;

	MHI_CNTRL_LOG("Enter with pm_state:%s MHI_STATE:%s\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state),
			TO_MHI_STATE_STR(mhi_cntrl->dev_state));

	/* copy subsystem failure reason string if supported */
	if (sfr_info && sfr_info->buf_addr) {
		memcpy(sfr_info->str, sfr_info->buf_addr, sfr_info->len);
		MHI_CNTRL_ERR("mhi:%s sfr: %s\n", mhi_cntrl->name,
				sfr_info->buf_addr);
	}

	/* link is not down if device supports RDDM */
	transition_state = (mhi_cntrl->rddm_supported) ?
		MHI_PM_DEVICE_ERR_DETECT : MHI_PM_LD_ERR_FATAL_DETECT;

	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, transition_state);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* proceed if we move to device error or are already in error state */
	if (!MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		MHI_CNTRL_ERR("Failed to transition to state:%s from:%s\n",
				to_mhi_pm_state_str(transition_state),
				to_mhi_pm_state_str(cur_state));
		goto exit_control_error;
	}

	mhi_cntrl->dev_state = MHI_STATE_SYS_ERR;

	/* notify waiters to bail out early since MHI has entered ERROR state */
	wake_up_all(&mhi_cntrl->state_event);

	/* start notifying all clients who request early notification */
	device_for_each_child(mhi_cntrl->dev, NULL, mhi_early_notify_device);

exit_control_error:
	MHI_CNTRL_LOG("Exit with pm_state:%s MHI_STATE:%s\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state),
			TO_MHI_STATE_STR(mhi_cntrl->dev_state));
}
EXPORT_SYMBOL(mhi_control_error);

void mhi_power_down(struct mhi_controller *mhi_cntrl, bool graceful)
{
	enum MHI_PM_STATE cur_state;
	enum MHI_PM_STATE transition_state = MHI_PM_SHUTDOWN_PROCESS;

	mutex_lock(&mhi_cntrl->pm_mutex);

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->power_down = true;
	/* if it's not graceful shutdown, force MHI to a linkdown state */
	if (!graceful) {
		cur_state = mhi_tryset_pm_state(mhi_cntrl,
						MHI_PM_LD_ERR_FATAL_DETECT);
		if (cur_state != MHI_PM_LD_ERR_FATAL_DETECT)
			MHI_CNTRL_ERR("Failed to move to state:%s from:%s\n",
				to_mhi_pm_state_str(MHI_PM_LD_ERR_FATAL_DETECT),
				to_mhi_pm_state_str(mhi_cntrl->pm_state));
		transition_state = MHI_PM_SHUTDOWN_NO_ACCESS;
	}
	write_unlock_irq(&mhi_cntrl->pm_lock);

	mutex_unlock(&mhi_cntrl->pm_mutex);

	mhi_queue_disable_transition(mhi_cntrl, transition_state);

	MHI_CNTRL_LOG("Wait for shutdown to complete\n");
	flush_work(&mhi_cntrl->st_worker);

	mhi_deinit_debugfs(mhi_cntrl);

	mhi_deinit_free_irq(mhi_cntrl);

	if (!mhi_cntrl->pre_init) {
		/* free all allocated resources */
		if (mhi_cntrl->fbc_image)
			mhi_free_bhie_table(mhi_cntrl, &mhi_cntrl->fbc_image);

		mhi_deinit_dev_ctxt(mhi_cntrl);
	}
}
EXPORT_SYMBOL(mhi_power_down);

int mhi_sync_power_up(struct mhi_controller *mhi_cntrl)
{
	int ret = mhi_async_power_up(mhi_cntrl);

	if (ret)
		return ret;

	wait_event_timeout(mhi_cntrl->state_event,
			   MHI_IN_MISSION_MODE(mhi_cntrl->ee) ||
			   MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
			   msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (MHI_IN_MISSION_MODE(mhi_cntrl->ee))
		return 0;

	MHI_ERR("MHI did not reach mission mode within %d ms\n",
		mhi_cntrl->timeout_ms);
	mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data,
			     MHI_CB_BOOTUP_TIMEOUT);
	return -ETIMEDOUT;
}
EXPORT_SYMBOL(mhi_sync_power_up);

int mhi_pm_suspend(struct mhi_controller *mhi_cntrl)
{
	int ret;
	enum MHI_PM_STATE new_state;
	struct mhi_chan *itr, *tmp;
	struct mhi_device *mhi_dev = mhi_cntrl->mhi_dev;

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE)
		return -EINVAL;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		return -EIO;

	/* do a quick check to see if any pending votes to keep us busy */
	if (atomic_read(&mhi_cntrl->dev_wake) ||
	    atomic_read(&mhi_cntrl->pending_pkts) ||
	    atomic_read(&mhi_dev->bus_vote)) {
		MHI_VERB("Busy, aborting M3\n");
		return -EBUSY;
	}

	/* exit MHI out of M2 state */
	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_get(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M0 ||
				 mhi_cntrl->dev_state == MHI_STATE_M1 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		MHI_ERR(
			"Did not enter M0||M1 state, cur_state:%s pm_state:%s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_m0_entry;
	}

	write_lock_irq(&mhi_cntrl->pm_lock);

	/*
	 * Check the votes once more to see if we should abort
	 * suepend. We're asserting wake so count would be @ least 1
	 */
	if (atomic_read(&mhi_cntrl->dev_wake) > 1 ||
	    atomic_read(&mhi_cntrl->pending_pkts) ||
	    atomic_read(&mhi_dev->bus_vote)) {
		MHI_VERB("Busy, aborting M3\n");
		write_unlock_irq(&mhi_cntrl->pm_lock);
		ret = -EBUSY;
		goto error_m0_entry;
	}

	/* anytime after this, we will resume thru runtime pm framework */
	MHI_LOG("Allowing M3 transition\n");
	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_ENTER);
	if (new_state != MHI_PM_M3_ENTER) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		MHI_ERR("Error setting to pm_state:%s from pm_state:%s\n",
			to_mhi_pm_state_str(MHI_PM_M3_ENTER),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));

		ret = -EIO;
		goto error_m0_entry;
	}

	/* set dev to M3 and wait for completion */
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M3);
	mhi_cntrl->wake_put(mhi_cntrl, false);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	MHI_LOG("Wait for M3 completion\n");

	/* finish reg writes before D3 cold */
	mhi_force_reg_write(mhi_cntrl);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M3 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		MHI_ERR("Did not enter M3 state, cur_state:%s pm_state:%s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	/* notify any clients we enter lpm */
	list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans, node) {
		if (itr->mhi_dev)
			mhi_notify(itr->mhi_dev, MHI_CB_LPM_ENTER);
	}

	return 0;

error_m0_entry:
	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	return ret;
}
EXPORT_SYMBOL(mhi_pm_suspend);

/**
 * mhi_pm_fast_suspend - Faster suspend path where we transition host to
 * inactive state w/o suspending device.  Useful for cases where we want apps to
 * go into power collapse but keep the physical link in active state.
 */
int mhi_pm_fast_suspend(struct mhi_controller *mhi_cntrl, bool notify_client)
{
	int ret;
	enum MHI_PM_STATE new_state;
	struct mhi_chan *itr, *tmp;
	struct mhi_device *mhi_dev = mhi_cntrl->mhi_dev;

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (mhi_cntrl->pm_state == MHI_PM_DISABLE) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EINVAL;
	}

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}
	read_unlock_bh(&mhi_cntrl->pm_lock);

	/* do a quick check to see if any pending votes to keep us busy */
	if (atomic_read(&mhi_cntrl->pending_pkts) ||
	    atomic_read(&mhi_dev->bus_vote)) {
		MHI_VERB("Busy, aborting M3\n");
		return -EBUSY;
	}

	/* wait here if controller wants device to be in M2 before proceeding */
	wait_event_timeout(mhi_cntrl->state_event,
			   mhi_cntrl->dev_state == MHI_STATE_M2,
			   msecs_to_jiffies(mhi_cntrl->m2_timeout_ms));

	/* disable ctrl event processing */
	tasklet_disable(&mhi_cntrl->mhi_event->task);

	write_lock_irq(&mhi_cntrl->pm_lock);

	/*
	 * Check the votes once more to see if we should abort
	 * suspend.
	 */
	if (atomic_read(&mhi_cntrl->pending_pkts) ||
	    atomic_read(&mhi_dev->bus_vote)) {
		MHI_VERB("Busy, aborting M3\n");
		ret = -EBUSY;
		goto error_suspend;
	}

	/* check pm state before saving it */
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		ret = -EIO;
		goto error_suspend;
	}

	/* anytime after this, we will resume thru runtime pm framework */
	MHI_LOG("Allowing Fast M3 transition\n");

	/* save the current states */
	mhi_cntrl->saved_pm_state = mhi_cntrl->pm_state;
	mhi_cntrl->saved_dev_state = mhi_cntrl->dev_state;

	/* If we're in M2, we need to switch back to M0 first */
	if (mhi_cntrl->pm_state == MHI_PM_M2) {
		new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M0);
		if (new_state != MHI_PM_M0) {
			MHI_ERR("Error set pm_state to:%s from pm_state:%s\n",
				to_mhi_pm_state_str(MHI_PM_M0),
				to_mhi_pm_state_str(mhi_cntrl->pm_state));
			ret = -EIO;
			goto error_suspend;
		}
	}

	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_ENTER);
	if (new_state != MHI_PM_M3_ENTER) {
		MHI_ERR("Error setting to pm_state:%s from pm_state:%s\n",
			to_mhi_pm_state_str(MHI_PM_M3_ENTER),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_suspend;
	}

	/* set dev to M3_FAST and host to M3 */
	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3);
	if (new_state != MHI_PM_M3) {
		MHI_ERR("Error setting to pm_state:%s from pm_state:%s\n",
			to_mhi_pm_state_str(MHI_PM_M3),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_suspend;
	}

	mhi_cntrl->dev_state = MHI_STATE_M3_FAST;
	mhi_cntrl->M3_FAST++;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* finish reg writes before DRV hand-off to avoid noc err */
	mhi_force_reg_write(mhi_cntrl);

	/* now safe to check ctrl event ring */
	tasklet_enable(&mhi_cntrl->mhi_event->task);
	mhi_msi_handlr(0, mhi_cntrl->mhi_event);

	if (!notify_client)
		return 0;

	/* notify any clients we enter lpm */
	list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans, node) {
		if (itr->mhi_dev)
			mhi_notify(itr->mhi_dev, MHI_CB_LPM_ENTER);
	}

	return 0;

error_suspend:
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* check ctrl event ring for pending work */
	tasklet_enable(&mhi_cntrl->mhi_event->task);
	mhi_msi_handlr(0, mhi_cntrl->mhi_event);

	return ret;
}
EXPORT_SYMBOL(mhi_pm_fast_suspend);

int mhi_pm_resume(struct mhi_controller *mhi_cntrl)
{
	enum MHI_PM_STATE cur_state;
	int ret;
	struct mhi_chan *itr, *tmp;

	MHI_LOG("Entered with pm_state:%s dev_state:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		TO_MHI_STATE_STR(mhi_cntrl->dev_state));

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE)
		return 0;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		return -EIO;

	MHI_ASSERT(mhi_cntrl->pm_state != MHI_PM_M3, "mhi_pm_state != M3");

	/* notify any clients we enter lpm */
	list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans, node) {
		if (itr->mhi_dev)
			mhi_notify(itr->mhi_dev, MHI_CB_LPM_EXIT);
	}

	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_EXIT);
	if (cur_state != MHI_PM_M3_EXIT) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		MHI_ERR("Error setting to pm_state:%s from pm_state:%s\n",
			to_mhi_pm_state_str(MHI_PM_M3_EXIT),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	/* set dev to M0 and wait for completion */
	mhi_cntrl->wake_get(mhi_cntrl, true);
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M0);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M0 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		MHI_ERR("Did not enter M0 state, cur_state:%s pm_state:%s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));

		/*
		 * It's possible device already in error state and we didn't
		 * process it due to low power mode, force a check
		 */
		mhi_intvec_threaded_handlr(0, mhi_cntrl);
		return -EIO;
	}

	/*
	 * If MHI on host is in suspending/suspended state, we do not process
	 * any special purpose requests, for example, bandwidth scaling events
	 * from the device. Check for special purpose event rings and handle
	 * the pending events upon resume.
	 */
	mhi_special_events_pending(mhi_cntrl);

	return 0;
}

int mhi_pm_fast_resume(struct mhi_controller *mhi_cntrl, bool notify_client)
{
	struct mhi_chan *itr, *tmp;
	struct mhi_event *mhi_event;
	int i;

	read_lock_bh(&mhi_cntrl->pm_lock);
	MHI_LOG("Entered with pm_state:%s dev_state:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		TO_MHI_STATE_STR(mhi_cntrl->dev_state));

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return 0;
	}

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}

	MHI_ASSERT(mhi_cntrl->pm_state != MHI_PM_M3, "mhi_pm_state != M3");
	read_unlock_bh(&mhi_cntrl->pm_lock);

	/* notify any clients we're about to exit lpm */
	if (notify_client) {
		list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans,
					 node) {
			if (itr->mhi_dev)
				mhi_notify(itr->mhi_dev, MHI_CB_LPM_EXIT);
		}
	}

	/* do not process control events */
	tasklet_disable(&mhi_cntrl->mhi_event->task);

	/* check if we entered error state again before doing the restore */
	write_lock_irq(&mhi_cntrl->pm_lock);

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		tasklet_enable(&mhi_cntrl->mhi_event->task);
		return -EIO;
	}

	if (mhi_cntrl->rddm_supported) {

		/* check EP is in proper state */
		if (mhi_cntrl->link_status(mhi_cntrl, mhi_cntrl->priv_data)) {
			MHI_ERR("Unable to access EP Config space\n");
			write_unlock_irq(&mhi_cntrl->pm_lock);
			tasklet_enable(&mhi_cntrl->mhi_event->task);
			return -ETIMEDOUT;
		}

		if (mhi_get_exec_env(mhi_cntrl) == MHI_EE_RDDM &&
		    !mhi_cntrl->power_down) {
			mhi_cntrl->ee = MHI_EE_RDDM;
			write_unlock_irq(&mhi_cntrl->pm_lock);

			MHI_ERR("RDDM event occurred!\n");

			/* notify critical clients with early notifications */
			mhi_control_error(mhi_cntrl);

			mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data,
					     MHI_CB_EE_RDDM);
			wake_up_all(&mhi_cntrl->state_event);

			tasklet_enable(&mhi_cntrl->mhi_event->task);
			goto exit_pm_fast_resume;
		}
	}

	/* restore the states */
	mhi_cntrl->pm_state = mhi_cntrl->saved_pm_state;
	mhi_cntrl->dev_state = mhi_cntrl->saved_dev_state;

	write_unlock_irq(&mhi_cntrl->pm_lock);

	switch (mhi_cntrl->pm_state) {
	case MHI_PM_M0:
		mhi_pm_m0_transition(mhi_cntrl);
		break;
	case MHI_PM_M2:
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_get(mhi_cntrl, true);
		mhi_cntrl->wake_put(mhi_cntrl, true);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		break;
	default:
		MHI_ERR("Unexpected PM state:%s after restore\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		tasklet_enable(&mhi_cntrl->mhi_event->task);
		return -EIO;
	}

	/* now it's safe to process ctrl events */
	tasklet_enable(&mhi_cntrl->mhi_event->task);

	/*
	 * In fast suspend/resume case device is not aware host transition
	 * to suspend state. So, device could be triggering a interrupt while
	 * host not accepting MSI. We have to manually check each event ring
	 * upon resume.
	 */
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (!mhi_event->request_irq)
			continue;

		mhi_msi_handlr(0, mhi_event);
	}

	/* schedules worker if any special purpose events need to be handled */
	mhi_special_events_pending(mhi_cntrl);

exit_pm_fast_resume:
	MHI_LOG("Exit with pm_state:%s dev_state:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		TO_MHI_STATE_STR(mhi_cntrl->dev_state));

	return 0;
}

int __mhi_device_get_sync(struct mhi_controller *mhi_cntrl)
{
	int ret;

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}

	mhi_cntrl->wake_get(mhi_cntrl, true);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	pm_wakeup_hard_event(&mhi_cntrl->mhi_dev->dev);
	mhi_cntrl->runtime_get(mhi_cntrl, mhi_cntrl->priv_data);

	mhi_force_reg_write(mhi_cntrl);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->pm_state == MHI_PM_M0 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		MHI_ERR("Did not enter M0 state, cur_state:%s pm_state:%s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_put(mhi_cntrl, false);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);
		return -EIO;
	}

	mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);
	return 0;
}

void mhi_device_get(struct mhi_device *mhi_dev, int vote)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	if (vote & MHI_VOTE_DEVICE) {
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_trigger_resume(mhi_cntrl);
		mhi_cntrl->wake_get(mhi_cntrl, true);
		MHI_LOG("dev_wake %d\n", atomic_read(&mhi_cntrl->dev_wake));
		read_unlock_bh(&mhi_cntrl->pm_lock);
		atomic_inc(&mhi_dev->dev_vote);
	}

	if (vote & MHI_VOTE_BUS) {
		mhi_cntrl->runtime_get(mhi_cntrl, mhi_cntrl->priv_data);
		atomic_inc(&mhi_dev->bus_vote);
	}
}
EXPORT_SYMBOL(mhi_device_get);

int mhi_device_get_sync(struct mhi_device *mhi_dev, int vote)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	int ret;

	/*
	 * regardless of any vote we will bring device out lpm and assert
	 * device wake
	 */
	ret = __mhi_device_get_sync(mhi_cntrl);
	if (ret)
		return ret;

	if (vote & MHI_VOTE_DEVICE) {
		atomic_inc(&mhi_dev->dev_vote);
	} else {
		/* client did not requested device vote so de-assert dev_wake */
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_put(mhi_cntrl, false);
		read_unlock_bh(&mhi_cntrl->pm_lock);
	}

	if (vote & MHI_VOTE_BUS) {
		mhi_cntrl->runtime_get(mhi_cntrl, mhi_cntrl->priv_data);
		atomic_inc(&mhi_dev->bus_vote);
	}

	return 0;
}
EXPORT_SYMBOL(mhi_device_get_sync);

int mhi_device_get_sync_atomic(struct mhi_device *mhi_dev, int timeout_us,
			       bool in_panic)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}

	mhi_cntrl->wake_get(mhi_cntrl, true);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	atomic_inc(&mhi_dev->dev_vote);
	pm_wakeup_hard_event(&mhi_cntrl->mhi_dev->dev);
	mhi_cntrl->runtime_get(mhi_cntrl, mhi_cntrl->priv_data);

	/* Return if client doesn't want us to wait */
	if (!timeout_us) {
		if (mhi_cntrl->pm_state != MHI_PM_M0)
			MHI_ERR("Return without waiting for M0\n");

		mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);
		return 0;
	}

	if (in_panic) {
		while (mhi_get_mhi_state(mhi_cntrl) != MHI_STATE_M0 &&
		       !MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) &&
		       timeout_us > 0) {
			udelay(MHI_FORCE_WAKE_DELAY_US);
			timeout_us -= MHI_FORCE_WAKE_DELAY_US;
		}
	} else {
		while (mhi_cntrl->pm_state != MHI_PM_M0 &&
		       !MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) &&
		       timeout_us > 0) {
			udelay(MHI_FORCE_WAKE_DELAY_US);
			timeout_us -= MHI_FORCE_WAKE_DELAY_US;
		}
	}

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) || timeout_us <= 0) {
		MHI_ERR("Did not enter M0 state, cur_state:%s pm_state:%s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_put(mhi_cntrl, false);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		atomic_dec(&mhi_dev->dev_vote);
		mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);
		return -ETIMEDOUT;
	}

	mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);

	return 0;
}
EXPORT_SYMBOL(mhi_device_get_sync_atomic);

void mhi_device_put(struct mhi_device *mhi_dev, int vote)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	if (vote & MHI_VOTE_DEVICE) {
		atomic_dec(&mhi_dev->dev_vote);
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_trigger_resume(mhi_cntrl);
		mhi_cntrl->wake_put(mhi_cntrl, false);
		MHI_LOG("dev_wake %d\n", atomic_read(&mhi_cntrl->dev_wake));
		read_unlock_bh(&mhi_cntrl->pm_lock);
	}

	if (vote & MHI_VOTE_BUS) {
		atomic_dec(&mhi_dev->bus_vote);
		mhi_cntrl->runtime_put(mhi_cntrl, mhi_cntrl->priv_data);

		/*
		 * if counts reach 0, clients release all votes
		 * send idle cb to to attempt suspend
		 */
		if (!atomic_read(&mhi_dev->bus_vote))
			mhi_cntrl->status_cb(mhi_cntrl, mhi_cntrl->priv_data,
					     MHI_CB_IDLE);
	}
}
EXPORT_SYMBOL(mhi_device_put);

int mhi_force_rddm_mode(struct mhi_controller *mhi_cntrl)
{
	int ret;

	MHI_CNTRL_LOG("Enter with pm_state:%s ee:%s\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state),
			TO_MHI_EXEC_STR(mhi_cntrl->ee));

	/* device does not support RDDM */
	if (!mhi_cntrl->rddm_supported)
		return -EINVAL;

	/* device already in rddm */
	if (mhi_cntrl->ee == MHI_EE_RDDM)
		return 0;

	MHI_CNTRL_LOG("Triggering SYS_ERR to force rddm state\n");
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_SYS_ERR);

	/* wait for rddm event */
	MHI_CNTRL_LOG("Waiting for device to enter RDDM state\n");
	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->ee == MHI_EE_RDDM,
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));
	ret = ret ? 0 : -EIO;

	MHI_CNTRL_LOG("Exiting with pm_state:%s ee:%s ret:%d\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state),
			TO_MHI_EXEC_STR(mhi_cntrl->ee), ret);

	return ret;
}
EXPORT_SYMBOL(mhi_force_rddm_mode);
