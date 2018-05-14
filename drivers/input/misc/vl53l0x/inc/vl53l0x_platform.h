/*
 *  vl53l0x_platform.h - Linux kernel modules for STM VL53L0 FlightSense TOF
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



#ifndef _VL_PLATFORM_H_
#define _VL_PLATFORM_H_

#include <linux/delay.h>
#include "vl53l0x_def.h"
#include "vl53l0x_platform_log.h"

#include "stmvl53l0x-i2c.h"
#include "stmvl53l0x-cci.h"
#include "stmvl53l0x.h"

/**
 * @file vl53l0x_platform.h
 *
 * @brief All end user OS/platform/application porting
 */

/**
 * @defgroup VL_platform_group VL53L0 Platform Functions
 * @brief    VL53L0 Platform Functions
 *  @{
 */

/**
 * @def PALDevDataGet
 * @brief Get ST private structure @a struct VL_DevData_t data access
 *
 * @param Dev       Device Handle
 * @param field     ST structure field name
 * It maybe used and as real data "ref" not just as "get" for sub-structure item
 * like PALDevDataGet(FilterData.field)[i] or
 * PALDevDataGet(FilterData.MeasurementIndex)++
 */
#define PALDevDataGet(Dev, field) (Dev->Data.field)

/**
 * @def PALDevDataSet(Dev, field, data)
 * @brief  Set ST private structure @a struct VL_DevData_t data field
 * @param Dev       Device Handle
 * @param field     ST structure field na*me
 * @param data      Data to be set
 */
#define PALDevDataSet(Dev, field, data) ((Dev->Data.field) = (data))


/**
 * @defgroup VL_registerAccess_group PAL Register Access Functions
 * @brief    PAL Register Access Functions
 *  @{
 */

/**
 * Lock comms interface to serialize all commands to a shared I2C interface for a specific device
 * @param   Dev       Device Handle
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_LockSequenceAccess(struct vl_data *Dev);

/**
 * Unlock comms interface to serialize all commands to a shared I2C interface for a specific device
 * @param   Dev       Device Handle
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_UnlockSequenceAccess(struct vl_data *Dev);


/**
 * Writes the supplied byte buffer to the device
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   pdata     Pointer to uint8_t buffer containing the data to be written
 * @param   count     Number of bytes in the supplied byte buffer
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_WriteMulti(struct vl_data *Dev, uint8_t index,
		uint8_t *pdata, uint32_t count);

/**
 * Reads the requested number of bytes from the device
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   pdata     Pointer to the uint8_t buffer to store read data
 * @param   count     Number of uint8_t's to read
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_ReadMulti(struct vl_data *Dev, uint8_t index,
		uint8_t *pdata, uint32_t count);

/**
 * Write single byte register
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   data      8 bit register data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_WrByte(struct vl_data *Dev, uint8_t index, uint8_t data);

/**
 * Write word register
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   data      16 bit register data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_WrWord(struct vl_data *Dev, uint8_t index, uint16_t data);

/**
 * Write double word (4 byte) register
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   data      32 bit register data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_WrDWord(struct vl_data *Dev, uint8_t index, uint32_t data);

/**
 * Read single byte register
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   data      pointer to 8 bit data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_RdByte(struct vl_data *Dev, uint8_t index, uint8_t *data);

/**
 * Read word (2byte) register
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   data      pointer to 16 bit data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_RdWord(struct vl_data *Dev, uint8_t index, uint16_t *data);

/**
 * Read dword (4byte) register
 * @param   Dev       Device Handle
 * @param   index     The register index
 * @param   data      pointer to 32 bit data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_RdDWord(struct vl_data *Dev, uint8_t index, uint32_t *data);

/**
 * Threat safe Update (read/modify/write) single byte register
 *
 * Final_reg = (Initial_reg & and_data) |or_data
 *
 * @param   Dev        Device Handle
 * @param   index      The register index
 * @param   AndData    8 bit and data
 * @param   OrData     8 bit or data
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_UpdateByte(struct vl_data *Dev, uint8_t index,
		uint8_t AndData, uint8_t OrData);

/** @} end of VL_registerAccess_group */


/**
 * @brief execute delay in all polling API call
 *
 * A typical multi-thread or RTOs implementation is to sleep the task for
 * some 5ms (with 100Hz max rate faster polling is not needed)
 * if nothing specific is need you can define it as an empty/void macro
 * @code
 * #define VL_PollingDelay(...) (void)0
 * @endcode
 * @param Dev       Device Handle
 * @return  VL_ERROR_NONE        Success
 * @return  "Other error code"    See ::int8_t
 */
int8_t VL_PollingDelay(struct vl_data *Dev);
/* usually best implemented as a real function */

/** @} end of VL_platform_group */

#endif  /* _VL_PLATFORM_H_ */



