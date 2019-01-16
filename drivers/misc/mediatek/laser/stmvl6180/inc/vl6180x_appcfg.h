
/*******************************************************************************
Copyright © 2014, STMicroelectronics International N.V.
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
 * vl6180x_appcfg.h
 *
 */

#ifndef VL6180X_APPCFG_H_
#define VL6180X_APPCFG_H_


/**
 * @def VL6180x_SINGLE_DEVICE_DRIVER
 * @brief enable lightweight single vl6180x device driver
 *
 * value 1 =>  single device capable
 * Configure optimized APi for single device driver with static data and minimal use of ref pointer \n
 * limited to single device driver or application in non multi thread/core environment \n
 *
 * value 0 =>  multiple device capable user must review "device" structure and type in porting files
 * @ingroup Configuration
 */
#define VL6180x_SINGLE_DEVICE_DRIVER 1


/**
 * @def VL6180x_RANGE_STATUS_ERRSTRING
 * @brief when define include range status Error string and related
 *
 * The string table lookup require some space in read only area
 * @ingroup Configuration
 */
#define VL6180x_RANGE_STATUS_ERRSTRING  1

/**
 * @def VL6180X_SAFE_POLLING_ENTER
 *
 * @brief Ensure safe polling method when set
 *
 * Polling for a condition can be hazardous and result in infinite looping if any previous interrupt status
 * condition is not cleared. \n
 * Setting these flags enforce error clearing on start of polling method to avoid it.
 * the drawback are : \n
 * @li extra use-less i2c bus usage and traffic
 * @li potentially slower measure rate.
 * If application ensure interrupt get clear on mode or interrupt configuration change
 * then keep option disabled. \n
 * To be safe set these option to 1
 * @ingroup Configuration
 */
#define VL6180X_SAFE_POLLING_ENTER  0


/**
 * @brief Enable function start/end logging
 *
 * requires porting  @a #LOG_FUNCTION_START @a #LOG_FUNCTION_END @a #LOG_FUNCTION_END_FMT
 * @ingroup Configuration
 */
#define VL6180X_LOG_ENABLE  0



#endif /* VL6180X_APPCFG_H_ */
