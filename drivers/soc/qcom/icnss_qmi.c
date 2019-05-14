/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "icnss_qmi: " fmt

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
#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include "wlan_firmware_service_v01.h"
#include "icnss_private.h"

#ifdef CONFIG_ICNSS_DEBUG
unsigned long qmi_timeout = 10000;
module_param(qmi_timeout, ulong, 0600);
#define WLFW_TIMEOUT			msecs_to_jiffies(qmi_timeout)
#else
#define WLFW_TIMEOUT			msecs_to_jiffies(10000)
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
		icnss_qmi_fatal_err("Fail to init txn for MSA Mem info resp %d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("MSA Mem info resp wait failed ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI MSA Mem info request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	icnss_pr_dbg("Receive mem_region_info_len: %d\n",
		     resp->mem_region_info_len);

	if (resp->mem_region_info_len > QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01) {
		icnss_qmi_fatal_err("Invalid memory region length received: %d\n",
			     resp->mem_region_info_len);
		ret = -EINVAL;
		goto out;
	}

	max_mapped_addr = priv->msa_pa + priv->msa_mem_size;
	priv->stats.msa_info_resp++;
	priv->nr_mem_region = resp->mem_region_info_len;
	for (i = 0; i < resp->mem_region_info_len; i++) {

		if (resp->mem_region_info[i].size > priv->msa_mem_size ||
		    resp->mem_region_info[i].region_addr > max_mapped_addr ||
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
		icnss_qmi_fatal_err("Fail to init txn for MSA Mem Ready resp %d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("MSA Mem Ready resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI MSA Mem Ready request rejected, result:%d error:%d\n",
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
	req->msa_ready_enable_valid = 1;
	req->msa_ready_enable = 1;
	req->pin_connect_result_enable_valid = 1;
	req->pin_connect_result_enable = 1;
	if (test_bit(FW_REJUVENATE_ENABLE, &quirks)) {
		req->rejuvenate_enable_valid = 1;
		req->rejuvenate_enable = 1;
	}

	priv->stats.ind_register_req++;

	ret = qmi_txn_init(&priv->qmi, &txn,
			   wlfw_ind_register_resp_msg_v01_ei, resp);
	if (ret < 0) {
		icnss_qmi_fatal_err("Fail to init txn for Ind Register resp %d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("Ind Register resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI Ind Register request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	priv->stats.ind_register_resp++;

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	priv->stats.ind_register_err++;
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

	icnss_pr_dbg("Sending capability message, state: 0x%lx\n", priv->state);

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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("Capability resp wait failed with ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI Capability request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		if (resp->resp.error == QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED)
			icnss_qmi_fatal_err("RF card not present\n");
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
	req->hw_debug = !!test_bit(HW_DEBUG_ENABLE, &quirks);

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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("Mode resp wait failed with ret %d\n", ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI Mode request rejected, result:%d error:%d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("Config resp wait failed with ret %d\n",
				    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI Config request rejected, result:%d error:%d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("INI resp wait failed with ret %d\n", ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI INI request rejected, result:%d error:%d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_pr_err("Athdiag Write resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI Athdiag Write request rejected, result:%d error:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
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
		icnss_qmi_fatal_err("Fail to init txn for Rejuvenate Ack resp %d\n",
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
	if (ret < 0) {
		icnss_qmi_fatal_err("Rejuvenate Ack resp wait failed with ret %d\n",
			     ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_qmi_fatal_err("QMI Rejuvenate Ack request rejected, result:%d error:%d\n",
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

	if (!test_bit(FW_REJUVENATE_ENABLE, &quirks)) {
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

	ret = qmi_txn_wait(&txn, WLFW_TIMEOUT);
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
	struct icnss_uevent_fw_down_data fw_down_data;

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (event_data == NULL)
		return;

	event_data->crashed = true;
	event_data->fw_rejuvenate = true;
	fw_down_data.crashed = true;
	set_bit(ICNSS_REJUVENATE, &priv->state);

	icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
				 &fw_down_data);
	icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				0, event_data);
}

static void fw_ready_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			    struct qmi_txn *txn, const void *data)
{
	icnss_pr_dbg("Received FW Ready Indication\n");

	if (!txn) {
		pr_err("spurious indication\n");
		return;
	}

	icnss_driver_event_post(ICNSS_DRIVER_EVENT_FW_READY_IND,
				0, NULL);
}

static void msa_ready_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			     struct qmi_txn *txn, const void *data)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);

	icnss_pr_dbg("Received MSA Ready Indication\n");

	if (!txn) {
		pr_err("spurious indication\n");
		return;
	}

	priv->stats.msa_ready_ind++;
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
	set_bit(ICNSS_WLFW_EXISTS, &priv->state);

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
	if (!priv)
		return -ENODEV;

	icnss_pr_info("QMI Service Disconnected: 0x%lx\n", priv->state);
	clear_bit(ICNSS_WLFW_CONNECTED, &priv->state);

	return 0;
}

static int wlfw_new_server(struct qmi_handle *qmi,
			   struct qmi_service *service)
{
	struct icnss_event_server_arrive_data *event_data;

	icnss_pr_dbg("WLFW server arrive: node %u port %u\n",
		     service->node, service->port);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (event_data == NULL)
		return -ENOMEM;

	event_data->node = service->node;
	event_data->port = service->port;

	icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
				0, event_data);

	return 0;
}

static void wlfw_del_server(struct qmi_handle *qmi,
			    struct qmi_service *service)
{
	struct icnss_priv *priv = container_of(qmi, struct icnss_priv, qmi);

	icnss_pr_dbg("WLFW server delete\n");

	if (priv) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);
	}

	icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_EXIT,
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

	ret = qmi_add_lookup(&priv->qmi, WLFW_SERVICE_ID_V01,
			     WLFW_SERVICE_VERS_V01, 0);
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
	ret = wlfw_wlan_mode_send_sync_msg(priv,
			   (enum wlfw_driver_mode_enum_v01)mode);
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


