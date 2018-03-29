/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_process_evt_src.c
 * Power Delvery Process Event For SRC
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
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"
#include "inc/pd_dpm_core.h"

/* PD Control MSG reactions */

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_GOOD_CRC) = {
	{ PE_SRC_GIVE_SOURCE_CAP, PE_SRC_READY },
	{ PE_SRC_SOFT_RESET, PE_SRC_SEND_CAPABILITIES },

	{ PE_DR_SRC_GIVE_SINK_CAP, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GOOD_CRC);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_GET_SOURCE_CAP) = {
	{ PE_SRC_READY, PE_SRC_GIVE_SOURCE_CAP },

/* Handler Port Partner Request first  */
	{ PE_DR_SRC_GET_SOURCE_CAP, PE_SRC_GIVE_SOURCE_CAP},
	{ PE_SRC_GET_SINK_CAP, PE_SRC_GIVE_SOURCE_CAP},
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GET_SOURCE_CAP);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_ACCEPT) = {
	{PE_SRC_SEND_SOFT_RESET, PE_SRC_SEND_CAPABILITIES },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_ACCEPT);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_REJECT) = {
	{ PE_DR_SRC_GET_SOURCE_CAP, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_REJECT);

/* PD Data MSG reactions */

DECL_PE_STATE_TRANSITION(PD_DATA_MSG_REQUEST) = {
	{ PE_SRC_SEND_CAPABILITIES, PE_SRC_NEGOTIATE_CAPABILITIES },
	{ PE_SRC_READY, PE_SRC_NEGOTIATE_CAPABILITIES },

/* Handler Port Partner Request first */
	{ PE_DR_SRC_GET_SOURCE_CAP, PE_SRC_GIVE_SOURCE_CAP},
	{ PE_SRC_GET_SINK_CAP, PE_SRC_GIVE_SOURCE_CAP},
};
DECL_PE_STATE_REACTION(PD_DATA_MSG_REQUEST);

DECL_PE_STATE_TRANSITION(PD_DATA_MSG_SOURCE_CAP) = {
	{ PE_DR_SRC_GET_SOURCE_CAP, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_DATA_MSG_SOURCE_CAP);

DECL_PE_STATE_TRANSITION(PD_DATA_MSG_SINK_CAP) = {
	{ PE_SRC_GET_SINK_CAP, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_DATA_MSG_SINK_CAP);

/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(pd_dpm_msg_ack) = {
	{ PE_SRC_NEGOTIATE_CAPABILITIES, PE_SRC_TRANSITION_SUPPLY },

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	{ PE_SRC_STARTUP, PE_SRC_SEND_CAPABILITIES },
#endif	/*  CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID */
};
DECL_PE_STATE_REACTION(pd_dpm_msg_ack);

DECL_PE_STATE_TRANSITION(pd_dpm_msg_nak) = {
	{ PE_SRC_NEGOTIATE_CAPABILITIES, PE_SRC_CAPABILITY_RESPONSE },
};
DECL_PE_STATE_REACTION(pd_dpm_msg_nak);

DECL_PE_STATE_TRANSITION(pd_dpm_msg_cap_changed) = {
	{ PE_SRC_READY, PE_SRC_SEND_CAPABILITIES },
	{ PE_SRC_WAIT_NEW_CAPABILITIES, PE_SRC_SEND_CAPABILITIES },
};
DECL_PE_STATE_REACTION(pd_dpm_msg_cap_changed);

/* HW Event reactions */

DECL_PE_STATE_TRANSITION(PD_HW_MSG_TX_FAILED) = {
	{ PE_SRC_SOFT_RESET, PE_SRC_HARD_RESET },
	{ PE_SRC_SEND_SOFT_RESET, PE_SRC_HARD_RESET },
};
DECL_PE_STATE_REACTION(PD_HW_MSG_TX_FAILED);

DECL_PE_STATE_TRANSITION(PD_HW_VBUS_STABLE) = {
	{ PE_SRC_TRANSITION_SUPPLY, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_HW_VBUS_STABLE);

/* PE Event reactions */

/* TODO: Remove it later, always trigger by pd_evt_source_start_timeout */
DECL_PE_STATE_TRANSITION(pd_pe_msg_reset_prl_completed) = {
	{ PE_SRC_STARTUP, PE_SRC_SEND_CAPABILITIES },
};
DECL_PE_STATE_REACTION(pd_pe_msg_reset_prl_completed);

DECL_PE_STATE_TRANSITION(pd_pe_msg_power_role_at_default) = {
	{ PE_SRC_TRANSITION_TO_DEFAULT, PE_SRC_STARTUP },
};
DECL_PE_STATE_REACTION(pd_pe_msg_power_role_at_default);

/* Timer Event reactions */

DECL_PE_STATE_TRANSITION(PD_TIMER_SENDER_RESPONSE) = {
	{ PE_SRC_SEND_CAPABILITIES, PE_SRC_HARD_RESET },
	{ PE_SRC_SEND_SOFT_RESET, PE_SRC_HARD_RESET },

	{ PE_SRC_GET_SINK_CAP, PE_SRC_READY },
	{ PE_DR_SRC_GET_SOURCE_CAP, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_SENDER_RESPONSE);

DECL_PE_STATE_TRANSITION(PD_TIMER_PS_HARD_RESET) = {
	{ PE_SRC_HARD_RESET, PE_SRC_TRANSITION_TO_DEFAULT },
	{ PE_SRC_HARD_RESET_RECEIVED, PE_SRC_TRANSITION_TO_DEFAULT },
};
DECL_PE_STATE_REACTION(PD_TIMER_PS_HARD_RESET);

DECL_PE_STATE_TRANSITION(PD_TIMER_BIST_CONT_MODE) = {
	{ PE_BIST_CARRIER_MODE_2, PE_SRC_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_BIST_CONT_MODE);

DECL_PE_STATE_TRANSITION(PD_TIMER_SOURCE_START) = {
	{ PE_SRC_STARTUP, PE_SRC_SEND_CAPABILITIES },
};
DECL_PE_STATE_REACTION(PD_TIMER_SOURCE_START);



/*
 * [BLOCK] Porcess Ctrl MSG
 */

static inline bool pd_process_ctrl_msg_good_crc(
	pd_port_t *pd_port, pd_event_t *pd_event)

{
	switch (pd_port->pe_state_curr) {
	case PE_SRC_SEND_SOFT_RESET:
	case PE_SRC_GET_SINK_CAP:
	case PE_DR_SRC_GET_SOURCE_CAP:
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
		return false;

	case PE_SRC_SEND_CAPABILITIES:
		pd_disable_timer(pd_port, PD_TIMER_NO_RESPONSE);
		pd_port->cap_counter = 0;
		pd_port->hard_reset_counter = 0;
		pd_notify_pe_hard_reset_completed(pd_port);
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
		/* pd_set_cc_res(pd_port, TYPEC_CC_RP_1_5); */
		return false;

	case PE_SRC_CAPABILITY_RESPONSE:
		if (!pd_port->explicit_contract)
			PE_TRANSIT_STATE(pd_port, PE_SRC_WAIT_NEW_CAPABILITIES);
		else if (pd_port->invalid_contract)
			PE_TRANSIT_STATE(pd_port, PE_SRC_HARD_RESET);
		else
			PE_TRANSIT_STATE(pd_port, PE_SRC_READY);
		return true;
	default:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GOOD_CRC);
	}
}

static inline bool pd_process_ctrl_msg_get_sink_cap(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_SRC_READY:
	case PE_DR_SRC_GET_SOURCE_CAP:
	case PE_SRC_GET_SINK_CAP:
		break;

	default:
		return false;
	}

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
		PE_TRANSIT_STATE(pd_port, PE_DR_SRC_GIVE_SINK_CAP);
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


	case PD_CTRL_ACCEPT:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_ACCEPT);
		break;

	case PD_CTRL_REJECT:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_REJECT);
		break;

	case PD_CTRL_GET_SOURCE_CAP:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GET_SOURCE_CAP);
		break;

	case PD_CTRL_GET_SINK_CAP:
		ret = pd_process_ctrl_msg_get_sink_cap(pd_port, pd_event);
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
			PE_TRANSIT_STATE(pd_port, PE_SRC_SOFT_RESET);
			return true;
		}
		break;

	/* Ignore */
	case PD_CTRL_PING:
		pd_notify_pe_recv_ping_event(pd_port);
	case PD_CTRL_PS_RDY:
	case PD_CTRL_GOTO_MIN:
	case PD_CTRL_WAIT:
		break;
	}

	if (ret == false)
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
		ret = PE_MAKE_STATE_TRANSIT(PD_DATA_MSG_REQUEST);
		break;

	case PD_DATA_VENDOR_DEF:
		return false;
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
		ret = PE_MAKE_STATE_TRANSIT(pd_dpm_msg_ack);
		break;
	case PD_DPM_NAK:
		ret = PE_MAKE_STATE_TRANSIT(pd_dpm_msg_nak);
		break;
	case PD_DPM_CAP_CHANGED:
		ret = PE_MAKE_STATE_TRANSIT(pd_dpm_msg_cap_changed);
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

static inline bool pd_process_hw_msg_vbus_present(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_SRC_STARTUP:
		pd_enable_timer(pd_port, PD_TIMER_SOURCE_START);
		break;

	case PE_SRC_TRANSITION_TO_DEFAULT:
		pd_put_pe_event(pd_port, PD_PE_POWER_ROLE_AT_DEFAULT);
		break;
	}

	return false;
}

static inline bool pd_process_hw_msg_tx_failed(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_port->pe_state_curr == PE_SRC_SEND_CAPABILITIES) {
		if (pd_port->pd_connected) {
			PE_DBG("PR_SWAP NoResp\r\n");
			return false;
		}

		PE_TRANSIT_STATE(pd_port, PE_SRC_DISCOVERY);
		return true;
	}

	return PE_MAKE_STATE_TRANSIT_FORCE(
		PD_HW_MSG_TX_FAILED, PE_SRC_SEND_SOFT_RESET);
}


static inline bool pd_process_hw_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_HW_CC_DETACHED:
		PE_TRANSIT_STATE(pd_port, PE_IDLE);
		return true;

	case PD_HW_CC_ATTACHED:
		PE_TRANSIT_STATE(pd_port, PE_SRC_STARTUP);
		return true;

	case PD_HW_RECV_HARD_RESET:
		PE_TRANSIT_STATE(pd_port, PE_SRC_HARD_RESET_RECEIVED);
		return true;

	case PD_HW_VBUS_PRESENT:
		ret = pd_process_hw_msg_vbus_present(pd_port, pd_event);
		break;

	case PD_HW_VBUS_SAFE0V:
		pd_enable_timer(pd_port, PD_TIMER_SRC_RECOVER);
		break;

	case PD_HW_VBUS_STABLE:
		ret = PE_MAKE_STATE_TRANSIT(PD_HW_VBUS_STABLE);
		break;

	case PD_HW_TX_FAILED:
		ret = pd_process_hw_msg_tx_failed(pd_port, pd_event);
		break;

	case PD_HW_VBUS_ABSENT:
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
		ret = PE_MAKE_STATE_TRANSIT(pd_pe_msg_reset_prl_completed);
		break;

	case PD_PE_POWER_ROLE_AT_DEFAULT:
		ret = PE_MAKE_STATE_TRANSIT(pd_pe_msg_power_role_at_default);
		break;
	}

	return ret;
}

/*
 * [BLOCK] Porcess Timer MSG
 */
static inline bool pd_process_timer_msg_source_start(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	if (pd_is_auto_discover_cable_id(pd_port)) {
		if (vdm_put_dpm_discover_cable_event(pd_port)) {
			/* waiting for dpm_ack event */
			return false;
		}
	}
#endif

	return PE_MAKE_STATE_TRANSIT(PD_TIMER_SOURCE_START);
}

static inline bool pd_process_timer_msg_source_cap(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_port->pe_state_curr != PE_SRC_DISCOVERY)
		return false;

	if (pd_port->cap_counter <= PD_CAPS_COUNT)
		PE_TRANSIT_STATE(pd_port, PE_SRC_SEND_CAPABILITIES);
	else	/* in this state, PD always not connected */
		PE_TRANSIT_STATE(pd_port, PE_SRC_DISABLED);

	return true;
}

static inline bool pd_process_timer_msg_no_response(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_port->hard_reset_counter <= PD_HARD_RESET_COUNT)
		PE_TRANSIT_STATE(pd_port, PE_SRC_HARD_RESET);
	else if (pd_port->pd_prev_connected)
		PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
	else
		PE_TRANSIT_STATE(pd_port, PE_SRC_DISABLED);

	return true;
}

static inline bool pd_process_timer_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->msg) {
	case PD_TIMER_BIST_CONT_MODE:
		return PE_MAKE_STATE_TRANSIT(PD_TIMER_BIST_CONT_MODE);

	case PD_TIMER_SOURCE_CAPABILITY:
		return pd_process_timer_msg_source_cap(pd_port, pd_event);

#ifndef CONFIG_USB_PD_DBG_IGRONE_TIMEOUT
	case PD_TIMER_SENDER_RESPONSE:
		return PE_MAKE_STATE_TRANSIT(PD_TIMER_SENDER_RESPONSE);
#endif

	case PD_TIMER_PS_HARD_RESET:
		return PE_MAKE_STATE_TRANSIT(PD_TIMER_PS_HARD_RESET);

	case PD_TIMER_SOURCE_START:
		return pd_process_timer_msg_source_start(pd_port, pd_event);

#ifndef CONFIG_USB_PD_DBG_IGRONE_TIMEOUT
	case PD_TIMER_NO_RESPONSE:
		return pd_process_timer_msg_no_response(pd_port, pd_event);
#endif

	case PD_TIMER_SOURCE_TRANSITION:
		if (pd_port->state_machine != PE_STATE_MACHINE_PR_SWAP)
			pd_dpm_src_transition_power(pd_port, pd_event);
		break;

#ifdef CONFIG_PD_DISCOVER_CABLE_ID
	case PD_TIMER_DISCOVER_ID:
		vdm_put_dpm_discover_cable_event(pd_port);
		break;
#endif

	case PD_TIMER_SRC_RECOVER:
		pd_dpm_source_vbus(pd_port, true);
		pd_enable_vbus_valid_detection(pd_port, true);
		break;
	}

	return false;
}

/*
 * [BLOCK] Process Policy Engine's SRC Message
 */

bool pd_process_event_src(pd_port_t *pd_port, pd_event_t *pd_event)
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
