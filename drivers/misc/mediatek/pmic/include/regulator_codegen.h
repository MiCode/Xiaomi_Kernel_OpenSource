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

#ifndef _REGULATOR_CODEGEN_H_
#define _REGULATOR_CODEGEN_H_

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include <mach/upmu_hw.h>
#include <mach/upmu_sw.h>

#ifdef CONFIG_MTK_PMIC_CHIP_MT6335
#include "mt6335/mtk_regulator_codegen.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6353
#include "mt6353/mtk_regulator_codegen.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6355
#include "mt6355/mtk_regulator_codegen.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6356
#include "mt6356/mtk_regulator_codegen.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
#include "mt6357/mtk_regulator_codegen.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
#include "mt6358/mtk_regulator_codegen.h"
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6359
#include "mt6359/mtk_regulator_codegen.h"
#endif

/*****************************************************************************
 * PMIC extern function
 ******************************************************************************/
extern int mtk_regulator_enable(struct regulator_dev *rdev);
extern int mtk_regulator_disable(struct regulator_dev *rdev);
extern int mtk_regulator_is_enabled(struct regulator_dev *rdev);
extern int mtk_regulator_set_voltage_sel(
			struct regulator_dev *rdev, unsigned int selector);
extern int mtk_regulator_get_voltage_sel(struct regulator_dev *rdev);
extern int mtk_regulator_list_voltage(
			struct regulator_dev *rdev, unsigned int selector);

#endif				/* _REGULATOR_CODEGEN_H_ */
