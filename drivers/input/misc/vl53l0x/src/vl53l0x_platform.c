/*
 *  vl53l0x_platform.c - Linux kernel modules for STM VL53L0 FlightSense TOF
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


/**
 * @file VL_i2c.c
 *
 * Copyright (C) 2014 ST MicroElectronics
 *
 * provide variable word size byte/Word/dword VL6180x register access via i2c
 *
 */
#include "vl53l0x_platform.h"
#include "vl53l0x_i2c_platform.h"
#include "vl53l0x_api.h"

#define LOG_FUNCTION_START(fmt, ...) \
		_LOG_FUNCTION_START(TRACE_MODULE_PLATFORM, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
		_LOG_FUNCTION_END(TRACE_MODULE_PLATFORM, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...)\
		_LOG_FUNCTION_END_FMT(TRACE_MODULE_PLATFORM, status,\
		fmt, ##__VA_ARGS__)



int8_t VL_LockSequenceAccess(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;

	return Status;
}

int8_t VL_UnlockSequenceAccess(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;

	return Status;
}

/* the ranging_sensor_comms.dll will take care of the page selection */
int8_t VL_WriteMulti(struct vl_data *Dev, uint8_t index,
				uint8_t *pdata, uint32_t count)
{

	int8_t Status = VL_ERROR_NONE;
	int32_t status_int = 0;
	uint8_t deviceAddress;

	if (count >= VL_MAX_I2C_XFER_SIZE)
		Status = VL_ERROR_INVALID_PARAMS;


	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_write_multi(Dev, index, pdata, count);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

/* the ranging_sensor_comms.dll will take care of the page selection */
int8_t VL_ReadMulti(struct vl_data *Dev, uint8_t index,
				uint8_t *pdata, uint32_t count)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	if (count >= VL_MAX_I2C_XFER_SIZE)
		Status = VL_ERROR_INVALID_PARAMS;


	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_read_multi(Dev, index, pdata, count);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}


int8_t VL_WrByte(struct vl_data *Dev, uint8_t index, uint8_t data)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_write_byte(Dev, index, data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

int8_t VL_WrWord(struct vl_data *Dev, uint8_t index, uint16_t data)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_write_word(Dev, index, data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

int8_t VL_WrDWord(struct vl_data *Dev, uint8_t index, uint32_t data)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_write_dword(Dev, index, data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

int8_t VL_UpdateByte(struct vl_data *Dev, uint8_t index,
				uint8_t AndData, uint8_t OrData)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;
	uint8_t data;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_read_byte(Dev, index, &data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	if (Status == VL_ERROR_NONE) {
		data = (data & AndData) | OrData;
		status_int = VL_write_byte(Dev, index, data);

		if (status_int != 0)
			Status = VL_ERROR_CONTROL_INTERFACE;
	}

	return Status;
}

int8_t VL_RdByte(struct vl_data *Dev, uint8_t index, uint8_t *data)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_read_byte(Dev, index, data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

int8_t VL_RdWord(struct vl_data *Dev, uint8_t index, uint16_t *data)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_read_word(Dev, index, data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

int8_t  VL_RdDWord(struct vl_data *Dev, uint8_t index, uint32_t *data)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t status_int;
	uint8_t deviceAddress;

	deviceAddress = Dev->I2cDevAddr;

	status_int = VL_read_dword(Dev, index, data);

	if (status_int != 0)
		Status = VL_ERROR_CONTROL_INTERFACE;

	return Status;
}

#define VL_POLLINGDELAY_LOOPNB  250
int8_t VL_PollingDelay(struct vl_data *Dev)
{
	int8_t status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");
	usleep_range(1000, 1001);
	LOG_FUNCTION_END(status);
	return status;
}
