/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_PMIC_6336_H_
#define _MT_PMIC_6336_H_

#include "mt6336_irq.h"
#ifndef MT6336_E1
#include "mt6336_upmu_hw.h"
#else
#include "mt6336_upmu_hw_e1.h"
#endif

#define MT6336_DEBUG_PR_DBG

#define MT6336TAG                "[MT6336] "

#ifdef MT6336_DEBUG
#define PMICDEB(fmt, arg...) pr_debug(MT6336TAG "cpuid=%d, " fmt, raw_smp_processor_id(), ##arg)
#define PMICFUC(fmt, arg...) pr_debug(MT6336TAG "cpuid=%d, %s\n", raw_smp_processor_id(), __func__)
#endif  /*-- defined MT6336_DEBUG --*/

#if defined MT6336_DEBUG_PR_DBG
#define PMICLOG(fmt, arg...)   pr_err(MT6336TAG fmt, ##arg)
#else
#define PMICLOG(fmt, arg...)
#endif  /*-- defined MT6336_DEBUG_PR_DBG --*/

#define PMICERR(fmt, arg...)   pr_debug(MT6336TAG "ERROR,line=%d " fmt, __LINE__, ##arg)
#define PMICREG(fmt, arg...)   pr_debug(MT6336TAG fmt, ##arg)


/* multi-read register */
extern unsigned int mt6336_read_bytes(unsigned int reg, unsigned char *returnData, unsigned int len);

/* access register api */
extern unsigned int mt6336_read_interface(unsigned int RegNum, unsigned char *val, unsigned char MASK,
unsigned char SHIFT);
extern unsigned int mt6336_config_interface(unsigned int RegNum, unsigned char val, unsigned char MASK,
unsigned char SHIFT);

/* write one register directly */
extern unsigned int mt6336_set_register_value(unsigned int RegNum, unsigned char val);
extern unsigned int mt6336_get_register_value(unsigned int RegNum);

/* write one register flag */
extern unsigned short mt6336_set_flag_register_value(MT6336_PMU_FLAGS_LIST_ENUM flagname, unsigned char val);
extern unsigned short mt6336_get_flag_register_value(MT6336_PMU_FLAGS_LIST_ENUM flagname);
/*
extern void wake_up_mt6336(void);
*/

extern const MT6336_PMU_FLAG_TABLE_ENTRY mt6336_pmu_flags_table[MT6336_PMU_COMMAND_MAX];
#endif /* _MT_PMIC_6336_H_ */
