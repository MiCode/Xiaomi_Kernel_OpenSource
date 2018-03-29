/*
 * Copyright (C) 2016 Richtek Technology Corp.
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

#ifndef PD_PROCESS_EVT_H_
#define PD_PROCESS_EVT_H_

#include "tcpci.h"
#include "pd_policy_engine.h"

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#define CONFIG_PD_DISCOVER_CABLE_ID
#endif

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
#undef CONFIG_PD_DISCOVER_CABLE_ID
#define CONFIG_PD_DISCOVER_CABLE_ID
#endif

typedef struct __pe_state_transition {
	uint8_t curr_state; /*state, msg, or cmd */
	uint8_t next_state;
} pe_state_transition_t;

typedef struct __pe_state_reaction {
	uint16_t nr_transition;
	const pe_state_transition_t *state_transition;
} pe_state_reaction_t;

#define DECL_PE_STATE_TRANSITION(state)	\
	static const pe_state_transition_t state##_state_transition[]

#define DECL_PE_STATE_REACTION(state)	\
	static const pe_state_reaction_t state##_reactions = {\
		.nr_transition = ARRAY_SIZE(state##_state_transition),\
		.state_transition = state##_state_transition,\
	}

static inline bool pd_check_pe_state_ready(pd_port_t *pd_port)
{
	/* TODO: Handle Port Partner first (skip our get_cap state )*/
	switch (pd_port->pe_state_curr) {
	case PE_SNK_READY:
	case PE_SRC_READY:
		return true;
	default:
		return false;
	}
}

/*
//-----------------------------------------------------------------------------
// Sink & Source Common Event
//-----------------------------------------------------------------------------
*/

bool pd_process_data_msg_bist(
	pd_port_t *pd_port, pd_event_t *pd_event);

bool pd_process_protocol_error(
	pd_port_t *pd_port, pd_event_t *pd_event);

bool pd_process_ctrl_msg_dr_swap(
	pd_port_t *pd_port, pd_event_t *pd_event);
bool pd_process_dpm_msg_dr_swap(
	pd_port_t *pd_port, pd_event_t *pd_event);

bool pd_process_ctrl_msg_pr_swap(
	pd_port_t *pd_port, pd_event_t *pd_event);
bool pd_process_dpm_msg_pr_swap(
	pd_port_t *pd_port, pd_event_t *pd_event);

bool pd_process_ctrl_msg_vconn_swap(
	pd_port_t *pd_port, pd_event_t *pd_event);
bool pd_process_dpm_msg_vconn_swap(
	pd_port_t *pd_port, pd_event_t *pd_event);

/*
//-----------------------------------------------------------------------------
*/

#define PE_TRANSIT_STATE(pd_port, state)	\
	(pd_port->pe_state_next = state)

#define PE_TRANSIT_POWER_STATE(pd_port, sink, source)	\
	(pd_port->pe_state_next =\
	((pd_port->power_role == PD_ROLE_SINK) ? sink : source))

#define PE_TRANSIT_DATA_STATE(pd_port, ufp, dfp)	\
	(pd_port->pe_state_next =\
	((pd_port->data_role == PD_ROLE_UFP) ? ufp : dfp))

#define PE_TRANSIT_READY_STATE(pd_port) \
	PE_TRANSIT_POWER_STATE(pd_port, PE_SNK_READY, PE_SRC_READY)

#define PE_TRANSIT_HARD_RESET_STATE(pd_port) \
	PE_TRANSIT_POWER_STATE(pd_port, PE_SNK_HARD_RESET, PE_SRC_HARD_RESET)

#define PE_TRANSIT_SOFT_RESET_STATE(pd_port) \
	PE_TRANSIT_POWER_STATE(pd_port, PE_SNK_SOFT_RESET, PE_SRC_SOFT_RESET)

#define PE_TRANSIT_VCS_SWAP_STATE(pd_port) \
	PE_TRANSIT_STATE(pd_port, pd_port->vconn_source ? \
		PE_VCS_WAIT_FOR_VCONN : PE_VCS_TURN_ON_VCONN)

#define PE_TRANSIT_SEND_SOFT_RESET_STATE(pd_port) \
	PE_TRANSIT_POWER_STATE(pd_port, \
	PE_SNK_SEND_SOFT_RESET, PE_SRC_SEND_SOFT_RESET)

/*
//-----------------------------------------------------------------------------
*/

#define PE_MAKE_STATE_TRANSIT(state)	\
		pd_make_pe_state_transit(\
			pd_port, pd_port->pe_state_curr, &state##_reactions)
/* PE_MAKE_STATE_TRANSIT */

#define PE_MAKE_STATE_TRANSIT_VIRT(state)	\
		pd_make_pe_state_transit_virt(\
			pd_port, pd_port->pe_state_curr, &state##_reactions)
/* PE_MAKE_STATE_TRANSIT_VIRT */

#define PE_MAKE_STATE_TRANSIT_FORCE(state, force)	\
		pd_make_pe_state_transit_force(\
		pd_port, pd_port->pe_state_curr, force, &state##_reactions)
/* PE_MAKE_STATE_TRANSIT_FORCE */

#define VDM_CMD_STATE_MASK(raw)		(raw & 0xdf)

#define PE_MAKE_VDM_CMD_STATE_TRANSIT(state)	\
		pd_make_pe_state_transit(\
			pd_port, \
			VDM_CMD_STATE_MASK(pd_event->pd_msg->payload[0]), \
			&state##_reactions)
/* PE_MAKE_VDM_CMD_STATE_TRANSIT */

#define PE_MAKE_VDM_CMD_STATE_TRANSIT_VIRT(state)	\
		pd_make_pe_state_transit_virt(\
			pd_port, \
			VDM_CMD_STATE_MASK(pd_event->pd_msg->payload[0]), \
			&state##_reactions)
/* PE_MAKE_VDM_CMD_STATE_TRANSIT_VIRT */




bool pd_make_pe_state_transit(pd_port_t *pd_port, uint8_t curr_state,
	const pe_state_reaction_t *state_reaction);

bool pd_make_pe_state_transit_virt(pd_port_t *pd_port, uint8_t curr_state,
	const pe_state_reaction_t *state_reaction);

bool pd_make_pe_state_transit_force(pd_port_t *pd_port,
	uint8_t curr_state, uint8_t force_state,
	const pe_state_reaction_t *state_reaction);

bool pd_process_event(pd_port_t *pd_port, pd_event_t *pd_event, bool vdm_evt);

extern bool pd_process_event_snk(pd_port_t *pd_port, pd_event_t *evt);
extern bool pd_process_event_src(pd_port_t *pd_port, pd_event_t *evt);
extern bool pd_process_event_drs(pd_port_t *pd_port, pd_event_t *evt);
extern bool pd_process_event_prs(pd_port_t *pd_port, pd_event_t *evt);
extern bool pd_process_event_vdm(pd_port_t *pd_port, pd_event_t *evt);
extern bool pd_process_event_vcs(pd_port_t *pd_port, pd_event_t *evt);

#endif /* PD_PROCESS_EVT_H_ */
