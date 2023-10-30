/*
 * Copyright (C) 2020 Richtek Inc.
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

#ifndef PD_PROCESS_EVT_H_
#define PD_PROCESS_EVT_H_

#include <linux/usb/tcpc/tcpci.h>
#include <linux/usb/tcpc/pd_policy_engine.h>

/*---------------------------------------------------------------------------*/

struct pe_state_transition {
	uint8_t curr_state; /*state, msg, or cmd */
	uint8_t next_state;
};

struct pe_state_reaction {
	uint16_t nr_transition;
	const struct pe_state_transition *state_transition;
};

#define DECL_PE_STATE_TRANSITION(state)	\
	static const struct pe_state_transition state##_state_transition[]

#define DECL_PE_STATE_REACTION(state)	\
	static const struct pe_state_reaction state##_reactions = {\
		.nr_transition = ARRAY_SIZE(state##_state_transition),\
		.state_transition = state##_state_transition,\
	}

/*-----------------------------------------------------------------------------
 * Sink & Source Common Event
 *---------------------------------------------------------------------------
 */

bool pd_process_protocol_error(
	struct pd_port *pd_port, struct pd_event *pd_event);

bool pd_process_tx_failed(struct pd_port *pd_port);

/*---------------------------------------------------------------------------*/

#define PE_TRANSIT_STATE(pd_port, state)	\
	(pd_port->pe_state_next = state)

#define PE_TRANSIT_DATA_STATE(pd_port, ufp, dfp)	\
	(pd_port->pe_state_next =\
	((pd_port->data_role == PD_ROLE_UFP) ? ufp : dfp))

static inline uint8_t pe_get_curr_ready_state(struct pd_port *pd_port)
{
	return pd_port->curr_ready_state;
}

static inline uint8_t pe_get_curr_hard_reset_state(struct pd_port *pd_port)
{
	return pd_port->curr_hreset_state;
}

static inline uint8_t pe_get_curr_soft_reset_state(struct pd_port *pd_port)
{
	return pd_port->curr_sreset_state;
}

static inline uint8_t pe_get_curr_evaluate_pr_swap_state(
	struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SINK)
		return PE_PRS_SNK_SRC_EVALUATE_PR_SWAP;

	return PE_PRS_SRC_SNK_EVALUATE_PR_SWAP;
}

static inline uint8_t pe_get_curr_send_pr_swap_state(
	struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SINK)
		return PE_PRS_SNK_SRC_SEND_SWAP;

	return PE_PRS_SRC_SNK_SEND_SWAP;
}

static inline uint8_t pd_get_curr_hard_reset_recv_state(
	struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SINK)
		return PE_SNK_TRANSITION_TO_DEFAULT;

	return PE_SRC_HARD_RESET_RECEIVED;
}

static inline uint8_t pd_get_curr_soft_reset_recv_state(
	struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SINK)
		return PE_SNK_SOFT_RESET;

	return PE_SRC_SOFT_RESET;
}

static inline void pe_transit_ready_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port, pe_get_curr_ready_state(pd_port));
}

static inline void pe_transit_hard_reset_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port, pe_get_curr_hard_reset_state(pd_port));
}

static inline void pe_transit_soft_reset_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port, pe_get_curr_soft_reset_state(pd_port));
}

static inline void pe_transit_soft_reset_recv_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port, pd_get_curr_soft_reset_recv_state(pd_port));
}

static inline void pe_transit_evaluate_pr_swap_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port,
		pe_get_curr_evaluate_pr_swap_state(pd_port));
}

static inline void pe_transit_send_pr_swap_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port,
		pe_get_curr_send_pr_swap_state(pd_port));
}

static inline void pe_transit_hard_reset_recv_state(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port,
		pd_get_curr_hard_reset_recv_state(pd_port));
}

/*---------------------------------------------------------------------------*/

static inline bool pd_check_pe_state_ready(struct pd_port *pd_port)
{
	uint8_t ready_state = pe_get_curr_ready_state(pd_port);

	return pd_port->pe_state_curr == ready_state;
}

/*---------------------------------------------------------------------------*/

#define PE_MAKE_STATE_TRANSIT_SINGLE(reaction, next)	\
		pd_make_pe_state_transit_single(\
			pd_port, pd_port->pe_state_curr, reaction, next)
/* PE_MAKE_STATE_TRANSIT_SINGLE */

#define PE_MAKE_STATE_TRANSIT_TO_HRESET(reaction)	\
	PE_MAKE_STATE_TRANSIT_SINGLE(reaction, \
		pe_get_curr_hard_reset_state(pd_port))
/* PE_MAKE_STATE_TRANSIT_TO_HRESET */

#define PE_MAKE_STATE_TRANSIT(state)	\
		pd_make_pe_state_transit(\
			pd_port, pd_port->pe_state_curr, &state##_reactions)
/* PE_MAKE_STATE_TRANSIT */


static inline bool pd_make_pe_state_transit_single(struct pd_port *pd_port,
	uint8_t curr_state, uint8_t reaction_state, uint8_t next_state)
{
	if (curr_state == reaction_state) {
		PE_TRANSIT_STATE(pd_port, next_state);
		return true;
	}

	return false;
}

bool pd_make_pe_state_transit(struct pd_port *pd_port, uint8_t curr_state,
	const struct pe_state_reaction *state_reaction);

bool pd_process_event(struct pd_port *pd_port, struct pd_event *pd_event);

extern bool pd_process_event_snk(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_src(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_drs(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_prs(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_vdm(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_vcs(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_com(struct pd_port *pd_port, struct pd_event *evt);
extern bool pd_process_event_tcp(struct pd_port *pd_port, struct pd_event *evt);

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
extern bool pd_process_event_dbg(struct pd_port *pd_port, struct pd_event *evt);
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */

#endif /* PD_PROCESS_EVT_H_ */
