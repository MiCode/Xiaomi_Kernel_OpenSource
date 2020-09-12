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

#ifndef __DRV_CLKDBG_MT6833_H
#define __DRV_CLKDBG_MT6833_H

enum dbg_sys_id {
	topckgen,
	infracfg_ao,
	scpsys,
	apmixed,
	apu0,
	apu1,
	apuvc,
	apuc,
	audio,
	mfgsys,
	mmsys,
	mdpsys,
	img1sys,
	img2sys,
	i2c_c,
	i2c_e,
	i2c_n,
	i2c_s,
	i2c_w,
	i2c_ws,
	infracfg,
	ipesys,
	camsys,
	cam_rawa_sys,
	cam_rawb_sys,
	pericfg,
	scp_par,
	vencsys,
	vdecsys,
	infracfg_dbg,
	infrapdn_dbg,
	dbg_sys_num,
};

extern void subsys_if_on(void);

/*ram console api*/
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif

extern const char * const *get_mt6833_all_clk_names(void);
extern void print_enabled_clks_once(void);
extern void print_subsys_reg(enum dbg_sys_id id);
extern int get_sw_req_vcore_opp(void);

#endif	/* __DRV_CLKDBG_MT6833_H */
