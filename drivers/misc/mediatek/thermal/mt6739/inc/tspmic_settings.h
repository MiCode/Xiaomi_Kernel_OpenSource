/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __TSPMIC_SETTINGS_H__
#define __TSPMIC_SETTINGS_H__

#include <mach/upmu_hw.h>
#include <mach/mtk_pmic_wrap.h>

/*=============================================================
 * Genernal
 *=============================================================
 */
#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))
#define _BIT_(_bit_)		(unsigned)(1 << (_bit_))
#define _BITMASK_(_bits_)	(((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))

#define mtktspmic_TEMP_CRIT 150000 /* 150.000 degree Celsius */
#define y_pmic_repeat_times	1

#define mtktspmic_info(fmt, args...)   pr_info("[Thermal/TZ/PMIC] " fmt, ##args)


#define mtktspmic_dprintk(fmt, args...)   \
do {									\
	if (mtktspmic_debug_log == 1) {				\
		pr_debug("[Thermal/TZ/PMIC] " fmt, ##args); \
	}								   \
} while (0)

extern int mtktspmic_debug_log;
extern void mtktspmic_cali_prepare(void);
extern void mtktspmic_cali_prepare2(void);
extern int mtktspmic_get_hw_temp(void);
extern int mt6357tsbuck1_get_hw_temp(void);
extern int mt6357tsbuck2_get_hw_temp(void);
extern u32 pmic_Read_Efuse_HPOffset(int i);

#endif	/* __TSPMIC_SETTINGS_H__ */
