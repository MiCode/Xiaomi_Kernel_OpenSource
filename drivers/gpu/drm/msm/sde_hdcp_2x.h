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

#ifndef __SDE_HDCP_2X_H__
#define __SDE_HDCP_2X_H__

#include "sde_hdcp.h"

#define TO_STR(x) #x

#define HDCP_MAX_MESSAGE_PARTS 4

/**
 * enum sde_hdcp_2x_wakeup_cmd - commands for interacting with HDCP driver
 * @HDCP_2X_CMD_INVALID:           initialization value
 * @HDCP_2X_CMD_START:             start authentication
 * @HDCP_2X_CMD_STOP:              stop authentication
 * @HDCP_2X_CMD_MSG_SEND_SUCCESS:  sending message to sink succeeded
 * @HDCP_2X_CMD_MSG_SEND_FAILED:   sending message to sink failed
 * @HDCP_2X_CMD_MSG_SEND_TIMEOUT:  sending message to sink timed out
 * @HDCP_2X_CMD_MSG_RECV_SUCCESS:  receiving message from sink succeeded
 * @HDCP_2X_CMD_MSG_RECV_FAILED:   receiving message from sink failed
 * @HDCP_2X_CMD_MSG_RECV_TIMEOUT:  receiving message from sink timed out
 * @HDCP_2X_CMD_QUERY_STREAM_TYPE: start content stream processing
 * @HDCP_2X_CMD_LINK_FAILED:       link failure notification
 * @HDCP_2X_CMD_MIN_ENC_LEVEL:     trigger minimum encryption level change
 */
enum sde_hdcp_2x_wakeup_cmd {
	HDCP_2X_CMD_INVALID,
	HDCP_2X_CMD_START,
	HDCP_2X_CMD_STOP,
	HDCP_2X_CMD_MSG_SEND_SUCCESS,
	HDCP_2X_CMD_MSG_SEND_FAILED,
	HDCP_2X_CMD_MSG_SEND_TIMEOUT,
	HDCP_2X_CMD_MSG_RECV_SUCCESS,
	HDCP_2X_CMD_MSG_RECV_FAILED,
	HDCP_2X_CMD_MSG_RECV_TIMEOUT,
	HDCP_2X_CMD_QUERY_STREAM_TYPE,
	HDCP_2X_CMD_LINK_FAILED,
	HDCP_2X_CMD_MIN_ENC_LEVEL,
};

/**
 * enum hdcp_transport_wakeup_cmd - commands to instruct display transport layer
 * @HDCP_TRANSPORT_CMD_INVALID:        initialization value
 * @HDCP_TRANSPORT_CMD_SEND_MESSAGE:   send message to sink
 * @HDCP_TRANSPORT_CMD_RECV_MESSAGE:   receive message from sink
 * @HDCP_TRANSPORT_CMD_STATUS_SUCCESS: successfully communicated with TrustZone
 * @HDCP_TRANSPORT_CMD_STATUS_FAILED:  failed to communicate with TrustZone
 * @HDCP_TRANSPORT_CMD_LINK_POLL:      poll the HDCP link
 * @HDCP_TRANSPORT_CMD_AUTHENTICATE:   start authentication
 */
enum hdcp_transport_wakeup_cmd {
	HDCP_TRANSPORT_CMD_INVALID,
	HDCP_TRANSPORT_CMD_SEND_MESSAGE,
	HDCP_TRANSPORT_CMD_RECV_MESSAGE,
	HDCP_TRANSPORT_CMD_STATUS_SUCCESS,
	HDCP_TRANSPORT_CMD_STATUS_FAILED,
	HDCP_TRANSPORT_CMD_LINK_POLL,
	HDCP_TRANSPORT_CMD_AUTHENTICATE
};

enum sde_hdcp_2x_device_type {
	HDCP_TXMTR_HDMI = 0x8001,
	HDCP_TXMTR_DP = 0x8002
};

/**
 * struct sde_hdcp_2x_lib_wakeup_data - command and data send to HDCP driver
 * @cmd:       command type
 * @context:   void pointer to the HDCP driver instance
 * @buf:       message received from the sink
 * @buf_len:   length of message received from the sink
 * @timeout:   time out value for timed transactions
 */
struct sde_hdcp_2x_wakeup_data {
	enum sde_hdcp_2x_wakeup_cmd cmd;
	void *context;
	uint32_t total_message_length;
	uint32_t timeout;
	u8 min_enc_level;
};

/**
 * struct sde_hdcp_2x_msg_part - a single part of an HDCP 2.2 message
 * @name:       user readable message name
 * @offset:     message part offset
 * @length      message part length
 */
struct sde_hdcp_2x_msg_part {
	char *name;
	uint32_t offset;
	uint32_t length;
};

/**
 * struct sde_hdcp_2x_msg_data - HDCP 2.2 message containing one or more parts
 * @num_messages:   total number of parts in a full message
 * @messages:       array containing num_messages parts
 * @rx_status:      value of rx_status register
 */
struct sde_hdcp_2x_msg_data {
	uint32_t num_messages;
	struct sde_hdcp_2x_msg_part messages[HDCP_MAX_MESSAGE_PARTS];
	uint8_t rx_status;
};

/**
 * struct hdcp_transport_wakeup_data - data sent to display transport layer
 * @cmd:            command type
 * @context:        void pointer to the display transport layer
 * @send_msg_buf:   buffer containing message to be sent to sink
 * @send_msg_len:   length of the message to be sent to sink
 * @timeout:        timeout value for timed transactions
 * @abort_mask:     mask used to determine whether HDCP link is valid
 * @message_data:   a pointer to the message description
 */
struct hdcp_transport_wakeup_data {
	enum hdcp_transport_wakeup_cmd cmd;
	void *context;
	unsigned char *buf;
	u32 buf_len;
	u32 timeout;
	u8 abort_mask;
	const struct sde_hdcp_2x_msg_data *message_data;
};

static inline const char *sde_hdcp_2x_cmd_to_str(
		enum sde_hdcp_2x_wakeup_cmd cmd)
{
	switch (cmd) {
	case HDCP_2X_CMD_START:
		return TO_STR(HDCP_2X_CMD_START);
	case HDCP_2X_CMD_STOP:
		return TO_STR(HDCP_2X_CMD_STOP);
	case HDCP_2X_CMD_MSG_SEND_SUCCESS:
		return TO_STR(HDCP_2X_CMD_MSG_SEND_SUCCESS);
	case HDCP_2X_CMD_MSG_SEND_FAILED:
		return TO_STR(HDCP_2X_CMD_MSG_SEND_FAILED);
	case HDCP_2X_CMD_MSG_SEND_TIMEOUT:
		return TO_STR(HDCP_2X_CMD_MSG_SEND_TIMEOUT);
	case HDCP_2X_CMD_MSG_RECV_SUCCESS:
		return TO_STR(HDCP_2X_CMD_MSG_RECV_SUCCESS);
	case HDCP_2X_CMD_MSG_RECV_FAILED:
		return TO_STR(HDCP_2X_CMD_MSG_RECV_FAILED);
	case HDCP_2X_CMD_MSG_RECV_TIMEOUT:
		return TO_STR(HDCP_2X_CMD_MSG_RECV_TIMEOUT);
	case HDCP_2X_CMD_QUERY_STREAM_TYPE:
		return TO_STR(HDCP_2X_CMD_QUERY_STREAM_TYPE);
	default:
		return "UNKNOWN";
	}
}

static inline const char *hdcp_transport_cmd_to_str(
		enum hdcp_transport_wakeup_cmd cmd)
{
	switch (cmd) {
	case HDCP_TRANSPORT_CMD_SEND_MESSAGE:
		return TO_STR(HDCP_TRANSPORT_CMD_SEND_MESSAGE);
	case HDCP_TRANSPORT_CMD_RECV_MESSAGE:
		return TO_STR(HDCP_TRANSPORT_CMD_RECV_MESSAGE);
	case HDCP_TRANSPORT_CMD_STATUS_SUCCESS:
		return TO_STR(HDCP_TRANSPORT_CMD_STATUS_SUCCESS);
	case HDCP_TRANSPORT_CMD_STATUS_FAILED:
		return TO_STR(HDCP_TRANSPORT_CMD_STATUS_FAILED);
	case HDCP_TRANSPORT_CMD_LINK_POLL:
		return TO_STR(HDCP_TRANSPORT_CMD_LINK_POLL);
	case HDCP_TRANSPORT_CMD_AUTHENTICATE:
		return TO_STR(HDCP_TRANSPORT_CMD_AUTHENTICATE);
	default:
		return "UNKNOWN";
	}
}

struct sde_hdcp_2x_ops {
	int (*wakeup)(struct sde_hdcp_2x_wakeup_data *data);
	bool (*feature_supported)(void *data);
	void (*force_encryption)(void *data, bool enable);
};

struct hdcp_transport_ops {
	int (*wakeup)(struct hdcp_transport_wakeup_data *data);
};

struct sde_hdcp_2x_register_data {
	struct hdcp_transport_ops *client_ops;
	struct sde_hdcp_2x_ops *ops;
	enum sde_hdcp_2x_device_type device_type;
	void *client_data;
	void **hdcp_data;
};

/* functions for the HDCP 2.2 state machine module */
int sde_hdcp_2x_register(struct sde_hdcp_2x_register_data *data);
void sde_hdcp_2x_deregister(void *data);
#endif
