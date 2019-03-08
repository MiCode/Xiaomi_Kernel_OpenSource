/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __LCM_UTIL_H__
#define __LCM_UTIL_H__

#include "lcm_drv.h"
#include "lcm_common.h"


#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
enum LCM_STATUS lcm_util_set_data(const struct LCM_UTIL_FUNCS *lcm_util,
	char type, struct LCM_DATA_T1 *t1);
enum LCM_STATUS lcm_util_set_write_cmd_v1(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T5 *t5,
	unsigned char force_update);
enum LCM_STATUS lcm_util_set_write_cmd_v11(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T5 *t5,
	unsigned char force_update, void *cmdq);
enum LCM_STATUS lcm_util_set_write_cmd_v2(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T3 *t3,
	unsigned char force_update);
enum LCM_STATUS lcm_util_set_write_cmd_v23(
	const struct LCM_UTIL_FUNCS *lcm_util, void *handle,
	struct LCM_DATA_T3 *t3, unsigned char force_update);
enum LCM_STATUS lcm_util_set_read_cmd_v2(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T4 *t4,
	unsigned int *compare);
#endif

#endif

