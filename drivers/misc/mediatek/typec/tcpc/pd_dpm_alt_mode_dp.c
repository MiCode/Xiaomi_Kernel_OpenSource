// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
static inline bool dp_update_dp_connected_one(struct pd_port *pd_port,
			uint32_t dp_connected, uint32_t dp_local_connected)
{
	bool valid_connected;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	if (dp_local_connected != DPSTS_BOTH_CONNECTED) {
		valid_connected = DP_CHECK_DP_CONNECTED_MATCH(
			dp_connected, dp_local_connected);
	} else {
		valid_connected = true;
		dp_data->local_status = DP_SELECT_CONNECTED(dp_connected);
	}

	return valid_connected;
}

/*
 * If we support ufp_d & dfp_d both, we should decide to use which role.
 * For dfp_u, the dp_connected is invalid, and re-send dp_status.
 * For ufp_u, the dp_connected is valid, and wait for dp_status from dfp_u
 *
 * If we don't support both, the dp_connected always is valid
 *
 */

static inline bool dp_update_dp_connected_both(struct pd_port *pd_port,
			uint32_t dp_local_connected, bool both_connected_valid)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	bool valid_connected = true;

	if (dp_local_connected == DPSTS_BOTH_CONNECTED) {
		dp_data->local_status = pd_port->dp_second_connected;
		valid_connected = both_connected_valid;
	}

	return valid_connected;
}

/* DP : DFP_U */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
#if DP_DBG_ENABLE
static const char * const dp_dfp_u_state_name[] = {
	"dp_dfp_u_none",
	"dp_dfp_u_discover_id",
	"dp_dfp_u_discover_svids",
	"dp_dfp_u_discover_modes",
	"dp_dfp_u_enter_mode",
	"dp_dfp_u_status_update",
	"dp_dfp_u_wait_attention",
	"dp_dfp_u_configure",
	"dp_dfp_u_operation",
};
#endif /* DP_DBG_ENABLE */

void dp_dfp_u_set_state(struct pd_port *pd_port, uint8_t state)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	dp_data->dfp_u_state = state;

	if (dp_data->dfp_u_state < DP_DFP_U_STATE_NR)
		DP_DBG("%s\n", dp_dfp_u_state_name[state]);
	else
		DP_DBG("dp_dfp_u_stop (%d)\n", state);
}

bool dp_dfp_u_notify_pe_startup(
		struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	if (!(pd_port->id_vdos[0] & PD_IDH_MODAL_SUPPORT))
		return true;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_ENTER_DP_MODE)
		dp_dfp_u_set_state(pd_port, DP_DFP_U_DISCOVER_ID);

	return true;
}

int dp_dfp_u_notify_pe_ready(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("%s\n", __func__);

	if (pd_port->data_role != PD_ROLE_DFP)
		return 0;

	if (dp_data->dfp_u_state != DP_DFP_U_DISCOVER_MODES)
		return 0;

	/* Check Cable later */
	pd_port->mode_svid = USB_SID_DISPLAYPORT;
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DISCOVER_MODES);
	return 1;
}

bool dp_notify_pe_shutdown(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	if (svid_data->active_mode) {
		pd_send_vdm_exit_mode(pd_port, TCPC_TX_SOP,
			svid_data->svid, svid_data->active_mode);
	}

	return true;
}

bool dp_dfp_u_notify_discover_id(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, bool ack)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	uint32_t *payload = pd_get_msg_data_payload(pd_port);

	if (dp_data->dfp_u_state != DP_DFP_U_DISCOVER_ID)
		return true;

	if (!ack) {
		dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_DISCOVER_ID_NAK_TIMEOUT);
		return true;
	}

	if (payload[VDO_INDEX_IDH] & PD_IDH_MODAL_SUPPORT)
		dp_dfp_u_set_state(pd_port, DP_DFP_U_DISCOVER_SVIDS);
	else
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_DISCOVER_ID_TYPE);

	return true;
}

bool dp_dfp_u_notify_discover_svid(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, bool ack)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	if (dp_data->dfp_u_state != DP_DFP_U_DISCOVER_SVIDS)
		return false;

	if (!ack) {
		dp_dfp_u_set_state(pd_port,
			DP_DFP_U_ERR_DISCOVER_SVID_NAK_TIMEOUT);
		return false;
	}

	if (!svid_data->exist) {
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_DISCOVER_SVID_DP_SID);
		return false;
	}

	dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_CABLE_FLOW);
	dp_dfp_u_set_state(pd_port, DP_DFP_U_DISCOVER_MODES);
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

static inline uint8_t dp_dfp_u_select_mode(struct pd_port *pd_port,
	struct dp_data *dp_data, struct svdm_svid_data *svid_data)
{
	uint32_t dp_local_mode, dp_remote_mode,
			remote_dp_config = 0, local_dp_config = 0;
	struct svdm_mode *remote, *local;
	int i, j;
	int match_score, best_match_score = 0;
	int local_index = -1, remote_index = -1;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

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
				dp_data->local_config = local_dp_config;
				dp_data->remote_config = remote_dp_config;
			}
		}
	}

#if DP_INFO_ENABLE
	for (i = 0; i < svid_data->remote_mode.mode_cnt; i++) {
		DP_INFO("Mode%d=0x%08x\n", i,
			svid_data->remote_mode.mode_vdo[i]);
	}

	DP_INFO("SelectMode:%d\n", remote_index);
#endif	/* DP_INFO_ENABLE */

	/*
	 * dp_mode = svid_data->remote_mode.mode_vdo[0];
	 * dp_local_mode = svid_data->local_mode.mode_vdo[0];
	 * cap_match = DP_DFP_U_CHECK_ROLE_CAP_MATCH(dp_mode, dp_local_mode),
	 * return cap_match ? 1 : 0;
	 */
	return remote_index + 1;
}

bool dp_dfp_u_notify_discover_modes(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, bool ack)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dp_data->dfp_u_state != DP_DFP_U_DISCOVER_MODES)
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

	pd_port->mode_obj_pos = dp_dfp_u_select_mode(
		pd_port, dp_data, svid_data);

	if (pd_port->mode_obj_pos == 0) {
		DPM_DBG("Can't find match mode\n");
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_DISCOVER_MODE_CAP);
		return false;
	}

	dp_dfp_u_set_state(pd_port, DP_DFP_U_ENTER_MODE);
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_ENTER_MODE);
	return true;
}

bool dp_dfp_u_notify_enter_mode(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, uint8_t ops, bool ack)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	if (dp_data->dfp_u_state != DP_DFP_U_ENTER_MODE)
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

	dp_data->local_status = pd_port->dp_first_connected;
	dp_dfp_u_set_state(pd_port, DP_DFP_U_STATUS_UPDATE);

#ifdef CONFIG_USB_PD_DBG_DP_DFP_D_AUTO_UPDATE
	/*
	 * For Real Product,
	 * DFP_U should not send status_update until USB status is changed
	 *	From : "USB Mode, USB Configration"
	 *	To : "DisplayPlay Mode, USB Configration"
	 *
	 * After USB status is changed,
	 * please call following function to continue DFP_U flow.
	 * tcpm_dpm_dp_status_update(tcpc, 0, 0, NULL)
	 */

	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DP_STATUS_UPDATE);
#endif	/* CONFIG_USB_PD_DBG_DP_DFP_D_AUTO_UPDATE */

	return true;
}

bool dp_dfp_u_notify_exit_mode(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, uint8_t ops)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	if (dp_data->dfp_u_state <= DP_DFP_U_ENTER_MODE)
		return false;

	if (svid_data->svid != USB_SID_DISPLAYPORT)
		return false;

	memset(dp_data, 0, sizeof(struct dp_data));
	dp_dfp_u_set_state(pd_port, DP_DFP_U_NONE);
	return true;
}

static inline bool dp_dfp_u_select_pin_mode(struct pd_port *pd_port)
{
	uint32_t dp_local_connected;
	uint32_t dp_mode[2], pin_cap[2];
	uint32_t pin_caps, signal;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct svdm_svid_data *svid_data =
		dpm_get_svdm_svid_data(pd_port, USB_SID_DISPLAYPORT);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (svid_data == NULL)
		return false;

	dp_mode[0] = SVID_DATA_LOCAL_MODE(svid_data, 0);
	dp_mode[1] = SVID_DATA_DFP_GET_ACTIVE_MODE(svid_data);

	dp_local_connected = PD_VDO_DPSTS_CONNECT(dp_data->local_status);

	switch (dp_local_connected) {
	case DPSTS_DFP_D_CONNECTED:
		pin_cap[0] = PD_DP_DFP_D_PIN_CAPS(dp_mode[0]);
		pin_cap[1] = PD_DP_UFP_D_PIN_CAPS(dp_mode[1]);
		break;

	case DPSTS_UFP_D_CONNECTED:
		/* TODO: checkit next version*/
		pin_cap[0] = PD_DP_UFP_D_PIN_CAPS(dp_mode[0]);
		pin_cap[1] = PD_DP_DFP_D_PIN_CAPS(dp_mode[1]);
		break;
	default:
		DP_ERR("select_pin error1\n");
		return false;
	}

	PE_DBG("modes=0x%x 0x%x\n", dp_mode[0], dp_mode[1]);
	PE_DBG("pins=0x%x 0x%x\n", pin_cap[0], pin_cap[1]);

	pin_caps = pin_cap[0] & pin_cap[1];

	/* if don't want multi-function then ignore those pin configs */
	if (!PD_VDO_DPSTS_MF_PREF(dp_data->remote_status))
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
		dp_data->local_config = VDO_DP_DFP_CFG(pin_caps, signal);
		dp_data->remote_config = VDO_DP_UFP_CFG(pin_caps, signal);
	} else {
		dp_data->local_config = VDO_DP_UFP_CFG(pin_caps, signal);
		dp_data->remote_config = VDO_DP_DFP_CFG(pin_caps, signal);
	}

	return true;
}

void dp_dfp_u_request_dp_configuration(struct pd_port *pd_port)
{
	if (!dp_dfp_u_select_pin_mode(pd_port)) {
		dp_dfp_u_set_state(pd_port,
			DP_DFP_U_ERR_CONFIGURE_SELECT_MODE);
		return;
	}

	tcpci_dp_notify_config_start(pd_port->tcpc);

	dp_dfp_u_set_state(pd_port, DP_DFP_U_CONFIGURE);
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DP_CONFIG);
}

static inline bool dp_dfp_u_update_dp_connected(struct pd_port *pd_port)
{
	bool valid_connected = false;
	uint32_t dp_connected, dp_local_connected;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	dp_connected = PD_VDO_DPSTS_CONNECT(dp_data->remote_status);
	dp_local_connected = PD_VDO_DPSTS_CONNECT(dp_data->local_status);

	switch (dp_connected) {
	case DPSTS_DFP_D_CONNECTED:
	case DPSTS_UFP_D_CONNECTED:
		valid_connected = dp_update_dp_connected_one(
			pd_port, dp_connected, dp_local_connected);

		if (!valid_connected) {
			dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_STATUS_UPDATE_ROLE);
		}
		break;

	case DPSTS_DISCONNECT:
		dp_dfp_u_set_state(pd_port, DP_DFP_U_WAIT_ATTENTION);
		break;

	case DPSTS_BOTH_CONNECTED:
		valid_connected = dp_update_dp_connected_both(
			pd_port, dp_local_connected, false);

		if (!valid_connected) {
			DP_INFO("BOTH_SEL_ONE\n");
			pd_put_tcp_vdm_event(pd_port,
				TCP_DPM_EVT_DP_STATUS_UPDATE);
		}
		break;
	}

	return valid_connected;
}

bool dp_dfp_u_notify_dp_status_update(struct pd_port *pd_port, bool ack)
{
	bool oper_mode = false;
	bool valid_connected;
	uint32_t *ptr;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	switch (dp_data->dfp_u_state) {
	case DP_DFP_U_OPERATION:
		oper_mode = true;
	case DP_DFP_U_STATUS_UPDATE:
		break;

	default:
		return false;
	}

	if (!ack) {
		tcpci_dp_notify_status_update_done(tcpc, 0, false);
		dp_dfp_u_set_state(pd_port,
				DP_DFP_U_ERR_STATUS_UPDATE_NAK_TIMEOUT);
		return false;
	}

	if (dpm_vdm_get_svid(pd_port) != USB_SID_DISPLAYPORT) {
		dp_dfp_u_set_state(pd_port, DP_DFP_U_ERR_STATUS_UPDATE_DP_SID);
		return true;
	}

	ptr = pd_get_msg_vdm_data_payload(pd_port);
	if (!ptr)
		dp_data->remote_status = 0;
	else
		dp_data->remote_status = ptr[0];
	DP_INFO("dp_status: 0x%x\n", dp_data->remote_status);

	if (oper_mode) {
		tcpci_dp_notify_status_update_done(
				tcpc, dp_data->remote_status, ack);
	} else {
		valid_connected =
			dp_dfp_u_update_dp_connected(pd_port);
		if (valid_connected)
			dp_dfp_u_request_dp_configuration(pd_port);
	}
	return true;
}

static inline void dp_ufp_u_auto_update(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_UPDATE
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	if (dp_data->dfp_u_state == DP_DFP_U_OPERATION)
		return;

	pd_port->mode_svid = USB_SID_DISPLAYPORT;
	dp_data->local_status |= DPSTS_DP_ENABLED | DPSTS_DP_HPD_STATUS;
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DP_STATUS_UPDATE);
#endif	/* CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_UPDATE */
}

bool dp_dfp_u_notify_dp_configuration(struct pd_port *pd_port, bool ack)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ack) {
		dp_ufp_u_auto_update(pd_port);
		dp_dfp_u_set_state(pd_port, DP_DFP_U_OPERATION);
	} else
		DP_ERR("config failed: 0x%0x\n", dp_data->remote_config);

	tcpci_dp_notify_config_done(tcpc,
		dp_data->local_config, dp_data->remote_config, ack);

	return true;
}

bool dp_dfp_u_notify_attention(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data)
{
	bool valid_connected;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	uint32_t *ptr;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	ptr = pd_get_msg_vdm_data_payload(pd_port);
	if (!ptr)
		dp_data->remote_status = 0;
	else
		dp_data->remote_status = ptr[0];

	DP_INFO("dp_status: 0x%x\n", dp_data->remote_status);

	switch (dp_data->dfp_u_state) {
	case DP_DFP_U_WAIT_ATTENTION:
		valid_connected =
			dp_dfp_u_update_dp_connected(pd_port);
		if (valid_connected)
			dp_dfp_u_request_dp_configuration(pd_port);
		break;

	case DP_DFP_U_OPERATION:
		tcpci_dp_attention(tcpc, dp_data->remote_status);
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
	"dp_ufp_u_operation",
};
#endif /* DPM_DBG_ENABLE */

static void dp_ufp_u_set_state(struct pd_port *pd_port, uint8_t state)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	dp_data->ufp_u_state = state;

	if (dp_data->ufp_u_state < DP_UFP_U_STATE_NR)
		DPM_DBG("%s\n", dp_ufp_u_state_name[state]);
	else
		DPM_DBG("dp_ufp_u_stop\n");
}

void dp_ufp_u_request_enter_mode(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, uint8_t ops)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	dp_data->local_status = pd_port->dp_first_connected;

	if (pd_port->dpm_caps & DPM_CAP_DP_PREFER_MF)
		dp_data->local_status |= DPSTS_DP_MF_PREF;

	if (pd_port->dp_first_connected == DPSTS_DISCONNECT)
		dp_ufp_u_set_state(pd_port, DP_UFP_U_STARTUP);
	else
		dp_ufp_u_set_state(pd_port, DP_UFP_U_WAIT);
}

void dp_ufp_u_request_exit_mode(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, uint8_t ops)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	memset(dp_data, 0, sizeof(struct dp_data));
	dp_ufp_u_set_state(pd_port, DP_UFP_U_NONE);
}

static inline bool dp_ufp_u_update_dp_connected(struct pd_port *pd_port)
{
	bool valid_connected = false;
	uint32_t dp_connected, dp_local_connected;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	dp_connected = PD_VDO_DPSTS_CONNECT(dp_data->remote_status);
	dp_local_connected = PD_VDO_DPSTS_CONNECT(dp_data->local_status);

	switch (dp_connected) {
	case DPSTS_DFP_D_CONNECTED:
	case DPSTS_UFP_D_CONNECTED:
		valid_connected = dp_update_dp_connected_one(
			pd_port, dp_connected, dp_local_connected);
		break;

	case DPSTS_BOTH_CONNECTED:
		valid_connected = dp_update_dp_connected_both(
			pd_port, dp_local_connected, true);
		break;

	default:
		break;
	}

	return valid_connected;
}

static inline int dp_ufp_u_request_dp_status(struct pd_port *pd_port)
{
	bool ack;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	uint32_t *ptr;

	ptr = pd_get_msg_vdm_data_payload(pd_port);
	if (!ptr)
		dp_data->remote_status = 0;
	else
		dp_data->remote_status = ptr[0];

	switch (dp_data->ufp_u_state) {
	case DP_UFP_U_WAIT:
		ack = dp_ufp_u_update_dp_connected(pd_port);
		break;

	case DP_UFP_U_STARTUP:
	case DP_UFP_U_OPERATION:
		ack = true;
		tcpci_dp_status_update(
			pd_port->tcpc, dp_data->remote_status);
		break;

	default:
		ack = false;
		break;
	}

	if (ack) {
		return pd_reply_svdm_request(pd_port,
			CMDT_RSP_ACK, 1, &dp_data->local_status);
	} else {
		return dpm_vdm_reply_svdm_nak(pd_port);
	}
}

bool dp_ufp_u_is_valid_dp_config(struct pd_port *pd_port, uint32_t dp_config)
{
	/* TODO: Check it later .... */
	uint32_t sel_config;
	bool retval = false;
	uint32_t local_pin;
	struct svdm_svid_data *svid_data = &pd_port->svid_data[0];
	uint32_t local_mode = svid_data->local_mode.mode_vdo[0];
	uint32_t remote_pin = PD_DP_CFG_PIN(dp_config);

	sel_config = MODE_DP_PORT_CAP(dp_config);
	switch (sel_config) {
	case DP_CONFIG_USB:
		retval = true;
		break;

	case DP_CONFIG_DFP_D:
		local_pin = PD_DP_DFP_D_PIN_CAPS(local_mode);
		if ((local_pin & remote_pin) &&
			(MODE_DP_PORT_CAP(local_mode) & MODE_DP_SRC))
			retval = true;
		break;

	case DP_CONFIG_UFP_D:
		local_pin = PD_DP_UFP_D_PIN_CAPS(local_mode);
		if ((local_pin & remote_pin) &&
			(MODE_DP_PORT_CAP(local_mode) & MODE_DP_SNK))
			retval = true;
		break;
	}

	return retval;
}

static inline void dp_ufp_u_auto_attention(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_ATTENTION
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	pd_port->mode_svid = USB_SID_DISPLAYPORT;
	dp_data->local_status |= DPSTS_DP_ENABLED | DPSTS_DP_HPD_STATUS;
#endif	/* CONFIG_USB_PD_DBG_DP_UFP_U_AUTO_ATTENTION */
}

static inline int dp_ufp_u_request_dp_config(struct pd_port *pd_port)
{
	bool ack = false;
	uint32_t dp_config, *ptr;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	ptr = pd_get_msg_vdm_data_payload(pd_port);
	if (!ptr)
		dp_config = 0;
	else
		dp_config = ptr[0];
	DPM_DBG("dp_config: 0x%x\n", dp_config);

	switch (dp_data->ufp_u_state) {
	case DP_UFP_U_STARTUP:
	case DP_UFP_U_WAIT:
	case DP_UFP_U_OPERATION:
		ack = dp_ufp_u_is_valid_dp_config(pd_port, dp_config);

		if (ack) {
			dp_data->local_config = dp_config;
			tcpci_dp_configure(tcpc, dp_config);
			dp_ufp_u_auto_attention(pd_port);
			dp_ufp_u_set_state(pd_port, DP_UFP_U_OPERATION);
		}
		break;
	}

	return dpm_vdm_reply_svdm_request(pd_port, ack);
}

static inline void dp_ufp_u_send_dp_attention(struct pd_port *pd_port)
{
	struct svdm_svid_data *svid_data;
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	switch (dp_data->ufp_u_state) {
	case DP_UFP_U_STARTUP:
	case DP_UFP_U_OPERATION:
		svid_data = dpm_get_svdm_svid_data(
				pd_port, USB_SID_DISPLAYPORT);
		PD_BUG_ON(svid_data == NULL);

		pd_send_vdm_dp_attention(pd_port, TCPC_TX_SOP,
			svid_data->active_mode, dp_data->local_status);
		break;

	default:
		VDM_STATE_DPM_INFORMED(pd_port);
		pd_notify_tcp_vdm_event_2nd_result(
			pd_port, TCP_DPM_RET_DENIED_NOT_READY);
		break;
	}
}

/* ---- UFP : DP Only ---- */

int pd_dpm_ufp_request_dp_status(struct pd_port *pd_port)
{
	return dp_ufp_u_request_dp_status(pd_port);
}

int pd_dpm_ufp_request_dp_config(struct pd_port *pd_port)
{
	return dp_ufp_u_request_dp_config(pd_port);
}

void pd_dpm_ufp_send_dp_attention(struct pd_port *pd_port)
{
	dp_ufp_u_send_dp_attention(pd_port);
}

/* ---- DFP : DP Only ---- */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

void pd_dpm_dfp_send_dp_status_update(struct pd_port *pd_port)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	pd_send_vdm_dp_status(pd_port, TCPC_TX_SOP,
		pd_port->mode_obj_pos, 1, &dp_data->local_status);
}

void pd_dpm_dfp_inform_dp_status_update(
	struct pd_port *pd_port, bool ack)
{
	VDM_STATE_DPM_INFORMED(pd_port);
	dp_dfp_u_notify_dp_status_update(pd_port, ack);
}

void pd_dpm_dfp_send_dp_configuration(struct pd_port *pd_port)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	pd_send_vdm_dp_config(pd_port, TCPC_TX_SOP,
		pd_port->mode_obj_pos, 1, &dp_data->remote_config);
}

void pd_dpm_dfp_inform_dp_configuration(
	struct pd_port *pd_port, bool ack)
{
	VDM_STATE_DPM_INFORMED(pd_port);
	dp_dfp_u_notify_dp_configuration(pd_port, ack);
}

#endif /* CONFIG_USB_PD_ALT_MODE_DFP */

bool dp_reset_state(struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	struct dp_data *dp_data = pd_get_dp_data(pd_port);

	memset(dp_data, 0, sizeof(struct dp_data));
	return true;
}

#define DEFAULT_DP_ROLE_CAP				(MODE_DP_SRC)
#define DEFAULT_DP_FIRST_CONNECTED		(DPSTS_DFP_D_CONNECTED)
#define DEFAULT_DP_SECOND_CONNECTED		(DPSTS_DFP_D_CONNECTED)

#ifdef CONFIG_USB_PD_ALT_MODE
static const struct {
	const char *prop_name;
	uint32_t mode;
} supported_dp_pin_modes[] = {
	{ "pin_assignment,mode_a", MODE_DP_PIN_A },
	{ "pin_assignment,mode_b", MODE_DP_PIN_B },
	{ "pin_assignment,mode_c", MODE_DP_PIN_C },
	{ "pin_assignment,mode_d", MODE_DP_PIN_D },
	{ "pin_assignment,mode_e", MODE_DP_PIN_E },
	{ "pin_assignment,mode_f", MODE_DP_PIN_F },
};

static const struct {
	const char *conn_mode;
	uint32_t val;
} dp_connect_mode[] = {
	{"both", DPSTS_BOTH_CONNECTED},
	{"dfp_d", DPSTS_DFP_D_CONNECTED},
	{"ufp_d", DPSTS_UFP_D_CONNECTED},
};

bool dp_parse_svid_data(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	struct device_node *np, *ufp_np, *dfp_np;
	const char *connection;
	uint32_t ufp_d_pin_cap = 0;
	uint32_t dfp_d_pin_cap = 0;
	uint32_t signal = MODE_DP_V13;
	uint32_t receptacle = 1;
	uint32_t usb2 = 0;
	int i = 0;

	np = of_find_node_by_name(
		pd_port->tcpc->dev.of_node, "displayport");
	if (np == NULL) {
		pr_err("%s get displayport data fail\n", __func__);
		return false;
	}

	pr_info("dp, svid\n");
	svid_data->svid = USB_SID_DISPLAYPORT;
	ufp_np = of_find_node_by_name(np, "ufp_d");
	dfp_np = of_find_node_by_name(np, "dfp_d");

	if (ufp_np) {
		pr_info("dp, ufp_np\n");
		for (i = 0; i < ARRAY_SIZE(supported_dp_pin_modes); i++) {
			if (of_property_read_bool(ufp_np,
				supported_dp_pin_modes[i].prop_name))
				ufp_d_pin_cap |=
					supported_dp_pin_modes[i].mode;
		}
	}

	if (dfp_np) {
		pr_info("dp, dfp_np\n");
		for (i = 0; i < ARRAY_SIZE(supported_dp_pin_modes); i++) {
			if (of_property_read_bool(dfp_np,
				supported_dp_pin_modes[i].prop_name))
				dfp_d_pin_cap |=
					supported_dp_pin_modes[i].mode;
		}
	}

	if (of_property_read_bool(np, "signal,dp_v13"))
		signal |= MODE_DP_V13;
	if (of_property_read_bool(np, "signal,dp_gen2"))
		signal |= MODE_DP_GEN2;
	if (of_property_read_bool(np, "usbr20_not_used"))
		usb2 = 1;
	if (of_property_read_bool(np, "typec,receptacle"))
		receptacle = 1;

	svid_data->local_mode.mode_cnt = 1;
	svid_data->local_mode.mode_vdo[0] = VDO_MODE_DP(
		ufp_d_pin_cap, dfp_d_pin_cap,
		usb2, receptacle, signal, (ufp_d_pin_cap ? MODE_DP_SNK : 0)
		| (dfp_d_pin_cap ? MODE_DP_SRC : 0));

	pd_port->dp_first_connected = DEFAULT_DP_FIRST_CONNECTED;
	pd_port->dp_second_connected = DEFAULT_DP_SECOND_CONNECTED;

	if (of_property_read_string(np, "1st_connection", &connection) == 0) {
		pr_info("dp, 1st_connection\n");
		for (i = 0; i < ARRAY_SIZE(dp_connect_mode); i++) {
			if (strcasecmp(connection,
				dp_connect_mode[i].conn_mode) == 0) {
				pd_port->dp_first_connected =
					dp_connect_mode[i].val;
				break;
			}
		}
	}

	if (of_property_read_string(np, "2nd_connection", &connection) == 0) {
		pr_info("dp, 2nd_connection\n");
		for (i = 0; i < ARRAY_SIZE(dp_connect_mode); i++) {
			if (strcasecmp(connection,
				dp_connect_mode[i].conn_mode) == 0) {
				pd_port->dp_second_connected =
					dp_connect_mode[i].val;
				break;
			}
		}
	}
	/* 2nd connection must not be BOTH */
	PD_BUG_ON(pd_port->dp_second_connected == DPSTS_BOTH_CONNECTED);
	/* UFP or DFP can't both be invalid */
	PD_BUG_ON(ufp_d_pin_cap == 0 && dfp_d_pin_cap == 0);
	if (pd_port->dp_first_connected == DPSTS_BOTH_CONNECTED) {
		PD_BUG_ON(ufp_d_pin_cap == 0);
		PD_BUG_ON(dfp_d_pin_cap == 0);
	}

	return true;
}
#endif	/* CONFIG_USB_PD_ALT_MODE */

#endif	/* CONFIG_USB_PD_ALT_MODE */
#endif	/* CONFIG_USB_POWER_DELIVERY */
