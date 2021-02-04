/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Power Delivery Managert Driver
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

#include "inc/tcpm.h"
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "pd_dpm_prv.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_pdo_select.h"
#endif	/* CONFIG_USB_POWER_DELIVERY */


/* Check status */

static int tcpm_check_typec_attached(struct tcpc_device *tcpc)
{
	if (tcpc->typec_attach_old == TYPEC_UNATTACHED ||
		tcpc->typec_attach_new == TYPEC_UNATTACHED)
		return TCPM_ERROR_UNATTACHED;

	return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int tcpm_check_pd_attached(struct tcpc_device *tcpc)
{
	struct pe_data *pe_data;
	int ret = tcpm_check_typec_attached(tcpc);

	if (!tcpc->pd_inited_flag)
		return TCPM_ERROR_PE_NOT_READY;

	if (ret != TCPM_SUCCESS)
		return ret;

#ifdef CONFIG_TYPEC_CAP_CUSTOM_SRC
	if (tcpc->typec_attach_old == TYPEC_ATTACHED_CUSTOM_SRC)
		return TCPM_ERROR_CUSTOM_SRC;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */

	pe_data = &tcpc->pd_port.pe_data;

	if (!pe_data->pd_prev_connected)
		return TCPM_ERROR_NO_PD_CONNECTED;

	if (!pe_data->pe_ready)
		return TCPM_ERROR_PE_NOT_READY;

	return TCPM_SUCCESS;
}
#endif	/* CONFIG_USB_POWER_DELIVERY */


/* Inquire TCPC status */

int tcpm_shutdown(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_TCPC_SHUTDOWN_VBUS_DISABLE
	if (tcpc_dev->typec_power_ctrl)
		tcpci_disable_vbus_control(tcpc_dev);
#endif	/* CONFIG_TCPC_SHUTDOWN_VBUS_DISABLE */

	if (tcpc_dev->ops->deinit)
		tcpc_dev->ops->deinit(tcpc_dev);

	return 0;
}

int tcpm_inquire_remote_cc(struct tcpc_device *tcpc_dev,
	uint8_t *cc1, uint8_t *cc2, bool from_ic)
{
	int rv = 0;

	if (from_ic) {
		rv = tcpci_get_cc(tcpc_dev);
		if (rv < 0)
			return rv;
	}

	*cc1 = tcpc_dev->typec_remote_cc[0];
	*cc2 = tcpc_dev->typec_remote_cc[1];
	return 0;
}

int tcpm_inquire_typec_remote_rp_curr(struct tcpc_device *tcpc_dev)
{
	int rp_lvl, ret = 0;

	if (tcpm_check_typec_attached(tcpc_dev))
		return 0;
	rp_lvl = tcpc_dev->typec_remote_rp_level;
	switch (rp_lvl) {
	case TYPEC_CC_VOLT_SNK_DFT:
		ret = 500;
		break;
	case TYPEC_CC_VOLT_SNK_1_5:
		ret = 1500;
		break;
	case TYPEC_CC_VOLT_SNK_3_0:
		ret = 3000;
		break;
	default:
		break;
	}
	return ret;
}

int tcpm_inquire_vbus_level(
	struct tcpc_device *tcpc_dev, bool from_ic)
{
	int rv = 0;
	uint16_t power_status = 0;

	if (from_ic) {
		rv = tcpci_get_power_status(tcpc_dev, &power_status);
		if (rv < 0)
			return rv;
	}

	return tcpc_dev->vbus_level;
}

bool tcpm_inquire_cc_polarity(
	struct tcpc_device *tcpc_dev)
{
	return tcpc_dev->typec_polarity;
}

uint8_t tcpm_inquire_typec_attach_state(
	struct tcpc_device *tcpc_dev)
{
	return tcpc_dev->typec_attach_new;
}

uint8_t tcpm_inquire_typec_role(
	struct tcpc_device *tcpc_dev)
{
	return tcpc_dev->typec_role;
}

uint8_t tcpm_inquire_typec_local_rp(
	struct tcpc_device *tcpc_dev)
{
	uint8_t level;

	switch (tcpc_dev->typec_local_rp_level) {
	case TYPEC_CC_RP_1_5:
		level = 1;
		break;

	case TYPEC_CC_RP_3_0:
		level = 2;
		break;

	default:
	case TYPEC_CC_RP_DFT:
		level = 0;
		break;
	}

	return level;
}

int tcpm_typec_set_wake_lock(
	struct tcpc_device *tcpc, bool user_lock)
{
	int ret;

	mutex_lock(&tcpc->access_lock);
	ret = tcpci_set_wake_lock(
		tcpc, tcpc->wake_lock_pd, user_lock);
	tcpc->wake_lock_user = user_lock;
	mutex_unlock(&tcpc->access_lock);

	return ret;
}

int tcpm_typec_set_usb_sink_curr(
	struct tcpc_device *tcpc_dev, int curr)
{
	bool force_sink_vbus = true;

#ifdef CONFIG_USB_POWER_DELIVERY
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	mutex_lock(&pd_port->pd_lock);

	if (pd_port->pe_data.pd_prev_connected)
		force_sink_vbus = false;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	tcpc_dev->typec_usb_sink_curr = curr;

	if (tcpc_dev->typec_remote_rp_level != TYPEC_CC_VOLT_SNK_DFT)
		force_sink_vbus = false;

	if (force_sink_vbus) {
		tcpci_sink_vbus(tcpc_dev,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, -1);
	}

#ifdef CONFIG_USB_POWER_DELIVERY
	mutex_unlock(&pd_port->pd_lock);
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return 0;
}

int tcpm_typec_set_rp_level(
	struct tcpc_device *tcpc_dev, uint8_t level)
{
	uint8_t res;

	if (level == 2)
		res = TYPEC_CC_RP_3_0;
	else if (level == 1)
		res = TYPEC_CC_RP_1_5;
	else
		res = TYPEC_CC_RP_DFT;

	return tcpc_typec_set_rp_level(tcpc_dev, res);
}

int tcpm_typec_set_custom_hv(struct tcpc_device *tcpc, bool en)
{
#ifdef CONFIG_TYPEC_CAP_CUSTOM_HV
	int ret;

	mutex_lock(&tcpc->access_lock);
	ret = tcpm_check_typec_attached(tcpc);
	if (ret == TCPM_SUCCESS)
		tcpc->typec_during_custom_hv = en;
	mutex_unlock(&tcpc->access_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_HV */
}

int tcpm_typec_role_swap(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	int ret = tcpm_check_typec_attached(tcpc_dev);

	if (ret != TCPM_SUCCESS)
		return ret;

	return tcpc_typec_swap_role(tcpc_dev);
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif /* CONFIG_TYPEC_CAP_ROLE_SWAP */
}

int tcpm_typec_change_role(
	struct tcpc_device *tcpc_dev, uint8_t typec_role)
{
	return tcpc_typec_change_role(tcpc_dev, typec_role);
}

int tcpm_typec_error_recovery(struct tcpc_device *tcpc_dev)
{
	return tcpc_typec_error_recovery(tcpc_dev);
}

int tcpm_typec_disable_function(
	struct tcpc_device *tcpc_dev, bool disable)
{
	if (disable)
		return tcpc_typec_disable(tcpc_dev);
	else
		return tcpc_typec_enable(tcpc_dev);
}

#ifdef CONFIG_USB_POWER_DELIVERY

bool tcpm_inquire_pd_connected(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->pe_data.pd_connected;
}

bool tcpm_inquire_pd_prev_connected(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->pe_data.pd_prev_connected;
}

uint8_t tcpm_inquire_pd_data_role(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->data_role;
}

uint8_t tcpm_inquire_pd_power_role(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->power_role;
}

uint8_t tcpm_inquire_pd_vconn_role(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->vconn_role;
}

uint8_t tcpm_inquire_pd_pe_ready(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->pe_data.pe_ready;
}

uint8_t tcpm_inquire_cable_current(
	struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	if (pd_port->pe_data.power_cable_present)
		return pd_get_cable_curr_lvl(pd_port)+1;

	return PD_CABLE_CURR_UNKNOWN;
}

uint32_t tcpm_inquire_dpm_flags(struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->pe_data.dpm_flags;
}

uint32_t tcpm_inquire_dpm_caps(struct tcpc_device *tcpc_dev)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	return pd_port->dpm_caps;
}

void tcpm_set_dpm_caps(struct tcpc_device *tcpc_dev, uint32_t caps)
{
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	mutex_lock(&pd_port->pd_lock);
	pd_port->dpm_caps = caps;
	mutex_unlock(&pd_port->pd_lock);
}

/* Inquire TCPC to get PD Information */

int tcpm_inquire_pd_contract(
	struct tcpc_device *tcpc, int *mv, int *ma)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if ((mv == NULL) || (ma == NULL))
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.explicit_contract) {
		*mv = pd_port->request_v;
		*ma = pd_port->request_i;
	} else
		ret = TCPM_ERROR_NO_EXPLICIT_CONTRACT;
	mutex_unlock(&pd_port->pd_lock);

	return ret;

}

int tcpm_inquire_cable_inform(
	struct tcpc_device *tcpc, uint32_t *vdos)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (vdos == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.power_cable_present) {
		memcpy(vdos, pd_port->pe_data.cable_vdos,
			sizeof(uint32_t) * VDO_MAX_NR);
	} else
		ret = TCPM_ERROR_NO_POWER_CABLE;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

int tcpm_inquire_pd_partner_inform(
	struct tcpc_device *tcpc, uint32_t *vdos)
{
#ifdef CONFIG_USB_PD_KEEP_PARTNER_ID
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (vdos == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.partner_id_present) {
		memcpy(vdos, pd_port->pe_data.partner_vdos,
			sizeof(uint32_t) * VDO_MAX_NR);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_USB_PD_KEEP_PARTNER_ID */
}

int tcpm_inquire_pd_partner_svids(
	struct tcpc_device *tcpc, struct tcpm_svid_list *list)
{
#ifdef CONFIG_USB_PD_KEEP_SVIDS
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;
	struct svdm_svid_list *svdm_list = &pd_port->pe_data.remote_svid_list;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (list == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (svdm_list->cnt) {
		list->cnt = svdm_list->cnt;
		memcpy(list->svids, svdm_list->svids,
			sizeof(uint16_t) * svdm_list->cnt);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_USB_PD_KEEP_SVIDS */
}

int tcpm_inquire_pd_partner_modes(
	struct tcpc_device *tcpc, uint16_t svid, struct tcpm_mode_list *list)
{
#ifdef CONFIG_USB_PD_ALT_MODE
	int ret = TCPM_SUCCESS;
	struct svdm_svid_data *svid_data;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);

	svid_data =
		dpm_get_svdm_svid_data(pd_port, USB_SID_DISPLAYPORT);

	if (svid_data == NULL) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_ERROR_PARAMETER;
	}

	if (svid_data->remote_mode.mode_cnt) {
		list->cnt = svid_data->remote_mode.mode_cnt;
		memcpy(list->modes, svid_data->remote_mode.mode_vdo,
			sizeof(uint32_t) * list->cnt);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_USB_PD_KEEP_SVIDS */
}

int tcpm_inquire_pd_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.remote_src_cap.nr) {
		cap->cnt = pd_port->pe_data.remote_src_cap.nr;
		memcpy(cap->pdos, pd_port->pe_data.remote_src_cap.pdos,
			sizeof(uint32_t) * cap->cnt);
	} else
		ret = TCPM_ERROR_NO_SOURCE_CAP;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

int tcpm_inquire_pd_sink_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.remote_snk_cap.nr) {
		cap->cnt = pd_port->pe_data.remote_snk_cap.nr;
		memcpy(cap->pdos, pd_port->pe_data.remote_snk_cap.pdos,
			sizeof(uint32_t) * cap->cnt);
	} else
		ret = TCPM_ERROR_NO_SINK_CAP;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

bool tcpm_extract_power_cap_val(
	uint32_t pdo, struct tcpm_power_cap_val *cap)
{
	struct dpm_pdo_info_t info;

	dpm_extract_pdo_info(pdo, &info);

	cap->type = info.type;
	cap->min_mv = info.vmin;
	cap->max_mv = info.vmax;

	if (info.type == DPM_PDO_TYPE_BAT)
		cap->uw = info.uw;
	else
		cap->ma = info.ma;

#ifdef CONFIG_USB_PD_REV30
	if (info.type == DPM_PDO_TYPE_APDO) {
		cap->apdo_type = info.apdo_type;
		cap->pwr_limit = info.pwr_limit;
	}
#endif	/* CONFIG_USB_PD_REV30 */

	return cap->type != TCPM_POWER_CAP_VAL_TYPE_UNKNOWN;
}

extern bool tcpm_extract_power_cap_list(
	struct tcpm_power_cap *cap, struct tcpm_power_cap_list *cap_list)
{
	uint8_t i;

	cap_list->nr = cap->cnt;
	for (i = 0; i < cap_list->nr; i++) {
		if (!tcpm_extract_power_cap_val(
			cap->pdos[i], &cap_list->cap_val[i]))
			return false;
	}

	return true;
}

int tcpm_get_remote_power_cap(struct tcpc_device *tcpc_dev,
		struct tcpm_remote_power_cap *remote_cap)
{
	struct tcpm_power_cap_val cap;
	int i;

	remote_cap->selected_cap_idx = tcpc_dev->
			pd_port.pe_data.remote_selected_cap;
	remote_cap->nr = tcpc_dev->pd_port.pe_data.remote_src_cap.nr;
	for (i = 0; i < remote_cap->nr; i++) {
		tcpm_extract_power_cap_val(tcpc_dev->
			pd_port.pe_data.remote_src_cap.pdos[i], &cap);
		remote_cap->max_mv[i] = cap.max_mv;
		remote_cap->min_mv[i] = cap.min_mv;
		remote_cap->ma[i] = cap.ma;
		remote_cap->type[i] = cap.type;
	}
	return TCPM_SUCCESS;
}

int tcpm_set_remote_power_cap(struct tcpc_device *tcpc_dev,
				int mv, int ma)
{
	return tcpm_dpm_pd_request(tcpc_dev, mv, ma, NULL);
}

static inline int __tcpm_inquire_select_source_cap(
	struct pd_port *pd_port, struct tcpm_power_cap_val *cap_val)
{
	uint8_t sel;
	struct pe_data *pe_data = &pd_port->pe_data;

	if (pd_port->power_role != PD_ROLE_SINK)
		return TCPM_ERROR_POWER_ROLE;

	sel = RDO_POS(pd_port->last_rdo) - 1;
	if (sel > pe_data->remote_src_cap.nr)
		return TCPM_ERROR_NO_SOURCE_CAP;

	if (!tcpm_extract_power_cap_val(
		pe_data->remote_src_cap.pdos[sel], cap_val))
		return TCPM_ERROR_NOT_FOUND;

	return TCPM_SUCCESS;
}

int tcpm_inquire_select_source_cap(
		struct tcpc_device *tcpc, struct tcpm_power_cap_val *cap_val)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	if (cap_val == NULL)
		return TCPM_ERROR_PARAMETER;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	mutex_lock(&pd_port->pd_lock);
	ret = __tcpm_inquire_select_source_cap(pd_port, cap_val);
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}


/* Request TCPC to send PD Request */

#ifdef CONFIG_USB_PD_BLOCK_TCPM

#define TCPM_BK_PD_CMD_TOUT	500

/* tPSTransition 550 ms */
#define TCPM_BK_REQUEST_TOUT		1500

/* tPSSourceOff 920 ms,	tPSSourceOn 480 ms*/
#define TCPM_BK_PR_SWAP_TOUT		2500
#define TCPM_BK_HARD_RESET_TOUT	3500

static int tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size);
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

int tcpm_put_tcp_dpm_event_cb(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	event->user_data = cb_data->user_data;
	event->event_cb = cb_data->event_cb;

	return tcpm_put_tcp_dpm_event(tcpc, event);
}

static int tcpm_put_tcp_dpm_event_cbk1(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data, uint32_t tout_ms)
{
#ifdef CONFIG_USB_PD_BLOCK_TCPM
	if (cb_data == NULL) {
		return tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, NULL, 0);
	}
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

	return tcpm_put_tcp_dpm_event_cb(tcpc, event, cb_data);
}

int tcpm_dpm_pd_power_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_PR_SWAP_AS_SNK + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PR_SWAP_TOUT);
}

int tcpm_dpm_pd_data_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DR_SWAP_AS_UFP + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_pd_vconn_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_VCONN_SWAP_OFF + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_pd_goto_min(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GOTOMIN,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_pd_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_SOFTRESET,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}

int tcpm_dpm_pd_get_source_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SOURCE_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}

int tcpm_dpm_pd_get_sink_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SINK_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_pd_request(struct tcpc_device *tcpc,
	int mv, int ma, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST,

		.tcp_dpm_data.pd_req.mv = mv,
		.tcp_dpm_data.pd_req.ma = ma,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}

int tcpm_dpm_pd_request_ex(struct tcpc_device *tcpc,
	uint8_t pos, uint32_t max, uint32_t oper,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_EX,
		.tcp_dpm_data.pd_req_ex.pos = pos,


	};

	if (oper > max)
		return TCPM_ERROR_PARAMETER;

	tcp_event.tcp_dpm_data.pd_req_ex.max = max;
	tcp_event.tcp_dpm_data.pd_req_ex.oper = oper;

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}

int tcpm_dpm_pd_bist_cm2(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_BIST_CM2,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

#ifdef CONFIG_USB_PD_REV30

static int tcpm_put_tcp_dpm_event_cbk2(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
#ifdef CONFIG_USB_PD_BLOCK_TCPM
	if (cb_data == NULL) {
		return tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, data, size);
	}
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

	return tcpm_put_tcp_dpm_event_cb(tcpc, event, cb_data);
}

int tcpm_dpm_pd_get_source_cap_ext(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_source_cap_ext *src_cap_ext)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SOURCE_CAP_EXT,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) src_cap_ext, PD_SCEDB_SIZE);
}

int tcpm_dpm_pd_fast_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	return TCPM_ERROR_NO_SUPPORT;
}

int tcpm_dpm_pd_get_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data, struct pd_status *status)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_STATUS,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) status, PD_SDB_SIZE);
}

int tcpm_dpm_pd_get_pps_status_raw(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status_raw *pps_status)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_PPS_STATUS,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *)pps_status, PD_PPSSDB_SIZE);
}

int tcpm_dpm_pd_get_pps_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status *pps_status)
{
	int ret;
	struct pd_pps_status_raw pps_status_raw;

	ret = tcpm_dpm_pd_get_pps_status_raw(
		tcpc, cb_data, &pps_status_raw);

	if (ret != 0)
		return ret;

	if (pps_status_raw.output_vol_raw == 0xffff)
		pps_status->output_mv = -1;
	else
		pps_status->output_mv =
			PD_PPS_GET_OUTPUT_MV(pps_status_raw.output_vol_raw);

	if (pps_status_raw.output_curr_raw == 0xff)
		pps_status->output_ma = -1;
	else
		pps_status->output_ma =
			PD_PPS_GET_OUTPUT_MA(pps_status_raw.output_curr_raw);

	pps_status->real_time_flags = pps_status_raw.real_time_flags;
	return ret;
}

int tcpm_dpm_pd_get_country_code(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_country_codes *ccdb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_COUNTRY_CODE,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) ccdb, PD_CCDB_MAX_SIZE);
}

int tcpm_dpm_pd_get_country_info(struct tcpc_device *tcpc, uint32_t ccdo,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_country_info *cidb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_COUNTRY_INFO,
		.tcp_dpm_data.index = ccdo,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) cidb, PD_CIDB_MAX_SIZE);
}

int tcpm_dpm_pd_get_bat_cap(struct tcpc_device *tcpc,
	struct pd_get_battery_capabilities *gbcdb,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_battery_capabilities *bcdb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_BAT_CAP,
		.tcp_dpm_data.gbcdb = *gbcdb,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) bcdb, PD_BCDB_SIZE);
}

int tcpm_dpm_pd_get_bat_status(struct tcpc_device *tcpc,
	struct pd_get_battery_status *gbsdb,
	const struct tcp_dpm_event_cb_data *cb_data,
	uint32_t *bsdo)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_BAT_STATUS,
		.tcp_dpm_data.gbsdb = *gbsdb,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) bsdo, sizeof(uint32_t) * PD_BSDO_SIZE);
}

int tcpm_dpm_pd_get_mfrs_info(struct tcpc_device *tcpc,
	struct pd_get_manufacturer_info *gmidb,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_manufacturer_info *midb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_MFRS_INFO,
		.tcp_dpm_data.gmidb = *gmidb,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) midb, PD_MIDB_MAX_SIZE);
}

int tcpm_dpm_pd_alert(struct tcpc_device *tcpc,
	uint32_t ado, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ALERT,
		.tcp_dpm_data.index = ado,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

#endif	/* CONFIG_USB_PD_REV30 */

int tcpm_dpm_pd_hard_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_HARD_RESET,
	};

	/* check not block case !!! */
	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_HARD_RESET_TOUT);
}

int tcpm_dpm_pd_error_recovery(struct tcpc_device *tcpc)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ERROR_RECOVERY,
	};

	if (tcpm_put_tcp_dpm_event(tcpc, &tcp_event) != TCPM_SUCCESS)
		tcpm_typec_error_recovery(tcpc);

	return TCPM_SUCCESS;
}

int tcpm_dpm_pd_cable_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_CABLE_SOFTRESET,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_discover_cable(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_CABLE,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_discover_id(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_ID,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_discover_svid(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_SVIDS,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_discover_mode(struct tcpc_device *tcpc,
	uint16_t svid, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_MODES,
		.tcp_dpm_data.svdm_data.svid = svid,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ENTER_MODE,
		.tcp_dpm_data.svdm_data.svid = svid,
		.tcp_dpm_data.svdm_data.ops = ops,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_exit_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_EXIT_MODE,
		.tcp_dpm_data.svdm_data.svid = svid,
		.tcp_dpm_data.svdm_data.ops = ops,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_vdm_attention(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ATTENTION,
		.tcp_dpm_data.svdm_data.svid = svid,
		.tcp_dpm_data.svdm_data.ops = ops,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

#ifdef CONFIG_USB_PD_ALT_MODE

int tcpm_inquire_dp_ufp_u_state(
	struct tcpc_device *tcpc, uint8_t *state)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (state == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	*state = pd_get_dp_data(pd_port)->ufp_u_state;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

int tcpm_dpm_dp_attention(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DP_ATTENTION,
		.tcp_dpm_data.dp_data.val = dp_status,
		.tcp_dpm_data.dp_data.mask = mask,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

int tcpm_inquire_dp_dfp_u_state(
	struct tcpc_device *tcpc, uint8_t *state)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (state == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	*state = pd_get_dp_data(pd_port)->dfp_u_state;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

int tcpm_dpm_dp_status_update(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DP_STATUS_UPDATE,
		.tcp_dpm_data.dp_data.val = dp_status,
		.tcp_dpm_data.dp_data.mask = mask,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

int tcpm_dpm_dp_config(struct tcpc_device *tcpc,
	uint32_t dp_config, uint32_t mask,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DP_CONFIG,
		.tcp_dpm_data.dp_data.val = dp_config,
		.tcp_dpm_data.dp_data.mask = mask,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}

#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM

int tcpm_dpm_send_custom_vdm(
	struct tcpc_device *tcpc,
	struct tcp_dpm_custom_vdm_data *vdm_data,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	int ret;
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_UVDM,
	};

	if (vdm_data->cnt > PD_DATA_OBJ_SIZE)
		return TCPM_ERROR_PARAMETER;

	memcpy(&tcp_event.tcp_dpm_data.vdm_data,
		vdm_data, sizeof(struct tcp_dpm_custom_vdm_data));

	/* Check it later */
	ret = tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	if ((ret == TCP_DPM_RET_SUCCESS)
		&& (cb_data == NULL) && vdm_data->wait_resp) {

		vdm_data->cnt = tcpc->pd_port.uvdm_cnt;
		memcpy(vdm_data->vdos,
			tcpc->pd_port.uvdm_data,
			sizeof(uint32_t) * vdm_data->cnt);
	}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	return ret;
}
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
void tcpm_replace_curr_tcp_event(
	struct pd_port *pd_port, struct tcp_dpm_event *event)
{
	int reason = TCP_DPM_RET_DENIED_UNKNOWN;

	switch (event->event_id) {
	case TCP_DPM_EVT_HARD_RESET:
		reason = TCP_DPM_RET_DROP_SENT_HRESET;
		break;
	case TCP_DPM_EVT_ERROR_RECOVERY:
		reason = TCP_DPM_RET_DROP_ERROR_REOCVERY;
		break;
	}

	mutex_lock(&pd_port->pd_lock);
	pd_notify_tcp_event_2nd_result(pd_port, reason);
	pd_port->tcp_event_drop_reset_once = true;
	pd_port->tcp_event_id_2nd = event->event_id;
	memcpy(&pd_port->tcp_event, event, sizeof(struct tcp_dpm_event));
	mutex_unlock(&pd_port->pd_lock);
}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event)
{
	int ret;
	bool imme = event->event_id >= TCP_DPM_EVT_IMMEDIATELY;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS &&
		(imme && ret == TCPM_ERROR_UNATTACHED))
		return ret;

	if (imme) {
		ret = pd_put_tcp_pd_event(pd_port, event->event_id);

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
		if (ret)
			tcpm_replace_curr_tcp_event(pd_port, event);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
	} else
		ret = pd_put_deferred_tcp_event(tcpc, event);

	if (!ret)
		return TCPM_ERROR_PUT_EVENT;

	return TCPM_SUCCESS;
}

int tcpm_notify_vbus_stable(
	struct tcpc_device *tcpc_dev)
{
#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	tcpc_disable_timer(tcpc_dev, PD_TIMER_VBUS_STABLE);
#endif

	pd_put_vbus_stable_event(tcpc_dev);
	return TCPM_SUCCESS;
}

uint8_t tcpm_inquire_pd_charging_policy(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->dpm_charging_policy;
}

uint8_t tcpm_inquire_pd_charging_policy_default(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->dpm_charging_policy_default;
}

int tcpm_reset_pd_charging_policy(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	return tcpm_set_pd_charging_policy(
		tcpc, tcpc->pd_port.dpm_charging_policy_default, cb_data);
}

int tcpm_set_pd_charging_policy_default(
	struct tcpc_device *tcpc, uint8_t policy)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	/* PPS should not be default charging policy ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	pd_port->dpm_charging_policy_default = policy;
	mutex_unlock(&pd_port->pd_lock);

	return TCPM_SUCCESS;
}

int tcpm_set_pd_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_port->dpm_charging_policy == policy)
		return TCPM_SUCCESS;

	/* PPS should call another function ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	pd_port->dpm_charging_policy = policy;
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
int tcpm_set_direct_charge_en(struct tcpc_device *tcpc, bool en)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	tcpc->pd_during_direct_charge = en;
	mutex_unlock(&pd_port->pd_lock);

	return 0;
}

bool tcpm_inquire_during_direct_charge(struct tcpc_device *tcpc)
{
	return tcpc->pd_during_direct_charge;
}
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

static int tcpm_put_tcp_dummy_event(struct tcpc_device *tcpc)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DUMMY,
	};

	return tcpm_put_tcp_dpm_event(tcpc, &tcp_event);
}

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE

int tcpm_dpm_set_vconn_supply_mode(struct tcpc_device *tcpc, uint8_t mode)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	tcpc->tcpc_vconn_supply = mode;
	dpm_reaction_set(pd_port, DPM_REACTION_DYNAMIC_VCONN);
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dummy_event(tcpc);
}

#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_USB_PD_REV30_PPS_SINK

int tcpm_set_apdo_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, int mv, int ma,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_port->dpm_charging_policy == policy) {
		TCPC_INFO("BUG!!! FIX IT!!!\r\n");
		return tcpm_dpm_pd_request(tcpc, mv, ma, NULL);
		/* return TCPM_ERROR_REPEAT_POLICY; */
	}

	/* Not PPS should call another function ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) < DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);

	if (pd_port->pd_connect_state != PD_CONNECT_PE_READY_SNK_APDO) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_ERROR_INVALID_POLICY;
	}

	pd_port->dpm_charging_policy = policy;
	pd_port->request_v_apdo = mv;
	pd_port->request_i_apdo = ma;
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}

int tcpm_inquire_pd_source_apdo(struct tcpc_device *tcpc,
	uint8_t apdo_type, uint8_t *cap_i, struct tcpm_power_cap_val *cap_val)
{
	int ret;
	uint8_t i;
	struct tcpm_power_cap cap;

	ret = tcpm_inquire_pd_source_cap(tcpc, &cap);
	if (ret != TCPM_SUCCESS)
		return ret;

	for (i = *cap_i; i < cap.cnt; i++) {
		if (!tcpm_extract_power_cap_val(cap.pdos[i], cap_val))
			continue;

		if (cap_val->type != DPM_PDO_TYPE_APDO)
			continue;

		if ((cap_val->apdo_type & TCPM_APDO_TYPE_MASK) != apdo_type)
			continue;

		*cap_i = i+1;
		return TCPM_SUCCESS;
	}

	return TCPM_ERROR_NOT_FOUND;
}

bool tcpm_inquire_during_pps_charge(struct tcpc_device *tcpc)
{
	int ret;
	struct tcpm_power_cap_val cap = {0};

	ret = tcpm_inquire_select_source_cap(tcpc, &cap);
	if (ret != 0)
		return false;

	return (cap.type == TCPM_POWER_CAP_VAL_TYPE_AUGMENT);
}

#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#ifdef CONFIG_USB_PD_REV30_BAT_INFO

static void tcpm_alert_bat_changed(
	struct pd_port *pd_port, enum pd_battery_reference ref)
{
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	uint8_t fixed = 0, swap = 0;

	if (ref > PD_BAT_REF_SWAP0)
		swap = 1 << (ref - PD_BAT_REF_SWAP0);
	else
		fixed = 1 << (ref - PD_BAT_REF_FIXED0);

	pd_port->pe_data.local_alert |=
		ADO(ADO_ALERT_BAT_CHANGED, fixed, swap);
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
}

static inline uint16_t tcpm_calc_battery_cap(
	struct pd_battery_info *battery_info, uint16_t soc)
{
	uint16_t wh;
	struct pd_battery_capabilities *bat_cap;

	bat_cap = &battery_info->bat_cap;

	if (bat_cap->bat_last_full_cap != BSDO_BAT_CAP_UNKNOWN)
		wh = bat_cap->bat_last_full_cap;
	else if (bat_cap->bat_design_cap != BSDO_BAT_CAP_UNKNOWN)
		wh = bat_cap->bat_design_cap;
	else
		wh = BSDO_BAT_CAP_UNKNOWN;

	if (wh != BSDO_BAT_CAP_UNKNOWN)
		wh = wh * soc / 1000;

	return wh;
}

static int tcpm_update_bsdo(
	struct pd_port *pd_port, enum pd_battery_reference ref,
	uint16_t soc, uint16_t wh, uint8_t status)
{
	uint8_t status_old;
	struct pd_battery_info *battery_info;

	battery_info = pd_get_battery_info(pd_port, ref);
	if (battery_info == NULL)
		return TCPM_ERROR_PARAMETER;

	status_old = BSDO_BAT_INFO(battery_info->bat_status);

	if (soc != 0)
		wh = tcpm_calc_battery_cap(battery_info, soc);

	battery_info->bat_status = BSDO(wh, status);

	if (status_old != status) {
		tcpm_alert_bat_changed(pd_port, ref);
		return TCPM_ALERT;
	}

	return TCPM_SUCCESS;
}

int tcpm_update_bat_status_wh(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_status_wh_no_mutex(tcpc, ref, status, wh);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	if (ret == TCPM_ALERT) {
		ret = TCPM_SUCCESS;
		tcpm_put_tcp_dummy_event(tcpc);
	}

	return ret;
}

int tcpm_update_bat_status_wh_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	return tcpm_update_bsdo(&tcpc->pd_port, ref, 0, wh, status);
}

int tcpm_update_bat_status_soc(struct tcpc_device *tcpc,
	uint8_t status, uint16_t soc)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_status_soc_no_mutex(tcpc, status, soc);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	return ret;
}

int tcpm_update_bat_status_soc_no_mutex(struct tcpc_device *tcpc,
	uint8_t status, uint16_t soc)
{
	int ret;
	uint8_t i;
	enum pd_battery_reference ref;

	for (i = 0; i < pd_get_fix_battery_nr(&tcpc->pd_port); i++) {
		ref = PD_BAT_REF_FIXED0 + i;
		ret = tcpm_update_bsdo(&tcpc->pd_port, ref, soc, 0, status);
		if (ret != TCPM_SUCCESS)
			return ret;
	}

	return TCPM_SUCCESS;
}

int tcpm_update_bat_last_full(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_last_full_no_mutex(tcpc, ref, wh);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	return ret;
}

int tcpm_update_bat_last_full_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	struct pd_battery_info *battery_info;

	battery_info = pd_get_battery_info(&tcpc->pd_port, ref);
	if (battery_info == NULL)
		return TCPM_ERROR_PARAMETER;

	battery_info->bat_cap.bat_last_full_cap = wh;
	return TCPM_SUCCESS;
}

#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL

static void tcpm_alert_status_changed(
	struct tcpc_device *tcpc, uint8_t ado_type)
{
#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	tcpc->pd_port.pe_data.local_alert |= ADO(ado_type, 0, 0);
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
}

int tcpm_update_pd_status_temp(struct tcpc_device *tcpc,
	enum pd_present_temperature_flag ptf_new, uint8_t temperature)
{
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP
	uint8_t ado_type = 0;
	enum pd_present_temperature_flag ptf_now;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);

	ptf_now = PD_STATUS_TEMP_PTF(pd_port->pd_status_temp_status);

	if (ptf_now != ptf_new) {
		if ((ptf_new >= PD_PTF_WARNING) ||
			(ptf_now >= PD_PTF_WARNING))
			ado_type |= ADO_ALERT_OPER_CHANGED;

		if (ptf_new == PD_PTF_OVER_TEMP) {
			ado_type |= ADO_ALERT_OTP;
			pd_port->pe_data.pd_status_event |= PD_STATUS_EVENT_OTP;
		}

		pd_port->pd_status_temp_status =
			PD_STATUS_TEMP_SET_PTF(ptf_new);
	}

	pd_port->pd_status_temp = temperature;
	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
#else
	return TCPM_ERROR_NO_IMPLEMENT;
#endif /* CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP */
}

int tcpm_update_pd_status_input(
	struct tcpc_device *tcpc, uint8_t input_new, uint8_t mask)
{
	uint8_t input;
	uint8_t ado_type = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	input = pd_port->pd_status_present_in;
	input &= ~mask;
	input |= (input_new & mask);

	if (input != pd_port->pd_status_present_in) {
		ado_type = ADO_ALERT_SRC_IN_CHANGED;
		pd_port->pd_status_present_in = input;
	}

	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
}

int tcpm_update_pd_status_bat_input(
	struct tcpc_device *tcpc, uint8_t bat_input, uint8_t bat_mask)
{
	uint8_t input;
	uint8_t ado_type = 0;
	const uint8_t flag = PD_STATUS_INPUT_INT_POWER_BAT;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	input = pd_port->pd_status_bat_in;
	input &= ~bat_mask;
	input |= (bat_input & bat_mask);

	if (input != pd_port->pd_status_bat_in) {
		if (input)
			pd_port->pd_status_present_in |= flag;
		else
			pd_port->pd_status_present_in &= ~flag;

		ado_type = ADO_ALERT_SRC_IN_CHANGED;
	}

	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
}

int tcpm_update_pd_status_event(
	struct tcpc_device *tcpc, uint8_t evt)
{
	uint8_t ado_type = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	if (evt & PD_STASUS_EVENT_OCP)
		ado_type |= ADO_ALERT_OCP;

	if (evt & PD_STATUS_EVENT_OTP)
		ado_type |= ADO_ALERT_OTP;

	if (evt & PD_STATUS_EVENT_OVP)
		ado_type |= ADO_ALERT_OVP;

	evt &= PD_STASUS_EVENT_MASK;

	mutex_lock(&pd_port->pd_lock);
	pd_port->pe_data.pd_status_event |= evt;
	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
}

#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */

#ifdef CONFIG_USB_PD_BLOCK_TCPM

static const char * const bk_event_ret_name[] = {
	"OK",
	"Unknown",	/* or not support by TCPM */
	"NotReady",
	"LocalCap",
	"PartnerCap",
	"SameRole",
	"InvalidReq",
	"RepeatReq",
	"WrongDR",
	"PDRev",

	"Detach",
	"SReset0",
	"SReset1",
	"HReset0",
	"HReset1",
	"Recovery",
	"BIST",
	"PEBusy",

	"Wait",
	"Reject",
	"TOUT",
	"NAK",
	"NotSupport",

	"BKTOUT",
	"NoResponse",
};

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
static inline void tcpm_dpm_bk_copy_data(struct pd_port *pd_port)
{
	uint8_t size = pd_port->tcpm_bk_cb_data_max;

	if (size >= pd_get_msg_data_size(pd_port))
		size = pd_get_msg_data_size(pd_port);

	if (pd_port->tcpm_bk_cb_data != NULL) {
		memcpy(pd_port->tcpm_bk_cb_data,
			pd_get_msg_data_payload(pd_port), size);
	}
}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

int tcpm_dpm_bk_event_cb(
	struct tcpc_device *tcpc, int ret, struct tcp_dpm_event *event)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_port->tcpm_bk_event_id != event->event_id) {
		TCPM_DBG("bk_event_cb_dummy: expect:%d real:%d\r\n",
			pd_port->tcpm_bk_event_id, event->event_id);
		return 0;
	}

	pd_port->tcpm_bk_ret = ret;
	pd_port->tcpm_bk_done = true;

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	if (ret == TCP_DPM_RET_SUCCESS)
		tcpm_dpm_bk_copy_data(pd_port);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	wake_up_interruptible(&pd_port->tcpm_bk_wait_que);
	return 0;
}

static inline int __tcpm_dpm_wait_bk_event(
	struct pd_port *pd_port, uint32_t tout_ms)
{
	int ret = TCP_DPM_RET_BK_TIMEOUT;

	wait_event_interruptible_timeout(pd_port->tcpm_bk_wait_que,
				pd_port->tcpm_bk_done,
				msecs_to_jiffies(tout_ms));

	if (pd_port->tcpm_bk_done)
		return pd_port->tcpm_bk_ret;
	mutex_lock(&pd_port->pd_lock);

	pd_port->tcpm_bk_event_id = TCP_DPM_EVT_UNKONW;

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcpm_bk_cb_data = NULL;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

int tcpm_dpm_wait_bk_event(struct pd_port *pd_port, uint32_t tout_ms)
{
	int ret = __tcpm_dpm_wait_bk_event(pd_port, tout_ms);

	if (ret < TCP_DPM_RET_NR)
		TCPM_DBG("bk_event_cb -> %s\r\n", bk_event_ret_name[ret]);

	return ret;
}

#ifdef CONFIG_MTK_HANDLE_PPS_TIMEOUT
static void mtk_handle_tcp_event_result(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event, int ret)
{
	struct tcp_dpm_event evt_hreset = {
		.event_id = TCP_DPM_EVT_HARD_RESET,
	};

	if (ret == TCPM_SUCCESS || ret == TCP_DPM_RET_NOT_SUPPORT)
		return;

	if (event->event_id == TCP_DPM_EVT_GET_STATUS
		|| event->event_id == TCP_DPM_EVT_GET_PPS_STATUS) {
		tcpm_put_tcp_dpm_event(tcpc, &evt_hreset);
	}
}
#endif /* CONFIG_MTK_HANDLE_PPS_TIMEOUT */

static int __tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	pd_port->tcpm_bk_done = false;
	pd_port->tcpm_bk_event_id = event->event_id;

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcpm_bk_cb_data = data;
	pd_port->tcpm_bk_cb_data_max = size;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	ret = tcpm_put_tcp_dpm_event(tcpc, event);
	if (ret != TCPM_SUCCESS)
		return ret;

	return tcpm_dpm_wait_bk_event(pd_port, tout_ms);
}

static int tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
	int ret;
	uint8_t retry = CONFIG_USB_PD_TCPM_CB_RETRY;
	struct pd_port *pd_port = &tcpc->pd_port;

	event->event_cb = tcpm_dpm_bk_event_cb;

	mutex_lock(&pd_port->tcpm_bk_lock);

	while (1) {
		ret = __tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, data, size);

		if ((ret != TCP_DPM_RET_TIMEOUT) || (retry == 0))
			break;

		retry--;
	}

	mutex_unlock(&pd_port->tcpm_bk_lock);

#ifndef CONFIG_USB_PD_TCPM_CB_2ND
	if ((data != NULL) && (ret == TCPM_SUCCESS))
		return TCPM_ERROR_EXPECT_CB2;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	if (ret == TCP_DPM_RET_DENIED_REPEAT_REQUEST)
		ret = TCPM_SUCCESS;

#ifdef CONFIG_MTK_HANDLE_PPS_TIMEOUT
	mtk_handle_tcp_event_result(tcpc, event, ret);
#endif /* CONFIG_MTK_HANDLE_PPS_TIMEOUT */

	return ret;
}

#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

#endif /* CONFIG_USB_POWER_DELIVERY */
