/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_ROUND_CORNER_H_
#define _MTK_DRM_ROUND_CORNER_H_

#include "../mtk_drm_gateic.h"
#include "dt-bindings/lcm/mtk_lcm_settings.h"

struct rc_pattern {
	unsigned int size;
	void *addr;
};

struct mtk_lcm_rc_pattern {
	char name[MTK_LCM_NAME_LENGTH];
	struct rc_pattern left_top;
	struct rc_pattern left_top_left;
	struct rc_pattern left_top_right;
	struct rc_pattern right_top;
	struct rc_pattern left_bottom;
	struct rc_pattern right_bottom;
};

enum mtk_lcm_rc_locate {
	RC_LEFT_TOP = 0,
	RC_LEFT_TOP_LEFT,
	RC_LEFT_TOP_RIGHT,
	RC_RIGHT_TOP,
	RC_LEFT_BOTTOM,
	RC_RIGHT_BOTTOM,
};

/* function: get round corner pattern address
 * input:
 *   index: pattern id parsing from panel dts settings
 *   locate: pattern location
 *   size: rc pattern data size
 * output: round corner pattern address
 */
void *mtk_lcm_get_rc_addr(const char *name, enum mtk_lcm_rc_locate locate,
		unsigned int *size);
#endif
