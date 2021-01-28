/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
