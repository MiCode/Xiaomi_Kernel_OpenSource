// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved. */

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/soc/qcom/qmi.h>

#include "bus.h"
#include "debug.h"
#include "main.h"
#include "qmi.h"
#include "genl.h"

#define WLFW_SERVICE_INS_ID_V01		1
#define WLFW_CLIENT_ID			0x4b4e454c
#define BDF_FILE_NAME_PREFIX		"bdwlan"
#define ELF_BDF_FILE_NAME		"bdwlan.elf"
#define ELF_BDF_FILE_NAME_GF		"bdwlang.elf"
#define ELF_BDF_FILE_NAME_PREFIX	"bdwlan.e"
#define ELF_BDF_FILE_NAME_GF_PREFIX	"bdwlang.e"
#define BIN_BDF_FILE_NAME		"bdwlan.bin"
#define BIN_BDF_FILE_NAME_GF		"bdwlang.bin"
#define BIN_BDF_FILE_NAME_PREFIX	"bdwlan.b"
#define BIN_BDF_FILE_NAME_GF_PREFIX	"bdwlang.b"
#define REGDB_FILE_NAME			"regdb.bin"
#define HDS_FILE_NAME			"hds.bin"
#define CHIP_ID_GF_MASK			0x10

#define QDSS_TRACE_CONFIG_FILE		"qdss_trace_config"
#define DEBUG_STR			"debug"
#define HW_V1_NUMBER			"v1"
#define HW_V2_NUMBER			"v2"
#define CE_MSI_NAME			"CE"

#define QMI_WLFW_TIMEOUT_MS		(plat_priv->ctrl_params.qmi_timeout)
#define QMI_WLFW_TIMEOUT_JF		msecs_to_jiffies(QMI_WLFW_TIMEOUT_MS)
#define COEX_TIMEOUT			QMI_WLFW_TIMEOUT_JF
#define IMS_TIMEOUT                     QMI_WLFW_TIMEOUT_JF

#define QMI_WLFW_MAX_RECV_BUF_SIZE	SZ_8K
#define IMSPRIVATE_SERVICE_MAX_MSG_LEN	SZ_8K
#define DMS_QMI_MAX_MSG_LEN		SZ_256
#define DMS_MAC_NOT_PROVISIONED		16

#define QMI_WLFW_MAC_READY_TIMEOUT_MS	50
#define QMI_WLFW_MAC_READY_MAX_RETRY	200

#ifdef CONFIG_CNSS2_DEBUG
static bool ignore_qmi_failure;
#define CNSS_QMI_ASSERT() CNSS_ASSERT(ignore_qmi_failure)
void cnss_ignore_qmi_failure(bool ignore)
{
	ignore_qmi_failure = ignore;
}
#else
#define CNSS_QMI_ASSERT() do { } while (0)
void cnss_ignore_qmi_failure(bool ignore) { }
#endif

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
	req->request_mem_enable_valid = 1;
	req->request_mem_enable = 1;
	req->fw_mem_ready_enable_valid = 1;
	req->fw_mem_ready_enable = 1;
	/* fw_ready indication is replaced by fw_init_done in HST/HSP */
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
	req->wfc_call_twt_config_enable_valid = 1;
	req->wfc_call_twt_config_enable = 1;

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
	CNSS_QMI_ASSERT();

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
	u64 iova_start = 0, iova_size = 0,
	    iova_ipa_start = 0, iova_ipa_size = 0;
	u64 feature_list = 0;

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

	if (cnss_bus_is_smmu_s1_enabled(plat_priv) &&
	    !cnss_bus_get_iova(plat_priv, &iova_start, &iova_size) &&
	    !cnss_bus_get_iova_ipa(plat_priv, &iova_ipa_start,
				   &iova_ipa_size)) {
		req->ddr_range_valid = 1;
		req->ddr_range[0].start = iova_start;
		req->ddr_range[0].size = iova_size + iova_ipa_size;
		cnss_pr_dbg("Sending iova starting 0x%llx with size 0x%llx\n",
			    req->ddr_range[0].start, req->ddr_range[0].size);
	}

	req->host_build_type_valid = 1;
	req->host_build_type = cnss_get_host_build_type();

	ret = cnss_get_feature_list(plat_priv, &feature_list);
	if (!ret) {
		req->feature_list_valid = 1;
		req->feature_list = feature_list;
		cnss_pr_dbg("Sending feature list 0x%llx\n",
			    req->feature_list);
	}

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
	CNSS_QMI_ASSERT();
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
	CNSS_QMI_ASSERT();
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
	int ret = 0, i;

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
	if (resp->dev_mem_info_valid) {
		for (i = 0; i < QMI_WLFW_MAX_DEV_MEM_NUM_V01; i++) {
			plat_priv->dev_mem_info[i].start =
				resp->dev_mem_info[i].start;
			plat_priv->dev_mem_info[i].size =
				resp->dev_mem_info[i].size;
			cnss_pr_buf("Device memory info[%d]: start = 0x%llx, size = 0x%llx\n",
				    i, plat_priv->dev_mem_info[i].start,
				    plat_priv->dev_mem_info[i].size);
		}
	}
	if (resp->fw_caps_valid)
		plat_priv->fw_pcie_gen_switch =
			!!(resp->fw_caps & QMI_WLFW_HOST_PCIE_GEN_SWITCH_V01);

	cnss_pr_dbg("Target capability: chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, otp_version: 0x%x\n",
		    plat_priv->chip_info.chip_id,
		    plat_priv->chip_info.chip_family,
		    plat_priv->board_info.board_id, plat_priv->soc_info.soc_id,
		    plat_priv->otp_version);
	cnss_pr_dbg("fw_version: 0x%x, fw_build_timestamp: %s, fw_build_id: %s\n",
		    plat_priv->fw_version_info.fw_version,
		    plat_priv->fw_version_info.fw_build_timestamp,
		    plat_priv->fw_build_id);

	kfree(req);
	kfree(resp);
	return 0;

out:
	CNSS_QMI_ASSERT();
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_get_bdf_file_name(struct cnss_plat_data *plat_priv,
				  u32 bdf_type, char *filename,
				  u32 filename_len)
{
	char filename_tmp[MAX_FIRMWARE_NAME_LEN];
	int ret = 0;

	switch (bdf_type) {
	case CNSS_BDF_ELF:
		/* Board ID will be equal or less than 0xFF in GF mask case */
		if (plat_priv->board_info.board_id == 0xFF) {
			if (plat_priv->chip_info.chip_id & CHIP_ID_GF_MASK)
				snprintf(filename_tmp, filename_len,
					 ELF_BDF_FILE_NAME_GF);
			else
				snprintf(filename_tmp, filename_len,
					 ELF_BDF_FILE_NAME);
		} else if (plat_priv->board_info.board_id < 0xFF) {
			if (plat_priv->chip_info.chip_id & CHIP_ID_GF_MASK)
				snprintf(filename_tmp, filename_len,
					 ELF_BDF_FILE_NAME_GF_PREFIX "%02x",
					 plat_priv->board_info.board_id);
			else
				snprintf(filename_tmp, filename_len,
					 ELF_BDF_FILE_NAME_PREFIX "%02x",
					 plat_priv->board_info.board_id);
		} else {
			snprintf(filename_tmp, filename_len,
				 BDF_FILE_NAME_PREFIX "%02x.e%02x",
				 plat_priv->board_info.board_id >> 8 & 0xFF,
				 plat_priv->board_info.board_id & 0xFF);
		}
		break;
	case CNSS_BDF_BIN:
		if (plat_priv->board_info.board_id == 0xFF) {
			if (plat_priv->chip_info.chip_id & CHIP_ID_GF_MASK)
				snprintf(filename_tmp, filename_len,
					 BIN_BDF_FILE_NAME_GF);
			else
				snprintf(filename_tmp, filename_len,
					 BIN_BDF_FILE_NAME);
		} else if (plat_priv->board_info.board_id < 0xFF) {
			if (plat_priv->chip_info.chip_id & CHIP_ID_GF_MASK)
				snprintf(filename_tmp, filename_len,
					 BIN_BDF_FILE_NAME_GF_PREFIX "%02x",
					 plat_priv->board_info.board_id);
			else
				snprintf(filename_tmp, filename_len,
					 BIN_BDF_FILE_NAME_PREFIX "%02x",
					 plat_priv->board_info.board_id);
		} else {
			snprintf(filename_tmp, filename_len,
				 BDF_FILE_NAME_PREFIX "%02x.b%02x",
				 plat_priv->board_info.board_id >> 8 & 0xFF,
				 plat_priv->board_info.board_id & 0xFF);
		}
		break;
	case CNSS_BDF_REGDB:
		snprintf(filename_tmp, filename_len, REGDB_FILE_NAME);
		break;
	case CNSS_BDF_HDS:
		snprintf(filename_tmp, filename_len, HDS_FILE_NAME);
		break;
	default:
		cnss_pr_err("Invalid BDF type: %d\n",
			    plat_priv->ctrl_params.bdf_type);
		ret = -EINVAL;
		break;
	}

	if (!ret)
		cnss_bus_add_fw_prefix_name(plat_priv, filename, filename_tmp);

	return ret;
}

int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv,
				 u32 bdf_type)
{
	struct wlfw_bdf_download_req_msg_v01 *req;
	struct wlfw_bdf_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	char filename[MAX_FIRMWARE_NAME_LEN];
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	u32 remaining;
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
	if (ret)
		goto err_req_fw;

	if (bdf_type == CNSS_BDF_REGDB)
		ret = cnss_request_firmware_direct(plat_priv, &fw_entry,
						   filename);
	else
		ret = firmware_request_nowarn(&fw_entry, filename,
					      &plat_priv->plat_dev->dev);

	if (ret) {
		cnss_pr_err("Failed to load BDF: %s, ret: %d\n", filename, ret);
		goto err_req_fw;
	}

	temp = fw_entry->data;

	/* Check if firmware image size is within expected range */
	if (fw_entry->size > U32_MAX)
		goto err_send;

	/* Typecast to match with interface defintition */
	remaining = (u32)fw_entry->size;

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

	release_firmware(fw_entry);

	/* QCA6490 enable S3E regulator for IPA configuration only */
	if (resp->host_bdf_data_valid) {
		if (!(resp->host_bdf_data & QMI_WLFW_HW_XPA_V01))
			cnss_enable_int_pow_amp_vreg(plat_priv);
	}

	kfree(req);
	kfree(resp);
	return 0;

err_send:
	release_firmware(fw_entry);
err_req_fw:
	if (!(bdf_type == CNSS_BDF_REGDB ||
	      test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state) ||
	      ret == -EAGAIN))
		CNSS_QMI_ASSERT();
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
	CNSS_QMI_ASSERT();
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_wlan_mac_req_send_sync(struct cnss_plat_data *plat_priv,
				     u8 *mac, u32 mac_len)
{
	struct wlfw_mac_addr_req_msg_v01 req;
	struct wlfw_mac_addr_resp_msg_v01 resp = {0};
	struct qmi_txn txn;
	int ret;

	if (!plat_priv || !mac || mac_len != QMI_WLFW_MAC_ADDR_SIZE_V01)
		return -EINVAL;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_mac_addr_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for mac req, err: %d\n",
			    ret);
		ret = -EIO;
		goto out;
	}

		cnss_pr_dbg("Sending WLAN mac req [%pM], state: 0x%lx\n",
			    mac, plat_priv->driver_state);
	memcpy(req.mac_addr, mac, mac_len);
	req.mac_addr_valid = 1;

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_MAC_ADDR_REQ_V01,
			       WLFW_MAC_ADDR_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_mac_addr_req_msg_v01_ei, &req);
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

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WLAN mac req failed, result: %d, err: %d\n",
			    resp.resp.result);
		ret = -resp.resp.result;
	}
out:
	return ret;
}

int cnss_wlfw_qdss_data_send_sync(struct cnss_plat_data *plat_priv, char *file_name,
				  u32 total_size)
{
	int ret = 0;
	struct wlfw_qdss_trace_data_req_msg_v01 *req;
	struct wlfw_qdss_trace_data_resp_msg_v01 *resp;
	unsigned char *p_qdss_trace_data_temp, *p_qdss_trace_data = NULL;
	u32 remaining;
	struct qmi_txn txn;

	cnss_pr_dbg("%s\n", __func__);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	p_qdss_trace_data = kzalloc(total_size, GFP_KERNEL);
	if (!p_qdss_trace_data) {
		ret = ENOMEM;
		goto end;
	}

	remaining = total_size;
	p_qdss_trace_data_temp = p_qdss_trace_data;
	while (remaining && resp->end == 0) {
		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				   wlfw_qdss_trace_data_resp_msg_v01_ei, resp);

		if (ret < 0) {
			cnss_pr_err("Fail to init txn for QDSS trace resp %d\n",
				    ret);
			goto fail;
		}

		ret = qmi_send_request
			(&plat_priv->qmi_wlfw, NULL, &txn,
			 QMI_WLFW_QDSS_TRACE_DATA_REQ_V01,
			 WLFW_QDSS_TRACE_DATA_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_qdss_trace_data_req_msg_v01_ei, req);

		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Fail to send QDSS trace data req %d\n",
				    ret);
			goto fail;
		}

		ret = qmi_txn_wait(&txn, plat_priv->ctrl_params.qmi_timeout);

		if (ret < 0) {
			cnss_pr_err("QDSS trace resp wait failed with rc %d\n",
				    ret);
			goto fail;
		} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("QMI QDSS trace request rejected, result:%d error:%d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			goto fail;
		} else {
			ret = 0;
		}

		cnss_pr_dbg("%s: response total size  %d data len %d",
			    __func__, resp->total_size, resp->data_len);

		if ((resp->total_size_valid == 1 &&
		     resp->total_size == total_size) &&
		   (resp->seg_id_valid == 1 && resp->seg_id == req->seg_id) &&
		   (resp->data_valid == 1 &&
		    resp->data_len <= QMI_WLFW_MAX_DATA_SIZE_V01)) {
			memcpy(p_qdss_trace_data_temp,
			       resp->data, resp->data_len);
		} else {
			cnss_pr_err("%s: Unmatched qdss trace data, Expect total_size %u, seg_id %u, Recv total_size_valid %u, total_size %u, seg_id_valid %u, seg_id %u, data_len_valid %u, data_len %u",
				    __func__,
				     total_size, req->seg_id,
				     resp->total_size_valid,
				     resp->total_size,
				     resp->seg_id_valid,
				     resp->seg_id,
				     resp->data_valid,
				     resp->data_len);
			ret = -1;
			goto fail;
		}

		remaining -= resp->data_len;
		p_qdss_trace_data_temp += resp->data_len;
		req->seg_id++;
	}

	if (remaining == 0 && (resp->end_valid && resp->end)) {
		ret = cnss_genl_send_msg(p_qdss_trace_data,
					 CNSS_GENL_MSG_TYPE_QDSS, file_name,
					 total_size);
		if (ret < 0) {
			cnss_pr_err("Fail to save QDSS trace data: %d\n",
				    ret);
		ret = -1;
		goto fail;
		}
	} else {
		cnss_pr_err("%s: QDSS trace file corrupted: remaining %u, end_valid %u, end %u",
			    __func__,
			     remaining, resp->end_valid, resp->end);
		ret = -1;
		goto fail;
	}

fail:
	kfree(p_qdss_trace_data);

end:
	kfree(req);
	kfree(resp);
	return ret;
}

#ifdef CONFIG_CNSS2_DEBUG
static void cnss_get_qdss_cfg_filename(struct cnss_plat_data *plat_priv,
				       char *filename, u32 filename_len)
{
	char filename_tmp[MAX_FIRMWARE_NAME_LEN];

	if (plat_priv->device_version.major_version == FW_V2_NUMBER)
		snprintf(filename_tmp, filename_len, QDSS_TRACE_CONFIG_FILE
			 "_%s_%s.cfg", DEBUG_STR, HW_V2_NUMBER);
	else
		snprintf(filename_tmp, filename_len, QDSS_TRACE_CONFIG_FILE
			 "_%s_%s.cfg", DEBUG_STR, HW_V1_NUMBER);

	cnss_bus_add_fw_prefix_name(plat_priv, filename, filename_tmp);
}
#else
static void cnss_get_qdss_cfg_filename(struct cnss_plat_data *plat_priv,
				       char *filename, u32 filename_len)
{
	char filename_tmp[MAX_FIRMWARE_NAME_LEN];

	if (plat_priv->device_version.major_version == FW_V2_NUMBER)
		snprintf(filename_tmp, filename_len, QDSS_TRACE_CONFIG_FILE
			 "_%s.cfg", HW_V2_NUMBER);
	else
		snprintf(filename_tmp, filename_len, QDSS_TRACE_CONFIG_FILE
			 "_%s.cfg", HW_V1_NUMBER);

	cnss_bus_add_fw_prefix_name(plat_priv, filename, filename_tmp);
}
#endif

int cnss_wlfw_qdss_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_qdss_trace_config_download_req_msg_v01 *req;
	struct wlfw_qdss_trace_config_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	char qdss_cfg_filename[MAX_FIRMWARE_NAME_LEN];
	u32 remaining;
	int ret = 0;

	cnss_pr_dbg("Sending QDSS config download message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	cnss_get_qdss_cfg_filename(plat_priv, qdss_cfg_filename, sizeof(qdss_cfg_filename));
	ret = cnss_request_firmware_direct(plat_priv, &fw_entry,
					   qdss_cfg_filename);
	if (ret) {
		cnss_pr_err("Failed to load QDSS: %s\n",
			    qdss_cfg_filename);
		goto err_req_fw;
	}

	temp = fw_entry->data;

	/* Check if firmware image size is within expected range */
	if (fw_entry->size > U32_MAX)
		goto err_send;

	/* Typecast to match with interface definition */
	remaining = (u32)fw_entry->size;

	cnss_pr_dbg("Downloading QDSS: %s, size: %u\n",
		    qdss_cfg_filename, remaining);

	while (remaining) {
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;

		if (remaining > QMI_WLFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init
			(&plat_priv->qmi_wlfw, &txn,
			 wlfw_qdss_trace_config_download_resp_msg_v01_ei,
			 resp);
		if (ret < 0) {
			cnss_pr_err("Failed to initialize txn for QDSS download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_send_request
		      (&plat_priv->qmi_wlfw, NULL, &txn,
		       QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01,
		       WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
		       wlfw_qdss_trace_config_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Failed to send respond QDSS download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_txn_wait(&txn, plat_priv->ctrl_params.qmi_timeout);
		if (ret < 0) {
			cnss_pr_err("Failed to wait for response of QDSS download request, err: %d\n",
				    ret);
			goto err_send;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("QDSS download request failed, result: %d, err: %d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			goto err_send;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

	release_firmware(fw_entry);
	kfree(req);
	kfree(resp);
	return 0;

err_send:
	release_firmware(fw_entry);
err_req_fw:

	kfree(req);
	kfree(resp);
	return ret;
}

static int wlfw_send_qdss_trace_mode_req
		(struct cnss_plat_data *plat_priv,
		 enum wlfw_qdss_trace_mode_enum_v01 mode,
		 unsigned long long option)
{
	int rc = 0;
	int tmp = 0;
	struct wlfw_qdss_trace_mode_req_msg_v01 *req;
	struct wlfw_qdss_trace_mode_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!plat_priv)
		return -ENODEV;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mode_valid = 1;
	req->mode = mode;
	req->option_valid = 1;
	req->option = option;

	tmp = plat_priv->hw_trc_override;

	req->hw_trc_disable_override_valid = 1;
	req->hw_trc_disable_override =
	(tmp > QMI_PARAM_DISABLE_V01 ? QMI_PARAM_DISABLE_V01 :
		 (tmp < 0 ? QMI_PARAM_INVALID_V01 : tmp));

	cnss_pr_dbg("%s: mode %u, option %llu, hw_trc_disable_override: %u",
		    __func__, mode, option, req->hw_trc_disable_override);

	rc = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			  wlfw_qdss_trace_mode_resp_msg_v01_ei, resp);
	if (rc < 0) {
		cnss_pr_err("Fail to init txn for QDSS Mode resp %d\n",
			    rc);
		goto out;
	}

	rc = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			      QMI_WLFW_QDSS_TRACE_MODE_REQ_V01,
			      WLFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			      wlfw_qdss_trace_mode_req_msg_v01_ei, req);
	if (rc < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send QDSS Mode req %d\n", rc);
		goto out;
	}

	rc = qmi_txn_wait(&txn, plat_priv->ctrl_params.qmi_timeout);
	if (rc < 0) {
		cnss_pr_err("QDSS Mode resp wait failed with rc %d\n",
			    rc);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("QMI QDSS Mode request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		rc = -resp->resp.result;
		goto out;
	}

	kfree(resp);
	kfree(req);
	return rc;
out:
	kfree(resp);
	kfree(req);
	CNSS_QMI_ASSERT();
	return rc;
}

int wlfw_qdss_trace_send_start(struct cnss_plat_data *plat_priv)
{
	return wlfw_send_qdss_trace_mode_req(plat_priv,
					     QMI_WLFW_QDSS_TRACE_ON_V01, 0);
}

int wlfw_qdss_trace_send_stop(struct cnss_plat_data *plat_priv, unsigned long long option)
{
	return wlfw_send_qdss_trace_mode_req(plat_priv, QMI_WLFW_QDSS_TRACE_OFF_V01,
					     option);
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
		CNSS_QMI_ASSERT();
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
	u32 i, ce_id, num_vectors, user_base_data, base_vector;
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
	if (config->num_shadow_reg_cfg) {
		req->shadow_reg_valid = 1;
		if (config->num_shadow_reg_cfg >
		    QMI_WLFW_MAX_NUM_SHADOW_REG_V01)
			req->shadow_reg_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V01;
		else
			req->shadow_reg_len = config->num_shadow_reg_cfg;
		memcpy(req->shadow_reg, config->shadow_reg_cfg,
		       sizeof(struct wlfw_shadow_reg_cfg_s_v01)
		       * req->shadow_reg_len);
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
	if (config->rri_over_ddr_cfg_valid) {
		req->rri_over_ddr_cfg_valid = 1;
		req->rri_over_ddr_cfg.base_addr_low =
			config->rri_over_ddr_cfg.base_addr_low;
		req->rri_over_ddr_cfg.base_addr_high =
			config->rri_over_ddr_cfg.base_addr_high;
	}
	if (config->send_msi_ce) {
		ret = cnss_bus_get_msi_assignment(plat_priv,
						  CE_MSI_NAME,
						  &num_vectors,
						  &user_base_data,
						  &base_vector);
		if (!ret) {
			req->msi_cfg_valid = 1;
			req->msi_cfg_len = QMI_WLFW_MAX_NUM_CE_V01;
			for (ce_id = 0; ce_id < QMI_WLFW_MAX_NUM_CE_V01;
				ce_id++) {
				req->msi_cfg[ce_id].ce_id = ce_id;
				req->msi_cfg[ce_id].msi_vector =
					(ce_id % num_vectors) + base_vector;
			}
		}
	}

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
	CNSS_QMI_ASSERT();
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

int cnss_wlfw_send_pcie_gen_speed_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_pcie_gen_switch_req_msg_v01 req;
	struct wlfw_pcie_gen_switch_resp_msg_v01 resp = {0};
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (plat_priv->pcie_gen_speed == QMI_PCIE_GEN_SPEED_INVALID_V01 ||
	    !plat_priv->fw_pcie_gen_switch) {
		cnss_pr_dbg("PCIE Gen speed not setup\n");
		return 0;
	}

	cnss_pr_dbg("Sending PCIE Gen speed: %d state: 0x%lx\n",
		    plat_priv->pcie_gen_speed, plat_priv->driver_state);
	req.pcie_speed = (enum wlfw_pcie_gen_speed_v01)
			plat_priv->pcie_gen_speed;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_pcie_gen_switch_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for PCIE speed switch err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_PCIE_GEN_SWITCH_REQ_V01,
			       WLFW_PCIE_GEN_SWITCH_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_pcie_gen_switch_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send PCIE speed switch, err: %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for PCIE Gen switch resp, err: %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("PCIE Gen Switch req failed, Speed: %d, result: %d, err: %d\n",
			    plat_priv->pcie_gen_speed, resp.resp.result,
			    resp.resp.error);
		ret = -resp.resp.result;
	}
out:
	/* Reset PCIE Gen speed after one time use */
	plat_priv->pcie_gen_speed = QMI_PCIE_GEN_SPEED_INVALID_V01;
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

static int cnss_wlfw_wfc_call_status_send_sync
	(struct cnss_plat_data *plat_priv,
	 const struct ims_private_service_wfc_call_status_ind_msg_v01 *ind_msg)
{
	struct wlfw_wfc_call_status_req_msg_v01 *req;
	struct wlfw_wfc_call_status_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Drop IMS WFC indication as FW not initialized\n");
		return -EINVAL;
	}
	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	/**
	 * WFC Call r1 design has CNSS as pass thru using opaque hex buffer.
	 * But in r2 update QMI structure is expanded and as an effect qmi
	 * decoded structures have padding. Thus we cannot use buffer design.
	 * For backward compatibility for r1 design copy only wfc_call_active
	 * value in hex buffer.
	 */
	req->wfc_call_status_len = sizeof(ind_msg->wfc_call_active);
	req->wfc_call_status[0] = ind_msg->wfc_call_active;

	/* wfc_call_active is mandatory in IMS indication */
	req->wfc_call_active_valid = 1;
	req->wfc_call_active = ind_msg->wfc_call_active;
	req->all_wfc_calls_held_valid = ind_msg->all_wfc_calls_held_valid;
	req->all_wfc_calls_held = ind_msg->all_wfc_calls_held;
	req->is_wfc_emergency_valid = ind_msg->is_wfc_emergency_valid;
	req->is_wfc_emergency = ind_msg->is_wfc_emergency;
	req->twt_ims_start_valid = ind_msg->twt_ims_start_valid;
	req->twt_ims_start = ind_msg->twt_ims_start;
	req->twt_ims_int_valid = ind_msg->twt_ims_int_valid;
	req->twt_ims_int = ind_msg->twt_ims_int;
	req->media_quality_valid = ind_msg->media_quality_valid;
	req->media_quality =
		(enum wlfw_wfc_media_quality_v01)ind_msg->media_quality;

	cnss_pr_dbg("CNSS->FW: WFC_CALL_REQ: state: 0x%lx\n",
		    plat_priv->driver_state);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wfc_call_status_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("CNSS->FW: WFC_CALL_REQ: QMI Txn Init: Err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WFC_CALL_STATUS_REQ_V01,
			       WLFW_WFC_CALL_STATUS_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wfc_call_status_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("CNSS->FW: WFC_CALL_REQ: QMI Send Err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("FW->CNSS: WFC_CALL_RSP: QMI Wait Err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("FW->CNSS: WFC_CALL_RSP: Result: %d Err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}
	ret = 0;
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

	cnss_pr_buf("Sending get info message, type: %d, cmd length: %d, state: 0x%lx\n",
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

/**
 * cnss_wlfw_fw_ready_ind_cb: FW ready indication handler (Helium arch)
 *
 * This event is not required for HST/ HSP as FW calibration done is
 * provided in QMI_WLFW_CAL_DONE_IND_V01
 */
static void cnss_wlfw_fw_ready_ind_cb(struct qmi_handle *qmi_wlfw,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_cal_info *cal_info;

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (plat_priv->device_id == QCA6390_DEVICE_ID ||
	    plat_priv->device_id == QCA6490_DEVICE_ID) {
		cnss_pr_dbg("Ignore FW Ready Indication for HST/HSP");
		return;
	}

	cnss_pr_dbg("Received QMI WLFW FW ready indication.\n");
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

/**
 * cnss_wlfw_fw_mem_file_save_ind_cb: Save given FW mem to filesystem
 *
 * QDSS_TRACE_SAVE_IND feature is overloaded to provide any host allocated
 * fw memory segment for dumping to file system. Only one type of mem can be
 * saved per indication and is provided in mem seg index 0.
 *
 * Return: None
 */
static void cnss_wlfw_fw_mem_file_save_ind_cb(struct qmi_handle *qmi_wlfw,
					      struct sockaddr_qrtr *sq,
					      struct qmi_txn *txn,
					      const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_save_ind_msg_v01 *ind_msg = data;
	struct cnss_qmi_event_fw_mem_file_save_data *event_data;
	int i = 0;

	if (!txn || !data) {
		cnss_pr_err("Spurious indication\n");
		return;
	}
	cnss_pr_dbg("QMI fw_mem_file_save: source: %d  mem_seg: %d type: %u len: %u\n",
		    ind_msg->source, ind_msg->mem_seg_valid,
		    ind_msg->mem_seg[0].type, ind_msg->mem_seg_len);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	event_data->mem_type = ind_msg->mem_seg[0].type;
	event_data->mem_seg_len = ind_msg->mem_seg_len;
	event_data->total_size = ind_msg->total_size;

	if (ind_msg->mem_seg_valid) {
		if (ind_msg->mem_seg_len > QMI_WLFW_MAX_STR_LEN_V01) {
			cnss_pr_err("Invalid seg len indication\n");
			goto free_event_data;
		}
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			event_data->mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			event_data->mem_seg[i].size = ind_msg->mem_seg[i].size;
			if (event_data->mem_type != ind_msg->mem_seg[i].type) {
				cnss_pr_err("FW Mem file save ind cannot have multiple mem types\n");
				goto free_event_data;
			}
			cnss_pr_dbg("seg-%d: addr 0x%llx size 0x%x\n",
				    i, ind_msg->mem_seg[i].addr,
				    ind_msg->mem_seg[i].size);
		}
	}

	if (ind_msg->file_name_valid)
		strlcpy(event_data->file_name, ind_msg->file_name,
			QMI_WLFW_MAX_STR_LEN_V01 + 1);
	if (ind_msg->source == 1) {
		if (!ind_msg->file_name_valid)
			strlcpy(event_data->file_name, "qdss_trace_wcss_etb",
				QMI_WLFW_MAX_STR_LEN_V01 + 1);
		cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA,
				       0, event_data);
	} else {
		if (event_data->mem_type == QMI_WLFW_MEM_QDSS_V01) {
			if (!ind_msg->file_name_valid)
				strlcpy(event_data->file_name, "qdss_trace_ddr",
					QMI_WLFW_MAX_STR_LEN_V01 + 1);
		} else {
			if (!ind_msg->file_name_valid)
				strlcpy(event_data->file_name, "fw_mem_dump",
					QMI_WLFW_MAX_STR_LEN_V01 + 1);
		}

		cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_FW_MEM_FILE_SAVE,
				       0, event_data);
	}

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

	cnss_pr_buf("Received QMI WLFW respond get info indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_pr_buf("Extract message with event length: %d, type: %d, is last: %d, seq no: %d\n",
		    ind_msg->data_len, ind_msg->type,
		    ind_msg->is_last, ind_msg->seq_no);

	if (plat_priv->get_info_cb_ctx && plat_priv->get_info_cb)
		plat_priv->get_info_cb(plat_priv->get_info_cb_ctx,
				       (void *)ind_msg->data,
				       ind_msg->data_len);
}

static int cnss_ims_wfc_call_twt_cfg_send_sync
	(struct cnss_plat_data *plat_priv,
	 const struct wlfw_wfc_call_twt_config_ind_msg_v01 *ind_msg)
{
	struct ims_private_service_wfc_call_twt_config_req_msg_v01 *req;
	struct ims_private_service_wfc_call_twt_config_rsp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!test_bit(CNSS_IMS_CONNECTED, &plat_priv->driver_state)) {
		cnss_pr_err("Drop FW WFC indication as IMS QMI not connected\n");
		return -EINVAL;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->twt_sta_start_valid = ind_msg->twt_sta_start_valid;
	req->twt_sta_start = ind_msg->twt_sta_start;
	req->twt_sta_int_valid = ind_msg->twt_sta_int_valid;
	req->twt_sta_int = ind_msg->twt_sta_int;
	req->twt_sta_upo_valid = ind_msg->twt_sta_upo_valid;
	req->twt_sta_upo = ind_msg->twt_sta_upo;
	req->twt_sta_sp_valid = ind_msg->twt_sta_sp_valid;
	req->twt_sta_sp = ind_msg->twt_sta_sp;
	req->twt_sta_dl_valid = req->twt_sta_dl_valid;
	req->twt_sta_dl = req->twt_sta_dl;
	req->twt_sta_config_changed_valid =
				ind_msg->twt_sta_config_changed_valid;
	req->twt_sta_config_changed = ind_msg->twt_sta_config_changed;

	cnss_pr_dbg("CNSS->IMS: TWT_CFG_REQ: state: 0x%lx\n",
		    plat_priv->driver_state);

	ret =
	qmi_txn_init(&plat_priv->ims_qmi, &txn,
		     ims_private_service_wfc_call_twt_config_rsp_msg_v01_ei,
		     resp);
	if (ret < 0) {
		cnss_pr_err("CNSS->IMS: TWT_CFG_REQ: QMI Txn Init Err: %d\n",
			    ret);
		goto out;
	}

	ret =
	qmi_send_request(&plat_priv->ims_qmi, NULL, &txn,
			 QMI_IMS_PRIVATE_SERVICE_WFC_CALL_TWT_CONFIG_REQ_V01,
		IMS_PRIVATE_SERVICE_WFC_CALL_TWT_CONFIG_REQ_MSG_V01_MAX_MSG_LEN,
		ims_private_service_wfc_call_twt_config_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("CNSS->IMS: TWT_CFG_REQ: QMI Send Err: %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("IMS->CNSS: TWT_CFG_RSP: QMI Wait Err: %d\n", ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("IMS->CNSS: TWT_CFG_RSP: Result: %d Err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}
	ret = 0;
out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_process_twt_cfg_ind_event(struct cnss_plat_data *plat_priv,
				   void *data)
{
	int ret;
	struct wlfw_wfc_call_twt_config_ind_msg_v01 *ind_msg = data;

	ret = cnss_ims_wfc_call_twt_cfg_send_sync(plat_priv, ind_msg);
	kfree(data);
	return ret;
}

static void cnss_wlfw_process_twt_cfg_ind(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn,
					  const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_wfc_call_twt_config_ind_msg_v01 *ind_msg = data;
	struct wlfw_wfc_call_twt_config_ind_msg_v01 *event_data;

	if (!txn) {
		cnss_pr_err("FW->CNSS: TWT_CFG_IND: Spurious indication\n");
		return;
	}

	if (!ind_msg) {
		cnss_pr_err("FW->CNSS: TWT_CFG_IND: Invalid indication\n");
		return;
	}
	cnss_pr_dbg("FW->CNSS: TWT_CFG_IND: %x %llx, %x %x, %x %x, %x %x, %x %x, %x %x\n",
		    ind_msg->twt_sta_start_valid, ind_msg->twt_sta_start,
		    ind_msg->twt_sta_int_valid, ind_msg->twt_sta_int,
		    ind_msg->twt_sta_upo_valid, ind_msg->twt_sta_upo,
		    ind_msg->twt_sta_sp_valid, ind_msg->twt_sta_sp,
		    ind_msg->twt_sta_dl_valid, ind_msg->twt_sta_dl,
		    ind_msg->twt_sta_config_changed_valid,
		    ind_msg->twt_sta_config_changed);

	event_data = kmemdup(ind_msg, sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_WLFW_TWT_CFG_IND, 0,
			       event_data);
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
		.fn = cnss_wlfw_fw_mem_file_save_ind_cb
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
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_WFC_CALL_TWT_CONFIG_IND_V01,
		.ei = wlfw_wfc_call_twt_config_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_wfc_call_twt_config_ind_msg_v01),
		.fn = cnss_wlfw_process_twt_cfg_ind
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
	CNSS_QMI_ASSERT();
	kfree(data);
	return ret;
}

int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv, void *data)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state)) {
		cnss_pr_err("Unexpected WLFW server arrive\n");
		CNSS_ASSERT(0);
		return -EINVAL;
	}

	cnss_ignore_qmi_failure(false);

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
	int ret;

	if (!plat_priv)
		return -ENODEV;

	clear_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service disconnected, state: 0x%lx\n",
		     plat_priv->driver_state);

	cnss_qmi_deinit(plat_priv);

	clear_bit(CNSS_QMI_DEL_SERVER, &plat_priv->driver_state);

	ret = cnss_qmi_init(plat_priv);
	if (ret < 0) {
		cnss_pr_err("QMI WLFW service registraton failed, ret\n", ret);
		CNSS_ASSERT(0);
	}
	return 0;
}

static int wlfw_new_server(struct qmi_handle *qmi_wlfw,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_qmi_event_server_arrive_data *event_data;

	if (plat_priv && test_bit(CNSS_QMI_DEL_SERVER, &plat_priv->driver_state)) {
		cnss_pr_info("WLFW server delete in progress, Ignore server arrive, state: 0x%lx\n",
			     plat_priv->driver_state);
		return 0;
	}

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

	if (plat_priv && test_bit(CNSS_QMI_DEL_SERVER, &plat_priv->driver_state)) {
		cnss_pr_info("WLFW server delete in progress, Ignore server delete, state: 0x%lx\n",
			     plat_priv->driver_state);
		return;
	}

	cnss_pr_dbg("WLFW server exiting\n");

	if (plat_priv) {
		cnss_ignore_qmi_failure(true);
		set_bit(CNSS_QMI_DEL_SERVER, &plat_priv->driver_state);
	}

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
		cnss_pr_err("Failed to initialize WLFW QMI handle, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_add_lookup(&plat_priv->qmi_wlfw, WLFW_SERVICE_ID_V01,
			     WLFW_SERVICE_VERS_V01, WLFW_SERVICE_INS_ID_V01);
	if (ret < 0)
		cnss_pr_err("Failed to add WLFW QMI lookup, err: %d\n", ret);

out:
	return ret;
}

void cnss_qmi_deinit(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->qmi_wlfw);
}

int cnss_qmi_get_dms_mac(struct cnss_plat_data *plat_priv)
{
	struct dms_get_mac_address_req_msg_v01 req;
	struct dms_get_mac_address_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	if  (!test_bit(CNSS_QMI_DMS_CONNECTED, &plat_priv->driver_state)) {
		cnss_pr_err("DMS QMI connection not established\n");
		return -EAGAIN;
	}
	cnss_pr_dbg("Requesting DMS MAC address");

	memset(&resp, 0, sizeof(resp));
	ret = qmi_txn_init(&plat_priv->qmi_dms, &txn,
			   dms_get_mac_address_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for dms, err: %d\n",
			    ret);
		goto out;
	}
	req.device = DMS_DEVICE_MAC_WLAN_V01;
	ret = qmi_send_request(&plat_priv->qmi_dms, NULL, &txn,
			       QMI_DMS_GET_MAC_ADDRESS_REQ_V01,
			       DMS_GET_MAC_ADDRESS_REQ_MSG_V01_MAX_MSG_LEN,
			       dms_get_mac_address_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send QMI_DMS_GET_MAC_ADDRESS_REQ_V01, err: %d\n",
			    ret);
		goto out;
	}
	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for QMI_DMS_GET_MAC_ADDRESS_RESP_V01, err: %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		if (resp.resp.error == DMS_MAC_NOT_PROVISIONED) {
			cnss_pr_err("NV MAC address is not provisioned");
			plat_priv->dms.nv_mac_not_prov = 1;
		} else {
			cnss_pr_err("QMI_DMS_GET_MAC_ADDRESS_REQ_V01 failed, result: %d, err: %d\n",
				    resp.resp.result, resp.resp.error);
		}
		ret = -resp.resp.result;
		goto out;
	}
	if (!resp.mac_address_valid ||
	    resp.mac_address_len != QMI_WLFW_MAC_ADDR_SIZE_V01) {
		cnss_pr_err("Invalid MAC address received from DMS\n");
		plat_priv->dms.mac_valid = false;
		goto out;
	}
	plat_priv->dms.mac_valid = true;
	memcpy(plat_priv->dms.mac, resp.mac_address, QMI_WLFW_MAC_ADDR_SIZE_V01);
	cnss_pr_info("Received DMS MAC: [%pM]\n", plat_priv->dms.mac);
out:
	return ret;
}

static int cnss_dms_connect_to_server(struct cnss_plat_data *plat_priv,
				      unsigned int node, unsigned int port)
{
	struct qmi_handle *qmi_dms = &plat_priv->qmi_dms;
	struct sockaddr_qrtr sq = {0};
	int ret = 0;

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = node;
	sq.sq_port = port;

	ret = kernel_connect(qmi_dms->sock, (struct sockaddr *)&sq,
			     sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Failed to connect to QMI DMS remote service Node: %d Port: %d\n",
			    node, port);
		goto out;
	}

	set_bit(CNSS_QMI_DMS_CONNECTED, &plat_priv->driver_state);
	cnss_pr_info("QMI DMS service connected, state: 0x%lx\n",
		     plat_priv->driver_state);
out:
	return ret;
}

static int dms_new_server(struct qmi_handle *qmi_dms,
			  struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_dms, struct cnss_plat_data, qmi_dms);

	if (!service)
		return -EINVAL;

	return cnss_dms_connect_to_server(plat_priv, service->node,
					  service->port);
}

static void dms_del_server(struct qmi_handle *qmi_dms,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_dms, struct cnss_plat_data, qmi_dms);

	clear_bit(CNSS_QMI_DMS_CONNECTED, &plat_priv->driver_state);
	cnss_pr_info("QMI DMS service disconnected, state: 0x%lx\n",
		     plat_priv->driver_state);
}

static struct qmi_ops qmi_dms_ops = {
	.new_server = dms_new_server,
	.del_server = dms_del_server,
};

int cnss_dms_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = qmi_handle_init(&plat_priv->qmi_dms, DMS_QMI_MAX_MSG_LEN,
			      &qmi_dms_ops, NULL);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize DMS handle, err: %d\n", ret);
		goto out;
	}

	ret = qmi_add_lookup(&plat_priv->qmi_dms, DMS_SERVICE_ID_V01,
			     DMS_SERVICE_VERS_V01, 0);
	if (ret < 0)
		cnss_pr_err("Failed to add DMS lookup, err: %d\n", ret);
out:
	return ret;
}

void cnss_dms_deinit(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->qmi_dms);
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

int cnss_process_wfc_call_ind_event(struct cnss_plat_data *plat_priv,
				    void *data)
{
	int ret;
	struct ims_private_service_wfc_call_status_ind_msg_v01 *ind_msg = data;

	ret = cnss_wlfw_wfc_call_status_send_sync(plat_priv, ind_msg);
	kfree(data);
	return ret;
}

static void
cnss_ims_process_wfc_call_ind_cb(struct qmi_handle *ims_qmi,
				 struct sockaddr_qrtr *sq,
				 struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(ims_qmi, struct cnss_plat_data, ims_qmi);
	const
	struct ims_private_service_wfc_call_status_ind_msg_v01 *ind_msg = data;
	struct ims_private_service_wfc_call_status_ind_msg_v01 *event_data;

	if (!txn) {
		cnss_pr_err("IMS->CNSS: WFC_CALL_IND: Spurious indication\n");
		return;
	}

	if (!ind_msg) {
		cnss_pr_err("IMS->CNSS: WFC_CALL_IND: Invalid indication\n");
		return;
	}
	cnss_pr_dbg("IMS->CNSS: WFC_CALL_IND: %x, %x %x, %x %x, %x %llx, %x %x, %x %x\n",
		    ind_msg->wfc_call_active, ind_msg->all_wfc_calls_held_valid,
		    ind_msg->all_wfc_calls_held,
		    ind_msg->is_wfc_emergency_valid, ind_msg->is_wfc_emergency,
		    ind_msg->twt_ims_start_valid, ind_msg->twt_ims_start,
		    ind_msg->twt_ims_int_valid, ind_msg->twt_ims_int,
		    ind_msg->media_quality_valid, ind_msg->media_quality);

	event_data = kmemdup(ind_msg, sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_IMS_WFC_CALL_IND,
			       0, event_data);
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
		.fn = cnss_ims_process_wfc_call_ind_cb
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
