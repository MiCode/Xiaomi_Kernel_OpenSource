/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6895_H
#define __DRV_CLKCHK_MT6895_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	apmixed = 3,
	nemi_reg = 4,
	semi_reg = 5,
	perao = 6,
	usb_d = 7,
	usb_sif = 8,
	usb_sif_p1 = 9,
	impc = 10,
	ufsao = 11,
	ufspdn = 12,
	imps = 13,
	impw = 14,
	mfg_ao = 15,
	mfgsc_ao = 16,
	mfgcfg = 17,
	mm0 = 18,
	mm1 = 19,
	img = 20,
	dip_top_dip1 = 21,
	dip_nr_dip1 = 22,
	wpe1_dip1 = 23,
	ipe = 24,
	wpe2_dip1 = 25,
	wpe3_dip1 = 26,
	vde1 = 27,
	vde2 = 28,
	ven1 = 29,
	ven2 = 30,
	apu0_ao = 31,
	npu_ao = 32,
	apu1_ao = 33,
	spm = 34,
	vlpcfg = 35,
	vlp_ck = 36,
	cam_m = 37,
	cam_ra = 38,
	cam_ya = 39,
	cam_rb = 40,
	cam_yb = 41,
	cam_rc = 42,
	cam_yc = 43,
	cam_mr = 44,
	ccu = 45,
	afe = 46,
	mminfra_config = 47,
	gce_d = 48,
	gce_m = 49,
	mdp = 50,
	mdp1 = 51,
	img_subcomm0 = 52,
	img_subcomm1 = 53,
	cam_mm_subcomm0 = 54,
	cam_mdp_subcomm1 = 55,
	cam_sys_subcomm1 = 56,
	chk_sys_num = 57,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6895(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6895_H */

