/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
