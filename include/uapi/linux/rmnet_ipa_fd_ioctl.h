/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_IPA_FD_IOCTL_H
#define _RMNET_IPA_FD_IOCTL_H

#include <linux/ioctl.h>
#include <linux/ipa_qmi_service_v01.h>

/**
 * unique magic number of the IPA_WAN device
 */
#define WAN_IOC_MAGIC 0x69

/* User space may not have this defined. */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#define WAN_IOCTL_ADD_FLT_RULE		0
#define WAN_IOCTL_ADD_FLT_INDEX		2
#define WAN_IOCTL_POLL_TETHERING_STATS  3
#define WAN_IOCTL_SET_DATA_QUOTA        4

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

#define WAN_IOC_ADD_FLT_RULE _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_RULE, \
		struct ipa_install_fltr_rule_req_msg_v01 *)

#define WAN_IOC_ADD_FLT_RULE_INDEX _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_INDEX, \
		struct ipa_fltr_installed_notif_req_msg_v01 *)

#define WAN_IOC_POLL_TETHERING_STATS _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_POLL_TETHERING_STATS, \
		struct wan_ioctl_poll_tethering_stats *)

#define WAN_IOC_SET_DATA_QUOTA _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_SET_DATA_QUOTA, \
		struct wan_ioctl_set_data_quota *)

#endif /* _RMNET_IPA_FD_IOCTL_H */
