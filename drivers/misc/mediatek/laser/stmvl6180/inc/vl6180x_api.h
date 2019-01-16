/*******************************************************************************
Copyright 2014, STMicroelectronics International N.V.
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
 * @file VL6180x_api.h
 * $Date: 2015-03-24 14:15:14 +0100 (Tue, 24 Mar 2015) $
 * $Revision: 2213 $
 */



#ifndef VL6180x_API_H_
#define VL6180x_API_H_

#include "vl6180x_def.h"
#include "vl6180x_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup api_ll API Low Level Functions
 *  @brief    API Low level functions
 */

/** @defgroup api_hl API High Level Functions
 *  @brief    API High level functions
 */
 

/*
 * Check and set default platform dependent configuration
 */
#ifndef  VL6180x_SINGLE_DEVICE_DRIVER
#error "VL6180x_SINGLE_DEVICE_DRIVER not defined"
/* TODO you may remove or comment these #error but it is best you update your vl6180x_platform.h file to define it*/
#endif


#ifndef  VL6180x_RANGE_STATUS_ERRSTRING
#warning "VL6180x_RANGE_STATUS_ERRSTRING not defined ?"
/* TODO you may remove or comment these #warning and keep the default below to keep compatibility
   or update your vl6180x_platform.h file */
/**
 * force VL6180x_RANGE_STATUS_ERRSTRING to not supported when not part of any cfg file
 */
#define VL6180x_RANGE_STATUS_ERRSTRING  0
#endif

#ifndef  VL6180X_SAFE_POLLING_ENTER
#warning "VL6180X_SAFE_POLLING_ENTER not defined, likely old vl6180x_cfg.h file ?"
/* TODO you may remove or comment these #warning and keep the default below to keep compatibility
   or update your vl6180x_platform.h file */
/**
 * force VL6180X_SAFE_POLLING_ENTER to off when not in cfg file
 */
#define VL6180X_SAFE_POLLING_ENTER 0 /* off by default as in api 2.0 */
#endif

#ifndef VL6180X_LOG_ENABLE
/**
 * Force VL6180X_LOG_ENABLE to none as default
 */
#define VL6180X_LOG_ENABLE  0
#endif

#if VL6180x_RANGE_STATUS_ERRSTRING
/**@def VL6180x_HAVE_RANGE_STATUS_ERRSTRING
 * @brief is defined when @a #VL6180x_RANGE_STATUS_ERRSTRING is enable
 */
#define  VL6180x_HAVE_RANGE_STATUS_ERRSTRING
#endif


/** @brief Get API version as "hex integer" 0xMMnnss
 */
#define VL6180x_ApiRevInt  ((VL6180x_API_REV_MAJOR<<24)+(VL6180x_API_REV_MINOR<<16)+VL6180x_API_REV_SUB)

/** Get API version as string for exe "2.1.12" "
 */
#define VL6180x_ApiRevStr  VL6180X_STR(VL6180x_API_REV_MAJOR) "." VL6180X_STR(VL6180x_API_REV_MINOR) "." VL6180X_STR(VL6180x_API_REV_SUB)

/** @defgroup api_init Init functions
 *  @brief    API init functions
 *  @ingroup api_hl
 *  @{  
 */
/**
 * @brief Wait for device booted after chip enable (hardware standby)
 * @par Function Description
 * After Chip enable Application you can also simply wait at least 1ms to ensure device is ready
 * @warning After device chip enable (gpio0) de-asserted  user must wait gpio1 to get asserted (hardware standby).
 * or wait at least 400usec prior to do any low level access or api call .
 *
 * This function implements polling for standby but you must ensure 400usec from chip enable passed\n
 * @warning if device get prepared @a VL6180x_Prepare() re-using these function can hold indefinitely\n
 *
 * @param dev  The device
 * @return     0 on success
 */
int VL6180x_WaitDeviceBooted(VL6180xDev_t dev);

/**
 *
 * @brief One time device initialization
 *
 * To be called once and only once after device is brought out of reset (Chip enable) and booted see @a VL6180x_WaitDeviceBooted()
 *
 * @par Function Description
 * When not used after a fresh device "power up" or reset, it may return @a #CALIBRATION_WARNING
 * meaning wrong calibration data may have been fetched from device that can result in ranging offset error\n
 * If application cannot execute device reset or need to run VL6180x_InitData  multiple time
 * then it  must ensure proper offset calibration saving and restore on its own
 * by using @a VL6180x_GetOffsetCalibrationData() on first power up and then @a VL6180x_SetOffsetCalibrationData() all all subsequent init
 *
 * @param dev  The device
 * @return     0 on success,  @a #CALIBRATION_WARNING if failed
 */
int VL6180x_InitData(VL6180xDev_t dev );

/**
 * @brief Configure GPIO1 function and set polarity.
 * @par Function Description
 * To be used prior to arm single shot measure or start  continuous mode.
 *
 * The function uses @a VL6180x_SetupGPIOx() for setting gpio 1.
 * @warning  changing polarity can generate a spurious interrupt on pins.
 * It sets an interrupt flags condition that must be cleared to avoid polling hangs. \n
 * It is safe to run VL6180x_ClearAllInterrupt() just after.
 *
 * @param dev           The device
 * @param IntFunction   The interrupt functionality to use one of :\n
 *  @a #GPIOx_SELECT_OFF \n
 *  @a #GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT
 * @param ActiveHigh  The interrupt line polarity see ::IntrPol_e
 *      use @a #INTR_POL_LOW (falling edge) or @a #INTR_POL_HIGH (rising edge)
 * @return 0 on success
 */
int VL6180x_SetupGPIO1(VL6180xDev_t dev, uint8_t IntFunction, int ActiveHigh);

 /**
  * @brief  Prepare device for operation
  * @par Function Description
  * Does static initialization and reprogram common default settings \n
  * Device is prepared for new measure, ready single shot ranging or ALS typical polling operation\n
  * After prepare user can : \n
  * @li Call other API function to set other settings\n
  * @li Configure the interrupt pins, etc... \n
  * @li Then start ranging or ALS operations in single shot or continuous mode
  *
  * @param dev   The device
  * @return      0 on success
  */
 int VL6180x_Prepare(VL6180xDev_t dev);
 
 /** @}  */
 

/** @defgroup api_hl_range Ranging functions
 *  @brief    Ranging functions
 *  @ingroup api_hl
 *  @{  
 */
 
 /**
 * @brief Start continuous ranging mode
 *
 * @details End user should ensure device is in idle state and not already running
 */
int VL6180x_RangeStartContinuousMode(VL6180xDev_t dev);

/**
 * @brief Start single shot ranging measure
 *
 * @details End user should ensure device is in idle state and not already running
 */
int VL6180x_RangeStartSingleShot(VL6180xDev_t dev);

/**
 * @brief Set maximum convergence time
 *
 * @par Function Description
 * Setting a low convergence time can impact maximal detectable distance.
 * Refer to VL6180x Datasheet Table 7 : Typical range convergence time.
 * A typical value for up to x3 scaling is 50 ms
 *
 * @param dev
 * @param MaxConTime_msec
 * @return 0 on success. <0 on error. >0 for calibration warning status
 */
int VL6180x_RangeSetMaxConvergenceTime(VL6180xDev_t dev, uint8_t  MaxConTime_msec);
 
/**
  * @brief Single shot Range measurement in polling mode.
  *
  * @par Function Description
  * Kick off a new single shot range  then wait for ready to retrieve it by polling interrupt status \n
  * Ranging must be prepared by a first call to  @a VL6180x_Prepare() and it is safer to clear  very first poll call \n
  * This function reference VL6180x_PollDelay(dev) porting macro/call on each polling loop,
  * but PollDelay(dev) may never be called if measure in ready on first poll loop \n
  * Should not be use in continuous mode operation as it will stop it and cause stop/start misbehaviour \n
  * \n This function clears Range Interrupt status , but not error one. For that uses  @a VL6180x_ClearErrorInterrupt() \n
  * This range error is not related VL6180x_RangeData_t::errorStatus that refer measure status \n
  * 
  * @param dev          The device
  * @param pRangeData   Will be populated with the result ranging data @a  VL6180x_RangeData_t
  * @return 0 on success , @a #RANGE_ERROR if device reports an error case in it status (not cleared) use
  *
  * \sa ::VL6180x_RangeData_t
  */
int VL6180x_RangePollMeasurement(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);

/**
 * @brief Check for measure readiness and get it if ready
 *
 * @par Function Description
 * Using this function is an alternative to @a VL6180x_RangePollMeasurement() to avoid polling operation. This is suitable for applications
 * where host CPU is triggered on a interrupt (not from VL6180X) to perform ranging operation. In this scenario, we assume that the very first ranging
 * operation is triggered by a call to @a VL6180x_RangeStartSingleShot(). Then, host CPU regularly calls @a VL6180x_RangeGetMeasurementIfReady() to
 * get a distance measure if ready. In case the distance is not ready, host may get it at the next call.\n
 *
 * @warning 
 * This function does not re-start a new measurement : this is up to the host CPU to do it.\n 
 * This function clears Range Interrupt for measure ready , but not error interrupts. For that, uses  @a VL6180x_ClearErrorInterrupt() \n
 *
 * @param dev  The device
 * @param pRangeData  Will be populated with the result ranging data if available
 * @return  0 when measure is ready pRange data is updated (untouched when not ready),  >0 for warning and @a #NOT_READY if measurement not yet ready, <0 for error @a #RANGE_ERROR if device report an error,
 */
int VL6180x_RangeGetMeasurementIfReady(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);

/**
 * @brief Retrieve range measurements set  from device
 *
 * @par Function Description
 * The measurement is made of range_mm status and error code @a VL6180x_RangeData_t \n
 * Based on configuration selected extra measures are included.
 *
 * @warning should not be used in continuous if wrap around filter is active \n
 * Does not perform any wait nor check for result availability or validity.
 *\sa VL6180x_RangeGetResult for "range only" measurement
 *
 * @param dev         The device
 * @param pRangeData  Pointer to the data structure to fill up
 * @return            0 on success
 */
int VL6180x_RangeGetMeasurement(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);

/**
 * @brief Get ranging result and only that
 *
 * @par Function Description
 * Unlike @a VL6180x_RangeGetMeasurement() this function only retrieves the range in millimeter \n
 * It does any required up-scale translation\n
 * It can be called after success status polling or in interrupt mode \n
 * @warning these function is not doing wrap around filtering \n
 * This function doesn't perform any data ready check!
 *
 * @param dev        The device
 * @param pRange_mm  Pointer to range distance
 * @return           0 on success
 */
int VL6180x_RangeGetResult(VL6180xDev_t dev, int32_t *pRange_mm);

/**
 * @brief Configure ranging interrupt reported to application
 *
 * @param dev            The device
 * @param ConfigGpioInt  Select ranging report\n select one (and only one) of:\n
 *   @a #CONFIG_GPIO_INTERRUPT_DISABLED \n
 *   @a #CONFIG_GPIO_INTERRUPT_LEVEL_LOW \n
 *   @a #CONFIG_GPIO_INTERRUPT_LEVEL_HIGH \n
 *   @a #CONFIG_GPIO_INTERRUPT_OUT_OF_WINDOW \n
 *   @a #CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY
 * @return   0 on success
 */
int VL6180x_RangeConfigInterrupt(VL6180xDev_t dev, uint8_t ConfigGpioInt);


/**
 * @brief Clear range interrupt
 *
 * @param dev    The device
 * @return  0    On success
 */
#define VL6180x_RangeClearInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_RANGING)

/**
 * @brief Return ranging error interrupt status
 *
 * @par Function Description
 * Appropriate Interrupt report must have been selected first by @a VL6180x_RangeConfigInterrupt() or @a  VL6180x_Prepare() \n
 *
 * Can be used in polling loop to wait for a given ranging event or in interrupt to read the trigger \n
 * Events triggers are : \n
 * @a #RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD \n
 * @a #RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD \n
 * @a #RES_INT_STAT_GPIO_OUT_OF_WINDOW \n (RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD|RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD)
 * @a #RES_INT_STAT_GPIO_NEW_SAMPLE_READY \n
 *
 * @sa IntrStatus_t
 * @param dev        The device
 * @param pIntStatus Pointer to status variable to update
 * @return           0 on success
 */
int VL6180x_RangeGetInterruptStatus(VL6180xDev_t dev, uint8_t *pIntStatus);

#if VL6180x_RANGE_STATUS_ERRSTRING

extern const char * ROMABLE_DATA VL6180x_RangeStatusErrString[];
/**
 * @brief Human readable error string for range error status
 *
 * @param RangeErrCode  The error code as stored on @a VL6180x_RangeData_t::errorStatus
 * @return  error string , NULL for invalid RangeErrCode
 * @sa ::RangeError_u
 */
const char * VL6180x_RangeGetStatusErrString(uint8_t RangeErrCode);
#else
#define VL6180x_RangeGetStatusErrString(...) NULL
#endif

/** @}  */

#if VL6180x_ALS_SUPPORT

/** @defgroup api_hl_als ALS functions
 *  @brief    ALS functions
 *  @ingroup api_hl
 *  @{  
 */

/**
 * @brief   Run a single ALS measurement in single shot polling mode
 *
 * @par Function Description
 * Kick off a new single shot ALS then wait new measurement ready to retrieve it ( polling system interrupt status register for als) \n
 * ALS must be prepared by a first call to @a VL6180x_Prepare() \n
 * \n Should not be used in continuous or interrupt mode it will break it and create hazard in start/stop \n
 *
 * @param dev          The device
 * @param pAlsData     Als data structure to fill up @a VL6180x_AlsData_t
 * @return             0 on success
 */
int VL6180x_AlsPollMeasurement(VL6180xDev_t dev, VL6180x_AlsData_t *pAlsData);


/**
 * @brief  Get actual ALS measurement
 *
 * @par Function Description
 * Can be called after success status polling or in interrupt mode to retrieve ALS measurement from device \n
 * This function doesn't perform any data ready check !
 *
 * @param dev        The device
 * @param pAlsData   Pointer to measurement struct @a VL6180x_AlsData_t
 * @return  0 on success
 */
int VL6180x_AlsGetMeasurement(VL6180xDev_t dev, VL6180x_AlsData_t *pAlsData);

/**
 * @brief  Configure ALS interrupts provide to application
 *
 * @param dev            The Device
 * @param ConfigGpioInt  Select one (and only one) of : \n
 *   @a #CONFIG_GPIO_INTERRUPT_DISABLED \n
 *   @a #CONFIG_GPIO_INTERRUPT_LEVEL_LOW \n
 *   @a #CONFIG_GPIO_INTERRUPT_LEVEL_HIGH \n
 *   @a #CONFIG_GPIO_INTERRUPT_OUT_OF_WINDOW \n
 *   @a #CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY
 * @return               0 on success may return #INVALID_PARAMS for invalid mode
 */
int VL6180x_AlsConfigInterrupt(VL6180xDev_t dev, uint8_t ConfigGpioInt);


/**
 * @brief Set ALS integration period
 *
 * @param dev        The device
 * @param period_ms  Integration period in msec. Value in between 50 to 100 msec is recommended\n
 * @return           0 on success
 */
int VL6180x_AlsSetIntegrationPeriod(VL6180xDev_t dev, uint16_t period_ms);

/**
 * @brief Set ALS "inter-measurement period"
 *
 * @par Function Description
 * The so call data-sheet "inter measurement period" is actually an extra inter-measurement delay
 *
 * @param dev        The device
 * @param intermeasurement_period_ms Inter measurement time in milli second\n
 *        @warning applied value is clipped to 2550 ms\n
 * @return           0 on success if value is
 */
int VL6180x_AlsSetInterMeasurementPeriod(VL6180xDev_t dev,  uint16_t intermeasurement_period_ms);

/**
 * @brief Set ALS analog gain code
 *
 * @par Function Description
 * ALS gain code value programmed in @a SYSALS_ANALOGUE_GAIN .
 * @param dev   The device
 * @param gain  Gain code see datasheet or AlsGainLookUp for real value. Value is clipped to 7.
 * @return  0 on success
 */
 
int VL6180x_AlsSetAnalogueGain(VL6180xDev_t dev, uint8_t gain);
/**
 * @brief Set thresholds for ALS continuous mode 
 * @warning Threshold are raw device value not lux!
 *
 * @par Function Description
 * Basically value programmed in @a SYSALS_THRESH_LOW and @a SYSALS_THRESH_HIGH registers
 * @param dev   The device
 * @param low   ALS low raw threshold for @a SYSALS_THRESH_LOW
 * @param high  ALS high raw threshold for  @a SYSALS_THRESH_HIGH
 * @return  0 on success
 */
int VL6180x_AlsSetThresholds(VL6180xDev_t dev, uint8_t low, uint8_t high);

/**
 * @brief  Clear ALS interrupt
 *
 * @param dev    The device
 * @return  0    On success
 */
 #define VL6180x_AlsClearInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_ALS)
 
/**
 * Read ALS interrupt status
 * @param dev         Device
 * @param pIntStatus  Pointer to status
 * @return            0 on success
 */
int VL6180x_AlsGetInterruptStatus(VL6180xDev_t dev, uint8_t *pIntStatus);
 
/** @}  */
#endif

/** @defgroup api_ll_init Init functions
 *  @brief    Init functions
 *  @ingroup api_ll
 *  @{  
 */
 
/**
 * @brief Low level ranging and ALS register static settings (you should call @a VL6180x_Prepare() function instead)
 *
 * @param dev
 * @return 0 on success
 */
int VL6180x_StaticInit(VL6180xDev_t dev);
 
 /** @}  */
 
/** @defgroup api_ll_range Ranging functions
 *  @brief    Ranging Low Level functions
 *  @ingroup api_ll
 *  @{  
 */

/**
 * @brief Wait for device to be ready (before a new ranging command can be issued by application)
 * @param dev        The device
 * @param MaxLoop    Max Number of i2c polling loop see @a #msec_2_i2cloop
 * @return           0 on success. <0 when fail \n
 *                   @ref VL6180x_ErrCode_t::TIME_OUT for time out \n
 *                   @ref VL6180x_ErrCode_t::INVALID_PARAMS if MaxLop<1
 */
int VL6180x_RangeWaitDeviceReady(VL6180xDev_t dev, int MaxLoop );

/**
 * @brief Program Inter measurement period (used only in continuous mode)
 *
 * @par Function Description
 * When trying to set too long time, it returns #INVALID_PARAMS
 *
 * @param dev     The device
 * @param InterMeasTime_msec Requires inter-measurement time in msec
 * @return 0 on success
 */
int VL6180x_RangeSetInterMeasPeriod(VL6180xDev_t dev, uint32_t  InterMeasTime_msec);


/**
 * @brief Set device ranging scaling factor
 *
 * @par Function Description
 * The ranging scaling factor is applied on the raw distance measured by the device to increase operating ranging at the price of the precision.
 * Changing the scaling factor when device is not in f/w standby state (free running) is not safe.
 * It can be source of spurious interrupt, wrongly scaled range etc ...
 * @warning __This  function doesns't update high/low threshold and other programmed settings linked to scaling factor__.
 *  To ensure proper operation, threshold and scaling changes should be done following this procedure: \n
 *  @li Set Group hold  : @a VL6180x_SetGroupParamHold() \n
 *  @li Get Threshold @a VL6180x_RangeGetThresholds() \n
 *  @li Change scaling : @a VL6180x_UpscaleSetScaling() \n
 *  @li Set Threshold : @a VL6180x_RangeSetThresholds() \n
 *  @li Unset Group Hold : @a VL6180x_SetGroupParamHold()
 *
 * @param dev      The device
 * @param scaling  Scaling factor to apply (1,2 or 3)
 * @return          0 on success when up-scale support is not configured it fail for any
 *                  scaling than the one statically configured.
 */
int VL6180x_UpscaleSetScaling(VL6180xDev_t dev, uint8_t scaling);

/**
 * @brief Get current ranging scaling factor
 *
 * @param dev The device
 * @return    The current scaling factor
 */
int VL6180x_UpscaleGetScaling(VL6180xDev_t dev);


/**
 * @brief Give filtered state (wrap-around filter) of a range measurement
 * @param pRangeData  Range measurement data
 * @return  0 means measure was not filtered,  when not 0 range from device got filtered by filter post processing
 */
#define VL6180x_RangeIsFilteredMeasurement(pRangeData) ((pRangeData)->errorStatus == RangingFiltered)

/**
 * @brief Get the maximal distance for actual scaling
 * @par Function Description
 * Do not use prior to @a VL6180x_Prepare() or at least @a VL6180x_InitData()
 *
 * Any range value more than the value returned by this function is to be considered as "no target detected"
 * or "no target in detectable range" \n
 * @warning The maximal distance depends on the scaling
 *
 * @param dev The device
 * @return    The maximal range limit for actual mode and scaling
 */
uint16_t VL6180x_GetUpperLimit(VL6180xDev_t dev);

/**
 * @brief Apply low and high ranging thresholds that are considered only in continuous mode
 *
 * @par Function Description
 * This function programs low and high ranging thresholds that are considered in continuous mode : 
 * interrupt will be raised only when an object is detected at a distance inside this [low:high] range.  
 * The function takes care of applying current scaling factor if any.\n
 * To be safe, in continuous operation, thresholds must be changed under "group parameter hold" cover.
 * Group hold can be activated/deactivated directly in the function or externally (then set 0)
 * using /a VL6180x_SetGroupParamHold() function.
 *
 * @param dev      The device
 * @param low      Low threshold in mm
 * @param high     High threshold in mm
 * @param SafeHold  Use of group parameters hold to surround threshold programming.
 * @return  0 On success
 */
int VL6180x_RangeSetThresholds(VL6180xDev_t dev, uint16_t low, uint16_t high, int SafeHold);

/**
 * @brief  Get scaled high and low threshold from device
 *
 * @par Function Description
 * Due to scaling factor, the returned value may be different from what has been programmed first (precision lost).
 * For instance VL6180x_RangeSetThresholds(dev,11,22) with scale 3
 * will read back 9 ((11/3)x3) and 21 ((22/3)x3).

 * @param dev  The device
 * @param low  scaled low Threshold ptr  can be NULL if not needed
 * @param high scaled High Threshold ptr can be NULL if not needed
 * @return 0 on success, return value is undefined if both low and high are NULL
 * @warning return value is undefined if both low and high are NULL
 */
int VL6180x_RangeGetThresholds(VL6180xDev_t dev, uint16_t *low, uint16_t *high);

/**
 * @brief Set ranging raw thresholds (scaling not considered so not recommended to use it)
 *
 * @param dev  The device
 * @param low  raw low threshold set to raw register
 * @param high raw high threshold set to raw  register
 * @return 0 on success
 */
int VL6180x_RangeSetRawThresholds(VL6180xDev_t dev, uint8_t low, uint8_t high);

/**
 * @brief Set Early Convergence Estimate ratio
 * @par Function Description
 * For more information on ECE check datasheet
 * @warning May return a calibration warning in some use cases
 *
 * @param dev        The device
 * @param FactorM    ECE factor M in M/D
 * @param FactorD    ECE factor D in M/D
 * @return           0 on success. <0 on error. >0 on warning
 */
int VL6180x_RangeSetEceFactor(VL6180xDev_t dev, uint16_t  FactorM, uint16_t FactorD);

/**
 * @brief Set Early Convergence Estimate state (See #SYSRANGE_RANGE_CHECK_ENABLES register)
 * @param dev       The device
 * @param enable    State to be set 0=disabled, otherwise enabled
 * @return          0 on success
 */
int VL6180x_RangeSetEceState(VL6180xDev_t dev, int enable );

/**
 * @brief Set activation state of the wrap around filter
 * @param dev   The device
 * @param state New activation state (0=off,  otherwise on)
 * @return      0 on success
 */
int VL6180x_FilterSetState(VL6180xDev_t dev, int state);

/**
 * Get activation state of the wrap around filter
 * @param dev  The device
 * @return     Filter enabled or not, when filter is not supported it always returns 0S
 */
int VL6180x_FilterGetState(VL6180xDev_t dev);


/**
 * @brief Set activation state of  DMax computation
 * @param dev   The device
 * @param state New activation state (0=off,  otherwise on)
 * @return      0 on success
 */
int VL6180x_DMaxSetState(VL6180xDev_t dev, int state);

/**
 * Get activation state of DMax computation
 * @param dev  The device
 * @return     Filter enabled or not, when filter is not supported it always returns 0S
 */
int VL6180x_DMaxGetState(VL6180xDev_t dev);


/**
 * @brief Set ranging mode and start/stop measure (use high level functions instead : @a VL6180x_RangeStartSingleShot() or @a VL6180x_RangeStartContinuousMode())
 *
 * @par Function Description
 * When used outside scope of known polling single shot stopped state, \n
 * user must ensure the device state is "idle" before to issue a new command.
 *
 * @param dev   The device
 * @param mode  A combination of working mode (#MODE_SINGLESHOT or #MODE_CONTINUOUS) and start/stop condition (#MODE_START_STOP) \n
 * @return      0 on success
 */
int VL6180x_RangeSetSystemMode(VL6180xDev_t dev, uint8_t mode);
 
/** @}  */ 

/** @defgroup api_ll_range_calibration Ranging calibration functions
 *  @brief    Ranging calibration functions
 *  @ingroup api_ll
 *  @{  
 */
/**
 * @brief Get part to part calibration offset
 *
 * @par Function Description
 * Should only be used after a successful call to @a VL6180x_InitData to backup device nvm value
 *
 * @param dev  The device
 * @return part to part calibration offset from device
 */
int8_t VL6180x_GetOffsetCalibrationData(VL6180xDev_t dev);

/**
 * Set or over-write part to part calibration offset
 * \sa VL6180x_InitData(), VL6180x_GetOffsetCalibrationData()
 * @param dev     The device
 * @param offset   Offset
 */
void VL6180x_SetOffsetCalibrationData(VL6180xDev_t dev, int8_t offset);

/**
 * @brief Set Cross talk compensation rate
 *
 * @par Function Description
 * It programs register @a #SYSRANGE_CROSSTALK_COMPENSATION_RATE
 *
 * @param dev  The device
 * @param Rate Compensation rate (9.7 fix point) see datasheet for details
 * @return     0 on success
 */
int  VL6180x_SetXTalkCompensationRate(VL6180xDev_t dev, FixPoint97_t Rate);

/** @}  */



#if VL6180x_ALS_SUPPORT
/** @defgroup api_ll_als ALS functions
 *  @brief    ALS functions
 *  @ingroup api_ll
 *  @{  
 */

/**
 * @brief Wait for device to be ready for new als operation or max pollign loop (time out)
 * @param dev        The device
 * @param MaxLoop    Max Number of i2c polling loop see @a #msec_2_i2cloop
 * @return           0 on success. <0 when @a VL6180x_ErrCode_t::TIME_OUT if timed out
 */
int VL6180x_AlsWaitDeviceReady(VL6180xDev_t dev, int MaxLoop );

/**
 * @brief Set ALS system mode and start/stop measure
 *
 * @warning When used outside after single shot polling, \n
 * User must ensure  the device state is ready before issuing a new command (using @a VL6180x_AlsWaitDeviceReady()). \n
 * Non respect of this, can cause loss of interrupt or device hanging.
 *
 * @param dev   The device
 * @param mode  A combination of working mode (#MODE_SINGLESHOT or #MODE_CONTINUOUS) and start condition (#MODE_START_STOP) \n
 * @return      0 on success
 */
int VL6180x_AlsSetSystemMode(VL6180xDev_t dev, uint8_t mode); 

/** @}  */ 
#endif 

/** @defgroup api_ll_misc Misc functions
 *  @brief    Misc functions
 *  @ingroup api_ll
 *  @{  
 */

/**
 * Set Group parameter Hold state
 *
 * @par Function Description
 * Group parameter holds @a #SYSTEM_GROUPED_PARAMETER_HOLD enable safe update (non atomic across multiple measure) by host
 * \n The critical register group is composed of: \n
 * #SYSTEM_INTERRUPT_CONFIG_GPIO \n
 * #SYSRANGE_THRESH_HIGH \n
 * #SYSRANGE_THRESH_LOW \n
 * #SYSALS_INTEGRATION_PERIOD \n
 * #SYSALS_ANALOGUE_GAIN \n
 * #SYSALS_THRESH_HIGH \n
 * #SYSALS_THRESH_LOW
 *
 *
 * @param dev   The device
 * @param Hold  Group parameter Hold state to be set (on/off)
 * @return      0 on success
 */
int VL6180x_SetGroupParamHold(VL6180xDev_t dev, int Hold);

/**
 * @brief Set new device i2c address
 *
 * After completion the device will answer to the new address programmed.
 *
 * @sa AN4478: Using multiple VL6180X's in a single design
 * @param dev       The device
 * @param NewAddr   The new i2c address (7bit)
 * @return          0 on success
 */
int VL6180x_SetI2CAddress(VL6180xDev_t dev, uint8_t NewAddr);

/**
 * @brief Fully configure gpio 0/1 pin : polarity and functionality
 *
 * @param dev          The device
 * @param pin          gpio pin 0 or 1
 * @param IntFunction  Pin functionality : either #GPIOx_SELECT_OFF or #GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT (refer to #SYSTEM_MODE_GPIO1 register definition)
 * @param ActiveHigh   Set active high polarity, or active low see @a ::IntrPol_e
 * @return             0 on success
 */
int VL6180x_SetupGPIOx(VL6180xDev_t dev, int pin, uint8_t IntFunction, int ActiveHigh);


/**
 * @brief Set interrupt pin polarity for the given GPIO
 *
 * @param dev          The device
 * @param pin          Pin 0 or 1
 * @param active_high  select active high or low polarity using @ref IntrPol_e
 * @return             0 on success
 */
int VL6180x_SetGPIOxPolarity(VL6180xDev_t dev, int pin, int active_high);

/**
 * Select interrupt functionality for the given GPIO
 *
 * @par Function Description
 * Functionality refer to @a SYSTEM_MODE_GPIO0
 *
 * @param dev            The device
 * @param pin            Pin to configure 0 or 1 (gpio0 or gpio1)\nNote that gpio0 is chip enable at power up !
 * @param functionality  Pin functionality : either #GPIOx_SELECT_OFF or #GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT (refer to #SYSTEM_MODE_GPIO1 register definition)
 * @return              0 on success
 */
int VL6180x_SetGPIOxFunctionality(VL6180xDev_t dev, int pin, uint8_t functionality);

/**
 * #brief Disable and turn to Hi-Z gpio output pin
 *
 * @param dev  The device
 * @param pin  The pin number to disable 0 or 1
 * @return     0 on success
 */
int VL6180x_DisableGPIOxOut(VL6180xDev_t dev, int pin);

/**
 * @def msec_2_i2cloop
 * @brief  Number of I2C polling loop (an 8 bit register) to run for maximal wait time.
 *
 * @par Function Description
 * When polling via I2C the overall time is mainly the I2C transaction time because it is a slow bus
 *   one 8 bit register poll on I2C bus timing is shown below: \n
 *   start + addr_w(a)  + 2x8bit index(a)  + stop  + start  + addr_rd(a)  + 1x8bit data_rd(a) + stop \n
 *   1        8   1        2*(8+1)           1       1          8     1        8           1    1 \n
 *  so 49 serial bits
 *
 * @param  time_ms  Time to wait in milli second 10
 * @param  i2c_khz  I2C bus frequencies in KHz  for instance 400
 * @return The number of loops (at least 1)
 */
#define msec_2_i2cloop( time_ms, i2c_khz )  (((time_ms)*(i2c_khz)/49)+1)

/** @}  */



/**
 * polarity use in @a VL6180x_SetupGPIOx() , @a VL6180x_SetupGPIO1()
 */
typedef enum  {
    INTR_POL_LOW        =0, /*!< set active low polarity best setup for falling edge */
    INTR_POL_HIGH       =1, /*!< set active high polarity best setup for rising edge */
}IntrPol_e;

/** @defgroup api_ll_intr Interrupts management functions
 *  @brief    Interrupts management functions
 *  @ingroup api_ll
 *  @{  
 */

/**
 * @brief     Get all interrupts cause
 *
 * @param dev    The device
 * @param status Ptr to interrupt status. You can use @a IntrStatus_t::val
 * @return 0 on success
 */
int VL6180x_GetInterruptStatus(VL6180xDev_t dev, uint8_t *status);

/**
 * @brief Clear given system interrupt condition
 *
 * @par Function Description
 * Clear given interrupt cause by writing into register #SYSTEM_INTERRUPT_CLEAR register.
 * @param dev       The device 
 * @param IntClear  Which interrupt source to clear. Use any combinations of #INTERRUPT_CLEAR_RANGING , #INTERRUPT_CLEAR_ALS , #INTERRUPT_CLEAR_ERROR.
 * @return  0       On success
 */
int VL6180x_ClearInterrupt(VL6180xDev_t dev, uint8_t IntClear );

/**
 * @brief Clear error interrupt
 *
 * @param dev    The device
 * @return  0    On success
 */
 #define VL6180x_ClearErrorInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_ERROR)

/**
 * @brief Clear All interrupt causes (als+range+error)
 *
 * @param dev    The device
 * @return  0    On success
 */
#define VL6180x_ClearAllInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_ERROR|INTERRUPT_CLEAR_RANGING|INTERRUPT_CLEAR_ALS)

/** @}  */


/** @defgroup api_reg API Register access functions
 *  @brief    Registers access functions called by API core functions
 *  @ingroup api_ll
 *  @{  
 */

/**
 * Write VL6180x single byte register
 * @param dev   The device
 * @param index The register index
 * @param data  8 bit register data
 * @return success
 */
int VL6180x_WrByte(VL6180xDev_t dev, uint16_t index, uint8_t data);
/**
 * Thread safe VL6180x Update (rd/modify/write) single byte register
 *
 * Final_reg = (Initial_reg & and_data) |or_data
 *
 * @param dev   The device
 * @param index The register index
 * @param AndData  8 bit and data
 * @param OrData   8 bit or data
 * @return 0 on success
 */
int VL6180x_UpdateByte(VL6180xDev_t dev, uint16_t index, uint8_t AndData, uint8_t OrData);
/**
 * Write VL6180x word register
 * @param dev   The device
 * @param index The register index
 * @param data  16 bit register data
 * @return  0 on success
 */
int VL6180x_WrWord(VL6180xDev_t dev, uint16_t index, uint16_t data);
/**
 * Write VL6180x double word (4 byte) register
 * @param dev   The device
 * @param index The register index
 * @param data  32 bit register data
 * @return  0 on success
 */
int VL6180x_WrDWord(VL6180xDev_t dev, uint16_t index, uint32_t data);

/**
 * Read VL6180x single byte register
 * @param dev   The device
 * @param index The register index
 * @param data  pointer to 8 bit data
 * @return 0 on success
 */
int VL6180x_RdByte(VL6180xDev_t dev, uint16_t index, uint8_t *data);

/**
 * Read VL6180x word (2byte) register
 * @param dev   The device
 * @param index The register index
 * @param data  pointer to 16 bit data
 * @return 0 on success
 */
int VL6180x_RdWord(VL6180xDev_t dev, uint16_t index, uint16_t *data);

/**
 * Read VL6180x dword (4byte) register
 * @param dev   The device
 * @param index The register index
 * @param data  pointer to 32 bit data
 * @return 0 on success
 */
int VL6180x_RdDWord(VL6180xDev_t dev, uint16_t index, uint32_t *data);

/** @}  */




#ifdef __cplusplus
}
#endif

#endif /* VL6180x_API_H_ */
