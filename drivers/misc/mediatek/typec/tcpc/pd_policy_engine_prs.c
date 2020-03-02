/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Power Delivery Policy Engine for PRS
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
 * [PD2.0] Figure 8-51:
 *      Dual-Role Port in Source to Sink Power Role Swap State Diagram
 */

void pe_prs_src_snk_evaluate_pr_swap_entry(struct pd_port *pd_port)
{
	pd_dpm_prs_evaluate_swap(pd_port, PD_ROLE_SINK);
}

void pe_prs_src_snk_accept_pr_swap_entry(struct pd_port *pd_port)
{
	pd_notify_pe_execute_pr_swap(pd_port, true);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_ACCEPT);
}

void pe_prs_src_snk_transition_to_off_entry(struct pd_port *pd_port)
{
	pd_lock_msg_output(pd_port);	/* for tSRCTransition */
	pd_notify_pe_execute_pr_swap(pd_port, true);

	pd_enable_timer(pd_port, PD_TIMER_SOURCE_TRANSITION);
}

void pe_prs_src_snk_assert_rd_entry(struct pd_port *pd_port)
{
	pd_dpm_prs_change_role(pd_port, PD_ROLE_SINK);
}

void pe_prs_src_snk_wait_source_on_entry(struct pd_port *pd_port)
{
	PE_STATE_HRESET_IF_TX_FAILED(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_PS_RDY);
}

void pe_prs_src_snk_send_swap_entry(struct pd_port *pd_port)
{
	pe_send_swap_request_entry(pd_port, PD_CTRL_PR_SWAP);
}

void pe_prs_src_snk_reject_pr_swap_entry(struct pd_port *pd_port)
{
	pd_reply_wait_reject_msg(pd_port);
}

/*
 * [PD2.0] Figure 8-52:
 *      Dual-role Port in Sink to Source Power Role Swap State Diagram
 */

void pe_prs_snk_src_evaluate_pr_swap_entry(struct pd_port *pd_port)
{
	pd_dpm_prs_evaluate_swap(pd_port, PD_ROLE_SOURCE);
}

void pe_prs_snk_src_accept_pr_swap_entry(
			struct pd_port *pd_port)
{
	pd_notify_pe_execute_pr_swap(pd_port, true);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_ACCEPT);
}

void pe_prs_snk_src_transition_to_off_entry(struct pd_port *pd_port)
{
	/*
	 * Sink should call pd_notify_pe_execute_pr_swap before this state,
	 * because source may turn off power & change CC before we got
	 * GoodCRC or Accept.
	 */

	pd_port->pe_data.during_swap = true;
	pd_enable_pe_state_timer(pd_port, PD_TIMER_PS_SOURCE_OFF);
	pd_dpm_prs_turn_off_power_sink(pd_port);
}

void pe_prs_snk_src_assert_rp_entry(struct pd_port *pd_port)
{
	pd_dpm_prs_change_role(pd_port, PD_ROLE_SOURCE);
}

void pe_prs_snk_src_source_on_entry(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_RESET_CABLE
	dpm_reaction_set(pd_port, DPM_REACTION_CAP_RESET_CABLE);
#endif	/* CONFIG_USB_PD_RESET_CABLE */

	PE_STATE_HRESET_IF_TX_FAILED(pd_port);
	pd_dpm_dynamic_enable_vconn(pd_port);
	pd_dpm_prs_enable_power_source(pd_port, true);

	/* Send PS_Rdy in process_event after source_on */
}

void pe_prs_snk_src_send_swap_entry(struct pd_port *pd_port)
{
	pd_notify_pe_execute_pr_swap(pd_port, false);
	pe_send_swap_request_entry(pd_port, PD_CTRL_PR_SWAP);
}

void pe_prs_snk_src_reject_swap_entry(struct pd_port *pd_port)
{
	pd_reply_wait_reject_msg(pd_port);
}
