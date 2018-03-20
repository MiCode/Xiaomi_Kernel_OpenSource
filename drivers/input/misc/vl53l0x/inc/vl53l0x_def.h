/*
 *  vl53l0x_def.h - Linux kernel modules for
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
 * @file VL_def.h
 *
 * @brief Type definitions for VL53L0X API.
 *
 */


#ifndef _VL_DEF_H_
#define _VL_DEF_H_


#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup VL_globaldefine_group VL53L0X Defines
 *	@brief	  VL53L0X Defines
 *	@{
 */


/** PAL SPECIFICATION major version */
#define VL53L0X10_SPECIFICATION_VER_MAJOR   1
/** PAL SPECIFICATION minor version */
#define VL53L0X10_SPECIFICATION_VER_MINOR   2
/** PAL SPECIFICATION sub version */
#define VL53L0X10_SPECIFICATION_VER_SUB	   7
/** PAL SPECIFICATION sub version */
#define VL53L0X10_SPECIFICATION_VER_REVISION 1440

/** VL53L0X PAL IMPLEMENTATION major version */
#define VL53L0X10_IMPLEMENTATION_VER_MAJOR	1
/** VL53L0X PAL IMPLEMENTATION minor version */
#define VL53L0X10_IMPLEMENTATION_VER_MINOR	0
/** VL53L0X PAL IMPLEMENTATION sub version */
#define VL53L0X10_IMPLEMENTATION_VER_SUB		9
/** VL53L0X PAL IMPLEMENTATION sub version */
#define VL53L0X10_IMPLEMENTATION_VER_REVISION	3673

/** PAL SPECIFICATION major version */
#define VL_SPECIFICATION_VER_MAJOR	 1
/** PAL SPECIFICATION minor version */
#define VL_SPECIFICATION_VER_MINOR	 2
/** PAL SPECIFICATION sub version */
#define VL_SPECIFICATION_VER_SUB	 7
/** PAL SPECIFICATION sub version */
#define VL_SPECIFICATION_VER_REVISION 1440

/** VL53L0X PAL IMPLEMENTATION major version */
#define VL_IMPLEMENTATION_VER_MAJOR	  1
/** VL53L0X PAL IMPLEMENTATION minor version */
#define VL_IMPLEMENTATION_VER_MINOR	  0
/** VL53L0X PAL IMPLEMENTATION sub version */
#define VL_IMPLEMENTATION_VER_SUB	  2
/** VL53L0X PAL IMPLEMENTATION sub version */
#define VL_IMPLEMENTATION_VER_REVISION	  4823
#define VL_DEFAULT_MAX_LOOP 2000
#define VL_MAX_STRING_LENGTH 32


#include "vl53l0x_device.h"
#include "vl53l0x_types.h"


/****************************************
 * PRIVATE define do not edit
 ****************************************/

/** @brief Defines the parameters of the Get Version Functions
 */
struct VL_Version_t {
	uint32_t	 revision; /*!< revision number */
	uint8_t		 major;	   /*!< major number */
	uint8_t		 minor;	   /*!< minor number */
	uint8_t		 build;	   /*!< build number */
};


/** @brief Defines the parameters of the Get Device Info Functions
 */
struct VL_DeviceInfo_t {
	char Name[VL_MAX_STRING_LENGTH];
		/*!< Name of the Device e.g. Left_Distance */
	char Type[VL_MAX_STRING_LENGTH];
		/*!< Type of the Device e.g VL53L0X */
	char ProductId[VL_MAX_STRING_LENGTH];
		/*!< Product Identifier String	*/
	uint8_t ProductType;
		/*!< Product Type, VL53L0X = 1, VL53L1 = 2 */
	uint8_t ProductRevisionMajor;
		/*!< Product revision major */
	uint8_t ProductRevisionMinor;
		/*!< Product revision minor */
};


/** @defgroup VL_define_Error_group Error and Warning code returned by API
 *	The following DEFINE are used to identify the PAL ERROR
 *	@{
 */

#define VL_ERROR_NONE		((int8_t)	0)
#define VL_ERROR_CALIBRATION_WARNING	((int8_t) -1)
	/*!< Warning invalid calibration data may be in used*/
		/*\a VL_InitData()*/
		/*\a VL_GetOffsetCalibrationData*/
		/*\a VL_SetOffsetCalibrationData */
#define VL_ERROR_MIN_CLIPPED			((int8_t) -2)
	/*!< Warning parameter passed was clipped to min before to be applied */

#define VL_ERROR_UNDEFINED				((int8_t) -3)
	/*!< Unqualified error */
#define VL_ERROR_INVALID_PARAMS			((int8_t) -4)
	/*!< Parameter passed is invalid or out of range */
#define VL_ERROR_NOT_SUPPORTED			((int8_t) -5)
	/*!< Function is not supported in current mode or configuration */
#define VL_ERROR_RANGE_ERROR			((int8_t) -6)
	/*!< Device report a ranging error interrupt status */
#define VL_ERROR_TIME_OUT				((int8_t) -7)
	/*!< Aborted due to time out */
#define VL_ERROR_MODE_NOT_SUPPORTED		((int8_t) -8)
	/*!< Asked mode is not supported by the device */
#define VL_ERROR_BUFFER_TOO_SMALL			((int8_t) -9)
	/*!< ... */
#define VL_ERROR_GPIO_NOT_EXISTING			((int8_t) -10)
	/*!< User tried to setup a non-existing GPIO pin */
#define VL_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED  ((int8_t) -11)
	/*!< unsupported GPIO functionality */
#define VL_ERROR_INTERRUPT_NOT_CLEARED		((int8_t) -12)
	/*!< Error during interrupt clear */
#define VL_ERROR_CONTROL_INTERFACE			((int8_t) -20)
	/*!< error reported from IO functions */
#define VL_ERROR_INVALID_COMMAND			((int8_t) -30)
	/*!< The command is not allowed in the current device state*/
	/* *	(power down) */
#define VL_ERROR_DIVISION_BY_ZERO			((int8_t) -40)
	/*!< In the function a division by zero occurs */
#define VL_ERROR_REF_SPAD_INIT			((int8_t) -50)
	/*!< Error during reference SPAD initialization */
#define VL_ERROR_NOT_IMPLEMENTED			((int8_t) -99)
	/*!< Tells requested functionality has not been implemented yet or*/
	/* * not compatible with the device */
/** @} VL_define_Error_group */


/** @defgroup VL_define_DeviceModes_group Defines Device modes
 *	Defines all possible modes for the device
 *	@{
 */

#define VL_DEVICEMODE_SINGLE_RANGING	((uint8_t)  0)
#define VL_DEVICEMODE_CONTINUOUS_RANGING	((uint8_t)  1)
#define VL_DEVICEMODE_SINGLE_HISTOGRAM	((uint8_t)  2)
#define VL_DEVICEMODE_CONTINUOUS_TIMED_RANGING ((uint8_t) 3)
#define VL_DEVICEMODE_SINGLE_ALS		((uint8_t) 10)
#define VL_DEVICEMODE_GPIO_DRIVE		((uint8_t) 20)
#define VL_DEVICEMODE_GPIO_OSC		((uint8_t) 21)
	/* ... Modes to be added depending on device */
/** @} VL_define_DeviceModes_group */



/** @defgroup VL_define_HistogramModes_group Defines Histogram modes
 *	Defines all possible Histogram modes for the device
 *	@{
 */

#define VL_HISTOGRAMMODE_DISABLED		((uint8_t) 0)
	/*!< Histogram Disabled */
#define VL_HISTOGRAMMODE_REFERENCE_ONLY	((uint8_t) 1)
	/*!< Histogram Reference array only */
#define VL_HISTOGRAMMODE_RETURN_ONLY	((uint8_t) 2)
	/*!< Histogram Return array only */
#define VL_HISTOGRAMMODE_BOTH		((uint8_t) 3)
	/*!< Histogram both Reference and Return Arrays */
	/* ... Modes to be added depending on device */
/** @} VL_define_HistogramModes_group */


/** @defgroup VL_define_PowerModes_group List of available Power Modes
 *	List of available Power Modes
 *	@{
 */

#define VL_POWERMODE_STANDBY_LEVEL1 ((uint8_t) 0)
	/*!< Standby level 1 */
#define VL_POWERMODE_STANDBY_LEVEL2 ((uint8_t) 1)
	/*!< Standby level 2 */
#define VL_POWERMODE_IDLE_LEVEL1	((uint8_t) 2)
	/*!< Idle level 1 */
#define VL_POWERMODE_IDLE_LEVEL2	((uint8_t) 3)
	/*!< Idle level 2 */

/** @} VL_define_PowerModes_group */


/** @brief Defines all parameters for the device
 */
struct VL_DeviceParameters_t {
	uint8_t DeviceMode;
	/*!< Defines type of measurement to be done for the next measure */
	uint8_t HistogramMode;
	/*!< Defines type of histogram measurement to be done for the next*/
	/* *	measure */
	uint32_t MeasurementTimingBudgetMicroSeconds;
	/*!< Defines the allowed total time for a single measurement */
	uint32_t InterMeasurementPeriodMilliSeconds;
	/*!< Defines time between two consecutive measurements (between two*/
	/* *	measurement starts). If set to 0 means back-to-back mode */
	uint8_t XTalkCompensationEnable;
	/*!< Tells if Crosstalk compensation shall be enable or not	 */
	uint16_t XTalkCompensationRangeMilliMeter;
	/*!< CrossTalk compensation range in millimeter	 */
	unsigned int XTalkCompensationRateMegaCps;
	/*!< CrossTalk compensation rate in Mega counts per seconds.*/
	/* *	Expressed in 16.16 fixed point format.	*/
	int32_t RangeOffsetMicroMeters;
	/*!< Range offset adjustment (mm).	*/

	uint8_t LimitChecksEnable[VL_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Limit Check enable for this device. */
	uint8_t LimitChecksStatus[VL_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Status of the check linked to last*/
	/** measurement. */
	unsigned int LimitChecksValue[VL_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Limit Check value for this device */

	uint8_t WrapAroundCheckEnable;
	/*!< Tells if Wrap Around Check shall be enable or not */
};


/** @defgroup VL_define_State_group Defines the current status
 *	of the device Defines the current status of the device
 *	@{
 */

#define VL_STATE_POWERDOWN		 ((uint8_t)  0)
	/*!< Device is in HW reset	*/
#define VL_STATE_WAIT_STATICINIT ((uint8_t)  1)
	/*!< Device is initialized and wait for static initialization  */
#define VL_STATE_STANDBY		 ((uint8_t)  2)
	/*!< Device is in Low power Standby mode   */
#define VL_STATE_IDLE			 ((uint8_t)  3)
	/*!< Device has been initialized and ready to do measurements  */
#define VL_STATE_RUNNING		 ((uint8_t)  4)
	/*!< Device is performing measurement */
#define VL_STATE_UNKNOWN		 ((uint8_t)  98)
	/*!< Device is in unknown state and need to be rebooted	 */
#define VL_STATE_ERROR			 ((uint8_t)  99)
	/*!< Device is in error state and need to be rebooted  */

/** @} VL_define_State_group */


/** @brief Structure containing the Dmax computation parameters and data
 */
struct VL_DMaxData_t {
	int32_t AmbTuningWindowFactor_K;
		/*!<  internal algo tuning (*1000) */
	int32_t RetSignalAt0mm;
		/*!< intermediate dmax computation value caching */
};

/**
 * @struct VL_RangeData_t
 * @brief Range measurement data.
 */
struct VL_RangingMeasurementData_t {
	uint32_t TimeStamp;		/*!< 32-bit time stamp. */
	uint32_t MeasurementTimeUsec;
		/*!< Give the Measurement time needed by the device to do the */
		/** measurement.*/


	uint16_t RangeMilliMeter;	/*!< range distance in millimeter. */

	uint16_t RangeDMaxMilliMeter;
		/*!< Tells what is the maximum detection distance of */
		/* the device */
		/* * in current setup and environment conditions (Filled when */
		/* *	applicable) */

	unsigned int SignalRateRtnMegaCps;
		/*!< Return signal rate (MCPS)\n these is a 16.16 fix point */
		/* *	value, which is effectively a measure of target */
		/* *	 reflectance.*/
	unsigned int AmbientRateRtnMegaCps;
		/*!< Return ambient rate (MCPS)\n these is a 16.16 fix point */
		/* *	value, which is effectively a measure of the ambien */
		/* *	t light.*/

	uint16_t EffectiveSpadRtnCount;
		/*!< Return the effective SPAD count for the return signal. */
		/* *	To obtain Real value it should be divided by 256 */

	uint8_t ZoneId;
		/*!< Denotes which zone and range scheduler stage the range */
		/* *	data relates to. */
	uint8_t RangeFractionalPart;
		/*!< Fractional part of range distance. Final value is a */
		/* *	FixPoint168 value. */
	uint8_t RangeStatus;
		/*!< Range Status for the current measurement. This is device */
		/* *	dependent. Value = 0 means value is valid. */
		/* *	See \ref RangeStatusPage */
};


#define VL_HISTOGRAM_BUFFER_SIZE 24

/**
 * @struct VL_HistogramData_t
 * @brief Histogram measurement data.
 */
struct VL_HistogramMeasurementData_t {
	/* Histogram Measurement data */
	uint32_t HistogramData[VL_HISTOGRAM_BUFFER_SIZE];
	/*!< Histogram data */
	uint8_t HistogramType; /*!< Indicate the types of histogram data : */
	/*Return only, Reference only, both Return and Reference */
	uint8_t FirstBin; /*!< First Bin value */
	uint8_t BufferSize; /*!< Buffer Size - Set by the user.*/
	uint8_t NumberOfBins;
	/*!< Number of bins filled by the histogram measurement */

	uint8_t ErrorStatus;
	/*!< Error status of the current measurement. \n */
	/* see @a ::uint8_t @a VL_GetStatusErrorString() */
};

#define VL_REF_SPAD_BUFFER_SIZE 6

/**
 * @struct VL_SpadData_t
 * @brief Spad Configuration Data.
 */
struct VL_SpadData_t {
	uint8_t RefSpadEnables[VL_REF_SPAD_BUFFER_SIZE];
	/*!< Reference Spad Enables */
	uint8_t RefGoodSpadMap[VL_REF_SPAD_BUFFER_SIZE];
	/*!< Reference Spad Good Spad Map */
};

struct VL_DeviceSpecificParameters_t {
	unsigned int OscFrequencyMHz; /* Frequency used */

	uint16_t LastEncodedTimeout;
	/* last encoded Time out used for timing budget*/

	uint8_t Pin0GpioFunctionality;
	/* store the functionality of the GPIO: pin0 */

	uint32_t FinalRangeTimeoutMicroSecs;
	 /*!< Execution time of the final range*/
	uint8_t FinalRangeVcselPulsePeriod;
	 /*!< Vcsel pulse period (pll clocks) for the final range measurement*/
	uint32_t PreRangeTimeoutMicroSecs;
	 /*!< Execution time of the final range*/
	uint8_t PreRangeVcselPulsePeriod;
	 /*!< Vcsel pulse period (pll clocks) for the pre-range measurement*/

	uint16_t SigmaEstRefArray;
	 /*!< Reference array sigma value in 1/100th of [mm] e.g. 100 = 1mm */
	uint16_t SigmaEstEffPulseWidth;
	 /*!< Effective Pulse width for sigma estimate in 1/100th */
	 /* * of ns e.g. 900 = 9.0ns */
	uint16_t SigmaEstEffAmbWidth;
	 /*!< Effective Ambient width for sigma estimate in 1/100th of ns */
	 /* * e.g. 500 = 5.0ns */


	uint8_t ReadDataFromDeviceDone; /* Indicate if read from device has */
	/*been done (==1) or not (==0) */
	uint8_t ModuleId; /* Module ID */
	uint8_t Revision; /* test Revision */
	char ProductId[VL_MAX_STRING_LENGTH];
		/* Product Identifier String  */
	uint8_t ReferenceSpadCount; /* used for ref spad management */
	uint8_t ReferenceSpadType;	/* used for ref spad management */
	uint8_t RefSpadsInitialised; /* reports if ref spads are initialised. */
	uint32_t PartUIDUpper; /*!< Unique Part ID Upper */
	uint32_t PartUIDLower; /*!< Unique Part ID Lower */
	unsigned int SignalRateMeasFixed400mm; /*!< Peek Signal rate at 400 mm*/

};

/**
 * @struct VL_DevData_t
 *
 * @brief VL53L0X PAL device ST private data structure \n
 * End user should never access any of these field directly
 *
 * These must never access directly but only via macro
 */
struct VL_DevData_t {
	struct VL_DMaxData_t DMaxData;
	/*!< Dmax Data */
	int32_t	 Part2PartOffsetNVMMicroMeter;
	/*!< backed up NVM value */
	int32_t	 Part2PartOffsetAdjustmentNVMMicroMeter;
	/*!< backed up NVM value representing additional offset adjustment */
	struct VL_DeviceParameters_t CurrentParameters;
	/*!< Current Device Parameter */
	struct VL_RangingMeasurementData_t LastRangeMeasure;
	/*!< Ranging Data */
	struct VL_HistogramMeasurementData_t LastHistogramMeasure;
	/*!< Histogram Data */
	struct VL_DeviceSpecificParameters_t DeviceSpecificParameters;
	/*!< Parameters specific to the device */
	struct VL_SpadData_t SpadData;
	/*!< Spad Data */
	uint8_t SequenceConfig;
	/*!< Internal value for the sequence config */
	uint8_t RangeFractionalEnable;
	/*!< Enable/Disable fractional part of ranging data */
	uint8_t PalState;
	/*!< Current state of the PAL for this device */
	uint8_t PowerMode;
	/*!< Current Power Mode	 */
	uint16_t SigmaEstRefArray;
	/*!< Reference array sigma value in 1/100th of [mm] e.g. 100 = 1mm */
	uint16_t SigmaEstEffPulseWidth;
	/*!< Effective Pulse width for sigma estimate in 1/100th */
	/* of ns e.g. 900 = 9.0ns */
	uint16_t SigmaEstEffAmbWidth;
	/*!< Effective Ambient width for sigma estimate in 1/100th of ns */
	/* * e.g. 500 = 5.0ns */
	uint8_t StopVariable;
	/*!< StopVariable used during the stop sequence */
	uint16_t targetRefRate;
	/*!< Target Ambient Rate for Ref spad management */
	unsigned int SigmaEstimate;
	/*!< Sigma Estimate - based on ambient & VCSEL rates and */
	/** signal_total_events */
	unsigned int SignalEstimate;
	/*!< Signal Estimate - based on ambient & VCSEL rates and cross talk */
	unsigned int LastSignalRefMcps;
	/*!< Latest Signal ref in Mcps */
	uint8_t *pTuningSettingsPointer;
	/*!< Pointer for Tuning Settings table */
	uint8_t UseInternalTuningSettings;
	/*!< Indicate if we use	 Tuning Settings table */
	uint16_t LinearityCorrectiveGain;
	/*!< Linearity Corrective Gain value in x1000 */
	uint16_t DmaxCalRangeMilliMeter;
	/*!< Dmax Calibration Range millimeter */
	unsigned int DmaxCalSignalRateRtnMegaCps;
	/*!< Dmax Calibration Signal Rate Return MegaCps */

};


/** @defgroup VL_define_InterruptPolarity_group Defines the Polarity
 * of the Interrupt
 *	Defines the Polarity of the Interrupt
 *	@{
 */

#define VL_INTERRUPTPOLARITY_LOW	   ((uint8_t)	0)
/*!< Set active low polarity best setup for falling edge. */
#define VL_INTERRUPTPOLARITY_HIGH	   ((uint8_t)	1)
/*!< Set active high polarity best setup for rising edge. */

/** @} VL_define_InterruptPolarity_group */


/** @defgroup VL_define_VcselPeriod_group Vcsel Period Defines
 *	Defines the range measurement for which to access the vcsel period.
 *	@{
 */

#define VL_VCSEL_PERIOD_PRE_RANGE	((uint8_t) 0)
/*!<Identifies the pre-range vcsel period. */
#define VL_VCSEL_PERIOD_FINAL_RANGE ((uint8_t) 1)
/*!<Identifies the final range vcsel period. */

/** @} VL_define_VcselPeriod_group */

/** @defgroup VL_define_SchedulerSequence_group Defines the steps
 * carried out by the scheduler during a range measurement.
 *	@{
 *	Defines the states of all the steps in the scheduler
 *	i.e. enabled/disabled.
 */
struct VL_SchedulerSequenceSteps_t {
	uint8_t		 TccOn;	   /*!<Reports if Target Centre Check On  */
	uint8_t		 MsrcOn;	   /*!<Reports if MSRC On  */
	uint8_t		 DssOn;		   /*!<Reports if DSS On  */
	uint8_t		 PreRangeOn;   /*!<Reports if Pre-Range On	*/
	uint8_t		 FinalRangeOn; /*!<Reports if Final-Range On  */
};

/** @} VL_define_SchedulerSequence_group */

/** @defgroup VL_define_SequenceStepId_group Defines the Polarity
 *	of the Interrupt
 *	Defines the the sequence steps performed during ranging..
 *	@{
 */

#define	 VL_SEQUENCESTEP_TCC		 ((uint8_t) 0)
/*!<Target CentreCheck identifier. */
#define	 VL_SEQUENCESTEP_DSS		 ((uint8_t) 1)
/*!<Dynamic Spad Selection function Identifier. */
#define	 VL_SEQUENCESTEP_MSRC		 ((uint8_t) 2)
/*!<Minimum Signal Rate Check function Identifier. */
#define	 VL_SEQUENCESTEP_PRE_RANGE	 ((uint8_t) 3)
/*!<Pre-Range check Identifier. */
#define	 VL_SEQUENCESTEP_FINAL_RANGE ((uint8_t) 4)
/*!<Final Range Check Identifier. */

#define	 VL_SEQUENCESTEP_NUMBER_OF_CHECKS			 5
/*!<Number of Sequence Step Managed by the API. */

/** @} VL_define_SequenceStepId_group */


/* MACRO Definitions */
/** @defgroup VL_define_GeneralMacro_group General Macro Defines
 *	General Macro Defines
 *	@{
 */

/* Defines */
#define VL_SETPARAMETERFIELD(Dev, field, value) \
	PALDevDataSet(Dev, CurrentParameters.field, value)

#define VL_GETPARAMETERFIELD(Dev, field, variable) \
	(variable = PALDevDataGet(Dev, CurrentParameters).field)


#define VL_SETARRAYPARAMETERFIELD(Dev, field, index, value) \
	PALDevDataSet(Dev, CurrentParameters.field[index], value)

#define VL_GETARRAYPARAMETERFIELD(Dev, field, index, variable) \
	(variable = PALDevDataGet(Dev, CurrentParameters).field[index])


#define VL_SETDEVICESPECIFICPARAMETER(Dev, field, value) \
		PALDevDataSet(Dev, DeviceSpecificParameters.field, value)

#define VL_GETDEVICESPECIFICPARAMETER(Dev, field) \
		PALDevDataGet(Dev, DeviceSpecificParameters).field


#define VL_FIXPOINT1616TOFIXPOINT97(Value) \
	(uint16_t)((Value>>9)&0xFFFF)
#define VL_FIXPOINT97TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<9)

#define VL_FIXPOINT1616TOFIXPOINT88(Value) \
	(uint16_t)((Value>>8)&0xFFFF)
#define VL_FIXPOINT88TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<8)

#define VL_FIXPOINT1616TOFIXPOINT412(Value) \
	(uint16_t)((Value>>4)&0xFFFF)
#define VL_FIXPOINT412TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<4)

#define VL_FIXPOINT1616TOFIXPOINT313(Value) \
	(uint16_t)((Value>>3)&0xFFFF)
#define VL_FIXPOINT313TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<3)

#define VL_FIXPOINT1616TOFIXPOINT08(Value) \
	(uint8_t)((Value>>8)&0x00FF)
#define VL_FIXPOINT08TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<8)

#define VL_FIXPOINT1616TOFIXPOINT53(Value) \
	(uint8_t)((Value>>13)&0x00FF)
#define VL_FIXPOINT53TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<13)

#define VL_FIXPOINT1616TOFIXPOINT102(Value) \
	(uint16_t)((Value>>14)&0x0FFF)
#define VL_FIXPOINT102TOFIXPOINT1616(Value) \
	(unsigned int)(Value<<12)

#define VL_MAKEUINT16(lsb, msb) (uint16_t)((((uint16_t)msb)<<8) + \
		(uint16_t)lsb)

/** @} VL_define_GeneralMacro_group */

/** @} VL_globaldefine_group */







#ifdef __cplusplus
}
#endif


#endif /* _VL_DEF_H_ */
