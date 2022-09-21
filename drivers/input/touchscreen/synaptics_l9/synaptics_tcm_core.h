/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#ifndef _SYNAPTICS_TCM_CORE_H_
#define _SYNAPTICS_TCM_CORE_H_

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_tcm.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/proc_fs.h>
#include <linux/spi/spi.h>
#include <linux/of_gpio.h>

#include <drm/mi_disp_notifier.h>
#include <linux/fb.h>
#include <linux/notifier.h>

#include "../xiaomi/xiaomi_touch.h"
#define GRIP_RECT_NUM 12
#define GRIP_PARAMETER_NUM 8
#define TP_INFO_MAX_LENGTH 50

#define SYNAPTICS_DEBUGFS_ENABLE
#define SYNAPTICS_POWERSUPPLY_CB
enum charge_status {
	NOT_CHARGING,
	CHARGING,
	WIRED_CHARGING,
	WIRELESS_CHARGING,
};
extern int power_supply_is_system_supplied(void);

#ifdef SYNAPTICS_DEBUGFS_ENABLE
#include <linux/debugfs.h>
#endif

#include "./synaptics_tcm_xiaomi_board_data.h"

#define SYNAPTICS_TCM_ID_PRODUCT (1 << 0)
#define SYNAPTICS_TCM_ID_VERSION 0x0201
#define SYNAPTICS_TCM_ID_SUBVERSION 0

#define PLATFORM_DRIVER_NAME "synaptics_tcm"

#define TOUCH_INPUT_NAME "synaptics_tcm_touch"
#define TOUCH_INPUT_PHYS_PATH "synaptics_tcm/touch_input"

#define WAKEUP_GESTURE (1)

/* The chunk size RD_CHUNK_SIZE/WR_CHUNK_SIZE will not apply in HDL sensors */
#define RD_CHUNK_SIZE 256 /* read length limit in bytes, 0 = unlimited */
#define WR_CHUNK_SIZE 256 /* write length limit in bytes, 0 = unlimited */
#define HDL_RD_CHUNK_SIZE 0 /* For HDL, 0 = unlimited */
#define HDL_WR_CHUNK_SIZE 0 /* For HDL, 0 = unlimited */

#define MESSAGE_HEADER_SIZE 4
#define MESSAGE_MARKER 0xa5
#define MESSAGE_PADDING 0x5a

/*
#define REPORT_NOTIFIER
*/

/*
#define WATCHDOG_SW
*/
#ifdef WATCHDOG_SW
#define RUN_WATCHDOG false
#define WATCHDOG_TRIGGER_COUNT 2
#define WATCHDOG_DELAY_MS 1000
#endif

#define HOST_DOWNLOAD_WAIT_MS 100
#define HOST_DOWNLOAD_TIMEOUT_MS 5000

#define SYNA_TCM_XIAOMI_TOUCHFEATURE

#define PANEL_ORIENTATION_DEGREE_0    0      /* normal portrait orientation */
#define PANEL_ORIENTATION_DEGREE_90   1      /* anticlockwise 90 degrees */
#define PANEL_ORIENTATION_DEGREE_180  2      /* anticlockwise 180 degrees */
#define PANEL_ORIENTATION_DEGREE_270  3      /* anticlockwise 270 degrees */

#define USE_KOBJ_SYSFS (1)


#define LOGx(func, dev, log, ...) \
	func(dev, "%s info: " log, __func__, ##__VA_ARGS__)

#define LOGy(func, dev, log, ...) \
	func(dev, "%s：(line %d) " log, __func__, __LINE__, ##__VA_ARGS__)

#define LOGD(dev, log, ...) LOGx(dev_dbg, dev, log, ##__VA_ARGS__)
#define LOGI(dev, log, ...) LOGx(dev_info, dev, log, ##__VA_ARGS__)
#define LOGN(dev, log, ...) LOGx(dev_notice, dev, log, ##__VA_ARGS__)
#define LOGW(dev, log, ...) LOGy(dev_warn, dev, log, ##__VA_ARGS__)
#define LOGE(dev, log, ...) LOGy(dev_err, dev, log, ##__VA_ARGS__)

#define INIT_BUFFER(buffer, is_clone) \
	mutex_init(&buffer.buf_mutex); \
	buffer.clone = is_clone

#define LOCK_BUFFER(buffer) \
	mutex_lock(&buffer.buf_mutex)

#define UNLOCK_BUFFER(buffer) \
	mutex_unlock(&buffer.buf_mutex)

#define RELEASE_BUFFER(buffer) \
	do { \
		if (buffer.clone == false) { \
			kfree(buffer.buf); \
			buffer.buf_size = 0; \
			buffer.data_length = 0; \
		} \
	} while (0)

#define MAX(a, b) \
	({__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_a > _b ? _a : _b; })

#define MIN(a, b) \
	({__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_a < _b ? _a : _b; })

#define STR(x) #x

#define CONCAT(a, b) a##b

#define IS_NOT_FW_MODE(mode) \
	((mode != MODE_APPLICATION_FIRMWARE) && (mode != MODE_HOSTDOWNLOAD_FIRMWARE))

#define IS_FW_MODE(mode) \
	((mode == MODE_APPLICATION_FIRMWARE) || (mode == MODE_HOSTDOWNLOAD_FIRMWARE))

#define SHOW_PROTOTYPE(m_name, a_name) \
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_show)(struct device *dev, \
		struct device_attribute *attr, char *buf); \
\
static struct device_attribute dev_attr_##a_name = \
		__ATTR(a_name, S_IRUGO, \
		CONCAT(m_name##_sysfs, _##a_name##_show), \
		syna_tcm_store_error);

#define STORE_PROTOTYPE(m_name, a_name) \
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_store)(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count); \
\
static struct device_attribute dev_attr_##a_name = \
		__ATTR(a_name, (S_IWUSR | S_IWGRP), \
		syna_tcm_show_error, \
		CONCAT(m_name##_sysfs, _##a_name##_store));

#define SHOW_STORE_PROTOTYPE(m_name, a_name) \
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_show)(struct device *dev, \
		struct device_attribute *attr, char *buf); \
\
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_store)(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count); \
\
static struct device_attribute dev_attr_##a_name = \
		__ATTR(a_name, (S_IRUGO | S_IWUSR | S_IWGRP), \
		CONCAT(m_name##_sysfs, _##a_name##_show), \
		CONCAT(m_name##_sysfs, _##a_name##_store));

#define ATTRIFY(a_name) (&dev_attr_##a_name)

#if (USE_KOBJ_SYSFS)

#define KOBJ_SHOW_PROTOTYPE(m_name, a_name) \
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_show)(struct kobject *kobj, \
		struct kobj_attribute *attr, char *buf); \
\
static struct kobj_attribute kobj_attr_##a_name = \
		__ATTR(a_name, S_IRUGO, \
		CONCAT(m_name##_sysfs, _##a_name##_show), NULL);

#define KOBJ_STORE_PROTOTYPE(m_name, a_name) \
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_store)(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count); \
\
static struct kobj_attribute kobj_attr_##a_name = \
		__ATTR(a_name, (S_IWUSR | S_IWGRP), \
		NULL, CONCAT(m_name##_sysfs, _##a_name##_store));

#define KOBJ_SHOW_STORE_PROTOTYPE(m_name, a_name) \
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_show)(struct kobject *kobj, \
		struct kobj_attribute *attr, char *buf); \
\
static ssize_t CONCAT(m_name##_sysfs, _##a_name##_store)(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count); \
\
static struct kobj_attribute kobj_attr_##a_name = \
		__ATTR(a_name, (S_IRUGO | S_IWUSR | S_IWGRP), \
		CONCAT(m_name##_sysfs, _##a_name##_show), \
		CONCAT(m_name##_sysfs, _##a_name##_store));

#define KOBJ_ATTRIFY(a_name) (&kobj_attr_##a_name)

#endif

enum module_type {
	TCM_ZEROFLASH = 0,
	TCM_REFLASH = 1,
	TCM_DEVICE = 2,
	TCM_TESTING = 3,
	TCM_RECOVERY = 4,
	TCM_DIAGNOSTICS = 5,
	TCM_LAST,
};

enum boot_mode {
	MODE_APPLICATION_FIRMWARE = 0x01,
	MODE_HOSTDOWNLOAD_FIRMWARE = 0x02,
	MODE_ROMBOOTLOADER = 0x04,
	MODE_BOOTLOADER = 0x0b,
	MODE_TDDI_BOOTLOADER = 0x0c,
	MODE_TDDI_HOSTDOWNLOAD_BOOTLOADER = 0x0d,
	MODE_PRODUCTIONTEST_FIRMWARE = 0x0e,
};

enum sensor_types {
	TYPE_UNKNOWN = 0,
	TYPE_FLASH = 1,
	TYPE_F35 = 2,
	TYPE_ROMBOOT = 3,
};

enum boot_status {
	BOOT_STATUS_OK = 0x00,
	BOOT_STATUS_BOOTING = 0x01,
	BOOT_STATUS_APP_BAD_DISPLAY_CRC = 0xfc,
	BOOT_STATUS_BAD_DISPLAY_CONFIG = 0xfd,
	BOOT_STATUS_BAD_APP_FIRMWARE = 0xfe,
	BOOT_STATUS_WARM_BOOT = 0xff,
};

enum app_status {
	APP_STATUS_OK = 0x00,
	APP_STATUS_BOOTING = 0x01,
	APP_STATUS_UPDATING = 0x02,
	APP_STATUS_BAD_APP_CONFIG = 0xff,
};

enum firmware_mode {
	FW_MODE_BOOTLOADER = 0,
	FW_MODE_APPLICATION = 1,
	FW_MODE_PRODUCTION_TEST = 2,
};

enum dynamic_config_id {
	DC_UNKNOWN = 0x00,
	DC_NO_DOZE,
	DC_DISABLE_NOISE_MITIGATION,
	DC_INHIBIT_FREQUENCY_SHIFT,
	DC_REQUESTED_FREQUENCY,
	DC_DISABLE_HSYNC,
	DC_REZERO_ON_EXIT_DEEP_SLEEP,
	DC_CHARGER_CONNECTED,
	DC_NO_BASELINE_RELAXATION,
	DC_IN_WAKEUP_GESTURE_MODE,
	DC_STIMULUS_FINGERS,
	DC_GRIP_SUPPRESSION_ENABLED,
	DC_ENABLE_THICK_GLOVE,
	DC_ENABLE_GLOVE,
	DC_ENABLE_TOUCH_AND_HOLD = 0xD4,
	DC_GAME_MODE_CTRL        = 0xD5,
	DC_SWIPE_IIRFILTER       = 0xD8,
	DC_FAST_TAP_HYTERESIS    = 0xD9, /* UP_THRESHOLD */
	DC_MOTION_TOLERANCE      = 0xDA,
	DC_TAP_JITTER            = 0xDB,
	DC_PALM_AREA_CHANGE      = 0xDC,
	DC_SET_REPORT_RATE       = 0xE6,
	DC_GESTURE_TYPE_ENABLE   = 0xFE,
};

enum command {
	CMD_NONE = 0x00,
	CMD_CONTINUE_WRITE = 0x01,
	CMD_IDENTIFY = 0x02,
	CMD_RESET = 0x04,
	CMD_ENABLE_REPORT = 0x05,
	CMD_DISABLE_REPORT = 0x06,
	CMD_GET_BOOT_INFO = 0x10,
	CMD_ERASE_FLASH = 0x11,
	CMD_WRITE_FLASH = 0x12,
	CMD_READ_FLASH = 0x13,
	CMD_RUN_APPLICATION_FIRMWARE = 0x14,
	CMD_SPI_MASTER_WRITE_THEN_READ = 0x15,
	CMD_REBOOT_TO_ROM_BOOTLOADER = 0x16,
	CMD_RUN_BOOTLOADER_FIRMWARE = 0x1f,
	CMD_GET_APPLICATION_INFO = 0x20,
	CMD_GET_STATIC_CONFIG = 0x21,
	CMD_SET_STATIC_CONFIG = 0x22,
	CMD_GET_DYNAMIC_CONFIG = 0x23,
	CMD_SET_DYNAMIC_CONFIG = 0x24,
	CMD_GET_TOUCH_REPORT_CONFIG = 0x25,
	CMD_SET_TOUCH_REPORT_CONFIG = 0x26,
	CMD_REZERO = 0x27,
	CMD_COMMIT_CONFIG = 0x28,
	CMD_DESCRIBE_DYNAMIC_CONFIG = 0x29,
	CMD_PRODUCTION_TEST = 0x2a,
	CMD_SET_CONFIG_ID = 0x2b,
	CMD_ENTER_DEEP_SLEEP = 0x2c,
	CMD_EXIT_DEEP_SLEEP = 0x2d,
	CMD_GET_TOUCH_INFO = 0x2e,
	CMD_GET_DATA_LOCATION = 0x2f,
	CMD_DOWNLOAD_CONFIG = 0x30,
	CMD_ENTER_PRODUCTION_TEST_MODE = 0x31,
	CMD_GET_FEATURES = 0x32,
	CMD_GET_ROMBOOT_INFO = 0x40,
	CMD_WRITE_PROGRAM_RAM = 0x41,
	CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE = 0x42,
	CMD_SPI_MASTER_WRITE_THEN_READ_EXTENDED = 0x43,
	CMD_ENTER_IO_BRIDGE_MODE = 0x44,
	CMD_ROMBOOT_DOWNLOAD = 0x45,
	CMD_MultiFunction = 0xC7,
};

enum cmd_c7_sub_cmd {
	C7_SUB_CMD_SET_GRIP_ZONE = 0x00,
	C7_SUB_CMD_GET_GRIP_ZONE = 0x01,
};

enum status_code {
	STATUS_IDLE = 0x00,
	STATUS_OK = 0x01,
	STATUS_BUSY = 0x02,
	STATUS_CONTINUED_READ = 0x03,
	STATUS_CRC_ERROR = 0x0A,
	STATUS_NOT_EXECUTED_IN_DEEP_SLEEP = 0x0b,
	STATUS_RECEIVE_BUFFER_OVERFLOW = 0x0c,
	STATUS_PREVIOUS_COMMAND_PENDING = 0x0d,
	STATUS_NOT_IMPLEMENTED = 0x0e,
	STATUS_ERROR = 0x0f,
	STATUS_INVALID = 0xff,
};

enum report_type {
	REPORT_IDENTIFY = 0x10,
	REPORT_TOUCH = 0x11,
	REPORT_DELTA = 0x12,
	REPORT_RAW = 0x13,
	REPORT_STATUS = 0x1b,
	REPORT_PRINTF = 0x82,
	REPORT_RID161 = 0xA1,
	REPORT_HDL_ROMBOOT = 0xfd,
	REPORT_HDL_F35 = 0xfe,
};

enum command_status {
	CMD_IDLE = 0,
	CMD_BUSY = 1,
	CMD_ERROR = -1,
};

enum flash_area {
	BOOTLOADER = 0,
	BOOT_CONFIG,
	APP_FIRMWARE,
	APP_CONFIG,
	DISP_CONFIG,
	CUSTOM_OTP,
	CUSTOM_LCM,
	CUSTOM_OEM,
	PPDT,
};

enum flash_data {
	LCM_DATA = 1,
	OEM_DATA,
	PPDT_DATA,
};

enum helper_task {
	HELP_NONE = 0,
	HELP_RUN_APPLICATION_FIRMWARE,
	HELP_SEND_REINIT_NOTIFICATION,
	HELP_TOUCH_REINIT,
	HELP_SEND_ROMBOOT_HDL,
};

enum fod_status {
	FOD_STATUS_INVALID           = -1,
	FOD_STATUS_UNLOCKED          = 0,
	FOD_STATUS_UNLOCKING         = 1,
	FOD_STATUS_INPUT_FINGERPRINT = 2,
	FOD_STATUS_UNLOCK_FAILED     = 3,
	FOD_STATUS_DELETED           = 100,
};

enum grip_zone_type {
	CORNER_ZONE = 0x00,
	EDGE_ZONE   = 0x01,
	DEAD_ZONE   = 0x02,
	CORNERCASE2_ZONE = 0x03,
};

struct syna_tcm_helper {
	atomic_t task;
	struct work_struct work;
	struct workqueue_struct *workqueue;
};

struct syna_tcm_watchdog {
	bool run;
	unsigned char count;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
};

struct syna_tcm_buffer {
	bool clone;
	unsigned char *buf;
	unsigned int buf_size;
	unsigned int data_length;
	struct mutex buf_mutex;
};

struct syna_tcm_report {
	unsigned char id;
	struct syna_tcm_buffer buffer;
};

struct syna_tcm_identification {
	unsigned char version;
	unsigned char mode;
	unsigned char part_number[16];
	unsigned char build_id[4];
	unsigned char max_write_size[2];
};

struct syna_tcm_boot_info {
	unsigned char version;
	unsigned char status;
	unsigned char asic_id[2];
	unsigned char write_block_size_words;
	unsigned char erase_page_size_words[2];
	unsigned char max_write_payload_size[2];
	unsigned char last_reset_reason;
	unsigned char pc_at_time_of_last_reset[2];
	unsigned char boot_config_start_block[2];
	unsigned char boot_config_size_blocks[2];
	unsigned char display_config_start_block[4];
	unsigned char display_config_length_blocks[2];
	unsigned char backup_display_config_start_block[4];
	unsigned char backup_display_config_length_blocks[2];
	unsigned char custom_otp_start_block[2];
	unsigned char custom_otp_length_blocks[2];
};

struct syna_tcm_app_info {
	unsigned char version[2];
	unsigned char status[2];
	unsigned char static_config_size[2];
	unsigned char dynamic_config_size[2];
	unsigned char app_config_start_write_block[2];
	unsigned char app_config_size[2];
	unsigned char max_touch_report_config_size[2];
	unsigned char max_touch_report_payload_size[2];
	unsigned char customer_config_id[16];
	unsigned char max_x[2];
	unsigned char max_y[2];
	unsigned char max_objects[2];
	unsigned char num_of_buttons[2];
	unsigned char num_of_image_rows[2];
	unsigned char num_of_image_cols[2];
	unsigned char has_hybrid_data[2];
	unsigned char num_of_force_elecs[2];
};

struct syna_tcm_romboot_info {
	unsigned char version;
	unsigned char status;
	unsigned char asic_id[2];
	unsigned char write_block_size_words;
	unsigned char max_write_payload_size[2];
	unsigned char last_reset_reason;
	unsigned char pc_at_time_of_last_reset[2];
};

struct syna_tcm_touch_info {
	unsigned char image_2d_scale_factor[4];
	unsigned char image_0d_scale_factor[4];
	unsigned char hybrid_x_scale_factor[4];
	unsigned char hybrid_y_scale_factor[4];
};

struct syna_tcm_message_header {
	unsigned char marker;
	unsigned char code;
	unsigned char length[2];
};

struct syna_tcm_features {
	unsigned char byte_0_reserved;
	unsigned char byte_1_reserved;
	unsigned char dual_firmware:1;
	unsigned char byte_2_reserved:7;
} __packed;

struct syna_zone {
	unsigned char x0[2];
	unsigned char y0[2];
	unsigned char x1[2];
	unsigned char y1[2];
	unsigned char x2[2];
	unsigned char y2[2];
	unsigned char x3[2];
	unsigned char y3[2];
	unsigned char x4[2];
	unsigned char y4[2];
	unsigned char x5[2];
	unsigned char y5[2];
	unsigned char x6[2];
	unsigned char y6[2];
	unsigned char x7[2];
	unsigned char y7[2];
};

struct syna_grip_zone {
	unsigned char type; /* BIT[3:0]: Panel Orientaton; BIT?:Game Mode */
	unsigned char reserved;
	struct syna_zone edge_zone;
	struct syna_zone dead_zone;
	struct syna_zone corner_zone;
};

struct syna_tcm_hcd {
	struct regulator *avdd;
	struct regulator *iovdd;
	pid_t isr_pid;
	atomic_t command_status;
	atomic_t host_downloading;
	atomic_t firmware_flashing;
	wait_queue_head_t hdl_wq;
	wait_queue_head_t reflash_wq;
	int irq;
	bool do_polling;
	bool in_suspending;
	bool in_suspend;
	bool irq_enabled;
	bool in_hdl_mode;
	bool is_detected;
	bool wakeup_gesture_enabled;
	bool fod_enabled;
	bool fod_finger;
	bool in_sleep;
	bool fod_display_enabled;
	unsigned char sensor_type;
	unsigned char fb_ready;
	unsigned char command;
	unsigned char async_report_id;
	unsigned char status_report_code;
	unsigned char response_code;
	unsigned int read_length;
	unsigned int payload_length;
	unsigned int packrat_number;
	unsigned int rd_chunk_size;
	unsigned int wr_chunk_size;
	unsigned int app_status;
	struct platform_device *pdev;
	struct regulator *pwr_reg;
	struct regulator *bus_reg;
	struct kobject *sysfs_dir;
	struct kobject *dynamnic_config_sysfs_dir;
	struct mutex extif_mutex;
	struct mutex reset_mutex;
	struct mutex irq_en_mutex;
	struct mutex io_ctrl_mutex;
	struct mutex rw_ctrl_mutex;
	struct mutex command_mutex;
	struct mutex identify_mutex;
	struct delayed_work polling_work;
	struct workqueue_struct *polling_workqueue;
	struct task_struct *notifier_thread;
	bool tp_pm_suspend;
	struct completion pm_resume_completion;

	struct class *syna_tcm_class;
	struct device *syna_tcm_dev;
	dev_t tp_dev_num;
	struct notifier_block fb_notifier;
	struct notifier_block power_supply_notifier;

	int charging_status;
	struct delayed_work power_supply_work;

	struct syna_tcm_buffer in;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_buffer temp;
	struct syna_tcm_buffer config;
	struct syna_tcm_report report;
	struct syna_tcm_app_info app_info;
	struct syna_tcm_boot_info boot_info;
	struct syna_tcm_romboot_info romboot_info;
	struct syna_tcm_touch_info touch_info;
	struct syna_tcm_identification id_info;
	struct syna_tcm_helper helper;
	struct syna_tcm_watchdog watchdog;
	struct syna_tcm_features features;
	const struct syna_tcm_hw_interface *hw_if;
	int (*reset)(struct syna_tcm_hcd *tcm_hcd);
	int (*reset_n_reinit)(struct syna_tcm_hcd *tcm_hcd, bool hw, bool update_wd);
	int (*sleep)(struct syna_tcm_hcd *tcm_hcd, bool en);
	int (*identify)(struct syna_tcm_hcd *tcm_hcd, bool id);
	int (*enable_irq)(struct syna_tcm_hcd *tcm_hcd, bool en, bool ns);
	int (*switch_mode)(struct syna_tcm_hcd *tcm_hcd,
			enum firmware_mode mode);
	int (*read_message)(struct syna_tcm_hcd *tcm_hcd,
			unsigned char *in_buf, unsigned int length);
	int (*write_message)(struct syna_tcm_hcd *tcm_hcd,
			unsigned char command, unsigned char *payload,
			unsigned int length, unsigned char **resp_buf,
			unsigned int *resp_buf_size, unsigned int *resp_length,
			unsigned char *response_code,
			unsigned int polling_delay_ms);
	int (*get_dynamic_config)(struct syna_tcm_hcd *tcm_hcd,
			enum dynamic_config_id id, unsigned short *value);
	int (*set_dynamic_config)(struct syna_tcm_hcd *tcm_hcd,
			enum dynamic_config_id id, unsigned short value);
	int (*get_data_location)(struct syna_tcm_hcd *tcm_hcd,
			enum flash_area area, unsigned int *addr,
			unsigned int *length);
	int (*read_flash_data)(enum flash_area area, bool run_app_firmware,
			struct syna_tcm_buffer *output);
	int (*syna_tcm_lockdown_info)(void);
	void (*report_touch)(void);
	void (*update_watchdog)(struct syna_tcm_hcd *tcm_hcd, bool en);
	struct proc_dir_entry *tp_irq_debug;
	struct work_struct early_suspend_work;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct work_struct fod_work;
	struct work_struct set_report_rate_work;
	struct workqueue_struct *event_wq;
	int gamemode_enable;
	int fod_icon_status;
	int non_ui_status;
	int finger_unlock_status;
	int palm_sensor_enable;
	int palm_enable_status;
	int nonui_status;
	bool aod_enable;
	int fod_status;
	bool doubletap_enable;
	int power_status;
	int charger_connected;
	int report_rate_mode;
	unsigned int gesture_type;
	int maintenance_result;
	struct mutex cmd_update_mutex;

#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
	struct work_struct cmd_update_work;
	struct work_struct grip_mode_work;
	struct workqueue_struct *game_wq;
#endif

#ifdef SYNAPTICS_DEBUGFS_ENABLE
	struct dentry *debugfs;
#endif

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;

	bool lockdown_info_ready;
	char lockdown_info[8];
	struct proc_dir_entry *tp_selftest_proc;
	struct proc_dir_entry *tp_data_dump_proc;
	struct proc_dir_entry *tp_fw_version_proc;
	struct proc_dir_entry *tp_lockdown_info_proc;
	int (*testing_xiaomi_report_data)(int report_type, char *buf);
	int (*testing_xiaomi_self_test)(char *buf);
	int (*testing_xiaomi_chip_id_read)(void);

	struct syna_tcm_xiaomi_board_data xiaomi_board_data;
	struct mutex long_mode_value_mutex;
	struct mutex esd_recovery_mutex;
};

struct syna_tcm_module_cb {
	enum module_type type;
	int (*init)(struct syna_tcm_hcd *tcm_hcd);
	int (*remove)(struct syna_tcm_hcd *tcm_hcd);
	int (*syncbox)(struct syna_tcm_hcd *tcm_hcd);
#ifdef REPORT_NOTIFIER
	int (*asyncbox)(struct syna_tcm_hcd *tcm_hcd);
#endif
	int (*reinit)(struct syna_tcm_hcd *tcm_hcd);
	int (*suspend)(struct syna_tcm_hcd *tcm_hcd);
	int (*resume)(struct syna_tcm_hcd *tcm_hcd);
	int (*early_suspend)(struct syna_tcm_hcd *tcm_hcd);
};

struct syna_tcm_module_handler {
	bool insert;
	bool detach;
	struct list_head link;
	struct syna_tcm_module_cb *mod_cb;
};

struct syna_tcm_module_pool {
	bool initialized;
	bool queue_work;
	struct mutex mutex;
	struct list_head list;
	struct work_struct work;
	struct workqueue_struct *workqueue;
	struct syna_tcm_hcd *tcm_hcd;
};

struct syna_tcm_bus_io {
	unsigned char type;
	int (*rmi_read)(struct syna_tcm_hcd *tcm_hcd, unsigned short addr,
			unsigned char *data, unsigned int length);
	int (*rmi_write)(struct syna_tcm_hcd *tcm_hcd, unsigned short addr,
			unsigned char *data, unsigned int length);
	int (*read)(struct syna_tcm_hcd *tcm_hcd, unsigned char *data,
			unsigned int length);
	int (*write)(struct syna_tcm_hcd *tcm_hcd, unsigned char *data,
			unsigned int length);
};

struct syna_tcm_hw_interface {
	struct syna_tcm_board_data *bdata;
	const struct syna_tcm_bus_io *bus_io;
};

int syna_tcm_bus_init(void);

void syna_tcm_bus_exit(void);

int syna_tcm_add_module(struct syna_tcm_module_cb *mod_cb, bool insert);

int touch_init(struct syna_tcm_hcd *tcm_hcd);
int touch_remove(struct syna_tcm_hcd *tcm_hcd);
int touch_reinit(struct syna_tcm_hcd *tcm_hcd);
int touch_early_suspend(struct syna_tcm_hcd *tcm_hcd);
int touch_suspend(struct syna_tcm_hcd *tcm_hcd);
int touch_resume(struct syna_tcm_hcd *tcm_hcd);

static inline int syna_tcm_rmi_read(struct syna_tcm_hcd *tcm_hcd,
		unsigned short addr, unsigned char *data, unsigned int length)
{
	return tcm_hcd->hw_if->bus_io->rmi_read(tcm_hcd, addr, data, length);
}

static inline int syna_tcm_rmi_write(struct syna_tcm_hcd *tcm_hcd,
		unsigned short addr, unsigned char *data, unsigned int length)
{
	return tcm_hcd->hw_if->bus_io->rmi_write(tcm_hcd, addr, data, length);
}

static inline int syna_tcm_read(struct syna_tcm_hcd *tcm_hcd,
		unsigned char *data, unsigned int length)
{
	return tcm_hcd->hw_if->bus_io->read(tcm_hcd, data, length);
}

static inline int syna_tcm_write(struct syna_tcm_hcd *tcm_hcd,
		unsigned char *data, unsigned int length)
{
	return tcm_hcd->hw_if->bus_io->write(tcm_hcd, data, length);
}

static inline ssize_t syna_tcm_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_err("%s: Attribute not readable\n",
			__func__);

	return -EPERM;
}

static inline ssize_t syna_tcm_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	pr_err("%s: Attribute not writable\n",
			__func__);

	return -EPERM;
}

static inline int secure_memcpy(unsigned char *dest, unsigned int dest_size,
		const unsigned char *src, unsigned int src_size,
		unsigned int count)
{
	if (dest == NULL || src == NULL)
		return -EINVAL;

	if (count > dest_size || count > src_size) {
		pr_err("%s: src_size = %d, dest_size = %d, count = %d\n",
				__func__, src_size, dest_size, count);
		return -EINVAL;
	}

	memcpy((void *)dest, (const void *)src, count);

	return 0;
}

static inline int syna_tcm_realloc_mem(struct syna_tcm_hcd *tcm_hcd,
		struct syna_tcm_buffer *buffer, unsigned int size)
{
	int retval;
	unsigned char *temp;

	if (size > buffer->buf_size) {
		temp = buffer->buf;

		buffer->buf = kmalloc(size, GFP_KERNEL);
		if (!(buffer->buf)) {
			dev_err(tcm_hcd->pdev->dev.parent,
					"%s: Failed to allocate memory\n",
					__func__);
			kfree(temp);
			buffer->buf_size = 0;
			return -ENOMEM;
		}

		retval = secure_memcpy(buffer->buf,
				size,
				temp,
				buffer->buf_size,
				buffer->buf_size);
		if (retval < 0) {
			dev_err(tcm_hcd->pdev->dev.parent,
					"%s: Failed to copy data\n",
					__func__);
			kfree(temp);
			kfree(buffer->buf);
			buffer->buf_size = 0;
			return retval;
		}

		kfree(temp);
		buffer->buf_size = size;
	}

	return 0;
}

static inline int syna_tcm_alloc_mem(struct syna_tcm_hcd *tcm_hcd,
		struct syna_tcm_buffer *buffer, unsigned int size)
{
	if (size > buffer->buf_size) {
		kfree(buffer->buf);
		buffer->buf = kmalloc(size, GFP_KERNEL);
		if (!(buffer->buf)) {
			dev_err(tcm_hcd->pdev->dev.parent,
				"%s: Failed to allocate memory\n",
				__func__);
			dev_err(tcm_hcd->pdev->dev.parent,
				"%s: Allocation size = %d\n",
				__func__, size);
			buffer->buf_size = 0;
			buffer->data_length = 0;
			return -ENOMEM;
		}
		buffer->buf_size = size;
	}

	memset(buffer->buf, 0x00, buffer->buf_size);
	buffer->data_length = 0;

	return 0;
}

static inline unsigned int le2_to_uint(const unsigned char *src)
{
	return (unsigned int)src[0] +
			(unsigned int)src[1] * 0x100;
}

static inline unsigned int le4_to_uint(const unsigned char *src)
{
	return (unsigned int)src[0] +
			(unsigned int)src[1] * 0x100 +
			(unsigned int)src[2] * 0x10000 +
			(unsigned int)src[3] * 0x1000000;
}

static inline unsigned int ceil_div(unsigned int dividend, unsigned divisor)
{
	return (dividend + divisor - 1) / divisor;
}

int touch_free_objects(struct syna_tcm_hcd *tcm_hcd);

int touch_flush_slots(struct syna_tcm_hcd *tcm_hcd);

extern int mi_disp_set_fod_queue_work(u32 fod_btn, bool from_touch);

void touch_fod_test(int value);

#endif
