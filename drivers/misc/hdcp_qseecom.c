/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

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

/* hdcp command status */
#define HDCP_SUCCESS                    0
#define HDCP_FAIL                       1
#define HDCP_HMAC_FAIL                  2
#define HDCP_AES_FAIL                   3
#define HDCP_BAD_PARAM                  4
#define HDCP_HASH_FAIL                  5
#define HDCP_OUT_OF_MEM                 6
#define HDCP_WRONG_MESSAGE_ID           7
#define HDCP_UNSUPPORTED_TXMTR          8
#define HDCP_WRONG_TXMTR_CAPAB_MASK     9
#define HDCP_UNSUPPORTED_RCVR           10
#define HDCP_WRONG_RCVR_CAPAB_MASK      11
#define HDCP_MAXLC_RETRIES_TRIED        12
#define HDCP_STATE_UNAUTHENTICATED      13
#define HDCP_CE_FAIL                    14
#define HDCP_RSA_FAIL                   15
#define HDCP_GET_RANDOM_FAIL            16
#define HDCP_SFS_FAIL                   17
#define HDCP_HPRIME_MISMATCH            18
#define HDCP_RCVRID_NOT_FOUND           19
#define HDCP_WRONG_STATE                20
#define HDCP_WRONG_V_RETURNED           21
#define HDCP_MAX_DEVICE_EXCEEDED        22
#define HDCP_MAX_CASCADE_EXCEEDED       23
#define HDCP_V_ROLLOVER                 24
#define HDCP_V_MISMATCH                 25
#define HDCP_MPRIME_MISMATCH            26
#define HDCP_NO_RECEIVERIDLIST          27
#define HDCP_TIMEOUT_EXPIRED            28
#define HDCP_SRM_CHECK_FAILURE          29
#define HDCP_INVALID_TXMTR_KEY_FORMAT   30
#define HDCP_INVALID_TXMTR_CONTEXT      31
#define HDCP_INVALID_RCVR_CONTEXT       32
#define HDCP_GENERIC_PROV_FAIL          33
#define HDCP_CONFIG_FAIL                34
#define HDCP_KEY_ALREADY_PROVISIONED    35
#define HDCP_KEY_NOT_PROVISIONED        36
#define HDCP_CALL_TOO_SOON              37

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
		pr_err("qseecom cmd %s failed with err = %d, status = %s\n", \
			#x, rc, hdcp_cmd_status_to_str(rsp_buf->status)); \
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
	hdcp_cmd_session_init      = SERVICE_CREATE_CMD(16),
	hdcp_cmd_session_deinit    = SERVICE_CREATE_CMD(17),
	hdcp_cmd_start_auth        = SERVICE_CREATE_CMD(18),
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
};

#define HDCP_CMD_STATUS_TO_STR(x) #x
static const char *hdcp_cmd_status_to_str(uint32_t err)
{
	switch (err) {
	case HDCP_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_FAIL);
	case HDCP_HMAC_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_HMAC_FAIL);
	case HDCP_AES_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_AES_FAIL);
	case HDCP_BAD_PARAM:
		return HDCP_CMD_STATUS_TO_STR(HDCP_BAD_PARAM);
	case HDCP_HASH_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_HASH_FAIL);
	case HDCP_OUT_OF_MEM:
		return HDCP_CMD_STATUS_TO_STR(HDCP_OUT_OF_MEM);
	case HDCP_WRONG_MESSAGE_ID:
		return HDCP_CMD_STATUS_TO_STR(HDCP_WRONG_MESSAGE_ID);
	case HDCP_UNSUPPORTED_TXMTR:
		return HDCP_CMD_STATUS_TO_STR(HDCP_UNSUPPORTED_TXMTR);
	case HDCP_WRONG_TXMTR_CAPAB_MASK:
		return HDCP_CMD_STATUS_TO_STR(HDCP_WRONG_TXMTR_CAPAB_MASK);
	case HDCP_UNSUPPORTED_RCVR:
		return HDCP_CMD_STATUS_TO_STR(HDCP_UNSUPPORTED_RCVR);
	case HDCP_WRONG_RCVR_CAPAB_MASK:
		return HDCP_CMD_STATUS_TO_STR(HDCP_WRONG_RCVR_CAPAB_MASK);
	case HDCP_MAXLC_RETRIES_TRIED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_MAXLC_RETRIES_TRIED);
	case HDCP_STATE_UNAUTHENTICATED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_STATE_UNAUTHENTICATED);
	case HDCP_CE_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_CE_FAIL);
	case HDCP_RSA_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_RSA_FAIL);
	case HDCP_GET_RANDOM_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_GET_RANDOM_FAIL);
	case HDCP_SFS_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_SFS_FAIL);
	case HDCP_HPRIME_MISMATCH:
		return HDCP_CMD_STATUS_TO_STR(HDCP_HPRIME_MISMATCH);
	case HDCP_RCVRID_NOT_FOUND:
		return HDCP_CMD_STATUS_TO_STR(HDCP_RCVRID_NOT_FOUND);
	case HDCP_WRONG_STATE:
		return HDCP_CMD_STATUS_TO_STR(HDCP_WRONG_STATE);
	case HDCP_WRONG_V_RETURNED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_WRONG_V_RETURNED);
	case HDCP_MAX_DEVICE_EXCEEDED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_MAX_DEVICE_EXCEEDED);
	case HDCP_MAX_CASCADE_EXCEEDED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_MAX_CASCADE_EXCEEDED);
	case HDCP_V_ROLLOVER:
		return HDCP_CMD_STATUS_TO_STR(HDCP_V_ROLLOVER);
	case HDCP_V_MISMATCH:
		return HDCP_CMD_STATUS_TO_STR(HDCP_V_MISMATCH);
	case HDCP_MPRIME_MISMATCH:
		return HDCP_CMD_STATUS_TO_STR(HDCP_MPRIME_MISMATCH);
	case HDCP_NO_RECEIVERIDLIST:
		return HDCP_CMD_STATUS_TO_STR(HDCP_NO_RECEIVERIDLIST);
	case HDCP_TIMEOUT_EXPIRED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_TIMEOUT_EXPIRED);
	case HDCP_SRM_CHECK_FAILURE:
		return HDCP_CMD_STATUS_TO_STR(HDCP_SRM_CHECK_FAILURE);
	case HDCP_INVALID_TXMTR_KEY_FORMAT:
		return HDCP_CMD_STATUS_TO_STR(HDCP_INVALID_TXMTR_KEY_FORMAT);
	case HDCP_INVALID_TXMTR_CONTEXT:
		return HDCP_CMD_STATUS_TO_STR(HDCP_INVALID_TXMTR_CONTEXT);
	case HDCP_INVALID_RCVR_CONTEXT:
		return HDCP_CMD_STATUS_TO_STR(HDCP_INVALID_RCVR_CONTEXT);
	case HDCP_GENERIC_PROV_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_GENERIC_PROV_FAIL);
	case HDCP_CONFIG_FAIL:
		return HDCP_CMD_STATUS_TO_STR(HDCP_CONFIG_FAIL);
	case HDCP_KEY_ALREADY_PROVISIONED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_KEY_ALREADY_PROVISIONED);
	case HDCP_KEY_NOT_PROVISIONED:
		return HDCP_CMD_STATUS_TO_STR(HDCP_KEY_NOT_PROVISIONED);
	case HDCP_CALL_TOO_SOON:
		return HDCP_CMD_STATUS_TO_STR(HDCP_CALL_TOO_SOON);
	default:
		return "";
	}
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
		pr_err("library already loaded\n");
		goto error;
	}

	rc = qseecom_start_app(&handle->qseecom_handle,
		 HDCP2P2_APP_NAME, QSEECOM_SBUFF_SIZE);
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
	pr_debug("success\n");

	return rc;
error:
	qseecom_shutdown_app(&handle->hdcpsrm_qseecom_handle);
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
		pr_debug("HDCP 2.2 supported\n");

		handle->feature_supported = true;

		hdcp2_app_unload(handle);
		supported = true;
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
		pr_err("app not loaded\n");
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
		pr_err("app not loaded\n");
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
		pr_err("app not loaded\n");
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
	if (rc)
		goto error;

	if (!handle->legacy_app)
		rc = hdcp2_app_start_auth(handle);
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
	else
		handle->app_data.repeater_flag = false;

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

void *hdcp2_init(u32 device_type)
{
	struct hdcp2_handle *handle = NULL;

	handle = kzalloc(sizeof(struct hdcp2_handle), GFP_KERNEL);
	if (!handle)
		goto error;

	handle->device_type = device_type;
error:
	return handle;
}

void hdcp2_deinit(void *ctx)
{
	if (ctx)
		kzfree(ctx);
}

void *hdcp1_init(void)
{
	struct hdcp1_handle *handle =
		kzalloc(sizeof(struct hdcp1_handle), GFP_KERNEL);

	return handle;
}

void hdcp1_deinit(void *data)
{
	kfree(data);
}

bool hdcp1_feature_supported(void *data)
{
	int rc = 0;
	bool supported = false;
	struct hdcp1_handle *handle = data;

	if (!handle) {
		pr_err("invalid input\n");
		goto error;
	}

	if (handle->feature_supported) {
		supported = true;
		goto error;
	}

	rc = qseecom_start_app(&handle->qseecom_handle, HDCP1_APP_NAME,
			QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_err("qseecom_start_app failed %d\n", rc);
		goto error;
	}

	pr_debug("HDCP 1.x supported\n");
	handle->feature_supported = true;
	supported = true;
error:
	return supported;
}

/* APIs exposed to all clients */
int hdcp1_set_keys(void *data, uint32_t *aksv_msb, uint32_t *aksv_lsb)
{
	int rc = 0;
	struct hdcp1_key_set_req *key_set_req;
	struct hdcp1_key_set_rsp *key_set_rsp;
	struct hdcp1_handle *hdcp1_handle = data;
	struct qseecom_handle *handle = NULL;

	if (aksv_msb == NULL || aksv_lsb == NULL) {
		pr_err("invalid aksv\n");
		return -EINVAL;
	}

	if (!hdcp1_handle || !hdcp1_handle->qseecom_handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!hdcp1_handle->feature_supported) {
		pr_err("HDCP 1.x not supported\n");
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

	return 0;
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

