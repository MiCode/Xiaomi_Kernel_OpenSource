/*
 *  stmvl53l0x_module.c - Linux kernel modules for STM VL53L0 FlightSense TOF
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
#include "vl53l0x_api.h"

#define IRQ_NUM	   59
#ifdef DEBUG_TIME_LOG
struct timeval start_tv, stop_tv;
#endif

/*
 * Global data
 */

#ifdef CAMERA_CCI
static struct stmvl53l0x_module_fn_t stmvl53l0x_module_func_tbl = {
	.init = stmvl53l0x_init_cci,
	.deinit = stmvl53l0x_exit_cci,
	.power_up = stmvl53l0x_power_up_cci,
	.power_down = stmvl53l0x_power_down_cci,
};
#else
static struct stmvl53l0x_module_fn_t stmvl53l0x_module_func_tbl = {
	.init = stmvl53l0x_init_i2c,
	.deinit = stmvl53l0x_exit_i2c,
	.power_up = stmvl53l0x_power_up_i2c,
	.power_down = stmvl53l0x_power_down_i2c,
};
#endif
struct stmvl53l0x_module_fn_t *pmodule_func_tbl;

struct stmvl53l0x_api_fn_t {
	int8_t (*GetVersion)(struct VL_Version_t *pVersion);
	int8_t (*GetPalSpecVersion)(struct VL_Version_t *pPalSpecVersion);

	int8_t (*GetProductRevision)(struct vl_data *Dev,
					uint8_t *pProductRevisionMajor,
					uint8_t *pProductRevisionMinor);
	int8_t (*GetDeviceInfo)(struct vl_data *Dev,
				struct VL_DeviceInfo_t *pVL_DeviceInfo);
	int8_t (*GetDeviceErrorStatus)(struct vl_data *Dev,
				uint8_t *pDeviceErrorStatus);
	int8_t (*GetRangeStatusString)(uint8_t RangeStatus,
				char *pRangeStatusString);
	int8_t (*GetDeviceErrorString)(uint8_t ErrorCode,
				char *pDeviceErrorString);
	int8_t (*GetPalErrorString)(int8_t PalErrorCode,
				char *pPalErrorString);
	int8_t (*GetPalStateString)(uint8_t PalStateCode,
				char *pPalStateString);
	int8_t (*GetPalState)(struct vl_data *Dev, uint8_t *pPalState);
	int8_t (*SetPowerMode)(struct vl_data *Dev,
				uint8_t PowerMode);
	int8_t (*GetPowerMode)(struct vl_data *Dev,
				uint8_t *pPowerMode);
	int8_t (*SetOffsetCalibrationDataMicroMeter)(struct vl_data *Dev,
				int32_t OffsetCalibrationDataMicroMeter);
	int8_t (*GetOffsetCalibrationDataMicroMeter)(struct vl_data *Dev,
				int32_t *pOffsetCalibrationDataMicroMeter);
	int8_t (*SetLinearityCorrectiveGain)(struct vl_data *Dev,
				int16_t LinearityCorrectiveGain);
	int8_t (*GetLinearityCorrectiveGain)(struct vl_data *Dev,
				uint16_t *pLinearityCorrectiveGain);
	int8_t (*SetGroupParamHold)(struct vl_data *Dev,
				uint8_t GroupParamHold);
	int8_t (*GetUpperLimitMilliMeter)(struct vl_data *Dev,
				uint16_t *pUpperLimitMilliMeter);
	int8_t (*SetDeviceAddress)(struct vl_data *Dev,
				uint8_t DeviceAddress);
	int8_t (*DataInit)(struct vl_data *Dev);
	int8_t (*SetTuningSettingBuffer)(struct vl_data *Dev,
				uint8_t *pTuningSettingBuffer,
				uint8_t UseInternalTuningSettings);
	int8_t (*GetTuningSettingBuffer)(struct vl_data *Dev,
				uint8_t **pTuningSettingBuffer,
				uint8_t *pUseInternalTuningSettings);
	int8_t (*StaticInit)(struct vl_data *Dev);
	int8_t (*WaitDeviceBooted)(struct vl_data *Dev);
	int8_t (*ResetDevice)(struct vl_data *Dev);
	int8_t (*SetDeviceParameters)(struct vl_data *Dev,
			const struct VL_DeviceParameters_t *pDeviceParameters);
	int8_t (*GetDeviceParameters)(struct vl_data *Dev,
			struct VL_DeviceParameters_t *pDeviceParameters);
	int8_t (*SetDeviceMode)(struct vl_data *Dev,
			uint8_t DeviceMode);
	int8_t (*GetDeviceMode)(struct vl_data *Dev,
			uint8_t *pDeviceMode);
	int8_t (*SetHistogramMode)(struct vl_data *Dev,
			uint8_t HistogramMode);
	int8_t (*GetHistogramMode)(struct vl_data *Dev,
			uint8_t *pHistogramMode);
	int8_t (*SetMeasurementTimingBudgetMicroSeconds)(struct vl_data *Dev,
			uint32_t  MeasurementTimingBudgetMicroSeconds);
	int8_t (*GetMeasurementTimingBudgetMicroSeconds)(
			struct vl_data *Dev,
			uint32_t *pMeasurementTimingBudgetMicroSeconds);
	int8_t (*GetVcselPulsePeriod)(struct vl_data *Dev,
			uint8_t VcselPeriodType,
			uint8_t	*pVCSELPulsePeriod);
	int8_t (*SetVcselPulsePeriod)(struct vl_data *Dev,
			uint8_t VcselPeriodType,
			uint8_t VCSELPulsePeriod);
	int8_t (*SetSequenceStepEnable)(struct vl_data *Dev,
			uint8_t SequenceStepId,
			uint8_t SequenceStepEnabled);
	int8_t (*GetSequenceStepEnable)(struct vl_data *Dev,
			uint8_t SequenceStepId,
			uint8_t *pSequenceStepEnabled);
	int8_t (*GetSequenceStepEnables)(struct vl_data *Dev,
		struct VL_SchedulerSequenceSteps_t *pSchedulerSequenceSteps);
	int8_t (*SetSequenceStepTimeout)(struct vl_data *Dev,
			uint8_t SequenceStepId,
			unsigned int TimeOutMilliSecs);
	int8_t (*GetSequenceStepTimeout)(struct vl_data *Dev,
			uint8_t SequenceStepId,
			unsigned int *pTimeOutMilliSecs);
	int8_t (*GetNumberOfSequenceSteps)(struct vl_data *Dev,
			uint8_t *pNumberOfSequenceSteps);
	int8_t (*GetSequenceStepsInfo)(
			uint8_t SequenceStepId,
			char *pSequenceStepsString);
	int8_t (*SetInterMeasurementPeriodMilliSeconds)(
			struct vl_data *Dev,
			uint32_t InterMeasurementPeriodMilliSeconds);
	int8_t (*GetInterMeasurementPeriodMilliSeconds)(
			struct vl_data *Dev,
			uint32_t *pInterMeasurementPeriodMilliSeconds);
	int8_t (*SetXTalkCompensationEnable)(struct vl_data *Dev,
			uint8_t XTalkCompensationEnable);
	int8_t (*GetXTalkCompensationEnable)(struct vl_data *Dev,
			uint8_t *pXTalkCompensationEnable);
	int8_t (*SetXTalkCompensationRateMegaCps)(
			struct vl_data *Dev,
			unsigned int XTalkCompensationRateMegaCps);
	int8_t (*GetXTalkCompensationRateMegaCps)(
			struct vl_data *Dev,
			unsigned int *pXTalkCompensationRateMegaCps);
	int8_t (*GetNumberOfLimitCheck)(
			uint16_t *pNumberOfLimitCheck);
	int8_t (*GetLimitCheckInfo)(struct vl_data *Dev,
			uint16_t LimitCheckId, char *pLimitCheckString);
	int8_t (*SetLimitCheckEnable)(struct vl_data *Dev,
			uint16_t LimitCheckId,
			uint8_t LimitCheckEnable);
	int8_t (*GetLimitCheckEnable)(struct vl_data *Dev,
			uint16_t LimitCheckId, uint8_t *pLimitCheckEnable);
	int8_t (*SetLimitCheckValue)(struct vl_data *Dev,
			uint16_t LimitCheckId,
			unsigned int LimitCheckValue);
	int8_t (*GetLimitCheckValue)(struct vl_data *Dev,
			uint16_t LimitCheckId,
			unsigned int *pLimitCheckValue);
	int8_t (*GetLimitCheckCurrent)(struct vl_data *Dev,
		uint16_t LimitCheckId, unsigned int *pLimitCheckCurrent);
	int8_t (*SetWrapAroundCheckEnable)(struct vl_data *Dev,
			uint8_t WrapAroundCheckEnable);
	int8_t (*GetWrapAroundCheckEnable)(struct vl_data *Dev,
			uint8_t *pWrapAroundCheckEnable);
	int8_t (*PerformSingleMeasurement)(struct vl_data *Dev);
	int8_t (*PerformRefCalibration)(struct vl_data *Dev,
			uint8_t *pVhvSettings, uint8_t *pPhaseCal);
	int8_t (*SetRefCalibration)(struct vl_data *Dev,
			uint8_t VhvSettings, uint8_t PhaseCal);
	int8_t (*GetRefCalibration)(struct vl_data *Dev,
			uint8_t *pVhvSettings, uint8_t *pPhaseCal);
	int8_t (*PerformXTalkCalibration)(struct vl_data *Dev,
			unsigned int XTalkCalDistance,
			unsigned int *pXTalkCompensationRateMegaCps);
	int8_t (*PerformOffsetCalibration)(struct vl_data *Dev,
			unsigned int CalDistanceMilliMeter,
			int32_t *pOffsetMicroMeter);
	int8_t (*StartMeasurement)(struct vl_data *Dev);
	int8_t (*StopMeasurement)(struct vl_data *Dev);
	int8_t (*GetMeasurementDataReady)(struct vl_data *Dev,
			uint8_t *pMeasurementDataReady);
	int8_t (*WaitDeviceReadyForNewMeasurement)(struct vl_data *Dev,
			uint32_t MaxLoop);
	int8_t (*GetRangingMeasurementData)(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData);
	int8_t (*GetHistogramMeasurementData)(struct vl_data *Dev,
	struct VL_HistogramMeasurementData_t *pHistogramMeasurementData);
	int8_t (*PerformSingleRangingMeasurement)(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData);
	int8_t (*PerformSingleHistogramMeasurement)(struct vl_data *Dev,
	struct VL_HistogramMeasurementData_t *pHistogramMeasurementData);
	int8_t (*SetNumberOfROIZones)(struct vl_data *Dev,
			uint8_t NumberOfROIZones);
	int8_t (*GetNumberOfROIZones)(struct vl_data *Dev,
			uint8_t *pNumberOfROIZones);
	int8_t (*GetMaxNumberOfROIZones)(struct vl_data *Dev,
			uint8_t *pMaxNumberOfROIZones);
	int8_t (*SetGpioConfig)(struct vl_data *Dev,
			uint8_t Pin,
			uint8_t DeviceMode,
			uint8_t Functionality,
			uint8_t Polarity);
	int8_t (*GetGpioConfig)(struct vl_data *Dev,
			uint8_t Pin,
			uint8_t *pDeviceMode,
			uint8_t *pFunctionality,
			uint8_t *pPolarity);
	int8_t (*SetInterruptThresholds)(struct vl_data *Dev,
			uint8_t DeviceMode,
			unsigned int ThresholdLow,
			unsigned int ThresholdHigh);
	int8_t (*GetInterruptThresholds)(struct vl_data *Dev,
			uint8_t DeviceMode,
			unsigned int *pThresholdLow,
			unsigned int *pThresholdHigh);
	int8_t (*ClearInterruptMask)(struct vl_data *Dev,
			uint32_t InterruptMask);
	int8_t (*GetInterruptMaskStatus)(struct vl_data *Dev,
			uint32_t *pInterruptMaskStatus);
	int8_t (*EnableInterruptMask)(struct vl_data *Dev,
		uint32_t InterruptMask);
	int8_t (*SetSpadAmbientDamperThreshold)(struct vl_data *Dev,
			uint16_t SpadAmbientDamperThreshold);
	int8_t (*GetSpadAmbientDamperThreshold)(struct vl_data *Dev,
			uint16_t *pSpadAmbientDamperThreshold);
	int8_t (*SetSpadAmbientDamperFactor)(struct vl_data *Dev,
			uint16_t SpadAmbientDamperFactor);
	int8_t (*GetSpadAmbientDamperFactor)(struct vl_data *Dev,
			uint16_t *pSpadAmbientDamperFactor);
	int8_t (*PerformRefSpadManagement)(struct vl_data *Dev,
			uint32_t *refSpadCount, uint8_t *isApertureSpads);
	int8_t (*SetReferenceSpads)(struct vl_data *Dev,
			uint32_t count, uint8_t isApertureSpads);
	int8_t (*GetReferenceSpads)(struct vl_data *Dev,
			uint32_t *pSpadCount, uint8_t *pIsApertureSpads);
};

static struct stmvl53l0x_api_fn_t stmvl53l0x_api_func_tbl = {
	.GetVersion = VL_GetVersion,
	.GetPalSpecVersion = VL_GetPalSpecVersion,
	.GetProductRevision = VL_GetProductRevision,
	.GetDeviceInfo = VL_GetDeviceInfo,
	.GetDeviceErrorStatus = VL_GetDeviceErrorStatus,
	.GetRangeStatusString = VL_GetRangeStatusString,
	.GetDeviceErrorString = VL_GetDeviceErrorString,
	.GetPalErrorString = VL_GetPalErrorString,
	.GetPalState = VL_GetPalState,
	.SetPowerMode = VL_SetPowerMode,
	.GetPowerMode = VL_GetPowerMode,
	.SetOffsetCalibrationDataMicroMeter =
		VL_SetOffsetCalibrationDataMicroMeter,
	.SetLinearityCorrectiveGain =
		VL_SetLinearityCorrectiveGain,
	.GetLinearityCorrectiveGain =
		VL_GetLinearityCorrectiveGain,
	.GetOffsetCalibrationDataMicroMeter =
		VL_GetOffsetCalibrationDataMicroMeter,
	.SetGroupParamHold = VL_SetGroupParamHold,
	.GetUpperLimitMilliMeter = VL_GetUpperLimitMilliMeter,
	.SetDeviceAddress = VL_SetDeviceAddress,
	.DataInit = VL_DataInit,
	.SetTuningSettingBuffer = VL_SetTuningSettingBuffer,
	.GetTuningSettingBuffer = VL_GetTuningSettingBuffer,
	.StaticInit = VL_StaticInit,
	.WaitDeviceBooted = VL_WaitDeviceBooted,
	.ResetDevice = VL_ResetDevice,
	.SetDeviceParameters = VL_SetDeviceParameters,
	.SetDeviceMode = VL_SetDeviceMode,
	.GetDeviceMode = VL_GetDeviceMode,
	.SetHistogramMode = VL_SetHistogramMode,
	.GetHistogramMode = VL_GetHistogramMode,
	.SetMeasurementTimingBudgetMicroSeconds =
		VL_SetMeasurementTimingBudgetMicroSeconds,
	.GetMeasurementTimingBudgetMicroSeconds =
		VL_GetMeasurementTimingBudgetMicroSeconds,
	.GetVcselPulsePeriod = VL_GetVcselPulsePeriod,
	.SetVcselPulsePeriod = VL_SetVcselPulsePeriod,
	.SetSequenceStepEnable = VL_SetSequenceStepEnable,
	.GetSequenceStepEnable = VL_GetSequenceStepEnable,
	.GetSequenceStepEnables = VL_GetSequenceStepEnables,
	.SetSequenceStepTimeout = VL_SetSequenceStepTimeout,
	.GetSequenceStepTimeout = VL_GetSequenceStepTimeout,
	.GetNumberOfSequenceSteps = VL_GetNumberOfSequenceSteps,
	.GetSequenceStepsInfo = VL_GetSequenceStepsInfo,
	.SetInterMeasurementPeriodMilliSeconds =
		VL_SetInterMeasurementPeriodMilliSeconds,
	.GetInterMeasurementPeriodMilliSeconds =
		VL_GetInterMeasurementPeriodMilliSeconds,
	.SetXTalkCompensationEnable = VL_SetXTalkCompensationEnable,
	.GetXTalkCompensationEnable = VL_GetXTalkCompensationEnable,
	.SetXTalkCompensationRateMegaCps =
		VL_SetXTalkCompensationRateMegaCps,
	.GetXTalkCompensationRateMegaCps =
		VL_GetXTalkCompensationRateMegaCps,
	.GetNumberOfLimitCheck = VL_GetNumberOfLimitCheck,
	.GetLimitCheckInfo = VL_GetLimitCheckInfo,
	.SetLimitCheckEnable = VL_SetLimitCheckEnable,
	.GetLimitCheckEnable = VL_GetLimitCheckEnable,
	.SetLimitCheckValue = VL_SetLimitCheckValue,
	.GetLimitCheckValue = VL_GetLimitCheckValue,
	.GetLimitCheckCurrent = VL_GetLimitCheckCurrent,
	.SetWrapAroundCheckEnable = VL_SetWrapAroundCheckEnable,
	.GetWrapAroundCheckEnable = VL_GetWrapAroundCheckEnable,
	.PerformSingleMeasurement = VL_PerformSingleMeasurement,
	.PerformRefCalibration = VL_PerformRefCalibration,
	.SetRefCalibration = VL_SetRefCalibration,
	.GetRefCalibration = VL_GetRefCalibration,
	.PerformXTalkCalibration = VL_PerformXTalkCalibration,
	.PerformOffsetCalibration = VL_PerformOffsetCalibration,
	.StartMeasurement = VL_StartMeasurement,
	.StopMeasurement = VL_StopMeasurement,
	.GetMeasurementDataReady = VL_GetMeasurementDataReady,
	.WaitDeviceReadyForNewMeasurement =
		VL_WaitDeviceReadyForNewMeasurement,
	.GetRangingMeasurementData = VL_GetRangingMeasurementData,
	.GetHistogramMeasurementData = VL_GetHistogramMeasurementData,
	.PerformSingleRangingMeasurement =
		VL_PerformSingleRangingMeasurement,
	.PerformSingleHistogramMeasurement =
		VL_PerformSingleHistogramMeasurement,
	.SetNumberOfROIZones = VL_SetNumberOfROIZones,
	.GetNumberOfROIZones = VL_GetNumberOfROIZones,
	.GetMaxNumberOfROIZones = VL_GetMaxNumberOfROIZones,
	.SetGpioConfig = VL_SetGpioConfig,
	.GetGpioConfig = VL_GetGpioConfig,
	.SetInterruptThresholds = VL_SetInterruptThresholds,
	.GetInterruptThresholds = VL_GetInterruptThresholds,
	.ClearInterruptMask = VL_ClearInterruptMask,
	.GetInterruptMaskStatus = VL_GetInterruptMaskStatus,
	.EnableInterruptMask = VL_EnableInterruptMask,
	.SetSpadAmbientDamperThreshold = VL_SetSpadAmbientDamperThreshold,
	.GetSpadAmbientDamperThreshold = VL_GetSpadAmbientDamperThreshold,
	.SetSpadAmbientDamperFactor = VL_SetSpadAmbientDamperFactor,
	.GetSpadAmbientDamperFactor = VL_GetSpadAmbientDamperFactor,
	.PerformRefSpadManagement = VL_PerformRefSpadManagement,
	.SetReferenceSpads = VL_SetReferenceSpads,
	.GetReferenceSpads = VL_GetReferenceSpads,

};
struct stmvl53l0x_api_fn_t *papi_func_tbl;

/*
 * IOCTL definitions
 */
#define VL_IOCTL_INIT			_IO('p', 0x01)
#define VL_IOCTL_XTALKCALB		_IOW('p', 0x02, unsigned int)
#define VL_IOCTL_OFFCALB		_IOW('p', 0x03, unsigned int)
#define VL_IOCTL_STOP			_IO('p', 0x05)
#define VL_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL_IOCTL_GETDATAS \
			_IOR('p', 0x0b, struct VL_RangingMeasurementData_t)
#define VL_IOCTL_REGISTER \
			_IOWR('p', 0x0c, struct stmvl53l0x_register)
#define VL_IOCTL_PARAMETER \
			_IOWR('p', 0x0d, struct stmvl53l0x_parameter)

static long stmvl53l0x_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);
/*static int stmvl53l0x_flush(struct file *file, fl_owner_t id);*/
static int stmvl53l0x_open(struct inode *inode, struct file *file);
static int stmvl53l0x_init_client(struct vl_data *data);
static int stmvl53l0x_start(struct vl_data *data, uint8_t scaling,
			enum init_mode_e mode);
static int stmvl53l0x_stop(struct vl_data *data);

#ifdef DEBUG_TIME_LOG
static void stmvl53l0x_DebugTimeGet(struct timeval *ptv)
{
	do_gettimeofday(ptv);
}

static void stmvl53l0x_DebugTimeDuration(struct timeval *pstart_tv,
			struct timeval *pstop_tv)
{
	long total_sec, total_msec;

	total_sec = pstop_tv->tv_sec - pstart_tv->tv_sec;
	total_msec = (pstop_tv->tv_usec - pstart_tv->tv_usec)/1000;
	total_msec += total_sec * 1000;
	dbg("elapsedTime:%ld\n", total_msec);
}
#endif

static void stmvl53l0x_setupAPIFunctions(void)
{

	/*cut 1.1*/
	err("to setup API cut 1.1\n");
	papi_func_tbl->GetVersion = VL_GetVersion;
	papi_func_tbl->GetPalSpecVersion = VL_GetPalSpecVersion;
	papi_func_tbl->GetProductRevision = VL_GetProductRevision;
	papi_func_tbl->GetDeviceInfo = VL_GetDeviceInfo;
	papi_func_tbl->GetDeviceErrorStatus = VL_GetDeviceErrorStatus;
	papi_func_tbl->GetRangeStatusString = VL_GetRangeStatusString;
	papi_func_tbl->GetDeviceErrorString = VL_GetDeviceErrorString;
	papi_func_tbl->GetPalErrorString = VL_GetPalErrorString;
	papi_func_tbl->GetPalState = VL_GetPalState;
	papi_func_tbl->SetPowerMode = VL_SetPowerMode;
	papi_func_tbl->GetPowerMode = VL_GetPowerMode;
	papi_func_tbl->SetOffsetCalibrationDataMicroMeter =
		VL_SetOffsetCalibrationDataMicroMeter;
	papi_func_tbl->GetOffsetCalibrationDataMicroMeter =
		VL_GetOffsetCalibrationDataMicroMeter;
	papi_func_tbl->SetLinearityCorrectiveGain =
		VL_SetLinearityCorrectiveGain;
	papi_func_tbl->GetLinearityCorrectiveGain =
		VL_GetLinearityCorrectiveGain;
	papi_func_tbl->SetGroupParamHold = VL_SetGroupParamHold;
	papi_func_tbl->GetUpperLimitMilliMeter =
		VL_GetUpperLimitMilliMeter;
	papi_func_tbl->SetDeviceAddress = VL_SetDeviceAddress;
	papi_func_tbl->DataInit = VL_DataInit;
	papi_func_tbl->SetTuningSettingBuffer = VL_SetTuningSettingBuffer;
	papi_func_tbl->GetTuningSettingBuffer = VL_GetTuningSettingBuffer;
	papi_func_tbl->StaticInit = VL_StaticInit;
	papi_func_tbl->WaitDeviceBooted = VL_WaitDeviceBooted;
	papi_func_tbl->ResetDevice = VL_ResetDevice;
	papi_func_tbl->SetDeviceParameters = VL_SetDeviceParameters;
	papi_func_tbl->SetDeviceMode = VL_SetDeviceMode;
	papi_func_tbl->GetDeviceMode = VL_GetDeviceMode;
	papi_func_tbl->SetHistogramMode = VL_SetHistogramMode;
	papi_func_tbl->GetHistogramMode = VL_GetHistogramMode;
	papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds =
		VL_SetMeasurementTimingBudgetMicroSeconds;
	papi_func_tbl->GetMeasurementTimingBudgetMicroSeconds =
		VL_GetMeasurementTimingBudgetMicroSeconds;
	papi_func_tbl->GetVcselPulsePeriod = VL_GetVcselPulsePeriod;
	papi_func_tbl->SetVcselPulsePeriod = VL_SetVcselPulsePeriod;
	papi_func_tbl->SetSequenceStepEnable = VL_SetSequenceStepEnable;
	papi_func_tbl->GetSequenceStepEnable = VL_GetSequenceStepEnable;
	papi_func_tbl->GetSequenceStepEnables = VL_GetSequenceStepEnables;
	papi_func_tbl->SetSequenceStepTimeout = VL_SetSequenceStepTimeout;
	papi_func_tbl->GetSequenceStepTimeout = VL_GetSequenceStepTimeout;
	papi_func_tbl->GetNumberOfSequenceSteps =
		VL_GetNumberOfSequenceSteps;
	papi_func_tbl->GetSequenceStepsInfo = VL_GetSequenceStepsInfo;
	papi_func_tbl->SetInterMeasurementPeriodMilliSeconds =
		VL_SetInterMeasurementPeriodMilliSeconds;
	papi_func_tbl->GetInterMeasurementPeriodMilliSeconds =
		VL_GetInterMeasurementPeriodMilliSeconds;
	papi_func_tbl->SetXTalkCompensationEnable =
		VL_SetXTalkCompensationEnable;
	papi_func_tbl->GetXTalkCompensationEnable =
		VL_GetXTalkCompensationEnable;
	papi_func_tbl->SetXTalkCompensationRateMegaCps =
		VL_SetXTalkCompensationRateMegaCps;
	papi_func_tbl->GetXTalkCompensationRateMegaCps =
		VL_GetXTalkCompensationRateMegaCps;
	papi_func_tbl->GetNumberOfLimitCheck = VL_GetNumberOfLimitCheck;
	papi_func_tbl->GetLimitCheckInfo = VL_GetLimitCheckInfo;
	papi_func_tbl->SetLimitCheckEnable = VL_SetLimitCheckEnable;
	papi_func_tbl->GetLimitCheckEnable = VL_GetLimitCheckEnable;
	papi_func_tbl->SetLimitCheckValue = VL_SetLimitCheckValue;
	papi_func_tbl->GetLimitCheckValue = VL_GetLimitCheckValue;
	papi_func_tbl->GetLimitCheckCurrent = VL_GetLimitCheckCurrent;
	papi_func_tbl->SetWrapAroundCheckEnable =
		VL_SetWrapAroundCheckEnable;
	papi_func_tbl->GetWrapAroundCheckEnable =
		VL_GetWrapAroundCheckEnable;
	papi_func_tbl->PerformSingleMeasurement =
		VL_PerformSingleMeasurement;
	papi_func_tbl->PerformRefCalibration = VL_PerformRefCalibration;
	papi_func_tbl->SetRefCalibration = VL_SetRefCalibration;
	papi_func_tbl->GetRefCalibration = VL_GetRefCalibration;
	papi_func_tbl->PerformXTalkCalibration =
		VL_PerformXTalkCalibration;
	papi_func_tbl->PerformOffsetCalibration =
		VL_PerformOffsetCalibration;
	papi_func_tbl->StartMeasurement = VL_StartMeasurement;
	papi_func_tbl->StopMeasurement = VL_StopMeasurement;
	papi_func_tbl->GetMeasurementDataReady =
		VL_GetMeasurementDataReady;
	papi_func_tbl->WaitDeviceReadyForNewMeasurement =
		VL_WaitDeviceReadyForNewMeasurement;
	papi_func_tbl->GetRangingMeasurementData =
		VL_GetRangingMeasurementData;
	papi_func_tbl->GetHistogramMeasurementData =
		VL_GetHistogramMeasurementData;
	papi_func_tbl->PerformSingleRangingMeasurement =
		VL_PerformSingleRangingMeasurement;
	papi_func_tbl->PerformSingleHistogramMeasurement =
		VL_PerformSingleHistogramMeasurement;
	papi_func_tbl->SetNumberOfROIZones = VL_SetNumberOfROIZones;
	papi_func_tbl->GetNumberOfROIZones = VL_GetNumberOfROIZones;
	papi_func_tbl->GetMaxNumberOfROIZones = VL_GetMaxNumberOfROIZones;
	papi_func_tbl->SetGpioConfig = VL_SetGpioConfig;
	papi_func_tbl->GetGpioConfig = VL_GetGpioConfig;
	papi_func_tbl->SetInterruptThresholds = VL_SetInterruptThresholds;
	papi_func_tbl->GetInterruptThresholds = VL_GetInterruptThresholds;
	papi_func_tbl->ClearInterruptMask = VL_ClearInterruptMask;
	papi_func_tbl->GetInterruptMaskStatus = VL_GetInterruptMaskStatus;
	papi_func_tbl->EnableInterruptMask = VL_EnableInterruptMask;
	papi_func_tbl->SetSpadAmbientDamperThreshold =
		VL_SetSpadAmbientDamperThreshold;
	papi_func_tbl->GetSpadAmbientDamperThreshold =
		VL_GetSpadAmbientDamperThreshold;
	papi_func_tbl->SetSpadAmbientDamperFactor =
		VL_SetSpadAmbientDamperFactor;
	papi_func_tbl->GetSpadAmbientDamperFactor =
		VL_GetSpadAmbientDamperFactor;
	papi_func_tbl->PerformRefSpadManagement =
		VL_PerformRefSpadManagement;
	papi_func_tbl->SetReferenceSpads = VL_SetReferenceSpads;
	papi_func_tbl->GetReferenceSpads = VL_GetReferenceSpads;

}

static void stmvl53l0x_ps_read_measurement(struct vl_data *data)
{
	struct timeval tv;
	struct vl_data *vl53l0x_dev = data;
	int8_t Status = VL_ERROR_NONE;
	unsigned int LimitCheckCurrent;

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
	Status = papi_func_tbl->GetLimitCheckCurrent(vl53l0x_dev,
		VL_CHECKENABLE_SIGMA_FINAL_RANGE,
		&LimitCheckCurrent);
	if (Status == VL_ERROR_NONE) {
		input_report_abs(data->input_dev_ps, ABS_WHEEL,
				LimitCheckCurrent);
	}
	input_report_abs(data->input_dev_ps, ABS_PRESSURE,
		data->rangeData.EffectiveSpadRtnCount);
	input_sync(data->input_dev_ps);

	if (data->enableDebug)
		err("range:%d, RtnRateMcps:%d,err:0x%x\n",
		data->rangeData.RangeMilliMeter,
			data->rangeData.SignalRateRtnMegaCps,
			data->rangeData.RangeStatus);
		err("Dmax:%d,rtnambr:%d,time:%d,Spad:%d,SigmaLimit:%d\n",
			data->rangeData.RangeDMaxMilliMeter,
			data->rangeData.AmbientRateRtnMegaCps,
			data->rangeData.MeasurementTimeUsec,
			data->rangeData.EffectiveSpadRtnCount,
			LimitCheckCurrent);


}

static void stmvl53l0x_cancel_handler(struct vl_data *data)
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
		err("cancel_delayed_work return FALSE\n");

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

}

void stmvl53l0x_schedule_handler(struct vl_data *data)
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

/* Flag used to exit the thread when kthread_stop() is invoked */
static int poll_thread_exit;
int stmvl53l0x_poll_thread(void *data)
{
	struct vl_data *vl53l0x_dev = data;
	int8_t Status = VL_ERROR_NONE;
	uint32_t sleep_time = 0;
	uint32_t interruptStatus = 0;

	dbg("Starting Polling thread\n");

	while (!kthread_should_stop()) {
		/* Check if enable_ps_sensor is true or
		exit request is made. If not block */
		wait_event(vl53l0x_dev->poll_thread_wq,
			(vl53l0x_dev->enable_ps_sensor || poll_thread_exit));
		if (poll_thread_exit) {
			dbg("Exiting the poll thread\n");
			break;
		}

		mutex_lock(&vl53l0x_dev->work_mutex);

		sleep_time = vl53l0x_dev->delay_ms;
		Status = VL_GetInterruptMaskStatus(vl53l0x_dev,
			&interruptStatus);
		if (Status == VL_ERROR_NONE &&
			interruptStatus &&
			interruptStatus != vl53l0x_dev->interruptStatus) {
			vl53l0x_dev->interruptStatus = interruptStatus;
			vl53l0x_dev->noInterruptCount = 0;
			stmvl53l0x_schedule_handler(vl53l0x_dev);

		} else {
			vl53l0x_dev->noInterruptCount++;
		}

	/* Force Clear interrupt mask and restart if no interrupt
	after twice the timingBudget */
	if ((vl53l0x_dev->noInterruptCount * vl53l0x_dev->delay_ms) >
	(vl53l0x_dev->timingBudget * 2)) {
		dbg("No interrupt after (%u) msec(TimingBudget = %u)\n",
		(vl53l0x_dev->noInterruptCount * vl53l0x_dev->delay_ms),
			vl53l0x_dev->timingBudget);
		Status = papi_func_tbl->ClearInterruptMask(vl53l0x_dev, 0);
		if (vl53l0x_dev->deviceMode ==
			VL_DEVICEMODE_SINGLE_RANGING) {
			Status = papi_func_tbl->StartMeasurement(vl53l0x_dev);
			if (Status != VL_ERROR_NONE)
				dbg("Status = %d\n", Status);
			}
		}

		mutex_unlock(&vl53l0x_dev->work_mutex);

		msleep(sleep_time);
	}

	return 0;
}

/* work handler */
static void stmvl53l0x_work_handler(struct work_struct *work)
{
	struct vl_data *data = container_of(work,
	struct vl_data,	dwork.work);

	struct vl_data *vl53l0x_dev = data;

	int8_t Status = VL_ERROR_NONE;

	mutex_lock(&data->work_mutex);
	/* dbg("Enter\n"); */


	if (vl53l0x_dev->enable_ps_sensor == 1) {
#ifdef DEBUG_TIME_LOG
		stmvl53l0x_DebugTimeGet(&stop_tv);
		stmvl53l0x_DebugTimeDuration(&start_tv, &stop_tv);
#endif
		/* ISR has scheduled this function */
		if (vl53l0x_dev->interrupt_received == 1) {
			Status = papi_func_tbl->GetInterruptMaskStatus(
			vl53l0x_dev, &vl53l0x_dev->interruptStatus);
			if (Status != VL_ERROR_NONE) {
				dbg("%s(%d) : Status = %d\n",
				__func__, __LINE__, Status);
			}
			vl53l0x_dev->interrupt_received = 0;
		}
		if (data->enableDebug)
			dbg("interruptStatus:0x%x, interrupt_received:%d\n",
			vl53l0x_dev->interruptStatus,
				vl53l0x_dev->interrupt_received);

		if (vl53l0x_dev->interruptStatus ==
			vl53l0x_dev->gpio_function) {
			Status = papi_func_tbl->ClearInterruptMask(vl53l0x_dev,
				0);
			if (Status != VL_ERROR_NONE) {
				dbg("%s(%d) : Status = %d\n",
					__func__, __LINE__, Status);
			} else {
				Status =
				papi_func_tbl->GetRangingMeasurementData(
					vl53l0x_dev, &(data->rangeData));
				/* to push the measurement */
				if (Status == VL_ERROR_NONE)
					stmvl53l0x_ps_read_measurement(data);
				else
					dbg("%s(%d) : Status = %d\n",
						__func__, __LINE__, Status);

				dbg("Measured range:%d\n",
				data->rangeData.RangeMilliMeter);

				if (data->enableDebug)
					dbg("Measured range:%d\n",
					data->rangeData.RangeMilliMeter);

				if (data->deviceMode ==
					VL_DEVICEMODE_SINGLE_RANGING)
					Status =
					papi_func_tbl->StartMeasurement(
					vl53l0x_dev);
			}
		}
#ifdef DEBUG_TIME_LOG
		stmvl53l0x_DebugTimeGet(&start_tv);
#endif

		}


	vl53l0x_dev->interruptStatus = 0;

	mutex_unlock(&data->work_mutex);

}


/*
 * SysFS support
 */
static ssize_t stmvl53l0x_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl53l0x_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);

	unsigned int val;

	kstrtoint(buf, 10, &val);
	if ((val != 0) && (val != 1)) {
		err("store unvalid value=%ld\n", val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	dbg("Enter, enable_ps_sensor flag:%d\n",
		data->enable_ps_sensor);
	dbg("enable ps senosr ( %ld)\n", val);

	if (val == 1) {
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, NORMAL_MODE);
		} else {
			err("Already enabled. Skip !");
		}
	} else {
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0x_stop(data);
		}
	}
	dbg("End\n");
	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, 0664/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_enable_ps_sensor,
					stmvl53l0x_store_enable_ps_sensor);

static ssize_t stmvl53l0x_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enableDebug);
}

/* for debug */
static ssize_t stmvl53l0x_store_enable_debug(struct device *dev,
					struct device_attribute *attr, const
					char *buf, size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);
	int on;

	kstrtoint(buf, 10, &on);
	if ((on != 0) &&  (on != 1)) {
		err("set debug=%ld\n", on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(enable_debug, 0660/*S_IWUSR | S_IRUGO*/,
				   stmvl53l0x_show_enable_debug,
					stmvl53l0x_store_enable_debug);

static ssize_t stmvl53l0x_show_set_delay_ms(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl53l0x_store_set_delay_ms(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);
	int delay_ms;

	kstrtoint(buf, 10, &delay_ms);
	if (delay_ms == 0) {
		err("set delay_ms=%ld\n", delay_ms);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->delay_ms = delay_ms;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_delay_ms, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_set_delay_ms,
					stmvl53l0x_store_set_delay_ms);

/* Timing Budget */
static ssize_t stmvl53l0x_show_timing_budget(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n", data->timingBudget);
}

static ssize_t stmvl53l0x_store_set_timing_budget(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);
	int timingBudget;

	kstrtoint(buf, 10, &timingBudget);
	if (timingBudget == 0) {
		err("set timingBudget=%ld\n", timingBudget);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->timingBudget = timingBudget;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_timing_budget, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_timing_budget,
					stmvl53l0x_store_set_timing_budget);


/* Long Range  */
static ssize_t stmvl53l0x_show_long_range(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->useLongRange);
}

static ssize_t stmvl53l0x_store_set_long_range(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);
	int useLongRange;

	kstrtoint(buf, 10, &useLongRange);
	if ((useLongRange != 0) &&  (useLongRange != 1)) {
		err("set useLongRange=%ld\n", useLongRange);
		return count;
	}

	mutex_lock(&data->work_mutex);
	data->useLongRange = useLongRange;
	if (useLongRange)
		data->timingBudget = 26000;
	else
		data->timingBudget = 200000;

	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_long_range, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_long_range,
					stmvl53l0x_store_set_long_range);

static ssize_t stmvl53l0x_show_meter(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);
	struct VL_RangingMeasurementData_t Measure;

	papi_func_tbl->PerformSingleRangingMeasurement(data, &Measure);
	dbg("Measure = %d\n", Measure.RangeMilliMeter);
	return snprintf(buf, 4, "%d\n", Measure.RangeMilliMeter);
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(show_meter, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_meter,
				   NULL);

static ssize_t stmvl53l0x_show_xtalk(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);
	struct VL_RangingMeasurementData_t Measure;

	dbg("Measure = %d\n", Measure.RangeMilliMeter);
	return snprintf(buf, 4, "%d\n", Measure.RangeMilliMeter);
}

static ssize_t stmvl53l0x_set_xtalk(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);
	unsigned int targetDistance;

	kstrtoint(buf, 10, &targetDistance);
	data->xtalkCalDistance = targetDistance;
	stmvl53l0x_start(data, 3, XTALKCALIB_MODE);
	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(xtalk_cal, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_xtalk,
				   stmvl53l0x_set_xtalk);

static ssize_t stmvl53l0x_show_offset(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct vl_data *data = dev_get_drvdata(dev);
	struct VL_RangingMeasurementData_t Measure;

	papi_func_tbl->PerformSingleRangingMeasurement(data, &Measure);
	dbg("Measure = %d\n", Measure.RangeMilliMeter);
	return snprintf(buf, 4, "%d\n", Measure.RangeMilliMeter);
}

static ssize_t stmvl53l0x_set_offset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct vl_data *data = dev_get_drvdata(dev);
	unsigned int targetDistance;

	kstrtoint(buf, 10, &targetDistance);
	data->offsetCalDistance = targetDistance;
	stmvl53l0x_start(data, 3, OFFSETCALIB_MODE);
	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(offset_cal, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_offset,
				   stmvl53l0x_set_offset);

static struct attribute *stmvl53l0x_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_set_delay_ms.attr ,
	&dev_attr_set_timing_budget.attr ,
	&dev_attr_set_long_range.attr ,
	&dev_attr_show_meter.attr ,
	&dev_attr_xtalk_cal.attr ,
	&dev_attr_offset_cal.attr ,
	NULL
};


static const struct attribute_group stmvl53l0x_attr_group = {
	.attrs = stmvl53l0x_attributes,
};

/*
 * misc device file operation functions
 */
static int stmvl53l0x_ioctl_handler(struct file *file,
			unsigned int cmd, unsigned long arg,
			void __user *p)
{
	int rc = 0;
	unsigned int xtalkint = 0;
	unsigned int targetDistance = 0;
	int8_t offsetint = 0;
	struct vl_data *data =
			container_of(file->private_data,
				struct vl_data, miscdev);
	struct stmvl53l0x_register reg;
	struct stmvl53l0x_parameter parameter;
	struct vl_data *vl53l0x_dev = data;
	uint8_t deviceMode;
	uint8_t page_num = 0;

	if (!data)
		return -EINVAL;

	dbg("Enter enable_ps_sensor:%d\n", data->enable_ps_sensor);
	switch (cmd) {
	/* enable */
	case VL_IOCTL_INIT:
		dbg("VL_IOCTL_INIT\n");
		/* turn on tof sensor only if it's not enabled by other
		client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, NORMAL_MODE);
		} else
			rc = -EINVAL;
		break;
	/* crosstalk calibration */
	case VL_IOCTL_XTALKCALB:
		dbg("VL_IOCTL_XTALKCALB\n");
		data->xtalkCalDistance = 100;
		if (copy_from_user(&targetDistance, (unsigned int *)p,
			sizeof(unsigned int))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		data->xtalkCalDistance = targetDistance;

		/* turn on tof sensor only if it's not enabled by other
		client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, XTALKCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up Xtalk value */
	case VL_IOCTL_SETXTALK:
		dbg("VL_IOCTL_SETXTALK\n");
		if (copy_from_user(&xtalkint, (unsigned int *)p,
			sizeof(unsigned int))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		dbg("SETXTALK as 0x%x\n", xtalkint);
/* later
 *	SetXTalkCompensationRate(vl53l0x_dev, xtalkint);
 */
		break;
	/* offset calibration */
	case VL_IOCTL_OFFCALB:
		dbg("VL_IOCTL_OFFCALB\n");
		data->offsetCalDistance = 50;
		if (copy_from_user(&targetDistance, (unsigned int *)p,
			sizeof(unsigned int))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		data->offsetCalDistance = targetDistance;
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, OFFSETCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up offset value */
	case VL_IOCTL_SETOFFSET:
		dbg("VL_IOCTL_SETOFFSET\n");
		if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		dbg("SETOFFSET as %d\n", offsetint);
/* later
		SetOffsetCalibrationData(vl53l0x_dev, offsetint);
*/
		break;
	/* disable */
	case VL_IOCTL_STOP:
		dbg("VL_IOCTL_STOP\n");
		/* turn off tof sensor only if it's enabled by other client */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0x_stop(data);
		}
		break;
	/* Get all range data */
	case VL_IOCTL_GETDATAS:
		dbg("VL_IOCTL_GETDATAS\n");
		if (copy_to_user((struct VL_RangingMeasurementData_t *)p,
			&(data->rangeData),
			sizeof(struct VL_RangingMeasurementData_t))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* Register tool */
	case VL_IOCTL_REGISTER:
		dbg("VL_IOCTL_REGISTER\n");
		if (copy_from_user(&reg, (struct stmvl53l0x_register *)p,
			sizeof(struct stmvl53l0x_register))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		reg.status = 0;
		page_num = (uint8_t)((reg.reg_index & 0x0000ff00) >> 8);
		dbg(
"VL_IOCTL_REGISTER,	page number:%d\n", page_num);
		if (page_num != 0)
			reg.status = VL_WrByte(vl53l0x_dev,
				0xFF, page_num);

		switch (reg.reg_bytes) {
		case(4):
			if (reg.is_read)
				reg.status = VL_RdDWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					&reg.reg_data);
			else
				reg.status = VL_WrDWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					reg.reg_data);
			break;
		case(2):
			if (reg.is_read)
				reg.status = VL_RdWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint16_t *)&reg.reg_data);
			else
				reg.status = VL_WrWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint16_t)reg.reg_data);
			break;
		case(1):
			if (reg.is_read)
				reg.status = VL_RdByte(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint8_t *)&reg.reg_data);
			else
				reg.status = VL_WrByte(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint8_t)reg.reg_data);
			break;
		default:
			reg.status = -1;

		}
		if (page_num != 0)
			reg.status = VL_WrByte(vl53l0x_dev, 0xFF, 0);


		if (copy_to_user((struct stmvl53l0x_register *)p, &reg,
				sizeof(struct stmvl53l0x_register))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* parameter access */
	case VL_IOCTL_PARAMETER:
		dbg("VL_IOCTL_PARAMETER\n");
		if (copy_from_user(&parameter, (struct stmvl53l0x_parameter *)p,
				sizeof(struct stmvl53l0x_parameter))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		parameter.status = 0;
		if (data->enableDebug)
			dbg("VL_IOCTL_PARAMETER Name = %d\n",
				parameter.name);
		switch (parameter.name) {
		case (OFFSET_PAR):
			if (parameter.is_read)
				parameter.status =
			papi_func_tbl->GetOffsetCalibrationDataMicroMeter(
						vl53l0x_dev, &parameter.value);
			else
				parameter.status =
			papi_func_tbl->SetOffsetCalibrationDataMicroMeter(
				vl53l0x_dev, parameter.value);
			dbg("get parameter value as %d\n",
				parameter.value);
			break;

		case (REFERENCESPADS_PAR):
			if (parameter.is_read) {
				parameter.status =
				papi_func_tbl->GetReferenceSpads(vl53l0x_dev,
					(uint32_t *)&(parameter.value),
					(uint8_t *)&(parameter.value2));
				if (data->enableDebug)
					dbg("Get RefSpad: Count:%u, Type:%u\n",
				parameter.value, (uint8_t)parameter.value2);
			} else {
				if (data->enableDebug)
					dbg("Set RefSpad: Count:%u, Type:%u\n",
				parameter.value, (uint8_t)parameter.value2);

				parameter.status =
					papi_func_tbl->SetReferenceSpads(
					vl53l0x_dev,
					(uint32_t)(parameter.value),
					(uint8_t)(parameter.value2));
			}
			break;

		case (REFCALIBRATION_PAR):
			if (parameter.is_read) {
				parameter.status =
				papi_func_tbl->GetRefCalibration(vl53l0x_dev,
				(uint8_t *)&(parameter.value),
				(uint8_t *)&(parameter.value2));
				if (data->enableDebug)
					dbg("Get Ref: Vhv:%u, PhaseCal:%u\n",
					(uint8_t)parameter.value,
					(uint8_t)parameter.value2);
			} else {
				if (data->enableDebug)
					dbg("Set Ref: Vhv:%u, PhaseCal:%u\n",
					(uint8_t)parameter.value,
					(uint8_t)parameter.value2);
				parameter.status =
					papi_func_tbl->SetRefCalibration(
					vl53l0x_dev, (uint8_t)(parameter.value),
					(uint8_t)(parameter.value2));
			}
			break;
		case (XTALKRATE_PAR):
			if (parameter.is_read)
				parameter.status =
				papi_func_tbl->GetXTalkCompensationRateMegaCps(
						vl53l0x_dev, (unsigned int *)
						&parameter.value);
			else
				parameter.status =
				papi_func_tbl->SetXTalkCompensationRateMegaCps(
						vl53l0x_dev,
						(unsigned int)
							parameter.value);

			break;
		case (XTALKENABLE_PAR):
			if (parameter.is_read)
				parameter.status =
				papi_func_tbl->GetXTalkCompensationEnable(
						vl53l0x_dev,
						(uint8_t *) &parameter.value);
			else
				parameter.status =
				papi_func_tbl->SetXTalkCompensationEnable(
						vl53l0x_dev,
						(uint8_t) parameter.value);
			break;
		case (GPIOFUNC_PAR):
			if (parameter.is_read) {
				parameter.status =
				papi_func_tbl->GetGpioConfig(vl53l0x_dev, 0,
						&deviceMode,
						&data->gpio_function,
						&data->gpio_polarity);
				parameter.value = data->gpio_function;
			} else {
				data->gpio_function = parameter.value;
				parameter.status =
				papi_func_tbl->SetGpioConfig(vl53l0x_dev, 0, 0,
						data->gpio_function,
						data->gpio_polarity);
			}
			break;
		case (LOWTHRESH_PAR):
			if (parameter.is_read) {
				parameter.status =
				papi_func_tbl->GetInterruptThresholds(
				vl53l0x_dev, 0, &(data->low_threshold),
				&(data->high_threshold));
				parameter.value = data->low_threshold >> 16;
			} else {
				data->low_threshold = parameter.value << 16;
				parameter.status =
				papi_func_tbl->SetInterruptThresholds(
				vl53l0x_dev, 0, data->low_threshold,
				data->high_threshold);
			}
			break;
		case (HIGHTHRESH_PAR):
			if (parameter.is_read) {
				parameter.status =
				papi_func_tbl->GetInterruptThresholds(
				vl53l0x_dev, 0, &(data->low_threshold),
				&(data->high_threshold));
				parameter.value = data->high_threshold >> 16;
			} else {
				data->high_threshold = parameter.value << 16;
				parameter.status =
				papi_func_tbl->SetInterruptThresholds(
				vl53l0x_dev, 0, data->low_threshold,
				data->high_threshold);
			}
			break;
		case (DEVICEMODE_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetDeviceMode(
					vl53l0x_dev,
					(uint8_t *)&
					(parameter.value));
			} else {
				parameter.status =
					papi_func_tbl->SetDeviceMode(
					vl53l0x_dev,
					(uint8_t)(parameter.value));
					data->deviceMode =
					(uint8_t)(parameter.value);
			}
			break;



		case (INTERMEASUREMENT_PAR):
			if (parameter.is_read) {
				parameter.status =
			papi_func_tbl->GetInterMeasurementPeriodMilliSeconds(
				vl53l0x_dev, (uint32_t *)&(parameter.value));
			} else {
				parameter.status =
			papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(
				vl53l0x_dev, (uint32_t)(parameter.value));
				data->interMeasurems = parameter.value;
			}
			break;

		}

		if (copy_to_user((struct stmvl53l0x_parameter *)p, &parameter,
				sizeof(struct stmvl53l0x_parameter))) {
			err("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int stmvl53l0x_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int stmvl53l0x_flush(struct file *file, fl_owner_t id)
{
	struct vl_data *data = container_of(file->private_data,
					struct vl_data, miscdev);
	(void) file;
	(void) id;

	if (data) {
		if (data->enable_ps_sensor == 1) {
			/* turn off tof sensor if it's enabled */
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0x_stop(data);
		}
	}
	return 0;
}

static long stmvl53l0x_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret;
	struct vl_data *data =
			container_of(file->private_data,
					struct vl_data, miscdev);
	mutex_lock(&data->work_mutex);
	ret = stmvl53l0x_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&data->work_mutex);

	return ret;
}

/*
 * Initialization function
 */
static int stmvl53l0x_init_client(struct vl_data *data)
{

	int8_t Status = VL_ERROR_NONE;
	struct VL_DeviceInfo_t DeviceInfo;
	struct vl_data *vl53l0x_dev = data;

	uint32_t refSpadCount;
	uint8_t isApertureSpads;
	uint8_t VhvSettings;
	uint8_t PhaseCal;

	dbg("Enter\n");

	vl53l0x_dev->I2cDevAddr      = 0x52;
	vl53l0x_dev->comms_type      =  1;
	vl53l0x_dev->comms_speed_khz =  400;

	/* Setup API functions based on revision */
	stmvl53l0x_setupAPIFunctions();

	if (Status == VL_ERROR_NONE && data->reset) {
		dbg("Call of VL_DataInit\n");
		/* Data initialization */
		Status = papi_func_tbl->DataInit(vl53l0x_dev);
	}

	if (Status == VL_ERROR_NONE) {
		dbg("VL_GetDeviceInfo:\n");
		Status = papi_func_tbl->GetDeviceInfo(vl53l0x_dev, &DeviceInfo);
		if (Status == VL_ERROR_NONE) {
			dbg("Device Name : %s\n", DeviceInfo.Name);
			dbg("Device Type : %s\n", DeviceInfo.Type);
			dbg("Device ID : %s\n", DeviceInfo.ProductId);
			dbg("Product type: %d\n", DeviceInfo.ProductType);
			dbg("ProductRevisionMajor : %d\n",
				DeviceInfo.ProductRevisionMajor);
			dbg("ProductRevisionMinor : %d\n",
				DeviceInfo.ProductRevisionMinor);
		}
	}

	if (Status == VL_ERROR_NONE) {
		dbg("Call of VL_StaticInit\n");
		Status = papi_func_tbl->StaticInit(vl53l0x_dev);
		/* Device Initialization */
	}

	if (Status == VL_ERROR_NONE && data->reset) {
		if (papi_func_tbl->PerformRefCalibration != NULL) {
			dbg("Call of VL_PerformRefCalibration\n");
			Status = papi_func_tbl->PerformRefCalibration(
				vl53l0x_dev, &VhvSettings, &PhaseCal);
				/* Ref calibration */
		}
	}

	if (Status == VL_ERROR_NONE && data->reset) {
		if (papi_func_tbl->PerformRefSpadManagement != NULL) {
			dbg("Call of VL_PerformRefSpadManagement\n");
			Status = papi_func_tbl->PerformRefSpadManagement(
				vl53l0x_dev, &refSpadCount, &isApertureSpads);
				/* Ref Spad Management */
		}
		data->reset = 0;
		/* needed, even the function is NULL */
	}

	if (Status == VL_ERROR_NONE) {

		dbg("Call of VL_SetDeviceMode\n");
		Status = papi_func_tbl->SetDeviceMode(vl53l0x_dev,
					VL_DEVICEMODE_SINGLE_RANGING);
		/* Setup in	single ranging mode */
	}
	if (Status == VL_ERROR_NONE)
		Status = papi_func_tbl->SetWrapAroundCheckEnable(
			vl53l0x_dev, 1);

	dbg("End\n");

	return 0;
}

static int stmvl53l0x_start(struct vl_data *data, uint8_t scaling,
	enum init_mode_e mode)
{
	int rc = 0;
	struct vl_data *vl53l0x_dev = data;
	int8_t Status = VL_ERROR_NONE;

	dbg("Enter\n");

	/* Power up */
	rc = pmodule_func_tbl->power_up(data->client_object, &data->reset);
	if (rc) {
		err("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	/* init */
	rc = stmvl53l0x_init_client(data);
	if (rc) {
		err("%d, error rc %d\n", __LINE__, rc);
		pmodule_func_tbl->power_down(data->client_object);
		return -EINVAL;
	}

	/* check mode */
	if (mode != NORMAL_MODE)
		papi_func_tbl->SetXTalkCompensationEnable(vl53l0x_dev, 1);

	if (mode == OFFSETCALIB_MODE) {
		/*VL_SetOffsetCalibrationDataMicroMeter(vl53l0x_dev, 0);*/
		unsigned int OffsetMicroMeter;

		papi_func_tbl->PerformOffsetCalibration(vl53l0x_dev,
			(data->offsetCalDistance<<16),
			&OffsetMicroMeter);
		dbg("Offset calibration:%u\n", OffsetMicroMeter);
		return rc;
	} else if (mode == XTALKCALIB_MODE) {
		unsigned int XTalkCompensationRateMegaCps;
		/*caltarget distance : 100mm and convert to
		* fixed point 16 16 format
		*/
		papi_func_tbl->PerformXTalkCalibration(vl53l0x_dev,
			(data->xtalkCalDistance<<16),
			&XTalkCompensationRateMegaCps);
		dbg("Xtalk calibration:%u\n", XTalkCompensationRateMegaCps);
		return rc;
	}
	/* set up device parameters */
	data->gpio_polarity = VL_INTERRUPTPOLARITY_LOW;

	/* Following two calls are made from IOCTL as well */
	papi_func_tbl->SetGpioConfig(vl53l0x_dev, 0, 0,
		data->gpio_function,
		VL_INTERRUPTPOLARITY_LOW);

	papi_func_tbl->SetInterruptThresholds(vl53l0x_dev, 0,
		data->low_threshold, data->high_threshold);

	if (data->deviceMode == VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING) {
		papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(
			vl53l0x_dev, data->interMeasurems);
	}

	dbg("DeviceMode:0x%x, interMeasurems:%d==\n", data->deviceMode,
			data->interMeasurems);
	papi_func_tbl->SetDeviceMode(vl53l0x_dev,
			data->deviceMode);
	papi_func_tbl->ClearInterruptMask(vl53l0x_dev,
							0);

	if (vl53l0x_dev->useLongRange) {
		dbg("Configure Long Ranging\n");
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
			VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			(unsigned int)(65536/10)); /* 0.1 * 65536 */
		if (Status == VL_ERROR_NONE) {
			Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
				VL_CHECKENABLE_SIGMA_FINAL_RANGE,
				(unsigned int)(60*65536));
		} else {
			dbg("SIGNAL_RATE_FINAL_RANGE failed err = %d\n",
				Status);
		}

		if (Status == VL_ERROR_NONE) {
			dbg("Set Timing Budget = %u\n",
				vl53l0x_dev->timingBudget);
			Status =
			papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds(
				vl53l0x_dev, vl53l0x_dev->timingBudget);
		} else {
			dbg("SetLimitCheckValue failed err =%d\n",
				Status);
		}

		if (Status == VL_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
				VL_VCSEL_PERIOD_PRE_RANGE, 18);
		} else {
			dbg("SetMeasurementTimingBudget failed err = %d\n",
				Status);
		}

		if (Status == VL_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
				VL_VCSEL_PERIOD_FINAL_RANGE, 14);
		} else {
			dbg("SetVcselPulsePeriod(PRE, 18) failed err = %d\n",
				Status);
		}

		if (Status != VL_ERROR_NONE) {
			dbg("SetVcselPulsePeriod(FINAL, 14) failed err = %d\n",
				Status);
		}
	} else {
		dbg("Configure High Accuracy\n");
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
			VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			(unsigned int)(25 * 65536 / 100)); /* 0.25 * 65536 */
		if (Status == VL_ERROR_NONE) {
			Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
				VL_CHECKENABLE_SIGMA_FINAL_RANGE,
				(unsigned int)(18*65536));
		} else {
			dbg("SIGNAL_RATE_FINAL_RANGE failed err = %d\n",
				Status);
		}

		if (Status == VL_ERROR_NONE) {
			dbg("Set Timing Budget = %u\n",
				vl53l0x_dev->timingBudget);
			Status =
			papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds(
				vl53l0x_dev, vl53l0x_dev->timingBudget);
		} else {
			dbg("SetLimitCheckValue failed err = %d\n",
				Status);
		}

		if (Status == VL_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
				VL_VCSEL_PERIOD_PRE_RANGE, 14);
		} else {
			dbg("SetMeasurementTimingBudget failed err = %d\n",
				Status);
		}

		if (Status == VL_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
				VL_VCSEL_PERIOD_FINAL_RANGE, 10);
		} else {
			dbg("SetVcselPulsePeriod failed err = %d\n",
				Status);
		}

		if (Status != VL_ERROR_NONE) {
			dbg("SetVcselPulsePeriod failed err = %d\n",
				Status);
		}

	}

	/* start the ranging */
	papi_func_tbl->StartMeasurement(vl53l0x_dev);
	data->enable_ps_sensor = 1;


	dbg("End\n");

	return rc;
}

static int stmvl53l0x_stop(struct vl_data *data)
{
	int rc = 0;
	struct vl_data *vl53l0x_dev = data;

	dbg("Enter\n");

	/* stop - if continuous mode */
	if (data->deviceMode == VL_DEVICEMODE_CONTINUOUS_RANGING ||
		data->deviceMode == VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING)
		papi_func_tbl->StopMeasurement(vl53l0x_dev);

	/* clean interrupt */
	papi_func_tbl->ClearInterruptMask(vl53l0x_dev, 0);

	/* cancel work handler */
	stmvl53l0x_cancel_handler(data);
	/* power down */
	rc = pmodule_func_tbl->power_down(data->client_object);
	if (rc) {
		err("%d, error rc %d\n", __LINE__, rc);
		return rc;
	}
	dbg("End\n");

	return rc;
}

/*
 * I2C init/probing/exit functions
 */
static const struct file_operations stmvl53l0x_ranging_fops = {
	.owner =			THIS_MODULE,
	.unlocked_ioctl =	stmvl53l0x_ioctl,
	.open =				stmvl53l0x_open,
	.flush =			stmvl53l0x_flush,
};

int stmvl53l0x_setup(struct vl_data *data)
{
	int rc = 0;

	dbg("Enter\n");

	/* init mutex */
	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);

	init_waitqueue_head(&data->poll_thread_wq);

	data->poll_thread = kthread_run(&stmvl53l0x_poll_thread,
				(void *)data, "STM-VL53L0");
	if (data->poll_thread == NULL) {
		dbg("%s(%d) - Failed to create Polling thread\n",
			__func__, __LINE__);
		goto exit_free_irq;
	}

	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl53l0x_work_handler);

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		rc = -ENOMEM;
		err("%d error:%d\n", __LINE__, rc);

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

	input_set_abs_params(data->input_dev_ps, ABS_WHEEL , 0, 0xffffffff,
		0, 0);

	data->input_dev_ps->name = "STM VL53L0 proximity sensor";

	rc = input_register_device(data->input_dev_ps);
	if (rc) {
		rc = -ENOMEM;
		err("%d error:%d\n", __LINE__, rc);
		goto exit_free_dev_ps;
	}
	/* setup drv data */
	input_set_drvdata(data->input_dev_ps, data);

	/* Register sysfs hooks */
	data->range_kobj = kobject_create_and_add("range", kernel_kobj);
	if (!data->range_kobj) {
		rc = -ENOMEM;
		err("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps;
	}
	rc = sysfs_create_group(&data->input_dev_ps->dev.kobj,
			&stmvl53l0x_attr_group);
	if (rc) {
		rc = -ENOMEM;
		err("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps_1;
	}
	/* init default device parameter value */
	data->enable_ps_sensor = 0;
	data->reset = 1;
	data->delay_ms = 30;	/* delay time to 30ms */
	data->enableDebug = 0;
	data->gpio_polarity = VL_INTERRUPTPOLARITY_LOW;
	data->gpio_function = VL_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
	data->low_threshold = 60;
	data->high_threshold = 200;
	data->deviceMode = VL_DEVICEMODE_SINGLE_RANGING;
	data->interMeasurems = 30;
	data->timingBudget = 26000;
	data->useLongRange = 1;

	dbg("support ver. %s enabled\n", DRIVER_VERSION);
	dbg("End");

	return 0;
exit_unregister_dev_ps_1:
	kobject_put(data->range_kobj);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_free_irq:
	kfree(data);
	return rc;
}

static int __init stmvl53l0x_init(void)
{
	int ret = -1;

	dbg("Enter\n");

	/* assign function table */
	pmodule_func_tbl = &stmvl53l0x_module_func_tbl;
	papi_func_tbl = &stmvl53l0x_api_func_tbl;

	/* client specific init function */
	ret = pmodule_func_tbl->init();

	if (ret)
		err("%d failed with %d\n", __LINE__, ret);

	dbg("End\n");

	return ret;
}

static void __exit stmvl53l0x_exit(void)
{
	dbg("Enter\n");

	dbg("End\n");
}

MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL v2");

module_init(stmvl53l0x_init);
module_exit(stmvl53l0x_exit);
