/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SOTER_PRIVATE_H
#define SOTER_PRIVATE_H

#include <linux/semaphore.h>
#include <tee_drv.h>
#include <linux/types.h>

/* Some Global Platform error codes used in this driver */
#define TEEC_SUCCESS			0x00000000
#define TEEC_ERROR_BAD_PARAMETERS	0xFFFF0006
#define TEEC_ERROR_COMMUNICATION	0xFFFF000E
#define TEEC_ERROR_OUT_OF_MEMORY	0xFFFF000C

#define TEEC_ORIGIN_COMMS		0x00000002

struct call_queue {
	/* Serializes access to this struct */
	struct mutex mutex;
	struct list_head waiters;
};

struct soter_priv {
	struct tee_device *teedev;
	struct call_queue call_queue;
	struct tee_shm_pool *pool;
	void *memremaped_shm;
};

struct soter_session {
	struct list_head list_node;
	u32 session_id;
};

struct soter_context_data {
	/* Serializes access to this struct */
	struct mutex mutex;
	struct list_head sess_list;
};

int soter_open_session(struct tee_context *ctx,
		       struct tee_ioctl_open_session_arg *arg,
		       struct tee_param *param);
int soter_close_session(struct tee_context *ctx, u32 session);
int soter_invoke_func(struct tee_context *ctx, struct tee_ioctl_invoke_arg *arg,
		      struct tee_param *param);
int soter_cancel_func(struct tee_context *ctx, u32 cancel_id, u32 session);
#endif
