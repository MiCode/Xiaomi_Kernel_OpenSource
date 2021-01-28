/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6360_PMU_RGBLED_H
#define __MT6360_PMU_RGBLED_H

enum {
	MT6360_LED_1,
	MT6360_LED_2,
	MT6360_LED_3,
	MT6360_LED_MOONLIGHT,
	MT6360_LED_MAX,
};

struct mt6360_rgbled_platform_data {
	const char *led_name[MT6360_LED_MAX];
	const char *led_default_trigger[MT6360_LED_MAX];
};

#define MT6360_LED_PWMDUTYMAX (7)

#define MT6360_LED_MODEMASK   (0xC0)
#define MT6360_LED_MODESHFT   (6)

#define MT6360_LED_PWMDUTYMASK (0xff)
#define MT6360_LED_PWMDUTYSHFT (0)

#define MT6360_LED1_PWMFREQMASK (0xe0)
#define MT6360_LED1_PWMFREQSHFT (5)
#define MT6360_LED2_PWMFREQMASK (0x1c)
#define MT6360_LED2_PWMFREQSHFT (2)

#define MT6360_LED3_PWMFREQMASK (0xe0)
#define MT6360_LED3_PWMFREQSHFT (5)

#define MT6360_LED_TR1MASK     (0xf0)
#define MT6360_LED_TR1SHFT     (4)

#define MT6360_LED_TR2MASK     (0x0f)
#define MT6360_LED_TR2SHFT     (0)

#define MT6360_LED_TF1MASK     (0xf0)
#define MT6360_LED_TF1SHFT     (4)

#define MT6360_LED_TF2MASK     (0x0f)
#define MT6360_LED_TF2SHFT     (0)

#define MT6360_LED_TONMASK     (0xf0)
#define MT6360_LED_TONSHFT     (4)

#define MT6360_LED_TOFFMASK     (0x0f)
#define MT6360_LED_TOFFSHFT     (0)

#define MT6360_LED1_SWMODE_MASK  (0x08)


#endif /* __MT6360_PMU_RGBLED_H */
