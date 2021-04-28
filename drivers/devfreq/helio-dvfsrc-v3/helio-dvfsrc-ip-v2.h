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

#ifndef __HELIO_DVFSRC_IP_V2_H
#define __HELIO_DVFSRC_IP_V2_H
#include <helio-dvfsrc-qos.h>
#include <mtk_dvfsrc_reg_v2.h>
#if defined(DVFSRC_SMC_CONTROL)
#include <mtk_dvfsrc_smc_reg.h>
#endif

#define DVFSRC_TIMEOUT          1000

extern u32 vcorefs_get_total_emi_status(void);
extern u32 vcorefs_get_scp_req_status(void);
extern u32 vcorefs_get_md_emi_latency_status(void);
extern u32 vcorefs_get_hifi_scenario(void);
extern u32 vcorefs_get_hifi_vcore_status(void);
extern u32 vcorefs_get_hifi_ddr_status(void);
extern u32 dvfsrc_get_md_bw(void);
extern u32 vcorefs_get_md_rising_ddr(void);
extern u32 vcorefs_get_hifi_rising_ddr(void);
extern u32 vcorefs_get_hrt_bw_ddr(void);
extern u32 vcorefs_get_md_scenario(void);
extern u32 vcorefs_get_md_scenario_ddr(void);
extern u32 vcorefs_get_md_imp_ddr(void);


extern u32 dvfsrc_calc_isp_hrt_opp(int data);
extern u32 dvfsrc_get_ddr_qos(void);
extern void dvfsrc_set_isp_hrt_bw(int data);
extern void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc);
extern struct regulator *dvfsrc_vcore_requlator(struct device *dev);
extern int dvfsrc_latch_register(int enable);
extern void dvfsrc_suspend_cb(struct helio_dvfsrc *dvfsrc);
extern void dvfsrc_resume_cb(struct helio_dvfsrc *dvfsrc);
#endif /* __HELIO_DVFSRC_IP_V2_H */


