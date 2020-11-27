// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2018, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/ipa.h>
#include <linux/vmalloc.h>

#include "ipa_qmi_service.h"
#include "ipa_ram_mmap.h"
#include "../ipa_common_i.h"

#define IPA_Q6_SVC_VERS 1
#define IPA_A5_SVC_VERS 1
#define Q6_QMI_COMPLETION_TIMEOUT (60*HZ)

#define IPA_A5_SERVICE_SVC_ID 0x31
#define IPA_A5_SERVICE_INS_ID 1
#define IPA_Q6_SERVICE_SVC_ID 0x31
#define IPA_Q6_SERVICE_INS_ID 2

#define QMI_SEND_STATS_REQ_TIMEOUT_MS 5000
#define QMI_SEND_REQ_TIMEOUT_MS 60000

#define QMI_IPA_FORCE_CLEAR_DATAPATH_TIMEOUT_MS 1000

static struct qmi_handle *ipa_svc_handle;
static struct workqueue_struct *ipa_clnt_req_workqueue;
static bool qmi_modem_init_fin, qmi_indication_fin;
static uint32_t ipa_wan_platform;
struct ipa_qmi_context *ipa_qmi_ctx;
static bool first_time_handshake;
static atomic_t workqueues_stopped;
static atomic_t ipa_qmi_initialized;
struct mutex ipa_qmi_lock;

struct ipa_msg_desc {
	uint16_t msg_id;
	int max_msg_len;
	struct qmi_elem_info *ei_array;
};

/* QMI A5 service */

static void handle_indication_req(struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_indication_reg_req_msg_v01 *indication_req;
	struct ipa_indication_reg_resp_msg_v01 resp;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;
	int rc;

	indication_req = (struct ipa_indication_reg_req_msg_v01 *)decoded_msg;
	IPAWANDBG("Received INDICATION Request\n");

	/* cache the client sq */
	memcpy(&ipa_qmi_ctx->client_sq, sq, sizeof(*sq));

	memset(&resp, 0, sizeof(struct ipa_indication_reg_resp_msg_v01));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	rc = qmi_send_response(qmi_handle, sq, txn,
			QMI_IPA_INDICATION_REGISTER_RESP_V01,
			QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01,
			ipa_indication_reg_resp_msg_data_v01_ei,
			&resp);

	if (rc < 0) {
		IPAWANERR("send response for Indication register failed\n");
		return;
	}

	qmi_indication_fin = true;
	/* check if need sending indication to modem */
	if (qmi_modem_init_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		qmi_modem_init_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;
		rc = qmi_send_indication(qmi_handle,
			&(ipa_qmi_ctx->client_sq),
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01,
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01,
			ipa_master_driver_init_complt_ind_msg_data_v01_ei,
			&ind);

		if (rc < 0) {
			IPAWANERR("send indication failed\n");
			qmi_indication_fin = false;
		}
	} else {
		IPAWANERR("not send indication\n");
	}
}


static void handle_install_filter_rule_req(struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_install_fltr_rule_req_msg_v01 *rule_req;
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	uint32_t rule_hdl[MAX_NUM_Q6_RULE];
	int rc = 0, i;

	rule_req = (struct ipa_install_fltr_rule_req_msg_v01 *)decoded_msg;
	memset(rule_hdl, 0, sizeof(rule_hdl));
	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_msg_v01));
	IPAWANDBG("Received install filter Request\n");

	rc = copy_ul_filter_rule_to_ipa((struct
		ipa_install_fltr_rule_req_msg_v01*)decoded_msg, rule_hdl);
	if (rc) {
		IPAWANERR("copy UL rules from modem is failed\n");
		return;
	}

	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	if (rule_req->filter_spec_list_valid == true) {
		resp.filter_handle_list_valid = true;
		if (rule_req->filter_spec_list_len > MAX_NUM_Q6_RULE) {
			resp.filter_handle_list_len = MAX_NUM_Q6_RULE;
			IPAWANERR("installed (%d) max Q6-UL rules ",
			MAX_NUM_Q6_RULE);
			IPAWANERR("but modem gives total (%u)\n",
			rule_req->filter_spec_list_len);
		} else {
			resp.filter_handle_list_len =
				rule_req->filter_spec_list_len;
		}
	} else {
		resp.filter_handle_list_valid = false;
	}

	/* construct UL filter rules response to Modem*/
	for (i = 0; i < resp.filter_handle_list_len; i++) {
		resp.filter_handle_list[i].filter_spec_identifier =
			rule_req->filter_spec_list[i].filter_spec_identifier;
		resp.filter_handle_list[i].filter_handle = rule_hdl[i];
	}

	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_INSTALL_FILTER_RULE_RESP_V01,
		QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01,
		ipa_install_fltr_rule_resp_msg_data_v01_ei,
		&resp);

	if (rc < 0)
		IPAWANERR("install filter rules failed\n");
	else
		IPAWANDBG("Replied to install filter request\n");
}

static void handle_filter_installed_notify_req(
	struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	int rc = 0;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	IPAWANDBG("Received filter_install_notify Request\n");
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;

	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01,
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01,
		ipa_fltr_installed_notif_resp_msg_data_v01_ei,
		&resp);

	if (rc < 0)
		IPAWANERR("handle filter rules failed\n");
	else
		IPAWANDBG("Responsed filter_install_notify Request\n");
}

static void handle_ipa_config_req(struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_config_resp_msg_v01 resp;
	int rc;

	memset(&resp, 0, sizeof(struct ipa_config_resp_msg_v01));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	IPAWANDBG("Received IPA CONFIG Request\n");
	rc = ipa_mhi_handle_ipa_config_req(
		(struct ipa_config_req_msg_v01 *)decoded_msg);
	if (rc) {
		IPAERR("ipa_mhi_handle_ipa_config_req failed %d\n", rc);
		resp.resp.result = IPA_QMI_RESULT_FAILURE_V01;
	}
	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp.resp.result, resp.resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_CONFIG_RESP_V01,
		QMI_IPA_CONFIG_RESP_MAX_MSG_LEN_V01,
		ipa_config_resp_msg_data_v01_ei,
		&resp);

	if (rc < 0)
		IPAWANERR("QMI_IPA_CONFIG_RESP_V01 failed\n");
	else
		IPAWANDBG("Responsed QMI_IPA_CONFIG_RESP_V01\n");
}

static void ipa_a5_svc_disconnect_cb(struct qmi_handle *qmi,
	unsigned int node, unsigned int port)
{
	IPAWANDBG_LOW("Received QMI client disconnect\n");
}

/****************************************************/
/*                 QMI A5 client ->Q6               */
/****************************************************/
static void ipa_q6_clnt_svc_arrive(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_svc_arrive, ipa_q6_clnt_svc_arrive);
static void ipa_q6_clnt_svc_exit(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_svc_exit, ipa_q6_clnt_svc_exit);
/* Test client port for IPC Router */
static struct qmi_handle *ipa_q6_clnt;

static int ipa_check_qmi_response(int rc,
				  int req_id,
				  enum ipa_qmi_result_type_v01 result,
				  enum ipa_qmi_error_type_v01 error,
				  char *resp_type)
{
	if (rc < 0) {
		if (rc == -ETIMEDOUT && ipa_rmnet_ctx.ipa_rmnet_ssr) {
			IPAWANERR(
			"Timeout for qmi request id %d\n", req_id);
			return rc;
		}
		if ((rc == -ENETRESET) || (rc == -ENODEV)) {
			IPAWANERR(
			"SSR while waiting for qmi request id %d\n", req_id);
			return rc;
		}
		IPAWANERR("Error sending qmi request id %d, rc = %d\n",
			req_id, rc);
		return rc;
	}
	if (result != IPA_QMI_RESULT_SUCCESS_V01 &&
	    ipa_rmnet_ctx.ipa_rmnet_ssr) {
		IPAWANERR(
		"Got bad response %d from request id %d (error %d)\n",
		req_id, result, error);
		return result;
	}
	IPAWANDBG_LOW("Received %s successfully\n", resp_type);
	return 0;
}

static int ipa_qmi_send_req_wait(struct qmi_handle *client_handle,
	struct ipa_msg_desc *req_desc, void *req,
	struct ipa_msg_desc *resp_desc, void *resp,
	unsigned long timeout_ms)
{
	struct qmi_txn txn;
	int ret;

	ret = qmi_txn_init(client_handle, &txn, resp_desc->ei_array, resp);

	if (ret < 0) {
		IPAWANERR("QMI txn init failed, ret= %d\n", ret);
		return ret;
	}

	ret = qmi_send_request(client_handle,
		&ipa_qmi_ctx->server_sq,
		&txn,
		req_desc->msg_id,
		req_desc->max_msg_len,
		req_desc->ei_array,
		req);

	if (ret < 0) {
		qmi_txn_cancel(&txn);
		return ret;
	}
	ret = qmi_txn_wait(&txn, msecs_to_jiffies(timeout_ms));

	return ret;
}

static int qmi_init_modem_send_sync_msg(void)
{
	struct ipa_init_modem_driver_req_msg_v01 req;
	struct ipa_init_modem_driver_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;
	u16 smem_restr_bytes = ipa2_get_smem_restr_bytes();

	memset(&req, 0, sizeof(struct ipa_init_modem_driver_req_msg_v01));
	memset(&resp, 0, sizeof(struct ipa_init_modem_driver_resp_msg_v01));

	req.platform_type_valid = true;
	req.platform_type = ipa_wan_platform;

	req.hdr_tbl_info_valid = (IPA_MEM_PART(modem_hdr_size) != 0);
	req.hdr_tbl_info.modem_offset_start =
		IPA_MEM_PART(modem_hdr_ofst) + smem_restr_bytes;
	req.hdr_tbl_info.modem_offset_end = IPA_MEM_PART(modem_hdr_ofst) +
		smem_restr_bytes + IPA_MEM_PART(modem_hdr_size) - 1;

	req.v4_route_tbl_info_valid = true;
	req.v4_route_tbl_info.route_tbl_start_addr = IPA_MEM_PART(v4_rt_ofst) +
		smem_restr_bytes;
	req.v4_route_tbl_info.num_indices = IPA_MEM_PART(v4_modem_rt_index_hi);
	req.v6_route_tbl_info_valid = true;

	req.v6_route_tbl_info.route_tbl_start_addr = IPA_MEM_PART(v6_rt_ofst) +
		smem_restr_bytes;
	req.v6_route_tbl_info.num_indices = IPA_MEM_PART(v6_modem_rt_index_hi);

	req.v4_filter_tbl_start_addr_valid = true;
	req.v4_filter_tbl_start_addr =
		IPA_MEM_PART(v4_flt_ofst) + smem_restr_bytes;

	req.v6_filter_tbl_start_addr_valid = true;
	req.v6_filter_tbl_start_addr =
		IPA_MEM_PART(v6_flt_ofst) + smem_restr_bytes;

	req.modem_mem_info_valid = (IPA_MEM_PART(modem_size) != 0);
	req.modem_mem_info.block_start_addr =
		IPA_MEM_PART(modem_ofst) + smem_restr_bytes;
	req.modem_mem_info.size = IPA_MEM_PART(modem_size);

	req.ctrl_comm_dest_end_pt_valid = true;
	req.ctrl_comm_dest_end_pt =
		ipa2_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);

	req.hdr_proc_ctx_tbl_info_valid =
		(IPA_MEM_PART(modem_hdr_proc_ctx_size) != 0);
	req.hdr_proc_ctx_tbl_info.modem_offset_start =
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst) + smem_restr_bytes;
	req.hdr_proc_ctx_tbl_info.modem_offset_end =
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst) +
		IPA_MEM_PART(modem_hdr_proc_ctx_size) + smem_restr_bytes - 1;

	req.zip_tbl_info_valid = (IPA_MEM_PART(modem_comp_decomp_size) != 0);
	req.zip_tbl_info.modem_offset_start =
		IPA_MEM_PART(modem_comp_decomp_size) + smem_restr_bytes;
	req.zip_tbl_info.modem_offset_end =
		IPA_MEM_PART(modem_comp_decomp_ofst) +
		IPA_MEM_PART(modem_comp_decomp_size) + smem_restr_bytes - 1;

	if (!ipa_uc_loaded_check()) {  /* First time boot */
		req.is_ssr_bootup_valid = false;
		req.is_ssr_bootup = 0;
	} else {  /* After SSR boot */
		req.is_ssr_bootup_valid = true;
		req.is_ssr_bootup = 1;
	}

	IPAWANDBG("platform_type %d\n", req.platform_type);
	IPAWANDBG("hdr_tbl_info.modem_offset_start %d\n",
			req.hdr_tbl_info.modem_offset_start);
	IPAWANDBG("hdr_tbl_info.modem_offset_end %d\n",
			req.hdr_tbl_info.modem_offset_end);
	IPAWANDBG("v4_route_tbl_info.route_tbl_start_addr %d\n",
			req.v4_route_tbl_info.route_tbl_start_addr);
	IPAWANDBG("v4_route_tbl_info.num_indices %d\n",
			req.v4_route_tbl_info.num_indices);
	IPAWANDBG("v6_route_tbl_info.route_tbl_start_addr %d\n",
			req.v6_route_tbl_info.route_tbl_start_addr);
	IPAWANDBG("v6_route_tbl_info.num_indices %d\n",
			req.v6_route_tbl_info.num_indices);
	IPAWANDBG("v4_filter_tbl_start_addr %d\n",
			req.v4_filter_tbl_start_addr);
	IPAWANDBG("v6_filter_tbl_start_addr %d\n",
			req.v6_filter_tbl_start_addr);
	IPAWANDBG("modem_mem_info.block_start_addr %d\n",
			req.modem_mem_info.block_start_addr);
	IPAWANDBG("modem_mem_info.size %d\n",
			req.modem_mem_info.size);
	IPAWANDBG("ctrl_comm_dest_end_pt %d\n",
			req.ctrl_comm_dest_end_pt);
	IPAWANDBG("is_ssr_bootup %d\n",
			req.is_ssr_bootup);

	req_desc.max_msg_len = QMI_IPA_INIT_MODEM_DRIVER_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INIT_MODEM_DRIVER_REQ_V01;
	req_desc.ei_array = ipa_init_modem_driver_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_INIT_MODEM_DRIVER_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INIT_MODEM_DRIVER_RESP_V01;
	resp_desc.ei_array = ipa_init_modem_driver_resp_msg_data_v01_ei;

	pr_info("Sending QMI_IPA_INIT_MODEM_DRIVER_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt, &req_desc, &req,
			&resp_desc, &resp,
			QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_INIT_MODEM_DRIVER_REQ_V01,
			rc);
		return rc;
	}

	pr_info("QMI_IPA_INIT_MODEM_DRIVER_REQ_V01 response received\n");
	return ipa_check_qmi_response(rc,
		QMI_IPA_INIT_MODEM_DRIVER_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_init_modem_driver_resp_msg_v01");
}

/* sending filter-install-request to modem*/
int qmi_filter_request_send(struct ipa_install_fltr_rule_req_msg_v01 *req)
{
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;
	int i;

	/* check if modem up */
	if (!qmi_indication_fin ||
		!qmi_modem_init_fin ||
		!ipa_q6_clnt) {
		IPAWANDBG("modem QMI haven't up yet\n");
		return -EINVAL;
	}

	/* check if the filter rules from IPACM is valid */
	if (req->filter_spec_list_len == 0) {
		IPAWANDBG("IPACM pass zero rules to Q6\n");
	} else {
		IPAWANDBG("IPACM pass %u rules to Q6\n",
		req->filter_spec_list_len);
	}

	if (req->filter_spec_list_len >= QMI_IPA_MAX_FILTERS_V01) {
		IPAWANDBG(
		"IPACM passes the number of filtering rules exceed limit\n");
		return -EINVAL;
	} else if (req->source_pipe_index_valid != 0) {
		IPAWANDBG(
		"IPACM passes source_pipe_index_valid not zero 0 != %d\n",
			req->source_pipe_index_valid);
		return -EINVAL;
	} else if (req->source_pipe_index >= ipa_ctx->ipa_num_pipes) {
		IPAWANDBG(
		"IPACM passes source pipe index not valid ID = %d\n",
		req->source_pipe_index);
		return -EINVAL;
	}
	for (i = 0; i < req->filter_spec_list_len; i++) {
		if ((req->filter_spec_list[i].ip_type !=
			QMI_IPA_IP_TYPE_V4_V01) &&
			(req->filter_spec_list[i].ip_type !=
			QMI_IPA_IP_TYPE_V6_V01))
			return -EINVAL;
		if (req->filter_spec_list[i].is_mux_id_valid == false)
			return -EINVAL;
		if (req->filter_spec_list[i].is_routing_table_index_valid
			== false)
			return -EINVAL;
		if ((req->filter_spec_list[i].filter_action <=
			QMI_IPA_FILTER_ACTION_INVALID_V01) &&
			(req->filter_spec_list[i].filter_action >
			QMI_IPA_FILTER_ACTION_EXCEPTION_V01))
			return -EINVAL;
	}
	mutex_lock(&ipa_qmi_lock);
	if (ipa_qmi_ctx != NULL) {
		/* cache the qmi_filter_request */
		memcpy(&(ipa_qmi_ctx->ipa_install_fltr_rule_req_msg_cache[
			ipa_qmi_ctx->num_ipa_install_fltr_rule_req_msg]),
			req,
			sizeof(struct ipa_install_fltr_rule_req_msg_v01));
		ipa_qmi_ctx->num_ipa_install_fltr_rule_req_msg++;
		ipa_qmi_ctx->num_ipa_install_fltr_rule_req_msg %= 10;
	}
	mutex_unlock(&ipa_qmi_lock);

	req_desc.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01;
	req_desc.ei_array = ipa_install_fltr_rule_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_RESP_V01;
	resp_desc.ei_array = ipa_install_fltr_rule_resp_msg_data_v01_ei;
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt, &req_desc,
			req,
			&resp_desc, &resp,
			QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_INSTALL_FILTER_RULE_REQ_V01,
			rc);
		return rc;
	}

	return ipa_check_qmi_response(rc,
		QMI_IPA_INSTALL_FILTER_RULE_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_install_filter");
}


int qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req)
{
	struct ipa_enable_force_clear_datapath_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;


	if (!req || !req->source_pipe_bitmask) {
		IPAWANERR("invalid params\n");
		return -EINVAL;
	}

	req_desc.max_msg_len =
	QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_V01;
	req_desc.ei_array = ipa_enable_force_clear_datapath_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_RESP_V01;
	resp_desc.ei_array =
		ipa_enable_force_clear_datapath_resp_msg_data_v01_ei;
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc,
		req,
		&resp_desc, &resp,
		QMI_IPA_FORCE_CLEAR_DATAPATH_TIMEOUT_MS);
	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
			QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_V01,
			rc);
		return rc;
	}
	if (resp.resp.result != IPA_QMI_RESULT_SUCCESS_V01) {
		IPAWANERR("filter_notify failed %d\n",
			resp.resp.result);
		return resp.resp.result;
	}
	IPAWANDBG("SUCCESS\n");
	return rc;
}

int qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req)
{
	struct ipa_disable_force_clear_datapath_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;


	if (!req) {
		IPAWANERR("invalid params\n");
		return -EINVAL;
	}

	req_desc.max_msg_len =
		QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_V01;
	req_desc.ei_array =
		ipa_disable_force_clear_datapath_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_RESP_V01;
	resp_desc.ei_array =
		ipa_disable_force_clear_datapath_resp_msg_data_v01_ei;
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc,
		req,
		&resp_desc, &resp,
		QMI_IPA_FORCE_CLEAR_DATAPATH_TIMEOUT_MS);
	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
			QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_V01,
			rc);
		return rc;
	}
	if (resp.resp.result != IPA_QMI_RESULT_SUCCESS_V01) {
		IPAWANERR("filter_notify failed %d\n",
			resp.resp.result);
		return resp.resp.result;
	}
	IPAWANDBG("SUCCESS\n");
	return rc;
}

/* sending filter-installed-notify-request to modem*/
int qmi_filter_notify_send(struct ipa_fltr_installed_notif_req_msg_v01 *req)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0, i = 0;

	/* check if the filter rules from IPACM is valid */
	if (req->filter_index_list_len == 0) {
		IPAWANDBG(" delete UL filter rule for pipe %d\n",
		req->source_pipe_index);
	} else if (req->filter_index_list_len > QMI_IPA_MAX_FILTERS_V01) {
		IPAWANERR(" UL filter rule for pipe %d exceed max (%u)\n",
		req->source_pipe_index,
		req->filter_index_list_len);
		return -EINVAL;
	} else if (req->filter_index_list[0].filter_index == 0 &&
		req->source_pipe_index !=
		ipa2_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD)) {
		IPAWANERR(" get index wrong for pipe %d\n",
			req->source_pipe_index);
		for (i = 0; i < req->filter_index_list_len; i++)
			IPAWANERR(" %d-st handle %d index %d\n",
				i,
				req->filter_index_list[i].filter_handle,
				req->filter_index_list[i].filter_index);
		return -EINVAL;
	}

	if (req->install_status != IPA_QMI_RESULT_SUCCESS_V01) {
		IPAWANERR(" UL filter rule for pipe %d install_status = %d\n",
			req->source_pipe_index, req->install_status);
		return -EINVAL;
	} else if (req->source_pipe_index >= ipa_ctx->ipa_num_pipes) {
		IPAWANERR("IPACM passes source pipe index not valid ID = %d\n",
		req->source_pipe_index);
		return -EINVAL;
	} else if (((req->embedded_pipe_index_valid != true) ||
			(req->embedded_call_mux_id_valid != true)) &&
			((req->embedded_pipe_index_valid != false) ||
			(req->embedded_call_mux_id_valid != false))) {
		IPAWANERR(
			"IPACM passes embedded pipe and mux valid not valid\n");
		return -EINVAL;
	} else if (req->embedded_pipe_index >= ipa_ctx->ipa_num_pipes) {
		IPAWANERR("IPACM passes source pipe index not valid ID = %d\n",
		req->source_pipe_index);
		return -EINVAL;
	}

	mutex_lock(&ipa_qmi_lock);
	if (ipa_qmi_ctx != NULL) {
		/* cache the qmi_filter_request */
		memcpy(&(ipa_qmi_ctx->ipa_fltr_installed_notif_req_msg_cache[
			ipa_qmi_ctx->num_ipa_fltr_installed_notif_req_msg]),
			req,
			sizeof(struct ipa_fltr_installed_notif_req_msg_v01));
		ipa_qmi_ctx->num_ipa_fltr_installed_notif_req_msg++;
		ipa_qmi_ctx->num_ipa_fltr_installed_notif_req_msg %= 10;
	}
	mutex_unlock(&ipa_qmi_lock);
	req_desc.max_msg_len =
	QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01;
	req_desc.ei_array = ipa_fltr_installed_notif_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01;
	resp_desc.ei_array = ipa_fltr_installed_notif_resp_msg_data_v01_ei;
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc,
		req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
		QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01,
		rc);
		return rc;
	}

	return ipa_check_qmi_response(rc,
		QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_fltr_installed_notif_resp");
}

static void ipa_q6_clnt_quota_reached_ind_cb(struct qmi_handle *handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *data)
{
	struct ipa_data_usage_quota_reached_ind_msg_v01 *qmi_ind;

	if (handle != ipa_q6_clnt) {
		IPAWANERR("Wrong client\n");
		return;
	}

	qmi_ind = (struct ipa_data_usage_quota_reached_ind_msg_v01 *) data;

	IPAWANDBG("Quota reached indication on qmux(%d) Mbytes(%lu)\n",
		qmi_ind->apn.mux_id,
		(unsigned long) qmi_ind->apn.num_Mbytes);
	ipa_broadcast_quota_reach_ind(qmi_ind->apn.mux_id,
		IPA_UPSTEAM_MODEM);
}

static void ipa_q6_clnt_svc_arrive(struct work_struct *work)
{
	int rc;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;

	rc = kernel_connect(ipa_q6_clnt->sock,
		(struct sockaddr *) &ipa_qmi_ctx->server_sq,
		sizeof(ipa_qmi_ctx->server_sq),
		0);

	if (rc < 0) {
		IPAWANERR("Couldnt connect Server\n");
		return;
	}

	IPAWANDBG("Q6 QMI service available now\n");
	/* Initialize modem IPA-driver */
	IPAWANDBG("send qmi_init_modem_send_sync_msg to modem\n");
	rc = qmi_init_modem_send_sync_msg();
	if ((rc == -ENETRESET) || (rc == -ENODEV)) {
		IPAWANERR("qmi_init_modem_send_sync_msg failed due to SSR!\n");
		/* Cleanup will take place when ipa_wwan_remove is called */
		vfree(ipa_q6_clnt);
		ipa_q6_clnt = NULL;
		return;
	}
	if (rc != 0) {
		IPAWANERR("qmi_init_modem_send_sync_msg failed\n");
		/*
		 * This is a very unexpected scenario, which requires a kernel
		 * panic in order to force dumps for QMI/Q6 side analysis.
		 */
		ipa_assert();
		return;
	}
	qmi_modem_init_fin = true;

	/* In cold-bootup, first_time_handshake = false */
	ipa_q6_handshake_complete(first_time_handshake);
	first_time_handshake = true;

	IPAWANDBG("complete, qmi_modem_init_fin : %d\n",
		qmi_modem_init_fin);

	if (qmi_indication_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		qmi_indication_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;
		rc = qmi_send_indication(ipa_svc_handle,
			&ipa_qmi_ctx->client_sq,
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01,
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01,
			ipa_master_driver_init_complt_ind_msg_data_v01_ei,
			&ind);

		IPAWANDBG("ipa_qmi_service_client good\n");
	} else {
		IPAWANERR("not send indication (%d)\n",
		qmi_indication_fin);
	}
}


static void ipa_q6_clnt_svc_exit(struct work_struct *work)
{

	if (ipa_qmi_ctx != NULL) {
		ipa_qmi_ctx->server_sq.sq_family = 0;
		ipa_qmi_ctx->server_sq.sq_node = 0;
		ipa_qmi_ctx->server_sq.sq_port = 0;
	}
}

static int ipa_q6_clnt_svc_event_notify_svc_new(struct qmi_handle *qmi,
	struct qmi_service *service)
{
	IPAWANDBG("QMI svc:%d vers:%d ins:%d node:%d port:%d\n",
		  service->service, service->version, service->instance,
		  service->node, service->port);

	if (ipa_qmi_ctx != NULL) {
		ipa_qmi_ctx->server_sq.sq_family = AF_QIPCRTR;
		ipa_qmi_ctx->server_sq.sq_node = service->node;
		ipa_qmi_ctx->server_sq.sq_port = service->port;
	}
	if (!atomic_read(&workqueues_stopped)) {
		queue_delayed_work(ipa_clnt_req_workqueue,
			&work_svc_arrive, 0);
	}
	return 0;
}

static void ipa_q6_clnt_svc_event_notify_net_reset(struct qmi_handle *qmi)
{
	if (!atomic_read(&workqueues_stopped))
		queue_delayed_work(ipa_clnt_req_workqueue,
			&work_svc_exit, 0);
}

static void ipa_q6_clnt_svc_event_notify_svc_exit(struct qmi_handle *qmi,
						struct qmi_service *svc)
{
	IPAWANDBG("QMI svc:%d vers:%d ins:%d node:%d port:%d\n", svc->service,
		svc->version, svc->instance, svc->node, svc->port);

	if (!atomic_read(&workqueues_stopped))
		queue_delayed_work(ipa_clnt_req_workqueue,
			&work_svc_exit, 0);
}

static struct qmi_ops server_ops = {
	.del_client = ipa_a5_svc_disconnect_cb,
};

static struct qmi_ops client_ops = {
	.new_server = ipa_q6_clnt_svc_event_notify_svc_new,
	.del_server = ipa_q6_clnt_svc_event_notify_svc_exit,
	.net_reset = ipa_q6_clnt_svc_event_notify_net_reset,
};

static struct qmi_msg_handler server_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_INDICATION_REGISTER_REQ_V01,
		.ei = ipa_indication_reg_req_msg_data_v01_ei,
		.decoded_size = sizeof(struct ipa_indication_reg_req_msg_v01),
		.fn = handle_indication_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01,
		.ei = ipa_install_fltr_rule_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_install_fltr_rule_req_msg_v01),
		.fn = handle_install_filter_rule_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01,
		.ei = ipa_fltr_installed_notif_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_fltr_installed_notif_req_msg_v01),
		.fn = handle_filter_installed_notify_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_CONFIG_REQ_V01,
		.ei = ipa_config_req_msg_data_v01_ei,
		.decoded_size = sizeof(struct ipa_config_req_msg_v01),
		.fn = handle_ipa_config_req,
	},
};

static struct qmi_msg_handler client_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_V01,
		.ei = ipa_data_usage_quota_reached_ind_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_data_usage_quota_reached_ind_msg_v01),
		.fn = ipa_q6_clnt_quota_reached_ind_cb,
	},
};

static void ipa_qmi_service_init_worker(void)
{
	int rc;

	/* Initialize QMI-service*/
	IPAWANDBG("IPA A7 QMI init OK :>>>>\n");

	/* start the QMI msg cache */
	ipa_qmi_ctx = vzalloc(sizeof(*ipa_qmi_ctx));
	if (!ipa_qmi_ctx) {
		IPAWANERR(":kzalloc err.\n");
		return;
	}
	ipa_qmi_ctx->modem_cfg_emb_pipe_flt =
		ipa2_get_modem_cfg_emb_pipe_flt();

	ipa_svc_handle = vzalloc(sizeof(*ipa_svc_handle));
	if (!ipa_svc_handle)
		goto destroy_ipa_A7_svc_wq;

	 rc = qmi_handle_init(ipa_svc_handle,
		QMI_IPA_MAX_MSG_LEN,
		&server_ops,
		server_handlers);

	if (rc < 0) {
		IPAWANERR("Initializing ipa_a5 svc failed %d\n", rc);
		goto destroy_qmi_handle;
	}

	rc = qmi_add_server(ipa_svc_handle,
		IPA_A5_SERVICE_SVC_ID,
		IPA_A5_SVC_VERS,
		IPA_A5_SERVICE_INS_ID);

	if (rc < 0) {
		IPAWANERR("Registering ipa_a5 svc failed %d\n",
				rc);
		goto deregister_qmi_srv;
	}
	/* Initialize QMI-client */

	ipa_clnt_req_workqueue = create_singlethread_workqueue("clnt_req");
	if (!ipa_clnt_req_workqueue) {
		IPAWANERR("Creating clnt_req workqueue failed\n");
		goto deregister_qmi_srv;
	}

	/* Create a Local client port for QMI communication */
	ipa_q6_clnt = vzalloc(sizeof(*ipa_q6_clnt));

	if (!ipa_q6_clnt)
		goto destroy_clnt_req_wq;

	rc = qmi_handle_init(ipa_q6_clnt,
		QMI_IPA_MAX_MSG_LEN,
		&client_ops,
		client_handlers);

	if (rc < 0) {
		IPAWANERR("Creating clnt handle failed\n");
		goto destroy_qmi_client_handle;
	}

	rc = qmi_add_lookup(ipa_q6_clnt,
		IPA_Q6_SERVICE_SVC_ID,
		IPA_Q6_SVC_VERS,
		IPA_Q6_SERVICE_INS_ID);

	if (rc < 0) {
		IPAWANERR("Adding Q6 Svc failed\n");
		goto deregister_qmi_client;
	}
	/* get Q6 service and start send modem-initial to Q6 */
	IPAWANDBG("wait service available\n");
	return;

deregister_qmi_client:
	qmi_handle_release(ipa_q6_clnt);
destroy_qmi_client_handle:
	vfree(ipa_q6_clnt);
	ipa_q6_clnt = NULL;
destroy_clnt_req_wq:
	destroy_workqueue(ipa_clnt_req_workqueue);
	ipa_clnt_req_workqueue = NULL;
deregister_qmi_srv:
	qmi_handle_release(ipa_svc_handle);
destroy_qmi_handle:
	vfree(ipa_qmi_ctx);
destroy_ipa_A7_svc_wq:
	 vfree(ipa_svc_handle);
	ipa_svc_handle = NULL;
	ipa_qmi_ctx = NULL;
}

int ipa_qmi_service_init(uint32_t wan_platform_type)
{
	ipa_wan_platform = wan_platform_type;
	qmi_modem_init_fin = false;
	qmi_indication_fin = false;
	atomic_set(&workqueues_stopped, 0);

	if (atomic_read(&ipa_qmi_initialized) == 0)
		ipa_qmi_service_init_worker();
	return 0;
}

void ipa_qmi_service_exit(void)
{

	atomic_set(&workqueues_stopped, 1);

	/* qmi-service */
	if (ipa_svc_handle != NULL) {
		qmi_handle_release(ipa_svc_handle);
		vfree(ipa_svc_handle);
		ipa_svc_handle = NULL;
	}

	/* qmi-client */


	/* Release client handle */
	if (ipa_q6_clnt != NULL) {
		qmi_handle_release(ipa_q6_clnt);
		vfree(ipa_q6_clnt);
		ipa_q6_clnt = NULL;
		if (ipa_clnt_req_workqueue) {
			destroy_workqueue(ipa_clnt_req_workqueue);
			ipa_clnt_req_workqueue = NULL;
		}
	}

	/* clean the QMI msg cache */
	mutex_lock(&ipa_qmi_lock);
	if (ipa_qmi_ctx != NULL) {
		vfree(ipa_qmi_ctx);
		ipa_qmi_ctx = NULL;
	}
	mutex_unlock(&ipa_qmi_lock);
	qmi_modem_init_fin = false;
	qmi_indication_fin = false;
	atomic_set(&ipa_qmi_initialized, 0);
}

void ipa_qmi_stop_workqueues(void)
{
	IPAWANDBG("Stopping all QMI workqueues\n");

	/* Stopping all workqueues so new work won't be scheduled */
	atomic_set(&workqueues_stopped, 1);

	/* Making sure that the current scheduled work won't be executed */
	cancel_delayed_work(&work_svc_arrive);
	cancel_delayed_work(&work_svc_exit);
}

/* voting for bus BW to ipa_rm*/
int vote_for_bus_bw(uint32_t *bw_mbps)
{
	struct ipa_rm_perf_profile profile;
	int ret;

	if (bw_mbps == NULL) {
		IPAWANERR("Bus BW is invalid\n");
		return -EINVAL;
	}

	memset(&profile, 0, sizeof(profile));
	profile.max_supported_bandwidth_mbps = *bw_mbps;
	ret = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_Q6_PROD,
			&profile);
	if (ret)
		IPAWANERR("Failed to set perf profile to BW %u\n",
			profile.max_supported_bandwidth_mbps);
	else
		IPAWANDBG("Succeeded to set perf profile to BW %u\n",
			profile.max_supported_bandwidth_mbps);

	return ret;
}

int ipa_qmi_get_data_stats(struct ipa_get_data_stats_req_msg_v01 *req,
			   struct ipa_get_data_stats_resp_msg_v01 *resp)
{
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_DATA_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_DATA_STATS_REQ_V01;
	req_desc.ei_array = ipa_get_data_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_GET_DATA_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_DATA_STATS_RESP_V01;
	resp_desc.ei_array = ipa_get_data_stats_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_GET_DATA_STATS_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
		&resp_desc, resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_GET_DATA_STATS_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_GET_DATA_STATS_RESP_V01 received\n");

	return ipa_check_qmi_response(rc,
		QMI_IPA_GET_DATA_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa_get_data_stats_resp_msg_v01");
}

int ipa_qmi_get_network_stats(struct ipa_get_apn_data_stats_req_msg_v01 *req,
			      struct ipa_get_apn_data_stats_resp_msg_v01 *resp)
{
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_APN_DATA_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_APN_DATA_STATS_REQ_V01;
	req_desc.ei_array = ipa_get_apn_data_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_GET_APN_DATA_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_APN_DATA_STATS_RESP_V01;
	resp_desc.ei_array = ipa_get_apn_data_stats_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_GET_APN_DATA_STATS_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
		&resp_desc, resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_GET_APN_DATA_STATS_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_GET_APN_DATA_STATS_RESP_V01 received\n");

	return ipa_check_qmi_response(rc,
		QMI_IPA_GET_APN_DATA_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa_get_apn_data_stats_req_msg_v01");
}

int ipa_qmi_set_data_quota(struct ipa_set_data_usage_quota_req_msg_v01 *req)
{
	struct ipa_set_data_usage_quota_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	memset(&resp, 0, sizeof(struct ipa_set_data_usage_quota_resp_msg_v01));

	req_desc.max_msg_len = QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01;
	req_desc.ei_array = ipa_set_data_usage_quota_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_V01;
	resp_desc.ei_array = ipa_set_data_usage_quota_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_V01 received\n");

	return ipa_check_qmi_response(rc,
		QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_set_data_usage_quota_req_msg_v01");
}

int ipa_qmi_stop_data_qouta(void)
{
	struct ipa_stop_data_usage_quota_req_msg_v01 req;
	struct ipa_stop_data_usage_quota_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	memset(&req, 0, sizeof(struct ipa_stop_data_usage_quota_req_msg_v01));
	memset(&resp, 0, sizeof(struct ipa_stop_data_usage_quota_resp_msg_v01));

	req_desc.max_msg_len =
		QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01;
	req_desc.ei_array = ipa_stop_data_usage_quota_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01;
	resp_desc.ei_array = ipa_stop_data_usage_quota_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa_qmi_send_req_wait(ipa_q6_clnt, &req_desc, &req,
		&resp_desc, &resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01 received\n");

	return ipa_check_qmi_response(rc,
		QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_stop_data_usage_quota_req_msg_v01");
}

void ipa_qmi_init(void)
{
	mutex_init(&ipa_qmi_lock);
}

void ipa_qmi_cleanup(void)
{
	mutex_destroy(&ipa_qmi_lock);
}
