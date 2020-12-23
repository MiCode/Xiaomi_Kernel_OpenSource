/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015,2017-2018,2020 The Linux Foundation. All rights reserved.*/

#ifndef MHI_SM_H
#define MHI_SM_H

#include "mhi.h"
#include <linux/slab.h>
#include <linux/msm_ep_pcie.h>


/**
 * enum mhi_dev_event - MHI state change events
 * @MHI_DEV_EVENT_CTRL_TRIG: CTRL register change event.
 *				Not supported,for future use
 * @MHI_DEV_EVENT_M0_STATE: M0 state change event
 * @MHI_DEV_EVENT_M1_STATE: M1 state change event. Not supported, for future use
 * @MHI_DEV_EVENT_M2_STATE: M2 state change event. Not supported, for future use
 * @MHI_DEV_EVENT_M3_STATE: M0 state change event
 * @MHI_DEV_EVENT_HW_ACC_WAKEUP: pendding data on IPA, initiate Host wakeup
 * @MHI_DEV_EVENT_CORE_WAKEUP: MHI core initiate Host wakup
 */
enum mhi_dev_event {
	MHI_DEV_EVENT_CTRL_TRIG,
	MHI_DEV_EVENT_M0_STATE,
	MHI_DEV_EVENT_M1_STATE,
	MHI_DEV_EVENT_M2_STATE,
	MHI_DEV_EVENT_M3_STATE,
	MHI_DEV_EVENT_HW_ACC_WAKEUP,
	MHI_DEV_EVENT_CORE_WAKEUP,
	MHI_DEV_EVENT_MAX
};

int mhi_dev_sm_init(struct mhi_dev *dev);
int mhi_dev_sm_exit(struct mhi_dev *dev);
int mhi_dev_sm_set_ready(void);
int mhi_dev_notify_sm_event(enum mhi_dev_event event);
int mhi_dev_sm_get_mhi_state(enum mhi_dev_state *state);
int mhi_dev_sm_syserr(void);
void mhi_dev_sm_pcie_handler(struct ep_pcie_notify *notify);

#endif /* MHI_SM_H */

