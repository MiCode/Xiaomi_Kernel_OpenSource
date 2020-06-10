// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <net/pkt_sched.h>
#include <soc/qcom/rmnet_qmi.h>
#include <soc/qcom/qmi_rmnet.h>
#include "dfc_defs.h"

#define CREATE_TRACE_POINTS
#include <trace/events/dfc.h>

struct dfc_qmap_header {
	u8  pad_len:6;
	u8  reserved_bit:1;
	u8  cd_bit:1;
	u8  mux_id;
	__be16   pkt_len;
} __aligned(1);

struct dfc_ack_cmd {
	struct dfc_qmap_header header;
	u8  command_name;
	u8  cmd_type:2;
	u8  reserved:6;
	u16 reserved2;
	u32 transaction_id;
	u8  ver:2;
	u8  reserved3:6;
	u8  type:2;
	u8  reserved4:6;
	u16 dfc_seq;
	u8  reserved5[3];
	u8  bearer_id;
} __aligned(1);

static void dfc_svc_init(struct work_struct *work);

/* **************************************************** */
#define DFC_SERVICE_ID_V01 0x4E
#define DFC_SERVICE_VERS_V01 0x01
#define DFC_TIMEOUT_JF msecs_to_jiffies(1000)

#define QMI_DFC_BIND_CLIENT_REQ_V01 0x0020
#define QMI_DFC_BIND_CLIENT_RESP_V01 0x0020
#define QMI_DFC_BIND_CLIENT_REQ_V01_MAX_MSG_LEN  11
#define QMI_DFC_BIND_CLIENT_RESP_V01_MAX_MSG_LEN 7

#define QMI_DFC_INDICATION_REGISTER_REQ_V01 0x0001
#define QMI_DFC_INDICATION_REGISTER_RESP_V01 0x0001
#define QMI_DFC_INDICATION_REGISTER_REQ_V01_MAX_MSG_LEN 8
#define QMI_DFC_INDICATION_REGISTER_RESP_V01_MAX_MSG_LEN 7

#define QMI_DFC_FLOW_STATUS_IND_V01 0x0022
#define QMI_DFC_TX_LINK_STATUS_IND_V01 0x0024

#define QMI_DFC_GET_FLOW_STATUS_REQ_V01 0x0023
#define QMI_DFC_GET_FLOW_STATUS_RESP_V01 0x0023
#define QMI_DFC_GET_FLOW_STATUS_REQ_V01_MAX_MSG_LEN 20
#define QMI_DFC_GET_FLOW_STATUS_RESP_V01_MAX_MSG_LEN 543

struct dfc_bind_client_req_msg_v01 {
	u8 ep_id_valid;
	struct data_ep_id_type_v01 ep_id;
};

struct dfc_bind_client_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct dfc_indication_register_req_msg_v01 {
	u8 report_flow_status_valid;
	u8 report_flow_status;
	u8 report_tx_link_status_valid;
	u8 report_tx_link_status;
};

struct dfc_indication_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

static struct qmi_elem_info dfc_qos_id_type_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct dfc_qos_id_type_v01,
					   qos_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum dfc_ip_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct dfc_qos_id_type_v01,
					   ip_type),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_flow_status_info_type_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   subs_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   mux_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   bearer_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   num_bytes),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   seq_num),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
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
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_info_type_v01,
					   qos_ids),
		.ei_array	= dfc_qos_id_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_ancillary_info_type_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_ancillary_info_type_v01,
					   subs_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_ancillary_info_type_v01,
					   mux_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_ancillary_info_type_v01,
					   bearer_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_ancillary_info_type_v01,
					   reserved),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct dfc_get_flow_status_req_msg_v01 {
	u8 bearer_id_list_valid;
	u8 bearer_id_list_len;
	u8 bearer_id_list[DFC_MAX_BEARERS_V01];
};

struct dfc_get_flow_status_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 flow_status_valid;
	u8 flow_status_len;
	struct dfc_flow_status_info_type_v01 flow_status[DFC_MAX_BEARERS_V01];
};

struct dfc_svc_ind {
	struct list_head list;
	u16 msg_id;
	union {
		struct dfc_flow_status_ind_msg_v01 dfc_info;
		struct dfc_tx_link_status_ind_msg_v01 tx_status;
	} d;
};

static struct qmi_elem_info dfc_bind_client_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct dfc_bind_client_req_msg_v01,
					   ep_id_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct data_ep_id_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct dfc_bind_client_req_msg_v01,
					   ep_id),
		.ei_array	= data_ep_id_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_bind_client_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct dfc_bind_client_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_indication_register_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_indication_register_req_msg_v01,
					   report_flow_status_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_indication_register_req_msg_v01,
					   report_flow_status),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct
					   dfc_indication_register_req_msg_v01,
					   report_tx_link_status_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct
					   dfc_indication_register_req_msg_v01,
					   report_tx_link_status),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_indication_register_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   dfc_indication_register_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_flow_status_ind_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   flow_status_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
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
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   flow_status),
		.ei_array	= dfc_flow_status_info_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   eod_ack_reqd_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   eod_ack_reqd),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   ancillary_info_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   ancillary_info_len),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= DFC_MAX_BEARERS_V01,
		.elem_size	= sizeof(struct
					 dfc_ancillary_info_type_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct
					   dfc_flow_status_ind_msg_v01,
					   ancillary_info),
		.ei_array	= dfc_ancillary_info_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_get_flow_status_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_get_flow_status_req_msg_v01,
					   bearer_id_list_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_get_flow_status_req_msg_v01,
					   bearer_id_list_len),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= DFC_MAX_BEARERS_V01,
		.elem_size	= sizeof(u8),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_get_flow_status_req_msg_v01,
					   bearer_id_list),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_get_flow_status_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   dfc_get_flow_status_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_get_flow_status_resp_msg_v01,
					   flow_status_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_get_flow_status_resp_msg_v01,
					   flow_status_len),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= DFC_MAX_BEARERS_V01,
		.elem_size	= sizeof(struct
					 dfc_flow_status_info_type_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_get_flow_status_resp_msg_v01,
					   flow_status),
		.ei_array	= dfc_flow_status_info_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_bearer_info_type_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_bearer_info_type_v01,
					   subs_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_bearer_info_type_v01,
					   mux_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_bearer_info_type_v01,
					   bearer_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum dfc_ip_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					   dfc_bearer_info_type_v01,
					   ip_type),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info dfc_tx_link_status_ind_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   dfc_tx_link_status_ind_msg_v01,
					   tx_status),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_tx_link_status_ind_msg_v01,
					   bearer_info_valid),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_tx_link_status_ind_msg_v01,
					   bearer_info_len),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= DFC_MAX_BEARERS_V01,
		.elem_size	= sizeof(struct
					 dfc_bearer_info_type_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   dfc_tx_link_status_ind_msg_v01,
					   bearer_info),
		.ei_array	= dfc_bearer_info_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
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

	ret = qmi_txn_init(dfc_handle, &txn,
			   dfc_bind_client_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("%s() Failed init for response, err: %d\n",
			__func__, ret);
		goto out;
	}

	req->ep_id_valid = 1;
	req->ep_id.ep_type = svc->ep_type;
	req->ep_id.iface_id = svc->iface_id;
	ret = qmi_send_request(dfc_handle, ssctl, &txn,
			       QMI_DFC_BIND_CLIENT_REQ_V01,
			       QMI_DFC_BIND_CLIENT_REQ_V01_MAX_MSG_LEN,
			       dfc_bind_client_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("%s() Failed sending request, err: %d\n",
			__func__, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, DFC_TIMEOUT_JF);
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

static int
dfc_indication_register_req(struct qmi_handle *dfc_handle,
			    struct sockaddr_qrtr *ssctl, u8 reg)
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

	ret = qmi_txn_init(dfc_handle, &txn,
			   dfc_indication_register_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("%s() Failed init for response, err: %d\n",
			__func__, ret);
		goto out;
	}

	req->report_flow_status_valid = 1;
	req->report_flow_status = reg;
	req->report_tx_link_status_valid = 1;
	req->report_tx_link_status = reg;

	ret = qmi_send_request(dfc_handle, ssctl, &txn,
			       QMI_DFC_INDICATION_REGISTER_REQ_V01,
			       QMI_DFC_INDICATION_REGISTER_REQ_V01_MAX_MSG_LEN,
			       dfc_indication_register_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("%s() Failed sending request, err: %d\n",
			__func__, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, DFC_TIMEOUT_JF);
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

static int
dfc_get_flow_status_req(struct qmi_handle *dfc_handle,
			struct sockaddr_qrtr *ssctl,
			struct dfc_get_flow_status_resp_msg_v01 *resp)
{
	struct dfc_get_flow_status_req_msg_v01 *req;
	struct qmi_txn *txn;
	int ret;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req)
		return -ENOMEM;

	txn = kzalloc(sizeof(*txn), GFP_ATOMIC);
	if (!txn) {
		kfree(req);
		return -ENOMEM;
	}

	ret = qmi_txn_init(dfc_handle, txn,
			   dfc_get_flow_status_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("%s() Failed init for response, err: %d\n",
			__func__, ret);
		goto out;
	}

	ret = qmi_send_request(dfc_handle, ssctl, txn,
			       QMI_DFC_GET_FLOW_STATUS_REQ_V01,
			       QMI_DFC_GET_FLOW_STATUS_REQ_V01_MAX_MSG_LEN,
			       dfc_get_flow_status_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(txn);
		pr_err("%s() Failed sending request, err: %d\n",
			__func__, ret);
		goto out;
	}

	ret = qmi_txn_wait(txn, DFC_TIMEOUT_JF);
	if (ret < 0) {
		pr_err("%s() Response waiting failed, err: %d\n",
			__func__, ret);
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s() Request rejected, result: %d, err: %d\n",
			__func__, resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
	}

out:
	kfree(txn);
	kfree(req);
	return ret;
}

static int dfc_init_service(struct dfc_qmi_data *data)
{
	int rc;

	rc = dfc_bind_client_req(&data->handle, &data->ssctl, &data->svc);
	if (rc < 0)
		return rc;

	return dfc_indication_register_req(&data->handle, &data->ssctl, 1);
}

static void
dfc_send_ack(struct net_device *dev, u8 bearer_id, u16 seq, u8 mux_id, u8 type)
{
	struct qos_info *qos = rmnet_get_qos_pt(dev);
	struct sk_buff *skb;
	struct dfc_ack_cmd *msg;
	int data_size = sizeof(struct dfc_ack_cmd);
	int header_size = sizeof(struct dfc_qmap_header);

	if (!qos)
		return;

	if (dfc_qmap) {
		dfc_qmap_send_ack(qos, bearer_id, seq, type);
		return;
	}

	skb = alloc_skb(data_size, GFP_ATOMIC);
	if (!skb)
		return;

	msg = (struct dfc_ack_cmd *)skb_put(skb, data_size);
	memset(msg, 0, data_size);

	msg->header.cd_bit = 1;
	msg->header.mux_id = mux_id;
	msg->header.pkt_len = htons(data_size - header_size);

	msg->bearer_id = bearer_id;
	msg->command_name = 4;
	msg->cmd_type = 0;
	msg->dfc_seq = htons(seq);
	msg->type = type;
	msg->ver = 2;
	msg->transaction_id = htonl(qos->tran_num);

	skb->dev = qos->real_dev;
	skb->protocol = htons(ETH_P_MAP);

	trace_dfc_qmap_cmd(mux_id, bearer_id, seq, type, qos->tran_num);
	qos->tran_num++;

	rmnet_map_tx_qmap_cmd(skb);
}

int dfc_bearer_flow_ctl(struct net_device *dev,
			struct rmnet_bearer_map *bearer,
			struct qos_info *qos)
{
	bool enable;

	enable = bearer->grant_size ? true : false;

	qmi_rmnet_flow_control(dev, bearer->mq_idx, enable);

	/* Do not flow disable tcp ack q in tcp bidir */
	if (bearer->ack_mq_idx != INVALID_MQ &&
	    (enable || !bearer->tcp_bidir))
		qmi_rmnet_flow_control(dev, bearer->ack_mq_idx, enable);

	if (!enable && bearer->ack_req)
		dfc_send_ack(dev, bearer->bearer_id,
			     bearer->seq, qos->mux_id,
			     DFC_ACK_TYPE_DISABLE);

	return 0;
}

static int dfc_all_bearer_flow_ctl(struct net_device *dev,
				struct qos_info *qos, u8 ack_req, u32 ancillary,
				struct dfc_flow_status_info_type_v01 *fc_info)
{
	struct rmnet_bearer_map *bearer;

	list_for_each_entry(bearer, &qos->bearer_head, list) {
		bearer->grant_size = fc_info->num_bytes;
		bearer->grant_thresh =
			qmi_rmnet_grant_per(bearer->grant_size);
		bearer->seq = fc_info->seq_num;
		bearer->ack_req = ack_req;
		bearer->tcp_bidir = DFC_IS_TCP_BIDIR(ancillary);
		bearer->last_grant = fc_info->num_bytes;
		bearer->last_seq = fc_info->seq_num;
		bearer->last_adjusted_grant = fc_info->num_bytes;

		dfc_bearer_flow_ctl(dev, bearer, qos);
	}

	return 0;
}

static u32 dfc_adjust_grant(struct rmnet_bearer_map *bearer,
			    struct dfc_flow_status_info_type_v01 *fc_info)
{
	u32 grant;

	if (!fc_info->rx_bytes_valid)
		return fc_info->num_bytes;

	if (bearer->bytes_in_flight > fc_info->rx_bytes)
		bearer->bytes_in_flight -= fc_info->rx_bytes;
	else
		bearer->bytes_in_flight = 0;

	/* Adjusted grant = grant - bytes_in_flight */
	if (fc_info->num_bytes > bearer->bytes_in_flight)
		grant = fc_info->num_bytes - bearer->bytes_in_flight;
	else
		grant = 0;

	trace_dfc_adjust_grant(fc_info->mux_id, fc_info->bearer_id,
			       fc_info->num_bytes, fc_info->rx_bytes,
			       bearer->bytes_in_flight, grant);
	return grant;
}

static int dfc_update_fc_map(struct net_device *dev, struct qos_info *qos,
			     u8 ack_req, u32 ancillary,
			     struct dfc_flow_status_info_type_v01 *fc_info,
			     bool is_query)
{
	struct rmnet_bearer_map *itm = NULL;
	int rc = 0;
	bool action = false;
	u32 adjusted_grant;

	itm = qmi_rmnet_get_bearer_map(qos, fc_info->bearer_id);
	if (!itm)
		itm = qmi_rmnet_get_bearer_noref(qos, fc_info->bearer_id);

	if (itm) {
		/* The RAT switch flag indicates the start and end of
		 * the switch. Ignore indications in between.
		 */
		if (DFC_IS_RAT_SWITCH(ancillary))
			itm->rat_switch = !fc_info->num_bytes;
		else
			if (itm->rat_switch)
				return 0;

		/* If TX is OFF but we received grant, ignore it */
		if (itm->tx_off  && fc_info->num_bytes > 0)
			return 0;

		/* Adjuste grant for query */
		if (dfc_qmap && is_query) {
			adjusted_grant = dfc_adjust_grant(itm, fc_info);
		} else {
			adjusted_grant = fc_info->num_bytes;
			itm->bytes_in_flight = 0;
		}

		if ((itm->grant_size == 0 && adjusted_grant > 0) ||
		    (itm->grant_size > 0 && adjusted_grant == 0))
			action = true;

		/* This is needed by qmap */
		if (dfc_qmap && itm->ack_req && !ack_req && itm->grant_size)
			dfc_qmap_send_ack(qos, itm->bearer_id,
					  itm->seq, DFC_ACK_TYPE_DISABLE);

		itm->grant_size = adjusted_grant;

		/* No further query if the adjusted grant is less
		 * than 20% of the original grant. Add to watch to
		 * recover if no indication is received.
		 */
		if (dfc_qmap && is_query &&
		    itm->grant_size < (fc_info->num_bytes / 5)) {
			itm->grant_thresh = itm->grant_size;
			qmi_rmnet_watchdog_add(itm);
		} else {
			itm->grant_thresh =
				qmi_rmnet_grant_per(itm->grant_size);
			qmi_rmnet_watchdog_remove(itm);
		}

		itm->seq = fc_info->seq_num;
		itm->ack_req = ack_req;
		itm->tcp_bidir = DFC_IS_TCP_BIDIR(ancillary);
		itm->last_grant = fc_info->num_bytes;
		itm->last_seq = fc_info->seq_num;
		itm->last_adjusted_grant = adjusted_grant;

		if (action)
			rc = dfc_bearer_flow_ctl(dev, itm, qos);
	}

	return rc;
}

void dfc_do_burst_flow_control(struct dfc_qmi_data *dfc,
			       struct dfc_flow_status_ind_msg_v01 *ind,
			       bool is_query)
{
	struct net_device *dev;
	struct qos_info *qos;
	struct dfc_flow_status_info_type_v01 *flow_status;
	struct dfc_ancillary_info_type_v01 *ai;
	u8 ack_req = ind->eod_ack_reqd_valid ? ind->eod_ack_reqd : 0;
	u32 ancillary;
	int i, j;

	rcu_read_lock();

	for (i = 0; i < ind->flow_status_len; i++) {
		flow_status = &ind->flow_status[i];

		ancillary = 0;
		if (ind->ancillary_info_valid) {
			for (j = 0; j < ind->ancillary_info_len; j++) {
				ai = &ind->ancillary_info[j];
				if (ai->mux_id == flow_status->mux_id &&
				    ai->bearer_id == flow_status->bearer_id) {
					ancillary = ai->reserved;
					break;
				}
			}
		}

		trace_dfc_flow_ind(dfc->index,
				   i, flow_status->mux_id,
				   flow_status->bearer_id,
				   flow_status->num_bytes,
				   flow_status->seq_num,
				   ack_req,
				   ancillary);

		dev = rmnet_get_rmnet_dev(dfc->rmnet_port,
					  flow_status->mux_id);
		if (!dev)
			goto clean_out;

		qos = (struct qos_info *)rmnet_get_qos_pt(dev);
		if (!qos)
			continue;

		spin_lock_bh(&qos->qos_lock);

		if (qmi_rmnet_ignore_grant(dfc->rmnet_port)) {
			spin_unlock_bh(&qos->qos_lock);
			continue;
		}

		if (unlikely(flow_status->bearer_id == 0xFF))
			dfc_all_bearer_flow_ctl(
				dev, qos, ack_req, ancillary, flow_status);
		else
			dfc_update_fc_map(
				dev, qos, ack_req, ancillary, flow_status,
				is_query);

		spin_unlock_bh(&qos->qos_lock);
	}

clean_out:
	rcu_read_unlock();
}

static void dfc_update_tx_link_status(struct net_device *dev,
				      struct qos_info *qos, u8 tx_status,
				      struct dfc_bearer_info_type_v01 *binfo)
{
	struct rmnet_bearer_map *itm = NULL;

	itm = qmi_rmnet_get_bearer_map(qos, binfo->bearer_id);
	if (!itm)
		return;

	/* If no change in tx status, ignore */
	if (itm->tx_off == !tx_status)
		return;

	if (itm->grant_size && !tx_status) {
		itm->grant_size = 0;
		itm->tcp_bidir = false;
		itm->bytes_in_flight = 0;
		qmi_rmnet_watchdog_remove(itm);
		dfc_bearer_flow_ctl(dev, itm, qos);
	} else if (itm->grant_size == 0 && tx_status && !itm->rat_switch) {
		itm->grant_size = DEFAULT_GRANT;
		itm->grant_thresh = qmi_rmnet_grant_per(DEFAULT_GRANT);
		itm->seq = 0;
		itm->ack_req = 0;
		dfc_bearer_flow_ctl(dev, itm, qos);
	}

	itm->tx_off = !tx_status;
}

void dfc_handle_tx_link_status_ind(struct dfc_qmi_data *dfc,
				   struct dfc_tx_link_status_ind_msg_v01 *ind)
{
	struct net_device *dev;
	struct qos_info *qos;
	struct dfc_bearer_info_type_v01 *bearer_info;
	int i;

	rcu_read_lock();

	for (i = 0; i < ind->bearer_info_len; i++) {
		bearer_info = &ind->bearer_info[i];

		trace_dfc_tx_link_status_ind(dfc->index, i,
					     ind->tx_status,
					     bearer_info->mux_id,
					     bearer_info->bearer_id);

		dev = rmnet_get_rmnet_dev(dfc->rmnet_port,
					  bearer_info->mux_id);
		if (!dev)
			goto clean_out;

		qos = (struct qos_info *)rmnet_get_qos_pt(dev);
		if (!qos)
			continue;

		spin_lock_bh(&qos->qos_lock);

		dfc_update_tx_link_status(
			dev, qos, ind->tx_status, bearer_info);

		spin_unlock_bh(&qos->qos_lock);
	}

clean_out:
	rcu_read_unlock();
}

static void dfc_qmi_ind_work(struct work_struct *work)
{
	struct dfc_qmi_data *dfc = container_of(work, struct dfc_qmi_data,
						qmi_ind_work);
	struct dfc_svc_ind *svc_ind;
	unsigned long flags;

	if (!dfc)
		return;

	local_bh_disable();

	do {
		spin_lock_irqsave(&dfc->qmi_ind_lock, flags);
		svc_ind = list_first_entry_or_null(&dfc->qmi_ind_q,
						   struct dfc_svc_ind, list);
		if (svc_ind)
			list_del(&svc_ind->list);
		spin_unlock_irqrestore(&dfc->qmi_ind_lock, flags);

		if (!svc_ind)
			break;

		if (!dfc->restart_state) {
			if (svc_ind->msg_id == QMI_DFC_FLOW_STATUS_IND_V01)
				dfc_do_burst_flow_control(
						dfc, &svc_ind->d.dfc_info,
						false);
			else if (svc_ind->msg_id ==
					QMI_DFC_TX_LINK_STATUS_IND_V01)
				dfc_handle_tx_link_status_ind(
						dfc, &svc_ind->d.tx_status);
		}
		kfree(svc_ind);
	} while (1);

	local_bh_enable();

	qmi_rmnet_set_dl_msg_active(dfc->rmnet_port);
}

static void dfc_clnt_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			    struct qmi_txn *txn, const void *data)
{
	struct dfc_qmi_data *dfc = container_of(qmi, struct dfc_qmi_data,
						handle);
	struct dfc_flow_status_ind_msg_v01 *ind_msg;
	struct dfc_svc_ind *svc_ind;
	unsigned long flags;

	if (qmi != &dfc->handle)
		return;

	ind_msg = (struct dfc_flow_status_ind_msg_v01 *)data;
	if (ind_msg->flow_status_valid) {
		if (ind_msg->flow_status_len > DFC_MAX_BEARERS_V01) {
			pr_err("%s() Invalid fc info len: %d\n",
			       __func__, ind_msg->flow_status_len);
			return;
		}

		svc_ind = kzalloc(sizeof(struct dfc_svc_ind), GFP_ATOMIC);
		if (!svc_ind)
			return;

		svc_ind->msg_id = QMI_DFC_FLOW_STATUS_IND_V01;
		memcpy(&svc_ind->d.dfc_info, ind_msg, sizeof(*ind_msg));

		spin_lock_irqsave(&dfc->qmi_ind_lock, flags);
		list_add_tail(&svc_ind->list, &dfc->qmi_ind_q);
		spin_unlock_irqrestore(&dfc->qmi_ind_lock, flags);

		queue_work(dfc->dfc_wq, &dfc->qmi_ind_work);
	}
}

static void dfc_tx_link_status_ind_cb(struct qmi_handle *qmi,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct dfc_qmi_data *dfc = container_of(qmi, struct dfc_qmi_data,
						handle);
	struct dfc_tx_link_status_ind_msg_v01 *ind_msg;
	struct dfc_svc_ind *svc_ind;
	unsigned long flags;

	if (qmi != &dfc->handle)
		return;

	ind_msg = (struct dfc_tx_link_status_ind_msg_v01 *)data;
	if (ind_msg->bearer_info_valid) {
		if (ind_msg->bearer_info_len > DFC_MAX_BEARERS_V01) {
			pr_err("%s() Invalid bearer info len: %d\n",
			       __func__, ind_msg->bearer_info_len);
			return;
		}

		svc_ind = kzalloc(sizeof(struct dfc_svc_ind), GFP_ATOMIC);
		if (!svc_ind)
			return;

		svc_ind->msg_id = QMI_DFC_TX_LINK_STATUS_IND_V01;
		memcpy(&svc_ind->d.tx_status, ind_msg, sizeof(*ind_msg));

		spin_lock_irqsave(&dfc->qmi_ind_lock, flags);
		list_add_tail(&svc_ind->list, &dfc->qmi_ind_q);
		spin_unlock_irqrestore(&dfc->qmi_ind_lock, flags);

		queue_work(dfc->dfc_wq, &dfc->qmi_ind_work);
	}
}

static void dfc_svc_init(struct work_struct *work)
{
	int rc = 0;
	struct dfc_qmi_data *data = container_of(work, struct dfc_qmi_data,
						 svc_arrive);
	struct qmi_info *qmi;

	if (data->restart_state == 1)
		return;

	rc = dfc_init_service(data);
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

	qmi->dfc_pending[data->index] = NULL;
	qmi->dfc_clients[data->index] = (void *)data;
	trace_dfc_client_state_up(data->index,
				  data->svc.instance,
				  data->svc.ep_type,
				  data->svc.iface_id);

	rtnl_unlock();

	pr_info("Connection established with the DFC Service\n");
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

	if (!data)
		pr_debug("%s() data is null\n", __func__);
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
		.decoded_size = sizeof(struct dfc_flow_status_ind_msg_v01),
		.fn = dfc_clnt_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_DFC_TX_LINK_STATUS_IND_V01,
		.ei = dfc_tx_link_status_ind_v01_ei,
		.decoded_size = sizeof(struct dfc_tx_link_status_ind_msg_v01),
		.fn = dfc_tx_link_status_ind_cb,
	},
	{},
};

int dfc_qmi_client_init(void *port, int index, struct svc_info *psvc,
			struct qmi_info *qmi)
{
	struct dfc_qmi_data *data;
	int rc = -ENOMEM;

	if (!port || !qmi)
		return -EINVAL;

	data = kzalloc(sizeof(struct dfc_qmi_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->rmnet_port = port;
	data->index = index;
	data->restart_state = 0;
	memcpy(&data->svc, psvc, sizeof(data->svc));

	INIT_WORK(&data->qmi_ind_work, dfc_qmi_ind_work);
	INIT_LIST_HEAD(&data->qmi_ind_q);
	spin_lock_init(&data->qmi_ind_lock);

	data->dfc_wq = create_singlethread_workqueue("dfc_wq");
	if (!data->dfc_wq) {
		pr_err("%s Could not create workqueue\n", __func__);
		goto err0;
	}

	INIT_WORK(&data->svc_arrive, dfc_svc_init);
	rc = qmi_handle_init(&data->handle,
			     QMI_DFC_GET_FLOW_STATUS_RESP_V01_MAX_MSG_LEN,
			     &server_ops, qmi_indication_handler);
	if (rc < 0) {
		pr_err("%s: failed qmi_handle_init - rc[%d]\n", __func__, rc);
		goto err1;
	}

	rc = qmi_add_lookup(&data->handle, DFC_SERVICE_ID_V01,
			    DFC_SERVICE_VERS_V01,
			    psvc->instance);
	if (rc < 0) {
		pr_err("%s: failed qmi_add_lookup - rc[%d]\n", __func__, rc);
		goto err2;
	}

	qmi->dfc_pending[index] = (void *)data;

	return 0;

err2:
	qmi_handle_release(&data->handle);
err1:
	destroy_workqueue(data->dfc_wq);
err0:
	kfree(data);
	return rc;
}

void dfc_qmi_client_exit(void *dfc_data)
{
	struct dfc_qmi_data *data = (struct dfc_qmi_data *)dfc_data;

	if (!data) {
		pr_err("%s() data is null\n", __func__);
		return;
	}

	data->restart_state = 1;
	trace_dfc_client_state_down(data->index, 0);
	qmi_handle_release(&data->handle);

	drain_workqueue(data->dfc_wq);
	destroy_workqueue(data->dfc_wq);
	kfree(data);
}

void dfc_qmi_burst_check(struct net_device *dev, struct qos_info *qos,
			 int ip_type, u32 mark, unsigned int len)
{
	struct rmnet_bearer_map *bearer = NULL;
	struct rmnet_flow_map *itm;
	u32 start_grant;

	spin_lock_bh(&qos->qos_lock);

	if (dfc_mode == DFC_MODE_MQ_NUM) {
		/* Mark is mq num */
		if (likely(mark < MAX_MQ_NUM))
			bearer = qos->mq[mark].bearer;
	} else {
		/* Mark is flow_id */
		itm = qmi_rmnet_get_flow_map(qos, mark, ip_type);
		if (likely(itm))
			bearer = itm->bearer;
	}

	if (unlikely(!bearer))
		goto out;

	trace_dfc_flow_check(dev->name, bearer->bearer_id,
			     len, mark, bearer->grant_size);

	bearer->bytes_in_flight += len;

	if (!bearer->grant_size)
		goto out;

	start_grant = bearer->grant_size;
	if (len >= bearer->grant_size)
		bearer->grant_size = 0;
	else
		bearer->grant_size -= len;

	if (start_grant > bearer->grant_thresh &&
	    bearer->grant_size <= bearer->grant_thresh) {
		dfc_send_ack(dev, bearer->bearer_id,
			     bearer->seq, qos->mux_id,
			     DFC_ACK_TYPE_THRESHOLD);
	}

	if (!bearer->grant_size)
		dfc_bearer_flow_ctl(dev, bearer, qos);

out:
	spin_unlock_bh(&qos->qos_lock);
}

void dfc_qmi_query_flow(void *dfc_data)
{
	struct dfc_qmi_data *data = (struct dfc_qmi_data *)dfc_data;
	struct dfc_get_flow_status_resp_msg_v01 *resp;
	struct dfc_svc_ind *svc_ind;
	int rc;

	resp = kzalloc(sizeof(*resp), GFP_ATOMIC);
	if (!resp)
		return;

	svc_ind = kzalloc(sizeof(*svc_ind), GFP_ATOMIC);
	if (!svc_ind) {
		kfree(resp);
		return;
	}

	if (!data)
		goto done;

	rc = dfc_get_flow_status_req(&data->handle, &data->ssctl, resp);

	if (rc < 0 || !resp->flow_status_valid || resp->flow_status_len < 1 ||
	    resp->flow_status_len > DFC_MAX_BEARERS_V01)
		goto done;

	svc_ind->d.dfc_info.flow_status_valid = resp->flow_status_valid;
	svc_ind->d.dfc_info.flow_status_len = resp->flow_status_len;
	memcpy(&svc_ind->d.dfc_info.flow_status, resp->flow_status,
		sizeof(resp->flow_status[0]) * resp->flow_status_len);
	dfc_do_burst_flow_control(data, &svc_ind->d.dfc_info, true);

done:
	kfree(svc_ind);
	kfree(resp);
}
