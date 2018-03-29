/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_process_evt_vdm.c
 * Power Delvery Process Event For VDM
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
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"
#include "inc/pd_dpm_core.h"

#define VDM_CMD_STATE(cmd, cmd_type)	\
	((cmd & 0x1f) | ((cmd_type & 0x03) << 6))

#define VDM_CMD_INIT_STATE(cmd, next_state)	\
	{ VDM_CMD_STATE(cmd, CMDT_INIT), next_state }

#define VDM_CMD_ACK_STATE(cmd, next_state)	\
	{ VDM_CMD_STATE(cmd, CMDT_RSP_ACK), next_state }

#define VDM_CMD_NACK_STATE(cmd, next_state)	\
	{ VDM_CMD_STATE(cmd, CMDT_RSP_NAK), next_state }

#define VDM_CMD_BUSY_STATE(cmd, next_state)	\
	{ VDM_CMD_STATE(cmd, CMDT_RSP_BUSY), next_state }


/* UFP PD VDM Command's reactions */

DECL_PE_STATE_TRANSITION(PD_UFP_VDM_CMD) = {

	VDM_CMD_INIT_STATE(CMD_DISCOVER_IDENT, PE_UFP_VDM_GET_IDENTITY),
	VDM_CMD_INIT_STATE(CMD_DISCOVER_SVID, PE_UFP_VDM_GET_SVIDS),
	VDM_CMD_INIT_STATE(CMD_DISCOVER_MODES, PE_UFP_VDM_GET_MODES),
	VDM_CMD_INIT_STATE(CMD_ENTER_MODE, PE_UFP_VDM_EVALUATE_MODE_ENTRY),
	VDM_CMD_INIT_STATE(CMD_EXIT_MODE, PE_UFP_VDM_MODE_EXIT),
	/* CHECK IT LATER */
	VDM_CMD_INIT_STATE(CMD_ATTENTION, PE_UFP_VDM_ATTENTION_REQUEST),

#ifdef CONFIG_USB_PD_ALT_MODE
	VDM_CMD_INIT_STATE(CMD_DP_STATUS, PE_UFP_VDM_DP_STATUS_UPDATE),
	VDM_CMD_INIT_STATE(CMD_DP_CONFIG, PE_UFP_VDM_DP_CONFIGURE),
#endif
};
DECL_PE_STATE_REACTION(PD_UFP_VDM_CMD);

/* DFP PD VDM Command's reactions */

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_DISCOVER_ID) = {
	VDM_CMD_ACK_STATE(CMD_DISCOVER_IDENT,
		PE_DFP_UFP_VDM_IDENTITY_ACKED),
	VDM_CMD_NACK_STATE(CMD_DISCOVER_IDENT, PE_DFP_UFP_VDM_IDENTITY_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DISCOVER_IDENT, PE_DFP_UFP_VDM_IDENTITY_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_DISCOVER_ID);

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_DISCOVER_SVID) = {
	VDM_CMD_ACK_STATE(CMD_DISCOVER_SVID,
		PE_DFP_VDM_SVIDS_ACKED),
	VDM_CMD_NACK_STATE(CMD_DISCOVER_SVID, PE_DFP_VDM_SVIDS_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DISCOVER_SVID, PE_DFP_VDM_SVIDS_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_DISCOVER_SVID);

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_DISCOVER_MODES) = {
	VDM_CMD_ACK_STATE(CMD_DISCOVER_MODES,
			PE_DFP_VDM_MODES_ACKED),
	VDM_CMD_NACK_STATE(CMD_DISCOVER_MODES, PE_DFP_VDM_MODES_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DISCOVER_MODES, PE_DFP_VDM_MODES_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_DISCOVER_MODES);

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_ENTER_MODE) = {
	VDM_CMD_ACK_STATE(CMD_ENTER_MODE,
			PE_DFP_VDM_MODE_ENTRY_ACKED),
	VDM_CMD_NACK_STATE(CMD_ENTER_MODE, PE_DFP_VDM_MODE_ENTRY_NAKED),
	VDM_CMD_BUSY_STATE(CMD_ENTER_MODE, PE_DFP_VDM_MODE_ENTRY_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_ENTER_MODE);

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_EXIT_MODE) = {
	VDM_CMD_ACK_STATE(CMD_EXIT_MODE,
		PE_DFP_VDM_MODE_ENTRY_ACKED),
	VDM_CMD_NACK_STATE(CMD_EXIT_MODE,
		PE_DFP_VDM_MODE_ENTRY_NAKED),
	VDM_CMD_BUSY_STATE(CMD_EXIT_MODE,
		PE_VIRT_HARD_RESET),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_EXIT_MODE);

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_ATTENTION) = {
	VDM_CMD_INIT_STATE(CMD_ATTENTION,
		PE_DFP_VDM_ATTENTION_REQUEST),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_ATTENTION);

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_DP_STATUS) = {
	VDM_CMD_ACK_STATE(CMD_DP_STATUS,
			PE_DFP_VDM_DP_STATUS_UPDATE_ACKED),
	VDM_CMD_NACK_STATE(CMD_DP_STATUS, PE_DFP_VDM_DP_STATUS_UPDATE_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DP_STATUS, PE_DFP_VDM_DP_STATUS_UPDATE_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_DP_STATUS);

DECL_PE_STATE_TRANSITION(PD_DFP_VDM_DP_CONFIG) = {
	VDM_CMD_ACK_STATE(CMD_DP_CONFIG,
			PE_DFP_VDM_DP_CONFIGURATION_ACKED),
	VDM_CMD_NACK_STATE(CMD_DP_CONFIG, PE_DFP_VDM_DP_CONFIGURATION_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DP_CONFIG, PE_DFP_VDM_DP_CONFIGURATION_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_DP_CONFIG);

#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */


/* HW Event reactions */

DECL_PE_STATE_TRANSITION(PD_HW_MSG_TX_FAILED) = {
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	{PE_SRC_VDM_IDENTITY_REQUEST, PE_SRC_VDM_IDENTITY_NAKED},
#endif	/* PD_CAP_SRC_STARTUP_DISCOVERY_ID */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	{PE_DFP_CBL_VDM_IDENTITY_REQUEST, PE_DFP_CBL_VDM_IDENTITY_NAKED},
#endif
};
DECL_PE_STATE_REACTION(PD_HW_MSG_TX_FAILED);

/* DPM Event reactions */

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_ACK) = {
	{ PE_UFP_VDM_GET_IDENTITY, PE_UFP_VDM_SEND_IDENTITY },
	{ PE_UFP_VDM_GET_SVIDS, PE_UFP_VDM_SEND_SVIDS },
	{ PE_UFP_VDM_GET_MODES, PE_UFP_VDM_SEND_MODES },
	{ PE_UFP_VDM_MODE_EXIT, PE_UFP_VDM_MODE_EXIT_ACK},
	{ PE_UFP_VDM_EVALUATE_MODE_ENTRY, PE_UFP_VDM_MODE_ENTRY_ACK },
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_ACK);

DECL_PE_STATE_TRANSITION(PD_DPM_MSG_NAK) = {
	{PE_UFP_VDM_GET_IDENTITY, PE_UFP_VDM_GET_IDENTITY_NAK},
	{PE_UFP_VDM_GET_SVIDS, PE_UFP_VDM_GET_SVIDS_NAK},
	{PE_UFP_VDM_GET_MODES, PE_UFP_VDM_GET_MODES_NAK},
	{PE_UFP_VDM_MODE_EXIT, PE_UFP_VDM_MODE_EXIT_NAK},
	{PE_UFP_VDM_EVALUATE_MODE_ENTRY, PE_UFP_VDM_MODE_ENTRY_NAK},
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_NAK);

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
#endif
};
DECL_PE_STATE_REACTION(PD_DPM_MSG_DISCOVER_CABLE);
#endif

/* Source Startup Discover Cable ID */
#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
DECL_PE_STATE_TRANSITION(PD_SRC_VDM_DISCOVER_CABLE) = {
	VDM_CMD_ACK_STATE(CMD_DISCOVER_IDENT, PE_SRC_VDM_IDENTITY_ACKED),
	VDM_CMD_NACK_STATE(CMD_DISCOVER_IDENT, PE_SRC_VDM_IDENTITY_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DISCOVER_IDENT, PE_SRC_VDM_IDENTITY_NAKED),
};
DECL_PE_STATE_REACTION(PD_SRC_VDM_DISCOVER_CABLE);
#endif /* CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
DECL_PE_STATE_TRANSITION(PD_DFP_VDM_DISCOVER_CABLE) = {
	VDM_CMD_ACK_STATE(CMD_DISCOVER_IDENT, PE_DFP_CBL_VDM_IDENTITY_ACKED),
	VDM_CMD_NACK_STATE(CMD_DISCOVER_IDENT, PE_DFP_CBL_VDM_IDENTITY_NAKED),
	VDM_CMD_BUSY_STATE(CMD_DISCOVER_IDENT, PE_DFP_CBL_VDM_IDENTITY_NAKED),
};
DECL_PE_STATE_REACTION(PD_DFP_VDM_DISCOVER_CABLE);

#endif /* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */





/* Timer Event reactions */

DECL_PE_STATE_TRANSITION(PD_TIMER_VDM_RESPONSE) = {
#ifdef CONFIG_RT7207_ADAPTER
	{ PE_VDM_RT7207_REQUEST, PE_VDM_RT7207_NAKED },
#endif /* CONFIG_RT7207_ADAPTER */
	{ PE_DFP_UFP_VDM_IDENTITY_REQUEST, PE_DFP_UFP_VDM_IDENTITY_NAKED },
	{ PE_DFP_VDM_SVIDS_REQUEST, PE_DFP_VDM_SVIDS_NAKED },
	{ PE_DFP_VDM_MODES_REQUEST, PE_DFP_VDM_MODES_NAKED },
	{ PE_DFP_VDM_MODE_EXIT_REQUEST, PE_VIRT_HARD_RESET },
	{ PE_DFP_VDM_MODE_ENTRY_REQUEST, PE_DFP_VDM_MODE_ENTRY_NAKED },

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	{ PE_SRC_VDM_IDENTITY_REQUEST, PE_SRC_VDM_IDENTITY_NAKED },
#endif

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	{ PE_DFP_CBL_VDM_IDENTITY_REQUEST, PE_DFP_CBL_VDM_IDENTITY_NAKED },
#endif

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	{ PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST,
			PE_DFP_VDM_DP_STATUS_UPDATE_NAKED },
	{ PE_DFP_VDM_DP_CONFIGURATION_REQUEST,
			PE_DFP_VDM_DP_CONFIGURATION_NAKED },
#endif
};
DECL_PE_STATE_REACTION(PD_TIMER_VDM_RESPONSE);

/*
 * [BLOCK] Porcess Ctrl MSG
 */

static inline bool pd_process_ctrl_msg_good_crc(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {
	case PE_UFP_VDM_SEND_IDENTITY:
	case PE_UFP_VDM_GET_IDENTITY_NAK:
	case PE_UFP_VDM_SEND_SVIDS:
	case PE_UFP_VDM_GET_SVIDS_NAK:

	case PE_UFP_VDM_SEND_MODES:
	case PE_UFP_VDM_GET_MODES_NAK:
	case PE_UFP_VDM_MODE_ENTRY_ACK:
	case PE_UFP_VDM_MODE_ENTRY_NAK:
	case PE_UFP_VDM_MODE_EXIT_ACK:
	case PE_UFP_VDM_MODE_EXIT_NAK:

#ifdef CONFIG_USB_PD_ALT_MODE
	case PE_UFP_VDM_DP_STATUS_UPDATE:
	case PE_UFP_VDM_DP_CONFIGURE:
#endif
		PE_TRANSIT_READY_STATE(pd_port);
		return true;

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	case PE_SRC_VDM_IDENTITY_REQUEST:
		pd_port->power_cable_present = true;
		pd_port->dpm_flags &= DPM_FLAGS_CHECK_CABLE_ID;
		return false;
#endif

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	case PE_DFP_CBL_VDM_IDENTITY_REQUEST:
		pd_port->power_cable_present = true;
		pd_port->dpm_flags &= ~(DPM_FLAGS_CHECK_CABLE_ID |
				DPM_FLAGS_CHECK_CABLE_ID_DFP);
		return false;
#endif
	}

	return false;
}

static inline bool pd_process_ctrl_msg(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		return pd_process_ctrl_msg_good_crc(pd_port, pd_event);

	default:
		break;
	}

	return ret;
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

static const char *const pe_vdm_dp_cmd_name[] = {
	"DPStatus",
	"DPConfig",
};

static const char * const pe_vdm_cmd_type_name[] = {
	"INIT",
	"ACK",
	"NACK",
	"BUSY",
};
#endif /* if (PE_EVT_INFO_VDM_DIS == 0) */

static inline void print_vdm_msg(pd_port_t *pd_port, pd_event_t *pd_event)
{
#if (PE_EVT_INFO_VDM_DIS == 0)
	uint8_t cmd;
	uint8_t cmd_type;
	const char *name = NULL;
	uint32_t vdm_hdr = pd_event->pd_msg->payload[0];

	cmd = PD_VDO_CMD(vdm_hdr);
	cmd_type = PD_VDO_CMDT(vdm_hdr);

	if (cmd <= ARRAY_SIZE(pe_vdm_cmd_name))
		name = pe_vdm_cmd_name[cmd-1];
#ifdef CONFIG_USB_PD_ALT_MODE
	if (cmd >= CMD_DP_STATUS) {
		cmd -= CMD_DP_STATUS;
		if (cmd <= ARRAY_SIZE(pe_vdm_dp_cmd_name))
			name = pe_vdm_dp_cmd_name[cmd];
	}
#endif

	if (name == NULL)
		return;

	if (cmd_type >= ARRAY_SIZE(pe_vdm_cmd_type_name))
		return;

	PE_DBG("%s:%s\r\n", name, pe_vdm_cmd_type_name[cmd_type]);
#endif /* IF (PE_EVT_INFO_VDM_DIS == 0) */
}

static inline bool pd_process_ufp_vdm(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
#if 0
	if (pd_port->reset_vdm) {
		pd_port->reset_vdm = false;
		curr_engine_state = pd_pe_get_ready_state(port);
	}
#endif

	if (!pd_check_pe_state_ready(pd_port)) {
		PE_DBG("659 : invalid, current status\r\n");
		return false;
	}

	if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_UFP_VDM_CMD))
		return true;

	return false;
}

static inline bool pd_process_dfp_vdm(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	uint32_t vdm_hdr = pd_event->pd_msg->payload[0];

	if ((PD_VDO_CMDT(vdm_hdr) == CMDT_INIT) &&
			PD_VDO_CMD(vdm_hdr) == CMD_ATTENTION) {
#if 0
		if (port->reset_vdm) {
			port->reset_vdm = false;
			curr_engine_state = pd_pe_get_ready_state(port);
		}
#endif
		if (!pd_check_pe_state_ready(pd_port)) {
			PE_DBG("670 : invalid, current status\r\n");
			return false;
		}

		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_ATTENTION))
			return true;
	}

	switch (pd_port->pe_state_curr) {
#ifdef CONFIG_RT7207_ADAPTER
	case PE_VDM_RT7207_REQUEST:
		PE_TRANSIT_STATE(pd_port, PE_VDM_RT7207_ACKED);
		return true;
#endif /* CONFIG_RT7207_ADAPTER */

	case PE_DFP_UFP_VDM_IDENTITY_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_DISCOVER_ID))
			return true;

	case PE_DFP_VDM_SVIDS_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_DISCOVER_SVID))
			return true;

	case PE_DFP_VDM_MODES_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_DISCOVER_MODES))
			return true;

	case PE_DFP_VDM_MODE_ENTRY_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_ENTER_MODE))
			return true;

	case PE_DFP_VDM_MODE_EXIT_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT_VIRT(PD_DFP_VDM_EXIT_MODE))
			return true;

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	case PE_DFP_VDM_DP_STATUS_UPDATE_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_DP_STATUS))
			return true;

	case PE_DFP_VDM_DP_CONFIGURATION_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_DP_CONFIG))
			return true;
#endif
	}
	return false;
}

static inline bool pd_process_sop_vdm(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	if (pd_port->data_role == PD_ROLE_UFP)
		ret = pd_process_ufp_vdm(pd_port, pd_event);
	else
		ret = pd_process_dfp_vdm(pd_port, pd_event);

	if (!ret)
		PE_DBG("Unknown VDM\r\n");
	return ret;
}

static inline bool pd_process_sop_prime_vdm(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_port->pe_state_curr) {

#ifdef CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
	case PE_SRC_VDM_IDENTITY_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_SRC_VDM_DISCOVER_CABLE))
			return true;
#endif /* CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	case PE_DFP_CBL_VDM_IDENTITY_REQUEST:
		if (PE_MAKE_VDM_CMD_STATE_TRANSIT(PD_DFP_VDM_DISCOVER_CABLE))
			return true;
#endif /* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */
	}
	return false;
}

static inline bool pd_process_data_msg(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;
	uint32_t vdm_hdr;
	pd_msg_t *pd_msg = pd_event->pd_msg;

	if (pd_event->msg != PD_DATA_VENDOR_DEF)
		return ret;

	/* From Port Partner, copy curr_state from pd_state */
	vdm_hdr = pd_msg->payload[0];
	if (PD_VDO_SVDM(vdm_hdr) && PD_VDO_CMDT(vdm_hdr) == CMDT_INIT) {
		pd_port->pe_vdm_state = pd_port->pe_pd_state;
		pd_port->pe_state_curr = pd_port->pe_pd_state;
#if PE_DBG_RESET_VDM_DIS == 0
		PE_DBG("reset vdm_state\r\n");
#endif /* PE_DBG_RESET_VDM_DIS == 0 */
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

static inline bool pd_process_dpm_msg_ack(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_port->data_role == PD_ROLE_DFP) {
		switch (pd_port->pe_state_curr) {
		case PE_DFP_UFP_VDM_IDENTITY_ACKED:
		case PE_DFP_UFP_VDM_IDENTITY_NAKED:
		case PE_DFP_CBL_VDM_IDENTITY_ACKED:
		case PE_DFP_CBL_VDM_IDENTITY_NAKED:
		case PE_DFP_VDM_SVIDS_ACKED:
		case PE_DFP_VDM_SVIDS_NAKED:
		case PE_DFP_VDM_MODES_ACKED:
		case PE_DFP_VDM_MODES_NAKED:
		case PE_DFP_VDM_MODE_ENTRY_ACKED:
		case PE_DFP_VDM_MODE_EXIT_REQUEST:
		case PE_DFP_VDM_MODE_EXIT_ACKED:
		case PE_DFP_VDM_ATTENTION_REQUEST:
#ifdef CONFIG_RT7207_ADAPTER
		case PE_VDM_RT7207_ACKED:
		case PE_VDM_RT7207_NAKED:
#endif /* CONFIG_RT7207_ADAPTER */
			PE_TRANSIT_READY_STATE(pd_port);
			return true;
		default:
			return false;
		}
	} else
		return PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_ACK);
}

static inline bool pd_process_dpm_msg_vdm_request(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	/* TODO: We have to notify DPM this event is ignored */
	if (!pd_check_pe_state_ready(pd_port)) {
		PE_DBG("skip vdm_request, not ready_state (%d)\r\n",
						pd_port->pe_state_curr);
		return false;
	}

	/* TODO: Actually, UFP can send attention ... */
	if (pd_port->data_role != PD_ROLE_DFP) {
		PE_DBG("skip vdm_request, not dfp\r\n");
		return false;
	}
	PE_TRANSIT_STATE(pd_port, pd_event->msg_sec);
	return true;
}

static inline bool pd_process_dpm_msg(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_DPM_ACK:
		ret = pd_process_dpm_msg_ack(pd_port, pd_event);
		break;

	case PD_DPM_NAK:
		ret = PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_NAK);
		break;

	case PD_DPM_VDM_REQUEST:
		ret = pd_process_dpm_msg_vdm_request(pd_port, pd_event);
		break;

#ifdef CONFIG_PD_DISCOVER_CABLE_ID
	case PD_DPM_DISCOVER_CABLE_ID:
		ret = PE_MAKE_STATE_TRANSIT(PD_DPM_MSG_DISCOVER_CABLE);
		break;
#endif
	}
	return ret;
}

/*
 * [BLOCK] Porcess HW MSG
 */

static inline bool pd_process_hw_msg_retry_vdm(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	PE_DBG("RetryVDM\r\n");
	return pd_process_sop_vdm(pd_port, pd_event);
}

static inline bool pd_process_hw_msg(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_HW_TX_FAILED:
		ret = PE_MAKE_STATE_TRANSIT(PD_HW_MSG_TX_FAILED);
		break;

	case PD_HW_RETRY_VDM:
		ret = pd_process_hw_msg_retry_vdm(pd_port, pd_event);
		break;
	}

	return ret;
}

/*
 * [BLOCK] Porcess Timer MSG
 */

static inline bool pd_process_timer_msg(
		pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->msg) {
	case PD_TIMER_VDM_RESPONSE:
		return PE_MAKE_STATE_TRANSIT_VIRT(PD_TIMER_VDM_RESPONSE);

	default:
		return false;
	}
}

/*
 * [BLOCK] Process Policy Engine's VDM Message
 */

bool pd_process_event_vdm(pd_port_t *pd_port, pd_event_t *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DATA_MSG:
		return pd_process_data_msg(pd_port, pd_event);

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	case PD_EVT_HW_MSG:
		return pd_process_hw_msg(pd_port, pd_event);

	case PD_EVT_TIMER_MSG:
		return pd_process_timer_msg(pd_port, pd_event);
	}

	return false;
}
