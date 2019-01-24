/*
 * drivers/char/vs_serial_client.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Serial vService client driver
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <vservices/transport.h>
#include <vservices/types.h>
#include <vservices/buffer.h>
#include <vservices/service.h>

#include <vservices/protocol/serial/common.h>
#include <vservices/protocol/serial/types.h>
#include <vservices/protocol/serial/client.h>

#include "vs_serial_common.h"

#define client_state_to_port(state) \
	container_of(state, struct vtty_port, u.vs_client)

static struct vs_mbuf *vs_serial_client_alloc_msg_buf(struct vtty_port *port,
		struct vs_pbuf *pbuf, gfp_t gfp_flags)
{
	return vs_client_serial_serial_alloc_msg(&port->u.vs_client, pbuf,
			gfp_flags);
}

static void vs_serial_client_free_msg_buf(struct vtty_port *port,
		struct vs_mbuf *mbuf, struct vs_pbuf *pbuf)
{
	vs_client_serial_serial_free_msg(&port->u.vs_client, pbuf, mbuf);
}

static int vs_serial_client_send_msg_buf(struct vtty_port *port,
		struct vs_mbuf *mbuf, struct vs_pbuf *pbuf)
{
	return vs_client_serial_serial_send_msg(&port->u.vs_client, *pbuf,
			mbuf);
}

static bool vs_serial_client_is_vservices_running(struct vtty_port *port)
{
	return VSERVICE_BASE_STATE_IS_RUNNING(port->u.vs_client.state.base);
}

static struct vtty_port_ops client_port_ops = {
	.alloc_msg_buf	= vs_serial_client_alloc_msg_buf,
	.free_msg_buf	= vs_serial_client_free_msg_buf,
	.send_msg_buf	= vs_serial_client_send_msg_buf,
	.is_running	= vs_serial_client_is_vservices_running,
};

static struct vs_client_serial_state *
vs_serial_client_alloc(struct vs_service_device *service)
{
	struct vtty_port *port;

	port = vs_serial_alloc_port(service, &client_port_ops);
	if (!port)
		return NULL;

	dev_set_drvdata(&service->dev, port);
	return &port->u.vs_client;
}

static void vs_serial_client_release(struct vs_client_serial_state *_state)
{
	vs_serial_release(client_state_to_port(_state));
}

static void vs_serial_client_closed(struct vs_client_serial_state *_state)
{
	vs_serial_reset(client_state_to_port(_state));
}

static void vs_serial_client_opened(struct vs_client_serial_state *_state)
{
	struct vtty_port *port = client_state_to_port(_state);

	dev_dbg(&port->service->dev, "ack_open\n");
	port->max_transfer_size = _state->packet_size;
}

static int
vs_serial_client_handle_message(struct vs_client_serial_state *_state,
		struct vs_pbuf data, struct vs_mbuf *mbuf)
{
	return vs_serial_handle_message(client_state_to_port(_state), mbuf,
			&data);
}

static struct vs_client_serial vs_client_serial_driver = {
	.rx_atomic		= true,
	.alloc			= vs_serial_client_alloc,
	.release		= vs_serial_client_release,
	.closed			= vs_serial_client_closed,
	.opened			= vs_serial_client_opened,
	.serial = {
		.msg_msg	= vs_serial_client_handle_message,
	},
};

static int __init vs_serial_client_init(void)
{
	return vservice_serial_client_register(&vs_client_serial_driver,
			"vserial");
}

static void __exit vs_serial_client_exit(void)
{
	vservice_serial_client_unregister(&vs_client_serial_driver);
}

module_init(vs_serial_client_init);
module_exit(vs_serial_client_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Serial Client Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
