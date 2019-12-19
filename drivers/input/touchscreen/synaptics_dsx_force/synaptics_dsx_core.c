/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/input/synaptics_dsx.h>
#include <linux/hwinfo.h>
#include "synaptics_dsx_core.h"

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#if defined(CONFIG_SECURE_TOUCH)
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/errno.h>
#endif

#ifdef CONFIG_TOUCH_DEBUG_FS
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#endif
#include <drm/drm_notifier.h>
#include <drm/drm_panel.h>
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
#include <../xiaomi/xiaomi_touch.h>
#endif

#define INPUT_PHYS_NAME "synaptics_dsx/touch_input"
#define STYLUS_PHYS_NAME "synaptics_dsx/stylus"

#define VIRTUAL_KEY_MAP_FILE_NAME "virtualkeys." PLATFORM_DRIVER_NAME

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

#define WAKEUP_GESTURE false

#define NO_0D_WHILE_2D
#define REPORT_2D_Z
#define REPORT_2D_W

#define REPORT_2D_PRESSURE


#define F12_DATA_15_WORKAROUND

#define IGNORE_FN_INIT_FAILURE
/*
#define FB_READY_RESET
#define FB_READY_WAIT_MS 100
#define FB_READY_TIMEOUT_S 30
*/
#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define REBUILD_WORK_DELAY_MS 500 /* ms */

#define EXP_FN_WORK_DELAY_MS 500 /* ms */
#define MAX_F11_TOUCH_WIDTH 15
#define MAX_F12_TOUCH_WIDTH 255
#define MAX_F12_TOUCH_PRESSURE 255

#define CHECK_STATUS_TIMEOUT_MS 100

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F01_PROD_ID_OFFSET 11

#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CONFIGURED (1 << 7)

#define F11_CONTINUOUS_MODE 0x00
#define F11_WAKEUP_GESTURE_MODE 0x04
#define F12_CONTINUOUS_MODE 0x00
#define F12_WAKEUP_GESTURE_MODE 0x02
#define F12_UDG_DETECT 0x0f
#define F12_HOMEKEY_DETECT 0x0c

#define DOUBLE_TAP	0x01
#define HOMEKEY_WAKEUP	0x80
#define F51_CUSTOM_PALM_CTL 0x063B
#define F12_2D_CTRL23  0x001A
#define F51_CUSTOM_CTRL109 0x063B
#define F51_CUSTOM_CTRL110 0x063C
#define F51_CUSTOM_CTRL111 0x063D
#define F51_CUSTOM_CTRL112 0x063E
#define F51_CUSTOM_CTRL113 0x063F
#define F51_CUSTOM_CTRL114 0x0640
#define F51_CUSTOM_CTRL115 0x0641

#define F54_FORCE_UPDATE 0x169

#define INPUT_EVENT_START			0
#define INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define INPUT_EVENT_SENSITIVE_MODE_ON		1
#define INPUT_EVENT_STYLUS_MODE_OFF		2
#define INPUT_EVENT_STYLUS_MODE_ON		3
#define INPUT_EVENT_WAKUP_MODE_OFF		4
#define INPUT_EVENT_WAKUP_MODE_ON		5
#define INPUT_EVENT_COVER_MODE_OFF		6
#define INPUT_EVENT_COVER_MODE_ON		7
#define INPUT_EVENT_END				7

#define BUTTON_WG_EN				1

static int synaptics_rmi4_check_status(struct synaptics_rmi4_data *rmi4_data,
		bool *was_in_bl_mode);
static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data,
		bool rebuild);
static void synaptics_rmi4_sleep_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable);
static void synaptics_rmi4_wakeup_gesture(struct synaptics_rmi4_data *rmi4_data,
		bool enable);
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable, bool attn_only);
static void synaptics_rmi4_wakeup_reconfigure(struct synaptics_rmi4_data *rmi4_data,
		bool enable);

#if defined(CONFIG_SECURE_TOUCH)
static ssize_t synaptics_secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_secure_touch_show(struct device *dev,
	    struct device_attribute *attr, char *buf);
#endif

#ifdef CONFIG_DRM
static int synaptics_rmi4_drm_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data);
static int synaptics_rmi4_drm_notifier_cb_tddi(struct notifier_block *self,
		unsigned long event, void *data);
#endif

#define DISP_REG_VDD (1<<0)
#define DISP_REG_LAB (1<<1)
#define DISP_REG_IBB (1<<2)
#define DISP_REG_ALL (DISP_REG_VDD | DISP_REG_LAB | DISP_REG_IBB)

static void drm_regulator_ctrl(struct synaptics_rmi4_data *rmi4_data, unsigned int flag, bool enable);
static void drm_reset_ctrl(const struct synaptics_dsx_board_data *bdata, bool on);
static void drm_reset_action(const struct synaptics_dsx_board_data *bdata);

#ifdef CONFIG_HAS_EARLYSUSPEND
#ifndef CONFIG_DRM
#define USE_EARLYSUSPEND
#endif
#endif

#ifdef USE_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h);

static void synaptics_rmi4_late_resume(struct early_suspend *h);
#endif
static irqreturn_t synaptics_rmi4_irq(int irq, void *data);
static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_wake_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_wake_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_irq_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_irq_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_panel_color_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_panel_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_virtual_key_map_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
static int synaptics_rmi4_palm_enable(struct synaptics_rmi4_data *rmi4_data,
		int on);

static ssize_t synaptics_rmi4_palm_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_palm_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_palm_tx_grip_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_palm_tx_grip_disable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_palm_tx_area_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_palm_tx_area_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_palm_tx_channel_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_palm_tx_channel_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_palm_rx_area_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_palm_rx_area_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_palm_rx_channel_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_palm_rx_channel_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int synaptics_rmi4_palm_enable(struct synaptics_rmi4_data *rmi4_data, int on);
#endif
struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_0_5 {
	union {
		struct {
			/* query 0 */
			unsigned char f11_query0_b0__2:3;
			unsigned char has_query_9:1;
			unsigned char has_query_11:1;
			unsigned char has_query_12:1;
			unsigned char has_query_27:1;
			unsigned char has_query_28:1;

			/* query 1 */
			unsigned char num_of_fingers:3;
			unsigned char has_rel:1;
			unsigned char has_abs:1;
			unsigned char has_gestures:1;
			unsigned char has_sensitibity_adjust:1;
			unsigned char f11_query1_b7:1;

			/* query 2 */
			unsigned char num_of_x_electrodes;

			/* query 3 */
			unsigned char num_of_y_electrodes;

			/* query 4 */
			unsigned char max_electrodes:7;
			unsigned char f11_query4_b7:1;

			/* query 5 */
			unsigned char abs_data_size:2;
			unsigned char has_anchored_finger:1;
			unsigned char has_adj_hyst:1;
			unsigned char has_dribble:1;
			unsigned char has_bending_correction:1;
			unsigned char has_large_object_suppression:1;
			unsigned char has_jitter_filter:1;
		} __packed;
		unsigned char data[6];
	};
};

struct synaptics_rmi4_f11_query_7_8 {
	union {
		struct {
			/* query 7 */
			unsigned char has_single_tap:1;
			unsigned char has_tap_and_hold:1;
			unsigned char has_double_tap:1;
			unsigned char has_early_tap:1;
			unsigned char has_flick:1;
			unsigned char has_press:1;
			unsigned char has_pinch:1;
			unsigned char has_chiral_scroll:1;

			/* query 8 */
			unsigned char has_palm_detect:1;
			unsigned char has_rotate:1;
			unsigned char has_touch_shapes:1;
			unsigned char has_scroll_zones:1;
			unsigned char individual_scroll_zones:1;
			unsigned char has_multi_finger_scroll:1;
			unsigned char has_multi_finger_scroll_edge_motion:1;
			unsigned char has_multi_finger_scroll_inertia:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f11_query_9 {
	union {
		struct {
			unsigned char has_pen:1;
			unsigned char has_proximity:1;
			unsigned char has_large_object_sensitivity:1;
			unsigned char has_suppress_on_large_object_detect:1;
			unsigned char has_two_pen_thresholds:1;
			unsigned char has_contact_geometry:1;
			unsigned char has_pen_hover_discrimination:1;
			unsigned char has_pen_hover_and_edge_filters:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_12 {
	union {
		struct {
			unsigned char has_small_object_detection:1;
			unsigned char has_small_object_detection_tuning:1;
			unsigned char has_8bit_w:1;
			unsigned char has_2d_adjustable_mapping:1;
			unsigned char has_general_information_2:1;
			unsigned char has_physical_properties:1;
			unsigned char has_finger_limit:1;
			unsigned char has_linear_cofficient_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_27 {
	union {
		struct {
			unsigned char f11_query27_b0:1;
			unsigned char has_pen_position_correction:1;
			unsigned char has_pen_jitter_filter_coefficient:1;
			unsigned char has_group_decomposition:1;
			unsigned char has_wakeup_gesture:1;
			unsigned char has_small_finger_correction:1;
			unsigned char has_data_37:1;
			unsigned char f11_query27_b7:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_ctrl_6_9 {
	union {
		struct {
			unsigned char sensor_max_x_pos_7_0;
			unsigned char sensor_max_x_pos_11_8:4;
			unsigned char f11_ctrl7_b4__7:4;
			unsigned char sensor_max_y_pos_7_0;
			unsigned char sensor_max_y_pos_11_8:4;
			unsigned char f11_ctrl9_b4__7:4;
		} __packed;
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f11_data_1_5 {
	union {
		struct {
			unsigned char x_position_11_4;
			unsigned char y_position_11_4;
			unsigned char x_position_3_0:4;
			unsigned char y_position_3_0:4;
			unsigned char wx:4;
			unsigned char wy:4;
			unsigned char z;
		} __packed;
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl32_is_present:1;
				unsigned char ctrl33_is_present:1;
				unsigned char ctrl34_is_present:1;
				unsigned char ctrl35_is_present:1;
				unsigned char ctrl36_is_present:1;
				unsigned char ctrl37_is_present:1;
				unsigned char ctrl38_is_present:1;
				unsigned char ctrl39_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl40_is_present:1;
				unsigned char ctrl41_is_present:1;
				unsigned char ctrl42_is_present:1;
				unsigned char ctrl43_is_present:1;
				unsigned char ctrl44_is_present:1;
				unsigned char ctrl45_is_present:1;
				unsigned char ctrl46_is_present:1;
				unsigned char ctrl47_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl48_is_present:1;
				unsigned char ctrl49_is_present:1;
				unsigned char ctrl50_is_present:1;
				unsigned char ctrl51_is_present:1;
				unsigned char ctrl52_is_present:1;
				unsigned char ctrl53_is_present:1;
				unsigned char ctrl54_is_present:1;
				unsigned char ctrl55_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl56_is_present:1;
				unsigned char ctrl57_is_present:1;
				unsigned char ctrl58_is_present:1;
				unsigned char ctrl59_is_present:1;
				unsigned char ctrl60_is_present:1;
				unsigned char ctrl61_is_present:1;
				unsigned char ctrl62_is_present:1;
				unsigned char ctrl63_is_present:1;
			} __packed;
		};
		unsigned char data[9];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __packed;
			struct {
				unsigned char data16_is_present:1;
				unsigned char data17_is_present:1;
				unsigned char data18_is_present:1;
				unsigned char data19_is_present:1;
				unsigned char data20_is_present:1;
				unsigned char data21_is_present:1;
				unsigned char data22_is_present:1;
				unsigned char data23_is_present:1;
			} __packed;
			struct {
				unsigned char data24_is_present:1;
				unsigned char data25_is_present:1;
				unsigned char data26_is_present:1;
				unsigned char data27_is_present:1;
				unsigned char data28_is_present:1;
				unsigned char data29_is_present:1;
				unsigned char data30_is_present:1;
				unsigned char data31_is_present:1;
			} __packed;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char finger_enable:1;
			unsigned char active_stylus_enable:1;
			unsigned char palm_enable:1;
			unsigned char unclassified_object_enable:1;
			unsigned char hovering_finger_enable:1;
			unsigned char gloved_finger_enable:1;
			unsigned char f12_ctr23_00_b6__7:2;
			unsigned char max_reported_objects;
			unsigned char f12_ctr23_02_b0:1;
			unsigned char report_active_stylus_as_finger:1;
			unsigned char report_palm_as_finger:1;
			unsigned char report_unclassified_object_as_finger:1;
			unsigned char report_hovering_finger_as_finger:1;
			unsigned char report_gloved_finger_as_finger:1;
			unsigned char report_narrow_object_swipe_as_finger:1;
			unsigned char report_handedge_as_finger:1;
			unsigned char cover_enable:1;
			unsigned char stylus_enable:1;
			unsigned char eraser_enable:1;
			unsigned char small_object_enable:1;
			unsigned char f12_ctr23_03_b4__7:4;
			unsigned char report_cover_as_finger:1;
			unsigned char report_stylus_as_finger:1;
			unsigned char report_eraser_as_finger:1;
			unsigned char report_small_object_as_finger:1;
			unsigned char f12_ctr23_04_b4__7:4;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_ctrl_31 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char rx_clip_low;
			unsigned char rx_clip_high;
			unsigned char wedge_clip_low;
			unsigned char wedge_clip_high;
			unsigned char num_of_p;
			unsigned char num_of_q;
		};
		unsigned char data[12];
	};
};

struct synaptics_rmi4_f12_ctrl_58 {
	union {
		struct {
			unsigned char reporting_format;
			unsigned char f12_ctr58_00_reserved;
			unsigned char min_force_lsb;
			unsigned char min_force_msb;
			unsigned char max_force_lsb;
			unsigned char max_force_msb;
			unsigned char light_press_threshold_lsb;
			unsigned char light_press_threshold_msb;
			unsigned char light_press_hysteresis_lsb;
			unsigned char light_press_hysteresis_msb;
			unsigned char hard_press_threshold_lsb;
			unsigned char hard_press_threshold_msb;
			unsigned char hard_press_hysteresis_lsb;
			unsigned char hard_press_hysteresis_msb;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
#ifdef REPORT_2D_Z
	unsigned char z;
#endif
#ifdef REPORT_2D_W
	unsigned char wx;
	unsigned char wy;
#endif
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char f1a_query0_b3__4:2;
			unsigned char has_query4:1;
			unsigned char has_query3:1;
			unsigned char has_query2:1;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_query_4 {
	union {
		struct {
			unsigned char has_ctrl19:1;
			unsigned char f1a_query4_b1__4:4;
			unsigned char has_ctrl24:1;
			unsigned char f1a_query4_b6__7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	unsigned char button_int_enable;
	unsigned char multi_button;
	unsigned char *txrx_map;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

struct synaptics_rmi4_exp_fhandler {
	struct synaptics_rmi4_exp_fn *exp_fn;
	bool insert;
	bool remove;
	struct list_head link;
};

struct synaptics_rmi4_exp_fn_data {
	bool initialized;
	bool queue_work;
	struct mutex mutex;
	struct list_head list;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct synaptics_rmi4_data *rmi4_data;
};

static struct synaptics_rmi4_exp_fn_data exp_data;

static struct synaptics_dsx_button_map *vir_button_map;

static struct device_attribute attrs[] = {
	__ATTR(reset, S_IWUSR,
			synaptics_rmi4_show_error,
			synaptics_rmi4_f01_reset_store),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUGO,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(0dbutton, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
	__ATTR(suspend, S_IWUSR,
			synaptics_rmi4_show_error,
			synaptics_rmi4_suspend_store),
	__ATTR(wake_gesture, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_wake_gesture_show,
			synaptics_rmi4_wake_gesture_store),
	__ATTR(irq_enable, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_irq_enable_show,
			synaptics_rmi4_irq_enable_store),
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	__ATTR(palm_enable, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_palm_enable_show,
			synaptics_rmi4_palm_enable_store),
	__ATTR(palm_rx_area, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_palm_rx_area_threshold_show,
			synaptics_rmi4_palm_rx_area_threshold_store),
	__ATTR(tx_grip_disable, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_palm_tx_grip_disable_show,
			synaptics_rmi4_palm_tx_grip_disable_store),
	__ATTR(palm_rx_channel, (S_IRUGO | S_IWUSR),
		synaptics_rmi4_palm_rx_channel_threshold_show,
		synaptics_rmi4_palm_rx_channel_threshold_store),
	__ATTR(palm_tx_area, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_palm_tx_area_threshold_show,
			synaptics_rmi4_palm_tx_area_threshold_store),
	__ATTR(palm_tx_channel, (S_IRUGO | S_IWUSR),
		synaptics_rmi4_palm_tx_channel_threshold_show,
		synaptics_rmi4_palm_tx_channel_threshold_store),
#endif
};

#if defined(CONFIG_SECURE_TOUCH)
static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP), synaptics_secure_touch_enable_show, synaptics_secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO , synaptics_secure_touch_show, NULL);
#if 0
static int synaptics_secure_touch_clk_prepare_enable(
		struct synaptics_rmi4_data *rmi4_data)
{
	int ret;

	ret = clk_prepare_enable(rmi4_data->iface_clk);
	if (ret) {
		dev_err(rmi4_data->pdev->dev.parent,
			"error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(rmi4_data->core_clk);
	if (ret) {
		clk_disable_unprepare(rmi4_data->iface_clk);
		dev_err(rmi4_data->pdev->dev.parent,
			"error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void synaptics_secure_touch_clk_disable_unprepare(
		struct synaptics_rmi4_data *rmi4_data)
{
	clk_disable_unprepare(rmi4_data->core_clk);
	clk_disable_unprepare(rmi4_data->iface_clk);
}
#endif
static void synaptics_secure_touch_init(struct synaptics_rmi4_data *data)
{
	//int ret = 0;

	data->st_initialized = 0;
	init_completion(&data->st_powerdown);
	init_completion(&data->st_irq_processed);
#if 0
	/* Get clocks */
	data->core_clk = clk_get(data->pdev->dev.parent, "core_clk");
	if (IS_ERR(data->core_clk)) {
		ret = PTR_ERR(data->core_clk);
		dev_err(data->pdev->dev.parent,
			"%s: error on clk_get(core_clk):%d\n", __func__, ret);
		return;
	}

	data->iface_clk = clk_get(data->pdev->dev.parent, "iface_clk");
	if (IS_ERR(data->iface_clk)) {
		ret = PTR_ERR(data->iface_clk);
		dev_err(data->pdev->dev.parent,
			"%s: error on clk_get(iface_clk)\n", __func__);
		goto err_iface_clk;
	}
#endif
	data->st_initialized = 1;
	return;
#if 0
err_iface_clk:
		clk_put(data->core_clk);
		data->core_clk = NULL;
#endif
}
static void synaptics_secure_touch_notify(struct synaptics_rmi4_data *data)
{
        sysfs_notify(&data->pdev->dev.parent->kobj, NULL, "secure_touch");
}
static irqreturn_t synaptics_filter_interrupt(struct synaptics_rmi4_data *data)
{
	if (atomic_read(&data->st_enabled)) {
		if (atomic_cmpxchg(&data->st_pending_irqs, 0, 1) == 0) {
			synaptics_secure_touch_notify(data);
			wait_for_completion_interruptible(
				&data->st_irq_processed);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
static void synaptics_secure_touch_stop(
	struct synaptics_rmi4_data *data,
	int blocking)
{
	if (atomic_read(&data->st_enabled)) {
		atomic_set(&data->st_pending_irqs, -1);
		synaptics_secure_touch_notify(data);
		if (blocking)
			wait_for_completion_interruptible(&data->st_powerdown);
	}
}
#else
static void synaptics_secure_touch_init(struct synaptics_rmi4_data *data)
{
}
static irqreturn_t synaptics_filter_interrupt(struct synaptics_rmi4_data *data)
{
	return IRQ_NONE;
}
static void synaptics_secure_touch_stop(
	struct synaptics_rmi4_data *data,
	int blocking)
{
}
#endif

#if defined(CONFIG_SECURE_TOUCH)
static ssize_t synaptics_secure_touch_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&data->st_enabled));
}
/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t synaptics_secure_touch_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = container_of(data->pdev->dev.parent, struct i2c_client, dev);
	struct device *adapter = client->adapter->dev.parent;
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!data->st_initialized)
		return -EIO;

	err = count;


	switch (value) {
	case 0:
		if (atomic_read(&data->st_enabled) == 0)
			break;

		//synaptics_secure_touch_clk_disable_unprepare(data);
		pm_runtime_put_sync(adapter);
		atomic_set(&data->st_enabled, 0);
		synaptics_secure_touch_notify(data);
		complete(&data->st_irq_processed);
		synaptics_rmi4_irq(data->irq, data);
		complete(&data->st_powerdown);

		break;
	case 1:
		if (atomic_read(&data->st_enabled)) {
			err = -EBUSY;
			break;
		}

		synchronize_irq(data->irq);
		if (pm_runtime_get_sync(adapter) < 0) {
			dev_err(data->pdev->dev.parent, "pm_runtime_get_sync failed\n");
			err = -EIO;
			break;
		}
#if 0
		if (synaptics_secure_touch_clk_prepare_enable(data) < 0) {
			pm_runtime_put_sync(adapter);
			err = -EIO;
			break;
		}
#endif
		reinit_completion(&data->st_powerdown);
		reinit_completion(&data->st_irq_processed);
		atomic_set(&data->st_enabled, 1);
		atomic_set(&data->st_pending_irqs,  0);
		break;
	default:
		dev_err(data->pdev->dev.parent,
			"unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
       dev_err(data->pdev->dev.parent, "synaptics_secure_touch_enable_store err=%x\n", err);
	return err;
}

/*
 * This function returns whether there are pending interrupts, or
 * other error conditions that need to be signaled to the userspace library,
 * according tot he following logic:
 * - st_enabled is 0 if secure touch is not enabled, returning -EBADF
 * - st_pending_irqs is -1 to signal that secure touch is in being stopped,
 *   returning -EINVAL
 * - st_pending_irqs is 1 to signal that there is a pending irq, returning
 *   the value "1" to the sysfs read operation
 * - st_pending_irqs is 0 (only remaining case left) if the pending interrupt
 *   has been processed, so the interrupt handler can be allowed to continue.
 */
static ssize_t synaptics_secure_touch_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&data->st_enabled) == 0)
		return -EBADF;

	if (atomic_cmpxchg(&data->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&data->st_pending_irqs, 1, 0) == 1)
		val = 1;
	else
		complete(&data->st_irq_processed);

	return scnprintf(buf, PAGE_SIZE, "%u", val);

}
#endif

static DEVICE_ATTR(panel_color, S_IRUSR, synaptics_rmi4_panel_color_show, NULL);
static DEVICE_ATTR(panel_vendor, S_IRUSR, synaptics_rmi4_panel_vendor_show, NULL);

static struct kobj_attribute virtual_key_map_attr = {
	.attr = {
		.name = VIRTUAL_KEY_MAP_FILE_NAME,
		.mode = S_IRUGO,
	},
	.show = synaptics_rmi4_virtual_key_map_show,
};

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			(rmi4_data->rmi4_mod_info.product_info[0]),
			(rmi4_data->rmi4_mod_info.product_info[1]));
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->firmware_id);
}

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
			device_status.flash_prog);
}

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->button_0d_enabled);
}

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	unsigned char ii;
	unsigned char intr_enable;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	if (rmi4_data->button_0d_enabled == input)
		return count;

	if (list_empty(&rmi->support_fn_list))
		return -ENODEV;

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
			ii = fhandler->intr_reg_num;

			retval = synaptics_rmi4_reg_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;

			if (input == 1)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_reg_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;
		}
	}

	rmi4_data->button_0d_enabled = input;

	return count;
}

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		synaptics_rmi4_suspend(dev);
	else if (input == 0)
		synaptics_rmi4_resume(dev);
	else
		return -EINVAL;

	return count;
}

static ssize_t synaptics_rmi4_wake_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->enable_wakeup_gesture);
}

static ssize_t synaptics_rmi4_wake_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (bdata->cut_off_power) {
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Unable to switch wakeup gesture mode\n", __func__);
		return count;
	}

	input = input > 0 ? 1 : 0;

	if (rmi4_data->f11_wakeup_gesture || rmi4_data->f12_wakeup_gesture)
		rmi4_data->enable_wakeup_gesture = input;

	if (rmi4_data->suspend)
		synaptics_rmi4_wakeup_reconfigure(rmi4_data, (bool)input);

	return count;
}

static ssize_t synaptics_rmi4_irq_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->irq_enabled);
}

static ssize_t synaptics_rmi4_irq_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;
	if (input)
		enable_irq(rmi4_data->irq);
	else
		disable_irq(rmi4_data->irq);

	return count;
}
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
static int synaptics_rmi4_palm_enable(struct synaptics_rmi4_data *rmi4_data, int on)
{
	int retval;
	unsigned char enable;
	unsigned char forceupdate = 0x4;
	unsigned char temp = 0;
	unsigned char input[2];
/*
	if (rmi4_data->palm_enabled == on) {
		dev_info(rmi4_data->pdev->dev.parent,
		"%s don't need update :%d\n", __func__, on);
		return 0;
	} else {
		dev_info(rmi4_data->pdev->dev.parent,
		"%s enable:%d\n", __func__, on);
	}
*/
	enable = on > 0 ? 1 : 0;


	dev_info(rmi4_data->pdev->dev.parent, "%s: on:%d\n", __func__, on);

	retval = synaptics_rmi4_reg_read(rmi4_data, F12_2D_CTRL23,
					&temp, sizeof(temp));
	if (retval < 0)
		return -EINVAL;

	if (enable)
		temp = temp | 0x04;
	else
		temp = temp & (~0x04);

	retval = synaptics_rmi4_reg_write(rmi4_data, F12_2D_CTRL23,
				&temp, sizeof(temp));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL109,
				&enable, sizeof(enable));
	if (retval < 0)
		return -EINVAL;

	if (!enable)
		goto out;

#if 1
	/* area threshold */
	if (rmi4_data->palm_rx_area_threshold) {
		input[1] = (unsigned char)((rmi4_data->palm_rx_area_threshold >> 8) & 0xff);
		input[0] = (unsigned char)(rmi4_data->palm_rx_area_threshold & 0xff);
		retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL110,
						&input[1], sizeof(input[1]));
		if (retval < 0)
			return -EINVAL;

		retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL111,
						&input[0], sizeof(input[0]));
		if (retval < 0)
			return -EINVAL;

	}
	/* channel threshold */
	if (rmi4_data->palm_rx_channel_threshold) {
		temp = (unsigned char)rmi4_data->palm_rx_channel_threshold;
		retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL112,
						&temp, sizeof(temp));
		if (retval < 0)
			return -EINVAL;
	}
	/* tx disable */
	if (rmi4_data->palm_tx_grip_disable) {
		temp = (unsigned char)rmi4_data->palm_tx_grip_disable;
		retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL113,
						&temp, sizeof(temp));
		if (retval < 0)
			return -EINVAL;
	}
#endif
out:
	retval = synaptics_rmi4_reg_write(rmi4_data, F54_FORCE_UPDATE,
				&forceupdate, sizeof(forceupdate));
	if (retval < 0)
		return -EINVAL;
	return 0;
}

static int synaptics_rmi4_palmsensor_enable(int on)
{
	struct synaptics_rmi4_data *rmi4_data = exp_data.rmi4_data;
	int ret = 0;

	if (!rmi4_data)
		return -EINVAL;
	rmi4_data->palm_enabled = on;
	if (rmi4_data->suspend) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: tp has suspend\n", __func__);
		rmi4_data->palm_sensor_changed = false;
		return 0;
	}
	ret = synaptics_rmi4_palm_enable(rmi4_data, on);
	if (!ret)
		rmi4_data->palm_sensor_changed = true;
	return ret;
}

static ssize_t synaptics_rmi4_palm_enable_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
/* We don't need read the register again  */
/*	retval = synaptics_rmi4_reg_read(rmi4_data, F12_2D_CTRL23,
					&enable, sizeof(enable));
	if (retval < 0)
		return EINVAL;

	rmi4_data->palm_enabled = ((enable & 0x04) > 0 ? 1 : 0);
*/
	return snprintf(buf, PAGE_SIZE, "%u\n", rmi4_data->palm_enabled);
}

static ssize_t synaptics_rmi4_palm_enable_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) != 1 || rmi4_data->suspend)
			return -EINVAL;

	synaptics_rmi4_palm_enable(rmi4_data, !!input);

	return count;
}

static ssize_t synaptics_rmi4_palm_rx_area_threshold_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char input[2];

	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data, F51_CUSTOM_CTRL110,
					&input[1], sizeof(input[1]));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_read(rmi4_data, F51_CUSTOM_CTRL111,
					&input[0], sizeof(input[0]));
	if (retval < 0)
		return -EINVAL;
	printk("%s input0:%d, input1:%d\n", __func__, input[0], input[1]);
	return snprintf(buf, PAGE_SIZE, "%d\n", input[1] << 8 | input[0]);
}

static ssize_t synaptics_rmi4_palm_rx_area_threshold_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int retval;
	unsigned char forceupdate = 0x4;
	unsigned char enable[2];
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) != 1 || rmi4_data->suspend)
			return -EINVAL;

	enable[1] = (unsigned char)((input >> 8) & 0xff);
	enable[0] = (unsigned char)(input & 0xff);
	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL110,
					&enable[1], sizeof(enable[1]));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL111,
					&enable[0], sizeof(enable[0]));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F54_FORCE_UPDATE,
				&forceupdate, sizeof(forceupdate));
	if (retval < 0)
		return -EINVAL;
	printk("%s input0:%d, input1:%d\n", __func__, enable[0], enable[1]);

	rmi4_data->palm_rx_area_threshold = input;

	return count;
}

static ssize_t synaptics_rmi4_palm_rx_channel_threshold_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char input;

	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data, F51_CUSTOM_CTRL112,
					&input, sizeof(input));
	if (retval < 0)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%d\n", input);
}

static ssize_t synaptics_rmi4_palm_rx_channel_threshold_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int retval;
	unsigned char forceupdate = 0x4;
	unsigned char enable;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) != 1 || rmi4_data->suspend)
			return -EINVAL;

	enable = (unsigned char)input;
	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL112,
					&enable, sizeof(enable));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F54_FORCE_UPDATE,
				&forceupdate, sizeof(forceupdate));
	if (retval < 0)
		return -EINVAL;

	rmi4_data->palm_rx_channel_threshold = input;

	return count;
}

static ssize_t synaptics_rmi4_palm_tx_grip_disable_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char input;

	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data, F51_CUSTOM_CTRL113,
					&input, sizeof(input));
	if (retval < 0)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%d\n", input);
}

static ssize_t synaptics_rmi4_palm_tx_grip_disable_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int retval;
	unsigned char forceupdate = 0x4;
	unsigned char enable;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) != 1 || rmi4_data->suspend)
			return -EINVAL;

	enable = (unsigned char)input;
	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL113,
					&enable, sizeof(enable));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F54_FORCE_UPDATE,
				&forceupdate, sizeof(forceupdate));
	if (retval < 0)
		return -EINVAL;

	rmi4_data->palm_tx_grip_disable = input;

	return count;
}

static ssize_t synaptics_rmi4_palm_tx_area_threshold_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char input;

	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data, F51_CUSTOM_CTRL114,
					&input, sizeof(input));
	if (retval < 0)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%d\n", input);
}

static ssize_t synaptics_rmi4_palm_tx_area_threshold_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int retval;
	unsigned char forceupdate = 0x4;
	unsigned char enable;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) != 1 || rmi4_data->suspend)
			return -EINVAL;

	enable = (unsigned char)input;
	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL114,
					&enable, sizeof(enable));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F54_FORCE_UPDATE,
				&forceupdate, sizeof(forceupdate));
	if (retval < 0)
		return -EINVAL;

	rmi4_data->palm_tx_area_threshold = input;

	return count;
}

static ssize_t synaptics_rmi4_palm_tx_channel_threshold_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char input;

	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data, F51_CUSTOM_CTRL115,
					&input, sizeof(input));
	if (retval < 0)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%d\n", input);
}

static ssize_t synaptics_rmi4_palm_tx_channel_threshold_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int retval;
	unsigned char forceupdate = 0x4;
	unsigned char enable;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) != 1 || rmi4_data->suspend)
			return -EINVAL;

	enable = (unsigned char)input;
	retval = synaptics_rmi4_reg_write(rmi4_data, F51_CUSTOM_CTRL115,
					&enable, sizeof(enable));
	if (retval < 0)
		return -EINVAL;

	retval = synaptics_rmi4_reg_write(rmi4_data, F54_FORCE_UPDATE,
				&forceupdate, sizeof(forceupdate));
	if (retval < 0)
		return -EINVAL;

	rmi4_data->palm_tx_channel_threshold = input;

	return count;
}
#endif
#endif

static ssize_t synaptics_rmi4_panel_color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%c\n",
			rmi4_data->lockdown_info[2]);
}

static ssize_t synaptics_rmi4_panel_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%c\n",
			rmi4_data->lockdown_info[0]);
}

static ssize_t synaptics_rmi4_virtual_key_map_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ii;
	int cnt;
	int count = 0;

	for (ii = 0; ii < vir_button_map->nbuttons; ii++) {
		cnt = snprintf(buf, PAGE_SIZE - count, "0x01:%d:%d:%d:%d:%d\n",
				vir_button_map->map[ii * 5 + 0],
				vir_button_map->map[ii * 5 + 1],
				vir_button_map->map[ii * 5 + 2],
				vir_button_map->map[ii * 5 + 3],
				vir_button_map->map[ii * 5 + 4]);
		buf += cnt;
		count += cnt;
	}

	return count;
}

#ifdef CONFIG_SYNA_TOUCH_COUNT_DUMP
static ssize_t syna_touch_suspend_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			rmi4_data->suspend);
}

static DEVICE_ATTR(touch_suspend_notify, (S_IRUGO | S_IRGRP), syna_touch_suspend_notify_show, NULL);
#endif

static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char finger_status_reg[3];
	unsigned char detected_gestures;
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
	int wx;
	int wy;
	int temp;
	struct synaptics_rmi4_f11_data_1_5 data;
	struct synaptics_rmi4_f11_extra_data *extra_data;

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;

	extra_data = (struct synaptics_rmi4_f11_extra_data *)fhandler->extra;

	if (rmi4_data->suspend && rmi4_data->wakeup_en) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data38_offset,
				&detected_gestures,
				sizeof(detected_gestures));
		if (retval < 0)
			return 0;

		if (detected_gestures) {
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 1);
			input_sync(rmi4_data->input_dev);
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 0);
			input_sync(rmi4_data->input_dev);
		}

		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0)
		return 0;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
				& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, finger_status);
#endif

		if (finger_status) {
			data_offset = data_addr +
					num_of_finger_status_regs +
					(finger * sizeof(data.data));
			retval = synaptics_rmi4_reg_read(rmi4_data,
					data_offset,
					data.data,
					sizeof(data.data));
			if (retval < 0) {
				touch_count = 0;
				goto exit;
			}

			x = (data.x_position_11_4 << 4) | data.x_position_3_0;
			y = (data.y_position_11_4 << 4) | data.y_position_3_0;
			wx = data.wx;
			wy = data.wy;

			if (rmi4_data->hw_if->board_data->swap_axes) {
				temp = x;
				x = y;
				y = temp;
				temp = wx;
				wx = wy;
				wy = temp;
			}

			if (rmi4_data->hw_if->board_data->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->hw_if->board_data->y_flip)
				y = rmi4_data->sensor_max_y - y;

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d: "
					"status = 0x%02x, "
					"x = %d, "
					"y = %d, "
					"wx = %d, "
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			touch_count++;
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif
	}

	input_sync(rmi4_data->input_dev);

exit:
	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return touch_count;
}

static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char index;
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned char gesture_type;
	unsigned short data_addr;
	int x;
	int y;
	int wx;
	int wy;
	int temp;
#if defined(REPORT_2D_PRESSURE) || defined(F51_DISCRETE_FORCE)
	int pressure;
#endif
	int touchs = 0;
#ifdef REPORT_2D_PRESSURE
	unsigned char f_fingers;
	unsigned char f_lsb;
	unsigned char f_msb;
	unsigned char *f_data;
#endif
#ifdef F51_DISCRETE_FORCE
	unsigned char force_level;
#endif
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;
	struct synaptics_rmi4_f12_finger_data *finger_data;
	static unsigned char finger_presence;
	static unsigned char stylus_presence;
#ifdef F12_DATA_15_WORKAROUND
	static unsigned char objects_already_present;
#endif

	if (rmi4_data->input_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent, "input_dev is NULL, do not report data\n");

		return 0;
	}

	fingers_to_process = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	if (rmi4_data->suspend && rmi4_data->wakeup_en) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data4_offset,
				rmi4_data->gesture_detection,
				sizeof(rmi4_data->gesture_detection));
		if (retval < 0)
			return 0;

		gesture_type = rmi4_data->gesture_detection[0];
		if (gesture_type && gesture_type != F12_UDG_DETECT) {
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 1);
			input_sync(rmi4_data->input_dev);
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 0);
			input_sync(rmi4_data->input_dev);
			dev_err(rmi4_data->pdev->dev.parent, "double click send input event\n");
		}

		return 0;
	}

	/* Determine the total number of fingers to process */
	if (extra_data->data15_size) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data15_offset,
				extra_data->data15_data,
				extra_data->data15_size);
		if (retval < 0)
			return 0;

		/* Start checking from the highest bit */
		index = extra_data->data15_size - 1; /* Highest byte */
		finger = (fingers_to_process - 1) % 8; /* Highest bit */
		do {
			if (extra_data->data15_data[index] & (1 << finger))
				break;

			if (finger) {
				finger--;
			} else if (index > 0) {
				index--; /* Move to the next lower byte */
				finger = 7;
			}

			fingers_to_process--;
		} while (fingers_to_process);

		dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of fingers to process = %d\n",
			__func__, fingers_to_process);
	}

#ifdef F12_DATA_15_WORKAROUND
	fingers_to_process = max(fingers_to_process, objects_already_present);
#endif

	if (!fingers_to_process) {
		synaptics_rmi4_free_fingers(rmi4_data);
		finger_presence = 0;
		stylus_presence = 0;
		rmi4_data->touchs = 0;
		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr + extra_data->data1_offset,
			(unsigned char *)fhandler->data,
			fingers_to_process * size_of_2d_data);
	if (retval < 0)
		return 0;

	data = (struct synaptics_rmi4_f12_finger_data *)fhandler->data;

#ifdef REPORT_2D_PRESSURE
	if (rmi4_data->report_pressure) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data29_offset,
				extra_data->data29_data,
				extra_data->data29_size);
		if (retval < 0)
			return 0;
	}
#endif

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (finger = 0; finger < fingers_to_process; finger++) {
		finger_data = data + finger;
		finger_status = finger_data->object_type_and_status;

#ifdef F12_DATA_15_WORKAROUND
		objects_already_present = finger + 1;
#endif

		x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
		y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
#ifdef REPORT_2D_W
		wx = finger_data->wx;
		wy = finger_data->wy;
#endif

		if (rmi4_data->hw_if->board_data->swap_axes) {
			temp = x;
			x = y;
			y = temp;
			temp = wx;
			wx = wy;
			wy = temp;
		}

		if (rmi4_data->hw_if->board_data->x_flip)
			x = rmi4_data->sensor_max_x - x;
		if (rmi4_data->hw_if->board_data->y_flip)
			y = rmi4_data->sensor_max_y - y;

		switch (finger_status) {
		case F12_FINGER_STATUS:
		case F12_GLOVED_FINGER_STATUS:
			if (stylus_presence) /* Stylus has priority over fingers */
				break;
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 1);
#endif

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			if (rmi4_data->wedge_sensor) {
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MAJOR, wx);
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MINOR, wx);
			} else {
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MAJOR,
						max(wx, wy));
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MINOR,
						min(wx, wy));
			}
#endif
#ifdef REPORT_2D_PRESSURE
			if (rmi4_data->report_pressure) {
				f_fingers = extra_data->data29_size / 2;
				f_data = extra_data->data29_data;
				if (finger + 1 > f_fingers) {
					pressure = 1;
				} else {
					f_lsb = finger * 2;
					f_msb = finger * 2 + 1;
					pressure = (int)f_data[f_lsb] << 0 |
							(int)f_data[f_msb] << 8;
				}
				pressure = pressure > 0 ? pressure : 1;
				if (pressure > rmi4_data->force_max)
					pressure = rmi4_data->force_max;
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_PRESSURE, pressure);
			}
#elif defined(F51_DISCRETE_FORCE)
			if (finger == 0) {
				retval = synaptics_rmi4_reg_read(rmi4_data,
						FORCE_LEVEL_ADDR,
						&force_level,
						sizeof(force_level));
				if (retval < 0)
					return 0;
				pressure = force_level > 0 ? force_level : 1;
			} else {
				pressure = 1;
			}
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_PRESSURE, pressure);
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d: "
					"status = 0x%02x, "
					"x = %d, "
					"y = %d, "
					"wx = %d, "
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			finger_presence = 1;
			touch_count++;
			touchs |= BIT(finger);
			rmi4_data->touchs |= BIT(finger);
			break;
		case F12_PALM_STATUS:
			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: palm event,palm_enabled:%u",
					__func__, rmi4_data->palm_enabled);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
			if (rmi4_data->palm_enabled)
				update_palm_sensor_value(1);
#endif
			break;
		case F12_STYLUS_STATUS:
		case F12_ERASER_STATUS:
			if (finger_presence) { /* Stylus has priority over fingers */
				mutex_unlock(&(rmi4_data->rmi4_report_mutex));
				synaptics_rmi4_free_fingers(rmi4_data);
				mutex_lock(&(rmi4_data->rmi4_report_mutex));
				finger_presence = 0;
			}
			if (stylus_presence) {/* Allow one stylus at a timee */
				if (finger + 1 != stylus_presence)
					break;
			}
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOUCH, 1);
			if (finger_status == F12_STYLUS_STATUS) {
				input_report_key(rmi4_data->stylus_dev,
						BTN_TOOL_PEN, 1);
			} else {
				input_report_key(rmi4_data->stylus_dev,
						BTN_TOOL_RUBBER, 1);
			}
			input_report_abs(rmi4_data->stylus_dev,
					ABS_X, x);
			input_report_abs(rmi4_data->stylus_dev,
					ABS_Y, y);

			stylus_presence = finger + 1;
			touch_count++;
			break;
		default:
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
#endif
			rmi4_data->touchs &= ~BIT(finger);
			touchs &= ~BIT(finger);
			break;
		}
	}

	for (finger = 0; finger < fhandler->num_of_data_points; finger++) {
		if (BIT(finger) & (rmi4_data->touchs ^ touchs)) {
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
		}
	}

	if (touch_count == 0) {
		finger_presence = 0;
#ifdef F12_DATA_15_WORKAROUND
		objects_already_present = 0;
#endif
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif

		if (rmi4_data->stylus_enable) {
			stylus_presence = 0;
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOUCH, 0);
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOOL_PEN, 0);
			if (rmi4_data->eraser_enable) {
				input_report_key(rmi4_data->stylus_dev,
						BTN_TOOL_RUBBER, 0);
			}
		}

		rmi4_data->touchs = 0;
	}

	input_sync(rmi4_data->input_dev);

	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return touch_count;
}

static void synaptics_rmi4_f1a_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0;
	unsigned char button;
	unsigned char index;
	unsigned char shift;
	unsigned char status;
	unsigned char *data;
	unsigned short data_addr = fhandler->full_addr.data_base;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	static unsigned char do_once = 1;
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif

	if (do_once) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		do_once = 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read button data registers\n",
				__func__);
		return;
	}

	data = f1a->button_data_buffer;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status) {
			if (!rmi4_data->suspend)
				continue;
		} else
			current_status[button] = status;

		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Button %d (code %d) ->%d\n",
				__func__, button,
				f1a->button_map[button],
				status);
#ifdef NO_0D_WHILE_2D
		if (rmi4_data->fingers_on_2d == false) {
			if (status == 1) {
				before_2d_status[button] = 1;
			} else {
				if (while_2d_status[button] == 1) {
					while_2d_status[button] = 0;
					continue;
				} else {
					before_2d_status[button] = 0;
				}
			}

			touch_count++;
			input_report_key(rmi4_data->input_dev,
					f1a->button_map[button],
					status);
		} else {
			if (before_2d_status[button] == 1) {
				before_2d_status[button] = 0;
				touch_count++;
				input_report_key(rmi4_data->input_dev,
						f1a->button_map[button],
						status);
			} else {
				if (status == 1)
					while_2d_status[button] = 1;
				else
					while_2d_status[button] = 0;
			}
		}
#else
		touch_count++;
		input_report_key(rmi4_data->input_dev,
				f1a->button_map[button],
				status);
#endif
	}

	if (touch_count)
		input_sync(rmi4_data->input_dev);

	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return;
}

static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	unsigned char touch_count_2d;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x reporting\n",
			__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
	case SYNAPTICS_RMI4_F11:
		touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
				fhandler);

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F12:
		touch_count_2d = synaptics_rmi4_f12_abs_report(rmi4_data,
				fhandler);

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F1A:
		synaptics_rmi4_f1a_report(rmi4_data, fhandler);
		break;
	default:
		break;
	}

	return;
}

static void synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data,
		bool report)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	unsigned char *intr = &data[1];
	bool was_in_bl_mode;
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			data,
			rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read interrupt status\n",
				__func__);
		return;
	}

	status.data[0] = data[0];
	if (status.status_code == STATUS_CRC_IN_PROGRESS) {
		retval = synaptics_rmi4_check_status(rmi4_data,
				&was_in_bl_mode);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to check status\n",
					__func__);
			return;
		}
		retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read device status\n",
					__func__);
			return;
		}
	}
	if (status.unconfigured && !status.flash_prog) {
		pr_notice("%s: spontaneous reset detected\n", __func__);
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to reinit device\n",
					__func__);
		}
	}

	if (!report)
		return;

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask &
						intr[fhandler->intr_reg_num]) {
					synaptics_rmi4_report_touch(rmi4_data,
							fhandler);
				}
			}
		}
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if (!exp_fhandler->insert &&
					!exp_fhandler->remove &&
					(exp_fhandler->exp_fn->attn != NULL))
				exp_fhandler->exp_fn->attn(rmi4_data, intr[0]);
		}
	}
	mutex_unlock(&exp_data.mutex);

	return;
}

static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (gpio_get_value(bdata->irq_gpio) != bdata->irq_on_state)
		goto exit;

	if (IRQ_HANDLED == synaptics_filter_interrupt(data))
		return IRQ_HANDLED;

	synaptics_rmi4_sensor_report(rmi4_data, true);

exit:
	return IRQ_HANDLED;
}

static int synaptics_rmi4_int_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char ii;
	unsigned char zero = 0x00;
	unsigned char *intr_mask;
	unsigned short intr_addr;

	intr_mask = rmi4_data->intr_mask;

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (intr_mask[ii] != 0x00) {
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			if (enable) {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&(intr_mask[ii]),
						sizeof(intr_mask[ii]));
				if (retval < 0)
					return retval;
			} else {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&zero,
						sizeof(zero));
				if (retval < 0)
					return retval;
			}
		}
	}

	return retval;
}

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable, bool attn_only)
{
	int retval = 0;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (attn_only) {
		retval = synaptics_rmi4_int_enable(rmi4_data, enable);
		return retval;
	}

	if (enable) {
		if (rmi4_data->irq_enabled)
			return retval;

		retval = synaptics_rmi4_int_enable(rmi4_data, false);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to disable synaptics int\n",
					__func__);
			return retval;
		}

		/* Process and clear interrupts */
		synaptics_rmi4_sensor_report(rmi4_data, false);

		retval = request_threaded_irq(rmi4_data->irq, NULL,
				synaptics_rmi4_irq, bdata->irq_flags,
				PLATFORM_DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		retval = synaptics_rmi4_int_enable(rmi4_data, true);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable synaptics int\n",
					__func__);
			return retval;
		}

		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;
		}
	}

	return retval;
}

static void synaptics_rmi4_set_intr_mask(struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	unsigned char ii;
	unsigned char intr_offset;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < (fd->intr_src_count + intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	return;
}

static int synaptics_rmi4_query_product_id(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short lockdown_addr;

	/* Product ID addr starts from F01_RMI_QUERY11 */
	lockdown_addr = rmi4_data->f01_query_base_addr + F01_PROD_ID_OFFSET;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			lockdown_addr,
			rmi4_data->lockdown_info,
			LOCKDOWN_INFO_SIZE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
			"Failed reading reg %d\n",
			lockdown_addr);
		return retval;
	}

	dev_info(rmi4_data->pdev->dev.parent,
			"Lockdown info: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			rmi4_data->lockdown_info[0], rmi4_data->lockdown_info[1],
			rmi4_data->lockdown_info[2], rmi4_data->lockdown_info[3],
			rmi4_data->lockdown_info[4], rmi4_data->lockdown_info[5],
			rmi4_data->lockdown_info[6], rmi4_data->lockdown_info[7]);

	return 0;
}

static int synaptics_rmi4_query_chip_id(struct synaptics_rmi4_data *rmi4_data)
{
	int retval, i;
	unsigned short chipdata_addr;
	unsigned char query_data[8] = {0};

	chipdata_addr = rmi4_data->f01_query_base_addr + F01_PROD_ID_OFFSET;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			chipdata_addr,
			query_data,
			8);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
			"Failed reading reg %d\n",
			chipdata_addr);
		return retval;
	}

	dev_info(rmi4_data->pdev->dev.parent,
			"chip info: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			query_data[0], query_data[1],
			query_data[2], query_data[3],
			query_data[4], query_data[5],
			query_data[6], query_data[7]);

	for (i = 0; i < rmi4_data->hw_if->board_data->config_array_size; i++) {
		if (!memcmp(rmi4_data->hw_if->board_data->config_array[i].chip_id_name,
				query_data,
				strlen(rmi4_data->hw_if->board_data->config_array[i].chip_id_name))) {
			rmi4_data->chip_id = rmi4_data->hw_if->board_data->config_array[i].chip_id;
			rmi4_data->chip_is_tddi = rmi4_data->hw_if->board_data->config_array[i].chip_is_tddi;
			rmi4_data->open_test_b7 = rmi4_data->hw_if->board_data->config_array[i].open_test_b7;
			rmi4_data->short_test_extend = rmi4_data->hw_if->board_data->config_array[i].short_test_extend;
			rmi4_data->factory_param = rmi4_data->hw_if->board_data->config_array[i].factory_param;
			rmi4_data->panel_power_seq = rmi4_data->hw_if->board_data->config_array[i].panel_power_seq;
			if (rmi4_data->chip_is_tddi)
				rmi4_data->hw_if->board_data->reset_gpio = -1;
			break;
		}
	}

	if (i >= rmi4_data->hw_if->board_data->config_array_size) {
		dev_err(rmi4_data->pdev->dev.parent, "failed to match the chip id\n");
		rmi4_data->chip_id = -1; /* Set chip_id -1 to ensure it won't do firmware upgrading */
		return -EINVAL;
	}

	return 0;
}

static int synaptics_rmi4_f01_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->data = NULL;
	fhandler->extra = NULL;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	rmi4_data->f01_query_base_addr = fd->query_base_addr;
	rmi4_data->f01_ctrl_base_addr = fd->ctrl_base_addr;
	rmi4_data->f01_data_base_addr = fd->data_base_addr;
	rmi4_data->f01_cmd_base_addr = fd->cmd_base_addr;

	if (rmi4_data->hw_if->board_data->lockdown_area == LOCKDOWN_AREA_PRODUCT_ID)
		synaptics_rmi4_query_product_id(rmi4_data);

	return 0;
}

static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	int temp;
	unsigned char offset;
	unsigned char fingers_supported;
	struct synaptics_rmi4_f11_extra_data *extra_data;
	struct synaptics_rmi4_f11_query_0_5 query_0_5;
	struct synaptics_rmi4_f11_query_7_8 query_7_8;
	struct synaptics_rmi4_f11_query_9 query_9;
	struct synaptics_rmi4_f11_query_12 query_12;
	struct synaptics_rmi4_f11_query_27 query_27;
	struct synaptics_rmi4_f11_ctrl_6_9 control_6_9;
	const struct synaptics_dsx_board_data *bdata =
				rmi4_data->hw_if->board_data;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for fhandler->extra\n",
				__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f11_extra_data *)fhandler->extra;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base,
			query_0_5.data,
			sizeof(query_0_5.data));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	if (query_0_5.num_of_fingers <= 4)
		fhandler->num_of_data_points = query_0_5.num_of_fingers + 1;
	else if (query_0_5.num_of_fingers == 5)
		fhandler->num_of_data_points = 10;

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + 6,
			control_6_9.data,
			sizeof(control_6_9.data));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x = control_6_9.sensor_max_x_pos_7_0 |
			(control_6_9.sensor_max_x_pos_11_8 << 8);
	rmi4_data->sensor_max_y = control_6_9.sensor_max_y_pos_7_0 |
			(control_6_9.sensor_max_y_pos_11_8 << 8);
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->max_touch_width = MAX_F11_TOUCH_WIDTH;

	if (bdata->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	fhandler->data = NULL;

	offset = sizeof(query_0_5.data);

	/* query 6 */
	if (query_0_5.has_rel)
		offset += 1;

	/* queries 7 8 */
	if (query_0_5.has_gestures) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_7_8.data,
				sizeof(query_7_8.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_7_8.data);
	}

	/* query 9 */
	if (query_0_5.has_query_9) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_9.data,
				sizeof(query_9.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_9.data);
	}

	/* query 10 */
	if (query_0_5.has_gestures && query_7_8.has_touch_shapes)
		offset += 1;

	/* query 11 */
	if (query_0_5.has_query_11)
		offset += 1;

	/* query 12 */
	if (query_0_5.has_query_12) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_12.data,
				sizeof(query_12.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_12.data);
	}

	/* query 13 */
	if (query_0_5.has_jitter_filter)
		offset += 1;

	/* query 14 */
	if (query_0_5.has_query_12 && query_12.has_general_information_2)
		offset += 1;

	/* queries 15 16 17 18 19 20 21 22 23 24 25 26*/
	if (query_0_5.has_query_12 && query_12.has_physical_properties)
		offset += 12;

	/* query 27 */
	if (query_0_5.has_query_27) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_27.data,
				sizeof(query_27.data));
		if (retval < 0)
			return retval;

		rmi4_data->f11_wakeup_gesture = query_27.has_wakeup_gesture;
	}

	if (!rmi4_data->f11_wakeup_gesture)
		return retval;

	/* data 0 */
	fingers_supported = fhandler->num_of_data_points;
	offset = (fingers_supported + 3) / 4;

	/* data 1 2 3 4 5 */
	offset += 5 * fingers_supported;

	/* data 6 7 */
	if (query_0_5.has_rel)
		offset += 2 * fingers_supported;

	/* data 8 */
	if (query_0_5.has_gestures && query_7_8.data[0])
		offset += 1;

	/* data 9 */
	if (query_0_5.has_gestures && (query_7_8.data[0] || query_7_8.data[1]))
		offset += 1;

	/* data 10 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_pinch || query_7_8.has_flick))
		offset += 1;

	/* data 11 12 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_flick || query_7_8.has_rotate))
		offset += 2;

	/* data 13 */
	if (query_0_5.has_gestures && query_7_8.has_touch_shapes)
		offset += (fingers_supported + 3) / 4;

	/* data 14 15 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_scroll_zones ||
			query_7_8.has_multi_finger_scroll ||
			query_7_8.has_chiral_scroll))
		offset += 2;

	/* data 16 17 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_scroll_zones &&
			query_7_8.individual_scroll_zones))
		offset += 2;

	/* data 18 19 20 21 22 23 24 25 26 27 */
	if (query_0_5.has_query_9 && query_9.has_contact_geometry)
		offset += 10 * fingers_supported;

	/* data 28 */
	if (query_0_5.has_bending_correction ||
			query_0_5.has_large_object_suppression)
		offset += 1;

	/* data 29 30 31 */
	if (query_0_5.has_query_9 && query_9.has_pen_hover_discrimination)
		offset += 3;

	/* data 32 */
	if (query_0_5.has_query_12 &&
			query_12.has_small_object_detection_tuning)
		offset += 1;

	/* data 33 34 */
	if (query_0_5.has_query_27 && query_27.f11_query27_b0)
		offset += 2;

	/* data 35 */
	if (query_0_5.has_query_12 && query_12.has_8bit_w)
		offset += fingers_supported;

	/* data 36 */
	if (query_0_5.has_bending_correction)
		offset += 1;

	/* data 37 */
	if (query_0_5.has_query_27 && query_27.has_data_37)
		offset += 1;

	/* data 38 */
	if (query_0_5.has_query_27 && query_27.has_wakeup_gesture)
		extra_data->data38_offset = offset;

	return retval;
}

static int synaptics_rmi4_f12_set_enables(struct synaptics_rmi4_data *rmi4_data,
		unsigned short ctrl28)
{
	int retval;
	static unsigned short ctrl_28_address;

	if (ctrl28)
		ctrl_28_address = ctrl28;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			ctrl_28_address,
			&rmi4_data->report_enable,
			sizeof(rmi4_data->report_enable));
	if (retval < 0)
		return retval;

	return retval;
}

static int synaptics_rmi4_f12_find_sub(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		unsigned char *presence, unsigned char presence_size,
		unsigned char structure_offset, unsigned char reg,
		unsigned char sub)
{
	int retval;
	unsigned char cnt;
	unsigned char regnum;
	unsigned char bitnum;
	unsigned char p_index;
	unsigned char s_index;
	unsigned char offset;
	unsigned char max_reg;
	unsigned char *structure;

	max_reg = (presence_size - 1) * 8 - 1;

	if (reg > max_reg) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Register number (%d) over limit\n",
				__func__, reg);
		return -EINVAL;
	}

	p_index = reg / 8 + 1;
	bitnum = reg % 8;
	if ((presence[p_index] & (1 << bitnum)) == 0x00) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Register %d is not present\n",
				__func__, reg);
		return -EINVAL;
	}

	structure = kmalloc(presence[0], GFP_KERNEL);
	if (!structure) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for structure register\n",
				__func__);
		return -ENOMEM;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + structure_offset,
			structure,
			presence[0]);
	if (retval < 0)
		goto exit;

	s_index = 0;

	for (regnum = 0; regnum < reg; regnum++) {
		p_index = regnum / 8 + 1;
		bitnum = regnum % 8;
		if ((presence[p_index] & (1 << bitnum)) == 0x00)
			continue;

		if (structure[s_index] == 0x00)
			s_index += 3;
		else
			s_index++;

		while (structure[s_index] & ~MASK_7BIT)
			s_index++;

		s_index++;
	}

	cnt = 0;
	s_index++;
	offset = sub / 7;
	bitnum = sub % 7;

	do {
		if (cnt == offset) {
			if (structure[s_index + cnt] & (1 << bitnum))
				retval = 1;
			else
				retval = 0;
			goto exit;
		}
		cnt++;
	} while (structure[s_index + cnt - 1] & ~MASK_7BIT);

	retval = 0;

exit:
	kfree(structure);

	return retval;
}

static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval = 0;
	int temp;
	unsigned char subpacket;
	unsigned char ctrl_23_size;
	unsigned char size_of_2d_data;
	unsigned char size_of_query5;
	unsigned char size_of_query8;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_20_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_26_offset;
	unsigned char ctrl_27_offset;
	unsigned char ctrl_28_offset;
	unsigned char ctrl_31_offset;
	unsigned char ctrl_47_offset;
	unsigned char ctrl_58_offset;
	unsigned char num_of_fingers;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_query_5 *query_5 = NULL;
	struct synaptics_rmi4_f12_query_8 *query_8 = NULL;
	struct synaptics_rmi4_f12_ctrl_8 *ctrl_8 = NULL;
	struct synaptics_rmi4_f12_ctrl_23 *ctrl_23 = NULL;
	struct synaptics_rmi4_f12_ctrl_31 *ctrl_31 = NULL;
	struct synaptics_rmi4_f12_ctrl_58 *ctrl_58 = NULL;
	const struct synaptics_dsx_board_data *bdata =
				rmi4_data->hw_if->board_data;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for fhandler->extra\n",
				__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	query_5 = kzalloc(sizeof(*query_5), GFP_KERNEL);
	if (!query_5) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for query_5\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	query_8 = kzalloc(sizeof(*query_8), GFP_KERNEL);
	if (!query_8) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for query_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	ctrl_8 = kzalloc(sizeof(*ctrl_8), GFP_KERNEL);
	if (!ctrl_8) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for ctrl_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	ctrl_23 = kzalloc(sizeof(*ctrl_23), GFP_KERNEL);
	if (!ctrl_23) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for ctrl_23\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	ctrl_31 = kzalloc(sizeof(*ctrl_31), GFP_KERNEL);
	if (!ctrl_31) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for ctrl_31\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	ctrl_58 = kzalloc(sizeof(*ctrl_58), GFP_KERNEL);
	if (!ctrl_58) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for ctrl_58\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 4,
			&size_of_query5,
			sizeof(size_of_query5));
	if (retval < 0)
		goto exit;

	pr_err("%s %d: fhandler->full_addr.query_base = 0x%04x\n", __func__, __LINE__, fhandler->full_addr.query_base);
	pr_err("%s %d: fhandler->full_addr.ctrl_base = 0x%04x\n", __func__, __LINE__, fhandler->full_addr.ctrl_base);
	pr_err("%s %d: size_of_query5 = %d\n", __func__, __LINE__, size_of_query5);

	if (size_of_query5 > sizeof(query_5->data))
		size_of_query5 = sizeof(query_5->data);
	memset(query_5->data, 0x00, sizeof(query_5->data));

	pr_err("%s %d: size_of_query5 = %d\n", __func__, __LINE__, size_of_query5);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 5,
			query_5->data,
			size_of_query5);
	if (retval < 0)
		goto exit;

	ctrl_8_offset = query_5->ctrl0_is_present +
			query_5->ctrl1_is_present +
			query_5->ctrl2_is_present +
			query_5->ctrl3_is_present +
			query_5->ctrl4_is_present +
			query_5->ctrl5_is_present +
			query_5->ctrl6_is_present +
			query_5->ctrl7_is_present;

	ctrl_20_offset = ctrl_8_offset +
			query_5->ctrl8_is_present +
			query_5->ctrl9_is_present +
			query_5->ctrl10_is_present +
			query_5->ctrl11_is_present +
			query_5->ctrl12_is_present +
			query_5->ctrl13_is_present +
			query_5->ctrl14_is_present +
			query_5->ctrl15_is_present +
			query_5->ctrl16_is_present +
			query_5->ctrl17_is_present +
			query_5->ctrl18_is_present +
			query_5->ctrl19_is_present;

	ctrl_23_offset = ctrl_20_offset +
			query_5->ctrl20_is_present +
			query_5->ctrl21_is_present +
			query_5->ctrl22_is_present;


	ctrl_26_offset = ctrl_23_offset +
			query_5->ctrl23_is_present +
			query_5->ctrl24_is_present +
			query_5->ctrl25_is_present;

	ctrl_27_offset = ctrl_26_offset +
			query_5->ctrl26_is_present;

	ctrl_28_offset = ctrl_27_offset +
			query_5->ctrl27_is_present;

	ctrl_31_offset = ctrl_28_offset +
			query_5->ctrl28_is_present +
			query_5->ctrl29_is_present +
			query_5->ctrl30_is_present;

	ctrl_47_offset = ctrl_31_offset +
			query_5->ctrl31_is_present +
			query_5->ctrl32_is_present +
			query_5->ctrl33_is_present +
			query_5->ctrl34_is_present +
			query_5->ctrl35_is_present +
			query_5->ctrl36_is_present +
			query_5->ctrl37_is_present +
			query_5->ctrl38_is_present +
			query_5->ctrl39_is_present +
			query_5->ctrl40_is_present +
			query_5->ctrl41_is_present +
			query_5->ctrl42_is_present +
			query_5->ctrl43_is_present +
			query_5->ctrl44_is_present +
			query_5->ctrl45_is_present +
			query_5->ctrl46_is_present;
	ctrl_58_offset = ctrl_47_offset +
			query_5->ctrl47_is_present +
			query_5->ctrl48_is_present +
			query_5->ctrl49_is_present +
			query_5->ctrl50_is_present +
			query_5->ctrl51_is_present +
			query_5->ctrl52_is_present +
			query_5->ctrl53_is_present +
			query_5->ctrl54_is_present +
			query_5->ctrl55_is_present +
			query_5->ctrl56_is_present +
			query_5->ctrl57_is_present;

	ctrl_23_size = 2;
	for (subpacket = 2; subpacket <= 4; subpacket++) {
		retval = synaptics_rmi4_f12_find_sub(rmi4_data,
				fhandler, query_5->data, sizeof(query_5->data),
				6, 23, subpacket);
		if (retval == 1)
			ctrl_23_size++;
		else if (retval < 0)
			goto exit;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_23_offset,
			ctrl_23->data,
			ctrl_23_size);
	if (retval < 0)
		goto exit;

	/* Maximum number of fingers supported */
	fhandler->num_of_data_points = min_t(unsigned char,
			ctrl_23->max_reported_objects,
			(unsigned char)F12_FINGERS_TO_SUPPORT);

	num_of_fingers = fhandler->num_of_data_points;
	rmi4_data->num_of_fingers = num_of_fingers;

	rmi4_data->stylus_enable = ctrl_23->stylus_enable;
	rmi4_data->eraser_enable = ctrl_23->eraser_enable;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 7,
			&size_of_query8,
			sizeof(size_of_query8));
	if (retval < 0)
		goto exit;

	if (size_of_query8 > sizeof(query_8->data))
		size_of_query8 = sizeof(query_8->data);
	memset(query_8->data, 0x00, sizeof(query_8->data));

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 8,
			query_8->data,
			size_of_query8);
	if (retval < 0)
		goto exit;

	/* Determine the presence of the Data0 register */
	extra_data->data1_offset = query_8->data0_is_present;

	if ((size_of_query8 >= 3) && (query_8->data15_is_present)) {
		extra_data->data15_offset = query_8->data0_is_present +
				query_8->data1_is_present +
				query_8->data2_is_present +
				query_8->data3_is_present +
				query_8->data4_is_present +
				query_8->data5_is_present +
				query_8->data6_is_present +
				query_8->data7_is_present +
				query_8->data8_is_present +
				query_8->data9_is_present +
				query_8->data10_is_present +
				query_8->data11_is_present +
				query_8->data12_is_present +
				query_8->data13_is_present +
				query_8->data14_is_present;
		extra_data->data15_size = (num_of_fingers + 7) / 8;
	} else {
		extra_data->data15_size = 0;
	}

#ifdef REPORT_2D_PRESSURE
	if ((size_of_query8 >= 5) && (query_8->data29_is_present)) {
		extra_data->data29_offset = query_8->data0_is_present +
				query_8->data1_is_present +
				query_8->data2_is_present +
				query_8->data3_is_present +
				query_8->data4_is_present +
				query_8->data5_is_present +
				query_8->data6_is_present +
				query_8->data7_is_present +
				query_8->data8_is_present +
				query_8->data9_is_present +
				query_8->data10_is_present +
				query_8->data11_is_present +
				query_8->data12_is_present +
				query_8->data13_is_present +
				query_8->data14_is_present +
				query_8->data15_is_present +
				query_8->data16_is_present +
				query_8->data17_is_present +
				query_8->data18_is_present +
				query_8->data19_is_present +
				query_8->data20_is_present +
				query_8->data21_is_present +
				query_8->data22_is_present +
				query_8->data23_is_present +
				query_8->data24_is_present +
				query_8->data25_is_present +
				query_8->data26_is_present +
				query_8->data27_is_present +
				query_8->data28_is_present;
		extra_data->data29_size = 0;
		for (subpacket = 0; subpacket <= num_of_fingers; subpacket++) {
			retval = synaptics_rmi4_f12_find_sub(rmi4_data,
					fhandler, query_8->data,
					sizeof(query_8->data),
					9, 29, subpacket);
			if (retval == 1)
				extra_data->data29_size += 2;
			else if (retval < 0)
				goto exit;
		}
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + ctrl_58_offset,
				ctrl_58->data,
				sizeof(ctrl_58->data));
		if (retval < 0)
			goto exit;

		pr_err("%s %d: extra_data->data29_offset = %d\n", __func__, __LINE__, extra_data->data29_offset);
		pr_err("%s %d: extra_data->data29_size = %d\n", __func__, __LINE__, extra_data->data29_size);
		pr_err("%s %d: ctrl_58_offset = %d\n", __func__, __LINE__, ctrl_58_offset);

		rmi4_data->force_min =
				(int)(ctrl_58->min_force_lsb << 0) |
				(int)(ctrl_58->min_force_msb << 8);
		rmi4_data->force_max =
				(int)(ctrl_58->max_force_lsb << 0) |
				(int)(ctrl_58->max_force_msb << 8);

		pr_err("%s %d: rmi4_data->force_min = %d\n", __func__, __LINE__, rmi4_data->force_min);
		pr_err("%s %d: rmi4_data->force_max = %d\n", __func__, __LINE__, rmi4_data->force_max);

		rmi4_data->report_pressure = true;
	} else {
		extra_data->data29_size = 0;
		rmi4_data->report_pressure = false;
	}
#endif

	rmi4_data->report_enable = RPT_DEFAULT;
#ifdef REPORT_2D_Z
	rmi4_data->report_enable |= RPT_Z;
#endif
#ifdef REPORT_2D_W
	rmi4_data->report_enable |= (RPT_WX | RPT_WY);
#endif

	retval = synaptics_rmi4_f12_set_enables(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_28_offset);
	if (retval < 0)
		goto exit;

	if (query_5->ctrl8_is_present) {
		rmi4_data->wedge_sensor = false;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + ctrl_8_offset,
				ctrl_8->data,
				sizeof(ctrl_8->data));
		if (retval < 0)
			goto exit;

		/* Maximum x and y */
		rmi4_data->sensor_max_x =
				((unsigned int)ctrl_8->max_x_coord_lsb << 0) |
				((unsigned int)ctrl_8->max_x_coord_msb << 8);
		rmi4_data->sensor_max_y =
				((unsigned int)ctrl_8->max_y_coord_lsb << 0) |
				((unsigned int)ctrl_8->max_y_coord_msb << 8);

		rmi4_data->max_touch_width = MAX_F12_TOUCH_WIDTH;
	} else {
		rmi4_data->wedge_sensor = true;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + ctrl_31_offset,
				ctrl_31->data,
				sizeof(ctrl_31->data));
		if (retval < 0)
			goto exit;

		/* Maximum x and y */
		rmi4_data->sensor_max_x =
				((unsigned int)ctrl_31->max_x_coord_lsb << 0) |
				((unsigned int)ctrl_31->max_x_coord_msb << 8);
		rmi4_data->sensor_max_y =
				((unsigned int)ctrl_31->max_y_coord_lsb << 0) |
				((unsigned int)ctrl_31->max_y_coord_msb << 8);

		rmi4_data->max_touch_width = MAX_F12_TOUCH_WIDTH;
	}

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	if (bdata->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	rmi4_data->f12_wakeup_gesture = query_5->ctrl27_is_present;
	if (rmi4_data->f12_wakeup_gesture) {
		extra_data->ctrl20_offset = ctrl_20_offset;
		extra_data->data4_offset = query_8->data0_is_present +
				query_8->data1_is_present +
				query_8->data2_is_present +
				query_8->data3_is_present;
	}

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	extra_data->ctrl26_offset = ctrl_26_offset;
	extra_data->ctrl27_offset = ctrl_27_offset;

	/* Allocate memory for finger data storage space */
	fhandler->data_size = num_of_fingers * size_of_2d_data;
	fhandler->data = kmalloc(fhandler->data_size, GFP_KERNEL);
	if (!fhandler->data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for fhandler->data\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

exit:
	kfree(query_5);
	kfree(query_8);
	kfree(ctrl_8);
	kfree(ctrl_23);
	kfree(ctrl_31);
	kfree(ctrl_58);

	return retval;
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f1a_handle *f1a;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for function handle\n",
				__func__);
		return -ENOMEM;
	}

	fhandler->data = (void *)f1a;
	fhandler->extra = NULL;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base,
			f1a->button_query.data,
			sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read query registers\n",
				__func__);
		return retval;
	}

	f1a->max_count = f1a->button_query.max_button_count + 1;

	f1a->button_control.txrx_map = kzalloc(f1a->max_count * 2, GFP_KERNEL);
	if (!f1a->button_control.txrx_map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for tx rx mapping\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_bitmask_size = (f1a->max_count + 7) / 8;

	f1a->button_data_buffer = kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for data buffer\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_map = kcalloc(f1a->max_count,
			sizeof(*(f1a->button_map)), GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for button map\n",
				__func__);
		return -ENOMEM;
	}

	return 0;
}

static int synaptics_rmi4_f1a_button_map(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char ii;
	unsigned char offset = 0;
	struct synaptics_rmi4_f1a_query_4 query_4;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	offset = f1a->button_query.has_general_control +
			f1a->button_query.has_interrupt_enable +
			f1a->button_query.has_multibutton_select;

	if (f1a->button_query.has_tx_rx_map) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + offset,
				f1a->button_control.txrx_map,
				f1a->max_count * 2);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read tx rx mapping\n",
					__func__);
			return retval;
		}

		rmi4_data->button_txrx_mapping = f1a->button_control.txrx_map;
	}

	if (f1a->button_query.has_query4) {
		offset = 2 + f1a->button_query.has_query2 +
				f1a->button_query.has_query3;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_4.data,
				sizeof(query_4.data));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read button features 4\n",
					__func__);
			return retval;
		}

		if (query_4.has_ctrl24)
			rmi4_data->external_afe_buttons = true;
		else
			rmi4_data->external_afe_buttons = false;
	}

	if (!bdata->cap_button_map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: cap_button_map is NULL in board file\n",
				__func__);
		return -ENODEV;
	} else if (!bdata->cap_button_map->map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Button map is missing in board file\n",
				__func__);
		return -ENODEV;
	} else {
		if (bdata->cap_button_map->nbuttons != f1a->max_count) {
			f1a->valid_button_count = min(f1a->max_count,
					bdata->cap_button_map->nbuttons);
		} else {
			f1a->valid_button_count = f1a->max_count;
		}

		for (ii = 0; ii < f1a->valid_button_count; ii++)
			f1a->button_map[ii] = bdata->cap_button_map->map[ii];
	}

	return 0;
}

static void synaptics_rmi4_f1a_kfree(struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	if (f1a) {
		kfree(f1a->button_control.txrx_map);
		kfree(f1a->button_data_buffer);
		kfree(f1a->button_map);
		kfree(f1a);
		fhandler->data = NULL;
	}

	return;
}

static int synaptics_rmi4_f1a_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	retval = synaptics_rmi4_f1a_alloc_mem(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	retval = synaptics_rmi4_f1a_button_map(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	rmi4_data->button_0d_enabled = 1;

	return 0;

error_exit:
	synaptics_rmi4_f1a_kfree(fhandler);

	return retval;
}

static void synaptics_rmi4_empty_fn_list(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_fn *fhandler_temp;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry_safe(fhandler,
				fhandler_temp,
				&rmi->support_fn_list,
				link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
				synaptics_rmi4_f1a_kfree(fhandler);
			} else {
				kfree(fhandler->extra);
				kfree(fhandler->data);
			}
			list_del(&fhandler->link);
			kfree(fhandler);
		}
	}
	INIT_LIST_HEAD(&rmi->support_fn_list);

	return;
}

static int synaptics_rmi4_check_status(struct synaptics_rmi4_data *rmi4_data,
		bool *was_in_bl_mode)
{
	int retval;
	int timeout = CHECK_STATUS_TIMEOUT_MS;
	struct synaptics_rmi4_f01_device_status status;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			status.data,
			sizeof(status.data));
	if (retval < 0)
		return retval;

	while (status.status_code == STATUS_CRC_IN_PROGRESS) {
		if (timeout > 0)
			msleep(20);
		else
			return -EINVAL;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
		if (retval < 0)
			return retval;

		timeout -= 20;
	}

	if (timeout != CHECK_STATUS_TIMEOUT_MS)
		*was_in_bl_mode = true;

	if (status.flash_prog == 1) {
		rmi4_data->flash_prog_mode = true;
		pr_notice("%s: In flash prog mode, status = 0x%02x\n",
				__func__,
				status.status_code);
	} else {
		rmi4_data->flash_prog_mode = false;
	}

	return 0;
}

static void synaptics_rmi4_set_configured(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set configured\n",
				__func__);
		return;
	}

	rmi4_data->no_sleep_setting = device_ctrl & NO_SLEEP_ON;
	device_ctrl |= CONFIGURED;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set configured\n",
				__func__);
	}

	return;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
		struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kzalloc(sizeof(**fhandler), GFP_KERNEL);
	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base =
			(rmi_fd->data_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.ctrl_base =
			(rmi_fd->ctrl_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.cmd_base =
			(rmi_fd->cmd_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.query_base =
			(rmi_fd->query_base_addr |
			(page_number << 8));

	return 0;
}

static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char page_number;
	unsigned char intr_count;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned short pdt_entry_addr;
	bool f01found;
	bool f35found;
	bool was_in_bl_mode;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

rescan_pdt:
	f01found = false;
	f35found = false;
	was_in_bl_mode = false;
	intr_count = 0;
	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			pdt_entry_addr &= ~(MASK_8BIT << 8);

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(rmi4_data->pdev->dev.parent,
						"%s: Reached end of PDT\n",
						__func__);
				break;
			}

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: F%02x found (page %d)\n",
					__func__, rmi_fd.fn_number,
					page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				if (rmi_fd.intr_src_count == 0)
					break;

				f01found = true;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f01_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;

				retval = synaptics_rmi4_check_status(rmi4_data,
						&was_in_bl_mode);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to check status\n",
							__func__);
					return retval;
				}

				if (was_in_bl_mode) {
					kfree(fhandler);
					fhandler = NULL;
					goto rescan_pdt;
				}

				if (rmi4_data->flash_prog_mode)
					goto flash_prog_mode;

				break;
			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f11_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F12:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f12_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f1a_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0) {
#ifdef IGNORE_FN_INIT_FAILURE
					kfree(fhandler);
					fhandler = NULL;
#else
					return retval;
#endif
				}
				break;
			case SYNAPTICS_RMI4_F35:
				f35found = true;
				break;
#ifdef F51_DISCRETE_FORCE
			case SYNAPTICS_RMI4_F51:
				rmi4_data->f51_query_base_addr =
						rmi_fd.query_base_addr |
						(page_number << 8);
				break;
#endif
			}

			/* Accumulate the interrupt count */
			intr_count += rmi_fd.intr_src_count;

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
			}
		}
	}

	if (!f01found) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to find F01\n",
				__func__);
		if (!f35found) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to find F35\n",
					__func__);
			return -EINVAL;
		} else {
			pr_notice("%s: In microbootloader mode\n",
					__func__);
			return 0;
		}
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of interrupt registers = %d\n",
			__func__, rmi4_data->num_of_intr_regs);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr,
			f01_query,
			sizeof(f01_query));
	if (retval < 0)
		return retval;

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;

	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	rmi->product_info[0] = f01_query[2];
	rmi->product_info[1] = f01_query[3];
	retval = secure_memcpy(rmi->product_id_string,
			sizeof(rmi->product_id_string),
			&f01_query[11],
			sizeof(f01_query) - 11,
			PRODUCT_ID_SIZE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to copy product ID string\n",
				__func__);
	}

	if (rmi->manufacturer_id != 1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Non-Synaptics device found, manufacturer ID = %d\n",
				__func__, rmi->manufacturer_id);
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_BUID_ID_OFFSET,
			rmi->build_id,
			sizeof(rmi->build_id));
	if (retval < 0)
		return retval;

	rmi4_data->firmware_id = (unsigned int)rmi->build_id[0] +
			(unsigned int)rmi->build_id[1] * 0x100 +
			(unsigned int)rmi->build_id[2] * 0x10000;

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
						fhandler->intr_mask;
			}
		}
	}

	if (rmi4_data->f11_wakeup_gesture || rmi4_data->f12_wakeup_gesture)
		rmi4_data->enable_wakeup_gesture = WAKEUP_GESTURE;
	else
		rmi4_data->enable_wakeup_gesture = false;

	synaptics_rmi4_set_configured(rmi4_data);

	return 0;
}

static int synaptics_rmi4_gpio_setup(int gpio, bool config, int dir, int state, const char *label)
{
	int retval = 0;

	if (config) {
		retval = gpio_request(gpio, label);
		if (retval) {
			pr_err("%s: Failed to get gpio %d (code: %d)",
					__func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval) {
			pr_err("%s: Failed to set gpio %d direction",
					__func__, gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return retval;
}

static int synaptics_rmi4_pinctrl_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	rmi4_data->ts_pinctrl = devm_pinctrl_get((rmi4_data->pdev->dev.parent));
	if (IS_ERR_OR_NULL(rmi4_data->ts_pinctrl)) {
		retval = PTR_ERR(rmi4_data->ts_pinctrl);
		dev_dbg(rmi4_data->pdev->dev.parent,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	rmi4_data->pinctrl_state_active
		= pinctrl_lookup_state(rmi4_data->ts_pinctrl, PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_active)) {
		retval = PTR_ERR(rmi4_data->pinctrl_state_active);
		dev_err(rmi4_data->pdev->dev.parent,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	rmi4_data->pinctrl_state_suspend
		= pinctrl_lookup_state(rmi4_data->ts_pinctrl, PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(rmi4_data->pinctrl_state_suspend);
		dev_dbg(rmi4_data->pdev->dev.parent,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(rmi4_data->ts_pinctrl);
err_pinctrl_get:
	rmi4_data->ts_pinctrl = NULL;
	return retval;
}

static void synaptics_rmi4_set_params(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;
	struct synaptics_rmi4_f1a_handle *f1a;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_X, 0,
			rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_Y, 0,
			rmi4_data->sensor_max_y, 0, 0);
#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			rmi4_data->max_touch_width, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MINOR, 0,
			rmi4_data->max_touch_width, 0, 0);
#endif
	set_bit(KEY_SLEEP, rmi4_data->input_dev->keybit);
	input_set_capability(rmi4_data->input_dev, EV_KEY, KEY_SLEEP);

#ifdef REPORT_2D_PRESSURE
	if (rmi4_data->report_pressure) {
		input_set_abs_params(rmi4_data->input_dev,
				ABS_MT_PRESSURE, rmi4_data->force_min,
				rmi4_data->force_max, 0, 0);
	}
#elif defined(F51_DISCRETE_FORCE)
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_PRESSURE, 0,
			FORCE_LEVEL_MAX, 0, 0);
#endif

#ifdef TYPE_B_PROTOCOL
#ifdef KERNEL_ABOVE_3_6
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers);
#endif
#endif

	f1a = NULL;
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				f1a = fhandler->data;
		}
	}

	if (f1a) {
		for (ii = 0; ii < f1a->valid_button_count; ii++) {
			set_bit(f1a->button_map[ii],
					rmi4_data->input_dev->keybit);
			input_set_capability(rmi4_data->input_dev,
					EV_KEY, f1a->button_map[ii]);
		}
	}

	if (vir_button_map->nbuttons) {
		for (ii = 0; ii < vir_button_map->nbuttons; ii++) {
			set_bit(vir_button_map->map[ii * 5],
					rmi4_data->input_dev->keybit);
			input_set_capability(rmi4_data->input_dev,
					EV_KEY, vir_button_map->map[ii * 5]);
		}
	}

	if (rmi4_data->f11_wakeup_gesture || rmi4_data->f12_wakeup_gesture) {
		set_bit(KEY_WAKEUP, rmi4_data->input_dev->keybit);
		input_set_capability(rmi4_data->input_dev, EV_KEY, KEY_WAKEUP);
	}

	return;
}

/* Re-configure touch settings when wakeup gesture mode is switched in suspend */
static void synaptics_rmi4_wakeup_reconfigure(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	synaptics_rmi4_sleep_enable(rmi4_data, !enable);

	if (enable) {
		drm_regulator_ctrl(rmi4_data, DISP_REG_ALL, true);
		drm_reset_ctrl(rmi4_data->hw_if->board_data, true);
	}

	synaptics_rmi4_wakeup_gesture(rmi4_data, enable);

	if (!enable) {
		drm_regulator_ctrl(rmi4_data, DISP_REG_ALL, false);
		drm_reset_ctrl(rmi4_data->hw_if->board_data, false);
	}

	synaptics_rmi4_irq_enable(rmi4_data, enable, false);

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if (enable) {
				if (exp_fhandler->exp_fn->resume != NULL)
					exp_fhandler->exp_fn->resume(rmi4_data);
			} else {
				if (exp_fhandler->exp_fn->suspend != NULL)
					exp_fhandler->exp_fn->suspend(rmi4_data);
			}
		}
	}
	mutex_unlock(&exp_data.mutex);
}

static void synaptics_key_ctrl(struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	int retval;
	unsigned char ii;
	unsigned char intr_enable;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (list_empty(&rmi->support_fn_list))
		return ;

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
			ii = fhandler->intr_reg_num;

			retval = synaptics_rmi4_reg_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return ;

			if (enable == true)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_reg_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return ;
		}
	}

	return ;
}

static void cover_mode_set(struct synaptics_rmi4_data *rmi4_data, int enable)
{
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	int retval;
	unsigned char ctrl26_value;

	mutex_lock(&rmi4_data->rmi4_cover_mutex);

	if (rmi4_data->suspend)
		goto end;

	rmi = &(rmi4_data->rmi4_mod_info);

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F12)
			break;
	}

	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;

	if (enable == INPUT_EVENT_COVER_MODE_OFF) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + extra_data->ctrl26_offset,
				&ctrl26_value,
				1);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read ctrl26 reg\n",
					__func__);
			goto end;
		}

		ctrl26_value &= 0xfc;

		retval = synaptics_rmi4_reg_write(rmi4_data,
				fhandler->full_addr.ctrl_base + extra_data->ctrl26_offset,
				&ctrl26_value,
				1);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to close cover mode\n",
					__func__);
			goto end;
		}

		synaptics_key_ctrl(rmi4_data, true);
		rmi4_data->enable_cover_mode = 0;
	} else {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + extra_data->ctrl26_offset,
				&ctrl26_value,
				1);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read ctrl26 reg\n",
					__func__);
			goto end;
		}

		ctrl26_value |= 0x03;
		retval = synaptics_rmi4_reg_write(rmi4_data,
				fhandler->full_addr.ctrl_base + extra_data->ctrl26_offset,
				&ctrl26_value,
				1);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to change cover mode\n",
					__func__);
			goto end;
		}

		synaptics_key_ctrl(rmi4_data, false);
	};
end:
	mutex_unlock(&rmi4_data->rmi4_cover_mutex);
}

static void synaptics_rmi4_switch_mode_work(struct work_struct *work)
{
	struct synaptics_rmi4_mode_switch *ms = container_of(work, struct synaptics_rmi4_mode_switch, switch_mode_work);
	struct synaptics_rmi4_data *rmi4_data = ms->data;
	const struct synaptics_dsx_board_data *bdata = rmi4_data->hw_if->board_data;
	unsigned char value = ms->mode;

	if (value >= INPUT_EVENT_WAKUP_MODE_OFF && value <= INPUT_EVENT_WAKUP_MODE_ON) {
		if (bdata->cut_off_power) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Unable to switch wakeup gesture mode\n", __func__);
			return;
		}

		rmi4_data->enable_wakeup_gesture = value - INPUT_EVENT_WAKUP_MODE_OFF;
	} else if (value >= INPUT_EVENT_COVER_MODE_OFF && value <= INPUT_EVENT_COVER_MODE_ON) {
		rmi4_data->enable_cover_mode = value;
		cover_mode_set(rmi4_data, rmi4_data->enable_cover_mode);
	} else {
		dev_err(rmi4_data->pdev->dev.parent,
			"Does not support touch mode %d\n", value);
	}

	if (ms != NULL) {
		kfree(ms);
		ms = NULL;
	}
}

static int synaptics_rmi4_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	struct synaptics_rmi4_data *rmi4_data = input_get_drvdata(dev);
	struct synaptics_rmi4_mode_switch *ms;

	if (type == EV_SYN && code == SYN_CONFIG) {
		dev_info(rmi4_data->pdev->dev.parent,
			"Set input event value = %d\n", value);

		if (value >= INPUT_EVENT_START && value <= INPUT_EVENT_END) {
			ms = (struct synaptics_rmi4_mode_switch*)kmalloc(sizeof(struct synaptics_rmi4_mode_switch),
					GFP_ATOMIC);
			if (ms != NULL) {
				ms->data = rmi4_data;
				ms->mode = (unsigned char)value;
				INIT_WORK(&ms->switch_mode_work, synaptics_rmi4_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				dev_err(rmi4_data->pdev->dev.parent,
					"Failed in allocating memory for switching mode!\n");
				return -ENOMEM;
			}
		} else {
			dev_err(rmi4_data->pdev->dev.parent,
				"Invalid event value %d\n", value);
			return -EINVAL;
		}
	}

	return 0;
}

static int synaptics_rmi4_set_input_dev(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
				rmi4_data->hw_if->board_data;

	rmi4_data->input_dev = input_allocate_device();
	if (rmi4_data->input_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate input device\n",
				__func__);
		retval = -ENOMEM;
		goto err_input_device;
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	rmi4_data->input_dev->name = PLATFORM_DRIVER_NAME;
	rmi4_data->input_dev->phys = INPUT_PHYS_NAME;
	rmi4_data->input_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	rmi4_data->input_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	rmi4_data->input_dev->dev.parent = rmi4_data->pdev->dev.parent;
	rmi4_data->input_dev->event = synaptics_rmi4_input_event;
	input_set_drvdata(rmi4_data->input_dev, rmi4_data);

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);
	set_bit(EV_KEY, rmi4_data->input_dev->evbit);
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, rmi4_data->input_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif

	if (bdata->max_y_for_2d >= 0)
		rmi4_data->sensor_max_y = bdata->max_y_for_2d;

	synaptics_rmi4_set_params(rmi4_data);

	retval = input_register_device(rmi4_data->input_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register input device\n",
				__func__);
		goto err_register_input;
	}

	if (!rmi4_data->stylus_enable)
		return 0;

	rmi4_data->stylus_dev = input_allocate_device();
	if (rmi4_data->stylus_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate stylus device\n",
				__func__);
		retval = -ENOMEM;
		goto err_stylus_device;
	}

	rmi4_data->stylus_dev->name = STYLUS_DRIVER_NAME;
	rmi4_data->stylus_dev->phys = STYLUS_PHYS_NAME;
	rmi4_data->stylus_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	rmi4_data->stylus_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	rmi4_data->stylus_dev->dev.parent = rmi4_data->pdev->dev.parent;
	input_set_drvdata(rmi4_data->stylus_dev, rmi4_data);

	set_bit(EV_KEY, rmi4_data->stylus_dev->evbit);
	set_bit(EV_ABS, rmi4_data->stylus_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->stylus_dev->keybit);
	set_bit(BTN_TOOL_PEN, rmi4_data->stylus_dev->keybit);
	if (rmi4_data->eraser_enable)
		set_bit(BTN_TOOL_RUBBER, rmi4_data->stylus_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->stylus_dev->propbit);
#endif

	input_set_abs_params(rmi4_data->stylus_dev, ABS_X, 0,
			rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(rmi4_data->stylus_dev, ABS_Y, 0,
			rmi4_data->sensor_max_y, 0, 0);

	retval = input_register_device(rmi4_data->stylus_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register stylus device\n",
				__func__);
		goto err_register_stylus;
	}

	return 0;

err_register_stylus:
	rmi4_data->stylus_dev = NULL;

err_stylus_device:
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

err_register_input:
err_query_device:
	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_free_device(rmi4_data->input_dev);

err_input_device:
	return retval;
}

static int synaptics_rmi4_set_gpio(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	retval = synaptics_rmi4_gpio_setup(
			bdata->irq_gpio,
			true, 0, 0, bdata->irq_gpio_name);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to configure attention GPIO\n",
				__func__);
		goto err_gpio_irq;
	}

	if (bdata->power_gpio >= 0) {
		retval = synaptics_rmi4_gpio_setup(
				bdata->power_gpio,
				true, 1, !bdata->power_on_state, bdata->power_gpio_name);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to configure power GPIO\n",
					__func__);
			goto err_gpio_power;
		}
	}

	if (bdata->reset_gpio >= 0) {
		retval = synaptics_rmi4_gpio_setup(
				bdata->reset_gpio,
				true, 1, !bdata->reset_on_state, bdata->reset_gpio_name);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to configure reset GPIO\n",
					__func__);
			goto err_gpio_reset;
		}
	}

	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
		msleep(bdata->reset_delay_ms);
	}

	return 0;

err_gpio_reset:
	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0, NULL);

err_gpio_power:
	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0, NULL);

err_gpio_irq:
	return retval;
}

static int synaptics_rmi4_get_reg(struct synaptics_rmi4_data *rmi4_data,
		bool get)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		rmi4_data->pwr_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->pwr_reg_name);
		if (IS_ERR(rmi4_data->pwr_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get power regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->lab_reg_name != NULL) && (*bdata->lab_reg_name != 0)) {
		rmi4_data->lab_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->lab_reg_name);
		if (IS_ERR(rmi4_data->lab_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get lab regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->lab_reg);
			goto regulator_put;
		}
	}

	if ((bdata->ibb_reg_name != NULL) && (*bdata->ibb_reg_name != 0)) {
		rmi4_data->ibb_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->ibb_reg_name);
		if (IS_ERR(rmi4_data->ibb_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get ibb regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->ibb_reg);
			goto regulator_put;
		}
	}

	if ((bdata->disp_reg_name != NULL) && (*bdata->disp_reg_name != 0)) {
		rmi4_data->disp_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->disp_reg_name);
		if (IS_ERR(rmi4_data->disp_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get vddio regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->disp_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		rmi4_data->bus_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->bus_reg_name);
		if (IS_ERR(rmi4_data->bus_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get bus pullup regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->bus_reg);
			goto regulator_put;
		}
	}

	return 0;

regulator_put:
	if (rmi4_data->pwr_reg) {
		regulator_put(rmi4_data->pwr_reg);
		rmi4_data->pwr_reg = NULL;
	}

	if (rmi4_data->lab_reg) {
		regulator_put(rmi4_data->lab_reg);
		rmi4_data->lab_reg = NULL;
	}
	if (rmi4_data->ibb_reg) {
		regulator_put(rmi4_data->ibb_reg);
		rmi4_data->ibb_reg = NULL;
	}
	if (rmi4_data->disp_reg) {
		regulator_put(rmi4_data->disp_reg);
		rmi4_data->disp_reg = NULL;
	}

	if (rmi4_data->bus_reg) {
		regulator_put(rmi4_data->bus_reg);
		rmi4_data->bus_reg = NULL;
	}

	return retval;
}

static int synaptics_rmi4_enable_reg(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (rmi4_data->bus_reg) {
		retval = regulator_enable(rmi4_data->bus_reg);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable bus pullup regulator\n",
					__func__);
			goto exit;
		}
	}

	if (bdata->disp_power_hold && rmi4_data->disp_reg) {
		retval = regulator_enable(rmi4_data->disp_reg);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable disp regulator\n",
					__func__);
			goto disable_bus_reg;
		}
	}

	if (rmi4_data->pwr_reg) {
		retval = regulator_enable(rmi4_data->pwr_reg);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable power regulator\n",
					__func__);
			goto disable_disp_reg;
		}
		msleep(bdata->power_delay_ms);
	}

	return 0;

disable_pwr_reg:
	if (rmi4_data->pwr_reg)
		regulator_disable(rmi4_data->pwr_reg);

disable_disp_reg:
	if (bdata->disp_power_hold && rmi4_data->disp_reg)
		regulator_disable(rmi4_data->disp_reg);

disable_bus_reg:
	if (rmi4_data->bus_reg)
		regulator_disable(rmi4_data->bus_reg);

exit:
	return retval;
}

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

#ifdef TYPE_B_PROTOCOL
	for (ii = 0; ii < rmi4_data->num_of_fingers; ii++) {
		input_mt_slot(rmi4_data->input_dev, ii);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(rmi4_data->input_dev,
			BTN_TOUCH, 0);
	input_report_key(rmi4_data->input_dev,
			BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
	input_mt_sync(rmi4_data->input_dev);
#endif
	input_sync(rmi4_data->input_dev);

	if (rmi4_data->stylus_enable) {
		input_report_key(rmi4_data->stylus_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->stylus_dev,
				BTN_TOOL_PEN, 0);
		if (rmi4_data->eraser_enable) {
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOOL_RUBBER, 0);
		}
		input_sync(rmi4_data->stylus_dev);
	}
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	if (rmi4_data->palm_enabled)
		update_palm_sensor_value(0);
#endif
	rmi4_data->touchs = 0;

	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	rmi4_data->fingers_on_2d = false;

	return 0;
}

static int synaptics_rmi4_sw_reset(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char command = 0x01;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0)
		return retval;

	msleep(rmi4_data->hw_if->board_data->reset_delay_ms);

	if (rmi4_data->hw_if->ui_hw_init) {
		retval = rmi4_data->hw_if->ui_hw_init(rmi4_data);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static void synaptics_rmi4_rebuild_work(struct work_struct *work)
{
	int retval;
	unsigned char attr_count;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct delayed_work *delayed_work =
			container_of(work, struct delayed_work, work);
	struct synaptics_rmi4_data *rmi4_data =
			container_of(delayed_work, struct synaptics_rmi4_data,
			rb_work);

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	mutex_lock(&exp_data.mutex);

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->remove != NULL)
				exp_fhandler->exp_fn->remove(rmi4_data);
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	synaptics_rmi4_free_fingers(rmi4_data);
	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
	if (rmi4_data->stylus_enable) {
		input_unregister_device(rmi4_data->stylus_dev);
		rmi4_data->stylus_dev = NULL;
	}

	retval = synaptics_rmi4_sw_reset(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command\n",
				__func__);
		goto exit;
	}

	retval = synaptics_rmi4_set_input_dev(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set up input device\n",
				__func__);
		goto exit;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto exit;
		}
	}

	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->init != NULL)
				exp_fhandler->exp_fn->init(rmi4_data);
	}

	retval = 0;

exit:
	synaptics_rmi4_irq_enable(rmi4_data, true, false);

	mutex_unlock(&exp_data.mutex);

	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));

	return;
}

static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	synaptics_rmi4_free_fingers(rmi4_data);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F12) {
				synaptics_rmi4_f12_set_enables(rmi4_data, 0);
				break;
			}
		}
	}

	retval = synaptics_rmi4_int_enable(rmi4_data, true);
	if (retval < 0)
		goto exit;

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->reinit != NULL)
				exp_fhandler->exp_fn->reinit(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	synaptics_rmi4_set_configured(rmi4_data);

	retval = 0;

exit:
	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
	return retval;
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data,
		bool rebuild)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	if (rebuild) {
		queue_delayed_work(rmi4_data->rb_workqueue,
				&rmi4_data->rb_work,
				msecs_to_jiffies(REBUILD_WORK_DELAY_MS));
		return 0;
	}

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

	retval = synaptics_rmi4_sw_reset(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command\n",
				__func__);
		goto exit;
	}

	synaptics_rmi4_free_fingers(rmi4_data);

	synaptics_rmi4_empty_fn_list(rmi4_data);

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to query device\n",
				__func__);
		goto exit;
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->reset != NULL)
				exp_fhandler->exp_fn->reset(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	retval = 0;

exit:
	synaptics_rmi4_irq_enable(rmi4_data, true, false);

	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));

	return retval;
}

#ifdef FB_READY_RESET
static void synaptics_rmi4_reset_work(struct work_struct *work)
{
	int retval;
	unsigned int timeout;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(work, struct synaptics_rmi4_data,
			reset_work);

	timeout = FB_READY_TIMEOUT_S * 1000 / FB_READY_WAIT_MS + 1;

	while (!rmi4_data->fb_ready) {
		msleep(FB_READY_WAIT_MS);
		timeout--;
		if (timeout == 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Timed out waiting for FB ready\n",
					__func__);
			return;
		}
	}

	mutex_lock(&rmi4_data->rmi4_exp_init_mutex);

	retval = synaptics_rmi4_reset_device(rmi4_data, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command\n",
				__func__);
	}

	mutex_unlock(&rmi4_data->rmi4_exp_init_mutex);

	return;
}
#endif

static void synaptics_rmi4_sleep_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	unsigned char device_ctrl;
	unsigned char no_sleep_setting = rmi4_data->no_sleep_setting;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read device control\n",
				__func__);
		return;
	}

	device_ctrl = device_ctrl & ~MASK_3BIT;
	if (enable)
		device_ctrl = device_ctrl | NO_SLEEP_OFF | SENSOR_SLEEP;
	else
		device_ctrl = device_ctrl | no_sleep_setting | NORMAL_OPERATION;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write device control\n",
				__func__);
		return;
	}

	rmi4_data->sensor_sleep = enable;

	return;
}

static void synaptics_rmi4_exp_fn_work(struct work_struct *work)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler_temp;
	struct synaptics_rmi4_data *rmi4_data = exp_data.rmi4_data;

	mutex_lock(&rmi4_data->rmi4_exp_init_mutex);
	mutex_lock(&rmi4_data->rmi4_reset_mutex);
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry_safe(exp_fhandler,
				exp_fhandler_temp,
				&exp_data.list,
				link) {
			if ((exp_fhandler->exp_fn->init != NULL) &&
					exp_fhandler->insert) {
				exp_fhandler->exp_fn->init(rmi4_data);
				exp_fhandler->insert = false;
			} else if ((exp_fhandler->exp_fn->remove != NULL) &&
					exp_fhandler->remove) {
				exp_fhandler->exp_fn->remove(rmi4_data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&exp_data.mutex);
	mutex_unlock(&rmi4_data->rmi4_reset_mutex);
	mutex_unlock(&rmi4_data->rmi4_exp_init_mutex);

	return;
}

void synaptics_rmi4_new_function_force(struct synaptics_rmi4_exp_fn *exp_fn,
		bool insert)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	if (!exp_data.initialized) {
		mutex_init(&exp_data.mutex);
		INIT_LIST_HEAD(&exp_data.list);
		exp_data.initialized = true;
	}

	mutex_lock(&exp_data.mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
		if (!exp_fhandler) {
			pr_err("%s: Failed to alloc mem for expansion function\n",
					__func__);
			goto exit;
		}
		exp_fhandler->exp_fn = exp_fn;
		exp_fhandler->insert = true;
		exp_fhandler->remove = false;
		list_add_tail(&exp_fhandler->link, &exp_data.list);
	} else if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if (exp_fhandler->exp_fn->fn_type == exp_fn->fn_type) {
				exp_fhandler->insert = false;
				exp_fhandler->remove = true;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&exp_data.mutex);

	if (exp_data.queue_work) {
		queue_delayed_work(exp_data.workqueue,
				&exp_data.work,
				msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
	}

	return;
}

#ifdef CONFIG_TOUCH_DEBUG_FS
static int synaptics_tpdbg_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	synaptics_rmi4_irq_enable(rmi4_data, false, false);
	if (!rmi4_data->enable_wakeup_gesture)
		synaptics_rmi4_sleep_enable(rmi4_data, true);

	if (rmi4_data->enable_wakeup_gesture) {
		msleep(300);
		synaptics_rmi4_wakeup_gesture(rmi4_data, true);
		synaptics_rmi4_irq_enable(rmi4_data, true, false);
		goto exit;
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->suspend != NULL)
				exp_fhandler->exp_fn->suspend(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

exit:
	return 0;
}

static int synaptics_tpdbg_resume(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (rmi4_data->enable_wakeup_gesture) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		if (bdata->reset_gpio >= 0) {
			gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
			msleep(bdata->reset_active_ms);
			gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
			msleep(bdata->reset_delay_ms);
		}
	} else {
		rmi4_data->current_page = MASK_8BIT;
		if (bdata->reset_gpio >= 0) {
			gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
			msleep(bdata->reset_active_ms);
			gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
			msleep(bdata->reset_delay_ms);
		}

		synaptics_rmi4_sleep_enable(rmi4_data, false);
		synaptics_rmi4_irq_enable(rmi4_data, true, false);

		mutex_lock(&exp_data.mutex);
		if (!list_empty(&exp_data.list)) {
			list_for_each_entry(exp_fhandler, &exp_data.list, link)
				if (exp_fhandler->exp_fn->resume != NULL)
					exp_fhandler->exp_fn->resume(rmi4_data);
		}
		mutex_unlock(&exp_data.mutex);
	}

	return 0;
}

static void tpdbg_shutdown(struct synaptics_rmi4_data *rmi4_data, bool sleep)
{
	synaptics_rmi4_sleep_enable(rmi4_data, sleep);
}

static void tpdbg_suspend(struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	if (enable)
		synaptics_tpdbg_suspend(rmi4_data);
	else
		synaptics_tpdbg_resume(rmi4_data);
}

static int tpdbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	const char *str = "cmd support as below:\n \
				\necho \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
				\necho \"tp-sd-en\" of \"tp-sd-off\" to ctrl panel in or off sleep mode\n \
				\necho \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static ssize_t tpdbg_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct synaptics_rmi4_data *rmi4_data = file->private_data;
	char *cmd = kzalloc(size + 1, GFP_KERNEL);
	int ret = size;

	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}

	cmd[size] = '\0';

	if (!strncmp(cmd, "irq-disable", 11))
		disable_irq(rmi4_data->irq);
	else if (!strncmp(cmd, "irq-enable", 10))
		enable_irq(rmi4_data->irq);
	else if (!strncmp(cmd, "tp-sd-en", 8))
		tpdbg_shutdown(rmi4_data, true);
	else if (!strncmp(cmd, "tp-sd-off", 9))
		tpdbg_shutdown(rmi4_data, false);
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		tpdbg_suspend(rmi4_data, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		tpdbg_suspend(rmi4_data, false);
out:
	kfree(cmd);

	return ret;
}

static int tpdbg_release (struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = tpdbg_open,
	.read = tpdbg_read,
	.write = tpdbg_write,
	.release = tpdbg_release,
};
#endif

extern unsigned int get_hw_version_major(void);

static int synaptics_rmi4_probe(struct platform_device *pdev)
{
	int retval;
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data;
	const struct synaptics_dsx_hw_interface *hw_if;
	const struct synaptics_dsx_board_data *bdata;

	hw_if = pdev->dev.platform_data;
	if (!hw_if) {
		dev_err(&pdev->dev,
				"%s: No hardware interface found\n",
				__func__);
		return -EINVAL;
	}

	bdata = hw_if->board_data;
	if (!bdata) {
		dev_err(&pdev->dev,
				"%s: No board data found\n",
				__func__);
		return -EINVAL;
	}

	rmi4_data = kzalloc(sizeof(*rmi4_data), GFP_KERNEL);
	if (!rmi4_data) {
		dev_err(&pdev->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
	}

	rmi4_data->pdev = pdev;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->hw_if = hw_if;
	rmi4_data->suspend = false;
	rmi4_data->irq_enabled = false;
	rmi4_data->fingers_on_2d = false;
	rmi4_data->wakeup_en = false;

	rmi4_data->reset_device = synaptics_rmi4_reset_device;
	rmi4_data->irq_enable = synaptics_rmi4_irq_enable;
	rmi4_data->sleep_enable = synaptics_rmi4_sleep_enable;
//	rmi4_data->hw_version = get_hw_version_major();

	mutex_init(&(rmi4_data->rmi4_reset_mutex));
	mutex_init(&(rmi4_data->rmi4_report_mutex));
	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->rmi4_exp_init_mutex));
	mutex_init(&(rmi4_data->rmi4_irq_enable_mutex));
	mutex_init(&(rmi4_data->rmi4_cover_mutex));

	init_completion(&rmi4_data->dump_completion);

	platform_set_drvdata(pdev, rmi4_data);

	vir_button_map = bdata->vir_button_map;

	retval = synaptics_rmi4_get_reg(rmi4_data, true);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to get regulators\n",
				__func__);
		goto err_get_reg;
	}

	retval = synaptics_rmi4_enable_reg(rmi4_data, true);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to enable regulators\n",
				__func__);
		goto err_enable_reg;
	}

	retval = synaptics_rmi4_set_gpio(rmi4_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to set up GPIO's\n",
				__func__);
		goto err_set_gpio;
	}

	retval = synaptics_rmi4_pinctrl_init(rmi4_data);
	if (!retval && rmi4_data->ts_pinctrl) {
		retval = pinctrl_select_state(rmi4_data->ts_pinctrl,
				rmi4_data->pinctrl_state_active);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to select %s pinstate %d\n",
					__func__, PINCTRL_STATE_ACTIVE, retval);
			goto err_pinctrl_init;
		}
	} else {
		dev_err(&pdev->dev,
				"%s: Failed to init pinctrl\n",
				__func__);
		goto err_pinctrl_init;
	}
	if (hw_if->ui_hw_init) {
		retval = hw_if->ui_hw_init(rmi4_data);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to initialize hardware interface\n",
					__func__);
			goto err_ui_hw_init;
		}
	}

	retval = synaptics_rmi4_set_input_dev(rmi4_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to set up input device\n",
				__func__);
		goto err_set_input_dev;
	}

	synaptics_rmi4_query_chip_id(rmi4_data);

#ifdef CONFIG_DRM
	if (!rmi4_data->chip_is_tddi)
		rmi4_data->drm_notifier.notifier_call = synaptics_rmi4_drm_notifier_cb;
	else
		rmi4_data->drm_notifier.notifier_call = synaptics_rmi4_drm_notifier_cb_tddi;
	retval = drm_register_client(&rmi4_data->drm_notifier);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to register fb notifier client\n",
				__func__);
	}
#endif

#ifdef USE_EARLYSUSPEND
	rmi4_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	rmi4_data->early_suspend.suspend = synaptics_rmi4_early_suspend;
	rmi4_data->early_suspend.resume = synaptics_rmi4_late_resume;
	register_early_suspend(&rmi4_data->early_suspend);
#endif

	if (!exp_data.initialized) {
		mutex_init(&exp_data.mutex);
		INIT_LIST_HEAD(&exp_data.list);
		exp_data.initialized = true;
	}

	rmi4_data->irq = gpio_to_irq(bdata->irq_gpio);

	retval = synaptics_rmi4_irq_enable(rmi4_data, true, false);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to enable attention interrupt\n",
				__func__);
		goto err_enable_irq;
	}

	if (vir_button_map->nbuttons) {
		rmi4_data->board_prop_dir = kobject_create_and_add(
				"board_properties", NULL);
		if (!rmi4_data->board_prop_dir) {
			dev_err(&pdev->dev,
					"%s: Failed to create board_properties directory\n",
					__func__);
			goto err_virtual_buttons;
		} else {
			retval = sysfs_create_file(rmi4_data->board_prop_dir,
					&virtual_key_map_attr.attr);
			if (retval < 0) {
				dev_err(&pdev->dev,
						"%s: Failed to create virtual key map file\n",
						__func__);
				goto err_virtual_buttons;
			}
		}
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto err_sysfs;
		}
	}

	retval = sysfs_create_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_color.attr);

	if (retval < 0) {
		dev_err(&pdev->dev,
			"%s: Failed to create sysfs attributes\n",
			__func__);
		goto err_sysfs;
	}

#if defined(CONFIG_SECURE_TOUCH)
	retval = sysfs_create_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch.attr);

	if (retval < 0) {
		dev_err(&pdev->dev,
			"%s: Failed to create sysfs attributes\n",
			__func__);
		goto err_sysfs_secure;
	}

	retval = sysfs_create_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch_enable.attr);

	if (retval < 0) {
		dev_err(&pdev->dev,
			"%s: Failed to create sysfs attributes\n",
			__func__);
		goto err_sysfs_secure_enable;
	}
#endif

	retval = sysfs_create_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_vendor.attr);

	if (retval < 0) {
		dev_err(&pdev->dev,
			"%s: Failed to create sysfs attributes\n",
			__func__);
		goto err_sysfs_panel_vendor;
	}

#ifdef CONFIG_TOUCH_DEBUG_FS
	rmi4_data->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (rmi4_data->debugfs) {
		debugfs_create_file("switch_state", 0660, rmi4_data->debugfs, rmi4_data, &tpdbg_operations);
	}
#endif

	dev_set_drvdata(rmi4_data->pdev->dev.parent, rmi4_data);

	rmi4_data->rb_workqueue =
			create_singlethread_workqueue("dsx_rebuild_workqueue");
	INIT_DELAYED_WORK(&rmi4_data->rb_work, synaptics_rmi4_rebuild_work);

	exp_data.workqueue = create_singlethread_workqueue("dsx_exp_workqueue");
	INIT_DELAYED_WORK(&exp_data.work, synaptics_rmi4_exp_fn_work);
	exp_data.rmi4_data = rmi4_data;
	exp_data.queue_work = true;
	queue_delayed_work(exp_data.workqueue,
			&exp_data.work,
			0);

#ifdef FB_READY_RESET
	rmi4_data->reset_workqueue =
			create_singlethread_workqueue("dsx_reset_workqueue");
	INIT_WORK(&rmi4_data->reset_work, synaptics_rmi4_reset_work);
	queue_work(rmi4_data->reset_workqueue, &rmi4_data->reset_work);
#endif

	device_init_wakeup(&pdev->dev, 1);

	synaptics_secure_touch_init(rmi4_data);
	synaptics_secure_touch_stop(rmi4_data, 1);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	rmi4_data->palm_tx_grip_disable = bdata->palm_tx_disable;
	rmi4_data->palm_rx_area_threshold = bdata->palm_rx_area;
	rmi4_data->palm_rx_channel_threshold = bdata->palm_rx_channel;
	dev_info(&pdev->dev, "%s load palm sensor dts param:tx_disable:%d, rx_area:%d, rx_channel:%d\n", __func__,
		rmi4_data->palm_tx_grip_disable, rmi4_data->palm_rx_area_threshold, rmi4_data->palm_rx_channel_threshold);
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	xiaomi_touch_interfaces.palm_sensor_write = synaptics_rmi4_palmsensor_enable;
	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif
#endif
#ifdef CONFIG_SYNA_TOUCH_COUNT_DUMP
	if (rmi4_data->syna_tp_class == NULL)
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
		rmi4_data->syna_tp_class = get_xiaomi_touch_class();
#else
		rmi4_data->syna_tp_class = class_create(THIS_MODULE, "touch");
#endif
	rmi4_data->syna_touch_dev = device_create(rmi4_data->syna_tp_class, NULL, 0x20, rmi4_data, "touch_suspend_notify");

	if (IS_ERR(rmi4_data->syna_touch_dev)) {
		dev_err(&pdev->dev, "%s ERROR: Failed to create device for the sysfs!\n", __func__);
		goto err_clickdump;
	}

	dev_set_drvdata(rmi4_data->syna_touch_dev, rmi4_data);
	retval = sysfs_create_file(&rmi4_data->syna_touch_dev->kobj, &dev_attr_touch_suspend_notify.attr);

	if (retval) {
		dev_err(&pdev->dev, "%s ERROR: Failed to create sysfs group!\n", __func__);
		goto err_clickdump;
	}
#endif

	return retval;

#ifdef CONFIG_SYNA_TOUCH_COUNT_DUMP
err_clickdump:
	device_destroy(rmi4_data->syna_tp_class, 0x20);
#endif
err_sysfs_panel_vendor:
#if defined(CONFIG_SECURE_TOUCH)
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch_enable.attr);
err_sysfs_secure_enable:
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch.attr);
err_sysfs_secure:
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_color.attr);
#endif
err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

err_virtual_buttons:
	if (rmi4_data->board_prop_dir) {
		sysfs_remove_file(rmi4_data->board_prop_dir,
				&virtual_key_map_attr.attr);
		kobject_put(rmi4_data->board_prop_dir);
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

err_enable_irq:
#ifdef CONFIG_DRM
	drm_unregister_client(&rmi4_data->drm_notifier);
#endif

#ifdef USE_EARLYSUSPEND
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
	if (rmi4_data->stylus_enable) {
		input_unregister_device(rmi4_data->stylus_dev);
		rmi4_data->stylus_dev = NULL;
	}

err_set_input_dev:
	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0, NULL);

	if (bdata->reset_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->reset_gpio, false, 0, 0, NULL);

	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0, NULL);
err_ui_hw_init:
err_pinctrl_init:
	if (rmi4_data->ts_pinctrl) {
		devm_pinctrl_put(rmi4_data->ts_pinctrl);
		rmi4_data->ts_pinctrl = NULL;
	}

err_set_gpio:
	synaptics_rmi4_enable_reg(rmi4_data, false);

err_enable_reg:
	synaptics_rmi4_get_reg(rmi4_data, false);

err_get_reg:
	kfree(rmi4_data);

	return retval;
}

static int synaptics_rmi4_remove(struct platform_device *pdev)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = platform_get_drvdata(pdev);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

#ifdef CONFIG_SYNA_TOUCH_COUNT_DUMP
		if (rmi4_data->hw_if->board_data->dump_click_count && !rmi4_data->current_clicknum_file) {
			kfree(rmi4_data->current_clicknum_file);
			rmi4_data->current_clicknum_file = NULL;
		}
#endif
#ifdef CONFIG_SYNA_TOUCH_COUNT_DUMP
		sysfs_remove_file(&rmi4_data->syna_touch_dev->kobj, &dev_attr_touch_suspend_notify.attr);
		device_destroy(rmi4_data->syna_tp_class, 0X20);
#endif

#ifdef FB_READY_RESET
	cancel_work_sync(&rmi4_data->reset_work);
	flush_workqueue(rmi4_data->reset_workqueue);
	destroy_workqueue(rmi4_data->reset_workqueue);
#endif

	cancel_delayed_work_sync(&exp_data.work);
	flush_workqueue(exp_data.workqueue);
	destroy_workqueue(exp_data.workqueue);

	cancel_delayed_work_sync(&rmi4_data->rb_work);
	flush_workqueue(rmi4_data->rb_workqueue);
	destroy_workqueue(rmi4_data->rb_workqueue);

#ifdef CONFIG_TOUCH_DEBUG_FS
	debugfs_remove_recursive(rmi4_data->debugfs);
#endif

	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_vendor.attr);
#if defined(CONFIG_SECURE_TOUCH)
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch_enable.attr);
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch.attr);
#endif
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_color.attr);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	if (rmi4_data->board_prop_dir) {
		sysfs_remove_file(rmi4_data->board_prop_dir,
				&virtual_key_map_attr.attr);
		kobject_put(rmi4_data->board_prop_dir);
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

#ifdef CONFIG_DRM
	drm_unregister_client(&rmi4_data->drm_notifier);
#endif

#ifdef USE_EARLYSUSPEND
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
	if (rmi4_data->stylus_enable) {
		input_unregister_device(rmi4_data->stylus_dev);
		rmi4_data->stylus_dev = NULL;
	}

	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0, NULL);

	if (bdata->reset_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->reset_gpio, false, 0, 0, NULL);

	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0, NULL);

	synaptics_rmi4_enable_reg(rmi4_data, false);
	synaptics_rmi4_get_reg(rmi4_data, false);

	kfree(rmi4_data);

	return 0;
}

static void synaptics_rmi4_f11_wg(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	unsigned char reporting_control;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F11)
			break;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			&reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	reporting_control = (reporting_control & ~MASK_3BIT);
	if (enable)
		reporting_control |= F11_WAKEUP_GESTURE_MODE;
	else
		reporting_control |= F11_CONTINUOUS_MODE;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fhandler->full_addr.ctrl_base,
			&reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	return;
}

static void drm_regulator_ctrl(struct synaptics_rmi4_data *rmi4_data, unsigned int flag, bool enable)
{
	int retval = 0;
	static unsigned int status;

	if (rmi4_data == NULL)
		return;
	if (!rmi4_data->hw_if->board_data->panel_is_incell)
		return;

	if (enable) {
		if (rmi4_data->disp_reg && (flag & DISP_REG_VDD) && !(status & DISP_REG_VDD)) {
			retval = regulator_enable(rmi4_data->disp_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to enable disp regulator\n",
						__func__);
				return;
			}

			status |= DISP_REG_VDD;
		}

		if (rmi4_data->lab_reg && (flag & DISP_REG_LAB) && !(status & DISP_REG_LAB)) {
			retval = regulator_enable(rmi4_data->lab_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to enable lab regulator\n",
						__func__);
				return;
			}

			status |= DISP_REG_LAB;
		}

		if (rmi4_data->ibb_reg && (flag & DISP_REG_IBB) && !(status & DISP_REG_IBB)) {
			retval = regulator_enable(rmi4_data->ibb_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to enable ibb regulator\n",
						__func__);
				return;
			}

			status |= DISP_REG_IBB;
		}
	} else {
		if (rmi4_data->ibb_reg && (flag & DISP_REG_IBB) && (status & DISP_REG_IBB)) {
			retval = regulator_disable(rmi4_data->ibb_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable ibb regulator\n",
						__func__);
				return;
			}

			status &= ~DISP_REG_IBB;
		}

		if (rmi4_data->lab_reg && (flag & DISP_REG_LAB) && (status & DISP_REG_LAB)) {
			retval = regulator_disable(rmi4_data->lab_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable lab regulator\n",
						__func__);
				return;
			}

			status &= ~DISP_REG_LAB;
		}

		if (rmi4_data->disp_reg && (flag & DISP_REG_VDD) && (status & DISP_REG_VDD)) {
			retval = regulator_disable(rmi4_data->disp_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable disp regulator\n",
						__func__);
				return;
			}

			status &= ~DISP_REG_VDD;
		}
	}
}

static void drm_reset_ctrl(const struct synaptics_dsx_board_data *bdata, bool on)
{
	if (!bdata->panel_is_incell)
		return;

	if (bdata->drm_reset > 0) {
		if (on)
			gpio_set_value(bdata->drm_reset, bdata->drm_reset_state);
		else
			gpio_set_value(bdata->drm_reset, !bdata->drm_reset_state);
	}
}

#if 0
static void mdss_panel_poweron(struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	if (!rmi4_data->hw_if->board_data->panel_is_incell)
		return;

	if (enable) {
#if 0
		/*disp regulator always on, so do not control this regulator*/
		if (rmi4_data->panel_power_seq.disp_pre_on_sleep)
			msleep(rmi4_data->panel_power_seq.disp_pre_on_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_VDD, true);
		if (rmi4_data->panel_power_seq.disp_post_on_sleep)
			msleep(rmi4_data->panel_power_seq.disp_post_on_sleep);
#endif
		if (rmi4_data->panel_power_seq.lab_pre_on_sleep)
			msleep(rmi4_data->panel_power_seq.lab_pre_on_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_LAB, true);
		if (rmi4_data->panel_power_seq.lab_post_on_sleep)
			msleep(rmi4_data->panel_power_seq.lab_post_on_sleep);

		if (rmi4_data->panel_power_seq.ibb_pre_on_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_pre_on_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_IBB, true);
		if (rmi4_data->panel_power_seq.ibb_post_on_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_post_on_sleep);
	} else {
		if (rmi4_data->panel_power_seq.ibb_pre_off_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_pre_off_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_IBB, false);
		if (rmi4_data->panel_power_seq.ibb_post_off_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_post_off_sleep);

		if (rmi4_data->panel_power_seq.lab_pre_off_sleep)
			msleep(rmi4_data->panel_power_seq.lab_pre_off_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_LAB, false);
		if (rmi4_data->panel_power_seq.lab_post_off_sleep)
			msleep(rmi4_data->panel_power_seq.lab_post_off_sleep);
#if 0
		/*disp regulator always on, so do not control this regulator*/
		if (rmi4_data->panel_power_seq.disp_pre_off_sleep)
			msleep(rmi4_data->panel_power_seq.disp_pre_off_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_VDD, false);
		if (rmi4_data->panel_power_seq.disp_post_off_sleep)
			msleep(rmi4_data->panel_power_seq.disp_post_off_sleep);
#endif
	}
	pr_debug("power %s seq:\n", enable ? "on" : "off");
#if 0
	/*disp regulator always on, so do not control this regulator*/
	pr_debug("IOVDD: preonsleep=%d,postonsleep=%d,preoffsleep=%d,postoffsleep=%d\n",
			rmi4_data->panel_power_seq.disp_pre_on_sleep,
			rmi4_data->panel_power_seq.disp_post_on_sleep,
			rmi4_data->panel_power_seq.disp_pre_off_sleep,
			rmi4_data->panel_power_seq.disp_post_off_sleep);
#endif
	pr_debug("LAB: preonsleep=%d,postonsleep=%d,preoffsleep=%d,postoffsleep=%d\n",
			rmi4_data->panel_power_seq.lab_pre_on_sleep,
			rmi4_data->panel_power_seq.lab_post_on_sleep,
			rmi4_data->panel_power_seq.lab_pre_off_sleep,
			rmi4_data->panel_power_seq.lab_post_off_sleep);
	pr_debug("IBB: preonsleep=%d,postonsleep=%d,preoffsleep=%d,postoffsleep=%d\n",
			rmi4_data->panel_power_seq.ibb_pre_on_sleep,
			rmi4_data->panel_power_seq.ibb_post_on_sleep,
			rmi4_data->panel_power_seq.ibb_pre_off_sleep,
			rmi4_data->panel_power_seq.ibb_post_off_sleep);
}
#endif
static void drm_reset_action(const struct synaptics_dsx_board_data *bdata)
{
	if (bdata->drm_reset > 0) {
		gpio_set_value(bdata->drm_reset, !bdata->drm_reset_state);
		msleep(10);
		gpio_set_value(bdata->drm_reset, bdata->drm_reset_state);
		msleep(10);
	}
}

static void synaptics_rmi4_f12_wg(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	unsigned char offset;
	unsigned char reporting_control[3];
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F12)
			break;
	}

	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	offset = extra_data->ctrl20_offset;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	if (enable)
		reporting_control[2] = F12_WAKEUP_GESTURE_MODE;
	else
		reporting_control[2] = F12_CONTINUOUS_MODE;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));

	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	return;
}

static void synaptics_rmi4_wakeup_gesture(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	if (rmi4_data->f11_wakeup_gesture)
		synaptics_rmi4_f11_wg(rmi4_data, enable);
	else if (rmi4_data->f12_wakeup_gesture)
		synaptics_rmi4_f12_wg(rmi4_data, enable);

	return;
}

#ifdef CONFIG_DRM
static int synaptics_rmi4_drm_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data)
{
	int *transition;
	struct drm_notify_data *evdata = data;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(self, struct synaptics_rmi4_data,
			drm_notifier);

	const struct synaptics_dsx_board_data *bdata = NULL;

	if (rmi4_data->hw_if->board_data)
		bdata = rmi4_data->hw_if->board_data;
	else
		return 0;

	/* Receive notifications from primary panel only */
	if (evdata && evdata->data && rmi4_data && evdata->is_primary) {
		if (event == DRM_EVENT_BLANK) {
			transition = evdata->data;
			if (*transition == DRM_BLANK_POWERDOWN) {
				synaptics_rmi4_suspend(&rmi4_data->pdev->dev);
				rmi4_data->fb_ready = false;
			} else if (*transition == DRM_BLANK_UNBLANK) {
				synaptics_rmi4_resume(&rmi4_data->pdev->dev);
				rmi4_data->fb_ready = true;
				if (rmi4_data->wakeup_en) {
					drm_panel_reset_skip_enable(false);
					//drm_regulator_ctrl(rmi4_data, DISP_REG_ALL, false);
					drm_dsi_ulps_enable(false);
					rmi4_data->wakeup_en = false;
				}

				rmi4_data->disable_data_dump = false;
			}
		} else if (event == DRM_EARLY_EVENT_BLANK) {
			transition = evdata->data;
			if (*transition == DRM_BLANK_POWERDOWN) {
				rmi4_data->disable_data_dump = true;
				if (rmi4_data->dump_flags) {
					reinit_completion(&rmi4_data->dump_completion);
					wait_for_completion_timeout(&rmi4_data->dump_completion, 4 * HZ);
				}

				if (rmi4_data->enable_wakeup_gesture) {
					rmi4_data->wakeup_en = true;
					//drm_regulator_ctrl(rmi4_data, DISP_REG_ALL, true);
					drm_panel_reset_skip_enable(true);
					drm_dsi_ulps_enable(true);
				}
			} else if (*transition == DRM_BLANK_UNBLANK) {
				if (bdata->reset_gpio >= 0 && rmi4_data->suspend) {
					gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
					msleep(bdata->reset_active_ms);
					gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
				}
				if (rmi4_data->wakeup_en) {
					drm_reset_action(bdata);
				}
			}
		}
	}
	return 0;
}

static int synaptics_rmi4_drm_notifier_cb_tddi(struct notifier_block *self,
		unsigned long event, void *data)
{
#if 0
	int *transition;
	struct fb_event *evdata = data;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(self, struct synaptics_rmi4_data,
			fb_notifier);

	if (mdss_prim_panel_is_dead())
		return 0;

	/* Receive notifications from primary panel only */
	if (evdata && evdata->data && rmi4_data && mdss_panel_is_prim(evdata->info)) {
		if (event == FB_EVENT_BLANK) {
			transition = evdata->data;
			if ((*transition == FB_BLANK_UNBLANK) || (*transition == FB_BLANK_NORMAL)) {
				if (rmi4_data->wakeup_en) {
					mdss_panel_reset_skip_enable(false);
					mdss_regulator_ctrl(rmi4_data, DISP_REG_ALL, false);
					rmi4_data->wakeup_en = false;
				} else {
					synaptics_rmi4_resume(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = true;
				}

				rmi4_data->disable_data_dump = false;
			} else if ((*transition == FB_BLANK_POWERDOWN) || (*transition == FB_BLANK_NORMAL)) {
				if (rmi4_data->wakeup_en) {
					synaptics_rmi4_suspend(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = true;
				}
			}
		} else if (event == FB_EARLY_EVENT_BLANK) {
			transition = evdata->data;
			if (*transition == FB_BLANK_UNBLANK) {
				if (rmi4_data->wakeup_en) {
					synaptics_rmi4_resume(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = true;
					msleep(30);
				}
			} else if ((*transition == FB_BLANK_POWERDOWN) || (*transition == FB_BLANK_NORMAL)) {
				rmi4_data->disable_data_dump = true;
				if (rmi4_data->dump_flags) {
					reinit_completion(&rmi4_data->dump_completion);
					wait_for_completion_timeout(&rmi4_data->dump_completion, 4 * HZ);
				}

				if (rmi4_data->enable_wakeup_gesture) {
					rmi4_data->wakeup_en = true;
					mdss_panel_reset_skip_enable(true);
					mdss_regulator_ctrl(rmi4_data, DISP_REG_ALL, true);
				}

				if (!rmi4_data->wakeup_en) {
					synaptics_rmi4_suspend(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = false;
				}
			}
		}
	}
#endif
	return 0;
}

#endif

#ifdef USE_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	if (rmi4_data->stay_awake)
		return;

	if (rmi4_data->enable_wakeup_gesture) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, true);
		enable_irq_wake(rmi4_data->irq);
		goto exit;
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);
	synaptics_rmi4_sleep_enable(rmi4_data, true);
	synaptics_rmi4_free_fingers(rmi4_data);

exit:
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->early_suspend != NULL)
				exp_fhandler->exp_fn->early_suspend(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->suspend = true;

	return;
}

static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
#ifdef FB_READY_RESET
	int retval;
#endif
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	if (rmi4_data->stay_awake)
		return;

	if (rmi4_data->enable_wakeup_gesture) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		disable_irq_wake(rmi4_data->irq);
		goto exit;
	}

	rmi4_data->current_page = MASK_8BIT;

	if (rmi4_data->suspend) {
		synaptics_rmi4_sleep_enable(rmi4_data, false);
		synaptics_rmi4_irq_enable(rmi4_data, true, false);
	}

exit:
#ifdef FB_READY_RESET
	if (rmi4_data->suspend) {
		retval = synaptics_rmi4_reset_device(rmi4_data, false);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to issue reset command\n",
					__func__);
		}
	}
#endif
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->late_resume != NULL)
				exp_fhandler->exp_fn->late_resume(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->suspend = false;

	return;
}
#endif

static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (rmi4_data->stay_awake || rmi4_data->suspend)
		return 0;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	if (rmi4_data->palm_enabled) {
		update_palm_sensor_value(0);
		synaptics_rmi4_palm_enable(rmi4_data, 0);
		rmi4_data->palm_enabled = false;
	}
#endif
	if (bdata->cut_off_power || (rmi4_data->chip_is_tddi && !rmi4_data->wakeup_en)) {
		if (rmi4_data->fw_updating)
			return 0;

		synaptics_rmi4_irq_enable(rmi4_data, false, false);

		if (bdata->power_gpio >= 0)
			gpio_set_value(bdata->power_gpio,
					!bdata->power_on_state);

		if (bdata->reset_gpio >= 0) {
			gpio_set_value(bdata->reset_gpio,
					bdata->reset_on_state);
			mdelay(bdata->reset_active_ms);
		}

		synaptics_rmi4_enable_reg(rmi4_data, false);
	} else {
		synaptics_secure_touch_stop(rmi4_data, 1);

		synaptics_rmi4_irq_enable(rmi4_data, false, false);

		if (!rmi4_data->wakeup_en)
			synaptics_rmi4_sleep_enable(rmi4_data, true);
		else {
			if (rmi4_data->chip_is_tddi)
				msleep(120);
			synaptics_rmi4_wakeup_gesture(rmi4_data, true);
			synaptics_rmi4_irq_enable(rmi4_data, true, false);
			goto exit;
		}

		mutex_lock(&exp_data.mutex);
		if (!list_empty(&exp_data.list)) {
			list_for_each_entry(exp_fhandler, &exp_data.list, link)
				if (exp_fhandler->exp_fn->suspend != NULL)
					exp_fhandler->exp_fn->suspend(rmi4_data);
		}
		mutex_unlock(&exp_data.mutex);
	}

exit:
	synaptics_rmi4_free_fingers(rmi4_data);

	rmi4_data->suspend = true;

	sysfs_notify(&rmi4_data->syna_touch_dev->kobj, NULL, "touch_suspend_notify");

	return 0;
}

static int synaptics_rmi4_resume(struct device *dev)
{
#ifdef FB_READY_RESET
	int retval;
#endif
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

#ifdef CONFIG_DRM
	static int skip;

	if (skip == 0) {
		skip = 1;
		return 0;
	}
#endif

	if (rmi4_data->stay_awake || !rmi4_data->suspend)
		return 0;

	if (bdata->cut_off_power || (rmi4_data->chip_is_tddi && !rmi4_data->wakeup_en)) {
		synaptics_rmi4_enable_reg(rmi4_data, true);

		if (bdata->power_gpio >= 0) {
			gpio_set_value(bdata->power_gpio,
					bdata->power_on_state);
			mdelay(bdata->power_delay_ms);
		}

		if (bdata->reset_gpio >= 0) {
			gpio_set_value(bdata->reset_gpio,
					!bdata->reset_on_state);
			mdelay(bdata->reset_delay_ms);
		}

		synaptics_rmi4_irq_enable(rmi4_data, true, false);
	} else {
		synaptics_secure_touch_stop(rmi4_data, 0);

		if (rmi4_data->wakeup_en) {
			synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		} else {
			rmi4_data->current_page = MASK_8BIT;

			synaptics_rmi4_sleep_enable(rmi4_data, false);
			synaptics_rmi4_irq_enable(rmi4_data, true, false);

#ifdef FB_READY_RESET
			retval = synaptics_rmi4_reset_device(rmi4_data, false);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to issue reset command\n",
						__func__);
			}
#endif
			mutex_lock(&exp_data.mutex);
			if (!list_empty(&exp_data.list)) {
				list_for_each_entry(exp_fhandler, &exp_data.list, link)
					if (exp_fhandler->exp_fn->resume != NULL)
						exp_fhandler->exp_fn->resume(rmi4_data);
			}
			mutex_unlock(&exp_data.mutex);
		}
	}

	rmi4_data->suspend = false;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
			if (rmi4_data->palm_enabled && !rmi4_data->palm_sensor_changed) {
				synaptics_rmi4_palm_enable(rmi4_data, 1);
				rmi4_data->palm_sensor_changed = true;
			}
#endif

	if (rmi4_data->enable_cover_mode)
		cover_mode_set(rmi4_data, rmi4_data->enable_cover_mode);

	sysfs_notify(&rmi4_data->syna_touch_dev->kobj, NULL, "touch_suspend_notify");
	return 0;
}

#ifdef CONFIG_PM
static int synaptics_rmi4_pm_suspend(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (device_may_wakeup(dev) &&
			rmi4_data->wakeup_en &&
			!bdata->cut_off_power) {
		dev_info(rmi4_data->pdev->dev.parent,
			"Enable touch irq wake\n");
		disable_irq(rmi4_data->irq);
		enable_irq_wake(rmi4_data->irq);
	}

	return 0;

}

static int synaptics_rmi4_pm_resume(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (device_may_wakeup(dev) &&
			rmi4_data->wakeup_en &&
			!bdata->cut_off_power) {
		dev_info(rmi4_data->pdev->dev.parent,
			"Disable touch irq wake\n");
		disable_irq_wake(rmi4_data->irq);
		enable_irq(rmi4_data->irq);
	}

	return 0;
}

static const struct dev_pm_ops synaptics_rmi4_dev_pm_ops = {
	.suspend = synaptics_rmi4_pm_suspend,
	.resume  = synaptics_rmi4_pm_resume,
};
#endif

static struct platform_driver synaptics_rmi4_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_FORCE,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = synaptics_rmi4_remove,
};

static int __init synaptics_rmi4_init(void)
{
	int retval;

	retval = synaptics_rmi4_bus_init_force();
	if (retval)
		return retval;

	return platform_driver_register(&synaptics_rmi4_driver);
}

static void __exit synaptics_rmi4_exit(void)
{
	platform_driver_unregister(&synaptics_rmi4_driver);

	synaptics_rmi4_bus_exit_force();

	return;
}

module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Touch Driver");
MODULE_LICENSE("GPL v2");
