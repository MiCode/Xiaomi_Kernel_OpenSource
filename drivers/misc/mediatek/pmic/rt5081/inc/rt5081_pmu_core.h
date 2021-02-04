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

#ifndef __LINUX_MFD_RT5081_PMU_CORE_H
#define __LINUX_MFD_RT5081_PMU_CORE_H

struct rt5081_pmu_core_platdata {
	uint8_t i2cstmr_rst_en:1;
	uint8_t i2cstmr_rst_tmr:2;
	uint8_t mrstb_en:1;
	uint8_t mrstb_tmr:3;
	uint8_t int_wdt:2;
	uint8_t int_deg:2;
};

#define RT5081_I2CRST_ENMASK	(0x80)
#define RT5081_I2CRST_ENSHFT	(7)
#define RT5081_I2CRST_TMRMASK	(0x60)
#define RT5081_I2CRST_TMRSHFT	(5)
#define RT5081_MRSTB_ENMASK	(0x10)
#define RT5081_MRSTB_ENSHFT	(4)
#define RT5081_MRSTB_TMRMASK	(0x0E)
#define RT5081_MRSTB_TMRSHFT	(1)

#define RT5081_INTWDT_TMRMASK	(0xC0)
#define RT5081_INTWDT_TMRSHFT	(6)
#define RT5081_INTDEG_TIMEMASK	(0x30)
#define RT5081_INTDEG_TIMESHFT	(4)

#endif /* #ifndef __LINUX_MFD_RT5081_PMU_CORE_H */
