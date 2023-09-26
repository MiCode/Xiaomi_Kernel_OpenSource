/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MDDP_F_DEV_H
#define _MDDP_F_DEV_H

#include <linux/if.h>

#define MDDP_MAX_LAN_DEV_NUM 2
#define MDDP_MAX_WAN_DEV_NUM 1

#define MDDP_WAN_DEV_NETIF_ID_BASE 0x00000400 /* IPC_NETIF_ID_LHIF_BEGIN */

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
struct mddp_f_dev_netif {
	char dev_name[IFNAMSIZ];
	struct net_device *netdev;
	int ifindex;
	int netif_id;
	int is_valid;
};

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int mddp_f_dev_to_netif_id(struct net_device *netdev);
void mddp_f_wan_netdev_set(struct net_device *netdev);

#endif /* _MDDP_F_DEV_H */
