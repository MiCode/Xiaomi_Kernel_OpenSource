/*
 * stm32-fwu-i2c.c - STM32 firmware update over i2c
 * Copyright (c) 2014 STMicroelectronics
 * Copyright (C) 2016 XiaoMi, Inc.
 * Antonio Borneo <borneo.antonio@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

/*
 * STM32 devices include a bootloader able to program the internal flash
 * through external interfaces, e.g. USART, I2C, USB, CAN.
 * The core communication protocol is quite similar across all the
 * interfaces.
 *
 * The protocol is described in the following documents:
 *
 * AN2606: STM32 microcontroller system memory boot mode
 *   http://www.st.com/web/en/resource/technical/document/application_note/CD00167594.pdf
 *
 * AN3155: USART protocol used in the STM32 bootloader
 *   http://www.st.com/web/en/resource/technical/document/application_note/CD00264342.pdf
 *
 * AN4221: I2C protocol used in the STM32 bootloader
 *   http://www.st.com/web/en/resource/technical/document/application_note/DM0072315.pdf
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>

#include <asm/sizes.h>

#include "stm32-fwu-i2c.h"
#include "stm32-bootloader.h"

#define RESET_ACTIVE		0
#define RESET_INACTIVE		1
#define BOOT0_NORMAL		0
#define BOOT0_BOOTLOADER	1

static struct stm32_cmd_get_reply stm32_i2c_cmd_get_reply[] = {
	{ 0x10, 11 },
	{ 0x11, 17 },
	{ 0x12, 18 },
	{ /* sentinel */ }
};

static void stm32_i2c_new_device(struct i2c_client *client);

static int stm32_i2c_send(struct device *dev, const char *buf, int count)
{
	struct i2c_client *client = to_i2c_client(dev);
	return i2c_master_send(client, buf, count);
}

static int stm32_i2c_recv(struct device *dev, char *buf, int count)
{
	struct i2c_client *client = to_i2c_client(dev);
	return i2c_master_recv(client, buf, count);
}

static int stm32_reset(struct device *dev, bool enter_bl)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;

	if (!gpio_is_valid(pdata->gpio_reset))
		return -EPERM;

	if (gpio_is_valid(pdata->gpio_boot0))
		gpio_set_value(pdata->gpio_boot0,
			       enter_bl ? BOOT0_BOOTLOADER : BOOT0_NORMAL);
	else
		if (!enter_bl)
			return -EPERM;

	gpio_set_value(pdata->gpio_reset, RESET_ACTIVE);
	msleep(5);
	gpio_set_value(pdata->gpio_reset, RESET_INACTIVE);
	msleep(20);
	return 0;
}

/*
 * return values:
 *	0 for verify ok
 *	1 for verify fail
 *	< 0 for errors
 */
static int stm32_i2c_verify_blk(struct i2c_client *client,
				unsigned int stm32_add, size_t size,
				const u8 *fw_ptr)
{
	int ret;
	size_t len;
	u8 *buf;
	uint32_t crc, sw_crc;

	ret = stm32_bl_crc_memory(&client->dev, stm32_add, size, &crc);
	if (ret && ret != -EOPNOTSUPP)
		return ret;

	if (!ret) {
		sw_crc = stm32_bl_sw_crc(CRC_INIT_VALUE, fw_ptr, size);
		if (crc == sw_crc) {
			dev_info(&client->dev, "CRC match\n");
			return 0;
		}
		dev_info(&client->dev, "CRC mismatch\n");
		return 1;
	}

	buf = kzalloc(STM32_MAX_BUFFER, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	while (size) {
		len = (size > STM32_MAX_BUFFER) ? STM32_MAX_BUFFER : size;
		dev_dbg(&client->dev, "Verify at 0x%08x, len %zu\n",
			stm32_add, len);
		ret = stm32_bl_read_memory(&client->dev, stm32_add, buf, len);
		if (ret < 0) {
			kfree(buf);
			return ret;
		}
		ret = memcmp(buf, fw_ptr, len);
		if (ret) {
			kfree(buf);
			return 1;
		}
		stm32_add += len;
		fw_ptr += len;
		size -= len;
	}
	kfree(buf);
	return 0;
}


/*
 * return values:
 *	0 for verify ok
 *	1 for verify fail
 *	< 0 for errors
 */
static int stm32_i2c_verify_fw(struct i2c_client *client,
			       const struct firmware *fw)
{
	unsigned int stm32_add;
	int ret;
	size_t size;
	const u8 *fw_ptr;

	stm32_add = STM32_FLASH_START;
	size = STM32_FLASH_SKIP_AREA_START_OFFSET;
	fw_ptr = fw->data;
	ret = stm32_i2c_verify_blk(client, stm32_add, size, fw_ptr);
	if (ret)
		return ret;

	stm32_add = STM32_FLASH_START + STM32_FLASH_SKIP_AREA_END_OFFSET;
	size = fw->size - STM32_FLASH_SKIP_AREA_END_OFFSET;
	fw_ptr = fw->data + STM32_FLASH_SKIP_AREA_END_OFFSET;

	return stm32_i2c_verify_blk(client, stm32_add, size, fw_ptr);
}

static int stm32_i2c_program_fw(struct i2c_client *client,
				const struct firmware *fw)
{
	unsigned int stm32_add;
	int ret;
	size_t size, len;
	const u8 *fw_ptr;

	stm32_add = STM32_FLASH_START;
	size = fw->size;
	fw_ptr = fw->data;

	while (size) {
		len = (size > STM32_MAX_BUFFER) ? STM32_MAX_BUFFER : size;
		dev_dbg(&client->dev, "Write at 0x%08x, len %zu\n",
			stm32_add, len);
		ret = stm32_bl_write_memory(&client->dev, stm32_add, fw_ptr,
					    len);
		if (ret < 0)
			return ret;
		stm32_add += len;
		fw_ptr += len;
		size -= len;
	}
	return 0;
}

static void stm32_i2c_load_fw_cb(const struct firmware *fw, void *context)
{
	struct i2c_client *client = context;
	struct stm32_i2c_platform_data *pdata =
		client->dev.platform_data;

	int ret = 0;
	const unsigned char *p;

	if (!fw) {
		dev_err(&client->dev, "failed reading firmware %s\n",
			pdata->firmware_name);
		goto done;
	}

	dev_dbg(&client->dev, "firmware %s read succesfully\n",
		pdata->firmware_name);

	p = fw->data;
	if (fw->size == 0 || fw->size > STM32_FLASH_SIZE) {
		dev_err(&client->dev,
			"Error: firmware file %s has invalid size!\n",
			pdata->firmware_name);
		ret = -EINVAL;
		goto done;
	}

	ret = stm32_bl_init(&client->dev);
	if (ret) {
		dev_err(&client->dev, "Error entering in stm32 bootloder\n");
		goto exit_bootloader;
	}
	dev_info(&client->dev, "Bootloader version %x.%x\n",
		 pdata->bootloader_ver >> 4, pdata->bootloader_ver & 0x0f);
	dev_info(&client->dev, "Chip ID = 0x%04x\n", pdata->chip_id);

	/* FIXME: switch to firmware CRC */
	ret = stm32_i2c_verify_fw(client, fw);
	if (ret < 0) {
		dev_err(&client->dev, "Error verifying firmware.\n");
		goto exit_bootloader;
	}
	if (ret == 0) {
		dev_info(&client->dev, "Firmware already up-to-date.\n");
		goto exit_bootloader;
	}
	dev_info(&client->dev, "Firmware needs update.\n");

	ret = stm32_bl_mass_erase(&client->dev);
	if (ret) {
		dev_err(&client->dev, "Error failed mass erase (%d).\n", ret);
		goto exit_bootloader;
	}
	dev_info(&client->dev, "Flash erased.\n");

	/* update firmware */
	ret = stm32_i2c_program_fw(client, fw);
	if (ret < 0) {
		dev_err(&client->dev, "Error updating firmware (%d).\n", ret);
		goto exit_bootloader;
	}
	dev_info(&client->dev, "Flash updated.\n");

	/* verify firmware */
	ret = stm32_i2c_verify_fw(client, fw);
	if (ret != 0) {
		dev_err(&client->dev, "Firmware update wrong.\n");
		goto exit_bootloader;
	}
	dev_info(&client->dev, "Firmware successfully updated.\n");

exit_bootloader:
	ret = stm32_reset(&client->dev, false);
	if (ret) {
		ret = stm32_bl_go(&client->dev, STM32_FLASH_START);
		msleep(5);
	}
	/* delay to wait for the firmware be ready */
	msleep(1000);
	stm32_i2c_new_device(client);

done:
	release_firmware(fw);

	dev_info(&client->dev, "Removing device\n");
	i2c_unregister_device(client);
	return;
}

#ifndef CONFIG_OF
static int stm32_i2c_of_populate_pdata(struct device *dev,
	struct stm32_i2c_platform_data *pdata)
{
	return -ENODEV;
}

static void stm32_i2c_new_device(struct i2c_client *client)
{
	return;
}
#else /* CONFIG_OF */
static int stm32_i2c_of_populate_pdata(struct device *dev,
	struct stm32_i2c_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	if (!np)
		return -ENODEV;

	pdata->gpio_reset = of_get_named_gpio(np, "gpio-reset", 0);
	pdata->gpio_boot0 = of_get_named_gpio(np, "gpio-boot0", 0);
	ret = of_property_read_string(np, "firmware-name",
				      &pdata->firmware_name);
    pr_info("pdata->gpio_reset = %d\n", pdata->gpio_reset);
    pr_info("pdata->gpio_boot0 = %d\n", pdata->gpio_boot0);
    pr_info("pdata->firmware_name = %s\n", pdata->firmware_name);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "Unable to read firmware name\n");
		return ret;
	}
	ret = of_property_read_u32(np, "rx-frame-max-length",
				   &pdata->rx_max_len);
	if (ret)
		dev_err(dev, "Unable to read RX frame max length\n");
	ret = of_property_read_u32(np, "tx-frame-max-length",
				   &pdata->tx_max_len);
	if (ret)
		dev_err(dev, "Unable to read TX frame max length\n");

	return 0;
}

/* mainly copied from of_i2c_register_devices() */
static void stm32_i2c_new_device(struct i2c_client *client)
{
	void *result;
	struct device_node *node;

	/* Only register child devices if the adapter has a node pointer set */
	if (!client->dev.of_node)
		return;

	dev_dbg(&client->dev, "of_i2c: walking child nodes\n");

	for_each_child_of_node(client->dev.of_node, node) {
		struct i2c_board_info info = {};
		struct dev_archdata dev_ad = {};
		const __be32 *addr;
		int len;

		dev_dbg(&client->dev, "of_i2c: register %s\n", node->full_name);

		if (of_modalias_node(node, info.type, sizeof(info.type)) < 0) {
			dev_err(&client->dev, "of_i2c: modalias failure on %s\n",
				node->full_name);
			continue;
		}

		addr = of_get_property(node, "reg", &len);
		if (!addr || (len < sizeof(int))) {
			dev_err(&client->dev, "of_i2c: invalid reg on %s\n",
				node->full_name);
			continue;
		}

		info.addr = be32_to_cpup(addr);
		if (info.addr > (1 << 10) - 1) {
			dev_err(&client->dev, "of_i2c: invalid addr=%x on %s\n",
				info.addr, node->full_name);
			continue;
		}

		info.irq = irq_of_parse_and_map(node, 0);
		info.of_node = of_node_get(node);
		info.archdata = &dev_ad;

		request_module("%s%s", I2C_MODULE_PREFIX, info.type);

		result = i2c_new_device(client->adapter, &info);
		if (result == NULL) {
			dev_err(&client->dev, "of_i2c: Failure registering %s\n",
				node->full_name);
			of_node_put(node);
			irq_dispose_mapping(info.irq);
			continue;
		}
	}
}
#endif /* CONFIG_OF */

static int stm32_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct stm32_i2c_platform_data *pdata;
    struct regulator *st_sensor_hub_vdd;
    struct regulator *mag_vdd;
	int ret;

	dev_err(&client->dev, "stm32_i2c_probe()\n");

	pdata = dev_get_platdata(&client->dev);
	if (!pdata) {
		dev_info(&client->dev, "check OF\n");
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		client->dev.platform_data = pdata;
		if (client->dev.of_node) {
			ret = stm32_i2c_of_populate_pdata(&client->dev, pdata);
			if (ret < 0)
				return ret;
		}
	}

	pdata = dev_get_platdata(&client->dev);

	if (!pdata->firmware_name) {
		dev_err(&client->dev, "missing firmware filename\n");
		return -ENOENT;
	}

	if (gpio_is_valid(pdata->gpio_reset)) {
		ret = devm_gpio_request(&client->dev, pdata->gpio_reset, "stm32_reset");
		if (ret)
			return ret;
		ret = gpio_direction_output(pdata->gpio_reset, RESET_INACTIVE);
		if (ret)
			return ret;
		pdata->hw_reset = stm32_reset;
	} else
		dev_info(&client->dev, "No GPIO for stm32 reset.");

	if (gpio_is_valid(pdata->gpio_boot0)) {
		ret = devm_gpio_request(&client->dev, pdata->gpio_boot0, "stm32_boot0");
		if (ret)
			return ret;
		ret = gpio_direction_output(pdata->gpio_boot0, BOOT0_BOOTLOADER);
		if (ret)
			return ret;
	} else
		dev_info(&client->dev, "No GPIO for stm32 boot0");

	pdata->cmd_get_reply = stm32_i2c_cmd_get_reply;
	pdata->send = stm32_i2c_send;
	pdata->recv = stm32_i2c_recv;

	st_sensor_hub_vdd = regulator_get(&client->dev, "vdd");
	if (IS_ERR(st_sensor_hub_vdd)) {
		dev_err(&client->dev,	"Regulator get failed vdd ret=%ld\n", PTR_ERR(st_sensor_hub_vdd));
	return IS_ERR(st_sensor_hub_vdd);
	}

	ret = regulator_enable(st_sensor_hub_vdd);
	if (ret) {
	dev_err(&client->dev, "Regulator set_vtg failed vdd ret=%d\n", ret);
	return ret;
	}

	mag_vdd = regulator_get(&client->dev, "mag");
	if (IS_ERR(mag_vdd)) {
		dev_err(&client->dev,	"Regulator get failed vdd ret=%ld\n", PTR_ERR(mag_vdd));
	return IS_ERR(mag_vdd);
	}

	ret = regulator_enable(mag_vdd);
	if (ret) {
	dev_err(&client->dev, "Regulator set_vtg failed vdd ret=%d\n", ret);
	return ret;
	}

	return request_firmware_nowait(THIS_MODULE, true,
		pdata->firmware_name, &client->dev, GFP_KERNEL, client,
		stm32_i2c_load_fw_cb);
}

static int stm32_i2c_remove(struct i2c_client *client)
{
	client->dev.platform_data = NULL;
	return 0;
}

static const struct i2c_device_id stm32_i2c_id[] = {
	{ "stm32_fwupdate",	0	  },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id of_stm32_i2c_fwu[] = {
	{ .compatible = "st,stm32_i2c_fwu", },
	{},
};

MODULE_DEVICE_TABLE(of, of_stm32_i2c_fwu);
#endif

static struct i2c_driver stm32_i2c_driver = {
	.driver = {
		.name	= "stm32_fwupdate",
		.of_match_table = of_match_ptr(of_stm32_i2c_fwu),
	},
	.probe		= stm32_i2c_probe,
	.remove		= stm32_i2c_remove,
	.id_table	= stm32_i2c_id,
};

module_i2c_driver(stm32_i2c_driver);

MODULE_AUTHOR("Antonio Borneo <borneo.antonio@gmail.com>");
MODULE_DESCRIPTION("stm32 i2c firmware update");
MODULE_LICENSE("GPL");
