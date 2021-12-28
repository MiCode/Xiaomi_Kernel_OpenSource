// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[sde-hdcp-2x] %s: " fmt, __func__

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
#include <linux/kfifo.h>

#include "sde_hdcp_2x.h"

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

/* Temporary define to override wrong TZ value */
#define AKE_SEND_CERT_MSG_DELAY 100

struct sde_hdcp_2x_ctrl {
	DECLARE_KFIFO(cmd_q, enum sde_hdcp_2x_wakeup_cmd, 8);
	wait_queue_head_t wait_q;
	struct hdcp2_app_data app_data;
	u32 timeout_left;
	u32 wait_timeout_ms;
	u32 total_message_length;
	atomic_t enable_pending;
	bool no_stored_km;
	bool feature_supported;
	bool force_encryption;
	bool authenticated;
	bool resend_lc_init;
	bool resend_stream_manage;
	void *client_data;
	void *hdcp2_ctx;
	struct hdcp_transport_ops *client_ops;
	bool repeater_flag;
	bool update_stream;
	int last_msg;
	atomic_t hdcp_off;
	enum sde_hdcp_2x_device_type device_type;
	u8 min_enc_level;
	struct list_head stream_handles;
	u8 stream_count;

	struct task_struct *thread;
	struct completion response_completion;
};

static void sde_hdcp_2x_clean(struct sde_hdcp_2x_ctrl *hdcp);

static const char *sde_hdcp_2x_message_name(int msg_id)
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
	default:
		return "UNKNOWN";
	}
}

static const struct sde_hdcp_2x_msg_data
				hdcp_msg_lookup[HDCP2P2_MAX_MESSAGES] = {
	[AKE_INIT] = { 2,
		{ {"rtx", 0x69000, 8}, {"TxCaps", 0x69008, 3} },
		0, 0 },
	[AKE_SEND_CERT] = { 3,
		{ {"cert-rx", 0x6900B, 522}, {"rrx", 0x69215, 8},
			{"RxCaps", 0x6921D, 3} },
		0, 110 },
	[AKE_NO_STORED_KM] = { 1,
		{ {"Ekpub_km", 0x69220, 128} },
		0, 0 },
	[AKE_STORED_KM] = { 2,
		{ {"Ekh_km", 0x692A0, 16}, {"m", 0x692B0, 16} },
		0, 0 },
	[AKE_SEND_H_PRIME] = { 1,
		{ {"H'", 0x692C0, 32} },
		(1 << 1), 7 },
	[AKE_SEND_PAIRING_INFO] =  { 1,
		{ {"Ekh_km", 0x692E0, 16} },
		(1 << 2), 5 },
	[LC_INIT] = { 1,
		{ {"rn", 0x692F0, 8} },
		0, 0 },
	[LC_SEND_L_PRIME] = { 1,
		{ {"L'", 0x692F8, 32} },
		0, 0 },
	[SKE_SEND_EKS] = { 2,
		{ {"Edkey_ks", 0x69318, 16}, {"riv", 0x69328, 8} },
		0, 0 },
	[SKE_SEND_TYPE_ID] = { 1,
		{ {"type", 0x69494, 1} },
		0, 0 },
	[REP_SEND_RECV_ID_LIST] = { 4,
		{ {"RxInfo", 0x69330, 2}, {"seq_num_V", 0x69332, 3},
			{"V'", 0x69335, 16}, {"ridlist", 0x69345, 155} },
		(1 << 0), 0 },
	[REP_SEND_ACK] = { 1,
		{ {"V", 0x693E0, 16} },
		0, 0 },
	[REP_STREAM_MANAGE] = { 3,
		{ {"seq_num_M", 0x693F0, 3}, {"k", 0x693F3, 2},
			{"streamID_Type", 0x693F5, 126} },
		0, 0 },
	[REP_STREAM_READY] = { 1,
		{ {"M'", 0x69473, 32} },
		0, 7 },
};

static int sde_hdcp_2x_get_next_message(struct sde_hdcp_2x_ctrl *hdcp,
				     struct hdcp_transport_wakeup_data *data)
{
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, hdcp->last_msg);
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
		if (hdcp->resend_lc_init)
			return LC_INIT;
		else
			return SKE_SEND_EKS;
	case SKE_SEND_EKS:
		if (!hdcp->repeater_flag)
			return SKE_SEND_TYPE_ID;
	case SKE_SEND_TYPE_ID:
		if (!hdcp->repeater_flag)
			return SKE_SEND_TYPE_ID;
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
		hdcp->resend_stream_manage = false;
		return REP_STREAM_READY;
	default:
		pr_err("Unknown message ID (%d)\n", hdcp->last_msg);
		return -EINVAL;
	}

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT);

	return -EINVAL;
}

static void sde_hdcp_2x_wait_for_response(struct sde_hdcp_2x_ctrl *hdcp)
{
	u32 timeout;

	switch (hdcp->last_msg) {
	case AKE_SEND_H_PRIME:
		if (hdcp->no_stored_km)
			hdcp->wait_timeout_ms = HZ;
		else
			hdcp->wait_timeout_ms = HZ / 4;
		break;
	case AKE_SEND_PAIRING_INFO:
		hdcp->wait_timeout_ms = HZ / 4;
		break;
	case REP_SEND_RECV_ID_LIST:
		if (!hdcp->authenticated)
			hdcp->wait_timeout_ms = HZ * 3;
		else
			hdcp->wait_timeout_ms = 0;
		break;
	default:
		hdcp->wait_timeout_ms = 0;
	}

	if (!hdcp->wait_timeout_ms)
		return;

	if (atomic_read(&hdcp->hdcp_off)) {
		pr_debug("invalid state: hdcp off\n");
		return;
	}

	reinit_completion(&hdcp->response_completion);
	timeout = wait_for_completion_timeout(&hdcp->response_completion,
			hdcp->wait_timeout_ms);
	if (!timeout) {
		pr_err("completion expired, last message = %s\n",
				sde_hdcp_2x_message_name(hdcp->last_msg));

		if (!atomic_read(&hdcp->hdcp_off))
			sde_hdcp_2x_clean(hdcp);
	}

	hdcp->wait_timeout_ms = 0;
}

static void sde_hdcp_2x_adjust_transaction_params(
		struct sde_hdcp_2x_ctrl *hdcp,
		struct hdcp_transport_wakeup_data *data)
{
	switch (hdcp->last_msg) {
	case AKE_SEND_CERT:
		data->transaction_delay = AKE_SEND_CERT_MSG_DELAY;
		break;
	case REP_STREAM_READY:
		break;
	default:
		data->transaction_delay = 0;
		break;
	}

	data->transaction_timeout =
			hdcp_msg_lookup[hdcp->last_msg].transaction_timeout;

	pr_debug("%s: transaction delay: %ums, transaction timeout: %ums\n",
			sde_hdcp_2x_message_name(hdcp->last_msg),
			data->transaction_delay, data->transaction_timeout);
}

static void sde_hdcp_2x_wakeup_client(struct sde_hdcp_2x_ctrl *hdcp,
				struct hdcp_transport_wakeup_data *data)
{
	int rc = 0;

	if (!hdcp || !hdcp->client_ops || !hdcp->client_ops->wakeup ||
			!data || (data->cmd == HDCP_TRANSPORT_CMD_INVALID))
		return;

	data->abort_mask = REAUTH_REQ | LINK_INTEGRITY_FAILURE;

	if (data->cmd == HDCP_TRANSPORT_CMD_SEND_MESSAGE ||
			data->cmd == HDCP_TRANSPORT_CMD_RECV_MESSAGE ||
			data->cmd == HDCP_TRANSPORT_CMD_LINK_POLL) {
		hdcp->last_msg =
			sde_hdcp_2x_get_next_message(hdcp, data);
		if (hdcp->last_msg <= INVALID_MESSAGE) {
			hdcp->last_msg = INVALID_MESSAGE;
			return;
		}

		data->message_data = &hdcp_msg_lookup[hdcp->last_msg];
	}

	sde_hdcp_2x_adjust_transaction_params(hdcp, data);

	rc = hdcp->client_ops->wakeup(data);
	if (rc)
		pr_err("error sending %s to client\n",
				hdcp_transport_cmd_to_str(data->cmd));

	sde_hdcp_2x_wait_for_response(hdcp);
}

static inline void sde_hdcp_2x_send_message(struct sde_hdcp_2x_ctrl *hdcp)
{
	struct hdcp_transport_wakeup_data cdata = {
					HDCP_TRANSPORT_CMD_SEND_MESSAGE };

	cdata.context = hdcp->client_data;
	cdata.transaction_delay = hdcp->app_data.timeout;
	cdata.buf_len = hdcp->app_data.response.length;

	/* ignore the first byte as it contains the message id */
	cdata.buf = hdcp->app_data.response.data + 1;

	sde_hdcp_2x_wakeup_client(hdcp, &cdata);
}

static bool sde_hdcp_2x_client_feature_supported(void *data)
{
	struct sde_hdcp_2x_ctrl *hdcp = data;

	while (atomic_read(&hdcp->enable_pending))
		usleep_range(1000, 1500);

	return hdcp2_feature_supported(hdcp->hdcp2_ctx);
}

static void sde_hdcp_2x_force_encryption(void *data, bool enable)
{
	struct sde_hdcp_2x_ctrl *hdcp = data;

	if (!hdcp) {
		pr_err("invalid input\n");
		return;
	}

	hdcp->force_encryption = enable;
	pr_info("force_encryption=%d\n", hdcp->force_encryption);
}

static void sde_hdcp_2x_clean(struct sde_hdcp_2x_ctrl *hdcp)
{
	struct list_head *element;
	struct sde_hdcp_stream *stream_entry;
	struct hdcp_transport_wakeup_data cdata = {HDCP_TRANSPORT_CMD_INVALID};

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, hdcp->authenticated);
	hdcp->authenticated = false;

	cdata.context = hdcp->client_data;
	cdata.cmd = HDCP_TRANSPORT_CMD_STATUS_FAILED;

	while (!list_empty(&hdcp->stream_handles)) {
		element = hdcp->stream_handles.next;
		list_del(element);

		stream_entry = list_entry(element, struct sde_hdcp_stream,
			list);
		hdcp2_close_stream(hdcp->hdcp2_ctx,
			stream_entry->stream_handle);
		kfree(stream_entry);
		hdcp->stream_count--;
	}

	if (!atomic_xchg(&hdcp->hdcp_off, 1))
		sde_hdcp_2x_wakeup_client(hdcp, &cdata);

	hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_STOP, &hdcp->app_data);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, hdcp->authenticated);
}

static u8 sde_hdcp_2x_stream_type(u8 min_enc_level)
{
	u8 stream_type = 0;

	switch (min_enc_level) {
	case 0:
	case 1:
		stream_type = 0;
		break;
	case 2:
		stream_type = 1;
		break;
	default:
		stream_type = 0;
		break;
	}

	pr_debug("min_enc_level = %u, type = %u\n", min_enc_level, stream_type);

	return stream_type;
}

static void sde_hdcp_2x_send_type(struct sde_hdcp_2x_ctrl *hdcp)
{
	if (atomic_read(&hdcp->hdcp_off)) {
		pr_debug("invalid state, hdcp off\n");
		return;
	}

	if (hdcp->repeater_flag) {
		pr_debug("invalid state, not receiver\n");
		return;
	}

	hdcp->app_data.response.data[0] = SKE_SEND_TYPE_ID;
	hdcp->app_data.response.data[1] =
		sde_hdcp_2x_stream_type(hdcp->min_enc_level);
	hdcp->app_data.response.length = 1;
	hdcp->app_data.timeout = 100;

	if (!atomic_read(&hdcp->hdcp_off))
		sde_hdcp_2x_send_message(hdcp);
}

static void sde_hdcp_2x_query_stream(struct sde_hdcp_2x_ctrl *hdcp)
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

	if (!hdcp->authenticated &&
			hdcp->app_data.response.data[0] != REP_SEND_ACK) {
		pr_debug("invalid state. HDCP repeater not authenticated\n");
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

	pr_debug("[tz]: %s\n", sde_hdcp_2x_message_name(
		hdcp->app_data.response.data[0]));
exit:
	if (!rc && !atomic_read(&hdcp->hdcp_off)) {
		/* Modify last message to ensure the proper message is sent */
		hdcp->last_msg = REP_SEND_ACK;
		sde_hdcp_2x_send_message(hdcp);
	}
}

static void sde_hdcp_2x_initialize_command(struct sde_hdcp_2x_ctrl *hdcp,
		enum hdcp_transport_wakeup_cmd cmd,
		struct hdcp_transport_wakeup_data *cdata)
{
		cdata->cmd = cmd;
		cdata->transaction_delay = hdcp->timeout_left;
		cdata->buf = hdcp->app_data.request.data + 1;
}

static void sde_hdcp_2x_msg_sent(struct sde_hdcp_2x_ctrl *hdcp)
{
	struct hdcp_transport_wakeup_data cdata = {
		HDCP_TRANSPORT_CMD_INVALID,
		hdcp->client_data};

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, hdcp->authenticated);

	if (atomic_read(&hdcp->hdcp_off)) {
		pr_debug("invalid state, hdcp off\n");
		goto exit;
	}

	SDE_EVT32_EXTERNAL(hdcp->app_data.response.data[0]);
	switch (hdcp->app_data.response.data[0]) {
	case SKE_SEND_TYPE_ID:
		if (!hdcp2_app_comm(hdcp->hdcp2_ctx,
				HDCP2_CMD_EN_ENCRYPTION, &hdcp->app_data)) {
			hdcp->authenticated = true;

			if (hdcp->force_encryption)
				hdcp2_force_encryption(hdcp->hdcp2_ctx, 1);

			cdata.cmd = HDCP_TRANSPORT_CMD_STATUS_SUCCESS;
			sde_hdcp_2x_wakeup_client(hdcp, &cdata);
		}

		/* poll for link check */
		sde_hdcp_2x_initialize_command(hdcp,
				HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		break;
	case SKE_SEND_EKS:
		if (hdcp->repeater_flag && !atomic_read(&hdcp->hdcp_off)) {
			/* poll for link check */
			sde_hdcp_2x_initialize_command(hdcp,
					HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		} else {
			hdcp->app_data.response.data[0] = SKE_SEND_TYPE_ID;
			hdcp->app_data.response.data[1] =
				sde_hdcp_2x_stream_type(hdcp->min_enc_level);
			hdcp->app_data.response.length = 1;
			hdcp->app_data.timeout = 100;

			sde_hdcp_2x_send_message(hdcp);
		}
		break;
	case REP_SEND_ACK:
		pr_debug("Repeater authentication successful. update_stream=%d\n",
				hdcp->update_stream);

		if (hdcp->update_stream) {
			sde_hdcp_2x_query_stream(hdcp);
			hdcp->update_stream = false;
		} else {
			sde_hdcp_2x_initialize_command(hdcp,
					HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		}
		break;
	default:
		cdata.cmd = HDCP_TRANSPORT_CMD_RECV_MESSAGE;
		cdata.transaction_delay = hdcp->app_data.timeout;
		cdata.buf = hdcp->app_data.request.data + 1;
	}

	sde_hdcp_2x_wakeup_client(hdcp, &cdata);

exit:
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, hdcp->authenticated);
}

static void sde_hdcp_2x_init(struct sde_hdcp_2x_ctrl *hdcp)
{
	int rc;
	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_START, &hdcp->app_data);
	if (rc)
		sde_hdcp_2x_clean(hdcp);
}

static void sde_hdcp_2x_start_auth(struct sde_hdcp_2x_ctrl *hdcp)
{
	int rc;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, hdcp->authenticated);
	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_START_AUTH,
		&hdcp->app_data);
	if (rc) {
		sde_hdcp_2x_clean(hdcp);
		return;
	}

	pr_debug("message received from TZ: %s\n",
		 sde_hdcp_2x_message_name(hdcp->app_data.response.data[0]));

	sde_hdcp_2x_send_message(hdcp);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, hdcp->authenticated);
}

static void sde_hdcp_2x_timeout(struct sde_hdcp_2x_ctrl *hdcp)
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
		sde_hdcp_2x_send_message(hdcp);
	return;
error:
	if (!atomic_read(&hdcp->hdcp_off))
		sde_hdcp_2x_clean(hdcp);
}

static void sde_hdcp_2x_msg_recvd(struct sde_hdcp_2x_ctrl *hdcp)
{
	int rc = 0;
	char *msg = NULL;
	u32 message_id_bytes = 0;
	u32 request_length, out_msg;
	struct hdcp_transport_wakeup_data cdata = {HDCP_TRANSPORT_CMD_INVALID};

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, hdcp->authenticated);
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

	if (hdcp->device_type == HDCP_TXMTR_DP ||
			hdcp->device_type == HDCP_TXMTR_DP_MST) {
		msg[0] = hdcp->last_msg;
		message_id_bytes = 1;
	}

	request_length += message_id_bytes;

	pr_debug("[sink]: %s\n", sde_hdcp_2x_message_name(msg[0]));

	hdcp->app_data.request.length = request_length;
	rc = hdcp2_app_comm(hdcp->hdcp2_ctx, HDCP2_CMD_PROCESS_MSG,
			&hdcp->app_data);
	if (rc) {
		pr_err("failed to process sink's response to %s (%d)\n",
				sde_hdcp_2x_message_name(msg[0]), rc);
		rc = -EINVAL;
		goto exit;
	}

	if (msg[0] == AKE_SEND_H_PRIME && hdcp->no_stored_km) {
		cdata.cmd = HDCP_TRANSPORT_CMD_RECV_MESSAGE;
		cdata.transaction_delay = hdcp->app_data.timeout;
		cdata.buf = hdcp->app_data.request.data + 1;
		goto exit;
	}

	if (hdcp->app_data.response.length == 0)
		out_msg = INVALID_MESSAGE;
	else
		out_msg = (u32)hdcp->app_data.response.data[0];

	pr_debug("[tz]: %s\n", sde_hdcp_2x_message_name(out_msg));

	if (msg[0] == REP_STREAM_READY && out_msg != REP_STREAM_MANAGE) {
		if (hdcp->resend_stream_manage) {
			pr_debug("resend stream management\n");
		} else if (!hdcp->authenticated) {
			rc = hdcp2_app_comm(hdcp->hdcp2_ctx,
					HDCP2_CMD_EN_ENCRYPTION,
					&hdcp->app_data);
			if (!rc) {
				hdcp->authenticated = true;

				if (hdcp->force_encryption)
					hdcp2_force_encryption(
							hdcp->hdcp2_ctx, 1);

				cdata.cmd = HDCP_TRANSPORT_CMD_STATUS_SUCCESS;
				sde_hdcp_2x_wakeup_client(hdcp, &cdata);
			} else {
				pr_err("failed to enable encryption (%d)\n",
						rc);
			}
		}

		sde_hdcp_2x_initialize_command(hdcp,
				HDCP_TRANSPORT_CMD_LINK_POLL, &cdata);
		goto exit;
	}

	hdcp->resend_lc_init = false;
	if (msg[0] == LC_SEND_L_PRIME && out_msg == LC_INIT)
		hdcp->resend_lc_init = true;

	if (msg[0] == REP_STREAM_READY && out_msg == REP_STREAM_MANAGE)
		pr_debug("resend %s\n", sde_hdcp_2x_message_name(out_msg));

	if (out_msg == AKE_NO_STORED_KM)
		hdcp->no_stored_km = true;
	else
		hdcp->no_stored_km = false;

	if (out_msg == SKE_SEND_EKS) {
		hdcp->repeater_flag = hdcp->app_data.repeater_flag;
		hdcp->update_stream = true;
	}

	if (!atomic_read(&hdcp->hdcp_off)) {
		cdata.cmd = HDCP_TRANSPORT_CMD_SEND_MESSAGE;
		cdata.buf = hdcp->app_data.response.data + 1;
		cdata.buf_len = hdcp->app_data.response.length;
		cdata.transaction_delay = hdcp->app_data.timeout;
	}
exit:
	sde_hdcp_2x_wakeup_client(hdcp, &cdata);

	if (rc && !atomic_read(&hdcp->hdcp_off))
		sde_hdcp_2x_clean(hdcp);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, hdcp->authenticated);
}

static struct list_head *sde_hdcp_2x_stream_present(
		struct sde_hdcp_2x_ctrl *hdcp, u8 stream_id, u8 virtual_channel)
{
	struct sde_hdcp_stream *stream_entry;
	struct list_head *entry;
	bool present = false;

	list_for_each(entry, &hdcp->stream_handles) {
		stream_entry = list_entry(entry,
			struct sde_hdcp_stream, list);
		if (stream_entry->virtual_channel == virtual_channel &&
				stream_entry->stream_id == stream_id) {
			present = true;
			break;
		}
	}

	if (!present)
		entry = NULL;
	return entry;
}


static void sde_hdcp_2x_manage_stream(struct sde_hdcp_2x_ctrl *hdcp)
{
	struct list_head *entry;
	struct list_head *element;
	struct sde_hdcp_stream *stream_entry;
	bool query_streams = false;

	entry = hdcp->stream_handles.next;
	while (entry != &hdcp->stream_handles) {
		stream_entry = list_entry(entry, struct sde_hdcp_stream, list);
		element = entry;
		entry = entry->next;

		if (!stream_entry->active) {
			hdcp2_close_stream(hdcp->hdcp2_ctx,
				stream_entry->stream_handle);
			hdcp->stream_count--;
			list_del(element);
			kfree(stream_entry);
			query_streams = true;
		} else if (!stream_entry->stream_handle) {
			if (hdcp2_open_stream(hdcp->hdcp2_ctx,
					stream_entry->virtual_channel,
					stream_entry->stream_id,
					&stream_entry->stream_handle))
				pr_err("Unable to open stream %d, virtual channel %d\n",
					stream_entry->stream_id,
					stream_entry->virtual_channel);
			else
				query_streams = true;
		}
	}

	if (query_streams) {
		if (hdcp->authenticated) {
			sde_hdcp_2x_query_stream(hdcp);
		} else if (hdcp->last_msg == REP_STREAM_MANAGE ||
				hdcp->last_msg == REP_STREAM_READY) {
			hdcp->resend_stream_manage = true;
		}
	}
}


static bool sde_hdcp_2x_remove_streams(struct sde_hdcp_2x_ctrl *hdcp,
		struct stream_info *streams, u8 num_streams)
{
	u8 i;
	u8 stream_id;
	u8 virtual_channel;
	struct list_head *entry;
	struct sde_hdcp_stream *stream_entry;
	bool changed = false;

	for (i = 0 ; i < num_streams; i++) {
		stream_id = streams[i].stream_id;
		virtual_channel = streams[i].virtual_channel;
		entry = sde_hdcp_2x_stream_present(hdcp, stream_id,
			virtual_channel);
		if (!entry)
			continue;

		stream_entry = list_entry(entry, struct sde_hdcp_stream,
			list);

		if (!stream_entry->stream_handle) {
			/* Stream wasn't fully initialized so remove it */
			hdcp->stream_count--;
			list_del(entry);
			kfree(stream_entry);
		} else {
			stream_entry->active = false;
		}
		changed = true;
	}

	return changed;
}

static bool sde_hdcp_2x_add_streams(struct sde_hdcp_2x_ctrl *hdcp,
		struct stream_info *streams, u8 num_streams)
{
	u8 i;
	u8 stream_id;
	u8 virtual_channel;
	struct sde_hdcp_stream *stream;
	bool changed = false;

	for (i = 0 ; i < num_streams; i++) {
		stream_id = streams[i].stream_id;
		virtual_channel = streams[i].virtual_channel;

		if (sde_hdcp_2x_stream_present(hdcp, stream_id,
				virtual_channel))
			continue;

		stream = kzalloc(sizeof(struct sde_hdcp_stream), GFP_KERNEL);
		if (!stream)
			continue;

		INIT_LIST_HEAD(&stream->list);
		stream->stream_handle = 0;
		stream->stream_id = stream_id;
		stream->virtual_channel = virtual_channel;
		stream->active = true;

		list_add(&stream->list, &hdcp->stream_handles);
		hdcp->stream_count++;
		changed = true;
	}

	return changed;
}


/** sde_hdcp_2x_wakeup() - wakeup the module to execute a requested command
 * @data: data required for executing corresponding command.
 *
 * This function is executed on caller's thread. Update the local data
 * and wakeup the local thread to execute the command. Once the local
 * thread is activated, caller's thread is returned and this function
 * is ready to receive next command.
 */
static int sde_hdcp_2x_wakeup(struct sde_hdcp_2x_wakeup_data *data)
{
	struct sde_hdcp_2x_ctrl *hdcp;
	int rc = 0;

	if (!data)
		return -EINVAL;

	hdcp = data->context;
	if (!hdcp)
		return -EINVAL;

	hdcp->timeout_left = data->timeout;
	hdcp->total_message_length = data->total_message_length;

	if (!completion_done(&hdcp->response_completion))
		complete_all(&hdcp->response_completion);

	switch (data->cmd) {
	case HDCP_2X_CMD_ENABLE:
		if (!atomic_cmpxchg(&hdcp->enable_pending, 0, 1)) {
			hdcp->device_type = data->device_type;
			kfifo_put(&hdcp->cmd_q, data->cmd);
			kthread_unpark(hdcp->thread);
			wake_up(&hdcp->wait_q);
		}
		break;
	case HDCP_2X_CMD_DISABLE:
		if (!atomic_xchg(&hdcp->hdcp_off, 1))
			kfifo_put(&hdcp->cmd_q, HDCP_2X_CMD_STOP);
		kfifo_put(&hdcp->cmd_q, data->cmd);
		kthread_park(hdcp->thread);
		break;
	case HDCP_2X_CMD_STOP:
		atomic_set(&hdcp->hdcp_off, 1);

		kfifo_put(&hdcp->cmd_q, data->cmd);
		kthread_park(hdcp->thread);
		break;
	case HDCP_2X_CMD_START:
		hdcp->no_stored_km = false;
		hdcp->repeater_flag = false;
		hdcp->update_stream = false;
		hdcp->authenticated = false;
		hdcp->last_msg = INVALID_MESSAGE;
		hdcp->timeout_left = 0;
		atomic_set(&hdcp->hdcp_off, 0);

		kfifo_put(&hdcp->cmd_q, data->cmd);
		kthread_unpark(hdcp->thread);
		wake_up(&hdcp->wait_q);
		break;
	case HDCP_2X_CMD_OPEN_STREAMS:
		if (sde_hdcp_2x_add_streams(hdcp, data->streams,
				data->num_streams)) {
			kfifo_put(&hdcp->cmd_q, data->cmd);
			wake_up(&hdcp->wait_q);
		}
		break;
	case HDCP_2X_CMD_CLOSE_STREAMS:
		if (sde_hdcp_2x_remove_streams(hdcp, data->streams,
				data->num_streams)) {
			kfifo_put(&hdcp->cmd_q, data->cmd);
			wake_up(&hdcp->wait_q);
		}
		break;
	case HDCP_2X_CMD_MIN_ENC_LEVEL:
		hdcp->min_enc_level = data->min_enc_level;
		if (hdcp->authenticated) {
			kfifo_put(&hdcp->cmd_q, data->cmd);
			wake_up(&hdcp->wait_q);
		}
		break;
	default:
		kfifo_put(&hdcp->cmd_q, data->cmd);
		wake_up(&hdcp->wait_q);
		break;
	}

	return rc;
}

static void sde_hdcp_2x_enable(struct sde_hdcp_2x_ctrl *hdcp)
{
	if (!hdcp)
		return;

	if (hdcp->hdcp2_ctx) {
		pr_debug("HDCP library context already acquired\n");
		return;
	}

	hdcp->hdcp2_ctx = hdcp2_init(hdcp->device_type);
	if (!hdcp->hdcp2_ctx)
		pr_err("Unable to acquire HDCP library handle\n");
}

static void sde_hdcp_2x_disable(struct sde_hdcp_2x_ctrl *hdcp)
{
	if (!hdcp->hdcp2_ctx)
		return;

	hdcp2_deinit(hdcp->hdcp2_ctx);
	hdcp->hdcp2_ctx = NULL;
}

static int sde_hdcp_2x_main(void *data)
{
	struct sde_hdcp_2x_ctrl *hdcp = data;
	enum sde_hdcp_2x_wakeup_cmd cmd;

	while (1) {
		wait_event(hdcp->wait_q,
			!kfifo_is_empty(&hdcp->cmd_q) ||
			kthread_should_stop() ||
			kthread_should_park());

		if (kthread_should_stop())
			break;

		if (kfifo_is_empty(&hdcp->cmd_q) && kthread_should_park()) {
			kthread_parkme();
			continue;
		}

		if (!kfifo_get(&hdcp->cmd_q, &cmd))
			continue;

		switch (cmd) {
		case HDCP_2X_CMD_ENABLE:
			sde_hdcp_2x_enable(hdcp);
			atomic_set(&hdcp->enable_pending, 0);
			break;
		case HDCP_2X_CMD_DISABLE:
			sde_hdcp_2x_disable(hdcp);
			break;
		case HDCP_2X_CMD_START:
			sde_hdcp_2x_init(hdcp);
			break;
		case HDCP_2X_CMD_STOP:
			sde_hdcp_2x_clean(hdcp);
			break;
		case HDCP_2X_CMD_START_AUTH:
			sde_hdcp_2x_start_auth(hdcp);
			break;
		case HDCP_2X_CMD_MSG_SEND_SUCCESS:
			sde_hdcp_2x_msg_sent(hdcp);
			break;
		case HDCP_2X_CMD_MSG_SEND_FAILED:
		case HDCP_2X_CMD_MSG_RECV_FAILED:
		case HDCP_2X_CMD_LINK_FAILED:
			sde_hdcp_2x_clean(hdcp);
			break;
		case HDCP_2X_CMD_MSG_RECV_SUCCESS:
			sde_hdcp_2x_msg_recvd(hdcp);
			break;
		case HDCP_2X_CMD_MSG_RECV_TIMEOUT:
			sde_hdcp_2x_timeout(hdcp);
			break;
		case HDCP_2X_CMD_QUERY_STREAM_TYPE:
			sde_hdcp_2x_query_stream(hdcp);
			break;
		case HDCP_2X_CMD_MIN_ENC_LEVEL:
			if (!hdcp->repeater_flag) {
				sde_hdcp_2x_send_type(hdcp);
				break;
			}
			sde_hdcp_2x_query_stream(hdcp);
			break;
		case HDCP_2X_CMD_OPEN_STREAMS:
		case HDCP_2X_CMD_CLOSE_STREAMS:
			sde_hdcp_2x_manage_stream(hdcp);
			break;
		default:
			break;
		}
	}

	return 0;
}

int sde_hdcp_2x_register(struct sde_hdcp_2x_register_data *data)
{
	int rc = 0;
	struct sde_hdcp_2x_ctrl *hdcp = NULL;

	if (!data) {
		pr_err("invalid input\n");
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
	data->ops->feature_supported = sde_hdcp_2x_client_feature_supported;
	data->ops->wakeup = sde_hdcp_2x_wakeup;
	data->ops->force_encryption = sde_hdcp_2x_force_encryption;

	hdcp = kzalloc(sizeof(*hdcp), GFP_KERNEL);
	if (!hdcp) {
		rc = -ENOMEM;
		goto unlock;
	}

	INIT_LIST_HEAD(&hdcp->stream_handles);
	hdcp->client_data = data->client_data;
	hdcp->client_ops = data->client_ops;

	INIT_KFIFO(hdcp->cmd_q);

	init_waitqueue_head(&hdcp->wait_q);
	atomic_set(&hdcp->hdcp_off, 1);
	atomic_set(&hdcp->enable_pending, 0);

	init_completion(&hdcp->response_completion);

	*data->hdcp_data = hdcp;

	hdcp->thread = kthread_run(sde_hdcp_2x_main, hdcp, "hdcp_2x");

	if (IS_ERR(hdcp->thread)) {
		pr_err("unable to start lib thread\n");
		rc = PTR_ERR(hdcp->thread);
		hdcp->thread = NULL;
		goto error;
	}

	hdcp->force_encryption = false;

	return 0;
error:
	kfree(hdcp);
	hdcp = NULL;
unlock:
	return rc;
}

void sde_hdcp_2x_deregister(void *data)
{
	struct sde_hdcp_2x_ctrl *hdcp = data;

	if (!hdcp)
		return;

	kthread_stop(hdcp->thread);
	sde_hdcp_2x_disable(data);
	kfree(hdcp);
}
