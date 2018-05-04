/*
 *  vl53l0x_api.h - Linux kernel modules for STM VL53L0 FlightSense TOF
 *						 sensor
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

#ifndef _VL_API_H_
#define _VL_API_H_

#include "vl53l0x_api_strings.h"
#include "vl53l0x_def.h"
#include "vl53l0x_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _MSC_VER
#   ifdef VL_API_EXPORTS
#       define VL_API  __declspec(dllexport)
#   else
#       define VL_API
#   endif
#else
#   define VL_API
#endif

/** @defgroup VL_cut11_group VL53L0X cut1.1 Function Definition
 *  @brief    VL53L0X cut1.1 Function Definition
 *  @{
 */

/** @defgroup VL_general_group VL53L0X General Functions
 *  @brief    General functions and definitions
 *  @{
 */

/**
 * @brief Return the VL53L0X PAL Implementation Version
 *
 * @note This function doesn't access to the device
 *
 * @param   pVersion              Pointer to current PAL Implementation Version
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetVersion(struct VL_Version_t *pVersion);

/**
 * @brief Return the PAL Specification Version used for the current
 * implementation.
 *
 * @note This function doesn't access to the device
 *
 * @param   pPalSpecVersion       Pointer to current PAL Specification Version
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetPalSpecVersion(
	struct VL_Version_t *pPalSpecVersion);

/**
 * @brief Reads the Product Revision for a for given Device
 * This function can be used to distinguish cut1.0 from cut1.1.
 *
 * @note This function Access to the device
 *
 * @param   Dev                 Device Handle
 * @param   pProductRevisionMajor  Pointer to Product Revision Major
 * for a given Device
 * @param   pProductRevisionMinor  Pointer to Product Revision Minor
 * for a given Device
 * @return  VL_ERROR_NONE      Success
 * @return  "Other error code"  See ::int8_t
 */
VL_API int8_t VL_GetProductRevision(struct vl_data *Dev,
	uint8_t *pProductRevisionMajor, uint8_t *pProductRevisionMinor);

/**
 * @brief Reads the Device information for given Device
 *
 * @note This function Access to the device
 *
 * @param   Dev                 Device Handle
 * @param   pVL_DeviceInfo  Pointer to current device info for a given
 *  Device
 * @return  VL_ERROR_NONE   Success
 * @return  "Other error code"  See ::int8_t
 */
VL_API int8_t VL_GetDeviceInfo(struct vl_data *Dev,
	struct VL_DeviceInfo_t *pVL_DeviceInfo);

/**
 * @brief Read current status of the error register for the selected device
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pDeviceErrorStatus    Pointer to current error code of the device
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetDeviceErrorStatus(struct vl_data *Dev,
	uint8_t *pDeviceErrorStatus);

/**
 * @brief Human readable Range Status string for a given RangeStatus
 *
 * @note This function doesn't access to the device
 *
 * @param   RangeStatus         The RangeStatus code as stored on
 * @a struct VL_RangingMeasurementData_t
 * @param   pRangeStatusString  The returned RangeStatus string.
 * @return  VL_ERROR_NONE   Success
 * @return  "Other error code"  See ::int8_t
 */
VL_API int8_t VL_GetRangeStatusString(uint8_t RangeStatus,
	char *pRangeStatusString);

/**
 * @brief Human readable error string for a given Error Code
 *
 * @note This function doesn't access to the device
 *
 * @param   ErrorCode           The error code as stored on ::uint8_t
 * @param   pDeviceErrorString  The error string corresponding to the ErrorCode
 * @return  VL_ERROR_NONE   Success
 * @return  "Other error code"  See ::int8_t
 */
VL_API int8_t VL_GetDeviceErrorString(
	uint8_t ErrorCode, char *pDeviceErrorString);

/**
 * @brief Human readable error string for current PAL error status
 *
 * @note This function doesn't access to the device
 *
 * @param   PalErrorCode       The error code as stored on @a int8_t
 * @param   pPalErrorString    The error string corresponding to the
 * PalErrorCode
 * @return  VL_ERROR_NONE  Success
 * @return  "Other error code" See ::int8_t
 */
VL_API int8_t VL_GetPalErrorString(int8_t PalErrorCode,
	char *pPalErrorString);

/**
 * @brief Human readable PAL State string
 *
 * @note This function doesn't access to the device
 *
 * @param   PalStateCode          The State code as stored on @a uint8_t
 * @param   pPalStateString       The State string corresponding to the
 * PalStateCode
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetPalStateString(uint8_t PalStateCode,
	char *pPalStateString);

/**
 * @brief Reads the internal state of the PAL for a given Device
 *
 * @note This function doesn't access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pPalState             Pointer to current state of the PAL for a
 * given Device
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetPalState(struct vl_data *Dev,
	uint8_t *pPalState);

/**
 * @brief Set the power mode for a given Device
 * The power mode can be Standby or Idle. Different level of both Standby and
 * Idle can exists.
 * This function should not be used when device is in Ranging state.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   PowerMode             The value of the power mode to set.
 * see ::uint8_t
 *                                Valid values are:
 *                                VL_POWERMODE_STANDBY_LEVEL1,
 *                                VL_POWERMODE_IDLE_LEVEL1
 * @return  VL_ERROR_NONE                  Success
 * @return  VL_ERROR_MODE_NOT_SUPPORTED    This error occurs when PowerMode
 * is not in the supported list
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_SetPowerMode(struct vl_data *Dev,
	uint8_t PowerMode);

/**
 * @brief Get the power mode for a given Device
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pPowerMode            Pointer to the current value of the power
 * mode. see ::uint8_t
 *                                Valid values are:
 *                                VL_POWERMODE_STANDBY_LEVEL1,
 *                                VL_POWERMODE_IDLE_LEVEL1
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetPowerMode(struct vl_data *Dev,
	uint8_t *pPowerMode);

/**
 * Set or over-hide part to part calibration offset
 * \sa VL_DataInit()   VL_GetOffsetCalibrationDataMicroMeter()
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   OffsetCalibrationDataMicroMeter    Offset (microns)
 * @return  VL_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::int8_t
 */
VL_API int8_t VL_SetOffsetCalibrationDataMicroMeter(
	struct vl_data *Dev, int32_t OffsetCalibrationDataMicroMeter);

/**
 * @brief Get part to part calibration offset
 *
 * @par Function Description
 * Should only be used after a successful call to @a VL_DataInit to backup
 * device NVM value
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   pOffsetCalibrationDataMicroMeter   Return part to part
 * calibration offset from device (microns)
 * @return  VL_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::int8_t
 */
VL_API int8_t VL_GetOffsetCalibrationDataMicroMeter(
	struct vl_data *Dev, int32_t *pOffsetCalibrationDataMicroMeter);

/**
 * Set the linearity corrective gain
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   LinearityCorrectiveGain            Linearity corrective
 * gain in x1000
 * if value is 1000 then no modification is applied.
 * @return  VL_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::int8_t
 */
VL_API int8_t VL_SetLinearityCorrectiveGain(struct vl_data *Dev,
	int16_t LinearityCorrectiveGain);

/**
 * @brief Get the linearity corrective gain
 *
 * @par Function Description
 * Should only be used after a successful call to @a VL_DataInit to backup
 * device NVM value
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   pLinearityCorrectiveGain           Pointer to the linearity
 * corrective gain in x1000
 * if value is 1000 then no modification is applied.
 * @return  VL_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::int8_t
 */
VL_API int8_t VL_GetLinearityCorrectiveGain(struct vl_data *Dev,
	uint16_t *pLinearityCorrectiveGain);

/**
 * Set Group parameter Hold state
 *
 * @par Function Description
 * Set or remove device internal group parameter hold
 *
 * @note This function is not Implemented
 *
 * @param   Dev      Device Handle
 * @param   GroupParamHold   Group parameter Hold state to be set (on/off)
 * @return  VL_ERROR_NOT_IMPLEMENTED        Not implemented
 */
VL_API int8_t VL_SetGroupParamHold(struct vl_data *Dev,
	uint8_t GroupParamHold);

/**
 * @brief Get the maximal distance for actual setup
 * @par Function Description
 * Device must be initialized through @a VL_SetParameters() prior calling
 * this function.
 *
 * Any range value more than the value returned is to be considered as
 * "no target detected" or
 * "no target in detectable range"\n
 * @warning The maximal distance depends on the setup
 *
 * @note This function is not Implemented
 *
 * @param   Dev      Device Handle
 * @param   pUpperLimitMilliMeter   The maximal range limit for actual setup
 * (in millimeter)
 * @return  VL_ERROR_NOT_IMPLEMENTED        Not implemented
 */
VL_API int8_t VL_GetUpperLimitMilliMeter(struct vl_data *Dev,
	uint16_t *pUpperLimitMilliMeter);


/**
 * @brief Get the Total Signal Rate
 * @par Function Description
 * This function will return the Total Signal Rate after a good ranging is done.
 *
 * @note This function access to Device
 *
 * @param   Dev      Device Handle
 * @param   pTotalSignalRate   Total Signal Rate value in Mega count per second
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_GetTotalSignalRate(struct vl_data *Dev,
	unsigned int *pTotalSignalRate);

/** @} VL_general_group */

/** @defgroup VL_init_group VL53L0X Init Functions
 *  @brief    VL53L0X Init Functions
 *  @{
 */

/**
 * @brief Set new device address
 *
 * After completion the device will answer to the new address programmed.
 * This function should be called when several devices are used in parallel
 * before start programming the sensor.
 * When a single device us used, there is no need to call this function.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   DeviceAddress         The new Device address
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_SetDeviceAddress(struct vl_data *Dev,
	uint8_t DeviceAddress);

/**
 *
 * @brief One time device initialization
 *
 * To be called once and only once after device is brought out of reset
 * (Chip enable) and booted see @a VL_WaitDeviceBooted()
 *
 * @par Function Description
 * When not used after a fresh device "power up" or reset, it may return
 * @a #VL_ERROR_CALIBRATION_WARNING meaning wrong calibration data
 * may have been fetched from device that can result in ranging offset error\n
 * If application cannot execute device reset or need to run VL_DataInit
 * multiple time then it  must ensure proper offset calibration saving and
 * restore on its own by using @a VL_GetOffsetCalibrationData() on first
 * power up and then @a VL_SetOffsetCalibrationData() in all subsequent
 * init.
 * This function will change the uint8_t from VL_STATE_POWERDOWN to
 * VL_STATE_WAIT_STATICINIT.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_DataInit(struct vl_data *Dev);

/**
 * @brief Set the tuning settings pointer
 *
 * This function is used to specify the Tuning settings buffer to be used
 * for a given device. The buffer contains all the necessary data to permit
 * the API to write tuning settings.
 * This function permit to force the usage of either external or internal
 * tuning settings.
 *
 * @note This function Access to the device
 *
 * @param   Dev                             Device Handle
 * @param   pTuningSettingBuffer            Pointer to tuning settings buffer.
 * @param   UseInternalTuningSettings       Use internal tuning settings value.
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_SetTuningSettingBuffer(struct vl_data *Dev,
	uint8_t *pTuningSettingBuffer, uint8_t UseInternalTuningSettings);

/**
 * @brief Get the tuning settings pointer and the internal external switch
 * value.
 *
 * This function is used to get the Tuning settings buffer pointer and the
 * value.
 * of the switch to select either external or internal tuning settings.
 *
 * @note This function Access to the device
 *
 * @param   Dev                        Device Handle
 * @param   ppTuningSettingBuffer      Pointer to tuning settings buffer.
 * @param   pUseInternalTuningSettings Pointer to store Use internal tuning
 *                                     settings value.
 * @return  VL_ERROR_NONE          Success
 * @return  "Other error code"         See ::int8_t
 */
VL_API int8_t VL_GetTuningSettingBuffer(struct vl_data *Dev,
	uint8_t **ppTuningSettingBuffer, uint8_t *pUseInternalTuningSettings);

/**
 * @brief Do basic device init (and eventually patch loading)
 * This function will change the uint8_t from
 * VL_STATE_WAIT_STATICINIT to VL_STATE_IDLE.
 * In this stage all default setting will be applied.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_StaticInit(struct vl_data *Dev);

/**
 * @brief Wait for device booted after chip enable (hardware standby)
 * This function can be run only when uint8_t is VL_STATE_POWERDOWN.
 *
 * @note This function is not Implemented
 *
 * @param   Dev      Device Handle
 * @return  VL_ERROR_NOT_IMPLEMENTED Not implemented
 *
 */
VL_API int8_t VL_WaitDeviceBooted(struct vl_data *Dev);

/**
 * @brief Do an hard reset or soft reset (depending on implementation) of the
 * device \nAfter call of this function, device must be in same state as right
 * after a power-up sequence.This function will change the uint8_t to
 * VL_STATE_POWERDOWN.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_ResetDevice(struct vl_data *Dev);

/** @} VL_init_group */

/** @defgroup VL_parameters_group VL53L0X Parameters Functions
 *  @brief    Functions used to prepare and setup the device
 *  @{
 */

/**
 * @brief  Prepare device for operation
 * @par Function Description
 * Update device with provided parameters
 * @li Then start ranging operation.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pDeviceParameters     Pointer to store current device parameters.
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_SetDeviceParameters(struct vl_data *Dev,
	const struct VL_DeviceParameters_t *pDeviceParameters);

/**
 * @brief  Retrieve current device parameters
 * @par Function Description
 * Get actual parameters of the device
 * @li Then start ranging operation.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pDeviceParameters     Pointer to store current device parameters.
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetDeviceParameters(struct vl_data *Dev,
	struct VL_DeviceParameters_t *pDeviceParameters);

/**
 * @brief  Set a new device mode
 * @par Function Description
 * Set device to a new mode (ranging, histogram ...)
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   DeviceMode            New device mode to apply
 *                                Valid values are:
 *                                VL_DEVICEMODE_SINGLE_RANGING
 *                                VL_DEVICEMODE_CONTINUOUS_RANGING
 *                                VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING
 *                                VL_DEVICEMODE_SINGLE_HISTOGRAM
 *                                VL_HISTOGRAMMODE_REFERENCE_ONLY
 *                                VL_HISTOGRAMMODE_RETURN_ONLY
 *                                VL_HISTOGRAMMODE_BOTH
 *
 *
 * @return  VL_ERROR_NONE               Success
 * @return  VL_ERROR_MODE_NOT_SUPPORTED This error occurs when DeviceMode is
 *                                          not in the supported list
 */
VL_API int8_t VL_SetDeviceMode(struct vl_data *Dev,
	uint8_t DeviceMode);

/**
 * @brief  Get current new device mode
 * @par Function Description
 * Get actual mode of the device(ranging, histogram ...)
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pDeviceMode           Pointer to current apply mode value
 *                                Valid values are:
 *                                VL_DEVICEMODE_SINGLE_RANGING
 *                                VL_DEVICEMODE_CONTINUOUS_RANGING
 *                                VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING
 *                                VL_DEVICEMODE_SINGLE_HISTOGRAM
 *                                VL_HISTOGRAMMODE_REFERENCE_ONLY
 *                                VL_HISTOGRAMMODE_RETURN_ONLY
 *                                VL_HISTOGRAMMODE_BOTH
 *
 * @return  VL_ERROR_NONE                   Success
 * @return  VL_ERROR_MODE_NOT_SUPPORTED     This error occurs when
 * DeviceMode is not in the supported list
 */
VL_API int8_t VL_GetDeviceMode(struct vl_data *Dev,
	uint8_t *pDeviceMode);

/**
 * @brief  Sets the resolution of range measurements.
 * @par Function Description
 * Set resolution of range measurements to either 0.25mm if
 * fraction enabled or 1mm if not enabled.
 *
 * @note This function Accesses the device
 *
 * @param   Dev               Device Handle
 * @param   Enable            Enable high resolution
 *
 * @return  VL_ERROR_NONE               Success
 * @return  "Other error code"              See ::int8_t
 */
VL_API int8_t VL_SetRangeFractionEnable(struct vl_data *Dev,
	uint8_t Enable);

/**
 * @brief  Gets the fraction enable parameter indicating the resolution of
 * range measurements.
 *
 * @par Function Description
 * Gets the fraction enable state, which translates to the resolution of
 * range measurements as follows :Enabled:=0.25mm resolution,
 * Not Enabled:=1mm resolution.
 *
 * @note This function Accesses the device
 *
 * @param   Dev               Device Handle
 * @param   pEnable           Output Parameter reporting the fraction enable state.
 *
 * @return  VL_ERROR_NONE                   Success
 * @return  "Other error code"                  See ::int8_t
 */
VL_API int8_t VL_GetFractionEnable(struct vl_data *Dev,
	uint8_t *pEnable);

/**
 * @brief  Set a new Histogram mode
 * @par Function Description
 * Set device to a new Histogram mode
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   HistogramMode         New device mode to apply
 *                                Valid values are:
 *                                VL_HISTOGRAMMODE_DISABLED
 *                                struct vl_data *ICEMODE_SINGLE_HISTOGRAM
 *                                VL_HISTOGRAMMODE_REFERENCE_ONLY
 *                                VL_HISTOGRAMMODE_RETURN_ONLY
 *                                VL_HISTOGRAMMODE_BOTH
 *
 * @return  VL_ERROR_NONE                   Success
 * @return  VL_ERROR_MODE_NOT_SUPPORTED     This error occurs when
 * HistogramMode is not in the supported list
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_SetHistogramMode(struct vl_data *Dev,
	uint8_t HistogramMode);

/**
 * @brief  Get current new device mode
 * @par Function Description
 * Get current Histogram mode of a Device
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pHistogramMode        Pointer to current Histogram Mode value
 *                                Valid values are:
 *                                VL_HISTOGRAMMODE_DISABLED
 *                                struct vl_data *ICEMODE_SINGLE_HISTOGRAM
 *                                VL_HISTOGRAMMODE_REFERENCE_ONLY
 *                                VL_HISTOGRAMMODE_RETURN_ONLY
 *                                VL_HISTOGRAMMODE_BOTH
 * @return  VL_ERROR_NONE     Success
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetHistogramMode(struct vl_data *Dev,
	uint8_t *pHistogramMode);

/**
 * @brief Set Ranging Timing Budget in microseconds
 *
 * @par Function Description
 * Defines the maximum time allowed by the user to the device to run a
 * full ranging sequence for the current mode (ranging, histogram, ASL ...)
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param MeasurementTimingBudgetMicroSeconds  Max measurement time in
 * microseconds.
 *                                   Valid values are:
 *                                   >= 17000 microsecs when wraparound enabled
 *                                   >= 12000 microsecs when wraparound disabled
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned if
 MeasurementTimingBudgetMicroSeconds out of range
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_SetMeasurementTimingBudgetMicroSeconds(
	struct vl_data *Dev, uint32_t MeasurementTimingBudgetMicroSeconds);

/**
 * @brief Get Ranging Timing Budget in microseconds
 *
 * @par Function Description
 * Returns the programmed the maximum time allowed by the user to the
 * device to run a full ranging sequence for the current mode
 * (ranging, histogram, ASL ...)
 *
 * @note This function Access to the device
 *
 * @param   Dev                                    Device Handle
 * @param   pMeasurementTimingBudgetMicroSeconds   Max measurement time in
 * microseconds.
 *                                   Valid values are:
 *                                   >= 17000 microsecs when wraparound enabled
 *                                   >= 12000 microsecs when wraparound disabled
 * @return  VL_ERROR_NONE                      Success
 * @return  "Other error code"                     See ::int8_t
 */
VL_API int8_t VL_GetMeasurementTimingBudgetMicroSeconds(
	struct vl_data *Dev, uint32_t *pMeasurementTimingBudgetMicroSeconds);

/**
 * @brief Gets the VCSEL pulse period.
 *
 * @par Function Description
 * This function retrieves the VCSEL pulse period for the given period type.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                      Device Handle
 * @param   VcselPeriodType          VCSEL period identifier (pre-range|final).
 * @param   pVCSELPulsePeriod        Pointer to VCSEL period value.
 * @return  VL_ERROR_NONE        Success
 * @return  VL_ERROR_INVALID_PARAMS  Error VcselPeriodType parameter not
 *                                       supported.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetVcselPulsePeriod(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t *pVCSELPulsePeriod);

/**
 * @brief Sets the VCSEL pulse period.
 *
 * @par Function Description
 * This function retrieves the VCSEL pulse period for the given period type.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                       Device Handle
 * @param   VcselPeriodType	      VCSEL period identifier (pre-range|final).
 * @param   VCSELPulsePeriod          VCSEL period value
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_INVALID_PARAMS  Error VcselPeriodType parameter not
 *                                       supported.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_SetVcselPulsePeriod(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t VCSELPulsePeriod);

/**
 * @brief Sets the (on/off) state of a requested sequence step.
 *
 * @par Function Description
 * This function enables/disables a requested sequence step.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   SequenceStepId	         Sequence step identifier.
 * @param   SequenceStepEnabled          Demanded state {0=Off,1=On}
 *                                       is enabled.
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_INVALID_PARAMS  Error SequenceStepId parameter not
 *                                       supported.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_SetSequenceStepEnable(struct vl_data *Dev,
	uint8_t SequenceStepId, uint8_t SequenceStepEnabled);

/**
 * @brief Gets the (on/off) state of a requested sequence step.
 *
 * @par Function Description
 * This function retrieves the state of a requested sequence step, i.e. on/off.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                    Device Handle
 * @param   SequenceStepId         Sequence step identifier.
 * @param   pSequenceStepEnabled   Out parameter reporting if the sequence step
 *                                 is enabled {0=Off,1=On}.
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_INVALID_PARAMS  Error SequenceStepId parameter not
 *                                       supported.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetSequenceStepEnable(struct vl_data *Dev,
	uint8_t SequenceStepId, uint8_t *pSequenceStepEnabled);

/**
 * @brief Gets the (on/off) state of all sequence steps.
 *
 * @par Function Description
 * This function retrieves the state of all sequence step in the scheduler.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   pSchedulerSequenceSteps      Pointer to struct containing result.
 * @return  VL_ERROR_NONE            Success
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetSequenceStepEnables(struct vl_data *Dev,
	struct VL_SchedulerSequenceSteps_t *pSchedulerSequenceSteps);

/**
 * @brief Sets the timeout of a requested sequence step.
 *
 * @par Function Description
 * This function sets the timeout of a requested sequence step.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   SequenceStepId               Sequence step identifier.
 * @param   TimeOutMilliSecs             Demanded timeout
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_INVALID_PARAMS  Error SequenceStepId parameter not
 *                                       supported.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_SetSequenceStepTimeout(struct vl_data *Dev,
	uint8_t SequenceStepId, unsigned int TimeOutMilliSecs);

/**
 * @brief Gets the timeout of a requested sequence step.
 *
 * @par Function Description
 * This function retrieves the timeout of a requested sequence step.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   SequenceStepId               Sequence step identifier.
 * @param   pTimeOutMilliSecs            Timeout value.
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_INVALID_PARAMS  Error SequenceStepId parameter not
 *                                       supported.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetSequenceStepTimeout(struct vl_data *Dev,
	uint8_t SequenceStepId,
	unsigned int *pTimeOutMilliSecs);

/**
 * @brief Gets number of sequence steps managed by the API.
 *
 * @par Function Description
 * This function retrieves the number of sequence steps currently managed
 * by the API
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   pNumberOfSequenceSteps       Out parameter reporting the number of
 *                                       sequence steps.
 * @return  VL_ERROR_NONE            Success
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetNumberOfSequenceSteps(struct vl_data *Dev,
	uint8_t *pNumberOfSequenceSteps);

/**
 * @brief Gets the name of a given sequence step.
 *
 * @par Function Description
 * This function retrieves the name of sequence steps corresponding to
 * SequenceStepId.
 *
 * @note This function doesn't Accesses the device
 *
 * @param   SequenceStepId               Sequence step identifier.
 * @param   pSequenceStepsString         Pointer to Info string
 *
 * @return  VL_ERROR_NONE            Success
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetSequenceStepsInfo(
	uint8_t SequenceStepId, char *pSequenceStepsString);

/**
 * Program continuous mode Inter-Measurement period in milliseconds
 *
 * @par Function Description
 * When trying to set too short time return  INVALID_PARAMS minimal value
 *
 * @note This function Access to the device
 *
 * @param   Dev                                  Device Handle
 * @param   InterMeasurementPeriodMilliSeconds   Inter-Measurement Period in ms.
 * @return  VL_ERROR_NONE                    Success
 * @return  "Other error code"                   See ::int8_t
 */
VL_API int8_t VL_SetInterMeasurementPeriodMilliSeconds(
	struct vl_data *Dev, uint32_t InterMeasurementPeriodMilliSeconds);

/**
 * Get continuous mode Inter-Measurement period in milliseconds
 *
 * @par Function Description
 * When trying to set too short time return  INVALID_PARAMS minimal value
 *
 * @note This function Access to the device
 *
 * @param   Dev                                  Device Handle
 * @param   pInterMeasurementPeriodMilliSeconds  Pointer to programmed
 *  Inter-Measurement Period in milliseconds.
 * @return  VL_ERROR_NONE                    Success
 * @return  "Other error code"                   See ::int8_t
 */
VL_API int8_t VL_GetInterMeasurementPeriodMilliSeconds(
	struct vl_data *Dev, uint32_t *pInterMeasurementPeriodMilliSeconds);

/**
 * @brief Enable/Disable Cross talk compensation feature
 *
 * @note This function is not Implemented.
 * Enable/Disable Cross Talk by set to zero the Cross Talk value
 * by using @a VL_SetXTalkCompensationRateMegaCps().
 *
 * @param   Dev                       Device Handle
 * @param   XTalkCompensationEnable   Cross talk compensation
 *  to be set 0=disabled else = enabled
 * @return  VL_ERROR_NOT_IMPLEMENTED   Not implemented
 */
VL_API int8_t VL_SetXTalkCompensationEnable(struct vl_data *Dev,
	uint8_t XTalkCompensationEnable);

/**
 * @brief Get Cross talk compensation rate
 *
 * @note This function is not Implemented.
 * Enable/Disable Cross Talk by set to zero the Cross Talk value by
 * using @a VL_SetXTalkCompensationRateMegaCps().
 *
 * @param   Dev                        Device Handle
 * @param   pXTalkCompensationEnable   Pointer to the Cross talk compensation
 *  state 0=disabled or 1 = enabled
 * @return  VL_ERROR_NOT_IMPLEMENTED   Not implemented
 */
VL_API int8_t VL_GetXTalkCompensationEnable(struct vl_data *Dev,
	uint8_t *pXTalkCompensationEnable);

/**
 * @brief Set Cross talk compensation rate
 *
 * @par Function Description
 * Set Cross talk compensation rate.
 *
 * @note This function Access to the device
 *
 * @param   Dev                            Device Handle
 * @param   XTalkCompensationRateMegaCps   Compensation rate in
 *  Mega counts per second (16.16 fix point) see datasheet for details
 * @return  VL_ERROR_NONE              Success
 * @return  "Other error code"             See ::int8_t
 */
VL_API int8_t VL_SetXTalkCompensationRateMegaCps(
	struct vl_data *Dev, unsigned int XTalkCompensationRateMegaCps);

/**
 * @brief Get Cross talk compensation rate
 *
 * @par Function Description
 * Get Cross talk compensation rate.
 *
 * @note This function Access to the device
 *
 * @param   Dev                            Device Handle
 * @param   pXTalkCompensationRateMegaCps  Pointer to Compensation rate
 in Mega counts per second (16.16 fix point) see datasheet for details
 * @return  VL_ERROR_NONE              Success
 * @return  "Other error code"             See ::int8_t
 */
VL_API int8_t VL_GetXTalkCompensationRateMegaCps(
	struct vl_data *Dev, unsigned int *pXTalkCompensationRateMegaCps);

/**
 * @brief Set Reference Calibration Parameters
 *
 * @par Function Description
 * Set Reference Calibration Parameters.
 *
 * @note This function Access to the device
 *
 * @param   Dev                            Device Handle
 * @param   VhvSettings                    Parameter for VHV
 * @param   PhaseCal                       Parameter for PhaseCal
 * @return  VL_ERROR_NONE              Success
 * @return  "Other error code"             See ::int8_t
 */
VL_API int8_t VL_SetRefCalibration(struct vl_data *Dev,
	uint8_t VhvSettings, uint8_t PhaseCal);

/**
 * @brief Get Reference Calibration Parameters
 *
 * @par Function Description
 * Get Reference Calibration Parameters.
 *
 * @note This function Access to the device
 *
 * @param   Dev                            Device Handle
 * @param   pVhvSettings                   Pointer to VHV parameter
 * @param   pPhaseCal                      Pointer to PhaseCal Parameter
 * @return  VL_ERROR_NONE              Success
 * @return  "Other error code"             See ::int8_t
 */
VL_API int8_t VL_GetRefCalibration(struct vl_data *Dev,
	uint8_t *pVhvSettings, uint8_t *pPhaseCal);

/**
 * @brief  Get the number of the check limit managed by a given Device
 *
 * @par Function Description
 * This function give the number of the check limit managed by the Device
 *
 * @note This function doesn't Access to the device
 *
 * @param   pNumberOfLimitCheck           Pointer to the number of check limit.
 * @return  VL_ERROR_NONE             Success
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetNumberOfLimitCheck(
	uint16_t *pNumberOfLimitCheck);

/**
 * @brief  Return a description string for a given limit check number
 *
 * @par Function Description
 * This function returns a description string for a given limit check number.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckString             Pointer to the
 description string of the given check limit.
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is
 returned when LimitCheckId value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetLimitCheckInfo(struct vl_data *Dev,
	uint16_t LimitCheckId, char *pLimitCheckString);

/**
 * @brief  Return a the Status of the specified check limit
 *
 * @par Function Description
 * This function returns the Status of the specified check limit.
 * The value indicate if the check is fail or not.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckStatus             Pointer to the
 Limit Check Status of the given check limit.
 * LimitCheckStatus :
 * 0 the check is not fail
 * 1 the check if fail or not enabled
 *
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is
 returned when LimitCheckId value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetLimitCheckStatus(struct vl_data *Dev,
	uint16_t LimitCheckId, uint8_t *pLimitCheckStatus);

/**
 * @brief  Enable/Disable a specific limit check
 *
 * @par Function Description
 * This function Enable/Disable a specific limit check.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   LimitCheckEnable              if 1 the check limit
 *  corresponding to LimitCheckId is Enabled
 *                                        if 0 the check limit
 *  corresponding to LimitCheckId is disabled
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned
 *  when LimitCheckId value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_SetLimitCheckEnable(struct vl_data *Dev,
	uint16_t LimitCheckId, uint8_t LimitCheckEnable);

/**
 * @brief  Get specific limit check enable state
 *
 * @par Function Description
 * This function get the enable state of a specific limit check.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckEnable             Pointer to the check limit enable
 * value.
 *  if 1 the check limit
 *        corresponding to LimitCheckId is Enabled
 *  if 0 the check limit
 *        corresponding to LimitCheckId is disabled
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned
 *  when LimitCheckId value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetLimitCheckEnable(struct vl_data *Dev,
	uint16_t LimitCheckId, uint8_t *pLimitCheckEnable);

/**
 * @brief  Set a specific limit check value
 *
 * @par Function Description
 * This function set a specific limit check value.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   LimitCheckValue               Limit check Value for a given
 * LimitCheckId
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned when either
 *  LimitCheckId or LimitCheckValue value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_SetLimitCheckValue(struct vl_data *Dev,
	uint16_t LimitCheckId, unsigned int LimitCheckValue);

/**
 * @brief  Get a specific limit check value
 *
 * @par Function Description
 * This function get a specific limit check value from device then it updates
 * internal values and check enables.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckValue              Pointer to Limit
 *  check Value for a given LimitCheckId.
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned
 *  when LimitCheckId value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetLimitCheckValue(struct vl_data *Dev,
	uint16_t LimitCheckId, unsigned int *pLimitCheckValue);

/**
 * @brief  Get the current value of the signal used for the limit check
 *
 * @par Function Description
 * This function get a the current value of the signal used for the limit check.
 * To obtain the latest value you should run a ranging before.
 * The value reported is linked to the limit check identified with the
 * LimitCheckId.
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckCurrent            Pointer to current Value for a
 * given LimitCheckId.
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned when
 * LimitCheckId value is out of range.
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetLimitCheckCurrent(struct vl_data *Dev,
	uint16_t LimitCheckId, unsigned int *pLimitCheckCurrent);

/**
 * @brief  Enable (or disable) Wrap around Check
 *
 * @note This function Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   WrapAroundCheckEnable  Wrap around Check to be set
 *                                 0=disabled, other = enabled
 * @return  VL_ERROR_NONE      Success
 * @return  "Other error code"     See ::int8_t
 */
VL_API int8_t VL_SetWrapAroundCheckEnable(struct vl_data *Dev,
		uint8_t WrapAroundCheckEnable);

/**
 * @brief  Get setup of Wrap around Check
 *
 * @par Function Description
 * This function get the wrapAround check enable parameters
 *
 * @note This function Access to the device
 *
 * @param   Dev                     Device Handle
 * @param   pWrapAroundCheckEnable  Pointer to the Wrap around Check state
 *                                  0=disabled or 1 = enabled
 * @return  VL_ERROR_NONE       Success
 * @return  "Other error code"      See ::int8_t
 */
VL_API int8_t VL_GetWrapAroundCheckEnable(struct vl_data *Dev,
		uint8_t *pWrapAroundCheckEnable);

/**
 * @brief   Set Dmax Calibration Parameters for a given device
 * When one of the parameter is zero, this function will get parameter
 * from NVM.
 * @note This function doesn't Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   RangeMilliMeter        Calibration Distance
 * @param   SignalRateRtnMegaCps   Signal rate return read at CalDistance
 * @return  VL_ERROR_NONE      Success
 * @return  "Other error code"     See ::int8_t
 */
VL_API int8_t VL_SetDmaxCalParameters(struct vl_data *Dev,
		uint16_t RangeMilliMeter, unsigned int SignalRateRtnMegaCps);

/**
 * @brief  Get Dmax Calibration Parameters for a given device
 *
 *
 * @note This function Access to the device
 *
 * @param   Dev                     Device Handle
 * @param   pRangeMilliMeter        Pointer to Calibration Distance
 * @param   pSignalRateRtnMegaCps   Pointer to Signal rate return
 * @return  VL_ERROR_NONE       Success
 * @return  "Other error code"      See ::int8_t
 */
VL_API int8_t VL_GetDmaxCalParameters(struct vl_data *Dev,
	uint16_t *pRangeMilliMeter, unsigned int *pSignalRateRtnMegaCps);

/** @} VL_parameters_group */

/** @defgroup VL_measurement_group VL53L0X Measurement Functions
 *  @brief    Functions used for the measurements
 *  @{
 */

/**
 * @brief Single shot measurement.
 *
 * @par Function Description
 * Perform simple measurement sequence (Start measure, Wait measure to end,
 * and returns when measurement is done).
 * Once function returns, user can get valid data by calling
 * VL_GetRangingMeasurement or VL_GetHistogramMeasurement
 * depending on defined measurement mode
 * User should Clear the interrupt in case this are enabled by using the
 * function VL_ClearInterruptMask().
 *
 * @warning This function is a blocking function
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @return  VL_ERROR_NONE    Success
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_PerformSingleMeasurement(struct vl_data *Dev);

/**
 * @brief Perform Reference Calibration
 *
 * @details Perform a reference calibration of the Device.
 * This function should be run from time to time before doing
 * a ranging measurement.
 * This function will launch a special ranging measurement, so
 * if interrupt are enable an interrupt will be done.
 * This function will clear the interrupt generated automatically.
 *
 * @warning This function is a blocking function
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @param   pVhvSettings         Pointer to vhv settings parameter.
 * @param   pPhaseCal            Pointer to PhaseCal parameter.
 * @return  VL_ERROR_NONE    Success
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_PerformRefCalibration(struct vl_data *Dev,
	uint8_t *pVhvSettings, uint8_t *pPhaseCal);

/**
 * @brief Perform XTalk Measurement
 *
 * @details Measures the current cross talk from glass in front
 * of the sensor.
 * This functions performs a histogram measurement and uses the results
 * to measure the crosstalk. For the function to be successful, there
 * must be no target in front of the sensor.
 *
 * @warning This function is a blocking function
 *
 * @warning This function is not supported when the final range
 * vcsel clock period is set below 10 PCLKS.
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @param   TimeoutMs            Histogram measurement duration.
 * @param   pXtalkPerSpad        Output parameter containing the crosstalk
 * measurement result, in MCPS/Spad. Format fixpoint 16:16.
 * @param   pAmbientTooHigh      Output parameter which indicate that
 * pXtalkPerSpad is not good if the Ambient is too high.
 * @return  VL_ERROR_NONE    Success
 * @return  VL_ERROR_INVALID_PARAMS vcsel clock period not supported
 * for this operation. Must not be less than 10PCLKS.
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_PerformXTalkMeasurement(struct vl_data *Dev,
	uint32_t TimeoutMs, unsigned int *pXtalkPerSpad,
	uint8_t *pAmbientTooHigh);

/**
 * @brief Perform XTalk Calibration
 *
 * @details Perform a XTalk calibration of the Device.
 * This function will launch a ranging measurement, if interrupts
 * are enabled an interrupt will be done.
 * This function will clear the interrupt generated automatically.
 * This function will program a new value for the XTalk compensation
 * and it will enable the cross talk before exit.
 * This function will disable the VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD.
 *
 * @warning This function is a blocking function
 *
 * @note This function Access to the device
 *
 * @note This function change the device mode to
 * struct vl_data *ICEMODE_SINGLE_RANGING
 *
 * @param   Dev                  Device Handle
 * @param   XTalkCalDistance     XTalkCalDistance value used for the XTalk
 * computation.
 * @param   pXTalkCompensationRateMegaCps  Pointer to new
 * XTalkCompensation value.
 * @return  VL_ERROR_NONE    Success
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_PerformXTalkCalibration(struct vl_data *Dev,
	unsigned int XTalkCalDistance,
	unsigned int *pXTalkCompensationRateMegaCps);

/**
 * @brief Perform Offset Calibration
 *
 * @details Perform a Offset calibration of the Device.
 * This function will launch a ranging measurement, if interrupts are
 * enabled an interrupt will be done.
 * This function will clear the interrupt generated automatically.
 * This function will program a new value for the Offset calibration value
 * This function will disable the VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD.
 *
 * @warning This function is a blocking function
 *
 * @note This function Access to the device
 *
 * @note This function does not change the device mode.
 *
 * @param   Dev                  Device Handle
 * @param   CalDistanceMilliMeter     Calibration distance value used for the
 * offset compensation.
 * @param   pOffsetMicroMeter  Pointer to new Offset value computed by the
 * function.
 *
 * @return  VL_ERROR_NONE    Success
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_PerformOffsetCalibration(struct vl_data *Dev,
	unsigned int CalDistanceMilliMeter, int32_t *pOffsetMicroMeter);

/**
 * @brief Start device measurement
 *
 * @details Started measurement will depend on device parameters set through
 * @a VL_SetParameters()
 * This is a non-blocking function.
 * This function will change the uint8_t from VL_STATE_IDLE to
 * VL_STATE_RUNNING.
 *
 * @note This function Access to the device
 *

 * @param   Dev                  Device Handle
 * @return  VL_ERROR_NONE                  Success
 * @return  VL_ERROR_MODE_NOT_SUPPORTED    This error occurs when
 * DeviceMode programmed with @a VL_SetDeviceMode is not in the supported
 * list:
 *	Supported mode are:
 *	struct vl_data *ICEMODE_SINGLE_RANGING,
 *	struct vl_data *ICEMODE_CONTINUOUS_RANGING,
 *	struct vl_data *ICEMODE_CONTINUOUS_TIMED_RANGING
 * @return  VL_ERROR_TIME_OUT    Time out on start measurement
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_StartMeasurement(struct vl_data *Dev);

/**
 * @brief Stop device measurement
 *
 * @details Will set the device in standby mode at end of current measurement\n
 *          Not necessary in single mode as device shall return automatically
 *          in standby mode at end of measurement.
 *          This function will change the uint8_t from
 *          VL_STATE_RUNNING to VL_STATE_IDLE.
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @return  VL_ERROR_NONE    Success
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_StopMeasurement(struct vl_data *Dev);

/**
 * @brief Return Measurement Data Ready
 *
 * @par Function Description
 * This function indicate that a measurement data is ready.
 * This function check if interrupt mode is used then check is done accordingly.
 * If perform function clear the interrupt, this function will not work,
 * like in case of @a VL_PerformSingleRangingMeasurement().
 * The previous function is blocking function, VL_GetMeasurementDataReady
 * is used for non-blocking capture.
 *
 * @note This function Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   pMeasurementDataReady  Pointer to Measurement Data Ready.
 *  0=data not ready, 1 = data ready
 * @return  VL_ERROR_NONE      Success
 * @return  "Other error code"     See ::int8_t
 */
VL_API int8_t VL_GetMeasurementDataReady(struct vl_data *Dev,
	uint8_t *pMeasurementDataReady);

/**
 * @brief Wait for device ready for a new measurement command.
 * Blocking function.
 *
 * @note This function is not Implemented
 *
 * @param   Dev      Device Handle
 * @param   MaxLoop    Max Number of polling loop (timeout).
 * @return  VL_ERROR_NOT_IMPLEMENTED   Not implemented
 */
VL_API int8_t VL_WaitDeviceReadyForNewMeasurement(
	struct vl_data *Dev, uint32_t MaxLoop);

/**
 * @brief Retrieve the Reference Signal after a measurements
 *
 * @par Function Description
 * Get Reference Signal from last successful Ranging measurement
 * This function return a valid value after that you call the
 * @a VL_GetRangingMeasurementData().
 *
 * @note This function Access to the device
 *
 * @param   Dev                      Device Handle
 * @param   pMeasurementRefSignal    Pointer to the Ref Signal to fill up.
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"       See ::int8_t
 */
VL_API int8_t VL_GetMeasurementRefSignal(struct vl_data *Dev,
	unsigned int *pMeasurementRefSignal);

/**
 * @brief Retrieve the measurements from device for a given setup
 *
 * @par Function Description
 * Get data from last successful Ranging measurement
 * @warning USER should take care about  @a VL_GetNumberOfROIZones()
 * before get data.
 * PAL will fill a NumberOfROIZones times the corresponding data
 * structure used in the measurement function.
 *
 * @note This function Access to the device
 *
 * @param   Dev                      Device Handle
 * @param   pRangingMeasurementData  Pointer to the data structure to fill up.
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"       See ::int8_t
 */
VL_API int8_t VL_GetRangingMeasurementData(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData);

/**
 * @brief Retrieve the measurements from device for a given setup
 *
 * @par Function Description
 * Get data from last successful Histogram measurement
 * @warning USER should take care about  @a VL_GetNumberOfROIZones()
 * before get data.
 * PAL will fill a NumberOfROIZones times the corresponding data structure
 * used in the measurement function.
 *
 * @note This function is not Implemented
 *
 * @param   Dev                         Device Handle
 * @param   pHistogramMeasurementData   Pointer to the histogram data structure.
 * @return  VL_ERROR_NOT_IMPLEMENTED   Not implemented
 */
VL_API int8_t VL_GetHistogramMeasurementData(struct vl_data *Dev,
	struct VL_HistogramMeasurementData_t *pHistogramMeasurementData);

/**
 * @brief Performs a single ranging measurement and retrieve the ranging
 * measurement data
 *
 * @par Function Description
 * This function will change the device mode to
 * struct vl_data *ICEMODE_SINGLE_RANGING with @a VL_SetDeviceMode(),
 * It performs measurement with @a VL_PerformSingleMeasurement()
 * It get data from last successful Ranging measurement with
 * @a VL_GetRangingMeasurementData.
 * Finally it clear the interrupt with @a VL_ClearInterruptMask().
 *
 * @note This function Access to the device
 *
 * @note This function change the device mode to
 * struct vl_data *ICEMODE_SINGLE_RANGING
 *
 * @param   Dev                       Device Handle
 * @param   pRangingMeasurementData   Pointer to the data structure to fill up.
 * @return  VL_ERROR_NONE         Success
 * @return  "Other error code"        See ::int8_t
 */
VL_API int8_t VL_PerformSingleRangingMeasurement(
	struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData);

/**
 * @brief Performs a single histogram measurement and retrieve the histogram
 * measurement data
 *   Is equivalent to VL_PerformSingleMeasurement +
 *   VL_GetHistogramMeasurementData
 *
 * @par Function Description
 * Get data from last successful Ranging measurement.
 * This function will clear the interrupt in case of these are enabled.
 *
 * @note This function is not Implemented
 *
 * @param   Dev                        Device Handle
 * @param   pHistogramMeasurementData  Pointer to the data structure to fill up.
 * @return  VL_ERROR_NOT_IMPLEMENTED   Not implemented
 */
VL_API int8_t VL_PerformSingleHistogramMeasurement(
	struct vl_data *Dev,
	struct VL_HistogramMeasurementData_t *pHistogramMeasurementData);

/**
 * @brief Set the number of ROI Zones to be used for a specific Device
 *
 * @par Function Description
 * Set the number of ROI Zones to be used for a specific Device.
 * The programmed value should be less than the max number of ROI Zones given
 * with @a VL_GetMaxNumberOfROIZones().
 * This version of API manage only one zone.
 *
 * @param   Dev                           Device Handle
 * @param   NumberOfROIZones              Number of ROI Zones to be used for a
 *  specific Device.
 * @return  VL_ERROR_NONE             Success
 * @return  VL_ERROR_INVALID_PARAMS   This error is returned if
 * NumberOfROIZones != 1
 */
VL_API int8_t VL_SetNumberOfROIZones(struct vl_data *Dev,
	uint8_t NumberOfROIZones);

/**
 * @brief Get the number of ROI Zones managed by the Device
 *
 * @par Function Description
 * Get number of ROI Zones managed by the Device
 * USER should take care about  @a VL_GetNumberOfROIZones()
 * before get data after a perform measurement.
 * PAL will fill a NumberOfROIZones times the corresponding data
 * structure used in the measurement function.
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pNumberOfROIZones     Pointer to the Number of ROI Zones value.
 * @return  VL_ERROR_NONE     Success
 */
VL_API int8_t VL_GetNumberOfROIZones(struct vl_data *Dev,
	uint8_t *pNumberOfROIZones);

/**
 * @brief Get the Maximum number of ROI Zones managed by the Device
 *
 * @par Function Description
 * Get Maximum number of ROI Zones managed by the Device.
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   pMaxNumberOfROIZones   Pointer to the Maximum Number
 *  of ROI Zones value.
 * @return  VL_ERROR_NONE      Success
 */
VL_API int8_t VL_GetMaxNumberOfROIZones(struct vl_data *Dev,
	uint8_t *pMaxNumberOfROIZones);

/** @} VL_measurement_group */

/** @defgroup VL_interrupt_group VL53L0X Interrupt Functions
 *  @brief    Functions used for interrupt managements
 *  @{
 */

/**
 * @brief Set the configuration of GPIO pin for a given device
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   Pin                   ID of the GPIO Pin
 * @param   Functionality         Select Pin functionality.
 *  Refer to ::uint8_t
 * @param   DeviceMode            Device Mode associated to the Gpio.
 * @param   Polarity              Set interrupt polarity. Active high
 *   or active low see ::uint8_t
 * @return  VL_ERROR_NONE                            Success
 * @return  VL_ERROR_GPIO_NOT_EXISTING               Only Pin=0 is accepted.
 * @return  VL_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED    This error occurs
 * when Functionality programmed is not in the supported list:
 *                             Supported value are:
 *                             VL_GPIOFUNCTIONALITY_OFF,
 *                             VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
 *                             VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH,
 VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT,
 *                               VL_GPIOFUNCTIONALITY_NEW_MEASURE_READY
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_SetGpioConfig(struct vl_data *Dev, uint8_t Pin,
	uint8_t DeviceMode, uint8_t Functionality,
	uint8_t Polarity);

/**
 * @brief Get current configuration for GPIO pin for a given device
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   Pin                   ID of the GPIO Pin
 * @param   pDeviceMode           Pointer to Device Mode associated to the Gpio.
 * @param   pFunctionality        Pointer to Pin functionality.
 *  Refer to ::uint8_t
 * @param   pPolarity             Pointer to interrupt polarity.
 *  Active high or active low see ::uint8_t
 * @return  VL_ERROR_NONE                            Success
 * @return  VL_ERROR_GPIO_NOT_EXISTING               Only Pin=0 is accepted.
 * @return  VL_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED   This error occurs
 * when Functionality programmed is not in the supported list:
 *                      Supported value are:
 *                      VL_GPIOFUNCTIONALITY_OFF,
 *                      VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
 *                      VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH,
 *                      VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT,
 *                      VL_GPIOFUNCTIONALITY_NEW_MEASURE_READY
 * @return  "Other error code"    See ::int8_t
 */
VL_API int8_t VL_GetGpioConfig(struct vl_data *Dev, uint8_t Pin,
	uint8_t *pDeviceMode,
	uint8_t *pFunctionality,
	uint8_t *pPolarity);

/**
 * @brief Set low and high Interrupt thresholds for a given mode
 * (ranging, ALS, ...) for a given device
 *
 * @par Function Description
 * Set low and high Interrupt thresholds for a given mode (ranging, ALS, ...)
 * for a given device
 *
 * @note This function Access to the device
 *
 * @note DeviceMode is ignored for the current device
 *
 * @param   Dev              Device Handle
 * @param   DeviceMode       Device Mode for which change thresholds
 * @param   ThresholdLow     Low threshold (mm, lux ..., depending on the mode)
 * @param   ThresholdHigh    High threshold (mm, lux ..., depending on the mode)
 * @return  VL_ERROR_NONE    Success
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_SetInterruptThresholds(struct vl_data *Dev,
	uint8_t DeviceMode, unsigned int ThresholdLow,
	unsigned int ThresholdHigh);

/**
 * @brief  Get high and low Interrupt thresholds for a given mode
 *  (ranging, ALS, ...) for a given device
 *
 * @par Function Description
 * Get high and low Interrupt thresholds for a given mode (ranging, ALS, ...)
 * for a given device
 *
 * @note This function Access to the device
 *
 * @note DeviceMode is ignored for the current device
 *
 * @param   Dev              Device Handle
 * @param   DeviceMode       Device Mode from which read thresholds
 * @param   pThresholdLow    Low threshold (mm, lux ..., depending on the mode)
 * @param   pThresholdHigh   High threshold (mm, lux ..., depending on the mode)
 * @return  VL_ERROR_NONE   Success
 * @return  "Other error code"  See ::int8_t
 */
VL_API int8_t VL_GetInterruptThresholds(struct vl_data *Dev,
	uint8_t DeviceMode, unsigned int *pThresholdLow,
	unsigned int *pThresholdHigh);

/**
 * @brief Return device stop completion status
 *
 * @par Function Description
 * Returns stop completiob status.
 * User shall call this function after a stop command
 *
 * @note This function Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   pStopStatus            Pointer to status variable to update
 * @return  VL_ERROR_NONE      Success
 * @return  "Other error code"     See ::int8_t
 */
VL_API int8_t VL_GetStopCompletedStatus(struct vl_data *Dev,
	uint32_t *pStopStatus);


/**
 * @brief Clear given system interrupt condition
 *
 * @par Function Description
 * Clear given interrupt(s).
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @param   InterruptMask        Mask of interrupts to clear
 * @return  VL_ERROR_NONE    Success
 * @return  VL_ERROR_INTERRUPT_NOT_CLEARED    Cannot clear interrupts
 *
 * @return  "Other error code"   See ::int8_t
 */
VL_API int8_t VL_ClearInterruptMask(struct vl_data *Dev,
	uint32_t InterruptMask);

/**
 * @brief Return device interrupt status
 *
 * @par Function Description
 * Returns currently raised interrupts by the device.
 * User shall be able to activate/deactivate interrupts through
 * @a VL_SetGpioConfig()
 *
 * @note This function Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   pInterruptMaskStatus   Pointer to status variable to update
 * @return  VL_ERROR_NONE      Success
 * @return  "Other error code"     See ::int8_t
 */
VL_API int8_t VL_GetInterruptMaskStatus(struct vl_data *Dev,
	uint32_t *pInterruptMaskStatus);

/**
 * @brief Configure ranging interrupt reported to system
 *
 * @note This function is not Implemented
 *
 * @param   Dev                  Device Handle
 * @param   InterruptMask         Mask of interrupt to Enable/disable
 *  (0:interrupt disabled or 1: interrupt enabled)
 * @return  VL_ERROR_NOT_IMPLEMENTED   Not implemented
 */
VL_API int8_t VL_EnableInterruptMask(struct vl_data *Dev,
	uint32_t InterruptMask);

/** @} VL_interrupt_group */

/** @defgroup VL_SPADfunctions_group VL53L0X SPAD Functions
 *  @brief    Functions used for SPAD managements
 *  @{
 */

/**
 * @brief  Set the SPAD Ambient Damper Threshold value
 *
 * @par Function Description
 * This function set the SPAD Ambient Damper Threshold value
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   SpadAmbientDamperThreshold    SPAD Ambient Damper Threshold value
 * @return  VL_ERROR_NONE             Success
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_SetSpadAmbientDamperThreshold(struct vl_data *Dev,
	uint16_t SpadAmbientDamperThreshold);

/**
 * @brief  Get the current SPAD Ambient Damper Threshold value
 *
 * @par Function Description
 * This function get the SPAD Ambient Damper Threshold value
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   pSpadAmbientDamperThreshold   Pointer to programmed
 *                                        SPAD Ambient Damper Threshold value
 * @return  VL_ERROR_NONE             Success
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetSpadAmbientDamperThreshold(struct vl_data *Dev,
	uint16_t *pSpadAmbientDamperThreshold);

/**
 * @brief  Set the SPAD Ambient Damper Factor value
 *
 * @par Function Description
 * This function set the SPAD Ambient Damper Factor value
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   SpadAmbientDamperFactor       SPAD Ambient Damper Factor value
 * @return  VL_ERROR_NONE             Success
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_SetSpadAmbientDamperFactor(struct vl_data *Dev,
	uint16_t SpadAmbientDamperFactor);

/**
 * @brief  Get the current SPAD Ambient Damper Factor value
 *
 * @par Function Description
 * This function get the SPAD Ambient Damper Factor value
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   pSpadAmbientDamperFactor      Pointer to programmed SPAD Ambient
 * Damper Factor value
 * @return  VL_ERROR_NONE             Success
 * @return  "Other error code"            See ::int8_t
 */
VL_API int8_t VL_GetSpadAmbientDamperFactor(struct vl_data *Dev,
	uint16_t *pSpadAmbientDamperFactor);

/**
 * @brief Performs Reference Spad Management
 *
 * @par Function Description
 * The reference SPAD initialization procedure determines the minimum amount
 * of reference spads to be enables to achieve a target reference signal rate
 * and should be performed once during initialization.
 *
 * @note This function Access to the device
 *
 * @note This function change the device mode to
 * struct vl_data *ICEMODE_SINGLE_RANGING
 *
 * @param   Dev                          Device Handle
 * @param   refSpadCount                 Reports ref Spad Count
 * @param   isApertureSpads              Reports if spads are of type
 *                                       aperture or non-aperture.
 *                                       1:=aperture, 0:=Non-Aperture
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_REF_SPAD_INIT   Error in the Ref Spad procedure.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_PerformRefSpadManagement(struct vl_data *Dev,
	uint32_t *refSpadCount, uint8_t *isApertureSpads);

/**
 * @brief Applies Reference SPAD configuration
 *
 * @par Function Description
 * This function applies a given number of reference spads, identified as
 * either Aperture or Non-Aperture.
 * The requested spad count and type are stored within the device specific
 * parameters data for access by the host.
 *
 * @note This function Access to the device
 *
 * @param   Dev                          Device Handle
 * @param   refSpadCount                 Number of ref spads.
 * @param   isApertureSpads              Defines if spads are of type
 *                                       aperture or non-aperture.
 *                                       1:=aperture, 0:=Non-Aperture
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_REF_SPAD_INIT   Error in the in the reference
 *                                       spad configuration.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_SetReferenceSpads(struct vl_data *Dev,
	uint32_t refSpadCount, uint8_t isApertureSpads);

/**
 * @brief Retrieves SPAD configuration
 *
 * @par Function Description
 * This function retrieves the current number of applied reference spads
 * and also their type : Aperture or Non-Aperture.
 *
 * @note This function Access to the device
 *
 * @param   Dev                          Device Handle
 * @param   refSpadCount                 Number ref Spad Count
 * @param   isApertureSpads              Reports if spads are of type
 *                                       aperture or non-aperture.
 *                                       1:=aperture, 0:=Non-Aperture
 * @return  VL_ERROR_NONE            Success
 * @return  VL_ERROR_REF_SPAD_INIT   Error in the in the reference
 *                                       spad configuration.
 * @return  "Other error code"           See ::int8_t
 */
VL_API int8_t VL_GetReferenceSpads(struct vl_data *Dev,
	uint32_t *refSpadCount, uint8_t *isApertureSpads);

/** @} VL_SPADfunctions_group */

/** @} VL_cut11_group */

#ifdef __cplusplus
}
#endif

#endif /* _VL_API_H_ */
