/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * $Revision: 22429 $
 * $Date: 2018-01-30 19:42:59 +0800 (周二, 30 一月 2018) $
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
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

#include "../lct_tp_info.h"

#define NVT_DEBUG 1

//---Touch Vendor ID---
#define TP_VENDOR_UNKNOW    0x00
#define TP_VENDOR_TIANMA    0x01
#define TP_VENDOR_BOE       0x02
#define TP_VENDOR_EBBG      0x03
#define TP_VENDOR_EBBG_2ND  0x04

//---GPIO number---
#define NVTTOUCH_RST_PIN 66
#define NVTTOUCH_INT_PIN 67


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING


//---I2C driver info.---
#define NVT_I2C_NAME "NVT-ts"
#define I2C_BLDR_Address 0x01
#define I2C_FW_Address 0x01
#define I2C_HW_Address 0x62

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)
//wanghan add
#if 1
#define LOGV(log, ...) \
        printk("[%s] %s (line %d): " log, NVT_I2C_NAME, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOGV(log, ...) {}
#endif

#if 0
#define LOG_ENTRY() \
        printk(KERN_NOTICE "[NVT-ts][debug] %s (file %s line %d) Entry.\n", __func__, __FILE__, __LINE__)
#define LOG_DONE() \
        printk(KERN_NOTICE "[NVT-ts][debug] %s (file %s line %d) Done.\n", __func__, __FILE__, __LINE__)
#else
#define LOG_ENTRY() {}
#define LOG_DONE() {}
#endif
//wanghan end

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 2340
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
/* add by yangjiangzhu compatible to shenchao and tianma TP FW  2018/3/16  start */
#define BOOT_UPDATE_FIRMWARE_NAME_TIANMA "novatek/tianma_nt36672a_miui_f7a.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_SHENCHAO "novatek/shenchao_nt36672a_miui_f7a.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_SHENCHAO_2ND "novatek/shenchao_2nd_nt36672a_miui_f7a.bin"
/* add by yangjiangzhu compatible to shenchao and tianma TP FW  2018/3/16  end */


//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 1
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */

//---Touch state.---
#define TOUCH_STATE_WORKING    0x00
#define TOUCH_STATE_UPGRADING  0x01

struct nvt_ts_data {
	uint8_t touch_state;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct nvt_work;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	uint8_t touch_vendor_id;
	struct regulator *vcc_i2c;
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
	struct mutex pm_mutex;
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
	uint16_t nvt_pid;
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
	struct i2c_client *client;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
    EVENT_MAP_HOST_CMD                      = 0x50,
    EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
    EVENT_MAP_RESET_COMPLETE                = 0x60,
    EVENT_MAP_FWINFO                        = 0x78,
    EVENT_MAP_PROJECTID                     = 0x9A,
} I2C_EVENT_MAP;

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
extern int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern void nvt_bootloader_reset(void);
extern void nvt_sw_reset_idle(void);
extern int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
extern int32_t nvt_get_fw_info(void);
extern int32_t nvt_clear_fw_status(void);
extern int32_t nvt_check_fw_status(void);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
extern void nvt_stop_crc_reboot(void);

#endif /* _LINUX_NVT_TOUCH_H */
