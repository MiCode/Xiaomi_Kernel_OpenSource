/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __HDCP_SMCINVOKE_H__
#define __HDCP_SMCINVOKE_H__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <soc/qcom/smci_object.h>

#include "hdcp_main.h"

struct hdcp1_smcinvoke_handle {
	struct smci_object hdcp1_app_obj;
	struct smci_object hdcp1_appcontroller_obj;
	struct smci_object hdcp1ops_app_obj;
	struct smci_object hdcp1ops_appcontroller_obj;
	bool feature_supported;
	uint32_t device_type;
	enum hdcp_state hdcp_state;
};

struct hdcp2_smcinvoke_handle {
	struct hdcp2_app_data app_data;
	uint32_t tz_ctxhandle;
	bool feature_supported;
	enum hdcp_state hdcp_state;
	struct smci_object hdcp2_app_obj;
	struct smci_object hdcp2_appcontroller_obj;
	struct smci_object hdcpsrm_app_obj;
	struct smci_object hdcpsrm_appcontroller_obj;
	uint32_t session_id;
	uint32_t device_type;
};

void *hdcp1_init_smcinvoke(void);
bool hdcp1_feature_supported_smcinvoke(void *data);
int hdcp1_set_enc_smcinvoke(void *data, bool enable);
int hdcp1_ops_notify_smcinvoke(void *data, void *topo, bool is_authenticated);
int hdcp1_start_smcinvoke(void *data, u32 *aksv_msb, u32 *aksv_lsb);
void hdcp1_stop_smcinvoke(void *data);

void *hdcp2_init_smcinvoke(u32 device_type);
void hdcp2_deinit_smcinvoke(void *ctx);
int hdcp2_app_start_smcinvoke(void *ctx, uint32_t req_len);
int hdcp2_app_start_auth_smcinvoke(void *ctx, uint32_t req_len);
int hdcp2_app_process_msg_smcinvoke(void *ctx, uint32_t req_len);
int hdcp2_app_timeout_smcinvoke(void *ctx, uint32_t req_len);
int hdcp2_app_enable_encryption_smcinvoke(void *ctx, uint32_t req_len);
int hdcp2_app_query_stream_smcinvoke(void *ctx, uint32_t req_len);
int hdcp2_app_stop_smcinvoke(void *ctx);
bool hdcp2_feature_supported_smcinvoke(void *ctx);
int hdcp2_force_encryption_smcinvoke(void *ctx, uint32_t enable);
int hdcp2_open_stream_smcinvoke(void *ctx, uint8_t vc_payload_id,
		uint8_t stream_number, uint32_t *stream_id);
int hdcp2_close_stream_smcinvoke(void *ctx, uint32_t stream_id);
int hdcp2_update_app_data_smcinvoke(void *ctx, struct hdcp2_app_data *app_data);

#endif /* __HDCP_SMCINVOKE_H__ */
