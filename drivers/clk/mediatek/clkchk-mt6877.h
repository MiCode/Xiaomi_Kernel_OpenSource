/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DRV_CLKCHK_MT6877_H
#define __DRV_CLKCHK_MT6877_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg_ao_bus = 2,
	spm = 3,
	apmixed = 4,
	scp_par = 5,
	audsys = 6,
	msdc0 = 7,
	impc = 8,
	impe = 9,
	imps = 10,
	impws = 11,
	impw = 12,
	impn = 13,
	mfg_ao = 14,
	mfgcfg = 15,
	mm = 16,
	imgsys1 = 17,
	imgsys2 = 18,
	vde2 = 19,
	ven1 = 20,
	apu_conn2 = 21,
	apu_conn1 = 22,
	apuv = 23,
	apu0 = 24,
	apu1 = 25,
	apum0 = 26,
	apu_ao = 27,
	cam_m = 28,
	cam_ra = 29,
	cam_rb = 30,
	ipe = 31,
	mdp = 32,
	chk_sys_num = 33,
};

extern const char * const *get_mt6877_all_clk_names(void);
extern struct regbase *get_mt6877_all_reg_bases(void);
extern struct regname *get_mt6877_all_reg_names(void);

/*ram console api*/
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif

extern void print_enabled_clks_once(void);
extern void print_subsys_reg(enum chk_sys_id id);
extern int get_sw_req_vcore_opp(void);

#endif	/* __DRV_CLKCHK_MT6877_H */

