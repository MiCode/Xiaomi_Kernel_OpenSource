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
#include <linux/hardware_info.h>



#include "atmel_mxt_fw.h"
#include "atmel_mxt_cfg.h"
#include "../../../../misc/mediatek/include/mt-plat/mtk_wd_api.h"


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
struct pinctrl *pinctrl2;
struct pinctrl_state *power_output1,*power_output0;
#ifdef WT_COMPILE_FACTORY_VERSION
#else
#define MXT_AUTO_UPDATE_FW
#endif
	
#define AM(a, args...) printk(KERN_ERR "mxt %s %d " a, __func__, __LINE__, ##args);


#define MXT_LOCKDOWN_OFFSET	4
#define MXT_LOCKDOWN_SIZE	8

#ifndef RTPM_PRIO_TPD
#define	RTPM_PRIO_TPD	0x04
#endif


/* Configuration file */
#define MXT_CFG_MAGIC		"OBP_RAW V1"

#define MXT_INFO_BLOCK_SIZE 0x07
#define MXT_FAMILY_OFFSET 	0x00
#define MXT_VARIANT_OFFSET 	0x01
#define MXT_VERSION_OFFSET  0x02
#define MXT_BUILD_OFFSET  	0x03

#define MXT_FAMILY_OFFSET_CFG 	0x00
#define MXT_VARIANT_OFFSET_CFG 	0x01
#define MXT_VERSION_OFFSET_CFG 	0x02
#define MXT_BUILD_OFFSET_CFG 	0x03

#define MXT_CRC_LOW_CFG		0x0A
#define MXT_CRC_MID_CFG		0x0B
#define MXT_CRC_HI_CFG		0x0C


#define MXT_CFG_ARRAY_INFO_LENGTH		0x0C
#define MXT_CFG_ARRAY_PREFIX_LENGTH		0x06


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
#define MXT_TIMER_T61				61
#define MXT_OBJ_NOISE_T72           72
#define MXT_UNLOCK_GESTURE_T81		81
#define MXT_TOUCH_SEQUENCE_PROCESSOR_T93	93
#define MXT_TOUCH_MULTITOUCHSCREEN_T100		100
#define MXT_AUX_TOUCHCONFIG_T104			104
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

#define THRESHOLD_MAX 28000 
#define THRESHOLD_MIN 21000 
#define RANGE_THRESHOLD 4000

/* MXT_GEN_POWER_T7 field */
struct t7_config {
	u8 idle;
	u8 active;
} __packed;

#define MXT_POWER_CFG_RUN		0
#define MXT_POWER_CFG_DEEPSLEEP		1
#define MXT_POWER_CFG_WAKEUP		2

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

/*T72 field definition - Wrote by david*/
/* MXT_PROCG_NOISESUPPRESSION_T72 */
#define MXT_NOISESUP_CTRL		0
#define MXT_NOISESUP_CALCFG		1
#define MXT_NOISESUP_CFG1		2
#define MXT_NOISESUP_STABCTRL		20
#define MXT_NOISESUP_NOISCTRL		40
#define MXT_NOISESUP_VNOICTRL		60

#define MXT_NOISESUP_VNOILOWNLTHR	77

#define MXT_NOICTRL_ENABLE	(1 << 0)
#define MXT_NOICFG_VNOISY	(1 << 1)
#define MXT_NOICFG_NOISY	(1 << 0)
#define MXT_STABCTRL_DUALXMODE	(1 << 3)
#define MXT_NOISCTRL_DUALXMODE	(1 << 3)
#define MXT_VNOICTRL_DUALXMODE	(1 << 3)

/* T46 CTE Configuration */
#define MXT_SPT_CTECONFIG_T46_CTRL	0

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
#define MXT_RESET_TIME		500	/* msec */
#define MXT_RESET_TIMEOUT	1000	/* msec */
#define MXT_CRC_TIMEOUT		1000	/* msec */
#define MXT_FW_RESET_TIME	3000	/* msec */
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

#define MAX1_WAKEUP_GESTURE_ENABLE

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
/* GESTURE */
#define MXT_GESTURE_UP			0x85
#define MXT_GESTURE_DOWN		0x86
#define MXT_GESTURE_LEFT		0x83
#define MXT_GESTURE_RIGHT		0x84
#define MXT_GESTURE_O			0x6F
#define MXT_GESTURE_W			0x77
#define MXT_GESTURE_M			0x6D
#define MXT_GESTURE_L			0x6C
#define MXT_GESTURE_S			0x73
#define MXT_GESTURE_V			0x76
#define MXT_GESTURE_Z			0x7A
#define MXT_GESTURE_C			0x63
#define MXT_GESTURE_E			0x65



#define MXT_GESTURE_DOUBLE_CLICK_MASK	0x02

#define RIGHT    KEY_RIGHT
#define LEFT     KEY_LEFT
#define DOWN     KEY_DOWN
#define UP       KEY_UP
#define DOUBLE   KEY_U


#define O		KEY_O
#define W		KEY_W
#define M		KEY_M
#define L		KEY_L
#define S		KEY_S
#define V		KEY_V
#define Z		KEY_Z
#define C		KEY_C
#define E		KEY_E


#endif


/********** for esd function ************/

#define MXT_ESDCHECK_ENABLE

#define ESDCHECK_CYCLE_TIME             2000
#define ESD_DISABLE						0
#define ESD_ENABLE						1





#define ADD_MINUS_PERSENT 30
#define TX_NUM 30
#define RX_NUM 15
#define PAGE 128
#define RAW_DATA_SIZE (TX_NUM * RX_NUM)

#define CUST_EINT_TOUCH_PANEL_NUM 		7
#define EINTF_TRIGGER_LOW 				0x00000008
#define CUST_EINTF_TRIGGER_LOW			EINTF_TRIGGER_LOW

#define GTP_RST_PORT    0
#define GTP_INT_PORT    1
#define GTP_enable_power_PORT 2

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
	u8 fw_version_control;
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
	bool irq_enabled;
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
	/*array used for storing selftest result - wrote by david*/
	u8 test_result[6];
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
	u8 T61_reportid_min;
	u8 T61_reportid_max;
	u16 T61_address;
	u16 T61_instances;
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u16 T81_address;
	u8 T81_size;
	u8 T81_reportid_min;
	u8 T81_reportid_max;
	u16 T100_address;
	u8 T100_reportid_min;
	u8 T100_reportid_max;
	u8 T25_reportid_min;
	u8 T25_reportid_max;

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
	u16 T104_address;
	u8  T104_reportid;
	
	bool enable_wakeup_gesture;
	bool double_click_wake_enable;
	bool down_to_up_wake_enable;
	bool up_to_down_wake_enable;
	bool right_to_left_wake_enable;
	bool left_to_right_wake_enable;
	bool mxt_wakeup_gesture_enable;
	int detected_gesture;
#endif

#ifdef MXT_ESDCHECK_ENABLE
	struct mutex esd_detect_mutex;
#endif

	struct mutex bus_access_mutex;
};






#ifdef MXT_ESDCHECK_ENABLE


/*************  for esd function ***************/

struct mxt_esdcheck_st
{
    u8      active              : 1;    /* 1- esd check active, need check esd 0- no esd check */
    u8      suspend             : 1;
    u8      proc_debug          : 1;    /* apk or adb is accessing I2C */
    u8      intr                : 1;    /* 1- Interrupt trigger */
	u8      unused				: 4;
	u16 	esd_detect_cnt;
    u32     hardware_reset_cnt;
    u32     i2c_nack_cnt;
    u32     i2c_dataerror_cnt;
};

static struct delayed_work mxt_esdcheck_work;
static struct workqueue_struct *mxt_esdcheck_workqueue = NULL;
static struct mxt_esdcheck_st mxt_esdcheck_data;

#endif





/*Global variables comes below*/
static struct mxt_platform_data mxt_platform_data;
static int tpd_flag = 0;
static int mxt_vendor_id;
static int mxt_firmware_version;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct task_struct *thread = NULL;

static struct task_struct *fw_upgrade_thread = NULL;

unsigned int mxt_touch_irq = 0;
struct mxt_data *mxt_i2c_data;

#define MXT_INPUT_EVENT_START			0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_OFF	0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_ON	1
#define MXT_INPUT_EVENT_STYLUS_MODE_OFF		2
#define MXT_INPUT_EVENT_STYLUS_MODE_ON		3
#define MXT_INPUT_EVENT_WAKUP_MODE_OFF		4
#define MXT_INPUT_EVENT_WAKUP_MODE_ON		5
#define MXT_INPUT_EVENT_EDGE_DISABLE		6
#define MXT_INPUT_EVENT_EDGE_FINGER		7
#define MXT_INPUT_EVENT_EDGE_HANDGRIP		8
#define MXT_INPUT_EVENT_EDGE_FINGER_HANDGRIP	9
#define MXT_INPUT_EVENT_END			9


#ifdef MAX1_WAKEUP_GESTURE_ENABLE
#define GESTURE_NODE "onoff"
#define GESTURE_DATA  "data"
#define DOUBLE_CLICK 143
#endif

#if 1
#define CTP_PARENT_PROC_NAME      "touchscreen"
#define CTP_OPEN_PROC_NAME        "ctp_openshort_test"
#define CTP_SELF_TEST 			  "tp_selftest"
#define FTS_PROC_LOCKDOWN_FILE    "lockdown_info"
static char tp_lockdown_info[128];

/*****************************************************************************
* Static variables
*****************************************************************************/
#if 1
static int selftest_result = 0;
static struct proc_dir_entry *ctp_selftest_proc = NULL;
static ssize_t ctp_selftest_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos);
static ssize_t ctp_selftest_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos);
static const struct file_operations ctp_selftest_procs_fops =
	{
		.write = ctp_selftest_proc_write,
		.read = ctp_selftest_proc_read,
		.owner = THIS_MODULE,
			};
#endif

static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos);
static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos);
static const struct file_operations ctp_open_procs_fops =
	{
		.write = ctp_open_proc_write,
		.read = ctp_open_proc_read,
		.owner = THIS_MODULE,
	};
static struct proc_dir_entry *ctp_device_proc = NULL;

static int mxt_lockdown_proc_show(struct seq_file *file, void* data)
{
	char temp[40] = {0};

	sprintf(temp, "%s\n", tp_lockdown_info);
	seq_printf(file, "%s\n", temp);

	return 0;
}

static int mxt_lockdown_proc_open (struct inode* inode, struct file* file)
{
	return single_open(file, mxt_lockdown_proc_show, inode->i_private);
}

static const struct file_operations mxt_lockdown_proc_fops =
{
	.open = mxt_lockdown_proc_open,
	.read = seq_read,
};


#endif


#ifdef MXT_ESDCHECK_ENABLE

/*Function declaration comes below*/

/********** for esd funciton ************/
static int mxt_hardware_reset(unsigned int hdelayms, struct mxt_data *data);
static int mxt_esdcheck_tp_reset( struct mxt_data *data);

static bool mxt_read_chip_info( struct mxt_data *data );

static int mxt_do_esd_check(struct mxt_data *data);
static void mxt_esdcheck_function(struct work_struct *work);
static int mxt_esdcheck_set_intr(bool intr);
static int mxt_esdcheck_get_status(void);
static int mxt_esdcheck_switch(bool enable);
static int mxt_esdcheck_suspend(void);
static int mxt_esdcheck_init(void);
static int mxt_esdcheck_exit(void);
static int mxt_write_object(struct mxt_data *data, u8 type, u8 offset, u8 val);
static int mxt_triger_T61_Instance5(struct mxt_data *data, u8 enable);
static int mxt_T6_diag_cmd(struct mxt_data *data, struct mxt_object *T6,u8 cmd);
static void tpd_power_on(int flag);
#endif

static void mxt_reset_slots(struct mxt_data *data);

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,const void *val);
static int __mxt_read_reg(struct i2c_client *client, u16 reg, u16 len,void *val);
static struct mxt_object *mxt_get_object(struct mxt_data *data, u8 type);
static int mxt_write_T72_DualX(struct mxt_data *data,int offset,bool enable);
static int mxt_read_hw_version(struct mxt_data *data);
static int mxt_read_fw_version_control(struct mxt_data *data);
static int mxt_load_fw_data(struct device *dev);
static int mxt_update_cfg_data(struct mxt_data *data);
static bool is_need_to_update_fw_at_probe(struct mxt_data *data);

static void mxt_tpd_suspend(struct device *h);
static void mxt_tpd_resume(struct device *h);
static int mxt_suspend(struct device *h);
static int mxt_resume(struct device *h);
static int mxt_process_messages_until_invalid(struct mxt_data *data);
static void mxt_start(struct mxt_data *data);
static void mxt_stop(struct mxt_data *data);
static int mxt_input_open(struct input_dev *dev);
static void mxt_input_close(struct input_dev *dev);
static int mxt_download_config(struct mxt_data *data, const char *cfg_name);
static int __mxt_update_fw(struct device *dev,
					const char *buf, size_t count);
static int mxt_parse_cfg_and_load(struct mxt_data *data,
	struct mxt_config_info *info, bool force);
static void mxt_disable_irq(struct mxt_data *data);

static void hardwareinfo_set(void *drv_data);  
extern void hardwareinfo_tp_register(void (*fn)(void *), void *driver_data);  

static int mxt_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	char buffer[16];

	if (type == EV_SYN && code == SYN_CONFIG) {
		sprintf(buffer, "%d", value);

		printk("mxt:Gesture on/off : %d",value);
		if (value >= MXT_INPUT_EVENT_START && value <= MXT_INPUT_EVENT_END) {
			if (value == MXT_INPUT_EVENT_WAKUP_MODE_ON ) {
				mxt_i2c_data->mxt_wakeup_gesture_enable = 1;
			} else if(value == MXT_INPUT_EVENT_WAKUP_MODE_OFF){
			       mxt_i2c_data->mxt_wakeup_gesture_enable = 0;
			}else {
			      mxt_i2c_data->mxt_wakeup_gesture_enable = 0;
				printk("mxt :Failed Open/Close Gesture Function!\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}

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

#if 1
static ssize_t gesture_read(struct file *file, char __user * page, size_t size, loff_t * ppos)
{
	int num;

	if(*ppos)
		return 0;

	 num = sprintf(page,"%d\n",mxt_i2c_data->mxt_wakeup_gesture_enable);
	*ppos += num;

	 return num;
}
static ssize_t gesture_write(struct file *filp, const char __user * buff, size_t len, loff_t * off)
{
    int ret;
    char temp[20]="";

    ret = copy_from_user(temp, buff, len);
    if (ret) {
	printk("mxt: gesture_write, <%s> copy_from_user failed.\n", __func__);
        return -EPERM;
    }

    printk("mxt: gesture_write, %s copy_from_user :%s\n",__func__,temp);

    ret = kstrtouint(temp, 0, (unsigned int *)&mxt_i2c_data->mxt_wakeup_gesture_enable);
    if (ret){
	printk("mxt: gesture_write, kstrtouint failed.\n");
        return -EFAULT;
     }
    
    printk("mxt: gesture_write, %s mxt gesture_data.gesture flag :%d\n",__func__,mxt_i2c_data->mxt_wakeup_gesture_enable);

    return len;
}
static const struct file_operations gesture_fops = {
         .owner = THIS_MODULE,
         .read = gesture_read,
         .write = gesture_write,
};

static ssize_t gesture_data_read(struct file *file, char __user * page, size_t size, loff_t * ppos)
{
	int num;

	if(*ppos)
		return 0;

	 num = sprintf(page,"K\n");
	*ppos += num;

	 return num;
}

static ssize_t gesture_data_write(struct file *filp, const char __user * buff, size_t len, loff_t * off)
{
    return len;
}

static const struct file_operations gesture_data_fops = {
         .owner = THIS_MODULE,
         .read = gesture_data_read,
         .write = gesture_data_write,
};

static int gesture_init(struct input_dev *input_dev)
{
    struct proc_dir_entry *proc_entry = NULL;
    struct proc_dir_entry *proc_data = NULL;
    struct proc_dir_entry *parent;
     
    parent = proc_mkdir("gesture", NULL);
    if (!parent) {
        pr_err("%s: failed to create proc entry\n", __func__);
        return -ENOMEM;
    }

    proc_entry = proc_create(GESTURE_NODE, 0666, parent, &gesture_fops);
    if (proc_entry == NULL) {
		printk("CAN't create proc entry /proc/%s !", GESTURE_NODE);
		return -1;
    } else {
		printk("Created proc entry /proc/%s !", GESTURE_NODE);
    }

    proc_data = proc_create(GESTURE_DATA, 0666, parent, &gesture_data_fops);
    if (proc_data == NULL) {
		printk("CAN't create proc entry /proc/%s !", GESTURE_DATA);
		return -1;
    } else {
		printk("Created proc entry /proc/%s !", GESTURE_DATA);
    }
      
    return 0;
}
#endif

#define BOEN_VENDOR     0x1
#define TP_IC_MXT640T   0xA4
	
static void hardwareinfo_set(void*drv_data)
	{
		char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
		char vendor_for_id[HARDWARE_MAX_ITEM_LONGTH];
		char ic_name[HARDWARE_MAX_ITEM_LONGTH];
		int err;
		u8 vendor_id;
		u8 ic_type;
		u8 fw_ver;


		vendor_id = mxt_vendor_id;
		ic_type = mxt_i2c_data->info->family_id;
		fw_ver = mxt_firmware_version;
		
#if 1
		if(vendor_id== BOEN_VENDOR)
		{
			snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"BOEN");
		}else{
			snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"Other vendor");
		}
	
		if(ic_type == TP_IC_MXT640T)
		{
			snprintf(ic_name,HARDWARE_MAX_ITEM_LONGTH,"MXT640T");
		}else{
			snprintf(ic_name,HARDWARE_MAX_ITEM_LONGTH,"Other IC");
		}
#endif
		printk("ttt vendor id :%d, ic_type : %d\n",vendor_id,ic_type);
		snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"%s,%s,FW:0x%x",vendor_for_id,ic_name,fw_ver);
		printk("ttt firmware_ver=%s\n", firmware_ver);
	
		err = hardwareinfo_set_prop(HARDWARE_TP,firmware_ver);
			if (err < 0)
			return ;
	
		return ;
	
	}

static inline void reinit_completion_new(struct completion *x)
{
	init_completion(x);
}


#ifdef MXT_ESDCHECK_ENABLE

/********** for esd funciton ************/
static int mxt_hardware_reset(unsigned int hdelayms, struct mxt_data *data){


	printk("mxt: mxt_hardware_reset, enter!\n");

	printk("mxt: mxt_hardware_reset,shut down vdd!\n");
	tpd_power_on(0);

	printk("mxt: mxt_hardware_reset,pull down Reset pin for 2ms!\n");
	GTP_GPIO_OUTPUT(GTP_RST_PORT,0);/*set Reset pin no pullup*/
	msleep(2);

	mxt_reset_slots(data);
	printk("mxt: mxt_hardware_reset,supply VDD 10ms!\n");
	tpd_power_on(1);
	msleep(10);
	printk("mxt: mxt_hardware_reset,pull up Reset pin!\n");
	GTP_GPIO_OUTPUT(GTP_RST_PORT,1);/*set Reset pin pullup*/
	printk("mxt: mxt_hardware_reset,delay 250ms!\n");
	msleep(hdelayms);
	printk("mxt: mxt_hardware_reset, exit!\n");
	return 0;
}

static int mxt_esdcheck_tp_reset( struct mxt_data *data ){

	printk("mxt: mxt_esdcheck_tp_reset, enter!\n");

	mxt_esdcheck_data.hardware_reset_cnt++;

    mxt_hardware_reset(250,data);

	mxt_esdcheck_data.esd_detect_cnt = 1;

	msleep(20);
	
	printk("mxt: mxt_esdcheck_tp_reset, exit!\n");

	return 0;
}

static bool mxt_read_chip_info( struct mxt_data *data ){

	int i,j;
	int ret;
	u8 lsb, msb;
	int cnt = 0;
	struct mxt_object *T6, *T37;
	u8 yChn = 0;
	u8 obuf[74];
	u16 ref[30];
	
	printk("mxt: mxt_read_chip_info, enter!\n");


	T6 = mxt_get_object(data, MXT_GEN_COMMAND_T6);
	T37 = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!T6 || mxt_obj_size(T6) < 6 || !T37 || mxt_obj_size(T37) < 3) {
		printk("mxt: mxt_read_chip_info, Invalid T6 or T37 object, need execute TP reset!\n");
		return 1;
	}
	
	ret = mxt_T6_diag_cmd(data, T6, MXT_T6_CMD_REFS);

	if(ret)
	{
		printk("mxt:mxt_read_chip_info, send T6 command failed, need execute TP reset!\n");
		return 1;
	}

	ret = __mxt_read_reg(data->client, T37->start_address,
				sizeof(obuf), obuf);

	if(ret)
	{
		printk("mxt:mxt_read_chip_info, read T37 ref failed, need execute TP reset!\n");
		return 1;
	}

	/* Verify first two bytes are current mode and page # */
	if (obuf[0] != MXT_T6_CMD_REFS) {
		printk("mxt:mxt_read_chip_info, the T37 read mode isn't correct, need execute TP reset!\n");
		return 1;
	}

	if (obuf[1] != 0) {
		printk("mxt:mxt_read_chip_info, the T37 read page isn't correct, need execute TP reset!\n");
		return 1;
		
	}

	cnt = 0;
	i = 0;
	
	for(j = 2; j < 32; j += 2)
    {	  
		lsb = obuf[j] & 0xff;
		msb = obuf[j+1] & 0xff;
		ref[i] = lsb | (msb << 8);
		
		
        if((ref[i] < 16000-2000)||(ref[i] > THRESHOLD_MAX+2000)){
            cnt++;
			printk("mxt:mxt_read_info, found ref abnormal at X0Y%d! ref = %d",i,ref[i]);
        	}
		i++;
    }

	for(j = 42; j < 72; j += 2)
    {	  
		lsb = obuf[j] & 0xff;
		msb = obuf[j+1] & 0xff;
		ref[i] = lsb | (msb << 8);
		
		
        if((ref[i] < 16000-2000)||(ref[i] > THRESHOLD_MAX+2000)){
			yChn = i-15;
            cnt++;
			printk("mxt:mxt_read_info, found ref abnormal at X1Y%d ref = %d",yChn,ref[i]);
        	}
		i++;
    }


	if(cnt > 0){

		printk("mxt:mxt_read_chip_info, the T37 read ref is abnormal cnt = %d, need execute TP reset!\n",cnt);
		return 1;
	}
	
#if 0	
	for (i=0; i<3; i++){

		error = __mxt_read_reg(client, 0, 1, &family_id);
		if (error)
        {
            printk("mxt: mxt_read_chip_info, failed ret = %d!!\n", error);
            mxt_esdcheck_data.i2c_nack_cnt++;
        }
        else
        {
            if (family_id == data->info.family_id) /* Upgrade sometimes can't detect */
            {
                break;
            }
            else
            {
                mxt_esdcheck_data.i2c_dataerror_cnt++;
            }
        }

	}

#endif
	/* if can't get correct data, then need hardware reset */
    if (mxt_esdcheck_data.esd_detect_cnt == 0)
    {
        printk("mxt: mxt_read_chip_info, read esd check data failed (0), need execute TP reset!! exit!\n");
        return 1;
    }

	printk("mxt: mxt_read_chip_info, no need reset! the esd_detect_cnt is %d, exit!\n",mxt_esdcheck_data.esd_detect_cnt);
	mutex_lock(&data->esd_detect_mutex);
	mxt_esdcheck_data.esd_detect_cnt = 0;
	mutex_unlock(&data->esd_detect_mutex);

	printk("mxt: mxt_read_chip_info, no need reset! exit!\n");
    return 0;
	
}


static int mxt_triger_T61_Instance5(struct mxt_data *data, u8 enable){


	u8 offset = 26;
	int ret;

	printk("mxt: mxt_triger_T61_instance5, enter!\n");
	
	ret = mxt_write_object(data,MXT_TIMER_T61,offset,enable);
	
	printk("mxt: mxt_triger_T61_instance5, exit!\n");
	
	return ret;
}




static int mxt_do_esd_check(struct mxt_data *data){


    bool    hardware_reset = 0;
	printk("mxt: mxt_do_esd_check, enter!\n");

    /* 1. esdcheck is interrupt, then return */
    if (mxt_esdcheck_data.intr == 1)
    {
        printk("mxt: mxt_do_esd_check, In interrupt state,no check esd, return immediately!!");
        return 0;
    }

    /* 2. check power state, if suspend, no need check esd */
    if (mxt_esdcheck_data.suspend == 1)
    {
        printk("mxt: mxt_do_esd_check, In suspend, not check esd, return immediately!!");
        /* because in suspend state, adb can be used, when upgrade FW, will active ESD check(active = 1)
        *  But in suspend, then will don't queue_delayed_work, when resume, don't check ESD again
        */
        mxt_esdcheck_data.active = 0;
        return 0;
    }

    /* 3. check fts_esdcheck_data.proc_debug state, if 1-proc busy, no need check esd*/
    if (mxt_esdcheck_data.proc_debug == 1)
    {
        printk("mxt: mxt_do_esd_check, In apk or adb command mode, not check esd, return immediately!!");
        return 0;
    }

	 /* 4. Get Chip ID */
    hardware_reset = mxt_read_chip_info(data);
 
    /* 5. If need hardware reset, then handle it here */
    if ( hardware_reset == 1)
    {
        mxt_esdcheck_tp_reset(data);

		if(data->T61_reportid_max){
			
			mxt_triger_T61_Instance5(data,1);

		}
    }

    printk("mxt: mxt_do_esd_check, Exit! NoACK=%d, Error Data=%d, Hardware Reset=%d\n", mxt_esdcheck_data.i2c_nack_cnt, mxt_esdcheck_data.i2c_dataerror_cnt, mxt_esdcheck_data.hardware_reset_cnt);
    return 0;

}



static void mxt_esdcheck_function(struct work_struct *work){

	struct mxt_data *data = mxt_i2c_data;
	printk("mxt: mxt_esdcheck_function, enter!\n");
	mxt_do_esd_check(data);

    if (mxt_esdcheck_data.suspend == 0 )
    {
        queue_delayed_work(mxt_esdcheck_workqueue, &mxt_esdcheck_work, msecs_to_jiffies(ESDCHECK_CYCLE_TIME));
    }

	printk("mxt: mxt_esdcheck_function, exit!\n");
}

static int mxt_esdcheck_set_intr(bool intr)
{
    /* interrupt don't add debug message */
    mxt_esdcheck_data.intr = intr;
    return 0;
}

static int mxt_esdcheck_get_status(void)
{
    /* interrupt don't add debug message */
    return mxt_esdcheck_data.active;
}

#if 0
static int mxt_esdcheck_proc_busy(bool proc_debug)
{
    mxt_esdcheck_data.proc_debug = proc_debug;
    return 0;
}
#endif

static int mxt_esdcheck_switch(bool enable)
{

	printk("mxt: mxt_esdcheck_switch, enter!\n");
    if (enable == 1)
    {
        if (mxt_esdcheck_data.active == 0)
        {
            printk("mxt: mxt_esdcheck_switch, [ESD]: ESD check start!!");
            mxt_esdcheck_data.active = 1;
			mxt_esdcheck_data.esd_detect_cnt = 1;
            queue_delayed_work(mxt_esdcheck_workqueue, &mxt_esdcheck_work, msecs_to_jiffies(ESDCHECK_CYCLE_TIME));
        }
    }
    else
    {
        if (mxt_esdcheck_data.active == 1)
        {
            printk("mxt: mxt_esdcheck_switch, [ESD]: ESD check stop!!");
            mxt_esdcheck_data.active = 0;
			mxt_esdcheck_data.esd_detect_cnt = 0;
            cancel_delayed_work_sync(&mxt_esdcheck_work);
        }
    }

    printk("mxt: mxt_esdcheck_switch, exit!\n");
    return 0;
}

static int mxt_esdcheck_suspend(void)
{
    printk("mxt: mxt_esdcheck_suspend, enter!\n");
    mxt_esdcheck_switch(ESD_DISABLE);
    mxt_esdcheck_data.suspend = 1;
    printk("mxt: mxt_esdcheck_suspend, exit!\n");
    return 0;
}

static int mxt_esdcheck_resume( void )
{
	printk("mxt: mxt_esdcheck_resume, enter!\n");
    mxt_esdcheck_data.esd_detect_cnt = 1;
    mxt_esdcheck_switch(ESD_ENABLE);
    mxt_esdcheck_data.suspend = 0;
    printk("mxt: mxt_esdcheck_resume, exit!\n");
    return 0;
}

static int mxt_esdcheck_init(void)
{
    int ret;
	printk("mxt: mxt_esdcheck_init,enter!\n ");

	memset((u8 *)&mxt_esdcheck_data, 0, sizeof(struct mxt_esdcheck_st));
	mxt_esdcheck_data.esd_detect_cnt = 1;
	
    INIT_DELAYED_WORK(&mxt_esdcheck_work, mxt_esdcheck_function);
    mxt_esdcheck_workqueue = create_workqueue("mxt_esdcheck_wq");
    if (mxt_esdcheck_workqueue == NULL)
    {
        printk("[ESD]: Failed to create esd work queue!!");
		return 1;
    }

    
    ret = mxt_esdcheck_switch(ESD_ENABLE);

	printk("mxt: mxt_esdcheck_init,exit!\n ");
    return ret;
}


static int mxt_esdcheck_exit(void)
{
    

    destroy_workqueue(mxt_esdcheck_workqueue);

   
    return 0;
}


#endif


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
		dev_info(&data->client->dev, "mxt: mxt_read_object,read from object %d, reg 0x%02x, val 0x%x\n",
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

	size = mxt_obj_size(object);
	instances = mxt_obj_instances(object);

	if (offset >= size * instances) {
		printk("mxt: mxt_write_object, Tried to write outside object T%d"
			" offset:%d, size:%d, instance:%d\n", type, offset, size, instances);
		
		dev_err(&data->client->dev, "mxt: mxt_write_object, Tried to write outside object T%d"
			" offset:%d, size:%d\n", type, offset, size);
		return -EINVAL;
	}

	reg = object->start_address;
	if (data->debug_enabled)
		dev_info(&data->client->dev, "mxt: mxt_write_object, write to object %d, reg 0x%02x, val 0x%x\n",
				(int)type, reg + offset, val);
	ret = __mxt_write_reg(data->client, reg + offset, 1, &val);

	return ret;
}

static void mxt_dump_message(struct mxt_data *data, u8 *message)
{
	printk("mxt: mxt_dump_message, MSG: %*ph\n", data->T5_msg_size, message);
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
		dev_err(dev, "mxt: mxt_debug_msg_enable, Failed to allocate buffer\n");
		return;
	}

	data->debug_v2_enabled = true;
	mutex_unlock(&data->debug_msg_lock);

	dev_info(dev, "mxt: mxt_debug_msg_enable, Enabled message output\n");
}

static void mxt_debug_msg_disable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (!data->debug_v2_enabled)
		return;

	dev_info(dev, "mxt: mxt_debug_msg_disable, disabling message output\n");
	data->debug_v2_enabled = false;

	mutex_lock(&data->debug_msg_lock);
	kfree(data->debug_msg_data);
	data->debug_msg_data = NULL;
	data->debug_msg_count = 0;
	mutex_unlock(&data->debug_msg_lock);
	dev_info(dev, "mxt: mxt_debug_msg_disable, Disabled message output\n");
}



/*added by david for T72 DualX control */
static int mxt_write_T72_DualX(struct mxt_data *data,int offset, bool enable)
{

	int error = 0;
	u8 val= 0;

	printk("mxt: mxt: mxt_write_T72_DualX,enter!\n");
	if (!data || !data->object_table)
		return -ENODEV;

	error = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			offset, &val);
	
	if (error) {
		dev_err(&data->client->dev,
			"mxt: mxt_write_T72_DualX, Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return error;
	}

	if(enable){
		
		val |= MXT_STABCTRL_DUALXMODE;
	}
	else{
		val &= ~ MXT_STABCTRL_DUALXMODE;
	}

	error = mxt_write_object(data, MXT_OBJ_NOISE_T72, offset, val);
	if (error)
		dev_err(&data->client->dev,
			"mxt: mxt_write_T72_DualX, Failed to write object %d\n", (int)MXT_OBJ_NOISE_T72);	

	printk("mxt: mxt: mxt_write_T72_DualX,exit!\n");
	return error;

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
		printk("Discarding %u messages\n", data->debug_msg_count);
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
	printk("mxt: mxt_wait_for_completion, enter!\n");
	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		return ret;
	} else if (ret == 0) {
		dev_err(dev, "mxt: mxt_wait_for_completion, Wait for completion time out.\n");
		return -EINVAL;
	}
	printk("mxt: mxt_wait_for_completion, exit!\n");
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
				"mxt: mxt_lookup_bootloader_address, Appmode i2c address 0x%02x not found\n",
				appmode);
			return -EINVAL;
	}

	data->bootloader_addr = bootloader;
    printk("mxt: mxt_lookup_bootloader_address, bootloader i2c address 0x%02x \n", bootloader);
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
	printk("mxt: mxt_probe_bootloader, mxt_bootloader_read return %d\n",ret);
	if (ret)
		return ret;
	
	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "mxt: mxt_probe_bootloader, Detected bootloader, status:%02X%s\n",
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

		dev_err(dev, "mxt: mxt_get_bootloader_version, Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_err(dev, "mxt: mxt_get_bootloader_version, Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state,bool wait)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;

recheck:
	/* - added by david */
	#if 0
	if (wait) {
		/*
		 * In application update mode, the interrupt
		 * line signals state transitions. We must wait for the
		 * CHG assertion before reading the status byte.
		 * Once the status byte has been read, the line is deasserted.
		 */
		ret = mxt_wait_for_completion(data, &data->bl_completion,
					      MXT_FW_CHG_TIMEOUT);
		if (ret) {
			/*
			 * TODO: handle -ERESTARTSYS better by terminating
			 * fw update process before returning to userspace
			 * by writing length 0x000 to device (iff we are in
			 * WAITING_FRAME_DATA state).
			 */
			dev_err(dev, "Update wait error %d\n", ret);
			return ret;
		}
	}
	#endif
	ret = mxt_bootloader_read(data, &val, 1);
	if (ret) {
		dev_err(dev, "mxt: mxt_check_bootloader, %s: i2c recv failed, ret=%d\n",
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
				udelay(100);
				goto recheck;
			} else if (val == MXT_FRAME_CRC_FAIL) {
				dev_err(dev, "mxt: mxt_check_bootloader, Bootloader CRC fail\n");
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "mxt: mxt_check_bootloader, Invalid bootloader mode state 0x%02X\n", val);
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
		if (xfer[1].len > MXT_MAX_BLOCK_READ)
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

	dev_warn(&data->client->dev, "mxt: mxt_get_object, Invalid object type T%u\n", type);
	return NULL;
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	u8 status = msg[1];
	u32 crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	printk("mxt: mxt_proc_t6_msg, process t6 message: 0x%x\n", status);
	complete(&data->crc_completion);
	if (crc != data->config_crc) {
		data->config_crc = crc;
		printk("mxt: mxt_proc_t6_msg, T6 Config Checksum: 0x%06X\n", crc);
	}

	/* Detect reset */
	if (status & MXT_T6_STATUS_RESET){
		complete(&data->reset_completion);
	}

	/* Output debug if status has changed */
	if (status != data->t6_status){
		printk("mxt: mxt_proc_t6_msg, T6 Status 0x%02X%s%s%s%s%s%s%s\n",
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


#ifdef TPD_HAVE_BUTTON
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
#endif

static void mxt_input_sync(struct mxt_data *data)
{

	if(!data->input_dev)
	{
		printk("mxt: mxt_input_sync, input_dev is NULL, do nothing, exit!");
		return;
	}
		
	
	
	input_mt_report_pointer_emulation(data->input_dev,
					  false);
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

	printk("mxt: mxt_proc_t9_message, T9 is no longer a touch object. Are you way behind the trend guy??\n");
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

	printk( "[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: %02X\n", id,
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
	printk("mxt: mxt_proc_t81_msg, process t81 message.\n");
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
	int i;
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

		if (status & MXT_T100_SUP)
		{
			
			for (i = 0; i < data->num_touchids - 2; i++) {
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
			}
			mxt_input_sync(data);
		}
		
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

#ifdef TPD_HAVE_BUTTON

static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	int key;
	bool curr_state, new_state;
	bool sync = false;
	u8 num_keys;
	const unsigned int *keymap;

	unsigned long keystates = le32_to_cpu(msg[2]);

	if(!input_dev)
		return;
	num_keys = data->pdata->num_keys[T15_T97_KEY];
	keymap = data->pdata->keymap[T15_T97_KEY];
	for (key = 0; key < 3; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);
        printk("mxt t15 key: %d, curr_state: %d, new_state: %d\n", key, curr_state, new_state);

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

#endif

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	
	u8 status = msg[1];

	printk("mxt: mxt_proc_t42_msg, process t42 message: 0x%x\n", status);
	
	if (status & MXT_T42_MSG_TCHSUP){}
	else{}
		
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	printk("mxt: mxt_proc_t48_msg, T48 state %d status %02X %s%s%s%s%s\n", state, status,
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

	if(!input_dev)
		return;
	
	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);
	printk("mxt: mxt_proc_t63_msg, process t63 message, id=0x%x\n", id);
	
	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_T63_STYLUS_PRESSURE_MASK;

	printk("[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
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
	printk("mxt: mxt_proc_t93_msg, process t93 message.\n");
	if ((msg[0] == data->T93_reportid) &&
		(msg[1] & MXT_GESTURE_DOUBLE_CLICK_MASK)) {
		data->detected_gesture = DOUBLE;
#if 0 
		input_report_key(dev, KEY_POWER, 1);
		input_sync(dev);
		input_report_key(dev, KEY_POWER, 0);
		input_sync(dev);
#else
		input_report_key(dev, DOUBLE_CLICK, 1);
		input_sync(dev);
		input_report_key(dev, DOUBLE_CLICK, 0);
		input_sync(dev);
#endif

		printk("mxt: double tap!");
	}
}

static void mxt_proc_t115_messages(struct mxt_data *data, u8 *msg)
{
	int gesture_detected = 0;
	struct input_dev *dev = data->input_dev;
	printk("mxt: mxt_proc_t115_msg, process t115 message.\n");
	if (msg[0] != data->T115_reportid)
		return;

	switch (msg[1]) {
		case MXT_GESTURE_DOWN:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = DOWN;
				gesture_detected = 1;
				printk("mxt: gesture down.");
			}
			break;
		case MXT_GESTURE_UP:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = UP;
				gesture_detected = 1;
				printk("mxt: gesture up.");
			}
			break;
		case MXT_GESTURE_LEFT:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = LEFT;
				gesture_detected = 1;

				printk("mxt: gesture left.");
			}
			break;
		case MXT_GESTURE_RIGHT:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = RIGHT;
				gesture_detected = 1;
				printk("mxt: gesture right.");
			}
			break;


		case MXT_GESTURE_O:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = O;
				gesture_detected = 1;
				printk("mxt: gesture O.");
			}

			break;
		case MXT_GESTURE_W:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = W;
				gesture_detected = 1;
				printk("mxt: gesture W.");
			}

			break;

		case MXT_GESTURE_M:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = M;
				gesture_detected = 1;
				printk("mxt: gesture M.");
			}

			break;
			
		case MXT_GESTURE_L:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = L;
				gesture_detected = 1;
				printk("mxt: gesture L.");
			}

			break;

		case MXT_GESTURE_S:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = S;
				gesture_detected = 1;
				printk("mxt: gesture S.");
			}

			break;

		case MXT_GESTURE_E:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = E;
				gesture_detected = 1;
				printk("mxt: gesture E.");
			}

			break;	
		case MXT_GESTURE_V:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = V;
				gesture_detected = 1;
				printk("mxt: gesture V.");
			}

			break;

		case MXT_GESTURE_Z:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = Z;
				gesture_detected = 1;
				printk("mxt: gesture Z.");
			}

			break;

		case MXT_GESTURE_C:
			if (data->mxt_wakeup_gesture_enable) {
				data->detected_gesture = C;
				gesture_detected = 1;
				printk("mxt: gesture C.");
			}

			break;
			
		default:
			break;
	}

	if (gesture_detected) {
		input_report_key(dev, data->detected_gesture, 1);
		input_sync(dev);
		input_report_key(dev, data->detected_gesture, 0);
		input_sync(dev);
	}
}
#endif



static void mxt_proc_t61_messages(struct mxt_data *data, u8 *msg)
{
#ifdef MXT_ESDCHECK_ENABLE
		if (msg[0] == data->T61_reportid_max)
		{	
			mutex_lock(&data->esd_detect_mutex);
			mxt_esdcheck_data.esd_detect_cnt++;
			mutex_unlock(&data->esd_detect_mutex);
			printk("mxt: mxt_proc_t61_messages, get T61 message for ESD checking. esdcheck data = %d. \n",mxt_esdcheck_data.esd_detect_cnt);
		}
#endif 
	
}


/*process T25 selftest message - wrote by david*/

static void mxt_proc_t25_messages(struct mxt_data *data, u8 *msg)
{
	printk("mxt: mxt_proc_t25_msg, process t25 message.\n");
	memcpy(data->test_result,
		&msg[1], sizeof(data->test_result));
}
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
	
		#ifdef TPD_HAVE_BUTTON     
			mxt_input_button(data, message);
			data->update_input = true;
		#endif	
		
	} else if (report_id >= data->T25_reportid_min
		   && report_id <= data->T25_reportid_max) {
		mxt_proc_t25_messages(data, message);
	} else if (report_id >= data->T61_reportid_min
		   && report_id <= data->T61_reportid_max) {
		mxt_proc_t61_messages(data, message);
	}else if (report_id >= data->T63_reportid_min 
			&& report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, message);
	} else if (report_id >= data->T15_reportid_min 
			&& report_id <= data->T15_reportid_max) {

	#ifdef TPD_HAVE_BUTTON
		mxt_proc_t15_messages(data, message);
	#endif

		
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	} else if ((report_id == data->T115_reportid) 
			&& data->suspended) {
		mxt_proc_t115_messages(data, message);
	} else if (report_id == data->T93_reportid
			&& data->suspended
			&& data->mxt_wakeup_gesture_enable) {
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
		dev_err(dev, "mxt: mxt_read_and_process_messages, Failed to read %u messages (%d)\n", count, ret);
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
		printk("mxt: T44 count %d exceeded max report id\n", count);
		count = data->max_reportid;
	}

	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		printk("mxt: Unexpected invalid message\n");
		return IRQ_NONE;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		ret = mxt_read_and_process_messages(data, num_left);
		if (ret < 0){
            printk("mxt: mxt_process_messages_t44 handle left messages error\n");
			goto end;
        }
		else if (ret != num_left)
			printk("mxt: Unexpected invalid message in left messages\n");
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
	dev_err(dev, "mxt: mxt_process_messages_until_invalid, CHG pin isn't cleared\n");
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

	struct sched_param param = {.sched_priority = RTPM_PRIO_TPD };

	sched_setscheduler(current, SCHED_RR, &param);

	do{
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);


	if (!data->object_table) {
			printk("mxt: touch_event_handler, current object_table is NULL!\n");
		}

#ifdef MXT_ESDCHECK_ENABLE

		mxt_esdcheck_set_intr(1);
		mutex_lock(&mxt_i2c_data->esd_detect_mutex);
		if(!mxt_esdcheck_data.esd_detect_cnt)  
			mxt_esdcheck_data.esd_detect_cnt=1;
		mutex_unlock(&mxt_i2c_data->esd_detect_mutex);
	
#endif

		if(!data->in_bootloader){
			if (data->T44_address) {
				mxt_process_messages_t44(data);
			} else {
				mxt_process_messages(data);
			}
			enable_irq(mxt_touch_irq);
		}

#ifdef MXT_ESDCHECK_ENABLE

		mxt_esdcheck_set_intr(0);

#endif

	}while(!kthread_should_stop());
	printk("mxt: touch_event_handler, touch_event_handler exit!\n");
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

	printk("mxt: mxt_soft_reset,soft resetting chip enter!\n");

	reinit_completion_new(&data->reset_completion);

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	if (ret){
		dev_err(&data->client->dev, "%s: send t6 reset command failed!\n", __func__);
		return ret;
	}

	ret = mxt_wait_for_completion(data, &data->reset_completion, MXT_RESET_TIMEOUT);
	if (ret){
		printk("mxt: mxt_soft_reset, wait for completion time out.\n");
	}
	printk("mxt: mxt_soft_reset,soft resetting chip exit!\n");
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

	
	if (data->T18_address) {
		error = __mxt_read_reg(client,
				       data->T18_address + MXT_COMMS_CTRL,
				       1, &val);
		if (error)
			return error;

		if (val & MXT_COMMS_RETRIGEN)
			return 0;
	}

	dev_warn(&client->dev, "mxt: mxt_check_retrigen, Enabling RETRIGEN workaround\n");
	data->use_retrigen_workaround = true;
	return 0;
}
static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 0, .idle = 0 };
	struct t7_config *active_mode = &data->t7_cfg;
	struct t7_config wakeup_mode = { .active = 32, .idle = 32 };

	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0){

		printk("mxt: mxt_set_t7_power_cfg,T7 cfg zero, overriding\n");
		data->t7_cfg.active = 255;
		data->t7_cfg.idle = 25;

	}

	switch(sleep){

		case MXT_POWER_CFG_DEEPSLEEP:
			new_config = &deepsleep;
			break;
		case MXT_POWER_CFG_RUN:
			new_config = active_mode;
			break;

		case MXT_POWER_CFG_WAKEUP:
			new_config = &wakeup_mode;
			break;
			
		default:
			new_config = active_mode;
			break;

	}

	
	error = __mxt_write_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), new_config);
	if (error)
		return error;

	printk("mxt: Set T7 ACTV:%d IDLE:%d\n", new_config->active, new_config->idle);

	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data)
{
	int error;
	bool retry = false;
recheck:
	error = __mxt_read_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), &data->t7_cfg);
	if (error)
		return error;
	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0) {
		if (!retry) {
			printk("mxt: mxt_init_t7_power_cfg,T7 cfg zero, resetting\n");
			mxt_soft_reset(data);
			retry = true;
			goto recheck;
		} else {
			printk("mxt: mxt_init_t7_power_cfg,T7 cfg zero after reset, overriding\n");
			data->t7_cfg.active = 255;
			data->t7_cfg.idle = 25;
			return mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		}
	}

	printk("mxt: mxt_init_t7_power_cfg, Initialized power cfg: ACTV %d, IDLE %d\n", data->t7_cfg.active, data->t7_cfg.idle);
	return 0;
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
		dev_err(dev, "mxt: mxt_read_lockdown_info, Failed to send lockdown info read command!\n");
		return ret;
	}

	while (i < 100) {
		ret = mxt_read_object(data, MXT_GEN_COMMAND_T6,
					MXT_COMMAND_DIAGNOSTIC, &val);
		if (ret) {
			dev_err(dev, "mxt: mxt_read_lockdown_info, Failed to read diagnostic!\n");
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
		dev_err(dev, "mxt: mxt_read_lockdown_info,Failed to read lockdown info!\n");

	dev_info(dev, "mxt: mxt_read_lockdown_info,Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
			data->lockdown_info[0], data->lockdown_info[1],
			data->lockdown_info[2], data->lockdown_info[3],
			data->lockdown_info[4], data->lockdown_info[5],
			data->lockdown_info[6], data->lockdown_info[7]);

	printk("mxt: Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
			data->lockdown_info[0], data->lockdown_info[1],
			data->lockdown_info[2], data->lockdown_info[3],
			data->lockdown_info[4], data->lockdown_info[5],
			data->lockdown_info[6], data->lockdown_info[7]);

	return 0;
}



static int mxt_update_cfg_data(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	struct mxt_object *object;
	int ret = 0;
	int offset = 0;
	int data_pos;
	int byte_offset;
	int i;
	int cfg_start_ofs;
	u32 info_crc, config_crc, calculated_crc;

	u32 info_crc_tmp[3];
	u32 cfg_crc_tmp[3];
	u8 cfg_info_tmp[7];
	
	u8 *config_mem;
	size_t config_mem_size;
	
	unsigned int type, instance, size;
	unsigned int type_tmp[2];
	unsigned int instance_tmp[2];
	unsigned int size_tmp[2];

	
	u8 val;
	u16 reg;
	size_t len = 0;;

	printk("mxt: mxt_update_cfg_data,enter!\n");

	len = sizeof(mxt_cfg_data);

	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);
	data_pos = 0;

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {


		cfg_info_tmp[i] = *(u8*)(mxt_cfg_data + data_pos);

		data_pos++;
		offset++;
		
	}

	cfg_info.family_id = cfg_info_tmp[0];
	cfg_info.variant_id = cfg_info_tmp[1];
	cfg_info.version = cfg_info_tmp[2];
	cfg_info.build = cfg_info_tmp[3];
	cfg_info.matrix_xsize = cfg_info_tmp[4];
	cfg_info.matrix_ysize = cfg_info_tmp[5];
	cfg_info.object_num = cfg_info_tmp[6];
		


	
	if (cfg_info.family_id != data->info->family_id) {
		dev_err(dev, "mxt: mxt_update_cfg_data, Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info->variant_id) {
		dev_err(dev, "mxt: mxt_update_cfg_data, Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}
	/* Read CRCs */
	for(i=0;i<3;i++){

		info_crc_tmp[i] = *(u8*)(mxt_cfg_data + data_pos);
		
		data_pos ++;
		offset++;
	}

	info_crc = info_crc_tmp[2]|(info_crc_tmp[1]<<8)|(info_crc_tmp[0]<<16);
	
	printk("mxt: mxt_update_cfg_data, the info_crc is %x, the offset is %d\n",info_crc,offset);


	/* Read Config CRCs*/

	for(i=0;i<3;i++){
		
		cfg_crc_tmp[i] = *(u8*)(mxt_cfg_data + data_pos);
		data_pos ++;
		offset++;
	}

	config_crc = cfg_crc_tmp[2]|(cfg_crc_tmp[1]<<8)|(cfg_crc_tmp[0]<<16);
	printk("mxt: mxt_update_cfg_data, the config_crc is %x, the offset is %d\n",config_crc,offset);
	
	/*
	 * The Info Block CRC is calculated over mxt_info and the object
	 * table. If it does not match then we are trying to load the
	 * configuration from a different chip or firmware version, so
	 * the configuration CRC is invalid anyway.
	 */
	if (info_crc == data->info_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "mxt: mxt_update_cfg_data, CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "mxt: mxt_update_cfg_data, Config CRC 0x%06X: OK\n",
				data->config_crc);
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "mxt: mxt_update_cfg_data, Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev,
			 "mxt: mxt_update_cfg_data, Warning: Info CRC error - device=0x%06X file=0x%06X\n",
			 data->info_crc, info_crc);
	}
	/* Malloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START +
			data->info->object_num * sizeof(struct mxt_object) +
			MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "mxt: mxt_update_cfg_data, Failed to allocate memory\n");
		ret = -ENOMEM;
		goto release;
	}
	while (data_pos < len) {
		
		/* Read type, instance, length */
		for(i=0;i<2;i++){

			type_tmp[i] = *(u8*)(mxt_cfg_data + data_pos);

			data_pos++;
			offset++;
		}
		
		for(i=0;i<2;i++){

			instance_tmp[i] = *(u8*)(mxt_cfg_data + data_pos);

			data_pos++;
			offset++;
		}

		for(i=0;i<2;i++){

			size_tmp[i] = *(u8*)(mxt_cfg_data + data_pos);

			data_pos++;
			offset++;
		}


		type = type_tmp[1]|(type_tmp[0]<<8);
		instance = instance_tmp[1]|(instance_tmp[0])<<8;
		size = size_tmp[1]|(size_tmp[0]<<8);

		printk("mxt: mxt_update_cfg_data, the type is %d, the instance is %d, the size is %d, the offset is %d\n",(int)type,(int)instance,(int)size,(int)offset);

		/* print - wrote by david */
		dev_info(dev, "mxt: mxt_update_cfg_data, write to type = %d, instance = %d, size = %d, offset = %d\n",
					(int)type, (int)instance, (int)size, (int)offset);
		object = mxt_get_object(data, type);
		if (!object) {
			/* Skip object */
			for (i = 0; i < size; i++) {
				val = *(u8*)(mxt_cfg_data + data_pos);

				data_pos ++;
				offset++;
			}
			continue;
		}
		if (size > mxt_obj_size(object)) {
			/*
			 * Either we are in fallback mode due to wrong
			 * config or config from a later fw version,
			 * or the file is corrupt or hand-edited.
			 */
			dev_warn(dev, "mxt: mxt_update_cfg_data, Discarding %zu byte(s) in T%u\n",
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
			dev_warn(dev, "mxt: mxt_update_cfg_data, Zeroing %zu byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}
		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "mxt: mxt_update_cfg_data, Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + mxt_obj_size(object) * instance;

		for (i = 0; i < size; i++) {

			/*
			ret = sscanf(mxt_cfg_data + data_pos, "%hhx%n",
				     &val,
				     &offset);

			if (ret != 1) {
				dev_err(dev, "Bad format in T%d\n", type);
				ret = -EINVAL;
				goto release_mem;
			}
			
			data_pos += offset;
			*/

			val = *(u8*)(mxt_cfg_data + data_pos);

			data_pos++;
			offset++;
			

			if (i > mxt_obj_size(object))
				continue;

			byte_offset = reg + i - cfg_start_ofs;

			if ((byte_offset >= 0)
			    && (byte_offset <= config_mem_size)) {
				*(config_mem + byte_offset) = val;
			} else {
				dev_err(dev, "mxt: mxt_update_cfg_data, Bad object: reg:%d, T%d, ofs=%d\n",
					reg, object->type, byte_offset);
				ret = -EINVAL;
				goto release_mem;
			}
		}

/*
		if (type == MXT_USER_DATA_T38 && data->t38_config) {
				memcpy(config_mem + reg - cfg_start_ofs + MXT_T38_INFO_SIZE, data->t38_config + MXT_T38_INFO_SIZE,
					data->T38_size - MXT_T38_INFO_SIZE);
		}

		*/
	}

	/* Calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < cfg_start_ofs) {
		dev_err(dev, "mxt: mxt_update_cfg_data, Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		ret = 0;
		goto release_mem;
	}

	calculated_crc = mxt_calculate_crc(config_mem,
					   data->T7_address - cfg_start_ofs,
					   config_mem_size);

	if (config_crc > 0 && (config_crc != calculated_crc))
		dev_warn(dev, "mxt: mxt_update_cfg_data, Config CRC error, calculated=%06X, file=%06X\n",
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
			dev_err(dev, "mxt: mxt_update_cfg_data, Config write error, ret=%d\n", ret);
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
	dev_info(dev, "mxt: mxt_update_cfg_data, Config successfully updated\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);
release_mem:
	kfree(config_mem);
release:
	printk("mxt: mxt_update_cfg_data,exit!\n");
	return ret;
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
		dev_err(dev, "mxt: mxt_update_cfg, Unrecognised config file\n");
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
			dev_err(dev, "mxt: mxt_update_cfg,Bad format\n");
			ret = -EINVAL;
			goto release;
		}

		data_pos += offset;
	}
	if (cfg_info.family_id != data->info->family_id) {
		dev_err(dev, "mxt: mxt_update_cfg,Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info->variant_id) {
		dev_err(dev, "mxt: mxt_update_cfg,Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}
	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "mxt: mxt_update_cfg,Bad format: failed to parse Info CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "mxt: mxt_update_cfg,Bad format: failed to parse Config CRC\n");
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
			dev_info(dev, "mxt: mxt_update_cfg,CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "mxt: mxt_update_cfg,Config CRC 0x%06X: OK\n",
				data->config_crc);
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "mxt: mxt_update_cfg,Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev,
			 "mxt: mxt_update_cfg,Warning: Info CRC error - device=0x%06X file=0x%06X\n",
			 data->info_crc, info_crc);
	}
	/* Malloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START +
			data->info->object_num * sizeof(struct mxt_object) +
			MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "mxt: mxt_update_cfg,Failed to allocate memory\n");
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
			dev_err(dev, "mxt: mxt_update_cfg,Bad format: failed to parse object\n");
			ret = -EINVAL;
			goto release_mem;
		}
		data_pos += offset;
		/* print - wrote by david */
		dev_info(dev, "mxt: mxt_update_cfg,write to type = %d, instance = %d, size = %d, offset = %d\n",
					(int)type, (int)instance, (int)size, (int)offset);
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
			dev_warn(dev, "mxt: mxt_update_cfg,Discarding %zu byte(s) in T%u\n",
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
			dev_warn(dev, "mxt: mxt_update_cfg,Zeroing %zu byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}
		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "mxt: mxt_update_cfg,Object instances exceeded!\n");
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
				dev_err(dev, "mxt: mxt_update_cfg,Bad object: reg:%d, T%d, ofs=%d\n",
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
		dev_err(dev, "mxt: mxt_update_cfg,Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		ret = 0;
		goto release_mem;
	}

	calculated_crc = mxt_calculate_crc(config_mem,
					   data->T7_address - cfg_start_ofs,
					   config_mem_size);

	if (config_crc > 0 && (config_crc != calculated_crc))
		dev_warn(dev, "mxt: mxt_update_cfg,Config CRC error, calculated=%06X, file=%06X\n",
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
			dev_err(dev, "mxt: mxt_update_cfg,Config write error, ret=%d\n", ret);
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
	dev_info(dev, "mxt: mxt_update_cfg,Config successfully updated\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);
release_mem:
	kfree(config_mem);
release:
	release_firmware(cfg);
	return ret;
}

static int mxt_acquire_irq(struct mxt_data *data)
{
	int error;

	enable_irq_wake(mxt_touch_irq);

	if (data->use_retrigen_workaround) {
		printk("mxt: mxt_acquire_irq, workaround applied - use_retrigen_workaround\n");
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
	data->T6_address = 0;
	data->T6_reportid = 0;
	data->T7_address = 0;
	data->T7_size = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T15_reportid_min = 0;
	data->T15_reportid_max = 0;
	data->T18_address = 0;
	data->T19_reportid = 0;
	data->T38_address = 0;
	data->T38_size = 0;
	data->T42_reportid_min = 0;
	data->T42_reportid_max = 0;
	data->T44_address = 0;
	data->T48_reportid = 0;
	data->T61_address = 0;
	data->T61_reportid_min = 0;
	data->T61_reportid_max = 0;
	data->T61_instances = 0;
	data->T63_reportid_min = 0;
	data->T63_reportid_max = 0;
	data->T81_address = 0;
	data->T81_size = 0;
	data->T81_reportid_min = 0;
	data->T81_reportid_max = 0;
	data->T100_address = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->T25_reportid_min = 0;
	data->T25_reportid_max = 0;
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

		printk( "mxt: mxt_parse_object_table, T%u Start:%u Size:%zu Instances:%zu Report IDs:%u-%u\n",
			object->type, object->start_address,
			mxt_obj_size(object), mxt_obj_instances(object),
			min_id, max_id);

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
				break;
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

			case MXT_TIMER_T61:
				data->T61_address = object->start_address;
				data->T61_reportid_min = min_id;
				data->T61_reportid_max = max_id;
				data->T61_instances = mxt_obj_instances(object);
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
			case MXT_AUX_TOUCHCONFIG_T104:
				data->T104_address = object->start_address;
				data->T104_reportid = min_id;
				break;

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
		dev_err(&client->dev, "mxt: mxt_parse_object_table, Invalid T44 position\n");
		return -EINVAL;
	}

	data->msg_buf = kcalloc(data->max_reportid,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(&client->dev, "mxt: mxt_parse_object_table, Failed to allocate message buffer\n");
		return -ENOMEM;
	}

	return 0;
}

#if 0
static int mxt_read_T7_config(struct mxt_data *data){


	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	struct t7_config *t7_config = &data->t7_cfg;
	u8 cfg_buf[2];
	int error = 0;

	error = __mxt_read_reg(client, data->T7_address, 2, cfg_buf);
	if (error) {
		dev_err(dev, "%s Failed to read t7 object\n", __func__);
		goto err_free_mem;
	}

	/* store the config info */
	t7_config ->idle = cfg_buf[0];
	t7_config->active = cfg_buf[1];

	printk("mxt: read T7 config, T7 address: 0x%x, idle: %d, active: %d, \n",
		data->T7_address, cfg_buf[0], cfg_buf[1]);

	return 0;
	
err_free_mem:
	t7_config ->idle = 255;
	t7_config->active = 255;
	return error;


}
#endif
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
	config_info->fw_version_control = info[6];
	data->t38_config = info;

	dev_info(dev, "%s: T38 address: 0x%x\n"
		"data: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, data->T38_address, info[0], info[1],
		info[2], info[3], info[4], info[5], info[6]);

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
	
	printk("mxt: mxt_read_info_block start!\n");
	/* If info block already allocated, free it */
	if (data->raw_info_block != NULL)
		mxt_free_object_table(data);

	/* Read 7-byte ID information block starting at address 0 */
	size = sizeof(struct mxt_info);
	id_buf = kzalloc(size, GFP_KERNEL);
	if (!id_buf) {
		dev_err(&client->dev, "mxt: mxt_read_info_block, Failed to allocate memory\n");
		return -ENOMEM;
	}
	
	error = __mxt_read_reg(client, 0, size, id_buf);

	if (error) {
		
		kfree(id_buf);
		return error;
	}

	/* Resize buffer to give space for rest of info block */
	num_objects = ((struct mxt_info *)id_buf)->object_num;

	printk("mxt: read info block, num_objects: %d\n", num_objects);

	size += (num_objects * sizeof(struct mxt_object))
		+ MXT_INFO_CHECKSUM_SIZE;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "mxt: mxt_read_info_block,Failed to allocate memory\n");
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
			"mxt: mxt_read_info_block,Info Block CRC error calculated=0x%06X read=0x%06X\n",
			calculated_crc, data->info_crc);
		error = -EIO;
		goto err_free_mem;
	}

	data->raw_info_block = buf;
	data->info = (struct mxt_info *)buf;

	printk("mxt: read info block, Family: %u Variant: %u Firmware V%u.%u.%02X Objects: %u\n",
				data->info->family_id, data->info->variant_id,
				data->info->version >> 4, data->info->version & 0xf,
				data->info->build, data->info->object_num);

	/* Parse object table information */
	error = mxt_parse_object_table(data, buf + MXT_OBJECT_START);
	if (error) {
		dev_err(&client->dev, "mxt: mxt_read_info_block,Error %d parsing object table\n", error);
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
		dev_err(dev, "mxt: mxt_regulator_enable, regulator_enable failed  reg_avdd\n");
		return;
	}

	msleep(MXT_REGULATOR_DELAY);
	gpio_direction_output(data->pdata->gpio_reset, 1);
	msleep(MXT_CHG_DELAY);

}

static void mxt_regulator_restore(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	gpio_direction_output(data->pdata->gpio_reset, 0);

	error = regulator_enable(data->reg_avdd);
	if (error) {
		dev_err(dev, "mxt: mxt_regulator_restore,regulator_enable failed  reg_avdd \n");
		return;
	}

	msleep(MXT_REGULATOR_DELAY);
	gpio_direction_output(data->pdata->gpio_reset, 1);
	msleep(MXT_CHG_DELAY);

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

	printk("mxt: mxt_read_t9_resolution, Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;
	unsigned int mt_flags = 0;

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

#ifdef TPD_HAVE_BUTTON

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
#endif

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
#ifdef TPD_HAVE_BUTTON

	/* For T15 key array */
	if (data->T15_reportid_min) {
		data->t15_keystatus = 0;
        AM("t15_num_keys: %d\n", data->pdata->t15_num_keys);
		for (i = 0; i < data->pdata->t15_num_keys; i++)
			input_set_capability(input_dev, EV_KEY,
					     data->pdata->t15_keymap[i]);
	}
#endif
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

	printk("mxt: mxt_read_t100_config, enter!\n");
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
		 "mxt: mxt_read_t100_config, exit!, T100 Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static int mxt_initialize_t100_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int error;
	
#ifdef TPD_HAVE_BUTTON
    int key;
	char num_keys;
	const unsigned int *keymap;
#endif

	printk("mxt: mxt_initialize_t100_input_device, enter!\n");
	error = mxt_read_t100_config(data);
	if (error){
		dev_err(dev, "mxt: mxt_initialize_t100_input_device,Failed to initialize T00 resolution\n");
		return error;
	}
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(dev, "mxt: mxt_initialize_t100_input_device,Failed to allocate memory\n");
		return -ENOMEM;
	}

	if (data->pdata->input_name){
		printk("mxt: mxt_initialize_t100_input_device,mxt pdata input name=%s\n",data->pdata->input_name);
		input_dev->name = data->pdata->input_name;
    }
	else{
		input_dev->name = "atmel_mxt_ts T100 touchscreen";
    }

	mutex_init(&input_dev->mutex);
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &data->client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	input_dev->event = mxt_input_event;
	
	set_bit(EV_ABS, input_dev->evbit);
#if 0
    input_set_capability(input_dev, EV_KEY, KEY_POWER);
    input_set_capability(input_dev, EV_KEY, KEY_U);
    input_set_capability(input_dev, EV_KEY, KEY_UP);
    input_set_capability(input_dev, EV_KEY, KEY_DOWN);
    input_set_capability(input_dev, EV_KEY, KEY_LEFT);
    input_set_capability(input_dev, EV_KEY, KEY_RIGHT);
    input_set_capability(input_dev, EV_KEY, KEY_O);
    input_set_capability(input_dev, EV_KEY, KEY_E);
    input_set_capability(input_dev, EV_KEY, KEY_M);
    input_set_capability(input_dev, EV_KEY, KEY_L);
    input_set_capability(input_dev, EV_KEY, KEY_W);
    input_set_capability(input_dev, EV_KEY, KEY_S);
    input_set_capability(input_dev, EV_KEY, KEY_V);
    input_set_capability(input_dev, EV_KEY, KEY_Z);
    input_set_capability(input_dev, EV_KEY, KEY_C);

    __set_bit(KEY_RIGHT, input_dev->keybit);
    __set_bit(KEY_LEFT, input_dev->keybit);
    __set_bit(KEY_UP, input_dev->keybit);
    __set_bit(KEY_DOWN, input_dev->keybit);
    __set_bit(KEY_U, input_dev->keybit);
    __set_bit(KEY_O, input_dev->keybit);
    __set_bit(KEY_E, input_dev->keybit);
    __set_bit(KEY_M, input_dev->keybit);
    __set_bit(KEY_W, input_dev->keybit);
    __set_bit(KEY_L, input_dev->keybit);
    __set_bit(KEY_S, input_dev->keybit);
    __set_bit(KEY_V, input_dev->keybit);
    __set_bit(KEY_C, input_dev->keybit);
    __set_bit(KEY_Z, input_dev->keybit);
#else
	  input_set_capability(input_dev, EV_KEY,DOUBLE_CLICK);
    __set_bit(DOUBLE_CLICK, input_dev->keybit);
#endif


	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	printk("mxt: mxt_initialize_t100_input_device, mxt set input keybit!\n");
	

#ifdef TPD_HAVE_BUTTON
	num_keys = data->pdata->num_keys[T15_T97_KEY];
	keymap = data->pdata->keymap[T15_T97_KEY];
	
    printk("mxt: mxt_initialize_t100_input_device,num_keys: %d\n", num_keys);
	
    for (key = 0; key < num_keys; key++) {
        printk("mxt: mxt_initialize_t100_input_device, mxt T15 key press: %u\n", key);

        input_set_capability(input_dev, EV_KEY, keymap[key]);
        printk("mxt: mxt_initialize_t100_input_device, mxt keymap[%d]: %d\n", key, keymap[key]);
    }
#endif


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
		dev_err(dev, "mxt: mxt_initialize_t100_input_device,Error %d initialising slots\n", error);
		goto err_free_mem;
	}

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
		dev_err(dev, " mxt: mxt_initialize_t100_input_device,Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}
static int mxt_register_input_device(struct mxt_data *data)
{
	int ret = 0;
	struct device *dev = &data->client->dev;

	printk("mxt: mxt_register_input_device, enter!\n");
	
	if (!data->T9_reportid_min && !data->T100_reportid_min) {
		dev_err(dev, "%s, invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (data->T9_reportid_min) {
		ret = mxt_initialize_t9_input_device(data);
		if (ret) {
			dev_err(dev, "mxt: mxt_register_input_device, Failed to register t9 input device\n");
			return ret;
		}
	} else if (data->T100_reportid_min) {
		ret = mxt_initialize_t100_input_device(data);
		if (ret) {
			dev_err(dev, "mxt: mxt_register_input_device, Failed to register t100 input device\n");
			return ret;
		}
	}else{
		dev_err(dev, "mxt: mxt_register_input_device, Failed to find T9 or T100 object\n");
		printk("tttt mxt_register_input_device Failed to find T9 or T100 object\n");
		}
	  gesture_init(data->input_dev);

	printk("mxt: mxt_register_input_device, exit!\n");
	return ret;
}

static int mxt_configure_objects(struct mxt_data *data, const struct firmware *cfg)
{
	struct device *dev = &data->client->dev;
	int error;

	error = mxt_init_t7_power_cfg(data);
	if (error) {
		dev_err(dev, "mxt: mxt_configure_objects, Failed to initialize power cfg\n");
		goto err_free_object_table;
	}

	if (cfg) {
		error = mxt_update_cfg(data, cfg);
		if (error)
			dev_warn(dev, "mxt: mxt_configure_objects, Error %d updating config\n", error);
	}

	error = mxt_register_input_device(data);
	if (error) {
		dev_err(dev, "mxt: mxt_configure_objects, Failed to register input device\n");
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
/* - wrote by david */
static int mxt_update_fw(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;

	char fw_name[MXT_FW_NAME_SIZE];
	int ret;

	u8 version,build;

	version = mxt_fw_data[MXT_VERSION_OFFSET];
	build = mxt_fw_data[MXT_BUILD_OFFSET];

	/* Get the fw name from parsing header file */
	ret = snprintf(fw_name, MXT_FW_NAME_SIZE, "mXT640T_V%d%d.fw", version,build);
	
	if (ret < 0) {
		dev_err(dev, "mxt: mxt_update_fw, TP: %s, Failed to get fw name from H file!\n", __func__);
		return ret;
	}

	printk("mxt: mxt_update_fw - fw name is %s,\n",fw_name);

	ret = __mxt_update_fw(&client->dev, fw_name, strlen(fw_name));
	if (ret) {
		dev_err(dev, "mxt: mxt_update_fw, unable to update firmware\n");
		return ret;
	}

	return 0;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	bool alt_bootloader_addr = false;
	bool retry = false;

	printk("mxt: mxt_initialize, enter!\n");
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
				dev_err(&client->dev, "mxt: Could not recover from bootloader mode, try to flash a firmware image to TP anyway!\n");
				/* - added by david */
				/*
				 * We can reflash from this state, so do not
				 * abort init
				 */
				 #if 1
				data->in_bootloader = true;
				/* Flash a new firmware anyway */
				error = mxt_update_fw(data);
				if (error){
					dev_err(&client->dev, "Failed to update fw\n");
					return error;
				}
				
				/* update the corresponding config data */
				error = mxt_parse_cfg_and_load(data,&data->pdata->info, true);
				if (error) {
					dev_err(&client->dev, "Failed to update the corresponding config data\n");
					return error;
				}
				
				#endif
				data->in_bootloader=false;

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


	error = mxt_debug_msg_init(data);
	if (error)
		goto err_free_object_table;

	return 0;

err_free_object_table:
	mxt_free_object_table(data);

	printk("mxt: mxt_initialize, exit!\n");
	return error;
}
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
	return scnprintf(buf, PAGE_SIZE, "mxt: device Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
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

static ssize_t mxt_fw_version_control_show(struct device *dev,
				   struct device_attribute *attr, char *buf){

	struct mxt_data *data = dev_get_drvdata(dev);
	
	u8 fw_version_ctrl = data->config_info.fw_version_control;
	
	return scnprintf(buf, PAGE_SIZE, "%02d\n",
			fw_version_ctrl);
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
	return scnprintf(buf, PAGE_SIZE, "%u\n",
			data->pattern_type);
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
	int count = 400;

	while ((timeout_counter++ <= count) && mxt_read_chg(data))
		udelay(20);

	if (timeout_counter > count) {
		dev_err(&data->client->dev, "mxt_wait_for_chg() timeout!\n");
	}

	return 0;
}

extern void mtk_wdt_restart(enum wd_restart_type type);
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
	u8 *buffer = NULL;

	ret = request_firmware(&fw, data->fw_name, dev);
	if (ret < 0) {
		dev_err(dev, "mxt: mxt_load_fw, Unable to open firmware %s\n", data->fw_name);
		return ret;
	}

	buffer = kmalloc(fw->size, GFP_KERNEL);
	if (!buffer) {
		dev_err(dev, "mxt: mxt_load_fw, malloc firmware buffer failed!\n");
		return -ENOMEM;
	}
	memcpy(buffer, fw->data, fw->size);
	len = fw->size;

	/* Check for incorrect enc file */
	ret = mxt_check_firmware_format(dev, fw);
	if (ret) {
		dev_info(dev, "mxt: mxt_load_fw, text format, convert it to binary!\n");
		len = mxt_convert_text_to_binary(buffer, len);
		if (len <= 0)
			goto release_firmware;
	}

	/* - added by david */
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
		printk("mxt: mxt_load_fw, has been in bootloader mode! in_bootloader = %d.\n", data->in_bootloader);
	}
	/* - added by david */
	else {
		
	}


	/* - added by david */
	mxt_free_object_table(data);

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (ret) {
		mxt_wait_for_chg(data);
		/* Bootloader may still be unlocked from previous attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, false);
		
		if (ret){
			dev_info(dev, "mxt: mxt_load_fw, bootloader status abnormal, but will try updating anyway\n");

		}
	} else {
		printk("mxt: mxt_load_fw, Unlocking bootloader\n");
		msleep(100);
		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		msleep(MXT_RESET_TIME);
		if (ret)
			goto release_firmware;
        	
	}

	while (pos < len) {
		mxt_wait_for_chg(data);
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, true);
		if (ret){
			
			dev_info(dev, "mxt: mxt_load_fw, bootloader status abnormal, but will try updating anyway\n");
		}

		frame_size = ((*(buffer + pos) << 8) | *(buffer + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data, buffer + pos, frame_size);
		if (ret)
			goto release_firmware;

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS, true);
		if (ret) {
			retry++;
			
			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				dev_err(dev, "mxt: mxt_load_fw,Retry count exceeded\n");
				goto release_firmware;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}
		if (frame % 100 == 0){
			printk("mxt: mxt_load_fw: sent %d frames, %d/%zd bytes\n",
			frame, pos, fw->size);
			mtk_wdt_restart(WD_TYPE_NORMAL);
		}
    	}
	printk("mxt: mxt_load_fw: sent %d frames, %d bytes\n", frame, pos);
	
	data->in_bootloader = false;
	
	printk("mxt: mxt_load_fw: update fw successfully! in_bootloader = %d! \n",data->in_bootloader);
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
		dev_warn(dev, "mxt: mxt_update_file_name, File name too long\n");
		return -EINVAL;
	}

	file_name_tmp = krealloc(*file_name, count + 1, GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "mxt: mxt_update_file_name, no memory\n");
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


static bool is_need_to_update_fw_at_probe(struct mxt_data *data)
{

	
		struct mxt_info *info = data->info;
		u8 family, variant, version, build;
		u32	checksum;
		u8 readData[7];
		u8 i,pos;

		printk("mxt: is_need_to_update_fw_at_probe, enter!\n");
		pos = 0;
		for(i=0;i<7;i++){
			readData[i] = *(u8*)(mxt_fw_data+pos);
			pos++;
		}
		
		family = readData[MXT_FAMILY_OFFSET];
		variant = readData[MXT_VARIANT_OFFSET];
		version = readData[MXT_VERSION_OFFSET];
		build = readData[MXT_BUILD_OFFSET];

		checksum = (u32)readData[6] | (u32)readData[5]<<8 | (u32)readData[4]<<16;

		
		if(info->family_id != family){

			printk("mxt: is_need_to_update_fw_at_probe - wrong family id!, will not update! chip family = %d, fw family id = %d\n",info->family_id,family);
			return false;
		}
		

		#ifdef MXT_FW_UPDATE_IGNORE_VARIANT
		if(info->variant_id != variant){

			printk("mxt: is_need_to_update_fw_at_probe - wrong variant id!, will not update! chip variant = %d, fw variant id = %d\n",info->variant_id,variant);
			return false;
		}
		#else
		if(info->variant_id != variant){

			printk("mxt: is_need_to_update_fw_at_probe - variant id is not same!, will force update! chip variant = %d, fw variant id = %d\n",info->variant_id,variant);
			return true;
		}
		#endif
		
		
		if(info->version > version){
			printk("mxt: is_need_to_update_fw_at_probe - chip firmware version is new!, will not update!, chip version = %d, fw version = %d,\n",info->version,version);
			return false;
			
		}
		else{

			if(info->version == version){

				if(info->build == build){

					printk("mxt: is_need_to_update_fw_at_probe - the firmware is same!, will not update!, chip build = %d, fw build = %d\n",info->build,build);
					return false;
					
				}

				if(info->build > build)
				{
					printk("mxt: is_need_to_update_fw_at_probe - chip firmware build is new!, will not update!, chip build = %d, fw build = %d\n",info->build,build);
					return false;

				}

			}
			

		}

		printk("mxt: is_need_to_update_fw_at_probe - firmware is need to update from the version: 0x%x, build: 0x%x to version: 0x%x, build: 0x%x\n",info->version,info->build,version,build);
		printk("mxt: is_need_to_update_fw_at_probe, exit!\n");
		return true;
		

}



static bool is_need_to_update_fw(struct mxt_data *data)
{
	struct mxt_info *info = data->info;
	struct device *dev = &data->client->dev;

	u8 family, variant, version, build;
	u8 readData[7];
	u8 i,pos;
			
	printk("mxt: is_need_to_update_fw, enter!\n");
	pos = 0;
	for(i=0;i<7;i++){
		readData[i] = *(u8*)(mxt_fw_data+pos);
		pos++;
	}
			
	family = readData[MXT_FAMILY_OFFSET];
	variant = readData[MXT_VARIANT_OFFSET];
	version = readData[MXT_VERSION_OFFSET];
	build = readData[MXT_BUILD_OFFSET];
	

	if(info->family_id != family){

			printk("mxt: is need to update fw - wrong family id!, will not update! chip family = %d, fw family id = %d\n",info->family_id,family);
			return false;
		}
	
	if (info->variant_id == variant && info->version == version && info->build == build) {
		dev_info(dev, "The same fw version\n");
		printk("mxt: update fw - The same fw version,will not update!\n");
		return false;
	}


	dev_info(dev, "TP: version& build are different: need to update fw\n");
	dev_info(dev, "info->version: 0x%x, info->build: 0x%x, version: 0x%x, build: 0x%x\n",
			info->version, info->build, version, build);

	printk("mxt: update fw - TP: version& build are different: need to update fw\n!");

	printk("mxt: update fw - info->version: 0x%x, info->build: 0x%x, version: 0x%x, build: 0x%x\n",info->version, info->build, version, build);
	printk("mxt: is_need_to_update_fw, exit!\n");
	return true;
}

/*update fw and config - wrote by david*/
static int mxt_update_fw_and_cfg(struct mxt_data *data)
{
  
	int error=0;
	
	struct device *dev = &data->client->dev;
	
	if (is_need_to_update_fw_at_probe(data)) {

#ifdef MXT_AUTO_UPDATE_FW
		error = mxt_update_fw(data);
		if (error) {
			dev_err(dev, "mxt: %s: Failed to update fw\n", __func__);
			return -EIO;
		}
#endif
		
		error = mxt_parse_cfg_and_load(data,&data->pdata->info, true);
		if (error) {
			dev_err(dev, "mxt: mxt_update_fw_and_cfg, Failed to update config, after firmware updating\n");
			return -EIO;
		}
	} else {
		error = mxt_parse_cfg_and_load(data, &data->pdata->info, false);
		if (error) {
			dev_err(dev, "mxt: mxt_update_fw_and_cfg, Failed to update config\n");
			return -EIO;
		}
	}
	return 0;
}


/* - wrote by david */
static int mxt_read_hw_version(struct mxt_data *data){

	int error;

	error = mxt_read_t38_object(data);
	if (error) {

		return error;
	}

	return data->pattern_type;

}

static int mxt_read_fw_version_control(struct mxt_data *data){

	int error;

	error = mxt_read_t38_object(data);
	if (error) {

		return error;
	}

	return data->config_info.fw_version_control;

}


/* - wrote by david */
static int __mxt_update_fw(struct device *dev,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;

	error = mxt_update_file_name(dev, &data->fw_name, buf, count);
	if (error)
		return error;

	error = mxt_load_fw_data(dev);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		printk("mxt: __mxt_update_fw, the firmware update failed(%d)\n",error);
		return error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");
		printk("mxt: __mxt_update_fw, the firmware update succeeded\n");
		data->suspended = false;

		msleep(MXT_RESET_TIMEOUT);

		error = mxt_initialize(data);
		if (error)
			return error;
	}

	
	/* - added by david */
	if(!data->in_bootloader){

	}

	return 0;
}


static int mxt_load_fw_data(struct device *dev)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	unsigned int frame_size=0;
	unsigned int pos = MXT_INFO_BLOCK_SIZE;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;
	unsigned int len = 0;
	unsigned int i = 0;
	
	printk("mxt: mxt_load_fw_data, enter!\n");
	len = sizeof(mxt_fw_data);
	
	printk("mxt: mxt_load_fw_data - fw len = %d\n",len);
	
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
		printk("mxt: mxt_load_fw_data, has been in bootloader mode! in_bootloader = %d!", data->in_bootloader);
	}

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (ret) {
		mxt_wait_for_chg(data);
		/* Bootloader may still be unlocked from previous attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, false);
		
		if (ret)
			dev_info(dev, "mxt: mxt_load_fw_data,bootloader status abnormal, but will try updating anyway\n");
	} else {
		printk("mxt: mxt_load_fw_data, - Unlocking bootloader\n");
		msleep(100);
		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		msleep(MXT_RESET_TIME);
		if (ret)
			goto release_firmware;
        	
	}

	while (pos < len) {
		mxt_wait_for_chg(data);

		
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, true);
		if (ret){
			dev_info(dev, "mxt: mxt_load_fw_data,bootloader status abnormal, but will try updating anyway\n");
			printk("mxt: mxt_load_fw_data - bootloader status abnormal, but will try updating anyway\n");
			}
		
		frame_size = ((*(mxt_fw_data + pos) << 8) | *(mxt_fw_data + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;


		/* Write one frame to device */
		ret = mxt_bootloader_write(data, mxt_fw_data + pos, frame_size);
		if (ret)
			goto release_firmware;

		i = pos;

		mxt_wait_for_chg(data);

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS, true);

		if (ret) {
			retry++;
			printk("mxt: load fw retry: %d\n", retry);
			/* Back off by 20ms per retry */
			msleep(retry * 10);

			if (retry > 20) {
				dev_err(dev, "mxt: mxt_load_fw_data, Retry count exceeded\n");
				printk("mxt: mxt_load_fw_data, load fw retry count exceeded!\n");
				goto release_firmware;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}
		if (frame % 10 == 0){
			printk("mxt: mxt_load_fw_data, load fw: sent %d frames, %d/%d bytes, then kick watch dog!\n",
			frame, pos, len);
			mtk_wdt_restart(WD_TYPE_NORMAL);
		}
	}
	printk("mxt: mxt_load_fw_data,load fw: sent %d frames, %d bytes\n", frame, pos);
	printk("mxt: mxt_load_fw_data,update fw successfully!\n");

	data->in_bootloader = false;

	printk("mxt: mxt_load_fw_data,exit! in_bootloader = %d.\n", data->in_bootloader);
release_firmware:
	return ret;


}




/* - added by david */

#ifdef CONFIG_OF
#endif
/* - added by david */
static void mxt_disable_irq(struct mxt_data *data)
{
	
		disable_irq(mxt_touch_irq);
}


static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;


	printk("mxt: mxt_update_fw_store, enter!\n");
		/* pause esd check */
#ifdef MXT_ESDCHECK_ENABLE

	mxt_esdcheck_switch(ESD_DISABLE);
	if(data->T61_reportid_max){
		mxt_triger_T61_Instance5(data,2);
	}
	
#endif


	error = mxt_update_file_name(dev, &data->fw_name, buf, count);
	if (error)
		return error;
	if(!is_need_to_update_fw(data)){
		dev_info(dev, "no need to update fw\n");
		return count;
	}
	/* - added by david */
	mxt_disable_irq(data);

	printk("mxt: update fw store, start update by echo node!");
	error = mxt_load_fw(dev);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		printk("mxt: update fw store,The firmware update failed(%d)\n",error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");
		printk("mxt: update fw store,The firmware update succeeded!\n");
		data->suspended = false;

		/* Wait for reset */
		msleep(MXT_RESET_TIMEOUT);

		error = mxt_initialize(data);
		if (error)
			return error;
	}

	/* - added by david*/
	if(!data->in_bootloader){
		enable_irq(mxt_touch_irq);
		printk("mxt: mxt_update_fw_store, has been exited bootloader mode, data->in_bootloader = %d.\n", data->in_bootloader);
	}


		/* esd check */
#ifdef MXT_ESDCHECK_ENABLE
	if(data->T61_reportid_max){
		mxt_triger_T61_Instance5(data,1);
	}
	mxt_esdcheck_switch(ESD_ENABLE);
#endif


	printk("mxt: mxt_update_fw_store, exit!\n");
	return count;
}

/* update config during the boot up - wrote by david */
static int mxt_download_config(struct mxt_data *data, const char *cfg_name)
{

	int ret=0;

	data->updating_config = true;
	printk("mxt: download config, enter! updatint_config = %d.\n",data->updating_config);
	ret = mxt_update_cfg_data(data);
	if (ret){
		printk("mxt: mxt_download_config, update cfg data error\n");
	}else{
		printk("mxt: mxt_download_config, update cfg data sucessfully\n");
	}
	data->updating_config = false;
	printk("mxt: download config, exit! updatint_config = %d.\n",data->updating_config);
	return ret;

}
static ssize_t mxt_update_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *cfg = NULL;
	int ret = 0;

	printk("mxt: update cfg store, start.");

		/* pause esd check */
#ifdef MXT_ESDCHECK_ENABLE
	mxt_esdcheck_switch(ESD_DISABLE);
	if(data->T61_reportid_max){
		mxt_triger_T61_Instance5(data,2);
	}
	
#endif
	
	if (data->in_bootloader) {
		dev_err(dev, "Not in appmode\n");
		return -EINVAL;
	}
	ret = mxt_update_file_name(dev, &data->cfg_name, buf, count);
	if (ret)
		return ret;

	printk("mxt: update cfg store, data->cfg_name=%s\n", data->cfg_name);
	
	ret = request_firmware(&cfg, data->cfg_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n",
			data->cfg_name);
		ret = -ENOENT;
		goto out;
	} else {
		printk("mxt: update cfg store, success to request config file %s\n",	data->cfg_name);
	}

	data->updating_config = true;

	mxt_free_input_device(data);
	if (data->suspended) {
		if (data->use_regulator) {
			
			mxt_regulator_enable(data);
		} else {
			mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		}
		
		data->suspended = false;
	}

	printk("mxt: update cfg store, start configure objects!\n");
	ret = mxt_configure_objects(data, cfg);
	if (ret) {
		dev_err(dev, "Failed to download config file %s\n", __func__);
		goto out;
	}
	ret = count;
	printk("mxt: update cfg store, succeed update!\n");
out:
	data->updating_config = false;

	/* esd check */
#ifdef MXT_ESDCHECK_ENABLE
	if(data->T61_reportid_max){
		mxt_triger_T61_Instance5(data,1);
	}
	mxt_esdcheck_switch(ESD_ENABLE);
#endif

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
		printk("debug_enabled write error\n");
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

		printk("%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		printk("debug_enabled write error\n");
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

	u8 stabDualx=0,noisDualX=0,vnoiDualX=0;
	u8 t46_ctrl = 0;

	if (!data || !data->object_table)
		return -ENODEV;

	T6 = mxt_get_object(data, MXT_GEN_COMMAND_T6);
	T37 = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!T6 || mxt_obj_size(T6) < 6 || !T37 || mxt_obj_size(T37) < 3) {
		dev_err(&data->client->dev, "Invalid T6 or T37 object\n");
		return -ENODEV;
	}

	ret = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			MXT_NOISESUP_STABCTRL, &stabDualx);

	if (ret) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return ret;
	}

	ret = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			MXT_NOISESUP_NOISCTRL, &noisDualX);

	if (ret) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return ret;
	}

	ret = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			MXT_NOISESUP_VNOICTRL, &vnoiDualX);

	if (ret) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return ret;
	}
	
	printk("mxt: read raw data,read dual-x settings,Stable = %d,Noisy = %d,VNoisy = %d!\n",stabDualx,noisDualX,vnoiDualX);
	
	ret = mxt_write_T72_DualX(data,MXT_NOISESUP_STABCTRL,false);

	if(ret){
		
		return ret;
	}
	
	ret = mxt_write_T72_DualX(data,MXT_NOISESUP_NOISCTRL,false);

	if(ret){
		
		return ret;
	}
	
	ret = mxt_write_T72_DualX(data,MXT_NOISESUP_VNOICTRL,false);
	
	if(ret){
		
		return ret;
	}
	
	printk("mxt: read_raw_data,disabled T72 dual-x settings.\n");

	ret = mxt_read_object(data,MXT_SPT_CTECONFIG_T46,MXT_SPT_CTECONFIG_T46_CTRL,&t46_ctrl);

	if (ret) {
		printk("mxt: read_raw_data,Failed to read object %d\n", (int)MXT_SPT_CTECONFIG_T46);
		return ret;
	}

	printk("mxt: read_raw_data, T46 default setting byte[0] = %d.\n",t46_ctrl);
	ret = mxt_write_object(data,MXT_SPT_CTECONFIG_T46,MXT_SPT_CTECONFIG_T46_CTRL,0x08);

	if(ret){

		printk("mxt: read_raw_data, failed to write T46 disabling the mutual P2P.\n");
		return ret;
	}

	printk("mxt: read_raw_data, disabled T46 mutual P2P!\n");

	ret = mxt_t6_command(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE, false);

	if(ret){
		return ret;
	}

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);

	if(ret){
		return ret;
	}


	printk("mxt: read_raw_data, chip BACKUP and RESET completed! start to read raw data!\n");

	msleep(300);
	


	/* Something has gone wrong if T37_buf is already allocated */
	if (data->T37_buf)
		return -EINVAL;

	T37_buf_size = data->info->matrix_xsize * data->info->matrix_ysize * sizeof(__le16);
	data->T37_buf_size = T37_buf_size;
	data->T37_buf = kmalloc(data->T37_buf_size, GFP_KERNEL);
	if (!data->T37_buf)
		return -ENOMEM;
	printk("ttt1 t37_buf addr:%p ,t37_buff_size:%d\n",(void *)data->T37_buf,data->T37_buf_size);

	/* Temporary buffer used to fetch one T37 page */
	obuf = kmalloc(mxt_obj_size(T37), GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

  
	num_pages = DIV_ROUND_UP(T37_buf_size, mxt_obj_size(T37) - 2);
	pos = 0;
	printk("ttt2 t37_buf addr:%p ,t37_buff_size:%d \n",(void *)data->T37_buf,data->T37_buf_size);
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
		printk("ttt3 t37_buf addr:%p ,chunk_len:%d \n",(void *)data->T37_buf,chunk_len);
		memcpy(&data->T37_buf[pos], &obuf[2], chunk_len);
		printk("ttt4 t37_buf addr:%p ,chunk_len:%d \n",(void *)data->T37_buf,chunk_len);
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
		printk("ttt5 t37_buf addr:%p , i:%d ,RAW_DATA_SIZE=30*15\n",(void *)data->T37_buf,i); 
    }


err_free_T37_buf:
	kfree(data->T37_buf);
	data->T37_buf = NULL;
	data->T37_buf_size = 0;
	printk("ttt6 t37_buf addr:%p \n",(void *)data->T37_buf); 
	kfree(obuf);
	
	printk("ttt7 t37_buf addr:%p \n",(void *)data->T37_buf); 

	ret = mxt_write_object(data, MXT_OBJ_NOISE_T72, MXT_NOISESUP_STABCTRL, stabDualx);
			 
	if(ret)
	{
		return ret;
	}
	printk("ttt8 t37_buf addr:%p \n",(void *)data->T37_buf); 
	ret = mxt_write_object(data, MXT_OBJ_NOISE_T72, MXT_NOISESUP_NOISCTRL, noisDualX);
			 
	if(ret)
	{
		return ret;
	}
	printk("ttt9 t37_buf addr:%p \n",(void *)data->T37_buf); 

	ret = mxt_write_object(data, MXT_OBJ_NOISE_T72, MXT_NOISESUP_VNOICTRL, vnoiDualX);

	if(ret){

		return ret;
	}

	printk("mxt: read_raw_data, restore the T72 dualX setting, Stable = %d,Noisy = %d,VNoisy = %d!\n",stabDualx,noisDualX,vnoiDualX);

	ret = mxt_write_object(data,MXT_SPT_CTECONFIG_T46,MXT_SPT_CTECONFIG_T46_CTRL,t46_ctrl);

	if(ret){

		return ret;
	}

	printk("mxt: read_raw_data, restore the T46 default setting, control = %d!\n",t46_ctrl);
	
	ret = mxt_t6_command(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE, false);

	if(ret)
	{
		return ret;
	}

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	
	if(ret){
		
		return ret;
	}

	msleep(300);
	printk("ttt10 t37_buf addr:%p ,ret :%d \n",(void *)data->T37_buf,ret); 
	printk("mxt: read_raw_data, chip BACKUP and RESET completed! Exit!\n");
	
	return ret ?: 0;
}

static ssize_t mxt_open_circuit_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ii;
	unsigned int jj;

	unsigned int sum = 0;
	short max = 0;
	short min = 0;
	short range = 0;
	short range_threshold = RANGE_THRESHOLD;

	short *report_data_16, limit_b, limit_l;


	printk("mxt: mxt_open_circuit_test_show, enter!\n");

		/* esd check */
#ifdef MXT_ESDCHECK_ENABLE
	mxt_esdcheck_switch(ESD_DISABLE);
	if(data->T61_reportid_max){
		mxt_triger_T61_Instance5(data,2);
	}
#endif


	read_raw_data(data, MXT_T6_CMD_REFS);
	report_data_16 = data->raw_data_16;
	limit_b = data->raw_data_avg + 3500;
	limit_l = data->raw_data_avg - 3500;

	for (ii = 0; ii < TX_NUM; ii++) {
		for (jj = 0; jj < RX_NUM; jj++) {

			sum += *report_data_16;
			if (max < *report_data_16)
				max = *report_data_16;

			if (ii == 0 && jj == 0)
				min = *report_data_16;
			else if (*report_data_16 < min)
				min = *report_data_16;

			
			if (*report_data_16 > THRESHOLD_MAX ||
				*report_data_16 < THRESHOLD_MIN) {
				return snprintf(buf, PAGE_SIZE,
					"Failed! TP Open Circuit Detected,\nTx:%d,Rx:%d,raw:%d, threshold:%d\n",
					ii, jj,
					*report_data_16, data->raw_data_avg);
			}
			report_data_16++;
		}
	}


	range = max-min;
	if(range > range_threshold)
	{
		return snprintf(buf, PAGE_SIZE,
					"Failed! TP Range Beyond Threshold,\nRange:%d, threshold:%d\n",
					range, range_threshold);
	}

	data->raw_data_avg = sum/RAW_DATA_SIZE;

		/* esd check */
#ifdef MXT_ESDCHECK_ENABLE
	if(data->T61_reportid_max){
		mxt_triger_T61_Instance5(data,1);
	}
	mxt_esdcheck_switch(ESD_ENABLE);
#endif
	
	return snprintf(buf, PAGE_SIZE, "Pass!\nmax = %d, min = %d, average = %d\n",max, min, data->raw_data_avg);
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

static ssize_t mxt_wakeup_gesture_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->mxt_wakeup_gesture_enable);
}




static ssize_t mxt_suspend_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			data->suspended);
}

static ssize_t mxt_wakeup_gesture_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if(data->suspended){
		
		printk("mxt: wakeup gesture store, is in suspend mode, exit.\n" );
		return 0;
	}
	data->mxt_wakeup_gesture_enable= input;
	printk("mxt: wakeup gesture store, input = %d", data->mxt_wakeup_gesture_enable);

	if(data->mxt_wakeup_gesture_enable == true)
		enable_irq_wake(mxt_touch_irq);
	else
		disable_irq_wake(mxt_touch_irq);

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


#ifdef MXT_ESDCHECK_ENABLE

static ssize_t mxt_esdcheck_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;

    
    count = sprintf(buf, "MXT Esd check: %s\n", mxt_esdcheck_get_status() ? "On" : "Off");


    return count;
}


static ssize_t mxt_esdcheck_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	unsigned int input;
   	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;
	
    if (input)
    {
        mxt_esdcheck_switch(ESD_ENABLE);
    }
    else
    {
        mxt_esdcheck_switch(ESD_DISABLE);
    }
    
    return count;
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
                    
                    data->mxt_wakeup_gesture_enable = true;
#endif
                    break;
        default:
                    break;
    }
    return count;
}


/*selftest node show funciton - wrote by david*/

static ssize_t mxt_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%02X, %02X, %02X, %02X, %02X, %02X\n",
			data->test_result[0], data->test_result[1],
			data->test_result[2], data->test_result[3],
			data->test_result[4], data->test_result[5]);
}

/*selftest node store funciton - wrote by david*/
static ssize_t mxt_selftest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error = 0;
	u8 selftest_cmd;
	u8 stabDualx=0,noisDualX=0,vnoiDualX=0;

	error = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			MXT_NOISESUP_STABCTRL, &stabDualx);

	if (error) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return error;
	}

	error = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			MXT_NOISESUP_NOISCTRL, &noisDualX);

	if (error) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return error;
	}

	error = mxt_read_object(data, MXT_OBJ_NOISE_T72,
			MXT_NOISESUP_VNOICTRL, &vnoiDualX);

	if (error) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)MXT_OBJ_NOISE_T72);
		return error;
	}
    
	printk("mxt: selftest,read dual-x settings,Stable = %d,Noisy = %d,VNoisy = %d!\n",stabDualx,noisDualX,vnoiDualX);

	error = mxt_write_T72_DualX(data,MXT_NOISESUP_STABCTRL,false);

	if(error){
		
		return error;
	}
	
	error = mxt_write_T72_DualX(data,MXT_NOISESUP_NOISCTRL,false);

	if(error){
		
		return error;
	}
	
	error = mxt_write_T72_DualX(data,MXT_NOISESUP_VNOICTRL,false);
	
	if(error){
		
		return error;
	}	


	msleep(100);
	/* run all selftest */
	error = mxt_write_object(data,
			MXT_SPT_SELFTEST_T25,
			0x01, 0x12);
	if (!error) {
		while (true) {
			msleep(10);
			error = mxt_read_object(data,
					MXT_SPT_SELFTEST_T25,
					0x01, &selftest_cmd);
			if (error || selftest_cmd == 0)
				break;
		}
	}


	error = mxt_write_object(data, MXT_OBJ_NOISE_T72, MXT_NOISESUP_STABCTRL, stabDualx);
				 
	if(error)
	{
		return error;
	}
	error = mxt_write_object(data, MXT_OBJ_NOISE_T72, MXT_NOISESUP_NOISCTRL, noisDualX);
				 
	if(error)
	{
		return error;
	}
	
	error = mxt_write_object(data, MXT_OBJ_NOISE_T72, MXT_NOISESUP_VNOICTRL, vnoiDualX);
	

	return error ? : count;
}
#if 0
/*clear bit and set bit of specific register - wrote by david*/
static int mxt_set_clr_reg(struct mxt_data *data,
				u8 type, u8 offset, u8 mask_s, u8 mask_c)
{
	int error = 0;
	u8 val= 0;

	error = mxt_read_object(data, type, offset, &val);
	if (error) {
		dev_err(&data->client->dev,
			"Failed to read object %d\n", (int)type);
		return error;
	}

	val &= ~mask_c;
	val |= mask_s;

	error = mxt_write_object(data, type, offset, val);
	if (error)
		dev_err(&data->client->dev,
			"Failed to write object %d\n", (int)type);
	return error;
}
#endif



/*selftest function - wrote by david */

static DEVICE_ATTR(selftest,  S_IWUSR | S_IRUSR, mxt_selftest_show, mxt_selftest_store);
static DEVICE_ATTR(touch_debug, 0644, NULL, touch_debug_store);
static DEVICE_ATTR(fw_version, 0644, mxt_fw_version_show, NULL);
static DEVICE_ATTR(fw_version_control,0644,mxt_fw_version_control_show,NULL);
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

static DEVICE_ATTR(detected_gesture, 0644, mxt_detected_gesture_show, NULL);

static DEVICE_ATTR(wakeup_gesture_enable, 0644,
	mxt_wakeup_gesture_enable_show, mxt_wakeup_gesture_enable_store);

static DEVICE_ATTR(system_suspend_status, 0644, mxt_suspend_info_show, NULL);

#endif


#ifdef MXT_ESDCHECK_ENABLE
static DEVICE_ATTR(mxt_esd_check, 0644, mxt_esdcheck_show, mxt_esdcheck_store);

#endif

static struct attribute *mxt_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_fw_version_control.attr,
	&dev_attr_touch_debug.attr,
	&dev_attr_cfg_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_selftest.attr, 
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
	
	&dev_attr_detected_gesture.attr,
	&dev_attr_wakeup_gesture_enable.attr,
	&dev_attr_system_suspend_status.attr,
#endif


#ifdef MXT_ESDCHECK_ENABLE

	&dev_attr_mxt_esd_check.attr,

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

	for (id = 0; id < num_mt_slots; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	mxt_input_sync(data);
}

static void mxt_start(struct mxt_data *data)
{
    
	printk("mxt: mxt_start, enter! data suspend : %d,in bootloader : %d,\n",data->suspended,data->in_bootloader);
    if (!data->suspended || data->in_bootloader)
        return;


#ifdef MAX1_WAKEUP_GESTURE_ENABLE
    printk("mxt: mxt_start, gesture_enable = %d", data->mxt_wakeup_gesture_enable);
    if (data->mxt_wakeup_gesture_enable) {

		printk("mxt: mxt_start, set T7 config on gesture thread.\n");
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
        data->suspended = false;
		
        return;
    }
#endif
	

    if (data->use_regulator) {
        printk("mxt: mxt_start, restore regulator.\n");
        mxt_regulator_restore(data);
    } else {
        
        /*
         * Discard any messages still in message buffer
         * from before chip went to sleep
         */
        mxt_process_messages_until_invalid(data);
		printk("mxt: start, set T7 config without gesture.\n");
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);

        /* Recalibrate since chip has been in deep sleep */
        mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);

        
    } 
    data->suspended = false;

	printk("mxt: mxt_start, exit! the data suspend : %d,\n",data->suspended);
	return;
}

static void mxt_stop(struct mxt_data *data)
{
    u8 error;


	printk("mxt: mxt_stop, enter! data suspend : %d,in bootloader : %d,updateing config : %d\n",data->suspended,data->in_bootloader,data->updating_config);
    if (data->suspended || data->in_bootloader)
        return;


#ifdef MAX1_WAKEUP_GESTURE_ENABLE
    printk("mxt: mxt_stop, gesture_enable = %d", data->mxt_wakeup_gesture_enable);
    if (data->mxt_wakeup_gesture_enable) {

		error = mxt_init_t7_power_cfg(data);

		if(error){
			printk("mxt:mxt_stop, init T7 power config failed!\n");
		}
		
		printk("mxt: mxt_stop, T7 config has been read/stored, set T7 config on gesture thread.\n");
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_WAKEUP);
      
        data->suspended = true;
        return;
    }
#endif
	
   

    if (data->use_regulator) {
        printk("mxt: mxt_stop, disable regulator.\n");
        mxt_regulator_disable(data);
    } else {
	error = mxt_init_t7_power_cfg(data);

		if(error){
			printk("mxt:mxt_stop, init T7 power config failed when no gesture support!\n");
		}
		
        printk("mxt: stop, T7 config has been read/stored, set T7 config without gesture.\n");
        mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
    }

    mxt_reset_slots(data);
    data->suspended = true;

	printk("mxt: mxt_stop, exit! the data suspend : %d.",data->suspended);
	return;
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

	dev_err(device, "mxt: %s\n", __func__);
	mxt_stop(data);
}

#ifdef CONFIG_OF
static struct mxt_platform_data *mxt_parse_dt(struct i2c_client *client)
{
	struct mxt_platform_data *pdata;
	struct device *dev = &client->dev;

    printk("mxt: parse_dt, start!\n");

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		{
		printk("mxt: mxt_parse_dt, pdata is null!\n");
		return NULL;
		}
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
	pdata->gpio_irq = 0;
	
	printk("mxt : mxt_parse_dt, gpio_irq num :%ld\n",pdata->gpio_irq);

	of_property_read_string(dev->of_node, "atmel,cfg_name",
				&pdata->cfg_name);
	
	printk("mxt: mxt_parse_dt name=%s\n",pdata->cfg_name);

	of_property_read_string(dev->of_node, "atmel,input_name",
				&pdata->input_name);

	of_property_read_string(dev->of_node, "atmel,fw_version",
				&pdata->fw_version);

	return pdata;
}
#endif

static inline void board_gpio_init(const struct mxt_platform_data *pdata)
{

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

	printk("mxt: tpd_irq_registration, enter!\n");
	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		mxt_touch_irq = irq_of_parse_and_map(node, 0);
		ret =  request_irq(mxt_touch_irq, mxt_interrupt, IRQF_TRIGGER_FALLING, "TOUCH_PANEL-eint", NULL);
		if (ret > 0) {
			ret = -1;
			printk("mxt: tpd_irq_registration, tpd request_irq IRQ LINE NOT AVAILABLE!.");
		}
		mxt_acquire_irq(mxt_i2c_data);

	}else{
		ret = -1;
		printk("mxt: tpd_irq_registration, tpd request_irq can not find touch eint device node!.");
	}
	printk("mxt: tpd_irq_registration, exit!, [%s]irq:%d, debounce:%d-%d\n", __func__, mxt_touch_irq, ints[0], ints[1]);	
	return ret;
}
void tpd_gpio_output_ATEML(int pin, int level)
{
	printk("mxt: tpd_gpio_output_ATEML, [tpd]tpd_gpio_output pin = %d, level = %d\n", pin, level);
	if (pin == 2) {
		if (level)
			pinctrl_select_state(pinctrl2, power_output1);
		else
			pinctrl_select_state(pinctrl2, power_output0);
	} 
	else
		printk("mxt: tpd_gpio_output_ATEML, NO pins define\n");
}

int tpd_gpio_enable_regulator_output(int flag2)
{
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
		printk("mxt: mxt_tpd_enable_regulator_output_0!\n");
		/*set TP volt*/
		tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
		ret = regulator_set_voltage(tpd->reg, 3000000, 3000000);
		if (ret != 0) {
			printk("mxt: tpd_enable_regulator_output, [POWER]Failed to set voltage of regulator,ret=%d!", ret);
			return ret;
		}
		ret = regulator_enable(tpd->reg);
		if (ret != 0) {
			printk("mxt: tpd_enable_regulator_output, [POWER]Fail to enable regulator when init,ret=%d!", ret);
			return ret;
		}
		}
	else
	   {
		ret = regulator_disable(tpd->reg);
		if (ret != 0) {
			printk("mxt: tpd_enable_regulator_output, [POWER]Fail to disable regulator when init,ret=%d!", ret);
			return ret;
		}
		}
		
		return 0;

}

static void tpd_power_on(int flag)
{
	if(flag)
	{
	 printk("mxt: mxt_tpd_power_on_0!\n");
     #ifdef MTK_POWER
	 printk("mxt: mxt_tpd_power_on_1!\n");
	 tpd_enable_regulator_output(1);
	 #else
		tpd_gpio_enable_regulator_output(1);
	 #endif
	}
	else
	{
	printk("mxt: mxt_tpd_power_on_01!\n");
	#ifdef MTK_POWER
	printk("mxt: mxt_tpd_power_on_02!\n");
	tpd_enable_regulator_output(0);
	#else
		tpd_gpio_enable_regulator_output(0);
	#endif
	}
}
int tpd_get_gpio_info_atmel(struct i2c_client *pdev)
{
	int ret;

	printk("mxt:tpd_get_gpio_info_atmel, enter!\n");
	pinctrl2 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl2)) {
		ret = PTR_ERR(pinctrl2);
		printk("mxt:tpd_get_gpio_info_atmel, mxt Cannot find touch pinctrl1!\n");
		return ret;
	}
	printk("mxt:tpd_get_gpio_info_atmel_1 begin\n");
		power_output0 = pinctrl_lookup_state(pinctrl2, "state_power_output0");
		if (IS_ERR(power_output0)) {
			ret = PTR_ERR(power_output0);
			printk( "mxt:tpd_get_gpio_info_atmel,Cannot find touch pinctrl state_power_output0!\n");
			return ret;
		}
		else
			{
			printk("mmxt:tpd_get_gpio_info_atmel,success\n");
			}
		power_output1 = pinctrl_lookup_state(pinctrl2, "state_power_output1");
		if (IS_ERR(power_output1)) {
			ret = PTR_ERR(power_output1);
			printk("mxt:tpd_get_gpio_info_atmel, Cannot find touch pinctrl state_power_output1!\n");
			return ret;
		}
	printk("mxt:tpd_get_gpio_info_atmel, mt_tpd_pinctr2----------\n");
	return 0;
}


/* - added by david */
#if 0
static struct device_node *mxt_find_cfg_node(struct mxt_data *data, struct device_node *np)
{
	struct device *dev = &data->client->dev;
	struct mxt_config_info *info = &data->pdata->info;
	struct device_node *cfg_np = NULL;
	u32 temp_val, rc;
	enum hw_pattern_type type;

	cfg_np = of_get_next_child(np, NULL);
	if (cfg_np == NULL) {
		dev_err(dev, "%s, have no cfg node\n", __func__);
		return NULL;
	}

	do {
		rc = of_property_read_u32(cfg_np, "atmel,type", &temp_val);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev,"TP: %s, Unable to read atmel,type\n", __func__);
			return NULL;
		}

		type = temp_val ? new_pattern: old_pattern;
		if (type == data->pattern_type) {
			dev_info(dev, "type: 0x%x, data->pattern_type: 0x%x\n", type, data->pattern_type);
			info->type = (u8) type;
			return cfg_np;
		}

		cfg_np = of_get_next_child(np , cfg_np);
	} while (cfg_np!= NULL);

	dev_err(dev, "No found cfg node, temp_val: 0x%x, data->pattern_type: 0x%x\n", type, data->pattern_type);
	return NULL;
}
#endif
/* - wrote by david */
static int mxt_parse_cfg_and_load(struct mxt_data *data,
	struct mxt_config_info *info, bool force)
{
	int rc = 0;
	char * cfg_name;

	struct device *dev = &data->client->dev;
	struct mxt_cfg_info *config_info = &data->config_info;

	u8 family,variant,version,build,pos;
	u8 i,t37,t37_size,cfg_id;
	u8 readData[13];
	u32 cfgCrc;

	cfg_name = data->cfg_name;
	

	printk("mxt: parse cfg and load, start to parse cfg !");
	pos = 0;
	for(i=0;i<sizeof(readData);i++){
		readData[i] = *(u8*)(mxt_cfg_data+pos);
		pos++;
	}
		
	family = readData[MXT_FAMILY_OFFSET_CFG];
	variant = readData[MXT_VARIANT_OFFSET_CFG];
	version = readData[MXT_VERSION_OFFSET_CFG];
	build = readData[MXT_BUILD_OFFSET_CFG];

	cfgCrc = (readData[MXT_CRC_HI_CFG])|(readData[MXT_CRC_MID_CFG]<<8)|(readData[MXT_CRC_LOW_CFG]<<16);


	if(family != data->info->family_id){

		printk("mxt: parse cfg and load, the chip type doesn't match, will not update!\n");
		return 0;

	}

	if(variant != data->info->variant_id){

		printk("mxt: parse cfg and load, the chip variant doesn't match, will not update!\n");
		return 0;

	}

	if (version != data->info->version || build != data->info->build) {
		printk("mxt: parse cfg and load, config corresponding fw version doesn't match, will not update!\n");
		return 0;
	}

	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);

	if(cfgCrc == data->config_crc){

		printk("mxt: parse cfg and load, config crc is same CRC = %d, will not update!\n",cfgCrc);
		return 0;

	}


	if (force){
		
		printk("mxt: parse cfg and load, force to update cfg anyway, ignore to read T38!");
		goto directly_update;
	}
		

	
	t37 = mxt_cfg_data[MXT_CFG_ARRAY_INFO_LENGTH + 2];
	
	if(t37 == 0x25){
	
		t37_size = mxt_cfg_data[MXT_CFG_ARRAY_INFO_LENGTH + 6];
	
		cfg_id = mxt_cfg_data[MXT_CFG_ARRAY_INFO_LENGTH + MXT_CFG_ARRAY_PREFIX_LENGTH * 3 + t37_size + 0x49 + 7];
		printk("mxt: parse cfg and load,T37 exists, t37_size = %d,cfg_id = %d!\n",t37_size,cfg_id);
	}
	else if(t37==0x44)
	{
		cfg_id = mxt_cfg_data[MXT_CFG_ARRAY_INFO_LENGTH + MXT_CFG_ARRAY_PREFIX_LENGTH * 2 + 0x49 + 7];
		printk("mxt: parse cfg and load,T37 does not exist, cfg_id = %d!\n",cfg_id);
	}
	else{
	
		printk("mxt: parse cfg and load, the config array format isn't correct,the first object is t%d, will not update!\n",t37);
		return 0;
	
	}

	rc = mxt_read_t38_object(data);

	if (rc) 
	{
		printk("mxt: parse cfg and load, cannot read the T38 info!\n");
		return rc;
	}
	
	printk("mxt: parse cfg and load, compare cfg_id ,cfg_id = %d,config_info->fw_version_control = %d!\n",cfg_id,config_info->fw_version_control);
	if((cfg_id < config_info->fw_version_control)|| (cfg_id == config_info->fw_version_control))
	{
		printk("mxt: parse cfg and load, file config version = %d is early than chip config version = %d, will not update!\n",cfg_id,config_info->fw_version_control);
		return 0;

	}
	
	printk("mxt: parse cfg and load, start to update config of family=%d, variant=%d, version=%d, build=%d, fw_control=%d",family,variant,version,build,cfg_id);
	
	
	
directly_update:
	rc = mxt_download_config(data,cfg_name);

	if(rc){
		dev_info(dev, "failed to update config");
		printk("mxt: parse cfg and load, failed to update config!\n");
	}

	return rc;
}

#if 1
 static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
 {
	struct mxt_data *data = dev_get_drvdata(&mxt_i2c_data->client->dev);
	int ii;
	unsigned int jj;

	unsigned int sum = 0;
	short max = 0;
	short min = 0;
	short range = 0;
	short range_threshold = RANGE_THRESHOLD;
	short *report_data_16, limit_b, limit_l;
	int result = 0;
	int len = count;
	int ret = -1; 
	if(*ppos){
		printk("solomon tp test again return\n");
		return 0;
	}
	*ppos += count;

	if (count > 9)
		 len = 9;

	printk("ttt ctp_open_proc_read begin\n");
	ret = read_raw_data(data, MXT_T6_CMD_REFS);
	if (ret) {
		printk("ttt read_raw_data fail\n");
		result = -1;
		if (copy_to_user(buf, "result=0", len)) {
			printk("copy_to_user fail\n");
			return -1;
		 }
		 return len; 
	}
	printk("ttt read_raw_data over\n");
	report_data_16 = data->raw_data_16;
	limit_b = data->raw_data_avg + 3500;
	limit_l = data->raw_data_avg - 3500;
	printk("ttt raw_data_avg:%d \n",data->raw_data_avg);
	for (ii = 0; ii < TX_NUM; ii++) {
		for (jj = 0; jj < RX_NUM; jj++) {

			sum += *report_data_16;
			if (max < *report_data_16)
				max = *report_data_16;

			if (ii == 0 && jj == 0)
				min = *report_data_16;
			else if (*report_data_16 < min)
				min = *report_data_16;

			
			if (*report_data_16 > THRESHOLD_MAX ||
				*report_data_16 < THRESHOLD_MIN) {
				result = -1;
				printk("ttt ii:%d ,jj:%d ,limit_b:%d ,limit_l:%d , report_data_16:%d \n",ii,jj,limit_b,limit_l,*report_data_16);
				
				if (copy_to_user(buf, "result=0", len)) {
					printk("copy_to_user fail\n");
					return -1;
				 }
				return len;
			}
			report_data_16++;
		}
	}


	range = max-min;
	if(range > range_threshold)
	{
		printk("ttt range failed\n");
		result = -1;
		if (copy_to_user(buf, "result=0", len)) {
			printk("copy_to_user fail\n");
			return -1;
		 }
		return len;
	}

	data->raw_data_avg = sum/RAW_DATA_SIZE;
	printk(" ttt data->raw_data_avg :%d \n",data->raw_data_avg);
	result = 1;

	printk("ttt solomon result = %d\n",result);

	if (result == 1){
		if (copy_to_user(buf, "result=1", len)) {
		   printk("copy_to_user fail\n");
		   return -1;
		   }
	   	}else{
	if (copy_to_user(buf, "result=0", len)) {
		   printk("copy_to_user fail\n");
		   return -1;
		   }
	   }
		
	return len;
}

 static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
 {
	 return -1;
	 }
#if 1
  static ssize_t ctp_selftest_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
  {
	  printk("mxt ctp_selftest_proc_read selftest_result = %d\n",selftest_result); 

	  if(*ppos){
		 printk("mxt tp test again return\n");
		 return 0;
		 }
	  *ppos += count;
	 
	  if (selftest_result == 2){
		  if (copy_to_user(buf, "2\n", 2)) {
			 printk("copy_to_user fail\n");
			 return -1;
			 }
		 }else if (selftest_result == 1){
		 if (copy_to_user(buf, "1\n", 2)) {
			 printk("copy_to_user fail\n");
			 return -1;
			 }
		 }else if (selftest_result == 0){
		 if (copy_to_user(buf, "0\n", 2)) {
			 printk("copy_to_user fail\n");
			 return -1;
			 }
		 }else{
		 if (copy_to_user(buf, "-1\n", 2)) {
			 printk("copy_to_user fail\n");
			 return -1;
			 }
		 }
		 
	 selftest_result = 0;
	 return 2;
 }
  static ssize_t ctp_selftest_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
  {
	 struct mxt_data *data = dev_get_drvdata(&mxt_i2c_data->client->dev);
	 int ii;
	 int ret = -1;
	 unsigned int jj;
	 unsigned int sum = 0;
	 short max = 0;
	 short min = 0;
	 short range = 0;
	 short range_threshold = RANGE_THRESHOLD;
	 short *report_data_16, limit_b, limit_l;
	 char test[10] = {"\0"};
	
	 printk("user echo %s to tp_selftest",userbuf);
	 if(copy_from_user(test,userbuf,3)){
			 printk("ctp_selftest_proc_write copy_from_user fail\n");
			 return -1;
		 }else{
			 *ppos += count;
		 }
		 
	 if(!strncmp("i2c",test,3)){
		 printk("mxt ctp_selftest_proc_write test begin\n");
         ret = read_raw_data(data, MXT_T6_CMD_REFS);
		 if(ret){
		 		selftest_result = 1;
				printk("ttt read_raw_data fail\n");
				return count;
			}
		 report_data_16 = data->raw_data_16;
		 limit_b = data->raw_data_avg + 3500;
		 limit_l = data->raw_data_avg - 3500;
		 for (ii = 0; ii < TX_NUM; ii++) {
			 for (jj = 0; jj < RX_NUM; jj++) {
				 sum += *report_data_16;
				 if (max < *report_data_16)
					 	max = *report_data_16;
				 	if (ii == 0 && jj == 0)
						 min = *report_data_16;
				 		else if (*report_data_16 < min)
					 		min = *report_data_16;
				 				if (*report_data_16 > THRESHOLD_MAX ||
									 *report_data_16 < THRESHOLD_MIN) {
					 					printk("ttt ii:%d ,jj:%d ,limit_b:%d ,limit_l:%d , report_data_16:%d \n",ii,jj,limit_b,limit_l,*report_data_16);
										selftest_result = 1;
										return count;
				 							}
				 	report_data_16++;
			 }
		 }
		 range = max-min;
		 if(range > range_threshold) {
			 printk("ttt range failed\n");
			 selftest_result = 1;
			 return count;
			 }
		 data->raw_data_avg = sum/RAW_DATA_SIZE;
		 printk(" ttt data->raw_data_avg :%d \n",data->raw_data_avg);
		 selftest_result = 2;
	 }else{
		 printk("mxt echo invaild cmd not do tp open short test\n");
		 selftest_result = 0;
	 }
		 printk("ctp_selftest_proc_write selftest_result = %d\n",selftest_result); 
		 return count;
  }
 
#endif

static void create_ctp_proc(void)
{
    //----------------------------------------
    //create read/write interface for tp information
    //the path is :proc/touchscreen
    //child node is :version
    //----------------------------------------
  struct proc_dir_entry *ctp_open_proc = NULL;
  if( ctp_device_proc == NULL)
   {
	    ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
	    if(ctp_device_proc == NULL)
	    {
	        printk("mxt:create parent_proc fail\n");
	        return;
	    }
	}
    ctp_open_proc = proc_create(CTP_OPEN_PROC_NAME, 0777, ctp_device_proc, &ctp_open_procs_fops);
    if (ctp_open_proc == NULL)
    {
        printk("mxt:create open_proc fail\n");
    }

	ctp_selftest_proc = proc_create(CTP_SELF_TEST, 0777, NULL,&ctp_selftest_procs_fops);
    if (ctp_selftest_proc == NULL){
        printk("mxt create ctp_self fail\n");
    }
	
}

static void mxt_open_short_init(void){

	create_ctp_proc();
}
static int mxt_lockdown_init(void)
{
	struct proc_dir_entry *mxt_lockdown_status_proc = NULL;
	

	if( 0x32 == mxt_i2c_data->lockdown_info[2] ){
	sprintf(tp_lockdown_info, "%02x%02x%02x%02x%02x%02x%02x%02x_black", \
			mxt_i2c_data->lockdown_info[0],mxt_i2c_data->lockdown_info[1],mxt_i2c_data->lockdown_info[2], \
			mxt_i2c_data->lockdown_info[3],mxt_i2c_data->lockdown_info[4],mxt_i2c_data->lockdown_info[5], \
			mxt_i2c_data->lockdown_info[6],mxt_i2c_data->lockdown_info[7]);
		}else if( 0x31== mxt_i2c_data->lockdown_info[2] ){		
		sprintf(tp_lockdown_info, "%02x%02x%02x%02x%02x%02x%02x%02x_white", \
				mxt_i2c_data->lockdown_info[0],mxt_i2c_data->lockdown_info[1],mxt_i2c_data->lockdown_info[2], \
				mxt_i2c_data->lockdown_info[3],mxt_i2c_data->lockdown_info[4],mxt_i2c_data->lockdown_info[5], \
				mxt_i2c_data->lockdown_info[6],mxt_i2c_data->lockdown_info[7]);
		}else{
			sprintf(tp_lockdown_info, "%02x%02x%02x%02x%02x%02x%02x%02x", \
							mxt_i2c_data->lockdown_info[0],mxt_i2c_data->lockdown_info[1],mxt_i2c_data->lockdown_info[2], \
							mxt_i2c_data->lockdown_info[3],mxt_i2c_data->lockdown_info[4],mxt_i2c_data->lockdown_info[5], \
							mxt_i2c_data->lockdown_info[6],mxt_i2c_data->lockdown_info[7]);}
			
		
	printk("ttt1 ,mxt tp_lockdown_info=%s\n", tp_lockdown_info);


	if( ctp_device_proc == NULL)
		{
			ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
			if(ctp_device_proc == NULL)
			{
				printk("mxt:create parent_proc fail\n");
				return 1;
			}
		}
		mxt_lockdown_status_proc = proc_create(FTS_PROC_LOCKDOWN_FILE, 0644, ctp_device_proc, &mxt_lockdown_proc_fops);
		if (mxt_lockdown_status_proc == NULL)
		{
			printk("mxt:, create_proc_entry ctp_lockdown_status_proc failed\n");
			return 1;
		}
		return 0 ;
}
#endif

static int fw_upgrade_handler(void *pdata)
{
    struct mxt_data *data = pdata;
    struct i2c_client *client = data->client;
    int error;

	
#ifdef MXT_ESDCHECK_ENABLE
	int retval;
#endif
    printk("mxt: mxt_probe, sub thread, step 4 - mxt update fw and config, start!\n");
    error = mxt_update_fw_and_cfg(data);
    if (error)
    {
		printk("mxt: mxt_probe, sub thread, Failed to update fw and cfg!\n");
		
	}
        


#ifdef MXT_ESDCHECK_ENABLE

	if(data->T61_reportid_max){

		error = mxt_esdcheck_init();

		if(error){

			printk("mxt: mxt_probe, ESD init failed!\n");
		}
		else{
			retval = mxt_triger_T61_Instance5(data,1);
			if (retval){
				mxt_esdcheck_switch(ESD_DISABLE);
				printk("mxt: mxt_probe, Waining triger T61 failed, shut down the ESD protect! \n");
			
			}
			else{
				
				printk("mxt: mxt_probe, ESD protect started!");
			}
		}
	
	}	
	else{

		printk("mxt: mxt_probe, didn't find T61 timer, STOP ESD protect!\n");
	}
#endif

	
		printk("mxt: mxt_probe, sub thread, step 5 - mxt creat sys group!\n");
		error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
		if (error) {
			dev_err(&client->dev, "mxt: mxt_probe, sub thread, step 5, Failure %d creating sysfs group\n",
				error);
			printk("mxt: mxt_probe, sub thread, step 5,Failure creating sysfs group\n");
			mxt_free_object_table(data);
        	kfree(data);
        	return error;
		}
	
		sysfs_bin_attr_init(&data->mem_access_attr);
		data->mem_access_attr.attr.name = "mem_access";
		data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR;
		data->mem_access_attr.read = mxt_mem_access_read;
		data->mem_access_attr.write = mxt_mem_access_write;
		data->mem_access_attr.size = data->mem_size;
	
		if (sysfs_create_bin_file(&client->dev.kobj,
					  &data->mem_access_attr) < 0) {
			dev_err(&client->dev, "mxt: mxt_probe, sub thread, step 5, Failed to create %s\n",
				data->mem_access_attr.attr.name);
			sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
			sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
			mxt_free_object_table(data);
			kfree(data);
			return error;
		}


	printk("mxt: mxt_probe, sub thread, step 7 - mxt register input device!\n");
	/*register input device*/
	error = mxt_register_input_device(data);
	if (error)
		dev_err(&data->client->dev, "mxt: mxt_probe, sub thread, step 7, Failed to register input device\n");
	
	/*register interrupt*/
	printk("mxt: mxt_probe, sub thread, step 6 - mxt register interrupt!\n");
	tpd_irq_registration();			



	printk("mxt: mxt_probe, sub thread, step 8 - mxt read lockdown info!\n");
	mxt_read_lockdown_info(data);

	printk("mxt: mxt_probe, sub thread, step 9 - mxt read T7 info!\n");
	mxt_init_t7_power_cfg(data);

    mxt_vendor_id = mxt_read_hw_version(data);
    printk("mxt: mxt_probe, sub thread, the hw version is %d",mxt_vendor_id);

    mxt_firmware_version = mxt_read_fw_version_control(data);
    printk("mxt: mxt_probe, sub thread, the fw control is %d",mxt_firmware_version);
#if 1
    mxt_lockdown_init();
    mxt_open_short_init();
    hardwareinfo_tp_register(hardwareinfo_set, NULL);

#endif

	printk("mxt: mxt_probe, sub thread, probe successful!\n");

    return 0;
}

static int mxt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mxt_data *data;
	int error;
    int retval;

	
    printk("mxt: mxt_probe, main thread start!\n");
	 
	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "mxt: mxt_probe, Failed to allocate memory\n");
		return -ENOMEM;
	}
	
	mxt_i2c_data = data;
	mutex_init(&data->bus_access_mutex);
	snprintf(data->phys, sizeof(data->phys), "mxt: mxt_probe, i2c-%u-%04x/input0", client->adapter->nr, client->addr);

	data->client = client;
	data->pdata = &mxt_platform_data;
	data->updating_config = false;
	
	if(!data->pdata)
	{
		printk("mxt: mxt_probe, data->pdata is NULL\n");
	}
	
	i2c_set_clientdata(client, data);

#ifdef CONFIG_OF
    printk("mxt: mxt_probe, main thread, step 1 - define CONFIG_OF\n");
	if (client->dev.of_node){
		printk("mxt: mxt_probe, main thread, step 1, data->pdata is NULL and dev.of_node is valid\n");
		data->pdata = mxt_parse_dt(client);

#ifdef TPD_HAVE_BUTTON

		data->pdata->num_keys = mxts_num_keys;
		data->pdata->keymap = mxts_keys;
#endif

	}
#endif
	

	if (!data->pdata) {
		printk("mxt: mxt_probe, main thread, step 1, data->pdata is NULL, going to reallocate\n");
		data->pdata = devm_kzalloc(&client->dev, sizeof(*data->pdata),
					   GFP_KERNEL);
		if (!data->pdata) {
			dev_err(&client->dev, "mxt: mxt_probe, main thread, step 1, Failed to allocate pdata\n");
			printk("mxt: mxt_probe, main thread, step 1, Failed to allocate pdata!\n");
			error = -ENOMEM;
			goto err_free_mem;
		}

	}
	printk("mxt: mxt_probe, main thread,update cfg name!\n");
	/* update the pdata config file name to data->cfg_name - added by david */

	if (data->pdata->cfg_name)
		mxt_update_file_name(&data->client->dev,
				     &data->cfg_name,
				     data->pdata->cfg_name,
				     strlen(data->pdata->cfg_name));


	init_completion(&data->bl_completion);
	init_completion(&data->reset_completion);
	init_completion(&data->crc_completion);
	mutex_init(&data->debug_msg_lock);

#ifdef MXT_ESDCHECK_ENABLE
	mutex_init(&data->esd_detect_mutex);
#endif
	
	thread = kthread_run(touch_event_handler, data, "atmel-tpd");
	if ( IS_ERR(thread) ) {
		retval = PTR_ERR(thread);
		printk(" mxt_probe, main thread, step 1, %s: failed to create kernel thread: %d\n",__func__, retval);
	}
	printk("mxt: mxt_probe, main thread,step 2 - power up!\n");
	

	tpd_get_gpio_info_atmel(client);

	/* power supply */
	tpd_power_on(1);
	msleep(10);
	
	GTP_GPIO_OUTPUT(GTP_RST_PORT,0);/*set Reset pin no pullup*/
	msleep(10);
	GTP_GPIO_OUTPUT(GTP_RST_PORT,1);/*set Reset pin pullup*/
	msleep(120);

	GTP_GPIO_OUTPUT(GTP_INT_PORT,0);/*set EINT pin no pullup*/
	GTP_GPIO_AS_INT(GTP_INT_PORT);	/*set EINT mode*/

	/*disable interrupt !!*/
	/* read the information block, initiate the mxt_data struct */

		
	printk("mxt: mxt_probe, main thread, step 3 - mxt initialize!\n");
	error = mxt_initialize(data);
	
	if (error) {
		printk("mxt: mxt_probe, main thread, Failed to initialize device!\n");
		goto err_free_object;
     
	}
	


    /* - added by david */
    /* Determine whether to update fw and config */

    printk("mxt: mxt_probe, sub probe thread start!\n");
    fw_upgrade_thread = kthread_run(fw_upgrade_handler, data, "mxt:fwupgr");
    if ( IS_ERR(fw_upgrade_thread) ) {
        retval = PTR_ERR(fw_upgrade_thread);
        printk(" mxt: mxt_probe, step 4, %s: failed to create kernel thread: %d\n", __func__, retval);

	}

	
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	data->mxt_wakeup_gesture_enable = 0;
	printk("mxt: mxt_probe, main thread, step 10, initial wakeup gesture enable = %d!\n",data->mxt_wakeup_gesture_enable);
	
#endif

	tpd_load_status = 1;
	printk("mxt: mxt_probe, main thread, probe successfully! tpd_load_status = %d.\n",tpd_load_status);
	return 0;

err_free_object:
	mxt_free_object_table(data);
err_free_mem:
	kfree(data);
	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	printk("mxt: mxt_remove, enter!\n");
	
	tpd_load_status = 0;
	if (data->mem_access_attr.attr.name)
		sysfs_remove_bin_file(&client->dev.kobj,
				      &data->mem_access_attr);

	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);

#ifdef MXT_ESDCHECK_ENABLE
		mxt_esdcheck_exit();
#endif

	
	free_irq(6, data);
	regulator_put(data->reg_avdd);
	regulator_put(data->reg_vdd);
	mxt_free_object_table(data);
	kfree(data);
	printk("mxt: mxt_remove, exit!\n");
	return 0;
}

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
void mxt_set_gesture(struct mxt_data *mxt, bool enable)
{

	printk("mxt: mxt_set_gesture, enter!\n");
	if (enable) {

		printk("mxt: mxt_set_gesture, decrease the T100 touch threshold!\n");
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,30,30);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,31,5);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,32,15);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,39,0);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,40,0);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,53,2);

		printk("mxt: set gesture, enable gesture!\n");
		/* Enable gesture, double click, event reporting */
		mxt_config_ctrl_set(mxt, mxt->T93_address, 0x01);

		/* Enable gesture, up, down, left, right, event reporting */
	} else {

		printk("mxt: mxt_set_gesture, write back the T100 threshold!\n");
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,30,50);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,31,15);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,32,30);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,39,1);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,40,1);
		mxt_write_object(mxt,MXT_TOUCH_MULTITOUCHSCREEN_T100,53,10);
		

		printk("mxt: set gesture, disable gesture!\n");
		/* Disable gesture, double click, event reporting */
		mxt_config_ctrl_clear(mxt, mxt->T93_address, 0x01);

		/* Disable gesture, up, down, left, right, event reporting */
	}
	printk("mxt: mxt_set_gesture, exit!\n");
}
#endif

#if defined(CONFIG_FB_PM)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct mxt_data *mxt =
		 container_of(work,
			struct mxt_data, fb_notify_work);

	printk("mxt: fb_notify_resume_work, enter!\n");
	
	mxt_resume(&(mxt->input_dev->dev));

	/* Enable general touch event reporting */
	mxt_config_ctrl_set(mxt, mxt->T100_address, 0x02);
#ifdef MAX1_WAKEUP_GESTURE_ENABLE
	if (mxt->mxt_wakeup_gesture_enable)
		mxt_set_gesture(mxt, false);
#endif

	printk("mxt: fb_notify_resume_work, exit!\n");
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
			printk("mxt: TP: %s(), FB_BLANK_UNBLANK\n", __func__);

			schedule_work(&mxt->fb_notify_work);

		} else if (*blank == FB_BLANK_POWERDOWN) {
			if (flush_work(&mxt->fb_notify_work))
				pr_warn("mxt: %s: waited resume worker finished\n", __func__);

			printk("mxt: TP: %s(), FB_BLANK_POWERDOWN\n", __func__);

			/* Disable general touch event reporting */
			mxt_config_ctrl_clear(mxt, mxt->T100_address, 0x02);

#ifdef MAX1_WAKEUP_GESTURE_ENABLE
			mxt->suspended = 1;
			if (mxt->mxt_wakeup_gesture_enable)
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


    printk("mxt: mxt_suspend input_dev->users: %d\n", input_dev->users);
	
	if (input_dev->users)
    {
    	printk("mxt: mxt_suspend, execute stop.\n");
		mxt_stop(data);
    }


	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;



    printk("mxt: mxt_resume, enter! input_dev->users: %d\n", input_dev->users);

	if (input_dev->users)
	{
		mxt_start(data);
	}


	printk("mxt: mxt_resume, exit!\n");
	return 0;
}

static void mxt_tpd_suspend(struct device *h)
{
	struct mxt_data *data = mxt_i2c_data;

	printk("mxt: mxt_tpd_suspend, enter! \n");

	if(data)
    {
    	
		if (data->suspended || data->in_bootloader){
			printk("mxt: mxt_tpd_suspend, in fw updating or system syspend already, do nothing! suspend : %d,in bootloader : %d,\n",data->suspended,data->in_bootloader);
			return;
		}
		
		/* Disable general touch event reporting */
		
		mxt_config_ctrl_clear(data, data->T100_address, 0x02);
		printk("mxt: tpd suspend, disabe T100 report.\n");


#ifdef MXT_ESDCHECK_ENABLE
		if(data->T61_reportid_max){
			mxt_triger_T61_Instance5(data,2);
		}
    	mxt_esdcheck_suspend();
#endif

		
#ifdef MAX1_WAKEUP_GESTURE_ENABLE

		if (data->mxt_wakeup_gesture_enable){
			mxt_set_gesture(data, true);
			printk("mxt: tpd suspend, gesture enabled!\n");
		}
			
#endif
		mxt_reset_slots(data);

		printk("mxt: tpd suspend, execute suspend!\n");
		mxt_suspend(&(data->client->dev));



		if (!data->mxt_wakeup_gesture_enable){
			tpd_power_on(0);
			printk("mxt: [mxt] close ldo gesture flag:%d \n",data->mxt_wakeup_gesture_enable);
		}
		
    }
    else
    {
        printk("mxt: mxt_tpd_suspend, mxt data is NULL, do nothing!\n");
    }

	printk("mxt: mxt_tpd_suspend, Exit. \n");
	
}

static void mxt_tpd_resume(struct device *h)
{
	struct mxt_data *data = mxt_i2c_data;
	printk("mxt: mxt_tpd_resume, enter. \n");
	
	if(data){

	if (!data->suspended || data->in_bootloader){
			printk("mxt: mxt_tpd_suspend, in fw updating or system resume already, do nothing! suspend : %d,in bootloader : %d,\n",data->suspended,data->in_bootloader);
			return;
		}


		if (!data->mxt_wakeup_gesture_enable){
			
				GTP_GPIO_OUTPUT(GTP_RST_PORT,0);/*set Reset pin no pullup*/
				msleep(2);
			
				tpd_power_on(1);
				msleep(10);
				
				GTP_GPIO_OUTPUT(GTP_RST_PORT,1);/*set Reset pin pullup*/
				msleep(250);
				printk("mxt: [mxt] open ldo gesture flag:%d \n",data->mxt_wakeup_gesture_enable);
			}


		printk("mxt: tpd resume, execute resume!\n");
		mxt_resume(&data->client->dev);
		
		/* Enable general touch event reporting */
		mxt_config_ctrl_set(data, data->T100_address, 0x02);
		printk("mxt: tpd resume, enable T100 report!\n");



#ifdef MXT_ESDCHECK_ENABLE

		if(data->T61_reportid_max){
		
			mxt_triger_T61_Instance5(data,1);

		}
			mxt_esdcheck_resume();
#endif


	#ifdef MAX1_WAKEUP_GESTURE_ENABLE
		if (data->mxt_wakeup_gesture_enable){

			printk("mxt: tpd resume, disable gesture!\n");
			mxt_set_gesture(data, false);

		}
	#endif

	}
	else{

		printk("mxt: mxt_tpd_resume, the mxt data is NULL, do nothing!\n");
	}

	printk("mxt: mxt_tpd_resume, Exit. \n");

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
	TPD_DMESG("mxt: tpd_local_init, Atmel I2C Touchscreen Driver (Built %s )\n", __FUNCTION__);

	if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
		TPD_DMESG("mxt: tpd_local_init, error! unable to add i2c driver.\n");
		return -1;
	}
#ifdef TPD_HAVE_BUTTON
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
#endif 

	TPD_DMESG("mxt: tpd_local_init, Atmel I2C Touchscreen Driver %s,tpd_load_status=%d\n", __FUNCTION__,tpd_load_status);
	if(tpd_load_status == 0) {
		printk("mxt: tpd_local_init, atmel touch panel driver failed to add!!!\n");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	printk(KERN_ERR "mxt: Atmel %s is ok!\n",__FUNCTION__);
	
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
#ifdef TPD_HAVE_BUTTON
	.num_keys = mxts_num_keys,
	.keymap = mxts_keys,
#endif
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	.max_y_t = 1919,
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

	TPD_DMESG("mxt: Mxt touch panel driver init111111:%s\n",__FUNCTION__);
	if(tpd_driver_add(&atmel_mxt_driver) < 0){
		pr_err("mxt: Fail to add tpd driver\n");
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

