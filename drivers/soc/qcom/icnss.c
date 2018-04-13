/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
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
#define MAX_PROP_SIZE			32
#define MAX_VOLTAGE_LEVEL		2
#define VREG_ON				1
#define VREG_OFF			0

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

struct icnss_vreg_info {
	struct regulator *reg;
	const char *name;
	u32 nominal_min;
	u32 max_voltage;
	bool state;
};

static struct {
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	struct icnss_vreg_info vreg_info;
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
	struct qmi_handle *wlfw_clnt;
	struct list_head qmi_event_list;
	spinlock_t qmi_event_lock;
	struct work_struct qmi_event_work;
	struct work_struct qmi_recv_msg_work;
	struct workqueue_struct *qmi_event_wq;
	phys_addr_t msa_pa;
	uint32_t msa_mem_size;
	void *msa_va;
	uint32_t state;
	struct wlfw_rf_chip_info_s_v01 chip_info;
	struct wlfw_rf_board_info_s_v01 board_info;
	struct wlfw_soc_info_s_v01 soc_info;
	struct wlfw_fw_version_info_s_v01 fw_version_info;
	u32 pwr_pin_result;
	u32 phy_io_pin_result;
	u32 rf_pin_result;
	struct icnss_mem_region_info
		icnss_mem_region[QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01];
	bool skip_qmi;
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

static int icnss_qmi_pin_connect_result_ind(void *msg, unsigned int msg_len)
{
	struct msg_desc ind_desc;
	struct wlfw_pin_connect_result_ind_msg_v01 ind_msg;
	int ret = 0;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	ind_desc.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01;
	ind_desc.max_msg_len = WLFW_PIN_CONNECT_RESULT_IND_MSG_V01_MAX_MSG_LEN;
	ind_desc.ei_array = wlfw_pin_connect_result_ind_msg_v01_ei;

	ret = qmi_kernel_decode(&ind_desc, &ind_msg, msg, msg_len);
	if (ret < 0) {
		pr_err("%s: Failed to decode message!\n", __func__);
		goto out;
	}

	/* store pin result locally */
	if (ind_msg.pwr_pin_result_valid)
		penv->pwr_pin_result = ind_msg.pwr_pin_result;
	if (ind_msg.phy_io_pin_result_valid)
		penv->phy_io_pin_result = ind_msg.phy_io_pin_result;
	if (ind_msg.rf_pin_result_valid)
		penv->rf_pin_result = ind_msg.rf_pin_result;

	pr_debug("%s: Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		__func__, ind_msg.pwr_pin_result, ind_msg.phy_io_pin_result,
		ind_msg.rf_pin_result);
out:
	return ret;
}

static int icnss_vreg_on(struct icnss_vreg_info *vreg_info)
{
	int ret = 0;

	if (!vreg_info->reg) {
		pr_err("%s: regulator is not initialized\n", __func__);
		return -ENOENT;
	}

	if (!vreg_info->max_voltage || !vreg_info->nominal_min) {
		pr_err("%s: %s invalid constraints specified\n",
			__func__, vreg_info->name);
		return -EINVAL;
	}

	ret = regulator_set_voltage(vreg_info->reg,
			vreg_info->nominal_min, vreg_info->max_voltage);
	if (ret < 0) {
		pr_err("%s: regulator_set_voltage failed for (%s). min_uV=%d,max_uV=%d,ret=%d\n",
			__func__, vreg_info->name,
			vreg_info->nominal_min,
			vreg_info->max_voltage, ret);
		return ret;
	}

	ret = regulator_enable(vreg_info->reg);
	if (ret < 0) {
		pr_err("%s: Fail to enable regulator (%s) ret=%d\n",
			__func__, vreg_info->name, ret);
	}
	return ret;
}

static int icnss_vreg_off(struct icnss_vreg_info *vreg_info)
{
	int ret = 0;
	int min_uV = 0;

	if (!vreg_info->reg) {
		pr_err("%s: regulator is not initialized\n", __func__);
		return -ENOENT;
	}

	ret = regulator_disable(vreg_info->reg);
	if (ret < 0) {
		pr_err("%s: Fail to disable regulator (%s) ret=%d\n",
			__func__, vreg_info->name, ret);
		return ret;
	}

	ret = regulator_set_voltage(vreg_info->reg,
				    min_uV, vreg_info->max_voltage);
	if (ret < 0) {
		pr_err("%s: regulator_set_voltage failed for (%s). min_uV=%d,max_uV=%d,ret=%d\n",
			__func__, vreg_info->name, min_uV,
			vreg_info->max_voltage, ret);
	}
	return ret;
}

static int icnss_vreg_set(bool state)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info = &penv->vreg_info;

	if (vreg_info->state == state) {
		pr_debug("Already %s state is %s\n", vreg_info->name,
			state ? "enabled" : "disabled");
		return ret;
	}

	if (state)
		ret = icnss_vreg_on(vreg_info);
	else
		ret = icnss_vreg_off(vreg_info);

	if (ret < 0)
		goto out;

	pr_debug("%s: %s is now %s\n", __func__, vreg_info->name,
			state ? "enabled" : "disabled");

	vreg_info->state = state;
out:
	return ret;
}

static int icnss_adrastea_power_on(void)
{
	int ret = 0;

	ret = icnss_vreg_set(VREG_ON);
	if (ret < 0) {
		pr_err("%s: Failed to turn on voltagre regulator: %d\n",
		       __func__, ret);
		goto out;
	}
	/* TZ API of power on adrastea */
out:
	return ret;
}

static int icnss_adrastea_power_off(void)
{
	int ret = 0;

	ret = icnss_vreg_set(VREG_OFF);
	if (ret < 0) {
		pr_err("%s: Failed to turn off voltagre regulator: %d\n",
		       __func__, ret);
		goto out;
	}
	/* TZ API of power off adrastea */
out:
	return ret;
}

static int wlfw_msa_mem_info_send_sync_msg(void)
{
	int ret = 0;
	int i;
	struct wlfw_msa_info_req_msg_v01 req;
	struct wlfw_msa_info_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.msa_addr = penv->msa_pa;
	req.size = penv->msa_mem_size;

	req_desc.max_msg_len = WLFW_MSA_INFO_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_MSA_INFO_REQ_V01;
	req_desc.ei_array = wlfw_msa_info_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_MSA_INFO_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_MSA_INFO_RESP_V01;
	resp_desc.ei_array = wlfw_msa_info_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
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

	pr_debug("%s: Receive mem_region_info_len: %d\n",
			__func__, resp.mem_region_info_len);

	if (resp.mem_region_info_len > 2) {
		pr_err("%s : Invalid memory region length received\n",
		       __func__);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < resp.mem_region_info_len; i++) {
		penv->icnss_mem_region[i].reg_addr =
			resp.mem_region_info[i].region_addr;
		penv->icnss_mem_region[i].size =
			resp.mem_region_info[i].size;
		penv->icnss_mem_region[i].secure_flag =
			resp.mem_region_info[i].secure_flag;
		pr_debug("%s : Memory Region: %d  Addr:0x%x Size : %d Flag: %d\n",
			 __func__,
			 i,
			 (unsigned int)penv->icnss_mem_region[i].reg_addr,
			 penv->icnss_mem_region[i].size,
			 penv->icnss_mem_region[i].secure_flag);
	}

out:
	return ret;
}

static int wlfw_msa_ready_send_sync_msg(void)
{
	int ret;
	struct wlfw_msa_ready_req_msg_v01 req;
	struct wlfw_msa_ready_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_MSA_READY_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_MSA_READY_REQ_V01;
	req_desc.ei_array = wlfw_msa_ready_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_MSA_READY_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_MSA_READY_RESP_V01;
	resp_desc.ei_array = wlfw_msa_ready_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
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
	req.msa_ready_enable_valid = 1;
	req.msa_ready_enable = 1;
	req.pin_connect_result_enable_valid = 1;
	req.pin_connect_result_enable = 1;

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
	if (resp.chip_info_valid)
		penv->chip_info = resp.chip_info;
	if (resp.board_info_valid)
		penv->board_info = resp.board_info;
	else
		penv->board_info.board_id = 0xFF;
	if (resp.soc_info_valid)
		penv->soc_info = resp.soc_info;
	if (resp.fw_version_info_valid)
		penv->fw_version_info = resp.fw_version_info;

	pr_debug("%s: chip_id: 0x%0x, chip_family: 0x%0x, board_id: 0x%0x, soc_id: 0x%0x, fw_version: 0x%0x, fw_build_timestamp: %s",
		__func__,
		penv->chip_info.chip_id,
		penv->chip_info.chip_family,
		penv->board_info.board_id,
		penv->soc_info.soc_id,
		penv->fw_version_info.fw_version,
		penv->fw_version_info.fw_build_timestamp);
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

static int wlfw_ini_send_sync_msg(bool enablefwlog)
{
	int ret;
	struct wlfw_ini_req_msg_v01 req;
	struct wlfw_ini_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.enablefwlog_valid = 1;
	req.enablefwlog = enablefwlog;

	req_desc.max_msg_len = WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_INI_REQ_V01;
	req_desc.ei_array = wlfw_ini_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_INI_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_INI_RESP_V01;
	resp_desc.ei_array = wlfw_ini_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
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
	case QMI_WLFW_MSA_READY_IND_V01:
		pr_debug("%s: Received MSA Ready Indication msg_id 0x%x\n",
			 __func__, msg_id);
		break;
	case QMI_WLFW_PIN_CONNECT_RESULT_IND_V01:
		pr_debug("%s: Received Pin Connect Test Result msg_id 0x%x\n",
			 __func__, msg_id);
		icnss_qmi_pin_connect_result_ind(msg, msg_len);
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
		pr_err("%s: Failed to register indication callback: %d\n",
		       __func__, ret);
		goto fail;
	}

	penv->state |= ICNSS_WLFW_QMI_CONNECTED;

	pr_info("%s: QMI Server Connected\n", __func__);

	ret = icnss_adrastea_power_on();
	if (ret < 0) {
		pr_err("%s: Failed to power on hardware: %d\n",
		       __func__, ret);
		goto fail;
	}

	ret = wlfw_ind_register_send_sync_msg();
	if (ret < 0) {
		pr_err("%s: Failed to send indication message: %d\n",
		       __func__, ret);
		goto err_power_on;
	}

	if (penv->msa_va) {
		ret = wlfw_msa_mem_info_send_sync_msg();
		if (ret < 0) {
			pr_err("%s: Failed to send MSA info: %d\n",
			       __func__, ret);
			goto err_power_on;
		}
		ret = wlfw_msa_ready_send_sync_msg();
		if (ret < 0) {
			pr_err("%s: Failed to send MSA ready : %d\n",
			       __func__, ret);
			goto err_power_on;
		}
	} else {
		pr_err("%s: Invalid MSA address\n", __func__);
		ret = -EINVAL;
		goto err_power_on;
	}

	ret = wlfw_cap_send_sync_msg();
	if (ret < 0) {
		pr_err("%s: Failed to get capability: %d\n",
		       __func__, ret);
		goto err_power_on;
	}
	return ret;

err_power_on:
	ret = icnss_vreg_set(VREG_OFF);
	if (ret < 0) {
		pr_err("%s: Failed to turn off voltagre regulator: %d\n",
		       __func__, ret);
	}
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

	ret = icnss_adrastea_power_off();
	if (ret < 0) {
		pr_err("%s: Failed to power off hardware: %d\n",
		       __func__, ret);
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

	pr_debug("%s: Event Notify: code: %ld", __func__, code);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		ret = icnss_qmi_event_post(ICNSS_QMI_EVENT_SERVER_ARRIVE, NULL);
		break;

	case QMI_SERVER_EXIT:
		ret = icnss_qmi_event_post(ICNSS_QMI_EVENT_SERVER_EXIT, NULL);
		break;
	default:
		pr_debug("%s: Invalid code: %ld", __func__, code);
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
			pr_debug("%s: Invalid Event type: %d",
				 __func__, event->type);
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

	if (penv->skip_qmi)
		penv->state |= ICNSS_FW_READY;

	/* check for all conditions before invoking probe */
	if (ICNSS_IS_FW_READY(penv->state) && penv->ops->probe) {
		ret = icnss_vreg_set(VREG_ON);
		if (ret < 0) {
			pr_err("%s: Failed to turn on voltagre regulator: %d\n",
				__func__, ret);
			goto out;
		}
		ret = penv->ops->probe(&pdev->dev);
	} else {
		pr_err("icnss: FW is not ready\n");
		ret = -ENOENT;
	}
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

	ret = icnss_vreg_set(VREG_OFF);
	if (ret < 0)
		pr_err("%s: Failed to turn off voltagre regulator: %d\n",
		       __func__, ret);
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

int icnss_set_fw_debug_mode(bool enablefwlog)
{
	int ret;

	ret = wlfw_ini_send_sync_msg(enablefwlog);
	if (ret)
		pr_err("icnss: Fail to send ini, ret = %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(icnss_set_fw_debug_mode);

int icnss_wlan_enable(struct icnss_wlan_enable_cfg *config,
		      enum icnss_driver_mode mode,
		      const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret;

	memset(&req, 0, sizeof(req));

	if (mode == ICNSS_WALTEST || mode == ICNSS_CCPM)
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
	if (penv->skip_qmi)
		ret = 0;

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

static ssize_t icnss_wlan_mode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	int val;
	int ret;

	if (!penv)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	if (val == ICNSS_WALTEST || val == ICNSS_CCPM) {
		pr_debug("%s: WLAN Test Mode -> %d\n", __func__, val);
		ret = icnss_wlan_enable(NULL, val, NULL);
		if (ret)
			pr_err("%s: WLAN Test Mode %d failed with %d\n",
			       __func__, val, ret);
	} else {
		pr_err("%s: Mode %d is not supported from command line\n",
		       __func__, val);
		ret = -EINVAL;
	}

	return ret;
}

static DEVICE_ATTR(icnss_wlan_mode, S_IWUSR, NULL, icnss_wlan_mode_store);

static int icnss_dt_parse_vreg_info(struct device *dev,
				struct icnss_vreg_info *vreg_info,
				const char *vreg_name)
{
	int ret = 0;
	u32 voltage_levels[MAX_VOLTAGE_LEVEL];
	char prop_name[MAX_PROP_SIZE];
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		pr_err("%s: No vreg data found for %s\n", __func__, vreg_name);
		ret = -EINVAL;
		return ret;
	}

	vreg_info->name = vreg_name;

	snprintf(prop_name, MAX_PROP_SIZE,
		"qcom,%s-voltage-level", vreg_name);
	ret = of_property_read_u32_array(np, prop_name, voltage_levels,
					ARRAY_SIZE(voltage_levels));
	if (ret) {
		pr_err("%s: error reading %s property\n", __func__, prop_name);
		return ret;
	}

	vreg_info->nominal_min = voltage_levels[0];
	vreg_info->max_voltage = voltage_levels[1];

	return ret;
}

static int icnss_get_resources(struct device *dev)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info;

	vreg_info = &penv->vreg_info;
	if (vreg_info->reg) {
		pr_err("%s: %s regulator is already initialized\n", __func__,
			vreg_info->name);
		return ret;
	}

	vreg_info->reg = devm_regulator_get(dev, vreg_info->name);
	if (IS_ERR(vreg_info->reg)) {
		ret = PTR_ERR(vreg_info->reg);
		if (ret == -EPROBE_DEFER) {
			pr_err("%s: %s probe deferred!\n", __func__,
				vreg_info->name);
		} else {
			pr_err("%s: Get %s failed!\n", __func__,
				vreg_info->name);
		}
	}
	return ret;
}

static int icnss_release_resources(void)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info = &penv->vreg_info;

	if (!vreg_info->reg) {
		pr_err("%s: regulator is not initialized\n", __func__);
		return -ENOENT;
	}

	devm_regulator_put(vreg_info->reg);
	return ret;
}

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	int i;
	struct device *dev = &pdev->dev;

	if (penv) {
		pr_err("%s: penv is already initialized\n", __func__);
		return -EEXIST;
	}

	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;

	penv->pdev = pdev;

	ret = icnss_dt_parse_vreg_info(dev, &penv->vreg_info, "vdd-io");
	if (ret < 0) {
		pr_err("%s: failed parsing vdd io data\n", __func__);
		goto out;
	}

	ret = icnss_get_resources(dev);
	if (ret < 0) {
		pr_err("%s: Regulator setup failed (%d)\n", __func__, ret);
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "membase");
	if (!res) {
		pr_err("%s: Memory base not found\n", __func__);
		ret = -EINVAL;
		goto release_regulator;
	}
	penv->mem_base_pa = res->start;
	penv->mem_base_va = ioremap(penv->mem_base_pa, resource_size(res));
	if (!penv->mem_base_va) {
		pr_err("%s: ioremap failed\n", __func__);
		ret = -EINVAL;
		goto release_regulator;
	}

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			pr_err("%s: Fail to get IRQ-%d\n", __func__, i);
			ret = -ENODEV;
			goto release_regulator;
		} else {
			penv->ce_irqs[i] = res->start;
		}
	}

	if (of_property_read_u32(dev->of_node, "qcom,wlan-msa-memory",
				 &penv->msa_mem_size) == 0) {
		if (penv->msa_mem_size) {
			penv->msa_va = dma_alloc_coherent(&pdev->dev,
							  penv->msa_mem_size,
							  &penv->msa_pa,
							  GFP_KERNEL);
			if (!penv->msa_va) {
				pr_err("%s: DMA alloc failed\n", __func__);
				ret = -EINVAL;
				goto release_regulator;
			}
			pr_debug("%s: MAS va: %p, MSA pa: %pa\n",
				 __func__, penv->msa_va, &penv->msa_pa);
		}
	} else {
		pr_err("%s: Fail to get MSA Memory Size\n", __func__);
		ret = -ENODEV;
		goto release_regulator;
	}

	penv->skip_qmi = of_property_read_bool(dev->of_node,
					       "qcom,skip-qmi");

	ret = device_create_file(dev, &dev_attr_icnss_wlan_mode);
	if (ret) {
		pr_err("%s: wlan_mode sys file creation failed\n",
		       __func__);
		goto err_wlan_mode;
	}

	spin_lock_init(&penv->qmi_event_lock);

	penv->qmi_event_wq = alloc_workqueue("icnss_qmi_event", 0, 0);
	if (!penv->qmi_event_wq) {
		pr_err("%s: workqueue creation failed\n", __func__);
		ret = -EFAULT;
		goto err_workqueue;
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
		goto err_qmi;
	}

	pr_debug("icnss: Platform driver probed successfully\n");

	return ret;

err_qmi:
	if (penv->qmi_event_wq)
		destroy_workqueue(penv->qmi_event_wq);
err_workqueue:
	device_remove_file(&pdev->dev, &dev_attr_icnss_wlan_mode);
err_wlan_mode:
	if (penv->msa_va)
		dma_free_coherent(&pdev->dev, penv->msa_mem_size,
				  penv->msa_va, penv->msa_pa);
release_regulator:
	ret = icnss_release_resources();
	if (ret < 0)
		pr_err("%s: fail to release the platform resource\n",
			 __func__);
out:
	devm_kfree(&pdev->dev, penv);
	penv = NULL;
	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	int ret = 0;

	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &wlfw_clnt_nb);
	if (penv->qmi_event_wq)
		destroy_workqueue(penv->qmi_event_wq);
	device_remove_file(&pdev->dev, &dev_attr_icnss_wlan_mode);
	if (penv->msa_va)
		dma_free_coherent(&pdev->dev, penv->msa_mem_size,
				  penv->msa_va, penv->msa_pa);

	ret = icnss_vreg_set(VREG_OFF);
	if (ret < 0)
		pr_err("%s: Failed to turn off voltagre regulator: %d\n",
		       __func__, ret);

	ret = icnss_release_resources();
	if (ret < 0)
		pr_err("%s: fail to release the platform resource\n",
			 __func__);
	return ret;
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
