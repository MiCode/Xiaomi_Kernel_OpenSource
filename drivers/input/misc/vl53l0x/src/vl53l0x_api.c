/*
 *  vl53l0x_api.c - Linux kernel modules for
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
#include "vl53l0x_tuning.h"
#include "vl53l0x_interrupt_threshold_settings.h"
#include "vl53l0x_api_core.h"
#include "vl53l0x_api_calibration.h"
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

#ifdef VL_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(TRACE_MODULE_API, \
	level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#endif

/* Group PAL General Functions */

int8_t VL_GetVersion(struct VL_Version_t *pVersion)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	pVersion->major = VL_IMPLEMENTATION_VER_MAJOR;
	pVersion->minor = VL_IMPLEMENTATION_VER_MINOR;
	pVersion->build = VL_IMPLEMENTATION_VER_SUB;

	pVersion->revision = VL_IMPLEMENTATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetPalSpecVersion(struct VL_Version_t *pPalSpecVersion)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	pPalSpecVersion->major = VL_SPECIFICATION_VER_MAJOR;
	pPalSpecVersion->minor = VL_SPECIFICATION_VER_MINOR;
	pPalSpecVersion->build = VL_SPECIFICATION_VER_SUB;

	pPalSpecVersion->revision = VL_SPECIFICATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetProductRevision(struct vl_data *Dev,
	uint8_t *pProductRevisionMajor, uint8_t *pProductRevisionMinor)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t revision_id;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_IDENTIFICATION_REVISION_ID,
		&revision_id);
	*pProductRevisionMajor = 1;
	*pProductRevisionMinor = (revision_id & 0xF0) >> 4;

	LOG_FUNCTION_END(Status);
	return Status;

}

int8_t VL_GetDeviceInfo(struct vl_data *Dev,
	struct VL_DeviceInfo_t *pVL_DeviceInfo)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_device_info(Dev, pVL_DeviceInfo);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetDeviceErrorStatus(struct vl_data *Dev,
	uint8_t *pDeviceErrorStatus)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t RangeStatus;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_RESULT_RANGE_STATUS,
		&RangeStatus);

	*pDeviceErrorStatus = (uint8_t)((RangeStatus & 0x78) >> 3);

	LOG_FUNCTION_END(Status);
	return Status;
}


int8_t VL_GetDeviceErrorString(uint8_t ErrorCode,
	char *pDeviceErrorString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_device_error_string(ErrorCode, pDeviceErrorString);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetRangeStatusString(uint8_t RangeStatus,
	char *pRangeStatusString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_range_status_string(RangeStatus,
		pRangeStatusString);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetPalErrorString(int8_t PalErrorCode,
	char *pPalErrorString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_pal_error_string(PalErrorCode, pPalErrorString);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetPalStateString(uint8_t PalStateCode,
	char *pPalStateString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_pal_state_string(PalStateCode, pPalStateString);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetPalState(struct vl_data *Dev, uint8_t *pPalState)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pPalState = PALDevDataGet(Dev, PalState);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetPowerMode(struct vl_data *Dev,
	uint8_t PowerMode)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	/* Only level1 of Power mode exists */
	if ((PowerMode != VL_POWERMODE_STANDBY_LEVEL1)
		&& (PowerMode != VL_POWERMODE_IDLE_LEVEL1)) {
		Status = VL_ERROR_MODE_NOT_SUPPORTED;
	} else if (PowerMode == VL_POWERMODE_STANDBY_LEVEL1) {
		/* set the standby level1 of power mode */
		Status = VL_WrByte(Dev, 0x80, 0x00);
		if (Status == VL_ERROR_NONE) {
			/* Set PAL State to standby */
			PALDevDataSet(Dev, PalState, VL_STATE_STANDBY);
			PALDevDataSet(Dev, PowerMode,
				VL_POWERMODE_STANDBY_LEVEL1);
		}

	} else {
		/* VL_POWERMODE_IDLE_LEVEL1 */
		Status = VL_WrByte(Dev, 0x80, 0x00);
		if (Status == VL_ERROR_NONE)
			Status = VL_StaticInit(Dev);

		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, PowerMode,
				VL_POWERMODE_IDLE_LEVEL1);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetPowerMode(struct vl_data *Dev,
	uint8_t *pPowerMode)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	/* Only level1 of Power mode exists */
	Status = VL_RdByte(Dev, 0x80, &Byte);

	if (Status == VL_ERROR_NONE) {
		if (Byte == 1) {
			PALDevDataSet(Dev, PowerMode,
				VL_POWERMODE_IDLE_LEVEL1);
		} else {
			PALDevDataSet(Dev, PowerMode,
				VL_POWERMODE_STANDBY_LEVEL1);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetOffsetCalibrationDataMicroMeter(struct vl_data *Dev,
	int32_t OffsetCalibrationDataMicroMeter)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_set_offset_calibration_data_micro_meter(Dev,
		OffsetCalibrationDataMicroMeter);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetOffsetCalibrationDataMicroMeter(struct vl_data *Dev,
	int32_t *pOffsetCalibrationDataMicroMeter)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_offset_calibration_data_micro_meter(Dev,
		pOffsetCalibrationDataMicroMeter);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetLinearityCorrectiveGain(struct vl_data *Dev,
	int16_t LinearityCorrectiveGain)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	if ((LinearityCorrectiveGain < 0) || (LinearityCorrectiveGain > 1000))
		Status = VL_ERROR_INVALID_PARAMS;
	else {
		PALDevDataSet(Dev, LinearityCorrectiveGain,
			LinearityCorrectiveGain);

		if (LinearityCorrectiveGain != 1000) {
			/* Disable FW Xtalk */
			Status = VL_WrWord(Dev,
			VL_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS, 0);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetLinearityCorrectiveGain(struct vl_data *Dev,
	uint16_t *pLinearityCorrectiveGain)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pLinearityCorrectiveGain = PALDevDataGet(Dev, LinearityCorrectiveGain);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetGroupParamHold(struct vl_data *Dev, uint8_t GroupParamHold)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetUpperLimitMilliMeter(struct vl_data *Dev,
	uint16_t *pUpperLimitMilliMeter)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetTotalSignalRate(struct vl_data *Dev,
	unsigned int *pTotalSignalRate)
{
	int8_t Status = VL_ERROR_NONE;
	struct VL_RangingMeasurementData_t LastRangeDataBuffer;

	LOG_FUNCTION_START("");

	LastRangeDataBuffer = PALDevDataGet(Dev, LastRangeMeasure);

	Status = VL_get_total_signal_rate(
		Dev, &LastRangeDataBuffer, pTotalSignalRate);

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL General Functions */

/* Group PAL Init Functions */
int8_t VL_SetDeviceAddress(struct vl_data *Dev, uint8_t DeviceAddress)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_WrByte(Dev, VL_REG_I2C_SLAVE_DEVICE_ADDRESS,
		DeviceAddress / 2);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_DataInit(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	struct VL_DeviceParameters_t CurrentParameters;
	int i;
	uint8_t StopVariable;

	LOG_FUNCTION_START("");

	/* by default the I2C is running at 1V8 if you want to change it you */
	/* need to include this define at compilation level. */
#ifdef USE_I2C_2V8
	Status = VL_UpdateByte(Dev,
		VL_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
		0xFE,
		0x01);
#endif

	/* Set I2C standard mode */
	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev, 0x88, 0x00);

	VL_SETDEVICESPECIFICPARAMETER(Dev, ReadDataFromDeviceDone, 0);

#ifdef USE_IQC_STATION
	if (Status == VL_ERROR_NONE)
		Status = VL_apply_offset_adjustment(Dev);
#endif

	/* Default value is 1000 for Linearity Corrective Gain */
	PALDevDataSet(Dev, LinearityCorrectiveGain, 1000);

	/* Dmax default Parameter */
	PALDevDataSet(Dev, DmaxCalRangeMilliMeter, 400);
	PALDevDataSet(Dev, DmaxCalSignalRateRtnMegaCps,
		(unsigned int)((0x00016B85))); /* 1.42 No Cover Glass*/

	/* Set Default static parameters */
	/* *set first temporary values 9.44MHz * 65536 = 618660 */
	VL_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz, 618660);

	/* Set Default XTalkCompensationRateMegaCps to 0  */
	VL_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps, 0);

	/* Get default parameters */
	Status = VL_GetDeviceParameters(Dev, &CurrentParameters);
	if (Status == VL_ERROR_NONE) {
		/* initialize PAL values */
		CurrentParameters.DeviceMode = VL_DEVICEMODE_SINGLE_RANGING;
		CurrentParameters.HistogramMode = VL_HISTOGRAMMODE_DISABLED;
		PALDevDataSet(Dev, CurrentParameters, CurrentParameters);
	}

	/* Sigma estimator variable */
	PALDevDataSet(Dev, SigmaEstRefArray, 100);
	PALDevDataSet(Dev, SigmaEstEffPulseWidth, 900);
	PALDevDataSet(Dev, SigmaEstEffAmbWidth, 500);
	PALDevDataSet(Dev, targetRefRate, 0x0A00); /* 20 MCPS in 9:7 format */

	/* Use internal default settings */
	PALDevDataSet(Dev, UseInternalTuningSettings, 1);

	Status |= VL_WrByte(Dev, 0x80, 0x01);
	Status |= VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_WrByte(Dev, 0x00, 0x00);
	Status |= VL_RdByte(Dev, 0x91, &StopVariable);
	PALDevDataSet(Dev, StopVariable, StopVariable);
	Status |= VL_WrByte(Dev, 0x00, 0x01);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);
	Status |= VL_WrByte(Dev, 0x80, 0x00);

	/* Enable all check */
	for (i = 0; i < VL_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
		if (Status == VL_ERROR_NONE)
			Status |= VL_SetLimitCheckEnable(Dev, i, 1);
		else
			break;

	}

	/* Disable the following checks */
	if (Status == VL_ERROR_NONE)
		Status = VL_SetLimitCheckEnable(Dev,
			VL_CHECKENABLE_SIGNAL_REF_CLIP, 0);

	if (Status == VL_ERROR_NONE)
		Status = VL_SetLimitCheckEnable(Dev,
			VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 0);

	if (Status == VL_ERROR_NONE)
		Status = VL_SetLimitCheckEnable(Dev,
			VL_CHECKENABLE_SIGNAL_RATE_MSRC, 0);

	if (Status == VL_ERROR_NONE)
		Status = VL_SetLimitCheckEnable(Dev,
			VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE, 0);

	/* Limit default values */
	if (Status == VL_ERROR_NONE) {
		Status = VL_SetLimitCheckValue(Dev,
			VL_CHECKENABLE_SIGMA_FINAL_RANGE,
				(unsigned int)(18 * 65536));
	}
	if (Status == VL_ERROR_NONE) {
		Status = VL_SetLimitCheckValue(Dev,
			VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				(unsigned int)(25 * 65536 / 100));
				/* 0.25 * 65536 */
	}

	if (Status == VL_ERROR_NONE) {
		Status = VL_SetLimitCheckValue(Dev,
			VL_CHECKENABLE_SIGNAL_REF_CLIP,
				(unsigned int)(35 * 65536));
	}

	if (Status == VL_ERROR_NONE) {
		Status = VL_SetLimitCheckValue(Dev,
			VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				(unsigned int)(0 * 65536));
	}

	if (Status == VL_ERROR_NONE) {

		PALDevDataSet(Dev, SequenceConfig, 0xFF);
		Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
			0xFF);

		/* Set PAL state to tell that we are waiting for call to */
		/* * VL_StaticInit */
		PALDevDataSet(Dev, PalState, VL_STATE_WAIT_STATICINIT);
	}

	if (Status == VL_ERROR_NONE)
		VL_SETDEVICESPECIFICPARAMETER(Dev, RefSpadsInitialised, 0);


	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetTuningSettingBuffer(struct vl_data *Dev,
	uint8_t *pTuningSettingBuffer, uint8_t UseInternalTuningSettings)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (UseInternalTuningSettings == 1) {
		/* Force use internal settings */
		PALDevDataSet(Dev, UseInternalTuningSettings, 1);
	} else {

		/* check that the first byte is not 0 */
		if (*pTuningSettingBuffer != 0) {
			PALDevDataSet(Dev, pTuningSettingsPointer,
				pTuningSettingBuffer);
			PALDevDataSet(Dev, UseInternalTuningSettings, 0);

		} else {
			Status = VL_ERROR_INVALID_PARAMS;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetTuningSettingBuffer(struct vl_data *Dev,
	uint8_t **ppTuningSettingBuffer, uint8_t *pUseInternalTuningSettings)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*ppTuningSettingBuffer = PALDevDataGet(Dev, pTuningSettingsPointer);
	*pUseInternalTuningSettings = PALDevDataGet(Dev,
		UseInternalTuningSettings);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_StaticInit(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	struct VL_DeviceParameters_t CurrentParameters = {0};
	uint8_t *pTuningSettingBuffer;
	uint16_t tempword = 0;
	uint8_t tempbyte = 0;
	uint8_t UseInternalTuningSettings = 0;
	uint32_t count = 0;
	uint8_t isApertureSpads = 0;
	uint32_t refSpadCount = 0;
	uint8_t ApertureSpads = 0;
	uint8_t vcselPulsePeriodPCLK;
	uint32_t seqTimeoutMicroSecs;

	LOG_FUNCTION_START("");

	Status = VL_get_info_from_device(Dev, 1);

	/* set the ref spad from NVM */
	count	= (uint32_t)VL_GETDEVICESPECIFICPARAMETER(Dev,
		ReferenceSpadCount);
	ApertureSpads = VL_GETDEVICESPECIFICPARAMETER(Dev,
		ReferenceSpadType);

	/* NVM value invalid */
	if ((ApertureSpads > 1) ||
		((ApertureSpads == 1) && (count > 32)) ||
		((ApertureSpads == 0) && (count > 12)))
		Status = VL_perform_ref_spad_management(Dev, &refSpadCount,
			&isApertureSpads);
	else
		Status = VL_set_reference_spads(Dev, count, ApertureSpads);


	/* Initialize tuning settings buffer to prevent compiler warning. */
	pTuningSettingBuffer = DefaultTuningSettings;

	if (Status == VL_ERROR_NONE) {
		UseInternalTuningSettings = PALDevDataGet(Dev,
			UseInternalTuningSettings);

		if (UseInternalTuningSettings == 0)
			pTuningSettingBuffer = PALDevDataGet(Dev,
				pTuningSettingsPointer);
		else
			pTuningSettingBuffer = DefaultTuningSettings;

	}

	if (Status == VL_ERROR_NONE)
		Status = VL_load_tuning_settings(Dev,
			pTuningSettingBuffer);


	/* Set interrupt config to new sample ready */
	if (Status == VL_ERROR_NONE) {
		Status = VL_SetGpioConfig(Dev, 0, 0,
		VL_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY,
		VL_INTERRUPTPOLARITY_LOW);
	}

	if (Status == VL_ERROR_NONE) {
		Status = VL_WrByte(Dev, 0xFF, 0x01);
		Status |= VL_RdWord(Dev, 0x84, &tempword);
		Status |= VL_WrByte(Dev, 0xFF, 0x00);
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz,
			VL_FIXPOINT412TOFIXPOINT1616(tempword));
	}

	/* After static init, some device parameters may be changed, */
	/* * so update them */
	if (Status == VL_ERROR_NONE)
		Status = VL_GetDeviceParameters(Dev, &CurrentParameters);


	if (Status == VL_ERROR_NONE) {
		Status = VL_GetFractionEnable(Dev, &tempbyte);
		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, RangeFractionalEnable, tempbyte);

	}

	if (Status == VL_ERROR_NONE)
		PALDevDataSet(Dev, CurrentParameters, CurrentParameters);


	/* read the sequence config and save it */
	if (Status == VL_ERROR_NONE) {
		Status = VL_RdByte(Dev,
		VL_REG_SYSTEM_SEQUENCE_CONFIG, &tempbyte);
		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, tempbyte);

	}

	/* Disable MSRC and TCC by default */
	if (Status == VL_ERROR_NONE)
		Status = VL_SetSequenceStepEnable(Dev,
					VL_SEQUENCESTEP_TCC, 0);


	if (Status == VL_ERROR_NONE)
		Status = VL_SetSequenceStepEnable(Dev,
		VL_SEQUENCESTEP_MSRC, 0);


	/* Set PAL State to standby */
	if (Status == VL_ERROR_NONE)
		PALDevDataSet(Dev, PalState, VL_STATE_IDLE);



	/* Store pre-range vcsel period */
	if (Status == VL_ERROR_NONE) {
		Status = VL_GetVcselPulsePeriod(
			Dev,
			VL_VCSEL_PERIOD_PRE_RANGE,
			&vcselPulsePeriodPCLK);
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETDEVICESPECIFICPARAMETER(
		Dev, PreRangeVcselPulsePeriod,
		vcselPulsePeriodPCLK);
	}

	/* Store final-range vcsel period */
	if (Status == VL_ERROR_NONE) {
		Status = VL_GetVcselPulsePeriod(
			Dev,
			VL_VCSEL_PERIOD_FINAL_RANGE,
			&vcselPulsePeriodPCLK);
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETDEVICESPECIFICPARAMETER(
		Dev, FinalRangeVcselPulsePeriod,
		vcselPulsePeriodPCLK);
	}

	/* Store pre-range timeout */
	if (Status == VL_ERROR_NONE) {
		Status = get_sequence_step_timeout(
			Dev,
			VL_SEQUENCESTEP_PRE_RANGE,
			&seqTimeoutMicroSecs);
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETDEVICESPECIFICPARAMETER(
			Dev,
			PreRangeTimeoutMicroSecs,
			seqTimeoutMicroSecs);
	}

	/* Store final-range timeout */
	if (Status == VL_ERROR_NONE) {
		Status = get_sequence_step_timeout(
			Dev,
			VL_SEQUENCESTEP_FINAL_RANGE,
			&seqTimeoutMicroSecs);
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETDEVICESPECIFICPARAMETER(
			Dev,
			FinalRangeTimeoutMicroSecs,
			seqTimeoutMicroSecs);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_WaitDeviceBooted(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_ResetDevice(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	/* Set reset bit */
	Status = VL_WrByte(Dev, VL_REG_SOFT_RESET_GO2_SOFT_RESET_N,
		0x00);

	/* Wait for some time */
	if (Status == VL_ERROR_NONE) {
		do {
			Status = VL_RdByte(Dev,
			VL_REG_IDENTIFICATION_MODEL_ID, &Byte);
		} while (Byte != 0x00);
	}

	VL_PollingDelay(Dev);

	/* Release reset */
	Status = VL_WrByte(Dev, VL_REG_SOFT_RESET_GO2_SOFT_RESET_N,
		0x01);

	/* Wait until correct boot-up of the device */
	if (Status == VL_ERROR_NONE) {
		do {
			Status = VL_RdByte(Dev,
			VL_REG_IDENTIFICATION_MODEL_ID, &Byte);
		} while (Byte == 0x00);
	}

	VL_PollingDelay(Dev);

	/* Set PAL State to VL_STATE_POWERDOWN */
	if (Status == VL_ERROR_NONE)
		PALDevDataSet(Dev, PalState, VL_STATE_POWERDOWN);


	LOG_FUNCTION_END(Status);
	return Status;
}
/* End Group PAL Init Functions */

/* Group PAL Parameters Functions */
int8_t VL_SetDeviceParameters(struct vl_data *Dev,
	const struct VL_DeviceParameters_t *pDeviceParameters)
{
	int8_t Status = VL_ERROR_NONE;
	int i;

	LOG_FUNCTION_START("");
	Status = VL_SetDeviceMode(Dev, pDeviceParameters->DeviceMode);

	if (Status == VL_ERROR_NONE)
		Status = VL_SetInterMeasurementPeriodMilliSeconds(Dev,
			pDeviceParameters->InterMeasurementPeriodMilliSeconds);


	if (Status == VL_ERROR_NONE)
		Status = VL_SetXTalkCompensationRateMegaCps(Dev,
			pDeviceParameters->XTalkCompensationRateMegaCps);


	if (Status == VL_ERROR_NONE)
		Status = VL_SetOffsetCalibrationDataMicroMeter(Dev,
			pDeviceParameters->RangeOffsetMicroMeters);


	for (i = 0; i < VL_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
		if (Status == VL_ERROR_NONE)
			Status |= VL_SetLimitCheckEnable(Dev, i,
				pDeviceParameters->LimitChecksEnable[i]);
		else
			break;

		if (Status == VL_ERROR_NONE)
			Status |= VL_SetLimitCheckValue(Dev, i,
				pDeviceParameters->LimitChecksValue[i]);
		else
			break;

	}

	if (Status == VL_ERROR_NONE)
		Status = VL_SetWrapAroundCheckEnable(Dev,
			pDeviceParameters->WrapAroundCheckEnable);

	if (Status == VL_ERROR_NONE)
		Status = VL_SetMeasurementTimingBudgetMicroSeconds(Dev,
			pDeviceParameters->MeasurementTimingBudgetMicroSeconds);


	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetDeviceParameters(struct vl_data *Dev,
	struct VL_DeviceParameters_t *pDeviceParameters)
{
	int8_t Status = VL_ERROR_NONE;
	int i;

	LOG_FUNCTION_START("");

	Status = VL_GetDeviceMode(Dev, &(pDeviceParameters->DeviceMode));

	if (Status == VL_ERROR_NONE)
		Status = VL_GetInterMeasurementPeriodMilliSeconds(Dev,
		&(pDeviceParameters->InterMeasurementPeriodMilliSeconds));


	if (Status == VL_ERROR_NONE)
		pDeviceParameters->XTalkCompensationEnable = 0;

	if (Status == VL_ERROR_NONE)
		Status = VL_GetXTalkCompensationRateMegaCps(Dev,
			&(pDeviceParameters->XTalkCompensationRateMegaCps));


	if (Status == VL_ERROR_NONE)
		Status = VL_GetOffsetCalibrationDataMicroMeter(Dev,
			&(pDeviceParameters->RangeOffsetMicroMeters));


	if (Status == VL_ERROR_NONE) {
		for (i = 0; i < VL_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
			/* get first the values, then the enables.
			 * VL_GetLimitCheckValue will modify the enable
			 * flags
			 */
			if (Status == VL_ERROR_NONE) {
				Status |= VL_GetLimitCheckValue(Dev, i,
				&(pDeviceParameters->LimitChecksValue[i]));
			} else {
				break;
			}
			if (Status == VL_ERROR_NONE) {
				Status |= VL_GetLimitCheckEnable(Dev, i,
				&(pDeviceParameters->LimitChecksEnable[i]));
			} else {
				break;
			}
		}
	}

	if (Status == VL_ERROR_NONE) {
		Status = VL_GetWrapAroundCheckEnable(Dev,
			&(pDeviceParameters->WrapAroundCheckEnable));
	}

	/* Need to be done at the end as it uses VCSELPulsePeriod */
	if (Status == VL_ERROR_NONE) {
		Status = VL_GetMeasurementTimingBudgetMicroSeconds(Dev,
		&(pDeviceParameters->MeasurementTimingBudgetMicroSeconds));
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetDeviceMode(struct vl_data *Dev,
	uint8_t DeviceMode)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("%d", (int)DeviceMode);

	switch (DeviceMode) {
	case VL_DEVICEMODE_SINGLE_RANGING:
	case VL_DEVICEMODE_CONTINUOUS_RANGING:
	case VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING:
	case VL_DEVICEMODE_GPIO_DRIVE:
	case VL_DEVICEMODE_GPIO_OSC:
		/* Supported modes */
		VL_SETPARAMETERFIELD(Dev, DeviceMode, DeviceMode);
		break;
	default:
		/* Unsupported mode */
		Status = VL_ERROR_MODE_NOT_SUPPORTED;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetDeviceMode(struct vl_data *Dev,
	uint8_t *pDeviceMode)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	VL_GETPARAMETERFIELD(Dev, DeviceMode, *pDeviceMode);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetRangeFractionEnable(struct vl_data *Dev,	uint8_t Enable)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("%d", (int)Enable);

	Status = VL_WrByte(Dev, VL_REG_SYSTEM_RANGE_CONFIG, Enable);

	if (Status == VL_ERROR_NONE)
		PALDevDataSet(Dev, RangeFractionalEnable, Enable);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetFractionEnable(struct vl_data *Dev, uint8_t *pEnabled)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_SYSTEM_RANGE_CONFIG, pEnabled);

	if (Status == VL_ERROR_NONE)
		*pEnabled = (*pEnabled & 1);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetHistogramMode(struct vl_data *Dev,
	uint8_t HistogramMode)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetHistogramMode(struct vl_data *Dev,
	uint8_t *pHistogramMode)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetMeasurementTimingBudgetMicroSeconds(struct vl_data *Dev,
	uint32_t MeasurementTimingBudgetMicroSeconds)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_set_measurement_timing_budget_micro_seconds(Dev,
		MeasurementTimingBudgetMicroSeconds);

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_GetMeasurementTimingBudgetMicroSeconds(struct vl_data *Dev,
	uint32_t *pMeasurementTimingBudgetMicroSeconds)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_measurement_timing_budget_micro_seconds(Dev,
		pMeasurementTimingBudgetMicroSeconds);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetVcselPulsePeriod(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t VCSELPulsePeriodPCLK)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_set_vcsel_pulse_period(Dev, VcselPeriodType,
		VCSELPulsePeriodPCLK);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetVcselPulsePeriod(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t *pVCSELPulsePeriodPCLK)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_vcsel_pulse_period(Dev, VcselPeriodType,
		pVCSELPulsePeriodPCLK);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetSequenceStepEnable(struct vl_data *Dev,
	uint8_t SequenceStepId, uint8_t SequenceStepEnabled)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SequenceConfig = 0;
	uint8_t SequenceConfigNew = 0;
	uint32_t MeasurementTimingBudgetMicroSeconds;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
		&SequenceConfig);

	SequenceConfigNew = SequenceConfig;

	if (Status == VL_ERROR_NONE) {
		if (SequenceStepEnabled == 1) {

			/* Enable requested sequence step
			 */
			switch (SequenceStepId) {
			case VL_SEQUENCESTEP_TCC:
				SequenceConfigNew |= 0x10;
				break;
			case VL_SEQUENCESTEP_DSS:
				SequenceConfigNew |= 0x28;
				break;
			case VL_SEQUENCESTEP_MSRC:
				SequenceConfigNew |= 0x04;
				break;
			case VL_SEQUENCESTEP_PRE_RANGE:
				SequenceConfigNew |= 0x40;
				break;
			case VL_SEQUENCESTEP_FINAL_RANGE:
				SequenceConfigNew |= 0x80;
				break;
			default:
				Status = VL_ERROR_INVALID_PARAMS;
			}
		} else {
			/* Disable requested sequence step
			 */
			switch (SequenceStepId) {
			case VL_SEQUENCESTEP_TCC:
				SequenceConfigNew &= 0xef;
				break;
			case VL_SEQUENCESTEP_DSS:
				SequenceConfigNew &= 0xd7;
				break;
			case VL_SEQUENCESTEP_MSRC:
				SequenceConfigNew &= 0xfb;
				break;
			case VL_SEQUENCESTEP_PRE_RANGE:
				SequenceConfigNew &= 0xbf;
				break;
			case VL_SEQUENCESTEP_FINAL_RANGE:
				SequenceConfigNew &= 0x7f;
				break;
			default:
				Status = VL_ERROR_INVALID_PARAMS;
			}
		}
	}

	if (SequenceConfigNew != SequenceConfig) {
		/* Apply New Setting */
		if (Status == VL_ERROR_NONE) {
			Status = VL_WrByte(Dev,
			VL_REG_SYSTEM_SEQUENCE_CONFIG, SequenceConfigNew);
		}
		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, SequenceConfigNew);


		/* Recalculate timing budget */
		if (Status == VL_ERROR_NONE) {
			VL_GETPARAMETERFIELD(Dev,
				MeasurementTimingBudgetMicroSeconds,
				MeasurementTimingBudgetMicroSeconds);

			VL_SetMeasurementTimingBudgetMicroSeconds(Dev,
				MeasurementTimingBudgetMicroSeconds);
		}
	}

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t sequence_step_enabled(struct vl_data *Dev,
	uint8_t SequenceStepId, uint8_t SequenceConfig,
	uint8_t *pSequenceStepEnabled)
{
	int8_t Status = VL_ERROR_NONE;
	*pSequenceStepEnabled = 0;

	LOG_FUNCTION_START("");

	switch (SequenceStepId) {
	case VL_SEQUENCESTEP_TCC:
		*pSequenceStepEnabled = (SequenceConfig & 0x10) >> 4;
		break;
	case VL_SEQUENCESTEP_DSS:
		*pSequenceStepEnabled = (SequenceConfig & 0x08) >> 3;
		break;
	case VL_SEQUENCESTEP_MSRC:
		*pSequenceStepEnabled = (SequenceConfig & 0x04) >> 2;
		break;
	case VL_SEQUENCESTEP_PRE_RANGE:
		*pSequenceStepEnabled = (SequenceConfig & 0x40) >> 6;
		break;
	case VL_SEQUENCESTEP_FINAL_RANGE:
		*pSequenceStepEnabled = (SequenceConfig & 0x80) >> 7;
		break;
	default:
		Status = VL_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetSequenceStepEnable(struct vl_data *Dev,
	uint8_t SequenceStepId, uint8_t *pSequenceStepEnabled)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SequenceConfig = 0;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
		&SequenceConfig);

	if (Status == VL_ERROR_NONE) {
		Status = sequence_step_enabled(Dev, SequenceStepId,
			SequenceConfig, pSequenceStepEnabled);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetSequenceStepEnables(struct vl_data *Dev,
	struct VL_SchedulerSequenceSteps_t *pSchedulerSequenceSteps)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SequenceConfig = 0;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
		&SequenceConfig);

	if (Status == VL_ERROR_NONE) {
		Status = sequence_step_enabled(Dev,
		VL_SEQUENCESTEP_TCC, SequenceConfig,
			&pSchedulerSequenceSteps->TccOn);
	}
	if (Status == VL_ERROR_NONE) {
		Status = sequence_step_enabled(Dev,
		VL_SEQUENCESTEP_DSS, SequenceConfig,
			&pSchedulerSequenceSteps->DssOn);
	}
	if (Status == VL_ERROR_NONE) {
		Status = sequence_step_enabled(Dev,
		VL_SEQUENCESTEP_MSRC, SequenceConfig,
			&pSchedulerSequenceSteps->MsrcOn);
	}
	if (Status == VL_ERROR_NONE) {
		Status = sequence_step_enabled(Dev,
		VL_SEQUENCESTEP_PRE_RANGE, SequenceConfig,
			&pSchedulerSequenceSteps->PreRangeOn);
	}
	if (Status == VL_ERROR_NONE) {
		Status = sequence_step_enabled(Dev,
		VL_SEQUENCESTEP_FINAL_RANGE, SequenceConfig,
			&pSchedulerSequenceSteps->FinalRangeOn);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetNumberOfSequenceSteps(struct vl_data *Dev,
	uint8_t *pNumberOfSequenceSteps)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pNumberOfSequenceSteps = VL_SEQUENCESTEP_NUMBER_OF_CHECKS;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetSequenceStepsInfo(
	uint8_t SequenceStepId, char *pSequenceStepsString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_sequence_steps_info(
			SequenceStepId,
			pSequenceStepsString);

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_SetSequenceStepTimeout(struct vl_data *Dev,
	uint8_t SequenceStepId, unsigned int TimeOutMilliSecs)
{
	int8_t Status = VL_ERROR_NONE;
	int8_t Status1 = VL_ERROR_NONE;
	uint32_t TimeoutMicroSeconds = ((TimeOutMilliSecs * 1000) + 0x8000)
		>> 16;
	uint32_t MeasurementTimingBudgetMicroSeconds;
	unsigned int OldTimeOutMicroSeconds;

	LOG_FUNCTION_START("");

	/* Read back the current value in case we need to revert back to this.
	 */
	Status = get_sequence_step_timeout(Dev, SequenceStepId,
		&OldTimeOutMicroSeconds);

	if (Status == VL_ERROR_NONE) {
		Status = set_sequence_step_timeout(Dev, SequenceStepId,
			TimeoutMicroSeconds);
	}

	if (Status == VL_ERROR_NONE) {
		VL_GETPARAMETERFIELD(Dev,
			MeasurementTimingBudgetMicroSeconds,
			MeasurementTimingBudgetMicroSeconds);

		/* At this point we don't know if the requested */
		/* value is valid, */
		/* therefore proceed to update the entire timing budget and */
		/* if this fails, revert back to the previous value. */
		Status = VL_SetMeasurementTimingBudgetMicroSeconds(Dev,
			MeasurementTimingBudgetMicroSeconds);

		if (Status != VL_ERROR_NONE) {
			Status1 = set_sequence_step_timeout(Dev, SequenceStepId,
				OldTimeOutMicroSeconds);

			if (Status1 == VL_ERROR_NONE) {
				Status1 =
				VL_SetMeasurementTimingBudgetMicroSeconds(
					Dev,
					MeasurementTimingBudgetMicroSeconds);
			}

			Status = Status1;
		}
	}

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_GetSequenceStepTimeout(struct vl_data *Dev,
	uint8_t SequenceStepId,
	unsigned int *pTimeOutMilliSecs)
{
	int8_t Status = VL_ERROR_NONE;
	uint32_t TimeoutMicroSeconds;

	LOG_FUNCTION_START("");

	Status = get_sequence_step_timeout(Dev, SequenceStepId,
		&TimeoutMicroSeconds);
	if (Status == VL_ERROR_NONE) {
		TimeoutMicroSeconds <<= 8;
		*pTimeOutMilliSecs = (TimeoutMicroSeconds + 500)/1000;
		*pTimeOutMilliSecs <<= 8;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetInterMeasurementPeriodMilliSeconds(struct vl_data *Dev,
	uint32_t InterMeasurementPeriodMilliSeconds)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t osc_calibrate_val;
	uint32_t IMPeriodMilliSeconds;

	LOG_FUNCTION_START("");

	Status = VL_RdWord(Dev, VL_REG_OSC_CALIBRATE_VAL,
		&osc_calibrate_val);

	if (Status == VL_ERROR_NONE) {
		if (osc_calibrate_val != 0) {
			IMPeriodMilliSeconds =
				InterMeasurementPeriodMilliSeconds
					* osc_calibrate_val;
		} else {
			IMPeriodMilliSeconds =
				InterMeasurementPeriodMilliSeconds;
		}
		Status = VL_WrDWord(Dev,
		VL_REG_SYSTEM_INTERMEASUREMENT_PERIOD,
			IMPeriodMilliSeconds);
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETPARAMETERFIELD(Dev,
			InterMeasurementPeriodMilliSeconds,
			InterMeasurementPeriodMilliSeconds);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetInterMeasurementPeriodMilliSeconds(struct vl_data *Dev,
	uint32_t *pInterMeasurementPeriodMilliSeconds)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t osc_calibrate_val;
	uint32_t IMPeriodMilliSeconds;

	LOG_FUNCTION_START("");

	Status = VL_RdWord(Dev, VL_REG_OSC_CALIBRATE_VAL,
		&osc_calibrate_val);

	if (Status == VL_ERROR_NONE) {
		Status = VL_RdDWord(Dev,
		VL_REG_SYSTEM_INTERMEASUREMENT_PERIOD,
			&IMPeriodMilliSeconds);
	}

	if (Status == VL_ERROR_NONE) {
		if (osc_calibrate_val != 0) {
			*pInterMeasurementPeriodMilliSeconds =
				IMPeriodMilliSeconds / osc_calibrate_val;
		}
		VL_SETPARAMETERFIELD(Dev,
			InterMeasurementPeriodMilliSeconds,
			*pInterMeasurementPeriodMilliSeconds);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetXTalkCompensationEnable(struct vl_data *Dev,
	uint8_t XTalkCompensationEnable)
{
	int8_t Status = VL_ERROR_NONE;
	unsigned int TempFix1616;
	uint16_t LinearityCorrectiveGain;

	LOG_FUNCTION_START("");

	LinearityCorrectiveGain = PALDevDataGet(Dev, LinearityCorrectiveGain);

	if ((XTalkCompensationEnable == 0)
		|| (LinearityCorrectiveGain != 1000)) {
		TempFix1616 = 0;
	} else {
		VL_GETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
			TempFix1616);
	}

	/* the following register has a format 3.13 */
	Status = VL_WrWord(Dev,
	VL_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS,
		VL_FIXPOINT1616TOFIXPOINT313(TempFix1616));

	if (Status == VL_ERROR_NONE) {
		if (XTalkCompensationEnable == 0) {
			VL_SETPARAMETERFIELD(Dev, XTalkCompensationEnable,
				0);
		} else {
			VL_SETPARAMETERFIELD(Dev, XTalkCompensationEnable,
				1);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetXTalkCompensationEnable(struct vl_data *Dev,
	uint8_t *pXTalkCompensationEnable)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	VL_GETPARAMETERFIELD(Dev, XTalkCompensationEnable, Temp8);
	*pXTalkCompensationEnable = Temp8;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetXTalkCompensationRateMegaCps(struct vl_data *Dev,
	unsigned int XTalkCompensationRateMegaCps)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Temp8;
	uint16_t LinearityCorrectiveGain;
	uint16_t data;

	LOG_FUNCTION_START("");

	VL_GETPARAMETERFIELD(Dev, XTalkCompensationEnable, Temp8);
	LinearityCorrectiveGain = PALDevDataGet(Dev, LinearityCorrectiveGain);

	if (Temp8 == 0) { /* disabled write only internal value */
		VL_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
			XTalkCompensationRateMegaCps);
	} else {
		/* the following register has a format 3.13 */
		if (LinearityCorrectiveGain == 1000) {
			data = VL_FIXPOINT1616TOFIXPOINT313(
				XTalkCompensationRateMegaCps);
		} else {
			data = 0;
		}

		Status = VL_WrWord(Dev,
		VL_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS, data);

		if (Status == VL_ERROR_NONE) {
			VL_SETPARAMETERFIELD(Dev,
				XTalkCompensationRateMegaCps,
				XTalkCompensationRateMegaCps);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetXTalkCompensationRateMegaCps(struct vl_data *Dev,
	unsigned int *pXTalkCompensationRateMegaCps)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t Value;
	unsigned int TempFix1616;

	LOG_FUNCTION_START("");

	Status = VL_RdWord(Dev,
	VL_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS, (uint16_t *)&Value);
	if (Status == VL_ERROR_NONE) {
		if (Value == 0) {
			/* the Xtalk is disabled return value from memory */
			VL_GETPARAMETERFIELD(Dev,
				XTalkCompensationRateMegaCps, TempFix1616);
			*pXTalkCompensationRateMegaCps = TempFix1616;
			VL_SETPARAMETERFIELD(Dev, XTalkCompensationEnable,
				0);
		} else {
			TempFix1616 = VL_FIXPOINT313TOFIXPOINT1616(Value);
			*pXTalkCompensationRateMegaCps = TempFix1616;
			VL_SETPARAMETERFIELD(Dev,
				XTalkCompensationRateMegaCps, TempFix1616);
			VL_SETPARAMETERFIELD(Dev, XTalkCompensationEnable,
				1);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetRefCalibration(struct vl_data *Dev, uint8_t VhvSettings,
	uint8_t PhaseCal)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_set_ref_calibration(Dev, VhvSettings, PhaseCal);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetRefCalibration(struct vl_data *Dev, uint8_t *pVhvSettings,
	uint8_t *pPhaseCal)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_ref_calibration(Dev, pVhvSettings, pPhaseCal);

	LOG_FUNCTION_END(Status);
	return Status;
}

/*
 * CHECK LIMIT FUNCTIONS
 */

int8_t VL_GetNumberOfLimitCheck(uint16_t *pNumberOfLimitCheck)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pNumberOfLimitCheck = VL_CHECKENABLE_NUMBER_OF_CHECKS;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetLimitCheckInfo(struct vl_data *Dev, uint16_t LimitCheckId,
	char *pLimitCheckString)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_limit_check_info(Dev, LimitCheckId,
		pLimitCheckString);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetLimitCheckStatus(struct vl_data *Dev,
	uint16_t LimitCheckId, uint8_t *pLimitCheckStatus)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL_ERROR_INVALID_PARAMS;
	} else {

		VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
			LimitCheckId, Temp8);

		*pLimitCheckStatus = Temp8;

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetLimitCheckEnable(struct vl_data *Dev,
	uint16_t LimitCheckId, uint8_t LimitCheckEnable)
{
	int8_t Status = VL_ERROR_NONE;
	unsigned int TempFix1616 = 0;
	uint8_t LimitCheckEnableInt = 0;
	uint8_t LimitCheckDisable = 0;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL_ERROR_INVALID_PARAMS;
	} else {
		if (LimitCheckEnable == 0) {
			TempFix1616 = 0;
			LimitCheckEnableInt = 0;
			LimitCheckDisable = 1;

		} else {
			VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				LimitCheckId, TempFix1616);
			LimitCheckDisable = 0;
			/* this to be sure to have either 0 or 1 */
			LimitCheckEnableInt = 1;
		}

		switch (LimitCheckId) {

		case VL_CHECKENABLE_SIGMA_FINAL_RANGE:
			/* internal computation: */
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
				VL_CHECKENABLE_SIGMA_FINAL_RANGE,
				LimitCheckEnableInt);

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:

			Status = VL_WrWord(Dev,
			VL_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
				VL_FIXPOINT1616TOFIXPOINT97(TempFix1616));

			break;

		case VL_CHECKENABLE_SIGNAL_REF_CLIP:

			/* internal computation: */
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
				VL_CHECKENABLE_SIGNAL_REF_CLIP,
				LimitCheckEnableInt);

			break;

		case VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD:

			/* internal computation: */
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				LimitCheckEnableInt);

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_MSRC:

			Temp8 = (uint8_t)(LimitCheckDisable << 1);
			Status = VL_UpdateByte(Dev,
				VL_REG_MSRC_CONFIG_CONTROL,
				0xFE, Temp8);

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE:

			Temp8 = (uint8_t)(LimitCheckDisable << 4);
			Status = VL_UpdateByte(Dev,
				VL_REG_MSRC_CONFIG_CONTROL,
				0xEF, Temp8);

			break;


		default:
			Status = VL_ERROR_INVALID_PARAMS;

		}

	}

	if (Status == VL_ERROR_NONE) {
		if (LimitCheckEnable == 0) {
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
				LimitCheckId, 0);
		} else {
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
				LimitCheckId, 1);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetLimitCheckEnable(struct vl_data *Dev,
	uint16_t LimitCheckId, uint8_t *pLimitCheckEnable)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL_ERROR_INVALID_PARAMS;
		*pLimitCheckEnable = 0;
	} else {
		VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
			LimitCheckId, Temp8);
		*pLimitCheckEnable = Temp8;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetLimitCheckValue(struct vl_data *Dev, uint16_t LimitCheckId,
	unsigned int LimitCheckValue)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksEnable, LimitCheckId,
		Temp8);

	if (Temp8 == 0) { /* disabled write only internal value */
		VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
			LimitCheckId, LimitCheckValue);
	} else {

		switch (LimitCheckId) {

		case VL_CHECKENABLE_SIGMA_FINAL_RANGE:
			/* internal computation: */
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				VL_CHECKENABLE_SIGMA_FINAL_RANGE,
				LimitCheckValue);
			break;

		case VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:

			Status = VL_WrWord(Dev,
			VL_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
				VL_FIXPOINT1616TOFIXPOINT97(
					LimitCheckValue));

			break;

		case VL_CHECKENABLE_SIGNAL_REF_CLIP:

			/* internal computation: */
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				VL_CHECKENABLE_SIGNAL_REF_CLIP,
				LimitCheckValue);

			break;

		case VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD:

			/* internal computation: */
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				LimitCheckValue);

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_MSRC:
		case VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE:

			Status = VL_WrWord(Dev,
				VL_REG_PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT,
				VL_FIXPOINT1616TOFIXPOINT97(
					LimitCheckValue));

			break;

		default:
			Status = VL_ERROR_INVALID_PARAMS;

		}

		if (Status == VL_ERROR_NONE) {
			VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				LimitCheckId, LimitCheckValue);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetLimitCheckValue(struct vl_data *Dev, uint16_t LimitCheckId,
	unsigned int *pLimitCheckValue)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t EnableZeroValue = 0;
	uint16_t Temp16;
	unsigned int TempFix1616;

	LOG_FUNCTION_START("");

	switch (LimitCheckId) {

	case VL_CHECKENABLE_SIGMA_FINAL_RANGE:
		/* internal computation: */
		VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
			VL_CHECKENABLE_SIGMA_FINAL_RANGE, TempFix1616);
		EnableZeroValue = 0;
		break;

	case VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
		Status = VL_RdWord(Dev,
		VL_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
			&Temp16);
		if (Status == VL_ERROR_NONE)
			TempFix1616 = VL_FIXPOINT97TOFIXPOINT1616(Temp16);


		EnableZeroValue = 1;
		break;

	case VL_CHECKENABLE_SIGNAL_REF_CLIP:
		/* internal computation: */
		VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
			VL_CHECKENABLE_SIGNAL_REF_CLIP, TempFix1616);
		EnableZeroValue = 0;
		break;

	case VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD:
		/* internal computation: */
		VL_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
		    VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD, TempFix1616);
		EnableZeroValue = 0;
		break;

	case VL_CHECKENABLE_SIGNAL_RATE_MSRC:
	case VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE:
		Status = VL_RdWord(Dev,
			VL_REG_PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT,
			&Temp16);
		if (Status == VL_ERROR_NONE)
			TempFix1616 = VL_FIXPOINT97TOFIXPOINT1616(Temp16);


		EnableZeroValue = 0;
		break;

	default:
		Status = VL_ERROR_INVALID_PARAMS;

	}

	if (Status == VL_ERROR_NONE) {

		if (EnableZeroValue == 1) {

			if (TempFix1616 == 0) {
				/* disabled: return value from memory */
				VL_GETARRAYPARAMETERFIELD(Dev,
					LimitChecksValue, LimitCheckId,
					TempFix1616);
				*pLimitCheckValue = TempFix1616;
				VL_SETARRAYPARAMETERFIELD(Dev,
					LimitChecksEnable, LimitCheckId, 0);
			} else {
				*pLimitCheckValue = TempFix1616;
				VL_SETARRAYPARAMETERFIELD(Dev,
					LimitChecksValue, LimitCheckId,
					TempFix1616);
				VL_SETARRAYPARAMETERFIELD(Dev,
					LimitChecksEnable, LimitCheckId, 1);
			}
		} else {
			*pLimitCheckValue = TempFix1616;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;

}

int8_t VL_GetLimitCheckCurrent(struct vl_data *Dev,
	uint16_t LimitCheckId, unsigned int *pLimitCheckCurrent)
{
	int8_t Status = VL_ERROR_NONE;
	struct VL_RangingMeasurementData_t LastRangeDataBuffer;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL_ERROR_INVALID_PARAMS;
	} else {
		switch (LimitCheckId) {
		case VL_CHECKENABLE_SIGMA_FINAL_RANGE:
			/* Need to run a ranging to have the latest values */
			*pLimitCheckCurrent = PALDevDataGet(Dev, SigmaEstimate);

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
			/* Need to run a ranging to have the latest values */
			LastRangeDataBuffer = PALDevDataGet(Dev,
				LastRangeMeasure);
			*pLimitCheckCurrent =
				LastRangeDataBuffer.SignalRateRtnMegaCps;

			break;

		case VL_CHECKENABLE_SIGNAL_REF_CLIP:
			/* Need to run a ranging to have the latest values */
			*pLimitCheckCurrent = PALDevDataGet(Dev,
				LastSignalRefMcps);

			break;

		case VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD:
			/* Need to run a ranging to have the latest values */
			LastRangeDataBuffer = PALDevDataGet(Dev,
				LastRangeMeasure);
			*pLimitCheckCurrent =
				LastRangeDataBuffer.SignalRateRtnMegaCps;

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_MSRC:
			/* Need to run a ranging to have the latest values */
			LastRangeDataBuffer = PALDevDataGet(Dev,
				LastRangeMeasure);
			*pLimitCheckCurrent =
				LastRangeDataBuffer.SignalRateRtnMegaCps;

			break;

		case VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE:
			/* Need to run a ranging to have the latest values */
			LastRangeDataBuffer = PALDevDataGet(Dev,
				LastRangeMeasure);
			*pLimitCheckCurrent =
				LastRangeDataBuffer.SignalRateRtnMegaCps;

			break;

		default:
			Status = VL_ERROR_INVALID_PARAMS;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;

}

/*
 * WRAPAROUND Check
 */
int8_t VL_SetWrapAroundCheckEnable(struct vl_data *Dev,
	uint8_t WrapAroundCheckEnable)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte;
	uint8_t WrapAroundCheckEnableInt;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG, &Byte);
	if (WrapAroundCheckEnable == 0) {
		/* Disable wraparound */
		Byte = Byte & 0x7F;
		WrapAroundCheckEnableInt = 0;
	} else {
		/*Enable wraparound */
		Byte = Byte | 0x80;
		WrapAroundCheckEnableInt = 1;
	}

	Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG, Byte);

	if (Status == VL_ERROR_NONE) {
		PALDevDataSet(Dev, SequenceConfig, Byte);
		VL_SETPARAMETERFIELD(Dev, WrapAroundCheckEnable,
			WrapAroundCheckEnableInt);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetWrapAroundCheckEnable(struct vl_data *Dev,
	uint8_t *pWrapAroundCheckEnable)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t data;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG, &data);
	if (Status == VL_ERROR_NONE) {
		PALDevDataSet(Dev, SequenceConfig, data);
		if (data & (0x01 << 7))
			*pWrapAroundCheckEnable = 0x01;
		else
			*pWrapAroundCheckEnable = 0x00;
	}
	if (Status == VL_ERROR_NONE) {
		VL_SETPARAMETERFIELD(Dev, WrapAroundCheckEnable,
			*pWrapAroundCheckEnable);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetDmaxCalParameters(struct vl_data *Dev,
	uint16_t RangeMilliMeter, unsigned int SignalRateRtnMegaCps)
{
	int8_t Status = VL_ERROR_NONE;
	unsigned int SignalRateRtnMegaCpsTemp = 0;

	LOG_FUNCTION_START("");

	/* Check if one of input parameter is zero, in that case the */
	/* value are get from NVM */
	if ((RangeMilliMeter == 0) || (SignalRateRtnMegaCps == 0)) {
		/* NVM parameters */
		/* Run VL_get_info_from_device wit option 4 to get */
		/* signal rate at 400 mm if the value have been already */
		/* get this function will return with no access to device */
		VL_get_info_from_device(Dev, 4);

		SignalRateRtnMegaCpsTemp = VL_GETDEVICESPECIFICPARAMETER(
			Dev, SignalRateMeasFixed400mm);

		PALDevDataSet(Dev, DmaxCalRangeMilliMeter, 400);
		PALDevDataSet(Dev, DmaxCalSignalRateRtnMegaCps,
			SignalRateRtnMegaCpsTemp);
	} else {
		/* User parameters */
		PALDevDataSet(Dev, DmaxCalRangeMilliMeter, RangeMilliMeter);
		PALDevDataSet(Dev, DmaxCalSignalRateRtnMegaCps,
			SignalRateRtnMegaCps);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetDmaxCalParameters(struct vl_data *Dev,
	uint16_t *pRangeMilliMeter, unsigned int *pSignalRateRtnMegaCps)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pRangeMilliMeter = PALDevDataGet(Dev, DmaxCalRangeMilliMeter);
	*pSignalRateRtnMegaCps = PALDevDataGet(Dev,
		DmaxCalSignalRateRtnMegaCps);

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL Parameters Functions */

/* Group PAL Measurement Functions */
int8_t VL_PerformSingleMeasurement(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t DeviceMode;

	LOG_FUNCTION_START("");

	/* Get Current DeviceMode */
	Status = VL_GetDeviceMode(Dev, &DeviceMode);

	/* Start immediately to run a single ranging measurement in case of */
	/* single ranging or single histogram */
	if (Status == VL_ERROR_NONE
		&& DeviceMode == VL_DEVICEMODE_SINGLE_RANGING)
		Status = VL_StartMeasurement(Dev);


	if (Status == VL_ERROR_NONE)
		Status = VL_measurement_poll_for_completion(Dev);


	/* Change PAL State in case of single ranging or single histogram */
	if (Status == VL_ERROR_NONE
		&& DeviceMode == VL_DEVICEMODE_SINGLE_RANGING)
		PALDevDataSet(Dev, PalState, VL_STATE_IDLE);


	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_PerformSingleHistogramMeasurement(struct vl_data *Dev,
	struct VL_HistogramMeasurementData_t *pHistogramMeasurementData)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_PerformRefCalibration(struct vl_data *Dev,
	uint8_t *pVhvSettings, uint8_t *pPhaseCal)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_perform_ref_calibration(Dev, pVhvSettings,
		pPhaseCal, 1);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_PerformXTalkMeasurement(struct vl_data *Dev,
	uint32_t TimeoutMs, unsigned int *pXtalkPerSpad,
	uint8_t *pAmbientTooHigh)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_PerformXTalkCalibration(struct vl_data *Dev,
	unsigned int XTalkCalDistance,
	unsigned int *pXTalkCompensationRateMegaCps)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_perform_xtalk_calibration(Dev, XTalkCalDistance,
		pXTalkCompensationRateMegaCps);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_PerformOffsetCalibration(struct vl_data *Dev,
	unsigned int CalDistanceMilliMeter, int32_t *pOffsetMicroMeter)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_perform_offset_calibration(Dev, CalDistanceMilliMeter,
		pOffsetMicroMeter);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_CheckAndLoadInterruptSettings(struct vl_data *Dev,
	uint8_t StartNotStopFlag)
{
	uint8_t InterruptConfig;
	unsigned int ThresholdLow;
	unsigned int ThresholdHigh;
	int8_t Status = VL_ERROR_NONE;

	InterruptConfig = VL_GETDEVICESPECIFICPARAMETER(Dev,
		Pin0GpioFunctionality);

	if ((InterruptConfig ==
		VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW) ||
		(InterruptConfig ==
		VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH) ||
		(InterruptConfig ==
		VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT)) {

		Status = VL_GetInterruptThresholds(Dev,
			VL_DEVICEMODE_CONTINUOUS_RANGING,
			&ThresholdLow, &ThresholdHigh);

		if (((ThresholdLow > 255*65536) ||
			(ThresholdHigh > 255*65536)) &&
			(Status == VL_ERROR_NONE)) {

			if (StartNotStopFlag != 0) {
				Status = VL_load_tuning_settings(Dev,
					InterruptThresholdSettings);
			} else {
				Status |= VL_WrByte(Dev, 0xFF, 0x04);
				Status |= VL_WrByte(Dev, 0x70, 0x00);
				Status |= VL_WrByte(Dev, 0xFF, 0x00);
				Status |= VL_WrByte(Dev, 0x80, 0x00);
			}

		}


	}

	return Status;

}


int8_t VL_StartMeasurement(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t DeviceMode;
	uint8_t Byte;
	uint8_t StartStopByte = VL_REG_SYSRANGE_MODE_START_STOP;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	/* Get Current DeviceMode */
	VL_GetDeviceMode(Dev, &DeviceMode);

	Status = VL_WrByte(Dev, 0x80, 0x01);
	Status = VL_WrByte(Dev, 0xFF, 0x01);
	Status = VL_WrByte(Dev, 0x00, 0x00);
	Status = VL_WrByte(Dev, 0x91, PALDevDataGet(Dev, StopVariable));
	Status = VL_WrByte(Dev, 0x00, 0x01);
	Status = VL_WrByte(Dev, 0xFF, 0x00);
	Status = VL_WrByte(Dev, 0x80, 0x00);

	switch (DeviceMode) {
	case VL_DEVICEMODE_SINGLE_RANGING:
		Status = VL_WrByte(Dev, VL_REG_SYSRANGE_START, 0x01);

		Byte = StartStopByte;
		if (Status == VL_ERROR_NONE) {
			/* Wait until start bit has been cleared */
			LoopNb = 0;
			do {
				if (LoopNb > 0)
					Status = VL_RdByte(Dev,
					VL_REG_SYSRANGE_START, &Byte);
				LoopNb = LoopNb + 1;
			} while (((Byte & StartStopByte) == StartStopByte)
				&& (Status == VL_ERROR_NONE)
				&& (LoopNb < VL_DEFAULT_MAX_LOOP));

			if (LoopNb >= VL_DEFAULT_MAX_LOOP)
				Status = VL_ERROR_TIME_OUT;

		}

		break;
	case VL_DEVICEMODE_CONTINUOUS_RANGING:
		/* Back-to-back mode */

		/* Check if need to apply interrupt settings */
		if (Status == VL_ERROR_NONE)
			Status = VL_CheckAndLoadInterruptSettings(Dev, 1);

		Status = VL_WrByte(Dev,
		VL_REG_SYSRANGE_START,
		VL_REG_SYSRANGE_MODE_BACKTOBACK);
		if (Status == VL_ERROR_NONE) {
			/* Set PAL State to Running */
			PALDevDataSet(Dev, PalState, VL_STATE_RUNNING);
		}
		break;
	case VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING:
		/* Continuous mode */
		/* Check if need to apply interrupt settings */
		if (Status == VL_ERROR_NONE)
			Status = VL_CheckAndLoadInterruptSettings(Dev, 1);

		Status = VL_WrByte(Dev,
		VL_REG_SYSRANGE_START,
		VL_REG_SYSRANGE_MODE_TIMED);

		if (Status == VL_ERROR_NONE) {
			/* Set PAL State to Running */
			PALDevDataSet(Dev, PalState, VL_STATE_RUNNING);
		}
		break;
	default:
		/* Selected mode not supported */
		Status = VL_ERROR_MODE_NOT_SUPPORTED;
	}


	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_StopMeasurement(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_WrByte(Dev, VL_REG_SYSRANGE_START,
	VL_REG_SYSRANGE_MODE_SINGLESHOT);

	Status = VL_WrByte(Dev, 0xFF, 0x01);
	Status = VL_WrByte(Dev, 0x00, 0x00);
	Status = VL_WrByte(Dev, 0x91, 0x00);
	Status = VL_WrByte(Dev, 0x00, 0x01);
	Status = VL_WrByte(Dev, 0xFF, 0x00);

	if (Status == VL_ERROR_NONE) {
		/* Set PAL State to Idle */
		PALDevDataSet(Dev, PalState, VL_STATE_IDLE);
	}

	/* Check if need to apply interrupt settings */
	if (Status == VL_ERROR_NONE)
		Status = VL_CheckAndLoadInterruptSettings(Dev, 0);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetMeasurementDataReady(struct vl_data *Dev,
	uint8_t *pMeasurementDataReady)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SysRangeStatusRegister;
	uint8_t InterruptConfig;
	uint32_t InterruptMask;

	LOG_FUNCTION_START("");

	InterruptConfig = VL_GETDEVICESPECIFICPARAMETER(Dev,
		Pin0GpioFunctionality);

	if (InterruptConfig ==
		VL_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY) {
		Status = VL_GetInterruptMaskStatus(Dev, &InterruptMask);
		if (InterruptMask ==
		VL_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY)
			*pMeasurementDataReady = 1;
		else
			*pMeasurementDataReady = 0;
	} else {
		Status = VL_RdByte(Dev, VL_REG_RESULT_RANGE_STATUS,
			&SysRangeStatusRegister);
		if (Status == VL_ERROR_NONE) {
			if (SysRangeStatusRegister & 0x01)
				*pMeasurementDataReady = 1;
			else
				*pMeasurementDataReady = 0;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_WaitDeviceReadyForNewMeasurement(struct vl_data *Dev,
	uint32_t MaxLoop)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented for VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}


int8_t VL_GetRangingMeasurementData(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t DeviceRangeStatus;
	uint8_t RangeFractionalEnable;
	uint8_t PalRangeStatus;
	uint8_t XTalkCompensationEnable;
	uint16_t AmbientRate;
	unsigned int SignalRate;
	uint16_t XTalkCompensationRateMegaCps;
	uint16_t EffectiveSpadRtnCount;
	uint16_t tmpuint16;
	uint16_t XtalkRangeMilliMeter;
	uint16_t LinearityCorrectiveGain;
	uint8_t localBuffer[12];
	struct VL_RangingMeasurementData_t LastRangeDataBuffer;

	LOG_FUNCTION_START("");

	/*
	 * use multi read even if some registers are not useful, result will
	 * be more efficient
	 * start reading at 0x14 dec20
	 * end reading at 0x21 dec33 total 14 bytes to read
	 */
	Status = VL_ReadMulti(Dev, 0x14, localBuffer, 12);

	if (Status == VL_ERROR_NONE) {

		pRangingMeasurementData->ZoneId = 0; /* Only one zone */
		pRangingMeasurementData->TimeStamp = 0; /* Not Implemented */

		tmpuint16 = VL_MAKEUINT16(localBuffer[11],
			localBuffer[10]);
		/* cut1.1 if SYSTEM__RANGE_CONFIG if 1 range is 2bits fractional
		 *(format 11.2) else no fractional
		 */

		pRangingMeasurementData->MeasurementTimeUsec = 0;

		SignalRate = VL_FIXPOINT97TOFIXPOINT1616(
			VL_MAKEUINT16(localBuffer[7], localBuffer[6]));
		/* peak_signal_count_rate_rtn_mcps */
		pRangingMeasurementData->SignalRateRtnMegaCps = SignalRate;

		AmbientRate = VL_MAKEUINT16(localBuffer[9],
			localBuffer[8]);
		pRangingMeasurementData->AmbientRateRtnMegaCps =
			VL_FIXPOINT97TOFIXPOINT1616(AmbientRate);

		EffectiveSpadRtnCount = VL_MAKEUINT16(localBuffer[3],
			localBuffer[2]);
		/* EffectiveSpadRtnCount is 8.8 format */
		pRangingMeasurementData->EffectiveSpadRtnCount =
			EffectiveSpadRtnCount;

		DeviceRangeStatus = localBuffer[0];

		/* Get Linearity Corrective Gain */
		LinearityCorrectiveGain = PALDevDataGet(Dev,
			LinearityCorrectiveGain);

		/* Get ranging configuration */
		RangeFractionalEnable = PALDevDataGet(Dev,
			RangeFractionalEnable);

		if (LinearityCorrectiveGain != 1000) {

			tmpuint16 = (uint16_t)((LinearityCorrectiveGain
				* tmpuint16 + 500) / 1000);

			/* Implement Xtalk */
			VL_GETPARAMETERFIELD(Dev,
				XTalkCompensationRateMegaCps,
				XTalkCompensationRateMegaCps);
			VL_GETPARAMETERFIELD(Dev, XTalkCompensationEnable,
				XTalkCompensationEnable);

			if (XTalkCompensationEnable) {

				if ((SignalRate
					- ((XTalkCompensationRateMegaCps
					* EffectiveSpadRtnCount) >> 8))
					<= 0) {
					if (RangeFractionalEnable)
						XtalkRangeMilliMeter = 8888;
					else
						XtalkRangeMilliMeter = 8888
							<< 2;
				} else {
					XtalkRangeMilliMeter =
					(tmpuint16 * SignalRate)
						/ (SignalRate
						- ((XTalkCompensationRateMegaCps
						* EffectiveSpadRtnCount)
						>> 8));
				}

				tmpuint16 = XtalkRangeMilliMeter;
			}

		}

		if (RangeFractionalEnable) {
			pRangingMeasurementData->RangeMilliMeter =
				(uint16_t)((tmpuint16) >> 2);
			pRangingMeasurementData->RangeFractionalPart =
				(uint8_t)((tmpuint16 & 0x03) << 6);
		} else {
			pRangingMeasurementData->RangeMilliMeter = tmpuint16;
			pRangingMeasurementData->RangeFractionalPart = 0;
		}

		/*
		 * For a standard definition of RangeStatus, this should
		 * return 0 in case of good result after a ranging
		 * The range status depends on the device so call a device
		 * specific function to obtain the right Status.
		 */
		Status |= VL_get_pal_range_status(Dev, DeviceRangeStatus,
			SignalRate, EffectiveSpadRtnCount,
			pRangingMeasurementData, &PalRangeStatus);

		if (Status == VL_ERROR_NONE)
			pRangingMeasurementData->RangeStatus = PalRangeStatus;

	}

	if (Status == VL_ERROR_NONE) {
		/* Copy last read data into Dev buffer */
		LastRangeDataBuffer = PALDevDataGet(Dev, LastRangeMeasure);

		LastRangeDataBuffer.RangeMilliMeter =
			pRangingMeasurementData->RangeMilliMeter;
		LastRangeDataBuffer.RangeFractionalPart =
			pRangingMeasurementData->RangeFractionalPart;
		LastRangeDataBuffer.RangeDMaxMilliMeter =
			pRangingMeasurementData->RangeDMaxMilliMeter;
		LastRangeDataBuffer.MeasurementTimeUsec =
			pRangingMeasurementData->MeasurementTimeUsec;
		LastRangeDataBuffer.SignalRateRtnMegaCps =
			pRangingMeasurementData->SignalRateRtnMegaCps;
		LastRangeDataBuffer.AmbientRateRtnMegaCps =
			pRangingMeasurementData->AmbientRateRtnMegaCps;
		LastRangeDataBuffer.EffectiveSpadRtnCount =
			pRangingMeasurementData->EffectiveSpadRtnCount;
		LastRangeDataBuffer.RangeStatus =
			pRangingMeasurementData->RangeStatus;

		PALDevDataSet(Dev, LastRangeMeasure, LastRangeDataBuffer);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetMeasurementRefSignal(struct vl_data *Dev,
	unsigned int *pMeasurementRefSignal)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SignalRefClipLimitCheckEnable = 0;

	LOG_FUNCTION_START("");

	Status = VL_GetLimitCheckEnable(Dev,
			VL_CHECKENABLE_SIGNAL_REF_CLIP,
			&SignalRefClipLimitCheckEnable);
	if (SignalRefClipLimitCheckEnable != 0)
		*pMeasurementRefSignal = PALDevDataGet(Dev, LastSignalRefMcps);
	else
		Status = VL_ERROR_INVALID_COMMAND;
	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_GetHistogramMeasurementData(struct vl_data *Dev,
	struct VL_HistogramMeasurementData_t *pHistogramMeasurementData)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_PerformSingleRangingMeasurement(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	/* This function will do a complete single ranging */
	/* Here we fix the mode! */
	Status = VL_SetDeviceMode(Dev, VL_DEVICEMODE_SINGLE_RANGING);

	if (Status == VL_ERROR_NONE)
		Status = VL_PerformSingleMeasurement(Dev);


	if (Status == VL_ERROR_NONE)
		Status = VL_GetRangingMeasurementData(Dev,
			pRangingMeasurementData);


	if (Status == VL_ERROR_NONE)
		Status = VL_ClearInterruptMask(Dev, 0);


	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetNumberOfROIZones(struct vl_data *Dev,
	uint8_t NumberOfROIZones)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (NumberOfROIZones != 1)
		Status = VL_ERROR_INVALID_PARAMS;


	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetNumberOfROIZones(struct vl_data *Dev,
	uint8_t *pNumberOfROIZones)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pNumberOfROIZones = 1;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetMaxNumberOfROIZones(struct vl_data *Dev,
	uint8_t *pMaxNumberOfROIZones)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pMaxNumberOfROIZones = 1;

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL Measurement Functions */

int8_t VL_SetGpioConfig(struct vl_data *Dev, uint8_t Pin,
	uint8_t DeviceMode, uint8_t Functionality,
	uint8_t Polarity)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t data;

	LOG_FUNCTION_START("");

	if (Pin != 0) {
		Status = VL_ERROR_GPIO_NOT_EXISTING;
	} else if (DeviceMode == VL_DEVICEMODE_GPIO_DRIVE) {
		if (Polarity == VL_INTERRUPTPOLARITY_LOW)
			data = 0x10;
		else
			data = 1;

		Status = VL_WrByte(Dev,
		VL_REG_GPIO_HV_MUX_ACTIVE_HIGH, data);

	} else if (DeviceMode == VL_DEVICEMODE_GPIO_OSC) {

		Status |= VL_WrByte(Dev, 0xff, 0x01);
		Status |= VL_WrByte(Dev, 0x00, 0x00);

		Status |= VL_WrByte(Dev, 0xff, 0x00);
		Status |= VL_WrByte(Dev, 0x80, 0x01);
		Status |= VL_WrByte(Dev, 0x85, 0x02);

		Status |= VL_WrByte(Dev, 0xff, 0x04);
		Status |= VL_WrByte(Dev, 0xcd, 0x00);
		Status |= VL_WrByte(Dev, 0xcc, 0x11);

		Status |= VL_WrByte(Dev, 0xff, 0x07);
		Status |= VL_WrByte(Dev, 0xbe, 0x00);

		Status |= VL_WrByte(Dev, 0xff, 0x06);
		Status |= VL_WrByte(Dev, 0xcc, 0x09);

		Status |= VL_WrByte(Dev, 0xff, 0x00);
		Status |= VL_WrByte(Dev, 0xff, 0x01);
		Status |= VL_WrByte(Dev, 0x00, 0x00);

	} else {

		if (Status == VL_ERROR_NONE) {
			switch (Functionality) {
			case VL_GPIOFUNCTIONALITY_OFF:
				data = 0x00;
				break;
			case VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW:
				data = 0x01;
				break;
			case VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH:
				data = 0x02;
				break;
			case VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT:
				data = 0x03;
				break;
			case VL_GPIOFUNCTIONALITY_NEW_MEASURE_READY:
				data = 0x04;
				break;
			default:
				Status =
				VL_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED;
			}
		}

		if (Status == VL_ERROR_NONE)
			Status = VL_WrByte(Dev,
			VL_REG_SYSTEM_INTERRUPT_CONFIG_GPIO, data);

		if (Status == VL_ERROR_NONE) {
			if (Polarity == VL_INTERRUPTPOLARITY_LOW)
				data = 0;
			else
				data = (uint8_t)(1 << 4);

			Status = VL_UpdateByte(Dev,
			VL_REG_GPIO_HV_MUX_ACTIVE_HIGH, 0xEF, data);
		}

		if (Status == VL_ERROR_NONE)
			VL_SETDEVICESPECIFICPARAMETER(Dev,
				Pin0GpioFunctionality, Functionality);

		if (Status == VL_ERROR_NONE)
			Status = VL_ClearInterruptMask(Dev, 0);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetGpioConfig(struct vl_data *Dev, uint8_t Pin,
	uint8_t *pDeviceMode,
	uint8_t *pFunctionality,
	uint8_t *pPolarity)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t GpioFunctionality;
	uint8_t data;

	LOG_FUNCTION_START("");

	/* pDeviceMode not managed by Ewok it return the current mode */

	Status = VL_GetDeviceMode(Dev, pDeviceMode);

	if (Status == VL_ERROR_NONE) {
		if (Pin != 0) {
			Status = VL_ERROR_GPIO_NOT_EXISTING;
		} else {
			Status = VL_RdByte(Dev,
			VL_REG_SYSTEM_INTERRUPT_CONFIG_GPIO, &data);
		}
	}

	if (Status == VL_ERROR_NONE) {
		switch (data & 0x07) {
		case 0x00:
			GpioFunctionality = VL_GPIOFUNCTIONALITY_OFF;
			break;
		case 0x01:
			GpioFunctionality =
			VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW;
			break;
		case 0x02:
			GpioFunctionality =
			VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH;
			break;
		case 0x03:
			GpioFunctionality =
			VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT;
			break;
		case 0x04:
			GpioFunctionality =
			VL_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
			break;
		default:
			Status = VL_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED;
		}
	}

	if (Status == VL_ERROR_NONE)
		Status = VL_RdByte(Dev,
			VL_REG_GPIO_HV_MUX_ACTIVE_HIGH, &data);

	if (Status == VL_ERROR_NONE) {
		if ((data & (uint8_t)(1 << 4)) == 0)
			*pPolarity = VL_INTERRUPTPOLARITY_LOW;
		else
			*pPolarity = VL_INTERRUPTPOLARITY_HIGH;
	}

	if (Status == VL_ERROR_NONE) {
		*pFunctionality = GpioFunctionality;
		VL_SETDEVICESPECIFICPARAMETER(Dev, Pin0GpioFunctionality,
			GpioFunctionality);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetInterruptThresholds(struct vl_data *Dev,
	uint8_t DeviceMode, unsigned int ThresholdLow,
	unsigned int ThresholdHigh)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t Threshold16;

	LOG_FUNCTION_START("");

	/* no dependency on DeviceMode for Ewok */
	/* Need to divide by 2 because the FW will apply a x2 */
	Threshold16 = (uint16_t)((ThresholdLow >> 17) & 0x00fff);
	Status = VL_WrWord(Dev, VL_REG_SYSTEM_THRESH_LOW,
		Threshold16);

	if (Status == VL_ERROR_NONE) {
		/* Need to divide by 2 because the FW will apply a x2 */
		Threshold16 = (uint16_t)((ThresholdHigh >> 17) & 0x00fff);
		Status = VL_WrWord(Dev, VL_REG_SYSTEM_THRESH_HIGH,
			Threshold16);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetInterruptThresholds(struct vl_data *Dev,
	uint8_t DeviceMode, unsigned int *pThresholdLow,
	unsigned int *pThresholdHigh)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t Threshold16;

	LOG_FUNCTION_START("");

	/* no dependency on DeviceMode for Ewok */

	Status = VL_RdWord(Dev, VL_REG_SYSTEM_THRESH_LOW,
		&Threshold16);
	/* Need to multiply by 2 because the FW will apply a x2 */
	*pThresholdLow = (unsigned int)((0x00fff & Threshold16) << 17);

	if (Status == VL_ERROR_NONE) {
		Status = VL_RdWord(Dev, VL_REG_SYSTEM_THRESH_HIGH,
			&Threshold16);
		/* Need to multiply by 2 because the FW will apply a x2 */
		*pThresholdHigh =
			(unsigned int)((0x00fff & Threshold16) << 17);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetStopCompletedStatus(struct vl_data *Dev,
	uint32_t *pStopStatus)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte = 0;

	LOG_FUNCTION_START("");

	Status = VL_WrByte(Dev, 0xFF, 0x01);

	if (Status == VL_ERROR_NONE)
		Status = VL_RdByte(Dev, 0x04, &Byte);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev, 0xFF, 0x0);

	*pStopStatus = Byte;

	if (Byte == 0) {
		Status = VL_WrByte(Dev, 0x80, 0x01);
		Status = VL_WrByte(Dev, 0xFF, 0x01);
		Status = VL_WrByte(Dev, 0x00, 0x00);
		Status = VL_WrByte(Dev, 0x91,
			PALDevDataGet(Dev, StopVariable));
		Status = VL_WrByte(Dev, 0x00, 0x01);
		Status = VL_WrByte(Dev, 0xFF, 0x00);
		Status = VL_WrByte(Dev, 0x80, 0x00);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/* Group PAL Interrupt Functions */
int8_t VL_ClearInterruptMask(struct vl_data *Dev,
	uint32_t InterruptMask)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t LoopCount;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	/* clear bit 0 range interrupt, bit 1 error interrupt */
	LoopCount = 0;
	do {
		Status = VL_WrByte(Dev,
			VL_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
		Status |= VL_WrByte(Dev,
			VL_REG_SYSTEM_INTERRUPT_CLEAR, 0x00);
		Status |= VL_RdByte(Dev,
			VL_REG_RESULT_INTERRUPT_STATUS, &Byte);
		LoopCount++;
	} while (((Byte & 0x07) != 0x00)
			&& (LoopCount < 3)
			&& (Status == VL_ERROR_NONE));


	if (LoopCount >= 3)
		Status = VL_ERROR_INTERRUPT_NOT_CLEARED;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetInterruptMaskStatus(struct vl_data *Dev,
	uint32_t *pInterruptMaskStatus)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	Status = VL_RdByte(Dev, VL_REG_RESULT_INTERRUPT_STATUS,
		&Byte);
	*pInterruptMaskStatus = Byte & 0x07;

	if (Byte & 0x18)
		Status = VL_ERROR_RANGE_ERROR;

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_EnableInterruptMask(struct vl_data *Dev,
	uint32_t InterruptMask)
{
	int8_t Status = VL_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented for VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL Interrupt Functions */

/* Group SPAD functions */

int8_t VL_SetSpadAmbientDamperThreshold(struct vl_data *Dev,
	uint16_t SpadAmbientDamperThreshold)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_WrWord(Dev, 0x40, SpadAmbientDamperThreshold);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetSpadAmbientDamperThreshold(struct vl_data *Dev,
	uint16_t *pSpadAmbientDamperThreshold)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_RdWord(Dev, 0x40, pSpadAmbientDamperThreshold);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_SetSpadAmbientDamperFactor(struct vl_data *Dev,
	uint16_t SpadAmbientDamperFactor)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	Byte = (uint8_t)(SpadAmbientDamperFactor & 0x00FF);

	Status = VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_WrByte(Dev, 0x42, Byte);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_GetSpadAmbientDamperFactor(struct vl_data *Dev,
	uint16_t *pSpadAmbientDamperFactor)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	Status = VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_RdByte(Dev, 0x42, &Byte);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);
	*pSpadAmbientDamperFactor = (uint16_t)Byte;

	LOG_FUNCTION_END(Status);
	return Status;
}

/* END Group SPAD functions */

/*****************************************************************************
 * Internal functions
 *****************************************************************************/

int8_t VL_SetReferenceSpads(struct vl_data *Dev, uint32_t count,
	uint8_t isApertureSpads)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_set_reference_spads(Dev, count, isApertureSpads);

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_GetReferenceSpads(struct vl_data *Dev, uint32_t *pSpadCount,
	uint8_t *pIsApertureSpads)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_get_reference_spads(Dev, pSpadCount, pIsApertureSpads);

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_PerformRefSpadManagement(struct vl_data *Dev,
	uint32_t *refSpadCount, uint8_t *isApertureSpads)
{
	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL_perform_ref_spad_management(Dev, refSpadCount,
		isApertureSpads);

	LOG_FUNCTION_END(Status);

	return Status;
}
