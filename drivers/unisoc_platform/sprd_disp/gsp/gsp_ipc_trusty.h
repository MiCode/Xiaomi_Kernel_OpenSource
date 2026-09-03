/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_IPC_TRUSTY_H_
#define _GSP_IPC_TRUSTY_H_

struct gsp_message {
	uint32_t cmd;
	uint8_t payload[4];
};

enum gsp_command {
	TA_SET_SECURE = 8,
	TA_SET_UNSECURE  = 9
};

#ifdef CONFIG_DRM_SPRD_GSP_IPC_TRUSTY
int gsp_tipc_init(void);
ssize_t gsp_tipc_write(void *data_ptr, size_t len);
ssize_t gsp_tipc_read(void *data_ptr, size_t max_len);
void gsp_tipc_exit(void);
#else
static int gsp_tipc_init(void)
{
	GSP_DEBUG("GSP does not support tipc init\n");
	return 0;
}

static ssize_t gsp_tipc_write(void *data_ptr, size_t len)
{
	GSP_DEBUG("GSP does not support tipc write\n");
	return 0;
}

static ssize_t gsp_tipc_read(void *data_ptr, size_t max_len)
{
	GSP_DEBUG("GSP does not support tipc read\n");
	return 0;
}

#endif

#endif
