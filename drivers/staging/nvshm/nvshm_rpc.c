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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/sunrpc/msg_prot.h>
#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_iobuf.h"
#include "nvshm_rpc.h"

enum {
	CONCURRENT_REQUESTS_MAX = 16,   /* MUST be a power of two */
};

struct nvshm_rpc_header {
	u32 xid;                        /* Endianness does not matter */
	enum rpc_msg_type msg_type;
};

typedef void (*nvshm_rpc_callback_t)(
	struct nvshm_rpc_message *message,
	void *context);

struct nvshm_rpc_request {
	u32 requestid;
	nvshm_rpc_callback_t callback;
	void *context;
};

struct nvshm_rpc {
	int chanid;
	struct nvshm_channel *pchan;
	struct nvshm_handle *handle;
	nvshm_rpc_callback_t dispatcher_callback;
	void *dispatcher_context;
	struct mutex requestid_mutex;
	u32 requestid;
	struct nvshm_rpc_request requests[CONCURRENT_REQUESTS_MAX];
	u32 free_requests_number;
};

static struct nvshm_rpc rpc_private;

/*
 * We want the request ID to be unique, even if a rollover happens, so we have
 * the array index as LSBs and a counter as MSBs.  Hence the requirement for
 * CONCURRENT_REQUESTS_MAX to be a power of 2.
 */
static u32 request_create(nvshm_rpc_callback_t callback, void *context)
{
	u32 requestid = 0;
	int i;

	mutex_lock(&rpc_private.requestid_mutex);
	if (rpc_private.free_requests_number == 0)
		goto end;

	for (i = 0; i < CONCURRENT_REQUESTS_MAX; ++i) {
		struct nvshm_rpc_request *request;

		if (rpc_private.requests[i].requestid)
			continue;

		rpc_private.requestid += CONCURRENT_REQUESTS_MAX;
		/* Make sure we never give out request ID 0 */
		if (rpc_private.requestid + i == 0)
			rpc_private.requestid += CONCURRENT_REQUESTS_MAX;

		request = &rpc_private.requests[i];
		request->requestid = rpc_private.requestid + i;
		request->callback = callback;
		request->context = context;
		--rpc_private.free_requests_number;
		requestid = request->requestid;
		break;
	}
end:
	mutex_unlock(&rpc_private.requestid_mutex);
	return requestid;
}

static struct nvshm_rpc_request *request_get(u32 requestid)
{
	struct nvshm_rpc_request *request = NULL;
	int i;

	/*
	 * We only have two threads here: one that creates the message and sends
	 * it, and one that receives the answer to it and reads it, then deletes
	 * it. Creation implies a free slot, so will not interfere. Hence we do
	 * not need to lock.
	 */
	for (i = 0; i < CONCURRENT_REQUESTS_MAX; ++i)
		if (rpc_private.requests[i].requestid == requestid) {
			request = &rpc_private.requests[i];
			break;
		}
	return request;
}

static void request_delete(u32 requestid)
{
	int i;

	mutex_lock(&rpc_private.requestid_mutex);
	for (i = 0; i < CONCURRENT_REQUESTS_MAX; ++i)
		if (rpc_private.requests[i].requestid == requestid) {
			rpc_private.requests[i].requestid = 0;
			++rpc_private.free_requests_number;
			break;
		}
	mutex_unlock(&rpc_private.requestid_mutex);
}

static void nvshm_rpc_rx_event(struct nvshm_channel *chan,
			       struct nvshm_iobuf *iobuf) {
	u8 *data = NVSHM_IOBUF_PAYLOAD(rpc_private.handle, iobuf);
	struct nvshm_rpc_header *header;
	struct nvshm_rpc_message *message;

	header = (struct nvshm_rpc_header *) data;
	data += sizeof(*header);
	/* Create message structure */
	message = kmalloc(sizeof(*message), GFP_KERNEL);
	if (unlikely(!message)) {
		pr_err("failed to allocate message\n");
		goto failed;
	}

	message->private = iobuf;
	message->payload = data;
	message->length = iobuf->length - sizeof(*header);
	if (header->msg_type == ntohl(RPC_REPLY)) {
		struct nvshm_rpc_request *request = request_get(header->xid);
		nvshm_rpc_callback_t callback;
		void *context;

		if (!request) {
			pr_err("invalid request ID %u\n", header->xid);
			goto failed;
		}
		/* Free the request in case the callback wants to send */
		callback = request->callback;
		context = request->context;
		request_delete(header->xid);
		/* Call back */
		if (callback)
			callback(message, context);
		else
			nvshm_rpc_free(message);
	} else {
		/* Check payload length */
		if (message->length == 0) {
			/* Empty payload: for latency measurement */
			struct nvshm_rpc_message *response;

			response = nvshm_rpc_allocresponse(0, message);
			nvshm_rpc_send(response);
			nvshm_rpc_free(message);
		} else if (rpc_private.dispatcher_callback != NULL) {
			/* Dispatch */
			rpc_private.dispatcher_callback(message,
						rpc_private.dispatcher_context);
		} else {
			nvshm_rpc_free(message);
		}
	}

	return;
failed:
	kfree(message);
	nvshm_iobuf_free(iobuf);
}

static void nvshm_rpc_error_event(struct nvshm_channel *chan,
				  enum nvshm_error_id error)
{
}

static void nvshm_rpc_start_tx(struct nvshm_channel *chan)
{
}

static struct nvshm_if_operations nvshm_rpc_ops = {
	.rx_event = nvshm_rpc_rx_event,
	.error_event = nvshm_rpc_error_event,
	.start_tx = nvshm_rpc_start_tx
};

int nvshm_rpc_init(struct nvshm_handle *handle)
{
	int chan;
	int i;

	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++)
		if (handle->chan[chan].map.type == NVSHM_CHAN_RPC) {
			rpc_private.chanid = chan;
			rpc_private.handle = handle;
			rpc_private.pchan = nvshm_open_channel(chan,
							       &nvshm_rpc_ops,
							       &rpc_private);
			if (!rpc_private.pchan) {
				pr_err("failed to open channel\n");
				goto fail;
			}
			/* Only one RPC channel */
			break;
		}

	/* Initialize request ID stuff (never destroyed) */
	mutex_init(&rpc_private.requestid_mutex);
	rpc_private.requestid = 0;
	for (i = 0; i < CONCURRENT_REQUESTS_MAX; ++i)
		rpc_private.requests[i].requestid = 0;
	rpc_private.free_requests_number = CONCURRENT_REQUESTS_MAX;
	return 0;
fail:
	return -1;
}

void nvshm_rpc_cleanup(void)
{
	/* FIXME Check module ref count if we ever make this a module */
	if (!rpc_private.pchan) {
		pr_err("not initialized\n");
		return;
	}

	nvshm_close_channel(rpc_private.pchan);
	mutex_destroy(&rpc_private.requestid_mutex);
}

void nvshm_rpc_setdispatcher(nvshm_rpc_callback_t callback, void *context)
{
	/*
	 * The dispatcher callback is set at init and unset at cleanup, when no
	 * message can be received. This therefore does not need locking.
	 */
	rpc_private.dispatcher_callback = callback;
	rpc_private.dispatcher_context = context;
}

struct nvshm_rpc_message*
nvshm_rpc_allocrequest(u32 size,
		       nvshm_rpc_callback_t callback,
		       void *context)
{
	u32 requestid;
	struct nvshm_iobuf *iobuf;
	struct nvshm_rpc_message *message;
	u8 *data;
	struct nvshm_rpc_header *header;

	/* Initialize iobuf */
	if (!rpc_private.pchan) {
		pr_err("not initialized\n");
		return NULL;
	}

	/* Get request ID */
	do {
		requestid = request_create(callback, context);
		/* Should not happen anyway... */
		if (requestid == 0)
			udelay(50);
	} while (requestid == 0);

	/* Initialize iobuf */
	iobuf = nvshm_iobuf_alloc(rpc_private.pchan, sizeof(*header) + size);
	if (!iobuf) {
		request_delete(requestid);
		pr_err("failed to allocate iobuf\n");
		return NULL;
	}

	iobuf->length = sizeof(*header) + size;
	data = NVSHM_IOBUF_PAYLOAD(rpc_private.handle, iobuf);

	/* Initialize header */
	header = (struct nvshm_rpc_header *) data;
	header->xid = requestid;
	header->msg_type = htonl(RPC_CALL);
	data += sizeof(*header);

	/* Initialize message */
	message = kmalloc(sizeof(*message), GFP_KERNEL);
	if (!message) {
		request_delete(requestid);
		nvshm_iobuf_free(iobuf);
		pr_err("failed to allocate message\n");
		return NULL;
	}

	message->private = iobuf;
	message->payload = data;
	message->length = size;
	return message;
}

struct nvshm_rpc_message *nvshm_rpc_allocresponse(u32 size,
				const struct nvshm_rpc_message *request)
{
	struct nvshm_iobuf *req_iobuf = request->private;
	u8 *req_data;
	struct nvshm_iobuf *iobuf;
	struct nvshm_rpc_message *message;
	u8 *data;
	struct nvshm_rpc_header *req_header;
	struct nvshm_rpc_header *header;

	/* Reader request header */
	if (!req_iobuf) {
		pr_err("null request iobuf\n");
		return NULL;
	}

	req_data = NVSHM_IOBUF_PAYLOAD(rpc_private.handle, req_iobuf);

	/* Initialize iobuf */
	if (!rpc_private.pchan) {
		pr_err("not initialized\n");
		return NULL;
	}
	iobuf = nvshm_iobuf_alloc(rpc_private.pchan, sizeof(*header) + size);
	if (!iobuf) {
		pr_err("failed to allocate iobuf\n");
		return NULL;
	}

	iobuf->length = sizeof(*header) + size;
	data = NVSHM_IOBUF_PAYLOAD(rpc_private.handle, iobuf);

	/* Copy header and opaque data from request */
	header = (struct nvshm_rpc_header *) data;
	req_header = (struct nvshm_rpc_header *) req_data;
	header->xid = req_header->xid;
	header->msg_type = htonl(RPC_REPLY);
	data += sizeof(*header);

	/* Initialize message */
	message = kmalloc(sizeof(*message), GFP_KERNEL);
	if (!message) {
		pr_err("failed to allocate message\n");
		nvshm_iobuf_free(iobuf);
		return NULL;
	}

	message->private = iobuf;
	message->payload = data;
	message->length = size;
	return message;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_allocresponse);

void nvshm_rpc_free(struct nvshm_rpc_message *message)
{
	struct nvshm_iobuf *iobuf = message->private;

	nvshm_iobuf_free(iobuf);
	kfree(message);
}

int nvshm_rpc_send(struct nvshm_rpc_message *message)
{
	/* Send */
	struct nvshm_iobuf *iobuf = message->private;
	int rc;

	/* Note: as RPC traffic is very low, we don't care about flow control */
	rc = nvshm_write(rpc_private.pchan, iobuf);
	/* Do not free iobuf here (see SHM specification for details) */
	kfree(message);
	if (rc < 0)
		nvshm_iobuf_free(iobuf);

	return rc;
}
