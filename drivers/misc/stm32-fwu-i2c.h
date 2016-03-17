/*
 * stm32 firmware update over i2c
 *
 * Copyright (c) 2014 STMicroelectronics
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Antonio Borneo <borneo.antonio@gmail.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef __STM32_FWU_I2C_PLATFORM_H__
#define __STM32_FWU_I2C_PLATFORM_H__

/*
 * Specify the length of reply for command GET
 * This is helpful for frame-oriented protocols, e.g. i2c, to avoid time
 * consuming try-fail-timeout-retry operation.
 */
struct stm32_cmd_get_reply {
	u8 version;
	u8 length;
};

/**
 * struct stm32_i2c_platform_data - FIXME description
 * @gpio_reset: The gpio used to reset STM32
 * @gpio_boot0: The gpio connected to STM32 pad boot0
 * @firmware_name: The filename of fw to flash
 * @rx_max_len: The max size of frame from STM32
 * @tx_max_len: The max size of frame to STM32
 *
 * Depending on communication interface, the data frames between
 * STM32 and this driver can be limited in lenght. Anyway, the
 * bootloader protocol fixes upper limit to 256, so no need to
 * specify @rx_max_len or @tx_max_len if higher than 256.
 */
struct stm32_i2c_platform_data {
	int gpio_reset;
	int gpio_boot0;
	const char *firmware_name;
	unsigned rx_max_len;
	unsigned tx_max_len;

	/* FIXME private info, move in i2c_set_clientdata() */
	u32 chip_id;
	u8 bootloader_ver;
	u8 cmd_rm, cmd_go, cmd_wm, cmd_er, cmd_wp, cmd_uw, cmd_rp, cmd_ur;
	u8 cmd_crc;
	struct stm32_cmd_get_reply *cmd_get_reply;
	int (*hw_reset)(struct device *dev, bool enter_bl);
	int (*send)(struct device *dev, const char *buf, int count);
	int (*recv)(struct device *dev, char *buf, int count);
};

#endif /* __STM32_FWU_I2C_PLATFORM_H__ */
