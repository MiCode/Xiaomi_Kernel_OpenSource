/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6886_H
#define __DRV_CLKCHK_MT6886_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	apmixed = 3,
	emi_reg = 4,
	emi_bus = 5,
	perao = 6,
	afe = 7,
	impc = 8,
	ufscfg_ao_bus = 9,
	ufsao = 10,
	ufspdn = 11,
	impes = 12,
	impw = 13,
	impe = 14,
	gpu_eb_rpc = 15,
	mfg_ao = 16,
	mfgsc_ao = 17,
	mm = 18,
	img = 19,
	img_sub0_bus = 20,
	img_sub1_bus = 21,
	dip_top_dip1 = 22,
	dip_nr1_dip1 = 23,
	dip_nr2_dip1 = 24,
	wpe1_dip1 = 25,
	wpe2_dip1 = 26,
	wpe3_dip1 = 27,
	traw_dip1 = 28,
	vde2 = 29,
	ven = 30,
	cam_sub0_bus = 31,
	cam_sub2_bus = 32,
	cam_sub1_bus = 33,
	spm = 34,
	vlpcfg = 35,
	vlp_ck = 36,
	scp = 37,
	scp_iic = 38,
	cam_m = 39,
	cam_ra = 40,
	cam_ya = 41,
	cam_rb = 42,
	cam_yb = 43,
	cam_mr = 44,
	ccu = 45,
	dvfsrc_apb = 46,
	mminfra_config = 47,
	mdp = 48,
	cci = 49,
	cpu_ll = 50,
	cpu_bl = 51,
	ptp = 52,
	hwv_wrt = 53,
	hwv = 54,
	hwv_ext = 55,
	chk_sys_num = 56,
};

enum chk_pd_id {
	MT6886_CHK_PD_MFG1 = 0,
	MT6886_CHK_PD_MFG2 = 1,
	MT6886_CHK_PD_MFG9 = 2,
	MT6886_CHK_PD_MFG10 = 3,
	MT6886_CHK_PD_MFG11 = 4,
	MT6886_CHK_PD_MFG12 = 5,
	MT6886_CHK_PD_MD1 = 6,
	MT6886_CHK_PD_CONN = 7,
	MT6886_CHK_PD_UFS0 = 8,
	MT6886_CHK_PD_UFS0_PHY = 9,
	MT6886_CHK_PD_AUDIO = 10,
	MT6886_CHK_PD_ADSP_TOP = 11,
	MT6886_CHK_PD_ADSP_INFRA = 12,
	MT6886_CHK_PD_ISP_MAIN = 13,
	MT6886_CHK_PD_ISP_DIP1 = 14,
	MT6886_CHK_PD_ISP_VCORE = 15,
	MT6886_CHK_PD_VDE0 = 16,
	MT6886_CHK_PD_VEN0 = 17,
	MT6886_CHK_PD_CAM_MAIN = 18,
	MT6886_CHK_PD_CAM_MRAW = 19,
	MT6886_CHK_PD_CAM_SUBA = 20,
	MT6886_CHK_PD_CAM_SUBB = 21,
	MT6886_CHK_PD_CAM_VCORE = 22,
	MT6886_CHK_PD_MDP0 = 23,
	MT6886_CHK_PD_DIS0 = 24,
	MT6886_CHK_PD_MM_INFRA = 25,
	MT6886_CHK_PD_MM_PROC = 26,
	MT6886_CHK_PD_APU = 27,
	MT6886_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6886(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6886(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6886(void);
extern u32 get_mt6886_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6886_H */
