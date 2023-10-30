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

#ifndef _STMVL53L5_PLATFORM_H_
#define _STMVL53L5_PLATFORM_H_

#include "stmvl53l5_module_dev.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define GPIO_LOW 0
#define GPIO_HIGH 1
#define GPIO_TRISTATE 2

int32_t stmvl53l5_write_multi(
	struct stmvl53l5_dev_t *p_dev, uint16_t index, uint8_t *pdata,
	uint32_t count);

int32_t stmvl53l5_read_multi(
	struct stmvl53l5_dev_t *p_dev, uint16_t index, uint8_t *pdata,
	uint32_t count);

int32_t stmvl53l5_wait_us(struct stmvl53l5_dev_t *p_dev, uint32_t wait_us);

int32_t stmvl53l5_wait_ms(struct stmvl53l5_dev_t *p_dev, uint32_t wait_ms);

int32_t stmvl53l5_gpio_low_power_control(
	struct stmvl53l5_dev_t *p_dev, uint8_t value);

int32_t stmvl53l5_gpio_power_enable(
	struct stmvl53l5_dev_t *p_dev, uint8_t value);

int32_t stmvl53l5_gpio_comms_select(
	struct stmvl53l5_dev_t *p_dev, uint8_t value);

int32_t stmvl53l5_gpio_interrupt_enable(
	struct stmvl53l5_dev_t *p_dev, void (*function)(void),
	uint8_t edge_type);

int32_t stmvl53l5_gpio_interrupt_disable(struct stmvl53l5_dev_t *p_dev);

int32_t stmvl53l5_get_tick_count(
	struct stmvl53l5_dev_t *p_dev, uint32_t *ptime_ms);

int32_t stmvl53l5_check_for_timeout(
	struct stmvl53l5_dev_t *p_dev, uint32_t start_time_ms,
	uint32_t end_time_ms, uint32_t timeout_ms);

#ifdef STM_VL53L5_GPIO_ENABLE
int32_t stmvl53l5_platform_init(struct stmvl53l5_dev_t *pdev);

int32_t stmvl53l5_platform_terminate(struct stmvl53l5_dev_t *pdev);
#endif

#ifdef __cplusplus
}
#endif

#endif
