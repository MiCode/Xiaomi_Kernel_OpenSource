/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef PD_POLICY_ENGINE_H_
#define PD_POLICY_ENGINE_H_

#include "pd_core.h"

/* ---- Policy Engine State ---- */

enum pd_pe_state_machine {
	PE_STATE_MACHINE_IDLE = 0,
	PE_STATE_MACHINE_NORMAL,
	PE_STATE_MACHINE_DR_SWAP,
	PE_STATE_MACHINE_PR_SWAP,
	PE_STATE_MACHINE_VCONN_SWAP,
};

/* ---- Policy Engine Runtime Flags ---- */

#define PE_STATE_FLAG_BACK_READY_IF_RECV_WAIT		(1<<0)
#define PE_STATE_FLAG_BACK_READY_IF_RECV_REJECT		(1<<1)
#define PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT	(1<<2)
#define PE_STATE_FLAG_BACK_READY_IF_TX_FAILED			(1<<3)
#define PE_STATE_FLAG_HRESET_IF_SR_TIMEOUT			(1<<4)
#define PE_STATE_FLAG_HRESET_IF_TX_FAILED				(1<<5)
#define PE_STATE_FLAG_IGNORE_UNKNOWN_EVENT			(1<<6)
#define PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER	(1<<7)

#define PE_STATE_WAIT_RESPONSE(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_WAIT_MSG(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT |\
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_WAIT_MSG_HRESET_IF_TOUT(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_HRESET_IF_SR_TIMEOUT |\
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_WAIT_MSG_OR_TX_FAILED(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_BACK_READY_IF_TX_FAILED |\
	PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT |\
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_WAIT_MSG_OR_RJ(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_BACK_READY_IF_RECV_REJECT |\
	PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT |\
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_WAIT_ANSWER_MSG(pd_port)	{\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_BACK_READY_IF_RECV_WAIT |\
	PE_STATE_FLAG_BACK_READY_IF_RECV_REJECT |\
	PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT |\
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_HRESET_IF_TX_FAILED(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_HRESET_IF_TX_FAILED; }

#define PE_STATE_IGNORE_UNKNOWN_EVENT(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_IGNORE_UNKNOWN_EVENT; }

#define PE_STATE_RECV_SOFT_RESET(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_HRESET_IF_TX_FAILED | \
	PE_STATE_FLAG_IGNORE_UNKNOWN_EVENT; }

#define PE_STATE_SEND_SOFT_RESET(pd_port) {\
	pd_port->pe_data.pe_state_flags = \
	PE_STATE_FLAG_HRESET_IF_TX_FAILED |\
	PE_STATE_FLAG_HRESET_IF_SR_TIMEOUT |\
	PE_STATE_FLAG_IGNORE_UNKNOWN_EVENT |\
	PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER; }

#define PE_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC	(1<<0)
#define PE_STATE_FLAG_BACK_READY_IF_DPM_ACK			(1<<1)
#define PE_STATE_FLAG_DPM_ACK_IMMEDIATELY				(1<<7)

#define PE_STATE_WAIT_TX_SUCCESS(pd_port)	{\
	pd_port->pe_data.pe_state_flags2 = \
	PE_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC; }

#define PE_STATE_DPM_INFORMED(pd_port)	{\
	pd_port->pe_data.pe_state_flags2 = \
	PE_STATE_FLAG_BACK_READY_IF_DPM_ACK |\
	PE_STATE_FLAG_DPM_ACK_IMMEDIATELY; }

#define PE_STATE_WAIT_DPM_ACK(pd_port) {\
	pd_port->pe_data.pe_state_flags2 = \
	PE_STATE_FLAG_BACK_READY_IF_DPM_ACK; }

#define PE_STATE_DPM_ACK_IMMEDIATELY(pd_port) {\
	pd_port->pe_data.pe_state_flags2 |= \
	PE_STATE_FLAG_DPM_ACK_IMMEDIATELY; }

#define VDM_STATE_FLAG_DPM_ACK_IMMEDIATELY	(1<<4)
#define VDM_STATE_FLAG_BACK_READY_IF_DPM_ACK		(1<<6)
#define VDM_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC	(1<<7)

#define VDM_STATE_DPM_INFORMED(pd_port)	{\
	pd_port->pe_data.vdm_state_flags = \
	VDM_STATE_FLAG_BACK_READY_IF_DPM_ACK |\
	VDM_STATE_FLAG_DPM_ACK_IMMEDIATELY; }

#define VDM_STATE_REPLY_SVDM_REQUEST(pd_port)	{\
	pd_port->pe_data.vdm_state_flags = \
		VDM_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC; }

#define VDM_STATE_NORESP_CMD(pd_port)	{\
	pd_port->pe_data.vdm_state_flags = \
		VDM_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC; }

static inline bool pd_check_pe_during_hard_reset(struct pd_port *pd_port)
{
	return pd_port->tcpc_dev->pd_wait_hard_reset_complete;
}

enum pd_pe_state {
	PE_STATE_START_ID = -1,
/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
	PE_SRC_STARTUP,
	PE_SRC_DISCOVERY,
	PE_SRC_SEND_CAPABILITIES,
	PE_SRC_NEGOTIATE_CAPABILITIES,
	PE_SRC_TRANSITION_SUPPLY,
	PE_SRC_TRANSITION_SUPPLY2,
	PE_SRC_READY,
	PE_SRC_DISABLED,
	PE_SRC_CAPABILITY_RESPONSE,
	PE_SRC_HARD_RESET,
	PE_SRC_HARD_RESET_RECEIVED,
	PE_SRC_TRANSITION_TO_DEFAULT,
	PE_SRC_GET_SINK_CAP,
	PE_SRC_WAIT_NEW_CAPABILITIES,
	PE_SRC_SEND_SOFT_RESET,
	PE_SRC_SOFT_RESET,

/* Source Startup Discover Cable */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#ifdef CONFIG_PD_SRC_RESET_CABLE
	PE_SRC_CBL_SEND_SOFT_RESET,
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
	PE_SRC_VDM_IDENTITY_REQUEST,
	PE_SRC_VDM_IDENTITY_ACKED,
	PE_SRC_VDM_IDENTITY_NAKED,
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */

/* Source for PD30 */
#ifdef CONFIG_USB_PD_REV30
	PE_SRC_SEND_NOT_SUPPORTED,
	PE_SRC_NOT_SUPPORTED_RECEIVED,
	PE_SRC_CHUNK_RECEIVED,
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	PE_SRC_SEND_SOURCE_ALERT,
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	PE_SRC_SINK_ALERT_RECEIVED,
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	PE_SRC_GIVE_SOURCE_CAP_EXT,
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	PE_SRC_GIVE_SOURCE_STATUS,
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	PE_SRC_GET_SINK_STATUS,
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	PE_SRC_GIVE_PPS_STATUS,
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */

/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
/* Sink Init */
	PE_SNK_STARTUP,
	PE_SNK_DISCOVERY,
	PE_SNK_WAIT_FOR_CAPABILITIES,
	PE_SNK_EVALUATE_CAPABILITY,
	PE_SNK_SELECT_CAPABILITY,
	PE_SNK_TRANSITION_SINK,
	PE_SNK_READY,
	PE_SNK_HARD_RESET,
	PE_SNK_TRANSITION_TO_DEFAULT,
	PE_SNK_GIVE_SINK_CAP,
	PE_SNK_GET_SOURCE_CAP,

	PE_SNK_SEND_SOFT_RESET,
	PE_SNK_SOFT_RESET,

/* Sink for PD30 */
#ifdef CONFIG_USB_PD_REV30
	PE_SNK_SEND_NOT_SUPPORTED,
	PE_SNK_NOT_SUPPORTED_RECEIVED,
	PE_SNK_CHUNK_RECEIVED,
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	PE_SNK_SOURCE_ALERT_RECEIVED,
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	PE_SNK_SEND_SINK_ALERT,
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	PE_SNK_GET_SOURCE_CAP_EXT,
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	PE_SNK_GET_SOURCE_STATUS,
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	PE_SNK_GIVE_SINK_STATUS,
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	PE_SNK_GET_PPS_STATUS,
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */

/******************* DR_SWAP *******************/
#ifdef CONFIG_USB_PD_DR_SWAP
/* DR_SWAP_DFP */
	PE_DRS_DFP_UFP_EVALUATE_DR_SWAP,
	PE_DRS_DFP_UFP_ACCEPT_DR_SWAP,
	PE_DRS_DFP_UFP_CHANGE_TO_UFP,
	PE_DRS_DFP_UFP_SEND_DR_SWAP,
	PE_DRS_DFP_UFP_REJECT_DR_SWAP,
/* DR_SWAP_UFP */
	PE_DRS_UFP_DFP_EVALUATE_DR_SWAP,
	PE_DRS_UFP_DFP_ACCEPT_DR_SWAP,
	PE_DRS_UFP_DFP_CHANGE_TO_DFP,
	PE_DRS_UFP_DFP_SEND_DR_SWAP,
	PE_DRS_UFP_DFP_REJECT_DR_SWAP,
#endif	/* CONFIG_USB_PD_DR_SWAP */

/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
/* PR_SWAP_SRC */
	PE_PRS_SRC_SNK_EVALUATE_PR_SWAP,
	PE_PRS_SRC_SNK_ACCEPT_PR_SWAP,
	PE_PRS_SRC_SNK_TRANSITION_TO_OFF,
	PE_PRS_SRC_SNK_ASSERT_RD,
	PE_PRS_SRC_SNK_WAIT_SOURCE_ON,
	PE_PRS_SRC_SNK_SEND_SWAP,
	PE_PRS_SRC_SNK_REJECT_PR_SWAP,

/* PR_SWAP_SNK */
	PE_PRS_SNK_SRC_EVALUATE_PR_SWAP,
	PE_PRS_SNK_SRC_ACCEPT_PR_SWAP,
	PE_PRS_SNK_SRC_TRANSITION_TO_OFF,
	PE_PRS_SNK_SRC_ASSERT_RP,
	PE_PRS_SNK_SRC_SOURCE_ON,
	PE_PRS_SNK_SRC_SEND_SWAP,
	PE_PRS_SNK_SRC_REJECT_SWAP,

/* get same role cap */
	PE_DR_SRC_GET_SOURCE_CAP,
	PE_DR_SRC_GIVE_SINK_CAP,
	PE_DR_SNK_GET_SINK_CAP,
	PE_DR_SNK_GIVE_SOURCE_CAP,

/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	PE_DR_SNK_GIVE_SOURCE_CAP_EXT,
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	PE_DR_SRC_GET_SOURCE_CAP_EXT,
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */

/******************* VCONN_SWAP *******************/
#ifdef CONFIG_USB_PD_VCONN_SWAP
	PE_VCS_SEND_SWAP,
	PE_VCS_EVALUATE_SWAP,
	PE_VCS_ACCEPT_SWAP,
	PE_VCS_REJECT_VCONN_SWAP,
	PE_VCS_WAIT_FOR_VCONN,
	PE_VCS_TURN_OFF_VCONN,
	PE_VCS_TURN_ON_VCONN,
	PE_VCS_SEND_PS_RDY,
#endif	/* CONFIG_USB_PD_VCONN_SWAP */

/******************* UFP_VDM *******************/
	PE_UFP_VDM_GET_IDENTITY,
	PE_UFP_VDM_GET_SVIDS,
	PE_UFP_VDM_GET_MODES,
	PE_UFP_VDM_EVALUATE_MODE_ENTRY,
	PE_UFP_VDM_MODE_EXIT,
	PE_UFP_VDM_ATTENTION_REQUEST,

#ifdef CONFIG_USB_PD_ALT_MODE
	PE_UFP_VDM_DP_STATUS_UPDATE,
	PE_UFP_VDM_DP_CONFIGURE,
#endif/* CONFIG_USB_PD_ALT_MODE */

/******************* DFP_VDM *******************/
	PE_DFP_UFP_VDM_IDENTITY_REQUEST,
	PE_DFP_UFP_VDM_IDENTITY_ACKED,
	PE_DFP_UFP_VDM_IDENTITY_NAKED,
	PE_DFP_CBL_VDM_IDENTITY_REQUEST,
	PE_DFP_CBL_VDM_IDENTITY_ACKED,
	PE_DFP_CBL_VDM_IDENTITY_NAKED,

	PE_DFP_VDM_SVIDS_REQUEST,
	PE_DFP_VDM_SVIDS_ACKED,
	PE_DFP_VDM_SVIDS_NAKED,
	PE_DFP_VDM_MODES_REQUEST,
	PE_DFP_VDM_MODES_ACKED,
	PE_DFP_VDM_MODES_NAKED,
	PE_DFP_VDM_MODE_ENTRY_REQUEST,
	PE_DFP_VDM_MODE_ENTRY_ACKED,
	PE_DFP_VDM_MODE_ENTRY_NAKED,
	PE_DFP_VDM_MODE_EXIT_REQUEST,
	PE_DFP_VDM_MODE_EXIT_ACKED,
	PE_DFP_VDM_ATTENTION_REQUEST,

#ifdef CONFIG_PD_DFP_RESET_CABLE
	PE_DFP_CBL_SEND_SOFT_RESET,
	PE_DFP_CBL_SEND_CABLE_RESET,
#endif	/* CONFIG_PD_DFP_RESET_CABLE */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST,
	PE_DFP_VDM_DP_STATUS_UPDATE_ACKED,
	PE_DFP_VDM_DP_STATUS_UPDATE_NAKED,

	PE_DFP_VDM_DP_CONFIGURATION_REQUEST,
	PE_DFP_VDM_DP_CONFIGURATION_ACKED,
	PE_DFP_VDM_DP_CONFIGURATION_NAKED,
#endif/* CONFIG_USB_PD_ALT_MODE_DFP */

/******************* UVDM & SVDM *******************/

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	PE_UFP_UVDM_RECV,
	PE_DFP_UVDM_SEND,
	PE_DFP_UVDM_ACKED,
	PE_DFP_UVDM_NAKED,
#endif/* CONFIG_USB_PD_CUSTOM_VDM */

/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	PE_GET_BATTERY_CAP,
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
	PE_GIVE_BATTERY_CAP,
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	PE_GET_BATTERY_STATUS,
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
	PE_GIVE_BATTERY_STATUS,
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	PE_GET_MANUFACTURER_INFO,
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
	PE_GIVE_MANUFACTURER_INFO,
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	PE_GET_COUNTRY_CODES,
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	PE_GIVE_COUNTRY_CODES,
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	PE_GET_COUNTRY_INFO,
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
	PE_GIVE_COUNTRY_INFO,
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */
	PE_VDM_NOT_SUPPORTED,
#endif /* CONFIG_USB_PD_REV30 */

/******************* Others *******************/
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	PE_DBG_READY,
#endif/* CONFIG_USB_PD_CUSTOM_DBGACC */

#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
	PE_OVER_RECV_HRESET_LIMIT,
#endif/* CONFIG_USB_PD_RECV_HRESET_COUNTER */

	PE_REJECT,
	PE_ERROR_RECOVERY,
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
	PE_ERROR_RECOVERY_ONCE,
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */
	PE_BIST_TEST_DATA,
	PE_BIST_CARRIER_MODE_2,
/* Wait tx finished */
	PE_IDLE1,
	PE_IDLE2,
	PD_NR_PE_STATES,
};

/**
 * pd_policy_engine_run
 *
 * Driving the PE to get next event, and take the corresponding actions.
 *
 *
 * Returns True if a pending event is processed;
 * Returns Zero to indicate there is no pending event.
 * Returns Negative Value if an error occurs.
 */

int pd_policy_engine_run(struct tcpc_device *tcpc_dev);


/* ---- Policy Engine (General) ---- */

void pe_power_ready_entry(struct pd_port *pd_port);

static inline void pe_send_swap_request_entry(
		struct pd_port *pd_port, uint8_t msg)
{
	PE_STATE_WAIT_ANSWER_MSG(pd_port);
	pd_send_sop_ctrl_msg(pd_port, msg);
}

/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
void pe_src_startup_entry(
	struct pd_port *pd_port);
void pe_src_discovery_entry(
	struct pd_port *pd_port);
void pe_src_send_capabilities_entry(
	struct pd_port *pd_port);
void pe_src_negotiate_capabilities_entry(
	struct pd_port *pd_port);
void pe_src_transition_supply_entry(
	struct pd_port *pd_port);
void pe_src_transition_supply_exit(
	struct pd_port *pd_port);
void pe_src_transition_supply2_entry(
	struct pd_port *pd_port);
void pe_src_ready_entry(
	struct pd_port *pd_port);
void pe_src_disabled_entry(
	struct pd_port *pd_port);
void pe_src_capability_response_entry(
	struct pd_port *pd_port);
void pe_src_hard_reset_entry(
	struct pd_port *pd_port);
void pe_src_hard_reset_received_entry(
	struct pd_port *pd_port);
void pe_src_transition_to_default_entry(
	struct pd_port *pd_port);
void pe_src_transition_to_default_exit(
	struct pd_port *pd_port);
void pe_src_get_sink_cap_entry(
	struct pd_port *pd_port);
void pe_src_get_sink_cap_exit(
	struct pd_port *pd_port);
void pe_src_wait_new_capabilities_entry(
	struct pd_port *pd_port);
void pe_src_send_soft_reset_entry(
	struct pd_port *pd_port);
void pe_src_soft_reset_entry(
	struct pd_port *pd_port);

/* Source Startup Discover Cable */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#ifdef CONFIG_PD_SRC_RESET_CABLE
void pe_src_cbl_send_soft_reset_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
void pe_src_vdm_identity_request_entry(
	struct pd_port *pd_port);
void pe_src_vdm_identity_acked_entry(
	struct pd_port *pd_port);
void pe_src_vdm_identity_naked_entry(
	struct pd_port *pd_port);
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */

/* Source for PD30 */
#ifdef CONFIG_USB_PD_REV30
void pe_src_send_not_supported_entry(
	struct pd_port *pd_port);
void pe_src_not_supported_received_entry(
	struct pd_port *pd_port);
void pe_src_chunk_received_entry(
	struct pd_port *pd_port);
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
void pe_src_send_source_alert_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
void pe_src_sink_alert_received_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
void pe_src_give_source_cap_ext_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
void pe_src_give_source_status_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
void pe_src_get_sink_status_entry(
	struct pd_port *pd_port);
void pe_src_get_sink_status_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
void pe_src_give_pps_status_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */

/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
/* Sink Init */
void pe_snk_startup_entry(
	struct pd_port *pd_port);
void pe_snk_discovery_entry(
	struct pd_port *pd_port);
void pe_snk_wait_for_capabilities_entry(
	struct pd_port *pd_port);
void pe_snk_evaluate_capability_entry(
	struct pd_port *pd_port);
void pe_snk_select_capability_entry(
	struct pd_port *pd_port);
void pe_snk_select_capability_exit(
	struct pd_port *pd_port);
void pe_snk_transition_sink_entry(
	struct pd_port *pd_port);
void pe_snk_transition_sink_exit(
	struct pd_port *pd_port);
void pe_snk_ready_entry(
	struct pd_port *pd_port);
void pe_snk_hard_reset_entry(
	struct pd_port *pd_port);
void pe_snk_transition_to_default_entry(
	struct pd_port *pd_port);
void pe_snk_give_sink_cap_entry(
	struct pd_port *pd_port);
void pe_snk_get_source_cap_entry(
	struct pd_port *pd_port);

void pe_snk_send_soft_reset_entry(
	struct pd_port *pd_port);
void pe_snk_soft_reset_entry(
	struct pd_port *pd_port);

/* Sink for PD30 */
#ifdef CONFIG_USB_PD_REV30
void pe_snk_send_not_supported_entry(
	struct pd_port *pd_port);
void pe_snk_not_supported_received_entry(
	struct pd_port *pd_port);
void pe_snk_chunk_received_entry(
	struct pd_port *pd_port);
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
void pe_snk_source_alert_received_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
void pe_snk_send_sink_alert_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
void pe_snk_get_source_cap_ext_entry(
	struct pd_port *pd_port);
void pe_snk_get_source_cap_ext_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
void pe_snk_get_source_status_entry(
	struct pd_port *pd_port);
void pe_snk_get_source_status_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
void pe_snk_give_sink_status_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
void pe_snk_get_pps_status_entry(
	struct pd_port *pd_port);
void pe_snk_get_pps_status_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */

/******************* DR_SWAP *******************/
#ifdef CONFIG_USB_PD_DR_SWAP
/* DR_SWAP_DFP */
void pe_drs_dfp_ufp_evaluate_dr_swap_entry(
	struct pd_port *pd_port);
void pe_drs_dfp_ufp_accept_dr_swap_entry(
	struct pd_port *pd_port);
void pe_drs_dfp_ufp_change_to_ufp_entry(
	struct pd_port *pd_port);
void pe_drs_dfp_ufp_send_dr_swap_entry(
	struct pd_port *pd_port);
void pe_drs_dfp_ufp_reject_dr_swap_entry(
	struct pd_port *pd_port);
/* DR_SWAP_UFP */
void pe_drs_ufp_dfp_evaluate_dr_swap_entry(
	struct pd_port *pd_port);
void pe_drs_ufp_dfp_accept_dr_swap_entry(
	struct pd_port *pd_port);
void pe_drs_ufp_dfp_change_to_dfp_entry(
	struct pd_port *pd_port);
void pe_drs_ufp_dfp_send_dr_swap_entry(
	struct pd_port *pd_port);
void pe_drs_ufp_dfp_reject_dr_swap_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_DR_SWAP */

/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
/* PR_SWAP_SRC */
void pe_prs_src_snk_evaluate_pr_swap_entry(
	struct pd_port *pd_port);
void pe_prs_src_snk_accept_pr_swap_entry(
	struct pd_port *pd_port);
void pe_prs_src_snk_transition_to_off_entry(
	struct pd_port *pd_port);
void pe_prs_src_snk_assert_rd_entry(
	struct pd_port *pd_port);
void pe_prs_src_snk_wait_source_on_entry(
	struct pd_port *pd_port);
void pe_prs_src_snk_wait_source_on_exit(
	struct pd_port *pd_port);
void pe_prs_src_snk_send_swap_entry(
	struct pd_port *pd_port);
void pe_prs_src_snk_reject_pr_swap_entry(
	struct pd_port *pd_port);

/* PR_SWAP_SNK */
void pe_prs_snk_src_evaluate_pr_swap_entry(
	struct pd_port *pd_port);
void pe_prs_snk_src_accept_pr_swap_entry(
	struct pd_port *pd_port);
void pe_prs_snk_src_transition_to_off_entry(
	struct pd_port *pd_port);
void pe_prs_snk_src_transition_to_off_exit(
	struct pd_port *pd_port);
void pe_prs_snk_src_assert_rp_entry(
	struct pd_port *pd_port);
void pe_prs_snk_src_source_on_entry(
	struct pd_port *pd_port);
void pe_prs_snk_src_source_on_exit(
	struct pd_port *pd_port);
void pe_prs_snk_src_send_swap_entry(
	struct pd_port *pd_port);
void pe_prs_snk_src_reject_swap_entry(
	struct pd_port *pd_port);

/* get same role cap */
void pe_dr_src_get_source_cap_entry(
	struct pd_port *pd_port);
void pe_dr_src_get_source_cap_exit(
	struct pd_port *pd_port);
void pe_dr_src_give_sink_cap_entry(
	struct pd_port *pd_port);
void pe_dr_snk_get_sink_cap_entry(
	struct pd_port *pd_port);
void pe_dr_snk_get_sink_cap_exit(
	struct pd_port *pd_port);
void pe_dr_snk_give_source_cap_entry(
	struct pd_port *pd_port);

/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
void pe_dr_snk_give_source_cap_ext_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
void pe_dr_src_get_source_cap_ext_entry(
	struct pd_port *pd_port);
void pe_dr_src_get_source_cap_ext_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */

/******************* VCONN_SWAP *******************/
#ifdef CONFIG_USB_PD_VCONN_SWAP
void pe_vcs_send_swap_entry(
	struct pd_port *pd_port);
void pe_vcs_evaluate_swap_entry(
	struct pd_port *pd_port);
void pe_vcs_accept_swap_entry(
	struct pd_port *pd_port);
void pe_vcs_reject_vconn_swap_entry(
	struct pd_port *pd_port);
void pe_vcs_wait_for_vconn_entry(
	struct pd_port *pd_port);
void pe_vcs_wait_for_vconn_exit(
	struct pd_port *pd_port);
void pe_vcs_turn_off_vconn_entry(
	struct pd_port *pd_port);
void pe_vcs_turn_on_vconn_entry(
	struct pd_port *pd_port);
void pe_vcs_send_ps_rdy_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_VCONN_SWAP */

/******************* UFP_VDM *******************/
void pe_ufp_vdm_get_identity_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_send_identity_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_get_identity_nak_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_get_svids_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_send_svids_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_get_svids_nak_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_get_modes_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_send_modes_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_get_modes_nak_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_evaluate_mode_entry_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_mode_entry_ack_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_mode_entry_nak_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_mode_exit_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_mode_exit_ack_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_mode_exit_nak_entry(
	struct pd_port *pd_port);

void pe_ufp_vdm_attention_request_entry(
	struct pd_port *pd_port);

#ifdef CONFIG_USB_PD_ALT_MODE
void pe_ufp_vdm_dp_status_update_entry(
	struct pd_port *pd_port);
void pe_ufp_vdm_dp_configure_entry(
	struct pd_port *pd_port);
#endif/* CONFIG_USB_PD_ALT_MODE */
/******************* DFP_VDM *******************/
void pe_dfp_ufp_vdm_identity_request_entry(
	struct pd_port *pd_port);
void pe_dfp_ufp_vdm_identity_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_ufp_vdm_identity_naked_entry(
	struct pd_port *pd_port);
void pe_dfp_cbl_vdm_identity_request_entry(
	struct pd_port *pd_port);
void pe_dfp_cbl_vdm_identity_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_cbl_vdm_identity_naked_entry(
	struct pd_port *pd_port);

void pe_dfp_vdm_svids_request_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_svids_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_svids_naked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_modes_request_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_modes_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_modes_naked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_mode_entry_request_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_mode_entry_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_mode_entry_naked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_mode_exit_request_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_mode_exit_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_attention_request_entry(
	struct pd_port *pd_port);

#ifdef CONFIG_PD_DFP_RESET_CABLE
void pe_dfp_cbl_send_soft_reset_entry(
	struct pd_port *pd_port);
void pe_dfp_cbl_send_cable_reset_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
void pe_dfp_vdm_dp_status_update_request_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_dp_status_update_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_dp_status_update_naked_entry(
	struct pd_port *pd_port);

void pe_dfp_vdm_dp_configuration_request_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_dp_configuration_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_vdm_dp_configuration_naked_entry(
	struct pd_port *pd_port);
#endif/* CONFIG_USB_PD_ALT_MODE_DFP */
/******************* UVDM & SVDM *******************/
#ifdef CONFIG_USB_PD_CUSTOM_VDM
void pe_ufp_uvdm_recv_entry(
	struct pd_port *pd_port);
void pe_dfp_uvdm_send_entry(
	struct pd_port *pd_port);
void pe_dfp_uvdm_acked_entry(
	struct pd_port *pd_port);
void pe_dfp_uvdm_naked_entry(
	struct pd_port *pd_port);
#endif/* CONFIG_USB_PD_CUSTOM_VDM */

/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
void pe_get_battery_cap_entry(
	struct pd_port *pd_port);
void pe_get_battery_cap_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
void pe_give_battery_cap_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
void pe_get_battery_status_entry(
	struct pd_port *pd_port);
void pe_get_battery_status_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
void pe_give_battery_status_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
void pe_get_manufacturer_info_entry(
	struct pd_port *pd_port);
void pe_get_manufacturer_info_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
void pe_give_manufacturer_info_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
void pe_get_country_codes_entry(
	struct pd_port *pd_port);
void pe_get_country_codes_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
void pe_give_country_codes_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
void pe_get_country_info_entry(
	struct pd_port *pd_port);
void pe_get_country_info_exit(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
void pe_give_country_info_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */
void pe_vdm_not_supported_entry(
	struct pd_port *pd_port);
#endif /* CONFIG_USB_PD_REV30 */
/******************* Others *******************/
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
void pe_dbg_ready_entry(
	struct pd_port *pd_port);
#endif/* CONFIG_USB_PD_CUSTOM_DBGACC */

#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
void pe_over_recv_hreset_limit_entry(
	struct pd_port *pd_port);
#endif/* CONFIG_USB_PD_RECV_HRESET_COUNTER */
void pe_reject_entry(
	struct pd_port *pd_port);
void pe_error_recovery_entry(
	struct pd_port *pd_port);
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
void pe_error_recovery_once_entry(
	struct pd_port *pd_port);
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */
void pe_bist_test_data_entry(
	struct pd_port *pd_port);
void pe_bist_test_data_exit(
	struct pd_port *pd_port);
void pe_bist_carrier_mode_2_entry(
	struct pd_port *pd_port);
void pe_bist_carrier_mode_2_exit(
	struct pd_port *pd_port);
/* Wait tx finished */
void pe_idle1_entry(
	struct pd_port *pd_port);
void pe_idle2_entry(
	struct pd_port *pd_port);

#endif /* PD_POLICY_ENGINE_H_ */
