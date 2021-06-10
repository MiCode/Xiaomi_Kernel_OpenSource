/*
 * Copyright (C) 2017 MediaTek Inc.
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
