/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef _MHI_MISC_H_
#define _MHI_MISC_H_

/**
 * mhi_set_m2_timeout_ms - Set M2 timeout in milliseconds to wait before a
 * fast/silent suspend
 * @mhi_cntrl: MHI controller
 * @timeout: timeout in ms
 */
void mhi_set_m2_timeout_ms(struct mhi_controller *mhi_cntrl, u32 timeout);

/**
 * mhi_pm_fast_resume - Resume MHI from a fast/silent suspended state
 * @mhi_cntrl: MHI controller
 * @notify_clients: if true, clients will be notified of the resume transition
 */
int mhi_pm_fast_resume(struct mhi_controller *mhi_cntrl, bool notify_clients);

/**
 * mhi_pm_fast_suspend - Move MHI into a fast/silent suspended state
 * @mhi_cntrl: MHI controller
 * @notify_clients: if true, clients will be notified of the suspend transition
 */
int mhi_pm_fast_suspend(struct mhi_controller *mhi_cntrl, bool notify_clients);

/**
 * mhi_debug_reg_dump - dump MHI registers for debug purpose
 * @mhi_cntrl: MHI controller
 */
void mhi_debug_reg_dump(struct mhi_controller *mhi_cntrl);

/**
 * mhi_dump_sfr - Print SFR string from RDDM table.
 * @mhi_cntrl: MHI controller
 */
void mhi_dump_sfr(struct mhi_controller *mhi_cntrl);

/**
 * mhi_scan_rddm_cookie - Look for supplied cookie value in the BHI debug
 * registers set by device to indicate rddm readiness for debugging purposes.
 * @mhi_cntrl: MHI controller
 * @cookie: cookie/pattern value to match
 *
 * Returns:
 * true if cookie is found
 * false if cookie is not found
 */
bool mhi_scan_rddm_cookie(struct mhi_controller *mhi_cntrl, u32 cookie);

/**
 * mhi_device_get_sync_atomic - Asserts device_wait and moves device to M0
 * @mhi_dev: Device associated with the channels
 * @timeout_us: timeout, in micro-seconds
 * @in_panic: If requested while kernel is in panic state and no ISRs expected
 *
 * The device_wake is asserted to keep device in M0 or bring it to M0.
 * If device is not in M0 state, then this function will wait for device to
 * move to M0, until @timeout_us elapses.
 * However, if device's M1 state-change event races with this function
 * then there is a possiblity of device moving from M0 to M2 and back
 * to M0. That can't be avoided as host must transition device from M1 to M2
 * as per the spec.
 * Clients can ignore that transition after this function returns as the device
 * is expected to immediately  move from M2 to M0 as wake is asserted and
 * wouldn't enter low power state.
 * If in_panic boolean is set, no ISRs are expected, hence this API will have to
 * resort to reading the MHI status register and poll on M0 state change.
 *
 * Returns:
 * 0 if operation was successful (however, M0 -> M2 -> M0 is possible later) as
 * mentioned above.
 * -ETIMEDOUT is device faled to move to M0 before @timeout_us elapsed
 * -EIO if the MHI state is one of the ERROR states.
 */
int mhi_device_get_sync_atomic(struct mhi_device *mhi_dev, int timeout_us,
			       bool in_panic);

#endif /* _MHI_MISC_H_ */
