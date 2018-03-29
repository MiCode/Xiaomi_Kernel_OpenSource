/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_dpm_alt_mode_dp.c
 *
 * Author: Sakya <jeff_chang@richtek.com>
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

#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "pd_dpm_prv.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_ALT_MODE

/* Display Port DFP_U / UFP_U */


/* DP_Role : DFP_D & UFP_D Both or DFP_D only */

#define DP_CHECK_DP_CONNECTED_MATCH(a, b)	\
	((a|b) == DPSTS_BOTH_CONNECTED)

#define DP_DFP_U_CHECK_ROLE_CAP_MATCH(a, b)	\
	((MODE_DP_PORT_CAP(a)|MODE_DP_PORT_CAP(b)) == MODE_DP_BOTH)

#define DP_SELECT_CONNECTED(b)		((b == DPSTS_DFP_D_CONNECTED) ? \
		DPSTS_UFP_D_CONNECTED : DPSTS_DFP_D_CONNECTED)

/*
 * If we support ufp_d & dfp_d both, we should choose another role.
 * If we don't support both, check dp_connected valid or not
 */
static inline bool dp_update_dp_connected_one(
	pd_port_t *pd_port, uint32_t dp_connected, uint32_t dp_local_connected)
{
	bool valid_connected;

	if (dp_local_connected != DPSTS_BOTH_CONNECTED) {
		valid_connected = DP_CHECK_DP_CONNECTED_MATCH(
			dp_local_connected, dp_connected);
	} else {
		valid_connected = true;
		pd_port->dp_status = DP_SELECT_CONNECTED(dp_connected);
	}

	return valid_connected;
}

/*
 * If we support ufp_d & dfp_d both, we should decide to use which role.
*/

static inline bool dp_update_dp_connected_both(
	pd_port_t *pd_port, uint32_t dp_connected, uint32_t dp_local_connected)
{
	if (dp_local_connected == DPSTS_BOTH_CONNECTED)
		pd_port->dp_status = pd_port->dp_second_connected;

	return true;
}

/* DP : DFP_U */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
enum pd_dfp_u_state {
	DP_DFP_U_NONE = 0,
	DP_DFP_U_STARTUP,
	DP_DFP_U_DISCOVER_ID,
	DP_DFP_U_DISCOVER_SVIDS,
	DP_DFP_U_DISCOVER_MODES,
	DP_DFP_U_ENTER_MODE,
	DP_DFP_U_STATUS_UPDATE,
	DP_DFP_U_WAIT_ATTENTION,
	DP_DFP_U_CONFIGURE,
	DP_DFP_U_OPERATION,
	DP_DFP_U_STATE_NR,

	DP_DFP_U_ERR = 0X10,

	DP_DFP_U_ERR_DISCOVER_ID_AMA_TYPE,
	DP_DFP_U_ERR_DISCOVER_ID_NAK_TIMEOUT,

	DP_DFP_U_ERR_DISCOVER_SVID_DP_SID,
	DP_DFP_U_ERR_DISCOVER_SVID_NAK_TIMEOUT,

	DP_DFP_U_ERR_DISCOVER_MODE_DP_SID,
	DP_DFP_U_ERR_DISCOVER_MODE_CAP,	/* NO SUPPORT UFP-D */
	DP_DFP_U_ERR_DISCOVER_MODE_NAK_TIMEROUT,

	DP_DFP_U_ERR_ENTER_MODE_DP_SID,
	DP_DFP_U_ERR_ENTER_MODE_NAK_TIMEOUT,

	DP_DFP_U_ERR_EXIT_MODE_DP_SID,
	DP_DFP_U_ERR_EXIT_MODE_NAK_TIMEOUT,

	DP_DFP_U_ERR_STATUS_UPDATE_DP_SID,
	DP_DFP_U_ERR_STATUS_UPDATE_NAK_TIMEOUT,
	DP_DFP_U_ERR_STATUS_UPDATE_ROLE,

	DP_DFP_U_ERR_CONFIGURE_SELECT_MODE,
};

#if DP_DBG_ENABLE
static const char * const dp_dfp_u_state_name[] = {
	"dp_dfp_u_none",
	"dp_dfp_u_startup",
	"dp_dfp_u_discover_id",
	"dp_dfp_u_discover_svids",
	"dp_dfp_u_discover_modes",
	"dp_dfp_u_enter_mode",
	"dp_dfp_u_status_update",
	"dp_dfp_u_wait_attention",
	"dp_dfp_u_configure",
	"dp_dfp_u_operation",
};
#endif /* DPM_DBG_ENABLE */

void dp_dfp_u_set_state(pd_port_t *pd_port, uint8_t state)
{
	pd_port->dp_dfp_u_state = state;

	if (pd_port->dp_dfp_u_state < DP_DFP_U_STATE_NR)
		DP_DBG("%s\r\n", dp_dfp_u_state_name[state]);
	else
		DP_DBG("dp_dfp_u_stop (%d)\r\n", state);
}

bool pd_dpm_request_enter_dp_mode(pd_port_t *pd_port)
{
	if (pd_port->data_role == PD_ROLE_DFP) {
		dp_dfp_u_set_state(pd_port, DP_DFP_U_DISCOVER_ID);
		return vdm_put_dpm_vdm_request_event(
			pd_port, PD_DPM_VDM_REQUEST_DISCOVER_ID);
	} else {
		/* TODO: If we only support DFP_D, should we send PR_SWAP? */
	}

	return false;
}

bool dp_dfp_u_notify_pe_startup(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data)
{
	if (pd_port->dpm_flags & DPM_FLAGS_CHECK_DP_MODE) {
		pd_port->dp_dfp_u_state = DP_DFP_U_STARTUP;

		pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_DP_MODE;
	}

	return true;
}

int dp_dfp_u_notify_pe_ready(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data, pd_event_t *pd_event)
{
	DPM_DBG("dp_dfp_u_notify_pe_ready\r\n");
	BUG_ON(pd_port->data_role != PD_ROLE_DFP);

	if (pd_port->dp_dfp_u_state != DP_DFP_U_STARTUP)
		return 0;

	return pd_dpm_request_enter_dp_mode(pd_port);
}

bool dp_dfp_u_notify_discover_id(pd_port_t *pd_port,
	svdm_svid_data_t *svid_data, pd_event_t *pd_event, bool ack)
{
	pd_msg_t *pd_msg = pd_event->pd_msg;

	if (pd_port->dp_dfp_u_state != DP_DFP_U_DISCOVER_ID)
		return true;

	if (!ack) {
		dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_DISCOVER_ID_NAK_TIMEOUT);
		return true;
	}

	switch (PD_IDH_PTYPE(pd_msg->payload[VDO_INDEX_IDH])) {
	case IDH_PTYPE_AMA:
		dp_dfp_u_set_state(pd_port, DP_DFP_U_DISCOVER_SVIDS);
		vdm_put_dpm_vdm_request_event(pd_port,
				PD_DPM_VDM_REQUEST_DISCOVER_SVIDS);
		break;

	default:
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_DISCOVER_ID_AMA_TYPE);
		break;
	}

	return true;
}

bool dp_dfp_u_notify_discover_svid(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data, bool ack)
{
	if (pd_port->dp_dfp_u_state != DP_DFP_U_DISCOVER_SVIDS)
		return false;

	if (!ack) {
		dp_dfp_u_set_state(pd_port,
			DP_DFP_U_ERR_DISCOVER_SVID_NAK_TIMEOUT);
		return false;
	}

	dp_dfp_u_set_state(pd_port, DP_DFP_U_DISCOVER_MODES);
	pd_port->mode_svid = USB_SID_DISPLAYPORT;
	vdm_put_dpm_vdm_request_event(
		pd_port, PD_DPM_VDM_REQUEST_DISCOVER_MODES);

	return true;
}
static inline bool is_dp_v1_cap_valid(uint32_t dp_cap)
{
	if  (((dp_cap >> 24) == 0) && ((dp_cap & 0x00ffffff) != 0))
		return true;
	return false;
}

#define DP_RECEPTACLE	(1 << 6)

/* priority : B -> A -> D/F -> C/E */
static inline int eval_dp_match_score(uint32_t local_mode,
	uint32_t remote_mode, uint32_t *local_dp_config,
	uint32_t *remote_dp_config)
{
	uint32_t local_pin_assignment = 0, remote_pin_assignment = 0;
	uint32_t common_pin_assignment;
	bool remote_is_ufp_pin_assignment = false;
	bool local_is_dfp_pin_assignment = false;
	int score = 0;

	if (!DP_DFP_U_CHECK_ROLE_CAP_MATCH(local_mode, remote_mode))
		return 0;

	if (((local_mode & MODE_DP_BOTH) == 0) ||
		((remote_mode & MODE_DP_BOTH) == 0))
		return 0;

	if (local_mode & DP_RECEPTACLE) {
		if (remote_mode & DP_RECEPTACLE) {
			if (local_mode & MODE_DP_SRC) {
				local_pin_assignment =
					MODE_DP_PIN_DFP(local_mode);
				remote_pin_assignment =
					MODE_DP_PIN_UFP(remote_mode);
				remote_is_ufp_pin_assignment = true;
				local_is_dfp_pin_assignment = true;
			} else {
				local_pin_assignment =
						MODE_DP_PIN_UFP(local_mode);
				remote_pin_assignment =
						MODE_DP_PIN_DFP(remote_mode);
				remote_is_ufp_pin_assignment = false;
				local_is_dfp_pin_assignment = false;
			}
		} else {
			/* remote is plug */
			if (local_mode & MODE_DP_SRC) {
				local_pin_assignment =
						MODE_DP_PIN_DFP(local_mode);
				remote_pin_assignment =
						MODE_DP_PIN_DFP(remote_mode);
				remote_is_ufp_pin_assignment = false;
				local_is_dfp_pin_assignment = true;
			}
		}
	} else {
		/* local is plug */
		if (remote_mode & DP_RECEPTACLE) {
			if (local_mode & MODE_DP_SNK) {
				local_pin_assignment =
						MODE_DP_PIN_DFP(local_mode);
				remote_pin_assignment =
						MODE_DP_PIN_DFP(remote_mode);
				remote_is_ufp_pin_assignment = false;
			}
		}
	}

	common_pin_assignment = local_pin_assignment & remote_pin_assignment;
	if (common_pin_assignment & (MODE_DP_PIN_C | MODE_DP_PIN_E)) {
		score = 1;
		if (common_pin_assignment & MODE_DP_PIN_E) {
			*local_dp_config = local_is_dfp_pin_assignment ?
				VDO_DP_DFP_CFG(DP_PIN_ASSIGN_SUPPORT_E,
				DP_SIG_DPV13) : VDO_DP_UFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
			*remote_dp_config = remote_is_ufp_pin_assignment ?
				VDO_DP_UFP_CFG(DP_PIN_ASSIGN_SUPPORT_E,
				DP_SIG_DPV13) : VDO_DP_DFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
		} else {
			*local_dp_config = local_is_dfp_pin_assignment ?
				VDO_DP_DFP_CFG(DP_PIN_ASSIGN_SUPPORT_C,
				DP_SIG_DPV13) : VDO_DP_UFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
			*remote_dp_config = remote_is_ufp_pin_assignment ?
				VDO_DP_UFP_CFG(DP_PIN_ASSIGN_SUPPORT_C,
				DP_SIG_DPV13) : VDO_DP_DFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
		}
	}
	if (common_pin_assignment & (MODE_DP_PIN_D | MODE_DP_PIN_F)) {
		score = 2;
		if (common_pin_assignment & MODE_DP_PIN_F) {
			*local_dp_config = local_is_dfp_pin_assignment ?
				VDO_DP_DFP_CFG(DP_PIN_ASSIGN_SUPPORT_F,
				DP_SIG_DPV13) : VDO_DP_UFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
			*remote_dp_config = remote_is_ufp_pin_assignment ?
				VDO_DP_UFP_CFG(DP_PIN_ASSIGN_SUPPORT_F,
				DP_SIG_DPV13) : VDO_DP_DFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
		} else {
			*local_dp_config = local_is_dfp_pin_assignment ?
				VDO_DP_DFP_CFG(DP_PIN_ASSIGN_SUPPORT_D,
				DP_SIG_DPV13) : VDO_DP_UFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
			*remote_dp_config = remote_is_ufp_pin_assignment ?
				VDO_DP_UFP_CFG(DP_PIN_ASSIGN_SUPPORT_D,
				DP_SIG_DPV13) : VDO_DP_DFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_DPV13);
		}
	}
	if ((MODE_DP_SIGNAL_SUPPORT(local_mode) & MODE_DP_GEN2) &&
		(MODE_DP_SIGNAL_SUPPORT(remote_mode) & MODE_DP_GEN2)) {
		if (common_pin_assignment & MODE_DP_PIN_A) {
			score = 3;
			*local_dp_config = local_is_dfp_pin_assignment ?
				VDO_DP_DFP_CFG(DP_PIN_ASSIGN_SUPPORT_A,
				DP_SIG_DPV13) : VDO_DP_UFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_GEN2);
			*remote_dp_config = remote_is_ufp_pin_assignment ?
				VDO_DP_UFP_CFG(DP_PIN_ASSIGN_SUPPORT_A,
				DP_SIG_DPV13) : VDO_DP_DFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_GEN2);
		}
		if (common_pin_assignment & MODE_DP_PIN_B) {
			score = 4;
			*local_dp_config = local_is_dfp_pin_assignment ?
				VDO_DP_DFP_CFG(DP_PIN_ASSIGN_SUPPORT_B,
				DP_SIG_DPV13) : VDO_DP_UFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_GEN2);
			*remote_dp_config = remote_is_ufp_pin_assignment ?
				VDO_DP_UFP_CFG(DP_PIN_ASSIGN_SUPPORT_B,
				DP_SIG_DPV13) : VDO_DP_DFP_CFG(
				DP_PIN_ASSIGN_SUPPORT_E, DP_SIG_GEN2);
		}
	}
	return score;
}

static inline uint8_t dp_dfp_u_select_mode(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data)
{
	uint32_t dp_local_mode, dp_remote_mode,
			remote_dp_config = 0, local_dp_config = 0;
	svdm_mode_t *remote, *local;
	int i, j;
	int match_score, best_match_score = 0;
	int local_index = -1, remote_index = -1;

	local = &svid_data->local_mode;
	remote = &svid_data->remote_mode;

	/* TODO: Evaluate All Modes later ... */
	for (j = 0; j < local->mode_cnt; j++) {
		dp_local_mode = local->mode_vdo[j];
		if (!is_dp_v1_cap_valid(dp_local_mode))
			continue;
		for (i = 0; i < remote->mode_cnt; i++) {
			dp_remote_mode = remote->mode_vdo[i];
			if (!is_dp_v1_cap_valid(dp_remote_mode))
				continue;
			match_score = eval_dp_match_score(dp_local_mode,
				dp_remote_mode, &local_dp_config,
				&remote_dp_config);
			if (match_score >  best_match_score) {
				local_index = j;
				remote_index = i;
				pd_port->remote_dp_config = remote_dp_config;
				pd_port->local_dp_config = local_dp_config;
			}
		}
	}

	/*
	dp_mode = svid_data->remote_mode.mode_vdo[0];
	dp_local_mode = svid_data->local_mode.mode_vdo[0];

	cap_match = DP_DFP_U_CHECK_ROLE_CAP_MATCH(dp_mode, dp_local_mode);
	return cap_match ? 1 : 0;*/
	return remote_index + 1;
}

bool dp_dfp_u_notify_discover_modes(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data, bool ack)
{
	if (pd_port->dp_dfp_u_state != DP_DFP_U_DISCOVER_MODES)
		return false;

	if (!ack) {
		dp_dfp_u_set_state(pd_port,
			DP_DFP_U_ERR_DISCOVER_MODE_NAK_TIMEROUT);
		return false;
	}

	if (svid_data->remote_mode.mode_cnt == 0) {
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_DISCOVER_MODE_DP_SID);
		return false;
	}

	pd_port->mode_obj_pos = dp_dfp_u_select_mode(pd_port, svid_data);

	if (pd_port->mode_obj_pos == 0) {
		DPM_DBG("Can't find match mode\r\n");
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_DISCOVER_MODE_CAP);
		return false;
	}

	dp_dfp_u_set_state(pd_port, DP_DFP_U_ENTER_MODE);
	vdm_put_dpm_vdm_request_event(
			pd_port, PD_DPM_VDM_REQUEST_ENTRY_MODE);
	return true;
}

bool dp_dfp_u_notify_enter_mode(pd_port_t *pd_port,
	svdm_svid_data_t *svid_data, uint8_t ops, bool ack)
{
	if (pd_port->dp_dfp_u_state != DP_DFP_U_ENTER_MODE)
		return true;

	if (!ack) {
		dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_ENTER_MODE_NAK_TIMEOUT);
		return true;
	}

	if (svid_data->active_mode == 0) {
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_ENTER_MODE_DP_SID);
		return false;
	}

	dp_dfp_u_set_state(pd_port, DP_DFP_U_STATUS_UPDATE);

	pd_port->dp_status = pd_port->dp_first_connected;
	vdm_put_dpm_vdm_request_event(
		pd_port, PD_DPM_VDM_REQUEST_DP_STATUS_UPDATE);

	return true;
}

bool dp_dfp_u_notify_exit_mode(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data, uint8_t ops)
{
	if (pd_port->dp_dfp_u_state <= DP_DFP_U_ENTER_MODE)
		return false;

	if (svid_data->svid != USB_SID_DISPLAYPORT)
		return false;

	dp_dfp_u_set_state(pd_port, DP_DFP_U_NONE);
	return true;
}

static inline bool dp_dfp_u_select_pin_mode(pd_port_t *pd_port)
{
	uint32_t dp_local_connected;
	uint32_t dp_mode[2], pin_cap[2];

	uint32_t pin_caps, signal;

	svdm_svid_data_t *svid_data =
		dpm_get_svdm_svid_data(pd_port, USB_SID_DISPLAYPORT);

	if (svid_data == NULL)
		return false;

	dp_mode[0] = SVID_DATA_LOCAL_MODE(svid_data, 0);
	dp_mode[1] = SVID_DATA_DFP_GET_ACTIVE_MODE(svid_data);

	dp_local_connected = PD_VDO_DPSTS_CONNECT(pd_port->dp_status);

	switch (dp_local_connected) {
	case DPSTS_DFP_D_CONNECTED:
		pin_cap[0] = PD_DP_DFP_D_PIN_CAPS(dp_mode[0]);
		pin_cap[1] = PD_DP_UFP_D_PIN_CAPS(dp_mode[1]);
		break;

	case DPSTS_UFP_D_CONNECTED:
		/* TODO: */
		DP_ERR("select_pin error0\n");
		return false;

	default:
		DP_ERR("select_pin error1\n");
		return false;
	}

	PE_DBG("modes=0x%x 0x%x\r\n", dp_mode[0], dp_mode[1]);
	PE_DBG("pins=0x%x 0x%x\r\n", pin_cap[0], pin_cap[1]);

	pin_caps = pin_cap[0] & pin_cap[1];

	/* if don't want multi-function then ignore those pin configs */
	if (!PD_VDO_DPSTS_MF_PREF(pd_port->dp_ufp_u_attention))
		pin_caps &= ~MODE_DP_PIN_MF_MASK;

	/* TODO: If DFP & UFP driver USB Gen2 signal */
	signal = DP_SIG_DPV13;
	pin_caps &= ~MODE_DP_PIN_BR2_MASK;

	if (!pin_caps) {
		DP_ERR("select_pin error2\n");
		return false;
	}

	/* Priority */
	if (pin_caps & MODE_DP_PIN_D)
		pin_caps = MODE_DP_PIN_D;
	else if (pin_caps & MODE_DP_PIN_F)
		pin_caps = MODE_DP_PIN_F;
	else if (pin_caps & MODE_DP_PIN_C)
		pin_caps = MODE_DP_PIN_C;
	else if (pin_caps & MODE_DP_PIN_E)
		pin_caps = MODE_DP_PIN_E;

	if (dp_local_connected == DPSTS_DFP_D_CONNECTED) {
		pd_port->local_dp_config = VDO_DP_DFP_CFG(pin_caps, signal);
		pd_port->remote_dp_config = VDO_DP_UFP_CFG(pin_caps, signal);
	} else {
		pd_port->local_dp_config = VDO_DP_UFP_CFG(pin_caps, signal);
		pd_port->remote_dp_config = VDO_DP_DFP_CFG(pin_caps, signal);
	}

	return true;
}

void dp_dfp_u_request_dp_configuration(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (!dp_dfp_u_select_pin_mode(pd_port)) {
		dp_dfp_u_set_state(pd_port,
			DP_DFP_U_ERR_CONFIGURE_SELECT_MODE);
		return;
	}

	tcpci_dp_notify_config_start(pd_port->tcpc_dev);

	dp_dfp_u_set_state(pd_port, DP_DFP_U_CONFIGURE);

	vdm_put_dpm_vdm_request_event(
		pd_port, PD_DPM_VDM_REQUEST_DP_CONFIG);
}

static inline bool dp_dfp_u_update_dp_connected(
	pd_port_t *pd_port, uint32_t dp_status)
{
	bool valid_connected = false;
	uint32_t dp_connected, dp_local_connected;

	dp_connected = PD_VDO_DPSTS_CONNECT(dp_status);
	dp_local_connected = PD_VDO_DPSTS_CONNECT(pd_port->dp_status);

	switch (dp_connected) {
	case DPSTS_DFP_D_CONNECTED:
	case DPSTS_UFP_D_CONNECTED:
		valid_connected = dp_update_dp_connected_one(
			pd_port, dp_connected, dp_local_connected);

		if (!valid_connected)
			dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_STATUS_UPDATE_ROLE);
		break;

	case DPSTS_DISCONNECT:
		dp_dfp_u_set_state(pd_port, DP_DFP_U_WAIT_ATTENTION);
		break;

	case DPSTS_BOTH_CONNECTED:
		dp_update_dp_connected_both(pd_port,
				dp_connected, dp_local_connected);

		if (pd_port->dp_dfp_u_state == DP_DFP_U_STATUS_UPDATE) {
			vdm_put_dpm_vdm_request_event(
				pd_port, PD_DPM_VDM_REQUEST_DP_STATUS_UPDATE);
		} else {
			valid_connected = true;
		}
		break;
	}

	return valid_connected;
}

bool dp_dfp_u_notify_dp_status_update(
	pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	uint32_t svid;
	bool oper_mode = false;
	bool valid_connected;
	uint32_t dp_status;
	pd_msg_t *pd_msg = pd_event->pd_msg;

	switch (pd_port->dp_dfp_u_state) {
	case DP_DFP_U_OPERATION:
		oper_mode = true;
	case DP_DFP_U_STATUS_UPDATE:
		break;

	default:
		return false;
	}

	if (!ack) {
		tcpci_dp_notify_status_update_done(pd_port->tcpc_dev, 0, false);
		dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_STATUS_UPDATE_NAK_TIMEOUT);
		return false;
	}

	svid = PD_VDO_VID(pd_msg->payload[0]);
	if (svid != USB_SID_DISPLAYPORT) {
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_STATUS_UPDATE_DP_SID);
		return true;
	}

	dp_status = pd_event->pd_msg->payload[1];
	pd_port->dp_ufp_u_attention = (uint8_t) dp_status;
	DPM_DBG("dp_status: 0x%x\r\n", dp_status);

	if (oper_mode) {
		tcpci_dp_notify_status_update_done(
				pd_port->tcpc_dev, dp_status, ack);
	} else {
		valid_connected =
			dp_dfp_u_update_dp_connected(pd_port, dp_status);
		if (valid_connected)
			dp_dfp_u_request_dp_configuration(pd_port, pd_event);
	}
	return true;
}

bool dp_dfp_u_notify_dp_configuration(
	pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	const uint32_t local_cfg = pd_port->local_dp_config;
	const uint32_t remote_cfg = pd_port->remote_dp_config;

	if (ack)
		dp_dfp_u_set_state(pd_port, DP_DFP_U_OPERATION);
	else
		DP_ERR("config failed: 0x%0x\r\n", remote_cfg);

	tcpci_dp_notify_config_done(
		pd_port->tcpc_dev, local_cfg, remote_cfg, ack);

	return true;
}

bool dp_dfp_u_notify_attention(pd_port_t *pd_port,
	svdm_svid_data_t *svid_data, pd_event_t *pd_event)
{
	bool valid_connected;
	uint32_t dp_status = pd_event->pd_msg->payload[1];

	DPM_DBG("dp_status: 0x%x\r\n", dp_status);

	pd_port->dp_ufp_u_attention = (uint8_t) dp_status;
	switch (pd_port->dp_dfp_u_state) {
	case DP_DFP_U_WAIT_ATTENTION:
		valid_connected =
			dp_dfp_u_update_dp_connected(pd_port, dp_status);
		if (valid_connected)
			dp_dfp_u_request_dp_configuration(pd_port, pd_event);
		break;

	case DP_DFP_U_OPERATION:
		tcpci_dp_attention(pd_port->tcpc_dev, dp_status);
		break;
	}

	return true;
}

#endif /* CONFIG_USB_PD_ALT_MODE_DFP */

/* DP : UFP_U */

#if DPM_DBG_ENABLE
static const char * const dp_ufp_u_state_name[] = {
	"dp_ufp_u_none",
	"dp_ufp_u_startup",
	"dp_ufp_u_wait",
	"dp_dfp_u_operation",
};
#endif /* DPM_DBG_ENABLE */

static void dp_ufp_u_set_state(pd_port_t *pd_port, uint8_t state)
{
	pd_port->dp_ufp_u_state = state;

	if (pd_port->dp_ufp_u_state < DP_UFP_U_STATE_NR)
		DPM_DBG("%s\r\n", dp_ufp_u_state_name[state]);
	else
		DPM_DBG("dp_ufp_u_stop\r\n");
}

void dp_ufp_u_request_enter_mode(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data, uint8_t ops)
{
	pd_port->dp_status = pd_port->dp_first_connected;

	if (pd_port->dp_first_connected == DPSTS_DISCONNECT)
		dp_ufp_u_set_state(pd_port, DP_UFP_U_STARTUP);
	else
		dp_ufp_u_set_state(pd_port, DP_UFP_U_WAIT);
}

void dp_ufp_u_request_exit_mode(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data, uint8_t ops)
{
	pd_port->dp_status = 0;
	dp_ufp_u_set_state(pd_port, DP_UFP_U_NONE);
}

static inline bool dp_ufp_u_update_dp_connected(
	pd_port_t *pd_port, uint32_t dp_status)
{
	bool valid_connected;
	uint32_t dp_connected, dp_local_connected;

	dp_connected = PD_VDO_DPSTS_CONNECT(dp_status);
	dp_local_connected = PD_VDO_DPSTS_CONNECT(pd_port->dp_status);

	switch (dp_connected) {
	case DPSTS_DFP_D_CONNECTED:
	case DPSTS_UFP_D_CONNECTED:
		valid_connected = dp_update_dp_connected_one(
			pd_port, dp_connected, dp_local_connected);
		break;

	case DPSTS_BOTH_CONNECTED:
		valid_connected = dp_update_dp_connected_both(
			pd_port, dp_connected, dp_local_connected);
		break;

	default:
		valid_connected = false;
		break;
	}

	return valid_connected;
}

int dp_ufp_u_request_dp_status(pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ack = true;
	uint32_t dp_status;

	BUG_ON(pd_event->pd_msg == NULL);
	dp_status = pd_event->pd_msg->payload[1];

	switch (pd_port->dp_ufp_u_state) {
	case DP_UFP_U_WAIT:
		ack = dp_ufp_u_update_dp_connected(pd_port, dp_status);
		break;

	case DP_UFP_U_STARTUP:
	case DP_UFP_U_OPERATION:
		tcpci_dp_status_update(pd_port->tcpc_dev, dp_status);
		break;
	}

	if (ack) {
		return pd_reply_svdm_request(pd_port,
			pd_event, CMDT_RSP_ACK, 1, &pd_port->dp_status);
	} else {
		return pd_reply_svdm_request_simply(
			pd_port, pd_event, CMDT_RSP_NAK);
	}
}

bool dp_ufp_u_is_valid_dp_config(pd_port_t *pd_port, uint32_t dp_config)
{
	/* TODO: Check it later .... */
	uint32_t sel_config;
	bool retval = false;
	svdm_svid_data_t *svid_data = &pd_port->svid_data[0];
	uint32_t local_mode = svid_data->local_mode.mode_vdo[0];

	sel_config = MODE_DP_PORT_CAP(dp_config);
	switch (sel_config) {
	case 0: /* USB */
		retval = true;
		break;
	case MODE_DP_SRC:
		if ((MODE_DP_PORT_CAP(local_mode) & MODE_DP_SRC) == 0) {
			retval = false;
			break;
		}
		if ((((dp_config & local_mode) >> 8) & 0xff) == 0) {
			retval = false;
			break;
		}
		break;
	case MODE_DP_SNK:
		if ((MODE_DP_PORT_CAP(local_mode) & MODE_DP_SNK) == 0) {
			retval = false;
			break;
		}
		if ((((dp_config & local_mode) >> 16) & 0xff) == 0) {
			retval = false;
			break;
		}
		break;
	}

	return retval;
}

int dp_ufp_u_request_dp_config(pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ack = false;
	uint32_t dp_config;

	BUG_ON(pd_event->pd_msg == NULL);
	dp_config = pd_event->pd_msg->payload[1];

	DPM_DBG("dp_config: 0x%x\r\n", dp_config);

	switch (pd_port->dp_ufp_u_state) {
	case DP_UFP_U_STARTUP:
	case DP_UFP_U_WAIT:
	case DP_UFP_U_OPERATION:
		ack = dp_ufp_u_is_valid_dp_config(pd_port, dp_config);

		if (ack) {
			tcpci_dp_configure(pd_port->tcpc_dev, dp_config);
			dp_ufp_u_set_state(pd_port, DP_UFP_U_OPERATION);
		}
		break;
	}

	return pd_reply_svdm_request_simply(
		pd_port, pd_event, ack ? CMDT_RSP_ACK : CMDT_RSP_NAK);
}

void dp_ufp_u_send_dp_attention(pd_port_t *pd_port, pd_event_t *pd_event)
{
	svdm_svid_data_t *svid_data;

	switch (pd_port->dp_ufp_u_state) {
	case DP_UFP_U_STARTUP:
	case DP_UFP_U_OPERATION:
		svid_data = dpm_get_svdm_svid_data(
				pd_port, USB_SID_DISPLAYPORT);
		BUG_ON(svid_data == NULL);

		pd_send_vdm_dp_attention(pd_port, TCPC_TX_SOP,
			svid_data->active_mode, pd_port->dp_status);
		break;
	}
}

/* ---- UFP : DP Only ---- */

int pd_dpm_ufp_request_dp_status(pd_port_t *pd_port, pd_event_t *pd_event)
{
	return dp_ufp_u_request_dp_status(pd_port, pd_event);
}

int pd_dpm_ufp_request_dp_config(pd_port_t *pd_port, pd_event_t *pd_event)
{
	return dp_ufp_u_request_dp_config(pd_port, pd_event);
}

void pd_dpm_ufp_send_dp_attention(pd_port_t *pd_port, pd_event_t *pd_event)
{
	dp_ufp_u_send_dp_attention(pd_port, pd_event);
}

/* ---- DFP : DP Only ---- */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

void pd_dpm_dfp_send_dp_status_update(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_vdm_dp_status(pd_port, TCPC_TX_SOP,
		pd_port->mode_obj_pos, 1, &pd_port->dp_status);
}

void pd_dpm_dfp_inform_dp_status_update(
	pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	dp_dfp_u_notify_dp_status_update(pd_port, pd_event, ack);
}

void pd_dpm_dfp_send_dp_configuration(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_vdm_dp_config(pd_port, TCPC_TX_SOP,
		pd_port->mode_obj_pos, 1, &pd_port->remote_dp_config);
}

void pd_dpm_dfp_inform_dp_configuration(pd_port_t *pd_port,
					pd_event_t *pd_event, bool ack)
{
	dp_dfp_u_notify_dp_configuration(pd_port, pd_event, ack);
}

#endif /* CONFIG_USB_PD_ALT_MODE_DFP */

bool dp_reset_state(pd_port_t *pd_port, svdm_svid_data_t *svid_data)
{
	/* TODO: */
	return true;
}

#endif	/* CONFIG_USB_PD_ALT_MODE */
#endif	/* CONFIG_USB_POWER_DELIVERY */
