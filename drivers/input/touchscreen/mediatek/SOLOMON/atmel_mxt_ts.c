/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011-2014 Atmel Corporation
 * Copyright (C) 2012 Google, Inc.
 *
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include "atmel_mxt_ts.h"
#include <linux/kthread.h>
#include <linux/of_irq.h>
#include <linux/suspend.h>
//#include <linux/irqchip/mt-eic.h>//delete by cassy
#include <linux/watchdog.h>
#include "tpd.h"
#include <linux/time.h>
#include <asm/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/byteorder/generic.h>
#include <linux/proc_fs.h>
//#include <mt_gpio.h>//delete by cassy
//#include <linux/rtpm_prio.h>//delete by cassy
//#include "mt_boot_common.h"//delete by cassy

#ifdef CONFIG_MTK_LEGACY
#include <cust_eint.h>
#include <pmic_drv.h>
#endif

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#ifdef CONFIG_OF_TOUCH
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#ifdef CONFIG_GTP_ICS_SLOT_REPORT
#include <linux/input/mt.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
//add by cassybegin
struct pinctrl *pinctrl2;
struct pinctrl_state *power_output1,*power_output0;
//#define MTK_POWER
//#define xiaomi_power
//add by cassy ed

#define AM(a, args...) printk(KERN_ERR "mxt %s %d " a, __func__, __LINE__, ##args);
//#define MAX1_WAKEUP_GESTURE_ENABLE
#define MXT_LOCKDOWN_OFFSET	4
#define MXT_LOCKDOWN_SIZE	8


/* Configuration file */
#define MXT_CFG_MAGIC		"OBP_RAW V1"
/* Registers */
#define MXT_OBJECT_START	0x07
#define MXT_OBJECT_SIZE		6
#define MXT_INFO_CHECKSUM_SIZE	3
#define MXT_MAX_BLOCK_WRITE	255
#define MXT_MAX_BLOCK_READ	255

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_USER_DATA_T38		38
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_PROCI_ACTIVE_STYLUS_T63	63
#define MXT_OBJ_NOISE_T72               72
#define MXT_UNLOCK_GESTURE_T81		81
#define MXT_TOUCH_SEQUENCE_PROCESSOR_T93	93
#define MXT_TOUCH_MULTITOUCHSCREEN_T100		100
#define MXT_TOUCH_SELF_CAPACITANCE_CONFIG_T111	111
#define MXT_SYMBOL_GESTURE_PROCESSOR_T115	115

/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

#define MXT_T6_CMD_PAGE_UP		0x01
#define MXT_T6_CMD_PAGE_DOWN		0x02
#define MXT_T6_CMD_DELTAS		0x10
#define MXT_T6_CMD_REFS			0x11
#define MXT_T6_CMD_DEVICE_ID		0x80
#define MXT_T6_CMD_TOUCH_THRESH		0xF4


/* Define for T6 status byte */
#define MXT_T6_STATUS_RESET	(1 << 7)
#define MXT_T6_STATUS_OFL	(1 << 6)
#define MXT_T6_STATUS_SIGERR	(1 << 5)
#define MXT_T6_STATUS_CAL	(1 << 4)
#define MXT_T6_STATUS_CFGERR	(1 << 3)
#define MXT_T6_STATUS_COMSERR	(1 << 2)

#define THRESHOLD_MAX 27500
#define THRESHOLD_MIN 19000

/* MXT_GEN_POWER_T7 field */
struct t7_config {
	u8 idle;
	u8 active;
} __packed;

#define MXT_POWER_CFG_RUN		0
#define MXT_POWER_CFG_DEEPSLEEP		1

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_T9_ORIENT		9
#define MXT_T9_RANGE		18

/* MXT_TOUCH_MULTI_T9 status */
#define MXT_T9_UNGRIP		(1 << 0)
#define MXT_T9_SUPPRESS		(1 << 1)
#define MXT_T9_AMP		(1 << 2)
#define MXT_T9_VECTOR		(1 << 3)
#define MXT_T9_MOVE		(1 << 4)
#define MXT_T9_RELEASE		(1 << 5)
#define MXT_T9_PRESS		(1 << 6)
#define MXT_T9_DETECT		(1 << 7)

struct t9_range {
	u16 x;
	u16 y;
} __packed;

/* MXT_TOUCH_MULTI_T9 orient */
#define MXT_T9_ORIENT_SWITCH	(1 << 0)

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1
#define MXT_COMMS_RETRIGEN      (1 << 6)

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_RESET_VALUE		0x01
#define MXT_BACKUP_VALUE	0x55
#define MXT_MUTUAL_REFERENCE_MODE	0x11

/* Define for MXT_PROCI_TOUCHSUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* T47 Stylus */
#define MXT_TOUCH_MAJOR_T47_STYLUS	1

/* T63 Stylus */
#define MXT_T63_STYLUS_PRESS	(1 << 0)
#define MXT_T63_STYLUS_RELEASE	(1 << 1)
#define MXT_T63_STYLUS_MOVE		(1 << 2)
#define MXT_T63_STYLUS_SUPPRESS	(1 << 3)

#define MXT_T63_STYLUS_DETECT	(1 << 4)
#define MXT_T63_STYLUS_TIP		(1 << 5)
#define MXT_T63_STYLUS_ERASER	(1 << 6)
#define MXT_T63_STYLUS_BARREL	(1 << 7)

#define MXT_T63_STYLUS_PRESSURE_MASK	0x3F

/* T100 Multiple Touch Touchscreen */
#define MXT_T100_CTRL		0
#define MXT_T100_CFG1		1
#define MXT_T100_TCHAUX		3
#define MXT_T100_XRANGE		13
#define MXT_T100_YRANGE		24

#define MXT_T100_CFG_SWITCHXY	(1 << 5)

#define MXT_T100_TCHAUX_VECT	(1 << 0)
#define MXT_T100_TCHAUX_AMPL	(1 << 1)
#define MXT_T100_TCHAUX_AREA	(1 << 2)

#define MXT_T100_DETECT		(1 << 7)
#define MXT_T100_TYPE_MASK	0x70
#define MXT_T100_TYPE_STYLUS	0x20
/* T100 Touch status */
#define MXT_T100_CTRL_RPTEN	(1 << 1)
#define MXT_T100_CFG1_SWITCHXY	(1 << 5)

#define MXT_T100_EVENT_NONE	0
#define MXT_T100_EVENT_MOVE	1
#define MXT_T100_EVENT_UNSUP	2
#define MXT_T100_EVENT_SUP	3
#define MXT_T100_EVENT_DOWN	4
#define MXT_T100_EVENT_UP	5
#define MXT_T100_EVENT_UNSUPSUP	6
#define MXT_T100_EVENT_UNSUPUP	7
#define MXT_T100_EVENT_DOWNSUP	8
#define MXT_T100_EVENT_DOWNUP	9

#define MXT_T100_TYPE_RESERVED	0
#define MXT_T100_TYPE_FINGER	1
#define MXT_T100_TYPE_PASSIVE_STYLUS	2
#define MXT_T100_TYPE_ACTIVE_STYLUS	3
#define MXT_T100_TYPE_HOVERING_FINGER	4
#define MXT_T100_TYPE_HOVERING_GLOVE	5
#define MXT_T100_TYPE_EDGE_TOUCH	7

#define MXT_T100_DETECT		(1 << 7)
#define MXT_T100_VECT		(1 << 0)
#define MXT_T100_AMPL		(1 << 1)
#define MXT_T100_AREA		(1 << 2)
#define MXT_T100_PEAK		(1 << 4)

#define MXT_T100_SUP		(1 << 6)


/* Delay times */
#define MXT_BACKUP_TIME		50	/* msec */
//#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_TIME		500	/* msec */
#define MXT_RESET_TIMEOUT	1000	/* msec */
#define MXT_CRC_TIMEOUT		1000	/* msec */
#define MXT_FW_RESET_TIME	3000	/* msec */
//#define MXT_FW_CHG_TIMEOUT	300	/* msec */
#define MXT_FW_CHG_TIMEOUT	300	/* msec */
#define MXT_WAKEUP_TIME		25	/* msec */
#define MXT_REGULATOR_DELAY	5	/* msec */
#define MXT_CHG_DELAY	        100	/* msec */
#define MXT_POWERON_DELAY	150	/* msec */
#define MXT_CALIBRATE_DELAY	100	/* msec */

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f
#define MXT_BOOT_EXTENDED_ID	(1 << 5)
#define MXT_BOOT_ID_MASK	0x1f

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

#define MXT_PIXELS_PER_MM	20

#define DEBUG_MSG_MAX		200

/* UPLOAD FW */
#define MXT_FW_NAME_SIZE	17
#define MXT_CFG_OFFSET_TYPE	1
#define MXT_CFG_OFFSET_INSTANCE	3
#define MXT_CFG_OFFSET_SIZE	5
#define MXT_T100_DISABLE_MASK	0xfd
#define MXT_T100_ENABLE_MASK	0x2
#define MXT_T81_ENABLE_MASK	0x1
#define MXT_T81_DISABLE_MASK	0xfe
#define MXT_T38_INFO_SIZE	10

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
/* GESTURE */
#define MXT_GESTURE_UP			0x56
#define MXT_GESTURE_DOWN		0x55
#define MXT_GESTURE_LEFT		0x57
#define MXT_GESTURE_RIGHT		0x58
#define MXT_GESTURE_DOUBLE_CLICK_MASK	0x02

#define RIGHT    1
#define LEFT     2
#define DOWN     4
#define UP       8
#define DOUBLE   3
#endif

#define ADD_MINUS_PERSENT 30
#define TX_NUM 26
#define RX_NUM 15
#define PAGE 128
#define RAW_DATA_SIZE (TX_NUM * RX_NUM)

#define CUST_EINT_TOUCH_PANEL_NUM 		7
#define EINTF_TRIGGER_LOW 				0x00000008
#define CUST_EINTF_TRIGGER_LOW			EINTF_TRIGGER_LOW

#define GTP_RST_PORT    0
#define GTP_INT_PORT    1
#define GTP_enable_power_PORT 2//add by cassy

#define GTP_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define GTP_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)
#define GTP_GPIO_OUTPUT_ATMEL(pin, level) tpd_gpio_output_ATEML(pin, level)


#define TPD_I2C_ADDR		0x4a    //7 bits, without  r/w bit
#define TPD_POWER_SOURCE	//PMIC_APP_CAP_TOUCH_VDD
#define I2C_ACCESS_NO_REG   (1 << 4)  // no reg address, directly access i2c reg 
#define I2C_ACCESS_NO_CACHE   (1 << 5)  //no dma cache need
#define I2C_ACCESS_R_REG_FIXED   (1 << 0)   //don't mov reg address if read len is too long

/*Struct defination comes below*/
struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct t81_configuration{
	u8 ctrl;
	u8 distlsbyte;
	u8 distmsbyte;
	u8 startxmin;
	u8 startymin;
	u8 startxsize;
	u8 startysize;
	u8 endxmin;
	u8 endymin;
	u8 endxsize;
	u8 endysize;
	u8 movlimmin;
	u8 movlimmax;
	u8 movhyst;
	u8 maxarea;
	u8 maxnumtch;
	u8 angle;
	u8 unused;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size_minus_one;
	u8 instances_minus_one;
	u8 num_report_ids;
} __packed;

/* For touch panel compatebility */
enum hw_pattern_type{
	old_pattern,
	new_pattern
};

struct mxt_cfg_version{
	u8 year;
	u8 month;
	u8 date;
};

/* To store the cfg version in the hardware */
struct mxt_cfg_info{
	enum hw_pattern_type type;
	u16 fw_version;
	struct mxt_cfg_version cfg_version;
};

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	char phys[64];		/* device physical location */
	struct mxt_platform_data *pdata;
	enum hw_pattern_type pattern_type;
	struct mxt_cfg_info config_info;
	u8 *t38_config;
	struct mxt_object *object_table;
	struct mxt_info *info;
	void *raw_info_block;
	unsigned int irq;
	unsigned int max_x;
	unsigned int max_y;
	bool in_bootloader;
	u16 mem_size;
	u8 t100_aux_ampl;
	u8 t100_aux_area;
	u8 t100_aux_vect;
	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool debug_v2_enabled;
	u8 *debug_msg_data;
	u16 debug_msg_count;
	struct bin_attribute debug_msg_attr;
	struct mutex debug_msg_lock;
	u8 max_reportid;
	u32 config_crc;
	u32 info_crc;
	u8 bootloader_addr;
	struct t7_config t7_cfg;
	u8 *msg_buf;
	u8 t6_status;
	bool update_input;
	u8 last_message_count;
	u8 num_touchids;
	u8 num_stylusids;
	unsigned long t15_keystatus;
	bool use_retrigen_workaround;
	bool use_regulator;
	struct regulator *reg_vdd;
	struct regulator *reg_avdd;
	struct regulator *reg_vdd_io;
	struct regulator *reg_vcc_i2c;
	char *fw_name;
	char *cfg_name;

	/* Cached T8 configuration */
	struct t81_configuration t81_cfg;

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T5_msg_size;
	u8 T6_reportid;
	u16 T6_address;
	u16 T7_address;
	u16 T7_size;
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T15_reportid_min;
	u8 T15_reportid_max;
	u16 T18_address;
	u8 T19_reportid;
	u16 T38_address;
	u8 T38_size;
	u8 T42_reportid_min;
	u8 T42_reportid_max;
	u16 T44_address;
	u8 T48_reportid;
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u16 T81_address;
	u8 T81_size;
	u8 T81_reportid_min;
	u8 T81_reportid_max;
	u16 T100_address;
	u8 T100_reportid_min;
	u8 T100_reportid_max;

	/* for fw update in bootloader */
	struct completion bl_completion;

	/* for reset handling */
	struct completion reset_completion;

	/* for config update handling */
	struct completion crc_completion;

	/* Indicates whether device is in suspend */
	bool suspended;

	/* Indicates whether device is updating configuration */
	bool updating_config;

	u16 raw_data_16[RAW_DATA_SIZE];
	u16 raw_data_avg;
	
	/* Protect access to the T37 object buffer, add by morven */	
	struct mutex T37_buf_mutex;	
	u8 *T37_buf;	
	size_t T37_buf_size;
	u8 lockdown_info[MXT_LOCKDOWN_SIZE];

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	u16 T93_address;
	u8 T93_reportid;
	u16 T115_address;
	u8 T115_reportid;
	bool enable_wakeup_gesture;
	bool double_click_wake_enable;
	bool down_to_up_wake_enable;
	bool up_to_down_wake_enable;
	bool right_to_left_wake_enable;
	bool left_to_right_wake_enable;
	int detected_gesture;
#endif

	struct mutex bus_access_mutex;
};

/*Global variables comes below*/
static struct mxt_platform_data mxt_platform_data;
static int tpd_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct task_struct *thread = NULL;
unsigned int mxt_touch_irq = 0;
struct mxt_data *mxt_i2c_data;

/*Function declaration comes below*/
static void mxt_tpd_suspend(struct device *h);
static void mxt_tpd_resume(struct device *h);
static int mxt_suspend(struct device *h);
static int mxt_resume(struct device *h);
static int mxt_process_messages_until_invalid(struct mxt_data *data);
static void mxt_start(struct mxt_data *data);
static void mxt_stop(struct mxt_data *data);
static int mxt_input_open(struct input_dev *dev);
static void mxt_input_close(struct input_dev *dev);

static size_t mxt_obj_size(const struct mxt_object *obj)
{
	return obj->size_minus_one + 1;
}

static size_t mxt_obj_instances(const struct mxt_object *obj)
{
	return obj->instances_minus_one + 1;
}

static bool mxt_object_readable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_COMMAND_T6:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_GEN_DATASOURCE_T53:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_SELFTEST_T25:
	case MXT_TOUCH_MULTITOUCHSCREEN_T100:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DIGITIZER_T43:
	case MXT_SPT_CTECONFIG_T46:
        case MXT_TOUCH_SELF_CAPACITANCE_CONFIG_T111:
	case MXT_OBJ_NOISE_T72:
		return true;
	default:
		return false;
	}
}
static inline void reinit_completion_new(struct completion *x)//add by cassy

//static inline void reinit_completion(struct completion *x)//delete by cassy
{
	init_completion(x);
}

static void mxt_dump_message(struct mxt_data *data, u8 *message)
{
	//printk("mxt MSG: %*ph\n", data->T5_msg_size, message);
}

static void mxt_debug_msg_enable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (data->debug_v2_enabled)
		return;

	mutex_lock(&data->debug_msg_lock);

	data->debug_msg_data = kcalloc(DEBUG_MSG_MAX,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->debug_msg_data) {
		dev_err(dev, "Failed to allocate buffer\n");
		return;
	}

	data->debug_v2_enabled = true;
	mutex_unlock(&data->debug_msg_lock);

	dev_info(dev, "Enabled message output\n");
}

static void mxt_debug_msg_disable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (!data->debug_v2_enabled)
		return;

	dev_info(dev, "disabling message output\n");
	data->debug_v2_enabled = false;

	mutex_lock(&data->debug_msg_lock);
	kfree(data->debug_msg_data);
	data->debug_msg_data = NULL;
	data->debug_msg_count = 0;
	mutex_unlock(&data->debug_msg_lock);
	dev_info(dev, "Disabled message output\n");
}
static void mxt_debug_msg_add(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	mutex_lock(&data->debug_msg_lock);

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return;
	}

	if (data->debug_msg_count < DEBUG_MSG_MAX) {
		memcpy(data->debug_msg_data +
		       data->debug_msg_count * data->T5_msg_size,
		       msg,
		       data->T5_msg_size);
		data->debug_msg_count++;
	} else {
		data->debug_msg_count = 0;
	}

	mutex_unlock(&data->debug_msg_lock);

	sysfs_notify(&data->client->dev.kobj, NULL, "debug_notify");
}

static ssize_t mxt_debug_msg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	return -EIO;
}

static ssize_t mxt_debug_msg_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t bytes)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	size_t bytes_read;

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return 0;
	}

	count = bytes / data->T5_msg_size;

	if (count > DEBUG_MSG_MAX)
		count = DEBUG_MSG_MAX;

	mutex_lock(&data->debug_msg_lock);

	if (count > data->debug_msg_count)
		count = data->debug_msg_count;

	bytes_read = count * data->T5_msg_size;

	memcpy(buf, data->debug_msg_data, bytes_read);
	data->debug_msg_count = 0;

	mutex_unlock(&data->debug_msg_lock);

	return bytes_read;
}

static int mxt_debug_msg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	sysfs_bin_attr_init(&data->debug_msg_attr);
	data->debug_msg_attr.attr.name = "debug_msg";
	data->debug_msg_attr.attr.mode = 0666;
	data->debug_msg_attr.read = mxt_debug_msg_read;
	data->debug_msg_attr.write = mxt_debug_msg_write;
	data->debug_msg_attr.size = data->T5_msg_size * DEBUG_MSG_MAX;

	if (sysfs_create_bin_file(&data->client->dev.kobj, &data->debug_msg_attr) < 0) {
		dev_err(dev, "Failed to create %s\n",
			data->debug_msg_attr.attr.name);
		return -EINVAL;
	}

	return 0;
}

static void mxt_debug_msg_remove(struct mxt_data *data)
{
	if (data->debug_msg_attr.attr.name)
		sysfs_remove_bin_file(&data->client->dev.kobj,
				      &data->debug_msg_attr);
}

static inline unsigned long test_flag_8bit(unsigned long mask, const volatile unsigned char *addr)
{
	return ((*addr) & mask) != 0;
}

static inline unsigned long test_flag(unsigned long mask, const volatile unsigned long *addr)
{
	return ((*addr) & mask) != 0;
}

static int mxt_wait_for_completion(struct mxt_data *data,
				   struct completion *comp,
				   unsigned int timeout_ms)
{
	struct device *dev = &data->client->dev;
	unsigned long timeout = msecs_to_jiffies(timeout_ms);
	long ret;

	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		return ret;
	} else if (ret == 0) {
		dev_err(dev, "Wait for completion time out.\n");
		return -EINVAL;
	}

	return 0;
}
static int __mxt_write_reg_ext(struct i2c_client *client, u16 addr, u16 reg, u16 len, const void *val, unsigned long flag);
static int __mxt_read_reg_ext(struct i2c_client *client, u16 addr, u16 reg, u16 len, void *val, unsigned long flag);

static int mxt_bootloader_read(struct mxt_data *data, u8 *val, unsigned int count)
{
	struct i2c_client *client = data->client;

	return __mxt_read_reg_ext(client, data->bootloader_addr, 0, count, val, I2C_ACCESS_NO_REG);
}

static int mxt_bootloader_write(struct mxt_data *data, const u8 * const val, unsigned int count)
{
	struct i2c_client *client = data->client;

	return __mxt_write_reg_ext(client, data->bootloader_addr, 0, count, val, I2C_ACCESS_NO_REG);
}

static int mxt_lookup_bootloader_address(struct mxt_data *data, bool retry)
{
	u8 appmode = data->client->addr;
	u8 bootloader;
	u8 family_id = data->info ? data->info->family_id : 0;

	switch (appmode) {
		case 0x4a:
		case 0x4b:
			/* Chips after 1664S use different scheme */
			if (retry || family_id >= 0xa2) {
				bootloader = appmode - 0x24;
				break;
			}
			/* Fall through for normal case */
		case 0x4c:
		case 0x4d:
		case 0x5a:
		case 0x5b:
			bootloader = appmode - 0x26;
			break;
		default:
			dev_err(&data->client->dev,
				"Appmode i2c address 0x%02x not found\n",
				appmode);
			return -EINVAL;
	}

	data->bootloader_addr = bootloader;
    dev_info(&data->client->dev, "mxt: bootloader i2c address 0x%02x \n", bootloader);
	return 0;
}

static int mxt_probe_bootloader(struct mxt_data *data, bool retry)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;
	bool crc_failure;
	ret = mxt_lookup_bootloader_address(data, retry);
	if (ret)
		return ret;

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;
	
	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "Detected bootloader, status:%02X%s\n",
			val, crc_failure ? ", APP_CRC_FAIL" : "");

	return 0;
}

static u8 mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (mxt_bootloader_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return val;
		}

		dev_err(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_err(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state,bool wait)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;

recheck:
	ret = mxt_bootloader_read(data, &val, 1);
	if (ret) {
		dev_err(dev, "%s: i2c recv failed, ret=%d\n",
			__func__, ret);
		return ret;
	}

	if (state == MXT_WAITING_BOOTLOAD_CMD) {
		val = mxt_get_bootloader_version(data, val);
	}

	switch (state) {
		case MXT_WAITING_BOOTLOAD_CMD:
			val &= ~MXT_BOOT_STATUS_MASK;
			break;
		case MXT_WAITING_FRAME_DATA:
		case MXT_APP_CRC_FAIL:
			val &= ~MXT_BOOT_STATUS_MASK;
			break;
		case MXT_FRAME_CRC_PASS:
			if (val == MXT_FRAME_CRC_CHECK) {
				mdelay(10);
				goto recheck;
			} else if (val == MXT_FRAME_CRC_FAIL) {
				dev_err(dev, "Bootloader CRC fail\n");
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "Invalid bootloader mode state 0x%02X\n", val);
		return -EINVAL;
	}

	return 0;
}

static int mxt_send_bootloader_cmd(struct mxt_data *data, bool unlock)
{
	int ret;
	u8 buf[2];

	if (unlock) {
		buf[0] = MXT_UNLOCK_CMD_LSB;
		buf[1] = MXT_UNLOCK_CMD_MSB;
	} else {
		buf[0] = 0x01;
		buf[1] = 0x01;
	}

	ret = mxt_bootloader_write(data, buf, 2);
	if (ret){
		return ret;
	}

	return 0;
}

static int __mxt_cache_read(struct i2c_client *client,u16 addr, 
				u16 reg, u16 len, void *val, u8 *r_cache, u8 *r_cache_pa, u8 *w_cache, u8 *w_cache_pa, unsigned long flag)
{
	struct i2c_msg *msgs;
	int num;

	struct i2c_msg xfer[2];
	char buf[2];
	u16 transferred;
	int retry = 3;
	int ret;
	if (test_flag(I2C_ACCESS_NO_CACHE,&flag)){
		w_cache = w_cache_pa = buf;
		r_cache = r_cache_pa = val;
	}

	if (test_flag(I2C_ACCESS_NO_REG,&flag)){
		msgs = &xfer[1];
		num = 1;
	}else{
		w_cache[0] = reg & 0xff;
		w_cache[1] = (reg >> 8) & 0xff;

		msgs = &xfer[0];
		num = ARRAY_SIZE(xfer);
		/* Write register */
		xfer[0].addr = addr;
		xfer[0].flags = 0;
		xfer[0].len = 2;
		xfer[0].buf = w_cache_pa;
	}
    
	/* Read data */
	xfer[1].addr = addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].buf = r_cache_pa;

#if defined(CONFIG_MXT_I2C_EXTFLAG)
	xfer[1].ext_flag = xfer[0].ext_flag = client->addr & 0xff00;
	xfer[1].timing = xfer[0].timing = 100;
	dev_dbg(&client->dev, "%s: i2c transfer(r)  (addr %x extflag %x) reg %d len %d\n",
		__func__, client->addr, xfer[0].ext_flag, reg, len);
#endif

	transferred = 0;
	

	while(transferred < len) {
		if (!test_flag(I2C_ACCESS_NO_REG | I2C_ACCESS_R_REG_FIXED,&flag)) {
			w_cache[0] = (reg +  transferred) & 0xff;
			w_cache[1] = ((reg + transferred) >> 8) & 0xff;
		}

		if (test_flag(I2C_ACCESS_NO_CACHE,&flag))
			xfer[1].buf = r_cache_pa + transferred;
		xfer[1].len = len - transferred;
		if (xfer[1].len > MXT_MAX_BLOCK_READ)///255
			xfer[1].len = MXT_MAX_BLOCK_READ;
	retry_read:
		ret = i2c_transfer(client->adapter, msgs, num);
		if (ret != num) {
			if (retry) {
				dev_dbg(&client->dev, "%s: i2c transfer(r) retry, reg %d\n", __func__, reg);
				msleep(MXT_WAKEUP_TIME);
				retry--;
				goto retry_read;
			} else {
				dev_err(&client->dev, "%s: i2c transfer(r) failed (%d) reg %d len %d transferred %d\n",
					__func__, ret, reg, len, transferred);
				return -EIO;
			}
		}
		if (!test_flag(I2C_ACCESS_NO_CACHE,&flag))
			memcpy(val + transferred, r_cache, xfer[1].len);
		transferred += xfer[1].len;
		
	}
	return 0;
}

static int __mxt_cache_write(struct i2c_client *client,u16 addr, 
				u16 reg, u16 len, const void *val, u8 *w_cache, u8 *w_cache_pa, unsigned long flag)
{
	struct i2c_msg xfer;
	void *buf = NULL;
	u16 transferred,extend;
	int retry = 3;
	int ret;

	if (test_flag(I2C_ACCESS_NO_REG,&flag)) {
		extend = 0;
		if (test_flag(I2C_ACCESS_NO_CACHE,&flag))
			w_cache = w_cache_pa = (u8 *)val;
	}else {
		extend = 2;
		if (test_flag(I2C_ACCESS_NO_CACHE,&flag)) {
			buf = kmalloc( len + extend, GFP_KERNEL);
			if (!buf)
				return -ENOMEM;
			w_cache = w_cache_pa = buf;
		}

		w_cache[0] = reg & 0xff;
		w_cache[1] = (reg >> 8) & 0xff;
	}

	/* Write register */
	xfer.addr = addr;
	xfer.flags = 0;
	xfer.buf = w_cache_pa;

#if defined(CONFIG_MXT_I2C_EXTFLAG)
	xfer.ext_flag = client->addr & 0xff00;
	xfer.timing = 100;
	dev_dbg(&client->dev, "%s: i2c transfer(w) (addr %x extflag %x) reg %d len %d\n",
		__func__, client->addr , xfer.ext_flag, reg, len);
#endif

	transferred = 0;
	while(transferred < len) {
		xfer.len = len - transferred+ extend;
		if (xfer.len> MXT_MAX_BLOCK_WRITE)
			xfer.len = MXT_MAX_BLOCK_WRITE;
	
		if (test_flag(I2C_ACCESS_NO_CACHE,&flag) &&
			test_flag(I2C_ACCESS_NO_REG,&flag))
			xfer.buf = w_cache_pa + transferred;
		else
			memcpy(w_cache + extend, val + transferred, xfer.len - extend);

		if (extend) {
			w_cache[0] = (reg +  transferred) & 0xff;
			w_cache[1] = ((reg + transferred) >> 8) & 0xff;
		}

	retry_write:
		ret = i2c_transfer(client->adapter, &xfer, 1);
		if (ret != 1) {
			if (retry) {
				dev_dbg(&client->dev, "%s: i2c transfer(w) retry, reg %d\n", __func__, reg);
				msleep(MXT_WAKEUP_TIME);
				retry--;
				goto retry_write;
			} else {
				dev_err(&client->dev, "%s: i2c transfer(w) failed (%d) reg %d len %d transferred %d\n",
					__func__, ret, reg, len, transferred);
				if (buf)
					kfree(buf);
				return -EIO;
			}
		}

		transferred += xfer.len -extend;
#if 0
		print_hex_dump(KERN_DEBUG, "[mxt] w:", DUMP_PREFIX_NONE, 16, 1,
					test_flag(I2C_ACCESS_NO_CACHE,&flag) ? xfer.buf : w_cache, xfer.len, false);
#endif
	}

	if (buf)
		kfree(buf);
	return 0;
}

static int __mxt_read_reg_ext(struct i2c_client *client, u16 addr, u16 reg, u16 len, void *val, unsigned long flag)
{
	struct mxt_data *data = i2c_get_clientdata(client);
	u8 *r_cache,*r_cache_pa,*w_cache,*w_cache_pa;
	int ret;

	r_cache_pa = r_cache = NULL;
	w_cache_pa = w_cache = NULL;

	flag |= I2C_ACCESS_NO_CACHE;

	mutex_lock(&data->bus_access_mutex);
	
	ret = __mxt_cache_read(client, addr, reg, len, val, r_cache, r_cache_pa, w_cache, w_cache_pa, flag);

	mutex_unlock(&data->bus_access_mutex);
	
	return ret;
}

static int __mxt_write_reg_ext(struct i2c_client *client, u16 addr, u16 reg, u16 len,
				const void *val, unsigned long flag)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	u8 *w_cache,*w_cache_pa;
	int ret;

	w_cache_pa = w_cache = NULL;
	flag |= I2C_ACCESS_NO_CACHE;

	mutex_lock(&data->bus_access_mutex);
	ret = __mxt_cache_write(client, addr, reg, len, val, w_cache, w_cache_pa, flag);
	mutex_unlock(&data->bus_access_mutex);

	return ret;
}

static int __mxt_read_reg(struct i2c_client *client, u16 reg, u16 len,
				void *val)
{
	

	return __mxt_read_reg_ext(client, client->addr, reg, len, val, 0);
}

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,
				const void *val)
{
	return __mxt_write_reg_ext(client, client->addr, reg, len, val, 0);
}

static inline void mxt_config_ctrl_set(struct mxt_data *mxt, u16 addr, u8 mask)
{
	int error, ctrl;

	error = __mxt_read_reg(mxt->client, addr, 1, &ctrl);
	if (error)
		dev_err(&mxt->client->dev, "%s:%d: i2c sent failed\n",
				__func__, __LINE__);
	ctrl |= mask;
	error = __mxt_write_reg(mxt->client, addr, 1, &ctrl);
	if (error)
		dev_err(&mxt->client->dev, "%s:%d: i2c sent failed\n",
				__func__, __LINE__);
}

static inline void mxt_config_ctrl_clear(struct mxt_data *mxt, u16 addr,
					u8 mask)
{
	int error, ctrl;

	error = __mxt_read_reg(mxt->client, addr, 1, &ctrl);
	if (error)
		dev_err(&mxt->client->dev, "%s:%d: i2c sent failed\n",
				__func__, __LINE__);
	ctrl &= ~mask;
	error = __mxt_write_reg(mxt->client, addr, 1, &ctrl);
	if (error)
		dev_err(&mxt->client->dev, "%s:%d: i2c sent failed\n",
				__func__, __LINE__);
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

static struct mxt_object *mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_warn(&data->client->dev, "Invalid object type T%u\n", type);
	return NULL;
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	u8 status = msg[1];
	u32 crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);
	complete(&data->crc_completion);
	if (crc != data->config_crc) {
		data->config_crc = crc;
	}

	/* Detect reset */
	if (status & MXT_T6_STATUS_RESET){
		complete(&data->reset_completion);
	}

	/* Output debug if status has changed */
	if (status != data->t6_status){
		pr_debug("T6 Status 0x%02X%s%s%s%s%s%s%s\n",
			status,
			status == 0 ? " OK" : "",
			status & MXT_T6_STATUS_RESET ? " RESET" : "",
			status & MXT_T6_STATUS_OFL ? " OFL" : "",
			status & MXT_T6_STATUS_SIGERR ? " SIGERR" : "",
			status & MXT_T6_STATUS_CAL ? " CAL" : "",
			status & MXT_T6_STATUS_CFGERR ? " CFGERR" : "",
			status & MXT_T6_STATUS_COMSERR ? " COMSERR" : "");
		/* Save current status */
		data->t6_status = status;
	}
}

static void mxt_input_button(struct mxt_data *data, u8 *message)
{
	struct input_dev *input = data->input_dev;
	const struct mxt_platform_data *pdata = data->pdata;
	bool button;
	int i;

	/* Active-low switch */
	for (i = 0; i < pdata->t19_num_keys; i++) {
		if (pdata->t19_keymap[i] == KEY_RESERVED)
			continue;
		button = !(message[1] & (1 << i));
		input_report_key(input, pdata->t19_keymap[i], button);
	}
}

static void mxt_input_sync(struct mxt_data *data)
{
	input_mt_report_pointer_emulation(data->input_dev,
					  data->pdata->t19_num_keys);
	input_sync(data->input_dev);
}

static void mxt_proc_t9_message(struct mxt_data *data, u8 *message)
{
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;
	int tool;

	return;

	id = message[0] - data->T9_reportid_min;
	status = message[1];
	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));

	/* Handle 10/12 bit switching */
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;

	area = message[5];

	amplitude = message[6];
	vector = message[7];

	pr_debug( "[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: %02X\n", id,
		(status & MXT_T9_DETECT) ? 'D' : '.',
		(status & MXT_T9_PRESS) ? 'P' : '.',
		(status & MXT_T9_RELEASE) ? 'R' : '.',
		(status & MXT_T9_MOVE) ? 'M' : '.',
		(status & MXT_T9_VECTOR) ? 'V' : '.',
		(status & MXT_T9_AMP) ? 'A' : '.',
		(status & MXT_T9_SUPPRESS) ? 'S' : '.',
		(status & MXT_T9_UNGRIP) ? 'U' : '.',
		x, y, area, amplitude, vector);

	input_mt_slot(input_dev, id);

	if (status & MXT_T9_DETECT) {
		/*
		 * Multiple bits may be set if the host is slow to read
		 * the status messages, indicating all the events that
		 * have happened.
		 */
		if (status & MXT_T9_RELEASE) {
			input_mt_report_slot_state(input_dev,
						   MT_TOOL_FINGER, 0);
			mxt_input_sync(data);
		}

		/* A size of zero indicates touch is from a linked T47 Stylus */
		if (area == 0) {
			area = MXT_TOUCH_MAJOR_T47_STYLUS;
			tool = MT_TOOL_PEN;
		} else {
			tool = MT_TOOL_FINGER;
		}

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
		input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	data->update_input = true;
}

static void mxt_proc_t81_message(struct mxt_data *data, u8 *message)
{
	int id ;
	u8 status;
	int xdelta, ydelta;
	struct input_dev *dev = data->input_dev;

	/* we don't enable this function now 141119 */
	return;

	if (message == NULL)
		return;
	id = message[0];
	status = message[1];
	xdelta = (message[3]<< 8) | message[2];
	ydelta = (message[5]<< 8) | message[4];

	if (xdelta && ydelta) {
		input_report_key(dev, KEY_POWER, 1);
		input_sync(dev);
		input_report_key(dev, KEY_POWER, 0);
		input_sync(dev);
		dev_info(&data->client->dev, "Report a power key event\n");
	}

	dev_info(&data->client->dev, "%s: id: 0x%x, status: 0x%x, xdelta: 0x%x, ydelta: 0x%x\n",
		__func__, id, status, xdelta, ydelta);

	return;
}

static void mxt_proc_t100_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;

	struct input_dev *sel_input_dev = NULL;
	u8 status, touch_event;
	int x;
	int y;
	int id;
	int index = 0;

	if (!input_dev)
		return;

	id = message[0] - data->T100_reportid_min;

	if (id < 0 || id > data->num_touchids) {
		dev_err(dev, "invalid touch id %d, total num touch is %d\n",
			id, data->num_touchids);
		return;
	}

	/*valid touch information is from id=2*/
	if (id == 0) {
		status = message[1];
	}else if (id >= 2) {
		/* deal with each point report */
		id -=2;
		status = message[1];
		touch_event = status & 0x0F;
		x = (message[3] << 8) | (message[2] & 0xFF);
		y = (message[5] << 8) | (message[4] & 0xFF);
		index = 6;

		sel_input_dev = input_dev;
		input_mt_slot(sel_input_dev, id);

		if (status & MXT_T100_DETECT) {
			if (touch_event == MXT_T100_EVENT_DOWN 
				|| touch_event == MXT_T100_EVENT_UNSUP
				|| touch_event == MXT_T100_EVENT_MOVE
				|| touch_event == MXT_T100_EVENT_NONE) {
				/* Touch in detect, report X/Y position */
				input_mt_report_slot_state(sel_input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(sel_input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(sel_input_dev, ABS_MT_POSITION_Y, y);


				if (data->t100_aux_ampl){
					input_report_abs(input_dev, ABS_MT_PRESSURE,message[data->t100_aux_ampl]);
				}
				else{
					input_report_abs(input_dev, ABS_MT_PRESSURE,0xff);
				}
					


				
				mxt_input_sync(data);
			}
		} else {
			/* Touch no longer in detect, so close out slot */
			input_mt_report_slot_state(sel_input_dev, MT_TOOL_FINGER, 0);
			mxt_input_sync(data);
		}
	}
	data->update_input = true;
}

static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	int key;
	bool curr_state, new_state;
	bool sync = false;
	u8 num_keys;
	const unsigned int *keymap;

	unsigned long keystates = le32_to_cpu(msg[2]);
    

	num_keys = data->pdata->num_keys[T15_T97_KEY];
	keymap = data->pdata->keymap[T15_T97_KEY];
	for (key = 0; key < 3; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);
        //printk("mxt t15 key: %d, curr_state: %d, new_state: %d\n", key, curr_state, new_state);

		if (!curr_state && new_state) {
			__set_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY, keymap[key], 1);
			sync = true;
		} else if (curr_state && !new_state) {
			__clear_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY, keymap[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	if (status & MXT_T42_MSG_TCHSUP)
		dev_info(dev, "T42 suppress\n");
	else
		dev_info(dev, "T42 normal\n");
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	pr_debug("T48 state %d status %02X %s%s%s%s%s\n", state, status,
		status & 0x01 ? "FREQCHG " : "",
		status & 0x02 ? "APXCHG " : "",
		status & 0x04 ? "ALGOERR " : "",
		status & 0x10 ? "STATCHG " : "",
		status & 0x20 ? "NLVLCHG " : "");

	return 0;
}

static void mxt_proc_t63_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 id;
	u16 x, y;
	u8 pressure;

	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);

	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_T63_STYLUS_PRESSURE_MASK;

	pr_debug("[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
		id,
		msg[1] & MXT_T63_STYLUS_SUPPRESS ? 'S' : '.',
		msg[1] & MXT_T63_STYLUS_MOVE     ? 'M' : '.',
		msg[1] & MXT_T63_STYLUS_RELEASE  ? 'R' : '.',
		msg[1] & MXT_T63_STYLUS_PRESS    ? 'P' : '.',
		x, y, pressure,
		msg[2] & MXT_T63_STYLUS_BARREL   ? 'B' : '.',
		msg[2] & MXT_T63_STYLUS_ERASER   ? 'E' : '.',
		msg[2] & MXT_T63_STYLUS_TIP      ? 'T' : '.',
		msg[2] & MXT_T63_STYLUS_DETECT   ? 'D' : '.');

	input_mt_slot(input_dev, id);

	if (msg[2] & MXT_T63_STYLUS_DETECT) {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
	} else {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 0);
	}

	input_report_key(input_dev, BTN_STYLUS,
			 (msg[2] & MXT_T63_STYLUS_ERASER));
	input_report_key(input_dev, BTN_STYLUS2,
			 (msg[2] & MXT_T63_STYLUS_BARREL));

	mxt_input_sync(data);
}

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
static void mxt_proc_t93_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *dev = data->input_dev;

	if ((msg[0] == data->T93_reportid) &&
		(msg[1] & MXT_GESTURE_DOUBLE_CLICK_MASK)) {
		data->detected_gesture = DOUBLE;
		input_report_key(dev, KEY_POWER, 1);
		input_sync(dev);
		input_report_key(dev, KEY_POWER, 0);
		input_sync(dev);
	}
}

static void mxt_proc_t115_messages(struct mxt_data *data, u8 *msg)
{
	int gesture_detected = 0;
	struct input_dev *dev = data->input_dev;
	if (msg[0] != data->T115_reportid)
		return;

	switch (msg[1]) {
		case MXT_GESTURE_DOWN:
			if (data->up_to_down_wake_enable) {
				data->detected_gesture = DOWN;
				gesture_detected = 1;
			}
			break;
		case MXT_GESTURE_UP:
			if (data->down_to_up_wake_enable) {
				data->detected_gesture = UP;
				gesture_detected = 1;
			}
			break;
		case MXT_GESTURE_LEFT:
			if (data->right_to_left_wake_enable) {
				data->detected_gesture = LEFT;
				gesture_detected = 1;
			}
			break;
		case MXT_GESTURE_RIGHT:
			if (data->left_to_right_wake_enable) {
				data->detected_gesture = RIGHT;
				gesture_detected = 1;
			}
			break;
		default:
			break;
	}

	if (gesture_detected) {
		input_report_key(dev, KEY_POWER, 1);
		input_sync(dev);
		input_report_key(dev, KEY_POWER, 0);
		input_sync(dev);
	}
}
#endif

static int mxt_proc_message(struct mxt_data *data, u8 *message)
{
	u8 report_id = message[0];
	bool dump = data->debug_enabled;
	
	if (report_id == MXT_RPTID_NOMSG){
		return 0;
	}

	if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, message);
	} else if (report_id >= data->T42_reportid_min
		   && report_id <= data->T42_reportid_max) {
		mxt_proc_t42_messages(data, message);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, message);
	} else if (report_id >= data->T9_reportid_min
	    && report_id <= data->T9_reportid_max) {
		mxt_proc_t9_message(data, message);
	} else if (report_id == data->T81_reportid_min) {
		mxt_proc_t81_message(data, message);
	} else if (report_id >= data->T100_reportid_min
	    && report_id <= data->T100_reportid_max) {
		mxt_proc_t100_message(data, message);
	} else if (report_id == data->T19_reportid) {
		mxt_input_button(data, message);
		data->update_input = true;
	} else if (report_id >= data->T63_reportid_min 
			&& report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, message);
	} else if (report_id >= data->T15_reportid_min 
			&& report_id <= data->T15_reportid_max) {
		mxt_proc_t15_messages(data, message);
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	} else if ((report_id == data->T115_reportid) 
			&& data->suspended) {
		mxt_proc_t115_messages(data, message);
	} else if (report_id == data->T93_reportid
			&& data->suspended
			&& data->double_click_wake_enable) {
		mxt_proc_t93_messages(data, message);
#endif
	}else{
		dump = true;
	}

	if (dump){
		mxt_dump_message(data, message);
	}

	if (data->debug_v2_enabled){
		mxt_debug_msg_add(data, message);
	}

	return 1;
}

static int mxt_read_and_process_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u8 num_valid = 0;

	/* Safety check for msg_buf */
	if (count > data->max_reportid)
		return -EINVAL;

	/* Process remaining messages if necessary */
	ret = __mxt_read_reg(data->client, data->T5_address,
				data->T5_msg_size * count, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d)\n", count, ret);
		return ret;
	}

	for (i = 0;  i < count; i++) {
		ret = mxt_proc_message(data,
			data->msg_buf + data->T5_msg_size * i);

		if (ret == 1)
			num_valid++;
	}

	/* return number of messages read */
	return num_valid;
}

static irqreturn_t mxt_process_messages_t44(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count, num_left;

	/* Read T44 and T5 together */
	ret = __mxt_read_reg(data->client, data->T44_address,
		data->T5_msg_size + 1, data->msg_buf);
	if (ret) {
		dev_err(dev, "mxt: Failed to read T44 and T5 (%d)\n", ret);
		return IRQ_NONE;
	}

	count = data->msg_buf[0];

	if (count == 0) {
		
		return IRQ_NONE;
	} else if (count > data->max_reportid) {
		
		count = data->max_reportid;
	}

	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		
		return IRQ_NONE;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		//printk("mxt_process_messages_t44 going to handle left 0x%x messages\n", num_left);
		ret = mxt_read_and_process_messages(data, num_left);
		if (ret < 0) {
			goto end;
	}
	}
end:
	if (data->update_input) {
		mxt_input_sync(data);
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static int mxt_process_messages_until_invalid(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int count, read;
	u8 tries = 2;
	count = data->max_reportid;

	/* Read messages until we force an invalid */
	do {
		read = mxt_read_and_process_messages(data, count);
		if (read < count)
			return 0;
	} while (--tries);
	

	if (data->update_input) {
		mxt_input_sync(data);
		data->update_input = false;
	}
	dev_err(dev, "CHG pin isn't cleared\n");
	return -EBUSY;
}

static irqreturn_t mxt_process_messages(struct mxt_data *data)
{
	int total_handled, num_handled;
	u8 count = data->last_message_count;
	if (count < 1 || count > data->max_reportid)
		count = 1;

	/* include final invalid message */
	total_handled = mxt_read_and_process_messages(data, count + 1);
	if (total_handled < 0){
		return IRQ_NONE;
	/* if there were invalid messages, then we are done */
	}else if (total_handled <= count){
		goto update_count;
	}

	/* keep reading two msgs until one is invalid or reportid limit */
	do {
		num_handled = mxt_read_and_process_messages(data, 2);
		if (num_handled < 0)
			return IRQ_NONE;

		total_handled += num_handled;

		if (num_handled < 2)
			break;
	} while (total_handled < data->num_touchids);

update_count:
	data->last_message_count = total_handled;

	if (data->update_input) {
		mxt_input_sync(data);
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	disable_irq_nosync(mxt_touch_irq);
	tpd_flag=1;
	wake_up_interruptible(&waiter);
	
	return IRQ_HANDLED;
}

static int touch_event_handler(void *pdata)
{
	struct mxt_data *data = pdata;
	do{
		set_current_state(TASK_INTERRUPTIBLE);

		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);

		if (!data->object_table) {
			
		}

		if(!data->in_bootloader){
			if (data->T44_address) {
				
				mxt_process_messages_t44(data);
			} else {
			
				mxt_process_messages(data);
			}
			enable_irq(mxt_touch_irq);
		}
	}while(!kthread_should_stop());
	return 0;
}

static int mxt_t6_command(struct mxt_data *data, u16 cmd_offset, u8 value, bool wait)
{
	u16 reg;
	u8 command_register;
	int timeout_counter = 0;
	int ret;

	reg = data->T6_address + cmd_offset;

	ret = mxt_write_reg(data->client, reg, value);
	if (ret) {
		dev_err(&data->client->dev, "%s: Reg writing failed!\n", __func__);
		return ret;
	}

	if (!wait)
		return 0;

	do {
		msleep(20);
		ret = __mxt_read_reg(data->client, reg, 1, &command_register);
		if (ret)
			return ret;
	} while (command_register != 0 && timeout_counter++ <= 100);

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "%s: Command failed!\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt_soft_reset(struct mxt_data *data)
{
	int ret = 0;


	//reinit_completion(&data->reset_completion);//delete by cassy
	reinit_completion_new(&data->reset_completion);//add by cassy

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	if (ret){
		dev_err(&data->client->dev, "%s: send t6 reset command failed!\n", __func__);
		return ret;
	}

	ret = mxt_wait_for_completion(data, &data->reset_completion, MXT_RESET_TIMEOUT);
	if (ret){
		//printk("wait for completion time out.\n");
	}

	return 0;
}

static void mxt_update_crc(struct mxt_data *data, u8 cmd, u8 value)
{
	mxt_t6_command(data, cmd, value, true);
	msleep(30);
	mxt_process_messages_until_invalid(data);
}

static void mxt_calc_crc24(u32 *crc, u8 firstbyte, u8 secondbyte)
{
	static const unsigned int crcpoly = 0x80001B;
	u32 result;
	u32 data_word;

	data_word = (secondbyte << 8) | firstbyte;
	result = ((*crc << 1) ^ data_word);

	if (result & 0x1000000)
		result ^= crcpoly;

	*crc = result;
}

static u32 mxt_calculate_crc(u8 *base, off_t start_off, off_t end_off)
{
	u32 crc = 0;
	u8 *ptr = base + start_off;
	u8 *last_val = base + end_off - 1;

	if (end_off < start_off)
		return -EINVAL;

	while (ptr < last_val) {
		mxt_calc_crc24(&crc, *ptr, *(ptr + 1));
		ptr += 2;
	}

	/* if len is odd, fill the last byte with 0 */
	if (ptr == last_val)
		mxt_calc_crc24(&crc, *ptr, 0);

	/* Mask to 24-bit */
	crc &= 0x00FFFFFF;

	return crc;
}

static int mxt_check_retrigen(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	int val;

	//if (irq_get_trigger_type(6) & IRQF_TRIGGER_LOW)
	if(1)
		return 0;

	if (data->T18_address) {
		error = __mxt_read_reg(client,
				       data->T18_address + MXT_COMMS_CTRL,
				       1, &val);
		if (error)
			return error;

		if (val & MXT_COMMS_RETRIGEN)
			return 0;
	}

	dev_warn(&client->dev, "Enabling RETRIGEN workaround\n");
	data->use_retrigen_workaround = true;
	return 0;
}
//add by cassy begin
static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	//struct device *dev = &data->client->dev;
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 48, .idle = 48 };
	struct t7_config active_mode = { .active = 255, .idle = 255 };

	if (sleep == MXT_POWER_CFG_DEEPSLEEP)
		new_config = &deepsleep;
	else
		new_config = &active_mode;
        
	error = __mxt_write_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), new_config);
	if (error)
		return error;

	//printk("mxt: Set T7 ACTV:%d IDLE:%d\n", new_config->active, new_config->idle);

	return 0;
}

//add by cassy end
static int mxt_init_t7_power_cfg(struct mxt_data *data)
{
	//struct device *dev = &data->client->dev;
	int error;
	bool retry = false;
recheck:
	error = __mxt_read_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), &data->t7_cfg);
	if (error)
		return error;
	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0) {
		if (!retry) {

			mxt_soft_reset(data);
			retry = true;
			goto recheck;
		} else {

			data->t7_cfg.active = 20;
			data->t7_cfg.idle = 100;
			return mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		}
	}

	//printk("Initialized power cfg: ACTV %d, IDLE %d\n", data->t7_cfg.active, data->t7_cfg.idle);
	return 0;
}

static int mxt_read_object(struct mxt_data *data,
				u8 type, u8 offset, u8 *val)
{
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	if (data->debug_enabled)
		dev_info(&data->client->dev, "read from object %d, reg 0x%02x, val 0x%x\n",
				(int)type, reg + offset, *val);
	return __mxt_read_reg(data->client, reg + offset, 1, val);
}

static int mxt_write_object(struct mxt_data *data,
				 u8 type, u8 offset, u8 val)
{
	struct mxt_object *object;
	u16 reg;
	int ret;
	u8 size, instances;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	size = mxt_obj_instances(object);
	instances = mxt_obj_instances(object);

	if (offset >= size * instances) {
		dev_err(&data->client->dev, "Tried to write outside object T%d"
			" offset:%d, size:%d\n", type, offset, size);
		return -EINVAL;
	}

	reg = object->start_address;
	if (data->debug_enabled)
		dev_info(&data->client->dev, "write to object %d, reg 0x%02x, val 0x%x\n",
				(int)type, reg + offset, val);
	ret = __mxt_write_reg(data->client, reg + offset, 1, &val);

	return ret;
}

static int mxt_read_lockdown_info(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_object *object;
	int ret, i = 0;
	u8 val;
	u16 reg;

	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_DIAGNOSTIC, 0x81);
	if (ret) {
		dev_err(dev, "Failed to send lockdown info read command!\n");
		return ret;
	}

	while (i < 100) {
		ret = mxt_read_object(data, MXT_GEN_COMMAND_T6,
					MXT_COMMAND_DIAGNOSTIC, &val);
		if (ret) {
			dev_err(dev, "Failed to read diagnostic!\n");
			return ret;
		}

		if (val == 0)
			break;

		i++;
		msleep(10);
	}

	object = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	ret = __mxt_read_reg(data->client, reg + MXT_LOCKDOWN_OFFSET,
			MXT_LOCKDOWN_SIZE, data->lockdown_info);
	if (ret)
		dev_err(dev, "Failed to read lockdown info!\n");

	dev_info(dev, "Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
			data->lockdown_info[0], data->lockdown_info[1],
			data->lockdown_info[2], data->lockdown_info[3],
			data->lockdown_info[4], data->lockdown_info[5],
			data->lockdown_info[6], data->lockdown_info[7]);

	return 0;
}

/*
 * mxt_update_cfg - download configuration to chip
 *
 * Atmel Raw Config File Format
 *
 * The first four lines of the raw config file contain:
 *  1) Version
 *  2) Chip ID Information (first 7 bytes of device memory)
 *  3) Chip Information Block 24-bit CRC Checksum
 *  4) Chip Configuration 24-bit CRC Checksum
 *
 * The rest of the file consists of one line per object instance:
 *   <TYPE> <INSTANCE> <SIZE> <CONTENTS>
 *
 *   <TYPE> - 2-byte object type as hex
 *   <INSTANCE> - 2-byte object instance number as hex
 *   <SIZE> - 2-byte object size as hex
 *   <CONTENTS> - array of <SIZE> 1-byte hex values
 */
static int mxt_update_cfg(struct mxt_data *data, const struct firmware *cfg)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	struct mxt_object *object;
	int ret;
	int offset;
	int data_pos;
	int byte_offset;
	int i;
	int cfg_start_ofs;
	u32 info_crc, config_crc, calculated_crc;
	u8 *config_mem;
	size_t config_mem_size;
	unsigned int type, instance, size;
	u8 val;
	u16 reg;

	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos = strlen(MXT_CFG_MAGIC);

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {
		ret = sscanf(cfg->data + data_pos, "%hhx%n",
			     (unsigned char *)&cfg_info + i,
			     &offset);
		if (ret != 1) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release;
		}

		data_pos += offset;
	}
	if (cfg_info.family_id != data->info->family_id) {
		dev_err(dev, "Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info->variant_id) {
		dev_err(dev, "Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}
	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Info CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Config CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;
	/*
	 * The Info Block CRC is calculated over mxt_info and the object
	 * table. If it does not match then we are trying to load the
	 * configuration from a different chip or firmware version, so
	 * the configuration CRC is invalid anyway.
	 */
	if (info_crc == data->info_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "Config CRC 0x%06X: OK\n",
				data->config_crc);
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev,
			 "Warning: Info CRC error - device=0x%06X file=0x%06X\n",
			 data->info_crc, info_crc);
	}
	/* Malloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START +
			data->info->object_num * sizeof(struct mxt_object) +
			MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto release;
	}
	while (data_pos < cfg->size) {
		/* Read type, instance, length */
		ret = sscanf(cfg->data + data_pos, "%x %x %x%n",
			     &type, &instance, &size, &offset);
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret != 3) {
			dev_err(dev, "Bad format: failed to parse object\n");
			ret = -EINVAL;
			goto release_mem;
		}
		data_pos += offset;
		object = mxt_get_object(data, type);
		if (!object) {
			/* Skip object */
			for (i = 0; i < size; i++) {
				ret = sscanf(cfg->data + data_pos, "%hhx%n",
					     &val,
					     &offset);
				data_pos += offset;
			}
			continue;
		}
		if (size > mxt_obj_size(object)) {
			/*
			 * Either we are in fallback mode due to wrong
			 * config or config from a later fw version,
			 * or the file is corrupt or hand-edited.
			 */
			dev_warn(dev, "Discarding %zu byte(s) in T%u\n",
				 size - mxt_obj_size(object), type);
		} else if (mxt_obj_size(object) > size) {
			/*
			 * If firmware is upgraded, new bytes may be added to
			 * end of objects. It is generally forward compatible
			 * to zero these bytes - previous behaviour will be
			 * retained. However this does invalidate the CRC and
			 * will force fallback mode until the configuration is
			 * updated. We warn here but do nothing else - the
			 * malloc has zeroed the entire configuration.
			 */
			dev_warn(dev, "Zeroing %zu byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}
		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + mxt_obj_size(object) * instance;

		for (i = 0; i < size; i++) {
			ret = sscanf(cfg->data + data_pos, "%hhx%n",
				     &val,
				     &offset);
			if (ret != 1) {
				dev_err(dev, "Bad format in T%d\n", type);
				ret = -EINVAL;
				goto release_mem;
			}
			data_pos += offset;

			if (i > mxt_obj_size(object))
				continue;

			byte_offset = reg + i - cfg_start_ofs;

			if ((byte_offset >= 0)
			    && (byte_offset <= config_mem_size)) {
				*(config_mem + byte_offset) = val;
			} else {
				dev_err(dev, "Bad object: reg:%d, T%d, ofs=%d\n",
					reg, object->type, byte_offset);
				ret = -EINVAL;
				goto release_mem;
			}
		}

		if (type == MXT_USER_DATA_T38 && data->t38_config) {
				memcpy(config_mem + reg - cfg_start_ofs + MXT_T38_INFO_SIZE, data->t38_config + MXT_T38_INFO_SIZE,
					data->T38_size - MXT_T38_INFO_SIZE);
		}
	}

	/* Calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < cfg_start_ofs) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		ret = 0;
		goto release_mem;
	}

	calculated_crc = mxt_calculate_crc(config_mem,
					   data->T7_address - cfg_start_ofs,
					   config_mem_size);

	if (config_crc > 0 && (config_crc != calculated_crc))
		dev_warn(dev, "Config CRC error, calculated=%06X, file=%06X\n",
			 calculated_crc, config_crc);
	/* Write configuration as blocks */
	byte_offset = 0;
	while (byte_offset < config_mem_size) {
		size = config_mem_size - byte_offset;

		if (size > MXT_MAX_BLOCK_WRITE)
			size = MXT_MAX_BLOCK_WRITE;

		ret = __mxt_write_reg(data->client,
				      cfg_start_ofs + byte_offset,
				      size, config_mem + byte_offset);
		if (ret != 0) {
			dev_err(dev, "Config write error, ret=%d\n", ret);
			goto release_mem;
		}

		byte_offset += size;
	}
	mxt_update_crc(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);
	ret = mxt_check_retrigen(data);
	if (ret)
		goto release_mem;
	ret = mxt_soft_reset(data);
	if (ret)
		goto release_mem;
	dev_info(dev, "Config successfully updated\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);
release_mem:
	kfree(config_mem);
release:
	release_firmware(cfg);
	return ret;
}
//delete by cassy begin
#if 0
static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	//struct device *dev = &data->client->dev;
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 48, .idle = 48 };
	struct t7_config active_mode = { .active = 255, .idle = 255 };

	if (sleep == MXT_POWER_CFG_DEEPSLEEP)
		new_config = &deepsleep;
	else
		new_config = &active_mode;
        
	error = __mxt_write_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), new_config);
	if (error)
		return error;


	return 0;
}
#endif
//delete by cassy end

static int mxt_acquire_irq(struct mxt_data *data)
{
	int error;

	enable_irq(mxt_touch_irq);

	if (data->use_retrigen_workaround) {
		error = mxt_process_messages_until_invalid(data);
		if (error)
			return error;
	}

	return 0;
}

static void mxt_free_input_device(struct mxt_data *data)
{
	if (data->input_dev) {
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}
}

static void mxt_free_object_table(struct mxt_data *data)
{
	mxt_debug_msg_remove(data);
	mxt_free_input_device(data);

	data->object_table = NULL;
	data->info = NULL;

	kfree(data->raw_info_block);
	data->raw_info_block = NULL;

	kfree(data->msg_buf);
	data->msg_buf = NULL;

	/* free the t38 configuration mem */
	if (data->t38_config) {
		kfree(data->t38_config);
		data->t38_config = NULL;
	}

	data->T5_address = 0;
	data->T5_msg_size = 0;
	data->T6_reportid = 0;
	data->T7_address = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T15_reportid_min = 0;
	data->T15_reportid_max = 0;
	data->T18_address = 0;
	data->T19_reportid = 0;
	data->T42_reportid_min = 0;
	data->T42_reportid_max = 0;
	data->T44_address = 0;
	data->T48_reportid = 0;
	data->T63_reportid_min = 0;
	data->T63_reportid_max = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->max_reportid = 0;
}

static int mxt_parse_object_table(struct mxt_data *data,
				  struct mxt_object *object_table)
{
	struct i2c_client *client = data->client;
	int i;
	u8 reportid;
	u16 end_address;

	/* Valid Report IDs start counting from 1 */
	reportid = 1;
	data->mem_size = 0;
	for (i = 0; i < data->info->object_num; i++) {
		struct mxt_object *object = object_table + i;
		u8 min_id, max_id;

		le16_to_cpus(&object->start_address);

		if (object->num_report_ids) {
			min_id = reportid;
			reportid += object->num_report_ids *
					mxt_obj_instances(object);
			max_id = reportid - 1;
		} else {
			min_id = 0;
			max_id = 0;
		}

		switch (object->type) {
			case MXT_GEN_MESSAGE_T5:
				if (data->info->family_id == 0x80) {
					/*
					 * On mXT224 read and discard unused CRC byte
					 * otherwise DMA reads are misaligned
					 */
					data->T5_msg_size = mxt_obj_size(object);
				} else {
					/* CRC not enabled, so skip last byte */
					data->T5_msg_size = mxt_obj_size(object) - 1;
				}
				data->T5_address = object->start_address;
			case MXT_GEN_COMMAND_T6:
				data->T6_reportid = min_id;
				data->T6_address = object->start_address;
				break;
			case MXT_GEN_POWER_T7:
				data->T7_address = object->start_address;
				data->T7_size = object->size_minus_one + 1;
				break;
			case MXT_TOUCH_MULTI_T9:
				/* Only handle messages from first T9 instance */
				data->T9_reportid_min = min_id;
				data->T9_reportid_max = min_id +
							object->num_report_ids - 1;
				data->num_touchids = object->num_report_ids;
				break;
			case MXT_TOUCH_KEYARRAY_T15:
				data->T15_reportid_min = min_id;
				data->T15_reportid_max = max_id;
				break;
			case MXT_SPT_COMMSCONFIG_T18:
				data->T18_address = object->start_address;
				break;
			case MXT_USER_DATA_T38:
				data->T38_address = object->start_address;
				data->T38_size = object->size_minus_one + 1;
				break;
			case MXT_PROCI_TOUCHSUPPRESSION_T42:
				data->T42_reportid_min = min_id;
				data->T42_reportid_max = max_id;
				break;
			case MXT_SPT_MESSAGECOUNT_T44:
				data->T44_address = object->start_address;
				break;
			case MXT_SPT_GPIOPWM_T19:
				data->T19_reportid = min_id;
				break;
			case MXT_PROCG_NOISESUPPRESSION_T48:
				data->T48_reportid = min_id;
				break;
			case MXT_PROCI_ACTIVE_STYLUS_T63:
				/* Only handle messages from first T63 instance */
				data->T63_reportid_min = min_id;
				data->T63_reportid_max = min_id;
				data->num_stylusids = 1;
				break;
			case MXT_UNLOCK_GESTURE_T81:
				data->T81_address = object->start_address;
				data->T81_size = object->size_minus_one + 1;
				data->T81_reportid_min = min_id;
				data->T81_reportid_max = min_id;
				break;
			case MXT_TOUCH_MULTITOUCHSCREEN_T100:
				data->T100_address = object->start_address;
				data->T100_reportid_min = min_id;
				data->T100_reportid_max = max_id;
				/* first two report IDs reserved */
				data->num_touchids = object->num_report_ids - 2;
				break;
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
			case MXT_TOUCH_SEQUENCE_PROCESSOR_T93:
				data->T93_address = object->start_address;
				data->T93_reportid = min_id;
				break;
			case MXT_SYMBOL_GESTURE_PROCESSOR_T115:
				data->T115_address = object->start_address;
				data->T115_reportid = min_id;
				break;
#endif
		}

		end_address = object->start_address
			+ mxt_obj_size(object) * mxt_obj_instances(object) - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	/* If T44 exists, T5 position has to be directly after */
	if (data->T44_address && (data->T5_address != data->T44_address + 1)) {
		dev_err(&client->dev, "Invalid T44 position\n");
		return -EINVAL;
	}

	data->msg_buf = kcalloc(data->max_reportid,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(&client->dev, "Failed to allocate message buffer\n");
		return -ENOMEM;
	}

	return 0;
}

static int mxt_read_t38_object(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	struct mxt_cfg_info *config_info = &data->config_info;
	size_t t38_size = data->T38_size;
	u8 *cfg_buf;
	u8 *info;
	int error = 0;

	if (data->t38_config) {
		kfree(data->t38_config);
		data->t38_config = NULL;
	}

	cfg_buf = kzalloc(t38_size, GFP_KERNEL);
	if (!cfg_buf) {
		dev_err(dev, "%s: Do not have enough memory\n", __func__);
		return -ENOMEM;
	}

	error = __mxt_read_reg(client, data->T38_address, t38_size, cfg_buf);
	if (error) {
		dev_err(dev, "%s Failed to read t38 object\n", __func__);
		goto err_free_mem;
	}

	/* store the config info */
	info = (u8 *)cfg_buf;
	config_info->type = info[0];
	config_info->fw_version = info[2] << 8 | info[1];
	config_info->cfg_version.year = info[3];
	config_info->cfg_version.month = info[4];
	config_info->cfg_version.date = info[5];
	data->t38_config = info;

	dev_info(dev, "%s: T38 address: 0x%x\n"
		"data: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, data->T38_address, info[0], info[1],
		info[2], info[3], info[4], info[5]);

	/* store the pattern type info */
	data->pattern_type = info[0];

	return 0;
err_free_mem:
	kfree(data->t38_config);
	data->t38_config = NULL;
	return error;
}

static int mxt_read_info_block(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	size_t size, byte_offset, read_size;
	void *id_buf, *buf;
	uint8_t num_objects;
	u32 calculated_crc;
	u8 *crc_ptr;

	/* If info block already allocated, free it */
	if (data->raw_info_block != NULL)
		mxt_free_object_table(data);

	/* Read 7-byte ID information block starting at address 0 */
	size = sizeof(struct mxt_info);
	id_buf = kzalloc(size, GFP_KERNEL);
	if (!id_buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	//allen problem
	error = __mxt_read_reg(client, 0, size, id_buf);
	
	if (error) {
		
		kfree(id_buf);
		return error;
	}

	/* Resize buffer to give space for rest of info block */
	num_objects = ((struct mxt_info *)id_buf)->object_num;


	size += (num_objects * sizeof(struct mxt_object))
		+ MXT_INFO_CHECKSUM_SIZE;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	memcpy(buf, id_buf, MXT_OBJECT_START);
	kfree(id_buf);

	/* Read rest of info block */
	byte_offset = MXT_OBJECT_START;
	while (byte_offset < size) {
		if (size - byte_offset > MXT_MAX_BLOCK_READ)
			read_size = MXT_MAX_BLOCK_READ;
		else
			read_size = size - byte_offset;

		error = __mxt_read_reg(client, byte_offset, read_size,
				buf + byte_offset);
		if (error)
			goto err_free_mem;

		byte_offset += read_size;
	}

	/* Extract & calculate checksum */
	crc_ptr = buf + size - MXT_INFO_CHECKSUM_SIZE;
	data->info_crc = crc_ptr[0] | (crc_ptr[1] << 8) | (crc_ptr[2] << 16);

	calculated_crc = mxt_calculate_crc(buf, 0,
					   size - MXT_INFO_CHECKSUM_SIZE);

	/*
	 * CRC mismatch can be caused by data corruption due to I2C comms
	 * issue or else device is not using Object Based Protocol (eg i2c-hid)
	 */
	if ((data->info_crc == 0) || (data->info_crc != calculated_crc)) {
		dev_err(&client->dev,
			"Info Block CRC error calculated=0x%06X read=0x%06X\n",
			calculated_crc, data->info_crc);
		error = -EIO;
		goto err_free_mem;
	}

	data->raw_info_block = buf;
	data->info = (struct mxt_info *)buf;

	pr_debug("Family: %u Variant: %u Firmware V%u.%u.%02X Objects: %u\n",
				data->info->family_id, data->info->variant_id,
				data->info->version >> 4, data->info->version & 0xf,
				data->info->build, data->info->object_num);

	/* Parse object table information */
	error = mxt_parse_object_table(data, buf + MXT_OBJECT_START);
	if (error) {
		dev_err(&client->dev, "Error %d parsing object table\n", error);
		mxt_free_object_table(data);
		return error;
	}

	data->object_table = (struct mxt_object *)(buf + MXT_OBJECT_START);

	error = mxt_read_t38_object(data);
	if (error) {
		dev_err(&client->dev, "%s: Failed to read t38 object\n",__func__);
		return error;
	}

	return 0;

err_free_mem:
	kfree(buf);
	return error;
}

static void mxt_regulator_enable(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	gpio_direction_output(data->pdata->gpio_reset, 0);

	error = regulator_enable(data->reg_avdd);
	if (error) {
		dev_err(dev, "regulator_enable failed  reg_avdd\n");
		return;
	}

	msleep(MXT_REGULATOR_DELAY);
	gpio_direction_output(data->pdata->gpio_reset, 1);
	msleep(MXT_CHG_DELAY);

retry_wait:
	//reinit_completion(&data->bl_completion);//delete by cassy
	reinit_completion_new(&data->bl_completion);//add by cassy
	data->in_bootloader = true;
	error = mxt_wait_for_completion(data, &data->bl_completion,
					MXT_POWERON_DELAY);
	if (error == -EINTR)
		goto retry_wait;
	data->in_bootloader = false;
}

static void mxt_regulator_restore(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	gpio_direction_output(data->pdata->gpio_reset, 0);

	error = regulator_enable(data->reg_avdd);
	if (error) {
		dev_err(dev, "regulator_enable failed  reg_avdd \n");
		return;
	}

	msleep(MXT_REGULATOR_DELAY);
	gpio_direction_output(data->pdata->gpio_reset, 1);
	msleep(MXT_CHG_DELAY);
	//mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);//wuxiaolin add
	enable_irq(mxt_touch_irq);

retry_wait:
	//reinit_completion(&data->bl_completion);//delete by cassy
	reinit_completion_new(&data->bl_completion);//add by cassy
	data->in_bootloader = true;
	error = mxt_wait_for_completion(data, &data->bl_completion,
					MXT_POWERON_DELAY);
	if (error == -EINTR) {
		dev_err(dev, "retry to wait bootloader completion\n");
		goto retry_wait;
	}

	data->in_bootloader = false;
}

static void mxt_regulator_disable(struct mxt_data *data)
{
	if(data->pdata->gpio_reset)
		gpio_direction_output(data->pdata->gpio_reset, 1);

	if(data->reg_avdd)
		regulator_disable(data->reg_avdd);
}

static int mxt_read_t9_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct t9_range range;
	unsigned char orient;
	struct mxt_object *object;

	object = mxt_get_object(data, MXT_TOUCH_MULTI_T9);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T9_RANGE,
			       sizeof(range), &range);
	if (error)
		return error;

	le16_to_cpus(&range.x);
	le16_to_cpus(&range.y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T9_ORIENT,
				1, &orient);
	if (error)
		return error;

	/* Handle default values */
	if (range.x == 0)
		range.x = 1023;

	if (range.y == 0)
		range.y = 1023;

	if (orient & MXT_T9_ORIENT_SWITCH) {
		data->max_x = range.y;
		data->max_y = range.x;
	} else {
		data->max_x = range.x;
		data->max_y = range.y;
	}


	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;
	unsigned int mt_flags = 0;
	int i;

	error = mxt_read_t9_resolution(data);
	if (error)
		dev_warn(dev, "Failed to initialize T9 resolution\n");

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev->name = "Atmel maXTouch Touchscreen";
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	if (pdata->t19_num_keys) {
		__set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);

		for (i = 0; i < pdata->t19_num_keys; i++)
			if (pdata->t19_keymap[i] != KEY_RESERVED)
				input_set_capability(input_dev, EV_KEY,
						     pdata->t19_keymap[i]);

		mt_flags |= INPUT_MT_POINTER;

		input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_X,
				  MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_Y,
				  MXT_PIXELS_PER_MM);

		input_dev->name = "Atmel maXTouch Touchpad";
	} else {
		mt_flags |= INPUT_MT_DIRECT;
	}

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			     0, 255, 0, 0);

	/* For multi touch */
	num_mt_slots = data->num_touchids + data->num_stylusids;
	error = input_mt_init_slots(input_dev, num_mt_slots, mt_flags);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
			     0, 255, 0, 0);

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS);
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS2);
		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For T15 key array */
	if (data->T15_reportid_min) {
		data->t15_keystatus = 0;
        AM("t15_num_keys: %d\n", data->pdata->t15_num_keys);
		for (i = 0; i < data->pdata->t15_num_keys; i++)
			input_set_capability(input_dev, EV_KEY,
					     data->pdata->t15_keymap[i]);
	}

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}

static int mxt_read_t100_config(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct mxt_object *object;
	u16 range_x, range_y;
	u8 cfg, tchaux;
	u8 aux;

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_XRANGE,
			       sizeof(range_x), &range_x);
	if (error)
		return error;

	le16_to_cpus(&range_x);

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_YRANGE,
			       sizeof(range_y), &range_y);
	if (error)
		return error;

	le16_to_cpus(&range_y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_CFG1,
				1, &cfg);
	if (error)
		return error;

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_TCHAUX,
				1, &tchaux);
	if (error)
		return error;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	if (range_y == 0)
		range_y = 1023;

	if (cfg & MXT_T100_CFG_SWITCHXY) {
		data->max_x = range_y;
		data->max_y = range_x;
	} else {
		data->max_x = range_x;
		data->max_y = range_y;
	}

	/* allocate aux bytes */
	aux = 6;

	if (tchaux & MXT_T100_TCHAUX_VECT)
		data->t100_aux_vect = aux++;

	if (tchaux & MXT_T100_TCHAUX_AMPL)
		data->t100_aux_ampl = aux++;

	if (tchaux & MXT_T100_TCHAUX_AREA)
		data->t100_aux_area = aux++;

	dev_info(&client->dev,
		 "T100 Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static int mxt_initialize_t100_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int error;
    int key;
	char num_keys;  //len is NUM_KEY_TYPE
	const unsigned int *keymap;

	error = mxt_read_t100_config(data);
	if (error){
		dev_err(dev, "Failed to initialize T00 resolution\n");
		return error;
	}
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	if (data->pdata->input_name){
		input_dev->name = data->pdata->input_name;
    }
	else{
		input_dev->name = "mtk-tpd";
    }

	mutex_init(&input_dev->mutex);
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &data->client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	set_bit(EV_ABS, input_dev->evbit);
	input_set_capability(input_dev, EV_KEY, KEY_POWER);

	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);

	num_keys = data->pdata->num_keys[T15_T97_KEY];
	keymap = data->pdata->keymap[T15_T97_KEY];
    AM("num_keys: %d\n", num_keys);
    for (key = 0; key < num_keys; key++) {
     
        input_set_capability(input_dev, EV_KEY, keymap[key]);
        AM("mxt keymap[%d]: %d\n", key, keymap[key]);
    }

	input_set_capability(input_dev, EV_KEY, KEY_POWER);

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);

	if (data->t100_aux_ampl)
		input_set_abs_params(input_dev, ABS_PRESSURE,
				     0, 255, 0, 0);

	/* For multi touch */
	error = input_mt_init_slots(input_dev, data->num_touchids,
				    INPUT_MT_DIRECT);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	//input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);

	if (data->t100_aux_area)
		input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				     0, MXT_MAX_AREA, 0, 0);

	if (data->t100_aux_ampl)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE,
				     0, 255, 0, 0);

	if (data->t100_aux_vect)
		input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
				     0, 255, 0, 0);

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}
//add by cassy begin
static int mxt_register_input_device(struct mxt_data *data)
{
	int ret = 0;
	struct device *dev = &data->client->dev;
	if (!data->T9_reportid_min && !data->T100_reportid_min) {
		dev_err(dev, "%s, invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (data->T9_reportid_min) {
		ret = mxt_initialize_t9_input_device(data);
		if (ret) {
			dev_err(dev, "Failed to register t9 input device\n");
			return ret;
		}
	} else if (data->T100_reportid_min) {
		ret = mxt_initialize_t100_input_device(data);
		if (ret) {
			dev_err(dev, "Failed to register t100 input device\n");
			return ret;
		}
	}else
		dev_err(dev, "Failed to find T9 or T100 object\n");

	return ret;
}

//add by cassy end
static int mxt_configure_objects(struct mxt_data *data, const struct firmware *cfg)
{
	struct device *dev = &data->client->dev;
	int error;

	error = mxt_init_t7_power_cfg(data);
	if (error) {
		dev_err(dev, "Failed to initialize power cfg\n");
		goto err_free_object_table;
	}

	if (cfg) {
		error = mxt_update_cfg(data, cfg);
		if (error)
			dev_warn(dev, "Error %d updating config\n", error);
	}

	error = mxt_register_input_device(data);
	if (error) {
		dev_err(dev, "Failed to register input device\n");
		return error;
	}

	return 0;

err_free_object_table:
	mxt_free_object_table(data);
	return error;
}

static int strtobyte(const char *data, u8 *value)
{
	char str[3];

	str[0] = data[0];
	str[1] = data[1];
	str[2] = '\0';

	return kstrtou8(str,16, value);
}

static size_t mxt_convert_text_to_binary(u8 *buffer, size_t len)
{
	int ret;
	int i;
	int j = 0;

	for (i = 0; i < len; i+=2) {
		ret = strtobyte(&buffer[i], &buffer[j]);
		if (ret)
			return -EINVAL;
		j++;
	}

	return (size_t)j;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	bool alt_bootloader_addr = false;
	bool retry = false;

retry_info:
	error = mxt_read_info_block(data);
	if (error) {
retry_bootloader:
		error = mxt_probe_bootloader(data, alt_bootloader_addr);
		if (error) {
			if (alt_bootloader_addr) {
				/* Chip is not in appmode or bootloader mode */
				return error;
			}

			dev_info(&client->dev, "Trying alternate bootloader address\n");
			alt_bootloader_addr = true;
			goto retry_bootloader;
		} else {
			if (retry) {
				dev_err(&client->dev, "mxt: Could not recover from bootloader mode, will mark the bootloader status\n");
				data->in_bootloader = true;
				return 0;
			}

			/* Attempt to exit bootloader into app mode */
			mxt_send_bootloader_cmd(data, false);
			msleep(MXT_FW_RESET_TIME);
			retry = true;
			goto retry_info;
		}
	}


	/* Enable general touch event reporting */
	mxt_config_ctrl_set(data, data->T100_address, 0x02);

	error = mxt_check_retrigen(data);
	if (error)
		goto err_free_object_table;

	error = mxt_acquire_irq(data);
	if (error)
		goto err_free_object_table;

	error = mxt_debug_msg_init(data);
	if (error)
		goto err_free_object_table;

	return 0;

err_free_object_table:
	mxt_free_object_table(data);
	return error;
}
#if 0//delete by cassy begin
static int mxt_register_input_device(struct mxt_data *data)
{
	int ret = 0;
	struct device *dev = &data->client->dev;
	if (!data->T9_reportid_min && !data->T100_reportid_min) {
		dev_err(dev, "%s, invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (data->T9_reportid_min) {
		ret = mxt_initialize_t9_input_device(data);
		if (ret) {
			dev_err(dev, "Failed to register t9 input device\n");
			return ret;
		}
	} else if (data->T100_reportid_min) {
		ret = mxt_initialize_t100_input_device(data);
		if (ret) {
			dev_err(dev, "Failed to register t100 input device\n");
			return ret;
		}
	}else
		dev_err(dev, "Failed to find T9 or T100 object\n");

	return ret;
}
#endif//delete by cassy end
/* Configuration crc check sum is returned as hex xxxxxx */
static ssize_t mxt_config_csum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%06x\n", data->config_crc);
}
static ssize_t mxt_lockdown_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
					data->lockdown_info[0], data->lockdown_info[1],
					data->lockdown_info[2], data->lockdown_info[3],
					data->lockdown_info[4], data->lockdown_info[5],
					data->lockdown_info[6], data->lockdown_info[7]);
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_fw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u.%u.%02X\n",
			 data->info->version >> 4, data->info->version & 0xf,
			 data->info->build);
}
/* Firmware Config Version is returned as YearMonthDay */
static ssize_t mxt_cfg_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_cfg_version *cfg_version = &data->config_info.cfg_version;
	return scnprintf(buf, PAGE_SIZE, "%02d%02d%02d\n",
			 cfg_version->year,
			 cfg_version->month,
			 cfg_version->date);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t mxt_hw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u.%u\n",
			data->info->family_id, data->info->variant_id);
}

static ssize_t mxt_show_instance(char *buf, int count,
				 struct mxt_object *object, int instance,
				 const u8 *val)
{
	int i;

	if (mxt_obj_instances(object) > 1)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "Instance %u\n", instance);

	for (i = 0; i < mxt_obj_size(object); i++)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\t[%2u]: %02x (%d)\n", i, val[i], val[i]);
	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t mxt_object_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_object *object;
	int count = 0;
	int i, j;
	int error;
	u8 *obuf;

	/* Pre-allocate buffer large enough to hold max sized object. */
	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	error = 0;
	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_readable(object->type))
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"T%u:\n", object->type);

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			error = __mxt_read_reg(data->client, addr, size, obuf);
			if (error)
				goto done;

			count = mxt_show_instance(buf, count, object, j, obuf);
		}
	}

done:
	kfree(obuf);
	return error ?: count;
}

static int mxt_check_firmware_format(struct device *dev,
				     const struct firmware *fw)
{
	unsigned int pos = 0;
	char c;

	while (pos < fw->size) {
		c = *(fw->data + pos);

		if (c < '0' || (c > '9' && c < 'A') || c > 'F')
			return 0;

		pos++;
	}

	/*
	 * To convert file try:
	 * xxd -r -p mXTXXX__APP_VX-X-XX.enc > maxtouch.fw
	 */
	dev_err(dev, "Firmware file isn't in binary format\n");

	return -EINVAL;
}

static u8 mxt_read_chg(struct mxt_data *data)
{
	int gpio_intr = data->pdata->gpio_irq;

	u8 val = (u8)gpio_get_value(gpio_intr);
	return val;
}

static int mxt_wait_for_chg(struct mxt_data *data)
{
	int timeout_counter = 0;
	int count = 10;

	while ((timeout_counter++ <= count) && mxt_read_chg(data))
		mdelay(10);

	if (timeout_counter > count) {
		dev_err(&data->client->dev, "mxt_wait_for_chg() timeout!\n");
	}

	return 0;
}

static int mxt_load_fw(struct device *dev)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	unsigned int frame_size=0;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;
	size_t len = 0;
	u8 *buffer;

	ret = request_firmware(&fw, data->fw_name, dev);
	if (ret < 0) {
		dev_err(dev, "Unable to open firmware %s\n", data->fw_name);
		return ret;
	}

	buffer = kmalloc(fw->size, GFP_KERNEL);
	if (!buffer) {
		dev_err(dev, "malloc firmware buffer failed!\n");
		return -ENOMEM;
	}
	memcpy(buffer, fw->data, fw->size);
	len = fw->size;

	/* Check for incorrect enc file */
	ret = mxt_check_firmware_format(dev, fw);
	if (ret) {
		dev_info(dev, "text format, convert it to binary!\n");
		len = mxt_convert_text_to_binary(buffer, len);
		if (len <= 0)
			goto release_firmware;
	}


	if (!data->in_bootloader) {
		/* Change to the bootloader mode */
		ret = mxt_t6_command(data, MXT_COMMAND_RESET,
		        MXT_BOOT_VALUE, false);
		if (ret)
		    goto release_firmware;

		msleep(MXT_RESET_TIME);

		/* Do not need to scan since we know family ID */
		ret = mxt_lookup_bootloader_address(data, 0);
		if (ret)
			goto release_firmware;
		data->in_bootloader = true;
	}

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (ret) {
		mxt_wait_for_chg(data);
		/* Bootloader may still be unlocked from previous attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, false);
		AM("\n");
		if (ret)
			dev_info(dev, "bootloader status abnormal, but will try updating anyway\n");
	} else {

		msleep(100);
		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		msleep(MXT_RESET_TIME);
		if (ret)
			goto release_firmware;
        	AM("\n");
	}

	while (pos < len) {
		mxt_wait_for_chg(data);
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, true);
		if (ret)
			dev_info(dev, "bootloader status abnormal, but will try updating anyway\n");

		frame_size = ((*(buffer + pos) << 8) | *(buffer + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data, buffer + pos, frame_size);
		if (ret)
			goto release_firmware;

		mxt_wait_for_chg(data);
		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS, true);
		if (ret) {
			retry++;
			AM("retry: %d\n", retry);
			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				dev_err(dev, "Retry count exceeded\n");
				goto release_firmware;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}
		if (frame % 10 == 0){

		}
    	}

	data->in_bootloader = false;

release_firmware:
	release_firmware(fw);
	if (buffer)
		kfree(buffer);
	return ret;
}

static int mxt_update_file_name(struct device *dev, char **file_name,
				const char *buf, size_t count)
{
	char *file_name_tmp;

	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	file_name_tmp = krealloc(*file_name, count + 1, GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	*file_name = file_name_tmp;
	memcpy(*file_name, buf, count);

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (buf[count - 1] == '\n')
		(*file_name)[count - 1] = '\0';
	else
		(*file_name)[count] = '\0';

	return 0;
}

static bool is_need_to_update_fw(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	const struct firmware *cfg = NULL;
	const char *config_name;
	int ret;
	int offset;
	int data_pos;
	int i;
	u32 info_crc, config_crc;
	dev_info(dev, "Enter mxt_check_fw_version\n");

	config_name = data->cfg_name;

	if (config_name == NULL) {
		dev_info(dev, "Not found matched config!\n");
		return -ENOENT;
	}

	ret = request_firmware(&cfg, config_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n", config_name);
		return 0;
	}

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = -EINVAL;
		goto release;
	}

	data_pos = strlen(MXT_CFG_MAGIC);

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {
		ret = sscanf(cfg->data + data_pos, "%hhx%n",
				 (unsigned char *)&cfg_info + i,
				 &offset);
		if (ret != 1) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release;
		}

		data_pos += offset;
	}

	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	if (info_crc == data->info_crc) {
		ret = 0;//FW is the same
		dev_info(dev, "Firmware does not need updating\n");
	} else {
		ret = 1;//FW is different
		dev_info(dev, "Firmware needs updating\n");
	}

	release:
	release_firmware(cfg);
	return ret;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;

	error = mxt_update_file_name(dev, &data->fw_name, buf, count);
	if (error)
		return error;
	if(!is_need_to_update_fw(data)){
		dev_info(dev, "no need to update fw\n");
		return count;
	}
	error = mxt_load_fw(dev);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		data->suspended = false;

		msleep(200);

		error = mxt_initialize(data);
		if (error)
			return error;
	}

	return count;
}

static ssize_t mxt_update_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *cfg;
	int ret = 0;

	if (data->in_bootloader) {
		dev_err(dev, "Not in appmode\n");
		return -EINVAL;
	}
	ret = mxt_update_file_name(dev, &data->cfg_name, buf, count);
	if (ret)
		return ret;

	ret = request_firmware(&cfg, data->cfg_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n",
			data->cfg_name);
		ret = -ENOENT;
		goto out;
	} else {
		
	}

	data->updating_config = true;

	mxt_free_input_device(data);
	if (data->suspended) {
		if (data->use_regulator) {
			enable_irq(mxt_touch_irq);
			mxt_regulator_enable(data);
		} else {
			mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
			mxt_acquire_irq(data);
		}
		AM("\n");
		data->suspended = false;
	}
	ret = mxt_configure_objects(data, cfg);
	if (ret) {
		dev_err(dev, "Failed to download config file %s\n", __func__);
		goto out;
	}
	ret = count;
out:
	data->updating_config = false;
	return ret;
}

static ssize_t mxt_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 1;
	unsigned int reset;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	msleep(MXT_CALIBRATE_DELAY);
	/* Recalibrate to avoid touch panel chaos */
	mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);

	dev_err(dev, "%s: after calibrate\n", __func__);

	return ret;
}

static ssize_t mxt_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char c;

	c = data->debug_enabled ? '1' : '0';
	return scnprintf(buf, PAGE_SIZE, "%c\n", c);
}

static ssize_t mxt_debug_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t mxt_debug_v2_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		if (i == 1)
			mxt_debug_msg_enable(data);
		else
			mxt_debug_msg_disable(data);

		return count;
	} else {
		return -EINVAL;
	}
}

static ssize_t mxt_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		return count;
	} else {

		return -EINVAL;
	}
}

static ssize_t mxt_sys_suspend_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct mxt_object *t81_obj= mxt_get_object(data,MXT_UNLOCK_GESTURE_T81);
	int error = 0;

	if (!t81_obj) {
		dev_err(&client->dev, "There is no t81 object\n");
		return 0;
	}

	error = __mxt_read_reg(client,t81_obj->start_address,
		t81_obj->size_minus_one + 1,&data->t81_cfg);
	if (error)
		dev_err(&client->dev, "%s: i2c_sent  failed\n",__func__);

	return scnprintf(buf, PAGE_SIZE,"0x%x\n", data->t81_cfg.ctrl);
}

static ssize_t mxt_sys_suspend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct mxt_object *t81_obj = mxt_get_object(data, MXT_UNLOCK_GESTURE_T81);
	struct mxt_object *t100_obj = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	u8 ctrl = 0, mask1 = 0xfe, mask2 = 0x1;
	u8 mask3 = 0xfd, mask4 = 0x2;
	u8 error = 0, value = 0;
	u8 *cmd = NULL;

	if (!t81_obj) {
		dev_err(&client->dev, "There is no object\n");
		return 0;
	}

	cmd = kmalloc(count + 1, GFP_KERNEL);
	if (cmd == NULL)
		goto release_error;

	memcpy(cmd, buf, count);

	if (cmd[count-1] == '\n')
		cmd[count-1] = '\0';
	else
		cmd[count] = '\0';

	if (!strcmp(cmd, "true")) {
		error = __mxt_read_reg(client,t100_obj->start_address, 1, &ctrl);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);

		value = ctrl & mask3;

		dev_info(dev, "%s: t100 configuration write: 0x%x\n",__func__, value);

		error = __mxt_write_reg(client,t100_obj->start_address, 1,&value);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);

		error = __mxt_read_reg(client, t81_obj->start_address, 1, &ctrl);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);
		value = ctrl | mask2;
		dev_info(&client->dev, "%s: t81_configuration write:0x%x\n",__func__,value);

		error = __mxt_write_reg(client,t81_obj->start_address, 1,&value);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);
	} else if (!strcmp(cmd, "false")) {
		error = __mxt_read_reg(client,t81_obj->start_address,1, &ctrl);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);

		value = ctrl & mask1;
		dev_info(&client->dev, "%s: else t81 configuration write 0x%x\n",__func__,value);

		error = __mxt_write_reg(client,t81_obj->start_address, 1,&value);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);

		error = __mxt_read_reg(client,t100_obj->start_address, 1, &ctrl);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);
		value = ctrl | mask4;
		dev_err(&client->dev, "%s: else t100 configuration write: 0x%x\n",__func__, value);

		error = __mxt_write_reg(client,t100_obj->start_address, 1,&value);
		if (error)
			dev_err(&client->dev, "%s: i2c sent failed\n", __func__);

		}
release_error:
	return count;
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
				       size_t *count)
{
	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_read_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_write_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

//modified by morven

/*
 * Helper function for performing a T6 diagnostic command
 */
static int mxt_T6_diag_cmd(struct mxt_data *data, struct mxt_object *T6,
			   u8 cmd)
{
	int ret;
	u16 addr = T6->start_address + MXT_COMMAND_DIAGNOSTIC;

	ret = mxt_write_reg(data->client, addr, cmd);
	if (ret)
		return ret;

	/*
	 * Poll T6.diag until it returns 0x00, which indicates command has
	 * completed.
	 */
	while (cmd != 0) {
		ret = __mxt_read_reg(data->client, addr, 1, &cmd);
		if (ret)
			return ret;
	}
	return 0;
}
 
unsigned int read_raw_data(struct mxt_data *data, u8 mode)
{	
	struct mxt_object *T6, *T37;
	u8 *obuf;
	ssize_t ret = 0;
	size_t i,j;
	u8 lsb, msb;
	size_t T37_buf_size, num_pages;
	size_t pos;

	if (!data || !data->object_table)
		return -ENODEV;

	T6 = mxt_get_object(data, MXT_GEN_COMMAND_T6);
	T37 = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!T6 || mxt_obj_size(T6) < 6 || !T37 || mxt_obj_size(T37) < 3) {
		dev_err(&data->client->dev, "Invalid T6 or T37 object\n");
		return -ENODEV;
	}

	/* Something has gone wrong if T37_buf is already allocated */
	if (data->T37_buf)
		return -EINVAL;

	T37_buf_size = data->info->matrix_xsize * data->info->matrix_ysize * sizeof(__le16);
	data->T37_buf_size = T37_buf_size;
	data->T37_buf = kmalloc(data->T37_buf_size, GFP_KERNEL);
	if (!data->T37_buf)
		return -ENOMEM;

	/* Temporary buffer used to fetch one T37 page */
	obuf = kmalloc(mxt_obj_size(T37), GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

    enable_irq(mxt_touch_irq);
	num_pages = DIV_ROUND_UP(T37_buf_size, mxt_obj_size(T37) - 2);
	pos = 0;
	for (i = 0; i < num_pages; i++) {
		u8 cmd;
		size_t chunk_len;

		/* For first page, send mode as cmd, otherwise PageUp */
		cmd = (i == 0) ? mode : MXT_T6_CMD_PAGE_UP;
		ret = mxt_T6_diag_cmd(data, T6, cmd);
		if (ret)
			goto err_free_T37_buf;

		ret = __mxt_read_reg(data->client, T37->start_address,
				mxt_obj_size(T37), obuf);
		if (ret)
			goto err_free_T37_buf;

		/* Verify first two bytes are current mode and page # */
		if (obuf[0] != mode) {
			dev_err(&data->client->dev,
				"Unexpected mode (%u != %u)\n", obuf[0], mode);
			ret = -EIO;
			goto err_free_T37_buf;
		}

		if (obuf[1] != i) {
			dev_err(&data->client->dev,
				"Unexpected page (%u != %zu)\n", obuf[1], i);
			ret = -EIO;
			goto err_free_T37_buf;
		}

		/*
		 * Copy the data portion of the page, or however many bytes are
		 * left, whichever is less.
		 */
		chunk_len = min(mxt_obj_size(T37) - 2, T37_buf_size - pos);
		memcpy(&data->T37_buf[pos], &obuf[2], chunk_len);
		pos += chunk_len;
	}
	
    i = 0;
    for(j = 0; j < data->info->matrix_xsize * data->info->matrix_ysize * 2; j += 2)
    {	   
		lsb = data->T37_buf[j] & 0xff;
		msb = data->T37_buf[j+1] & 0xff;
        if((lsb | (msb << 8)) == 0)
            continue;
		data->raw_data_16[i] = lsb | (msb << 8);
		i++;
	if (i >= RAW_DATA_SIZE) {
	pr_debug("oversize problem\n");
	break;
	}
    }

	goto out;

err_free_T37_buf:
	kfree(data->T37_buf);
	data->T37_buf = NULL;
	data->T37_buf_size = 0;
out:
	kfree(obuf);
	enable_irq(data->irq);
	return ret ?: 0;
}

static ssize_t mxt_open_circuit_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ii;
	unsigned int jj;
	short *report_data_16, limit_b, limit_l;

	read_raw_data(data, MXT_T6_CMD_REFS);
	report_data_16 = data->raw_data_16;
	limit_b = data->raw_data_avg + 3500;
	limit_l = data->raw_data_avg - 3500;

	for (ii = 0; ii < TX_NUM; ii++) {
		for (jj = 0; jj < RX_NUM; jj++) {
			if (*report_data_16 > limit_b ||
				*report_data_16 < limit_l ||
				*report_data_16 > THRESHOLD_MAX ||
				*report_data_16 < THRESHOLD_MIN) {
				return snprintf(buf, PAGE_SIZE,
					"TP Open Circuit Detected,\nTx:%d,Rx:%d,raw:%d, threshold:%d\n",
					ii, jj,
					*report_data_16, data->raw_data_avg);
			}
			report_data_16++;
		}
	}
	return snprintf(buf, PAGE_SIZE, "Pass\n");
}

static ssize_t mxt_raw_cap_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ii;
	unsigned int jj;
	int cnt;
	int count = 0;

	unsigned int sum = 0;
	short max = 0;
	short min = 0;
	u16 *report_data_16;

	read_raw_data(data, MXT_T6_CMD_REFS);
	report_data_16 = data->raw_data_16;

	for (ii = 0; ii < TX_NUM; ii++) {
		for (jj = 0; jj < RX_NUM; jj++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "%-4d, ",
					*report_data_16);

			sum += *report_data_16;

			if (max < *report_data_16)
				max = *report_data_16;

			if (ii == 0 && jj == 0)
				min = *report_data_16;
			else if (*report_data_16 < min)
				min = *report_data_16;

			report_data_16++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;
	}
	cnt = snprintf(buf, PAGE_SIZE - count, "\n");
	buf += cnt;
	count += cnt;

	cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d\nrx = %d\n",
			TX_NUM, RX_NUM);
	buf += cnt;
	count += cnt;

	data->raw_data_avg = sum/RAW_DATA_SIZE;
	cnt = snprintf(buf, PAGE_SIZE - count,
			"max = %d, min = %d, average = %d\n",
			max, min, data->raw_data_avg);
	buf += cnt;
	count += cnt;

	return count;
}

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
static ssize_t mxt_double_click_wake_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "double_click_wake_enable:%u ; enable_wakeup_gesture:%u\n",
			data->double_click_wake_enable, data->enable_wakeup_gesture);
}

static ssize_t mxt_double_click_wake_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	data->double_click_wake_enable = input;

    AM("irq: %d\n", data->client->irq);
	if (data->double_click_wake_enable)
		data->enable_wakeup_gesture = true;
	else
		data->enable_wakeup_gesture = false;

    if(data->enable_wakeup_gesture == true){
        enable_irq_wake(data->client->irq);
    } else {
        disable_irq_wake(data->client->irq);
    }

	return count;
}

static ssize_t mxt_down_to_up_wake_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->down_to_up_wake_enable);
}

static ssize_t mxt_down_to_up_wake_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	data->down_to_up_wake_enable = input;

	if (data->down_to_up_wake_enable)
		data->enable_wakeup_gesture = true;
	else
		data->enable_wakeup_gesture = false;

	if(data->enable_wakeup_gesture == true)
		enable_irq_wake(data->client->irq);
	else 
		disable_irq_wake(data->client->irq);

	return count;
}

static ssize_t mxt_up_to_down_wake_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->up_to_down_wake_enable);
}

static ssize_t mxt_up_to_down_wake_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	data->up_to_down_wake_enable = input;

	if (data->up_to_down_wake_enable)
		data->enable_wakeup_gesture = true;
	else
		data->enable_wakeup_gesture = false;

	if(data->enable_wakeup_gesture == true)
		enable_irq_wake(data->client->irq);
	else
		disable_irq_wake(data->client->irq);

	return count;
}

static ssize_t mxt_right_to_left_wake_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->right_to_left_wake_enable);
}

static ssize_t mxt_right_to_left_wake_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	data->right_to_left_wake_enable = input;

	if (data->right_to_left_wake_enable)
		data->enable_wakeup_gesture = true;
	else
		data->enable_wakeup_gesture = false;

	if(data->enable_wakeup_gesture == true)
		enable_irq_wake(data->client->irq);
	else 
		disable_irq_wake(data->client->irq);

	return count;
}

static ssize_t mxt_left_to_right_wake_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->left_to_right_wake_enable);
}

static ssize_t mxt_left_to_right_wake_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	data->left_to_right_wake_enable = input;

	if (data->left_to_right_wake_enable)
		data->enable_wakeup_gesture = true;
	else
		data->enable_wakeup_gesture = false;

	if(data->enable_wakeup_gesture == true)
		enable_irq_wake(data->client->irq);
	else 
		disable_irq_wake(data->client->irq);

	return count;
}

static ssize_t mxt_detected_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->detected_gesture);
}
#endif

static ssize_t touch_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int cmd, arg;
	struct mxt_data *data = dev_get_drvdata(dev);
	struct input_dev *input_dev = data->input_dev;
    if(sscanf(buf,"%d%d", &cmd, &arg) < 0)
        return -EINVAL;
    switch(cmd)
    {
        case 0:   
                    input_report_key(input_dev, KEY_APPSELECT, 1);
                    input_sync(input_dev);
                    input_report_key(input_dev, KEY_APPSELECT, 0);
                    input_sync(input_dev);
                    break;
        case 1:    
                    input_report_key(input_dev, KEY_HOMEPAGE, 1);
                    input_sync(input_dev);
                    input_report_key(input_dev, KEY_HOMEPAGE, 0);
                    input_sync(input_dev);
                    break;
        case 2:     
                    input_report_key(input_dev, KEY_BACK, 1);
                    input_sync(input_dev);
                    input_report_key(input_dev, KEY_BACK, 0);
                    input_sync(input_dev);
                    break;
        case 3:
                    mxt_suspend(dev);
                    break;
        case 4:
                    mxt_resume(dev);
                    break;
        case 5:
                    input_report_key(input_dev, KEY_POWER, 1);
                    input_sync(input_dev);
                    input_report_key(input_dev, KEY_POWER, 0);
                    input_sync(input_dev);
                    break;
        case 6:
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
                    data->enable_wakeup_gesture = true
#endif
                    break;
        default:
                    break;
    }
    return count;
}

static DEVICE_ATTR(touch_debug, 0644, NULL, touch_debug_store);
static DEVICE_ATTR(fw_version, 0644, mxt_fw_version_show, NULL);
static DEVICE_ATTR(cfg_version, 0644, mxt_cfg_version_show, NULL);
static DEVICE_ATTR(hw_version, 0644, mxt_hw_version_show, NULL);
static DEVICE_ATTR(object, 0644, mxt_object_show, NULL);
static DEVICE_ATTR(update_fw, 0644, NULL, mxt_update_fw_store);
static DEVICE_ATTR(update_cfg, 0644, NULL, mxt_update_cfg_store);
static DEVICE_ATTR(reset, 0644, NULL, mxt_reset_store);
static DEVICE_ATTR(debug_v2_enable, 0644, NULL, mxt_debug_v2_enable_store);
static DEVICE_ATTR(debug_notify, 0644, mxt_debug_notify_show, NULL);
static DEVICE_ATTR(debug_enable, 0644, mxt_debug_enable_show, mxt_debug_enable_store);
static DEVICE_ATTR(config_csum, 0644, mxt_config_csum_show, NULL);
static DEVICE_ATTR(sys_suspend, 0644,	mxt_sys_suspend_show, mxt_sys_suspend_store);
static DEVICE_ATTR(open_circuit_test, 0644, mxt_open_circuit_test_show, NULL);
static DEVICE_ATTR(raw_cap_data, 0644, mxt_raw_cap_data_show, NULL);
static DEVICE_ATTR(lockdown_info, 0644, mxt_lockdown_info_show, NULL);

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
static DEVICE_ATTR(double_click_wake_enable, 0644,
	mxt_double_click_wake_enable_show, mxt_double_click_wake_enable_store);
static DEVICE_ATTR(down_to_up_wake_enable, 0644,
	mxt_down_to_up_wake_enable_show, mxt_down_to_up_wake_enable_store);
static DEVICE_ATTR(up_to_down_wake_enable, 0644,
	mxt_up_to_down_wake_enable_show, mxt_up_to_down_wake_enable_store);
static DEVICE_ATTR(right_to_left_wake_enable, 0644,
	mxt_right_to_left_wake_enable_show,
	mxt_right_to_left_wake_enable_store);
static DEVICE_ATTR(left_to_right_wake_enable, 0644,
	mxt_left_to_right_wake_enable_show,
	mxt_left_to_right_wake_enable_store);
static DEVICE_ATTR(detected_gesture, 0644, mxt_detected_gesture_show, NULL);
#endif

static struct attribute *mxt_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_touch_debug.attr,
	&dev_attr_cfg_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_object.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_update_cfg.attr,
	&dev_attr_reset.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_debug_v2_enable.attr,
	&dev_attr_debug_notify.attr,
	&dev_attr_config_csum.attr,
	&dev_attr_sys_suspend.attr,
	&dev_attr_open_circuit_test.attr,
	&dev_attr_raw_cap_data.attr,
	&dev_attr_lockdown_info.attr,
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	&dev_attr_double_click_wake_enable.attr,
	&dev_attr_down_to_up_wake_enable.attr,
	&dev_attr_up_to_down_wake_enable.attr,
	&dev_attr_right_to_left_wake_enable.attr,
	&dev_attr_left_to_right_wake_enable.attr,
	&dev_attr_detected_gesture.attr,
#endif
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static void mxt_reset_slots(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	unsigned int num_mt_slots;
	int id;

	if (!input_dev)
		return;

	num_mt_slots = data->num_touchids + data->num_stylusids;
            AM("num_mt_slots: %d\n", num_mt_slots);

	for (id = 0; id < num_mt_slots; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	mxt_input_sync(data);
}

static void mxt_start(struct mxt_data *data)
{
    AM("\n");
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
    AM("\n");
    if (data->enable_wakeup_gesture) {
        AM("\n");
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
        mxt_t6_command(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE, false);
        AM("\n");
        return;
    }
#endif

    if (!data->suspended || data->in_bootloader)
        return;

    if (data->use_regulator) {
        AM("\n");
        mxt_regulator_restore(data);
    } else {
        AM("\n");
        /*
         * Discard any messages still in message buffer
         * from before chip went to sleep
         */
        mxt_process_messages_until_invalid(data);

        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);

        /* Recalibrate since chip has been in deep sleep */
        mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);

        mxt_acquire_irq(data);
        AM("\n");
    } 
    data->suspended = false;
}

static void mxt_stop(struct mxt_data *data)
{
    u8 value;
    AM("\n");
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
    AM("\n");
    if (data->enable_wakeup_gesture) {
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
        mxt_t6_command(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE, false);
        data->suspended = true;
        return;
    }
#endif 
    if (data->suspended || data->in_bootloader || data->updating_config)
        return;

    __mxt_read_reg(data->client, MXT_TOUCH_SELF_CAPACITANCE_CONFIG_T111 + 21, 1, &value);
    if(value == 255) {
        value = 60;
        __mxt_write_reg(data->client, MXT_TOUCH_SELF_CAPACITANCE_CONFIG_T111 + 21, 1, &value);
    }

    disable_irq(mxt_touch_irq);

    if (data->use_regulator)
    {
        AM("\n");
        mxt_regulator_disable(data);
    }
    else
    {
        AM("\n");
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
    }

    mxt_reset_slots(data);
    data->suspended = true;
}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	mxt_start(data);

	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	struct device *device = &data->client->dev;

	dev_err(device, "%s\n", __func__);
	mxt_stop(data);
}

#ifdef CONFIG_OF
static struct mxt_platform_data *mxt_parse_dt(struct i2c_client *client)
{
	struct mxt_platform_data *pdata;
	struct device *dev = &client->dev;
	struct property *prop;
	unsigned int *keymap;
	int proplen, ret;

    	AM("\n");

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	/* reset gpio */
	pdata->gpio_reset = of_get_named_gpio_flags(dev->of_node,
		"atmel,reset-gpio", 0, NULL);

	/*power gpio*/
	pdata->gpio_vdd = of_get_named_gpio_flags(dev->of_node,
		"atmel,vdd-gpio", 0, NULL);

	/*irq gpio*/
	pdata->gpio_irq = of_get_named_gpio_flags(dev->of_node,
		"atmel,irq-gpio", 0, NULL);
	if(pdata->gpio_irq == 0){
		pdata->gpio_irq = 85;
	}


	of_property_read_string(dev->of_node, "atmel,cfg_name",
				&pdata->cfg_name);

	of_property_read_string(dev->of_node, "atmel,input_name",
				&pdata->input_name);

	of_property_read_string(dev->of_node, "atmel,fw_version",
				&pdata->fw_version);

	prop = of_find_property(dev->of_node, "linux,gpio-keymap", &proplen);
	if (prop) {
		pdata->t19_num_keys = proplen / sizeof(u32);

		keymap = devm_kzalloc(dev,
			pdata->t19_num_keys * sizeof(u32), GFP_KERNEL);
		if (!keymap)
			return NULL;

		pdata->t19_keymap = keymap;

		ret = of_property_read_u32_array(client->dev.of_node,
			"linux,gpio-keymap", keymap, pdata->t19_num_keys);
		if (ret) {
			dev_err(dev,
				"Unable to read device tree key codes: %d\n",
				 ret);
			return NULL;
		}
	}

	return pdata;
}
#endif

static inline void board_gpio_init(const struct mxt_platform_data *pdata)
{

    // if gpio init in board, or use regulator , skip this function

    /* set irq pin input/pull high */
    /*	 power off vdd/avdd   */
    /*		msleep(100);		*/
    /* 	  set reset output 0 	*/
    /*	 power up vdd/avdd	*/
    /*		msleep(50);			*/
    /*	   set reset output 1 	*/
    /*		msleep(200);		*/


}

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = { 0, 0 };

	
	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);

		mxt_touch_irq = irq_of_parse_and_map(node, 0);
		ret =  request_irq(mxt_touch_irq, mxt_interrupt, IRQF_TRIGGER_LOW,"TOUCH_PANEL-eint", NULL);
		if (ret > 0) {
			ret = -1;
			
		}
	}
	return ret;
}
//add by cassy begin
void tpd_gpio_output_ATEML(int pin, int level)
{
	//mutex_lock(&data->debug_msg_lock);//should add by cassy ,notice
	
	if (pin == 2) {
		if (level)
			pinctrl_select_state(pinctrl2, power_output1);
		else
			pinctrl_select_state(pinctrl2, power_output0);
	} 
}

int tpd_gpio_enable_regulator_output(int flag2)
{
//int ret;
if(flag2)
 {
   GTP_GPIO_OUTPUT_ATMEL(GTP_enable_power_PORT,1);
 }
else
{
   GTP_GPIO_OUTPUT_ATMEL(GTP_enable_power_PORT,0);
 }
return 0;	
}

int tpd_enable_regulator_output(int flag_new)

{
	int ret;
		
	if (flag_new)
		{
		
		/*set TP volt*/
		tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
		ret = regulator_set_voltage(tpd->reg, 3000000, 3000000);
		if (ret != 0) {
			
			return ret;
		}
		ret = regulator_enable(tpd->reg);
		if (ret != 0) {
			
			return ret;
		}
		}
	else
	   {
		ret = regulator_disable(tpd->reg);
		if (ret != 0) {
			//printk("[POWER]Fail to disable regulator when init,ret=%d!", ret);
			return ret;
		}
		}
		
		return 0;

}
//add by cassy end

static void tpd_power_on(int flag)
{
	if(flag)
	{
		//evb if;xiaomi else
	 
     #ifdef MTK_POWER
	 tpd_enable_regulator_output(1);
	 #else
		tpd_gpio_enable_regulator_output(1);//reserve for GPIO control power
	 #endif
	}
	else
	{
	
	#ifdef MTK_POWER
	
	tpd_enable_regulator_output(0);
	#else
		tpd_gpio_enable_regulator_output(0);//reserve for GPIO control power
	#endif
	}
}
//add by cassy begin
int tpd_get_gpio_info_atmel(struct i2c_client *pdev)
{
	int ret;

	pinctrl2 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl2)) {
		ret = PTR_ERR(pinctrl2);
		return ret;
	}
		power_output0 = pinctrl_lookup_state(pinctrl2, "state_power_output0");
		if (IS_ERR(power_output0)) {
			ret = PTR_ERR(power_output0);
			return ret;
		}
		else
			{
			//printk("success\n");
			}
		power_output1 = pinctrl_lookup_state(pinctrl2, "state_power_output1");
		if (IS_ERR(power_output1)) {
			ret = PTR_ERR(power_output1);
			return ret;
		}
	return 0;
}


//add by cassy end
static int mxt_probe(struct i2c_client *client,	const struct i2c_device_id *id)
{
	struct mxt_data *data;
	int error;
	
    	int retval;
	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	
	mxt_i2c_data = data;
	mutex_init(&data->bus_access_mutex);
	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/input0", client->adapter->nr, client->addr);

	data->client = client;
	data->pdata = &mxt_platform_data;
	if(!data->pdata)
	{
	}

	i2c_set_clientdata(client, data);

#ifdef CONFIG_OF
	if (!data->pdata && client->dev.of_node){
		data->pdata = mxt_parse_dt(client);
	}
#endif
	
	if (!data->pdata) {
		data->pdata = devm_kzalloc(&client->dev, sizeof(*data->pdata),
					   GFP_KERNEL);
		if (!data->pdata) {
			dev_err(&client->dev, "Failed to allocate pdata\n");
			error = -ENOMEM;
			goto err_free_mem;
		}

	}
	init_completion(&data->bl_completion);
	init_completion(&data->reset_completion);
	init_completion(&data->crc_completion);
	mutex_init(&data->debug_msg_lock);

	thread = kthread_run(touch_event_handler, data, "atmel-tpd");
	if ( IS_ERR(thread) ) {
		retval = PTR_ERR(thread);
		pr_err(" %s: failed to create kernel thread: %d\n",__func__, retval);
	}
//add by cassy begin

tpd_get_gpio_info_atmel(client);

//add by cassy end
/* power supply */
	tpd_power_on(1);
	msleep(10);
	
	GTP_GPIO_OUTPUT(GTP_RST_PORT,0);/*set Reset pin no pullup*/
	msleep(50);
	GTP_GPIO_OUTPUT(GTP_RST_PORT,1);/*set Reset pin pullup*/
	msleep(200);

	GTP_GPIO_OUTPUT(GTP_INT_PORT,0);/*set EINT pin no pullup*/
	GTP_GPIO_AS_INT(GTP_INT_PORT);	/*set EINT mode*/

	error = mxt_initialize(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize device\n");
		goto err_free_object;
	}

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		goto err_free_object;
	}

	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mem_access_attr.attr.name);
		goto err_remove_sysfs_group;
	}

	/*register interrupt*/
	tpd_irq_registration();			
	/*register input device*/
	error = mxt_register_input_device(data);
	if (error)
		dev_err(&data->client->dev, "Failed to register input device\n");

	mxt_read_lockdown_info(data);

	tpd_load_status = 1;
	return 0;

err_remove_sysfs_group:
	sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_free_object:
	mxt_free_object_table(data);
//err_free_irq:
//	free_irq(client->irq, data);
err_free_mem:
	kfree(data);
	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);
	tpd_load_status = 0;
	if (data->mem_access_attr.attr.name)
		sysfs_remove_bin_file(&client->dev.kobj,
				      &data->mem_access_attr);

	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	free_irq(6, data);
	regulator_put(data->reg_avdd);
	regulator_put(data->reg_vdd);
	mxt_free_object_table(data);
	kfree(data);

	return 0;
}

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
void mxt_set_gesture(struct mxt_data *mxt, bool enable)
{
	if (enable) {
		/* Enable gesture, double click, event reporting */
		mxt_config_ctrl_set(mxt, mxt->T93_address, 0x02);

		/* Enable gesture, up, down, left, right, event reporting */
		mxt_config_ctrl_set(mxt, mxt->T115_address, 0x02);
	} else {
		/* Disable gesture, double click, event reporting */
		mxt_config_ctrl_clear(mxt, mxt->T93_address, 0x02);

		/* Disable gesture, up, down, left, right, event reporting */
		mxt_config_ctrl_clear(mxt, mxt->T115_address, 0x02);
	}
}
#endif

#if defined(CONFIG_FB_PM)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct mxt_data *mxt =
		 container_of(work,
			struct mxt_data, fb_notify_work);
	mxt_resume(&(mxt->input_dev->dev));

	/* Enable general touch event reporting */
	mxt_config_ctrl_set(mxt, mxt->T100_address, 0x02);
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	if (mxt->enable_wakeup_gesture)
		mxt_set_gesture(mxt, false);
#endif
}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct mxt_data *mxt = container_of(self, struct mxt_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
				mxt && mxt->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
			mxt->suspended = 0;
#endif

			schedule_work(&mxt->fb_notify_work);

		} else if (*blank == FB_BLANK_POWERDOWN) {
			if (flush_work(&mxt->fb_notify_work))
				pr_warn("%s: waited resume worker finished\n", __func__);

			/* Disable general touch event reporting */
			mxt_config_ctrl_clear(mxt, mxt->T100_address, 0x02);

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
			mxt->suspended = 1;
			if (mxt->enable_wakeup_gesture)
				mxt_set_gesture(mxt, true);
#endif
			mxt_reset_slots(mxt);

			mxt_suspend(&(mxt->input_dev->dev));
		}
	}
	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

//	mutex_lock(&input_dev->mutex);
	if (input_dev->users)
    {
		mxt_stop(data);
    }

//	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

//	mutex_lock(&input_dev->mutex);

    AM("mxt: mxt_resume input_dev->users: %d\n", input_dev->users);

	if (input_dev->users)
	{
		mxt_start(data);
	}

//	mutex_unlock(&input_dev->mutex);

	return 0;
}

static void mxt_tpd_suspend(struct device *h)
{
	struct mxt_data *data = mxt_i2c_data;

	if(data)
    {
		mxt_suspend(&(data->client->dev));
    }
    else
    {
        //printk("mxt: data is NULL\n");
    }
}

static void mxt_tpd_resume(struct device *h)
{
	struct mxt_data *data = mxt_i2c_data;
	
	if(data)
		mxt_resume(&data->client->dev);
}

#endif

static const struct of_device_id mxt_i2c_table[] = {
	{.compatible = "mediatek,atmel_cap_touch",},
	{},
};

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "atmel_mxt_tp", 0 },
	{ "mXT224", 0 },
	{ }
};

MODULE_DEVICE_TABLE(of, mxt_i2c_table);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.name	= "atmel_mxt_ts",
		.owner	= THIS_MODULE,
        .of_match_table = mxt_i2c_table,
	},
	.probe		= mxt_probe,
	.remove		= mxt_remove,
	.id_table	= mxt_id,
};

static int tpd_local_init(void)
{

	if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
		return -1;
	}
#ifdef TPD_HAVE_BUTTON     
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif 

	if(tpd_load_status == 0) {  // disable auto load touch driver for linux3.0 porting
		
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t atmel_mxt_driver = {
	.tpd_device_name = "atmel_mxt_ts",
	.tpd_local_init = tpd_local_init,
	.suspend = mxt_tpd_suspend,
	.resume = mxt_tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif		
};

static struct mxt_platform_data mxt_platform_data = {
	.irqflags = IRQF_TRIGGER_LOW		/*IRQF_TRIGGER_FALLING*/,  
	.num_keys = mxts_num_keys,
	.keymap = mxts_keys,
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	.max_y_t = 1919,     //Max value of Touch AA, asix more than this value will process by VirtualKeyHit
	.vkey_space_ratio = {5,8,15,10},
#endif
#if defined(CONFIG_MXT_SELFCAP_TUNE)
	.config_array = mxt_config_array,
#endif
	.fw_version = "10AB.fw",
	.cfg_name = "10AB.raw"
};

static int __init atmel_mxt_init(void)
{
//	i2c_register_board_info(TPD_I2C_BUS, i2c_tpd, ARRAY_SIZE(i2c_tpd));
	if(tpd_driver_add(&atmel_mxt_driver) < 0){
		pr_err("Fail to add tpd driver\n");
		return -1;
	}

	return 0;
}

static void __exit atmel_mxt_exit(void)
{
	tpd_driver_remove(&atmel_mxt_driver);
	return;
}

module_init(atmel_mxt_init);
module_exit(atmel_mxt_exit);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");

