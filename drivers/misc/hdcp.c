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
#define HDCP_TXMTR_QUERY_STREAM_TYPE              SERVICE_TXMTR_CREATE_CMD(6)
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

enum hdcp_state {
	HDCP_STATE_INIT = 0x00,
	HDCP_STATE_APP_LOADED = 0x01,
	HDCP_STATE_TXMTR_INIT = 0x02,
	HDCP_STATE_AUTHENTICATED = 0x04,
	HDCP_STATE_ERROR = 0x08
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

struct __attribute__ ((__packed__)) hdcp_query_stream_type_req {
	uint32_t commandid;
	uint32_t ctxhandle;
};

struct __attribute__ ((__packed__)) hdcp_query_stream_type_rsp {
	uint32_t status;
	uint32_t commandid;
	uint32_t timeout;
	uint32_t msglen;
	uint8_t  msg[MAX_TX_MESSAGE_SIZE];
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

/*
 * struct hdcp2p2_handle - handle for hdcp client
 * @qseecom_handle - for sending commands to qseecom
 * @hdcp_workqueue - work queue for hdcp thread
 * @auth_work - work placed in the work queue
 * @listener_buf - buffer containing message shared with the client
 * @msglen - size message in the buffer
 * @tz_ctxhandle - context handle shared with tz
 * @hdcp_timeout - timeout in msecs shared for hdcp messages
 * @ske_flag - flag to indicate that msg from tz is ske_send_eks
 * @client_ctx - client context maintained by hdmi
 * @client_ops - handle to call APIs exposed by hdcp client
 * @timeout_lock - this lock protects hdcp_timeout field
 * @msg_lock - this lock protects the message buffer
 */
struct hdcp2p2_handle {
	struct workqueue_struct *hdcp_workqueue;
	struct work_struct auth_work;
	unsigned char *listener_buf;
	uint32_t msglen;
	uint32_t tz_ctxhandle;
	uint32_t hdcp_timeout;
	bool ske_flag;
	bool no_stored_km_flag;
	void *client_ctx;
	struct hdcp_client_ops *client_ops;
	struct mutex hdcp_lock;
	enum hdcp_state hdcp_state;
	bool repeater_flag;
	struct qseecom_handle *qseecom_handle;
	uint32_t ref_cnt;
	atomic_t hdcp_off;
};

struct hdcp2p2_message_map {
	int msg_id;
	const char *msg_name;
};

static void hdcp2p2_cleanup(void *phdcpcontext);

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

static int hdcp2p2_manage_timeout(struct hdcp2p2_handle *handle)
{
	int rc = 0;
	struct hdcp_send_timeout_req *req_buf;
	struct hdcp_send_timeout_rsp *rsp_buf;

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	req_buf = (struct hdcp_send_timeout_req *)
		(handle->qseecom_handle->sbuf);
	req_buf->commandid = HDCP_TXMTR_SEND_MESSAGE_TIMEOUT;
	req_buf->ctxhandle = handle->tz_ctxhandle;

	rsp_buf = (struct hdcp_send_timeout_rsp *)(handle->
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_send_timeout_req)));

	rc = qseecom_send_command(handle->qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_send_timeout_req)), rsp_buf,
			QSEECOM_ALIGN(sizeof(struct hdcp_send_timeout_rsp)));

	if ((rc < 0) || (rsp_buf->status != HDCP_SUCCESS)) {
		pr_err("qseecom cmd failed for with err = %d status = %d\n",
			rc, rsp_buf->status);
		rc = -EINVAL;
		goto exit;
	}

	if (rsp_buf->commandid == HDCP_TXMTR_SEND_MESSAGE_TIMEOUT) {
		pr_err("HDCP_TXMTR_SEND_MESSAGE_TIMEOUT\n");
		rc = -EAGAIN;
		goto exit;
	}

	/*
	 * if the response contains LC_Init message
	 * send the message again to TZ
	 */
	if ((rsp_buf->commandid == HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE) &&
		((int)rsp_buf->message[0] == LC_INIT_MESSAGE_ID) &&
			(rsp_buf->msglen == LC_INIT_MESSAGE_SIZE)) {
		if (!atomic_read(&handle->hdcp_off))
			queue_work(handle->hdcp_workqueue, &handle->auth_work);
		else
			pr_err("not queueing auth work, hdcp off underway\n");
	}
exit:
	return rc;
}

static int hdcp2p2_enable_encryption(struct hdcp2p2_handle *handle)
{
	int rc = 0;
	struct hdcp_set_hw_key_req *req_buf;
	struct hdcp_set_hw_key_rsp *rsp_buf;
	uint32_t timeout = 0;

	if (handle->ske_flag) {
		/*
		 * wait for 200ms before enabling encryption
		 * as per hdcp2p2 sepcifications.
		 */
		msleep(SLEEP_SET_HW_KEY_MS);

		req_buf = (struct hdcp_set_hw_key_req *)(
			handle->qseecom_handle->sbuf);
		req_buf->commandid = HDCP_TXMTR_SET_HW_KEY;
		req_buf->ctxhandle = handle->tz_ctxhandle;

		rsp_buf = (struct hdcp_set_hw_key_rsp *)(
			handle->qseecom_handle->sbuf + QSEECOM_ALIGN(
			sizeof(struct hdcp_set_hw_key_req)));

		rc = qseecom_send_command(handle->qseecom_handle, req_buf,
			QSEECOM_ALIGN(sizeof(struct hdcp_set_hw_key_req)),
				rsp_buf, QSEECOM_ALIGN(sizeof(
				struct hdcp_set_hw_key_rsp)));

		if ((rc < 0) || (rsp_buf->status < 0)) {
			pr_err("qseecom cmd failed with err = %d status = %d\n",
				rc, rsp_buf->status);
			timeout = -1;
			goto exit;
		}

		/* reached an authenticated state */
		handle->hdcp_state |= HDCP_STATE_AUTHENTICATED;

		/* if not a repeater then there is no need to start the timer */
		if (!handle->repeater_flag)
			goto exit;

		/* set the timeout value to the actual - 200ms */
		timeout = handle->hdcp_timeout - SLEEP_SET_HW_KEY_MS;
	}
exit:
	return timeout;
}

static int hdcp2p2_send_message(struct hdcp2p2_handle *handle)
{
	int rc = -EINVAL;
	char *rcvr_buf;
	uint32_t timeout;

	if (!handle) {
		pr_err("invalid input\n");
		goto exit;
	}

	if (!handle->listener_buf) {
		pr_err("no message to copy\n");
		goto exit;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp library not loaded\n");
		goto exit;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr is not initialized\n");
		goto exit;
	}

	rcvr_buf = kzalloc(MAX_TX_MESSAGE_SIZE, GFP_KERNEL);
	if (!rcvr_buf)
		goto exit;

	memcpy(rcvr_buf, handle->listener_buf, MAX_TX_MESSAGE_SIZE);

	timeout = handle->hdcp_timeout;

	if (atomic_read(&handle->hdcp_off)) {
		pr_err("send msg check: hdcp off underway\n");
		rc = 0;
		goto exit;
	}

	if (handle->client_ops->hdcp_send_message) {
		rc = handle->client_ops->hdcp_send_message(
			handle->client_ctx,
			rcvr_buf, handle->msglen);
		if (rc) {
			pr_err("error sending message to client\n");
			goto exit;
		}
	} else {
		pr_err("send_msg client ops not defined\n");
		goto exit;
	}

	rc = hdcp2p2_enable_encryption(handle);
	if (rc < 0) {
		pr_err("error enabling ecryption\n");
		goto exit;
	} else if (rc > 0)  {
		timeout = rc;
	}

	if (handle->hdcp_timeout == 0) {
		rc = 0;
		goto exit;
	}

	if (atomic_read(&handle->hdcp_off)) {
		pr_err("recv msg check: hdcp off underway\n");
		rc = 0;
		goto exit;
	}

	if (handle->client_ops->hdcp_recv_message) {
		rc = handle->client_ops->hdcp_recv_message(
			handle->client_ctx, rcvr_buf,
			handle->msglen, timeout);
		if (rc == -ETIMEDOUT) {
			pr_err("msg read time %dms expired\n", timeout);
			rc = hdcp2p2_manage_timeout(handle);
		}
	} else {
		pr_err("recv_msg client ops not defined\n");
	}
exit:
	kzfree(rcvr_buf);

	if (rc)
		hdcp2p2_cleanup(handle);

	return rc;
}

static void hdcp2p2_authenticate_work(struct work_struct *work)
{
	struct hdcp2p2_handle *handle = container_of(work,
		struct hdcp2p2_handle, auth_work);

	hdcp2p2_send_message(handle);
}

static int hdcp2p2_txmtr_query_stream_type(void *phdcpcontext)
{
	int rc = 0;
	struct hdcp_query_stream_type_req *req_buf;
	struct hdcp_query_stream_type_rsp *rsp_buf;
	struct hdcp2p2_handle *handle = phdcpcontext;

	if (!handle) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_debug("hdcp library not loaded\n");
		goto end;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr is not initialized\n");
		rc = -EINVAL;
		goto end;
	}

	flush_work(&handle->auth_work);

	/* send command to TZ */
	req_buf = (struct hdcp_query_stream_type_req *)handle->
			qseecom_handle->sbuf;
	req_buf->commandid = HDCP_TXMTR_QUERY_STREAM_TYPE;
	req_buf->ctxhandle = handle->tz_ctxhandle;
	rsp_buf = (struct hdcp_query_stream_type_rsp *)(handle->
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_query_stream_type_req)));

	rc = qseecom_send_command(handle->qseecom_handle, req_buf,
	QSEECOM_ALIGN(sizeof(struct hdcp_query_stream_type_req)), rsp_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_query_stream_type_rsp)));

	if ((rc < 0) || (rsp_buf->status < 0) || (rsp_buf->msglen <= 0) ||
		(rsp_buf->commandid != HDCP_TXMTR_QUERY_STREAM_TYPE) ||
				(rsp_buf->msg == NULL)) {
		pr_err("qseecom cmd failed with err=%d status=%d\n",
			rc, rsp_buf->status);
		rc = -1;
		goto end;
	}

	pr_debug("message received is %s\n",
		hdcp2p2_message_name((int)rsp_buf->msg[0]));

	memset(handle->listener_buf, 0, MAX_TX_MESSAGE_SIZE);
	memcpy(handle->listener_buf, (unsigned char *)rsp_buf->msg,
			rsp_buf->msglen);
	handle->hdcp_timeout = rsp_buf->timeout;
	handle->msglen = rsp_buf->msglen;

	if (!atomic_read(&handle->hdcp_off))
		queue_work(handle->hdcp_workqueue, &handle->auth_work);
	else
		pr_err("not queueing auth work, hdcp off underway\n");

	pr_debug("hdcp txmtr query stream type success\n");
end:
	return rc;
}

static int hdcp2p2_library_load(struct hdcp2p2_handle *handle)
{
	int rc = 0;
	struct hdcp_init_req *req_buf;
	struct hdcp_init_rsp *rsp_buf;

	if (!handle) {
		pr_err("invalid input\n");
		goto end;
	}

	if (handle->ref_cnt > 0) {
		pr_err("error: ref_cnt %d, should be 0\n", handle->ref_cnt);
		goto end;
	}

	/*
	 * allocating resource for qseecom handle
	 * the app is not loaded here
	 */
	rc = qseecom_start_app(&(handle->qseecom_handle),
			TZAPP_NAME, QSEECOM_SBUFF_SIZE);
	if (rc) {
		pr_err("qseecom_start_app failed %d\n", rc);
		goto end;
	}

	pr_debug("qseecom_start_app success\n");

	/* now load the app by sending hdcp_lib_init */
	req_buf = (struct hdcp_init_req *)handle->qseecom_handle->sbuf;
	req_buf->commandid = HDCP_LIB_INIT;
	rsp_buf = (struct hdcp_init_rsp *)(handle->qseecom_handle->
		sbuf + QSEECOM_ALIGN(sizeof(struct hdcp_init_req)));

	rc = qseecom_send_command(handle->qseecom_handle,
		req_buf, QSEECOM_ALIGN(sizeof(struct hdcp_init_req)),
		rsp_buf, QSEECOM_ALIGN(sizeof(struct hdcp_init_rsp)));

	if (rc < 0) {
		pr_err("qseecom cmd failed err = %d\n", rc);
		goto end;
	}

	pr_debug("loading secure app success\n");

	handle->ref_cnt++;
	handle->hdcp_state |= HDCP_STATE_APP_LOADED;
end:
	return rc;
}

static int hdcp2p2_library_unload(struct hdcp2p2_handle *handle)
{
	int rc = 0;
	struct hdcp_deinit_req *req_buf;
	struct hdcp_deinit_rsp *rsp_buf;

	if (!handle) {
		pr_err("invalid input\n");
		goto end;
	}

	if (!handle->ref_cnt) {
		pr_err("error: ref_cnt 0\n");
		goto end;
	}

	/* unloading app by sending hdcp_lib_deinit cmd */
	req_buf = (struct hdcp_deinit_req *)handle->
			qseecom_handle->sbuf;
	req_buf->commandid = HDCP_LIB_DEINIT;
	rsp_buf = (struct hdcp_deinit_rsp *)(handle->qseecom_handle->
		sbuf + QSEECOM_ALIGN(sizeof(struct hdcp_deinit_req)));

	rc = qseecom_send_command(handle->qseecom_handle,
		req_buf, QSEECOM_ALIGN(sizeof(struct hdcp_deinit_req)),
		rsp_buf, QSEECOM_ALIGN(sizeof(struct hdcp_deinit_rsp)));

	if (rc < 0) {
		pr_err("qseecom cmd failed err = %d\n", rc);
		goto end;
	}

	/* deallocate the resources for qseecom handle */
	rc = qseecom_shutdown_app(&handle->qseecom_handle);
	if (rc) {
		pr_err("qseecom_shutdown_app failed err: %d\n", rc);
		goto end;
	}

	pr_debug("unloading secure app success\n");

	handle->ref_cnt--;
	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
end:

	return rc;
}

static int hdcp2p2_txmtr_init(struct hdcp2p2_handle *handle)
{
	int rc = 0;
	struct hdcp_init_req *req_buf;
	struct hdcp_init_rsp *rsp_buf;

	if (!handle) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("app not loaded\n");
		goto exit;
	}

	if (handle->hdcp_state & HDCP_STATE_TXMTR_INIT) {
		pr_err("txmtr already initialized\n");
		goto exit;
	}

	/* send HDCP_Txmtr_Init command to TZ */
	req_buf = (struct hdcp_init_req *)handle->
			qseecom_handle->sbuf;
	req_buf->commandid = HDCP_TXMTR_INIT;
	rsp_buf = (struct hdcp_init_rsp *)(handle->qseecom_handle->
		sbuf + QSEECOM_ALIGN(sizeof(struct hdcp_init_req)));

	rc = qseecom_send_command(handle->qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_init_req)), rsp_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_init_rsp)));

	if ((rc < 0) || (rsp_buf->status != HDCP_SUCCESS) ||
		(rsp_buf->commandid != HDCP_TXMTR_INIT) ||
		(rsp_buf->msglen <= 0) || (rsp_buf->message == NULL)) {
		pr_err("qseecom cmd failed with err = %d, status = %d\n",
			rc, rsp_buf->status);
		rc = -1;
		goto exit;
	}

	/* send the response to HDMI driver */
	memset(handle->listener_buf, 0, MAX_TX_MESSAGE_SIZE);
	memcpy(handle->listener_buf, (unsigned char *)rsp_buf->message,
			rsp_buf->msglen);
	handle->msglen = rsp_buf->msglen;
	handle->hdcp_timeout = rsp_buf->timeout;

	handle->tz_ctxhandle = rsp_buf->ctxhandle;
	handle->hdcp_state |= HDCP_STATE_TXMTR_INIT;

	if (!atomic_read(&handle->hdcp_off))
		queue_work(handle->hdcp_workqueue, &handle->auth_work);
	else
		pr_err("not queueing auth work, hdcp off underway\n");

	pr_debug("hdcp txmtr successfully initialized\n");
exit:
	return rc;
}

static int hdcp2p2_txmtr_deinit(struct hdcp2p2_handle *handle)
{
	int rc = 0;
	struct hdcp_deinit_req *req_buf;
	struct hdcp_deinit_rsp *rsp_buf;

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("app not loaded\n");
		goto exit;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		/* unload library here */
		pr_err("txmtr not initialized\n");
		goto exit;
	}

	/* send command to TZ */
	req_buf = (struct hdcp_deinit_req *)handle->
			qseecom_handle->sbuf;
	req_buf->commandid = HDCP_TXMTR_DEINIT;
	req_buf->ctxhandle = handle->tz_ctxhandle;
	rsp_buf = (struct hdcp_deinit_rsp *)(handle->
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_deinit_req)));

	rc = qseecom_send_command(handle->qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_deinit_req)), rsp_buf,
			QSEECOM_ALIGN(sizeof(struct hdcp_deinit_rsp)));

	if ((rc < 0) || (rsp_buf->status < 0) ||
			(rsp_buf->commandid != HDCP_TXMTR_DEINIT)) {
		pr_err("qseecom cmd failed with err = %d status = %d\n",
					rc, rsp_buf->status);
		rc = -1;
		goto exit;
	}

	handle->hdcp_state &= ~HDCP_STATE_TXMTR_INIT;
	pr_debug("hdcp txmtr successfully deinitialized\n");
exit:
	return rc;
}

static bool hdcp2p2_client_feature_supported(void *phdcpcontext)
{
	int rc = 0;
	bool supported = false;
	struct hdcp2p2_handle *handle = phdcpcontext;

	if (!handle) {
		pr_err("invalid input\n");
		goto end;
	}

	rc = hdcp2p2_library_load(handle);
	if (!rc) {
		pr_debug("HDCP2p2 supported\n");
		hdcp2p2_library_unload(handle);
		supported = true;
	}
end:
	return supported;
}

static int hdcp2p2_client_start(void *phdcpcontext)
{
	int rc = 0;
	struct hdcp2p2_handle *handle = phdcpcontext;

	if (!handle) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	mutex_lock(&handle->hdcp_lock);

	handle->no_stored_km_flag = 0;
	handle->ske_flag = 0;
	handle->repeater_flag = 0;
	handle->ref_cnt = 0;
	handle->hdcp_state = HDCP_STATE_INIT;

	rc = hdcp2p2_library_load(handle);
	if (rc)
		pr_err("error loading library\n");
	else
		rc = hdcp2p2_txmtr_init(handle);

	mutex_unlock(&handle->hdcp_lock);

	return rc;
}

static int hdcp2p2_client_end(struct hdcp2p2_handle *handle)
{
	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->hdcp_lock);

	hdcp2p2_txmtr_deinit(handle);
	hdcp2p2_library_unload(handle);

	mutex_unlock(&handle->hdcp_lock);

	return 0;
}

static int hdcp2p2_client_stop(void *phdcpcontext)
{
	struct hdcp2p2_handle *handle = phdcpcontext;

	if (!handle)
		return -EINVAL;

	atomic_set(&handle->hdcp_off, 1);

	flush_work(&handle->auth_work);

	hdcp2p2_client_end(handle);

	atomic_set(&handle->hdcp_off, 0);

	return 0;
}

static void hdcp2p2_cleanup(void *phdcpcontext)
{
	struct hdcp2p2_handle *handle = phdcpcontext;

	if (!handle) {
		pr_err("invalid input\n");
		return;
	};

	if (atomic_read(&handle->hdcp_off)) {
		pr_err("hdcp off underway\n");
		return;
	}

	hdcp2p2_client_end(handle);

	/* notify client so that client can cleanup */
	handle->client_ops->hdcp_tz_error(handle->client_ctx);
}

static int hdcp2p2_txmtr_process_message(void *phdcpcontext,
		unsigned char *msg, uint32_t msglen)
{
	int rc = 0;
	struct hdcp2p2_handle *handle = phdcpcontext;
	struct hdcp_rcvd_msg_req *req_buf;
	struct hdcp_rcvd_msg_rsp *rsp_buf;

	if (!handle) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	if ((!msg) || (msglen <= 0)) {
		pr_err("invalid msg\n");
		rc = -EINVAL;
		goto exit;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp library is not loaded\n");
		goto exit;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		goto exit;
	}

	/* send the message to QSEECOM */
	req_buf = (struct hdcp_rcvd_msg_req *)(handle->
			qseecom_handle->sbuf);
	req_buf->commandid = HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE;
	memcpy(req_buf->msg, msg, msglen);
	req_buf->msglen = msglen;
	req_buf->ctxhandle = handle->tz_ctxhandle;

	rsp_buf = (struct hdcp_rcvd_msg_rsp *)(handle->
		qseecom_handle->sbuf + QSEECOM_ALIGN(sizeof(
			struct hdcp_rcvd_msg_req)));

	rc = qseecom_send_command(handle->qseecom_handle, req_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_rcvd_msg_req)), rsp_buf,
		QSEECOM_ALIGN(sizeof(struct hdcp_rcvd_msg_rsp)));

	/* No response was obtained from TZ */
	if ((msg[0] == AKE_SEND_H_PRIME_MESSAGE_ID) &&
			handle->no_stored_km_flag) {
		pr_debug("Got HPrime from tx, nothing sent to rx\n");
		goto exit;
	}

	if ((msg[0] == REPEATER_AUTH_STREAM_READY_MESSAGE_ID) &&
			(rc == 0) && (rsp_buf->status == 0)) {
		pr_debug("Got Auth_Stream_Ready, nothing sent to rx\n");
		goto exit;
	}

	if ((rc < 0) || (rsp_buf->status < 0) || (rsp_buf->msglen <= 0) ||
		(rsp_buf->commandid != HDCP_TXMTR_PROCESS_RECEIVED_MESSAGE) ||
				(rsp_buf->msg == NULL)) {
		pr_err("qseecom cmd failed with err=%d status=%d\n",
			rc, rsp_buf->status);
		rc = -1;
		goto exit;
	}

	pr_debug("message received is %s\n",
		hdcp2p2_message_name((int)rsp_buf->msg[0]));

	/* set the flag if response is AKE_No_Stored_km */
	if (((int)rsp_buf->msg[0] == AKE_NO_STORED_KM_MESSAGE_ID)) {
		pr_debug("Setting no_stored_km_flag\n");
		handle->no_stored_km_flag = 1;
	} else {
		handle->no_stored_km_flag = 0;
	}

	/*
	 * set ske flag is response is SKE_SEND_EKS
	 * also set repeater flag if it's a repeater
	 */
	handle->ske_flag = 0;
	if ((rsp_buf->msg[0] == SKE_SEND_EKS_MESSAGE_ID) &&
			(rsp_buf->msglen == SKE_SEND_EKS_MESSAGE_SIZE)) {
		handle->ske_flag = 1;
		/* check if it's a repeater */
		if ((rsp_buf->flag ==
			HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST) &&
						(rsp_buf->timeout > 0))
			handle->repeater_flag = 1;
	}

	memset(handle->listener_buf, 0, MAX_TX_MESSAGE_SIZE);
	memcpy(handle->listener_buf, (unsigned char *)rsp_buf->msg,
			rsp_buf->msglen);
	handle->hdcp_timeout = rsp_buf->timeout;
	handle->msglen = rsp_buf->msglen;

	if (!atomic_read(&handle->hdcp_off))
		queue_work(handle->hdcp_workqueue, &handle->auth_work);
	else
		pr_err("not queueing auth work, hdcp off underway\n");

	pr_debug("hdcp txmtr process message success\n");

exit:
	return rc;
}

/* APIs exposed to all clients */
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
		pr_err("qseecom_start_app failed %d\n", rc);
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
		pr_err("qseecom cmd failed err=%d\n", rc);
		return -ENOKEY;
	}

	rc = key_set_rsp->ret;
	if (rc) {
		pr_err("set key cmd failed, rsp=%d\n",
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

int hdcp_library_register(void **pphdcpcontext,
	struct hdcp_client_ops *client_ops,
	struct hdcp_txmtr_ops *txmtr_ops,
	void *client_ctx)
{
	int rc = 0;
	struct hdcp2p2_handle *handle = NULL;

	if (!pphdcpcontext) {
		pr_err("invalid input: context passed\n");
		return -EINVAL;
	}

	if (!txmtr_ops) {
		pr_err("invalid input: txmtr context\n");
		return -EINVAL;
	}

	if (!client_ops) {
		pr_err("invalid input: client_ops\n");
		return -EINVAL;
	}

	/* populate ops to be called by client */
	txmtr_ops->start = hdcp2p2_client_start;
	txmtr_ops->stop = hdcp2p2_client_stop;
	txmtr_ops->feature_supported = hdcp2p2_client_feature_supported;
	txmtr_ops->process_message = hdcp2p2_txmtr_process_message;
	txmtr_ops->hdcp_txmtr_query_stream_type =
		hdcp2p2_txmtr_query_stream_type;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle) {
		rc = -ENOMEM;
		goto unlock;
	}

	handle->client_ctx = client_ctx;
	handle->client_ops = client_ops;

	mutex_init(&handle->hdcp_lock);

	handle->hdcp_workqueue = create_singlethread_workqueue("hdcp-module");
	if (handle->hdcp_workqueue == NULL) {
		pr_err("hdcp wq creation failed\n");
		rc = -EFAULT;
		goto error;
	}

	INIT_WORK(&handle->auth_work, hdcp2p2_authenticate_work);

	handle->listener_buf = kzalloc(MAX_TX_MESSAGE_SIZE, GFP_KERNEL);
	if (!(handle->listener_buf)) {
		rc = -ENOMEM;
		goto error;
	}

	*((struct hdcp2p2_handle **)pphdcpcontext) = handle;

	pr_debug("hdcp lib successfully registered\n");

	return 0;
error:
	/* deallocate resources */
	if (handle->hdcp_workqueue != NULL) {
		destroy_workqueue(handle->hdcp_workqueue);
		handle->hdcp_workqueue = NULL;
	}

	kzfree(handle->listener_buf);
	handle->listener_buf = NULL;
	kzfree(handle);
	handle = NULL;
unlock:
	return rc;
}

void hdcp_library_deregister(void *phdcpcontext)
{
	struct hdcp2p2_handle *handle = phdcpcontext;

	if (!handle)
		return;

	kzfree(handle->qseecom_handle);

	if (handle->hdcp_workqueue)
		destroy_workqueue(handle->hdcp_workqueue);

	mutex_destroy(&handle->hdcp_lock);

	kzfree(handle->listener_buf);
	kzfree(handle);
}
