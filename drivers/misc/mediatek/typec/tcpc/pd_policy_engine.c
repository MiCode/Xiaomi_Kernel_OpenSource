/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Power Delivery Policy Engine Driver
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

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_process_evt.h"
#include "inc/pd_policy_engine.h"

/* ---- Policy Engine State ---- */

#if PE_DBG_ENABLE | PE_STATE_INFO_ENABLE
#if PE_STATE_FULL_NAME

static const char *const pe_state_name[] = {

/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
	"PE_SRC_STARTUP",
	"PE_SRC_DISCOVERY",
	"PE_SRC_SEND_CAPABILITIES",
	"PE_SRC_NEGOTIATE_CAPABILITIES",
	"PE_SRC_TRANSITION_SUPPLY",
	"PE_SRC_TRANSITION_SUPPLY2",
	"PE_SRC_READY",
	"PE_SRC_DISABLED",
	"PE_SRC_CAPABILITY_RESPONSE",
	"PE_SRC_HARD_RESET",
	"PE_SRC_HARD_RESET_RECEIVED",
	"PE_SRC_TRANSITION_TO_DEFAULT",
	"PE_SRC_GET_SINK_CAP",
	"PE_SRC_WAIT_NEW_CAPABILITIES",
	"PE_SRC_SEND_SOFT_RESET",
	"PE_SRC_SOFT_RESET",
/* Source Startup Discover Cable */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#ifdef CONFIG_PD_SRC_RESET_CABLE
	"PE_SRC_CBL_SEND_SOFT_RESET",
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
	"PE_SRC_VDM_IDENTITY_REQUEST",
	"PE_SRC_VDM_IDENTITY_ACKED",
	"PE_SRC_VDM_IDENTITY_NAKED",
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */
/* Source for PD30 */
#ifdef CONFIG_USB_PD_REV30
	"PE_SRC_SEND_NOT_SUPPORTED",
	"PE_SRC_NOT_SUPPORTED_RECEIVED",
	"PE_SRC_CHUNK_RECEIVED",
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	"PE_SRC_SEND_SOURCE_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	"PE_SRC_SINK_ALERT_RECEIVED",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	"PE_SRC_GIVE_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	"PE_SRC_GIVE_SOURCE_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	"PE_SRC_GET_SINK_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	"PE_SRC_GIVE_PPS_STATUS",
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */
/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
	"PE_SNK_STARTUP",
	"PE_SNK_DISCOVERY",
	"PE_SNK_WAIT_FOR_CAPABILITIES",
	"PE_SNK_EVALUATE_CAPABILITY",
	"PE_SNK_SELECT_CAPABILITY",
	"PE_SNK_TRANSITION_SINK",
	"PE_SNK_READY",
	"PE_SNK_HARD_RESET",
	"PE_SNK_TRANSITION_TO_DEFAULT",
	"PE_SNK_GIVE_SINK_CAP",
	"PE_SNK_GET_SOURCE_CAP",
	"PE_SNK_SEND_SOFT_RESET",
	"PE_SNK_SOFT_RESET",
/* Sink for PD30 */
#ifdef CONFIG_USB_PD_REV30
	"PE_SNK_SEND_NOT_SUPPORTED",
	"PE_SNK_NOT_SUPPORTED_RECEIVED",
	"PE_SNK_CHUNK_RECEIVED",
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	"PE_SNK_SOURCE_ALERT_RECEIVED",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	"PE_SNK_SEND_SINK_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	"PE_SNK_GET_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	"PE_SNK_GET_SOURCE_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	"PE_SNK_GIVE_SINK_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	"PE_SNK_GET_PPS_STATUS",
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */
/******************* DR_SWAP *******************/
#ifdef CONFIG_USB_PD_DR_SWAP
/* DR_SWAP_DFP */
	"PE_DRS_DFP_UFP_EVALUATE_DR_SWAP",
	"PE_DRS_DFP_UFP_ACCEPT_DR_SWAP",
	"PE_DRS_DFP_UFP_CHANGE_TO_UFP",
	"PE_DRS_DFP_UFP_SEND_DR_SWAP",
	"PE_DRS_DFP_UFP_REJECT_DR_SWAP",
/* DR_SWAP_UFP */
	"PE_DRS_UFP_DFP_EVALUATE_DR_SWAP",
	"PE_DRS_UFP_DFP_ACCEPT_DR_SWAP",
	"PE_DRS_UFP_DFP_CHANGE_TO_DFP",
	"PE_DRS_UFP_DFP_SEND_DR_SWAP",
	"PE_DRS_UFP_DFP_REJECT_DR_SWAP",
#endif	/* CONFIG_USB_PD_DR_SWAP */
/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
/* PR_SWAP_SRC */
	"PE_PRS_SRC_SNK_EVALUATE_PR_SWAP",
	"PE_PRS_SRC_SNK_ACCEPT_PR_SWAP",
	"PE_PRS_SRC_SNK_TRANSITION_TO_OFF",
	"PE_PRS_SRC_SNK_ASSERT_RD",
	"PE_PRS_SRC_SNK_WAIT_SOURCE_ON",
	"PE_PRS_SRC_SNK_SEND_SWAP",
	"PE_PRS_SRC_SNK_REJECT_PR_SWAP",
/* PR_SWAP_SNK */
	"PE_PRS_SNK_SRC_EVALUATE_PR_SWAP",
	"PE_PRS_SNK_SRC_ACCEPT_PR_SWAP",
	"PE_PRS_SNK_SRC_TRANSITION_TO_OFF",
	"PE_PRS_SNK_SRC_ASSERT_RP",
	"PE_PRS_SNK_SRC_SOURCE_ON",
	"PE_PRS_SNK_SRC_SEND_SWAP",
	"PE_PRS_SNK_SRC_REJECT_SWAP",
/* get same role cap */
	"PE_DR_SRC_GET_SOURCE_CAP",
	"PE_DR_SRC_GIVE_SINK_CAP",
	"PE_DR_SNK_GET_SINK_CAP",
	"PE_DR_SNK_GIVE_SOURCE_CAP",
/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	"PE_DR_SNK_GIVE_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	"PE_DR_SRC_GET_SOURCE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */
/******************* VCONN_SWAP *******************/
#ifdef CONFIG_USB_PD_VCONN_SWAP
	"PE_VCS_SEND_SWAP",
	"PE_VCS_EVALUATE_SWAP",
	"PE_VCS_ACCEPT_SWAP",
	"PE_VCS_REJECT_VCONN_SWAP",
	"PE_VCS_WAIT_FOR_VCONN",
	"PE_VCS_TURN_OFF_VCONN",
	"PE_VCS_TURN_ON_VCONN",
	"PE_VCS_SEND_PS_RDY",
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
/******************* UFP_VDM *******************/
	"PE_UFP_VDM_GET_IDENTITY",
	"PE_UFP_VDM_GET_SVIDS",
	"PE_UFP_VDM_GET_MODES",
	"PE_UFP_VDM_EVALUATE_MODE_ENTRY",
	"PE_UFP_VDM_MODE_EXIT",
	"PE_UFP_VDM_ATTENTION_REQUEST",
#ifdef CONFIG_USB_PD_ALT_MODE
	"PE_UFP_VDM_DP_STATUS_UPDATE",
	"PE_UFP_VDM_DP_CONFIGURE",
#endif/* CONFIG_USB_PD_ALT_MODE */
/******************* DFP_VDM *******************/
	"PE_DFP_UFP_VDM_IDENTITY_REQUEST",
	"PE_DFP_UFP_VDM_IDENTITY_ACKED",
	"PE_DFP_UFP_VDM_IDENTITY_NAKED",
	"PE_DFP_CBL_VDM_IDENTITY_REQUEST",
	"PE_DFP_CBL_VDM_IDENTITY_ACKED",
	"PE_DFP_CBL_VDM_IDENTITY_NAKED",
	"PE_DFP_VDM_SVIDS_REQUEST",
	"PE_DFP_VDM_SVIDS_ACKED",
	"PE_DFP_VDM_SVIDS_NAKED",
	"PE_DFP_VDM_MODES_REQUEST",
	"PE_DFP_VDM_MODES_ACKED",
	"PE_DFP_VDM_MODES_NAKED",
	"PE_DFP_VDM_MODE_ENTRY_REQUEST",
	"PE_DFP_VDM_MODE_ENTRY_ACKED",
	"PE_DFP_VDM_MODE_ENTRY_NAKED",
	"PE_DFP_VDM_MODE_EXIT_REQUEST",
	"PE_DFP_VDM_MODE_EXIT_ACKED",
	"PE_DFP_VDM_ATTENTION_REQUEST",
#ifdef CONFIG_PD_DFP_RESET_CABLE
	"PE_DFP_CBL_SEND_SOFT_RESET",
	"PE_DFP_CBL_SEND_CABLE_RESET",
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	"PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST",
	"PE_DFP_VDM_DP_STATUS_UPDATE_ACKED",
	"PE_DFP_VDM_DP_STATUS_UPDATE_NAKED",
	"PE_DFP_VDM_DP_CONFIGURATION_REQUEST",
	"PE_DFP_VDM_DP_CONFIGURATION_ACKED",
	"PE_DFP_VDM_DP_CONFIGURATION_NAKED",
#endif/* CONFIG_USB_PD_ALT_MODE_DFP */
/******************* UVDM & SVDM *******************/
#ifdef CONFIG_USB_PD_CUSTOM_VDM
	"PE_UFP_UVDM_RECV",
	"PE_DFP_UVDM_SEND",
	"PE_DFP_UVDM_ACKED",
	"PE_DFP_UVDM_NAKED",
#endif/* CONFIG_USB_PD_CUSTOM_VDM */
	"PE_UFP_VDM_SEND_NAK",
/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	"PE_GET_BATTERY_CAP",
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
	"PE_GIVE_BATTERY_CAP",
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	"PE_GET_BATTERY_STATUS",
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
	"PE_GIVE_BATTERY_STATUS",
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	"PE_GET_MANUFACTURER_INFO",
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
	"PE_GIVE_MANUFACTURER_INFO",
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	"PE_GET_COUNTRY_CODES",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	"PE_GIVE_COUNTRY_CODES",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	"PE_GET_COUNTRY_INFO",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
	"PE_GIVE_COUNTRY_INFO",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */

	"PE_VDM_NOT_SUPPORTED",
#endif /* CONFIG_USB_PD_REV30 */
/******************* Others *******************/
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	"PE_DBG_READY",
#endif/* CONFIG_USB_PD_CUSTOM_DBGACC */
#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
	"PE_OVER_RECV_HRESET_LIMIT",
#endif/* CONFIG_USB_PD_RECV_HRESET_COUNTER */
	"PE_REJECT",
	"PE_ERROR_RECOVERY",
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
	"PE_ERROR_RECOVERY_ONCE",
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */
	"PE_BIST_TEST_DATA",
	"PE_BIST_CARRIER_MODE_2",

#ifdef CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG
	"PE_UNEXPECTED_TX_WAIT",
	"PE_SEND_SOFT_RESET_TX_WAIT",
	"PE_RECV_SOFT_RESET_TX_WAIT",
	"PE_SEND_SOFT_RESET_STANDBY",
#endif	/* CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG */

/* Wait tx finished */
	"PE_IDLE1",
	"PE_IDLE2",
};

#else

static const char *const pe_state_name[] = {
/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
	"SRC_START",
	"SRC_DISC",
	"SRC_SEND_CAP",
	"SRC_NEG_CAP",
	"SRC_TRANS_SUPPLY",
	"SRC_TRANS_SUPPLY2",
	"SRC_READY",
	"SRC_DISABLED",
	"SRC_CAP_RESP",
	"SRC_HRESET",
	"SRC_HRESET_RECV",
	"SRC_TRANS_DFT",
	"SRC_GET_CAP",
	"SRC_WAIT_CAP",
	"SRC_SEND_SRESET",
	"SRC_SRESET",
/* Source Startup Discover Cable */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#ifdef CONFIG_PD_SRC_RESET_CABLE
	"SRC_CBL_SEND_SRESET",
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
	"SRC_VDM_ID_REQ",
	"SRC_VDM_ID_ACK",
	"SRC_VDM_ID_NAK",
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */
/* Source for PD30 */
#ifdef CONFIG_USB_PD_REV30
	"SRC_NO_SUPP",
	"SRC_NO_SUPP_RECV",
	"SRC_CK_RECV",
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	"SRC_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	"SRC_RECV_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	"SRC_GIVE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	"SRC_GIVE_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	"SRC_GET_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	"SRC_GIVE_PPS",
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */
/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
/* Sink Init */
	"SNK_START",
	"SNK_DISC",
	"SNK_WAIT_CAP",
	"SNK_EVA_CAP",
	"SNK_SEL_CAP",
	"SNK_TRANS_SINK",
	"SNK_READY",
	"SNK_HRESET",
	"SNK_TRANS_DFT",
	"SNK_GIVE_CAP",
	"SNK_GET_CAP",
	"SNK_SEND_SRESET",
	"SNK_SRESET",
/* Sink for PD30 */
#ifdef CONFIG_USB_PD_REV30
	"SNK_NO_SUPP",
	"SNK_NO_SUPP_RECV",
	"SNK_CK_RECV",
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	"SNK_RECV_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	"SNK_ALERT",
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	"SNK_GET_CAP_EX",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	"SNK_GET_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	"SNK_GIVE_STATUS",
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	"SNK_GET_PPS",
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */
/******************* DR_SWAP *******************/
#ifdef CONFIG_USB_PD_DR_SWAP
/* DR_SWAP_DFP */
	"D_DFP_EVA",
	"D_DFP_ACCEPT",
	"D_DFP_CHANGE",
	"D_DFP_SEND",
	"D_DFP_REJECT",
/* DR_SWAP_UFP */
	"D_UFP_EVA",
	"D_UFP_ACCEPT",
	"D_UFP_CHANGE",
	"D_UFP_SEND",
	"D_UFP_REJECT",
#endif	/* CONFIG_USB_PD_DR_SWAP */
/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
/* PR_SWAP_SRC */
	"P_SRC_EVA",
	"P_SRC_ACCEPT",
	"P_SRC_TRANS_OFF",
	"P_SRC_ASSERT",
	"P_SRC_WAIT_ON",
	"P_SRC_SEND",
	"P_SRC_REJECT",

/* PR_SWAP_SNK */
	"P_SNK_EVA",
	"P_SNK_ACCEPT",
	"P_SNK_TRANS_OFF",
	"P_SNK_ASSERT",
	"P_SNK_SOURCE_ON",
	"P_SNK_SEND",
	"P_SNK_REJECT",
/* get same role cap */
	"DR_SRC_GET_CAP",
	"DR_SRC_GIVE_CAP",
	"DR_SNK_GET_CAP",
	"DR_SNK_GIVE_CAP",
/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	"DR_SNK_GIVE_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	"DR_SRC_GET_CAP_EXT",
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */
/******************* VCONN_SWAP *******************/
#ifdef CONFIG_USB_PD_VCONN_SWAP
	"V_SEND",
	"V_EVA",
	"V_ACCEPT",
	"V_REJECT",
	"V_WAIT_VCONN",
	"V_TURN_OFF",
	"V_TURN_ON",
	"V_PS_RDY",
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
/******************* UFP_VDM *******************/
	"U_GET_ID",
	"U_GET_SVID",
	"U_GET_MODE",
	"U_EVA_MODE",
	"U_MODE_EX",
	"U_ATTENTION",
#ifdef CONFIG_USB_PD_ALT_MODE
	"U_D_STATUS",
	"U_D_CONFIG",
#endif/* CONFIG_USB_PD_ALT_MODE */
/******************* DFP_VDM *******************/
	"D_UID_REQ",
	"D_UID_A",
	"D_UID_N",
	"D_CID_REQ",
	"D_CID_ACK",
	"D_CID_NAK",
	"D_SVID_REQ",
	"D_SVID_ACK",
	"D_SVID_NAK",
	"D_MODE_REQ",
	"D_MODE_ACK",
	"D_MODE_NAK",
	"D_MODE_EN_REQ",
	"D_MODE_EN_ACK",
	"D_MODE_EN_NAK",
	"D_MODE_EX_REQ",
	"D_MODE_EX_ACK",
	"D_ATTENTION",
#ifdef CONFIG_PD_DFP_RESET_CABLE
	"D_C_SRESET",
	"D_C_CRESET",
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	"D_DP_STATUS_REQ",
	"D_DP_STATUS_ACK",
	"D_DP_STATUS_NAK",
	"D_DP_CONFIG_REQ",
	"D_DP_CONFIG_ACK",
	"D_DP_CONFIG_NAK",
#endif/* CONFIG_USB_PD_ALT_MODE_DFP */
/******************* UVDM & SVDM *******************/
#ifdef CONFIG_USB_PD_CUSTOM_VDM
	"U_UVDM_RECV",
	"D_UVDM_SEND",
	"D_UVDM_ACKED",
	"D_UVDM_NAKED",
#endif/* CONFIG_USB_PD_CUSTOM_VDM */
	"U_SEND_NAK",
/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	"GET_BAT_CAP",
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
	"GIVE_BAT_CAP",
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	"GET_BAT_STATUS",
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
	"GIVE_BAT_STATUS",
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	"GET_MFRS_INFO",
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
	"GIVE_MFRS_INFO",
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	"GET_CC",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	"GIVE_CC",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	"GET_CI",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
	"GIVE_CI",
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */

	"VDM_NO_SUPP",
#endif /* CONFIG_USB_PD_REV30 */
/******************* Others *******************/
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	"DBG_READY",
#endif/* CONFIG_USB_PD_CUSTOM_DBGACC */
#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
	"OVER_HRESET_LIMIT",
#endif/* CONFIG_USB_PD_RECV_HRESET_COUNTER */
	"REJECT",
	"ERR_RECOVERY",
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
	"ERR_RECOVERY1",
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */
	"BIST_TD",
	"BIST_C2",

#ifdef CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG
	"UNEXPECTED_TX",
	"SEND_SRESET_TX",
	"RECV_SRESET_TX",
	"SEND_SRESET_STANDBY",
#endif	/* CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG */

/* Wait tx finished */
	"IDLE1",
	"IDLE2",
};
#endif	/* PE_STATE_FULL_NAME */
#endif /* PE_DBG_ENABLE | PE_STATE_INFO_ENABLE */

struct pe_state_actions {
	void (*entry_action)
		(struct pd_port *pd_port);
	/* const void (*exit_action)
	 * (struct pd_port *pd_port, struct pd_event *pd_event);
	 */
};

#define PE_STATE_ACTIONS(state) { .entry_action = state##_entry, }

static const struct pe_state_actions pe_state_actions[] = {
/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
	PE_STATE_ACTIONS(pe_src_startup),
	PE_STATE_ACTIONS(pe_src_discovery),
	PE_STATE_ACTIONS(pe_src_send_capabilities),
	PE_STATE_ACTIONS(pe_src_negotiate_capabilities),
	PE_STATE_ACTIONS(pe_src_transition_supply),
	PE_STATE_ACTIONS(pe_src_transition_supply2),
	PE_STATE_ACTIONS(pe_src_ready),
	PE_STATE_ACTIONS(pe_src_disabled),
	PE_STATE_ACTIONS(pe_src_capability_response),
	PE_STATE_ACTIONS(pe_src_hard_reset),
	PE_STATE_ACTIONS(pe_src_hard_reset_received),
	PE_STATE_ACTIONS(pe_src_transition_to_default),
	PE_STATE_ACTIONS(pe_src_get_sink_cap),
	PE_STATE_ACTIONS(pe_src_wait_new_capabilities),
	PE_STATE_ACTIONS(pe_src_send_soft_reset),
	PE_STATE_ACTIONS(pe_src_soft_reset),
/* Source Startup Discover Cable */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#ifdef CONFIG_PD_SRC_RESET_CABLE
	PE_STATE_ACTIONS(pe_src_cbl_send_soft_reset),
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
	PE_STATE_ACTIONS(pe_src_vdm_identity_request),
	PE_STATE_ACTIONS(pe_src_vdm_identity_acked),
	PE_STATE_ACTIONS(pe_src_vdm_identity_naked),
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */
/* Source for PD30 */
#ifdef CONFIG_USB_PD_REV30
	PE_STATE_ACTIONS(pe_src_send_not_supported),
	PE_STATE_ACTIONS(pe_src_not_supported_received),
	PE_STATE_ACTIONS(pe_src_chunk_received),
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	PE_STATE_ACTIONS(pe_src_send_source_alert),
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	PE_STATE_ACTIONS(pe_src_sink_alert_received),
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	PE_STATE_ACTIONS(pe_src_give_source_cap_ext),
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	PE_STATE_ACTIONS(pe_src_give_source_status),
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	PE_STATE_ACTIONS(pe_src_get_sink_status),
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	PE_STATE_ACTIONS(pe_src_give_pps_status),
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */
/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
/* Sink Init */
	PE_STATE_ACTIONS(pe_snk_startup),
	PE_STATE_ACTIONS(pe_snk_discovery),
	PE_STATE_ACTIONS(pe_snk_wait_for_capabilities),
	PE_STATE_ACTIONS(pe_snk_evaluate_capability),
	PE_STATE_ACTIONS(pe_snk_select_capability),
	PE_STATE_ACTIONS(pe_snk_transition_sink),
	PE_STATE_ACTIONS(pe_snk_ready),
	PE_STATE_ACTIONS(pe_snk_hard_reset),
	PE_STATE_ACTIONS(pe_snk_transition_to_default),
	PE_STATE_ACTIONS(pe_snk_give_sink_cap),
	PE_STATE_ACTIONS(pe_snk_get_source_cap),
	PE_STATE_ACTIONS(pe_snk_send_soft_reset),
	PE_STATE_ACTIONS(pe_snk_soft_reset),
/* Sink for PD30 */
#ifdef CONFIG_USB_PD_REV30
	PE_STATE_ACTIONS(pe_snk_send_not_supported),
	PE_STATE_ACTIONS(pe_snk_not_supported_received),
	PE_STATE_ACTIONS(pe_snk_chunk_received),
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	PE_STATE_ACTIONS(pe_snk_source_alert_received),
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	PE_STATE_ACTIONS(pe_snk_send_sink_alert),
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	PE_STATE_ACTIONS(pe_snk_get_source_cap_ext),
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	PE_STATE_ACTIONS(pe_snk_get_source_status),
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	PE_STATE_ACTIONS(pe_snk_give_sink_status),
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	PE_STATE_ACTIONS(pe_snk_get_pps_status),
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */
/******************* DR_SWAP *******************/
#ifdef CONFIG_USB_PD_DR_SWAP
/* DR_SWAP_DFP */
	PE_STATE_ACTIONS(pe_drs_dfp_ufp_evaluate_dr_swap),
	PE_STATE_ACTIONS(pe_drs_dfp_ufp_accept_dr_swap),
	PE_STATE_ACTIONS(pe_drs_dfp_ufp_change_to_ufp),
	PE_STATE_ACTIONS(pe_drs_dfp_ufp_send_dr_swap),
	PE_STATE_ACTIONS(pe_drs_dfp_ufp_reject_dr_swap),
/* DR_SWAP_UFP */
	PE_STATE_ACTIONS(pe_drs_ufp_dfp_evaluate_dr_swap),
	PE_STATE_ACTIONS(pe_drs_ufp_dfp_accept_dr_swap),
	PE_STATE_ACTIONS(pe_drs_ufp_dfp_change_to_dfp),
	PE_STATE_ACTIONS(pe_drs_ufp_dfp_send_dr_swap),
	PE_STATE_ACTIONS(pe_drs_ufp_dfp_reject_dr_swap),
#endif	/* CONFIG_USB_PD_DR_SWAP */
/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
/* PR_SWAP_SRC */
	PE_STATE_ACTIONS(pe_prs_src_snk_evaluate_pr_swap),
	PE_STATE_ACTIONS(pe_prs_src_snk_accept_pr_swap),
	PE_STATE_ACTIONS(pe_prs_src_snk_transition_to_off),
	PE_STATE_ACTIONS(pe_prs_src_snk_assert_rd),
	PE_STATE_ACTIONS(pe_prs_src_snk_wait_source_on),
	PE_STATE_ACTIONS(pe_prs_src_snk_send_swap),
	PE_STATE_ACTIONS(pe_prs_src_snk_reject_pr_swap),

/* PR_SWAP_SNK */
	PE_STATE_ACTIONS(pe_prs_snk_src_evaluate_pr_swap),
	PE_STATE_ACTIONS(pe_prs_snk_src_accept_pr_swap),
	PE_STATE_ACTIONS(pe_prs_snk_src_transition_to_off),
	PE_STATE_ACTIONS(pe_prs_snk_src_assert_rp),
	PE_STATE_ACTIONS(pe_prs_snk_src_source_on),
	PE_STATE_ACTIONS(pe_prs_snk_src_send_swap),
	PE_STATE_ACTIONS(pe_prs_snk_src_reject_swap),
/* get same role cap */
	PE_STATE_ACTIONS(pe_dr_src_get_source_cap),
	PE_STATE_ACTIONS(pe_dr_src_give_sink_cap),
	PE_STATE_ACTIONS(pe_dr_snk_get_sink_cap),
	PE_STATE_ACTIONS(pe_dr_snk_give_source_cap),
/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	PE_STATE_ACTIONS(pe_dr_snk_give_source_cap_ext),
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	PE_STATE_ACTIONS(pe_dr_src_get_source_cap_ext),
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */
/******************* VCONN_SWAP *******************/
#ifdef CONFIG_USB_PD_VCONN_SWAP
	PE_STATE_ACTIONS(pe_vcs_send_swap),
	PE_STATE_ACTIONS(pe_vcs_evaluate_swap),
	PE_STATE_ACTIONS(pe_vcs_accept_swap),
	PE_STATE_ACTIONS(pe_vcs_reject_vconn_swap),
	PE_STATE_ACTIONS(pe_vcs_wait_for_vconn),
	PE_STATE_ACTIONS(pe_vcs_turn_off_vconn),
	PE_STATE_ACTIONS(pe_vcs_turn_on_vconn),
	PE_STATE_ACTIONS(pe_vcs_send_ps_rdy),
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
/******************* UFP_VDM *******************/
	PE_STATE_ACTIONS(pe_ufp_vdm_get_identity),
	PE_STATE_ACTIONS(pe_ufp_vdm_get_svids),
	PE_STATE_ACTIONS(pe_ufp_vdm_get_modes),
	PE_STATE_ACTIONS(pe_ufp_vdm_evaluate_mode_entry),
	PE_STATE_ACTIONS(pe_ufp_vdm_mode_exit),
	PE_STATE_ACTIONS(pe_ufp_vdm_attention_request),
#ifdef CONFIG_USB_PD_ALT_MODE
	PE_STATE_ACTIONS(pe_ufp_vdm_dp_status_update),
	PE_STATE_ACTIONS(pe_ufp_vdm_dp_configure),
#endif/* CONFIG_USB_PD_ALT_MODE */
/******************* DFP_VDM *******************/
	PE_STATE_ACTIONS(pe_dfp_ufp_vdm_identity_request),
	PE_STATE_ACTIONS(pe_dfp_ufp_vdm_identity_acked),
	PE_STATE_ACTIONS(pe_dfp_ufp_vdm_identity_naked),
	PE_STATE_ACTIONS(pe_dfp_cbl_vdm_identity_request),
	PE_STATE_ACTIONS(pe_dfp_cbl_vdm_identity_acked),
	PE_STATE_ACTIONS(pe_dfp_cbl_vdm_identity_naked),
	PE_STATE_ACTIONS(pe_dfp_vdm_svids_request),
	PE_STATE_ACTIONS(pe_dfp_vdm_svids_acked),
	PE_STATE_ACTIONS(pe_dfp_vdm_svids_naked),
	PE_STATE_ACTIONS(pe_dfp_vdm_modes_request),
	PE_STATE_ACTIONS(pe_dfp_vdm_modes_acked),
	PE_STATE_ACTIONS(pe_dfp_vdm_modes_naked),
	PE_STATE_ACTIONS(pe_dfp_vdm_mode_entry_request),
	PE_STATE_ACTIONS(pe_dfp_vdm_mode_entry_acked),
	PE_STATE_ACTIONS(pe_dfp_vdm_mode_entry_naked),
	PE_STATE_ACTIONS(pe_dfp_vdm_mode_exit_request),
	PE_STATE_ACTIONS(pe_dfp_vdm_mode_exit_acked),
	PE_STATE_ACTIONS(pe_dfp_vdm_attention_request),
#ifdef CONFIG_PD_DFP_RESET_CABLE
	PE_STATE_ACTIONS(pe_dfp_cbl_send_soft_reset),
	PE_STATE_ACTIONS(pe_dfp_cbl_send_cable_reset),
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	PE_STATE_ACTIONS(pe_dfp_vdm_dp_status_update_request),
	PE_STATE_ACTIONS(pe_dfp_vdm_dp_status_update_acked),
	PE_STATE_ACTIONS(pe_dfp_vdm_dp_status_update_naked),
	PE_STATE_ACTIONS(pe_dfp_vdm_dp_configuration_request),
	PE_STATE_ACTIONS(pe_dfp_vdm_dp_configuration_acked),
	PE_STATE_ACTIONS(pe_dfp_vdm_dp_configuration_naked),
#endif/* CONFIG_USB_PD_ALT_MODE_DFP */
/******************* UVDM & SVDM *******************/
#ifdef CONFIG_USB_PD_CUSTOM_VDM
	PE_STATE_ACTIONS(pe_ufp_uvdm_recv),
	PE_STATE_ACTIONS(pe_dfp_uvdm_send),
	PE_STATE_ACTIONS(pe_dfp_uvdm_acked),
	PE_STATE_ACTIONS(pe_dfp_uvdm_naked),
#endif/* CONFIG_USB_PD_CUSTOM_VDM */
	PE_STATE_ACTIONS(pe_ufp_vdm_send_nak),
/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	PE_STATE_ACTIONS(pe_get_battery_cap),
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
	PE_STATE_ACTIONS(pe_give_battery_cap),
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	PE_STATE_ACTIONS(pe_get_battery_status),
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
	PE_STATE_ACTIONS(pe_give_battery_status),
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	PE_STATE_ACTIONS(pe_get_manufacturer_info),
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
	PE_STATE_ACTIONS(pe_give_manufacturer_info),
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	PE_STATE_ACTIONS(pe_get_country_codes),
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	PE_STATE_ACTIONS(pe_give_country_codes),
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	PE_STATE_ACTIONS(pe_get_country_info),
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */
#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
	PE_STATE_ACTIONS(pe_give_country_info),
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */
	PE_STATE_ACTIONS(pe_vdm_not_supported),
#endif /* CONFIG_USB_PD_REV30 */
/******************* Others *******************/
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	PE_STATE_ACTIONS(pe_dbg_ready),
#endif/* CONFIG_USB_PD_CUSTOM_DBGACC */
#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
	PE_STATE_ACTIONS(pe_over_recv_hreset_limit),
#endif/* CONFIG_USB_PD_RECV_HRESET_COUNTER */
	PE_STATE_ACTIONS(pe_reject),
	PE_STATE_ACTIONS(pe_error_recovery),
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
	PE_STATE_ACTIONS(pe_error_recovery_once),
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */
	PE_STATE_ACTIONS(pe_bist_test_data),
	PE_STATE_ACTIONS(pe_bist_carrier_mode_2),

#ifdef CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG
	PE_STATE_ACTIONS(pe_unexpected_tx_wait),
	PE_STATE_ACTIONS(pe_send_soft_reset_tx_wait),
	PE_STATE_ACTIONS(pe_recv_soft_reset_tx_wait),
	PE_STATE_ACTIONS(pe_send_soft_reset_standby),
#endif	/* CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG */

/* Wait tx finished */
	PE_STATE_ACTIONS(pe_idle1),
	PE_STATE_ACTIONS(pe_idle2),
};

/* pd_state_action_fcn_t pe_get_exit_action(uint8_t pe_state) */
void (*pe_get_exit_action(uint8_t pe_state))
		(struct pd_port *)
{
	void (*retval)(struct pd_port *) = NULL;

	switch (pe_state) {
/******************* Source *******************/
#ifdef CONFIG_USB_PD_PE_SOURCE
	case PE_SRC_TRANSITION_TO_DEFAULT:
		retval = pe_src_transition_to_default_exit;
		break;
	case PE_SRC_GET_SINK_CAP:
		retval = pe_src_get_sink_cap_exit;
		break;
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	case PE_SRC_GET_SINK_STATUS:
		retval = pe_src_get_sink_status_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SOURCE */

/******************* Sink *******************/
#ifdef CONFIG_USB_PD_PE_SINK
	case PE_SNK_SELECT_CAPABILITY:
		retval = pe_snk_select_capability_exit;
		break;

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	case PE_SNK_GET_SOURCE_CAP_EXT:
		retval = pe_snk_get_source_cap_ext_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */

#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
	case PE_SNK_GET_SOURCE_STATUS:
		retval = pe_snk_get_source_status_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	case PE_SNK_GET_PPS_STATUS:
		retval = pe_snk_get_pps_status_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PE_SINK */

/******************* PR_SWAP *******************/
#ifdef CONFIG_USB_PD_PR_SWAP
	case PE_DR_SRC_GET_SOURCE_CAP:
		retval = pe_dr_src_get_source_cap_exit;
		break;

	case PE_DR_SNK_GET_SINK_CAP:
		retval = pe_dr_snk_get_sink_cap_exit;
		break;

/* get same role cap for PD30 */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	case PE_DR_SRC_GET_SOURCE_CAP_EXT:
		retval = pe_dr_src_get_source_cap_ext_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_PD_PR_SWAP */

/******************* PD30 Common *******************/
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	case PE_GET_BATTERY_CAP:
		retval = pe_get_battery_cap_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */

#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	case PE_GET_BATTERY_STATUS:
		retval = pe_get_battery_status_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */

#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	case PE_GET_MANUFACTURER_INFO:
		retval = pe_get_manufacturer_info_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */


#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	case PE_GET_COUNTRY_CODES:
		retval = pe_get_country_codes_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */

#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	case PE_GET_COUNTRY_INFO:
		retval = pe_get_country_info_exit;
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */

#endif /* CONFIG_USB_PD_REV30 */
	case PE_BIST_TEST_DATA:
		retval = pe_bist_test_data_exit;
		break;
	case PE_BIST_CARRIER_MODE_2:
		retval = pe_bist_carrier_mode_2_exit;
		break;
	default:
		break;
	}

	return retval;
}

static inline void print_state(
	struct pd_port *pd_port, uint8_t state)
{
	/*
	 * Source (P, Provider), Sink (C, Consumer)
	 * DFP (D), UFP (U)
	 * Vconn Source (Y/N)
	 */
	bool __maybe_unused vdm_evt = pd_curr_is_vdm_evt(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

#if PE_DBG_ENABLE
	PE_DBG("%s -> %s (%c%c%c)\n",
		vdm_evt ? "VDM" : "PD", pe_state_name[state],
		pd_port->power_role ? 'P' : 'C',
		pd_port->data_role ? 'D' : 'U',
		pd_port->vconn_role ? 'Y' : 'N');
#else
	PE_STATE_INFO("%s-> %s\n",
		vdm_evt ? "VDM" : "PD", pe_state_name[state]);
#endif	/* PE_DBG_ENABLE */
}

static void pe_reset_vdm_state_variable(
	struct pd_port *pd_port, struct pe_data *pe_data)
{
	if (pe_data->vdm_state_timer)
		pd_disable_timer(pd_port, pe_data->vdm_state_timer);

	pe_data->vdm_state_flags = 0;
	pe_data->vdm_state_timer = 0;
}

static inline void pd_pe_state_change(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	void (*prev_exit_action)(struct pd_port *pd_port);
	void (*next_entry_action)(struct pd_port *pd_port);
	struct pe_data *pe_data = &pd_port->pe_data;

	uint8_t old_state = pd_port->pe_state_curr;
	uint8_t new_state = pd_port->pe_state_next;

	if (old_state >= PD_NR_PE_STATES || new_state >= PD_NR_PE_STATES) {
		PD_BUG_ON(1);
		return;
	}

	if (new_state < PE_IDLE1)
		prev_exit_action = pe_get_exit_action(old_state);
	else
		prev_exit_action = NULL;

	next_entry_action = pe_state_actions[new_state].entry_action;

#if PE_STATE_INFO_VDM_DIS
	if (!pd_curr_is_vdm_evt(pd_port))
#endif	/* PE_STATE_INFO_VDM_DIS */
		print_state(pd_port, new_state);

	if (pe_data->pe_state_flags &
		PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER) {
		pd_disable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
	}

	if (pd_curr_is_vdm_evt(pd_port))
		pe_reset_vdm_state_variable(pd_port, pe_data);
	else if (pe_data->pe_state_timer) {
		pd_disable_timer(pd_port, pe_data->pe_state_timer);
		pe_data->pe_state_timer = 0;
	}

	pe_data->pe_state_flags = 0;
	pe_data->pe_state_flags2 = 0;

	if (prev_exit_action)
		prev_exit_action(pd_port);

	if (next_entry_action)
		next_entry_action(pd_port);

	if (pd_curr_is_vdm_evt(pd_port))
		pd_port->pe_vdm_state = new_state;
	else
		pd_port->pe_pd_state = new_state;

	pd_port->pe_state_curr = new_state;

	/* Change RX cap first for compliance */
	if (pd_port->state_machine > PE_STATE_MACHINE_NORMAL)
		pd_set_rx_enable(pd_port, PD_RX_CAP_PE_SWAP);
}

static int pd_handle_event(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool dpm_imme;
	struct pe_data *pe_data = &pd_port->pe_data;

	if (pd_curr_is_vdm_evt(pd_port)) {
		dpm_imme = pd_port->pe_data.vdm_state_flags
			& VDM_STATE_FLAG_DPM_ACK_IMMEDIATELY;
		if ((pe_data->reset_vdm_state && (!dpm_imme)) ||
			(pd_event->event_type == PD_EVT_TCP_MSG)) {
			pe_data->reset_vdm_state = false;
			pd_port->pe_vdm_state = pd_port->pe_pd_state;
			pe_reset_vdm_state_variable(pd_port, pe_data);
		}

		pd_port->pe_state_curr = pd_port->pe_vdm_state;
	} else {
		pd_port->pe_state_curr = pd_port->pe_pd_state;
	}

	if (pd_process_event(pd_port, pd_event))
		pd_pe_state_change(pd_port, pd_event);

	pd_free_event(pd_port->tcpc, pd_event);
	return 1;
}

/*
 * Get Next Event
 */

enum PE_NEW_EVT_TYPE {
	PE_NEW_EVT_NULL = 0,
	PE_NEW_EVT_PD = 1,
	PE_NEW_EVT_VDM = 2,
};

static inline bool pd_try_get_vdm_event(
	struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	bool ret = false;
	struct pd_port *pd_port = &tcpc->pd_port;

	switch (pd_port->pe_pd_state) {
#ifdef CONFIG_USB_PD_PE_SINK
	case PE_SNK_READY:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;
#endif	/* CONFIG_USB_PD_PE_SINK */

#ifdef CONFIG_USB_PD_PE_SOURCE
	case PE_SRC_READY:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;
	case PE_SRC_STARTUP:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;
	case PE_SRC_DISCOVERY:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;

#ifdef CONFIG_PD_SRC_RESET_CABLE
	case PE_SRC_CBL_SEND_SOFT_RESET:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
#endif	/* CONFIG_USB_PD_PE_SOURCE */

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	case PE_DBG_READY:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */
	case PE_IDLE1:
		ret = pd_get_vdm_event(tcpc, pd_event);
		break;
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_USB_PD_REV30

static inline bool pd_check_sink_tx_ok(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (pd_check_rev30(pd_port) &&
		(pd_port->pe_data.pd_traffic_control != PD_SINK_TX_OK))
		return false;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	return true;
}

static inline bool pd_check_source_tx_ok(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (!pd_check_rev30(pd_port))
		return true;

	if (pd_port->pe_data.pd_traffic_control == PD_SOURCE_TX_OK)
		return true;

	if (!pd_port->pe_data.pd_traffic_idle)
		pd_set_sink_tx(pd_port, PD30_SINK_TX_NG);

	return false;
#else
	return true;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
}

static inline bool pd_check_pd30_tx_ready(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_PE_SINK
	if (pd_port->pe_pd_state == PE_SNK_READY)
		return pd_check_sink_tx_ok(pd_port);
#endif	/* CONFIG_USB_PD_PE_SINK */

#ifdef CONFIG_USB_PD_PE_SOURCE
	if (pd_port->pe_pd_state == PE_SRC_READY)
		return pd_check_source_tx_ok(pd_port);
#endif	/* CONFIG_USB_PD_PE_SOURCE */

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	if (pd_port->pe_pd_state == PE_DBG_READY)
		return true;
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */

	return false;
}
#else
static inline bool pd_check_pd20_tx_ready(struct pd_port *pd_port)
{
	switch (pd_port->pe_pd_state) {
#ifdef CONFIG_USB_PD_PE_SINK
	case PE_SNK_READY:
		return true;
#endif	/* CONFIG_USB_PD_PE_SINK */

#ifdef CONFIG_USB_PD_PE_SOURCE
	case PE_SRC_READY:
		return true;
#endif	/* CONFIG_USB_PD_PE_SOURCE */

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	case PE_DBG_READY:
		return true;
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */

	default:
		return false;
	}
}
#endif	/* ndef CONFIG_USB_PD_REV30 */

/**
 * pd_check_tx_ready
 *
 * Check PE is ready to initiate an active event (AMS).
 *
 * For revision 2, checking the PE state is in Ready.
 * For revision 3, checking SinkTx besides above description.
 *
 * If PR=SRC and CC=SinkTxOK, change CC to SinkTxNG;
 *
 * Returns a boolean value to present PE is available or not.
 */

static inline bool pd_check_tx_ready(struct pd_port *pd_port)
{
	/* VDM BUSY : Waiting for response */
	if (pd_port->pe_data.vdm_state_timer)
		return false;

#ifdef CONFIG_USB_PD_REV30
	return pd_check_pd30_tx_ready(pd_port);
#else
	return pd_check_pd20_tx_ready(pd_port);
#endif	/* CONFIG_USB_PD_REV30 */
}

/**
 * pd_try_get_deferred_tcp_event
 *
 * Get a pending TCPM event from the event queue
 *
 * Returns TCP_DPM_EVT_ID if succeeded,
 * otherwise returns DPM_READY_REACTION_BUSY.
 */

static inline uint8_t pd_try_get_deferred_tcp_event(struct pd_port *pd_port)
{
	if (!pd_get_deferred_tcp_event(
		pd_port->tcpc, &pd_port->tcp_event))
		return DPM_READY_REACTION_BUSY;

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcp_event_drop_reset_once = true;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	pd_port->tcp_event_id_1st = pd_port->tcp_event.event_id;
	return pd_port->tcp_event_id_1st;
}

/**
 * pd_try_get_active_event
 *
 * Get a pending active event if TX is available
 *
 * Event Priority :
 *	DPM reactions, TCPM request.
 *
 * Returns PE_NEW_EVT_TYPE.
 */

static inline uint8_t pd_try_get_active_event(
	struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	uint8_t ret;
	uint8_t from_pe = PD_TCP_FROM_PE;
	struct pd_port *pd_port = &tcpc->pd_port;

	if (!pd_check_tx_ready(pd_port))
		return PE_NEW_EVT_NULL;

#ifdef CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG
	if (pd_port->pe_data.pd_unexpected_event_pending) {
		pd_port->pe_data.pd_unexpected_event_pending = false;
		*pd_event = pd_port->pe_data.pd_unexpected_event;
		pd_port->pe_data.pd_unexpected_event.pd_msg = NULL;
		PE_INFO("##$$120\n");
		DPM_INFO("Re-Run Unexpected Msg");
		return PE_NEW_EVT_PD;
	}
#endif	/* CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG */

	ret = pd_dpm_get_ready_reaction(pd_port);

	if (ret == 0) {
		from_pe = PD_TCP_FROM_TCPM;
		ret = pd_try_get_deferred_tcp_event(pd_port);
	}

#if DPM_DBG_ENABLE
	if ((ret != 0) && (ret != DPM_READY_REACTION_BUSY)) {
		DPM_DBG("from_pe: %d, evt:%d, reaction:0x%x\n",
			from_pe, ret, pd_port->pe_data.dpm_reaction_id);
	}
#endif	/* DPM_DBG_ENABLE */

	if (ret == DPM_READY_REACTION_BUSY)
		return PE_NEW_EVT_NULL;

	pd_event->event_type = PD_EVT_TCP_MSG;
	pd_event->msg = ret;
	pd_event->msg_sec = from_pe;
	pd_event->pd_msg = NULL;

	if (ret >= TCP_DPM_EVT_VDM_COMMAND)
		return PE_NEW_EVT_VDM;

#ifdef CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG
	pd_port->pe_data.pd_sent_ams_init_cmd = false;
#endif	/* CONFIG_USB_PD_DISCARD_AND_UNEXPECT_MSG */

	return PE_NEW_EVT_PD;
}

/**
 * pd_try_get_next_event
 *
 * Get a pending event
 *
 * Event Priority :
 *	PD state machine's event, VDM state machine's event,
 *	Active event (DPM reactions, TCPM request)
 *
 * Returns PE_NEW_EVT_TYPE.
 */

static inline uint8_t pd_try_get_next_event(
	struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	uint8_t ret = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_get_event(tcpc, pd_event))
		return PE_NEW_EVT_PD;

	if (pd_try_get_vdm_event(tcpc, pd_event))
		return PE_NEW_EVT_VDM;

	mutex_lock(&pd_port->pd_lock);
	ret = pd_try_get_active_event(tcpc, pd_event);
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

/*
 * Richtek Policy Engine
 */

static inline int pd_handle_dpm_immediately(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool dpm_immediately;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_curr_is_vdm_evt(pd_port)) {
		dpm_immediately = pd_port->pe_data.vdm_state_flags
			& VDM_STATE_FLAG_DPM_ACK_IMMEDIATELY;
	} else {
		dpm_immediately = pd_port->pe_data.pe_state_flags2
			& PE_STATE_FLAG_DPM_ACK_IMMEDIATELY;
	}

	if (dpm_immediately) {
		PE_DBG("DPM_Immediately\n");
		pd_event->event_type = PD_EVT_DPM_MSG;
		pd_event->msg = PD_DPM_ACK;
		return pd_handle_event(pd_port, pd_event);
	}

	return false;
}

int pd_policy_engine_run(struct tcpc_device *tcpc)
{
	bool loop = true;
	uint8_t ret;
	struct pd_port *pd_port = &tcpc->pd_port;
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);

	ret = pd_try_get_next_event(tcpc, pd_event);
	if (ret == PE_NEW_EVT_NULL) {
		loop = false;
		goto out;
	}

	mutex_lock(&pd_port->pd_lock);

	pd_port->curr_is_vdm_evt = (ret == PE_NEW_EVT_VDM);

	pd_handle_event(pd_port, pd_event);
	pd_handle_dpm_immediately(pd_port, pd_event);

	mutex_unlock(&pd_port->pd_lock);
out:
	return loop;
}
