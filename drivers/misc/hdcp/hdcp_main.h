/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __HDCP_MAIN_H__
#define __HDCP_MAIN_H__

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hdcp_qseecom.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <misc/qseecom_kernel.h>

#define HDCP2P2_APP_NAME "hdcp2p2"
#define HDCP1_APP_NAME "hdcp1"
#define HDCP1OPS_APP_NAME "ops"
#define HDCPSRM_APP_NAME "hdcpsrm"
#define QSEECOM_SBUFF_SIZE 0x1000

#define MAX_REC_ID_LIST_SIZE 160
#define MAX_TX_MESSAGE_SIZE 129
#define MAX_RX_MESSAGE_SIZE 534
#define MAX_TOPOLOGY_ELEMS 32
#define HDCP1_NOTIFY_TOPOLOGY 1
#define HDCP1_AKSV_SIZE 8

#define HDCP_CLIENT_MAKE_VERSION(maj, min, patch) \
	((((maj)&0xFF) << 16) | (((min)&0xFF) << 8) | ((patch)&0xFF))

#define HCDP_TXMTR_GET_MAJOR_VERSION(v) (((v) >> 16) & 0xFF)
#define HCDP_TXMTR_GET_MINOR_VERSION(v) (((v) >> 8) & 0xFF)
#define HCDP_TXMTR_GET_PATCH_VERSION(v) ((v)&0xFF)

#define HDCP_CLIENT_MAJOR_VERSION 2
#define HDCP_CLIENT_MINOR_VERSION 1
#define HDCP_CLIENT_PATCH_VERSION 0

#define HDCP_SUCCESS 0

/* Wait 200ms after authentication */
#define SLEEP_FORCE_ENCRYPTION_MS 200

#define SLEEP_SET_HW_KEY_MS 300

/* flags set by tz in response message */
#define HDCP_TXMTR_SUBSTATE_INIT 0
#define HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST 1
#define HDCP_TXMTR_SUBSTATE_PROCESSED_RECIEVERID_LIST 2
#define HDCP_TXMTR_SUBSTATE_WAITING_FOR_STREAM_READY_MESSAGE 3
#define HDCP_TXMTR_SUBSTATE_REPEATER_AUTH_COMPLETE 4

enum hdcp_state {
	HDCP_STATE_INIT = 0x00,
	HDCP_STATE_APP_LOADED = 0x01,
	HDCP_STATE_SESSION_INIT = 0x02,
	HDCP_STATE_TXMTR_INIT = 0x04,
	HDCP_STATE_AUTHENTICATED = 0x08,
	HDCP_STATE_ERROR = 0x10
};

struct hdcp_ta_interface {
	void *(*trusted_app_hdcp1_init)(void);
	bool (*trusted_app_hdcp1_feature_supported)(void *data);
	int (*trusted_app_hdcp1_set_enc)(void *data, bool enable);
	int (*trusted_app_hdcp1_ops_notify)(void *data, void *topo,
		bool is_authenticated);
	int (*trusted_app_hdcp1_start)(void *data, u32 *aksv_msb,
		  u32 *aksv_lsb);
	void (*trusted_app_hdcp1_stop)(void *data);
	void *(*trusted_app_hdcp2_init)(u32 device_type);
	void (*trusted_app_hdcp2_deinit)(void *ctx);
	int (*trusted_app_hdcp2_app_start)(void *ctx, uint32_t req_len);
	int (*trusted_app_hdcp2_app_start_auth)(void *ctx, uint32_t req_len);
	int (*trusted_app_hdcp2_app_process_msg)(void *ctx, uint32_t req_len);
	int (*trusted_app_hdcp2_app_timeout)(void *ctx, uint32_t req_len);
	int (*trusted_app_hdcp2_app_enable_encryption)(void *ctx, uint32_t req_len);
	int (*trusted_app_hdcp2_app_query_stream)(void *ctx, uint32_t req_len);
	int (*trusted_app_hdcp2_app_stop)(void *ctx);
	bool (*trusted_app_hdcp2_feature_supported)(void *ctx);
	int (*trusted_app_hdcp2_force_encryption)(void *ctx, uint32_t enable);
	int (*trusted_app_hdcp2_open_stream)(void *ctx, uint8_t vc_payload_id,
		 uint8_t stream_number, uint32_t *stream_id);
	int (*trusted_app_hdcp2_close_stream)(void *ctx, uint32_t stream_id);
	int (*trusted_app_hdcp2_update_app_data)(void *ctx,
		 struct hdcp2_app_data *app_data);
};

int hdcp1_validate_aksv(u32 aksv_msb, u32 aksv_lsb);

#endif /* __HDCP_MAIN_H__ */
