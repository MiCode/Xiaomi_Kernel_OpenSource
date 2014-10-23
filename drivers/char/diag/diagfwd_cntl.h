/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DIAGFWD_CNTL_H
#define DIAGFWD_CNTL_H

/* Message registration commands */
#define DIAG_CTRL_MSG_REG		1
/* Message passing for DTR events */
#define DIAG_CTRL_MSG_DTR		2
/* Control Diag sleep vote, buffering etc */
#define DIAG_CTRL_MSG_DIAGMODE		3
/* Diag data based on "light" diag mask */
#define DIAG_CTRL_MSG_DIAGDATA		4
/* Send diag internal feature mask 'diag_int_feature_mask' */
#define DIAG_CTRL_MSG_FEATURE		8
/* Send Diag log mask for a particular equip id */
#define DIAG_CTRL_MSG_EQUIP_LOG_MASK	9
/* Send Diag event mask */
#define DIAG_CTRL_MSG_EVENT_MASK_V2	10
/* Send Diag F3 mask */
#define DIAG_CTRL_MSG_F3_MASK_V2	11
#define DIAG_CTRL_MSG_NUM_PRESETS	12
#define DIAG_CTRL_MSG_SET_PRESET_ID	13
#define DIAG_CTRL_MSG_LOG_MASK_WITH_PRESET_ID	14
#define DIAG_CTRL_MSG_EVENT_MASK_WITH_PRESET_ID	15
#define DIAG_CTRL_MSG_F3_MASK_WITH_PRESET_ID	16
#define DIAG_CTRL_MSG_CONFIG_PERIPHERAL_TX_MODE	17
#define DIAG_CTRL_MSG_PERIPHERAL_BUF_DRAIN_IMM	18
#define DIAG_CTRL_MSG_CONFIG_PERIPHERAL_WMQ_VAL	19
#define DIAG_CTRL_MSG_DCI_CONNECTION_STATUS	20
#define DIAG_CTRL_MSG_LAST_EVENT_REPORT		22
#define DIAG_CTRL_MSG_LOG_RANGE_REPORT		23
#define DIAG_CTRL_MSG_SSID_RANGE_REPORT		24
#define DIAG_CTRL_MSG_BUILD_MASK_REPORT		25
#define DIAG_CTRL_MSG_DCI_HANDSHAKE_PKT		29

/*
 * Feature Mask Definitions: Feature mask is used to sepcify Diag features
 * supported by the Apps processor
 *
 * F_DIAG_FEATURE_MASK_SUPPORT - Denotes we support sending and receiving
 *                               feature masks
 * F_DIAG_LOG_ON_DEMAND_APPS - Apps responds to Log on Demand request
 * F_DIAG_REQ_RSP_SUPPORT - Apps supported dedicated request response Channel
 * F_DIAG_APPS_HDLC_ENCODE - HDLC encoding is done on the forward channel
 * F_DIAG_STM - Denotes Apps supports Diag over STM
 */
#define F_DIAG_FEATURE_MASK_SUPPORT		0
#define F_DIAG_LOG_ON_DEMAND_APPS		2
#define F_DIAG_REQ_RSP_SUPPORT			4
#define F_DIAG_APPS_HDLC_ENCODE			6
#define F_DIAG_STM				9
#define F_DIAG_PERIPHERAL_BUFFERING		10
#define F_DIAG_MASK_CENTRALIZATION		11

#define ENABLE_SEPARATE_CMDRSP	1
#define DISABLE_SEPARATE_CMDRSP	0

#define DISABLE_STM	0
#define ENABLE_STM	1
#define STATUS_STM	2

#define UPDATE_PERIPHERAL_STM_STATE	1
#define CLEAR_PERIPHERAL_STM_STATE	2

#define ENABLE_APPS_HDLC_ENCODING	1
#define DISABLE_APPS_HDLC_ENCODING	0

#define DIAG_MODE_PKT_LEN	36

struct diag_ctrl_pkt_header_t {
	uint32_t pkt_id;
	uint32_t len;
};

struct cmd_code_range {
	uint16_t cmd_code_lo;
	uint16_t cmd_code_hi;
	uint32_t data;
};

struct diag_ctrl_cmd_reg {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint16_t cmd_code;
	uint16_t subsysid;
	uint16_t count_entries;
	uint16_t port;
};

struct diag_ctrl_event_mask {
	uint32_t cmd_type;
	uint32_t data_len;
	uint8_t stream_id;
	uint8_t status;
	uint8_t event_config;
	uint32_t event_mask_size;
	/* Copy event mask here */
} __packed;

struct diag_ctrl_log_mask {
	uint32_t cmd_type;
	uint32_t data_len;
	uint8_t stream_id;
	uint8_t status;
	uint8_t equip_id;
	uint32_t num_items; /* Last log code for this equip_id */
	uint32_t log_mask_size; /* Size of log mask stored in log_mask[] */
	/* Copy log mask here */
} __packed;

struct diag_ctrl_msg_mask {
	uint32_t cmd_type;
	uint32_t data_len;
	uint8_t stream_id;
	uint8_t status;
	uint8_t msg_mode;
	uint16_t ssid_first; /* Start of range of supported SSIDs */
	uint16_t ssid_last; /* Last SSID in range */
	uint32_t msg_mask_size; /* ssid_last - ssid_first + 1 */
	/* Copy msg mask here */
} __packed;

struct diag_ctrl_feature_mask {
	uint32_t ctrl_pkt_id;
	uint32_t ctrl_pkt_data_len;
	uint32_t feature_mask_len;
	/* Copy feature mask here */
} __packed;

struct diag_ctrl_msg_diagmode {
	uint32_t ctrl_pkt_id;
	uint32_t ctrl_pkt_data_len;
	uint32_t version;
	uint32_t sleep_vote;
	uint32_t real_time;
	uint32_t use_nrt_values;
	uint32_t commit_threshold;
	uint32_t sleep_threshold;
	uint32_t sleep_time;
	uint32_t drain_timer_val;
	uint32_t event_stale_timer_val;
} __packed;

struct diag_ctrl_msg_stm {
	uint32_t ctrl_pkt_id;
	uint32_t ctrl_pkt_data_len;
	uint32_t version;
	uint8_t  control_data;
} __packed;

struct diag_ctrl_dci_status {
	uint32_t ctrl_pkt_id;
	uint32_t ctrl_pkt_data_len;
	uint32_t version;
	uint8_t count;
} __packed;

struct diag_ctrl_dci_handshake_pkt {
	uint32_t ctrl_pkt_id;
	uint32_t ctrl_pkt_data_len;
	uint32_t version;
	uint32_t magic;
} __packed;

struct diag_ctrl_last_event_report {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint16_t event_last_id;
} __packed;

struct diag_ctrl_log_range_report {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint32_t last_equip_id;
	uint32_t num_ranges;
} __packed;

struct diag_ctrl_log_range {
	uint32_t equip_id;
	uint32_t num_items;
} __packed;

struct diag_ctrl_ssid_range_report {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint32_t count;
} __packed;

struct diag_ctrl_build_mask_report {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint32_t count;
} __packed;

struct diag_ctrl_peripheral_tx_mode {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint8_t stream_id;
	uint8_t tx_mode;
} __packed;

struct diag_ctrl_drain_immediate {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint8_t stream_id;
} __packed;

struct diag_ctrl_set_wq_val {
	uint32_t pkt_id;
	uint32_t len;
	uint32_t version;
	uint8_t stream_id;
	uint8_t high_wm_val;
	uint8_t low_wm_val;
} __packed;

int diagfwd_cntl_init(void);
void diagfwd_cntl_exit(void);
void diag_read_smd_cntl_work_fn(struct work_struct *);
void diag_notify_ctrl_update_fn(struct work_struct *work);
void diag_clean_reg_fn(struct work_struct *work);
void diag_cntl_smd_work_fn(struct work_struct *work);
int diag_process_smd_cntl_read_data(struct diag_smd_info *smd_info, void *buf,
								int total_recd);
int diag_send_diag_mode_update_by_smd(struct diag_smd_info *smd_info,
							int real_time);
int diag_send_peripheral_buffering_mode(struct diag_buffering_mode_t *params);
void diag_update_proc_vote(uint16_t proc, uint8_t vote, int index);
void diag_update_real_time_vote(uint16_t proc, uint8_t real_time, int index);
void diag_real_time_work_fn(struct work_struct *work);
int diag_send_stm_state(struct diag_smd_info *smd_info,
				uint8_t stm_control_data);
int diag_send_peripheral_drain_immediate(struct diag_smd_info *smd_info);
int diag_send_buffering_tx_mode_pkt(struct diag_smd_info *smd_info,
				    struct diag_buffering_mode_t *params);
int diag_send_buffering_wm_values(struct diag_smd_info *smd_info,
				  struct diag_buffering_mode_t *params);
void diag_cntl_stm_notify(struct diag_smd_info *smd_info, int action);

#endif
