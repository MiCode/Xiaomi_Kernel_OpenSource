/*
 *  drivers/misc/mediatek/pmic/mt6360/inc/mt6360_pmu_adc.h
 *
 *  Copyright (C) 2018 Mediatek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT6360_PMU_ADC_H
#define __MT6360_PMU_ADC_H

struct mt6360_adc_platform_data {
	u32 adc_wait_t;
	u32 adc_idle_t;
	u32 zcv_en;
};

enum {
	USBID_CHANNEL,
	VBUSDIV5_CHANNEL,
	VBUSDIV2_CHANNEL,
	VSYS_CHANNEL,
	VBAT_CHANNEL,
	IBUS_CHANNEL,
	IBAT_CHANNEL,
	CHG_VDDP_CHANNEL,
	TEMP_JC_CHANNEL,
	VREF_TS_CHANNEL,
	TS_CHANNEL,
	MAX_CHANNEL,
};

#endif /* __MT6360_PMU_ADC_H */
