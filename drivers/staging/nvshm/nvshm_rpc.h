/*
 * Copyright (C) 2013 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_STAGING_NVSHM_NVSHM_RPC_H
#define __DRIVERS_STAGING_NVSHM_NVSHM_RPC_H

#include <linux/types.h>

/**
 * Type for a RPC message (request or response.)
 *
 * @param payload Payload to send across
 * @param length Payload length - DO NOT MODIFY
 * @param private An internal context - DO NOT MODIFY
 */
struct nvshm_rpc_message {
	void *payload;
	/* The fields below are set at allocation time and are private */
	u32 length;
	void *private;
};

/**
 * Set a default dispatcher.
 *
 * The default dispatcher is the dispatcher that receives requests from clients
 * on the remote processor, while responses are sent back the originator's
 * callback automatically.
 *
 * Reminder: the callback (or one of its sub-processes) MUST free the message.
 *
 * @param callback Callback to use to receive incoming messages
 * @param context Context to remind at callback time (may be NULL)
 */
void nvshm_rpc_setdispatcher(
	void (*callback)(struct nvshm_rpc_message *message, void *context),
	void *context);

/**
 * Allocate a message buffer for request.
 *
 * The point here is for the client to fill in this buffer and not make a copy.
 * NOTE: SENT MESSAGES ARE FREED AUTOMATICALLY.
 *
 * Reminder: the callback (or one of its sub-processes) MUST free the message.
 *
 * @param size Size of the buffer to allocate
 * @param callback Callback to use to receive ASYNCHRONOUS responses
 * @param context A user context to pass to the callback, if relevant
 * @return a buffer, or NULL on error
 */
struct nvshm_rpc_message *nvshm_rpc_allocrequest(
	u32 size,
	void (*callback)(struct nvshm_rpc_message *message, void *context),
	void *context);

/**
 * Allocate a message buffer for response.
 *
 * The point here is for the client to fill in this buffer and avoid making a
 * copy.
 * NOTE: SENT MESSAGES ARE FREED AUTOMATICALLY.
 *
 * @param size Size of the buffer to allocate
 * @param request Request message as received
 * @return a buffer, or NULL on error
 */
struct nvshm_rpc_message *nvshm_rpc_allocresponse(
	u32 size,
	const struct nvshm_rpc_message *request);

/**
 * Free a message buffer.
 *
 * Use of this function should never be need if the message is sent, as the
 * destruction is then automatic.  It is needed to destroy the response to
 * synchronous calls though, and the message passed to both dispatcher and
 * message callbacks.
 *
 * @param message Message to free
 */
void nvshm_rpc_free(
	struct nvshm_rpc_message *message);

/**
 * Send a request or response message.
 *
 * Responses go through the callback (if any)
 *
 * @param message Request or response to send, automatically freed once sent
 * @return 0, or negative on error
 */
int nvshm_rpc_send(
	struct nvshm_rpc_message *message);

#endif /* #ifndef __DRIVERS_STAGING_NVSHM_NVSHM_RPC_H */
