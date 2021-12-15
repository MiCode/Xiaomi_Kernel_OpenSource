/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _PMIC_API_H_
#define _PMIC_API_H_

#ifdef CONFIG_MTK_PMIC_CHIP_MT6353
#include "mt6353/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6335
#include "mt6335/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6355
#include "mt6355/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6356
#include "mt6356/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
#include "mt6357/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
#include "mt6358/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6359
#include "mt6359/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6359P
#include "mt6359p/mtk_pmic_api.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6390
#include "mt6390/mtk_pmic_api.h"
#endif
#endif				/* _PMIC_API_H_ */
