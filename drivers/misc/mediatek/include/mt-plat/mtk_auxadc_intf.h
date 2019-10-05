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

#ifndef __LINUX_MTK_AUXADC_INTF_H
#define __LINUX_MTK_AUXADC_INTF_H

#include <mach/mtk_pmic.h>

/* =========== User Layer ================== */
/*
 * pmic_get_auxadc_value(legacy PMIC AUXADC interface)
 * if return value < 0 -> means get data fail
 */
extern int pmic_get_auxadc_value(int list);

enum {
#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
	/* mt6357 */
	AUXADC_LIST_BATADC,
	AUXADC_LIST_MT6357_START = AUXADC_LIST_BATADC,
	AUXADC_LIST_VCDT,
	AUXADC_LIST_BATTEMP,
	AUXADC_LIST_BATID,
	AUXADC_LIST_VBIF,
	AUXADC_LIST_MT6357_CHIP_TEMP,
	AUXADC_LIST_DCXO,
	AUXADC_LIST_ACCDET,
	AUXADC_LIST_TSX,
	AUXADC_LIST_HPOFS_CAL,
	AUXADC_LIST_ISENSE,
	AUXADC_LIST_MT6357_BUCK1_TEMP,
	AUXADC_LIST_MT6357_BUCK2_TEMP,
	AUXADC_LIST_MT6357_END = AUXADC_LIST_MT6357_BUCK2_TEMP,
#endif
	AUXADC_LIST_MAX,
};

#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
extern void mt6357_auxadc_init(void);
extern void mt6357_auxadc_dump_regs(char *buf);
extern int mt6357_get_auxadc_value(u8 channel);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6357 */

#endif /* __LINUX_MTK_AUXADC_INTF_H */
