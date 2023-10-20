/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */


/* MD_RF_NOTIFY(0, LCM_NOTFY1, "LCM")
 * para. 0: bit in parameter md send;
 * para. 1: function name;
 * para. 2: module name;
 */
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
MD_RF_NOTIFY(0, mtk_disp_mipi_ccci_callback, "MIPI_CLK")
MD_RF_NOTIFY(1, mtk_disp_osc_ccci_callback, "LCM_OSC")
#endif
