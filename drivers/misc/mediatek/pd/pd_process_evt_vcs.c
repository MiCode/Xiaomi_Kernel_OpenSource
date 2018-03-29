/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_process_evt_vcs.c
 * Power Delvery Process Event For VCS
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

/* PD Control MSG reactions */

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_REJECT_WAIT) = {
	{ PE_VCS_SEND_SWAP, PE_VIRT_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_REJECT_WAIT);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_PS_RDY) = {
	{ PE_VCS_WAIT_FOR_VCONN, PE_VCS_TURN_OFF_VCONN },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_PS_RDY);

/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_VCS_EVALUATE_SWAP, PE_VCS_ACCEPT_SWAP },
	{ PE_VCS_TURN_ON_VCONN, PE_VCS_SEND_PS_RDY },
	{ PE_VCS_TURN_OFF_VCONN, PE_VIRT_READY },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_NAK) = {
	{ PE_VCS_EVALUATE_SWAP, PE_VCS_REJECT_VCONN_SWAP },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_NAK);

/* Timer Event reactions */

DECL_PE_STATE_TRANSITION(PD_TIMER_SENDER_RESPONSE) = {
	{ PE_VCS_SEND_SWAP, PE_VIRT_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_SENDER_RESPONSE);


DECL_PE_STATE_TRANSITION(PD_TIMER_VCONN_ON) = {
	{ PE_VCS_WAIT_FOR_VCONN, PE_VIRT_HARD_RESET},
};
DECL_PE_STATE_REACTION(PD_TIMER_VCONN_ON);

/*
 * [BLOCK] Porcess PD Ctrl MSG
 */

static inline bool pd_process_ctrl_msg_good_crc(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_VCS_REJECT_VCONN_SWAP:
	case PE_VCS_SEND_PS_RDY:
		PE_TRANSIT_READY_STATE(pd_port);
		return true;

	case PE_VCS_ACCEPT_SWAP:
		PE_TRANSIT_VCS_SWAP_STATE(pd_port);
		return true;

	case PE_VCS_SEND_SWAP:
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
		return false;

	default:
		return false;
	}
}

static inline bool pd_process_ctrl_msg_accept(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_port->pe_state_curr == PE_VCS_SEND_SWAP) {
		PE_TRANSIT_VCS_SWAP_STATE(pd_port);
		return true;
	}

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
		return pd_process_ctrl_msg_accept(pd_port, pd_event);

	case PD_CTRL_WAIT:
	case PD_CTRL_REJECT:
		ret = PE_MAKE_STATE_TRANSIT_VIRT(PD_CTRL_MSG_REJECT_WAIT);
		break;

	case PD_CTRL_PS_RDY:
		ret = PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_PS_RDY);
		break;
	}

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
		ret = PE_MAKE_STATE_TRANSIT_VIRT(PD_DPM_MSG_ACK);
		break;
	case PD_DPM_NAK:
		ret = PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_NAK);
		break;
	}

	return ret;
}

/*
 * [BLOCK] Porcess Timer MSG
 */

static inline bool pd_process_timer_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_TIMER_SENDER_RESPONSE:
		ret = PE_MAKE_STATE_TRANSIT_VIRT(PD_TIMER_SENDER_RESPONSE);
		break;

	case PD_TIMER_VCONN_ON:
		ret = PE_MAKE_STATE_TRANSIT_VIRT(PD_TIMER_VCONN_ON);
		break;
	}

	return ret;
}

/*
 * [BLOCK] Process Policy Engine's VCS Message
 */

bool pd_process_event_vcs(pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	case PD_EVT_TIMER_MSG:
		return pd_process_timer_msg(pd_port, pd_event);

	default:
		return false;
	}
}
