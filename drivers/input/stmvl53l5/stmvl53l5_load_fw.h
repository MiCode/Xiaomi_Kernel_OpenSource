/**************************************************************************
 * Copyright (c) 2016, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2021 XiaoMi, Inc.

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************/

#ifndef STMVL53L5_LOAD_FW_H
#define STMVL53L5_LOAD_FW_H

union dci_union__go2_status_0_go1_u {
	uint8_t bytes;
	struct {

		uint8_t mcu__boot_complete_go1 : 1;

		uint8_t mcu__analog_checks_ok_go1 : 1;

		uint8_t mcu__threshold_triggered_g01 : 1;
		uint8_t mcu__error_flag_go1 : 1;
		uint8_t mcu__ui_range_data_present_go1 : 1;
		uint8_t mcu__ui_new_range_data_avail_go1 : 1;
		uint8_t mcu__ui_update_blocked_go1 : 1;

		uint8_t mcu__hw_trap_flag_go1 : 1;
	};
};

union dci_union__go2_status_1_go1_u {
	uint8_t bytes;
	struct {

		uint8_t mcu__avdd_reg_ok_go1 : 1;

		uint8_t mcu__pll_lock_ok_go1 : 1;

		uint8_t mcu__ls_watchdog_pass_go1 : 1;

		uint8_t mcu__warning_flag_go1 : 1;

		uint8_t mcu__cp_collapse_flag_go1 : 1;
		uint8_t mcu__spare2 : 1;
		uint8_t mcu__spare3 : 1;
		uint8_t mcu__spare4 : 1;
	};
};

struct dci_ui__dev_info_t {

	union dci_union__go2_status_0_go1_u dev_info__go2_status_0;

	union dci_union__go2_status_1_go1_u dev_info__go2_status_1;

	uint8_t dev_info__device_status;

	uint8_t dev_info__ui_stream_count;
};

#define GO2_STATUS_0 0x6
#define GO2_STATUS_1 0x7

#define VL53L5_GO2_STATUS_0(p_dev) \
	((p_dev)->dev_info__go2_status_0)

#define VL53L5_GO2_STATUS_1(p_dev) \
	((p_dev)->dev_info__go2_status_1)

#define HW_TRAP(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__hw_trap_flag_go1 == 1)

#define MCU_ERROR(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__error_flag_go1 == 1)

#define MCU_BOOT_COMPLETE(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__boot_complete_go1 == 1)

#define MCU_BOOT_NOT_COMPLETE(p_dev)\
	(VL53L5_GO2_STATUS_0(p_dev).mcu__boot_complete_go1 == 0)


int32_t stmvl53l5_load_fw_stm(struct i2c_client *client, uint8_t * i2c_buffer);
int32_t stmvl53l5_move_device_to_low_power(struct i2c_client *client, struct spi_data_t *spi_data, uint8_t i2c_not_spi, uint8_t * raw_data_buffer);

#endif
