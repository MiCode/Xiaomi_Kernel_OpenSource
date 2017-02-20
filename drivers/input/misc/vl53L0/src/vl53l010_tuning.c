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
 ******************************************************************************/

#include "vl53l010_tuning.h"

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

#ifdef VL53L0_LOG_ENABLE
#define trace_print(level, ...) \
	trace_print_module_function(TRACE_MODULE_API,\
	level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#endif

/*
 * //////////////////////////////////////////////////////
 * ////       DEFAULT TUNING SETTINGS                ////
 * //////////////////////////////////////////////////////
 */
VL53L0_Error VL53L010_load_tuning_settings(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	LOG_FUNCTION_START("");

	/* update 14_12_15_v11 */
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x91, 0x3C);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x54, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x33, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x32, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x30, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x50, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x60, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x70, 0x06);

	Status |= VL53L0_WrByte(Dev, 0x46, 0x1a);
	Status |= VL53L0_WrWord(Dev, 0x51, 0x01a3);
	Status |= VL53L0_WrWord(Dev, 0x61, 0x01c4);
	Status |= VL53L0_WrWord(Dev, 0x71, 0x018c);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x31, 0x0f);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x66, 0x38);

	Status |= VL53L0_WrByte(Dev, 0x47, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x48, 0xff);
	Status |= VL53L0_WrByte(Dev, 0x57, 0x4c);
	Status |= VL53L0_WrByte(Dev, 0x67, 0x3c);
	Status |= VL53L0_WrByte(Dev, 0x77, 0x5c);

	Status |= VL53L0_WrWord(Dev, 0x44, 0x0000);

	Status |= VL53L0_WrByte(Dev, 0x27, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x55, 0x00);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x30, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

	Status |= VL53L0_WrByte(Dev, 0x10, 0x0f);
	Status |= VL53L0_WrByte(Dev, 0x11, 0xff);
	Status |= VL53L0_WrByte(Dev, 0x40, 0x82);
	Status |= VL53L0_WrByte(Dev, 0x41, 0xff);
	Status |= VL53L0_WrByte(Dev, 0x42, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x43, 0x12);

	Status |= VL53L0_WrByte(Dev, 0x20, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x21, 0x00);

	Status |= VL53L0_WrByte(Dev, 0x28, 0x06);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x48, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

	Status |= VL53L0_WrByte(Dev, 0x7a, 0x0a);
	Status |= VL53L0_WrByte(Dev, 0x7b, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x78, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x44, 0xff);
	Status |= VL53L0_WrByte(Dev, 0x45, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x46, 0x10);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

	Status |= VL53L0_WrByte(Dev, 0x04, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x05, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x06, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x07, 0x00);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x0d, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x01, 0xF8);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x8e, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

	if (Status != 0)
		Status = VL53L0_ERROR_CONTROL_INTERFACE;

	LOG_FUNCTION_END(Status);
	return Status;
}
