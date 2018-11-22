/*
 * drivers/char/vs_serial_server.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Serial vService server driver
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
#include <vservices/protocol/serial/server.h>

#include "vs_serial_common.h"

#define server_state_to_port(state) \
	container_of(state, struct vtty_port, u.vs_server)

static struct vs_mbuf *vs_serial_server_alloc_msg_buf(struct vtty_port *port,
		struct vs_pbuf *pbuf, gfp_t gfp_flags)
{
	return vs_server_serial_serial_alloc_msg(&port->u.vs_server, pbuf,
			gfp_flags);
}

static void vs_serial_server_free_msg_buf(struct vtty_port *port,
		struct vs_mbuf *mbuf, struct vs_pbuf *pbuf)
{
	vs_server_serial_serial_free_msg(&port->u.vs_server, pbuf, mbuf);
}

static int vs_serial_server_send_msg_buf(struct vtty_port *port,
		struct vs_mbuf *mbuf, struct vs_pbuf *pbuf)
{
	return vs_server_serial_serial_send_msg(&port->u.vs_server, *pbuf, mbuf);
}

static bool vs_serial_server_is_vservices_running(struct vtty_port *port)
{
	return VSERVICE_BASE_STATE_IS_RUNNING(port->u.vs_server.state.base);
}

static struct vtty_port_ops server_port_ops = {
	.alloc_msg_buf	= vs_serial_server_alloc_msg_buf,
	.free_msg_buf	= vs_serial_server_free_msg_buf,
	.send_msg_buf	= vs_serial_server_send_msg_buf,
	.is_running	= vs_serial_server_is_vservices_running,
};

static struct vs_server_serial_state *
vs_serial_server_alloc(struct vs_service_device *service)
{
	struct vtty_port *port;

	port = vs_serial_alloc_port(service, &server_port_ops);
	if (!port)
		return NULL;

	dev_set_drvdata(&service->dev, port);
	return &port->u.vs_server;
}

static void vs_serial_server_release(struct vs_server_serial_state *_state)
{
	vs_serial_release(server_state_to_port(_state));
}

static void vs_serial_server_closed(struct vs_server_serial_state *_state)
{
	vs_serial_reset(server_state_to_port(_state));
}

static int
vs_serial_server_handle_message(struct vs_server_serial_state *_state,
		struct vs_pbuf data, struct vs_mbuf *mbuf)
{
	return vs_serial_handle_message(server_state_to_port(_state), mbuf,
			&data);
}

static vs_server_response_type_t
vs_serial_server_req_open(struct vs_server_serial_state *_state)
{
	struct vtty_port *port = server_state_to_port(_state);

	dev_dbg(&port->service->dev, "req_open\n");

	/* FIXME: Jira ticket SDK-3521 - ryanm. */
	port->max_transfer_size = vs_service_max_mbuf_size(port->service) - 8;
	_state->packet_size = port->max_transfer_size;

	return VS_SERVER_RESP_SUCCESS;
}

static vs_server_response_type_t
vs_serial_server_req_close(struct vs_server_serial_state *_state)
{
	struct vtty_port *port = server_state_to_port(_state);

	dev_dbg(&port->service->dev, "req_close\n");

	return VS_SERVER_RESP_SUCCESS;
}

static struct vs_server_serial vs_server_serial_driver = {
	.rx_atomic		= true,
	.alloc			= vs_serial_server_alloc,
	.release		= vs_serial_server_release,
	.closed			= vs_serial_server_closed,
	.open			= vs_serial_server_req_open,
	.close			= vs_serial_server_req_close,
	.serial = {
		.msg_msg	= vs_serial_server_handle_message,
	},

	/* Large default quota for batching data messages */
	.in_quota_best		= 16,
	.out_quota_best		= 16,
};

static int __init vs_serial_server_init(void)
{
	return vservice_serial_server_register(&vs_server_serial_driver,
			"vserial");
}

static void __exit vs_serial_server_exit(void)
{
	vservice_serial_server_unregister(&vs_server_serial_driver);
}

module_init(vs_serial_server_init);
module_exit(vs_serial_server_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Serial Server Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
