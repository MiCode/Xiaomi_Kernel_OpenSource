/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef HELIOSCOM_INTERFACE_H
#define HELIOSCOM_INTERFACE_H

/*
 * helios_soft_reset() - soft reset Helios
 * Return 0 on success or -Ve on error
 */
int helios_soft_reset(void);

/*
 * is_twm_exit()
 * Return true if device is booting up on TWM exit.
 * value is auto cleared once read.
 */
bool is_twm_exit(void);

/*
 * is_helios_running()
 * Return true if helios is running.
 * value is auto cleared once read.
 */
bool is_helios_running(void);

/*
 * set_helios_dsp_state()
 * Set helios dsp state
 */
void set_helios_dsp_state(bool status);

/*
 * set_helios_bt_state()
 * Set helios bt state
 */
void set_helios_bt_state(bool status);
void helioscom_intf_notify_glink_channel_state(bool state);
void helioscom_rx_msg(void *data, int len);

/*
 * Message header type - generic header structure
 */
struct msg_header_t {
	uint32_t opcode;
	uint32_t payload_size;
};

/**
 * Opcodes to be received on helios-control channel.
 */
enum WMHeliosCtrlChnlOpcode {
	/*
	 * Command to helios to enter TWM mode
	 */
	GMI_MGR_ENTER_TWM = 1,

	/*
	 * Notification to helios about Modem Processor Sub System
	 * is down due to a subsystem reset.
	 */
	GMI_MGR_SSR_MPSS_DOWN_NOTIFICATION = 2,

	/*
	 * Notification to helios about Modem Processor Sub System
	 * being brought up after a subsystem reset.
	 */
	GMI_MGR_SSR_MPSS_UP_NOTIFICATION = 3,

	/*
	 * Notification to helios about ADSP Sub System
	 * is down due to a subsystem reset.
	 */
	GMI_MGR_SSR_ADSP_DOWN_INDICATION = 8,

	/*
	 * Notification to helios about Modem Processor
	 * Sub System being brought up after a subsystem reset.
	 */
	GMI_MGR_SSR_ADSP_UP_INDICATION = 9,

	/*
	 * Notification to MSM for generic wakeup in tracker mode
	 */
	GMI_MGR_WAKE_UP_NO_REASON = 10,

	/*
	 * Notification to Helios About Entry to Tracker-DS
	 */
	GMI_MGR_ENTER_TRACKER_DS = 11,

	/*
	 * Notification to Helios About Entry to Tracker-DS
	 */
	GMI_MGR_EXIT_TRACKER_DS = 12,

	/*
	 * Notification to Helios About Time-sync update
	 */
	GMI_MGR_TIME_SYNC_UPDATE = 13,	/* payload struct: time_sync_t*/

	/*
	 * Notification to Helios About Timeval UTC
	 */
	GMI_MGR_TIMEVAL_UTC = 14,		/* payload struct: timeval_utc_t*/

	/*
	 * Notification to Helios About Daylight saving time
	 */
	GMI_MGR_DST = 15,			/* payload struct: dst_t*/

};
#endif /* HELIOSCOM_INTERFACE_H */

