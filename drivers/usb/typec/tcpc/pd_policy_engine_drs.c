/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Power Delivery Policy Engine for DRS
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
 * [PD2.0] Figure 8-49: Type-C DFP to UFP Data Role Swap State Diagram
 */

void pe_drs_dfp_ufp_evaluate_dr_swap_entry(struct pd_port *pd_port)
{
	pd_dpm_drs_evaluate_swap(pd_port, PD_ROLE_UFP);
}

void pe_drs_dfp_ufp_accept_dr_swap_entry(struct pd_port *pd_port)
{
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_ACCEPT);
}

void pe_drs_dfp_ufp_change_to_ufp_entry(struct pd_port *pd_port)
{
	pd_dpm_drs_change_role(pd_port, PD_ROLE_UFP);
}

void pe_drs_dfp_ufp_send_dr_swap_entry(struct pd_port *pd_port)
{
	pe_send_swap_request_entry(pd_port, PD_CTRL_DR_SWAP);
}

void pe_drs_dfp_ufp_reject_dr_swap_entry(struct pd_port *pd_port)
{
	pd_reply_wait_reject_msg(pd_port);
}

/*
 * [PD2.0] Figure 8-50: Type-C UFP to DFP Data Role Swap State Diagram
 */

void pe_drs_ufp_dfp_evaluate_dr_swap_entry(struct pd_port *pd_port)
{
	pd_dpm_drs_evaluate_swap(pd_port, PD_ROLE_DFP);
}

void pe_drs_ufp_dfp_accept_dr_swap_entry(struct pd_port *pd_port)
{
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_ACCEPT);
}

void pe_drs_ufp_dfp_change_to_dfp_entry(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_RESET_CABLE
	dpm_reaction_set(pd_port, DPM_REACTION_CAP_RESET_CABLE);
#endif	/* CONFIG_USB_PD_RESET_CABLE */

	pd_dpm_drs_change_role(pd_port, PD_ROLE_DFP);
}

void pe_drs_ufp_dfp_send_dr_swap_entry(struct pd_port *pd_port)
{
	pe_send_swap_request_entry(pd_port, PD_CTRL_DR_SWAP);
}

void pe_drs_ufp_dfp_reject_dr_swap_entry(
			struct pd_port *pd_port)
{
	pd_reply_wait_reject_msg(pd_port);
}
