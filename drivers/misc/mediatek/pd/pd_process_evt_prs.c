/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_process_evt_prs.c
 * Power Delvery Process Event For PRS
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
	{ PE_PRS_SRC_SNK_ACCEPT_PR_SWAP, PE_PRS_SRC_SNK_TRANSITION_TO_OFF },
	{ PE_PRS_SRC_SNK_REJECT_PR_SWAP, PE_SRC_READY },

	{ PE_PRS_SNK_SRC_ACCEPT_PR_SWAP, PE_PRS_SNK_SRC_TRANSITION_TO_OFF },
	{ PE_PRS_SNK_SRC_REJECT_SWAP, PE_SNK_READY },

	/* VBUS-ON & PS_RDY SENT */
	{ PE_PRS_SNK_SRC_SOURCE_ON, PE_SRC_STARTUP },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_GOOD_CRC);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_ACCEPT) = {
	{ PE_PRS_SRC_SNK_SEND_SWAP, PE_PRS_SRC_SNK_TRANSITION_TO_OFF },
	{ PE_PRS_SNK_SRC_SEND_SWAP, PE_PRS_SNK_SRC_TRANSITION_TO_OFF },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_ACCEPT);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_REJECT_WAIT) = {
	{ PE_PRS_SRC_SNK_SEND_SWAP, PE_SRC_READY },
	{ PE_PRS_SNK_SRC_SEND_SWAP, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_REJECT_WAIT);

DECL_PE_STATE_TRANSITION(PD_CTRL_MSG_PS_RDY) = {
	{ PE_PRS_SRC_SNK_WAIT_SOURCE_ON, PE_SNK_STARTUP },
	{ PE_PRS_SNK_SRC_TRANSITION_TO_OFF, PE_PRS_SNK_SRC_ASSERT_RP },
};
DECL_PE_STATE_REACTION(PD_CTRL_MSG_PS_RDY);

/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_PRS_SRC_SNK_EVALUATE_PR_SWAP, PE_PRS_SRC_SNK_ACCEPT_PR_SWAP },
	{ PE_PRS_SNK_SRC_EVALUATE_PR_SWAP, PE_PRS_SNK_SRC_ACCEPT_PR_SWAP },

	{ PE_PRS_SRC_SNK_ASSERT_RD, PE_PRS_SRC_SNK_WAIT_SOURCE_ON },
	{ PE_PRS_SNK_SRC_ASSERT_RP, PE_PRS_SNK_SRC_SOURCE_ON },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_NAK) = {
	{ PE_PRS_SRC_SNK_EVALUATE_PR_SWAP, PE_PRS_SRC_SNK_REJECT_PR_SWAP },
	{ PE_PRS_SNK_SRC_EVALUATE_PR_SWAP, PE_PRS_SNK_SRC_REJECT_SWAP },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_NAK);

/* HW Event reactions */

DECL_PE_STATE_TRANSITION(PD_HW_TX_FAILED) = {
#ifdef CONFIG_USB_PD_PR_SWAP_ERROR_RECOVERY
	{ PE_PRS_SRC_SNK_WAIT_SOURCE_ON, PE_ERROR_RECOVERY },
	{ PE_PRS_SNK_SRC_SOURCE_ON, PE_ERROR_RECOVERY },
#else
	{ PE_PRS_SRC_SNK_WAIT_SOURCE_ON, PE_SNK_HARD_RESET },
	{ PE_PRS_SNK_SRC_SOURCE_ON, PE_SRC_HARD_RESET },
#endif
};
DECL_PE_STATE_REACTION(PD_HW_TX_FAILED);

DECL_PE_STATE_TRANSITION(PD_HW_VBUS_SAFE0V) = {
	{ PE_PRS_SRC_SNK_TRANSITION_TO_OFF, PE_PRS_SRC_SNK_ASSERT_RD },
};
DECL_PE_STATE_REACTION(PD_HW_VBUS_SAFE0V);


/* Timer Event reactions */

DECL_PE_STATE_TRANSITION(PD_TIMER_SENDER_RESPONSE) = {
	{ PE_PRS_SRC_SNK_SEND_SWAP, PE_SRC_READY },
	{ PE_PRS_SNK_SRC_SEND_SWAP, PE_SNK_READY },
};
DECL_PE_STATE_REACTION(PD_TIMER_SENDER_RESPONSE);

DECL_PE_STATE_TRANSITION(PD_TIMER_PS_SOURCE_ON) = {
#ifdef CONFIG_USB_PD_PR_SWAP_ERROR_RECOVERY
	{ PE_PRS_SRC_SNK_WAIT_SOURCE_ON, PE_ERROR_RECOVERY },
#else
	{ PE_PRS_SRC_SNK_WAIT_SOURCE_ON, PE_SNK_HARD_RESET },
#endif
};
DECL_PE_STATE_REACTION(PD_TIMER_PS_SOURCE_ON);

DECL_PE_STATE_TRANSITION(PD_TIMER_PS_SOURCE_OFF) = {
#ifdef CONFIG_USB_PD_PR_SWAP_ERROR_RECOVERY
	{ PE_PRS_SNK_SRC_TRANSITION_TO_OFF, PE_ERROR_RECOVERY },
#else
	{ PE_PRS_SNK_SRC_TRANSITION_TO_OFF, PE_SNK_HARD_RESET },
#endif
};
DECL_PE_STATE_REACTION(PD_TIMER_PS_SOURCE_OFF);


/*
 * [BLOCK] Porcess PD Ctrl MSG
 */

static inline bool pd_process_ctrl_msg_good_crc(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_PRS_SRC_SNK_WAIT_SOURCE_ON:
		pd_enable_timer(pd_port, PD_TIMER_PS_SOURCE_ON);
		pd_unlock_msg_output(pd_port);	/* for tSRCTransition */
		return false;

	case PE_PRS_SRC_SNK_SEND_SWAP:
	case PE_PRS_SNK_SRC_SEND_SWAP:
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
		return false;

	default:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_GOOD_CRC);
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
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_REJECT_WAIT);

	case PD_CTRL_PS_RDY:
		return PE_MAKE_STATE_TRANSIT(PD_CTRL_MSG_PS_RDY);

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
 * [BLOCK] Porcess HW MSG
 */

static inline bool pd_process_hw_msg_vbus_present(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_port->pe_state_curr == PE_PRS_SNK_SRC_SOURCE_ON)
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_PS_RDY);

	return false;
}

static inline bool pd_process_hw_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->msg) {

	case PD_HW_VBUS_PRESENT:
		return pd_process_hw_msg_vbus_present(pd_port, pd_event);

	case PD_HW_TX_FAILED:
		return PE_MAKE_STATE_TRANSIT(PD_HW_TX_FAILED);

	case PD_HW_VBUS_SAFE0V:
		return PE_MAKE_STATE_TRANSIT(PD_HW_VBUS_SAFE0V);

	default:
		return false;
	}
}

/*
 * [BLOCK] Porcess Timer MSG
 */

static inline bool pd_process_timer_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->msg) {
	case PD_TIMER_SENDER_RESPONSE:
		return PE_MAKE_STATE_TRANSIT(PD_TIMER_SENDER_RESPONSE);

	case PD_TIMER_PS_SOURCE_ON:
		return PE_MAKE_STATE_TRANSIT(PD_TIMER_PS_SOURCE_ON);

	case PD_TIMER_PS_SOURCE_OFF:
		return PE_MAKE_STATE_TRANSIT(PD_TIMER_PS_SOURCE_OFF);

	case PD_TIMER_SOURCE_TRANSITION:
		pd_dpm_prs_enable_power_source(pd_port, false);
		return false;
	default:
		return false;
	}
}

/*
 * [BLOCK] Process Policy Engine's PRS Message
 */

bool pd_process_event_prs(pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	case PD_EVT_HW_MSG:
		return pd_process_hw_msg(pd_port, pd_event);

	case PD_EVT_TIMER_MSG:
		return pd_process_timer_msg(pd_port, pd_event);

	default:
		return false;
	}
}
