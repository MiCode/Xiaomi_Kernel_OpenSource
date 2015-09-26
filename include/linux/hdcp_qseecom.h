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

struct hdcp_txmtr_ops {
	int (*start)(void *phdcpcontext);
	int (*stop)(void *phdcpcontext);
	bool (*feature_supported)(void *phdcpcontext);

	int (*process_message)(void *phdcpcontext,
		unsigned char *msg, uint32_t msg_size);
	int (*hdcp_txmtr_get_state)(void *phdcpcontext,
		uint32_t *state);
	int (*hdcp_txmtr_query_stream_type)(void *phdcpcontext);
};

struct hdcp_client_ops {
	int (*hdcp_send_message)(void *client_ctx,
		char *message, uint32_t msg_size);
	int (*hdcp_recv_message)(void *client_ctx,
		char *message, uint32_t msg_size,
		u32 timeout);
	int (*hdcp_tz_error)(void *client_ctx);
};

int hdcp_library_register(void **pphdcpcontext,
	struct hdcp_client_ops *client_ops,
	struct hdcp_txmtr_ops *txmtr_ops, void *client_ctx);
void hdcp_library_deregister(void *phdcpcontext);
int hdcp1_set_keys(uint32_t *aksv_msb, uint32_t *aksv_lsb);

#endif /* __HDCP_QSEECOM_H */
