/*******************************************************************************
* Copyright (c) 2021, STMicroelectronics - All Rights Reserved
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

#ifndef STMVL53L5_IOCTL_DEFS_H_
#define STMVL53L5_IOCTL_DEFS_H_

struct stmvl53l5_comms_struct_t {
	uint16_t len;
	uint16_t reg_index;
	uint8_t *buf;
};

struct stmvl53l5_power_state_t {
	enum vl53l5_power_states previous;
	enum vl53l5_power_states request;
};

#define STMVL53L5_IOCTL_CHAR 'p'

#define STMVL53L5_IOCTL_WRITE \
	_IOW(STMVL53L5_IOCTL_CHAR, 0x1, void*)
#define STMVL53L5_IOCTL_READ \
	_IOWR(STMVL53L5_IOCTL_CHAR, 0x2, void*)
#define STMVL53L5_IOCTL_ROM_BOOT \
	_IOR(STMVL53L5_IOCTL_CHAR, 0x3, void*)
#define STMVL53L5_IOCTL_FW_LOAD \
	_IO(STMVL53L5_IOCTL_CHAR, 0x4)
#define STMVL53L5_IOCTL_WAIT_FOR_INTERRUPT \
	_IO(STMVL53L5_IOCTL_CHAR, 0x5)
#define STMVL53L5_IOCTL_POWER \
	_IOR(STMVL53L5_IOCTL_CHAR, 0x6, void*)
#define STMVL53L5_IOCTL_KERNEL_VERSION \
	_IOR(STMVL53L5_IOCTL_CHAR, 0x7, void*)
#define STMVL53L5_IOCTL_GET_ERROR \
	_IOR(STMVL53L5_IOCTL_CHAR, 0x8, void*)
#endif
