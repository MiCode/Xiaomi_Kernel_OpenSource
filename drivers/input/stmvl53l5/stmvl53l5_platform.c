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

#include <linux/spi/spi.h>
#include <linux/i2c.h>
#ifdef STM_VL53L5_GPIO_ENABLE
#include <linux/gpio.h>
#endif
#include <linux/delay.h>

#include "stmvl53l5_platform.h"
#include "stmvl53l5_logging.h"
#include "stmvl53l5_error_codes.h"

#define GET_CHUNKS_REQD(size, chunk_size) \
	((size / chunk_size) + ((size % chunk_size) != 0))

#define CURRENT_CHUNK_BYTES(chunk, chunks_reqd, size, chunk_size) \
	(((chunk + 1) * chunk_size) > size ? \
	(size - chunk * chunk_size) : \
	chunk_size)

#define SPI_READWRITE_BIT 0x8000

#define SPI_WRITE_MASK(x) (x | SPI_READWRITE_BIT)
#define SPI_READ_MASK(x)  (x & ~SPI_READWRITE_BIT)

#ifdef STM_VL53L5_GPIO_ENABLE
static int _set_gpio(int *p_gpio, char value)
{
	int status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (*p_gpio != -1) {
		gpio_set_value(*p_gpio, value);
		stmvl53l5_log_debug("Gpio %d value %d", *p_gpio, value);
	} else {
		stmvl53l5_log_error("Failed. Gpio not available");
		status = -99999;
	}

	LOG_FUNCTION_START("");

	return status;
}
#endif

static uint32_t _calculate_twos_complement_uint32(uint32_t value)
{
	uint32_t twos_complement = 0;

	twos_complement = ~value + 0x01;

	return twos_complement;
}

static int _i2c_write(struct stmvl53l5_module_t *module, int index,
		      uint8_t *data, uint16_t len)
{
	int status = STMVL53L5_ERROR_NONE;
	uint8_t *p_buffer = NULL;
	struct i2c_msg msg;

	p_buffer = kzalloc(VL53L5_COMMS_CHUNK_SIZE + 2, GFP_KERNEL);
	if (p_buffer == NULL) {
		stmvl53l5_log_error("Failed to allocated write buffer\n");
		status = -ENOMEM;
		goto exit;
	}

	if (len > VL53L5_COMMS_CHUNK_SIZE || len == 0) {
		stmvl53l5_log_error("invalid len %d\n", len);
		status = -1;
		goto exit;
	}

	p_buffer[0] = (index >> 8) & 0xFF;
	p_buffer[1] = (index >> 0) & 0xFF;

	memcpy(p_buffer + 2, data, len);

	msg.addr = module->client->addr;
	msg.flags = module->client->flags;
	msg.buf = p_buffer;
	msg.len = len + 2;

	status = i2c_transfer(module->client->adapter, &msg, 1);

	if (status != 1) {
		stmvl53l5_log_error(
			"i2c_transfer error :%d, index 0x%x len %d addr %x",
			status, index, len, module->client->addr);
	}
	status = (status != 1) ? STMVL53L5_COMMS_ERROR : STMVL53L5_ERROR_NONE;

exit:
	kfree(p_buffer);
	return status;
}

static int _i2c_read(struct stmvl53l5_module_t *module, int index,
		uint8_t *data, uint16_t len)
{
	int status = STMVL53L5_ERROR_NONE;
	uint8_t buffer[2];
	struct i2c_msg msg[2];

	if (len > VL53L5_COMMS_CHUNK_SIZE || len == 0) {
		stmvl53l5_log_error("invalid len %d\n", len);
		return STMVL53L5_COMMS_ERROR;
	}

	buffer[0] = (index >> 8) & 0xFF;
	buffer[1] = (index >> 0) & 0xFF;

	msg[0].addr = module->client->addr;
	msg[0].flags = module->client->flags;
	msg[0].buf = buffer;
	msg[0].len = 2;

	msg[1].addr = module->client->addr;
	msg[1].flags = I2C_M_RD | module->client->flags;
	msg[1].buf = data;
	msg[1].len = len;

	status = i2c_transfer(module->client->adapter, msg, 2);

	if (status != 2) {
		stmvl53l5_log_error(
			"i2c_transfer :%d, @%x index 0x%x len %d\n",
			status, module->client->addr, index,
			len);

	}
	status = (status != 2) ? STMVL53L5_COMMS_ERROR : STMVL53L5_ERROR_NONE;
	return status;
}

static int _spi_write(struct stmvl53l5_module_t *p_module, int index,
		      uint8_t *data, uint16_t len)
{
	int status = STMVL53L5_ERROR_NONE;
	uint8_t index_bytes[2] = {0};
	struct spi_message m;
	struct spi_transfer t[2];

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	index_bytes[0] = ((SPI_WRITE_MASK(index) & 0xff00) >> 8);
	index_bytes[1] = (SPI_WRITE_MASK(index) & 0xff);

	t[0].tx_buf = index_bytes;
	t[0].len = 2;

	t[1].tx_buf = data;
	t[1].len = (unsigned int)len;

	spi_message_add_tail(&t[0], &m);
	spi_message_add_tail(&t[1], &m);

	status = spi_sync(p_module->device, &m);
	if (status != 0) {
		stmvl53l5_log_error("spi_sync failed. %d", status);
		goto exit;
	}

exit:
	return status;
}

static int _spi_read(struct stmvl53l5_module_t *p_module, int index,
		     uint8_t *data, uint16_t len)
{
	int status = STMVL53L5_ERROR_NONE;
	uint8_t index_bytes[2] = {0};
	struct spi_message m;
	struct spi_transfer t[2];

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	index_bytes[0] = ((SPI_READ_MASK(index) >> 8) & 0xff);
	index_bytes[1] = (SPI_READ_MASK(index) & 0xff);

	t[0].tx_buf = index_bytes;
	t[0].len = 2;

	t[1].rx_buf = data;
	t[1].len = (unsigned int)len;

	spi_message_add_tail(&t[0], &m);
	spi_message_add_tail(&t[1], &m);

	status = spi_sync(p_module->device, &m);
	if (status != 0) {
		stmvl53l5_log_error("spi_sync failed. %d", status);
		goto exit;
	}

exit:
	return status;
}

int32_t stmvl53l5_write_multi(
	struct stmvl53l5_dev_t *pdev,
	uint16_t index,
	uint8_t *pdata,
	uint32_t count)
{
	struct stmvl53l5_module_t *p_module;
	uint16_t chunk_size = 0;
	uint16_t chunks_reqd = 0;
	uint16_t cur_bytes = 0;
	uint16_t offset = 0;
	uint16_t i = 0;
	uint16_t status = STMVL53L5_ERROR_NONE;

	p_module = (struct stmvl53l5_module_t *)
		container_of(pdev, struct stmvl53l5_module_t, stdev);

	chunk_size = VL53L5_COMMS_CHUNK_SIZE;
	chunks_reqd = GET_CHUNKS_REQD(count, chunk_size);

	for (i = 0; i < chunks_reqd; i++) {
		cur_bytes = CURRENT_CHUNK_BYTES(i, chunks_reqd, count,
						chunk_size);
		offset = i * chunk_size;

		if (p_module->comms_type == STMVL53L5_I2C) {
			status = _i2c_write(p_module, index + offset,
						pdata + offset, cur_bytes);
		} else if (p_module->comms_type == STMVL53L5_SPI) {
			status = _spi_write(p_module, index + offset,
						pdata + offset, cur_bytes);
		} else {
			status = STMVL53L5_COMMS_ERROR;
		}

		if (status != 0)
			stmvl53l5_log_error("Write failure: %d\n", status);
	}

	return status;
}

int32_t stmvl53l5_read_multi(
	struct stmvl53l5_dev_t *pdev,
	uint16_t index,
	uint8_t *pdata,
	uint32_t count)
{
	struct stmvl53l5_module_t *p_module;
	uint16_t chunk_size = 0;
	uint16_t chunks_reqd = 0;
	uint16_t cur_bytes = 0;
	uint16_t offset = 0;
	uint16_t i = 0;
	uint16_t status = STMVL53L5_ERROR_NONE;

	p_module = (struct stmvl53l5_module_t *)
		container_of(pdev, struct stmvl53l5_module_t, stdev);

	chunk_size = VL53L5_COMMS_CHUNK_SIZE;
	chunks_reqd = GET_CHUNKS_REQD(count, chunk_size);

	for (i = 0; i < chunks_reqd; i++) {
		cur_bytes = CURRENT_CHUNK_BYTES(i, chunks_reqd, count,
						chunk_size);
		offset = i * chunk_size;
		if (p_module->comms_type == STMVL53L5_I2C) {
			status = _i2c_read(p_module, index + offset,
						pdata + offset, cur_bytes);
		} else if (p_module->comms_type == STMVL53L5_SPI) {
			status = _spi_read(p_module, index + offset,
						pdata + offset, cur_bytes);
		} else {
			status = STMVL53L5_COMMS_ERROR;
		}

		if (status != 0)
			stmvl53l5_log_error("Read failure: %d\n", status);
	}

	return status;
}

int32_t stmvl53l5_wait_us(
	struct stmvl53l5_dev_t *pdev, uint32_t wait_us)
{
	int32_t status = STMVL53L5_ERROR_NONE;

	if (wait_us < 10)
		udelay(wait_us);
	else if (wait_us < 20000)
		usleep_range(wait_us, wait_us + 1);
	else
		msleep(wait_us / 1000);

	return status;
}

int32_t stmvl53l5_wait_ms(
	struct stmvl53l5_dev_t *pdev, uint32_t wait_ms)
{
	int32_t status = STMVL53L5_ERROR_NONE;

	status = stmvl53l5_wait_us(pdev, wait_ms * 1000);

	return status;
}

#ifdef STM_VL53L5_GPIO_ENABLE

int32_t stmvl53l5_gpio_low_power_control(
	struct stmvl53l5_dev_t *pdev, uint8_t value)
{
	int32_t status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_module_t *p_module = NULL;

	LOG_FUNCTION_START("");

	p_module = (struct stmvl53l5_module_t *)
		container_of(pdev, struct stmvl53l5_module_t, stdev);

	stmvl53l5_log_debug("Setting gpio low power (%d) to %d\n",
			   p_module->gpio->lpn_gpio_nb, value);
	status = _set_gpio(&p_module->gpio->lpn_gpio_nb, value);

	LOG_FUNCTION_END(status);
	return status;
}

int32_t stmvl53l5_gpio_power_enable(
	struct stmvl53l5_dev_t *pdev, uint8_t value)
{
	int32_t status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_module_t *p_module = NULL;

	LOG_FUNCTION_START("");

	p_module = (struct stmvl53l5_module_t *)
		container_of(pdev, struct stmvl53l5_module_t, stdev);

	stmvl53l5_log_debug("Setting gpio power enable (%d) to %d\n",
			   p_module->gpio->pwren_gpio_nb, value);
	status = _set_gpio(&p_module->gpio->pwren_gpio_nb, value);

	LOG_FUNCTION_END(status);
	return status;
}

int32_t stmvl53l5_gpio_comms_select(
	struct stmvl53l5_dev_t *pdev, uint8_t value)
{
	int32_t status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_module_t *p_module = NULL;

	LOG_FUNCTION_START("");

	p_module = (struct stmvl53l5_module_t *)
		container_of(pdev, struct stmvl53l5_module_t, stdev);

	stmvl53l5_log_debug("Setting gpio comms select (%d) to %d\n",
			    p_module->gpio->comms_gpio_nb, value);
	status = _set_gpio(&p_module->gpio->comms_gpio_nb, value);

	LOG_FUNCTION_END(status);
	return status;
}
#endif

int32_t stmvl53l5_get_tick_count(
	struct stmvl53l5_dev_t *pdev,
	uint32_t *ptime_ms)
{
	*ptime_ms = jiffies_to_msecs(jiffies);
	return 0;
}

int32_t stmvl53l5_check_for_timeout(
	struct stmvl53l5_dev_t *pdev,
	uint32_t start_time_ms,
	uint32_t end_time_ms,
	uint32_t timeout_ms)
{
	int32_t status = STMVL53L5_ERROR_NONE;
	uint32_t time_diff_ms;

	if (start_time_ms <= end_time_ms) {
		time_diff_ms = end_time_ms - start_time_ms;
	} else {
		time_diff_ms = _calculate_twos_complement_uint32(start_time_ms)
			       + end_time_ms;
	}
	if (time_diff_ms > timeout_ms)
		status = STMVL53L5_ERROR_TIME_OUT;

	stmvl53l5_log_debug("stmvl53l5_check_for_timeout time_diff_ms:(%u) timeout_ms:(%u)\n",
			   time_diff_ms, timeout_ms);
	return status;
}

#ifdef STM_VL53L5_GPIO_ENABLE
int32_t stmvl53l5_platform_init(struct stmvl53l5_dev_t *p_dev)
{
	int32_t status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_module_t *p_module;

	p_module = (struct stmvl53l5_module_t *)
		container_of(p_dev, struct stmvl53l5_module_t, stdev);

	LOG_FUNCTION_START("");

	status = stmvl53l5_gpio_low_power_control(p_dev, GPIO_LOW);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_gpio_power_enable(p_dev, GPIO_LOW);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_gpio_comms_select(p_dev, GPIO_LOW);
	if (status < STATUS_OK)
		goto exit;

	if (p_module->comms_type == STMVL53L5_SPI) {
		status = stmvl53l5_gpio_comms_select(p_dev, GPIO_HIGH);
		if (status < STATUS_OK)
			goto exit;
	}

	status = stmvl53l5_wait_us(p_dev, 1000);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_gpio_low_power_control(p_dev, GPIO_HIGH);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_gpio_power_enable(p_dev, GPIO_HIGH);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_wait_us(p_dev, 200);
	if (status < STATUS_OK)
		goto exit;

exit:
	LOG_FUNCTION_END(status);
	return status;
}

int32_t stmvl53l5_platform_terminate(struct stmvl53l5_dev_t *p_dev)
{
	int32_t status = STMVL53L5_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = stmvl53l5_gpio_low_power_control(p_dev, GPIO_LOW);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_gpio_power_enable(p_dev, GPIO_LOW);
	if (status < STATUS_OK)
		goto exit;

	status = stmvl53l5_gpio_comms_select(p_dev, GPIO_TRISTATE);
	if (status != STMVL53L5_ERROR_NONE)
		goto exit;
exit:
	LOG_FUNCTION_START("");
	return status;
}
#endif
