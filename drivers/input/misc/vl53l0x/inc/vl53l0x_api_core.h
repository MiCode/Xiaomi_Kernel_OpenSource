/*
 *  vl53l0x_api_core.h - Linux kernel modules for
 *  STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef _VL_API_CORE_H_
#define _VL_API_CORE_H_

#include "vl53l0x_def.h"
#include "vl53l0x_platform.h"


#ifdef __cplusplus
extern "C" {
#endif


int8_t VL_reverse_bytes(uint8_t *data, uint32_t size);

int8_t VL_measurement_poll_for_completion(struct vl_data *Dev);

uint8_t VL_encode_vcsel_period(uint8_t vcsel_period_pclks);

uint8_t VL_decode_vcsel_period(uint8_t vcsel_period_reg);

uint32_t VL_isqrt(uint32_t num);

uint32_t VL_quadrature_sum(uint32_t a, uint32_t b);

int8_t VL_get_info_from_device(struct vl_data *Dev, uint8_t option);

int8_t VL_set_vcsel_pulse_period(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t VCSELPulsePeriodPCLK);

int8_t VL_get_vcsel_pulse_period(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t *pVCSELPulsePeriodPCLK);

uint32_t VL_decode_timeout(uint16_t encoded_timeout);

int8_t get_sequence_step_timeout(struct vl_data *Dev,
			uint8_t SequenceStepId,
			uint32_t *pTimeOutMicroSecs);

int8_t set_sequence_step_timeout(struct vl_data *Dev,
			uint8_t SequenceStepId,
			uint32_t TimeOutMicroSecs);

int8_t VL_set_measurement_timing_budget_micro_seconds(
	struct vl_data *Dev, uint32_t MeasurementTimingBudgetMicroSeconds);

int8_t VL_get_measurement_timing_budget_micro_seconds(
	struct vl_data *Dev, uint32_t *pMeasurementTimingBudgetMicroSeconds);

int8_t VL_load_tuning_settings(struct vl_data *Dev,
		uint8_t *pTuningSettingBuffer);

int8_t VL_calc_sigma_estimate(struct vl_data *Dev,
		struct VL_RangingMeasurementData_t *pRangingMeasurementData,
		unsigned int *pSigmaEstimate, uint32_t *pDmax_mm);

int8_t VL_get_total_xtalk_rate(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData,
	unsigned int *ptotal_xtalk_rate_mcps);

int8_t VL_get_total_signal_rate(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData,
	unsigned int *ptotal_signal_rate_mcps);

int8_t VL_get_pal_range_status(struct vl_data *Dev,
		 uint8_t DeviceRangeStatus,
		 unsigned int SignalRate,
		 uint16_t EffectiveSpadRtnCount,
		 struct VL_RangingMeasurementData_t *pRangingMeasurementData,
		 uint8_t *pPalRangeStatus);

uint32_t VL_calc_timeout_mclks(struct vl_data *Dev,
	uint32_t timeout_period_us, uint8_t vcsel_period_pclks);

uint16_t VL_encode_timeout(uint32_t timeout_macro_clks);

#ifdef __cplusplus
}
#endif

#endif /* _VL_API_CORE_H_ */
