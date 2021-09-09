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
	gpueb_ao = 16,
	mfgsc_ao = 17,
	mfgcfg = 18,
	mm0 = 19,
	mm1 = 20,
	img = 21,
	dip_top_dip1 = 22,
	dip_nr_dip1 = 23,
	wpe1_dip1 = 24,
	ipe = 25,
	wpe2_dip1 = 26,
	wpe3_dip1 = 27,
	vde1 = 28,
	vde2 = 29,
	ven1 = 30,
	ven2 = 31,
	apurc = 32,
	apurcv = 33,
	apu0_ao = 34,
	npu_ao = 35,
	apu1_ao = 36,
	mvpu0_top_config = 37,
	apud0 = 38,
	apu_dla_1_config = 39,
	apuac = 40,
	spm = 41,
	vlpcfg = 42,
	vlp_ck = 43,
	cam_m = 44,
	cam_ra = 45,
	cam_ya = 46,
	cam_rb = 47,
	cam_yb = 48,
	cam_rc = 49,
	cam_yc = 50,
	cam_mr = 51,
	ccu = 52,
	afe = 53,
	mminfra_config = 54,
	gce_d = 55,
	gce_m = 56,
	mdp = 57,
	mdp1 = 58,
	chk_sys_num = 59,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6895(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6895_H */

