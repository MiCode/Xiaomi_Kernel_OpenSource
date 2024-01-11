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

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/atomic.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#include "stmvl53l5_load_firmware.h"
#include "stmvl53l5_module_dev.h"
#include "stmvl53l5_logging.h"
#include "stmvl53l5_ioctl_defs.h"
#include "stmvl53l5_platform.h"
#include "stmvl53l5_power.h"
#include "stmvl53l5_version.h"
#include "stmvl53l5_error_codes.h"

#define STMVL53L5_FIRMWARE_LOAD_STATUS(p_module) \
	(((p_module)->firmware_loaded == true) ? \
	 STMVL53L5_ERROR_NONE : STMVL53L5_FIRMWARE_NOT_LOADED_ERROR)

struct stmvl53l5_gpio_t gpio_owns;

static struct miscdevice stmvl53l5_miscdev;

static unsigned char i2c_driver_added;
static unsigned char misc_registered;

struct common_grp__version_t {
	unsigned short version__revision;
	unsigned short version__build;
	unsigned short version__minor;
	unsigned short version__major;
};

static unsigned char irq_handler_registered;
static unsigned int stmvl53l5_irq_num;

static const struct i2c_device_id stmvl53l5_i2c_id[] = {
	{STMVL53L5_DRV_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, stmvl53l5_i2c_id);

static unsigned char spi_driver_registered;
static const struct spi_device_id stmvl53l5_spi_id[] = {
	{STMVL53L5_DRV_NAME, 0},
	{},
};

static struct stmvl53l5_module_t *stm_module;

static struct regulator *p_3vx_vreg = NULL;
static struct regulator *p_1v8_vreg = NULL;

struct load_firmware_struct {
	struct stmvl53l5_module_t *stm_wq_mopdule;
	struct work_struct load_fw;
};

struct workqueue_struct *load_fw_wq;

struct load_firmware_struct load_firmware_struct_t;

static int enable_regulator_1V8(struct device *dev, struct regulator **pp_vreg)
{
	int ret_source = 0;
	int ret_enable = 0;
	*pp_vreg = regulator_get(dev, "_1P8_power");
	if (IS_ERR(*pp_vreg)) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		ret_source = PTR_ERR(*pp_vreg);
		goto put_regulator;
	} else {
		pr_info("stmvl53l5 : 1P8 successful found\n");
	}
	ret_source = regulator_set_voltage(*pp_vreg, 1800000, 1800000);

	if (ret_source) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		goto put_regulator;
	}
	ret_source = regulator_set_load(*pp_vreg, 85000);
	if (ret_source) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		goto put_regulator;
	}
	ret_enable = regulator_enable(*pp_vreg);
	if (ret_enable) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		goto disable_regulator;
	}

disable_regulator:
	if(ret_enable) {
		regulator_disable(*pp_vreg);
		regulator_set_load(*pp_vreg, 0);
		regulator_set_voltage(*pp_vreg, 0, 1800000);
	}
put_regulator:
	if (ret_enable || ret_source) {
		regulator_put(*pp_vreg);
		*pp_vreg = NULL;
	}
	return ret_enable || ret_source;
}

static int enable_regulator_3VX(struct device *dev, struct regulator **pp_vreg, int voltage_value)
{
	int ret_source = 0;
	int ret_enable = 0;
	*pp_vreg = regulator_get(dev, "_3P0_power");
	if (IS_ERR(*pp_vreg)) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		ret_source = PTR_ERR(*pp_vreg);
		goto put_regulator;
	} else {
		pr_info("stmvl53l5 : 3P0 successful found\n");
	}
	ret_source = regulator_set_voltage(*pp_vreg, voltage_value, voltage_value);

	if (ret_source) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		goto put_regulator;
	}
	ret_source = regulator_set_load(*pp_vreg, 85000);
	if (ret_source) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		goto put_regulator;
	}
	ret_enable = regulator_enable(*pp_vreg);
	if (ret_enable) {
		pr_err("stmvl53l5 : Error at %s(%d)\n", __func__, __LINE__);
		goto disable_regulator;
	}

disable_regulator:
	if(ret_enable) {
		regulator_disable(*pp_vreg);
		regulator_set_load(*pp_vreg, 0);
		regulator_set_voltage(*pp_vreg, 0, voltage_value);
	}
put_regulator:
	if (ret_enable || ret_source) {
		regulator_put(*pp_vreg);
		*pp_vreg = NULL;
	}
	return ret_enable || ret_source;
}

static int enable_power(struct spi_device *spi)
{
	int ret = -1;
	ret = enable_regulator_3VX(&spi->dev,&p_3vx_vreg, 3300000);
	ret |= enable_regulator_1V8(&spi->dev,&p_1v8_vreg);
	return ret;
}

static void disable_power(void)
{
	regulator_disable(p_1v8_vreg);
	regulator_set_load(p_1v8_vreg, 0);
	regulator_set_voltage(p_1v8_vreg, 0, 1800000);
	regulator_put(p_1v8_vreg);
	p_1v8_vreg = NULL;

	regulator_disable(p_3vx_vreg);
	regulator_set_load(p_3vx_vreg, 0);
	regulator_set_voltage(p_3vx_vreg, 0, 3304000);
	regulator_put(p_3vx_vreg);
	p_3vx_vreg = NULL;
}

static int stmvl53l5_get_gpio(int gpio_number, const char *name, int direction)
{
	int status = STMVL53L5_ERROR_NONE;

	if (gpio_number == -1) {
		status = -ENODEV;
		goto no_gpio;
	}

	STMVL53L5_LOG_DEBUG("request gpio_number %d", gpio_number);
	status = gpio_request(gpio_number, name);
	if (status)
		goto request_failed;

	if (direction == 1) {
		status = gpio_direction_output(gpio_number, 0);
		if (status) {
			STMVL53L5_LOG_ERROR("failed configure gpio(%d)",
					    gpio_number);
			goto direction_failed;
		}
	} else {
		status = gpio_direction_input(gpio_number);
		if (status) {
			STMVL53L5_LOG_ERROR("fail to configure gpio(%d)",
					    gpio_number);
			goto direction_failed;
		}
	}

	return status;

direction_failed:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	gpio_free(gpio_number);

request_failed:
no_gpio:
	return status;
}

static void stmvl53l5_put_gpio(int gpio_number)
{
	STMVL53L5_LOG_DEBUG("release gpio_number %d", gpio_number);
	gpio_free(gpio_number);
}

static int stmvl53l5_open(struct inode *inode, struct file *file)
{
	STMVL53L5_LOG_DEBUG("(%d)", __LINE__);
	return STMVL53L5_ERROR_NONE;
}

static int stmvl53l5_release(struct inode *inode, struct file *file)
{
	STMVL53L5_LOG_DEBUG("(%d)", __LINE__);
	return STMVL53L5_ERROR_NONE;
}

static irqreturn_t stmvl53l5_intr_handler(int stmvl53l5_irq_num, void *dev_id)
{

	atomic_set(&stm_module->intr_ready_flag, 1);

	wake_up_interruptible(&stm_module->wq);

	return IRQ_HANDLED;
}

static int _stmvl53l5_ioctl_write(struct stmvl53l5_module_t *p_module,
				  void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_comms_struct_t comms_struct;
	unsigned char *data_buffer = NULL;

	status = STMVL53L5_FIRMWARE_LOAD_STATUS(p_module);
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Firmware not loaded");
		goto exit;
	}

	status = copy_from_user(&comms_struct, p, sizeof(comms_struct));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}

	data_buffer = kzalloc(comms_struct.len, GFP_DMA | GFP_KERNEL);
	if (IS_ERR(data_buffer)) {
		STMVL53L5_LOG_ERROR("Failed to allocate memory");
		status = PTR_ERR(data_buffer);
		goto exit;
	}

	status = copy_from_user(
		data_buffer, comms_struct.buf, comms_struct.len);
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit_free;
	}

	status = stmvl53l5_write_multi(&p_module->stdev, comms_struct.reg_index,
					data_buffer, comms_struct.len);

exit_free:
	kfree(data_buffer);
exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_ioctl_read(struct stmvl53l5_module_t *p_module,
				 void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_comms_struct_t comms_struct;
	unsigned char *data_buffer = NULL;

	status = STMVL53L5_FIRMWARE_LOAD_STATUS(p_module);
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Firmware not loaded");
		goto exit;
	}

	status = copy_from_user(&comms_struct, p, sizeof(comms_struct));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}

	data_buffer = kzalloc(comms_struct.len, GFP_DMA | GFP_KERNEL);
	if (IS_ERR(data_buffer)) {
		STMVL53L5_LOG_ERROR("Failed to allocate memory");
		status = PTR_ERR(data_buffer);
		goto exit;
	}

	status = stmvl53l5_read_multi(&p_module->stdev, comms_struct.reg_index,
					data_buffer, comms_struct.len);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit_free;

	status = copy_to_user(comms_struct.buf, data_buffer, comms_struct.len);
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit_free;
	}

exit_free:
	kfree(data_buffer);
exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_ioctl_rom_boot(struct stmvl53l5_module_t *p_module,
				     void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_boot_struct_t boot_struct = {0};

	status = stmvl53l5_check_rom_firmware_boot(&p_module->stdev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	boot_struct.revision_id = p_module->stdev.host_dev.revision_id;
	boot_struct.device_booted = p_module->stdev.host_dev.device_booted;

	status = copy_to_user(
		p, &boot_struct, sizeof(struct stmvl53l5_boot_struct_t));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}

exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_ioctl_fw_load(struct stmvl53l5_module_t *p_module)
{
	int status = STMVL53L5_ERROR_NONE;

	status = stmvl53l5_load_fw_stm(&p_module->stdev);
	if (status < STMVL53L5_ERROR_NONE) {
		p_module->firmware_loaded = false;
		goto exit;
	} else {
		p_module->firmware_loaded = true;
	}

exit:
	if (status)
		STMVL53L5_LOG_ERROR("Error %d", status);

	return status;
}

static char *_power_state_string(unsigned int power_state)
{
	char *state_string = NULL;

	switch (power_state) {
	case STMVL53L5_POWER_STATE_OFF:
		state_string = "power off";
		break;
	case STMVL53L5_POWER_STATE_ULP_IDLE:
		state_string = "Ultra Low power idle";
		break;
	case STMVL53L5_POWER_STATE_LP_IDLE_COMMS:
		state_string = "Low power idle";
		break;
	case STMVL53L5_POWER_STATE_HP_IDLE:
		state_string = "High power idle";
		break;
	case STMVL53L5_POWER_STATE_RANGING:
		state_string = "Ranging";
		break;
	default:
		break;
	}

	return state_string;
}

static int _stmvl53l5_ioctl_power(struct stmvl53l5_module_t *p_module, void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;
	struct stmvl53l5_power_state_t power_state = {0};

	status = STMVL53L5_FIRMWARE_LOAD_STATUS(p_module);
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Firmware not loaded");
		goto exit;
	}

	status = copy_from_user(&power_state, p,
				sizeof(struct stmvl53l5_power_state_t));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}

	power_state.previous =
			(unsigned int)p_module->stdev.host_dev.power_state;

	status = stmvl53l5_set_power_mode(
			&p_module->stdev,
			(enum stmvl53l5_power_states)power_state.request);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	STMVL53L5_LOG_INFO("setting power mode (%s) from (%s)",
			    _power_state_string(power_state.request),
			    _power_state_string(power_state.previous));

	status = copy_to_user(p, &power_state,
			      sizeof(struct stmvl53l5_power_state_t));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}

exit:
	STMVL53L5_LOG_ERROR("Error %d", status);
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_ioctl_wait_for_interrupt(
			struct stmvl53l5_module_t *p_module)
{
	int status = STMVL53L5_ERROR_NONE;

	status = STMVL53L5_FIRMWARE_LOAD_STATUS(p_module);
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Firmware not loaded");
		goto exit;
	}

	atomic_set(&p_module->force_wakeup, 0);
	status = wait_event_interruptible(
		p_module->wq, atomic_read(&p_module->intr_ready_flag) != 0);

	if (status || atomic_read(&p_module->force_wakeup)) {
		STMVL53L5_LOG_ERROR("status = %d, force_wakeup flag = %d",
				status, atomic_read(&p_module->force_wakeup));
		atomic_set(&p_module->intr_ready_flag, 0);
		atomic_set(&p_module->force_wakeup, 0);
		status = -EINTR;
		goto exit;
	}

	atomic_set(&p_module->intr_ready_flag, 0);

exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_ioctl_version(
		struct stmvl53l5_module_t *p_module, void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;
	struct common_grp__version_t version_struct = {STMVL53L5_VER_REVISION,
						       STMVL53L5_VER_BUILD,
						       STMVL53L5_VER_MINOR,
						       STMVL53L5_VER_MAJOR};

	status = copy_to_user(p, &version_struct,
				sizeof(struct common_grp__version_t));
	if (status < STMVL53L5_ERROR_NONE) {
		status = -EINVAL;
		STMVL53L5_LOG_ERROR("Error %d", status);
	}

	return status;
}

static int _stmvl53l5_ioctl_interrupt_owned(
		struct stmvl53l5_module_t *p_module, void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;
	bool interrupt_state = false;

	if (stm_module->gpio->intr_gpio_owned)
		interrupt_state = true;

	status = copy_to_user(p, &interrupt_state, sizeof(bool));
	if (status < STMVL53L5_ERROR_NONE) {
		status = -EINVAL;
		STMVL53L5_LOG_ERROR("Error %d", status);
	}

	return status;
}

static int _stmvl53l5_ioctl_get_error(struct stmvl53l5_module_t *p_module,
				      void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;

	status = copy_to_user(p, &p_module->last_driver_error, sizeof(int));
	if (status < STMVL53L5_ERROR_NONE) {
		status = -EINVAL;
		STMVL53L5_LOG_ERROR("Error %d", status);
	}

	return status;
}

static int _stmvl53l5_ioctl_set_spi_transfer_speed(
		struct stmvl53l5_module_t *p_module, void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;

	status = copy_from_user(&p_module->transfer_speed_hz, p, sizeof(int));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}
exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_ioctl_get_revid(struct stmvl53l5_module_t *p_module,
				      void __user *p)
{
	int status = STMVL53L5_ERROR_NONE;

	status = copy_to_user(
		p, &p_module->stdev.host_dev.revision_id, sizeof(unsigned char));
	if (status < STMVL53L5_ERROR_NONE) {
		STMVL53L5_LOG_ERROR("Failed to copy from user");
		status = -EINVAL;
		goto exit;
	}

exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static long stmvl53l5_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int status = STMVL53L5_ERROR_NONE;
	void __user *p = (void __user *)arg;

	STMVL53L5_LOG_DEBUG("%u", cmd);
	STMVL53L5_LOG_DEBUG("power: %u", STMVL53L5_IOCTL_POWER);
	switch (cmd) {
	case STMVL53L5_IOCTL_WRITE:
		status = _stmvl53l5_ioctl_write(stm_module, p);
		break;
	case STMVL53L5_IOCTL_READ:
		status = _stmvl53l5_ioctl_read(stm_module, p);
		break;
	case STMVL53L5_IOCTL_ROM_BOOT:
		status = _stmvl53l5_ioctl_rom_boot(stm_module, p);
		break;
	case STMVL53L5_IOCTL_FW_LOAD:
		status = _stmvl53l5_ioctl_fw_load(stm_module);
		break;
	case STMVL53L5_IOCTL_POWER:
		STMVL53L5_LOG_ERROR("vl53l5 enter stmvl53l5_ioctl_power!");
		status = _stmvl53l5_ioctl_power(stm_module, p);
		break;
	case STMVL53L5_IOCTL_KERNEL_VERSION:
		status = _stmvl53l5_ioctl_version(stm_module, p);
		break;
	case STMVL53L5_IOCTL_WAIT_FOR_INTERRUPT:
		if (irq_handler_registered == 1)
			status = _stmvl53l5_ioctl_wait_for_interrupt(stm_module);
		else
			status = STMVL53L5_ERROR_NO_INTERRUPT_HANDLER;
		break;
	case STMVL53L5_IOCTL_GET_ERROR:
		status = _stmvl53l5_ioctl_get_error(stm_module, p);
		break;
	case STMVL53L5_IOCTL_KERNEL_INTERRUPT_OWNED:
		status = _stmvl53l5_ioctl_interrupt_owned(stm_module, p);
		break;
	case STMVL53L5_IOCTL_SET_SPI_TRANSFER_SPEED:
		status = _stmvl53l5_ioctl_set_spi_transfer_speed(stm_module, p);
		break;
	case STMVL53L5_IOCTL_GET_REVID:
		status = _stmvl53l5_ioctl_get_revid(stm_module, p);
		break;
	default:
		status = -EINVAL;
		break;
	}

	if (stm_module->last_driver_error == 0)
		stm_module->last_driver_error = status;

	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);

	return status;
}

static const struct file_operations stmvl53l5_ranging_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= stmvl53l5_ioctl,
	.open			= stmvl53l5_open,
	.release		= stmvl53l5_release,
};

static int stmvl53l5_load_fimware(struct stmvl53l5_module_t *p_module)
{
	int status = STMVL53L5_ERROR_NONE;

	status = stmvl53l5_check_rom_firmware_boot(&p_module->stdev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_load_fw_stm(&p_module->stdev);
	if (status < STMVL53L5_ERROR_NONE) {
		p_module->firmware_loaded = false;
		goto exit_power;
	} else {
		p_module->firmware_loaded = true;
	}

exit_power:
	status = stmvl53l5_set_power_mode(&p_module->stdev,
					  STMVL53L5_AFTERBOOT_POWER_STATE);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;
	else
		STMVL53L5_LOG_INFO("device set to power state: %s",
			_power_state_string(STMVL53L5_AFTERBOOT_POWER_STATE));

exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static void stmvl53l5_wq_func(struct work_struct *work)
{
	int status = STMVL53L5_ERROR_NONE;
	int i;
	struct load_firmware_struct *p = (struct load_firmware_struct *)container_of(work,
			struct load_firmware_struct, load_fw);

	status = stmvl53l5_load_fimware(p->stm_wq_mopdule);
	if (status) {
		for (i = 0;i < 3;i++) {
			disable_power();
			status  = enable_power(p->stm_wq_mopdule->device);
			status = stmvl53l5_load_fimware(p->stm_wq_mopdule);
			if(status == STMVL53L5_ERROR_NONE)  break;
		}
		if (status) {
			disable_power();
			STMVL53L5_LOG_ERROR("Error %d", status);
		}
	}
	STMVL53L5_LOG_INFO("wq excecute success!");
	return;
}

static int stmvl53l5_parse_dt(struct device *dev)
{
	int status = STMVL53L5_ERROR_NONE;
	struct device_node *np = dev->of_node;

	if (of_find_property(np, "spi-cpha", NULL))
		stm_module->device->mode |= SPI_CPHA;
	if (of_find_property(np, "spi-cpol", NULL))
		stm_module->device->mode |= SPI_CPOL;

#ifdef STM_VL53L5_GPIO_ENABLE
	gpio_owns.pwren_gpio_nb = of_get_named_gpio(np, "stm,pwren", 0);
	if (gpio_is_valid(gpio_owns.pwren_gpio_nb)) {
		status = stmvl53l5_get_gpio(gpio_owns.pwren_gpio_nb,
					"vl53l5_power_enable_gpio", 1);
		if (status < STMVL53L5_ERROR_NONE) {
			STMVL53L5_LOG_ERROR("Failed to acquire PWREN GPIO(%d)",
						gpio_owns.pwren_gpio_nb);
			goto exit;
		} else {
			gpio_owns.pwren_gpio_owned = 1;
			STMVL53L5_LOG_DEBUG("GPIO(%d) Acquired",
					gpio_owns.pwren_gpio_nb);
		}
	}

	gpio_owns.lpn_gpio_nb = of_get_named_gpio(np, "stm,lpn", 0);
	status = stmvl53l5_get_gpio(gpio_owns.lpn_gpio_nb,
				    "vl53l5_lpn_gpio", 1);
	if (gpio_is_valid(gpio_owns.lpn_gpio_nb)) {
		if (status < STMVL53L5_ERROR_NONE) {
			STMVL53L5_LOG_ERROR("Failed to acquire LPN GPIO(%d)",
					    gpio_owns.lpn_gpio_nb);
			goto exit;
		} else {
			gpio_owns.lpn_gpio_owned = 1;
			STMVL53L5_LOG_DEBUG("GPIO(%d) Acquired",
					    gpio_owns.lpn_gpio_nb);
		}
	}

	gpio_owns.comms_gpio_nb = of_get_named_gpio(np, "stm,comms_select", 0);
	if (gpio_is_valid(gpio_owns.comms_gpio_nb)) {
		status = stmvl53l5_get_gpio(gpio_owns.comms_gpio_nb,
					"vl53l5_comms_select_gpio", 1);
		if (status < STMVL53L5_ERROR_NONE) {
			STMVL53L5_LOG_ERROR(
				"Failed to acquire COMMS_SELECT GPIO(%d)",
				gpio_owns.comms_gpio_nb);
			goto exit;
		} else {
			gpio_owns.comms_gpio_owned = 1;
			STMVL53L5_LOG_DEBUG("GPIO(%d) Acquired",
					gpio_owns.comms_gpio_nb);
		}
	}
exit:
#endif
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int stmvl53l5_parse_dt_special(struct device *dev, bool is_l8_product)
{
	int status = STMVL53L5_ERROR_NONE;
	struct device_node *np = dev->of_node;

	gpio_owns.intr_gpio_nb = of_get_named_gpio(np, "stm,interrupt", 0);
	if (is_l8_product) {
		if (gpio_is_valid(gpio_owns.intr_gpio_nb)) {
			status = stmvl53l5_get_gpio(gpio_owns.intr_gpio_nb,
						"vl53l5_intr_gpio", 0);
			if (status < STMVL53L5_ERROR_NONE) {
				STMVL53L5_LOG_ERROR("Failed to acquire INTR GPIO(%d)",
						gpio_owns.intr_gpio_nb);
				goto exit;
			} else {
				gpio_owns.intr_gpio_owned = 1;

				stmvl53l5_irq_num = gpio_to_irq(gpio_owns.intr_gpio_nb);

				init_waitqueue_head(&stm_module->wq);

				status = request_threaded_irq(
					stmvl53l5_irq_num,
					NULL,
					stmvl53l5_intr_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"st_tof_sensor",
					NULL);

				if (status) {
					STMVL53L5_LOG_ERROR(
						"Failed to Reg handler gpio %d, irq %d",
						gpio_owns.intr_gpio_nb, stmvl53l5_irq_num);

					stm_module->gpio = NULL;
					misc_deregister(&stmvl53l5_miscdev);
					kfree(stm_module);
					misc_registered = 0;
					status = -EPERM;
					goto exit;
				} else {
					STMVL53L5_LOG_INFO(
						"Register handler gpio %d, irq %d",
						gpio_owns.intr_gpio_nb, stmvl53l5_irq_num);
					irq_handler_registered = 1;
				}
			}
		}
		of_property_read_string(np, "stm,l8_firmware_name",
					&stm_module->firmware_name);
	} else {
		of_property_read_string(np, "stm,l5_firmware_name",
					&stm_module->firmware_name);
	}
exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int _stmvl53l5_read_device_id(void)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned char page = 0;

	status = stmvl53l5_write_multi(&stm_module->stdev, 0x7FFF, &page, 1);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;
	status = stmvl53l5_read_multi(&stm_module->stdev, 0x00,
				    &stm_module->stdev.host_dev.device_id, 1);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;
	status = stmvl53l5_read_multi(&stm_module->stdev, 0x01,
				    &stm_module->stdev.host_dev.revision_id, 1);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	if ((stm_module->stdev.host_dev.device_id != 0xF0) ||
	    (stm_module->stdev.host_dev.revision_id != 0x02 &&
	     stm_module->stdev.host_dev.revision_id != 0x0C)) {
		STMVL53L5_LOG_ERROR("Error. Invalid revision id : 0x%X",
				stm_module->stdev.host_dev.revision_id);
		status = STMVL53L5_UNKNOWN_SILICON_REVISION;
		goto exit;
	}
	STMVL53L5_LOG_INFO("device_id : 0x%X. revision_id : 0x%X",
		stm_module->stdev.host_dev.device_id,
		stm_module->stdev.host_dev.revision_id);
	if (stm_module->stdev.host_dev.revision_id == 0x0C) {
		stm_module->is_L8_product = true;
	} else {
		stm_module->is_L8_product = false;
	}

exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int stmvl53l5_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int status = STMVL53L5_ERROR_NONE;

	STMVL53L5_LOG_INFO("probing i2c");

	stm_module = kzalloc(sizeof(struct stmvl53l5_module_t),
			     GFP_DMA | GFP_KERNEL);
	if (stm_module == NULL) {
		STMVL53L5_LOG_ERROR("Failed to allocate memory");
		status = -ENOMEM;
		goto exit;
	}

	stm_module->client = client;
	stm_module->gpio = &gpio_owns;
	stm_module->comms_type = STMVL53L5_I2C;
	stm_module->firmware_name = NULL;

	status = stmvl53l5_parse_dt(&stm_module->client->dev);
	if (status) {
		STMVL53L5_LOG_ERROR("Failed to parse device tree");
		goto exit;
	}
#ifdef STM_VL53L5_GPIO_ENABLE
	status = stmvl53l5_platform_init(&stm_module->stdev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;
#endif
	stm_module->stdev.host_dev.power_state = STMVL53L5_POWER_STATE_HP_IDLE;

	status = _stmvl53l5_read_device_id();
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_parse_dt_special(&stm_module->client->dev, stm_module->is_L8_product);
	if (status) {
		STMVL53L5_LOG_ERROR("Failed to parse device tree");
		goto exit;
	}

	stmvl53l5_miscdev.minor = MISC_DYNAMIC_MINOR;
	stmvl53l5_miscdev.name = "stmvl53l5";
	stmvl53l5_miscdev.fops = &stmvl53l5_ranging_fops;
	stmvl53l5_miscdev.mode = 0444;

	status = misc_register(&stmvl53l5_miscdev);
	if (status) {
		STMVL53L5_LOG_ERROR("Failed to create misc device");
		goto exit;
	}

	misc_registered = 1;

	status = stmvl53l5_load_fimware(stm_module);
exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static int stmvl53l5_i2c_remove(struct i2c_client *client)
{
#ifdef STM_VL53L5_GPIO_ENABLE
	(void)stmvl53l5_platform_terminate(&stm_module->stdev);
#endif
	STMVL53L5_LOG_INFO("Remove i2c");

	stm_module->gpio = NULL;
	kfree(stm_module);

	return STMVL53L5_ERROR_NONE;
}

static struct i2c_driver stmvl53l5_i2c_driver = {
	.driver = {
		.name = STMVL53L5_DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = stmvl53l5_i2c_probe,
	.remove = stmvl53l5_i2c_remove,
	.id_table = stmvl53l5_i2c_id,
};

static int stmvl53l5_spi_probe(struct spi_device *spi)
{
	int status = STMVL53L5_ERROR_NONE;
	unsigned long long duration;
	ktime_t start_ns,delta;

	start_ns = ktime_get();

	pr_err("vl53l5 probing spi");
	status = enable_power(spi);
	if (status) {
		pr_err("stmvl53l5: Error. Could not enable power, return -1\n");
		return status;
	}

	stm_module = kzalloc(sizeof(struct stmvl53l5_module_t),
			     GFP_DMA | GFP_KERNEL);
	if (stm_module == NULL) {
		STMVL53L5_LOG_ERROR("Failed to allocate memory");
		status = -ENOMEM;
		goto exit;
	}

	stm_module->device = spi;
	stm_module->gpio = &gpio_owns;
	stm_module->comms_type = STMVL53L5_SPI;
	stm_module->firmware_name = NULL;
	stm_module->transfer_speed_hz = STMVL53L5_SPI_DEFAULT_TRANSFER_SPEED_HZ;

	status = stmvl53l5_parse_dt(&stm_module->device->dev);
	if (status) {
		STMVL53L5_LOG_ERROR("Failed to parse device tree");
		goto exit;
	}

#ifdef STM_VL53L5_GPIO_ENABLE
	status = stmvl53l5_platform_init(&stm_module->stdev);
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;
#endif
	stm_module->stdev.host_dev.power_state = STMVL53L5_POWER_STATE_HP_IDLE;

	status = _stmvl53l5_read_device_id();
	if (status < STMVL53L5_ERROR_NONE)
		goto exit;

	status = stmvl53l5_parse_dt_special(&stm_module->device->dev, stm_module->is_L8_product);
	if (status) {
		STMVL53L5_LOG_ERROR("Failed to parse device tree");
		goto exit;
	}
	stmvl53l5_miscdev.minor = MISC_DYNAMIC_MINOR;
	stmvl53l5_miscdev.name = "stmvl53l5";
	stmvl53l5_miscdev.fops = &stmvl53l5_ranging_fops;
	stmvl53l5_miscdev.mode = 0444;

	status = misc_register(&stmvl53l5_miscdev);
	if (status) {
		STMVL53L5_LOG_ERROR("Failed to create misc device");
		goto exit;
	}

	misc_registered = 1;

	load_firmware_struct_t.stm_wq_mopdule = stm_module;
	load_fw_wq = create_workqueue("load_fw_wq");
	if (load_fw_wq == NULL) {
		STMVL53L5_LOG_INFO("create workqueue to load firmware failed!");
		goto exit;
	}

	INIT_WORK(&load_firmware_struct_t.load_fw, stmvl53l5_wq_func);
	queue_work(load_fw_wq, &load_firmware_struct_t.load_fw);
	STMVL53L5_LOG_INFO("load_firmware by spi!");
	delta = ktime_sub(ktime_get() , start_ns);
	duration = (unsigned long long) ktime_to_ms(delta);
	STMVL53L5_LOG_ERROR("spi load_firmware took %lld msecs\n", duration);

	//status = stmvl53l5_load_fimware(stm_module);

exit:
	if (status < STMVL53L5_ERROR_NONE) {
		disable_power();
		STMVL53L5_LOG_ERROR("Error %d", status);
	}
	return status;
}

static int stmvl53l5_spi_remove(struct spi_device *device)
{
#ifdef STM_VL53L5_GPIO_ENABLE
	(void)stmvl53l5_platform_terminate(&stm_module->stdev);
#endif
	STMVL53L5_LOG_INFO("Remove SPI");

	stm_module->gpio = NULL;
	kfree(stm_module);
	disable_power();
	return STMVL53L5_ERROR_NONE;
}

static struct spi_driver stmvl53l5_spi_driver = {
	.driver = {
		.name   = STMVL53L5_DRV_NAME,
		.owner  = THIS_MODULE,
	},
	.probe  = stmvl53l5_spi_probe,
	.remove = stmvl53l5_spi_remove,
	.id_table = stmvl53l5_spi_id,
};

static int __init stmvl53l5_module_init(void)
{
	int status = STMVL53L5_ERROR_NONE;

	STMVL53L5_LOG_INFO("module init");

	i2c_driver_added = 0;
	misc_registered = 0;
	stmvl53l5_irq_num = 0;
	irq_handler_registered = 0;
	stm_module = NULL;

	status = i2c_add_driver(&stmvl53l5_i2c_driver);
	if (status) {
		i2c_del_driver(&stmvl53l5_i2c_driver);
		STMVL53L5_LOG_ERROR("could not add i2c driver");
		goto exit;
	} else {
		i2c_driver_added = 1;
	}

	status = spi_register_driver(&stmvl53l5_spi_driver);
	if (status) {
		spi_unregister_driver(&stmvl53l5_spi_driver);
		STMVL53L5_LOG_ERROR("Error registering spi driver %d", status);
		goto exit;
	} else {
		spi_driver_registered = 1;
	}

exit:
	if (status < STMVL53L5_ERROR_NONE)
		STMVL53L5_LOG_ERROR("Error %d", status);
	return status;
}

static void __exit stmvl53l5_module_exit(void)
{
	STMVL53L5_LOG_INFO("module exit");

	if (misc_registered) {
		STMVL53L5_LOG_INFO("deregister device");
		misc_deregister(&stmvl53l5_miscdev);
		misc_registered = 0;
	}

	if (spi_driver_registered) {
		STMVL53L5_LOG_INFO("unregister SPI");
		spi_unregister_driver(&stmvl53l5_spi_driver);
		spi_driver_registered = 0;
	}

	if (i2c_driver_added) {
		STMVL53L5_LOG_INFO("delete I2C");
		i2c_del_driver(&stmvl53l5_i2c_driver);
		i2c_driver_added = 0;
	}
	if (irq_handler_registered)
		free_irq(stmvl53l5_irq_num, NULL);

	if (gpio_owns.intr_gpio_owned == 1)
		stmvl53l5_put_gpio(gpio_owns.intr_gpio_nb);

#ifdef STM_VL53L5_GPIO_ENABLE
	if (gpio_owns.pwren_gpio_owned == 1)
		stmvl53l5_put_gpio(gpio_owns.pwren_gpio_nb);

	if (gpio_owns.lpn_gpio_owned == 1)
		stmvl53l5_put_gpio(gpio_owns.lpn_gpio_nb);

	if (gpio_owns.comms_gpio_owned == 1)
		stmvl53l5_put_gpio(gpio_owns.comms_gpio_nb);
#endif
	STMVL53L5_LOG_INFO("delete workqueue!");
	destroy_workqueue(load_fw_wq);
	stm_module = NULL;
}

module_init(stmvl53l5_module_init);
module_exit(stmvl53l5_module_exit);
MODULE_LICENSE("GPL");
