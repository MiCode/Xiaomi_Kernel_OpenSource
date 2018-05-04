/*
 *  vl53l0x_api_string.c - Linux kernel modules for
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


#include "vl53l0x_api.h"
#include "vl53l0x_api_core.h"
#include "vl53l0x_api_strings.h"

#ifndef __KERNEL__
#include <stdlib.h>
#endif

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)


int8_t VL_check_part_used(struct vl_data *Dev,
		uint8_t *Revision,
		struct VL_DeviceInfo_t *pVL_DeviceInfo)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t ModuleIdInt;
	char *ProductId_tmp;

	LOG_FUNCTION_START("");

	Status = VL_get_info_from_device(Dev, 2);

	if (Status == VL_ERROR_NONE) {
		ModuleIdInt = VL_GETDEVICESPECIFICPARAMETER(Dev, ModuleId);

	if (ModuleIdInt == 0) {
		*Revision = 0;
		VL_COPYSTRING(pVL_DeviceInfo->ProductId, "");
	} else {
		*Revision = VL_GETDEVICESPECIFICPARAMETER(Dev, Revision);
		ProductId_tmp = VL_GETDEVICESPECIFICPARAMETER(Dev,
			ProductId);
		VL_COPYSTRING(pVL_DeviceInfo->ProductId,
			ProductId_tmp);
	}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


int8_t VL_get_device_info(struct vl_data *Dev,
				struct VL_DeviceInfo_t *pVL_DeviceInfo)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t revision_id;
	uint8_t Revision;

	Status = VL_check_part_used(Dev, &Revision, pVL_DeviceInfo);

	if (Status == VL_ERROR_NONE) {
		if (Revision == 0) {
			VL_COPYSTRING(pVL_DeviceInfo->Name,
					VL_STRING_DEVICE_INFO_NAME_TS0);
		} else if ((Revision <= 34) && (Revision != 32)) {
			VL_COPYSTRING(pVL_DeviceInfo->Name,
					VL_STRING_DEVICE_INFO_NAME_TS1);
		} else if (Revision < 39) {
			VL_COPYSTRING(pVL_DeviceInfo->Name,
					VL_STRING_DEVICE_INFO_NAME_TS2);
		} else {
			VL_COPYSTRING(pVL_DeviceInfo->Name,
					VL_STRING_DEVICE_INFO_NAME_ES1);
		}

		VL_COPYSTRING(pVL_DeviceInfo->Type,
				VL_STRING_DEVICE_INFO_TYPE);

	}

	if (Status == VL_ERROR_NONE) {
		Status = VL_RdByte(Dev,
			VL_REG_IDENTIFICATION_MODEL_ID,
			&pVL_DeviceInfo->ProductType);
	}

	if (Status == VL_ERROR_NONE) {
		Status = VL_RdByte(Dev,
			VL_REG_IDENTIFICATION_REVISION_ID,
				&revision_id);
		pVL_DeviceInfo->ProductRevisionMajor = 1;
		pVL_DeviceInfo->ProductRevisionMinor =
					(revision_id & 0xF0) >> 4;
	}

	return Status;
}


int8_t VL_get_device_error_string(uint8_t ErrorCode,
		char *pDeviceErrorString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	switch (ErrorCode) {
	case VL_DEVICEERROR_NONE:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_NONE);
	break;
	case VL_DEVICEERROR_VCSELCONTINUITYTESTFAILURE:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_VCSELCONTINUITYTESTFAILURE);
	break;
	case VL_DEVICEERROR_VCSELWATCHDOGTESTFAILURE:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_VCSELWATCHDOGTESTFAILURE);
	break;
	case VL_DEVICEERROR_NOVHVVALUEFOUND:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_NOVHVVALUEFOUND);
	break;
	case VL_DEVICEERROR_MSRCNOTARGET:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_MSRCNOTARGET);
	break;
	case VL_DEVICEERROR_SNRCHECK:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_SNRCHECK);
	break;
	case VL_DEVICEERROR_RANGEPHASECHECK:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_RANGEPHASECHECK);
	break;
	case VL_DEVICEERROR_SIGMATHRESHOLDCHECK:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_SIGMATHRESHOLDCHECK);
	break;
	case VL_DEVICEERROR_TCC:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_TCC);
	break;
	case VL_DEVICEERROR_PHASECONSISTENCY:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_PHASECONSISTENCY);
	break;
	case VL_DEVICEERROR_MINCLIP:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_MINCLIP);
	break;
	case VL_DEVICEERROR_RANGECOMPLETE:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_RANGECOMPLETE);
	break;
	case VL_DEVICEERROR_ALGOUNDERFLOW:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_ALGOUNDERFLOW);
	break;
	case VL_DEVICEERROR_ALGOOVERFLOW:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_ALGOOVERFLOW);
	break;
	case VL_DEVICEERROR_RANGEIGNORETHRESHOLD:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_DEVICEERROR_RANGEIGNORETHRESHOLD);
	break;

	default:
		VL_COPYSTRING(pDeviceErrorString,
			VL_STRING_UNKNOWN_ERROR_CODE);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_range_status_string(uint8_t RangeStatus,
		char *pRangeStatusString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	switch (RangeStatus) {
	case 0:
		VL_COPYSTRING(pRangeStatusString,
			VL_STRING_RANGESTATUS_RANGEVALID);
	break;
	case 1:
		VL_COPYSTRING(pRangeStatusString,
			VL_STRING_RANGESTATUS_SIGMA);
	break;
	case 2:
		VL_COPYSTRING(pRangeStatusString,
			VL_STRING_RANGESTATUS_SIGNAL);
	break;
	case 3:
		VL_COPYSTRING(pRangeStatusString,
			VL_STRING_RANGESTATUS_MINRANGE);
	break;
	case 4:
		VL_COPYSTRING(pRangeStatusString,
			VL_STRING_RANGESTATUS_PHASE);
	break;
	case 5:
		VL_COPYSTRING(pRangeStatusString,
			VL_STRING_RANGESTATUS_HW);
	break;

	default: /**/
		VL_COPYSTRING(pRangeStatusString,
				VL_STRING_RANGESTATUS_NONE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_pal_error_string(int8_t PalErrorCode,
		char *pPalErrorString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	switch (PalErrorCode) {
	case VL_ERROR_NONE:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_NONE);
	break;
	case VL_ERROR_CALIBRATION_WARNING:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_CALIBRATION_WARNING);
	break;
	case VL_ERROR_MIN_CLIPPED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_MIN_CLIPPED);
	break;
	case VL_ERROR_UNDEFINED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_UNDEFINED);
	break;
	case VL_ERROR_INVALID_PARAMS:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_INVALID_PARAMS);
	break;
	case VL_ERROR_NOT_SUPPORTED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_NOT_SUPPORTED);
	break;
	case VL_ERROR_INTERRUPT_NOT_CLEARED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_INTERRUPT_NOT_CLEARED);
	break;
	case VL_ERROR_RANGE_ERROR:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_RANGE_ERROR);
	break;
	case VL_ERROR_TIME_OUT:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_TIME_OUT);
	break;
	case VL_ERROR_MODE_NOT_SUPPORTED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_MODE_NOT_SUPPORTED);
	break;
	case VL_ERROR_BUFFER_TOO_SMALL:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_BUFFER_TOO_SMALL);
	break;
	case VL_ERROR_GPIO_NOT_EXISTING:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_GPIO_NOT_EXISTING);
	break;
	case VL_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED);
	break;
	case VL_ERROR_CONTROL_INTERFACE:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_CONTROL_INTERFACE);
	break;
	case VL_ERROR_INVALID_COMMAND:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_INVALID_COMMAND);
	break;
	case VL_ERROR_DIVISION_BY_ZERO:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_DIVISION_BY_ZERO);
	break;
	case VL_ERROR_REF_SPAD_INIT:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_REF_SPAD_INIT);
	break;
	case VL_ERROR_NOT_IMPLEMENTED:
		VL_COPYSTRING(pPalErrorString,
			VL_STRING_ERROR_NOT_IMPLEMENTED);
	break;

	default:
		VL_COPYSTRING(pPalErrorString,
				VL_STRING_UNKNOWN_ERROR_CODE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_pal_state_string(uint8_t PalStateCode,
		char *pPalStateString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	switch (PalStateCode) {
	case VL_STATE_POWERDOWN:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_POWERDOWN);
	break;
	case VL_STATE_WAIT_STATICINIT:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_WAIT_STATICINIT);
	break;
	case VL_STATE_STANDBY:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_STANDBY);
	break;
	case VL_STATE_IDLE:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_IDLE);
	break;
	case VL_STATE_RUNNING:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_RUNNING);
	break;
	case VL_STATE_UNKNOWN:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_UNKNOWN);
	break;
	case VL_STATE_ERROR:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_ERROR);
	break;

	default:
		VL_COPYSTRING(pPalStateString,
			VL_STRING_STATE_UNKNOWN);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_sequence_steps_info(
		uint8_t SequenceStepId,
		char *pSequenceStepsString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	switch (SequenceStepId) {
	case VL_SEQUENCESTEP_TCC:
		VL_COPYSTRING(pSequenceStepsString,
			VL_STRING_SEQUENCESTEP_TCC);
	break;
	case VL_SEQUENCESTEP_DSS:
		VL_COPYSTRING(pSequenceStepsString,
			VL_STRING_SEQUENCESTEP_DSS);
	break;
	case VL_SEQUENCESTEP_MSRC:
		VL_COPYSTRING(pSequenceStepsString,
			VL_STRING_SEQUENCESTEP_MSRC);
	break;
	case VL_SEQUENCESTEP_PRE_RANGE:
		VL_COPYSTRING(pSequenceStepsString,
			VL_STRING_SEQUENCESTEP_PRE_RANGE);
	break;
	case VL_SEQUENCESTEP_FINAL_RANGE:
		VL_COPYSTRING(pSequenceStepsString,
			VL_STRING_SEQUENCESTEP_FINAL_RANGE);
	break;

	default:
		Status = VL_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);

	return Status;
}


int8_t VL_get_limit_check_info(struct vl_data *Dev,
	uint16_t LimitCheckId, char *pLimitCheckString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	switch (LimitCheckId) {
	case VL_CHECKENABLE_SIGMA_FINAL_RANGE:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_CHECKENABLE_SIGMA_FINAL_RANGE);
	break;
	case VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE);
	break;
	case VL_CHECKENABLE_SIGNAL_REF_CLIP:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_CHECKENABLE_SIGNAL_REF_CLIP);
	break;
	case VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_CHECKENABLE_RANGE_IGNORE_THRESHOLD);
	break;

	case VL_CHECKENABLE_SIGNAL_RATE_MSRC:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_CHECKENABLE_SIGNAL_RATE_MSRC);
	break;

	case VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_CHECKENABLE_SIGNAL_RATE_PRE_RANGE);
	break;

	default:
		VL_COPYSTRING(pLimitCheckString,
			VL_STRING_UNKNOWN_ERROR_CODE);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}
