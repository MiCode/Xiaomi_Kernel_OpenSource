/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_COOLER_SETTING_H
#define _MTK_COOLER_SETTING_H

/*=============================================================
 * CONFIG (SW related)
 *=============================================================
 */
 /* mtk_cooler_mutt.c */
/* 1: turn on MD throttle V2 cooler; 0: turn off */
#define FEATURE_MUTT_V2			(1)

/* mtk_cooler_mutt.c */
/* 1: turn on MD Thermal Warning Notification; 0: turn off */
#define FEATURE_THERMAL_DIAG		(1)

/* mtk_cooler_mutt.c */
/* 1: turn on adaptive MD throttle cooler; 0: turn off  */
#define FEATURE_ADAPTIVE_MUTT		(1)

/* mtk_ta.c */
/* 1: turn on SPA; 0: turn off  */
#define FEATURE_SPA			(0)




/* mtk_cooler_mutt.c*/
/*
 * "GEN  < 95 MD" --- not define FEATURE_MUTT_INTERFACE_VER
 * "GEN >= 95 MD" --- define FEATURE_MUTT_INTERFACE_VER = 2 ,
 * Add VER num to do version control if interface is changed in next GEN.
 */
#define FEATURE_MUTT_INTERFACE_VER	(2)


/*APU(mdla/vpu) throttle*/
#define THERMAL_APU_UNLIMIT

#if defined(THERMAL_APU_UNLIMIT)
extern unsigned int cl_get_apu_status(void);
extern void cl_set_apu_status(int vv);
#endif

#endif				/* _MTK_COOLER_SETTING_H */
