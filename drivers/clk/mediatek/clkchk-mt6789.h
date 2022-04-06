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
	impc = 7,
	afe = 8,
	impw = 9,
	impen = 10,
	impn = 11,
	mfgcfg = 12,
	mm = 13,
	imgsys1 = 14,
	vde2 = 15,
	ven1 = 16,
	cam_m = 17,
	cam_ra = 18,
	cam_rb = 19,
	ipe = 20,
	mdp = 21,
	dbgao = 22,
	chk_sys_num = 23,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6789(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6789_H */

