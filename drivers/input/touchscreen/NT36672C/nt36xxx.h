/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 46000 $
 * $Date: 2019-06-12 14:25:52 +0800 (週三, 12 六月 2019) $
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
#define	_LINUX_NVT_TOUCH_H

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#include "nt36xxx_mem_map.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_panel_ext.h"
#include "mtk_disp_notify.h"
#endif

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING

//---GPIO pin mode---
#define PINCTRL_STATE_SPI_DEFAULT   "gt9896s_spi_mode"

//---SPI driver info.---
#define NVT_SPI_NAME "novatek,NVT-ts-spi"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_debug("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 2400
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
#define BOOT_UPDATE_FIRMWARE_NAME novatek_firmware
#define MP_UPDATE_FIRMWARE_NAME   "novatek_ts_mp.bin"
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65
extern char novatek_firmware[25];

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	struct kthread_delayed_work nvt_fwu_dw;
	uint16_t addr;
	int8_t phys[32];
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	struct notifier_block disp_notifier;
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
	uint8_t hw_crc;
	uint16_t nvt_pid;
	uint8_t rbuf[1025];
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
	struct mtk_chip_config spi_ctrl;
#endif
};

#if NVT_TOUCH_PROC
struct nvt_flash_data {
	rwlock_t lock;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,	// IC reset
	RESET_STATE_REK,	// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
	EVENT_MAP_HOST_CMD = 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE = 0x51,
	EVENT_MAP_RESET_COMPLETE = 0x60,
	EVENT_MAP_FWINFO = 0x78,
	EVENT_MAP_PROJECTID = 0x9A,
} SPI_EVENT_MAP;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)

typedef enum {
	NVTWRITE = 0,
	NVTREAD = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_eng_reset(void);
void nvt_sw_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_bld_crc_enable(void);
void nvt_fw_crc_enable(void);
int32_t nvt_update_firmware(char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif				/* #if NVT_TOUCH_ESD_PROTECT */
#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
extern void nvt_mp_proc_deinit(void);
#endif
#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif
#if BOOT_UPDATE_FIRMWARE
static struct kthread_worker *nvt_fwu_worker;
extern void Boot_Update_Firmware(struct kthread_work *work);
#endif

#endif				/* _LINUX_NVT_TOUCH_H */
