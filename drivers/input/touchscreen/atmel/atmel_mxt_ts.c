/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011 Atmel Corporation
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/debugfs.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
#include "plug.h"
#endif

/* Version */
#define MXT_VER_20		20
#define MXT_VER_21		21
#define MXT_VER_22		22

/* Firmware files */
#define MXT_FW_NAME		"maxtouch.fw"
#define MXT_CFG_MAGIC		"OBP_RAW V1"

/* Registers */
#define MXT_FAMILY_ID		0x00
#define MXT_VARIANT_ID		0x01
#define MXT_VERSION		0x02
#define MXT_BUILD		0x03
#define MXT_MATRIX_X_SIZE	0x04
#define MXT_MATRIX_Y_SIZE	0x05
#define MXT_OBJECT_NUM		0x06
#define MXT_OBJECT_START	0x07

#define MXT_OBJECT_SIZE		6
#define MXT_INFO_CHECKSUM_SIZE	3

#define MXT_MAX_BLOCK_WRITE	256

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_SPT_USERDATA_T38		38
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
#define MXT_PROCI_ACTIVE_STYLUS_T63	63
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_SPT_NOISESUPPRESSION_T48    48
#define MXT_SPT_ADAPTIVE_THRESHOLD_T55	55
#define MXT_SPT_TIMER_T61		61
#define MXT_PROCI_LENSBENDING_T65	65
#define MXT_SPT_GOLDENREF_T66		66
#define MXT_SPT_DYMCFG_T70		70
#define MXT_SPT_DYMDATA_T71		71
#define MXT_PROCG_NOISESUPPRESSION_T72	72
#define MXT_PROCI_GLOVEDETECTION_T78		78
#define MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80	80
#define MXT_TOUCH_MULTI_T100		100
#define MXT_TOUCH_MULTITOUCHSCREEN_T100	100

/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* MXT_GEN_POWER_T7 field */
#define MXT_POWER_IDLEACQINT	0
#define MXT_POWER_ACTVACQINT	1
#define MXT_POWER_ACTV2IDLETO	2

#define MXT_POWER_CFG_RUN		0
#define MXT_POWER_CFG_DEEPSLEEP		1

/* MXT_GEN_ACQUIRE_T8 field */
#define MXT_ACQUIRE_CHRGTIME	0
#define MXT_ACQUIRE_TCHDRIFT	2
#define MXT_ACQUIRE_DRIFTST	3
#define MXT_ACQUIRE_TCHAUTOCAL	4
#define MXT_ACQUIRE_SYNC	5
#define MXT_ACQUIRE_ATCHCALST	6
#define MXT_ACQUIRE_ATCHCALSTHR	7
#define MXT_ACQUIRE_MEASALLOW	10

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_TOUCH_CTRL		0
#define MXT_TOUCH_XORIGIN	1
#define MXT_TOUCH_YORIGIN	2
#define MXT_TOUCH_XSIZE		3
#define MXT_TOUCH_YSIZE		4
#define MXT_TOUCH_BLEN		6
#define MXT_TOUCH_TCHTHR	7
#define MXT_TOUCH_TCHDI		8
#define MXT_TOUCH_ORIENT	9
#define MXT_TOUCH_MOVHYSTI	11
#define MXT_TOUCH_MOVHYSTN	12
#define MXT_TOUCH_NUMTOUCH	14
#define MXT_TOUCH_MRGHYST	15
#define MXT_TOUCH_MRGTHR	16
#define MXT_TOUCH_AMPHYST	17
#define MXT_TOUCH_XRANGE_LSB	18
#define MXT_TOUCH_XRANGE_MSB	19
#define MXT_TOUCH_YRANGE_LSB	20
#define MXT_TOUCH_YRANGE_MSB	21
#define MXT_TOUCH_XLOCLIP	22
#define MXT_TOUCH_XHICLIP	23
#define MXT_TOUCH_YLOCLIP	24
#define MXT_TOUCH_YHICLIP	25
#define MXT_TOUCH_XEDGECTRL	26
#define MXT_TOUCH_XEDGEDIST	27
#define MXT_TOUCH_YEDGECTRL	28
#define MXT_TOUCH_YEDGEDIST	29
#define MXT_TOUCH_JUMPLIMIT	30

/* MXT_TOUCH_MULTI_T100 field */
#define MXT_MULTITOUCH_CTRL		0
#define MXT_MULTITOUCH_CFG1		1
#define MXT_MULTITOUCH_SCRAUX			2
#define MXT_MULTITOUCH_TCHAUX		3
#define MXT_MULTITOUCH_TCHEVENTCFG		4
#define MXT_MULTITOUCH_AKSCFG			5
#define MXT_MULTITOUCH_NUMTCH		6
#define MXT_MULTITOUCH_XYCFG		7
#define MXT_MULTITOUCH_XORIGIN		8
#define MXT_MULTITOUCH_XSIZE		9
#define MXT_MULTITOUCH_XPITCH			10
#define MXT_MULTITOUCH_XLOCLIP		11
#define MXT_MULTITOUCH_XHICLIP		12
#define MXT_MULTITOUCH_XRANGE_LSB		13
#define MXT_MULTITOUCH_XRANGE_MSB		14
#define MXT_MULTITOUCH_XEDGECFG		15
#define MXT_MULTITOUCH_XEDGEDIST		16
#define MXT_MULTITOUCH_DXEDGECFG		17
#define MXT_MULTITOUCH_DXEDGEDIST		18
#define MXT_MULTITOUCH_YORIGIN		19
#define MXT_MULTITOUCH_YSIZE		20
#define MXT_MULTITOUCH_YPITCH			21
#define MXT_MULTITOUCH_YLOCLIP		22
#define MXT_MULTITOUCH_YHICLIP		23
#define MXT_MULTITOUCH_YRANGE_LSB		24
#define MXT_MULTITOUCH_YRANGE_MSB		25
#define MXT_MULTITOUCH_YEDGECFG		26
#define MXT_MULTITOUCH_YEDGEDIST		27
#define MXT_MULTITOUCH_GAIN		28
#define MXT_MULTITOUCH_DXGAIN			29
#define MXT_MULTITOUCH_TCHTHR			30
#define MXT_MULTITOUCH_TCHHYST		31
#define MXT_MULTITOUCH_INTTHR			32
#define MXT_MULTITOUCH_NOISESF		33
#define MXT_MULTITOUCH_MGRTHR		35
#define MXT_MULTITOUCH_MRGTHRADJSTR		36
#define MXT_MULTITOUCH_MRGHYST		37
#define MXT_MULTITOUCH_DXTHRSF		38
#define MXT_MULTITOUCH_TCHDIDOWN		39
#define MXT_MULTITOUCH_TCHDIUP		40
#define MXT_MULTITOUCH_NEXTTCHDI		41
#define MXT_MULTITOUCH_JUMPLIMIT		43
#define MXT_MULTITOUCH_MOVFILTER		44
#define MXT_MULTITOUCH_MOVSMOOTH		45
#define MXT_MULTITOUCH_MOVPRED		46
#define MXT_MULTITOUCH_MOVHYSTILSB		47
#define MXT_MULTITOUCH_MOVHYSTIMSB		48
#define MXT_MULTITOUCH_MOVHYSTNLSB		49
#define MXT_MULTITOUCH_MOVHYSTNMSB		50
#define MXT_MULTITOUCH_AMPLHYST		51
#define MXT_MULTITOUCH_SCRAREAHYST		52

/* MXT_TOUCH_KEYARRAY_T15 */
#define MXT_KEYARRAY_CTRL	0

/* MXT_PROCI_GRIPFACE_T20 field */
#define MXT_GRIPFACE_CTRL	0
#define MXT_GRIPFACE_XLOGRIP	1
#define MXT_GRIPFACE_XHIGRIP	2
#define MXT_GRIPFACE_YLOGRIP	3
#define MXT_GRIPFACE_YHIGRIP	4
#define MXT_GRIPFACE_MAXTCHS	5
#define MXT_GRIPFACE_SZTHR1	7
#define MXT_GRIPFACE_SZTHR2	8
#define MXT_GRIPFACE_SHPTHR1	9
#define MXT_GRIPFACE_SHPTHR2	10
#define MXT_GRIPFACE_SUPEXTTO	11

/* MXT_PROCI_NOISE field */
#define MXT_NOISE_CTRL		0
#define MXT_NOISE_OUTFLEN	1
#define MXT_NOISE_GCAFUL_LSB	3
#define MXT_NOISE_GCAFUL_MSB	4
#define MXT_NOISE_GCAFLL_LSB	5
#define MXT_NOISE_GCAFLL_MSB	6
#define MXT_NOISE_ACTVGCAFVALID	7
#define MXT_NOISE_NOISETHR	8
#define MXT_NOISE_FREQHOPSCALE	10
#define MXT_NOISE_FREQ0		11
#define MXT_NOISE_FREQ1		12
#define MXT_NOISE_FREQ2		13
#define MXT_NOISE_FREQ3		14
#define MXT_NOISE_FREQ4		15
#define MXT_NOISE_IDLEGCAFVALID	16

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1

/* MXT_SPT_GPIOPWM_T19 */
#define MXT_GPIOPWM_CTRL		0
#define MXT_GPIOPWM_INTPULLUP		3
#define MXT_GPIO_FORCERPT		0x7

/* MXT_SPT_CTECONFIG_T28 field */
#define MXT_CTE_CTRL		0
#define MXT_CTE_CMD		1
#define MXT_CTE_MODE		2
#define MXT_CTE_IDLEGCAFDEPTH	3
#define MXT_CTE_ACTVGCAFDEPTH	4
#define MXT_CTE_VOLTAGE		5

#define MXT_VOLTAGE_DEFAULT	2700000
#define MXT_VOLTAGE_STEP	10000

/* MXT_DEBUG_DIAGNOSTIC_T37 */
#define MXT_DIAG_PAGE_UP	0x01
#define MXT_DIAG_MULT_DELTA	0x10
#define MXT_DIAG_MULT_REF	0x11
#define MXT_DIAG_PAGE_SIZE	0x80
#define MXT_DIAG_TOTAL_SIZE	0x2A0
#define MXT_DIAG_SELF_SIZE	0x5A

/* MXT_SPT_USERDATA_T38 */
#define MXT_FW_UPDATE_FLAG	0

/* MXT_PROCI_STYLUS_T47 */
#define MXT_PSTYLUS_CTRL	0

/* MXT_PROCG_NOISESUPPRESSION_T72 */
#define MXT_NOISESUP_CALCFG	1
#define MXT_NOISESUP_CFG1	2
#define MXT_T72_NOISE_SUPPRESSION_NOISELVCHG    (1 << 4)

/* MXT_PROCI_GLOVEDETECTION_T78 */
#define MXT_GLOVE_CTRL		0x00

/* Defines for Suspend/Resume */
#define MXT_SUSPEND_STATIC      0
#define MXT_SUSPEND_DYNAMIC     1
#define MXT_T7_IDLEACQ_DISABLE  0
#define MXT_T7_ACTVACQ_DISABLE  0
#define MXT_T7_ACTV2IDLE_DISABLE 0
#define MXT_T9_DISABLE          0
#define MXT_T9_ENABLE           0x83
#define MXT_T22_DISABLE         0
#define MXT_T100_DISABLE	0

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_RESET_VALUE		0x01
#define MXT_BACKUP_VALUE	0x55

/* Define for MXT_PROCG_NOISESUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* Delay times */
#define MXT_BACKUP_TIME		25	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_NOCHGREAD	400	/* msec */
#define MXT_FWRESET_TIME	1000	/* msec */
#define MXT_WAKEUP_TIME		25	/* msec */

/* Defines for MXT_SLOWSCAN_EXTENSIONS */
#define SLOSCAN_DISABLE         0       /* Disable slow scan */
#define SLOSCAN_ENABLE          1       /* Enable slow scan */
#define SLOSCAN_SET_ACTVACQINT  2       /* Set ACTV scan rate */
#define SLOSCAN_SET_IDLEACQINT  3       /* Set IDLE scan rate */
#define SLOSCAN_SET_ACTV2IDLETO 4       /* Set the ACTIVE to IDLE TimeOut */

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

/* Define for T6 status byte */
#define MXT_STATUS_RESET	(1 << 7)
#define MXT_STATUS_OFL		(1 << 6)
#define MXT_STATUS_SIGERR	(1 << 5)
#define MXT_STATUS_CAL		(1 << 4)
#define MXT_STATUS_CFGERR	(1 << 3)
#define MXT_STATUS_COMSERR	(1 << 2)

/* Define for T8 measallow byte */
#define MXT_MEASALLOW_MULT	(1 << 0)
#define MXT_MEASALLOW_SELT	(1 << 1)

/* T9 Touch status */
#define MXT_T9_UNGRIP		(1 << 0)
#define MXT_T9_SUPPRESS		(1 << 1)
#define MXT_T9_AMP		(1 << 2)
#define MXT_T9_VECTOR		(1 << 3)
#define MXT_T9_MOVE		(1 << 4)
#define MXT_T9_RELEASE		(1 << 5)
#define MXT_T9_PRESS		(1 << 6)
#define MXT_T9_DETECT		(1 << 7)

/* T100 Touch status */
#define MXT_T100_EVENT		(1 << 0)
#define MXT_T100_TYPE		(1 << 4)
#define MXT_T100_DETECT	(1 << 7)
#define MXT_T100_VECT		(1 << 0)
#define MXT_T100_AMPL		(1 << 1)
#define MXT_T100_AREA		(1 << 2)
#define MXT_T100_PEAK		(1 << 4)

#define MXT_T100_SUP		(1 << 6)

/* T15 KeyArray */
#define MXT_KEY_ADAPTTHREN	(1 << 2)

/* Touch orient bits */
#define MXT_XY_SWITCH		(1 << 0)
#define MXT_X_INVERT		(1 << 1)
#define MXT_Y_INVERT		(1 << 2)

/* T47 passive stylus */
#define MXT_PSTYLUS_ENABLE	(1 << 0)

/* T63 Stylus */
#define MXT_STYLUS_PRESS	(1 << 0)
#define MXT_STYLUS_RELEASE	(1 << 1)
#define MXT_STYLUS_MOVE		(1 << 2)
#define MXT_STYLUS_SUPPRESS	(1 << 3)

#define MXT_STYLUS_DETECT	(1 << 4)
#define MXT_STYLUS_TIP		(1 << 5)
#define MXT_STYLUS_ERASER	(1 << 6)
#define MXT_STYLUS_BARREL	(1 << 7)

#define MXT_STYLUS_PRESSURE_MASK	0x3F

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

/* T66 Golden Reference */
#define MXT_GOLDENREF_CTRL		0x00
#define MXT_GOLDENREF_FCALFAILTHR	0x01
#define MXT_GOLDENREF_FCALDRIFTCNT	0x02
#define MXT_GOLDENREF_FCALDRIFTCOEF	0x03
#define MXT_GOLDENREF_FCALDRIFTTLIM	0x04

#define MXT_GOLDCTRL_ENABLE		(1 << 0)
#define MXT_GOLDCTRL_REPEN		(1 << 1)

#define MXT_GOLDSTS_BADSTOREDATA	(1 << 0)
#define MXT_GOLDSTS_FCALSEQERR	(1 << 3)
#define MXT_GOLDSTS_FCALSEQTO		(1 << 4)
#define MXT_GOLDSTS_FCALSEQDONE	(1 << 5)
#define MXT_GOLDSTS_FCALPASS		(1 << 6)
#define MXT_GOLDSTS_FCALFAIL		(1 << 7)

#define MXT_GOLDCMD_NONE	0x00
#define MXT_GOLDCMD_PRIME	0x04
#define MXT_GOLDCMD_GENERATE	0x08
#define MXT_GOLDCMD_CONFIRM	0x0C

#define MXT_GOLD_CMD_MASK	0x0C

#define MXT_GOLDSTATE_INVALID	0xFF
#define MXT_GOLDSTATE_IDLE	MXT_GOLDSTS_FCALSEQDONE
#define MXT_GOLDSTATE_PRIME	0x02
#define MXT_GOLDSTATE_GEN	0x04
#define MXT_GOLDSTATE_GEN_PASS	(0x04 | MXT_GOLDSTS_FCALPASS)
#define MXT_GOLDSTATE_GEN_FAIL	(0x04 | MXT_GOLDSTS_FCALFAIL)

#define MXT_GOLD_STATE_MASK	0x06

/* T78 glove setting */
#define MXT_GLOVECTL_ALL_ENABLE	0xB9
#define MXT_GLOVECTL_GAINEN	(1 << 4)

/* T80 retransmission */
#define MXT_RETRANS_CTRL	0x0
#define MXT_RETRANS_ATCHTHR	0x4
#define MXT_RETRANS_CTRL_MOISTCALEN	(1 << 4)

/* T72 noise suppression */
#define MXT_NOICFG_VNOISY	(1 << 1)
#define MXT_NOICFG_NOISY	(1 << 0)

#define MXT_DEBUGFS_DIR		"atmel_mxt_ts"
#define MXT_DEBUGFS_FILE		"object"

#define MXT_INPUT_EVENT_START		0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_ON		1
#define MXT_INPUT_EVENT_STYLUS_MODE_OFF		2
#define MXT_INPUT_EVENT_STYLUS_MODE_ON		3
#define MXT_INPUT_EVENT_END		3

#define MXT_MAX_FINGER_NUM	10

#define MXT_VTG_MIN_UV		2600000
#define MXT_VTG_MAX_UV		3300000
#define MXT_I2C_VTG_MIN_UV	1800000
#define MXT_I2C_VTG_MAX_UV	1800000


struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u16 size;
	u16 instances;
	u8 num_report_ids;

	/* to map object and message */
	u8 min_reportid;
	u8 max_reportid;
};

enum mxt_device_state { INIT, APPMODE, BOOTLOADER, FAILED, SHUTDOWN };

/* This structure is used to save/restore values during suspend/resume */
struct mxt_suspend {
	u8 suspend_obj;
	u8 suspend_reg;
	u8 suspend_val;
	u8 suspend_flags;
	u8 restore_val;
};

struct mxt_golden_msg {
	u8 status;
	u8 fcalmaxdiff;
	u8 fcalmaxdiffx;
	u8 fcalmaxdiffy;
};

struct mxt_mode_switch {
	struct mxt_data *data;
	u8 mode;
	struct work_struct switch_mode_work;
};

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct mxt_platform_data *pdata;
	enum mxt_device_state state;
	struct mxt_object *object_table;
	struct regulator *regulator_vdd;
	u16 mem_size;
	struct mxt_info info;
	unsigned int irq;
	unsigned int max_x;
	unsigned int max_y;
	bool in_bootloader;
	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool driver_paused;
	u8 bootloader_addr;
	u8 actv_cycle_time;
	u8 idle_cycle_time;
	u8 actv2idle_timeout;
	u8 is_stopped;
	u8 max_reportid;
	u32 config_crc;
	u32 info_block_crc;
	u8 num_touchids;
	u8 num_stylusids;
	u8 *msg_buf;
	u8 last_message_count;
	u8 t100_tchaux_bits;
	unsigned long keystatus;
	u8 vendor_id;
	int current_index;
	u8 update_flag;
	u8 test_result[6];
	int touch_num;
	u8 diag_mode;
	u8 atchthr;
	u8 sensitive_mode;
	int golden_ok;

	/* Slowscan parameters	*/
	int slowscan_enabled;
	u8 slowscan_actv_cycle_time;
	u8 slowscan_idle_cycle_time;
	u8 slowscan_actv2idle_timeout;
	u8 slowscan_shad_actv_cycle_time;
	u8 slowscan_shad_idle_cycle_time;
	u8 slowscan_shad_actv2idle_timeout;
	struct mxt_golden_msg golden_msg;

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T5_msg_size;
	u16 T6_address;
	u8 T6_reportid;
	u16 T7_address;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	u16 T8_address;
	u16 T9_address;
#endif
	u8 T9_reportid_min;
	u8 T9_reportid_max;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	u16 T15_address;
#endif
	u8 T15_reportid_min;
	u8 T15_reportid_max;
	u8 T19_reportid_min;
	u8 T19_reportid_max;
	u8 T25_reportid_min;
	u8 T25_reportid_max;
	u16 T37_address;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	u16 T38_address;
	u16 T40_address;
#endif
	u8 T42_reportid_min;
	u8 T42_reportid_max;
	u16 T44_address;
	u8 T48_reportid;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	u16 T55_address;
	u8 T61_reportid_min;
	u8 T61_reportid_max;
	u16 T61_address;
	u16 T61_instances;
#endif
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u8 T66_reportid;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	u16 T65_address;
	u16 T72_address;
	u8 T72_reported_min;
	u8 T72_reportid_max;
	u16 T80_address;
	u16 T100_address;
#endif
	u8 T100_reportid_min;
	u8 T100_reportid_max;

	/* Indicates whether device is in suspend */
	//bool is_stopped;

#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
	struct task_struct *irq_tsk;
	wait_queue_head_t wait;
#define MXT_EVENT_IRQ 1
#define MXT_EVENT_EXTERN 2
#define MXT_EVENT_IRQ_FLAG 5
	unsigned long busy;
#endif

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	struct plug_interface plug_in;
#endif
};

static struct mxt_suspend mxt_save[] = {
	{MXT_GEN_POWER_T7, MXT_POWER_IDLEACQINT,
		MXT_T7_IDLEACQ_DISABLE, MXT_SUSPEND_DYNAMIC, 0},
	{MXT_GEN_POWER_T7, MXT_POWER_ACTVACQINT,
		MXT_T7_ACTVACQ_DISABLE, MXT_SUSPEND_DYNAMIC, 0},
	{MXT_GEN_POWER_T7, MXT_POWER_ACTV2IDLETO,
		MXT_T7_ACTV2IDLE_DISABLE, MXT_SUSPEND_DYNAMIC, 0}
};

/* I2C slave address pairs */
struct mxt_i2c_address_pair {
	u8 bootloader;
	u8 application;
};

static const struct mxt_i2c_address_pair mxt_i2c_addresses[] = {
#ifdef BOOTLOADER_1664_1188
	{ 0x26, 0x4a },
	{ 0x27, 0x4b },
#else
	{ 0x24, 0x4a },
	{ 0x25, 0x4b },
	{ 0x26, 0x4c },
	{ 0x27, 0x4d },
	{ 0x34, 0x5a },
	{ 0x35, 0x5b },
#endif
};

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static void mxt_plugin_pre_process(struct plug_interface *pl, bool in_boot);
static long mxt_plugin_post_process(struct plug_interface *pl, bool in_boot);
static void mxt_plugin_hook_t6(struct plug_interface *pl, u8 status);
static void mxt_plugin_hook_t9(struct plug_interface *pl, int id, int x, int y, u8 status);
static int mxt_plugin_hook_t61(struct plug_interface *pl, int id, u8 status);
static void mxt_plugin_hook_t72(struct plug_interface *pl, u8* msg);
static int mxt_plugin_hook_reg_init(struct plug_interface *pl, u8 *config_mem, size_t config_mem_size, int cfg_start_ofs);
static void mxt_plugin_hook_reset_slots(struct plug_interface *pl);
static void mxt_plugin_hook_t100(struct plug_interface *pl, int id, int x, int y, u8 status);
static int mxt_plugin_hook_set_t7(struct plug_interface *pl, bool sleep);
static int mxt_plugin_cal_t37_check_and_calibrate(struct plug_interface *pl, bool check_sf);
static void mxt_plugin_thread_stopped(struct plug_interface *pl);
static int mxt_plugin_force_stop(struct plug_interface *pl);
static int mxt_plugin_start(struct plug_interface *pl, bool resume);
static int mxt_plugin_stop(struct plug_interface *pl, bool suspend);
static int mxt_plugin_pre_init(struct mxt_data *data);
static int mxt_plugin_init(struct mxt_data *data);
static void mxt_plugin_deinit(struct plug_interface *pl);
static ssize_t mxt_plugin_show(struct device *dev,	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_store(struct device *dev,	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_plugin_tag_show(struct device *dev, struct device_attribute *attr, char *buf);
#endif

static int mxt_bootloader_read(struct mxt_data *data, u8 *val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);

	return (ret == 1) ? 0 : ret;
}

static int mxt_bootloader_write(struct mxt_data *data, const u8 * const val,
	unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (u8 *)val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);

	return (ret == 1) ? 0 : ret;
}

static int mxt_get_bootloader_address(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int i;

	for (i = 0; i < ARRAY_SIZE(mxt_i2c_addresses); i++) {
		if (mxt_i2c_addresses[i].application == client->addr) {
			data->bootloader_addr = mxt_i2c_addresses[i].bootloader;

			dev_info(&client->dev, "Bootloader i2c addr: 0x%02x\n",
				data->bootloader_addr);

			return 0;
		}
	}

	dev_err(&client->dev, "Address 0x%02x not found in address table\n",
		client->addr);
	return -EINVAL;
}

static int mxt_probe_bootloader(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;
	bool crc_failure;

	ret = mxt_get_bootloader_address(data);
	if (ret)
		return ret;

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret) {
		dev_err(dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "Detected bootloader, status:%02X%s\n",
		val, crc_failure ? ", APP_CRC_FAIL" : "");

	return 0;
}

static u8 mxt_read_chg(struct mxt_data *data)
{
	int gpio_intr = data->pdata->irq_gpio;

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
		return -EIO;
	}

	return 0;
}

static u8 mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (mxt_bootloader_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return -EIO;
		}

		dev_info(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_info(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data,
				unsigned int state)
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
			mxt_wait_for_chg(data);
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
	if (ret) {
		dev_err(&data->client->dev, "%s: i2c send failed, ret=%d\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static int mxt_read_reg(struct i2c_client *client,
			u16 reg, u16 len, void *val)
{
	struct device *dev = &client->dev;
	struct i2c_msg xfer[2];
	u8 buf[2];
	int ret;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	ret = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		dev_err(dev, "%s: i2c transfer failed (%d)\n",
			__func__, ret);
		return -EIO;
	}

	return 0;
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	struct device *dev = &client->dev;
	u8 buf[3];

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = val;

	if (i2c_master_send(client, buf, 3) != 3) {
		dev_err(dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt_write_block(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
	int i;
	struct {
		__le16 le_addr;
		u8  data[MXT_MAX_BLOCK_WRITE];
	} i2c_block_transfer;

	if (length > MXT_MAX_BLOCK_WRITE)
		return -EINVAL;

	memcpy(i2c_block_transfer.data, value, length);

	i2c_block_transfer.le_addr = cpu_to_le16(addr);

	i = i2c_master_send(client, (u8 *) &i2c_block_transfer, length + 2);

	if (i == (length + 2))
		return 0;
	else
		return -EIO;
}

int __mxt_read_reg(struct i2c_client *client,
			u16 reg, u16 len, void *val)
{
	return mxt_read_reg(client, reg, len, val);
}

int __mxt_write_reg(struct i2c_client *client,
			u16 reg, u16 len, void *val)
{
	return mxt_write_block(client, reg, len, (u8*)val);
}


static struct mxt_object *mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type T%u\n", type);
	return NULL;
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
	return mxt_read_reg(data->client, reg + offset, 1, val);
}

static int mxt_write_object(struct mxt_data *data,
				 u8 type, u8 offset, u8 val)
{
	struct mxt_object *object;
	u16 reg;
	int ret;

	object = mxt_get_object(data, type);
	if (!object || offset >= object->size)
		return -EINVAL;

	if (offset >= object->size * object->instances) {
		dev_err(&data->client->dev, "Tried to write outside object T%d"
			" offset:%d, size:%d\n", type, offset, object->size);
		return -EINVAL;
	}

	reg = object->start_address;
	if (data->debug_enabled)
		dev_info(&data->client->dev, "write to object %d, reg 0x%02x, val 0x%x\n",
				(int)type, reg + offset, val);
	ret = mxt_write_reg(data->client, reg + offset, val);

	return ret;
}

static int mxt_soft_reset(struct mxt_data *data, u8 value)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "Resetting chip\n");

	mxt_write_object(data, MXT_GEN_COMMAND_T6,
			MXT_COMMAND_RESET, value);

	msleep(MXT_RESET_NOCHGREAD);

	return 0;
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u32 crc;
	u8 status = msg[1];

	crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	if (crc != data->config_crc) {
		data->config_crc = crc;
		dev_dbg(dev, "T6 cfg crc 0x%06X\n", crc);
	}

	if (status & MXT_STATUS_CAL) {
		dev_info(dev, "Calibration start!\n");
	}

	if (status)
		dev_dbg(dev, "T6 status %s%s%s%s%s%s\n",
			(status & MXT_STATUS_RESET) ? "RESET " : "",
			(status & MXT_STATUS_OFL) ? "OFL " : "",
			(status & MXT_STATUS_SIGERR) ? "SIGERR " : "",
			(status & MXT_STATUS_CAL) ? "CAL " : "",
			(status & MXT_STATUS_CFGERR) ? "CFGERR " : "",
			(status & MXT_STATUS_COMSERR) ? "COMSERR " : "");

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t6(&data->plug_in, status);
#endif
}

static void mxt_input_sync(struct mxt_data *data)
{
	input_mt_report_pointer_emulation(data->input_dev, false);
	input_sync(data->input_dev);
}

static void mxt_proc_t9_messages(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;
	int id;

	if (!input_dev || data->driver_paused)
		return;

	id = message[0] - data->T9_reportid_min;

	if (id < 0 || id > data->num_touchids) {
		dev_err(dev, "invalid touch id %d, total num touch is %d\n",
			id, data->num_touchids);
		return;
	}

	status = message[1];

	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;
	area = message[5];
	amplitude = message[6];
	vector = message[7];

	if (data->debug_enabled) {
		dev_info(dev,
			"[%d] %c%c%c%c%c%c%c%c x: %d y: %d area: %d amp: %d vector: %02X\n",
			id,
			(status & MXT_T9_DETECT) ? 'D' : '.',
			(status & MXT_T9_PRESS) ? 'P' : '.',
			(status & MXT_T9_RELEASE) ? 'R' : '.',
			(status & MXT_T9_MOVE) ? 'M' : '.',
			(status & MXT_T9_VECTOR) ? 'V' : '.',
			(status & MXT_T9_AMP) ? 'A' : '.',
			(status & MXT_T9_SUPPRESS) ? 'S' : '.',
			(status & MXT_T9_UNGRIP) ? 'U' : '.',
			x, y, area, amplitude, vector);
	}

	input_mt_slot(input_dev, id);

	if ((status & MXT_T9_DETECT) && (status & MXT_T9_RELEASE)) {
		/* Touch in detect, just after being released, so
		 * get new touch tracking ID */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		mxt_input_sync(data);
	}

	if (status & MXT_T9_DETECT) {
		/* Touch in detect, report X/Y position */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);

		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
		input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
	} else {
		/* Touch no longer in detect, so close out slot */
		mxt_input_sync(data);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t9(&data->plug_in, id, x, y, status);
#endif
}

static int mxt_do_diagnostic(struct mxt_data *data, u8 mode)
{
	int error = 0;
	u8 val;
	int time_out = 500;
	int i = 0;

	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_DIAGNOSTIC, mode);
	if (error) {
		dev_err(&data->client->dev, "Failed to diag ref data value\n");
		return error;
	}

	while(i < time_out) {
		error = mxt_read_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_DIAGNOSTIC, &val);
		if (error) {
			dev_err(&data->client->dev, "Failed to diag ref data value\n");
			 return error;
		}
		if (val == 0)
			return 0;
		i++;
	}

	return -ETIMEDOUT;
}

static void mxt_proc_t100_messages(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 status;
	int x;
	int y;
	int area = 0;
	int amplitude = 0;
	u8 vector = 0;
	u8 peak = 0;
	int id;
	int index = 0;

	if (!input_dev || data->driver_paused)
		return;

	id = message[0] - data->T100_reportid_min;

	if (id < 0 || id > data->num_touchids) {
		dev_err(dev, "invalid touch id %d, total num touch is %d\n",
			id, data->num_touchids);
		return;
	}

	if (id == 0) {
		status = message[1];
		data->touch_num = message[2];
		if (data->debug_enabled)
			dev_info(dev, "touch num = %d\n", data->touch_num);

		if (status & MXT_T100_SUP)
		{
			int i;
			for (i = 0; i < data->num_touchids - 2; i++) {
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
			}
			mxt_input_sync(data);
		}
	}
	else if (id >= 2) {
		/* deal with each point report */
		status = message[1];
		x = (message[3] << 8) | (message[2] & 0xFF);
		y = (message[5] << 8) | (message[4] & 0xFF);
		index = 6;

		if (data->t100_tchaux_bits &  MXT_T100_VECT)
			vector = message[index++];
		if (data->t100_tchaux_bits &  MXT_T100_AMPL) {
			amplitude = message[index++];
		}
		if (data->t100_tchaux_bits &  MXT_T100_AREA) {
			area = message[index++];
		}
		if (data->t100_tchaux_bits &  MXT_T100_PEAK)
			peak = message[index++];

		input_mt_slot(input_dev, id - 2);

		if (status & MXT_T100_DETECT) {
			/* Touch in detect, report X/Y position */
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
			input_report_abs(input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

			if (data->t100_tchaux_bits &  MXT_T100_AMPL)
			{
				if (amplitude == 0)
					amplitude = 1;
				input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
			}
			if (data->t100_tchaux_bits &  MXT_T100_AREA) {
				if (area == 0)
					area = 1;
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
			}
			if (data->t100_tchaux_bits &  MXT_T100_VECT) {
				if (vector == 0)
					vector = 1;
				input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
			}
		} else {
			/* Touch no longer in detect, so close out slot */
			mxt_input_sync(data);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		}
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t100(&data->plug_in, id, x, y, status);
#endif
}

static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	u8 key;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = le32_to_cpu(msg[2]);
	int index = data->current_index;

	if (!input_dev)
		return;

	for (key = 0; key < pdata->config_array[index].key_num; key++) {
		curr_state = test_bit(key, &data->keystatus);
		new_state = test_bit(key, &keystates);

		if (!curr_state && new_state) {
			dev_dbg(dev, "T15 key press: %u\n", key);
			__set_bit(key, &data->keystatus);
			input_event(input_dev, EV_KEY, pdata->config_array[index].key_codes[key], 1);
			sync = true;
		} else if (curr_state && !new_state) {
			dev_dbg(dev, "T15 key release: %u\n", key);
			__clear_bit(key, &data->keystatus);
			input_event(input_dev, EV_KEY,  pdata->config_array[index].key_codes[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}

static void mxt_proc_t19_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;

	data->vendor_id = msg[1];
	data->vendor_id &= pdata->gpio_mask;
	dev_info(dev, "T19: vendor_id = 0x%x\n", data->vendor_id);
}

static void mxt_proc_t25_messages(struct mxt_data *data, u8 *msg)
{
	memcpy(data->test_result,
		&msg[1], sizeof(data->test_result));
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
	struct device *dev = &data->client->dev;
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	dev_dbg(dev, "T48 state %d status %02X %s%s%s%s%s\n",
			state,
			status,
			(status & 0x01) ? "FREQCHG " : "",
			(status & 0x02) ? "APXCHG " : "",
			(status & 0x04) ? "ALGOERR " : "",
			(status & 0x10) ? "STATCHG " : "",
			(status & 0x20) ? "NLVLCHG " : "");

	return 0;
}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static int mxt_proc_t61_message(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	int id;
	u8 status;

	id = msg[0] - data->T61_reportid_min;
	status = msg[1];
	dev_info(dev, "T61 status 0x%x\n", status);

	mxt_plugin_hook_t61(&data->plug_in, id, status);

	return 0;
}
#endif

static void mxt_proc_t63_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 id;
	u16 x, y;
	u8 pressure;

	if (!input_dev)
		return;

	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);

	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_STYLUS_PRESSURE_MASK;

	dev_dbg(dev,
		"[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
		id,
		(msg[1] & MXT_STYLUS_SUPPRESS) ? 'S' : '.',
		(msg[1] & MXT_STYLUS_MOVE)     ? 'M' : '.',
		(msg[1] & MXT_STYLUS_RELEASE)  ? 'R' : '.',
		(msg[1] & MXT_STYLUS_PRESS)    ? 'P' : '.',
		x, y, pressure,
		(msg[2] & MXT_STYLUS_BARREL) ? 'B' : '.',
		(msg[2] & MXT_STYLUS_ERASER) ? 'E' : '.',
		(msg[2] & MXT_STYLUS_TIP)    ? 'T' : '.',
		(msg[2] & MXT_STYLUS_DETECT) ? 'D' : '.');

	input_mt_slot(input_dev, id);

	if (msg[2] & MXT_STYLUS_DETECT) {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
	} else {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 0);
	}

	input_report_key(input_dev, BTN_STYLUS, (msg[2] & MXT_STYLUS_ERASER));
	input_report_key(input_dev, BTN_STYLUS2, (msg[2] & MXT_STYLUS_BARREL));

	mxt_input_sync(data);
}

static void mxt_proc_t66_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "message for t66= 0x%x 0x%x 0x%x 0x%x\n",
			msg[1], msg[2], msg[3], msg[4]);

	data->golden_msg.status = msg[1];
	data->golden_msg.fcalmaxdiff = msg[2];
	data->golden_msg.fcalmaxdiffx = msg[3];
	data->golden_msg.fcalmaxdiffy = msg[4];
}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static void mxt_proc_t72_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "T72 noise state1 = 0x%x state2 = 0x%x\n",
		msg[1], msg[2]);

	if (msg[1] & MXT_T72_NOISE_SUPPRESSION_NOISELVCHG) {
		dev_info(dev, "T72 noise change, state = %d, peak = %d, level = %d\n",
			msg[2] & 0x7, msg[4], msg[5]);
	}

	mxt_plugin_hook_t72(&data->plug_in, msg);
}
#endif

static int mxt_proc_message(struct mxt_data *data, u8 *msg)
{
	u8 report_id = msg[0];

	if (report_id == MXT_RPTID_NOMSG)
		return -1;

	if (data->debug_enabled)
		print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1,
			       msg, data->T5_msg_size, false);

	if (report_id >= data->T9_reportid_min
	    && report_id <= data->T9_reportid_max) {
		mxt_proc_t9_messages(data, msg);
	} else if (report_id >= data->T63_reportid_min
		   && report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, msg);
	} else if (report_id >= data->T15_reportid_min
		   && report_id <= data->T15_reportid_max) {
		mxt_proc_t15_messages(data, msg);
	} else if (report_id >= data->T19_reportid_min
		   && report_id <= data->T19_reportid_max) {
		mxt_proc_t19_messages(data, msg);
	} else if (report_id >= data->T25_reportid_min
		   && report_id <= data->T25_reportid_max) {
		mxt_proc_t25_messages(data, msg);
	} else if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, msg);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, msg);
	} else if (report_id >= data->T42_reportid_min
		   && report_id <= data->T42_reportid_max) {
		mxt_proc_t42_messages(data, msg);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	} else if (report_id >= data->T61_reportid_min
		   && report_id <= data->T61_reportid_max) {
		mxt_proc_t61_message(data, msg);
#endif
	} else if (report_id == data->T66_reportid) {
		mxt_proc_t66_messages(data, msg);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	} else if (report_id >= data->T72_reported_min
		   && report_id <= data->T72_reportid_max) {
		mxt_proc_t72_messages(data, msg);
#endif
	} else if (report_id >= data->T100_reportid_min
		   && report_id <= data->T100_reportid_max) {
		mxt_proc_t100_messages(data, msg);
	}

	return 0;
}

static int mxt_read_count_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u8 num_valid = 0;

	/* Safety check for msg_buf */
	if (count > data->max_reportid)
		return -EINVAL;

	/* Process remaining messages if necessary */
	ret = mxt_read_reg(data->client, data->T5_address,
				data->T5_msg_size * count, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d)\n", count, ret);
		return ret;
	}

	for (i = 0;  i < count; i++) {
		ret = mxt_proc_message(data,
			data->msg_buf + data->T5_msg_size * i);

		if (ret == 0)
			num_valid++;
		else
			break;
	}

	/* return number of messages read */
	return num_valid;
}

static irqreturn_t mxt_read_messages_t44(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count, num_left;

	/* Read T44 and T5 together */
	ret = mxt_read_reg(data->client, data->T44_address,
		data->T5_msg_size + 1, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read T44 and T5 (%d)\n", ret);
		return IRQ_NONE;
	}

	count = data->msg_buf[0];

	if (count == 0) {
		dev_dbg(dev, "Interrupt triggered but zero messages\n");
		return IRQ_NONE;
	} else if (count > data->max_reportid) {
		dev_err(dev, "T44 count exceeded max report id\n");
		count = data->max_reportid;
	}

	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		dev_warn(dev, "Unexpected invalid message\n");
		return IRQ_NONE;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		ret = mxt_read_count_messages(data, num_left);
		if (ret < 0) {
			mxt_input_sync(data);
			return IRQ_NONE;
		} else if (ret != num_left) {
			dev_warn(dev, "Unexpected invalid message\n");
		}
	}

	mxt_input_sync(data);

	return IRQ_HANDLED;
}

static int mxt_read_t9_messages_until_invalid(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int count, read;
	u8 tries = 2;

	count = data->max_reportid;

	/* Read messages until we force an invalid */
	do {
		read = mxt_read_count_messages(data, count);
		if (read < count)
			return 0;
	} while (--tries);

	dev_err(dev, "CHG pin isn't cleared\n");
	return -EBUSY;
}

static irqreturn_t mxt_read_t9_messages(struct mxt_data *data)
{
	int total_handled, num_handled;
	u8 count = data->last_message_count;

	if (count < 1 || count > data->max_reportid)
		count = 1;

	/* include final invalid message */
	total_handled = mxt_read_count_messages(data, count + 1);
	if (total_handled < 0)
		return IRQ_NONE;
	/* if there were invalid messages, then we are done */
	else if (total_handled <= count)
		goto update_count;

	/* read two at a time until an invalid message or else we reach
	 * reportid limit */
	do {
		num_handled = mxt_read_count_messages(data, 2);
		if (num_handled < 0)
			return IRQ_NONE;

		total_handled += num_handled;

		if (num_handled < 2)
			break;
	} while (total_handled < data->num_touchids);

update_count:
	data->last_message_count = total_handled;
	mxt_input_sync(data);
	return IRQ_HANDLED;
}

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	struct mxt_data *data = dev_id;
	if (data->T44_address)
		return mxt_read_messages_t44(data);
	else
		return mxt_read_t9_messages(data);
}

#if defined(CONFIG_MXT_IRQ_WORKQUEUE)

static void mxt_active_proc_thread(void *dev_id, unsigned int event)
{
	struct mxt_data *data;

	data = (struct mxt_data *)dev_id;

	if (!data) {
		dev_err(&data->client->dev, "mxt_active_proc_thread invalid handle\n");
		return;
	}

	set_bit(event,&data->busy);
	dev_dbg(&data->client->dev, "mxt_active_proc_thread event %d busy %lx\n", event, data->busy);
	wake_up_interruptible(&data->wait);
}

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)

static void board_pulse_irq_thread(void)
{
	struct mxt_data *data = mxt_g_data;

	if (!data) {
		printk(KERN_ERR "MXT_EXTERNAL_TRIGGER_IRQ: mxt_g_data is not prepared \n");
		return;
	}

	board_disable_irq(data->pdata,data->irq);
	mxt_active_proc_thread(data,MXT_EVENT_IRQ);
}
#else
static irqreturn_t mxt_interrupt_pulse_workqueue(int irq, void *dev_id)
{
	struct mxt_data *data = (struct mxt_data *)dev_id;

#if !defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	disable_irq_nosync(irq);
#endif
	if (data) {
		//cancel_delayed_work_sync(&data->irq_work);
		mxt_active_proc_thread(data,MXT_EVENT_IRQ);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}
#endif

//warning: don't put any write/read code in this function
static int mxt_process_message_thread(void *dev_id)
{
	struct mxt_data *data = (struct mxt_data*)dev_id;
	long interval = MAX_SCHEDULE_TIMEOUT;
	int post = false;

	while (!kthread_should_stop()) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		mxt_plugin_pre_process(&data->plug_in,data->in_bootloader);
#endif
		set_current_state(TASK_INTERRUPTIBLE);

		wait_event_interruptible_timeout(
				data->wait,
				test_bit(MXT_EVENT_IRQ,&data->busy)||
				test_bit(MXT_EVENT_EXTERN,&data->busy)||
				kthread_should_stop(),
				interval);

		dev_dbg(&data->client->dev, "mxt_process_message_thread busy %lx suspend %d  boot %d interval %ld\n",
				data->busy,
				data->is_stopped,
				data->in_bootloader,
				interval);

		if (kthread_should_stop()) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			mxt_plugin_thread_stopped(&data->plug_in);
#endif
			break;
		}

		if (test_and_clear_bit(MXT_EVENT_IRQ,&data->busy)) {
			mxt_interrupt(data->irq, (void *)data);
			enable_irq(data->irq);
		}

		if (data->is_stopped) {
			interval = MAX_SCHEDULE_TIMEOUT;
			if (test_bit(MXT_EVENT_EXTERN,&data->busy))
				post = true;
		}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		else
			post = true;
		if (post) {
			clear_bit(MXT_EVENT_EXTERN,&data->busy);
			interval = (long)mxt_plugin_post_process(&data->plug_in,data->in_bootloader);
		}
#endif
	}

	set_current_state(TASK_RUNNING);

	return 0;
}
#endif

static void mxt_read_current_crc(struct mxt_data *data)
{
	/* CRC has already been read */
	if (data->config_crc > 0)
		return;

	mxt_write_object(data, MXT_GEN_COMMAND_T6,
		MXT_COMMAND_REPORTALL, 1);

	msleep(30);

	/* Read all messages until invalid, this will update the
	   config crc stored in mxt_data */
	mxt_read_t9_messages_until_invalid(data);

	/* on failure, CRC is set to 0 and config will always be downloaded */
}

static void mxt_do_calibration(struct mxt_data *data);

static int mxt_download_config(struct mxt_data *data, const char *fn)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	struct mxt_object *object;
	const struct firmware *cfg = NULL;
	int ret;
	int offset;
	int data_pos;
	int byte_offset;
	int i;
	int config_start_offset;
	u32 info_crc, config_crc;
	u8 *config_mem;
	size_t config_mem_size;
	int cfg_start_ofs;
	unsigned int type, instance, size;
	u8 val;
	u16 reg;

	ret = request_firmware(&cfg, fn, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n", fn);
		return 0;
	}

	mxt_read_current_crc(data);

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
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		print_trunk(cfg->data, data_pos, offset);
#endif
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
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	print_trunk(cfg->data, data_pos, offset);
#endif
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = -EINVAL;
		goto release;
	}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	print_trunk(cfg->data, data_pos, offset);
#endif
	data_pos += offset;

	/* The Info Block CRC is calculated over mxt_info and the object table
	 * If it does not match then we are trying to load the configuration
	 * from a different chip or firmware version, so the configuration CRC
	 * is invalid anyway. */
	if (info_crc == data->info_block_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "Config CRC 0x%06X: OK\n", data->config_crc);
#if defined(CONFIG_MXT_CAL_TRIGGER_CAL_WHEN_CFG_MATCH)
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			/* Recalibrate since chip has been in deep sleep */
			if (mxt_plugin_cal_t37_check_and_calibrate(&data->plug_in, true) != 0)
#endif
				mxt_do_calibration(data);
#endif
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev, "Info block CRC mismatch - attempting to apply config\n");
	}

	/* Malloc memory to store configuration */
	config_start_offset = MXT_OBJECT_START
		+ data->info.object_num * MXT_OBJECT_SIZE;
	cfg_start_ofs = config_start_offset + MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - config_start_offset;
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
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		print_trunk(cfg->data, data_pos, offset);
#endif
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret != 3) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release_mem;
		}
		data_pos += offset;

		pr_info("write to type = %d, instance = %d, size = %d, offset = %d\n",
			(int)type, (int)instance, (int)size, (int)offset);

		object = mxt_get_object(data, type);
		if (!object) {
			ret = -EINVAL;
			goto release_mem;
		}

		if (instance >= object->instances) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + object->size * instance;

		if (size > object->size) {
			/* Either we are in fallback mode due to wrong
			* config or config from a later fw version,
			* or the file is corrupt or hand-edited */
			dev_warn(dev, "Discarding %u bytes in T%u!\n",
				size - object->size, type);
			size = object->size;
		} else if (object->size > size) {
			/* If firmware is upgraded, new bytes may be added to
			* end of objects. It is generally forward compatible
			* to zero these bytes - previous behaviour will be
			* retained. However this does invalidate the CRC and
			* will force fallback mode until the configuration is
			* updated. We warn here but do nothing else - the
			* malloc has zeroed the entire configuration. */
			dev_warn(dev, "Zeroing %d byte(s) in T%d\n",
				object->size - size, type);
		}

		for (i = 0; i < size; i++) {
			ret = sscanf(cfg->data + data_pos, "%hhx%n",
					&val,
					&offset);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			print_trunk(cfg->data, data_pos, offset);
#endif
			if (ret != 1) {
				dev_err(dev, "Bad format\n");
				ret = -EINVAL;
				goto release_mem;
			}

			byte_offset = reg + i - config_start_offset;

			if ((byte_offset >= 0)
				&& (byte_offset <= config_mem_size)) {
				*(config_mem + byte_offset) = val;
			} else {
				dev_err(dev, "Bad object: reg:%d, T%d, ofs=%d\n",
					reg, object->type, byte_offset);
				ret = -EINVAL;
				goto release_mem;
			}

			data_pos += offset;
		}
	}

	/* calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < config_start_offset) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
				data->T7_address, config_start_offset);
		ret = 0;
		goto release_mem;
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_reg_init(&data->plug_in, config_mem, config_mem_size, cfg_start_ofs);
#endif
	/* Write configuration as blocks */
	byte_offset = 0;
	while (byte_offset < config_mem_size) {
		size = config_mem_size - byte_offset;

		if (size > MXT_MAX_BLOCK_WRITE)
			size = MXT_MAX_BLOCK_WRITE;

		ret = mxt_write_block(data->client,
				      config_start_offset + byte_offset,
				      size, config_mem + byte_offset);
		if (ret != 0) {
			dev_err(dev, "Config write error, ret=%d\n", ret);
			goto release_mem;
		}

		byte_offset += size;
	}

	ret = 1; /* tell the caller config has been sent */

release_mem:
	kfree(config_mem);
release:
	release_firmware(cfg);
	return ret;
}

static int mxt_chip_reset(struct mxt_data *data);

static int mxt_set_power_cfg(struct mxt_data *data, u8 mode)
{
	struct device *dev = &data->client->dev;
	int error = 0;
	int i, cnt;

	if (data->state != APPMODE) {
		dev_err(dev, "Not in APPMODE\n");
		return -EINVAL;
	}

	mxt_plugin_hook_set_t7(&data->plug_in, mode == MXT_POWER_CFG_DEEPSLEEP);

	switch (mode) {
	case MXT_POWER_CFG_DEEPSLEEP:
		/* Touch disable */
		cnt = ARRAY_SIZE(mxt_save);
		for (i = 0; i < cnt; i++) {
			if (mxt_get_object(data, mxt_save[i].suspend_obj) == NULL)
				continue;
			if (mxt_save[i].suspend_flags == MXT_SUSPEND_DYNAMIC)
				error |= mxt_read_object(data,
					mxt_save[i].suspend_obj,
					mxt_save[i].suspend_reg,
					&mxt_save[i].restore_val);
				error |= mxt_write_object(data,
					mxt_save[i].suspend_obj,
					mxt_save[i].suspend_reg,
					mxt_save[i].suspend_val);
			if (error) {
				error = mxt_chip_reset(data);
				if (error)
					dev_err(dev, "Failed to do chip reset!\n");
				break;
			}
		}
		break;

	case MXT_POWER_CFG_RUN:
	default:
		/* Touch enable */
		cnt =  ARRAY_SIZE(mxt_save);
		while (cnt--) {
			if (mxt_get_object(data, mxt_save[cnt].suspend_obj) == NULL)
				continue;
			error |= mxt_write_object(data,
						mxt_save[cnt].suspend_obj,
						mxt_save[cnt].suspend_reg,
						mxt_save[cnt].restore_val);
			if (error) {
				error = mxt_chip_reset(data);
				if (error)
					dev_err(dev, "Failed to do chip reset!\n");
				break;
			}
		}
		break;
	}

	if (error)
		goto i2c_error;

	data->is_stopped = (mode == MXT_POWER_CFG_DEEPSLEEP) ? 1 : 0;

	return 0;

i2c_error:
	dev_err(dev, "Failed to set power cfg\n");
	return error;
}

static int mxt_read_power_cfg(struct mxt_data *data, u8 *actv_cycle_time,
				u8 *idle_cycle_time, u8 *actv2idle_timeout)
{
	int error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7,
				MXT_POWER_ACTVACQINT,
				actv_cycle_time);
	if (error)
		return error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7,
				MXT_POWER_IDLEACQINT,
				idle_cycle_time);
	if (error)
		return error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7,
				MXT_POWER_ACTV2IDLETO,
				actv2idle_timeout);
	if (error)
		return error;

	return 0;
}

static int mxt_check_power_cfg_post_reset(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	error = mxt_read_power_cfg(data, &data->actv_cycle_time,
				   &data->idle_cycle_time,
				   &data->actv2idle_timeout);
	if (error)
		return error;

	/* Power config is zero, select free run */
	if (data->actv_cycle_time == 0 || data->idle_cycle_time == 0) {
		dev_dbg(dev, "Overriding power cfg to free run\n");
		data->actv_cycle_time = 255;
		data->idle_cycle_time = 255;

		error = mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
		if (error)
			return error;
	}

	return 0;
}

static int mxt_probe_power_cfg(struct mxt_data *data)
{
	int error;

	data->slowscan_actv_cycle_time = 120;   /* 120mS */
	data->slowscan_idle_cycle_time = 10;    /* 10mS */
	data->slowscan_actv2idle_timeout = 100; /* 10 seconds */

	error = mxt_read_power_cfg(data, &data->actv_cycle_time,
				   &data->idle_cycle_time,
				   &data->actv2idle_timeout);
	if (error)
		return error;

	/* If in deep sleep mode, attempt reset */
	if (data->actv_cycle_time == 0 || data->idle_cycle_time == 0) {
		error = mxt_soft_reset(data, MXT_RESET_VALUE);
		if (error)
			return error;

		error = mxt_check_power_cfg_post_reset(data);
		if (error)
			return error;
	}

	return 0;
}

static int mxt_read_internal_gpio(struct mxt_data *data)
{
	int ret;
	int timeout = 1000;
	int i = 0;
	bool is_timeout = true;
	struct device *dev = &data->client->dev;
	u8 val;

	ret = mxt_write_object(data, MXT_SPT_GPIOPWM_T19,
				MXT_GPIOPWM_INTPULLUP, 0xFF);
	if (ret) {
		dev_err(dev, "Failed to enable internal pull-up resistor!\n");
		return ret;
	}

	ret = mxt_write_object(data, MXT_SPT_GPIOPWM_T19,
				MXT_GPIOPWM_CTRL, MXT_GPIO_FORCERPT);
	if (ret) {
		dev_err(dev, "Failed to write force report to GPIO !\n");
		return ret;
	}

	while(i < timeout) {
		ret = mxt_read_object(data, MXT_SPT_GPIOPWM_T19,
					MXT_GPIOPWM_CTRL, &val);
		if (ret) {
			dev_err(dev, "Failed to read GPIO ctrl status !\n");
			return ret;
		}

		if ((val & MXT_GPIO_FORCERPT) == 0x3) {
			is_timeout = false;
			break;
		}
		i ++;
		msleep(1);
	}

	if (is_timeout) {
		dev_err(dev, "polling forcerpt bit timeout !\n");
		return -ETIMEDOUT;
	}

	mxt_read_t9_messages_until_invalid(data);

	ret = mxt_write_object(data, MXT_SPT_GPIOPWM_T19,
				MXT_GPIOPWM_INTPULLUP, 0);
	if (ret) {
		dev_err(dev, "Failed to disable internal pull-up resistor!\n");
		return ret;
	}

	return 0;
}

static const char * mxt_get_config(struct mxt_data *data, bool is_default)
{
	const struct mxt_platform_data *pdata = data->pdata;
	int i;

	for (i = 0; i < pdata->config_array_size; i++) {
		if (data->info.family_id== pdata->config_array[i].family_id &&
			data->info.variant_id == pdata->config_array[i].variant_id &&
			data->info.version == pdata->config_array[i].version &&
			data->info.build == pdata->config_array[i].build) {
			if (!is_default) {
				if (data->vendor_id == pdata->config_array[i].vendor_id) {
					data->current_index = i;
					return  pdata->config_array[i].mxt_cfg_name;
				}
			} else {
				data->current_index = i;
				return  pdata->config_array[i].mxt_cfg_name;
			}
		}
	}

	return NULL;
}

static int mxt_backup_nv(struct mxt_data *data)
{
	int error;
	u8 command_register;
	int timeout_counter = 0;

	/* Backup to memory */
	mxt_write_object(data, MXT_GEN_COMMAND_T6,
			MXT_COMMAND_BACKUPNV,
			MXT_BACKUP_VALUE);
	msleep(MXT_BACKUP_TIME);

	do {
		error = mxt_read_object(data, MXT_GEN_COMMAND_T6,
					MXT_COMMAND_BACKUPNV,
					&command_register);
		if (error)
			return error;

		msleep(20);

	} while ((command_register != 0) && (++timeout_counter <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "No response after backup!\n");
		return -EIO;
	}

	/* Soft reset */
	error = mxt_soft_reset(data, MXT_RESET_VALUE);
	if (error) {
		dev_err(&data->client->dev, "Failed to do reset!\n");
		return error;
	}

	return 0;
}

static int mxt_check_reg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	const char* config_name = NULL;
	bool is_recheck = false;

start:
	ret = mxt_read_internal_gpio(data);
	if (ret) {
		dev_err(dev, "Can not get internal gpio status, just give default one!\n");
		is_recheck = true;
	} else {
		config_name = mxt_get_config(data, false);
		is_recheck = false;
	}

	if (config_name == NULL) {
		dev_info(dev, "Not found matched config!\n");
		return -ENOENT;
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_force_stop(&data->plug_in);
	mxt_plugin_deinit(&data->plug_in);
#endif
	ret = mxt_download_config(data, config_name);
	if (ret < 0)
		return ret;
	else if (ret == 0)
		/* CRC matched, or no config file, or config parse failure
		 * - no need to reset */
		return 0;

	/* Backup to memory */
	ret = mxt_backup_nv(data);
	if (ret) {
		dev_err(dev, "back nv failed!\n");
		return ret;
	}

	if (is_recheck)
		goto start;

	ret = mxt_check_power_cfg_post_reset(data);
	if (ret)
		return ret;

	return 0;
}

static int mxt_read_info_block_crc(struct mxt_data *data)
{
	int ret;
	u16 offset;
	u8 buf[3];

	offset = MXT_OBJECT_START + MXT_OBJECT_SIZE * data->info.object_num;

	ret = mxt_read_reg(data->client, offset, sizeof(buf), buf);
	if (ret)
		return ret;

	data->info_block_crc = (buf[2] << 16) | (buf[1] << 8) | buf[0];

	return 0;
}

static int mxt_get_object_table(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u16 end_address;
	u8 reportid = 0;
	u8 buf[data->info.object_num][MXT_OBJECT_SIZE];
	data->mem_size = 0;

	data->object_table = kcalloc(data->info.object_num,
				     sizeof(struct mxt_object), GFP_KERNEL);
	if (!data->object_table) {
		dev_err(dev, "Failed to allocate object table\n");
		return -ENOMEM;
	}

	ret = mxt_read_reg(client, MXT_OBJECT_START, sizeof(buf), buf);
	if (ret)
		goto free_object_table;

	for (i = 0; i < data->info.object_num; i++) {
		struct mxt_object *object = data->object_table + i;

		object->type = buf[i][0];
		object->start_address = (buf[i][2] << 8) | buf[i][1];
		object->size = buf[i][3] + 1;
		object->instances = buf[i][4] + 1;
		object->num_report_ids = buf[i][5];

		if (object->num_report_ids) {
			reportid += object->num_report_ids * object->instances;
			object->max_reportid = reportid;
			object->min_reportid = object->max_reportid -
				object->instances * object->num_report_ids + 1;
		}

		end_address = object->start_address
			+ object->size * object->instances - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;

		/* save data for objects used when processing interrupts */
		switch (object->type) {
		case MXT_TOUCH_MULTI_T9:
			data->T9_reportid_max = object->max_reportid;
			data->T9_reportid_min = object->min_reportid;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			data->T9_address = object->start_address;
#endif
			data->num_touchids = object->num_report_ids * object->instances;
			break;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = object->max_reportid;
			data->T6_address = object->start_address;
			break;
		case MXT_GEN_MESSAGE_T5:
			if (data->info.family_id == 0x80) {
				/* On mXT224 must read and discard CRC byte
				 * otherwise DMA reads are misaligned */
				data->T5_msg_size = object->size;
			} else {
				/* CRC not enabled, therefore don't read last byte */
				data->T5_msg_size = object->size - 1;
			}
			data->T5_address = object->start_address;
			break;
		case MXT_GEN_POWER_T7:
			data->T7_address = object->start_address;
			break;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		case MXT_GEN_ACQUIRE_T8:
			data->T8_address = object->start_address;
			break;
#endif
		case MXT_TOUCH_KEYARRAY_T15:
			data->T15_reportid_max = object->max_reportid;
			data->T15_reportid_min = object->min_reportid;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			data->T15_address = object->start_address;
#endif
			break;
		case MXT_SPT_GPIOPWM_T19:
			data->T19_reportid_max = object->max_reportid;
			data->T19_reportid_min = object->min_reportid;
			break;
		case MXT_SPT_SELFTEST_T25:
			data->T25_reportid_max = object->max_reportid;
			data->T25_reportid_min = object->min_reportid;
			break;
		case MXT_DEBUG_DIAGNOSTIC_T37:
			data->T37_address = object->start_address;
			break;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		case MXT_SPT_USERDATA_T38:
			data->T38_address = object->start_address;
			break;
		case MXT_PROCI_GRIP_T40:
			data->T40_address = object->start_address;
			break;
#endif
		case MXT_PROCI_TOUCHSUPPRESSION_T42:
			data->T42_reportid_max = object->max_reportid;
			data->T42_reportid_min = object->min_reportid;
			break;
		case MXT_SPT_MESSAGECOUNT_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_SPT_NOISESUPPRESSION_T48:
			data->T48_reportid = object->max_reportid;
			break;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		case MXT_SPT_ADAPTIVE_THRESHOLD_T55:
			data->T55_address = object->start_address;
			break;
		case MXT_SPT_TIMER_T61:
			data->T61_address = object->start_address;
			data->T61_reportid_min = object->min_reportid;
			data->T61_reportid_max = object->max_reportid;
			data->T61_instances = object->instances;
			break;
#endif
		case MXT_PROCI_ACTIVE_STYLUS_T63:
			data->T63_reportid_max = object->max_reportid;
			data->T63_reportid_min = object->min_reportid;
			data->num_stylusids =
				object->num_report_ids * object->instances;
			break;
		case MXT_SPT_GOLDENREF_T66:
			data->T66_reportid = object->max_reportid;
			break;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		case MXT_PROCI_LENSBENDING_T65:
			data->T65_address = object->start_address;
			break;
		case MXT_PROCG_NOISESUPPRESSION_T72:
			data->T72_address = object->start_address;
			data->T72_reported_min = object->min_reportid;
			data->T72_reportid_max = object->max_reportid;
			break;
		case MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80:
			data->T80_address = object->start_address;
			break;
#endif
		case MXT_TOUCH_MULTI_T100:
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			data->T100_address = object->start_address;
#endif
			data->T100_reportid_max = object->max_reportid;
			data->T100_reportid_min = object->min_reportid;
			data->num_touchids = object->num_report_ids * object->instances;
			break;
		}

		dev_dbg(dev, "T%u, start:%u size:%u instances:%u "
			"min_reportid:%u max_reportid:%u\n",
			object->type, object->start_address, object->size,
			object->instances,
			object->min_reportid, object->max_reportid);
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	/* If T44 exists, T9 position has to be directly after */
	if (data->T44_address && (data->T5_address != data->T44_address + 1)) {
		dev_err(dev, "Invalid T44 position\n");
		ret = -EINVAL;
		goto free_object_table;
	}

	/* Allocate message buffer */
	data->msg_buf = kcalloc(data->max_reportid, data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(dev, "Failed to allocate message buffer\n");
		ret = -ENOMEM;
		goto free_object_table;
	}

	return 0;

free_object_table:
	kfree(data->object_table);
	return ret;
}

static int mxt_read_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	unsigned int x_range, y_range;
	unsigned char orient;
	unsigned char val;

	/* Update matrix size in info struct */
	error = mxt_read_reg(client, MXT_MATRIX_X_SIZE, 1, &val);
	if (error)
		return error;
	data->info.matrix_xsize = val;

	error = mxt_read_reg(client, MXT_MATRIX_Y_SIZE, 1, &val);
	if (error)
		return error;
	data->info.matrix_ysize = val;

	if (mxt_get_object(data, MXT_TOUCH_MULTI_T100) != NULL) {
		/* Read X/Y size of touchscreen */
		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_XRANGE_MSB, &val);
		if (error)
			return error;
		x_range = val << 8;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_XRANGE_LSB, &val);
		if (error)
			return error;
		x_range |= val;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_YRANGE_MSB, &val);
		if (error)
			return error;
		y_range = val << 8;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_YRANGE_LSB, &val);
		if (error)
			return error;
		y_range |= val;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_CFG1, &val);
		if (error)
			return error;
		orient = (val & 0xE0) >> 5;
	} else {
		/* Read X/Y size of touchscreen */
		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T9,
				MXT_TOUCH_XRANGE_MSB, &val);
		if (error)
			return error;
		x_range = val << 8;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T9,
				MXT_TOUCH_XRANGE_LSB, &val);
		if (error)
			return error;
		x_range |= val;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T9,
				MXT_TOUCH_YRANGE_MSB, &val);
		if (error)
			return error;
		y_range = val << 8;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T9,
				MXT_TOUCH_YRANGE_LSB, &val);
		if (error)
			return error;
		y_range |= val;

		error =  mxt_read_object(data, MXT_TOUCH_MULTI_T9,
				MXT_TOUCH_ORIENT, &orient);
		if (error)
			return error;
	}

	dev_info(&client->dev, "xrange = %d, yrange = %d\n", x_range, y_range);
	/* Handle default values */
	if (x_range == 0)
		x_range = 1023;

	if (y_range == 0)
		y_range = 1023;

	if (orient & MXT_XY_SWITCH) {
		data->max_x = y_range;
		data->max_y = x_range;
	} else {
		data->max_x = x_range;
		data->max_y = y_range;
	}

	dev_info(&client->dev,
			"Matrix Size X%uY%u Touchscreen size X%uY%u\n",
			data->info.matrix_xsize, data->info.matrix_ysize,
			data->max_x, data->max_y);

	return 0;
}

static int mxt_initialize_regulator(struct mxt_data *data, bool on)
{
	int ret;
	struct i2c_client *client = data->client;

	/*
		Vdd and AVdd can be powered up in any order
		XVdd must not be powered up until after Vdd
		and must obey the rate-of-rise specification
	*/
	if (!on)
		goto err_disable_vdd;

	data->regulator_vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(data->regulator_vdd)) {
		dev_info(&client->dev,
			"Atmel regulator_get for vdd failed: %ld\n",
						PTR_ERR(data->regulator_vdd));
		goto err_null_regulator;
	}

	if (regulator_count_voltages(data->regulator_vdd) > 0) {
		ret = regulator_set_voltage(data->regulator_vdd, MXT_VTG_MIN_UV,
						MXT_VTG_MAX_UV);
		if (ret) {
			dev_err(&client->dev,
				"Regulator set_vtg failed vdd ret = %d\n", ret);
			goto err_put_vdd_regulator;
		}
	}

	dev_info(&client->dev,
		"Atmel regulator_get for vdd succeeded\n");

	ret = regulator_enable(data->regulator_vdd);
	if (ret < 0) {
		dev_err(&client->dev,
			"Atmel regulator_enable for vdd failed; Error code:%d\n", ret);
		goto err_vdd_set_vtg;
	}

	return 0;

err_disable_vdd:
	regulator_disable(data->regulator_vdd);
err_vdd_set_vtg:
	if (regulator_count_voltages(data->regulator_vdd) > 0)
		regulator_set_voltage(data->regulator_vdd, 0, MXT_VTG_MAX_UV);
err_put_vdd_regulator:
	devm_regulator_put(data->regulator_vdd);
err_null_regulator:
	data->regulator_vdd = NULL;

	return -EINVAL;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static int mxt_get_t38_flag(struct mxt_data *data)
{
	int error;
	u8 flag;

	error = mxt_read_object(data, MXT_SPT_USERDATA_T38,
					MXT_FW_UPDATE_FLAG, &flag);
	if (error)
		return error;

	data->update_flag = flag;

	return 0;
}

static int mxt_get_init_setting(struct mxt_data *data)
{
	int error;
	u8 glovectrl;
	u8 atchthr;
	struct device *dev = &data->client->dev;

	if (mxt_get_object(data, MXT_PROCI_GLOVEDETECTION_T78) != NULL) {
		error = mxt_read_object(data, MXT_PROCI_GLOVEDETECTION_T78,
						MXT_GLOVE_CTRL, &glovectrl);
		if (error) {
			dev_err(dev, "Failed to read glove setting from t78!\n");
			return error;
		}
		if ((glovectrl & 0x01) != 0)
			data->sensitive_mode = 1;
	}

	if (mxt_get_object(data, MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80) != NULL) {
		error = mxt_read_object(data, MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,
					MXT_RETRANS_ATCHTHR, &atchthr);
		if (error) {
			dev_err(dev, "Faield to read from t80 anti-touch threshold!\n");
			return error;
		}
		data->atchthr = atchthr;
	}

	error = mxt_read_resolution(data);
	if (error) {
		dev_err(dev, "Failed to initialize screen size\n");
		return error;
	}

	return 0;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_info *info = &data->info;
	int error;
	u8 retry_count = 0;

retry_probe:
	/* Read info block */
	error = mxt_read_reg(client, 0, sizeof(*info), info);
	if (error) {
		error = mxt_probe_bootloader(data);
		if (error) {
			/* Chip is not in appmode or bootloader mode */
			return error;
		} else {
			if (++retry_count > 10) {
				dev_err(&client->dev,
					"Could not recover device from "
					"bootloader mode\n");
				data->state = BOOTLOADER;
				/* this is not an error state, we can reflash
				 * from here */
				 error = mxt_update_fw_store(&client->dev, NULL,
						data->pdata->mxt_fw_name,
						strlen(data->pdata->mxt_fw_name));
				if (error != strlen(data->pdata->mxt_fw_name))
				{
					dev_err(&client->dev, "Error when update firmware!\n");
					return error;
				}
				return 0;
			}

			/* Tell bootloader to enter app mode. Ignore errors
			 * since we're in a retry loop */
			mxt_send_bootloader_cmd(data, false);
			msleep(MXT_FWRESET_TIME);
			goto retry_probe;
		}
	}

	dev_info(&client->dev,
		"Family ID: %d Variant ID: %d Version: %d.%d.%02X "
		"Object Num: %d\n",
		info->family_id, info->variant_id,
		info->version >> 4, info->version & 0xf,
		info->build, info->object_num);

	data->state = APPMODE;

	/* Get object table information */
	error = mxt_get_object_table(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading object table\n", error);
		return error;
	}

	error = mxt_get_t38_flag(data);
	if (error) {
		dev_err(&client->dev, "Error %d getting update flag\n", error);
		return error;
	}

	/* Read information block CRC */
	error = mxt_read_info_block_crc(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading info block CRC\n", error);
	}

	error = mxt_probe_power_cfg(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize power cfg\n");
		return error;
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	error = mxt_plugin_pre_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d plugin init\n", error);
		return error;
	}
#endif

	/* Check register init values */
	error = mxt_check_reg_init(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize config\n");
		return error;
	}

	if (mxt_get_object(data, MXT_TOUCH_MULTI_T100) != NULL)
	{
		error = mxt_read_object(data, MXT_TOUCH_MULTI_T100,
					MXT_MULTITOUCH_TCHAUX,
					&data->t100_tchaux_bits);
		if (error) {
			dev_err(&client->dev, "Failed to read tchaux!\n");
			return error;
		}
	}

	error = mxt_get_init_setting(data);
	if (error) {
		dev_err(&client->dev, "Failed to get init setting.\n");
		return error;
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	error = mxt_plugin_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d plugin init\n", error);
		return error;
	}
#endif

	return 0;
}

static int strtobyte(const char *data, u8 *value)
{
	char str[3];

	str[0] = data[0];
	str[1] = data[1];
	str[2] = '\0';

	return kstrtou8(str, 16, value);
}

static size_t mxt_convert_text_to_binary(u8 *buffer, size_t len)
{
	int ret;
	int i;
	int j = 0;

	for (i = 0; i < len; i+=2) {
		ret = strtobyte(&buffer[i], &buffer[j]);
		if (ret) {
			return -EINVAL;
		}
		j++;
	}

	return (size_t)j;
}

static int mxt_check_firmware_format(struct device *dev, const struct firmware *fw)
{
	unsigned int pos = 0;
	char c;

	while (pos < fw->size) {
		c = *(fw->data + pos);

		if (c < '0' || (c > '9' && c < 'A') || c > 'F')
			return 0;

		pos++;
	}

	/* To convert file try
	  * xxd -r -p mXTXXX__APP_VX-X-XX.enc > maxtouch.fw */
	dev_err(dev, "Aborting: firmware file must be in binary format\n");

	return -1;
}

static void mxt_reset_toggle(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	int i;

	for (i = 0; i < 10; i++) {
		gpio_set_value_cansleep(pdata->reset_gpio, 0);
		msleep(1);
		gpio_set_value_cansleep(pdata->reset_gpio, 1);
		msleep(60);
	}

	gpio_set_value_cansleep(pdata->reset_gpio, 1);
}

static int mxt_load_fw(struct device *dev, const char *fn)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame= 0;
	int ret;
	unsigned short ori_addr = data->client->addr;
	size_t len = 0;
	u8 *buffer;

	ret = request_firmware(&fw, fn, dev);
	if (ret < 0) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		return ret;
	}

	buffer = kmalloc(fw->size ,GFP_KERNEL);
	if (!buffer) {
		dev_err(dev, "malloc firmware buffer failed!\n");
		return -ENOMEM;
	}
	memcpy(buffer, fw->data, fw->size);
	len = fw->size;

	ret  = mxt_check_firmware_format(dev, fw);
	if (ret) {
		dev_info(dev, "text format, convert it to binary!\n");
		len = mxt_convert_text_to_binary(buffer, len);
		if (len <= 0)
			goto release_firmware;
	}


	if (data->state != BOOTLOADER) {
		/* Change to the bootloader mode */
		mxt_reset_toggle(data);

		ret = mxt_get_bootloader_address(data);
		if (ret)
			goto release_firmware;

		data->client->addr = data->bootloader_addr;
		data->state = BOOTLOADER;
	}

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD);
	if (ret) {
		mxt_wait_for_chg(data);
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}
	} else {
		dev_info(dev, "Unlocking bootloader\n");

		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}
	}

	while (pos < len) {
		mxt_wait_for_chg(data);
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}

		frame_size = ((*(buffer + pos) << 8) | *(buffer + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data,buffer + pos, frame_size);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}

		mxt_wait_for_chg(data);
		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS);

		if (ret) {
			retry ++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				data->state = FAILED;
				goto release_firmware;
			}
		} else {
				retry ++;
				pos += frame_size;
				frame ++;
		}

		if (frame % 10 == 0) {
			dev_info(dev, "Updated %d frames, %d/%zd bytes\n", frame, pos, len);
		}
	}

	dev_info(dev, "Finished, sent %d frames, %zd bytes\n", frame, pos);

	data->state = INIT;

release_firmware:
	data->client->addr = ori_addr;
	release_firmware(fw);
	kfree(buffer);
	return ret;
}

static ssize_t mxt_update_fw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	ssize_t count = sprintf(buf,
			"family_id=0x%02x, variant_id=0x%02x, version=0x%02x, build=0x%02x, vendor=0x%02x\n",
			data->info.family_id, data->info.variant_id,
			data->info.version, data->info.build,
			data->vendor_id);
	return count;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	char *fw_name;
	int len = 0;

	if (count <= 0)
		return -EINVAL;

	len = strnlen(buf, count);
	fw_name = kmalloc(len + 1, GFP_KERNEL);
	if (fw_name == NULL)
		return -ENOMEM;

	if (count > 0) {
		strncpy(fw_name, buf, len);
		if (fw_name[len - 1] == '\n')
			fw_name[len - 1] = 0;
		else
			fw_name[len] = 0;
	}

	dev_info(dev, "Identify firmware name :%s \n", fw_name);
	disable_irq(data->irq);

	error = mxt_load_fw(dev, fw_name);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		/* Wait for reset */
		msleep(MXT_FWRESET_TIME);

		kfree(data->object_table);
		data->object_table = NULL;
		kfree(data->msg_buf);
		data->msg_buf = NULL;

		mxt_initialize(data);
	}

	if (data->state == APPMODE) {
		enable_irq(data->irq);
	}

	kfree(fw_name);
	return count;
}

static ssize_t mxt_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf + count, "%d", data->info.version);
	count += sprintf(buf + count, "\n");

	return count;
}

static ssize_t mxt_build_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf + count, "%d", data->info.build);
	count += sprintf(buf + count, "\n");

	return count;
}

static ssize_t mxt_pause_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count;
	char c;

	c = data->driver_paused ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_pause_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->driver_paused = (i == 1);
		dev_dbg(dev, "%s\n", i ? "paused" : "unpaused");
		return count;
	} else {
		dev_dbg(dev, "pause_driver write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	char c;

	c = data->debug_enabled ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
				       size_t *count)
{
	if (data->state != APPMODE) {
		dev_err(&data->client->dev, "Not in APPMODE\n");
		return -EINVAL;
	}

	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_slowscan_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;
	int error;
	u8 actv_cycle_time;
	u8 idle_cycle_time;
	u8 actv2idle_timeout;
	dev_info(dev, "Calling mxt_slowscan_show()\n");

	error = mxt_read_object(data, MXT_GEN_POWER_T7,
		MXT_POWER_ACTVACQINT,
		&actv_cycle_time);

	if (error)
		return error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7,
		MXT_POWER_IDLEACQINT,
		&idle_cycle_time);

	if (error)
		return error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7,
		MXT_POWER_ACTV2IDLETO,
		&actv2idle_timeout);

	if (error)
		return error;

	count += sprintf(buf + count,
			"SLOW SCAN (enable/disable) = %s.\n",
			data->slowscan_enabled ? "enabled" : "disabled");
	count += sprintf(buf + count,
			"SLOW SCAN (actv_cycle_time) = %umS.\n",
			data->slowscan_actv_cycle_time);
	count += sprintf(buf + count,
			"SLOW SCAN (idle_cycle_time) = %umS.\n",
			data->slowscan_idle_cycle_time);
	count += sprintf(buf + count,
			"SLOW SCAN (actv2idle_timeout) = %u.%0uS.\n",
			data->slowscan_actv2idle_timeout / 10,
			data->slowscan_actv2idle_timeout % 10);
	count += sprintf(buf + count,
			"CURRENT   (actv_cycle_time) = %umS.\n",
			actv_cycle_time);
	count += sprintf(buf + count,
			"CURRENT   (idle_cycle_time) = %umS.\n",
			idle_cycle_time);
	count += sprintf(buf + count,
			"CURRENT   (actv2idle_timeout) = %u.%0uS.\n",
			actv2idle_timeout / 10, actv2idle_timeout % 10);

	return count;
}

static ssize_t mxt_slowscan_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int fn;
	int val;
	int ret;

	dev_info(dev, "Calling mxt_slowscan_store()\n");
	ret = sscanf(buf, "%u %u", &fn, &val);
	if ((ret == 1) || (ret == 2)) {
		switch (fn) {
		case SLOSCAN_DISABLE:
			if (data->slowscan_enabled) {
				data->actv_cycle_time =
					data->slowscan_shad_actv_cycle_time;
				data->idle_cycle_time =
					data->slowscan_shad_idle_cycle_time;
				data->actv2idle_timeout =
					data->slowscan_shad_actv2idle_timeout;
				data->slowscan_enabled = 0;
				mxt_set_power_cfg(data, 0);
			}
			break;

		case SLOSCAN_ENABLE:
			if (!data->slowscan_enabled) {
				data->slowscan_shad_actv_cycle_time =
					data->actv_cycle_time;
				data->slowscan_shad_idle_cycle_time =
					data->idle_cycle_time;
				data->slowscan_shad_actv2idle_timeout =
					data->actv2idle_timeout;
				data->actv_cycle_time =
					data->slowscan_actv_cycle_time;
				data->idle_cycle_time =
					data->slowscan_idle_cycle_time;
				data->actv2idle_timeout =
					data->slowscan_actv2idle_timeout;
				data->slowscan_enabled = 1;
				mxt_set_power_cfg(data, 0);
			}
			break;

		case SLOSCAN_SET_ACTVACQINT:
			data->slowscan_actv_cycle_time = val;
			break;

		case SLOSCAN_SET_IDLEACQINT:
			data->slowscan_idle_cycle_time = val;
			break;

		case SLOSCAN_SET_ACTV2IDLETO:
			data->slowscan_actv2idle_timeout = val;
			break;
		}
	}
	return count;
}

static void mxt_do_calibration(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error, i;
	u8 val;
	int time_out = 100;

	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_CALIBRATE, 1);
	if (error) {
		dev_err(dev, "failed to do calibration!\n");
		return;
	}

	for (i = 0; i < time_out; i++) {
		error = mxt_read_object(data, MXT_GEN_COMMAND_T6,
					MXT_COMMAND_CALIBRATE, &val);
		if (error) {
			dev_err(dev, "failed to read calibration!\n");
			return;
		}

		if (val == 0)
			break;
		msleep(10);
	}
}

static ssize_t mxt_update_fw_flag_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;
	int i;

	if (sscanf(buf, "%u", &i) == 1)  {
		dev_dbg(dev, "write fw update flag %d to t38\n", i);
		ret = mxt_write_object(data, MXT_SPT_USERDATA_T38,
					MXT_FW_UPDATE_FLAG, (u8)i);
		if (ret < 0)
			return ret;
		ret = mxt_backup_nv(data);
		if (ret)
			return ret;
	}

	return count;
}

static ssize_t mxt_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%02x, %02x, %02x, %02x, %02x, %02x\n",
			data->test_result[0], data->test_result[1],
			data->test_result[2], data->test_result[3],
			data->test_result[4], data->test_result[5]);
}

static ssize_t mxt_selftest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	u8 selftest_cmd;

	/* run all selftest */
	error = mxt_write_object(data,
			MXT_SPT_SELFTEST_T25,
			0x01, 0xfe);
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

	return error ? : count;
}

static int mxt_get_diag_data(struct mxt_data *data, char *buf)
{
	struct device *dev = &data->client->dev;
	int error;
	int read_size = 0;
	u16 addr = data->T37_address;

	error = mxt_do_diagnostic(data, data->diag_mode);
	if (error) {
		dev_err(dev, "do diagnostic 0x%02x failed!\n", data->diag_mode);
		return error;
	}

	while (read_size < MXT_DIAG_TOTAL_SIZE) {
		error = mxt_read_reg(data->client, addr + 2,
					MXT_DIAG_PAGE_SIZE, buf + read_size);
		if (error) {
			dev_err(dev, "Read from T37 failed!\n");
			return error;
		}

		read_size += MXT_DIAG_PAGE_SIZE;

		error = mxt_do_diagnostic(data, MXT_DIAG_PAGE_UP);
		if (error) {
			dev_err(dev, "do diagnostic 0x%02x failed!\n", MXT_DIAG_PAGE_UP);
			return error;
		}
	}

	if (data->debug_enabled)
		print_hex_dump(KERN_DEBUG, "Data: ", DUMP_PREFIX_NONE, 16, 1,
				       buf, MXT_DIAG_TOTAL_SIZE, false);

	return 0;
}

static ssize_t mxt_diagnostic_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	int i = 0;
	int len = 0;
	int remain_size, transfer_size;
	int row_size = 16;
	int group_size = 1;
	char *tmp_buffer = kmalloc(MXT_DIAG_TOTAL_SIZE, GFP_KERNEL);
	if (tmp_buffer == NULL)
		return -ENOMEM;

	error = mxt_get_diag_data(data, tmp_buffer);
	if (error) {
		kfree(tmp_buffer);
		return error;
	}

	remain_size = MXT_DIAG_TOTAL_SIZE % row_size;
	transfer_size = MXT_DIAG_TOTAL_SIZE - remain_size;
	while (i  < transfer_size) {
		hex_dump_to_buffer(tmp_buffer + i, row_size, row_size, group_size,
					buf + len, PAGE_SIZE - len, false);
		i += row_size;
		len = strlen(buf);
		buf[len] = '\n';
		len ++;
	}

	if (remain_size != 0)
		hex_dump_to_buffer(tmp_buffer + i, remain_size, row_size, group_size,
					buf + len, PAGE_SIZE - len, false);

	kfree(tmp_buffer);
	return strlen(buf);
}

static ssize_t mxt_diagnostic_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;
	u8 mode;

	if (sscanf(buf, "%u", &i) == 1)  {
		mode = (u8)i;
		dev_info(dev, "Diag mode = 0x%02x\n", mode);
		data->diag_mode = mode;
	}

	return count;
}

static int mxt_chip_reset(struct mxt_data *data)
{
	int error;

	disable_irq(data->client->irq);
	if (gpio_is_valid(data->pdata->power_gpio))
		gpio_set_value(data->pdata->power_gpio, 0);
	else
		mxt_initialize_regulator(data, false);
	msleep(20);
	if (gpio_is_valid(data->pdata->power_gpio))
		gpio_set_value(data->pdata->power_gpio, 1);
	else
		mxt_initialize_regulator(data, true);
	msleep(10);
	mxt_wait_for_chg(data);
	enable_irq(data->client->irq);
	error = mxt_soft_reset(data, MXT_RESET_VALUE);
	if (error)
		return error;

	error = mxt_initialize(data);

	return error;
}

static ssize_t mxt_chip_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error;
	struct mxt_data *data = dev_get_drvdata(dev);

	error = mxt_chip_reset(data);
	if (error)
		return error;
	else
		return count;
}

static ssize_t mxt_chg_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	int chg_state;

	chg_state = gpio_get_value(data->pdata->irq_gpio);
	count = sprintf(buf, "%d\n", chg_state);

	return count;
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
		ret = mxt_read_reg(data->client, off, count, buf);

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
		ret = mxt_write_block(data->client, off, count, buf);

	return ret == 0 ? count : 0;
}

static int mxt_execute_golden_command(struct mxt_data *data, u8 cmd)
{
	struct device *dev = &data->client->dev;
	int error;
	u8 ori_val = MXT_GOLDCTRL_ENABLE | MXT_GOLDCTRL_REPEN;
	ori_val &= ~MXT_GOLD_CMD_MASK;
	ori_val |= cmd;
	error = mxt_write_object(data, MXT_SPT_GOLDENREF_T66,
                                MXT_GOLDENREF_CTRL, ori_val);
	pr_info("write command = 0x%x\n", ori_val);
	if (error) {
		dev_err(dev, "Write object t66 error, when execute cmd 0x%x\n", cmd);
		return error;
	}

	return 0;
}

static int mxt_do_prepare_golden(struct mxt_data *data, bool dualx_on)
{
	struct device *dev = &data->client->dev;
	int error;
	int i;
	u8 t8_close [] = {0xFF, 0x01, 0x00, 0x00};

	/* first close T8 anti-calib */
	for (i = 0; i < sizeof(t8_close); i++) {
		error = mxt_write_object(data, MXT_GEN_ACQUIRE_T8,
				MXT_ACQUIRE_ATCHCALST+i, t8_close[i]);
		if (error) {
			dev_err(dev, "Failed to close anti-calib\n");
			return error;
		}
	}

	/* close the t66 en*/
	error = mxt_write_object(data, MXT_SPT_GOLDENREF_T66,
					MXT_GOLDENREF_CTRL, 0);
	if (error) {
		dev_err(dev, "failed to disable t66!\n");
		return error;
	}

	if (dualx_on) {
		error = mxt_write_object(data, MXT_PROCG_NOISESUPPRESSION_T72,
					MXT_NOISESUP_CALCFG, MXT_NOICFG_VNOISY);
		if (error) {
			dev_err(dev, "failed to enter very noisy mode!\n");
			return error;
		}
	}

	/* calib after t66 disabled, make sure the surface is flat */
	mxt_do_calibration(data);

        return 0;
}

static int mxt_check_golden_status(struct mxt_data *data, bool dualx_on)
{
	struct device *dev = &data->client->dev;
	int error;
	int i = 0;
	int time_out = 1000;

	if (dualx_on) {
		error = mxt_write_object(data, MXT_PROCG_NOISESUPPRESSION_T72,
					MXT_NOISESUP_CALCFG, MXT_NOICFG_VNOISY);
		if (error) {
			dev_err(dev, "failed to enter very noisy mode!\n");
			return error;
		}
	}

	error = mxt_write_object(data, MXT_SPT_GOLDENREF_T66,MXT_GOLDENREF_CTRL,
				MXT_GOLDCTRL_ENABLE | MXT_GOLDCTRL_REPEN);
	if (error) {
		dev_err(dev, "failed to enable t66\n");
		return error;
	}

	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
                                MXT_COMMAND_REPORTALL, 1);
	if (error) {
		dev_err(dev, "failed to report all msg!\n");
		return error;
	}

	data->golden_msg.status = MXT_GOLDSTATE_INVALID;
	while (1) {
		if (data->golden_msg.status == MXT_GOLDSTATE_INVALID) {
			if(i++ >= time_out) {
				error = -ETIMEDOUT;
				return error;
			}
			msleep(10);
			continue;
		} else
			break;
		data->golden_msg.status = MXT_GOLDSTATE_INVALID;
	}

	if (data->golden_msg.status & MXT_GOLDSTS_BADSTOREDATA)
		return -EINVAL;

	return 0;
}

static int mxt_do_golden_ref_tune(struct mxt_data *data, bool dualx_on)
{
	struct device *dev = &data->client->dev;
	bool is_sequence_done = false;
	int error;
	int j, k;
	u8 status;
	int retry_times = 5;
	int time_out = 1000;

	error = mxt_do_prepare_golden(data, dualx_on);
	if (error)
		return error;

	msleep(100);
	data->golden_msg.status = MXT_GOLDSTATE_INVALID;
	error = mxt_execute_golden_command(data, MXT_GOLDCMD_PRIME);
	if (error) {
		dev_err(dev, "execute command 1 failed\n");
		return error;
	}

	j = 0;
	k = 0;
	while (!is_sequence_done) {
		if (data->golden_msg.status == MXT_GOLDSTATE_INVALID) {
			if(k++ >= time_out) {
				error = -ETIMEDOUT;
				return error;
			}
			msleep(10);
			continue;
		}
		k = 0;
		pr_info("data->golden_msg.status = 0x%x\n", data->golden_msg.status);
		status = data->golden_msg.status & MXT_GOLD_STATE_MASK;

		if (data->golden_msg.status & MXT_GOLDSTS_FCALSEQERR) {
					error = mxt_execute_golden_command(data, MXT_GOLDCMD_NONE);
					if (error) {
						dev_err(dev, "execute command 0 failed\n");
						return error;
					}
					msleep(100);
					error = mxt_execute_golden_command(data, MXT_GOLDCMD_PRIME);
					if (error) {
						dev_err(dev, "execute command 1 failed\n");
						return error;
					}
		} else if (data->golden_msg.status == MXT_GOLDSTATE_IDLE) {
					error = mxt_execute_golden_command(data, MXT_GOLDCMD_NONE);
					if (error) {
						dev_err(dev, "execute command 0 failed\n");
						return error;
					}
					is_sequence_done = true;
		} else if (status == MXT_GOLDSTATE_PRIME) {
			error = mxt_execute_golden_command(data, MXT_GOLDCMD_GENERATE);
			if (error) {
				dev_err(dev, "execute command 2 failed\n");
				return error;
			}
		} else if (status == MXT_GOLDSTATE_GEN) {
			if (data->golden_msg.status == MXT_GOLDSTATE_GEN_PASS) {
				error = mxt_execute_golden_command(data, MXT_GOLDCMD_CONFIRM);
				if (error) {
					dev_err(dev, "execute command 2 failed\n");
					return error;
				}
			} else  if(data->golden_msg.status == MXT_GOLDSTATE_GEN_FAIL) {
				dev_info(dev, "max_diff = 0x%x, max_diffx = 0x%x, max_diffy = 0x%x\n",
					data->golden_msg.fcalmaxdiff,
					data->golden_msg.fcalmaxdiffx,
					data->golden_msg.fcalmaxdiffy);
				error = mxt_execute_golden_command(data, MXT_GOLDCMD_NONE);
				if (error) {
					dev_err(dev, "execute command 0 failed\n");
					return error;
				}
				return -EINVAL;
			}
		} else if (j++ == retry_times) {
				dev_err(dev, "Generate golden reference failed!\n");
				/* disable the T66 */
				error = mxt_write_object(data, MXT_SPT_GOLDENREF_T66,
						MXT_GOLDENREF_CTRL, 0x0);
				if (error)
					dev_err(dev, "Disable T66 failed!\n");

				return -ETIMEDOUT;
		}
		data->golden_msg.status = MXT_GOLDSTATE_INVALID;
	}

	error = mxt_soft_reset(data, MXT_RESET_VALUE);
	if (error) {
		dev_err(dev, "Failed when reset!\n");
		return error;
	}

	error = mxt_check_golden_status(data, dualx_on);
	if (error) {
		dev_err(dev, "Store golden ref is bad!\n");
		return error;
	}

	dev_info(dev, "Run golden ref generating successfully!\n");
	return 0;
}

static ssize_t mxt_golden_ref_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;

	count = sprintf(buf, "%d\n", data->golden_ok);

	return count;
}

#define DELTA_TYPE_MULT		0x1
#define REF_TYPE_MULT		0x2
#define MONITOR_HAS_RISK		0x0
#define MONITOR_NO_RISK		0x1
static int mxt_monitor_delta_no_calib_risk(struct mxt_data *data, u8 type)
{
	struct device *dev = &data->client->dev;
	int error, i;
	u16 addr = data->T37_address;
	short val, no_touch_threshold, prev_val;
	u8 *buf;
	size_t buf_size;
	u8 mode;
	const struct mxt_platform_data *pdata = data->pdata;
	int index = data->current_index;
	int result = MONITOR_NO_RISK;
	int read_size = 0;
	int count = 0, row = 0;
	short *ref_buf;

	if (type == DELTA_TYPE_MULT) {
		buf_size = MXT_DIAG_TOTAL_SIZE;
		mode = MXT_DIAG_MULT_DELTA;
		no_touch_threshold = pdata->config_array[index].mult_no_touch_threshold;
	} else if (type == REF_TYPE_MULT) {
		buf_size = MXT_DIAG_TOTAL_SIZE;
		mode = MXT_DIAG_MULT_REF;
		ref_buf = (short*)kmalloc(sizeof(short) * (buf_size >> 1), GFP_KERNEL);
		if (ref_buf == NULL)
			return MONITOR_NO_RISK;
	} else
		return MONITOR_NO_RISK;

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (buf == NULL) {
		dev_err(dev, "Failed to alloc buffer for delta getting!\n");
		return MONITOR_NO_RISK;
	}

	error = mxt_do_diagnostic(data, mode);
	if (error) {
		dev_err(dev, "Failed to send get diag data cmd!\n");
		result = false;
		goto end;
	}

	while (read_size < buf_size) {
		error = mxt_read_reg(data->client, addr + 2,
					MXT_DIAG_PAGE_SIZE, buf + read_size);
		if (error) {
			dev_err(dev, "Read from T37 failed!\n");
			return error;
		}

		read_size += MXT_DIAG_PAGE_SIZE;

		error = mxt_do_diagnostic(data, MXT_DIAG_PAGE_UP);
		if (error) {
			dev_err(dev, "do diagnostic 0x%02x failed!\n", MXT_DIAG_PAGE_UP);
			return error;
		}
	}

	for (i = 0; i < buf_size; i+= 2) {
		val = (buf[i+1] << 8) | buf[i];
		if (type == DELTA_TYPE_MULT) {
			if (val > no_touch_threshold || val < -(no_touch_threshold))
				result = MONITOR_HAS_RISK;
		} else if (type == REF_TYPE_MULT) {
			ref_buf[row * pdata->rx_num + count] = val;
			count ++;
			if (data->debug_enabled) {
				if (count == 1)
					printk(KERN_CONT "#### ");
				printk(KERN_CONT "%5d ", val);
			}

			if (count == pdata->rx_num) {
				if (data->debug_enabled)
					printk(KERN_CONT "\n");
				count = 0;
				row ++;
			}
			prev_val = val;
		}
	}

end:
	if (type == REF_TYPE_MULT)
		kfree(ref_buf);

	kfree(buf);
	return result;
}

#define TOTAL_TEST_NUM	3
static bool mxt_is_golden_ref_good(struct mxt_data *data, bool dualx_on)
{
	int i;
	struct device *dev = &data->client->dev;

	if (dualx_on) {
		int error;
		error = mxt_write_object(data, MXT_PROCG_NOISESUPPRESSION_T72,
					MXT_NOISESUP_CALCFG, MXT_NOICFG_VNOISY);
		if (error) {
			dev_err(dev, "failed to enter very noisy mode!\n");
			return false;
		}
        }

	msleep(100);

	for (i = 0; i < TOTAL_TEST_NUM; i++) {
		if (mxt_monitor_delta_no_calib_risk(data, DELTA_TYPE_MULT) !=
						MONITOR_NO_RISK)
			return false;
			msleep(1000);
	}

	return true;
}

static ssize_t mxt_golden_ref_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	int val;

	data->golden_ok = 0;
	sscanf(buf, "%u", &val);
	mutex_lock(&data->input_dev->mutex);
	if (val == 1) {
		dev_info(dev, "Call mxt_golden_ref_store\n");
		if (mxt_get_object(data, MXT_SPT_GOLDENREF_T66) == NULL) {
			dev_err(dev, "No such object, ignore!\n");
			mutex_unlock(&data->input_dev->mutex);
			return 0;
		}

		error = mxt_soft_reset(data, MXT_RESET_VALUE);
		if (error)
			goto end;
		error = mxt_do_golden_ref_tune(data, false);
		if (error)
			goto end;
		if (!mxt_is_golden_ref_good(data, false)) {
			dev_err(dev, "non-dual-x detect failed!\n");
			goto end;
		}

		error = mxt_do_golden_ref_tune(data, true);
		if (error)
			goto end;
		if (mxt_is_golden_ref_good(data, true))
			data->golden_ok = 1;
		else
			dev_err(dev, "dual-x detect failed!\n");
	} else {
		mutex_unlock(&data->input_dev->mutex);
		return -EINVAL;
	}

end:
	mutex_unlock(&data->input_dev->mutex);
	error = mxt_soft_reset(data, MXT_RESET_VALUE);
	if (error)
                return error;
	mxt_do_calibration(data);
	return count;
}

static DEVICE_ATTR(update_fw, S_IWUSR | S_IRUSR, mxt_update_fw_show, mxt_update_fw_store);
static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR, mxt_debug_enable_show,
			mxt_debug_enable_store);
static DEVICE_ATTR(pause_driver, S_IWUSR | S_IRUSR, mxt_pause_show,
			mxt_pause_store);
static DEVICE_ATTR(version, S_IRUGO, mxt_version_show, NULL);
static DEVICE_ATTR(build, S_IRUGO, mxt_build_show, NULL);
static DEVICE_ATTR(slowscan_enable, S_IWUSR | S_IRUSR,
			mxt_slowscan_show, mxt_slowscan_store);
static DEVICE_ATTR(update_fw_flag, S_IWUSR, NULL, mxt_update_fw_flag_store);
static DEVICE_ATTR(selftest,  S_IWUSR | S_IRUSR, mxt_selftest_show, mxt_selftest_store);
static DEVICE_ATTR(diagnostic, S_IWUSR | S_IRUSR, mxt_diagnostic_show, mxt_diagnostic_store);
static DEVICE_ATTR(chip_reset, S_IWUSR, NULL, mxt_chip_reset_store);
static DEVICE_ATTR(chg_state, S_IRUGO, mxt_chg_state_show, NULL);
static DEVICE_ATTR(golden_ref, S_IWUSR | S_IRUSR, mxt_golden_ref_show, mxt_golden_ref_store);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static DEVICE_ATTR(plugin, S_IWUSR | S_IRUSR, mxt_plugin_show, mxt_plugin_store);
static DEVICE_ATTR(plugin_tag, S_IWUSR | S_IRUSR, mxt_plugin_tag_show, NULL);
#endif

static struct attribute *mxt_attrs[] = {
	&dev_attr_update_fw.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_pause_driver.attr,
	&dev_attr_version.attr,
	&dev_attr_build.attr,
	&dev_attr_slowscan_enable.attr,
	&dev_attr_update_fw_flag.attr,
	&dev_attr_selftest.attr,
	&dev_attr_diagnostic.attr,
	&dev_attr_chip_reset.attr,
	&dev_attr_chg_state.attr,
	&dev_attr_golden_ref.attr,
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	&dev_attr_plugin.attr,
	&dev_attr_plugin_tag.attr,
#endif
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static void mxt_start(struct mxt_data *data, bool resume)
{
	int error;
	struct device *dev = &data->client->dev;

	if (data->is_stopped == 0)
		return;

	error = mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
	if (error)
		return;

	/* At this point, it may be necessary to clear state
	 * by disabling/re-enabling the noise suppression object */

	/* Recalibrate since chip has been in deep sleep */
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	if (mxt_plugin_cal_t37_check_and_calibrate(&data->plug_in, false) != 0)
#endif
		mxt_do_calibration(data);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_start(&data->plug_in, resume);
#endif

	data->is_stopped = false;
	dev_dbg(dev, "MXT started\n");
}

static void mxt_stop(struct mxt_data *data, bool suspend)
{
	int error;
	struct device *dev = &data->client->dev;

	if (data->is_stopped)
		return;

	error = mxt_set_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);

	if (!error)
		dev_dbg(dev, "MXT is_stopped\n");

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_stop(&data->plug_in, suspend);
#endif
	data->is_stopped = true;
}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	mxt_start(data, false);

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	board_pulse_irq_thread();
#endif
	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	mxt_stop(data, false);
}

static void mxt_clear_touch_event(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	int index = data->current_index;
	int id, i;
	static int tracking_id = -2;

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_reset_slots(&data->plug_in);
#endif

	for (id = 0; id < data->num_touchids; id++) {
		input_mt_slot(input_dev, id);
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, tracking_id--);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
		if (tracking_id >= 0)
			tracking_id = -2;
	}
	for (i = 0; i < data->pdata->config_array[index].key_num; i++)
		clear_bit(data->pdata->config_array[index].key_codes[i], input_dev->key);

	input_sync(input_dev);
}

static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	disable_irq(client->irq);

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		mxt_stop(data, true);

	mutex_unlock(&input_dev->mutex);

	mxt_clear_touch_event(data);

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	enable_irq(client->irq);

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		mxt_start(data, true);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int mxt_input_enable(struct input_dev *in_dev)
{
	int error = 0;
	struct mxt_data *ts = input_get_drvdata(in_dev);

	error = mxt_resume(&ts->client->dev);
	if (error)
		dev_err(&ts->client->dev, "%s: failed\n", __func__);

	return error;
}

static int mxt_input_disable(struct input_dev *in_dev)
{
	int error = 0;
	struct mxt_data *ts = input_get_drvdata(in_dev);

	error = mxt_suspend(&ts->client->dev);
	if (error)
		dev_err(&ts->client->dev, "%s: failed\n", __func__);

	return error;
}

static int mxt_initialize_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int ret;
	int i;
	int index = data->current_index;
	bool t100_exists;

	/* Initialize input device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	if (data->pdata->input_name) {
		input_dev->name = data->pdata->input_name;
	} else {
		input_dev->name = "atmel-maxtouch";
	}

	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;
	input_dev->enable = mxt_input_enable;
	input_dev->disable = mxt_input_disable;
	input_dev->enabled = true;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	if (mxt_get_object(data, MXT_TOUCH_MULTI_T100) != NULL)
		t100_exists = true;

	/* For multi touch */
	input_mt_init_slots(input_dev,
		data->num_touchids + data->num_stylusids);
	if (t100_exists) {
		if (data->t100_tchaux_bits & MXT_T100_AREA) {
			dev_info(dev, "report area\n");
			input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
					     0, MXT_MAX_AREA, 0, 0);
		}
	} else
		input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
					0, MXT_MAX_AREA, 0, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);

	if (t100_exists) {
		if (data->t100_tchaux_bits &  MXT_T100_AMPL) {
			dev_info(dev, "report pressure\n");
			input_set_abs_params(input_dev, ABS_MT_PRESSURE,
					     0, 255, 0, 0);
		}
		if (data->t100_tchaux_bits &  MXT_T100_VECT) {
			dev_info(dev, "report vect\n");
			input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
				     0, 255, 0, 0);
		}
	} else {
		input_set_abs_params(input_dev, ABS_MT_PRESSURE,
					0, 255, 0, 0);
		input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
					0, 255, 0, 0);
	}

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		__set_bit(BTN_STYLUS, input_dev->keybit);
		__set_bit(BTN_STYLUS2, input_dev->keybit);

		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For T15 key array */
	if (data->pdata->config_array[index].key_codes) {
		for (i = 0; i < data->pdata->config_array[index].key_num; i++) {
			if (data->pdata->config_array[index].key_codes[i])
				input_set_capability(input_dev, EV_KEY,
							data->pdata->config_array[index].key_codes[i]);
		}
	}

	input_set_drvdata(input_dev, data);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(dev, "Error %d registering input device\n", ret);
		input_free_device(input_dev);
		return ret;
	}

	data->input_dev = input_dev;

	return 0;
}

static struct dentry *debug_base;

static int mxt_debugfs_object_show(struct seq_file *m, void *v)
{
	struct mxt_data *data = m->private;
	struct mxt_object *object;
	struct device *dev = &data->client->dev;
	int i, j, k;
	int error;
	int obj_size;
	u8 val;

	seq_printf(m,
		  "Family ID: %02X Variant ID: %02X Version: %d.%d Build: 0x%02X"
		  "\nObject Num: %dMatrix X Size: %d Matrix Y Size: %d\n",
		   data->info.family_id, data->info.variant_id,
		   data->info.version >> 4, data->info.version & 0xf,
		   data->info.build, data->info.object_num,
		   data->info.matrix_xsize, data->info.matrix_ysize);

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		obj_size = object->size + 1;

		for (j = 0; j < object->instances; j++) {
			seq_printf(m, "Type %d NumId %d MaxId %d\n",
				   object->type, object->num_report_ids,
				   object->max_reportid);

			for (k = 0; k < obj_size; k++) {
				error = mxt_read_object(data, object->type,
							j * obj_size + k, &val);
				if (error) {
					dev_err(dev,
						"Failed to read object %d "
						"instance %d at offset %d\n",
						object->type, j, k);
					return error;
				}

				seq_printf(m, "%02x ", val);
				if (k % 10 == 9 || k + 1 == obj_size)
					seq_printf(m, "\n");
			}
		}
	}

	return 0;
}

static ssize_t mxt_debugfs_object_store(struct file *file,
			const char __user *buf, size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mxt_data *data = m->private;
	u8 type, offset, val;
	int error;

	if (sscanf(buf, "%hhu:%hhu=%hhx", &type, &offset, &val) == 3) {
		error = mxt_write_object(data, type, offset, val);
		if (error)
			count = error;
	} else
		count = -EINVAL;

	return count;
}

static int mxt_debugfs_object_open(struct inode *inode, struct file *file)
{
	return single_open(file, mxt_debugfs_object_show, inode->i_private);
}

static const struct file_operations mxt_object_fops = {
	.owner		= THIS_MODULE,
	.open		= mxt_debugfs_object_open,
	.read		= seq_read,
	.write		= mxt_debugfs_object_store,
	.release	= single_release,
};

static void __devinit mxt_debugfs_init(struct mxt_data *data)
{
	debug_base = debugfs_create_dir(MXT_DEBUGFS_DIR, NULL);
	if (IS_ERR_OR_NULL(debug_base))
		pr_err("atmel_mxt_ts: Failed to create debugfs dir\n");
	if (IS_ERR_OR_NULL(debugfs_create_file(MXT_DEBUGFS_FILE,
					       0444,
					       debug_base,
					       data,
					       &mxt_object_fops))) {
		pr_err("atmel_mxt_ts: Failed to create object file\n");
		debugfs_remove_recursive(debug_base);
	}
}

static void mxt_dump_value(struct device *dev, struct mxt_platform_data *pdata)
{
	int i = 0;

	dev_info(dev, "reset gpio= %d\n", pdata->reset_gpio);
	dev_info(dev, "irq gpio= %d\n", pdata->irq_gpio);
	dev_info(dev, "fw name = %s\n", pdata->mxt_fw_name);
	dev_info(dev, "config size = %d\n", pdata->config_array_size);
	dev_info(dev, "gpio mask = 0x%x\n", pdata->gpio_mask);

	for (i = 0; i < pdata->config_array_size; i++) {
		dev_info(dev, "family_id = 0x%x\n", pdata->config_array[i].family_id);
		dev_info(dev, "variant_id = 0x%x\n", pdata->config_array[i].variant_id);
		dev_info(dev, "version = 0x%x\n", pdata->config_array[i].version);
		dev_info(dev, "build = 0x%x\n", pdata->config_array[i].build);
		dev_info(dev, "mxt_cfg_name = %s\n", pdata->config_array[i].mxt_cfg_name);
		dev_info(dev, "vendor_id = 0x%x\n", pdata->config_array[i].vendor_id);
	}
}

#ifdef CONFIG_OF
static int mxt_parse_dt(struct device *dev, struct mxt_platform_data *pdata)
{
	int ret;
	struct mxt_config_info *info;
	struct device_node *temp, *np = dev->of_node;
	u32 temp_val;

	pdata->i2c_pull_up = of_property_read_bool(np, "atmel,i2c-pull-up");

	/* reset, irq, power gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "atmel,reset-gpio",
				0, &pdata->reset_gpio_flags);
	pdata->irq_gpio = of_get_named_gpio_flags(np, "atmel,irq-gpio",
				0, &pdata->irq_gpio_flags);
	pdata->power_gpio = of_get_named_gpio_flags(np, "atmel,power-gpio",
				0, &pdata->power_gpio_flags);

	ret = of_property_read_u32(np, "atmel,irqflags", &temp_val);
	if (ret) {
		dev_err(dev, "Unable to read irqflags id\n");
		return ret;
	} else
		pdata->irqflags = temp_val;

	ret = of_property_read_string(np, "atmel,mxt-fw-name",
			&pdata->mxt_fw_name);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
		return ret;
	}

	ret = of_property_read_u32(np, "atmel,gpio-mask", (u32*)&temp_val);
	if (ret)
		dev_err(dev, "Unable to read gpio mask\n");
	else
		pdata->gpio_mask = (u8)temp_val;

	ret = of_property_read_u32(np, "atmel,rx-num", (u32*)&temp_val);
	if (ret) {
		dev_err(dev, "Unable to get rx number.\n");
		return ret;
	} else
		pdata->rx_num = (u8)temp_val;

	ret = of_property_read_u32(np, "atmel,tx-num", (u32*)&temp_val);
	if (ret) {
		dev_err(dev, "Unable to get tx number.\n");
		return ret;
	} else
		pdata->tx_num = (u8)temp_val;

	ret = of_property_read_u32(np, "atmel,config-array-size", &pdata->config_array_size);
	if (ret) {
		dev_err(dev, "Unable to get array size\n");
		return ret;
	}

	pdata->config_array = devm_kzalloc(dev, pdata->config_array_size *
					sizeof(struct mxt_config_info), GFP_KERNEL);
	if (!pdata->config_array) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	info = pdata->config_array;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_u32(temp, "atmel,family-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read family id\n");
			return ret;
		} else
			info->family_id = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,variant-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read variant id\n");
			return ret;
		} else
			info->variant_id = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,version", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read version\n");
			return ret;
		} else
			info->version = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,build", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read build\n");
			return ret;
		} else
			info->build = (u8)temp_val;
		ret = of_property_read_string(temp, "atmel,mxt-cfg-name",
			&info->mxt_cfg_name);
		if (ret && (ret != -EINVAL)) {
			dev_err(dev, "Unable to read cfg name\n");
			return ret;
		}
		ret = of_property_read_u32(temp, "atmel,vendor-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read vendor id\n");
			return ret;
		} else
			info->vendor_id = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,mult-no-touch-threshold", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read mult-no-touch-threshold\n");
			return ret;
		} else
			info->mult_no_touch_threshold = temp_val;
		ret = of_property_read_u32(temp, "atmel,key-num", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read key num id\n");
			return ret;
		} else
			info->key_num = temp_val;
		if (info->key_num != 0) {
			info->key_codes = devm_kzalloc(dev,
						sizeof(int) * info->key_num, GFP_KERNEL);
			if (!info->key_codes)
				return -ENOMEM;
			ret = of_property_read_u32_array(temp, "atmel,key-codes",
							info->key_codes, info->key_num);
			if (ret) {
				dev_err(dev, "Unable to read key codes\n");
				return ret;
			}
		}

		info++;
	}

	mxt_dump_value(dev, pdata);

	return 0;
}
#else
static int mxt_parse_dt(struct device *dev, struct mxt_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int __devinit mxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mxt_platform_data *pdata;
	struct mxt_data *data;
	int error;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct mxt_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		error = mxt_parse_dt(&client->dev, pdata);
		if (error)
			return error;
	} else
		pdata = client->dev.platform_data;

	if (!pdata)
		return -EINVAL;

	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	data->state = INIT;

	data->client = client;
	data->pdata = pdata;
	data->irq = client->irq;

	if (gpio_is_valid(pdata->power_gpio)) {
		error = gpio_request(pdata->power_gpio, "mxt_gpio_power");
		if (!error) {
			error = gpio_direction_output(pdata->power_gpio, 1);
			if (error) {
				pr_err("%s: unable to set direction gpio %d\n",
					__func__, pdata->power_gpio);
				goto err_free_power_gpio;
			}
		} else {
			pr_err("%s: unable to request power gpio %d\n",
				__func__, pdata->power_gpio);
			goto err_free_data;
		}
	} else
		error = mxt_initialize_regulator(data, true);
		if (error)
			goto err_free_data;

	if (gpio_is_valid(pdata->irq_gpio)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->irq_gpio, "mxt_irq_gpio");
		if (error) {
			pr_err("%s: unable to request gpio [%d]\n", __func__,
						pdata->irq_gpio);
			goto err_disable_regulator;
		}
		error = gpio_direction_input(pdata->irq_gpio);
		if (error) {
			pr_err("%s: unable to set_direction for gpio [%d]\n",
					__func__, pdata->irq_gpio);
			goto err_irq_gpio_req;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->reset_gpio, "mxt_reset_gpio");
		if (error) {
			pr_err("%s: unable to request reset gpio %d\n",
				__func__, pdata->reset_gpio);
			goto err_irq_gpio_req;
		}

		error = gpio_direction_output(pdata->reset_gpio, 1);
		if (error) {
			pr_err("%s: unable to set direction for gpio %d\n",
				__func__, pdata->reset_gpio);
			goto err_reset_gpio_req;
		}
	}

	i2c_set_clientdata(data->client, data);
	mdelay(10);
	mxt_wait_for_chg(data);

	/* Initialize i2c device */
	error = mxt_initialize(data);
	if (error)
	{
		pr_err("reset gpio = %d\n", (int)gpio_get_value(pdata->reset_gpio));
		pr_err("chg gpio = %d\n", (int)gpio_get_value(pdata->irq_gpio));

		if (error != -ENOENT)
			goto err_reset_gpio_req;
		else {
			if (0) {
				error = mxt_update_fw_store(&client->dev, NULL,
							pdata->mxt_fw_name, strlen(pdata->mxt_fw_name));
				if (error != strlen(pdata->mxt_fw_name))
				{
					dev_err(&client->dev, "Error when update firmware!\n");
					goto err_disable_regulator;
				}
			}
		}
	}

	error = mxt_initialize_input_device(data);
	if (error)
		goto err_free_object;

#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
	init_waitqueue_head(&data->wait);
	data->irq_tsk = kthread_run(mxt_process_message_thread, data, "Atmel_mxt_ts");
	if (!data->irq_tsk) {
		dev_err(&client->dev, "Error %d can't create irq thread\n", error);
		error = -ESRCH;
		goto err_free_input_device;
	}
#if !defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	error = request_irq(client->irq, mxt_interrupt_pulse_workqueue,
			pdata->irqflags,
			client->dev.driver->name, data);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_irq_workqueue;
	}
#endif
#else
	error = request_threaded_irq(client->irq, NULL, mxt_interrupt,
			pdata->irqflags, client->dev.driver->name, data);
	if (error) {
		dev_err(&client->dev, "Error %d registering irq\n", error);
		goto err_free_input_device;
	}
#endif

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		goto err_free_irq;
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

	mxt_debugfs_init(data);

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	mxt_g_data = data;
#endif

	return 0;

err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_free_irq:
	free_irq(client->irq, data);
#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
#if !defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
err_free_irq_workqueue:
#endif
	kthread_stop(data->irq_tsk);
#endif
err_free_input_device:
	input_unregister_device(data->input_dev);
err_free_object:
	kfree(data->msg_buf);
	kfree(data->object_table);
err_reset_gpio_req:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
err_irq_gpio_req:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_disable_regulator:
	if (gpio_is_valid(pdata->power_gpio))
		gpio_set_value_cansleep(pdata->power_gpio, 0);
	else
		mxt_initialize_regulator(data, false);
err_free_power_gpio:
	if (gpio_is_valid(pdata->power_gpio)) {
		gpio_free(pdata->power_gpio);
	}
err_free_data:
	kfree(data);
	return error;
}

static int __devexit mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);
	const struct mxt_platform_data *pdata = data->pdata;

	sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data->msg_buf);
	data->msg_buf = NULL;
	kfree(data->object_table);
	data->object_table = NULL;
	if (gpio_is_valid(pdata->power_gpio))
		gpio_set_value_cansleep(pdata->power_gpio, 0);
	else
		mxt_initialize_regulator(data, false);

	if (gpio_is_valid(pdata->power_gpio))
		gpio_free(pdata->power_gpio);

	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free (pdata->irq_gpio);

	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);

	kfree(data);
	data = NULL;

	return 0;
}

static void mxt_shutdown(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	disable_irq(data->irq);
	data->state = SHUTDOWN;
}

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "mXT224", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
#include "plug.c"
#endif

#ifdef CONFIG_OF
static struct of_device_id mxt_match_table[] = {
	{ .compatible = "atmel,mxt-ts",},
	{ },
};
#else
#define mxt_match_table NULL
#endif

static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= "atmel_mxt_ts",
		.owner	= THIS_MODULE,
		.of_match_table = mxt_match_table,
	},
	.probe		= mxt_probe,
	.remove		= __devexit_p(mxt_remove),
	.shutdown	= mxt_shutdown,
	.id_table	= mxt_id,
};

static int __init mxt_init(void)
{
	return i2c_add_driver(&mxt_driver);
}

static void __exit mxt_exit(void)
{
	i2c_del_driver(&mxt_driver);
}

late_initcall(mxt_init);
module_exit(mxt_exit);

/* Module information */
MODULE_AUTHOR("Lionel Zhang <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");

