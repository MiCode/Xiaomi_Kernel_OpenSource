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
	disp_dsc = 14,
	imgsys1 = 15,
	vde2 = 16,
	ven1 = 17,
	cam_m = 18,
	cam_ra = 19,
	cam_rb = 20,
	ipe = 21,
	mdp = 22,
	dbgao = 23,
	chk_sys_num = 24,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6789(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6789_H */

