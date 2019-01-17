/*
 * drivers/char/vs_serial_common.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _VS_SERIAL_COMMON_H
#define _VS_SERIAL_COMMON_H

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/console.h>

#include <vservices/protocol/serial/common.h>
#include <vservices/protocol/serial/types.h>
#include <vservices/protocol/serial/server.h>
#include <vservices/protocol/serial/client.h>

#define OUTBUFFER_SIZE 1024
#define vtty_list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)

struct vtty_port;
struct vs_service_device;

struct vtty_port_ops {
	struct vs_mbuf	*(*alloc_msg_buf)(struct vtty_port *port,
			struct vs_pbuf *pbuf, gfp_t gfp_flags);
	void		(*free_msg_buf)(struct vtty_port *port,
			struct vs_mbuf *mbuf, struct vs_pbuf *pbuf);
	int		(*send_msg_buf)(struct vtty_port *port,
			struct vs_mbuf *mbuf, struct vs_pbuf *pbuf);
	bool		(*is_running)(struct vtty_port *port);
};

struct vtty_port {
	union {
		struct vs_client_serial_state vs_client;
		struct vs_server_serial_state vs_server;
	} u;

	struct vs_service_device	*service;
	int				port_num;

	struct tty_driver		*vtty_driver;

	struct vtty_port_ops		ops;

	/* output data */
	bool				doing_release;

	int				max_transfer_size;

	/* Tracks if tty layer can receive data from driver */
	bool				tty_canrecv;

	/*
	 * List of pending incoming buffers from the vServices stack. If we
	 * receive a buffer, but cannot write it to the tty layer then we
	 * queue it on this list to handle later. in_lock protects access to
	 * the pending_in_packets list and the tty_canrecv field.
	 */
	struct list_head		pending_in_packets;
	spinlock_t			in_lock;

#ifdef CONFIG_OKL4_VTTY_CONSOLE
	struct console			console;
#endif

	struct tty_port			port;
};

extern struct vtty_port *
vs_serial_alloc_port(struct vs_service_device *service,
	struct vtty_port_ops *port_ops);
extern void vs_serial_release(struct vtty_port *port);
extern void vs_serial_reset(struct vtty_port *port);
extern int vs_serial_handle_message(struct vtty_port *port,
		struct vs_mbuf *mbuf, struct vs_pbuf *pbuf);

#endif /* _VS_SERIAL_COMMON_H */
