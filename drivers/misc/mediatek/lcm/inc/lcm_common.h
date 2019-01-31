/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __LCM_COMMON_H__
#define __LCM_COMMON_H__

#include "lcm_drv.h"

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
enum LCM_STATUS {
	LCM_STATUS_OK = 0,
	LCM_STATUS_ERROR,
};


void lcm_common_parse_dts(const struct LCM_DTS *DTS,
	unsigned char force_update);
void lcm_common_set_util_funcs(const struct LCM_UTIL_FUNCS *util);
void lcm_common_get_params(LCM_PARAMS *params);
void lcm_common_init(void);
void lcm_common_suspend(void);
void lcm_common_resume(void);
void lcm_common_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height);
void lcm_common_setbacklight(unsigned int level);
void lcm_common_setbacklight_cmdq(void *handle, unsigned int level);
unsigned int lcm_common_compare_id(void);
unsigned int lcm_common_ata_check(unsigned char *buffer);
#endif

#endif
