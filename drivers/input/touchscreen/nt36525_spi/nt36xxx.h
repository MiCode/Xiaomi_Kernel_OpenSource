/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * $Revision: 122098 $
 * $Date: 2023-07-13 10:23:26 +0800 (週四, 13 七月 2023) $
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

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"
#include <linux/regulator/consumer.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define HAVE_VFS_WRITE
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define reinit_completion(x) INIT_COMPLETION(*(x))
#endif

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX_MODULE
#include <linux/platform_data/spi-mt65xx.h>
#endif

// include longcheer header
#include "../lct_tp/lct_tp_info.h"
#include "../lct_tp/lct_tp_work.h"
#include "../lct_tp/lct_tp_selftest.h"
#include "../lct_tp/lct_tp_grip_area.h"
#include "../lct_tp/lct_tp_proximity_switch.h"
//enable tp work feature
#define LCT_TP_WORK_EN      1
//enable tp grip area feature
#define LCT_TP_GRIP_AREA_EN 1

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING

//---bus transfer length---
#define BUS_TRANSFER_LENGTH  256

//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_PEN_NAME "NVTCapacitivePen"

//---Touch info.---
#define TOUCH_MAX_WIDTH 720
#define TOUCH_MAX_HEIGHT 1650
#define PEN_MAX_WIDTH 2160
#define PEN_MAX_HEIGHT 3840
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000
//---for Pen---
#define PEN_PRESSURE_MAX (4095)
#define PEN_DISTANCE_MAX (1)
#define PEN_TILT_MIN (-60)
#define PEN_TILT_MAX (60)

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define NVT_SAVE_TEST_DATA_IN_FILE 0
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 0
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
#define BOOT_UPDATE_FIRMWARE_NAME "novatek_ts_truly_fw.bin"
#define MP_UPDATE_FIRMWARE_NAME   "novatek_ts_truly_mp.bin"
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65
#define NVT_PM_WAIT_BUS_RESUME_COMPLETE 1

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 1
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define CHECK_PEN_DATA_CHECKSUM 0

#if BOOT_UPDATE_FIRMWARE
#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
#if IS_ENABLED(CONFIG_DRM_PANEL)
#define NVT_DRM_PANEL_NOTIFY 1
#elif IS_ENABLED(_MSM_DRM_NOTIFY_H_)
#define NVT_MSM_DRM_NOTIFY 1
#elif IS_ENABLED(CONFIG_FB)
#define NVT_FB_NOTIFY 1
#elif IS_ENABLED(CONFIG_HAS_EARLYSUSPEND)
#define NVT_EARLYSUSPEND_NOTIFY 1
#endif
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
#define NVT_QCOM_PANEL_EVENT_NOTIFY 1
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK)
#define NVT_MTK_DRM_NOTIFY 1
#endif
#endif

#define NVT_USB_PLUGIN 1
#if NVT_USB_PLUGIN
typedef struct touchscreen_usb_plugin_data {
	bool valid;
	bool usb_plugged_in;
	void (*event_callback)(void);
} touchscreen_usb_plugin_data_t;
#endif

struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
        struct work_struct resume_work;
	struct workqueue_struct *resume_wq;
	uint16_t addr;
	int8_t phys[32];
#if IS_ENABLED(NVT_DRM_PANEL_NOTIFY)
	struct notifier_block drm_panel_notif;
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	struct notifier_block drm_notif;
#elif IS_ENABLED(NVT_FB_NOTIFY)
	struct notifier_block fb_notif;
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	struct early_suspend early_suspend;
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
	struct notifier_block disp_notifier;
#endif
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
	int32_t irq_gpio;
	uint32_t irq_flags;
	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t hw_crc;
	uint8_t auto_copy;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	bool pen_support;
	uint8_t x_gang_num;
	uint8_t y_gang_num;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t crc_err_flag_addr;
    bool sensor_tp_probe_status;
    bool sensor_tp_fw_status;
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX_MODULE
    struct mtk_chip_config spi_ctrl;
#endif
#if NVT_PM_WAIT_BUS_RESUME_COMPLETE
	bool dev_pm_suspend;
	struct completion dev_pm_resume_completion;
#endif
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
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
} SPI_EVENT_MAP;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(31*1024)
#define NVT_READ_LEN		(2*1024)
#define NVT_XBUF_LEN		(NVT_TRANSFER_LEN+1+DUMMY_BYTES)

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
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
void nvt_fw_crc_enable(void);
void nvt_tx_auto_copy_mode(void);
void nvt_read_fw_history(uint32_t fw_history_addr);
int32_t nvt_update_firmware(char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_wait_auto_copy(void);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int lct_nvt_tp_selftest_callback(unsigned char cmd);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#endif /* _LINUX_NVT_TOUCH_H */
