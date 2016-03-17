/*
 * stm32 bootloader core functions
 *
 * Copyright (c) 2014 STMicroelectronics
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Antonio Borneo <borneo.antonio@gmail.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef __STM32_BOOTLOADER_H__
#define __STM32_BOOTLOADER_H__

/* possible return value, not necessarily an error */
#define STM32_NACK		0x1F

#define STM32_FLASH_SKIP_AREA_START_OFFSET   0x4000
#define STM32_FLASH_SKIP_AREA_END_OFFSET     0x8000
#define STM32_FLASH_START	0x08000000
#define STM32_MAX_BUFFER	256

/* FIXME: should be device dependent */
#define STM32_FLASH_SIZE	SZ_256K

#define CRC_INIT_VALUE  0xFFFFFFFF


int stm32_bl_init(struct device *dev);
int stm32_bl_mass_erase(struct device *dev);
int stm32_bl_read_memory(struct device *dev, uint32_t address, u8 *data,
			 size_t len);
int stm32_bl_write_memory(struct device *dev, uint32_t address, const u8 *data,
			  size_t len);
int stm32_bl_go(struct device *dev, uint32_t address);
int stm32_bl_crc_memory(struct device *dev, uint32_t address, size_t len,
			uint32_t *crc);
uint32_t stm32_bl_sw_crc(uint32_t crc, const u8 *buf, unsigned int len);

#endif /* __STM32_BOOTLOADER_H__ */
