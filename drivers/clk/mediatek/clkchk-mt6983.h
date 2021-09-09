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
	cam_yc = 4,
	vde1 = 5,
	ven2 = 6,
	disp = 7,
	img = 8,
	dip_top_dip1 = 9,
	dip_nr_dip1 = 10,
	wpe1_dip1 = 11,
	ipe = 12,
	wpe2_dip1 = 13,
	wpe3_dip1 = 14,
	vde2 = 15,
	ven1 = 16,
	cam_rc = 17,
	spm = 18,
	vlpcfg = 19,
	vlp_ck = 20,
	cam_m = 21,
	cam_ra = 22,
	cam_ya = 23,
	cam_rb = 24,
	cam_yb = 25,
	cam_mr = 26,
	ccu = 27,
	afe = 28,
	mminfra_config = 29,
	disp1 = 30,
	mdp1 = 31,
	mdp = 32,
	chk_sys_num = 33,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6983(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6983_H */

