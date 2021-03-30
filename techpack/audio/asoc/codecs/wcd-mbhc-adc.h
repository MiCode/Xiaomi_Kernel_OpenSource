/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */
#ifndef __WCD_MBHC_ADC_H__
#define __WCD_MBHC_ADC_H__

#include <asoc/wcd-mbhc-v2.h>

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
