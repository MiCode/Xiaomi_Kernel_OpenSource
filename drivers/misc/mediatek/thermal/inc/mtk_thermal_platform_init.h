/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MTK_THERMAL_PLATFORM_INIT_H__
#define __MTK_THERMAL_PLATFORM_INIT_H__
extern
unsigned int mtk_thermal_set_user_scenarios(unsigned int mask);
extern
unsigned int mtk_thermal_clear_user_scenarios(unsigned int mask);
extern int mtk_thermal_platform_init(void);
extern void mtk_thermal_platform_exit(void);
extern void mtk_cooler_shutdown_exit(void);
extern int mtk_cooler_shutdown_init(void);
extern int mtk_cooler_backlight_init(void);
extern void mtk_cooler_backlight_exit(void);
extern void mtk_cooler_kshutdown_exit(void);
extern int mtk_cooler_kshutdown_init(void);
extern void mtk_cooler_cam_exit(void);
extern int mtk_cooler_cam_init(void);
extern void mtk_thermal_pm_exit(void);
extern int mtk_thermal_pm_init(void);
extern void mt6358tsbuck1_exit(void);
extern int mt6358tsbuck1_init(void);
extern void mt6358tsbuck2_exit(void);
extern int mt6358tsbuck2_init(void);
extern void mt6358tsbuck3_exit(void);
extern int mt6358tsbuck3_init(void);
extern int mtktsbattery_init(void);
extern void mtktsbattery_exit(void);
extern void mtkts_bts_exit(void);
extern int mtkts_bts_init(void);
extern void mtkts_btsmdpa_exit(void);
extern int mtkts_btsmdpa_init(void);
extern void tscpu_exit(void);
extern int tscpu_init(void);
extern void mtktspa_exit(void);
extern int mtktspa_init(void);
extern void mtk_mdm_txpwr_exit(void);
extern int mtk_mdm_txpwr_init(void);
extern void mtktscharger_exit(void);
extern int mtktscharger_init(void);
extern void wmt_tm_deinit(void);
extern int wmt_tm_init(void);
extern void tsallts_exit(void);
extern int tsallts_init(void);
extern int mtk_imgs_init(void);
extern void mtk_imgs_exit(void);
extern void mtkts_dctm_exit(void);
extern int mtkts_dctm_init(void);
extern void mtktspmic_exit(void);
extern int mtktspmic_init(void);
extern void mtkts_bif_exit(void);
extern int mtkts_bif_init(void);
//for thermal cooler
extern int ta_init(void);
extern void mtk_cooler_mutt_exit(void);
extern int mtk_cooler_mutt_init(void);
extern void mtk_cooler_bcct_exit(void);
extern int mtk_cooler_bcct_init(void);
#if IS_ENABLED(CONFIG_MTK_GAUGE_VERSION)
extern int  mtkcooler_bcct_late_init(void);
#endif
extern void mtk_cooler_atm_exit(void);
extern int mtk_cooler_atm_init(void);
extern void mtk_cooler_dtm_exit(void);
extern int mtk_cooler_dtm_init(void);
extern void mtk_cooler_sysrst_exit(void);
extern int mtk_cooler_sysrst_init(void);
extern int mtk_cooler_VR_FPS_init(void);
extern void mtk_cooler_VR_FPS_exit(void);
#endif
