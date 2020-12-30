/*
 * Copyright (C) 2020 MediaTek Inc.
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
	msdc1_top = 9,
	impe = 10,
	imps = 11,
	impws = 12,
	impw = 13,
	impn = 14,
	msdc0_top = 15,
	mfg_ao = 16,
	mfgcfg = 17,
	mm = 18,
	imgsys1 = 19,
	imgsys2 = 20,
	vde2 = 21,
	ven1 = 22,
	apu_conn2 = 23,
	apu_conn1 = 24,
	apuv = 25,
	apu0 = 26,
	apu1 = 27,
	apum0 = 28,
	apu_ao = 29,
	cam_m = 30,
	cam_ra = 31,
	cam_rb = 32,
	ipe = 33,
	mdp = 34,
	chk_sys_num = 35,
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

