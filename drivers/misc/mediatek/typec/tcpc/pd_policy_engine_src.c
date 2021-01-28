// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-38 Source Port Policy Engine state diagram
 */

void pe_src_startup_entry(struct pd_port *pd_port)
{
	enum typec_pwr_opmode opmode;

	switch (pd_port->tcpc_dev->typec_remote_rp_level) {
	case TYPEC_CC_VOLT_SNK_DFT:
		opmode = TYPEC_PWR_MODE_USB;
		break;
	case TYPEC_CC_VOLT_SNK_1_5:
		opmode = TYPEC_PWR_MODE_1_5A;
		break;
	case TYPEC_CC_VOLT_SNK_3_0:
		opmode = TYPEC_PWR_MODE_3_0A;
		break;
	default:
		opmode = TYPEC_PWR_MODE_USB;
		break;
	}

	typec_set_pwr_opmode(pd_port->tcpc_dev->typec_port, opmode);
	pd_reset_protocol_layer(pd_port, false);
	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_STARTUP);

	/*
	 * When entering this state,
	 * VBUS must be valid even for cc_attached.
	 */

	pd_enable_timer(pd_port, PD_TIMER_SOURCE_START);
}

void pe_src_discovery_entry(struct pd_port *pd_port)
{
	/* MessageID Should be 0 for First SourceCap (Ellisys)... */

	/* The SourceCapabilitiesTimer continues to run during the states
	 * defined in Source Startup Structured VDM Discover Identity State
	 * Diagram
	 */

	pd_port->pe_data.msg_id_tx[TCPC_TX_SOP] = 0;
	pd_port->pe_data.pd_connected = false;

	pd_enable_timer(pd_port, PD_TIMER_SOURCE_CAPABILITY);

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	if (pd_is_discover_cable(pd_port)) {
		pd_port->pe_data.msg_id_tx[TCPC_TX_SOP_PRIME] = 0;
		pd_enable_timer(pd_port, PD_TIMER_DISCOVER_ID);
	}
#endif
}

void pe_src_send_capabilities_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG_HRESET_IF_TOUT(pd_port);

	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_SEND_WAIT_CAP);

	pd_dpm_send_source_caps(pd_port);
	pd_port->pe_data.cap_counter++;
}

void pe_src_negotiate_capabilities_entry(
				struct pd_port *pd_port)
{
	pd_handle_first_pd_command(pd_port);
	pd_dpm_src_evaluate_request(pd_port);
}

void pe_src_transition_supply_entry(struct pd_port *pd_port)
{
	uint8_t msg = PD_CTRL_ACCEPT;
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);

	/* goto-min */
	if (pd_event->event_type == PD_EVT_TCP_MSG)	 {
		msg = PD_CTRL_GOTO_MIN;
		pd_port->request_i_new = pd_port->request_i_op;
	}

	typec_set_pwr_opmode(pd_port->tcpc_dev->typec_port, TYPEC_PWR_MODE_PD);
	pd_send_sop_ctrl_msg(pd_port, msg);
}

void pe_src_transition_supply2_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_PS_RDY);
}

void pe_src_ready_entry(struct pd_port *pd_port)
{
	pd_notify_pe_src_explicit_contract(pd_port);
	pe_power_ready_entry(pd_port);
	pd_port->tcpc_dev->typec_caps.data = TYPEC_PORT_DRD;
}

void pe_src_disabled_entry(struct pd_port *pd_port)
{
	pd_notify_pe_hard_reset_completed(pd_port);

	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_DISABLE);
	pd_update_connect_state(pd_port, PD_CONNECT_TYPEC_ONLY_SRC);
	pd_dpm_dynamic_disable_vconn(pd_port);
}

void pe_src_capability_response_entry(struct pd_port *pd_port)
{
	pd_reply_wait_reject_msg_no_resp(pd_port);
}

void pe_src_hard_reset_entry(struct pd_port *pd_port)
{
	pd_send_hard_reset(pd_port);
	pd_enable_timer(pd_port, PD_TIMER_PS_HARD_RESET);
}

void pe_src_hard_reset_received_entry(struct pd_port *pd_port)
{
	pd_enable_timer(pd_port, PD_TIMER_PS_HARD_RESET);
}

void pe_src_transition_to_default_entry(struct pd_port *pd_port)
{
	pd_reset_local_hw(pd_port);
	pd_dpm_src_hard_reset(pd_port);
}

void pe_src_transition_to_default_exit(struct pd_port *pd_port)
{
	pd_set_vconn(pd_port, PD_ROLE_VCONN_ON);
	pd_enable_timer(pd_port, PD_TIMER_NO_RESPONSE);
}

void pe_src_get_sink_cap_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG(pd_port);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_SINK_CAP);
}

void pe_src_get_sink_cap_exit(struct pd_port *pd_port)
{
	pd_dpm_dr_inform_sink_cap(pd_port);
}

void pe_src_wait_new_capabilities_entry(
			struct pd_port *pd_port)
{
	/* Wait for new Source Capabilities */
}

void pe_src_send_soft_reset_entry(struct pd_port *pd_port)
{
	pd_send_soft_reset(pd_port);
}

void pe_src_soft_reset_entry(struct pd_port *pd_port)
{
	pd_handle_soft_reset(pd_port);
}

/*
 * [PD2.0] Figure 8-81
 Source Startup Structured VDM Discover Identity State Diagram (TODO)
 */

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID

#ifdef CONFIG_PD_SRC_RESET_CABLE
void pe_src_cbl_send_soft_reset_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_RESPONSE(pd_port);

	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_DISCOVER_CABLE);

	pd_send_cable_soft_reset(pd_port);
}
#endif	/* CONFIG_PD_SRC_RESET_CABLE */

void pe_src_vdm_identity_request_entry(struct pd_port *pd_port)
{
	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_DISCOVER_CABLE);

	pd_port->pe_data.discover_id_counter++;
	pd_send_vdm_discover_id(pd_port, TCPC_TX_SOP_PRIME);
}

void pe_src_vdm_identity_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_inform_cable_id(pd_port, true);
}

void pe_src_vdm_identity_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_inform_cable_id(pd_port, true);
}

#endif	/* CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID */


#ifdef CONFIG_USB_PD_REV30

/*
 * [PD3.0] Source Port Not Supported Message State Diagram
 */

void pe_src_send_not_supported_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_NOT_SUPPORTED);
}

void pe_src_not_supported_received_entry(struct pd_port *pd_port)
{
	PE_STATE_DPM_INFORMED(pd_port);

	pd_dpm_inform_not_support(pd_port);
}

void pe_src_chunk_received_entry(struct pd_port *pd_port)
{
	pd_enable_timer(pd_port, PD_TIMER_CK_NO_SUPPORT);
}

/*
 * [PD3.0] Figure 8-73 Source Port Source Alert State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
void pe_src_send_source_alert_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);
	pd_dpm_send_alert(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

/*
 * [PD3.0] Figure 8-76 Source Port Sink Alert State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
void pe_src_sink_alert_received_entry(struct pd_port *pd_port)
{
	PE_STATE_DPM_INFORMED(pd_port);

	pd_dpm_inform_alert(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

/*
 * [PD3.0] Figure 8-78 Source Give Source Capabilities Extended State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
void pe_src_give_source_cap_ext_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_source_cap_ext(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */

/*
 * [PD3.0] Figure 8-80 Source Give Source Status State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
void pe_src_give_source_status_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_status(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */

/*
 * [PD3.0] Figure 8-81 Source Port Get Sink Status State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
void pe_src_get_sink_status_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_STATUS);
}

void pe_src_get_sink_status_exit(struct pd_port *pd_port)
{
	pd_dpm_inform_status(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

/*
 * [PD3.0] Figure 8-84 Source Give Source PPS Status State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
void pe_src_give_pps_status_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	/* TODO */
	PD_BUG_ON(1);
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */

#endif	/* CONFIG_USB_PD_REV30 */
