/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6983_H
#define __DRV_CLKCHK_MT6983_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	apmixed = 2,
	perao = 3,
	vde1 = 4,
	vde2 = 5,
	ven1 = 6,
	ven2 = 7,
	disp = 8,
	disp1 = 9,
	mdp1 = 10,
	mdp = 11,
	img = 12,
	dip_top_dip1 = 13,
	dip_nr_dip1 = 14,
	ipe = 15,
	wpe1_dip1 = 16,
	wpe2_dip1 = 17,
	wpe3_dip1 = 18,
	spm = 19,
	vlpcfg = 20,
	vlp_ck = 21,
	cam_m = 22,
	cam_ra = 23,
	cam_ya = 24,
	cam_rb = 25,
	cam_yb = 26,
	cam_rc = 27,
	cam_yc = 28,
	cam_mr = 29,
	ccu = 30,
	afe = 31,
	mminfra_config = 32,
	chk_sys_num = 33,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6983(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6983_H */

