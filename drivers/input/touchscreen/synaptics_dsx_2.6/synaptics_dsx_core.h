/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2016 The Linux Foundation. All rights reserved.
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

#ifndef _SYNAPTICS_DSX_RMI4_H_
#define _SYNAPTICS_DSX_RMI4_H_

#define SYNAPTICS_DS4 (1 << 0)
#define SYNAPTICS_DS5 (1 << 1)
#define SYNAPTICS_DSX_DRIVER_PRODUCT (SYNAPTICS_DS4 | SYNAPTICS_DS5)
#define SYNAPTICS_DSX_DRIVER_VERSION 0x2061

#include <linux/version.h>
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#if defined(CONFIG_SECURE_TOUCH_SYNAPTICS_DSX_V26)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38))
#define KERNEL_ABOVE_2_6_38
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
#define KERNEL_ABOVE_3_6
#endif

#ifdef KERNEL_ABOVE_2_6_38
#define sstrtoul(...) kstrtoul(__VA_ARGS__)
#else
#define sstrtoul(...) strict_strtoul(__VA_ARGS__)
#endif

#define PDT_PROPS (0X00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x00D0)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)
#define ADDRESS_WORD_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1A)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F35 (0x35)
#define SYNAPTICS_RMI4_F38 (0x38)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)
#define SYNAPTICS_RMI4_FDB (0xDB)

#define PRODUCT_INFO_SIZE 2
#define PRODUCT_ID_SIZE 10
#define BUILD_ID_SIZE 3

#define F12_FINGERS_TO_SUPPORT 10
#define F12_NO_OBJECT_STATUS 0x00
#define F12_FINGER_STATUS 0x01
#define F12_ACTIVE_STYLUS_STATUS 0x02
#define F12_PALM_STATUS 0x03
#define F12_HOVERING_FINGER_STATUS 0x05
#define F12_GLOVED_FINGER_STATUS 0x06
#define F12_NARROW_OBJECT_STATUS 0x07
#define F12_HAND_EDGE_STATUS 0x08
#define F12_COVER_STATUS 0x0A
#define F12_STYLUS_STATUS 0x0B
#define F12_ERASER_STATUS 0x0C
#define F12_SMALL_OBJECT_STATUS 0x0D

#define F12_GESTURE_DETECTION_LEN 5

#define MAX_NUMBER_OF_BUTTONS 4
#define MAX_INTR_REGISTERS 4

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE   "pmx_ts_release"
enum exp_fn {
	RMI_DEV = 0,
	RMI_FW_UPDATER,
	RMI_TEST_REPORTING,
	RMI_PROXIMITY,
	RMI_ACTIVE_PEN,
	RMI_GESTURE,
	RMI_VIDEO,
	RMI_DEBUG,
	RMI_LAST,
};

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT entry
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_version: version of function
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	union {
		struct {
			unsigned char query_base_addr;
			unsigned char cmd_base_addr;
			unsigned char ctrl_base_addr;
			unsigned char data_base_addr;
			unsigned char intr_src_count:3;
			unsigned char reserved_1:2;
			unsigned char fn_version:2;
			unsigned char reserved_2:1;
			unsigned char fn_number;
		} __packed;
		unsigned char data[6];
	};
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for command registers
 * @ctrl_base: 16-bit base address for control registers
 * @data_base: 16-bit base address for data registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

/*
 * struct synaptics_rmi4_f11_extra_data - extra data of F$11
 * @data38_offset: offset to F11_2D_DATA38 register
 */
struct synaptics_rmi4_f11_extra_data {
	unsigned char data38_offset;
};

/*
 * struct synaptics_rmi4_f12_extra_data - extra data of F$12
 * @data1_offset: offset to F12_2D_DATA01 register
 * @data4_offset: offset to F12_2D_DATA04 register
 * @data15_offset: offset to F12_2D_DATA15 register
 * @data15_size: size of F12_2D_DATA15 register
 * @data15_data: buffer for reading F12_2D_DATA15 register
 * @data23_offset: offset to F12_2D_DATA23 register
 * @data23_size: size of F12_2D_DATA23 register
 * @data23_data: buffer for reading F12_2D_DATA23 register
 * @ctrl20_offset: offset to F12_2D_CTRL20 register
 */
struct synaptics_rmi4_f12_extra_data {
	unsigned char data1_offset;
	unsigned char data4_offset;
	unsigned char data15_offset;
	unsigned char data15_size;
	unsigned char data15_data[(F12_FINGERS_TO_SUPPORT + 7) / 8];
	unsigned char data23_offset;
	unsigned char data23_size;
	unsigned char data23_data[F12_FINGERS_TO_SUPPORT];
	unsigned char ctrl20_offset;
};

/*
 * struct synaptics_rmi4_fn - RMI function handler
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 * @extra: pointer to extra data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
	void *extra;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: RMI protocol major version number
 * @version_minor: RMI protocol minor version number
 * @manufacturer_id: manufacturer ID
 * @product_props: product properties
 * @product_info: product information
 * @product_id_string: product ID
 * @build_id: firmware build ID
 * @support_fn_list: linked list for function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[PRODUCT_INFO_SIZE];
	unsigned char product_id_string[PRODUCT_ID_SIZE + 1];
	unsigned char build_id[BUILD_ID_SIZE];
	struct list_head support_fn_list;
};

/*
 * struct synaptics_rmi4_data - RMI4 device instance data
 * @pdev: pointer to platform device
 * @input_dev: pointer to associated input device
 * @stylus_dev: pointer to associated stylus device
 * @hw_if: pointer to hardware interface data
 * @rmi4_mod_info: device information
 * @board_prop_dir: /sys/board_properties directory for virtual key map file
 * @pwr_reg: pointer to regulator for power control
 * @bus_reg: pointer to regulator for bus pullup control
 * @rmi4_reset_mutex: mutex for software reset
 * @rmi4_report_mutex: mutex for input event reporting
 * @rmi4_io_ctrl_mutex: mutex for communication interface I/O
 * @rmi4_exp_init_mutex: mutex for expansion function module initialization
 * @rb_work: work for rebuilding input device
 * @rb_workqueue: workqueue for rebuilding input device
 * @fb_notifier: framebuffer notifier client
 * @reset_work: work for issuing reset after display framebuffer ready
 * @reset_workqueue: workqueue for issuing reset after display framebuffer ready
 * @early_suspend: early suspend power management
 * @current_page: current RMI page for register access
 * @button_0d_enabled: switch for enabling 0d button support
 * @num_of_tx: number of Tx channels for 2D touch
 * @num_of_rx: number of Rx channels for 2D touch
 * @num_of_fingers: maximum number of fingers for 2D touch
 * @max_touch_width: maximum touch width
 * @report_enable: input data to report for F$12
 * @no_sleep_setting: default setting of NoSleep in F01_RMI_CTRL00 register
 * @gesture_detection: detected gesture type and properties
 * @intr_mask: interrupt enable mask
 * @button_txrx_mapping: Tx Rx mapping of 0D buttons
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f$01
 * @f01_cmd_base_addr: command base address for f$01
 * @f01_ctrl_base_addr: control base address for f$01
 * @f01_data_base_addr: data base address for f$01
 * @firmware_id: firmware build ID
 * @irq: attention interrupt
 * @sensor_max_x: maximum x coordinate for 2D touch
 * @sensor_max_y: maximum y coordinate for 2D touch
 * @flash_prog_mode: flag to indicate flash programming mode status
 * @irq_enabled: flag to indicate attention interrupt enable status
 * @fingers_on_2d: flag to indicate presence of fingers in 2D area
 * @suspend: flag to indicate whether in suspend state
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @stay_awake: flag to indicate whether to stay awake during suspend
 * @fb_ready: flag to indicate whether display framebuffer in ready state
 * @f11_wakeup_gesture: flag to indicate support for wakeup gestures in F$11
 * @f12_wakeup_gesture: flag to indicate support for wakeup gestures in F$12
 * @enable_wakeup_gesture: flag to indicate usage of wakeup gestures
 * @wedge_sensor: flag to indicate use of wedge sensor
 * @report_pressure: flag to indicate reporting of pressure data
 * @stylus_enable: flag to indicate reporting of stylus data
 * @eraser_enable: flag to indicate reporting of eraser data
 * @external_afe_buttons: flag to indicate presence of external AFE buttons
 * @reset_device: pointer to device reset function
 * @irq_enable: pointer to interrupt enable function
 * @sleep_enable: pointer to sleep enable function
 * @report_touch: pointer to touch reporting function
 */
struct synaptics_rmi4_data {
	struct platform_device *pdev;
	struct input_dev *input_dev;
	struct input_dev *stylus_dev;
	const struct synaptics_dsx_hw_interface *hw_if;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct kobject *board_prop_dir;
	struct regulator *pwr_reg;
	struct regulator *bus_reg;
	struct mutex rmi4_reset_mutex;
	struct mutex rmi4_report_mutex;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex rmi4_exp_init_mutex;
	struct delayed_work rb_work;
	struct workqueue_struct *rb_workqueue;
#ifdef CONFIG_FB
	struct work_struct fb_notify_work;
	struct notifier_block fb_notifier;
	struct work_struct reset_work;
	struct workqueue_struct *reset_workqueue;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	unsigned char current_page;
	unsigned char button_0d_enabled;
	unsigned char num_of_tx;
	unsigned char num_of_rx;
	unsigned char num_of_fingers;
	unsigned char max_touch_width;
	unsigned char report_enable;
	unsigned char no_sleep_setting;
	unsigned char gesture_detection[F12_GESTURE_DETECTION_LEN];
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned char *button_txrx_mapping;
	unsigned short num_of_intr_regs;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned int firmware_id;
	int irq;
	int sensor_max_x;
	int sensor_max_y;
	bool flash_prog_mode;
	bool irq_enabled;
	bool fingers_on_2d;
	bool suspend;
	bool sensor_sleep;
	bool stay_awake;
	bool fb_ready;
	bool f11_wakeup_gesture;
	bool f12_wakeup_gesture;
	bool wakeup_gesture_en;
	bool enable_wakeup_gesture;
	bool wedge_sensor;
	bool report_pressure;
	bool stylus_enable;
	bool eraser_enable;
	bool external_afe_buttons;
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data,
			bool rebuild);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable,
			bool attn_only);
	void (*sleep_enable)(struct synaptics_rmi4_data *rmi4_data,
			bool enable);
	void (*report_touch)(struct synaptics_rmi4_data *rmi4_data,
			struct synaptics_rmi4_fn *fhandler);
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
#if defined(CONFIG_SECURE_TOUCH_SYNAPTICS_DSX_V26)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_irq_processed;
	bool st_initialized;
	struct clk *core_clk;
	struct clk *iface_clk;
#endif
};

struct synaptics_dsx_bus_access {
	unsigned char type;
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
		unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
		unsigned char *data, unsigned short length);
#if defined(CONFIG_SECURE_TOUCH_SYNAPTICS_DSX_V26)
	int (*get)(struct synaptics_rmi4_data *rmi4_data);
	void (*put)(struct synaptics_rmi4_data *rmi4_data);
#endif
};

struct synaptics_dsx_hw_interface {
	struct synaptics_dsx_board_data *board_data;
	const struct synaptics_dsx_bus_access *bus_access;
	int (*bl_hw_init)(struct synaptics_rmi4_data *rmi4_data);
	int (*ui_hw_init)(struct synaptics_rmi4_data *rmi4_data);
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	int (*init)(struct synaptics_rmi4_data *rmi4_data);
	void (*remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*reset)(struct synaptics_rmi4_data *rmi4_data);
	void (*reinit)(struct synaptics_rmi4_data *rmi4_data);
	void (*early_suspend)(struct synaptics_rmi4_data *rmi4_data);
	void (*suspend)(struct synaptics_rmi4_data *rmi4_data);
	void (*resume)(struct synaptics_rmi4_data *rmi4_data);
	void (*late_resume)(struct synaptics_rmi4_data *rmi4_data);
	void (*attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
};

int synaptics_rmi4_bus_init_v26(void);

void synaptics_rmi4_bus_exit_v26(void);

void synaptics_rmi4_new_function(struct synaptics_rmi4_exp_fn *exp_fn_module,
		bool insert);

int synaptics_fw_updater(const unsigned char *fw_data);

static inline int synaptics_rmi4_reg_read(
		struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr,
		unsigned char *data,
		unsigned short len)
{
	return rmi4_data->hw_if->bus_access->read(rmi4_data, addr, data, len);
}

static inline int synaptics_rmi4_reg_write(
		struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr,
		unsigned char *data,
		unsigned short len)
{
	return rmi4_data->hw_if->bus_access->write(rmi4_data, addr, data, len);
}

#if defined(CONFIG_SECURE_TOUCH_SYNAPTICS_DSX_V26)
static inline int synaptics_rmi4_bus_get(struct synaptics_rmi4_data *rmi4_data)
{
	return rmi4_data->hw_if->bus_access->get(rmi4_data);
}
static inline void synaptics_rmi4_bus_put(struct synaptics_rmi4_data *rmi4_data)
{
	rmi4_data->hw_if->bus_access->put(rmi4_data);
}
#endif

static inline int secure_memcpy(unsigned char *dest, unsigned int dest_size,
		const unsigned char *src, unsigned int src_size,
		unsigned int count)
{
	if (dest == NULL || src == NULL)
		return -EINVAL;

	if (count > dest_size || count > src_size)
		return -EINVAL;

	memcpy((void *)dest, (const void *)src, count);

	return 0;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}

#endif
