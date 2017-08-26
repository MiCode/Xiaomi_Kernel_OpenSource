/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

/**
 * enum hdcp_lib_wakeup_cmd - commands for interacting with HDCP driver
 * @HDCP_LIB_WKUP_CMD_INVALID:           initialization value
 * @HDCP_LIB_WKUP_CMD_START:             start authentication
 * @HDCP_LIB_WKUP_CMD_STOP:              stop authentication
 * @HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS:  sending message to sink succeeded
 * @HDCP_LIB_WKUP_CMD_MSG_SEND_FAILED:   sending message to sink failed
 * @HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS:  receiving message from sink succeeded
 * @HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED:   receiving message from sink failed
 * @HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT:  receiving message from sink timed out
 * @HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE: start content stream processing
 * @HDCP_LIB_WKUP_CMD_LINK_FAILED:       link failure notification
 */
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

/**
 * enum hdcp_wakeup_cmd - commands for interacting with display transport layer
 * @HDCP_WKUP_CMD_INVALID:        initialization value
 * @HDCP_WKUP_CMD_SEND_MESSAGE:   send message to sink
 * @HDCP_WKUP_CMD_RECV_MESSAGE:   receive message from sink
 * @HDCP_WKUP_CMD_STATUS_SUCCESS: successfully communicated with TrustZone
 * @HDCP_WKUP_CMD_STATUS_FAILED:  failed to communicate with TrustZone
 * @HDCP_WKUP_CMD_LINK_POLL:      poll the HDCP link
 * @HDCP_WKUP_CMD_AUTHENTICATE:   start authentication
 */
enum hdcp_wakeup_cmd {
	HDCP_WKUP_CMD_INVALID,
	HDCP_WKUP_CMD_SEND_MESSAGE,
	HDCP_WKUP_CMD_RECV_MESSAGE,
	HDCP_WKUP_CMD_STATUS_SUCCESS,
	HDCP_WKUP_CMD_STATUS_FAILED,
	HDCP_WKUP_CMD_LINK_POLL,
	HDCP_WKUP_CMD_AUTHENTICATE
};

/**
 * struct hdcp_lib_wakeup_data - command and data send to HDCP driver
 * @cmd:                 command type
 * @context:             void pointer to the HDCP driver instance
 * @recvd_msg_buf:       message received from the sink
 * @recvd_msg_len:       length of message received from the sink
 * @timeout:             time out value for timed transactions
 */
struct hdcp_lib_wakeup_data {
	enum hdcp_lib_wakeup_cmd cmd;
	void *context;
	char *recvd_msg_buf;
	uint32_t recvd_msg_len;
	uint32_t timeout;
};

/**
 * struct hdcp_msg_part - a single part of an HDCP 2.2 message
 * @name:       user readable message name
 * @offset:     message part offset
 * @length      message part length
 */
struct hdcp_msg_part {
	char *name;
	uint32_t offset;
	uint32_t length;
};

/**
 * struct hdcp_msg_data - a full HDCP 2.2 message containing one or more parts
 * @num_messages:   total number of parts in a full message
 * @messages:       array containing num_messages parts
 * @rx_status:      value of rx_status register
 */
struct hdcp_msg_data {
	uint32_t num_messages;
	struct hdcp_msg_part messages[HDCP_MAX_MESSAGE_PARTS];
	uint8_t rx_status;
};

/**
 * struct hdcp_wakeup_data - command and data sent to display transport layer
 * @cmd:            command type
 * @context:        void pointer to the display transport layer
 * @send_msg_buf:   buffer containing message to be sent to sink
 * @send_msg_len:   length of the message to be sent to sink
 * @timeout:        timeout value for timed transactions
 * @abort_mask:     mask used to determine whether HDCP link is valid
 * @message_data:   a pointer to the message description
 */
struct hdcp_wakeup_data {
	enum hdcp_wakeup_cmd cmd;
	void *context;
	char *send_msg_buf;
	uint32_t send_msg_len;
	uint32_t timeout;
	uint8_t abort_mask;
	const struct hdcp_msg_data *message_data;
};

static inline char *hdcp_cmd_to_str(uint32_t cmd)
{
	switch (cmd) {
	case HDCP_WKUP_CMD_SEND_MESSAGE:
		return "HDCP_WKUP_CMD_SEND_MESSAGE";
	case HDCP_WKUP_CMD_RECV_MESSAGE:
		return "HDCP_WKUP_CMD_RECV_MESSAGE";
	case HDCP_WKUP_CMD_STATUS_SUCCESS:
		return "HDCP_WKUP_CMD_STATUS_SUCCESS";
	case HDCP_WKUP_CMD_STATUS_FAILED:
		return "HDCP_WKUP_CMD_STATUS_FAIL";
	case HDCP_WKUP_CMD_LINK_POLL:
		return "HDCP_WKUP_CMD_LINK_POLL";
	case HDCP_WKUP_CMD_AUTHENTICATE:
		return "HDCP_WKUP_CMD_AUTHENTICATE";
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

/**
 * struct hdcp_txmtr_ops - interface to HDCP Driver
 * @wakeup:                  wake the HDCP driver with a new command
 * @feature_supported:       checks for HDCP support on the target device
 */
struct hdcp_txmtr_ops {
	int (*wakeup)(struct hdcp_lib_wakeup_data *data);
	bool (*feature_supported)(void *phdcpcontext);
};

/**
 * struct hdcp_client_ops - call back functions to display transport layer
 * @wakeup:            wake up display transport layer with a new command
 * @notify_lvl_change  notify of encryption level changes
 */
struct hdcp_client_ops {
	int (*wakeup)(struct hdcp_wakeup_data *data);
	void (*notify_lvl_change)(void *client_ctx, int min_lvl);
};

/**
 * enum hdcp_device_type - display interface types
 * @HDCP_TXMTR_HDMI:  HDMI interface
 * @HDCP_TXMTR_DP:  DisplayPort interface
 */
enum hdcp_device_type {
	HDCP_TXMTR_HDMI = 0x8001,
	HDCP_TXMTR_DP = 0x8002
};

/**
 * struct hdcp_register_data - data used in HDCP driver clients' registration
 * @client_ops:          call back functions from the client
 * @txmtr_ops:           HDCP driver interface
 * @device_type:         display interface type of the client
 * @client_ctx:          void pointer to client data object
 * @hdcp_ctx:            void pointer to HDCP driver reference for client use
 */
struct hdcp_register_data {
	struct hdcp_client_ops *client_ops;
	struct hdcp_txmtr_ops *txmtr_ops;
	enum hdcp_device_type device_type;
	void *client_ctx;
	void **hdcp_ctx;
};

int hdcp_library_register(struct hdcp_register_data *data);
void hdcp_library_deregister(void *phdcpcontext);
bool hdcp1_check_if_supported_load_app(void);
int hdcp1_set_keys(uint32_t *aksv_msb, uint32_t *aksv_lsb);
int hdcp1_set_enc(bool enable);
void hdcp1_cache_repeater_topology(void *hdcp1_cached_tp);
void hdcp1_notify_topology(void);
#endif /* __HDCP_QSEECOM_H */
