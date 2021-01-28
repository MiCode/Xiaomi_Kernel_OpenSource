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

#ifdef HRT_AEE_LAYER_MASK
#undef HRT_AEE_LAYER_MASK
#endif

#ifndef CONFIG_MTK_ROUND_CORNER_SUPPORT
#define HRT_AEE_LAYER_MASK 0xFFFFFFF7
#define MAX_PHY_OVL_CNT (7)
#else
#define HRT_AEE_LAYER_MASK 0xFFFFFFF3
#define MAX_PHY_OVL_CNT (7 - 2)
#endif

/* #define HAS_LARB_HRT */

enum DISP_DEBUG_LEVEL {
	DISP_DEBUG_LEVEL_CRITICAL = 0,
	DISP_DEBUG_LEVEL_ERR,
	DISP_DEBUG_LEVEL_WARN,
	DISP_DEBUG_LEVEL_DEBUG,
	DISP_DEBUG_LEVEL_INFO,
};

enum HRT_LEVEL {
	HRT_LEVEL_ULPM = 0, /* NULL */
	HRT_LEVEL_LPM,      /* NULL */
	HRT_LEVEL_HPM,      /* OPP3 */
	HRT_LEVEL_UHPM,     /* OPP0 */
	HRT_LEVEL_NUM,
};

enum HRT_TB_TYPE {
	HRT_TB_TYPE_GENERAL = 0,
	HRT_TB_NUM,
};

enum {
	HRT_LEVEL_DEFAULT = HRT_LEVEL_NUM + 1,
};

enum HRT_BOUND_TYPE {
	HRT_BOUND_TYPE_LP4 = 0,    /* LP4-2ch */
	HRT_BOUND_TYPE_LP3,	/* LP3-1ch 16:9, 18:9*/
	HRT_BOUND_TYPE_LP4_1CH,    /* LP4-1ch */
	HRT_BOUND_TYPE_LP4_HYBRID, /* LP4-2ch special */
	HRT_BOUND_TYPE_LP3_PLUSP,  /* LP3- 19:9, 20:9, 21:9*/
	HRT_BOUND_NUM,
};

enum HRT_PATH_SCENARIO {
	HRT_PATH_GENERAL = MAKE_UNIFIED_HRT_PATH_FMT(HRT_PATH_RSZ_NONE,
						     HRT_PATH_PIPE_SINGLE,
						     HRT_PATH_DISP_DUAL_EXT, 1),
	HRT_PATH_UNKNOWN = MAKE_UNIFIED_HRT_PATH_FMT(0, 0, 0, 2),
	HRT_PATH_NUM = MAKE_UNIFIED_HRT_PATH_FMT(0, 0, 0, 3),
};

void layering_rule_init(void);

#endif
