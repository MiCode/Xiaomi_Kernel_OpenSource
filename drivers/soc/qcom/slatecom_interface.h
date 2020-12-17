/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#ifndef SLATECOM_INTERFACE_H
#define SLATECOM_INTERFACE_H

/*
 * slate_soft_reset() - soft reset Blackghost
 * Return 0 on success or -Ve on error
 */
int slate_soft_reset(void);

/*
 * is_twm_exit()
 * Return true if device is booting up on TWM exit.
 * value is auto cleared once read.
 */
bool is_twm_exit(void);

/*
 * is_slate_running()
 * Return true if slate is running.
 * value is auto cleared once read.
 */
bool is_slate_running(void);

/*
 * set_slate_dsp_state()
 * Set slate dsp state
 */
void set_slate_dsp_state(bool status);

/*
 * set_slate_bt_state()
 * Set slate bt state
 */
void set_slate_bt_state(bool status);

#endif /* SLATECOM_INTERFACE_H */

