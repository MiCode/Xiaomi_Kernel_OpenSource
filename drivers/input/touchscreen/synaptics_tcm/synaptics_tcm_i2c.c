/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2019 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2019 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include "synaptics_tcm_core.h"
#include "linux/moduleparam.h"

#define XFER_ATTEMPTS 10

static unsigned char *buf;

static unsigned int buf_size;

static struct syna_tcm_bus_io bus_io;

static struct syna_tcm_hw_interface hw_if;

static struct platform_device *syna_tcm_i2c_device;

active_tp_setup(synaptics_tcm);

#ifdef CONFIG_OF
static int parse_dt(struct device *dev, struct syna_tcm_board_data *bdata)
{
	int retval;
	u32 value;
	struct property *prop;
	struct device_node *np = dev->of_node;
	const char *name;

	prop = of_find_property(np, "synaptics,irq-gpio", NULL);
	if (prop && prop->length) {
		bdata->irq_gpio = of_get_named_gpio_flags(np,
				"synaptics,irq-gpio", 0,
				(enum of_gpio_flags *)&bdata->irq_flags);
	} else {
		bdata->irq_gpio = -1;
	}

	retval = of_property_read_u32(np, "synaptics,irq-on-state", &value);
	if (retval < 0)
		bdata->irq_on_state = 0;
	else
		bdata->irq_on_state = value;

	retval = of_property_read_string(np, "synaptics,pwr-reg-name", &name);
	if (retval < 0)
		bdata->pwr_reg_name = NULL;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "synaptics,bus-reg-name", &name);
	if (retval < 0)
		bdata->bus_reg_name = NULL;
	else
		bdata->bus_reg_name = name;

	prop = of_find_property(np, "synaptics,power-gpio", NULL);
	if (prop && prop->length) {
		bdata->power_gpio = of_get_named_gpio_flags(np,
				"synaptics,power-gpio", 0, NULL);
	} else {
		bdata->power_gpio = -1;
	}

	prop = of_find_property(np, "synaptics,power-on-state", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,power-on-state",
				&value);
		if (retval < 0) {
			LOGE(dev, "Failed to read synaptics,power-on-state\n");
			return retval;
		}
		bdata->power_on_state = value;
	} else {
		bdata->power_on_state = 0;
	}

	prop = of_find_property(np, "synaptics,power-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,power-delay-ms",
				&value);
		if (retval < 0) {
			LOGE(dev, "Failed to read synaptics,power-delay-ms\n");
			return retval;
		}
		bdata->power_delay_ms = value;
	} else {
		bdata->power_delay_ms = 0;
	}

	prop = of_find_property(np, "synaptics,reset-gpio", NULL);
	if (prop && prop->length) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"synaptics,reset-gpio", 0, NULL);
	} else {
		bdata->reset_gpio = -1;
	}

	prop = of_find_property(np, "synaptics,reset-on-state", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,reset-on-state",
				&value);
		if (retval < 0) {
			LOGE(dev, "Failed to read synaptics,reset-on-state\n");
			return retval;
		}
		bdata->reset_on_state = value;
	} else {
		bdata->reset_on_state = 0;
	}

	prop = of_find_property(np, "synaptics,reset-active-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,reset-active-ms",
				&value);
		if (retval < 0) {
			LOGE(dev, "Failed to read synaptics,reset-active-ms\n");
			return retval;
		}
		bdata->reset_active_ms = value;
	} else {
		bdata->reset_active_ms = 0;
	}

	prop = of_find_property(np, "synaptics,reset-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,reset-delay-ms",
				&value);
		if (retval < 0) {
			LOGE(dev, "Unable to read synaptics,reset-delay-ms\n");
			return retval;
		}
		bdata->reset_delay_ms = value;
	} else {
		bdata->reset_delay_ms = 0;
	}

	prop = of_find_property(np, "synaptics,x-flip", NULL);
	bdata->x_flip = prop > 0 ? true : false;

	prop = of_find_property(np, "synaptics,y-flip", NULL);
	bdata->y_flip = prop > 0 ? true : false;

	prop = of_find_property(np, "synaptics,swap-axes", NULL);
	bdata->swap_axes = prop > 0 ? true : false;

	prop = of_find_property(np, "synaptics,ubl-i2c-addr", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,ubl-i2c-addr",
				&value);
		if (retval < 0) {
			LOGE(dev, "Unable to read synaptics,ubl-i2c-addr\n");
			return retval;
		}
		bdata->ubl_i2c_addr = value;
	} else {
		bdata->ubl_i2c_addr = 0;
	}

	bdata->extend_report = of_property_read_bool(np,
			"synaptics,extend_report");

	return 0;
}
#endif

static int syna_tcm_i2c_alloc_mem(struct syna_tcm_hcd *tcm_hcd,
		unsigned int size)
{
	struct i2c_client *i2c = to_i2c_client(tcm_hcd->pdev->dev.parent);

	if (size > buf_size) {
		if (buf_size)
			kfree(buf);
		buf = kmalloc(size, GFP_KERNEL);
		if (!buf) {
			LOGE(&i2c->dev,
					"Failed to allocate memory for buf\n");
			buf_size = 0;
			return -ENOMEM;
		}
		buf_size = size;
	}

	return 0;
}

static int syna_tcm_i2c_rmi_read(struct syna_tcm_hcd *tcm_hcd,
		unsigned short addr, unsigned char *data, unsigned int length)
{
	int retval;
	unsigned char address;
	unsigned int attempt;
	struct i2c_msg msg[2];
	struct i2c_client *i2c = to_i2c_client(tcm_hcd->pdev->dev.parent);
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	address = (unsigned char)addr;

	msg[0].addr = bdata->ubl_i2c_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;

	msg[1].addr = bdata->ubl_i2c_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = data;

	for (attempt = 0; attempt < XFER_ATTEMPTS; attempt++) {
		if (i2c_transfer(i2c->adapter, msg, 2) == 2) {
			retval = length;
			goto exit;
		}

		LOGD(&i2c->dev, "Transfer attempt %d times\n", attempt + 1);

		if (attempt + 1 == XFER_ATTEMPTS) {
			LOGE(&i2c->dev, "Transfer failed\n");
			retval = -EIO;
			goto exit;
		}

		msleep(20);
	}

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int syna_tcm_i2c_rmi_write(struct syna_tcm_hcd *tcm_hcd,
		unsigned short addr, unsigned char *data, unsigned int length)
{
	int retval;
	unsigned int attempt;
	unsigned int byte_count;
	struct i2c_msg msg;
	struct i2c_client *i2c = to_i2c_client(tcm_hcd->pdev->dev.parent);
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	byte_count = length + 1;

	retval = syna_tcm_i2c_alloc_mem(tcm_hcd, byte_count);
	if (retval < 0) {
		LOGE(&i2c->dev,
				"Failed to allocate memory\n");
		goto exit;
	}

	buf[0] = (unsigned char)addr;
	retval = secure_memcpy(&buf[1],
			buf_size - 1,
			data,
			length,
			length);
	if (retval < 0) {
		LOGE(&i2c->dev,
				"Failed to copy write data\n");
		goto exit;
	}

	msg.addr = bdata->ubl_i2c_addr;
	msg.flags = 0;
	msg.len = byte_count;
	msg.buf = buf;

	for (attempt = 0; attempt < XFER_ATTEMPTS; attempt++) {
		if (i2c_transfer(i2c->adapter, &msg, 1) == 1) {
			retval = length;
			goto exit;
		}

		LOGD(&i2c->dev, "Transfer attempt %d times\n", attempt + 1);

		if (attempt + 1 == XFER_ATTEMPTS) {
			LOGE(&i2c->dev, "Transfer failed\n");
			retval = -EIO;
			goto exit;
		}

		msleep(20);
	}

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int syna_tcm_i2c_read(struct syna_tcm_hcd *tcm_hcd, unsigned char *data,
		unsigned int length)
{
	int retval;
	unsigned int attempt;
	struct i2c_msg msg;
	struct i2c_client *i2c = to_i2c_client(tcm_hcd->pdev->dev.parent);

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	msg.addr = i2c->addr;
	msg.flags = I2C_M_RD;
	msg.len = length;
	msg.buf = data;

	for (attempt = 0; attempt < XFER_ATTEMPTS; attempt++) {
		if (i2c_transfer(i2c->adapter, &msg, 1) == 1) {
			retval = length;
			goto exit;
		}

		LOGD(&i2c->dev, "Transfer attempt %d times\n", attempt + 1);

		if (attempt + 1 == XFER_ATTEMPTS) {
			LOGE(&i2c->dev, "Transfer failed\n");
			retval = -EIO;
			goto exit;
		}

		msleep(20);
	}

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int syna_tcm_i2c_write(struct syna_tcm_hcd *tcm_hcd, unsigned char *data,
		unsigned int length)
{
	int retval;
	unsigned int attempt;
	struct i2c_msg msg;
	struct i2c_client *i2c = to_i2c_client(tcm_hcd->pdev->dev.parent);

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	msg.addr = i2c->addr;
	msg.flags = 0;
	msg.len = length;
	msg.buf = data;

	for (attempt = 0; attempt < XFER_ATTEMPTS; attempt++) {
		if (i2c_transfer(i2c->adapter, &msg, 1) == 1) {
			retval = length;
			goto exit;
		}

		LOGD(&i2c->dev, "Transfer attempt %d times\n", attempt + 1);

		if (attempt + 1 == XFER_ATTEMPTS) {
			LOGE(&i2c->dev, "Transfer failed\n");
			retval = -EIO;
			goto exit;
		}

		msleep(20);
	}

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int syna_tcm_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *dev_id)
{
	int retval;
	struct device_node *dt = i2c->dev.of_node;

	if (synaptics_tcm_check_assigned_tp(dt, "compatible",
				"qcom,i2c-touch-active") < 0)
		return -ENODEV;

	syna_tcm_i2c_device = platform_device_alloc(PLATFORM_DRIVER_NAME, 0);
	if (!syna_tcm_i2c_device) {
		LOGE(&i2c->dev,
				"Failed to allocate platform device\n");
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	hw_if.bdata = devm_kzalloc(&i2c->dev, sizeof(*hw_if.bdata), GFP_KERNEL);
	if (!hw_if.bdata) {
		LOGE(&i2c->dev,
				"Failed to allocate memory for board data\n");
		return -ENOMEM;
	}
	parse_dt(&i2c->dev, hw_if.bdata);
#else
	hw_if.bdata = i2c->dev.platform_data;
#endif

	bus_io.type = BUS_I2C;
	bus_io.read = syna_tcm_i2c_read;
	bus_io.write = syna_tcm_i2c_write;
	bus_io.rmi_read = syna_tcm_i2c_rmi_read;
	bus_io.rmi_write = syna_tcm_i2c_rmi_write;

	hw_if.bus_io = &bus_io;

	syna_tcm_i2c_device->dev.parent = &i2c->dev;
	syna_tcm_i2c_device->dev.platform_data = &hw_if;

	retval = platform_device_add(syna_tcm_i2c_device);
	if (retval < 0) {
		LOGE(&i2c->dev,
				"Failed to add platform device\n");
		return retval;
	}

	return 0;
}

static int syna_tcm_i2c_remove(struct i2c_client *i2c)
{
	syna_tcm_i2c_device->dev.platform_data = NULL;

	platform_device_unregister(syna_tcm_i2c_device);

	return 0;
}

static const struct i2c_device_id syna_tcm_id_table[] = {
	{I2C_MODULE_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, syna_tcm_id_table);

#ifdef CONFIG_OF
static const struct of_device_id syna_tcm_of_match_table[] = {
	{
		.compatible = "synaptics,tcm-i2c",
	},
	{},
};
MODULE_DEVICE_TABLE(of, syna_tcm_of_match_table);
#else
#define syna_tcm_of_match_table NULL
#endif

static struct i2c_driver syna_tcm_i2c_driver = {
	.driver = {
		.name = I2C_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = syna_tcm_of_match_table,
	},
	.probe = syna_tcm_i2c_probe,
	.remove = syna_tcm_i2c_remove,
	.id_table = syna_tcm_id_table,
};

int syna_tcm_bus_init(void)
{
	return i2c_add_driver(&syna_tcm_i2c_driver);
}
EXPORT_SYMBOL(syna_tcm_bus_init);

void syna_tcm_bus_exit(void)
{
	kfree(buf);

	i2c_del_driver(&syna_tcm_i2c_driver);
}
EXPORT_SYMBOL(syna_tcm_bus_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM I2C Bus Module");
MODULE_LICENSE("GPL v2");
