/*
 *  vl53l0x_api_calibration.h - Linux kernel modules for
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

#ifndef _VL_API_CALIBRATION_H_
#define _VL_API_CALIBRATION_H_

#include "vl53l0x_def.h"
#include "vl53l0x_platform.h"


#ifdef __cplusplus
extern "C" {
#endif

int8_t VL_perform_xtalk_calibration(struct vl_data *Dev,
		unsigned int XTalkCalDistance,
		unsigned int *pXTalkCompensationRateMegaCps);

int8_t VL_perform_offset_calibration(struct vl_data *Dev,
		unsigned int CalDistanceMilliMeter,
		int32_t *pOffsetMicroMeter);

int8_t VL_set_offset_calibration_data_micro_meter(struct vl_data *Dev,
		int32_t OffsetCalibrationDataMicroMeter);

int8_t VL_get_offset_calibration_data_micro_meter(struct vl_data *Dev,
		int32_t *pOffsetCalibrationDataMicroMeter);

int8_t VL_apply_offset_adjustment(struct vl_data *Dev);

int8_t VL_perform_ref_spad_management(struct vl_data *Dev,
		uint32_t *refSpadCount, uint8_t *isApertureSpads);

int8_t VL_set_reference_spads(struct vl_data *Dev,
		uint32_t count, uint8_t isApertureSpads);

int8_t VL_get_reference_spads(struct vl_data *Dev,
		uint32_t *pSpadCount, uint8_t *pIsApertureSpads);

int8_t VL_perform_phase_calibration(struct vl_data *Dev,
	uint8_t *pPhaseCal, const uint8_t get_data_enable,
	const uint8_t restore_config);

int8_t VL_perform_ref_calibration(struct vl_data *Dev,
	uint8_t *pVhvSettings, uint8_t *pPhaseCal, uint8_t get_data_enable);

int8_t VL_set_ref_calibration(struct vl_data *Dev,
		uint8_t VhvSettings, uint8_t PhaseCal);

int8_t VL_get_ref_calibration(struct vl_data *Dev,
		uint8_t *pVhvSettings, uint8_t *pPhaseCal);




#ifdef __cplusplus
}
#endif

#endif /* _VL_API_CALIBRATION_H_ */
