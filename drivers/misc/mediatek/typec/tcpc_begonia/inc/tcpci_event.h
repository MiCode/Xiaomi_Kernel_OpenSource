/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TCPC_EVENT_BUF_H_INCLUDED
#define TCPC_EVENT_BUF_H_INCLUDED

#include "tcpci_timer.h"
#include "tcpm.h"


#define PD_MSG_BUF_SIZE		(4*2)
#define PD_EVENT_BUF_SIZE	(8*2)
#define TCP_EVENT_BUF_SIZE	(2*2)

struct tcpc_device;

struct pd_msg {
	uint8_t frame_type;
	uint16_t msg_hdr;
	uint32_t payload[7];
	unsigned long time_stamp;
};

struct pd_event {
	uint8_t event_type;
	uint8_t msg;
	uint8_t msg_sec;
	struct pd_msg *pd_msg;
};

struct pd_msg *pd_alloc_msg(struct tcpc_device *tcpc_dev);
void pd_free_msg(struct tcpc_device *tcpc_dev, struct pd_msg *pd_msg);

bool pd_get_event(struct tcpc_device *tcpc_dev, struct pd_event *pd_event);
bool pd_put_event(struct tcpc_device *tcpc_dev,
		const struct pd_event *pd_event, bool from_port_partner);
void pd_free_event(struct tcpc_device *tcpc_dev, struct pd_event *pd_event);

bool pd_get_vdm_event(struct tcpc_device *tcpc_dev, struct pd_event *pd_event);
bool pd_put_vdm_event(struct tcpc_device *tcpc_dev,
			struct pd_event *pd_event, bool from_port_partner);

bool pd_put_last_vdm_event(struct tcpc_device *tcpc_dev);

bool pd_get_deferred_tcp_event(
	struct tcpc_device *tcpc_dev, struct tcp_dpm_event *tcp_event);
bool pd_put_deferred_tcp_event(
	struct tcpc_device *tcpc_dev, const struct tcp_dpm_event *tcp_event);

extern int tcpci_event_init(struct tcpc_device *tcpc_dev);
extern int tcpci_event_deinit(struct tcpc_device *tcpc_dev);
extern void pd_event_buf_reset(struct tcpc_device *tcpc_dev);

bool pd_put_cc_attached_event(struct tcpc_device *tcpc_dev, uint8_t type);
void pd_put_cc_detached_event(struct tcpc_device *tcpc_dev);
void pd_put_recv_hard_reset_event(struct tcpc_device *tcpc_dev);
void pd_put_sent_hard_reset_event(struct tcpc_device *tcpc_dev);
bool pd_put_pd_msg_event(struct tcpc_device *tcpc_dev, struct pd_msg *pd_msg);
void pd_put_hard_reset_completed_event(struct tcpc_device *tcpc_dev);
void pd_put_vbus_changed_event(struct tcpc_device *tcpc_dev, bool from_ic);
void pd_put_vbus_safe0v_event(struct tcpc_device *tcpc_dev);
void pd_put_vbus_stable_event(struct tcpc_device *tcpc_dev);
void pd_put_vbus_present_event(struct tcpc_device *tcpc_dev);

enum pd_event_type {
	PD_EVT_PD_MSG = 0,	/* either ctrl msg or data msg */
	PD_EVT_CTRL_MSG,
	PD_EVT_DATA_MSG,

#ifdef CONFIG_USB_PD_REV30
	PD_EVT_EXT_MSG,
#endif	/* CONFIG_USB_PD_REV30 */

	PD_EVT_PD_MSG_END,

	PD_EVT_DPM_MSG = PD_EVT_PD_MSG_END,
	PD_EVT_HW_MSG,
	PD_EVT_PE_MSG,
	PD_EVT_TIMER_MSG,

	PD_EVT_TCP_MSG,
};

enum pd_msg_type {
/* Control Message type */
	/* 0 Reserved */
	PD_CTRL_GOOD_CRC = 1,
	PD_CTRL_GOTO_MIN = 2,
	PD_CTRL_ACCEPT = 3,
	PD_CTRL_REJECT = 4,
	PD_CTRL_PING = 5,
	PD_CTRL_PS_RDY = 6,
	PD_CTRL_GET_SOURCE_CAP = 7,
	PD_CTRL_GET_SINK_CAP = 8,
	PD_CTRL_DR_SWAP = 9,
	PD_CTRL_PR_SWAP = 10,
	PD_CTRL_VCONN_SWAP = 11,
	PD_CTRL_WAIT = 12,
	PD_CTRL_SOFT_RESET = 13,
	/* 14-15 Reserved */
	PD_CTRL_PD30_START = 0x10 + 0,
#ifdef CONFIG_USB_PD_REV30
	PD_CTRL_NOT_SUPPORTED = 0x10 + 0,
	PD_CTRL_GET_SOURCE_CAP_EXT = 0x10 + 1,
	PD_CTRL_GET_STATUS = 0x10 + 2,
	PD_CTRL_FR_SWAP = 0x10 + 3,
	PD_CTRL_GET_PPS_STATUS = 0x10 + 4,
	PD_CTRL_GET_COUNTRY_CODE = 0x10 + 5,
#endif	/* CONFIG_USB_PD_REV30 */
	/* 22-31 Reserved */
	PD_CTRL_MSG_NR,
/* Data message type */
	/* 0 Reserved */
	PD_DATA_SOURCE_CAP = 1,
	PD_DATA_REQUEST = 2,
	PD_DATA_BIST = 3,
	PD_DATA_SINK_CAP = 4,
	PD_DATA_PD30_START = 5,
#ifdef CONFIG_USB_PD_REV30
	PD_DATA_BAT_STATUS = 5,
	PD_DATA_ALERT = 6,
	PD_DATA_GET_COUNTRY_INFO = 7,
#endif	/* CONFIG_USB_PD_REV30 */
	/* 7-14 Reserved */
	PD_DATA_VENDOR_DEF = 15,
	PD_DATA_MSG_NR,
#ifdef CONFIG_USB_PD_REV30
/* Extended message type */
	/* 0 Reserved */
	PD_EXT_SOURCE_CAP_EXT = 1,
	PD_EXT_STATUS = 2,
	PD_EXT_GET_BAT_CAP = 3,
	PD_EXT_GET_BAT_STATUS = 4,
	PD_EXT_BAT_CAP = 5,
	PD_EXT_GET_MFR_INFO = 6,
	PD_EXT_MFR_INFO = 7,
	PD_EXT_SEC_REQUEST = 8,
	PD_EXT_SEC_RESPONSE = 9,
	PD_EXT_FW_UPDATE_REQUEST = 10,
	PD_EXT_FW_UPDATE_RESPONSE = 11,
	PD_EXT_PPS_STATUS = 12,
	PD_EXT_COUNTRY_INFO = 13,
	PD_EXT_COUNTRY_CODES = 14,
	/* 15 Reserved */
	PD_EXT_MSG_NR,
#endif	/* CONFIG_USB_PD_REV30 */
/* HW Message type */
	PD_HW_CC_DETACHED = 0,
	PD_HW_CC_ATTACHED,
	PD_HW_RECV_HARD_RESET,
	PD_HW_VBUS_PRESENT,
	PD_HW_VBUS_ABSENT,
	PD_HW_VBUS_SAFE0V,
	PD_HW_VBUS_STABLE,
	PD_HW_TX_FAILED,	/* no good crc or discard */
	PD_HW_TX_DISCARD,	/* discard vdm msg */
	PD_HW_RETRY_VDM,	/* discard vdm msg (retry) */
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	PD_HW_SINK_TX_CHANGE,
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
	PD_HW_MSG_NR,
/* PE Message type*/
	PD_PE_RESET_PRL_COMPLETED = 0,
	PD_PE_POWER_ROLE_AT_DEFAULT,
	PD_PE_HARD_RESET_COMPLETED,
	PD_PE_IDLE,
	PD_PE_VDM_RESET,
	PD_PE_VDM_NOT_SUPPORT,
	PD_PE_MSG_NR,
/* DPM Message type */
	PD_DPM_NOTIFIED = 0,
	PD_DPM_ACK = PD_DPM_NOTIFIED,
	PD_DPM_NAK,
	PD_DPM_CAP_CHANGED,
	PD_DPM_NOT_SUPPORT,
	PD_DPM_MSG_NR,
};

enum pd_dpm_notify_type {
	PD_DPM_NOTIFY_OK = 0,
	PD_DPM_NOTIFY_CAP_MISMATCH,
};

enum pd_dpm_nak_type {
	PD_DPM_NAK_REJECT = 0,
	PD_DPM_NAK_WAIT = 1,
};

enum pd_tcp_sec_msg_type {
	PD_TCP_FROM_TCPM = 0,
	PD_TCP_FROM_PE = 1,
};

enum pd_tx_transmit_state {
	PD_TX_STATE_GOOD_CRC = 0,
	PD_TX_STATE_NO_GOOD_CRC,
	PD_TX_STATE_DISCARD,
	PD_TX_STATE_HARD_RESET,
	PD_TX_STATE_NO_RESPONSE,

	PD_TX_STATE_WAIT,
	PD_TX_STATE_WAIT_CRC_VDM = PD_TX_STATE_WAIT,
	PD_TX_STATE_WAIT_CRC_PD,
	PD_TX_STATE_WAIT_HARD_RESET,
};

static inline bool pd_event_msg_match(struct pd_event *pd_event,
					uint8_t type, uint8_t msg)
{
	if (pd_event->event_type != type)
		return false;

	return pd_event->msg == msg;
}

static inline bool pd_event_ctrl_msg_match(
		struct pd_event *pd_event, uint8_t msg)
{
	return pd_event_msg_match(pd_event, PD_EVT_CTRL_MSG, msg);
}

static inline bool pd_event_data_msg_match(
		struct pd_event *pd_event, uint8_t msg)
{
	return pd_event_msg_match(pd_event, PD_EVT_DATA_MSG, msg);
}

static inline bool pd_event_hw_msg_match(struct pd_event *pd_event, uint8_t msg)
{
	return pd_event_msg_match(pd_event, PD_EVT_HW_MSG, msg);
}

static inline bool pd_event_pe_msg_match(struct pd_event *pd_event, uint8_t msg)
{
	return pd_event_msg_match(pd_event, PD_EVT_PE_MSG, msg);
}

static inline bool pd_event_timer_msg_match(
			struct pd_event *pd_event, uint8_t msg)
{
	return pd_event_msg_match(pd_event, PD_EVT_TIMER_MSG, msg);
}

#ifdef CONFIG_USB_PD_REV30

static inline bool pd_event_ext_msg_match(
	struct pd_event *pd_event, uint8_t msg)
{
	return pd_event_msg_match(pd_event, PD_EVT_EXT_MSG, msg);
}

#endif	/* CONFIG_USB_PD_REV30 */

#endif /* TCPC_EVENT_BUF_H_INCLUDED */
