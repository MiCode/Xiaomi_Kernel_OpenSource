/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_IDLE_MODULE_PLAT_H__
#define __MTK_IDLE_MODULE_PLAT_H__

#include <mtk_idle_internal.h>
#include <mtk_idle_module.h>


enum mtk_idle_module_type_support {
	IDLE_MODULE_TYPE_SO3_DP_SO,
	IDLE_MODULE_TYPE_DP_SO3_SO,
	IDLE_MODULE_TYPE_MAX
};

/********************************************************************
 * Dram/Syspll/Bus26m default feature enable/disable
 *******************************************************************/
#define MTK_IDLE_FEATURE_ENABLE_SODI        (1)
#define MTK_IDLE_FEATURE_ENABLE_DPIDLE      (1)
#define MTK_IDLE_FEATURE_ENABLE_SODI3       (1)

enum MTK_LP_FEATURE_DTS {
	MTK_LP_FEATURE_DTS_SODI = 0,
	MTK_LP_FEATURE_DTS_SODI3,
	MTK_LP_FEATURE_DTS_DP,
	MTK_LP_FEATURE_DTS_IDLEDRAM,
	MTK_LP_FEATURE_DTS_IDLESYSPLL,
	MTK_LP_FEATURE_DTS_IDLEBUS26M,
	MTK_LP_FEATURE_DTS_SUSPEND,
};

/*Parsing idle-state's status about sodi*/
#define GET_MTK_OF_PROPERTY_STATUS_SODI(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, "sodi")

/*Parsing idle-state's status about dpidle*/
#define GET_MTK_OF_PROPERTY_STATUS_DPIDLE(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, "dpidle")

/*Parsing idle-state's status about sodi3*/
#define GET_MTK_OF_PROPERTY_STATUS_SODI3(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, "sodi3")


/*Check the mtk idle node which in dts exist or not*/
#define IS_MTK_LP_DTS_AVAILABLE_SODI(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_SODI)
#define IS_MTK_LP_DTS_AVAILABLE_DPIDLE(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_DP)
#define IS_MTK_LP_DTS_AVAILABLE_SODI3(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_SODI3)

/*Get the mtk idle dts node status value*/
#define GET_MTK_LP_DTS_VALUE_SODI(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_SODI)
#define GET_MTK_LP_DTS_VALUE_DPIDLE(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_DP)
#define GET_MTK_LP_DTS_VALUE_SODI3(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_SODI3)

void mtk_sodi_init(struct mtk_idle_init_data *pData);
bool mtk_sodi_enabled(void);
bool sodi_can_enter(int reason, struct mtk_idle_info *info);
int mtk_sodi_enter(int cpu);

void mtk_dpidle_init(struct mtk_idle_init_data *pData);
bool mtk_dpidle_enabled(void);
bool dpidle_can_enter(int reason, struct mtk_idle_info *info);
int mtk_dpidle_enter(int cpu);

void mtk_sodi3_init(struct mtk_idle_init_data *pData);
bool mtk_sodi3_enabled(void);
bool sodi3_can_enter(int reason, struct mtk_idle_info *info);
int mtk_sodi3_enter(int cpu);
#endif
