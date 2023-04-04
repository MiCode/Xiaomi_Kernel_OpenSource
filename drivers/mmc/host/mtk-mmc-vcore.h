// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 MediaTek Inc.
 */
#ifndef __MTK_MMC_VCORE_H__
#define __MTK_MMC_VCORE_H__

#if IS_ENABLED(CONFIG_MTK_SPM_V4)
#include "mtk-mmc.h"
#include <vcorefs_v3/mtk_vcorefs_governor.h>

typedef int (*request_dvfs_opp)(enum dvfs_kicker, enum dvfs_opp);
typedef int (*dvfs_setting)(int msdc, bool enable);

struct msdc_vcore_callback {
	int id;
	request_dvfs_opp request_opp_cb;
	dvfs_setting setting_cb;
};

static struct msdc_vcore_callback msdc_vcore_cbs[3] = { 0 };

void msdc_register_vcore_callback(int id,
					request_dvfs_opp request_opp_cb,
					dvfs_setting setting_cb);

void msdc_unregister_vcore_callback(int id);
#else
#define msdc_register_vcore_callback(...)
#define msdc_unregister_vcore_callback(...)
#endif

#endif