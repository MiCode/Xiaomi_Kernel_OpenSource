/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _MTK_TS_SETTING_H
#define _MTK_TS_SETTING_H

/*=============================================================
 * CONFIG (SW related)
 *=============================================================
 */
/* mtk_ts_pa.c */
/* 1: turn on MD UL throughput update; 0: turn off */
#define Feature_Thro_update				(1)


/*
 *	Request HPM in Low temperature condition
 */
#define THERMAL_LT_SET_HPM (1)

#if THERMAL_LT_SET_HPM
extern int enter_hpm_temp;
extern int leave_hpm_temp;
extern int enable_hpm_temp;
#endif

/*In src/mtk_tc.c*/
extern int get_immediate_ts4_wrap(void);

#endif				/* _MTK_TS_SETTING_H */
