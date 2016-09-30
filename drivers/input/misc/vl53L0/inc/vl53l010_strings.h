/*******************************************************************************
 * Copyright © 2016, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef VL53L010_STRINGS_H_
#define VL53L010_STRINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

	#define  VL53L010_STRING_DEVICE_INFO_NAME          \
		"VL53L0 cut1.0"
	#define  VL53L010_STRING_DEVICE_INFO_NAME_TS0      \
		"VL53L0 TS0"
	#define  VL53L010_STRING_DEVICE_INFO_NAME_TS1       \
		"VL53L0 TS1"
	#define  VL53L010_STRING_DEVICE_INFO_NAME_TS2       \
		"VL53L0 TS2"
	#define  VL53L010_STRING_DEVICE_INFO_NAME_ES1       \
		"VL53L0 ES1 or later"
	#define  VL53L010_STRING_DEVICE_INFO_TYPE           \
		"VL53L0"

	/* PAL ERROR strings */
	#define  VL53L010_STRING_ERROR_NONE                 \
		"No Error"
	#define  VL53L010_STRING_ERROR_CALIBRATION_WARNING   \
		"Calibration Warning Error"
	#define  VL53L010_STRING_ERROR_MIN_CLIPPED           \
		"Min clipped error"
	#define  VL53L010_STRING_ERROR_UNDEFINED             \
		"Undefined error"
	#define  VL53L010_STRING_ERROR_INVALID_PARAMS        \
		"Invalid parameters error"
	#define  VL53L010_STRING_ERROR_NOT_SUPPORTED         \
		"Not supported error"
	#define  VL53L010_STRING_ERROR_RANGE_ERROR           \
		"Range error"
	#define  VL53L010_STRING_ERROR_TIME_OUT              \
		"Time out error"
	#define  VL53L010_STRING_ERROR_MODE_NOT_SUPPORTED     \
		"Mode not supported error"
	#define  VL53L010_STRING_ERROR_NOT_IMPLEMENTED        \
		"Not implemented error"

	#define  VL53L010_STRING_UNKNOW_ERROR_CODE            \
		"Unknow Error Code"
	#define  VL53L010_STRING_ERROR_BUFFER_TOO_SMALL       \
		"Buffer too small"

	#define  VL53L010_STRING_ERROR_GPIO_NOT_EXISTING      \
		"GPIO not existing"
	#define  VL53L010_STRING_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED   \
		"GPIO functionality not supported"
	#define  VL53L010_STRING_ERROR_CONTROL_INTERFACE                  \
		"Control Interface Error"


	/* Device Specific */
	#define  VL53L010_STRING_DEVICEERROR_NONE                         \
		"No Update"
	#define  VL53L010_STRING_DEVICEERROR_VCSELCONTINUITYTESTFAILURE   \
		"VCSEL Continuity Test Failure"
	#define  VL53L010_STRING_DEVICEERROR_VCSELWATCHDOGTESTFAILURE     \
		"VCSEL Watchdog Test Failure"
	#define  VL53L010_STRING_DEVICEERROR_NOVHVVALUEFOUND              \
		"No VHV Value found"
	#define  VL53L010_STRING_DEVICEERROR_MSRCNOTARGET                 \
		"MSRC No Target Error"
	#define  VL53L010_STRING_DEVICEERROR_MSRCMINIMUMSNR                \
		"MSRC Minimum SNR Error"
	#define  VL53L010_STRING_DEVICEERROR_MSRCWRAPAROUND                \
		"MSRC Wraparound Error"
	#define  VL53L010_STRING_DEVICEERROR_TCC                           \
		"TCC Error"
	#define  VL53L010_STRING_DEVICEERROR_RANGEAWRAPAROUND              \
		"Range A Wraparound Error"
	#define  VL53L010_STRING_DEVICEERROR_RANGEBWRAPAROUND              \
		"Range B Wraparound Error"
	#define  VL53L010_STRING_DEVICEERROR_MINCLIP                       \
		"Min Clip Error"
	#define  VL53L010_STRING_DEVICEERROR_RANGECOMPLETE                 \
		"Range Complete"
	#define  VL53L010_STRING_DEVICEERROR_ALGOUNDERFLOW                 \
		"Range Algo Underflow Error"
	#define  VL53L010_STRING_DEVICEERROR_ALGOOVERFLOW                  \
		"Range Algo Overlow Error"
	#define  VL53L010_STRING_DEVICEERROR_FINALSNRLIMIT                 \
		"Final Minimum SNR Error"
	#define  VL53L010_STRING_DEVICEERROR_NOTARGETIGNORE                \
		"No Target Ignore Error"
	#define  VL53L010_STRING_DEVICEERROR_UNKNOWN                       \
		"Unknown error code"


	/* Check Enable */
	#define  VL53L010_STRING_CHECKENABLE_SIGMA                         \
		"SIGMA"
	#define  VL53L010_STRING_CHECKENABLE_SIGNAL_RATE                    \
		"SIGNAL RATE"



#ifdef __cplusplus
}
#endif

#endif

