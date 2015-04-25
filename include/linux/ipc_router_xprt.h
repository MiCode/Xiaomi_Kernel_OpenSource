/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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

#ifndef _IPC_ROUTER_XPRT_H
#define _IPC_ROUTER_XPRT_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/msm_ipc.h>
#include <linux/ipc_router.h>
#include <linux/kref.h>

#define IPC_ROUTER_XPRT_EVENT_DATA  1
#define IPC_ROUTER_XPRT_EVENT_OPEN  2
#define IPC_ROUTER_XPRT_EVENT_CLOSE 3

#define FRAG_PKT_WRITE_ENABLE 0x1

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
 * @opt_len: Combined size of the all optional headers in units of words.
 * @size: Size of the IPC Router payload.
 * @src_node_id: Source Node ID of the message.
 * @src_port_id: Source Port ID of the message.
 * @dst_node_id: Destination Node ID of the message.
 * @dst_port_id: Destination Port ID of the message.
 */
struct rr_header_v2 {
	uint8_t version;
	uint8_t type;
	uint8_t control_flag;
	uint8_t opt_len;
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

/**
 * rr_opt_hdr - Optional header for IPC Router header version 2
 * @len: Total length of the optional header.
 * @data: Pointer to the actual optional header.
 */
struct rr_opt_hdr {
	size_t len;
	unsigned char *data;
};

#define IPC_ROUTER_HDR_SIZE sizeof(union rr_header)
#define IPCR_WORD_SIZE 4

/**
 * rr_packet - Router to Router packet structure
 * @list: Pointer to prev & next packets in a port's rx list.
 * @hdr: Header information extracted from or prepended to a packet.
 * @opt_hdr: Optinal header information.
 * @pkt_fragment_q: Queue of SKBs containing payload.
 * @length: Length of data in the chain of SKBs
 * @ref: Reference count for the packet.
 */
struct rr_packet {
	struct list_head list;
	struct rr_header_v1 hdr;
	struct rr_opt_hdr opt_hdr;
	struct sk_buff_head *pkt_fragment_q;
	uint32_t length;
	struct kref ref;
};

/**
 * msm_ipc_router_xprt - Structure to hold XPRT specific information
 * @name: Name of the XPRT.
 * @link_id: Network cluster ID to which the XPRT belongs to.
 * @priv: XPRT's private data.
 * @get_version: Method to get header version supported by the XPRT.
 * @set_version: Method to set header version in XPRT.
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
	void (*set_version)(struct msm_ipc_router_xprt *xprt,
			    unsigned version);
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

/**
 * create_pkt() - Create a Router packet
 * @data: SKB queue to be contained inside the packet.
 *
 * @return: pointer to packet on success, NULL on failure.
 */
struct rr_packet *create_pkt(struct sk_buff_head *data);
struct rr_packet *clone_pkt(struct rr_packet *pkt);
void release_pkt(struct rr_packet *pkt);

/**
 * ipc_router_peek_pkt_size() - Peek into the packet header to get potential packet size
 * @data: Starting address of the packet which points to router header.
 *
 * @returns: potential packet size on success, < 0 on error.
 *
 * This function is used by the underlying transport abstraction layer to
 * peek into the potential packet size of an incoming packet. This information
 * is used to perform link layer fragmentation and re-assembly
 */
int ipc_router_peek_pkt_size(char *data);

#endif
