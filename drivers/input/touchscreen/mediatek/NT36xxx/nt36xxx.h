 /*
  * Copyright (C) 2010 - 2017 Novatek, Inc.
  *
  * Revision: 15504
  * $Date: 2017-11-16 17:42:51 +0800 (週四, 16 十一月 2017) $
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  *
  */
#ifndef _LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/i2c.h>
#include <linux/input.h>

#define NVT_DEBUG 1

#define NVT_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define NVT_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

//---INT trigger mode---
//#define EINTF_TRIGGER_RISING     0x00000001
//#define EINTF_TRIGGER_FALLING    0x00000002
#define INT_TRIGGER_TYPE IRQF_TRIGGER_RISING

//---I2C driver info.---
#define NVT_I2C_NAME "NVT-ts"
#define I2C_BLDR_Address 0x01
#define I2C_FW_Address 0x01
#define I2C_HW_Address 0x62

#define NVT_LOG(fmt, args...)    \
	pr_info("[%s] %s %d: " fmt, NVT_I2C_NAME, \
	__func__, __LINE__, ##args)
#define NVT_ERR(fmt, args...)    \
	pr_info("[%s] %s %d: " fmt, NVT_I2C_NAME, \
	__func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"

//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 1920
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 0
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif

#define BOOT_UPDATE_FIRMWARE 0
#define BOOT_UPDATE_FIRMWARE_NAME "novatek_ts_fw.bin"

//--I2C DMA info.---
#define I2C_DMA_SUPPORT 1
#define DMA_MAX_TRANSACTION_LENGTH        255   // for DMA mode
#define DMA_MAX_I2C_TRANSFER_SIZE        (DMA_MAX_TRANSACTION_LENGTH - 1)
#define MAX_TRANSACTION_LENGTH            8
#define MAX_I2C_TRANSFER_SIZE            (MAX_TRANSACTION_LENGTH - 1)

struct nvt_ts_mem_map {
	uint32_t EVENT_BUF_ADDR;
	uint32_t RAW_PIPE0_ADDR;
	uint32_t RAW_PIPE0_Q_ADDR;
	uint32_t RAW_PIPE1_ADDR;
	uint32_t RAW_PIPE1_Q_ADDR;
	uint32_t BASELINE_ADDR;
	uint32_t BASELINE_Q_ADDR;
	uint32_t BASELINE_BTN_ADDR;
	uint32_t BASELINE_BTN_Q_ADDR;
	uint32_t DIFF_PIPE0_ADDR;
	uint32_t DIFF_PIPE0_Q_ADDR;
	uint32_t DIFF_PIPE1_ADDR;
	uint32_t DIFF_PIPE1_Q_ADDR;
	uint32_t RAW_BTN_PIPE0_ADDR;
	uint32_t RAW_BTN_PIPE0_Q_ADDR;
	uint32_t RAW_BTN_PIPE1_ADDR;
	uint32_t RAW_BTN_PIPE1_Q_ADDR;
	uint32_t DIFF_BTN_PIPE0_ADDR;
	uint32_t DIFF_BTN_PIPE0_Q_ADDR;
	uint32_t DIFF_BTN_PIPE1_ADDR;
	uint32_t DIFF_BTN_PIPE1_Q_ADDR;
	uint32_t READ_FLASH_CHECKSUM_ADDR;
	uint32_t RW_FLASH_DATA_ADDR;
};

struct nvt_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct nvt_work;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint16_t abs_x_max;
	uint16_t abs_y_max;
	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
	int32_t irq_gpio;
	uint32_t irq_flags;
	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
	uint16_t nvt_pid;
};

#if NVT_TOUCH_PROC
struct nvt_flash_data {
	rwlock_t lock;
	struct i2c_client *client;
};
#endif

enum RST_COMPLETE_STATE {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
};

enum I2C_EVENT_MAP {
	EVENT_MAP_HOST_CMD                      = 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
	EVENT_MAP_RESET_COMPLETE                = 0x60,
	EVENT_MAP_FWINFO                        = 0x78,
	EVENT_MAP_PROJECTID                     = 0x9A,
};

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
extern int32_t CTP_I2C_READ(struct i2c_client *client,
uint16_t address, uint8_t *buf, uint16_t len);
extern int32_t CTP_I2C_WRITE(struct i2c_client *client,
	uint16_t address, uint8_t *buf, uint16_t len);
extern void nvt_bootloader_reset(void);
extern void nvt_sw_reset_idle(void);
extern int32_t nvt_check_fw_reset_state(
	enum RST_COMPLETE_STATE check_reset_state);
extern int32_t nvt_get_fw_info(void);
extern int32_t nvt_clear_fw_status(void);
extern int32_t nvt_check_fw_status(void);
#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
#endif
#if BOOT_UPDATE_FIRMWARE
extern void Boot_Update_Firmware(struct work_struct *work);
#endif


#endif /* _LINUX_NVT_TOUCH_H */
