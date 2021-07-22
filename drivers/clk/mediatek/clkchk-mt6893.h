/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6893_H
#define __DRV_CLKCHK_MT6893_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg_ao_bus = 2,
	spm = 3,
	apmixed = 4,
	scp_adsp = 5,
	impc = 6,
	audsys = 7,
	impe = 8,
	imps = 9,
	impn = 10,
	mfgcfg = 11,
	mm = 12,
	imgsys1 = 13,
	imgsys2 = 14,
	vde1 = 15,
	vde2 = 16,
	ven1 = 17,
	ven2 = 18,
	apuc = 19,
	apuv = 20,
	apu0 = 21,
	apu1 = 22,
	apu2 = 23,
	apum0 = 24,
	apum1 = 25,
	cam_m = 26,
	cam_ra = 27,
	cam_rb = 28,
	cam_rc = 29,
	ipe = 30,
	mdp = 31,
	chk_sys_num = 32,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6893(enum chk_sys_id id);

#endif	/* __DRV_CLKCHK_MT6893_H */

