/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_LP_DTS_DEF_H__
#define __MTK_LP_DTS_DEF_H__

enum MTK_LP_FEATURE_DTS {
	MTK_LP_FEATURE_DTS_SODI = 0,
	MTK_LP_FEATURE_DTS_SODI3,
	MTK_LP_FEATURE_DTS_DP,
	MTK_LP_FEATURE_DTS_SUSPEND,
};

#define MTK_OF_PROPERTY_STATUS_FOUND	(1<<0U)
#define MTK_OF_PROPERTY_VALUE_ENABLE	(1<<1U)
#define MTK_LP_FEATURE_DTS_NAME_SODI	"SODI"
#define MTK_LP_FEATURE_DTS_NAME_SODI3	"SODI3"
#define MTK_LP_FEATURE_DTS_NAME_DP		"DPIDLE"
#define MTK_LP_FEATURE_DTS_NAME_SUSPEND	"SUSPEND"

#define MTK_LP_FEATURE_DTS_PROPERTY_IDLE_NODE	"idle-states"
#define MTK_LP_SPM_DTS_COMPATIABLE_NODE			"mediatek,sleep"
#endif
