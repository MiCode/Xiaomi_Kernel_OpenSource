/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#define THERMAL_LT_SET_OPP

/*In src/mtk_tc.c*/
extern int tscpu_min_temperature(void);

#endif				/* _MTK_TS_SETTING_H */
