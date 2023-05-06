/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Power Delivery Process Event For DRS
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

#include <linux/usb/tcpc/pd_core.h>
#include <linux/usb/tcpc/tcpci_event.h>
#include <linux/usb/tcpc/pd_process_evt.h>

/* PD Control MSG reactions */

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_GOOD_CRC) = {
	{ PE_DRS_DFP_UFP_ACCEPT_DR_SWAP, PE_DRS_DFP_UFP_CHANGE_TO_UFP },
	{ PE_DRS_UFP_DFP_ACCEPT_DR_SWAP, PE_DRS_UFP_DFP_CHANGE_TO_DFP },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GOOD_CRC);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_ACCEPT) = {
	{ PE_DRS_DFP_UFP_SEND_DR_SWAP, PE_DRS_DFP_UFP_CHANGE_TO_UFP },
	{ PE_DRS_UFP_DFP_SEND_DR_SWAP, PE_DRS_UFP_DFP_CHANGE_TO_DFP },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_ACCEPT);

/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_DRS_DFP_UFP_EVALUATE_DR_SWAP, PE_DRS_DFP_UFP_ACCEPT_DR_SWAP },
	{ PE_DRS_UFP_DFP_EVALUATE_DR_SWAP, PE_DRS_UFP_DFP_ACCEPT_DR_SWAP },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_NAK) = {
	{ PE_DRS_DFP_UFP_EVALUATE_DR_SWAP, PE_DRS_DFP_UFP_REJECT_DR_SWAP },
	{ PE_DRS_UFP_DFP_EVALUATE_DR_SWAP, PE_DRS_UFP_DFP_REJECT_DR_SWAP },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_NAK);


/*
 * [BLOCK] Porcess PD Ctrl MSG
 */

static inline bool pd_process_ctrl_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GOOD_CRC);

	case PD_CTRL_ACCEPT:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_ACCEPT);

	default:
		return false;
	}
}

/*
 * [BLOCK] Porcess DPM MSG
 */

static inline bool pd_process_dpm_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_DPM_ACK:
		return PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_ACK);

	case PD_DPM_NAK:
		return PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_NAK);
	}

	return false;
}

/*
 * [BLOCK] Process Policy Engine's DRS Message
 */

bool pd_process_event_drs(struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	default:
		return false;
	}
}
