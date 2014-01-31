/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_THERMAL_H
#define __MSM_THERMAL_H

struct msm_thermal_data {
	struct platform_device *pdev;
	uint32_t sensor_id;
	uint32_t poll_ms;
	int32_t limit_temp_degC;
	int32_t temp_hysteresis_degC;
	uint32_t bootup_freq_step;
	uint32_t bootup_freq_control_mask;
	int32_t core_limit_temp_degC;
	int32_t core_temp_hysteresis_degC;
	int32_t hotplug_temp_degC;
	int32_t hotplug_temp_hysteresis_degC;
	uint32_t core_control_mask;
	uint32_t freq_mitig_temp_degc;
	uint32_t freq_mitig_temp_hysteresis_degc;
	uint32_t freq_mitig_control_mask;
	uint32_t freq_limit;
	int32_t vdd_rstr_temp_degC;
	int32_t vdd_rstr_temp_hyst_degC;
	int32_t vdd_mx_min;
	int32_t psm_temp_degC;
	int32_t psm_temp_hyst_degC;
	int32_t ocr_temp_degC;
	int32_t ocr_temp_hyst_degC;
	uint32_t ocr_sensor_id;
	int32_t phase_rpm_resource_type;
	int32_t phase_rpm_resource_id;
	int32_t gfx_phase_warm_temp_degC;
	int32_t gfx_phase_warm_temp_hyst_degC;
	int32_t gfx_phase_hot_temp_degC;
	int32_t gfx_phase_hot_temp_hyst_degC;
	int32_t gfx_sensor;
	int32_t gfx_phase_request_key;
	int32_t cx_phase_hot_temp_degC;
	int32_t cx_phase_hot_temp_hyst_degC;
	int32_t cx_phase_request_key;
	int32_t vdd_mx_temp_degC;
	int32_t vdd_mx_temp_hyst_degC;
	int32_t therm_reset_temp_degC;
};

#ifdef CONFIG_THERMAL_MONITOR
extern int msm_thermal_init(struct msm_thermal_data *pdata);
extern int msm_thermal_device_init(void);
extern int msm_thermal_set_frequency(uint32_t cpu, uint32_t freq,
	bool is_max);
#else
static inline int msm_thermal_init(struct msm_thermal_data *pdata)
{
	return -ENOSYS;
}
static inline int msm_thermal_device_init(void)
{
	return -ENOSYS;
}
static inline int msm_thermal_set_frequency(uint32_t cpu, uint32_t freq,
	bool is_max)
{
	return -ENOSYS;
}
#endif

#endif /*__MSM_THERMAL_H*/
