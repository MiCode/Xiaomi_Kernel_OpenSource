/* Copyright (c) 2011-2016, 2018, The Linux Foundation. All rights reserved.
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

#ifndef _IPC_ROUTER_PRIVATE_H
#define _IPC_ROUTER_PRIVATE_H

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/msm_ipc.h>
#include <linux/ipc_router.h>
#include <linux/ipc_router_xprt.h>

#include <net/sock.h>

/* definitions for the R2R wire protcol */
#define IPC_ROUTER_V1		1
/*
 * Ambiguous definition but will enable multiplexing IPC_ROUTER_V2 packets
 * with an existing alternate transport in user-space, if needed.
 */
#define IPC_ROUTER_V2		3
#define IPC_ROUTER_VER_BITMASK ((BIT(IPC_ROUTER_V1)) | (BIT(IPC_ROUTER_V2)))
#define IPC_ROUTER_HELLO_MAGIC 0xE110
#define IPC_ROUTER_CHECKSUM_MASK 0xFFFF

#define IPC_ROUTER_ADDRESS			0x0000FFFF

#define IPC_ROUTER_NID_LOCAL	CONFIG_IPC_ROUTER_NODE_ID
#define MAX_IPC_PKT_SIZE 66000

#define IPC_ROUTER_LOW_RX_QUOTA		5
#define IPC_ROUTER_HIGH_RX_QUOTA	10

#define IPC_ROUTER_INFINITY -1
#define DEFAULT_RCV_TIMEO IPC_ROUTER_INFINITY
#define DEFAULT_SND_TIMEO IPC_ROUTER_INFINITY

#define ALIGN_SIZE(x) ((4 - ((x) & 3)) & 3)

#define ALL_SERVICE 0xFFFFFFFF
#define ALL_INSTANCE 0xFFFFFFFF

#define CONTROL_FLAG_CONFIRM_RX 0x1
#define CONTROL_FLAG_OPT_HDR 0x2

enum {
	CLIENT_PORT,
	SERVER_PORT,
	CONTROL_PORT,
	IRSC_PORT,
};

enum {
	NULL_MODE,
	SINGLE_LINK_MODE,
	MULTI_LINK_MODE,
};

enum {
	CONNECTION_RESET = -1,
	NOT_CONNECTED,
	CONNECTED,
};

struct msm_ipc_sock {
	struct sock sk;
	struct msm_ipc_port *port;
	void *default_node_vote_info;
};

/**
 * msm_ipc_router_create_raw_port() - Create an IPC Router port
 * @endpoint: User-space space socket information to be cached.
 * @notify: Function to notify incoming events on the port.
 *   @event: Event ID to be handled.
 *   @oob_data: Any out-of-band data associated with the event.
 *   @oob_data_len: Size of the out-of-band data, if valid.
 *   @priv: Private data registered during the port creation.
 * @priv: Private Data to be passed during the event notification.
 *
 * @return: Valid pointer to port on success, NULL on failure.
 *
 * This function is used to create an IPC Router port. The port is used for
 * communication locally or outside the subsystem.
 */
struct msm_ipc_port *msm_ipc_router_create_raw_port(void *endpoint,
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv),
	void *priv);
int msm_ipc_router_send_to(struct msm_ipc_port *src,
			   struct sk_buff_head *data,
			   struct msm_ipc_addr *dest,
			   long timeout);
int msm_ipc_router_read(struct msm_ipc_port *port_ptr,
			struct rr_packet **pkt,
			size_t buf_len);

int msm_ipc_router_recv_from(struct msm_ipc_port *port_ptr,
		      struct rr_packet **pkt,
		      struct msm_ipc_addr *src_addr,
		      long timeout);
int msm_ipc_router_register_server(struct msm_ipc_port *server_port,
			    struct msm_ipc_addr *name);
int msm_ipc_router_unregister_server(struct msm_ipc_port *server_port);

int msm_ipc_router_init_sockets(void);
void msm_ipc_router_exit_sockets(void);

void msm_ipc_sync_sec_rule(uint32_t service, uint32_t instance, void *rule);

void msm_ipc_sync_default_sec_rule(void *rule);

int msm_ipc_router_rx_data_wait(struct msm_ipc_port *port_ptr, long timeout);

void msm_ipc_router_free_skb(struct sk_buff_head *skb_head);

/**
 * ipc_router_set_conn() - Set the connection by initializing dest address
 * @port_ptr: Local port in which the connection has to be set.
 * @addr: Destination address of the connection.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
int ipc_router_set_conn(struct msm_ipc_port *port_ptr,
			struct msm_ipc_addr *addr);

void *msm_ipc_load_default_node(void);

void msm_ipc_unload_default_node(void *pil);

/**
 * ipc_router_dummy_write_space() - Dummy write space available callback
 * @sk:	Socket pointer for which the callback is called.
 */
void ipc_router_dummy_write_space(struct sock *sk);

#endif
