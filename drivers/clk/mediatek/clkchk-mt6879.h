/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6879_H
#define __DRV_CLKCHK_MT6879_H

enum chk_sys_id {
	mcusys_config_reg = 0,
	top = 1,
	ifrao = 2,
	infracfg = 3,
	apmixed = 4,
	ifr = 5,
	nemi_reg = 6,
	perao = 7,
	impc = 8,
	ufsao = 9,
	ufspdn = 10,
	impe = 11,
	impw = 12,
	impen = 13,
	mfg_ao = 14,
	mfgcfg = 15,
	mm = 16,
	img = 17,
	dip_top_dip1 = 18,
	dip_nr_dip1 = 19,
	wpe1_dip1 = 20,
	ipe = 21,
	wpe2_dip1 = 22,
	wpe3_dip1 = 23,
	vde2 = 24,
	ven1 = 25,
	apu_ao = 26,
	spm = 27,
	vlpcfg = 28,
	vlp_ck = 29,
	cam_m = 30,
	cam_ra = 31,
	cam_ya = 32,
	cam_rb = 33,
	cam_yb = 34,
	cam_mr = 35,
	ccu = 36,
	afe = 37,
	mminfra_config = 38,
	mdp = 39,
	chk_sys_num = 40,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6879(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6879_H */

