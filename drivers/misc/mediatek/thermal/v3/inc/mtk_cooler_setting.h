/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
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

#endif				/* _MTK_COOLER_SETTING_H */
