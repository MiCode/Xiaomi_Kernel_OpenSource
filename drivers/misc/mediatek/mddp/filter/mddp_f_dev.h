/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MDDP_F_DEV_H
#define _MDDP_F_DEV_H


#define MDDP_MAX_LAN_DEV_NUM 32
#define MDDP_MAX_WAN_DEV_NUM 8

#define MDDP_USB_NETIF_ID 0x00000100 /* IPC_NETIF_ID_ETH_BEGIN */
#define MDDP_WAN_DEV_NETIF_ID_BASE 0x00000400 /* IPC_NETIF_ID_LHIF_BEGIN */

#define MDDP_USB_BRIDGE_IF_NAME "ccmni-lan"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
struct mddp_f_dev_netif {
	char dev_name[IFNAMSIZ];
	int netif_id;
	int is_valid;
};

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
bool mddp_f_is_support_dev(char *dev_name);
bool mddp_f_is_support_lan_dev(char *dev_name);
bool mddp_f_is_support_wan_dev(char *dev_name);
int mddp_f_dev_name_to_id(char *dev_name);
int mddp_f_dev_name_to_netif_id(char *dev_name);

#endif /* _MDDP_F_DEV_H */
