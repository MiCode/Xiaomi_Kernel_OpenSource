/*
 * mddp_filter.h - Public API/structure provided by filter.
 *
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

#ifndef __MDDP_FILTER_H
#define __MDDP_FILTER_H

#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT) || \
defined(CONFIG_MTK_MDDP_WH_SUPPORT)

#include <linux/netdevice.h>

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_filter_init(void);
void mddp_filter_uninit(void);

int mddp_f_in_nf(int iface, struct sk_buff *skb);
void mddp_f_out_nf(int iface,
		struct sk_buff *skb, const struct net_device *out);

const char *mddp_f_data_usage_id_to_dev_name(int id);
int mddp_f_data_usage_wan_dev_name_to_id(char *dev_name);
bool mddp_f_dev_add_lan_dev(char *dev_name, int netif_id);
bool mddp_f_dev_add_wan_dev(char *dev_name);
bool mddp_f_dev_del_lan_dev(char *dev_name);
bool mddp_f_dev_del_wan_dev(char *dev_name);

#else

#define mddp_filter_init() 0
#define mddp_filter_uninit()
#define mddp_f_in_nf(x, y)
#define mddp_f_out_nf(x, y, z)
#define mddp_f_data_usage_id_to_dev_name(x) ""
#define mddp_f_data_usage_wan_dev_name_to_id(x) 0
#define mddp_f_dev_add_lan_dev(x, y)
#define mddp_f_dev_add_wan_dev(x)
#define mddp_f_dev_del_lan_dev(x)
#define mddp_f_dev_del_wan_dev(x)

#endif /* MTK_MD_DIRECT_TETHERING_SUPPORT or MTK_MDDP_WH_SUPPORT */
#endif /* __MDDP_FILTER_H */
