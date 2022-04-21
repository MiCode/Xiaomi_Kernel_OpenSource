/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __DRV_CLKCHK_MT6893_H
#define __DRV_CLKCHK_MT6893_H

enum chk_sys_id {
	topckgen,
	infracfg,
	scpsys,
	apmixed,
	audio,
	mfgsys,
	mmsys,
	mdpsys,
	img1sys,
	img2sys,
	ipesys,
	camsys,
	cam_rawa_sys,
	cam_rawb_sys,
	cam_rawc_sys,
	vencsys,
	venc_c1_sys,
	vdecsys,
	vdec_soc_sys,
	ipu_vcore,
	ipu_conn,
	ipu0,
	ipu1,
	ipu2,
	infracfg_dbg,
	scp_par,
	chk_sys_num,
};

extern const char * const *get_mt6893_all_clk_names(void);
extern struct regbase *get_mt6893_all_reg_bases(void);
extern struct regname *get_mt6893_all_reg_names(void);

/*ram console api*/
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif

extern void print_enabled_clks_once(void);
extern void print_subsys_reg(enum chk_sys_id id);
extern int get_sw_req_vcore_opp(void);

#endif	/* __DRV_CLKCHK_MT6893_H */

