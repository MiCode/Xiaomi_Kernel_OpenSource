/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[mdss-hdcp-2x] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kthread.h>

#include "mdss_hdcp_2x.h"

/* all message IDs */
#define INVALID_MESSAGE        0
#define AKE_INIT               2
#define AKE_SEND_CERT          3
#define AKE_NO_STORED_KM       4
#define AKE_STORED_KM          5
#define AKE_SEND_H_PRIME       7
#define AKE_SEND_PAIRING_INFO  8
#define LC_INIT                9
#define LC_SEND_L_PRIME       10
#define SKE_SEND_EKS          11
#define REP_SEND_RECV_ID_LIST 12
#define REP_SEND_ACK          15
#define REP_STREAM_MANAGE     16
#define REP_STREAM_READY      17
#define SKE_SEND_TYPE_ID      18
#define HDCP2P2_MAX_MESSAGES  19

#define REAUTH_REQ BIT(3)
#define LINK_INTEGRITY_FAILURE BIT(4)

#define HDCP_2X_EXECUTE(x) { \
		kthread_queue_work(&hdcp->worker, &hdcp->wk_##x); \
}

struct mdss_hdcp_2x_ctrl {
	struct hdcp2_app_data app_data;
	u32 timeout_left;
	u32 total_message_length;
	bool no_stored_km;
	bool feature_supported;
	bool authenticated;
	void *client_data;
	void *hdcp2_ctx;
	struct hdcp_transport_ops *client_ops;
	struct mutex wakeup_mutex;
	enum mdss_hdcp_2x_wakeup_cmd wakeup_cmd;
	bool repeater_flag;
	bool update_stream;
	int last_msg;
	atomic_t hdcp_off;
	enum mdss_hdcp_2x_device_type device_type;

	struct task_struct *thread;
	struct completion topo_wait;

	struct kthread_worker worker;
	struct kthread_work wk_init;
	struct kthread_work wk_msg_sent;
	struct kthread_work wk_msg_recvd;
	struct kthread_work wk_timeout;
	struct kthread_work wk_clean;
	struct kthread_work wk_stream;
};

static const char *mdss_hdcp_2x_message_name(int msg_id)
{
	switch (msg_id) {
	case INVALID_MESSAGE:       return TO_STR(INVALID_MESSAGE);
	case AKE_INIT:              return TO_STR(AKE_INIT);
	case AKE_SEND_CERT:         return TO_STR(AKE_SEND_CERT);
	case AKE_NO_STORED_KM:      return TO_STR(AKE_NO_STORED_KM);
	case AKE_STORED_KM:         return TO_STR(AKE_STORED_KM);
	case AKE_SEND_H_PRIME:      return TO_STR(AKE_SEND_H_PRIME);
	case AKE_SEND_PAIRING_INFO: return TO_STR(AKE_SEND_PAIRING_INFO);
	case LC_INIT:               return TO_STR(LC_INIT);
	case LC_SEND_L_PRIME:       return TO_STR(LC_SEND_L_PRIME);
	case SKE_SEND_EKS:          return TO_STR(SKE_SEND_EKS);
	case REP_SEND_RECV_ID_LIST: return TO_STR(REP_SEND_RECV_ID_LIST);
	case REP_STREAM_MANAGE:     return TO_STR(REP_STREAM_MANAGE);
	case REP_STREAM_READY:      return TO_STR(REP_STREAM_READY);
	case SKE_SEND_TYPE_ID:      return TO_STR(SKE_SEND_TYPE_ID);
	default: return "UNKNOWN";
	}
}

static const struct mdss_hdcp_2x_msg_data
				hdcp_msg_lookup[HDCP2P2_MAX_MESSAGES] = {
	[AKE_INIT] = { 2,
		{ {"rtx", 0x69000, 8}, {"TxCaps", 0x69008, 3} },
		0 },
	[AKE_SEND_CERT] = { 3,
		{ {"cert-rx", 0x6900B, 522}, {"rrx", 0x69215, 8},
			{"RxCaps", 0x6921D, 3} },
		0 },
	[AKE_NO_STORED_KM] = { 1,
		{ {"Ekpub_km", 0x69220, 128} },
		0 },
	[AKE_STORED_KM] = { 2,
		{ {"Ekh_km", 0x692A0, 16}, {"m", 0x692B0, 16} },
		0 },
	[AKE_SEND_H_PRIME] = { 1,
		{ {"H'", 0x692C0, 32} },
		BIT(1) },
	[AKE_SEND_PAIRING_INFO] =  { 1,
		{ {"Ekh_km", 0x692E0, 16} },
		BIT(2) },
	[LC_INIT] = { 1,
		{ {"rn", 0x692F0, 8} },
		0 },
	[LC_SEND_L_PRIME] = { 1,
		{ {"L'", 0x692F8, 32} },
		0 },
	[SKE_SEND_EKS] = { 2,
		{ {"Edkey_ks", 0x69318, 16}, {"riv", 0x69328, 8} },
		0 },
	[SKE_SEND_TYPE_ID] = { 1,
		{ {"type", 0x69494, 1} },
		0 },
	[REP_SEND_RECV_ID_LIST] = { 4,
		{ {"RxInfo", 0x69330, 2}, {"seq_num_V", 0x69332, 3},
			{"V'", 0x69335, 16}, {"ridlist", 0x69345, 155} },
		BIT(0) },
	[REP_SEND_ACK] = { 1,
		{ {"V", 0x693E0, 16} },
		0 },
	[REP_STREAM_MANAGE] = { 3,
		{ {"seq_num_M", 0x693F0, 3}, {"k", 0x693F3, 2},
			{"streamID_Type", 0x693F5, 126} },
		0 },
	[REP_STREAM_READY] = { 1,
		{ {"M'", 0x69473, 32} },
		0 },
};

static void mdss_hdcp_2x_check_worker_status(struct mdss_hdcp_2x_ctrl *hdcp)
{
	if (!list_empty(&hdcp->wk_init.node))
		pr_debug("init work queued\n");

	if (hdcp->worker.current_work == &hdcp->wk_init)
		pr_debug("init work executing\n");

	if (!list_empty(&hdcp->wk_msg_sent.node))
		pr_debug("msg_sent work queued\n");

	if (hdcp->worker.current_work == &hdcp->wk_msg_sent)
		pr_debug("msg_sent work executing\n");

	if (!list_empty(&hdcp->wk_msg_recvd.node))
		pr_debug("msg_recvd work queued\n");

	if (hdcp->worker.current_work == &hdcp->wk_msg_recvd)
		pr_debug("msg_recvd work executing\n");

	if (!list_empty(&hdcp->wk_timeout.node))
		pr_debug("timeout work queued\n");

	if (hdcp->worker.current_work == &hdcp->wk_timeout)
		pr_debug("timeout work executing\n");

	if (!list_empty(&hdcp->wk_clean.node))
		pr_debug("clean work queued\n");

	if (hdcp->worker.current_work == &hdcp->wk_clean)
		pr_debug("clean work executing\n");

	if (!list_empty(&hdcp->wk_stream.node))
		pr_debug("stream work queued\n");

	if (hdcp->worker.current_work == &hdcp->wk_stream)
		pr_debug("stream work executing\n");
}

static int mdss_hdcp_2x_get_next_message(struct mdss_hdcp_2x_ctrl *hdcp,
				     struct hdcp_transport_wakeup_data *data)
{
	switch (hdcp->last_msg) {
	case INVALID_MESSAGE:
		return AKE_INIT;
	case AKE_INIT:
		return AKE_SEND_CERT;
	case AKE_SEND_CERT:
		if (hdcp->no_stored_km)
			return AKE_NO_STORED_KM;
		else
			return AKE_STORED_KM;
	case AKE_STORED_KM:
	case AKE_NO_STORED_KM:
		return AKE_SEND_H_PRIME;
	case AKE_SEND_H_PRIME:
		if (hdcp->no_stored_km)
			return AKE_SEND_PAIRING_INFO;
		else
			return LC_INIT;
	case AKE_SEND_PAIRING_INFO:
		return LC_INIT;
	case LC_INIT:
		return LC_SEND_L_PRIME;
	case LC_SEND_L_PRIME:
		return SKE_SEND_EKS;
	case SKE_SEND_EKS:
		if (!hdcp->repeater_flag)
			return SKE_SEND_TYPE_ID;
	case SKE_SEND_TYPE_ID:
	case REP_STREAM_READY:
	case REP_SEND_ACK:
		if (!hdcp->repeater_flag)
			return INVALID_MESSAGE;

		if (data->cmd == HDCP_TRANSPORT_CMD_SEND_MESSAGE)
			return REP_STREAM_MANAGE;
		else
			return REP_SEND_RECV_ID_LIST;
	case REP_SEND_RECV_ID_LIST:
		return REP_SEND_ACK;
	case REP_STREAM_MANAGE:
		return REP_STREAM_READY;
	default:
		pr_err("Uknown message ID (%d)", hdcp->last_msg);
		return -EINVAL;
	}
}

static void mdss_hdcp_2x_wakeup_client(struct mdss_hdcp_2x_ctrl *hdcp,
				struct hdcp_transport_wakeup_data *data)
{
	int rc = 0;

	if (hdcp && hdcp->client_ops && hdcp->client_ops->wakeup &&
	    data && (data->cmd != HDCP_TRANSPORT_CMD_INVALID)) {
		data->abort_mask = REAUTH_REQ | LINK_INTEGRITY_FAILURE;

		if (data->cmd == HDCP_TRANSPORT_CMD_SEND_MESSAGE ||
		    data->cmd == HDCP_TRANSPORT_CMD_RECV_MESSAGE ||
		    data->cmd == HDCP_TRANSPORT_CMD_LINK_POLL) {
			hdcp->last_msg =
				mdss_hdcp_2x_get_next_message(hdcp, data);
			if (hdcp->last_msg <= INVALID_MESSAGE) {
				hdcp->last_msg = INVALID_MESSAGE;
				return;
			}

			data->message_data = &hdcp_msg_lookup[hdcp->last_msg];
		}

		rc = hdcp->client_ops->wakeup(data);
		if (rc)
			pr_err("error sending %s to hdcp client\n",
			       hdcp_transport_cmd_to_str(data->cmd));
	}
}

static inline void mdss_hdcp_2x_send_message(struct mdss_hdcp_2x_ctrl *hdcp)
{
	char msg_name[50];
	struct hdcp_transport_wakeup_data cdata = {
					HDCP_TRANSPORT_CMD_SEND_MESSAGE };

	cdata.context = hdcp->client_data;
	cdata.timeout = hdcp->app_data.timeout;
	cdata.buf_len = hdcp->app_data.response.length;

	/* ignore the first byte as it contains the message id */
	cdata.buf = hdcp->app_data.response.data + 1;

	snprintf(msg_name, sizeof(msg_name), "%s: ",
		mdss_hdcp_2x_message_name(hdcp->app_data.response.data[0]));

	print_hex_dump(KERN_DEBUG, msg_name,
		DUMP_PREFIX_NONE, 16, 1, cdata.buf,
		cdata.buf_len, false);

	mdss_hdcp_2x_wakeup_client(hdcp, &cdata);
}

static bool mdss_hdcp_2x_client_feature_supported(void *data)
{
	struct mdss_hdcp_2x_ctrl *hdcp = data;

	return hdcp2_feature_supported(hdcp->hdcp2_ctx);
}

static int mdss_hdcp_2x_check_valid_state(struct mdss_hdcp_2x_ctrl *hdcp)
{
	int rc = 0;

	if (!list_empty(&hdcp->worker.work_list))
		mdss_hdcp_2x_check_worker_status(hdcp);

	if (hdcp->wakeup_cmd == HDCP_2X_CMD_START) {
		if (!list_empty(&hdcp->worker.work_list)) {
			rc = -EBUSY;
			goto exit;
		}
	} else {
		if (atomic_read(&hdcp->hdcp_off)) {
			pr_debug("hdcp2.2 session tearing down\n");
			goto exit;
		}
	}
exit:
	return rc;
}

static void mdss_hdcp_2x_clean(struct mdss_hdcp_2x_ctrl *hdcp)
{
	struct hdcp_transport_wakeup_data cdata = {
						HDCP_TRANSPORT_CMD_INVALID };

	hdcp->authenticated = false;

	hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_STOP, &hdcp->app_data);

	cdata.context = hdcp->client_data;
	cdata.cmd = HDCP_TRANSPORT_CMD_STATUS_FAILED;

	if (!atomic_read(&hdcp->hdcp_off))
		mdss_hdcp_2x_wakeup_client(hdcp, &cdata);

	atomic_set(&hdcp->hdcp_off, 1);
}

static void mdss_hdcp_2x_cleanup_work(struct kthread_work *work)
{

	struct mdss_hdcp_2x_ctrl *hdcp =
		container_of(work, struct mdss_hdcp_2x_ctrl, wk_clean);

	mdss_hdcp_2x_clean(hdcp);
}

static void mdss_hdcp_2x_stream(struct mdss_hdcp_2x_ctrl *hdcp)
{
	int rc = 0;

	if (atomic_read(&hdcp->hdcp_off)) {
		pr_debug("invalid state, hdcp off\n");
		return;
	}

	if (!hdcp->repeater_flag) {
		pr_debug("invalid state, not a repeater\n");
		return;
	}

	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_QUERY_STREAM,
			&hdcp->app_data);
	if (rc)
		goto exit;

	if (!hdcp->app_data.response.data || !hdcp->app_data.request.data) {
		pr_err("invalid response/request buffers\n");
		rc = -EINVAL;
		goto exit;
	}

	pr_debug("message received from TZ: %s\n",
		 mdss_hdcp_2x_message_name(hdcp->app_data.response.data[0]));
exit:
	if (!rc && !atomic_read(&hdcp->hdcp_off))
		mdss_hdcp_2x_send_message(hdcp);
}

static void mdss_hdcp_2x_query_stream_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_ctrl *hdcp =
		container_of(work, struct mdss_hdcp_2x_ctrl, wk_stream);

	mdss_hdcp_2x_stream(hdcp);
}

static void mdss_hdcp_2x_initialize_command(struct mdss_hdcp_2x_ctrl *hdcp,
		enum hdcp_transport_wakeup_cmd cmd,
		struct hdcp_transport_wakeup_data *cdata)
{
		cdata->cmd = cmd;
		cdata->timeout = hdcp->timeout_left;
		cdata->buf = hdcp->app_data.request.data + 1;
}

static void mdss_hdcp_2x_msg_sent(struct mdss_hdcp_2x_ctrl *hdcp)
{
	struct hdcp_transport_wakeup_data cdata = {
						HDCP_TRANSPORT_CMD_INVALID };
	cdata.context = hdcp->client_data;

	switch (hdcp->app_data.response.data[0]) {
	case SKE_SEND_TYPE_ID:
		if (!hdcp2_app_comm(hdcp->hdcp2_ctx,
				HDCP2_CMD_SET_HW_KEY, &hdcp->app_data)) {
			hdcp->authenticated = true;

			cdata.cmd = HDCP_TRANSPORT_CMD_STATUS_SUCCESS;
			mdss_hdcp_2x_wakeup_client(hdcp, &cdata);
		}

		/* poll for link check */
		mdss_hdcp_2x_initialize_command(hdcp,
				HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		break;
	case SKE_SEND_EKS:
		if (hdcp->repeater_flag && !atomic_read(&hdcp->hdcp_off)) {
			/* poll for link check */
			mdss_hdcp_2x_initialize_command(hdcp,
					HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		} else {
			hdcp->app_data.response.data[0] = SKE_SEND_TYPE_ID;
			hdcp->app_data.response.length = 2;
			hdcp->app_data.timeout = 100;

			mdss_hdcp_2x_send_message(hdcp);
		}
		break;
	case REP_SEND_ACK:
		pr_debug("Repeater authentication successful\n");

		if (hdcp->update_stream) {
			HDCP_2X_EXECUTE(stream);
			hdcp->update_stream = false;
		} else {
			mdss_hdcp_2x_initialize_command(hdcp,
					HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		}
		break;
	default:
		cdata.cmd = HDCP_TRANSPORT_CMD_RECV_MESSAGE;
		cdata.timeout = hdcp->timeout_left;
		cdata.buf = hdcp->app_data.request.data + 1;
	}

	mdss_hdcp_2x_wakeup_client(hdcp, &cdata);
}

static void mdss_hdcp_2x_msg_sent_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_ctrl *hdcp =
		container_of(work, struct mdss_hdcp_2x_ctrl, wk_msg_sent);

	if (hdcp->wakeup_cmd != HDCP_2X_CMD_MSG_SEND_SUCCESS) {
		pr_err("invalid wakeup command %d\n", hdcp->wakeup_cmd);
		return;
	}

	mdss_hdcp_2x_msg_sent(hdcp);
}

static void mdss_hdcp_2x_init(struct mdss_hdcp_2x_ctrl *hdcp)
{
	int rc = 0;

	if (hdcp->wakeup_cmd != HDCP_2X_CMD_START) {
		pr_err("invalid wakeup command %d\n", hdcp->wakeup_cmd);
		return;
	}

	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_START, &hdcp->app_data);
	if (rc)
		goto exit;

	pr_debug("message received from TZ: %s\n",
		 mdss_hdcp_2x_message_name(hdcp->app_data.response.data[0]));

	mdss_hdcp_2x_send_message(hdcp);

	return;
exit:
	HDCP_2X_EXECUTE(clean);
}

static void mdss_hdcp_2x_init_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_ctrl *hdcp =
		container_of(work, struct mdss_hdcp_2x_ctrl, wk_init);

	mdss_hdcp_2x_init(hdcp);
}

static void mdss_hdcp_2x_timeout(struct mdss_hdcp_2x_ctrl *hdcp)
{
	int rc = 0;
	int message_id;

	if (atomic_read(&hdcp->hdcp_off)) {
		pr_debug("invalid state, hdcp off\n");
		return;
	}

	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_TIMEOUT,
			&hdcp->app_data);
	if (rc)
		goto error;

	message_id = (int)hdcp->app_data.response.data[0];
	if (message_id == LC_INIT && !atomic_read(&hdcp->hdcp_off))
		mdss_hdcp_2x_send_message(hdcp);
	return;
error:
	if (!atomic_read(&hdcp->hdcp_off))
		HDCP_2X_EXECUTE(clean);
}

static void mdss_hdcp_2x_timeout_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_ctrl *hdcp =
		container_of(work, struct mdss_hdcp_2x_ctrl, wk_timeout);

	mdss_hdcp_2x_timeout(hdcp);
}

static void mdss_hdcp_2x_msg_recvd(struct mdss_hdcp_2x_ctrl *hdcp)
{
	int rc = 0;
	char *msg = NULL;
	char msg_name[50];
	u32 message_id_bytes = 0;
	u32 request_length, out_msg;
	struct hdcp_transport_wakeup_data cdata = {
						HDCP_TRANSPORT_CMD_INVALID };

	if (atomic_read(&hdcp->hdcp_off)) {
		pr_debug("invalid state, hdcp off\n");
		return;
	}

	cdata.context = hdcp->client_data;

	request_length = hdcp->total_message_length;
	msg = hdcp->app_data.request.data;

	if (request_length == 0) {
		pr_err("invalid message length\n");
		goto exit;
	}

	if (hdcp->device_type == HDCP_TXMTR_DP) {
		msg[0] = hdcp->last_msg;
		message_id_bytes = 1;
	}

	request_length += message_id_bytes;

	snprintf(msg_name, sizeof(msg_name), "%s: ",
		mdss_hdcp_2x_message_name((int)msg[0]));

	print_hex_dump(KERN_DEBUG, msg_name,
		DUMP_PREFIX_NONE, 16, 1, msg, request_length, false);

	pr_debug("message received from SINK: %s\n",
			mdss_hdcp_2x_message_name(msg[0]));

	hdcp->app_data.request.length = request_length;
	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_PROCESS_MSG,
			&hdcp->app_data);
	if (rc) {
		pr_err("failed to process message from hdcp sink (%d)\n", rc);
		rc = -EINVAL;
		goto exit;
	}

	if (msg[0] == AKE_SEND_H_PRIME && hdcp->no_stored_km) {
		cdata.cmd = HDCP_TRANSPORT_CMD_RECV_MESSAGE;
		cdata.timeout = hdcp->app_data.timeout;
		cdata.buf = hdcp->app_data.request.data + 1;
		goto exit;
	}

	if (msg[0] == REP_STREAM_READY) {
		if (!hdcp->authenticated) {
			rc = hdcp2_app_comm(hdcp->hdcp2_ctx,
					HDCP2_CMD_SET_HW_KEY,
					&hdcp->app_data);
			if (!rc) {
				hdcp->authenticated = true;

				cdata.cmd = HDCP_TRANSPORT_CMD_STATUS_SUCCESS;
				mdss_hdcp_2x_wakeup_client(hdcp, &cdata);
			} else {
				pr_err("failed to enable encryption (%d)\n",
						rc);
			}
		}

		mdss_hdcp_2x_initialize_command(hdcp,
				HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		goto exit;
	}

	out_msg = (u32)hdcp->app_data.response.data[0];

	pr_debug("message received from TZ: %s\n",
			mdss_hdcp_2x_message_name(out_msg));

	if (out_msg == AKE_NO_STORED_KM)
		hdcp->no_stored_km = 1;
	else
		hdcp->no_stored_km = 0;

	if (out_msg == SKE_SEND_EKS) {
		hdcp->repeater_flag = hdcp->app_data.repeater_flag;
		hdcp->update_stream = true;
	}

	if (!atomic_read(&hdcp->hdcp_off)) {
		pr_debug("creating client data for: %s\n",
				mdss_hdcp_2x_message_name(out_msg));
		cdata.cmd = HDCP_TRANSPORT_CMD_SEND_MESSAGE;
		cdata.buf = hdcp->app_data.response.data + 1;
		cdata.buf_len = hdcp->app_data.response.length;
		cdata.timeout = hdcp->app_data.timeout;
	}
exit:
	mdss_hdcp_2x_wakeup_client(hdcp, &cdata);

	if (rc && !atomic_read(&hdcp->hdcp_off))
		HDCP_2X_EXECUTE(clean);
}

static void mdss_hdcp_2x_msg_recvd_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_ctrl *hdcp =
		container_of(work, struct mdss_hdcp_2x_ctrl, wk_msg_recvd);

	mdss_hdcp_2x_msg_recvd(hdcp);
}

static int mdss_hdcp_2x_wakeup(struct mdss_hdcp_2x_wakeup_data *data)
{
	struct mdss_hdcp_2x_ctrl *hdcp;
	int rc = 0;

	if (!data)
		return -EINVAL;

	hdcp = data->context;
	if (!hdcp)
		return -EINVAL;

	mutex_lock(&hdcp->wakeup_mutex);

	hdcp->wakeup_cmd = data->cmd;
	hdcp->timeout_left = data->timeout;
	hdcp->total_message_length = data->total_message_length;

	pr_debug("%s\n", mdss_hdcp_2x_cmd_to_str(hdcp->wakeup_cmd));

	rc = mdss_hdcp_2x_check_valid_state(hdcp);
	if (rc)
		goto exit;

	if (!completion_done(&hdcp->topo_wait))
		complete_all(&hdcp->topo_wait);

	switch (hdcp->wakeup_cmd) {
	case HDCP_2X_CMD_START:
		hdcp->no_stored_km = 0;
		hdcp->repeater_flag = false;
		hdcp->update_stream = false;
		hdcp->last_msg = INVALID_MESSAGE;
		hdcp->timeout_left = 0;
		atomic_set(&hdcp->hdcp_off, 0);

		HDCP_2X_EXECUTE(init);
		break;
	case HDCP_2X_CMD_STOP:
		atomic_set(&hdcp->hdcp_off, 1);

		HDCP_2X_EXECUTE(clean);
		break;
	case HDCP_2X_CMD_MSG_SEND_SUCCESS:
		HDCP_2X_EXECUTE(msg_sent);
		break;
	case HDCP_2X_CMD_MSG_SEND_FAILED:
	case HDCP_2X_CMD_MSG_RECV_FAILED:
	case HDCP_2X_CMD_LINK_FAILED:
		HDCP_2X_EXECUTE(clean);
		break;
	case HDCP_2X_CMD_MSG_RECV_SUCCESS:
		HDCP_2X_EXECUTE(msg_recvd);
		break;
	case HDCP_2X_CMD_MSG_RECV_TIMEOUT:
		HDCP_2X_EXECUTE(timeout);
		break;
	case HDCP_2X_CMD_QUERY_STREAM_TYPE:
		HDCP_2X_EXECUTE(stream);
		break;
	default:
		pr_err("invalid wakeup command %d\n", hdcp->wakeup_cmd);
	}
exit:
	mutex_unlock(&hdcp->wakeup_mutex);

	return rc;
}

int mdss_hdcp_2x_register(struct mdss_hdcp_2x_register_data *data)
{
	int rc = 0;
	struct mdss_hdcp_2x_ctrl *hdcp = NULL;

	if (!data) {
		pr_err("invalid hdcp init data\n");
		return -EINVAL;
	}

	if (!data->ops) {
		pr_err("invalid input: txmtr context\n");
		return -EINVAL;
	}

	if (!data->client_ops) {
		pr_err("invalid input: client_ops\n");
		return -EINVAL;
	}

	if (!data->hdcp_data) {
		pr_err("invalid input: hdcp_data\n");
		return -EINVAL;
	}

	/* populate ops to be called by client */
	data->ops->feature_supported = mdss_hdcp_2x_client_feature_supported;
	data->ops->wakeup = mdss_hdcp_2x_wakeup;

	hdcp = kzalloc(sizeof(*hdcp), GFP_KERNEL);
	if (!hdcp) {
		rc = -ENOMEM;
		goto unlock;
	}

	hdcp->client_data = data->client_data;
	hdcp->client_ops = data->client_ops;
	hdcp->device_type = data->device_type;

	hdcp->hdcp2_ctx = hdcp2_init(hdcp->device_type);

	atomic_set(&hdcp->hdcp_off, 0);

	mutex_init(&hdcp->wakeup_mutex);

	kthread_init_worker(&hdcp->worker);

	kthread_init_work(&hdcp->wk_init,      mdss_hdcp_2x_init_work);
	kthread_init_work(&hdcp->wk_msg_sent,  mdss_hdcp_2x_msg_sent_work);
	kthread_init_work(&hdcp->wk_msg_recvd, mdss_hdcp_2x_msg_recvd_work);
	kthread_init_work(&hdcp->wk_timeout,   mdss_hdcp_2x_timeout_work);
	kthread_init_work(&hdcp->wk_clean,     mdss_hdcp_2x_cleanup_work);
	kthread_init_work(&hdcp->wk_stream,    mdss_hdcp_2x_query_stream_work);

	init_completion(&hdcp->topo_wait);

	*data->hdcp_data = hdcp;

	hdcp->thread = kthread_run(kthread_worker_fn,
			     &hdcp->worker, "hdcp_tz_lib");

	if (IS_ERR(hdcp->thread)) {
		pr_err("unable to start lib thread\n");
		rc = PTR_ERR(hdcp->thread);
		hdcp->thread = NULL;
		goto error;
	}

	return 0;
error:
	kzfree(hdcp);
	hdcp = NULL;
unlock:
	return rc;
}

void mdss_hdcp_2x_deregister(void *data)
{
	struct mdss_hdcp_2x_ctrl *hdcp = data;

	if (!hdcp)
		return;

	kthread_stop(hdcp->thread);
	mutex_destroy(&hdcp->wakeup_mutex);
	hdcp2_deinit(hdcp->hdcp2_ctx);
	kzfree(hdcp);
}
