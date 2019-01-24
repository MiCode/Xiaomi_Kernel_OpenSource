/*
 * include/vservices/transport.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains the transport vtable structure. This is made public so
 * that the application drivers can call the vtable functions directly (via
 * the inlined wrappers in service.h) rather than indirectly via a function
 * call.
 *
 */

#ifndef _VSERVICES_TRANSPORT_H_
#define _VSERVICES_TRANSPORT_H_

#include <linux/types.h>

#include <vservices/types.h>

struct vs_transport;
struct vs_mbuf;
struct vs_service_device;

/**
 * struct vs_transport_vtable - Transport driver operations. Transport drivers
 * must provide implementations for all operations in this table.
 * --- Message buffer allocation ---
 * @alloc_mbuf: Allocate an mbuf of the given size for the given service
 * @free_mbuf: Deallocate an mbuf
 * @mbuf_size: Return the size in bytes of a message buffer. The size returned
 *             should be the total number of bytes including any headers.
 * @max_mbuf_size: Return the maximum allowable message buffer allocation size.
 * --- Message sending ---
 * @send: Queue an mbuf for sending
 * @flush: Start the transfer for the current message batch, if any
 * @notify: Send a notification
 * --- Transport-level reset handling ---
 * @reset: Reset the transport layer
 * @ready: Ready the transport layer
 * --- Service management ---
 * @service_add: A new service has been added to this transport's session
 * @service_remove: A service has been removed from this transport's session
 * @service_start: A service on this transport's session has had its resource
 *     allocations set and is about to start. This is always interleaved with
 *     service_reset, with one specific exception: the core service client,
 *     which has its quotas initially hard-coded to 0 send / 1 recv and
 *     adjusted when the initial startup message arrives.
 * @service_reset: A service on this transport's session has just been reset,
 *     and any resources allocated to it should be cleaned up to prepare
 *     for later reallocation.
 * @service_send_avail: The number of message buffers that this service is
 *                      able to send before going over quota.
 * --- Query transport capabilities ---
 * @get_notify_bits: Fetch the number of sent and received notification bits
 *     supported by this transport. Note that this can be any positive value
 *     up to UINT_MAX.
 * @get_quota_limits: Fetch the total send and receive message buffer quotas
 *     supported by this transport. Note that this can be any positive value
 *     up to UINT_MAX.
 */
struct vs_transport_vtable {
	/* Message buffer allocation */
	struct vs_mbuf *(*alloc_mbuf)(struct vs_transport *transport,
			struct vs_service_device *service, size_t size,
			gfp_t gfp_flags);
	void (*free_mbuf)(struct vs_transport *transport,
			struct vs_service_device *service,
			struct vs_mbuf *mbuf);
	size_t (*mbuf_size)(struct vs_mbuf *mbuf);
	size_t (*max_mbuf_size)(struct vs_transport *transport);

	/* Sending messages */
	int (*send)(struct vs_transport *transport,
			struct vs_service_device *service,
			struct vs_mbuf *mbuf, unsigned long flags);
	int (*flush)(struct vs_transport *transport,
			struct vs_service_device *service);
	int (*notify)(struct vs_transport *transport,
			struct vs_service_device *service,
			unsigned long bits);

	/* Raising and clearing transport-level reset */
	void (*reset)(struct vs_transport *transport);
	void (*ready)(struct vs_transport *transport);

	/* Service management */
	int (*service_add)(struct vs_transport *transport,
			struct vs_service_device *service);
	void (*service_remove)(struct vs_transport *transport,
			struct vs_service_device *service);

	int (*service_start)(struct vs_transport *transport,
			struct vs_service_device *service);
	int (*service_reset)(struct vs_transport *transport,
			struct vs_service_device *service);

	ssize_t (*service_send_avail)(struct vs_transport *transport,
			struct vs_service_device *service);

	/* Query transport capabilities */
	void (*get_notify_bits)(struct vs_transport *transport,
			unsigned *send_notify_bits, unsigned *recv_notify_bits);
	void (*get_quota_limits)(struct vs_transport *transport,
			unsigned *send_quota, unsigned *recv_quota);
};

/* Flags for .send */
#define VS_TRANSPORT_SEND_FLAGS_MORE		0x1

/**
 * struct vs_transport - A structure representing a transport
 * @type: type of transport i.e. microvisror/loopback etc
 * @vt: Transport operations table
 * @notify_info: Array of incoming notification settings
 * @notify_info_size: Size of the incoming notification array
 */
struct vs_transport {
	const char *type;
	const struct vs_transport_vtable *vt;
	struct vs_notify_info *notify_info;
	int notify_info_size;
};

/**
 * struct vs_mbuf - Message buffer. This is always allocated and released by the
 * transport callbacks defined above, so it may be embedded in a
 * transport-specific structure containing additional state.
 * @data: Message data buffer
 * @size: Size of the data buffer in bytes
 * @is_recv: True if this mbuf was received from the other end of the
 *           transport. False if it was allocated by this end for sending.
 * @priv: Private value that will not be touched by the framework
 * @queue: list_head for entry in lists. The session layer uses this queue
 * for receiving messages. The transport driver may use this queue for its
 * own purposes when sending messages.
 */
struct vs_mbuf {
	void *data;
	size_t size;
	bool is_recv;
	void *priv;
	struct list_head queue;
};

#endif /* _VSERVICES_TRANSPORT_H_ */
