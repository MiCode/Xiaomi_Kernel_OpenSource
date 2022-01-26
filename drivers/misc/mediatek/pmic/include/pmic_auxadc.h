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

#ifndef _PMIC_AUXADC_H_
#define _PMIC_AUXADC_H_

#include <mach/upmu_hw.h>

extern bool is_isense_supported(void);
extern void pmic_auxadc_suspend(void);
extern void pmic_auxadc_resume(void);
extern void wk_auxadc_reset(void);
extern unsigned short pmic_set_hk_reg_value(
			PMU_FLAGS_LIST_ENUM flagname, unsigned int val);

#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
/* MT6358 add it */
extern int wk_vbat_cali(int vbat_out, int precision_factor);
#endif

#endif				/* _PMIC_AUXADC_H_ */
