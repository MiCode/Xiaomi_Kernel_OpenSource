/*
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_RT5081_PMU_BLED_H
#define __LINUX_RT5081_PMU_BLED_H

struct rt5081_pmu_bled_platdata {
	uint8_t ext_en_pin:1;
	uint8_t chan_en:4;
	uint8_t map_linear:1;
	uint8_t bl_ovp_level:2;
	uint8_t bl_ocp_level:2;
	uint8_t use_pwm:1;
	uint8_t pwm_fsample:2;
	uint8_t pwm_deglitch:2;
	uint8_t pwm_hys_en:1;
	uint8_t pwm_hys:2;
	uint8_t pwm_avg_cycle:3;
	uint8_t bled_ramptime:4;
	uint8_t bled_flash_ramp:3;
	uint32_t max_bled_brightness;
	const char *bled_name;
};

/* RT5081_PMU_REG_BLEN : 0xA0 */
#define RT5081_BLED_EN	(0x40)
#define RT5081_BLED_EXTEN (0x80)
#define RT5081_BLED_CHANENSHFT 2
#define RT5081_BLED_MAPLINEAR (0x02)

/* RT5081_PMU_REG_BLBSTCTRL : 0xA1 */
#define RT5081_BLED_OVOCSHDNDIS (0x88)
#define RT5081_BLED_OVPSHFT (5)
#define RT5081_BLED_OCPSHFT (1)

/* RT5081_PMU_REG_BLPWM : 0xA2 */
#define RT5081_BLED_PWMSHIFT (7)
#define RT5081_BLED_PWMDSHFT (5)
#define RT5081_BLED_PWMFSHFT (3)
#define RT5081_BLED_PWMHESHFT (2)
#define RT5081_BLED_PWMHSHFT (0)

/* RT5081_PMU_REG_BLCTRL : 0xA3 */
#define RT5081_BLED_RAMPTSHFT (4)

/* RT5081_PMU_REG_BLDIM2 : 0xA6 */
#define RT5081_DIM2_MASK (0x7)

/* RT5081_PMU_REG_BLDIM1 : 0xA5 */
#define RT5081_DIM_MASK	(0xFF)

/* RT5081_PMU_REG_BLFL : 0xA7 */
#define RT5081_BLFLMODE_MASK (0xC0)
#define RT5081_BLFLMODE_SHFT (6)
#define RT5081_BLFLRAMP_SHFT (3)

/* RT5081_PMU_REG_BLFLTO : 0xA8 */
#define RT5081_BLSTRB_TOMASK (0x7F)

#endif
