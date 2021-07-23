/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * PD Device Policy Manager Ready State reactions
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

#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "inc/pd_dpm_pdo_select.h"
#include "pd_dpm_prv.h"

#define DPM_REACTION_COND_ALWAYS		(1<<0)
#define DPM_REACTION_COND_UFP_ONLY	(1<<1)
#define DPM_REACTION_COND_DFP_ONLY	(1<<2)
#define DPM_REACTION_COND_PD30		(1<<3)

#define DPM_REACCOND_DFP	\
	(DPM_REACTION_COND_ALWAYS | DPM_REACTION_COND_DFP_ONLY)

#define DPM_REACCOND_UFP	\
	(DPM_REACTION_COND_ALWAYS | DPM_REACTION_COND_UFP_ONLY)

#define DPM_REACTION_COND_CHECK_ONCE			(1<<5)
#define DPM_REACTION_COND_ONE_SHOT			(1<<6)
#define DPM_REACTION_COND_LIMITED_RETRIES		(1<<7)

/*
 * DPM flow delay reactions
 */

#ifdef CONFIG_USB_PD_UFP_FLOW_DELAY
static uint8_t dpm_reaction_ufp_flow_delay(struct pd_port *pd_port)
{
	DPM_INFO("UFP Delay\r\n");
	pd_restart_timer(pd_port, PD_TIMER_UFP_FLOW_DELAY);
	return DPM_READY_REACTION_BUSY;
}
#endif	/* CONFIG_USB_PD_UFP_FLOW_DELAY */

#ifdef CONFIG_USB_PD_DFP_FLOW_DELAY
static uint8_t dpm_reaction_dfp_flow_delay(struct pd_port *pd_port)
{
	DPM_INFO("DFP Delay\r\n");
	pd_restart_timer(pd_port, PD_TIMER_DFP_FLOW_DELAY);
	return DPM_READY_REACTION_BUSY;
}
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY */

#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
static uint8_t dpm_reaction_vconn_stable_delay(struct pd_port *pd_port)
{
	if (pd_port->vconn_role == PD_ROLE_VCONN_DYNAMIC_ON) {
		DPM_INFO("VStable Delay\r\n");
		return DPM_READY_REACTION_BUSY;
	}

	return 0;
}
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */

/*
 * DPM get cap reaction
 */

#ifdef CONFIG_USB_PD_REV30
static uint8_t dpm_reaction_get_source_cap_ext(struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SINK)
		return TCP_DPM_EVT_GET_SOURCE_CAP_EXT;

	return 0;
}
#endif	/* CONFIG_USB_PD_REV30 */

static uint8_t dpm_reaction_get_sink_cap(struct pd_port *pd_port)
{
	return TCP_DPM_EVT_GET_SINK_CAP;
}

static uint8_t dpm_reaction_get_source_cap(struct pd_port *pd_port)
{
	return TCP_DPM_EVT_GET_SOURCE_CAP;
}

static uint8_t dpm_reaction_attemp_get_flag(struct pd_port *pd_port)
{
	return TCP_DPM_EVT_GET_SINK_CAP;
}

/*
 * DPM swap reaction
 */

#ifdef CONFIG_USB_PD_PR_SWAP
static uint8_t dpm_reaction_request_pr_swap(struct pd_port *pd_port)
{
	uint32_t prefer_role =
		DPM_CAP_EXTRACT_PR_CHECK(pd_port->dpm_caps);

	if (!(pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER))
		return 0;

	if (pd_port->power_role == PD_ROLE_SINK) {
		if (prefer_role == DPM_CAP_PR_CHECK_PREFER_SRC)
			return TCP_DPM_EVT_PR_SWAP_AS_SRC;
	} else {
#ifdef CONFIG_USB_PD_SRC_TRY_PR_SWAP_IF_BAD_PW
		if (dpm_check_good_power(pd_port) == GOOD_PW_PARTNER)
			return TCP_DPM_EVT_PR_SWAP_AS_SNK;
#endif	/* CONFIG_USB_PD_SRC_TRY_PR_SWAP_IF_BAD_PW */

		if (prefer_role == DPM_CAP_PR_CHECK_PREFER_SNK)
			return TCP_DPM_EVT_PR_SWAP_AS_SNK;
	}

	return 0;
}
#endif	/* CONFIG_USB_PD_PR_SWAP */

#ifdef CONFIG_USB_PD_DR_SWAP
static uint8_t dpm_reaction_request_dr_swap(struct pd_port *pd_port)
{
	uint32_t prefer_role =
		DPM_CAP_EXTRACT_DR_CHECK(pd_port->dpm_caps);

	if (!(pd_port->dpm_caps & DPM_CAP_LOCAL_DR_DATA))
		return 0;

	if (pd_port->data_role == PD_ROLE_DFP
		&& prefer_role == DPM_CAP_DR_CHECK_PREFER_UFP)
		return TCP_DPM_EVT_DR_SWAP_AS_UFP;

	if (pd_port->data_role == PD_ROLE_UFP
		&& prefer_role == DPM_CAP_DR_CHECK_PREFER_DFP)
		return TCP_DPM_EVT_DR_SWAP_AS_DFP;

	return 0;
}
#endif	/* #ifdef CONFIG_USB_PD_DR_SWAP */


/*
 * DPM DiscoverCable reaction
 */

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
static uint8_t dpm_reaction_dynamic_vconn(struct pd_port *pd_port)
{
	pd_dpm_dynamic_enable_vconn(pd_port);
	return 0;
}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_USB_PD_DISCOVER_CABLE_REQUEST_VCONN
static uint8_t dpm_reaction_request_vconn_source(struct pd_port *pd_port)
{
	bool return_vconn = true;

	if (!(pd_port->dpm_caps & DPM_CAP_LOCAL_VCONN_SUPPLY))
		return 0;

	if (pd_port->vconn_role)
		return 0;

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (pd_port->tcpc_dev->tcpc_vconn_supply == TCPC_VCONN_SUPPLY_STARTUP)
		return_vconn = false;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_USB_PD_REV30
	if (pd_check_rev30(pd_port))
		return_vconn = false;
#endif	/* CONFIG_USB_PD_REV30 */

	if (return_vconn)
		dpm_reaction_set(pd_port, DPM_REACTION_RETURN_VCONN_SRC);

	return TCP_DPM_EVT_VCONN_SWAP_ON;
}
#endif	/* CONFIG_USB_PD_DISCOVER_CABLE_REQUEST_VCONN */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
static uint8_t pd_dpm_reaction_discover_cable(struct pd_port *pd_port)
{
#ifdef CONFIG_PD_DFP_RESET_CABLE
	if (pd_is_reset_cable(pd_port))
		return TCP_DPM_EVT_CABLE_SOFTRESET;
#endif	/* CONFIG_PD_DFP_RESET_CABLE */

	if (pd_is_discover_cable(pd_port)) {
		pd_restart_timer(pd_port, PD_TIMER_DISCOVER_ID);
		return DPM_READY_REACTION_BUSY;
	}

	return 0;
}
#endif	/* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */

#ifdef CONFIG_USB_PD_DISCOVER_CABLE_RETURN_VCONN
static uint8_t dpm_reaction_return_vconn_source(struct pd_port *pd_port)
{
	if (pd_port->vconn_role) {
		DPM_DBG("VconnReturn\r\n");
		return TCP_DPM_EVT_VCONN_SWAP_OFF;
	}

	return 0;
}
#endif	/* CONFIG_USB_PD_DISCOVER_CABLE_RETURN_VCONN */

/*
 * DPM EnterMode reaction
 */

#ifdef CONFIG_USB_PD_ATTEMP_DISCOVER_ID
static uint8_t dpm_reaction_discover_id(struct pd_port *pd_port)
{
	return TCP_DPM_EVT_DISCOVER_ID;
}
#endif	/* CONFIG_USB_PD_ATTEMP_DISCOVER_ID */

#ifdef CONFIG_USB_PD_ATTEMP_DISCOVER_SVID
static uint8_t dpm_reaction_discover_svid(struct pd_port *pd_port)
{
	return TCP_DPM_EVT_DISCOVER_SVIDS;
}
#endif	/* CONFIG_USB_PD_ATTEMP_DISCOVER_SVID */

#ifdef CONFIG_USB_PD_MODE_OPERATION
static uint8_t dpm_reaction_mode_operation(struct pd_port *pd_port)
{
	if (svdm_notify_pe_ready(pd_port))
		return DPM_READY_REACTION_BUSY;

	return 0;
}
#endif	/* CONFIG_USB_PD_MODE_OPERATION */

/*
 * DPM Local/Remote Alert reaction
 */

#ifdef CONFIG_USB_PD_REV30

#ifdef CONFIG_USB_PD_DPM_AUTO_SEND_ALERT

static uint8_t dpm_reaction_send_alert(struct pd_port *pd_port)
{
	uint32_t alert_urgent;
	struct pe_data *pe_data = &pd_port->pe_data;

	alert_urgent = pe_data->local_alert;
	alert_urgent &= ~ADO_GET_STATUS_ONCE_MASK;

	if (!pe_data->get_status_once)
		pe_data->local_alert = alert_urgent;

	if ((!pe_data->pe_ready) && (alert_urgent == 0))
		return 0;

	if (pe_data->local_alert == 0)
		return 0;

	return TCP_DPM_EVT_ALERT;
}

#endif	/* CONFIG_USB_PD_DPM_AUTO_SEND_ALERT */

#ifdef CONFIG_USB_PD_DPM_AUTO_GET_STATUS

const uint32_t c_get_status_alert_type = ADO_ALERT_OCP|
	ADO_ALERT_OTP|ADO_ALERT_OVP|ADO_ALERT_OPER_CHANGED|
	ADO_ALERT_SRC_IN_CHANGED;

static inline uint8_t dpm_reaction_alert_status_changed(struct pd_port *pd_port)
{
	pd_port->pe_data.remote_alert &=
		~ADO_ALERT_TYPE_SET(c_get_status_alert_type);

	return TCP_DPM_EVT_GET_STATUS;
}

static inline uint8_t dpm_reaction_alert_battry_changed(struct pd_port *pd_port)
{
	uint8_t i;
	uint8_t mask;
	uint8_t bat_change_i = 255;
	uint8_t bat_change_mask1, bat_change_mask2;

	bat_change_mask1 = ADO_FIXED_BAT(pd_port->pe_data.remote_alert);
	bat_change_mask2 = ADO_HOT_SWAP_BAT(pd_port->pe_data.remote_alert);

	if (bat_change_mask1) {
		for (i = 0; i < 4; i++) {
			mask = 1<<i;
			if (bat_change_mask1 & mask) {
				bat_change_i = i;
				bat_change_mask1 &= ~mask;
				pd_port->pe_data.remote_alert &=
					~ADO_FIXED_BAT_SET(bat_change_mask1);
				break;
			}
		}
	} else if (bat_change_mask2) {
		for (i = 0; i < 4; i++) {
			mask = 1<<i;
			if (bat_change_mask2 & mask) {
				bat_change_i = i + 4;
				bat_change_mask2 &= ~mask;
				pd_port->pe_data.remote_alert &=
					~ADO_HOT_SWAP_BAT_SET(bat_change_mask2);
				break;
			}
		}
	}

	if (bat_change_mask1 == 0 && bat_change_mask2 == 0) {
		pd_port->pe_data.remote_alert &=
			~ADO_ALERT_TYPE_SET(ADO_ALERT_BAT_CHANGED);
	}

	if (bat_change_i == 255)
		return 0;

	pd_port->tcp_event.tcp_dpm_data.data_object[0] = bat_change_i;
	return TCP_DPM_EVT_GET_BAT_STATUS;
}

static uint8_t dpm_reaction_handle_alert(struct pd_port *pd_port)
{
	uint32_t alert_type = ADO_ALERT_TYPE(pd_port->pe_data.remote_alert);

	if (alert_type & c_get_status_alert_type)
		return dpm_reaction_alert_status_changed(pd_port);

	if (alert_type & ADO_ALERT_BAT_CHANGED)
		return dpm_reaction_alert_battry_changed(pd_port);

	return 0;
}
#endif	/* CONFIG_USB_PD_DPM_AUTO_GET_STATUS */

#endif	/* CONFIG_USB_PD_REV30 */

/*
 * DPM Idle reaction
 */

static inline uint8_t dpm_get_pd_connect_state(struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SOURCE) {
#ifdef CONFIG_USB_PD_REV30
		if (pd_check_rev30(pd_port))
			return PD_CONNECT_PE_READY_SRC_PD30;
#endif     /* CONFIG_USB_PD_REV30 */

		return PD_CONNECT_PE_READY_SRC;
	}

#ifdef CONFIG_USB_PD_REV30
	if (pd_check_rev30(pd_port)) {
		if (pd_is_source_support_apdo(pd_port))
			return PD_CONNECT_PE_READY_SNK_APDO;

		return PD_CONNECT_PE_READY_SNK_PD30;
	}
#endif     /* CONFIG_USB_PD_REV30 */

	return PD_CONNECT_PE_READY_SNK;
}

static inline void dpm_check_vconn_highv_prot(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	bool vconn_highv_prot;
	struct pe_data *pe_data = &pd_port->pe_data;

	vconn_highv_prot = pd_port->request_v_new > 5000;
	if (vconn_highv_prot != pe_data->vconn_highv_prot) {
		DPM_INFO("VC_HIGHV_PROT: %d\r\n", vconn_highv_prot);

		pe_data->vconn_highv_prot = vconn_highv_prot;

		if (!vconn_highv_prot)
			pd_set_vconn(pd_port, pd_port->vconn_role);
	}
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */
}

static uint8_t dpm_reaction_update_pe_ready(struct pd_port *pd_port)
{
	uint8_t state;

	if (!pd_port->pe_data.pe_ready) {
		DPM_INFO("PE_READY\r\n");
		pd_port->pe_data.pe_ready = true;
#ifdef CONFIG_DUAL_ROLE_USB_INTF
		dual_role_instance_changed(pd_port->tcpc_dev->dr_usb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */
	}

	state = dpm_get_pd_connect_state(pd_port);
	pd_update_connect_state(pd_port, state);

	dpm_check_vconn_highv_prot(pd_port);
	pd_dpm_dynamic_disable_vconn(pd_port);

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	pd_port->pe_data.pd_traffic_idle = true;
	if (pd_check_rev30(pd_port) &&
		(pd_port->power_role == PD_ROLE_SOURCE))
		pd_set_sink_tx(pd_port, PD30_SINK_TX_OK);
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	return 0;
}

/*
 * DPM reaction declaration
 */

typedef uint8_t (*dpm_reaction_fun)(struct pd_port *pd_port);

struct dpm_ready_reaction {
	uint32_t bit_mask;
	uint8_t condition;
	dpm_reaction_fun	handler;
};

#define DECL_DPM_REACTION(xmask, xcond, xhandler)	{ \
	.bit_mask = xmask,	\
	.condition = xcond,	\
	.handler = xhandler, \
}

#define DECL_DPM_REACTION_ALWAYS(xhandler)	\
	DECL_DPM_REACTION(DPM_REACTION_CAP_ALWAYS,	\
		DPM_REACTION_COND_ALWAYS,	\
		xhandler)

#define DECL_DPM_REACTION_CHECK_ONCE(xmask, xhandler)	\
	DECL_DPM_REACTION(xmask,	\
		DPM_REACTION_COND_ALWAYS |	\
		DPM_REACTION_COND_CHECK_ONCE,	\
		xhandler)

#define DECL_DPM_REACTION_LIMITED_RETRIES(xmask, xhandler)	\
	DECL_DPM_REACTION(xmask,	\
		DPM_REACTION_COND_ALWAYS |\
		DPM_REACTION_COND_LIMITED_RETRIES,	\
		xhandler)

#define DECL_DPM_REACTION_ONE_SHOT(xmask, xhandler)	\
	DECL_DPM_REACTION(xmask,	\
		DPM_REACTION_COND_ALWAYS |	\
		DPM_REACTION_COND_ONE_SHOT, \
		xhandler)

#define DECL_DPM_REACTION_UFP(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_UFP_ONLY,	\
		xhandler)

#define DECL_DPM_REACTION_DFP(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_DFP_ONLY,	\
		xhandler)

#define DECL_DPM_REACTION_PD30(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_PD30,	 \
		xhandler)

#define DECL_DPM_REACTION_PD30_LIMITED_RETRIES(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_PD30 |\
		DPM_REACTION_COND_LIMITED_RETRIES, \
		xhandler)

#define DECL_DPM_REACTION_PD30_ONE_SHOT(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_PD30 | \
		DPM_REACTION_COND_ONE_SHOT, \
		xhandler)

#define DECL_DPM_REACTION_DFP_PD30_ONE_SHOT(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_DFP_ONLY |\
		DPM_REACTION_COND_PD30 | \
		DPM_REACTION_COND_ONE_SHOT, \
		xhandler)

#define DECL_DPM_REACTION_DFP_PD30_LIMITED_RETRIES(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_DFP_ONLY |\
		DPM_REACTION_COND_PD30 | \
		DPM_REACTION_COND_LIMITED_RETRIES, \
		xhandler)

#define DECL_DPM_REACTION_DFP_PD30_CHECK_ONCE(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_DFP_ONLY |\
		DPM_REACTION_COND_PD30 | \
		DPM_REACTION_COND_CHECK_ONCE, \
		xhandler)

#define DECL_DPM_REACTION_DFP_PD30_RUN_ONCE(xmask, xhandler) \
	DECL_DPM_REACTION(xmask, \
		DPM_REACTION_COND_DFP_ONLY |\
		DPM_REACTION_COND_PD30 | \
		DPM_REACTION_COND_CHECK_ONCE | \
		DPM_REACTION_COND_ONE_SHOT, \
		xhandler)

static const struct dpm_ready_reaction dpm_reactions[] = {

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_DPM_AUTO_SEND_ALERT
	DECL_DPM_REACTION_PD30(
		DPM_REACTION_CAP_ALWAYS,
		dpm_reaction_send_alert),
#endif	/* CONFIG_USB_PD_DPM_AUTO_SEND_ALERT */
#ifdef CONFIG_USB_PD_DPM_AUTO_GET_STATUS
	DECL_DPM_REACTION_PD30(
		DPM_REACTION_CAP_ALWAYS,
		dpm_reaction_handle_alert),
#endif	/* CONFIG_USB_PD_DPM_AUTO_GET_STATUS */
#endif	/* CONFIG_USB_PD_REV30 */

#ifdef CONFIG_USB_PD_UFP_FLOW_DELAY
	DECL_DPM_REACTION_UFP(
		DPM_REACTION_UFP_FLOW_DELAY,
		dpm_reaction_ufp_flow_delay),
#endif	/* CONFIG_USB_PD_UFP_FLOW_DELAY */

	DECL_DPM_REACTION_LIMITED_RETRIES(
		DPM_REACTION_GET_SINK_CAP,
		dpm_reaction_get_sink_cap),

	DECL_DPM_REACTION_LIMITED_RETRIES(
		DPM_REACTION_GET_SOURCE_CAP,
		dpm_reaction_get_source_cap),

	DECL_DPM_REACTION_LIMITED_RETRIES(
		DPM_REACTION_ATTEMPT_GET_FLAG,
		dpm_reaction_attemp_get_flag),

#ifdef CONFIG_USB_PD_REV30
	DECL_DPM_REACTION_PD30_ONE_SHOT(
		DPM_REACTION_GET_SOURCE_CAP_EXT,
		dpm_reaction_get_source_cap_ext),
#endif	/* CONFIG_USB_PD_REV30 */

#ifdef CONFIG_USB_PD_PR_SWAP
	DECL_DPM_REACTION_CHECK_ONCE(
		DPM_REACTION_REQUEST_PR_SWAP,
		dpm_reaction_request_pr_swap),
#endif	/* CONFIG_USB_PD_PR_SWAP */

#ifdef CONFIG_USB_PD_DR_SWAP
	DECL_DPM_REACTION_CHECK_ONCE(
		DPM_REACTION_REQUEST_DR_SWAP,
		dpm_reaction_request_dr_swap),
#endif	/* CONFIG_USB_PD_DR_SWAP */

#ifdef CONFIG_USB_PD_DFP_FLOW_DELAY
	DECL_DPM_REACTION_DFP(
		DPM_REACTION_DFP_FLOW_DELAY,
		dpm_reaction_dfp_flow_delay),
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY */

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	DECL_DPM_REACTION_DFP_PD30_CHECK_ONCE(
		DPM_REACTION_DYNAMIC_VCONN,
		dpm_reaction_dynamic_vconn),
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_USB_PD_DISCOVER_CABLE_REQUEST_VCONN
	DECL_DPM_REACTION_DFP_PD30_RUN_ONCE(
		DPM_REACTION_REQUEST_VCONN_SRC,
		dpm_reaction_request_vconn_source),
#endif	/* CONFIG_USB_PD_DISCOVER_CABLE_REQUEST_VCONN */

#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
	DECL_DPM_REACTION_DFP_PD30_CHECK_ONCE(
		DPM_REACTION_VCONN_STABLE_DELAY,
		dpm_reaction_vconn_stable_delay),
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	DECL_DPM_REACTION_DFP_PD30_CHECK_ONCE(
		DPM_REACTION_DISCOVER_CABLE,
		pd_dpm_reaction_discover_cable),
#endif	/* CONFIG_USB_PD_DFP_READY_DISCOVER_ID */

#ifdef CONFIG_USB_PD_DISCOVER_CABLE_RETURN_VCONN
	DECL_DPM_REACTION_DFP_PD30_RUN_ONCE(
		DPM_REACTION_RETURN_VCONN_SRC,
		dpm_reaction_return_vconn_source),
#endif	/* CONFIG_USB_PD_DISCOVER_CABLE_RETURN_VCONN */

#ifdef CONFIG_USB_PD_ATTEMP_DISCOVER_ID
	DECL_DPM_REACTION_DFP_PD30_LIMITED_RETRIES(
		DPM_REACTION_DISCOVER_ID,
		dpm_reaction_discover_id),
#endif	/* CONFIG_USB_PD_ATTEMP_DISCOVER_ID */

#ifdef CONFIG_USB_PD_ATTEMP_DISCOVER_SVID
	DECL_DPM_REACTION_DFP_PD30_LIMITED_RETRIES(
		DPM_REACTION_DISCOVER_SVID,
		dpm_reaction_discover_svid),
#endif	/* CONFIG_USB_PD_ATTEMP_DISCOVER_SVID */

#ifdef CONFIG_USB_PD_MODE_OPERATION
	DECL_DPM_REACTION_ALWAYS(
		dpm_reaction_mode_operation),
#endif	/* CONFIG_USB_PD_MODE_OPERATION */

	DECL_DPM_REACTION_ALWAYS(
		dpm_reaction_update_pe_ready),
};

/**
 * dpm_get_reaction_env
 *
 * Get current reaction's environmental conditions.
 *
 * Returns environmental conditions.
 */

static inline uint8_t dpm_get_reaction_env(struct pd_port *pd_port)
{
	uint8_t conditions;

	if (pd_port->data_role == PD_ROLE_DFP)
		conditions = DPM_REACCOND_DFP;
	else
		conditions = DPM_REACCOND_UFP;

#ifdef CONFIG_USB_PD_REV30
	if (pd_check_rev30(pd_port))
		conditions |= DPM_REACTION_COND_PD30;
#endif	/* CONFIG_USB_PD_REV30 */

	return conditions;
}

/**
 * dpm_check_reaction_busy
 *
 * check this reaction is still keep busy
 *
 * @ reaction : which reaction is checked.
 *
 * Return Boolean to indicate busy or not.
 */

static inline bool dpm_check_reaction_busy(struct pd_port *pd_port,
		const struct dpm_ready_reaction *reaction)
{
	if (reaction->bit_mask & DPM_REACTION_CAP_ALWAYS)
		return false;

	return !dpm_reaction_check(
			pd_port, DPM_REACTION_CAP_READY_ONCE);
}

/**
 * dpm_check_reaction_available
 *
 * check this reaction is available.
 *
 * @ env : Environmental conditions of reaction table
 * @ reaction : which reaction is checked.
 *
 * Returns TCP_DPM_EVT_ID if available;
 * Returns Zero to indicate if not available.
 * Returns DPM_READY_REACTION_BUSY to indicate
 *	this reaction is waiting for being finished.
 */

static inline uint8_t dpm_check_reaction_available(struct pd_port *pd_port,
	uint8_t env, const struct dpm_ready_reaction *reaction)
{
	int ret;

	if (!dpm_reaction_check(pd_port, reaction->bit_mask))
		return 0;

	if (!(reaction->condition & env))
		return 0;

	if (dpm_check_reaction_busy(pd_port, reaction))
		return DPM_READY_REACTION_BUSY;

	ret = reaction->handler(pd_port);

	if (ret == 0 &&
		reaction->condition & DPM_REACTION_COND_CHECK_ONCE)
		dpm_reaction_clear(pd_port, reaction->bit_mask);

	return ret;
}

/**
 * dpm_check_reset_reaction
 *
 * Once trigger one reaction,
 * check if automatically clear this reaction flag.
 *
 * For the following reactions type:
 *	DPM_REACTION_COND_ONE_SHOT,
 *	DPM_REACTION_COND_LIMITED_RETRIES
 *
 * @ reaction : which reaction is triggered.
 *
 * Return Boolean to indicate automatically clear or not.
 */

static inline bool dpm_check_clear_reaction(struct pd_port *pd_port,
	const struct dpm_ready_reaction *reaction)
{
	if (pd_port->pe_data.dpm_reaction_id != reaction->bit_mask)
		pd_port->pe_data.dpm_reaction_retry = 0;

	pd_port->pe_data.dpm_reaction_retry++;
	pd_port->pe_data.dpm_reaction_id = reaction->bit_mask;

	if (reaction->condition & DPM_REACTION_COND_ONE_SHOT)
		return true;

	if (reaction->condition & DPM_REACTION_COND_LIMITED_RETRIES)
		return pd_port->pe_data.dpm_reaction_retry > 3;

	return false;
}

uint8_t pd_dpm_get_ready_reaction(struct pd_port *pd_port)
{
	uint8_t evt;
	uint8_t env;
	uint32_t clear_reaction = DPM_REACTION_CAP_READY_ONCE;

	const struct dpm_ready_reaction *reaction = dpm_reactions;
	const struct dpm_ready_reaction *reaction_last =
			dpm_reactions + ARRAY_SIZE(dpm_reactions);

	env = dpm_get_reaction_env(pd_port);

	do {
		evt = dpm_check_reaction_available(pd_port, env, reaction);
	} while ((evt == 0) && (++reaction < reaction_last));

	if (evt > 0 && dpm_check_clear_reaction(pd_port, reaction)) {
		clear_reaction |= reaction->bit_mask;
		DPM_DBG("clear_reaction=%d\r\n", evt);
	}

	dpm_reaction_clear(pd_port, clear_reaction);
	return evt;
}
