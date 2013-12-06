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

#define WAN_IOCTL_ADD_FLT_RULE		0
#define WAN_IOCTL_ADD_FLT_INDEX		2

#define WAN_IOC_ADD_FLT_RULE _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_RULE, \
		struct ipa_install_fltr_rule_req_msg_v01 *)

#define WAN_IOC_ADD_FLT_RULE_INDEX _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_INDEX, \
		struct ipa_fltr_installed_notif_req_msg_v01 *)

#endif /* _RMNET_IPA_FD_IOCTL_H */
