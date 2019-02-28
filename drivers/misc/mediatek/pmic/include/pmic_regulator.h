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

#ifndef _PMIC_REGULATOR_H_
#define _PMIC_REGULATOR_H_

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include "include/pmic.h"

#define REGULATOR_READY

#define REGULATOR_TEST 0

extern struct mtk_regulator mtk_ldos[];
extern struct of_regulator_match pmic_regulator_matches[];
extern int mtk_ldos_size;
extern int pmic_regulator_matches_size;

#ifndef CONFIG_MTK_PMIC_CHIP_MT6353
#ifdef CONFIG_MTK_PMIC_CHIP_MT6335
/*---extern variable---*/
extern struct mtk_bucks_t mtk_bucks_class[];
/*---extern function---*/
extern int buck_is_enabled(enum BUCK_TYPE type);
extern int buck_enable(enum BUCK_TYPE type, unsigned char en);
extern int buck_set_mode(enum BUCK_TYPE type, unsigned char pmode);
extern int buck_set_voltage(enum BUCK_TYPE type, unsigned int voltage);
extern unsigned int buck_get_voltage(enum BUCK_TYPE type);
#endif /*--COMMON API after MT6335--*/
#endif /*--COMMON API after MT6353--*/

#ifdef REGULATOR_TEST
extern void pmic_regulator_en_test(void);
extern void pmic_regulator_vol_test(void);
#endif /*--REGULATOR_TEST--*/

#endif				/* _PMIC_REGULATOR_H_ */
