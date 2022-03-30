// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"
#include "inc/pd_dpm_core.h"

#if CONFIG_USB_PD_PR_SWAP
static inline int pd_handle_tcp_event_pr_swap(
	struct pd_port *pd_port, uint8_t new_role)
{
	if (pd_port->power_role == new_role)
		return TCP_DPM_RET_DENIED_SAME_ROLE;

	if (!(pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER))
		return TCP_DPM_RET_DENIED_LOCAL_CAP;

	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	pd_port->pe_data.during_swap = false;
	pd_port->state_machine = PE_STATE_MACHINE_PR_SWAP;

	pe_transit_send_pr_swap_state(pd_port);
	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_USB_PD_PR_SWAP */

#if CONFIG_USB_PD_DR_SWAP
static inline int pd_handle_tcp_event_dr_swap(
	struct pd_port *pd_port, uint8_t new_role)
{
	if (pd_port->data_role == new_role)
		return TCP_DPM_RET_DENIED_SAME_ROLE;

	if (!(pd_port->dpm_caps & DPM_CAP_LOCAL_DR_DATA))
		return TCP_DPM_RET_DENIED_LOCAL_CAP;

	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	pd_port->pe_data.during_swap = false;
	pd_port->state_machine = PE_STATE_MACHINE_DR_SWAP;

	PE_TRANSIT_DATA_STATE(pd_port,
		PE_DRS_UFP_DFP_SEND_DR_SWAP,
		PE_DRS_DFP_UFP_SEND_DR_SWAP);

	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_USB_PD_DR_SWAP */

#if CONFIG_USB_PD_VCONN_SWAP
static inline int pd_handle_tcp_event_vconn_swap(
	struct pd_port *pd_port, uint8_t new_role)
{
	uint8_t old_role = pd_port->vconn_role ? 1 : 0;

	if (old_role == new_role)
		return TCP_DPM_RET_DENIED_SAME_ROLE;

	if ((!pd_port->vconn_role) &&
		(!(pd_port->dpm_caps & DPM_CAP_LOCAL_VCONN_SUPPLY)))
		return TCP_DPM_RET_DENIED_LOCAL_CAP;

	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	pd_port->state_machine = PE_STATE_MACHINE_VCONN_SWAP;
	PE_TRANSIT_STATE(pd_port, PE_VCS_SEND_SWAP);
	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_USB_PD_VCONN_SWAP */

#if CONFIG_USB_PD_PE_SOURCE
static inline int pd_handle_tcp_event_gotomin(struct pd_port *pd_port)
{
	if (pd_port->pe_state_curr != PE_SRC_READY)
		return TCP_DPM_RET_DENIED_NOT_READY;

	if (!(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_GIVE_BACK))
		return TCP_DPM_RET_DENIED_PARTNER_CAP;

	PE_TRANSIT_STATE(pd_port, PE_SRC_TRANSITION_SUPPLY);
	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_USB_PD_PE_SOURCE */

static inline int pd_handle_tcp_event_softreset(struct pd_port *pd_port)
{
	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	pe_transit_soft_reset_state(pd_port);
	return TCP_DPM_RET_SENT;
}

#if CONFIG_PD_DFP_RESET_CABLE
static inline int pd_handle_tcp_event_cable_softreset(struct pd_port *pd_port)
{
	bool role_check;

	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	role_check = pd_port->data_role == PD_ROLE_DFP;

	if (pd_check_rev30(pd_port))
		role_check = pd_port->vconn_role;

	if (!role_check)
		return TCP_DPM_RET_DENIED_WRONG_DATA_ROLE;

	PE_TRANSIT_STATE(pd_port, PE_DFP_CBL_SEND_SOFT_RESET);
	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_PD_DFP_RESET_CABLE */

static inline int pd_handle_tcp_event_get_source_cap(struct pd_port *pd_port)
{
	switch (pd_port->pe_state_curr) {
	case PE_SNK_READY:
		PE_TRANSIT_STATE(pd_port, PE_SNK_GET_SOURCE_CAP);
		return TCP_DPM_RET_SENT;

#if CONFIG_USB_PD_PR_SWAP
	case PE_SRC_READY:
		if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
			PE_TRANSIT_STATE(pd_port, PE_DR_SRC_GET_SOURCE_CAP);
			return TCP_DPM_RET_SENT;
		}
#endif	/* CONFIG_USB_PD_PR_SWAP */

		return TCP_DPM_RET_DENIED_LOCAL_CAP;
	}

	return TCP_DPM_RET_DENIED_NOT_READY;
}

static inline int pd_handle_tcp_event_get_sink_cap(struct pd_port *pd_port)
{
	switch (pd_port->pe_state_curr) {
	case PE_SRC_READY:
		PE_TRANSIT_STATE(pd_port, PE_SRC_GET_SINK_CAP);
		return TCP_DPM_RET_SENT;

#if CONFIG_USB_PD_PR_SWAP
	case PE_SNK_READY:
		if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
			PE_TRANSIT_STATE(pd_port, PE_DR_SNK_GET_SINK_CAP);
			return TCP_DPM_RET_SENT;
		}
#endif	/* CONFIG_USB_PD_PR_SWAP */

		return TCP_DPM_RET_DENIED_LOCAL_CAP;
	}

	return TCP_DPM_RET_DENIED_NOT_READY;
}

#if CONFIG_USB_PD_PE_SINK
static inline int pd_handle_tcp_event_request(struct pd_port *pd_port)
{
	int ret = 0;
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;

	if (pd_port->pe_state_curr != PE_SNK_READY)
		return TCP_DPM_RET_DENIED_NOT_READY;

	switch (pd_get_curr_pd_event(pd_port)->msg) {
	case TCP_DPM_EVT_REQUEST:
		ret = pd_dpm_update_tcp_request(
			pd_port, &tcp_event->tcp_dpm_data.pd_req);
		break;
	case TCP_DPM_EVT_REQUEST_EX:
		ret = pd_dpm_update_tcp_request_ex(
			pd_port, &tcp_event->tcp_dpm_data.pd_req_ex);
		break;
	case TCP_DPM_EVT_REQUEST_AGAIN:
		ret = pd_dpm_update_tcp_request_again(pd_port);
		break;
	}

	if (ret != TCP_DPM_RET_SUCCESS)
		return ret;

	PE_TRANSIT_STATE(pd_port, PE_SNK_SELECT_CAPABILITY);
	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_USB_PD_PE_SINK */

static inline int pd_handle_tcp_event_bist_cm2(struct pd_port *pd_port)
{
	uint32_t bist = BDO_MODE_CARRIER2;

	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	pd_send_sop_data_msg(pd_port, PD_DATA_BIST, 1, &bist);
	return TCP_DPM_RET_SENT;
}

#if CONFIG_USB_PD_REV30

#if CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
static inline int pd_handle_tcp_event_get_source_cap_ext(
					struct pd_port *pd_port)
{
	switch (pd_port->pe_state_curr) {
	case PE_SNK_READY:
		PE_TRANSIT_STATE(pd_port, PE_SNK_GET_SOURCE_CAP_EXT);
		return TCP_DPM_RET_SENT;

#if CONFIG_USB_PD_PR_SWAP
	case PE_SRC_READY:
		if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
			PE_TRANSIT_STATE(pd_port,
				PE_DR_SRC_GET_SOURCE_CAP_EXT);
			return TCP_DPM_RET_SENT;
		}
#endif	/* CONFIG_USB_PD_PR_SWAP */
		return TCP_DPM_RET_DENIED_LOCAL_CAP;
	}

	return TCP_DPM_RET_DENIED_NOT_READY;
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */

#if CONFIG_USB_PD_REV30_PPS_SINK
static inline int pd_handle_tcp_event_get_pps_status(struct pd_port *pd_port)
{
	if (pd_port->pe_state_curr != PE_SNK_READY)
		return TCP_DPM_RET_DENIED_NOT_READY;

	PE_TRANSIT_STATE(pd_port, PE_SNK_GET_PPS_STATUS);
	return TCP_DPM_RET_SENT;
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

static inline int pd_make_tcp_event_transit_ready(
	struct pd_port *pd_port, uint8_t state)
{
	if (!pd_check_pe_state_ready(pd_port))
		return TCP_DPM_RET_DENIED_NOT_READY;

	PE_TRANSIT_STATE(pd_port, state);
	return TCP_DPM_RET_SENT;
}

static inline int pd_make_tcp_event_transit_ready2(
	struct pd_port *pd_port, uint8_t snk_state, uint8_t src_state)
{
	switch (pd_port->pe_state_curr) {

#if CONFIG_USB_PD_PE_SINK
	case PE_SNK_READY:
		PE_TRANSIT_STATE(pd_port, snk_state);
		return TCP_DPM_RET_SENT;
#endif	/* CONFIG_USB_PD_PE_SINK */

#if CONFIG_USB_PD_PE_SOURCE
	case PE_SRC_READY:
		PE_TRANSIT_STATE(pd_port, src_state);
		return TCP_DPM_RET_SENT;
#endif	/* CONFIG_USB_PD_PE_SOURCE */
	}

	return TCP_DPM_RET_DENIED_NOT_READY;
}

#if CONFIG_USB_PD_REV30_ALERT_LOCAL
static inline int pd_handle_tcp_event_alert(struct pd_port *pd_port)
{
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;

	if (pd_get_curr_pd_event(pd_port)->msg_sec == PD_TCP_FROM_TCPM)
		pd_port->pe_data.local_alert |= tcp_event->tcp_dpm_data.index;

	return pd_make_tcp_event_transit_ready2(pd_port,
			PE_SNK_SEND_SINK_ALERT, PE_SRC_SEND_SOURCE_ALERT);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

#endif	/* CONFIG_USB_PD_REV30 */

static inline int pd_handle_tcp_event_hardreset(struct pd_port *pd_port)
{
	pe_transit_hard_reset_state(pd_port);
	return TCP_DPM_RET_SENT;
}

static inline int pd_handle_tcp_event_error_recovery(struct pd_port *pd_port)
{
	PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
	return TCP_DPM_RET_SENT;
}

static inline int pd_handle_tcp_dpm_event(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	int ret = TCP_DPM_RET_DENIED_UNKNOWN;

#if CONFIG_USB_PD_REV30
	if (pd_event->msg >= TCP_DPM_EVT_PD30_COMMAND
		&& pd_event->msg < TCP_DPM_EVT_VDM_COMMAND) {
		if (!pd_check_rev30(pd_port))
			return TCP_DPM_RET_DENIED_PD_REV;
	}
#endif	/* CONFIG_USB_PD_REV30 */

	switch (pd_event->msg) {
	default:
		break;
	case TCP_DPM_EVT_PR_SWAP_AS_SNK:
	case TCP_DPM_EVT_PR_SWAP_AS_SRC:
#if CONFIG_USB_PD_PR_SWAP
		ret = pd_handle_tcp_event_pr_swap(pd_port,
			pd_event->msg - TCP_DPM_EVT_PR_SWAP_AS_SNK);
#endif	/* CONFIG_USB_PD_PR_SWAP */
		break;

	case TCP_DPM_EVT_DR_SWAP_AS_UFP:
	case TCP_DPM_EVT_DR_SWAP_AS_DFP:
#if CONFIG_USB_PD_DR_SWAP
		ret = pd_handle_tcp_event_dr_swap(pd_port,
			pd_event->msg - TCP_DPM_EVT_DR_SWAP_AS_UFP);
#endif	/* CONFIG_USB_PD_DR_SWAP */
		break;

	case TCP_DPM_EVT_VCONN_SWAP_OFF:
	case TCP_DPM_EVT_VCONN_SWAP_ON:
#if CONFIG_USB_PD_VCONN_SWAP
		ret = pd_handle_tcp_event_vconn_swap(pd_port,
			pd_event->msg - TCP_DPM_EVT_VCONN_SWAP_OFF);
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
		break;

	case TCP_DPM_EVT_GOTOMIN:
#if CONFIG_USB_PD_PE_SOURCE
		ret =  pd_handle_tcp_event_gotomin(pd_port);
#endif	/* CONFIG_USB_PD_PE_SOURCE */
		break;
	case TCP_DPM_EVT_SOFTRESET:
		ret = pd_handle_tcp_event_softreset(pd_port);
		break;

	case TCP_DPM_EVT_CABLE_SOFTRESET:
#if CONFIG_PD_DFP_RESET_CABLE
		ret = pd_handle_tcp_event_cable_softreset(pd_port);
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
		break;

	case TCP_DPM_EVT_GET_SOURCE_CAP:
		ret = pd_handle_tcp_event_get_source_cap(pd_port);
		break;

	case TCP_DPM_EVT_GET_SINK_CAP:
		ret =  pd_handle_tcp_event_get_sink_cap(pd_port);
		break;

#if CONFIG_USB_PD_PE_SINK
	case TCP_DPM_EVT_REQUEST:
		ret = pd_handle_tcp_event_request(pd_port);
		break;
	case TCP_DPM_EVT_REQUEST_EX:
		ret = pd_handle_tcp_event_request(pd_port);
		break;
	case TCP_DPM_EVT_REQUEST_AGAIN:
		ret =  pd_handle_tcp_event_request(pd_port);
		break;
#endif	/* CONFIG_USB_PD_PE_SINK */

	case TCP_DPM_EVT_BIST_CM2:
		ret = pd_handle_tcp_event_bist_cm2(pd_port);
		break;

#if CONFIG_USB_PD_REV30
#if CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	case TCP_DPM_EVT_GET_SOURCE_CAP_EXT:
		ret = pd_handle_tcp_event_get_source_cap_ext(pd_port);
		break;
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */

#if CONFIG_USB_PD_REV30_STATUS_REMOTE
	case TCP_DPM_EVT_GET_STATUS:
		ret = pd_make_tcp_event_transit_ready2(pd_port,
			PE_SNK_GET_SOURCE_STATUS, PE_SRC_GET_SINK_STATUS);
		break;
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#if CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	case TCP_DPM_EVT_GET_COUNTRY_CODE:
		ret = pd_make_tcp_event_transit_ready(
			pd_port, PE_GET_COUNTRY_CODES);
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */

#if CONFIG_USB_PD_REV30_PPS_SINK
	case TCP_DPM_EVT_GET_PPS_STATUS:
		ret = pd_handle_tcp_event_get_pps_status(pd_port);
		break;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#if CONFIG_USB_PD_REV30_ALERT_LOCAL
	case TCP_DPM_EVT_ALERT:
		ret = pd_handle_tcp_event_alert(pd_port);
		break;
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

#if CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	case TCP_DPM_EVT_GET_COUNTRY_INFO:
		ret = pd_make_tcp_event_transit_ready(
			pd_port, PE_GET_COUNTRY_INFO);
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */

#if CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	case TCP_DPM_EVT_GET_BAT_CAP:
		ret = pd_make_tcp_event_transit_ready(
			pd_port, PE_GET_BATTERY_CAP);
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */

#if CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	case TCP_DPM_EVT_GET_BAT_STATUS:
		ret = pd_make_tcp_event_transit_ready(
			pd_port, PE_GET_BATTERY_STATUS);
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */

#if CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	case TCP_DPM_EVT_GET_MFRS_INFO:
		ret = pd_make_tcp_event_transit_ready(
			pd_port, PE_GET_MANUFACTURER_INFO);
		break;
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */
#endif /* CONFIG_USB_PD_REV30 */

	case TCP_DPM_EVT_HARD_RESET:
		ret = pd_handle_tcp_event_hardreset(pd_port);
		break;

	case TCP_DPM_EVT_ERROR_RECOVERY:
		ret = pd_handle_tcp_event_error_recovery(pd_port);
		break;
	}

	return ret;
}

bool pd_process_event_tcp(struct pd_port *pd_port, struct pd_event *pd_event)
{
	int ret = pd_handle_tcp_dpm_event(pd_port, pd_event);

	pd_notify_tcp_event_1st_result(pd_port, ret);
	return ret == TCP_DPM_RET_SENT;
}

