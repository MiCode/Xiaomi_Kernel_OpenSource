/*
 *  stmvl53l0_module.c - Linux kernel modules for STM VL53L0 FlightSense TOF
 *						 sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
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
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
/*
 * API includes
 */
#include "vl53l0_api.h"
#include "vl53l010_api.h"

#define USE_INT
/* #define DEBUG_TIME_LOG */
#ifdef DEBUG_TIME_LOG
struct timeval start_tv, stop_tv;
#endif

/*
 * Global data
 */

#ifdef CAMERA_CCI
static struct stmvl53l0_module_fn_t stmvl53l0_module_func_tbl = {
	.init = stmvl53l0_init_cci,
	.deinit = stmvl53l0_exit_cci,
	.power_up = stmvl53l0_power_up_cci,
	.power_down = stmvl53l0_power_down_cci,
	.query_power_status = stmvl53l0_cci_power_status,
};
#else
static struct stmvl53l0_module_fn_t stmvl53l0_module_func_tbl = {
	.init = stmvl53l0_init_i2c,
	.deinit = stmvl53l0_exit_i2c,
	.power_up = stmvl53l0_power_up_i2c,
	.power_down = stmvl53l0_power_down_i2c,
	.stmv53l0_cci_power_status = NULL;
};
#endif
struct stmvl53l0_module_fn_t *pmodule_func_tbl;

struct stmvl53l0_api_fn_t {
	int8_t (*GetVersion)(VL53L0_Version_t *pVersion);
	int8_t (*GetPalSpecVersion)(VL53L0_Version_t *pPalSpecVersion);

	int8_t (*GetProductRevision)(VL53L0_DEV Dev,
					uint8_t *pProductRevisionMajor,
					uint8_t *pProductRevisionMinor);
	int8_t (*GetDeviceInfo)(VL53L0_DEV Dev,
				VL53L0_DeviceInfo_t *pVL53L0_DeviceInfo);
	int8_t (*GetDeviceErrorStatus)(VL53L0_DEV Dev,
				VL53L0_DeviceError *pDeviceErrorStatus);
	int8_t (*GetRangeStatusString)(uint8_t RangeStatus,
				char *pRangeStatusString);
	int8_t (*GetDeviceErrorString)(VL53L0_DeviceError ErrorCode,
				char *pDeviceErrorString);
	int8_t (*GetPalErrorString)(VL53L0_Error PalErrorCode,
				char *pPalErrorString);
	int8_t (*GetPalStateString)(VL53L0_State PalStateCode,
				char *pPalStateString);
	int8_t (*GetPalState)(VL53L0_DEV Dev,	VL53L0_State *pPalState);
	int8_t (*SetPowerMode)(VL53L0_DEV Dev,
				VL53L0_PowerModes PowerMode);
	int8_t (*GetPowerMode)(VL53L0_DEV Dev,
				VL53L0_PowerModes *pPowerMode);
	int8_t (*SetOffsetCalibrationDataMicroMeter)(VL53L0_DEV Dev,
				int32_t OffsetCalibrationDataMicroMeter);
	int8_t (*GetOffsetCalibrationDataMicroMeter)(VL53L0_DEV Dev,
				int32_t *pOffsetCalibrationDataMicroMeter);
	int8_t (*SetLinearityCorrectiveGain)(VL53L0_DEV Dev,
				int16_t LinearityCorrectiveGain);
	int8_t (*GetLinearityCorrectiveGain)(VL53L0_DEV Dev,
				uint16_t *pLinearityCorrectiveGain);
	int8_t (*SetGroupParamHold)(VL53L0_DEV Dev,
				uint8_t GroupParamHold);
	int8_t (*GetUpperLimitMilliMeter)(VL53L0_DEV Dev,
				uint16_t *pUpperLimitMilliMeter);
	int8_t (*SetDeviceAddress)(VL53L0_DEV Dev,
				uint8_t DeviceAddress);
	int8_t (*DataInit)(VL53L0_DEV Dev);
	int8_t (*SetTuningSettingBuffer)(VL53L0_DEV Dev,
				uint8_t *pTuningSettingBuffer,
				uint8_t UseInternalTuningSettings);
	int8_t (*GetTuningSettingBuffer)(VL53L0_DEV Dev,
				uint8_t **pTuningSettingBuffer,
				uint8_t *pUseInternalTuningSettings);
	int8_t (*StaticInit)(VL53L0_DEV Dev);
	int8_t (*WaitDeviceBooted)(VL53L0_DEV Dev);
	int8_t (*ResetDevice)(VL53L0_DEV Dev);
	int8_t (*SetDeviceParameters)(VL53L0_DEV Dev,
			const VL53L0_DeviceParameters_t *pDeviceParameters);
	int8_t (*GetDeviceParameters)(VL53L0_DEV Dev,
				VL53L0_DeviceParameters_t *pDeviceParameters);
	int8_t (*SetDeviceMode)(VL53L0_DEV Dev,
				VL53L0_DeviceModes DeviceMode);
	int8_t (*GetDeviceMode)(VL53L0_DEV Dev,
				VL53L0_DeviceModes *pDeviceMode);
	int8_t (*SetHistogramMode)(VL53L0_DEV Dev,
				VL53L0_HistogramModes HistogramMode);
	int8_t (*GetHistogramMode)(VL53L0_DEV Dev,
				VL53L0_HistogramModes *pHistogramMode);
	int8_t (*SetMeasurementTimingBudgetMicroSeconds)(VL53L0_DEV Dev,
				uint32_t  MeasurementTimingBudgetMicroSeconds);
	int8_t (*GetMeasurementTimingBudgetMicroSeconds)(
				VL53L0_DEV Dev,
				uint32_t *pMeasurementTimingBudgetMicroSeconds);
	int8_t (*GetVcselPulsePeriod)(VL53L0_DEV Dev,
				VL53L0_VcselPeriod VcselPeriodType,
				uint8_t	*pVCSELPulsePeriod);
	int8_t (*SetVcselPulsePeriod)(VL53L0_DEV Dev,
				VL53L0_VcselPeriod VcselPeriodType,
				uint8_t VCSELPulsePeriod);
	int8_t (*SetSequenceStepEnable)(VL53L0_DEV Dev,
				VL53L0_SequenceStepId SequenceStepId,
				uint8_t SequenceStepEnabled);
	int8_t (*GetSequenceStepEnable)(VL53L0_DEV Dev,
				VL53L0_SequenceStepId SequenceStepId,
				uint8_t *pSequenceStepEnabled);
	int8_t (*GetSequenceStepEnables)(VL53L0_DEV Dev,
		VL53L0_SchedulerSequenceSteps_t *pSchedulerSequenceSteps);
	int8_t (*SetSequenceStepTimeout)(VL53L0_DEV Dev,
				VL53L0_SequenceStepId SequenceStepId,
				FixPoint1616_t TimeOutMilliSecs);
	int8_t (*GetSequenceStepTimeout)(VL53L0_DEV Dev,
				VL53L0_SequenceStepId SequenceStepId,
				FixPoint1616_t *pTimeOutMilliSecs);
	int8_t (*GetNumberOfSequenceSteps)(VL53L0_DEV Dev,
				uint8_t *pNumberOfSequenceSteps);
	int8_t (*GetSequenceStepsInfo)(
				VL53L0_SequenceStepId SequenceStepId,
				char *pSequenceStepsString);
	int8_t (*SetInterMeasurementPeriodMilliSeconds)(
				VL53L0_DEV Dev,
				uint32_t InterMeasurementPeriodMilliSeconds);
	int8_t (*GetInterMeasurementPeriodMilliSeconds)(
				VL53L0_DEV Dev,
				uint32_t *pInterMeasurementPeriodMilliSeconds);
	int8_t (*SetXTalkCompensationEnable)(VL53L0_DEV Dev,
				uint8_t XTalkCompensationEnable);
	int8_t (*GetXTalkCompensationEnable)(VL53L0_DEV Dev,
				uint8_t *pXTalkCompensationEnable);
	int8_t (*SetXTalkCompensationRateMegaCps)(
				VL53L0_DEV Dev,
				FixPoint1616_t XTalkCompensationRateMegaCps);
	int8_t (*GetXTalkCompensationRateMegaCps)(
				VL53L0_DEV Dev,
				FixPoint1616_t *pXTalkCompensationRateMegaCps);
	int8_t (*GetNumberOfLimitCheck)(
				uint16_t *pNumberOfLimitCheck);
	int8_t (*GetLimitCheckInfo)(VL53L0_DEV Dev,
				uint16_t LimitCheckId, char *pLimitCheckString);
	int8_t (*SetLimitCheckEnable)(VL53L0_DEV Dev,
				uint16_t LimitCheckId,
				uint8_t LimitCheckEnable);
	int8_t (*GetLimitCheckEnable)(VL53L0_DEV Dev,
		uint16_t LimitCheckId, uint8_t *pLimitCheckEnable);
	int8_t (*SetLimitCheckValue)(VL53L0_DEV Dev,
				uint16_t LimitCheckId,
				FixPoint1616_t LimitCheckValue);
	int8_t (*GetLimitCheckValue)(VL53L0_DEV Dev,
				uint16_t LimitCheckId,
				FixPoint1616_t *pLimitCheckValue);
	int8_t (*GetLimitCheckCurrent)(VL53L0_DEV Dev,
		uint16_t LimitCheckId, FixPoint1616_t *pLimitCheckCurrent);
	int8_t (*SetWrapAroundCheckEnable)(VL53L0_DEV Dev,
				uint8_t WrapAroundCheckEnable);
	int8_t (*GetWrapAroundCheckEnable)(VL53L0_DEV Dev,
				uint8_t *pWrapAroundCheckEnable);
	int8_t (*PerformSingleMeasurement)(VL53L0_DEV Dev);
	int8_t (*PerformRefCalibration)(VL53L0_DEV Dev,
				uint8_t *pVhvSettings, uint8_t *pPhaseCal);
	int8_t (*SetRefCalibration)(VL53L0_DEV Dev,
			uint8_t VhvSettings,
			uint8_t PhaseCal);
	int8_t (*GetRefCalibration)(VL53L0_DEV Dev,
			uint8_t *pVhvSettings,
			uint8_t *pPhaseCal);
	int8_t (*PerformXTalkCalibration)(VL53L0_DEV Dev,
				FixPoint1616_t XTalkCalDistance,
				FixPoint1616_t *pXTalkCompensationRateMegaCps);
	int8_t (*PerformOffsetCalibration)(VL53L0_DEV Dev,
				FixPoint1616_t CalDistanceMilliMeter,
				int32_t *pOffsetMicroMeter);
	int8_t (*StartMeasurement)(VL53L0_DEV Dev);
	int8_t (*StopMeasurement)(VL53L0_DEV Dev);
	int8_t (*GetMeasurementDataReady)(VL53L0_DEV Dev,
				uint8_t *pMeasurementDataReady);
	int8_t (*WaitDeviceReadyForNewMeasurement)(VL53L0_DEV Dev,
				uint32_t MaxLoop);
	int8_t (*GetRangingMeasurementData)(VL53L0_DEV Dev,
		VL53L0_RangingMeasurementData_t *pRangingMeasurementData);
	int8_t (*GetHistogramMeasurementData)(VL53L0_DEV Dev,
		VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData);
	int8_t (*PerformSingleRangingMeasurement)(VL53L0_DEV Dev,
		VL53L0_RangingMeasurementData_t *pRangingMeasurementData);
	int8_t (*PerformSingleHistogramMeasurement)(VL53L0_DEV Dev,
		VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData);
	int8_t (*SetNumberOfROIZones)(VL53L0_DEV Dev,
				uint8_t NumberOfROIZones);
	int8_t (*GetNumberOfROIZones)(VL53L0_DEV Dev,
				uint8_t *pNumberOfROIZones);
	int8_t (*GetMaxNumberOfROIZones)(VL53L0_DEV Dev,
				uint8_t *pMaxNumberOfROIZones);
	int8_t (*SetGpioConfig)(VL53L0_DEV Dev,
				uint8_t Pin,
				VL53L0_DeviceModes DeviceMode,
				VL53L0_GpioFunctionality Functionality,
				VL53L0_InterruptPolarity Polarity);
	int8_t (*GetGpioConfig)(VL53L0_DEV Dev,
				uint8_t Pin,
				VL53L0_DeviceModes *pDeviceMode,
				VL53L0_GpioFunctionality *pFunctionality,
				VL53L0_InterruptPolarity *pPolarity);
	int8_t (*SetInterruptThresholds)(VL53L0_DEV Dev,
				VL53L0_DeviceModes DeviceMode,
				FixPoint1616_t ThresholdLow,
				FixPoint1616_t ThresholdHigh);
	int8_t (*GetInterruptThresholds)(VL53L0_DEV Dev,
				VL53L0_DeviceModes DeviceMode,
				FixPoint1616_t *pThresholdLow,
				FixPoint1616_t *pThresholdHigh);
	int8_t (*ClearInterruptMask)(VL53L0_DEV Dev,
				uint32_t InterruptMask);
	int8_t (*GetInterruptMaskStatus)(VL53L0_DEV Dev,
				uint32_t *pInterruptMaskStatus);
	int8_t (*EnableInterruptMask)(VL53L0_DEV Dev, uint32_t InterruptMask);
	int8_t (*SetSpadAmbientDamperThreshold)(VL53L0_DEV Dev,
				uint16_t SpadAmbientDamperThreshold);
	int8_t (*GetSpadAmbientDamperThreshold)(VL53L0_DEV Dev,
				uint16_t *pSpadAmbientDamperThreshold);
	int8_t (*SetSpadAmbientDamperFactor)(VL53L0_DEV Dev,
				uint16_t SpadAmbientDamperFactor);
	int8_t (*GetSpadAmbientDamperFactor)(VL53L0_DEV Dev,
				uint16_t *pSpadAmbientDamperFactor);
	int8_t (*PerformRefSpadManagement)(VL53L0_DEV Dev,
		uint32_t *refSpadCount, uint8_t *isApertureSpads);
	int8_t (*SetReferenceSpads)(VL53L0_DEV Dev,
			 uint32_t count, uint8_t isApertureSpads);
	int8_t (*GetReferenceSpads)(VL53L0_DEV Dev,
			uint32_t *pSpadCount, uint8_t *pIsApertureSpads);
};

static struct stmvl53l0_api_fn_t stmvl53l0_api_func_tbl = {
	.GetVersion = VL53L0_GetVersion,
	.GetPalSpecVersion = VL53L0_GetPalSpecVersion,
	.GetProductRevision = VL53L0_GetProductRevision,
	.GetDeviceInfo = VL53L0_GetDeviceInfo,
	.GetDeviceErrorStatus = VL53L0_GetDeviceErrorStatus,
	.GetRangeStatusString = VL53L0_GetRangeStatusString,
	.GetDeviceErrorString = VL53L0_GetDeviceErrorString,
	.GetPalErrorString = VL53L0_GetPalErrorString,
	.GetPalState = VL53L0_GetPalState,
	.SetPowerMode = VL53L0_SetPowerMode,
	.GetPowerMode = VL53L0_GetPowerMode,
	.SetOffsetCalibrationDataMicroMeter =
		VL53L0_SetOffsetCalibrationDataMicroMeter,
	.SetLinearityCorrectiveGain =
		VL53L0_SetLinearityCorrectiveGain,
	.GetLinearityCorrectiveGain =
		VL53L0_GetLinearityCorrectiveGain,
	.GetOffsetCalibrationDataMicroMeter =
		VL53L0_GetOffsetCalibrationDataMicroMeter,
	.SetGroupParamHold = VL53L0_SetGroupParamHold,
	.GetUpperLimitMilliMeter = VL53L0_GetUpperLimitMilliMeter,
	.SetDeviceAddress = VL53L0_SetDeviceAddress,
	.DataInit = VL53L0_DataInit,
	.SetTuningSettingBuffer = VL53L0_SetTuningSettingBuffer,
	.GetTuningSettingBuffer = VL53L0_GetTuningSettingBuffer,
	.StaticInit = VL53L0_StaticInit,
	.WaitDeviceBooted = VL53L0_WaitDeviceBooted,
	.ResetDevice = VL53L0_ResetDevice,
	.SetDeviceParameters = VL53L0_SetDeviceParameters,
	.SetDeviceMode = VL53L0_SetDeviceMode,
	.GetDeviceMode = VL53L0_GetDeviceMode,
	.SetHistogramMode = VL53L0_SetHistogramMode,
	.GetHistogramMode = VL53L0_GetHistogramMode,
	.SetMeasurementTimingBudgetMicroSeconds =
		VL53L0_SetMeasurementTimingBudgetMicroSeconds,
	.GetMeasurementTimingBudgetMicroSeconds =
		VL53L0_GetMeasurementTimingBudgetMicroSeconds,
	.GetVcselPulsePeriod = VL53L0_GetVcselPulsePeriod,
	.SetVcselPulsePeriod = VL53L0_SetVcselPulsePeriod,
	.SetSequenceStepEnable = VL53L0_SetSequenceStepEnable,
	.GetSequenceStepEnable = VL53L0_GetSequenceStepEnable,
	.GetSequenceStepEnables = VL53L0_GetSequenceStepEnables,
	.SetSequenceStepTimeout = VL53L0_SetSequenceStepTimeout,
	.GetSequenceStepTimeout = VL53L0_GetSequenceStepTimeout,
	.GetNumberOfSequenceSteps = VL53L0_GetNumberOfSequenceSteps,
	.GetSequenceStepsInfo = VL53L0_GetSequenceStepsInfo,
	.SetInterMeasurementPeriodMilliSeconds =
		VL53L0_SetInterMeasurementPeriodMilliSeconds,
	.GetInterMeasurementPeriodMilliSeconds =
		VL53L0_GetInterMeasurementPeriodMilliSeconds,
	.SetXTalkCompensationEnable = VL53L0_SetXTalkCompensationEnable,
	.GetXTalkCompensationEnable = VL53L0_GetXTalkCompensationEnable,
	.SetXTalkCompensationRateMegaCps =
		VL53L0_SetXTalkCompensationRateMegaCps,
	.GetXTalkCompensationRateMegaCps =
		VL53L0_GetXTalkCompensationRateMegaCps,
	.GetNumberOfLimitCheck = VL53L0_GetNumberOfLimitCheck,
	.GetLimitCheckInfo = VL53L0_GetLimitCheckInfo,
	.SetLimitCheckEnable = VL53L0_SetLimitCheckEnable,
	.GetLimitCheckEnable = VL53L0_GetLimitCheckEnable,
	.SetLimitCheckValue = VL53L0_SetLimitCheckValue,
	.GetLimitCheckValue = VL53L0_GetLimitCheckValue,
	.GetLimitCheckCurrent = VL53L0_GetLimitCheckCurrent,
	.SetWrapAroundCheckEnable = VL53L0_SetWrapAroundCheckEnable,
	.GetWrapAroundCheckEnable = VL53L0_GetWrapAroundCheckEnable,
	.PerformSingleMeasurement = VL53L0_PerformSingleMeasurement,
	.PerformRefCalibration = VL53L0_PerformRefCalibration,
	.SetRefCalibration = VL53L0_SetRefCalibration,
	.GetRefCalibration = VL53L0_GetRefCalibration,
	.PerformXTalkCalibration = VL53L0_PerformXTalkCalibration,
	.PerformOffsetCalibration = VL53L0_PerformOffsetCalibration,
	.StartMeasurement = VL53L0_StartMeasurement,
	.StopMeasurement = VL53L0_StopMeasurement,
	.GetMeasurementDataReady = VL53L0_GetMeasurementDataReady,
	.WaitDeviceReadyForNewMeasurement =
		VL53L0_WaitDeviceReadyForNewMeasurement,
	.GetRangingMeasurementData = VL53L0_GetRangingMeasurementData,
	.GetHistogramMeasurementData = VL53L0_GetHistogramMeasurementData,
	.PerformSingleRangingMeasurement =
		VL53L0_PerformSingleRangingMeasurement,
	.PerformSingleHistogramMeasurement =
		VL53L0_PerformSingleHistogramMeasurement,
	.SetNumberOfROIZones = VL53L0_SetNumberOfROIZones,
	.GetNumberOfROIZones = VL53L0_GetNumberOfROIZones,
	.GetMaxNumberOfROIZones = VL53L0_GetMaxNumberOfROIZones,
	.SetGpioConfig = VL53L0_SetGpioConfig,
	.GetGpioConfig = VL53L0_GetGpioConfig,
	.SetInterruptThresholds = VL53L0_SetInterruptThresholds,
	.GetInterruptThresholds = VL53L0_GetInterruptThresholds,
	.ClearInterruptMask = VL53L0_ClearInterruptMask,
	.GetInterruptMaskStatus = VL53L0_GetInterruptMaskStatus,
	.EnableInterruptMask = VL53L0_EnableInterruptMask,
	.SetSpadAmbientDamperThreshold = VL53L0_SetSpadAmbientDamperThreshold,
	.GetSpadAmbientDamperThreshold = VL53L0_GetSpadAmbientDamperThreshold,
	.SetSpadAmbientDamperFactor = VL53L0_SetSpadAmbientDamperFactor,
	.GetSpadAmbientDamperFactor = VL53L0_GetSpadAmbientDamperFactor,
	.PerformRefSpadManagement = VL53L0_PerformRefSpadManagement,
	.SetReferenceSpads = VL53L0_SetReferenceSpads,
	.GetReferenceSpads = VL53L0_GetReferenceSpads,

};
struct stmvl53l0_api_fn_t *papi_func_tbl;

/*
 * IOCTL definitions
 */
#define VL53L0_IOCTL_INIT			_IO('p', 0x01)
#define VL53L0_IOCTL_XTALKCALB		_IOW('p', 0x02, unsigned int)
#define VL53L0_IOCTL_OFFCALB		_IOW('p', 0x03, unsigned int)
#define VL53L0_IOCTL_STOP			_IO('p', 0x05)
#define VL53L0_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL53L0_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL53L0_IOCTL_ACTIVATE_USE_CASE	_IOW('p', 0x08, uint8_t)
#define VL53L0_IOCTL_ACTIVATE_CUSTOM_USE_CASE	\
			_IOW('p', 0x09, struct stmvl53l0_custom_use_case)

#define VL53L0_IOCTL_GETDATAS \
			_IOR('p', 0x0b, VL53L0_RangingMeasurementData_t)
#define VL53L0_IOCTL_REGISTER \
			_IOWR('p', 0x0c, struct stmvl53l0_register)
#define VL53L0_IOCTL_PARAMETER \
			_IOWR('p', 0x0d, struct stmvl53l0_parameter)


/* Mask fields to indicate Offset and Xtalk Comp
 * values have been set by application
 */
#define SET_OFFSET_CALIB_DATA_MICROMETER_MASK 0x1
#define SET_XTALK_COMP_RATE_MCPS_MASK         0x2

/* Macros used across different functions */
#define USE_CASE_LONG_DISTANCE	1
#define USE_CASE_HIGH_ACCURACY	2
#define USE_CASE_HIGH_SPEED		3
#define USE_CASE_CUSTOM			4

#define LONG_DISTANCE_TIMING_BUDGET			26000
#define LONG_DISTANCE_SIGNAL_RATE_LIMIT		(65536 / 10) /* 0.1  */
#define LONG_DISTANCE_SIGMA_LIMIT			(60*65536)
#define LONG_DISTANCE_PRE_RANGE_PULSE_PERIOD	18
#define LONG_DISTANCE_FINAL_RANGE_PULSE_PERIOD	14



#define HIGH_ACCURACY_TIMING_BUDGET				200000
#define HIGH_ACCURACY_SIGNAL_RATE_LIMIT	 (25 * 65536 / 100) /*0.25*/
#define HIGH_ACCURACY_SIGMA_LIMIT				(18*65536)
#define HIGH_ACCURACY_PRE_RANGE_PULSE_PERIOD	14
#define HIGH_ACCURACY_FINAL_RANGE_PULSE_PERIOD	10



#define HIGH_SPEED_TIMING_BUDGET				20000
#define HIGH_SPEED_SIGNAL_RATE_LIMIT	(25 * 65536 / 100) /* 0.25 */
#define HIGH_SPEED_SIGMA_LIMIT					(32*65536)
#define HIGH_SPEED_PRE_RANGE_PULSE_PERIOD		14
#define HIGH_SPEED_FINAL_RANGE_PULSE_PERIOD		10





static long stmvl53l0_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);
/*static int stmvl53l0_flush(struct file *file, fl_owner_t id);*/
static int stmvl53l0_open(struct inode *inode, struct file *file);
static int stmvl53l0_init_client(struct stmvl53l0_data *data);
static int stmvl53l0_start(struct stmvl53l0_data *data, uint8_t scaling,
			init_mode_e mode);
static int stmvl53l0_stop(struct stmvl53l0_data *data);
static int stmvl53l0_config_use_case(struct stmvl53l0_data *data);

#ifdef DEBUG_TIME_LOG
static void stmvl53l0_DebugTimeGet(struct timeval *ptv)
{
	do_gettimeofday(ptv);
}

static void stmvl53l0_DebugTimeDuration(struct timeval *pstart_tv,
			struct timeval *pstop_tv)
{
	long total_sec, total_msec;

	total_sec = pstop_tv->tv_sec - pstart_tv->tv_sec;
	total_msec = (pstop_tv->tv_usec - pstart_tv->tv_usec)/1000;
	total_msec += total_sec * 1000;
	pr_err("elapsedTime:%ld\n", total_msec);
}
#endif

static void stmvl53l0_setupAPIFunctions(struct stmvl53l0_data *data)
{
	uint8_t revision = 0;
	VL53L0_DEV vl53l0_dev = data;

	/* Read Revision ID */
	VL53L0_RdByte(vl53l0_dev,
				VL53L0_REG_IDENTIFICATION_REVISION_ID,
				&revision);
	vl53l0_errmsg("read REVISION_ID: 0x%x\n", revision);
	revision = (revision & 0xF0) >> 4;
	if (revision == 1) {
		/*cut 1.1*/
		vl53l0_errmsg("to setup API cut 1.1\n");
		papi_func_tbl->GetVersion = VL53L0_GetVersion;
		papi_func_tbl->GetPalSpecVersion = VL53L0_GetPalSpecVersion;
		papi_func_tbl->GetProductRevision = VL53L0_GetProductRevision;
		papi_func_tbl->GetDeviceInfo = VL53L0_GetDeviceInfo;
		papi_func_tbl->GetDeviceErrorStatus =
			 VL53L0_GetDeviceErrorStatus;
		papi_func_tbl->GetRangeStatusString =
			 VL53L0_GetRangeStatusString;
		papi_func_tbl->GetDeviceErrorString =
			 VL53L0_GetDeviceErrorString;
		papi_func_tbl->GetPalErrorString =
			 VL53L0_GetPalErrorString;
		papi_func_tbl->GetPalState =
			 VL53L0_GetPalState;
		papi_func_tbl->SetPowerMode =
			 VL53L0_SetPowerMode;
		papi_func_tbl->GetPowerMode =
			 VL53L0_GetPowerMode;
		papi_func_tbl->SetOffsetCalibrationDataMicroMeter =
				VL53L0_SetOffsetCalibrationDataMicroMeter;
		papi_func_tbl->GetOffsetCalibrationDataMicroMeter =
				VL53L0_GetOffsetCalibrationDataMicroMeter;
		papi_func_tbl->SetLinearityCorrectiveGain =
VL53L0_SetLinearityCorrectiveGain;
		papi_func_tbl->GetLinearityCorrectiveGain =
VL53L0_GetLinearityCorrectiveGain;
		papi_func_tbl->SetGroupParamHold = VL53L0_SetGroupParamHold;
		papi_func_tbl->GetUpperLimitMilliMeter =
			 VL53L0_GetUpperLimitMilliMeter;
		papi_func_tbl->SetDeviceAddress =
			 VL53L0_SetDeviceAddress;
		papi_func_tbl->DataInit =
			 VL53L0_DataInit;
		papi_func_tbl->SetTuningSettingBuffer =
			 VL53L0_SetTuningSettingBuffer;
		papi_func_tbl->GetTuningSettingBuffer =
			 VL53L0_GetTuningSettingBuffer;
		papi_func_tbl->StaticInit =
			 VL53L0_StaticInit;
		papi_func_tbl->WaitDeviceBooted =
			 VL53L0_WaitDeviceBooted;
		papi_func_tbl->ResetDevice =
			 VL53L0_ResetDevice;
		papi_func_tbl->SetDeviceParameters =
			 VL53L0_SetDeviceParameters;
		papi_func_tbl->SetDeviceMode = VL53L0_SetDeviceMode;
		papi_func_tbl->GetDeviceMode = VL53L0_GetDeviceMode;
		papi_func_tbl->SetHistogramMode = VL53L0_SetHistogramMode;
		papi_func_tbl->GetHistogramMode = VL53L0_GetHistogramMode;
		papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds =
		 VL53L0_SetMeasurementTimingBudgetMicroSeconds;
		papi_func_tbl->GetMeasurementTimingBudgetMicroSeconds =
			 VL53L0_GetMeasurementTimingBudgetMicroSeconds;
		papi_func_tbl->GetVcselPulsePeriod = VL53L0_GetVcselPulsePeriod;
		papi_func_tbl->SetVcselPulsePeriod = VL53L0_SetVcselPulsePeriod;
		papi_func_tbl->SetSequenceStepEnable =
				 VL53L0_SetSequenceStepEnable;
		papi_func_tbl->GetSequenceStepEnable =
				 VL53L0_GetSequenceStepEnable;
		papi_func_tbl->GetSequenceStepEnables =
				 VL53L0_GetSequenceStepEnables;
		papi_func_tbl->SetSequenceStepTimeout =
				 VL53L0_SetSequenceStepTimeout;
		papi_func_tbl->GetSequenceStepTimeout =
				 VL53L0_GetSequenceStepTimeout;
		papi_func_tbl->GetNumberOfSequenceSteps =
				 VL53L0_GetNumberOfSequenceSteps;
		papi_func_tbl->GetSequenceStepsInfo =
				 VL53L0_GetSequenceStepsInfo;
		papi_func_tbl->SetInterMeasurementPeriodMilliSeconds =
				 VL53L0_SetInterMeasurementPeriodMilliSeconds;
		papi_func_tbl->GetInterMeasurementPeriodMilliSeconds =
				VL53L0_GetInterMeasurementPeriodMilliSeconds;
		papi_func_tbl->SetXTalkCompensationEnable =
				 VL53L0_SetXTalkCompensationEnable;
		papi_func_tbl->GetXTalkCompensationEnable =
				 VL53L0_GetXTalkCompensationEnable;
		papi_func_tbl->SetXTalkCompensationRateMegaCps =
				 VL53L0_SetXTalkCompensationRateMegaCps;
		papi_func_tbl->GetXTalkCompensationRateMegaCps =
				 VL53L0_GetXTalkCompensationRateMegaCps;
		papi_func_tbl->GetNumberOfLimitCheck =
				 VL53L0_GetNumberOfLimitCheck;
		papi_func_tbl->GetLimitCheckInfo =
				 VL53L0_GetLimitCheckInfo;
		papi_func_tbl->SetLimitCheckEnable =
				 VL53L0_SetLimitCheckEnable;
		papi_func_tbl->GetLimitCheckEnable =
				 VL53L0_GetLimitCheckEnable;
		papi_func_tbl->SetLimitCheckValue =
				 VL53L0_SetLimitCheckValue;
		papi_func_tbl->GetLimitCheckValue =
				 VL53L0_GetLimitCheckValue;
		papi_func_tbl->GetLimitCheckCurrent =
				 VL53L0_GetLimitCheckCurrent;
		papi_func_tbl->SetWrapAroundCheckEnable =
				 VL53L0_SetWrapAroundCheckEnable;
		papi_func_tbl->GetWrapAroundCheckEnable =
				 VL53L0_GetWrapAroundCheckEnable;
		papi_func_tbl->PerformSingleMeasurement =
				 VL53L0_PerformSingleMeasurement;
		papi_func_tbl->PerformRefCalibration =
				 VL53L0_PerformRefCalibration;
		papi_func_tbl->SetRefCalibration =
				 VL53L0_SetRefCalibration;
		papi_func_tbl->GetRefCalibration =
					 VL53L0_GetRefCalibration;
		papi_func_tbl->PerformXTalkCalibration =
				 VL53L0_PerformXTalkCalibration;
		papi_func_tbl->PerformOffsetCalibration =
				 VL53L0_PerformOffsetCalibration;
		papi_func_tbl->StartMeasurement =
				 VL53L0_StartMeasurement;
		papi_func_tbl->StopMeasurement =
				 VL53L0_StopMeasurement;
		papi_func_tbl->GetMeasurementDataReady =
				 VL53L0_GetMeasurementDataReady;
		papi_func_tbl->WaitDeviceReadyForNewMeasurement =
			VL53L0_WaitDeviceReadyForNewMeasurement;
		papi_func_tbl->GetRangingMeasurementData =
			VL53L0_GetRangingMeasurementData;
		papi_func_tbl->GetHistogramMeasurementData =
			VL53L0_GetHistogramMeasurementData;
		papi_func_tbl->PerformSingleRangingMeasurement =
			VL53L0_PerformSingleRangingMeasurement;
		papi_func_tbl->PerformSingleHistogramMeasurement =
			VL53L0_PerformSingleHistogramMeasurement;
		papi_func_tbl->SetNumberOfROIZones =
			 VL53L0_SetNumberOfROIZones;
		papi_func_tbl->GetNumberOfROIZones =
			VL53L0_GetNumberOfROIZones;
		papi_func_tbl->GetMaxNumberOfROIZones =
			 VL53L0_GetMaxNumberOfROIZones;
		papi_func_tbl->SetGpioConfig =
			 VL53L0_SetGpioConfig;
		papi_func_tbl->GetGpioConfig =
			 VL53L0_GetGpioConfig;
		papi_func_tbl->SetInterruptThresholds =
			 VL53L0_SetInterruptThresholds;
		papi_func_tbl->GetInterruptThresholds =
			 VL53L0_GetInterruptThresholds;
		papi_func_tbl->ClearInterruptMask =
			 VL53L0_ClearInterruptMask;
		papi_func_tbl->GetInterruptMaskStatus =
				 VL53L0_GetInterruptMaskStatus;
		papi_func_tbl->EnableInterruptMask = VL53L0_EnableInterruptMask;
		papi_func_tbl->SetSpadAmbientDamperThreshold =
			VL53L0_SetSpadAmbientDamperThreshold;
		papi_func_tbl->GetSpadAmbientDamperThreshold =
			VL53L0_GetSpadAmbientDamperThreshold;
		papi_func_tbl->SetSpadAmbientDamperFactor =
			VL53L0_SetSpadAmbientDamperFactor;
		papi_func_tbl->GetSpadAmbientDamperFactor =
			VL53L0_GetSpadAmbientDamperFactor;
		papi_func_tbl->PerformRefSpadManagement =
			 VL53L0_PerformRefSpadManagement;
		papi_func_tbl->SetReferenceSpads = VL53L0_SetReferenceSpads;
		papi_func_tbl->GetReferenceSpads = VL53L0_GetReferenceSpads;
	} else if (revision == 0) {
		/*cut 1.0*/
		vl53l0_errmsg("to setup API cut 1.0\n");
		papi_func_tbl->GetVersion = VL53L010_GetVersion;
		papi_func_tbl->GetPalSpecVersion = VL53L010_GetPalSpecVersion;
		/* papi_func_tbl->GetProductRevision = NULL;*/
		papi_func_tbl->GetDeviceInfo = VL53L010_GetDeviceInfo;
		papi_func_tbl->GetDeviceErrorStatus =
					 VL53L010_GetDeviceErrorStatus;
		papi_func_tbl->GetDeviceErrorString =
					 VL53L010_GetDeviceErrorString;
		papi_func_tbl->GetPalErrorString =
					 VL53L010_GetPalErrorString;
		papi_func_tbl->GetPalState =
					 VL53L010_GetPalState;
		papi_func_tbl->SetPowerMode =
					 VL53L010_SetPowerMode;
		papi_func_tbl->GetPowerMode =
					 VL53L010_GetPowerMode;
		papi_func_tbl->SetOffsetCalibrationDataMicroMeter =
			VL53L010_SetOffsetCalibrationDataMicroMeter;
		papi_func_tbl->GetOffsetCalibrationDataMicroMeter =
			VL53L010_GetOffsetCalibrationDataMicroMeter;
		papi_func_tbl->SetGroupParamHold =
					 VL53L010_SetGroupParamHold;
		papi_func_tbl->GetUpperLimitMilliMeter =
					 VL53L010_GetUpperLimitMilliMeter;
		papi_func_tbl->SetDeviceAddress =
					 VL53L010_SetDeviceAddress;
		papi_func_tbl->DataInit = VL53L010_DataInit;
		/*
		 *papi_func_tbl->SetTuningSettingBuffer = NULL;
		 *papi_func_tbl->GetTuningSettingBuffer = NULL;
		*/
		papi_func_tbl->StaticInit = VL53L010_StaticInit;
		papi_func_tbl->WaitDeviceBooted = VL53L010_WaitDeviceBooted;
		papi_func_tbl->ResetDevice = VL53L010_ResetDevice;
		papi_func_tbl->SetDeviceParameters =
					 VL53L010_SetDeviceParameters;
		papi_func_tbl->SetDeviceMode = VL53L010_SetDeviceMode;
		papi_func_tbl->GetDeviceMode = VL53L010_GetDeviceMode;
		papi_func_tbl->SetHistogramMode = VL53L010_SetHistogramMode;
		papi_func_tbl->GetHistogramMode = VL53L010_GetHistogramMode;
		papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds =
			VL53L010_SetMeasurementTimingBudgetMicroSeconds;
		papi_func_tbl->GetMeasurementTimingBudgetMicroSeconds =
			VL53L010_GetMeasurementTimingBudgetMicroSeconds;
		/*
		 * papi_func_tbl->GetVcselPulsePeriod = NULL;
		 *papi_func_tbl->SetVcselPulsePeriod = NULL;
		 *papi_func_tbl->SetSequenceStepEnable = NULL;
		 *papi_func_tbl->GetSequenceStepEnable = NULL;
		 *papi_func_tbl->GetSequenceStepEnables = NULL;
		 *papi_func_tbl->SetSequenceStepTimeout = NULL;
		 *papi_func_tbl->GetSequenceStepTimeout = NULL;
		 *papi_func_tbl->GetNumberOfSequenceSteps =NULL;
		 *papi_func_tbl->GetSequenceStepsInfo = NULL;
		 */
		papi_func_tbl->SetInterMeasurementPeriodMilliSeconds =
			VL53L010_SetInterMeasurementPeriodMilliSeconds;
		papi_func_tbl->GetInterMeasurementPeriodMilliSeconds =
			VL53L010_GetInterMeasurementPeriodMilliSeconds;
		papi_func_tbl->SetXTalkCompensationEnable =
			VL53L010_SetXTalkCompensationEnable;
		papi_func_tbl->GetXTalkCompensationEnable =
			VL53L010_GetXTalkCompensationEnable;
		papi_func_tbl->SetXTalkCompensationRateMegaCps =
			VL53L010_SetXTalkCompensationRateMegaCps;
		papi_func_tbl->GetXTalkCompensationRateMegaCps =
			VL53L010_GetXTalkCompensationRateMegaCps;
		papi_func_tbl->GetNumberOfLimitCheck =
				 VL53L010_GetNumberOfLimitCheck;
		papi_func_tbl->GetLimitCheckInfo = VL53L010_GetLimitCheckInfo;
		papi_func_tbl->SetLimitCheckEnable =
					 VL53L010_SetLimitCheckEnable;
		papi_func_tbl->GetLimitCheckEnable =
					 VL53L010_GetLimitCheckEnable;
		papi_func_tbl->SetLimitCheckValue =
					 VL53L010_SetLimitCheckValue;
		papi_func_tbl->GetLimitCheckValue =
					 VL53L010_GetLimitCheckValue;
		papi_func_tbl->GetLimitCheckCurrent =
					 VL53L010_GetLimitCheckCurrent;
		papi_func_tbl->SetWrapAroundCheckEnable =
			VL53L010_SetWrapAroundCheckEnable;
		papi_func_tbl->GetWrapAroundCheckEnable =
			VL53L010_GetWrapAroundCheckEnable;
		papi_func_tbl->PerformSingleMeasurement =
			VL53L010_PerformSingleMeasurement;
		/*papi_func_tbl->PerformRefCalibration =
		 * VL53L010_PerformRefCalibration;
		 */
		papi_func_tbl->PerformRefCalibration = NULL;
		papi_func_tbl->SetRefCalibration = NULL;
		papi_func_tbl->GetRefCalibration = NULL;
		papi_func_tbl->PerformXTalkCalibration =
				 VL53L010_PerformXTalkCalibration;
		papi_func_tbl->PerformOffsetCalibration =
			VL53L010_PerformOffsetCalibration;
		papi_func_tbl->StartMeasurement = VL53L010_StartMeasurement;
		papi_func_tbl->StopMeasurement = VL53L010_StopMeasurement;
		papi_func_tbl->GetMeasurementDataReady =
					 VL53L010_GetMeasurementDataReady;
		papi_func_tbl->WaitDeviceReadyForNewMeasurement =
			VL53L010_WaitDeviceReadyForNewMeasurement;
		papi_func_tbl->GetRangingMeasurementData =
			VL53L010_GetRangingMeasurementData;
		papi_func_tbl->GetHistogramMeasurementData =
			VL53L010_GetHistogramMeasurementData;
		papi_func_tbl->PerformSingleRangingMeasurement =
			VL53L010_PerformSingleRangingMeasurement;
		papi_func_tbl->PerformSingleHistogramMeasurement =
			VL53L010_PerformSingleHistogramMeasurement;
		papi_func_tbl->SetNumberOfROIZones =
				 VL53L010_SetNumberOfROIZones;
		papi_func_tbl->GetNumberOfROIZones =
				 VL53L010_GetNumberOfROIZones;
		papi_func_tbl->GetMaxNumberOfROIZones =
				 VL53L010_GetMaxNumberOfROIZones;
		papi_func_tbl->SetGpioConfig = VL53L010_SetGpioConfig;
		papi_func_tbl->GetGpioConfig = VL53L010_GetGpioConfig;
		papi_func_tbl->SetInterruptThresholds =
				 VL53L010_SetInterruptThresholds;
		papi_func_tbl->GetInterruptThresholds =
				 VL53L010_GetInterruptThresholds;
		papi_func_tbl->ClearInterruptMask =
				 VL53L010_ClearInterruptMask;
		papi_func_tbl->GetInterruptMaskStatus =
				 VL53L010_GetInterruptMaskStatus;
		papi_func_tbl->EnableInterruptMask =
				 VL53L010_EnableInterruptMask;
		papi_func_tbl->SetSpadAmbientDamperThreshold =
			VL53L010_SetSpadAmbientDamperThreshold;
		papi_func_tbl->GetSpadAmbientDamperThreshold =
			VL53L010_GetSpadAmbientDamperThreshold;
		papi_func_tbl->SetSpadAmbientDamperFactor =
			VL53L010_SetSpadAmbientDamperFactor;
		papi_func_tbl->GetSpadAmbientDamperFactor =
			VL53L010_GetSpadAmbientDamperFactor;
		papi_func_tbl->PerformRefSpadManagement = NULL;
		papi_func_tbl->SetReferenceSpads = NULL;
		papi_func_tbl->GetReferenceSpads = NULL;
	}

}

static void stmvl53l0_ps_read_measurement(struct stmvl53l0_data *data)
{
	struct timeval tv;
	VL53L0_DEV vl53l0_dev = data;
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	FixPoint1616_t LimitCheckCurrent;

	do_gettimeofday(&tv);

	data->ps_data = data->rangeData.RangeMilliMeter;
	input_report_abs(data->input_dev_ps, ABS_DISTANCE,
		(int)(data->ps_data + 5) / 10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X, tv.tv_sec);
	input_report_abs(data->input_dev_ps, ABS_HAT0Y, tv.tv_usec);
	input_report_abs(data->input_dev_ps, ABS_HAT1X,
		data->rangeData.RangeMilliMeter);
	input_report_abs(data->input_dev_ps, ABS_HAT1Y,
		data->rangeData.RangeStatus);
	input_report_abs(data->input_dev_ps, ABS_HAT2X,
		data->rangeData.SignalRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT2Y,
		data->rangeData.AmbientRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT3X,
		data->rangeData.MeasurementTimeUsec);
	input_report_abs(data->input_dev_ps, ABS_HAT3Y,
		data->rangeData.RangeDMaxMilliMeter);
	Status = papi_func_tbl->GetLimitCheckCurrent(vl53l0_dev,
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
					&LimitCheckCurrent);
	if (Status == VL53L0_ERROR_NONE) {
		input_report_abs(data->input_dev_ps, ABS_WHEEL,
				LimitCheckCurrent);
	}
	input_report_abs(data->input_dev_ps, ABS_PRESSURE,
		data->rangeData.EffectiveSpadRtnCount);
	input_sync(data->input_dev_ps);

	if (data->enableDebug)
		vl53l0_errmsg(
"range:%d, RtnRateMcps:%d,err:0x%x,Dmax:%d,rtnambr:%d,time:%d,Spad:%d,SigmaLimit:%d\n",
			data->rangeData.RangeMilliMeter,
			data->rangeData.SignalRateRtnMegaCps,
			data->rangeData.RangeStatus,
			data->rangeData.RangeDMaxMilliMeter,
			data->rangeData.AmbientRateRtnMegaCps,
			data->rangeData.MeasurementTimeUsec,
			data->rangeData.EffectiveSpadRtnCount,
			LimitCheckCurrent
			);


}

static void stmvl53l0_cancel_handler(struct stmvl53l0_data *data)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	ret = cancel_delayed_work(&data->dwork);
	if (ret == 0)
		vl53l0_errmsg("cancel_delayed_work return FALSE\n");

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

}

void stmvl53l0_schedule_handler(struct stmvl53l0_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->dwork);
	schedule_delayed_work(&data->dwork, 0);
	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

}


#ifdef USE_INT
static irqreturn_t stmvl53l0_interrupt_handler(int vec, void *info)
{

	struct stmvl53l0_data *data = (struct stmvl53l0_data *)info;

	if (data->irq == vec) {
		data->interrupt_received = 1;
		schedule_delayed_work(&data->dwork, 0);
	}
	return IRQ_HANDLED;
}
#else
/* Flag used to exit the thread when kthread_stop() is invoked */
static int poll_thread_exit;
int stmvl53l0_poll_thread(void *data)
{
	VL53L0_DEV vl53l0_dev = data;
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint32_t sleep_time = 0;
	uint32_t interruptStatus = 0;

	pr_err("%s(%d) : Starting Polling thread\n", __func__, __LINE__);

	while (!kthread_should_stop()) {
		/* Check if enable_ps_sensor is true or exit request is made.
		 * If not block
		 */
		wait_event(vl53l0_dev->poll_thread_wq,
			(vl53l0_dev->enable_ps_sensor || poll_thread_exit));
		if (poll_thread_exit) {
			pr_err(
		"%s(%d) : Exiting the poll thread\n", __func__, __LINE__);
			break;
		}

		mutex_lock(&vl53l0_dev->work_mutex);

		sleep_time = vl53l0_dev->delay_ms;
		Status = VL53L0_GetInterruptMaskStatus(vl53l0_dev,
					 &interruptStatus);
		if (Status == VL53L0_ERROR_NONE &&
			interruptStatus &&
			interruptStatus != vl53l0_dev->interruptStatus) {
			vl53l0_dev->interruptStatus = interruptStatus;
			vl53l0_dev->noInterruptCount = 0;
			stmvl53l0_schedule_handler(vl53l0_dev);
		} else {
			vl53l0_dev->noInterruptCount++;
		}

		/*Force Clear interrupt mask and restart if
		 *no interrupt after twice the timingBudget
		 */
		if ((vl53l0_dev->noInterruptCount * vl53l0_dev->delay_ms) >
			 (vl53l0_dev->timingBudget * 2)) {
			pr_err("No interrupt after (%u) msec(TimingBudget = %u) . Clear Interrupt Mask and restart\n",
				(vl53l0_dev->noInterruptCount *
					 vl53l0_dev->delay_ms),
					vl53l0_dev->timingBudget);
			Status = papi_func_tbl->ClearInterruptMask(vl53l0_dev,
					 0);
			if (vl53l0_dev->deviceMode ==
			 VL53L0_DEVICEMODE_SINGLE_RANGING) {
				Status = papi_func_tbl->StartMeasurement(
							vl53l0_dev);
				if (Status != VL53L0_ERROR_NONE) {
					pr_err("%s(%d) : Status = %d\n",
						 __func__, __LINE__, Status);
				}
			}
		}
		mutex_unlock(&vl53l0_dev->work_mutex);
		/* Sleep for delay_ms milliseconds */
		msleep(sleep_time);
	}

	return 0;
}
#endif

/* work handler */
static void stmvl53l0_work_handler(struct work_struct *work)
{
	struct stmvl53l0_data *data = container_of(work, struct stmvl53l0_data,
				dwork.work);
	VL53L0_DEV vl53l0_dev = data;

	VL53L0_Error Status = VL53L0_ERROR_NONE;

	mutex_lock(&data->work_mutex);
	/* vl53l0_dbgmsg("Enter\n"); */

	if (pmodule_func_tbl->query_power_status(data->client_object) == 0) {
		if (data->enable_ps_sensor == 1) {
			stmvl53l0_stop(data);
			data->enable_ps_sensor = 0;
		}
	}

	if (vl53l0_dev->enable_ps_sensor == 1) {
#ifdef DEBUG_TIME_LOG
		stmvl53l0_DebugTimeGet(&stop_tv);
		stmvl53l0_DebugTimeDuration(&start_tv, &stop_tv);
#endif
		/* Check if ISR has scheduled this function */
		if (vl53l0_dev->interrupt_received == 1) {
			Status = papi_func_tbl->GetInterruptMaskStatus(
					vl53l0_dev,
					&vl53l0_dev->interruptStatus);
			if (Status != VL53L0_ERROR_NONE)
				pr_err("%s(%d) : Status = %d\n",
						 __func__, __LINE__, Status);
			vl53l0_dev->interrupt_received = 0;
		}
		if (data->enableDebug)
			pr_err("interruptStatus:0x%x, interrupt_received:%d\n",
				vl53l0_dev->interruptStatus,
				vl53l0_dev->interrupt_received);

		if (vl53l0_dev->interruptStatus == vl53l0_dev->gpio_function) {
			Status =
				papi_func_tbl->GetRangingMeasurementData(
						vl53l0_dev,
						&(data->rangeData));
			/* to push the measurement */
			if (Status == VL53L0_ERROR_NONE)
				stmvl53l0_ps_read_measurement(data);
			else
				pr_err("%s(%d) : Status = %d\n",
						__func__, __LINE__, Status);

			if (data->enableDebug)
				pr_err("Measured range:%d\n",
					data->rangeData.RangeMilliMeter);

			Status = papi_func_tbl->ClearInterruptMask(
					vl53l0_dev, 0);
			if (Status != VL53L0_ERROR_NONE) {
				pr_err("%s(%d) : Status = %d\n",
						__func__, __LINE__, Status);
			}

			if (data->deviceMode ==
					VL53L0_DEVICEMODE_SINGLE_RANGING) {
				/* Before restarting measurement
				 *  check if use case needs to be changed
				 */

				if (data->updateUseCase) {
					Status =
						stmvl53l0_config_use_case(data);
					if (Status !=
							VL53L0_ERROR_NONE) {
						vl53l0_errmsg(
					"Failed to configure Use case = %u\n",
							vl53l0_dev->useCase);
					} else {
						data->updateUseCase = 0;
					}
				}
				Status =
					papi_func_tbl->StartMeasurement(
							vl53l0_dev);
			}
		}
#ifdef DEBUG_TIME_LOG
		stmvl53l0_DebugTimeGet(&start_tv);
#endif

		}


	vl53l0_dev->interruptStatus = 0;

	mutex_unlock(&data->work_mutex);

}


/*
 * SysFS support
 */
static ssize_t stmvl53l0_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl53l0_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	unsigned long val = 0;

	int ret = kstrtoul(buf, 10, &val);

	if (ret != 0)
		return ret;

	if ((val != 0) && (val != 1)) {
		vl53l0_errmsg("store unvalid value = %lu\n", val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, enable_ps_sensor flag:%d\n",
		data->enable_ps_sensor);
	vl53l0_dbgmsg("enable ps senosr ( %ld)\n", val);

	if (val == 1) {
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, NORMAL_MODE);
		} else {
			vl53l0_errmsg("Already enabled. Skip !");
		}
	} else {
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
	}
	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, 0664/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0_show_enable_ps_sensor,
					stmvl53l0_store_enable_ps_sensor);

static ssize_t stmvl53l0_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enableDebug);
}

/* for debug */
static ssize_t stmvl53l0_store_enable_debug(struct device *dev,
					struct device_attribute *attr, const
					char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	unsigned long on = 0;

	int ret = kstrtoul(buf, 10, &on);

	if (ret != 0)
		return ret;

	if ((on != 0) &&  (on != 1)) {
		vl53l0_errmsg("set debug=%lu\n", on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(enable_debug, 0660/*S_IWUSR | S_IRUGO*/,
				   stmvl53l0_show_enable_debug,
					stmvl53l0_store_enable_debug);

static ssize_t stmvl53l0_show_set_delay_ms(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl53l0_store_set_delay_ms(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	unsigned long delay_ms = 0;

	int ret = kstrtoul(buf, 10, &delay_ms);

	if (ret != 0)
		return ret;
	if (delay_ms == 0) {
		vl53l0_errmsg("set delay_ms=%lu\n", delay_ms);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->delay_ms = delay_ms;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_delay_ms, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0_show_set_delay_ms,
					stmvl53l0_store_set_delay_ms);

/* Timing Budget */
static ssize_t stmvl53l0_show_timing_budget(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n", data->timingBudget);
}

static ssize_t stmvl53l0_store_set_timing_budget(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	unsigned long timingBudget = 0;
	int ret = kstrtoul(buf, 10, &timingBudget);

	if (ret != 0)
		return ret;
	if (timingBudget == 0) {
		vl53l0_errmsg("set timingBudget=%lu\n", timingBudget);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->timingBudget = timingBudget;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_timing_budget, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0_show_timing_budget,
					stmvl53l0_store_set_timing_budget);


/* Use case  */
static ssize_t stmvl53l0_show_use_case(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	switch (data->useCase) {
	case USE_CASE_LONG_DISTANCE:
		return snprintf(buf, 20, "Long Distance\n");
	case USE_CASE_HIGH_ACCURACY:
		return snprintf(buf, 20, "High Accuracy\n");
	case USE_CASE_HIGH_SPEED:
		return snprintf(buf, 20, "High Speed\n");
	default:
		break;
	}

	return snprintf(buf, 25, "Unknown use case\n");
}

static ssize_t stmvl53l0_store_set_use_case(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	unsigned long useCase = 0;
	int ret = kstrtoul(buf, 10, &useCase);

	if (ret != 0)
		return ret;


	mutex_lock(&data->work_mutex);

	if (useCase == USE_CASE_LONG_DISTANCE) {
		data->timingBudget = LONG_DISTANCE_TIMING_BUDGET;
	} else if (useCase == USE_CASE_HIGH_SPEED) {
		data->timingBudget = HIGH_SPEED_TIMING_BUDGET;
	} else if (useCase == USE_CASE_HIGH_ACCURACY) {
		data->timingBudget = HIGH_ACCURACY_TIMING_BUDGET;
	} else {
		count = -EINVAL;
		mutex_unlock(&data->work_mutex);
		return count;
	}

	data->useCase = useCase;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_use_case, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0_show_use_case,
					stmvl53l0_store_set_use_case);


/* Get Current configuration info */
static ssize_t stmvl53l0_show_current_configuration(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *vl53l0_dev = dev_get_drvdata(dev);
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	int ret = -1;
	FixPoint1616_t	LimitValue = 0;
	uint8_t LimitEnable = 0;
	uint32_t refSpadCount = 0;
	uint8_t isApertureSpads = 0;
	uint8_t VhvSettings = 0;
	uint8_t PhaseCal = 0;
	uint8_t pulsePeriod = 0;
	uint32_t timingBudget = 0;
	int32_t offsetCalibrationDataMicroMeter = -1;
	uint8_t XTalkCompensationEnable = 0;
	FixPoint1616_t xTalkCompensationRateMegaCps = 0;


	ret = scnprintf(buf, PAGE_SIZE, "VL53L0 current configuration:\n");

	mutex_lock(&vl53l0_dev->work_mutex);
	pr_err("Driver Config:UseCase:%d, offsetCalDistance:%u,xtalkCalDistance:%u,setCalibratedValue:0x%X\n",
			vl53l0_dev->useCase, vl53l0_dev->offsetCalDistance,
			vl53l0_dev->xtalkCalDistance,
			vl53l0_dev->setCalibratedValue);

	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"Driver Config:UseCase:%d, offsetCalDistance:%u,xtalkCalDistance:%u,setCalibratedValue:0x%X\n",
			vl53l0_dev->useCase,
			vl53l0_dev->offsetCalDistance,
			vl53l0_dev->xtalkCalDistance,
			vl53l0_dev->setCalibratedValue);

	if (vl53l0_dev->useCase == USE_CASE_CUSTOM) {
		pr_err("CustomUseCase : Sigma=%u :Signal=%u: Pre=%u :Final=%u\n",
					vl53l0_dev->sigmaLimit,
					vl53l0_dev->signalRateLimit,
					vl53l0_dev->preRangePulsePeriod,
					vl53l0_dev->finalRangePulsePeriod);

		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				"CustomUseCase : Sigma=%u :Signal=%u: Pre=%u :Final=%u\n",
					vl53l0_dev->sigmaLimit,
					vl53l0_dev->signalRateLimit,
					vl53l0_dev->preRangePulsePeriod,
					vl53l0_dev->finalRangePulsePeriod);
	}


	Status = papi_func_tbl->GetLimitCheckValue(vl53l0_dev,
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
					&LimitValue);

	if (Status == VL53L0_ERROR_NONE) {
		Status = papi_func_tbl->GetLimitCheckEnable(vl53l0_dev,
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
					&LimitEnable);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Get LimitCheckValue SIGMA_FINAL_RANGE as:%d,Enable:%d\n",
				(LimitValue>>16), LimitEnable);
		ret	+= scnprintf(buf + ret, PAGE_SIZE - ret,
					 "Sigma Limit:%u, Enable:%u\n",
					(LimitValue>>16),
					 LimitEnable);
		Status = papi_func_tbl->GetLimitCheckValue(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				&LimitValue);
		Status = papi_func_tbl->GetLimitCheckEnable(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				&LimitEnable);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Get LimitCheckValue SIGNAL_FINAL_RANGE as:%d(Fix1616),Enable:%d\n",
				(LimitValue), LimitEnable);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
							 "SIGNAL Limit:%u, Enable:%u\n",
							LimitValue,
							LimitEnable);

		Status = papi_func_tbl->GetLimitCheckValue(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_REF_CLIP,
				&LimitValue);
		Status = papi_func_tbl->GetLimitCheckEnable(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_REF_CLIP,
				&LimitEnable);

	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Get LimitCheckValue SIGNAL_REF_CLIP as:%d(fix1616),Enable:%d\n",
				(LimitValue), LimitEnable);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					"RefClipLimit:%u, Enable:%u\n",
					LimitValue,
					LimitEnable);

		Status = papi_func_tbl->GetLimitCheckValue(vl53l0_dev,
				VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				&LimitValue);
		Status = papi_func_tbl->GetLimitCheckEnable(vl53l0_dev,
				VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				&LimitEnable);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err(
	"Get LimitCheckValue RANGE_IGNORE_THRESHOLDas:%d(fix1616),Enable:%d\n",
				(LimitValue),
				LimitEnable);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "RngIgnoreThresh:%u, Enable:%u\n",
				LimitValue,
				LimitEnable);

		Status = papi_func_tbl->GetRefCalibration(vl53l0_dev,
				&VhvSettings, &PhaseCal); /* Ref calibration */


	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("GetRefCalibration - Vhv = %u, PhaseCal = %u\n",
						VhvSettings, PhaseCal);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					"Vhv:%u, PhCal:%u\n",
					VhvSettings,
					PhaseCal);

		/* Ref Spad Management */
		Status = papi_func_tbl->GetReferenceSpads(vl53l0_dev,
			&refSpadCount, &isApertureSpads);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("GetSpads - Count = %u, IsAperture = %u\n",
						refSpadCount, isApertureSpads);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
						"SpadCount:%u, IsAperture:%u\n",
						refSpadCount,
						isApertureSpads);

		Status = papi_func_tbl->GetMeasurementTimingBudgetMicroSeconds(
					vl53l0_dev,
					&timingBudget);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("TimingBudget = %u\n", timingBudget);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "TimBudget:%u\n",
								timingBudget);
		Status = papi_func_tbl->GetVcselPulsePeriod(vl53l0_dev,
					VL53L0_VCSEL_PERIOD_PRE_RANGE,
					&pulsePeriod);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("GetVcselPulsePeriod - PRE_RANGE = %u\n",
						pulsePeriod);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					 "PulsePreRange:%u\n",
					pulsePeriod);

		Status = papi_func_tbl->GetVcselPulsePeriod(vl53l0_dev,
					VL53L0_VCSEL_PERIOD_FINAL_RANGE,
					&pulsePeriod);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("GetVcselPulsePeriod - FINAL_RANGE = %u\n",
						pulsePeriod);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "PulseFinalRange:%u\n",
				pulsePeriod);

		Status = papi_func_tbl->GetOffsetCalibrationDataMicroMeter(
				vl53l0_dev,
				&offsetCalibrationDataMicroMeter);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("OffsetCalibrationDataMicroMeter = %d\n",
				 offsetCalibrationDataMicroMeter);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Offset:%d\n",
					offsetCalibrationDataMicroMeter);

		Status = papi_func_tbl->GetXTalkCompensationEnable(vl53l0_dev,
					&XTalkCompensationEnable);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Xtalk Enable = %u\n", XTalkCompensationEnable);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "XtalkEnable:%u\n",
					XTalkCompensationEnable);

		Status = papi_func_tbl->GetXTalkCompensationRateMegaCps(
					vl53l0_dev,
					&xTalkCompensationRateMegaCps);
	}


	if (Status == VL53L0_ERROR_NONE) {
		pr_err("XtalkComp MCPS = %u\n", xTalkCompensationRateMegaCps);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "XtalkMcps:%u\n",
					xTalkCompensationRateMegaCps);

	} else {
		pr_err("Error = %d\n", Status);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Error:%d\n",
					Status);

	}

	pr_err("Total Bytes returned = %d\n", ret);

	mutex_unlock(&vl53l0_dev->work_mutex);


	return ret;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(show_current_configuration, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0_show_current_configuration,
					NULL);
/* for work handler scheduler time */
static ssize_t stmvl53l0_do_flush(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	int ret = 0;
	/* Revisit : Avoid lock if it takes too long time */
	mutex_lock(&data->work_mutex);

	vl53l0_dbgmsg("Starting timer to fire in 1ms (%ld)\n", jiffies);
	ret = mod_timer(&data->timer, jiffies + msecs_to_jiffies(1));
	if (ret)
		pr_err("Error from mod_timer = %d\n", ret);

	mutex_unlock(&data->work_mutex);
	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(do_flush, 0660/*S_IWUGO | S_IRUGO*/,
				   NULL,
					stmvl53l0_do_flush);
static struct attribute *stmvl53l0_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_set_delay_ms.attr,
	&dev_attr_set_timing_budget.attr,
	&dev_attr_set_use_case.attr,
	&dev_attr_do_flush.attr,
	&dev_attr_show_current_configuration.attr,
	NULL
};


static const struct attribute_group stmvl53l0_attr_group = {
	.attrs = stmvl53l0_attributes,
};

/*
 * misc device file operation functions
 */
static int stmvl53l0_ioctl_handler(struct file *file,
			unsigned int cmd, unsigned long arg,
			void __user *p)
{
	int rc = 0;
	unsigned int xtalkint = 0;
	unsigned int targetDistance = 0;
	int8_t offsetint = 0;
	uint8_t useCase = 0;
	struct stmvl53l0_custom_use_case customUseCase;
	struct stmvl53l0_data *data =
			container_of(file->private_data,
				struct stmvl53l0_data, miscdev);
	struct stmvl53l0_register reg;
	struct stmvl53l0_parameter parameter;
	VL53L0_DEV vl53l0_dev = data;
	VL53L0_DeviceModes deviceMode;
	uint8_t page_num = 0;
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	if (!data)
		return -EINVAL;

	vl53l0_dbgmsg("Enter enable_ps_sensor:%d\n", data->enable_ps_sensor);
	switch (cmd) {
	/* enable */
	case VL53L0_IOCTL_INIT:
		vl53l0_dbgmsg("VL53L0_IOCTL_INIT\n");
		/* turn on tof sensor only if it's not enabled by other
		 * client
		 */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, NORMAL_MODE);
		} else
			rc = -EINVAL;
		break;
	/* crosstalk calibration */
	case VL53L0_IOCTL_XTALKCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_XTALKCALB\n");
		data->xtalkCalDistance = 100;
		if (copy_from_user(&targetDistance, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		data->xtalkCalDistance = targetDistance;

		/* turn on tof sensor only if it's not enabled by other
		 * client
		 */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, XTALKCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up Xtalk value */
	case VL53L0_IOCTL_SETXTALK:
		vl53l0_dbgmsg("VL53L0_IOCTL_SETXTALK\n");
		if (copy_from_user(&xtalkint, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0_dbgmsg("SETXTALK as 0x%x\n", xtalkint);

		/* later
		* SetXTalkCompensationRate(vl53l0_dev, xtalkint);
		*/
		break;
	/* offset calibration */
	case VL53L0_IOCTL_OFFCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_OFFCALB\n");
		data->offsetCalDistance = 50;
		if (copy_from_user(&targetDistance, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		data->offsetCalDistance = targetDistance;
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, OFFSETCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up offset value */
	case VL53L0_IOCTL_SETOFFSET:
		vl53l0_dbgmsg("VL53L0_IOCTL_SETOFFSET\n");
		if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0_dbgmsg("SETOFFSET as %d\n", offsetint);

		/* later
		* SetOffsetCalibrationData(vl53l0_dev, offsetint);
		*/
		break;

	/* Config use case */
	case VL53L0_IOCTL_ACTIVATE_USE_CASE:
		vl53l0_dbgmsg("VL53L0_IOCTL_ACTIVATE_USE_CASE\n");
		if (copy_from_user(&useCase, (uint8_t *)p, sizeof(uint8_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}

		/* Validate the user passed use case.
		 * Update the timingBudget value. The other
		 * parameters are updated approrpiately in config_use_case()
	     * Currently the timing budget can be updated through
	     * sysfs entry, and this needs additional steps to manage.
		 */
		switch (useCase) {
		case USE_CASE_LONG_DISTANCE:
			data->timingBudget = LONG_DISTANCE_TIMING_BUDGET;
			break;

		case USE_CASE_HIGH_ACCURACY:
			data->timingBudget = HIGH_ACCURACY_TIMING_BUDGET;
			break;

		case USE_CASE_HIGH_SPEED:
			data->timingBudget = HIGH_SPEED_TIMING_BUDGET;
			break;

		default:
			vl53l0_errmsg("%d, Unknown Use case = %u\n", __LINE__,
				 useCase);
			return -EFAULT;
		}
		vl53l0_dbgmsg("useCase as %d\n", useCase);
		/* record the use case */
		data->useCase = useCase;

		/* If ranging is in progress, let the work handler
		 * update the use case
		 */
		if (data->enable_ps_sensor)
			data->updateUseCase = 1;
		break;

	/* Config Custom use case */
	case VL53L0_IOCTL_ACTIVATE_CUSTOM_USE_CASE:
		vl53l0_dbgmsg("VL53L0_IOCTL_ACTIVATE_CUSTOM_USE_CASE\n");
		if (copy_from_user(&customUseCase,
			 (struct stmvl53l0_custom_use_case *)p,
			sizeof(struct stmvl53l0_custom_use_case))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}

		data->sigmaLimit = customUseCase.sigmaLimit;
		data->signalRateLimit = customUseCase.signalRateLimit;
		data->preRangePulsePeriod = customUseCase.preRangePulsePeriod;
		data->finalRangePulsePeriod =
					customUseCase.finalRangePulsePeriod;
		data->timingBudget = customUseCase.timingBudget;
		vl53l0_dbgmsg(
			"Sigma=%u,Signal=%u,Pre=%u,Final=%u,timingBudget=%u\n",
						data->sigmaLimit,
						 data->signalRateLimit,
						data->preRangePulsePeriod,
						 data->finalRangePulsePeriod,
						data->timingBudget);

		/* record the use case */
		data->useCase = USE_CASE_CUSTOM;
		/* If ranging is in progress,
		 * let the work handler update the use case
		 */
		if (data->enable_ps_sensor)
			data->updateUseCase = 1;

		break;


	/* disable */
	case VL53L0_IOCTL_STOP:
		vl53l0_dbgmsg("VL53L0_IOCTL_STOP\n");
		/* turn off tof sensor only if it's enabled by other client */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
		break;
	/* Get all range data */
	case VL53L0_IOCTL_GETDATAS:
		vl53l0_dbgmsg("VL53L0_IOCTL_GETDATAS\n");
		if (copy_to_user((VL53L0_RangingMeasurementData_t *)p,
			&(data->rangeData),
			sizeof(VL53L0_RangingMeasurementData_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* Register tool */
	case VL53L0_IOCTL_REGISTER:
		vl53l0_dbgmsg("VL53L0_IOCTL_REGISTER\n");
		if (copy_from_user(&reg, (struct stmvl53l0_register *)p,
			sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		reg.status = 0;
		page_num = (uint8_t)((reg.reg_index & 0x0000ff00) >> 8);
		vl53l0_dbgmsg(
"VL53L0_IOCTL_REGISTER,	page number:%d\n", page_num);
		if (page_num != 0)
			reg.status = VL53L0_WrByte(vl53l0_dev, 0xFF, page_num);

		switch (reg.reg_bytes) {
		case(4):
			if (reg.is_read)
				reg.status = VL53L0_RdDWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					&reg.reg_data);
			else
				reg.status = VL53L0_WrDWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					reg.reg_data);
			break;
		case(2):
			if (reg.is_read)
				reg.status = VL53L0_RdWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint16_t *)&reg.reg_data);
			else
				reg.status = VL53L0_WrWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint16_t)reg.reg_data);
			break;
		case(1):
			if (reg.is_read)
				reg.status = VL53L0_RdByte(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint8_t *)&reg.reg_data);
			else
				reg.status = VL53L0_WrByte(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint8_t)reg.reg_data);
			break;
		default:
			reg.status = -1;

		}
		if (page_num != 0)
			reg.status = VL53L0_WrByte(vl53l0_dev, 0xFF, 0);


		if (copy_to_user((struct stmvl53l0_register *)p, &reg,
				sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* parameter access */
	case VL53L0_IOCTL_PARAMETER:
		vl53l0_dbgmsg("VL53L0_IOCTL_PARAMETER\n");
		if (copy_from_user(&parameter, (struct stmvl53l0_parameter *)p,
				sizeof(struct stmvl53l0_parameter))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		parameter.status = 0;
		if (data->enableDebug)
			vl53l0_dbgmsg(
			"VL53L0_IOCTL_PARAMETER Name = %d\n", parameter.name);
		switch (parameter.name) {
		case (OFFSET_PAR):
			if (parameter.is_read)
				parameter.status =
			    papi_func_tbl->GetOffsetCalibrationDataMicroMeter(
						vl53l0_dev, &parameter.value);
			else {
				parameter.status =
			    papi_func_tbl->SetOffsetCalibrationDataMicroMeter(
						vl53l0_dev, parameter.value);
				  data->OffsetMicroMeter = parameter.value;
				  data->setCalibratedValue
			       |= SET_OFFSET_CALIB_DATA_MICROMETER_MASK;

			}
			vl53l0_dbgmsg("get parameter value as %d\n",
				 parameter.value);
			break;

		case (REFERENCESPADS_PAR):
			if (parameter.is_read) {
				parameter.status =
				  papi_func_tbl->GetReferenceSpads(vl53l0_dev,
						(uint32_t *)&(parameter.value),
				    (uint8_t *)&(parameter.value2));
				if (data->enableDebug)
					vl53l0_dbgmsg(
						"Get RefSpad : Count:%u, Type:%u\n",
						parameter.value,
						(uint8_t)parameter.value2);
			} else {
				if (data->enableDebug)
					vl53l0_dbgmsg(
					"Set RefSpad : Count:%u, Type:%u\n",
					parameter.value,
					(uint8_t)parameter.value2);

				parameter.status =
				  papi_func_tbl->SetReferenceSpads(vl53l0_dev,
				(uint32_t)(parameter.value),
				(uint8_t)(parameter.value2));

				data->refSpadCount = parameter.value;
				data->isApertureSpads =
					(uint8_t)(parameter.value2);
			}
			break;

		case (REFCALIBRATION_PAR):
			if (parameter.is_read) {
				parameter.status =
			     papi_func_tbl->GetRefCalibration(vl53l0_dev,
			     (uint8_t *)&(parameter.value),
			     (uint8_t *)&(parameter.value2));
					if (data->enableDebug)
						vl53l0_dbgmsg(
						"Get Ref : Vhv:%u, PhaseCal:%u\n",
						(uint8_t)parameter.value,
						(uint8_t)parameter.value2);
			} else {
				if (data->enableDebug)
					vl53l0_dbgmsg(
					"Set Ref : Vhv:%u, PhaseCal:%u\n",
					(uint8_t)parameter.value,
					(uint8_t)parameter.value2);
					parameter.status =
					    papi_func_tbl->SetRefCalibration(
					    vl53l0_dev,
					    (uint8_t)(parameter.value),
					    (uint8_t)(parameter.value2));
				data->VhvSettings = (uint8_t)parameter.value;
				data->PhaseCal    = (uint8_t)(parameter.value2);
			}
			break;
		case (XTALKRATE_PAR):
			if (parameter.is_read)
				parameter.status =
				papi_func_tbl->GetXTalkCompensationRateMegaCps(
				    vl53l0_dev,
				    (FixPoint1616_t *)
				    &parameter.value);
			else {
				/* Range Ignore Threshold value */
				FixPoint1616_t ritValue = 0;

				parameter.status =
			papi_func_tbl->SetXTalkCompensationRateMegaCps(
						vl53l0_dev,
						(FixPoint1616_t)
							parameter.value);
				data->XTalkCompensationRateMegaCps =
					 parameter.value;
				data->setCalibratedValue |=
					 SET_XTALK_COMP_RATE_MCPS_MASK;


				/*0.7 KCps converted to MCps */
				if (data->XTalkCompensationRateMegaCps <
						 7*65536/10000) {
					ritValue = 15 * 7 * 65536/100000;
				} else {
					ritValue = 15 *
					vl53l0_dev->XTalkCompensationRateMegaCps
							 /10;
				}

				if (papi_func_tbl->SetLimitCheckEnable
					!= NULL) {
					Status =
					papi_func_tbl->SetLimitCheckEnable(
					vl53l0_dev,
				VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
					1);
				}

				if ((Status == VL53L0_ERROR_NONE)  &&
					(papi_func_tbl->SetLimitCheckValue
						 != NULL)) {
					vl53l0_dbgmsg(
					"Set RIT - %u\n", ritValue);
					Status =
				papi_func_tbl->SetLimitCheckValue(
				vl53l0_dev,
				VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				ritValue);
				}

			}
			break;
		case (XTALKENABLE_PAR):
			if (parameter.is_read)
				parameter.status =
				papi_func_tbl->GetXTalkCompensationEnable(
					vl53l0_dev,
					(uint8_t *) &parameter.value);
			else
				parameter.status =
				papi_func_tbl->SetXTalkCompensationEnable(
				vl53l0_dev,
				(uint8_t) parameter.value);
			break;
		case (GPIOFUNC_PAR):
			if (parameter.is_read) {
				parameter.status =
				papi_func_tbl->GetGpioConfig(
				vl53l0_dev,
				0,
				&deviceMode,
				&data->gpio_function,
				&data->gpio_polarity);
				parameter.value =
					 data->gpio_function;
			} else {
				data->gpio_function = parameter.value;
				parameter.status =
				  papi_func_tbl->SetGpioConfig(
					vl53l0_dev,
					 0,
					 0,
					data->gpio_function,
					data->gpio_polarity);
			}
			break;
		case (LOWTHRESH_PAR):
			if (parameter.is_read) {
				parameter.status =
				  papi_func_tbl->GetInterruptThresholds(
					vl53l0_dev,
					 0,
					&(data->low_threshold),
					&(data->high_threshold));
				parameter.value =
					 data->low_threshold >> 16;
			} else {
				data->low_threshold = parameter.value << 16;
				parameter.status =
				  papi_func_tbl->SetInterruptThresholds(
					vl53l0_dev,
					 0,
					data->low_threshold,
					 data->high_threshold);
			}
			break;
		case (HIGHTHRESH_PAR):
			if (parameter.is_read) {
				parameter.status =
				  papi_func_tbl->GetInterruptThresholds(
					vl53l0_dev,
					 0,
					&(data->low_threshold),
					 &(data->high_threshold));
				parameter.value =
					data->high_threshold >> 16;
			} else {
				data->high_threshold =
					 parameter.value << 16;
				parameter.status =
				  papi_func_tbl->SetInterruptThresholds(
					vl53l0_dev,
					 0,
					data->low_threshold,
					 data->high_threshold);
			}
			break;
		case (DEVICEMODE_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetDeviceMode(
					vl53l0_dev,
				 (VL53L0_DeviceModes *)&(parameter.value));
			} else {
				parameter.status =
				  papi_func_tbl->SetDeviceMode(
					vl53l0_dev,
					(VL53L0_DeviceModes)(parameter.value));
				data->deviceMode =
				 (VL53L0_DeviceModes)(parameter.value);
			}
			break;



		case (INTERMEASUREMENT_PAR):
			if (parameter.is_read) {
				parameter.status =
			   papi_func_tbl->GetInterMeasurementPeriodMilliSeconds(
					vl53l0_dev,
					(uint32_t *)&(parameter.value));
			} else {
				parameter.status =
			  papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(
					vl53l0_dev,
					(uint32_t)(parameter.value));
				data->interMeasurems = parameter.value;
			}
			break;

		}

		if (copy_to_user((struct stmvl53l0_parameter *)p, &parameter,
				sizeof(struct stmvl53l0_parameter))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int stmvl53l0_open(struct inode *inode, struct file *file)
{
	return 0;
}

/* Flush will be invoked on close(device_fd) */
static int stmvl53l0_flush(struct file *file, fl_owner_t id)
{
	struct stmvl53l0_data *data = container_of(file->private_data,
					struct stmvl53l0_data, miscdev);
	(void) file;
	(void) id;
	/* Revisit : Check if the
	 *instance are opened multiple times on some platforms
	 */
	mutex_lock(&data->work_mutex);
	if (data) {
		if (data->enable_ps_sensor == 1) {
			/* turn off tof sensor if it's enabled */
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
	}
	mutex_unlock(&data->work_mutex);

	return 0;
}

static long stmvl53l0_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret;
	struct stmvl53l0_data *data =
			container_of(file->private_data,
					struct stmvl53l0_data, miscdev);
	mutex_lock(&data->work_mutex);
	ret = stmvl53l0_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&data->work_mutex);

	return ret;
}

/*
 * Initialization function
 */
static int stmvl53l0_init_client(struct stmvl53l0_data *data)
{

	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceInfo_t DeviceInfo;
	VL53L0_DEV vl53l0_dev = data;
	uint32_t refSpadCount;
	uint8_t isApertureSpads;
	uint8_t VhvSettings;
	uint8_t PhaseCal;


	vl53l0_dbgmsg("Enter\n");

	data->I2cDevAddr      = 0x52;
	data->comms_type      = 1;
	data->comms_speed_khz = 400;



	/* Setup API functions based on revision */
	stmvl53l0_setupAPIFunctions(data);

	/* Perform Ref and RefSpad calibrations and save the values */


	if (data->reset) {
		pr_err("Call of VL53L0_DataInit\n");
		/* Data initialization */
		Status = papi_func_tbl->DataInit(vl53l0_dev);
		/* data->reset = 0; */
		if (Status != VL53L0_ERROR_NONE) {
			vl53l0_errmsg(
			 "%d- error status %d\n", __LINE__, Status);
			return Status;
		}
	}

	vl53l0_dbgmsg("VL53L0_GetDeviceInfo:\n");
	Status = papi_func_tbl->GetDeviceInfo(vl53l0_dev, &DeviceInfo);
	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Device Name : %s\n", DeviceInfo.Name);
		pr_err("Device Type : %s\n", DeviceInfo.Type);
		pr_err("Device ID : %s\n", DeviceInfo.ProductId);
		pr_err("Product type: %d\n", DeviceInfo.ProductType);
		pr_err("ProductRevisionMajor : %d\n",
			DeviceInfo.ProductRevisionMajor);
		pr_err("ProductRevisionMinor : %d\n",
				DeviceInfo.ProductRevisionMinor);
	}
		/* Device Initialization */
	vl53l0_dbgmsg("Call of VL53L0_StaticInit\n");
	Status = papi_func_tbl->StaticInit(vl53l0_dev);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg("%d- error status %d\n", __LINE__, Status);
		return Status;
	}





	if (papi_func_tbl->PerformRefCalibration != NULL && data->reset) {
		vl53l0_dbgmsg("Call of VL53L0_PerformRefCalibration\n");
		Status = papi_func_tbl->PerformRefCalibration(vl53l0_dev,
				&VhvSettings, &PhaseCal); /* Ref calibration */
		if (Status !=
			 VL53L0_ERROR_NONE) {
			vl53l0_errmsg(
			"%d- error status %d\n", __LINE__, Status);
			return Status;
		}
	}
	vl53l0_dbgmsg("VHV = %u, PhaseCal = %u\n", VhvSettings, PhaseCal);
	vl53l0_dev->VhvSettings = VhvSettings;
	vl53l0_dev->PhaseCal = PhaseCal;

	if (papi_func_tbl->PerformRefSpadManagement != NULL && data->reset) {
		vl53l0_dbgmsg(
			"Call of VL53L0_PerformRefSpadManagement\n");
		Status = papi_func_tbl->PerformRefSpadManagement(vl53l0_dev,
				&refSpadCount,
				&isApertureSpads); /* Ref Spad Management */
		if (Status != VL53L0_ERROR_NONE) {
			vl53l0_errmsg(
			"%d- error status %d\n", __LINE__, Status);
			return Status;
		}
	}

	vl53l0_dbgmsg(
		"SpadCount = %u, isAperature = %u\n",
		 refSpadCount, isApertureSpads);
	vl53l0_dev->refSpadCount = refSpadCount;
	vl53l0_dev->isApertureSpads = isApertureSpads;




	if (Status == VL53L0_ERROR_NONE && data->reset) {
		if ((papi_func_tbl->SetOffsetCalibrationDataMicroMeter
			  != NULL) &&
		(vl53l0_dev->setCalibratedValue &
			 SET_OFFSET_CALIB_DATA_MICROMETER_MASK)) {
			vl53l0_dbgmsg(
			"Call of SetOffsetCalibrationDataMicroMeter\n");
			Status =
			papi_func_tbl->SetOffsetCalibrationDataMicroMeter(
				vl53l0_dev,
				vl53l0_dev->OffsetMicroMeter);
			if (Status != VL53L0_ERROR_NONE) {
				vl53l0_errmsg(
				"%d- error status %d\n", __LINE__, Status);
				return Status;
			}
		}
	}



	if (data->reset) {
		if ((papi_func_tbl->SetXTalkCompensationRateMegaCps != NULL) &&
		(vl53l0_dev->setCalibratedValue &
			SET_XTALK_COMP_RATE_MCPS_MASK)) {
			vl53l0_dbgmsg(
			"Call of SetXTalkCompensationRateMegaCps\n");
			Status = papi_func_tbl->SetXTalkCompensationRateMegaCps(
				vl53l0_dev,
				vl53l0_dev->XTalkCompensationRateMegaCps);
			if (Status != VL53L0_ERROR_NONE) {
				vl53l0_errmsg(
					"%d- error status %d\n",
					 __LINE__, Status);
				return Status;
			}
		}
	}

	if (data->reset) {
		if (vl53l0_dev->setCalibratedValue &
			/* Xtalk calibration done*/
			 SET_XTALK_COMP_RATE_MCPS_MASK) {
			/* Range Ignore Threshold */
			FixPoint1616_t ritValue = 0;

			if (vl53l0_dev->XTalkCompensationRateMegaCps <
				7*65536/10000) {
				/*0.7 KCps converted to MCps */
				ritValue = 15 * 7 * 65536/100000;
			} else {
				ritValue = 15 *
				 vl53l0_dev->XTalkCompensationRateMegaCps/10;
			}

			if (papi_func_tbl->SetLimitCheckEnable != NULL) {
				Status = papi_func_tbl->SetLimitCheckEnable(
				vl53l0_dev,
				VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				1);
				if (Status != VL53L0_ERROR_NONE) {
					vl53l0_errmsg(
					"%d- error status %d\n", __LINE__,
					 Status);
					return Status;
				}
			}

			if (papi_func_tbl->SetLimitCheckValue != NULL) {
				vl53l0_dbgmsg("Set RIT - %u\n", ritValue);
				Status = papi_func_tbl->SetLimitCheckValue(
				vl53l0_dev,
				VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				ritValue);
				if (Status != VL53L0_ERROR_NONE) {
					vl53l0_errmsg(
					"%d- error status %d\n",
					 __LINE__, Status);
					return Status;
				}
			}
		}
		data->reset = 0;
	}

	/* Setup in single ranging mode */

		pr_err("Call of VL53L0_SetDeviceMode\n");
	Status = papi_func_tbl->SetDeviceMode(vl53l0_dev,
					VL53L0_DEVICEMODE_SINGLE_RANGING);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg("%d- error status %d\n", __LINE__, Status);
		return Status;
	}


	Status = papi_func_tbl->SetWrapAroundCheckEnable(vl53l0_dev, 1);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg("%d- error status %d\n", __LINE__, Status);
		return Status;
	}


	vl53l0_dbgmsg("End\n");

	return 0;
}


static int stmvl53l0_config_use_case(struct stmvl53l0_data *data)
{
	VL53L0_DEV		vl53l0_dev = data;
	VL53L0_Error	Status = VL53L0_ERROR_NONE;
	FixPoint1616_t	signalRateLimit;
	FixPoint1616_t	sigmaLimit;
	uint32_t		preRangePulsePeriod;
	uint32_t		finalRangePulsePeriod;

	vl53l0_dbgmsg("Enter\n");

	switch (vl53l0_dev->useCase) {

	case USE_CASE_LONG_DISTANCE:
		sigmaLimit			= LONG_DISTANCE_SIGMA_LIMIT;
		signalRateLimit		= LONG_DISTANCE_SIGNAL_RATE_LIMIT;
		preRangePulsePeriod	= LONG_DISTANCE_PRE_RANGE_PULSE_PERIOD;
		finalRangePulsePeriod = LONG_DISTANCE_FINAL_RANGE_PULSE_PERIOD;
		break;

	case USE_CASE_HIGH_ACCURACY:
		sigmaLimit		= HIGH_ACCURACY_SIGMA_LIMIT;
		signalRateLimit	= HIGH_ACCURACY_SIGNAL_RATE_LIMIT;
		preRangePulsePeriod	= HIGH_ACCURACY_PRE_RANGE_PULSE_PERIOD;
		finalRangePulsePeriod = HIGH_ACCURACY_FINAL_RANGE_PULSE_PERIOD;
		break;

	case USE_CASE_HIGH_SPEED:
		sigmaLimit		= HIGH_SPEED_SIGMA_LIMIT;
		signalRateLimit	= HIGH_SPEED_SIGNAL_RATE_LIMIT;
		preRangePulsePeriod	= HIGH_SPEED_PRE_RANGE_PULSE_PERIOD;
		finalRangePulsePeriod = HIGH_SPEED_FINAL_RANGE_PULSE_PERIOD;
		break;

	case USE_CASE_CUSTOM:
		/* Set by application through IOCTL interface */
		sigmaLimit			= vl53l0_dev->sigmaLimit;
		signalRateLimit		= vl53l0_dev->signalRateLimit;
		preRangePulsePeriod = vl53l0_dev->preRangePulsePeriod;
		finalRangePulsePeriod = vl53l0_dev->finalRangePulsePeriod;
		break;

	default:
		vl53l0_errmsg(
			"Invalid use case = %d\n", vl53l0_dev->useCase);
		/* Invalid parameter, should not reach here */
		return -EINVAL;
	}

	vl53l0_dbgmsg(
		"Configure UseCase(%d) : Sigma=%u,Signal=%u,Pre=%u,Final=%u,timingBudget=%u\n",
					vl53l0_dev->useCase,
					sigmaLimit,
					signalRateLimit,
					preRangePulsePeriod,
					finalRangePulsePeriod,
					vl53l0_dev->timingBudget);

	if (papi_func_tbl->SetLimitCheckEnable != NULL) {
		Status = papi_func_tbl->SetLimitCheckEnable(
			vl53l0_dev,
			VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
			 1);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = papi_func_tbl->SetLimitCheckEnable(
			vl53l0_dev,
			VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			 1);
	} else {
		vl53l0_errmsg(
		"SetLimitCheckEnable(SIGMA_FINAL_RANGE) failed with errcode = %d\n",
		 Status);
	}


	if (Status == VL53L0_ERROR_NONE) {
		Status = papi_func_tbl->SetLimitCheckValue(
				vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				signalRateLimit);
	} else {
		vl53l0_errmsg(
			"SetLimitCheckEnable(SIGNAL_RATE_FINAL_RANGE) failed with errcode = %d\n",
		 Status);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0_dev,
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
					sigmaLimit);
	} else {
		vl53l0_dbgmsg(
			"SIGNAL_RATE_FINAL_RANGE failed with errcode = %d\n",
			 Status);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status =
		papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds(
				vl53l0_dev,
				vl53l0_dev->timingBudget);
	} else {
		vl53l0_dbgmsg(
			"SIGMA_FINAL_RANGE failed with errcode = %d\n",
			 Status);
	}


	if (Status == VL53L0_ERROR_NONE) {
		Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0_dev,
					VL53L0_VCSEL_PERIOD_PRE_RANGE,
					preRangePulsePeriod);
	} else {
		vl53l0_dbgmsg(
		 "SetMeasurementTimingBudget failed with errcode = %d\n",
			 Status);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0_dev,
			VL53L0_VCSEL_PERIOD_FINAL_RANGE,
			finalRangePulsePeriod);
	} else
		vl53l0_dbgmsg(
		"SetVcselPulsePeriod(PRE) failed with errcode = %d\n", Status);


	if (Status != VL53L0_ERROR_NONE)
		vl53l0_dbgmsg(
		"SetVcselPulsePeriod(FINAL)failed with errcode = %d\n", Status);

	vl53l0_dbgmsg("End\n");
	return Status;
}
static int stmvl53l0_start(struct stmvl53l0_data *data, uint8_t scaling,
	init_mode_e mode)
{
	int rc = 0;
	VL53L0_DEV vl53l0_dev = data;
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	vl53l0_dbgmsg("Enter\n");

	/* Power up */
	rc = pmodule_func_tbl->power_up(data->client_object, &data->reset);
	if (rc) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}

	/* init */
	rc = stmvl53l0_init_client(data);
	if (rc) {
		vl53l0_errmsg("%d, error rc %d\n", __LINE__, rc);
		pmodule_func_tbl->power_down(data->client_object);
		return -EINVAL;
	}

	/* check mode */
	if (mode != NORMAL_MODE)
		papi_func_tbl->SetXTalkCompensationEnable(vl53l0_dev, 1);

	if (mode == OFFSETCALIB_MODE) {
		/*VL53L0_SetOffsetCalibrationDataMicroMeter(vl53l0_dev, 0);*/
		FixPoint1616_t OffsetMicroMeter;

		papi_func_tbl->PerformOffsetCalibration(vl53l0_dev,
			(data->offsetCalDistance<<16),
			&OffsetMicroMeter);
		pr_err("Offset calibration:%u\n", OffsetMicroMeter);
		vl53l0_dev->OffsetMicroMeter = OffsetMicroMeter;
		vl53l0_dev->setCalibratedValue |=
		 SET_OFFSET_CALIB_DATA_MICROMETER_MASK;

		return rc;
	} else if (mode == XTALKCALIB_MODE) {
		FixPoint1616_t XTalkCompensationRateMegaCps;
		/*caltarget distance : 100mm and convert to
		* fixed point 16 16 format
		*/
		papi_func_tbl->PerformXTalkCalibration(vl53l0_dev,
			(data->xtalkCalDistance<<16),
			&XTalkCompensationRateMegaCps);
		pr_err("Xtalk calibration:%u\n",
			 XTalkCompensationRateMegaCps);
		vl53l0_dev->XTalkCompensationRateMegaCps =
				 XTalkCompensationRateMegaCps;
		vl53l0_dev->setCalibratedValue |=
			 SET_XTALK_COMP_RATE_MCPS_MASK;

		return rc;
	}
	/* set up device parameters */
	data->gpio_polarity = VL53L0_INTERRUPTPOLARITY_LOW;

	/* Following two calls are made from IOCTL as well */
	Status = papi_func_tbl->SetGpioConfig(vl53l0_dev, 0, 0,
		data->gpio_function,
		VL53L0_INTERRUPTPOLARITY_LOW);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg("Failed to SetGpioConfig. Error = %d\n", Status);
		return -EPERM;
	}


	Status = papi_func_tbl->SetInterruptThresholds(vl53l0_dev, 0,
		data->low_threshold,
		 data->high_threshold);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg(
		"Failed to SetInterruptThresholds. Error = %d\n", Status);
		return -EPERM;
	}


	if (data->deviceMode == VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING) {
		Status = papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(
			vl53l0_dev,
			data->interMeasurems);
		if (Status != VL53L0_ERROR_NONE) {
			vl53l0_errmsg(
			"Failed to SetInterMeasurementPeriodMilliSeconds. Error = %d\n",
			 Status);
			return -EPERM;
		}
		pr_err(
			"DeviceMode:0x%x, interMeasurems:%d==\n",
			data->deviceMode,
			data->interMeasurems);


	}


	Status = papi_func_tbl->SetDeviceMode(
			vl53l0_dev,
			data->deviceMode);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg("Failed to SetDeviceMode. Error = %d\n", Status);
		return -EPERM;
	}

	Status = papi_func_tbl->ClearInterruptMask(vl53l0_dev,
							0);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg(
		"Failed to ClearInterruptMask. Error = %d\n", Status);
		return -EPERM;
	}



	Status = stmvl53l0_config_use_case(vl53l0_dev);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg(
			"Failed to configure Use case = %u\n",
				vl53l0_dev->useCase);
		return -EPERM;
	}

	/* start the ranging */
	Status = papi_func_tbl->StartMeasurement(vl53l0_dev);
	if (Status != VL53L0_ERROR_NONE) {
		vl53l0_errmsg(
			"Failed to StartMeasurement. Error = %d\n", Status);
		return -EPERM;
	}

	data->enable_ps_sensor = 1;

#ifndef USE_INT
	/* Unblock the thread execution */
	wake_up(&vl53l0_dev->poll_thread_wq);
#endif

	vl53l0_dbgmsg("End\n");

	return rc;
}

static int stmvl53l0_stop(struct stmvl53l0_data *data)
{
	int rc = 0;
	VL53L0_DEV vl53l0_dev = data;

	vl53l0_dbgmsg("Enter\n");

	/* stop - if continuous mode */
	if (data->deviceMode == VL53L0_DEVICEMODE_CONTINUOUS_RANGING ||
		data->deviceMode == VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING)
		papi_func_tbl->StopMeasurement(vl53l0_dev);

	/* clean interrupt */
	papi_func_tbl->ClearInterruptMask(vl53l0_dev, 0);

	/* cancel work handler */
	stmvl53l0_cancel_handler(data);

	/* Clear updateUseCase pending operation */
	data->updateUseCase = 0;
	/* power down */
	rc = pmodule_func_tbl->power_down(data->client_object);
	if (rc) {
		vl53l0_errmsg("%d, error rc %d\n", __LINE__, rc);
		return rc;
	}
	vl53l0_dbgmsg("End\n");

	return rc;
}
static void stmvl53l0_timer_fn(unsigned long data)
{

	VL53L0_DEV vl53l0_dev = (VL53L0_DEV)data;

	vl53l0_dev->flushCount++;

	input_report_abs(vl53l0_dev->input_dev_ps, ABS_GAS,
		  vl53l0_dev->flushCount);


	input_sync(vl53l0_dev->input_dev_ps);

	vl53l0_dbgmsg("Sensor HAL Flush Count = %u\n", vl53l0_dev->flushCount);
}



/*
 * I2C init/probing/exit functions
 */
static const struct file_operations stmvl53l0_ranging_fops = {
	.owner =			THIS_MODULE,
	.unlocked_ioctl =	stmvl53l0_ioctl,
	.open =				stmvl53l0_open,
	.flush =			stmvl53l0_flush,
};



int stmvl53l0_setup(struct stmvl53l0_data *data)
{
	int rc = 0;

#ifdef USE_INT
	int irq = 0;
#endif

	vl53l0_dbgmsg("Enter\n");

	/* init mutex */
	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);

#ifdef USE_INT
	/* init interrupt */
	gpio_request(data->irq_gpio, "vl53l0_gpio_int");
	gpio_direction_input(data->irq_gpio);
	irq = gpio_to_irq(data->irq_gpio);
	if (irq < 0) {
		vl53l0_errmsg("filed to map GPIO: %d to interrupt:%d\n",
			data->irq_gpio, irq);
	} else {
		vl53l0_dbgmsg("register_irq:%d\n", irq);
		/* IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG -
		 * poliarty:1
		 */
		rc = request_threaded_irq(irq, NULL,
				stmvl53l0_interrupt_handler,
				IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
				"vl53l0_interrupt",
				(void *)data);
		if (rc) {
			vl53l0_errmsg(
"%d, Could not allocate STMVL53L0_INT ! result:%d\n",  __LINE__, rc);
			free_irq(irq, data);
			goto exit_free_irq;
		}
	}
	data->irq = irq;
	vl53l0_errmsg("interrupt is hooked\n");
#else

	init_waitqueue_head(&data->poll_thread_wq);

	data->poll_thread = kthread_run(&stmvl53l0_poll_thread,
							(void *)data,
							"STM-VL53L0");
	if (data->poll_thread == NULL) {
		pr_err(
	"%s(%d) - Failed to create Polling thread\n", __func__, __LINE__);
		goto exit_free_irq;
	}
#endif

	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl53l0_work_handler);

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_free_irq;
	}
	set_bit(EV_ABS, data->input_dev_ps->evbit);
	/* range in cm*/
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0);
	/* tv_sec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0X, 0, 0xffffffff,
		0, 0);
	/* tv_usec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0Y, 0, 0xffffffff,
		0, 0);
	/* range in_mm */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1X, 0, 765, 0, 0);
	/* error code change maximum to 0xff for more flexibility */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1Y, 0, 0xff, 0, 0);
	/* rtnRate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2X, 0, 0xffffffff,
		0, 0);
	/* rtn_amb_rate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2Y, 0, 0xffffffff,
		0, 0);
	/* rtn_conv_time */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3X, 0, 0xffffffff,
		0, 0);
	/* dmax */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3Y, 0, 0xffffffff,
		0, 0);

	input_set_abs_params(data->input_dev_ps, ABS_PRESSURE, 0, 0xffffffff,
		0, 0);

	input_set_abs_params(data->input_dev_ps, ABS_WHEEL, 0, 0xffffffff,
		0, 0);

	input_set_abs_params(data->input_dev_ps, ABS_GAS, 0, 0xffffffff,
		0, 0);
	data->input_dev_ps->name = "STM VL53L0 proximity sensor";

	rc = input_register_device(data->input_dev_ps);
	if (rc) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_free_dev_ps;
	}
	/* setup drv data */
	input_set_drvdata(data->input_dev_ps, data);

	/* Register sysfs hooks */
	data->range_kobj = kobject_create_and_add("range", kernel_kobj);
	if (!data->range_kobj) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps;
	}
	rc = sysfs_create_group(&data->input_dev_ps->dev.kobj,
			&stmvl53l0_attr_group);
	if (rc) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps_1;
	}

	setup_timer(&data->timer,
				 stmvl53l0_timer_fn,
				(unsigned long)data);

	/* to register as a misc device */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "stmvl53l0_ranging";
	data->miscdev.fops = &stmvl53l0_ranging_fops;
	vl53l0_errmsg("Misc device registration name:%s\n", data->dev_name);
	if (misc_register(&data->miscdev) != 0)
		vl53l0_errmsg(
"Could not register misc. dev for stmvl53l0	ranging\n");

	/* init default device parameter value */
	data->enable_ps_sensor = 0;
	data->reset = 1;
	data->delay_ms = 30;	/* delay time to 30ms */
	data->enableDebug = 0;
	data->gpio_polarity = VL53L0_INTERRUPTPOLARITY_LOW;
	data->gpio_function = VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
	data->low_threshold = 60;
	data->high_threshold = 200;
	data->deviceMode = VL53L0_DEVICEMODE_SINGLE_RANGING;
	data->interMeasurems = 30;
	data->useCase = USE_CASE_LONG_DISTANCE;
	data->timingBudget = LONG_DISTANCE_TIMING_BUDGET;

	/* Set default values used in Custom Mode Use Case */
	data->signalRateLimit = LONG_DISTANCE_SIGNAL_RATE_LIMIT;
	data->sigmaLimit = LONG_DISTANCE_SIGMA_LIMIT;
	data->preRangePulsePeriod = LONG_DISTANCE_PRE_RANGE_PULSE_PERIOD;
	data->finalRangePulsePeriod = LONG_DISTANCE_FINAL_RANGE_PULSE_PERIOD;




	vl53l0_dbgmsg("support ver. %s enabled\n", DRIVER_VERSION);
	vl53l0_dbgmsg("End");

	return 0;
exit_unregister_dev_ps_1:
	kobject_put(data->range_kobj);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_free_irq:
#ifdef USE_INT
	free_irq(irq, data);
#endif
	kfree(data);
	return rc;
}

void stmvl53l0_cleanup(struct stmvl53l0_data *data)
{
#ifndef USE_INT
	pr_err("%s(%d) : Stop poll_thread\n", __func__, __LINE__);
	poll_thread_exit = 1;
	kthread_stop(data->poll_thread);
#endif
}
static int __init stmvl53l0_init(void)
{
	int ret = -1;

	vl53l0_dbgmsg("Enter\n");

	/* assign function table */
	pmodule_func_tbl = &stmvl53l0_module_func_tbl;
	papi_func_tbl = &stmvl53l0_api_func_tbl;

	/* client specific init function */
	ret = pmodule_func_tbl->init();

	if (ret)
		vl53l0_errmsg("%d failed with %d\n", __LINE__, ret);

	vl53l0_dbgmsg("End\n");

	return ret;
}

static void __exit stmvl53l0_exit(void)
{
	vl53l0_dbgmsg("Enter\n");

	vl53l0_dbgmsg("End\n");
}


MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl53l0_init);
module_exit(stmvl53l0_exit);

