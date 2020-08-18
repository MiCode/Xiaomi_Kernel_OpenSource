/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Power Delivery Policy Engine for VCS
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
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-57 VCONN Swap State Diagram
 */

void pe_vcs_send_swap_entry(struct pd_port *pd_port)
{
	pe_send_swap_request_entry(pd_port, PD_CTRL_VCONN_SWAP);
}

void pe_vcs_evaluate_swap_entry(struct pd_port *pd_port)
{
	pd_dpm_vcs_evaluate_swap(pd_port);
}

void pe_vcs_accept_swap_entry(struct pd_port *pd_port)
{
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_ACCEPT);
}

void pe_vcs_reject_vconn_swap_entry(struct pd_port *pd_port)
{
	pd_reply_wait_reject_msg(pd_port);
}

void pe_vcs_wait_for_vconn_entry(struct pd_port *pd_port)
{
	pd_enable_pe_state_timer(pd_port, PD_TIMER_VCONN_ON);
}

void pe_vcs_turn_off_vconn_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_DPM_ACK(pd_port);
	pd_dpm_vcs_enable_vconn(pd_port, PD_ROLE_VCONN_OFF);
}

void pe_vcs_turn_on_vconn_entry(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_RESET_CABLE
	dpm_reaction_set(pd_port, DPM_REACTION_CAP_RESET_CABLE);
#endif	/* CONFIG_USB_PD_RESET_CABLE */
#endif	/* CONFIG_USB_PD_REV30 */

	pd_dpm_vcs_enable_vconn(pd_port, PD_ROLE_VCONN_DYNAMIC_ON);
}

void pe_vcs_send_ps_rdy_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_PS_RDY);
}
