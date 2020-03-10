// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved. */

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/soc/qcom/qmi.h>

#include "bus.h"
#include "debug.h"
#include "main.h"
#include "qmi.h"

#define WLFW_SERVICE_INS_ID_V01		1
#define WLFW_CLIENT_ID			0x4b4e454c
#define MAX_BDF_FILE_NAME		13
#define BDF_FILE_NAME_PREFIX		"bdwlan"
#define ELF_BDF_FILE_NAME		"bdwlan.elf"
#define ELF_BDF_FILE_NAME_PREFIX	"bdwlan.e"
#define BIN_BDF_FILE_NAME		"bdwlan.bin"
#define BIN_BDF_FILE_NAME_PREFIX	"bdwlan.b"
#define REGDB_FILE_NAME			"regdb.bin"
#define DUMMY_BDF_FILE_NAME		"bdwlan.dmy"

#define QMI_WLFW_TIMEOUT_MS		(plat_priv->ctrl_params.qmi_timeout)
#define QMI_WLFW_TIMEOUT_JF		msecs_to_jiffies(QMI_WLFW_TIMEOUT_MS)
#define COEX_TIMEOUT			QMI_WLFW_TIMEOUT_JF
#define IMS_TIMEOUT                     QMI_WLFW_TIMEOUT_JF

#define QMI_WLFW_MAX_RECV_BUF_SIZE	SZ_8K

#define QMI_WLFW_MAC_READY_TIMEOUT_MS	50
#define QMI_WLFW_MAC_READY_MAX_RETRY	200

static char *cnss_qmi_mode_to_str(enum cnss_driver_mode mode)
{
	switch (mode) {
	case CNSS_MISSION:
		return "MISSION";
	case CNSS_FTM:
		return "FTM";
	case CNSS_EPPING:
		return "EPPING";
	case CNSS_WALTEST:
		return "WALTEST";
	case CNSS_OFF:
		return "OFF";
	case CNSS_CCPM:
		return "CCPM";
	case CNSS_QVIT:
		return "QVIT";
	case CNSS_CALIBRATION:
		return "CALIBRATION";
	default:
		return "UNKNOWN";
	}
};

static int cnss_wlfw_ind_register_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_ind_register_req_msg_v01 *req;
	struct wlfw_ind_register_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->client_id_valid = 1;
	req->client_id = WLFW_CLIENT_ID;
	req->fw_ready_enable_valid = 1;
	req->fw_ready_enable = 1;
	req->request_mem_enable_valid = 1;
	req->request_mem_enable = 1;
	req->fw_mem_ready_enable_valid = 1;
	req->fw_mem_ready_enable = 1;
	req->fw_init_done_enable_valid = 1;
	req->fw_init_done_enable = 1;
	req->pin_connect_result_enable_valid = 1;
	req->pin_connect_result_enable = 1;
	req->cal_done_enable_valid = 1;
	req->cal_done_enable = 1;
	req->qdss_trace_req_mem_enable_valid = 1;
	req->qdss_trace_req_mem_enable = 1;
	req->qdss_trace_save_enable_valid = 1;
	req->qdss_trace_save_enable = 1;
	req->qdss_trace_free_enable_valid = 1;
	req->qdss_trace_free_enable = 1;
	req->respond_get_info_enable_valid = 1;
	req->respond_get_info_enable = 1;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_ind_register_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for indication register request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_IND_REGISTER_REQ_V01,
			       WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ind_register_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send indication register request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of indication register request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Indication register request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (resp->fw_status_valid) {
		if (resp->fw_status & QMI_WLFW_ALREADY_REGISTERED_V01) {
			ret = -EALREADY;
			goto qmi_registered;
		}
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_ASSERT(0);

qmi_registered:
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_wlfw_host_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_host_cap_req_msg_v01 *req;
	struct wlfw_host_cap_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending host capability message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->num_clients_valid = 1;
	if (test_bit(ENABLE_DAEMON_SUPPORT,
		     &plat_priv->ctrl_params.quirks))
		req->num_clients = 2;
	else
		req->num_clients = 1;
	cnss_pr_dbg("Number of clients is %d\n", req->num_clients);

	req->wake_msi = cnss_bus_get_wake_irq(plat_priv);
	if (req->wake_msi) {
		cnss_pr_dbg("WAKE MSI base data is %d\n", req->wake_msi);
		req->wake_msi_valid = 1;
	}

	req->bdf_support_valid = 1;
	req->bdf_support = 1;

	req->m3_support_valid = 1;
	req->m3_support = 1;

	req->m3_cache_support_valid = 1;
	req->m3_cache_support = 1;

	req->cal_done_valid = 1;
	req->cal_done = plat_priv->cal_done;
	cnss_pr_dbg("Calibration done is %d\n", plat_priv->cal_done);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_host_cap_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_HOST_CAP_REQ_V01,
			       WLFW_HOST_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_host_cap_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of host capability request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Host capability request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_respond_mem_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_respond_mem_req_msg_v01 *req;
	struct wlfw_respond_mem_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret = 0, i;

	cnss_pr_dbg("Sending respond memory message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mem_seg_len = plat_priv->fw_mem_seg_len;
	for (i = 0; i < req->mem_seg_len; i++) {
		if (!fw_mem[i].pa || !fw_mem[i].size) {
			if (fw_mem[i].type == 0) {
				cnss_pr_err("Invalid memory for FW type, segment = %d\n",
					    i);
				ret = -EINVAL;
				goto out;
			}
			cnss_pr_err("Memory for FW is not available for type: %u\n",
				    fw_mem[i].type);
			ret = -ENOMEM;
			goto out;
		}

		cnss_pr_dbg("Memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
			    fw_mem[i].va, &fw_mem[i].pa,
			    fw_mem[i].size, fw_mem[i].type);

		req->mem_seg[i].addr = fw_mem[i].pa;
		req->mem_seg[i].size = fw_mem[i].size;
		req->mem_seg[i].type = fw_mem[i].type;
	}

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_respond_mem_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_RESPOND_MEM_REQ_V01,
			       WLFW_RESPOND_MEM_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_respond_mem_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Respond memory request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_tgt_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_cap_req_msg_v01 *req;
	struct wlfw_cap_resp_msg_v01 *resp;
	struct qmi_txn txn;
	char *fw_build_timestamp;
	int ret = 0;

	cnss_pr_dbg("Sending target capability message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_cap_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for target capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_CAP_REQ_V01,
			       WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_cap_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send respond target capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of target capability request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Target capability request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (resp->chip_info_valid) {
		plat_priv->chip_info.chip_id = resp->chip_info.chip_id;
		plat_priv->chip_info.chip_family = resp->chip_info.chip_family;
	}
	if (resp->board_info_valid)
		plat_priv->board_info.board_id = resp->board_info.board_id;
	else
		plat_priv->board_info.board_id = 0xFF;
	if (resp->soc_info_valid)
		plat_priv->soc_info.soc_id = resp->soc_info.soc_id;
	if (resp->fw_version_info_valid) {
		plat_priv->fw_version_info.fw_version =
			resp->fw_version_info.fw_version;
		fw_build_timestamp = resp->fw_version_info.fw_build_timestamp;
		fw_build_timestamp[QMI_WLFW_MAX_TIMESTAMP_LEN] = '\0';
		strlcpy(plat_priv->fw_version_info.fw_build_timestamp,
			resp->fw_version_info.fw_build_timestamp,
			QMI_WLFW_MAX_TIMESTAMP_LEN + 1);
	}
	if (resp->fw_build_id_valid) {
		resp->fw_build_id[QMI_WLFW_MAX_BUILD_ID_LEN] = '\0';
		strlcpy(plat_priv->fw_build_id, resp->fw_build_id,
			QMI_WLFW_MAX_BUILD_ID_LEN + 1);
	}
	if (resp->voltage_mv_valid) {
		plat_priv->cpr_info.voltage = resp->voltage_mv;
		cnss_pr_dbg("Voltage for CPR: %dmV\n",
			    plat_priv->cpr_info.voltage);
		cnss_update_cpr_info(plat_priv);
	}
	if (resp->time_freq_hz_valid) {
		plat_priv->device_freq_hz = resp->time_freq_hz;
		cnss_pr_dbg("Device frequency is %d HZ\n",
			    plat_priv->device_freq_hz);
	}
	if (resp->otp_version_valid)
		plat_priv->otp_version = resp->otp_version;

	cnss_pr_dbg("Target capability: chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s, fw_build_id: %s, otp_version: 0x%x\n",
		    plat_priv->chip_info.chip_id,
		    plat_priv->chip_info.chip_family,
		    plat_priv->board_info.board_id, plat_priv->soc_info.soc_id,
		    plat_priv->fw_version_info.fw_version,
		    plat_priv->fw_version_info.fw_build_timestamp,
		    plat_priv->fw_build_id,
		    plat_priv->otp_version);

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_get_bdf_file_name(struct cnss_plat_data *plat_priv,
				  u32 bdf_type, char *filename,
				  u32 filename_len)
{
	int ret = 0;

	switch (bdf_type) {
	case CNSS_BDF_ELF:
		if (plat_priv->board_info.board_id == 0xFF)
			snprintf(filename, filename_len, ELF_BDF_FILE_NAME);
		else if (plat_priv->board_info.board_id < 0xFF)
			snprintf(filename, filename_len,
				 ELF_BDF_FILE_NAME_PREFIX "%02x",
				 plat_priv->board_info.board_id);
		else
			snprintf(filename, filename_len,
				 BDF_FILE_NAME_PREFIX "%02x.e%02x",
				 plat_priv->board_info.board_id >> 8 & 0xFF,
				 plat_priv->board_info.board_id & 0xFF);
		break;
	case CNSS_BDF_BIN:
		if (plat_priv->board_info.board_id == 0xFF)
			snprintf(filename, filename_len, BIN_BDF_FILE_NAME);
		else if (plat_priv->board_info.board_id < 0xFF)
			snprintf(filename, filename_len,
				 BIN_BDF_FILE_NAME_PREFIX "%02x",
				 plat_priv->board_info.board_id);
		else
			snprintf(filename, filename_len,
				 BDF_FILE_NAME_PREFIX "%02x.b%02x",
				 plat_priv->board_info.board_id >> 8 & 0xFF,
				 plat_priv->board_info.board_id & 0xFF);
		break;
	case CNSS_BDF_REGDB:
		snprintf(filename, filename_len, REGDB_FILE_NAME);
		break;
	case CNSS_BDF_DUMMY:
		cnss_pr_dbg("CNSS_BDF_DUMMY is set, sending dummy BDF\n");
		snprintf(filename, filename_len, DUMMY_BDF_FILE_NAME);
		ret = MAX_BDF_FILE_NAME;
		break;
	default:
		cnss_pr_err("Invalid BDF type: %d\n",
			    plat_priv->ctrl_params.bdf_type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv,
				 u32 bdf_type)
{
	struct wlfw_bdf_download_req_msg_v01 *req;
	struct wlfw_bdf_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	char filename[MAX_BDF_FILE_NAME];
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	unsigned int remaining;
	int ret = 0;

	cnss_pr_dbg("Sending BDF download message, state: 0x%lx, type: %d\n",
		    plat_priv->driver_state, bdf_type);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = cnss_get_bdf_file_name(plat_priv, bdf_type,
				     filename, sizeof(filename));
	if (ret > 0) {
		temp = DUMMY_BDF_FILE_NAME;
		remaining = MAX_BDF_FILE_NAME;
		goto bypass_bdf;
	} else if (ret < 0) {
		goto err_req_fw;
	}

	ret = request_firmware(&fw_entry, filename, &plat_priv->plat_dev->dev);
	if (ret) {
		cnss_pr_err("Failed to load BDF: %s\n", filename);
		goto err_req_fw;
	}

	temp = fw_entry->data;
	remaining = fw_entry->size;

bypass_bdf:
	cnss_pr_dbg("Downloading BDF: %s, size: %u\n", filename, remaining);

	while (remaining) {
		req->valid = 1;
		req->file_id_valid = 1;
		req->file_id = plat_priv->board_info.board_id;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;
		req->bdf_type_valid = 1;
		req->bdf_type = bdf_type;

		if (remaining > QMI_WLFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				   wlfw_bdf_download_resp_msg_v01_ei, resp);
		if (ret < 0) {
			cnss_pr_err("Failed to initialize txn for BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_send_request
			(&plat_priv->qmi_wlfw, NULL, &txn,
			 QMI_WLFW_BDF_DOWNLOAD_REQ_V01,
			 WLFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_bdf_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Failed to send respond BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
		if (ret < 0) {
			cnss_pr_err("Failed to wait for response of BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("BDF download request failed, result: %d, err: %d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			goto err_send;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

	if (bdf_type != CNSS_BDF_DUMMY)
		release_firmware(fw_entry);

	kfree(req);
	kfree(resp);
	return 0;

err_send:
	if (bdf_type != CNSS_BDF_DUMMY)
		release_firmware(fw_entry);
err_req_fw:
	if (bdf_type != CNSS_BDF_REGDB)
		CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_m3_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_m3_info_req_msg_v01 *req;
	struct wlfw_m3_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;
	int ret = 0;

	cnss_pr_dbg("Sending M3 information message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	if (!m3_mem->pa || !m3_mem->size) {
		cnss_pr_err("Memory for M3 is not available\n");
		ret = -ENOMEM;
		goto out;
	}

	cnss_pr_dbg("M3 memory, va: 0x%pK, pa: %pa, size: 0x%zx\n",
		    m3_mem->va, &m3_mem->pa, m3_mem->size);

	req->addr = plat_priv->m3_mem.pa;
	req->size = plat_priv->m3_mem.size;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_m3_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_M3_INFO_REQ_V01,
			       WLFW_M3_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_m3_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("M3 information request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_wlfw_wlan_mac_req_send_sync(struct cnss_plat_data *plat_priv,
					    u8 *mac, u32 mac_len)
{
	struct wlfw_mac_addr_req_msg_v01 *req;
	struct wlfw_mac_addr_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret;
	u8 is_query;

	if (!plat_priv)
		return -ENODEV;

	/* NULL mac && zero mac_len means querying the status of MAC in FW */
	if ((mac && mac_len != QMI_WLFW_MAC_ADDR_SIZE_V01) ||
	    (!mac && mac_len != 0))
		return -EINVAL;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_mac_addr_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for mac req, err: %d\n",
			    ret);
		ret = -EIO;
		goto out;
	}

	is_query = !mac;
	if (!is_query) {
		/* DO NOT print this for mac query, that might be too many */
		cnss_pr_dbg("Sending WLAN mac req [%pM], state: 0x%lx\n",
			    mac, plat_priv->driver_state);
		memcpy(req->mac_addr, mac, mac_len);

		/* 0 - query status of wlfw MAC; 1 - set wlfw MAC */
		req->mac_addr_valid = 1;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_MAC_ADDR_REQ_V01,
			       WLFW_MAC_ADDR_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_mac_addr_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send mac req, err: %d\n", ret);

		ret = -EIO;
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for resp of mac req, err: %d\n",
			    ret);

		ret = -EIO;
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WLAN mac req failed, result: %d, err: %d\n",
			    resp->resp.result);

		ret = -EIO;
		goto out;
	}

	if (resp->resp.error != QMI_ERR_NONE_V01) {
		ret = ((resp->resp.error == QMI_ERR_NETWORK_NOT_READY_V01 &&
			is_query) ? -EAGAIN : -EIO);
		if (ret != -EAGAIN)
			cnss_pr_err("Got error resp for mac req, err: %d\n",
				    resp->resp.error);
		goto out;
	}

	cnss_pr_dbg("WLAN mac req completed\n");

out:
	kfree(req);
	kfree(resp);
	return ret;
}

static void cnss_wait_for_wlfw_mac_ready(struct cnss_plat_data *plat_priv)
{
	int ret, retry = 0;

	if (!plat_priv)
		return;

	cnss_pr_dbg("Checking wlfw mac, state: 0x%lx\n",
		    plat_priv->driver_state);
	do {
		/* query the current status of WLAN MAC */
		ret = cnss_wlfw_wlan_mac_req_send_sync(plat_priv, NULL, 0);
		if (!ret) {
			cnss_pr_dbg("wlfw mac is ready\n");
			break;
		}

		if (ret != -EAGAIN) {
			cnss_pr_err("failed to query wlfw mac, error: %d\n",
				    ret);
			break;
		}

		if (++retry >= QMI_WLFW_MAC_READY_MAX_RETRY) {
			cnss_pr_err("Timeout to wait for wlfw mac ready\n");
			break;
		}

		msleep(QMI_WLFW_MAC_READY_TIMEOUT_MS);
	} while (true);
}

int cnss_wlfw_wlan_mode_send_sync(struct cnss_plat_data *plat_priv,
				  enum cnss_driver_mode mode)
{
	struct wlfw_wlan_mode_req_msg_v01 *req;
	struct wlfw_wlan_mode_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (mode == CNSS_MISSION && plat_priv->use_nv_mac)
		cnss_wait_for_wlfw_mac_ready(plat_priv);

	cnss_pr_dbg("Sending mode message, mode: %s(%d), state: 0x%lx\n",
		    cnss_qmi_mode_to_str(mode), mode, plat_priv->driver_state);

	if (mode == CNSS_OFF &&
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_dbg("Recovery is in progress, ignore mode off request\n");
		return 0;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mode = (enum wlfw_driver_mode_enum_v01)mode;
	req->hw_debug_valid = 1;
	req->hw_debug = 0;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wlan_mode_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WLAN_MODE_REQ_V01,
			       WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_mode_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Mode request failed, mode: %s(%d), result: %d, err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, resp->resp.result,
			    resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	if (mode == CNSS_OFF) {
		cnss_pr_dbg("WLFW service is disconnected while sending mode off request\n");
		ret = 0;
	} else {
		CNSS_ASSERT(0);
	}
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_wlan_cfg_send_sync(struct cnss_plat_data *plat_priv,
				 struct cnss_wlan_enable_cfg *config,
				 const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 *req;
	struct wlfw_wlan_cfg_resp_msg_v01 *resp;
	struct qmi_txn txn;
	u32 i;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending WLAN config message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->host_version_valid = 1;
	strlcpy(req->host_version, host_version,
		QMI_WLFW_MAX_STR_LEN_V01 + 1);

	req->tgt_cfg_valid = 1;
	if (config->num_ce_tgt_cfg > QMI_WLFW_MAX_NUM_CE_V01)
		req->tgt_cfg_len = QMI_WLFW_MAX_NUM_CE_V01;
	else
		req->tgt_cfg_len = config->num_ce_tgt_cfg;
	for (i = 0; i < req->tgt_cfg_len; i++) {
		req->tgt_cfg[i].pipe_num = config->ce_tgt_cfg[i].pipe_num;
		req->tgt_cfg[i].pipe_dir = config->ce_tgt_cfg[i].pipe_dir;
		req->tgt_cfg[i].nentries = config->ce_tgt_cfg[i].nentries;
		req->tgt_cfg[i].nbytes_max = config->ce_tgt_cfg[i].nbytes_max;
		req->tgt_cfg[i].flags = config->ce_tgt_cfg[i].flags;
	}

	req->svc_cfg_valid = 1;
	if (config->num_ce_svc_pipe_cfg > QMI_WLFW_MAX_NUM_SVC_V01)
		req->svc_cfg_len = QMI_WLFW_MAX_NUM_SVC_V01;
	else
		req->svc_cfg_len = config->num_ce_svc_pipe_cfg;
	for (i = 0; i < req->svc_cfg_len; i++) {
		req->svc_cfg[i].service_id = config->ce_svc_cfg[i].service_id;
		req->svc_cfg[i].pipe_dir = config->ce_svc_cfg[i].pipe_dir;
		req->svc_cfg[i].pipe_num = config->ce_svc_cfg[i].pipe_num;
	}

	req->shadow_reg_v2_valid = 1;
	if (config->num_shadow_reg_v2_cfg >
	    QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01)
		req->shadow_reg_v2_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01;
	else
		req->shadow_reg_v2_len = config->num_shadow_reg_v2_cfg;

	memcpy(req->shadow_reg_v2, config->shadow_reg_v2_cfg,
	       sizeof(struct wlfw_shadow_reg_v2_cfg_s_v01)
	       * req->shadow_reg_v2_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wlan_cfg_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WLAN_CFG_REQ_V01,
			       WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_cfg_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WLAN config request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_athdiag_read_send_sync(struct cnss_plat_data *plat_priv,
				     u32 offset, u32 mem_type,
				     u32 data_len, u8 *data)
{
	struct wlfw_athdiag_read_req_msg_v01 *req;
	struct wlfw_athdiag_read_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (!data || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Invalid parameters for athdiag read: data %pK, data_len %u\n",
			    data, data_len);
		return -EINVAL;
	}

	cnss_pr_dbg("athdiag read: state 0x%lx, offset %x, mem_type %x, data_len %u\n",
		    plat_priv->driver_state, offset, mem_type, data_len);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_athdiag_read_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ATHDIAG_READ_REQ_V01,
			       WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_read_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Athdiag read request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (!resp->data_valid || resp->data_len != data_len) {
		cnss_pr_err("athdiag read data is invalid, data_valid = %u, data_len = %u\n",
			    resp->data_valid, resp->data_len);
		ret = -EINVAL;
		goto out;
	}

	memcpy(data, resp->data, resp->data_len);

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_athdiag_write_send_sync(struct cnss_plat_data *plat_priv,
				      u32 offset, u32 mem_type,
				      u32 data_len, u8 *data)
{
	struct wlfw_athdiag_write_req_msg_v01 *req;
	struct wlfw_athdiag_write_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (!data || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Invalid parameters for athdiag write: data %pK, data_len %u\n",
			    data, data_len);
		return -EINVAL;
	}

	cnss_pr_dbg("athdiag write: state 0x%lx, offset %x, mem_type %x, data_len %u, data %pK\n",
		    plat_priv->driver_state, offset, mem_type, data_len, data);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;
	memcpy(req->data, data, data_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_athdiag_write_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ATHDIAG_WRITE_REQ_V01,
			       WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_write_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Athdiag write request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_ini_send_sync(struct cnss_plat_data *plat_priv,
			    u8 fw_log_mode)
{
	struct wlfw_ini_req_msg_v01 *req;
	struct wlfw_ini_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log_mode: %d\n",
		    plat_priv->driver_state, fw_log_mode);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->enablefwlog_valid = 1;
	req->enablefwlog = fw_log_mode;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_ini_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_INI_REQ_V01,
			       WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ini_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Ini request failed, fw_log_mode: %d, result: %d, err: %d\n",
			    fw_log_mode, resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_antenna_switch_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_antenna_switch_req_msg_v01 *req;
	struct wlfw_antenna_switch_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending antenna switch sync request, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_antenna_switch_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for antenna switch request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ANTENNA_SWITCH_REQ_V01,
			       WLFW_ANTENNA_SWITCH_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_antenna_switch_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send antenna switch request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of antenna switch request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Antenna switch request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (resp->antenna_valid)
		plat_priv->antenna = resp->antenna;

	cnss_pr_dbg("Antenna valid: %u, antenna 0x%llx\n",
		    resp->antenna_valid, resp->antenna);

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_antenna_grant_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_antenna_grant_req_msg_v01 *req;
	struct wlfw_antenna_grant_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending antenna grant sync request, state: 0x%lx, grant 0x%llx\n",
		    plat_priv->driver_state, plat_priv->grant);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->grant_valid = 1;
	req->grant = plat_priv->grant;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_antenna_grant_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for antenna grant request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ANTENNA_GRANT_REQ_V01,
			       WLFW_ANTENNA_GRANT_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_antenna_grant_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send antenna grant request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of antenna grant request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Antenna grant request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_qdss_trace_mem_info_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_qdss_trace_mem_info_req_msg_v01 *req;
	struct wlfw_qdss_trace_mem_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int ret = 0;
	int i;

	cnss_pr_dbg("Sending QDSS trace mem info, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mem_seg_len = plat_priv->qdss_mem_seg_len;
	for (i = 0; i < req->mem_seg_len; i++) {
		cnss_pr_dbg("Memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
			    qdss_mem[i].va, &qdss_mem[i].pa,
			    qdss_mem[i].size, qdss_mem[i].type);

		req->mem_seg[i].addr = qdss_mem[i].pa;
		req->mem_seg[i].size = qdss_mem[i].size;
		req->mem_seg[i].type = qdss_mem[i].type;
	}

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_qdss_trace_mem_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for QDSS trace mem request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01,
			       WLFW_QDSS_TRACE_MEM_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_qdss_trace_mem_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send QDSS trace mem info request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Fail to wait for response of QDSS trace mem info request, err %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("QDSS trace mem info request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_wfc_call_status_send_sync(struct cnss_plat_data *plat_priv,
					u32 data_len, const void *data)
{
	struct wlfw_wfc_call_status_req_msg_v01 *req;
	struct wlfw_wfc_call_status_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending WFC call status: state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->wfc_call_status_len = data_len;
	memcpy(req->wfc_call_status, data, req->wfc_call_status_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wfc_call_status_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for WFC call status request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WFC_CALL_STATUS_REQ_V01,
			       WLFW_WFC_CALL_STATUS_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wfc_call_status_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send WFC call status request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Fail to wait for response of WFC call status request, err %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WFC call status request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_dynamic_feature_mask_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_dynamic_feature_mask_req_msg_v01 *req;
	struct wlfw_dynamic_feature_mask_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending dynamic feature mask 0x%llx, state: 0x%lx\n",
		    plat_priv->dynamic_feature,
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mask_valid = 1;
	req->mask = plat_priv->dynamic_feature;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_dynamic_feature_mask_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for dynamic feature mask request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
		(&plat_priv->qmi_wlfw, NULL, &txn,
		 QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01,
		 WLFW_DYNAMIC_FEATURE_MASK_REQ_MSG_V01_MAX_MSG_LEN,
		 wlfw_dynamic_feature_mask_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send dynamic feature mask request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Fail to wait for response of dynamic feature mask request, err %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Dynamic feature mask request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_get_info_send_sync(struct cnss_plat_data *plat_priv, int type,
				 void *cmd, int cmd_len)
{
	struct wlfw_get_info_req_msg_v01 *req;
	struct wlfw_get_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_vdbg("Sending get info message, type: %d, cmd length: %d, state: 0x%lx\n",
		     type, cmd_len, plat_priv->driver_state);

	if (cmd_len > QMI_WLFW_MAX_DATA_SIZE_V01)
		return -EINVAL;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->type = type;
	req->data_len = cmd_len;
	memcpy(req->data, cmd, req->data_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_get_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for get info request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_GET_INFO_REQ_V01,
			       WLFW_GET_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_get_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send get info request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of get info request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Get info request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

unsigned int cnss_get_qmi_timeout(struct cnss_plat_data *plat_priv)
{
	cnss_pr_dbg("QMI timeout is %u ms\n", QMI_WLFW_TIMEOUT_MS);

	return QMI_WLFW_TIMEOUT_MS;
}

static void cnss_wlfw_request_mem_ind_cb(struct qmi_handle *qmi_wlfw,
					 struct sockaddr_qrtr *sq,
					 struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_request_mem_ind_msg_v01 *ind_msg = data;
	int i;

	cnss_pr_dbg("Received QMI WLFW request memory indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	plat_priv->fw_mem_seg_len = ind_msg->mem_seg_len;
	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		cnss_pr_dbg("FW requests for memory, size: 0x%x, type: %u\n",
			    ind_msg->mem_seg[i].size, ind_msg->mem_seg[i].type);
		plat_priv->fw_mem[i].type = ind_msg->mem_seg[i].type;
		plat_priv->fw_mem[i].size = ind_msg->mem_seg[i].size;
		if (plat_priv->fw_mem[i].type == CNSS_MEM_TYPE_DDR)
			plat_priv->fw_mem[i].attrs |=
				DMA_ATTR_FORCE_CONTIGUOUS;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_REQUEST_MEM,
			       0, NULL);
}

static void cnss_wlfw_fw_mem_ready_ind_cb(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW memory ready indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_FW_MEM_READY,
			       0, NULL);
}

static void cnss_wlfw_fw_ready_ind_cb(struct qmi_handle *qmi_wlfw,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_cal_info *cal_info;

	cnss_pr_dbg("Received QMI WLFW FW ready indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cal_info = kzalloc(sizeof(*cal_info), GFP_KERNEL);
	if (!cal_info)
		return;

	cal_info->cal_status = CNSS_CAL_DONE;
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE,
			       0, cal_info);
}

static void cnss_wlfw_fw_init_done_ind_cb(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW initialization done indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_FW_READY, 0, NULL);
}

static void cnss_wlfw_pin_result_ind_cb(struct qmi_handle *qmi_wlfw,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_pin_connect_result_ind_msg_v01 *ind_msg = data;

	cnss_pr_dbg("Received QMI WLFW pin connect result indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (ind_msg->pwr_pin_result_valid)
		plat_priv->pin_result.fw_pwr_pin_result =
		    ind_msg->pwr_pin_result;
	if (ind_msg->phy_io_pin_result_valid)
		plat_priv->pin_result.fw_phy_io_pin_result =
		    ind_msg->phy_io_pin_result;
	if (ind_msg->rf_pin_result_valid)
		plat_priv->pin_result.fw_rf_pin_result = ind_msg->rf_pin_result;

	cnss_pr_dbg("Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		    ind_msg->pwr_pin_result, ind_msg->phy_io_pin_result,
		    ind_msg->rf_pin_result);
}

static void cnss_wlfw_cal_done_ind_cb(struct qmi_handle *qmi_wlfw,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_cal_info *cal_info;

	cnss_pr_dbg("Received QMI WLFW calibration done indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cal_info = kzalloc(sizeof(*cal_info), GFP_KERNEL);
	if (!cal_info)
		return;

	cal_info->cal_status = CNSS_CAL_DONE;
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE,
			       0, cal_info);
}

static void cnss_wlfw_qdss_trace_req_mem_ind_cb(struct qmi_handle *qmi_wlfw,
						struct sockaddr_qrtr *sq,
						struct qmi_txn *txn,
						const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_req_mem_ind_msg_v01 *ind_msg = data;
	int i;

	cnss_pr_dbg("Received QMI WLFW QDSS trace request mem indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (plat_priv->qdss_mem_seg_len) {
		cnss_pr_err("Ignore double allocation for QDSS trace, current len %u\n",
			    plat_priv->qdss_mem_seg_len);
		return;
	}

	plat_priv->qdss_mem_seg_len = ind_msg->mem_seg_len;
	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		cnss_pr_dbg("QDSS requests for memory, size: 0x%x, type: %u\n",
			    ind_msg->mem_seg[i].size, ind_msg->mem_seg[i].type);
		plat_priv->qdss_mem[i].type = ind_msg->mem_seg[i].type;
		plat_priv->qdss_mem[i].size = ind_msg->mem_seg[i].size;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM,
			       0, NULL);
}

static void cnss_wlfw_qdss_trace_save_ind_cb(struct qmi_handle *qmi_wlfw,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *txn,
					     const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_save_ind_msg_v01 *ind_msg = data;
	struct cnss_qmi_event_qdss_trace_save_data *event_data;
	int i = 0;

	cnss_pr_dbg("Received QMI WLFW QDSS trace save indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_pr_dbg("QDSS_trace_save info: source %u, total_size %u, file_name_valid %u, file_name %s\n",
		    ind_msg->source, ind_msg->total_size,
		    ind_msg->file_name_valid, ind_msg->file_name);

	if (ind_msg->source == 1)
		return;

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	if (ind_msg->mem_seg_valid) {
		if (ind_msg->mem_seg_len > QDSS_TRACE_SEG_LEN_MAX) {
			cnss_pr_err("Invalid seg len %u\n",
				    ind_msg->mem_seg_len);
			goto free_event_data;
		}
		cnss_pr_dbg("QDSS_trace_save seg len %u\n",
			    ind_msg->mem_seg_len);
		event_data->mem_seg_len = ind_msg->mem_seg_len;
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			event_data->mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			event_data->mem_seg[i].size = ind_msg->mem_seg[i].size;
			cnss_pr_dbg("seg-%d: addr 0x%llx size 0x%x\n",
				    i, ind_msg->mem_seg[i].addr,
				    ind_msg->mem_seg[i].size);
		}
	}

	event_data->total_size = ind_msg->total_size;

	if (ind_msg->file_name_valid)
		strlcpy(event_data->file_name, ind_msg->file_name,
			QDSS_TRACE_FILE_NAME_MAX + 1);
	else
		strlcpy(event_data->file_name, "qdss_trace",
			QDSS_TRACE_FILE_NAME_MAX + 1);

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE,
			       0, event_data);

	return;

free_event_data:
	kfree(event_data);
}

static void cnss_wlfw_qdss_trace_free_ind_cb(struct qmi_handle *qmi_wlfw,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *txn,
					     const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_FREE,
			       0, NULL);
}

static void cnss_wlfw_respond_get_info_ind_cb(struct qmi_handle *qmi_wlfw,
					      struct sockaddr_qrtr *sq,
					      struct qmi_txn *txn,
					      const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_respond_get_info_ind_msg_v01 *ind_msg = data;

	cnss_pr_vdbg("Received QMI WLFW respond get info indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_pr_vdbg("Extract message with event length: %d, type: %d, is last: %d, seq no: %d\n",
		     ind_msg->data_len, ind_msg->type,
		     ind_msg->is_last, ind_msg->seq_no);

	if (plat_priv->get_info_cb_ctx && plat_priv->get_info_cb)
		plat_priv->get_info_cb(plat_priv->get_info_cb_ctx,
				       (void *)ind_msg->data,
				       ind_msg->data_len);
}
static struct qmi_msg_handler qmi_wlfw_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_REQUEST_MEM_IND_V01,
		.ei = wlfw_request_mem_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_request_mem_ind_msg_v01),
		.fn = cnss_wlfw_request_mem_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_MEM_READY_IND_V01,
		.ei = wlfw_fw_mem_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_mem_ready_ind_msg_v01),
		.fn = cnss_wlfw_fw_mem_ready_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_READY_IND_V01,
		.ei = wlfw_fw_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_ready_ind_msg_v01),
		.fn = cnss_wlfw_fw_ready_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_INIT_DONE_IND_V01,
		.ei = wlfw_fw_init_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_init_done_ind_msg_v01),
		.fn = cnss_wlfw_fw_init_done_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01,
		.ei = wlfw_pin_connect_result_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_pin_connect_result_ind_msg_v01),
		.fn = cnss_wlfw_pin_result_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_CAL_DONE_IND_V01,
		.ei = wlfw_cal_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_cal_done_ind_msg_v01),
		.fn = cnss_wlfw_cal_done_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01,
		.ei = wlfw_qdss_trace_req_mem_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_qdss_trace_req_mem_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_req_mem_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_SAVE_IND_V01,
		.ei = wlfw_qdss_trace_save_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_qdss_trace_save_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_save_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_FREE_IND_V01,
		.ei = wlfw_qdss_trace_free_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_qdss_trace_free_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_free_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_RESPOND_GET_INFO_IND_V01,
		.ei = wlfw_respond_get_info_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_respond_get_info_ind_msg_v01),
		.fn = cnss_wlfw_respond_get_info_ind_cb
	},
	{}
};

static int cnss_wlfw_connect_to_server(struct cnss_plat_data *plat_priv,
				       void *data)
{
	struct cnss_qmi_event_server_arrive_data *event_data = data;
	struct qmi_handle *qmi_wlfw = &plat_priv->qmi_wlfw;
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	if (!event_data)
		return -EINVAL;

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = event_data->node;
	sq.sq_port = event_data->port;

	ret = kernel_connect(qmi_wlfw->sock, (struct sockaddr *)&sq,
			     sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Failed to connect to QMI WLFW remote service port\n");
		goto out;
	}

	set_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service connected, state: 0x%lx\n",
		     plat_priv->driver_state);

	kfree(data);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(data);
	return ret;
}

int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv, void *data)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	ret = cnss_wlfw_connect_to_server(plat_priv, data);
	if (ret < 0)
		goto out;

	ret = cnss_wlfw_ind_register_send_sync(plat_priv);
	if (ret < 0) {
		if (ret == -EALREADY)
			ret = 0;
		goto out;
	}

	ret = cnss_wlfw_host_cap_send_sync(plat_priv);
	if (ret < 0)
		goto out;

	return 0;

out:
	return ret;
}

int cnss_wlfw_server_exit(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	clear_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service disconnected, state: 0x%lx\n",
		     plat_priv->driver_state);

	return 0;
}

static int wlfw_new_server(struct qmi_handle *qmi_wlfw,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_qmi_event_server_arrive_data *event_data;

	cnss_pr_dbg("WLFW server arriving: node %u port %u\n",
		    service->node, service->port);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return -ENOMEM;

	event_data->node = service->node;
	event_data->port = service->port;

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_SERVER_ARRIVE,
			       0, event_data);

	return 0;
}

static void wlfw_del_server(struct qmi_handle *qmi_wlfw,
			    struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("WLFW server exiting\n");

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_SERVER_EXIT,
			       0, NULL);
}

static struct qmi_ops qmi_wlfw_ops = {
	.new_server = wlfw_new_server,
	.del_server = wlfw_del_server,
};

int cnss_qmi_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = qmi_handle_init(&plat_priv->qmi_wlfw,
			      QMI_WLFW_MAX_RECV_BUF_SIZE,
			      &qmi_wlfw_ops, qmi_wlfw_msg_handlers);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize QMI handle, err: %d\n", ret);
		goto out;
	}

	ret = qmi_add_lookup(&plat_priv->qmi_wlfw, WLFW_SERVICE_ID_V01,
			     WLFW_SERVICE_VERS_V01, WLFW_SERVICE_INS_ID_V01);
	if (ret < 0)
		cnss_pr_err("Failed to add QMI lookup, err: %d\n", ret);

out:
	return ret;
}

void cnss_qmi_deinit(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->qmi_wlfw);
}

int coex_antenna_switch_to_wlan_send_sync_msg(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct coex_antenna_switch_to_wlan_req_msg_v01 *req;
	struct coex_antenna_switch_to_wlan_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending coex antenna switch_to_wlan\n");

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->antenna = plat_priv->antenna;

	ret = qmi_txn_init(&plat_priv->coex_qmi, &txn,
			   coex_antenna_switch_to_wlan_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for coex antenna switch_to_wlan resp %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
		(&plat_priv->coex_qmi, NULL, &txn,
		 QMI_COEX_SWITCH_ANTENNA_TO_WLAN_REQ_V01,
		 COEX_ANTENNA_SWITCH_TO_WLAN_REQ_MSG_V01_MAX_MSG_LEN,
		 coex_antenna_switch_to_wlan_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send coex antenna switch_to_wlan req %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, COEX_TIMEOUT);
	if (ret < 0) {
		cnss_pr_err("Coex antenna switch_to_wlan resp wait failed with ret %d\n",
			    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Coex antenna switch_to_wlan request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (resp->grant_valid)
		plat_priv->grant = resp->grant;

	cnss_pr_dbg("Coex antenna grant: 0x%llx\n", resp->grant);

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	return ret;
}

int coex_antenna_switch_to_mdm_send_sync_msg(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct coex_antenna_switch_to_mdm_req_msg_v01 *req;
	struct coex_antenna_switch_to_mdm_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending coex antenna switch_to_mdm\n");

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->antenna = plat_priv->antenna;

	ret = qmi_txn_init(&plat_priv->coex_qmi, &txn,
			   coex_antenna_switch_to_mdm_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for coex antenna switch_to_mdm resp %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
		(&plat_priv->coex_qmi, NULL, &txn,
		 QMI_COEX_SWITCH_ANTENNA_TO_MDM_REQ_V01,
		 COEX_ANTENNA_SWITCH_TO_MDM_REQ_MSG_V01_MAX_MSG_LEN,
		 coex_antenna_switch_to_mdm_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send coex antenna switch_to_mdm req %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, COEX_TIMEOUT);
	if (ret < 0) {
		cnss_pr_err("Coex antenna switch_to_mdm resp wait failed with ret %d\n",
			    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Coex antenna switch_to_mdm request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	return ret;
}

static int coex_new_server(struct qmi_handle *qmi,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, coex_qmi);
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	cnss_pr_dbg("COEX server arrive: node %u port %u\n",
		    service->node, service->port);

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = service->node;
	sq.sq_port = service->port;
	ret = kernel_connect(qmi->sock, (struct sockaddr *)&sq, sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Fail to connect to remote service port\n");
		return ret;
	}

	set_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state);
	cnss_pr_dbg("COEX Server Connected: 0x%lx\n",
		    plat_priv->driver_state);
	return 0;
}

static void coex_del_server(struct qmi_handle *qmi,
			    struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, coex_qmi);

	cnss_pr_dbg("COEX server exit\n");

	clear_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state);
}

static struct qmi_ops coex_qmi_ops = {
	.new_server = coex_new_server,
	.del_server = coex_del_server,
};

int cnss_register_coex_service(struct cnss_plat_data *plat_priv)
{	int ret;

	ret = qmi_handle_init(&plat_priv->coex_qmi,
			      COEX_SERVICE_MAX_MSG_LEN,
			      &coex_qmi_ops, NULL);
	if (ret < 0)
		return ret;

	ret = qmi_add_lookup(&plat_priv->coex_qmi, COEX_SERVICE_ID_V01,
			     COEX_SERVICE_VERS_V01, 0);
	return ret;
}

void cnss_unregister_coex_service(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->coex_qmi);
}

/* IMS Service */
int ims_subscribe_for_indication_send_async(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct ims_private_service_subscribe_for_indications_req_msg_v01 *req;
	struct qmi_txn *txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending ASYNC ims subscribe for indication\n");

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->wfc_call_status_valid = 1;
	req->wfc_call_status = 1;

	txn = &plat_priv->txn;
	ret = qmi_txn_init(&plat_priv->ims_qmi, txn, NULL, NULL);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for ims subscribe for indication resp %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
	(&plat_priv->ims_qmi, NULL, txn,
	QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_V01,
	IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_MSG_V01_MAX_MSG_LEN,
	ims_private_service_subscribe_for_indications_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(txn);
		cnss_pr_err("Fail to send ims subscribe for indication req %d\n",
			    ret);
		goto out;
	}

	kfree(req);
	return 0;

out:
	kfree(req);
	return ret;
}

static void ims_subscribe_for_indication_resp_cb(struct qmi_handle *qmi,
						 struct sockaddr_qrtr *sq,
						 struct qmi_txn *txn,
						 const void *data)
{
	const
	struct ims_private_service_subscribe_for_indications_rsp_msg_v01 *resp =
		data;

	cnss_pr_dbg("Received IMS subscribe indication response\n");

	if (!txn) {
		cnss_pr_err("spurious response\n");
		return;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("IMS subscribe for indication request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		txn->result = -resp->resp.result;
	}
}

static void ims_wfc_call_status_ind_cb(struct qmi_handle *ims_qmi,
				       struct sockaddr_qrtr *sq,
				       struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(ims_qmi, struct cnss_plat_data, ims_qmi);
	const
	struct ims_private_service_wfc_call_status_ind_msg_v01 *ind_msg = data;
	u32 data_len = 0;

	cnss_pr_dbg("Received IMS wfc call status indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (!ind_msg) {
		cnss_pr_err("Invalid indication\n");
		return;
	}

	data_len = sizeof(*ind_msg);
	if (data_len > QMI_WLFW_MAX_WFC_CALL_STATUS_DATA_SIZE_V01) {
		cnss_pr_err("Exceed maxinum data len:%u\n", data_len);
		return;
	}

	cnss_wlfw_wfc_call_status_send_sync(plat_priv, data_len, ind_msg);
}

static struct qmi_msg_handler qmi_ims_msg_handlers[] = {
	{
		.type = QMI_RESPONSE,
		.msg_id =
		QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_V01,
		.ei =
		ims_private_service_subscribe_for_indications_rsp_msg_v01_ei,
		.decoded_size = sizeof(struct
		ims_private_service_subscribe_for_indications_rsp_msg_v01),
		.fn = ims_subscribe_for_indication_resp_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_IMS_PRIVATE_SERVICE_WFC_CALL_STATUS_IND_V01,
		.ei = ims_private_service_wfc_call_status_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct ims_private_service_wfc_call_status_ind_msg_v01),
		.fn = ims_wfc_call_status_ind_cb
	},
	{}
};

static int ims_new_server(struct qmi_handle *qmi,
			  struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, ims_qmi);
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	cnss_pr_dbg("IMS server arrive: node %u port %u\n",
		    service->node, service->port);

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = service->node;
	sq.sq_port = service->port;
	ret = kernel_connect(qmi->sock, (struct sockaddr *)&sq, sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Fail to connect to remote service port\n");
		return ret;
	}

	set_bit(CNSS_IMS_CONNECTED, &plat_priv->driver_state);
	cnss_pr_dbg("IMS Server Connected: 0x%lx\n",
		    plat_priv->driver_state);

	ret = ims_subscribe_for_indication_send_async(plat_priv);
	return ret;
}

static void ims_del_server(struct qmi_handle *qmi,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, ims_qmi);

	cnss_pr_dbg("IMS server exit\n");

	clear_bit(CNSS_IMS_CONNECTED, &plat_priv->driver_state);
}

static struct qmi_ops ims_qmi_ops = {
	.new_server = ims_new_server,
	.del_server = ims_del_server,
};

int cnss_register_ims_service(struct cnss_plat_data *plat_priv)
{	int ret;

	ret = qmi_handle_init(&plat_priv->ims_qmi,
			      IMSPRIVATE_SERVICE_MAX_MSG_LEN,
			      &ims_qmi_ops, qmi_ims_msg_handlers);
	if (ret < 0)
		return ret;

	ret = qmi_add_lookup(&plat_priv->ims_qmi, IMSPRIVATE_SERVICE_ID_V01,
			     IMSPRIVATE_SERVICE_VERS_V01, 0);
	return ret;
}

void cnss_unregister_ims_service(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->ims_qmi);
}
