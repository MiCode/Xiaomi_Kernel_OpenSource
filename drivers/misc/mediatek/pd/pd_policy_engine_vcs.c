/*
 * drives/usb/pd/pd_policy_engine_vcs.c
 * Power Delvery Policy Engine for VSC
 *
 * Copyright (C) 2015 Richtek Technology Corp.
 * Author: TH <tsunghan_tasi@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-57 VCONN Swap State Diagram
 */

void pe_vcs_send_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_VCONN_SWAP);
}

void pe_vcs_evaluate_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_vcs_evaluate_swap(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_vcs_accept_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
}

void pe_vcs_reject_vconn_swap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_event->msg_sec == PD_DPM_NAK_REJECT)
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_REJECT);
	else
		pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_WAIT);
}

void pe_vcs_wait_for_vconn_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_enable_timer(pd_port, PD_TIMER_VCONN_ON);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_vcs_wait_for_vconn_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VCONN_ON);
}

void pe_vcs_turn_off_vconn_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_vcs_enable_vconn(pd_port, false);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_vcs_turn_on_vconn_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_vcs_enable_vconn(pd_port, true);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_vcs_send_ps_rdy_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_PS_RDY);
}
