/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_policy_engine_snk.c
 * Power Delvery Policy Engine for SNK
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
 * [PD2.0] Figure 8-39 Sink Port state diagram
 */

void pe_snk_startup_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	uint8_t rx_cap = PD_RX_CAP_PE_STARTUP;

	pd_port->state_machine = PE_STATE_MACHINE_SINK;
	pd_reset_protocol_layer(pd_port);

	switch (pd_event->event_type) {
	case PD_EVT_HW_MSG:	/* CC attached */
		pd_put_pe_event(pd_port, PD_PE_RESET_PRL_COMPLETED);
		break;

	case PD_EVT_PE_MSG: /* From Hard-Reset */
		pd_enable_vbus_valid_detection(pd_port, false);
		break;

	case PD_EVT_CTRL_MSG: /* From PR-SWAP (Received PS_RDY) */
		/* If we reset rx_cap in here,
		 * maybe can't meet tSwapSink (Check it later) */
		if (!pd_dpm_check_vbus_valid(pd_port)) {
			PE_INFO("rx_cap_on\r\n");
			rx_cap = PD_RX_CAP_PE_SEND_WAIT_CAP;
		}

		pd_put_pe_event(pd_port, PD_PE_RESET_PRL_COMPLETED);
		pd_free_pd_event(pd_port, pd_event);
		break;
	}

	pd_set_rx_enable(pd_port, rx_cap);
}

void pe_snk_discovery_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
#ifdef CONFIG_USB_PD_FAST_RESP_TYPEC_SRC
	pd_disable_timer(pd_poprt, PD_TIMER_SRC_RECOVER);
#endif /* CONFIG_USB_PD_FAST_RESP_TYPEC_SRC */
	pd_enable_vbus_valid_detection(pd_port, true);
}

void pe_snk_wait_for_capabilities_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_notify_pe_hard_reset_completed(pd_port);

	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_SEND_WAIT_CAP);
	pd_enable_timer(pd_port, PD_TIMER_SINK_WAIT_CAP);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_snk_wait_for_capabilities_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_SINK_WAIT_CAP);
}

void pe_snk_evaluate_capability_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	/* Stop NoResponseTimer and reset HardResetCounter to zero */

	pd_disable_timer(pd_port, PD_TIMER_NO_RESPONSE);

	pd_port->hard_reset_counter = 0;
	pd_port->pd_connected = 1;
	pd_port->pd_prev_connected = 1;
	pd_port->explicit_contract = false;

	pd_dpm_snk_evaluate_caps(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_snk_select_capability_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_event->msg == PD_DPM_NOTIFIED) {
		PE_DBG("SelectCap%d, rdo:0x%08x\r\n",
			pd_event->msg_sec, pd_port->last_rdo);
	} else {
		/* new request, for debug only */
		pd_dpm_sink_vbus(pd_port, false);
		PE_DBG("NewReq, rdo:0x%08x\r\n", pd_port->last_rdo);
	}

	pd_lock_msg_output(pd_port);	/* SenderResponse */
	pd_send_data_msg(pd_port,
		TCPC_TX_SOP, PD_DATA_REQUEST, 1, &pd_port->last_rdo);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_snk_select_capability_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);

	/* Waiting for Hard-Reset Done */
	if (!pd_event_msg_match(pd_event,
		PD_EVT_TIMER_MSG, PD_TIMER_SENDER_RESPONSE))
		pd_unlock_msg_output(pd_port);
}

void pe_snk_transition_sink_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_enable_timer(pd_port, PD_TIMER_PS_TRANSITION);

	if (pd_event->msg == PD_CTRL_GOTO_MIN) {
		if (pd_port->dpm_caps & DPM_CAP_LOCAL_GIVE_BACK) {
			pd_port->request_i_new = 0;
			pd_port->request_v_new = TCPC_VBUS_SINK_5V;
			pd_dpm_snk_transition_power(pd_port, pd_event);
		}
	}

	pd_free_pd_event(pd_port, pd_event);
}

void pe_snk_transition_sink_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_snk_transition_power(pd_port, pd_event);
	pd_disable_timer(pd_port, PD_TIMER_PS_TRANSITION);
}

void pe_snk_ready_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_event_msg_match(pd_event, PD_EVT_CTRL_MSG, PD_CTRL_WAIT))
		pd_enable_timer(pd_port, PD_TIMER_SINK_REQUEST);

	pd_port->state_machine = PE_STATE_MACHINE_SINK;
	pe_power_ready_entry(pd_port, pd_event);
}

void pe_snk_hard_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_hard_reset(pd_port);
}

void pe_snk_transition_to_default_entry(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_reset_local_hw(pd_port);
	pd_dpm_snk_hard_reset(pd_port, pd_event);
}

void pe_snk_transition_to_default_exit(
				pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_enable_timer(pd_port, PD_TIMER_NO_RESPONSE);

#ifdef CONFIG_USB_PD_FAST_RESP_TYPEC_SRC
	if (!pd_port->pd_prev_connected)
		pd_enable_timer(pd_port, PD_TIMER_SRC_RECOVER);
#endif /* CONFIG_USB_PD_FAST_RESP_TYPEC_SRC */
}

void pe_snk_give_sink_cap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_send_sink_caps(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_snk_get_source_cap_entry(
			pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_GET_SOURCE_CAP);
}

void pe_snk_send_soft_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_port->state_machine = PE_STATE_MACHINE_SINK;

	pd_reset_protocol_layer(pd_port);
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_SOFT_RESET);

	pd_free_pd_event(pd_port, pd_event);
}

void pe_snk_soft_reset_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_port->state_machine = PE_STATE_MACHINE_SINK;

	pd_reset_protocol_layer(pd_port);
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_ACCEPT);

	pd_free_pd_event(pd_port, pd_event);
}
