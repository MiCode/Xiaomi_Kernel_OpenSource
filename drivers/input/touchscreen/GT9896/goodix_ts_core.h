/*
 * Goodix Touchscreen Driver
 * Core layer of touchdriver architecture.
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Authors: Wang Yafei <wangyafei@goodix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#ifndef _GOODIX_TS_CORE_H_
#define _GOODIX_TS_CORE_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <asm/arch_timer.h>
#include <linux/pm_qos.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#endif
#include <linux/power_supply.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_DRM
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define GOODIX_FLASH_CONFIG_WITH_ISP	1
/* macros definition */
#define GOODIX_CORE_DRIVER_NAME			"goodix_ts"
#define GOODIX_PEN_DRIVER_NAME			"goodix_ts,pen"
#define GOODIX_DRIVER_VERSION			"v1.4.3.1"
#define GOODIX_BUS_RETRY_TIMES			3
#define GOODIX_MAX_TOUCH				10
#define GOODIX_CFG_MAX_SIZE				4096
#define GOODIX_ESD_TICK_WRITE_DATA		0xAA
#define GOODIX_PID_MAX_LEN				8
#define GOODIX_VID_MAX_LEN				8

#define GOODIX_DEFAULT_CFG_NAME			"goodix_config.cfg"

#define IC_TYPE_NONE					0
#define IC_TYPE_NORMANDY				1
#define IC_TYPE_NANJING					2
#define IC_TYPE_YELLOWSTONE				3

#define GOODIX_TOUCH_EVENT				0x80
#define GOODIX_REQUEST_EVENT			0x40
#define GOODIX_GESTURE_EVENT			0x20
#define GOODIX_HOTKNOT_EVENT			0x10

#define GOODIX_PEN_MAX_PRESSURE			4096
#define GOODIX_MAX_TP_KEY				4
#define GOODIX_MAX_PEN_KEY				2
#define GOODIX_LOCKDOWN_SIZE			8
#define KEY_GOTO						0x162
#define BTN_INFO						0x152
#define GTP_RESULT_INVALID 0
#define GTP_RESULT_PASS 2
#define GTP_RESULT_FAIL 1

#define GTP_GAME_CMD_ADD				0x4160
#define GTP_GAME_CMD					0x0E


/*
 * struct goodix_module - external modules container
 * @head: external modules list
 * @initilized: whether this struct is initilized
 * @mutex: mutex lock
 * @count: current number of registered external module
 * @wq: workqueue to do register work
 * @core_data: core_data pointer
 */
struct goodix_module {
	struct list_head head;
	bool initilized;
	struct mutex mutex;
	unsigned int count;
	struct workqueue_struct *wq;
	struct completion core_comp;
	struct goodix_ts_core *core_data;
};

/*
 * struct goodix_ts_board_data -  board data
 * @avdd_name: name of analoy regulator
 * @reset_gpio: reset gpio number
 * @irq_gpio: interrupt gpio number
 * @irq_flag: irq trigger type
 * @power_on_delay_us: power on delay time (us)
 * @power_off_delay_us: power off delay time (us)
 * @swap_axis: whether swaw x y axis
 * @panel_max_x/y/w/p: resolution and size
 * @panel_max_key: max supported keys
 * @pannel_key_map: key map
 * @fw_name: name of the firmware image
 */
struct goodix_ts_board_data {
	char avdd_name[24];
	unsigned int reset_gpio;
	unsigned int irq_gpio;
	int irq;
	unsigned int  irq_flags;

	unsigned int power_on_delay_us;
	unsigned int power_off_delay_us;

	unsigned int swap_axis;
	unsigned int panel_max_x;
	unsigned int panel_max_y;
	unsigned int panel_max_w; /*major and minor*/
	unsigned int panel_max_p; /*pressure*/
	unsigned int panel_max_key;
	unsigned int panel_key_map[GOODIX_MAX_TP_KEY];
	unsigned int x2x;
	unsigned int y2y;
	bool pen_enable;
	unsigned int tp_key_num;
	/*add end*/

	const char *fw_name;
	const char *cfg_bin_name;
	bool esd_default_on;
};

enum goodix_fw_update_mode {
	UPDATE_MODE_DEFAULT = 0,
	UPDATE_MODE_FORCE = (1<<0), /* force update mode */

	UPDATE_MODE_BLOCK = (1<<1), /* update in block mode */

	UPDATE_MODE_FLASH_CFG = (1<<2), /* reflash config */

	UPDATE_MODE_SRC_SYSFS = (1<<4), /* firmware file from sysfs */
	UPDATE_MODE_SRC_HEAD = (1<<5), /* firmware file from head file */
	UPDATE_MODE_SRC_REQUEST = (1<<6), /* request firmware */
	UPDATE_MODE_SRC_ARGS = (1<<7), /* firmware data from function args */
};

enum goodix_cfg_init_state {
	TS_CFG_UNINITALIZED,
	TS_CFG_STABLE,
	TS_CFG_TEMP,
};

/*
 * struct goodix_ts_config - chip config data
 * @initialized: cfg init state, 0 uninit,
 * 	1 init with stable config, 2 init with temp config.
 * @name: name of this config
 * @lock: mutex
 * @reg_base: register base of config data
 * @length: bytes of the config
 * @delay: delay time after sending config
 * @data: config data buffer
 */
struct goodix_ts_config {
	int initialized;
	char name[24];
	struct mutex lock;
	unsigned int reg_base;
	unsigned int length;
	unsigned int delay; /*ms*/
	unsigned char data[GOODIX_CFG_MAX_SIZE];
};

/*
 * struct goodix_ts_cmd - command package
 * @initialized: whether initialized
 * @cmd_reg: command register
 * @length: command length in bytes
 * @cmds: command data
 */
#pragma pack(4)
struct goodix_ts_cmd {
	u32 initialized;
	u32 cmd_reg;
	u32 length;
	u8 cmds[8];
	};
#pragma pack()

/* interrupt event type */
enum ts_event_type {
	EVENT_INVALID = 0,
	EVENT_TOUCH = (1 << 0), /* finger touch event */
	EVENT_PEN = (1 << 1), /* pen event */
	EVENT_REQUEST = (1 << 2),
};

/* requset event type */
enum ts_request_type {
	REQUEST_INVALID,
	REQUEST_CONFIG,
	REQUEST_BAKREF,
	REQUEST_RESET,
	REQUEST_MAINCLK,
};

/* notifier event */
enum ts_notify_event {
	NOTIFY_FWUPDATE_START,
	NOTIFY_FWUPDATE_FAILED,
	NOTIFY_FWUPDATE_SUCCESS,
	NOTIFY_SUSPEND,
	NOTIFY_RESUME,
	NOTIFY_ESD_OFF,
	NOTIFY_ESD_ON,
	NOTIFY_CFG_BIN_FAILED,
	NOTIFY_CFG_BIN_SUCCESS,
};

/* suspend status*/
enum tp_suspend_stat {
	TP_NO_SUSPEND,
	TP_GESTURE,
	TP_SLEEP,
};

enum touch_point_status {
	TS_NONE,
	TS_RELEASE,
	TS_TOUCH,
};
/* coordinate package */
struct goodix_ts_coords {
	int status; /* NONE, RELEASE, TOUCH */
	unsigned int x, y, w, p, area, overlapping_area;
};

struct goodix_pen_coords {
	int status; /* NONE, RELEASE, TOUCH */
	int tool_type;  /* BTN_TOOL_RUBBER BTN_TOOL_PEN */
	unsigned int x, y, p;
	signed char tilt_x;
	signed char tilt_y;
};

struct goodix_ts_key {
	int status;
	int code;
};

/* touch event data */
struct goodix_touch_data {
	int touch_num;
	struct goodix_ts_coords coords[GOODIX_MAX_TOUCH];
	struct goodix_ts_key keys[GOODIX_MAX_TP_KEY];
};

struct goodix_pen_data {
	struct goodix_pen_coords coords;
	struct goodix_ts_key keys[GOODIX_MAX_PEN_KEY];
};

/*
 * struct goodix_ts_event - touch event struct
 * @event_type: touch event type, touch data or
 * request event
 * @event_data: event data
 */
struct goodix_ts_event {
	enum ts_event_type event_type;
	struct goodix_touch_data touch_data;
	struct goodix_pen_data pen_data;
};

/*
 * struct goodix_ts_version - firmware version
 * @valid: whether these infomation is valid
 * @pid: product id string
 * @vid: firmware version code
 * @cid: customer id code
 * @sensor_id: sendor id
 */
struct goodix_ts_version {
	bool valid;
	char pid[GOODIX_PID_MAX_LEN];
	char vid[GOODIX_VID_MAX_LEN];
	u8 cid;
	u8 sensor_id;
};

struct goodix_ts_regs {
	u16 cfg_send_flag;

	u16 version_base;
	u8 version_len;

	u16 pid;
	u8  pid_len;

	u16 vid;
	u8 vid_len;

	u16 sensor_id;
	u8 sensor_id_mask;

	u16 fw_mask;
	u16 fw_status;
	u16 cfg_addr;
	u16 esd;
	u16 command;
	u16 coor;
	u16 gesture;
	u16 fw_request;
	u16 proximity;
};

enum goodix_cfg_bin_state {
	CFG_BIN_STATE_UNINIT, /* config bin uninit */
	CFG_BIN_STATE_ERROR, /* config bin encounter fatal error */
	CFG_BIN_STATE_INITIALIZED, /* config bin init success */
	CFG_BIN_STATE_TEMP, /* config bin need reparse */
};

/*
 * struct goodix_ts_device - ts device data
 * @name: device name
 * @version: reserved
 * @bus_type: i2c or spi
 * @ic_type: normandy or yellowstone
 * @cfg_bin_state: see enum goodix_cfg_bin_state
 * @fw_uptodate: set to 1 after do fw update
 * @board_data: board data obtained from dts
 * @normal_cfg: normal config data
 * @highsense_cfg: high sense config data
 * @hw_ops: hardware operations
 * @chip_version: firmware version infomation
 * @sleep_cmd: sleep commang
 * @gesture_cmd: gesture command
 * @dev: device pointer,may be a i2c or spi device
 * @of_node: device node
 */
struct goodix_ts_device {
	char *name;
	int version;
	int bus_type;
	int ic_type;
	int cfg_bin_state;
	struct mutex charger_mutex;
	struct goodix_ts_regs reg;
	struct mutex report_mutex;
	struct goodix_ts_board_data board_data;
	struct goodix_ts_config normal_cfg;
	struct goodix_ts_config highsense_cfg;
	const struct goodix_ts_hw_ops *hw_ops;

	struct goodix_ts_version chip_version;
	struct device *dev;
};

/*
 * struct goodix_ts_hw_ops -  hardware opeartions
 * @init: hardware initialization
 * @reset: hardware reset
 * @read: read data from touch device
 * @write: write data to touch device
 * @send_cmd: send command to touch device
 * @send_config: send configuration data
 * @read_version: read firmware version
 * @event_handler: touch event handler
 * @suspend: put touch device into low power mode
 * @resume: put touch device into working mode
 */
struct goodix_ts_hw_ops {

	int (*init)(struct goodix_ts_device *dev);
	int (*dev_confirm)(struct goodix_ts_device *ts_dev);
	int (*reset)(struct goodix_ts_device *dev);
	int (*read)(struct goodix_ts_device *dev, unsigned int addr,
			unsigned char *data, unsigned int len);
	int (*write)(struct goodix_ts_device *dev, unsigned int addr,
			unsigned char *data, unsigned int len);
	int (*read_trans)(struct goodix_ts_device *dev, unsigned int addr,
			unsigned char *data, unsigned int len);
	int (*write_trans)(struct goodix_ts_device *dev, unsigned int addr,
			unsigned char *data, unsigned int len);
	int (*send_cmd)(struct goodix_ts_device *dev, struct goodix_ts_cmd *cmd);
	int (*send_config)(struct goodix_ts_device *dev,
			struct goodix_ts_config *config);
	int (*read_config)(struct goodix_ts_device *dev, u8 *config_data);
	int (*read_version)(struct goodix_ts_device *dev,
			struct goodix_ts_version *version);
	int (*event_handler)(struct goodix_ts_device *dev,
			struct goodix_ts_event *ts_event);
	int (*check_hw)(struct goodix_ts_device *dev);
	int (*suspend)(struct goodix_ts_device *dev);
	int (*resume)(struct goodix_ts_device *dev);
};

/*
 * struct goodix_ts_esd - esd protector structure
 * @esd_work: esd delayed work
 * @esd_on: 1 - turn on esd protection, 0 - turn
 *  off esd protection
 */
struct goodix_ts_esd {
	struct delayed_work esd_work;
	struct notifier_block esd_notifier;
	struct goodix_ts_core *ts_core;
	atomic_t esd_on;
};

/*
 * struct godix_ts_core - core layer data struct
 * @initialized: indicate core state, 1 ok, 0 bad
 * @pdev: core layer platform device
 * @ts_dev: hardware layer touch device
 * @input_dev: input device
 * @avdd: analog regulator
 * @pinctrl: pinctrl handler
 * @pin_sta_active: active/normal pin state
 * @pin_sta_suspend: suspend/sleep pin state
 * @ts_event: touch event data struct
 * @power_on: power on/off flag
 * @irq: irq number
 * @irq_enabled: irq enabled/disabled flag
 * @suspended: suspend/resume flag
 * @ts_notifier: generic notifier
 * @ts_esd: esd protector structure
 * @fb_notifier: framebuffer notifier
 * @early_suspend: early suspend
 */
struct goodix_ts_core {
	atomic_t initialized;
	struct platform_device *pdev;
	struct goodix_ts_device *ts_dev;
	struct input_dev *input_dev;
	struct input_dev *pen_dev;

	struct regulator *avdd;
#ifdef CONFIG_PINCTRL
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_sta_active;
	struct pinctrl_state *pin_sta_suspend;
	struct pinctrl_state *pin_sta_boot;
#endif
	struct goodix_ts_event ts_event;
	unsigned long touch_id;
	unsigned long event_status;
	unsigned char lockdown_info[GOODIX_LOCKDOWN_SIZE];
	bool sleep_finger;

	int power_on;
	int irq;
	size_t irq_trig_cnt;

	atomic_t irq_enabled;
	atomic_t suspended;

	struct workqueue_struct *event_wq;
	struct workqueue_struct *touch_feature_wq;
	struct workqueue_struct *touch_gesture_wq;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct work_struct cmd_update_work;
	struct work_struct gesture_work;
	struct notifier_block ts_notifier;
	struct goodix_ts_esd ts_esd;

	struct notifier_block power_supply_notif;
	struct work_struct power_supply_work;
	struct power_supply *battery_psy;

	struct proc_dir_entry *tp_fw_version_proc;
	struct proc_dir_entry *tp_lockdown_info_proc;
	struct proc_dir_entry *tp_selftest_proc;
	struct proc_dir_entry *tp_data_dump_proc;

	struct dentry *debugfs;

#ifdef CONFIG_DRM
	struct notifier_block fb_notifier;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif

#ifdef CONFIG_TOUCHSCREEN_FOD
	int fod_status;
	int aod_status;
	int fod_icon_status;
	int nonui_status;
	int fod_pressed;
	int fod_test;
	char fod_area;
	struct class *gtp_tp_class;
	struct device *gtp_fod_dev;
#endif
	int result_type;
	struct mutex work_stat;
#ifdef CONFIG_PM
	bool tp_pm_suspend;
	struct completion pm_resume_completion;
#endif
	char gesture_enabled;
	atomic_t suspend_stat;
	int double_wakeup;
	int power_status;
	uint64_t start_time, end_time;
	struct pm_qos_request tp_qos_request;
};

/* external module structures */
enum goodix_ext_priority {
	EXTMOD_PRIO_RESERVED = 0,
	EXTMOD_PRIO_FWUPDATE,
	EXTMOD_PRIO_GESTURE,
	EXTMOD_PRIO_HOTKNOT,
	EXTMOD_PRIO_DBGTOOL,
	EXTMOD_PRIO_DEFAULT,
};

struct goodix_ext_module;
/* external module's operations callback */
struct goodix_ext_module_funcs {
	int (*init)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*exit)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*before_reset)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*after_reset)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*before_suspend)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*after_suspend)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*before_resume)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*after_resume)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
	int (*irq_event)(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module);
};

/*
 * struct goodix_ext_module - external module struct
 * @list: list used to link into modules manager
 * @name: name of external module
 * @priority: module priority vlaue, zero is invalid
 * @funcs: operations callback
 * @priv_data: private data region
 * @kobj: kobject
 * @work: used to queue one work to do registration
 */
struct goodix_ext_module {
	struct list_head list;
	char *name;
	enum goodix_ext_priority priority;
	const struct goodix_ext_module_funcs *funcs;
	void *priv_data;
	struct kobject kobj;
	struct work_struct work;
};

/*
 * struct goodix_ext_attribute - exteranl attribute struct
 * @attr: attribute
 * @show: show interface of external attribute
 * @store: store interface of external attribute
 */
struct goodix_ext_attribute {
	struct attribute attr;
	ssize_t (*show)(struct goodix_ext_module *, char *);
	ssize_t (*store)(struct goodix_ext_module *, const char *, size_t);
};

/* external attrs helper macro */
#define __EXTMOD_ATTR(_name, _mode, _show, _store)	{	\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show   = _show,	\
	.store  = _store,	\
}

/* external attrs helper macro, used to define external attrs */
#define DEFINE_EXTMOD_ATTR(_name, _mode, _show, _store)	\
static struct goodix_ext_attribute ext_attr_##_name = \
	__EXTMOD_ATTR(_name, _mode, _show, _store);

/*
 * get board data pointer
 */
static inline struct goodix_ts_board_data *board_data(
		struct goodix_ts_core *core)
{
	if (!core || !core->ts_dev)
		return NULL;
	return &(core->ts_dev->board_data);
}

/*
 * get touch device pointer
 */
static inline struct goodix_ts_device *ts_device(
		struct goodix_ts_core *core)
{
	if (!core)
		return NULL;
	return core->ts_dev;
}

/*
 * get touch hardware operations pointer
 */
static inline const struct goodix_ts_hw_ops *ts_hw_ops(
		struct goodix_ts_core *core)
{
	if (!core || !core->ts_dev)
		return NULL;
	return core->ts_dev->hw_ops;
}

/*
 * checksum helper functions
 * checksum can be u8/le16/be16/le32/be32 format
 * NOTE: the caller shoule be responsible for the
 * legality of @data and @size parameters, so be
 * careful when call these functions.
 */
static inline u8 checksum_u8(u8 *data, u32 size)
{
	u8 checksum = 0;
	u32 i;

	for (i = 0; i < size; i++)
		checksum += data[i];
	return checksum;
}

/* cal u8 data checksum for yellowston */
static inline u16 checksum_u8_ys(u8 *data, u32 size)
{
	u16 checksum = 0;
	u32 i;

	for (i = 0; i < size - 2; i++)
		checksum += data[i];
	return checksum - (data[size - 2] << 8 | data[size - 1]);
}

static inline u16 checksum_le16(u8 *data, u32 size)
{
	u16 checksum = 0;
	u32 i;

	for (i = 0; i < size; i += 2)
		checksum += le16_to_cpup((__le16 *)(data + i));
	return checksum;
}

static inline u16 checksum_be16(u8 *data, u32 size)
{
	u16 checksum = 0;
	u32 i;

	for (i = 0; i < size; i += 2)
		checksum += be16_to_cpup((__be16 *)(data + i));
	return checksum;
}

static inline u32 checksum_le32(u8 *data, u32 size)
{
	u32 checksum = 0;
	u32 i;

	for (i = 0; i < size; i += 4)
		checksum += le32_to_cpup((__le32 *)(data + i));
	return checksum;
}

static inline u32 checksum_be32(u8 *data, u32 size)
{
	u32 checksum = 0;
	u32 i;

	for (i = 0; i < size; i += 4)
		checksum += be32_to_cpup((__be32 *)(data + i));
	return checksum;
}

/*
 * define event action
 * EVT_xxx macros are used in opeartions callback
 * defined in @goodix_ext_module_funcs to control
 * the behaviors of event such as suspend/resume/irq_event.
 * generally there are two types of behaviors:
 *  1. you want the flow of this event be canceled,
 *  in this condition, you should return EVT_CANCEL_XXX in
 *	the operations callback.
 *	e.g. the firmware update module is updating
 *	the firmware, you want to cancel suspend flow,
 *	so you need to return EVT_CANCEL_SUSPEND in
 *	suspend callback function.
 *  2. you want the flow of this event continue, in
 *	this condition, you should return EVT_HANDLED in
 *	the callback function.
 * */
#define EVT_HANDLED				0
#define EVT_CONTINUE			0
#define EVT_CANCEL				1
#define EVT_CANCEL_IRQEVT		1
#define EVT_CANCEL_SUSPEND		1
#define EVT_CANCEL_RESUME		1
#define EVT_CANCEL_RESET		1

/*
 * errno define
 * Note:
 *	1. bus read/write functions defined in hardware
 *	  layer code(e.g. goodix_xxx_i2c.c) *must* return
 *	  -EBUS if failed to transfer data on bus.
 */
#define EBUS					1000
#define ETIMEOUT				1001
#define ECHKSUM					1002
#define EMEMCMP					1003

//#define CONFIG_GOODIX_DEBUG
/* log macro */
#define ts_info(fmt, arg...)	pr_info("[GTP-INF][%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define	ts_err(fmt, arg...)	pr_err("[GTP-ERR][%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define boot_log(fmt, arg...)	g_info(fmt, ##arg)
#ifdef CONFIG_GOODIX_DEBUG
#define ts_debug(fmt, arg...)	pr_info("[GTP-DBG][%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#else
#define ts_debug(fmt, arg...)	do {} while (0)
#endif


/**
 * goodix_register_ext_module - interface for external module
 * to register into touch core modules structure
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module);

/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module);

/**
 * goodix_ts_irq_enable - Enable/Disable a irq

 * @core_data: pointer to touch core data
 * enable: enable or disable irq
 * return: 0 ok, <0 failed
 */
int goodix_ts_irq_enable(struct goodix_ts_core *core_data, bool enable);

struct kobj_type *goodix_get_default_ktype(void);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v);


/**
 *  * goodix_ts_power_on - Turn on power to the touch device
 *   * @core_data: pointer to touch core data
 *    * return: 0 ok, <0 failed
 *     */
int goodix_ts_power_on(struct goodix_ts_core *core_data);

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *core_data);

int goodix_ts_irq_setup(struct goodix_ts_core *core_data);

int goodix_ts_esd_init(struct goodix_ts_core *core);

int goodix_ts_register_notifier(struct notifier_block *nb);

int goodix_ts_fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data);
int goodix_set_i2c_doze_mode(struct goodix_ts_device *dev, int enable);

void goodix_msg_printf(const char *fmt, ...);
#ifndef CONFIG_TOUCHSCREEN_GOODIX_GTX8_UPDATE
static inline int goodix_do_fw_update(int mode)
{
	return -1111;
}
#else
int goodix_do_fw_update(int mode);
#endif

extern int sync_read_rawdata( unsigned int reg,
		unsigned char *data, unsigned int len);

extern int goodix_tools_register(void);
extern int goodix_tools_unregister(void);

#define TS_RAWDATA_BUFF_MAX					2000
#define TS_RAWDATA_RESULT_MAX				100
struct ts_rawdata_info{
	int used_size; /* fill in rawdata size */
	u16 buff[TS_RAWDATA_BUFF_MAX];
	char result[TS_RAWDATA_RESULT_MAX];
};
extern int gtx8_get_rawdata(void *tsdev, struct ts_rawdata_info *info);
extern int gtx8_dump_data(void *tsdev, char *buf, int *buf_size);
extern void set_test_flag(int flag);

#endif
