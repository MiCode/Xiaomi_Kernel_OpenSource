/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_filter.h - Public API/structure provided by filter.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_FILTER_H
#define __MDDP_FILTER_H

#ifdef CONFIG_MTK_MDDP_WH_SUPPORT

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_filter_init(void);
void mddp_filter_uninit(void);

int mddp_f_data_usage_wan_dev_name_to_id(char *dev_name);
void mddp_f_dev_add_lan_dev(char *dev_name, int netif_id);
void mddp_f_dev_add_wan_dev(char *dev_name);
void mddp_f_dev_del_lan_dev(char *dev_name);
void mddp_f_dev_del_wan_dev(char *dev_name);

int32_t mddp_f_suspend_tag(void);
int32_t mddp_f_resume_tag(void);
int32_t mddp_f_msg_hdlr(uint32_t msg_id, void *buf, uint32_t buf_len);
int32_t mddp_f_set_ct_value(uint8_t *buf, uint32_t buf_len);
void mddp_netfilter_hook(void);
void mddp_netfilter_unhook(void);

#else

#define mddp_filter_init() 0
#define mddp_filter_uninit()
#define mddp_f_data_usage_wan_dev_name_to_id(x) 0
#define mddp_f_dev_add_lan_dev(x, y)
#define mddp_f_dev_add_wan_dev(x)
#define mddp_f_dev_del_lan_dev(x)
#define mddp_f_dev_del_wan_dev(x)

#define mddp_f_suspend_tag() 0
#define mddp_f_resume_tag() 0
#define mddp_f_msg_hdlr() 0
#define mddp_f_set_ct_value(x, y) 0
#define mddp_netfilter_hook() 0
#define mddp_netfilter_unhook() 0

#endif /* CONFIG_MTK_MDDP_WH_SUPPORT */
#endif /* __MDDP_FILTER_H */
