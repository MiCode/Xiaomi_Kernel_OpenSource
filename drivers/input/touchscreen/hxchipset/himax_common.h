/*
 * Himax Android Driver Sample Code for common functions
 *
 * Copyright (C) 2018 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef HIMAX_COMMON_H
#define HIMAX_COMMON_H

#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/firmware.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "himax_platform.h"

#ifdef CONFIG_DRM
	#include <linux/msm_drm_notify.h>
#elif defined(CONFIG_FB)
	#include <linux/notifier.h>
	#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_OF
	#include <linux/of_gpio.h>
#endif

#define HIMAX_DRIVER_VER "1.2.2.4_ABCD1234_01"

#define FLASH_DUMP_FILE "/sdcard/HX_Flash_Dump.bin"

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	#define HX_TP_PROC_2T2R
	/*#define HX_TP_SELF_TEST_DRIVER*/ /* if enable, selftest works in driver */
#endif
/*===========Himax Option function=============*/
#define HX_RST_PIN_FUNC
#define HX_RESUME_SEND_CMD
#define HX_ESD_RECOVERY
/*#define HX_AUTO_UPDATE_FW*/
/*#define HX_SMART_WAKEUP*/
/*#define HX_GESTURE_TRACK*/
/*#define HX_HIGH_SENSE*/
/*#define HX_PALM_REPORT*/
/*#define HX_USB_DETECT_GLOBAL*/
/*#define HX_USB_DETECT_CALLBACK*/
/*#define HX_PROTOCOL_A*/ /* for MTK special platform.If turning on,it will report to system by using specific format. */
/*#define HX_RESUME_HW_RESET*/
#define HX_PROTOCOL_B_3PA
/*#define HX_FIX_TOUCH_INFO*/ /* if open, you need to change the touch info in the fix_touch_info*/
/*#define HX_ZERO_FLASH*/
#define HX_ORG_SELFTEST
/*#define CONFIG_CHIP_DTCFG*/

/*#define HX_EN_SEL_BUTTON*/  /* Support Self Virtual key, default is close*/
/*#define HX_EN_MUT_BUTTON*/  /* Support Mutual Virtual Key, default is close*/

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
/*#define HX_PLATFOME_DEFINE_KEY*/  /* for specific platform to set key(button) */
#endif

#define HX_KEY_MAX_COUNT 4
#define DEFAULT_RETRY_CNT 3

#define HX_85XX_A_SERIES_PWON "HX85xxA"
#define HX_85XX_B_SERIES_PWON "HX85xxB"
#define HX_85XX_C_SERIES_PWON "HX85xxC"
#define HX_85XX_D_SERIES_PWON "HX85xxD"
#define HX_85XX_E_SERIES_PWON "HX85xxE"
#define HX_85XX_ES_SERIES_PWON "HX85xxES"
#define HX_85XX_F_SERIES_PWON "HX85xxF"
#define HX_85XX_H_SERIES_PWON "HX85xxH"
#define HX_83100A_SERIES_PWON "HX83100A"
#define HX_83102A_SERIES_PWON "HX83102A"
#define HX_83102B_SERIES_PWON "HX83102B"
#define HX_83102D_SERIES_PWON "HX83102D"
#define HX_83103A_SERIES_PWON "HX83103A"
#define HX_83110A_SERIES_PWON "HX83110A"
#define HX_83110B_SERIES_PWON "HX83110B"
#define HX_83111B_SERIES_PWON "HX83111B"
#define HX_83112A_SERIES_PWON "HX83112A"
#define HX_83112B_SERIES_PWON "HX83112B"
#define HX_83112D_SERIES_PWON "HX83112D"
#define HX_83112E_SERIES_PWON "HX83112E"
#define HX_83191A_SERIES_PWON "HX83191A"

#define HX_TP_BIN_CHECKSUM_SW 1
#define HX_TP_BIN_CHECKSUM_HW 2
#define HX_TP_BIN_CHECKSUM_CRC 3

#define SHIFTBITS 5

#define  FW_SIZE_32k 32768
#define  FW_SIZE_60k 61440
#define  FW_SIZE_64k 65536
#define  FW_SIZE_124k 126976
#define  FW_SIZE_128k 131072

#define NO_ERR 0
#define READY_TO_SERVE 1
#define WORK_OUT 2
#define I2C_FAIL -1
#define MEM_ALLOC_FAIL -2
#define CHECKSUM_FAIL -3
#define GESTURE_DETECT_FAIL -4
#define INPUT_REGISTER_FAIL -5
#define FW_NOT_READY -6
#define LENGTH_FAIL -7
#define OPEN_FILE_FAIL -8
#define ERR_WORK_OUT -10
#define HW_CRC_FAIL 1

#define HX_FINGER_ON 1
#define HX_FINGER_LEAVE	2


enum HX_TS_PATH {
	HX_REPORT_COORD = 1,
	HX_REPORT_SMWP_EVENT,
	HX_REPORT_COORD_RAWDATA,
};

enum HX_TS_STATUS {
	HX_TS_GET_DATA_FAIL = -4,
	HX_ESD_EVENT,
	HX_CHKSUM_FAIL,
	HX_PATH_FAIL,
	HX_TS_NORMAL_END = 0,
	HX_ESD_REC_OK,
	HX_READY_SERVE,
	HX_REPORT_DATA,
	HX_ESD_WARNING,
	HX_IC_RUNNING,
	HX_ZERO_EVENT_COUNT,
	HX_RST_OK,
};

enum cell_type {
	CHIP_IS_ON_CELL,
	CHIP_IS_IN_CELL
};
#ifdef HX_FIX_TOUCH_INFO
enum fix_touch_info {
	FIX_HX_RX_NUM = 0,
	FIX_HX_TX_NUM = 0,
	FIX_HX_BT_NUM = 0,
	FIX_HX_X_RES = 0,
	FIX_HX_Y_RES = 0,
	FIX_HX_MAX_PT = 0,
	FIX_HX_XY_REVERSE = false,
	FIX_HX_INT_IS_EDGE = true,
#ifdef HX_TP_PROC_2T2R
	FIX_HX_RX_NUM_2 = 0,
	FIX_HX_TX_NUM_2 = 0,
#endif
};
#endif

#ifdef HX_ZERO_FLASH
	#define HX_0F_DEBUG
#endif
struct himax_ic_data {
	int vendor_fw_ver;
	int vendor_config_ver;
	int vendor_touch_cfg_ver;
	int vendor_display_cfg_ver;
	int vendor_cid_maj_ver;
	int vendor_cid_min_ver;
	int vendor_panel_ver;
	int vendor_sensor_id;
	int HX_RX_NUM;
	int HX_TX_NUM;
	int HX_BT_NUM;
	int HX_X_RES;
	int HX_Y_RES;
	int HX_MAX_PT;
	bool HX_XY_REVERSE;
	bool HX_INT_IS_EDGE;
#ifdef HX_TP_PROC_2T2R
	int HX_RX_NUM_2;
	int HX_TX_NUM_2;
#endif
};

struct himax_virtual_key {
	int index;
	int keycode;
	int x_range_min;
	int x_range_max;
	int y_range_min;
	int y_range_max;
};

struct himax_target_report_data {
	int *x;
	int *y;
	int *w;
	int *finger_id;
	int finger_on;
	int finger_num;
#ifdef HX_PLATFORM_DEFINE_KEY
	int key_size;
	int *key_x;
	int *key_y;
	int *key_w;
#endif
#ifdef HX_SMART_WAKEUP
	int SMWP_event_chk;
#endif
};

struct himax_report_data {
	int touch_all_size;
	int raw_cnt_max;
	int raw_cnt_rmd;
	int touch_info_size;
	uint8_t	finger_num;
	uint8_t	finger_on;
	uint8_t *hx_coord_buf;
	uint8_t hx_state_info[2];
#if defined(HX_SMART_WAKEUP)
	int event_size;
	uint8_t *hx_event_buf;
#endif

	int rawdata_size;
	uint8_t diag_cmd;
	uint8_t *hx_rawdata_buf;
	uint8_t rawdata_frame_size;
};

struct himax_ts_data {
	bool suspended;
	atomic_t suspend_mode;
	uint8_t x_channel;
	uint8_t y_channel;
	uint8_t useScreenRes;
	uint8_t diag_cmd;
	char chip_name[30];
	uint8_t chip_cell_type;

	uint8_t protocol_type;
	uint8_t first_pressed;
	uint8_t coord_data_size;
	uint8_t area_data_size;
	uint8_t coordInfoSize;
	uint8_t raw_data_frame_size;
	uint8_t raw_data_nframes;
	uint8_t nFinger_support;
	uint8_t irq_enabled;
	uint8_t diag_self[50];

	uint16_t finger_pressed;
	uint16_t last_slot;
	uint16_t pre_finger_mask;
	uint16_t old_finger;
	int hx_point_num;

	uint32_t debug_log_level;
	uint32_t widthFactor;
	uint32_t heightFactor;
	uint32_t tw_x_min;
	uint32_t tw_x_max;
	uint32_t tw_y_min;
	uint32_t tw_y_max;
	uint32_t pl_x_min;
	uint32_t pl_x_max;
	uint32_t pl_y_min;
	uint32_t pl_y_max;

	int rst_gpio;
	int use_irq;
	int (*power)(int on);
	int pre_finger_data[10][2];

	struct device *dev;
	struct workqueue_struct *himax_wq;
	struct work_struct work;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct i2c_client *client;
	struct himax_i2c_platform_data *pdata;
	struct himax_virtual_key *button;
	struct mutex rw_lock;

/******* SPI-start *******/
	struct mutex spi_lock;
	struct spi_device *spi;
	int hx_irq;
/******* SPI-end *******/

	int in_self_test;

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
	struct workqueue_struct *himax_att_wq;
	struct delayed_work work_att;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif

	struct workqueue_struct *flash_wq;
	struct work_struct flash_work;

#ifdef HX_AUTO_UPDATE_FW
	struct workqueue_struct *himax_update_wq;
	struct delayed_work work_update;
#endif

#ifdef HX_ZERO_FLASH
	struct workqueue_struct *himax_0f_update_wq;
	struct delayed_work work_0f_update;
#endif

	struct workqueue_struct *himax_diag_wq;
	struct delayed_work himax_diag_delay_wrok;

#ifdef HX_SMART_WAKEUP
	uint8_t SMWP_enable;
	uint8_t gesture_cust_en[16];
	struct wakeup_source ts_SMWP_wake_src;
#endif

#ifdef HX_HIGH_SENSE
	uint8_t HSEN_enable;
#endif

#if defined(HX_USB_DETECT_CALLBACK) || defined(HX_USB_DETECT_GLOBAL)
	uint8_t usb_connected;
	uint8_t *cable_config;
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	struct workqueue_struct *ito_test_wq;
	struct work_struct ito_test_work;
#endif

};

struct himax_debug {
	bool flash_dump_going;
	void (*fp_ts_dbg_func)(struct himax_ts_data *ts, int start);
	int (*fp_set_diag_cmd)(struct himax_ic_data *ic_data, struct himax_report_data *hx_touch_data);
};

enum input_protocol_type {
	PROTOCOL_TYPE_A	= 0x00,
	PROTOCOL_TYPE_B	= 0x01,
};

#ifdef HX_HIGH_SENSE
	void himax_set_HSEN_func(uint8_t HSEN_enable);
#endif

#ifdef HX_SMART_WAKEUP
void himax_set_SMWP_func(uint8_t SMWP_enable);

#define GEST_PTLG_ID_LEN (4)
#define GEST_PTLG_HDR_LEN (4)
#define GEST_PTLG_HDR_ID1 (0xCC)
#define GEST_PTLG_HDR_ID2 (0x44)
#define GEST_PT_MAX_NUM (128)

enum gesture_event_type {
	EV_GESTURE_01 = 0x01,
	EV_GESTURE_02,
	EV_GESTURE_03,
	EV_GESTURE_04,
	EV_GESTURE_05,
	EV_GESTURE_06,
	EV_GESTURE_07,
	EV_GESTURE_08,
	EV_GESTURE_09,
	EV_GESTURE_10,
	EV_GESTURE_11,
	EV_GESTURE_12,
	EV_GESTURE_13,
	EV_GESTURE_14,
	EV_GESTURE_15,
	EV_GESTURE_PWR = 0x80,
};

#define KEY_CUST_01 251
#define KEY_CUST_02 252
#define KEY_CUST_03 253
#define KEY_CUST_04 254
#define KEY_CUST_05 255
#define KEY_CUST_06 256
#define KEY_CUST_07 257
#define KEY_CUST_08 258
#define KEY_CUST_09 259
#define KEY_CUST_10 260
#define KEY_CUST_11 261
#define KEY_CUST_12 262
#define KEY_CUST_13 263
#define KEY_CUST_14 264
#define KEY_CUST_15 265
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	extern uint8_t himax_ito_test(void);
#endif

extern struct himax_ts_data *private_ts;
extern struct himax_ic_data *ic_data;
extern struct himax_report_data *hx_touch_data;
extern struct himax_core_fp g_core_fp;
extern struct himax_debug *debug_data;
extern uint8_t HX_PROC_SEND_FLAG;

#ifdef HX_AUTO_UPDATE_FW
extern int g_i_FW_VER;
extern int g_i_CFG_VER;
extern int g_i_CID_MAJ; /* GUEST ID */
extern int g_i_CID_MIN; /* VER for GUEST */
extern unsigned char i_CTPM_FW[];
#endif

extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;

extern unsigned long FW_VER_MAJ_FLASH_LENG;
extern unsigned long FW_VER_MIN_FLASH_LENG;
extern unsigned long CFG_VER_MAJ_FLASH_LENG;
extern unsigned long CFG_VER_MIN_FLASH_LENG;
extern unsigned long CID_VER_MAJ_FLASH_LENG;
extern unsigned long CID_VER_MIN_FLASH_LENG;
extern unsigned char IC_CHECKSUM;

extern unsigned long FW_CFG_VER_FLASH_ADDR;

#ifdef HX_RST_PIN_FUNC
	extern u8 HX_HW_RESET_ACTIVATE;

	extern void himax_rst_gpio_set(int pinnum, uint8_t value);
#endif

/* void himax_HW_reset(uint8_t loadconfig,uint8_t int_off); */

extern int himax_chip_common_suspend(struct himax_ts_data *ts);
extern int himax_chip_common_resume(struct himax_ts_data *ts);
extern int himax_chip_common_init(void);
extern void himax_chip_common_deinit(void);
extern int himax_input_register(struct himax_ts_data *ts);
extern void himax_ts_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);
extern int himax_report_data_init(void);
extern struct proc_dir_entry *himax_touch_proc_dir;

extern int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata);
extern int himax_dev_set(struct himax_ts_data *ts);
extern int himax_input_register_device(struct input_dev *input_dev);

#ifdef HX_USB_DETECT_GLOBAL
extern void himax_cable_detect_func(bool force_renew);
#endif

/* ts_work about start */
extern struct himax_target_report_data *g_target_report_data;
extern int himax_report_data(struct himax_ts_data *ts, int ts_path, int ts_status);
/* ts_work about end */

#endif

