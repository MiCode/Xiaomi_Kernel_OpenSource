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
#ifndef __WCD_MBHC_ADC_H__
#define __WCD_MBHC_ADC_H__

#include "wcd-mbhc-v2.h"

enum wcd_mbhc_adc_mux_ctl {
	MUX_CTL_AUTO = 0,
	MUX_CTL_IN2P,
	MUX_CTL_IN3P,
	MUX_CTL_IN4P,
	MUX_CTL_HPH_L,
	MUX_CTL_HPH_R,
	MUX_CTL_NONE,
};

#if IS_ENABLED(CONFIG_SND_SOC_WCD_MBHC_ADC)
void wcd_mbhc_adc_init(struct wcd_mbhc *mbhc);
#else
static inline void wcd_mbhc_adc_init(struct wcd_mbhc *mbhc)
{

}
#endif
#endif /* __WCD_MBHC_ADC_H__ */
