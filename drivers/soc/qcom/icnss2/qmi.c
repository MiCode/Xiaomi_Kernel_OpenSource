// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "icnss2_qmi: " fmt

#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ipc_logging.h>
#include <linux/thread_info.h>
#include <linux/firmware.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/platform_device.h>
#include <soc/qcom/icnss2.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include "wlan_firmware_service_v01.h"
#include "main.h"
#include "qmi.h"
#include "debug.h"
#include "genl.h"

#define WLFW_SERVICE_WCN_INS_ID_V01	3
#define WLFW_SERVICE_INS_ID_V01		0
#define WLFW_CLIENT_ID			0x4b4e454c
#define QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED	0x77

#define MAX_BDF_FILE_NAME		13
#define BDF_FILE_NAME_PREFIX		"bdwlan"
#define ELF_BDF_FILE_NAME		"bdwlan.elf"
#define ELF_BDF_FILE_NAME_PREFIX	"bdwlan.e"
#define BIN_BDF_FILE_NAME		"bdwlan.bin"
#define BIN_BDF_FILE_NAME_PREFIX	"bdwlan.b"
#define REGDB_FILE_NAME			"regdb.bin"
#define DUMMY_BDF_FILE_NAME		"bdwlan.dmy"

#define QDSS_TRACE_CONFIG_FILE "qdss_trace_config.cfg"

#define DEVICE_BAR_SIZE			0x200000
#define M3_SEGMENT_ADDR_MASK		0xFFFFFFFF

#ifdef CONFIG_ICNSS2_DEBUG
bool ignore_fw_timeout;
#define ICNSS_QMI_ASSERT() ICNSS_ASSERT(ignore_fw_timeout)
#else
#define ICNSS_QMI_ASSERT() do { } while (0)
#endif

#ifdef CONFIG_ICNSS2_DEBUG
void icnss_ignore_fw_timeout(bool ignore)
{
	ignore_fw_timeout = ignore;
}
#else
void icnss_ignore_fw_timeout(bool ignore) { }
#endif

#define icnss_qmi_fatal_err(_fmt, ...) do {		\
	icnss_pr_err("fatal: "_fmt, ##__VA_ARGS__);	\
	ICNSS_QMI_ASSERT();				\
	} while (0)

int wlfw_msa_mem_info_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	int i;
	struct wlfw_msa_info_req_msg_v01 *req;
	struct wlfw_msa_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	uint64_t max_mapped_addr;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending MSA mem info, state: 0x%lx\n", priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->msa_addr = priv->msa_pa;
	req->size = priv->msa_mem_size;

	priv->stats.msa_info_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_msa_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err(
				"Fail to init txn for MSA Mem info resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_MSA_INFO_REQ_V01,
			       WLFW_MSA_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_msa_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send MSA Mem info req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err("MSA Mem info resp wait failed ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
		"QMI MSA Mem info request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	icnss_pr_dbg("Receive mem_region_info_len: %d\n",
		     resp->mem_region_info_len);

	if (resp->mem_region_info_len > QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01) {
		icnss_qmi_fatal_err(
			"Invalid memory region length received: %d\n",
			 resp->mem_region_info_len);
		ret = -EINVAL;
		goto out;
	}

	max_mapped_addr = priv->msa_pa + priv->msa_mem_size;
	priv->stats.msa_info_resp++;
	priv->nr_mem_region = resp->mem_region_info_len;
	for (i = 0; i < resp->mem_region_info_len; i++) {

		if (resp->mem_region_info[i].size > priv->msa_mem_size ||
		    resp->mem_region_info[i].region_addr >= max_mapped_addr ||
		    resp->mem_region_info[i].region_addr < priv->msa_pa ||
		    resp->mem_region_info[i].size +
		    resp->mem_region_info[i].region_addr > max_mapped_addr) {
			icnss_pr_dbg("Received out of range Addr: 0x%llx Size: 0x%x\n",
					resp->mem_region_info[i].region_addr,
					resp->mem_region_info[i].size);
			ret = -EINVAL;
			goto fail_unwind;
		}

		priv->mem_region[i].reg_addr =
			resp->mem_region_info[i].region_addr;
		priv->mem_region[i].size =
			resp->mem_region_info[i].size;
		priv->mem_region[i].secure_flag =
			resp->mem_region_info[i].secure_flag;
		icnss_pr_dbg("Memory Region: %d Addr: 0x%llx Size: 0x%x Flag: 0x%08x\n",
			     i, priv->mem_region[i].reg_addr,
			     priv->mem_region[i].size,
			     priv->mem_region[i].secure_flag);
	}

	kfree(resp);
	kfree(req);
	return 0;

fail_unwind:
	memset(&priv->mem_region[0], 0, sizeof(priv->mem_region[0]) * i);
out:
	kfree(resp);
	kfree(req);
	priv->stats.msa_info_err++;
	return ret;
}

int wlfw_msa_ready_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_msa_ready_req_msg_v01 *req;
	struct wlfw_msa_ready_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending MSA ready request message, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	priv->stats.msa_ready_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_msa_ready_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err(
				"Fail to init txn for MSA Mem Ready resp %d\n",
				ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_MSA_READY_REQ_V01,
			       WLFW_MSA_READY_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_msa_ready_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send MSA Mem Ready req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err(
				"MSA Mem Ready resp wait failed with ret %d\n",
				ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
		   "QMI MSA Mem Ready request rejected, result:%d error:%d\n",
		   resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.msa_ready_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.msa_ready_err++;
	return ret;
}

int wlfw_device_info_send_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_device_info_req_msg_v01 *req;
	struct wlfw_device_info_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending Device Info request message, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	priv->stats.device_info_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_device_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err(
				"Fail to init txn for Device Info resp %d\n",
				ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_DEVICE_INFO_REQ_V01,
			       WLFW_DEVICE_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_device_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send device info req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err(
				"Device Info resp wait failed with ret %d\n",
				ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
		   "QMI Device info request rejected, result:%d error:%d\n",
		   resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.device_info_resp++;

	if (resp->bar_addr_valid)
		priv->mem_base_pa = resp->bar_addr;

	if (resp->bar_size_valid)
		priv->mem_base_size = resp->bar_size;

	if (!priv->mem_base_pa) {
		ret = -EINVAL;
		icnss_qmi_fatal_err("Fail to get bar address\n");
		goto out;
	}

	if (priv->mem_base_size <  DEVICE_BAR_SIZE) {
		ret = -EINVAL;
		icnss_qmi_fatal_err("Bar size is not proper 0x%x\n",
				    priv->mem_base_size);
		goto out;
	}

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.device_info_err++;
	return ret;
}

int wlfw_power_save_send_msg(struct icnss_priv *priv,
			     enum wlfw_power_save_mode_v01 mode)
{
	int ret;
	struct wlfw_power_save_req_msg_v01 *req;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return -EINVAL;

	if (test_bit(ICNSS_PD_RESTART, &priv->state) ||
	    !test_bit(ICNSS_MODE_ON, &priv->state))
		return 0;

	icnss_pr_dbg("Sending power save mode: %d, state: 0x%lx\n",
		     mode, priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->power_save_mode_valid = 1;
	req->power_save_mode = mode;

	if (mode == WLFW_POWER_SAVE_EXIT_V01)
		priv->stats.exit_power_save_req++;
	else
		priv->stats.enter_power_save_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   NULL, NULL);
	if (ret < 0) {
		icnss_qmi_fatal_err("Fail to init txn for exit power save%d\n",
				    ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_POWER_SAVE_REQ_V01,
			       WLFW_POWER_SAVE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_power_save_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send exit power save req %d\n",
				    ret);
		goto out;
	}

	qmi_txn_cancel(&txn);

	if (mode == WLFW_POWER_SAVE_EXIT_V01)
		priv->stats.exit_power_save_resp++;
	else
		priv->stats.enter_power_save_resp++;

	kfree(req);
	return 0;

out:
	kfree(req);

	if (mode == WLFW_POWER_SAVE_EXIT_V01)
		priv->stats.exit_power_save_err++;
	else
		priv->stats.enter_power_save_err++;
	return ret;
}

int wlfw_send_soc_wake_msg(struct icnss_priv *priv,
			   enum wlfw_soc_wake_enum_v01 type)
{
	int ret;
	struct wlfw_soc_wake_req_msg_v01 *req;
	struct wlfw_soc_wake_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return -EINVAL;

	icnss_pr_dbg("Sending soc wake msg, type: 0x%x\n",
		     type);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}
	req->wake_valid = 1;
	req->wake = type;

	priv->stats.soc_wake_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_soc_wake_resp_msg_v01_ei, resp);

	if (ret < 0) {
		icnss_pr_err("Fail to init txn for wake msg resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_SOC_WAKE_REQ_V01,
			       WLFW_SOC_WAKE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_soc_wake_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send soc wake msg %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err("SOC wake timed out with ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
			"SOC wake request rejected,result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.soc_wake_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(req);
	kfree(resp);
	priv->stats.soc_wake_err++;
	return ret;
}

int wlfw_ind_register_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_ind_register_req_msg_v01 *req;
	struct wlfw_ind_register_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		     priv->state);

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
	req->pin_connect_result_enable_valid = 1;
	req->pin_connect_result_enable = 1;

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		req->msa_ready_enable_valid = 1;
		req->msa_ready_enable = 1;
		if (test_bit(FW_REJUVENATE_ENABLE,
			 &priv->ctrl_params.quirks)) {
			req->rejuvenate_enable_valid = 1;
			req->rejuvenate_enable = 1;
		}
	} else if (priv->device_id == WCN6750_DEVICE_ID) {
		req->fw_init_done_enable_valid = 1;
		req->fw_init_done_enable = 1;
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
		req->m3_dump_upload_segments_req_enable_valid = 1;
		req->m3_dump_upload_segments_req_enable = 1;
	}

	priv->stats.ind_register_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_ind_register_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err(
				"Fail to init txn for Ind Register resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_IND_REGISTER_REQ_V01,
			       WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ind_register_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send Ind Register req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err(
			"Ind Register resp wait failed with ret %d\n",
			 ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
		    "QMI Ind Register request rejected, result:%d error:%d\n",
		     resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.ind_register_resp++;

	if (resp->fw_status_valid &&
	   (resp->fw_status & QMI_WLFW_ALREADY_REGISTERED_V01)) {
		ret = -EALREADY;
		icnss_pr_dbg("WLFW already registered\n");
		goto qmi_registered;
	}

	kfree(resp);
	kfree(req);

	return 0;

out:
	priv->stats.ind_register_err++;
qmi_registered:
	kfree(resp);
	kfree(req);
	return ret;
}

int wlfw_cap_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_cap_req_msg_v01 *req;
	struct wlfw_cap_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending target capability message, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	priv->stats.cap_req++;

	ret = qmi_txn_init(&priv->qmi, &txn, wlfw_cap_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err("Fail to init txn for Capability resp %d\n",
				    ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_CAP_REQ_V01,
			       WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_cap_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send Capability req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err("Capability resp wait failed with ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = -resp->resp.result;
		if (resp->resp.error == QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED) {
			icnss_pr_err("RF card not present\n");
			goto out;
		}
		icnss_qmi_fatal_err(
			"QMI Capability request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		goto out;
	}

	priv->stats.cap_resp++;

	if (resp->chip_info_valid) {
		priv->chip_info.chip_id = resp->chip_info.chip_id;
		priv->chip_info.chip_family = resp->chip_info.chip_family;
	}
	if (resp->board_info_valid)
		priv->board_id = resp->board_info.board_id;
	else
		priv->board_id = 0xFF;
	if (resp->soc_info_valid)
		priv->soc_id = resp->soc_info.soc_id;
	if (resp->fw_version_info_valid) {
		priv->fw_version_info.fw_version =
			resp->fw_version_info.fw_version;
		strlcpy(priv->fw_version_info.fw_build_timestamp,
				resp->fw_version_info.fw_build_timestamp,
				WLFW_MAX_TIMESTAMP_LEN + 1);
	}

	if (resp->voltage_mv_valid) {
		priv->cpr_info.voltage = resp->voltage_mv;
		icnss_pr_dbg("Voltage for CPR: %dmV\n",
			    priv->cpr_info.voltage);
		icnss_update_cpr_info(priv);
	}

	if (resp->fw_build_id_valid)
		strlcpy(priv->fw_build_id, resp->fw_build_id,
			QMI_WLFW_MAX_BUILD_ID_LEN_V01 + 1);

	icnss_pr_dbg("Capability, chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s, fw_build_id: %s",
		     priv->chip_info.chip_id, priv->chip_info.chip_family,
		     priv->board_id, priv->soc_id,
		     priv->fw_version_info.fw_version,
		     priv->fw_version_info.fw_build_timestamp,
		     priv->fw_build_id);

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.cap_err++;
	return ret;
}

static int icnss_get_bdf_file_name(struct icnss_priv *priv,
				   u32 bdf_type, char *filename,
				   u32 filename_len)
{
	int ret = 0;

	switch (bdf_type) {
	case ICNSS_BDF_ELF:
		if (priv->board_id == 0xFF)
			snprintf(filename, filename_len, ELF_BDF_FILE_NAME);
		else if (priv->board_id < 0xFF)
			snprintf(filename, filename_len,
				 ELF_BDF_FILE_NAME_PREFIX "%02x",
				 priv->board_id);
		else
			snprintf(filename, filename_len,
				 BDF_FILE_NAME_PREFIX "%02x.e%02x",
				 priv->board_id >> 8 & 0xFF,
				 priv->board_id & 0xFF);
		break;
	case ICNSS_BDF_BIN:
		if (priv->board_id == 0xFF)
			snprintf(filename, filename_len, BIN_BDF_FILE_NAME);
		else if (priv->board_id < 0xFF)
			snprintf(filename, filename_len,
				 BIN_BDF_FILE_NAME_PREFIX "%02x",
				 priv->board_id);
		else
			snprintf(filename, filename_len,
				 BDF_FILE_NAME_PREFIX "%02x.b%02x",
				 priv->board_id >> 8 & 0xFF,
				 priv->board_id & 0xFF);
		break;
	case ICNSS_BDF_REGDB:
		snprintf(filename, filename_len, REGDB_FILE_NAME);
		break;
	case ICNSS_BDF_DUMMY:
		icnss_pr_dbg("CNSS_BDF_DUMMY is set, sending dummy BDF\n");
		snprintf(filename, filename_len, DUMMY_BDF_FILE_NAME);
		ret = MAX_BDF_FILE_NAME;
		break;
	default:
		icnss_pr_err("Invalid BDF type: %d\n",
			     priv->ctrl_params.bdf_type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int icnss_wlfw_bdf_dnld_send_sync(struct icnss_priv *priv, u32 bdf_type)
{
	struct wlfw_bdf_download_req_msg_v01 *req;
	struct wlfw_bdf_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	char filename[MAX_BDF_FILE_NAME];
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	unsigned int remaining;
	int ret = 0;

	icnss_pr_dbg("Sending BDF download message, state: 0x%lx, type: %d\n",
		     priv->state, bdf_type);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = icnss_get_bdf_file_name(priv, bdf_type,
				      filename, sizeof(filename));
	if (ret > 0) {
		temp = DUMMY_BDF_FILE_NAME;
		remaining = MAX_BDF_FILE_NAME;
		goto bypass_bdf;
	} else if (ret < 0) {
		goto err_req_fw;
	}

	ret = request_firmware(&fw_entry, filename, &priv->pdev->dev);
	if (ret) {
		icnss_pr_err("Failed to load BDF: %s\n", filename);
		goto err_req_fw;
	}

	temp = fw_entry->data;
	remaining = fw_entry->size;

bypass_bdf:
	icnss_pr_dbg("Downloading BDF: %s, size: %u\n", filename, remaining);

	while (remaining) {
		req->valid = 1;
		req->file_id_valid = 1;
		req->file_id = priv->board_id;
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

		ret = qmi_txn_init(&priv->qmi, &txn,
				   wlfw_bdf_download_resp_msg_v01_ei, resp);
		if (ret < 0) {
			icnss_pr_err("Failed to initialize txn for BDF download request, err: %d\n",
				      ret);
			goto err_send;
		}

		ret = qmi_send_request
			(&priv->qmi, NULL, &txn,
			 QMI_WLFW_BDF_DOWNLOAD_REQ_V01,
			 WLFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_bdf_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			icnss_pr_err("Failed to send respond BDF download request, err: %d\n",
				      ret);
			goto err_send;
		}

		ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
		if (ret < 0) {
			icnss_pr_err("Failed to wait for response of BDF download request, err: %d\n",
				      ret);
			goto err_send;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			icnss_pr_err("BDF download request failed, result: %d, err: %d\n",
				      resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			goto err_send;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

	if (bdf_type != ICNSS_BDF_DUMMY)
		release_firmware(fw_entry);

	kfree(req);
	kfree(resp);
	return 0;

err_send:
	if (bdf_type != ICNSS_BDF_DUMMY)
		release_firmware(fw_entry);
err_req_fw:
	if (bdf_type != ICNSS_BDF_REGDB)
		ICNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int icnss_wlfw_qdss_data_send_sync(struct icnss_priv *priv, char *file_name,
				   u32 total_size)
{
	int ret = 0;
	struct wlfw_qdss_trace_data_req_msg_v01 *req;
	struct wlfw_qdss_trace_data_resp_msg_v01 *resp;
	unsigned char *p_qdss_trace_data_temp, *p_qdss_trace_data = NULL;
	unsigned int remaining;
	struct qmi_txn txn;

	icnss_pr_dbg("%s", __func__);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	p_qdss_trace_data = kzalloc(total_size, GFP_KERNEL);
	if (p_qdss_trace_data == NULL) {
		ret = ENOMEM;
		goto end;
	}

	remaining = total_size;
	p_qdss_trace_data_temp = p_qdss_trace_data;
	while (remaining && resp->end == 0) {

		ret = qmi_txn_init(&priv->qmi, &txn,
				   wlfw_qdss_trace_data_resp_msg_v01_ei, resp);

		if (ret < 0) {
			icnss_pr_err("Fail to init txn for QDSS trace resp %d\n",
				     ret);
			goto fail;
		}

		ret = qmi_send_request
			(&priv->qmi, NULL, &txn,
			 QMI_WLFW_QDSS_TRACE_DATA_REQ_V01,
			 WLFW_QDSS_TRACE_DATA_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_qdss_trace_data_req_msg_v01_ei, req);

		if (ret < 0) {
			qmi_txn_cancel(&txn);
			icnss_pr_err("Fail to send QDSS trace data req %d\n",
				     ret);
			goto fail;
		}

		ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);

		if (ret < 0) {
			icnss_pr_err("QDSS trace resp wait failed with rc %d\n",
				     ret);
			goto fail;
		} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			icnss_pr_err("QMI QDSS trace request rejected, result:%d error:%d\n",
				     resp->resp.result, resp->resp.error);
				     ret = -resp->resp.result;
			goto fail;
		} else {
			ret = 0;
		}

		icnss_pr_dbg("%s: response total size  %d data len %d",
			     __func__, resp->total_size, resp->data_len);

		if ((resp->total_size_valid == 1 &&
		    resp->total_size == total_size)
		   && (resp->seg_id_valid == 1 && resp->seg_id == req->seg_id)
		   && (resp->data_valid == 1 &&
		resp->data_len <= QMI_WLFW_MAX_DATA_SIZE_V01)) {

			memcpy(p_qdss_trace_data_temp,
			       resp->data, resp->data_len);
		} else {
			icnss_pr_err("%s: Unmatched qdss trace data, Expect total_size %u, seg_id %u, Recv total_size_valid %u, total_size %u, seg_id_valid %u, seg_id %u, data_len_valid %u, data_len %u",
				     __func__,
				     total_size, req->seg_id,
				     resp->total_size_valid,
				     resp->total_size,
				     resp->seg_id_valid,
				     resp->seg_id,
				     resp->data_valid,
				     resp->data_len);
			ret = -EINVAL;
			goto fail;
		}

		remaining -= resp->data_len;
		p_qdss_trace_data_temp += resp->data_len;
		req->seg_id++;
	}

	if (remaining == 0 && (resp->end_valid && resp->end)) {
		ret = icnss_genl_send_msg(p_qdss_trace_data,
					 ICNSS_GENL_MSG_TYPE_QDSS, file_name,
					 total_size);
		if (ret < 0) {
			icnss_pr_err("Fail to save QDSS trace data: %d\n",
				     ret);
		ret = -EINVAL;
		}
	} else {
		icnss_pr_err("%s: QDSS trace file corrupted: remaining %u, end_valid %u, end %u",
			     __func__,
			     remaining, resp->end_valid, resp->end);
		ret = -EINVAL;
	}

fail:
	kfree(p_qdss_trace_data);

end:
	kfree(req);
	kfree(resp);
	return ret;
}

int icnss_wlfw_qdss_dnld_send_sync(struct icnss_priv *priv)
{
	struct wlfw_qdss_trace_config_download_req_msg_v01 *req;
	struct wlfw_qdss_trace_config_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	unsigned int remaining;
	int ret = 0;

	icnss_pr_dbg("Sending QDSS config download message, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = request_firmware(&fw_entry, QDSS_TRACE_CONFIG_FILE,
			       &priv->pdev->dev);
	if (ret) {
		icnss_pr_err("Failed to load QDSS: %s\n",
			     QDSS_TRACE_CONFIG_FILE);
		goto err_req_fw;
	}

	temp = fw_entry->data;
	remaining = fw_entry->size;

	icnss_pr_dbg("Downloading QDSS: %s, size: %u\n",
		     QDSS_TRACE_CONFIG_FILE, remaining);

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
			(&priv->qmi, &txn,
			 wlfw_qdss_trace_config_download_resp_msg_v01_ei,
			 resp);
		if (ret < 0) {
			icnss_pr_err("Failed to initialize txn for QDSS download request, err: %d\n",
				      ret);
			goto err_send;
		}

		ret = qmi_send_request
		      (&priv->qmi, NULL, &txn,
		       QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01,
		       WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
		       wlfw_qdss_trace_config_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			icnss_pr_err("Failed to send respond QDSS download request, err: %d\n",
				      ret);
			goto err_send;
		}

		ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
		if (ret < 0) {
			icnss_pr_err("Failed to wait for response of QDSS download request, err: %d\n",
				      ret);
			goto err_send;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			icnss_pr_err("QDSS download request failed, result: %d, err: %d\n",
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

int wlfw_wlan_mode_send_sync_msg(struct icnss_priv *priv,
		enum wlfw_driver_mode_enum_v01 mode)
{
	int ret;
	struct wlfw_wlan_mode_req_msg_v01 *req;
	struct wlfw_wlan_mode_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	/* During recovery do not send mode request for WLAN OFF as
	 * FW not able to process it.
	 */
	if (test_bit(ICNSS_PD_RESTART, &priv->state) &&
	    mode == QMI_WLFW_OFF_V01)
		return 0;

	icnss_pr_dbg("Sending Mode request, state: 0x%lx, mode: %d\n",
		     priv->state, mode);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mode = mode;
	req->hw_debug_valid = 1;
	req->hw_debug = !!test_bit(HW_DEBUG_ENABLE, &priv->ctrl_params.quirks);

	priv->stats.mode_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_wlan_mode_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err("Fail to init txn for Mode resp %d\n", ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_WLAN_MODE_REQ_V01,
			       WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_mode_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send Mode req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err("Mode resp wait failed with ret %d\n", ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
			"QMI Mode request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.mode_resp++;

	if (mode == QMI_WLFW_OFF_V01) {
		icnss_pr_dbg("Clear mode on 0x%lx, mode: %d\n",
			     priv->state, mode);
		clear_bit(ICNSS_MODE_ON, &priv->state);
	} else {
		icnss_pr_dbg("Set mode on 0x%lx, mode: %d\n",
			     priv->state, mode);
		set_bit(ICNSS_MODE_ON, &priv->state);
	}

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.mode_req_err++;
	return ret;
}

static int wlfw_send_qdss_trace_mode_req
		(struct icnss_priv *priv,
		 enum wlfw_qdss_trace_mode_enum_v01 mode,
		 unsigned long long option)
{
	int rc = 0;
	int tmp = 0;
	struct wlfw_qdss_trace_mode_req_msg_v01 *req;
	struct wlfw_qdss_trace_mode_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
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

	tmp = priv->hw_trc_override;

	req->hw_trc_disable_override_valid = 1;
	req->hw_trc_disable_override =
	(tmp > QMI_PARAM_DISABLE_V01 ? QMI_PARAM_DISABLE_V01 :
		 (tmp < 0 ? QMI_PARAM_INVALID_V01 : tmp));

	icnss_pr_dbg("%s: mode %u, option %llu, hw_trc_disable_override: %u",
		     __func__, mode, option, req->hw_trc_disable_override);

	rc = qmi_txn_init(&priv->qmi, &txn,
			  wlfw_qdss_trace_mode_resp_msg_v01_ei, resp);
	if (rc < 0) {
		icnss_qmi_fatal_err("Fail to init txn for QDSS Mode resp %d\n",
				    rc);
		goto out;
	}

	rc = qmi_send_request(&priv->qmi, NULL, &txn,
			      QMI_WLFW_QDSS_TRACE_MODE_REQ_V01,
			      WLFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			      wlfw_qdss_trace_mode_req_msg_v01_ei, req);
	if (rc < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send QDSS Mode req %d\n", rc);
		goto out;
	}

	rc = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (rc < 0) {
		icnss_qmi_fatal_err("QDSS Mode resp wait failed with rc %d\n",
				    rc);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
				"QMI QDSS Mode request rejected, result:%d error:%d\n",
				resp->resp.result, resp->resp.error);
		rc = -resp->resp.result;
		goto out;
	}

out:
	kfree(resp);
	kfree(req);
	return rc;
}

int wlfw_qdss_trace_start(struct icnss_priv *priv)
{
	return wlfw_send_qdss_trace_mode_req(priv,
					     QMI_WLFW_QDSS_TRACE_ON_V01, 0);
}

int wlfw_qdss_trace_stop(struct icnss_priv *priv, unsigned long long option)
{
	return wlfw_send_qdss_trace_mode_req(priv, QMI_WLFW_QDSS_TRACE_OFF_V01,
					     option);
}

int wlfw_wlan_cfg_send_sync_msg(struct icnss_priv *priv,
				struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	int ret;
	struct wlfw_wlan_cfg_req_msg_v01 *req;
	struct wlfw_wlan_cfg_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending config request, state: 0x%lx\n", priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	memcpy(req, data, sizeof(*req));

	priv->stats.cfg_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_wlan_cfg_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err("Fail to init txn for Config resp %d\n",
				    ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_WLAN_CFG_REQ_V01,
			       WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_cfg_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send Config req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err("Config resp wait failed with ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
			"QMI Config request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.cfg_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.cfg_req_err++;
	return ret;
}

int wlfw_send_modem_shutdown_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_shutdown_req_msg_v01 *req;
	struct wlfw_shutdown_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending modem shutdown request, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->shutdown_valid = 1;
	req->shutdown = 1;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_shutdown_resp_msg_v01_ei, resp);

	if (ret < 0) {
		icnss_pr_err("Fail to init txn for shutdown resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_SHUTDOWN_REQ_V01,
			       WLFW_SHUTDOWN_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_shutdown_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send Shutdown req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Shutdown resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI modem shutdown request rejected result:%d error:%d\n",
			     resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

out:
	kfree(resp);
	kfree(req);
	return ret;
}

int wlfw_ini_send_sync_msg(struct icnss_priv *priv, uint8_t fw_log_mode)
{
	int ret;
	struct wlfw_ini_req_msg_v01 *req;
	struct wlfw_ini_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log_mode: %d\n",
		     priv->state, fw_log_mode);

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

	priv->stats.ini_req++;

	ret = qmi_txn_init(&priv->qmi, &txn, wlfw_ini_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err("Fail to init txn for INI resp %d\n", ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_INI_REQ_V01,
			       WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ini_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send INI req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err("INI resp wait failed with ret %d\n", ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
			"QMI INI request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.ini_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.ini_req_err++;
	return ret;
}

int wlfw_athdiag_read_send_sync_msg(struct icnss_priv *priv,
					   uint32_t offset, uint32_t mem_type,
					   uint32_t data_len, uint8_t *data)
{
	int ret;
	struct wlfw_athdiag_read_req_msg_v01 *req;
	struct wlfw_athdiag_read_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Diag read: state 0x%lx, offset %x, mem_type %x, data_len %u\n",
		     priv->state, offset, mem_type, data_len);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return  -ENOMEM;
	}

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_athdiag_read_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Fail to init txn for Athdiag Read resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_ATHDIAG_READ_REQ_V01,
			       WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_read_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send Athdiag Read req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Athdaig Read resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI Athdiag Read request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	} else {
		ret = 0;
	}

	if (!resp->data_valid || resp->data_len < data_len) {
		icnss_pr_err("Athdiag read data is invalid, data_valid = %u, data_len = %u\n",
			     resp->data_valid, resp->data_len);
		ret = -EINVAL;
		goto out;
	}

	memcpy(data, resp->data, resp->data_len);

out:
	kfree(resp);
	kfree(req);
	return ret;
}

int wlfw_athdiag_write_send_sync_msg(struct icnss_priv *priv,
					    uint32_t offset, uint32_t mem_type,
					    uint32_t data_len, uint8_t *data)
{
	int ret;
	struct wlfw_athdiag_write_req_msg_v01 *req;
	struct wlfw_athdiag_write_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Diag write: state 0x%lx, offset %x, mem_type %x, data_len %u, data %pK\n",
		     priv->state, offset, mem_type, data_len, data);

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

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_athdiag_write_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Fail to init txn for Athdiag Write resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_ATHDIAG_WRITE_REQ_V01,
			       WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_write_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send Athdiag Write req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Athdiag Write resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI Athdiag Write request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	} else {
		ret = 0;
	}

out:
	kfree(resp);
	kfree(req);
	return ret;
}

int wlfw_rejuvenate_ack_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_rejuvenate_ack_req_msg_v01 *req;
	struct wlfw_rejuvenate_ack_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Sending rejuvenate ack request, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	priv->stats.rejuvenate_ack_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_rejuvenate_ack_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err(
			"Fail to init txn for Rejuvenate Ack resp %d\n",
			 ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_REJUVENATE_ACK_REQ_V01,
			       WLFW_REJUVENATE_ACK_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_rejuvenate_ack_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_qmi_fatal_err("Fail to send Rejuvenate Ack req %d\n",
				    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_qmi_fatal_err(
			     "Rejuvenate Ack resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err(
		   "QMI Rejuvenate Ack request rejected, result:%d error:%d\n",
		    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.rejuvenate_ack_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.rejuvenate_ack_err++;
	return ret;
}

int wlfw_dynamic_feature_mask_send_sync_msg(struct icnss_priv *priv,
					   uint64_t dynamic_feature_mask)
{
	int ret;
	struct wlfw_dynamic_feature_mask_req_msg_v01 *req;
	struct wlfw_dynamic_feature_mask_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	if (!test_bit(ICNSS_WLFW_CONNECTED, &priv->state)) {
		icnss_pr_err("Invalid state for dynamic feature: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	if (!test_bit(FW_REJUVENATE_ENABLE, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("FW rejuvenate is disabled from quirks\n");
		return 0;
	}

	icnss_pr_dbg("Sending dynamic feature mask request, val 0x%llx, state: 0x%lx\n",
		     dynamic_feature_mask, priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mask_valid = 1;
	req->mask = dynamic_feature_mask;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_dynamic_feature_mask_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Fail to init txn for Dynamic Feature Mask resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
		       QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01,
		       WLFW_DYNAMIC_FEATURE_MASK_REQ_MSG_V01_MAX_MSG_LEN,
		       wlfw_dynamic_feature_mask_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send Dynamic Feature Mask req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Dynamic Feature Mask resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI Dynamic Feature Mask request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	icnss_pr_dbg("prev_mask_valid %u, prev_mask 0x%llx, curr_maks_valid %u, curr_mask 0x%llx\n",
		     resp->prev_mask_valid, resp->prev_mask,
		     resp->curr_mask_valid, resp->curr_mask);

out:
	kfree(resp);
	kfree(req);
	return ret;
}

void icnss_handle_rejuvenate(struct icnss_priv *priv)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct icnss_uevent_fw_down_data fw_down_data = {0};

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (event_data == NULL)
		return;

	event_data->crashed = true;
	event_data->fw_rejuvenate = true;
	fw_down_data.crashed = true;
	set_bit(ICNSS_REJUVENATE, &priv->state);

	icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
				 &fw_down_data);
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				0, event_data);
}

int wlfw_qdss_trace_mem_info_send_sync(struct icnss_priv *priv)
{
	struct wlfw_qdss_trace_mem_info_req_msg_v01 *req;
	struct wlfw_qdss_trace_mem_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int ret = 0;
	int i;

	icnss_pr_dbg("Sending QDSS trace mem info, state: 0x%lx\n",
		     priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mem_seg_len = priv->qdss_mem_seg_len;
	for (i = 0; i < req->mem_seg_len; i++) {
		icnss_pr_dbg("Memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
			     qdss_mem[i].va, &qdss_mem[i].pa,
			     qdss_mem[i].size, qdss_mem[i].type);

		req->mem_seg[i].addr = qdss_mem[i].pa;
		req->mem_seg[i].size = qdss_mem[i].size;
		req->mem_seg[i].type = qdss_mem[i].type;
	}

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_qdss_trace_mem_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Fail to initialize txn for QDSS trace mem request: err %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01,
			       WLFW_QDSS_TRACE_MEM_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_qdss_trace_mem_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send QDSS trace mem info request: err %d\n",
			     ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Fail to wait for response of QDSS trace mem info request, err %d\n",
			     ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QDSS trace mem info request failed, result: %d, err: %d\n",
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

int icnss_wlfw_m3_dump_upload_done_send_sync(struct icnss_priv *priv,
					     u32 pdev_id, int status)
{
	struct wlfw_m3_dump_upload_done_req_msg_v01 *req;
	struct wlfw_m3_dump_upload_done_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	icnss_pr_dbg("Sending M3 Upload done req, pdev %d, status %d\n",
		     pdev_id, status);

	req->pdev_id = pdev_id;
	req->status = status;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_m3_dump_upload_done_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Fail to initialize txn for M3 dump upload done req: err %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01,
			       WLFW_M3_DUMP_UPLOAD_DONE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_m3_dump_upload_done_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send M3 dump upload done request: err %d\n",
			     ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Fail to wait for response of M3 dump upload done request, err %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("M3 Dump Upload Done Req failed, result: %d, err: 0x%X\n",
			     resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

out:
	kfree(req);
	kfree(resp);
	return ret;
}

static void fw_ready_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			    struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv =
		container_of(qmi, struct icnss_priv, qmi);

	icnss_pr_dbg("Received FW Ready Indication\n");

	if (!txn) {
		pr_err("spurious indication\n");
		return;
	}

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_FW_READY_IND,
				0, NULL);
}

static void msa_ready_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			     struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);
	struct device *dev = &priv->pdev->dev;
	const struct wlfw_msa_ready_ind_msg_v01 *ind_msg = data;
	uint64_t msa_base_addr = priv->msa_pa;
	phys_addr_t hang_data_phy_addr;

	icnss_pr_dbg("Received MSA Ready Indication\n");

	if (!txn) {
		pr_err("spurious indication\n");
		return;
	}

	priv->stats.msa_ready_ind++;

	/* Check if the length is valid &
	 * the length should not be 0 and
	 * should be <=  WLFW_MAX_HANG_EVENT_DATA_SIZE(400)
	 */

	if (ind_msg->hang_data_length_valid &&
	    ind_msg->hang_data_length &&
	    ind_msg->hang_data_length <= WLFW_MAX_HANG_EVENT_DATA_SIZE)
		priv->hang_event_data_len = ind_msg->hang_data_length;
	else
		goto out;

	/* Check if the offset is valid &
	 * the offset should be in range of 0 to msa_mem_size-hang_data_length
	 */

	if (ind_msg->hang_data_addr_offset_valid &&
	    (ind_msg->hang_data_addr_offset <= (priv->msa_mem_size -
						 ind_msg->hang_data_length)))
		hang_data_phy_addr = msa_base_addr +
						ind_msg->hang_data_addr_offset;
	else
		goto out;

	if (priv->hang_event_data_pa == hang_data_phy_addr)
		goto exit;

	priv->hang_event_data_pa = hang_data_phy_addr;
	priv->hang_event_data_va = devm_ioremap(dev, priv->hang_event_data_pa,
						ind_msg->hang_data_length);

	if (!priv->hang_event_data_va) {
		icnss_pr_err("Hang Data ioremap failed: phy addr: %pa\n",
			     &priv->hang_event_data_pa);
		goto fail;
	}
exit:
	icnss_pr_dbg("Hang Event Data details,Offset:0x%x, Length:0x%x,va_addr: 0x%pK\n",
		     ind_msg->hang_data_addr_offset,
		     ind_msg->hang_data_length,
		     priv->hang_event_data_va);

	return;

out:
	icnss_pr_err("Invalid Hang Data details, Offset:0x%x, Length:0x%x",
		     ind_msg->hang_data_addr_offset,
		     ind_msg->hang_data_length);
fail:
	priv->hang_event_data_va = NULL;
	priv->hang_event_data_pa = 0;
	priv->hang_event_data_len = 0;
}

static void pin_connect_result_ind_cb(struct qmi_handle *qmi,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);
	const struct wlfw_pin_connect_result_ind_msg_v01 *ind_msg = data;

	icnss_pr_dbg("Received Pin Connect Result Indication\n");

	if (!txn) {
		pr_err("spurious indication\n");
		return;
	}

	if (ind_msg->pwr_pin_result_valid)
		priv->pwr_pin_result = ind_msg->pwr_pin_result;
	if (ind_msg->phy_io_pin_result_valid)
		priv->phy_io_pin_result = ind_msg->phy_io_pin_result;
	if (ind_msg->rf_pin_result_valid)
		priv->rf_pin_result = ind_msg->rf_pin_result;

	icnss_pr_dbg("Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		     ind_msg->pwr_pin_result, ind_msg->phy_io_pin_result,
		     ind_msg->rf_pin_result);
	priv->stats.pin_connect_result++;
}

static void rejuvenate_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			      struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);
	const struct wlfw_rejuvenate_ind_msg_v01 *ind_msg = data;

	icnss_pr_dbg("Received Rejuvenate Indication\n");

	if (!txn) {
		pr_err("spurious indication\n");
		return;
	}

	icnss_ignore_fw_timeout(true);

	if (ind_msg->cause_for_rejuvenation_valid)
		priv->cause_for_rejuvenation = ind_msg->cause_for_rejuvenation;
	else
		priv->cause_for_rejuvenation = 0;
	if (ind_msg->requesting_sub_system_valid)
		priv->requesting_sub_system = ind_msg->requesting_sub_system;
	else
		priv->requesting_sub_system = 0;
	if (ind_msg->line_number_valid)
		priv->line_number = ind_msg->line_number;
	else
		priv->line_number = 0;
	if (ind_msg->function_name_valid)
		memcpy(priv->function_name, ind_msg->function_name,
		       QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1);
	else
		memset(priv->function_name, 0,
		       QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1);

	icnss_pr_info("Cause for rejuvenation: 0x%x, requesting sub-system: 0x%x, line number: %u, function name: %s\n",
		      priv->cause_for_rejuvenation,
		      priv->requesting_sub_system,
		      priv->line_number,
		      priv->function_name);

	priv->stats.rejuvenate_ind++;

	icnss_handle_rejuvenate(priv);
}

static void cal_done_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			    struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);

	icnss_pr_dbg("Received QMI WLFW calibration done indication\n");

	if (!txn) {
		icnss_pr_err("Spurious indication\n");
		return;
	}

	priv->cal_done = true;
	clear_bit(ICNSS_COLD_BOOT_CAL, &priv->state);
}

static void fw_init_done_ind_cb(struct qmi_handle *qmi,
				struct sockaddr_qrtr *sq,
				struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);
	struct device *dev = &priv->pdev->dev;
	const struct wlfw_fw_init_done_ind_msg_v01 *ind_msg = data;
	uint64_t msa_base_addr = priv->msa_pa;
	phys_addr_t hang_data_phy_addr;

	icnss_pr_dbg("Received QMI WLFW FW initialization done indication\n");

	if (!txn) {
		icnss_pr_err("Spurious indication\n");
		return;
	}

	/* Check if the length is valid &
	 * the length should not be 0 and
	 * should be <=  WLFW_MAX_HANG_EVENT_DATA_SIZE(400)
	 */

	if (ind_msg->hang_data_length_valid &&
	ind_msg->hang_data_length &&
	ind_msg->hang_data_length <= WLFW_MAX_HANG_EVENT_DATA_SIZE)
		priv->hang_event_data_len = ind_msg->hang_data_length;
	else
		goto out;

	/* Check if the offset is valid &
	 * the offset should be in range of 0 to msa_mem_size-hang_data_length
	 */

	if (ind_msg->hang_data_addr_offset_valid &&
	    (ind_msg->hang_data_addr_offset <= (priv->msa_mem_size -
					ind_msg->hang_data_length)))
		hang_data_phy_addr = msa_base_addr +
					ind_msg->hang_data_addr_offset;
	else
		goto out;

	if (priv->hang_event_data_pa == hang_data_phy_addr)
		goto exit;

	priv->hang_event_data_pa = hang_data_phy_addr;
	priv->hang_event_data_va = devm_ioremap(dev, priv->hang_event_data_pa,
					ind_msg->hang_data_length);

	if (!priv->hang_event_data_va) {
		icnss_pr_err("Hang Data ioremap failed: phy addr: %pa\n",
		&priv->hang_event_data_pa);
		goto fail;
	}

exit:
	icnss_pr_dbg("Hang Event Data details,Offset:0x%x, Length:0x%x,va_addr: 0x%pK\n",
		     ind_msg->hang_data_addr_offset,
		     ind_msg->hang_data_length,
		     priv->hang_event_data_va);

	goto post;

out:
	icnss_pr_err("Invalid Hang Data details, Offset:0x%x, Length:0x%x",
		     ind_msg->hang_data_addr_offset,
		     ind_msg->hang_data_length);
fail:
	priv->hang_event_data_va = NULL;
	priv->hang_event_data_pa = 0;
	priv->hang_event_data_len = 0;
post:
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_FW_INIT_DONE_IND,
				0, NULL);

}

static void wlfw_qdss_trace_req_mem_ind_cb(struct qmi_handle *qmi,
					   struct sockaddr_qrtr *sq,
					   struct qmi_txn *txn,
					   const void *data)
{
	struct icnss_priv *priv =
		container_of(qmi, struct icnss_priv, qmi);
	const struct wlfw_qdss_trace_req_mem_ind_msg_v01 *ind_msg = data;
	int i;

	icnss_pr_dbg("Received QMI WLFW QDSS trace request mem indication\n");

	if (!txn) {
		icnss_pr_err("Spurious indication\n");
		return;
	}

	if (priv->qdss_mem_seg_len) {
		icnss_pr_err("Ignore double allocation for QDSS trace, current len %u\n",
			     priv->qdss_mem_seg_len);
		return;
	}

	priv->qdss_mem_seg_len = ind_msg->mem_seg_len;
	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
		icnss_pr_dbg("QDSS requests for memory, size: 0x%x, type: %u\n",
			     ind_msg->mem_seg[i].size,
			     ind_msg->mem_seg[i].type);
		priv->qdss_mem[i].type = ind_msg->mem_seg[i].type;
		priv->qdss_mem[i].size = ind_msg->mem_seg[i].size;
	}

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM,
				0, NULL);
}

static void wlfw_qdss_trace_save_ind_cb(struct qmi_handle *qmi,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn,
					const void *data)
{
	struct icnss_priv *priv =
		container_of(qmi, struct icnss_priv, qmi);
	const struct wlfw_qdss_trace_save_ind_msg_v01 *ind_msg = data;
	struct icnss_qmi_event_qdss_trace_save_data *event_data;
	int i = 0;

	icnss_pr_dbg("Received QMI WLFW QDSS trace save indication\n");

	if (!txn) {
		icnss_pr_err("Spurious indication\n");
		return;
	}

	icnss_pr_dbg("QDSS_trace_save info: source %u, total_size %u, file_name_valid %u, file_name %s\n",
		     ind_msg->source, ind_msg->total_size,
		     ind_msg->file_name_valid, ind_msg->file_name);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	if (ind_msg->mem_seg_valid) {
		if (ind_msg->mem_seg_len > QDSS_TRACE_SEG_LEN_MAX) {
			icnss_pr_err("Invalid seg len %u\n",
				     ind_msg->mem_seg_len);
			goto free_event_data;
		}
		icnss_pr_dbg("QDSS_trace_save seg len %u\n",
			     ind_msg->mem_seg_len);
		event_data->mem_seg_len = ind_msg->mem_seg_len;
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			event_data->mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			event_data->mem_seg[i].size = ind_msg->mem_seg[i].size;
			icnss_pr_dbg("seg-%d: addr 0x%llx size 0x%x\n",
				     i, ind_msg->mem_seg[i].addr,
				     ind_msg->mem_seg[i].size);
		}
	}

	event_data->total_size = ind_msg->total_size;

	if (ind_msg->file_name_valid)
		strlcpy(event_data->file_name, ind_msg->file_name,
			QDSS_TRACE_FILE_NAME_MAX + 1);

	if (ind_msg->source == 1) {
		if (!ind_msg->file_name_valid)
			strlcpy(event_data->file_name, "qdss_trace_wcss_etb",
			QDSS_TRACE_FILE_NAME_MAX + 1);
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA,
				0, event_data);
	} else {
		if (!ind_msg->file_name_valid)
			strlcpy(event_data->file_name, "qdss_trace_ddr",
			QDSS_TRACE_FILE_NAME_MAX + 1);
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_QDSS_TRACE_SAVE,
				0, event_data);
	}

	return;

free_event_data:
	kfree(event_data);
}

static void wlfw_qdss_trace_free_ind_cb(struct qmi_handle *qmi,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn,
					const void *data)
{
	struct icnss_priv *priv =
		container_of(qmi, struct icnss_priv, qmi);

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_QDSS_TRACE_FREE,
				0, NULL);
}

static void icnss_wlfw_respond_get_info_ind_cb(struct qmi_handle *qmi,
					      struct sockaddr_qrtr *sq,
					      struct qmi_txn *txn,
					      const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);
	const struct wlfw_respond_get_info_ind_msg_v01 *ind_msg = data;

	icnss_pr_vdbg("Received QMI WLFW respond get info indication\n");

	if (!txn) {
		icnss_pr_err("Spurious indication\n");
		return;
	}

	icnss_pr_vdbg("Extract message with event length: %d, type: %d, is last: %d, seq no: %d\n",
		     ind_msg->data_len, ind_msg->type,
		     ind_msg->is_last, ind_msg->seq_no);

	if (priv->get_info_cb_ctx && priv->get_info_cb)
		priv->get_info_cb(priv->get_info_cb_ctx,
				       (void *)ind_msg->data,
				       ind_msg->data_len);
}

static void icnss_wlfw_m3_dump_upload_segs_req_ind_cb(struct qmi_handle *qmi,
						      struct sockaddr_qrtr *sq,
						      struct qmi_txn *txn,
						      const void *d)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);
	const struct wlfw_m3_dump_upload_segments_req_ind_msg_v01 *ind_msg = d;
	struct icnss_m3_upload_segments_req_data *event_data = NULL;
	u64 max_mapped_addr = 0;
	u64 segment_addr = 0;
	int i = 0;

	icnss_pr_dbg("Received QMI WLFW M3 dump upload sigments indication\n");

	if (!txn) {
		icnss_pr_err("Spurious indication\n");
		return;
	}

	icnss_pr_dbg("M3 Dump upload info: pdev_id: %d no_of_segments: %d\n",
		     ind_msg->pdev_id, ind_msg->no_of_valid_segments);

	if (ind_msg->no_of_valid_segments > QMI_WLFW_MAX_M3_SEGMENTS_SIZE_V01)
		return;

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	event_data->pdev_id = ind_msg->pdev_id;
	event_data->no_of_valid_segments = ind_msg->no_of_valid_segments;
	max_mapped_addr = priv->msa_pa + priv->msa_mem_size;

	for (i = 0; i < ind_msg->no_of_valid_segments; i++) {
		segment_addr = ind_msg->m3_segment[i].addr &
				M3_SEGMENT_ADDR_MASK;

		if (ind_msg->m3_segment[i].size > priv->msa_mem_size ||
		    segment_addr >= max_mapped_addr ||
		    segment_addr < priv->msa_pa ||
		    ind_msg->m3_segment[i].size +
		    segment_addr > max_mapped_addr) {
			icnss_pr_dbg("Received out of range Segment %d Addr: 0x%llx Size: 0x%x, Name: %s, type: %d\n",
				     (i + 1), segment_addr,
				     ind_msg->m3_segment[i].size,
				     ind_msg->m3_segment[i].name,
				     ind_msg->m3_segment[i].type);
			goto out;
		}

		event_data->m3_segment[i].addr = segment_addr;
		event_data->m3_segment[i].size = ind_msg->m3_segment[i].size;
		event_data->m3_segment[i].type = ind_msg->m3_segment[i].type;
		strlcpy(event_data->m3_segment[i].name,
			ind_msg->m3_segment[i].name,
			WLFW_MAX_STR_LEN + 1);

		icnss_pr_dbg("Received Segment %d Addr: 0x%llx Size: 0x%x, Name: %s, type: %d\n",
			     (i + 1), segment_addr,
			     ind_msg->m3_segment[i].size,
			     ind_msg->m3_segment[i].name,
			     ind_msg->m3_segment[i].type);
	}

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ,
				0, event_data);

	return;
out:
	kfree(event_data);
}

static struct qmi_msg_handler wlfw_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_READY_IND_V01,
		.ei = wlfw_fw_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_ready_ind_msg_v01),
		.fn = fw_ready_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_MSA_READY_IND_V01,
		.ei = wlfw_msa_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_msa_ready_ind_msg_v01),
		.fn = msa_ready_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01,
		.ei = wlfw_pin_connect_result_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_pin_connect_result_ind_msg_v01),
		.fn = pin_connect_result_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_REJUVENATE_IND_V01,
		.ei = wlfw_rejuvenate_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_rejuvenate_ind_msg_v01),
		.fn = rejuvenate_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_CAL_DONE_IND_V01,
		.ei = wlfw_cal_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_cal_done_ind_msg_v01),
		.fn = cal_done_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_INIT_DONE_IND_V01,
		.ei = wlfw_fw_init_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_init_done_ind_msg_v01),
		.fn = fw_init_done_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01,
		.ei = wlfw_qdss_trace_req_mem_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_qdss_trace_req_mem_ind_msg_v01),
		.fn = wlfw_qdss_trace_req_mem_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_SAVE_IND_V01,
		.ei = wlfw_qdss_trace_save_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_qdss_trace_save_ind_msg_v01),
		.fn = wlfw_qdss_trace_save_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_FREE_IND_V01,
		.ei = wlfw_qdss_trace_free_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_qdss_trace_free_ind_msg_v01),
		.fn = wlfw_qdss_trace_free_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_RESPOND_GET_INFO_IND_V01,
		.ei = wlfw_respond_get_info_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_respond_get_info_ind_msg_v01),
		.fn = icnss_wlfw_respond_get_info_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_M3_DUMP_UPLOAD_SEGMENTS_REQ_IND_V01,
		.ei = wlfw_m3_dump_upload_segments_req_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct wlfw_m3_dump_upload_segments_req_ind_msg_v01),
		.fn = icnss_wlfw_m3_dump_upload_segs_req_ind_cb
	},
	{}
};

int icnss_connect_to_fw_server(struct icnss_priv *priv, void *data)
{
	struct icnss_event_server_arrive_data *event_data = data;
	struct qmi_handle *qmi = &priv->qmi;
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	if (!priv) {
		ret = -ENODEV;
		goto out;
	}

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = event_data->node;
	sq.sq_port = event_data->port;
	ret = kernel_connect(qmi->sock, (struct sockaddr *)&sq, sizeof(sq), 0);
	if (ret < 0) {
		icnss_pr_err("Fail to connect to remote service port\n");
		goto out;
	}

	icnss_pr_info("QMI Server Connected: state: 0x%lx\n", priv->state);

	kfree(data);
	return 0;

out:
	kfree(data);
	ICNSS_ASSERT(0);
	return ret;
}

int icnss_clear_server(struct icnss_priv *priv)
{
	int ret;

	if (!priv)
		return -ENODEV;

	icnss_pr_info("QMI Service Disconnected: 0x%lx\n", priv->state);
	clear_bit(ICNSS_WLFW_CONNECTED, &priv->state);

	icnss_unregister_fw_service(priv);

	clear_bit(ICNSS_DEL_SERVER, &priv->state);

	ret =  icnss_register_fw_service(priv);
	if (ret < 0) {
		icnss_pr_err("WLFW server registration failed\n");
		ICNSS_ASSERT(0);
	}

	return 0;
}

static int wlfw_new_server(struct qmi_handle *qmi,
			   struct qmi_service *service)
{
	struct icnss_priv *priv =
		container_of(qmi, struct icnss_priv, qmi);
	struct icnss_event_server_arrive_data *event_data;

	if (priv && test_bit(ICNSS_DEL_SERVER, &priv->state)) {
		icnss_pr_info("WLFW server delete in progress, Ignore server arrive: 0x%lx\n",
			      priv->state);
		return 0;
	}

	icnss_pr_dbg("WLFW server arrive: node %u port %u\n",
		     service->node, service->port);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (event_data == NULL)
		return -ENOMEM;

	event_data->node = service->node;
	event_data->port = service->port;

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
				0, event_data);

	return 0;
}

static void wlfw_del_server(struct qmi_handle *qmi,
			    struct qmi_service *service)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);

	if (priv && test_bit(ICNSS_DEL_SERVER, &priv->state)) {
		icnss_pr_info("WLFW server delete in progress, Ignore server delete:  0x%lx\n",
			      priv->state);
		return;
	}

	icnss_pr_dbg("WLFW server delete\n");

	if (priv) {
		set_bit(ICNSS_DEL_SERVER, &priv->state);
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);
	}

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_SERVER_EXIT,
				0, NULL);
}

static struct qmi_ops wlfw_qmi_ops = {
	.new_server = wlfw_new_server,
	.del_server = wlfw_del_server,
};

int icnss_register_fw_service(struct icnss_priv *priv)
{
	int ret;

	ret = qmi_handle_init(&priv->qmi,
			      WLFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			      &wlfw_qmi_ops, wlfw_msg_handlers);
	if (ret < 0)
		return ret;

	if (priv->device_id == WCN6750_DEVICE_ID)
		ret = qmi_add_lookup(&priv->qmi, WLFW_SERVICE_ID_V01,
				     WLFW_SERVICE_VERS_V01,
				     WLFW_SERVICE_WCN_INS_ID_V01);
	else
		ret = qmi_add_lookup(&priv->qmi, WLFW_SERVICE_ID_V01,
				     WLFW_SERVICE_VERS_V01,
				     WLFW_SERVICE_INS_ID_V01);
	return ret;
}

void icnss_unregister_fw_service(struct icnss_priv *priv)
{
	qmi_handle_release(&priv->qmi);
}

int icnss_send_wlan_enable_to_fw(struct icnss_priv *priv,
			struct icnss_wlan_enable_cfg *config,
			enum icnss_driver_mode mode,
			const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret;

	icnss_pr_dbg("Mode: %d, config: %pK, host_version: %s\n",
		     mode, config, host_version);

	memset(&req, 0, sizeof(req));

	if (mode == ICNSS_WALTEST || mode == ICNSS_CCPM)
		goto skip;

	if (!config || !host_version) {
		icnss_pr_err("Invalid cfg pointer, config: %pK, host_version: %pK\n",
			     config, host_version);
		ret = -EINVAL;
		goto out;
	}

	req.host_version_valid = 1;
	strlcpy(req.host_version, host_version,
		WLFW_MAX_STR_LEN + 1);

	req.tgt_cfg_valid = 1;
	if (config->num_ce_tgt_cfg > WLFW_MAX_NUM_CE)
		req.tgt_cfg_len = WLFW_MAX_NUM_CE;
	else
		req.tgt_cfg_len = config->num_ce_tgt_cfg;
	for (i = 0; i < req.tgt_cfg_len; i++) {
		req.tgt_cfg[i].pipe_num = config->ce_tgt_cfg[i].pipe_num;
		req.tgt_cfg[i].pipe_dir = config->ce_tgt_cfg[i].pipe_dir;
		req.tgt_cfg[i].nentries = config->ce_tgt_cfg[i].nentries;
		req.tgt_cfg[i].nbytes_max = config->ce_tgt_cfg[i].nbytes_max;
		req.tgt_cfg[i].flags = config->ce_tgt_cfg[i].flags;
	}

	req.svc_cfg_valid = 1;
	if (config->num_ce_svc_pipe_cfg > WLFW_MAX_NUM_SVC)
		req.svc_cfg_len = WLFW_MAX_NUM_SVC;
	else
		req.svc_cfg_len = config->num_ce_svc_pipe_cfg;
	for (i = 0; i < req.svc_cfg_len; i++) {
		req.svc_cfg[i].service_id = config->ce_svc_cfg[i].service_id;
		req.svc_cfg[i].pipe_dir = config->ce_svc_cfg[i].pipe_dir;
		req.svc_cfg[i].pipe_num = config->ce_svc_cfg[i].pipe_num;
	}

	if (priv->device_id == WCN6750_DEVICE_ID) {
		req.shadow_reg_v2_valid = 1;
		if (config->num_shadow_reg_v2_cfg >
			QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01)
			req.shadow_reg_v2_len =
				QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01;
		else
			req.shadow_reg_v2_len = config->num_shadow_reg_v2_cfg;

		memcpy(req.shadow_reg_v2, config->shadow_reg_v2_cfg,
			 sizeof(struct wlfw_shadow_reg_v2_cfg_s_v01) *
			 req.shadow_reg_v2_len);
	} else if (priv->device_id == ADRASTEA_DEVICE_ID) {
		req.shadow_reg_valid = 1;
		if (config->num_shadow_reg_cfg >
			QMI_WLFW_MAX_NUM_SHADOW_REG_V01)
			req.shadow_reg_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V01;
		else
			req.shadow_reg_len = config->num_shadow_reg_cfg;

		memcpy(req.shadow_reg, config->shadow_reg_cfg,
		       sizeof(struct wlfw_msi_cfg_s_v01) * req.shadow_reg_len);
	}

	ret = wlfw_wlan_cfg_send_sync_msg(priv, &req);
	if (ret)
		goto out;
skip:
	ret = wlfw_wlan_mode_send_sync_msg(priv,
			   (enum wlfw_driver_mode_enum_v01)mode);
out:
	if (test_bit(SKIP_QMI, &priv->ctrl_params.quirks))
		ret = 0;

	return ret;
}

int icnss_send_wlan_disable_to_fw(struct icnss_priv *priv)
{
	enum wlfw_driver_mode_enum_v01 mode = QMI_WLFW_OFF_V01;

	return wlfw_wlan_mode_send_sync_msg(priv, mode);
}

int icnss_send_vbatt_update(struct icnss_priv *priv, uint64_t voltage_uv)
{
	int ret;
	struct wlfw_vbatt_req_msg_v01 *req;
	struct wlfw_vbatt_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!priv)
		return -ENODEV;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return -EINVAL;

	icnss_pr_dbg("Sending Vbatt message, state: 0x%lx\n", priv->state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	priv->stats.vbatt_req++;

	req->voltage_uv = voltage_uv;

	ret = qmi_txn_init(&priv->qmi, &txn, wlfw_vbatt_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Fail to init txn for Vbatt message resp %d\n",
			     ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_VBATT_REQ_V01,
			       WLFW_VBATT_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_vbatt_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Fail to send Vbatt message req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("VBATT message resp wait failed with ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI Vbatt message request rejected, result:%d error:%d\n",
				    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.vbatt_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.vbatt_req_err++;
	return ret;
}

#ifdef CONFIG_ICNSS2_DEBUG
static inline u32 icnss_get_host_build_type(void)
{
	return QMI_HOST_BUILD_TYPE_PRIMARY_V01;
}
#else
static inline u32 icnss_get_host_build_type(void)
{
	return QMI_HOST_BUILD_TYPE_SECONDARY_V01;
}
#endif

int wlfw_host_cap_send_sync(struct icnss_priv *priv)
{
	struct wlfw_host_cap_req_msg_v01 *req;
	struct wlfw_host_cap_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	u64 iova_start = 0, iova_size = 0,
	    iova_ipa_start = 0, iova_ipa_size = 0;

	icnss_pr_dbg("Sending host capability message, state: 0x%lx\n",
		    priv->state);

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
		     &priv->ctrl_params.quirks))
		req->num_clients = 2;
	else
		req->num_clients = 1;
	icnss_pr_dbg("Number of clients is %d\n", req->num_clients);

	req->bdf_support_valid = 1;
	req->bdf_support = 1;

	req->cal_done_valid = 1;
	req->cal_done = priv->cal_done;
	icnss_pr_dbg("Calibration done is %d\n", priv->cal_done);

	if (priv->smmu_s1_enable &&
	    !icnss_get_iova(priv, &iova_start, &iova_size) &&
	    !icnss_get_iova_ipa(priv, &iova_ipa_start,
				&iova_ipa_size)) {
		req->ddr_range_valid = 1;
		req->ddr_range[0].start = iova_start;
		req->ddr_range[0].size = iova_size + iova_ipa_size;
		req->ddr_range[1].start = priv->msa_pa;
		req->ddr_range[1].size = priv->msa_mem_size;
		icnss_pr_dbg("Sending iova starting 0x%llx with size 0x%llx\n",
			    req->ddr_range[0].start, req->ddr_range[0].size);
		icnss_pr_dbg("Sending msa starting 0x%llx with size 0x%llx\n",
			    req->ddr_range[1].start, req->ddr_range[1].size);
	}

	req->host_build_type_valid = 1;
	req->host_build_type = icnss_get_host_build_type();

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_host_cap_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Failed to initialize txn for host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&priv->qmi, NULL, &txn,
			       QMI_WLFW_HOST_CAP_REQ_V01,
			       WLFW_HOST_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_host_cap_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Failed to send host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Failed to wait for response of host capability request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("Host capability request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	ICNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int icnss_wlfw_get_info_send_sync(struct icnss_priv *plat_priv, int type,
				 void *cmd, int cmd_len)
{
	struct wlfw_get_info_req_msg_v01 *req;
	struct wlfw_get_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	icnss_pr_dbg("Sending get info message, type: %d, cmd length: %d, state: 0x%lx\n",
		     type, cmd_len, plat_priv->state);

	if (cmd_len > QMI_WLFW_MAX_DATA_SIZE_V01)
		return -EINVAL;

	if (test_bit(ICNSS_FW_DOWN, &plat_priv->state))
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

	ret = qmi_txn_init(&plat_priv->qmi, &txn,
			   wlfw_get_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_pr_err("Failed to initialize txn for get info request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi, NULL, &txn,
			       QMI_WLFW_GET_INFO_REQ_V01,
			       WLFW_GET_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_get_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		icnss_pr_err("Failed to send get info request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, plat_priv->ctrl_params.qmi_timeout);
	if (ret < 0) {
		icnss_pr_err("Failed to wait for response of get info request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("Get info request failed, result: %d, err: %d\n",
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
