// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[hdcp-qseecom] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/hdcp_qseecom.h>
#include <linux/kthread.h>

#include "qseecom_kernel.h"

#define HDCP2P2_APP_NAME      "hdcp2p2"
#define HDCP1_APP_NAME        "hdcp1"
#define HDCPSRM_APP_NAME      "hdcpsrm"
#define QSEECOM_SBUFF_SIZE    0x1000

#define MAX_TX_MESSAGE_SIZE	129
#define MAX_RX_MESSAGE_SIZE	534
#define MAX_TOPOLOGY_ELEMS	32
#define HDCP1_AKSV_SIZE         8

#define HDCP1_SET_KEY       202
#define HDCP1_KEY_VERIFY    204
#define HDCP1_SET_ENC       205

#define BITS_8_IN_BYTES       1
#define BITS_16_IN_BYTES      2
#define BITS_24_IN_BYTES      3
#define BITS_32_IN_BYTES      4
#define BITS_40_IN_BYTES      5
#define BITS_64_IN_BYTES      8
#define BITS_128_IN_BYTES    16
#define BITS_160_IN_BYTES    20
#define BITS_256_IN_BYTES    32
#define BITS_1024_IN_BYTES  128
#define BITS_3072_IN_BYTES  384
#define TXCAPS_SIZE           3
#define RXCAPS_SIZE           3
#define RXINFO_SIZE           2
#define SEQ_NUM_V_SIZE        3

#define RCVR_ID_SIZE BITS_40_IN_BYTES
#define MAX_RCVR_IDS_ALLOWED_IN_LIST 31
#define MAX_RCVR_ID_LIST_SIZE \
		(RCVR_ID_SIZE*MAX_RCVR_IDS_ALLOWED_IN_LIST)
/*
 * minimum wait as per standard is 200 ms. keep it 300 ms
 * to be on safe side.
 */
#define SLEEP_SET_HW_KEY_MS 300

/* Wait 200ms after authentication */
#define SLEEP_FORCE_ENCRYPTION_MS 200

/* hdcp command status */
#define HDCP_SUCCESS                    0

const char *HdcpErrors[] = {
	"HDCP_SUCCESS",
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
	"HDCP_STREAMNUMBER_INUSE"
};

/* flags set by tz in response message */
#define HDCP_TXMTR_SUBSTATE_INIT                              0
#define HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST       1
#define HDCP_TXMTR_SUBSTATE_PROCESSED_RECIEVERID_LIST         2
#define HDCP_TXMTR_SUBSTATE_WAITING_FOR_STREAM_READY_MESSAGE  3
#define HDCP_TXMTR_SUBSTATE_REPEATER_AUTH_COMPLETE            4

#define HDCP_DEVICE_ID                         0x0008000
#define HDCP_CREATE_DEVICE_ID(x)               (HDCP_DEVICE_ID | (x))

#define HDCP_TXMTR_HDMI                        HDCP_CREATE_DEVICE_ID(1)

#define HDCP_TXMTR_SERVICE_ID                 0x0001000
#define SERVICE_CREATE_CMD(x)                 (HDCP_TXMTR_SERVICE_ID | x)

#define HCDP_TXMTR_GET_MAJOR_VERSION(v) (((v) >> 16) & 0xFF)
#define HCDP_TXMTR_GET_MINOR_VERSION(v) (((v) >> 8) & 0xFF)
#define HCDP_TXMTR_GET_PATCH_VERSION(v) ((v) & 0xFF)

#define HDCP_CLIENT_MAJOR_VERSION 2
#define HDCP_CLIENT_MINOR_VERSION 1
#define HDCP_CLIENT_PATCH_VERSION 0
#define HDCP_CLIENT_MAKE_VERSION(maj, min, patch) \
	((((maj) & 0xFF) << 16) | (((min) & 0xFF) << 8) | ((patch) & 0xFF))

#define hdcp2_app_init_var(x) \
	struct hdcp_##x##_req *req_buf = NULL; \
	struct hdcp_##x##_rsp *rsp_buf = NULL; \
	if (!handle->qseecom_handle) { \
		pr_err("invalid qseecom_handle while processing %s\n", #x); \
		rc = -EINVAL; \
		goto error; \
	} \
	req_buf = (struct hdcp_##x##_req *) handle->qseecom_handle->sbuf; \
	rsp_buf = (struct hdcp_##x##_rsp *) (handle->qseecom_handle->sbuf + \
		QSEECOM_ALIGN(sizeof(struct hdcp_##x##_req))); \
	req_buf->commandid = hdcp_cmd_##x


#define hdcp2_app_process_cmd(x) \
({ \
	int rc = qseecom_send_command(handle->qseecom_handle, \
		req_buf, QSEECOM_ALIGN(sizeof(struct hdcp_##x##_req)), \
		rsp_buf, QSEECOM_ALIGN(sizeof(struct hdcp_##x##_rsp))); \
	if ((rc < 0) || (rsp_buf->status != HDCP_SUCCESS)) { \
		pr_err("qseecom cmd %s failed with err = %d, status = %d:%s\n" \
		, #x, rc, rsp_buf->status, \
		hdcp_cmd_status_to_str(rsp_buf->status)); \
		rc = -EINVAL; \
	} \
	rc; \
})

enum {
	hdcp_cmd_tx_init           = SERVICE_CREATE_CMD(1),
	hdcp_cmd_tx_init_v1        = SERVICE_CREATE_CMD(1),
	hdcp_cmd_tx_deinit         = SERVICE_CREATE_CMD(2),
	hdcp_cmd_rcvd_msg          = SERVICE_CREATE_CMD(3),
	hdcp_cmd_send_timeout      = SERVICE_CREATE_CMD(4),
	hdcp_cmd_set_hw_key        = SERVICE_CREATE_CMD(5),
	hdcp_cmd_query_stream_type = SERVICE_CREATE_CMD(6),
	hdcp_cmd_init_v1           = SERVICE_CREATE_CMD(11),
	hdcp_cmd_init              = SERVICE_CREATE_CMD(11),
	hdcp_cmd_deinit            = SERVICE_CREATE_CMD(12),
	hdcp_cmd_version           = SERVICE_CREATE_CMD(14),
	hdcp_cmd_verify_key        = SERVICE_CREATE_CMD(15),
	hdcp_cmd_session_init      = SERVICE_CREATE_CMD(16),
	hdcp_cmd_session_deinit    = SERVICE_CREATE_CMD(17),
	hdcp_cmd_start_auth        = SERVICE_CREATE_CMD(18),
	hdcp_cmd_session_open_stream = SERVICE_CREATE_CMD(20),
	hdcp_cmd_session_close_stream = SERVICE_CREATE_CMD(21),
	hdcp_cmd_force_encryption  = SERVICE_CREATE_CMD(22),
};

enum hdcp_state {
	HDCP_STATE_INIT = 0x00,
	HDCP_STATE_APP_LOADED = 0x01,
	HDCP_STATE_SESSION_INIT = 0x02,
	HDCP_STATE_TXMTR_INIT = 0x04,
	HDCP_STATE_AUTHENTICATED = 0x08,
	HDCP_STATE_ERROR = 0x10
};

enum hdcp_element {
	HDCP_TYPE_UNKNOWN,
	HDCP_TYPE_RECEIVER,
	HDCP_TYPE_REPEATER,
};

enum hdcp_version {
	HDCP_VERSION_UNKNOWN,
	HDCP_VERSION_2_2,
	HDCP_VERSION_1_4
};

struct receiver_info {
	unsigned char rcvrInfo[RCVR_ID_SIZE];
	enum hdcp_element elem_type;
	enum hdcp_version hdcp_version;
};

struct topology_info {
	unsigned int nNumRcvrs;
	struct receiver_info rcvinfo[MAX_TOPOLOGY_ELEMS];
};

struct __attribute__ ((__packed__)) hdcp1_key_set_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp1_key_set_rsp {
	uint32_t commandid;
	uint32_t ret;
	uint8_t ksv[HDCP1_AKSV_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_version_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_version_rsp {
	uint32_t status;
	uint32_t commandId;
	uint32_t appversion;
};

struct __attribute__ ((__packed__)) hdcp_verify_key_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_verify_key_rsp {
	uint32_t status;
	uint32_t commandId;
};

struct __attribute__ ((__packed__)) hdcp1_key_verify_req {
	uint32_t commandid;
	uint32_t key_type;
};

struct __attribute__ ((__packed__)) hdcp1_key_verify_rsp {
	uint32_t commandId;
	uint32_t ret;
};

struct __attribute__ ((__packed__)) hdcp_init_v1_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_init_v1_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_init_req {
	uint32_t commandid;
	uint32_t clientversion;
};

struct __attribute__ ((__packed__)) hdcp_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t appversion;
};

struct __attribute__ ((__packed__)) hdcp_deinit_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_session_init_req {
	uint32_t commandid;
	uint32_t deviceid;
};

struct __attribute__ ((__packed__)) hdcp_session_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t sessionid;
};

struct __attribute__ ((__packed__)) hdcp_session_deinit_req {
	uint32_t commandid;
	uint32_t sessionid;
};

struct __attribute__ ((__packed__)) hdcp_session_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_tx_init_v1_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_tx_init_v1_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_tx_init_req {
	uint32_t commandid;
	uint32_t sessionid;
};

struct __attribute__ ((__packed__)) hdcp_tx_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_tx_deinit_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_tx_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_rcvd_msg_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t msglen;
	uint8_t msg[MAX_RX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_rcvd_msg_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t state;
	uint32_t timeout;
	uint32_t flag;
	uint32_t msglen;
	uint8_t msg[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_set_hw_key_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_set_hw_key_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_send_timeout_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_send_timeout_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_query_stream_type_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_query_stream_type_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t msg[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_set_stream_type_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint8_t streamtype;
};

struct __attribute__ ((__packed__)) hdcp_set_stream_type_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__ ((__packed__)) hdcp_update_srm_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t srmoffset;
	uint32_t srmlength;
};

struct __attribute__ ((__packed__)) hdcp_update_srm_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_get_topology_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_get_topology_rsp {
	uint32_t status;
	uint32_t commandid;
	struct topology_info topologyinfo;
};

struct __attribute__ ((__packed__)) rxvr_info_struct {
	uint8_t rcvrCert[522];
	uint8_t rrx[BITS_64_IN_BYTES];
	uint8_t rxcaps[RXCAPS_SIZE];
	bool repeater;
};

struct __attribute__ ((__packed__)) repeater_info_struct {
	uint8_t RxInfo[RXINFO_SIZE];
	uint8_t seq_num_V[SEQ_NUM_V_SIZE];
	bool seq_num_V_Rollover_flag;
	uint8_t ReceiverIDList[MAX_RCVR_ID_LIST_SIZE];
	uint32_t ReceiverIDListLen;
};

struct __attribute__ ((__packed__)) hdcp1_set_enc_req {
	uint32_t commandid;
	uint32_t enable;
};

struct __attribute__ ((__packed__)) hdcp1_set_enc_rsp {
	uint32_t commandid;
	uint32_t ret;
};

struct __attribute__ ((__packed__)) hdcp_start_auth_req {
	uint32_t commandid;
	uint32_t ctxHandle;
};

struct __attribute__ ((__packed__)) hdcp_start_auth_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t message[MAX_TX_MESSAGE_SIZE];
};

struct __attribute__((__packed__)) hdcp_session_open_stream_req {
	uint32_t commandid;
	uint32_t sessionid;
	uint32_t vcpayloadid;
	uint32_t stream_number;
	uint32_t streamMediaType;
};

struct __attribute__((__packed__)) hdcp_session_open_stream_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t streamid;
};

struct __attribute__((__packed__)) hdcp_session_close_stream_req {
	uint32_t commandid;
	uint32_t sessionid;
	uint32_t streamid;
};

struct __attribute__((__packed__)) hdcp_session_close_stream_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct __attribute__((__packed__)) hdcp_force_encryption_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t enable;
};

struct __attribute__ ((__packed__)) hdcp_force_encryption_rsp {
	uint32_t status;
	uint32_t commandid;
};

struct hdcp2_handle {
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

	int (*app_init)(struct hdcp2_handle *handle);
	int (*tx_init)(struct hdcp2_handle *handle);
};

/*
 * struct hdcp1_handle - handle for HDCP 1.x client
 * @qseecom_handle - for sending commands to qseecom
 * @feature_supported - set to true if the platform supports HDCP 1.x
 * @device_type - the interface type (HDMI or DisplayPort)
 */
struct hdcp1_handle {
	struct qseecom_handle *qseecom_handle;
	bool feature_supported;
	uint32_t device_type;
	enum hdcp_state hdcp_state;
	char *app_name;
};

#define HDCP_CMD_STATUS_TO_STR(x) #x
static const char *hdcp_cmd_status_to_str(uint32_t err)
{
	int len = ARRAY_SIZE(HdcpErrors);

	if (err >= 0 && err < len)
		return HdcpErrors[err];
	else
		return "";
}

static int hdcp_get_version(struct hdcp2_handle *handle)
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

	pr_debug("hdp2p2 app major version %d, app version %d\n",
		 app_major_version, rsp_buf->appversion);

	if (app_major_version == 1)
		handle->legacy_app = true;
error:
	return rc;
}

static int hdcp2_app_init_legacy(struct hdcp2_handle *handle)
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

static int hdcp2_app_init(struct hdcp2_handle *handle)
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

	req_buf->clientversion =
	    HDCP_CLIENT_MAKE_VERSION(HDCP_CLIENT_MAJOR_VERSION,
				     HDCP_CLIENT_MINOR_VERSION,
				     HDCP_CLIENT_PATCH_VERSION);

	rc = hdcp2_app_process_cmd(init);
	if (rc)
		goto error;

	app_minor_version = HCDP_TXMTR_GET_MINOR_VERSION(rsp_buf->appversion);
	if (app_minor_version != HDCP_CLIENT_MINOR_VERSION) {
		pr_err
		    ("client-app minor version mismatch app(%d), client(%d)\n",
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

static int hdcp2_app_tx_init(struct hdcp2_handle *handle)
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

static int hdcp2_app_tx_init_legacy(struct hdcp2_handle *handle)
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

static int hdcp2_app_load(struct hdcp2_handle *handle)
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

	rc = qseecom_start_app(&handle->qseecom_handle,
		 handle->app_name, QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_err("qseecom_start_app failed for HDCP2P2 (%d)\n", rc);
		goto error;
	}

	rc = qseecom_start_app(&handle->hdcpsrm_qseecom_handle,
		 HDCPSRM_APP_NAME, QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_err("qseecom_start_app failed for HDCPSRM (%d)\n", rc);
		goto hdcpsrm_error;
	}

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

	rc = handle->app_init(handle);
	if (rc) {
		pr_err("app init failed\n");
		goto get_version_error;
	}

	handle->hdcp_state |= HDCP_STATE_APP_LOADED;

	return rc;
get_version_error:
	qseecom_shutdown_app(&handle->hdcpsrm_qseecom_handle);
hdcpsrm_error:
	qseecom_shutdown_app(&handle->qseecom_handle);
error:
	return rc;
}

static int hdcp2_app_unload(struct hdcp2_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(deinit);

	hdcp2_app_process_cmd(deinit);

	/* deallocate the resources for qseecom HDCPSRM handle */
	rc = qseecom_shutdown_app(&handle->hdcpsrm_qseecom_handle);
	if (rc)
		pr_err("qseecom_shutdown_app failed for HDCPSRM (%d)\n", rc);

	/* deallocate the resources for qseecom HDCP2P2 handle */
	rc = qseecom_shutdown_app(&handle->qseecom_handle);
	if (rc) {
		pr_err("qseecom_shutdown_app failed for HDCP2P2 (%d)\n", rc);
		return rc;
	}

	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
	pr_debug("%s app unloaded\n", handle->app_name);

	return rc;
error:
	qseecom_shutdown_app(&handle->hdcpsrm_qseecom_handle);
	return rc;
}

static int hdcp2_verify_key(struct hdcp2_handle *handle)
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

bool hdcp2_feature_supported(void *data)
{
	int rc = 0;
	bool supported = false;
	struct hdcp2_handle *handle = data;

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

static int hdcp2_app_session_init(struct hdcp2_handle *handle)
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

static int hdcp2_app_session_deinit(struct hdcp2_handle *handle)
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

static int hdcp2_app_tx_deinit(struct hdcp2_handle *handle)
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

static int hdcp2_app_start_auth(struct hdcp2_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(start_auth);

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

	handle->app_data.response.data = rsp_buf->message;
	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
	handle->app_data.repeater_flag = false;

	handle->tz_ctxhandle = rsp_buf->ctxhandle;

	pr_debug("success\n");
error:
	return rc;
}

static int hdcp2_app_start(struct hdcp2_handle *handle)
{
	int rc = 0;

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

static int hdcp2_app_stop(struct hdcp2_handle *handle)
{
	int rc = 0;

	rc = hdcp2_app_tx_deinit(handle);
	if (rc)
		goto end;

	if (!handle->legacy_app) {
		rc = hdcp2_app_session_deinit(handle);
		if (rc)
			goto end;
	}

	rc = hdcp2_app_unload(handle);
end:
	return rc;
}

static int hdcp2_app_process_msg(struct hdcp2_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(rcvd_msg);

	if (!handle->app_data.request.data) {
		pr_err("invalid request buffer\n");
		rc = -EINVAL;
		goto error;
	}

	req_buf->msglen = handle->app_data.request.length;
	req_buf->ctxhandle = handle->tz_ctxhandle;

	rc = hdcp2_app_process_cmd(rcvd_msg);
	if (rc)
		goto error;

	/* check if it's a repeater */
	if (rsp_buf->flag == HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST)
		handle->app_data.repeater_flag = true;

	handle->app_data.response.data = rsp_buf->msg;
	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
error:
	return rc;
}

static int hdcp2_app_timeout(struct hdcp2_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(send_timeout);

	rc = hdcp2_app_process_cmd(send_timeout);
	if (rc)
		goto error;

	handle->app_data.response.data = rsp_buf->message;
	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
error:
	return rc;
}

static int hdcp2_app_enable_encryption(struct hdcp2_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(set_hw_key);

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

	pr_debug("success\n");
	return rc;
error:
	return rc;
}

int hdcp2_force_encryption_utility(struct hdcp2_handle *handle, uint32_t enable)
{
	int rc = 0;

	hdcp2_app_init_var(force_encryption);
	if (handle->hdcp_state == HDCP_STATE_AUTHENTICATED)
		msleep(SLEEP_FORCE_ENCRYPTION_MS);

	req_buf->ctxhandle = handle->tz_ctxhandle;
	req_buf->enable = enable;

	rc = hdcp2_app_process_cmd(force_encryption);
	if (rc || (rsp_buf->commandid != hdcp_cmd_force_encryption))
		goto error;

	return 0;
error:
	return rc;
}

int hdcp2_force_encryption(void *ctx, uint32_t enable)
{
	int rc = 0;
	struct hdcp2_handle *handle = NULL;

	if (!ctx) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	handle = ctx;
	rc = hdcp2_force_encryption_utility(handle, enable);
	if (rc)
		goto error;

	pr_debug("success\n");
	return 0;
error:
	pr_err("failed, rc=%d\n", rc);
	return rc;
}
EXPORT_SYMBOL(hdcp2_force_encryption);

static int hdcp2_app_query_stream(struct hdcp2_handle *handle)
{
	int rc = 0;

	hdcp2_app_init_var(query_stream_type);

	req_buf->ctxhandle = handle->tz_ctxhandle;

	rc = hdcp2_app_process_cmd(query_stream_type);
	if (rc)
		goto error;

	handle->app_data.response.data = rsp_buf->msg;
	handle->app_data.response.length = rsp_buf->msglen;
	handle->app_data.timeout = rsp_buf->timeout;
error:
	return rc;
}

static unsigned char *hdcp2_get_recv_buf(struct hdcp2_handle *handle)
{
	struct hdcp_rcvd_msg_req *req_buf;

	req_buf = (struct hdcp_rcvd_msg_req *)(handle->qseecom_handle->sbuf);
	return req_buf->msg;
}

int hdcp2_app_comm(void *ctx, enum hdcp2_app_cmd cmd,
		struct hdcp2_app_data *app_data)
{
	struct hdcp2_handle *handle = NULL;
	int rc = 0;

	if (!ctx || !app_data) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	handle = ctx;
	handle->app_data.request.length = app_data->request.length;

	pr_debug("command %s\n", hdcp2_app_cmd_str(cmd));

	switch (cmd) {
	case HDCP2_CMD_START:
		rc = hdcp2_app_start(handle);
		break;
	case HDCP2_CMD_START_AUTH:
		rc = hdcp2_app_start_auth(handle);
		break;
	case HDCP2_CMD_PROCESS_MSG:
		rc = hdcp2_app_process_msg(handle);
		break;
	case HDCP2_CMD_TIMEOUT:
		rc = hdcp2_app_timeout(handle);
		break;
	case HDCP2_CMD_EN_ENCRYPTION:
		rc = hdcp2_app_enable_encryption(handle);
		break;
	case HDCP2_CMD_QUERY_STREAM:
		rc = hdcp2_app_query_stream(handle);
		break;
	case HDCP2_CMD_STOP:
		rc = hdcp2_app_stop(handle);
	default:
		goto error;
	}

	if (rc)
		goto error;

	handle->app_data.request.data = hdcp2_get_recv_buf(handle);

	app_data->request.data = handle->app_data.request.data;
	app_data->request.length = handle->app_data.request.length;
	app_data->response.data = handle->app_data.response.data;
	app_data->response.length = handle->app_data.response.length;
	app_data->timeout = handle->app_data.timeout;
	app_data->repeater_flag = handle->app_data.repeater_flag;
error:
	return rc;
}
EXPORT_SYMBOL(hdcp2_app_comm);

static int hdcp2_open_stream_helper(struct hdcp2_handle *handle,
		uint8_t vc_payload_id,
		uint8_t stream_number,
		uint32_t *stream_id)
{
	int rc = 0;

	hdcp2_app_init_var(session_open_stream);

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
	req_buf->streamMediaType = 0;

	rc = hdcp2_app_process_cmd(session_open_stream);
	if (rc)
		goto error;

	*stream_id = rsp_buf->streamid;

	pr_debug("success\n");

error:
	return rc;
}

int hdcp2_open_stream(void *ctx, uint8_t vc_payload_id, uint8_t stream_number,
		uint32_t *stream_id)
{
	struct hdcp2_handle *handle = NULL;

	if (!ctx) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	handle = ctx;

	return hdcp2_open_stream_helper(handle, vc_payload_id, stream_number,
		stream_id);
}
EXPORT_SYMBOL(hdcp2_open_stream);

static int hdcp2_close_stream_helper(struct hdcp2_handle *handle,
		uint32_t stream_id)
{
	int rc = 0;

	hdcp2_app_init_var(session_close_stream);

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

int hdcp2_close_stream(void *ctx, uint32_t stream_id)
{
	struct hdcp2_handle *handle = NULL;

	if (!ctx) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	handle = ctx;

	return hdcp2_close_stream_helper(handle, stream_id);
}
EXPORT_SYMBOL(hdcp2_close_stream);

void *hdcp2_init(u32 device_type)
{
	struct hdcp2_handle *handle = NULL;

	handle = kzalloc(sizeof(struct hdcp2_handle), GFP_KERNEL);
	if (!handle)
		goto error;

	handle->device_type = device_type;
	handle->app_name = HDCP2P2_APP_NAME;
error:
	return handle;
}
EXPORT_SYMBOL(hdcp2_init);

void hdcp2_deinit(void *ctx)
{
	kzfree(ctx);
}
EXPORT_SYMBOL(hdcp2_deinit);

void *hdcp1_init(void)
{
	struct hdcp1_handle *handle =
		kzalloc(sizeof(struct hdcp1_handle), GFP_KERNEL);

	if (!handle)
		goto error;

	handle->app_name = HDCP1_APP_NAME;
error:
	return handle;
}

void hdcp1_deinit(void *data)
{
	kfree(data);
}

static int hdcp1_count_ones(u8 *array, u8 len)
{
	int i, j, count = 0;

	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);
	return count;
}

static int hdcp1_validate_aksv(u32 aksv_msb, u32 aksv_lsb)
{
	int const number_of_ones = 20;
	u8 aksv[5];

	pr_debug("AKSV=%02x%08x\n", aksv_msb, aksv_lsb);

	aksv[0] =  aksv_lsb        & 0xFF;
	aksv[1] = (aksv_lsb >> 8)  & 0xFF;
	aksv[2] = (aksv_lsb >> 16) & 0xFF;
	aksv[3] = (aksv_lsb >> 24) & 0xFF;
	aksv[4] =  aksv_msb        & 0xFF;

	/* check there are 20 ones in AKSV */
	if (hdcp1_count_ones(aksv, 5) != number_of_ones) {
		pr_err("AKSV bit count failed\n");
		return -EINVAL;
	}

	return 0;
}


static int hdcp1_set_key(struct hdcp1_handle *hdcp1_handle, u32 *aksv_msb,
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
	rc = qseecom_send_command(handle, key_set_req,
			QSEECOM_ALIGN(sizeof
				(struct hdcp1_key_set_req)),
			key_set_rsp,
			QSEECOM_ALIGN(sizeof
				(struct hdcp1_key_set_rsp)));

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

static int hdcp1_app_load(struct hdcp1_handle *handle)
{
	int rc = 0;

	if (!handle) {
		pr_err("invalid handle\n");
		goto error;
	}

	rc = qseecom_start_app(&handle->qseecom_handle, handle->app_name,
			QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_err("%s app load failed (%d)\n", handle->app_name, rc);
		goto error;
	}

	handle->hdcp_state |= HDCP_STATE_APP_LOADED;
	pr_debug("%s app loaded\n", handle->app_name);

error:
	return rc;
}

static void hdcp1_app_unload(struct hdcp1_handle *handle)
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

	/* deallocate the resources for qseecom HDCP 1.x handle */
	rc = qseecom_shutdown_app(&handle->qseecom_handle);
	if (rc) {
		pr_err("%s app unload failed (%d)\n", handle->app_name, rc);
		return;
	}

	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
	pr_debug("%s app unloaded\n", handle->app_name);
}

static int hdcp1_verify_key(struct hdcp1_handle *hdcp1_handle)
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
	key_verify_rsp = (struct hdcp1_key_verify_rsp *)(handle->sbuf +
			QSEECOM_ALIGN(sizeof(struct hdcp1_key_verify_req)));
	rc = qseecom_send_command(handle, key_verify_req,
			QSEECOM_ALIGN(sizeof
				(struct hdcp1_key_verify_req)),
			key_verify_rsp,
			QSEECOM_ALIGN(sizeof
				(struct hdcp1_key_set_rsp)));

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

bool hdcp1_feature_supported(void *data)
{
	bool supported = false;
	struct hdcp1_handle *handle = data;
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

int hdcp1_set_enc(void *data, bool enable)
{
	int rc = 0;
	struct hdcp1_set_enc_req *set_enc_req;
	struct hdcp1_set_enc_rsp *set_enc_rsp;
	struct hdcp1_handle *hdcp1_handle = data;
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
	rc = qseecom_send_command(handle, set_enc_req,
			QSEECOM_ALIGN(sizeof
				(struct hdcp1_set_enc_req)),
			set_enc_rsp,
			QSEECOM_ALIGN(sizeof
				(struct hdcp1_set_enc_rsp)));

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

int hdcp1_start(void *data, u32 *aksv_msb, u32 *aksv_lsb)
{
	int rc = 0;
	struct hdcp1_handle *hdcp1_handle = data;

	if (!aksv_msb || !aksv_lsb) {
		pr_err("invalid aksv output buffer\n");
		rc = -EINVAL;
		goto error;
	}

	if (!hdcp1_handle) {
		pr_err("invalid handle\n");
		rc = -EINVAL;
		goto error;
	}

	if (!hdcp1_handle->feature_supported) {
		pr_err("feature not supported\n");
		rc = -EINVAL;
		goto error;
	}

	if (hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_debug("%s app already loaded\n", hdcp1_handle->app_name);
		goto error;
	}

	rc = hdcp1_app_load(hdcp1_handle);
	if (rc)
		goto error;

	rc = hdcp1_set_key(hdcp1_handle, aksv_msb, aksv_lsb);
	if (rc)
		goto key_error;

	pr_debug("success\n");
	return rc;

key_error:
	hdcp1_app_unload(hdcp1_handle);
error:
	return rc;
}

void hdcp1_stop(void *data)
{
	struct hdcp1_handle *hdcp1_handle = data;

	if (!hdcp1_handle || !hdcp1_handle->qseecom_handle) {
		pr_err("invalid handle\n");
		return;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_debug("%s app not loaded\n", hdcp1_handle->app_name);
		return;
	}

	hdcp1_app_unload(hdcp1_handle);
}
