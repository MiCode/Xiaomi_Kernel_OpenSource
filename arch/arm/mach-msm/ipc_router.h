/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_IPC_ROUTER_H
#define _ARCH_ARM_MACH_MSM_IPC_ROUTER_H

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/msm_ipc.h>

#include <net/sock.h>

/* definitions for the R2R wire protcol */
#define IPC_ROUTER_V1		1
/*
 * Ambiguous definition but will enable multiplexing IPC_ROUTER_V2 packets
 * with an existing alternate transport in user-space, if needed.
 */
#define IPC_ROUTER_V2		3

#define IPC_ROUTER_ADDRESS			0x0000FFFF

#define IPC_ROUTER_NID_LOCAL			1
#define MAX_IPC_PKT_SIZE 66000

#define IPC_ROUTER_CTRL_CMD_DATA		1
#define IPC_ROUTER_CTRL_CMD_HELLO		2
#define IPC_ROUTER_CTRL_CMD_BYE			3
#define IPC_ROUTER_CTRL_CMD_NEW_SERVER		4
#define IPC_ROUTER_CTRL_CMD_REMOVE_SERVER	5
#define IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT	6
#define IPC_ROUTER_CTRL_CMD_RESUME_TX		7
#define IPC_ROUTER_CTRL_CMD_EXIT		8
#define IPC_ROUTER_CTRL_CMD_PING		9

#define IPC_ROUTER_DEFAULT_RX_QUOTA	5

#define IPC_ROUTER_XPRT_EVENT_DATA  1
#define IPC_ROUTER_XPRT_EVENT_OPEN  2
#define IPC_ROUTER_XPRT_EVENT_CLOSE 3

#define IPC_ROUTER_INFINITY -1
#define DEFAULT_RCV_TIMEO 0

#define ALIGN_SIZE(x) ((4 - ((x) & 3)) & 3)

#define ALL_SERVICE 0xFFFFFFFF
#define ALL_INSTANCE 0xFFFFFFFF

#define CONTROL_FLAG_CONFIRM_RX 0x1
#define CONTROL_FLAG_OPT_HDR 0x2

#define FRAG_PKT_WRITE_ENABLE 0x1

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

/**
 * rr_control_msg - Control message structure
 * @cmd: Command identifier for HELLO message in Version 1.
 * @hello: Message structure for HELLO message in Version 2.
 * @srv: Message structure for NEW_SERVER/REMOVE_SERVER events.
 * @cli: Message structure for REMOVE_CLIENT event.
 */
union rr_control_msg {
	uint32_t cmd;
	struct {
		uint32_t cmd;
		uint32_t magic;
		uint32_t capability;
	} hello;
	struct {
		uint32_t cmd;
		uint32_t service;
		uint32_t instance;
		uint32_t node_id;
		uint32_t port_id;
	} srv;
	struct {
		uint32_t cmd;
		uint32_t node_id;
		uint32_t port_id;
	} cli;
};

/**
 * rr_header_v1 - IPC Router header version 1
 * @version: Version information.
 * @type: IPC Router Message Type.
 * @src_node_id: Source Node ID of the message.
 * @src_port_id: Source Port ID of the message.
 * @control_flag: Flag to indicate flow control.
 * @size: Size of the IPC Router payload.
 * @dst_node_id: Destination Node ID of the message.
 * @dst_port_id: Destination Port ID of the message.
 */
struct rr_header_v1 {
	uint32_t version;
	uint32_t type;
	uint32_t src_node_id;
	uint32_t src_port_id;
	uint32_t control_flag;
	uint32_t size;
	uint32_t dst_node_id;
	uint32_t dst_port_id;
};

/**
 * rr_header_v2 - IPC Router header version 2
 * @version: Version information.
 * @type: IPC Router Message Type.
 * @control_flag: Flags to indicate flow control, optional header etc.
 * @size: Size of the IPC Router payload.
 * @src_node_id: Source Node ID of the message.
 * @src_port_id: Source Port ID of the message.
 * @dst_node_id: Destination Node ID of the message.
 * @dst_port_id: Destination Port ID of the message.
 */
struct rr_header_v2 {
	uint8_t version;
	uint8_t type;
	uint16_t control_flag;
	uint32_t size;
	uint16_t src_node_id;
	uint16_t src_port_id;
	uint16_t dst_node_id;
	uint16_t dst_port_id;
} __attribute__((__packed__));

union rr_header {
	struct rr_header_v1 hdr_v1;
	struct rr_header_v2 hdr_v2;
};

#define IPC_ROUTER_HDR_SIZE sizeof(union rr_header)

/**
 * rr_packet - Router to Router packet structure
 * @list: Pointer to prev & next packets in a port's rx list.
 * @hdr: Header information extracted from or prepended to a packet.
 * @pkt_fragment_q: Queue of SKBs containing payload.
 * @length: Length of data in the chain of SKBs
 */
struct rr_packet {
	struct list_head list;
	struct rr_header_v1 hdr;
	struct sk_buff_head *pkt_fragment_q;
	uint32_t length;
};

struct msm_ipc_sock {
	struct sock sk;
	struct msm_ipc_port *port;
	void *default_pil;
};

/**
 * msm_ipc_router_xprt - Structure to hold XPRT specific information
 * @name: Name of the XPRT.
 * @link_id: Network cluster ID to which the XPRT belongs to.
 * @priv: XPRT's private data.
 * @get_version: Method to get header version supported by the XPRT.
 * @get_option: Method to get XPRT specific options.
 * @read_avail: Method to get data size available to be read from the XPRT.
 * @read: Method to read data from the XPRT.
 * @write_avail: Method to get write space available in the XPRT.
 * @write: Method to write data to the XPRT.
 * @close: Method to close the XPRT.
 * @sft_close_done: Method to indicate to the XPRT that handling of reset
 *                  event is complete.
 */
struct msm_ipc_router_xprt {
	char *name;
	uint32_t link_id;
	void *priv;

	int (*get_version)(struct msm_ipc_router_xprt *xprt);
	int (*get_option)(struct msm_ipc_router_xprt *xprt);
	int (*read_avail)(struct msm_ipc_router_xprt *xprt);
	int (*read)(void *data, uint32_t len,
		    struct msm_ipc_router_xprt *xprt);
	int (*write_avail)(struct msm_ipc_router_xprt *xprt);
	int (*write)(void *data, uint32_t len,
		     struct msm_ipc_router_xprt *xprt);
	int (*close)(struct msm_ipc_router_xprt *xprt);
	void (*sft_close_done)(struct msm_ipc_router_xprt *xprt);
};

void msm_ipc_router_xprt_notify(struct msm_ipc_router_xprt *xprt,
				unsigned event,
				void *data);


struct rr_packet *clone_pkt(struct rr_packet *pkt);
void release_pkt(struct rr_packet *pkt);


struct msm_ipc_port *msm_ipc_router_create_raw_port(void *endpoint,
		void (*notify)(unsigned event, void *priv),
		void *priv);
int msm_ipc_router_send_to(struct msm_ipc_port *src,
			   struct sk_buff_head *data,
			   struct msm_ipc_addr *dest);
int msm_ipc_router_read(struct msm_ipc_port *port_ptr,
			struct rr_packet **pkt,
			size_t buf_len);
int msm_ipc_router_bind_control_port(struct msm_ipc_port *port_ptr);

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

#if defined CONFIG_MSM_IPC_ROUTER_SMD_XPRT
extern void *msm_ipc_load_default_node(void);

extern void msm_ipc_unload_default_node(void *pil);
#else
static inline void *msm_ipc_load_default_node(void)
{ return NULL; }

static inline void msm_ipc_unload_default_node(void *pil) { }
#endif

#endif
