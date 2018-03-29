/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_policy_engine_prs.c
 * Power Delvery Policy Engine for PRS
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

#include <linux/delay.h>

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-51:
 *      Dual-Role Port in Source to Sink Power Role Swap State Diagram
 */

void pe_prs_src_snk_evaluate_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_prs_evaluate_swap(pd_port, PD_ROLE_SINK);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_prs_src_snk_accept_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_notify_pe_execute_pr_swap(pd_port);

	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
}

void pe_prs_src_snk_transition_to_off_entry(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_lock_msg_output(pd_port);	/* for tSRCTransition */
	pd_notify_pe_execute_pr_swap(pd_port);

	pd_enable_timer(pd_port, PD_TIMER_SOURCE_TRANSITION);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_prs_src_snk_assert_rd_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_prs_change_role(pd_port, PD_ROLE_SINK);
}

void pe_prs_src_snk_wait_source_on_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_PS_RDY);
}

void pe_prs_src_snk_wait_source_on_exit(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_PS_SOURCE_ON);
}

void pe_prs_src_snk_send_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_PR_SWAP);
}

void pe_prs_src_snk_reject_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_event->msg_sec == PD_DPM_NAK_REJECT)
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_REJECT);
	else
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_WAIT);
}

/*
 * [PD2.0] Figure 8-52:
 *      Dual-role Port in Sink to Source Power Role Swap State Diagram
 */

void pe_prs_snk_src_evaluate_pr_swap_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_prs_evaluate_swap(pd_port, PD_ROLE_SOURCE);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_prs_snk_src_accept_pr_swap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	/* Source may turn off power before we got good-crc*/
	pd_notify_pe_execute_pr_swap(pd_port);

	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
}

void pe_prs_snk_src_transition_to_off_entry(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_notify_pe_execute_pr_swap(pd_port);

	pd_enable_timer(pd_port, PD_TIMER_PS_SOURCE_OFF);
	pd_dpm_prs_turn_off_power_sink(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_prs_snk_src_transition_to_off_exit(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_PS_SOURCE_OFF);
}

void pe_prs_snk_src_assert_rp_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_prs_change_role(pd_port, PD_ROLE_SOURCE);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_prs_snk_src_source_on_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_prs_enable_power_source(pd_port, true);
}

void pe_prs_snk_src_source_on_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
/*
	Do it in process_event after source_on
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_PS_RDY);
*/
}

void pe_prs_snk_src_send_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_PR_SWAP);
}

void pe_prs_snk_src_reject_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_event->msg_sec == PD_DPM_NAK_REJECT)
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_REJECT);
	else
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_WAIT);
}
