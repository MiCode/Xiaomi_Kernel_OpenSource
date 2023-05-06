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

#ifndef VL53L5_REGISTER_UTILS_H_
#define VL53L5_REGISTER_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "stmvl53l5_module_dev.h"

#define PAGE_SELECT 0x7FFF

#define GPIO_LOW 0
#define GPIO_HIGH 1

#define REGULATOR_DISABLE 0
#define REGULATOR_ENABLE 1

#define REGDVDD1V1__INDEX 0x000F
#define XSHUT_CTRL 0x0009

#define DEFAULT_PAGE 2
#define GO2_PAGE 0

#define GO2_STATUS_0 0x6
#define GO2_STATUS_1 0x7

#define MASK_XSHUT_REGISTER(reg_val, value)\
do {\
	reg_val &= 0xf8;\
	reg_val |= (value ? 0x04 : 0x02);\
} while (0)

#define REGULATOR_REGISTER_OR_MASK(return_mask, lp_reg_enable, hp_reg_enable)\
do {\
	return_mask = 0;\
	return_mask |= (lp_reg_enable ? 0x00 : 0x02);\
	return_mask |= (hp_reg_enable ? 0x00 : 0x01);\
} while (0)

#define REGULATOR_REGISTER_AND_MASK(return_mask)\
	(return_mask = 0xfc)

#define BOOT_STATE_MATCHED(p_dev, state) \
	(((state == VL53L5_BOOT_STATE_HIGH) && MCU_BOOT_COMPLETE(p_dev)) || \
	((state == VL53L5_BOOT_STATE_LOW) && MCU_BOOT_NOT_COMPLETE(p_dev)))

enum vl53l5_boot_state {

	VL53L5_BOOT_STATE_LOW = 0,

	VL53L5_BOOT_STATE_HIGH = 1,

	VL53L5_BOOT_STATE_ERROR = 2
};

#define VL53L5_GO2_STATUS_0(p_dev) \
	((p_dev)->host_dev.ui_dev_info.dev_info__go2_status_0)

#define VL53L5_GO2_STATUS_1(p_dev) \
	((p_dev)->host_dev.ui_dev_info.dev_info__go2_status_1)

#define HW_TRAP(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__hw_trap_flag_go1 == 1)

#define MCU_ERROR(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__error_flag_go1 == 1)

#define MCU_BOOT_COMPLETE(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__boot_complete_go1 == 1)

#define MCU_BOOT_NOT_COMPLETE(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__boot_complete_go1 == 0)

#define MCU_FIRST_BOOT_COMPLETE(p_dev)\
	(VL53L5_GO2_STATUS_1(p_dev).mcu__initial_ram_boot_complete == 1)

#define MCU_FIRST_BOOT_NOT_COMPLETE(p_dev)\
	(VL53L5_GO2_STATUS_1(p_dev).mcu__initial_ram_boot_complete == 0)

#define MCU_FIRST_BOOT_COMPLETE_CUT_1_4(p_dev)\
	(VL53L5_GO2_STATUS_1(p_dev).mcu__spare0 == 1)

#define MCU_FIRST_BOOT_NOT_COMPLETE_CUT_1_4(p_dev)\
	(VL53L5_GO2_STATUS_1(p_dev).mcu__spare0 == 0)

#define MCU_FIRST_BOOT(p_dev)\
	(VL53L5_GO2_STATUS_1(p_dev).bytes == 0x1f)

#define MCU_NVM_PROGRAMMED(p_dev)\
	(VL53L5_GO2_STATUS_1(p_dev).mcu__spare1 == 1)

int32_t stmvl53l5_register_read_modify_write(
	struct stmvl53l5_dev_t *p_dev, uint16_t addr,
	uint8_t first_and_mask, uint8_t second_or_mask);

int32_t stmvl53l5_set_page(struct stmvl53l5_dev_t *p_dev, uint8_t page);

int32_t stmvl53l5_set_manual_xshut_state(
	struct stmvl53l5_dev_t *p_dev, uint8_t state);

int32_t stmvl53l5_set_regulators(
	struct stmvl53l5_dev_t *p_dev,
	uint8_t lp_reg_enable,
	uint8_t hp_reg_enable);

int32_t stmvl53l5_wait_mcu_boot(
	struct stmvl53l5_dev_t *p_dev, enum vl53l5_boot_state state,
	uint32_t timeout_ms, uint32_t wait_time_ms);

int32_t stmvl53l5_check_device_booted(struct stmvl53l5_dev_t *p_dev);

#ifdef __cplusplus
}
#endif

#endif
