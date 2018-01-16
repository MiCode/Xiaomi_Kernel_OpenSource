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
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/service-notifier.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/service-locator.h>
#include "core.h"
#include "qmi.h"
#include "snoc.h"
#include "wcn3990_qmi_service_v01.h"

static DECLARE_WAIT_QUEUE_HEAD(ath10k_fw_ready_wait_event);

static int
ath10k_snoc_service_notifier_notify(struct notifier_block *nb,
				    unsigned long notification, void *data)
{
	struct ath10k_snoc *ar_snoc = container_of(nb, struct ath10k_snoc,
					       service_notifier_nb);
	enum pd_subsys_state *state = data;
	struct ath10k *ar = ar_snoc->ar;
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;
	int ret;

	switch (notification) {
	case SERVREG_NOTIF_SERVICE_STATE_DOWN_V01:
		ath10k_dbg(ar, ATH10K_DBG_SNOC, "Service down, data: 0x%pK\n",
			   data);

		if (!state || *state != ROOT_PD_SHUTDOWN) {
			atomic_set(&ar_snoc->fw_crashed, 1);
			atomic_set(&qmi_cfg->fw_ready, 0);
		}

		ath10k_dbg(ar, ATH10K_DBG_SNOC, "PD went down %d\n",
			   atomic_read(&ar_snoc->fw_crashed));
		break;
	case SERVREG_NOTIF_SERVICE_STATE_UP_V01:
		ath10k_dbg(ar, ATH10K_DBG_SNOC, "Service up\n");
		ret = wait_event_timeout(ath10k_fw_ready_wait_event,
					 (atomic_read(&qmi_cfg->fw_ready) &&
				   atomic_read(&qmi_cfg->server_connected)),
			msecs_to_jiffies(ATH10K_SNOC_WLAN_FW_READY_TIMEOUT));
		if (ret) {
			if (ar_snoc->drv_state != ATH10K_DRIVER_STATE_PROBED)
				queue_work(ar->workqueue, &ar->restart_work);
		} else {
			ath10k_err(ar, "restart failed, fw_ready timed out\n");
			return NOTIFY_OK;
		}
		break;
	default:
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "Service state Unknown, notification: 0x%lx\n",
			    notification);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int ath10k_snoc_get_service_location_notify(struct notifier_block *nb,
						   unsigned long opcode,
						   void *data)
{
	struct ath10k_snoc *ar_snoc = container_of(nb, struct ath10k_snoc,
						   get_service_nb);
	struct ath10k *ar = ar_snoc->ar;
	struct pd_qmi_client_data *pd = data;
	int curr_state;
	int ret;
	int i;
	struct ath10k_service_notifier_context *notifier;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Get service notify opcode: %lu\n",
		   opcode);

	if (opcode != LOCATOR_UP)
		return NOTIFY_DONE;

	if (!pd->total_domains) {
		ath10k_err(ar, "Did not find any domains\n");
		ret = -ENOENT;
		goto out;
	}

	notifier = kcalloc(pd->total_domains,
			   sizeof(struct ath10k_service_notifier_context),
			   GFP_KERNEL);
	if (!notifier) {
		ret = -ENOMEM;
		goto out;
	}

	ar_snoc->service_notifier_nb.notifier_call =
					ath10k_snoc_service_notifier_notify;

	for (i = 0; i < pd->total_domains; i++) {
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "%d: domain_name: %s, instance_id: %d\n", i,
				   pd->domain_list[i].name,
				   pd->domain_list[i].instance_id);

		notifier[i].handle =
			service_notif_register_notifier(
					pd->domain_list[i].name,
					pd->domain_list[i].instance_id,
					&ar_snoc->service_notifier_nb,
					&curr_state);
		notifier[i].instance_id = pd->domain_list[i].instance_id;
		strlcpy(notifier[i].name, pd->domain_list[i].name,
			QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);

		if (IS_ERR(notifier[i].handle)) {
			ath10k_err(ar, "%d: Unable to register notifier for %s(0x%x)\n",
				   i, pd->domain_list->name,
				   pd->domain_list->instance_id);
			ret = PTR_ERR(notifier[i].handle);
			goto free_handle;
		}
	}

	ar_snoc->service_notifier = notifier;
	ar_snoc->total_domains = pd->total_domains;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "PD restart enabled\n");

	return NOTIFY_OK;

free_handle:
	for (i = 0; i < pd->total_domains; i++) {
		if (notifier[i].handle) {
			service_notif_unregister_notifier(
						notifier[i].handle,
						&ar_snoc->service_notifier_nb);
		}
	}
	kfree(notifier);

out:
	ath10k_err(ar, "PD restart not enabled: %d\n", ret);

	return NOTIFY_OK;
}

int ath10k_snoc_pd_restart_enable(struct ath10k *ar)
{
	int ret;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Get service location\n");

	ar_snoc->get_service_nb.notifier_call =
		ath10k_snoc_get_service_location_notify;
	ret = get_service_location(ATH10K_SERVICE_LOCATION_CLIENT_NAME,
				   ATH10K_WLAN_SERVICE_NAME,
				   &ar_snoc->get_service_nb);
	if (ret) {
		ath10k_err(ar, "Get service location failed: %d\n", ret);
		goto out;
	}

	return 0;
out:
	ath10k_err(ar, "PD restart not enabled: %d\n", ret);
	return ret;
}

int ath10k_snoc_pdr_unregister_notifier(struct ath10k *ar)
{
	int i;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	for (i = 0; i < ar_snoc->total_domains; i++) {
		if (ar_snoc->service_notifier[i].handle)
			service_notif_unregister_notifier(
				ar_snoc->service_notifier[i].handle,
				&ar_snoc->service_notifier_nb);
	}

	kfree(ar_snoc->service_notifier);

	ar_snoc->service_notifier = NULL;

	return 0;
}

static int ath10k_snoc_modem_notifier_nb(struct notifier_block *nb,
					 unsigned long code,
					 void *data)
{
	struct notif_data *notif = data;
	struct ath10k_snoc *ar_snoc = container_of(nb, struct ath10k_snoc,
						   modem_ssr_nb);
	struct ath10k *ar = ar_snoc->ar;
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	if (code != SUBSYS_BEFORE_SHUTDOWN)
		return NOTIFY_OK;

	if (notif->crashed) {
		atomic_set(&ar_snoc->fw_crashed, 1);
		atomic_set(&qmi_cfg->fw_ready, 0);
	}

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Modem went down %d\n",
		   atomic_read(&ar_snoc->fw_crashed));

	return NOTIFY_OK;
}

int ath10k_snoc_modem_ssr_register_notifier(struct ath10k *ar)
{
	int ret = 0;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	ar_snoc->modem_ssr_nb.notifier_call = ath10k_snoc_modem_notifier_nb;

	ar_snoc->modem_notify_handler =
		subsys_notif_register_notifier("modem", &ar_snoc->modem_ssr_nb);

	if (IS_ERR(ar_snoc->modem_notify_handler)) {
		ret = PTR_ERR(ar_snoc->modem_notify_handler);
		ath10k_err(ar, "Modem register notifier failed: %d\n", ret);
	}

	return ret;
}

int ath10k_snoc_modem_ssr_unregister_notifier(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	subsys_notif_unregister_notifier(ar_snoc->modem_notify_handler,
					 &ar_snoc->modem_ssr_nb);
	ar_snoc->modem_notify_handler = NULL;

	return 0;
}

static char *
ath10k_snoc_driver_event_to_str(enum ath10k_snoc_driver_event_type type)
{
	switch (type) {
	case ATH10K_SNOC_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case ATH10K_SNOC_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case ATH10K_SNOC_DRIVER_EVENT_FW_READY_IND:
		return "FW_READY";
	case ATH10K_SNOC_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

static int
ath10k_snoc_driver_event_post(enum ath10k_snoc_driver_event_type type,
			      u32 flags, void *data)
{
	int ret = 0;
	int i = 0;
	unsigned long irq_flags;
	struct ath10k *ar = (struct ath10k *)data;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Posting event: %s type: %d\n",
		   ath10k_snoc_driver_event_to_str(type), type);

	if (type >= ATH10K_SNOC_DRIVER_EVENT_MAX) {
		ath10k_err(ar, "Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	spin_lock_irqsave(&qmi_cfg->event_lock, irq_flags);

	for (i = 0; i < ATH10K_SNOC_DRIVER_EVENT_MAX; i++) {
		if (atomic_read(&qmi_cfg->qmi_ev_list[i].event_handled)) {
			qmi_cfg->qmi_ev_list[i].type = type;
			qmi_cfg->qmi_ev_list[i].data = data;
			init_completion(&qmi_cfg->qmi_ev_list[i].complete);
			qmi_cfg->qmi_ev_list[i].ret =
					ATH10K_SNOC_EVENT_PENDING;
			qmi_cfg->qmi_ev_list[i].sync =
					!!(flags & ATH10K_SNOC_EVENT_SYNC);
			atomic_set(&qmi_cfg->qmi_ev_list[i].event_handled, 0);
			list_add_tail(&qmi_cfg->qmi_ev_list[i].list,
				      &qmi_cfg->event_list);
			break;
		}
	}

	if (i >= ATH10K_SNOC_DRIVER_EVENT_MAX)
		i = ATH10K_SNOC_DRIVER_EVENT_SERVER_ARRIVE;

	spin_unlock_irqrestore(&qmi_cfg->event_lock, irq_flags);

	queue_work(qmi_cfg->event_wq, &qmi_cfg->event_work);

	if (!(flags & ATH10K_SNOC_EVENT_SYNC))
		goto out;

	if (flags & ATH10K_SNOC_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&qmi_cfg->qmi_ev_list[i].complete);
	else
		ret = wait_for_completion_interruptible(
			&qmi_cfg->qmi_ev_list[i].complete);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Completed event: %s(%d)\n",
		   ath10k_snoc_driver_event_to_str(type), type);

	spin_lock_irqsave(&qmi_cfg->event_lock, irq_flags);
	if (ret == -ERESTARTSYS &&
	    qmi_cfg->qmi_ev_list[i].ret == ATH10K_SNOC_EVENT_PENDING) {
		qmi_cfg->qmi_ev_list[i].sync = false;
		atomic_set(&qmi_cfg->qmi_ev_list[i].event_handled, 1);
		spin_unlock_irqrestore(&qmi_cfg->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&qmi_cfg->event_lock, irq_flags);

out:
	return ret;
}

static int
ath10k_snoc_wlan_mode_send_sync_msg(struct ath10k *ar,
				    enum wlfw_driver_mode_enum_v01 mode)
{
	int ret;
	struct wlfw_wlan_mode_req_msg_v01 req;
	struct wlfw_wlan_mode_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	if (!qmi_cfg || !qmi_cfg->wlfw_clnt)
		return -ENODEV;

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "Sending Mode request, mode: %d\n", mode);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mode = mode;
	req.hw_debug_valid = 1;
	req.hw_debug = 0;

	req_desc.max_msg_len = WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_MODE_REQ_V01;
	req_desc.ei_array = wlfw_wlan_mode_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_MODE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_MODE_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_mode_resp_msg_v01_ei;

	ret = qmi_send_req_wait(qmi_cfg->wlfw_clnt,
				&req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		ath10k_err(ar, "Send mode req failed, mode: %d ret: %d\n",
			   mode, ret);
		return ret;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath10k_err(ar, "QMI mode request rejected:");
		ath10k_err(ar, "mode:%d result:%d error:%d\n",
			   mode, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		return ret;
	}

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "wlan Mode request send success, mode: %d\n", mode);
	return 0;
}

static int
ath10k_snoc_wlan_cfg_send_sync_msg(struct ath10k *ar,
				   struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	int ret;
	struct wlfw_wlan_cfg_req_msg_v01 req;
	struct wlfw_wlan_cfg_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	if (!qmi_cfg || !qmi_cfg->wlfw_clnt)
		return -ENODEV;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Sending config request\n");

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	memcpy(&req, data, sizeof(req));

	req_desc.max_msg_len = WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_CFG_REQ_V01;
	req_desc.ei_array = wlfw_wlan_cfg_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_CFG_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_CFG_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_cfg_resp_msg_v01_ei;

	ret = qmi_send_req_wait(qmi_cfg->wlfw_clnt,
				&req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		ath10k_err(ar, "Send config req failed %d\n", ret);
		return ret;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath10k_err(ar, "QMI config request rejected:");
		ath10k_err(ar, "result:%d error:%d\n",
			   resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		return ret;
	}

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "wlan config request success..\n");
	return 0;
}

int ath10k_snoc_qmi_wlan_enable(struct ath10k *ar,
				struct ath10k_wlan_enable_cfg *config,
				enum ath10k_driver_mode mode,
				const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;
	unsigned long time_left;

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "Mode: %d, config: %p, host_version: %s\n",
		   mode, config, host_version);

	memset(&req, 0, sizeof(req));
	if (!config || !host_version) {
		ath10k_err(ar, "WLAN_EN Config Invalid:%p: host_version:%p\n",
			   config, host_version);
		ret = -EINVAL;
		return ret;
	}

	time_left = wait_event_timeout(
			   ath10k_fw_ready_wait_event,
			   (atomic_read(&qmi_cfg->fw_ready) &&
			    atomic_read(&qmi_cfg->server_connected)),
			   msecs_to_jiffies(ATH10K_SNOC_WLAN_FW_READY_TIMEOUT));
	if (time_left == 0) {
		ath10k_err(ar, "Wait for FW ready and server connect timed out\n");
		return -ETIMEDOUT;
	}

	req.host_version_valid = 1;
	strlcpy(req.host_version, host_version,
		QMI_WLFW_MAX_STR_LEN_V01 + 1);

	req.tgt_cfg_valid = 1;
	if (config->num_ce_tgt_cfg > QMI_WLFW_MAX_NUM_CE_V01)
		req.tgt_cfg_len = QMI_WLFW_MAX_NUM_CE_V01;
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
	if (config->num_ce_svc_pipe_cfg > QMI_WLFW_MAX_NUM_SVC_V01)
		req.svc_cfg_len = QMI_WLFW_MAX_NUM_SVC_V01;
	else
		req.svc_cfg_len = config->num_ce_svc_pipe_cfg;
	for (i = 0; i < req.svc_cfg_len; i++) {
		req.svc_cfg[i].service_id = config->ce_svc_cfg[i].service_id;
		req.svc_cfg[i].pipe_dir = config->ce_svc_cfg[i].pipe_dir;
		req.svc_cfg[i].pipe_num = config->ce_svc_cfg[i].pipe_num;
	}

	req.shadow_reg_valid = 1;
	if (config->num_shadow_reg_cfg >
	    QMI_WLFW_MAX_NUM_SHADOW_REG_V01)
		req.shadow_reg_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V01;
	else
		req.shadow_reg_len = config->num_shadow_reg_cfg;

	memcpy(req.shadow_reg, config->shadow_reg_cfg,
	       sizeof(struct wlfw_shadow_reg_cfg_s_v01) * req.shadow_reg_len);

	ret = ath10k_snoc_wlan_cfg_send_sync_msg(ar, &req);
	if (ret) {
		ath10k_err(ar, "WLAN config send failed\n");
		return ret;
	}

	ret = ath10k_snoc_wlan_mode_send_sync_msg(ar, mode);
	if (ret) {
		ath10k_err(ar, "WLAN mode send failed\n");
		return ret;
	}

	return 0;
}

int ath10k_snoc_qmi_wlan_disable(struct ath10k *ar)
{
	return ath10k_snoc_wlan_mode_send_sync_msg(ar, QMI_WLFW_OFF_V01);
}

static int ath10k_snoc_ind_register_send_sync_msg(struct ath10k *ar)
{
	int ret;
	struct wlfw_ind_register_req_msg_v01 req;
	struct wlfw_ind_register_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "Sending indication register message,\n");

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.client_id_valid = 1;
	req.client_id = WLFW_CLIENT_ID;
	req.fw_ready_enable_valid = 1;
	req.fw_ready_enable = 1;
	req.msa_ready_enable_valid = 1;
	req.msa_ready_enable = 1;

	req_desc.max_msg_len = WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_IND_REGISTER_REQ_V01;
	req_desc.ei_array = wlfw_ind_register_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_IND_REGISTER_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_IND_REGISTER_RESP_V01;
	resp_desc.ei_array = wlfw_ind_register_resp_msg_v01_ei;

	ret = qmi_send_req_wait(qmi_cfg->wlfw_clnt,
				&req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		ath10k_err(ar, "Send indication register req failed %d\n", ret);
		return ret;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath10k_err(ar, "QMI indication register request rejected:");
		ath10k_err(ar, "resut:%d error:%d\n",
			   resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		return ret;
	}

	return 0;
}

static void ath10k_snoc_qmi_wlfw_clnt_notify_work(struct work_struct *work)
{
	int ret;
	struct ath10k_snoc_qmi_config *qmi_cfg =
		container_of(work, struct ath10k_snoc_qmi_config,
			     qmi_recv_msg_work);
	struct ath10k_snoc *ar_snoc =
		container_of(qmi_cfg, struct ath10k_snoc, qmi_cfg);
	struct ath10k *ar = ar_snoc->ar;

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "Receiving Event in work queue context\n");

	do {
	} while ((ret = qmi_recv_msg(qmi_cfg->wlfw_clnt)) == 0);

	if (ret != -ENOMSG)
		ath10k_err(ar, "Error receiving message: %d\n", ret);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Receiving Event completed\n");
}

static void
ath10k_snoc_qmi_wlfw_clnt_notify(struct qmi_handle *handle,
				 enum qmi_event_type event,
				 void *notify_priv)
{
	struct ath10k_snoc_qmi_config *qmi_cfg =
		(struct ath10k_snoc_qmi_config *)notify_priv;
	struct ath10k_snoc *ar_snoc =
		container_of(qmi_cfg, struct ath10k_snoc, qmi_cfg);
	struct ath10k *ar = ar_snoc->ar;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "QMI client notify: %d\n", event);

	if (!qmi_cfg || !qmi_cfg->wlfw_clnt)
		return;

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&qmi_cfg->qmi_recv_msg_work);
		break;
	default:
		ath10k_dbg(ar, ATH10K_DBG_SNOC, "Unknown Event: %d\n", event);
		break;
	}
}

static void
ath10k_snoc_qmi_wlfw_clnt_ind(struct qmi_handle *handle,
			      unsigned int msg_id, void *msg,
			      unsigned int msg_len, void *ind_cb_priv)
{
	struct ath10k_snoc_qmi_config *qmi_cfg =
		(struct ath10k_snoc_qmi_config *)ind_cb_priv;
	struct ath10k_snoc *ar_snoc =
		container_of(qmi_cfg, struct ath10k_snoc, qmi_cfg);
	struct ath10k *ar = ar_snoc->ar;

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "Received Ind 0x%x, msg_len: %d\n", msg_id, msg_len);
	switch (msg_id) {
	case QMI_WLFW_FW_READY_IND_V01:
		ath10k_snoc_driver_event_post(
			ATH10K_SNOC_DRIVER_EVENT_FW_READY_IND, 0, ar);
		break;
	case QMI_WLFW_MSA_READY_IND_V01:
		qmi_cfg->msa_ready = true;
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "Received MSA Ready, ind = 0x%x\n", msg_id);
		break;
	default:
		ath10k_err(ar, "Invalid msg_id 0x%x\n", msg_id);
		break;
	}
}

static int ath10k_snoc_driver_event_server_arrive(struct ath10k *ar)
{
	int ret = 0;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	if (!qmi_cfg)
		return -ENODEV;

	qmi_cfg->wlfw_clnt = qmi_handle_create(
			ath10k_snoc_qmi_wlfw_clnt_notify, qmi_cfg);
	if (!qmi_cfg->wlfw_clnt) {
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "QMI client handle create failed\n");
		return -ENOMEM;
	}

	ret = qmi_connect_to_service(qmi_cfg->wlfw_clnt,
				     WLFW_SERVICE_ID_V01,
				     WLFW_SERVICE_VERS_V01,
				     WLFW_SERVICE_INS_ID_V01);
	if (ret < 0) {
		ath10k_err(ar, "QMI WLAN Service not found : %d\n", ret);
		goto err_qmi_config;
	}

	ret = qmi_register_ind_cb(qmi_cfg->wlfw_clnt,
				  ath10k_snoc_qmi_wlfw_clnt_ind, qmi_cfg);
	if (ret < 0) {
		ath10k_err(ar, "Failed to register indication callback: %d\n",
			   ret);
		goto err_qmi_config;
	}

	ret = ath10k_snoc_ind_register_send_sync_msg(ar);
	if (ret) {
		ath10k_err(ar, "Failed to config qmi ind register\n");
		goto err_qmi_config;
	}

	atomic_set(&qmi_cfg->server_connected, 1);
	wake_up_all(&ath10k_fw_ready_wait_event);
	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "QMI Server Arrive Configuration Success\n");
	return 0;

err_qmi_config:
	qmi_handle_destroy(qmi_cfg->wlfw_clnt);
	qmi_cfg->wlfw_clnt = NULL;
	return ret;
}

static int ath10k_snoc_driver_event_server_exit(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "QMI Server Exit event received\n");
	atomic_set(&qmi_cfg->fw_ready, 0);
	qmi_cfg->msa_ready = false;
	atomic_set(&qmi_cfg->server_connected, 0);
	qmi_handle_destroy(qmi_cfg->wlfw_clnt);
	return 0;
}

static int ath10k_snoc_driver_event_fw_ready_ind(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;
	int ret;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "FW Ready event received.\n");
	atomic_set(&qmi_cfg->fw_ready, 1);
	if (ar_snoc->drv_state == ATH10K_DRIVER_STATE_PROBED) {
		ret = ath10k_core_register(ar,
					   ar_snoc->target_info.soc_version);
		if (ret) {
			ath10k_err(ar,
				   "failed to register driver core: %d\n",
				   ret);
			return 0;
		}
		ar_snoc->drv_state = ATH10K_DRIVER_STATE_STARTED;
	}
	wake_up_all(&ath10k_fw_ready_wait_event);

	return 0;
}

static void ath10k_snoc_driver_event_work(struct work_struct *work)
{
	int ret;
	unsigned long irq_flags;
	struct ath10k_snoc_qmi_driver_event *event;
	struct ath10k_snoc_qmi_config *qmi_cfg =
		container_of(work, struct ath10k_snoc_qmi_config, event_work);
	struct ath10k_snoc *ar_snoc =
		container_of(qmi_cfg, struct ath10k_snoc, qmi_cfg);
	struct ath10k *ar = ar_snoc->ar;

	spin_lock_irqsave(&qmi_cfg->event_lock, irq_flags);

	while (!list_empty(&qmi_cfg->event_list)) {
		event = list_first_entry(&qmi_cfg->event_list,
					 struct ath10k_snoc_qmi_driver_event,
					 list);
		list_del(&event->list);
		spin_unlock_irqrestore(&qmi_cfg->event_lock, irq_flags);

		ath10k_dbg(ar, ATH10K_DBG_SNOC, "Processing event: %s%s(%d)\n",
			   ath10k_snoc_driver_event_to_str(event->type),
			   event->sync ? "-sync" : "", event->type);

		switch (event->type) {
		case ATH10K_SNOC_DRIVER_EVENT_SERVER_ARRIVE:
			ret = ath10k_snoc_driver_event_server_arrive(ar);
			break;
		case ATH10K_SNOC_DRIVER_EVENT_SERVER_EXIT:
			ret = ath10k_snoc_driver_event_server_exit(ar);
			break;
		case ATH10K_SNOC_DRIVER_EVENT_FW_READY_IND:
			ret = ath10k_snoc_driver_event_fw_ready_ind(ar);
			break;
		default:
			ath10k_err(ar, "Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		atomic_set(&event->event_handled, 1);
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "Event Processed: %s%s(%d), ret: %d\n",
			   ath10k_snoc_driver_event_to_str(event->type),
			   event->sync ? "-sync" : "", event->type, ret);
		spin_lock_irqsave(&qmi_cfg->event_lock, irq_flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&qmi_cfg->event_lock, irq_flags);
		spin_lock_irqsave(&qmi_cfg->event_lock, irq_flags);
	}

	spin_unlock_irqrestore(&qmi_cfg->event_lock, irq_flags);
}

static int
ath10k_snoc_qmi_wlfw_clnt_svc_event_notify(struct notifier_block *this,
					   unsigned long code,
					   void *_cmd)
{
	int ret = 0;
	struct ath10k_snoc_qmi_config *qmi_cfg =
		container_of(this, struct ath10k_snoc_qmi_config, wlfw_clnt_nb);
	struct ath10k_snoc *ar_snoc =
			container_of(qmi_cfg, struct ath10k_snoc, qmi_cfg);
	struct ath10k *ar = ar_snoc->ar;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Event Notify: code: %ld", code);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		ret = ath10k_snoc_driver_event_post(
			ATH10K_SNOC_DRIVER_EVENT_SERVER_ARRIVE, 0, ar);
		break;
	case QMI_SERVER_EXIT:
		ret = ath10k_snoc_driver_event_post(
			ATH10K_SNOC_DRIVER_EVENT_SERVER_EXIT, 0, ar);
		break;
	default:
		ath10k_err(ar, "Invalid code: %ld", code);
		break;
	}

	return ret;
}

int ath10k_snoc_start_qmi_service(struct ath10k *ar)
{
	int ret;
	int i;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	qmi_cfg->event_wq = alloc_workqueue("ath10k_snoc_driver_event",
					    WQ_UNBOUND, 1);
	if (!qmi_cfg->event_wq) {
		ath10k_err(ar, "Workqueue creation failed\n");
		return -EFAULT;
	}

	spin_lock_init(&qmi_cfg->event_lock);
	atomic_set(&qmi_cfg->fw_ready, 0);
	atomic_set(&qmi_cfg->server_connected, 0);

	INIT_WORK(&qmi_cfg->event_work, ath10k_snoc_driver_event_work);
	INIT_WORK(&qmi_cfg->qmi_recv_msg_work,
		  ath10k_snoc_qmi_wlfw_clnt_notify_work);
	INIT_LIST_HEAD(&qmi_cfg->event_list);

	for (i = 0; i < ATH10K_SNOC_DRIVER_EVENT_MAX; i++)
		atomic_set(&qmi_cfg->qmi_ev_list[i].event_handled, 1);

	qmi_cfg->wlfw_clnt_nb.notifier_call =
		ath10k_snoc_qmi_wlfw_clnt_svc_event_notify;
	ret = qmi_svc_event_notifier_register(WLFW_SERVICE_ID_V01,
					      WLFW_SERVICE_VERS_V01,
					      WLFW_SERVICE_INS_ID_V01,
					      &qmi_cfg->wlfw_clnt_nb);
	if (ret < 0) {
		ath10k_err(ar, "Notifier register failed: %d\n", ret);
		ret = -EFAULT;
		goto out_destroy_wq;
	}

	if (icnss_is_fw_ready())
		atomic_set(&qmi_cfg->fw_ready, 1);
	else
		ath10k_err(ar, "FW ready indication not received yet\n");

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "QMI service started successfully\n");
	return 0;

	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &qmi_cfg->wlfw_clnt_nb);
out_destroy_wq:
	destroy_workqueue(qmi_cfg->event_wq);
	return ret;
}

void ath10k_snoc_stop_qmi_service(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_qmi_config *qmi_cfg = &ar_snoc->qmi_cfg;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Removing QMI service..\n");

	wake_up_all(&ath10k_fw_ready_wait_event);
	cancel_work_sync(&qmi_cfg->event_work);
	cancel_work_sync(&qmi_cfg->qmi_recv_msg_work);
	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &qmi_cfg->wlfw_clnt_nb);
	destroy_workqueue(qmi_cfg->event_wq);
	qmi_handle_destroy(qmi_cfg->wlfw_clnt);
	qmi_cfg = NULL;
}
