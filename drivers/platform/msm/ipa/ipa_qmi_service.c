/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include "ipa_qmi_service.h"
#include "ipa_ram_mmap.h"

#define IPA_Q6_SVC_VERS 1
#define IPA_A5_SVC_VERS 1
#define Q6_QMI_COMPLETION_TIMEOUT (60*HZ)

#define IPA_A5_SERVICE_SVC_ID 0x31
#define IPA_A5_SERVICE_INS_ID 1
#define IPA_Q6_SERVICE_SVC_ID 0x31
#define IPA_Q6_SERVICE_INS_ID 2

#define QMI_SEND_REQ_TIMEOUT_MS 10000

static struct qmi_handle *ipa_svc_handle;
static void ipa_a5_svc_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, ipa_a5_svc_recv_msg);
static struct workqueue_struct *ipa_svc_workqueue;
static struct workqueue_struct *ipa_clnt_req_workqueue;
static struct workqueue_struct *ipa_clnt_resp_workqueue;
static void *curr_conn;
static bool qmi_modem_init_fin, qmi_indication_fin;
static struct work_struct ipa_qmi_service_init_work;
static bool is_load_uc;


/* QMI A5 service */

static struct msg_desc ipa_indication_reg_req_desc = {
	.max_msg_len = QMI_IPA_INDICATION_REGISTER_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INDICATION_REGISTER_REQ_V01,
	.ei_array = ipa_indication_reg_req_msg_data_v01_ei,
};
static struct msg_desc ipa_indication_reg_resp_desc = {
	.max_msg_len = QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INDICATION_REGISTER_RESP_V01,
	.ei_array = ipa_indication_reg_resp_msg_data_v01_ei,
};
static struct msg_desc ipa_master_driver_complete_indication_desc = {
	.max_msg_len = QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01,
	.ei_array = ipa_master_driver_init_complt_ind_msg_data_v01_ei,
};
static struct msg_desc ipa_install_fltr_rule_req_desc = {
	.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01,
	.ei_array = ipa_install_fltr_rule_req_msg_data_v01_ei,
};
static struct msg_desc ipa_install_fltr_rule_resp_desc = {
	.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_INSTALL_FILTER_RULE_RESP_V01,
	.ei_array = ipa_install_fltr_rule_resp_msg_data_v01_ei,
};
static struct msg_desc ipa_filter_installed_notif_req_desc = {
	.max_msg_len = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01,
	.ei_array = ipa_fltr_installed_notif_req_msg_data_v01_ei,
};
static struct msg_desc ipa_filter_installed_notif_resp_desc = {
	.max_msg_len = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01,
	.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01,
	.ei_array = ipa_fltr_installed_notif_resp_msg_data_v01_ei,
};

static int handle_indication_req(void *req_h, void *req)
{
	struct ipa_indication_reg_req_msg_v01 *indication_req;
	struct ipa_indication_reg_resp_msg_v01 resp;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;
	int rc;

	indication_req = (struct ipa_indication_reg_req_msg_v01 *)req;
	IPAWANDBG("Received INDICATION Request\n");

	memset(&resp, 0, sizeof(struct ipa_indication_reg_resp_msg_v01));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	rc = qmi_send_resp_from_cb(ipa_svc_handle, curr_conn, req_h,
			&ipa_indication_reg_resp_desc, &resp, sizeof(resp));
	qmi_indication_fin = true;
	/* check if need sending indication to modem */
	if (qmi_modem_init_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		qmi_modem_init_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;
		rc = qmi_send_ind(ipa_svc_handle, curr_conn,
			&ipa_master_driver_complete_indication_desc,
			&ind,
			sizeof(ind));
	} else {
		IPAWANERR("not send indication\n");
	}
	return rc;
}


static int handle_install_filter_rule_req(void *req_h, void *req)
{
	struct ipa_install_fltr_rule_req_msg_v01 *rule_req;
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	uint32_t rule_hdl[MAX_NUM_Q6_RULE];
	int rc = 0, i;

	rule_req = (struct ipa_install_fltr_rule_req_msg_v01 *)req;
	memset(rule_hdl, 0, sizeof(rule_hdl));
	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_msg_v01));
	IPAWANDBG("Received install filter Request\n");

	rc = copy_ul_filter_rule_to_ipa((struct
		ipa_install_fltr_rule_req_msg_v01*)req, rule_hdl);
	if (rc)
		IPAWANERR("copy UL rules from modem is failed\n");

	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	if (rule_req->filter_spec_list_valid == true) {
		resp.filter_handle_list_valid = true;
		resp.filter_handle_list_len = rule_req->filter_spec_list_len;
	} else {
		resp.filter_handle_list_valid = false;
	}

	/* construct UL filter rules response to Modem*/
	for (i = 0; i < resp.filter_handle_list_len; i++) {
		resp.filter_handle_list[i].filter_spec_identifier =
			rule_req->filter_spec_list[i].filter_spec_identifier;
		resp.filter_handle_list[i].filter_handle = rule_hdl[i];
	}

	rc = qmi_send_resp_from_cb(ipa_svc_handle, curr_conn, req_h,
			&ipa_install_fltr_rule_resp_desc, &resp, sizeof(resp));

	IPAWANDBG("Replied to install filter request\n");
	return rc;
}

static int handle_filter_installed_notify_req(void *req_h, void *req)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	int rc = 0;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	IPAWANDBG("Received filter_install_notify Request\n");
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;

	rc = qmi_send_resp_from_cb(ipa_svc_handle, curr_conn, req_h,
			&ipa_filter_installed_notif_resp_desc,
			&resp, sizeof(resp));

	IPAWANDBG("Responsed filter_install_notify Request\n");
	return rc;
}

static int ipa_a5_svc_connect_cb(struct qmi_handle *handle,
			       void *conn_h)
{
	if (ipa_svc_handle != handle || !conn_h)
		return -EINVAL;

	if (curr_conn) {
		IPAWANERR("Service is busy\n");
		return -ECONNREFUSED;
	}
	curr_conn = conn_h;
	return 0;
}

static int ipa_a5_svc_disconnect_cb(struct qmi_handle *handle,
				  void *conn_h)
{
	if (ipa_svc_handle != handle || curr_conn != conn_h)
		return -EINVAL;

	curr_conn = NULL;
	return 0;
}

static int ipa_a5_svc_req_desc_cb(unsigned int msg_id,
				struct msg_desc **req_desc)
{
	int rc;
	switch (msg_id) {
	case QMI_IPA_INDICATION_REGISTER_REQ_V01:
		*req_desc = &ipa_indication_reg_req_desc;
		rc = sizeof(struct ipa_indication_reg_req_msg_v01);
		break;

	case QMI_IPA_INSTALL_FILTER_RULE_REQ_V01:
		*req_desc = &ipa_install_fltr_rule_req_desc;
		rc = sizeof(struct ipa_install_fltr_rule_req_msg_v01);
		break;
	case QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01:
		*req_desc = &ipa_filter_installed_notif_req_desc;
		rc = sizeof(struct ipa_fltr_installed_notif_req_msg_v01);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static int ipa_a5_svc_req_cb(struct qmi_handle *handle, void *conn_h,
			void *req_h, unsigned int msg_id, void *req)
{
	int rc;
	if (ipa_svc_handle != handle || curr_conn != conn_h)
		return -EINVAL;

	switch (msg_id) {
	case QMI_IPA_INDICATION_REGISTER_REQ_V01:
		rc = handle_indication_req(req_h, req);
		break;
	case QMI_IPA_INSTALL_FILTER_RULE_REQ_V01:
		rc = handle_install_filter_rule_req(req_h, req);
		rc = wwan_update_mux_channel_prop();
		break;
	case QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01:
		rc = handle_filter_installed_notify_req(req_h, req);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static void ipa_a5_svc_recv_msg(struct work_struct *work)
{
	int rc;

	do {
		IPAWANDBG("Notified about a Receive Event");
		rc = qmi_recv_msg(ipa_svc_handle);
	} while (rc == 0);
	if (rc != -ENOMSG)
		IPAWANERR("Error receiving message\n");
}

static void qmi_ipa_a5_svc_ntfy(struct qmi_handle *handle,
		enum qmi_event_type event, void *priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		queue_delayed_work(ipa_svc_workqueue,
				   &work_recv_msg, 0);
		break;
	default:
		break;
	}
}

static struct qmi_svc_ops_options ipa_a5_svc_ops_options = {
	.version = 1,
	.service_id = IPA_A5_SERVICE_SVC_ID,
	.service_vers = IPA_A5_SVC_VERS,
	.service_ins = IPA_A5_SERVICE_INS_ID,
	.connect_cb = ipa_a5_svc_connect_cb,
	.disconnect_cb = ipa_a5_svc_disconnect_cb,
	.req_desc_cb = ipa_a5_svc_req_desc_cb,
	.req_cb = ipa_a5_svc_req_cb,
};


/****************************************************/
/*                 QMI A5 client ->Q6               */
/****************************************************/
static void ipa_q6_clnt_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg_client, ipa_q6_clnt_recv_msg);
static void ipa_q6_clnt_svc_arrive(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_svc_arrive, ipa_q6_clnt_svc_arrive);
static void ipa_q6_clnt_svc_exit(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_svc_exit, ipa_q6_clnt_svc_exit);
/* Test client port for IPC Router */
static struct qmi_handle *ipa_q6_clnt;
static int ipa_q6_clnt_reset;

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
	IPAWANDBG("Received %s successfully\n", resp_type);
	return 0;
}

static int qmi_init_modem_send_sync_msg(void)
{
	struct ipa_init_modem_driver_req_msg_v01 req;
	struct ipa_init_modem_driver_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;

	memset(&req, 0, sizeof(struct ipa_init_modem_driver_req_msg_v01));
	memset(&resp, 0, sizeof(struct ipa_init_modem_driver_resp_msg_v01));
	req.platform_type_valid = true;
	req.platform_type = QMI_IPA_PLATFORM_TYPE_LE_V01;
	req.hdr_tbl_info_valid = true;
	req.hdr_tbl_info.modem_offset_start = IPA_v2_RAM_MODEM_HDR_OFST + 256;
	req.hdr_tbl_info.modem_offset_end = IPA_v2_RAM_MODEM_HDR_OFST + 256
		+ IPA_v2_RAM_MODEM_HDR_SIZE - 1;
	req.v4_route_tbl_info_valid = true;
	req.v4_route_tbl_info.route_tbl_start_addr =
		IPA_v2_RAM_V4_RT_OFST + 256;
	req.v4_route_tbl_info.num_indices = IPA_v2_V4_MODEM_RT_INDEX_HI;
	req.v6_route_tbl_info_valid = true;
	req.v6_route_tbl_info.route_tbl_start_addr =
		IPA_v2_RAM_V6_RT_OFST + 256;
	req.v6_route_tbl_info.num_indices = IPA_v2_V6_MODEM_RT_INDEX_HI;
	req.v4_filter_tbl_start_addr_valid = true;
	req.v4_filter_tbl_start_addr = IPA_v2_RAM_V4_FLT_OFST + 256;
	req.v6_filter_tbl_start_addr_valid = true;
	req.v6_filter_tbl_start_addr = IPA_v2_RAM_V6_FLT_OFST + 256;
	req.modem_mem_info_valid = true;
	req.modem_mem_info.block_start_addr = IPA_v2_RAM_MODEM_OFST + 256;
	req.modem_mem_info.size = IPA_v2_RAM_MODEM_SIZE;
	req.ctrl_comm_dest_end_pt_valid = true;
	req.ctrl_comm_dest_end_pt =
		ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	if (is_load_uc) {  /* First time boot */
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

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	return ipa_check_qmi_response(rc,
		QMI_IPA_INIT_MODEM_DRIVER_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_init_modem_driver_resp_msg_v01");
}

/* sending filter-install-request to modem*/
int qmi_filter_request_send(struct ipa_install_fltr_rule_req_msg_v01 *req)
{
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;

	/* check if the filter rules from IPACM is valid */
	if (req->filter_spec_list_len == 0) {
		IPAWANDBG("IPACM pass zero rules to Q6\n");
	} else {
		IPAWANDBG("IPACM pass %d rules to Q6\n",
		req->filter_spec_list_len);
	}

	req_desc.max_msg_len = QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01;
	req_desc.ei_array = ipa_install_fltr_rule_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_install_fltr_rule_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INSTALL_FILTER_RULE_RESP_V01;
	resp_desc.ei_array = ipa_install_fltr_rule_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt, &req_desc,
			req,
			sizeof(struct ipa_install_fltr_rule_req_msg_v01),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	return ipa_check_qmi_response(rc,
		QMI_IPA_INSTALL_FILTER_RULE_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_install_filter");
}


/* sending filter-installed-notify-request to modem*/
int qmi_filter_notify_send(struct ipa_fltr_installed_notif_req_msg_v01 *req)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc = 0;

	/* check if the filter rules from IPACM is valid */
	if (req->filter_index_list_len == 0) {
		IPAERR(" delete UL filter rule for pipe %d\n",
		req->source_pipe_index);
		return -EINVAL;
	}

	req_desc.max_msg_len =
	QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01;
	req_desc.ei_array = ipa_fltr_installed_notif_req_msg_data_v01_ei;

	memset(&resp, 0, sizeof(struct ipa_fltr_installed_notif_resp_msg_v01));
	resp_desc.max_msg_len =
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01;
	resp_desc.ei_array = ipa_fltr_installed_notif_resp_msg_data_v01_ei;

	rc = qmi_send_req_wait(ipa_q6_clnt,
			&req_desc,
			req,
			sizeof(struct ipa_fltr_installed_notif_req_msg_v01),
			&resp_desc, &resp, sizeof(resp),
			QMI_SEND_REQ_TIMEOUT_MS);
	return ipa_check_qmi_response(rc,
		QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_fltr_installed_notif_resp");
}

static void ipa_q6_clnt_recv_msg(struct work_struct *work)
{
	int rc;
	rc = qmi_recv_msg(ipa_q6_clnt);
	if (rc < 0)
		IPAWANERR("Error receiving message\n");
}

static void ipa_q6_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		IPAWANDBG("client qmi recv message called");
		queue_delayed_work(ipa_clnt_resp_workqueue,
				   &work_recv_msg_client, 0);
		break;
	default:
		break;
	}
}


static void ipa_q6_clnt_svc_arrive(struct work_struct *work)
{
	int rc;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;

	/*
	 * Setting the current connection to NULL, as due to a race between
	 * server and client clean-up in SSR, the disconnect_cb might not
	 * have necessarily been called
	 */
	curr_conn = NULL;

	/* Create a Local client port for QMI communication */
	ipa_q6_clnt = qmi_handle_create(ipa_q6_clnt_notify, NULL);
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
	ipa_q6_clnt_reset = 0;
	IPAWANDBG("Q6 QMI service available now\n");
	/* Initialize modem IPA-driver */
	IPAWANDBG("send qmi_init_modem_send_sync_msg to modem\n");
	rc = qmi_init_modem_send_sync_msg();
	qmi_modem_init_fin = true;
	IPAWANDBG("complete, qmi_modem_init_fin : %d\n",
		qmi_modem_init_fin);

	if (qmi_indication_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		qmi_indication_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;
		rc = qmi_send_ind(ipa_svc_handle, curr_conn,
			&ipa_master_driver_complete_indication_desc,
			&ind,
			sizeof(ind));
		IPAWANDBG("ipa_qmi_service_client good\n");
	} else {
		IPAWANERR("not send indication (%d)\n",
		qmi_indication_fin);
	}
}


static void ipa_q6_clnt_svc_exit(struct work_struct *work)
{
	qmi_handle_destroy(ipa_q6_clnt);
	ipa_q6_clnt_reset = 1;
	ipa_q6_clnt = NULL;
}


static int ipa_q6_clnt_svc_event_notify(struct notifier_block *this,
				      unsigned long code,
				      void *_cmd)
{
	IPAWANDBG("event %ld\n", code);
	switch (code) {
	case QMI_SERVER_ARRIVE:
		queue_delayed_work(ipa_clnt_req_workqueue,
				   &work_svc_arrive, 0);
		break;
	case QMI_SERVER_EXIT:
		queue_delayed_work(ipa_clnt_req_workqueue,
				   &work_svc_exit, 0);
		break;
	default:
		break;
	}
	return 0;
}


static struct notifier_block ipa_q6_clnt_nb = {
	.notifier_call = ipa_q6_clnt_svc_event_notify,
};

static void ipa_qmi_service_init_worker(struct work_struct *work)
{
	int rc;

	/* Initialize QMI-service*/
	IPAWANDBG("IPA A7 QMI init OK :>>>>\n");
	qmi_modem_init_fin = false;
	qmi_indication_fin = false;

	ipa_svc_workqueue = create_singlethread_workqueue("ipa_A7_svc");
	if (!ipa_svc_workqueue)
		return;

	ipa_svc_handle = qmi_handle_create(qmi_ipa_a5_svc_ntfy, NULL);
	if (!ipa_svc_handle) {
		IPAWANERR("Creating ipa_A7_svc qmi handle failed\n");
		destroy_workqueue(ipa_svc_workqueue);
		return;
	}

	rc = qmi_svc_register(ipa_svc_handle, &ipa_a5_svc_ops_options);
	if (rc < 0) {
		IPAWANERR("Registering ipa_a5 svc failed %d\n",
				rc);
		qmi_handle_destroy(ipa_svc_handle);
		destroy_workqueue(ipa_svc_workqueue);
		return;
	}

	/* Initialize QMI-client */

	ipa_clnt_req_workqueue = create_singlethread_workqueue("clnt_req");
	if (!ipa_clnt_req_workqueue)
		return;

	ipa_clnt_resp_workqueue = create_singlethread_workqueue("clnt_resp");
	if (!ipa_clnt_resp_workqueue)
		return;

	rc = qmi_svc_event_notifier_register(IPA_Q6_SERVICE_SVC_ID,
				IPA_Q6_SVC_VERS,
				IPA_Q6_SERVICE_INS_ID, &ipa_q6_clnt_nb);
	if (rc < 0) {
		IPAWANERR("notifier register failed\n");
		destroy_workqueue(ipa_clnt_req_workqueue);
		destroy_workqueue(ipa_clnt_resp_workqueue);
		return;
	}

	/* get Q6 service and start send modem-initial to Q6 */
	IPAWANDBG("wait service available\n");
	return;
}

int ipa_qmi_service_init(bool load_uc)
{
	is_load_uc = load_uc;
	if (!ipa_svc_handle) {
		INIT_WORK(&ipa_qmi_service_init_work,
			ipa_qmi_service_init_worker);
		schedule_work(&ipa_qmi_service_init_work);
	}
	return 0;
}

void ipa_qmi_service_exit(void)
{
	int ret = 0;
	/* qmi-service */
	ret = qmi_svc_unregister(ipa_svc_handle);
	if (ret < 0)
		IPAWANERR("Error unregistering qmi service handle %p, ret=%d\n",
		ipa_svc_handle, ret);
	flush_workqueue(ipa_svc_workqueue);
	ret = qmi_handle_destroy(ipa_svc_handle);
	if (ret < 0)
		IPAWANERR("Error destroying qmi handle %p, ret=%d\n",
		ipa_svc_handle, ret);
	destroy_workqueue(ipa_svc_workqueue);

	/* qmi-client */
	ret = qmi_svc_event_notifier_unregister(IPA_Q6_SERVICE_SVC_ID,
				IPA_Q6_SVC_VERS,
				IPA_Q6_SERVICE_INS_ID, &ipa_q6_clnt_nb);
	if (ret < 0)
		IPAWANERR(
		"Error qmi_svc_event_notifier_unregister service %d, ret=%d\n",
		IPA_Q6_SERVICE_SVC_ID, ret);
	destroy_workqueue(ipa_clnt_req_workqueue);
	destroy_workqueue(ipa_clnt_resp_workqueue);

	ipa_svc_handle = 0;
}
