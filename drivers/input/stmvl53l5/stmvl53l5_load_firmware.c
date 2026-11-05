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

#include <linux/firmware.h>
#include <linux/completion.h>

#include "stmvl53l5_load_firmware.h"
#include "stmvl53l5_platform.h"
#include "stmvl53l5_register_utils.h"
#include "stmvl53l5_logging.h"
#include "stmvl53l5_error_codes.h"

#define CHECK_FOR_TIMEOUT(status, p_dev, start_ms, end_ms, timeout_ms) \
	(status = stmvl53l5_check_for_timeout(\
		(p_dev), start_ms, end_ms, timeout_ms))

#define STMVL53L5_COMMS_BUFFER_SIZE_BYTES	5132

#define STMVL53L5_ASSIGN_COMMS_BUFF(p_dev, comms_buff_ptr, max_count) \
do {\
	(p_dev)->host_dev.p_comms_buff = (comms_buff_ptr);\
	(p_dev)->host_dev.comms_buff_max_count = (max_count);\
} while (0)

#define STMVL53L5_ISNULL(ptr) ((ptr) == NULL)

#define STMVL53L5_COMMS_BUFF_ISNULL(p_dev)\
	((p_dev)->host_dev.p_comms_buff == NULL)

#define STMVL53L5_COMMS_BUFF_ISEMPTY(p_dev)\
	((p_dev)->host_dev.comms_buff_count == 0)

#define STMVL53L5_FW_BUFF_ISNULL(p_dev)\
	((p_dev)->host_dev.p_fw_buff == NULL)

#define STMVL53L5_FW_BUFF_ISEMPTY(p_dev)\
	((p_dev)->host_dev.fw_buff_count == 0)

#define STMVL53L5_HW_TRAP(p_dev)\
	(STMVL53L5_GO2_STATUS_0(p_dev).mcu__hw_trap_flag_go1 == 1)

#define STMVL53L5_MCU_ERROR(p_dev)\
	(STMVL53L5_GO2_STATUS_0(p_dev).mcu__error_flag_go1 == 1)

#define STMVL53L5_MCU_BOOT_COMPLETE(p_dev)\
	(STMVL53L5_GO2_STATUS_0(p_dev).mcu__boot_complete_go1 == 1)

#define STMVL53L5_MCU_BOOT_NOT_COMPLETE(p_dev)\
	(STMVL53L5_GO2_STATUS_0(p_dev).mcu__boot_complete_go1 == 0)

#define STMVL53L5_MAX_FW_FILE_SIZE 100000
#define STMVL53L5_WRITE_CHUNK_SIZE(p_dev) \
	((p_dev)->host_dev.comms_buff_max_count)

#define DCI_UI__FIRMWARE_CHECKSUM_IDX ((unsigned int) 0x812FFC)
#define BYTE_4 4

static int _stmvl53l5_write_byte(struct stmvl53l5_dev_t *p_dev,
				 unsigned short address, unsigned char value)
{
	return stmvl53l5_write_multi(p_dev, address, &value, 1);
}

static int _stmvl53l5_read_byte(struct stmvl53l5_dev_t *p_dev,
				unsigned short address, unsigned char *p_value)
{
	return stmvl53l5_read_multi(p_dev, address, p_value, 1);
}

static int _stmvl53l5_write_page(struct stmvl53l5_dev_t *p_dev,
				 unsigned char *p_buffer,
				 unsigned short page_offset,
				 unsigned int page_size,
				 unsigned int max_chunk_size,
				 unsigned int *p_write_count)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned int write_size = 0;
	unsigned int remainder_size = 0;
	unsigned char *p_write_buff = NULL;

	if ((page_offset + max_chunk_size) < page_size)
		write_size = max_chunk_size;
	else
		write_size = page_size - page_offset;

	if (*p_write_count > p_dev->host_dev.fw_buff_count) {

		p_write_buff = p_dev->host_dev.p_comms_buff;
		memset(p_write_buff, 0, write_size);
	} else {
		if ((p_dev->host_dev.fw_buff_count - *p_write_count)
				 < write_size) {

			p_write_buff = p_dev->host_dev.p_comms_buff;
			remainder_size =
				p_dev->host_dev.fw_buff_count - *p_write_count;
			memcpy(p_write_buff,
			       p_buffer + *p_write_count,
			       remainder_size);
			memset(p_write_buff + remainder_size,
			       0,
			       write_size - remainder_size);
		} else {

			p_write_buff = p_buffer + *p_write_count;
		}
	}
	status = stmvl53l5_write_multi(p_dev, page_offset, p_write_buff,
				    write_size);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	(*p_write_count) += write_size;

exit:
	return status;
}

static int _stmvl53l5_write_data_to_ram(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	unsigned short tcpm_page_offset =
			p_dev->host_dev.patch_data.tcpm_page_offset;
	unsigned char tcpm_page = p_dev->host_dev.patch_data.tcpm_page;
	unsigned char current_page = 0;
	unsigned int tcpm_page_size = 0;
	unsigned int write_count = 0;
	unsigned int tcpm_offset = p_dev->host_dev.patch_data.tcpm_offset;
	unsigned int tcpm_size = p_dev->host_dev.patch_data.tcpm_size;
	unsigned int current_size = tcpm_size;
	unsigned char *write_buff = NULL;

	LOG_FUNCTION_START("");

	p_dev->host_dev.fw_buff_count = tcpm_size;

	write_buff = &p_dev->host_dev.p_fw_buff[tcpm_offset];

	p_dev->host_dev.firmware_load = true;

	for (current_page = tcpm_page; current_page < 12; current_page++) {
		status = _stmvl53l5_write_byte(p_dev, 0x7FFF, current_page);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;

		if (current_page == 9)
			tcpm_page_size = 0x8000;
		if (current_page == 10)
			tcpm_page_size = 0x8000;
		if (current_page == 11)
			tcpm_page_size = 0x5000;
		if (current_size < tcpm_page_size)
			tcpm_page_size = current_size;

		for (tcpm_page_offset = 0; tcpm_page_offset < tcpm_page_size;
				tcpm_page_offset +=
				STMVL53L5_WRITE_CHUNK_SIZE(p_dev)) {
			status = _stmvl53l5_write_page(p_dev, write_buff,
				tcpm_page_offset, tcpm_page_size,
				STMVL53L5_WRITE_CHUNK_SIZE(p_dev),
				&write_count);
			if (status != STMVL53L5_ERROR_NONE)
				goto exit;
		}

		if (write_count == tcpm_size)
			break;
		current_size -= write_count;
	}

exit:
	p_dev->host_dev.firmware_load = false;

	LOG_FUNCTION_END(status);
	return status;
}

static unsigned int _stmvl53l5_decode_uint32_t(
	unsigned short count,
	unsigned char *pbuffer)
{
	unsigned int value = 0x00;

	while (count-- > 0)
		value = (value << 8) | (unsigned int)pbuffer[count];

	return value;
}

static void _stmvl53l5_decode_patch_struct(struct stmvl53l5_dev_t *p_dev)
{
	unsigned char *p_buff = p_dev->host_dev.p_fw_buff;

	LOG_FUNCTION_START("");

	p_dev->host_dev.patch_data.patch_ver_major =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		    "p_dev->host_dev.patch_data.patch_ver_major: 0x%x\n",
		    p_dev->host_dev.patch_data.patch_ver_major);

	p_dev->host_dev.patch_data.patch_ver_minor =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		    "p_dev->host_dev.patch_data.patch_ver_minor: 0x%x\n",
		    p_dev->host_dev.patch_data.patch_ver_minor);

	p_dev->host_dev.patch_data.patch_ver_build =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		    "p_dev->host_dev.patch_data.patch_ver_build: 0x%x\n",
		    p_dev->host_dev.patch_data.patch_ver_build);

	p_dev->host_dev.patch_data.patch_ver_revision =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		    "p_dev->host_dev.patch_data.patch_ver_revision: 0x%x\n",
		    p_dev->host_dev.patch_data.patch_ver_revision);

	p_buff += BYTE_4;

	p_dev->host_dev.patch_data.patch_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_offset: 0x%x\n",
		p_dev->host_dev.patch_data.patch_offset);

	p_dev->host_dev.patch_data.patch_size =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_size: 0x%x\n",
		p_dev->host_dev.patch_data.patch_size);

	p_dev->host_dev.patch_data.patch_checksum =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_checksum: 0x%x\n",
		p_dev->host_dev.patch_data.patch_checksum);

	p_dev->host_dev.patch_data.tcpm_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.tcpm_offset: 0x%x\n",
		p_dev->host_dev.patch_data.tcpm_offset);

	p_dev->host_dev.patch_data.tcpm_size =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.tcpm_size: 0x%x\n",
		p_dev->host_dev.patch_data.tcpm_size);

	p_dev->host_dev.patch_data.tcpm_page =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.tcpm_page: 0x%x\n",
		p_dev->host_dev.patch_data.tcpm_page);

	p_dev->host_dev.patch_data.tcpm_page_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.tcpm_page_offset: 0x%x\n",
		p_dev->host_dev.patch_data.tcpm_page_offset);

	p_buff += 48;

	p_dev->host_dev.patch_data.checksum_en_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.checksum_en_offset: 0x%x\n",
		p_dev->host_dev.patch_data.checksum_en_offset);

	p_dev->host_dev.patch_data.checksum_en_size =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.checksum_en_size: 0x%x\n",
		p_dev->host_dev.patch_data.checksum_en_size);

	p_dev->host_dev.patch_data.checksum_en_page =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.checksum_en_page: 0x%x\n",
		p_dev->host_dev.patch_data.checksum_en_page);

	p_dev->host_dev.patch_data.checksum_en_page_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.checksum_en_page_offset: 0x%x\n",
		p_dev->host_dev.patch_data.checksum_en_page_offset);

	p_dev->host_dev.patch_data.patch_code_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_code_offset: 0x%x\n",
		p_dev->host_dev.patch_data.patch_code_offset);

	p_dev->host_dev.patch_data.patch_code_size =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_code_size: 0x%x\n",
		p_dev->host_dev.patch_data.patch_code_size);

	p_dev->host_dev.patch_data.patch_code_page =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_code_page: 0x%x\n",
		p_dev->host_dev.patch_data.patch_code_page);

	p_dev->host_dev.patch_data.patch_code_page_offset =
		_stmvl53l5_decode_uint32_t(BYTE_4, p_buff);
	p_buff += BYTE_4;
	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		"p_dev->host_dev.patch_data.patch_code_page_offset: 0x%x\n",
		p_dev->host_dev.patch_data.patch_code_page_offset);

	LOG_FUNCTION_END(0);
}

static int _stmvl53l5_write_patch_code(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned short page =
		(unsigned short)p_dev->host_dev.patch_data.patch_code_page;
	unsigned short addr =
		(unsigned short)p_dev->host_dev.patch_data.patch_code_page_offset;
	unsigned int size = p_dev->host_dev.patch_data.patch_code_size;
	unsigned int offset = p_dev->host_dev.patch_data.patch_code_offset;
	unsigned char *reg_data = &p_dev->host_dev.p_fw_buff[offset];

	LOG_FUNCTION_START("");

	stmvl53l5_trace_print(VL53L5_TRACE_LEVEL_DEBUG,
			      "page: 0x%x addr: 0x%x\n", page, addr);

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, page);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_write_multi(p_dev, addr, reg_data, size);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_write_checksum_en(
		struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned short page =
		(unsigned short)p_dev->host_dev.patch_data.checksum_en_page;
	unsigned short addr =
		(unsigned short)p_dev->host_dev.patch_data.checksum_en_page_offset;
	unsigned int size = p_dev->host_dev.patch_data.checksum_en_size;
	unsigned int offset = p_dev->host_dev.patch_data.checksum_en_offset;
	unsigned char *reg_data = &p_dev->host_dev.p_fw_buff[offset];

	LOG_FUNCTION_START("");

	stmvl53l5_trace_print(VL53L5_TRACE_LEVEL_DEBUG,
			      "page: 0x%x addr: 0x%x\n", page, addr);

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, page);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_write_multi(p_dev, addr, reg_data, size);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_check_fw_checksum(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned int checksum = 0;
	unsigned int expected_checksum =
		p_dev->host_dev.patch_data.patch_checksum;
	unsigned char data[4] = {0};
	unsigned short ui_addr =
		(unsigned short)(DCI_UI__FIRMWARE_CHECKSUM_IDX & 0xFFFF);

	LOG_FUNCTION_START("");

	data[0] = 0;
	status = stmvl53l5_read_multi(p_dev, ui_addr, data, 4);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	checksum = (unsigned int)((data[0] << 24) |
				  (data[1] << 16) |
				  (data[2] << 8) |
				  data[3]);

	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
		    "Expected Checksum: 0x%x Actual Checksum: 0x%x\n",
		    expected_checksum, checksum);

	if (checksum != expected_checksum) {
		status = STMVL53L5_ERROR_INIT_FW_CHECKSUM;
		goto exit;
	}

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_enable_host_access_to_go1_async(
					struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned int start_time_ms   = 0;
	unsigned int current_time_ms = 0;
	unsigned char m_status = 0;

	LOG_FUNCTION_START("");

	if (p_dev->host_dev.revision_id == 2) {
		status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x02);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
		status = _stmvl53l5_write_byte(p_dev, 0x03, 0x12);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
		status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x01);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	} else {
		status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x01);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
		status = _stmvl53l5_write_byte(p_dev, 0x06, 0x01);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;
	}

	m_status = 0;
	status = stmvl53l5_get_tick_count(p_dev, &start_time_ms);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	while ((m_status & 0x04) == 0) {
		status = _stmvl53l5_read_byte(p_dev, 0x21, &m_status);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;

		status = stmvl53l5_get_tick_count(p_dev, &current_time_ms);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit;

		CHECK_FOR_TIMEOUT(
			status, p_dev, start_time_ms, current_time_ms,
			STMVL53L5_BOOT_COMPLETION_POLLING_TIMEOUT_MS);
		if (status < STMVL53L5_ERROR_NONE) {
			stmvl53l5_trace_print(
			STMVL53L5_TRACE_LEVEL_ERRORS,
			"ERROR: timeout waiting for mcu idle m_status %02x\n",
			m_status);
			status = STMVL53L5_ERROR_MCU_IDLE_TIMEOUT;
			goto exit;
		}
	}

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x0C, 0x01);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;
exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_reset_mcu_and_wait_boot(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned char u_start[] = {0, 0, 0x42, 0};

	LOG_FUNCTION_START("");

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_write_multi(p_dev, 0x114, u_start, 4);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x0B, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x0C, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x0B, 0x01);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_wait_mcu_boot(p_dev, STMVL53L5_BOOT_STATE_HIGH,
				      0, STMVL53L5_MCU_BOOT_WAIT_DELAY);

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_disable_pll(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x400F, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x21A, 0x43);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x21A, 0x03);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x21A, 0x01);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x21A, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x219, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x21B, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_set_to_power_on_status(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x101, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x102, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x010a, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x4002, 0x01);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x4002, 0x00);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x103, 0x01);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_write_byte(p_dev, 0x010a, 0x03);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_wait_for_boot_complete_before_fw_load(
	struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = _stmvl53l5_enable_host_access_to_go1_async(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_set_to_power_on_status(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_wait_for_boot_complete_after_fw_load(
	struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = _stmvl53l5_disable_pll(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_reset_mcu_and_wait_boot(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

int stmvl53l5_load_firmware(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
			      "\n\n#### load_firmware ####\n\n");

	if (STMVL53L5_ISNULL(p_dev)) {
		status = STMVL53L5_ERROR_INVALID_PARAMS;
		goto exit;
	}
	if (STMVL53L5_FW_BUFF_ISNULL(p_dev)) {
		status = STMVL53L5_ERROR_FW_BUFF_NOT_FOUND;
		goto exit;
	}
	if (STMVL53L5_FW_BUFF_ISEMPTY(p_dev)) {
		status = STMVL53L5_ERROR_FW_BUFF_NOT_FOUND;
		goto exit;
	}
	if (STMVL53L5_COMMS_BUFF_ISNULL(p_dev)) {
		status = STMVL53L5_ERROR_INVALID_PARAMS;
		goto exit;
	}

	_stmvl53l5_decode_patch_struct(p_dev);

	status = _stmvl53l5_wait_for_boot_complete_before_fw_load(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_write_data_to_ram(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_write_patch_code(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_write_checksum_en(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_wait_for_boot_complete_after_fw_load(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_write_byte(p_dev, 0x7FFF, 0x02);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_check_fw_checksum(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

exit_change_page:
	if (status < STMVL53L5_ERROR_NONE)

		(void)_stmvl53l5_write_byte(p_dev, 0x7FFF, 0x02);

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_check_device_booted(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	STMVL53L5_GO2_STATUS_1(p_dev).bytes = 0;

	status = _stmvl53l5_read_byte(p_dev, 0x07,
					&STMVL53L5_GO2_STATUS_1(p_dev).bytes);
	if (status != STMVL53L5_ERROR_NONE)
		goto exit;

	if (p_dev->host_dev.revision_id == 0x0c) {
		if (STMVL53L5_MCU_FIRST_BOOT_COMPLETE_REVISION_C(p_dev))
			p_dev->host_dev.device_booted = true;
		else
			p_dev->host_dev.device_booted = false;
	} else {
		if (STMVL53L5_MCU_FIRST_BOOT_COMPLETE(p_dev))
			p_dev->host_dev.device_booted = true;
		else
			p_dev->host_dev.device_booted = false;
	}

exit:
	return status;
}

static int _check_rom_firmware_boot(
	struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	status = stmvl53l5_wait_mcu_boot(p_dev, STMVL53L5_BOOT_STATE_HIGH,
					 0, 0);

	(void)_stmvl53l5_write_byte(p_dev, 0x000E, 0x01);

	return status;
}

int stmvl53l5_check_rom_firmware_boot(struct stmvl53l5_dev_t *p_dev)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (STMVL53L5_ISNULL(p_dev)) {
		status = STMVL53L5_ERROR_INVALID_PARAMS;
		goto exit;
	}

	status = stmvl53l5_set_page(p_dev, GO2_PAGE);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = _stmvl53l5_read_byte(p_dev, 0x00, &p_dev->host_dev.device_id);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_read_byte(
		p_dev, 0x01, &p_dev->host_dev.revision_id);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	status = _stmvl53l5_check_device_booted(p_dev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_change_page;

	if (p_dev->host_dev.device_booted == true)
		goto exit_change_page;

	if ((p_dev->host_dev.device_id == 0xF0) &&
	    (p_dev->host_dev.revision_id == 0x02 ||
	     p_dev->host_dev.revision_id == 0x0C)) {
		stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_INFO,
			    "device id 0x%x revision id 0x%x\n",
			    p_dev->host_dev.device_id,
			    p_dev->host_dev.revision_id);
		status = _check_rom_firmware_boot(p_dev);
		if (status < STMVL53L5_ERROR_NONE)
			goto exit_change_page;
	} else {
		stmvl53l5_trace_print(STMVL53L5_TRACE_LEVEL_ERRORS,
			    "Unsupported device type\n");

		status = STMVL53L5_UNKNOWN_SILICON_REVISION;
		goto exit_change_page;
	}

exit_change_page:

	if (status != STMVL53L5_ERROR_NONE)
		(void)stmvl53l5_set_page(p_dev, DEFAULT_PAGE);
	else
		status = stmvl53l5_set_page(p_dev, DEFAULT_PAGE);

exit:
	LOG_FUNCTION_END(status);
	return status;
}

static int _stmvl53l5_load_fw(struct stmvl53l5_dev_t *pdev, const struct firmware *fw)
{
	int status = 0;
	unsigned char *comms_buffer = NULL;
	unsigned char *fw_data;
	const struct stmvl53l5_fw_header_t *header;
	unsigned char *fw_buffer = NULL;

	fw_data = (unsigned char *)fw->data;
	header = (struct stmvl53l5_fw_header_t *)fw_data;

	STMVL53L5_LOG_INFO("Binary ver %i.%i.%i.%i",
		header->binary_ver_major,
		header->binary_ver_minor,
		header->binary_ver_build,
		header->binary_ver_revision);

	fw_buffer = kzalloc(header->fw_size, GFP_DMA | GFP_KERNEL);
	if (IS_ERR(fw_buffer)) {
				status = PTR_ERR(comms_buffer);
				  goto exit;
	}
	pdev->host_dev.fw_buff_count = header->fw_size;
	pdev->host_dev.p_fw_buff = fw_buffer;
	memcpy(fw_buffer, &fw_data[header->fw_offset], pdev->host_dev.fw_buff_count);

	comms_buffer = kzalloc(STMVL53L5_COMMS_BUFFER_SIZE_BYTES,
				GFP_DMA | GFP_KERNEL);
	if (IS_ERR(comms_buffer)) {
		status = PTR_ERR(comms_buffer);
		goto exit;
	}

	STMVL53L5_ASSIGN_COMMS_BUFF(pdev, comms_buffer,
				STMVL53L5_COMMS_BUFFER_SIZE_BYTES);

	status = stmvl53l5_load_firmware(pdev);

	pdev->host_dev.p_fw_buff = NULL;

	kfree(fw_buffer);
	kfree(comms_buffer);

exit:
	return status;
}

static void _stmvl53l5_fw_complete(const struct firmware *fw, void *context)
{
	struct stmvl53l5_module_t *dev = context;

	if (fw != NULL)

		dev->fw_comp_status = _stmvl53l5_load_fw(&dev->stdev, fw);
	else {
		dev->fw_comp_status = -ENOENT;
		STMVL53L5_LOG_ERROR("failed to load file %s\n", dev->firmware_name);
	}

	if (fw != NULL)
		release_firmware(fw);

	complete(&dev->fw_comp);
}

int stmvl53l5_load_fw_stm(struct stmvl53l5_dev_t *pdev)
{
	int status = STMVL53L5_ERROR_NONE;
	const char *fw_path;
	struct device *device = NULL;
	struct stmvl53l5_module_t *p_module;

	p_module = (struct stmvl53l5_module_t *)
		container_of(pdev, struct stmvl53l5_module_t, stdev);

	if (p_module->comms_type == STMVL53L5_SPI) {
		device = &p_module->device->dev;
	} else if (p_module->comms_type == STMVL53L5_I2C) {
		device = &p_module->client->dev;
	}
	fw_path = p_module->firmware_name;

	STMVL53L5_LOG_DEBUG("Req FW : %s", fw_path);

	init_completion(&p_module->fw_comp);
	p_module->fw_comp_status = STMVL53L5_ERROR_NONE;

	status = request_firmware_nowait(THIS_MODULE, 1,
					fw_path,
					device,
					GFP_KERNEL,
					p_module,
					_stmvl53l5_fw_complete);

	wait_for_completion(&p_module->fw_comp);

	if (p_module->fw_comp_status < STMVL53L5_ERROR_NONE)
		status = p_module->fw_comp_status;

	return status;
}
