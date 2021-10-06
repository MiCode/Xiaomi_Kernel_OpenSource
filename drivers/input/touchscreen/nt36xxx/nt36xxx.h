/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (Wed, 10 Jul 2019) $
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
#define _LINUX_NVT_TOUCH_H

#if !IS_ENABLED(CONFIG_TOUCHSCREEN_NT36XXX_SPI) /* TOUCHSCREEN_NT36XXX I2C */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


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

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 2408
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 1

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 0
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
#define BOOT_UPDATE_FIRMWARE_NAME "novatek_ts_fw.bin"

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */

struct nvt_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
	const struct i2c_device_id *id;
#if defined(CONFIG_DRM)
	struct notifier_block drm_panel_notif;
#elif defined(_MSM_DRM_NOTIFY_H_)
	struct notifier_block drm_notif;
#else
	struct notifier_block fb_notif;

#endif
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
	uint8_t xbuf[1025];
	struct mutex xbuf_lock;
	bool irq_enabled;
	void *notifier_cookie;
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
	struct i2c_client *client;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,        // ReK baseline
	RESET_STATE_REK_FINISH, // baseline is ready
	RESET_STATE_NORMAL_RUN, // normal run
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
extern int32_t nvt_set_page(uint16_t i2c_addr, uint32_t addr);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
extern void nvt_stop_crc_reboot(void);

#else  /* TOUCHSCREEN_NT36XXX_SPI */

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#include "nt36xxx_mem_map.h"

#define NVT_SPI_DEBUG 0

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943

//---INT trigger mode---
//#define NVT_SPI_IRQ_TYPE_EDGE_RISING 1
//#define NVT_SPI_IRQ_TYPE_EDGE_FALLING 2
#define NVT_SPI_INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING

//---SPI driver info.---
#define NVT_SPI_NAME "NVT-SPI"

#if NVT_SPI_DEBUG
#define NVT_LOG(fmt, args...) pr_err("[%s] %s %d: " fmt, "NVT-SPI", __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...) pr_debug("[%s] %s %d: " fmt, "NVT-SPI", __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...) pr_err("[%s] %s %d: " fmt, "NVT-SPI", __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_SPI_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_SPI_PEN_NAME "NVTCapacitivePen"

//---Touch info.---
#define NVT_SPI_TOUCH_DEFAULT_MAX_WIDTH 1080
#define NVT_SPI_TOUCH_DEFAULT_MAX_HEIGHT 2400
#define NVT_SPI_TOUCH_MAX_FINGER_NUM 10
#define NVT_SPI_TOUCH_KEY_NUM 0
#if NVT_SPI_TOUCH_KEY_NUM > 0
extern const uint16_t nvt_spi_touch_key_array[NVT_SPI_TOUCH_KEY_NUM];
#endif
#define NVT_SPI_TOUCH_FORCE_NUM 1000
//---for Pen---
#define NVT_SPI_PEN_PRESSURE_MAX (4095)
#define NVT_SPI_PEN_DISTANCE_MAX (1)
#define NVT_SPI_PEN_TILT_MIN (-60)
#define NVT_SPI_PEN_TILT_MAX (60)

/* Enable only when module have tp reset pin and connected to host */
#define NVT_SPI_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_SPI_TOUCH_PROC 1
#define NVT_SPI_TOUCH_EXT_PROC 1
#define NVT_SPI_TOUCH_MP 0
#define NVT_SPI_MT_PROTOCOL_B 1
#define NVT_SPI_WAKEUP_GESTURE 0

#if NVT_SPI_WAKEUP_GESTURE
extern const uint16_t nvt_spi_gesture_key_array[];
#endif
#define NVT_SPI_BOOT_UPDATE_FIRMWARE 1
#define NVT_SPI_BOOT_UPDATE_FIRMWARE_NAME "novatek_spi_fw.bin"
#define NVT_SPI_MP_UPDATE_FIRMWARE_NAME   "novatek_ts_mp.bin"
#define NVT_SPI_POINT_DATA_CHECKSUM 1
#define NVT_SPI_POINT_DATA_CHECKSUM_LEN 65

//---ESD Protect.---
#define NVT_SPI_TOUCH_ESD_PROTECT 0
#define NVT_SPI_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_SPI_TOUCH_WDT_RECOVERY 1

#define NVT_SPI_CHECK_PEN_DATA_CHECKSUM 0

struct nvt_spi_data_t {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];

#if defined(CONFIG_DRM)
	struct notifier_block drm_panel_notif;
#elif defined(_MSM_DRM_NOTIFY_H_)
	struct notifier_block drm_notif;
#else
	struct notifier_block fb_notif;
#endif

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
	const struct nvt_spi_mem_map *mmap;
	uint8_t hw_crc;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	bool pen_support;
	bool wgp_stylus;
	uint8_t x_gang_num;
	uint8_t y_gang_num;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];

	void *notifier_cookie;
	const char *touch_environment;

#ifdef CONFIG_NOVATEK_SPI_TRUSTED_TOUCH
	struct trusted_touch_vm_info *vm_info;
	struct mutex fts_clk_io_ctrl_mutex;
	struct completion trusted_touch_powerdown;
	struct clk *core_clk;
	struct clk *iface_clk;
	atomic_t trusted_touch_initialized;
	atomic_t trusted_touch_enabled;
	atomic_t trusted_touch_underway;
	atomic_t trusted_touch_event;
	atomic_t trusted_touch_abort_status;
	atomic_t delayed_vm_probe_pending;
	atomic_t trusted_touch_mode;
#endif
};

#if NVT_SPI_TOUCH_PROC
struct nvt_spi_flash_data {
	rwlock_t lock;
};
#endif

enum NVT_SPI_RST_COMPLETE_STATE {
	NVT_SPI_RESET_STATE_INIT = 0xA0,// IC reset
	NVT_SPI_RESET_STATE_REK,        // ReK baseline
	NVT_SPI_RESET_STATE_REK_FINISH, // baseline is ready
	NVT_SPI_RESET_STATE_NORMAL_RUN, // normal run
	NVT_SPI_RESET_STATE_MAX  = 0xAF
};

enum NVT_SPI_EVENT_MAP {
	NVT_SPI_EVENT_MAP_HOST_CMD                    = 0x50,
	NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE = 0x51,
	NVT_SPI_EVENT_MAP_RESET_COMPLETE              = 0x60,
	NVT_SPI_EVENT_MAP_FWINFO                      = 0x78,
	NVT_SPI_EVENT_MAP_PROJECTID                   = 0x9A,
};

//---SPI READ/WRITE---
#define NVT_SPI_WRITE_MASK(a) (a | 0x80)
#define NVT_SPI_READ_MASK(a)  (a & 0x7F)

#define NVT_SPI_DUMMY_BYTES  (1)
#define NVT_SPI_TRANSFER_LEN (63*1024)
#define NVT_SPI_READ_LEN     (2*1024)
#define NVT_SPI_XBUF_LEN     (NVT_SPI_TRANSFER_LEN+1+NVT_SPI_DUMMY_BYTES)

enum NVT_SPI_RW {
	NVT_SPI_WRITE = 0,
	NVT_SPI_READ  = 1
};

//---extern structures---
extern struct nvt_spi_data_t *nvt_spi_data;

//---extern functions---
int32_t nvt_spi_read(uint8_t *buf, uint16_t len);
int32_t nvt_spi_write(uint8_t *buf, uint16_t len);
void nvt_spi_bootloader_reset(void);
void nvt_spi_eng_reset(void);
void nvt_spi_sw_reset(void);
void nvt_spi_sw_reset_idle(void);
void nvt_spi_boot_ready(void);
void nvt_spi_bld_crc_enable(void);
void nvt_spi_fw_crc_enable(void);
void nvt_spi_tx_auto_copy_mode(void);
int32_t nvt_spi_update_firmware(char *firmware_name);
void nvt_spi_update_firmware_work(struct work_struct *work);
int32_t nvt_spi_check_fw_reset_state(enum NVT_SPI_RST_COMPLETE_STATE reset_state);
int32_t nvt_spi_get_fw_info(void);
int32_t nvt_spi_clear_fw_status(void);
int32_t nvt_spi_check_fw_status(void);
int32_t nvt_spi_check_spi_dma_tx_info(void);
int32_t nvt_spi_set_page(uint32_t addr);
int32_t nvt_spi_write_addr(uint32_t addr, uint8_t data);

#if NVT_SPI_TOUCH_EXT_PROC
int32_t nvt_spi_extra_proc_init(void);
void nvt_spi_extra_proc_deinit(void);
#endif

#if NVT_SPI_TOUCH_MP
extern int32_t nvt_spi_mp_proc_init(void);
extern void nvt_spi_mp_proc_deinit(void);
#endif

#if NVT_SPI_TOUCH_ESD_PROTECT
extern void nvt_spi_esd_check_enable(uint8_t enable);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */


#endif

#endif /* _LINUX_NVT_TOUCH_H */
