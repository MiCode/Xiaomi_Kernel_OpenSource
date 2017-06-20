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
#include "mhi_hwio.h"
#include "mhi_trace.h"
#include "mhi_bhi.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

const char *state_transition_str(enum STATE_TRANSITION state)
{
	static const char * const
		mhi_states_transition_str[STATE_TRANSITION_MAX] = {
		[STATE_TRANSITION_RESET] = "RESET",
		[STATE_TRANSITION_READY] = "READY",
		[STATE_TRANSITION_M0] = "M0",
		[STATE_TRANSITION_M1] = "M1",
		[STATE_TRANSITION_M2] = "M2",
		[STATE_TRANSITION_M3] = "M3",
		[STATE_TRANSITION_BHI] = "BHI",
		[STATE_TRANSITION_SBL] = "SBL",
		[STATE_TRANSITION_AMSS] = "AMSS",
		[STATE_TRANSITION_LINK_DOWN] = "LINK_DOWN",
		[STATE_TRANSITION_WAKE] = "WAKE",
		[STATE_TRANSITION_BHIE] = "BHIE",
		[STATE_TRANSITION_RDDM] = "RDDM",
		[STATE_TRANSITION_SYS_ERR] = "SYS_ERR",
	};

	return (state < STATE_TRANSITION_MAX) ?
		mhi_states_transition_str[state] : "Invalid";
}

int set_mhi_base_state(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 pcie_word_val = 0;
	int r = 0;

	mhi_dev_ctxt->bhi_ctxt.bhi_base = mhi_dev_ctxt->core.bar0_base;
	pcie_word_val = mhi_reg_read(mhi_dev_ctxt->bhi_ctxt.bhi_base, BHIOFF);

	/* confirm it's a valid reading */
	if (unlikely(pcie_word_val == U32_MAX)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Invalid BHI Offset:0x%x\n", pcie_word_val);
		return -EIO;
	}
	mhi_dev_ctxt->bhi_ctxt.bhi_base += pcie_word_val;
	pcie_word_val = mhi_reg_read(mhi_dev_ctxt->bhi_ctxt.bhi_base,
				     BHI_EXECENV);
	mhi_dev_ctxt->dev_exec_env = pcie_word_val;
	if (pcie_word_val == MHI_EXEC_ENV_AMSS) {
		mhi_dev_ctxt->base_state = STATE_TRANSITION_RESET;
	} else if (pcie_word_val == MHI_EXEC_ENV_PBL) {
		mhi_dev_ctxt->base_state = STATE_TRANSITION_BHI;
	} else {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Invalid EXEC_ENV: 0x%x\n",
			pcie_word_val);
		r = -EIO;
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"EXEC_ENV: %d Base state %d\n",
		pcie_word_val, mhi_dev_ctxt->base_state);
	return r;
}

int init_mhi_base_state(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;

	r = mhi_init_state_transition(mhi_dev_ctxt, mhi_dev_ctxt->base_state);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to start state change event, to %d\n",
			mhi_dev_ctxt->base_state);
	}
	return r;
}

enum MHI_STATE mhi_get_m_state(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 state = mhi_reg_read_field(mhi_dev_ctxt->mmio_info.mmio_addr,
				       MHISTATUS,
				       MHISTATUS_MHISTATE_MASK,
				       MHISTATUS_MHISTATE_SHIFT);

	return state;
}

bool mhi_in_sys_err(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 state = mhi_reg_read_field(mhi_dev_ctxt->mmio_info.mmio_addr,
				       MHISTATUS, MHISTATUS_SYSERR_MASK,
				       MHISTATUS_SYSERR_SHIFT);

	return (state) ? true : false;
}

void mhi_set_m_state(struct mhi_device_ctxt *mhi_dev_ctxt,
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

/*
 * Not all MHI states transitions are sync transitions. Linkdown, SSR, and
 * shutdown can happen anytime asynchronously. This function will transition to
 * new state only if it's a valid transitions.
 *
 * Priority increase as we go down, example while in any states from L0, start
 * state from L1, L2, or L3 can be set.  Notable exception to this rule is state
 * DISABLE.  From DISABLE state we can transition to only POR or SSR_PENDING
 * state.  Also for example while in L2 state, user cannot jump back to L1 or
 * L0 states.
 * Valid transitions:
 * L0: DISABLE <--> POR
 *     DISABLE <--> SSR_PENDING
 *     POR <--> POR
 *     POR -> M0 -> M1 -> M1_M2 -> M2 --> M0
 *     M1_M2 -> M0 (Device can trigger it)
 *     M0 -> M3_ENTER -> M3 -> M3_EXIT --> M0
 *     M1 -> M3_ENTER --> M3
 * L1: SYS_ERR_DETECT -> SYS_ERR_PROCESS --> POR
 * L2: SHUTDOWN_PROCESS -> DISABLE -> SSR_PENDING (via SSR Notification only)
 * L3: LD_ERR_FATAL_DETECT <--> LD_ERR_FATAL_DETECT
 *     LD_ERR_FATAL_DETECT -> SHUTDOWN_PROCESS
 */
static const struct mhi_pm_transitions const mhi_state_transitions[] = {
	/* L0 States */
	{
		MHI_PM_DISABLE,
		MHI_PM_POR | MHI_PM_SSR_PENDING
	},
	{
		MHI_PM_POR,
		MHI_PM_POR | MHI_PM_DISABLE | MHI_PM_M0 |
		MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M0,
		MHI_PM_M1 | MHI_PM_M3_ENTER | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M1,
		MHI_PM_M1_M2_TRANSITION | MHI_PM_M3_ENTER |
		MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M1_M2_TRANSITION,
		MHI_PM_M2 | MHI_PM_M0 | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M2,
		MHI_PM_M0 | MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M3_ENTER,
		MHI_PM_M3 | MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M3,
		MHI_PM_M3_EXIT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M3_EXIT,
		MHI_PM_M0 | MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	/* L1 States */
	{
		MHI_PM_SYS_ERR_DETECT,
		MHI_PM_SYS_ERR_PROCESS | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_SYS_ERR_PROCESS,
		MHI_PM_POR | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	/* L2 States */
	{
		MHI_PM_SHUTDOWN_PROCESS,
		MHI_PM_DISABLE | MHI_PM_LD_ERR_FATAL_DETECT
	},
	/* L3 States */
	{
		MHI_PM_LD_ERR_FATAL_DETECT,
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_SHUTDOWN_PROCESS
	},
	/* From SSR notification only */
	{
		MHI_PM_SSR_PENDING,
		MHI_PM_DISABLE
	}
};

enum MHI_PM_STATE __must_check mhi_tryset_pm_state(
				struct mhi_device_ctxt *mhi_dev_ctxt,
				enum MHI_PM_STATE state)
{
	unsigned long cur_state = mhi_dev_ctxt->mhi_pm_state;
	int index = find_last_bit(&cur_state, 32);

	if (unlikely(index >= ARRAY_SIZE(mhi_state_transitions))) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"cur_state:0x%lx out side of mhi_state_transitions\n",
			cur_state);
		return cur_state;
	}

	if (unlikely(mhi_state_transitions[index].from_state != cur_state)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"index:%u cur_state:0x%lx != actual_state: 0x%x\n",
			index, cur_state,
			mhi_state_transitions[index].from_state);
		return cur_state;
	}

	if (unlikely(!(mhi_state_transitions[index].to_states & state))) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Not allowing pm state transition from:0x%lx to:0x%x state\n",
			cur_state, state);
		return cur_state;
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Transition to pm state from:0x%lx to:0x%x\n",
		cur_state, state);
	mhi_dev_ctxt->mhi_pm_state = state;
	return mhi_dev_ctxt->mhi_pm_state;
}

static void conditional_chan_db_write(
				struct mhi_device_ctxt *mhi_dev_ctxt, u32 chan)
{
	u64 db_value;
	unsigned long flags;
	struct mhi_ring *mhi_ring = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];

	spin_lock_irqsave(&mhi_ring->ring_lock, flags);
	db_value = mhi_v2p_addr(mhi_dev_ctxt,
				MHI_RING_TYPE_XFER_RING,
				chan,
				(uintptr_t)mhi_ring->wp);
	mhi_ring->db_mode.process_db(mhi_dev_ctxt,
				     mhi_dev_ctxt->mmio_info.chan_db_addr,
				     chan,
				     db_value);
	spin_unlock_irqrestore(&mhi_ring->ring_lock, flags);
}

static void ring_all_chan_dbs(struct mhi_device_ctxt *mhi_dev_ctxt,
			      bool reset_db_mode)
{
	u32 i = 0;
	struct mhi_ring *local_ctxt = NULL;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Ringing chan dbs\n");
	for (i = 0; i < MHI_MAX_CHANNELS; ++i)
		if (VALID_CHAN_NR(i)) {
			local_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[i];

			/* Reset the DB Mode state to DB Mode */
			if (local_ctxt->db_mode.preserve_db_state == 0
			    && reset_db_mode)
				local_ctxt->db_mode.db_mode = 1;

			if (local_ctxt->wp != local_ctxt->rp)
				conditional_chan_db_write(mhi_dev_ctxt, i);
		}
}

static void ring_all_cmd_dbs(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u64 db_value;
	u64 rp = 0;
	struct mhi_ring *local_ctxt = NULL;

	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE, "Ringing chan dbs\n");

	local_ctxt = &mhi_dev_ctxt->mhi_local_cmd_ctxt[PRIMARY_CMD_RING];
	rp = mhi_v2p_addr(mhi_dev_ctxt, MHI_RING_TYPE_CMD_RING,
						PRIMARY_CMD_RING,
						(uintptr_t)local_ctxt->rp);
	db_value = mhi_v2p_addr(mhi_dev_ctxt,
				MHI_RING_TYPE_CMD_RING,
				PRIMARY_CMD_RING,
				(uintptr_t)local_ctxt->wp);
	if (rp != db_value)
		local_ctxt->db_mode.process_db(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.cmd_db_addr,
				0,
				db_value);
}

static void ring_all_ev_dbs(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 i;
	u64 db_value = 0;
	struct mhi_event_ctxt *event_ctxt = NULL;
	struct mhi_ring *mhi_ring;
	spinlock_t *lock = NULL;
	unsigned long flags;

	for (i = 0; i < mhi_dev_ctxt->mmio_info.nr_event_rings; ++i) {
		mhi_ring = &mhi_dev_ctxt->mhi_local_event_ctxt[i];
		lock = &mhi_ring->ring_lock;
		spin_lock_irqsave(lock, flags);
		event_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[i];
		db_value = mhi_v2p_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_EVENT_RING,
					i,
					(uintptr_t)mhi_ring->wp);
		mhi_ring->db_mode.process_db(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.event_db_addr,
				i,
				db_value);
		spin_unlock_irqrestore(lock, flags);
	}
}

int process_m0_transition(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	unsigned long flags;
	enum MHI_PM_STATE cur_state;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Entered With State %s\n",
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));

	switch (mhi_dev_ctxt->mhi_state) {
	case MHI_STATE_M2:
		mhi_dev_ctxt->counters.m2_m0++;
		break;
	case MHI_STATE_M3:
		mhi_dev_ctxt->counters.m3_m0++;
		break;
	default:
		break;
	}

	write_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
	mhi_dev_ctxt->mhi_state = MHI_STATE_M0;
	cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, MHI_PM_M0);
	write_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);
	if (unlikely(cur_state != MHI_PM_M0)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to transition to state 0x%x from 0x%x\n",
			MHI_PM_M0, cur_state);
		return -EIO;
	}
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, true);

	if (mhi_dev_ctxt->flags.mhi_initialized) {
		ring_all_ev_dbs(mhi_dev_ctxt);
		ring_all_chan_dbs(mhi_dev_ctxt, true);
		ring_all_cmd_dbs(mhi_dev_ctxt);
	}

	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	wake_up(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Exited\n");

	return 0;
}

void process_m1_transition(struct work_struct *work)
{
	struct mhi_device_ctxt *mhi_dev_ctxt;
	enum MHI_PM_STATE cur_state;

	mhi_dev_ctxt = container_of(work,
				    struct mhi_device_ctxt,
				    process_m1_worker);
	mutex_lock(&mhi_dev_ctxt->pm_lock);
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Processing M1 state transition from state %s\n",
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));

	/* We either Entered M3 or we did M3->M0 Exit */
	if (mhi_dev_ctxt->mhi_pm_state != MHI_PM_M1)
		goto invalid_pm_state;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Transitioning to M2 Transition\n");
	cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, MHI_PM_M1_M2_TRANSITION);
	if (unlikely(cur_state != MHI_PM_M1_M2_TRANSITION)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to transition to state 0x%x from 0x%x\n",
			MHI_PM_M1_M2_TRANSITION, cur_state);
		goto invalid_pm_state;
	}
	mhi_dev_ctxt->counters.m1_m2++;
	mhi_dev_ctxt->mhi_state = MHI_STATE_M2;
	mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_M2);
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	usleep_range(MHI_M2_DEBOUNCE_TMR_US, MHI_M2_DEBOUNCE_TMR_US + 50);
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	/* During DEBOUNCE Time We could be receiving M0 Event */
	if (mhi_dev_ctxt->mhi_pm_state == MHI_PM_M1_M2_TRANSITION) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Entered M2 State\n");
		cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, MHI_PM_M2);
		if (unlikely(cur_state != MHI_PM_M2)) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Failed to transition to state 0x%x from 0x%x\n",
				MHI_PM_M2, cur_state);
			goto invalid_pm_state;
		}
	}
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	if (unlikely(atomic_read(&mhi_dev_ctxt->counters.device_wake))) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Exiting M2 Immediately, count:%d\n",
			atomic_read(&mhi_dev_ctxt->counters.device_wake));
		read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
		mhi_dev_ctxt->assert_wake(mhi_dev_ctxt, true);
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
		read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	} else if (mhi_dev_ctxt->core.pci_master) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Schedule RPM suspend");
		pm_runtime_mark_last_busy(&mhi_dev_ctxt->pcie_device->dev);
		pm_request_autosuspend(&mhi_dev_ctxt->pcie_device->dev);
	}
	mutex_unlock(&mhi_dev_ctxt->pm_lock);
	return;

invalid_pm_state:
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mutex_unlock(&mhi_dev_ctxt->pm_lock);
}

int process_m3_transition(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	unsigned long flags;
	enum MHI_PM_STATE cur_state;
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Entered with State %s\n",
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));

	switch (mhi_dev_ctxt->mhi_state) {
	case MHI_STATE_M1:
		mhi_dev_ctxt->counters.m1_m3++;
		break;
	case MHI_STATE_M0:
		mhi_dev_ctxt->counters.m0_m3++;
		break;
	default:
		break;
	}

	write_lock_irqsave(&mhi_dev_ctxt->pm_xfer_lock, flags);
	mhi_dev_ctxt->mhi_state = MHI_STATE_M3;
	cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, MHI_PM_M3);
	write_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);
	if (unlikely(cur_state != MHI_PM_M3)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to transition to state 0x%x from 0x%x\n",
			MHI_PM_M3, cur_state);
		return -EIO;
	}
	wake_up(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	return 0;
}

static int process_ready_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	int r = 0;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Processing READY state transition\n");

	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->mhi_state = MHI_STATE_READY;
	if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		return -EIO;
	}
	r = mhi_init_mmio(mhi_dev_ctxt);
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	/* Initialize MMIO */
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failure during MMIO initialization\n");
		return r;
	}
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	r = mhi_add_elements_to_event_rings(mhi_dev_ctxt,
				cur_work_item);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failure during event ring init\n");
		return r;
	}

	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		return -EIO;
	}
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRL,
			MHICTRL_MHISTATE_MASK,
			MHICTRL_MHISTATE_SHIFT,
			MHI_STATE_M0);
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	return r;
}

static int process_reset_transition(
			struct mhi_device_ctxt *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	int r = 0;
	enum MHI_PM_STATE cur_state;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Processing RESET state transition\n");
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->mhi_state = MHI_STATE_RESET;
	cur_state = mhi_tryset_pm_state(mhi_dev_ctxt, MHI_PM_POR);
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	if (unlikely(cur_state != MHI_PM_POR)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Error transitining from state:0x%x to:0x%x\n",
			cur_state, MHI_PM_POR);
		return -EIO;
	}

	mhi_dev_ctxt->counters.mhi_reset_cntr++;
	r = mhi_test_for_device_reset(mhi_dev_ctxt);
	if (r)
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Device not RESET ret %d\n", r);
	r = mhi_test_for_device_ready(mhi_dev_ctxt);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"timed out waiting for ready ret:%d\n", r);
		return r;
	}

	r = mhi_init_state_transition(mhi_dev_ctxt,
				STATE_TRANSITION_READY);
	if (0 != r)
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to initiate %s state trans\n",
			state_transition_str(STATE_TRANSITION_READY));
	return r;
}

static void enable_clients(struct mhi_device_ctxt *mhi_dev_ctxt,
			   enum MHI_EXEC_ENV exec_env)
{
	struct mhi_client_handle *client_handle = NULL;
	struct mhi_chan_info *chan_info;
	int i = 0;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Enabling Clients, exec env %d.\n", exec_env);

	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
		if (!mhi_dev_ctxt->client_handle_list[i])
			continue;

		client_handle = mhi_dev_ctxt->client_handle_list[i];
		chan_info = &client_handle->client_config->chan_info;
		if (exec_env == GET_CHAN_PROPS(CHAN_BRINGUP_STAGE,
					       chan_info->flags)) {
			client_handle->enabled = true;
			mhi_notify_client(client_handle, MHI_CB_MHI_ENABLED);
		}
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Done.\n");
}

static int process_amss_transition(
				struct mhi_device_ctxt *mhi_dev_ctxt,
				enum STATE_TRANSITION cur_work_item)
{
	int r = 0;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Processing AMSS state transition\n");
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_AMSS;
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->flags.mhi_initialized = true;
	complete(&mhi_dev_ctxt->cmd_complete);

	r = mhi_add_elements_to_event_rings(mhi_dev_ctxt,
					cur_work_item);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to set local chan state ret %d\n", r);
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
		return r;
	}
	enable_clients(mhi_dev_ctxt, mhi_dev_ctxt->dev_exec_env);


	/*
	 * runtime_allow will decrement usage_count, counts were
	 * incremented by pci fw pci_pm_init() or by
	 * mhi shutdown/ssr apis.
	 */
	if (mhi_dev_ctxt->core.pci_master) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Allow runtime suspend\n");

		pm_runtime_mark_last_busy(&mhi_dev_ctxt->pcie_device->dev);
		pm_runtime_allow(&mhi_dev_ctxt->pcie_device->dev);
	}

	/* During probe we incremented, releasing that count */
	read_lock_bh(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
	read_unlock_bh(&mhi_dev_ctxt->pm_xfer_lock);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Exited\n");
	return 0;
}

void process_stt_work_item(
			struct mhi_device_ctxt  *mhi_dev_ctxt,
			enum STATE_TRANSITION cur_work_item)
{
	int r = 0;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Transitioning to %s\n",
		state_transition_str(cur_work_item));
	trace_mhi_state(cur_work_item);
	switch (cur_work_item) {
	case STATE_TRANSITION_BHI:
		write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		mhi_dev_ctxt->mhi_state = MHI_STATE_BHI;
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		wake_up_interruptible(mhi_dev_ctxt->mhi_ev_wq.bhi_event);
		break;
	case STATE_TRANSITION_RESET:
		r = process_reset_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_READY:
		r = process_ready_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_SBL:
		write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_SBL;
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		enable_clients(mhi_dev_ctxt, mhi_dev_ctxt->dev_exec_env);
		break;
	case STATE_TRANSITION_AMSS:
		r = process_amss_transition(mhi_dev_ctxt, cur_work_item);
		break;
	case STATE_TRANSITION_BHIE:
		write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_BHIE;
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		wake_up(mhi_dev_ctxt->mhi_ev_wq.bhi_event);
		break;
	case STATE_TRANSITION_RDDM:
		write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		mhi_dev_ctxt->dev_exec_env = MHI_EXEC_ENV_RDDM;
		mhi_dev_ctxt->deassert_wake(mhi_dev_ctxt);
		write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
		complete(&mhi_dev_ctxt->cmd_complete);

		/* Notify bus master device entered rddm mode */
		if (!mhi_dev_ctxt->core.pci_master) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Notifying bus master RDDM Status\n");
			mhi_dev_ctxt->status_cb(MHI_CB_RDDM,
						mhi_dev_ctxt->priv_data);
		}
		break;
	default:
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Unrecongized state: %s\n",
			state_transition_str(cur_work_item));
		BUG();
		break;
	}
}

void mhi_state_change_worker(struct work_struct *work)
{
	int r;
	struct mhi_device_ctxt *mhi_dev_ctxt = container_of(work,
				    struct mhi_device_ctxt,
				    st_thread_worker);
	enum STATE_TRANSITION cur_work_item;
	struct mhi_state_work_queue *work_q =
			&mhi_dev_ctxt->state_change_work_item_list;
	struct mhi_ring *state_change_q = &work_q->q_info;

	while (work_q->q_info.rp != work_q->q_info.wp) {
		spin_lock_irq(work_q->q_lock);
		cur_work_item = *(enum STATE_TRANSITION *)(state_change_q->rp);
		r = ctxt_del_element(&work_q->q_info, NULL);
		MHI_ASSERT(r == 0,
			"Failed to delete element from STT workqueue\n");
		spin_unlock_irq(work_q->q_lock);
		process_stt_work_item(mhi_dev_ctxt, cur_work_item);
	}
}

/**
 * mhi_init_state_transition - Add a new state transition work item to
 *			the state transition thread work item list.
 *
 * @mhi_dev_ctxt	The mhi_dev_ctxt context
 * @new_state		The state we wish to transition to
 *
 */
int mhi_init_state_transition(struct mhi_device_ctxt *mhi_dev_ctxt,
		enum STATE_TRANSITION new_state)
{
	unsigned long flags = 0;
	int r = 0, nr_avail_work_items = 0;
	enum STATE_TRANSITION *cur_work_item = NULL;

	struct mhi_ring *stt_ring =
		&mhi_dev_ctxt->state_change_work_item_list.q_info;
	struct mhi_state_work_queue *work_q =
			&mhi_dev_ctxt->state_change_work_item_list;

	spin_lock_irqsave(work_q->q_lock, flags);
	nr_avail_work_items =
		get_nr_avail_ring_elements(mhi_dev_ctxt, stt_ring);

	BUG_ON(nr_avail_work_items <= 0);
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Processing state transition %s\n",
		state_transition_str(new_state));
	*(enum STATE_TRANSITION *)stt_ring->wp = new_state;
	r = ctxt_add_element(stt_ring, (void **)&cur_work_item);
	BUG_ON(r);
	spin_unlock_irqrestore(work_q->q_lock, flags);
	schedule_work(&mhi_dev_ctxt->st_thread_worker);
	return r;
}
