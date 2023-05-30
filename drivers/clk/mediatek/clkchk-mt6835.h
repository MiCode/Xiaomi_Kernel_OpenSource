/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6835_H
#define __DRV_CLKCHK_MT6835_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	apmixed = 3,
	nemi_reg = 4,
	dpmaif = 5,
	emi_bus = 6,
	perao = 7,
	afe = 8,
	impc = 9,
	ufsao = 10,
	ufspdn = 11,
	impws = 12,
	imps = 13,
	impen = 14,
	mfgcfg = 15,
	mm = 16,
	imgsys1 = 17,
	vde2 = 18,
	ven1 = 19,
	spm = 20,
	vlpcfg = 21,
	vlp_ck = 22,
	scp_iic = 23,
	cam_m = 24,
	cam_sub1_bus = 25,
	cam_sub0_bus = 26,
	cam_ra = 27,
	cam_rb = 28,
	ipe = 29,
	dvfsrc_apb = 30,
	sramrc_apb = 31,
	mminfra_config = 32,
	mdp = 33,
	chk_sys_num = 34,
};

enum chk_pd_id {
	MT6835_CHK_PD_MD1 = 0,
	MT6835_CHK_PD_CONN = 1,
	MT6835_CHK_PD_UFS0 = 2,
	MT6835_CHK_PD_AUDIO = 3,
	MT6835_CHK_PD_ISP_DIP1 = 4,
	MT6835_CHK_PD_ISP_IPE = 5,
	MT6835_CHK_PD_VDE0 = 6,
	MT6835_CHK_PD_VEN0 = 7,
	MT6835_CHK_PD_CAM_MAIN = 8,
	MT6835_CHK_PD_CAM_SUBA = 9,
	MT6835_CHK_PD_CAM_SUBB = 10,
	MT6835_CHK_PD_DIS0 = 11,
	MT6835_CHK_PD_MM_INFRA = 12,
	MT6835_CHK_PD_MM_PROC = 13,
	MT6835_CHK_PD_MFG0 = 14,
	MT6835_CHK_PD_MFG1 = 15,
	MT6835_CHK_PD_MFG2 = 16,
	MT6835_CHK_PD_MFG3 = 17,
	MT6835_CHK_PD_APU = 18,
	MT6835_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6835(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6835(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6835(void);
extern u32 get_mt6835_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6835_H */
