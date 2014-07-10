 /*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

/* ***************** Constants ********************************************** */
#define RMNET_LOCAL_LOGICAL_ENDPOINT -1

#define RMNET_EGRESS_FORMAT__RESERVED__         (1<<0)
#define RMNET_EGRESS_FORMAT_MAP                 (1<<1)
#define RMNET_EGRESS_FORMAT_AGGREGATION         (1<<2)
#define RMNET_EGRESS_FORMAT_MUXING              (1<<3)

#define RMNET_INGRESS_FIX_ETHERNET              (1<<0)
#define RMNET_INGRESS_FORMAT_MAP                (1<<1)
#define RMNET_INGRESS_FORMAT_DEAGGREGATION      (1<<2)
#define RMNET_INGRESS_FORMAT_DEMUXING           (1<<3)
#define RMNET_INGRESS_FORMAT_MAP_COMMANDS       (1<<4)
#define RMNET_INGRESS_FORMAT_MAP_CKSUMV3        (1<<5)

/* ***************** Netlink API ******************************************** */
#define RMNET_NETLINK_PROTO 31
#define RMNET_MAX_STR_LEN  16
#define RMNET_NL_DATA_MAX_LEN 64

#define RMNET_NETLINK_MSG_COMMAND    0
#define RMNET_NETLINK_MSG_RETURNCODE 1
#define RMNET_NETLINK_MSG_RETURNDATA 2

struct rmnet_nl_msg_s {
	uint16_t reserved;
	uint16_t message_type;
	uint16_t reserved2:14;
	uint16_t crd:2;
	union {
		uint16_t arg_length;
		uint16_t return_code;
	};
	union {
		uint8_t data[RMNET_NL_DATA_MAX_LEN];
		struct {
			uint8_t  dev[RMNET_MAX_STR_LEN];
			uint32_t flags;
			uint16_t agg_size;
			uint16_t agg_count;
			uint8_t  tail_spacing;
		} data_format;
		struct {
			uint8_t dev[RMNET_MAX_STR_LEN];
			int32_t ep_id;
			uint8_t operating_mode;
			uint8_t next_dev[RMNET_MAX_STR_LEN];
		} local_ep_config;
		struct {
			uint32_t id;
			uint8_t  vnd_name[RMNET_MAX_STR_LEN];
		} vnd;
		struct {
			uint32_t id;
			uint32_t map_flow_id;
			uint32_t tc_flow_id;
		} flow_control;
	};
};

enum rmnet_netlink_message_types_e {
	/*
	 * RMNET_NETLINK_ASSOCIATE_NETWORK_DEVICE - Register RMNET data driver
	 *                                          on a particular device.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_ASSOCIATE_NETWORK_DEVICE,

	/*
	 * RMNET_NETLINK_UNASSOCIATE_NETWORK_DEVICE - Unregister RMNET data
	 *                                            driver on a particular
	 *                                            device.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_UNASSOCIATE_NETWORK_DEVICE,

	/*
	 * RMNET_NETLINK_GET_NETWORK_DEVICE_ASSOCIATED - Get if RMNET data
	 *                                            driver is registered on a
	 *                                            particular device.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: 1 if registered, 0 if not
	 */
	RMNET_NETLINK_GET_NETWORK_DEVICE_ASSOCIATED,

	/*
	 * RMNET_NETLINK_SET_LINK_EGRESS_DATA_FORMAT - Sets the egress data
	 *                                             format for a particular
	 *                                             link.
	 * Args: uint32_t egress_flags
	 *       char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_SET_LINK_EGRESS_DATA_FORMAT,

	/*
	 * RMNET_NETLINK_GET_LINK_EGRESS_DATA_FORMAT - Gets the egress data
	 *                                             format for a particular
	 *                                             link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: 4-bytes data: uint32_t egress_flags
	 */
	RMNET_NETLINK_GET_LINK_EGRESS_DATA_FORMAT,

	/*
	 * RMNET_NETLINK_SET_LINK_INGRESS_DATA_FORMAT - Sets the ingress data
	 *                                              format for a particular
	 *                                              link.
	 * Args: uint32_t ingress_flags
	 *       char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: status code
	 */
	RMNET_NETLINK_SET_LINK_INGRESS_DATA_FORMAT,

	/*
	 * RMNET_NETLINK_GET_LINK_INGRESS_DATA_FORMAT - Gets the ingress data
	 *                                              format for a particular
	 *                                              link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 * Returns: 4-bytes data: uint32_t ingress_flags
	 */
	RMNET_NETLINK_GET_LINK_INGRESS_DATA_FORMAT,

	/*
	 * RMNET_NETLINK_SET_LOGICAL_EP_CONFIG - Sets the logical endpoint
	 *                                       configuration for a particular
	 *                                       link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 *     int32_t logical_ep_id, valid values are -1 through 31
	 *     uint8_t rmnet_mode: one of none, vnd, bridged
	 *     char[] egress_dev_name: Egress device if operating in bridge mode
	 * Returns: status code
	 */
	RMNET_NETLINK_SET_LOGICAL_EP_CONFIG,

	/*
	 * RMNET_NETLINK_UNSET_LOGICAL_EP_CONFIG - Un-sets the logical endpoint
	 *                                       configuration for a particular
	 *                                       link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 *       int32_t logical_ep_id, valid values are -1 through 31
	 * Returns: status code
	 */
	RMNET_NETLINK_UNSET_LOGICAL_EP_CONFIG,

	/*
	 * RMNET_NETLINK_GET_LOGICAL_EP_CONFIG - Gets the logical endpoint
	 *                                       configuration for a particular
	 *                                       link.
	 * Args: char[] dev_name: Null terminated ASCII string, max length: 15
	 *        int32_t logical_ep_id, valid values are -1 through 31
	 * Returns: uint8_t rmnet_mode: one of none, vnd, bridged
	 * char[] egress_dev_name: Egress device
	 */
	RMNET_NETLINK_GET_LOGICAL_EP_CONFIG,

	/*
	 * RMNET_NETLINK_NEW_VND - Creates a new virtual network device node
	 * Args: int32_t node number
	 * Returns: status code
	 */
	RMNET_NETLINK_NEW_VND,

	/*
	 * RMNET_NETLINK_NEW_VND_WITH_PREFIX - Creates a new virtual network
	 *                                     device node with the specified
	 *                                     prefix for the device name
	 * Args: int32_t node number
	 *       char[] vnd_name - Use as prefix
	 * Returns: status code
	 */
	RMNET_NETLINK_NEW_VND_WITH_PREFIX,

	/*
	 * RMNET_NETLINK_GET_VND_NAME - Gets the string name of a VND from ID
	 * Args: int32_t node number
	 * Returns: char[] vnd_name
	 */
	RMNET_NETLINK_GET_VND_NAME,

	/*
	 * RMNET_NETLINK_FREE_VND - Removes virtual network device node
	 * Args: int32_t node number
	 * Returns: status code
	 */
	RMNET_NETLINK_FREE_VND,

	/*
	 * RMNET_NETLINK_ADD_VND_TC_FLOW - Add flow control handle on VND
	 * Args: int32_t node number
	 *       uint32_t MAP Flow Handle
	 *       uint32_t TC Flow Handle
	 * Returns: status code
	 */
	RMNET_NETLINK_ADD_VND_TC_FLOW,

	/*
	 * RMNET_NETLINK_DEL_VND_TC_FLOW - Removes flow control handle on VND
	 * Args: int32_t node number
	 *       uint32_t MAP Flow Handle
	 * Returns: status code
	 */
	RMNET_NETLINK_DEL_VND_TC_FLOW
};

enum rmnet_config_endpoint_modes_e {
	RMNET_EPMODE_NONE,
	RMNET_EPMODE_VND,
	RMNET_EPMODE_BRIDGE,
	RMNET_EPMODE_LENGTH /* Must be the last item in the list */
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
