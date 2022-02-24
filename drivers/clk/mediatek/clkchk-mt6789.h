/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6789_H
#define __DRV_CLKCHK_MT6789_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	peri = 3,
	spm = 4,
	apmixed = 5,
	dvfsrc_top = 6,
	ifr = 7,
	impc = 8,
	afe = 9,
	msdc0 = 10,
	impw = 11,
	impen = 12,
	impn = 13,
	mfgcfg = 14,
	mm = 15,
	disp_dsc = 16,
	imgsys1 = 17,
	vde2 = 18,
	ven1 = 19,
	cam_m = 20,
	cam_ra = 21,
	cam_rb = 22,
	ipe = 23,
	mdp = 24,
	dbgao = 25,
	chk_sys_num = 26,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6789(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6789_H */

