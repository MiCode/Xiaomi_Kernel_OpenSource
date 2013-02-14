/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _M_ADC_PROC_H
#define _M_ADC_PROC_H

#include <linux/msm_adc.h>
int32_t tdkntcgtherm(int32_t adc_code, const struct adc_properties *,
		const struct chan_properties *, struct adc_chan_result *);
int32_t scale_default(int32_t adc_code, const struct adc_properties *,
		const struct chan_properties *, struct adc_chan_result *);
int32_t scale_msm_therm(int32_t adc_code, const struct adc_properties *,
		const struct chan_properties *, struct adc_chan_result *);
int32_t scale_batt_therm(int32_t adc_code, const struct adc_properties *,
		const struct chan_properties *, struct adc_chan_result *);
int32_t scale_pmic_therm(int32_t adc_code, const struct adc_properties *,
		const struct chan_properties *, struct adc_chan_result *);
int32_t scale_xtern_chgr_cur(int32_t adc_code, const struct adc_properties *,
		const struct chan_properties *, struct adc_chan_result *);
#endif /* _M_ADC_PROC_H */
