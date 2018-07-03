/*
 * stm32 firmware update over i2c
 *
 * Copyright (c) 2014 STMicroelectronics
 * Author: Antonio Borneo <borneo.antonio@gmail.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __STM32_FWU_I2C_PLATFORM_H__
#define __STM32_FWU_I2C_PLATFORM_H__

struct stm32_cmd_get_reply {
	u8 version;
	u8 length;
};

struct stm32_i2c_platform_data {
	int gpio_reset;
	int gpio_boot0;
	const char *firmware_name;
	unsigned rx_max_len;
	unsigned tx_max_len;

	u32 chip_id;
	u8 bootloader_ver;
	u8 cmd_rm, cmd_go, cmd_wm, cmd_er, cmd_wp, cmd_uw, cmd_rp, cmd_ur;
	u8 cmd_crc;
	struct stm32_cmd_get_reply *cmd_get_reply;
	int (*hw_reset)(struct device *dev, bool enter_bl);
	int (*send)(struct device *dev, const char *buf, int count);
	int (*recv)(struct device *dev, char *buf, int count);
};

#endif
