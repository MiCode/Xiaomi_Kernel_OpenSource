/* * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LAYER_STRATEGY_EX__
#define __LAYER_STRATEGY_EX__

#include "layering_rule_base.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#define MAX_PHY_OVL_CNT (12-2)
#else
#define MAX_PHY_OVL_CNT (12)
#endif

/* #define HAS_LARB_HRT */
#ifndef CONFIG_MTK_ROUND_CORNER_SUPPORT
#define HRT_AEE_LAYER_MASK 0xFFFFFFDF
#else
#define HRT_AEE_LAYER_MASK 0xFFFFFFEF
#endif

enum DISP_DEBUG_LEVEL {
	DISP_DEBUG_LEVEL_CRITICAL = 0,
	DISP_DEBUG_LEVEL_ERR,
	DISP_DEBUG_LEVEL_WARN,
	DISP_DEBUG_LEVEL_DEBUG,
	DISP_DEBUG_LEVEL_INFO,
};

enum HRT_LEVEL {
	HRT_LEVEL_LEVEL0 = 0,	/* OPP3 */
	HRT_LEVEL_LEVEL1,	/* OPP2 */
	HRT_LEVEL_LEVEL2,	/* OPP1 */
	HRT_LEVEL_LEVEL3,	/* OPP0 */
	HRT_LEVEL_NUM,
};

enum HRT_TB_TYPE {
	HRT_TB_TYPE_GENERAL = 0,
	HRT_TB_TYPE_RPO_L0,
	HRT_TB_TYPE_RPO_DIM_L0,
	HRT_TB_TYPE_RPO_BOTH,
	HRT_TB_NUM,
};

enum {
	HRT_LEVEL_DEFAULT = HRT_LEVEL_NUM + 1,
};

enum HRT_BOUND_TYPE {
	HRT_BOUND_TYPE_LP4 = 0,		/* LP4-2ch */
	HRT_BOUND_TYPE_LP3,			/* LP3-1ch */
	HRT_BOUND_TYPE_LP4_1CH,		/* LP4-1ch */
	HRT_BOUND_TYPE_LP4_HYBRID,	/* LP4-2ch special */
	HRT_BOUND_TYPE_LP3_HD,
	HRT_BOUND_TYPE_LP4_HD,
	HRT_BOUND_TYPE_LP4_FHD_19,
	HRT_BOUND_TYPE_LP4_FHD_19_CMD,
	HRT_BOUND_NUM,
};

enum HRT_PATH_SCENARIO {
	HRT_PATH_GENERAL =
		MAKE_UNIFIED_HRT_PATH_FMT(HRT_PATH_RSZ_NONE,
			  HRT_PATH_PIPE_SINGLE, HRT_PATH_DISP_DUAL_EXT, 1),
	HRT_PATH_RPO_L0 =
		MAKE_UNIFIED_HRT_PATH_FMT(HRT_PATH_RSZ_PARTIAL,
			  HRT_PATH_PIPE_SINGLE, HRT_PATH_DISP_SINGLE, 2),
	HRT_PATH_RPO_DIM_L0 =
		MAKE_UNIFIED_HRT_PATH_FMT(HRT_PATH_RSZ_PARTIAL,
			  HRT_PATH_PIPE_SINGLE, HRT_PATH_DISP_SINGLE, 3),
	HRT_PATH_RPO_BOTH =
		MAKE_UNIFIED_HRT_PATH_FMT(HRT_PATH_RSZ_PARTIAL,
			  HRT_PATH_PIPE_SINGLE, HRT_PATH_DISP_SINGLE, 4),
	HRT_PATH_UNKNOWN = MAKE_UNIFIED_HRT_PATH_FMT(0, 0, 0, 5),
	HRT_PATH_NUM = MAKE_UNIFIED_HRT_PATH_FMT(0, 0, 0, 6),
};

enum HRT_OPP_LEVEL {
	HRT_OPP_LEVEL_LEVEL0 = 0,	/* OPP0 */
	HRT_OPP_LEVEL_LEVEL1,	/* OPP1 */
	HRT_OPP_LEVEL_LEVEL2,	/* OPP2 */
	HRT_OPP_LEVEL_LEVEL3,	/* OPP3 */
	HRT_OPP_LEVEL_DEFAULT,	/* DEFAULT */
	HRT_OPP_LEVEL_NUM,
};

enum HRT_DRAMC_TYPE {
	HRT_DRAMC_TYPE_LP4_3733 = 0,		/* LP4-3733 */
	HRT_DRAMC_TYPE_LP4_3200,			/* LP4-3200 */
	HRT_DRAMC_TYPE_LP3,		/* LP3 */
	HRT_DRAMC_TYPE_NUM,
};

void layering_rule_init(void);
int layering_rule_get_mm_freq_table(enum HRT_OPP_LEVEL opp_level);
int layering_rule_get_emi_freq_table(enum HRT_OPP_LEVEL opp_level);
void layering_rule_set_max_hrt_level(void);
int layering_rule_get_max_hrt_level(void);
void antilatency_config_hrt(void);
void update_layering_opt_by_disp_opt(enum DISP_HELPER_OPT option, int value);
int set_emi_bound_tb(int idx, int num, int *val);

/* For dynamic modify HRT table */
int modify_display_hrt_cb(int num);
void hrt_table_locked(void);
void hrt_table_unlocked(void);
int get_hrt_bound(int is_larb, int hrt_level);
int get_hrt_discount(void);

#endif
