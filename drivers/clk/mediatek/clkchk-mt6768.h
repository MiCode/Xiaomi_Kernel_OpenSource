/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLKCHK_MT6833_H
#define __DRV_CLKCHK_MT6833_H

enum chk_sys_id {
	topckgen = 0,
	infracfg,
	pericfg,
	scpsys,
	apmixedsys,
	audiosys,
	gce,
	mipi_0a,
	mipi_0b,
	mipi_1a,
	mipi_1b,
	mipi_2a,
	mipi_2b,
	mfgsys,
	mmsys,
	imgsys,
	camsys,
	vencsys,
	vdecsys,
	chk_sys_num,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6768(enum chk_sys_id id);
#endif	/* __DRV_CLKCHK_MT6833_H */

