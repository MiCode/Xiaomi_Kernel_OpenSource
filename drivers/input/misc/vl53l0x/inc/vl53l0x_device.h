/*
 *  vl53l0x_device.h - Linux kernel modules for
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

/**
 * Device specific defines. To be adapted by implementer for the targeted
 * device.
 */

#ifndef _VL_DEVICE_H_
#define _VL_DEVICE_H_

#include "vl53l0x_types.h"


/** @defgroup VL_DevSpecDefines_group VL53L0X cut1.1
 *  Device Specific Defines
 *  @brief VL53L0X cut1.1 Device Specific Defines
 *  @{
 */


/** @defgroup uint8_t_group Device Error
 *  @brief Device Error code
 *
 *  This enum is Device specific it should be updated in the implementation
 *  Use @a VL_GetStatusErrorString() to get the string.
 *  It is related to Status Register of the Device.
 *  @{
 */

#define VL_DEVICEERROR_NONE                     ((uint8_t) 0)
	/*!< 0  NoError  */
#define VL_DEVICEERROR_VCSELCONTINUITYTESTFAILURE ((uint8_t) 1)
#define VL_DEVICEERROR_VCSELWATCHDOGTESTFAILURE ((uint8_t) 2)
#define VL_DEVICEERROR_NOVHVVALUEFOUND         ((uint8_t) 3)
#define VL_DEVICEERROR_MSRCNOTARGET            ((uint8_t) 4)
#define VL_DEVICEERROR_SNRCHECK                ((uint8_t) 5)
#define VL_DEVICEERROR_RANGEPHASECHECK         ((uint8_t) 6)
#define VL_DEVICEERROR_SIGMATHRESHOLDCHECK     ((uint8_t) 7)
#define VL_DEVICEERROR_TCC                     ((uint8_t) 8)
#define VL_DEVICEERROR_PHASECONSISTENCY        ((uint8_t) 9)
#define VL_DEVICEERROR_MINCLIP                 ((uint8_t) 10)
#define VL_DEVICEERROR_RANGECOMPLETE           ((uint8_t) 11)
#define VL_DEVICEERROR_ALGOUNDERFLOW           ((uint8_t) 12)
#define VL_DEVICEERROR_ALGOOVERFLOW            ((uint8_t) 13)
#define VL_DEVICEERROR_RANGEIGNORETHRESHOLD    ((uint8_t) 14)

/** @} end of uint8_t_group */


/** @defgroup VL_CheckEnable_group Check Enable list
 *  @brief Check Enable code
 *
 *  Define used to specify the LimitCheckId.
 *  Use @a VL_GetLimitCheckInfo() to get the string.
 *  @{
 */

#define VL_CHECKENABLE_SIGMA_FINAL_RANGE           0
#define VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE     1
#define VL_CHECKENABLE_SIGNAL_REF_CLIP             2
#define VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD      3
#define VL_CHECKENABLE_SIGNAL_RATE_MSRC            4
#define VL_CHECKENABLE_SIGNAL_RATE_PRE_RANGE       5

#define VL_CHECKENABLE_NUMBER_OF_CHECKS            6

/** @}  end of VL_CheckEnable_group */


/** @defgroup uint8_t_group Gpio Functionality
 *  @brief Defines the different functionalities for the device GPIO(s)
 *  @{
 */

#define VL_GPIOFUNCTIONALITY_OFF                     \
	((uint8_t)  0) /*!< NO Interrupt  */
#define VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW   \
	((uint8_t)  1) /*!< Level Low (value < thresh_low) */
#define VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH   \
	((uint8_t)  2)/*!< Level High (value > thresh_high)*/
#define VL_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT    \
	((uint8_t)  3)
	/*!< Out Of Window (value < thresh_low OR value > thresh_high)  */
#define VL_GPIOFUNCTIONALITY_NEW_MEASURE_READY        \
	((uint8_t)  4) /*!< New Sample Ready  */

/** @} end of uint8_t_group */


/* Device register map */

/** @defgroup VL_DefineRegisters_group Define Registers
 *  @brief List of all the defined registers
 *  @{
 */
#define VL_REG_SYSRANGE_START                        0x000
	/** mask existing bit in #VL_REG_SYSRANGE_START*/
	#define VL_REG_SYSRANGE_MODE_MASK          0x0F
	/** bit 0 in #VL_REG_SYSRANGE_START write 1 toggle state in */
	/* continuous mode and arm next shot in single shot mode */
	#define VL_REG_SYSRANGE_MODE_START_STOP    0x01
	/** bit 1 write 0 in #VL_REG_SYSRANGE_START set single shot mode */
	#define VL_REG_SYSRANGE_MODE_SINGLESHOT    0x00
	/** bit 1 write 1 in #VL_REG_SYSRANGE_START set back-to-back */
	/*  operation mode */
	#define VL_REG_SYSRANGE_MODE_BACKTOBACK    0x02
	/** bit 2 write 1 in #VL_REG_SYSRANGE_START set timed operation */
	/* *  mode */
	#define VL_REG_SYSRANGE_MODE_TIMED         0x04
	/** bit 3 write 1 in #VL_REG_SYSRANGE_START set histogram operation */
	/* *  mode */
	#define VL_REG_SYSRANGE_MODE_HISTOGRAM     0x08


#define VL_REG_SYSTEM_THRESH_HIGH               0x000C
#define VL_REG_SYSTEM_THRESH_LOW                0x000E


#define VL_REG_SYSTEM_SEQUENCE_CONFIG		0x0001
#define VL_REG_SYSTEM_RANGE_CONFIG			0x0009
#define VL_REG_SYSTEM_INTERMEASUREMENT_PERIOD	0x0004


#define VL_REG_SYSTEM_INTERRUPT_CONFIG_GPIO               0x000A
	#define VL_REG_SYSTEM_INTERRUPT_GPIO_DISABLED	0x00
	#define VL_REG_SYSTEM_INTERRUPT_GPIO_LEVEL_LOW	0x01
	#define VL_REG_SYSTEM_INTERRUPT_GPIO_LEVEL_HIGH	0x02
	#define VL_REG_SYSTEM_INTERRUPT_GPIO_OUT_OF_WINDOW	0x03
	#define VL_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY	0x04

#define VL_REG_GPIO_HV_MUX_ACTIVE_HIGH          0x0084


#define VL_REG_SYSTEM_INTERRUPT_CLEAR           0x000B

/* Result registers */
#define VL_REG_RESULT_INTERRUPT_STATUS          0x0013
#define VL_REG_RESULT_RANGE_STATUS              0x0014

#define VL_REG_RESULT_CORE_PAGE  1
#define VL_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN   0x00BC
#define VL_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_RTN    0x00C0
#define VL_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF   0x00D0
#define VL_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_REF    0x00D4
#define VL_REG_RESULT_PEAK_SIGNAL_RATE_REF             0x00B6

/* Algo register */

#define VL_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM       0x0028

#define VL_REG_I2C_SLAVE_DEVICE_ADDRESS                0x008a

/* Check Limit registers */
#define VL_REG_MSRC_CONFIG_CONTROL                     0x0060

#define VL_REG_PRE_RANGE_CONFIG_MIN_SNR                      0X0027
#define VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW              0x0056
#define VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH             0x0057
#define VL_REG_PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT            0x0064

#define VL_REG_FINAL_RANGE_CONFIG_MIN_SNR                    0X0067
#define VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW            0x0047
#define VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH           0x0048
#define VL_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT   0x0044


#define VL_REG_PRE_RANGE_CONFIG_SIGMA_THRESH_HI              0X0061
#define VL_REG_PRE_RANGE_CONFIG_SIGMA_THRESH_LO              0X0062

/* PRE RANGE registers */
#define VL_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD                 0x0050
#define VL_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI            0x0051
#define VL_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO            0x0052

#define VL_REG_SYSTEM_HISTOGRAM_BIN                          0x0081
#define VL_REG_HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT         0x0033
#define VL_REG_HISTOGRAM_CONFIG_READOUT_CTRL                 0x0055

#define VL_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD               0x0070
#define VL_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI          0x0071
#define VL_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO          0x0072
#define VL_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS         0x0020

#define VL_REG_MSRC_CONFIG_TIMEOUT_MACROP                    0x0046


#define VL_REG_SOFT_RESET_GO2_SOFT_RESET_N	                 0x00bf
#define VL_REG_IDENTIFICATION_MODEL_ID                       0x00c0
#define VL_REG_IDENTIFICATION_REVISION_ID                    0x00c2

#define VL_REG_OSC_CALIBRATE_VAL                             0x00f8


#define VL_SIGMA_ESTIMATE_MAX_VALUE                          65535
/* equivalent to a range sigma of 655.35mm */

#define VL_REG_GLOBAL_CONFIG_VCSEL_WIDTH          0x032
#define VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0   0x0B0
#define VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_1   0x0B1
#define VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_2   0x0B2
#define VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_3   0x0B3
#define VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_4   0x0B4
#define VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_5   0x0B5

#define VL_REG_GLOBAL_CONFIG_REF_EN_START_SELECT   0xB6
#define VL_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD 0x4E /* 0x14E */
#define VL_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET    0x4F /* 0x14F */
#define VL_REG_POWER_MANAGEMENT_GO1_POWER_FORCE    0x80

/*
 * Speed of light in um per 1E-10 Seconds
 */

#define VL_SPEED_OF_LIGHT_IN_AIR 2997

#define VL_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV     0x0089

#define VL_REG_ALGO_PHASECAL_LIM                         0x0030 /* 0x130 */
#define VL_REG_ALGO_PHASECAL_CONFIG_TIMEOUT              0x0030

/** @} VL_DefineRegisters_group */

/** @} VL_DevSpecDefines_group */


#endif

/* _VL_DEVICE_H_ */


