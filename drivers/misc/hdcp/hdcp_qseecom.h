/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __HDCP_QSEECOM_H__
#define __HDCP_QSEECOM_H__

#include <linux/hdcp_qseecom.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "hdcp_main.h"

struct hdcp1_qsee_handle {
	struct qseecom_handle *qseecom_handle;
	struct qseecom_handle *hdcpops_handle;
	bool feature_supported;
	uint32_t device_type;
	enum hdcp_state hdcp_state;
	char *app_name;
};

struct hdcp2_qsee_handle {
	struct hdcp2_app_data app_data;
	uint32_t tz_ctxhandle;
	bool feature_supported;
	enum hdcp_state hdcp_state;
	struct qseecom_handle *qseecom_handle;
	struct qseecom_handle *hdcpsrm_qseecom_handle;
	uint32_t session_id;
	bool legacy_app;
	uint32_t device_type;
	char *app_name;
	unsigned char *req_buf;
	unsigned char *res_buf;
	int (*app_init)(struct hdcp2_qsee_handle *handle);
	int (*tx_init)(struct hdcp2_qsee_handle *handle);
};

struct hdcp1_key_set_req {
	uint32_t commandid;
} __packed;

struct hdcp1_key_set_rsp {
	uint32_t commandid;
	uint32_t ret;
	uint8_t ksv[HDCP1_AKSV_SIZE];
} __packed;

struct hdcp1_ops_notify_req {
	uint32_t commandid;
	uint32_t device_type;
	uint8_t recv_id_list[MAX_REC_ID_LIST_SIZE];
	int32_t recv_id_len;
	struct hdcp1_topology topology;
	bool is_authenticated;
} __packed;

struct hdcp1_ops_notify_rsp {
	uint32_t commandid;
	uint32_t ret;
} __packed;

struct hdcp1_set_enc_req {
	uint32_t commandid;
	uint32_t enable;
} __packed;

struct hdcp1_set_enc_rsp {
	uint32_t commandid;
	uint32_t ret;
} __packed;

struct hdcp1_key_verify_req {
	uint32_t commandid;
	uint32_t key_type;
} __packed;

struct hdcp1_key_verify_rsp {
	uint32_t commandId;
	uint32_t ret;
} __packed;

struct hdcp_init_v1_req {
	uint32_t commandid;
} __packed;

struct hdcp_init_v1_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
} __packed;

struct hdcp_init_req {
	uint32_t commandid;
	uint32_t clientversion;
} __packed;

struct hdcp_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t appversion;
} __packed;

struct hdcp_session_init_req {
	uint32_t commandid;
	uint32_t deviceid;
} __packed;

struct hdcp_session_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t sessionid;
} __packed;

struct hdcp_tx_init_v1_req {
	uint32_t commandid;
} __packed;

struct hdcp_tx_init_v1_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
} __packed;

struct hdcp_tx_init_req {
	uint32_t commandid;
	uint32_t sessionid;
} __packed;

struct hdcp_tx_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
} __packed;

struct hdcp_version_req {
	uint32_t commandid;
} __packed;

struct hdcp_version_rsp {
	uint32_t status;
	uint32_t commandId;
	uint32_t appversion;
} __packed;

struct hdcp_session_open_stream_req {
	uint32_t commandid;
	uint32_t sessionid;
	uint32_t vcpayloadid;
	uint32_t stream_number;
	uint32_t stream_media_type;
} __packed;

struct hdcp_session_open_stream_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t streamid;
} __packed;

struct hdcp_session_close_stream_req {
	uint32_t commandid;
	uint32_t sessionid;
	uint32_t streamid;
} __packed;

struct hdcp_session_close_stream_rsp {
	uint32_t status;
	uint32_t commandid;
} __packed;

struct hdcp_force_encryption_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t enable;
} __packed;

struct hdcp_force_encryption_rsp {
	uint32_t status;
	uint32_t commandid;
} __packed;

struct hdcp_tx_deinit_req {
	uint32_t commandid;
	uint32_t ctxhandle;
} __packed;

struct hdcp_tx_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
} __packed;

struct hdcp_session_deinit_req {
	uint32_t commandid;
	uint32_t sessionid;
} __packed;

struct hdcp_session_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
} __packed;

struct hdcp_deinit_req {
	uint32_t commandid;
} __packed;

struct hdcp_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
} __packed;

struct hdcp_query_stream_type_req {
	uint32_t commandid;
	uint32_t ctxhandle;
} __packed;

struct hdcp_query_stream_type_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t msg[MAX_TX_MESSAGE_SIZE];
} __packed;

struct hdcp_set_hw_key_req {
	uint32_t commandid;
	uint32_t ctxhandle;
} __packed;

struct hdcp_set_hw_key_rsp {
	uint32_t status;
	uint32_t commandid;
} __packed;

struct hdcp_send_timeout_req {
	uint32_t commandid;
	uint32_t ctxhandle;
} __packed;

struct hdcp_send_timeout_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
} __packed;

struct hdcp_start_auth_req {
	uint32_t commandid;
	uint32_t ctxHandle;
} __packed;

struct hdcp_start_auth_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
} __packed;

struct hdcp_rcvd_msg_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t msglen;
	uint8_t msg[MAX_RX_MESSAGE_SIZE];
} __packed;

struct hdcp_rcvd_msg_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t state;
	uint32_t timeout;
	uint32_t flag;
	uint32_t msglen;
	uint8_t msg[MAX_TX_MESSAGE_SIZE];
} __packed;

struct hdcp_verify_key_req {
	uint32_t commandid;
} __packed;

struct hdcp_verify_key_rsp {
	uint32_t status;
	uint32_t commandId;
} __packed;

#define HDCP1_SET_KEY 202
#define HDCP1_KEY_VERIFY 204
#define HDCP1_SET_ENC 205

/* DP device type */
#define DEVICE_TYPE_DP 0x8002

void *hdcp1_init_qseecom(void);
bool hdcp1_feature_supported_qseecom(void *data);
int hdcp1_set_enc_qseecom(void *data, bool enable);
int hdcp1_ops_notify_qseecom(void *data, void *topo, bool is_authenticated);
int hdcp1_start_qseecom(void *data, u32 *aksv_msb, u32 *aksv_lsb);
void hdcp1_stop_qseecom(void *data);

void *hdcp2_init_qseecom(u32 device_type);
void hdcp2_deinit_qseecom(void *ctx);
int hdcp2_app_start_qseecom(void *ctx, uint32_t req_len);
int hdcp2_app_start_auth_qseecom(void *ctx, uint32_t req_len);
int hdcp2_app_process_msg_qseecom(void *ctx, uint32_t req_len);
int hdcp2_app_timeout_qseecom(void *ctx, uint32_t req_len);
int hdcp2_app_enable_encryption_qseecom(void *ctx, uint32_t req_len);
int hdcp2_app_query_stream_qseecom(void *ctx, uint32_t req_len);
int hdcp2_app_stop_qseecom(void *ctx);
bool hdcp2_feature_supported_qseecom(void *ctx);
int hdcp2_force_encryption_qseecom(void *ctx, uint32_t enable);
int hdcp2_open_stream_qseecom(void *ctx, uint8_t vc_payload_id,
		  uint8_t stream_number, uint32_t *stream_id);
int hdcp2_close_stream_qseecom(void *ctx, uint32_t stream_id);
int hdcp2_update_app_data_qseecom(void *ctx, struct hdcp2_app_data *app_data);

#endif /* __HDCP_QSEECOM_H__ */
