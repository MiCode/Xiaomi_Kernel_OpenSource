/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef IPA_UC_HOLB_MONITOR_H
#define IPA_UC_HOLB_MONITOR_H

#define IPA_HOLB_MONITOR_MAX_STUCK_COUNT 5
#define IPA_HOLB_POLLING_PERIOD_MS 10
#define HOLB_OP 0x1
#define NOTIFY_AP_ON_HOLB 0x2
#define HOLB_MONITOR_MASK (HOLB_OP | NOTIFY_AP_ON_HOLB)
#define IPA_HOLB_CLIENT_MAX 30
#define IPA_HOLB_EVENT_LOG_MAX 20
#define IPA_CLIENT_IS_HOLB_CONS(x) \
(x == IPA_CLIENT_USB_CONS || x == IPA_CLIENT_WLAN2_CONS || \
x == IPA_CLIENT_WLAN1_CONS || x == IPA_CLIENT_WIGIG1_CONS || \
x == IPA_CLIENT_WIGIG2_CONS || x == IPA_CLIENT_WIGIG3_CONS || \
x == IPA_CLIENT_WIGIG4_CONS)

/*
 * enum holb_client_state - Client state for HOLB
 * IPA_HOLB_INIT : Initial state on bootup before client connect/disconnect
 * IPA_HOLB_ADD_PENDING : Client GSI channel started but uC not enabled.
 * IPA_HOLB_ADD : ADD_HOLB_MONITOR command sent for client.
 * IPA_HOLB_DEL : DEL_HOLB_MONITOR command sent for client.
 */
enum ipa_holb_client_state {
	IPA_HOLB_INIT = 0,
	IPA_HOLB_ADD_PENDING  = 1,
	IPA_HOLB_ADD  = 2,
	IPA_HOLB_DEL  = 3,
};

/**
 * struct ipa_holb_events - HOLB enable/disable events log
 * @qTimerLSB: LSB for event qtimer
 * @qTimerMSB: MSB for event qtimer
 * @enable: Even for enable/disable
 */
struct ipa_holb_events {
	uint32_t qTimerLSB;
	uint32_t qTimerMSB;
	bool enable;
};

/**
 * struct  ipa_uc_holb_client_info - Client info needed for HOLB callback
 * @gsi_chan_hdl: GSI Channel of the client to be monitored
 * @action_mask: HOLB action mask
 * @max_stuck_cnt: Max number of attempts uC should try before sending an event
 * @ee: EE that the chid belongs to
 * @debugfs_param: If debugfs is used to set the client parameters
 * @state: Client state
 * @events: HOLB enable/disable events log
 * @current_idx: index of current event
 * @enable_cnt: accumulate count for enable
 * @disable_cnt: accumulate count for disable
 */
struct ipa_uc_holb_client_info {
	uint16_t gsi_chan_hdl;
	uint32_t action_mask;
	uint32_t max_stuck_cnt;
	uint8_t ee;
	bool debugfs_param;
	enum ipa_holb_client_state state;
	struct ipa_holb_events events[IPA_HOLB_EVENT_LOG_MAX];
	uint32_t current_idx;
	uint32_t enable_cnt;
	uint32_t disable_cnt;
};

/**
 * struct ipa_holb_monitor - Parameters needed for the HOLB monitor feature
 * @num_holb_clients: Number of clients with holb monitor enabled
 * @client: Array of clients tracked for HOLB Monitor
 * @ipa_uc_holb_monitor_poll_period : Polling period in ms
 * @uc_holb_lock : Lock for feature operations
 *
 */
struct ipa_holb_monitor {
	u32 num_holb_clients;
	u32 poll_period;
	u32 max_cnt_wlan;
	u32 max_cnt_usb;
	u32 max_cnt_11ad;
	struct ipa_uc_holb_client_info client[IPA_HOLB_CLIENT_MAX];
	struct mutex uc_holb_lock;
};

/**
 * ipa3_uc_holb_client_handler - Iterates through all HOLB clients and sends
 * ADD_HOLB_MONITOR command if necessary.
 *
 */
void ipa3_uc_holb_client_handler(void);

/**
 * ipa3_uc_client_add_holb_monitor() - Sends ADD_HOLB_MONITOR for gsi channels
 * if uC is enabled, else saves client state
 * @gsi_chan_hdl: GSI Channel of the client to be monitored
 * @action_mask: HOLB action mask
 * @max_stuck_cnt: Max number of attempts uC should try before sending an event
 * @ee: EE that the chid belongs to
 *
 * Return value: 0 on success, negative value otherwise
 */
int ipa3_uc_client_add_holb_monitor(uint16_t gsi_ch, uint32_t action_mask,
		uint32_t max_stuck_cnt, uint8_t ee);

/**
 * ipa3_uc_client_del_holb_monitor() - Sends DEL_HOLB_MONITOR for gsi channels
 * if uC is enabled, else saves client state
 * @gsi_chan_hdl: GSI Channel of the client to be monitored
 * @ee: EE that the chid belongs to
 *
 * Return value: 0 on success, negative value otherwise
 */
int ipa3_uc_client_del_holb_monitor(uint16_t gsi_ch, uint8_t ee);

/**
 * ipa3_set_holb_client_by_ch() - Set client parameters for specific
 * gsi channel
 * @client: Client values to be set for the gsi channel
 *
 */
void ipa3_set_holb_client_by_ch(struct ipa_uc_holb_client_info client);

/**
 * ipa3_uc_holb_event_log() - Log HOLB event for specific gsi
 * channel
 * @gsi_ch: Client values to be set for the gsi channel
 * @enable: event is for enable/disable
 * @qtimer_lsb: msb for event qtimer
 * @qtimer_msb: lsb for event qtimer
 */
void ipa3_uc_holb_event_log(uint16_t gsi_ch, bool enable,
	uint32_t qtimer_lsb, uint32_t qtimer_msb);

#endif /* IPA_UC_HOLB_MONITOR_H */
