/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/workqueue.h>
#include <linux/hdcp_qseecom.h>

#include "qseecom_kernel.h"

#define TZAPP_NAME            "hdcp2p2"
#define HDCP1_APP_NAME        "hdcp1"
#define QSEECOM_SBUFF_SIZE    0x1000

#define MAX_TX_MESSAGE_SIZE	129
#define MAX_RX_MESSAGE_SIZE	534
#define MAX_TOPOLOGY_ELEMS	32
#define HDCP1_AKSV_SIZE         8

/* parameters related to LC_Init message */
#define MESSAGE_ID_SIZE            1
#define LC_INIT_MESSAGE_SIZE       (MESSAGE_ID_SIZE+BITS_64_IN_BYTES)

/* parameters related to SKE_Send_EKS message */
#define SKE_SEND_EKS_MESSAGE_SIZE \
	(MESSAGE_ID_SIZE+BITS_128_IN_BYTES+BITS_64_IN_BYTES)

/* all message IDs */
#define AKE_SEND_CERT_MESSAGE_ID         3
#define AKE_NO_STORED_KM_MESSAGE_ID      4
#define AKE_STORED_KM_MESSAGE_ID         5
#define AKE_SEND_H_PRIME_MESSAGE_ID      7
#define AKE_SEND_PAIRING_INFO_MESSAGE_ID 8
#define LC_INIT_MESSAGE_ID               9
#define LC_SEND_L_PRIME_MESSAGE_ID      10
#define SKE_SEND_EKS_MESSAGE_ID         11
#define REPEATER_AUTH_SEND_RECEIVERID_LIST_MESSAGE_ID 12
#define REPEATER_AUTH_SEND_ACK_MESSAGE_ID      15
#define REPEATER_AUTH_STREAM_MANAGE_MESSAGE_ID 16
#define REPEATER_AUTH_STREAM_READY_MESSAGE_ID  17
#define HDCP1_SET_KEY_MESSAGE_ID       202

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
#define SLEEP_SET_HW_KEY_MS 200


#define QSEECOM_ALIGN_SIZE    0x40
#define QSEECOM_ALIGN_MASK    (QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)\
	((x + QSEECOM_ALIGN_SIZE) & (~QSEECOM_ALIGN_MASK))

/* hdcp command status */
#define HDCP_SUCCESS      0

/* flags set by tz in response message */
#define HDCP_TXMTR_SUBSTATE_INIT                              0
#define HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST       1
#define HDCP_TXMTR_SUBSTATE_PROCESSED_RECIEVERID_LIST         2
#define HDCP_TXMTR_SUBSTATE_WAITING_FOR_STREAM_READY_MESSAGE  3
#define HDCP_TXMTR_SUBSTATE_REPEATER_AUTH_COMPLETE            4


#define HDCP_TXMTR_SERVICE_ID                 0x0001000
#define SERVICE_TXMTR_CREATE_CMD(x)          (HDCP_TXMTR_SERVICE_ID | x)

#define HDCP_TXMTR_INIT                           SERVICE_TXMTR_CREATE_CMD(1)
#define HDCP_TXMTR_DEINIT                         SERVICE_TXMTR_CREATE_CMD(2)
#define HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE       SERVICE_TXMTR_CREATE_CMD(3)
#define HDCP_TXMTR_SEND_MESSAGE_TIMEOUT           SERVICE_TXMTR_CREATE_CMD(4)
#define HDCP_TXMTR_SET_HW_KEY                     SERVICE_TXMTR_CREATE_CMD(5)
#define HDCP_TXMTR_SET_STREAM_TYPE                SERVICE_TXMTR_CREATE_CMD(6)
#define HDCP_TXMTR_GET_KSXORLC128_AND_RIV         SERVICE_TXMTR_CREATE_CMD(7)
#define HDCP_TXMTR_PROVISION_KEY                  SERVICE_TXMTR_CREATE_CMD(8)
#define HDCP_TXMTR_GET_TOPOLOGY_INFO              SERVICE_TXMTR_CREATE_CMD(9)
#define HDCP_TXMTR_UPDATE_SRM                     SERVICE_TXMTR_CREATE_CMD(10)
/*This API calls the library init function */
#define HDCP_LIB_INIT                             SERVICE_TXMTR_CREATE_CMD(11)
/*This API calls the library deinit function */
#define HDCP_LIB_DEINIT                           SERVICE_TXMTR_CREATE_CMD(12)


enum hdcp_app_status {
	LOADED,
	UNLOADED,
	FAILED = -1,
};

enum hdcp_mod_state {
	HDCP_STATE_DEINIT,
	HDCP_STATE_INIT,
	HDCP_STATE_TXMTR_INIT,
	HDCP_STATE_TXMTR_DEINIT,
	HDCP_STATE_MSG_RECEIVED,
	HDCP_STATE_AUTHENTICATED,
	HDCP_STATE_ERROR = 0xFFFFFFFF
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

struct __attribute__ ((__packed__)) hdcp_init_req {
	uint32_t commandid;
};

struct __attribute__ ((__packed__)) hdcp_init_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t  message[MAX_TX_MESSAGE_SIZE];
};


struct __attribute__ ((__packed__)) hdcp_deinit_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};


struct __attribute__ ((__packed__)) hdcp_deinit_rsp {
	uint32_t status;
	uint32_t commandid;
};


struct __attribute__ ((__packed__)) hdcp_rcvd_msg_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint32_t msglen;
	uint8_t  msg[MAX_RX_MESSAGE_SIZE];
};


struct __attribute__ ((__packed__)) hdcp_rcvd_msg_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t state;
	uint32_t timeout;
	uint32_t flag;
	uint32_t msglen;
	uint8_t  msg[MAX_TX_MESSAGE_SIZE];
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
	uint8_t  message[MAX_TX_MESSAGE_SIZE];
};


struct __attribute__ ((__packed__)) hdcp_set_stream_type_req {
	uint32_t commandid;
	uint32_t ctxhandle;
	uint8_t  streamtype;
};

struct __attribute__ ((__packed__)) hdcp_set_stream_type_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t  message[MAX_TX_MESSAGE_SIZE];
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


struct hdcp_dev_cntrl {
	/*
	 * we impose this lock to ensure that only one API
	 * is called at a time
	 */
	struct mutex hdcp_lock;
	enum hdcp_mod_state hdcp_state;
	bool repeater_flag;
	struct qseecom_handle *qseecom_handle;
	uint32_t ref_cnt;
};

static struct hdcp_dev_cntrl hdcp_mod;

#define HDCP_STATE_NAME (hdcp_state_name(hdcp_mod.hdcp_state))

struct hdcp2p2_message_map {
	int msg_id;
	const char *msg_name;
};

static const char *hdcp2p2_message_name(int msg_id)
{
	/*
	 * Message ID map. The first number indicates the message number
	 * assigned to the message by the HDCP 2.2 spec. This is also the first
	 * byte of every HDCP 2.2 authentication protocol message.
	 */
	static struct hdcp2p2_message_map hdcp2p2_msg_map[] = {
		{2, "AKE_INIT"},
		{3, "AKE_SEND_CERT"},
		{4, "AKE_NO_STORED_KM"},
		{5, "AKE_STORED_KM"},
		{7, "AKE_SEND_H_PRIME"},
		{8, "AKE_SEND_PAIRING_INFO"},
		{9, "LC_INIT"},
		{10, "LC_SEND_L_PRIME"},
		{11, "SKE_SEND_EKS"},
		{12, "REPEATER_AUTH_SEND_RECEIVERID_LIST"},
		{15, "REPEATER_AUTH_SEND_ACK"},
		{16, "REPEATER_AUTH_STREAM_MANAGE"},
		{17, "REPEATER_AUTH_STREAM_READY"},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(hdcp2p2_msg_map); i++) {
		if (msg_id == hdcp2p2_msg_map[i].msg_id)
			return hdcp2p2_msg_map[i].msg_name;
	}
	return "UNKNOWN";
}

static const char *hdcp_state_name(enum hdcp_mod_state hdcp_state)
{
	switch (hdcp_state) {
	case HDCP_STATE_DEINIT:        return "HDCP_STATE_DEINIT";
	case HDCP_STATE_INIT:          return "HDCP_STATE_INIT";
	case HDCP_STATE_TXMTR_INIT:    return "HDCP_STATE_TXMTR_INIT";
	case HDCP_STATE_MSG_RECEIVED:  return "HDCP_STATE_MSG_RECEIVED";
	case HDCP_STATE_AUTHENTICATED: return "HDCP_STATE_AUTHENTICATED";
	case HDCP_STATE_ERROR:         return "HDCP_STATE_ERROR";
	default:                       return "???";
	}
} /*hdcp_state_name*/

/* Function proto types */
static int hdcp2p2_send_message(struct hdcp_client_handle *);
static int hdcp2p2_manage_timeout(struct hdcp_client_handle *);
static int hdcp2p2_txmtr_init(void *);
static int hdcp2p2_txmtr_deinit(void *);
static void hdcp2p2_msg_arrive_work_fn(struct work_struct *);
static int hdcp2p2_txmtr_process_message(void *, unsigned char *, uint32_t);

/* called on hdcp thread */
static int hdcp2p2_send_message(struct hdcp_client_handle *hdcp_handle)
{
	int rc = 0;
	struct hdcp_set_hw_key_req *req_buf;
	struct hdcp_set_hw_key_rsp *rsp_buf;
	char *rcvr_buf;
	uint32_t timeout;

	if (hdcp_handle->listener_buf == NULL) {
		pr_debug("%s: no message to copy\n", __func__);
		goto exit;
	}
	rcvr_buf = kzalloc(MAX_TX_MESSAGE_SIZE, GFP_KERNEL);

	if (!rcvr_buf) {
		pr_err("%s: Failed to allocate memory to rcvr buf\n",
			__func__);
		return -ENOMEM;
	}

	mutex_lock(&hdcp_handle->mutex);
	memcpy(rcvr_buf, hdcp_handle->listener_buf, MAX_TX_MESSAGE_SIZE);
	mutex_unlock(&hdcp_handle->mutex);

	rc = hdcp_handle->client_ops->hdcp_send_message(hdcp_handle->
		client_ctx, hdcp_handle, rcvr_buf,
			hdcp_handle->msglen);

	if (rc) {
		/* authentication failure */
		hdcp2p2_txmtr_deinit(hdcp_handle);
		goto exit;
	}

	/* if SKE_SEND_EKS message was received from TZ*/
	if (hdcp_handle->ske_flag) {
		/* sleep for 200ms and send command to qseecom to set hw keys */
		msleep(SLEEP_SET_HW_KEY_MS);
		req_buf = (struct hdcp_set_hw_key_req *)(
			hdcp_mod.qseecom_handle->sbuf);
		req_buf->commandid = HDCP_TXMTR_SET_HW_KEY;
		req_buf->ctxhandle = hdcp_handle->tz_ctxhandle;

		rsp_buf = (struct hdcp_set_hw_key_rsp *)(
			hdcp_mod.qseecom_handle->sbuf + QSEECOM_ALIGN(
			sizeof(struct hdcp_set_hw_key_req)));

		rc = qseecom_send_command(hdcp_mod.qseecom_handle, req_buf,
			QSEECOM_ALIGN(sizeof(struct hdcp_set_hw_key_req)),
				rsp_buf, QSEECOM_ALIGN(sizeof(
				struct hdcp_set_hw_key_rsp)));

		if ((rc < 0) || (rsp_buf->status < 0)) {
			pr_err("%s: qseecom cmd failed with err = %d status = %d\n",
				__func__, rc, rsp_buf->status);
			hdcp_handle->timeout_flag = 1;
			hdcp_handle->client_ops->hdcp_tz_error(hdcp_handle->
				client_ctx, hdcp_handle);
			rc = -1;
			goto exit;
		}

		/* reached an authenticated state */
		hdcp_mod.hdcp_state = HDCP_STATE_AUTHENTICATED;

		/* if not a repeater then there is no need to start the timer */
		if (!hdcp_mod.repeater_flag) {
			rc = 0;
			goto exit;
		}
		/* set the timeout value to the actual - 200ms */
		mutex_lock(&hdcp_handle->mutex);
		timeout =  hdcp_handle->hdcp_timeout -
				SLEEP_SET_HW_KEY_MS;
		mutex_unlock(&hdcp_handle->mutex);
	}

	if (hdcp_handle->hdcp_timeout == 0) {
		hdcp_handle->timeout_flag = 0;
		rc = 0;
		goto exit;
	}

	mutex_lock(&hdcp_handle->mutex);
	timeout = hdcp_handle->hdcp_timeout;
	mutex_unlock(&hdcp_handle->mutex);

	if (!wait_for_completion_timeout(&hdcp_handle->done,
			msecs_to_jiffies(timeout))) {
		pr_debug("%s: timeout occured of %d ms\n",
			__func__, timeout);
		hdcp_handle->timeout_flag = 1;
		rc = hdcp2p2_manage_timeout(hdcp_handle);
		goto exit;
	}

	hdcp_handle->timeout_flag = 0;
exit:
	kzfree(rcvr_buf);
	rcvr_buf = NULL;
	return rc;
}


static int hdcp2p2_manage_timeout(struct hdcp_client_handle *hdcp_handle)
{
	int rc = 0;
	int auth_fail = 1;
	struct hdcp_send_timeout_req *req_buf;
	struct hdcp_send_timeout_rsp *rsp_buf;
	if (hdcp_handle->lc_init_flag)
		auth_fail = 0;
	rc = hdcp_handle->client_ops->hdcp_tz_timeout(hdcp_handle->
			client_ctx, hdcp_handle, auth_fail);
	if (rc) {
		pr_err("%s, error returned from HDMI\n", __func__);
		hdcp2p2_txmtr_deinit(hdcp_handle);
		return rc;
	}

	req_buf = (struct hdcp_send_timeout_req *)(hdcp_mod.
				qseecom_handle->sbuf);
	req_buf->commandid = HDCP_TXMTR_SEND_MESSAGE_TIMEOUT;
	req_buf->ctxhandle = hdcp_handle->tz_ctxhandle;

	rsp_buf = (struct hdcp_send_timeout_rsp *)(hdcp_mod.
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_send_timeout_req)));

	rc = qseecom_send_command(hdcp_mod.qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_send_timeout_req)), rsp_buf,
			QSEECOM_ALIGN(sizeof(struct hdcp_send_timeout_rsp)));

	if ((rc < 0) || (rsp_buf->status != HDCP_SUCCESS)) {
		pr_err("%s: qseecom cmd failed for with err = %d status = %d\n",
					__func__, rc, rsp_buf->status);
		hdcp_handle->client_ops->hdcp_tz_error(hdcp_handle->
					client_ctx, hdcp_handle);
		return -EINVAL;
	}
	if (rsp_buf->commandid == HDCP_TXMTR_SEND_MESSAGE_TIMEOUT) {
		hdcp_handle->lc_init_flag = 0;
		return rc;
	}
	/*
	 * if the response contains LC_Init message
	 * send the message again to TZ
	 */
	if ((rsp_buf->commandid == HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE) &&
		((int)rsp_buf->message[0] == LC_INIT_MESSAGE_ID) &&
			(rsp_buf->msglen == LC_INIT_MESSAGE_SIZE)) {
		hdcp_handle->lc_init_flag = 1;
		rc = hdcp2p2_send_message(hdcp_handle);
	}
	return rc;
}


static void hdcp2p2_msg_arrive_work_fn(struct work_struct *work)
{
	struct hdcp_client_handle *handle = NULL;
	handle = container_of(work, struct hdcp_client_handle,
					work_msg_arrive);
	hdcp2p2_send_message(handle);
	return;
}

/*
 * APIs exposed to all clients
 */

int hdcp1_set_keys(uint32_t *aksv_msb, uint32_t *aksv_lsb)
{
	int rc = 0;
	struct hdcp1_key_set_req *key_set_req;
	struct hdcp1_key_set_rsp *key_set_rsp;
	struct qseecom_handle *hdcp1_handle = NULL;

	if (aksv_msb == NULL || aksv_lsb == NULL)
		return -EINVAL;

	/* start hdcp1 app */
	rc = qseecom_start_app(&hdcp1_handle, HDCP1_APP_NAME,
						QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_err("%s: qseecom_start_app failed %d\n", __func__, rc);
		return -ENOSYS;
	}

	/* set keys and request aksv */
	key_set_req = (struct hdcp1_key_set_req *)hdcp1_handle->sbuf;
	key_set_req->commandid = HDCP1_SET_KEY_MESSAGE_ID;
	key_set_rsp = (struct hdcp1_key_set_rsp *)(hdcp1_handle->sbuf +
			QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_req)));
	rc = qseecom_send_command(hdcp1_handle,
		key_set_req, QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_req)),
		key_set_rsp, QSEECOM_ALIGN(sizeof(struct hdcp1_key_set_rsp)));

	if (rc < 0) {
		pr_err("%s: qseecom cmd failed err=%d\n", __func__, rc);
		return -ENOKEY;
	}

	rc = key_set_rsp->ret;
	if (rc) {
		pr_err("%s: set key cmd failed, rsp=%d\n", __func__,
			key_set_rsp->ret);
		return -ENOKEY;
	}

	/* copy bytes into msb and lsb */
	*aksv_msb = key_set_rsp->ksv[0] << 24;
	*aksv_msb |= key_set_rsp->ksv[1] << 16;
	*aksv_msb |= key_set_rsp->ksv[2] << 8;
	*aksv_msb |= key_set_rsp->ksv[3];
	*aksv_lsb = key_set_rsp->ksv[4] << 24;
	*aksv_lsb |= key_set_rsp->ksv[5] << 16;
	*aksv_lsb |= key_set_rsp->ksv[6] << 8;
	*aksv_lsb |= key_set_rsp->ksv[7];

	return 0;
}
EXPORT_SYMBOL(hdcp1_set_keys);


/*
 * initilaizes internal states of the library and loads HDCP secure application
 * returns a context handle to the caller as an indication of success
 */

int hdcp_library_init(void **pphdcpcontext, struct hdcp_client_ops *client_ops,
		struct hdcp_txmtr_ops *txmtr_ops, void *client_ctx)
{
	int rc = 0;
	struct hdcp_init_req *req_buf;
	struct hdcp_init_rsp *rsp_buf;

	struct hdcp_client_handle *handle = NULL;
	struct hdcp_txmtr_ops *ops = NULL;

	/* handle passed should not be NULL */
	if (!pphdcpcontext) {
		pr_err("%s: context passed is NULL\n", __func__);
		return -EINVAL;
	}

	if (!txmtr_ops) {
		pr_err("%s: txmtr context passed is NULL\n", __func__);
		return -EINVAL;
	}

	if (!client_ops) {
		pr_err("%s: invalid input: client_ops\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp_mod.hdcp_lock);

	/* if library is already initialized then return (not an error) */
	if (hdcp_mod.hdcp_state == HDCP_STATE_INIT) {
		pr_debug("%s: library already initialized\n",
				__func__);
		goto unlock;
	}

	/* Library should be in deinit state before calling this API */
	if (hdcp_mod.hdcp_state != HDCP_STATE_DEINIT) {
		pr_err("%s: invalid state of hdcp library: %s\n",
				__func__, HDCP_STATE_NAME);
		rc = -EINVAL;
		goto unlock;
	}

	/* Load the secure app if it's unloaded*/
	if (hdcp_mod.ref_cnt == 0) {
		/*
		 * allocating resource for qseecom handle
		 * the app is not loaded here
		 */
		hdcp_mod.qseecom_handle = NULL;
		rc = qseecom_start_app(&(hdcp_mod.qseecom_handle),
				TZAPP_NAME, QSEECOM_SBUFF_SIZE);
		if (rc) {
			pr_err("%s: qseecom_start_app failed %d\n",
							__func__, rc);
			goto unlock;
		}
		pr_debug("qseecom_start_app success\n");

		/* now load the app by sending hdcp_lib_init */
		req_buf = (struct hdcp_init_req *)hdcp_mod.
				qseecom_handle->sbuf;
		req_buf->commandid = HDCP_LIB_INIT;
		rsp_buf = (struct hdcp_init_rsp *)(hdcp_mod.qseecom_handle->
			sbuf + QSEECOM_ALIGN(sizeof(struct hdcp_init_req)));
		rc = qseecom_send_command(hdcp_mod.qseecom_handle,
			req_buf, QSEECOM_ALIGN(sizeof(struct hdcp_init_req)),
			rsp_buf, QSEECOM_ALIGN(sizeof(struct hdcp_init_rsp)));

		if (rc < 0) {
			pr_err("%s: qseecom cmd failed err = %d\n",
					__func__, rc);
			/* hdcp_mod.hdcp_state = HDCP_STATE_ERROR; */
			goto unlock;
		}
		pr_debug("%s: loading secure app success\n", __func__);
	}

	hdcp_mod.ref_cnt++;
	txmtr_ops->hdcp_txmtr_deinit = hdcp2p2_txmtr_deinit;
	txmtr_ops->hdcp_txmtr_init = hdcp2p2_txmtr_init;
	txmtr_ops->hdcp_txmtr_process_message = hdcp2p2_txmtr_process_message;

	ops = NULL;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle) {
		pr_err("%s: failed to allocate memory to client handle\n",
						__func__);
		rc = -ENOMEM;
		goto unlock;
	}
	handle->client_ctx = client_ctx;
	handle->client_ops = client_ops;

	mutex_init(&handle->mutex);
	init_completion(&handle->done);

	handle->hdcp_workqueue = create_singlethread_workqueue("hdcp-module");
	if (handle->hdcp_workqueue == NULL) {
		pr_err("%s: hdcp wq creation failed\n", __func__);
		rc = -EFAULT;
		goto error;
	}

	INIT_WORK(&(handle->work_msg_arrive), hdcp2p2_msg_arrive_work_fn);

	handle->listener_buf = kzalloc(MAX_TX_MESSAGE_SIZE, GFP_KERNEL);
	if (!(handle->listener_buf)) {
		pr_err("Failed to allocate memory to the buffer\n");
		rc = -ENOMEM;
		goto error;
	}

	hdcp_mod.hdcp_state = HDCP_STATE_INIT;
	*((struct hdcp_client_handle **)pphdcpcontext) = handle;
	mutex_unlock(&hdcp_mod.hdcp_lock);
	pr_debug("%s: hdcp lib successfully initialized\n", __func__);
	return 0;
error:
	/* deallocate resources */
	if (handle->hdcp_workqueue != NULL) {
		destroy_workqueue(handle->hdcp_workqueue);
		handle->hdcp_workqueue = NULL;
	}
	complete(&handle->done);
	kzfree(handle->listener_buf);
	handle->listener_buf = NULL;
	kzfree(handle);
	handle = NULL;
unlock:
	mutex_unlock(&hdcp_mod.hdcp_lock);
	return rc;
}
EXPORT_SYMBOL(hdcp_library_init);


/*
 * deinitializes internal states of HDCP library
 * HdcpLibraryInit needs to be called prior to this call
 * hdcp_context is passed as a double pointer
 */
int hdcp_library_deinit(void *phdcpcontext)
{
	int rc = 0;
	struct hdcp_deinit_req *req_buf;
	struct hdcp_deinit_rsp *rsp_buf;

	struct hdcp_client_handle *handle = NULL;

	if (!phdcpcontext) {
		pr_err("%s: hdcp client handle is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&hdcp_mod.hdcp_lock);

	/* if library is already deinitialized then just return */
	if (hdcp_mod.hdcp_state == HDCP_STATE_DEINIT) {
		pr_debug("%s: library already deinitialized\n",
			__func__);
		goto unlock;
	}

	handle = (struct hdcp_client_handle *)phdcpcontext;
	/* deallocate resources */
	complete(&handle->done);
	if (handle->hdcp_workqueue != NULL) {
		destroy_workqueue(handle->hdcp_workqueue);
		handle->hdcp_workqueue = NULL;
	}
	kzfree(handle->listener_buf);
	handle->listener_buf = NULL;
	if (hdcp_mod.ref_cnt == 1) {

		/* unloading app by sending hdcp_lib_deinit cmd */
		req_buf = (struct hdcp_deinit_req *)hdcp_mod.
				qseecom_handle->sbuf;
		req_buf->commandid = HDCP_LIB_DEINIT;
		rsp_buf = (struct hdcp_deinit_rsp *)(hdcp_mod.qseecom_handle->
			sbuf + QSEECOM_ALIGN(sizeof(struct hdcp_deinit_req)));

		rc = qseecom_send_command(hdcp_mod.qseecom_handle,
			req_buf, QSEECOM_ALIGN(sizeof(struct hdcp_deinit_req)),
			rsp_buf, QSEECOM_ALIGN(sizeof(struct hdcp_deinit_rsp)));

		if (rc < 0) {
			pr_err("%s: qseecom cmd failed err = %d\n",
						__func__, rc);
			goto unlock;
		}

		/*deallocate the resources for qseecom handle */
		rc = qseecom_shutdown_app(&hdcp_mod.qseecom_handle);
		if (rc) {
			pr_err("%s: qseecom_shutdown_app failed err: %d\n",
				__func__, rc);
			goto unlock;
		}

		pr_debug("%s: unloading secure app success\n", __func__);
		hdcp_mod.hdcp_state = HDCP_STATE_DEINIT;
	}
	hdcp_mod.ref_cnt--;

	kzfree(handle);
	handle = NULL;
	pr_debug("%s: hdcp lib successfully deinitialized\n", __func__);
unlock:
	mutex_unlock(&hdcp_mod.hdcp_lock);
	return rc;
}
EXPORT_SYMBOL(hdcp_library_deinit);


/*
 * initializes the HDCP transmitter
 * HdcpLibraryInit needs to be invoked first to obtain the context handle
 * This function is called on HDMI thread
 */
static int hdcp2p2_txmtr_init(void *phdcpcontext)
{
	int rc = 0;
	struct hdcp_client_handle *hdcp_handle;
	struct hdcp_init_req *req_buf;
	struct hdcp_init_rsp *rsp_buf;

	if (!phdcpcontext) {
		pr_err("%s: hdcp client handle is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp_mod.hdcp_lock);

	/* if transmitter already initialized then just return */
	if (hdcp_mod.hdcp_state == HDCP_STATE_TXMTR_INIT) {
		pr_debug("%s: txmtr already initialized\n",
				__func__);
		goto unlock;
	}

	/* library should be initialized before calling this API */
	if (hdcp_mod.hdcp_state == HDCP_STATE_DEINIT) {
		pr_err("%s: invalid state of hdcp library: %s\n",
				__func__, HDCP_STATE_NAME);
		rc = -EINVAL;
		goto unlock;
	}
	hdcp_handle = (struct hdcp_client_handle *)phdcpcontext;

	/* send HDCP_Txmtr_Init command to TZ */
	req_buf = (struct hdcp_init_req *)hdcp_mod.
			qseecom_handle->sbuf;
	req_buf->commandid = HDCP_TXMTR_INIT;
	rsp_buf = (struct hdcp_init_rsp *)(hdcp_mod.qseecom_handle->
		sbuf + QSEECOM_ALIGN(sizeof(struct hdcp_init_req)));

	rc = qseecom_send_command(hdcp_mod.qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_init_req)), rsp_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_init_rsp)));

	if ((rc < 0) || (rsp_buf->status != HDCP_SUCCESS) ||
		(rsp_buf->commandid != HDCP_TXMTR_INIT) ||
		(rsp_buf->msglen <= 0) || (rsp_buf->message == NULL)) {
		pr_err("%s: qseecom cmd failed with err = %d, status = %d\n",
				__func__, rc, rsp_buf->status);
		rc = -1;
		goto unlock;
	}

	/* send the response to HDMI driver */
	mutex_lock(&hdcp_handle->mutex);
	memset(hdcp_handle->listener_buf, 0, MAX_TX_MESSAGE_SIZE);
	memcpy(hdcp_handle->listener_buf, (unsigned char *)rsp_buf->message,
			rsp_buf->msglen);
	hdcp_handle->msglen = rsp_buf->msglen;
	hdcp_handle->hdcp_timeout = rsp_buf->timeout;
	mutex_unlock(&hdcp_handle->mutex);

	hdcp_handle->tz_ctxhandle = rsp_buf->ctxhandle;
	queue_work(hdcp_handle->hdcp_workqueue, &hdcp_handle->work_msg_arrive);
	hdcp_mod.hdcp_state = HDCP_STATE_TXMTR_INIT;
	pr_err("%s: hdcp txmtr successfully initialized\n", __func__);

unlock:
	mutex_unlock(&hdcp_mod.hdcp_lock);
	return rc;
}

/*
 * deinitializes the HDCP transmitter
 * this is called on HDMI thread
 */
static int hdcp2p2_txmtr_deinit(void *phdcpcontext)
{
	int rc = 0;
	struct hdcp_client_handle *hdcp_handle;
	struct hdcp_deinit_req *req_buf;
	struct hdcp_deinit_rsp *rsp_buf;

	if (!phdcpcontext) {
		pr_err("%s: hdcp context passed is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp_mod.hdcp_lock);

	if (hdcp_mod.hdcp_state == HDCP_STATE_DEINIT) {
		pr_debug("%s: hdcp library already deinitialized\n", __func__);
		goto unlock;
	}

	if (hdcp_mod.hdcp_state == HDCP_STATE_TXMTR_DEINIT) {
		pr_debug("%s: hdcp txmtr already deinitialized\n", __func__);
		goto unlock;
	}

	hdcp_handle = (struct hdcp_client_handle *)phdcpcontext;

	/* send command to TZ */
	req_buf = (struct hdcp_deinit_req *)hdcp_mod.
			qseecom_handle->sbuf;
	req_buf->commandid = HDCP_TXMTR_DEINIT;
	req_buf->ctxhandle = hdcp_handle->tz_ctxhandle;
	rsp_buf = (struct hdcp_deinit_rsp *)(hdcp_mod.
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_deinit_req)));

	rc = qseecom_send_command(hdcp_mod.qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_deinit_req)), rsp_buf,
			QSEECOM_ALIGN(sizeof(struct hdcp_deinit_rsp)));

	if ((rc < 0) || (rsp_buf->status < 0) ||
			(rsp_buf->commandid != HDCP_TXMTR_DEINIT)) {
		pr_err("%s: qseecom cmd failed with err = %d status = %d\n",
					__func__, rc, rsp_buf->status);
		rc = -1;
		goto unlock;
	}
	hdcp_mod.hdcp_state = HDCP_STATE_TXMTR_DEINIT;
	pr_debug("%s: hdcp txmtr successfully deinitialized", __func__);
unlock:
	mutex_unlock(&hdcp_mod.hdcp_lock);
	return rc;
}


static int hdcp2p2_txmtr_process_message(void *phdcpcontext,
		unsigned char *msg, uint32_t msglen)
{
	int rc = 0;
	struct hdcp_client_handle *hdcp_handle;
	struct hdcp_rcvd_msg_req *req_buf;
	struct hdcp_rcvd_msg_rsp *rsp_buf;

	if (!phdcpcontext) {
		pr_err("%s: hdcp client handle is NULL\n", __func__);
		return -EINVAL;
	}
	if ((!msg) || (msglen <= 0)) {
		pr_err("%s: invalid parameters passed\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&hdcp_mod.hdcp_lock);
	/* If library is not yet initialized then return error */
	if (hdcp_mod.hdcp_state == HDCP_STATE_DEINIT) {
		pr_err("%s: invalid state of hdcp library: %s\n",
				__func__, HDCP_STATE_NAME);
		rc = -EINVAL;
		goto unlock;
	}
	hdcp_handle = (struct hdcp_client_handle *)phdcpcontext;
	/* check for timeout before sending the message */
	if (hdcp_handle->timeout_flag) {
		pr_debug("%s: timeout has already occured; return", __func__);
		goto unlock;
	}

	/* interrupt the timer running on HDCP thread*/
	complete(&hdcp_handle->done);

	/* send the message to QSEECOM */
	req_buf = (struct hdcp_rcvd_msg_req *)(hdcp_mod.
			qseecom_handle->sbuf);
	req_buf->commandid = HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE;
	memcpy(req_buf->msg, msg, msglen);
	req_buf->msglen = msglen;
	req_buf->ctxhandle = hdcp_handle->tz_ctxhandle;

	rsp_buf = (struct hdcp_rcvd_msg_rsp *)(hdcp_mod.
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_rcvd_msg_req)));

	rc = qseecom_send_command(hdcp_mod.qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_rcvd_msg_req)), rsp_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_rcvd_msg_rsp)));

	/* No response was obtained from TZ */
	if ((msg[0] == AKE_SEND_H_PRIME_MESSAGE_ID) &&
			hdcp_handle->no_stored_km_flag) {
		pr_debug("%s: Got HPrime from tx, nothing sent to rx\n",
				__func__);
		goto unlock;
	}

	if ((rc < 0) || (rsp_buf->status < 0) || (rsp_buf->msglen <= 0) ||
		(rsp_buf->commandid != HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE) ||
				(rsp_buf->msg == NULL)) {
		pr_err("%s: qseecom cmd failed with err=%d status=%d\n",
			__func__, rc, rsp_buf->status);
		/* set the timeout flag so that the next message returns */
		hdcp_handle->timeout_flag = 1;
		rc = -1;
		goto unlock;
	}

	pr_debug("%s: message received is %s\n", __func__,
			hdcp2p2_message_name((int)rsp_buf->msg[0]));

	/* set the flag if response is AKE_No_Stored_km */
	if (((int)rsp_buf->msg[0] == AKE_NO_STORED_KM_MESSAGE_ID)) {
		pr_debug("%s: Setting no_stored_km_flag\n", __func__);
		hdcp_handle->no_stored_km_flag = 1;
	} else {
		hdcp_handle->no_stored_km_flag = 0;
	}

	/* set the flag if response is LC_Init */
	if (((int)rsp_buf->msg[0] == LC_INIT_MESSAGE_ID) &&
				(rsp_buf->msglen == LC_INIT_MESSAGE_SIZE)) {
		hdcp_handle->lc_init_flag = 1;
	} else {
		hdcp_handle->lc_init_flag = 0;
	}

	/*
	 * set ske flag is response is SKE_SEND_EKS
	 * also set repeater flag if it's a repeater
	 */
	hdcp_handle->ske_flag = 0;
	if ((rsp_buf->msg[0] == SKE_SEND_EKS_MESSAGE_ID) &&
			(rsp_buf->msglen == SKE_SEND_EKS_MESSAGE_SIZE)) {
		hdcp_handle->ske_flag = 1;
		/* check if it's a repeater */
		if ((rsp_buf->flag ==
			HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST) &&
						(rsp_buf->timeout > 0))
			hdcp_mod.repeater_flag = 1;
	}

	mutex_lock(&hdcp_handle->mutex);
	memset(hdcp_handle->listener_buf, 0, MAX_TX_MESSAGE_SIZE);
	memcpy(hdcp_handle->listener_buf, (unsigned char *)rsp_buf->msg,
			rsp_buf->msglen);
	hdcp_handle->hdcp_timeout = rsp_buf->timeout;
	hdcp_handle->msglen = rsp_buf->msglen;
	mutex_unlock(&hdcp_handle->mutex);
	/* before queuing the work, initialize completion variable */
	init_completion(&hdcp_handle->done);
	queue_work(hdcp_handle->hdcp_workqueue, &hdcp_handle->work_msg_arrive);
	pr_debug("%s: hdcp txmtr process message success", __func__);

unlock:
	mutex_unlock(&hdcp_mod.hdcp_lock);
	return rc;

}

static int __init hdcp_mod_init(void)
{
	hdcp_mod.hdcp_state = HDCP_STATE_DEINIT;
	mutex_init(&hdcp_mod.hdcp_lock);
	hdcp_mod.repeater_flag = 0;
	hdcp_mod.ref_cnt = 0;
	return 0;
}

static void __exit hdcp_mod_exit(void)
{
	kzfree(hdcp_mod.qseecom_handle);
	hdcp_mod.qseecom_handle = NULL;
}

module_init(hdcp_mod_init);
module_exit(hdcp_mod_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Kernel HDCP Trustzone support module");

