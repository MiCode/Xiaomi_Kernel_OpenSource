/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/qmi_encdec.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/ipa.h>
#include <linux/vmalloc.h>

#include "ipa_qmi_service.h"
#include "ipa_ram_mmap.h"

#define IPA_Q6_SVC_VERS 1
#define IPA_A5_SVC_VERS 1
#define Q6_QMI_COMPLETION_TIMEOUT (60*HZ)

#define IPA_A5_SERVICE_SVC_ID 0x31
#define IPA_A5_SERVICE_INS_ID 1
#define IPA_Q6_SERVICE_SVC_ID 0x31
#define IPA_Q6_SERVICE_INS_ID 2

#define QMI_SEND_STATS_REQ_TIMEOUT_MS 5000
#define QMI_SEND_REQ_TIMEOUT_MS 60000

static struct qmi_handle *ipa3_svc_handle;
static void ipa3_a5_svc_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, ipa3_a5_svc_recv_msg);
static struct workqueue_struct *ipa_svc_workqueue;
static struct workqueue_struct *ipa_clnt_req_workqueue;
static struct workqueue_struct *ipa_clnt_resp_workqueue;
static void *curr_conn;
static bool ipa3_qmi_modem_init_fin, ipa3_qmi_indication_fin;
static struct work_struct ipa3_qmi_service_init_work;
static uint32_t ipa_wan_platform;
struct ipa3_qmi_context *ipa3_qmi_ctx;
static bool workqueues_stopped;
static bool ipa3_modem_init_cmplt;
static bool first_time_handshake;
struct mutex ipa3_qmi_lock;

/* QMI A5 service */

static struct msg_desc ipa3_indication_reg_req_desc = {
	.max_msg_len = QMI_IPA_INDICATION_REGISTER_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INDICATION_REGISTER_REQ_V01,
	.ei_array = ipa3_indication_reg_req_msg_data_v01_ei,
};
static struct msg_desc ipa3_indication_reg_resp_desc = {
	.max_msg_len = QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INDICATION_REGISTER_RESP_V01,
	.ei_array = ipa3_indication_reg_resp_msg_data_v01_ei,
};
static struct msg_desc ipa3_master_driver_complete_indication_desc = {
	.max_msg_len = QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01,
	.ei_array = ipa3_master_driver_init_complt_ind_msg_data_v01_ei,
};
static struct msg_desc ipa3_install_fltr_rule_req_desc = {
	.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01,
	.ei_array = ipa3_install_fltr_rule_req_msg_data_v01_ei,
};
static struct msg_desc ipa3_install_fltr_rule_resp_desc = {
	.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INSTALL_FILTER_RULE_RESP_V01,
	.ei_array = ipa3_install_fltr_rule_resp_msg_data_v01_ei,
};
static struct msg_desc ipa3_filter_installed_notif_req_desc = {
	.max_msg_len = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01,
	.ei_array = ipa3_fltr_installed_notif_req_msg_data_v01_ei,
};
static struct msg_desc ipa3_filter_installed_notif_resp_desc = {
	.max_msg_len = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01,
	.ei_array = ipa3_fltr_installed_notif_resp_msg_data_v01_ei,
};
static struct msg_desc ipa3_config_req_desc = {
	.max_msg_len = QMI_IPA_CONFIG_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_CONFIG_REQ_V01,
	.ei_array = ipa3_config_req_msg_data_v01_ei,
};
static struct msg_desc ipa3_config_resp_desc = {
	.max_msg_len = QMI_IPA_CONFIG_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_CONFIG_RESP_V01,
	.ei_array = ipa3_config_resp_msg_data_v01_ei,
};

static struct msg_desc ipa3_init_modem_driver_cmplt_req_desc = {
	.max_msg_len = QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01,
	.ei_array = ipa3_init_modem_driver_cmplt_req_msg_data_v01_ei,
};

static struct msg_desc ipa3_init_modem_driver_cmplt_resp_desc = {
	.max_msg_len = QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_V01,
	.ei_array = ipa3_init_modem_driver_cmplt_resp_msg_data_v01_ei,
};

static struct msg_desc ipa3_install_fltr_rule_req_ex_desc = {
	.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01,
	.ei_array = ipa3_install_fltr_rule_req_ex_msg_data_v01_ei,
};

static int ipa3_handle_indication_req(void *req_h, void *req)
{
	struct ipa_indication_reg_req_msg_v01 *indication_req;
	struct ipa_indication_reg_resp_msg_v01 resp;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;
	int rc;

	indication_req = (struct ipa_indication_reg_req_msg_v01 *)req;
	IPAWANDBG("Received INDICATION Request\n");

	memset(&resp, 0, sizeof(struct ipa_indication_reg_resp_msg_v01));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	rc = qmi_send_resp_from_cb(ipa3_svc_handle, curr_conn, req_h,
			&ipa3_indication_reg_resp_desc, &resp, sizeof(resp));
	ipa3_qmi_indication_fin = true;
	/* check if need sending indication to modem */
	if (ipa3_qmi_modem_init_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		ipa3_qmi_modem_init_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;
		rc = qmi_send_ind_from_cb(ipa3_svc_handle, curr_conn,
			&ipa3_master_driver_complete_indication_desc,
			&ind,
			sizeof(ind));
	} else {
		IPAWANERR("not send indication\n");
	}
	return rc;
}


static int ipa3_handle_install_filter_rule_req(void *req_h, void *req)
{
	struct ipa_install_fltr_rule_req_msg_v01 *rule_req;
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	uint32_t rule_hdl[MAX_NUM_Q6_RULE];
	int rc = 0, i;

	rule_req = (struct ipa_install_fltr_rule_req_msg_v01 *)req;
	memset(rule_hdl, 0, sizeof(rule_hdl));
	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_msg_v01));
	IPAWANDBG("Received install filter Request\n");

	rc = ipa3_copy_ul_filter_rule_to_ipa((struct
		ipa_install_fltr_rule_req_msg_v01*)req);
	if (rc)
		IPAWANERR("copy UL rules from modem is failed\n");

	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	if (rule_req->filter_spec_ex_list_valid == true) {
		resp.rule_id_valid = 1;
		if (rule_req->filter_spec_ex_list_len > MAX_NUM_Q6_RULE) {
			resp.rule_id_len = MAX_NUM_Q6_RULE;
			IPAWANERR("installed (%d) max Q6-UL rules ",
			MAX_NUM_Q6_RULE);
			IPAWANERR("but modem gives total (%u)\n",
			rule_req->filter_spec_ex_list_len);
		} else {
			resp.rule_id_len =
				rule_req->filter_spec_ex_list_len;
		}
	} else {
		resp.rule_id_valid = 0;
		resp.rule_id_len = 0;
	}

	/* construct UL filter rules response to Modem*/
	for (i = 0; i < resp.rule_id_len; i++) {
		resp.rule_id[i] =
			rule_req->filter_spec_ex_list[i].rule_id;
	}

	rc = qmi_send_resp_from_cb(ipa3_svc_handle, curr_conn, req_h,
			&ipa3_install_fltr_rule_resp_desc, &resp, sizeof(resp));

	IPAWANDBG("Replied to install filter request\n");
	return rc;
}

static int ipa3_handle_filter_installed_notify_req(void *req_h, void *req)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	int rc = 0;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	IPAWANDBG("Received filter_install_notify Request\n");
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;

	rc = qmi_send_resp_from_cb(ipa3_svc_handle, curr_conn, req_h,
			&ipa3_filter_installed_notif_resp_desc,
			&resp, sizeof(resp));

	IPAWANDBG("Responsed filter_install_notify Request\n");
	return rc;
}

static int handle_ipa_config_req(void *req_h, void *req)
{
	struct ipa_config_resp_msg_v01 resp;
	int rc;

	memset(&resp, 0, sizeof(struct ipa_config_resp_msg_v01));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	IPAWANDBG("Received IPA CONFIG Request\n");
	rc = ipa_mhi_handle_ipa_config_req(
		(struct ipa_config_req_msg_v01 *)req);
	if (rc) {
		IPAERR("ipa3_mhi_handle_ipa_config_req failed %d\n", rc);
		resp.resp.result = IPA_QMI_RESULT_FAILURE_V01;
	}
	rc = qmi_send_resp_from_cb(ipa3_svc_handle, curr_conn, req_h,
		&ipa3_config_resp_desc,
		&resp, sizeof(resp));
	IPAWANDBG("Responsed IPA CONFIG Request\n");
	return rc;
}

static int ipa3_handle_modem_init_cmplt_req(void *req_h, void *req)
{
	struct ipa_init_modem_driver_cmplt_req_msg_v01 *cmplt_req;
	struct ipa_init_modem_driver_cmplt_resp_msg_v01 resp;
	int rc;

	IPAWANDBG("Received QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01\n");
	cmplt_req = (struct ipa_init_modem_driver_cmplt_req_msg_v01 *)req;

	if (ipa3_modem_init_cmplt == false) {
		ipa3_modem_init_cmplt = true;
		if (ipa3_qmi_modem_init_fin == true) {
			IPAWANDBG("load uc related registers (%d)\n",
			ipa3_qmi_modem_init_fin);
			ipa3_uc_load_notify();
		}
	}

	memset(&resp, 0, sizeof(resp));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;

	rc = qmi_send_resp_from_cb(ipa3_svc_handle, curr_conn, req_h,
			&ipa3_init_modem_driver_cmplt_resp_desc,
			&resp, sizeof(resp));

	IPAWANDBG("Sent QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_V01\n");
	return rc;
}

static int ipa3_a5_svc_connect_cb(struct qmi_handle *handle,
			       void *conn_h)
{
	if (ipa3_svc_handle != handle || !conn_h)
		return -EINVAL;

	if (curr_conn) {
		IPAWANERR("Service is busy\n");
		return -ECONNREFUSED;
	}
	curr_conn = conn_h;
	return 0;
}

static int ipa3_a5_svc_disconnect_cb(struct qmi_handle *handle,
				  void *conn_h)
{
	if (ipa3_svc_handle != handle || curr_conn != conn_h)
		return -EINVAL;

	curr_conn = NULL;
	return 0;
}

static int ipa3_a5_svc_req_desc_cb(unsigned int msg_id,
				struct msg_desc **req_desc)
{
	int rc;

	switch (msg_id) {
	case QMI_IPA_INDICATION_REGISTER_REQ_V01:
		*req_desc = &ipa3_indication_reg_req_desc;
		rc = sizeof(struct ipa_indication_reg_req_msg_v01);
		break;

	case QMI_IPA_INSTALL_FILTER_RULE_REQ_V01:
		*req_desc = &ipa3_install_fltr_rule_req_desc;
		rc = sizeof(struct ipa_install_fltr_rule_req_msg_v01);
		break;
	case QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01:
		*req_desc = &ipa3_install_fltr_rule_req_ex_desc;
		rc = sizeof(struct ipa_install_fltr_rule_req_ex_msg_v01);
		break;
	case QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01:
		*req_desc = &ipa3_filter_installed_notif_req_desc;
		rc = sizeof(struct ipa_fltr_installed_notif_req_msg_v01);
		break;
	case QMI_IPA_CONFIG_REQ_V01:
		*req_desc = &ipa3_config_req_desc;
		rc = sizeof(struct ipa_config_req_msg_v01);
		break;
	case QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01:
		*req_desc = &ipa3_init_modem_driver_cmplt_req_desc;
		rc = sizeof(struct ipa_init_modem_driver_cmplt_req_msg_v01);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static int ipa3_a5_svc_req_cb(struct qmi_handle *handle, void *conn_h,
			void *req_h, unsigned int msg_id, void *req)
{
	int rc;

	if (ipa3_svc_handle != handle || curr_conn != conn_h)
		return -EINVAL;

	switch (msg_id) {
	case QMI_IPA_INDICATION_REGISTER_REQ_V01:
		rc = ipa3_handle_indication_req(req_h, req);
		break;
	case QMI_IPA_INSTALL_FILTER_RULE_REQ_V01:
		rc = ipa3_handle_install_filter_rule_req(req_h, req);
		rc = ipa3_wwan_update_mux_channel_prop();
		break;
	case QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01:
		rc = ipa3_handle_filter_installed_notify_req(req_h, req);
		break;
	case QMI_IPA_CONFIG_REQ_V01:
		rc = handle_ipa_config_req(req_h, req);
		break;
	case QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01:
		rc = ipa3_handle_modem_init_cmplt_req(req_h, req);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static void ipa3_a5_svc_recv_msg(struct work_struct *work)
{
	int rc;

	do {
		IPAWANDBG_LOW("Notified about a Receive Event");
		rc = qmi_recv_msg(ipa3_svc_handle);
	} while (rc == 0);
	if (rc != -ENOMSG)
		IPAWANERR("Error receiving message\n");
}

static void qmi_ipa_a5_svc_ntfy(struct qmi_handle *handle,
		enum qmi_event_type event, void *priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		if (!workqueues_stopped)
			queue_delayed_work(ipa_svc_workqueue,
					   &work_recv_msg, 0);
		break;
	default:
		break;
	}
}

static struct qmi_svc_ops_options ipa3_a5_svc_ops_options = {
	.version = 1,
	.service_id = IPA_A5_SERVICE_SVC_ID,
	.service_vers = IPA_A5_SVC_VERS,
	.service_ins = IPA_A5_SERVICE_INS_ID,
	.connect_cb = ipa3_a5_svc_connect_cb,
	.disconnect_cb = ipa3_a5_svc_disconnect_cb,
	.req_desc_cb = ipa3_a5_svc_req_desc_cb,
	.req_cb = ipa3_a5_svc_req_cb,
};


/****************************************************/
/*                 QMI A5 client ->Q6               */
/****************************************************/
static void ipa3_q6_clnt_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_work_recv_msg_client, ipa3_q6_clnt_recv_msg);
static void ipa3_q6_clnt_svc_arrive(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_work_svc_arrive, ipa3_q6_clnt_svc_arrive);
static void ipa3_q6_clnt_svc_exit(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_work_svc_exit, ipa3_q6_clnt_svc_exit);
/* Test client port for IPC Router */
static struct qmi_handle *ipa_q6_clnt;
static int ipa_q6_clnt_reset;

static int ipa3_check_qmi_response(int rc,
				  int req_id,
				  enum ipa_qmi_result_type_v01 result,
				  enum ipa_qmi_error_type_v01 error,
				  char *resp_type)
{
	if (rc < 0) {
		if (rc == -ETIMEDOUT && ipa3_rmnet_ctx.ipa_rmnet_ssr) {
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
	    ipa3_rmnet_ctx.ipa_rmnet_ssr) {
		IPAWANERR(
		"Got bad response %d from request id %d (error %d)\n",
		req_id, result, error);
		return result;
	}
	IPAWANDBG_LOW("Received %s successfully\n", resp_type);
	return 0;
}

static int ipa3_qmi_init_modem_send_sync_msg(void)
{
	struct ipa_init_modem_driver_req_msg_v01 req;
	struct ipa_init_modem_driver_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;
	u16 smem_restr_bytes = ipa3_get_smem_restr_bytes();

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
	req.v4_route_tbl_info.route_tbl_start_addr =
		IPA_MEM_PART(v4_rt_nhash_ofst) + smem_restr_bytes;
	req.v4_route_tbl_info.num_indices =
		IPA_MEM_PART(v4_modem_rt_index_hi);
	req.v6_route_tbl_info_valid = true;

	req.v6_route_tbl_info.route_tbl_start_addr =
		IPA_MEM_PART(v6_rt_nhash_ofst) + smem_restr_bytes;
	req.v6_route_tbl_info.num_indices =
		IPA_MEM_PART(v6_modem_rt_index_hi);

	req.v4_filter_tbl_start_addr_valid = true;
	req.v4_filter_tbl_start_addr =
		IPA_MEM_PART(v4_flt_nhash_ofst) + smem_restr_bytes;

	req.v6_filter_tbl_start_addr_valid = true;
	req.v6_filter_tbl_start_addr =
		IPA_MEM_PART(v6_flt_nhash_ofst) + smem_restr_bytes;

	req.modem_mem_info_valid = (IPA_MEM_PART(modem_size) != 0);
	req.modem_mem_info.block_start_addr =
		IPA_MEM_PART(modem_ofst) + smem_restr_bytes;
	req.modem_mem_info.size = IPA_MEM_PART(modem_size);

	req.ctrl_comm_dest_end_pt_valid = true;
	req.ctrl_comm_dest_end_pt =
		ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);

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

	req.v4_hash_route_tbl_info_valid = true;
	req.v4_hash_route_tbl_info.route_tbl_start_addr =
		IPA_MEM_PART(v4_rt_hash_ofst) + smem_restr_bytes;
	req.v4_hash_route_tbl_info.num_indices =
		IPA_MEM_PART(v4_modem_rt_index_hi);

	req.v6_hash_route_tbl_info_valid = true;
	req.v6_hash_route_tbl_info.route_tbl_start_addr =
		IPA_MEM_PART(v6_rt_hash_ofst) + smem_restr_bytes;
	req.v6_hash_route_tbl_info.num_indices =
		IPA_MEM_PART(v6_modem_rt_index_hi);

	req.v4_hash_filter_tbl_start_addr_valid = true;
	req.v4_hash_filter_tbl_start_addr =
		IPA_MEM_PART(v4_flt_hash_ofst) + smem_restr_bytes;

	req.v6_hash_filter_tbl_start_addr_valid = true;
	req.v6_hash_filter_tbl_start_addr =
		IPA_MEM_PART(v6_flt_hash_ofst) + smem_restr_bytes;

	if (!ipa3_uc_loaded_check()) {  /* First time boot */
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
	IPAWANDBG("v4_hash_route_tbl_info.route_tbl_start_addr %d\n",
		req.v4_hash_route_tbl_info.route_tbl_start_addr);
	IPAWANDBG("v4_hash_route_tbl_info.num_indices %d\n",
		req.v4_hash_route_tbl_info.num_indices);
	IPAWANDBG("v6_hash_route_tbl_info.route_tbl_start_addr %d\n",
		req.v6_hash_route_tbl_info.route_tbl_start_addr);
	IPAWANDBG("v6_hash_route_tbl_info.num_indices %d\n",
		req.v6_hash_route_tbl_info.num_indices);
	IPAWANDBG("v4_hash_filter_tbl_start_addr %d\n",
		req.v4_hash_filter_tbl_start_addr);
	IPAWANDBG("v6_hash_filter_tbl_start_addr %d\n",
		req.v6_hash_filter_tbl_start_addr);

	req_desc.max_msg_len = QMI_IPA_INIT_MODEM_DRIVER_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INIT_MODEM_DRIVER_REQ_V01;
	req_desc.ei_array = ipa3_init_modem_driver_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_INIT_MODEM_DRIVER_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INIT_MODEM_DRIVER_RESP_V01;
	resp_desc.ei_array = ipa3_init_modem_driver_resp_msg_data_v01_ei;

	pr_info("Sending QMI_IPA_INIT_MODEM_DRIVER_REQ_V01\n");
	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	pr_info("QMI_IPA_INIT_MODEM_DRIVER_REQ_V01 response received\n");
	return ipa3_check_qmi_response(rc,
		QMI_IPA_INIT_MODEM_DRIVER_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_init_modem_driver_resp_msg_v01");
}

/* sending filter-install-request to modem*/
int ipa3_qmi_filter_request_send(struct ipa_install_fltr_rule_req_msg_v01 *req)
{
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;
	int i;

	/* check if modem up */
	if (!ipa3_qmi_indication_fin ||
		!ipa3_qmi_modem_init_fin ||
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
	} else if (req->source_pipe_index >= ipa3_ctx->ipa_num_pipes) {
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

	mutex_lock(&ipa3_qmi_lock);
	if (ipa3_qmi_ctx != NULL) {
		/* cache the qmi_filter_request */
		memcpy(&(ipa3_qmi_ctx->ipa_install_fltr_rule_req_msg_cache[
			ipa3_qmi_ctx->num_ipa_install_fltr_rule_req_msg]),
			req,
			sizeof(struct ipa_install_fltr_rule_req_msg_v01));
		ipa3_qmi_ctx->num_ipa_install_fltr_rule_req_msg++;
		ipa3_qmi_ctx->num_ipa_install_fltr_rule_req_msg %= 10;
	}
	mutex_unlock(&ipa3_qmi_lock);

	req_desc.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01;
	req_desc.ei_array = ipa3_install_fltr_rule_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_RESP_V01;
	resp_desc.ei_array = ipa3_install_fltr_rule_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc,
			req,
			sizeof(struct ipa_install_fltr_rule_req_msg_v01),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	return ipa3_check_qmi_response(rc,
		QMI_IPA_INSTALL_FILTER_RULE_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_install_filter");
}

/* sending filter-install-request to modem*/
int ipa3_qmi_filter_request_ex_send(
	struct ipa_install_fltr_rule_req_ex_msg_v01 *req)
{
	struct ipa_install_fltr_rule_resp_ex_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;
	int i;

	/* check if modem up */
	if (!ipa3_qmi_indication_fin ||
		!ipa3_qmi_modem_init_fin ||
		!ipa_q6_clnt) {
		IPAWANDBG("modem QMI haven't up yet\n");
		return -EINVAL;
	}

	/* check if the filter rules from IPACM is valid */
	if (req->filter_spec_ex_list_len == 0) {
		IPAWANDBG("IPACM pass zero rules to Q6\n");
	} else {
		IPAWANDBG("IPACM pass %u rules to Q6\n",
		req->filter_spec_ex_list_len);
	}

	if (req->filter_spec_ex_list_len >= QMI_IPA_MAX_FILTERS_EX_V01) {
		IPAWANDBG(
		"IPACM pass the number of filtering rules exceed limit\n");
		return -EINVAL;
	} else if (req->source_pipe_index_valid != 0) {
		IPAWANDBG(
		"IPACM passes source_pipe_index_valid not zero 0 != %d\n",
			req->source_pipe_index_valid);
		return -EINVAL;
	}

	for (i = 0; i < req->filter_spec_ex_list_len; i++) {
		if ((req->filter_spec_ex_list[i].ip_type !=
			QMI_IPA_IP_TYPE_V4_V01) &&
			(req->filter_spec_ex_list[i].ip_type !=
			QMI_IPA_IP_TYPE_V6_V01))
			return -EINVAL;
		if (req->filter_spec_ex_list[i].is_mux_id_valid == false)
			return -EINVAL;
		if (req->filter_spec_ex_list[i].is_routing_table_index_valid
			== false)
			return -EINVAL;
		if ((req->filter_spec_ex_list[i].filter_action <=
			QMI_IPA_FILTER_ACTION_INVALID_V01) &&
			(req->filter_spec_ex_list[i].filter_action >
			QMI_IPA_FILTER_ACTION_EXCEPTION_V01))
			return -EINVAL;
	}
	mutex_lock(&ipa3_qmi_lock);
	if (ipa3_qmi_ctx != NULL) {
		/* cache the qmi_filter_request */
		memcpy(&(ipa3_qmi_ctx->ipa_install_fltr_rule_req_ex_msg_cache[
			ipa3_qmi_ctx->num_ipa_install_fltr_rule_req_ex_msg]),
			req,
			sizeof(struct ipa_install_fltr_rule_req_ex_msg_v01));
		ipa3_qmi_ctx->num_ipa_install_fltr_rule_req_ex_msg++;
		ipa3_qmi_ctx->num_ipa_install_fltr_rule_req_ex_msg %= 10;
	}
	mutex_unlock(&ipa3_qmi_lock);

	req_desc.max_msg_len =
		QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01;
	req_desc.ei_array = ipa3_install_fltr_rule_req_ex_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_ex_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_INSTALL_FILTER_RULE_EX_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_EX_RESP_V01;
	resp_desc.ei_array = ipa3_install_fltr_rule_resp_ex_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc,
			req,
			sizeof(struct ipa_install_fltr_rule_req_ex_msg_v01),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	return ipa3_check_qmi_response(rc,
		QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_install_filter");
}

/* sending ul-filter-install-request to modem*/
int ipa3_qmi_ul_filter_request_send(
	struct ipa_configure_ul_firewall_rules_req_msg_v01 *req)
{
	struct ipa_configure_ul_firewall_rules_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;

	/* check if the filter rules from IPACM is valid */
	if (req->firewall_rules_list_len < 0) {
		IPAWANERR("Ul filter rules are not valid\n");
		return -EINVAL;
	}

	IPAWANDBG("IPACM pass %u rules to Q6\n",
		req->firewall_rules_list_len);

	mutex_lock(&ipa3_qmi_lock);
	if (ipa3_qmi_ctx != NULL) {
		/* cache the qmi_filter_request */
		memcpy(
		&(ipa3_qmi_ctx->ipa_configure_ul_firewall_rules_req_msg_cache[
		ipa3_qmi_ctx->num_ipa_configure_ul_firewall_rules_req_msg]),
		req,
		sizeof(struct
		ipa_configure_ul_firewall_rules_req_msg_v01));
		ipa3_qmi_ctx->num_ipa_configure_ul_firewall_rules_req_msg++;
		ipa3_qmi_ctx->num_ipa_configure_ul_firewall_rules_req_msg %=
			MAX_NUM_QMI_RULE_CACHE;
	}
	mutex_unlock(&ipa3_qmi_lock);

	req_desc.max_msg_len =
		QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_V01;
	req_desc.ei_array =
		ipa3_configure_ul_firewall_rules_req_msg_data_v01_ei;

	memset(&resp, 0,
		sizeof(struct ipa_configure_ul_firewall_rules_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_INSTALL_UL_FIREWALL_RULES_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INSTALL_UL_FIREWALL_RULES_RESP_V01;
	resp_desc.ei_array =
		ipa3_configure_ul_firewall_rules_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc,
		req,
		sizeof(
		struct ipa_configure_ul_firewall_rules_req_msg_v01),
		&resp_desc, &resp, sizeof(resp),
		QMI_SEND_REQ_TIMEOUT_MS);
	return ipa3_check_qmi_response(rc,
		QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_received_ul_firewall_filter");
}

int ipa3_qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req)
{
	struct ipa_enable_force_clear_datapath_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc = 0;


	if (!req || !req->source_pipe_bitmask) {
		IPAWANERR("invalid params\n");
		return -EINVAL;
	}

	req_desc.max_msg_len =
	QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_V01;
	req_desc.ei_array =
		ipa3_enable_force_clear_datapath_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_RESP_V01;
	resp_desc.ei_array =
		ipa3_enable_force_clear_datapath_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt,
			&req_desc,
			req,
			sizeof(*req),
			&resp_desc, &resp, sizeof(resp), 0);
	if (rc < 0) {
		IPAWANERR("send req failed %d\n", rc);
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

int ipa3_qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req)
{
	struct ipa_disable_force_clear_datapath_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc = 0;


	if (!req) {
		IPAWANERR("invalid params\n");
		return -EINVAL;
	}

	req_desc.max_msg_len =
		QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_V01;
	req_desc.ei_array =
		ipa3_disable_force_clear_datapath_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_RESP_V01;
	resp_desc.ei_array =
		ipa3_disable_force_clear_datapath_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt,
			&req_desc,
			req,
			sizeof(*req),
			&resp_desc, &resp, sizeof(resp), 0);
	if (rc < 0) {
		IPAWANERR("send req failed %d\n", rc);
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
int ipa3_qmi_filter_notify_send(
		struct ipa_fltr_installed_notif_req_msg_v01 *req)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc = 0;

	/* check if the filter rules from IPACM is valid */
	if (req->rule_id_len == 0) {
		IPAWANERR(" delete UL filter rule for pipe %d\n",
		req->source_pipe_index);
		return -EINVAL;
	} else if (req->filter_index_list_len > QMI_IPA_MAX_FILTERS_V01) {
		IPAWANERR(" UL filter rule for pipe %d exceed max (%u)\n",
		req->source_pipe_index,
		req->filter_index_list_len);
		return -EINVAL;
	}

	if (req->install_status != IPA_QMI_RESULT_SUCCESS_V01) {
		IPAWANERR(" UL filter rule for pipe %d install_status = %d\n",
			req->source_pipe_index, req->install_status);
		return -EINVAL;
	} else if (req->rule_id_valid != 1) {
		IPAWANERR(" UL filter rule for pipe %d rule_id_valid = %d\n",
			req->source_pipe_index, req->rule_id_valid);
		return -EINVAL;
	} else if (req->source_pipe_index >= ipa3_ctx->ipa_num_pipes) {
		IPAWANDBG(
		"IPACM passes source pipe index not valid ID = %d\n",
		req->source_pipe_index);
		return -EINVAL;
	} else if (((req->embedded_pipe_index_valid != true) ||
			(req->embedded_call_mux_id_valid != true)) &&
			((req->embedded_pipe_index_valid != false) ||
			(req->embedded_call_mux_id_valid != false))) {
		IPAWANERR(
			"IPACM passes embedded pipe and mux valid not valid\n");
		return -EINVAL;
	} else if (req->embedded_pipe_index >= ipa3_ctx->ipa_num_pipes) {
		IPAWANERR("IPACM passes source pipe index not valid ID = %d\n",
		req->source_pipe_index);
		return -EINVAL;
	}

	if (req->source_pipe_index == -1) {
		IPAWANERR("Source pipe index invalid\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_qmi_lock);
	if (ipa3_qmi_ctx != NULL) {
		/* cache the qmi_filter_request */
		memcpy(&(ipa3_qmi_ctx->ipa_fltr_installed_notif_req_msg_cache[
			ipa3_qmi_ctx->num_ipa_fltr_installed_notif_req_msg]),
			req,
			sizeof(struct ipa_fltr_installed_notif_req_msg_v01));
		ipa3_qmi_ctx->num_ipa_fltr_installed_notif_req_msg++;
		ipa3_qmi_ctx->num_ipa_fltr_installed_notif_req_msg %= 10;
	}
	mutex_unlock(&ipa3_qmi_lock);

	req_desc.max_msg_len =
	QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01;
	req_desc.ei_array = ipa3_fltr_installed_notif_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01;
	resp_desc.ei_array = ipa3_fltr_installed_notif_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt,
			&req_desc,
			req,
			sizeof(struct ipa_fltr_installed_notif_req_msg_v01),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	return ipa3_check_qmi_response(rc,
		QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_fltr_installed_notif_resp");
}

static void ipa3_q6_clnt_recv_msg(struct work_struct *work)
{
	int rc;

	do {
		IPAWANDBG_LOW("Notified about a Receive Event");
		rc = qmi_recv_msg(ipa_q6_clnt);
	} while (rc == 0);
	if (rc != -ENOMSG)
		IPAWANERR("Error receiving message\n");
}

static void ipa3_q6_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		IPAWANDBG_LOW("client qmi recv message called");
		if (!workqueues_stopped)
			queue_delayed_work(ipa_clnt_resp_workqueue,
					   &ipa3_work_recv_msg_client, 0);
		break;
	default:
		break;
	}
}

static void ipa3_q6_clnt_ind_cb(struct qmi_handle *handle, unsigned int msg_id,
			       void *msg, unsigned int msg_len,
			       void *ind_cb_priv)
{
	struct ipa_data_usage_quota_reached_ind_msg_v01 qmi_ind;
	struct ipa_configure_ul_firewall_rules_ind_msg_v01 qmi_ul_firewall_ind;
	struct msg_desc qmi_ind_desc;
	int rc = 0;

	if (handle != ipa_q6_clnt) {
		IPAWANERR("Wrong client\n");
		return;
	}

	if (QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_V01 == msg_id) {
		memset(&qmi_ind, 0, sizeof(
			struct ipa_data_usage_quota_reached_ind_msg_v01));
		qmi_ind_desc.max_msg_len =
			QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_MAX_MSG_LEN_V01;
		qmi_ind_desc.msg_id = QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_V01;
		qmi_ind_desc.ei_array =
			ipa3_data_usage_quota_reached_ind_msg_data_v01_ei;

		rc = qmi_kernel_decode(&qmi_ind_desc, &qmi_ind, msg, msg_len);
		if (rc < 0) {
			IPAWANERR("Error decoding msg_id %d\n", msg_id);
			return;
		}
		IPAWANDBG("Quota reached indication on qmux(%d) Mbytes(%lu)\n",
			  qmi_ind.apn.mux_id,
			  (unsigned long int) qmi_ind.apn.num_Mbytes);
		ipa3_broadcast_quota_reach_ind(qmi_ind.apn.mux_id);
	}

	if (QMI_IPA_INSTALL_UL_FIREWALL_RULES_IND_V01 == msg_id) {
		memset(&qmi_ul_firewall_ind, 0, sizeof(
			struct ipa_configure_ul_firewall_rules_ind_msg_v01));
		qmi_ind_desc.max_msg_len =
			QMI_IPA_INSTALL_UL_FIREWALL_RULES_IND_MAX_MSG_LEN_V01;
		qmi_ind_desc.msg_id = QMI_IPA_INSTALL_UL_FIREWALL_RULES_IND_V01;
		qmi_ind_desc.ei_array =
			ipa3_configure_ul_firewall_rules_ind_msg_data_v01_ei;

		rc = qmi_kernel_decode(
			&qmi_ind_desc, &qmi_ul_firewall_ind, msg, msg_len);
		if (rc < 0) {
			IPAWANERR("Error decoding msg_id %d\n", msg_id);
			return;
		}

		IPAWANDBG("UL firewall rules install indication on Q6");
		if (qmi_ul_firewall_ind.result.is_success ==
				QMI_IPA_UL_FIREWALL_STATUS_SUCCESS_V01) {
			IPAWANDBG(" : Success\n");
			IPAWANDBG
			("Mux ID : %d\n", qmi_ul_firewall_ind.result.mux_id);
		} else if (qmi_ul_firewall_ind.result.is_success ==
				QMI_IPA_UL_FIREWALL_STATUS_FAILURE_V01){
			IPAWANERR(": Failure\n");
		} else {
			IPAWANERR(": Unexpected Result");
		}
	}
}

static void ipa3_q6_clnt_svc_arrive(struct work_struct *work)
{
	int rc;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;

	/* Create a Local client port for QMI communication */
	ipa_q6_clnt = qmi_handle_create(ipa3_q6_clnt_notify, NULL);
	if (!ipa_q6_clnt) {
		IPAWANERR("QMI client handle alloc failed\n");
		return;
	}

	IPAWANDBG("Lookup server name, get client-hdl(%p)\n",
		ipa_q6_clnt);
	rc = qmi_connect_to_service(ipa_q6_clnt,
			IPA_Q6_SERVICE_SVC_ID,
			IPA_Q6_SVC_VERS,
			IPA_Q6_SERVICE_INS_ID);
	if (rc < 0) {
		IPAWANERR("Server not found\n");
		qmi_handle_destroy(ipa_q6_clnt);
		ipa_q6_clnt = NULL;
		return;
	}

	rc = qmi_register_ind_cb(ipa_q6_clnt, ipa3_q6_clnt_ind_cb, NULL);
	if (rc < 0)
		IPAWANERR("Unable to register for indications\n");

	ipa_q6_clnt_reset = 0;
	IPAWANDBG("Q6 QMI service available now\n");
	/* Initialize modem IPA-driver */
	IPAWANDBG("send ipa3_qmi_init_modem_send_sync_msg to modem\n");
	rc = ipa3_qmi_init_modem_send_sync_msg();
	if ((rc == -ENETRESET) || (rc == -ENODEV)) {
		IPAWANERR(
			"ipa3_qmi_init_modem_send_sync_msg failed due to SSR!\n");
		/* Cleanup will take place when ipa3_wwan_remove is called */
		return;
	}
	if (rc != 0) {
		IPAWANERR("ipa3_qmi_init_modem_send_sync_msg failed\n");
		/*
		 * This is a very unexpected scenario, which requires a kernel
		 * panic in order to force dumps for QMI/Q6 side analysis.
		 */
		BUG();
		return;
	}
	ipa3_qmi_modem_init_fin = true;

	/* got modem_init_cmplt_req already, load uc-related register */
	if (ipa3_modem_init_cmplt == true) {
		IPAWANDBG("load uc related registers (%d)\n",
		ipa3_modem_init_cmplt);
			ipa3_uc_load_notify();
	}

	/* In cold-bootup, first_time_handshake = false */
	ipa3_q6_handshake_complete(first_time_handshake);
	first_time_handshake = true;
	IPAWANDBG("complete, ipa3_qmi_modem_init_fin : %d\n",
		ipa3_qmi_modem_init_fin);

	if (ipa3_qmi_indication_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		ipa3_qmi_indication_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;
		rc = qmi_send_ind(ipa3_svc_handle, curr_conn,
			&ipa3_master_driver_complete_indication_desc,
			&ind,
			sizeof(ind));
		IPAWANDBG("ipa_qmi_service_client good\n");
	} else {
		IPAWANERR("not send indication (%d)\n",
		ipa3_qmi_indication_fin);
	}
}


static void ipa3_q6_clnt_svc_exit(struct work_struct *work)
{
	qmi_handle_destroy(ipa_q6_clnt);
	ipa_q6_clnt_reset = 1;
	ipa_q6_clnt = NULL;
}


static int ipa3_q6_clnt_svc_event_notify(struct notifier_block *this,
				      unsigned long code,
				      void *_cmd)
{
	IPAWANDBG("event %ld\n", code);
	switch (code) {
	case QMI_SERVER_ARRIVE:
		if (!workqueues_stopped)
			queue_delayed_work(ipa_clnt_req_workqueue,
					   &ipa3_work_svc_arrive, 0);
		break;
	case QMI_SERVER_EXIT:
		if (!workqueues_stopped)
			queue_delayed_work(ipa_clnt_req_workqueue,
					   &ipa3_work_svc_exit, 0);
		break;
	default:
		break;
	}
	return 0;
}


static struct notifier_block ipa3_q6_clnt_nb = {
	.notifier_call = ipa3_q6_clnt_svc_event_notify,
};

static void ipa3_qmi_service_init_worker(struct work_struct *work)
{
	int rc;

	/* Initialize QMI-service*/
	IPAWANDBG("IPA A7 QMI init OK :>>>>\n");

	/* start the QMI msg cache */
	ipa3_qmi_ctx = vzalloc(sizeof(*ipa3_qmi_ctx));
	if (!ipa3_qmi_ctx) {
		IPAWANERR(":kzalloc err.\n");
		return;
	}
	ipa3_qmi_ctx->modem_cfg_emb_pipe_flt =
		ipa3_get_modem_cfg_emb_pipe_flt();

	ipa_svc_workqueue = create_singlethread_workqueue("ipa_A7_svc");
	if (!ipa_svc_workqueue) {
		IPAWANERR("Creating ipa_A7_svc workqueue failed\n");
		vfree(ipa3_qmi_ctx);
		ipa3_qmi_ctx = NULL;
		return;
	}

	ipa3_svc_handle = qmi_handle_create(qmi_ipa_a5_svc_ntfy, NULL);
	if (!ipa3_svc_handle) {
		IPAWANERR("Creating ipa_A7_svc qmi handle failed\n");
		goto destroy_ipa_A7_svc_wq;
	}

	/*
	 * Setting the current connection to NULL, as due to a race between
	 * server and client clean-up in SSR, the disconnect_cb might not
	 * have necessarily been called
	 */
	curr_conn = NULL;

	rc = qmi_svc_register(ipa3_svc_handle, &ipa3_a5_svc_ops_options);
	if (rc < 0) {
		IPAWANERR("Registering ipa_a5 svc failed %d\n",
				rc);
		goto destroy_qmi_handle;
	}

	/* Initialize QMI-client */

	ipa_clnt_req_workqueue = create_singlethread_workqueue("clnt_req");
	if (!ipa_clnt_req_workqueue) {
		IPAWANERR("Creating clnt_req workqueue failed\n");
		goto deregister_qmi_srv;
	}

	ipa_clnt_resp_workqueue = create_singlethread_workqueue("clnt_resp");
	if (!ipa_clnt_resp_workqueue) {
		IPAWANERR("Creating clnt_resp workqueue failed\n");
		goto destroy_clnt_req_wq;
	}

	rc = qmi_svc_event_notifier_register(IPA_Q6_SERVICE_SVC_ID,
				IPA_Q6_SVC_VERS,
				IPA_Q6_SERVICE_INS_ID, &ipa3_q6_clnt_nb);
	if (rc < 0) {
		IPAWANERR("notifier register failed\n");
		goto destroy_clnt_resp_wq;
	}

	/* get Q6 service and start send modem-initial to Q6 */
	IPAWANDBG("wait service available\n");
	return;

destroy_clnt_resp_wq:
	destroy_workqueue(ipa_clnt_resp_workqueue);
	ipa_clnt_resp_workqueue = NULL;
destroy_clnt_req_wq:
	destroy_workqueue(ipa_clnt_req_workqueue);
	ipa_clnt_req_workqueue = NULL;
deregister_qmi_srv:
	qmi_svc_unregister(ipa3_svc_handle);
destroy_qmi_handle:
	qmi_handle_destroy(ipa3_svc_handle);
	ipa3_svc_handle = 0;
destroy_ipa_A7_svc_wq:
	destroy_workqueue(ipa_svc_workqueue);
	ipa_svc_workqueue = NULL;
	vfree(ipa3_qmi_ctx);
	ipa3_qmi_ctx = NULL;
}

int ipa3_qmi_service_init(uint32_t wan_platform_type)
{
	ipa_wan_platform = wan_platform_type;
	ipa3_qmi_modem_init_fin = false;
	ipa3_qmi_indication_fin = false;
	ipa3_modem_init_cmplt = false;
	workqueues_stopped = false;

	if (!ipa3_svc_handle) {
		INIT_WORK(&ipa3_qmi_service_init_work,
			ipa3_qmi_service_init_worker);
		schedule_work(&ipa3_qmi_service_init_work);
	}
	return 0;
}

void ipa3_qmi_service_exit(void)
{
	int ret = 0;

	workqueues_stopped = true;

	/* qmi-service */
	if (ipa3_svc_handle) {
		ret = qmi_svc_unregister(ipa3_svc_handle);
		if (ret < 0)
			IPAWANERR("unregister qmi handle %p failed, ret=%d\n",
			ipa3_svc_handle, ret);
	}
	if (ipa_svc_workqueue) {
		flush_workqueue(ipa_svc_workqueue);
		destroy_workqueue(ipa_svc_workqueue);
		ipa_svc_workqueue = NULL;
	}

	if (ipa3_svc_handle) {
		ret = qmi_handle_destroy(ipa3_svc_handle);
		if (ret < 0)
			IPAWANERR("Error destroying qmi handle %p, ret=%d\n",
			ipa3_svc_handle, ret);
	}

	/* qmi-client */

	/* Unregister from events */
	ret = qmi_svc_event_notifier_unregister(IPA_Q6_SERVICE_SVC_ID,
				IPA_Q6_SVC_VERS,
				IPA_Q6_SERVICE_INS_ID, &ipa3_q6_clnt_nb);
	if (ret < 0)
		IPAWANERR(
		"Error qmi_svc_event_notifier_unregister service %d, ret=%d\n",
		IPA_Q6_SERVICE_SVC_ID, ret);

	/* Release client handle */
	ipa3_q6_clnt_svc_exit(0);

	if (ipa_clnt_req_workqueue) {
		destroy_workqueue(ipa_clnt_req_workqueue);
		ipa_clnt_req_workqueue = NULL;
	}
	if (ipa_clnt_resp_workqueue) {
		destroy_workqueue(ipa_clnt_resp_workqueue);
		ipa_clnt_resp_workqueue = NULL;
	}

	/* clean the QMI msg cache */
	mutex_lock(&ipa3_qmi_lock);
	if (ipa3_qmi_ctx != NULL) {
		vfree(ipa3_qmi_ctx);
		ipa3_qmi_ctx = NULL;
	}
	mutex_unlock(&ipa3_qmi_lock);

	ipa3_svc_handle = 0;
	ipa3_qmi_modem_init_fin = false;
	ipa3_qmi_indication_fin = false;
	ipa3_modem_init_cmplt = false;
}

void ipa3_qmi_stop_workqueues(void)
{
	IPAWANDBG("Stopping all QMI workqueues\n");

	/* Stopping all workqueues so new work won't be scheduled */
	workqueues_stopped = true;

	/* Making sure that the current scheduled work won't be executed */
	cancel_delayed_work(&work_recv_msg);
	cancel_delayed_work(&ipa3_work_recv_msg_client);
	cancel_delayed_work(&ipa3_work_svc_arrive);
	cancel_delayed_work(&ipa3_work_svc_exit);
}


/* voting for bus BW to ipa_rm*/
int ipa3_vote_for_bus_bw(uint32_t *bw_mbps)
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

int ipa3_qmi_get_data_stats(struct ipa_get_data_stats_req_msg_v01 *req,
			   struct ipa_get_data_stats_resp_msg_v01 *resp)
{
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_DATA_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_DATA_STATS_REQ_V01;
	req_desc.ei_array = ipa3_get_data_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_GET_DATA_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_DATA_STATS_RESP_V01;
	resp_desc.ei_array = ipa3_get_data_stats_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_GET_DATA_STATS_REQ_V01\n");

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
			sizeof(struct ipa_get_data_stats_req_msg_v01),
			&resp_desc, resp,
			sizeof(struct ipa_get_data_stats_resp_msg_v01),
			QMI_SEND_STATS_REQ_TIMEOUT_MS);

	IPAWANDBG_LOW("QMI_IPA_GET_DATA_STATS_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_GET_DATA_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa_get_data_stats_resp_msg_v01");
}

int ipa3_qmi_get_network_stats(struct ipa_get_apn_data_stats_req_msg_v01 *req,
			      struct ipa_get_apn_data_stats_resp_msg_v01 *resp)
{
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_APN_DATA_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_APN_DATA_STATS_REQ_V01;
	req_desc.ei_array = ipa3_get_apn_data_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_GET_APN_DATA_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_APN_DATA_STATS_RESP_V01;
	resp_desc.ei_array = ipa3_get_apn_data_stats_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_GET_APN_DATA_STATS_REQ_V01\n");

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
			sizeof(struct ipa_get_apn_data_stats_req_msg_v01),
			&resp_desc, resp,
			sizeof(struct ipa_get_apn_data_stats_resp_msg_v01),
			QMI_SEND_STATS_REQ_TIMEOUT_MS);

	IPAWANDBG_LOW("QMI_IPA_GET_APN_DATA_STATS_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_GET_APN_DATA_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa_get_apn_data_stats_req_msg_v01");
}

int ipa3_qmi_set_data_quota(struct ipa_set_data_usage_quota_req_msg_v01 *req)
{
	struct ipa_set_data_usage_quota_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;

	memset(&resp, 0, sizeof(struct ipa_set_data_usage_quota_resp_msg_v01));

	req_desc.max_msg_len = QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01;
	req_desc.ei_array = ipa3_set_data_usage_quota_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_V01;
	resp_desc.ei_array = ipa3_set_data_usage_quota_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01\n");

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
			sizeof(struct ipa_set_data_usage_quota_req_msg_v01),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_STATS_REQ_TIMEOUT_MS);

	IPAWANDBG_LOW("QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_set_data_usage_quota_req_msg_v01");
}

int ipa3_qmi_stop_data_qouta(void)
{
	struct ipa_stop_data_usage_quota_req_msg_v01 req;
	struct ipa_stop_data_usage_quota_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;

	memset(&req, 0, sizeof(struct ipa_stop_data_usage_quota_req_msg_v01));
	memset(&resp, 0, sizeof(struct ipa_stop_data_usage_quota_resp_msg_v01));

	req_desc.max_msg_len =
		QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01;
	req_desc.ei_array = ipa3_stop_data_usage_quota_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01;
	resp_desc.ei_array = ipa3_stop_data_usage_quota_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01\n");

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, &req, sizeof(req),
		&resp_desc, &resp, sizeof(resp),
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	IPAWANDBG_LOW("QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_stop_data_usage_quota_req_msg_v01");
}

int ipa3_qmi_enable_per_client_stats(
	struct ipa_enable_per_client_stats_req_msg_v01 *req,
	struct ipa_enable_per_client_stats_resp_msg_v01 *resp)
{
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len =
		QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id =
		QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_V01;
	req_desc.ei_array =
		ipa3_enable_per_client_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_ENABLE_PER_CLIENT_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id =
		QMI_IPA_ENABLE_PER_CLIENT_STATS_RESP_V01;
	resp_desc.ei_array =
		ipa3_enable_per_client_stats_resp_msg_data_v01_ei;

	IPAWANDBG("Sending QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_V01\n");

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
		sizeof(struct ipa_enable_per_client_stats_req_msg_v01),
		&resp_desc, resp,
		sizeof(struct ipa_enable_per_client_stats_resp_msg_v01),
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	IPAWANDBG("QMI_IPA_ENABLE_PER_CLIENT_STATS_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa3_qmi_enable_per_client_stats");
}

int ipa3_qmi_get_per_client_packet_stats(
	struct ipa_get_stats_per_client_req_msg_v01 *req,
	struct ipa_get_stats_per_client_resp_msg_v01 *resp)
{
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_STATS_PER_CLIENT_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01;
	req_desc.ei_array = ipa3_get_stats_per_client_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_GET_STATS_PER_CLIENT_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_STATS_PER_CLIENT_RESP_V01;
	resp_desc.ei_array = ipa3_get_stats_per_client_resp_msg_data_v01_ei;

	IPAWANDBG("Sending QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01\n");

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, req,
			sizeof(struct ipa_get_stats_per_client_req_msg_v01),
			&resp_desc, resp,
			sizeof(struct ipa_get_stats_per_client_resp_msg_v01),
			QMI_SEND_STATS_REQ_TIMEOUT_MS);

	IPAWANDBG("QMI_IPA_GET_STATS_PER_CLIENT_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01, resp->resp.result,
		resp->resp.error,
		"struct ipa_get_stats_per_client_req_msg_v01");
}

void ipa3_qmi_init(void)
{
	mutex_init(&ipa3_qmi_lock);
}

void ipa3_qmi_cleanup(void)
{
	mutex_destroy(&ipa3_qmi_lock);
}

