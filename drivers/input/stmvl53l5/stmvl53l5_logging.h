/*******************************************************************************
* Copyright (c) 2022, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L5 Kernel Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L5 Kernel Driver may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*******************************************************************************/

#ifndef STMVL53L5_LOGGING_H_
#define STMVL53L5_LOGGING_H_

#ifdef __cplusplus
extern "C" {
#endif

#define STMVL53L5_TRACE_LEVEL_NONE	0x00000000
#define STMVL53L5_TRACE_LEVEL_ERRORS	0x00000001
#define STMVL53L5_TRACE_LEVEL_WARNING	0x00000002
#define STMVL53L5_TRACE_LEVEL_INFO	0x00000004
#define STVL53L5_TRACE_LEVEL_DEBUG	0x00000008
#define STMVL53L5_TRACE_LEVEL_ALL	0x00000010
#define STMVL53L5_TRACE_LEVEL_IGNORE	0x00000020

#define stmvl53l5_trace_print(level, ...) \
	_LOG_TRACE_PRINT(VL53L5_TRACE_MODULE_VL53L5, \
	level, VL53L5_TRACE_FUNCTION_ALL, ##__VA_ARGS__)

#define LOG_FUNCTION_START(fmt, args...) \
	_LOG_FUNCTION_START(VL53L5_TRACE_MODULE_VL53L5, fmt, ##args)

#define LOG_FUNCTION_END(status, args...) \
	_LOG_FUNCTION_END(VL53L5_TRACE_MODULE_VL53L5, status, ##args)

#define STMVL53L5_LOG_ERROR(fmt, args...) \
	pr_err("%s: " fmt, __func__, ##args)

#define STMVL53L5_LOG_DEBUG(fmt, args...) \
	pr_debug("%s: " fmt, __func__, ##args)

#define STMVL53L5_LOG_INFO(fmt, args...) \
	pr_info("%s: " fmt, __func__, ##args)

#define STMVL53L5_LOG_WARNING(fmt, args...) \
	pr_warn("%s: " fmt, __func__, ##args)

#define _LOG_TRACE_PRINT(module, level, function, fmt, args...)  \
	STMVL53L5_LOG_DEBUG("" fmt "", ##args)

#define _LOG_FUNCTION_START(module, fmt, args...) \
	STMVL53L5_LOG_DEBUG("START " fmt "\n", ##args)

#define	_LOG_FUNCTION_END(module, status, args...)\
	STMVL53L5_LOG_DEBUG("END %d\n", (int)status, ##args)

#ifdef __cplusplus
}
#endif

#endif
