/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6833_H
#define __DRV_CLKCHK_MT6833_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	peri = 3,
	spm = 4,
	apmixed = 5,
	ifr = 6,
	impc = 7,
	afe = 8,
	msdc0 = 9,
	impe = 10,
	imps = 11,
	impws = 12,
	impw = 13,
	impn = 14,
	mfgcfg = 15,
	mm = 16,
	imgsys1 = 17,
	imgsys2 = 18,
	vde2 = 19,
	ven1 = 20,
	cam_m = 21,
	cam_ra = 22,
	cam_rb = 23,
	ipe = 24,
	mdp = 25,
	chk_sys_num = 26,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6833(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6833_H */

