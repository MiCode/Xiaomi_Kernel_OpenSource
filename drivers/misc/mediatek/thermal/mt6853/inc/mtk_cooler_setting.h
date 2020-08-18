/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef _MTK_COOLER_SETTING_H
#define _MTK_COOLER_SETTING_H

/*=============================================================
 * CONFIG (SW related)
 *=============================================================
 */
/* mtk_cooler_mutt_gen97.c */
/* 1: turn on MD Thermal Warning Notification; 0: turn off */
#define FEATURE_THERMAL_DIAG		(1)

/* mtk_cooler_mutt_gen97.c */
/* 1: turn on adaptive MD throttle cooler; 0: turn off  */
#define FEATURE_ADAPTIVE_MUTT		(1)

/* mtk_ta.c */
/* 1: turn on SPA; 0: turn off  */
#define FEATURE_SPA			(0)


/*APU(mdla/vpu) throttle*/
#define THERMAL_APU_UNLIMIT

#if defined(THERMAL_APU_UNLIMIT)
extern unsigned int cl_get_apu_status(void);
extern void cl_set_apu_status(int vv);
#endif
#endif				/* _MTK_COOLER_SETTING_H */
