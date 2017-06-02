/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regmap.h>
#include "sdm660-cdc-registers.h"

extern struct reg_default
		msm89xx_cdc_core_defaults[MSM89XX_CDC_CORE_CACHE_SIZE];
extern struct reg_default
		msm89xx_pmic_cdc_defaults[MSM89XX_PMIC_CDC_CACHE_SIZE];

bool msm89xx_cdc_core_readable_reg(struct device *dev, unsigned int reg);
bool msm89xx_cdc_core_writeable_reg(struct device *dev, unsigned int reg);
bool msm89xx_cdc_core_volatile_reg(struct device *dev, unsigned int reg);

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	AIF2_VIFEED,
	AIF3_SVA,
	NUM_CODEC_DAIS,
};

enum codec_versions {
	TOMBAK_1_0,
	TOMBAK_2_0,
	CONGA,
	CAJON,
	CAJON_2_0,
	DIANGU,
	DRAX_CDC,
	UNSUPPORTED,
};

/* Support different hph modes */
enum {
	NORMAL_MODE = 0,
	HD2_MODE,
};

enum dig_cdc_notify_event {
	DIG_CDC_EVENT_INVALID,
	DIG_CDC_EVENT_CLK_ON,
	DIG_CDC_EVENT_CLK_OFF,
	DIG_CDC_EVENT_RX1_MUTE_ON,
	DIG_CDC_EVENT_RX1_MUTE_OFF,
	DIG_CDC_EVENT_RX2_MUTE_ON,
	DIG_CDC_EVENT_RX2_MUTE_OFF,
	DIG_CDC_EVENT_RX3_MUTE_ON,
	DIG_CDC_EVENT_RX3_MUTE_OFF,
	DIG_CDC_EVENT_PRE_RX1_INT_ON,
	DIG_CDC_EVENT_PRE_RX2_INT_ON,
	DIG_CDC_EVENT_POST_RX1_INT_OFF,
	DIG_CDC_EVENT_POST_RX2_INT_OFF,
	DIG_CDC_EVENT_SSR_DOWN,
	DIG_CDC_EVENT_SSR_UP,
	DIG_CDC_EVENT_LAST,
};
