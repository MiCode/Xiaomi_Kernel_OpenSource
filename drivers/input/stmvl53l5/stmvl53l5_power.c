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

#include "stmvl53l5_power.h"
#include "stmvl53l5_register_utils.h"
#include "stmvl53l5_load_firmware.h"
#include "stmvl53l5_platform.h"
#include "stmvl53l5_error_codes.h"
#include "stmvl53l5_logging.h"

#define GPIO_LOW 0
#define GPIO_HIGH 1

#define REGULATOR_DISABLE 0
#define REGULATOR_ENABLE 1

#define DEFAULT_PAGE 2
#define GO2_PAGE 0

#define STMVL53L5_CHECK_POWER_STATE(p_dev, state)\
	((p_dev)->host_dev.power_state == (state))

#define STMVL53L5_CHECK_POWER_STATE_OFF(p_dev)\
	(STMVL53L5_CHECK_POWER_STATE((p_dev), STMVL53L5_POWER_STATE_OFF))

#define STMVL53L5_CHECK_POWER_STATE_ULP_IDLE(p_dev)\
	(STMVL53L5_CHECK_POWER_STATE((p_dev),\
		STMVL53L5_POWER_STATE_ULP_IDLE))

#define STMVL53L5_CHECK_POWER_STATE_LP_IDLE_COMMS(p_dev)\
	(STMVL53L5_CHECK_POWER_STATE((p_dev),\
		STMVL53L5_POWER_STATE_LP_IDLE_COMMS))

#define STMVL53L5_CHECK_POWER_STATE_HP_IDLE(p_dev)\
	(STMVL53L5_CHECK_POWER_STATE((p_dev),\
		STMVL53L5_POWER_STATE_HP_IDLE))

#define STMVL53L5_CHECK_POWER_STATE_RANGING(p_dev)\
	(STMVL53L5_CHECK_POWER_STATE((p_dev),\
		STMVL53L5_POWER_STATE_RANGING))

static int _set_power_to_hp_idle(struct stmvl53l5_dev_t *p_dev);

static int _set_power_to_lp_idle_comms(struct stmvl53l5_dev_t *p_dev);

static int _set_power_to_ulp_idle(struct stmvl53l5_dev_t *p_dev);

static int _go_to_hp_idle(struct stmvl53l5_dev_t *p_dev);

static int _go_to_lp_idle_comms(struct stmvl53l5_dev_t *p_dev);

static int _go_to_ulp_idle(struct stmvl53l5_dev_t *p_dev);

int stmvl53l5_set_power_mode(
	struct stmvl53l5_dev_t *p_dev,
	enum stmvl53l5_power_states power_mode)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	if ((STMVL53L5_CHECK_POWER_STATE_OFF(p_dev) &&
	    (STMVL53L5_MCU_FIRST_BOOT_NOT_COMPLETE(p_dev))) ||
	     STMVL53L5_CHECK_POWER_STATE_RANGING(p_dev)) {
		status = STMVL53L5_ERROR_POWER_STATE;
		goto exit;
	}

	switch (power_mode) {
	case STMVL53L5_POWER_STATE_HP_IDLE:
		if (p_dev->host_dev.power_state
				!= STMVL53L5_POWER_STATE_HP_IDLE)
			status = _set_power_to_hp_idle(p_dev);
		break;
	case STMVL53L5_POWER_STATE_LP_IDLE_COMMS:
		if (p_dev->host_dev.power_state
				!= STMVL53L5_POWER_STATE_LP_IDLE_COMMS)
			status = _set_power_to_lp_idle_comms(p_dev);
		break;

	case STMVL53L5_POWER_STATE_ULP_IDLE:
		if (p_dev->host_dev.power_state
				!= STMVL53L5_POWER_STATE_ULP_IDLE)
			status = _set_power_to_ulp_idle(p_dev);
		break;
	case STMVL53L5_POWER_STATE_OFF:

	case STMVL53L5_POWER_STATE_RANGING:

	default:
		status = STMVL53L5_ERROR_POWER_STATE;
		break;
	}

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _go_to_hp_idle(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	if (STMVL53L5_CHECK_POWER_STATE_ULP_IDLE(p_dev)) {

		status = stmvl53l5_set_manual_xshut_state(p_dev, GPIO_HIGH);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;

		status = stmvl53l5_set_regulators(p_dev, REGULATOR_ENABLE,
					 REGULATOR_ENABLE);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;

	} else if (STMVL53L5_CHECK_POWER_STATE_LP_IDLE_COMMS(p_dev)) {

		status = stmvl53l5_set_manual_xshut_state(p_dev, GPIO_HIGH);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	} else if (STMVL53L5_CHECK_POWER_STATE_OFF(p_dev) &&
		   p_dev->host_dev.device_booted == true) {

		status = stmvl53l5_set_manual_xshut_state(p_dev, GPIO_HIGH);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	} else {
		status = STMVL53L5_ERROR_POWER_STATE;
		goto exit;
	}

	status = stmvl53l5_wait_mcu_boot(p_dev, STMVL53L5_BOOT_STATE_HIGH,
					0, STMVL53L5_HP_IDLE_WAIT_DELAY);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	p_dev->host_dev.power_state = STMVL53L5_POWER_STATE_HP_IDLE;

exit:
	return status;
}

static int _go_to_lp_idle_comms(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	status = stmvl53l5_set_manual_xshut_state(p_dev, GPIO_LOW);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_wait_mcu_boot(p_dev, STMVL53L5_BOOT_STATE_LOW,
					0, STMVL53L5_LP_IDLE_WAIT_DELAY);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	p_dev->host_dev.power_state = STMVL53L5_POWER_STATE_LP_IDLE_COMMS;

exit:
	return status;
}

static int _go_to_ulp_idle(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	status = _go_to_lp_idle_comms(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_set_regulators(p_dev, REGULATOR_DISABLE,
					  REGULATOR_DISABLE);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	p_dev->host_dev.power_state = STMVL53L5_POWER_STATE_ULP_IDLE;

exit:
	return status;
}

static int _set_power_to_hp_idle(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	bool comms_on = true;
	enum stmvl53l5_power_states current_state = p_dev->host_dev.power_state;
	struct stmvl53l5_module_t *p_module;

	p_module = (struct stmvl53l5_module_t *)
		container_of(p_dev, struct stmvl53l5_module_t, stdev);

	if (STMVL53L5_CHECK_POWER_STATE_HP_IDLE(p_dev))
		goto exit;

	if (comms_on) {

		status = stmvl53l5_set_page(p_dev, GO2_PAGE);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit_page_changed;
	}

	status = _go_to_hp_idle(p_dev);

exit_page_changed:

	if (status != STMVL53L5_ERROR_NONE) {
		(void)stmvl53l5_set_page(p_dev, DEFAULT_PAGE);
		goto exit;
	} else {
		status = stmvl53l5_set_page(p_dev, DEFAULT_PAGE);
		if (status != STMVL53L5_ERROR_NONE)
			goto exit;
	}

	if (current_state == STMVL53L5_POWER_STATE_ULP_IDLE) {
		status = stmvl53l5_load_fw_stm(p_dev);
		if (status < STMVL53L5_ERROR_NONE) {
			p_module->firmware_loaded = false;
			goto exit;
		} else {
			p_module->firmware_loaded = true;
		}
	}

exit:
	return status;
}

static int _set_power_to_lp_idle_comms(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	enum stmvl53l5_power_states current_state = p_dev->host_dev.power_state;

	bool comms_on = true;

	if (STMVL53L5_CHECK_POWER_STATE_LP_IDLE_COMMS(p_dev))
		goto exit;

	if (current_state == STMVL53L5_POWER_STATE_ULP_IDLE) {
		status = _set_power_to_hp_idle(p_dev);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	}

	if (comms_on) {

		status = stmvl53l5_set_page(p_dev, GO2_PAGE);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	}
	if (!STMVL53L5_CHECK_POWER_STATE_HP_IDLE(p_dev)) {

		status = _go_to_hp_idle(p_dev);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit_page_changed;

	}

	status = _go_to_lp_idle_comms(p_dev);

exit_page_changed:

	if (status < STMVL53L5_ERROR_NONE)
		(void)stmvl53l5_set_page(p_dev, DEFAULT_PAGE);
	else
		status = stmvl53l5_set_page(p_dev, DEFAULT_PAGE);

exit:
	return status;
}

static int _set_power_to_ulp_idle(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	bool comms_on = true;

	if (STMVL53L5_CHECK_POWER_STATE_ULP_IDLE(p_dev))
		goto exit;

	if (comms_on) {

		status = stmvl53l5_set_page(p_dev, GO2_PAGE);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	}

	if (!STMVL53L5_CHECK_POWER_STATE_HP_IDLE(p_dev)) {

		status = _go_to_hp_idle(p_dev);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit_page_changed;
	}

	status = _go_to_ulp_idle(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_page_changed;

exit_page_changed:

	if (status < STMVL53L5_ERROR_NONE)
		(void)stmvl53l5_set_page(p_dev, DEFAULT_PAGE);
	else
		status = stmvl53l5_set_page(p_dev, DEFAULT_PAGE);

exit:
	return status;
}
