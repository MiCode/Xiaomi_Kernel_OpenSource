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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spi.h>

#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>

#include "stmvl53l5_i2c.h"
#include "stmvl53l5_spi.h"
#include "vl53l5_fw_data.h"
#include "stmvl53l5_load_fw.h"

#define VL53L5_COMMS_BUFFER_SIZE_BYTES	5052
#define PAGE_SELECT 0x7FFF
#define XSHUT_CTRL 0x0009

const uint8_t _fw_buffer[] = EWOKMZ_STXP70_TCPM_RAM_FULL;

uint8_t * host_dev_p_fw_buff;
uint32_t host_dev_fw_buff_count;
uint8_t * host_dev_p_comms_buff;
uint32_t host_dev_comms_buff_max_count;

uint8_t _comms_buffer[VL53L5_COMMS_BUFFER_SIZE_BYTES] = {0};

#define VL53L5_ASSIGN_FW_BUFF(fw_buff_ptr, count) \
do {\
	host_dev_p_fw_buff = (uint8_t *)(fw_buff_ptr);\
	host_dev_fw_buff_count = (count);\
} while (0)

#define VL53L5_ASSIGN_COMMS_BUFF(comms_buff_ptr, max_count) \
do {\
	host_dev_p_comms_buff = (comms_buff_ptr);\
	host_dev_comms_buff_max_count = (max_count);\
} while (0)

#define MASK_XSHUT_REGISTER(reg_val, value)\
do {\
	reg_val &= 0xf8;\
	reg_val |= (value ? 0x04 : 0x02);\
} while (0)


struct dci_ui__dev_info_t dev;

int32_t wait_mcu_boot(struct i2c_client *client, struct spi_data_t *spi_data, uint8_t i2c_not_spi, uint8_t *raw_data_buffer, uint8_t state)
{
	int32_t status = 0; //STATUS_OK;
	// uint32_t start_time_ms   = 0;
	// uint32_t current_time_ms = 0;

	// status = vl53l5_get_tick_count(p_dev, &start_time_ms);
	// if (status < STATUS_OK)
	//	goto exit;

	VL53L5_GO2_STATUS_0(&dev).bytes = 0;

	do {

		if (i2c_not_spi)
			status = stmvl53l5_read_multi(client, raw_data_buffer, GO2_STATUS_0, &VL53L5_GO2_STATUS_0(&dev).bytes, 1);
		else
			status = stmvl53l5_spi_read(spi_data, GO2_STATUS_0, &VL53L5_GO2_STATUS_0(&dev).bytes, 1);
		if (status != 0) //< STATUS_OK)
			goto exit;

		if (HW_TRAP(&dev)) {
			if (i2c_not_spi)
				status = stmvl53l5_read_multi(client, raw_data_buffer, GO2_STATUS_1, &VL53L5_GO2_STATUS_1(&dev).bytes, 1);
			else
				status = stmvl53l5_spi_read(spi_data, GO2_STATUS_1, &VL53L5_GO2_STATUS_1(&dev).bytes, 1);
			if (status != 0) //< STATUS_OK)
				goto exit;

			// if (VL53L5_GO2_STATUS_1(&dev).bytes)
			//	 status = VL53L5_ERROR_MCU_ERROR_POWER_STATE;
			// else
			//	status =
			//	    VL53L5_ERROR_FALSE_MCU_ERROR_POWER_STATE;
			status = -1;
			goto exit;
		}

		if (MCU_ERROR(&dev)) {
			status = -1; //VL53L5_ERROR_MCU_ERROR_POWER_STATE;
			goto exit;
		}

		if (((state == 1) && MCU_BOOT_COMPLETE(&dev)) ||
		    ((state == 0) && MCU_BOOT_NOT_COMPLETE(&dev)))
			break;

		// status = vl53l5_get_tick_count(p_dev, &current_time_ms);
		// if (status < STATUS_OK)
		//	 goto exit;

		// CHECK_FOR_TIMEOUT(
		//	 status, p_dev, start_time_ms, current_time_ms,
		//	 VL53L5_BOOT_COMPLETION_POLLING_TIMEOUT_MS);
		// if (status < STATUS_OK) {
		//	 status = VL53L5_ERROR_BOOT_COMPLETE_TIMEOUT;
		//	goto exit;
		// }
		msleep(10);

	} while (1);

exit:
	if (status != 0)
		printk("stmvl53l5: wait_mcu_boot failed !\n");

	return status;
}



int32_t _check_rom_firmware_boot_cut_1_2(struct i2c_client *client, uint8_t * raw_data_buffer)
{
	int status = 0;

	status = wait_mcu_boot(client, NULL, 1, raw_data_buffer, 1);
	if (status != 0)
		return -1;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x000E, 0x01);
	return status;
}

int32_t check_rom_firmware_boot(struct i2c_client *client, uint8_t * raw_data_buffer)
{
	int32_t status = 0;
	uint8_t page = 0;
	uint8_t device_id = 0;
	uint8_t revision_id = 0;

	//LOG_FUNCTION_START("");
	//printf("entering check rom fw boot ....\n");

	status = stmvl53l5_write_multi(client, raw_data_buffer, PAGE_SELECT, &page, 1);
	if (status != 0)
		goto exit;

	status = stmvl53l5_read_multi(client, raw_data_buffer, 0x00, &device_id, 1);
	if (status != 0)
		goto exit;
	printk ("stmvl53l5: device id = 0x%x\n", device_id);

	status = stmvl53l5_read_multi(client, raw_data_buffer, 0x01, &revision_id, 1);
	if (status != 0)
		goto exit;
	printk ("stmvl53l5: revision id = 0x%x\n", revision_id);

	if ((device_id == 0xF0) && (revision_id == 0x02)) {
		status = _check_rom_firmware_boot_cut_1_2(client, raw_data_buffer);
		if (status != 0)
			goto exit;
	} else {
		status = -1;
		goto exit;
	}

exit:
	if  (status != 0)
		printk("stmvl53l5: check_rom_firmware_boot failed : %d\n", status);
	return status;
}


static int32_t _enable_host_access_to_go1_async(struct i2c_client *client, uint8_t * raw_data_buffer)
{
	int32_t status = 0;
	// uint32_t start_time_ms   = 0;
	// uint32_t current_time_ms = 0;
	uint8_t m_status = 0;
	uint8_t revision_id = 0;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_read_byte(client, raw_data_buffer, 0x0001, &revision_id);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x02);
	if (status != 0)
		goto exit;

	if (revision_id != 2) {
		status = -1;
		goto exit;
	}

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x03, 0x0D);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x01);
	if (status != 0)
		goto exit;

	m_status = 0;
	// status = vl53l5_get_tick_count(p_dev, &start_time_ms);
	if (status != 0)
		goto exit;

	while ((m_status & 0x10) == 0) {
		status = stmvl53l5_read_byte(client, raw_data_buffer, 0x21, &m_status);
		if (status != 0)
			goto exit;

		// status = vl53l5_get_tick_count(p_dev, &current_time_ms);
		// if (status != 0)
		//	 goto exit;

		// CHECK_FOR_TIMEOUT(
		//	status, p_dev, start_time_ms, current_time_ms,
		//	VL53L5_BOOT_COMPLETION_POLLING_TIMEOUT_MS);
		// if (status != 0) {
		//	trace_print(
		//	VL53L5_TRACE_LEVEL_ERRORS,
		//	"ERROR: timeout waiting for mcu idle m_status %02x\n",
		//	m_status);
		//	status = VL53L5_ERROR_MCU_IDLE_TIMEOUT;
		//	goto exit;
		//}
		msleep(10);
	}

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x0C, 0x01);
	if (status != 0)
		goto exit;

exit:
	if (status != 0)
		printk("stmvl53l5: _enable_host_access_to_go1_async failed: %d\n", status);
	return status;
}

static int32_t _set_to_power_on_status(struct i2c_client *client,  uint8_t * raw_data_buffer)
{
	int32_t status = 0;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x101, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x102, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x4002, 0x01);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x4002, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x103, 0x01);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x400F, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x21A, 0x43);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x21A, 0x03);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x21A, 0x01);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x21A, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x219, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x21B, 0x00);
	if (status != 0)
		goto exit;

exit:
	if (status != 0)
		printk("stmvl53l5: _set_to_power_on_status failed : %d\n", status);
	return status;
}


int32_t _wake_up_mcu(struct i2c_client *client,  uint8_t * raw_data_buffer)
{
	int32_t status = 0;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x0C, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x01);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x20, 0x07);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x20, 0x06);
	if (status != 0)
		goto exit;

exit:
	if (status != 0)
		printk("stmvl53l5: _wake_up_mcu failed : %d\n", status);
	return status;
}

int32_t _wait_for_boot_complete_before_fw_load(struct i2c_client *client, uint8_t * raw_data_buffer)
{
	int32_t status = 0;

	status = _enable_host_access_to_go1_async(client, raw_data_buffer);
	if (status != 0)
		goto exit;

	status = _set_to_power_on_status(client, raw_data_buffer);
	if (status != 0)
		goto exit;

	status = _wake_up_mcu(client, raw_data_buffer);
	if (status != 0)
		goto exit;

exit:
	if (status != 0)
		printk("stmvl53l5: _wait_for_boot_complete_before_fw_load failed : %d\n", status);
	return status;
}

int32_t _reset_mcu_and_wait_boot(struct i2c_client *client,  uint8_t * raw_data_buffer)
{
	int32_t status = 0;
	uint8_t u_start[] = {0, 0, 0x42, 0};

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_multi(client, raw_data_buffer, 0x114, u_start, 4);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x0B, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x0C, 0x00);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x0B, 0x01);
	if (status != 0)
		goto exit;

	status = wait_mcu_boot(client, NULL, 1, raw_data_buffer, 1);
	if (status != 0)
		goto exit;

exit:
	if (status != 0)
		printk("stmvl53l5: _reset_mcu_and_wait_boot failed : %d\n", status);
	return status;
}


int32_t _wait_for_boot_complete_after_fw_load(struct i2c_client *client,  uint8_t * raw_data_buffer)
{
	int32_t status = 0;

	status = _enable_host_access_to_go1_async(client, raw_data_buffer);
	if (status != 0)
		goto exit;

	status = _reset_mcu_and_wait_boot(client, raw_data_buffer);
	if (status != 0)
		goto exit;

exit:
	if (status != 0)
		printk("stmvl53l5: _wait_for_boot_complete_after_fw_load failed : %d\n", status);
	return status;
}


int32_t _write_page(
	struct i2c_client *client, uint8_t * raw_data_buffer, uint32_t page_offset,
	uint32_t page_size, uint32_t max_chunk_size, uint32_t *p_write_count)
{
	int32_t status;
	uint32_t write_size = 0;
	uint32_t remainder_size = 0;
	uint8_t *p_write_buff = NULL;

	if ((page_offset + max_chunk_size) < page_size)
		write_size = max_chunk_size;
	else
		write_size = page_size - page_offset;

	if (*p_write_count > host_dev_fw_buff_count) {

		p_write_buff = host_dev_p_comms_buff;
		memset(p_write_buff, 0, write_size);
	} else {
		if ((host_dev_fw_buff_count - *p_write_count)
				 < write_size) {

			p_write_buff = host_dev_p_comms_buff;
			remainder_size =
				host_dev_fw_buff_count - *p_write_count;
			memcpy(p_write_buff,
			       host_dev_p_fw_buff + *p_write_count,
			       remainder_size);
			memset(p_write_buff + remainder_size,
			       0,
			       write_size - remainder_size);
		} else {

			p_write_buff =
				host_dev_p_fw_buff + *p_write_count;
		}
	}

	status = stmvl53l5_write_multi(client, raw_data_buffer, page_offset, p_write_buff,
				    write_size);
	if (status != 0) {
		status = -1;
		goto exit;
	}
	(*p_write_count) += write_size;

exit:
	if (status != 0)
		 printk("stmvl53l5: _write_page failed : %d\n", status);
	return status;
}


int32_t _download_fw_to_ram(struct i2c_client *client,  uint8_t * raw_data_buffer)
{
	int32_t status = 0;

	uint16_t tdcm_offset = 0;
	uint8_t tdcm_page = 9;
	uint32_t tdcm_page_size = 0;
	uint32_t write_count = 0;

	for (tdcm_page = 9; tdcm_page < 12; tdcm_page++) {
		status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, tdcm_page);
		if (status != 0)
			goto exit;

		if (tdcm_page == 9)
			tdcm_page_size = 0x8000;
		if (tdcm_page == 10)
			tdcm_page_size = 0x8000;
		if (tdcm_page == 11)
			tdcm_page_size = 0x5000;

		for (tdcm_offset = 0; tdcm_offset < tdcm_page_size;
				tdcm_offset += VL53L5_COMMS_BUFFER_SIZE_BYTES) {
			status = _write_page(
				client, raw_data_buffer, tdcm_offset, tdcm_page_size,
				VL53L5_COMMS_BUFFER_SIZE_BYTES, &write_count);
			if (status != 0)
				goto exit;
		}
	}

exit:
	if (status != 0)
		printk("stmvl53l5: _download_fw_to_ram failed : %d\n", status);
	return status;
}


int32_t load_firmware(struct i2c_client *client,  uint8_t * raw_data_buffer)
{
	int32_t status = 0;

	status = _wait_for_boot_complete_before_fw_load(client, raw_data_buffer);
	if (status != 0)
		goto exit;

	status = _download_fw_to_ram(client, raw_data_buffer);
	if (status != 0)
		goto exit;

	status = _wait_for_boot_complete_after_fw_load(client, raw_data_buffer);
	if (status != 0)
		goto exit;

	status = stmvl53l5_write_byte(client, raw_data_buffer, 0x7FFF, 0x02);
	if (status != 0)
		goto exit;

exit:
	printk("stmvl53l5: load_firmware : GO2s0=0x%x GO2s1=0x%x status=%d\n", VL53L5_GO2_STATUS_0(&dev).bytes, VL53L5_GO2_STATUS_1(&dev).bytes, status);
	return status;
}


int32_t stmvl53l5_load_fw_stm(struct i2c_client *client, uint8_t * raw_data_buffer)
{

	int status = 0;

	VL53L5_ASSIGN_FW_BUFF(_fw_buffer, sizeof(_fw_buffer));
	VL53L5_ASSIGN_COMMS_BUFF(_comms_buffer, sizeof(_comms_buffer));
	status = check_rom_firmware_boot(client, raw_data_buffer);
	status |= load_firmware(client, raw_data_buffer);
	printk("stmvl53l5: load_fw : %d\n", status);

	return status;

}

int32_t stmvl53l5_move_device_to_low_power(struct i2c_client *client, struct spi_data_t *spi_data, uint8_t i2c_not_spi, uint8_t * raw_data_buffer)
{

	int status = 0;
	uint8_t page = 0;
	uint8_t reg_val = 0;

	// put the device in low power idle with comms enabled
	if (i2c_not_spi)
		status = stmvl53l5_write_multi(client, raw_data_buffer, PAGE_SELECT, &page, 1);
	else
		status = stmvl53l5_spi_write(spi_data, PAGE_SELECT, &page, 1);
	if (status != 0)
		return -1;

	reg_val = 0;
	if (i2c_not_spi)
		status = stmvl53l5_read_multi(client, raw_data_buffer, XSHUT_CTRL, &reg_val, 1);
	else
		status = stmvl53l5_spi_write(spi_data, XSHUT_CTRL, &reg_val, 1);

	if (status != 0)
		return -1;

	MASK_XSHUT_REGISTER(reg_val, 0);

 	if (i2c_not_spi)
		status = stmvl53l5_write_multi(client, raw_data_buffer, XSHUT_CTRL, &reg_val, 1);
	else
		status = stmvl53l5_spi_write(spi_data, XSHUT_CTRL, &reg_val, 1);

	status = wait_mcu_boot(client, spi_data, i2c_not_spi, raw_data_buffer, 0);
	if (status != 0)
		return -1;

	printk("stmvl53l5: device set in low power (idle with comms) : %d\n", status);

	return status;

}

