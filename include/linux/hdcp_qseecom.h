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

#ifndef __HDCP_QSEECOM_H
#define __HDCP_QSEECOM_H
#include <linux/types.h>

enum hdcp_lib_wakeup_cmd {
	HDCP_WKUP_CMD_START,
	HDCP_WKUP_CMD_STOP,
	HDCP_WKUP_CMD_MSG_SEND_SUCCESS,
	HDCP_WKUP_CMD_MSG_SEND_FAILED,
	HDCP_WKUP_CMD_MSG_RECV_SUCCESS,
	HDCP_WKUP_CMD_MSG_RECV_FAILED,
	HDCP_WKUP_CMD_MSG_RECV_TIMEOUT,
};

enum hdmi_hdcp_cmd {
	HDMI_HDCP_SEND_MESSAGE,
	HDMI_HDCP_RECV_MESSAGE,
	HDMI_HDCP_STATUS_SUCCESS,
	HDMI_HDCP_STATUS_FAIL
};

static inline char *hdmi_hdcp_cmd_to_str(uint32_t cmd)
{
	switch (cmd) {
	case HDMI_HDCP_SEND_MESSAGE:
		return "HDMI_HDCP_SEND_MESSAGE";
	case HDMI_HDCP_RECV_MESSAGE:
		return "HDMI_HDCP_RECV_MESSAGE";
	case HDMI_HDCP_STATUS_SUCCESS:
		return "HDMI_HDCP_STATUS_SUCCESS";
	case HDMI_HDCP_STATUS_FAIL:
		return "HDMI_HDCP_STATUS_FAIL";
	default:
		return "???";
	}
}

static inline char *hdcp_cmd_to_str(uint32_t cmd)
{
	switch (cmd) {
	case HDCP_WKUP_CMD_START:
		return "HDCP_WKUP_CMD_START";
	case HDCP_WKUP_CMD_STOP:
		return "HDCP_WKUP_CMD_STOP";
	case HDCP_WKUP_CMD_MSG_SEND_SUCCESS:
		return "HDCP_WKUP_CMD_MSG_SEND_SUCCESS";
	case HDCP_WKUP_CMD_MSG_SEND_FAILED:
		return "HDCP_WKUP_CMD_MSG_SEND_FAILED";
	case HDCP_WKUP_CMD_MSG_RECV_SUCCESS:
		return "HDCP_WKUP_CMD_MSG_RECV_SUCCESS";
	case HDCP_WKUP_CMD_MSG_RECV_FAILED:
		return "HDCP_WKUP_CMD_MSG_RECV_FAILED";
	case HDCP_WKUP_CMD_MSG_RECV_TIMEOUT:
		return "HDCP_WKUP_CMD_MSG_RECV_TIMEOUT";
	default:
		return "???";
	}
}

struct hdcp_txmtr_ops {
	int (*wakeup)(void *phdcpcontext, enum hdcp_lib_wakeup_cmd cmd,
		char *msg, uint32_t len);
	bool (*feature_supported)(void *phdcpcontext);

	int (*hdcp_txmtr_get_state)(void *phdcpcontext,
		uint32_t *state);
	int (*hdcp_query_stream_type)(void *phdcpcontext);
};

struct hdcp_client_ops {
	int (*wakeup)(void *client_ctx, enum hdmi_hdcp_cmd cmd,
		char *msg, uint32_t msglen, uint32_t timeout);
};

int hdcp_library_register(void **pphdcpcontext,
	struct hdcp_client_ops *client_ops,
	struct hdcp_txmtr_ops *txmtr_ops, void *client_ctx);
void hdcp_library_deregister(void *phdcpcontext);
int hdcp1_set_keys(uint32_t *aksv_msb, uint32_t *aksv_lsb);

#endif /* __HDCP_QSEECOM_H */
