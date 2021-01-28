/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_filter.h - Public API/structure provided by filter.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_FILTER_H
#define __MDDP_FILTER_H

#if defined(MDDP_TETHERING_SUPPORT)

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

int32_t mddp_f_suspend_tag(void);
int32_t mddp_f_resume_tag(void);

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

#endif /* MDDP_TETHERING_SUPPORT */
#endif /* __MDDP_FILTER_H */
