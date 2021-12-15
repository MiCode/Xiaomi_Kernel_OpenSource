/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __LINUX_MT6370_PMU_RGBLED_H
#define __LINUX_MT6370_PMU_RGBLED_H

enum {
	MT6370_PMU_LED1 = 0,
	MT6370_PMU_LED2,
	MT6370_PMU_LED3,
	MT6370_PMU_LED4,
	MT6370_PMU_MAXLED,
};


struct mt6370_pmu_rgbled_platdata {
	const char *led_name[MT6370_PMU_MAXLED];
	const char *led_default_trigger[MT6370_PMU_MAXLED];
};

#define MT6370_LED_MODEMASK (0x60)
#define MT6370_LED_MODESHFT (5)

#define MT6370_LED_PWMDUTYSHFT (0)
#define MT6370_LED_PWMDUTYMASK (0x1F)
#define MT6370_LED_PWMDUTYMAX (31)

#define MT6370_LEDTR1_MASK (0xF0)
#define MT6370_LEDTR1_SHFT (4)
#define MT6370_LEDTR2_MASK (0x0F)
#define MT6370_LEDTR2_SHFT (0)
#define MT6370_LEDTF1_MASK (0xF0)
#define MT6370_LEDTF1_SHFT (4)
#define MT6370_LEDTF2_MASK (0x0F)
#define MT6370_LEDTF2_SHFT (0)
#define MT6370_LEDTON_MASK (0xF0)
#define MT6370_LEDTON_SHFT (4)
#define MT6370_LEDTOFF_MASK (0x0F)
#define MT6370_LEDTOFF_SHFT (0)
#define MT6370_LEDBREATH_MAX (15)

#endif
