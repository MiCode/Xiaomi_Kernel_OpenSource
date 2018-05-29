/* Himax Android Driver Sample Code for Himax chipset
*
* Copyright (C) 2015 Himax Corporation.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef HIMAX_COMMON_H
#define HIMAX_COMMON_H

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

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
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "himax_platform.h"

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#define HIMAX_DRIVER_VER "0.2.4.0"

#define FLASH_DUMP_FILE "/data/user/Flash_Dump.bin"
#define DIAG_COORDINATE_FILE "/sdcard/Coordinate_Dump.csv"

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)

#define HX_TP_PROC_DIAG
#define HX_TP_PROC_REGISTER
#define HX_TP_PROC_DEBUG
#define HX_TP_PROC_FLASH_DUMP
#define HX_TP_PROC_SELF_TEST
#define HX_TP_PROC_RESET
#define HX_TP_PROC_SENSE_ON_OFF
//#define HX_TP_PROC_2T2R

int himax_touch_proc_init(void);
void himax_touch_proc_deinit(void);
#endif

//===========Himax Option function=============
//#define HX_RST_PIN_FUNC
//#define HX_AUTO_UPDATE_FW
//#define HX_HIGH_SENSE
//#define HX_SMART_WAKEUP
//#define HX_USB_DETECT
//#define HX_ESD_WORKAROUND
//#define HX_USB_DETECT2

//#define HX_EN_SEL_BUTTON		       // Support Self Virtual key		,default is close
#define HX_EN_MUT_BUTTON		    // Support Mutual Virtual Key	,default is close

#define HX_KEY_MAX_COUNT             4			
#define DEFAULT_RETRY_CNT            3

#define HX_VKEY_0   KEY_BACK
#define HX_VKEY_1   KEY_HOME
#define HX_VKEY_2   KEY_RESERVED
#define HX_VKEY_3   KEY_RESERVED
#define HX_KEY_ARRAY    {HX_VKEY_0, HX_VKEY_1, HX_VKEY_2, HX_VKEY_3}

#define SHIFTBITS 5
//#define FLASH_SIZE 131072
#define  FW_SIZE_60k 	61440
#define  FW_SIZE_64k 	65536
#define  FW_SIZE_124k 	126976
#define  FW_SIZE_128k 	131072

struct himax_ic_data {
	int vendor_fw_ver;
	int vendor_config_ver;
	int vendor_sensor_id;
	int		HX_RX_NUM;
	int		HX_TX_NUM;
	int		HX_BT_NUM;
	int		HX_X_RES;
	int		HX_Y_RES;
	int		HX_MAX_PT;
	bool	HX_XY_REVERSE;
	bool	HX_INT_IS_EDGE;
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

struct himax_config {
	uint8_t  default_cfg;
	uint8_t  sensor_id;
	uint8_t  fw_ver;
	uint16_t length;
	uint32_t tw_x_min;
	uint32_t tw_x_max;
	uint32_t tw_y_min;
	uint32_t tw_y_max;
	uint32_t pl_x_min;
	uint32_t pl_x_max;
	uint32_t pl_y_min;
	uint32_t pl_y_max;
	uint8_t c1[11];
	uint8_t c2[11];
	uint8_t c3[11];
	uint8_t c4[11];
	uint8_t c5[11];
	uint8_t c6[11];
	uint8_t c7[11];
	uint8_t c8[11];
	uint8_t c9[11];
	uint8_t c10[11];
	uint8_t c11[11];
	uint8_t c12[11];
	uint8_t c13[11];
	uint8_t c14[11];
	uint8_t c15[11];
	uint8_t c16[11];
	uint8_t c17[11];
	uint8_t c18[17];
	uint8_t c19[15];
	uint8_t c20[5];
	uint8_t c21[11];
	uint8_t c22[4];
	uint8_t c23[3];
	uint8_t c24[3];
	uint8_t c25[4];
	uint8_t c26[2];
	uint8_t c27[2];
	uint8_t c28[2];
	uint8_t c29[2];
	uint8_t c30[2];
	uint8_t c31[2];
	uint8_t c32[2];
	uint8_t c33[2];
	uint8_t c34[2];
	uint8_t c35[3];
	uint8_t c36[5];
	uint8_t c37[5];
	uint8_t c38[9];
	uint8_t c39[14];
	uint8_t c40[159];
	uint8_t c41[99];
};

struct himax_ts_data {
	bool suspended;
	bool probe_done;
	struct mutex fb_mutex;
	atomic_t suspend_mode;
	uint8_t x_channel;
	uint8_t y_channel;
	uint8_t useScreenRes;
	uint8_t diag_command;
	
	uint8_t protocol_type;
	uint8_t first_pressed;
	uint8_t coord_data_size;
	uint8_t area_data_size;
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
	
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
	struct workqueue_struct 			*flash_wq;
	struct work_struct 					flash_work;
#endif

#ifdef HX_RST_PIN_FUNC
	int rst_gpio;
#endif

#ifdef HX_TP_PROC_DIAG
	struct workqueue_struct *himax_diag_wq;
	struct delayed_work himax_diag_delay_wrok;
#endif
#ifdef HX_SMART_WAKEUP
	uint8_t SMWP_enable;
	uint8_t gesture_cust_en[16];
	struct wake_lock ts_SMWP_wake_lock;
	struct workqueue_struct *himax_smwp_wq;
	struct delayed_work smwp_work;
#endif

#ifdef HX_HIGH_SENSE
	uint8_t HSEN_enable;
	struct workqueue_struct *himax_hsen_wq;
	struct delayed_work hsen_work;
#endif

#if defined(HX_USB_DETECT)||defined(HX_USB_DETECT2)
	uint8_t usb_connected;
	uint8_t *cable_config;
#endif

	/* pinctrl data */
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
};

#define HX_CMD_NOP					 0x00	
#define HX_CMD_SETMICROOFF			 0x35	
#define HX_CMD_SETROMRDY			 0x36	
#define HX_CMD_TSSLPIN				 0x80	
#define HX_CMD_TSSLPOUT 			 0x81	
#define HX_CMD_TSSOFF				 0x82	
#define HX_CMD_TSSON				 0x83	
#define HX_CMD_ROE					 0x85	
#define HX_CMD_RAE					 0x86	
#define HX_CMD_RLE					 0x87	
#define HX_CMD_CLRES				 0x88	
#define HX_CMD_TSSWRESET			 0x9E	
#define HX_CMD_SETDEEPSTB			 0xD7	
#define HX_CMD_SET_CACHE_FUN		 0xDD	
#define HX_CMD_SETIDLE				 0xF2	
#define HX_CMD_SETIDLEDELAY 		 0xF3	
#define HX_CMD_SELFTEST_BUFFER		 0x8D	
#define HX_CMD_MANUALMODE			 0x42
#define HX_CMD_FLASH_ENABLE 		 0x43
#define HX_CMD_FLASH_SET_ADDRESS	 0x44
#define HX_CMD_FLASH_WRITE_REGISTER  0x45
#define HX_CMD_FLASH_SET_COMMAND	 0x47
#define HX_CMD_FLASH_WRITE_BUFFER	 0x48
#define HX_CMD_FLASH_PAGE_ERASE 	 0x4D
#define HX_CMD_FLASH_SECTOR_ERASE	 0x4E
#define HX_CMD_CB					 0xCB
#define HX_CMD_EA					 0xEA
#define HX_CMD_4A					 0x4A
#define HX_CMD_4F					 0x4F
#define HX_CMD_B9					 0xB9
#define HX_CMD_76					 0x76

enum input_protocol_type {
	PROTOCOL_TYPE_A	= 0x00,
	PROTOCOL_TYPE_B	= 0x01,
};

#ifdef HX_HIGH_SENSE
void himax_set_HSEN_func(struct i2c_client *client,uint8_t HSEN_enable);
#endif

#ifdef HX_SMART_WAKEUP
#define GEST_PTLG_ID_LEN	(4)
#define GEST_PTLG_HDR_LEN	(4)
#define GEST_PTLG_HDR_ID1	(0xCC)
#define GEST_PTLG_HDR_ID2	(0x44)
#define GEST_PT_MAX_NUM 	(128)

#ifdef HX_GESTURE_TRACK
static int gest_pt_cnt;
static int gest_pt_x[GEST_PT_MAX_NUM];
static int gest_pt_y[GEST_PT_MAX_NUM];
static int gest_start_x,gest_start_y,gest_end_x,gest_end_y;
static int gest_width,gest_height,gest_mid_x,gest_mid_y;
static int gn_gesture_coor[16];
#endif

void himax_set_SMWP_func(struct i2c_client *client,uint8_t SMWP_enable);
extern bool FAKE_POWER_KEY_SEND;

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

#ifdef HX_ESD_WORKAROUND
	extern	u8 HX_ESD_RESET_ACTIVATE;
#endif

extern int irq_enable_count;

#ifdef QCT
irqreturn_t himax_ts_thread(int irq, void *ptr);
int himax_input_register(struct himax_ts_data *ts);
#endif

extern int himax_chip_common_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int himax_chip_common_remove(struct i2c_client *client);
extern int himax_chip_common_suspend(struct himax_ts_data *ts);
extern int himax_chip_common_resume(struct himax_ts_data *ts);
int himax_loadSensorConfig(struct i2c_client *client, struct himax_i2c_platform_data *pdata);

#ifdef HX_USB_DETECT2
//extern kal_bool upmu_is_chr_det(void);
void himax_cable_detect_func(void);
#endif

#endif

