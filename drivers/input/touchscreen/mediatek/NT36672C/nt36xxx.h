/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 63020 $
 * $Date: 2020-05-26 16:16:35 +0800 (周二, 26 5月 2020) $
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

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/hqsysfs.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

/* Huaqin modify for HQ-131657 by feiwen at 2021/06/03 start */
#define TP_RESUME_EN 1
/* Huaqin modify for HQ-131657 by feiwen at 2021/06/03 end */

/* Huaqin modify for HQ-131657 by liunianliang at 2021/06/16 start */
#define TP_SUSPEND_EN 1
/* Huaqin modify for HQ-131657 by liunianliang at 2021/06/16 end */

#define NVT_DEBUG 1
/*BSP.Tp - 2020.11.05 -add NVT_LOCKDOWN - start*/
#define NVT_LOCKDOWN 1
/*BSP.Tp - 2020.11.05 -add NVT_LOCKDOWN, end*/
//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943
/*BSP.Tp - 2020.11.05 -add NVT_LOCKDOWN - start*/
#define TP_LOCKDOWN_INFO "tp_lockdown_info"
#if NVT_LOCKDOWN
int32_t nvt_proc_tp_lockdown_info(void);
void nvt_lockdown_proc_deinit(void);
#endif
/*BSP.Tp - 2020.11.05 -add NVT_LOCKDOWN, end*/

//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING


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

#define NVT_IRQ_SWITCH
//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 2400
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#define	WAKEUP_OFF		0x04
#define	WAKEUP_ON		0x05
extern bool nvt_gesture_flag;
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/* Enable only when module have tp reset pin and connected to host */
/* Huaqin modify for TP not need tp reset by zhangjiangbin at 2021/07/13 start */
#define NVT_TOUCH_SUPPORT_HW_RST 0
/* Huaqin modify for TP not need tp reset by zhangjiangbin at 2021/07/13 end */

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1
#define TP_SELFTEST 1
#if	TP_SELFTEST
extern	int32_t	nvt_tp_selftest_proc_init(void);
extern	void	nvt_tp_selftest_proc_deinit(void);
#endif
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
/*BSP.TP - add tp compare - 20201116 - Start*/
extern char *BOOT_UPDATE_FIRMWARE_NAME;
extern char *MP_UPDATE_FIRMWARE_NAME;
/*BSP.TP - add tp compare - 20201116 - End*/
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

//---ESD Protect.---
/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/05 start */
#define NVT_TOUCH_ESD_PROTECT 1
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1
#define NVT_TOUCH_ESD_DISP_RECOVERY 1
#define NVT_TOUCH_VDD_TP_RECOVERY 1
/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/05 end */
struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
	struct notifier_block drm_notif;
#else
	struct notifier_block fb_notif;
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
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
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	bool palm_sensor_changed;
	bool palm_sensor_switch;
	#endif
/*BSP.Tp - 2020.11.05 -add NVT_LOCKDOWN - start*/
	char lockdowninfo[17];
/*BSP.Tp - 2020.11.05 -add NVT_LOCKDOWN - end*/
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
#endif
/*BSP.TP add nvt_irq modified in 20201111.Start*/
	spinlock_t irq_lock;
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
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 start */
#if NVT_TOUCH_ESD_DISP_RECOVERY
#define ILM_CRC_FLAG        0x01
#define DLM_CRC_FLAG        0x02
#define CRC_DONE            0x04
#define F2C_RW_READ         0x00
#define F2C_RW_WRITE        0x01
#define BIT_F2C_EN          0
#define BIT_F2C_RW          1
#define BIT_CPU_IF_ADDR_INC 2
#define BIT_CPU_POLLING_EN  5
#define FFM2CPU_CTL         0x3F280
#define F2C_LENGTH          0x3F283
#define CPU_IF_ADDR         0x3F284
#define FFM_ADDR            0x3F286
#define CP_TP_CPU_REQ       0x3F291
#define TOUCH_DATA_ADDR     0x20000
#define DISP_OFF_ADDR       0x2800
#endif /* NVT_TOUCH_ESD_DISP_RECOVERY */
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 end */
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
#endif /* #if NVT_TOUCH_ESD_PROTECT */
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 start */
#if NVT_TOUCH_VDD_TP_RECOVERY
void nvt_bootloader_reset_locked(void);
int32_t nvt_esd_vdd_tp_recovery(void);
#endif
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 end */
#endif /* _LINUX_NVT_TOUCH_H */
