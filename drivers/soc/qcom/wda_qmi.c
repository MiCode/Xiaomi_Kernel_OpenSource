/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/rtnetlink.h>
#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/rmnet_qmi.h>
#define CREATE_TRACE_POINTS
#include <trace/events/wda.h>
#include "qmi_rmnet_i.h"

struct wda_qmi_data {
	void *rmnet_port;
	struct workqueue_struct *wda_wq;
	struct work_struct svc_arrive;
	struct qmi_handle handle;
	struct sockaddr_qrtr ssctl;
	struct svc_info svc;
	int restart_state;
};

static void wda_svc_config(struct work_struct *work);
/* **************************************************** */
#define WDA_SERVICE_ID_V01 0x1A
#define WDA_SERVICE_VERS_V01 0x01
#define WDA_TIMEOUT_JF  msecs_to_jiffies(1000)

#define QMI_WDA_SET_POWERSAVE_CONFIG_REQ_V01 0x002D
#define QMI_WDA_SET_POWERSAVE_CONFIG_RESP_V01 0x002D
#define QMI_WDA_SET_POWERSAVE_CONFIG_REQ_V01_MAX_MSG_LEN 18
#define QMI_WDA_SET_POWERSAVE_CONFIG_RESP_V01_MAX_MSG_LEN 14

#define QMI_WDA_SET_POWERSAVE_MODE_REQ_V01 0x002E
#define QMI_WDA_SET_POWERSAVE_MODE_RESP_V01 0x002E
#define QMI_WDA_SET_POWERSAVE_MODE_REQ_V01_MAX_MSG_LEN 4
#define QMI_WDA_SET_POWERSAVE_MODE_RESP_V01_MAX_MSG_LEN 7

enum wda_powersave_config_mask_enum_v01 {
	WDA_DATA_POWERSAVE_CONFIG_MASK_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	WDA_DATA_POWERSAVE_CONFIG_NOT_SUPPORTED = 0x00,
	WDA_DATA_POWERSAVE_CONFIG_DL_MARKER_V01 = 0x01,
	WDA_DATA_POWERSAVE_CONFIG_FLOW_CTL_V01 = 0x02,
	WDA_DATA_POWERSAVE_CONFIG_ALL_MASK_V01 = 0x7FFFFFFF,
	WDA_DATA_POWERSAVE_CONFIG_MASK_ENUM_MAX_ENUM_VAL_V01 = 2147483647
};

struct wda_set_powersave_config_req_msg_v01 {
	/* Mandatory */
	struct data_ep_id_type_v01 ep_id;
	/* Optional */
	uint8_t req_data_cfg_valid;
	enum wda_powersave_config_mask_enum_v01 req_data_cfg;
};

struct wda_set_powersave_config_resp_msg_v01 {
	/* Mandatory */
	struct qmi_response_type_v01 resp;
	/* Optional */
	uint8_t data_cfg_valid;
	enum wda_powersave_config_mask_enum_v01 data_cfg;
};

struct wda_set_powersave_mode_req_msg_v01 {
	/* Mandatory */
	uint8_t powersave_control_flag;
};

struct wda_set_powersave_mode_resp_msg_v01 {
	/* Mandatory */
	struct qmi_response_type_v01 resp;
};

static struct qmi_elem_info wda_set_powersave_config_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct data_ep_id_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
				wda_set_powersave_config_req_msg_v01,
				ep_id),
		.ei_array	= data_ep_id_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
				wda_set_powersave_config_req_msg_v01,
				req_data_cfg_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum
					 wda_powersave_config_mask_enum_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
				wda_set_powersave_config_req_msg_v01,
				req_data_cfg),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wda_set_powersave_config_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					wda_set_powersave_config_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					wda_set_powersave_config_resp_msg_v01,
					   data_cfg_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum
					 wda_powersave_config_mask_enum_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					wda_set_powersave_config_resp_msg_v01,
					   data_cfg),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wda_set_powersave_mode_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   wda_set_powersave_mode_req_msg_v01,
					   powersave_control_flag),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wda_set_powersave_mode_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   wda_set_powersave_mode_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static int wda_set_powersave_mode_req(void *wda_data, uint8_t enable)
{
	struct wda_qmi_data *data = (struct wda_qmi_data *)wda_data;
	struct wda_set_powersave_mode_resp_msg_v01 *resp;
	struct wda_set_powersave_mode_req_msg_v01  *req;
	struct qmi_txn txn;
	int ret;

	if (!data || !data->rmnet_port)
		return -EINVAL;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_ATOMIC);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = qmi_txn_init(&data->handle, &txn,
			   wda_set_powersave_mode_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("%s() Failed init for response, err: %d\n",
			__func__, ret);
		goto out;
	}

	req->powersave_control_flag = enable;
	ret = qmi_send_request(&data->handle, &data->ssctl, &txn,
			QMI_WDA_SET_POWERSAVE_MODE_REQ_V01,
			QMI_WDA_SET_POWERSAVE_MODE_REQ_V01_MAX_MSG_LEN,
			wda_set_powersave_mode_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("%s() Failed sending request, err: %d\n",
			__func__, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, WDA_TIMEOUT_JF);
	if (ret < 0) {
		pr_err("%s() Response waiting failed, err: %d\n",
			__func__, ret);
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s() Request rejected, result: %d, err: %d\n",
			__func__, resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
	}

out:
	kfree(resp);
	kfree(req);
	return ret;
}

static int wda_set_powersave_config_req(struct qmi_handle *wda_handle)
{
	struct wda_qmi_data *data = container_of(wda_handle,
						 struct wda_qmi_data, handle);
	struct wda_set_powersave_config_resp_msg_v01 *resp;
	struct wda_set_powersave_config_req_msg_v01  *req;
	struct qmi_txn txn;
	int ret;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_ATOMIC);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	ret = qmi_txn_init(wda_handle, &txn,
			   wda_set_powersave_config_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("%s() Failed init for response, err: %d\n",
			__func__, ret);
		goto out;
	}

	req->ep_id.ep_type = data->svc.ep_type;
	req->ep_id.iface_id = data->svc.iface_id;
	req->req_data_cfg_valid = 1;
	req->req_data_cfg = WDA_DATA_POWERSAVE_CONFIG_ALL_MASK_V01;
	ret = qmi_send_request(wda_handle, &data->ssctl, &txn,
			QMI_WDA_SET_POWERSAVE_CONFIG_REQ_V01,
			QMI_WDA_SET_POWERSAVE_CONFIG_REQ_V01_MAX_MSG_LEN,
			wda_set_powersave_config_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("%s() Failed sending request, err: %d\n", __func__, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, WDA_TIMEOUT_JF);
	if (ret < 0) {
		pr_err("%s() Response waiting failed, err: %d\n",
			__func__, ret);
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s() Request rejected, result: %d, error: %d\n",
			__func__, resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
	}

out:
	kfree(resp);
	kfree(req);
	return ret;
}

static void wda_svc_config(struct work_struct *work)
{
	struct wda_qmi_data *data = container_of(work, struct wda_qmi_data,
						 svc_arrive);
	struct qmi_info *qmi;
	int rc;

	if (data->restart_state == 1)
		return;
	rc = wda_set_powersave_config_req(&data->handle);
	if (rc < 0) {
		pr_err("%s Failed to init service, err[%d]\n", __func__, rc);
		return;
	}

	if (data->restart_state == 1)
		return;
	while (!rtnl_trylock()) {
		if (!data->restart_state)
			cond_resched();
		else
			return;
	}
	qmi = (struct qmi_info *)rmnet_get_qmi_pt(data->rmnet_port);
	if (!qmi) {
		rtnl_unlock();
		return;
	}

	qmi->wda_pending = NULL;
	qmi->wda_client = (void *)data;
	trace_wda_client_state_up(data->svc.instance,
				  data->svc.ep_type,
				  data->svc.iface_id);

	rtnl_unlock();

	pr_info("Connection established with the WDA Service\n");
}

static int wda_svc_arrive(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct wda_qmi_data *data = container_of(qmi, struct wda_qmi_data,
						 handle);

	data->ssctl.sq_family = AF_QIPCRTR;
	data->ssctl.sq_node = svc->node;
	data->ssctl.sq_port = svc->port;

	queue_work(data->wda_wq, &data->svc_arrive);

	return 0;
}

static void wda_svc_exit(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct wda_qmi_data *data = container_of(qmi, struct wda_qmi_data,
						 handle);

	if (!data)
		pr_info("%s() data is null\n", __func__);
}

static struct qmi_ops server_ops = {
	.new_server = wda_svc_arrive,
	.del_server = wda_svc_exit,
};

int
wda_qmi_client_init(void *port, struct svc_info *psvc, struct qmi_info *qmi)
{
	struct wda_qmi_data *data;
	int rc = -ENOMEM;

	if (!port || !qmi)
		return -EINVAL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->wda_wq = create_singlethread_workqueue("wda_wq");
	if (!data->wda_wq) {
		pr_err("%s Could not create workqueue\n", __func__);
		goto err0;
	}

	data->rmnet_port = port;
	data->restart_state = 0;
	memcpy(&data->svc, psvc, sizeof(data->svc));
	INIT_WORK(&data->svc_arrive, wda_svc_config);

	rc = qmi_handle_init(&data->handle,
			     QMI_WDA_SET_POWERSAVE_CONFIG_RESP_V01_MAX_MSG_LEN,
			     &server_ops, NULL);
	if (rc < 0) {
		pr_err("%s: Failed qmi_handle_init, err: %d\n", __func__, rc);
		goto err1;
	}

	rc = qmi_add_lookup(&data->handle, WDA_SERVICE_ID_V01,
			    WDA_SERVICE_VERS_V01, psvc->instance);
	if (rc < 0) {
		pr_err("%s(): Failed qmi_add_lookup, err: %d\n", __func__, rc);
		goto err2;
	}

	qmi->wda_pending = (void *)data;
	return 0;

err2:
	qmi_handle_release(&data->handle);
err1:
	destroy_workqueue(data->wda_wq);
err0:
	kfree(data);
	return rc;
}

void wda_qmi_client_exit(void *wda_data)
{
	struct wda_qmi_data *data = (struct wda_qmi_data *)wda_data;

	if (!data) {
		pr_info("%s() data is null\n", __func__);
		return;
	}

	data->restart_state = 1;
	trace_wda_client_state_down(0);
	qmi_handle_release(&data->handle);
	destroy_workqueue(data->wda_wq);
	kfree(data);
}

int wda_set_powersave_mode(void *wda_data, uint8_t enable)
{
	trace_wda_set_powersave_mode(enable);
	return wda_set_powersave_mode_req(wda_data, enable);
}
