// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"

/* PD Control MSG reactions */

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_ACCEPT) = {
	{ PE_SNK_SELECT_CAPABILITY, PE_SNK_TRANSITION_SINK },
	{ PE_SNK_SEND_SOFT_RESET, PE_SNK_WAIT_FOR_CAPABILITIES },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_ACCEPT);

/* PD Data MSG reactions */

DECL_PE_STATE_TRANSITION(PD_DATA_MSG_SOURCE_CAP) = {
	{ PE_SNK_WAIT_FOR_CAPABILITIES, PE_SNK_EVALUATE_CAPABILITY },
	{ PE_SNK_READY, PE_SNK_EVALUATE_CAPABILITY },

	/* PR-Swap issue (Check it later) */
	{ PE_SNK_STARTUP, PE_SNK_EVALUATE_CAPABILITY },
	{ PE_SNK_DISCOVERY, PE_SNK_EVALUATE_CAPABILITY },

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	{ PE_SNK_GET_SOURCE_CAP, PE_SNK_EVALUATE_CAPABILITY },
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
};
DECL_PE_STATE_REACTION(PD_DATA_MSG_SOURCE_CAP);

/*
 * [BLOCK] Porcess Ctrl MSG
 */

static bool pd_process_ctrl_msg_get_source_cap(
		struct pd_port *pd_port, uint8_t next)
{
	if (pd_port->pe_state_curr != PE_SNK_READY)
		return false;

#ifdef CONFIG_USB_PD_PR_SWAP
	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
		PE_TRANSIT_STATE(pd_port, next);
		return true;
	}
#endif	/* CONFIG_USB_PD_PR_SWAP */

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_REJECT);
	return false;
}

static inline bool pd_process_ctrl_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
#ifdef CONFIG_USB_PD_PARTNER_CTRL_MSG_FIRST
	switch (pd_port->pe_state_curr) {
	case PE_SNK_GET_SOURCE_CAP:

#ifdef CONFIG_USB_PD_PR_SWAP
	case PE_DR_SNK_GET_SINK_CAP:
#endif	/* CONFIG_USB_PD_PR_SWAP */
		if (pd_event->msg >= PD_CTRL_GET_SOURCE_CAP &&
			pd_event->msg <= PD_CTRL_VCONN_SWAP) {
			PE_DBG("Port Partner Request First\r\n");
			pd_port->pe_state_curr = PE_SNK_READY;
			pd_disable_timer(
				pd_port, PD_TIMER_SENDER_RESPONSE);
		}
		break;
	}
#endif	/* CONFIG_USB_PD_PARTNER_CTRL_MSG_FIRST */

	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_SOFT_RESET, PE_SNK_WAIT_FOR_CAPABILITIES);

	case PD_CTRL_GOTO_MIN:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_READY, PE_SNK_TRANSITION_SINK))
			return true;
		break;

	case PD_CTRL_ACCEPT:
		if (PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_ACCEPT))
			return true;
		break;

	case PD_CTRL_PS_RDY:
		if (pd_port->pe_state_curr == PE_SNK_TRANSITION_SINK) {
			pd_dpm_snk_transition_power(pd_port);
			PE_TRANSIT_STATE(pd_port, PE_SNK_READY);
			return true;
		}
		break;

	case PD_CTRL_GET_SOURCE_CAP:
		if (pd_process_ctrl_msg_get_source_cap(
			pd_port, PE_DR_SNK_GIVE_SOURCE_CAP))
			return true;
		break;

	case PD_CTRL_GET_SINK_CAP:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_READY, PE_SNK_GIVE_SINK_CAP))
			return true;
		break;

	case PD_CTRL_REJECT:
	case PD_CTRL_WAIT:
		if (pd_port->pe_state_curr == PE_SNK_SELECT_CAPABILITY) {
			if (pd_port->pe_data.explicit_contract)
				PE_TRANSIT_STATE(pd_port, PE_SNK_READY);
			else {
				PE_TRANSIT_STATE(pd_port,
					PE_SNK_WAIT_FOR_CAPABILITIES);
			}
			return true;
		}
		break;

#ifdef CONFIG_USB_PD_REV30
	case PD_CTRL_NOT_SUPPORTED:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_READY, PE_SNK_NOT_SUPPORTED_RECEIVED))
			return true;
		break;

#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	case PD_CTRL_GET_SOURCE_CAP_EXT:
		if (pd_process_ctrl_msg_get_source_cap(
			pd_port, PE_DR_SNK_GIVE_SOURCE_CAP_EXT))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	case PD_CTRL_GET_STATUS:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_READY, PE_SNK_GIVE_SINK_STATUS))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#endif	/* CONFIG_USB_PD_REV30 */

	default:
		pd_port->curr_unsupported_msg = true;
		break;
	}

	return pd_process_protocol_error(pd_port, pd_event);
}

/*
 * [BLOCK] Porcess Data MSG
 */

static inline bool pd_process_data_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_DATA_SOURCE_CAP:
#ifdef CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP
		pd_port->msg_id_pr_swap_last = 0xff;
#endif	/* CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP */
		if (PE_MAKE_STATE_TRANSIT(PD_DATA_MSG_SOURCE_CAP))
			return true;
		break;

#ifdef CONFIG_USB_PD_PR_SWAP
	case PD_DATA_SINK_CAP:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_DR_SNK_GET_SINK_CAP, PE_SNK_READY))
			return true;
		break;
#endif	/* CONFIG_USB_PD_PR_SWAP */
		/* fall-through */
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
	case PD_DATA_ALERT:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_READY, PE_SNK_SOURCE_ALERT_RECEIVED))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */
	default:
		pd_port->curr_unsupported_msg = true;
		break;
	}

	return pd_process_protocol_error(pd_port, pd_event);
}

/*
 * [BLOCK] Porcess Extend MSG
 */
#ifdef CONFIG_USB_PD_REV30
static inline bool pd_process_ext_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {

#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
	case PD_EXT_SOURCE_CAP_EXT:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_GET_SOURCE_CAP_EXT, PE_SNK_READY))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
	case PD_EXT_STATUS:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_GET_SOURCE_STATUS, PE_SNK_READY))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	case PD_EXT_PPS_STATUS:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_GET_PPS_STATUS, PE_SNK_READY))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	default:
		pd_port->curr_unsupported_msg = true;
		break;
	}

	return pd_process_protocol_error(pd_port, pd_event);
}
#endif	/* CONFIG_USB_PD_REV30 */

/*
 * [BLOCK] Porcess DPM MSG
 */

static inline bool pd_process_dpm_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_DPM_ACK:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_EVALUATE_CAPABILITY, PE_SNK_SELECT_CAPABILITY);

	default:
		return false;
	}
}

/*
 * [BLOCK] Porcess HW MSG
 */

static inline bool pd_process_hw_msg_sink_tx_change(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	uint8_t pd_traffic;

	if (!pd_check_rev30(pd_port))
		return false;

	pd_traffic = pd_event->msg_sec ?
		PD_SINK_TX_OK : PD_SINK_TX_NG;

	if (pd_port->pe_data.pd_traffic_control == pd_traffic)
		return false;

	dpm_reaction_set_ready_once(pd_port);
	pd_port->pe_data.pd_traffic_control = pd_traffic;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	return false;
}

static inline bool pd_process_hw_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_HW_VBUS_PRESENT:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_DISCOVERY, PE_SNK_WAIT_FOR_CAPABILITIES);

	case PD_HW_VBUS_ABSENT:
		if (pd_port->pe_state_curr == PE_SNK_TRANSITION_TO_DEFAULT)
			pd_put_pe_event(pd_port, PD_PE_POWER_ROLE_AT_DEFAULT);
		return false;

	case PD_HW_TX_FAILED:
		return pd_process_tx_failed(pd_port);

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	case PD_HW_SINK_TX_CHANGE:
		return pd_process_hw_msg_sink_tx_change(pd_port, pd_event);
#endif /* CONFIG_USB_PD_REV30_COLLISION_AVOID */
	};

	return false;
}

/*
 * [BLOCK] Porcess PE MSG
 */

static inline bool pd_process_pe_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_PE_RESET_PRL_COMPLETED:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_STARTUP, PE_SNK_DISCOVERY);

	case PD_PE_HARD_RESET_COMPLETED:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_HARD_RESET, PE_SNK_TRANSITION_TO_DEFAULT);

	case PD_PE_POWER_ROLE_AT_DEFAULT:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_TRANSITION_TO_DEFAULT, PE_SNK_STARTUP);

	default:
		return false;
	}

}

/*
 * [BLOCK] Porcess Timer MSG
 */

static inline void pd_report_typec_only_charger(struct pd_port *pd_port)
{
	uint8_t state;
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	if (tcpc_dev->typec_remote_rp_level == TYPEC_CC_VOLT_SNK_DFT)
		state = PD_CONNECT_TYPEC_ONLY_SNK_DFT;
	else
		state = PD_CONNECT_TYPEC_ONLY_SNK;

	PE_INFO("TYPE-C Only Charger!\r\n");
	pd_dpm_sink_vbus(pd_port, true);
	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_IDLE);
	pd_notify_pe_hard_reset_completed(pd_port);
	pd_update_connect_state(pd_port, state);
}

static inline bool pd_process_timer_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_TIMER_SINK_REQUEST:
		return PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_READY, PE_SNK_SELECT_CAPABILITY);

#ifndef CONFIG_USB_PD_DBG_IGRONE_TIMEOUT
	case PD_TIMER_SINK_WAIT_CAP:
	case PD_TIMER_PS_TRANSITION:
		if (pd_port->pe_data.hard_reset_counter <=
						PD_HARD_RESET_COUNT) {
			PE_TRANSIT_STATE(pd_port, PE_SNK_HARD_RESET);
			return true;
		}
		break;

	case PD_TIMER_NO_RESPONSE:
		if (!pd_dpm_check_vbus_valid(pd_port)) {
			PE_DBG("NoResp&VBUS=0\r\n");
			PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
			return true;
		} else if (pd_port->pe_data.hard_reset_counter <=
						PD_HARD_RESET_COUNT) {
			PE_TRANSIT_STATE(pd_port, PE_SNK_HARD_RESET);
			return true;
		} else if (pd_port->pe_data.pd_prev_connected) {
			PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
			return true;
		}
		pd_report_typec_only_charger(pd_port);
		break;
#endif	/* CONFIG_USB_PD_DBG_IGRONE_TIMEOUT */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	case PD_TIMER_DISCOVER_ID:
		vdm_put_dpm_discover_cable_event(pd_port);
		break;
#endif	/* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */

#ifdef CONFIG_USB_PD_REV30
	case PD_TIMER_CK_NO_SUPPORT:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_SNK_CHUNK_RECEIVED, PE_SNK_SEND_NOT_SUPPORTED))
			return true;
		/* fall through */
#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
	case PD_TIMER_SNK_FLOW_DELAY:
		if (pd_port->pe_data.pd_traffic_control == PD_SINK_TX_START) {
			pd_port->pe_data.pd_traffic_control = PD_SINK_TX_OK;
			dpm_reaction_set_ready_once(pd_port);
		}
		break;
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30 */
	}

	return false;
}

/*
 * [BLOCK] Process Policy Engine's SNK Message
 */

bool pd_process_event_snk(struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DATA_MSG:
		return pd_process_data_msg(pd_port, pd_event);

#ifdef CONFIG_USB_PD_REV30
	case PD_EVT_EXT_MSG:
		return pd_process_ext_msg(pd_port, pd_event);
#endif	/* CONFIG_USB_PD_REV30 */

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	case PD_EVT_HW_MSG:
		return pd_process_hw_msg(pd_port, pd_event);

	case PD_EVT_PE_MSG:
		return pd_process_pe_msg(pd_port, pd_event);

	case PD_EVT_TIMER_MSG:
		return pd_process_timer_msg(pd_port, pd_event);

	default:
		return false;
	}
}
