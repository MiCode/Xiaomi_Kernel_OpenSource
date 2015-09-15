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

/* User space may not have this defined. */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

struct wan_ioctl_poll_tethering_stats {
	/* Polling interval in seconds */
	uint64_t polling_interval_secs;

	/* Indicate whether to reset the stats (use 1) or not */
	uint8_t reset_stats;
};

struct wan_ioctl_set_data_quota {
	/* Name of the interface on which to set the quota */
	char interface_name[IFNAMSIZ];

	/* Quota (in Mbytes) for the above interface */
	uint64_t quota_mbytes;

	/* Indicate whether to set the quota (use 1) or unset the quota */
	uint8_t set_quota;
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

#endif /* _RMNET_IPA_FD_IOCTL_H */
