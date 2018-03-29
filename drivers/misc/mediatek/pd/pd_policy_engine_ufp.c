/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_policy_engine.c
 * Power Delvery Core Driver
 * Power Delvery Policy Engine for UFP
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
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-58 UFP Structured VDM Discover Identity State Diagram
 */

void pe_ufp_vdm_get_identity_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_id_info(pd_port, pd_event);
}

void pe_ufp_vdm_send_identity_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_response_id(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_ufp_vdm_get_identity_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_NAK);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-59 UFP Structured VDM Discover SVIDs State Diagram
 */

void pe_ufp_vdm_get_svids_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_svid_info(pd_port, pd_event);
}

void pe_ufp_vdm_send_svids_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_response_svids(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_ufp_vdm_get_svids_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_NAK);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-60 UFP Structured VDM Discover Modes State Diagram
 */

void pe_ufp_vdm_get_modes_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_mode_info(pd_port, pd_event);
}

void pe_ufp_vdm_send_modes_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_response_modes(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_ufp_vdm_get_modes_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_NAK);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-61 UFP Structured VDM Enter Mode State Diagram
 */

void pe_ufp_vdm_evaluate_mode_entry_entry(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_enter_mode(pd_port, pd_event);
}

void pe_ufp_vdm_mode_entry_ack_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_ACK);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_ufp_vdm_mode_entry_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_NAK);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-62 UFP Structured VDM Exit Mode State Diagram
 */

void pe_ufp_vdm_mode_exit_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_exit_mode(pd_port, pd_event);
}

void pe_ufp_vdm_mode_exit_ack_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_ACK);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_ufp_vdm_mode_exit_nak_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reply_svdm_request_simply(pd_port, pd_event, CMDT_RSP_NAK);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-63 UFP VDM Attention State Diagram
 */

void pe_ufp_vdm_attention_request_entry(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->mode_svid) {
#ifdef CONFIG_USB_PD_ALT_MODE
	case USB_SID_DISPLAYPORT:
		pd_dpm_ufp_send_dp_attention(pd_port, pd_event);
		break;
#endif
	default:
		pd_send_vdm_attention(pd_port,
			TCPC_TX_SOP, pd_port->mode_svid, pd_port->mode_obj_pos);
		break;
	}

	pd_free_pd_event(pd_port, pd_event);
}

/*
 * ALT Mode
 */

#ifdef CONFIG_USB_PD_ALT_MODE

void pe_ufp_vdm_dp_status_update_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_dp_status(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_ufp_vdm_dp_configure_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_request_dp_config(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

#endif
