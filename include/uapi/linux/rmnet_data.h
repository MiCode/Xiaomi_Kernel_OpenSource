/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _RMNET_DATA_H_
#define _RMNET_DATA_H_

#include <linux/types.h>

/* Constants */
#define RMNET_LOCAL_LOGICAL_ENDPOINT -1

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

/* Netlink API */
#define RMNET_NETLINK_PROTO 31
#define RMNET_MAX_STR_LEN  16
#define RMNET_NL_DATA_MAX_LEN 64

#define RMNET_NETLINK_MSG_COMMAND    0
#define RMNET_NETLINK_MSG_RETURNCODE 1
#define RMNET_NETLINK_MSG_RETURNDATA 2

struct rmnet_nl_msg_s {
	__u16 reserved;
	__u16 message_type;
	__u16 reserved2:14;
	__u16 crd:2;
	union {
		__u16 arg_length;
		__u16 return_code;
	};
	union {
		__u8 data[RMNET_NL_DATA_MAX_LEN];
		struct {
			__u8  dev[RMNET_MAX_STR_LEN];
			__u32 flags;
			__u16 agg_size;
			__u16 agg_count;
			__u8  tail_spacing;
		} data_format;
		struct {
			__u8 dev[RMNET_MAX_STR_LEN];
			__s32 ep_id;
			__u8 operating_mode;
			__u8 next_dev[RMNET_MAX_STR_LEN];
		} local_ep_config;
		struct {
			__u32 id;
			__u8  vnd_name[RMNET_MAX_STR_LEN];
		} vnd;
		struct {
			__u32 id;
			__u32 map_flow_id;
			__u32 tc_flow_id;
		} flow_control;
	};
};

enum rmnet_netlink_message_types_e {
	/* RMNET_NETLINK_ASSOCIATE_NETWORK_DEVICE - Register RMNET data driver
	 *                                          on a particular device.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_ASSOCIATE_NETWORK_DEVICE,

	/* RMNET_NETLINK_UNASSOCIATE_NETWORK_DEVICE - Unregister RMNET data
	 *                                            driver on a particular
	 *                                            device.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_UNASSOCIATE_NETWORK_DEVICE,

	/* RMNET_NETLINK_GET_NETWORK_DEVICE_ASSOCIATED - Get if RMNET data
	 *                                            driver is registered on a
	 *                                            particular device.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: 1 if registered, 0 if not
	 */
	RMNET_NETLINK_GET_NETWORK_DEVICE_ASSOCIATED,

	/* RMNET_NETLINK_SET_LINK_EGRESS_DATA_FORMAT - Sets the egress data
	 *                                             format for a particular
	 *                                             link.
	 * Args: __u32 egress_flags
	 *       char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_SET_LINK_EGRESS_DATA_FORMAT,

	/* RMNET_NETLINK_GET_LINK_EGRESS_DATA_FORMAT - Gets the egress data
	 *                                             format for a particular
	 *                                             link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: 4-bytes data: __u32 egress_flags
	 */
	RMNET_NETLINK_GET_LINK_EGRESS_DATA_FORMAT,

	/* RMNET_NETLINK_SET_LINK_INGRESS_DATA_FORMAT - Sets the ingress data
	 *                                              format for a particular
	 *                                              link.
	 * Args: __u32 ingress_flags
	 *       char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_SET_LINK_INGRESS_DATA_FORMAT,

	/* RMNET_NETLINK_GET_LINK_INGRESS_DATA_FORMAT - Gets the ingress data
	 *                                              format for a particular
	 *                                              link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: 4-bytes data: __u32 ingress_flags
	 */
	RMNET_NETLINK_GET_LINK_INGRESS_DATA_FORMAT,

	/* RMNET_NETLINK_SET_LOGICAL_EP_CONFIG - Sets the logical endpoint
	 *                                       configuration for a particular
	 *                                       link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 *     __s32 logical_ep_id, valid values are -1 through 31
	 *     __u8 rmnet_mode: one of none, vnd, bridged
	 *     char[] egress_dev_name: Egress device if operating in bridge mode
	 * Returns: status code
	 */
	RMNET_NETLINK_SET_LOGICAL_EP_CONFIG,

	/* RMNET_NETLINK_UNSET_LOGICAL_EP_CONFIG - Un-sets the logical endpoint
	 *                                       configuration for a particular
	 *                                       link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 *       __s32 logical_ep_id, valid values are -1 through 31
	 * Returns: status code
	 */
	RMNET_NETLINK_UNSET_LOGICAL_EP_CONFIG,

	/* RMNET_NETLINK_GET_LOGICAL_EP_CONFIG - Gets the logical endpoint
	 *                                       configuration for a particular
	 *                                       link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 *        __s32 logical_ep_id, valid values are -1 through 31
	 * Returns: __u8 rmnet_mode: one of none, vnd, bridged
	 * char[] egress_dev_name: Egress device
	 */
	RMNET_NETLINK_GET_LOGICAL_EP_CONFIG,

	/* RMNET_NETLINK_NEW_VND - Creates a new virtual network device node
	 * Args: __s32 node number
	 * Returns: status code
	 */
	RMNET_NETLINK_NEW_VND,

	/* RMNET_NETLINK_NEW_VND_WITH_PREFIX - Creates a new virtual network
	 *                                     device node with the specified
	 *                                     prefix for the device name
	 * Args: __s32 node number
	 *       char[] vnd_name - Use as prefix
	 * Returns: status code
	 */
	RMNET_NETLINK_NEW_VND_WITH_PREFIX,

	/* RMNET_NETLINK_GET_VND_NAME - Gets the string name of a VND from ID
	 * Args: __s32 node number
	 * Returns: char[] vnd_name
	 */
	RMNET_NETLINK_GET_VND_NAME,

	/* RMNET_NETLINK_FREE_VND - Removes virtual network device node
	 * Args: __s32 node number
	 * Returns: status code
	 */
	RMNET_NETLINK_FREE_VND,

	/* RMNET_NETLINK_ADD_VND_TC_FLOW - Add flow control handle on VND
	 * Args: __s32 node number
	 *       __u32 MAP Flow Handle
	 *       __u32 TC Flow Handle
	 * Returns: status code
	 */
	RMNET_NETLINK_ADD_VND_TC_FLOW,

	/* RMNET_NETLINK_DEL_VND_TC_FLOW - Removes flow control handle on VND
	 * Args: __s32 node number
	 *       __u32 MAP Flow Handle
	 * Returns: status code
	 */
	RMNET_NETLINK_DEL_VND_TC_FLOW,

	/*
	 * RMNET_NETLINK_NEW_VND_WITH_NAME - Creates a new virtual network
	 *                                   device node with the specified
	 *                                   device name
	 * Args: __s32 node number
	 *       char[] vnd_name - Use as name
	 * Returns: status code
	 */
	RMNET_NETLINK_NEW_VND_WITH_NAME
};
#define RMNET_NETLINK_NEW_VND_WITH_NAME RMNET_NETLINK_NEW_VND_WITH_NAME

enum rmnet_config_endpoint_modes_e {
	/* Pass the frame up the stack with no modifications to skb->dev      */
	RMNET_EPMODE_NONE,
	/* Replace skb->dev to a virtual rmnet device and pass up the stack   */
	RMNET_EPMODE_VND,
	/* Pass the frame directly to another device with dev_queue_xmit().   */
	RMNET_EPMODE_BRIDGE,
	/* Must be the last item in the list                                  */
	RMNET_EPMODE_LENGTH
};

enum rmnet_config_return_codes_e {
	RMNET_CONFIG_OK,
	RMNET_CONFIG_UNKNOWN_MESSAGE,
	RMNET_CONFIG_UNKNOWN_ERROR,
	RMNET_CONFIG_NOMEM,
	RMNET_CONFIG_DEVICE_IN_USE,
	RMNET_CONFIG_INVALID_REQUEST,
	RMNET_CONFIG_NO_SUCH_DEVICE,
	RMNET_CONFIG_BAD_ARGUMENTS,
	RMNET_CONFIG_BAD_EGRESS_DEVICE,
	RMNET_CONFIG_TC_HANDLE_FULL
};

#endif /* _RMNET_DATA_H_ */
