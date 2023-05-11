/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Power Delivery Process Event For VDM
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
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"
#include "inc/pd_dpm_core.h"

/* VDM reactions */

#define VDM_CMD_FLAG_CABLE_CMD			(1<<0)
#define VDM_CMD_FLAG_SEND_BY_UFP			(1<<1)
#define VDM_CMD_FLAG_SEND_BY_DFP			(1<<2)
#define VDM_CMD_FLAG_RECV_BY_UFP			(1<<3)
#define VDM_CMD_FLAG_RECV_BY_DFP			(1<<4)
#define VDM_CMD_FLAG_PD30_DUPLEX			(1<<5)

struct vdm_state_transition {
	uint8_t	vdm_cmd;
	uint8_t vdm_init_state;
	uint8_t	vdm_request_state;
	uint8_t	vdm_cmd_flags;
};

#define VDM_CMD_REACTION(cmd, init, req, flags)	{ \
	.vdm_cmd = cmd,	\
	.vdm_init_state = init, \
	.vdm_request_state = req,	\
	.vdm_cmd_flags = flags, \
}

#define VDM_DFP_CMD_REACTION(cmd, init, req)		\
	VDM_CMD_REACTION(cmd, init, req,  \
		VDM_CMD_FLAG_SEND_BY_DFP | VDM_CMD_FLAG_RECV_BY_UFP)

#define VDM_UFP_CMD_REACTION(cmd, init, req)		\
	VDM_CMD_REACTION(cmd, init, req, \
		VDM_CMD_FLAG_SEND_BY_UFP | VDM_CMD_FLAG_RECV_BY_DFP)

#define VDM_CABLE_CMD_REACTION(cmd, init, req)		\
	VDM_CMD_REACTION(cmd, init, req, \
		VDM_CMD_FLAG_SEND_BY_UFP | VDM_CMD_FLAG_SEND_BY_DFP \
		| VDM_CMD_FLAG_CABLE_CMD)

#define VDM_DFP_CMD_REACTION_PD30(cmd, init, req)		\
	VDM_CMD_REACTION(cmd, init, req,  \
		VDM_CMD_FLAG_SEND_BY_DFP | VDM_CMD_FLAG_RECV_BY_UFP |	\
		VDM_CMD_FLAG_PD30_DUPLEX)

#define VDM_UFP_CMD_REACTION_PD30(cmd, init, req)		\
	VDM_CMD_REACTION(cmd, init, req, \
		VDM_CMD_FLAG_SEND_BY_UFP | VDM_CMD_FLAG_RECV_BY_DFP |	\
		VDM_CMD_FLAG_PD30_DUPLEX)

static const struct vdm_state_transition pe_vdm_state_reactions[] = {

	VDM_DFP_CMD_REACTION_PD30(CMD_DISCOVER_IDENT,
		PE_UFP_VDM_GET_IDENTITY,
		PE_DFP_UFP_VDM_IDENTITY_REQUEST
	),

	VDM_DFP_CMD_REACTION_PD30(CMD_DISCOVER_SVID,
		PE_UFP_VDM_GET_SVIDS,
		PE_DFP_VDM_SVIDS_REQUEST
	),

	VDM_DFP_CMD_REACTION_PD30(CMD_DISCOVER_MODES,
		PE_UFP_VDM_GET_MODES,
		PE_DFP_VDM_MODES_REQUEST
	),

	VDM_DFP_CMD_REACTION(CMD_ENTER_MODE,
		PE_UFP_VDM_EVALUATE_MODE_ENTRY,
		PE_DFP_VDM_MODE_ENTRY_REQUEST
	),

	VDM_DFP_CMD_REACTION(CMD_EXIT_MODE,
		PE_UFP_VDM_MODE_EXIT,
		PE_DFP_VDM_MODE_EXIT_REQUEST
	),

	VDM_UFP_CMD_REACTION_PD30(CMD_ATTENTION,
		PE_DFP_VDM_ATTENTION_REQUEST,
		PE_UFP_VDM_ATTENTION_REQUEST
	),

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	VDM_DFP_CMD_REACTION(CMD_DP_STATUS,
		PE_UFP_VDM_DP_STATUS_UPDATE,
		PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST
	),

	VDM_DFP_CMD_REACTION(CMD_DP_CONFIG,
		PE_UFP_VDM_DP_CONFIGURE,
		PE_DFP_VDM_DP_CONFIGURATION_REQUEST
	),
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	/* Only handle Timeout Case */
	VDM_DFP_CMD_REACTION_PD30(0,
		PE_UFP_UVDM_RECV,
		PE_DFP_UVDM_SEND
	),
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	VDM_CABLE_CMD_REACTION(CMD_DISCOVER_IDENT,
		0,
		PE_SRC_VDM_IDENTITY_REQUEST
	),
#endif /* CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	VDM_CABLE_CMD_REACTION(CMD_DISCOVER_IDENT,
		0,
		PE_DFP_CBL_VDM_IDENTITY_REQUEST
	),
#endif /* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */
};

static inline bool pd_vdm_state_transit_rx(struct pd_port *pd_port,
	const struct vdm_state_transition *state_transition)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (!pd_check_pe_state_ready(pd_port)) {
		PE_DBG("670 : invalid, current status\n");
		return false;
	}

	PE_TRANSIT_STATE(pd_port, state_transition->vdm_init_state);
	return true;
}

static inline bool pd_vdm_state_transit_tx(struct pd_port *pd_port,
	uint8_t vdm_cmdt, const struct vdm_state_transition *state_transition)
{
	int ret_code = TCP_DPM_RET_VDM_NAK;
	uint8_t curr_state = state_transition->vdm_request_state;
	uint8_t next_state = curr_state + 1;

	switch (vdm_cmdt) {

	case CMDT_RSP_NAK:
		if (curr_state != PE_DFP_VDM_MODE_EXIT_REQUEST)
			next_state = curr_state+2;
		break;

	case CMDT_RSP_BUSY:
		if (curr_state == PE_DFP_VDM_MODE_EXIT_REQUEST)
			next_state = pe_get_curr_hard_reset_state(pd_port);
		else
			next_state = curr_state+2;
		break;

	default:
		ret_code = TCP_DPM_RET_VDM_ACK;
		break;
	}

	pd_notify_tcp_vdm_event_2nd_result(pd_port, ret_code);
	PE_TRANSIT_STATE(pd_port, next_state);
	return true;
}

bool vdm_is_state_transition_available(struct pd_port *pd_port,
	bool recv, const struct vdm_state_transition *state_transition)
{
	uint8_t shift = 1;
	uint8_t vdm_cmd_flags = state_transition->vdm_cmd_flags;

	if (pd_check_rev30(pd_port) &&
		(vdm_cmd_flags & VDM_CMD_FLAG_PD30_DUPLEX)) {
		return true;
	}

	if (pd_port->data_role == PD_ROLE_DFP)
		shift += 1;

	if (recv)
		shift += 2;

	return vdm_cmd_flags & (1 << shift);
}

static bool pd_vdm_state_transit(
	struct pd_port *pd_port, uint8_t vdm_cmdt,
	const struct vdm_state_transition *state_transition)
{
	uint8_t curr_state;

	curr_state = pd_port->pe_state_curr;

	if (curr_state == PE_UFP_VDM_ATTENTION_REQUEST) {
		VDM_STATE_DPM_INFORMED(pd_port);
		pd_notify_tcp_vdm_event_2nd_result(
			pd_port, TCP_DPM_RET_TIMEOUT);
		return false;
	}

	if (vdm_cmdt == CMDT_INIT) {	/* Recv */
		if (!vdm_is_state_transition_available(
			pd_port, true, state_transition))
			return false;

		return pd_vdm_state_transit_rx(pd_port, state_transition);
	}

	/* Send */
	if (!vdm_is_state_transition_available(
				pd_port, false, state_transition))
		return false;

	if (curr_state != state_transition->vdm_request_state)
		return false;

	return pd_vdm_state_transit_tx(
			pd_port, vdm_cmdt, state_transition);
}

enum {
	VDM_STATE_TRANSIT_SOP_CMD = 0,
	VDM_STATE_TRANSIT_SOP_PRIME_CMD = VDM_CMD_FLAG_CABLE_CMD,
	VDM_STATE_TRANSIT_NAK = 2,

	VDM_STATE_TRANSIT_CHECK_TX = 3,
};

static bool pe_check_vdm_state_transit_valid(
	struct pd_port *pd_port, uint8_t transit_type, uint8_t *vdm_cmdt,
	const struct vdm_state_transition *state_transition)
{
	uint8_t curr_state;
	uint8_t vdm_cmd;
	uint8_t cable_cmd;
	uint8_t vdm_cmd_flags;
	uint32_t curr_vdm_hdr;

	curr_state = pd_port->pe_state_curr;

	if (transit_type == VDM_STATE_TRANSIT_NAK) {
		if (curr_state != state_transition->vdm_request_state)
			return false;

		*vdm_cmdt = CMDT_RSP_BUSY;
		return true;
	} else if (transit_type >= VDM_STATE_TRANSIT_CHECK_TX) {
		return state_transition->vdm_request_state == transit_type;
	}

	curr_vdm_hdr = pd_port->curr_vdm_hdr;

	vdm_cmd = PD_VDO_CMD(curr_vdm_hdr);
	*vdm_cmdt = PD_VDO_CMDT(curr_vdm_hdr);

	if (state_transition->vdm_cmd != vdm_cmd)
		return false;

	vdm_cmd_flags = state_transition->vdm_cmd_flags;
	cable_cmd = vdm_cmd_flags & VDM_CMD_FLAG_CABLE_CMD;

	if (cable_cmd != transit_type)
		return false;

	if (cable_cmd && curr_state != state_transition->vdm_request_state)
		return false;

	return true;
}

static bool pd_make_vdm_state_transit(
		struct pd_port *pd_port, uint8_t transit_type)
{
	int i;
	bool check_tx;
	uint8_t vdm_cmdt;
	const struct vdm_state_transition *state_transition;

	uint8_t nr_transition = ARRAY_SIZE(pe_vdm_state_reactions);
	const struct vdm_state_transition *state_reaction =
						pe_vdm_state_reactions;

	check_tx = transit_type >= VDM_STATE_TRANSIT_CHECK_TX;

	for (i = 0; i < nr_transition; i++) {
		state_transition = &state_reaction[i];

		if (!pe_check_vdm_state_transit_valid(
			pd_port, transit_type, &vdm_cmdt, state_transition))
			continue;

		if (check_tx) {
			return vdm_is_state_transition_available(
				pd_port, false, state_transition);
		} else {
			return pd_vdm_state_transit(
				pd_port, vdm_cmdt, state_transition);
		}
	}

	return false;
}

static inline bool pd_make_vdm_state_transit_sop(struct pd_port *pd_port)
{
	return pd_make_vdm_state_transit(
		pd_port, VDM_STATE_TRANSIT_SOP_CMD);
}

static inline bool pd_make_vdm_state_transit_cable(struct pd_port *pd_port)
{
	return pd_make_vdm_state_transit(
		pd_port, VDM_STATE_TRANSIT_SOP_PRIME_CMD);
}

static inline bool pd_make_vdm_state_transit_nak(struct pd_port *pd_port)
{
	return pd_make_vdm_state_transit(
		pd_port, VDM_STATE_TRANSIT_NAK);
}

/* Discover Cable ID */

#ifdef CONFIG_PD_DISCOVER_CABLE_ID
DECL_PE_STATE_TRANSITION(PD_DPM_MSG_DISCOVER_CABLE) = {
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	{ PE_SRC_STARTUP, PE_SRC_VDM_IDENTITY_REQUEST},
	{ PE_SRC_DISCOVERY, PE_SRC_VDM_IDENTITY_REQUEST},
#endif

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	{ PE_SRC_READY, PE_DFP_CBL_VDM_IDENTITY_REQUEST},
	{ PE_SNK_READY, PE_DFP_CBL_VDM_IDENTITY_REQUEST},
#endif	/* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */

#ifdef CONFIG_PD_SRC_RESET_CABLE
	{ PE_SRC_CBL_SEND_SOFT_RESET, PE_SRC_VDM_IDENTITY_REQUEST},
#endif	/* CONFIG_PD_SRC_RESET_CABLE */
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_DISCOVER_CABLE);
#endif	/* CONFIG_PD_DISCOVER_CABLE_ID */

/*
 * [BLOCK] Porcess Ctrl MSG
 */

#ifdef CONFIG_USB_PD_ALT_MODE
#ifdef CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_ATTENTION
static inline bool pd_ufp_u_auto_send_attention(struct pd_port *pd_port)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	if (dp_data->local_config != 0) {
		PE_TRANSIT_STATE(pd_port,
			PE_UFP_VDM_ATTENTION_REQUEST);
		return true;
	}

	return false;
}
#endif	/* CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_ATTENTION */
#endif	/* CONFIG_USB_PD_ALT_MODE */

static inline bool pd_process_ctrl_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct pe_data *pe_data = &pd_port->pe_data;

	/* VDM Only handle CtrlMsg = GoodCRC */

	if (pd_event->msg != PD_CTRL_GOOD_CRC)
		return false;

	if (pe_data->vdm_state_flags &
		VDM_STATE_FLAG_ENABLE_VDM_RESPONSE_TIMER)
		pd_enable_timer(pd_port, pe_data->vdm_state_timer);

	switch (pd_port->pe_state_curr) {
#ifdef CONFIG_USB_PD_ALT_MODE
#ifdef CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_ATTENTION
	case PE_UFP_VDM_DP_CONFIGURE:
		if (pd_ufp_u_auto_send_attention(pd_port))
			return true;
		break;
#endif	/* CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_ATTENTION */

	case PE_UFP_VDM_ATTENTION_REQUEST:
		pd_notify_tcp_vdm_event_2nd_result(
			pd_port, TCP_DPM_RET_VDM_ACK);
		break;
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	case PE_SRC_VDM_IDENTITY_REQUEST:
		pe_data->power_cable_present = true;
		return false;
#endif	/* CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	case PE_DFP_CBL_VDM_IDENTITY_REQUEST:
		pe_data->power_cable_present = true;
		return false;
#endif	/* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	case PE_DFP_UVDM_SEND:
		if (!pd_port->uvdm_wait_resp) {
			PE_TRANSIT_STATE(pd_port, PE_DFP_UVDM_ACKED);
			return true;
		}
		break;
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */
	}

	if (pe_data->vdm_state_flags
		& VDM_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC) {
		pe_transit_ready_state(pd_port);
		return true;
	}

	return false;
}

/*
 * [BLOCK] Porcess Custom MSG (SVDM/UVDM)
 */

#ifdef CONFIG_USB_PD_CUSTOM_VDM

bool pd_process_custom_vdm(struct pd_port *pd_port, bool svdm)
{
	bool recv = false;

	if (pd_port->data_role == PD_ROLE_UFP)
		recv = true;

	if (pd_check_rev30(pd_port) && svdm)
		recv = true;

	if (pd_check_pe_state_ready(pd_port)) {
		if (recv) {
			PE_TRANSIT_STATE(pd_port, PE_UFP_UVDM_RECV);
			return true;
		}
	} else if (pd_port->pe_state_curr == PE_DFP_UVDM_SEND) {
		PE_TRANSIT_STATE(pd_port, PE_DFP_UVDM_ACKED);
		return true;
	}

	pd_put_dpm_event(pd_port, PD_DPM_NOT_SUPPORT);
	return false;
}

#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

static inline bool pd_process_uvdm(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

#ifdef CONFIG_USB_PD_UVDM
	/* support sop only */
	if (pd_event->pd_msg->frame_type != TCPC_TX_SOP)
		return false;

	if (pd_process_custom_vdm(pd_port, false))
		return true;
#else
	pd_put_dpm_event(pd_port, PD_DPM_NOT_SUPPORT);
#endif	/* CONFIG_USB_PD_UVDM */

	/* TODO: Reply Not_Supported Message*/
	PE_DBG("659 : Invalid UVDM\n");
	return false;
}

/*
 * [BLOCK] Porcess Data MSG (VDM)
 */

#if (PE_EVT_INFO_VDM_DIS == 0)
static const char * const pe_vdm_cmd_name[] = {
	"DiscoverID",
	"DiscoverSVID",
	"DiscoverMode",
	"EnterMode",
	"ExitMode",
	"Attention",
};

#if PE_INFO_ENABLE
static const char *const pe_vdm_cmd_type_name[] = {
	"INIT",
	"ACK",
	"NACK",
	"BUSY",
};
#endif /* PE_INFO_ENABLE */

static inline const char *assign_vdm_cmd_name(uint8_t cmd)
{
	if (cmd <= ARRAY_SIZE(pe_vdm_cmd_name) && cmd != 0) {
		cmd -= 1;
		return pe_vdm_cmd_name[cmd];
	}

	return NULL;
}

#ifdef CONFIG_USB_PD_ALT_MODE
static const char *const pe_vdm_dp_cmd_name[] = {
	"DPStatus",
	"DPConfig",
};

static inline const char *assign_vdm_dp_cmd_name(uint8_t cmd)
{
	if (cmd >= CMD_DP_STATUS) {
		cmd -= CMD_DP_STATUS;
		if (cmd < ARRAY_SIZE(pe_vdm_dp_cmd_name))
			return pe_vdm_dp_cmd_name[cmd];
	}

	return NULL;
}
#endif	/* CONFIG_USB_PD_ALT_MODE */

#endif /* if (PE_EVT_INFO_VDM_DIS == 0) */

static inline void print_vdm_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
#if (PE_EVT_INFO_VDM_DIS == 0)
	uint8_t cmd;
	uint8_t cmd_type;
	uint16_t svid;
	const char *name = NULL;
	uint32_t vdm_hdr = pd_port->curr_vdm_hdr;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	cmd = PD_VDO_CMD(vdm_hdr);
	cmd_type = PD_VDO_CMDT(vdm_hdr);
	svid = PD_VDO_VID(vdm_hdr);

	name = assign_vdm_cmd_name(cmd);

#ifdef CONFIG_USB_PD_ALT_MODE
	if (name == NULL && svid == USB_SID_DISPLAYPORT)
		name = assign_vdm_dp_cmd_name(cmd);
#endif	/* CONFIG_USB_PD_ALT_MODE */

	if (name == NULL)
		return;

#if PE_INFO_ENABLE
	if (cmd_type >= ARRAY_SIZE(pe_vdm_cmd_type_name))
		return;

	PE_INFO("%s:%s\n", name, pe_vdm_cmd_type_name[cmd_type]);
#endif /* PE_INFO_ENABLE */

#endif	/* PE_EVT_INFO_VDM_DIS */
}

static inline bool pd_process_sop_vdm(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_make_vdm_state_transit_sop(pd_port))
		return true;

#ifdef CONFIG_USB_PD_SVDM
	if (pd_process_custom_vdm(pd_port, true))
		return true;
#endif	/* CONFIG_USB_PD_SVDM */

	PE_DBG("Unknown SVDM\n");
	return false;
}

static inline bool pd_process_sop_prime_vdm(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	return pd_make_vdm_state_transit_cable(pd_port);
}

static inline bool pd_process_data_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;
	uint32_t vdm_hdr;
	struct pd_msg *pd_msg = pd_event->pd_msg;

	if (pd_event->msg != PD_DATA_VENDOR_DEF)
		return ret;

	vdm_hdr = pd_msg->payload[0];
	pd_port->curr_vdm_hdr = vdm_hdr;
	pd_port->curr_vdm_ops = PD_VDO_OPOS(vdm_hdr);
	pd_port->curr_vdm_svid = PD_VDO_VID(vdm_hdr);

	if (!PD_VDO_SVDM(vdm_hdr))
		return pd_process_uvdm(pd_port, pd_event);

	/* From Port Partner, copy curr_state from pd_state */
	if (PD_VDO_CMDT(vdm_hdr) == CMDT_INIT) {
		pd_port->pe_vdm_state = pd_port->pe_pd_state;
		pd_port->pe_state_curr = pd_port->pe_pd_state;

#if PE_DBG_RESET_VDM_DIS == 0
		PE_DBG("reset vdm_state\n");
#endif /* if PE_DBG_RESET_VDM_DIS == 0 */
	}

	print_vdm_msg(pd_port, pd_event);

	if (pd_msg->frame_type == TCPC_TX_SOP_PRIME)
		ret = pd_process_sop_prime_vdm(pd_port, pd_event);
	else
		ret = pd_process_sop_vdm(pd_port, pd_event);

	return ret;
}

/*
 * [BLOCK] Porcess PDM MSG
 */

static inline bool pd_process_dpm_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	if (pd_event->msg != PD_DPM_ACK)
		return false;

	if (pd_port->pe_data.vdm_state_flags &
		VDM_STATE_FLAG_BACK_READY_IF_DPM_ACK) {
		pe_transit_ready_state(pd_port);
		return true;
	}

	return false;
}

/*
 * [BLOCK] Porcess HW MSG
 */

static inline bool pd_process_hw_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct pe_data *pe_data = &pd_port->pe_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_event->msg == PD_HW_TX_DISCARD &&
		(pe_data->vdm_discard_retry_count < 10)) {
		PE_INFO("vdm_discard_retry\n");
		pe_data->vdm_discard_retry_flag = true;
		pe_data->vdm_discard_retry_count++;
	}

	switch (pd_event->msg) {
	case PD_HW_TX_FAILED:
	case PD_HW_TX_DISCARD:
		return pd_make_vdm_state_transit_nak(pd_port);

	case PD_HW_RETRY_VDM:
		if (pd_port->pe_data.vdm_state_timer)
			return pd_make_vdm_state_transit_nak(pd_port);
		PE_DBG("RetryVDM\n");
		return pd_process_sop_vdm(pd_port, pd_event);
	}

	return false;
}

/*
 * [BLOCK] Porcess PE MSG
 */

static inline bool pd_process_pe_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_PE_VDM_NOT_SUPPORT:
		break;

	case PD_PE_VDM_RESET:
		pd_port->pe_data.reset_vdm_state = true;
		break;

	default:
		return false;
	}

	return pd_make_vdm_state_transit_nak(pd_port);
}

/*
 * [BLOCK] Porcess Timer MSG
 */

static inline bool pd_process_timer_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_TIMER_VDM_MODE_ENTRY:
	case PD_TIMER_VDM_MODE_EXIT:
	case PD_TIMER_VDM_RESPONSE:
	case PD_TIMER_UVDM_RESPONSE:
		return pd_make_vdm_state_transit_nak(pd_port);

	default:
		return false;
	}
}

/*
 * [BLOCK] Porcess TCP MSG
 */

const uint8_t tcp_vdm_evt_init_state[] = {
	PE_DFP_UFP_VDM_IDENTITY_REQUEST, /* TCP_DPM_EVT_DISCOVER_ID */
	PE_DFP_VDM_SVIDS_REQUEST, /* TCP_DPM_EVT_DISCOVER_SVIDS */
	PE_DFP_VDM_MODES_REQUEST, /* TCP_DPM_EVT_DISCOVER_MODES */
	PE_DFP_VDM_MODE_ENTRY_REQUEST, /* TCP_DPM_EVT_ENTER_MODE */
	PE_DFP_VDM_MODE_EXIT_REQUEST, /* TCP_DPM_EVT_EXIT_MODE */
	PE_UFP_VDM_ATTENTION_REQUEST, /* TCP_DPM_EVT_ATTENTION */

#ifdef CONFIG_USB_PD_ALT_MODE
	PE_UFP_VDM_ATTENTION_REQUEST, /* TCP_DPM_EVT_DP_ATTENTION */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST, /* TCP_DPM_EVT_DP_STATUS_UPDATE */
	PE_DFP_VDM_DP_CONFIGURATION_REQUEST, /* TCP_DPM_EVT_DP_CONFIG */
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	PE_DFP_UVDM_SEND, /* TCP_DPM_EVT_UVDM */
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */
};

static inline bool pd_process_tcp_cable_event(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret;
	int tcp_ret;
#ifdef CONFIG_PD_DISCOVER_CABLE_ID
	bool role_check = true;

	if (pd_check_rev30(pd_port))
		role_check = pd_port->vconn_role;

	if (role_check) {
		ret = PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_DISCOVER_CABLE);
		tcp_ret = ret ? TCP_DPM_RET_SENT : TCP_DPM_RET_DENIED_NOT_READY;
	} else {
		ret = false;
		tcp_ret = TCP_DPM_RET_DENIED_WRONG_DATA_ROLE;
	}
#else
	ret = false;
	tcp_ret = TCP_DPM_RET_DENIED_UNKNOWN;
#endif	/* CONFIG_PD_DISCOVER_CABLE_ID */

	pd_notify_tcp_event_1st_result(pd_port, tcp_ret);
	return ret;
}

#ifdef CONFIG_USB_PD_ALT_MODE

static inline uint32_t tcpc_update_bits(
	uint32_t var, uint32_t update, uint32_t mask)
{
	return (var & (~mask)) | (update & mask);
}

static inline void pd_parse_tcp_dpm_evt_svdm(struct pd_port *pd_port)
{
	struct tcp_dpm_svdm_data *svdm_data =
		&pd_port->tcp_event.tcp_dpm_data.svdm_data;

	pd_port->mode_svid = svdm_data->svid;
	pd_port->mode_obj_pos = svdm_data->ops;
}

static inline void pd_parse_tcp_dpm_evt_dp_status(struct pd_port *pd_port)
{
	struct tcp_dpm_dp_data *dp_data_tcp =
		&pd_port->tcp_event.tcp_dpm_data.dp_data;

	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	pd_port->mode_svid = USB_SID_DISPLAYPORT;
	dp_data->local_status = tcpc_update_bits(
		dp_data->local_status,
		dp_data_tcp->val, dp_data_tcp->mask);
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

static inline void pd_parse_tcp_dpm_evt_dp_config(struct pd_port *pd_port)
{
	struct tcp_dpm_dp_data *dp_data_tcp =
		&pd_port->tcp_event.tcp_dpm_data.dp_data;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	pd_port->mode_svid = USB_SID_DISPLAYPORT;
	dp_data->local_config = tcpc_update_bits(
		dp_data->local_config, dp_data_tcp->val, dp_data_tcp->mask);
}

#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
static inline void pd_parse_tcp_dpm_evt_uvdm(struct pd_port *pd_port)
{
	struct tcp_dpm_custom_vdm_data *vdm_data =
		&pd_port->tcp_event.tcp_dpm_data.vdm_data;

	pd_port->uvdm_cnt = vdm_data->cnt;
	pd_port->uvdm_wait_resp = vdm_data->wait_resp;
	memcpy(pd_port->uvdm_data,
		vdm_data->vdos, sizeof(uint32_t) * vdm_data->cnt);

#ifdef CONFIG_USB_PD_SVDM
	if (pd_check_rev30(pd_port) &&
		(pd_port->uvdm_data[0] & VDO_SVDM_TYPE))
		pd_port->uvdm_data[0] |= VDO_SVDM_VERS(SVDM_REV20);
#endif	/* CONFIG_USB_PD_SVDM */
}
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

static inline void pd_parse_tcp_dpm_evt_from_tcpm(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
#ifdef CONFIG_USB_PD_KEEP_SVIDS
	case TCP_DPM_EVT_DISCOVER_SVIDS:
		pd_port->pe_data.remote_svid_list.cnt = 0;
		break;
#endif	/* CONFIG_USB_PD_KEEP_SVIDS */

#ifdef CONFIG_USB_PD_ALT_MODE
	case TCP_DPM_EVT_DISCOVER_MODES:
	case TCP_DPM_EVT_ENTER_MODE:
	case TCP_DPM_EVT_EXIT_MODE:
	case TCP_DPM_EVT_ATTENTION:
		pd_parse_tcp_dpm_evt_svdm(pd_port);
		break;

	case TCP_DPM_EVT_DP_ATTENTION:
		pd_parse_tcp_dpm_evt_dp_status(pd_port);
		break;

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	case TCP_DPM_EVT_DP_STATUS_UPDATE:
		pd_parse_tcp_dpm_evt_dp_status(pd_port);
		break;
	case TCP_DPM_EVT_DP_CONFIG:
		pd_parse_tcp_dpm_evt_dp_config(pd_port);
		break;
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	case TCP_DPM_EVT_UVDM:
		pd_parse_tcp_dpm_evt_uvdm(pd_port);
		break;
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */
	}
}

static inline bool pd_check_tcp_msg_valid(
		struct pd_port *pd_port, uint8_t new_state)
{
	return pd_make_vdm_state_transit(pd_port, new_state);
}

static inline bool pd_process_tcp_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	uint8_t new_state;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_event->msg == TCP_DPM_EVT_DISCOVER_CABLE)
		return pd_process_tcp_cable_event(pd_port, pd_event);

	if (!pd_check_pe_state_ready(pd_port)) {
		pd_notify_tcp_event_1st_result(
			pd_port, TCP_DPM_RET_DENIED_NOT_READY);
		PE_DBG("skip vdm_request, not ready_state (%d)\n",
					pd_port->pe_state_curr);
		return false;
	}

	new_state = pd_event->msg - TCP_DPM_EVT_DISCOVER_ID;
	if (new_state >= ARRAY_SIZE(tcp_vdm_evt_init_state)) {
		PD_BUG_ON(1);
		return false;
	}

	new_state = tcp_vdm_evt_init_state[new_state];

	if (!pd_check_tcp_msg_valid(pd_port, new_state)) {
		pd_notify_tcp_event_1st_result(
			pd_port, TCP_DPM_RET_DENIED_WRONG_DATA_ROLE);
		PE_DBG("skip vdm_request, WRONG DATA ROLE\n");
		return false;
	}

	if (pd_event->msg_sec == PD_TCP_FROM_TCPM)
		pd_parse_tcp_dpm_evt_from_tcpm(pd_port, pd_event);

	PE_TRANSIT_STATE(pd_port, new_state);
	pd_notify_tcp_event_1st_result(pd_port, TCP_DPM_RET_SENT);
	return true;
}

/*
 * [BLOCK] Process Policy Engine's VDM Message
 */

bool pd_process_event_vdm(struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_port->pe_data.vdm_discard_retry_flag = false;

	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DATA_MSG:
		return pd_process_data_msg(pd_port, pd_event);

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	case PD_EVT_HW_MSG:
		return pd_process_hw_msg(pd_port, pd_event);

	case PD_EVT_PE_MSG:
		return pd_process_pe_msg(pd_port, pd_event);

	case PD_EVT_TIMER_MSG:
		return pd_process_timer_msg(pd_port, pd_event);

	case PD_EVT_TCP_MSG:
		return pd_process_tcp_msg(pd_port, pd_event);
	}

	return false;
}
