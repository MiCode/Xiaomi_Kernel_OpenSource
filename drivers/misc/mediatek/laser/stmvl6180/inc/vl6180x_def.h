/*******************************************************************************
Copyright ?2014, STMicroelectronics International N.V.
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
********************************************************************************/

/*
 * $Date: 2015-05-13 14:12:05 +0200 (Wed, 13 May 2015) $
 * $Revision: 2290 $
 */

/**
 * @file VL6180x_def.h
 *
 * @brief Type definitions for vl6180x api.
 *
 */


#ifndef _VL6180x_DEF
#define _VL6180x_DEF

/** API major version */
#define VL6180x_API_REV_MAJOR   3
/** API minor version */
#define VL6180x_API_REV_MINOR   0
/** API sub version */
#define VL6180x_API_REV_SUB     1

#define VL6180X_STR_HELPER(x) #x
#define VL6180X_STR(x) VL6180X_STR_HELPER(x)

#include "vl6180x_cfg.h"
#include "vl6180x_types.h"

/*
 * check configuration macro raise error or warning and suggest a default value
 */

#ifndef VL6180x_UPSCALE_SUPPORT
#error "VL6180x_UPSCALE_SUPPORT not defined"
/* TODO you must define value for  upscale support in your vl6180x_cfg.h  */
#endif

#ifndef VL6180x_ALS_SUPPORT
#error "VL6180x_ALS_SUPPORT not defined"
/* TODO you must define VL6180x_ALS_SUPPORT  with a value in your vl6180x_cfg.h  set to 0 do disable*/
#endif

#ifndef VL6180x_HAVE_DMAX_RANGING
#error "VL6180x_HAVE_DMAX_RANGING not defined"
/* TODO you may remove or comment these #error and keep the default below  or update your vl6180x_cfg.h .h file */
/**
 * force VL6180x_HAVE_DMAX_RANGING to not supported when not part of cfg file
 */
#define VL6180x_HAVE_DMAX_RANGING   0
#endif

#ifndef VL6180x_EXTENDED_RANGE
#define VL6180x_EXTENDED_RANGE   0
#endif

#ifndef  VL6180x_WRAP_AROUND_FILTER_SUPPORT
#error "VL6180x_WRAP_AROUND_FILTER_SUPPORT not defined ?"
/* TODO you may remove or comment these #error and keep the default below  or update vl6180x_cfg.h file */
/**
 * force VL6180x_WRAP_AROUND_FILTER_SUPPORT to not supported when not part of cfg file
 */
#define VL6180x_WRAP_AROUND_FILTER_SUPPORT 0
#endif




/****************************************
 * PRIVATE define do not edit
 ****************************************/

/** Maximal buffer size ever use in i2c */
#define VL6180x_MAX_I2C_XFER_SIZE   8 /* At present time it 6 byte max but that can change */

#if VL6180x_UPSCALE_SUPPORT < 0
/**
 * @def VL6180x_HAVE_UPSCALE_DATA
 * @brief  is defined if device data structure has data so when user configurable up-scale is active
 */
#define VL6180x_HAVE_UPSCALE_DATA /* have data only for user configurable up-scale config */
#endif

#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
/**
 * @def VL6180x_HAVE_WRAP_AROUND_DATA
 * @brief  is defined if device data structure has filter data so when active in cfg file
 */
#define  VL6180x_HAVE_WRAP_AROUND_DATA
#endif

#if VL6180x_ALS_SUPPORT != 0
/**
 * @def  VL6180x_HAVE_ALS_DATA
 * @brief is defined when als data are include in device data structure so when als suport if configured
 */
#define VL6180x_HAVE_ALS_DATA
#endif


#if VL6180x_WRAP_AROUND_FILTER_SUPPORT || VL6180x_HAVE_DMAX_RANGING
	#define	VL6180x_HAVE_RATE_DATA
#endif

/** Error and warning code returned by API
 *
 * negative value are true error mostly fatal\n
 * positive value  are warning most of time it's ok to continue\n
 */
enum VL6180x_ErrCode_t{
	API_NO_ERROR        = 0,
    CALIBRATION_WARNING = 1,  /*!< warning invalid calibration data may be in used \a  VL6180x_InitData() \a VL6180x_GetOffsetCalibrationData \a VL6180x_SetOffsetCalibrationData*/
    MIN_CLIPED          = 2,  /*!< warning parameter passed was clipped to min before to be applied */
    NOT_GUARANTEED      = 3,  /*!< Correct operation is not guaranteed typically using extended ranging on vl6180x */
    NOT_READY           = 4,  /*!< the data is not ready retry */

    API_ERROR      = -1,    /*!< Unqualified error */
    INVALID_PARAMS = -2,    /*!< parameter passed is invalid or out of range */
    NOT_SUPPORTED  = -3,    /*!< function is not supported in current mode or configuration */
    RANGE_ERROR    = -4,    /*!< device report a ranging error interrupt status */
    TIME_OUT       = -5,    /*!< aborted due to time out */
};

/**
 * Filtered result data structure  range data is to be used
 */
typedef struct RangeFilterResult_tag {
    uint16_t range_mm;      /*!< Filtered ranging value */
    uint16_t rawRange_mm;   /*!< raw range value (scaled) */
} RangeFilterResult_t;

/**
 * "small" unsigned data type used in filter
 *
 * if data space saving is not a concern it can be change to platform native unsigned int
 */
typedef uint8_t  FilterType1_t;

/**
 * @def FILTER_NBOF_SAMPLES
 * @brief sample history len used for wrap around filtering
 */
#define FILTER_NBOF_SAMPLES             10
/**
 * Wrap around filter internal data
 */
struct FilterData_t {
    uint32_t MeasurementIndex;                      /*!< current measurement index */
    uint16_t LastTrueRange[FILTER_NBOF_SAMPLES];    /*!< filtered/corrected  distance history */
    uint32_t LastReturnRates[FILTER_NBOF_SAMPLES];  /*!< Return rate history */
    uint16_t StdFilteredReads;                      /*!< internal use */
    FilterType1_t Default_ZeroVal;                  /*!< internal use */
    FilterType1_t Default_VAVGVal;                  /*!< internal use */
    FilterType1_t NoDelay_ZeroVal;                  /*!< internal use */
    FilterType1_t NoDelay_VAVGVal;                  /*!< internal use */
    FilterType1_t Previous_VAVGDiff;                /*!< internal use */
};

#if  VL6180x_HAVE_DMAX_RANGING
typedef int32_t DMaxFix_t;
struct DMaxData_t {
    uint32_t ambTuningWindowFactor_K; /*!<  internal algo tuning (*1000) */

    DMaxFix_t retSignalAt400mm;  /*!< intermediate dmax computation value caching @a #SYSRANGE_CROSSTALK_COMPENSATION_RATE and private reg 0x02A */
    //int32_t RegB8;              /*!< register 0xB8 cached to speed reduce i2c traffic for dmax computation */
    /* place all word data below to optimize struct packing */
    //int32_t minSignalNeeded;    /*!< optimized computation intermediate base on register cached value */
    int32_t snrLimit_K;         /*!< cached and optimized computation intermediate from  @a #SYSRANGE_MAX_AMBIENT_LEVEL_MULT */
    uint16_t ClipSnrLimit;      /*!< Max value for snr limit */
    /* place all byte data below to optimize packing */
    //uint8_t MaxConvTime;        /*!< cached max convergence time @a #SYSRANGE_MAX_CONVERGENCE_TIME*/
};
#endif

/**
 * @struct VL6180xDevData_t
 *
 * @brief Per VL6180x device St private data structure \n
 * End user should never access any of these field directly
 *
 * These must never access directly but only via VL6180xDev/SetData(dev, field) macro
 */
struct VL6180xDevData_t {

    uint32_t Part2PartAmbNVM;  /*!< backed up NVM value */
    uint32_t XTalkCompRate_KCps; /*! Cached XTlak Compensation Rate */

    uint16_t EceFactorM;        /*!< Ece Factor M numerator  */
    uint16_t EceFactorD;        /*!< Ece Factor D denominator*/

#ifdef VL6180x_HAVE_ALS_DATA
    uint16_t IntegrationPeriod; /*!< cached als Integration period avoid slow read from device at each measure */
    uint16_t AlsGainCode;       /*!< cached Als gain avoid slow read from device at each measure */
    uint16_t AlsScaler;         /*!< cached Als scaler avoid slow read from device at each measure */
#endif

#ifdef VL6180x_HAVE_UPSCALE_DATA
    uint8_t UpscaleFactor;      /*!<  up-scaling factor*/
#endif

#ifdef  VL6180x_HAVE_WRAP_AROUND_DATA
    uint8_t WrapAroundFilterActive; /*!< Filter on/off */
    struct FilterData_t FilterData; /*!< Filter internal data state history ... */
#endif

#if  VL6180x_HAVE_DMAX_RANGING
    struct DMaxData_t DMaxData;
    uint8_t DMaxEnable;
#endif
    int8_t  Part2PartOffsetNVM;     /*!< backed up NVM value */
};

#if VL6180x_SINGLE_DEVICE_DRIVER
extern  struct VL6180xDevData_t SingleVL6180xDevData;
#define VL6180xDevDataGet(dev, field) (SingleVL6180xDevData.field)
/* is also used as direct accessor like VL6180xDevDataGet(dev, x)++*/
#define VL6180xDevDataSet(dev, field, data) (SingleVL6180xDevData.field)=(data)
#endif


/**
 * @struct VL6180x_RangeData_t
 * @brief Range and any optional measurement data.
 */
typedef struct {
    int32_t range_mm;          /*!< range distance in mm. */
    int32_t signalRate_mcps;   /*!< signal rate (MCPS)\n these is a 9.7 fix point value, which is effectively a measure of target reflectance.*/
    uint32_t errorStatus;      /*!< Error status of the current measurement. \n
                                  see @a ::RangeError_u @a VL6180x_GetRangeStatusErrString() */


#ifdef VL6180x_HAVE_RATE_DATA
    uint32_t rtnAmbRate;    /*!< Return Ambient rate in KCount per sec related to \a RESULT_RANGE_RETURN_AMB_COUNT */
    uint32_t rtnRate;       /*!< Return rate in KCount per sec  related to \a RESULT_RANGE_RETURN_SIGNAL_COUNT  */
    uint32_t rtnConvTime;   /*!< Return Convergence time \a RESULT_RANGE_RETURN_CONV_TIME */
    uint32_t refConvTime;   /*!< Reference convergence time \a RESULT_RANGE_REFERENCE_CONV_TIME */
#endif


#if  VL6180x_HAVE_DMAX_RANGING
    uint32_t DMax;              /*!< DMax  when applicable */
#endif

#ifdef  VL6180x_HAVE_WRAP_AROUND_DATA
    RangeFilterResult_t FilteredData; /*!< Filter result main range_mm is updated */
#endif
}VL6180x_RangeData_t;


/** use where fix point 9.7 bit values are expected
 *
 * given a floating point value f it's .7 bit point is (int)(f*(1<<7))*/
typedef uint16_t FixPoint97_t;

/** lux data type */
typedef uint32_t lux_t;

/**
 * @brief This data type defines als  measurement data.
 */
typedef struct VL6180x_AlsData_st{
    lux_t lux;                 /**< Light measurement (Lux) */
    uint32_t errorStatus;      /**< Error status of the current measurement. \n
     * No Error := 0. \n
     * Refer to product sheets for other error codes. */
}VL6180x_AlsData_t;

/**
 * @brief Range status Error code
 *
 * @a VL6180x_GetRangeStatusErrString() if configured ( @a #VL6180x_RANGE_STATUS_ERRSTRING )
 * related to register @a #RESULT_RANGE_STATUS and additional post processing
 */
typedef enum {
    NoError_=0,                /*!< 0  0b0000 NoError  */
    VCSEL_Continuity_Test,     /*!< 1  0b0001 VCSEL_Continuity_Test */
    VCSEL_Watchdog_Test,       /*!< 2  0b0010 VCSEL_Watchdog_Test */
    VCSEL_Watchdog,            /*!< 3  0b0011 VCSEL_Watchdog */
    PLL1_Lock,                 /*!< 4  0b0100 PLL1_Lock */
    PLL2_Lock,                 /*!< 5  0b0101 PLL2_Lock */
    Early_Convergence_Estimate,/*!< 6  0b0110 Early_Convergence_Estimate */
    Max_Convergence,           /*!< 7  0b0111 Max_Convergence */
    No_Target_Ignore,          /*!< 8  0b1000 No_Target_Ignore */
    Not_used_9,                /*!< 9  0b1001 Not_used */
    Not_used_10,               /*!< 10 0b1010 Not_used_ */
    Max_Signal_To_Noise_Ratio, /*!< 11 0b1011 Max_Signal_To_Noise_Ratio*/
    Raw_Ranging_Algo_Underflow,/*!< 12 0b1100 Raw_Ranging_Algo_Underflow*/
    Raw_Ranging_Algo_Overflow, /*!< 13 0b1101 Raw_Ranging_Algo_Overflow */
    Ranging_Algo_Underflow,    /*!< 14 0b1110 Ranging_Algo_Underflow */
    Ranging_Algo_Overflow,     /*!< 15 0b1111 Ranging_Algo_Overflow */

    /* code below are addition for API/software side they are not hardware*/
    RangingFiltered =0x10,     /*!< 16 0b10000 filtered by post processing*/

} RangeError_u;


/** @defgroup device_regdef Device registers & masks definitions
 *  @brief    Device registers and masks definitions
 */

 
/** @ingroup device_regdef
 * @{*/

/**
 * The device model ID
 */
#define IDENTIFICATION_MODEL_ID                 0x000
/**
 * Revision identifier of the Device for major change.
 */
#define IDENTIFICATION_MODULE_REV_MAJOR         0x003
/**
 * Revision identifier of the Device for minor change.
 */
#define IDENTIFICATION_MODULE_REV_MINOR         0x004


/**
 * @def SYSTEM_MODE_GPIO0
 * @brief Configures polarity and select which function gpio 0 serves.
 *  Gpio0 is chip enable at power up ! Be aware of all h/w implication of turning it to output.
 *  Same definition as #SYSTEM_MODE_GPIO1
 * @ingroup device_regdef
 */
#define SYSTEM_MODE_GPIO0                       0x010
/**
 * @def SYSTEM_MODE_GPIO1
 * @brief Configures polarity and select what als or ranging functionality gpio pin serves.
 *
 * Function can be #GPIOx_SELECT_OFF  #GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT.\n
 * Same definition apply to register GPIO0 that is used as chip enable at power up.
 * @ingroup device_regdef
 */
#define SYSTEM_MODE_GPIO1                       0x011
    /** gpio pad POLARITY mask in #SYSTEM_MODE_GPIO1 (and/or 0) write  1  to set active high polarity (positive edge) */
    #define GPIOx_POLARITY_SELECT_MASK              0x20
    /** gpio pad Function select shift in #SYSTEM_MODE_GPIO1 or 0 */
    #define GPIOx_FUNCTIONALITY_SELECT_SHIFT          1
    /** gpio pad Function select mask in #SYSTEM_MODE_GPIO1 or 0 */
    #define GPIOx_FUNCTIONALITY_SELECT_MASK          (0xF<<GPIOx_FUNCTIONALITY_SELECT_SHIFT)
    /** select no interrupt in #SYSTEM_MODE_GPIO1 pad is put in  Hi-Z*/
    #define GPIOx_SELECT_OFF                        0x00
    /** select gpiox as interrupt output in  #SYSTEM_MODE_GPIO1 */
    #define GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT      0x08
    /** select range as source for interrupt on in #SYSTEM_MODE_GPIO1 */
    #define GPIOx_MODE_SELECT_RANGING               0x00
    /** select als as source for interrupt on in #SYSTEM_MODE_GPIO1 */
    #define GPIOx_MODE_SELECT_ALS                   0x01


/**
 * @def SYSTEM_INTERRUPT_CONFIG_GPIO
 *
 * @brief   Configure Als and Ranging interrupt reporting
 *
 * Possible values for Range and ALS are\n
 *
 * #CONFIG_GPIO_INTERRUPT_DISABLED\n
 * #CONFIG_GPIO_INTERRUPT_LEVEL_LOW\n
 * #CONFIG_GPIO_INTERRUPT_LEVEL_HIGH\n
 * #CONFIG_GPIO_INTERRUPT_OUT_OF_WINDOW\n
 * #CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY\n
 * Apply respective rang/als shift and mask \n
 *  #CONFIG_GPIO_RANGE_SHIFT and full reg mask #CONFIG_GPIO_RANGE_MASK\n
 *  #CONFIG_GPIO_ALS_SHIFT and full reg mask #CONFIG_GPIO_ALS_MASK\n
 *
 * \sa GPIO use for interrupt #SYSTEM_MODE_GPIO0 or #SYSTEM_MODE_GPIO1\n
 * @ingroup device_regdef
 */
#define SYSTEM_INTERRUPT_CONFIG_GPIO           0x014
    /** RANGE bits shift in #SYSTEM_INTERRUPT_CONFIG_GPIO */
    #define CONFIG_GPIO_RANGE_SHIFT            0
    /** RANGE bits mask in #SYSTEM_INTERRUPT_CONFIG_GPIO  (unshifted)*/
    #define CONFIG_GPIO_RANGE_MASK             (0x7<<CONFIG_GPIO_RANGE_SHIFT)
    /** ALS bits shift in #SYSTEM_INTERRUPT_CONFIG_GPIO */
    #define CONFIG_GPIO_ALS_SHIFT              3
    /** ALS bits mask in #SYSTEM_INTERRUPT_CONFIG_GPIO  (unshifted)*/
    #define CONFIG_GPIO_ALS_MASK               (0x7<<CONFIG_GPIO_ALS_SHIFT)
    /** interrupt is disabled */
    #define CONFIG_GPIO_INTERRUPT_DISABLED         0x00
    /** trigger when value < low threshold */
    #define CONFIG_GPIO_INTERRUPT_LEVEL_LOW        0x01
    /** trigger when value < low threshold */
    #define CONFIG_GPIO_INTERRUPT_LEVEL_HIGH       0x02
    /** trigger when outside range defined by high low threshold */
    #define CONFIG_GPIO_INTERRUPT_OUT_OF_WINDOW    0x03
    /** trigger when new sample are ready */
    #define CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY 0x04

/**
 *  @def SYSTEM_INTERRUPT_CLEAR
 *  @brief Writing to this register will clear interrupt source
 *
 *  Use or combination of any #INTERRUPT_CLEAR_RANGING , #INTERRUPT_CLEAR_ALS , #INTERRUPT_CLEAR_ERROR
 *  @ingroup device_regdef
 */
#define SYSTEM_INTERRUPT_CLEAR                0x015
    /** clear ranging interrupt in write to #SYSTEM_INTERRUPT_CLEAR */
    #define INTERRUPT_CLEAR_RANGING                0x01
    /** clear als interrupt  in write to #SYSTEM_INTERRUPT_CLEAR */
    #define INTERRUPT_CLEAR_ALS                    0x02
    /** clear error interrupt in write to #SYSTEM_INTERRUPT_CLEAR */
    #define INTERRUPT_CLEAR_ERROR                  0x04

/** After power up or reset this register will start reading 1 when device is ready */
#define SYSTEM_FRESH_OUT_OF_RESET             0x016

/**
 * @def SYSTEM_GROUPED_PARAMETER_HOLD
 * @brief Writing 1/0 activate/deactivate safe host update of multiple register in critical group \n
 *        rather use \a VL6180x_SetGroupParamHold()
 *
 * The critical register group is made of: \n
 * #SYSTEM_INTERRUPT_CONFIG_GPIO \n
 * #SYSRANGE_THRESH_HIGH \n
 * #SYSRANGE_THRESH_LOW \n
 * #SYSALS_INTEGRATION_PERIOD \n
 * #SYSALS_ANALOGUE_GAIN \n
 * #SYSALS_THRESH_HIGH \n
 * #SYSALS_THRESH_LOW
 * @ingroup device_regdef
 */
#define SYSTEM_GROUPED_PARAMETER_HOLD         0x017


/**
 * @def SYSRANGE_START
 * @brief Start/stop and set operating range mode
 *
 * Write Combination of #MODE_START_STOP  and #MODE_CONTINUOUS to select and start desired operation.
 *
 * @ingroup device_regdef
 */
#define SYSRANGE_START                        0x018
    /** mask existing bit in #SYSRANGE_START*/
    #define MODE_MASK          0x03
    /** bit 0 in #SYSRANGE_START write 1 toggle state in continuous mode and arm next shot in single shot mode */
    #define MODE_START_STOP    0x01
    /** bit 1 write 1 in #SYSRANGE_START set continuous operation mode */
    #define MODE_CONTINUOUS    0x02
    /** bit 1 write 0 in #SYSRANGE_START set single shot mode */
    #define MODE_SINGLESHOT    0x00

/**
 * @def SYSRANGE_THRESH_HIGH
 * High level range  threshold (must be scaled)
 * @ingroup device_regdef
 */
#define SYSRANGE_THRESH_HIGH                  0x019

/**
 * @def SYSRANGE_THRESH_LOW
 * Low level range  threshold (must be scaled)
 * @ingroup device_regdef
 */
#define SYSRANGE_THRESH_LOW                   0x01A

/**
 * @def SYSRANGE_INTERMEASUREMENT_PERIOD
 * @brief Continuous mode intermeasurement delay \a VL6180x_RangeSetInterMeasPeriod()
 *
 * Time delay between measurements in Ranging continuous mode.\n
 * Range 0-254 (0 = 10ms).\n Step size = 10ms.
 *
 * @ingroup device_regdef
 */
#define SYSRANGE_INTERMEASUREMENT_PERIOD      0x01B

/**
 * @brief Maximum time to run measurement in Ranging modes.
 * Range 1 - 63 ms (1 code = 1 ms);
 *
 * Measurement aborted when limit reached to aid power  reduction.\
 * For example, 0x01 = 1ms, 0x0a = 10ms.\
 * Note: Effective max_convergence_time depends on readout_averaging_sample_period setting.
 *
 * @ingroup device_regdef
 */
#define SYSRANGE_MAX_CONVERGENCE_TIME         0x01C
/**@brief Cross talk compensation rate
 * @warning  never write register directly use @a VL6180x_SetXTalkCompensationRate()
 * refer to manual for calibration procedure and computation
 * @ingroup device_regdef
 */
#define SYSRANGE_CROSSTALK_COMPENSATION_RATE  0x01E
/**
 * @brief Minimum range value in mm to qualify for crosstalk compensation
 */
#define SYSRANGE_CROSSTALK_VALID_HEIGHT       0x021
#define SYSRANGE_EARLY_CONVERGENCE_ESTIMATE   0x022
#define SYSRANGE_PART_TO_PART_RANGE_OFFSET    0x024
#define SYSRANGE_RANGE_IGNORE_VALID_HEIGHT    0x025
#define SYSRANGE_RANGE_IGNORE_THRESHOLD       0x026
#define SYSRANGE_EMITTER_BLOCK_THRESHOLD      0x028
#define SYSRANGE_MAX_AMBIENT_LEVEL_THRESH     0x02A
#define SYSRANGE_MAX_AMBIENT_LEVEL_MULT       0x02C
/** @brief  various Enable check enabel register
 *  @a VL6180x_RangeSetEceState()
 */
#define SYSRANGE_RANGE_CHECK_ENABLES          0x02D
    #define RANGE_CHECK_ECE_ENABLE_MASK      0x01
    #define RANGE_CHECK_RANGE_ENABLE_MASK    0x02
    #define RANGE_CHECK_SNR_ENABLKE          0x10

#define SYSRANGE_VHV_RECALIBRATE              0x02E
#define SYSRANGE_VHV_REPEAT_RATE              0x031

/**
 * @def SYSALS_START
 * @brief Start/stop and set operating als mode
 *
 * same bit definition as range \a #SYSRANGE_START \n
 */
#define SYSALS_START                          0x038

/** ALS low Threshold high */
#define SYSALS_THRESH_HIGH                    0x03A
/** ALS low Threshold low */
#define SYSALS_THRESH_LOW                     0x03C
/** ALS intermeasurement period */
#define SYSALS_INTERMEASUREMENT_PERIOD        0x03E
/** 
 * @warning or value with 0x40 when writing to these register*/
#define SYSALS_ANALOGUE_GAIN                  0x03F
/** ALS integration period */
#define SYSALS_INTEGRATION_PERIOD             0x040

/**
 * @brief Result range status
 *
 *  Hold the various range interrupt flags and error Specific error codes
 */
#define RESULT_RANGE_STATUS                   0x04D
    /** Device ready for new command bit 0*/
    #define RANGE_DEVICE_READY_MASK       0x01
    /** mask for error status covers bits [7:4]  in #RESULT_RANGE_STATUS @a ::RangeError_u */
    #define RANGE_ERROR_CODE_MASK         0xF0 /* */
    /** range error bit position in #RESULT_RANGE_STATUS */
    #define RANGE_ERROR_CODE_SHIFT        4

/**
 * @def RESULT_ALS_STATUS
 * @brief Result  als status \n
 *  Hold the various als interrupt flags and Specific error codes
 */
#define RESULT_ALS_STATUS                     0x4E
    /** Device ready for new command bit 0*/
   #define ALS_DEVICE_READY_MASK       0x01

/**
 * @def RESULT_ALS_VAL
 * @brief 16 Bit ALS count output value.
 *
 * Lux value depends on Gain and integration settings and calibrated lux/count setting
 * \a VL6180x_AlsGetLux() \a VL6180x_AlsGetMeasurement()
 */
#define RESULT_ALS_VAL                        0x50

/**
 * @def FW_ALS_RESULT_SCALER
 * @brief Als scaler register  Bits [3:0] analogue gain 1 to 16x
 * these register content is cached by API in \a VL6180xDevData_t::AlsScaler
 * for lux computation acceleration
 */
#define FW_ALS_RESULT_SCALER                  0x120


/**
 * these union can be use as a generic bit field type for map #RESULT_INTERRUPT_STATUS_GPIO register
 * @ingroup device_regdef
 */
typedef union IntrStatus_u{
    uint8_t val;           /*!< raw 8 bit register value*/
    struct  {
        unsigned Range     :3; /*!< Range status one of :\n  \a #RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD  \n \a #RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD  \n \a #RES_INT_STAT_GPIO_OUT_OF_WINDOW \n \a #RES_INT_STAT_GPIO_NEW_SAMPLE_READY */
        unsigned Als       :3; /*!< Als status one of: \n \a #RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD  \n \a #RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD  \n \a #RES_INT_STAT_GPIO_OUT_OF_WINDOW \n \a #RES_INT_STAT_GPIO_NEW_SAMPLE_READY  */
        unsigned Error     :2; /*!<  Error status of: \n \a #RES_INT_ERROR_LASER_SAFETY  \n \a #RES_INT_ERROR_PLL */
     } status;                 /*!< interrupt status as bit field */
} IntrStatus_t;

/**
 * @def RESULT_INTERRUPT_STATUS_GPIO
 * @brief System interrupt status report selected interrupt for als and ranging
 *
 * These register can be polled even if no gpio pins is active\n
 * What reported is selected by \a  #SYSTEM_INTERRUPT_CONFIG_GPIO \n
 * Range mask with \a #RES_INT_RANGE_MASK and shit by \a #RES_INT_RANGE_SHIFT
 * Als   mask with \a #RES_INT_ALS_MASK and shit by \a #RES_INT_ALS_SHIFT
 * Result value express condition (or combination?)
 * \a #RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD \n
 * \a #RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD \n
 * \a #RES_INT_STAT_GPIO_OUT_OF_WINDOW \n
 * \a #RES_INT_STAT_GPIO_NEW_SAMPLE_READY
 *
 * @ingroup device_regdef
 */
#define RESULT_INTERRUPT_STATUS_GPIO          0x4F
    /** ranging interrupt 1st bit position in #RESULT_INTERRUPT_STATUS_GPIO */
    #define RES_INT_RANGE_SHIFT  0
    /** ALS interrupt 1st bit position in #RESULT_INTERRUPT_STATUS_GPIO */
    #define RES_INT_ALS_SHIFT    3
    /** interrupt bit position in #RESULT_INTERRUPT_STATUS_GPIO */
    #define RES_INT_ERROR_SHIFT  6
    /** Ranging interrupt mask in #RESULT_INTERRUPT_STATUS_GPIO (prior to shift)  \sa IntrStatus_t */
    #define RES_INT_RANGE_MASK (0x7<<RES_INT_RANGE_SHIFT)
    /** als interrupt mask in #RESULT_INTERRUPT_STATUS_GPIO (prior to shift)  \sa IntrStatus_t */
    #define RES_INT_ALS_MASK   (0x7<<RES_INT_ALS_SHIFT)

    /** low threshold condition in #RESULT_INTERRUPT_STATUS_GPIO for */
    #define RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD  0x01
    /** high threshold condition in #RESULT_INTERRUPT_STATUS_GPIO for ALs or Rage*/
    #define RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD 0x02
    /** out of window condition in #RESULT_INTERRUPT_STATUS_GPIO */
    #define RES_INT_STAT_GPIO_OUT_OF_WINDOW        0x03
    /** new sample ready in #RESULT_INTERRUPT_STATUS_GPIO */
    #define RES_INT_STAT_GPIO_NEW_SAMPLE_READY     0x04
    /** error  in #RESULT_INTERRUPT_STATUS_GPIO */
    #define RES_INT_ERROR_MASK (0x3<<RES_INT_ERROR_SHIFT)
        /** laser safety error on #RES_INT_ERROR_MASK of #RESULT_INTERRUPT_STATUS_GPIO */
        #define RES_INT_ERROR_LASER_SAFETY  1
        /** pll 1 or 2 error on #RES_INT_ERROR_MASK of #RESULT_INTERRUPT_STATUS_GPIO*/
        #define RES_INT_ERROR_PLL           2

/**
 * Final range result value presented to the user for use. Unit is in mm.
 */
#define RESULT_RANGE_VAL                        0x062

/**
 * Raw Range result value with offset applied (no cross talk compensation applied). Unit is in mm.
 */
#define RESULT_RANGE_RAW                        0x064

/**
 * @brief Sensor count rate of signal returns correlated to IR emitter.
 *
 * Computed from RETURN_SIGNAL_COUNT / RETURN_CONV_TIME. Mcps 9.7 format
 */
#define RESULT_RANGE_SIGNAL_RATE                0x066

/**
 * @brief Return signal count
 *
 *  Sensor count output value attributed to signal correlated to IR emitter on the Return array.
 */
#define RESULT_RANGE_RETURN_SIGNAL_COUNT        0x06C

/**
 * @brief Reference signal count
 *
 * sensor count output value attributed to signal correlated to IR emitter on the Reference array.
 */
#define RESULT_RANGE_REFERENCE_SIGNAL_COUNT     0x070

/**
 * @brief Return ambient count
 *
 * sensor count output value attributed to uncorrelated ambient signal on the Return array.
 * Must be multiplied by 6 if used to calculate the ambient to signal threshold
 */
#define RESULT_RANGE_RETURN_AMB_COUNT           0x074

/**
 * @brief   Reference ambient count
 *
 * Sensor count output value attributed to uncorrelated ambient signal on the Reference array.
 */
#define RESULT_RANGE_REFERENCE_AMB_COUNT        0x078

/**
 * sensor count output value attributed to signal on the Return array.
 */
#define RESULT_RANGE_RETURN_CONV_TIME           0x07C

/**
 * sensor count output value attributed to signal on the Reference array.
 */
#define RESULT_RANGE_REFERENCE_CONV_TIME        0x080


/**
 * @def RANGE_SCALER
 * @brief RANGE scaling register
 *
 * Never should  user write directly onto that register directly \a VL6180x_UpscaleSetScaling()
 */
#define RANGE_SCALER                            0x096

/**
 * @def READOUT_AVERAGING_SAMPLE_PERIOD
 * @brief Readout averaging sample period register
 *
 *
 * The internal readout averaging sample period can be adjusted from 0 to 255.
 * Increasing the sampling period decreases noise but also reduces the effective
 * max convergence time and increases power consumption
 * Each unit sample period corresponds to around 64.5 ?s additional processing time.
 * The recommended setting is 48 which equates to around 4.3 ms.
 *
 * see datasheet for more detail
 */
#define READOUT_AVERAGING_SAMPLE_PERIOD     0x10A

/**
 * @def I2C_SLAVE_DEVICE_ADDRESS
 * User programmable I2C address (7-bit). Device address can be re-designated after power-up.
 * @warning What programmed in the register 7-0 are bit 8-1 of i2c address on bus (bit 0 is rd/wr)
 * so what prohamd is commonly whar ergfer as adrerss /2
 * @sa VL6180x_SetI2CAddress()
 */
#define I2C_SLAVE_DEVICE_ADDRESS               0x212

#endif /* _VL6180x_DEF */
