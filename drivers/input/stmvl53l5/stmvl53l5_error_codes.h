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

#ifndef _STMVL53L5_ERROR_CODES_H_
#define _STMVL53L5_ERROR_CODES_H_

#ifdef __cplusplus
extern "C" {
#endif

#define STMVL53L5_ERROR int

#define STMVL53L5_ERROR_NONE				((STMVL53L5_ERROR)  0)

#define STMVL53L5_ERROR_INVALID_PARAMS			((STMVL53L5_ERROR) - 4)

#define STMVL53L5_ERROR_TIME_OUT			((STMVL53L5_ERROR) - 7)

#define STMVL53L5_ERROR_POWER_STATE			((STMVL53L5_ERROR) - 42)

#define STMVL53L5_ERROR_BOOT_COMPLETE_TIMEOUT		((STMVL53L5_ERROR) - 51)

#define STMVL53L5_ERROR_MCU_IDLE_TIMEOUT		((STMVL53L5_ERROR) - 52)

#define STMVL53L5_ERROR_FW_BUFF_NOT_FOUND		((STMVL53L5_ERROR) - 57)

#define STMVL53L5_ERROR_FALSE_MCU_ERROR_POWER_STATE	((STMVL53L5_ERROR) - 61)

#define STMVL53L5_BYTE_SWAP_FAIL			((STMVL53L5_ERROR) - 71)

#define STMVL53L5_COMMS_ERROR				((STMVL53L5_ERROR) - 93)

#define STMVL53L5_UNKNOWN_SILICON_REVISION		((STMVL53L5_ERROR) - 100)

#define STMVL53L5_ERROR_MCU_ERROR_WAIT_STATE		((STMVL53L5_ERROR) - 69)

#define STMVL53L5_ERROR_MCU_ERROR_HW_STATE		((STMVL53L5_ERROR) - 59)

#define STMVL53L5_ERROR_MCU_NVM_NOT_PROGRAMMED		((STMVL53L5_ERROR) - 50)

#define STMVL53L5_ERROR_INIT_FW_CHECKSUM		((STMVL53L5_ERROR) - 48)

#define STMVL53L5_ERROR_NO_INTERRUPT_HANDLER		((STMVL53L5_ERROR) - 300)

#define STMVL53L5_FIRMWARE_NOT_LOADED_ERROR		((STMVL53L5_ERROR) - 301)

#ifdef __cplusplus
}
#endif

#endif
