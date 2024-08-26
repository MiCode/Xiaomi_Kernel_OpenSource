// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_ADC_CHANNEL_DEF_H__
#define __LINUX_HUAQIN_ADC_CHANNEL_DEF_H__

enum sc_adc_channel {
	ADC_GET_VBUS = 0,
	ADC_GET_VSYS,
	ADC_GET_VBAT,
	ADC_GET_VAC,
	ADC_GET_IBUS,
	ADC_GET_IBAT,

	ADC_GET_TSBUS,
	ADC_GET_TSBAT,
	ADC_GET_TDIE,
	ADC_GET_MAX,
};

#endif /* __LINUX_HUAQIN_ADC_CHANNEL_DEF_H__ */

