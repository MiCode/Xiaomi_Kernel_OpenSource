/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_process_evt_snk.c
 * Power Delvery Process Event For SNK
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

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"

/* PD Control MSG reactions */

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_GOOD_CRC) = {
	/* sink */
	{ PE_SNK_GIVE_SINK_CAP, PE_SNK_READY },
	{ PE_SNK_GET_SOURCE_CAP, PE_SNK_READY },

	{ PE_SNK_SOFT_RESET, PE_SNK_WAIT_FOR_CAPABILITIES },

	/* dual */
	{ PE_DR_SNK_GIVE_SOURCE_CAP, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GOOD_CRC);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_GOTO_MIN) = {
	{ PE_SNK_READY, PE_SNK_TRANSITION_SINK },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GOTO_MIN);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_ACCEPT) = {
	{ PE_SNK_SELECT_CAPABILITY, PE_SNK_TRANSITION_SINK },
	{ PE_SNK_SEND_SOFT_RESET, PE_SNK_WAIT_FOR_CAPABILITIES },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_ACCEPT);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_PS_RDY) = {
	{ PE_SNK_TRANSITION_SINK, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_PS_RDY);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_GET_SINK_CAP) = {
	{ PE_SNK_READY, PE_SNK_GIVE_SINK_CAP },

	{ PE_SNK_GET_SOURCE_CAP, PE_SNK_GIVE_SINK_CAP },
	{ PE_DR_SNK_GET_SINK_CAP, PE_SNK_GIVE_SINK_CAP },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GET_SINK_CAP);

/* PD Data MSG reactions */

DECL_PE_STATE_TRANSITION(PD_DATA_MSG_SOURCE_CAP) = {
	{ PE_SNK_WAIT_FOR_CAPABILITIES, PE_SNK_EVALUATE_CAPABILITY },
	{ PE_SNK_READY, PE_SNK_EVALUATE_CAPABILITY },

	/* PR-Swap issue (Check it later) */
	{ PE_SNK_STARTUP, PE_SNK_EVALUATE_CAPABILITY },
	{ PE_SNK_DISCOVERY, PE_SNK_EVALUATE_CAPABILITY },
};
DECL_PE_STATE_REACTION(PD_DATA_MSG_SOURCE_CAP);

DECL_PE_STATE_TRANSITION(PD_DATA_MSG_SINK_CAP) = {
	{ PE_DR_SNK_GET_SINK_CAP, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_DATA_MSG_SINK_CAP);


/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_SNK_EVALUATE_CAPABILITY, PE_SNK_SELECT_CAPABILITY },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_NAK) = {
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_NAK);

/* HW Event reactions */

DECL_PE_STATE_TRANSITION(PD_HW_MSG_VBUS_PRESENT) = {
	{ PE_SNK_DISCOVERY, PE_SNK_WAIT_FOR_CAPABILITIES},
};
DECL_PE_STATE_REACTION(PD_HW_MSG_VBUS_PRESENT);

DECL_PE_STATE_TRANSITION(PD_HW_MSG_VBUS_ABSENT) = {
	{ PE_SNK_STARTUP, PE_SNK_DISCOVERY },
};
DECL_PE_STATE_REACTION(PD_HW_MSG_VBUS_ABSENT);

DECL_PE_STATE_TRANSITION(PD_HW_MSG_TX_FAILED) = {
	{ PE_SNK_SOFT_RESET, PE_SNK_HARD_RESET },
	{ PE_SNK_SEND_SOFT_RESET, PE_SNK_HARD_RESET },
};
DECL_PE_STATE_REACTION(PD_HW_MSG_TX_FAILED);


/* PE Event reactions */

DECL_PE_STATE_TRANSITION(PD_PE_MSG_HARD_RESET_COMPLETED) = {
	{ PE_SNK_HARD_RESET, PE_SNK_TRANSITION_TO_DEFAULT },
};
DECL_PE_STATE_REACTION(PD_PE_MSG_HARD_RESET_COMPLETED);

DECL_PE_STATE_TRANSITION(PD_PE_MSG_RESET_PRL_COMPLETED) = {
	{ PE_SNK_STARTUP, PE_SNK_DISCOVERY },
};
DECL_PE_STATE_REACTION(PD_PE_MSG_RESET_PRL_COMPLETED);

DECL_PE_STATE_TRANSITION(PD_PE_MSG_POWER_ROLE_AT_DEFAULT) = {
	{ PE_SNK_TRANSITION_TO_DEFAULT, PE_SNK_STARTUP },
};
DECL_PE_STATE_REACTION(PD_PE_MSG_POWER_ROLE_AT_DEFAULT);

/* Timer Event reactions */

DECL_PE_STATE_TRANSITION(PD_TIMER_BIST_CONT_MODE) = {
	{ PE_BIST_CARRIER_MODE_2, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_BIST_CONT_MODE);

DECL_PE_STATE_TRANSITION(PD_TIMER_SENDER_RESPONSE) = {
	{ PE_SNK_SELECT_CAPABILITY, PE_SNK_HARD_RESET },
	{ PE_SNK_SEND_SOFT_RESET, PE_SNK_HARD_RESET },

	{ PE_DR_SNK_GET_SINK_CAP, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_SENDER_RESPONSE);

DECL_PE_STATE_TRANSITION(PD_TIMER_SINK_REQUEST) = {
	{ PE_SNK_READY, PE_SNK_SELECT_CAPABILITY },
};
DECL_PE_STATE_REACTION(PD_TIMER_SINK_REQUEST);

/*
 * [BLOCK] Porcess Ctrl MSG
 */

static inline bool pd_process_ctrl_msg_good_crc(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_SNK_SELECT_CAPABILITY:
	case PE_SNK_SEND_SOFT_RESET:
	case PE_DR_SNK_GET_SINK_CAP:
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
		return false;

	default:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GOOD_CRC);
	}
}

static inline bool pd_process_ctrl_msg_get_source_cap(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_SNK_READY:
	case PE_DR_SNK_GET_SINK_CAP:
	case PE_SNK_GET_SOURCE_CAP:
		break;

	default:
		return false;
	}

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
		PE_TRANSIT_STATE(pd_port, PE_DR_SNK_GIVE_SOURCE_CAP);
		return true;
	}
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_REJECT);
	return false;
}

static inline bool pd_process_ctrl_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		return pd_process_ctrl_msg_good_crc(pd_port, pd_event);

	case PD_CTRL_GOTO_MIN:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GOTO_MIN);
		break;

	case PD_CTRL_ACCEPT:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_ACCEPT);
		break;

	case PD_CTRL_PING:
		pd_notify_pe_recv_ping_event(pd_port);
		break;

	case PD_CTRL_PS_RDY:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_PS_RDY);
		break;

	case PD_CTRL_GET_SOURCE_CAP:
		ret = pd_process_ctrl_msg_get_source_cap(pd_port, pd_event);
		break;

	case PD_CTRL_GET_SINK_CAP:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GET_SINK_CAP);
		break;

	case PD_CTRL_REJECT:
		if (pd_port->pe_state_curr == PE_DR_SNK_GET_SINK_CAP) {
			PE_TRANSIT_STATE(pd_port, PE_SNK_READY);
			return true;
		}
		/* no break!*/

	case PD_CTRL_WAIT:
		if (pd_port->pe_state_curr == PE_SNK_SELECT_CAPABILITY) {
			if (pd_port->explicit_contract)
				PE_TRANSIT_STATE(pd_port, PE_SNK_READY);
			else
				PE_TRANSIT_STATE(pd_port,
					PE_SNK_WAIT_FOR_CAPABILITIES);

			return true;
		}
		break;

	/* Swap */
	case PD_CTRL_DR_SWAP:
		ret = pd_process_ctrl_msg_dr_swap(pd_port, pd_event);
		break;

	case PD_CTRL_PR_SWAP:
		ret = pd_process_ctrl_msg_pr_swap(pd_port, pd_event);
		break;

	case PD_CTRL_VCONN_SWAP:
		ret = pd_process_ctrl_msg_vconn_swap(pd_port, pd_event);
		break;

	/* SoftReset */
	case PD_CTRL_SOFT_RESET:
		if (!pd_port->during_swap) {
			PE_TRANSIT_STATE(pd_port, PE_SNK_SOFT_RESET);
			return true;
		}
		break;
	}

	if (!ret)
		ret = pd_process_protocol_error(pd_port, pd_event);
	return ret;
}

/*
 * [BLOCK] Porcess Data MSG
 */

static inline bool pd_process_data_msg(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_DATA_SOURCE_CAP:
		ret = PE_MAKE_STATE_TRANSIT(PD_DATA_MSG_SOURCE_CAP);
		break;

	case PD_DATA_SINK_CAP:
		ret = PE_MAKE_STATE_TRANSIT(PD_DATA_MSG_SINK_CAP);
		break;

	case PD_DATA_BIST:
		ret = pd_process_data_msg_bist(pd_port, pd_event);
		break;

	case PD_DATA_REQUEST:
	case PD_DATA_VENDOR_DEF:
		break;
	}

	if (!ret)
		ret = pd_process_protocol_error(pd_port, pd_event);
	return ret;
}

/*
 * [BLOCK] Porcess DPM MSG
 */

static inline bool pd_process_dpm_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_DPM_ACK:
		ret = PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_ACK);
		break;
	case PD_DPM_NAK:
		ret = PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_NAK);
		break;
	case PD_DPM_ERROR_RECOVERY:
		PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
		return true;
	}

	return ret;
}

/*
 * [BLOCK] Porcess HW MSG
 */

static inline bool pd_process_hw_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_HW_CC_DETACHED:
		PE_TRANSIT_STATE(pd_port, PE_IDLE);
		return true;

	case PD_HW_CC_ATTACHED:
		PE_TRANSIT_STATE(pd_port, PE_SNK_STARTUP);
		return true;

	case PD_HW_RECV_HARD_RESET:
		PE_TRANSIT_STATE(pd_port, PE_SNK_TRANSITION_TO_DEFAULT);
		return true;

	case PD_HW_VBUS_PRESENT:
		ret = PE_MAKE_STATE_TRANSIT(PD_HW_MSG_VBUS_PRESENT);
		break;

	case PD_HW_VBUS_ABSENT:
		ret = PE_MAKE_STATE_TRANSIT(PD_HW_MSG_VBUS_ABSENT);
		break;

	case PD_HW_TX_FAILED:
		ret = PE_MAKE_STATE_TRANSIT_FORCE(
			PD_HW_MSG_TX_FAILED, PE_SNK_SEND_SOFT_RESET);
		break;
	};

	return ret;
}

/*
 * [BLOCK] Porcess PE MSG
 */

static inline bool pd_process_pe_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_PE_RESET_PRL_COMPLETED:
		ret = PE_MAKE_STATE_TRANSIT(PD_PE_MSG_RESET_PRL_COMPLETED);
		break;

	case PD_PE_HARD_RESET_COMPLETED:
		ret = PE_MAKE_STATE_TRANSIT(PD_PE_MSG_HARD_RESET_COMPLETED);
		break;

	case PD_PE_POWER_ROLE_AT_DEFAULT:
		ret = PE_MAKE_STATE_TRANSIT(PD_PE_MSG_POWER_ROLE_AT_DEFAULT);
		break;
	}

	return ret;
}

/*
 * [BLOCK] Porcess Timer MSG
 */

static inline void pd_report_typec_only_charger(pd_port_t *pd_port)
{
	/* TODO: pd_set_rx_enable(pd_port, PD_RX_CAP_PE_DISABLE); */
	PE_INFO("TYPE-C Only Charger\n");
	pd_dpm_sink_vbus(pd_port, true);
	pd_update_connect_state(pd_port, PD_CONNECT_TYPEC_ONLY);
}

static inline bool pd_process_timer_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_TIMER_BIST_CONT_MODE:
		ret = PE_MAKE_STATE_TRANSIT(PD_TIMER_BIST_CONT_MODE);
		break;

	case PD_TIMER_SINK_REQUEST:
		ret = PE_MAKE_STATE_TRANSIT(PD_TIMER_SINK_REQUEST);
		break;

#ifndef CONFIG_USB_PD_DBG_IGRONE_TIMEOUT
	case PD_TIMER_SENDER_RESPONSE:
		ret = PE_MAKE_STATE_TRANSIT(PD_TIMER_SENDER_RESPONSE);
		break;

	case PD_TIMER_SINK_WAIT_CAP:
	case PD_TIMER_PS_TRANSITION:
		if (pd_port->hard_reset_counter <= PD_HARD_RESET_COUNT) {
			PE_TRANSIT_STATE(pd_port, PE_SNK_HARD_RESET);
			return true;
		}
		break;

#ifdef CONFIG_USB_PD_FAST_RESP_TYPEC_SRC
	case PD_TIMER_SRC_RECOVER:
		if (pd_port->pe_state == PE_SNK_STARTUP) {
			pd_disable_timer(pd_port, PD_TIMER_NO_RESPONSE);
			pd_report_typec_only_charger(pd_port);
		}
		break;
#endif /* CONFIG_USB_PD_FAST_RESP_TYPEC_SRC */

	case PD_TIMER_NO_RESPONSE:
		if (!pd_dpm_check_vbus_valid(pd_port)) {
			PE_DBG("NoResp&VBUS=0\r\n");
			PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
			return true;
		}
		if (pd_port->hard_reset_counter <= PD_HARD_RESET_COUNT) {
			PE_TRANSIT_STATE(pd_port, PE_SNK_HARD_RESET);
			return true;
		}
		if (pd_port->pd_prev_connected) {
			PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
			return true;
		}
		pd_report_typec_only_charger(pd_port);
		break;
#endif

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	case PD_TIMER_DISCOVER_ID:
		vdm_put_dpm_discover_cable_event(pd_port);
		break;
#endif
	}

	return ret;
}

/*
 * [BLOCK] Process Policy Engine's SNK Message
 */

bool pd_process_event_snk(pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DATA_MSG:
		return pd_process_data_msg(pd_port, pd_event);

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
