/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Power Delivery Process Event For VCS
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
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"

#ifdef CONFIG_USB_PD_VCONN_SWAP
/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_VCS_EVALUATE_SWAP, PE_VCS_ACCEPT_SWAP },
	{ PE_VCS_TURN_ON_VCONN, PE_VCS_SEND_PS_RDY },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

/*
 * [BLOCK] Process PD Ctrl MSG
 */

static inline bool pd_process_ctrl_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	uint8_t vconn_state = pd_port->vconn_role ?
		PE_VCS_WAIT_FOR_VCONN : PE_VCS_TURN_ON_VCONN;

	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_VCS_ACCEPT_SWAP, vconn_state))
			return true;
		break;

	case PD_CTRL_ACCEPT:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_VCS_SEND_SWAP, vconn_state))
			return true;
		break;

	case PD_CTRL_PS_RDY:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_VCS_WAIT_FOR_VCONN, PE_VCS_TURN_OFF_VCONN))
			return true;
		break;
	}

	return false;
}

/*
 * [BLOCK] Process DPM MSG
 */

static inline bool pd_process_dpm_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_DPM_ACK:
		return PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_ACK);

	case PD_DPM_NAK:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_VCS_EVALUATE_SWAP, PE_VCS_REJECT_VCONN_SWAP))
			return true;
		break;
	}

	return false;
}

/*
 * [BLOCK] Process Timer MSG
 */

static inline bool pd_process_timer_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_TIMER_VCONN_ON:
		if (PE_MAKE_STATE_TRANSIT_TO_HRESET(PE_VCS_WAIT_FOR_VCONN))
			return true;
		break;

#if CONFIG_USB_PD_VCONN_READY_TOUT != 0
	case PD_TIMER_VCONN_READY:
		PE_STATE_DPM_ACK_IMMEDIATELY(pd_port);
		break;
#endif
	}

	return false;
}

/*
 * [BLOCK] Process Policy Engine's VCS Message
 */

bool pd_process_event_vcs(struct pd_port *pd_port, struct pd_event *pd_event)
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
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
