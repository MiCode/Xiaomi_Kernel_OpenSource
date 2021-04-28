/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _RMNET_IPA_FD_IOCTL_H
#define _RMNET_IPA_FD_IOCTL_H

#include <linux/ioctl.h>
#include <linux/ipa_qmi_service_v01.h>
#include <linux/msm_ipa.h>

/**
 * unique magic number of the IPA_WAN device
 */
#define WAN_IOC_MAGIC 0x69

#define WAN_IOCTL_ADD_FLT_RULE		0
#define WAN_IOCTL_ADD_FLT_INDEX		1
#define WAN_IOCTL_VOTE_FOR_BW_MBPS	2
#define WAN_IOCTL_POLL_TETHERING_STATS  3
#define WAN_IOCTL_SET_DATA_QUOTA        4
#define WAN_IOCTL_SET_TETHER_CLIENT_PIPE 5
#define WAN_IOCTL_QUERY_TETHER_STATS     6
#define WAN_IOCTL_RESET_TETHER_STATS     7
#define WAN_IOCTL_QUERY_DL_FILTER_STATS  8
#define WAN_IOCTL_ADD_FLT_RULE_EX        9
#define WAN_IOCTL_QUERY_TETHER_STATS_ALL  10
#define WAN_IOCTL_NOTIFY_WAN_STATE  11
#define WAN_IOCTL_ADD_UL_FLT_RULE          12
#define WAN_IOCTL_ENABLE_PER_CLIENT_STATS    13
#define WAN_IOCTL_QUERY_PER_CLIENT_STATS     14
#define WAN_IOCTL_SET_LAN_CLIENT_INFO        15
#define WAN_IOCTL_CLEAR_LAN_CLIENT_INFO      16
#define WAN_IOCTL_SEND_LAN_CLIENT_MSG        17
#define WAN_IOCTL_ADD_OFFLOAD_CONNECTION     18
#define WAN_IOCTL_RMV_OFFLOAD_CONNECTION     19
#define WAN_IOCTL_GET_WAN_MTU                20
#define WAN_IOCTL_NOTIFY_NAT_MOVE_RES        21

/* User space may not have this defined. */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

/**
 * struct wan_ioctl_poll_tethering_stats - structure used for
 *                                         WAN_IOCTL_POLL_TETHERING_STATS IOCTL.
 *
 * @polling_interval_secs: Polling interval in seconds.
 * @reset_stats:           Indicate whether to reset the stats (use 1) or not.
 *
 * The structure to be used by the user space in order to request for the
 * tethering stats to be polled. Setting the interval to 0 indicates to stop
 * the polling process.
 */
struct wan_ioctl_poll_tethering_stats {
	uint64_t polling_interval_secs;
	uint8_t  reset_stats;
};

/**
 * struct wan_ioctl_set_data_quota - structure used for
 *                                   WAN_IOCTL_SET_DATA_QUOTA IOCTL.
 *
 * @interface_name:  Name of the interface on which to set the quota.
 * @quota_mbytes:    Quota (in Mbytes) for the above interface.
 * @set_quota:       Indicate whether to set the quota (use 1) or
 *                   unset the quota.
 *
 * The structure to be used by the user space in order to request
 * a quota to be set on a specific interface (by specifying its name).
 */
struct wan_ioctl_set_data_quota {
	char     interface_name[IFNAMSIZ];
	uint64_t quota_mbytes;
	uint8_t  set_quota;
};

struct wan_ioctl_set_tether_client_pipe {
	/* enum of tether interface */
	enum ipacm_client_enum ipa_client;
	uint8_t reset_client;
	uint32_t ul_src_pipe_len;
	uint32_t ul_src_pipe_list[QMI_IPA_MAX_PIPES_V01];
	uint32_t dl_dst_pipe_len;
	uint32_t dl_dst_pipe_list[QMI_IPA_MAX_PIPES_V01];
};

struct wan_ioctl_query_tether_stats {
	/* Name of the upstream interface */
	char upstreamIface[IFNAMSIZ];
	/* Name of the tethered interface */
	char tetherIface[IFNAMSIZ];
	/* enum of tether interface */
	enum ipacm_client_enum ipa_client;
	uint64_t ipv4_tx_packets;
	uint64_t ipv4_tx_bytes;
	uint64_t ipv4_rx_packets;
	uint64_t ipv4_rx_bytes;
	uint64_t ipv6_tx_packets;
	uint64_t ipv6_tx_bytes;
	uint64_t ipv6_rx_packets;
	uint64_t ipv6_rx_bytes;
};

struct wan_ioctl_query_tether_stats_all {
	/* Name of the upstream interface */
	char upstreamIface[IFNAMSIZ];
	/* enum of tether interface */
	enum ipacm_client_enum ipa_client;
	uint8_t reset_stats;
	uint64_t tx_bytes;
	uint64_t rx_bytes;
};

struct wan_ioctl_reset_tether_stats {
	/* Name of the upstream interface, not support now */
	char upstreamIface[IFNAMSIZ];
	/* Indicate whether to reset the stats (use 1) or not */
	uint8_t reset_stats;
};

struct wan_ioctl_query_dl_filter_stats {
	/* Indicate whether to reset the filter stats (use 1) or not*/
	uint8_t reset_stats;
	/* Modem response QMI */
	struct ipa_get_data_stats_resp_msg_v01 stats_resp;
	/* provide right index to 1st firewall rule */
	uint32_t index;
};

struct wan_ioctl_notify_wan_state {
	uint8_t up;
	/* Name of the upstream interface */
	char upstreamIface[IFNAMSIZ];
#define WAN_IOCTL_NOTIFY_WAN_INTF_NAME WAN_IOCTL_NOTIFY_WAN_INTF_NAME
};
struct wan_ioctl_send_lan_client_msg {
	/* Lan client info. */
	struct ipa_lan_client_msg lan_client;
	/* Event to indicate whether client is
	 * connected or disconnected.
	 */
	enum ipa_per_client_stats_event client_event;
};

struct wan_ioctl_lan_client_info {
	/* Device type of the client. */
	enum ipacm_per_client_device_type device_type;
	/* MAC Address of the client. */
	uint8_t mac[IPA_MAC_ADDR_SIZE];
	/* Init client. */
	uint8_t client_init;
	/* Client Index */
	int8_t client_idx;
	/* Header length of the client. */
	uint8_t hdr_len;
	/* Source pipe of the lan client. */
	enum ipa_client_type ul_src_pipe;
	/* Counter indices for h/w fnr stats */
#define IPA_HW_FNR_STATS
	uint8_t ul_cnt_idx;
	uint8_t dl_cnt_idx;
};

struct wan_ioctl_per_client_info {
	/* MAC Address of the client. */
	uint8_t mac[IPA_MAC_ADDR_SIZE];
	/* Ipv4 UL traffic bytes. */
	uint64_t ipv4_tx_bytes;
	/* Ipv4 DL traffic bytes. */
	uint64_t ipv4_rx_bytes;
	/* Ipv6 UL traffic bytes. */
	uint64_t ipv6_tx_bytes;
	/* Ipv6 DL traffic bytes. */
	uint64_t ipv6_rx_bytes;
};

struct wan_ioctl_query_per_client_stats {
	/* Device type of the client. */
	enum ipacm_per_client_device_type device_type;
	/* Indicate whether to reset the stats (use 1) or not */
	uint8_t reset_stats;
	/* Indicates whether client is disconnected. */
	uint8_t disconnect_clnt;
	/* Number of clients. */
	uint8_t num_clients;
	/* Client information. */
	struct wan_ioctl_per_client_info
		client_info[IPA_MAX_NUM_HW_PATH_CLIENTS];
};

#define WAN_IOC_ADD_FLT_RULE _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_RULE, \
		struct ipa_install_fltr_rule_req_msg_v01 *)

#define WAN_IOC_ADD_FLT_RULE_INDEX _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_INDEX, \
		struct ipa_fltr_installed_notif_req_msg_v01 *)

#define WAN_IOC_VOTE_FOR_BW_MBPS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_VOTE_FOR_BW_MBPS, \
		uint32_t *)

#define WAN_IOC_POLL_TETHERING_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_POLL_TETHERING_STATS, \
		struct wan_ioctl_poll_tethering_stats *)

#define WAN_IOC_SET_DATA_QUOTA _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_SET_DATA_QUOTA, \
		struct wan_ioctl_set_data_quota *)

#define WAN_IOC_SET_TETHER_CLIENT_PIPE _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_SET_TETHER_CLIENT_PIPE, \
		struct wan_ioctl_set_tether_client_pipe *)

#define WAN_IOC_QUERY_TETHER_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_TETHER_STATS, \
		struct wan_ioctl_query_tether_stats *)

#define WAN_IOC_RESET_TETHER_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_RESET_TETHER_STATS, \
		struct wan_ioctl_reset_tether_stats *)

#define WAN_IOC_QUERY_DL_FILTER_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_DL_FILTER_STATS, \
		struct wan_ioctl_query_dl_filter_stats *)

#define WAN_IOC_ADD_FLT_RULE_EX _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_RULE_EX, \
		struct ipa_install_fltr_rule_req_ex_msg_v01 *)

#define WAN_IOC_QUERY_TETHER_STATS_ALL _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_TETHER_STATS_ALL, \
		struct wan_ioctl_query_tether_stats_all *)

#define WAN_IOC_NOTIFY_WAN_STATE _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_NOTIFY_WAN_STATE, \
		struct wan_ioctl_notify_wan_state *)

#define WAN_IOC_ADD_UL_FLT_RULE _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_UL_FLT_RULE, \
		struct ipa_configure_ul_firewall_rules_req_msg_v01 *)

#define WAN_IOC_ENABLE_PER_CLIENT_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ENABLE_PER_CLIENT_STATS, \
		bool *)

#define WAN_IOC_QUERY_PER_CLIENT_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_PER_CLIENT_STATS, \
		struct wan_ioctl_query_per_client_stats *)

#define WAN_IOC_SET_LAN_CLIENT_INFO _IOWR(WAN_IOC_MAGIC, \
			WAN_IOCTL_SET_LAN_CLIENT_INFO, \
			struct wan_ioctl_lan_client_info *)

#define WAN_IOC_SEND_LAN_CLIENT_MSG _IOWR(WAN_IOC_MAGIC, \
				WAN_IOCTL_SEND_LAN_CLIENT_MSG, \
				struct wan_ioctl_send_lan_client_msg *)

#define WAN_IOC_CLEAR_LAN_CLIENT_INFO _IOWR(WAN_IOC_MAGIC, \
			WAN_IOCTL_CLEAR_LAN_CLIENT_INFO, \
			struct wan_ioctl_lan_client_info *)

#define WAN_IOC_ADD_OFFLOAD_CONNECTION _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_OFFLOAD_CONNECTION, \
		struct ipa_add_offload_connection_req_msg_v01 *)

#define WAN_IOC_RMV_OFFLOAD_CONNECTION _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_RMV_OFFLOAD_CONNECTION, \
		struct ipa_remove_offload_connection_req_msg_v01 *)

#define WAN_IOC_GET_WAN_MTU _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_GET_WAN_MTU, \
		struct ipa_mtu_info *)

#define WAN_IOC_NOTIFY_NAT_MOVE_RES _IOWR(WAN_IOC_MAGIC, \
	WAN_IOCTL_NOTIFY_NAT_MOVE_RES, \
	bool)
#endif /* _RMNET_IPA_FD_IOCTL_H */
