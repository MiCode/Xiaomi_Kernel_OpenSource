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

/*
 * struct hdcp_client_handle - handle for hdcp client
 * @qseecom_handle - for sending commands to qseecom
 * @hwKeyReady - if key has been set in hdmi hardware
 * @hdcp_workqueue - work queue for hdcp thread
 * @work_msg_arrive - work placed in the work queue
 * @listener_buf - buffer containing message shared with the client
 * @msglen - size message in the buffer
 * @tz_ctxhandle - context handle shared with tz
 * @hdcp_timeout - timeout in msecs shared for hdcp messages
 * @timeout_flag - flag to indicate that timeout has occured
 * @lc_init_flag - flag to indicate that msg from tz is lc_init
 * @ske_flag - flag to indicate that msg from tz is ske_send_eks
 * @done - completion structure for the timer
 * @client_ctx - client context maintained by hdmi
 * @client_ops - handle to call APIs exposed by hdcp client
 * @timeout_lock - this lock protects hdcp_timeout field
 * @msg_lock - this lock protects the message buffer
 */
struct hdcp_client_handle {
	bool hwKeyReady;
	struct workqueue_struct *hdcp_workqueue;
	struct work_struct work_msg_arrive;
	unsigned char *listener_buf;
	uint32_t msglen;
	uint32_t tz_ctxhandle;
	uint32_t hdcp_timeout;
	bool timeout_flag;
	bool lc_init_flag;
	bool ske_flag;
	bool no_stored_km_flag;
	struct completion done;
	void *client_ctx;
	struct hdcp_client_ops *client_ops;
	struct mutex mutex;
};

struct hdcp_txmtr_ops {
	int (*hdcp_txmtr_init)(void *phdcpcontext);
	int (*hdcp_txmtr_deinit)(void *phdcpcontext);
	int (*hdcp_txmtr_process_message)(void *phdcpcontext,
		unsigned char *msg, uint32_t msg_size);
	int (*hdcp_txmtr_get_state)(void *phdcpcontext,
		uint32_t *state);
};

struct hdcp_client_ops {
	int (*hdcp_send_message)(void *client_ctx, void *phdcpcontext,
		char *message, uint32_t msg_size);
	int (*hdcp_tz_error)(void *client_ctx, void *phdcpcontext);
	int (*hdcp_tz_timeout)(void *client_ctx, void *phdcpcontext,
		bool auth_failure);
};

int hdcp_library_init(void **pphdcpcontext, struct hdcp_client_ops *client_ops,
	struct hdcp_txmtr_ops *txmtr_ops, void *client_ctx);
int hdcp_library_deinit(void *phdcpcontext);
int hdcp1_set_keys(uint32_t *aksv_msb, uint32_t *aksv_lsb);

#endif /* __HDCP_QSEECOM_H */
