// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
#include "ipa_mhi_proxy.h"

#define IPA_Q6_SVC_VERS 1
#define IPA_A5_SVC_VERS 1
#define Q6_QMI_COMPLETION_TIMEOUT (60*HZ)

#define IPA_A5_SERVICE_SVC_ID 0x31
#define IPA_A5_SERVICE_INS_ID 1
#define IPA_Q6_SERVICE_SVC_ID 0x31
#define IPA_Q6_SERVICE_INS_ID 2

#define QMI_SEND_STATS_REQ_TIMEOUT_MS 5000
#define QMI_SEND_REQ_TIMEOUT_MS 60000
#define QMI_MHI_SEND_REQ_TIMEOUT_MS 1000

#define QMI_IPA_FORCE_CLEAR_DATAPATH_TIMEOUT_MS 1000

static struct qmi_handle *ipa3_svc_handle;
static struct workqueue_struct *ipa_clnt_req_workqueue;
static bool ipa3_qmi_modem_init_fin, ipa3_qmi_indication_fin;
static struct work_struct ipa3_qmi_service_init_work;
static uint32_t ipa_wan_platform;
struct ipa3_qmi_context *ipa3_qmi_ctx;
static bool workqueues_stopped;
static bool ipa3_modem_init_cmplt;
static bool first_time_handshake;
static bool send_qmi_init_q6;
struct mutex ipa3_qmi_lock;
struct ipa_msg_desc {
	uint16_t msg_id;
	int max_msg_len;
	struct qmi_elem_info *ei_array;
};

static struct ipa_mhi_prime_aggr_info_req_msg_v01 aggr_req = {
	.aggr_info_valid = 1,
	.aggr_info_len = 5,
	.aggr_info[0] = {
		.ic_type = DATA_IC_TYPE_MHI_PRIME_V01,
		.ep_type = DATA_EP_DESC_TYPE_DPL_PROD_V01,
		.bytes_count = 16,
	},
	.aggr_info[1] = {
		.ic_type = DATA_IC_TYPE_MHI_PRIME_V01,
		.ep_type = DATA_EP_DESC_TYPE_TETH_CONS_V01,
		.bytes_count = 24,
		.aggr_type = DATA_AGGR_TYPE_QMAPv5_V01,
	},
	.aggr_info[2] = {
		.ic_type = DATA_IC_TYPE_MHI_PRIME_V01,
		.ep_type = DATA_EP_DESC_TYPE_TETH_PROD_V01,
		.bytes_count = 16,
		.aggr_type = DATA_AGGR_TYPE_QMAPv5_V01,
	},
	.aggr_info[3] = {
		.ic_type = DATA_IC_TYPE_MHI_PRIME_V01,
		.ep_type = DATA_EP_DESC_TYPE_TETH_RMNET_CONS_V01,
		.bytes_count = 31,
		.aggr_type = DATA_AGGR_TYPE_QMAPv5_V01,
	},
	.aggr_info[4] = {
		.ic_type = DATA_IC_TYPE_MHI_PRIME_V01,
		.ep_type = DATA_EP_DESC_TYPE_TETH_RMNET_PROD_V01,
		.bytes_count = 31,
		.aggr_type = DATA_AGGR_TYPE_QMAPv5_V01,
	},
};

/* QMI A5 service */

static void ipa3_handle_indication_req(struct qmi_handle *qmi_handle,
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
	memcpy(&ipa3_qmi_ctx->client_sq, sq, sizeof(*sq));

	memset(&resp, 0, sizeof(struct ipa_indication_reg_resp_msg_v01));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;

	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp.resp.result, resp.resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_INDICATION_REGISTER_RESP_V01,
		QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01,
		ipa3_indication_reg_resp_msg_data_v01_ei,
		&resp);

	if (rc < 0) {
		IPAWANERR("send response for Indication register failed\n");
		return;
	}

	ipa3_qmi_indication_fin = true;

	/* check if need sending indication to modem */
	if (ipa3_qmi_modem_init_fin)	{
		IPAWANDBG("send indication to modem (%d)\n",
		ipa3_qmi_modem_init_fin);
		memset(&ind, 0, sizeof(struct
				ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;

		rc = qmi_send_indication(qmi_handle,
			&(ipa3_qmi_ctx->client_sq),
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01,
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01,
			ipa3_master_driver_init_complt_ind_msg_data_v01_ei,
			&ind);

		if (rc < 0) {
			IPAWANERR("send indication failed\n");
			ipa3_qmi_indication_fin = false;
		}
	} else {
		IPAWANERR("not send indication\n");
	}
}

static void ipa3_handle_install_filter_rule_req(struct qmi_handle *qmi_handle,
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

	rc = ipa3_copy_ul_filter_rule_to_ipa((struct
		ipa_install_fltr_rule_req_msg_v01*)decoded_msg);

	if (rc) {
		IPAWANERR("copy UL rules from modem is failed\n");
		return;
	}

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

	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp.resp.result, resp.resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_INSTALL_FILTER_RULE_RESP_V01,
		QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01,
		ipa3_install_fltr_rule_resp_msg_data_v01_ei,
		&resp);

	if (rc < 0)
		IPAWANERR("install filter rules failed\n");
	else
		IPAWANDBG("Replied to install filter request\n");
}

static void ipa3_handle_filter_installed_notify_req(
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

	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp.resp.result, resp.resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01,
		QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01,
		ipa3_fltr_installed_notif_resp_msg_data_v01_ei,
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
		IPAERR("ipa3_mhi_handle_ipa_config_req failed %d\n", rc);
		resp.resp.result = IPA_QMI_RESULT_FAILURE_V01;
	}

	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp.resp.result, resp.resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_CONFIG_RESP_V01,
		QMI_IPA_CONFIG_RESP_MAX_MSG_LEN_V01,
		ipa3_config_resp_msg_data_v01_ei,
		&resp);

	if (rc < 0)
		IPAWANERR("QMI_IPA_CONFIG_RESP_V01 failed\n");
	else
		IPAWANDBG("Responsed QMI_IPA_CONFIG_RESP_V01\n");
}

static void ipa3_handle_modem_init_cmplt_req(struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_init_modem_driver_cmplt_req_msg_v01 *cmplt_req;
	struct ipa_init_modem_driver_cmplt_resp_msg_v01 resp;
	int rc;

	IPAWANDBG("Received QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01\n");
	cmplt_req = (struct ipa_init_modem_driver_cmplt_req_msg_v01 *)
		decoded_msg;

	if (!ipa3_modem_init_cmplt) {
		ipa3_modem_init_cmplt = true;
	}

	memset(&resp, 0, sizeof(resp));
	resp.resp.result = IPA_QMI_RESULT_SUCCESS_V01;

	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp.resp.result, resp.resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_V01,
		QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_MAX_MSG_LEN_V01,
		ipa3_init_modem_driver_cmplt_resp_msg_data_v01_ei,
		&resp);


	if (rc < 0)
		IPAWANERR("QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_V01 failed\n");
	else
		IPAWANDBG("Sent QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_V01\n");
}

static void ipa3_handle_mhi_alloc_channel_req(struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_mhi_alloc_channel_req_msg_v01 *ch_alloc_req;
	struct ipa_mhi_alloc_channel_resp_msg_v01 *resp = NULL;
	int rc;

	IPAWANDBG("Received QMI_IPA_MHI_ALLOC_CHANNEL_REQ_V01\n");
	ch_alloc_req = (struct ipa_mhi_alloc_channel_req_msg_v01 *)decoded_msg;

	resp = imp_handle_allocate_channel_req(ch_alloc_req);
	if (!resp) {
		IPAWANERR("imp handle allocate channel req fails\n");
		return;
	}

	IPAWANDBG("qmi_snd_rsp: result %d, err %d, arr_vald: %d, arr_len %d\n",
		resp->resp.result, resp->resp.error, resp->alloc_resp_arr_valid,
		resp->alloc_resp_arr_len);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_MHI_ALLOC_CHANNEL_RESP_V01,
		IPA_MHI_ALLOC_CHANNEL_RESP_MSG_V01_MAX_MSG_LEN,
		ipa_mhi_alloc_channel_resp_msg_v01_ei,
		resp);

	if (rc < 0)
		IPAWANERR("QMI_IPA_MHI_ALLOC_CHANNEL_RESP_V01 failed\n");
	else
		IPAWANDBG("Sent QMI_IPA_MHI_ALLOC_CHANNEL_RESP_V01\n");
}

static void ipa3_handle_mhi_vote_req(struct qmi_handle *qmi_handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *decoded_msg)
{
	struct ipa_mhi_clk_vote_req_msg_v01 *vote_req;
	struct ipa_mhi_clk_vote_resp_msg_v01 *resp = NULL, resp2;
	int rc;
	uint32_t bw_mbps = 0;

	vote_req = (struct ipa_mhi_clk_vote_req_msg_v01 *)decoded_msg;
	IPAWANDBG("Received QMI_IPA_MHI_CLK_VOTE_REQ_V01(%d)\n",
		vote_req->mhi_vote);

	memset(&resp2, 0, sizeof(struct ipa_mhi_clk_vote_resp_msg_v01));

	/* for mpm used for ipa clk voting */
	if (ipa3_is_apq()) {
		IPAWANDBG("Throughput(%d:%d) clk-rate(%d:%d)\n",
			vote_req->tput_value_valid,
			vote_req->tput_value,
			vote_req->clk_rate_valid,
			vote_req->clk_rate);
		if (vote_req->clk_rate_valid) {
			switch (vote_req->clk_rate) {
			case QMI_IPA_CLOCK_RATE_LOW_SVS_V01:
				bw_mbps = 0;
				break;
			case QMI_IPA_CLOCK_RATE_SVS_V01:
				bw_mbps = 350;
				break;
			case QMI_IPA_CLOCK_RATE_NOMINAL_V01:
				bw_mbps = 690;
				break;
			case QMI_IPA_CLOCK_RATE_TURBO_V01:
				bw_mbps = 1200;
				break;
			default:
				IPAWANERR("Note supported clk_rate (%d)\n",
				vote_req->clk_rate);
				bw_mbps = 0;
				resp2.resp.result = IPA_QMI_RESULT_FAILURE_V01;
				resp2.resp.error =
					IPA_QMI_ERR_NOT_SUPPORTED_V01;
				break;
			}
			if (ipa3_vote_for_bus_bw(&bw_mbps)) {
				IPAWANERR("Failed to vote BW (%u)\n", bw_mbps);
				resp2.resp.result = IPA_QMI_RESULT_FAILURE_V01;
				resp2.resp.error =
					IPA_QMI_ERR_NOT_SUPPORTED_V01;
			}
			resp = &resp2;
		} else {
			IPAWANERR("clk_rate_valid is false\n");
			return;
		}
	} else {
		resp = imp_handle_vote_req(vote_req->mhi_vote);
		if (!resp) {
			IPAWANERR("imp handle vote req fails\n");
			return;
		}
		IPAWANDBG("start sending QMI_IPA_MHI_CLK_VOTE_RESP_V01\n");
	}

	IPAWANDBG("qmi_snd_rsp: result %d, err %d\n",
		resp->resp.result, resp->resp.error);
	rc = qmi_send_response(qmi_handle, sq, txn,
		QMI_IPA_MHI_CLK_VOTE_RESP_V01,
		IPA_MHI_CLK_VOTE_RESP_MSG_V01_MAX_MSG_LEN,
		ipa_mhi_clk_vote_resp_msg_v01_ei,
		resp);

	if (rc < 0)
		IPAWANERR("QMI_IPA_MHI_CLK_VOTE_RESP_V01 failed\n");
	else
		IPAWANDBG("Finished senting QMI_IPA_MHI_CLK_VOTE_RESP_V01\n");
}

static void ipa3_a5_svc_disconnect_cb(struct qmi_handle *qmi,
	unsigned int node, unsigned int port)
{
	IPAWANDBG_LOW("Received QMI client disconnect\n");
}

/****************************************************/
/*                 QMI A5 client ->Q6               */
/****************************************************/
static void ipa3_q6_clnt_svc_arrive(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_work_svc_arrive, ipa3_q6_clnt_svc_arrive);
static void ipa3_q6_clnt_svc_exit(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_work_svc_exit, ipa3_q6_clnt_svc_exit);
/* Test client port for IPC Router */
static struct qmi_handle *ipa_q6_clnt;

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

static int ipa3_qmi_send_req_wait(struct qmi_handle *client_handle,
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
		&ipa3_qmi_ctx->server_sq,
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

static int ipa3_qmi_init_modem_send_sync_msg(void)
{
	struct ipa_init_modem_driver_req_msg_v01 req;
	struct ipa_init_modem_driver_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;
	u16 smem_restr_bytes = ipa3_get_smem_restr_bytes();
	int wan_cons_ep;

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

	wan_cons_ep = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	if (wan_cons_ep == IPA_EP_NOT_ALLOCATED) {
		IPAWANDBG("APPS_WAN_CONS is not valid\n");
		req.ctrl_comm_dest_end_pt_valid = false;
		req.ctrl_comm_dest_end_pt = 0;
	} else {
		req.ctrl_comm_dest_end_pt_valid = true;
		req.ctrl_comm_dest_end_pt =
			ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	}

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

	/* if hashing not supported, Modem filter/routing hash
	 * tables should not fill with valid data.
	 */
	if (!ipa3_ctx->ipa_fltrt_not_hashable) {
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
	}
	req.hw_stats_quota_base_addr_valid = true;
	req.hw_stats_quota_base_addr =
		IPA_MEM_PART(stats_quota_ofst) + smem_restr_bytes;

	req.hw_stats_quota_size_valid = true;
	req.hw_stats_quota_size = IPA_MEM_PART(stats_quota_size);

	req.hw_drop_stats_base_addr_valid = true;
	req.hw_drop_stats_base_addr =
		IPA_MEM_PART(stats_drop_ofst) + smem_restr_bytes;

	req.hw_drop_stats_table_size_valid = true;
	req.hw_drop_stats_table_size = IPA_MEM_PART(stats_drop_size);

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
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, &req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_INIT_MODEM_DRIVER_REQ_V01,
			rc);
		return rc;
	}

	pr_info("QMI_IPA_INIT_MODEM_DRIVER_REQ_V01 response received\n");
	return ipa3_check_qmi_response(rc,
		QMI_IPA_INIT_MODEM_DRIVER_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_init_modem_driver_resp_msg_v01");
}

/* sending filter-install-request to modem*/
int ipa3_qmi_filter_request_send(struct ipa_install_fltr_rule_req_msg_v01 *req)
{
	struct ipa_install_fltr_rule_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
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
			QMI_IPA_FILTER_ACTION_INVALID_V01) ||
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

	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_INSTALL_FILTER_RULE_REQ_V01,
			rc);
		return rc;
	}

	return ipa3_check_qmi_response(rc,
		QMI_IPA_INSTALL_FILTER_RULE_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_install_filter");
}

/* sending filter-install-request to modem*/
int ipa3_qmi_filter_request_ex_send(
	struct ipa_install_fltr_rule_req_ex_msg_v01 *req)
{
	struct ipa_install_fltr_rule_resp_ex_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
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
			QMI_IPA_FILTER_ACTION_INVALID_V01) ||
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

	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01,
			rc);
		return rc;
	}

	return ipa3_check_qmi_response(rc,
		QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_install_filter");
}

/* sending add offload-connection-request to modem*/
int ipa3_qmi_add_offload_request_send(
	struct ipa_add_offload_connection_req_msg_v01 *req)
{
	struct ipa_add_offload_connection_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;
	int i, j;
	uint32_t id;

	/* check if modem up */
	if (!ipa3_qmi_modem_init_fin ||
		!ipa_q6_clnt) {
		IPAWANDBG("modem QMI haven't up yet\n");
		return -EINVAL;
	}

	/* check if the filter rules from IPACM is valid */
	if (req->filter_spec_ex2_list_len == 0) {
		IPAWANDBG("IPACM pass zero rules to Q6\n");
	} else {
		IPAWANDBG("IPACM pass %u rules to Q6\n",
		req->filter_spec_ex2_list_len);
	}

	/* currently set total max to 64 */
	if (req->filter_spec_ex2_list_len +
		ipa3_qmi_ctx->num_ipa_offload_connection
		>= QMI_IPA_MAX_FILTERS_V01) {
		IPAWANDBG(
		"cur(%d), req(%d), exceed limit (%d)\n",
			ipa3_qmi_ctx->num_ipa_offload_connection,
			req->filter_spec_ex2_list_len,
			QMI_IPA_MAX_FILTERS_V01);
		return -EINVAL;
	}

	for (i = 0; i < req->filter_spec_ex2_list_len; i++) {
		if ((req->filter_spec_ex2_list[i].ip_type !=
			QMI_IPA_IP_TYPE_V4_V01) &&
			(req->filter_spec_ex2_list[i].ip_type !=
			QMI_IPA_IP_TYPE_V6_V01))
			return -EINVAL;
		if (req->filter_spec_ex2_list[i].is_mux_id_valid == false)
			return -EINVAL;
		if ((req->filter_spec_ex2_list[i].filter_action <=
			QMI_IPA_FILTER_ACTION_INVALID_V01) ||
			(req->filter_spec_ex2_list[i].filter_action >
			QMI_IPA_FILTER_ACTION_EXCEPTION_V01))
			return -EINVAL;
	}

	req_desc.max_msg_len =
		IPA_ADD_OFFLOAD_CONNECTION_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_IPA_ADD_OFFLOAD_CONNECTION_REQ_V01;
	req_desc.ei_array = ipa_add_offload_connection_req_msg_v01_ei;

	memset(&resp, 0, sizeof(struct
		ipa_add_offload_connection_resp_msg_v01));
	resp_desc.max_msg_len =
		IPA_ADD_OFFLOAD_CONNECTION_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_IPA_ADD_OFFLOAD_CONNECTION_RESP_V01;
	resp_desc.ei_array = ipa_add_offload_connection_resp_msg_v01_ei;

	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_ADD_OFFLOAD_CONNECTION_REQ_V01,
			rc);
		return rc;
	}

	rc = ipa3_check_qmi_response(rc,
		QMI_IPA_ADD_OFFLOAD_CONNECTION_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_add_offload_connection");

	if (rc) {
		IPAWANERR("QMI get Response %d failed, rc= %d\n",
			QMI_IPA_ADD_OFFLOAD_CONNECTION_REQ_V01,
			rc);
		return rc;
	}

	/* Check & copy rule-handle */
	if (!resp.filter_handle_list_valid) {
		IPAWANERR("QMI resp invalid %d failed\n",
			resp.filter_handle_list_valid);
		return -ERANGE;
	}

	if (resp.filter_handle_list_len !=
		req->filter_spec_ex2_list_len) {
		IPAWANERR("QMI resp invalid size %d req %d\n",
			resp.filter_handle_list_len,
			req->filter_spec_ex2_list_len);
		return -ERANGE;
	}

	mutex_lock(&ipa3_qmi_lock);
	for (i = 0; i < req->filter_spec_ex2_list_len; i++) {
		id = resp.filter_handle_list[i].filter_spec_identifier;
		/* check rule-id matched or not */
		if (req->filter_spec_ex2_list[i].rule_id !=
			id) {
			IPAWANERR("QMI error (%d)st-(%d) rule-id (%d)\n",
				i,
				id,
				req->filter_spec_ex2_list[i].rule_id);
			mutex_unlock(&ipa3_qmi_lock);
			return -EINVAL;
		}
		/* find free spot*/
		for (j = 0; j < QMI_IPA_MAX_FILTERS_V01; j++) {
			if (!ipa3_qmi_ctx->ipa_offload_cache[j].valid)
				break;
		}

		if (j == QMI_IPA_MAX_FILTERS_V01) {
			IPAWANERR("can't find free spot for rule-id %d\n",
				id);
			mutex_unlock(&ipa3_qmi_lock);
			return -EINVAL;
		}

		/* save rule-id handle to cache */
		ipa3_qmi_ctx->ipa_offload_cache[j].rule_id =
			resp.filter_handle_list[i].filter_spec_identifier;
		ipa3_qmi_ctx->ipa_offload_cache[j].rule_hdl =
			resp.filter_handle_list[i].filter_handle;
		ipa3_qmi_ctx->ipa_offload_cache[j].valid = true;
		ipa3_qmi_ctx->ipa_offload_cache[j].ip_type =
			req->filter_spec_ex2_list[i].ip_type;
		ipa3_qmi_ctx->num_ipa_offload_connection++;
	}
	mutex_unlock(&ipa3_qmi_lock);
	IPAWANDBG("Update cached conntrack entries (%d)\n",
		ipa3_qmi_ctx->num_ipa_offload_connection);
	return rc;
}

/* sending rmv offload-connection-request to modem*/
int ipa3_qmi_rmv_offload_request_send(
	struct ipa_remove_offload_connection_req_msg_v01 *req)
{
	struct ipa_remove_offload_connection_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;
	int i, j;
	uint32_t id;

	/* check if modem up */
	if (!ipa3_qmi_modem_init_fin ||
		!ipa_q6_clnt) {
		IPAWANDBG("modem QMI haven't up yet\n");
		return -EINVAL;
	}

	/* check if the # of handles from IPACM is valid */
	if (!req->clean_all_rules_valid && req->filter_handle_list_len == 0) {
		IPAWANDBG("IPACM deleted zero rules !\n");
		return -EINVAL;
	}

	IPAWANDBG("IPACM pass (%d) rules handles to Q6, cur (%d)\n",
	req->filter_handle_list_len,
	ipa3_qmi_ctx->num_ipa_offload_connection);

	/*  max as num_ipa_offload_connection */
	if (req->filter_handle_list_len >
		ipa3_qmi_ctx->num_ipa_offload_connection) {
		IPAWANDBG(
		"cur(%d), req_rmv(%d)\n",
			ipa3_qmi_ctx->num_ipa_offload_connection,
			req->filter_handle_list_len);
		return -EINVAL;
	}

	mutex_lock(&ipa3_qmi_lock);
	for (i = 0; i < req->filter_handle_list_len; i++) {
		/* check if rule-id match */
		id =
			req->filter_handle_list[i].filter_spec_identifier;
		for (j = 0; j < QMI_IPA_MAX_FILTERS_V01; j++) {
			if ((ipa3_qmi_ctx->ipa_offload_cache[j].valid) &&
				(ipa3_qmi_ctx->ipa_offload_cache[j].rule_id ==
				id))
				break;
		}
		if (j == QMI_IPA_MAX_FILTERS_V01) {
			IPAWANERR("can't find rule-id %d\n",
				id);
			mutex_unlock(&ipa3_qmi_lock);
			return -EINVAL;
		}

		/* fill up the filter_handle */
		req->filter_handle_list[i].filter_handle =
			ipa3_qmi_ctx->ipa_offload_cache[j].rule_hdl;
		ipa3_qmi_ctx->ipa_offload_cache[j].valid = false;
		ipa3_qmi_ctx->num_ipa_offload_connection--;
	}
	mutex_unlock(&ipa3_qmi_lock);

	req_desc.max_msg_len =
		IPA_REMOVE_OFFLOAD_CONNECTION_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_IPA_REMOVE_OFFLOAD_CONNECTION_REQ_V01;
	req_desc.ei_array = ipa_remove_offload_connection_req_msg_v01_ei;

	/* clean the Dl rules  in the cache if flag is set */
	if (req->clean_all_rules) {
		for (i = 0; i < QMI_IPA_MAX_FILTERS_V01; i++)
			if (ipa3_qmi_ctx->ipa_offload_cache[i].valid)
				ipa3_qmi_ctx->ipa_offload_cache[i].valid =
				false;
	}


	memset(&resp, 0, sizeof(struct
		ipa_remove_offload_connection_resp_msg_v01));
	resp_desc.max_msg_len =
		IPA_REMOVE_OFFLOAD_CONNECTION_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_IPA_REMOVE_OFFLOAD_CONNECTION_RESP_V01;
	resp_desc.ei_array = ipa_remove_offload_connection_resp_msg_v01_ei;

	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_REMOVE_OFFLOAD_CONNECTION_REQ_V01,
			rc);
		return rc;
	}
	IPAWANDBG("left cached conntrack entries (%d)\n",
		ipa3_qmi_ctx->num_ipa_offload_connection);

	return ipa3_check_qmi_response(rc,
		QMI_IPA_REMOVE_OFFLOAD_CONNECTION_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_rmv_offload_connection");
}

/* sending ul-filter-install-request to modem*/
int ipa3_qmi_ul_filter_request_send(
	struct ipa_configure_ul_firewall_rules_req_msg_v01 *req)
{
	struct ipa_configure_ul_firewall_rules_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc, i;

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

	/* check if modem is up */
	if (!ipa3_qmi_indication_fin ||
		!ipa3_qmi_modem_init_fin ||
		!ipa_q6_clnt) {
		IPAWANDBG("modem QMI service is not up yet\n");
		return -EINVAL;
	}

	/* Passing 0 rules means that firewall is disabled */
	if (req->firewall_rules_list_len == 0)
		IPAWANDBG("IPACM passed 0 rules to Q6\n");

	if (req->firewall_rules_list_len >= QMI_IPA_MAX_UL_FIREWALL_RULES_V01) {
		IPAWANERR(
		"Number of rules passed by IPACM, %d, exceed limit %d\n",
			req->firewall_rules_list_len,
			QMI_IPA_MAX_UL_FIREWALL_RULES_V01);
		return -EINVAL;
	}

	/* Check for valid IP type */
	for (i = 0; i < req->firewall_rules_list_len; i++) {
		if (req->firewall_rules_list[i].ip_type !=
				QMI_IPA_IP_TYPE_V4_V01 &&
			req->firewall_rules_list[i].ip_type !=
				QMI_IPA_IP_TYPE_V6_V01) {
			IPAWANERR("Invalid IP type %d\n",
					req->firewall_rules_list[i].ip_type);
			return -EINVAL;
		}
	}

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
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);
	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
			QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_V01,
			rc);
		return rc;
	}

	return ipa3_check_qmi_response(rc,
		QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_V01,
		resp.resp.result,
		resp.resp.error, "ipa_received_ul_firewall_filter");
}

int ipa3_qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req)
{
	struct ipa_enable_force_clear_datapath_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;

	if (!req || !req->source_pipe_bitmask) {
		IPAWANERR("invalid params\n");
		return -EINVAL;
	}

	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
		ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		IPAWANDBG("Simulating success on emu/virt mode\n");
		return 0;
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

	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
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

	return ipa3_check_qmi_response(rc,
		QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_V01,
		resp.resp.result,
		resp.resp.error, "ipa_enable_force_clear_datapath");
}

int ipa3_qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req)
{
	struct ipa_disable_force_clear_datapath_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;


	if (!req) {
		IPAWANERR("invalid params\n");
		return -EINVAL;
	}

	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
		ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		IPAWANDBG("Simulating success on emu/virt mode\n");
		return 0;
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
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
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

	return ipa3_check_qmi_response(rc,
		QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_V01,
		resp.resp.result,
		resp.resp.error, "ipa_disable_force_clear_datapath");
}

/* sending filter-installed-notify-request to modem*/
int ipa3_qmi_filter_notify_send(
		struct ipa_fltr_installed_notif_req_msg_v01 *req)
{
	struct ipa_fltr_installed_notif_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;

	/* check if the filter rules from IPACM is valid */
	if (req->rule_id_len == 0) {
		IPAWANDBG(" delete UL filter rule for pipe %d\n",
		req->source_pipe_index);
	} else if (req->rule_id_len > QMI_IPA_MAX_FILTERS_V01) {
		IPAWANERR(" UL filter rule for pipe %d exceed max (%u)\n",
		req->source_pipe_index,
		req->rule_id_len);
		return -EINVAL;
	}

	if (req->rule_id_ex_len == 0) {
		IPAWANDBG(" delete UL filter rule for pipe %d\n",
		req->source_pipe_index);
	} else if (req->rule_id_ex_len > QMI_IPA_MAX_FILTERS_EX2_V01) {
		IPAWANERR(" UL filter rule for pipe %d exceed max (%u)\n",
		req->source_pipe_index,
		req->rule_id_ex_len);
		return -EINVAL;
	}

	if (req->install_status != IPA_QMI_RESULT_SUCCESS_V01) {
		IPAWANERR(" UL filter rule for pipe %d install_status = %d\n",
			req->source_pipe_index, req->install_status);
		return -EINVAL;
	} else if ((req->rule_id_valid != 1) &&
		(req->rule_id_ex_valid != 1)) {
		IPAWANERR(" UL filter rule for pipe %d rule_id_valid = %d/%d\n",
			req->source_pipe_index, req->rule_id_valid,
			req->rule_id_ex_valid);
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

	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
			QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01,
			rc);
		return rc;
	}

	return ipa3_check_qmi_response(rc,
		QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_fltr_installed_notif_resp");
}

static void ipa3_q6_clnt_quota_reached_ind_cb(struct qmi_handle *handle,
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
		qmi_ind->apn.mux_id, (unsigned long) qmi_ind->apn.num_Mbytes);
	ipa3_broadcast_quota_reach_ind(qmi_ind->apn.mux_id,
		IPA_UPSTEAM_MODEM);
}

static void ipa3_q6_clnt_install_firewall_rules_ind_cb(
	struct qmi_handle *handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *data)
{
	struct ipa_configure_ul_firewall_rules_ind_msg_v01 qmi_ul_firewall_ind;

	memset(&qmi_ul_firewall_ind, 0, sizeof(
		struct ipa_configure_ul_firewall_rules_ind_msg_v01));
	memcpy(&qmi_ul_firewall_ind, data, sizeof(
		struct ipa_configure_ul_firewall_rules_ind_msg_v01));

	IPAWANDBG("UL firewall rules install indication on Q6");
	if (qmi_ul_firewall_ind.result.is_success ==
		QMI_IPA_UL_FIREWALL_STATUS_SUCCESS_V01) {
		IPAWANDBG(" : Success\n");
		IPAWANDBG
		("Mux ID : %d\n", qmi_ul_firewall_ind.result.mux_id);
	} else if (qmi_ul_firewall_ind.result.is_success ==
		QMI_IPA_UL_FIREWALL_STATUS_FAILURE_V01) {
		IPAWANERR(": Failure\n");
	} else {
		IPAWANERR(": Unexpected Result");
	}
}

static void ipa3_q6_clnt_bw_vhang_ind_cb(struct qmi_handle *handle,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *data)
{
	struct ipa_bw_change_ind_msg_v01 *qmi_ind;
	uint32_t bw_mbps = 0;

	if (handle != ipa_q6_clnt) {
		IPAWANERR("Wrong client\n");
		return;
	}

	qmi_ind = (struct ipa_bw_change_ind_msg_v01 *) data;

	IPAWANDBG("Q6 BW change UL valid(%d):(%d)Kbps\n",
		qmi_ind->peak_bw_ul_valid,
		qmi_ind->peak_bw_ul);

	IPAWANDBG("Q6 BW change DL valid(%d):(%d)Kbps\n",
		qmi_ind->peak_bw_dl_valid,
		qmi_ind->peak_bw_dl);

	if (qmi_ind->peak_bw_ul_valid)
		bw_mbps += qmi_ind->peak_bw_ul/1000;

	if (qmi_ind->peak_bw_dl_valid)
		bw_mbps += qmi_ind->peak_bw_dl/1000;

	IPAWANDBG("vote modem BW (%u)\n", bw_mbps);
	if (ipa3_vote_for_bus_bw(&bw_mbps)) {
		IPAWANERR("Failed to vote BW (%u)\n", bw_mbps);
	}
}

static void ipa3_q6_clnt_svc_arrive(struct work_struct *work)
{
	int rc;
	struct ipa_master_driver_init_complt_ind_msg_v01 ind;

	rc = kernel_connect(ipa_q6_clnt->sock,
		(struct sockaddr *) &ipa3_qmi_ctx->server_sq,
		sizeof(ipa3_qmi_ctx->server_sq),
		0);

	if (rc < 0) {
		IPAWANERR("Couldnt connect Server\n");
		return;
	}

	if (!send_qmi_init_q6)
		return;

	IPAWANDBG("Q6 QMI service available now\n");
	if (ipa3_is_apq()) {
		ipa3_qmi_modem_init_fin = true;
		IPAWANDBG("QMI-client complete, ipa3_qmi_modem_init_fin : %d\n",
			ipa3_qmi_modem_init_fin);
		return;
	}

	/* Initialize modem IPA-driver */
	IPAWANDBG("send ipa3_qmi_init_modem_send_sync_msg to modem\n");
	rc = ipa3_qmi_init_modem_send_sync_msg();
	if ((rc == -ENETRESET) || (rc == -ENODEV)) {
		IPAWANERR(
		"ipa3_qmi_init_modem_send_sync_msg failed due to SSR!\n");
		/* Cleanup when ipa3_wwan_remove is called */
		vfree(ipa_q6_clnt);
		ipa_q6_clnt = NULL;
		return;
	}

	if (rc != 0) {
		IPAWANERR("ipa3_qmi_init_modem_send_sync_msg failed\n");
		/*
		 * Hardware not responding.
		 * This is a very unexpected scenario
		 * which requires a kernel panic in
		 * order to force dumps for QMI/Q6 side analysis.
		 */
		BUG();
	}
	ipa3_qmi_modem_init_fin = true;

	/* In cold-bootup, first_time_handshake = false */
	ipa3_q6_handshake_complete(first_time_handshake);
	first_time_handshake = true;
	IPAWANDBG("complete, ipa3_qmi_modem_init_fin : %d\n",
		ipa3_qmi_modem_init_fin);

	if (ipa3_qmi_indication_fin) {
		IPAWANDBG("send indication to modem (%d)\n",
		ipa3_qmi_indication_fin);
		memset(&ind, 0, sizeof(struct
			ipa_master_driver_init_complt_ind_msg_v01));
		ind.master_driver_init_status.result =
			IPA_QMI_RESULT_SUCCESS_V01;

		rc = qmi_send_indication(ipa3_svc_handle,
			&ipa3_qmi_ctx->client_sq,
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01,
			QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01,
			ipa3_master_driver_init_complt_ind_msg_data_v01_ei,
			&ind);

		IPAWANDBG("ipa_qmi_service_client good\n");
	} else {
		IPAWANERR("not send indication (%d)\n",
		ipa3_qmi_indication_fin);
	}

	send_qmi_init_q6 = false;

}

static void ipa3_q6_clnt_svc_exit(struct work_struct *work)
{
	if (ipa3_qmi_ctx != NULL) {
		ipa3_qmi_ctx->server_sq.sq_family = 0;
		ipa3_qmi_ctx->server_sq.sq_node = 0;
		ipa3_qmi_ctx->server_sq.sq_port = 0;
	}
}

static int ipa3_q6_clnt_svc_event_notify_svc_new(struct qmi_handle *qmi,
	struct qmi_service *service)
{
	IPAWANDBG("QMI svc:%d vers:%d ins:%d node:%d port:%d\n",
		  service->service, service->version, service->instance,
		  service->node, service->port);

	if (ipa3_qmi_ctx != NULL) {
		ipa3_qmi_ctx->server_sq.sq_family = AF_QIPCRTR;
		ipa3_qmi_ctx->server_sq.sq_node = service->node;
		ipa3_qmi_ctx->server_sq.sq_port = service->port;
	}
	if (!workqueues_stopped) {
		queue_delayed_work(ipa_clnt_req_workqueue,
			&ipa3_work_svc_arrive, 0);
	}
	return 0;
}

static void ipa3_q6_clnt_svc_event_notify_net_reset(struct qmi_handle *qmi)
{
	if (!workqueues_stopped)
		queue_delayed_work(ipa_clnt_req_workqueue,
			&ipa3_work_svc_exit, 0);
}

static void ipa3_q6_clnt_svc_event_notify_svc_exit(struct qmi_handle *qmi,
						   struct qmi_service *svc)
{
	IPAWANDBG("QMI svc:%d vers:%d ins:%d node:%d port:%d\n", svc->service,
		  svc->version, svc->instance, svc->node, svc->port);

	if (!workqueues_stopped)
		queue_delayed_work(ipa_clnt_req_workqueue,
			&ipa3_work_svc_exit, 0);
}

static struct qmi_ops server_ops = {
	.del_client = ipa3_a5_svc_disconnect_cb,
};

static struct qmi_ops client_ops = {
	.new_server = ipa3_q6_clnt_svc_event_notify_svc_new,
	.del_server = ipa3_q6_clnt_svc_event_notify_svc_exit,
	.net_reset = ipa3_q6_clnt_svc_event_notify_net_reset,
};

static struct qmi_msg_handler server_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_INDICATION_REGISTER_REQ_V01,
		.ei = ipa3_indication_reg_req_msg_data_v01_ei,
		.decoded_size = sizeof(struct ipa_indication_reg_req_msg_v01),
		.fn = ipa3_handle_indication_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_INSTALL_FILTER_RULE_REQ_V01,
		.ei = ipa3_install_fltr_rule_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_install_fltr_rule_req_msg_v01),
		.fn = ipa3_handle_install_filter_rule_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01,
		.ei = ipa3_fltr_installed_notif_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_fltr_installed_notif_req_msg_v01),
		.fn = ipa3_handle_filter_installed_notify_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_CONFIG_REQ_V01,
		.ei = ipa3_config_req_msg_data_v01_ei,
		.decoded_size = sizeof(struct ipa_config_req_msg_v01),
		.fn = handle_ipa_config_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01,
		.ei = ipa3_init_modem_driver_cmplt_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_init_modem_driver_cmplt_req_msg_v01),
		.fn = ipa3_handle_modem_init_cmplt_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01,
		.ei = ipa3_init_modem_driver_cmplt_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_init_modem_driver_cmplt_req_msg_v01),
		.fn = ipa3_handle_modem_init_cmplt_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_MHI_ALLOC_CHANNEL_REQ_V01,
		.ei = ipa_mhi_alloc_channel_req_msg_v01_ei,
		.decoded_size = sizeof(
			struct ipa_mhi_alloc_channel_req_msg_v01),
		.fn = ipa3_handle_mhi_alloc_channel_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_IPA_MHI_CLK_VOTE_REQ_V01,
		.ei = ipa_mhi_clk_vote_req_msg_v01_ei,
		.decoded_size = sizeof(struct ipa_mhi_clk_vote_req_msg_v01),
		.fn = ipa3_handle_mhi_vote_req,
	},

};

/*  clinet_handlers are client callbacks that will be called from QMI context
 *  when an indication from Q6 server arrives.
 *  In our case, client_handlers needs handling only for QMI_INDICATION,
 *  since the QMI_REQUEST/ QMI_RESPONSE are handled in a blocking fashion
 *  at the time of sending QMI_REQUESTs.
 */
static struct qmi_msg_handler client_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_V01,
		.ei = ipa3_data_usage_quota_reached_ind_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_data_usage_quota_reached_ind_msg_v01),
		.fn = ipa3_q6_clnt_quota_reached_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_IPA_INSTALL_UL_FIREWALL_RULES_IND_V01,
		.ei = ipa3_install_fltr_rule_req_msg_data_v01_ei,
		.decoded_size = sizeof(
			struct ipa_configure_ul_firewall_rules_ind_msg_v01),
		.fn = ipa3_q6_clnt_install_firewall_rules_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_IPA_BW_CHANGE_INDICATION_V01,
		.ei = ipa_bw_change_ind_msg_v01_ei,
		.decoded_size = sizeof(struct ipa_bw_change_ind_msg_v01),
		.fn = ipa3_q6_clnt_bw_vhang_ind_cb,
	},
};


static void ipa3_qmi_service_init_worker(struct work_struct *work)
{
	int rc;

	/* start the QMI msg cache */
	ipa3_qmi_ctx = vzalloc(sizeof(*ipa3_qmi_ctx));
	if (!ipa3_qmi_ctx) {
		IPAWANERR("Failed to allocate ipa3_qmi_ctx\n");
		return;
	}

	if (ipa3_is_apq()) {
		/* Only start QMI-client */
		IPAWANDBG("Only start IPA A7 QMI client\n");
		goto qmi_client_start;
	}

	/* Initialize QMI-service*/
	IPAWANDBG("IPA A7 QMI init OK :>>>>\n");

	ipa3_qmi_ctx->modem_cfg_emb_pipe_flt =
		ipa3_get_modem_cfg_emb_pipe_flt();

	ipa3_qmi_ctx->num_ipa_offload_connection = 0;
	ipa3_svc_handle = vzalloc(sizeof(*ipa3_svc_handle));

	if (!ipa3_svc_handle)
		goto destroy_ipa_A7_svc_wq;

	rc = qmi_handle_init(ipa3_svc_handle,
		QMI_IPA_MAX_MSG_LEN,
		&server_ops,
		server_handlers);

	if (rc < 0) {
		IPAWANERR("Initializing ipa_a5 svc failed %d\n", rc);
		goto destroy_qmi_handle;
	}

	rc = qmi_add_server(ipa3_svc_handle,
		IPA_A5_SERVICE_SVC_ID,
		IPA_A5_SVC_VERS,
		IPA_A5_SERVICE_INS_ID);

	if (rc < 0) {
		IPAWANERR("Registering ipa_a5 svc failed %d\n",
				rc);
		goto deregister_qmi_srv;
	}

qmi_client_start:
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
	if (!ipa3_is_apq())
		qmi_handle_release(ipa3_svc_handle);
destroy_qmi_handle:
	vfree(ipa3_qmi_ctx);
destroy_ipa_A7_svc_wq:
	if (!ipa3_is_apq()) {
		vfree(ipa3_svc_handle);
		ipa3_svc_handle = NULL;
	}
	ipa3_qmi_ctx = NULL;
}

int ipa3_qmi_service_init(uint32_t wan_platform_type)
{
	ipa_wan_platform = wan_platform_type;
	ipa3_qmi_modem_init_fin = false;
	ipa3_qmi_indication_fin = false;
	ipa3_modem_init_cmplt = false;
	send_qmi_init_q6 = true;
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

	workqueues_stopped = true;

	/* qmi-service */
	if (ipa3_svc_handle != NULL) {
		qmi_handle_release(ipa3_svc_handle);
		vfree(ipa3_svc_handle);
		ipa3_svc_handle = NULL;
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
	mutex_lock(&ipa3_qmi_lock);
	if (ipa3_qmi_ctx != NULL) {
		vfree(ipa3_qmi_ctx);
		ipa3_qmi_ctx = NULL;
	}
	mutex_unlock(&ipa3_qmi_lock);

	ipa3_qmi_modem_init_fin = false;
	ipa3_qmi_indication_fin = false;
	ipa3_modem_init_cmplt = false;
	send_qmi_init_q6 = true;
}

void ipa3_qmi_stop_workqueues(void)
{
	IPAWANDBG("Stopping all QMI workqueues\n");

	/* Stopping all workqueues so new work won't be scheduled */
	workqueues_stopped = true;

	/* Making sure that the current scheduled work won't be executed */
	cancel_delayed_work(&ipa3_work_svc_arrive);
	cancel_delayed_work(&ipa3_work_svc_exit);
}

/* voting for bus BW to ipa_rm*/
int ipa3_vote_for_bus_bw(uint32_t *bw_mbps)
{
	int ret;

	IPAWANDBG("Bus BW is %d\n", *bw_mbps);

	if (bw_mbps == NULL) {
		IPAWANERR("Bus BW is invalid\n");
		return -EINVAL;
	}

	ret = ipa3_wwan_set_modem_perf_profile(*bw_mbps);
	if (ret)
		IPAWANERR("Failed to set perf profile to BW %u\n",
			*bw_mbps);
	else
		IPAWANDBG("Succeeded to set perf profile to BW %u\n",
			*bw_mbps);

	return ret;
}

int ipa3_qmi_get_data_stats(struct ipa_get_data_stats_req_msg_v01 *req,
			   struct ipa_get_data_stats_resp_msg_v01 *resp)
{
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_DATA_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_DATA_STATS_REQ_V01;
	req_desc.ei_array = ipa3_get_data_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_GET_DATA_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_DATA_STATS_RESP_V01;
	resp_desc.ei_array = ipa3_get_data_stats_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_GET_DATA_STATS_REQ_V01\n");

	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_GET_DATA_STATS_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_GET_DATA_STATS_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_GET_DATA_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa_get_data_stats_resp_msg_v01");
}

int ipa3_qmi_get_network_stats(struct ipa_get_apn_data_stats_req_msg_v01 *req,
			      struct ipa_get_apn_data_stats_resp_msg_v01 *resp)
{
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_APN_DATA_STATS_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_APN_DATA_STATS_REQ_V01;
	req_desc.ei_array = ipa3_get_apn_data_stats_req_msg_data_v01_ei;

	resp_desc.max_msg_len = QMI_IPA_GET_APN_DATA_STATS_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_APN_DATA_STATS_RESP_V01;
	resp_desc.ei_array = ipa3_get_apn_data_stats_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_GET_APN_DATA_STATS_REQ_V01\n");

	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_GET_APN_DATA_STATS_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_GET_APN_DATA_STATS_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_GET_APN_DATA_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa_get_apn_data_stats_req_msg_v01");
}

int ipa3_qmi_set_data_quota(struct ipa_set_data_usage_quota_req_msg_v01 *req)
{
	struct ipa_set_data_usage_quota_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
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
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_set_data_usage_quota_req_msg_v01");
}

int ipa3_qmi_set_aggr_info(enum ipa_aggr_enum_type_v01 aggr_enum_type)
{
	struct ipa_mhi_prime_aggr_info_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	IPAWANDBG("sending aggr_info_request\n");

	/* replace to right qmap format */
	aggr_req.aggr_info[1].aggr_type = aggr_enum_type;
	aggr_req.aggr_info[2].aggr_type = aggr_enum_type;
	aggr_req.aggr_info[3].aggr_type = aggr_enum_type;
	aggr_req.aggr_info[4].aggr_type = aggr_enum_type;

	memset(&resp, 0, sizeof(struct ipa_mhi_prime_aggr_info_resp_msg_v01));

	req_desc.max_msg_len = IPA_MHI_PRIME_AGGR_INFO_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_IPA_MHI_PRIME_AGGR_INFO_REQ_V01;
	req_desc.ei_array = ipa_mhi_prime_aggr_info_req_msg_v01_ei;

	resp_desc.max_msg_len =
		IPA_MHI_PRIME_AGGR_INFO_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_IPA_MHI_PRIME_AGGR_INFO_RESP_V01;
	resp_desc.ei_array = ipa_mhi_prime_aggr_info_resp_msg_v01_ei;

	IPAWANDBG("Sending QMI_IPA_MHI_PRIME_AGGR_INFO_REQ_V01(%d)\n",
		aggr_enum_type);
	if (unlikely(!ipa_q6_clnt)) {
		IPAWANERR(" ipa_q6_clnt not initialized\n");
		return -ETIMEDOUT;
	}
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, &aggr_req,
		&resp_desc, &resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_MHI_PRIME_AGGR_INFO_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_mhi_prime_aggr_info_req_msg_v01");
}

int ipa3_qmi_req_ind(void)
{
	struct ipa_indication_reg_req_msg_v01 req;
	struct ipa_indication_reg_resp_msg_v01 resp;
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	memset(&req, 0, sizeof(struct ipa_indication_reg_req_msg_v01));
	memset(&resp, 0, sizeof(struct ipa_indication_reg_resp_msg_v01));

	req.bw_change_ind_valid = true;
	req.bw_change_ind = true;

	req_desc.max_msg_len =
		QMI_IPA_INDICATION_REGISTER_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_INDICATION_REGISTER_REQ_V01;
	req_desc.ei_array = ipa3_indication_reg_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_INDICATION_REGISTER_RESP_V01;
	resp_desc.ei_array = ipa3_indication_reg_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_INDICATION_REGISTER_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, &req,
		&resp_desc, &resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_INDICATION_REGISTER_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_INDICATION_REGISTER_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_INDICATION_REGISTER_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_indication_reg_req_msg_v01");
}

int ipa3_qmi_stop_data_qouta(void)
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
	req_desc.ei_array = ipa3_stop_data_usage_quota_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01;
	resp_desc.ei_array = ipa3_stop_data_usage_quota_resp_msg_data_v01_ei;

	IPAWANDBG_LOW("Sending QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;
	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, &req,
		&resp_desc, &resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("QMI send Req %d failed, rc= %d\n",
			QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG_LOW("QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_stop_data_usage_quota_req_msg_v01");
}

int ipa3_qmi_enable_per_client_stats(
	struct ipa_enable_per_client_stats_req_msg_v01 *req,
	struct ipa_enable_per_client_stats_resp_msg_v01 *resp)
{
	struct ipa_msg_desc req_desc, resp_desc;
	int rc = 0;

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

	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
			QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG("QMI_IPA_ENABLE_PER_CLIENT_STATS_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_V01, resp->resp.result,
		resp->resp.error, "ipa3_qmi_enable_per_client_stats");
}

int ipa3_qmi_get_per_client_packet_stats(
	struct ipa_get_stats_per_client_req_msg_v01 *req,
	struct ipa_get_stats_per_client_resp_msg_v01 *resp)
{
	struct ipa_msg_desc req_desc, resp_desc;
	int rc;

	req_desc.max_msg_len = QMI_IPA_GET_STATS_PER_CLIENT_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01;
	req_desc.ei_array = ipa3_get_stats_per_client_req_msg_data_v01_ei;

	resp_desc.max_msg_len =
		QMI_IPA_GET_STATS_PER_CLIENT_RESP_MAX_MSG_LEN_V01;
	resp_desc.msg_id = QMI_IPA_GET_STATS_PER_CLIENT_RESP_V01;
	resp_desc.ei_array = ipa3_get_stats_per_client_resp_msg_data_v01_ei;

	IPAWANDBG("Sending QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01\n");

	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, resp,
		QMI_SEND_STATS_REQ_TIMEOUT_MS);

	if (rc < 0) {
		IPAWANERR("send Req %d failed, rc= %d\n",
			QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01,
			rc);
		return rc;
	}

	IPAWANDBG("QMI_IPA_GET_STATS_PER_CLIENT_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01, resp->resp.result,
		resp->resp.error,
		"struct ipa_get_stats_per_client_req_msg_v01");
}

int ipa3_qmi_send_mhi_ready_indication(
	struct ipa_mhi_ready_indication_msg_v01 *req)
{
	IPAWANDBG("Sending QMI_IPA_MHI_READY_IND_V01\n");

	if (unlikely(!ipa3_svc_handle))
		return -ETIMEDOUT;

	return qmi_send_indication(ipa3_svc_handle,
		&ipa3_qmi_ctx->client_sq,
		QMI_IPA_MHI_READY_IND_V01,
		IPA_MHI_READY_INDICATION_MSG_V01_MAX_MSG_LEN,
		ipa_mhi_ready_indication_msg_v01_ei,
		req);
}

int ipa3_qmi_send_mhi_cleanup_request(struct ipa_mhi_cleanup_req_msg_v01 *req)
{

	struct ipa_msg_desc req_desc, resp_desc;
	struct ipa_mhi_cleanup_resp_msg_v01 resp;
	int rc;

	memset(&resp, 0, sizeof(resp));

	IPAWANDBG("Sending QMI_IPA_MHI_CLEANUP_REQ_V01\n");
	if (unlikely(!ipa_q6_clnt))
		return -ETIMEDOUT;

	req_desc.max_msg_len = IPA_MHI_CLK_VOTE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_IPA_MHI_CLEANUP_REQ_V01;
	req_desc.ei_array = ipa_mhi_cleanup_req_msg_v01_ei;

	resp_desc.max_msg_len = IPA_MHI_CLK_VOTE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_IPA_MHI_CLEANUP_RESP_V01;
	resp_desc.ei_array = ipa_mhi_cleanup_resp_msg_v01_ei;

	rc = ipa3_qmi_send_req_wait(ipa_q6_clnt,
		&req_desc, req,
		&resp_desc, &resp,
		QMI_MHI_SEND_REQ_TIMEOUT_MS);

	IPAWANDBG("QMI_IPA_MHI_CLEANUP_RESP_V01 received\n");

	return ipa3_check_qmi_response(rc,
		QMI_IPA_MHI_CLEANUP_REQ_V01, resp.resp.result,
		resp.resp.error, "ipa_mhi_cleanup_req_msg");
}

int ipa3_qmi_send_rsc_pipe_indication(
	struct ipa_endp_desc_indication_msg_v01 *req)
{
	IPAWANDBG("Sending QMI_IPA_ENDP_DESC_INDICATION_V01\n");

	if (unlikely(!ipa3_svc_handle))
		return -ETIMEDOUT;

	return qmi_send_indication(ipa3_svc_handle,
		&ipa3_qmi_ctx->client_sq,
		QMI_IPA_ENDP_DESC_INDICATION_V01,
		IPA_ENDP_DESC_INDICATION_MSG_V01_MAX_MSG_LEN,
		ipa_endp_desc_indication_msg_v01_ei,
		req);
}

void ipa3_qmi_init(void)
{
	mutex_init(&ipa3_qmi_lock);
}

void ipa3_qmi_cleanup(void)
{
	mutex_destroy(&ipa3_qmi_lock);
}

