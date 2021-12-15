// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKDBG_MT6885_H
#define __DRV_CLKDBG_MT6885_H

enum dbg_sys_id {
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
	dbg_sys_num,
};

extern void subsys_if_on(void);
extern const char * const *get_mt6893_all_clk_names(void);
extern void print_enabled_clks_once(void);
extern void print_subsys_reg(enum dbg_sys_id id);
extern int get_sw_req_vcore_opp(void);

#endif	/* __DRV_CLKDBG_MT6758_H */
