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
 * $Date: 2015-01-08 05:27:09 -0800 (Thu, 08 Jan 2015) $
 * $Revision: 2038 $
 */
 
/**
 * @file VL6180x_cfg.h
 *
 * Extended range configuration (for Hybrid AF applications for instance)
 */

#ifndef VL6180x_CFG_H_
#define VL6180x_CFG_H_


/** @defgroup api_config Configuration
 *  @brief API static configuration
 */



/** @ingroup api_config
 * @{*/


/**
 * @def VL6180x_UPSCALE_SUPPORT
 * @brief Configure up-scale capabilities and default up-scale factor for ranging operations
 * 
 * @li 1 : Fixed scaling by 1 (no up-scaling support)
 * @li 2 : Fixed scaling by 2
 * @li 3 : Fixed scaling by 3
 * @li  -1 -2 -3 : Run time programmable through @a VL6180x_UpscaleSetScaling(). Default scaling factore is -VL6180x_UPSCALE_SUPPORT \n
 */
#define VL6180x_UPSCALE_SUPPORT -3

/**
 * @def VL6180x_ALS_SUPPORT
 * @brief Enable ALS support
 *
 * Set to 0 if ALS is not used in application. This can help reducing code size if it is a concern.
 */
#define VL6180x_ALS_SUPPORT      0

/**
 * @def VL6180x_HAVE_DMAX_RANGING
 * @brief Enable DMax calculation for ranging applications.
 *  
 * When set to 1, __Dmax__ is returned by API typically when  @a VL6180x_RangePollMeasurement() high level
 * function is called (this is returned in @a VL6180x_RangeData_t structure).
 * __Dmax__ is an estimation of the maximum distance (in mm) the product can report a valid distance of a 17% target for 
 * the current ambient light conditions (__Dmax__ decreases when ambient light increases). __Dmax__ should be used only
 * when the product is not able to return a valid distance (no object or object is too far from the ranging sensor).
 * Typically, this is done by checking the __errorStatus__ field of the @a VL6180x_RangeData_t structure returned by 
 * the @a VL6180x_RangePollMeasurement() function.
 * You may refer to ::RangeError_u to get full list of supported error codes.
 * @warning Dmax is estimated for a 17% grey target. If the real target has a reflectance lower than 17%, report Dmax could be over-estimated 
 */
#define VL6180x_HAVE_DMAX_RANGING   1 // Laser //1

/**
 * @def VL6180x_WRAP_AROUND_FILTER_SUPPORT
 * @brief Enable wrap around filter (WAF) feature
 *  
 * In specific conditions, when targeting a mirror or a very reflective metal, a __wrap around__ effect can occur internally to the
 * ranging product which results in returning a wrong distance (under-estimated). Goal of the WAF is to detect this wrap arround effect
 * and to filter it by returning a non-valid distance : __errorStatus__ set to 16 (see ::RangeError_u)
 * @warning Wrap-around filter can not be used when device is running in continuous mode 
 * 
 * @li 0 : Filter is not supported, no filtering code is included in API
 * @li 1 : Filter is supported and active by default
 * @li -1 : Filter is supported but is not active by default @a VL6180x_FilterSetState() can turn it on and off at any time
 */
#define VL6180x_WRAP_AROUND_FILTER_SUPPORT   1

/**
 * @def VL6180x_EXTENDED_RANGE
 * @brief Enable extended ranging support
 *
 * Device that do not formally support extended ranging should only be used with a scaling factor of 1.
 * Correct operation with scaling factor other than 1 (>200mm ) is not granted by ST.
 */
#define VL6180x_EXTENDED_RANGE 1

#if (VL6180x_EXTENDED_RANGE) && (VL6180x_ALS_SUPPORT)
#warning "Als support should be OFF for extended range"
#endif

#endif
/** @} */ // end of api_config  

/* VL6180x_CFG_H_ */
