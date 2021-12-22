/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6879_H
#define __DRV_CLKCHK_MT6879_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	apmixed = 3,
	ifr = 4,
	nemi_reg = 5,
	perao = 6,
	impc = 7,
	ufsao = 8,
	ufspdn = 9,
	impe = 10,
	impw = 11,
	impen = 12,
	mfg_ao = 13,
	mfgcfg = 14,
	mm = 15,
	img = 16,
	dip_top_dip1 = 17,
	dip_nr_dip1 = 18,
	wpe1_dip1 = 19,
	ipe = 20,
	wpe2_dip1 = 21,
	wpe3_dip1 = 22,
	vde2 = 23,
	ven1 = 24,
	apu_ao = 25,
	spm = 26,
	vlpcfg = 27,
	vlp_ck = 28,
	cam_m = 29,
	cam_ra = 30,
	cam_ya = 31,
	cam_rb = 32,
	cam_yb = 33,
	cam_mr = 34,
	ccu = 35,
	afe = 36,
	mminfra_config = 37,
	mdp = 38,
	bcrm_ifr_ao = 39,
	bcrm_ifr_ao1 = 40,
	bcrm_ifr_pdn = 41,
	bcrm_ifr_pdn1 = 42,
	hfrp = 43,
	mminfra_smi = 44,
	sspm = 45,
	sspm_cfg = 46,
	infracfg1 = 47,
	chk_sys_num = 48,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6879(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6879(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6879(void);
#endif	/* __DRV_CLKCHK_MT6879_H */

