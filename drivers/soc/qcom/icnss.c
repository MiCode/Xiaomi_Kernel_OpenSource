/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/qmi_encdec.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/msm_qmi_interface.h>

#include "wlan_firmware_service_v01.h"

enum icnss_qmi_event_type {
	ICNSS_QMI_EVENT_SERVER_ARRIVE,
	ICNSS_QMI_EVENT_SERVER_EXIT,
	ICNSS_QMI_EVENT_FW_READY_IND,
};

struct icnss_qmi_event {
	struct list_head list;
	enum icnss_qmi_event_type type;
	void *data;
};

#define ICNSS_PANIC			1
#define WLFW_TIMEOUT_MS			3000
#define WLFW_SERVICE_INS_ID_V01		0
#define ICNSS_WLFW_QMI_CONNECTED	BIT(0)
#define ICNSS_FW_READY			BIT(1)

#define ICNSS_IS_WLFW_QMI_CONNECTED(_state) \
		((_state) & ICNSS_WLFW_QMI_CONNECTED)
#define ICNSS_IS_FW_READY(_state) ((_state) & ICNSS_FW_READY)

#ifdef ICNSS_PANIC
#define ICNSS_ASSERT(_condition) do {			\
		if (!(_condition)) {				\
			pr_err("ICNSS ASSERT in %s Line %d\n",	\
				__func__, __LINE__);		\
			BUG_ON(1);				\
		}						\
	} while (0)
#else
#define ICNSS_ASSERT(_condition) do {			\
		if (!(_condition)) {				\
			pr_err("ICNSS ASSERT in %s Line %d\n",	\
				__func__, __LINE__);		\
			WARN_ON(1);				\
		}						\
	} while (0)
#endif

struct ce_irq_list {
	int irq;
	irqreturn_t (*handler)(int, void *);
};

static struct {
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
	struct qmi_handle *wlfw_clnt;
	struct list_head qmi_event_list;
	spinlock_t qmi_event_lock;
	struct work_struct qmi_event_work;
	struct work_struct qmi_recv_msg_work;
	struct workqueue_struct *qmi_event_wq;
	uint32_t state;
	u32 board_id;
	u32 num_peers;
	u32 mac_version;
	char fw_version[QMI_WLFW_MAX_STR_LEN_V01 + 1];
} *penv;

static int icnss_qmi_event_post(enum icnss_qmi_event_type type, void *data)
{
	struct icnss_qmi_event *event = NULL;
	unsigned long flags;
	int gfp = GFP_KERNEL;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (event == NULL)
		return -ENOMEM;

	event->type = type;
	event->data = data;
	spin_lock_irqsave(&penv->qmi_event_lock, flags);
	list_add_tail(&event->list, &penv->qmi_event_list);
	spin_unlock_irqrestore(&penv->qmi_event_lock, flags);

	queue_work(penv->qmi_event_wq, &penv->qmi_event_work);

	return 0;
}

static int wlfw_ind_register_send_sync_msg(void)
{
	int ret;
	struct wlfw_ind_register_req_msg_v01 req;
	struct wlfw_ind_register_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.fw_ready_enable_valid = 1;
	req.fw_ready_enable = 1;

	req_desc.max_msg_len = WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_IND_REGISTER_REQ_V01;
	req_desc.ei_array = wlfw_ind_register_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_IND_REGISTER_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_IND_REGISTER_RESP_V01;
	resp_desc.ei_array = wlfw_ind_register_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s: send req failed %d\n", __func__, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed %d %d\n",
		       __func__, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
out:
	return ret;
}

static int wlfw_cap_send_sync_msg(void)
{
	int ret;
	struct wlfw_cap_req_msg_v01 req;
	struct wlfw_cap_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_CAP_REQ_V01;
	req_desc.ei_array = wlfw_cap_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_CAP_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_CAP_RESP_V01;
	resp_desc.ei_array = wlfw_cap_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s: send req failed %d\n", __func__, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed %d %d\n",
		       __func__, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

	/* store cap locally */
	if (resp.board_id_valid)
		penv->board_id = resp.board_id;
	if (resp.num_peers_valid)
		penv->num_peers = resp.num_peers;
	if (resp.mac_version_valid)
		penv->mac_version = resp.mac_version;
	if (resp.fw_version_valid)
		strlcpy(penv->fw_version, resp.fw_version,
			QMI_WLFW_MAX_STR_LEN_V01 + 1);

	pr_debug("%s: board_id:0x%0x num_peers: %d mac_version: 0x%0x fw_version: %s",
		__func__, penv->board_id, penv->num_peers,
		penv->mac_version, penv->fw_version);
out:
	return ret;
}

static int wlfw_wlan_mode_send_sync_msg(enum wlfw_driver_mode_enum_v01 mode)
{
	int ret;
	struct wlfw_wlan_mode_req_msg_v01 req;
	struct wlfw_wlan_mode_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mode = mode;

	req_desc.max_msg_len = WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_MODE_REQ_V01;
	req_desc.ei_array = wlfw_wlan_mode_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_MODE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_MODE_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_mode_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s: send req failed %d\n", __func__, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed %d %d\n",
		       __func__, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
out:
	return ret;
}

static int wlfw_wlan_cfg_send_sync_msg(struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	int ret;
	struct wlfw_wlan_cfg_req_msg_v01 req;
	struct wlfw_wlan_cfg_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		return -ENODEV;
		goto out;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	memcpy(&req, data, sizeof(req));

	req_desc.max_msg_len = WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_CFG_REQ_V01;
	req_desc.ei_array = wlfw_wlan_cfg_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_CFG_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_CFG_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_cfg_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s: send req failed %d\n", __func__, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed %d %d\n",
		       __func__, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
out:
	return ret;
}

static void icnss_qmi_wlfw_clnt_notify_work(struct work_struct *work)
{
	int ret;

	if (!penv || !penv->wlfw_clnt)
		return;

	do {
		pr_debug("%s: Received Event\n", __func__);
	} while ((ret = qmi_recv_msg(penv->wlfw_clnt)) == 0);

	if (ret != -ENOMSG)
		pr_err("%s: Error receiving message\n", __func__);
}

static void icnss_qmi_wlfw_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	if (!penv || !penv->wlfw_clnt)
		return;

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&penv->qmi_recv_msg_work);
		break;
	default:
		pr_debug("%s: Received Event:  %d\n", __func__, event);
		break;
	}
}

static void icnss_qmi_wlfw_clnt_ind(struct qmi_handle *handle,
			  unsigned int msg_id, void *msg,
			  unsigned int msg_len, void *ind_cb_priv)
{
	if (!penv)
		return;

	pr_debug("%s: Received Ind 0x%x\n", __func__, msg_id);

	switch (msg_id) {
	case QMI_WLFW_FW_READY_IND_V01:
		icnss_qmi_event_post(ICNSS_QMI_EVENT_FW_READY_IND, NULL);
		break;
	default:
		pr_err("%s: Invalid msg_id 0x%x\n", __func__, msg_id);
		break;
	}
}

static int icnss_qmi_event_server_arrive(void *data)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	penv->wlfw_clnt = qmi_handle_create(icnss_qmi_wlfw_clnt_notify, penv);
	if (!penv->wlfw_clnt) {
		pr_err("%s: QMI client handle alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	ret = qmi_connect_to_service(penv->wlfw_clnt,
					WLFW_SERVICE_ID_V01,
					WLFW_SERVICE_VERS_V01,
					WLFW_SERVICE_INS_ID_V01);
	if (ret < 0) {
		pr_err("%s: Server not found : %d\n", __func__, ret);
		goto fail;
	}

	ret = qmi_register_ind_cb(penv->wlfw_clnt,
				  icnss_qmi_wlfw_clnt_ind, penv);
	if (ret < 0) {
		pr_err("Failed to register indication callback: %d\n",
		       ret);
		goto fail;
	}

	penv->state |= ICNSS_WLFW_QMI_CONNECTED;

	pr_info("%s: QMI Server Connected\n", __func__);

	ret = wlfw_ind_register_send_sync_msg();
	if (ret < 0) {
		pr_err("Failed to send indication message: %d\n",
		       ret);
		goto out;
	}

	ret = wlfw_cap_send_sync_msg();
	if (ret < 0) {
		pr_err("Failed to get capability: %d\n",
		       ret);
		goto out;
	}
	return ret;
fail:
	qmi_handle_destroy(penv->wlfw_clnt);
	penv->wlfw_clnt = NULL;
out:
	ICNSS_ASSERT(0);
	return ret;
}

static int icnss_qmi_event_server_exit(void *data)
{
	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	pr_info("%s: QMI Service Disconnected\n", __func__);

	qmi_handle_destroy(penv->wlfw_clnt);
	penv->state = 0;
	penv->wlfw_clnt = NULL;

	return 0;
}

static int icnss_qmi_event_fw_ready_ind(void *data)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	penv->state |= ICNSS_FW_READY;

	if (!penv->pdev) {
		pr_err("%s: Device is not ready\n", __func__);
		ret = -ENODEV;
		goto out;
	}
	if (!penv->ops || !penv->ops->probe) {
		pr_err("%s: WLAN driver is not registed yet\n", __func__);
		ret = -ENOENT;
		goto out;
	}

	ret = penv->ops->probe(&penv->pdev->dev);
	if (ret < 0)
		pr_err("%s: Driver probe failed: %d\n", __func__, ret);
out:
	return ret;
}

static int icnss_qmi_wlfw_clnt_svc_event_notify(struct notifier_block *this,
					       unsigned long code,
					       void *_cmd)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	pr_debug("Event Notify: code: %ld", code);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		ret = icnss_qmi_event_post(ICNSS_QMI_EVENT_SERVER_ARRIVE, NULL);
		break;

	case QMI_SERVER_EXIT:
		ret = icnss_qmi_event_post(ICNSS_QMI_EVENT_SERVER_EXIT, NULL);
		break;
	default:
		pr_debug("Invalid code: %ld", code);
		break;
	}
	return ret;
}

static void icnss_qmi_wlfw_event_work(struct work_struct *work)
{
	struct icnss_qmi_event *event;
	unsigned long flags;

	spin_lock_irqsave(&penv->qmi_event_lock, flags);

	while (!list_empty(&penv->qmi_event_list)) {
		event = list_first_entry(&penv->qmi_event_list,
					 struct icnss_qmi_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&penv->qmi_event_lock, flags);

		switch (event->type) {
		case ICNSS_QMI_EVENT_SERVER_ARRIVE:
			icnss_qmi_event_server_arrive(event->data);
			break;
		case ICNSS_QMI_EVENT_SERVER_EXIT:
			icnss_qmi_event_server_exit(event->data);
			break;
		case ICNSS_QMI_EVENT_FW_READY_IND:
			icnss_qmi_event_fw_ready_ind(event->data);
			break;
		default:
			pr_debug("Invalid Event type: %d", event->type);
			break;
		}
		kfree(event);
		spin_lock_irqsave(&penv->qmi_event_lock, flags);
	}
	spin_unlock_irqrestore(&penv->qmi_event_lock, flags);
}

static struct notifier_block wlfw_clnt_nb = {
	.notifier_call = icnss_qmi_wlfw_clnt_svc_event_notify,
};

int icnss_register_driver(struct icnss_driver_ops *ops)
{
	struct platform_device *pdev;
	int ret = 0;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	pdev = penv->pdev;
	if (!pdev) {
		ret = -ENODEV;
		goto out;
	}

	if (penv->ops) {
		pr_err("icnss: driver already registered\n");
		ret = -EEXIST;
		goto out;
	}
	penv->ops = ops;

	/* check for all conditions before invoking probe */
	if (ICNSS_IS_FW_READY(penv->state) && penv->ops->probe)
		ret = penv->ops->probe(&pdev->dev);

out:
	return ret;
}
EXPORT_SYMBOL(icnss_register_driver);

int icnss_unregister_driver(struct icnss_driver_ops *ops)
{
	int ret = 0;
	struct platform_device *pdev;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	pdev = penv->pdev;
	if (!pdev) {
		ret = -ENODEV;
		goto out;
	}
	if (!penv->ops) {
		pr_err("icnss: driver not registered\n");
		ret = -ENOENT;
		goto out;
	}
	if (penv->ops->remove)
		penv->ops->remove(&pdev->dev);

	penv->ops = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_driver);

int icnss_register_ce_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		pr_err("icnss: Invalid CE ID %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		pr_err("icnss: handler already registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, IRQF_SHARED, name, &penv->pdev->dev);
	if (ret) {
		pr_err("icnss: IRQ not registered %d\n", irq);
		ret = -EINVAL;
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;
	pr_debug("icnss: IRQ registered %d\n", irq);
out:
	return ret;

}
EXPORT_SYMBOL(icnss_register_ce_irq);

int icnss_unregister_ce_irq(unsigned int ce_id)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		pr_err("icnss: handler not registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, &penv->pdev->dev);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_ce_irq);

int icnss_ce_request_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		pr_err("icnss: Invalid CE ID %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		pr_err("icnss: handler already registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, flags, name, ctx);
	if (ret) {
		pr_err("icnss: IRQ not registered %d\n", irq);
		ret = -EINVAL;
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;
	pr_debug("icnss: IRQ registered %d\n", irq);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_request_irq);

int icnss_ce_free_irq(unsigned int ce_id, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		pr_err("icnss: handler not registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, ctx);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_free_irq);

void icnss_enable_irq(unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev) {
		pr_err("icnss: platform driver not initialized\n");
		return;
	}
	irq = penv->ce_irqs[ce_id];
	enable_irq(irq);
}
EXPORT_SYMBOL(icnss_enable_irq);

void icnss_disable_irq(unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev) {
		pr_err("icnss: platform driver not initialized\n");
		return;
	}
	irq = penv->ce_irqs[ce_id];
	disable_irq(irq);
}
EXPORT_SYMBOL(icnss_disable_irq);

int icnss_get_soc_info(struct icnss_soc_info *info)
{
	if (!penv) {
		pr_err("icnss: platform driver not initialized\n");
		return -EINVAL;
	}

	info->v_addr = penv->mem_base_va;
	info->p_addr = penv->mem_base_pa;

	return 0;
}
EXPORT_SYMBOL(icnss_get_soc_info);

int icnss_wlan_enable(struct icnss_wlan_enable_cfg *config,
		      enum icnss_driver_mode mode,
		      const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret;

	memset(&req, 0, sizeof(req));

	if (mode == ICNSS_WALTEST)
		goto skip;
	else if (!config || !host_version) {
		pr_err("%s: Invalid cfg pointer\n", __func__);
		ret = -EINVAL;
		goto out;
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

	ret = wlfw_wlan_cfg_send_sync_msg(&req);
	if (ret) {
		pr_err("%s: Failed to send cfg, ret = %d\n", __func__, ret);
		goto out;
	}
skip:
	ret = wlfw_wlan_mode_send_sync_msg(mode);
	if (ret)
		pr_err("%s: Failed to send mode, ret = %d\n", __func__, ret);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_wlan_enable);

int icnss_wlan_disable(enum icnss_driver_mode mode)
{
	return wlfw_wlan_mode_send_sync_msg(QMI_WLFW_OFF_V01);
}
EXPORT_SYMBOL(icnss_wlan_disable);

int icnss_get_ce_id(int irq)
{
	int i;

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		if (penv->ce_irqs[i] == irq)
			return i;
	}
	pr_err("icnss: No matching CE id for irq %d\n", irq);
	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_ce_id);

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	int i;

	if (penv)
		return -EEXIST;

	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;

	penv->pdev = pdev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "membase");
	if (!res) {
		pr_err("icnss: Memory base not found\n");
		ret = -EINVAL;
		goto out;
	}
	penv->mem_base_pa = res->start;
	penv->mem_base_va = ioremap(penv->mem_base_pa, resource_size(res));
	if (!penv->mem_base_va) {
		pr_err("icnss: ioremap failed\n");
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			pr_err("icnss: Fail to get IRQ-%d\n", i);
			ret = -ENODEV;
			goto out;
		} else {
			penv->ce_irqs[i] = res->start;
		}
	}

	penv->qmi_event_wq = alloc_workqueue("icnss_qmi_event", 0, 0);
	if (!penv->qmi_event_wq) {
		pr_err("%s: workqueue creation failed\n", __func__);
		ret = -EFAULT;
		goto out;
	}

	INIT_WORK(&penv->qmi_event_work, icnss_qmi_wlfw_event_work);
	INIT_WORK(&penv->qmi_recv_msg_work, icnss_qmi_wlfw_clnt_notify_work);
	INIT_LIST_HEAD(&penv->qmi_event_list);

	ret = qmi_svc_event_notifier_register(WLFW_SERVICE_ID_V01,
					      WLFW_SERVICE_VERS_V01,
					      WLFW_SERVICE_INS_ID_V01,
					      &wlfw_clnt_nb);
	if (ret < 0) {
		pr_err("%s: notifier register failed\n", __func__);
		destroy_workqueue(penv->qmi_event_wq);
		goto out;
	}

	pr_debug("icnss: Platform driver probed successfully\n");
out:
	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &wlfw_clnt_nb);
	if (penv->qmi_event_wq)
		destroy_workqueue(penv->qmi_event_wq);

	return 0;
}


static const struct of_device_id icnss_dt_match[] = {
	{.compatible = "qcom,icnss"},
	{}
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.driver = {
		.name = "icnss",
		.owner = THIS_MODULE,
		.of_match_table = icnss_dt_match,
	},
};

static int __init icnss_initialize(void)
{
	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iCNSS CORE platform driver");
