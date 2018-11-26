/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * $Revision: 21600 $
 * $Date: 2018-01-12 15:21:45 +0800 (週五, 12 一月 2018) $
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

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

#define PINCTRL_STATE_ACTIVE		"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND		"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE		"pmx_ts_release"

#define NVT_DEBUG 1

/*---GPIO number---*/
#define NVTTOUCH_INT_PIN 943


/*---INT trigger mode---*/
/*#define IRQ_TYPE_EDGE_RISING 1*/
/*#define IRQ_TYPE_EDGE_FALLING 2*/
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING


/*---I2C driver info.---*/
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

/*---Input device info.---*/
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"


/*---Touch info.---*/
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 2246
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/*---Customerized func.---*/
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
#define BOOT_UPDATE_FIRMWARE_NAME "novatek_nt36672_e10.fw"

/*---ESD Protect.---*/
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_LOCKDOWN_SIZE	8

#define NVT_TOUCH_COUNT_DUMP
#ifdef NVT_TOUCH_COUNT_DUMP
#define TOUCH_COUNT_FILE_MAXSIZE 50
#endif

struct nvt_config_info {
	u8 tp_vendor;
	u8 tp_color;
	u8 tp_hw_version;
	const char *nvt_cfg_name;
	const char *nvt_limit_name;
#ifdef NVT_TOUCH_COUNT_DUMP
	const char *clicknum_file_name;
#endif
};

struct nvt_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct nvt_work;
	struct delayed_work nvt_fwu_work;
	struct regulator *vddio_reg;
	struct regulator *lab_reg;
	struct regulator *ibb_reg;
	struct nvt_config_info *config_array;
	struct attribute_group attrs;
	struct proc_dir_entry *tp_selftest_proc;
	struct dentry *debugfs;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	const char *vddio_reg_name;
	const char *lab_reg_name;
	const char *ibb_reg_name;
	const char *fw_name;

	int result_type;
	u8 lockdown_info[NVT_LOCKDOWN_SIZE];
	uint16_t addr;
	int8_t phys[32];
#if defined(CONFIG_DRM)
	struct notifier_block notifier;
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
	int32_t reset_tddi;
	struct mutex lock;
	struct mutex mdata_lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
	uint16_t nvt_pid;

	int gesture_enabled;
	int glove_enabled;
	int charger_enabled;
	int stylus_enabled;
	int cover_enabled;
	int grip_enabled;
	int dbclick_count;
	size_t config_array_size;
	int current_index;
	bool dev_pm_suspend;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct workqueue_struct *event_wq;
	struct completion dev_pm_suspend_completion;
#ifdef NVT_TOUCH_COUNT_DUMP
	struct class *nvt_tp_class;
	struct device *nvt_touch_dev;
	bool dump_click_count;
	char *current_clicknum_file;
#endif

};

struct nvt_mode_switch {
	struct nvt_ts_data *nvt_data;
	unsigned char mode;
	struct work_struct switch_mode_work;
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
	struct i2c_client *client;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,/* IC reset */
	RESET_STATE_REK,		/* ReK baseline */
	RESET_STATE_REK_FINISH,	/* baseline is ready */
	RESET_STATE_NORMAL_RUN,	/* normal run */
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
    EVENT_MAP_HOST_CMD                      = 0x50,
    EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
    EVENT_MAP_RESET_COMPLETE                = 0x60,
    EVENT_MAP_FWINFO                        = 0x78,
    EVENT_MAP_PROJECTID                     = 0x9A,
} I2C_EVENT_MAP;

/*---extern structures---*/
extern struct nvt_ts_data *ts;

/*---extern functions---*/
extern int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern int nvt_bootloader_reset(void);
extern void nvt_sw_reset_idle(void);
extern int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
extern int32_t nvt_get_fw_info(void);
extern int32_t nvt_clear_fw_status(void);
extern int32_t nvt_check_fw_status(void);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
extern void nvt_stop_crc_reboot(void);
extern int32_t nvt_get_lockdown_info(char *lockdata);

#endif /* _LINUX_NVT_TOUCH_H */
