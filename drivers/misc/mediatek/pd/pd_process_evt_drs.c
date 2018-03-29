/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_process_evt_drs.c
 * Power Delvery Process Event For DRS
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

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_ACCEPT) = {
	{ PE_DRS_DFP_UFP_SEND_DR_SWAP, PE_DRS_DFP_UFP_CHANGE_TO_UFP },
	{ PE_DRS_UFP_DFP_SEND_DR_SWAP, PE_DRS_UFP_DFP_CHANGE_TO_DFP },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_ACCEPT);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_REJECT_WAIT) = {
	{ PE_DRS_DFP_UFP_SEND_DR_SWAP, PE_VIRT_READY },
	{ PE_DRS_UFP_DFP_SEND_DR_SWAP, PE_VIRT_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_REJECT_WAIT);

/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_DRS_DFP_UFP_EVALUATE_DR_SWAP, PE_DRS_DFP_UFP_ACCEPT_DR_SWAP },
	{ PE_DRS_UFP_DFP_EVALUATE_DR_SWAP, PE_DRS_UFP_DFP_ACCEPT_DR_SWAP },
	{ PE_DRS_DFP_UFP_CHANGE_TO_UFP, PE_VIRT_READY },
	{ PE_DRS_UFP_DFP_CHANGE_TO_DFP, PE_VIRT_READY },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_NAK) = {
	{ PE_DRS_DFP_UFP_EVALUATE_DR_SWAP, PE_DRS_DFP_UFP_REJECT_DR_SWAP },
	{ PE_DRS_UFP_DFP_EVALUATE_DR_SWAP, PE_DRS_UFP_DFP_REJECT_DR_SWAP },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_NAK);

/* Timer Event reactions */

DECL_PE_STATE_TRANSITION(PD_TIMER_SENDER_RESPONSE) = {
	{ PE_DRS_DFP_UFP_SEND_DR_SWAP, PE_VIRT_READY },
	{ PE_DRS_UFP_DFP_SEND_DR_SWAP, PE_VIRT_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_SENDER_RESPONSE);

/*
 * [BLOCK] Porcess PD Ctrl MSG
 */

static inline bool pd_process_ctrl_msg_good_crc(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_DRS_DFP_UFP_REJECT_DR_SWAP:
	case PE_DRS_UFP_DFP_REJECT_DR_SWAP:
		PE_TRANSIT_READY_STATE(pd_port);
		return true;

	case PE_DRS_DFP_UFP_ACCEPT_DR_SWAP:
		PE_TRANSIT_STATE(pd_port, PE_DRS_DFP_UFP_CHANGE_TO_UFP);
		return true;

	case PE_DRS_UFP_DFP_ACCEPT_DR_SWAP:
		PE_TRANSIT_STATE(pd_port, PE_DRS_UFP_DFP_CHANGE_TO_DFP);
		return true;

	case PE_DRS_DFP_UFP_SEND_DR_SWAP:
	case PE_DRS_UFP_DFP_SEND_DR_SWAP:
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
		return false;

	default:
		return false;
	}
}

static inline bool pd_process_ctrl_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		return pd_process_ctrl_msg_good_crc(pd_port, pd_event);

	case PD_CTRL_ACCEPT:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_ACCEPT);

	case PD_CTRL_WAIT:
	case PD_CTRL_REJECT:
		return PE_MAKE_STATE_TRANSIT_VIRT(PD_CTRL_MSG_REJECT_WAIT);

	default:
		return false;
	}
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
	}

	return ret;
}

/*
 * [BLOCK] Process Policy Engine's DRS Message
 */

bool pd_process_event_drs(pd_port_t *pd_port, pd_event_t *pd_event)
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
