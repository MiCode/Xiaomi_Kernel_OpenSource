// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-39 Sink Port state diagram
 */

void pe_snk_startup_entry(struct pd_port *pd_port)
{
	uint8_t rx_cap = PD_RX_CAP_PE_STARTUP;
	bool pr_swap = pd_port->state_machine == PE_STATE_MACHINE_PR_SWAP;
	enum typec_pwr_opmode opmode;

#ifdef CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP
	uint8_t msg_id_last = pd_port->pe_data.msg_id_rx[TCPC_TX_SOP];
#endif	/* CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP */

	pd_reset_protocol_layer(pd_port, false);

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

	if (pr_swap) {
		/*
		 * If PE reset rx_cap to startup in here,
		 * maybe can't meet tSwapSink for PR_SWAP case
		 */
		rx_cap = PD_RX_CAP_PE_SEND_WAIT_CAP;

#ifdef CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP
		pd_port->msg_id_pr_swap_last = msg_id_last;
#endif	/* CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP */
	}

#ifdef CONFIG_USB_PD_SNK_HRESET_KEEP_DRAW
	/* iSafe0mA: Maximum current a Sink
	 * is allowed to draw when VBUS is driven to vSafe0V
	 */
	if (pd_check_pe_during_hard_reset(pd_port))
		pd_dpm_sink_vbus(pd_port, false);
#endif	/* CONFIG_USB_PD_SNK_HRESET_KEEP_DRAW */

	pd_set_rx_enable(pd_port, rx_cap);
	pd_put_pe_event(pd_port, PD_PE_RESET_PRL_COMPLETED);
}

void pe_snk_discovery_entry(struct pd_port *pd_port)
{
	pd_enable_vbus_valid_detection(pd_port, true);
}

void pe_snk_wait_for_capabilities_entry(
				struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_SNK_HRESET_KEEP_DRAW
	/* Default current draw after HardReset */
	if (pd_check_pe_during_hard_reset(pd_port))
		pd_dpm_sink_vbus(pd_port, true);
#endif	/* CONFIG_USB_PD_SNK_HRESET_KEEP_DRAW */

	pd_notify_pe_hard_reset_completed(pd_port);

	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_SEND_WAIT_CAP);
	pd_enable_pe_state_timer(pd_port, PD_TIMER_SINK_WAIT_CAP);
}

void pe_snk_evaluate_capability_entry(struct pd_port *pd_port)
{
	/* Disable UART output for Source SenderResponse */
	pd_lock_msg_output(pd_port);

	pd_handle_hard_reset_recovery(pd_port);
	pd_handle_first_pd_command(pd_port);

	pd_port->pe_data.explicit_contract = false;
	pd_dpm_snk_evaluate_caps(pd_port);
}

void pe_snk_select_capability_entry(struct pd_port *pd_port)
{
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);

	PE_STATE_WAIT_MSG_HRESET_IF_TOUT(pd_port);

	if (pd_event->event_type == PD_EVT_DPM_MSG) {
		PE_DBG("SelectCap%d, rdo:0x%08x\r\n",
			pd_event->msg_sec, pd_port->last_rdo);
	} else {
		/* new request, for debug only */
		/* pd_dpm_sink_vbus(pd_port, false); */
		PE_DBG("NewReq, rdo:0x%08x\r\n", pd_port->last_rdo);
	}

	/* Disable UART output for Sink SenderResponse */
	pd_lock_msg_output(pd_port);

	pd_send_sop_data_msg(pd_port,
		PD_DATA_REQUEST, 1, &pd_port->last_rdo);
}

void pe_snk_select_capability_exit(struct pd_port *pd_port)
{
	if (pd_check_ctrl_msg_event(pd_port, PD_CTRL_ACCEPT)) {
		pd_port->pe_data.remote_selected_cap =
					RDO_POS(pd_port->last_rdo);
		pd_port->cap_miss_match = 0;
	} else if (pd_check_ctrl_msg_event(pd_port, PD_CTRL_REJECT)) {
#ifdef CONFIG_USB_PD_RENEGOTIATION_COUNTER
		if (pd_port->cap_miss_match == 0x01) {
			PE_INFO("reset renegotiation cnt by cap mismatch\r\n");
			pd_port->pe_data.renegotiation_count = 0;
		}
#endif /* CONFIG_USB_PD_RENEGOTIATION_COUNTER */
		pd_port->cap_miss_match |= (1 << 1);
	} else
		pd_port->cap_miss_match = 0;

	/* Waiting for Hard-Reset Done */
	if (!pd_check_timer_msg_event(pd_port, PD_TIMER_SENDER_RESPONSE))
		pd_unlock_msg_output(pd_port);
}

void pe_snk_transition_sink_entry(struct pd_port *pd_port)
{
	pd_enable_pe_state_timer(pd_port, PD_TIMER_PS_TRANSITION);

#ifdef CONFIG_USB_PD_SNK_GOTOMIN
	if (pd_check_ctrl_msg_event(pd_port, PD_CTRL_GOTO_MIN)) {
		if (pd_port->dpm_caps & DPM_CAP_LOCAL_GIVE_BACK)
			pd_port->request_i_new = pd_port->request_i_op;
	}
#endif	/* CONFIG_USB_PD_SNK_GOTOMIN */

	pd_dpm_snk_standby_power(pd_port);
}

void pe_snk_ready_entry(struct pd_port *pd_port)
{
	if (pd_check_ctrl_msg_event(pd_port, PD_CTRL_WAIT))
		pd_enable_timer(pd_port, PD_TIMER_SINK_REQUEST);

	pd_notify_pe_snk_explicit_contract(pd_port);
	pe_power_ready_entry(pd_port);
	pd_port->tcpc_dev->typec_caps.data = TYPEC_PORT_DRD;
	typec_set_pwr_opmode(pd_port->tcpc_dev->typec_port, TYPEC_PWR_MODE_PD);
}

void pe_snk_hard_reset_entry(struct pd_port *pd_port)
{
	pd_send_hard_reset(pd_port);
}

void pe_snk_transition_to_default_entry(struct pd_port *pd_port)
{
	pd_reset_local_hw(pd_port);
	pd_dpm_snk_hard_reset(pd_port);

	/*
	 * Sink PE will wait vSafe0v in this state,
	 * So original exit action be executed in here too.
	 */

	pd_enable_timer(pd_port, PD_TIMER_NO_RESPONSE);
	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_STARTUP);
	pd_enable_vbus_valid_detection(pd_port, false);
}

void pe_snk_give_sink_cap_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_sink_caps(pd_port);
}

void pe_snk_get_source_cap_entry(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	PE_STATE_WAIT_MSG(pd_port);
#else
	PE_STATE_WAIT_TX_SUCCESS(pd_port);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_SOURCE_CAP);
}

void pe_snk_send_soft_reset_entry(struct pd_port *pd_port)
{
	pd_send_soft_reset(pd_port);
}

void pe_snk_soft_reset_entry(struct pd_port *pd_port)
{
	pd_handle_soft_reset(pd_port);
}

/* ---- Policy Engine (PD30) ---- */

#ifdef CONFIG_USB_PD_REV30

/*
 * [PD3.0] Figure 8-71 Sink Port Not Supported Message State Diagram
 */

void pe_snk_send_not_supported_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_NOT_SUPPORTED);
}

void pe_snk_not_supported_received_entry(struct pd_port *pd_port)
{
	PE_STATE_DPM_INFORMED(pd_port);

	pd_dpm_inform_not_support(pd_port);
}

void pe_snk_chunk_received_entry(struct pd_port *pd_port)
{
	pd_enable_timer(pd_port, PD_TIMER_CK_NO_SUPPORT);
}

/*
 * [PD3.0] Figure 8-74 Sink Port Source Alert State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
void pe_snk_source_alert_received_entry(struct pd_port *pd_port)
{
	PE_STATE_DPM_INFORMED(pd_port);

	pd_dpm_inform_alert(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

/*
 * [PD3.0] Figure 8-75 Sink Port Sink Alert State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
void pe_snk_send_sink_alert_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);
	pd_dpm_send_alert(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

/*
 * [PD3.0] Figure 8-77 Sink Port Get Source Capabilities Extended State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
void pe_snk_get_source_cap_ext_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_SOURCE_CAP_EXT);
}

void pe_snk_get_source_cap_ext_exit(struct pd_port *pd_port)
{
	pd_dpm_inform_source_cap_ext(pd_port);
}

#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */

/*
 * [PD3.0] Figure 8-79 Sink Port Get Source Status State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
void pe_snk_get_source_status_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_STATUS);
}

void pe_snk_get_source_status_exit(struct pd_port *pd_port)
{
	pd_dpm_inform_status(pd_port);
}

#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

/*
 * [PD3.0] Figure 8-82 Sink Give Sink Status State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
void pe_snk_give_sink_status_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_status(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */

/*
 * [PD3.0] Figure 8-83 Sink Port Get Source PPS Status State Diagram
 */

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
void pe_snk_get_pps_status_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_PPS_STATUS);
}

void pe_snk_get_pps_status_exit(struct pd_port *pd_port)
{
	pd_dpm_inform_pps_status(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#endif	/* CONFIG_USB_PD_REV30 */
