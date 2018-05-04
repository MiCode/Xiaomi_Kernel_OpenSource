/*
 *  vl53l0x_api_string.h - Linux kernel modules for
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



#ifndef VL_API_STRINGS_H_
#define VL_API_STRINGS_H_

#include "vl53l0x_def.h"
#include "vl53l0x_platform.h"

#ifdef __cplusplus
extern "C" {
#endif


int8_t VL_get_device_info(struct vl_data *Dev,
			struct VL_DeviceInfo_t *pVL_DeviceInfo);

int8_t VL_get_device_error_string(uint8_t ErrorCode,
		char *pDeviceErrorString);

int8_t VL_get_range_status_string(uint8_t RangeStatus,
		char *pRangeStatusString);

int8_t VL_get_pal_error_string(int8_t PalErrorCode,
		char *pPalErrorString);

int8_t VL_get_pal_state_string(uint8_t PalStateCode,
		char *pPalStateString);

int8_t VL_get_sequence_steps_info(
		uint8_t SequenceStepId,
		char *pSequenceStepsString);

int8_t VL_get_limit_check_info(struct vl_data *Dev,
	uint16_t LimitCheckId, char *pLimitCheckString);


#ifdef USE_EMPTY_STRING
	#define  VL_STRING_DEVICE_INFO_NAME                             ""
	#define  VL_STRING_DEVICE_INFO_NAME_TS0                         ""
	#define  VL_STRING_DEVICE_INFO_NAME_TS1                         ""
	#define  VL_STRING_DEVICE_INFO_NAME_TS2                         ""
	#define  VL_STRING_DEVICE_INFO_NAME_ES1                         ""
	#define  VL_STRING_DEVICE_INFO_TYPE                             ""

	/* PAL ERROR strings */
	#define  VL_STRING_ERROR_NONE                                   ""
	#define  VL_STRING_ERROR_CALIBRATION_WARNING                    ""
	#define  VL_STRING_ERROR_MIN_CLIPPED                            ""
	#define  VL_STRING_ERROR_UNDEFINED                              ""
	#define  VL_STRING_ERROR_INVALID_PARAMS                         ""
	#define  VL_STRING_ERROR_NOT_SUPPORTED                          ""
	#define  VL_STRING_ERROR_RANGE_ERROR                            ""
	#define  VL_STRING_ERROR_TIME_OUT                               ""
	#define  VL_STRING_ERROR_MODE_NOT_SUPPORTED                     ""
	#define  VL_STRING_ERROR_BUFFER_TOO_SMALL                       ""
	#define  VL_STRING_ERROR_GPIO_NOT_EXISTING                      ""
	#define  VL_STRING_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED       ""
	#define  VL_STRING_ERROR_CONTROL_INTERFACE                      ""
	#define  VL_STRING_ERROR_INVALID_COMMAND                        ""
	#define  VL_STRING_ERROR_DIVISION_BY_ZERO                       ""
	#define  VL_STRING_ERROR_REF_SPAD_INIT                          ""
	#define  VL_STRING_ERROR_NOT_IMPLEMENTED                        ""

	#define  VL_STRING_UNKNOWN_ERROR_CODE                            ""



	/* Range Status */
	#define  VL_STRING_RANGESTATUS_NONE                             ""
	#define  VL_STRING_RANGESTATUS_RANGEVALID                       ""
	#define  VL_STRING_RANGESTATUS_SIGMA                            ""
	#define  VL_STRING_RANGESTATUS_SIGNAL                           ""
	#define  VL_STRING_RANGESTATUS_MINRANGE                         ""
	#define  VL_STRING_RANGESTATUS_PHASE                            ""
	#define  VL_STRING_RANGESTATUS_HW                               ""


	/* Range Status */
	#define  VL_STRING_STATE_POWERDOWN                              ""
	#define  VL_STRING_STATE_WAIT_STATICINIT                        ""
	#define  VL_STRING_STATE_STANDBY                                ""
	#define  VL_STRING_STATE_IDLE                                   ""
	#define  VL_STRING_STATE_RUNNING                                ""
	#define  VL_STRING_STATE_UNKNOWN                                ""
	#define  VL_STRING_STATE_ERROR                                  ""


	/* Device Specific */
	#define  VL_STRING_DEVICEERROR_NONE                             ""
	#define  VL_STRING_DEVICEERROR_VCSELCONTINUITYTESTFAILURE       ""
	#define  VL_STRING_DEVICEERROR_VCSELWATCHDOGTESTFAILURE         ""
	#define  VL_STRING_DEVICEERROR_NOVHVVALUEFOUND                  ""
	#define  VL_STRING_DEVICEERROR_MSRCNOTARGET                     ""
	#define  VL_STRING_DEVICEERROR_SNRCHECK                         ""
	#define  VL_STRING_DEVICEERROR_RANGEPHASECHECK                  ""
	#define  VL_STRING_DEVICEERROR_SIGMATHRESHOLDCHECK              ""
	#define  VL_STRING_DEVICEERROR_TCC                              ""
	#define  VL_STRING_DEVICEERROR_PHASECONSISTENCY                 ""
	#define  VL_STRING_DEVICEERROR_MINCLIP                          ""
	#define  VL_STRING_DEVICEERROR_RANGECOMPLETE                    ""
	#define  VL_STRING_DEVICEERROR_ALGOUNDERFLOW                    ""
	#define  VL_STRING_DEVICEERROR_ALGOOVERFLOW                     ""
	#define  VL_STRING_DEVICEERROR_RANGEIGNORETHRESHOLD             ""
	#define  VL_STRING_DEVICEERROR_UNKNOWN                          ""

	/* Check Enable */
	#define  VL_STRING_CHECKENABLE_SIGMA_FINAL_RANGE                ""
	#define  VL_STRING_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE          ""
	#define  VL_STRING_CHECKENABLE_SIGNAL_REF_CLIP                  ""
	#define  VL_STRING_CHECKENABLE_RANGE_IGNORE_THRESHOLD           ""

	/* Sequence Step */
	#define  VL_STRING_SEQUENCESTEP_TCC                             ""
	#define  VL_STRING_SEQUENCESTEP_DSS                             ""
	#define  VL_STRING_SEQUENCESTEP_MSRC                            ""
	#define  VL_STRING_SEQUENCESTEP_PRE_RANGE                       ""
	#define  VL_STRING_SEQUENCESTEP_FINAL_RANGE                     ""
#else
	#define  VL_STRING_DEVICE_INFO_NAME          "VL53L0X cut1.0"
	#define  VL_STRING_DEVICE_INFO_NAME_TS0      "VL53L0X TS0"
	#define  VL_STRING_DEVICE_INFO_NAME_TS1      "VL53L0X TS1"
	#define  VL_STRING_DEVICE_INFO_NAME_TS2      "VL53L0X TS2"
	#define  VL_STRING_DEVICE_INFO_NAME_ES1      "VL53L0X ES1 or later"
	#define  VL_STRING_DEVICE_INFO_TYPE          "VL53L0X"

	/* PAL ERROR strings */
	#define  VL_STRING_ERROR_NONE \
			"No Error"
	#define  VL_STRING_ERROR_CALIBRATION_WARNING \
			"Calibration Warning Error"
	#define  VL_STRING_ERROR_MIN_CLIPPED \
			"Min clipped error"
	#define  VL_STRING_ERROR_UNDEFINED \
			"Undefined error"
	#define  VL_STRING_ERROR_INVALID_PARAMS \
			"Invalid parameters error"
	#define  VL_STRING_ERROR_NOT_SUPPORTED \
			"Not supported error"
	#define  VL_STRING_ERROR_RANGE_ERROR \
			"Range error"
	#define  VL_STRING_ERROR_TIME_OUT \
			"Time out error"
	#define  VL_STRING_ERROR_MODE_NOT_SUPPORTED \
			"Mode not supported error"
	#define  VL_STRING_ERROR_BUFFER_TOO_SMALL \
			"Buffer too small"
	#define  VL_STRING_ERROR_GPIO_NOT_EXISTING \
			"GPIO not existing"
	#define  VL_STRING_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED \
			"GPIO funct not supported"
	#define  VL_STRING_ERROR_INTERRUPT_NOT_CLEARED \
			"Interrupt not Cleared"
	#define  VL_STRING_ERROR_CONTROL_INTERFACE \
			"Control Interface Error"
	#define  VL_STRING_ERROR_INVALID_COMMAND \
			"Invalid Command Error"
	#define  VL_STRING_ERROR_DIVISION_BY_ZERO \
			"Division by zero Error"
	#define  VL_STRING_ERROR_REF_SPAD_INIT \
			"Reference Spad Init Error"
	#define  VL_STRING_ERROR_NOT_IMPLEMENTED \
			"Not implemented error"

	#define  VL_STRING_UNKNOWN_ERROR_CODE \
			"Unknown Error Code"



	/* Range Status */
	#define  VL_STRING_RANGESTATUS_NONE                 "No Update"
	#define  VL_STRING_RANGESTATUS_RANGEVALID           "Range Valid"
	#define  VL_STRING_RANGESTATUS_SIGMA                "Sigma Fail"
	#define  VL_STRING_RANGESTATUS_SIGNAL               "Signal Fail"
	#define  VL_STRING_RANGESTATUS_MINRANGE          "Min Range Fail"
	#define  VL_STRING_RANGESTATUS_PHASE                "Phase Fail"
	#define  VL_STRING_RANGESTATUS_HW                   "Hardware Fail"


	/* Range Status */
	#define  VL_STRING_STATE_POWERDOWN               "POWERDOWN State"
	#define  VL_STRING_STATE_WAIT_STATICINIT \
			"Wait for staticinit State"
	#define  VL_STRING_STATE_STANDBY                 "STANDBY State"
	#define  VL_STRING_STATE_IDLE                    "IDLE State"
	#define  VL_STRING_STATE_RUNNING                 "RUNNING State"
	#define  VL_STRING_STATE_UNKNOWN                 "UNKNOWN State"
	#define  VL_STRING_STATE_ERROR                   "ERROR State"


	/* Device Specific */
	#define  VL_STRING_DEVICEERROR_NONE                   "No Update"
	#define  VL_STRING_DEVICEERROR_VCSELCONTINUITYTESTFAILURE \
			"VCSEL Continuity Test Failure"
	#define  VL_STRING_DEVICEERROR_VCSELWATCHDOGTESTFAILURE \
			"VCSEL Watchdog Test Failure"
	#define  VL_STRING_DEVICEERROR_NOVHVVALUEFOUND \
			"No VHV Value found"
	#define  VL_STRING_DEVICEERROR_MSRCNOTARGET \
			"MSRC No Target Error"
	#define  VL_STRING_DEVICEERROR_SNRCHECK \
			"SNR Check Exit"
	#define  VL_STRING_DEVICEERROR_RANGEPHASECHECK \
			"Range Phase Check Error"
	#define  VL_STRING_DEVICEERROR_SIGMATHRESHOLDCHECK \
			"Sigma Threshold Check Error"
	#define  VL_STRING_DEVICEERROR_TCC \
			"TCC Error"
	#define  VL_STRING_DEVICEERROR_PHASECONSISTENCY \
			"Phase Consistency Error"
	#define  VL_STRING_DEVICEERROR_MINCLIP \
			"Min Clip Error"
	#define  VL_STRING_DEVICEERROR_RANGECOMPLETE \
			"Range Complete"
	#define  VL_STRING_DEVICEERROR_ALGOUNDERFLOW \
			"Range Algo Underflow Error"
	#define  VL_STRING_DEVICEERROR_ALGOOVERFLOW \
			"Range Algo Overlow Error"
	#define  VL_STRING_DEVICEERROR_RANGEIGNORETHRESHOLD \
			"Range Ignore Threshold Error"
	#define  VL_STRING_DEVICEERROR_UNKNOWN \
			"Unknown error code"

	/* Check Enable */
	#define  VL_STRING_CHECKENABLE_SIGMA_FINAL_RANGE \
			"SIGMA FINAL RANGE"
	#define  VL_STRING_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE \
			"SIGNAL RATE FINAL RANGE"
	#define  VL_STRING_CHECKENABLE_SIGNAL_REF_CLIP \
			"SIGNAL REF CLIP"
	#define  VL_STRING_CHECKENABLE_RANGE_IGNORE_THRESHOLD \
			"RANGE IGNORE THRESHOLD"
	#define  VL_STRING_CHECKENABLE_SIGNAL_RATE_MSRC \
			"SIGNAL RATE MSRC"
	#define  VL_STRING_CHECKENABLE_SIGNAL_RATE_PRE_RANGE \
			"SIGNAL RATE PRE RANGE"

	/* Sequence Step */
	#define  VL_STRING_SEQUENCESTEP_TCC                   "TCC"
	#define  VL_STRING_SEQUENCESTEP_DSS                   "DSS"
	#define  VL_STRING_SEQUENCESTEP_MSRC                  "MSRC"
	#define  VL_STRING_SEQUENCESTEP_PRE_RANGE             "PRE RANGE"
	#define  VL_STRING_SEQUENCESTEP_FINAL_RANGE           "FINAL RANGE"
#endif /* USE_EMPTY_STRING */


#ifdef __cplusplus
}
#endif

#endif

