/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_filter.h - Public API/structure provided by filter.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_FILTER_H
#define __MDDP_FILTER_H

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_filter_init(void);
void mddp_filter_uninit(void);

int mddp_f_data_usage_wan_dev_name_to_id(char *dev_name);
bool mddp_f_dev_add_lan_dev(char *dev_name, int netif_id);
bool mddp_f_dev_add_wan_dev(char *dev_name);
void mddp_f_dev_del_lan_dev(char *dev_name);
void mddp_f_dev_del_wan_dev(char *dev_name);
struct net_device *mddp_f_is_support_lan_dev(int ifindex);
struct net_device *mddp_f_is_support_wan_dev(int ifindex);

int32_t mddp_f_msg_hdlr(uint32_t msg_id, void *buf, uint32_t buf_len);
int32_t mddp_f_set_ct_value(uint8_t *buf, uint32_t buf_len);
void mddp_netfilter_hook(void);
void mddp_netfilter_unhook(void);

#endif /* __MDDP_FILTER_H */
