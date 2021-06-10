/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLKDBG_MT6853_H
#define __DRV_CLKDBG_MT6853_H

enum chk_sys_id {
	top,
	ifrao,
	infracfg_ao_bus,
	spm,
	apmixed,
	scp_par,
	impc,
	audsys,
	impe,
	imps,
	impws,
	impw,
	impn,
	mfg,
	mm,
	imgsys1,
	imgsys2,
	vdec,
	venc,
	apuc,
	apuv,
	apu0,
	apu1,
	cam_m,
	cam_ra,
	cam_rb,
	ipe,
	mdp,
	chk_sys_num,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif
#endif	/* __DRV_CLKDBG_MT6853_H */
