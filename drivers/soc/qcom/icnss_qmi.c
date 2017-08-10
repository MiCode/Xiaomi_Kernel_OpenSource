/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/qmi_encdec.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ipc_logging.h>
#include <linux/thread_info.h>
#include <linux/qpnp/qpnp-adc.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include "wlan_firmware_service_v01.h"
#include "icnss_private.h"

#ifdef CONFIG_ICNSS_DEBUG
unsigned long qmi_timeout = 10000;
module_param(qmi_timeout, ulong, 0600);
#define WLFW_TIMEOUT_MS			qmi_timeout
#else
#define WLFW_TIMEOUT_MS			10000
#endif

#define WLFW_SERVICE_INS_ID_V01		0
#define WLFW_CLIENT_ID			0x4b4e454c
#define QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED	0x77

#ifdef CONFIG_ICNSS_DEBUG
bool ignore_fw_timeout;
#define ICNSS_QMI_ASSERT() ICNSS_ASSERT(ignore_fw_timeout)
#else
#define ICNSS_QMI_ASSERT() do { } while (0)
#endif

#ifdef CONFIG_ICNSS_DEBUG
void icnss_ignore_fw_timeout(bool ignore)
{
	ignore_fw_timeout = ignore;
}
#else
void icnss_ignore_fw_timeout(bool ignore) { }
#endif

int icnss_qmi_pin_connect_result_ind(struct icnss_priv *priv,
					void *msg, unsigned int msg_len)
{
	struct msg_desc ind_desc;
	struct wlfw_pin_connect_result_ind_msg_v01 ind_msg;
	int ret = 0;

	if (!priv || !priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}
	memset(&ind_msg, 0, sizeof(ind_msg));
	ind_desc.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01;
	ind_desc.max_msg_len = WLFW_PIN_CONNECT_RESULT_IND_MSG_V01_MAX_MSG_LEN;
	ind_desc.ei_array = wlfw_pin_connect_result_ind_msg_v01_ei;

	ret = qmi_kernel_decode(&ind_desc, &ind_msg, msg, msg_len);
	if (ret < 0) {
		icnss_pr_err("Failed to decode message: %d, msg_len: %u\n",
			     ret, msg_len);
		goto out;
	}
	/* store pin result locally */
	if (ind_msg.pwr_pin_result_valid)
		priv->pwr_pin_result = ind_msg.pwr_pin_result;
	if (ind_msg.phy_io_pin_result_valid)
		priv->phy_io_pin_result = ind_msg.phy_io_pin_result;
	if (ind_msg.rf_pin_result_valid)
		priv->rf_pin_result = ind_msg.rf_pin_result;

	icnss_pr_dbg("Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		     ind_msg.pwr_pin_result, ind_msg.phy_io_pin_result,
		     ind_msg.rf_pin_result);
	priv->stats.pin_connect_result++;
out:
	return ret;
}

int wlfw_msa_mem_info_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	int i;
	struct wlfw_msa_info_req_msg_v01 req;
	struct wlfw_msa_info_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Sending MSA mem info, state: 0x%lx\n", priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.msa_addr = priv->msa_pa;
	req.size = priv->msa_mem_size;

	req_desc.max_msg_len = WLFW_MSA_INFO_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_MSA_INFO_REQ_V01;
	req_desc.ei_array = wlfw_msa_info_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_MSA_INFO_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_MSA_INFO_RESP_V01;
	resp_desc.ei_array = wlfw_msa_info_resp_msg_v01_ei;

	priv->stats.msa_info_req++;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send MSA Mem info req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI MSA Mem info request rejected, result:%d error:%d\n",
			resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}

	icnss_pr_dbg("Receive mem_region_info_len: %d\n",
		     resp.mem_region_info_len);

	if (resp.mem_region_info_len > QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01) {
		icnss_pr_err("Invalid memory region length received: %d\n",
			     resp.mem_region_info_len);
		ret = -EINVAL;
		goto out;
	}

	priv->stats.msa_info_resp++;
	priv->nr_mem_region = resp.mem_region_info_len;
	for (i = 0; i < resp.mem_region_info_len; i++) {
		priv->mem_region[i].reg_addr =
			resp.mem_region_info[i].region_addr;
		priv->mem_region[i].size =
			resp.mem_region_info[i].size;
		priv->mem_region[i].secure_flag =
			resp.mem_region_info[i].secure_flag;
		icnss_pr_dbg("Memory Region: %d Addr: 0x%llx Size: 0x%x Flag: 0x%08x\n",
			     i, priv->mem_region[i].reg_addr,
			     priv->mem_region[i].size,
			     priv->mem_region[i].secure_flag);
	}

	return 0;

out:
	priv->stats.msa_info_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_msa_ready_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_msa_ready_req_msg_v01 req;
	struct wlfw_msa_ready_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}
	icnss_pr_dbg("Sending MSA ready request message, state: 0x%lx\n",
		     priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_MSA_READY_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_MSA_READY_REQ_V01;
	req_desc.ei_array = wlfw_msa_ready_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_MSA_READY_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_MSA_READY_RESP_V01;
	resp_desc.ei_array = wlfw_msa_ready_resp_msg_v01_ei;

	priv->stats.msa_ready_req++;
	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send MSA ready req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI MSA ready request rejected: result:%d error:%d\n",
			resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.msa_ready_resp++;

	return 0;

out:
	priv->stats.msa_ready_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_ind_register_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_ind_register_req_msg_v01 req;
	struct wlfw_ind_register_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		     priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.client_id_valid = 1;
	req.client_id = WLFW_CLIENT_ID;
	req.fw_ready_enable_valid = 1;
	req.fw_ready_enable = 1;
	req.msa_ready_enable_valid = 1;
	req.msa_ready_enable = 1;
	req.pin_connect_result_enable_valid = 1;
	req.pin_connect_result_enable = 1;
	if (test_bit(FW_REJUVENATE_ENABLE, &quirks)) {
		req.rejuvenate_enable_valid = 1;
		req.rejuvenate_enable = 1;
	}

	req_desc.max_msg_len = WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_IND_REGISTER_REQ_V01;
	req_desc.ei_array = wlfw_ind_register_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_IND_REGISTER_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_IND_REGISTER_RESP_V01;
	resp_desc.ei_array = wlfw_ind_register_resp_msg_v01_ei;

	priv->stats.ind_register_req++;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send indication register req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI indication register request rejected, resut:%d error:%d\n",
		       resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.ind_register_resp++;

	return 0;

out:
	priv->stats.ind_register_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_cap_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_cap_req_msg_v01 req;
	struct wlfw_cap_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending capability message, state: 0x%lx\n", priv->state);

	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_CAP_REQ_V01;
	req_desc.ei_array = wlfw_cap_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_CAP_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_CAP_RESP_V01;
	resp_desc.ei_array = wlfw_cap_resp_msg_v01_ei;

	priv->stats.cap_req++;
	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send capability req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI capability request rejected, result:%d error:%d\n",
		       resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		if (resp.resp.error == QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED)
			icnss_pr_err("RF card Not present");
		goto out;
	}

	priv->stats.cap_resp++;
	/* store cap locally */
	if (resp.chip_info_valid) {
		priv->chip_info.chip_id = resp.chip_info.chip_id;
		priv->chip_info.chip_family = resp.chip_info.chip_family;
	}
	if (resp.board_info_valid)
		priv->board_id = resp.board_info.board_id;
	else
		priv->board_id = 0xFF;
	if (resp.soc_info_valid)
		priv->soc_id = resp.soc_info.soc_id;
	if (resp.fw_version_info_valid) {
		priv->fw_version_info.fw_version =
			resp.fw_version_info.fw_version;
		strlcpy(priv->fw_version_info.fw_build_timestamp,
				resp.fw_version_info.fw_build_timestamp,
				WLFW_MAX_TIMESTAMP_LEN + 1);
	}
	if (resp.fw_build_id_valid)
		strlcpy(priv->fw_build_id, resp.fw_build_id,
			QMI_WLFW_MAX_BUILD_ID_LEN_V01 + 1);

	icnss_pr_dbg("Capability, chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s, fw_build_id: %s",
		     priv->chip_info.chip_id, priv->chip_info.chip_family,
		     priv->board_id, priv->soc_id,
		     priv->fw_version_info.fw_version,
		     priv->fw_version_info.fw_build_timestamp,
		     priv->fw_build_id);

	return 0;

out:
	priv->stats.cap_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_wlan_mode_send_sync_msg(struct icnss_priv *priv,
		enum wlfw_driver_mode_enum_v01 mode)
{
	int ret;
	struct wlfw_wlan_mode_req_msg_v01 req;
	struct wlfw_wlan_mode_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt)
		return -ENODEV;

	/* During recovery do not send mode request for WLAN OFF as
	 * FW not able to process it.
	 */
	if (test_bit(ICNSS_PD_RESTART, &priv->state) &&
	    mode == QMI_WLFW_OFF_V01)
		return 0;

	icnss_pr_dbg("Sending Mode request, state: 0x%lx, mode: %d\n",
		     priv->state, mode);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mode = mode;
	req.hw_debug_valid = 1;
	req.hw_debug = !!test_bit(HW_DEBUG_ENABLE, &quirks);

	req_desc.max_msg_len = WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_MODE_REQ_V01;
	req_desc.ei_array = wlfw_wlan_mode_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_MODE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_MODE_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_mode_resp_msg_v01_ei;

	priv->stats.mode_req++;
	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send mode req failed, mode: %d ret: %d\n",
			     mode, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI mode request rejected, mode:%d result:%d error:%d\n",
			     mode, resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.mode_resp++;

	return 0;

out:
	priv->stats.mode_req_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_wlan_cfg_send_sync_msg(struct icnss_priv *priv,
		struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	int ret;
	struct wlfw_wlan_cfg_req_msg_v01 req;
	struct wlfw_wlan_cfg_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending config request, state: 0x%lx\n", priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	memcpy(&req, data, sizeof(req));

	req_desc.max_msg_len = WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_CFG_REQ_V01;
	req_desc.ei_array = wlfw_wlan_cfg_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_CFG_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_CFG_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_cfg_resp_msg_v01_ei;

	priv->stats.cfg_req++;
	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send config req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI config request rejected, result:%d error:%d\n",
		       resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.cfg_resp++;

	return 0;

out:
	priv->stats.cfg_req_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_ini_send_sync_msg(struct icnss_priv *priv, uint8_t fw_log_mode)
{
	int ret;
	struct wlfw_ini_req_msg_v01 req;
	struct wlfw_ini_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv || !priv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log_mode: %d\n",
		     priv->state, fw_log_mode);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.enablefwlog_valid = 1;
	req.enablefwlog = fw_log_mode;

	req_desc.max_msg_len = WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_INI_REQ_V01;
	req_desc.ei_array = wlfw_ini_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_INI_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_INI_RESP_V01;
	resp_desc.ei_array = wlfw_ini_resp_msg_v01_ei;

	priv->stats.ini_req++;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send INI req failed fw_log_mode: %d, ret: %d\n",
			     fw_log_mode, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI INI request rejected, fw_log_mode:%d result:%d error:%d\n",
			     fw_log_mode, resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.ini_resp++;

	return 0;

out:
	priv->stats.ini_req_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_athdiag_read_send_sync_msg(struct icnss_priv *priv,
					   uint32_t offset, uint32_t mem_type,
					   uint32_t data_len, uint8_t *data)
{
	int ret;
	struct wlfw_athdiag_read_req_msg_v01 req;
	struct wlfw_athdiag_read_resp_msg_v01 *resp = NULL;
	struct msg_desc req_desc, resp_desc;

	if (!priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Diag read: state 0x%lx, offset %x, mem_type %x, data_len %u\n",
		     priv->state, offset, mem_type, data_len);

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto out;
	}
	memset(&req, 0, sizeof(req));

	req.offset = offset;
	req.mem_type = mem_type;
	req.data_len = data_len;

	req_desc.max_msg_len = WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_ATHDIAG_READ_REQ_V01;
	req_desc.ei_array = wlfw_athdiag_read_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_ATHDIAG_READ_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_ATHDIAG_READ_RESP_V01;
	resp_desc.ei_array = wlfw_athdiag_read_resp_msg_v01_ei;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, resp, sizeof(*resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("send athdiag read req failed %d\n", ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI athdiag read request rejected, result:%d error:%d\n",
			     resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
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
	return ret;
}

int wlfw_athdiag_write_send_sync_msg(struct icnss_priv *priv,
					    uint32_t offset, uint32_t mem_type,
					    uint32_t data_len, uint8_t *data)
{
	int ret;
	struct wlfw_athdiag_write_req_msg_v01 *req = NULL;
	struct wlfw_athdiag_write_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Diag write: state 0x%lx, offset %x, mem_type %x, data_len %u, data %pK\n",
		     priv->state, offset, mem_type, data_len, data);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto out;
	}
	memset(&resp, 0, sizeof(resp));

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;
	memcpy(req->data, data, data_len);

	req_desc.max_msg_len = WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_ATHDIAG_WRITE_REQ_V01;
	req_desc.ei_array = wlfw_athdiag_write_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_ATHDIAG_WRITE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_ATHDIAG_WRITE_RESP_V01;
	resp_desc.ei_array = wlfw_athdiag_write_resp_msg_v01_ei;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, req, sizeof(*req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("send athdiag write req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI athdiag write request rejected, result:%d error:%d\n",
			     resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
out:
	kfree(req);
	return ret;
}

int icnss_decode_rejuvenate_ind(struct icnss_priv *priv,
		void *msg, unsigned int msg_len)
{
	struct msg_desc ind_desc;
	struct wlfw_rejuvenate_ind_msg_v01 ind_msg;
	int ret = 0;

	if (!priv || !priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&ind_msg, 0, sizeof(ind_msg));

	ind_desc.msg_id = QMI_WLFW_REJUVENATE_IND_V01;
	ind_desc.max_msg_len = WLFW_REJUVENATE_IND_MSG_V01_MAX_MSG_LEN;
	ind_desc.ei_array = wlfw_rejuvenate_ind_msg_v01_ei;

	ret = qmi_kernel_decode(&ind_desc, &ind_msg, msg, msg_len);
	if (ret < 0) {
		icnss_pr_err("Failed to decode rejuvenate ind message: ret %d, msg_len %u\n",
			     ret, msg_len);
		goto out;
	}

	if (ind_msg.cause_for_rejuvenation_valid)
		priv->cause_for_rejuvenation = ind_msg.cause_for_rejuvenation;
	else
		priv->cause_for_rejuvenation = 0;
	if (ind_msg.requesting_sub_system_valid)
		priv->requesting_sub_system = ind_msg.requesting_sub_system;
	else
		priv->requesting_sub_system = 0;
	if (ind_msg.line_number_valid)
		priv->line_number = ind_msg.line_number;
	else
		priv->line_number = 0;
	if (ind_msg.function_name_valid)
		memcpy(priv->function_name, ind_msg.function_name,
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
out:
	return ret;
}

int wlfw_rejuvenate_ack_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_rejuvenate_ack_req_msg_v01 req;
	struct wlfw_rejuvenate_ack_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	icnss_pr_dbg("Sending rejuvenate ack request, state: 0x%lx\n",
		     priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_REJUVENATE_ACK_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_REJUVENATE_ACK_REQ_V01;
	req_desc.ei_array = wlfw_rejuvenate_ack_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_REJUVENATE_ACK_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_REJUVENATE_ACK_RESP_V01;
	resp_desc.ei_array = wlfw_rejuvenate_ack_resp_msg_v01_ei;

	priv->stats.rejuvenate_ack_req++;
	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send rejuvenate ack req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI rejuvenate ack request rejected, result:%d error %d\n",
			     resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.rejuvenate_ack_resp++;
	return 0;

out:
	priv->stats.rejuvenate_ack_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

int wlfw_dynamic_feature_mask_send_sync_msg(struct icnss_priv *priv,
					   uint64_t dynamic_feature_mask)
{
	int ret;
	struct wlfw_dynamic_feature_mask_req_msg_v01 req;
	struct wlfw_dynamic_feature_mask_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!test_bit(ICNSS_WLFW_QMI_CONNECTED, &priv->state)) {
		icnss_pr_err("Invalid state for dynamic feature: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	if (!test_bit(FW_REJUVENATE_ENABLE, &quirks)) {
		icnss_pr_dbg("FW rejuvenate is disabled from quirks\n");
		return 0;
	}

	icnss_pr_dbg("Sending dynamic feature mask request, val 0x%llx, state: 0x%lx\n",
		     dynamic_feature_mask, priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mask_valid = 1;
	req.mask = dynamic_feature_mask;

	req_desc.max_msg_len =
		WLFW_DYNAMIC_FEATURE_MASK_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01;
	req_desc.ei_array = wlfw_dynamic_feature_mask_req_msg_v01_ei;

	resp_desc.max_msg_len =
		WLFW_DYNAMIC_FEATURE_MASK_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_DYNAMIC_FEATURE_MASK_RESP_V01;
	resp_desc.ei_array = wlfw_dynamic_feature_mask_resp_msg_v01_ei;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send dynamic feature mask req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI dynamic feature mask request rejected, result:%d error %d\n",
			     resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}

	icnss_pr_dbg("prev_mask_valid %u, prev_mask 0x%llx, curr_maks_valid %u, curr_mask 0x%llx\n",
		     resp.prev_mask_valid, resp.prev_mask,
		     resp.curr_mask_valid, resp.curr_mask);

	return 0;

out:
	return ret;
}

void icnss_qmi_wlfw_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	struct icnss_priv *priv = notify_priv;

	icnss_pr_vdbg("QMI client notify: %d\n", event);

	if (!priv || !priv->wlfw_clnt)
		return;

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&priv->fw_recv_msg_work);
		break;
	default:
		icnss_pr_dbg("Unknown Event:  %d\n", event);
		break;
	}
}

void icnss_handle_rejuvenate(struct icnss_priv *priv)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct icnss_uevent_fw_down_data fw_down_data;

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (event_data == NULL)
		return;

	event_data->crashed = true;
	event_data->fw_rejuvenate = true;
	fw_down_data.crashed = true;

	icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
				 &fw_down_data);
	icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				0, event_data);
}

void icnss_qmi_wlfw_clnt_ind(struct qmi_handle *handle,
			  unsigned int msg_id, void *msg,
			  unsigned int msg_len, void *ind_cb_priv)
{
	struct icnss_priv *priv = ind_cb_priv;

	if (!priv || !priv->wlfw_clnt)
		return;

	icnss_pr_dbg("Received Ind 0x%x, msg_len: %d\n", msg_id, msg_len);

	switch (msg_id) {
	case QMI_WLFW_FW_READY_IND_V01:
		icnss_driver_event_post(ICNSS_DRIVER_EVENT_FW_READY_IND,
					0, NULL);
		break;
	case QMI_WLFW_MSA_READY_IND_V01:
		icnss_pr_dbg("Received MSA Ready Indication msg_id 0x%x\n",
			     msg_id);
		priv->stats.msa_ready_ind++;
		break;
	case QMI_WLFW_PIN_CONNECT_RESULT_IND_V01:
		icnss_pr_dbg("Received Pin Connect Test Result msg_id 0x%x\n",
			     msg_id);
		icnss_qmi_pin_connect_result_ind(priv, msg, msg_len);
		break;
	case QMI_WLFW_REJUVENATE_IND_V01:
		icnss_pr_dbg("Received Rejuvenate Indication msg_id 0x%x, state: 0x%lx\n",
			     msg_id, priv->state);

		icnss_ignore_fw_timeout(true);
		icnss_decode_rejuvenate_ind(priv, msg, msg_len);
		icnss_handle_rejuvenate(priv);
		break;
	default:
		icnss_pr_err("Invalid msg_id 0x%x\n", msg_id);
		break;
	}
}

int icnss_connect_to_fw_server(struct icnss_priv *priv)
{
	int ret = 0;

	if (!priv) {
		ret = -ENODEV;
		goto out;
	}
	set_bit(ICNSS_WLFW_EXISTS, &priv->state);

	priv->wlfw_clnt = qmi_handle_create(icnss_qmi_wlfw_clnt_notify, priv);
	if (!priv->wlfw_clnt) {
		icnss_pr_err("QMI client handle create failed\n");
		ret = -ENOMEM;
		goto out;
	}
	ret = qmi_connect_to_service(priv->wlfw_clnt, WLFW_SERVICE_ID_V01,
				     WLFW_SERVICE_VERS_V01,
				     WLFW_SERVICE_INS_ID_V01);
	if (ret < 0) {
		icnss_pr_err("QMI WLAN Service not found : %d\n", ret);
		goto fail;
	}

	ret = qmi_register_ind_cb(priv->wlfw_clnt,
				  icnss_qmi_wlfw_clnt_ind, priv);
	if (ret < 0) {
		icnss_pr_err("Failed to register indication callback: %d\n",
			     ret);
		goto fail;
	}
	icnss_pr_info("QMI Server Connected: state: 0x%lx\n", priv->state);

	return ret;

fail:
	qmi_handle_destroy(priv->wlfw_clnt);
	priv->wlfw_clnt = NULL;
out:
	ICNSS_ASSERT(0);
	return ret;
}

int icnss_clear_server(struct icnss_priv *priv)
{
	if (!priv || !priv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_info("QMI Service Disconnected: 0x%lx\n", priv->state);

	qmi_handle_destroy(priv->wlfw_clnt);

	clear_bit(ICNSS_WLFW_QMI_CONNECTED, &priv->state);
	priv->wlfw_clnt = NULL;

	return 0;
}

int icnss_qmi_wlfw_clnt_svc_event_notify(struct notifier_block *this,
					       unsigned long code,
					       void *_cmd)
{
	int ret = 0;

	icnss_pr_dbg("Event Notify: code: %ld", code);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
					      0, NULL);
		break;

	case QMI_SERVER_EXIT:
		ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_EXIT,
					      0, NULL);
		break;
	default:
		icnss_pr_dbg("Invalid code: %ld", code);
		break;
	}
	return ret;
}

struct notifier_block wlfw_clnt_nb = {
	.notifier_call = icnss_qmi_wlfw_clnt_svc_event_notify,
};

static void icnss_qmi_wlfw_clnt_notify_work(struct work_struct *work)
{
	int ret = 0;

	if (!penv || !penv->wlfw_clnt)
		return;

	icnss_pr_vdbg("Receiving Event in work queue context\n");

	do {
	} while ((ret = qmi_recv_msg(priv->wlfw_clnt)) == 0);

	if (ret != -ENOMSG)
		icnss_pr_err("Error receiving message: %d\n", ret);


	icnss_pr_vdbg("Receiving Event completed\n");
}


int icnss_register_fw_service(struct icnss_priv *priv)
{
	INIT_WORK(&priv->fw_recv_msg_work, icnss_qmi_wlfw_clnt_notify_work);

	return qmi_svc_event_notifier_register(WLFW_SERVICE_ID_V01,
					      WLFW_SERVICE_VERS_V01,
					      WLFW_SERVICE_INS_ID_V01,
					      &wlfw_clnt_nb);
}

void icnss_unregister_fw_service(struct icnss_priv *priv)
{
	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &wlfw_clnt_nb);
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

	req.shadow_reg_valid = 1;
	if (config->num_shadow_reg_cfg >
	    WLFW_MAX_NUM_SHADOW_REG)
		req.shadow_reg_len = WLFW_MAX_NUM_SHADOW_REG;
	else
		req.shadow_reg_len = config->num_shadow_reg_cfg;

	memcpy(req.shadow_reg, config->shadow_reg_cfg,
	       sizeof(struct icnss_shadow_reg_cfg) * req.shadow_reg_len);

	ret = wlfw_wlan_cfg_send_sync_msg(priv, &req);
	if (ret)
		goto out;
skip:
	ret = wlfw_wlan_mode_send_sync_msg(priv, mode);
out:
	if (test_bit(SKIP_QMI, &quirks))
		ret = 0;

	return ret;
}

int icnss_send_wlan_disable_to_fw(struct icnss_priv *priv)
{
	enum wlfw_driver_mode_enum_v01 mode = QMI_WLFW_OFF_V01;

	return wlfw_wlan_mode_send_sync_msg(priv, mode);
}


