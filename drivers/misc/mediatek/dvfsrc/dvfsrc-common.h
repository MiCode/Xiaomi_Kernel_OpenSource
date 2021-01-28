/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __DVFSRC_COMMON_H
#define __DVFSRC_COMMON_H

/* DEBUG INFO */
#define DVFSRC_MD_RISING_DDR_REQ	0
#define DVFSRC_MD_HRT_BW		1
#define DVFSRC_HIFI_VCORE_REQ		2
#define DVFSRC_HIFI_DDR_REQ		3
#define DVFSRC_HIFI_RISING_DDR_REQ	4
#define DVFSRC_HRT_BW_DDR_REQ		6
#define DVFSRC_MD_SCENARIO_REQ		7


/* SIP COMMON COMMAND*/
#define MTK_SIP_VCOREFS_INIT 0
#define MTK_SIP_VCOREFS_KICK 1
#define MTK_SIP_VCOREFS_GET_OPP_TYPE 2
#define MTK_SIP_VCOREFS_GET_FW_TYPE 3
#define MTK_SIP_VCOREFS_GET_VCORE_UV  4
#define MTK_SIP_VCOREFS_GET_DRAM_FREQ 5
#define MTK_SIP_VCOREFS_GET_NUM_V  6

#define MTK_SIP_VCOREFS_FB_ACTION 8


#if IS_ENABLED(CONFIG_MTK_DVFSRC)
extern void register_dvfsrc_opp_handler(int (*handler)(u32 id));
extern void register_dvfsrc_debug_handler(int (*handler)(u32 id));
extern void register_dvfsrc_cm_ddr_handler(void (*handler)(u32 level));
extern void register_dvfsrc_hopping_handler(void (*handler)(int on));
extern void register_dvfsrc_md_scenario_handler(u32 (*handler)(void));
extern int mtk_dvfsrc_query_debug_info(u32 id);
#else
static inline void register_dvfsrc_opp_handler(int (*handler)(u32 id))
{ }
static inline void register_dvfsrc_debug_handler(int (*handler)(u32 id))
{ }
static inline void register_dvfsrc_cm_ddr_handler(void (*handler)(u32 level))
{ }
static inline void register_dvfsrc_hopping_handler(void (*handler)(int on))
{ }
static inline void register_dvfsrc_md_scenario_handler(u32 (*handler)(void))
{ }
static inline int mtk_dvfsrc_query_debug_info(u32 id)
{ return 0; }
#endif /* CONFIG_MTK_DVFSRC */
#endif
