/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __DVFSRC_EXP_H
#define __DVFSRC_EXP_H

#define MTK_DVFSRC_NUM_DVFS_OPP		0
#define MTK_DVFSRC_NUM_DRAM_OPP		1
#define MTK_DVFSRC_NUM_VCORE_OPP	2
#define MTK_DVFSRC_CURR_DVFS_OPP	3
#define MTK_DVFSRC_CURR_DRAM_OPP	4
#define MTK_DVFSRC_CURR_VCORE_OPP	5
#define MTK_DVFSRC_CURR_DVFS_LEVEL	6
#define MTK_DVFSRC_CURR_DRAM_KHZ	7
#define MTK_DVFSRC_CURR_VCORE_UV	8

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
extern int mtk_dvfsrc_query_opp_info(u32 id);
extern int mtk_dvfsrc_vcore_opp_count(void);
extern int mtk_dvfsrc_vcore_uv_table(u32 opp);
extern int vcorefs_get_num_opp(void);
extern int vcorefs_get_opp_info_num(void);
extern int vcorefs_get_src_req_num(void);
extern char **vcorefs_get_opp_info_name(void);
extern unsigned int *vcorefs_get_opp_info(void);
extern char **vcorefs_get_src_req_name(void);
extern unsigned int *vcorefs_get_src_req(void);
extern void dvfsrc_set_power_model_ddr_request(u32 level);
extern void dvfsrc_enable_dvfs_freq_hopping(int on);
extern int register_dvfsrc_vchk_notifier(struct notifier_block *nb);
extern int unregister_dvfsrc_vchk_notifier(struct notifier_block *nb);
extern int register_dvfsrc_dump_notifier(struct notifier_block *nb);
extern int unregister_dvfsrc_dump_notifier(struct notifier_block *nb);
extern u32 dvfsrc_get_md_scenario(void);

#else
static inline int mtk_dvfsrc_query_opp_info(u32 id)
{ return 0; }
static inline int mtk_dvfsrc_vcore_opp_count(void)
{ return 0; }
static inline int mtk_dvfsrc_vcore_uv_table(u32 opp)
{ return 0; }
static inline  int vcorefs_get_num_opp(void)
{ return 0; }
static inline  int vcorefs_get_opp_info_num(void)
{ return 0; }
static inline  int vcorefs_get_src_req_num(void)
{ return 0; }
static inline  char **vcorefs_get_opp_info_name(void)
{ return NULL; }
static inline  unsigned int *vcorefs_get_opp_info(void)
{ return NULL; }
static inline  char **vcorefs_get_src_req_name(void)
{ return NULL; }
static inline  unsigned int *vcorefs_get_src_req(void)
{ return NULL; }
static inline void dvfsrc_set_power_model_ddr_request(u32 level)
{ }
static inline void dvfsrc_enable_dvfs_freq_hopping(int on)
{ }
static inline int register_dvfsrc_vchk_notifier(struct notifier_block *nb)
{ return 0; }
static inline int unregister_dvfsrc_vchk_notifier(struct notifier_block *nb)
{ return 0; }
static inline int register_dvfsrc_dump_notifier(struct notifier_block *nb)
{ return 0; }
static inline int unregister_dvfsrc_dump_notifier(struct notifier_block *nb)
{ return 0; }
static inline u32 dvfsrc_get_md_scenario(void)
{ return 0; }
#endif /* CONFIG_MTK_DVFSRC */
#endif
