/*
 *  vl53l0x_api_ranging.c - Linux kernel modules for
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



#include "vl53l0x_api.h"
#include "vl53l0x_api_core.h"


#ifndef __KERNEL__
#include <stdlib.h>
#endif
#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

