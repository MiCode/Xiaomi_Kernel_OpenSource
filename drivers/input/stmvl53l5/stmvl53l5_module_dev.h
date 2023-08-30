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

#ifndef __STMVL53L5_MODULE_H__
#define __STMVL53L5_MODULE_H__

#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>

#ifdef __cplusplus
extern "C" {
#endif
#define STMVL53L5_DRV_NAME "stmvl53l5"
#define VL53L5_COMMS_CHUNK_SIZE 1024
#define STATUS_OK 0

#define VL53L5_MCU_BOOT_WAIT_DELAY 50

#define VL53L5_HP_IDLE_WAIT_DELAY 0

#define VL53L5_LP_IDLE_WAIT_DELAY 0
#define VL53L5_BOOT_COMPLETION_POLLING_TIMEOUT_MS 500
#define VL53L5_PATCH_LOAD 1
#define VL53L5_PATCH_BOOT 2
#define VL53L5_RAM_LOAD 3

enum stmvl53l5_comms_type {
	STMVL53L5_I2C = 0,
	STMVL53L5_SPI
};

struct vl53l5_patch_data_t {
	uint32_t patch_ver_major;
	uint32_t patch_ver_minor;
	uint32_t patch_ver_build;
	uint32_t patch_ver_revision;

	uint32_t boot_flag;

	uint32_t patch_offset;

	uint32_t patch_size;

	uint32_t patch_checksum;

	uint32_t tcpm_offset;

	uint32_t tcpm_size;

	uint32_t tcpm_page;

	uint32_t tcpm_page_offset;

	uint32_t hooks_offset;

	uint32_t hooks_size;

	uint32_t hooks_page;

	uint32_t hooks_page_offset;

	uint32_t breakpoint_en_offset;

	uint32_t breakpoint_en_size;

	uint32_t breakpoint_en_page;

	uint32_t breakpoint_en_page_offset;

	uint32_t breakpoint_offset;

	uint32_t breakpoint_size;

	uint32_t breakpoint_page;

	uint32_t breakpoint_page_offset;

	uint32_t checksum_en_offset;

	uint32_t checksum_en_size;

	uint32_t checksum_en_page;

	uint32_t checksum_en_page_offset;

	uint32_t patch_code_offset;

	uint32_t patch_code_size;

	uint32_t patch_code_page;

	uint32_t patch_code_page_offset;

	uint32_t dci_tcpm_patch_0_offset;

	uint32_t dci_tcpm_patch_0_size;

	uint32_t dci_tcpm_patch_0_page;

	uint32_t dci_tcpm_patch_0_page_offset;

	uint32_t dci_tcpm_patch_1_offset;

	uint32_t dci_tcpm_patch_1_size;

	uint32_t dci_tcpm_patch_1_page;

	uint32_t dci_tcpm_patch_1_page_offset;
};

struct stmvl53l5_gpio_t {
#ifdef STM_VL53L5_GPIO_ENABLE

	int pwren_gpio_nb;

	unsigned char pwren_gpio_owned;

	int lpn_gpio_nb;

	unsigned char lpn_gpio_owned;

	int comms_gpio_nb;

	unsigned char comms_gpio_owned;
#endif

	int intr_gpio_nb;

	unsigned char intr_gpio_owned;
};

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
		uint8_t mcu__spare0 : 1;
		uint8_t mcu__initial_ram_boot_complete : 1;
		uint8_t mcu__spare1 : 1;
	};
};

struct dci_ui__dev_info_t {

	union dci_union__go2_status_0_go1_u dev_info__go2_status_0;

	union dci_union__go2_status_1_go1_u dev_info__go2_status_1;

	uint8_t dev_info__device_status;

	uint8_t dev_info__ui_stream_count;
};

enum vl53l5_power_states {

	VL53L5_POWER_STATE_OFF = 0,

	VL53L5_POWER_STATE_ULP_IDLE = 1,

	VL53L5_POWER_STATE_LP_IDLE_COMMS = 3,

	VL53L5_POWER_STATE_HP_IDLE = 4,

	VL53L5_POWER_STATE_RANGING = 5
};

struct stmvl53l5_fw_header_t {
	unsigned int binary_ver_major;
	unsigned int binary_ver_minor;
	unsigned int binary_ver_build;
	unsigned int binary_ver_revision;
	unsigned int fw_offset;
	unsigned int fw_size;
};

struct stmvl53l5_kernel_t {
	uint8_t *p_fw_buff;

	uint32_t fw_buff_count;

	uint8_t *p_comms_buff;

	uint32_t comms_buff_max_count;

	uint32_t comms_buff_count;

	struct dci_ui__dev_info_t ui_dev_info;

	enum vl53l5_power_states power_state;

	bool device_booted;

	struct vl53l5_patch_data_t patch_data;

	uint8_t device_id;
	uint8_t revision_id;
};

struct stmvl53l5_dev_t {
	struct stmvl53l5_kernel_t host_dev;
};

struct stmvl53l5_module_t {

	struct stmvl53l5_dev_t stdev;

	struct spi_device *device;

	struct i2c_client *client;

	wait_queue_head_t wq;

	atomic_t intr_ready_flag;

	atomic_t force_wakeup;

	struct stmvl53l5_gpio_t *gpio;

	const char *firmware_name;

	int last_driver_error;

	enum stmvl53l5_comms_type comms_type;
};

#ifdef __cplusplus
}
#endif

#endif
