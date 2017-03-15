/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011 Atmel Corporation
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
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include "atmel_mxt_ts.h"
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

static u8 TP_Maker, LCD_Maker, Panel_Ink;
static u8 Fw_Version[3];
u8 Panel_ID;

char *config_name_buff;


/* Version */
#define MXT_VER_20		20
#define MXT_VER_21		21
#define MXT_VER_22		22

/* Firmware files */
#define MXT_FW_NAME		"maxtouch.fw"
#define MXT_CFG_MAGIC		"OBP_RAW V1"

#define REV_D	0x32

/* Regulator voltages */
#define MXT_VDDIO_MIN_UV	2800000
#define MXT_VDDIO_MAX_UV	2800000
#define MXT_VDD_MIN_UV		2800000
#define MXT_VDD_MAX_UV		2800000

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

#define MXT_MAX_BLOCK_WRITE	255

/* BIST attr size */
#define MXT_REF_ATTR_SIZE	(2 * PAGE_SIZE)

/* Channels */
#define MXT_336T_CHANNELS_X	24
#define MXT_336T_CHANNELS_Y	14
#define MXT_336T_MAX_NODES	(MXT_336T_CHANNELS_X * MXT_336T_CHANNELS_Y)
#define MXT_336T_X_SELFREF_RESERVED	2
#define MXT_874U_CHANNELS_X	38
#define MXT_874U_CHANNELS_Y	23
#define MXT_874U_MAX_NODES	(MXT_874U_CHANNELS_X * MXT_874U_CHANNELS_Y)
#define MXT_874U_X_SELFREF_RESERVED	4
#define MXT_308U_CHANNELS_X	14
#define MXT_308U_CHANNELS_Y	24
#define MXT_308U_MAX_NODES	(MXT_308U_CHANNELS_X * MXT_308U_CHANNELS_Y)
#define MXT_308U_X_SELFREF_RESERVED	2

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
#define MXT_TOUCH_EDGE_POINT_T35	35
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ACTIVE_STYLUS_T63	63
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_PROCI_STYLUS_T47		47
#define MXT_SPT_NOISESUPPRESSION_T48	48
#define MXT_SPT_TIMER_T61		61
#define MXT_PROCI_LENSBENDING_T65	65
#define MXT_SPT_GOLDENREF_T66		66
#define MXT_PDS_INFO_T68		68
#define MXT_SPT_DYMCFG_T70		70
#define MXT_SPT_DYMDATA_T71		71
#define MXT_PROCG_NOISESUPPRESSION_T72	72
#define MXT_PROCI_GLOVEDETECTION_T78	78
#define MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80	80
#define MXT_TOUCH_MORE_GESTURE_T81	81
#define MXT_TOUCH_GESTURE_T92		92
#define MXT_TOUCH_SEQUENCE_LOGGER_T93	93
#define MXT_TOUCH_MULTI_T100		100
#define MXT_SPT_TOUCHSCREENHOVER_T101	101
#define MXT_PROCG_NOISESUPSELFCAP_T108	108
#define MXT_SPT_SELFCAPGLOBALCONFIG_T109	109
#define MXT_SPT_AUXTOUCHCONFIG_T104	104
#define MXT_SPT_SELFCAPCONFIG_T111	111
#define MXT_TOUCH_KEYARRAY_T97		97
#define MXT_TOUCH_EDGE_GESTURE_T220	220

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
#define MXT_POWER_CFG_WAKEUP_GESTURE	2

/* MXT_GEN_ACQUIRE_T8 field */
#define MXT_ACQUIRE_CHRGTIME	0
#define MXT_ACQUIRE_TCHDRIFT	2
#define MXT_ACQUIRE_DRIFTST	3
#define MXT_ACQUIRE_TCHAUTOCAL	4
#define MXT_ACQUIRE_SYNC	5
#define MXT_ACQUIRE_ATCHCALST	6
#define MXT_ACQUIRE_ATCHCALSTHR	7
#define MXT_ACQUIRE_MEASALLOW	10

/* MXT_SPT_USERDATA_T38 field */
#define MXT_CFG_VERSION	0


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

/* MXT_SPT_CTECONFIG_T46 field */
#define MXT_CTECONFIG_ADCSPERSYNC	4

/* MXT_SPT_SELFCAPCONFIG_T111 field */
#define MXT_SELF_ADCSPERSYNC_INST0	25
#define MXT_SELF_ADCSPERSYNC_INST1	55
#define MXT_SELF_ADCSPERSYNC_INST2	85

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
#define MXT_GPIO_DISABLEOUTPUT		0

/* MXT_SPT_CTECONFIG_T28 field */
#define MXT_CTE_CTRL		0
#define MXT_CTE_CMD		1
#define MXT_CTE_MODE		2
#define MXT_CTE_IDLEGCAFDEPTH	3
#define MXT_CTE_ACTVGCAFDEPTH	4
#define MXT_CTE_VOLTAGE		5

#define MXT_VOLTAGE_DEFAULT	2700000
#define MXT_VOLTAGE_STEP	10000

#define MXT_KEY_SIZE 3
/* MXT_DEBUG_DIAGNOSTIC_T37 */
#define MXT_DIAG_PAGE_UP	0x01
#define MXT_DIAG_MUTUAL_DELTA	0x10
#define MXT_DIAG_MUTUAL_REF	0x11
#define MXT_DIAG_KEY_DELTA	0x17
#define MXT_DIAG_KEY_REF	0x18
#define MXT_DIAG_SELF_DELTA	0xF7
#define MXT_DIAG_SELF_REF	0xF8
#define MXT_DIAG_PAGE_SIZE	0x80
#define MXT_DIAG_TOTAL_SIZE	0x438
#define MXT_DIAG_SELF_SIZE	0x6C
#define MXT_DIAG_REV_ID		21
#define MXT_LOCKDOWN_OFFSET	4

/* MXT_SPT_USERDATA_T38 */
#define MXT_FW_UPDATE_FLAG	0
#define MXT_CONFIG_INFO_SIZE	8

/* MXT_PROCI_STYLUS_T47 */
#define MXT_PSTYLUS_CTRL	0

/* MXT_SPT_TIMER_T61 */
#define MXT_TIMER_PERIODLSB	3
#define MXT_TIMER_PERIODMSB	4

/* MXT_PROCI_LENSBENDING_T65 */
#define MXT_LENSBENDING_CTRL	0

/* MXT_PDS_INFO_T68 */
#define MXT_LOCKDOWN_SIZE	8

/* MXT_PROCG_NOISESUPPRESSION_T72 */
#define MXT_NOISESUP_CTRL	0
#define MXT_NOISESUP_CALCFG	1
#define MXT_NOISESUP_CFG1	2

/* MXT_PROCI_GLOVEDETECTION_T78 */
#define MXT_GLOVE_CTRL		0x00

/* MXT_TOUCH_KEYARRAY_T97 */
#define MXT_TOUCH_KEYARRAY_INST0_CTRL	0
#define MXT_TOUCH_KEYARRAY_INST1_CTRL	10
#define MXT_TOUCH_KEYARRAY_INST2_CTRL	20

/* MXT_SPT_TOUCHSCREENHOVER_T101 */
#define MXT_HOVER_CTRL		0x00

/* MXT_SPT_AUXTOUCHCONFIG_T104 */
#define MXT_AUXTCHCFG_XTCHTHR	2
#define MXT_AUXTCHCFG_INTTHRX	4
#define MXT_AUXTCHCFG_YTCHTHR	7
#define MXT_AUXTCHCFG_INTTHRY	9

/* MXT_SPT_SELFCAPGLOBALCONFIG_T109 */
#define MXT_SELFCAPCFG_CTRL	0
#define MXT_SELFCAPCFG_CMD	3


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
#define MXT_RESET_BOOTLOADER	0xA5
#define MXT_BACKUP_VALUE	0x55

/* Define for MXT_PROCG_NOISESUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* Delay times */
#define MXT_BACKUP_TIME		25	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_NOCHGREAD	600	/* msec */
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

/* T15 KeyArray */
#define MXT_KEY_RPTEN		(1 << 1)
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

/* T81 gesture */
#define MXT_GESTURE_CTRL	0x0

/* T72 noise suppression */
#define MXT_NOICTRL_ENABLE	(1 << 0)
#define MXT_NOICFG_VNOISY	(1 << 1)
#define MXT_NOICFG_NOISY	(1 << 0)

/* T93 double tap */
#define MXT_DBL_TAP_CTRL	0x0

/* T109 self-cap */
#define MXT_SELFCTL_RPTEN	0x2
#define MXT_SELFCMD_TUNE	0x1
#define MXT_SELFCMD_STM_TUNE	0x2
#define MXT_SELFCMD_AFN_TUNE	0x3
#define MXT_SELFCMD_STCR_TUNE	0x4
#define MXT_SELFCMD_AFCR_TUNE	0x5
#define MXT_SELFCMD_AFNVMSTCR_TUNE	0x6
#define MXT_SELFCMD_RCR_TUNE	0x7

#define MXT_DEBUGFS_DIR		"atmel_mxt_ts"
#define MXT_DEBUGFS_FILE		"object"


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

#define MXT_MAX_FINGER_NUM	16
#define BOOTLOADER_1664_1188	1

#define LOCKDOW_TPMAKER 0
#define LOCKDOW_DISPMAKER 1
#define LOCKDOW_INKCOLOR 2
#define LOCKDOW_HWVERSION 3
#define LOCKDOW_PROJIDH 4
#define LOCKDOW_PROJIDL 5
#define LOCKDOW_CGMAKER 6
#define LOCKDOW_RESERVE 7



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
	u8 wakeup_gesture_val;
	u8 suspend_flags;
	u8 restore_val;
};

struct mxt_golden_msg {
	u8 status;
	u8 fcalmaxdiff;
	u8 fcalmaxdiffx;
	u8 fcalmaxdiffy;
};


struct mxt_selfcap_status {
	u8 cmd;
	u8 error_code;
};

struct mxt_mode_switch {
	struct mxt_data *data;
	u8 mode;
	struct work_struct switch_mode_work;
};

enum mxt_edge_mode {
	EDGE_DISABLE = 0,
	EDGE_FINGER,
	EDGE_HANDGRIP,
	EDGE_FINGER_HANDGRIP
};

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	bool fw_updated;

	const  struct mxt_platform_data *pdata;
	enum mxt_device_state state;
	struct mxt_object *object_table;
	struct regulator *regulator_vdd;
	struct regulator *regulator_vddio;
	u16 mem_size;
	struct mxt_info info;
	unsigned int irq;
	unsigned int max_x;
	unsigned int max_y;
	struct bin_attribute mem_access_attr;
	struct bin_attribute self_ref_attr;
	struct bin_attribute mutual_ref_attr;
	bool debug_enabled;
	bool driver_paused;
	bool irq_enabled;
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
	u8 panel_id;
	u8 rev_id;
	int current_index;
	u8 update_flag;
	u8 test_result[6];
	int touch_num;
	u8 diag_mode;
	u8 atchthr;
	u8 sensitive_mode;
	u8 stylus_mode;
	u8 wakeup_gesture_mode;
	bool is_wakeup_by_gesture;
	int hover_tune_status;
	struct delayed_work calibration_delayed_work;
	struct delayed_work resume_delayed_work;
	u8 adcperx_normal[10];
	u8 adcperx_wakeup[10];
	bool firmware_updated;
	u8 lockdown_info[MXT_LOCKDOWN_SIZE];
	u8 config_info[MXT_CONFIG_INFO_SIZE];
	char *raw_ref_buf;
	s16 *raw_key_delta;

	/* Slowscan parameters	*/
	int slowscan_enabled;
	u8 slowscan_actv_cycle_time;
	u8 slowscan_idle_cycle_time;
	u8 slowscan_actv2idle_timeout;
	u8 slowscan_shad_actv_cycle_time;
	u8 slowscan_shad_idle_cycle_time;
	u8 slowscan_shad_actv2idle_timeout;
	struct mxt_golden_msg golden_msg;
	struct mxt_selfcap_status selfcap_status;
	bool finger_down[MXT_MAX_FINGER_NUM];

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T5_msg_size;
	u8 T6_reportid;
	u16 T7_address;
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T15_reportid_min;
	u8 T15_reportid_max;
	u8 T19_reportid_min;
	u8 T19_reportid_max;
	u8 T25_reportid_min;
	u8 T25_reportid_max;
	u16 T37_address;
	u8 T42_reportid_min;
	u8 T42_reportid_max;
	u16 T44_address;
	u8 T48_reportid;
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u8 T66_reportid;
	u8 T81_reportid_min;
	u8 T81_reportid_max;
	u8 T92_reportid_min;
	u8 T92_reportid_max;
	u8 T93_reportid_min;
	u8 T93_reportid_max;
	u8 T97_reportid_min;
	u8 T97_reportid_max;
	u8 T100_reportid_min;
	u8 T100_reportid_max;
	u8 T109_reportid;
	u8 T220_reportid_min;
	u8 T220_reportid_max;

	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;

#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
};

static struct mxt_suspend mxt_save[] = {
	{MXT_GEN_POWER_T7, MXT_POWER_IDLEACQINT,
		MXT_T7_IDLEACQ_DISABLE, 75, MXT_SUSPEND_DYNAMIC, 0},
	{MXT_GEN_POWER_T7, MXT_POWER_ACTVACQINT,
		MXT_T7_ACTVACQ_DISABLE, 35, MXT_SUSPEND_DYNAMIC, 0},
	{MXT_GEN_POWER_T7, MXT_POWER_ACTV2IDLETO,
		MXT_T7_ACTV2IDLE_DISABLE, 20, MXT_SUSPEND_DYNAMIC, 0}
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
#if CTP_PROC_INTERFACE
static u8 lockdown_info[MXT_LOCKDOWN_SIZE];
#define CTP_PARENT_PROC_NAME  "touchscreen"
#define CTP_LOCKDOWN_INFOR_NAME   "lockdown_info"
#define CTP_OPEN_PROC_NAME        "ctp_openshort_test"
extern  u8 tp_color;
struct mxt_data *cmcs_data;
static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos);
static const struct file_operations ctp_lockdown_procs_fops = {
	.write = ctp_lockdown_proc_write,
	.read = ctp_lockdown_proc_read,
	.owner = THIS_MODULE,
};

static ssize_t ctp_open_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos);
static const struct file_operations ctp_open_procs_fops = {
	.write = ctp_open_proc_write,
	.read = ctp_open_proc_read,
	.owner = THIS_MODULE,
};
#endif

static int mxt_read_key_delta(struct mxt_data *data);

static int mxt_initialize_input_device(struct mxt_data *data);

static BLOCKING_NOTIFIER_HEAD(glove_mode_chain);

int mxt_register_glove_mode_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&glove_mode_chain, nb);
}
EXPORT_SYMBOL(mxt_register_glove_mode_notifier);

int mxt_unregister_glove_mode_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&glove_mode_chain, nb);
}
EXPORT_SYMBOL(mxt_unregister_glove_mode_notifier);

#ifdef TOUCH_WAKEUP_EVENT_RECORD
#include <linux/fs.h>
#include <asm/fcntl.h>
#include <asm/uaccess.h>
#include <linux/rtc.h>

#define EVENT_TOUCH_WAKEUP	0
#define EVENT_SCREEN_ON		1
#define EVENT_SCREEN_OFF	2

static struct file *filp_record;
static const char *file_record = "/data/atmel_wakeup.log";
static unsigned int wakeup_count;
static atomic_t wakeup_flag = ATOMIC_INIT(0);

static void wakeup_event_record_init(void)
{
	filp_record = filp_open(file_record, O_WRONLY | O_CREAT | O_APPEND, 0600);
	if (IS_ERR(filp_record)) {
		pr_err("%s open failed\n", file_record);
		filp_record = NULL;
		return ;
	}
}

static void wakeup_event_record_exit(void)
{
	if (filp_record) {
		filp_close(filp_record, NULL);
		filp_record = NULL;
	}
}

static void wakeup_event_record_write(int event_type)
{
	int ret = 0;
	unsigned char data[256] = {0x0,};
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	if (filp_record == NULL) {
		wakeup_event_record_init();
		pr_info("Touch wakeup record file created\n");
	}
	if (event_type == EVENT_TOUCH_WAKEUP) {
		wakeup_count++;
		sprintf(data, "Touch wakeup %d (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			wakeup_count, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	} else if (event_type == EVENT_SCREEN_ON) {
		sprintf(data, "Screen on (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	} else {
		sprintf(data, "Screen off (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	}

	if (filp_record == NULL) {
		pr_err("File is NULL\n");
		return;
	}
	ret = kernel_write(filp_record, data, strlen(data), filp_record->f_pos);
	if (ret < 0) {
		pr_err("%s write failed, return %d\n", file_record, ret);
		wakeup_event_record_exit();
		wakeup_event_record_init();
	}
}
#endif

static ssize_t mxt_update_firmware(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count, bool *upgraded);
static void mxt_stop(struct mxt_data *data);
static int mxt_check_fw_version(struct mxt_data *data);
static int mxt_disable_hsync_config(struct mxt_data *data);

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

static void mxt_disable_irq(struct mxt_data *data)
{
	if (likely(data->irq_enabled)) {
		disable_irq(data->irq);
		data->irq_enabled = false;
	}
}

static void mxt_enable_irq(struct mxt_data *data)
{
	if (likely(!data->irq_enabled)) {
		enable_irq(data->irq);
		data->irq_enabled = true;
	}
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
	if (!object)
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

static int mxt_set_clr_reg(struct mxt_data *data,
				u8 type, u8 offset, u8 mask_s, u8 mask_c)
{
	int error;
	u8 val;

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

static int mxt_soft_reset(struct mxt_data *data, u8 value)
{
	struct device *dev = &data->client->dev;
	int error;

	dev_info(dev, "Resetting chip\n");

	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
			MXT_COMMAND_RESET, value);
	if (error)
		return error;

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

	if (status & MXT_STATUS_SIGERR) {
		dev_info(dev, "HSYNC lost!\n");
		mxt_disable_hsync_config(data);
	}

	if (status)
		dev_dbg(dev, "T6 status %s%s%s%s%s%s\n",
			(status & MXT_STATUS_RESET) ? "RESET " : "",
			(status & MXT_STATUS_OFL) ? "OFL " : "",
			(status & MXT_STATUS_SIGERR) ? "SIGERR " : "",
			(status & MXT_STATUS_CAL) ? "CAL " : "",
			(status & MXT_STATUS_CFGERR) ? "CFGERR " : "",
			(status & MXT_STATUS_COMSERR) ? "COMSERR " : "");
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

	dev_dbg(dev,
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

	while (i < time_out) {
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

static int mxt_set_power_cfg(struct mxt_data *data, u8 mode);
static void mxt_set_gesture_wake_up(struct mxt_data *data, bool enable);

static void mxt_proc_t100_messages(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;

	struct input_dev *sel_input_dev = NULL;
	u8 status, touch_type, touch_event;
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

		if (status & MXT_T100_SUP) {
			int i;
			for (i = 0; i < data->num_touchids - 2; i++) {
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
			}
			mxt_input_sync(data);
		}
	} else if (id >= 2) {
		/* deal with each point report */
		status = message[1];
		touch_type = (status & 0x70) >> 4;
		touch_event = status & 0x0F;
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

		sel_input_dev = input_dev;
		input_mt_slot(sel_input_dev, id - 2);

		if (status & MXT_T100_DETECT) {
			if (touch_event == MXT_T100_EVENT_DOWN || touch_event == MXT_T100_EVENT_UNSUP
			|| touch_event == MXT_T100_EVENT_MOVE || touch_event == MXT_T100_EVENT_NONE) {
				/* Touch in detect, report X/Y position */
				if (touch_event == MXT_T100_EVENT_DOWN ||
					touch_event == MXT_T100_EVENT_UNSUP)
					data->finger_down[id - 2] = true;
				if ((touch_event == MXT_T100_EVENT_MOVE ||
					touch_event == MXT_T100_EVENT_NONE) &&
					!data->finger_down[id - 2])
					return;

				input_mt_report_slot_state(sel_input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(sel_input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(sel_input_dev, ABS_MT_POSITION_Y, y);

				if (touch_type == MXT_T100_TYPE_HOVERING_FINGER)
					input_report_abs(sel_input_dev, BTN_TOUCH, 0);
				else
					input_report_abs(sel_input_dev, BTN_TOUCH, 1);

				if (touch_type == MXT_T100_TYPE_HOVERING_GLOVE)
					blocking_notifier_call_chain(&glove_mode_chain,
							1, NULL);
				else
					blocking_notifier_call_chain(&glove_mode_chain,
							0, NULL);

				if (data->t100_tchaux_bits &  MXT_T100_AMPL) {
					if (touch_type == MXT_T100_TYPE_HOVERING_FINGER)
						amplitude = 0;
					else if (amplitude == 0)
						amplitude = 1;
					input_report_abs(sel_input_dev, ABS_MT_PRESSURE, amplitude);
				}
				if (data->t100_tchaux_bits &  MXT_T100_AREA) {
					if (touch_type == MXT_T100_TYPE_HOVERING_FINGER)
						area = 0;
					else if (area == 0)
						area = 1;
					input_report_abs(sel_input_dev, ABS_MT_TOUCH_MAJOR, area);
				}
				if (data->t100_tchaux_bits &  MXT_T100_VECT)
					input_report_abs(sel_input_dev, ABS_MT_ORIENTATION, vector);
				mxt_input_sync(data);
			}
		} else {
			/* Touch no longer in detect, so close out slot */
			if (data->touch_num == 0 &&
				data->wakeup_gesture_mode &&
				data->is_wakeup_by_gesture) {
				dev_info(dev, "wakeup finger release, restore t7 and t8!\n");
				data->is_wakeup_by_gesture = false;
				mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
			}
			input_mt_report_slot_state(sel_input_dev, MT_TOOL_FINGER, 0);
			data->finger_down[id - 2] = false;
			mxt_input_sync(data);
		}
	}
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
	int keyvalue;

	if (!input_dev)
		return;
	keyvalue = mxt_read_key_delta(data);
	printk("Key:keystates:%ld, keyvalue:%d\n", keystates, keyvalue);

	for (key = 0; key < pdata->config_array[index].key_num; key++) {
		curr_state = test_bit(key, &data->keystatus);
		new_state = test_bit(key, &keystates);
		if (keyvalue == 0x07 || keystates == 0x07) {
			__set_bit(key, &data->keystatus);
			continue;
		}
		sync = true;
		printk("Key:curr_state:%d, new_state:%d\n", curr_state, new_state);
		if (!curr_state && new_state) {
			input_mt_slot(data->input_dev, 0);
			dev_dbg(dev, "T15 key press: %u\n", key);
			printk("Key:%d, Touch!\n", key);
			__set_bit(key, &data->keystatus);

			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, 2*fhd_key_dim_x[key + 1]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, 2*FHD_KEY_Y);

		} else if (curr_state && !new_state) {
			input_mt_slot(data->input_dev, 0);
			printk("Key:%d, Realease!\n", key);
			__clear_bit(key, &data->keystatus);

			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

		}
	}

	printk("Key:input sync!\n");
}

static void mxt_proc_t19_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;

	data->vendor_id = msg[1];
	data->vendor_id &= pdata->gpio_mask;
	dev_info(dev, "T19: vendor_id & gpio_mask = 0x%x & 0x%x = 0x%x\n",
		msg[1], pdata->gpio_mask, data->vendor_id);
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

static void mxt_proc_t81_message(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;

	if (!input_dev)
		return;

	dev_info(dev, "msg for t81 = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		msg[0], msg[1], msg[2], msg[3], msg[4], msg[5]);

	if (data->is_stopped) {
#ifdef TOUCH_WAKEUP_EVENT_RECORD
		atomic_set(&wakeup_flag, 1);
		wakeup_event_record_write(EVENT_TOUCH_WAKEUP);
#endif
		data->is_wakeup_by_gesture = true;
		input_event(input_dev, EV_KEY, KEY_WAKEUP, 1);
		input_sync(input_dev);
		input_event(input_dev, EV_KEY, KEY_WAKEUP, 0);
		input_sync(input_dev);
	}
}

static void mxt_proc_t92_message(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "msg for t92 = 0x%x 0x%x\n",
		msg[0], msg[1]);

	/* we can do something to handle wakeup gesture */
}

static void mxt_proc_t93_message(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;

	if (!input_dev)
		return;

	dev_info(dev, "msg for t93 = 0x%x 0x%x\n",
		msg[1], msg[2]);

	if (data->is_stopped
		&& (msg[1] & 0x02)) {
#ifdef TOUCH_WAKEUP_EVENT_RECORD
		atomic_set(&wakeup_flag, 1);
		wakeup_event_record_write(EVENT_TOUCH_WAKEUP);
#endif
		data->is_wakeup_by_gesture = true;
		input_event(input_dev, EV_KEY, KEYCODE_WAKEUP, 1);
		input_sync(input_dev);
		input_event(input_dev, EV_KEY, KEYCODE_WAKEUP, 0);
		input_sync(input_dev);
	}
}

static void mxt_proc_t97_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	int index = data->current_index;

	u8 key;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = le32_to_cpu(msg[2]);

	if (!input_dev)
		return;

	for (key = 0; key < pdata->config_array[index].key_num; key++) {
		curr_state = test_bit(key, &data->keystatus);
		new_state = test_bit(key, &keystates);

		if (!curr_state && new_state) {
			dev_dbg(dev, "T97 key press: %u, key_code = %u\n", key, pdata->config_array[index].key_codes[key]);
			__set_bit(key, &data->keystatus);
			input_event(input_dev, EV_KEY, pdata->config_array[index].key_codes[key], 1);
			sync = true;
		} else if (curr_state && !new_state) {
			dev_dbg(dev, "T97 key release: %u, key_code = %u\n", key, pdata->config_array[index].key_codes[key]);
			__clear_bit(key, &data->keystatus);
			input_event(input_dev, EV_KEY,  pdata->config_array[index].key_codes[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}

static void mxt_proc_t109_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "msg for t109 = 0x%x 0x%x\n",
		msg[1], msg[2]);

	data->selfcap_status.cmd = msg[1];
	data->selfcap_status.error_code = msg[2];
}

static void mxt_proc_t220_messages(struct mxt_data *data, u8 *msg)
{
	return;
}

static int mxt_proc_message(struct mxt_data *data, u8 *msg)
{
	u8 report_id = msg[0];

	if (report_id == MXT_RPTID_NOMSG)
		return -EPERM;

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
	} else if (report_id == data->T66_reportid) {
		mxt_proc_t66_messages(data, msg);
	} else if (report_id >= data->T81_reportid_min
		   && report_id <= data->T81_reportid_max) {
		mxt_proc_t81_message(data, msg);
	} else if (report_id >= data->T92_reportid_min
		   && report_id <= data->T92_reportid_max) {
		mxt_proc_t92_message(data, msg);
	} else if (report_id >= data->T93_reportid_min
		   && report_id <= data->T93_reportid_max) {
		mxt_proc_t93_message(data, msg);
	} else if (report_id >= data->T97_reportid_min
		   && report_id <= data->T97_reportid_max) {
		mxt_proc_t97_messages(data, msg);
	} else if (report_id >= data->T100_reportid_min
		   && report_id <= data->T100_reportid_max) {
		mxt_proc_t100_messages(data, msg);
	} else if (report_id == data->T109_reportid) {
		mxt_proc_t109_messages(data, msg);
	} else if (report_id >= data->T220_reportid_min
		   && report_id <= data->T220_reportid_max)
		mxt_proc_t220_messages(data, msg);

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
		dev_warn(dev, "Interrupt triggered but zero messages\n");
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

	if (data->fw_updated == true) {
		goto update_cfg_in;
	}
	/* The Info Block CRC is calculated over mxt_info and the object table
	 * If it does not match then we are trying to load the configuration
	 * from a different chip or firmware version, so the configuration CRC
	 * is invalid anyway. */
	if (info_crc == data->info_block_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "Config CRC 0x%06X: OK\n", data->config_crc);
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev, "Info block CRC mismatch - attempting to apply config\n");
	}
update_cfg_in:
	/* Malloc memory to store configuration */
	config_start_offset = MXT_OBJECT_START
		+ data->info.object_num * MXT_OBJECT_SIZE;
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
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret != 3) {
			dev_err(dev, "Bad format, ret=%d\n", ret);
			ret = -EINVAL;
			goto release_mem;
		}
		data_pos += offset;

		dev_info(dev, "write to type = %d, instance = %d, size = %d, offset = %d\n",
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

	switch (mode) {
	case MXT_POWER_CFG_WAKEUP_GESTURE:
		/* Wakeup gesture mode */
		cnt = ARRAY_SIZE(mxt_save);
		for (i = 0; i < cnt; i++) {
			if (mxt_get_object(data, mxt_save[i].suspend_obj) == NULL)
				continue;
			if (mxt_save[i].suspend_flags == MXT_SUSPEND_DYNAMIC)
				error |= mxt_write_object(data,
					mxt_save[i].suspend_obj,
					mxt_save[i].suspend_reg,
					mxt_save[i].wakeup_gesture_val);
			if (error) {
				error = mxt_chip_reset(data);
				if (error)
					dev_err(dev, "Failed to do chip reset!\n");
				break;
			}
		}
		break;

	case MXT_POWER_CFG_DEEPSLEEP:
		/* Touch disable */
		cnt = ARRAY_SIZE(mxt_save);
		for (i = 0; i < cnt; i++) {
			if (mxt_get_object(data, mxt_save[i].suspend_obj) == NULL)
				continue;
			if (mxt_save[i].suspend_flags == MXT_SUSPEND_DYNAMIC)
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

	data->is_stopped = !!(mode == MXT_POWER_CFG_DEEPSLEEP || mode == MXT_POWER_CFG_WAKEUP_GESTURE);

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

static const char *mxt_get_config(struct mxt_data *data, bool is_default)
{
	const struct mxt_platform_data *pdata = data->pdata;
	int i;

	if (pdata->default_config == -1) {
		/* no default config is set */
		is_default = false;
	}

	for (i = 0; i < pdata->config_array_size; i++) {
		dev_info(&data->client->dev, "Choose config %x,%x,%x,%x,%x,%x\n",
			data->info.family_id, data->info.variant_id, data->info.version, data->info.build, data->rev_id, data->panel_id);
		dev_info(&data->client->dev, "Choose config1 %x,%x,%x,%x,%x,%x\n",
			pdata->config_array[i].family_id, pdata->config_array[i].variant_id, pdata->config_array[i].version, pdata->config_array[i].build, pdata->config_array[i].rev_id, pdata->config_array[i].panel_id);
		if (data->info.family_id == pdata->config_array[i].family_id &&
			data->info.variant_id == pdata->config_array[i].variant_id &&
			data->info.version == pdata->config_array[i].version &&
			data->info.build == pdata->config_array[i].build &&
			data->rev_id == pdata->config_array[i].rev_id &&
			data->panel_id == pdata->config_array[i].panel_id)
			break;
	}

	if (i >= pdata->config_array_size) {
		/* No matching config */
		if (!is_default)
			return NULL;
		else
			i = pdata->default_config;
	}

	if (0x01 == data->lockdown_info[LOCKDOW_HWVERSION]) {
		config_name_buff = "mxt_308u_b5_w.raw.01";
	} else if (0x02 == data->lockdown_info[LOCKDOW_HWVERSION]) {
		config_name_buff = "mxt_308u_b5_w.raw.02";
	} else {
		return NULL;
	}

	dev_info(&data->client->dev, "Choose config %d: %s, is_default = %d\n",
			i, config_name_buff, is_default);

	data->current_index = i;

	return config_name_buff;
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

static int mxt_read_rev(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i = 0;
	u8 val;

	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_DIAGNOSTIC, 0x80);
	if (ret) {
		dev_err(dev, "Failed to send rev read command!\n");
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

	ret = mxt_read_object(data, MXT_DEBUG_DIAGNOSTIC_T37,
				MXT_DIAG_REV_ID, &data->rev_id);
	if (ret) {
		dev_err(dev, "Failed to read rev id!\n");
		return ret;
	}

	dev_info(dev, "read rev_id = 0x%x\n", data->rev_id);

	return 0;
}

static int mxt_read_config_info(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret = 0;
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, MXT_SPT_USERDATA_T38);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	ret = mxt_read_reg(data->client, reg,
		MXT_CONFIG_INFO_SIZE, data->config_info);
	if (ret)
		dev_err(dev, "Failed to read T38\n");

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
	ret = mxt_read_reg(data->client, reg + MXT_LOCKDOWN_OFFSET,
			MXT_LOCKDOWN_SIZE, data->lockdown_info);
	if (ret)
		dev_err(dev, "Failed to read lockdown info!\n");

	return 0;
}
static int mxt_check_reg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u32 k = 0;

	bool is_recheck = false, use_default_cfg = false;

	if (data->firmware_updated)
		use_default_cfg = true;

start:
	ret = mxt_read_rev(data);
	if (ret) {
		dev_err(dev, "Can not get rev!\n");
		if (is_recheck) {
			/* We still cannot get rev id in re-check routine, reg init fails */
			dev_err(dev, "Still unable to get rev id after re-check\n");
			return -ENODEV;
		} else
			is_recheck = true;
	}

	if (!data->firmware_updated) {
		ret = mxt_read_config_info(data);
		if (ret) {
			dev_err(dev, "Can not get config info, set config info to 0xFF\n");
			memset(data->config_info, 0xFF, MXT_CONFIG_INFO_SIZE);
			if (is_recheck) {
				/* We still cannot get config info in re-check routine, reg init fails */
				dev_err(dev, "Still unable to get config info after re-check\n");
				return -ENODEV;
			} else
				is_recheck = true;
		}
	} else {
		/* Once we upgrade the firmware, the old config might not match new firmware.
		 * Therefore, we might get the wrong value when we try to get config info with
		 * the old config. Since the config info has already been retrieved with the old
		 * FW & CFG, use the old info instead */
		dev_info(dev, "Just upgraded firmware, use the old config info instead.\n");
	}

	ret = mxt_read_lockdown_info(data);
	if (ret) {
		dev_err(dev, "Can not get lockdown info, set lockdown info to 0xFF!\n");
		memset(data->lockdown_info, 0xFF, MXT_LOCKDOWN_SIZE);
		if (is_recheck) {
			/* We still cannot get lockdown info in re-check routine, reg init fails */
			dev_err(dev, "Still unable to get lockdown info after re-check\n");
			return -ENODEV;
		} else
			is_recheck = true;
	}

	dev_info(dev, "Config info: %02X %02X %02X %02X %02X %02X %02X %02X",
			data->config_info[0], data->config_info[1],
			data->config_info[2], data->config_info[3],
			data->config_info[4], data->config_info[5],
			data->config_info[6], data->config_info[7]);

	dev_info(dev, "Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
			data->lockdown_info[0], data->lockdown_info[1],
			data->lockdown_info[2], data->lockdown_info[3],
			data->lockdown_info[4], data->lockdown_info[5],
			data->lockdown_info[6], data->lockdown_info[7]);

	TP_Maker = data->lockdown_info[0];
	LCD_Maker = data->lockdown_info[1];
	Panel_Ink = data->lockdown_info[2];
		for (k = 0; k < MXT_LOCKDOWN_SIZE; k++) {
			lockdown_info[k] = data->lockdown_info[k];
		}
	tp_color =  lockdown_info[2];
	data->panel_id = data->lockdown_info[0];

	/* WAD: Some old panels do not have lockdown info, just check the first
	 * byte of config info, since this byte was stored as user_id.
	 * If still misses, recognize them as Biel panels */
	if (data->panel_id == 0) {
		if (data->pdata->default_panel_id) {
			dev_err(dev, "No lockdown info stored, use default panel id\n");
			data->panel_id = data->pdata->default_panel_id;
		} else {
			dev_err(dev, "No lockdown info stored\n");
		}
	}

	dev_info(dev, "ATMEL308: Panel_id:0x%02X\n", data->panel_id);

	if (data->config_info[0] >= 0x65) {
		dev_info(dev, "NOTE: THIS IS A DEBUG CONFIG(V%02X), WILL NOT BE UPGRADED!\n",
				data->config_info[0]);
		return 0;
	}

	mxt_read_lockdown_info(data);
	dev_info(dev, "Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
			data->lockdown_info[0], data->lockdown_info[1],
			data->lockdown_info[2], data->lockdown_info[3],
			data->lockdown_info[4], data->lockdown_info[5],
			data->lockdown_info[6], data->lockdown_info[7]);

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
			data->num_touchids = object->num_report_ids * object->instances;
			break;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = object->max_reportid;
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
		case MXT_TOUCH_KEYARRAY_T15:
			data->T15_reportid_max = object->max_reportid;
			data->T15_reportid_min = object->min_reportid;
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
		case MXT_PROCI_ACTIVE_STYLUS_T63:
			data->T63_reportid_max = object->max_reportid;
			data->T63_reportid_min = object->min_reportid;
			data->num_stylusids =
				object->num_report_ids * object->instances;
			break;
		case MXT_SPT_GOLDENREF_T66:
			data->T66_reportid = object->max_reportid;
			break;
		case MXT_TOUCH_MORE_GESTURE_T81:
			data->T81_reportid_min = object->min_reportid;
			data->T81_reportid_max = object->max_reportid;
			break;
		case MXT_TOUCH_GESTURE_T92:
			data->T92_reportid_min = object->min_reportid;
			data->T92_reportid_max = object->max_reportid;
			break;
		case MXT_TOUCH_SEQUENCE_LOGGER_T93:
			data->T93_reportid_min = object->min_reportid;
			data->T93_reportid_max = object->max_reportid;
			break;
		case MXT_TOUCH_MULTI_T100:
			data->T100_reportid_max = object->max_reportid;
			data->T100_reportid_min = object->min_reportid;
			data->num_touchids = object->num_report_ids * object->instances;
			break;
		case MXT_SPT_SELFCAPGLOBALCONFIG_T109:
			data->T109_reportid = object->max_reportid;
			break;
		case MXT_TOUCH_KEYARRAY_T97:
			data->T97_reportid_max = object->max_reportid;
			data->T97_reportid_min = object->min_reportid;
			break;
		case MXT_TOUCH_EDGE_GESTURE_T220:
			data->T220_reportid_max = object->max_reportid;
			data->T220_reportid_min = object->min_reportid;
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

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);


static int mxt_wait_for_self_tune_msg(struct mxt_data *data, u8 expect_val)
{
	int time_out = 1000;
	int i = 0;

	while (i < time_out) {
		if (data->selfcap_status.cmd == expect_val)
			return 0;
		i++;
		msleep(10);
	}

	return -ETIMEDOUT;
}

static int mxt_do_self_tune(struct mxt_data *data, u8 cmd)
{
	int error;
	struct device *dev = &data->client->dev;

	memset(&data->selfcap_status, 0x0, sizeof(data->selfcap_status));

	if (mxt_get_object(data, MXT_SPT_SELFCAPGLOBALCONFIG_T109) == NULL) {
		dev_err(dev, "No T109 exist!\n");
		return 0;
	}

	error = mxt_write_object(data, MXT_SPT_SELFCAPGLOBALCONFIG_T109,
					MXT_SELFCAPCFG_CTRL, MXT_SELFCTL_RPTEN);
	if (error) {
		dev_err(dev, "Error when enable t109 report en!\n");
		return error;
	}

	error = mxt_write_object(data, MXT_SPT_SELFCAPGLOBALCONFIG_T109,
					MXT_SELFCAPCFG_CMD, cmd);
	if (error) {
		dev_err(dev, "Error when execute cmd 0x%x!\n", cmd);
		return error;
	}

	error = mxt_wait_for_self_tune_msg(data, cmd);

	if (!error) {
		if (data->selfcap_status.error_code != 0)
			return -EINVAL;
	}

	return 0;
}

static int mxt_get_init_setting(struct mxt_data *data)
{
	int error;
	u8 selfthr;
	u8 intthr;
	u8 glovectrl;
	u8 atchthr;
	int i;
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	int index = data->current_index;

	if (mxt_get_object(data, MXT_SPT_AUXTOUCHCONFIG_T104) != NULL) {
		error = mxt_read_object(data, MXT_SPT_AUXTOUCHCONFIG_T104,
						MXT_AUXTCHCFG_XTCHTHR, &selfthr);
		if (error) {
			dev_err(dev, "Failed to read self threshold from t104!\n");
			return error;
		}

		error = mxt_read_object(data, MXT_SPT_AUXTOUCHCONFIG_T104,
						MXT_AUXTCHCFG_INTTHRX, &intthr);
		if (error) {
			dev_err(dev, "Failed to read internal threshold from t104!\n");
			return error;
		}
	}

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

	for (i = 0; i < ARRAY_SIZE(mxt_save); i++) {
		error = mxt_read_object(data, MXT_GEN_POWER_T7,
					i, &mxt_save[i].restore_val);
		if (error) {
			dev_err(dev, "Failed to read T7 byte %d\n", i);
			return error;
		}
	}

	if (mxt_get_object(data, MXT_PROCG_NOISESUPSELFCAP_T108) != NULL) {
		for (i = 0; i < ARRAY_SIZE(data->adcperx_normal); i++) {
			data->adcperx_wakeup[i] = pdata->config_array[index].wake_up_self_adcx;
			error = mxt_read_object(data, MXT_PROCG_NOISESUPSELFCAP_T108,
						19 + i, &data->adcperx_normal[i]);
			if (error)  {
				dev_err(dev, "Failed to read T108 setting %d\n", i);
				return error;
			}
		}
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
	/* If error occurs in reading register, do not download firmware */
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
				/* this is not an error state, we will reflash fw later by shell command*/

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
	Fw_Version[0] = info->version >> 4;
	Fw_Version[1] = info->version & 0xf;
	Fw_Version[2] = info->build;
	/* Get object table information */
	error = mxt_get_object_table(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading object table\n", error);
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

	/* Check register init values */
	error = mxt_check_reg_init(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize config\n");
		return error;
	}

	if (mxt_get_object(data, MXT_TOUCH_MULTI_T100) != NULL) {
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

	for (i = 0; i < len; i += 2) {
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

	return -EPERM;
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
	unsigned int frame = 0;
	int ret;
	unsigned short ori_addr = data->client->addr;
	size_t len = 0;
	u8 *buffer;
	dev_err(dev, "fw_step 1: request_firmware. ");
	ret = request_firmware(&fw, fn, dev);
	if (ret < 0) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		return ret;
	}

	buffer = kmalloc(fw->size, GFP_KERNEL);
	if (!buffer) {
		dev_err(dev, "malloc firmware buffer failed!\n");
		return -ENOMEM;
	}
	memcpy(buffer, fw->data, fw->size);
	len = fw->size;

	dev_err(dev, "fw_step 1: check_firmware_format. ");
	ret  = mxt_check_firmware_format(dev, fw);
	if (ret) {
		dev_info(dev, "text format, convert it to binary!\n");
		len = mxt_convert_text_to_binary(buffer, len);
		if (len <= 0)
			goto release_firmware;
	}


	if (data->state != BOOTLOADER) {
		/* Change to the bootloader mode */
		if (mxt_soft_reset(data, MXT_RESET_BOOTLOADER))
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
		ret = mxt_bootloader_write(data, buffer + pos, frame_size);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}

		mxt_wait_for_chg(data);
		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS);

		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				data->state = FAILED;
				goto release_firmware;
			}
		} else {
				retry++;
				pos += frame_size;
				frame++;
		}

		if (frame % 10 == 0) {
			dev_info(dev, "Updated %d frames, %d/%zd bytes\n", frame, pos, len);
		}
	}

	dev_err(dev, "fw_step 1: finished");
	dev_err(dev, "Finished, sent %d frames, %d bytes\n", frame, pos);

	data->state = INIT;

release_firmware:
	data->client->addr = ori_addr;
	release_firmware(fw);
	kfree(buffer);
	return ret;
}

static int mxt_check_fw_version(struct mxt_data *data)
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
	dev_err(dev, "Enter mxt_check_fw_version\n");

	config_name = mxt_get_config(data, false);

	if (config_name == NULL) {
		dev_info(dev, "Not found matched config!\n");
		return 0;
	}

	ret = request_firmware(&cfg, config_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n", config_name);
		return 0;
	}

	mxt_read_current_crc(data);

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = 0;
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
			ret = 0;
			goto release;
		}

		data_pos += offset;
	}

	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = 0;
		goto release;
	}
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = 0;
		goto release;
	}
	data_pos += offset;

	/* The Info Block CRC is calculated over mxt_info and the object table
	 * If it does not match then we are trying to load the configuration
	 * from a different chip or firmware version, so the configuration CRC
	 * is invalid anyway. */
	if (info_crc == data->info_block_crc) {
		ret = 0;
		dev_err(dev, "Firmware does not need updating\n");
	} else {
		ret = 1;
		dev_err(dev, "Firmware needs updating\n");
	}

release:
	release_firmware(cfg);
	return ret;
}

static int mxt_update_config(struct mxt_data *data)
{
	int ret;
	const char *config_name = NULL;
	struct device *dev = &data->client->dev;
	unsigned char val;

	config_name = mxt_get_config(data, false);

	if (config_name == NULL) {
		dev_info(dev, "Not found matched config!\n");
		return -ENOENT;
	}

	ret = mxt_download_config(data, config_name);
	if (ret < 0)
		goto out;
	else if (ret == 0)
		/* CRC matched, or no config file, or config parse failure.
		 * Even if we need to re-check, we still cannot get the correct
		 * info in current config. So there is no need to reset */
		goto read_version;

	/* Backup to memory */
	ret = mxt_backup_nv(data);
	if (ret) {
		dev_err(dev, "back nv failed!\n");
		goto out;
	}

	ret = mxt_probe_power_cfg(data);
	if (ret) {
		dev_err(dev, "Failed to initialize power cfg\n");
		goto out;
	}
	ret = mxt_get_init_setting(data);
	if (ret) {
		dev_err(dev, "Failed to get init setting.\n");
		goto out;
	}
	ret = mxt_initialize_input_device(data);
	if (ret) {
		dev_err(dev, "Failed to get init input device.\n");
		goto out;
	}

read_version:
	ret =  mxt_read_object(data, MXT_SPT_USERDATA_T38, MXT_CFG_VERSION, &val);
	if (ret) {
		dev_err(dev, "Failed to get CFG version.\n");
		goto out;
	} else {
		dev_err(dev, "CFG version is %d\n", val);
	}

out:
	return ret;
}

static ssize_t mxt_update_firmware(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count, bool *upgraded)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	char *fw_name;
	int len = 0;
	int ret;

	data->fw_updated = false;

	if (upgraded)
		*upgraded = false;
	if (data->state == BOOTLOADER) {
		goto update_fw;
	}

	ret = mxt_check_fw_version(data);
	if (!ret) {
		goto update_cfg;
	}
update_fw:

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

	dev_info(dev, "Identify firmware name :%s\n", fw_name);
	mxt_disable_irq(data);

	error = mxt_load_fw(dev, fw_name);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");
		data->fw_updated = true;
		/* Wait for reset */
		msleep(MXT_FWRESET_TIME);

		kfree(data->object_table);
		data->object_table = NULL;
		kfree(data->msg_buf);
		data->msg_buf = NULL;

		if (upgraded)
			*upgraded = true;

		mxt_initialize(data);
	}

	if (data->state == APPMODE) {
		mxt_enable_irq(data);
	}

	kfree(fw_name);
update_cfg:
	mxt_update_config(data);
	return count;
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
	return mxt_update_firmware(dev, attr, buf, count, NULL);
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

static void mxt_self_tune(struct mxt_data *data, u8 save_cmd)
{
	struct device *dev = &data->client->dev;
	int retry_times = 10;
	int i = 0;
	int error;

	while (i++ < retry_times) {
		error = mxt_do_self_tune(data, MXT_SELFCMD_TUNE);
		if (error) {
			dev_err(dev, "Self tune cmd failed!\n");
			continue;
		}
		error = mxt_do_self_tune(data, save_cmd);
		if (!error)
			return;
		else {
			dev_err(dev, "Self store cmd failed!\n");
			continue;
		}
	}

	dev_err(dev, "Even retry self tuning for 10 times, still can't pass.!\n");
}
static ssize_t mxt_self_tune_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u8 execute_cmd;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%hhu", &execute_cmd) == 1)
		mxt_self_tune(data, MXT_SELFCMD_STCR_TUNE);
	else
		return -EINVAL;

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

static void mxt_calibration_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct mxt_data *data = container_of(delayed_work, struct mxt_data,
						calibration_delayed_work);

	mxt_do_calibration(data);
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
	return sprintf(buf, "%02X, %02X, %02X, %02X, %02X, %02X\n",
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

static int mxt_stylus_mode_switch(struct mxt_data *data, bool mode_on)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	int error;
	u8 ctrl;
	u8 mult_intthr;
	u8 mult_tchthr;
	int index = data->current_index;

	error = mxt_read_object(data, MXT_PROCI_STYLUS_T47,
					MXT_PSTYLUS_CTRL, &ctrl);
	if (error) {
		dev_err(dev, "Failed to read from T47!\n");
		return error;
	}

	if (mode_on) {
		ctrl |= MXT_PSTYLUS_ENABLE;
		mult_intthr = pdata->config_array[index].mult_intthr_sensitive;
		mult_tchthr = pdata->config_array[index].mult_tchthr_sensitive;
	} else {
		ctrl &= ~(MXT_PSTYLUS_ENABLE);
		if (!data->sensitive_mode) {
			mult_intthr = pdata->config_array[index].mult_intthr_not_sensitive;
			mult_tchthr = pdata->config_array[index].mult_tchthr_not_sensitive;
		} else {
			mult_intthr = pdata->config_array[index].mult_intthr_sensitive;
			mult_tchthr = pdata->config_array[index].mult_tchthr_sensitive;
		}
	}

	error = mxt_write_object(data, MXT_PROCI_STYLUS_T47,
			MXT_PSTYLUS_CTRL, ctrl);
	if (error) {
		dev_err(dev, "Failed to read from t47!\n");
		return error;
	}

	error = mxt_write_object(data, MXT_TOUCH_MULTI_T100,
					MXT_MULTITOUCH_INTTHR, mult_intthr);
	if (error) {
		dev_err(dev, "Failed in writing t100 intthr!\n");
		return error;
	}

	if (mult_tchthr != 0) {
		error = mxt_write_object(data, MXT_TOUCH_MULTI_T100,
						MXT_MULTITOUCH_TCHTHR, mult_tchthr);
		if (error) {
			dev_err(dev, "Failed in writing t100 tchthr!\n");
			return error;
		}

		error = mxt_write_object(data, MXT_SPT_DYMDATA_T71,
						pdata->config_array[index].t71_tchthr_pos, mult_tchthr);
		if (error) {
			dev_err(dev, "Failed in writing t71 tchthr!\n");
			return error;
		}
	}

	data->stylus_mode = (u8)mode_on;
	return 0;
}

static ssize_t mxt_stylus_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;

	count = sprintf(buf, "%d\n", (int)data->stylus_mode);

	return count;
}

static ssize_t mxt_stylus_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error, i;

	if (sscanf(buf, "%u", &i) == 1)  {
		if (i == 1) {
			error = mxt_stylus_mode_switch(data, true);
			if (error) {
				dev_err(dev, "Failed to enable stylus mode!\n");
				return error;
			}
		} else if (i == 0) {
			error = mxt_stylus_mode_switch(data, false);
			if (error) {
				dev_err(dev, "Failed to disable stylus mode!\n");
				return error;
			}
		} else
			return -EINVAL;

	}

	return count;
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
		len++;
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

static int mxt_sensitive_mode_switch(struct mxt_data *data, bool mode_on)
{
	int error;
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	int index = data->current_index;
	u8 key_ctrl;
	u8 mult_intthr;
	u8 atchthr;
	u8 mult_tchthr;

	error = mxt_read_object(data, MXT_TOUCH_KEYARRAY_T15,
					MXT_KEYARRAY_CTRL, &key_ctrl);
	if (error) {
		dev_err(dev, "Failed to read from T15 ctrl!\n");
		return error;
	}

	if (mode_on) {
		error = mxt_write_object(data, MXT_PROCI_GLOVEDETECTION_T78,
						MXT_GLOVE_CTRL, MXT_GLOVECTL_ALL_ENABLE);
		if (error)
			return error;

		key_ctrl |= MXT_KEY_ADAPTTHREN;
		mult_intthr = pdata->config_array[index].mult_intthr_sensitive;
		mult_tchthr = pdata->config_array[index].mult_tchthr_sensitive;
		atchthr = pdata->config_array[index].atchthr_sensitive;
	} else {
		error = mxt_write_object(data, MXT_PROCI_GLOVEDETECTION_T78,
						MXT_GLOVE_CTRL, 0x0);
		if (error)
			return error;

		key_ctrl &= (~MXT_KEY_ADAPTTHREN);
		if (!data->stylus_mode) {
			mult_intthr = pdata->config_array[index].mult_intthr_not_sensitive;
			mult_tchthr = pdata->config_array[index].mult_tchthr_not_sensitive;
		} else {
			mult_intthr = pdata->config_array[index].mult_intthr_sensitive;
			mult_tchthr = pdata->config_array[index].mult_tchthr_sensitive;
		}
		atchthr = data->atchthr;
	}

	error = mxt_write_object(data, MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,
					MXT_RETRANS_ATCHTHR, atchthr);
	if (error) {
		dev_err(dev, "Failed in writing t80 atchthr!\n");
		return error;
	}

	error = mxt_write_object(data, MXT_TOUCH_MULTI_T100,
					MXT_MULTITOUCH_INTTHR, mult_intthr);
	if (error) {
		dev_err(dev, "Failed in writing t100 intthr!\n");
		return error;
	}
	if (mult_tchthr != 0) {
		error = mxt_write_object(data, MXT_TOUCH_MULTI_T100,
						MXT_MULTITOUCH_TCHTHR, mult_tchthr);
		if (error) {
			dev_err(dev, "Failed in writing t100 tchthr!\n");
			return error;
		}

		error = mxt_write_object(data, MXT_SPT_DYMDATA_T71,
						pdata->config_array[index].t71_tchthr_pos, mult_tchthr);
		if (error) {
			dev_err(dev, "Failed in writing t71 tchthr!\n");
			return error;
		}
	}

	error = mxt_write_object(data, MXT_TOUCH_KEYARRAY_T15,
					MXT_KEYARRAY_CTRL, key_ctrl);
	if (error) {
		dev_err(dev, "Failed in writing t15 key ctrl!\n");
		return error;
	}

	data->sensitive_mode = (u8)mode_on;

	return error;
}

static ssize_t mxt_wakeup_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;

	count = sprintf(buf, "%d\n", (int)data->wakeup_gesture_mode);

	return count;
}

static void mxt_enable_gesture_mode(struct mxt_data *data, bool enable)
{
	u8 t93_val;
	int error;

	t93_val = enable ? 0x0F : 0x0D;

	/* T93 is for double tap */
	error = mxt_write_object(data, MXT_TOUCH_SEQUENCE_LOGGER_T93,
				MXT_DBL_TAP_CTRL, t93_val);
	if (error)
		dev_err(&data->client->dev, "write to t93 enabled failed!\n");
}

static ssize_t mxt_wakeup_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct mxt_platform_data *pdata = data->pdata;
	int index = data->current_index;
	unsigned long val;
	int error;

	if (pdata->config_array[index].wake_up_self_adcx == 0)
		return count;

	if (pdata->cut_off_power) {
		dev_err(&data->client->dev, "Wakeup gesture not supported\n");
		return count;
	}

	error = strict_strtoul(buf, 0, &val);

	if (!error)
		data->wakeup_gesture_mode = (u8)val;

	if (data->is_stopped) {
		/* Set wakeup gesture mode in deepsleep,
		 * should re-set the registers */
		if (data->wakeup_gesture_mode) {
			mxt_enable_irq(data);
			if (data->input_dev->users)
				mxt_stop(data);
		} else {
			mxt_disable_irq(data);
			data->is_wakeup_by_gesture = false;
			mxt_set_gesture_wake_up(data, false);
			mxt_enable_gesture_mode(data, false);
			mxt_set_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
		}
	}

	return error ? : count;
}

static ssize_t mxt_sensitive_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;

	count = sprintf(buf, "%d\n", (int)data->sensitive_mode);

	return count;
}

static ssize_t  mxt_sensitive_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (!error) {
		if (val == 1) {
			error = mxt_sensitive_mode_switch(data, true);
			if (error)
				dev_err(dev, "Failed to open sensitive mode!\n");
		} else if (val == 0) {
			error = mxt_sensitive_mode_switch(data, false);
			if (error)
				dev_err(dev, "Failed to close sensitive mode!\n");
		}
	}

	return error ? : count;
}

static int mxt_do_diagnostic_test(struct mxt_data *data, int cmd, int buf_size, u8 *buf)
{
	int error;
	struct device *dev = &data->client->dev;
	u16 addr = data->T37_address;
	int read_size = 0, tmp_size;

	if (!buf) {
		dev_err(dev, "No enough memory\n");
		return -ENOMEM;
	}

	error = mxt_do_diagnostic(data, cmd);
	if (error) {
		dev_err(dev, "Do diagnostic 0x%02x failed!\n", cmd);
		return -EINVAL;
	}

	while (read_size < buf_size) {
		tmp_size = buf_size - read_size;
		if (tmp_size > MXT_DIAG_PAGE_SIZE)
			tmp_size = MXT_DIAG_PAGE_SIZE;

		error = mxt_read_reg(data->client, addr + 2,
				tmp_size, buf + read_size);
		if (error) {
			dev_err(dev, "Read from T37 failed!\n");
			return -EINVAL;
		}

		read_size += tmp_size;

		error = mxt_do_diagnostic(data, MXT_DIAG_PAGE_UP);
		if (error) {
			dev_err(dev, "Do diagnostic 0x%02x failed!\n", MXT_DIAG_PAGE_UP);
			return -EINVAL;
		}
	}

	return 0;
}

static ssize_t mxt_mutual_ref_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	size_t len;

	if (!data->raw_ref_buf) {
		dev_err(dev, "No raw reference data\n");
		return 0;
	}

	len = strlen(data->raw_ref_buf);
	if (off > MXT_REF_ATTR_SIZE || off > len)
		return 0;

	if (off + count > len)
		count = len - off;

	memcpy(buf, &data->raw_ref_buf[off], count);

	return count;

}

static int mxt_read_key_delta(struct mxt_data *data)
{
	int error = 0;
	int max_nodes = MXT_KEY_SIZE;
	u8 keyvalue = 0;
	int maxdelta = 0;
	if (data->raw_key_delta)
		kfree(data->raw_key_delta);

	data->raw_key_delta = kzalloc(MXT_KEY_SIZE * sizeof(char), GFP_KERNEL);
	if (!data->raw_key_delta) {
		dev_err(&data->client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto out;
	}

	data->raw_key_delta = kzalloc(max_nodes * sizeof(s16), GFP_KERNEL);
	if (!data->raw_key_delta) {
		dev_err(&data->client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_raw_key_delta;
	}

	error = mxt_do_diagnostic_test(data, MXT_DIAG_KEY_DELTA, max_nodes * sizeof(s16), (u8 *)data->raw_key_delta);
	if (error < 0) {
		dev_err(&data->client->dev, "Error getting mutual cap references\n");
		error = -ENOMEM;
		goto err_free_raw_key_delta;
	}

	if (0x01 == data->lockdown_info[LOCKDOW_HWVERSION]) {
		maxdelta = 150;
	} else if (0x02 == data->lockdown_info[LOCKDOW_HWVERSION]) {
		maxdelta = 300;
	}
	if (data->raw_key_delta[0] > maxdelta)
		keyvalue |= 0x01;
	if (data->raw_key_delta[1] > maxdelta)
		keyvalue |= 0x02;
	if (data->raw_key_delta[2] > maxdelta)
		keyvalue |= 0x04;

	dev_info(&data->client->dev, "key delta %d, %d, %d, %d\n", data->raw_key_delta[0], data->raw_key_delta[1], data->raw_key_delta[2], keyvalue);

	goto out;
err_free_raw_key_delta:
	kfree(data->raw_key_delta);
	data->raw_key_delta = NULL;
out:
	return keyvalue;
}

static ssize_t mxt_mutual_ref_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int chan_x, chan_y, max_nodes;
	s16 *ref_buf;
	int error = 0, i, j;
	unsigned int value;

	if (sscanf(buf, "%u", &value) != 1 || value != 1) {
		error = -EINVAL;
		goto out;
	}

	if (data->info.family_id == 0xA4 && data->info.variant_id == 0x15) {
		/* MXT336T */
		chan_x = MXT_336T_CHANNELS_X;
		chan_y = MXT_336T_CHANNELS_Y;
		max_nodes = MXT_336T_MAX_NODES;
	} else if (data->info.family_id == 0xA6 && data->info.variant_id == 0x00) {
		/* MXT874U */
		chan_x = MXT_874U_CHANNELS_X;
		chan_y = MXT_874U_CHANNELS_Y;
		max_nodes = MXT_874U_MAX_NODES;
	} else if (data->info.family_id == 0xA6 && data->info.variant_id == 0x07) {
		/* MXT308U */
		chan_x = MXT_308U_CHANNELS_X;
		chan_y = MXT_308U_CHANNELS_Y;
		max_nodes = MXT_308U_MAX_NODES;
	}

	if (data->raw_ref_buf)
		kfree(data->raw_ref_buf);

	data->raw_ref_buf = kzalloc(MXT_REF_ATTR_SIZE * sizeof(char), GFP_KERNEL);
	if (!data->raw_ref_buf) {
		dev_err(dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto out;
	}

	ref_buf = kzalloc(max_nodes * sizeof(s16), GFP_KERNEL);
	if (!ref_buf) {
		dev_err(dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_raw_ref_buf;
	}

	error = mxt_do_diagnostic_test(data, MXT_DIAG_MUTUAL_REF,
			max_nodes * sizeof(s16), (u8 *)ref_buf);
	if (error < 0) {
		dev_err(dev, "Error getting mutual cap references\n");
		error = -ENOMEM;
		goto err_free_mutual_ref_buf;
	}

	for (i = 0; i < chan_x; i++) {
		for (j = 0; j < chan_y; j++) {
			snprintf(data->raw_ref_buf, MXT_REF_ATTR_SIZE, "%s%d ",
				data->raw_ref_buf, ref_buf[i * chan_y + j]);
		}
		strncat(data->raw_ref_buf, "\n", MXT_REF_ATTR_SIZE);
	}

	kfree(ref_buf);
	goto out;

err_free_mutual_ref_buf:
	kfree(ref_buf);
err_free_raw_ref_buf:
	kfree(data->raw_ref_buf);
	data->raw_ref_buf = NULL;
out:
	return count;
}

static ssize_t mxt_self_ref_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	size_t len;

	if (!data->raw_ref_buf) {
		dev_err(dev, "No raw reference data\n");
		return 0;
	}

	len = strlen(data->raw_ref_buf);
	if (off > MXT_REF_ATTR_SIZE || off > len)
		return 0;

	if (off + count > len)
		count = len - off;

	memcpy(buf, &data->raw_ref_buf[off], count);

	return count;
}

static ssize_t mxt_self_ref_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int chan_x, chan_y, chan_x_reserved, even_x_start, odd_x_start;
	s16 *ref_buf;
	int error = 0, i;
	unsigned int value;

	if (sscanf(buf, "%u", &value) != 1 || value != 1) {
		error = -EINVAL;
		goto out;
	}

	if (data->info.family_id == 0xA4 && data->info.variant_id == 0x15) {
		/* MXT336T */
		chan_x = MXT_336T_CHANNELS_X;
		chan_y = MXT_336T_CHANNELS_Y;
		chan_x_reserved = MXT_336T_X_SELFREF_RESERVED;
		even_x_start = MXT_336T_CHANNELS_Y;
		odd_x_start = MXT_336T_CHANNELS_Y +
			MXT_336T_CHANNELS_X / 2 +
			MXT_336T_X_SELFREF_RESERVED;
	} else if (data->info.family_id == 0xA6 && data->info.variant_id == 0x00) {
		/* MXT874U */
		chan_x = MXT_874U_CHANNELS_X;
		chan_y = MXT_874U_CHANNELS_Y;
		chan_x_reserved = MXT_874U_X_SELFREF_RESERVED;
		even_x_start = MXT_874U_CHANNELS_Y;
		odd_x_start = MXT_874U_CHANNELS_Y +
			MXT_874U_CHANNELS_X / 2 +
			MXT_874U_X_SELFREF_RESERVED;
	} else if (data->info.family_id == 0xA6 && data->info.variant_id == 0x07) {
		/* MXT308U */
		chan_x = MXT_308U_CHANNELS_X;
		chan_y = MXT_308U_CHANNELS_Y;
		chan_x_reserved = MXT_308U_X_SELFREF_RESERVED;
		even_x_start = MXT_308U_CHANNELS_X;
		odd_x_start = MXT_308U_CHANNELS_Y +
			MXT_308U_CHANNELS_X / 2 +
			MXT_308U_X_SELFREF_RESERVED;
	}

	if (data->raw_ref_buf)
		kfree(data->raw_ref_buf);

	data->raw_ref_buf = kzalloc(MXT_REF_ATTR_SIZE * sizeof(char), GFP_KERNEL);
	if (!data->raw_ref_buf) {
		dev_err(dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto out;
	}

	ref_buf = kzalloc((chan_y + chan_x + chan_x_reserved) * sizeof(s16),
		GFP_KERNEL);
	if (!ref_buf) {
		dev_err(dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_raw_ref_buf;
	}

	error = mxt_do_diagnostic_test(data, MXT_DIAG_SELF_REF,
			(chan_y + chan_x + chan_x_reserved) * sizeof(s16),
			(u8 *)ref_buf);
	if (error < 0) {
		dev_err(dev, "Error getting self cap references\n");
		error = -ENOMEM;
		goto err_free_self_ref_buf;
	}

	/* Self cap ref for Y channels */
	for (i = 0; i < chan_y; i++)
		snprintf(data->raw_ref_buf, MXT_REF_ATTR_SIZE, "%s%d ",
				data->raw_ref_buf, ref_buf[i]);
	strncat(data->raw_ref_buf, "\n", MXT_REF_ATTR_SIZE);

	/* Self cap ref for X channels */
	for (i = 0; i < chan_x; i++) {
		if (i & 0x01) {
			/* Self ref data from odd x array */
			snprintf(data->raw_ref_buf, MXT_REF_ATTR_SIZE, "%s%d ",
				data->raw_ref_buf, ref_buf[odd_x_start + (i - 1) / 2]);
		} else {
			/* Self ref data from even x array */
			snprintf(data->raw_ref_buf, MXT_REF_ATTR_SIZE, "%s%d ",
				data->raw_ref_buf, ref_buf[even_x_start + i / 2]);
		}
	}
	strncat(data->raw_ref_buf, "\n", MXT_REF_ATTR_SIZE);

	kfree(ref_buf);
	goto out;

err_free_self_ref_buf:
	kfree(ref_buf);
err_free_raw_ref_buf:
	kfree(data->raw_ref_buf);
	data->raw_ref_buf = NULL;
out:
	return count;
}

static int mxt_chip_reset(struct mxt_data *data)
{
	int error;

	mxt_disable_irq(data);
	gpio_set_value(data->pdata->reset_gpio, 0);
	msleep(20);
	gpio_set_value(data->pdata->reset_gpio, 1);

	msleep(10);
	mxt_wait_for_chg(data);
	mxt_enable_irq(data);
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

static void mxt_switch_mode_work(struct work_struct *work)
{
	struct mxt_mode_switch *ms = container_of(work, struct mxt_mode_switch, switch_mode_work);
	struct mxt_data *data = ms->data;
	const struct mxt_platform_data *pdata = data->pdata;
	int index = data->current_index;
	u8 value = ms->mode;

	if (value == MXT_INPUT_EVENT_SENSITIVE_MODE_ON ||
				value == MXT_INPUT_EVENT_SENSITIVE_MODE_OFF)
		mxt_sensitive_mode_switch(data, (bool)(value - MXT_INPUT_EVENT_SENSITIVE_MODE_OFF));
	else if (value == MXT_INPUT_EVENT_STYLUS_MODE_ON ||
				value == MXT_INPUT_EVENT_STYLUS_MODE_OFF)
		mxt_stylus_mode_switch(data, (bool)(value - MXT_INPUT_EVENT_STYLUS_MODE_OFF));
	else if (value == MXT_INPUT_EVENT_WAKUP_MODE_ON ||
				value == MXT_INPUT_EVENT_WAKUP_MODE_OFF) {
		if (pdata->cut_off_power) {
			dev_err(&data->client->dev, "Wakeup gesture not supported\n");
			return;
		}

		if (pdata->config_array[index].wake_up_self_adcx != 0) {
			data->wakeup_gesture_mode = value - MXT_INPUT_EVENT_WAKUP_MODE_OFF;
			if (data->is_stopped) {
				/* Set wakeup gesture mode in deepsleep,
				 * should re-set the registers */
				if (data->wakeup_gesture_mode) {
					mxt_enable_irq(data);
					if (data->input_dev->users)
						mxt_stop(data);
				} else {
					mxt_disable_irq(data);
					data->is_wakeup_by_gesture = false;
					mxt_set_gesture_wake_up(data, false);
					mxt_enable_gesture_mode(data, false);
					mxt_set_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
				}

			}
		}
	}

	if (ms != NULL) {
		kfree(ms);
		ms = NULL;
	}
}

static int mxt_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	struct mxt_data *data = input_get_drvdata(dev);
	char buffer[16];
	struct mxt_mode_switch *ms;

	if (type == EV_SYN && code == SYN_CONFIG) {
		if (data->debug_enabled) {
			dev_info(&data->client->dev,
				"event write value = %d \n", value);
		}
		sprintf(buffer, "%d", value);
		dev_info(&data->client->dev, "Gesture onoff = %d \n", value);
		if (value >= MXT_INPUT_EVENT_START && value <= MXT_INPUT_EVENT_END) {
			ms = (struct mxt_mode_switch *)kmalloc(sizeof(struct mxt_mode_switch), GFP_ATOMIC);
			if (ms != NULL) {
				ms->data = data;
				ms->mode = (u8)value;
				INIT_WORK(&ms->switch_mode_work, mxt_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				dev_err(&data->client->dev,
					"Failed in allocating memory for mxt_mode_switch!\n");
				return -ENOMEM;
			}
		}
	}

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

static DEVICE_ATTR(update_fw, S_IWUSR | S_IRUSR, mxt_update_fw_show, mxt_update_fw_store);
static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR, mxt_debug_enable_show,
			mxt_debug_enable_store);
static DEVICE_ATTR(pause_driver, S_IWUSR | S_IRUSR, mxt_pause_show,
			mxt_pause_store);
static DEVICE_ATTR(version, S_IRUGO, mxt_version_show, NULL);
static DEVICE_ATTR(build, S_IRUGO, mxt_build_show, NULL);
static DEVICE_ATTR(slowscan_enable, S_IWUSR | S_IRUSR,
			mxt_slowscan_show, mxt_slowscan_store);
static DEVICE_ATTR(self_tune, S_IWUSR, NULL, mxt_self_tune_store);
static DEVICE_ATTR(update_fw_flag, S_IWUSR, NULL, mxt_update_fw_flag_store);
static DEVICE_ATTR(selftest,  S_IWUSR | S_IRUSR, mxt_selftest_show, mxt_selftest_store);
static DEVICE_ATTR(stylus, S_IWUSR | S_IRUSR, mxt_stylus_show, mxt_stylus_store);
static DEVICE_ATTR(diagnostic, S_IWUSR | S_IRUSR, mxt_diagnostic_show, mxt_diagnostic_store);
static DEVICE_ATTR(sensitive_mode, S_IWUSR | S_IRUSR, mxt_sensitive_mode_show, mxt_sensitive_mode_store);
static DEVICE_ATTR(chip_reset, S_IWUSR, NULL, mxt_chip_reset_store);
static DEVICE_ATTR(chg_state, S_IRUGO, mxt_chg_state_show, NULL);
static DEVICE_ATTR(wakeup_mode, S_IWUSR | S_IRUSR, mxt_wakeup_mode_show, mxt_wakeup_mode_store);
static struct attribute *mxt_attrs[] = {
	&dev_attr_update_fw.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_pause_driver.attr,
	&dev_attr_version.attr,
	&dev_attr_build.attr,
	&dev_attr_slowscan_enable.attr,
	&dev_attr_self_tune.attr,
	&dev_attr_update_fw_flag.attr,
	&dev_attr_selftest.attr,
	&dev_attr_stylus.attr,
	&dev_attr_diagnostic.attr,
	&dev_attr_sensitive_mode.attr,
	&dev_attr_chip_reset.attr,
	&dev_attr_chg_state.attr,
	&dev_attr_wakeup_mode.attr,
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static int mxt_disable_hsync_config(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	error = mxt_write_object(data, MXT_SPT_CTECONFIG_T46,
			MXT_CTECONFIG_ADCSPERSYNC, 0);
	if (error) {
		dev_err(dev, "Write to T46 byte %d failed!\n",
				MXT_CTECONFIG_ADCSPERSYNC);
		return error;
	}

	error = mxt_write_object(data, MXT_SPT_SELFCAPCONFIG_T111,
			MXT_SELF_ADCSPERSYNC_INST0, 0);
	if (error) {
		dev_err(dev, "Write to T111 byte %d failed!\n",
				MXT_SELF_ADCSPERSYNC_INST0);
		return error;
	}

	error = mxt_write_object(data, MXT_SPT_SELFCAPCONFIG_T111,
			MXT_SELF_ADCSPERSYNC_INST1, 0);
	if (error) {
		dev_err(dev, "Write to T111 byte %d failed!\n",
				MXT_SELF_ADCSPERSYNC_INST1);
		return error;
	}

	error = mxt_write_object(data, MXT_SPT_SELFCAPCONFIG_T111,
			MXT_SELF_ADCSPERSYNC_INST2, 0);
	if (error) {
		dev_err(dev, "Write to T111 byte %d failed!\n",
				MXT_SELF_ADCSPERSYNC_INST2);
		return error;
	}

	return 0;
}

static void mxt_set_gesture_wake_up(struct mxt_data *data, bool enable)
{
	int error, i;
	struct device *dev = &data->client->dev;
	u8 *t108_val;
	u8 t61_val1, t61_val2;

	/* Set T61 calibration period to 50s in wakeup gesture mode */
	if (enable) {
		t108_val = data->adcperx_wakeup;
		t61_val1 = 0x50;
		t61_val2 = 0xC3;
	} else {
		t108_val = data->adcperx_normal;
		t61_val1 = 0x88;
		t61_val2 = 0x13;
	}

	for (i = 0; i < sizeof(data->adcperx_normal); i++) {
		error = mxt_write_object(data, MXT_PROCG_NOISESUPSELFCAP_T108,
				i + 19, t108_val[i]);
		if (error) {
			dev_err(dev, "write to t108 byte %d failed!\n", i);
			return;
		}
	}

	if (enable) {
		error = mxt_set_clr_reg(data, MXT_PROCG_NOISESUPPRESSION_T72,
					MXT_NOISESUP_CTRL, 0, MXT_NOICTRL_ENABLE);
	} else {
		error = mxt_set_clr_reg(data, MXT_PROCG_NOISESUPPRESSION_T72,
					MXT_NOISESUP_CTRL, MXT_NOICTRL_ENABLE, 0);
	}

	if (error) {
		dev_err(dev, "write to t72 failed!\n");
		return;
	}

	/* Turn off T100 report */
	if (enable) {
		error = mxt_set_clr_reg(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_CTRL, 0, MXT_T100_CTRL_RPTEN);
	} else {
		error = mxt_set_clr_reg(data, MXT_TOUCH_MULTI_T100,
				MXT_MULTITOUCH_CTRL, MXT_T100_CTRL_RPTEN, 0);
	}

	if (error) {
		dev_err(dev, "write to t100 failed!\n");
		return;
	}

	/* Turn off key array report */
	if (enable) {
		error = mxt_set_clr_reg(data, MXT_TOUCH_KEYARRAY_T15,
				MXT_KEYARRAY_CTRL, 0, MXT_KEY_RPTEN);
	} else {
		error = mxt_set_clr_reg(data, MXT_TOUCH_KEYARRAY_T15,
				MXT_KEYARRAY_CTRL, MXT_KEY_RPTEN, 0);
	}

	if (error) {
		dev_err(dev, "write to t15 failed!\n");
		return;
	}

	error = mxt_write_object(data, MXT_SPT_TIMER_T61,
				MXT_TIMER_PERIODLSB, t61_val1);
	error |= mxt_write_object(data, MXT_SPT_TIMER_T61,
				MXT_TIMER_PERIODMSB, t61_val2);
	if (error)
		dev_info(&data->client->dev, "write to t61 failed\n");
}

static void mxt_start(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	if (data->wakeup_gesture_mode) {
		mxt_set_gesture_wake_up(data, false);
		mxt_enable_gesture_mode(data, false);
		if (!data->is_wakeup_by_gesture)
			mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
	} else {
		if (data->is_stopped == 0)
			return;

		error = mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
		if (error)
			return;
		/* At this point, it may be necessary to clear state
		 * by disabling/re-enabling the noise suppression object */

		/* Recalibrate since chip has been in deep sleep */
		schedule_delayed_work(&data->calibration_delayed_work, msecs_to_jiffies(100));
	}

	dev_dbg(dev, "MXT started\n");
}

static void mxt_stop(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	if (data->wakeup_gesture_mode) {
		data->is_wakeup_by_gesture = false;
		mxt_set_power_cfg(data, MXT_POWER_CFG_WAKEUP_GESTURE);
		mxt_set_gesture_wake_up(data, true);
		mxt_enable_gesture_mode(data, true);
	} else {
		if (data->is_stopped)
			return;

		cancel_delayed_work_sync(&data->calibration_delayed_work);
		error = mxt_set_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);

		if (!error)
			dev_dbg(dev, "MXT suspended\n");
	}
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

	mxt_stop(data);
}

static void mxt_clear_touch_event(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	int index = data->current_index;
	int id, i;

	if (!input_dev)
		return;


	for (id = 0; id < data->num_touchids - 2; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
		data->finger_down[id] = false;
	}
	for (i = 0; i < data->pdata->config_array[index].key_num; i++)
		clear_bit(data->pdata->config_array[index].key_codes[i], input_dev->key);

	input_sync(input_dev);
}

static int mxt_set_external_gpio_pullup(struct mxt_data *data, bool pull_up)
{
	int ret = 0;
	u8 value = pull_up ? 0xFF : 0;

	ret = mxt_write_object(data, MXT_SPT_GPIOPWM_T19,
			MXT_GPIOPWM_INTPULLUP, value);
	if (ret)
		dev_err(&data->client->dev, "Unable to pull %s exteral GPIO\n",
			pull_up ? "up" : "down");

	return ret;
}

static int mxt_set_ptc_enabled(struct mxt_data *data, bool enabled)
{
	int ret = 0;
	u8 value = enabled ? 3 : 0;

	ret = mxt_write_object(data, MXT_TOUCH_KEYARRAY_T97,
			MXT_TOUCH_KEYARRAY_INST0_CTRL, value);
	ret |= mxt_write_object(data, MXT_TOUCH_KEYARRAY_T97,
			MXT_TOUCH_KEYARRAY_INST1_CTRL, value);
	ret |= mxt_write_object(data, MXT_TOUCH_KEYARRAY_T97,
			MXT_TOUCH_KEYARRAY_INST2_CTRL, value);

	if (ret)
		dev_err(&data->client->dev, "Unabled to write to T97\n");

	return ret;
}

static void mxt_resume_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct mxt_data *data = container_of(delayed_work, struct mxt_data,
						resume_delayed_work);

	mxt_wait_for_chg(data);
	mxt_enable_irq(data);
	data->is_stopped = 0;
}

static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
	int ret;

	if (data->pdata->cut_off_power) {
		cancel_delayed_work_sync(&data->resume_delayed_work);
		mutex_lock(&input_dev->mutex);
		if (data->is_stopped) {
			mutex_unlock(&input_dev->mutex);
			return 0;
		}

		mxt_disable_irq(data);
		gpio_set_value(data->pdata->reset_gpio, 0);

		mxt_clear_touch_event(data);

		if (data->regulator_vddio) {
			ret = regulator_disable(data->regulator_vddio);
			if (ret < 0)
				dev_err(dev, "regulator disable for vddio failed: %d\n", ret);
		}

		if (data->regulator_vdd) {
			ret = regulator_disable(data->regulator_vdd);
			if (ret < 0)
				dev_err(dev, "regulator disable for vdd failed: %d\n", ret);
		}

		data->is_stopped = 1;
		mutex_unlock(&input_dev->mutex);
	} else {
		if (!data->wakeup_gesture_mode)
			mxt_disable_irq(data);

		mutex_lock(&input_dev->mutex);

		if (input_dev->users)
			mxt_stop(data);

		mutex_unlock(&input_dev->mutex);

		if (data->pdata->use_ptc_key)
			mxt_set_ptc_enabled(data, false);
		mxt_set_external_gpio_pullup(data, true);

		mxt_clear_touch_event(data);
	}

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
	int ret;

	if (data->pdata->cut_off_power) {
		mutex_lock(&input_dev->mutex);
		if (!data->is_stopped) {
			mutex_unlock(&input_dev->mutex);
			return 0;
		}

		if (data->regulator_vddio) {
			ret = regulator_enable(data->regulator_vddio);
			if (ret < 0)
				dev_err(dev, "regulator enable for vddio failed: %d\n", ret);
		}

		if (data->regulator_vdd) {
			ret = regulator_enable(data->regulator_vdd);
			if (ret < 0)
				dev_err(dev, "regulator enable for vdd failed: %d\n", ret);
		}

		mdelay(1);
		gpio_set_value(data->pdata->reset_gpio, 1);
		mutex_unlock(&input_dev->mutex);

		schedule_delayed_work(&data->resume_delayed_work, msecs_to_jiffies(100));
	} else {
		if (!data->wakeup_gesture_mode)
			mxt_enable_irq(data);

		if (data->pdata->use_ptc_key)
			mxt_set_ptc_enabled(data, true);
		mxt_set_external_gpio_pullup(data, false);

		mutex_lock(&input_dev->mutex);

		if (input_dev->users)
			mxt_start(data);

		mutex_unlock(&input_dev->mutex);
	}

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
	int index = data->current_index;

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
	input_dev->event = mxt_input_event;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	/* For multi touch */
	input_mt_init_slots(input_dev,
		data->num_touchids + data->num_stylusids, 0);
	if (data->t100_tchaux_bits &  MXT_T100_AREA) {
		dev_info(dev, "report area\n");
		input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				     0, MXT_MAX_AREA, 0, 0);
	}
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
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

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		__set_bit(BTN_STYLUS, input_dev->keybit);
		__set_bit(BTN_STYLUS2, input_dev->keybit);

		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For wakeup gesture */
	if (data->pdata->config_array[index].wakeup_gesture_support) {
		input_set_capability(input_dev, EV_KEY, KEYCODE_WAKEUP);
		__set_bit(KEYCODE_WAKEUP, input_dev->keybit);
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

static int mxt_initialize_pinctrl(struct mxt_data *data)
{
	int ret = 0;
	struct device *dev = &data->client->dev;

	data->ts_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		dev_err(dev, "Target does not use pinctrl\n");
		ret = PTR_ERR(data->ts_pinctrl);
		data->ts_pinctrl = NULL;
		return ret;
	}

	data->gpio_state_active
		= pinctrl_lookup_state(data->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(data->gpio_state_active)) {
		dev_err(dev, "Can not get ts default pinstate\n");
		ret = PTR_ERR(data->gpio_state_active);
		data->ts_pinctrl = NULL;
		return ret;
	}

	data->gpio_state_suspend
		= pinctrl_lookup_state(data->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(data->gpio_state_suspend)) {
		dev_err(dev, "Can not get ts sleep pinstate\n");
		ret = PTR_ERR(data->gpio_state_suspend);
		data->ts_pinctrl = NULL;
		return ret;
	}

	return 0;
}

static int mxt_pinctrl_select(struct mxt_data *data, bool on)
{
	int ret = 0;
	struct pinctrl_state *pins_state;
	struct device *dev = &data->client->dev;

	pins_state = on ? data->gpio_state_active : data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(data->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(dev, "can not set %s pins\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else {
		dev_err(dev, "not a valid '%s' pinstate\n",
			on ? "pmx_ts_active" : "pmx_ts_suspend");
	}

	return ret;
}


#ifdef CONFIG_FB
static int fb_notifier_cb(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct mxt_data *mxt_data =
		container_of(self, struct mxt_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && mxt_data) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			dev_dbg(&mxt_data->client->dev, "##### UNBLANK SCREEN #####\n");
			mxt_input_enable(mxt_data->input_dev);
#ifdef TOUCH_WAKEUP_EVENT_RECORD
				if (atomic_read(&wakeup_flag) == 1)
					wakeup_event_record_write(EVENT_SCREEN_ON);
#endif
		} else if (*blank == FB_BLANK_POWERDOWN) {
			dev_dbg(&mxt_data->client->dev, "##### BLANK SCREEN #####\n");
			mxt_input_disable(mxt_data->input_dev);
#ifdef TOUCH_WAKEUP_EVENT_RECORD
				if (atomic_read(&wakeup_flag) == 1) {
					wakeup_event_record_write(EVENT_SCREEN_OFF);
					atomic_set(&wakeup_flag, 0);
				}
#endif
		}
	}

	return 0;
}

static void configure_sleep(struct mxt_data *data)
{
	int ret;

	data->fb_notif.notifier_call = fb_notifier_cb;
	ret = fb_register_client(&data->fb_notif);
	if (ret) {
		dev_err(&data->client->dev,
			"Unable to register fb_notifier, err: %d\n", ret);
	}
}
#else
static void configure_sleep(struct mxt_data *data)
{
	data->input_dev->enable = mxt_input_enable;
	data->input_dev->disable = mxt_input_disable;
	data->input_dev->enabled = true;
}
#endif


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

static void mxt_debugfs_init(struct mxt_data *data)
{
	debug_base = debugfs_create_dir(MXT_DEBUGFS_DIR, NULL);
	if (IS_ERR_OR_NULL(debug_base))
		dev_err(&data->client->dev, "atmel_mxt_ts: Failed to create debugfs dir\n");
	if (IS_ERR_OR_NULL(debugfs_create_file(MXT_DEBUGFS_FILE,
					       0444,
					       debug_base,
					       data,
					       &mxt_object_fops))) {
		dev_err(&data->client->dev, "atmel_mxt_ts: Failed to create object file\n");
		debugfs_remove_recursive(debug_base);
	}
}

static void mxt_dump_value(struct device *dev, struct mxt_platform_data *pdata)
{
	int i = 0;

	dev_info(dev, "ATMEL DEVICE TREE:\n");
	dev_info(dev, "reset gpio= %d\n", pdata->reset_gpio);
	dev_info(dev, "irq gpio= %d\n", pdata->irq_gpio);
	dev_info(dev, "fw name = %s\n", pdata->mxt_fw_name);
	dev_info(dev, "config size = %zd\n", pdata->config_array_size);
	dev_info(dev, "gpio mask = 0x%x\n", pdata->gpio_mask);
	dev_info(dev, "default config = %d\n", pdata->default_config);
	dev_info(dev, "default panel id = %d\n", pdata->default_panel_id);
	dev_info(dev, "use ptc key = %d\n", pdata->use_ptc_key);
	dev_info(dev, "cut off power = %d\n", pdata->cut_off_power);

	for (i = 0; i < pdata->config_array_size; i++) {
		dev_info(dev, "config[%d]: family_id = 0x%x\n", i, pdata->config_array[i].family_id);
		dev_info(dev, "config[%d]: variant_id = 0x%x\n", i, pdata->config_array[i].variant_id);
		dev_info(dev, "config[%d]: version = 0x%x\n", i, pdata->config_array[i].version);
		dev_info(dev, "config[%d]: build = 0x%x\n", i, pdata->config_array[i].build);
		dev_info(dev, "config[%d]: mxt_cfg_name = %s\n", i, pdata->config_array[i].mxt_cfg_name);
		dev_info(dev, "config[%d]: vendor_id = 0x%x\n", i, pdata->config_array[i].vendor_id);
		dev_info(dev, "config[%d]: panel_id = 0x%x\n", i, pdata->config_array[i].panel_id);
		dev_info(dev, "config[%d]: rev_id = 0x%x\n", i, pdata->config_array[i].rev_id);
		dev_info(dev, "config[%d]: wakeup self adcx = 0x%x\n", i, pdata->config_array[i].wake_up_self_adcx);
	}
	dev_info(dev, "END OF ATMEL DEVICE TREE\n");
}

#ifdef CONFIG_OF
static int mxt_parse_dt(struct device *dev, struct mxt_platform_data *pdata)
{
	int ret;
	struct mxt_config_info *info;
	struct device_node *temp, *np = dev->of_node;
	struct property *prop;
	u32 temp_val;

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "atmel,reset-gpio",
				0, &pdata->reset_gpio_flags);
	pdata->irq_gpio = of_get_named_gpio_flags(np, "atmel,irq-gpio",
				0, &pdata->irq_gpio_flags);
	ret = of_property_read_u32(np, "atmel,irqflags", &temp_val);
	if (ret) {
		dev_err(dev, "Unable to read irqflags id\n");
	} else
		pdata->irqflags = temp_val;

	ret = of_property_read_string(np, "atmel,mxt-fw-name",
			&pdata->mxt_fw_name);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
	}

	pdata->cut_off_power = of_property_read_bool(np, "atmel,cut-off-power");

	pdata->use_ptc_key = of_property_read_bool(np, "atmel,use-ptc-key");

	ret = of_property_read_u32(np, "atmel,gpio-mask", (u32 *)&temp_val);
	if (ret)
		dev_err(dev, "Unable to read gpio mask\n");
	else
		pdata->gpio_mask = (u8)temp_val;

	ret = of_property_read_u32(np, "atmel,config-array-size", (u32 *)&pdata->config_array_size);
	if (ret) {
		dev_err(dev, "Unable to get array size\n");
	}

	ret = of_property_read_u32(np, "atmel,default-config", &pdata->default_config);
	if (ret) {
		dev_err(dev, "Unable to get default config\n");
		pdata->default_config = -1;
	}

	ret = of_property_read_u32(np, "atmel,default-panel-id",
			&pdata->default_panel_id);
	if (ret)
		dev_dbg(dev, "No default panel id\n");

	pdata->config_array = devm_kzalloc(dev, pdata->config_array_size *
					sizeof(struct mxt_config_info), GFP_KERNEL);
	if (!pdata->config_array) {
		dev_err(dev, "Unable to allocate memory\n");
	}

	info = pdata->config_array;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_u32(temp, "atmel,family-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read family id\n");
		} else
			info->family_id = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,variant-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read variant id\n");
		} else
			info->variant_id = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,version", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read version\n");
		} else
			info->version = (u8)temp_val;
		ret = of_property_read_u32(temp, "atmel,build", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read build\n");
		} else
			info->build = (u8)temp_val;
		ret = of_property_read_string(temp, "atmel,mxt-cfg-name",
			&info->mxt_cfg_name);
		if (ret && (ret != -EINVAL)) {
			dev_err(dev, "Unable to read cfg name\n");
		}
		ret = of_property_read_u32(temp, "atmel,vendor-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read vendor id\n");
		} else
			info->vendor_id = (u8)temp_val;

		ret = of_property_read_u32(temp, "atmel,panel-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read panel id\n");

		} else
			info->panel_id = (u8)temp_val;

		ret = of_property_read_u32(temp, "atmel,rev-id", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read rev id\n");
		} else
			info->rev_id = (u8)temp_val;

		prop = of_find_property(temp, "atmel,key-codes", NULL);
		if (prop) {
			info->key_num = prop->length / sizeof(u32);
			info->key_codes = devm_kzalloc(dev,
					sizeof(int) * info->key_num,
					GFP_KERNEL);
			if (!info->key_codes)
				return -ENOMEM;
			ret = of_property_read_u32_array(temp, "atmel,key-codes",
					info->key_codes, info->key_num);
			if (ret) {
				dev_err(dev, "Unable to read key codes\n");
			}
		}

		ret = of_property_read_u32(temp, "atmel,selfintthr-stylus", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read selfintthr-stylus\n");
		} else
			info->selfintthr_stylus = temp_val;
		ret = of_property_read_u32(temp, "atmel,t71-tchthr-pos", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read t71-glove-ctrl-reg\n");
		} else
			info->t71_tchthr_pos = temp_val;
		ret = of_property_read_u32(temp, "atmel,self-chgtime-min", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read self-chgtime-min\n");
		} else
			info->self_chgtime_min = temp_val;
		ret = of_property_read_u32(temp, "atmel,self-chgtime-max", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read self-chgtime-max\n");
		} else
			info->self_chgtime_max = temp_val;
		ret = of_property_read_u32(temp, "atmel,mult-intthr-sensitive", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read mult-intthr-sensitive\n");
		} else
			info->mult_intthr_sensitive = temp_val;
		ret = of_property_read_u32(temp, "atmel,mult-intthr-not-sensitive", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read mult-intthr-not-sensitive\n");
		} else
			info->mult_intthr_not_sensitive = temp_val;
		ret = of_property_read_u32(temp, "atmel,atchthr-sensitive", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read mult-intthr-not-sensitive\n");
		} else
			info->atchthr_sensitive = temp_val;
		ret = of_property_read_u32(temp, "atmel,mult-tchthr-sensitive", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read mult-tchthr-sensitive\n");
		} else
			info->mult_tchthr_sensitive = temp_val;
		ret = of_property_read_u32(temp, "atmel,mult-tchthr-not-sensitive", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read mult-tchthr-not-sensitive\n");
		} else
			info->mult_tchthr_not_sensitive = temp_val;

		ret = of_property_read_u32(temp, "atmel,wake-up-self-adcx", &temp_val);
		if (ret) {
			dev_err(dev, "Unable to read wake-up-self-adcx\n");
		} else
			info->wake_up_self_adcx = (u8)temp_val;

		info->wakeup_gesture_support = of_property_read_bool(temp,
				"atmel,support-wakeup-gesture");

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
#if CTP_PROC_INTERFACE
static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char *ptr = buf;
	if (*ppos) {
		CTP_INFO("tp test again return\n");
		return 0;
	}
	*ppos += count;

	return  sprintf(ptr, "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				lockdown_info[0], lockdown_info[1], lockdown_info[2], lockdown_info[3],
				lockdown_info[4], lockdown_info[5], lockdown_info[6], lockdown_info[7]);
}

static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}

static ssize_t ctp_open_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct mxt_data *data = cmcs_data;
	int error;
	u8 selftest_cmd;
	u8 result = 0;
	char *ptr = buf;
	if (*ppos) {
		CTP_INFO("tp test again return\n");
		return 0;
	}
	*ppos += count;

	/* run all selftest */
	CTP_INFO("TP Test Begin.\n");
	error = mxt_write_object(data, MXT_SPT_SELFTEST_T25, 0x01, 0xfe);
	if (!error) {
		while (true) {
			msleep(10);
			error = mxt_read_object(data,
					MXT_SPT_SELFTEST_T25,
					0x01, &selftest_cmd);
			if (error || selftest_cmd == 0) {
				CTP_INFO("TP Test End.\n");
				break;
			}
		}
	}

	mdelay(3000);
	CTP_INFO("%02X, %02X, %02X, %02X, %02X, %02X\n", data->test_result[0], data->test_result[1], data->test_result[2], data->test_result[3], data->test_result[4], data->test_result[5]);
	if (data->test_result[0] == 0xfe) {
		result = 1;
	} else {
		result = 0;
	}

	return  sprintf(ptr, "result=%d\n", result);
}
static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}

static void create_ctp_proc(void)
{
	struct proc_dir_entry *ctp_device_proc = NULL;
	struct proc_dir_entry *ctp_lockdown_proc = NULL;
	struct proc_dir_entry *ctp_open_proc = NULL;

	ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
	if (ctp_device_proc == NULL) {
		CTP_ERROR("atmel_mxt: create parent_proc fail\n");
		return;
	}

	ctp_lockdown_proc = proc_create(CTP_LOCKDOWN_INFOR_NAME, 0666, ctp_device_proc, &ctp_lockdown_procs_fops);
	if (ctp_lockdown_proc == NULL)
		CTP_ERROR("atmel_mxt: create ctp_lockdown_proc fail\n");

	ctp_open_proc = proc_create(CTP_OPEN_PROC_NAME, 0666, ctp_device_proc, &ctp_open_procs_fops);
	if (ctp_open_proc == NULL)
		CTP_ERROR("ft5x06: create open_proc fail\n");

}
#endif

static int mxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mxt_platform_data *pdata;
	struct mxt_data *data;
	int error;
	unsigned char val;

	CTP_DEBUG("step 1: parse dts. ");
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
	cmcs_data = data;

	CTP_DEBUG("step 2: Power on . ");
	error = mxt_initialize_pinctrl(data);
	if (error || !data->ts_pinctrl) {
		dev_err(&client->dev, "Initialize pinctrl failed\n");
		goto	err_pinctrl_sleep;
	} else {
		error = mxt_pinctrl_select(data, true);
		if (error < 0) {
			dev_err(&client->dev, "pinctrl_select failed\n");
			goto	err_pinctrl_sleep;
		}
	}

	CTP_DEBUG("step 3 : reset IC.");
	if (gpio_is_valid(pdata->irq_gpio)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->irq_gpio, "mxt_irq_gpio");
		if (error) {
			dev_err(&client->dev, "unable to request gpio [%d]\n",
				pdata->irq_gpio);
			goto err_reset_gpio_req;
		}
		error = gpio_direction_input(pdata->irq_gpio);
		if (error) {
			dev_err(&client->dev, "unable to set_direction for gpio [%d]\n",
				pdata->irq_gpio);
			goto err_irq_gpio_req;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->reset_gpio, "mxt_reset_gpio");
		if (error) {
			dev_err(&client->dev, "unable to request reset gpio %d\n",
				pdata->reset_gpio);
			goto err_irq_gpio_req;
		}
		error = gpio_direction_output(pdata->reset_gpio, 0);
		if (error) {
			dev_err(&client->dev, "unable to set direction for gpio %d\n",
				pdata->reset_gpio);
			goto err_reset_gpio_req;
		}

		mdelay(10);
		error = gpio_direction_output(pdata->reset_gpio, 1);
		if (error) {
			dev_err(&client->dev, "unable to set direction for gpio %d\n",
				pdata->reset_gpio);
			goto err_reset_gpio_req;
		}
	}

	mdelay(100);
	i2c_set_clientdata(data->client, data);
	mdelay(10);
	mxt_wait_for_chg(data);
	INIT_DELAYED_WORK(&data->calibration_delayed_work,
				mxt_calibration_delayed_work);
	INIT_DELAYED_WORK(&data->resume_delayed_work,
				mxt_resume_delayed_work);

		CTP_DEBUG("step 4 : test IC I2C.");
	/* Initialize i2c device */
	error = mxt_initialize(data);

	CTP_DEBUG("step 5 : register input device.");
	error = mxt_initialize_input_device(data);
	if (error)
		goto err_free_object;


	configure_sleep(data);

	error = request_threaded_irq(client->irq, NULL, mxt_interrupt,
			pdata->irqflags, client->dev.driver->name, data);
	if (error) {
		dev_err(&client->dev, "Error %d registering irq\n", error);
		goto err_free_input_device;
	}
	data->irq_enabled = true;

	device_init_wakeup(&client->dev, 1);

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

	/* Create sysfs bin files for BIST */
	sysfs_bin_attr_init(&data->self_ref_attr);
	data->self_ref_attr.attr.name = "self_ref";
	data->self_ref_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->self_ref_attr.read = mxt_self_ref_read;
	data->self_ref_attr.write = mxt_self_ref_write;
	data->self_ref_attr.size = MXT_REF_ATTR_SIZE;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->self_ref_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->self_ref_attr.attr.name);
		goto err_remove_mem_access_attr;
	}

	sysfs_bin_attr_init(&data->mutual_ref_attr);
	data->mutual_ref_attr.attr.name = "mutual_ref";
	data->mutual_ref_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->mutual_ref_attr.read = mxt_mutual_ref_read;
	data->mutual_ref_attr.write = mxt_mutual_ref_write;
	data->mutual_ref_attr.size = MXT_REF_ATTR_SIZE;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mutual_ref_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mutual_ref_attr.attr.name);
		goto err_remove_self_ref_attr;
	}

	mxt_debugfs_init(data);
	/* Update hardware info - Touch IC: Atmel */

	error =  mxt_read_object(data, MXT_SPT_USERDATA_T38, MXT_CFG_VERSION, &val);
		if (error) {
			dev_err(&client->dev, "Failed to get CFG version.\n");
		} else {
			dev_err(&client->dev, "CFG version is %d\n", val);
		}


#if CTP_PROC_INTERFACE
	create_ctp_proc();
#endif
	CTP_DEBUG("Atmel Probe done");
	return 0;

err_remove_self_ref_attr:
	sysfs_remove_bin_file(&client->dev.kobj, &data->self_ref_attr);
err_remove_mem_access_attr:
	sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_free_irq:
	free_irq(client->irq, data);
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

err_pinctrl_sleep:
	if (data->ts_pinctrl) {
		if (mxt_pinctrl_select(data, false) < 0)
			dev_err(&client->dev, "Cannot get idle pinctrl state\n");
	}
/*
err_free_regulator:
	mxt_configure_regulator(data, false);
err_free_data:
*/
	kfree(data);

	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);
	const struct mxt_platform_data *pdata = data->pdata;

#ifdef TOUCH_WAKEUP_EVENT_RECORD
	wakeup_event_record_exit();
#endif
	cancel_delayed_work(&data->calibration_delayed_work);
	cancel_delayed_work(&data->resume_delayed_work);
	sysfs_remove_bin_file(&client->dev.kobj, &data->mutual_ref_attr);
	sysfs_remove_bin_file(&client->dev.kobj, &data->self_ref_attr);
	sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data->msg_buf);
	data->msg_buf = NULL;
	kfree(data->object_table);
	data->object_table = NULL;

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

	mxt_disable_irq(data);
	data->state = SHUTDOWN;
}

#ifdef CONFIG_PM
static int mxt_ts_suspend(struct device *dev)
{
	struct mxt_data *data =  dev_get_drvdata(dev);

	if (device_may_wakeup(dev) &&
			data->wakeup_gesture_mode) {
		dev_info(dev, "touch enable irq wake\n");
		mxt_disable_irq(data);
		enable_irq_wake(data->client->irq);
	}

	return 0;
}

static int mxt_ts_resume(struct device *dev)
{
	struct mxt_data *data =  dev_get_drvdata(dev);

	if (device_may_wakeup(dev) &&
			data->wakeup_gesture_mode) {
		dev_info(dev, "touch disable irq wake\n");
		disable_irq_wake(data->client->irq);
		mxt_enable_irq(data);
	}

	return 0;
}

static const struct dev_pm_ops mxt_touchscreen_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = mxt_ts_suspend,
	.resume         = mxt_ts_resume,
#endif
};
#endif

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "mXT224", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

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
#ifdef CONFIG_PM
		.pm = &mxt_touchscreen_pm_ops,
#endif
	},
	.probe		= mxt_probe,
	.remove		= mxt_remove,
	.shutdown	= mxt_shutdown,
	.id_table	= mxt_id,
};

static int __init mxt_init(void)
{
	CTP_DEBUG("mxt_init.");
	return i2c_add_driver(&mxt_driver);
}

static void __exit mxt_exit(void)
{
	i2c_del_driver(&mxt_driver);
}

late_initcall(mxt_init);
module_exit(mxt_exit);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");

