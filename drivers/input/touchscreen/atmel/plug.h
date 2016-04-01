#ifndef __LINUX_ATMEL_MXT_PLUG
#define __LINUX_ATMEL_MXT_PLUG

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/kthread.h>

#include "atomic_op.h"

#define CONFIG_MXT_MISC_WORKAROUND
#define CONFIG_MXT_WDG_WORKAROUND
#define CONFIG_MXT_CAL_TRIGGER_CAL_WHEN_CFG_MATCH
#define CONFIG_MXT_T71_RESET_WATCHDOG
#define CONFIG_MXT_UPDATE_BY_OBJECT

#include "ts_key.h"
#if defined(CONFIG_MXT_CAL_T37_WORKAROUND)
#define CONFIG_MXT_AC_T72_WORKAROUND
#include "plugin_cal_t37.h"
#endif

#if defined(CONFIG_MXT_AC_T72_WORKAROUND)
#include "plugin_ac_t72.h"
#endif

#if defined(CONFIG_MXT_PROCI_PI_WORKAROUND)
#include "plugin_proci.h"
#endif

#if defined(CONFIG_MXT_MISC_WORKAROUND)
#include "plugin_misc.h"
#endif

#if defined(CONFIG_MXT_CLIP_WORKAROUND)
#include "plugin_clip.h"
#endif

#if (defined(CONFIG_MXT_CAL_T37_WORKAROUND) || \
	defined(CONFIG_MXT_AC_T72_WORKAROUND) || \
	defined(CONFIG_MXT_PROCI_PI_WORKAROUND) || \
	defined(CONFIG_MXT_MISC_WORKAROUND) || \
	defined(CONFIG_MXT_CLIP_WORKAROUND) || \
	defined(CONFIG_MXT_WDG_WORKAROUND))
#define CONFIG_MXT_PLUGIN_SUPPORT
#define CONFIG_MXT_IRQ_WORKQUEUE
#endif

#define T61_TIMER_ID_WDG 4
#define T61_TIMER_ID_PI 5

#define MXT_PAGE_SIZE 64

#define DBG_LEVEL 1

#if (DBG_LEVEL == 0)
#define dev_dbg2 dev_dbg
#define dev_info2 dev_dbg
#elif (DBG_LEVEL == 1)
#define dev_dbg2 dev_dbg
#define dev_info2 dev_info
#else
#define dev_dbg2 dev_info
#define dev_info2 dev_info
#endif

enum {
	MXT_RESERVED_T0 = 0,
	MXT_RESERVED_T1,
	MXT_DEBUG_DELTAS_T2,
	MXT_DEBUG_REFERENCES_T3,
	MXT_DEBUG_SIGNALS_T4,
	MXT_GEN_MESSAGEPROCESSOR_T5,
	MXT_GEN_COMMANDPROCESSOR_T6,
	MXT_GEN_POWERCONFIG_T7,
	MXT_GEN_ACQUISITIONCONFIG_T8,
	MXT_TOUCH_MULTITOUCHSCREEN_T9,
	MXT_TOUCH_SINGLETOUCHSCREEN_T10,
	MXT_TOUCH_XSLIDER_T11,
	MXT_TOUCH_YSLIDER_T12,
	MXT_TOUCH_XWHEEL_T13,
	MXT_TOUCH_YWHEEL_T14,
	MXT_TOUCH_KEYARRAY_T15,
	MXT_PROCG_SIGNALFILTER_T16,
	MXT_PROCI_LINEARIZATIONTABLE_T17,
	MXT_SPT_COMCONFIG_T18,
	MXT_SPT_GPIOPWM_T19,
	MXT_PROCI_GRIPFACESUPPRESSION_T20,
	MXT_RESERVED_T21,
	MXT_PROCG_NOISESUPPRESSION_T22,
	MXT_TOUCH_PROXIMITY_T23,
	MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24,
	MXT_SPT_SELFTEST_T25,
	MXT_DEBUG_CTERANGE_T26,
	MXT_PROCI_TWOTOUCHGESTUREPROCESSOR_T27,
	MXT_SPT_CTECONFIG_T28,
	MXT_SPT_GPI_T29,
	MXT_SPT_GATE_T30,
	MXT_TOUCH_KEYSET_T31,
	MXT_TOUCH_XSLIDERSET_T32,
	MXT_RESERVED_T33,
	MXT_GEN_MESSAGEBLOCK_T34,
	MXT_SPT_GENERICDATA_T35,
	MXT_RESERVED_T36,
	MXT_DEBUG_DIAGNOSTIC_T37,
	MXT_SPT_USERDATA_T38,
	MXT_SPARE_T39,
	MXT_PROCI_GRIPSUPPRESSION_T40,
	MXT_SPARE_T41,
	MXT_PROCI_TOUCHSUPPRESSION_T42,
	MXT_SPT_DIGITIZER_T43,
	MXT_SPARE_T44,
	MXT_SPARE_T45,
	MXT_SPT_CTECONFIG_T46,
	MXT_PROCI_STYLUS_T47,
	MXT_PROCG_NOISESUPPRESSION_T48,
	MXT_SPARE_T49,
	MXT_SPARE_T50,
	MXT_SPARE_T51,
	MXT_TOUCH_PROXIMITY_KEY_T52,
	MXT_GEN_DATASOURCE_T53,
	MXT_SPARE_T54,
	MXT_ADAPTIVE_T55,
	MXT_PROCI_SHIELDLESS_T56,
	MXT_PROCI_EXTRATOUCHSCREENDATA_T57,
	MXT_SPARE_T58,
	MXT_SPARE_T59,
	MXT_SPARE_T60,
	MXT_SPT_TIMER_T61,
	MXT_PROCG_NOISESUPPRESSION_T62,
	MXT_PROCI_ACTIVESTYLUS_T63,
	MXT_SPARE_T64,
	MXT_PROCI_LENSBENDING_T65,
	MXT_SPT_GOLDENREFERENCES_T66,
	MXT_SPARE_T67,
	MXT_SPARE_T68,
	MXT_PROCI_PALMGESTUREPROCESSOR_T69,
	MXT_SPT_DYNAMICCONFIGURATIONCONTROLLER_T70,
	MXT_SPT_DYNAMICCONFIGURATIONCONTAINER_T71,
	MXT_PROCG_NOISESUPPRESSION_T72,
	MXT_PROCI_GLOVEDETECTION_T78 = 78,
	MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80 = 80,
	MXT_PROCI_UNLOCKGESTURE_T81,
	MXT_PROCI_GESTURE_T92 = 92,
	MXT_PROCI_TOUCHSEQUENCELOGGER_T93,
	MXT_TOUCH_SPT_PTC_TUNINGPARAMS_T96 = 96,
	MXT_TOUCH_PTC_KEYS_T97,
	MXT_PROCI_KEYGESTUREPROCESSOR_T99 = 99,
	MXT_TOUCH_MULTITOUCHSCREEN_T100,
	MXT_SPT_TOUCHSCREENHOVER_T101,
	MXT_SPT_SELFCAPHOVERCTECONFIG_T102,
	MXT_PROCI_AUXTOUCHCONFIG_T104 = 104,
	MXT_PROCG_NOISESUPSELFCAP_T108 = 108,
	MXT_SPT_SELFCAPGLOBALCONFIG_T109,
	MXT_SPT_SELFCAPTUNINGPARAMS_T110,
	MXT_SPT_SELFCAPCONFIG_T111,
	MXT_SPT_SELFCAPMEASURECONFIG_T113 = 113,
	MXT_PROCI_SYMBOLGESTURE_T115 = 115,
	MXT_SPT_SYMBOLGESTURECONFIG_T116,
	MXT_RESERVED_T255 = 255,
};

/* Define for T6 command */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* Define for T6 debug mode command */
#define MXT_T6_DEBUG_PAGEUP	0x1
#define MXT_T6_DEBUG_PAGEDOWN	0x2
#define MXT_T6_DEBUG_DELTA	0x10
#define MXT_T6_DEBUG_REF	0x11
#define MXT_T6_DEBUG_DELTA_PTC	0x14
#define MXT_T6_DEBUG_REF_PTC		0x15
#define MXT_T6_DEBUG_SE	0x33
#define MXT_T6_DEBUG_GESTRUE	0x34
#define MXT_T6_DEBUG_PID	0x81
#define MXT_T6_DEBUG_DELTA_SC	0xF7

/* Define for T6 status byte */
#define MXT_T6_STATUS_RESET	(1 << 7)
#define MXT_T6_STATUS_OFL	(1 << 6)
#define MXT_T6_STATUS_SIGERR	(1 << 5)
#define MXT_T6_STATUS_CAL	(1 << 4)
#define MXT_T6_STATUS_CFGERR	(1 << 3)
#define MXT_T6_STATUS_COMSERR	(1 << 2)

struct diagnostic_info {
	u8 cmd;
	s8 page;
	u8 index;
	u8 num;
};

struct diagnostic_block{
	struct diagnostic_info diag;
	int step;
	int max_step;
	int curr;
	int ext;

	u8 *buf;
	int interval;
	int count;
};
/* MXT_GEN_ACQUIRE_T8 field */
enum t8_status{
	T8_HALT,
	T8_NORMAL,
	T8_NOISE,
	T8_VERY_NOISE,
	T8_MIDDLE,
	T8_WEAK_PALM,
};

struct t8_config {
	u8 atchcalst;
	u8 atchcalsthr;
	u8 atchfrccalthr;
	s8 atchfrccalratio;
	u8 tchautocal;
} __packed;

#define MXT_T8_CHRGTIME	0
#define MXT_T8_TCHDRIFT	2
#define MXT_T8_DRIFTST	3
#define MXT_T8_TCHAUTOCAL	4
#define MXT_T8_SYNC	5
#define MXT_T8_ATCHCALST	6
#define MXT_T8_ATCHCALSTHR	7
#define MXT_T8_ATCHFRCCALTHR		8
#define MXT_T8_ATCHFRCCALRATIO	9

enum t8_cfg_set_option{
	MXT_T8_MASK_ATCHCALST = 0,
	MXT_T8_MASK_ATCHCALSTHR,
	MXT_T8_MASK_ATCHFRCCALTHR,
	MXT_T8_MASK_ATCHFRCCALRATIO,
	MXT_T8_MASK_TCHAUTOCAL,
};


/* MXT_TOUCH_MULTI_T9 field */
enum t9_t100_status{
	T9_T100_NORMAL,
	T9_T100_THLD_NOISE,
	T9_T100_THLD_VERY_NOISE,
	T9_T100_NORMAL_STEP1,
	T9_T100_THLD_NOISE_STEP1,
	T9_T100_THLD_VERY_NOISE_STEP1,
	T9_T100_SINGLE_TOUCH,
	T9_T100_CONFIG_NUM,
};

enum t9_t100_cfg_set_option{
	MXT_T9_T100_MASK_TCHHR = 0,
	MXT_T9_T100_MASK_TCHHYST,
	MXT_T9_T100_MASK_MRGTHR,
	MXT_T9_T100_MASK_MRGHYST,
	MXT_T9_T100_MASK_N_TOUCH,
};

#define T9_T100_CTRL_ENABLE	(1<<0)
#define T9_T100_CTRL_REPEN	(1<<1)

struct t9_t100_config {
	u8 threshold;
	u8 hysterisis;

	u8 internal_threshold;
	u8 internal_hysterisis;

	u8 merge_threshold;
	u8 merge_hysterisis;
	u8 num_touch;
	u8 x0;
	u8 y0;
	u8 xsize;
	u8 ysize;
	u8 dualx_threshold;
} __packed;

#define MXT_T9_CTRL			0
#define MXT_T9_XORIGN		1
#define MXT_T9_YORIGN		2
#define MXT_T9_XSIZE		3
#define MXT_T9_YSIZE		4
#define MXT_T9_TCHHR		7
#define MXT_T9_ORIENT		9
#define MXT_T9_NUMTOUCH		14
#define MXT_T9_MRGHYST		15
#define MXT_T9_MRGTHR		16
#define MXT_T9_RANGE		18
#define MXT_T9_TCHHYST		31
#define MXT_T9_DUALX_THLD	42

/* MXT_TOUCH_MULTI_T9 status */
#define MXT_T9_UNGRIP		(1 << 0)
#define MXT_T9_SUPPRESS		(1 << 1)
#define MXT_T9_AMP		(1 << 2)
#define MXT_T9_VECTOR		(1 << 3)
#define MXT_T9_MOVE		(1 << 4)
#define MXT_T9_RELEASE		(1 << 5)
#define MXT_T9_PRESS		(1 << 6)
#define MXT_T9_DETECT		(1 << 7)
/*
struct t9_range {
	u16 x;
	u16 y;
} __packed;
*/
/* MXT_TOUCH_MULTI_T9 orient */
#define MXT_T9_ORIENT_SWITCH	(1 << 0)

/* T15 Key array */
struct t15_config {
	u8 threshold;
	u8 hysterisis;
	u8 x0;
	u8 y0;
	u8 xsize;
	u8 ysize;
} __packed;

#define MXT_T15_XORIGN		1
#define MXT_T15_YORIGN		2
#define MXT_T15_XSIZE		3
#define MXT_T15_YSIZE		4
#define MXT_T15_TCHHR		7

/* Define for MXT_SPT_USERDATA_T38 */

struct t38_config {
	u8 data[8];
} __packed;

#define MXT_T38_MAGIC_WORD	0x92
#define MXT_T38_OFFICIAL_RESERVED   0
#define MXT_T38_CONFIG_VERSION_0   1
#define MXT_T38_CONFIG_VERSION_1   2
#define MXT_T38_BLOCK_LOW_LIMIT_LEVEL		3
#define MXT_T38_BLOCK_HIGH_LIMIT_LEVEL		4
#define MXT_T38_T9_T100_THLD_NORMAL_STEP1	5
#define MXT_T38_T9_T100_THLD_NOISE		6
#define MXT_T38_MGWD				7

/* Define for MXT_PROCI_GRIPSUPPRESSION_T40 */
#define MXT_GRIP_CTRL			0
#define MXT_GRIP_XLOCRIP		1
#define MXT_GRIP_XHICRIP		2
#define MXT_GRIP_YLOCRIP		3
#define MXT_GRIP_YHICRIP		4

/* T40 reg array */
struct t40_config {
	u8 ctrl;
	u8 xlow;
	u8 xhigh;
	u8 ylow;
	u8 yhigh;
} __packed;

/* T42 reg array */
#define MXT_T42_CTRL		0

enum t42_status{
	T42_NORMAL,
	T42_DISABLE,
	T42_CONFIG_NUM,
};

struct t42_config {
	u8 ctrl;
} __packed;

enum t42_cfg_set_option{
	MXT_T42_MASK_CTRL = 0,
};

/* T55 reg array */
#define MXT_T55_CTRL			0
#define MXT_T55_TARGETTHR		1
#define MXT_T55_THRADJLIM		2
#define MXT_T55_RESETSTEPTIME		3
#define MXT_T55_FORCECHGDIST		4
#define MXT_T55_FORCECHGTIME		5

#define MXT_T55_CTRL_EN		(1<<0)

enum t55_status{
	T55_NORMAL,
	T55_DISABLE,
	T55_CONFIG_NUM,
};

struct t55_config {
	u8 ctrl;
	/*
	u8 tthld;
	u8 tlimit;
	u8 rtime;
	u8 adjdistance;
	u8 adjtime;
	*/
} __packed;

/* T61 reg array */
struct t61_config {
	u8 ctrl;
	u8 cmd;
	u8 mode;
	u16 period;
} __packed;

/* T65 reg array */
#define MXT_T65_CTRL		0
#define MXT_T65_GRADTHR		1
#define MXT_T65_LPFILTER	10

enum t65_status{
	T65_NORMAL,
	T65_ZERO_GRADTHR,
	T65_CONFIG_NUM,
};

struct t65_config {
	u8 ctrl;
	u8 grad_thr;
	u8 rsv[8];
	u8 lpfilter;
} __packed;

enum t65_cfg_set_option{
	MXT_T65_MASK_CTRL = 0,
	MXT_T65_MASK_GRADTHR,
	MXT_T65_MASK_LPFILTER,
};

struct t68_config_head {
	u8 ctrl;
	u8 rsv[2];
	u16 type;
	u8 len;
} __packed;

struct t68_config_tail {
	u8 cmd;
	u8 rsv[2];
} __packed;

/* T71 reg array */
#define MXT_T71_RESET_MAGIC_WORD	0x16
#define MXT_T71_RESET_TAG		0

/* T80 reg array */
#define MXT_T80_CTRL		0
#define MXT_T80_COMP_GAIN	1

enum t80_status{
	T80_NORMAL,
	T80_LOW_GAIN,
	T80_CONFIG_NUM,
};

struct t80_config {
	u8 ctrl;
	u8 comp_gain;
	u8 target_delta;
	u8 compthr;
	u8 atchthr;
	/*
	u8 moistcfg;
	u8 moistdto;*/
} __packed;

enum t80_cfg_set_option{
	MXT_T80_MASK_CTRL = 0,
	MXT_T80_MASK_COMP_GAIN,
};

struct t92_config {
	u8 rsv[12];
	u8 rptcode;
} __packed;

#define PTC_KEY_GROUPS 4
struct t96_config {
	s16 params[PTC_KEY_GROUPS];
} __packed;

#define MXT_T61_CTRL	0
#define MXT_T61_CMD	1
#define MXT_T61_MODE	2
#define MXT_T61_PERIOD	3

#define MXT_T61_CTRL_EN		(1<<0)
#define MXT_T61_CTRL_RPTEN	(1<<1)

#define MXT_T61_RUNNING		(1<<0)
#define MXT_T61_FORCERPT	(1<<4)
#define MXT_T61_STOP		(1<<5)
#define MXT_T61_START		(1<<6)
#define MXT_T61_ELAPSED		(1<<7)

#define MXT_T100_CTRL	0
#define MXT_T100_NUMTOUCH	6
#define MXT_T100_XORIGN		8
#define MXT_T100_XSIZE		9
#define MXT_T100_YORIGN		19
#define MXT_T100_YSIZE		20
#define MXT_T100_TCHHR		30
#define MXT_T100_TCHHYST	31
#define MXT_T100_INTTHR		32
#define MXT_T100_MRGTHR		35
#define MXT_T100_DXTHRSF	38
#define MXT_T100_MRGHYST	37
#define MXT_T100_INTTHRHYST	53

#define MXT_T9_T100_DETECT		(1 << 7)

#define MXT_SCRAUX_STS_DETECT	(1 << 7)
#define MXT_SCRAUX_STS_SUP		(1 << 6)

struct scr_info {
	u8 status;
	u8 num_tch;
	u16 area_tch;
	u16 area_atch;
	u16 area_inttch;
};

enum{
	MSG_SCRAUX_STATUS = 0,
	MSG_SCRAUX_NUMRPTTCH,
	MSG_SCRAUX_TCHAREA0,
	MSG_SCRAUX_TCHAREA1,
	MSG_SCRAUX_ATCHAREA0,
	MSG_SCRAUX_ATCHAREA1,
	MSG_SCRAUX_INTTHRAREA0,
	MSG_SCRAUX_INTTHRAREA1,
	NUM_MSG_SCRAUX
};

struct ext_info {
	u8 status;
	u8 amp;
	u8 vec;
	u8 area;
	u8 peak;
	u8 width;
	u8 height;
};

enum{
	MSG_T9_T100_STATUS = 0,
	MSG_T9_T100_AREA,
	MSG_T9_T100_AMP,
	MSG_T9_T100_VEC,
	MSG_T9_T100_HW,
	MSG_T9_T100_PEAK,
	MSG_T9_T100_AREAHW0,
	MSG_T9_T100_AREAHW1,
	NUM_MSG_T9_T100,
};

#define MAX_TRACE_POINTS 10
#define MAX_AMPLITUDE_VALUE 255

struct sc_config{
	u8 gain;
	u8 chthr;
	u8 chhyst;
	u8 intthr;
	u8 inthyst;
} __packed;

struct t104_config {
	struct sc_config x;
	struct sc_config y;
} __packed;

struct reg_config {
	u16 reg;
	u16 instance;
	u16 offset;
	u16 reg_len;
#define MAX_REG_DATA_LEN 16
	u8 buf[MAX_REG_DATA_LEN];
	u16 len;
	unsigned long mask;
	unsigned long flag;
	unsigned long tag;
	u8 sleep;

#define FLAG_REG_DATA_IN_EXT_BUF (1<<30)
	u8 *ext_buf;

};

enum{
	MX = 0,
	MX_POS,
	MX_AA,
	MX_T,
	MX_K,
	MX_SUM,
};

struct point{
	int x;
	int y;
};

struct rect{
	int x0;
	int y0;
	int x1;
	int y1;
};

enum{
	SC_X = 0,
	SC_Y,
	SC_NUM,
};

struct range_t {
	int   start;
	int   end;
};

struct mxt_config {
	struct device *dev;
	unsigned int max_x;
	unsigned int max_y;

	struct rect m[MX_SUM];
	struct range_t r[SC_NUM];
	struct t8_config t8;
	struct t9_t100_config t9_t100;
	struct t15_config t15;
	struct t38_config t38;
	struct t40_config t40;
	struct t42_config t42;
	struct t55_config t55;
#define T61_MAX_INSTANCE_NUM 2
	struct t61_config t61[T61_MAX_INSTANCE_NUM];
	struct t65_config t65;
	struct t80_config t80;
	struct t92_config t92;
	struct t96_config t96;
	struct t104_config t104;
} __packed;

#define PL_STATUS_FLAG_NOISE						(1<<0)
#define PL_STATUS_FLAG_VERY_NOISE				(1<<1)
#define PL_STATUS_FLAG_NOISE_CHANGE				(1<<2)
#define PL_STATUS_FLAG_DUALX						(1<<3)
#define PL_STATUS_FLAG_PROXIMITY_REMOVED		(1<<4)

#define PL_STATUS_FLAG_SUSPEND					(1<<5)
#define PL_STATUS_FLAG_RESUME					(1<<6)
#define PL_STATUS_FLAG_POWERUP					(1<<7)

#define PL_STATUS_FLAG_RESETING					(1<<8)
#define PL_STATUS_FLAG_RESET_END					(1<<9)
#define PL_STATUS_FLAG_CAL_END					(1<<10)
#define PL_STATUS_FLAG_PHONE_ON					(1<<11)

#define PL_FUNCTION_FLAG_BINDING				(1<<12)
#define PL_FUNCTION_FLAG_OPTION					(1<<13)
#define PL_FUNCTION_FLAG_GLOVE					(1<<14)
#define PL_FUNCTION_FLAG_STYLUS					(1<<15)
#define PL_FUNCTION_FLAG_WAKEUP_GESTURE		(1<<16)
#define PL_FUNCTION_FLAG_WAKEUP					(PL_FUNCTION_FLAG_WAKEUP_GESTURE)

#define PL_STATUS_FLAG_HIGH_MASK_SHIFT		20

#define PL_STATUS_FLAG_PLUG_CAL					(1<<20)
#define PL_STATUS_FLAG_PLUG_MSC					(1<<21)
#define PL_STATUS_FLAG_PLUG_PI					(1<<22)
#define PL_STATUS_FLAG_PLUG_CLIP					(1<<23)
#define PL_STATUS_FLAG_PLUG_WDG					(1<<24)
#define PL_STATUS_FLAG_PAUSE						(1<<27)

#define PL_STATUS_FLAG_NEED_RESET				(1<<28)
#define PL_STATUS_FLAG_NOSUSPEND				(1<<29)
#define PL_STATUS_FLAG_STOP						(1<<30)
#define PL_STATUS_FLAG_FORCE_STOP				(1<<31)

#define PL_STATUS_FLAG_NOISE_MASK			(PL_STATUS_FLAG_NOISE|PL_STATUS_FLAG_VERY_NOISE)
#define PL_STATUS_FLAG_PAUSE_MASK			(0x0ff00000)
#define PL_FUNCTION_FLAG_MASK				(0x000ff000)
#define PL_STATUS_FLAG_LOW_MASK				(0)
#define PL_STATUS_FLAG_MASK				(-1)

#define MAKEWORD(a, b)  ((unsigned short)(((unsigned char)(a)) \
	| ((unsigned short)((unsigned char)(b))) << 8))

typedef int (*ges_hook_callback)(/*struct plugin_proci*/void *pi_id, u8 *msg, unsigned long pl_flag);

struct plugin_cal{
	void *obs;
	void *cfg;

	int (*init)(struct plugin_cal *p);
	void (*deinit)(struct plugin_cal *p);
	void (*start)(struct plugin_cal *p, bool resume);
	void (*stop)(struct plugin_cal *p);
	void (*hook_t6)(struct plugin_cal *p, u8 status);
	void (*hook_t9)(struct plugin_cal *p, int id, int x, int y, u8 status);
	void (*hook_t100)(struct plugin_cal *p, int id, int x, int y, u8 status);
	void (*hook_t42)(struct plugin_cal *p, u8 status);
	void (*hook_t61)(struct plugin_cal *p, int id, u8 status);
	void (*hook_t72)(struct plugin_cal *p, u8 *msg);
	void (*pre_process)(struct plugin_cal *p, unsigned long pl_flag);
	long (*post_process)(struct plugin_cal *p, unsigned long pl_flag);
	void (*hook_reset_slots)(struct plugin_cal *p);
	int  (*check_and_calibrate)(struct plugin_cal *p, bool check_sf, bool resume, unsigned long pl_flag);
	int (*show)(struct plugin_cal *p);
	int (*store)(struct plugin_cal *p, const char *buf, size_t count);

	void *dev;
	const struct mxt_config *dcfg;
	void (*set_and_clr_flag)(void *pl_dev, int mask_s, int mask_c);
	int (*set_t6)(void *pl_dev, u16 cmd_offset, u8 value, bool wait);
	int (*set_t6_cal)(void *pl_dev);
	int (*set_t8_cfg)(void *pl_dev, int state, unsigned long unset);
	int (*set_t9_t100_cfg)(void *pl_dev, int state, unsigned long unset);
	int (*set_t42_cfg)(void *pl_dev, int state, unsigned long unset);
	int (*set_t55_adp_thld)(void *pl_dev, int state);
	int (*set_t61_timer)(void *pl_dev, bool enable, int id);
	int (*set_t65_cfg)(void *pl_dev, int state, unsigned long unset);
	int (*set_t80_cfg)(void *pl_dev, int state, unsigned long unset);
	void (*emu_plugin_hook_t6)(void *pl_dev, u8 status);
};

struct plugin_ac{
	void *obs;
	void *cfg;

	int (*init)(struct plugin_ac *p);
	void (*deinit)(struct plugin_ac *p);
	void (*start)(struct plugin_ac *p, bool resume);
	void (*stop)(struct plugin_ac *p);
	void (*hook_t6)(struct plugin_ac *p, u8 status);
	void (*hook_t9)(struct plugin_ac *p, int id, int x, int y, u8 status);
	void (*hook_t100)(struct plugin_ac *p, int id, int x, int y, u8 status);
	void (*hook_t42)(struct plugin_ac *p, u8 status);
	void (*hook_t61)(struct plugin_ac *p, int id, u8 status);
	void (*hook_t72)(struct plugin_ac *p, u8 *msg);
	void (*pre_process)(struct plugin_ac *p, int pl_flag);
	long (*post_process)(struct plugin_ac *p, unsigned long pl_flag);
	void (*hook_reset_slots)(struct plugin_ac *p);
	int (*show)(struct plugin_ac *p);
	int (*store)(struct plugin_ac *p, const char *buf, size_t count);

	void *dev;
	const struct mxt_config *dcfg;
	void (*set_and_clr_flag)(void *pl_dev, int mask_s, int mask_c);
	int (*set_t6_cal)(void *pl_dev);
	int (*set_t8_cfg)(void *pl_dev, int state, unsigned long mask);
	int (*set_t9_t100_cfg)(void *pl_dev, int state, unsigned long unset);
	int (*set_t55_adp_thld)(void *pl_dev, int state);
	int (*set_t61_timer)(void *pl_dev, bool enable, int id);
	int (*set_t65_cfg)(void *pl_dev, int state, unsigned long unset);
	int (*set_t80_cfg)(void *pl_dev, int state, unsigned long unset);
};

struct plugin_proci{
	void *obs;
	void *cfg;

	int (*init)(struct plugin_proci *p);
	void (*deinit)(struct plugin_proci *p);
	void (*start)(struct plugin_proci *p, bool resume);
	void (*stop)(struct plugin_proci *p);
	void (*hook_t6)(struct plugin_proci *p, u8 status);
	ges_hook_callback hook_t24;
	ges_hook_callback hook_t61;
	ges_hook_callback hook_t81;
	ges_hook_callback hook_t92;
	ges_hook_callback hook_t93;
	ges_hook_callback hook_t99;
	ges_hook_callback hook_t115;
	ges_hook_callback hook_t116;

	int (*wake_enable)(struct plugin_proci *p, unsigned long pl_flag);
	int (*wake_disable)(struct plugin_proci *p, unsigned long pl_flag);
	void (*pre_process)(struct plugin_proci *p, unsigned long pl_flag);
	long (*post_process)(struct plugin_proci *p, unsigned long pl_flag);
	int (*show)(struct plugin_proci *p);
	int (*store)(struct plugin_proci *p, const char *buf, size_t count);

	void *dev;
	const struct mxt_config *dcfg;
	void (*set_and_clr_flag)(void *pl_dev, int mask_s, int mask_c);
	int (*set_obj_cfg)(void *pl_dev, struct reg_config *config, u8 *stack_buf, unsigned long flag);
	int (*get_obj_cfg)(void *pl_dev, struct reg_config *config, unsigned long flag);
	int (*get_diagnostic_data)(void *pl_dev, u8 cmd, u8 page, u8 index, u8 num, char *buf, int interval, int interval_c);
	int (*reset)(void *pl_dev);
	void (*emu_plugin_hook_t6)(void *pl_dev, u8 status);
	void (*active_thread)(void *pl_dev);
};

struct plugin_misc{
	void *obs;
	void *cfg;

	int (*init)(struct plugin_misc *p);
	void (*deinit)(struct plugin_misc *p);
	void (*start)(struct plugin_misc *p, bool resume);
	void (*stop)(struct plugin_misc *p);
	void (*hook_t6)(struct plugin_misc *p, u8 status);
	void (*hook_t68)(struct plugin_misc *p, u8 *msg);
	int (*get_pid_name)(struct plugin_misc *p, char *name, int len);

	void (*pre_process)(struct plugin_misc *p, unsigned long pl_flag);
	long (*post_process)(struct plugin_misc *p, unsigned long pl_flag);
	int (*show)(struct plugin_misc *p);
	int (*store)(struct plugin_misc *p, const char *buf, size_t count);
	int (*check_tune_status)(struct plugin_misc *p);

	void *dev;
	const struct mxt_config *dcfg;
	void (*set_and_clr_flag)(void *pl_dev, int mask_s, int mask_c);
	int (*set_t6_cal)(void *pl_dev);
	int (*set_obj_cfg)(void *pl_dev, struct reg_config *config, u8 *stack_buf, unsigned long flag);
	int (*get_obj_cfg)(void *pl_dev, struct reg_config *config, unsigned long flag);
	int (*get_diagnostic_data)(void *pl_dev, u8 cmd, u8 page, u8 index, u8 num, char *buf, int interval, int interval_c);
	void (*active_thread)(void *pl_dev);
};

struct plugin_clip{
	void *obs;
	void *cfg;

	int (*init)(struct plugin_clip *p);
	void (*deinit)(struct plugin_clip *p);
	void (*start)(struct plugin_clip *p, bool resume);
	void (*stop)(struct plugin_clip *p);
	void (*hook_t6)(struct plugin_clip *p, u8 status);
	int (*hook_t9)(struct plugin_clip *p, int id, int x, int y, u8 *sts);
	int (*hook_t100)(struct plugin_clip *p, int id, int x, int y, u8 *sts);
	int (*hook_t100_scraux)(struct plugin_clip *p, struct scr_info *in);
	void (*hook_reset_slots)(struct plugin_clip *p);

	void (*pre_process)(struct plugin_clip *p, unsigned long pl_flag);
	long (*post_process)(struct plugin_clip *p, unsigned long pl_flag);
	int (*show)(struct plugin_clip *p);
	int (*store)(struct plugin_clip *p, const char *buf, size_t count);

	void *dev;
	const struct mxt_config *dcfg;
	void (*set_and_clr_flag)(void *pl_dev, int mask_s, int mask_c);
	int (*set_t6_cal)(void *pl_dev);
	int (*set_obj_cfg)(void *pl_dev, struct reg_config *config, u8 *stack_buf, unsigned long flag);
	int (*get_obj_cfg)(void *pl_dev, struct reg_config *config, unsigned long flag);
	int (*get_diagnostic_data)(void *pl_dev, u8 cmd, u8 page, u8 index, u8 num, char *buf, int interval, int interval_c);
	void (*active_thread)(void *pl_dev);
};

struct plugin_wdg{
	void *obs;
	void *cfg;

	int (*init)(struct plugin_wdg *p);
	void (*deinit)(struct plugin_wdg *p);
	void (*start)(struct plugin_wdg *p, bool resume);
	void (*stop)(struct plugin_wdg *p);
	void (*hook_t6)(struct plugin_wdg *p, u8 state);
	int (*hook_reg_access)(struct plugin_wdg *p, u16 addr, u16 reg, u16 len, const void *val, unsigned long flag, int result, bool is_w);
	void (*hook_t61)(struct plugin_wdg *p, int id, u8 state);

	void (*pre_process)(struct plugin_wdg *p, unsigned long pl_flag);
	long (*post_process)(struct plugin_wdg *p, unsigned long pl_flag);
	int (*show)(struct plugin_wdg *p);
	int (*store)(struct plugin_wdg *p, const char *buf, size_t count);

	void *dev;
	const struct mxt_config *dcfg;
	void (*set_and_clr_flag)(void *pl_dev, int mask_s, int mask_c);
	int (*reset)(void *pl_dev, int por);
	int (*set_obj_cfg)(void *pl_dev, struct reg_config *config, u8 *stack_buf, unsigned long flag);
	int (*get_obj_cfg)(void *pl_dev, struct reg_config *config, unsigned long flag);
	void (*active_thread)(void *pl_dev);
};


struct plug_observer{
	unsigned long flag;
#if defined(CONFIG_MXT_T71_RESET_WATCHDOG)
	unsigned time_reset_check_st;
#endif
};

struct plug_config{
	struct t8_config *t8_cfg;
	int num_t8_config;

	struct t9_t100_config *t9_t100_cfg;
	int num_t9_t100_config;

	struct t42_config *t42_cfg;
	int num_t42_config;

	struct t55_config *t55_cfg;
	int num_t55_config;

	struct t61_config *t61_cfg;
	int num_t61_config;

	struct t65_config *t65_cfg;
	int num_t65_config;

	struct t80_config *t80_cfg;
	int num_t80_config;

#if defined(CONFIG_MXT_T71_RESET_WATCHDOG)
	unsigned long interval_reset_check_wait;
#endif
};

struct plug_interface{
	bool inited;
	void *dev;
	void (*active_thread)(void *dev_id, unsigned int event);
	struct mxt_config init_cfg;

	struct plug_observer observer;
	struct plug_config config;

	struct plugin_cal *cal;
	struct plugin_ac *ac;
	struct plugin_proci *pi;
	struct plugin_misc *msc;
	struct plugin_clip *clp;
	struct plugin_wdg *wd;

	char suffix_pid_name[16];
};

bool valid_config(void *pcfg, int len, u8 check_val);
int mxt_diagnostic_reset_page(void *dev_id, u8 cmd, int interval, int interval_c);
int mxt_surface_page_aquire_sc(void *dev_id, s16 *buf, u8 cmd, const struct rect *surface, int interval, int interval_c);
int mxt_surface_page_aquire(void *dev_id, s16 *buf, u8 cmd, const struct rect *surface, struct rect *pst, int y_offset, int *out_pos, int interval, int interval_c);
int mxt_get_current_page_info(void *dev_id, void *buf, u16 len);
void print_matrix(const char *prefix, const s16 *buf, int x_size, int y_size);
void print_trunk(const u8 *data, int pos, int offset);
void print_dec16_buf(const char *level, const char *prefix_str, const s16 *buf, int num);
size_t dec_dump_to_buffer(const char *prefix, const void *buf, size_t num, int groupsize,
			char *linebuf, size_t linebuflen);

int plugin_cal_init(struct plugin_cal *p);
int plugin_ac_init(struct plugin_ac *p);
int plugin_proci_init(struct plugin_proci *p);
int plugin_misc_init(struct plugin_misc *p);
int plugin_clip_init(struct plugin_clip *p);

void plugin_interface_wdg_init(struct plugin_wdg *p);

#endif/* __LINUX_ATMEL_MXT_PLUG */
