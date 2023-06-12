/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * PD Device Policy Manager Core Driver
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

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "inc/pd_dpm_pdo_select.h"
#include "inc/pd_core.h"
#include "pd_dpm_prv.h"

struct pd_device_policy_manager {
	uint8_t temp;
};

static const struct svdm_svid_ops svdm_svid_ops[] = {
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
		.parse_svid_data = dp_parse_svid_data,
	},
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_RICHTEK_UVDM
	{
		.name = "Richtek",
		.svid = USB_VID_RICHTEK,

		.dfp_notify_uvdm = richtek_dfp_notify_uvdm,
		.ufp_notify_uvdm = richtek_ufp_notify_uvdm,

		.notify_pe_startup = richtek_dfp_notify_pe_startup,
		.notify_pe_ready = richtek_dfp_notify_pe_ready,
	},
#endif	/* CONFIG_USB_PD_RICHTEK_UVDM */

#ifdef CONFIG_USB_PD_ALT_MODE_RTDC
	{
		.name = "Direct Charge",
		.svid = USB_VID_DIRECTCHARGE,

		.dfp_inform_id = dc_dfp_notify_discover_id,
		.dfp_inform_svids = dc_dfp_notify_discover_svid,
		.dfp_inform_modes = dc_dfp_notify_discover_modes,

		.dfp_inform_enter_mode = dc_dfp_notify_enter_mode,
		.dfp_inform_exit_mode = dc_dfp_notify_exit_mode,

		.notify_pe_startup = dc_dfp_notify_pe_startup,
		.notify_pe_ready = dc_dfp_notify_pe_ready,

		.dfp_notify_uvdm = dc_dfp_notify_uvdm,
		.ufp_notify_uvdm = dc_ufp_notify_uvdm,

		.parse_svid_data = dc_parse_svid_data,
		.reset_state = dc_reset_state,
	},
#endif	/* CONFIG_USB_PD_ALT_MODE_RTDC */
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

/*
 * DPM Init
 */

static void pd_dpm_update_pdos_flags(struct pd_port *pd_port, uint32_t pdo)
{
	uint16_t dpm_flags = pd_port->pe_data.dpm_flags
		& ~DPM_FLAGS_RESET_PARTNER_MASK;

	/* Only update PDO flags if pdo's type is fixed */
	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_FIXED) {
		if (pdo & PDO_FIXED_DUAL_ROLE)
			dpm_flags |= DPM_FLAGS_PARTNER_DR_POWER;

		if (pdo & PDO_FIXED_DATA_SWAP)
			dpm_flags |= DPM_FLAGS_PARTNER_DR_DATA;

		if (pdo & PDO_FIXED_EXTERNAL)
			dpm_flags |= DPM_FLAGS_PARTNER_EXTPOWER;

		if (pdo & PDO_FIXED_COMM_CAP)
			dpm_flags |= DPM_FLAGS_PARTNER_USB_COMM;

		if (pdo & PDO_FIXED_SUSPEND)
			dpm_flags |= DPM_FLAGS_PARTNER_USB_SUSPEND;
	}

	pd_port->pe_data.dpm_flags = dpm_flags;
}


int pd_dpm_send_sink_caps(struct pd_port *pd_port)
{
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_check_rev30(pd_port))
		snk_cap->nr = pd_port->local_snk_cap_nr_pd30;
	else
		snk_cap->nr = pd_port->local_snk_cap_nr_pd20;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	return pd_send_sop_data_msg(pd_port, PD_DATA_SINK_CAP,
		snk_cap->nr, snk_cap->pdos);
}

int pd_dpm_send_source_caps(struct pd_port *pd_port)
{
	uint8_t i;
	uint32_t cable_curr = 3000;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	struct pd_port_power_caps *src_cap0 = &pd_port->local_src_cap_default;
	struct pd_port_power_caps *src_cap1 = &pd_port->local_src_cap;

	if (pd_port->pe_data.power_cable_present) {
		cable_curr = pd_get_cable_current_limit(pd_port);
		DPM_DBG("cable_limit: %dmA\n", cable_curr);
	}

	src_cap1->nr = src_cap0->nr;
	for (i = 0; i < src_cap0->nr; i++) {
		src_cap1->pdos[i] =
			pd_reset_pdo_power(tcpc, src_cap0->pdos[i], cable_curr);
	}

	return pd_send_sop_data_msg(pd_port, PD_DATA_SOURCE_CAP,
		src_cap1->nr, src_cap1->pdos);
}

void pd_dpm_inform_cable_id(struct pd_port *pd_port, bool src_startup)
{
#ifdef CONFIG_USB_PD_REV30
	struct pe_data *pe_data = &pd_port->pe_data;
#endif /* CONFIG_USB_PD_REV30 */
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (payload) {
		memcpy(pd_port->pe_data.cable_vdos, payload,
			pd_get_msg_data_size(pd_port));

		DPM_DBG("InformCable, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
				payload[0], payload[1], payload[2], payload[3]);

		dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_CABLE);
	} else {
#ifdef CONFIG_USB_PD_REV30
		if (pe_data->discover_id_counter >= PD_DISCOVER_ID30_COUNT)
			pd_sync_sop_prime_spec_revision(pd_port, PD_REV20);
#endif	/* CONFIG_USB_PD_REV30 */
	}

	if (src_startup)
		pd_enable_timer(pd_port, PD_TIMER_SOURCE_START);
	else
		VDM_STATE_DPM_INFORMED(pd_port);
}

static bool dpm_response_request(struct pd_port *pd_port, bool accept)
{
	if (accept)
		return pd_put_dpm_ack_event(pd_port);

	return pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_REJECT);
}

/* ---- SNK ---- */

static void dpm_build_sink_pdo_info(struct dpm_pdo_info_t *sink_pdo_info,
		uint8_t type, int request_v, int request_i)
{
	sink_pdo_info->type = type;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (type == DPM_PDO_TYPE_APDO) {
		request_v = (request_v / 20) * 20;
		request_i = (request_i / 50) * 50;
	} else
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
		request_i = (request_i / 10) * 10;

	sink_pdo_info->vmin = sink_pdo_info->vmax = request_v;
	sink_pdo_info->ma = request_i;
	sink_pdo_info->uw = request_v * request_i;
}

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
static int pps_request_thread_fn(void *data)
{
	struct tcpc_device *tcpc = data;
	struct pd_port *pd_port = &tcpc->pd_port;
	int ret = 0;
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	while (true) {
		ret = wait_event_interruptible(pd_port->pps_request_wait_que,
				atomic_read(&pd_port->pps_request) ||
				kthread_should_stop());
		if (kthread_should_stop() || ret) {
			dev_notice(&tcpc->dev, "%s exits(%d)\n", __func__, ret);
			break;
		}
		while (!wait_event_timeout(pd_port->pps_request_wait_que,
					!atomic_read(&pd_port->pps_request) ||
					kthread_should_stop(),
					msecs_to_jiffies(7*1000))) {
			pd_put_deferred_tcp_event(tcpc, &tcp_event);
		}
	}

	return 0;
}

void pd_dpm_start_pps_request_thread(struct pd_port *pd_port, bool en)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO("pps_thread (%s)\n", en ? "start" : "end");
	if (en) {
		__pm_stay_awake(pd_port->pps_request_wake_lock);
		atomic_set(&pd_port->pps_request, true);
		wake_up(&pd_port->pps_request_wait_que);
	} else {
		atomic_set(&pd_port->pps_request, false);
		wake_up(&pd_port->pps_request_wait_que);
		__pm_relax(pd_port->pps_request_wake_lock);
	}
}

static bool dpm_build_request_info_apdo(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info,
		struct pd_port_power_caps *src_cap, uint8_t charging_policy)
{
	struct dpm_pdo_info_t sink_pdo_info;

	dpm_build_sink_pdo_info(&sink_pdo_info, DPM_PDO_TYPE_APDO,
			pd_port->request_v_apdo, pd_port->request_i_apdo);

	return dpm_find_match_req_info(req_info,
			&sink_pdo_info, src_cap->nr, src_cap->pdos,
			-1, charging_policy);
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

static bool dpm_build_request_info_pdo(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info,
		struct pd_port_power_caps *src_cap, uint8_t charging_policy)
{
	bool find_cap = false;
	int i, max_uw = -1;
	struct dpm_pdo_info_t sink_pdo_info;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	for (i = 0; i < snk_cap->nr; i++) {
		DPM_DBG("EvaSinkCap%d\n", i+1);
		dpm_extract_pdo_info(snk_cap->pdos[i], &sink_pdo_info);

		find_cap = dpm_find_match_req_info(req_info,
				&sink_pdo_info, src_cap->nr, src_cap->pdos,
				max_uw, charging_policy);

		if (find_cap) {
			if (req_info->type == DPM_PDO_TYPE_BAT)
				max_uw = req_info->oper_uw;
			else
				max_uw = req_info->vmax * req_info->oper_ma;

			DPM_DBG("Find SrcCap%d(%s):%d mw\n",
					req_info->pos, req_info->mismatch ?
					"Mismatch" : "Match", max_uw/1000);
			pd_port->pe_data.local_selected_cap = i + 1;
		}
	}

	return max_uw > 0;
}

static bool dpm_build_request_info(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	int i;
	uint8_t charging_policy = pd_port->dpm_charging_policy;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	memset(req_info, 0, sizeof(struct dpm_rdo_info_t));

	DPM_INFO("Policy=0x%X\n", charging_policy);

	for (i = 0; i < src_cap->nr; i++)
		DPM_DBG("SrcCap%d: 0x%08x\n", i+1, src_cap->pdos[i]);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if ((charging_policy & DPM_CHARGING_POLICY_MASK)
		== DPM_CHARGING_POLICY_PPS) {
		return dpm_build_request_info_apdo(
				pd_port, req_info, src_cap, charging_policy);
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	return dpm_build_request_info_pdo(
			pd_port, req_info, src_cap, charging_policy);
}

static bool dpm_build_default_request_info(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	struct dpm_pdo_info_t sink, source;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;

	pd_port->pe_data.local_selected_cap = 1;

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

static inline void dpm_update_request_i_new(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	if (req_info->mismatch)
		pd_port->request_i_new = pd_port->request_i_op;
	else
		pd_port->request_i_new = pd_port->request_i_max;
}

static inline void dpm_update_request_bat(struct pd_port *pd_port,
	struct dpm_rdo_info_t *req_info, uint32_t flags)
{
	uint32_t mw_op, mw_max;

	mw_op = req_info->oper_uw / 1000;
	mw_max = req_info->max_uw / 1000;

	pd_port->request_i_op = req_info->oper_uw / req_info->vmin;
	pd_port->request_i_max = req_info->max_uw / req_info->vmin;

	dpm_update_request_i_new(pd_port, req_info);

	pd_port->last_rdo = RDO_BATT(
			req_info->pos, mw_op, mw_max, flags);
}

static inline void dpm_update_request_not_bat(struct pd_port *pd_port,
	struct dpm_rdo_info_t *req_info, uint32_t flags)
{
	pd_port->request_i_op = req_info->oper_ma;
	pd_port->request_i_max = req_info->max_ma;

	dpm_update_request_i_new(pd_port, req_info);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (req_info->type == DPM_PDO_TYPE_APDO) {
		pd_port->request_apdo_new = true;
		pd_port->last_rdo = RDO_APDO(
				req_info->pos, req_info->vmin,
				req_info->oper_ma, flags);
		return;
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (req_info->mismatch && (pd_port->cap_miss_match == 0x3)) {
		pd_port->cap_miss_match = 0;
		req_info->mismatch = 0;
		flags &= ~RDO_CAP_MISMATCH;
		pd_port->last_rdo = RDO_FIXED(
			req_info->pos, req_info->oper_ma,
			req_info->oper_ma, flags);
	} else {
		pd_port->last_rdo = RDO_FIXED(
			req_info->pos, req_info->oper_ma,
			req_info->max_ma, flags);
	}
}

static inline void dpm_update_request(
	struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	uint32_t flags = 0;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	pd_port->request_apdo_new = false;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_GIVE_BACK)
		flags |= RDO_GIVE_BACK;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_NO_SUSPEND)
		flags |= RDO_NO_SUSPEND;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_USB_COMM)
		flags |= RDO_COMM_CAP;

	if (req_info->mismatch) {
		flags |= RDO_CAP_MISMATCH;
		pd_port->cap_miss_match |= 0x1;
		DPM_INFO("cap miss match case\n");
	}

	pd_port->request_v_new = req_info->vmax;

	if (req_info->type == DPM_PDO_TYPE_BAT)
		dpm_update_request_bat(pd_port, req_info, flags);
	else
		dpm_update_request_not_bat(pd_port, req_info, flags);

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	pd_notify_pe_direct_charge(pd_port,
			req_info->vmin < TCPC_VBUS_SINK_5V);
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
}

int pd_dpm_update_tcp_request(struct pd_port *pd_port,
		struct tcp_dpm_pd_request *pd_req)
{
	bool find_cap = false;
	uint8_t type = DPM_PDO_TYPE_FIXED;
	struct dpm_rdo_info_t req_info;
	struct dpm_pdo_info_t sink_pdo_info;
	uint8_t charging_policy = pd_port->dpm_charging_policy;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	memset(&req_info, 0, sizeof(struct dpm_rdo_info_t));

	DPM_DBG("charging_policy=0x%X\n", charging_policy);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if ((charging_policy & DPM_CHARGING_POLICY_MASK)
		== DPM_CHARGING_POLICY_PPS)
		type = DPM_PDO_TYPE_APDO;
#endif	/*CONFIG_USB_PD_REV30_PPS_SINK */

	dpm_build_sink_pdo_info(&sink_pdo_info, type, pd_req->mv, pd_req->ma);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->request_apdo &&
		(sink_pdo_info.vmin == pd_port->request_v) &&
		(sink_pdo_info.ma == pd_port->request_i))
		return TCP_DPM_RET_DENIED_REPEAT_REQUEST;
#endif	/*CONFIG_USB_PD_REV30_PPS_SINK */

	find_cap = dpm_find_match_req_info(&req_info,
			&sink_pdo_info, src_cap->nr, src_cap->pdos,
			-1, charging_policy);

	if (!find_cap) {
		DPM_INFO("Can't find match_cap\n");
		return TCP_DPM_RET_DENIED_INVALID_REQUEST;
	}

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if ((charging_policy & DPM_CHARGING_POLICY_MASK)
		== DPM_CHARGING_POLICY_PPS) {
		pd_port->request_v_apdo = sink_pdo_info.vmin;
		pd_port->request_i_apdo = sink_pdo_info.ma;
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	dpm_update_request(pd_port, &req_info);
	return TCP_DPM_RET_SUCCESS;
}

int pd_dpm_update_tcp_request_ex(struct pd_port *pd_port,
	struct tcp_dpm_pd_request_ex *pd_req)
{
	struct dpm_pdo_info_t source;
	struct dpm_rdo_info_t req_info;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_req->pos > src_cap->nr)
		return false;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->dpm_charging_policy == DPM_CHARGING_POLICY_PPS) {
		DPM_INFO("Reject tcp_rqeuest_ex if charging_policy=pps\n");
		return TCP_DPM_RET_DENIED_INVALID_REQUEST;
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	dpm_extract_pdo_info(src_cap->pdos[pd_req->pos-1], &source);

	req_info.pos = pd_req->pos;
	req_info.type = source.type;
	req_info.mismatch = false;
	req_info.vmax = source.vmax;
	req_info.vmin = source.vmin;

	if (req_info.type == DPM_PDO_TYPE_BAT) {
		req_info.max_uw = pd_req->max_uw;
		req_info.oper_uw = pd_req->oper_uw;
		if (pd_req->oper_uw < pd_req->max_uw)
			req_info.mismatch = true;
	} else {
		req_info.max_ma = pd_req->max_ma;
		req_info.oper_ma = pd_req->oper_ma;
		if (pd_req->oper_ma < pd_req->max_ma)
			req_info.mismatch = true;
	}

	dpm_update_request(pd_port, &req_info);
	return TCP_DPM_RET_SUCCESS;
}

int pd_dpm_update_tcp_request_again(struct pd_port *pd_port)
{
	bool find_cap = false;
	int sink_nr, source_nr;
	struct dpm_rdo_info_t req_info;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	sink_nr = snk_cap->nr;
	source_nr = src_cap->nr;

	if ((source_nr <= 0) || (sink_nr <= 0)) {
		DPM_INFO("SrcNR or SnkNR = 0\n");
		return TCP_DPM_RET_DENIED_INVALID_REQUEST;
	}

	find_cap = dpm_build_request_info(pd_port, &req_info);

	/* If we can't find any cap to use, choose default setting */
	if (!find_cap) {
		DPM_INFO("Can't find any SrcCap\n");
		dpm_build_default_request_info(pd_port, &req_info);
	} else
		DPM_INFO("Select SrcCap%d\n", req_info.pos);

	dpm_update_request(pd_port, &req_info);
	return TCP_DPM_RET_SUCCESS;
}

void pd_dpm_snk_evaluate_caps(struct pd_port *pd_port)
{
	bool find_cap = false;
	struct dpm_rdo_info_t req_info;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	PD_BUG_ON(pd_get_msg_data_payload(pd_port) == NULL);

	pd_dpm_dr_inform_source_cap(pd_port);

	if ((src_cap->nr <= 0) || (snk_cap->nr <= 0)) {
		DPM_INFO("SrcNR or SnkNR = 0\n");
		return;
	}

	find_cap = dpm_build_request_info(pd_port, &req_info);

	/* If we can't find any cap to use, choose default setting */
	if (!find_cap) {
		DPM_INFO("Can't find any SrcCap\n");
		dpm_build_default_request_info(pd_port, &req_info);
	} else
		DPM_INFO("Select SrcCap%d\n", req_info.pos);

	dpm_update_request(pd_port, &req_info);

	if (req_info.pos > 0)
		pd_put_dpm_notify_event(pd_port, req_info.pos);
}

void pd_dpm_snk_standby_power(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_SNK_STANDBY_POWER
	/*
	 * pSnkStdby :
	 *   Maximum power consumption while in Sink Standby. (2.5W)
	 * I1 = (pSnkStdby/VBUS)
	 * I2 = (pSnkStdby/VBUS) + cSnkBulkPd(DVBUS/Dt)
	 * STANDBY_UP = I1 < I2, STANDBY_DOWN = I1 > I2
	 *
	 * tSnkNewPower (t1):
	 *   Maximum transition time between power levels. (15ms)
	 */

	uint8_t type;
	int ma = -1;
	int standby_curr = 2500000 / max(pd_port->request_v,
					 pd_port->request_v_new);

#ifdef CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	struct tcpc_device *tcpc = pd_port->tcpc;
	struct pe_data *pe_data = &pd_port->pe_data;
	bool vconn_highv_prot = pd_port->request_v_new > 5000;

	if (!pe_data->vconn_highv_prot && vconn_highv_prot &&
		tcpc->tcpc_flags & TCPC_FLAGS_VCONN_SAFE5V_ONLY) {
		PE_INFO("VC_HIGHV_PROT: %d\n", vconn_highv_prot);
		pe_data->vconn_highv_prot_role = pd_port->vconn_role;
		pd_set_vconn(pd_port, PD_ROLE_VCONN_OFF);
		pe_data->vconn_highv_prot = vconn_highv_prot;
	}
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	/*
	 * A Sink is not required to transition to Sink Standby
	 *	when operating with a Programmable Power Supply
	 *	(Check it later, Aginst new spec)
	 */
	if (pd_port->request_apdo_new)
		return;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (pd_port->request_v_new > pd_port->request_v) {
		/* Case2 Increasing the Voltage */
		/* Case3 Increasing the Voltage and Current */
		/* Case4 Increasing the Voltage and Decreasing the Curren */
		ma = standby_curr;
		type = TCP_VBUS_CTRL_STANDBY_UP;
	} else if (pd_port->request_v_new < pd_port->request_v) {
		/* Case5 Decreasing the Voltage and Increasing the Current */
		/* Case7 Decreasing the Voltage */
		/* Case8 Decreasing the Voltage and the Current*/
		ma = standby_curr;
		type = TCP_VBUS_CTRL_STANDBY_DOWN;
	} else if (pd_port->request_i_new < pd_port->request_i) {
		/* Case6 Decreasing the Current, t1 i = new */
		ma = pd_port->request_i_new;
		type = TCP_VBUS_CTRL_STANDBY;
	}

	if (ma >= 0) {
		tcpci_sink_vbus(
			pd_port->tcpc, type, pd_port->request_v_new, ma);
	}
#else
#ifdef CONFIG_USB_PD_SNK_GOTOMIN
	tcpci_sink_vbus(pd_port->tcpc, TCP_VBUS_CTRL_REQUEST,
		pd_port->request_v, pd_port->request_i_new);
#endif	/* CONFIG_USB_PD_SNK_GOTOMIN */
#endif	/* CONFIG_USB_PD_SNK_STANDBY_POWER */
}

void pd_dpm_snk_transition_power(struct pd_port *pd_port)
{
	tcpci_sink_vbus(pd_port->tcpc, TCP_VBUS_CTRL_REQUEST,
		pd_port->request_v_new, pd_port->request_i_new);

	pd_port->request_v = pd_port->request_v_new;
	pd_port->request_i = pd_port->request_i_new;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->request_apdo != pd_port->request_apdo_new) {
		pd_port->request_apdo = pd_port->request_apdo_new;
		pd_dpm_start_pps_request_thread(
			pd_port, pd_port->request_apdo_new);
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
}

void pd_dpm_snk_hard_reset(struct pd_port *pd_port)
{
	/*
	 * tSnkHardResetPrepare :
	 * Time allotted for the Sink power electronics
	 * to prepare for a Hard Reset
	 */

	int mv = 0, ma = 0;
	bool ignore_hreset = false;

#ifdef CONFIG_USB_PD_SNK_HRESET_KEEP_DRAW
	if (!pd_port->pe_data.pd_prev_connected) {
#ifdef CONFIG_USB_PD_SNK_IGNORE_HRESET_IF_TYPEC_ONLY
		ignore_hreset = true;
#else
		ma = -1;
		mv = TCPC_VBUS_SINK_5V;
#endif	/* CONFIG_USB_PD_SNK_IGNORE_HRESET_IF_TYPEC_ONLY */
	}
#endif	/* CONFIG_USB_PD_SNK_HRESET_KEEP_DRAW */

	if (!ignore_hreset) {
		tcpci_sink_vbus(
			pd_port->tcpc, TCP_VBUS_CTRL_HRESET, mv, ma);
	}

	pd_put_pe_event(pd_port, PD_PE_POWER_ROLE_AT_DEFAULT);
}

/* ---- SRC ---- */

static inline bool dpm_evaluate_request(
	struct pd_port *pd_port, uint32_t rdo, uint8_t rdo_pos)
{
	uint32_t pdo;
	uint32_t sink_v;
	uint32_t op_curr, max_curr;
	struct dpm_pdo_info_t src_info;
	struct pd_port_power_caps *src_cap = &pd_port->local_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	pd_port->pe_data.dpm_flags &= (~DPM_FLAGS_PARTNER_MISMATCH);

	if ((rdo_pos == 0) || (rdo_pos > src_cap->nr)) {
		DPM_INFO("RequestPos Wrong (%d)\n", rdo_pos);
		return false;
	}

	pdo = src_cap->pdos[rdo_pos-1];

	dpm_extract_pdo_info(pdo, &src_info);
	pd_extract_rdo_power(rdo, pdo, &op_curr, &max_curr);

	if (src_info.ma < op_curr) {
		DPM_INFO("src_i (%d) < op_i (%d)\n", src_info.ma, op_curr);
		return false;
	}

	if (rdo & RDO_CAP_MISMATCH) {
		/* TODO: handle it later */
		DPM_INFO("CAP_MISMATCH\n");
		pd_port->pe_data.dpm_flags |= DPM_FLAGS_PARTNER_MISMATCH;
	} else if (src_info.ma < max_curr) {
		DPM_INFO("src_i (%d) < max_i (%d)\n", src_info.ma, max_curr);
		return false;
	}

	sink_v = src_info.vmin;

#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_APDO) {
		sink_v = RDO_APDO_EXTRACT_OP_MV(rdo);

		if ((sink_v < src_info.vmin) || (sink_v > src_info.vmax)) {
			DPM_INFO("sink_v (%d) not in src_v (%d~%d)\n",
				sink_v, src_info.vmin, src_info.vmax);
			return false;
		}
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */

	/* Accept request */

	pd_port->request_i_op = op_curr;
	pd_port->request_i_max = max_curr;

	if (rdo & RDO_CAP_MISMATCH)
		pd_port->request_i_new = op_curr;
	else
		pd_port->request_i_new = max_curr;

	pd_port->request_v_new = sink_v;
	return true;
}

void pd_dpm_src_evaluate_request(struct pd_port *pd_port)
{
	uint32_t rdo;
	uint8_t rdo_pos;
	struct pe_data *pe_data;
	uint32_t *payload = pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	PD_BUG_ON(payload == NULL);

	rdo = payload[0];
	rdo_pos = RDO_POS(rdo);

	DPM_INFO("RequestCap%d\n", rdo_pos);

	pe_data = &pd_port->pe_data;

	if (dpm_evaluate_request(pd_port, rdo, rdo_pos))  {
		pe_data->local_selected_cap = rdo_pos;
		pd_put_dpm_notify_event(pd_port, rdo_pos);
	} else {
		/*
		 * "Contract Invalid" means that the previously
		 * negotiated Voltage and Current values
		 * are no longer included in the Sources new Capabilities.
		 * If the Sink fails to make a valid Request in this case
		 * then Power Delivery operation is no longer possible
		 * and Power Delivery mode is exited with a Hard Reset.
		 */

		pe_data->invalid_contract = false;
		pe_data->local_selected_cap = 0;
		pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_REJECT);
	}
}

void pd_dpm_src_transition_power(struct pd_port *pd_port)
{
	pd_enable_vbus_stable_detection(pd_port);

#ifdef CONFIG_USB_PD_SRC_HIGHCAP_POWER
	if (pd_port->request_v > pd_port->request_v_new) {
		mutex_lock(&pd_port->tcpc->access_lock);
		tcpci_enable_force_discharge(
			pd_port->tcpc, true, pd_port->request_v_new);
		mutex_unlock(&pd_port->tcpc->access_lock);
	}
#endif	/* CONFIG_USB_PD_SRC_HIGHCAP_POWER */

	tcpci_source_vbus(pd_port->tcpc, TCP_VBUS_CTRL_REQUEST,
		pd_port->request_v_new, pd_port->request_i_new);

	if (pd_port->request_v == pd_port->request_v_new)
		pd_put_vbus_stable_event(pd_port->tcpc);
#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	else
		pd_enable_timer(pd_port, PD_TIMER_VBUS_STABLE);
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */

	pd_port->request_v = pd_port->request_v_new;
	pd_port->request_i = pd_port->request_i_new;
}

void pd_dpm_src_hard_reset(struct pd_port *pd_port)
{
	tcpci_source_vbus(pd_port->tcpc,
		TCP_VBUS_CTRL_HRESET, TCPC_VBUS_SOURCE_0V, 0);
	pd_enable_vbus_safe0v_detection(pd_port);
}

/* ---- UFP : update_svid_data ---- */

static inline bool dpm_ufp_update_svid_data_enter_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("EnterMode (svid0x%04x, ops:%d)\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	if (svid_data == NULL)
		return false;

	/* Only accept 1 mode active at the same time */
	if (svid_data->active_mode)
		return false;

	if ((ops == 0) || (ops > svid_data->local_mode.mode_cnt))
		return false;

	svid_data->active_mode = ops;
	pd_port->pe_data.modal_operation = true;

	svdm_ufp_request_enter_mode(pd_port, svid, ops);

	tcpci_enter_mode(tcpc, svid, ops, svid_data->local_mode.mode_vdo[ops]);
	return true;
}

static inline bool dpm_ufp_update_svid_data_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	uint8_t i;
	bool modal_operation;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("ExitMode (svid0x%04x, mode:%d)\n", svid, ops);

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

		pd_port->pe_data.modal_operation = modal_operation;

		svdm_ufp_request_exit_mode(pd_port, svid, ops);
		tcpci_exit_mode(pd_port->tcpc, svid);
		return true;
	}

	return false;
}


/* ---- UFP : Response VDM Request ---- */

static int dpm_vdm_ufp_response_id(struct pd_port *pd_port)
{
	if (pd_check_rev30(pd_port))
		pd_port->id_vdos[0] = pd_port->id_header;
	else
		pd_port->id_vdos[0] = VDO_IDH_PD20(pd_port->id_header);

	return pd_reply_svdm_request(pd_port, CMDT_RSP_ACK,
		pd_check_rev30(pd_port) ? pd_port->id_vdo_nr : 3,
		pd_port->id_vdos);
}

static int dpm_ufp_response_svids(struct pd_port *pd_port)
{
	struct svdm_svid_data *svid_data;
	uint16_t svid_list[2];
	uint32_t svids[VDO_MAX_NR];
	uint8_t i = 0, j = 0, cnt = pd_port->svid_data_cnt;

	PD_BUG_ON(pd_port->svid_data_cnt >= VDO_MAX_SVID_NR);

	while (i < cnt) {
		svid_data = &pd_port->svid_data[i++];
		svid_list[0] = svid_data->svid;

		if (i < cnt) {
			svid_data = &pd_port->svid_data[i++];
			svid_list[1] = svid_data->svid;
		} else
			svid_list[1] = 0;

		svids[j++] = VDO_SVID(svid_list[0], svid_list[1]);
	}

	if ((cnt % 2) == 0)
		svids[j++] = VDO_SVID(0, 0);

	return pd_reply_svdm_request(pd_port, CMDT_RSP_ACK, j, svids);
}

static int dpm_vdm_ufp_response_modes(struct pd_port *pd_port)
{
	struct svdm_svid_data *svid_data;
	uint16_t svid = dpm_vdm_get_svid(pd_port);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	PD_BUG_ON(svid_data == NULL);

	return pd_reply_svdm_request(
		pd_port, CMDT_RSP_ACK,
		svid_data->local_mode.mode_cnt,
		svid_data->local_mode.mode_vdo);
}

/* ---- UFP : Handle VDM Request ---- */

void pd_dpm_ufp_request_id_info(struct pd_port *pd_port)
{
	bool ack = dpm_vdm_get_svid(pd_port) == USB_SID_PD;

	if (!ack) {
		dpm_vdm_reply_svdm_nak(pd_port);
		return;
	}

	dpm_vdm_ufp_response_id(pd_port);
}

void pd_dpm_ufp_request_svid_info(struct pd_port *pd_port)
{
	bool ack = false;

	if (pd_is_support_modal_operation(pd_port))
		ack = (dpm_vdm_get_svid(pd_port) == USB_SID_PD);

	if (!ack) {
		dpm_vdm_reply_svdm_nak(pd_port);
		return;
	}

	dpm_ufp_response_svids(pd_port);
}

void pd_dpm_ufp_request_mode_info(struct pd_port *pd_port)
{
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	bool ack = dpm_get_svdm_svid_data(pd_port, svid) != NULL;

	if (!ack) {
		dpm_vdm_reply_svdm_nak(pd_port);
		return;
	}

	dpm_vdm_ufp_response_modes(pd_port);
}

void pd_dpm_ufp_request_enter_mode(struct pd_port *pd_port)
{
	bool ack = dpm_ufp_update_svid_data_enter_mode(pd_port,
		dpm_vdm_get_svid(pd_port), dpm_vdm_get_ops(pd_port));

	dpm_vdm_reply_svdm_request(pd_port, ack);
}

void pd_dpm_ufp_request_exit_mode(struct pd_port *pd_port)
{
	bool ack = dpm_ufp_update_svid_data_exit_mode(pd_port,
		dpm_vdm_get_svid(pd_port), dpm_vdm_get_ops(pd_port));

	dpm_vdm_reply_svdm_request(pd_port, ack);
}

/* ---- DFP : update_svid_data ---- */

static inline void dpm_dfp_update_partner_id(
			struct pd_port *pd_port, uint32_t *payload)
{
#ifdef CONFIG_USB_PD_KEEP_PARTNER_ID
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	uint32_t size = sizeof(uint32_t) * (cnt);

	pd_port->pe_data.partner_id_present = true;
	memcpy(pd_port->pe_data.partner_vdos, payload, size);
#endif	/* CONFIG_USB_PD_KEEP_PARTNER_ID */
}
static inline void dpm_dfp_update_svid_data_exist(
			struct pd_port *pd_port, uint16_t svid)
{
	uint8_t k;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
#ifdef CONFIG_USB_PD_KEEP_SVIDS
	struct svdm_svid_list *list = &pd_port->pe_data.remote_svid_list;

	if (list->cnt < VDO_MAX_SVID_NR)
		list->svids[list->cnt++] = svid;
	else
		DPM_INFO("ERR:SVIDCNT\n");
#endif	/* CONFIG_USB_PD_KEEP_SVIDS */

	for (k = 0; k < pd_port->svid_data_cnt; k++) {

		svid_data = &pd_port->svid_data[k];

		if (svid_data->svid == svid)
			svid_data->exist = 1;
	}
}

static inline void dpm_dfp_update_svid_data_modes(struct pd_port *pd_port,
	uint16_t svid, uint32_t *mode_list, uint8_t count)
{
	uint8_t i;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("InformMode (0x%04x:%d):\n", svid, count);
	for (i = 0; i < count; i++)
		DPM_DBG("Mode[%d]: 0x%08x\n", i, mode_list[i]);

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
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("EnterMode (svid0x%04x, mode:%d)\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	svid_data->active_mode = ops;
	pd_port->pe_data.modal_operation = true;

	tcpci_enter_mode(tcpc,
		svid_data->svid, ops, svid_data->remote_mode.mode_vdo[ops]);
}

static inline void dpm_dfp_update_svid_data_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	uint8_t i;
	bool modal_operation;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("ExitMode (svid0x%04x, mode:%d)\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	if ((ops == 7) || (ops == svid_data->active_mode)) {
		svid_data->active_mode = 0;

		modal_operation = false;
		for (i = 0; i < pd_port->svid_data_cnt; i++) {

			svid_data = &pd_port->svid_data[i];

			if (svid_data->active_mode) {
				modal_operation = true;
				break;
			}
		}

		pd_port->pe_data.modal_operation = modal_operation;
		tcpci_exit_mode(tcpc, svid);
	}
}


/* ---- DFP : Inform VDM Result ---- */

void pd_dpm_dfp_inform_id(struct pd_port *pd_port, bool ack)
{
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	VDM_STATE_DPM_INFORMED(pd_port);

	if (!payload) {
		dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_ID |
					    DPM_REACTION_DISCOVER_SVID);
		return;
	}

	if (ack) {
		DPM_DBG("InformID, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
				payload[0], payload[1], payload[2], payload[3]);

		dpm_dfp_update_partner_id(pd_port, payload);
	}

	if (!pd_port->pe_data.vdm_discard_retry_flag) {
		/*
		 * For PD compliance test,
		 * If device doesn't reply discoverID
		 * or doesn't support modal operation,
		 * then don't send discoverSVID
		 */
		if (!ack || !(payload[0] & PD_IDH_MODAL_SUPPORT))
			dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_SVID);
		else
			dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_SVID);

		svdm_dfp_inform_id(pd_port, ack);
		dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_ID);
	}
}

static inline int dpm_dfp_consume_svids(
	struct pd_port *pd_port, uint32_t *svid_list, uint8_t count)
{
	bool discover_again = true;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	uint8_t i, j;
	uint16_t svid[2];

	DPM_DBG("InformSVID (%d):\n", count);

	if (count < 6)
		discover_again = false;

	for (i = 0; i < count; i++) {
		svid[0] = PD_VDO_SVID_SVID0(svid_list[i]);
		svid[1] = PD_VDO_SVID_SVID1(svid_list[i]);

		DPM_DBG("svid[%d]: 0x%04x 0x%04x\n", i, svid[0], svid[1]);

		for (j = 0; j < 2; j++) {
			if (svid[j] == 0) {
				discover_again = false;
				break;
			}

			dpm_dfp_update_svid_data_exist(pd_port, svid[j]);
		}
	}

	if (discover_again) {
		DPM_DBG("DiscoverSVID Again\n");
		pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DISCOVER_SVIDS);
		return 1;
	}

	return 0;
}

void pd_dpm_dfp_inform_svids(struct pd_port *pd_port, bool ack)
{
	uint8_t count;
	uint32_t *svid_list;

	VDM_STATE_DPM_INFORMED(pd_port);

	if (ack) {
		count = pd_get_msg_vdm_data_count(pd_port);
		svid_list = pd_get_msg_vdm_data_payload(pd_port);
		if (!svid_list)
			return;
		if (dpm_dfp_consume_svids(pd_port, svid_list, count))
			return;
	}

	if (!pd_port->pe_data.vdm_discard_retry_flag) {
		svdm_dfp_inform_svids(pd_port, ack);
		dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_SVID);
	}
}

void pd_dpm_dfp_inform_modes(struct pd_port *pd_port, bool ack)
{
	uint8_t count;
	uint16_t svid = 0;
	uint32_t *payload;
	uint16_t expected_svid = pd_port->mode_svid;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ack) {
		svid = dpm_vdm_get_svid(pd_port);

		if (svid != expected_svid) {
			ack = false;
			DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
				svid, expected_svid);
		} else {
			count = pd_get_msg_vdm_data_count(pd_port);
			payload = pd_get_msg_vdm_data_payload(pd_port);
			if (payload)
				dpm_dfp_update_svid_data_modes(
					pd_port, svid, payload, count);
		}
	}

	svdm_dfp_inform_modes(pd_port, expected_svid, ack);
	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_dfp_inform_enter_mode(struct pd_port *pd_port, bool ack)
{
	uint8_t ops = 0;
	uint16_t svid = 0;
	uint16_t expected_svid = pd_port->mode_svid;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ack) {
		ops = dpm_vdm_get_ops(pd_port);
		svid = dpm_vdm_get_svid(pd_port);

		/* TODO: check ops later ?! */
		if (svid != expected_svid) {
			ack = false;
			DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
				svid, expected_svid);
		} else {
			dpm_dfp_update_svid_enter_mode(pd_port, svid, ops);
		}
	}

	svdm_dfp_inform_enter_mode(pd_port, expected_svid, ops, ack);
	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_dfp_inform_exit_mode(struct pd_port *pd_port)
{
	uint8_t ops = dpm_vdm_get_ops(pd_port);
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	uint8_t expected_ops = pd_port->mode_obj_pos;
	uint16_t expected_svid = pd_port->mode_svid;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if ((expected_svid != svid) || (expected_ops != ops))
		DPM_DBG("expected_svid & ops wrong\n");

	dpm_dfp_update_svid_data_exit_mode(
		pd_port, expected_svid, expected_ops);

	svdm_dfp_inform_exit_mode(pd_port, expected_svid, expected_ops);
	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_dfp_inform_attention(struct pd_port *pd_port)
{
#if DPM_DBG_ENABLE
	uint8_t ops = dpm_vdm_get_ops(pd_port);
#endif
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("Attention (svid0x%04x, mode:%d)\n", svid, ops);

	svdm_dfp_inform_attention(pd_port, svid);
	VDM_STATE_DPM_INFORMED(pd_port);
}

/* ---- Unstructured VDM ---- */

#ifdef CONFIG_USB_PD_CUSTOM_VDM

void pd_dpm_ufp_recv_uvdm(struct pd_port *pd_port)
{
	struct svdm_svid_data *svid_data;
	uint16_t svid = dpm_vdm_get_svid(pd_port);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	pd_port->uvdm_svid = svid;
	pd_port->uvdm_cnt = pd_get_msg_data_count(pd_port);

	memcpy(pd_port->uvdm_data,
		pd_get_msg_data_payload(pd_port),
		pd_get_msg_data_size(pd_port));

	if (svid_data) {
		if (svid_data->ops->ufp_notify_uvdm)
			svid_data->ops->ufp_notify_uvdm(pd_port, svid_data);
		else
			VDM_STATE_DPM_INFORMED(pd_port);

		tcpci_notify_uvdm(pd_port->tcpc, true);
	} else {
		pd_put_dpm_event(pd_port, PD_DPM_NOT_SUPPORT);
		VDM_STATE_DPM_INFORMED(pd_port);
	}
}

void pd_dpm_dfp_send_uvdm(struct pd_port *pd_port)
{
	pd_send_custom_vdm(pd_port, TCPC_TX_SOP);
	pd_port->uvdm_svid = PD_VDO_VID(pd_port->uvdm_data[0]);

	if (pd_port->uvdm_wait_resp)
		VDM_STATE_RESPONSE_CMD(pd_port, PD_TIMER_UVDM_RESPONSE);
}

void pd_dpm_dfp_inform_uvdm(struct pd_port *pd_port, bool ack)
{
	uint16_t svid;
	uint16_t expected_svid = pd_port->uvdm_svid;
	struct svdm_svid_data *svid_data =
		dpm_get_svdm_svid_data(pd_port, expected_svid);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ack && pd_port->uvdm_wait_resp) {
		svid = dpm_vdm_get_svid(pd_port);

		if (svid != expected_svid) {
			ack = false;
			DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
				svid, expected_svid);
		} else {
			pd_port->uvdm_cnt = pd_get_msg_data_count(pd_port);
			memcpy(pd_port->uvdm_data,
				pd_get_msg_data_payload(pd_port),
				pd_get_msg_data_size(pd_port));
		}
	}

	if (svid_data) {
		if (svid_data->ops->dfp_notify_uvdm)
			svid_data->ops->dfp_notify_uvdm(
				pd_port, svid_data, ack);
	}

	tcpci_notify_uvdm(tcpc, ack);
	pd_notify_tcp_vdm_event_2nd_result(pd_port,
		ack ? TCP_DPM_RET_VDM_ACK : TCP_DPM_RET_VDM_NAK);
	VDM_STATE_DPM_INFORMED(pd_port);
}

#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

void pd_dpm_ufp_send_svdm_nak(struct pd_port *pd_port)
{
	dpm_vdm_reply_svdm_nak(pd_port);
}

/*
 * DRP : Inform Source/Sink Cap
 */

void pd_dpm_dr_inform_sink_cap(struct pd_port *pd_port)
{
	const uint32_t reaction_clear = DPM_REACTION_GET_SINK_CAP
		| DPM_REACTION_ATTEMPT_GET_FLAG;

	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);
	struct pd_port_power_caps *snk_cap = &pd_port->pe_data.remote_snk_cap;

	if (!pd_event_data_msg_match(pd_event, PD_DATA_SINK_CAP))
		return;

	snk_cap->nr = pd_get_msg_data_count(pd_port);
	memcpy(snk_cap->pdos,
		pd_get_msg_data_payload(pd_port),
		pd_get_msg_data_size(pd_port));

	pd_dpm_update_pdos_flags(pd_port, snk_cap->pdos[0]);

	dpm_reaction_clear(pd_port, reaction_clear);
}

void pd_dpm_dr_inform_source_cap(struct pd_port *pd_port)
{
	uint32_t reaction_clear = DPM_REACTION_GET_SOURCE_CAP
		| DPM_REACTION_ATTEMPT_GET_FLAG;

	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;

	if (!pd_event_data_msg_match(pd_event, PD_DATA_SOURCE_CAP))
		return;

	src_cap->nr = pd_get_msg_data_count(pd_port);
	memcpy(src_cap->pdos,
		pd_get_msg_data_payload(pd_port),
		pd_get_msg_data_size(pd_port));

	pd_dpm_update_pdos_flags(pd_port, src_cap->pdos[0]);

	if (!(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_DR_POWER))
		reaction_clear |= DPM_REACTION_GET_SINK_CAP;

	dpm_reaction_clear(pd_port, reaction_clear);
}

/*
 * DRP : Data Role Swap
 */

#ifdef CONFIG_USB_PD_DR_SWAP

void pd_dpm_drs_evaluate_swap(struct pd_port *pd_port, uint8_t role)
{
	pd_put_dpm_ack_event(pd_port);
}

void pd_dpm_drs_change_role(struct pd_port *pd_port, uint8_t role)
{
	pd_set_data_role(pd_port, role);

	pd_port->pe_data.pe_ready = false;

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	pd_port->pe_data.pd_traffic_idle = false;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

#ifdef CONFIG_USB_PD_DFP_FLOW_DELAY_DRSWAP
	dpm_reaction_set(pd_port, DPM_REACTION_DFP_FLOW_DELAY);
#else
	dpm_reaction_clear(pd_port, DPM_REACTION_DFP_FLOW_DELAY);
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY_DRSWAP */

	PE_STATE_DPM_INFORMED(pd_port);
}

#endif	/* CONFIG_USB_PD_DR_SWAP */

/*
 * DRP : Power Role Swap
 */

#ifdef CONFIG_USB_PD_PR_SWAP

#if 0
static bool pd_dpm_evaluate_source_cap_match(pd_port_t *pd_port)
{
	int i, j;
	bool find_cap = false;
	struct dpm_pdo_info_t sink, source;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;

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
}
#endif

/*
 * Rules:
 * External Sources -> EXS
 * Provider/Consumers -> PC
 * Consumers/Provider -> CP
 * 1.  PC (with EXS) shall always deny PR_SWAP from CP (without EXS)
 * 2.  PC (without EXS) shall always acppet PR_SWAP from CP (with EXS)
 * unless the requester isn't able to provide PDOs.
 */

int dpm_check_good_power(struct pd_port *pd_port)
{
	bool local_ex, partner_ex;

	local_ex =
		(pd_port->dpm_caps & DPM_CAP_LOCAL_EXT_POWER) != 0;

	partner_ex =
		(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_EXTPOWER) != 0;

	if (local_ex != partner_ex) {
		if (partner_ex)
			return GOOD_PW_PARTNER;
		return GOOD_PW_LOCAL;
	}

	if (local_ex)
		return GOOD_PW_BOTH;

	return GOOD_PW_NONE;
}

void pd_dpm_prs_evaluate_swap(struct pd_port *pd_port, uint8_t role)
{
	int good_power;
	bool sink, accept = true;

	bool check_src = (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SRC) ?
		true : false;
	bool check_snk = (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SNK) ?
		true : false;

#ifdef CONFIG_USB_PD_SRC_REJECT_PR_SWAP_IF_GOOD_PW
	bool check_ext =
		(pd_port->dpm_caps & DPM_CAP_CHECK_EXT_POWER) ? true : false;

	if (check_ext)
		check_src = true;
#endif	/* CONFIG_USB_PD_SRC_REJECT_PR_SWAP_IF_GOOD_PW */

	if (check_src|check_snk) {
		sink = pd_port->power_role == PD_ROLE_SINK;
		good_power = dpm_check_good_power(pd_port);

		switch (good_power) {
		case GOOD_PW_PARTNER:
			if (sink && check_snk)
				accept = false;
			break;

		case GOOD_PW_LOCAL:
			if ((!sink) && (check_src))
				accept = false;
			break;

		default:
			accept = true;
			break;
		}
	}

	dpm_response_request(pd_port, accept);
}

void pd_dpm_prs_turn_off_power_sink(struct pd_port *pd_port)
{
	/* iSnkSwapStdby : 2.5mA */
	tcpci_sink_vbus(pd_port->tcpc,
		TCP_VBUS_CTRL_PR_SWAP, TCPC_VBUS_SINK_0V, 0);
}

void pd_dpm_prs_enable_power_source(struct pd_port *pd_port, bool en)
{
	int vbus_level = en ? TCPC_VBUS_SOURCE_5V : TCPC_VBUS_SOURCE_0V;

	tcpci_source_vbus(pd_port->tcpc,
		TCP_VBUS_CTRL_PR_SWAP, vbus_level, -1);

	if (en)
		pd_enable_vbus_valid_detection(pd_port, en);
	else
		pd_enable_vbus_safe0v_detection(pd_port);
}

void pd_dpm_prs_change_role(struct pd_port *pd_port, uint8_t role)
{
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	pd_port->pe_data.pd_traffic_idle = false;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	dpm_reaction_clear(pd_port, DPM_REACTION_REQUEST_PR_SWAP);
	pd_set_power_role(pd_port, role);
	pd_put_dpm_ack_event(pd_port);
}

#endif	/* CONFIG_USB_PD_PR_SWAP */

/*
 * DRP : Vconn Swap
 */

#ifdef CONFIG_USB_PD_VCONN_SWAP

void pd_dpm_vcs_evaluate_swap(struct pd_port *pd_port)
{
	bool accept = true;

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	struct tcpc_device *tcpc = pd_port->tcpc;

	/* Reject it if we don't want supply vconn */
	if ((!pd_port->vconn_role) &&
		(tcpc->tcpc_vconn_supply == TCPC_VCONN_SUPPLY_NEVER))
		accept = false;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	dpm_response_request(pd_port, accept);
}

void pd_dpm_vcs_enable_vconn(struct pd_port *pd_port, uint8_t role)
{
	pd_set_vconn(pd_port, role);

	/* If we can't enable vconn immediately,
	 * then after vconn_on,
	 * Vconn Controller should pd_put_dpm_ack_event()
	 */

#if CONFIG_USB_PD_VCONN_READY_TOUT
	if (role != PD_ROLE_VCONN_OFF) {
		pd_enable_timer(pd_port, PD_TIMER_VCONN_READY);
		return;
	}
#endif	/* CONFIG_USB_PD_VCONN_READY_TOUT */

	PE_STATE_DPM_ACK_IMMEDIATELY(pd_port);
}

#endif	/* CONFIG_USB_PD_VCONN_SWAP */

/*
 * PE : PD3.0
 */

#ifdef CONFIG_USB_PD_REV30

#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
void pd_dpm_inform_source_cap_ext(struct pd_port *pd_port)
{
	struct pd_source_cap_ext *scedb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_SOURCE_CAP_EXT)) {
		scedb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%04x, pid=0x%04x\n", scedb->vid, scedb->pid);
		DPM_INFO2("fw_ver=0x%02x, hw_ver=0x%02x\n",
			scedb->fw_ver, scedb->hw_ver);

		dpm_reaction_clear(pd_port,
			DPM_REACTION_GET_SOURCE_CAP_EXT);
	}
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */

#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
int pd_dpm_send_source_cap_ext(struct pd_port *pd_port)
{
	return pd_send_sop_ext_msg(pd_port, PD_EXT_SOURCE_CAP_EXT,
		PD_SCEDB_SIZE, &pd_port->src_cap_ext);
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */

#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL

static const struct pd_battery_capabilities c_invalid_bcdb = {
	0, 0, 0, 0, PD_BCDB_BAT_TYPE_INVALID
};

int pd_dpm_send_battery_cap(struct pd_port *pd_port)
{
	struct pd_battery_info *bat_info;
	const struct pd_battery_capabilities *bcdb;
	struct pd_get_battery_capabilities *gbcdb =
		pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO2("bat_ref=%d\n", gbcdb->bat_cap_ref);

	bat_info = pd_get_battery_info(pd_port, gbcdb->bat_cap_ref);

	if (bat_info != NULL) {
		tcpci_notify_request_bat_info(
			tcpc, gbcdb->bat_cap_ref);
		bcdb = &bat_info->bat_cap;
	} else
		bcdb = &c_invalid_bcdb;

	return pd_send_sop_ext_msg(pd_port, PD_EXT_BAT_CAP,
		PD_BCDB_SIZE, bcdb);
}
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */

#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
void pd_dpm_inform_battery_cap(struct pd_port *pd_port)
{
	struct pd_battery_capabilities *bcdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_BAT_CAP)) {
		bcdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%04x, pid=0x%04x\n",
			bcdb->vid, bcdb->pid);
	}
}
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */

#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL

static const uint32_t c_invalid_bsdo =
	BSDO(0xffff, BSDO_BAT_INFO_INVALID_REF);

int pd_dpm_send_battery_status(struct pd_port *pd_port)
{
	const uint32_t *bsdo;
	struct pd_battery_info *bat_info;
	struct pd_get_battery_status *gbsdb =
		pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO2("bat_ref=%d\n", gbsdb->bat_status_ref);

	bat_info = pd_get_battery_info(pd_port, gbsdb->bat_status_ref);

	if (bat_info != NULL) {
		tcpci_notify_request_bat_info(
			tcpc, gbsdb->bat_status_ref);
		bsdo = &bat_info->bat_status;
	} else
		bsdo = &c_invalid_bsdo;

#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	pd_port->pe_data.get_status_once = true;
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

	return pd_send_sop_data_msg(pd_port,
		PD_DATA_BAT_STATUS, PD_BSDO_SIZE, bsdo);
}
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */


#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
void pd_dpm_inform_battery_status(struct pd_port *pd_port)
{
	uint32_t *payload;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_data_msg_event(pd_port, PD_DATA_BAT_STATUS)) {
		payload = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("0x%08x\n", payload[0]);
	}
}
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */

static const struct pd_manufacturer_info c_invalid_mfrs = {
	.vid = 0xFFFF, .pid = 0, .mfrs_string = "Not Supported",
};

#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
int pd_dpm_send_mfrs_info(struct pd_port *pd_port)
{
	uint8_t len = 0;
	struct pd_battery_info *bat_info;
	const struct pd_manufacturer_info *midb = NULL;

	struct pd_get_manufacturer_info *gmidb =
		pd_get_msg_data_payload(pd_port);

	if (gmidb->info_target == PD_GMIDB_TARGET_PORT)
		midb = &pd_port->mfrs_info;

	if (gmidb->info_target == PD_GMIDB_TARGET_BATTRY) {
		bat_info = pd_get_battery_info(pd_port, gmidb->info_ref);
		if (bat_info)
			midb = &bat_info->mfrs_info;
	}

	if (midb == NULL)
		midb = &c_invalid_mfrs;

	len = strnlen((char *)midb->mfrs_string, sizeof(midb->mfrs_string));
	if (len < sizeof(midb->mfrs_string))
		len++;
	return pd_send_sop_ext_msg(pd_port, PD_EXT_MFR_INFO,
		PD_MIDB_MIN_SIZE + len, midb);
}
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */

#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
void pd_dpm_inform_mfrs_info(struct pd_port *pd_port)
{
	struct pd_manufacturer_info *midb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_MFR_INFO)) {
		midb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%x, pid=0x%x\n", midb->vid, midb->pid);
	}
}
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */


#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
void pd_dpm_inform_country_codes(struct pd_port *pd_port)
{
	struct pd_country_codes *ccdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_COUNTRY_CODES)) {
		ccdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("len=%d, country_code[0]=0x%04x\n",
			ccdb->length, ccdb->country_code[0]);
	}
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */


#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
int pd_dpm_send_country_codes(struct pd_port *pd_port)
{
	uint8_t i;
	struct pd_country_codes ccdb;
	struct pd_country_authority *ca = pd_port->country_info;

	ccdb.length = pd_port->country_nr;

	for (i = 0; i < ccdb.length; i++)
		ccdb.country_code[i] = ca[i].code;

	return pd_send_sop_ext_msg(pd_port, PD_EXT_COUNTRY_CODES,
		2 + ccdb.length*2, &ccdb);
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */

#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
void pd_dpm_inform_country_info(struct pd_port *pd_port)
{
	struct pd_country_info *cidb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_COUNTRY_INFO)) {
		cidb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("cc=0x%04x, ci=%d\n",
			cidb->country_code, cidb->country_special_data[0]);
	}
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */

#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
int pd_dpm_send_country_info(struct pd_port *pd_port)
{
	uint8_t i, cidb_size;
	struct pd_country_info cidb;
	struct pd_country_authority *ca = pd_port->country_info;
	uint32_t *pccdo = pd_get_msg_data_payload(pd_port);
	uint16_t cc = CCDO_COUNTRY_CODE(*pccdo);

	cidb_size = PD_CIDB_MIN_SIZE;
	cidb.country_code = cc;
	cidb.reserved = 0;
	cidb.country_special_data[0] = 0;

	for (i = 0; i < pd_port->country_nr; i++) {
		if (ca[i].code == cc) {
			cidb_size += ca[i].len;
			memcpy(cidb.country_special_data,
					ca[i].data, ca[i].len);
			break;
		}
	}

	return pd_send_sop_ext_msg(pd_port, PD_EXT_COUNTRY_INFO,
		 cidb_size, &cidb);
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */

#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
void pd_dpm_inform_alert(struct pd_port *pd_port)
{
	uint32_t *data = pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO("inform_alert:0x%08x\n", data[0]);

	pd_port->pe_data.pd_traffic_idle = false;
	pd_port->pe_data.remote_alert = data[0];
	tcpci_notify_alert(pd_port->tcpc, data[0]);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
int pd_dpm_send_alert(struct pd_port *pd_port)
{
	uint32_t ado = pd_port->pe_data.local_alert;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	pd_port->pe_data.local_alert = 0;
	DPM_INFO("send_alert:0x%08x\n", ado);

	return pd_send_sop_data_msg(pd_port, PD_DATA_ALERT,
		PD_ADO_SIZE, &ado);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
void pd_dpm_inform_status(struct pd_port *pd_port)
{
	struct pd_status *sdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_STATUS)) {
		sdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("Temp=%d, IN=0x%x, BAT_IN=0x%x, EVT=0x%x, PTF=0x%x\n",
			sdb->internal_temp, sdb->present_input,
			sdb->present_battery_input, sdb->event_flags,
			PD_STATUS_TEMP_PTF(sdb->temp_status));

		tcpci_notify_status(tcpc, sdb);
	}
}
#endif /* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
int pd_dpm_send_status(struct pd_port *pd_port)
{
	struct pd_status sdb;
	struct pe_data *pe_data = &pd_port->pe_data;

	memset(&sdb, 0, PD_SDB_SIZE);

	sdb.present_input = pd_port->pd_status_present_in;

#ifdef CONFIG_USB_PD_REV30_BAT_INFO
	if (sdb.present_input &
		PD_STATUS_INPUT_INT_POWER_BAT) {
		sdb.present_battery_input = pd_port->pd_status_bat_in;
	}
#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */

	sdb.event_flags = pe_data->pd_status_event;
	pe_data->pd_status_event &= ~PD_STASUS_EVENT_READ_CLEAR;

#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP
	sdb.internal_temp = pd_port->pd_status_temp;
	sdb.temp_status = pd_port->pd_status_temp_status;
#else
	sdb.internal_temp = 0;
	sdb.temp_status = 0;
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP */

	if (sdb.event_flags & PD_STATUS_EVENT_OTP)
		sdb.temp_status = PD_STATUS_TEMP_SET_PTF(PD_PTF_OVER_TEMP);

	if (pd_port->power_role !=  PD_ROLE_SINK)
		sdb.event_flags &= ~PD_STATUS_EVENT_OVP;

#ifdef CONFIG_USB_PD_REV30_ALERT_LOCAL
	pe_data->get_status_once = true;
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

	return pd_send_sop_ext_msg(pd_port, PD_EXT_STATUS,
			PD_SDB_SIZE, &sdb);
}
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */


#ifdef CONFIG_USB_PD_REV30_PPS_SINK
void pd_dpm_inform_pps_status(struct pd_port *pd_port)
{
	struct pd_pps_status_raw *ppssdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_PPS_STATUS)) {
		ppssdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("mv=%d, ma=%d\n",
			PD_PPS_GET_OUTPUT_MV(ppssdb->output_vol_raw),
			PD_PPS_GET_OUTPUT_MA(ppssdb->output_curr_raw));
	}
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

void pd_dpm_inform_not_support(struct pd_port *pd_port)
{
	/* TODO */
}

#endif	/* CONFIG_USB_PD_REV30 */

/*
 * PE : Dynamic Control Vconn
 */

void pd_dpm_dynamic_enable_vconn(struct pd_port *pd_port)
{
#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (tcpc->tcpc_vconn_supply <= TCPC_VCONN_SUPPLY_ALWAYS)
		return;

	if (pd_port->vconn_role == PD_ROLE_VCONN_DYNAMIC_OFF) {
		DPM_INFO2("DynamicVCEn\n");
		pd_set_vconn(pd_port, PD_ROLE_VCONN_DYNAMIC_ON);
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */
}

void pd_dpm_dynamic_disable_vconn(struct pd_port *pd_port)
{
#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	bool keep_vconn;
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (!pd_port->vconn_role)
		return;

	switch (tcpc->tcpc_vconn_supply) {
	case TCPC_VCONN_SUPPLY_EMARK_ONLY:
		keep_vconn = pd_port->pe_data.power_cable_present;
		break;
	case TCPC_VCONN_SUPPLY_STARTUP:
		keep_vconn = false;
		break;
	default:
		keep_vconn = true;
		break;
	}

	if (keep_vconn)
		return;

	if (tcpc->tcp_event_count)
		return;

	if (pd_port->vconn_role != PD_ROLE_VCONN_DYNAMIC_OFF) {
		DPM_INFO2("DynamicVCDis\n");
		pd_set_vconn(pd_port, PD_ROLE_VCONN_DYNAMIC_OFF);
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */
}

/*
 * PE : Notify DPM
 */

int pd_dpm_notify_pe_startup(struct pd_port *pd_port)
{
	uint32_t reactions = DPM_REACTION_CAP_ALWAYS;

#ifdef CONFIG_USB_PD_DFP_FLOW_DELAY_STARTUP
	reactions |= DPM_REACTION_DFP_FLOW_DELAY;
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY_STARTUP */

#ifdef CONFIG_USB_PD_UFP_FLOW_DELAY
	reactions |= DPM_REACTION_UFP_FLOW_DELAY;
#endif	/* CONFIG_USB_PD_UFP_FLOW_DELAY */

#ifdef CONFIG_USB_PD_SRC_TRY_PR_SWAP_IF_BAD_PW
	reactions |= DPM_REACTION_ATTEMPT_GET_FLAG |
		DPM_REACTION_REQUEST_PR_SWAP;
#else
	if (DPM_CAP_EXTRACT_PR_CHECK(pd_port->dpm_caps)) {
		reactions |= DPM_REACTION_REQUEST_PR_SWAP;
		if (DPM_CAP_EXTRACT_PR_CHECK(pd_port->dpm_caps) ==
			DPM_CAP_PR_CHECK_PREFER_SNK)
			reactions |= DPM_REACTION_ATTEMPT_GET_FLAG;
	}

	if (pd_port->dpm_caps & DPM_CAP_CHECK_EXT_POWER)
		reactions |= DPM_REACTION_ATTEMPT_GET_FLAG;
#endif	/* CONFIG_USB_PD_SRC_TRY_PR_SWAP_IF_BAD_PW */

	if (DPM_CAP_EXTRACT_DR_CHECK(pd_port->dpm_caps)) {
		reactions |= DPM_REACTION_REQUEST_DR_SWAP;
		if (DPM_CAP_EXTRACT_DR_CHECK(pd_port->dpm_caps) ==
			DPM_CAP_DR_CHECK_PREFER_UFP)
			reactions |= DPM_REACTION_ATTEMPT_GET_FLAG;
	}

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_CABLE)
		reactions |= DPM_REACTION_CAP_DISCOVER_CABLE;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_CABLE_DFP)
		reactions |= DPM_REACTION_DISCOVER_CABLE_FLOW;

#ifdef CONFIG_USB_PD_ATTEMP_ENTER_MODE
	reactions |= DPM_REACTION_DISCOVER_ID |
		DPM_REACTION_DISCOVER_SVID;
#else
	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_ID)
		reactions |= DPM_REACTION_DISCOVER_ID;
	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_DISCOVER_SVID)
		reactions |= DPM_REACTION_DISCOVER_SVID;
#endif	/* CONFIG_USB_PD_ATTEMP_ENTER_MODE */

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
	reactions |= DPM_REACTION_GET_SOURCE_CAP_EXT;
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
#endif	/* CONFIG_USB_PD_REV30 */

	dpm_reaction_set(pd_port, reactions);

	svdm_reset_state(pd_port);
	svdm_notify_pe_startup(pd_port);
	return 0;

}

int pd_dpm_notify_pe_hardreset(struct pd_port *pd_port)
{
	struct pe_data *pe_data = &pd_port->pe_data;

	svdm_reset_state(pd_port);

	pe_data->pe_ready = false;

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	pe_data->pd_traffic_idle = false;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	if (pe_data->dpm_svdm_retry_cnt >= CONFIG_USB_PD_DPM_SVDM_RETRY)
		return 0;

	pe_data->dpm_svdm_retry_cnt++;

#ifdef CONFIG_USB_PD_ATTEMP_ENTER_MODE
	dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_ID |
		DPM_REACTION_DISCOVER_SVID);
#endif	/* CONFIG_USB_PD_ATTEMP_ENTER_MODE */

	svdm_notify_pe_startup(pd_port);
	return 0;
}

/*
 * SVDM
 */

static inline bool dpm_register_svdm_ops(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, const struct svdm_svid_ops *ops)
{
	bool ret = true;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ops->parse_svid_data)
		ret = ops->parse_svid_data(pd_port, svid_data);

	if (ret) {
		svid_data->ops = ops;
		svid_data->svid = ops->svid;
		DPM_DBG("register_svdm: 0x%x\n", ops->svid);
	}

	return ret;
}

struct svdm_svid_data *dpm_get_svdm_svid_data(
		struct pd_port *pd_port, uint16_t svid)
{
	uint8_t i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->svid == svid)
			return svid_data;
	}

	return NULL;
}

bool svdm_reset_state(struct pd_port *pd_port)
{
	int i;
	struct svdm_svid_data *svid_data;

	pd_port->dpm_charging_policy = pd_port->dpm_charging_policy_default;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->reset_state)
			svid_data->ops->reset_state(pd_port, svid_data);
	}

	return true;
}

bool svdm_notify_pe_startup(struct pd_port *pd_port)
{
	int i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->notify_pe_startup)
			svid_data->ops->notify_pe_startup(pd_port, svid_data);
	}

	return true;
}

/*
 * dpm_core_init
 */

int pd_dpm_core_init(struct pd_port *pd_port)
{
	int i, j;
	bool ret;
	uint8_t svid_ops_nr = ARRAY_SIZE(svdm_svid_ops);
	struct tcpc_device *tcpc = pd_port->tcpc;

	pd_port->svid_data = devm_kzalloc(&tcpc->dev,
		sizeof(struct svdm_svid_data) * svid_ops_nr, GFP_KERNEL);

	if (!pd_port->svid_data)
		return -ENOMEM;

	for (i = 0, j = 0; i < svid_ops_nr; i++) {
		ret = dpm_register_svdm_ops(pd_port,
			&pd_port->svid_data[j], &svdm_svid_ops[i]);

		if (ret)
			j++;
	}

	pd_port->svid_data_cnt = j;

#ifdef CONFIG_USB_PD_REV30
	pd_port->pps_request_wake_lock =
		wakeup_source_register(&tcpc->dev, "pd_pps_request_wake_lock");
	init_waitqueue_head(&pd_port->pps_request_wait_que);
	atomic_set(&pd_port->pps_request, false);
	pd_port->pps_request_task = kthread_run(pps_request_thread_fn, tcpc,
						"pps_request_%s",
						tcpc->desc.name);
#endif /* CONFIG_USB_PD_REV30 */

	return 0;
}
