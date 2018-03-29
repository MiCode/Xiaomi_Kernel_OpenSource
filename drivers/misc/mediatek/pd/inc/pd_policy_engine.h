/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef PD_POLICY_ENGINE_H_
#define PD_POLICY_ENGINE_H_

#include "pd_core.h"

/* ---- Policy Engine State ---- */

enum pd_pe_state_machine {
	PE_STATE_MACHINE_IDLE = 0,
	PE_STATE_MACHINE_SINK,
	PE_STATE_MACHINE_SOURCE,
	PE_STATE_MACHINE_DR_SWAP,
	PE_STATE_MACHINE_PR_SWAP,
	PE_STATE_MACHINE_VCONN_SWAP,
};

enum pd_pe_state {
	PE_SRC_STARTUP = 0,
	PE_SRC_DISCOVERY,
	PE_SRC_SEND_CAPABILITIES,
	PE_SRC_NEGOTIATE_CAPABILITIES,
	PE_SRC_TRANSITION_SUPPLY,
	PE_SRC_READY,
	PE_SRC_DISABLED,
	PE_SRC_CAPABILITY_RESPONSE,
	PE_SRC_HARD_RESET,
	PE_SRC_HARD_RESET_RECEIVED,
	PE_SRC_TRANSITION_TO_DEFAULT,
	PE_SRC_GIVE_SOURCE_CAP,
	PE_SRC_GET_SINK_CAP,
	PE_SRC_WAIT_NEW_CAPABILITIES,

	PE_SRC_SEND_SOFT_RESET,
	PE_SRC_SOFT_RESET,
	PE_SRC_PING,

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	PE_SRC_VDM_IDENTITY_REQUEST,
	PE_SRC_VDM_IDENTITY_ACKED,
	PE_SRC_VDM_IDENTITY_NAKED,
#endif	/* PD_CAP_PE_SRC_STARTUP_DISCOVER_ID */

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

	PE_DRS_DFP_UFP_EVALUATE_DR_SWAP,
	PE_DRS_DFP_UFP_ACCEPT_DR_SWAP,
	PE_DRS_DFP_UFP_CHANGE_TO_UFP,
	PE_DRS_DFP_UFP_SEND_DR_SWAP,
	PE_DRS_DFP_UFP_REJECT_DR_SWAP,

	PE_DRS_UFP_DFP_EVALUATE_DR_SWAP,
	PE_DRS_UFP_DFP_ACCEPT_DR_SWAP,
	PE_DRS_UFP_DFP_CHANGE_TO_DFP,
	PE_DRS_UFP_DFP_SEND_DR_SWAP,
	PE_DRS_UFP_DFP_REJECT_DR_SWAP,

	PE_PRS_SRC_SNK_EVALUATE_PR_SWAP,
	PE_PRS_SRC_SNK_ACCEPT_PR_SWAP,
	PE_PRS_SRC_SNK_TRANSITION_TO_OFF,
	PE_PRS_SRC_SNK_ASSERT_RD,
	PE_PRS_SRC_SNK_WAIT_SOURCE_ON,
	PE_PRS_SRC_SNK_SEND_SWAP,
	PE_PRS_SRC_SNK_REJECT_PR_SWAP,

	PE_PRS_SNK_SRC_EVALUATE_PR_SWAP,
	PE_PRS_SNK_SRC_ACCEPT_PR_SWAP,
	PE_PRS_SNK_SRC_TRANSITION_TO_OFF,
	PE_PRS_SNK_SRC_ASSERT_RP,
	PE_PRS_SNK_SRC_SOURCE_ON,
	PE_PRS_SNK_SRC_SEND_SWAP,
	PE_PRS_SNK_SRC_REJECT_SWAP,

	PE_DR_SRC_GET_SOURCE_CAP,
	PE_DR_SRC_GIVE_SINK_CAP,
	PE_DR_SNK_GET_SINK_CAP,
	PE_DR_SNK_GIVE_SOURCE_CAP,

	PE_VCS_SEND_SWAP,
	PE_VCS_EVALUATE_SWAP,
	PE_VCS_ACCEPT_SWAP,
	PE_VCS_REJECT_VCONN_SWAP,
	PE_VCS_WAIT_FOR_VCONN,
	PE_VCS_TURN_OFF_VCONN,
	PE_VCS_TURN_ON_VCONN,
	PE_VCS_SEND_PS_RDY,

	PE_UFP_VDM_GET_IDENTITY,
	PE_UFP_VDM_SEND_IDENTITY,
	PE_UFP_VDM_GET_IDENTITY_NAK,

	PE_UFP_VDM_GET_SVIDS,
	PE_UFP_VDM_SEND_SVIDS,
	PE_UFP_VDM_GET_SVIDS_NAK,

	PE_UFP_VDM_GET_MODES,
	PE_UFP_VDM_SEND_MODES,
	PE_UFP_VDM_GET_MODES_NAK,

	PE_UFP_VDM_EVALUATE_MODE_ENTRY,
	PE_UFP_VDM_MODE_ENTRY_ACK,
	PE_UFP_VDM_MODE_ENTRY_NAK,

	PE_UFP_VDM_MODE_EXIT,
	PE_UFP_VDM_MODE_EXIT_ACK,
	PE_UFP_VDM_MODE_EXIT_NAK,

	PE_UFP_VDM_ATTENTION_REQUEST,

#ifdef CONFIG_USB_PD_ALT_MODE
	PE_UFP_VDM_DP_STATUS_UPDATE,
	PE_UFP_VDM_DP_CONFIGURE,
#endif

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

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST,
	PE_DFP_VDM_DP_STATUS_UPDATE_ACKED,
	PE_DFP_VDM_DP_STATUS_UPDATE_NAKED,

	PE_DFP_VDM_DP_CONFIGURATION_REQUEST,
	PE_DFP_VDM_DP_CONFIGURATION_ACKED,
	PE_DFP_VDM_DP_CONFIGURATION_NAKED,
#endif

	PE_ERROR_RECOVERY,

	PE_BIST_TEST_DATA,
	PE_BIST_CARRIER_MODE_2,

	PE_IDLE,
#ifdef CONFIG_RT7207_ADAPTER
	PE_VDM_RT7207_REQUEST,
	PE_VDM_RT7207_ACKED,
	PE_VDM_RT7207_NAKED,
#endif /* CONFIG_RT7207_ADAPTER */

	PD_NR_PE_STATES,

	PE_VIRT_HARD_RESET,
	PE_VIRT_READY,

};

enum pd_dpm_vdm_request_type {
	PD_DPM_VDM_REQUEST_DISCOVER_ID =
		PE_DFP_UFP_VDM_IDENTITY_REQUEST,
	PD_DPM_VDM_REQUEST_DISCOVER_SVIDS =
		PE_DFP_VDM_SVIDS_REQUEST,
	PD_DPM_VDM_REQUEST_DISCOVER_MODES =
		PE_DFP_VDM_MODES_REQUEST,
	PD_DPM_VDM_REQUEST_ENTRY_MODE =
		PE_DFP_VDM_MODE_ENTRY_REQUEST,
	PD_DPM_VDM_REQUEST_EXIT_MODE =
		PE_DFP_VDM_MODE_EXIT_REQUEST,

	PD_DPM_VDM_REQUEST_ATTENTION =
		PE_UFP_VDM_ATTENTION_REQUEST,

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	PD_DPM_VDM_REQUEST_DP_STATUS_UPDATE =
		PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST,
	PD_DPM_VDM_REQUEST_DP_CONFIG =
		PE_DFP_VDM_DP_CONFIGURATION_REQUEST,
#endif
#ifdef CONFIG_RT7207_ADAPTER
	PD_DPM_VDM_RT7207_REQUEST =
			PE_VDM_RT7207_REQUEST,
#endif /* CONFIG_RT7207_ADAPTER */
};

/*
 * > 0 : Process Event
 * = 0 : No Event
 * < 0 : Error
*/

int pd_policy_engine_run(struct tcpc_device *tcpc_dev);


/* ---- Policy Engine (General) ---- */

void pe_power_ready_entry(pd_port_t *pd_port, pd_event_t *pd_event);


/* ---- Policy Engine (SNK) ---- */

void pe_snk_startup_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_discovery_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_wait_for_capabilities_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_evaluate_capability_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_select_capability_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_transition_sink_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_ready_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_hard_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_transition_to_default_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_give_sink_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_get_source_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_send_soft_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_soft_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_snk_wait_for_capabilities_exit(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_select_capability_exit(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_transition_sink_exit(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_snk_transition_to_default_exit(
			pd_port_t *pd_port, pd_event_t *pd_event);


/* ---- Policy Engine (SRC) ---- */

void pe_src_startup_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_discovery_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_send_capabilities_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_negotiate_capabilities_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_transition_supply_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_ready_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_disabled_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_capability_response_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_hard_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_hard_reset_received_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_transition_to_default_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_give_source_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_get_sink_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_wait_new_capabilities_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_send_soft_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_soft_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_ping_entry(pd_port_t *pd_port, pd_event_t *pd_event);

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
void pe_src_vdm_identity_request_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_vdm_identity_acked_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_vdm_identity_naked_entry(pd_port_t *pd_port, pd_event_t *pd_event);
#endif

void pe_src_send_capabilities_exit(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_transition_supply_exit(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_transition_to_default_exit(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_src_get_sink_cap_exit(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- Policy Engine (DRS) ---- */

void pe_drs_dfp_ufp_evaluate_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_dfp_ufp_accept_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_dfp_ufp_change_to_ufp_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_dfp_ufp_send_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_dfp_ufp_reject_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);

void pe_drs_ufp_dfp_evaluate_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_ufp_dfp_accept_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_ufp_dfp_change_to_dfp_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_ufp_dfp_send_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_drs_ufp_dfp_reject_dr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);


/* ---- Policy Engine (PRS) ---- */

void pe_prs_src_snk_evaluate_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_src_snk_accept_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_src_snk_transition_to_off_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_src_snk_assert_rd_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_src_snk_wait_source_on_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_src_snk_send_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_src_snk_reject_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

void pe_prs_snk_src_evaluate_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_accept_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_transition_to_off_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_assert_rp_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_source_on_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_send_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_reject_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_prs_src_snk_wait_source_on_exit(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_transition_to_off_exit(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_prs_snk_src_source_on_exit(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- Policy Engine (DR) ---- */

void pe_dr_src_get_source_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dr_src_give_sink_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dr_snk_get_sink_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dr_snk_give_source_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dr_src_get_source_cap_exit(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dr_snk_get_sink_cap_exit(pd_port_t *pd_port, pd_event_t *pd_event);


/* ---- Policy Engine (VCS) ---- */

void pe_vcs_send_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_evaluate_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_accept_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_reject_vconn_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_wait_for_vconn_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_turn_off_vconn_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_turn_on_vconn_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vcs_send_ps_rdy_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_vcs_wait_for_vconn_exit(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- Policy Engine (UFP) ---- */

void pe_ufp_vdm_get_identity_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_send_identity_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_get_identity_nak_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

void pe_ufp_vdm_get_svids_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_send_svids_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_get_svids_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_ufp_vdm_get_modes_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_send_modes_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_get_modes_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_ufp_vdm_evaluate_mode_entry_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_mode_entry_ack_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_mode_entry_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_ufp_vdm_mode_exit_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_mode_exit_ack_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_mode_exit_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_ufp_vdm_attention_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

#ifdef CONFIG_USB_PD_ALT_MODE
void pe_ufp_vdm_dp_status_update_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_ufp_vdm_dp_configure_entry(pd_port_t *pd_port, pd_event_t *pd_event);
#endif


/* ---- Policy Engine (DFP) ---- */

#ifdef CONFIG_RT7207_ADAPTER
void pe_vdm_rt7207_request_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vdm_rt7207_acked_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_vdm_rt7207_naked_entry(pd_port_t *pd_port, pd_event_t *pd_event);
#endif /* CONFIG_RT7207_ADAPTER */

void pe_dfp_ufp_vdm_identity_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_ufp_vdm_identity_acked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_ufp_vdm_identity_naked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_cbl_vdm_identity_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_cbl_vdm_identity_acked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_cbl_vdm_identity_naked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_vdm_svids_request_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_svids_acked_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_svids_naked_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_vdm_modes_request_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_modes_acked_entry(pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_modes_naked_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_vdm_mode_entry_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_mode_entry_acked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_mode_entry_naked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_vdm_mode_exit_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_mode_exit_acked_entry(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_vdm_attention_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
void pe_dfp_vdm_dp_status_update_request_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_dp_status_update_acked_entry(
				pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_dp_status_update_naked_entry
				(pd_port_t *pd_port, pd_event_t *pd_event);

void pe_dfp_vdm_dp_configuration_request_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_dp_configuration_acked_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
void pe_dfp_vdm_dp_configuration_naked_entry(
			pd_port_t *pd_port, pd_event_t *pd_event);
#endif

#endif /* PD_POLICY_ENGINE_H_ */
