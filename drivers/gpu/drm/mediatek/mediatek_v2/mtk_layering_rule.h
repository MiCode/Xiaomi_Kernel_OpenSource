/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __LAYER_STRATEGY_EX__
#define __LAYER_STRATEGY_EX__

#include "mtk_layering_rule_base.h"
#include "mtk_drm_helper.h"
/* #include "lcm_drv.h" */

#define MAX_PHY_OVL_CNT (12)
/* #define HAS_LARB_HRT */
#define HRT_AEE_LAYER_MASK 0xFFFFFFBF

enum DISP_DEBUG_LEVEL {
	DISP_DEBUG_LEVEL_CRITICAL = 0,
	DISP_DEBUG_LEVEL_ERR,
	DISP_DEBUG_LEVEL_WARN,
	DISP_DEBUG_LEVEL_DEBUG,
	DISP_DEBUG_LEVEL_INFO,
};

enum HRT_LEVEL {
	HRT_LEVEL_LEVEL0 = 0,
	HRT_LEVEL_LEVEL1,
	HRT_LEVEL_LEVEL2,
	HRT_LEVEL_LEVEL3,
	HRT_LEVEL_NUM,
	HRT_LEVEL_DEFAULT = HRT_LEVEL_NUM + 1,
};

enum HRT_TB_TYPE {
	HRT_TB_TYPE_GENERAL0 = 0,
	HRT_TB_TYPE_GENERAL1,
	HRT_TB_TYPE_RPO_L0,
	HRT_TB_TYPE_SINGLE_LAYER,
	HRT_TB_NUM,
};

enum HRT_BOUND_TYPE {
	HRT_BOUND_TYPE_LP4 = 0,
	HRT_BOUND_NUM,
};

void mtk_layering_rule_init(struct drm_device *dev);
void mtk_update_layering_opt_by_disp_opt(enum MTK_DRM_HELPER_OPT opt,
					 int value);
unsigned int _layering_rule_get_hrt_idx(void);
unsigned long long _layering_get_frame_bw(struct drm_crtc *crtc,
					struct drm_display_mode *mode);
// int layering_get_valid_hrt(void);
// void copy_hrt_bound_table(int is_larb, int *hrt_table);

#endif
