/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6855_H
#define __DRV_CLKCHK_MT6855_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg = 2,
	apmixed = 3,
	imp = 4,
	perao = 5,
	afe = 6,
	mfg_ao = 7,
	mm = 8,
	imgsys1 = 9,
	imgsys2 = 10,
	vde2 = 11,
	ven1 = 12,
	spm = 13,
	vlpcfg = 14,
	vlp_ck = 15,
	cam_m = 16,
	cam_ra = 17,
	cam_rb = 18,
	ipe = 19,
	mminfra_config = 20,
	mdp = 21,
	chk_sys_num = 22,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6855(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6855_H */

