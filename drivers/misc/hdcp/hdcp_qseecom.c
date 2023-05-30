// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/hdcp_qseecom.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <misc/qseecom_kernel.h>

#include "hdcp_qseecom.h"
#include "hdcp_main.h"

#define HDCP_CMD_STATUS_TO_STR(x) #x

#define hdcp2_app_init_var(x) \
	struct hdcp_##x##_req *req_buf = NULL; \
	struct hdcp_##x##_rsp *rsp_buf = NULL; \
	if (!handle->qseecom_handle) { \
		pr_err("invalid qseecom_handle while processing %s\n", #x); \
		rc = -EINVAL; \
		goto error; \
	} \
	req_buf = (struct hdcp_##x##_req *)handle->qseecom_handle->sbuf; \
	rsp_buf = (struct hdcp_##x##_rsp *)(handle->qseecom_handle->sbuf + \
			   QSEECOM_ALIGN(sizeof(struct hdcp_##x##_req))); \
	req_buf->commandid = hdcp_cmd_##x

#define hdcp2_app_process_cmd(x) \
	({ \
		int rc = qseecom_send_command( \
			handle->qseecom_handle, req_buf, \
			QSEECOM_ALIGN(sizeof(struct hdcp_##x##_req)), rsp_buf, \
			QSEECOM_ALIGN(sizeof(struct hdcp_##x##_rsp))); \
		if ((rc < 0) || (rsp_buf->status != HDCP_SUCCESS)) { \
			pr_err("qseecom cmd %s failed with err = %d, status = %d:%s\n", \
				   #x, rc, rsp_buf->status, \
				   hdcp_cmd_status_to_str(rsp_buf->status)); \
			rc = -EINVAL; \
		} \
		rc; \
	})

const char *hdcp_errors[] =	{"HDCP_SUCCESS",
				"HDCP_FAIL",
				"HDCP_BAD_PARAM",
				"HDCP_DEVICE_TYPE_UNSUPPORTED",
				"HDCP_INVALID_COMMAND",
				"HDCP_INVALID_COMMAND_HANDLE",
				"HDCP_ERROR_SIZE_IN",
				"HDCP_ERROR_SIZE_OUT",
				"HDCP_DATA_SIZE_INSUFFICIENT",
				"HDCP_UNSUPPORTED_RX_VERSION",
				"HDCP_WRONG_RX_CAPAB_MASK",
				"HDCP_WRONG_RX_RSVD",
				"HDCP_WRONG_RX_HDCP_CAPABLE",
				"HDCP_RSA_SIGNATURE_VERIFY_FAILED",
				"HDCP_VERIFY_H_PRIME_FAILED",
				"HDCP_LC_FAILED",
				"HDCP_MESSAGE_TIMEOUT",
				"HDCP_COUNTER_ROLL_OVER",
				"HDCP_WRONG_RXINFO_RSVD",
				"HDCP_RXINFO_MAX_DEVS",
				"HDCP_RXINFO_MAX_CASCADE",
				"HDCP_WRONG_INITIAL_SEQ_NUM_V",
				"HDCP_SEQ_NUM_V_ROLL_OVER",
				"HDCP_WRONG_SEQ_NUM_V",
				"HDCP_VERIFY_V_FAILED",
				"HDCP_RPT_METHOD_INVOKED",
				"HDCP_RPT_STRM_LEN_WRONG",
				"HDCP_VERIFY_STRM_M_FAILED",
				"HDCP_TRANSMITTER_NOT_FOUND",
				"HDCP_SESSION_NOT_FOUND",
				"HDCP_MAX_SESSION_EXCEEDED",
				"HDCP_MAX_CONNECTION_EXCEEDED",
				"HDCP_MAX_STREAMS_EXCEEDED",
				"HDCP_MAX_DEVICES",
				"HDCP_ALLOC_FAILED",
				"HDCP_CONNECTION_NOT_FOUND",
				"HDCP_HASH_FAILED",
				"HDCP_BN_FAILED",
				"HDCP_ENCRYPT_KM_FAILED",
				"HDCP_DECRYPT_KM_FAILED",
				"HDCP_HMAC_FAILED",
				"HDCP_GET_RANDOM_FAILED",
				"HDCP_INVALID_KEY_HEADER",
				"HDCP_INVALID_KEY_LC_HASH",
				"HDCP_INVALID_KEY_HASH",
				"HDCP_KEY_WRITE_FAILED",
				"HDCP_KEY_READ_FAILED",
				"HDCP_KEY_DECRYPT_FAILED",
				"HDCP_TEST_KEY_ON_SECURE_DEVICE",
				"HDCP_KEY_VERSION_UNSUPPORTED",
				"HDCP_RXID_NOT_FOUND",
				"HDCP_STORAGE_INIT_FAILED",
				"HDCP_STORAGE_FILE_OPEN_FAILED",
				"HDCP_STORAGE_FILE_READ_FAILED",
				"HDCP_STORAGE_FILE_WRITE_FAILED",
				"HDCP_STORAGE_ID_UNSUPPORTED",
				"HDCP_MUTUAL_EXCLUSIVE_DEVICE_PRESENT",
				"HDCP_INVALID_STATE",
				"HDCP_CONFIG_READ_FAILED",
				"HDCP_OPEN_TZ_SERVICE_FAILED",
				"HDCP_HW_CLOCK_OFF",
				"HDCP_SET_HW_KEY_FAILED",
				"HDCP_CLEAR_HW_KEY_FAILED",
				"HDCP_GET_CONTENT_LEVEL_FAILED",
				"HDCP_STREAMID_INUSE",
				"HDCP_STREAM_NOT_FOUND",
				"HDCP_FORCE_ENCRYPTION_FAILED",
				"HDCP_STREAMNUMBER_INUSE"};

#define HDCP_TXMTR_SERVICE_ID 0x0001000
#define SERVICE_CREATE_CMD(x) (HDCP_TXMTR_SERVICE_ID | x)

#define HDCP_CMD_STATUS_TO_STR(x) #x

enum {
	hdcp_cmd_tx_init = SERVICE_CREATE_CMD(1),
	hdcp_cmd_tx_init_v1 = SERVICE_CREATE_CMD(1),
	hdcp_cmd_tx_deinit = SERVICE_CREATE_CMD(2),
	hdcp_cmd_rcvd_msg = SERVICE_CREATE_CMD(3),
	hdcp_cmd_send_timeout = SERVICE_CREATE_CMD(4),
	hdcp_cmd_set_hw_key = SERVICE_CREATE_CMD(5),
	hdcp_cmd_query_stream_type = SERVICE_CREATE_CMD(6),
	hdcp_cmd_init_v1 = SERVICE_CREATE_CMD(11),
	hdcp_cmd_init = SERVICE_CREATE_CMD(11),
	hdcp_cmd_deinit = SERVICE_CREATE_CMD(12),
	hdcp_cmd_version = SERVICE_CREATE_CMD(14),
	hdcp_cmd_verify_key = SERVICE_CREATE_CMD(15),
	hdcp_cmd_session_init = SERVICE_CREATE_CMD(16),
	hdcp_cmd_session_deinit = SERVICE_CREATE_CMD(17),
	hdcp_cmd_start_auth = SERVICE_CREATE_CMD(18),
	hdcp_cmd_session_open_stream = SERVICE_CREATE_CMD(20),
	hdcp_cmd_session_close_stream = SERVICE_CREATE_CMD(21),
	hdcp_cmd_force_encryption = SERVICE_CREATE_CMD(22),
};

static struct qseecom_handle *qseecom_handle_g;
static struct qseecom_handle *hdcpsrm_qseecom_handle_g;
static int hdcp2_app_started;

static struct qseecom_handle *hdcp1_qseecom_handle_g;
static int hdcp1_app_started;

static const char *hdcp_cmd_status_to_str(uint32_t err)
{
	int len = ARRAY_SIZE(hdcp_errors);

	if (err >= 0 && err < len)
		return hdcp_errors[err];
	else
		return "";
}

static int hdcp1_app_load(struct hdcp1_qsee_handle *handle)
{
	int rc = 0;

	if (!handle) {
		pr_err("invalid handle\n");
		goto error;
	}

	if (!hdcp1_qseecom_handle_g) {
		rc = qseecom_start_app(&hdcp1_qseecom_handle_g, handle->app_name,
				QSEECOM_SBUFF_SIZE);
		if (rc) {
			pr_err("%s app load failed (%d)\n", handle->app_name, rc);
			goto error;
		}
	}
	handle->qseecom_handle = hdcp1_qseecom_handle_g;
	hdcp1_app_started++;

	rc = qseecom_start_app(&handle->hdcpops_handle, HDCP1OPS_APP_NAME,
			QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_warn("%s app load failed (%d)\n", HDCP1OPS_APP_NAME, rc);
		handle->hdcpops_handle = NULL;
	}

	handle->hdcp_state |= HDCP_STATE_APP_LOADED;
	pr_debug("%s app loaded\n", handle->app_name);

error:
	return rc;
}

static void hdcp1_app_unload(struct hdcp1_qsee_handle *handle)
{
	int rc = 0;

	if (!handle || !handle->qseecom_handle) {
		pr_err("invalid handle\n");
		return;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_warn("%s app not loaded\n", handle->app_name);
		return;
	}

	if (handle->hdcpops_handle) {
		/* deallocate the resources for HDCP 1.x ops handle */
		rc = qseecom_shutdown_app(&handle->hdcpops_handle);
		if (rc)
			pr_warn("%s app unload failed (%d)\n", HDCP1OPS_APP_NAME, rc);
	}

	/* deallocate the resources for qseecom HDCP 1.x handle */
	rc = qseecom_shutdown_app(&handle->qseecom_handle);
	if (rc) {
		pr_err("%s app unload failed (%d)\n", handle->app_name, rc);
		return;
	}

	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
	pr_debug("%s app unloaded\n", handle->app_name);
}

static int hdcp1_set_key(struct hdcp1_qsee_handle *hdcp1_handle, u32 *aksv_msb,
				 u32 *aksv_lsb)
{
	int rc = 0;
	struct hdcp1_key_set_req *key_set_req;
	struct hdcp1_key_set_rsp *key_set_rsp;
	struct qseecom_handle *handle = NULL;

	if (aksv_msb == NULL || aksv_lsb == NULL) {
		pr_err("invalid aksv\n");
		return -EINVAL;
	}

	if (!hdcp1_handle || !hdcp1_handle->qseecom_handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", hdcp1_handle->app_name);
		return -EINVAL;
	}

	handle = hdcp1_handle->qseecom_handle;

	/* set keys and request aksv */
	key_set_req = (struct hdcp1_key_set_req *)handle->sbuf;
	key_set_req->commandid = HDCP1_SET_KEY;
	key_set_rsp = (struct hdcp1_key_set_rsp *)(handle->sbuf +
			QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_req)));
	rc = qseecom_send_command(
		handle, key_set_req, QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_req)),
		key_set_rsp, QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_rsp)));

	if (rc < 0) {
		pr_err("qseecom cmd failed err=%d\n", rc);
		return -ENOKEY;
	}

	rc = key_set_rsp->ret;
	if (rc) {
		pr_err("set key cmd failed, rsp=%d\n", key_set_rsp->ret);
		return -ENOKEY;
	}

	/* copy bytes into msb and lsb */
	*aksv_msb = key_set_rsp->ksv[0] << 24 | key_set_rsp->ksv[1] << 16 |
				key_set_rsp->ksv[2] << 8 | key_set_rsp->ksv[3];
	*aksv_lsb = key_set_rsp->ksv[4] << 24 | key_set_rsp->ksv[5] << 16 |
				key_set_rsp->ksv[6] << 8 | key_set_rsp->ksv[7];

	rc = hdcp1_validate_aksv(*aksv_msb, *aksv_lsb);
	if (rc) {
		pr_err("aksv validation failed (%d)\n", rc);
		return rc;
	}

	return 0;
}

static int hdcp1_verify_key(struct hdcp1_qsee_handle *hdcp1_handle)
{
	int rc = 0;
	struct hdcp1_key_verify_req *key_verify_req;
	struct hdcp1_key_verify_rsp *key_verify_rsp;
	struct qseecom_handle *handle = NULL;

	if (!hdcp1_handle || !hdcp1_handle->qseecom_handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", hdcp1_handle->app_name);
		return -EINVAL;
	}

	handle = hdcp1_handle->qseecom_handle;

	key_verify_req = (struct hdcp1_key_verify_req *)handle->sbuf;
	key_verify_req->commandid = HDCP1_KEY_VERIFY;
	key_verify_rsp =
		(struct hdcp1_key_verify_rsp *)(handle->sbuf +
		 QSEECOM_ALIGN(sizeof(struct hdcp1_key_verify_req)));
	rc = qseecom_send_command(
		handle, key_verify_req,
		QSEECOM_ALIGN(sizeof(struct hdcp1_key_verify_req)), key_verify_rsp,
		QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_rsp)));

	if (rc < 0) {
		pr_err("command HDCP1_KEY_VERIFY failed (%d)\n", rc);
		return -EINVAL;
	}

	rc = key_verify_rsp->ret;
	if (rc) {
		pr_err("key_verify failed, rsp=%d\n", key_verify_rsp->ret);
		return -EINVAL;
	}

	pr_debug("success\n");

	return 0;
}

static int hdcp2_app_unload(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(deinit);

	hdcp2_app_started--;
	if (!hdcp2_app_started) {

		hdcp2_app_process_cmd(deinit);
		/* deallocate the resources for qseecom HDCPSRM handle */
		rc = qseecom_shutdown_app(&handle->hdcpsrm_qseecom_handle);
		if (rc)
			pr_err("qseecom_shutdown_app failed for HDCPSRM (%d)\n", rc);

		hdcpsrm_qseecom_handle_g = NULL;
		/* deallocate the resources for qseecom HDCP2P2 handle */
		rc = qseecom_shutdown_app(&handle->qseecom_handle);
		if (rc) {
			pr_err("qseecom_shutdown_app failed for HDCP2P2 (%d)\n", rc);
			return rc;
		}
		qseecom_handle_g = NULL;
	}
	handle->qseecom_handle = NULL;
	handle->hdcpsrm_qseecom_handle = NULL;

	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
	pr_debug("%s app unloaded\n", handle->app_name);

	return rc;
error:
	if (!hdcp2_app_started)
		qseecom_shutdown_app(&handle->hdcpsrm_qseecom_handle);
	return rc;
}

static int hdcp2_verify_key(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(verify_key);

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", handle->app_name);
		rc = -EINVAL;
		goto error;
	}

	rc = hdcp2_app_process_cmd(verify_key);
	pr_debug("verify_key = %d\n", rc);

error:
	return rc;
}

static int hdcp2_app_tx_deinit(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(tx_deinit);

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", handle->app_name);
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->ctxhandle = handle->tz_ctxhandle;

	rc = hdcp2_app_process_cmd(tx_deinit);
	if (rc)
		goto error;

	handle->hdcp_state &= ~HDCP_STATE_TXMTR_INIT;
	pr_debug("success\n");
error:
	return rc;
}

static int hdcp2_app_session_deinit(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(session_deinit);

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", handle->app_name);
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->sessionid = handle->session_id;

	rc = hdcp2_app_process_cmd(session_deinit);
	if (rc)
		goto error;

	handle->hdcp_state &= ~HDCP_STATE_SESSION_INIT;
	pr_debug("success\n");
error:
	return rc;
}

void *hdcp1_init_qseecom(void)
{
	struct hdcp1_qsee_handle *handle =
		kzalloc(sizeof(struct hdcp1_qsee_handle), GFP_KERNEL);

	if (!handle)
		goto error;

	handle->app_name = HDCP1_APP_NAME;

error:
	return handle;
}

bool hdcp1_feature_supported_qseecom(void *data)
{
	bool supported = false;
	struct hdcp1_qsee_handle *handle = data;
	int rc = 0;

	if (!handle) {
		pr_err("invalid handle\n");
		goto error;
	}

	if (handle->feature_supported) {
		supported = true;
		goto error;
	}

	rc = hdcp1_app_load(handle);
	if (!rc && (handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		if (!hdcp1_verify_key(handle)) {
			pr_debug("HDCP 1.x supported\n");
			handle->feature_supported = true;
			supported = true;
		}
		hdcp1_app_unload(handle);
	}
error:
	return supported;
}

int hdcp1_set_enc_qseecom(void *data, bool enable)
{
	int rc = 0;
	struct hdcp1_set_enc_req *set_enc_req;
	struct hdcp1_set_enc_rsp *set_enc_rsp;
	struct hdcp1_qsee_handle *hdcp1_handle = data;
	struct qseecom_handle *handle = NULL;

	if (!hdcp1_handle || !hdcp1_handle->qseecom_handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!hdcp1_handle->feature_supported) {
		pr_err("HDCP 1.x not supported\n");
		return -EINVAL;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", hdcp1_handle->app_name);
		return -EINVAL;
	}

	handle = hdcp1_handle->qseecom_handle;

	/* set keys and request aksv */
	set_enc_req = (struct hdcp1_set_enc_req *)handle->sbuf;
	set_enc_req->commandid = HDCP1_SET_ENC;
	set_enc_req->enable = enable;
	set_enc_rsp = (struct hdcp1_set_enc_rsp *)(handle->sbuf +
		   QSEECOM_ALIGN(sizeof(struct hdcp1_set_enc_req)));
	rc = qseecom_send_command(
		handle, set_enc_req, QSEECOM_ALIGN(sizeof(struct hdcp1_set_enc_req)),
		set_enc_rsp, QSEECOM_ALIGN(sizeof(struct hdcp1_set_enc_rsp)));

	if (rc < 0) {
		pr_err("qseecom cmd failed err=%d\n", rc);
		return -EINVAL;
	}

	rc = set_enc_rsp->ret;
	if (rc) {
		pr_err("enc cmd failed, rsp=%d\n", set_enc_rsp->ret);
		return -EINVAL;
	}

	pr_debug("success\n");
	return 0;
}

int hdcp1_ops_notify_qseecom(void *data, void *topo, bool is_authenticated)
{
	int rc = 0;
	struct hdcp1_ops_notify_req *ops_notify_req;
	struct hdcp1_ops_notify_rsp *ops_notify_rsp;
	struct hdcp1_qsee_handle *hdcp1_handle = data;
	struct qseecom_handle *handle = NULL;
	struct hdcp1_topology *topology = NULL;

	if (!hdcp1_handle || !hdcp1_handle->hdcpops_handle) {
		pr_err("invalid HDCP 1.x ops handle\n");
		return -EINVAL;
	}

	if (!hdcp1_handle->feature_supported) {
		pr_err("HDCP 1.x not supported\n");
		return -EINVAL;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", HDCP1OPS_APP_NAME);
		return -EINVAL;
	}

	handle = hdcp1_handle->hdcpops_handle;
	topology = (struct hdcp1_topology *)topo;

	/* set keys and request aksv */
	ops_notify_req = (struct hdcp1_ops_notify_req *)handle->sbuf;
	ops_notify_req->commandid = HDCP1_NOTIFY_TOPOLOGY;
	ops_notify_req->device_type = DEVICE_TYPE_DP;
	ops_notify_req->is_authenticated = is_authenticated;
	ops_notify_req->topology.depth = topology->depth;
	ops_notify_req->topology.device_count = topology->device_count;
	ops_notify_req->topology.max_devices_exceeded =
		topology->max_devices_exceeded;
	ops_notify_req->topology.max_cascade_exceeded =
		topology->max_cascade_exceeded;

	/*
	 * For hdcp1.4 below two nodes are not applicable but as
	 * TZ ops ta talks with other drivers with same structure
	 * and want to maintain same interface across hdcp versions,
	 * we are setting the values to 0.
	 */
	ops_notify_req->topology.hdcp2LegacyDeviceDownstream = 0;
	ops_notify_req->topology.hdcp1DeviceDownstream = 0;

	memset(ops_notify_req->recv_id_list, 0,
		   sizeof(uint8_t) * MAX_REC_ID_LIST_SIZE);

	ops_notify_rsp =
		(struct hdcp1_ops_notify_rsp *)(handle->sbuf +
		  QSEECOM_ALIGN(sizeof(struct hdcp1_ops_notify_req)));
	rc = qseecom_send_command(
		handle, ops_notify_req,
		QSEECOM_ALIGN(sizeof(struct hdcp1_ops_notify_req)), ops_notify_rsp,
		QSEECOM_ALIGN(sizeof(struct hdcp1_ops_notify_rsp)));

	rc = ops_notify_rsp->ret;
	if (rc < 0) {
		pr_warn("Ops notify cmd failed, rsp=%d\n", ops_notify_rsp->ret);
		return -EINVAL;
	}

	pr_debug("ops notify success\n");
	return 0;
}

int hdcp1_start_qseecom(void *data, u32 *aksv_msb, u32 *aksv_lsb)
{
	int rc = 0;
	struct hdcp1_qsee_handle *handle = data;

	if (!aksv_msb || !aksv_lsb) {
		pr_err("invalid aksv output buffer\n");
		rc = -EINVAL;
		goto error;
	}

	if (!handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	if (!handle->feature_supported) {
		pr_err("feature not supported\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_debug("%s app already loaded\n", handle->app_name);
		goto error;
	}

	rc = hdcp1_app_load(handle);
	if (rc)
		goto error;

	rc = hdcp1_set_key(handle, aksv_msb, aksv_lsb);
	if (rc)
		goto key_error;

	pr_debug("success\n");
	return rc;

key_error:
	hdcp1_app_unload(handle);
error:
	return rc;
}

void hdcp1_stop_qseecom(void *data)
{
	struct hdcp1_qsee_handle *hdcp1_handle = data;

	if (!hdcp1_handle || !hdcp1_handle->qseecom_handle ||
		!hdcp1_handle->hdcpops_handle) {
		pr_err("invalid handle\n");
		return;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_debug("%s app not loaded\n", hdcp1_handle->app_name);
		return;
	}

	hdcp1_app_unload(hdcp1_handle);
}

static int hdcp2_app_init_legacy(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(init_v1);

	if (!handle->legacy_app) {
		pr_err("wrong init function\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("library already loaded\n");
		goto error;
	}

	rc = hdcp2_app_process_cmd(init_v1);
	if (rc)
		goto error;

	pr_debug("success\n");
error:
	return rc;
}

static int hdcp2_app_tx_init_legacy(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(tx_init_v1);

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("app not loaded\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_TXMTR_INIT) {
		pr_err("txmtr already initialized\n");
		goto error;
	}

	rc = hdcp2_app_process_cmd(tx_init_v1);
	if (rc)
		goto error;

	handle->app_data.response.data = rsp_buf->message;
	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;

	handle->tz_ctxhandle = rsp_buf->ctxhandle;
	handle->hdcp_state |= HDCP_STATE_TXMTR_INIT;

	pr_debug("success\n");
error:
	return rc;
}

static int hdcp2_app_init(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;
	uint32_t app_minor_version = 0;

	hdcp2_app_init_var(init);

	if (handle->legacy_app) {
		pr_err("wrong init function\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("library already loaded\n");
		goto error;
	}

	req_buf->clientversion = HDCP_CLIENT_MAKE_VERSION(
		HDCP_CLIENT_MAJOR_VERSION, HDCP_CLIENT_MINOR_VERSION,
		HDCP_CLIENT_PATCH_VERSION);

	rc = hdcp2_app_process_cmd(init);
	if (rc)
		goto error;

	app_minor_version = HCDP_TXMTR_GET_MINOR_VERSION(rsp_buf->appversion);
	if (app_minor_version != HDCP_CLIENT_MINOR_VERSION) {
		pr_err("client-app minor version mismatch app(%d), client(%d)\n",
			   app_minor_version, HDCP_CLIENT_MINOR_VERSION);
		rc = -1;
		goto error;
	}

	pr_debug("success\n");

	pr_debug("client version major(%d), minor(%d), patch(%d)\n",
			 HDCP_CLIENT_MAJOR_VERSION, HDCP_CLIENT_MINOR_VERSION,
			 HDCP_CLIENT_PATCH_VERSION);

	pr_debug("app version major(%d), minor(%d), patch(%d)\n",
			 HCDP_TXMTR_GET_MAJOR_VERSION(rsp_buf->appversion),
			 HCDP_TXMTR_GET_MINOR_VERSION(rsp_buf->appversion),
			 HCDP_TXMTR_GET_PATCH_VERSION(rsp_buf->appversion));
error:
	return rc;
}

static int hdcp2_app_tx_init(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(tx_init);

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_TXMTR_INIT) {
		pr_err("txmtr already initialized\n");
		goto error;
	}

	req_buf->sessionid = handle->session_id;

	rc = hdcp2_app_process_cmd(tx_init);
	if (rc)
		goto error;

	handle->tz_ctxhandle = rsp_buf->ctxhandle;
	handle->hdcp_state |= HDCP_STATE_TXMTR_INIT;

	pr_debug("success\n");
error:
	return rc;
}

static int hdcp_get_version(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;
	uint32_t app_major_version = 0;

	hdcp2_app_init_var(version);

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("library already loaded\n");
		goto error;
	}

	rc = hdcp2_app_process_cmd(version);
	if (rc)
		goto error;

	app_major_version = HCDP_TXMTR_GET_MAJOR_VERSION(rsp_buf->appversion);

	pr_debug("hdp2p2 app major version %d, app version %d\n", app_major_version,
			 rsp_buf->appversion);

	if (app_major_version == 1)
		handle->legacy_app = true;

error:
	return rc;
}

static int hdcp2_app_load(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("%s app already loaded\n", handle->app_name);
		goto error;
	}

	if (!qseecom_handle_g) {
		rc = qseecom_start_app(&qseecom_handle_g,
			 handle->app_name, QSEECOM_SBUFF_SIZE);
		if (rc) {
			pr_err("qseecom_start_app failed for HDCP2P2 (%d)\n", rc);
			goto error;
		}
	}

	handle->qseecom_handle = qseecom_handle_g;

	if (!hdcpsrm_qseecom_handle_g) {
		rc = qseecom_start_app(&hdcpsrm_qseecom_handle_g,
			 HDCPSRM_APP_NAME, QSEECOM_SBUFF_SIZE);
		if (rc) {
			pr_err("qseecom_start_app failed for HDCPSRM (%d)\n", rc);
			goto hdcpsrm_error;
		}
	}

	handle->hdcpsrm_qseecom_handle = hdcpsrm_qseecom_handle_g;
	pr_debug("qseecom_start_app success\n");

	rc = hdcp_get_version(handle);
	if (rc) {
		pr_err("library get version failed\n");
		goto get_version_error;
	}

	if (handle->legacy_app) {
		handle->app_init = hdcp2_app_init_legacy;
		handle->tx_init = hdcp2_app_tx_init_legacy;
	} else {
		handle->app_init = hdcp2_app_init;
		handle->tx_init = hdcp2_app_tx_init;
	}

	if (!hdcp2_app_started) {
		rc = handle->app_init(handle);
		if (rc) {
			pr_err("app init failed\n");
			goto get_version_error;
		}
	}

	hdcp2_app_started++;

	handle->hdcp_state |= HDCP_STATE_APP_LOADED;
	return rc;
get_version_error:
	if (!hdcp2_app_started) {
		qseecom_shutdown_app(&hdcpsrm_qseecom_handle_g);
		hdcpsrm_qseecom_handle_g = NULL;
	}
	handle->hdcpsrm_qseecom_handle = NULL;
hdcpsrm_error:
	if (!hdcp2_app_started) {
		qseecom_shutdown_app(&qseecom_handle_g);
		qseecom_handle_g = NULL;
	}
	handle->qseecom_handle = NULL;
error:
	return rc;
}

static int hdcp2_app_session_init(struct hdcp2_qsee_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(session_init);

	if (!handle->qseecom_handle || !handle->qseecom_handle->sbuf) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", handle->app_name);
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_SESSION_INIT) {
		pr_err("session already initialized\n");
		goto error;
	}

	req_buf->deviceid = handle->device_type;

	rc = hdcp2_app_process_cmd(session_init);
	if (rc)
		goto error;

	pr_debug("session id %d\n", rsp_buf->sessionid);

	handle->session_id = rsp_buf->sessionid;
	handle->hdcp_state |= HDCP_STATE_SESSION_INIT;

	pr_debug("success\n");
error:
	return rc;
}

void *hdcp2_init_qseecom(u32 device_type)
{
	struct hdcp2_qsee_handle *handle =
		kzalloc(sizeof(struct hdcp2_qsee_handle), GFP_KERNEL);

	if (!handle)
		goto error;

	handle->device_type = device_type;
	handle->app_name = HDCP2P2_APP_NAME;

	handle->res_buf = kmalloc(QSEECOM_SBUFF_SIZE, GFP_KERNEL);
	if (!handle->res_buf) {
		kfree_sensitive(handle);
		return NULL;
	}

	handle->req_buf = kmalloc(QSEECOM_SBUFF_SIZE, GFP_KERNEL);
	if (!handle->req_buf) {
		kfree_sensitive(handle->res_buf);
		kfree_sensitive(handle);
		return NULL;
	}

	handle->app_data.request.data =  handle->req_buf;
	handle->app_data.response.data = handle->res_buf;
error:
	return handle;
}

void hdcp2_deinit_qseecom(void *ctx)
{
	struct hdcp2_qsee_handle *handle = NULL;
	int rc = 0;

	handle = ctx;

	if (!handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	kfree_sensitive(handle->res_buf);
	kfree_sensitive(handle->req_buf);

error:
	kfree_sensitive(ctx);
}

int hdcp2_app_start_qseecom(void *ctx, uint32_t req_len)
{
	struct hdcp2_qsee_handle *handle = NULL;
	int rc = 0;

	handle = ctx;

	if (!handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	rc = hdcp2_app_load(handle);
	if (rc)
		goto error;

	if (!handle->legacy_app) {
		rc = hdcp2_app_session_init(handle);
		if (rc)
			goto error;
	}

	if (handle->tx_init == NULL) {
		pr_err("invalid txmtr init function pointer\n");
		rc = -EINVAL;
		goto error;
	}

	rc = handle->tx_init(handle);

error:
	return rc;
}

int hdcp2_app_start_auth_qseecom(void *ctx, uint32_t req_len)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	hdcp2_app_init_var(start_auth);

	if (!handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->ctxHandle = handle->tz_ctxhandle;

	rc = hdcp2_app_process_cmd(start_auth);
	if (rc)
		goto error;

	memcpy(handle->res_buf, rsp_buf->message, rsp_buf->msglen);

	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
	handle->app_data.repeater_flag = false;

	handle->tz_ctxhandle = rsp_buf->ctxhandle;

	pr_debug("success\n");
error:
	return rc;
}

int hdcp2_app_process_msg_qseecom(void *ctx, uint32_t req_len)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	hdcp2_app_init_var(rcvd_msg);

	if (!handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	if (!handle->app_data.request.data) {
		pr_err("invalid request buffer\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->msglen = handle->app_data.request.length;
	req_buf->ctxhandle = handle->tz_ctxhandle;

	memcpy(req_buf->msg, handle->req_buf, handle->app_data.request.length);

	rc = hdcp2_app_process_cmd(rcvd_msg);
	if (rc)
		goto error;

	/* check if it's a repeater */
	if (rsp_buf->flag == HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST)
		handle->app_data.repeater_flag = true;

	memcpy(handle->res_buf, rsp_buf->msg, rsp_buf->msglen);

	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;

error:
	return rc;
}

int hdcp2_app_timeout_qseecom(void *ctx, uint32_t req_len)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	hdcp2_app_init_var(send_timeout);

	if (!handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	rc = hdcp2_app_process_cmd(send_timeout);
	if (rc)
		goto error;

	memcpy(handle->res_buf, rsp_buf->message, rsp_buf->msglen);

	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
error:
	return rc;
}

int hdcp2_app_enable_encryption_qseecom(void *ctx, uint32_t req_len)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	hdcp2_app_init_var(set_hw_key);

	if (!handle) {
		pr_err("Invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	/*
	 * wait at least 200ms before enabling encryption
	 * as per hdcp2p2 specifications.
	 */
	msleep(SLEEP_SET_HW_KEY_MS);

	req_buf->ctxhandle = handle->tz_ctxhandle;

	rc = hdcp2_app_process_cmd(set_hw_key);
	if (rc)
		goto error;

	handle->hdcp_state |= HDCP_STATE_AUTHENTICATED;
error:
	return rc;
}

int hdcp2_app_query_stream_qseecom(void *ctx, uint32_t req_len)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	hdcp2_app_init_var(query_stream_type);

	if (!handle) {
		pr_err("Invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	req_buf->ctxhandle = handle->tz_ctxhandle;

	rc = hdcp2_app_process_cmd(query_stream_type);
	if (rc)
		goto error;

	memcpy(handle->res_buf, rsp_buf->msg, rsp_buf->msglen);

	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
error:
	return rc;
}

int hdcp2_app_stop_qseecom(void *ctx)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	rc = hdcp2_app_tx_deinit(handle);
	if (rc)
		goto error;

	if (!handle->legacy_app) {
		rc = hdcp2_app_session_deinit(handle);
		if (rc)
			goto error;
	}

	rc = hdcp2_app_unload(handle);
error:
	return rc;
}

bool hdcp2_feature_supported_qseecom(void *ctx)
{
	int rc = 0;
	bool supported = false;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->feature_supported) {
		supported = true;
		goto error;
	}

	rc = hdcp2_app_load(handle);
	if (!rc) {
		if (!hdcp2_verify_key(handle)) {
			pr_debug("HDCP 2.2 supported\n");
			handle->feature_supported = true;
			supported = true;
		}
		hdcp2_app_unload(handle);
	}
error:
	return supported;
}

int hdcp2_force_encryption_qseecom(void *ctx, uint32_t enable)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	hdcp2_app_init_var(force_encryption);

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state == HDCP_STATE_AUTHENTICATED)
		msleep(SLEEP_FORCE_ENCRYPTION_MS);

	req_buf->ctxhandle = handle->tz_ctxhandle;
	req_buf->enable = enable;

	rc = hdcp2_app_process_cmd(force_encryption);
	if (rc || (rsp_buf->commandid != hdcp_cmd_force_encryption))
		goto error;

error:
	return rc;
}

int hdcp2_open_stream_qseecom(void *ctx, uint8_t vc_payload_id,
		uint8_t stream_number, uint32_t *stream_id)
{
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;
	int rc = 0;

	hdcp2_app_init_var(session_open_stream);

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->sessionid = handle->session_id;
	req_buf->vcpayloadid = vc_payload_id;
	req_buf->stream_number = stream_number;
	req_buf->stream_media_type = 0;

	rc = hdcp2_app_process_cmd(session_open_stream);
	if (rc)
		goto error;

	*stream_id = rsp_buf->streamid;

	pr_debug("success\n");

error:
	return rc;
}

int hdcp2_close_stream_qseecom(void *ctx, uint32_t stream_id)
{
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;
	int rc = 0;

	hdcp2_app_init_var(session_close_stream);

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->sessionid = handle->session_id;
	req_buf->streamid = stream_id;

	rc = hdcp2_app_process_cmd(session_close_stream);
	if (rc)
		goto error;

	pr_debug("success\n");
error:
	return rc;
}

int hdcp2_update_app_data_qseecom(void *ctx, struct hdcp2_app_data *app_data)
{
	int rc = 0;
	struct hdcp2_qsee_handle *handle = (struct hdcp2_qsee_handle *)ctx;

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	app_data->request.data = handle->app_data.request.data;
	app_data->request.length = handle->app_data.request.length;
	app_data->response.data = handle->app_data.response.data;
	app_data->response.length = handle->app_data.response.length;
	app_data->timeout = handle->app_data.timeout;
	app_data->repeater_flag = handle->app_data.repeater_flag;

error:
	return rc;
}
