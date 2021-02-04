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
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/stringify.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/device.h>
//#include <linux/rtpm_prio.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include<mt-plat/mtk_boot_common.h>
//#include "mt_gpio.h"

//#include "cust_gpio_usage.h"
#include <asm/uaccess.h>

#include "synaptics_dsx_i2c.h"
#include "synaptics_dsx.h"
#include "tpd_custom_synaptics.h"
//#include "mt_boot_common.h"


#ifdef CONFIG_OF_TOUCH
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#else
#include <cust_eint.h>
#endif

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#define SENSOR_DEVICE "td4322"
#define TPD_DEVICE "synaptics_dsx_"SENSOR_DEVICE

#define DRIVER_NAME "synaptics_dsx_i2c"
#define INPUT_PHYS_NAME "synaptics_dsx_i2c/input0"

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

#define UBL_I2C_ADDR 0x2c

#define SENSOR_MAX_X 1080
#define SENSOR_MAX_Y 2160

#define WAKEUP_GESTURE false

#define NO_0D_WHILE_2D
/*
#define REPORT_2D_Z
*/
#define REPORT_2D_W

#define F12_DATA_15_WORKAROUND

/*
#define IGNORE_FN_INIT_FAILURE
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
#define attrify(propname) (&dev_attr_##propname.attr)

#define EXP_FN_WORK_DELAY_MS 1000 /* ms */
#define SYN_I2C_RETRY_TIMES 5
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS	100
#define DELAY_BOOT_READY	200
#define DELAY_RESET_LOW		20
#define DELAY_UI_READY		200

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12

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
#define F12_CONTINUOUS_MODE 0xFD
#define F12_WAKEUP_GESTURE_MODE 0x02

#define F12_UDG_DETECT 0x0f

//for MTK DMA
//#define USE_I2C_DMA
#ifdef USE_I2C_DMA
#include <linux/dma-mapping.h>
static unsigned char *wDMABuf_va;
static dma_addr_t wDMABuf_pa;
#endif


static DECLARE_WAIT_QUEUE_HEAD(finger_waiter);

static tp_debug_level ts_debug_level = 0;
#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

#define TPD_ERR(a, arg...)  pr_err(TPD_DEVICE ":[Error %d] " a, __LINE__, ##arg)
#define TPD_INFO(a, arg...)  pr_err(TPD_DEVICE ":[Info %d] " a, __LINE__, ##arg)

#define TPD_DEBUG(a, arg...) \
	do{\
		if (LEVEL_DEBUG == ts_debug_level)\
			TPD_INFO(a, ##arg);\
	}while(0)

#define TPD_DETAIL(a, arg...)\
	do{\
		if (LEVEL_BASIC != ts_debug_level)\
			TPD_INFO(a, ##arg);\
	}while(0)

#define TPD_SPECIFIC_PRINT(count, a, arg...)\
	do{\
        if (count++ == TPD_PRINT_POINT_NUM || LEVEL_DEBUG == ts_debug_level) {\
            TPD_INFO(a, ##arg);\
            count = 0;\
        }\
	}while(0)


#define RETURN_ON_FAIL(func, ret, args...) \
	do{					\
		ret = func;    \
		if(ret < 0) {  \
			TPD_ERR(":%s %d", ##args, ret); \
			return ret; \
		}				\
	}while(0)

#define RETURN_ON_NULL(func, ret, fmt, args...) \
	do{	\
		ret = func; \
		if (ret == NULL) { \
			TPD_ERR(fmt, ##args);	\
			return -ENOMEM;	\
		}\
	}while(0)

#define GOTO_ON_FAIL(func, end, args...) \
	if(func < 0) {  \
		TPD_ERR(":%s", ##args); \
		goto end; \
	}

#define PRINTK_ON_FAIL(func, ret, args...) \
	do{					\
		ret = func;    \
		if(ret < 0) {  \
			TPD_ERR(":%s %d", ##args, ret); \
		}				\
	}while(0)

static struct synaptics_rmi_hw_info hw_res;

DEFINE_MUTEX(rmi4_report_mutex);
static struct device *g_dev = NULL;

// for 0D button
static unsigned int cap_button_codes[] = TPD_0D_BUTTON;
static struct synaptics_dsx_cap_button_map cap_button_map = {
	.nbuttons = ARRAY_SIZE(cap_button_codes),
	.map = cap_button_codes,
};

#ifdef CONFIG_OF_TOUCH
static unsigned int touch_irq = 0;
u8 tpd_intr_type = 0;
unsigned int tpd_intr_pin = 0;
#endif

static int lcm;
static char lcm_name[64];

//extern int primary_display_register_tp_gesture_cb(int (*f)(void *), void *data);
//extern int primary_display_register_tp_wakeup_cb(void (*f)(int, void *), void *data);
static int synaptics_rmi4_finger_proctect_data_get(struct synaptics_rmi4_data *rmi4_data);
static void synaptics_rmi4_wakeup_gesture(struct synaptics_rmi4_data *rmi4_data, bool enable);
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data, bool enable);
static void synaptics_rmi4_sleep_enable(struct synaptics_rmi4_data *rmi4_data, bool enable);
static void synaptics_rmi4_get_config_id(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_set_esd_mode(struct synaptics_rmi4_data *rmi4_data);
//static int synaptics_rmi4_soft_reset(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_irq_enable_soft(struct synaptics_rmi4_data *rmi4_data, bool enable);
static int synaptics_rmi4_irq_enable_safety(struct synaptics_rmi4_data *rmi4_data, bool enable);
static void synaptics_rmi4_speedup_resume(struct work_struct *work);


static int of_get_synaptic_platform_data(struct i2c_client *client, struct synaptics_rmi_hw_info *hw_res)
{
	if (client->dev.of_node) {
		of_property_read_u32_index(client->dev.of_node, "rst-gpio", 0, &hw_res->rst_gpio_number);
		of_property_read_u32_index(client->dev.of_node, "int-gpio", 0, &hw_res->int_gpio_number);
		of_property_read_u32_index(client->dev.of_node, "id1-gpio", 0, &hw_res->id1_gpio_number);
	} else {
		TPD_ERR("No device found\n");
		return -ENODEV;
	}

	TPD_INFO("rst %d int %d, id1 %d\n", hw_res->rst_gpio_number, hw_res->int_gpio_number, hw_res->id1_gpio_number);
	return 0;
}

#ifdef CONFIG_OF_TOUCH
static irqreturn_t tpd_eint_handler_fn(int irq, void *dev_id);
#else
static void tpd_eint_handler(void);
#endif

static int touch_event_handler(void *data);

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_f12_set_enables(struct synaptics_rmi4_data *rmi4_data,
		unsigned short ctrl28);

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data);


static void synaptics_rmi4_suspend(struct device *dev);

static void synaptics_rmi4_resume(struct device *dev);

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
		};
		unsigned char data[5];
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
		};
		unsigned char data[3];
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
			unsigned char obj_type_enable;
			unsigned char max_reported_objects;
		};
		unsigned char data[2];
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
			unsigned char reserved:5;
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
	unsigned int max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned int *button_map;
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

static struct device_attribute attrs[] = {
	__ATTR(reset, 0660,
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
	__ATTR(0dbutton, 0660,
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
	__ATTR(suspend, 0660,
			synaptics_rmi4_show_error,
			synaptics_rmi4_suspend_store),
};

struct synaptics_rmi4_proc_info {
	char name[64];
	struct file_operations operator;
};

#define __DEBUG_INFO(_name) {		\
	.name = #_name,					\
	.operator = {					\
		.owner = THIS_MODULE,		\
		.open  = synaptics_rmi4_##_name##_open,	\
		.read  = seq_read,			\
		.release = single_release,	\
	}	\
}

#define __FUNC_INFO(_name) {	\
	.name = #_name,		\
	.operator = {		\
		.owner = THIS_MODULE,	\
		.open  = simple_open,	\
		.read  = synaptics_rmi4_##_name##_read,	\
		.write = synaptics_rmi4_##_name##_write,\
	}	\
}

#define __DEBUG_INFO_OPEN(_name) \
	static int synaptics_rmi4_##_name##_open(struct inode *inode, struct file *file) {	\
		return single_open(file, synaptics_rmi4_##_name##_read, PDE_DATA(inode));		\
	}

static struct synaptics_rmi4_exp_fhandler* synaptics_rmi4_handler(enum exp_fn fn) {
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if(exp_fhandler->exp_fn->fn_type == fn) {
				mutex_unlock(&exp_data.mutex);
				return exp_fhandler;
			}
		}
	}
	mutex_unlock(&exp_data.mutex);
	return NULL;
}


static int synaptics_rmi4_limit_read(struct seq_file *s, void *v) {
	int ret = 0;
	int rx, tx;
	int i = 0, j = 0;
	int count = 0;
	int do_twice = 2;
	int limit_size = 0;
	const struct firmware *fw = NULL;
	struct limit_header *ph;
	int16_t *limit_data16;
	struct synaptics_rmi4_data *rmi4_data = s->private;

	if (rmi4_data->config_id < 0xDB091A07) {
		snprintf(rmi4_data->limit_path, MAX_NAME_LEN, "tp/16091_Limit_TD4322_A04_%s.img", rmi4_data->lcm_name);
		TPD_INFO("The previous limit param, use the limit before\n");
	}

	ret = request_firmware(&fw, rmi4_data->limit_path, &rmi4_data->i2c_client->dev);
	if (ret < 0) {
		seq_printf(s, "Failed to get limit imge file %s\n", rmi4_data->limit_path);
        return ret;
	}

	rx = rmi4_data->num_of_rx;
	tx = rmi4_data->num_of_tx;

	ph = (struct limit_header *)fw->data;

	TPD_DEBUG("%d %d %d %d %d %d %d\n", ph->magic1, ph->magic2, ph->withCBC, 
			ph->array_limit_size, ph->array_limit_offset, ph->array_limitcbc_size, ph->array_limitcbc_offset);

	seq_printf(s, "tx = %d\nrx = %d\n", tx, rx);
	while (do_twice > 0) {
		count = 0;
		if (do_twice == 2) {
			limit_data16 = (int16_t *)(fw->data + ph->array_limit_offset);
			limit_size = ph->array_limit_size;
			seq_printf(s, "Without CBC:\n");
		} else if (do_twice == 1) {
			limit_data16 = (int16_t *)(fw->data + ph->array_limitcbc_offset);
			limit_size = ph->array_limitcbc_size;
			if (!ph->withCBC)
				break;
			seq_printf(s, "With CBC:\n");
		}

		TPD_INFO("limit size %d\n", limit_size);
		for (i = 0; i < tx + 1; i++) {
			if (count >= limit_size)
				break;
			for( j = 0; j < rx; j++) {
				if (count >= limit_size)
					break;
				seq_printf(s, "%-5d%-5d", *limit_data16, *(limit_data16 + 1));
				count += 4;
				limit_data16 += 2;
			}
			seq_printf(s, "\n");
		}
		do_twice--;
		seq_printf(s, "\n");
	}

	release_firmware(fw);
	return 0;
}

static int synaptics_rmi4_delta_read(struct seq_file *s, void *v) {
	char *result = NULL;
	ssize_t count_size = 0;
	//int ret = 0;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler = synaptics_rmi4_handler(RMI_TEST_REPORTING);
	struct synaptics_rmi4_data *rmi4_data;

	rmi4_data= s->private;
	TPD_INFO("%s:Start\n",__func__);
	
    disable_irq_nosync(rmi4_data->irq);
	mutex_lock(&rmi4_data->rmi4_func_mutex);
	if ((exp_fhandler == NULL) || (exp_fhandler->exp_fn->invoke == NULL)) {
		TPD_ERR("Failed to get handler\n");
		goto Fail;
	}

	result = kzalloc(PAGE_SIZE, GFP_KERNEL);
	
	if (result == NULL) {
		TPD_ERR("Failed to alloc memory\n");
		goto Fail;
	}

	count_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_DELTA_SHOW, result, NULL);
	seq_printf(s, "%s", result);

	kfree(result);
	TPD_INFO("%s:End\n",__func__);

Fail:
	mutex_unlock(&rmi4_data->rmi4_func_mutex);
    enable_irq(rmi4_data->irq);
	return 0;
}

static int synaptics_rmi4_self_delta_read(struct seq_file *s, void *v) {
	return 0;
}

static int synaptics_rmi4_self_raw_read(struct seq_file *s, void *v) {
	return 0;
}

static int synaptics_rmi4_baseline_read(struct seq_file *s, void *v) {
	char *result = NULL;
	ssize_t count_size = 0;
	//int ret = 0;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler = synaptics_rmi4_handler(RMI_TEST_REPORTING);
	struct synaptics_rmi4_data *rmi4_data;

	rmi4_data= s->private;

	TPD_INFO("%s:Start\n",__func__);
	
    disable_irq_nosync(rmi4_data->irq);
	mutex_lock(&rmi4_data->rmi4_func_mutex);
	if ((exp_fhandler == NULL) || (exp_fhandler->exp_fn->invoke == NULL)) {
		TPD_ERR("Failed to get handler\n");
		goto Fail;
	}

	result = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (result == NULL) {
		TPD_ERR("Failed to alloc memory\n");
		goto Fail;
	}

	exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_FULL_RAW_STORE, "1", NULL);
	count_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_FULL_RAW_SHOW, result, NULL);

	seq_printf(s, "%s", result);

	kfree(result);
	TPD_INFO("%s:End\n",__func__);

Fail:	
	mutex_unlock(&rmi4_data->rmi4_func_mutex);
    enable_irq(rmi4_data->irq);
	return 0;
}

static int synaptics_rmi4_main_register_read(struct seq_file *s, void *v) {
	return 0;
}

static int synaptics_rmi4_reserve_read(struct seq_file *s, void *v) {
	seq_printf(s, "synpatics reserved\n");
	return 0;
}

static int tp_request_limit(struct synaptics_rmi4_data *rmi4_data, struct synaptics_rmi4_limit *params) {
	struct timespec now_time;
	struct rtc_time rtc_now_time;
	uint8_t path[64];
	mm_segment_t old_fs;
	int fd = -1;
	int ret = 0;
	const struct firmware *fw = NULL;

	//step1: get time and path
	getnstimeofday(&now_time);
    rtc_time_to_tm(now_time.tv_sec, &rtc_now_time);

    snprintf(path, 64, "/sdcard/tp_testlimit_"SENSOR_DEVICE"_%02d%02d%02d-%02d%02d%02d-utc.csv",
            (rtc_now_time.tm_year + 1900) % 100, rtc_now_time.tm_mon + 1, rtc_now_time.tm_mday,
            rtc_now_time.tm_hour, rtc_now_time.tm_min, rtc_now_time.tm_sec);
	
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    fd = sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        TPD_INFO("Open file '%s' failed.\n", path);
        set_fs(old_fs);
		return -EOPENSTALE;
    }

	if (rmi4_data->config_id  < 0xDB091A07) {
		snprintf(rmi4_data->limit_path, MAX_NAME_LEN, "tp/16091_Limit_TD4322_A04_%s.img", rmi4_data->lcm_name);
		TPD_INFO("The previous limit param, use the limit before\n");
	}

	ret = request_firmware(&fw, rmi4_data->limit_path, &rmi4_data->i2c_client->dev);
	if (ret < 0) {
		TPD_ERR("Request limit image %s failed\n", rmi4_data->limit_path);
		sys_close(fd);
		return -EFAULT;
	}

	params->fd = fd;
	params->fw = fw;
	params->header = (struct limit_header *)fw->data;

	return ret;
}

static int synaptics_rmi4_baseline_test_read(struct seq_file *s, void *v) {
	char *result = NULL;
	ssize_t count_size = 0;
	size_t index = 0;
	int ret = 0;
	struct synaptics_rmi4_limit limit_fw;
	const struct firmware *fw;
	struct limit_header *header;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler = synaptics_rmi4_handler(RMI_TEST_REPORTING);
	struct synaptics_rmi4_data *rmi4_data = s->private;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler_fw = synaptics_rmi4_handler(RMI_FW_UPDATER);


	exp_fhandler_fw->exp_fn->invoke(rmi4_data, RMI_UPDATE_FIRMWARE, "1", NULL);
	synaptics_rmi4_get_config_id(rmi4_data);
/*
	if (rmi4_data->rmi4_mod_info.manufacture_info.version) {
		sprintf(rmi4_data->rmi4_mod_info.manufacture_info.version, "0x%x", rmi4_data->config_id);
	}
*/
	if ((exp_fhandler == NULL) || (exp_fhandler->exp_fn->invoke == NULL))
		return 0;

	TPD_INFO("%s:Start\n",__func__);
	result = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (result == NULL) {
		return count_size;
	}

	//step1: check base line
	index += count_size;
	ret = tp_request_limit(rmi4_data, &limit_fw);
	if (ret < 0) {
		index += snprintf(result, PAGE_SIZE, "ERROR:request limit image failed\n");
		goto Failed;
	}

	fw = limit_fw.fw;
	header = limit_fw.header;
	TPD_DEBUG("%d %d %d %d %d %d %d\n", header->magic1, header->magic2, header->withCBC, 
		header->array_limit_size, header->array_limit_offset, header->array_limitcbc_size, header->array_limitcbc_offset);

	exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_FULL_RAW_STORE, "1", NULL);
	count_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_FULL_RAW_TEST, result + index, &limit_fw);
	if (strncmp(result + index, "PASS", 4) != 0) {
		release_firmware(limit_fw.fw);
		sys_close(limit_fw.fd);
		goto Failed;
	}

	release_firmware(limit_fw.fw);
	sys_close(limit_fw.fd);

	//step2: check electrode open
	index += count_size;
	exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_ELECTORDE_OPEN_STORE, "1", NULL);
	count_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_ELECTORDE_OPEN_SHOW, result + index, NULL);
	if (strncmp(result + index, "PASS", 4) != 0)
		goto Failed;


	//step3: check electrod short
	index += count_size;
	exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_EXTEND_EE_SHORT_STORE, "1", NULL);
	count_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_EXTEND_EE_SHORT_SHOW, result + index, NULL);

	if (strncmp(result + index, "PASS", 4) != 0)
		goto Failed;

	index += count_size;

	index += snprintf(result + index, PAGE_SIZE, "0");

	goto Exit;

Failed:
	index += count_size;
	index += snprintf(result + index, PAGE_SIZE, "1");

Exit:
	seq_printf(s, result);

	synaptics_rmi4_reset_device(rmi4_data);

	kfree(result);
	TPD_INFO("%s:End\n",__func__);
	return 0;
}

static int  synaptics_rmi4_coordinate_read(struct seq_file *s, void *v) {
	int ret = 0;
	struct synaptics_rmi4_data *rmi4_data = s->private;
	char page[PAGE_SIZE/4];
	ret = sprintf(page, "%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d\n", rmi4_data->coord.gesture,
			rmi4_data->coord.Point_start.x, rmi4_data->coord.Point_start.y, rmi4_data->coord.Point_end.x, rmi4_data->coord.Point_end.y,
			rmi4_data->coord.Point_1st.x, rmi4_data->coord.Point_1st.y, rmi4_data->coord.Point_2nd.x, rmi4_data->coord.Point_2nd.y,
			rmi4_data->coord.Point_3rd.x, rmi4_data->coord.Point_3rd.y, rmi4_data->coord.Point_4th.x, rmi4_data->coord.Point_4th.y,
			rmi4_data->coord.clockwise);
	seq_printf(s, page);
	return ret;
}


static ssize_t synaptics_rmi4_debug_level_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	unsigned int input;
	char buf[PAGE_SIZE/8];

	if(copy_from_user(buf, buffer, count)){
		return count;
	}

	if (sscanf(buf, "%u", &input) == 1) {
		if (input < LEVEL_MAX) {
			ts_debug_level = input;	
			TPD_INFO("Set debug level to %d\n", input);
			return count;
		} else {
			TPD_ERR("Invalid debug level\n");
		}
	}
	return 0;
}

static ssize_t synaptics_rmi4_debug_level_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	char buf[PAGE_SIZE/8];
	if (snprintf(buf, PAGE_SIZE/8, "%u\n", ts_debug_level) < 0)
		return 0;
	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
}

static ssize_t synaptics_rmi4_oppo_tp_fw_update_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	ssize_t ret_size = 0;
	int retval = 0, input;
	unsigned int firmware_id_temp;
	char k_buf[PAGE_SIZE/16];
	struct synaptics_rmi4_exp_fhandler *exp_fhandler = synaptics_rmi4_handler(RMI_FW_UPDATER);
	struct synaptics_rmi4_data *rmi4_data = PDE_DATA(file_inode(file));

	if ((exp_fhandler == NULL) || (exp_fhandler->exp_fn->invoke == NULL)) {
		TPD_ERR("Update service not init success, can not update firmware\n");
		return 0;
	}

	if (rmi4_data->is_suspended)
		return count;

	mutex_lock(&rmi4_data->rmi4_func_mutex);
	rmi4_data->loading_fw = true;
	synaptics_rmi4_get_config_id(rmi4_data);
	TPD_INFO("Start to Firmware Update. Before: 0x%x\n", rmi4_data->config_id);
	firmware_id_temp = rmi4_data->firmware_id;

	if(copy_from_user(k_buf, buffer, count)){
		mutex_unlock(&rmi4_data->rmi4_func_mutex);
		TPD_ERR("%s: read fw_update input error.\n", __func__);
		rmi4_data->loading_fw = false;
		return count;
	}

	retval = sscanf(k_buf, "%d", &input);
	if (input == 0)
		ret_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_UPDATE_FIRMWARE, "1", NULL);
	else if (input == 1)
		ret_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_UPDATE_FIRMWARE, "2", NULL);
	else 
		ret_size = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_UPDATE_FIRMWARE, "3", NULL);

	if (firmware_id_temp == 0 && rmi4_data->spurious_fp_support) {
		PRINTK_ON_FAIL(synaptics_rmi4_finger_proctect_data_get(rmi4_data), retval, "get finger protect data failed\n");
	}

	synaptics_rmi4_get_config_id(rmi4_data);
/*
	if (rmi4_data->rmi4_mod_info.manufacture_info.version) {
		sprintf(rmi4_data->rmi4_mod_info.manufacture_info.version, "0x%x", rmi4_data->config_id);
	}
*/
	rmi4_data->loading_fw = false;
	mutex_unlock(&rmi4_data->rmi4_func_mutex);
	TPD_INFO("End to Firmware Update. After: 0x%x\n", rmi4_data->config_id);
	return count;
}

static ssize_t synaptics_rmi4_oppo_tp_fw_update_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	return 0;
}

static ssize_t synaptics_rmi4_oppo_tp_limit_area_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	return 0;
}

static ssize_t synaptics_rmi4_oppo_tp_limit_area_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	return 0;
}

static ssize_t synaptics_rmi4_oppo_tp_limit_enable_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	return 0;
}

static ssize_t synaptics_rmi4_oppo_tp_limit_enable_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	return 0;
}

static int synaptics_rmi4_gesture_switch(struct synaptics_rmi4_data *rmi4_data, int onoff) {
	if (onoff == 0 || onoff == 2) {
		TPD_INFO("%s Disable gesture right now.\n", __func__);
		//synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		synaptics_rmi4_sleep_enable(rmi4_data, true);
	} else if (onoff == 1) {
		TPD_INFO("%s Enable gesture right now.\n", __func__);
		synaptics_rmi4_sleep_enable(rmi4_data, false);
		//synaptics_rmi4_wakeup_gesture(rmi4_data, true);
		/*enable gpio wake system through intterrupt*/
		enable_irq_wake(rmi4_data->irq);
	} else {
		TPD_ERR("Error gesture state\n");
	}

	return 0;
}

static ssize_t synaptics_rmi4_double_tap_enable_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = PDE_DATA(file_inode(file));

	if (sscanf(buffer, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&rmi4_data->rmi4_func_mutex);
	if (rmi4_data->enable_wakeup_gesture == input) {
		TPD_ERR("Same state, no need to process\n");
		mutex_unlock(&rmi4_data->rmi4_func_mutex);
		return -EEXIST;
	}

	TPD_INFO("%s Input:%d Sleep State:%d Gesture State:%d\n", __func__, input, rmi4_data->is_suspended, rmi4_data->enable_wakeup_gesture);

	if (rmi4_data->f11_wakeup_gesture || rmi4_data->f12_wakeup_gesture)
		rmi4_data->enable_wakeup_gesture = input;

	if (rmi4_data->is_suspended) {
		synaptics_rmi4_gesture_switch(rmi4_data, rmi4_data->enable_wakeup_gesture);
	}

	mutex_unlock(&rmi4_data->rmi4_func_mutex);

	return count;
}

static ssize_t synaptics_rmi4_double_tap_enable_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	int ret = 0;
	char page[PAGE_SIZE/4];
	struct synaptics_rmi4_data *rmi4_data = PDE_DATA(file_inode(file));

	TPD_DEBUG("%s gesture enable is: %d\n", __func__, rmi4_data->enable_wakeup_gesture);
	ret = sprintf(page, "%d\n", rmi4_data->enable_wakeup_gesture);
	ret = simple_read_from_buffer(user_buf, count, ppos, page, strlen(page));
	return ret;
}

static ssize_t synaptics_rmi4_finger_protect_result_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	return 0;
}

static ssize_t synaptics_rmi4_finger_protect_result_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	int ret = 0;
	char page[PAGE_SIZE/4];
	struct synaptics_rmi4_data *rmi4_data = PDE_DATA(file_inode(file));
	ret = sprintf(page, "%d\n", rmi4_data->spuri_fp_touch.fp_touch_st);
    ret = simple_read_from_buffer(user_buf, count, ppos, page, strlen(page));
    return ret;
}


static ssize_t synaptics_rmi4_finger_protect_trigger_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
	int op = 0;
	struct synaptics_rmi4_data *rmi4_data = PDE_DATA(file_inode(file));

	if (count > 2)
		return -EINVAL;

	if (1 == sscanf(buffer, "%d", &op)) {
        if (op == 1) {
            rmi4_data->spuri_fp_touch.fp_trigger= true;
            rmi4_data->spuri_fp_touch.fp_touch_st = FINGER_PROTECT_NOTREADY;
            wake_up_interruptible(&finger_waiter);
        }
	} else {
		TPD_ERR("%s Invalid input %d\n", __func__, op);
	}

	TPD_DEBUG("%s: input trigger %d\n", __func__, op);
	return count;
}

static ssize_t synaptics_rmi4_finger_protect_trigger_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
	int ret = 0;
	char page[PAGE_SIZE/4];
	struct synaptics_rmi4_data *rmi4_data = PDE_DATA(file_inode(file));
	ret = sprintf(page, "%d\n", rmi4_data->spuri_fp_touch.fp_trigger);
    ret = simple_read_from_buffer(user_buf, count, ppos, page, strlen(page));
    return ret;
}


__DEBUG_INFO_OPEN(limit)
__DEBUG_INFO_OPEN(delta)
__DEBUG_INFO_OPEN(self_delta)
__DEBUG_INFO_OPEN(self_raw)
__DEBUG_INFO_OPEN(baseline)
__DEBUG_INFO_OPEN(main_register)
__DEBUG_INFO_OPEN(reserve)
__DEBUG_INFO_OPEN(baseline_test)
__DEBUG_INFO_OPEN(coordinate)


#define MAX_PROC_FUNC_SIZE 9
#define MAX_PROC_DEBUG_SIZE 7

static struct synaptics_rmi4_proc_info func_operators[MAX_PROC_FUNC_SIZE] = {
	__DEBUG_INFO(baseline_test),
	__DEBUG_INFO(coordinate),
	__FUNC_INFO(debug_level),
	__FUNC_INFO(oppo_tp_fw_update),
	__FUNC_INFO(oppo_tp_limit_area),
	__FUNC_INFO(oppo_tp_limit_enable),
	__FUNC_INFO(double_tap_enable),
	__FUNC_INFO(finger_protect_result),
	__FUNC_INFO(finger_protect_trigger),
};

static struct synaptics_rmi4_proc_info debug_operators[MAX_PROC_DEBUG_SIZE] = {
	__DEBUG_INFO(limit),
	__DEBUG_INFO(delta),
	__DEBUG_INFO(self_delta),
	__DEBUG_INFO(self_raw),
	__DEBUG_INFO(baseline),
	__DEBUG_INFO(main_register),
	__DEBUG_INFO(reserve),
};


#ifdef CONFIG_OF_TOUCH
static int tpd_irq_registration(struct synaptics_rmi4_data *rmi4_data)
{
	int ret = 0;
#if 0
	struct device_node *node = NULL;
	u32 ints[2] = { 0, 0 };
	u32 intr[2] = {0, 0};

	node = of_find_compatible_node(NULL, NULL, "mediatek, TOUCH_PANEL-eint");
	if (node) {
		if (!of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints)))
			gpio_set_debounce(ints[0], ints[1]);

		if (!of_property_read_u32_array(node , "interrupts", intr, ARRAY_SIZE(intr))) {
			touch_irq = irq_of_parse_and_map(node, 0);
			rmi4_data->irq = touch_irq;
		} else {
			printk("fatal: get interrupts failed\n!.");
			return -1;
		}

		rmi4_data->irq_flags = IRQF_TRIGGER_LOW;

		ret = request_threaded_irq(touch_irq, NULL,
			tpd_eint_handler_fn,
			rmi4_data->irq_flags | IRQF_ONESHOT, 
			"TOUCH_TD4322-eint", rmi4_data);

		if (ret < 0) {
			printk("tpd request_irq IRQ LINE NOT AVAILABLE!.");
			return -1;
		}
	} else {
		printk("tpd request_irq can not find touch eint device node!.");
		return -1;
	}
#else
	touch_irq = rmi4_data->i2c_client->irq;
	rmi4_data->irq = touch_irq;
	rmi4_data->irq_flags = IRQF_TRIGGER_FALLING;
	TPD_INFO("touch irq %d\n", touch_irq);
	ret = request_threaded_irq(touch_irq, NULL,
		tpd_eint_handler_fn,
		rmi4_data->irq_flags | IRQF_ONESHOT, 
		"TOUCH_TD4322-eint", rmi4_data);

	if (ret < 0) {
		printk("tpd request_irq IRQ LINE NOT AVAILABLE!.");
		return -EFAULT;
	}	
#endif
	return ret;
}
#endif


void synaptics_gpio_as_int(int pin, struct synaptics_rmi4_data *rmi4_data)
{
	mutex_lock(&(rmi4_data->rmi4_gpio_mutex));
	TPD_DEBUG("%s\n", __func__);
	if (!IS_ERR_OR_NULL(rmi4_data->hw_res->syn_pinctrl1)) {
		if (pin == 1)
			pinctrl_select_state(rmi4_data->hw_res->syn_pinctrl1, rmi4_data->hw_res->syn_eint_as_int);
	}
	mutex_unlock(&(rmi4_data->rmi4_gpio_mutex));
}

void synatpics_gpio_output(int pin, int level, struct synaptics_rmi4_data *rmi4_data)
{
	mutex_lock(&(rmi4_data->rmi4_gpio_mutex));
	TPD_DEBUG("%s pin = %d, level = %d\n", __func__, pin, level);

	if (!IS_ERR_OR_NULL(rmi4_data->hw_res->syn_pinctrl1)) {
		if (pin == 1) {
			if (level)
				pinctrl_select_state(rmi4_data->hw_res->syn_pinctrl1, rmi4_data->hw_res->syn_eint_as_int);
			else
				pinctrl_select_state(rmi4_data->hw_res->syn_pinctrl1, rmi4_data->hw_res->syn_eint_as_input);

			} else {
			if (level)
				pinctrl_select_state(rmi4_data->hw_res->syn_pinctrl1, rmi4_data->hw_res->syn_rst_on);
			else
				pinctrl_select_state(rmi4_data->hw_res->syn_pinctrl1, rmi4_data->hw_res->syn_rst_off);
		}
	}
	mutex_unlock(&(rmi4_data->rmi4_gpio_mutex));
}

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1 && reset != 2)
		return -EINVAL;

	if (reset == 2) {
		TPD_INFO("set esd mode\n");
		synaptics_rmi4_set_esd_mode(rmi4_data);
		return count;
	}

	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0) {
		dev_err(dev,
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

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(dev,
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

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;

			if (input == 1)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_i2c_write(rmi4_data,
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

 /**
 * synaptics_rmi4_set_page()
 *
 * Called by synaptics_rmi4_i2c_read() and synaptics_rmi4_i2c_write().
 *
 * This function writes to the page select register to switch to the
 * assigned page.
 */
 /*
static int synaptics_rmi4_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr)
{
	int retval = 0;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = rmi4_data->i2c_client;

	page = ((address >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				dev_err(&i2c->dev,
						"%s: I2C retry %d\n",
						__func__, retry + 1);
				msleep(20);
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return (retval == PAGE_SELECT_LEN) ? retval : -EIO;
}
*/


static int tpd_set_page(struct synaptics_rmi4_data *rmi4_data,
			unsigned short addr)
{
	int retval = 0;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = rmi4_data->i2c_client;

	page = ((addr >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				dev_err(&rmi4_data->i2c_client->dev, 
					"%s: I2C retry %d\n", __func__, retry + 1);
				msleep(20);
				if (retry == (SYN_I2C_RETRY_TIMES / 2)) {
					if (i2c->addr == rmi4_data->i2c_addr)
						i2c->addr = UBL_I2C_ADDR;
					else
						i2c->addr = rmi4_data->i2c_addr;
				}
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}


int tpd_i2c_read_data(struct synaptics_rmi4_data *rmi4_data,struct i2c_client *client,
			   unsigned short addr, unsigned char *data, unsigned short length)
 {

	unsigned char retry = 0;
	unsigned char *pData = data;
	unsigned char tmp_addr = (unsigned char)addr;
	int retval = 0;
	int left_len = length;

	/* u16 old_flag; */
	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	retval = tpd_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN){
		retval = -EIO;		
		goto exit;
	}

	retval = i2c_master_send(client, &tmp_addr, 1);
	while (left_len > 0) {
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			if (left_len > 8)
				retval = i2c_master_recv(client, pData, 8);
			else
				retval = i2c_master_recv(client, pData, left_len);
			if (retval <= 0) {
				dev_err(&client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
				msleep(20);
				if (retry == (SYN_I2C_RETRY_TIMES / 2)) {
					if (client->addr == rmi4_data->i2c_addr)
						client->addr = UBL_I2C_ADDR;
					else
						client->addr = rmi4_data->i2c_addr;
				}
				left_len = length;
				pData = data;
				retval = i2c_master_send(client, &tmp_addr, 1);
				continue;
			} else {
				break;
			}
		}
		if (retry == SYN_I2C_RETRY_TIMES) {
			retval = -EIO;
			goto exit;
		}
		left_len -= 8;
		pData += 8;
	}
exit:
	 mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	 return retval;
 }
EXPORT_SYMBOL(tpd_i2c_read_data);

#ifdef USE_I2C_DMA
int tpd_i2c_write_data_dma(struct synaptics_rmi4_data *rmi4_data,struct i2c_client *client,
				unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval = 0;
	unsigned char retry;
	unsigned char *buf_va = NULL;
	struct i2c_msg msg[1];

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	msg[0].addr = rmi4_data->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = length + 1;
	msg[0].ext_flag = (rmi4_data->i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG ),
	msg[0].buf = (unsigned char *)(uintptr_t)wDMABuf_pa;
	msg[0].timing = 400;
	
	retval = tpd_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN){
		retval = -EIO;	
		goto exit;
	}

	buf_va = wDMABuf_va;
	buf_va[0] = addr & MASK_8BIT;
	memcpy(&buf_va[1], &data[0], length);

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);

		if (retry == (SYN_I2C_RETRY_TIMES / 2)) {
			if (rmi4_data->i2c_client->addr == rmi4_data->i2c_addr)
				rmi4_data->i2c_client->addr = UBL_I2C_ADDR;
			else
				rmi4_data->i2c_client->addr = rmi4_data->i2c_addr;
			msg[0].addr = rmi4_data->i2c_client->addr;
		}
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C write over retry limit\n",
				__func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}
EXPORT_SYMBOL(tpd_i2c_write_data_dma);

#else
int tpd_i2c_write_data(struct synaptics_rmi4_data *rmi4_data,struct i2c_client *client,
				unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval = 0;
	u8 retry = 0;
	u8 *buf;
	int tmp_addr = addr;


	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	retval = tpd_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		TPD_ERR("tpd_set_page fail, retval = %d\n", retval);
		retval = -EIO;
		goto exit;
	}

	buf = kzalloc(sizeof(unsigned char) * (length + 1), GFP_KERNEL);
	*buf = tmp_addr;
	memcpy(buf + 1 , data, length);
	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		retval = i2c_master_send(client, buf, (length + 1));
		if (retval <= 0) {
			dev_err(&client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
			msleep(20);
			continue;
		} else {
			break;
		}
	}
	kfree(buf);

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
 }
EXPORT_SYMBOL(tpd_i2c_write_data);

#endif

 /**
 * synaptics_rmi4_i2c_read()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	return tpd_i2c_read_data(rmi4_data,rmi4_data->i2c_client, addr, data, length);
}

 /**
 * synaptics_rmi4_i2c_write()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function writes data of an arbitrary length to the sensor,
 * starting from an assigned register address of the sensor, via I2C with
 * a retry mechanism.
 */
static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
#ifdef USE_I2C_DMA
	return tpd_i2c_write_data_dma(rmi4_data,rmi4_data->i2c_client, addr, data, length);
#else
	return tpd_i2c_write_data(rmi4_data,rmi4_data->i2c_client, addr, data, length);
#endif
}

 /**
 * synaptics_rmi4_f11_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $11
 * finger data has been detected.
 *
 * This function reads the Function $11 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
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
	//int temp;
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

	if (rmi4_data->is_suspended && rmi4_data->enable_wakeup_gesture) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				data_addr + extra_data->data38_offset,
				&detected_gestures,
				sizeof(detected_gestures));
		if (retval < 0)
			return 0;

		if (detected_gestures) {
			input_report_key(rmi4_data->input_dev, KEY_POWER, 1);
			input_sync(rmi4_data->input_dev);
			input_report_key(rmi4_data->input_dev, KEY_POWER, 0);
			input_sync(rmi4_data->input_dev);
			rmi4_data->is_suspended = false;
		}

		return 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0)
		return 0;
	mutex_lock(&rmi4_report_mutex);
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
			retval = synaptics_rmi4_i2c_read(rmi4_data,
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

#ifdef TPD_HAVE_BUTTON
			if (NORMAL_BOOT != rmi4_data->hw_res->boot_mode)
			{
				tpd_button(x, y, 1);
			}
#endif
			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Finger %d:\n"
					"status = 0x%02x\n"
					"x = %d\n"
					"y = %d\n"
					"wx = %d\n"
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
#ifdef TPD_HAVE_BUTTON
		if (NORMAL_BOOT != rmi4_data->hw_res->boot_mode)
		{
			tpd_button(x, y, 0);
		}
#endif
	}

	input_sync(rmi4_data->input_dev);
exit:
	mutex_unlock(&(rmi4_report_mutex));
	return touch_count;
}

static int synaptics_rmi4_f51_coordinate_point(struct synaptics_rmi4_data *rmi4_data) {
	int ret = 0, i = 0;
	unsigned char coordinate_data[25] = {0};
	unsigned short f51_data_base = (0x04 << 8) | 0x00; // gesture data is in f51
	uint16_t trspoint = 0;
	RETURN_ON_FAIL(synaptics_rmi4_i2c_read(rmi4_data, f51_data_base, &coordinate_data[0], sizeof(coordinate_data)), 
		ret, "Coordinate data get failed\n");

	for(i = 0; i< 23; i += 2){
		trspoint = coordinate_data[i] | coordinate_data[i+1] << 8;
		TPD_DEBUG("TP read coordinate_point[%d] = %d\n", i, trspoint);
	}

	TPD_DEBUG("TP read coordinate_poin[24] = 0x%x\n",coordinate_data[24]);
	rmi4_data->coord.Point_start.x = (coordinate_data[0] | (coordinate_data[1] << 8)) * LCD_WIDTH/ (rmi4_data->sensor_max_x);
	rmi4_data->coord.Point_start.y = (coordinate_data[2] | (coordinate_data[3] << 8)) * LCD_HEIGHT/ (1920);
	rmi4_data->coord.Point_end.x   = (coordinate_data[4] | (coordinate_data[5] << 8)) * LCD_WIDTH / (rmi4_data->sensor_max_x);
	rmi4_data->coord.Point_end.y   = (coordinate_data[6] | (coordinate_data[7] << 8)) * LCD_HEIGHT / (1920);
	rmi4_data->coord.Point_1st.x   = (coordinate_data[8] | (coordinate_data[9] << 8)) * LCD_WIDTH / (rmi4_data->sensor_max_x);
	rmi4_data->coord.Point_1st.y   = (coordinate_data[10] | (coordinate_data[11] << 8)) * LCD_HEIGHT / (1920);
	rmi4_data->coord.Point_2nd.x   = (coordinate_data[12] | (coordinate_data[13] << 8)) * LCD_WIDTH / (rmi4_data->sensor_max_x);
	rmi4_data->coord.Point_2nd.y   = (coordinate_data[14] | (coordinate_data[15] << 8)) * LCD_HEIGHT / (1920);
	rmi4_data->coord.Point_3rd.x   = (coordinate_data[16] | (coordinate_data[17] << 8)) * LCD_WIDTH / (rmi4_data->sensor_max_x);
	rmi4_data->coord.Point_3rd.y   = (coordinate_data[18] | (coordinate_data[19] << 8)) * LCD_HEIGHT / (1920);
	rmi4_data->coord.Point_4th.x   = (coordinate_data[20] | (coordinate_data[21] << 8)) * LCD_WIDTH / (rmi4_data->sensor_max_x);
	rmi4_data->coord.Point_4th.y   = (coordinate_data[22] | (coordinate_data[23] << 8)) * LCD_HEIGHT / (1920);

	rmi4_data->coord.clockwise = (coordinate_data[24] & 0x10)?1:(coordinate_data[24] & 0x20)?0:2; // 1--clockwise, 0--anticlockwise, not circle, report 2

//	ret = synaptics_rmi4_set_page(ts->client, 0xff, 0x0);

	return 0;
}
static int synaptics_rmi4_f12_gesture_report(struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_fn *fhandler) {
	int ret = 0, i =0;
	unsigned char swipe;
	unsigned char detected_gestures[F12_GESTURE_DETECTION_LEN]; // byte0 indicate gesture type, byte1~byte4 are gesture paramter
	unsigned short f51_data_base = (0x04 << 8) | 0x00; // gesture data is in f51
	struct synaptics_rmi4_f12_extra_data *extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;

	rmi4_data->coord.gesture = UnkownGestrue;
	RETURN_ON_FAIL(synaptics_rmi4_i2c_read(rmi4_data, fhandler->full_addr.data_base + extra_data->data4_offset, &detected_gestures[0], sizeof(detected_gestures)),
		ret, "Gesture Buffer I2C read failed\n");

	RETURN_ON_FAIL(synaptics_rmi4_i2c_read(rmi4_data, f51_data_base + 0x18, &swipe, sizeof(swipe)),
		ret, "Gesture Swipe I2C read failed\n");

	for(i = 0; i < sizeof(detected_gestures); i++) {
		TPD_DEBUG("Gesture Property 0x%x\n", detected_gestures[i]);
	}

	switch(detected_gestures[0]) {
		case CIRCLE_DETECT:
			rmi4_data->coord.gesture = Circle;
			break;
		case SWIPE_DETECT:
			rmi4_data->coord.gesture =   (swipe == 0x41) ? Left2RightSwip   :
				(swipe == 0x42) ? Right2LeftSwip :
				(swipe == 0x44) ? Up2DownSwip    :
				(swipe == 0x48) ? Down2UpSwip    :
				(swipe == 0x80) ? DouSwip        :
				UnkownGestrue;
			break;
		case DTAP_DETECT:
			rmi4_data->coord.gesture = DouTap;
			break;
		case VEE_DETECT:
			rmi4_data->coord.gesture =   (detected_gestures[2] == 0x01) ? DownVee  :
				(detected_gestures[2] == 0x02) ? UpVee    :
				(detected_gestures[2] == 0x04) ? RightVee :
				(detected_gestures[2] == 0x08) ? LeftVee  :
				UnkownGestrue;
			break;
		case UNICODE_DETECT:
			rmi4_data->coord.gesture =   (detected_gestures[2] == 0x77 && detected_gestures[3] == 0x00) ? Wgestrue :
				(detected_gestures[2] == 0x6d && detected_gestures[3] == 0x00) ? Mgestrue :
				UnkownGestrue;
			break;
		case 0:
			rmi4_data->coord.gesture = UnkownGestrue;
	}

	TPD_INFO("detect %s gesture\n", rmi4_data->coord.gesture == DouTap ? "double tap" :
			rmi4_data->coord.gesture == UpVee ? "up vee" :
			rmi4_data->coord.gesture == DownVee ? "down vee" :
			rmi4_data->coord.gesture == LeftVee ? "(>)" :
			rmi4_data->coord.gesture == RightVee ? "(<)" :
			rmi4_data->coord.gesture == Circle ? "circle" :
			rmi4_data->coord.gesture == DouSwip ? "(||)" :
			rmi4_data->coord.gesture == Left2RightSwip ? "(-->)" :
			rmi4_data->coord.gesture == Right2LeftSwip ? "(<--)" :
			rmi4_data->coord.gesture == Up2DownSwip ? "up to down |" :
			rmi4_data->coord.gesture == Down2UpSwip ? "down to up |" :
			rmi4_data->coord.gesture == Mgestrue ? "(M)" :
			rmi4_data->coord.gesture == Wgestrue ? "(W)" : "unknown");

	synaptics_rmi4_f51_coordinate_point(rmi4_data);

	if(rmi4_data->coord.gesture != UnkownGestrue) {
		input_report_key(rmi4_data->input_dev, KEY_F4, 1);
		input_sync(rmi4_data->input_dev);
		input_report_key(rmi4_data->input_dev, KEY_F4, 0);
		input_sync(rmi4_data->input_dev);
	}

	return 0;
}

 /**
 * synaptics_rmi4_f12_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $12
 * finger data has been detected.
 *
 * This function reads the Function $12 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned short data_addr;
	int x, y, wx, wy, temp;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;
	struct synaptics_rmi4_f12_finger_data *finger_data;
#ifdef F12_DATA_15_WORKAROUND
	static unsigned char fingers_already_present;
#endif
	static unsigned int last_major_width = 0;

	//record the last finger position, and the max finger number that processed
	static int x_last = 0, y_last = 0, x_start = 0, y_start = 0;
	static unsigned char finger_num_max = 0;
	static int touch_number = 0;
	static int report_dot_count =0;

	fingers_to_process = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	if (rmi4_data->is_suspended && (rmi4_data->enable_wakeup_gesture == 1)) {
		synaptics_rmi4_f12_gesture_report(rmi4_data, fhandler);
		return 0;
	}

	/* Determine the total number of fingers to process */
	if (extra_data->data15_size) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				data_addr + extra_data->data15_offset,
				extra_data->data15_data,
				extra_data->data15_size);
		if (retval < 0)
			return 0;

		/* Start checking from the highest bit */
		temp = extra_data->data15_size - 1; /* Highest byte */
		finger = (fingers_to_process - 1) % 8; /* Highest bit */
		do {
			if (extra_data->data15_data[temp] & (1 << finger))
				break;

			if (finger) {
				finger--;
			} else {
				temp--; /* Move to the next lower byte */
				finger = 7;
			}

			fingers_to_process--;
		} while (fingers_to_process);

		TPD_DEBUG("%s: Number of fingers to process = %d\n", __func__, fingers_to_process);
	}

#ifdef F12_DATA_15_WORKAROUND
	fingers_to_process = max(fingers_to_process, fingers_already_present);
#endif

	if (!fingers_to_process) {
		synaptics_rmi4_free_fingers(rmi4_data);
		return 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr + extra_data->data1_offset,
			(unsigned char *)fhandler->data,
			fingers_to_process * size_of_2d_data);
	if (retval < 0)
		return 0;

	data = (struct synaptics_rmi4_f12_finger_data *)fhandler->data;
	mutex_lock(&rmi4_report_mutex);
	for (finger = 0; finger < fingers_to_process; finger++) {
		finger_data = data + finger;
		finger_status = finger_data->object_type_and_status & MASK_1BIT;

#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev, MT_TOOL_FINGER, finger_status);
#endif

		if (finger_status) {
#ifdef F12_DATA_15_WORKAROUND
			fingers_already_present = finger + 1;
#endif

			x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
			y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
#ifdef REPORT_2D_W
			wx = finger_data->wx;
			wy = finger_data->wy;
#endif

			last_major_width++;

			input_report_key(rmi4_data->input_dev, BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev, BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev, ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			if (last_major_width == 50) {
				last_major_width = 0;
				input_report_abs(rmi4_data->input_dev, ABS_MT_TOUCH_MAJOR, max(wx, wy) + 1);
				input_report_abs(rmi4_data->input_dev, ABS_MT_TOUCH_MINOR, min(wx, wy) + 1);
			} else {
				input_report_abs(rmi4_data->input_dev, ABS_MT_TOUCH_MAJOR, max(wx, wy));
				input_report_abs(rmi4_data->input_dev, ABS_MT_TOUCH_MINOR, min(wx, wy));
			}
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			TPD_SPECIFIC_PRINT(report_dot_count, "%s: Finger %d:\n"
					"status = 0x%02x x = %d y = %d wx = %d wy = %d\n", __func__, finger, finger_status,	x, y, wx, wy);

			touch_count++;

/*add this only for debug, you can remove if you don't need it.   start*/
			touch_number++;
			if (touch_number == 1) {
				TPD_DETAIL("First Finger Touch x:%d y:%d\n", x, y);
			}

			if (fingers_to_process == 1) {
				if (x_start == 0 && y_start == 0) {
					x_start = x; y_start = y;
				}

				if ((x - x_start) > X_MIN_MOVE_PIXEL || (x_start - x) > X_MIN_MOVE_PIXEL ||
					(y - y_start) > Y_MIN_MOVE_PIXEL || (y_start - y) > Y_MIN_MOVE_PIXEL) {
					TPD_DETAIL("Moved delta %d %d pixel\n", x - x_start, y - y_start);
					x_start = x; y_start = y;
				}
			}

			x_last = x; y_last = y;

			finger_num_max = max(finger_num_max, fingers_to_process);
/****end****/
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev, BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev, BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif

/*add this delow only for debug*/
		TPD_DETAIL("Last Finger Touch x:%d y:%d, max %d fingers processed\n", x_last, y_last, finger_num_max);
		finger_num_max = 0;
		touch_number = 0;
		x_start = y_start = 0;
		x_last = y_last = 0;
		last_major_width = 0;
	}

	input_sync(rmi4_data->input_dev);
	mutex_unlock(&rmi4_report_mutex);
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
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif

	if (rmi4_data->clear_button) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		rmi4_data->clear_button = 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read button data registers\n",
				__func__);
		return;
	}

	data = f1a->button_data_buffer;
	mutex_lock(&rmi4_report_mutex);
	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status)
			continue;
		else
			current_status[button] = status;

		dev_dbg(&rmi4_data->i2c_client->dev,
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
	mutex_unlock(&rmi4_report_mutex);
	return;
}

 /**
 * synaptics_rmi4_report_touch()
 *
 * Called by synaptics_rmi4_sensor_report().
 *
 * This function calls the appropriate finger data reporting function
 * based on the function handler it receives and returns the number of
 * fingers detected.
 */
static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	unsigned char touch_count_2d;

	TPD_DETAIL(": Function %02x reporting\n", fhandler->fn_number);

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

 /**
 * synaptics_rmi4_sensor_report()
 *
 * Called by synaptics_rmi4_irq().
 *
 * This function determines the interrupt source(s) from the sensor
 * and calls synaptics_rmi4_report_touch() with the appropriate
 * function handler for each function with valid data inputs.
 */
static void synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	unsigned char *intr = &data[1];
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			data,
			rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read interrupt status\n",
				__func__);
		return;
	}

	status.data[0] = data[0];

	if (status.unconfigured && !status.flash_prog) {
		TPD_INFO("%s: spontaneous reset detected\n", __func__);
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to reinit device\n",
					__func__);
		}

		if ((rmi4_data->enable_wakeup_gesture == 1 || rmi4_data->enable_wakeup_gesture == 2)
			&& rmi4_data->is_suspended) {
			synaptics_rmi4_wakeup_gesture(rmi4_data, true);
			if (rmi4_data->enable_wakeup_gesture == 2) {
				synaptics_rmi4_sleep_enable(rmi4_data, false);
				TPD_DETAIL("Sensor touched, enter sleep mode\n");
			}
		}

		return;
	}

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

 /**
 * synaptics_rmi4_irq()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
#ifdef CONFIG_OF_TOUCH
static irqreturn_t tpd_eint_handler_fn(int irq, void *dev_id)
{
	touch_event_handler(dev_id);
	return IRQ_HANDLED;
}
#else
static void tpd_eint_handler(void)
{
	TPD_DEBUG_PRINT_INT;
	wake_up_interruptible(&waiter);
}
#endif

static int synaptics_rmi4_irq_enable_safety(struct synaptics_rmi4_data *rmi4_data, bool enable) {
	if (enable) {
		if (!rmi4_data->irq_enabled) {
#ifdef CONFIG_OF_TOUCH
			enable_irq(touch_irq);
#else
			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
			rmi4_data->irq_enabled = true;
			TPD_INFO("enable irq force\n");
		}
	} else {
		if (rmi4_data->irq_enabled) {
#ifdef CONFIG_OF_TOUCH
			disable_irq_nosync(touch_irq);
#else
			mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
			rmi4_data->irq_enabled = false;
			TPD_INFO("disable irq force\n");
		}
	}
	return 0;
}

static int touch_event_handler(void *data)
{
	struct synaptics_rmi4_data *rmi4_data = (struct synaptics_rmi4_data *)data;

	TPD_DEBUG("%s\n", __func__); // print a message that indicate the irq is triggered
	if (!rmi4_data->touch_stopped)
		synaptics_rmi4_sensor_report(rmi4_data);

	return 0;
}


static int synaptics_rmi4_irq_enable_soft(struct synaptics_rmi4_data *rmi4_data,
	bool enable) {
	
	int retval = 0;
	unsigned char intr_status[MAX_INTR_REGISTERS];
	uint8_t abs_interrupt_status = 0; //0x00 disable, 0x7f enable
	if (enable) {
		/* Clear interrupts first */
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr + 1,
				intr_status,
				rmi4_data->num_of_intr_regs);
		if (retval < 0) {
			TPD_ERR("Failed to clear interrupt\n");
			return retval;
		}
		abs_interrupt_status = 0x7f;
	} else {
		abs_interrupt_status = 0x00;
	}

	retval = synaptics_rmi4_i2c_write(rmi4_data,
		rmi4_data->f01_ctrl_base_addr + 1,
		&abs_interrupt_status, 1);
	if (retval < 0) {
		TPD_ERR("Failed to enable/disable device irq 0x%x\n", abs_interrupt_status);
	}

	return retval;
}
 /**
 * synaptics_rmi4_irq_enable()
 *
 * Called by synaptics_rmi4_probe() and the power management functions
 * in this driver and also exported to other expansion Function modules
 * such as rmi_dev.
 *
 * This function handles the enabling and disabling of the attention
 * irq including the setting up of the ISR thread.
 */
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	synaptics_rmi4_irq_enable_soft(rmi4_data, enable);

	synaptics_rmi4_irq_enable_safety(rmi4_data, enable);

	return 0;
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
			ii < ((fd->intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	return;
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

	return 0;
}

static int synaptics_rmi4_f51_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int page_number)
{
	
	rmi4_data->f51_query_base_addr = (page_number << 8)| (fd->query_base_addr);
	rmi4_data->f51_ctrl_base_addr =  (page_number << 8)| (fd->ctrl_base_addr);
	rmi4_data->f51_data_base_addr =  (page_number << 8)| (fd->data_base_addr);
	rmi4_data->f51_cmd_base_addr =   (page_number << 8)| (fd->cmd_base_addr);

	TPD_INFO("ctrl:0x%x query:0x%x data:0x%x cmd:0x%x\n",
		rmi4_data->f51_ctrl_base_addr,
		rmi4_data->f51_query_base_addr,
		rmi4_data->f51_data_base_addr,
		rmi4_data->f51_cmd_base_addr);
	return 0;
}

 /**
 * synaptics_rmi4_f11_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This funtion parses information from the Function 11 registers
 * and determines the number of fingers supported, x and y data ranges,
 * offset to the associated interrupt status register, interrupt bit
 * mask, and gathers finger data acquisition capabilities from the query
 * registers.
 */
static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char offset;
	unsigned char fingers_supported;
	struct synaptics_rmi4_f11_extra_data *extra_data;
	struct synaptics_rmi4_f11_query_0_5 query_0_5;
	struct synaptics_rmi4_f11_query_7_8 query_7_8;
	struct synaptics_rmi4_f11_query_9 query_9;
	struct synaptics_rmi4_f11_query_12 query_12;
	struct synaptics_rmi4_f11_query_27 query_27;
	struct synaptics_rmi4_f11_ctrl_6_9 control_6_9;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fhandle->extra\n",
				__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f11_extra_data *)fhandler->extra;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
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

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + 6,
			control_6_9.data,
			sizeof(control_6_9.data));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x = SENSOR_MAX_X;
	rmi4_data->sensor_max_y = SENSOR_MAX_Y; //control_6_9.sensor_max_y_pos_7_0 |
			//(control_6_9.sensor_max_y_pos_11_8 << 8);

	/* It's recommended to parse max_x and max_y from contrel register, but this does not match MTK's mtk-tpd.c */

#ifdef TPD_HAVE_BUTTON
	rmi4_data->sensor_max_y = rmi4_data->sensor_max_y * TPD_DISPLAY_HEIGH_RATIO / TPD_TOUCH_HEIGH_RATIO;
#endif
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->max_touch_width = MAX_F11_TOUCH_WIDTH;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	fhandler->data = NULL;

	offset = sizeof(query_0_5.data);

	/* query 6 */
	if (query_0_5.has_rel)
		offset += 1;

	/* queries 7 8 */
	if (query_0_5.has_gestures) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_7_8.data,
				sizeof(query_7_8.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_7_8.data);
	}

	/* query 9 */
	if (query_0_5.has_query_9) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
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
		retval = synaptics_rmi4_i2c_read(rmi4_data,
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
		retval = synaptics_rmi4_i2c_read(rmi4_data,
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

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			ctrl_28_address,
			&rmi4_data->report_enable,
			sizeof(rmi4_data->report_enable));
	if (retval < 0)
		return retval;

	return retval;
}

 /**
 * synaptics_rmi4_f12_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This funtion parses information from the Function 12 registers and
 * determines the number of fingers supported, offset to the data1
 * register, x and y data ranges, offset to the associated interrupt
 * status register, interrupt bit mask, and allocates memory resources
 * for finger data acquisition.
 */
static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char size_of_2d_data;
	unsigned char size_of_query5;
	unsigned char size_of_query8;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_20_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_27_offset;
	unsigned char ctrl_28_offset;
	unsigned char num_of_fingers;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_query_5 *query_5 = NULL;
	struct synaptics_rmi4_f12_query_8 *query_8 = NULL;
	struct synaptics_rmi4_f12_ctrl_8 *ctrl_8 = NULL;
	struct synaptics_rmi4_f12_ctrl_23 *ctrl_23 = NULL;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fhandler->extra\n",
				__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	query_5 = kzalloc(sizeof(*query_5), GFP_KERNEL);
	if (!query_5) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for query_5\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	query_8 = kzalloc(sizeof(*query_8), GFP_KERNEL);
	if (!query_8) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for query_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	ctrl_8 = kzalloc(sizeof(*ctrl_8), GFP_KERNEL);
	if (!ctrl_8) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for ctrl_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	ctrl_23 = kzalloc(sizeof(*ctrl_23), GFP_KERNEL);
	if (!ctrl_23) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for ctrl_23\n",
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

	if (size_of_query5 > sizeof(query_5->data))
		size_of_query5 = sizeof(query_5->data);
	memset(query_5->data, 0x00, sizeof(query_5->data));

	retval = synaptics_rmi4_i2c_read(rmi4_data,
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
	ctrl_27_offset = ctrl_23_offset +
			query_5->ctrl23_is_present +
			query_5->ctrl24_is_present +
			query_5->ctrl25_is_present +
			query_5->ctrl26_is_present;
	ctrl_28_offset = ctrl_27_offset +
			query_5->ctrl27_is_present;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_23_offset,
			ctrl_23->data,
			sizeof(ctrl_23->data));
	if (retval < 0)
		goto exit;

	/* Maximum number of fingers supported */
	fhandler->num_of_data_points = min(ctrl_23->max_reported_objects,
			(unsigned char)F12_FINGERS_TO_SUPPORT);

	num_of_fingers = fhandler->num_of_data_points;
	rmi4_data->num_of_fingers = num_of_fingers;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base + 7,
			&size_of_query8,
			sizeof(size_of_query8));
	if (retval < 0)
		goto exit;
	
	if (size_of_query8 > sizeof(query_8->data))
		size_of_query8 = sizeof(query_8->data);
	memset(query_8->data, 0x00, sizeof(query_8->data));	

	retval = synaptics_rmi4_i2c_read(rmi4_data,
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
		return retval;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_8_offset,
			ctrl_8->data,
			sizeof(ctrl_8->data));
	if (retval < 0)	
		goto exit;
		
	/* Maximum x and y */
	rmi4_data->sensor_max_x = SENSOR_MAX_X;
			//((unsigned short)ctrl_8->max_x_coord_lsb << 0) |
			//((unsigned short)ctrl_8->max_x_coord_msb << 8);
	rmi4_data->sensor_max_y = SENSOR_MAX_Y;
			//((unsigned short)ctrl_8->max_y_coord_lsb << 0) |
			//((unsigned short)ctrl_8->max_y_coord_msb << 8);

		/* It's recommended to parse max_x and max_y from contrel register, but this does not match MTK's mtk-tpd.c */

#ifdef TPD_HAVE_BUTTON
	rmi4_data->sensor_max_y = rmi4_data->sensor_max_y * TPD_DISPLAY_HEIGH_RATIO / TPD_TOUCH_HEIGH_RATIO;
#endif
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->num_of_rx = ctrl_8->num_of_rx;
	rmi4_data->num_of_tx = ctrl_8->num_of_tx;
	rmi4_data->max_touch_width = max(rmi4_data->num_of_rx,
			rmi4_data->num_of_tx);
	rmi4_data->f12_wakeup_gesture = query_5->ctrl27_is_present;
	if (rmi4_data->f12_wakeup_gesture) {
		extra_data->ctrl20_offset = ctrl_20_offset;
		extra_data->data4_offset = query_8->data0_is_present +
				query_8->data1_is_present +
				query_8->data2_is_present +
				query_8->data3_is_present;
		extra_data->ctrl27_offset = ctrl_27_offset;
	}

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	/* Allocate memory for finger data storage space */
	fhandler->data_size = num_of_fingers * size_of_2d_data;
	fhandler->data = kmalloc(fhandler->data_size, GFP_KERNEL);
	if (!fhandler->data) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fhandler->data\n",
				__func__);
		return -ENOMEM;
		goto exit;
	}
exit:
	kfree(query_5);
	kfree(query_8);
	kfree(ctrl_8);
	kfree(ctrl_23);

return retval;
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f1a_handle *f1a;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for function handle\n",
				__func__);
		return -ENOMEM;
	}

	fhandler->data = (void *)f1a;
	fhandler->extra = NULL;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			f1a->button_query.data,
			sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read query registers\n",
				__func__);
		return retval;
	}

	f1a->max_count = f1a->button_query.max_button_count + 1;

	f1a->button_control.txrx_map = kzalloc(f1a->max_count * 2, GFP_KERNEL);
	if (!f1a->button_control.txrx_map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for tx rx mapping\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_bitmask_size = (f1a->max_count + 7) / 8;

	f1a->button_data_buffer = kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for data buffer\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_map = kcalloc(f1a->max_count,
			sizeof(*(f1a->button_map)), GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
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
	unsigned char mapping_offset = 0;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	mapping_offset = f1a->button_query.has_general_control +
			f1a->button_query.has_interrupt_enable +
			f1a->button_query.has_multibutton_select;

	if (f1a->button_query.has_tx_rx_map) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				fhandler->full_addr.ctrl_base + mapping_offset,
				f1a->button_control.txrx_map,
				sizeof(f1a->button_control.txrx_map));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to read tx rx mapping\n",
					__func__);
			return retval;
		}

		rmi4_data->button_txrx_mapping = f1a->button_control.txrx_map;
	}

	if (cap_button_map.map) {
		if (cap_button_map.nbuttons != f1a->max_count) {
			f1a->valid_button_count = min(f1a->max_count,
					cap_button_map.nbuttons);
		} else {
			f1a->valid_button_count = f1a->max_count;
		}

		for (ii = 0; ii < f1a->valid_button_count; ii++)
			f1a->button_map[ii] = cap_button_map.map[ii];
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
	unsigned char command = 0x01;
	unsigned char intr_status;
	struct synaptics_rmi4_f01_device_status status;

	/* Do a device reset first */
	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0)
		return retval;

	msleep(DELAY_UI_READY);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			status.data,
			sizeof(status.data));
	if (retval < 0)
		return retval;

	while (status.status_code == STATUS_CRC_IN_PROGRESS) {
		if (timeout > 0)
			msleep(20);
		else
			return -1;

		retval = synaptics_rmi4_i2c_read(rmi4_data,
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

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1,
			&intr_status,
			sizeof(intr_status));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read interrupt status\n",
				__func__);
		return retval;
	}

	return 0;
}

static void synaptics_rmi4_set_configured(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to set configured\n",
				__func__);
		return;
	}

	rmi4_data->no_sleep_setting = device_ctrl & NO_SLEEP_ON;
	device_ctrl |= CONFIGURED;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to set configured\n",
				__func__);
	}

	return;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
		struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kmalloc(sizeof(**fhandler), GFP_KERNEL);
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

 /**
 * synaptics_rmi4_query_device()
 *
 * Called by synaptics_rmi4_probe().
 *
 * This funtion scans the page description table, records the offsets
 * to the register types of Function $01, sets up the function handlers
 * for Function $11 and Function $12, determines the number of interrupt
 * sources from the sensor, adds valid Functions with data inputs to the
 * Function linked list, parses information from the query registers of
 * Function $01, and enables the interrupt sources from the valid Functions
 * with data inputs.
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char page_number;
	unsigned char intr_count;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned short pdt_entry_addr;
	unsigned short intr_addr;
	bool was_in_bl_mode;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

rescan_pdt:
	was_in_bl_mode = false;
	intr_count = 0;
	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(&rmi4_data->i2c_client->dev,
						"%s: Reached end of PDT\n",
						__func__);
				break;
			}

			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: F%02x found (page %d)\n",
					__func__, rmi_fd.fn_number,
					page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
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
					dev_err(&rmi4_data->i2c_client->dev,
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
					dev_err(&rmi4_data->i2c_client->dev,
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
					dev_err(&rmi4_data->i2c_client->dev,
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
					dev_err(&rmi4_data->i2c_client->dev,
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
			case SYNAPTICS_RMI4_F51:
				retval = synaptics_rmi4_f51_init(rmi4_data,
						NULL, &rmi_fd, page_number);
				if (retval < 0)
					return retval;

				break;
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
				
			}
		}
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Number of interrupt registers = %d\n",
			__func__, rmi4_data->num_of_intr_regs);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
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
	rmi->product_info[0] = f01_query[2] & MASK_7BIT;
	rmi->product_info[1] = f01_query[3] & MASK_7BIT;
	rmi->date_code[0] = f01_query[4] & MASK_5BIT;
	rmi->date_code[1] = f01_query[5] & MASK_4BIT;
	rmi->date_code[2] = f01_query[6] & MASK_5BIT;
	rmi->tester_id = ((f01_query[7] & MASK_7BIT) << 8) |
			(f01_query[8] & MASK_7BIT);
	rmi->serial_number = ((f01_query[9] & MASK_7BIT) << 8) |
			(f01_query[10] & MASK_7BIT);
	memcpy(rmi->product_id_string, &f01_query[11], 10);

	if (rmi->manufacturer_id != 1) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Non-Synaptics device found, manufacturer ID = %d\n",
				__func__, rmi->manufacturer_id);
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
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

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				return retval;
		}
	}

	synaptics_rmi4_set_configured(rmi4_data);

	return 0;
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

#ifdef TYPE_B_PROTOCOL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers,0);
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

	if (rmi4_data->f11_wakeup_gesture || rmi4_data->f12_wakeup_gesture) {
		set_bit(KEY_POWER, rmi4_data->input_dev->keybit);
		input_set_capability(rmi4_data->input_dev, EV_KEY, KEY_POWER);
	}
	return;
}

static int synaptics_rmi4_set_input_dev(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	rmi4_data->input_dev = input_allocate_device();
	if (rmi4_data->input_dev == NULL) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to allocate input device\n",
				__func__);
		retval = -ENOMEM;
		goto err_input_device;
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	//set esd mode to 0x02 after reset device
	synaptics_rmi4_set_esd_mode(rmi4_data);
	
	rmi4_data->input_dev->name = DRIVER_NAME;
	rmi4_data->input_dev->phys = INPUT_PHYS_NAME;
	rmi4_data->input_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	rmi4_data->input_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	rmi4_data->input_dev->id.bustype = BUS_I2C;
	rmi4_data->input_dev->dev.parent = &rmi4_data->i2c_client->dev;
	input_set_drvdata(rmi4_data->input_dev, rmi4_data);

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);
	set_bit(EV_KEY, rmi4_data->input_dev->evbit);
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, rmi4_data->input_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif

	set_bit(KEY_F4 , rmi4_data->input_dev->keybit);//doulbe-tap resume

	synaptics_rmi4_set_params(rmi4_data);

	retval = input_register_device(rmi4_data->input_dev);
	if (retval) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register input device\n",
				__func__);
		goto err_register_input;
	}

	return 0;

err_register_input:
err_query_device:
	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_free_device(rmi4_data->input_dev);

err_input_device:
	return retval;
}

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;
	mutex_lock(&rmi4_report_mutex);
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

	input_report_key(rmi4_data->input_dev, KEY_MENU, 0);
	input_sync(rmi4_data->input_dev);
	input_report_key(rmi4_data->input_dev, KEY_HOMEPAGE, 0);
	input_sync(rmi4_data->input_dev);
	input_report_key(rmi4_data->input_dev, KEY_BACK, 0);
	input_sync(rmi4_data->input_dev);
	input_report_key(rmi4_data->input_dev, BTN_TOUCH, 0);

	mutex_unlock(&rmi4_report_mutex);
	rmi4_data->fingers_on_2d = false;

	return 0;
}

static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned short intr_addr;
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

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				goto exit;
		}
	}

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

static int synaptics_rmi4_set_esd_mode(struct synaptics_rmi4_data *rmi4_data) {
	int retval;
	unsigned char command = 0x02;
	//unsigned short esd_mode_addr = 0x0459;

	TPD_INFO("%s, esd_mode_addr = 0x%x\n", __func__,rmi4_data->f51_cmd_base_addr);
	/* exit ESD Mode */
	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f51_cmd_base_addr,
			&command,
			sizeof(command));
	if(retval < 0) {
		TPD_ERR("%s: failed to set esd mode\n", __func__);
		return retval;
	}

	return retval;
}

/*
static int synaptics_rmi4_soft_reset(struct synaptics_rmi4_data *rmi4_data) {
	int retval;
	unsigned char command = 0x01;

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));
	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}
	TPD_DETAIL("Soft Reset TP\n");
	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
	return retval;
}
*/

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char command = 0x01;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	rmi4_data->touch_stopped = true;

	synaptics_rmi4_irq_enable(rmi4_data, false);
	
	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
		return retval;
	}

	msleep(DELAY_UI_READY);

	synaptics_rmi4_free_fingers(rmi4_data);

	synaptics_rmi4_empty_fn_list(rmi4_data);

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to query device\n",
				__func__);
		mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
		return retval;
	}

	synaptics_rmi4_set_params(rmi4_data);

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->reset != NULL)
				exp_fhandler->exp_fn->reset(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	synaptics_rmi4_set_esd_mode(rmi4_data);

	rmi4_data->touch_stopped = false;

	synaptics_rmi4_irq_enable(rmi4_data, true);	

	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));

	return 0;
}


static void synaptics_rmi4_sleep_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	unsigned char device_ctrl;
	unsigned char no_sleep_setting = rmi4_data->no_sleep_setting;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read device control\n",
				__func__);
		return;
	}

	device_ctrl = device_ctrl & ~MASK_3BIT;
	if (enable)
		device_ctrl = device_ctrl | NO_SLEEP_OFF | SENSOR_SLEEP;
	else
		device_ctrl = device_ctrl | no_sleep_setting | NORMAL_OPERATION;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write device control\n",
				__func__);
		return;
	}

	return;
}




/**
* synaptics_rmi4_exp_fn_work()
*
* Called by the kernel at the scheduled time.
*
* This function is a work thread that checks for the insertion and
* removal of other expansion Function modules such as rmi_dev and calls
* their initialization and removal callback functions accordingly.
*/
static void synaptics_rmi4_exp_fn_work(struct work_struct *work)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler_temp;
	struct synaptics_rmi4_data *rmi4_data = exp_data.rmi4_data;

	TPD_INFO("%s Enter\n", __func__);
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry_safe(exp_fhandler,
				exp_fhandler_temp,
				&exp_data.list,
				link) {
			if ((exp_fhandler->exp_fn->init != NULL) &&
					exp_fhandler->insert) {
				retval = exp_fhandler->exp_fn->init(rmi4_data);
				if (retval < 0) {
					list_del(&exp_fhandler->link);
					kfree(exp_fhandler);
				} else {
					exp_fhandler->insert = false;
				}
			} else if ((exp_fhandler->exp_fn->remove != NULL) &&
					exp_fhandler->remove) {
				exp_fhandler->exp_fn->remove(rmi4_data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&exp_data.mutex);

	//step12: get finger protect data
	if (rmi4_data->spurious_fp_support) {	
		mutex_lock(&rmi4_data->rmi4_func_mutex);
		PRINTK_ON_FAIL(synaptics_rmi4_finger_proctect_data_get(rmi4_data), retval, "Failed to get fingerprint protect data\n");
		mutex_unlock(&rmi4_data->rmi4_func_mutex);
	}

	return;
}

/**
* synaptics_rmi4_new_function()
*
* Called by other expansion Function modules in their module init and
* module exit functions.
*
* This function is used by other expansion Function modules such as
* rmi_dev to register themselves with the driver by providing their
* initialization and removal callback function pointers so that they
* can be inserted or removed dynamically at module init and exit times,
* respectively.
*/
void synaptics_rmi4_new_function(struct synaptics_rmi4_exp_fn *exp_fn,
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
EXPORT_SYMBOL(synaptics_rmi4_new_function);

static int synaptics_rmi4_fb_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;
    struct synaptics_rmi4_data *rmi4_data = container_of(self, struct synaptics_rmi4_data, fb_notify);

	if ((evdata) && (evdata->data) && (rmi4_data)) {
		blank = *(int *)evdata->data;
		TPD_INFO("%s Entering fb notify, event %ld, blank %d\n", __func__, event, blank);
		if (blank == FB_BLANK_UNBLANK) {
			if (event == FB_EVENT_BLANK) {
				synaptics_rmi4_resume(NULL);
			} else if (event == FB_EARLY_EVENT_BLANK) {
				if (rmi4_data->is_suspended) {
					disable_irq_nosync(rmi4_data->irq);
				}
			}
		} else if (event == FB_EVENT_BLANK && blank == FB_BLANK_POWERDOWN) { //tp suspend after lcd
			synaptics_rmi4_suspend(NULL);
		}
	}

	return 0;
}

static ssize_t synaptics_rmi4_f34_configid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", rmi4_data->config_id);
}

static ssize_t synaptics_rmi4_f01_product_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(rmi4_data->rmi4_mod_info.product_id_string));
}

static DEVICE_ATTR(tp_firmware_version, 0664, synaptics_rmi4_f34_configid_show, synaptics_rmi4_store_error);
static DEVICE_ATTR(product_id, 0664, synaptics_rmi4_f01_product_id_show, synaptics_rmi4_store_error);

static struct attribute *synaptics_rmi4_attrs[] = {
	attrify(tp_firmware_version),
	attrify(product_id),
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = synaptics_rmi4_attrs,
};

static int synaptics_rmi4_alloc_i2c(struct i2c_client *client) {
	//force to set i2c address to 0x20
	client->addr = 0x20;
	//client->timing = 350;
	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data not supported\n",
				__func__);
		return -EIO;
	}

#ifdef USE_I2C_DMA
	wDMABuf_va = (unsigned char *)dma_zalloc_coherent(&client->dev, WRITE_SIZE_LIMIT,
				 &wDMABuf_pa, GFP_KERNEL);
		if(!wDMABuf_va){
			dev_err(&client->dev,"Allocate DMA I2C Buffer failed, exit\n");
		return -ENOMEM;
		}
#endif
	return 0;
}

 static int synaptics_rmi4_alloc_device(struct i2c_client *client, struct synaptics_rmi4_data **data) {
 	struct synaptics_rmi4_data *rmi4_data = NULL;
 	rmi4_data = kzalloc(sizeof(struct synaptics_rmi4_data), GFP_KERNEL);
	if (!rmi4_data) {
		dev_err(&client->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
	}

	rmi4_data->i2c_client = client;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->touch_stopped = false;
	rmi4_data->is_suspended = false;
	rmi4_data->irq_enabled = false;
	rmi4_data->fingers_on_2d = false;
	rmi4_data->loading_fw = false;
	rmi4_data->clear_button = true;

	rmi4_data->i2c_read = synaptics_rmi4_i2c_read;
	rmi4_data->i2c_write = synaptics_rmi4_i2c_write;
	rmi4_data->irq_enable = synaptics_rmi4_irq_enable;
	rmi4_data->reset_device = synaptics_rmi4_reset_device;
	rmi4_data->sleep_enable = synaptics_rmi4_sleep_enable;

	rmi4_data->i2c_addr = client->addr;

	rmi4_data->hw_res = &hw_res;
	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->rmi4_reset_mutex));
	mutex_init(&(rmi4_data->rmi4_exp_init_mutex));
	mutex_init(&(rmi4_data->rmi4_gpio_mutex));
	mutex_init(&(rmi4_data->rmi4_func_mutex));

	i2c_set_clientdata(client, rmi4_data);

	*data = rmi4_data;
	return 0;
 }

static int synaptics_rmi4_device_reset(struct synaptics_rmi4_data *rmi4_data) {
	if (rmi4_data->hw_res == NULL)
		return -ENODEV;

	/* OPTIONAL: hardware reset pin toggling */
	/*
	synaptics_gpio_as_int(1, rmi4_data);// eint 1
	synatpics_gpio_output(0, 1, rmi4_data);// reset pin high
	msleep(DELAY_BOOT_READY);
	synatpics_gpio_output(0, 0, rmi4_data);// reset pin low
	msleep(DELAY_RESET_LOW);
	synatpics_gpio_output(0, 1, rmi4_data);// reset pin high
	msleep(DELAY_UI_READY);
	*/
	return 0;
 }

static int synaptics_rmi4_irq_request(struct synaptics_rmi4_data *rmi4_data) {
	int retval = 0;

	/* EINT device tree, default EINT enable */
	if (tpd_irq_registration(rmi4_data) < 0) {
		pr_err(" %s: failed regist irq: %d\n",__func__, retval);
		return -1;
	}

	return retval;
}

static int synaptics_rmi4_resume_workqueue_alloc(struct synaptics_rmi4_data *rmi4_data) {
	int ret = 0;
	rmi4_data->speedup_resume_wq = create_singlethread_workqueue("speedup_resume_wq");
    if (!rmi4_data->speedup_resume_wq) {
        ret = -ENOMEM;
		return ret;
    }
    INIT_WORK(&rmi4_data->speed_up_work, synaptics_rmi4_speedup_resume);

	return ret;
}

static int synaptics_rmi4_create_sysfs(struct synaptics_rmi4_data *rmi4_data, unsigned char *attr_count) {
	int retval = 0;
	if (!exp_data.initialized) {
			mutex_init(&exp_data.mutex);
			INIT_LIST_HEAD(&exp_data.list);
			exp_data.initialized = true;
		}

	exp_data.workqueue = create_singlethread_workqueue("dsx_exp_workqueue");
	INIT_DELAYED_WORK(&exp_data.work, synaptics_rmi4_exp_fn_work);
	exp_data.rmi4_data = rmi4_data;
	exp_data.queue_work = true;
	queue_delayed_work(exp_data.workqueue,
			&exp_data.work,
			msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));

	for ((*attr_count) = 0; (*attr_count) < ARRAY_SIZE(attrs); (*attr_count)++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[*attr_count].attr);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			return retval;
		}
	}

	retval = sysfs_create_group(&rmi4_data->i2c_client->dev.kobj, &attr_group);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs attributes\n",
				__func__);
		return retval;
	}
	return retval;
}

static int synaptics_rmi4_register_proc(struct synaptics_rmi4_data *rmi4_data) {
	int i = 0;
	struct synaptics_rmi4_proc_info *info;
	struct proc_dir_entry *prEntry_root = NULL;
	struct proc_dir_entry *prEntry_debug_info = NULL;
	struct proc_dir_entry *prEntry_tmp = NULL;

	//setp 1: create parent directory
	RETURN_ON_NULL(proc_mkdir("touchpanel", NULL), prEntry_root, "Create root dir failed\n");
	for(i = 0; i < MAX_PROC_FUNC_SIZE; i++) {
		info = &func_operators[i];
		if (strlen(info->name) != 0) {
			RETURN_ON_NULL(proc_create_data(info->name, 0666, prEntry_root, &info->operator, rmi4_data), prEntry_tmp, "Proc create %s failed\n", info->name);
		}
	}

	//Step 1: register debug interface
	RETURN_ON_NULL(proc_mkdir("debug_info", prEntry_root), prEntry_debug_info, "Create debug info dir failed\n");
	for(i = 0; i < MAX_PROC_DEBUG_SIZE; i++) {
		info = &debug_operators[i];
		if (strlen(info->name) != 0) {
			RETURN_ON_NULL(proc_create_data(info->name, 0666, prEntry_debug_info, &info->operator, rmi4_data), prEntry_tmp, "Proc create %s failed\n", info->name);
		}
	}

	TPD_INFO("proc register success\n");
	return 0;
}

/*
static void synaptics_rmi4_wakeup_finger_protect(int wakeup, void *data) {
	struct synaptics_rmi4_data *rmi4_data;
	if (data == NULL)
		return;

	rmi4_data = (struct synaptics_rmi4_data *)data;

	if (rmi4_data->spuri_fp_touch.lcd_trigger_fp_check && rmi4_data->spurious_fp_support) {
		if (wakeup) {
			rmi4_data->spuri_fp_touch.lcd_resume_ok = true;
			wake_up_interruptible(&finger_waiter);
		} else {
			rmi4_data->spuri_fp_touch.lcd_resume_ok = false;
		}
	}

	TPD_DEBUG("%s: wakeup %d, lcd %d\n", __func__, wakeup, rmi4_data->spuri_fp_touch.lcd_resume_ok);
}

static int synaptics_rmi4_lcdon_in_gesture(void *data) {
	struct synaptics_rmi4_data *rmi4_data;
	int lcd_on = 0;
	if (data == NULL)
		return 0;
	rmi4_data = (struct synaptics_rmi4_data *)data;

	if (rmi4_data->enable_wakeup_gesture == 1 || rmi4_data->enable_wakeup_gesture == 2)
		lcd_on = 1;

	TPD_DEBUG("%s: lcd state %d\n", __func__, lcd_on);
	return lcd_on;
}
*/

static void synaptics_rmi4_get_config_id(struct synaptics_rmi4_data *rmi4_data) {
	int ret = 0;
	unsigned short f34_ctrl_base = 0x0009;
	unsigned char config_id[4];
	ret = synaptics_rmi4_i2c_read(rmi4_data, f34_ctrl_base, &config_id[0], sizeof(config_id));
	if (ret < 0) {
		return;
	}

	rmi4_data->config_id = (unsigned int)config_id[3] +
			(unsigned int)config_id[2] * 0x100 +
			(unsigned int)config_id[1] * 0x10000 +
			(unsigned int)config_id[0] * 0x1000000;
}

static int synaptics_rmi4_sensor_info(struct synaptics_rmi4_data *rmi4_data) {
	char *manu = NULL;
	char *version = NULL;
	if (manu == NULL) {
		manu = kzalloc(MAX_NAME_LEN, GFP_KERNEL);
		if (manu == NULL)
			return -ENOMEM;
	}

	if (version == NULL) {
		version = kzalloc(MAX_NAME_LEN, GFP_KERNEL);
		if (version == NULL)
			return -ENOMEM;
	}

	rmi4_data->lcm = lcm;
	rmi4_data->lcm_name = lcm_name;

	//load different limit image for different lcm
	snprintf(rmi4_data->limit_path, MAX_NAME_LEN, "tp/16091_Limit_TD4322_%s.img", rmi4_data->lcm_name);
	snprintf(rmi4_data->firmware_path, MAX_NAME_LEN, "tp/16091_FW_TD4322_%s.img", rmi4_data->lcm_name);
	TPD_INFO("FW name:%s Limit name:%s\n", rmi4_data->firmware_path, rmi4_data->limit_path);
	//read the config id first
	synaptics_rmi4_get_config_id(rmi4_data);

	snprintf(version, MAX_NAME_LEN, "0x%x", rmi4_data->config_id);
	snprintf(manu, MAX_NAME_LEN, "TD4322-%s-%x%x", rmi4_data->lcm_name, rmi4_data->rmi4_mod_info.product_info[0], rmi4_data->rmi4_mod_info.product_info[1]);
//	register_device_proc("tp", version, manu);

	rmi4_data->enable_wakeup_gesture = false;
/*
	rmi4_data->rmi4_mod_info.manufacture_info.manufacture = manu;
	rmi4_data->rmi4_mod_info.manufacture_info.version = version;
*/
	rmi4_data->spurious_fp_support = true;
	rmi4_data->spuri_fp_touch.fp_trigger = false;
	rmi4_data->spuri_fp_touch.thread = NULL;
	rmi4_data->spuri_fp_touch.lcd_trigger_fp_check = true;
	rmi4_data->i2c_ready = false;
	return 0;
}

static fp_touch_state synaptics_rmi4_spurious_fp_check(struct synaptics_rmi4_data *rmi4_data) {
	int i = 0, j = 0, delta_count = 0;
	int count = 0;
	int ret = 0;
	int tx = rmi4_data->num_of_tx + 1;
	int16_t static_data = 0, touch_data = 0;
	uint8_t *raw_data;
	int raw_size;
	fp_touch_state fp_touch_state = FINGER_PROTECT_TOUCH_UP;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler = synaptics_rmi4_handler(RMI_TEST_REPORTING);

	TPD_DEBUG("%s Enter\n", __func__);
	raw_data = kzalloc(rmi4_data->num_of_tx*MAX_FINGER_PROTECT_RX*2, GFP_KERNEL);
	if (raw_data == NULL) {
		return FINGER_PROTECT_NOTREADY;
	}

	if (!rmi4_data->spuri_fp_data) {
		TPD_ERR("Get screen data first\n");
		kfree(raw_data);
		return fp_touch_state;
	}

	raw_size = tx * MAX_FINGER_PROTECT_RX * 2;
	ret = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_FULL_RAW_PARTITION, raw_data, &raw_size);

	if (ret < 0) {
		return FINGER_PROTECT_TOUCH_DOWN;
	}

	for (i = 0; i < tx; i++) {
		for (j = 0; j < MAX_FINGER_PROTECT_RX; j++) {
			if ((i == tx -1) && j > 2) // add this for 2 buttons.
				break;
			static_data = *(rmi4_data->spuri_fp_data + count);
			touch_data = *(raw_data + 2*count) | (*(raw_data + 2*count + 1) << 8);
			if ((touch_data - static_data) > SPURIOUS_FP_LIMIT) {
				TPD_DEBUG("delta_data too large, delta_data = %d TX[%d] RX[%d]\n", static_data - touch_data, i, j);
				delta_count++;
			}
			count++;
		}
		if (delta_count > 2) {
			fp_touch_state = FINGER_PROTECT_TOUCH_DOWN;
			delta_count = 0;
			if (!ts_debug_level)
				break;
		}
	}

	kfree(raw_data);
	TPD_INFO("%s Exit, state %d\n", __func__, fp_touch_state);
	return fp_touch_state;
}

static int synaptics_rm4_finger_protect_handler(void *data) {
	struct synaptics_rmi4_data *rmi4_data = (struct synaptics_rmi4_data *)data;

	if (!rmi4_data) {
        TPD_ERR("ts is null should nerver get here!\n");
        return 0;
    }

    do {
        set_current_state(TASK_INTERRUPTIBLE);
        if (rmi4_data->spuri_fp_touch.lcd_trigger_fp_check)
            wait_event_interruptible(finger_waiter, rmi4_data->spuri_fp_touch.fp_trigger && rmi4_data->i2c_ready && rmi4_data->spuri_fp_touch.lcd_resume_ok);
        else
            wait_event_interruptible(finger_waiter, rmi4_data->spuri_fp_touch.fp_trigger && rmi4_data->i2c_ready);
        set_current_state(TASK_RUNNING);
        rmi4_data->spuri_fp_touch.fp_trigger = false;
        rmi4_data->spuri_fp_touch.fp_touch_st = FINGER_PROTECT_NOTREADY;

        mutex_lock(&rmi4_data->rmi4_func_mutex);
        rmi4_data->spuri_fp_touch.fp_touch_st = synaptics_rmi4_spurious_fp_check(rmi4_data);
        if (rmi4_data->fingers_on_2d) {
            TPD_ERR("%s tp touch down,clear flag\n",__func__);
            rmi4_data->fingers_on_2d = 0;
        }
        mutex_unlock(&rmi4_data->rmi4_func_mutex);
    } while (!kthread_should_stop());

    return 0;

}

static int synaptics_rmi4_finger_proctect_data_get(struct synaptics_rmi4_data *rmi4_data) {
	int i = 0, j = 0;
	int retry = 3;
	int success = false;
	int count = 0, cnt = 0;
	int tx = rmi4_data->num_of_tx + 1;
	unsigned char *raw_data;
	int raw_size;
	char buffer[256];
	int ret = 0;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler = synaptics_rmi4_handler(RMI_TEST_REPORTING);

	if (exp_fhandler == NULL) {
		TPD_ERR("F54 not init success, init first\n");
		return -ENODEV;
	}

	if (rmi4_data->firmware_id == 0) {
		TPD_ERR("firmware is null, update firmware first\n");
		return -ENODATA;
	}

	if (rmi4_data->spuri_fp_touch.thread == NULL) {
		rmi4_data->spuri_fp_touch.thread = kthread_run(synaptics_rm4_finger_protect_handler, rmi4_data, "td4322_fp");
		if (IS_ERR(rmi4_data->spuri_fp_touch.thread)) {
			rmi4_data->spurious_fp_support = false;
			return -ENOLINK;
		}
	} else {
		//this can only run one time
		return 0;
	}

	//get protect data
	if (rmi4_data->spuri_fp_data)
		return 0;

	raw_size = tx *MAX_FINGER_PROTECT_RX*2;
	rmi4_data->spuri_fp_data = kzalloc(raw_size, GFP_KERNEL);
	if (rmi4_data->spuri_fp_data == NULL)
		return -ENOMEM;

	raw_data = kzalloc(rmi4_data->num_of_tx*MAX_FINGER_PROTECT_RX*2, GFP_KERNEL);
	if (raw_data == NULL) {
		kfree(rmi4_data->spuri_fp_data);
		return -ENOMEM;
	}

	while(retry) {
		ret = exp_fhandler->exp_fn->invoke(rmi4_data, RMI_TDDI_FULL_RAW_PARTITION, raw_data, &raw_size);
		if (ret >= 0) {
			success = true;
			break;
		}
		retry--;
	}

	if (!success) {
		kfree(rmi4_data->spuri_fp_data);
		kfree(raw_data);
		return -ENODATA;
	}

	for (i = 0; i < tx; i++) {
		cnt = 0;
		memset(buffer, 0, sizeof(buffer));
		for (j = 0; j < MAX_FINGER_PROTECT_RX; j++) {
			*(rmi4_data->spuri_fp_data + count) = *(raw_data + 2*count) | (*(raw_data + 2*count + 1) << 8);
			cnt += sprintf(buffer + cnt, "%-5d", *(rmi4_data->spuri_fp_data + count));
			count++;
		}
		TPD_INFO("%s \n", buffer);
	}

	TPD_DEBUG("%s:End\n", __func__);
	kfree(raw_data);
	return 0;
}

static int synaptics_rmi4_platform_init(struct i2c_client* client);
 /**
 * synaptics_rmi4_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 * This funtion allocates and initializes the resources for the driver
 * as an input driver, turns on the power to the sensor, queries the
 * sensor for its supported Functions and characteristics, registers
 * the driver to the input subsystem, sets up the interrupt, handles
 * the registration of the early_suspend and late_resume functions,
 * and creates a work queue for detection of other expansion Function
 * modules.
 */
 #if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
static int __devinit synaptics_rmi4_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
#else
static int synaptics_rmi4_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
#endif
{
	int retval;
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data;

	TPD_INFO("%s Enter\n", __func__);
    synaptics_rmi4_platform_init(client);

	//step2: alloc i2c resource
	RETURN_ON_FAIL(synaptics_rmi4_alloc_i2c(client), retval, "alloc i2c failed\n");

	//step3: alloc device memory
	RETURN_ON_FAIL(synaptics_rmi4_alloc_device(client, &rmi4_data), retval, "alloc device failed\n");

	//step4: reset the device
	GOTO_ON_FAIL(synaptics_rmi4_device_reset(rmi4_data), err_set_input_dev, "reset devince failed\n");

	//step5: alloc input
	GOTO_ON_FAIL(synaptics_rmi4_set_input_dev(rmi4_data), err_set_input_dev, "Failed to set up input device\n");

	//step6: setup event handler
	GOTO_ON_FAIL(synaptics_rmi4_irq_request(rmi4_data), err_enable_irq, "Irq request and event handler create failed\n");

	//step7:
	GOTO_ON_FAIL(synaptics_rmi4_resume_workqueue_alloc(rmi4_data), err_alloc_resume_queue, "Alloc speedup queue failed\n");
	
	//step8: creat sysfs attributes
	GOTO_ON_FAIL(synaptics_rmi4_create_sysfs(rmi4_data, &attr_count), err_sysfs, "Create sysfs attributes failed\n");

	//step9: register tp info
	PRINTK_ON_FAIL(synaptics_rmi4_sensor_info(rmi4_data), retval, "Register tp info failed\n");

	//step10: register notify
	rmi4_data->fb_notify.notifier_call = synaptics_rmi4_fb_callback;
	retval = fb_register_client(&rmi4_data->fb_notify);

	//step11: register proc interface
	PRINTK_ON_FAIL(synaptics_rmi4_register_proc(rmi4_data), retval, "Register proc failed\n");

	//step12: register black gesture callback
	//PRINTK_ON_FAIL(primary_display_register_tp_gesture_cb(synaptics_rmi4_lcdon_in_gesture, rmi4_data), retval, "Register gesture callback failed\n");

	//step13: register finger protect callback
	if (rmi4_data->spurious_fp_support) {
		//PRINTK_ON_FAIL(primary_display_register_tp_wakeup_cb(synaptics_rmi4_wakeup_finger_protect, rmi4_data), retval, "Register gesture callback failed\n");
	}

	g_dev = &rmi4_data->input_dev->dev;
	TPD_INFO("%s Exit\n", __func__);
	return retval;

err_sysfs:
	sysfs_remove_group(&client->dev.kobj, &attr_group);
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	cancel_delayed_work_sync(&exp_data.work);
	flush_workqueue(exp_data.workqueue);
	destroy_workqueue(exp_data.workqueue);

	synaptics_rmi4_irq_enable(rmi4_data, false);

err_alloc_resume_queue:
	free_irq(rmi4_data->irq, rmi4_data);

err_enable_irq:

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

err_set_input_dev:
	kfree(rmi4_data);

	return retval;
}

 /**
 * synaptics_rmi4_remove()
 *
 * Called by the kernel when the association with an I2C device of the
 * same name is broken (when the driver is unloaded).
 *
 * This funtion terminates the work queue, stops sensor data acquisition,
 * frees the interrupt, unregisters the driver from the input subsystem,
 * turns off the power to the sensor, and frees other allocated resources.
 */
 #if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
static int __devexit synaptics_rmi4_remove(struct i2c_client *client)
#else
static int synaptics_rmi4_remove(struct i2c_client *client)
#endif
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = i2c_get_clientdata(client);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	cancel_delayed_work_sync(&exp_data.work);
	flush_workqueue(exp_data.workqueue);
	destroy_workqueue(exp_data.workqueue);

	synaptics_rmi4_irq_enable(rmi4_data, false);

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

#ifdef USE_I2C_DMA
	if(wDMABuf_va){
		dma_free_coherent(&client->dev, WRITE_SIZE_LIMIT , wDMABuf_va, wDMABuf_pa);
	}
#endif	
	
	kfree(rmi4_data);

	return 0;
}

#ifdef CONFIG_PM
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

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			&reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	reporting_control = (reporting_control & ~MASK_3BIT);
	if (enable)
		reporting_control |= F11_WAKEUP_GESTURE_MODE;
	else
		reporting_control |= F11_CONTINUOUS_MODE;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			fhandler->full_addr.ctrl_base,
			&reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	return;
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

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	if (enable)
		reporting_control[2] |= F12_WAKEUP_GESTURE_MODE;
	else
		reporting_control[2] &= F12_CONTINUOUS_MODE;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));

	TPD_INFO("Gesture Status 0x%x, 0x%x, 0x%x\n", reporting_control[0], reporting_control[1], reporting_control[2]);
	return;
}

static void synaptics_rmi4_wakeup_gesture(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	if (rmi4_data->f11_wakeup_gesture)
		synaptics_rmi4_f11_wg(rmi4_data, enable);
	else if (rmi4_data->f12_wakeup_gesture) //used this in td4322
		synaptics_rmi4_f12_wg(rmi4_data, enable);

	return;
}

static int synaptics_i2c_suspend(struct device *dev) {
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(g_dev);
	rmi4_data->i2c_ready = false;

	if (rmi4_data->enable_wakeup_gesture == 1) {
		enable_irq_wake(rmi4_data->irq);
	}

	return 0;
}

static int synaptics_i2c_resume(struct device *dev) {
	 struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(g_dev);

	 //enable_irq(touch_irq);
	 rmi4_data->i2c_ready = true;
	 if (rmi4_data->spurious_fp_support && rmi4_data->spuri_fp_touch.fp_trigger) {
		 wake_up_interruptible(&finger_waiter);
	 }
	 return 0;
 }

 /**
 * synaptics_rmi4_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 * This function stops finger data acquisition and puts the sensor to
 * sleep (if not already done so during the early suspend phase),
 * disables the interrupt, and turns off the power to the sensor.
 */
static void synaptics_rmi4_suspend(struct device *dev)
{
	//struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(g_dev);

	TPD_DETAIL("%s staying awake %d, lcd %d, i2c %d\n", __func__, rmi4_data->staying_awake, 
		rmi4_data->spuri_fp_touch.lcd_resume_ok, rmi4_data->i2c_ready);
	mutex_lock(&rmi4_data->rmi4_func_mutex);

	if (rmi4_data->is_suspended) {
		TPD_ERR("Not enter suspended twice\n");		
		mutex_unlock(&rmi4_data->rmi4_func_mutex);
		return;
	}

	if (rmi4_data->staying_awake)
		goto exit_func;

	rmi4_data->is_suspended = true;

	synaptics_rmi4_free_fingers(rmi4_data);

	if (rmi4_data->enable_wakeup_gesture == 1 || rmi4_data->enable_wakeup_gesture == 2) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, true);
		/*enable gpio wake system through intterrupt*/
		if (rmi4_data->enable_wakeup_gesture == 2) {
			synaptics_rmi4_sleep_enable(rmi4_data, false);
			TPD_DETAIL("Sensor touched, enter sleep mode\n");
		}
		goto exit_func;
	}

	rmi4_data->touch_stopped = true;
	rmi4_data->sensor_sleep = true;

exit_func:
	synaptics_i2c_suspend(dev);
	mutex_unlock(&rmi4_data->rmi4_func_mutex);
	return;
}

static void synaptics_rmi4_resume(struct device *dev) {
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(g_dev);

	if (!rmi4_data->is_suspended) {
		TPD_ERR("Enter resume after suspend\n");
		return;
	}

	rmi4_data->is_suspended = false;

	if (rmi4_data->loading_fw) {
		TPD_INFO("Loading firmware, return\n");
		return;
	}

	rmi4_data->clear_button = true;

	//free irq here, and request irq in speedup resume
	free_irq(rmi4_data->irq, rmi4_data);
    queue_work(rmi4_data->speedup_resume_wq, &rmi4_data->speed_up_work);
}
 /**
 * synaptics_rmi4_resume()
 *
 * Called by the kernel during the resume phase when the system
 * wakes up from suspend.
 *
 * This function turns on the power to the sensor, wakes the sensor
 * from sleep, enables the interrupt, and starts finger data
 * acquisition.
 */
static void synaptics_rmi4_speedup_resume(struct work_struct *work)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(g_dev);

	TPD_DETAIL("%s staying awake %d, lcd %d, i2c %d\n", __func__, rmi4_data->staying_awake,
		rmi4_data->spuri_fp_touch.lcd_resume_ok, rmi4_data->i2c_ready);

	mutex_lock(&rmi4_data->rmi4_func_mutex);
	if (rmi4_data->staying_awake)
		goto exit_func;


	if (rmi4_data->enable_wakeup_gesture == 1) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		//goto exit_exp;
	}

	/*
	* remove soft reset because lcd hardware reset when resume
	*
	retval = synaptics_rmi4_soft_reset(rmi4_data);
	if (retval < 0) {
		TPD_ERR("Failed to reset device\n");
		goto exit_func;
	}
	*/

	synaptics_rmi4_sleep_enable(rmi4_data, false);
	synaptics_rmi4_set_esd_mode(rmi4_data);

	retval = synaptics_rmi4_reinit_device(rmi4_data);
	if (retval < 0) {
		TPD_ERR("Failed to reinit device\n");
		goto exit_func;
	}

//exit_exp:
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->resume != NULL)
				exp_fhandler->exp_fn->resume(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

exit_func:
	rmi4_data->touch_stopped = false;
	rmi4_data->sensor_sleep = false;

	retval = request_threaded_irq(touch_irq, NULL, tpd_eint_handler_fn,
		rmi4_data->irq_flags | IRQF_ONESHOT, 
		"TOUCH_TD4322-eint", rmi4_data);
	if (retval < 0) {
		TPD_ERR("irq request failed\n");	
 	}

	synaptics_rmi4_irq_enable_soft(rmi4_data, true);
	synaptics_i2c_resume(NULL);
	mutex_unlock(&rmi4_data->rmi4_func_mutex);
	return;
}
#endif



static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{ TPD_DEVICE, 0},
	{},
};

static const struct of_device_id synaptics_rmi4_i2c_of_match[] = {
	{.compatible = "mediatek,synaptics_td4322"},
	{},
};



static struct i2c_driver synaptics_rmi4_i2c_driver = {
	.probe = synaptics_rmi4_probe,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	.remove = __devexit_p(synaptics_rmi4_remove),
#else
	.remove = synaptics_rmi4_remove,
#endif
	.id_table	= synaptics_rmi4_id_table,
    .driver     = {
        .name   = TPD_DEVICE,
        .of_match_table =  synaptics_rmi4_i2c_of_match,
        //.pm = &synaptics_pm_ops,
    },
};

static int synaptics_rmi4_platform_init(struct i2c_client* client){
	int ret = 0;

	of_get_synaptic_platform_data(client, &hw_res);

	//get the gpio configs
	hw_res.boot_mode = get_boot_mode();
	if (hw_res.boot_mode == 3 || hw_res.boot_mode == 7) {
		hw_res.boot_mode = NORMAL_BOOT;
	}

    hw_res.syn_pinctrl1 = devm_pinctrl_get(&client->dev);
    if (IS_ERR(hw_res.syn_pinctrl1)) {
        ret = PTR_ERR(hw_res.syn_pinctrl1);
        TPD_INFO("can not find touch pintrl1");
        return ret;
    }

    hw_res.syn_eint_as_int = pinctrl_lookup_state(hw_res.syn_pinctrl1, "state_td4322_as_int");
    if (IS_ERR(hw_res.syn_eint_as_int)) {
        ret = PTR_ERR(hw_res.syn_eint_as_int);
        TPD_INFO("can not find touch state_eint_as_int\n");
        return ret;
    }

	hw_res.syn_eint_as_input = pinctrl_lookup_state(hw_res.syn_pinctrl1, "state_td4322_as_input");
	if (IS_ERR(hw_res.syn_eint_as_input)) {
		ret = PTR_ERR(hw_res.syn_eint_as_input);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	hw_res.syn_vio18_off = pinctrl_lookup_state(hw_res.syn_pinctrl1, "td4322_power_vio18_off");
	if (IS_ERR(hw_res.syn_vio18_off)) {
		ret = PTR_ERR(hw_res.syn_vio18_off);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	hw_res.syn_vio18_on = pinctrl_lookup_state(hw_res.syn_pinctrl1, "td4322_power_vio18_on");
	if (IS_ERR(hw_res.syn_vio18_on)) {
		ret = PTR_ERR(hw_res.syn_vio18_on);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	hw_res.syn_rst_off = pinctrl_lookup_state(hw_res.syn_pinctrl1, "td4322_power_rst_off");
	if (IS_ERR(hw_res.syn_rst_off)) {
		ret = PTR_ERR(hw_res.syn_rst_off);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	hw_res.syn_rst_on = pinctrl_lookup_state(hw_res.syn_pinctrl1, "td4322_power_rst_on");
	if (IS_ERR(hw_res.syn_rst_on)) {
		ret = PTR_ERR(hw_res.syn_rst_on);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	hw_res.syn_id1_off = pinctrl_lookup_state(hw_res.syn_pinctrl1, "td4322_detect_id1_off");
	if (IS_ERR(hw_res.syn_id1_off)) {
		ret = PTR_ERR(hw_res.syn_id1_off);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	hw_res.syn_id1_on = pinctrl_lookup_state(hw_res.syn_pinctrl1, "td4322_detect_id1_on");
	if (IS_ERR(hw_res.syn_id1_on)) {
		ret = PTR_ERR(hw_res.syn_id1_on);
		TPD_INFO("can not find touch state_eint_as_input\n");
		return ret;
	}

	TPD_INFO("%s Exit\n", __func__);
	return 0;
}

static int synaptics_rmi4_detect_id(void)
{
/*add for mediatek internal use*/
	strcpy(lcm_name, "tianma");
	lcm = 3;
	pr_err("%s: force set lcm_name: %s, lcm=%d\n", __func__, lcm_name, lcm);
/*end*/
	if (lcm == 3 || lcm == 4 || lcm == 5)
		return 0;
	else
		return -ENODEV;
}

 /**
 * synaptics_rmi4_init()
 *
 * Called by the kernel during do_initcalls (if built-in)
 * or when the driver is loaded (if a module).
 *
 * This function registers the driver to the I2C subsystem.
 *
 */
static int __init synaptics_rmi4_init(void)
{
	int retval = 0;
	TPD_DEBUG("Init\n");

	//step1: detect ID
	RETURN_ON_FAIL(synaptics_rmi4_detect_id(), retval, "id not match, exit probe\n");

	if (i2c_add_driver(&synaptics_rmi4_i2c_driver) != 0) {
        TPD_INFO("unable to add i2c driver.\n");
        return -ENODEV;
    }
	return 0;
}


void lcd_wakeup_finger_protect(bool wakeup)
{
    //do nothing
}

int tp_gesture_enable_flag(void)
{
    return 0;
}


 /**
 * synaptics_rmi4_exit()
 *
 * Called by the kernel when the driver is unloaded.
 *
 * This funtion unregisters the driver from the I2C subsystem.
 *
 */
static void __exit synaptics_rmi4_exit(void)
{
	return;
}

/*
static void strUpcase(char *s, int len) {
	while( *s != '\0' && len > 0) {
		if ( *s >= 'a' && *s <= 'z') {
			*s -= 'a' - 'A';
		}
		s++;
		len--;
	}
}

static int synaptics_rmi4_lcm_id(char *lcm_char) {
	int i = 0;
	char *lcm_names[] = {
		"depute_new",
		"depute",
		"jdi",
		"tianma",
		"truly",
		"boe",
		NULL,
	};

	lcm = -1;
	while(lcm_names[i] != NULL) {
		if(strstr(lcm_char, lcm_names[i]) != NULL) {
			printk("%s %s and %s matched %d\n", __func__, lcm_char, lcm_names[i], i);
			lcm = i;
			strcpy(lcm_name, lcm_names[i]);
			strUpcase(lcm_name, strlen(lcm_name));
			break;
		}
		i++;
	}

	if (lcm == -1) {
		printk("%s match failed\n", __func__);
	}
	return 0;
}

__setup("lcm=", synaptics_rmi4_lcm_id);
*/
module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Touch Driver");
MODULE_LICENSE("GPL v2");
