/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_dpm_core.c
 * Power Delvery DPM Core Driver
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

#include <linux/delay.h>

#include "inctcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "pd_dpm_prv.h"

typedef struct __pd_device_policy_manager {
	uint8_t temp;
} pd_device_policy_manager_t;

static const svdm_svid_ops_t svdm_svid_ops[] = {
#ifdef CONFIG_USB_PD_ALT_MODE
	{
		.name = "DisplayPort",
		.svid = USB_SID_DISPLAYPORT,

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		.dfp_inform_id = dp_dfp_u_notify_discover_id,
		.dfp_inform_svids = dp_dfp_u_notify_discover_svid,
		.dfp_inform_modes = dp_dfp_u_notify_discover_modes,

		.dfp_inform_enter_mode = dp_dfp_u_notify_enter_mode,
		.dfp_inform_exit_mode = dp_dfp_u_notify_exit_mode,

		.dfp_inform_attention = dp_dfp_u_notify_attention,
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */

		.ufp_request_enter_mode = dp_ufp_u_request_enter_mode,
		.ufp_request_exit_mode = dp_ufp_u_request_exit_mode,

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		.notify_pe_startup = dp_dfp_u_notify_pe_startup,
		.notify_pe_ready = dp_dfp_u_notify_pe_ready,
#endif	/* #ifdef CONFIG_USB_PD_ALT_MODE_DFP */

		.reset_state = dp_reset_state,
	},
#endif	/* CONFIG_USB_PD_ALT_MODE */
};

int dpm_check_supported_modes(void)
{
	int i;
	bool is_disorder = false;
	bool found_error = false;

	for (i = 0; i < ARRAY_SIZE(svdm_svid_ops); i++) {
		if (i < (ARRAY_SIZE(svdm_svid_ops) - 1)) {
			if (svdm_svid_ops[i + 1].svid <=
				svdm_svid_ops[i].svid)
				is_disorder = true;
		}
		pr_info("SVDM supported mode [%d]: name = %s, svid = 0x%x\n",
			i, svdm_svid_ops[i].name,
			svdm_svid_ops[i].svid);
	}
	pr_info("%s : found \"disorder\"...\n", __func__);
	found_error |= is_disorder;
	return found_error ? -EFAULT : 0;
}

/*-----------------------------------------------------------------------------
/ DPM Init
/-----------------------------------------------------------------------------
*/

static void pd_dpm_update_pdos_flags(pd_port_t *pd_port, uint32_t pdo)
{
	pd_port->dpm_flags &= ~DPM_FLAGS_RESET_PARTNER_MASK;

	/* Only update PDO flags if pdo's type is fixed */
	if ((pdo & PDO_TYPE_MASK) != PDO_TYPE_FIXED)
		return;

	if (pdo & PDO_FIXED_DUAL_ROLE)
		pd_port->dpm_flags |= DPM_FLAGS_PARTNER_DR_POWER;

	if (pdo & PDO_FIXED_DATA_SWAP)
		pd_port->dpm_flags |= DPM_FLAGS_PARTNER_DR_DATA;

	if (pdo & PDO_FIXED_EXTERNAL)
		pd_port->dpm_flags |= DPM_FLAGS_PARTNER_EXTPOWER;

	if (pdo & PDO_FIXED_COMM_CAP)
		pd_port->dpm_flags |= DPM_FLAGS_PARTNER_USB_COMM;

}

int pd_dpm_enable_vconn(pd_port_t *pd_port, bool en)
{
	return pd_set_vconn(pd_port, en);
}

int pd_dpm_send_sink_caps(pd_port_t *pd_port)
{
	pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;

	return pd_send_data_msg(pd_port, TCPC_TX_SOP, PD_DATA_SINK_CAP,
		snk_cap->nr, snk_cap->pdos);
}

int pd_dpm_send_source_caps(pd_port_t *pd_port)
{
	uint8_t i;
	uint32_t cable_curr = 3000;

	pd_port_power_caps *src_cap0 = &pd_port->local_src_cap_default;
	pd_port_power_caps *src_cap1 = &pd_port->local_src_cap;

	if (pd_port->power_cable_present) {
		cable_curr =
			pd_extract_cable_curr(
				pd_port->cable_vdos[IDH_PTYPE_ACABLE]);
		DPM_DBG("cable_limit: %dmA\r\n", cable_curr);
	}

	src_cap1->nr = src_cap0->nr;
	for (i = 0; i < src_cap0->nr; i++) {
		src_cap1->pdos[i] =
			pd_reset_pdo_power(src_cap0->pdos[i], cable_curr);
	}

	return pd_send_data_msg(pd_port, TCPC_TX_SOP, PD_DATA_SOURCE_CAP,
		src_cap1->nr, src_cap1->pdos);
}

enum {
	GOOD_PW_NONE = 0,	/* both no GP */
	GOOD_PW_PARTNER,	/* partner has GP */
	GOOD_PW_LOCAL,		/* local has GP */
	GOOD_PW_BOTH,		/* both have GPs */
};

static inline int dpm_check_good_power(pd_port_t *pd_port)
{
	bool local_ex, partner_ex;

	local_ex =
		(pd_port->dpm_caps & DPM_CAP_LOCAL_EXT_POWER) != 0;

	partner_ex =
		(pd_port->dpm_flags & DPM_FLAGS_PARTNER_EXTPOWER) != 0;

	if (local_ex != partner_ex) {
		if (partner_ex)
			return GOOD_PW_PARTNER;
		return GOOD_PW_LOCAL;
	}

	if (local_ex)
		return GOOD_PW_BOTH;

	return GOOD_PW_NONE;
}

static inline bool dpm_response_request(pd_port_t *pd_port, bool accept)
{
	if (accept)
		return pd_put_dpm_ack_event(pd_port);
	return pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_REJECT);
}

/* ---- SNK ---- */

struct dpm_pdo_info_t {
	uint8_t type;
	int vmin;
	int vmax;
	int uw;
	int ma;
};

struct dpm_rdo_info_t {
	uint8_t pos;
	uint8_t type;
	bool mismatch;

	int vmin;
	int vmax;

	union {
		uint32_t max_uw;
		uint32_t max_ma;
	};

	union {
		uint32_t oper_uw;
		uint32_t oper_ma;
	};
};

#define DPM_PDO_TYPE_FIXED	0
#define DPM_PDO_TYPE_BAT	1
#define DPM_PDO_TYPE_VAR	2
#define DPM_PDO_TYPE(pdo)	((pdo & PDO_TYPE_MASK) >> 30)

static inline bool dpm_is_valid_pdo_pair(struct dpm_pdo_info_t *sink,
	struct dpm_pdo_info_t *source, uint32_t caps)
{
	if (sink->vmax < source->vmax)
		return false;

	if (sink->vmin > source->vmin)
		return false;

	if (caps & DPM_CAP_SNK_IGNORE_MISMATCH_CURRENT)
		return (sink->ma <= source->ma);

	return true;
}

static inline void dpm_extract_pdo_info(
			uint32_t pdo, struct dpm_pdo_info_t *info)
{
	memset(info, 0, sizeof(struct dpm_pdo_info_t));

	info->type = DPM_PDO_TYPE(pdo);

	switch (info->type) {
	case DPM_PDO_TYPE_FIXED:
		info->ma = PDO_FIXED_EXTRACT_CURR(pdo);
		info->vmax = info->vmin = PDO_FIXED_EXTRACT_VOLT(pdo);
		info->uw = info->ma * info->vmax;
		break;

	case DPM_PDO_TYPE_VAR:
		info->ma = PDO_VAR_OP_CURR(pdo);
		info->vmin = PDO_VAR_EXTRACT_MIN_VOLT(pdo);
		info->vmax = PDO_VAR_EXTRACT_MAX_VOLT(pdo);
		info->uw = info->ma * info->vmax;
		break;

	case DPM_PDO_TYPE_BAT:
		info->uw = PDO_BATT_EXTRACT_OP_POWER(pdo) * 1000;
		info->vmin = PDO_BATT_EXTRACT_MIN_VOLT(pdo);
		info->vmax = PDO_BATT_EXTRACT_MAX_VOLT(pdo);
		info->ma = info->uw / info->vmin;
		break;
	}
}

#ifndef MIN
#define MIN(a, b)	((a < b) ? (a) : (b))
#endif

static inline int dpm_calc_src_cap_power_uw(
	struct dpm_pdo_info_t *source, struct dpm_pdo_info_t *sink)
{
	int uw, ma;

	if (source->type == DPM_PDO_TYPE_BAT) {
		uw = source->uw;

		if (sink->type == DPM_PDO_TYPE_BAT)
			uw = MIN(uw, sink->uw);
	} else {
		ma = source->ma;

		if (sink->type != DPM_PDO_TYPE_BAT)
			ma = MIN(ma, sink->ma);

		uw = ma * source->vmax;
	}

	return uw;
}

static bool dpm_find_match_req_info(struct dpm_rdo_info_t *req_info,
	uint32_t snk_pdo, int cnt, uint32_t *src_pdos, int min_uw,
	uint32_t caps)
{
	bool overload;
	int ret = -1;
	int i;
	int uw, max_uw = min_uw, cur_mv = 0;
	struct dpm_pdo_info_t sink, source;

	dpm_extract_pdo_info(snk_pdo, &sink);

	for (i = 0; i < cnt; i++) {
		dpm_extract_pdo_info(src_pdos[i], &source);
		if (!dpm_is_valid_pdo_pair(&sink, &source, caps))
			continue;

		uw = dpm_calc_src_cap_power_uw(&source, &sink);

		overload = uw > max_uw;

		if (caps & DPM_CAP_SNK_PREFER_LOW_VOLTAGE)
			overload |= (uw == max_uw) && (source.vmax < cur_mv);

		if (overload) {
			ret = i;
			max_uw = uw;
			cur_mv = source.vmax;
		}
	}

	if (ret >= 0) {
		req_info->pos = ret + 1;
		req_info->type = source.type;

		dpm_extract_pdo_info(src_pdos[ret], &source);

		req_info->vmax = source.vmax;
		req_info->vmin = source.vmin;

		if (sink.type == DPM_PDO_TYPE_BAT)
			req_info->mismatch = max_uw < sink.uw;
		else
			req_info->mismatch = source.ma < sink.ma;

		if (source.type == DPM_PDO_TYPE_BAT) {
			req_info->max_uw = sink.uw;
			req_info->oper_uw = max_uw;
		} else {
			req_info->max_ma = sink.ma;
			req_info->oper_ma = MIN(sink.ma, source.ma);
		}
	}

	return (ret >= 0);
}

static bool dpm_build_request_info(
	pd_port_t *pd_port, struct dpm_rdo_info_t *req_info)
{
	bool find_cap = false;
	int i, max_uw = 0;
	pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	pd_port_power_caps *src_cap = &pd_port->remote_src_cap;

	memset(req_info, 0, sizeof(struct dpm_rdo_info_t));

	for (i = 0; i < src_cap->nr; i++)
		DPM_DBG("SrcCap%d: 0x%08x\r\n", i+1, src_cap->pdos[i]);

	for (i = 0; i < snk_cap->nr; i++) {
		DPM_DBG("EvaSinkCap%d\r\n", i+1);

		find_cap = dpm_find_match_req_info(req_info,
			snk_cap->pdos[i], src_cap->nr, src_cap->pdos,
			max_uw, pd_port->dpm_caps);

		if (find_cap) {
			if (req_info->type == DPM_PDO_TYPE_BAT)
				max_uw = req_info->oper_uw;
			else
				max_uw = req_info->vmax * req_info->oper_ma;

			DPM_DBG("Find SrcCap%d(%s):%d mw\r\n",
				req_info->pos, req_info->mismatch ?
					"Mismatch" : "Match", max_uw/1000);
			pd_port->local_selected_cap = i + 1;
			pd_port->remote_selected_cap = req_info->pos;
		}
	}

	return max_uw != 0;
}

static bool dpm_build_default_request_info(
	pd_port_t *pd_port, struct dpm_rdo_info_t *req_info)
{
	struct dpm_pdo_info_t sink, source;
	pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	pd_port_power_caps *src_cap = &pd_port->remote_src_cap;

	pd_port->local_selected_cap = 1;
	pd_port->remote_selected_cap = 1;

	dpm_extract_pdo_info(snk_cap->pdos[0], &sink);
	dpm_extract_pdo_info(src_cap->pdos[0], &source);

	req_info->pos = 1;
	req_info->type = source.type;
	req_info->mismatch = true;
	req_info->vmax = 5000;
	req_info->vmin = 5000;

	if (req_info->type == DPM_PDO_TYPE_BAT) {
		req_info->max_uw = sink.uw;
		req_info->oper_uw = source.uw;

	} else {
		req_info->max_ma = sink.ma;
		req_info->oper_ma = source.ma;
	}

	return true;
}

static inline void dpm_update_request(
	pd_port_t *pd_port, struct dpm_rdo_info_t *req_info)
{
	uint32_t mw_op, mw_max;

	uint32_t flags = 0;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_GIVE_BACK)
		flags |= RDO_GIVE_BACK;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_NO_SUSPEND)
		flags |= RDO_NO_SUSPEND;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_USB_COMM)
		flags |= RDO_COMM_CAP;

	if (req_info->mismatch)
		flags |= RDO_CAP_MISMATCH;

	pd_port->request_v_new = req_info->vmax;

	if (req_info->type == DPM_PDO_TYPE_BAT) {
		mw_op = req_info->oper_uw / 1000;
		mw_max = req_info->max_uw / 1000;
		pd_port->request_i_new = req_info->oper_uw / req_info->vmin;
		pd_port->last_rdo = RDO_BATT(
				req_info->pos, mw_op, mw_max, flags);
	} else {
		pd_port->request_i_new = req_info->oper_ma;
		pd_port->last_rdo = RDO_FIXED(
			req_info->pos, req_info->oper_ma,
			req_info->max_ma, flags);
	}
}

bool pd_dpm_send_request(pd_port_t *pd_port, int mv, int ma)
{
	bool find_cap = false;
	struct dpm_rdo_info_t req_info;
	pd_port_power_caps *src_cap = &pd_port->remote_src_cap;
	uint32_t snk_pdo = PDO_FIXED(mv, ma, 0);

	memset(&req_info, 0, sizeof(struct dpm_rdo_info_t));

	find_cap = dpm_find_match_req_info(&req_info,
		snk_pdo, src_cap->nr, src_cap->pdos,
		0, pd_port->dpm_caps);

	if (!find_cap)
		return false;

	dpm_update_request(pd_port, &req_info);
	return pd_put_dpm_pd_request_event(pd_port,
			PD_DPM_PD_REQUEST_PW_REQUEST);
}

void pd_dpm_snk_evaluate_caps(pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool find_cap = false;
	int sink_nr, source_nr;

	struct dpm_rdo_info_t req_info;
	pd_msg_t *pd_msg = pd_event->pd_msg;
	pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	pd_port_power_caps *src_cap = &pd_port->remote_src_cap;

	BUG_ON(pd_msg == NULL);

	sink_nr = snk_cap->nr;
	source_nr = PD_HEADER_CNT(pd_msg->msg_hdr);

	if ((source_nr <= 0) || (sink_nr <= 0)) {
		DPM_DBG("SrcNR or SnkNR = 0\r\n");
		return;
	}

	src_cap->nr = source_nr;
	memcpy(src_cap->pdos, pd_msg->payload, sizeof(uint32_t) * source_nr);
	pd_dpm_update_pdos_flags(pd_port, src_cap->pdos[0]);

	find_cap = dpm_build_request_info(pd_port, &req_info);

	/* If we can't find any cap to use, choose default setting */
	if (!find_cap) {
		DPM_DBG("Can't find any SrcCap\r\n");
		dpm_build_default_request_info(pd_port, &req_info);
	}

	dpm_update_request(pd_port, &req_info);

	pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_SOURCE_CAP;
	if (!(pd_port->dpm_flags & DPM_FLAGS_PARTNER_DR_POWER))
		pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_SINK_CAP;

	if (req_info.pos > 0)
		pd_put_dpm_notify_event(pd_port, req_info.pos);
}

void pd_dpm_snk_transition_power(pd_port_t *pd_port, pd_event_t *pd_event)
{
	tcpci_sink_vbus(pd_port->tcpc_dev,
		pd_port->request_v_new, pd_port->request_i_new);

	pd_port->request_v = pd_port->request_v_new;
	pd_port->request_i = pd_port->request_i_new;
}

void pd_dpm_snk_hard_reset(pd_port_t *pd_port, pd_event_t *pd_event)
{
	tcpci_sink_vbus(pd_port->tcpc_dev, 0, 0);
	pd_put_pe_event(pd_port, PD_PE_POWER_ROLE_AT_DEFAULT);
}

/* ---- SRC ---- */

void pd_dpm_src_evaluate_request(pd_port_t *pd_port, pd_event_t *pd_event)
{
	uint8_t rdo_pos;
	uint32_t rdo, pdo;
	uint32_t op_curr, max_curr;
	uint32_t source_vmin, source_vmax, source_i;
	bool accept_request = true;

	pd_msg_t *pd_msg = pd_event->pd_msg;
	pd_port_power_caps *src_cap = &pd_port->local_src_cap;

	BUG_ON(pd_msg == NULL);

	rdo = pd_msg->payload[0];
	rdo_pos = RDO_POS(rdo);

	DPM_DBG("RequestCap%d\r\n", rdo_pos);

	pd_port->dpm_flags &= (~DPM_FLAGS_PARTNER_MISMATCH);
	if ((rdo_pos > 0) && (rdo_pos <= src_cap->nr)) {

		pdo = src_cap->pdos[rdo_pos-1];

		pd_extract_rdo_power(rdo, pdo, &op_curr, &max_curr);
		pd_extract_pdo_power(pdo,
			&source_vmin, &source_vmax, &source_i);

		if (source_i < op_curr) {
			DPM_DBG("src_i (%d) < op_i (%d)\r\n",
							source_i, op_curr);
			accept_request = false;
		}

		if (rdo & RDO_CAP_MISMATCH) {
			/* TODO: handle it later */
			DPM_DBG("CAP_MISMATCH\r\n");
			pd_port->dpm_flags |= DPM_FLAGS_PARTNER_MISMATCH;
		} else if (source_i < max_curr) {
			DPM_DBG("src_i (%d) < max_i (%d)\r\n",
						source_i, max_curr);
			accept_request = false;
		}
	} else {
		accept_request = false;
		DPM_DBG("RequestPos Wrong (%d)\r\n", rdo_pos);
	}

	if (accept_request) {
		pd_port->local_selected_cap = rdo_pos;
		pd_port->request_i_new = op_curr;
		pd_port->request_v_new = source_vmin;
		pd_put_dpm_notify_event(pd_port, rdo_pos);
	} else {

		/*
		 * "Contract Invalid" means that the previously
		 * negotiated Voltage and Current values
		 * are no longer included in the Source��s new Capabilities.
		 * If the Sink fails to make a valid Request in this case
		 * then Power Delivery operation is no longer possible
		 * and Power Delivery mode is exited with a Hard Reset.
		*/

		pd_port->local_selected_cap = 0;
		pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_REJECT_INVALID);
	}
}

void pd_dpm_src_transition_power(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_enable_vbus_stable_detection(pd_port);

	tcpci_source_vbus(pd_port->tcpc_dev,
		pd_port->request_v_new, pd_port->request_i_new);

	if (pd_port->request_v == pd_port->request_v_new)
		pd_put_vbus_stable_event(pd_port->tcpc_dev);
#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	else
		pd_enable_timer(pd_port, PD_TIMER_VBUS_STABLE);
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */

	pd_port->request_v = pd_port->request_v_new;
	pd_port->request_i = pd_port->request_i_new;
}

void pd_dpm_src_inform_cable_vdo(pd_port_t *pd_port, pd_event_t *pd_event)
{
	const int size = sizeof(uint32_t) * VDO_MAX_SIZE;

	if (pd_event->pd_msg)
		memcpy(pd_port->cable_vdos, pd_event->pd_msg->payload, size);
	else
		memset(pd_port->cable_vdos, 0, size);

	pd_put_dpm_ack_event(pd_port);
}

void pd_dpm_src_hard_reset(pd_port_t *pd_port)
{
	tcpci_source_vbus(pd_port->tcpc_dev, TCPC_VBUS_SOURCE_0V, 0);
	pd_enable_vbus_safe0v_detection(pd_port);
}

/* ---- UFP : update_svid_data ---- */

static inline bool dpm_ufp_update_svid_data_enter_mode(
	pd_port_t *pd_port, uint16_t svid, uint8_t ops)
{
	svdm_svid_data_t *svid_data;

	DPM_DBG("EnterMode (svid0x%04x, ops:%d)\r\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	if (svid_data == NULL)
		return false;

	/* Only accept 1 mode active at the same time */
	if (svid_data->active_mode)
		return false;

	if ((ops == 0) || (ops > svid_data->local_mode.mode_cnt))
		return false;

	svid_data->active_mode = ops;
	pd_port->modal_operation = true;

	svdm_ufp_request_enter_mode(pd_port, svid, ops);

	tcpci_enter_mode(pd_port->tcpc_dev,
		svid, ops, svid_data->local_mode.mode_vdo[ops]);
	return true;
}

static inline bool dpm_ufp_update_svid_data_exit_mode(
	pd_port_t *pd_port, uint16_t svid, uint8_t ops)
{
	uint8_t i;
	bool modal_operation;
	svdm_svid_data_t *svid_data;

	DPM_DBG("ExitMode (svid0x%04x, mode:%d)\r\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	if (svid_data == NULL)
		return false;

	if (svid_data->active_mode == 0)
		return false;

	if ((ops == 0) || (ops == svid_data->active_mode)) {
		svid_data->active_mode = 0;

		modal_operation = false;
		for (i = 0; i < pd_port->svid_data_cnt; i++) {
			svid_data = &pd_port->svid_data[i];

			if (svid_data->active_mode) {
				modal_operation = true;
				break;
			}
		}

		pd_port->modal_operation = modal_operation;
/*
#ifdef CONFIG_USB_PD_ALT_MODE
	if (svid == USB_SID_DISPLAYPORT)
		dp_ufp_u_request_exit_mode(pd_port);
#endif
*/
		svdm_ufp_request_exit_mode(pd_port, svid, ops);
		tcpci_exit_mode(pd_port->tcpc_dev, svid);
		return true;
	}

	return false;
}

/* ---- UFP : Evaluate VDM Request ---- */

static inline bool pd_dpm_ufp_reply_request(
	pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	return vdm_put_dpm_event(
		pd_port, ack ? PD_DPM_ACK : PD_DPM_NAK, pd_event->pd_msg);
}

static inline uint32_t dpm_vdm_get_svid(pd_event_t *pd_event)
{
	pd_msg_t *pd_msg = pd_event->pd_msg;

	BUG_ON(pd_msg == NULL);
	return PD_VDO_VID(pd_msg->payload[0]);
}

void pd_dpm_ufp_request_id_info(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_ufp_reply_request(pd_port, pd_event,
			dpm_vdm_get_svid(pd_event) == USB_SID_PD);
}

void pd_dpm_ufp_request_svid_info(pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ack = false;

	if (pd_is_support_modal_operation(pd_port))
		ack = (dpm_vdm_get_svid(pd_event) == USB_SID_PD);
	pd_dpm_ufp_reply_request(pd_port, pd_event, ack);
}

void pd_dpm_ufp_request_mode_info(pd_port_t *pd_port, pd_event_t *pd_event)
{
	uint16_t svid = dpm_vdm_get_svid(pd_event);
	bool ack = dpm_get_svdm_svid_data(pd_port, svid) != NULL;

	pd_dpm_ufp_reply_request(pd_port, pd_event, ack);
}

void pd_dpm_ufp_request_enter_mode(pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ack = false;
	uint16_t svid;
	uint8_t ops;

	BUG_ON(pd_event->pd_msg == NULL);
	dpm_vdm_get_svid_ops(pd_event, &svid, &ops);
	ack = dpm_ufp_update_svid_data_enter_mode(pd_port, svid, ops);

	pd_dpm_ufp_reply_request(pd_port, pd_event, ack);
}

void pd_dpm_ufp_request_exit_mode(pd_port_t *pd_port, pd_event_t *pd_event)
{
	bool ack;
	uint16_t svid;
	uint8_t ops;

	dpm_vdm_get_svid_ops(pd_event, &svid, &ops);
	ack = dpm_ufp_update_svid_data_exit_mode(pd_port, svid, ops);
	pd_dpm_ufp_reply_request(pd_port, pd_event, ack);
}

/* ---- UFP : Response VDM Request ---- */

int pd_dpm_ufp_response_id(pd_port_t *pd_port, pd_event_t *pd_event)
{
	return pd_reply_svdm_request(pd_port, pd_event,
		CMDT_RSP_ACK, pd_port->id_vdo_nr, pd_port->id_vdos);
}

int pd_dpm_ufp_response_svids(pd_port_t *pd_port, pd_event_t *pd_event)
{
	svdm_svid_data_t *svid_data;
	uint16_t svid_list[2];
	uint32_t svids[VDO_MAX_DATA_SIZE];
	uint8_t i = 0, j = 0, cnt = pd_port->svid_data_cnt;

	BUG_ON(pd_port->svid_data_cnt >= VDO_MAX_SVID_SIZE);

	if (unlikely(cnt >= VDO_MAX_SVID_SIZE))
		cnt = VDO_MAX_SVID_SIZE;

	while (i < cnt) {
		svid_data = &pd_port->svid_data[i];
		svid_list[0] = svid_data->svid;

		i++;
		if (i < cnt) {
			svid_data = &pd_port->svid_data[i];
			svid_list[1] = svid_data->svid;
		} else
			svid_list[1] = 0;

		svids[j++] = VDO_SVID(svid_list[0], svid_list[1]);
	}

	if ((cnt % 2) == 0)
		svids[j++] = VDO_SVID(0, 0);

	return pd_reply_svdm_request(
		pd_port, pd_event, CMDT_RSP_ACK, j, svids);
}

int pd_dpm_ufp_response_modes(pd_port_t *pd_port, pd_event_t *pd_event)
{
	svdm_svid_data_t *svid_data;
	uint16_t svid = dpm_vdm_get_svid(pd_event);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data != NULL) {
		return pd_reply_svdm_request(
			pd_port, pd_event, CMDT_RSP_ACK,
			svid_data->local_mode.mode_cnt,
			svid_data->local_mode.mode_vdo);
	} else {
		PE_DBG("ERROR-4965\r\n");
		return pd_reply_svdm_request_simply(
			pd_port, pd_event, CMDT_RSP_NAK);
	}
}

/* ---- DFP : update_svid_data ---- */

static inline void dpm_dfp_update_svid_data_exist(
			pd_port_t *pd_port, uint16_t svid)
{
	uint8_t k;
	svdm_svid_data_t *svid_data;

#ifdef CONFIG_USB_PD_KEEP_SVIDS
	svdm_svid_list_t *list = &pd_port->remote_svid_list;

	if (list->cnt < VDO_MAX_SVID_SIZE)
		list->svids[list->cnt++] = svid;
	else
		DPM_DBG("ERR:SVIDCNT\r\n");
#endif

	for (k = 0; k < pd_port->svid_data_cnt; k++) {

		svid_data = &pd_port->svid_data[k];

		if (svid_data->svid == svid)
			svid_data->exist = 1;
	}
}

static inline void dpm_dfp_update_svid_data_modes(
	pd_port_t *pd_port, uint16_t svid, uint32_t *mode_list, uint8_t count)
{
	uint8_t i;
	svdm_svid_data_t *svid_data;

	DPM_DBG("InformMode (0x%04x:%d): \r\n", svid, count);
	for (i = 0; i < count; i++)
		DPM_DBG("Mode[%d]: 0x%08x\r\n", i, mode_list[i]);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	svid_data->remote_mode.mode_cnt = count;

	if (count != 0) {
		memcpy(svid_data->remote_mode.mode_vdo,
			mode_list, sizeof(uint32_t) * count);
	}
}

static inline void dpm_dfp_update_svid_enter_mode(
	pd_port_t *pd_port, uint16_t svid, uint8_t ops)
{
	svdm_svid_data_t *svid_data;

	DPM_DBG("EnterMode (svid0x%04x, mode:%d)\r\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	svid_data->active_mode = ops;
	pd_port->modal_operation = true;

	tcpci_enter_mode(pd_port->tcpc_dev,
		svid_data->svid, ops, svid_data->remote_mode.mode_vdo[ops]);
}

static inline void dpm_dfp_update_svid_data_exit_mode(
	pd_port_t *pd_port, uint16_t svid, uint8_t ops)
{
	uint8_t i;
	bool modal_operation;
	svdm_svid_data_t *svid_data;

	DPM_DBG("ExitMode (svid0x%04x, mode:%d)\r\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	if ((ops == 0) || (ops == svid_data->active_mode)) {
		svid_data->active_mode = 0;

		modal_operation = false;
		for (i = 0; i < pd_port->svid_data_cnt; i++) {

			svid_data = &pd_port->svid_data[i];

			if (svid_data->active_mode) {
				modal_operation = true;
				break;
			}
		}

		pd_port->modal_operation = modal_operation;
		tcpci_exit_mode(pd_port->tcpc_dev, svid);
	}
}


/* ---- DFP : Inform VDM Result ---- */

#ifdef CONFIG_RT7207_ADAPTER
void rt7207_get_response_msg(pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	pd_msg_t *pd_msg = pd_event->pd_msg;
	struct tcp_notify noti;
	int i;

	if (ack) {
		for (i = 0; i < 7; i++)
			noti.payload[i] = pd_msg->payload[i];
		noti.rt7207_vdm_success = true;
	} else {
		pd_dbg_info("%s get response fail\n", __func__);
		noti.rt7207_vdm_success = false;
	}

	vdm_put_dpm_notified_event(pd_port);
	srcu_notifier_call_chain(&pd_port->tcpc_dev->evt_nh,
					TCP_NOTIFY_RT7207_VDM, &noti);
}
#endif /* CONFIG_RT7207_ADAPTER */

void pd_dpm_dfp_inform_id(pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
#if DPM_DBG_ENABLE
	pd_msg_t *pd_msg = pd_event->pd_msg;
#endif /* DPM_DBG_ENABLE */

	if (ack) {
		DPM_DBG("InformID, 0x%02x, 0x%02x, 0x%02x, 0x%02x\r\n",
				pd_msg->payload[0], pd_msg->payload[1],
				pd_msg->payload[2], pd_msg->payload[3]);
	}

	svdm_dfp_inform_id(pd_port, pd_event, ack);
	vdm_put_dpm_notified_event(pd_port);
}

static inline int dpm_dfp_consume_svids(
	pd_port_t *pd_port, uint32_t *svid_list, uint8_t count)
{
	bool discover_again = true;

	uint8_t i, j;
	uint16_t svid[2];

	DPM_DBG("InformSVID (%d): \r\n", count);

	if (count < 6)
		discover_again = false;

	for (i = 0; i < count; i++) {
		svid[0] = PD_VDO_SVID_SVID0(svid_list[i]);
		svid[1] = PD_VDO_SVID_SVID1(svid_list[i]);

		DPM_DBG("svid[%d]: 0x%04x 0x%04x\r\n", i, svid[0], svid[1]);

		for (j = 0; j < 2; j++) {
			if (svid[j] == 0) {
				discover_again = false;
				break;
			}

			dpm_dfp_update_svid_data_exist(pd_port, svid[j]);
		}
	}

	if (discover_again) {
		DPM_DBG("DiscoverSVID Again\r\n");
		vdm_put_dpm_vdm_request_event(
			pd_port, PD_DPM_VDM_REQUEST_DISCOVER_SVIDS);
		return 1;
	}

	return 0;
}

void pd_dpm_dfp_inform_svids(pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	uint8_t count;
	uint32_t *svid_list;
	pd_msg_t *pd_msg = pd_event->pd_msg;

	if (ack) {
		svid_list = &pd_msg->payload[1];
		count = (PD_HEADER_CNT(pd_msg->msg_hdr)-1);

		if (dpm_dfp_consume_svids(pd_port, svid_list, count))
			return;
	}

	svdm_dfp_inform_svids(pd_port, ack);
	vdm_put_dpm_notified_event(pd_port);
}

void pd_dpm_dfp_inform_modes(
	pd_port_t *pd_port, pd_event_t *pd_event, bool ack)
{
	uint8_t count;
	uint16_t svid = 0;
	uint16_t expected_svid = pd_port->mode_svid;

	pd_msg_t *pd_msg = pd_event->pd_msg;

	if (ack) {
		count = (PD_HEADER_CNT(pd_msg->msg_hdr));
		svid = PD_VDO_VID(pd_msg->payload[VDO_INDEX_HDR]);

		if (svid != expected_svid) {
			ack = false;
			DPM_DBG("Not expected SVID (0x%04x, 0x%04x)\r\n",
				svid, expected_svid);
		} else {
			dpm_dfp_update_svid_data_modes(
				pd_port, svid, &pd_msg->payload[1], count-1);
		}
	}

	svdm_dfp_inform_modes(pd_port, expected_svid, ack);
	vdm_put_dpm_notified_event(pd_port);
}

void pd_dpm_dfp_inform_enter_mode(pd_port_t *pd_port,
				pd_event_t *pd_event, bool ack)
{
	uint16_t svid = 0;
	uint16_t expected_svid = pd_port->mode_svid;
	uint8_t ops = 0;

	if (ack) {
		dpm_vdm_get_svid_ops(pd_event, &svid, &ops);

		/* TODO: check ops later ?! */
		if (svid != expected_svid) {
			ack = false;
			DPM_DBG("Not expected SVID (0x%04x, 0x%04x)\r\n",
				svid, expected_svid);
		} else {
			dpm_dfp_update_svid_enter_mode(pd_port, svid, ops);
		}
	}

	svdm_dfp_inform_enter_mode(pd_port, expected_svid, ops, ack);
	vdm_put_dpm_notified_event(pd_port);
}

void pd_dpm_dfp_inform_exit_mode(pd_port_t *pd_port, pd_event_t *pd_event)
{
	uint16_t svid = 0;
	uint16_t expected_svid = pd_port->mode_svid;
	uint8_t ops;

	if (pd_event->event_type != PD_EVT_TIMER_MSG) {
		dpm_vdm_get_svid_ops(pd_event, &svid, &ops);
	} else {
		svid = pd_port->mode_svid;
		ops = pd_port->mode_obj_pos;
	}

	dpm_dfp_update_svid_data_exit_mode(pd_port, expected_svid, ops);

	svdm_dfp_inform_exit_mode(pd_port, expected_svid, ops);
	vdm_put_dpm_notified_event(pd_port);
}

void pd_dpm_dfp_inform_attention(pd_port_t *pd_port, pd_event_t *pd_event)
{
	uint16_t svid = 0;
	uint8_t ops;

	dpm_vdm_get_svid_ops(pd_event, &svid, &ops);
	DPM_DBG("Attention (svid0x%04x, mode:%d)\r\n", svid, ops);

	svdm_dfp_inform_attention(pd_port, svid, pd_event);
	vdm_put_dpm_notified_event(pd_port);
}

void pd_dpm_dfp_inform_cable_vdo(pd_port_t *pd_port, pd_event_t *pd_event)
{
	const int size = sizeof(uint32_t) * VDO_MAX_SIZE;

	if (pd_event->pd_msg)
		memcpy(pd_port->cable_vdos, pd_event->pd_msg->payload, size);
	else
		memset(pd_port->cable_vdos, 0, size);

	vdm_put_dpm_notified_event(pd_port);
}

/*
 * DRP : Inform Source/Sink Cap
 */

void pd_dpm_dr_inform_sink_cap(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_msg_t *pd_msg = pd_event->pd_msg;
	pd_port_power_caps *snk_cap = &pd_port->remote_snk_cap;

	if (pd_event_msg_match(pd_event, PD_EVT_DATA_MSG, PD_DATA_SINK_CAP)) {
		BUG_ON(pd_msg == NULL);
		snk_cap->nr = PD_HEADER_CNT(pd_msg->msg_hdr);
		memcpy(snk_cap->pdos, pd_msg->payload,
					sizeof(uint32_t) * snk_cap->nr);

		pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_SINK_CAP;
	} else {
		if (pd_event_msg_match(pd_event,
				PD_EVT_CTRL_MSG, PD_CTRL_REJECT))
			pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_SINK_CAP;

		snk_cap->nr = 0;
		snk_cap->pdos[0] = 0;
	}

	pd_dpm_update_pdos_flags(pd_port, snk_cap->pdos[0]);
}

void pd_dpm_dr_inform_source_cap(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_msg_t *pd_msg = pd_event->pd_msg;
	pd_port_power_caps *src_cap = &pd_port->remote_src_cap;

	if (pd_event_msg_match(pd_event, PD_EVT_DATA_MSG, PD_DATA_SOURCE_CAP)) {
		BUG_ON(pd_msg == NULL);
		src_cap->nr = PD_HEADER_CNT(pd_msg->msg_hdr);
		memcpy(src_cap->pdos, pd_msg->payload,
				sizeof(uint32_t) * src_cap->nr);

		pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_SOURCE_CAP;
	} else {
		if (pd_event_msg_match(pd_event,
					PD_EVT_CTRL_MSG, PD_CTRL_REJECT))
			pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_SOURCE_CAP;

		src_cap->nr = 0;
		src_cap->pdos[0] = 0;
	}

	pd_dpm_update_pdos_flags(pd_port, src_cap->pdos[0]);
}

/*
 * DRP : Data Role Swap
 */

void pd_dpm_drs_evaluate_swap(pd_port_t *pd_port, uint8_t role)
{
	/* TODO : Check it later */
	pd_put_dpm_ack_event(pd_port);
}

void pd_dpm_drs_change_role(pd_port_t *pd_port, uint8_t role)
{
	pd_set_data_role(pd_port, role);

	/* pd_put_dpm_ack_event(pd_port); */
	pd_port->dpm_ack_immediately = true;
}

/*
 * DRP : Power Role Swap
 */

 /*
static bool pd_dpm_evaluate_source_cap_match(pd_port_t* pd_port)
{
	int i, j;
	bool find_cap = false;
	struct dpm_pdo_info_t sink, source;
	pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	pd_port_power_caps *src_cap = &pd_port->remote_src_cap;

	if ((src_cap->nr <= 0) || (snk_cap->nr <= 0))
		return false;

	for (j = 0; (j < snk_cap->nr) && (!find_cap); j++) {
		dpm_extract_pdo_info(snk_cap->pdos[j], &sink);

		for (i = 0; (i < src_cap->nr) && (!find_cap); i++) {
			dpm_extract_pdo_info(src_cap->pdos[i], &source);

			find_cap = dpm_is_valid_pdo_pair(
				&sink, &source, pd_port->dpm_caps);
		}
	}

	return find_cap;
} */

/*
Rules:
	External Sources -> EXS
	Provider/Consumers -> PC
	Consumers/Provider -> CP

	1.  PC (with EXS) shall always deny PR_SWAP from CP (without EXS)

	2.  PC (without EXS) shall always acppet PR_SWAP from CP (with EXS)
		unless the requester isn't able to provide PDOs.
*/

void pd_dpm_prs_evaluate_swap(pd_port_t *pd_port, uint8_t role)
{
	int good_power;
	bool accept = true;
	bool sink, check_src, check_snk;

	if (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GOOD_POWER) {
		sink = pd_port->power_role == PD_ROLE_SINK;
		check_src = (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SRC)
			? 1 : 0;
		check_snk = (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SNK)
			? 1 : 0;
		good_power = dpm_check_good_power(pd_port);

		switch (good_power) {
		case GOOD_PW_PARTNER:
			if (sink && check_snk)
				accept = false;
			break;

		case GOOD_PW_LOCAL:
			if ((!sink) && check_src)
				accept = false;
			break;

		case GOOD_PW_NONE:
			accept = true;
			break;

		default:
			accept = true;
			break;
#if 0
			if ((!sink) && check_src)
				accept = pd_dpm_evaluate_source_cap_match(
								pd_port);
#endif
		}
	}

	dpm_response_request(pd_port, accept);
}

void pd_dpm_prs_turn_off_power_sink(pd_port_t *pd_port)
{
	tcpci_sink_vbus(pd_port->tcpc_dev, 0, 0);
}

void pd_dpm_prs_enable_power_source(pd_port_t *pd_port, bool en)
{
	int vbus_level = en ? TCPC_VBUS_SOURCE_5V : TCPC_VBUS_SOURCE_0V;

	tcpci_source_vbus(pd_port->tcpc_dev, vbus_level, 0);

	if (en)
		pd_enable_vbus_valid_detection(pd_port, en);
	else
		pd_enable_vbus_safe0v_detection(pd_port);
}

void pd_dpm_prs_change_role(pd_port_t *pd_port, uint8_t role)
{
	pd_set_power_role(pd_port, role);
	pd_put_dpm_ack_event(pd_port);
}

/*
 * DRP : Vconn Swap
 */

void pd_dpm_vcs_evaluate_swap(pd_port_t *pd_port)
{
	bool accept = true;

	dpm_response_request(pd_port, accept);
}

void pd_dpm_vcs_enable_vconn(pd_port_t *pd_port, bool en)
{
	pd_dpm_enable_vconn(pd_port, en);

	/* TODO: If we can't enable vconn immediately,
		then after vconn_on,
		Vconn Controller should pd_put_dpm_ack_event() */

	pd_port->dpm_ack_immediately = true;
}

/*
 * PE : Notify DPM
 */

static inline int pd_dpm_ready_get_sink_cap(pd_port_t *pd_port)
{
	if (!(pd_port->dpm_flags & DPM_FLAGS_CHECK_SINK_CAP))
		return 0;

	if (pd_port->get_snk_cap_count >= PD_GET_SNK_CAP_RETRIES)
		return 0;

	pd_port->get_snk_cap_count++;
	pd_put_dpm_pd_request_event(
		pd_port, PD_DPM_PD_REQUEST_GET_SINK_CAP);

	return 1;
}

static inline int pd_dpm_ready_get_source_cap(pd_port_t *pd_port)
{
	if (!(pd_port->dpm_flags & DPM_FLAGS_CHECK_SOURCE_CAP))
		return 0;

	if (pd_port->get_src_cap_count >= PD_GET_SRC_CAP_RETRIES)
		return 0;

	pd_port->get_src_cap_count++;
	pd_put_dpm_pd_request_event(
		pd_port, PD_DPM_PD_REQUEST_GET_SOURCE_CAP);

	return 1;
}

static inline int pd_dpm_notify_pe_src_ready(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
	if (pd_dpm_ready_get_source_cap(pd_port))
		return 1;

	return pd_dpm_ready_get_sink_cap(pd_port);
}

static inline int pd_dpm_notify_pe_snk_ready(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
#if 0
	return pd_dpm_ready_get_sink_cap(pd_port);
#else
	return 0;
#endif
}

static inline int pd_dpm_notify_pe_dfp_ready(
	pd_port_t *pd_port, pd_event_t *pd_event)
{
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	if (svdm_notify_pe_ready(pd_port, pd_event))
		return 1;
#else

#ifdef CONFIG_USB_PD_ATTEMP_DISCOVER_ID
	if (pd_port->dpm_flags & DPM_FLAGS_CHECK_UFP_ID) {
		pd_port->dpm_flags &= ~DPM_FLAGS_CHECK_UFP_ID;
		if (vdm_put_dpm_vdm_request_event(
			pd_port, PD_DPM_VDM_REQUEST_DISCOVER_ID))
			return 1;
	}
#endif

#endif

#ifdef CONFIG_USB_PD_DFP_READY_DISCOVER_ID
	if (pd_port->dpm_flags & DPM_FLAGS_CHECK_CABLE_ID_DFP) {
		if (pd_is_auto_discover_cable_id(pd_port)) {
			pd_disable_timer(pd_port, PD_TIMER_DISCOVER_ID);
			pd_enable_timer(pd_port, PD_TIMER_DISCOVER_ID);
			return 0;
		}
	}
#endif

	return 0;
}

int pd_dpm_notify_pe_startup(pd_port_t *pd_port)
{
	uint32_t caps, flags = 0;

	caps = DPM_CAP_EXTRACT_PR_CHECK(pd_port->dpm_caps);
	if (caps != DPM_CAP_PR_CHECK_DISABLE)
		flags |= DPM_FLAGS_CHECK_PR_ROLE;

	caps = DPM_CAP_EXTRACT_DR_CHECK(pd_port->dpm_caps);
	if (caps != DPM_CAP_DR_CHECK_DISABLE)
		flags |= DPM_FLAGS_CHECK_DR_ROLE;

	if (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SRC)
		flags |= DPM_FLAGS_CHECK_SINK_CAP;

	if (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SNK)
		flags |= DPM_FLAGS_CHECK_SOURCE_CAP;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_CABLE)
		flags |= DPM_FLAGS_CHECK_CABLE_ID;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_CABLE_DFP)
		flags |= DPM_FLAGS_CHECK_CABLE_ID_DFP;
	/* If try to enter DP, then skip normal discover_id flow */

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_ENTER_DP_MODE)
		flags |= DPM_FLAGS_CHECK_DP_MODE;
	else if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_ID)
		flags |= DPM_FLAGS_CHECK_UFP_ID;

	pd_port->dpm_flags = flags;

	svdm_notify_pe_startup(pd_port);
	return 0;
}

int pd_dpm_notify_pe_ready(pd_port_t *pd_port, pd_event_t *pd_event)
{
	int ret = 0;

	if (pd_port->power_role == PD_ROLE_SOURCE)
		ret = pd_dpm_notify_pe_src_ready(pd_port, pd_event);
	else
		ret = pd_dpm_notify_pe_snk_ready(pd_port, pd_event);

	if (ret != 0)
		return ret;

	if (pd_port->data_role == PD_ROLE_DFP)
		ret = pd_dpm_notify_pe_dfp_ready(pd_port, pd_event);

	if (ret != 0)
		return ret;

	if (!pd_port->pe_ready) {
		pd_port->pe_ready = true;
		pd_update_connect_state(pd_port, PD_CONNECT_PE_READY);
	}

	return 0;
}

/*
 * dpm_core_init
 */

int pd_dpm_core_init(pd_port_t *pd_port)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(svdm_svid_ops); i++)
		dpm_register_svdm_ops(pd_port, &svdm_svid_ops[i]);
	return 0;
}
