/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _HIMAX_COMMON_H_
#define _HIMAX_COMMON_H_

#include <asm/segment.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>

#include "himax_platform.h"
#include <linux/async.h>
#include <linux/buffer_head.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/types.h>
//#include <linux/wakelock.h>

#if defined(CONFIG_FB)
#include <linux/fb.h>
#include <linux/notifier.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#define HIMAX_DRIVER_VER "0.2.13.0_ABCD1234_01"

#define FLASH_DUMP_FILE "/sdcard/HX_Flash_Dump.bin"

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)

#define HX_TP_PROC_DIAG
#define HX_TP_PROC_REGISTER
#define HX_TP_PROC_DEBUG
#define HX_TP_PROC_FLASH_DUMP
#define HX_TP_PROC_SELF_TEST
#define HX_TP_PROC_RESET
#define HX_TP_PROC_SENSE_ON_OFF
#define HX_TP_PROC_2T2R
/* #define HX_TP_PROC_GUEST_INFO */

#ifdef HX_TP_PROC_SELF_TEST
/* #define HX_TP_SELF_TEST_DRIVER //if enable, selftest works in driver */
#define HX_ORG_SELFTEST
/* #define HX_INSPECT_LPWUG_TEST */
/* #define HX_DOZE_TEST */
#endif

int himax_touch_proc_init(void);
void himax_touch_proc_deinit(void);
#endif
/* ===========Himax Option function============= */
#define HX_RST_PIN_FUNC
#define HX_RESUME_SEND_CMD
#define HX_ESD_RECOVERY
/* #define HX_AUTO_UPDATE_FW */
/* #define HX_CHIP_STATUS_MONITOR		for ESD 2nd solution,it does not
 */
/* support incell,default off*/
/* #define HX_SMART_WAKEUP */
/* #define HX_GESTURE_TRACK */
/* #define HX_HIGH_SENSE */
/* #define HX_PALM_REPORT */
/* #define HX_USB_DETECT_GLOBAL */
/* #define HX_USB_DETECT_CALLBACK */
/* #define HX_PROTOCOL_A					 for MTK special
 */
/* platform.If turning on,it will report to system by using specific format. */
/* #define HX_RESUME_HW_RESET */
#define HX_PROTOCOL_B_3PA
/* #define HX_FIX_TOUCH_INFO	if open, you need to change the touch info */
/* in the fix_touch_info*/

/* #define HX_EN_SEL_BUTTON			 Support Self Virtual key */
/* ,default is close*/
/* #define HX_EN_MUT_BUTTON			 Support Mutual Virtual Key */
/* ,default is close*/

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
/* #define HX_PLATFOME_DEFINE_KEY		 for specific platform to set */
/* key(button) */
#endif

#define HX_KEY_MAX_COUNT 4
#define DEFAULT_RETRY_CNT 3

#define HX_85XX_A_SERIES_PWON 1
#define HX_85XX_B_SERIES_PWON 2
#define HX_85XX_C_SERIES_PWON 3
#define HX_85XX_D_SERIES_PWON 4
#define HX_85XX_E_SERIES_PWON 5
#define HX_85XX_ES_SERIES_PWON 6
#define HX_85XX_F_SERIES_PWON 7
#define HX_85XX_G_SERIES_PWON 8
#define HX_85XX_H_SERIES_PWON 9
#define HX_83100A_SERIES_PWON 10
#define HX_83102A_SERIES_PWON 11
#define HX_83102B_SERIES_PWON 12
#define HX_83103A_SERIES_PWON 13
#define HX_83110A_SERIES_PWON 14
#define HX_83110B_SERIES_PWON 15
#define HX_83111B_SERIES_PWON 16
#define HX_83112A_SERIES_PWON 17
#define HX_83112B_SERIES_PWON 18
#define HX_83191_SERIES_PWON 19

#define HX_TP_BIN_CHECKSUM_SW 1
#define HX_TP_BIN_CHECKSUM_HW 2
#define HX_TP_BIN_CHECKSUM_CRC 3

#define SHIFTBITS 5

#define FW_SIZE_32k 32768
#define FW_SIZE_60k 61440
#define FW_SIZE_64k 65536
#define FW_SIZE_124k 126976
#define FW_SIZE_128k 131072

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

#define HX_FINGER_ON 1
#define HX_FINGER_LEAVE 2

#define HX_REPORT_COORD 1
#define HX_REPORT_SMWP_EVENT 2

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
	FIX_HX_TX_NUM_2 = 0
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

struct himax_report_data {
	int touch_all_size;
	int raw_cnt_max;
	int raw_cnt_rmd;
	int touch_info_size;
	uint8_t finger_num;
	uint8_t finger_on;
	uint8_t *hx_coord_buf;
	uint8_t hx_state_info[2];
#if defined(HX_SMART_WAKEUP)
	int event_size;
	uint8_t *hx_event_buf;
#endif
#if defined(HX_TP_PROC_DIAG)
	int rawdata_size;
	uint8_t diag_cmd;
	uint8_t *hx_rawdata_buf;
	uint8_t rawdata_frame_size;
#endif
};

struct himax_ts_data {
	bool suspended;
	atomic_t suspend_mode;
	uint8_t x_channel;
	uint8_t y_channel;
	uint8_t useScreenRes;
	uint8_t diag_command;

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

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
	struct workqueue_struct *himax_att_wq;
	struct delayed_work work_att;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	struct workqueue_struct *himax_chip_monitor_wq;
	struct delayed_work himax_chip_monitor;
#endif
#ifdef HX_TP_PROC_FLASH_DUMP
	struct workqueue_struct *flash_wq;
	struct work_struct flash_work;
#endif
#ifdef HX_TP_PROC_GUEST_INFO
	struct workqueue_struct *guest_info_wq;
	struct work_struct guest_info_work;
#endif

#ifdef HX_AUTO_UPDATE_FW
	struct workqueue_struct *himax_update_wq;
	struct delayed_work work_update;
#endif

#ifdef HX_ZERO_FLASH
	struct workqueue_struct *himax_0f_update_wq;
	struct delayed_work work_0f_update;
#endif

#ifdef HX_TP_PROC_DIAG
	struct workqueue_struct *himax_diag_wq;
	struct delayed_work himax_diag_delay_wrok;
#endif

#ifdef HX_SMART_WAKEUP
	uint8_t SMWP_enable;
	uint8_t gesture_cust_en[16];
	struct wake_lock ts_SMWP_wake_lock;
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

#ifdef HX_CHIP_STATUS_MONITOR
struct chip_monitor_data {
	int HX_CHIP_POLLING_COUNT;
	int HX_POLLING_TIMER;   /* unit:sec */
	int HX_POLLING_TIMES;   /* ex:5(timer)x2(times)=10sec(polling time) */
	int HX_ON_HAND_SHAKING; /*  */
	int HX_CHIP_MONITOR_EN;
};
#endif

enum input_protocol_type {
	PROTOCOL_TYPE_A = 0x00,
	PROTOCOL_TYPE_B = 0x01,
};

#ifdef HX_HIGH_SENSE
void himax_set_HSEN_func(struct i2c_client *client, uint8_t HSEN_enable);
#endif

#ifdef HX_SMART_WAKEUP
#define GEST_PTLG_ID_LEN (4)
#define GEST_PTLG_HDR_LEN (4)
#define GEST_PTLG_HDR_ID1 (0xCC)
#define GEST_PTLG_HDR_ID2 (0x44)
#define GEST_PT_MAX_NUM (128)

void himax_set_SMWP_func(struct i2c_client *client, uint8_t SMWP_enable);

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

#if defined(HX_SMART_WAKEUP) || defined(HX_INSPECT_LPWUG_TEST)
extern bool FAKE_POWER_KEY_SEND;
#endif

extern int irq_enable_count;

/* void himax_HW_reset(uint8_t loadconfig,uint8_t int_off); */

#ifdef QCT
irqreturn_t himax_ts_thread(int irq, void *ptr);
int himax_input_register(struct himax_ts_data *ts);
#endif

extern int himax_chip_common_probe(struct i2c_client *client,
				   const struct i2c_device_id *id);
extern int himax_chip_common_remove(struct i2c_client *client);
extern int himax_chip_common_suspend(struct himax_ts_data *ts);
extern int himax_chip_common_resume(struct himax_ts_data *ts);

#ifdef HX_RST_PIN_FUNC
extern void himax_ic_reset(uint8_t loadconfig, uint8_t int_off);
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) ||                      \
	defined(HX_USB_DETECT_GLOBAL)
extern void himax_resend_cmd_func(bool suspended);
extern void himax_rst_cmd_recovery_func(bool suspended);
#endif

#ifdef HX_ESD_RECOVERY
extern int g_zero_event_count;
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
extern int himax_touch_proc_init(void);
extern void himax_touch_proc_deinit(void);
/* PROC-START */
#ifdef HX_TP_PROC_FLASH_DUMP
extern void himax_ts_flash_func(void);
extern void setFlashBuffer(void);
extern bool getFlashDumpGoing(void);
extern uint8_t getSysOperation(void);
extern void setSysOperation(uint8_t operation);
#endif
#ifdef HX_TP_PROC_GUEST_INFO
extern int himax_guest_info_get_status(void);
extern void himax_guest_info_set_status(int setting);
extern int himax_read_project_id(void);
#endif

#if defined(HX_PLATFOME_DEFINE_KEY)
extern void himax_platform_key(void);
#endif

#ifdef HX_TP_PROC_DIAG
extern void himax_ts_diag_func(void);

int32_t *getMutualBuffer(void);
int32_t *getMutualNewBuffer(void);
int32_t *getMutualOldBuffer(void);
int32_t *getSelfBuffer(void);
extern uint8_t getXChannel(void);
extern uint8_t getYChannel(void);
extern uint8_t getDiagCommand(void);
extern void setXChannel(uint8_t x);
extern void setYChannel(uint8_t y);
extern void setMutualBuffer(void);
extern void setMutualNewBuffer(void);
extern void setMutualOldBuffer(void);
extern uint8_t diag_coor[128];
extern int himax_set_diag_cmd(struct himax_ic_data *ic_data,
			      struct himax_report_data *hx_touch_data);
#ifdef HX_TP_PROC_2T2R
extern bool Is_2T2R;
int32_t *getMutualBuffer_2(void);
extern uint8_t getXChannel_2(void);
extern uint8_t getYChannel_2(void);
extern void setXChannel_2(uint8_t x);
extern void setYChannel_2(uint8_t y);
extern void setMutualBuffer_2(void);
#endif
#endif
/* PROC-END */
#endif

extern int himax_parse_dt(struct himax_ts_data *ts,
			  struct himax_i2c_platform_data *pdata);
extern bool himax_calculateChecksum(struct i2c_client *client,
				    bool change_iref);

#if defined(HX_TP_PROC_SELF_TEST) || defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
extern int g_self_test_entered;
#endif

extern int himax_get_touch_data_size(void);
/* void himax_HW_reset(uint8_t loadconfig,uint8_t int_off); */
extern void himax_log_touch_data(uint8_t *buf,
				 struct himax_report_data *hx_touch_data);
extern void himax_log_touch_event(int x, int y, int w, int loop_i,
				  uint8_t EN_NoiseFilter, int touched);
extern void himax_log_touch_event_detail(struct himax_ts_data *ts, int x, int y,
					 int w, int loop_i,
					 uint8_t EN_NoiseFilter, int touched,
					 uint16_t old_finger);

#if defined(HX_ESD_RECOVERY)
extern void himax_esd_ic_reset(void);
extern int himax_ic_esd_recovery(int hx_esd_event, int hx_zero_event,
				 int length);
#endif

extern int himax_dev_set(struct himax_ts_data *ts);
extern int himax_input_register_device(struct input_dev *input_dev);

#ifdef HX_ZERO_FLASH
extern void himax_0f_operation(struct work_struct *work);
#endif

#if defined(HX_PALM_REPORT)
int himax_palm_detect(uint8_t *buf);
#endif

int himax_report_data_init(void);

#endif
