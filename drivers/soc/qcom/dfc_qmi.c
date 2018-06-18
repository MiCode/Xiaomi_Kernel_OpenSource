/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/rtnetlink.h>
#include <net/pkt_sched.h>
#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/rmnet_qmi.h>
#include <linux/ip.h>
#include "qmi_rmnet_i.h"

#define DFC_MAX_BEARERS_V01 16
#define DFC_MAX_QOS_ID_V01 2
#define DEFAULT_FLOW_ID 0

/* bearer list update result */
#define NO_BEARER 0
#define NO_CHANGE 1
#define UPDATED 2

struct qmap_header {
	uint8_t  pad_len:6;
	uint8_t  reserved_bit:1;
	uint8_t  cd_bit:1;
	uint8_t  mux_id;
	__be16   pkt_len;
} __aligned(1);

struct dfc_ack_cmd {
	struct qmap_header header;
	uint8_t  command_name;
	uint8_t  cmd_type:2;
	uint8_t  reserved:6;
	uint16_t reserved2;
	uint32_t transaction_id;
	uint8_t  qos_ver:2;
	uint8_t  reserved3:6;
	uint8_t  qos_type:2;
	uint8_t  reserved4:6;
	uint16_t dfc_seq;
	uint8_t  reserved5[3];
	uint8_t  bearer_id;
} __aligned(1);

struct dfc_qos_ids {
	uint32_t qos_id_valid;
	uint32_t qos_id;
};

struct dfc_work {
	struct work_struct work;
	struct net_device *dev;
	uint8_t bearer_id;
	uint8_t  ack_req;
	uint16_t seq;
	uint8_t mux_id;
};

struct dfc_qmi_data {
	void *rmnet_port;
	struct workqueue_struct *dfc_wq;
	struct work_struct svc_arrive;
	struct qmi_handle handle;
	struct sockaddr_qrtr ssctl;
	int modem;
};

struct dfc_svc_ind {
	struct work_struct work;
	struct dfc_qmi_data *data;
	void *dfc_info;
};

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
struct dfc_ind_reg {
	struct work_struct work;
	struct dfc_qmi_data *data;
	int reg;
};
static void dfc_ind_reg_dereg(struct work_struct *work);
#endif

static void dfc_svc_init(struct work_struct *work);
static void dfc_do_burst_flow_control(struct work_struct *work);
static void dfc_disable_flow(struct work_struct *work);

/* **************************************************** */
#define DFC_SERVICE_ID_V01 0x4E
#define DFC_SERVICE_VERS_V01 0x01
#define DFC_TIMEOUT_MS 10000

#define QMI_DFC_BIND_CLIENT_REQ_V01 0x0020
#define QMI_DFC_BIND_CLIENT_RESP_V01 0x0020
#define QMI_DFC_BIND_CLIENT_REQ_V01_MAX_MSG_LEN  11
#define QMI_DFC_BIND_CLIENT_RESP_V01_MAX_MSG_LEN 7

#define QMI_DFC_INDICATION_REGISTER_REQ_V01 0x0001
#define QMI_DFC_INDICATION_REGISTER_RESP_V01 0x0001
#define QMI_DFC_INDICATION_REGISTER_REQ_V01_MAX_MSG_LEN 4
#define QMI_DFC_INDICATION_REGISTER_RESP_V01_MAX_MSG_LEN 7

#define QMI_DFC_FLOW_STATUS_IND_V01 0x0022
#define QMI_DFC_FLOW_STATUS_IND_V01_MAX_MSG_LEN 471

struct dfc_bind_client_req_msg_v01 {
	uint8_t ep_id_valid;
	struct data_ep_id_type_v01 ep_id;
};

struct dfc_bind_client_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct dfc_indication_register_req_msg_v01 {
	uint8_t report_flow_status_valid;
	uint8_t report_flow_status;
};

struct dfc_indication_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

enum dfc_ip_type_enum_v01 {
	DFC_IP_TYPE_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	DFC_IPV4_TYPE_V01 = 0x4,
	DFC_IPV6_TYPE_V01 = 0x6,
	DFC_IP_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
};

struct dfc_qos_id_type_v01 {
	uint32_t qos_id;
	enum dfc_ip_type_enum_v01 ip_type;
};

struct dfc_flow_status_info_type_v01 {
	uint8_t subs_id;
	uint8_t mux_id;
	uint8_t bearer_id;
	uint32_t num_bytes;
	uint16_t seq_num;
	uint8_t qos_ids_len;
	struct dfc_qos_id_type_v01 qos_ids[DFC_MAX_QOS_ID_V01];
};

static struct qmi_elem_info dfc_qos_id_type_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct dfc_qos_id_type_v01,
					   qos_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum dfc_ip_type_enum_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct dfc_qos_id_type_v01,
					   ip_type),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_flow_status_info_type_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   subs_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   mux_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   bearer_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   num_bytes),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   seq_num),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   qos_ids_len),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= DFC_MAX_QOS_ID_V01,
		.elem_size	= sizeof(struct dfc_qos_id_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   qos_ids),
		.ei_array	= dfc_qos_id_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

/**
 * Indication Message; Gives the flow status to control points
 * that have registered for this event reporting.
 */
struct dfc_flow_status_ind_msg_v01 {
	uint8_t	flow_status_valid;
	uint8_t	flow_status_len;
	struct	dfc_flow_status_info_type_v01 flow_status[DFC_MAX_BEARERS_V01];
	uint8_t	eod_ack_reqd_valid;
	uint8_t	eod_ack_reqd;
};

static struct qmi_elem_info dfc_bind_client_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct dfc_bind_client_req_msg_v01,
					   ep_id_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct data_ep_id_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct dfc_bind_client_req_msg_v01,
					   ep_id),
		.ei_array	= data_ep_id_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_bind_client_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct dfc_bind_client_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_indication_register_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_indication_register_req_msg_v01,
					   report_flow_status_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_indication_register_req_msg_v01,
					   report_flow_status),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_indication_register_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   dfc_indication_register_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_flow_status_ind_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   flow_status_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   flow_status_len),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= DFC_MAX_BEARERS_V01,
		.elem_size	= sizeof(struct
					 dfc_flow_status_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   flow_status),
		.ei_array	= dfc_flow_status_info_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   eod_ack_reqd_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   eod_ack_reqd),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static int
dfc_bind_client_req(struct qmi_handle *dfc_handle,
		    struct sockaddr_qrtr *ssctl, struct svc_info *svc)
{
	struct dfc_bind_client_resp_msg_v01 *resp;
	struct dfc_bind_client_req_msg_v01 *req;
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
	/* Prepare req and response message */
	req->ep_id_valid = 1;
	req->ep_id.ep_type = svc->ep_type;
	req->ep_id.iface_id = svc->iface_id;

	ret = qmi_txn_init(dfc_handle, &txn,
			   dfc_bind_client_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("Fail to init txn for bind client resp %d\n", ret);
		goto out;
	}

	ret = qmi_send_request(dfc_handle, ssctl, &txn,
			       QMI_DFC_BIND_CLIENT_REQ_V01,
			       QMI_DFC_BIND_CLIENT_REQ_V01_MAX_MSG_LEN,
			       dfc_bind_client_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("Fail to send bind client req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, DFC_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("bind client resp wait failed ret %d\n", ret);
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI bind client request rejected, result:%d err:%d\n",
			resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
	}

out:
	kfree(resp);
	kfree(req);
	return ret;
}

static int
dfc_indication_register_req(struct qmi_handle *dfc_handle,
			    struct sockaddr_qrtr *ssctl, uint8_t reg)
{
	struct dfc_indication_register_resp_msg_v01 *resp;
	struct dfc_indication_register_req_msg_v01 *req;
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
	/* Prepare req and response message */
	req->report_flow_status_valid = 1;
	req->report_flow_status = reg;

	ret = qmi_txn_init(dfc_handle, &txn,
			   dfc_indication_register_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("%s() Failed init txn for resp %d\n", __func__, ret);
		goto out;
	}

	ret = qmi_send_request(dfc_handle, ssctl, &txn,
			       QMI_DFC_INDICATION_REGISTER_REQ_V01,
			       QMI_DFC_INDICATION_REGISTER_REQ_V01_MAX_MSG_LEN,
			       dfc_indication_register_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("%s() Fail to send indication register req %d\n",
		       __func__, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, DFC_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s() resp wait failed ret:%d\n", __func__, ret);
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s() rejected, result:%d error:%d\n",
		       __func__, resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
	}

out:
	kfree(resp);
	kfree(req);
	return ret;
}

static int dfc_init_service(struct dfc_qmi_data *data, struct qmi_info *qmi)
{
	int rc;

	rc = dfc_bind_client_req(&data->handle, &data->ssctl,
				 &qmi->fc_info[data->modem].svc);
	if (rc < 0)
		return rc;

	return dfc_indication_register_req(&data->handle, &data->ssctl, 1);
}

static int dfc_disable_bearer_flows(struct net_device *dev, uint8_t bearer_id)
{
	struct qos_info *qos = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct list_head *p;
	struct rmnet_flow_map *itm;
	int rc = 0;

	if (!qos)
		return 0;

	list_for_each(p, &qos->flow_head) {
		itm = list_entry(p, struct rmnet_flow_map, list);

		if (unlikely(!itm))
			return 0;

		if (itm->bearer_id == bearer_id) {
			rtnl_lock();
			tc_qdisc_flow_control(dev, itm->tcm_handle, 0);
			rtnl_unlock();
			rc++;
		}
	}
	return rc;
}

static int dfc_update_fc_map(struct qos_info *qos, uint8_t ack_req,
			     struct dfc_flow_status_info_type_v01 *fc_info)
{
	struct rmnet_bearer_map *itm = NULL;
	unsigned long flags;
	int rc = NO_BEARER;

	write_lock_irqsave(&qos->flow_map_lock, flags);
	itm = qmi_rmnet_get_bearer_map(qos, fc_info->bearer_id);
	if (itm) {
		if ((itm->grant_size == fc_info->num_bytes) &&
		    (itm->counter > 0)) {
			/*flow is enabled and grant_size is the same*/
			rc = NO_CHANGE;
		} else {
			itm->grant_size = fc_info->num_bytes;
			itm->seq = fc_info->seq_num;
			itm->ack_req = ack_req;
			rc = UPDATED;
		}
		itm->counter = 0;
	}
	write_unlock_irqrestore(&qos->flow_map_lock, flags);
	return rc;
}

static int dfc_do_fc(struct net_device *dev, uint32_t flow_id,
		     int ip_type, int enable)
{
	struct qos_info *qos = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map *itm = NULL;
	int len = 0;

	if (!qos)
		return 0;

	itm = qmi_rmnet_get_flow_map(qos, flow_id, ip_type);
	if (itm) {
		rtnl_lock();
		len = tc_qdisc_flow_control(dev, itm->tcm_handle, enable);
		rtnl_unlock();
	}
	return len;
}

static void dfc_disable_flow(struct work_struct *work)
{
	struct dfc_work *data = (struct dfc_work *)work;
	int rc = dfc_disable_bearer_flows(data->dev, data->bearer_id);

	pr_debug("%s() %d flows disabled\n", __func__, rc);
	kfree(data);
}

static void dfc_do_flow_controls(struct net_device *dev,
				 struct dfc_flow_status_info_type_v01 *flow)
{
	int i;
	int enable = (flow->num_bytes > 0) ? 1 : 0;
	int qdisc_len;

	for (i = 0; i < flow->qos_ids_len; i++) {
		/* do flow control per specified flow */
		if (flow->qos_ids[i].ip_type == DFC_IPV4_TYPE_V01) {
			qdisc_len = dfc_do_fc(dev, flow->qos_ids[i].qos_id,
					      AF_INET, enable);
			pr_debug("%s() qdisc_len=%d\n", __func__, qdisc_len);
		} else if (flow->qos_ids[i].ip_type == DFC_IPV6_TYPE_V01) {
			qdisc_len = dfc_do_fc(dev, flow->qos_ids[i].qos_id,
					      AF_INET6, enable);
		} else {
			pr_err("%s() ip type[%d] not supported\n",
				__func__, flow->qos_ids[i].ip_type);
		}
	}
}

static void dfc_do_burst_flow_control(struct work_struct *work)
{
	struct dfc_svc_ind *svc_ind = (struct dfc_svc_ind *)work;
	struct dfc_flow_status_ind_msg_v01 *ind =
		(struct dfc_flow_status_ind_msg_v01 *)svc_ind->dfc_info;
	struct net_device *dev;
	struct qos_info *qos;
	struct dfc_flow_status_info_type_v01 *flow_status;
	uint8_t ack_req = ind->eod_ack_reqd_valid ? ind->eod_ack_reqd : 0;
	int i, rc;

	for (i = 0; i < ind->flow_status_len; i++) {
		flow_status = &ind->flow_status[i];
		dev = rmnet_get_rmnet_dev(svc_ind->data->rmnet_port,
					  flow_status->mux_id);
		if (!dev)
			goto clean_out;

		qos = (struct qos_info *)rmnet_get_qos_pt(dev);
		if (!qos)
			continue;

		rc = dfc_update_fc_map(qos, ack_req, flow_status);
		if (rc == NO_BEARER) {
			pr_debug("%s: num_bytes[%u]\n",
				 __func__, flow_status->num_bytes);
			qos->default_grant = flow_status->num_bytes;
			continue;
		} else if (rc == NO_CHANGE) {
			continue;
		} else {
			if ((flow_status->num_bytes > 0) ||
			    (flow_status->bearer_id != 0xFF))
				dfc_do_flow_controls(dev, flow_status);
			else
				netif_stop_queue(dev);

		}
	}

clean_out:
	kfree(ind);
	kfree(svc_ind);
}

static void dfc_clnt_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			    struct qmi_txn *txn, const void *data)
{
	struct dfc_qmi_data *dfc = container_of(qmi, struct dfc_qmi_data,
						handle);
	struct dfc_flow_status_ind_msg_v01 *ind_msg;
	struct dfc_svc_ind *svc_ind;

	if (qmi != &dfc->handle) {
		pr_err("Wrong client\n");
		return;
	}

	ind_msg = (struct dfc_flow_status_ind_msg_v01 *)data;
	if (ind_msg->flow_status_valid) {
		if (ind_msg->flow_status_len > DFC_MAX_BEARERS_V01) {
			pr_err("Invalid fc info len: %d\n",
			       ind_msg->flow_status_len);
			return;
		}

		svc_ind = kmalloc(sizeof(struct dfc_svc_ind), GFP_ATOMIC);
		if (!svc_ind)
			return;

		INIT_WORK((struct work_struct *)svc_ind,
			  dfc_do_burst_flow_control);
		svc_ind->dfc_info = kmalloc(sizeof(*ind_msg), GFP_ATOMIC);
		if (!svc_ind->dfc_info) {
			kfree(svc_ind);
			return;
		}

		memcpy(svc_ind->dfc_info, ind_msg, sizeof(*ind_msg));
		svc_ind->data = dfc;
		queue_work(dfc->dfc_wq, (struct work_struct *)svc_ind);
	}
}

static void dfc_svc_init(struct work_struct *work)
{
	int rc = 0;
	struct dfc_qmi_data *data = container_of(work, struct dfc_qmi_data,
						 svc_arrive);
	struct qmi_info *qmi;

	qmi = (struct qmi_info *)rmnet_get_qmi_pt(data->rmnet_port);
	if (!qmi) {
		qmi_handle_release(&data->handle);
		return;
	}

	/* Request indication from modem service */
	rc = dfc_init_service(data, qmi);
	if (rc < 0) {
		qmi_handle_release(&data->handle);
		return;
	}

	qmi->fc_info[data->modem].dfc_client = (void *)data;
	pr_debug("Connection established with the DFC Service\n");
}

static int dfc_svc_arrive(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct dfc_qmi_data *data = container_of(qmi, struct dfc_qmi_data,
						 handle);

	data->ssctl.sq_family = AF_QIPCRTR;
	data->ssctl.sq_node = svc->node;
	data->ssctl.sq_port = svc->port;

	queue_work(data->dfc_wq, &data->svc_arrive);

	return 0;
}

static void dfc_svc_exit(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct dfc_qmi_data *data = container_of(qmi, struct dfc_qmi_data,
						 handle);
	struct qmi_info *qmi_pt;
	int modem;

	pr_debug("Connection with DFC service lost\n");
	qmi_pt = (struct qmi_info *)rmnet_get_qmi_pt(data->rmnet_port);
	if (qmi_pt) {
		for (modem = 0; modem < 2; modem++) {
			if (qmi_pt->fc_info[modem].dfc_client == (void *)data)
				qmi_pt->fc_info[modem].dfc_client = NULL;
			break;
		}
	}
	destroy_workqueue(data->dfc_wq);
	kfree(data);
}

static struct qmi_ops server_ops = {
	.new_server = dfc_svc_arrive,
	.del_server = dfc_svc_exit,
};

static struct qmi_msg_handler qmi_indication_handler[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_DFC_FLOW_STATUS_IND_V01,
		.ei = dfc_flow_status_ind_v01_ei,
		.decoded_size = QMI_DFC_FLOW_STATUS_IND_V01_MAX_MSG_LEN,
		.fn = dfc_clnt_ind_cb,
	},
	{},
};

/* **************************************************** */
int dfc_qmi_client_init(void *port, int modem)
{
	struct qmi_info *qmi = rmnet_get_qmi_pt(port);
	struct dfc_qmi_data *data;
	int rc = 0;

	if (!qmi)
		return -EINVAL;

	data = kmalloc(sizeof(struct dfc_qmi_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(struct dfc_qmi_data));
	data->rmnet_port = port;
	data->modem = modem;

	data->dfc_wq = create_singlethread_workqueue("dfc_wq");
	if (!data->dfc_wq) {
		pr_err("%s Could not create workqueue\n", __func__);
		kfree(data);
		return -ENOMEM;
	}
	INIT_WORK(&data->svc_arrive, dfc_svc_init);
	rc = qmi_handle_init(&data->handle,
			     QMI_DFC_FLOW_STATUS_IND_V01_MAX_MSG_LEN,
			     &server_ops, qmi_indication_handler);
	if (rc < 0) {
		pr_err("%s: failed qmi_handle_init - rc[%d]\n", __func__, rc);
		kfree(data);
		return rc;
	}

	rc = qmi_add_lookup(&data->handle, DFC_SERVICE_ID_V01,
			    DFC_SERVICE_VERS_V01,
			    qmi->fc_info[modem].svc.instance);
	if (rc < 0) {
		pr_err("%s: failed qmi_add_lookup - rc[%d]\n", __func__, rc);
		qmi_handle_release(&data->handle);
	}

	return rc;
}

void dfc_qmi_client_exit(void *dfc_data)
{
	struct dfc_qmi_data *data = (struct dfc_qmi_data *)dfc_data;

	if (!data)
		return;

	qmi_handle_release(&data->handle);
}

void dfc_qmi_burst_check(struct net_device *dev, struct qos_info *qos,
			 struct sk_buff *skb)
{
	struct dfc_work *svc_check;
	struct rmnet_bearer_map *bearer;
	struct rmnet_flow_map *itm;
	unsigned long flags;
	int ip_type;

	if (!qos || !skb)
		return;

	ip_type = (ip_hdr(skb)->version == IP_VER_6) ? AF_INET6 : AF_INET;
	write_lock_irqsave(&qos->flow_map_lock, flags);
	itm = qmi_rmnet_get_flow_map(qos, skb->mark, ip_type);

	if (!itm) {
		write_unlock_irqrestore(&qos->flow_map_lock, flags);
	} else {
		bearer = qmi_rmnet_get_bearer_map(qos, itm->bearer_id);
		if (unlikely(!bearer)) {
			write_unlock_irqrestore(&qos->flow_map_lock, flags);
			return;
		}
		bearer->counter += skb->len;
		if (bearer->counter < bearer->grant_size) {
			write_unlock_irqrestore(&qos->flow_map_lock, flags);
		} else {
			bearer->counter = 0;
			write_unlock_irqrestore(&qos->flow_map_lock, flags);

			svc_check = kmalloc(sizeof(struct dfc_work),
					    GFP_ATOMIC);
			if (!svc_check)
				return;

			INIT_WORK((struct work_struct *)svc_check,
				  dfc_disable_flow);
			svc_check->dev = dev;
			svc_check->bearer_id = bearer->bearer_id;
			svc_check->ack_req = bearer->ack_req;
			svc_check->seq = bearer->seq;
			svc_check->mux_id = qos->mux_id;
			schedule_work((struct work_struct *)svc_check);
		}
	}
}

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
static void dfc_ind_reg_dereg(struct work_struct *work)
{
	struct dfc_ind_reg *ind_reg = (struct dfc_ind_reg *)work;

	dfc_indication_register_req(&ind_reg->data->handle,
				    &ind_reg->data->ssctl, ind_reg->reg);
	kfree(ind_reg);
}

int dfc_reg_unreg_fc_ind(void *dfc_data, int reg)
{
	struct dfc_qmi_data *data = (struct dfc_qmi_data *)dfc_data;
	struct dfc_ind_reg *ind_reg;

	if (!data)
		return -EINVAL;

	ind_reg = kmalloc(sizeof(struct dfc_ind_reg), GFP_ATOMIC);
	if (!ind_reg)
		return -ENOMEM;

	INIT_WORK((struct work_struct *)ind_reg, dfc_ind_reg_dereg);
	ind_reg->data = data;
	ind_reg->reg = reg;
	schedule_work((struct work_struct *)ind_reg);
	return 0;
}
#endif /*defed CONFIG_QCOM_QMI_POWER_COLLAPSE*/
