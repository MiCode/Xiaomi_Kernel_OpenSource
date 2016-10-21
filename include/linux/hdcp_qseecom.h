/* Copyright (c) 2015, 2016, The Linux Foundation. All rights reserved.
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

#define HDCP_MAX_MESSAGE_PARTS 4

enum hdcp_lib_wakeup_cmd {
	HDCP_LIB_WKUP_CMD_INVALID,
	HDCP_LIB_WKUP_CMD_START,
	HDCP_LIB_WKUP_CMD_STOP,
	HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS,
	HDCP_LIB_WKUP_CMD_MSG_SEND_FAILED,
	HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS,
	HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED,
	HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT,
	HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE,
	HDCP_LIB_WKUP_CMD_LINK_FAILED,
};

enum hdmi_hdcp_wakeup_cmd {
	HDMI_HDCP_WKUP_CMD_INVALID,
	HDMI_HDCP_WKUP_CMD_SEND_MESSAGE,
	HDMI_HDCP_WKUP_CMD_RECV_MESSAGE,
	HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS,
	HDMI_HDCP_WKUP_CMD_STATUS_FAILED,
	HDMI_HDCP_WKUP_CMD_LINK_POLL,
	HDMI_HDCP_WKUP_CMD_AUTHENTICATE
};

struct hdcp_lib_wakeup_data {
	enum hdcp_lib_wakeup_cmd cmd;
	void *context;
	char *recvd_msg_buf;
	uint32_t recvd_msg_len;
	uint32_t timeout;
};

struct hdcp_msg_part {
	uint32_t offset;
	uint32_t length;
};

struct hdcp_msg_data {
	uint32_t num_messages;
	struct hdcp_msg_part messages[HDCP_MAX_MESSAGE_PARTS];
	uint8_t rx_status;
};

struct hdmi_hdcp_wakeup_data {
	enum hdmi_hdcp_wakeup_cmd cmd;
	void *context;
	char *send_msg_buf;
	uint32_t send_msg_len;
	uint32_t timeout;
	uint8_t abort_mask;
	const struct hdcp_msg_data *message_data;
};

static inline char *hdmi_hdcp_cmd_to_str(uint32_t cmd)
{
	switch (cmd) {
	case HDMI_HDCP_WKUP_CMD_SEND_MESSAGE:
		return "HDMI_HDCP_WKUP_CMD_SEND_MESSAGE";
	case HDMI_HDCP_WKUP_CMD_RECV_MESSAGE:
		return "HDMI_HDCP_WKUP_CMD_RECV_MESSAGE";
	case HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS:
		return "HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS";
	case HDMI_HDCP_WKUP_CMD_STATUS_FAILED:
		return "HDMI_HDCP_WKUP_CMD_STATUS_FAIL";
	case HDMI_HDCP_WKUP_CMD_LINK_POLL:
		return "HDMI_HDCP_WKUP_CMD_LINK_POLL";
	case HDMI_HDCP_WKUP_CMD_AUTHENTICATE:
		return "HDMI_HDCP_WKUP_CMD_AUTHENTICATE";
	default:
		return "???";
	}
}

static inline char *hdcp_lib_cmd_to_str(uint32_t cmd)
{
	switch (cmd) {
	case HDCP_LIB_WKUP_CMD_START:
		return "HDCP_LIB_WKUP_CMD_START";
	case HDCP_LIB_WKUP_CMD_STOP:
		return "HDCP_LIB_WKUP_CMD_STOP";
	case HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS:
		return "HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS";
	case HDCP_LIB_WKUP_CMD_MSG_SEND_FAILED:
		return "HDCP_LIB_WKUP_CMD_MSG_SEND_FAILED";
	case HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS:
		return "HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS";
	case HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED:
		return "HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED";
	case HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT:
		return "HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT";
	case HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE:
		return "HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE";
	case HDCP_LIB_WKUP_CMD_LINK_FAILED:
		return "HDCP_LIB_WKUP_CMD_LINK_FAILED";
	default:
		return "???";
	}
}

struct hdcp_txmtr_ops {
	int (*wakeup)(struct hdcp_lib_wakeup_data *data);
	bool (*feature_supported)(void *phdcpcontext);
	void (*update_exec_type)(void *ctx, bool tethered);
	int (*hdcp_txmtr_get_state)(void *phdcpcontext,
		uint32_t *state);
};

struct hdcp_client_ops {
	int (*wakeup)(struct hdmi_hdcp_wakeup_data *data);
};

enum hdcp_device_type {
	HDCP_TXMTR_HDMI = 0x8001,
	HDCP_TXMTR_DP = 0x8002
};

struct hdcp_register_data {
	struct hdcp_client_ops *client_ops;
	struct hdcp_txmtr_ops *txmtr_ops;
	enum hdcp_device_type device_type;
	void *client_ctx;
	void **hdcp_ctx;
	bool tethered;
};

int hdcp_library_register(struct hdcp_register_data *data);
void hdcp_library_deregister(void *phdcpcontext);
bool hdcp1_check_if_supported_load_app(void);
int hdcp1_set_keys(uint32_t *aksv_msb, uint32_t *aksv_lsb);
int hdcp1_set_enc(bool enable);

#endif /* __HDCP_QSEECOM_H */
