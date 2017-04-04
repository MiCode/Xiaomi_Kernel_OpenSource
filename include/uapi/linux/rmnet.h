/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * RMNET Data configuration specification
 */

#ifndef _RMNET_DATA_H_
#define _RMNET_DATA_H_

/* Netlink API */
#define RMNET_NETLINK_PROTO 31
#define RMNET_MAX_STR_LEN  16
#define RMNET_NL_DATA_MAX_LEN 64

#define RMNET_NETLINK_MSG_COMMAND    0
#define RMNET_NETLINK_MSG_RETURNCODE 1
#define RMNET_NETLINK_MSG_RETURNDATA 2

/* Constants */
#define RMNET_EGRESS_FORMAT__RESERVED__         (1<<0)
#define RMNET_EGRESS_FORMAT_MAP                 (1<<1)
#define RMNET_EGRESS_FORMAT_AGGREGATION         (1<<2)
#define RMNET_EGRESS_FORMAT_MUXING              (1<<3)
#define RMNET_EGRESS_FORMAT_MAP_CKSUMV3         (1<<4)
#define RMNET_EGRESS_FORMAT_MAP_CKSUMV4         (1<<5)

#define RMNET_INGRESS_FIX_ETHERNET              (1<<0)
#define RMNET_INGRESS_FORMAT_MAP                (1<<1)
#define RMNET_INGRESS_FORMAT_DEAGGREGATION      (1<<2)
#define RMNET_INGRESS_FORMAT_DEMUXING           (1<<3)
#define RMNET_INGRESS_FORMAT_MAP_COMMANDS       (1<<4)
#define RMNET_INGRESS_FORMAT_MAP_CKSUMV3        (1<<5)
#define RMNET_INGRESS_FORMAT_MAP_CKSUMV4        (1<<6)

struct rmnet_nl_msg_s {
	__be16 reserved;
	__be16 message_type;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u16   crd:2,
		reserved2:14;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u16   reserved2:14,
		crd:2;
#endif
	union {
		__be16 arg_length;
		__be16 return_code;
	};
	union {
		__u8 data[RMNET_NL_DATA_MAX_LEN];
		struct {
			__u8   dev[RMNET_MAX_STR_LEN];
			__be32 flags;
			__be16 agg_size;
			__be16 agg_count;
			__u8   tail_spacing;
		} data_format;
		struct {
			__u8  dev[RMNET_MAX_STR_LEN];
			__be32 ep_id;
			__u8  operating_mode;
			__u8  next_dev[RMNET_MAX_STR_LEN];
		} local_ep_config;
		struct {
			__be32 id;
			__u8   vnd_name[RMNET_MAX_STR_LEN];
		} vnd;
	};
};

/* RMNET_NETLINK_ASSOCIATE_NETWORK_DEVICE - Register RMNET data driver
 *                                          on a particular device.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: status code
 */
#define RMNET_NETLINK_ASSOCIATE_NETWORK_DEVICE 0

/* RMNET_NETLINK_UNASSOCIATE_NETWORK_DEVICE - Unregister RMNET data
 *                                            driver on a particular
 *                                            device.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: status code
 */
#define RMNET_NETLINK_UNASSOCIATE_NETWORK_DEVICE 1

/* RMNET_NETLINK_GET_NETWORK_DEVICE_ASSOCIATED - Get if RMNET data
 *                                            driver is registered on a
 *                                            particular device.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: 1 if registered, 0 if not
 */
#define RMNET_NETLINK_GET_NETWORK_DEVICE_ASSOCIATED 2

/* RMNET_NETLINK_SET_LINK_EGRESS_DATA_FORMAT - Sets the egress data
 *                                             format for a particular
 *                                             link.
 * Args: __be32 egress_flags
 *       char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: status code
 */
#define RMNET_NETLINK_SET_LINK_EGRESS_DATA_FORMAT 3

/* RMNET_NETLINK_GET_LINK_EGRESS_DATA_FORMAT - Gets the egress data
 *                                             format for a particular
 *                                             link.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: 4-bytes data: __be32 egress_flags
 */
#define RMNET_NETLINK_GET_LINK_EGRESS_DATA_FORMAT 4

/* RMNET_NETLINK_SET_LINK_INGRESS_DATA_FORMAT - Sets the ingress data
 *                                              format for a particular
 *                                              link.
 * Args: __be32 ingress_flags
 *       char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: status code
 */
#define RMNET_NETLINK_SET_LINK_INGRESS_DATA_FORMAT 5

/* RMNET_NETLINK_GET_LINK_INGRESS_DATA_FORMAT - Gets the ingress data
 *                                              format for a particular
 *                                              link.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 * Returns: 4-bytes data: __be32 ingress_flags
 */
#define RMNET_NETLINK_GET_LINK_INGRESS_DATA_FORMAT 6

/* RMNET_NETLINK_SET_LOGICAL_EP_CONFIG - Sets the logical endpoint
 *                                       configuration for a particular
 *                                       link.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 *     __be32 logical_ep_id, valid values are -1 through 31
 *     __u8 rmnet_mode: one of none, vnd, bridged
 *     char[] egress_dev_name: Egress device if operating in bridge mode
 * Returns: status code
 */
#define RMNET_NETLINK_SET_LOGICAL_EP_CONFIG 7

/* RMNET_NETLINK_UNSET_LOGICAL_EP_CONFIG - Un-sets the logical endpoint
 *                                       configuration for a particular
 *                                       link.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 *       __be32 logical_ep_id, valid values are -1 through 31
 * Returns: status code
 */
#define RMNET_NETLINK_UNSET_LOGICAL_EP_CONFIG 8

/* RMNET_NETLINK_GET_LOGICAL_EP_CONFIG - Gets the logical endpoint
 *                                       configuration for a particular
 *                                       link.
 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
 *        __be32 logical_ep_id, valid values are -1 through 31
 * Returns: __u8 rmnet_mode: one of none, vnd, bridged
 * char[] egress_dev_name: Egress device
 */
#define RMNET_NETLINK_GET_LOGICAL_EP_CONFIG 9

/* RMNET_NETLINK_NEW_VND - Creates a new virtual network device node
 * Args: __be32 node number
 * Returns: status code
 */
#define RMNET_NETLINK_NEW_VND 10

/* RMNET_NETLINK_NEW_VND_WITH_PREFIX - Creates a new virtual network
 *                                     device node with the specified
 *                                     prefix for the device name
 * Args: __be32 node number
 *       char[] vnd_name - Use as prefix
 * Returns: status code
 */
#define RMNET_NETLINK_NEW_VND_WITH_PREFIX 11

/* RMNET_NETLINK_GET_VND_NAME - Gets the string name of a VND from ID
 * Args: __be32 node number
 * Returns: char[] vnd_name
 */
#define RMNET_NETLINK_GET_VND_NAME 12

/* RMNET_NETLINK_FREE_VND - Removes virtual network device node
 * Args: __be32 node number
 * Returns: status code
 */
#define RMNET_NETLINK_FREE_VND 13

/* Pass the frame up the stack with no modifications to skb->dev */
#define RMNET_EPMODE_NONE 0
/* Replace skb->dev to a virtual rmnet device and pass up the stack */
#define RMNET_EPMODE_VND 1
/* Pass the frame directly to another device with dev_queue_xmit(). */
#define RMNET_EPMODE_BRIDGE 2
/* Must be the last item in the list */
#define RMNET_EPMODE_LENGTH 3

#define RMNET_CONFIG_OK 0
#define RMNET_CONFIG_UNKNOWN_MESSAGE 1
#define RMNET_CONFIG_UNKNOWN_ERROR 2
#define RMNET_CONFIG_NOMEM 3
#define RMNET_CONFIG_DEVICE_IN_USE 4
#define RMNET_CONFIG_INVALID_REQUEST 5
#define RMNET_CONFIG_NO_SUCH_DEVICE 6
#define RMNET_CONFIG_BAD_ARGUMENTS 7
#define RMNET_CONFIG_BAD_EGRESS_DEVICE 8
#define RMNET_CONFIG_TC_HANDLE_FULL 9

#endif /* _RMNET_DATA_H_ */
