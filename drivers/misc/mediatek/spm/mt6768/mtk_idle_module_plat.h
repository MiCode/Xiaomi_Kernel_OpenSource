/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
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
#define MTK_IDLE_FEATURE_ENABLE_IDLEDRAM	(0)
#define MTK_IDLE_FEATURE_ENABLE_IDLESYSPLL	(0)
#define MTK_IDLE_FEATURE_ENABLE_IDLEBUS26M	(0)

enum MTK_LP_FEATURE_DTS {
	MTK_LP_FEATURE_DTS_SODI = 0,
	MTK_LP_FEATURE_DTS_SODI3,
	MTK_LP_FEATURE_DTS_DP,
	MTK_LP_FEATURE_DTS_IDLEDRAM,
	MTK_LP_FEATURE_DTS_IDLESYSPLL,
	MTK_LP_FEATURE_DTS_IDLEBUS26M,
	MTK_LP_FEATURE_DTS_SUSPEND,
};

enum mt6768_idle_model {
	IDLE_MODEL_START = 0,
	IDLE_MODEL_RESOURCE_HEAD = IDLE_MODEL_START,
	IDLE_MODEL_BUS26M = IDLE_MODEL_RESOURCE_HEAD,
	IDLE_MODEL_SYSPLL,
	IDLE_MODEL_DRAM,
	IDLE_MODEL_NUM,
};

/*Parsing idle-state's status about sodi*/
#define GET_MTK_OF_PROPERTY_STATUS_IDLEDRAM(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, "idledram")

/*Parsing idle-state's status about sodi3*/
#define GET_MTK_OF_PROPERTY_STATUS_IDLESYSPLL(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, "idlesyspll")

/*Parsing idle-state's status about dp*/
#define GET_MTK_OF_PROPERTY_STATUS_IDLEBUS26M(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, "idlebus26m")


/*Check the mtk idle node which in dts exist or not*/
#define IS_MTK_LP_DTS_AVAILABLE_IDLEDRAM(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_IDLEDRAM)
#define IS_MTK_LP_DTS_AVAILABLE_IDLESYSPLL(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_IDLESYSPLL)
#define IS_MTK_LP_DTS_AVAILABLE_IDLEBUS26M(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_IDLEBUS26M)

/*Get the mtk idle dts node status value*/
#define GET_MTK_LP_DTS_VALUE_IDLEDRAM(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_IDLEDRAM)
#define GET_MTK_LP_DTS_VALUE_IDLESYSPLL(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_IDLESYSPLL)
#define GET_MTK_LP_DTS_VALUE_IDLEBUS26M(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_IDLEBUS26M)

void mtk_idle_dram_init(struct mtk_idle_init_data *pData);
bool mtk_idle_dram_enabled(void);
bool mtk_idle_dram_can_enter(int reason, struct mtk_idle_info *info);
int mtk_idle_dram_enter(int cpu);

void mtk_idle_syspll_init(struct mtk_idle_init_data *pData);
bool mtk_idle_syspll_enabled(void);
bool mtk_idle_syspll_can_enter(int reason, struct mtk_idle_info *info);
int mtk_idle_syspll_enter(int cpu);

void mtk_idle_bus26m_init(struct mtk_idle_init_data *pData);
bool mtk_idle_bus26m_enabled(void);
bool mtk_idle_bus26m_can_enter(int reason, struct mtk_idle_info *info);
int mtk_idle_bus26m_enter(int cpu);

int mtk_idle_module_initialize_plat(void);

void mtk_idle_module_exit_plat(void);
#endif
