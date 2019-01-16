/**
* @file    mt_cpufreq.c
* @brief   Driver for CPU DVFS
*
*/

#define __MT_CPUFREQ_C__

/*=============================================================*/
/* Include files                                               */
/*=============================================================*/

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/aee.h>

#include <asm/system.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

/* project includes */
#include "mach/mt_reg_base.h"
#include "mach/mt_typedefs.h"

#include "mach/irqs.h"
#include "mach/mt_irq.h"
#include "mach/mt_thermal.h"
#include "mach/mt_spm_idle.h"
#include "mach/mt_pmic_wrap.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_freqhopping.h"
#include "mach/mt_spm.h"
#include "mach/mt_ptp.h"
#include "mach/mt_ptp2.h"
#include "mach/mt_static_power.h"
#include "mach/upmu_sw.h"
#include "mach/mt_hotplug_strategy.h"
#include "mach/mt_boot.h"

#include "mach/mtk_rtc_hal.h"
#include "mach/mt_rtc_hw.h"

/* local includes */
#include "mach/mt_cpufreq.h"

/* forward references */
extern int is_ext_buck_sw_ready(void); // TODO: ask James to provide the head file
extern int ext_buck_vosel(unsigned long val); // TODO: ask James to provide the head file
extern int da9210_read_byte(kal_uint8 cmd, kal_uint8 *ret_data); // TODO: ask James to provide the head file
/* extern u32 get_devinfo_with_index(u32 index); */ // TODO: ask sbchk_base.c owner to provide head file
extern int mtktscpu_get_Tj_temp(void); // TODO: ask Jerry to provide the head file
extern void (*cpufreq_freq_check)(enum mt_cpu_dvfs_id id); // TODO: ask Marc to provide the head file (pass big or little???)
extern void hp_limited_cpu_num(int num); // TODO: ask Marc to provide the head file (pass big or little???)


/*=============================================================*/
/* Macro definition                                            */
/*=============================================================*/

/*
 * CONFIG
 */
#define FIXME 0

#define CONFIG_CPU_DVFS_SHOWLOG 1

// #define CONFIG_CPU_DVFS_BRINGUP 1               /* for bring up */
// #define CONFIG_CPU_DVFS_RANDOM_TEST 1           /* random test for UT/IT */
// #define CONFIG_CPU_DVFS_FFTT_TEST 1             /* FF TT SS volt test */
// #define CONFIG_CPU_DVFS_TURBO 1                 /* turbo mode */
#define CONFIG_CPU_DVFS_DOWNGRADE_FREQ 1        /* downgrade freq */

#ifdef __KERNEL__
//#define CONFIG_CPU_DVFS_AEE_RR_REC 1
#endif

/* used @ set_cur_volt_big() */
#define MIN_DIFF_VSRAM_PROC     10
#define NORMAL_DIFF_VRSAM_VPROC 50 // 20
#define MAX_DIFF_VSRAM_VPROC    200

#define PMIC_SETTLE_TIME(old_mv, new_mv) ((((old_mv) > (new_mv)) ? ((old_mv) - (new_mv)) : ((new_mv) - (old_mv))) * 2 / 25 + 25 + 1) /* us, PMIC settle time, should not be changed */ // (((((old_mv) > (new_mv)) ? ((old_mv) - (new_mv)) : ((new_mv) - (old_mv))) + 9 ) / 10)
#define PLL_SETTLE_TIME         (30)            /* us, PLL settle time, should not be changed */ // TODO: sync with DE, 20us or 30us???
#define RAMP_DOWN_TIMES         (2)             /* RAMP DOWN TIMES to postpone frequency degrade */
#define FHCTL_CHANGE_FREQ       (1000000)       /* if cross 1GHz when DFS, don't used FHCTL */ // TODO: rename CPUFREQ_BOUNDARY_FOR_FHCTL

#define DEFAULT_VOLT_VGPU       (1125)
#define DEFAULT_VOLT_VCORE_AO   (1125)
#define DEFAULT_VOLT_VCORE_PDN  (1125)

/* for DVFS OPP table */ // TODO: necessary or just specify in opp table directly???

#define DVFS_BIG_F0 (1898000) /* KHz */
#define DVFS_BIG_F1 (1495000) /* KHz */
#define DVFS_BIG_F2 (1365000) /* KHz */
#define DVFS_BIG_F3 (1248000) /* KHz */
#define DVFS_BIG_F4 (1144000) /* KHz */
#define DVFS_BIG_F5 (1001000) /* KHz */
#define DVFS_BIG_F6 (806000)  /* KHz */
#define DVFS_BIG_F7 (403000)  /* KHz */

#if defined(SLT_VMAX)
#define DVFS_BIG_V0 (1150)    /* mV */
#else
#define DVFS_BIG_V0 (1100)    /* mV */
#endif
#define DVFS_BIG_V1 (1079)    /* mV */
#define DVFS_BIG_V2 (1050)    /* mV */
#define DVFS_BIG_V3 (1032)    /* mV */
#define DVFS_BIG_V4 (1000)    /* mV */
#define DVFS_BIG_V5 (963)     /* mV */
#define DVFS_BIG_V6 (914)     /* mV */
#define DVFS_BIG_V7 (814)     /* mV */

#define DVFS_LITTLE_F0 (1690000) /* KHz */
#define DVFS_LITTLE_F1 (1495000) /* KHz */
#define DVFS_LITTLE_F2 (1365000) /* KHz */
#define DVFS_LITTLE_F3 (1248000) /* KHz */
#define DVFS_LITTLE_F4 (1144000) /* KHz */
#define DVFS_LITTLE_F5 (1001000) /* KHz */
#define DVFS_LITTLE_F6 (806000)  /* KHz */
#define DVFS_LITTLE_F7 (403000)  /* KHz */

#if defined(SLT_VMAX)
#define DVFS_LITTLE_V0 (1150)    /* mV */
#else
#define DVFS_LITTLE_V0 (1125)    /* mV */
#endif
#define DVFS_LITTLE_V1 (1079)    /* mV */
#define DVFS_LITTLE_V2 (1050)    /* mV */
#define DVFS_LITTLE_V3 (1023)    /* mV */
#define DVFS_LITTLE_V4 (1000)    /* mV */
#define DVFS_LITTLE_V5 (963)     /* mV */
#define DVFS_LITTLE_V6 (914)     /* mV */
#define DVFS_LITTLE_V7 (814)     /* mV */

#define PWR_THRO_MODE_LBAT_1365MHZ	BIT(0)
#define PWR_THRO_MODE_BAT_OC_806MHZ	BIT(1)
#define PWR_THRO_MODE_BAT_PER_1365MHZ	BIT(2)

#define CPU_DVFS_OPPIDX_1365MHZ		2
#define CPU_DVFS_OPPIDX_806MHZ		6

/*
 * LOG
 */
// #define USING_XLOG

#define HEX_FMT "0x%08x"
#undef TAG

#ifdef USING_XLOG
#include <linux/xlog.h>

#define TAG     "Power/cpufreq"

#define cpufreq_err(fmt, args...)       \
	xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define cpufreq_warn(fmt, args...)      \
	xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define cpufreq_info(fmt, args...)      \
	xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define cpufreq_dbg(fmt, args...)       \
	xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define cpufreq_ver(fmt, args...)       \
	xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else   /* USING_XLOG */

#define TAG     "[Power/cpufreq] "

#define cpufreq_err(fmt, args...)       \
	printk(KERN_ERR TAG KERN_CONT fmt, ##args)
#define cpufreq_warn(fmt, args...)      \
	printk(KERN_WARNING TAG KERN_CONT fmt, ##args)
#define cpufreq_info(fmt, args...)      \
	printk(KERN_NOTICE TAG KERN_CONT fmt, ##args)
#define cpufreq_dbg(fmt, args...)       \
	printk(KERN_INFO TAG KERN_CONT fmt, ##args)
#define cpufreq_ver(fmt, args...)       \
	printk(KERN_DEBUG TAG KERN_CONT fmt, ##args)

#endif  /* USING_XLOG */

#define FUNC_LV_MODULE          BIT(0)  /* module, platform driver interface */
#define FUNC_LV_CPUFREQ         BIT(1)  /* cpufreq driver interface          */
#define FUNC_LV_API             BIT(2)  /* mt_cpufreq driver global function */
#define FUNC_LV_LOCAL           BIT(3)  /* mt_cpufreq driver lcaol function  */
#define FUNC_LV_HELP            BIT(4)  /* mt_cpufreq driver help function   */

static unsigned int func_lv_mask = 0; // (FUNC_LV_MODULE | FUNC_LV_CPUFREQ | FUNC_LV_API | FUNC_LV_LOCAL | FUNC_LV_HELP);

#if defined(CONFIG_CPU_DVFS_SHOWLOG)
#define FUNC_ENTER(lv)          do { if ((lv) & func_lv_mask) cpufreq_dbg(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv)           do { if ((lv) & func_lv_mask) cpufreq_dbg("<< %s():%d\n", __func__, __LINE__); } while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif /* CONFIG_CPU_DVFS_SHOWLOG */

/*
 * BIT Operation
 */
#define _BIT_(_bit_)                    (unsigned)(1 << (_bit_))
#define _BITS_(_bits_, _val_)           ((((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_)               (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * REG ACCESS
 */
#define cpufreq_read(addr)                  DRV_Reg32(addr)
#define cpufreq_write(addr, val)            mt_reg_sync_writel(val, addr)
#define cpufreq_write_mask(addr, mask, val) cpufreq_write(addr, (cpufreq_read(addr) & ~(_BITMASK_(mask))) | _BITS_(mask, val))


/*=============================================================*/
/* Local type definition                                       */
/*=============================================================*/


/*=============================================================*/
/* Local variable definition                                   */
/*=============================================================*/
static CHIP_SW_VER chip_ver = CHIP_SW_VER_02;
static struct mt_cpu_tlp_power_info *cpu_tlp_power_tbl = NULL;

/*=============================================================*/
/* Local function definition                                   */
/*=============================================================*/


/*=============================================================*/
/* Gobal function definition                                   */
/*=============================================================*/

/*
 * LOCK
 */
#if 0   /* spinlock */ // TODO: FIXME, it would cause warning @ big because of i2c access with atomic operation
static DEFINE_SPINLOCK(cpufreq_lock);
#define cpufreq_lock(flags) spin_lock_irqsave(&cpufreq_lock, flags)
#define cpufreq_unlock(flags) spin_unlock_irqrestore(&cpufreq_lock, flags)
#else   /* mutex */
DEFINE_MUTEX(cpufreq_mutex);
bool is_in_cpufreq = 0;
#define cpufreq_lock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_lock(&cpufreq_mutex); \
		is_in_cpufreq = 1;\
		spm_mcdi_wakeup_all_cores();\
	} while (0)

#define cpufreq_unlock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		is_in_cpufreq = 0;\
		mutex_unlock(&cpufreq_mutex); \
	} while (0)
#endif

/*
 * EFUSE
 */
#define CPUFREQ_EFUSE_INDEX     (24)

#define CPU_LEVEL_0             (0x3)
#define CPU_LEVEL_1             (0x2)
#define CPU_LEVEL_2             (0x1)

#define CPU_LV_TO_OPP_IDX(lv)   ((lv)-1) /* cpu_level to opp_idx */

extern u32 get_devinfo_with_index(u32 index); // TODO: FIXME #include "devinfo.h"

static unsigned int read_efuse_speed(void) // TODO: remove it latter
{
	unsigned int lv = 0;
	unsigned int efuse = _GET_BITS_VAL_(27 : 24, get_devinfo_with_index(CPUFREQ_EFUSE_INDEX));

#ifdef CONFIG_OF
{
	struct device_node *node = NULL;
	unsigned int cpu_speed = 0;

	while ((node = of_find_node_by_type(node, "cpu"))) {
		if (!of_property_read_u32(node, "clock-frequency", &cpu_speed) && (cpu_speed/1000/1000) != 1700)
		{
			cpu_speed = cpu_speed / 1000 / 1000;    // MHz
			break;
		}
	}
		
	switch (cpu_speed) {
		case 2000: //or 2002
			lv = CPU_LEVEL_1;   // 95M
			break;
		case 2200: //or 2201
			lv = CPU_LEVEL_2;   // 95
			break;
		default:
			lv = CPU_LEVEL_1;   // 95M
			break;
	}
}
#endif
	return lv;
}

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
enum dvfs_state {
	CPU_DVFS_LITTLE_IS_DOING_DVFS = 0,
	CPU_DVFS_LITTLE_IS_TURBO,
	CPU_DVFS_BIG_IS_DOING_DVFS = 0,
	CPU_DVFS_BIG_IS_TURBO,
};

extern unsigned char *aee_rr_rec_big_volt(void);
extern unsigned char *aee_rr_rec_little_volt(void);
extern unsigned char *aee_rr_rec_cpu_oppidx(void);
extern unsigned char *aee_rr_rec_dvfs_state(void);

static unsigned char *p_big_volt;
static unsigned char *p_little_volt;
static unsigned char *p_cpu_oppidx;
static unsigned char *p_dvfs_state;

static void _mt_cpufreq_aee_init(void)
{
	p_big_volt = aee_rr_rec_big_volt();
	p_little_volt = aee_rr_rec_little_volt();
	p_cpu_oppidx = aee_rr_rec_cpu_oppidx();
	p_dvfs_state = aee_rr_rec_dvfs_state();

	*p_big_volt = 0;
	*p_little_volt = 0;
	*p_cpu_oppidx = 0;
	*p_dvfs_state = 0;
}
#endif
/*
 * PMIC_WRAP
 */
// TODO: defined @ pmic head file???
#define VOLT_TO_PMIC_VAL(volt)  ((((volt) - 700) * 100 + 625 - 1) / 625)
#define PMIC_VAL_TO_VOLT(pmic)  (((pmic) * 625) / 100 + 700) // (((pmic) * 625 + 100 - 1) / 100 + 700)

#define VOLT_TO_EXTBUCK_VAL(volt) (((((volt) - 300) + 9) / 10) & 0x7F)
#define EXTBUCK_VAL_TO_VOLT(val)  (300 + ((val) & 0x7F) * 10)

/* PMIC WRAP ADDR */ // TODO: include other head file
#define PMIC_WRAP_DVFS_ADR0     (PWRAP_BASE + 0x0E8)
#define PMIC_WRAP_DVFS_WDATA0   (PWRAP_BASE + 0x0EC)
#define PMIC_WRAP_DVFS_ADR1     (PWRAP_BASE + 0x0F0)
#define PMIC_WRAP_DVFS_WDATA1   (PWRAP_BASE + 0x0F4)
#define PMIC_WRAP_DVFS_ADR2     (PWRAP_BASE + 0x0F8)
#define PMIC_WRAP_DVFS_WDATA2   (PWRAP_BASE + 0x0FC)
#define PMIC_WRAP_DVFS_ADR3     (PWRAP_BASE + 0x100)
#define PMIC_WRAP_DVFS_WDATA3   (PWRAP_BASE + 0x104)
#define PMIC_WRAP_DVFS_ADR4     (PWRAP_BASE + 0x108)
#define PMIC_WRAP_DVFS_WDATA4   (PWRAP_BASE + 0x10C)
#define PMIC_WRAP_DVFS_ADR5     (PWRAP_BASE + 0x110)
#define PMIC_WRAP_DVFS_WDATA5   (PWRAP_BASE + 0x114)
#define PMIC_WRAP_DVFS_ADR6     (PWRAP_BASE + 0x118)
#define PMIC_WRAP_DVFS_WDATA6   (PWRAP_BASE + 0x11C)
#define PMIC_WRAP_DVFS_ADR7     (PWRAP_BASE + 0x120)
#define PMIC_WRAP_DVFS_WDATA7   (PWRAP_BASE + 0x124)

/* PMIC ADDR */ // TODO: include other head file
#define PMIC_ADDR_VPROC_CA7_VOSEL_ON      0x847C  /* [6:0]                     */
#define PMIC_ADDR_VPROC_CA7_VOSEL_SLEEP   0x847E  /* [6:0]                     */
#define PMIC_ADDR_VPROC_CA7_EN            0x8476  /* [0] (shared with others)  */
#define PMIC_ADDR_VSRAM_CA7_FAST_TRSN_EN	0x8CB4	/* [3]                       */
#define PMIC_ADDR_VSRAM_CA7_EN            0x8CBC  /* [10] (shared with others) */

#define PMIC_ADDR_VSRAM_CA15L_VOSEL_ON    0x0264  /* [6:0]                     */
#define PMIC_ADDR_VSRAM_CA15L_VOSEL_SLEEP 0x0266  /* [6:0]                     */
#define PMIC_ADDR_VSRAM_CA15L_FAST_TRSN_EN	0x051C	/* [6]                       */
#define PMIC_ADDR_VSRAM_CA15L_EN          0x0524  /* [10] (shared with others) */

#define PMIC_ADDR_VGPU_VOSEL_ON           0x02B0  /* [6:0]                     */
#define PMIC_ADDR_VGPU_VOSEL_SLEEP        0x02B2  /* [6:0]                     */
#define PMIC_ADDR_VCORE_AO_VOSEL_ON       0x036C  /* [6:0]                     */
#define PMIC_ADDR_VCORE_AO_VOSEL_SLEEP    0x036E  /* [6:0]                     */
#define PMIC_ADDR_VCORE_PDN_VOSEL_ON      0x024E  /* [6:0]                     */
#define PMIC_ADDR_VCORE_PDN_VOSEL_SLEEP   0x0250  /* [6:0]                     */
#define PMIC_ADDR_VCORE_PDN_EN_CTRL       0x0244  /* [0] (shared with others)  */

#define NR_PMIC_WRAP_CMD 8 /* num of pmic wrap cmd (fixed value) */

struct pmic_wrap_setting {
	enum pmic_wrap_phase_id phase;

	struct {
		const unsigned int cmd_addr;
		const unsigned int cmd_wdata;
	} addr[NR_PMIC_WRAP_CMD];

	struct {
		struct {
			unsigned int cmd_addr;
			unsigned int cmd_wdata;
		} _[NR_PMIC_WRAP_CMD];
		const int nr_idx;
	} set[NR_PMIC_WRAP_PHASE];
};

static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE, /* invalid setting for init */
	.addr = {
		{ PMIC_WRAP_DVFS_ADR0, PMIC_WRAP_DVFS_WDATA0, },
		{ PMIC_WRAP_DVFS_ADR1, PMIC_WRAP_DVFS_WDATA1, },
		{ PMIC_WRAP_DVFS_ADR2, PMIC_WRAP_DVFS_WDATA2, },
		{ PMIC_WRAP_DVFS_ADR3, PMIC_WRAP_DVFS_WDATA3, },
		{ PMIC_WRAP_DVFS_ADR4, PMIC_WRAP_DVFS_WDATA4, },
		{ PMIC_WRAP_DVFS_ADR5, PMIC_WRAP_DVFS_WDATA5, },
		{ PMIC_WRAP_DVFS_ADR6, PMIC_WRAP_DVFS_WDATA6, },
		{ PMIC_WRAP_DVFS_ADR7, PMIC_WRAP_DVFS_WDATA7, },
	},

	.set[PMIC_WRAP_PHASE_NORMAL] = {
		._[IDX_NM_VSRAM_CA15L]          = { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,     VOLT_TO_PMIC_VAL(DVFS_BIG_V0),            },
		._[IDX_NM_VPROC_CA7]            = { PMIC_ADDR_VPROC_CA7_VOSEL_ON,       VOLT_TO_PMIC_VAL(DVFS_LITTLE_V0),         },
		._[IDX_NM_VGPU]                 = { PMIC_ADDR_VGPU_VOSEL_ON ,           VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VGPU),      },
		._[IDX_NM_VCORE_AO]             = { PMIC_ADDR_VCORE_AO_VOSEL_ON ,       VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VCORE_AO),  },
		._[IDX_NM_VCORE_PDN]            = { PMIC_ADDR_VCORE_PDN_VOSEL_ON ,      VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VCORE_PDN), },
		.nr_idx = NR_IDX_NM,
	},

	.set[PMIC_WRAP_PHASE_SUSPEND] = {
		._[IDX_SP_VSRAM_CA15L_PWR_ON]   = { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,     VOLT_TO_PMIC_VAL(1000), },
		._[IDX_SP_VSRAM_CA15L_SHUTDOWN] = { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,     VOLT_TO_PMIC_VAL(700),         },
		._[IDX_SP_VPROC_CA7_PWR_ON]     = { PMIC_ADDR_VPROC_CA7_EN,             _BITS_(0  : 0,  1),            }, /* VDVFS2_EN = 1'b1                                                                     */
		._[IDX_SP_VPROC_CA7_SHUTDOWN]   = { PMIC_ADDR_VPROC_CA7_EN,             _BITS_(0  : 0,  0),            }, /* VDVFS2_EN = 1'b0                                                                     */
		._[IDX_SP_VSRAM_CA7_PWR_ON]     = { PMIC_ADDR_VSRAM_CA7_EN,             _BITS_(11 : 10, 1) | _BIT_(8), }, /* RG_VSRAM_DVFS2_ON_CTRL = 1'b0, RG_VSRAM_DVFS2_EN = 1'b1, RG_VSRAM_DVFS2_STBTD = 1'b1 */
		._[IDX_SP_VSRAM_CA7_SHUTDOWN]   = { PMIC_ADDR_VSRAM_CA7_EN,             _BITS_(11 : 10, 0) | _BIT_(8), }, /* RG_VSRAM_DVFS2_ON_CTRL = 1'b0, RG_VSRAM_DVFS2_EN = 1'b0, RG_VSRAM_DVFS2_STBTD = 1'b1 */
		._[IDX_SP_VCORE_PDN_EN_HW_MODE] = { PMIC_ADDR_VCORE_PDN_EN_CTRL,        _BITS_(1  : 0,  3),            }, /* VDVFS11_VOSEL_CTRL = 1'b1, VDVFS11_EN_CTRL = 1'b1                                    */
		._[IDX_SP_VCORE_PDN_EN_SW_MODE] = { PMIC_ADDR_VCORE_PDN_EN_CTRL,        _BITS_(1  : 0,  2),            }, /* VDVFS11_VOSEL_CTRL = 1'b1, VDVFS11_EN_CTRL = 1'b0                                    */
		.nr_idx = NR_IDX_SP,
	},

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
		._[IDX_DI_VPROC_CA7_NORMAL]     = { PMIC_ADDR_VPROC_CA7_VOSEL_ON,       VOLT_TO_PMIC_VAL(1000), },
		._[IDX_DI_VPROC_CA7_SLEEP]      = { PMIC_ADDR_VPROC_CA7_VOSEL_ON,       VOLT_TO_PMIC_VAL(800),  },
		._[IDX_DI_VSRAM_CA7_FAST_TRSN_EN]	= { PMIC_ADDR_VSRAM_CA7_FAST_TRSN_EN,	_BITS_(3 : 3, 0),	},
		._[IDX_DI_VSRAM_CA7_FAST_TRSN_DIS]	= { PMIC_ADDR_VSRAM_CA7_FAST_TRSN_EN,	_BITS_(3 : 3, 1),	},
		._[IDX_DI_VCORE_AO_NORMAL]      = { PMIC_ADDR_VCORE_AO_VOSEL_ON,        VOLT_TO_PMIC_VAL(1125), },
		._[IDX_DI_VCORE_AO_SLEEP]       = { PMIC_ADDR_VCORE_AO_VOSEL_ON,        VOLT_TO_PMIC_VAL(900),  },
		._[IDX_DI_VCORE_PDN_NORMAL]     = { PMIC_ADDR_VCORE_PDN_VOSEL_ON,       VOLT_TO_PMIC_VAL(1125), },
		._[IDX_DI_VCORE_PDN_SLEEP]      = { PMIC_ADDR_VCORE_PDN_VOSEL_ON,       VOLT_TO_PMIC_VAL(900),  },
		.nr_idx = NR_IDX_DI,
	},


	.set[PMIC_WRAP_PHASE_DEEPIDLE_BIG] = {
		._[IDX_DI_VSRAM_CA15L_NORMAL]	= { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,	VOLT_TO_PMIC_VAL(1000), },
		._[IDX_DI_VSRAM_CA15L_SLEEP]	= { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,	VOLT_TO_PMIC_VAL(700),	},
		._[IDX_DI_VSRAM_CA15L_FAST_TRSN_EN]	= { PMIC_ADDR_VSRAM_CA15L_FAST_TRSN_EN, _BITS_(6 : 6, 0),	},
		._[IDX_DI_VSRAM_CA15L_FAST_TRSN_DIS]	= { PMIC_ADDR_VSRAM_CA15L_FAST_TRSN_EN, _BITS_(6 : 6, 1),	},
		._[IDX_DI_VCORE_AO_NORMAL]	= { PMIC_ADDR_VCORE_AO_VOSEL_ON,	VOLT_TO_PMIC_VAL(1125), },
		._[IDX_DI_VCORE_AO_SLEEP]	= { PMIC_ADDR_VCORE_AO_VOSEL_ON,	VOLT_TO_PMIC_VAL(900),	},
		._[IDX_DI_VCORE_PDN_NORMAL]	= { PMIC_ADDR_VCORE_PDN_VOSEL_ON,	VOLT_TO_PMIC_VAL(1125), },
		._[IDX_DI_VCORE_PDN_SLEEP]	= { PMIC_ADDR_VCORE_PDN_VOSEL_ON,	VOLT_TO_PMIC_VAL(900),	},
		.nr_idx = NR_IDX_DI,
	},

	.set[PMIC_WRAP_PHASE_SODI] = {
		._[IDX_SO_VSRAM_CA15L_NORMAL]		= { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,	VOLT_TO_PMIC_VAL(1000),	},
		._[IDX_SO_VSRAM_CA15L_SLEEP]		= { PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,	VOLT_TO_PMIC_VAL(700),	},
		._[IDX_SO_VPROC_CA7_NORMAL]		= { PMIC_ADDR_VPROC_CA7_VOSEL_ON,	VOLT_TO_PMIC_VAL(1000), },
		._[IDX_SO_VPROC_CA7_SLEEP]		= { PMIC_ADDR_VPROC_CA7_VOSEL_ON,	VOLT_TO_PMIC_VAL(800),	},
		._[IDX_SO_VSRAM_CA15L_FAST_TRSN_EN]	= { PMIC_ADDR_VSRAM_CA15L_FAST_TRSN_EN,	_BITS_(6 : 6, 0),	},
		._[IDX_SO_VSRAM_CA15L_FAST_TRSN_DIS]	= { PMIC_ADDR_VSRAM_CA15L_FAST_TRSN_EN,	_BITS_(6 : 6, 1),	},
		._[IDX_SO_VSRAM_CA7_FAST_TRSN_EN]	= { PMIC_ADDR_VSRAM_CA7_FAST_TRSN_EN,	_BITS_(3 : 3, 0),	},
		._[IDX_SO_VSRAM_CA7_FAST_TRSN_DIS]	= { PMIC_ADDR_VSRAM_CA7_FAST_TRSN_EN,	_BITS_(3 : 3, 1),	},
		.nr_idx = NR_IDX_SO,
	},
};

#if 1   /* spinlock */
static DEFINE_SPINLOCK(pmic_wrap_lock);
#define pmic_wrap_lock(flags) spin_lock_irqsave(&pmic_wrap_lock, flags)
#define pmic_wrap_unlock(flags) spin_unlock_irqrestore(&pmic_wrap_lock, flags)
#else   /* mutex */
static DEFINE_MUTEX(pmic_wrap_mutex);

#define pmic_wrap_lock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_lock(&pmic_wrap_mutex); \
	} while (0)

#define pmic_wrap_unlock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_unlock(&pmic_wrap_mutex); \
	} while (0)
#endif

static int _spm_dvfs_ctrl_volt(u32 value)
{
#define MAX_RETRY_COUNT (100)

	u32 ap_dvfs_con;
	int retry = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0)); // TODO: FIXME

	ap_dvfs_con = spm_read(SPM_AP_DVFS_CON_SET);
	spm_write(SPM_AP_DVFS_CON_SET, (ap_dvfs_con & ~(0x7)) | value);
	udelay(5);

	while ((spm_read(SPM_AP_DVFS_CON_SET) & (0x1 << 31)) == 0) {
		if (retry >= MAX_RETRY_COUNT) {
			cpufreq_err("FAIL: no response from PMIC wrapper\n");
			return -1;
		}

		retry++;
		cpufreq_dbg("wait for ACK signal from PMIC wrapper, retry = %d\n", retry);

		udelay(5);
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase)
{
	int i;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);

#if 0   // TODO: FIXME, check IPO-H case

	if (pw.phase == phase)
		return;

#endif

	pmic_wrap_lock(flags);

	pw.phase = phase;

	for (i = 0; i < pw.set[phase].nr_idx; i++) {
		cpufreq_write(pw.addr[i].cmd_addr, pw.set[phase]._[i].cmd_addr);
		cpufreq_write(pw.addr[i].cmd_wdata, pw.set[phase]._[i].cmd_wdata);
	}

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_set_pmic_phase);

void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata) /* just set wdata value */
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);
	BUG_ON(idx >= pw.set[phase].nr_idx);

	pmic_wrap_lock(flags);

	pw.set[phase]._[idx].cmd_wdata = cmd_wdata;

	if (pw.phase == phase)
		cpufreq_write(pw.addr[idx].cmd_wdata, cmd_wdata);

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_set_pmic_cmd);

void mt_cpufreq_apply_pmic_cmd(int idx) /* kick spm */
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(idx >= pw.set[pw.phase].nr_idx);

	pmic_wrap_lock(flags);

	_spm_dvfs_ctrl_volt(idx);

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_apply_pmic_cmd);

/* cpu voltage sampler */
static cpuVoltsampler_func g_pCpuVoltSampler = NULL;

void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB)
{
	g_pCpuVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_setvolt_registerCB);

/* SDIO */
unsigned int mt_get_cur_volt_vcore_ao(void)
{
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VCORE_AO_VOSEL_ON, &rdata);

	rdata = PMIC_VAL_TO_VOLT(rdata);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata; /* mv: vproc */
}

unsigned int mt_get_cur_volt_vcore_pdn(void)
{
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VCORE_PDN_VOSEL_ON, &rdata);

	rdata = PMIC_VAL_TO_VOLT(rdata);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata; /* mv: vproc */
}

void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled)
{
	// empty function
}

void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt) /* unit: mv x 1000 */
{
	unsigned int cur_pmic_val_vcore_ao;
	unsigned int cur_pmic_val_vcore_pdn;
	unsigned int target_pmic_val_vcore_ao;
	int step;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VCORE_AO_VOSEL_ON, &cur_pmic_val_vcore_ao);
	pwrap_read(PMIC_ADDR_VCORE_PDN_VOSEL_ON, &cur_pmic_val_vcore_pdn);
	target_pmic_val_vcore_ao = VOLT_TO_PMIC_VAL(volt / 1000);

	step = (target_pmic_val_vcore_ao > cur_pmic_val_vcore_ao) ? 1 : -1;

	while (target_pmic_val_vcore_ao != cur_pmic_val_vcore_ao) {
		cur_pmic_val_vcore_ao += step;
		cur_pmic_val_vcore_pdn += step;

		mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_AO, cur_pmic_val_vcore_ao);
		mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_PDN, cur_pmic_val_vcore_pdn);

		mt_cpufreq_apply_pmic_cmd(IDX_NM_VCORE_AO);
		mt_cpufreq_apply_pmic_cmd(IDX_NM_VCORE_PDN);

		udelay(4);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

unsigned int mt_vcore_dvfs_volt_get_by_sdio(void) /* unit: mv x 1000 */
{
	return mt_get_cur_volt_vcore_ao() * 1000;
}

/*
 * mt_cpufreq driver
 */
#define MAX_CPU_NUM 4 /* for limited_max_ncpu */

#define OP(khz, volt) {                 \
	.cpufreq_khz = khz,             \
	.cpufreq_volt = volt,           \
	.cpufreq_volt_org = volt,       \
	}

struct mt_cpu_freq_info {
	const unsigned int cpufreq_khz;
	unsigned int cpufreq_volt;
	const unsigned int cpufreq_volt_org;
};

struct mt_cpu_power_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_ncpu;
	unsigned int cpufreq_power;
};

struct mt_cpu_dvfs;

struct mt_cpu_dvfs_ops {
	/* for thermal */
	void (*protect)(struct mt_cpu_dvfs *p, unsigned int limited_power);      /* set power limit by thermal */ // TODO: sync with mt_cpufreq_thermal_protect()
	unsigned int (*get_temp)(struct mt_cpu_dvfs *p);                         /* return temperature         */ // TODO: necessary???
	int (*setup_power_table)(struct mt_cpu_dvfs *p);

	/* for freq change (PLL/MUX) */
	unsigned int (*get_cur_phy_freq)(struct mt_cpu_dvfs *p);                 /* return (physical) freq (KHz) */
	void (*set_cur_freq)(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz); /* set freq  */

	/* for volt change (PMICWRAP/extBuck) */
	unsigned int (*get_cur_volt)(struct mt_cpu_dvfs *p);                     /* return volt (mV)                        */
	int (*set_cur_volt)(struct mt_cpu_dvfs *p, unsigned int mv);             /* set volt, return 0 (success), -1 (fail) */
};

struct mt_cpu_dvfs {
	const char *name;
	unsigned int cpu_id;                    /* for cpufreq */
	struct mt_cpu_dvfs_ops *ops;

	/* opp (freq) table */
	struct mt_cpu_freq_info *opp_tbl;       /* OPP table */
	int nr_opp_tbl;                         /* size for OPP table */
	int idx_opp_tbl;                        /* current OPP idx */
	int idx_normal_max_opp;                 /* idx for normal max OPP */
	int idx_opp_tbl_for_late_resume;	/* keep the setting for late resume */
	int idx_opp_tbl_for_pwr_thro;		/* keep the setting for power throttling */

	struct cpufreq_frequency_table *freq_tbl_for_cpufreq; /* freq table for cpufreq */

	/* power table */
	struct mt_cpu_power_info *power_tbl;
	unsigned int nr_power_tbl;

	/* enable/disable DVFS function */
	int dvfs_disable_count;
	bool cpufreq_pause; // TODO: FIXME, rename (used by suspend/resume to avoid I2C access)
	bool dvfs_disable_by_ptpod;
	bool limit_max_freq_early_suspend; // TODO: rename (dvfs_disable_by_early_suspend)
	bool is_fixed_freq; // TODO: FIXME, rename (used by sysfs)

	/* limit for thermal */
	unsigned int limited_max_ncpu;
	unsigned int limited_max_freq;
	// unsigned int limited_min_freq; // TODO: remove it???

	unsigned int thermal_protect_limited_power;

	/* limit for HEVC (via. sysfs) */
	unsigned int limited_freq_by_hevc;

	/* limit max freq from user */
	unsigned int limited_max_freq_by_user;

	/* for ramp down */
	int ramp_down_count;
	int ramp_down_count_const;

	/* param for micro throttling */
	bool downgrade_freq_for_ptpod;

	int over_max_cpu;
	int ptpod_temperature_limit_1;
	int ptpod_temperature_limit_2;
	int ptpod_temperature_time_1;
	int ptpod_temperature_time_2;

	int pre_online_cpu;
	unsigned int pre_freq;
	unsigned int downgrade_freq;

	unsigned int downgrade_freq_counter;
	unsigned int downgrade_freq_counter_return;

	unsigned int downgrade_freq_counter_limit;
	unsigned int downgrade_freq_counter_return_limit;

	/* turbo mode */
	unsigned int turbo_mode;

	/* power throttling */
	unsigned int pwr_thro_mode;
};

/* for thermal */
static int setup_power_table(struct mt_cpu_dvfs *p);

/* for freq change (PLL/MUX) */
static unsigned int get_cur_phy_freq(struct mt_cpu_dvfs *p);
static void set_cur_freq(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz);

/* for volt change (PMICWRAP/extBuck) */
static unsigned int get_cur_volt_little(struct mt_cpu_dvfs *p);
static int set_cur_volt_little(struct mt_cpu_dvfs *p, unsigned int mv);

static unsigned int get_cur_volt_big(struct mt_cpu_dvfs *p);
static int set_cur_volt_big(struct mt_cpu_dvfs *p, unsigned int mv);

static struct mt_cpu_dvfs_ops little_ops = {
	.setup_power_table = setup_power_table,

	.get_cur_phy_freq = get_cur_phy_freq,
	.set_cur_freq = set_cur_freq,

	.get_cur_volt = get_cur_volt_little,
	.set_cur_volt = set_cur_volt_little,
};

static struct mt_cpu_dvfs_ops big_ops = {
	.setup_power_table = setup_power_table,

	.get_cur_phy_freq = get_cur_phy_freq,
	.set_cur_freq = set_cur_freq,

	.get_cur_volt = get_cur_volt_big,
	.set_cur_volt = set_cur_volt_big,
};

static struct mt_cpu_dvfs cpu_dvfs[] = { // TODO: FIXME, big/LITTLE exclusive, NR_MT_CPU_DVFS
	[MT_CPU_DVFS_LITTLE]    = {
		.name                           = __stringify(MT_CPU_DVFS_LITTLE),
		.cpu_id                         = MT_CPU_DVFS_LITTLE, // TODO: FIXME
		.ops                            = &little_ops,

		.over_max_cpu                   = 4,
		.ptpod_temperature_limit_1      = 110000,
		.ptpod_temperature_limit_2      = 120000,
		.ptpod_temperature_time_1       = 1,
		.ptpod_temperature_time_2       = 4,
		.pre_online_cpu                 = 0,
		.pre_freq                       = 0,
		.downgrade_freq                 = 0,
		.downgrade_freq_counter         = 0,
		.downgrade_freq_counter_return  = 0,
		.downgrade_freq_counter_limit   = 0,
		.downgrade_freq_counter_return_limit = 0,

		.ramp_down_count_const		= RAMP_DOWN_TIMES,

		.turbo_mode			= 1,

		.idx_opp_tbl_for_pwr_thro	= -1,
	},

	[MT_CPU_DVFS_BIG]       = {
		.name                           = __stringify(MT_CPU_DVFS_BIG),
		.cpu_id                         = MT_CPU_DVFS_BIG, // TODO: FIXME
		.ops                            = &big_ops,

		.over_max_cpu                   = 4,
		.ptpod_temperature_limit_1      = 110000,
		.ptpod_temperature_limit_2      = 120000,
		.ptpod_temperature_time_1       = 1,
		.ptpod_temperature_time_2       = 4,
		.pre_online_cpu                 = 0,
		.pre_freq                       = 0,
		.downgrade_freq                 = 0,
		.downgrade_freq_counter         = 0,
		.downgrade_freq_counter_return  = 0,
		.downgrade_freq_counter_limit   = 0,
		.downgrade_freq_counter_return_limit = 0,

		.ramp_down_count_const		= RAMP_DOWN_TIMES,

		.turbo_mode			= 1,

		.idx_opp_tbl_for_pwr_thro	= -1,
	},
};

#define for_each_cpu_dvfs(i, p)                 for (i = 0, p = cpu_dvfs; i < NR_MT_CPU_DVFS; i++, p = &cpu_dvfs[i])
#define cpu_dvfs_is(p, id)                      (p == &cpu_dvfs[id])
#define cpu_dvfs_is_availiable(p)               (p->opp_tbl)
#define cpu_dvfs_get_name(p)                    (p->name)

#define cpu_dvfs_get_cur_freq(p)                (p->opp_tbl[p->idx_opp_tbl].cpufreq_khz)
#define cpu_dvfs_get_freq_by_idx(p, idx)        (p->opp_tbl[idx].cpufreq_khz)
#define cpu_dvfs_get_max_freq(p)                (p->opp_tbl[0].cpufreq_khz)
#define cpu_dvfs_get_normal_max_freq(p)         (p->opp_tbl[p->idx_normal_max_opp].cpufreq_khz)
#define cpu_dvfs_get_min_freq(p)                (p->opp_tbl[p->nr_opp_tbl - 1].cpufreq_khz)

#define cpu_dvfs_get_cur_volt(p)                (p->opp_tbl[p->idx_opp_tbl].cpufreq_volt)
#define cpu_dvfs_get_volt_by_idx(p, idx)        (p->opp_tbl[idx].cpufreq_volt)

static struct mt_cpu_dvfs *id_to_cpu_dvfs(enum mt_cpu_dvfs_id id)
{
	return (id < NR_MT_CPU_DVFS) ? &cpu_dvfs[id] : NULL;
}

/* DVFS OPP table */

#define NR_MAX_OPP_TBL  8       // TODO: refere to PTP-OD
#define NR_MAX_CPU      4       // TODO: one cluster, any kernel define for this - CONFIG_NR_CPU???

/* LITTLE CPU LEVEL 0 */
static struct mt_cpu_freq_info opp_tbl_little_e1_0[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* LITTLE CPU LEVEL 1 */
static struct mt_cpu_freq_info opp_tbl_little_e1_1[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* LITTLE CPU LEVEL 2 */
static struct mt_cpu_freq_info opp_tbl_little_e1_2[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* big CPU LEVEL 0 */
static struct mt_cpu_freq_info opp_tbl_big_e1_0[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
};

/* big CPU LEVEL 1 */
static struct mt_cpu_freq_info opp_tbl_big_e1_1[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
};

/* big CPU LEVEL 2 */
static struct mt_cpu_freq_info opp_tbl_big_e1_2[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
};

/* LITTLE CPU LEVEL 0 (E2) */
static struct mt_cpu_freq_info opp_tbl_little_e2_0[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* LITTLE CPU LEVEL 1 (E2) */
static struct mt_cpu_freq_info opp_tbl_little_e2_1[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* LITTLE CPU LEVEL 2 (E2) */
static struct mt_cpu_freq_info opp_tbl_little_e2_2[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* big CPU LEVEL 0 (E2) */
static struct mt_cpu_freq_info opp_tbl_big_e2_0[] = {
	OP(2002000, 1100),
	OP(1781000, 1060),
	OP(1560000, 1019),
	OP(1352000, 981),
	OP(DVFS_BIG_F4, 943),
	OP(DVFS_BIG_F5, 916),
	OP(DVFS_BIG_F6, 880),
	OP(DVFS_BIG_F7, 806),
};

/* big CPU LEVEL 1 (E2) */
static struct mt_cpu_freq_info opp_tbl_big_e2_1[] = {
	OP(2002000, 1100),
	OP(1781000, 1060),
	OP(1560000, 1019),
	OP(1352000, 981),
	OP(DVFS_BIG_F4, 943),
	OP(DVFS_BIG_F5, 916),
	OP(DVFS_BIG_F6, 880),
	OP(DVFS_BIG_F7, 806),
};

/* big CPU LEVEL 2 (E2) */
static struct mt_cpu_freq_info opp_tbl_big_e2_2[] = {
	OP(2210000, 1100),
	OP(1937000, 1046),
	OP(1664000, 1003),
	OP(1404000, 962),
	OP(DVFS_BIG_F4, 921),
	OP(DVFS_BIG_F5, 899),
	OP(DVFS_BIG_F6, 868),
	OP(DVFS_BIG_F7, 805),
};

struct opp_tbl_info {
	struct mt_cpu_freq_info *const opp_tbl;
	const int size;
};

#define ARRAY_AND_SIZE(x) (x), ARRAY_SIZE(x)

static struct opp_tbl_info opp_tbls_little[2][3] = {
	{
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)] = { ARRAY_AND_SIZE(opp_tbl_little_e1_0), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = { ARRAY_AND_SIZE(opp_tbl_little_e1_1), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = { ARRAY_AND_SIZE(opp_tbl_little_e1_2), },
	},
	{
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)] = { ARRAY_AND_SIZE(opp_tbl_little_e2_0), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = { ARRAY_AND_SIZE(opp_tbl_little_e2_1), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = { ARRAY_AND_SIZE(opp_tbl_little_e2_2), },
	},
};

static struct opp_tbl_info opp_tbls_big[2][3] = {
	{
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)] = { ARRAY_AND_SIZE(opp_tbl_big_e1_0), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = { ARRAY_AND_SIZE(opp_tbl_big_e1_1), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = { ARRAY_AND_SIZE(opp_tbl_big_e1_2), },
	},
	{
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)] = { ARRAY_AND_SIZE(opp_tbl_big_e2_0), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = { ARRAY_AND_SIZE(opp_tbl_big_e2_1), },
		[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = { ARRAY_AND_SIZE(opp_tbl_big_e2_2), },
	},
};

/* for freq change (PLL/MUX) */

#define PLL_FREQ_STEP		(13000)		/* KHz */

// #define PLL_MAX_FREQ		(1989000)	/* KHz */ // TODO: check max freq
#define PLL_MIN_FREQ		(130000)	/* KHz */
#define PLL_DIV1_FREQ		(1001000)	/* KHz */
#define PLL_DIV2_FREQ		(520000)	/* KHz */
#define PLL_DIV4_FREQ		(260000)	/* KHz */
#define PLL_DIV8_FREQ		(PLL_MIN_FREQ)	/* KHz */

#define DDS_DIV1_FREQ		(0x0009A000)	/* 1001MHz */
#define DDS_DIV2_FREQ		(0x010A0000)	/* 520MHz  */
#define DDS_DIV4_FREQ		(0x020A0000)	/* 260MHz  */
#define DDS_DIV8_FREQ		(0x030A0000)	/* 130MHz  */

/* turbo mode */

#define TURBO_MODE_BOUNDARY_CPU_NUM	2
#define OPP7_BOUNDARY_CPU_NUM	1
/* idx sort by temp from low to high */
enum turbo_mode {
	TURBO_MODE_2,
	TURBO_MODE_1,
	TURBO_MODE_NONE,

	NR_TURBO_MODE,
};

/* idx sort by temp from low to high */
struct turbo_mode_cfg {
	int temp;       /* degree x 1000 */
	int freq_delta; /* percentage    */
	int volt_delta; /* mv            */
} turbo_mode_cfg[] = {
	[TURBO_MODE_2] = {
		.temp = 65000,
		.freq_delta = 10,
		.volt_delta = 40,
	},
	[TURBO_MODE_1] = {
		.temp = 85000,
		.freq_delta = 5,
		.volt_delta = 20,
	},
	[TURBO_MODE_NONE] = {
		.temp = 125000,
		.freq_delta = 0,
		.volt_delta = 0,
	},
};

#define TURBO_MODE_FREQ(mode, freq) (((freq * (100 + turbo_mode_cfg[mode].freq_delta)) / PLL_FREQ_STEP) / 100 * PLL_FREQ_STEP)
#define TURBO_MODE_VOLT(mode, volt) (volt + turbo_mode_cfg[mode].volt_delta)

static int num_online_cpus_little_delta = 0;
static int num_online_cpus_big_delta = 0;

static int num_online_cpus_little_delta_for_403 = 0;

static enum turbo_mode get_turbo_mode(struct mt_cpu_dvfs *p, unsigned int target_khz)
{
	enum turbo_mode mode = TURBO_MODE_NONE;
	int temp = tscpu_get_temp_by_bank(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? THERMAL_BANK0 : THERMAL_BANK1);
	int num_online_cpus_little, num_online_cpus_big;
	int i;

	if (-1 == hps_get_num_online_cpus(&num_online_cpus_little, &num_online_cpus_big)) {
		num_online_cpus_little = 4;
		num_online_cpus_big = 4;
	} else {
		num_online_cpus_little += num_online_cpus_little_delta;
		num_online_cpus_big += num_online_cpus_big_delta;
	}

	if (p->turbo_mode
	    && target_khz == cpu_dvfs_get_freq_by_idx(p, 0)
	    && ((cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? num_online_cpus_little : num_online_cpus_big)) <= TURBO_MODE_BOUNDARY_CPU_NUM
	   ) {
		for (i = 0; i < NR_TURBO_MODE; i++) {
			if (temp < turbo_mode_cfg[i].temp) {
				mode = i;
				break;
			}
		}
	}

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC

	if (TURBO_MODE_NONE != mode) {
		if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
			*p_dvfs_state |= (1 << CPU_DVFS_LITTLE_IS_TURBO);
		else
			*p_dvfs_state |= (1 << CPU_DVFS_BIG_IS_TURBO);

		//while(1);
	} else {
		if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
			*p_dvfs_state &= ~(1 << CPU_DVFS_LITTLE_IS_TURBO);
		else
			*p_dvfs_state &= ~(1 << CPU_DVFS_BIG_IS_TURBO);
	}

#endif

	if (TURBO_MODE_NONE != mode) cpufreq_ver("%s(), mode = %d, temp = %d, target_khz = %d (%d), num_online_cpus_little = %d, num_online_cpus_big = %d\n", __func__, mode, temp, target_khz, TURBO_MODE_FREQ(mode, target_khz), num_online_cpus_little, num_online_cpus_big); // <-XXX

	return mode;
}

/* for PTP-OD */

static int _set_cur_volt_locked(struct mt_cpu_dvfs *p, unsigned int mv)
{
	int ret = -1;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p) || p->cpufreq_pause) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 0;
	}

	if (chip_ver != CHIP_SW_VER_02 || cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
		/* update for deep idle */
		mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
#ifdef MTK_FORCE_CLUSTER1
					(chip_ver == CHIP_SW_VER_01) ? IDX_DI_VSRAM_CA15L_NORMAL : IDX_DI_VPROC_CA7_NORMAL,
#else
					IDX_DI_VPROC_CA7_NORMAL,
#endif
					VOLT_TO_PMIC_VAL(mv + (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 0 : NORMAL_DIFF_VRSAM_VPROC))
				       );
	}

	/* update for suspend */
	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)
	    && pw.set[PMIC_WRAP_PHASE_SUSPEND]._[IDX_SP_VSRAM_CA15L_PWR_ON].cmd_addr == PMIC_ADDR_VSRAM_CA15L_VOSEL_ON
	   )
		mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_SUSPEND,
					IDX_SP_VSRAM_CA15L_PWR_ON,
					VOLT_TO_PMIC_VAL(mv + NORMAL_DIFF_VRSAM_VPROC)
				       );

	/* set volt */
	ret = p->ops->set_cur_volt(p, mv);

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

static int _restore_default_volt(struct mt_cpu_dvfs *p)
{
	unsigned long flags;
	int i;
	int ret = -1;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 0;
	}

	cpufreq_lock(flags);

	/* restore to default volt */
	for (i = 0; i < p->nr_opp_tbl; i++)
		p->opp_tbl[i].cpufreq_volt = p->opp_tbl[i].cpufreq_volt_org;

	/* set volt */
	ret = _set_cur_volt_locked(p,
				   TURBO_MODE_VOLT(get_turbo_mode(p, cpu_dvfs_get_cur_freq(p)),
						   cpu_dvfs_get_cur_volt(p)
						  )
				  );

	cpufreq_unlock(flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

unsigned int mt_cpufreq_max_frequency_by_DVS(enum mt_cpu_dvfs_id id, int idx) // TODO: rename to mt_cpufreq_get_freq_by_idx()
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p) || idx >= p->nr_opp_tbl) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_freq_by_idx(p, idx);
}
EXPORT_SYMBOL(mt_cpufreq_max_frequency_by_DVS);

int mt_cpufreq_voltage_set_by_ptpod(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl) // TODO: rename to mt_cpufreq_update_volt()
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned long flags;
	int i;
	int ret = -1;

	FUNC_ENTER(FUNC_LV_API);

#if 0	// TODO: remove it latter

	if (id != 0)
		return 0; // TODO: FIXME, just for E1

#endif	// TODO: remove it latter

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	BUG_ON(nr_volt_tbl > p->nr_opp_tbl);

	cpufreq_lock(flags);

	/* update volt table */
	for (i = 0; i < nr_volt_tbl; i++)
		p->opp_tbl[i].cpufreq_volt = PMIC_VAL_TO_VOLT(volt_tbl[i]);

	/* set volt */
	ret = _set_cur_volt_locked(p,
				   TURBO_MODE_VOLT(get_turbo_mode(p, cpu_dvfs_get_cur_freq(p)),
						   cpu_dvfs_get_cur_volt(p)
						  )
				  );

	cpufreq_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);

	return ret;
}
EXPORT_SYMBOL(mt_cpufreq_voltage_set_by_ptpod);

void mt_cpufreq_return_default_DVS_by_ptpod(enum mt_cpu_dvfs_id id) // TODO: rename to mt_cpufreq_restore_default_volt()
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return;
	}

	_restore_default_volt(p);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_return_default_DVS_by_ptpod);

static unsigned int _cpu_freq_calc(unsigned int con1, unsigned int ckdiv1)
{
	unsigned int freq = 0;

#if 0   /* method 1 */
	static const unsigned int pll_vcodivsel_map[2] = {1, 2};
	static const unsigned int pll_prediv_map[4] = {1, 2, 4, 4};
	static const unsigned int pll_posdiv_map[8] = {1, 2, 4, 8, 16, 16, 16, 16};
	static const unsigned int pll_fbksel_map[4] = {1, 2, 4, 4};
	static const unsigned int pll_n_info_map[14] = { /* assume fin = 26MHz */
		13000000,
		6500000,
		3250000,
		1625000,
		812500,
		406250,
		203125,
		101563,
		50782,
		25391,
		12696,
		6348,
		3174,
		1587,
	};

	unsigned int posdiv    = _GET_BITS_VAL_(26 : 24, con1);
	unsigned int vcodivsel = 0; /* _GET_BITS_VAL_(19 : 19, con0); */ /* XXX: always zero */
	unsigned int prediv    = 0; /* _GET_BITS_VAL_(5 : 4, con0);   */ /* XXX: always zero */
	unsigned int n_info_i  = _GET_BITS_VAL_(20 : 14, con1);
	unsigned int n_info_f  = _GET_BITS_VAL_(13 : 0, con1);

	int i;
	unsigned int mask;
	unsigned int vco_i = 0;
	unsigned int vco_f = 0;

	posdiv = pll_posdiv_map[posdiv];
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_i = 26 * n_info_i;

	for (i = 0; i < 14; i++) {
		mask = 1U << (13 - i);

		if (n_info_f & mask) {
			vco_f += pll_n_info_map[i];

			if (!(n_info_f & (mask - 1))) /* could break early if remaining bits are 0 */
				break;
		}
	}

	vco_f = (vco_f + 1000000 / 2) / 1000000; /* round up */

	freq = (vco_i + vco_f) * 1000 * vcodivsel / prediv / posdiv; /* KHz */
#else   /* method 2 */
	con1 &= _BITMASK_(26 : 0);

	if (con1 >= DDS_DIV8_FREQ) {
		freq = DDS_DIV8_FREQ;
		freq = PLL_DIV8_FREQ + (((con1 - freq) / 0x2000) * PLL_FREQ_STEP / 8);
	} else if (con1 >= DDS_DIV4_FREQ) {
		freq = DDS_DIV4_FREQ;
		freq = PLL_DIV4_FREQ + (((con1 - freq) / 0x2000) * PLL_FREQ_STEP / 4);
	} else if (con1 >= DDS_DIV2_FREQ) {
		freq = DDS_DIV2_FREQ;
		freq = PLL_DIV2_FREQ + (((con1 - freq) / 0x2000) * PLL_FREQ_STEP / 2);
	} else if (con1 >= DDS_DIV1_FREQ) {
		freq = DDS_DIV1_FREQ;
		freq = PLL_DIV1_FREQ + (((con1 - freq) / 0x2000) * PLL_FREQ_STEP);
	} else
		BUG();

#endif

	FUNC_ENTER(FUNC_LV_HELP);

	switch (ckdiv1) {
	case 9:
		freq = freq * 3 / 4;
		break;

	case 10:
		freq = freq * 2 / 4;
		break;

	case 11:
		freq = freq * 1 / 4;
		break;

	case 17:
		freq = freq * 4 / 5;
		break;

	case 18:
		freq = freq * 3 / 5;
		break;

	case 19:
		freq = freq * 2 / 5;
		break;

	case 20:
		freq = freq * 1 / 5;
		break;

	case 25:
		freq = freq * 5 / 6;
		break;

	case 26:
		freq = freq * 4 / 6;
		break;

	case 27:
		freq = freq * 3 / 6;
		break;

	case 28:
		freq = freq * 2 / 6;
		break;

	case 29:
		freq = freq * 1 / 6;
		break;

	case 8:
	case 16:
	case 24:
		break;

	default:
		// BUG(); // TODO: FIXME
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return freq; // TODO: adjust by ptp level???
}

static unsigned int get_cur_phy_freq(struct mt_cpu_dvfs *p)
{
	unsigned int con1;
	unsigned int ckdiv1;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	con1 = cpufreq_read(4 + (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_CON0 : ARMCA15PLL_CON0));

	ckdiv1 = cpufreq_read(TOP_CKDIV1);
	ckdiv1 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? _GET_BITS_VAL_(4 : 0, ckdiv1) : _GET_BITS_VAL_(9 : 5, ckdiv1);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return _cpu_freq_calc(con1, ckdiv1);
}

static unsigned int _mt_cpufreq_get_cur_phy_freq(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return p->ops->get_cur_phy_freq(p);
}

static unsigned int _cpu_dds_calc(unsigned int khz) /* XXX: NOT OK FOR 1007.5MHz */
{
	unsigned int dds;

	FUNC_ENTER(FUNC_LV_HELP);

	if (khz >= PLL_DIV1_FREQ)
		dds = DDS_DIV1_FREQ + ((khz - PLL_DIV1_FREQ) / PLL_FREQ_STEP) * 0x2000;
	else if (khz >= PLL_DIV2_FREQ)
		dds = DDS_DIV2_FREQ + ((khz - PLL_DIV2_FREQ) * 2 / PLL_FREQ_STEP) * 0x2000;
	else if (khz >= PLL_DIV4_FREQ)
		dds = DDS_DIV4_FREQ + ((khz - PLL_DIV4_FREQ) * 4 / PLL_FREQ_STEP) * 0x2000;
	else if (khz >= PLL_DIV8_FREQ)
		dds = DDS_DIV8_FREQ + ((khz - PLL_DIV8_FREQ) * 8 / PLL_FREQ_STEP) * 0x2000;
	else
		BUG();

	FUNC_EXIT(FUNC_LV_HELP);

	return dds;
}

static void _cpu_clock_switch(struct mt_cpu_dvfs *p, enum top_ckmuxsel sel)
{
	unsigned int val = cpufreq_read(TOP_CKMUXSEL);
	unsigned int mask = _BITMASK_(1 : 0);

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(sel >= NR_TOP_CKMUXSEL);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)) {
		sel <<= 2;
		mask <<= 2; /* _BITMASK_(3 : 2) */
	}

	cpufreq_write(TOP_CKMUXSEL, (val & ~mask) | sel);

	FUNC_EXIT(FUNC_LV_HELP);
}

static enum top_ckmuxsel _get_cpu_clock_switch(struct mt_cpu_dvfs *p)
{
	unsigned int val = cpufreq_read(TOP_CKMUXSEL);
	unsigned int mask = _BITMASK_(1 : 0);

	FUNC_ENTER(FUNC_LV_HELP);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG))
		val = (val & (mask << 2)) >> 2; /* _BITMASK_(3 : 2) */
	else
		val &= mask;                    /* _BITMASK_(1 : 0) */

	FUNC_EXIT(FUNC_LV_HELP);

	return val;
}

int mt_cpufreq_clock_switch(enum mt_cpu_dvfs_id id, enum top_ckmuxsel sel)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	if (!p)
		return -1;

	_cpu_clock_switch(p, sel);

	return 0;
}

enum top_ckmuxsel mt_cpufreq_get_clock_switch(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	if (!p)
		return -1;

	return _get_cpu_clock_switch(p);
}

static void set_cur_freq(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz)
{
	unsigned int addr_con1;
	unsigned int dds;
	unsigned int is_fhctl_used;
	unsigned int cur_volt;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (cur_khz < PLL_DIV1_FREQ && PLL_DIV1_FREQ < target_khz) {
		set_cur_freq(p, cur_khz, PLL_DIV1_FREQ);
		cur_khz = PLL_DIV1_FREQ;
	} else if (target_khz <= PLL_DIV1_FREQ && PLL_DIV1_FREQ < cur_khz) {
		set_cur_freq(p, cur_khz, PLL_DIV1_FREQ + PLL_FREQ_STEP);
		cur_khz = PLL_DIV1_FREQ + PLL_FREQ_STEP;
	}

	addr_con1 = 4 + (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_CON0 : ARMCA15PLL_CON0);

#if defined(CONFIG_CPU_DVFS_BRINGUP)
	is_fhctl_used = 0;
#else
	is_fhctl_used = (PLL_DIV1_FREQ < target_khz) ? 1 : 0;
#endif

	cpufreq_ver("@%s():%d, cur_khz = %d, target_khz = %d, is_fhctl_used = %d\n", __func__, __LINE__, cur_khz, target_khz, is_fhctl_used);

	/* calc dds */
	dds = _cpu_dds_calc(target_khz);

	if (!is_fhctl_used) {
		// enable_clock(MT_CG_MPLL_D2, "CPU_DVFS");

#if (DVFS_BIG_F4 != DVFS_LITTLE_F4)
#error "DVFS_BIG_F4 != DVFS_LITTLE_F4"
#endif
		cur_volt = p->ops->get_cur_volt(p);

		if (cur_volt < cpu_dvfs_get_volt_by_idx(p, 4)) /* MAINPLL_FREQ ~= DVFS_BIG_F4 */
			p->ops->set_cur_volt(p, cpu_dvfs_get_volt_by_idx(p, 4));
		else
			cur_volt = 0;

		_cpu_clock_switch(p, TOP_CKMUXSEL_MAINPLL);
	}

	/* set dds */
	if (!is_fhctl_used)
		cpufreq_write(addr_con1, dds | _BIT_(31)); /* CHG */
	else {
		BUG_ON(dds & _BITMASK_(26 : 24)); /* should not use posdiv */
#ifndef __KERNEL__
		freqhopping_dvt_dvfs_enable(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_ID : ARMCA15PLL_ID, target_khz);
#else  /* __KERNEL__ */
		mt_dfs_armpll(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? FH_ARMCA7_PLLID : FH_ARMCA15_PLLID, dds);
#endif /* ! __KERNEL__ */
	}

	udelay(PLL_SETTLE_TIME);

	if (!is_fhctl_used) {
		_cpu_clock_switch(p, TOP_CKMUXSEL_ARMPLL);

		if (cur_volt)
			p->ops->set_cur_volt(p, cur_volt);

		// disable_clock(MT_CG_MPLL_D2, "CPU_DVFS");
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

/* for volt change (PMICWRAP/extBuck) */

static unsigned int get_cur_volt_little(struct mt_cpu_dvfs *p)
{
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VPROC_CA7_EN, &rdata);

	rdata &= _BITMASK_(0 : 0); /* enable or disable (i.e. 0mv or not) */

	if (rdata) { /* enabled i.e. not 0mv */
		pwrap_read(PMIC_ADDR_VPROC_CA7_VOSEL_ON, &rdata);

		rdata = PMIC_VAL_TO_VOLT(rdata);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata; /* mv: vproc */
}

static unsigned int get_cur_vsram_big(struct mt_cpu_dvfs *p)
{
	unsigned int rdata;
	unsigned int retry_cnt = 5;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VSRAM_CA15L_EN, &rdata);

	rdata &= _BITMASK_(10 : 10); /* enable or disable (i.e. 0mv or not) */

	if (rdata) { /* enabled i.e. not 0mv */
		do {
			pwrap_read(PMIC_ADDR_VSRAM_CA15L_VOSEL_ON, &rdata);
		} while (rdata == _BITMASK_(10 : 10) && retry_cnt--);
		rdata &= 0x7F;
		rdata = PMIC_VAL_TO_VOLT(rdata);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata; /* mv: vproc */
}

static unsigned int get_cur_volt_big(struct mt_cpu_dvfs *p)
{
	unsigned char ret_val;
	unsigned int ret_mv;
	unsigned int retry_cnt = 5;

	FUNC_ENTER(FUNC_LV_LOCAL);

	do {
		if (!da9210_read_byte(0xD8, &ret_val)) { // TODO: FIXME, it is better not to access da9210 directly
			cpufreq_err("%s(), fail to read ext buck volt\n", __func__);
			ret_mv = 0;
		} else
			ret_mv = EXTBUCK_VAL_TO_VOLT(ret_val);
	} while (ret_mv == EXTBUCK_VAL_TO_VOLT(0) && retry_cnt--); // XXX: EXTBUCK_VAL_TO_VOLT(0) is impossible setting and need to retry

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret_mv;
}

unsigned int mt_cpufreq_cur_vproc(enum mt_cpu_dvfs_id id) // TODO: rename it to mt_cpufreq_get_cur_volt()
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);
	BUG_ON(NULL == p->ops);

	FUNC_EXIT(FUNC_LV_API);

	return p->ops->get_cur_volt(p);
}
EXPORT_SYMBOL(mt_cpufreq_cur_vproc);

static int set_cur_volt_little(struct mt_cpu_dvfs *p, unsigned int mv) /* mv: vproc */
{
	unsigned int cur_mv = get_cur_volt_little(p);

	FUNC_ENTER(FUNC_LV_LOCAL);

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	*p_little_volt = VOLT_TO_PMIC_VAL(mv);
#endif

	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_SODI, IDX_SO_VPROC_CA7_NORMAL, VOLT_TO_PMIC_VAL(mv));
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VPROC_CA7, VOLT_TO_PMIC_VAL(mv));
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VPROC_CA7);

	/* delay for scaling up */
	if (mv > cur_mv)
		udelay(PMIC_SETTLE_TIME(cur_mv, mv));

	if (NULL != g_pCpuVoltSampler)
		g_pCpuVoltSampler(MT_CPU_DVFS_LITTLE, mv);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return 0;
}

static void dump_opp_table(struct mt_cpu_dvfs *p)
{
	int i;

	cpufreq_ver("[%s/%d]\n"
		    "cpufreq_oppidx = %d\n",
		    p->name, p->cpu_id,
		    p->idx_opp_tbl
		   );

	for (i = 0; i < p->nr_opp_tbl; i++) {
		cpufreq_ver("\tOP(%d, %d),\n",
			    cpu_dvfs_get_freq_by_idx(p, i),
			    cpu_dvfs_get_volt_by_idx(p, i)
			   );
	}
}

static int set_cur_volt_big(struct mt_cpu_dvfs *p, unsigned int mv) /* mv: vproc */
{
	unsigned int cur_vsram_mv = get_cur_vsram_big(p);
	unsigned int cur_vproc_mv = get_cur_volt_big(p);
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	*p_big_volt = VOLT_TO_EXTBUCK_VAL(mv);
#endif

	if (unlikely(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))))) {
		unsigned int addr[] = { 0x0100, 0x0102, 0x8000, 0x8002, 0x018E, 0x0196, 0x80F8, 0x8100, };
		unsigned int rdata;
		unsigned char ret_val;
		int i;

		dump_opp_table(p);

		cpufreq_err("@%s():%d, cur_vsram_mv = %d (%d), cur_vproc_mv = %d (%d), 0xF000D0C4 = 0x%08X, 0xF000D0C8 = 0x%08X, MAINPLL_CON0 = 0x%08X, MAINPLL_CON1 = 0x%08X, MAINPLL_PWR_CON0 = 0x%08X, CLK_CFG_0 = 0x%08X, TOP_DCMCTL = 0x%08X, PERI_PDN0_STA = 0x%08X\n",
			    __func__,
			    __LINE__,
			    cur_vsram_mv,
			    cpu_dvfs_get_cur_volt(p),
			    cur_vproc_mv,
			    cpu_dvfs_get_cur_volt(p) + NORMAL_DIFF_VRSAM_VPROC,
			    *(volatile unsigned int *)0xF000D0C4,
			    *(volatile unsigned int *)0xF000D0C8,
			    *(volatile unsigned int *)MAINPLL_CON0,
			    *(volatile unsigned int *)MAINPLL_CON1,
			    *(volatile unsigned int *)MAINPLL_PWR_CON0,
			    *(volatile unsigned int *)CLK_CFG_0,
			    *(volatile unsigned int *)(INFRACFG_AO_BASE + 0x10),
			    *(volatile unsigned int *)PERI_PDN0_STA
			   );

		for (i = 0; i < ARRAY_SIZE(addr); i++) {
			pwrap_read(addr[i], &rdata);
			cpufreq_err("[0x%08X] = 0x%08X\n", addr[i], rdata);
		}

		for (i = 0; i < 10; i++) {
			extern kal_uint32 da9210_config_interface(kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT);
			extern kal_uint32 da9210_read_interface(kal_uint8 RegNum, kal_uint8 * val, kal_uint8 MASK, kal_uint8 SHIFT);
			da9210_config_interface(0x0, 0x1, 0x1, 7); // page reverts to 0 after one access
			da9210_config_interface(0x0, 0x2, 0xF, 0); // select to page 2,3
			da9210_read_interface(0x5, &ret_val, 0xF, 4);
			cpufreq_err("%s(), da9210 ID = %x\n", __func__, ret_val);
		}

		for (i = 0; i < 10; i++) {
			extern kal_uint32 tps6128x_read_byte(kal_uint8 cmd, kal_uint8 * returnData);
			tps6128x_read_byte(0x3, &ret_val); // read val = 0xb
			cpufreq_err("%s(), tps6128x ID = %x\n", __func__, ret_val);
		}

		aee_kernel_warning(TAG, "@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n", __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);

		cur_vproc_mv = cpu_dvfs_get_cur_volt(p);
		cur_vsram_mv = cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC;
	}

	/* UP */
	if (mv > cur_vproc_mv) {
		unsigned int target_vsram_mv = mv + NORMAL_DIFF_VRSAM_VPROC;
		unsigned int next_vsram_mv;

		do {
			next_vsram_mv = min(((MAX_DIFF_VSRAM_VPROC - 20) + cur_vproc_mv), target_vsram_mv);

			/* update vsram */
			cur_vsram_mv = max(next_vsram_mv, 930); // TODO: use define for 930mv // <-XXX

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n", __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}

			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_SODI, IDX_SO_VSRAM_CA15L_NORMAL, VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VSRAM_CA15L, VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_apply_pmic_cmd(IDX_NM_VSRAM_CA15L);

			/* update vproc */
			cur_vproc_mv = next_vsram_mv - NORMAL_DIFF_VRSAM_VPROC;

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n", __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}

			if (!ext_buck_vosel(cur_vproc_mv * 100)) {
				cpufreq_err("%s(), fail to set ext buck volt\n", __func__);
				ret = -1;
			}

			udelay(PMIC_SETTLE_TIME(cur_vproc_mv - MAX_DIFF_VSRAM_VPROC, cur_vproc_mv)); // TODO: always fix max gap <- refine it???

			// cpufreq_dbg("@%s(), UP, cur_vsram_mv = %d, cur_vproc_mv = %d, delay = %d\n", __func__, cur_vsram_mv, cur_vproc_mv, PMIC_SETTLE_TIME(cur_vproc_mv - MAX_DIFF_VSRAM_VPROC, cur_vproc_mv));
		} while (target_vsram_mv > cur_vsram_mv);
	}
	/* DOWN */
	else if (mv < cur_vproc_mv) {
		unsigned int next_vproc_mv;
		unsigned int next_vsram_mv = cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC;

		do {
			next_vproc_mv = max((next_vsram_mv - (MAX_DIFF_VSRAM_VPROC - 20)), mv);

			/* update vproc */
			cur_vproc_mv = next_vproc_mv;

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n", __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}

			if (!ext_buck_vosel(cur_vproc_mv * 100)) {
				cpufreq_err("%s(), fail to set ext buck volt\n", __func__);
				ret = -1;
			}

			/* update vsram */
			next_vsram_mv = cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC;
			cur_vsram_mv = max(next_vsram_mv, 930); // TODO: use define for 930mv // <-XXX

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n", __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}

			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_SODI, IDX_SO_VSRAM_CA15L_NORMAL, VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VSRAM_CA15L, VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_apply_pmic_cmd(IDX_NM_VSRAM_CA15L);

			udelay(PMIC_SETTLE_TIME(cur_vproc_mv + MAX_DIFF_VSRAM_VPROC, cur_vproc_mv)); // TODO: always fix max gap <- refine it???

			// cpufreq_dbg("@%s(), DOWN, cur_vsram_mv = %d, cur_vproc_mv = %d, delay = %d\n", __func__, cur_vsram_mv, cur_vproc_mv, PMIC_SETTLE_TIME(cur_vproc_mv + MAX_DIFF_VSRAM_VPROC, cur_vproc_mv));
		} while (cur_vproc_mv > mv);
	}

	if (NULL != g_pCpuVoltSampler)
		g_pCpuVoltSampler(MT_CPU_DVFS_BIG, mv);

#if 1
	cpufreq_ver("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n", __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
#else
	{
		unsigned int rdata;

		pwrap_read(PMIC_ADDR_VSRAM_CA15L_VOSEL_ON, &rdata);
		rdata = PMIC_VAL_TO_VOLT(rdata);

		cpufreq_ver("@%s():%d, cur_vsram_mv = %d (%d), cur_vproc_mv = %d (%d)\n", __func__, __LINE__, cur_vsram_mv, rdata, cur_vproc_mv, get_cur_volt_big(p));
	}
#endif
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}

/* cpufreq set (freq & volt) */

static unsigned int _search_available_volt(struct mt_cpu_dvfs *p, unsigned int target_khz)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* search available voltage */
	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (target_khz <= cpu_dvfs_get_freq_by_idx(p, i))
			break;
	}

	BUG_ON(i < 0); /* i.e. target_khz > p->opp_tbl[0].cpufreq_khz */

	FUNC_EXIT(FUNC_LV_HELP);

	return cpu_dvfs_get_volt_by_idx(p, i);
}

static int _cpufreq_set_locked(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz)
{
	// unsigned int cpu;			// XXX: it causes deadlock
	// struct cpufreq_freqs freqs;		// XXX: it causes deadlock
	// struct cpufreq_policy *policy;	// XXX: it causes deadlock

	unsigned int mv;
	int ret = 0;

	enum turbo_mode mode = get_turbo_mode(p, target_khz);

	FUNC_ENTER(FUNC_LV_HELP);

	mv = _search_available_volt(p, target_khz);

	if (cur_khz != TURBO_MODE_FREQ(mode, target_khz)) cpufreq_ver("@%s(), target_khz = %d (%d), mv = %d (%d), num_online_cpus = %d, cur_khz = %d\n", __func__, target_khz, TURBO_MODE_FREQ(mode, target_khz), mv, TURBO_MODE_VOLT(mode, mv), num_online_cpus(), cur_khz);

	mv = TURBO_MODE_VOLT(mode, mv);
	target_khz = TURBO_MODE_FREQ(mode, target_khz);

	if (cur_khz == target_khz)
		goto out;

	{
		/* update for deep idle */
		if (chip_ver != CHIP_SW_VER_02 || cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
#ifdef MTK_FORCE_CLUSTER1
						(chip_ver == CHIP_SW_VER_01) ? IDX_DI_VSRAM_CA15L_NORMAL : IDX_DI_VPROC_CA7_NORMAL,
#else
						IDX_DI_VPROC_CA7_NORMAL,
#endif
						VOLT_TO_PMIC_VAL(mv + (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 0 : NORMAL_DIFF_VRSAM_VPROC))
					       );
		}

		/* update for suspend */
		if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)
		    && pw.set[PMIC_WRAP_PHASE_SUSPEND]._[IDX_SP_VSRAM_CA15L_PWR_ON].cmd_addr == PMIC_ADDR_VSRAM_CA15L_VOSEL_ON
		   )
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_SUSPEND,
						IDX_SP_VSRAM_CA15L_PWR_ON,
						VOLT_TO_PMIC_VAL(mv + NORMAL_DIFF_VRSAM_VPROC)
					       );
	}

	/* set volt (UP) */
	if (cur_khz < target_khz) {
		ret = p->ops->set_cur_volt(p, mv);

		if (ret) /* set volt fail */
			goto out;
	}

	// freqs.old = cur_khz;		// XXX: it causes deadlock
	// freqs.new = target_khz;	// XXX: it causes deadlock

	// policy = cpufreq_cpu_get(p->cpu_id); // TODO: FIXME, for E1			// XXX: it causes deadlock

	// if (policy) {								// XXX: it causes deadlock
	// 	for_each_online_cpu(cpu) { // TODO: big LITTLE issue (id mapping)	// XXX: it causes deadlock
	// 		freqs.cpu = cpu;						// XXX: it causes deadlock
	// 		cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);	// XXX: it causes deadlock
	// 	}									// XXX: it causes deadlock
	// }										// XXX: it causes deadlock

	/* set freq (UP/DOWN) */
	if (cur_khz != target_khz)
		p->ops->set_cur_freq(p, cur_khz, target_khz);

	// if (policy) {								// XXX: it causes deadlock
	// 	for_each_online_cpu(cpu) { // TODO: big LITTLE issue (id mapping)	// XXX: it causes deadlock
	// 		freqs.cpu = cpu;						// XXX: it causes deadlock
	// 		cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);	// XXX: it causes deadlock
	// 	}									// XXX: it causes deadlock
	//										// XXX: it causes deadlock
	// 	cpufreq_cpu_put(policy);						// XXX: it causes deadlock
	// }										// XXX: it causes deadlock

	/* set volt (DOWN) */
	if (cur_khz > target_khz) {
		ret = p->ops->set_cur_volt(p, mv);

		if (ret) /* set volt fail */
			goto out;
	}

	FUNC_EXIT(FUNC_LV_HELP);
out:
	return ret;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx);

static void _mt_cpufreq_set(enum mt_cpu_dvfs_id id, int new_opp_idx)
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int cur_freq;
	unsigned int target_freq;
	unsigned int cpu;
	struct cpufreq_freqs freqs;
	struct cpufreq_policy *policy;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);
	// BUG_ON(new_opp_idx >= p->nr_opp_tbl);

	policy = cpufreq_cpu_get(p->cpu_id);

	if (policy) {
		freqs.old = policy->cur;
		freqs.new = mt_cpufreq_max_frequency_by_DVS(id, new_opp_idx);

		for_each_online_cpu(cpu) { // TODO: big LITTLE issue (id mapping)
			freqs.cpu = cpu;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
		}
	}

	cpufreq_lock(flags);	// <-XXX

	if (new_opp_idx == -1) new_opp_idx = p->idx_opp_tbl;

#if defined(CONFIG_CPU_DVFS_BRINGUP)
	new_opp_idx = id_to_cpu_dvfs(id)->idx_normal_max_opp;
#elif defined(CONFIG_CPU_DVFS_RANDOM_TEST)
	new_opp_idx = jiffies & 0x7; /* 0~7 */
#else
	new_opp_idx = _calc_new_opp_idx(id_to_cpu_dvfs(id), new_opp_idx);
#endif

	cur_freq = p->ops->get_cur_phy_freq(p);
	target_freq = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);

	/* enable FBB for CA15L before entering into first OPP */
	if (id == MT_CPU_DVFS_BIG && 0 != p->idx_opp_tbl && 0 == new_opp_idx)
		turn_on_FBB();

	/* disable SPARK for CA15L before leaving first OPP */
	if (id == MT_CPU_DVFS_BIG && 0 == p->idx_opp_tbl && 0 != new_opp_idx)
		turn_off_SPARK();

	_cpufreq_set_locked(p, cur_freq, target_freq);

	/* disable FBB for CA15L after leaving first OPP */
	if (id == MT_CPU_DVFS_BIG && 0 == p->idx_opp_tbl && 0 != new_opp_idx)
		turn_off_FBB();

	/* enable SPARK for CA15L at first OPP and voltage no less than 1000mv */
	if (id == MT_CPU_DVFS_BIG && 0 != p->idx_opp_tbl && 0 == new_opp_idx && p->ops->get_cur_volt(p) >= 1000)
		turn_on_SPARK();

	p->idx_opp_tbl = new_opp_idx;

	cpufreq_unlock(flags);	// <-XXX

	if (policy) {
		freqs.new = p->ops->get_cur_phy_freq(p);
		for_each_online_cpu(cpu) { // TODO: big LITTLE issue (id mapping)
			freqs.cpu = cpu;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
		}
		cpufreq_cpu_put(policy);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static enum mt_cpu_dvfs_id _get_cpu_dvfs_id(unsigned int cpu_id);

static int __cpuinit turbo_mode_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
#if 1
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu)); // TODO: FIXME, for E1
	int num_online_cpus_little, num_online_cpus_big;

	if (-1 == hps_get_num_online_cpus(&num_online_cpus_little, &num_online_cpus_big)) {
		num_online_cpus_little = 4;
		num_online_cpus_big = 4;
	}

	cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d, num_online_cpus = %d, %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus_little, num_online_cpus_big); // <-XXX

	dev = get_cpu_device(cpu);

	if (dev) {
		if ((cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) && (OPP7_BOUNDARY_CPU_NUM == num_online_cpus_little)) {
			switch (action & 0xF) {
			case CPU_UP_PREPARE: //1->2
				num_online_cpus_little_delta_for_403 = 1;
			case CPU_DEAD: //2->1
				_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, -1);
				cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d, num_online_cpus = %d, %d, %d, %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus_little, num_online_cpus_big, num_online_cpus_little_delta, num_online_cpus_big_delta); // <-XXX
				break;
			}
		}
		else
		{
			switch (action & 0xF) {
			case CPU_ONLINE:
			case CPU_UP_CANCELED:
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) 
					num_online_cpus_little_delta_for_403 = 0;
	
				cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d, num_online_cpus = %d, %d, %d, %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus_little, num_online_cpus_big, num_online_cpus_little_delta, num_online_cpus_big_delta); // <-XXX
				break;
			}
		}
		
		if (TURBO_MODE_BOUNDARY_CPU_NUM == (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? num_online_cpus_little : num_online_cpus_big)) {
			switch (action & 0xF) {
			case CPU_UP_PREPARE:
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) num_online_cpus_little_delta = 1;
				else if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)) num_online_cpus_big_delta = 1;

			case CPU_DEAD:
				_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, -1);
				cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d, num_online_cpus = %d, %d, %d, %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus_little, num_online_cpus_big, num_online_cpus_little_delta, num_online_cpus_big_delta); // <-XXX
				break;
			}
		} else {
			switch (action & 0xF) {
			case CPU_ONLINE:
			case CPU_UP_CANCELED:
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) 
				{
					num_online_cpus_little_delta = 0;
					num_online_cpus_little_delta_for_403 = 0;
				}
				else if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)) num_online_cpus_big_delta = 0;

				cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d, num_online_cpus = %d, %d, %d, %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus_little, num_online_cpus_big, num_online_cpus_little_delta, num_online_cpus_big_delta); // <-XXX
				break;
			}
		}
	}

#else	// XXX: DON'T USE cpufreq_driver_target() for the case which cur_freq == target_freq
	struct cpufreq_policy *policy;
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu)); // TODO: FIXME, for E1

	cpufreq_ver("@%s():%d, cpu = %d, action = %d, oppidx = %d, num_online_cpus = %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus()); // <-XXX

	dev = get_cpu_device(cpu);

	if (dev
	    && 0 == p->idx_opp_tbl
	    && TURBO_MODE_BOUNDARY_CPU_NUM == num_online_cpus()
	   ) {
		switch (action) {
		case CPU_UP_PREPARE:
		case CPU_DEAD:

			policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				cpufreq_driver_target(policy, cpu_dvfs_get_cur_freq(p), CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy);
			}

			cpufreq_ver("@%s():%d, cpu = %d, action = %d, oppidx = %d, num_online_cpus = %d\n", __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus()); // <-XXX
			break;
		}
	}

#endif
	return NOTIFY_OK;
}

static struct notifier_block __refdata turbo_mode_cpu_notifier = {
	.notifier_call = turbo_mode_cpu_callback,
};

static void _set_no_limited(struct mt_cpu_dvfs *p)
{
	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	p->limited_max_freq = cpu_dvfs_get_max_freq(p);
	p->limited_max_ncpu = MAX_CPU_NUM;
	p->thermal_protect_limited_power = 0;

	FUNC_EXIT(FUNC_LV_HELP);
}

static void _downgrade_freq_check(enum mt_cpu_dvfs_id id)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int temp = 0;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	/* if not CPU_LEVEL0 */
	if (cpu_dvfs_get_max_freq(p) < ((MT_CPU_DVFS_LITTLE == id) ? DVFS_LITTLE_F0 : DVFS_BIG_F0)) // TODO: FIXME, the comparison is always fail
		goto out;

	/* get temp */
#if 0 // TODO: FIXME

	if (mt_ptp_status((MT_CPU_DVFS_LITTLE == id) ? PTP_DET_LITTLE : PTP_DET_BIG) == 1)
		temp = (((DRV_Reg32(PTP_TEMP) & 0xff)) + 25) * 1000; // TODO: mt_ptp.c provide mt_ptp_get_temp()
	else
		temp = mtktscpu_get_Tj_temp(); // TODO: FIXME, what is the difference for big & LITTLE

#else
	temp = tscpu_get_temp_by_bank((MT_CPU_DVFS_LITTLE == id) ? THERMAL_BANK0 : THERMAL_BANK1); // TODO: mt_ptp.c provide mt_ptp_get_temp()
#endif

	if (temp < 0 || 125000 < temp) {
		// cpufreq_dbg("%d (temp) < 0 || 12500 < %d (temp)\n", temp, temp);
		goto out;
	}

	{
		static enum turbo_mode pre_mode = TURBO_MODE_NONE;
		enum turbo_mode cur_mode = get_turbo_mode(p, cpu_dvfs_get_cur_freq(p));

		if (pre_mode != cur_mode) {
			_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, p->idx_opp_tbl);
			cpufreq_ver("@%s():%d, oppidx = %d, num_online_cpus = %d, pre_mode = %d, cur_mode = %d\n", __func__, __LINE__, p->idx_opp_tbl, num_online_cpus(), pre_mode, cur_mode); // <-XXX
			pre_mode = cur_mode;
		}
	}

	if (temp <= p->ptpod_temperature_limit_1) {
		p->downgrade_freq_for_ptpod  = false;
		// cpufreq_dbg("%d (temp) < %d (limit_1)\n", temp, p->ptpod_temperature_limit_1);
		goto out;
	} else if ((temp > p->ptpod_temperature_limit_1) && (temp < p->ptpod_temperature_limit_2)) {
		p->downgrade_freq_counter_return_limit = p->downgrade_freq_counter_limit * p->ptpod_temperature_time_1;
		// cpufreq_dbg("%d (temp) > %d (limit_1)\n", temp, p->ptpod_temperature_limit_1);
	} else {
		p->downgrade_freq_counter_return_limit = p->downgrade_freq_counter_limit * p->ptpod_temperature_time_2;
		// cpufreq_dbg("%d (temp) > %d (limit_2)\n", temp, p->ptpod_temperature_limit_2);
	}

	if (p->downgrade_freq_for_ptpod == false) {
		if ((num_online_cpus() == p->pre_online_cpu) && (cpu_dvfs_get_cur_freq(p) == p->pre_freq)) {
			if ((num_online_cpus() >= p->over_max_cpu) && (p->idx_opp_tbl == 0)) {
				p->downgrade_freq_counter++;
				// cpufreq_dbg("downgrade_freq_counter_limit = %d\n", p->downgrade_freq_counter_limit);
				// cpufreq_dbg("downgrade_freq_counter = %d\n", p->downgrade_freq_counter);

				if (p->downgrade_freq_counter >= p->downgrade_freq_counter_limit) {
					p->downgrade_freq = cpu_dvfs_get_freq_by_idx(p, 1);

					p->downgrade_freq_for_ptpod = true;
					p->downgrade_freq_counter = 0;

					cpufreq_dbg("freq limit, downgrade_freq_for_ptpod = %d\n", p->downgrade_freq_for_ptpod);

					policy = cpufreq_cpu_get(p->cpu_id);

					if (!policy)
						goto out;

					cpufreq_driver_target(policy, p->downgrade_freq, CPUFREQ_RELATION_L);

					cpufreq_cpu_put(policy);
				}
			} else
				p->downgrade_freq_counter = 0;
		} else {
			p->pre_online_cpu = num_online_cpus();
			p->pre_freq = cpu_dvfs_get_cur_freq(p);

			p->downgrade_freq_counter = 0;
		}
	} else {
		p->downgrade_freq_counter_return++;

		// cpufreq_dbg("downgrade_freq_counter_return_limit = %d\n", p->downgrade_freq_counter_return_limit);
		// cpufreq_dbg("downgrade_freq_counter_return = %d\n", p->downgrade_freq_counter_return);

		if (p->downgrade_freq_counter_return >= p->downgrade_freq_counter_return_limit) {
			p->downgrade_freq_for_ptpod  = false;
			p->downgrade_freq_counter_return = 0;

			// cpufreq_dbg("Release freq limit, downgrade_freq_for_ptpod = %d\n", p->downgrade_freq_for_ptpod);
		}
	}

out:
	FUNC_EXIT(FUNC_LV_API);
}

static void _init_downgrade(struct mt_cpu_dvfs *p, unsigned int cpu_level)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (cpu_level) {
	case CPU_LEVEL_0:
	case CPU_LEVEL_1:
	case CPU_LEVEL_2:
	default:
		p->downgrade_freq_counter_limit = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 10 : 10;
		p->ptpod_temperature_time_1     = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 2  : 1;
		p->ptpod_temperature_time_2     = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 8  : 4;
		break;
	}

	/* install callback */
	cpufreq_freq_check = _downgrade_freq_check;

	FUNC_EXIT(FUNC_LV_HELP);
}

static int _sync_opp_tbl_idx(struct mt_cpu_dvfs *p)
{
	int ret = -1;
	unsigned int freq;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);
	BUG_ON(NULL == p->opp_tbl);
	BUG_ON(NULL == p->ops);

	freq = p->ops->get_cur_phy_freq(p);

	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (freq <= cpu_dvfs_get_freq_by_idx(p, i)) {
			p->idx_opp_tbl = i;
			break;
		}

	}

	if (i >= 0) {
		cpufreq_dbg("%s freq = %d\n", cpu_dvfs_get_name(p), cpu_dvfs_get_cur_freq(p));

		// TODO: apply correct voltage???

		ret = 0;
	} else
		cpufreq_warn("%s can't find freq = %d\n", cpu_dvfs_get_name(p), freq);

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

static void _mt_cpufreq_sync_opp_tbl_idx(void)
{
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_cpu_dvfs(i, p) {
		if (cpu_dvfs_is_availiable(p))
			_sync_opp_tbl_idx(p);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static struct cpumask cpumask_big;
static struct cpumask cpumask_little;

static enum mt_cpu_dvfs_id _get_cpu_dvfs_id(unsigned int cpu_id)
{
#if 1	// TODO: FIXME, just for E1
	return cpumask_test_cpu(cpu_id, &cpumask_little) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG;
#else	// TODO: FIXME, just for E1
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		if (p->cpu_id == cpu_id)
			break;
	}

	BUG_ON(i >= NR_MT_CPU_DVFS);

	return i;
#endif	// TODO: FIXME, just for E1
}

int mt_cpufreq_state_set(int enabled) // TODO: state set by id??? keep this function???
{
	bool set_normal_max_opp = false;
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_API);
#if 0
	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		cpufreq_lock(flags);

		if (enabled) {
			/* enable CPU DVFS */
			if (p->cpufreq_pause) {
				p->dvfs_disable_count--;
				cpufreq_dbg("enable %s DVFS: dvfs_disable_count = %d\n", p->name, p->dvfs_disable_count);

				if (p->dvfs_disable_count <= 0)
					p->cpufreq_pause = false;
				else
					cpufreq_dbg("someone still disable %s DVFS and cant't enable it\n", p->name);
			} else
				cpufreq_dbg("%s DVFS already enabled\n", p->name);
		} else {
			/* disable DVFS */
			p->dvfs_disable_count++;

			if (p->cpufreq_pause)
				cpufreq_dbg("%s DVFS already disabled\n", p->name);
			else {
				p->cpufreq_pause = true;
				set_normal_max_opp = true;
			}
		}

		cpufreq_unlock(flags);

		if (set_normal_max_opp) {
			struct cpufreq_policy *policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p), CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy);
			} else {
				cpufreq_warn("can't get cpufreq policy to disable %s DVFS\n", p->name);
				ret = -1;
			}
		}

		set_normal_max_opp = false;
	}
#endif
	FUNC_EXIT(FUNC_LV_API);

	return ret;
}
EXPORT_SYMBOL(mt_cpufreq_state_set);

/* Power Table */
#if 0
#define P_MCU_L         (1243)  /* MCU Leakage Power          */
#define P_MCU_T         (2900)  /* MCU Total Power            */
#define P_CA7_L         (110)   /* CA7 Leakage Power          */
#define P_CA7_T         (305)   /* Single CA7 Core Power      */

#define P_MCL99_105C_L  (1243)  /* MCL99 Leakage Power @ 105C */
#define P_MCL99_25C_L   (93)    /* MCL99 Leakage Power @ 25C  */
#define P_MCL50_105C_L  (587)   /* MCL50 Leakage Power @ 105C */
#define P_MCL50_25C_L   (35)    /* MCL50 Leakage Power @ 25C  */

#define T_105           (105)   /* Temperature 105C           */
#define T_65            (65)    /* Temperature 65C            */
#define T_25            (25)    /* Temperature 25C            */

#define P_MCU_D ((P_MCU_T - P_MCU_L) - 8 * (P_CA7_T - P_CA7_L)) /* MCU dynamic power except of CA7 cores */

#define P_TOTAL_CORE_L ((P_MCL99_105C_L  * 27049) / 100000) /* Total leakage at T_65 */
#define P_EACH_CORE_L  ((P_TOTAL_CORE_L * ((P_CA7_L * 1000) / P_MCU_L)) / 1000) /* 1 core leakage at T_65 */

#define P_CA7_D_1_CORE ((P_CA7_T - P_CA7_L) * 1) /* CA7 dynamic power for 1 cores turned on */
#define P_CA7_D_2_CORE ((P_CA7_T - P_CA7_L) * 2) /* CA7 dynamic power for 2 cores turned on */
#define P_CA7_D_3_CORE ((P_CA7_T - P_CA7_L) * 3) /* CA7 dynamic power for 3 cores turned on */
#define P_CA7_D_4_CORE ((P_CA7_T - P_CA7_L) * 4) /* CA7 dynamic power for 4 cores turned on */

#define A_1_CORE (P_MCU_D + P_CA7_D_1_CORE) /* MCU dynamic power for 1 cores turned on */
#define A_2_CORE (P_MCU_D + P_CA7_D_2_CORE) /* MCU dynamic power for 2 cores turned on */
#define A_3_CORE (P_MCU_D + P_CA7_D_3_CORE) /* MCU dynamic power for 3 cores turned on */
#define A_4_CORE (P_MCU_D + P_CA7_D_4_CORE) /* MCU dynamic power for 4 cores turned on */

static void _power_calculation(struct mt_cpu_dvfs *p, int idx, int ncpu)
{
	int multi = 0, p_dynamic = 0, p_leakage = 0, freq_ratio = 0, volt_square_ratio = 0;
	int possible_cpu = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	possible_cpu = num_possible_cpus(); // TODO: FIXME

	volt_square_ratio = (((p->opp_tbl[idx].cpufreq_volt * 100) / 1000) *
			     ((p->opp_tbl[idx].cpufreq_volt * 100) / 1000)) / 100;
	freq_ratio = (p->opp_tbl[idx].cpufreq_khz / 1700);

	cpufreq_dbg("freq_ratio = %d, volt_square_ratio %d\n", freq_ratio, volt_square_ratio);

	multi = ((p->opp_tbl[idx].cpufreq_volt * 100) / 1000) *
		((p->opp_tbl[idx].cpufreq_volt * 100) / 1000) *
		((p->opp_tbl[idx].cpufreq_volt * 100) / 1000);

	switch (ncpu) {
	case 0:
		/* 1 core */
		p_dynamic = (((A_1_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 7 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	case 1:
		/* 2 core */
		p_dynamic = (((A_2_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 6 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	case 2:
		/* 3 core */
		p_dynamic = (((A_3_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 5 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	case 3:
		/* 4 core */
		p_dynamic = (((A_4_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 4 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	default:
		break;
	}

	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_ncpu    = ncpu + 1;
	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_khz     = p->opp_tbl[idx].cpufreq_khz;
	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_power   = p_dynamic + p_leakage;

	cpufreq_dbg("p->power_tbl[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		    (idx * possible_cpu + ncpu),
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_ncpu,
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_khz,
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_power
		   );

	FUNC_EXIT(FUNC_LV_HELP);
}

static int setup_power_table(struct mt_cpu_dvfs *p)
{
	static const unsigned int pwr_tbl_cgf[] = { 0, 0, 1, 0, 1, 0, 1, 0, };
	unsigned int pwr_eff_tbl[NR_MAX_OPP_TBL][NR_MAX_CPU];
	unsigned int pwr_eff_num;
	int possible_cpu;
	int i, j;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	if (p->power_tbl)
		goto out;

	cpufreq_dbg("P_MCU_D = %d\n", P_MCU_D);
	cpufreq_dbg("P_CA7_D_1_CORE = %d, P_CA7_D_2_CORE = %d, P_CA7_D_3_CORE = %d, P_CA7_D_4_CORE = %d\n", P_CA7_D_1_CORE, P_CA7_D_2_CORE, P_CA7_D_3_CORE, P_CA7_D_4_CORE);
	cpufreq_dbg("P_TOTAL_CORE_L = %d, P_EACH_CORE_L = %d\n", P_TOTAL_CORE_L, P_EACH_CORE_L);
	cpufreq_dbg("A_1_CORE = %d, A_2_CORE = %d, A_3_CORE = %d, A_4_CORE = %d\n", A_1_CORE, A_2_CORE, A_3_CORE, A_4_CORE);

	possible_cpu = num_possible_cpus(); // TODO: FIXME

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl = kzalloc(p->nr_opp_tbl * possible_cpu * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

	if (NULL == p->power_tbl) {
		ret = -ENOMEM;
		goto out;
	}

	/* setup power efficiency array */
	for (i = 0, pwr_eff_num = 0; i < possible_cpu; i++) {
		if (1 == pwr_tbl_cgf[i])
			pwr_eff_num++;
	}

	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (1 == pwr_tbl_cgf[j])
				pwr_eff_tbl[i][j] = 1;
		}
	}

	p->nr_power_tbl = p->nr_opp_tbl * (possible_cpu - pwr_eff_num);

	/* calc power and fill in power table */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (0 == pwr_eff_tbl[i][j])
				_power_calculation(p, i, j);
		}
	}

	/* sort power table */
	for (i = p->nr_opp_tbl * possible_cpu; i > 0; i--) {
		for (j = 1; j <= i; j++) {
			if (p->power_tbl[j - 1].cpufreq_power < p->power_tbl[j].cpufreq_power) {
				struct mt_cpu_power_info tmp;

				tmp.cpufreq_khz                 = p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu                = p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power               = p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz   = p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu  = p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power = p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz     = tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu    = tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power   = tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		cpufreq_dbg("[%d] = { .khz = %d, .ncup = %d, .power = %d }\n",
			    p->power_tbl[i].cpufreq_khz,
			    p->power_tbl[i].cpufreq_ncpu,
			    p->power_tbl[i].cpufreq_power
			   );
	}

out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}
#else
static void _power_calculation(struct mt_cpu_dvfs *p, int oppidx, int ncpu)
{
#define CA7_REF_POWER	715	/* mW  */
#define CA7_REF_FREQ	1696000 /* KHz */
#define CA7_REF_VOLT	1000	/* mV  */
#define CA15L_REF_POWER	3910	/* mW  */
#define CA15L_REF_FREQ	2093000 /* KHz */
#define CA15L_REF_VOLT	1020	/* mV  */

	int p_dynamic = 0, ref_freq, ref_volt;
	int possible_cpu = num_possible_cpus(); // TODO: FIXME

	FUNC_ENTER(FUNC_LV_HELP);

	p_dynamic = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_POWER : CA15L_REF_POWER;
	ref_freq  = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_FREQ  : CA15L_REF_FREQ;
	ref_volt  = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_VOLT  : CA15L_REF_VOLT;

	p_dynamic = p_dynamic *
		    (p->opp_tbl[oppidx].cpufreq_khz / 1000) / (ref_freq / 1000) *
		    p->opp_tbl[oppidx].cpufreq_volt / ref_volt *
		    p->opp_tbl[oppidx].cpufreq_volt / ref_volt +
		    mt_spower_get_leakage(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_SPOWER_CA7 : MT_SPOWER_CA17, p->opp_tbl[oppidx].cpufreq_volt, 85);

	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_ncpu  = ncpu + 1;
	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_khz   = p->opp_tbl[oppidx].cpufreq_khz;
	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_power = p_dynamic * (ncpu + 1) / possible_cpu;

	FUNC_EXIT(FUNC_LV_HELP);
}

static int setup_power_table(struct mt_cpu_dvfs *p)
{
	static const unsigned int pwr_tbl_cgf[NR_MAX_CPU] = { 0, 0, 0, 0, };
	unsigned int pwr_eff_tbl[NR_MAX_OPP_TBL][NR_MAX_CPU];
	unsigned int pwr_eff_num;
	int possible_cpu = num_possible_cpus(); // TODO: FIXME
	int i, j;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	if (p->power_tbl)
		goto out;

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl = kzalloc(p->nr_opp_tbl * possible_cpu * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

	if (NULL == p->power_tbl) {
		ret = -ENOMEM;
		goto out;
	}

	/* setup power efficiency array */
	for (i = 0, pwr_eff_num = 0; i < possible_cpu; i++) {
		if (1 == pwr_tbl_cgf[i])
			pwr_eff_num++;
	}

	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (1 == pwr_tbl_cgf[j])
				pwr_eff_tbl[i][j] = 1;
		}
	}

	p->nr_power_tbl = p->nr_opp_tbl * (possible_cpu - pwr_eff_num);

	/* calc power and fill in power table */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (0 == pwr_eff_tbl[i][j])
				_power_calculation(p, i, j);
		}
	}

	/* sort power table */
	for (i = p->nr_opp_tbl * possible_cpu; i > 0; i--) {
		for (j = 1; j <= i; j++) {
			if (p->power_tbl[j - 1].cpufreq_power < p->power_tbl[j].cpufreq_power) {
				struct mt_cpu_power_info tmp;

				tmp.cpufreq_khz                 = p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu                = p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power               = p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz   = p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu  = p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power = p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz     = tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu    = tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power   = tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		cpufreq_dbg("[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d }\n",
			    i,
			    p->power_tbl[i].cpufreq_khz,
			    p->power_tbl[i].cpufreq_ncpu,
			    p->power_tbl[i].cpufreq_power
			   );
	}

#if 0 // def CONFIG_THERMAL // TODO: FIXME
	mtk_cpufreq_register(p->power_tbl, p->nr_power_tbl);
#endif

out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}
#endif

static void _mt_cpufreq_tlp_power_init(struct mt_cpu_dvfs *p)
{
#define CA7_REF_POWER	715	/* mW  */
#define CA7_REF_FREQ	1696000 /* KHz */
#define CA7_REF_VOLT	1000	/* mV  */
#define CA15L_REF_POWER	3910	/* mW  */
#define CA15L_REF_FREQ	2093000 /* KHz */
#define CA15L_REF_VOLT	1020	/* mV  */

	static bool leakage_inited_little;
	static bool leakage_inited_big;
	unsigned int sindex;

	int p_dynamic = 0, ref_freq, ref_volt;
	int possible_cpu_little, possible_cpu_big;

	int i, j, k;

	if (cpu_tlp_power_tbl == NULL) {
		cpu_tlp_power_tbl = get_cpu_tlp_power_tbl();
		leakage_inited_little = false;
		leakage_inited_big = false;
	}

	if (-1 == hps_get_num_possible_cpus(&possible_cpu_little, &possible_cpu_big)) {
		possible_cpu_little = 4;
		possible_cpu_big = 4;
	}

	ref_freq  = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_FREQ  : CA15L_REF_FREQ;
	ref_volt  = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_VOLT  : CA15L_REF_VOLT;

	/* add static power */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < cpu_tlp_power_tbl[i].nr_power_table; j++) {

			if (leakage_inited_little == false && leakage_inited_big == false)
				cpu_tlp_power_tbl[i].power_tbl[j].power = 0;

			if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
				if (cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little > 0 && false == leakage_inited_little) {
					sindex = cpu_tlp_power_tbl[i].power_tbl[j].khz_little - 1;

					p_dynamic = CA7_REF_POWER *
						    (p->opp_tbl[sindex].cpufreq_khz / 1000) / (ref_freq / 1000) *
						    p->opp_tbl[sindex].cpufreq_volt / ref_volt *
						    p->opp_tbl[sindex].cpufreq_volt / ref_volt;

					cpu_tlp_power_tbl[i].power_tbl[j].power += (p_dynamic + mt_spower_get_leakage(MT_SPOWER_CA7, p->opp_tbl[sindex].cpufreq_volt, 85)) * cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little / possible_cpu_little;
				}
			} else {
				if (cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big > 0 && false == leakage_inited_big) {
					sindex = cpu_tlp_power_tbl[i].power_tbl[j].khz_big - 1;

					p_dynamic = CA15L_REF_POWER *
						    (p->opp_tbl[sindex].cpufreq_khz / 1000) / (ref_freq / 1000) *
						    p->opp_tbl[sindex].cpufreq_volt / ref_volt *
						    p->opp_tbl[sindex].cpufreq_volt / ref_volt;

					cpu_tlp_power_tbl[i].power_tbl[j].power += (p_dynamic + mt_spower_get_leakage(MT_SPOWER_CA17, p->opp_tbl[sindex].cpufreq_volt, 85)) * cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big / possible_cpu_big;
				}
			}
		}
	}

	if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
		leakage_inited_little = true;
	else
		leakage_inited_big = true;

	/* sort power table */
	for (i = 0; i < 8; i++) {
		for (j = (cpu_tlp_power_tbl[i].nr_power_table - 1); j > 0; j--) {
			for (k = 1; k <= j; k++) {
				if (cpu_tlp_power_tbl[i].power_tbl[k - 1].power < cpu_tlp_power_tbl[i].power_tbl[k].power) {
					struct mt_cpu_power_tbl tmp;

					tmp.ncpu_big    = cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_big;
					tmp.khz_big     = cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_big;
					tmp.ncpu_little = cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_little;
					tmp.khz_little  = cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_little;
					tmp.performance = cpu_tlp_power_tbl[i].power_tbl[k - 1].performance;
					tmp.power       = cpu_tlp_power_tbl[i].power_tbl[k - 1].power;

					cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_big    = cpu_tlp_power_tbl[i].power_tbl[k].ncpu_big;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_big     = cpu_tlp_power_tbl[i].power_tbl[k].khz_big;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_little = cpu_tlp_power_tbl[i].power_tbl[k].ncpu_little;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_little  = cpu_tlp_power_tbl[i].power_tbl[k].khz_little;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].performance = cpu_tlp_power_tbl[i].power_tbl[k].performance;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].power       = cpu_tlp_power_tbl[i].power_tbl[k].power;

					cpu_tlp_power_tbl[i].power_tbl[k].ncpu_big    = tmp.ncpu_big;
					cpu_tlp_power_tbl[i].power_tbl[k].khz_big     = tmp.khz_big;
					cpu_tlp_power_tbl[i].power_tbl[k].ncpu_little = tmp.ncpu_little;
					cpu_tlp_power_tbl[i].power_tbl[k].khz_little  = tmp.khz_little;
					cpu_tlp_power_tbl[i].power_tbl[k].performance = tmp.performance;
					cpu_tlp_power_tbl[i].power_tbl[k].power       = tmp.power;
				}
			}
		}
	}

	/* dump power table */
	for (i = 0; i < 8; i++) {
		cpufreq_dbg("TLP = %d\n", i + 1);

		for (j = 0; j < cpu_tlp_power_tbl[i].nr_power_table; j++) {
			cpufreq_dbg("%u, %u, %u, %u, %u, %u\n",
				    cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big,
				    cpu_tlp_power_tbl[i].power_tbl[j].khz_big,
				    cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little,
				    cpu_tlp_power_tbl[i].power_tbl[j].khz_little,
				    cpu_tlp_power_tbl[i].power_tbl[j].performance,
				    cpu_tlp_power_tbl[i].power_tbl[j].power);
		}

		cpufreq_dbg("\n\n");
	}
}

static int _mt_cpufreq_setup_freqs_table(struct cpufreq_policy *policy, struct mt_cpu_freq_info *freqs, int num)
{
	struct mt_cpu_dvfs *p;
	struct cpufreq_frequency_table *table;
	int i, ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == policy);
	BUG_ON(NULL == freqs);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	if (NULL == p->freq_tbl_for_cpufreq) {
		table = kzalloc((num + 1) * sizeof(*table), GFP_KERNEL);

		if (NULL == table) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < num; i++) {
			table[i].index = i;
			table[i].frequency = freqs[i].cpufreq_khz;
		}

		table[num].index = i; // TODO: FIXME, why need this???
		table[num].frequency = CPUFREQ_TABLE_END;

		p->opp_tbl = freqs;
		p->nr_opp_tbl = num;
		p->freq_tbl_for_cpufreq = table;
	}

	ret = cpufreq_frequency_table_cpuinfo(policy, p->freq_tbl_for_cpufreq);

	if (!ret)
		cpufreq_frequency_table_get_attr(p->freq_tbl_for_cpufreq, policy->cpu);

	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
	cpumask_copy(policy->related_cpus, policy->cpus);

	if (chip_ver == CHIP_SW_VER_01) {
		if (NULL == p->power_tbl)
			p->ops->setup_power_table(p);
	} else
		_mt_cpufreq_tlp_power_init(p);

out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return 0;
}

void mt_cpufreq_enable_by_ptpod(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	p->dvfs_disable_by_ptpod = false;

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return;
	}

	_mt_cpufreq_set(id, p->idx_opp_tbl_for_late_resume);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_enable_by_ptpod);

unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	p->dvfs_disable_by_ptpod = true;

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return;
	}

#if 0 // XXX: BUG_ON(irqs_disabled()) @ __cpufreq_notify_transition()
	{
		struct cpufreq_policy *policy;
		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p), CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		} else
			cpufreq_warn("can't get cpufreq policy to disable %s DVFS\n", p->name);
	}
#else
	p->idx_opp_tbl_for_late_resume = p->idx_opp_tbl;
	_mt_cpufreq_set(id, p->idx_normal_max_opp); // XXX: useless, decided @ _calc_new_opp_idx()
#endif

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_cur_freq(p);
}
EXPORT_SYMBOL(mt_cpufreq_disable_by_ptpod);

int mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	struct mt_cpu_dvfs *p;
	int possible_cpu;
	int possible_cpu_big;
	int ncpu;
	int found = 0;
	unsigned long flag;
	unsigned int max_ncpu_big, max_khz_big, max_ncpu_little, max_khz_little;
	int i, j, k, tlp;

	FUNC_ENTER(FUNC_LV_API);

	if (chip_ver == CHIP_SW_VER_02 && NULL == cpu_tlp_power_tbl) {
		FUNC_EXIT(FUNC_LV_API);
		return -1;
	}

	cpufreq_dbg("%s(): limited_power = %d\n", __func__, limited_power);

	cpufreq_lock(flag);                                     /* <- lock */

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		p->thermal_protect_limited_power = limited_power;
	}

	if (-1 == hps_get_num_possible_cpus(&possible_cpu, &possible_cpu_big)) {
		possible_cpu = 4;
		possible_cpu_big = 4;
	}

	/* no limited */
	if (0 == limited_power) {
		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_availiable(p))
				continue;

			p->limited_max_ncpu = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? possible_cpu : possible_cpu_big;
			p->limited_max_freq = cpu_dvfs_get_max_freq(p);
		}
		/* limited */
	} else {
		if (chip_ver == CHIP_SW_VER_01) {
			p = id_to_cpu_dvfs(_get_cpu_dvfs_id(0));

			if (cpu_dvfs_is_availiable(p)) {
				for (ncpu = possible_cpu; ncpu > 0; ncpu--) {
					for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
						if (p->power_tbl[i].cpufreq_power <= limited_power) { // p->power_tbl[i].cpufreq_ncpu == ncpu &&
							p->limited_max_ncpu = p->power_tbl[i].cpufreq_ncpu;
							p->limited_max_freq = p->power_tbl[i].cpufreq_khz;
							found = 1;
							ncpu = 0; /* for break outer loop */
							break;
						}
					}
				}

				/* not found and use lowest power limit */
				if (!found) {
					p->limited_max_ncpu = p->power_tbl[p->nr_power_tbl - 1].cpufreq_ncpu;
					p->limited_max_freq = p->power_tbl[p->nr_power_tbl - 1].cpufreq_khz;
				}
			}
		} else {
			/* search index that power is less than or equal to limited power */
			tlp = 7; // TODO: FIXME, int hps_get_tlp(unsigned int * tlp_ptr), fix TLP = 8

			for (i = 0; i < cpu_tlp_power_tbl[tlp].nr_power_table; i++) {
				if (cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_little == 0)
					continue;

				if (cpu_tlp_power_tbl[tlp].power_tbl[i].power <= limited_power)
					break;
			}

#if 0

			if (i < cpu_tlp_power_tbl[tlp].nr_power_table) {
				max_ncpu_big = 0;
				max_khz_big = 0;
				max_ncpu_little = 0;
				max_khz_little = 0;

				for (j = i; j < cpu_tlp_power_tbl[tlp].nr_power_table; j++) {
					if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little == 0)
						continue;

					if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little > max_ncpu_little)
						max_ncpu_little = cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little;

					if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_big > max_ncpu_big)
						max_ncpu_big = cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_big;

					for_each_cpu_dvfs(k, p) {
						if (!cpu_dvfs_is_availiable(p))
							continue;

						if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
							if (cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_little - 1) > max_khz_little)
								max_khz_little = cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_little - 1);
						} else {
							if (cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_big - 1) > max_khz_big)
								max_khz_big = cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_big - 1);
						}
					}
				}

				for_each_cpu_dvfs(i, p) {
					if (!cpu_dvfs_is_availiable(p))
						continue;

					if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
						p->limited_max_ncpu = max_ncpu_little;
						p->limited_max_freq = max_khz_little;
					} else {
						p->limited_max_ncpu = max_ncpu_big;
						p->limited_max_freq = max_khz_big;
					}
				}
			} else {
				/* not found and use lowest power limit */
				for_each_cpu_dvfs(i, p) {
					if (!cpu_dvfs_is_availiable(p))
						continue;

					if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
						p->limited_max_ncpu = cpu_tlp_power_tbl[tlp].power_tbl[cpu_tlp_power_tbl[tlp].nr_power_table - 2].ncpu_little;
						p->limited_max_freq = cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[cpu_tlp_power_tbl[tlp].nr_power_table - 2].khz_little - 1);
					} else {
						p->limited_max_ncpu = cpu_tlp_power_tbl[tlp].power_tbl[cpu_tlp_power_tbl[tlp].nr_power_table - 2].ncpu_big;
						p->limited_max_freq = cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[cpu_tlp_power_tbl[tlp].nr_power_table - 2].khz_big - 1);
					}
				}
			}

			for_each_cpu_dvfs(i, p) {
				if (!cpu_dvfs_is_availiable(p))
					continue;

				cpufreq_dbg("%s, limited_max_freq = %d, limited_max_ncpu = %d\n", p->name, p->limited_max_freq, p->limited_max_ncpu);
			}
#else

			if (i < cpu_tlp_power_tbl[tlp].nr_power_table) {

				for (j = i; j < cpu_tlp_power_tbl[tlp].nr_power_table; j++) {
					if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little == 0)
						continue;

					if (cpu_tlp_power_tbl[tlp].power_tbl[j].performance > cpu_tlp_power_tbl[tlp].power_tbl[i].performance)
						i = j;
				}

				if (cpu_dvfs_is_availiable((&cpu_dvfs[MT_CPU_DVFS_BIG]))) {
					cpu_dvfs[MT_CPU_DVFS_BIG].limited_max_ncpu	= cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_big;
					cpu_dvfs[MT_CPU_DVFS_BIG].limited_max_freq	= cpu_dvfs_get_freq_by_idx((&cpu_dvfs[MT_CPU_DVFS_BIG]), cpu_tlp_power_tbl[tlp].power_tbl[i].khz_big - 1);
				}

				if (cpu_dvfs_is_availiable((&cpu_dvfs[MT_CPU_DVFS_LITTLE]))) {
					cpu_dvfs[MT_CPU_DVFS_LITTLE].limited_max_ncpu	= cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_little;
					cpu_dvfs[MT_CPU_DVFS_LITTLE].limited_max_freq	= cpu_dvfs_get_freq_by_idx((&cpu_dvfs[MT_CPU_DVFS_LITTLE]), cpu_tlp_power_tbl[tlp].power_tbl[i].khz_little - 1);
				}
			}

#endif
		}
	}

	cpufreq_unlock(flag);                                   /* <- unlock */

#if 1 // TODO: FIXME, apply limit
	{
		struct cpufreq_policy *policy;

		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_availiable(p))
				continue;

#if 0 // defined(CONFIG_CPU_FREQ_GOV_HOTPLUG) // TODO (Chun-Wei) : FIXME
			hp_limited_cpu_num(p->limited_max_ncpu); /* notify hotplug governor */
#else
			hps_set_cpu_num_limit(LIMIT_THERMAL, cpu_dvfs[MT_CPU_DVFS_LITTLE].limited_max_ncpu, cpu_dvfs[MT_CPU_DVFS_BIG].limited_max_ncpu);
#endif
			policy = cpufreq_cpu_get(p->cpu_id); // TODO: FIXME, not always 0 (it is OK for E1 BUT) /* <- policy get */

			if (policy) {
				//cpufreq_driver_target(policy, p->limited_max_freq, CPUFREQ_RELATION_L);
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
					_mt_cpufreq_set(MT_CPU_DVFS_LITTLE, p->idx_opp_tbl);
				else
					_mt_cpufreq_set(MT_CPU_DVFS_BIG, p->idx_opp_tbl);

				cpufreq_cpu_put(policy); /* <- policy put */
			}
		}
	}
#endif

	FUNC_EXIT(FUNC_LV_API);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_thermal_protect);

/* for ramp down */
void mt_cpufreq_set_ramp_down_count_const(enum mt_cpu_dvfs_id id, int count)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	BUG_ON(NULL == p);

	p->ramp_down_count_const = count;
}
EXPORT_SYMBOL(mt_cpufreq_set_ramp_down_count_const);

static int _keep_max_freq(struct mt_cpu_dvfs *p, unsigned int freq_old, unsigned int freq_new) // TODO: inline @ mt_cpufreq_target()
{
	int ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (RAMP_DOWN_TIMES < p->ramp_down_count_const)
		p->ramp_down_count_const--;
	else
		p->ramp_down_count_const = RAMP_DOWN_TIMES;

	if (freq_new < freq_old && p->ramp_down_count < p->ramp_down_count_const) {
		ret = 1;
		p->ramp_down_count++;
	} else
		p->ramp_down_count = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	return ret;
}

static int _search_available_freq_idx(struct mt_cpu_dvfs *p, unsigned int target_khz, unsigned int relation) /* return -1 (not found) */
{
	int new_opp_idx = -1;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	if (CPUFREQ_RELATION_L == relation) {
		for (i = (signed)(p->nr_opp_tbl - 1); i >= 0; i--) {
			if (cpu_dvfs_get_freq_by_idx(p, i) >= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	} else { /* CPUFREQ_RELATION_H */
		for (i = 0; i < (signed)p->nr_opp_tbl; i++) {
			if (cpu_dvfs_get_freq_by_idx(p, i) <= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static int _thermal_limited_verify(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	unsigned int target_khz = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);
	int possible_cpu = 0;
	unsigned int online_cpu = 0;
	int found = 0;
	int i, j, tlp;
	unsigned int max_performance, max_performance_id = 0;
	unsigned int online_cpu_big = 0, online_cpu_little = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	possible_cpu = num_possible_cpus(); // TODO: FIXME
	online_cpu = num_online_cpus(); // TODO: FIXME

#if defined(CONFIG_CPU_FREQ_GOV_HOTPLUG)
	{
		extern int g_cpus_sum_load_current;
		cpufreq_dbg("%s(): begin, idx = %d, online_cpu = %d, xxx = %d\n", __func__, new_opp_idx, online_cpu, p->limited_freq_by_hevc);
	}
#else
	cpufreq_dbg("%s(): begin, idx = %d, online_cpu = %d\n", __func__, new_opp_idx, online_cpu);
#endif

	/* no limited */
	if (0 == p->thermal_protect_limited_power)
		return new_opp_idx;

	if (chip_ver == CHIP_SW_VER_01) {
		for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
			if (p->power_tbl[i].cpufreq_ncpu == p->limited_max_ncpu
			    && p->power_tbl[i].cpufreq_khz  == p->limited_max_freq
			   )
				break;
		}

		cpufreq_dbg("%s(): idx = %d, limited_max_ncpu = %d, limited_max_freq = %d\n", __func__, i, p->limited_max_ncpu, p->limited_max_freq);

		for (; i < p->nr_opp_tbl * possible_cpu; i++) {
			if (p->power_tbl[i].cpufreq_ncpu == online_cpu) {
				if (target_khz >= p->power_tbl[i].cpufreq_khz) {
					found = 1;
					break;
				}
			}
		}

		if (found) {
			target_khz = p->power_tbl[i].cpufreq_khz;
			cpufreq_dbg("%s(): freq found, idx = %d, target_khz = %d, online_cpu = %d\n", __func__, i, target_khz, online_cpu);
		} else {
			target_khz = p->limited_max_freq;
			cpufreq_dbg("%s(): freq not found, set to limited_max_freq = %d\n", __func__, target_khz);
		}
	} else {
		tlp = 7; // TODO: FIXME, int hps_get_tlp(unsigned int * tlp_ptr), fix TLP = 8

		for (i = 0; i < cpu_tlp_power_tbl[tlp].nr_power_table; i++) {
			if (cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_little == 0)
				continue;

			if (cpu_tlp_power_tbl[tlp].power_tbl[i].power <= p->thermal_protect_limited_power)
				break;
		}

		if (hps_get_num_online_cpus(&online_cpu_little, &online_cpu_big) == -1) {
			online_cpu_big = 4; /* TODO (Chun-Wei) : Should be replaced by function call */
			online_cpu_little = 4; /* TODO (Chun-Wei) : Should be replaced by function call */
		}

		max_performance = 0;
		max_performance_id = 0;

		for (j = i; j < cpu_tlp_power_tbl[tlp].nr_power_table; j++) {
			if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_big == online_cpu_big
			    && cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little == online_cpu_little
			   ) {
#if 0

				if (cpu_tlp_power_tbl[tlp].power_tbl[j].performance > max_performance) {
					max_performance = cpu_tlp_power_tbl[tlp].power_tbl[j].performance;
					max_performance_id = j;

					if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
						if (target_khz >= cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_little - 1))
							found = 1;
					} else {
						if (target_khz >= cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_big - 1))
							found = 1;
					}
				}

#else
				found = 1;
				break;
#endif
			}
		}

		if (found) {
			if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
				target_khz = cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_little - 1);
			else
				target_khz = cpu_dvfs_get_freq_by_idx(p, cpu_tlp_power_tbl[tlp].power_tbl[j].khz_big - 1);

			cpufreq_dbg("%s(): freq found, idx = %d, target_khz = %d, online_cpu_little = %d, online_cpu_big = %d\n", __func__, j, target_khz, online_cpu_little, online_cpu_big);
		} else {
			target_khz = p->limited_max_freq;
			cpufreq_dbg("%s(): freq not found, set to limited_max_freq = %d\n", __func__, target_khz);
		}
	}

	i = _search_available_freq_idx(p, target_khz, CPUFREQ_RELATION_H); // TODO: refine this function for idx searching

	FUNC_EXIT(FUNC_LV_HELP);

	return (i > new_opp_idx) ? i : new_opp_idx;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	int idx;
	unsigned int online_cpu_big = 0, online_cpu_little = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* for ramp down */
	if (0) { // (_keep_max_freq(p, cpu_dvfs_get_cur_freq(p), cpu_dvfs_get_freq_by_idx(p, new_opp_idx))) { // TODO: refine ramp down mechanism
		cpufreq_dbg("%s(): ramp down, idx = %d, freq_old = %d, freq_new = %d\n", __func__, new_opp_idx, cpu_dvfs_get_cur_freq(p), cpu_dvfs_get_freq_by_idx(p, new_opp_idx));
		new_opp_idx = p->idx_opp_tbl;
	}

	if (chip_ver == CHIP_SW_VER_02
	    && cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)
	    && hps_get_num_online_cpus(&online_cpu_little, &online_cpu_big) != -1
	    && online_cpu_big > 0
	    && new_opp_idx > 4 // XXX: i.e. DVFS_LITTLE_F4
	   )
		new_opp_idx = 4;

	/* HEVC */
	if (p->limited_freq_by_hevc) {
		idx = _search_available_freq_idx(p, p->limited_freq_by_hevc, CPUFREQ_RELATION_L);

		if (idx != -1 && new_opp_idx > idx) {
			new_opp_idx = idx;
			cpufreq_dbg("%s(): hevc limited freq, idx = %d\n", __func__, new_opp_idx);
		}
	}

#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)

	if (true == p->downgrade_freq_for_ptpod) {
		if (cpu_dvfs_get_freq_by_idx(p, new_opp_idx) > p->downgrade_freq) {
			idx = _search_available_freq_idx(p, p->downgrade_freq, CPUFREQ_RELATION_H);

			if (idx != -1) {
				new_opp_idx = idx;
				cpufreq_dbg("%s(): downgrade freq, idx = %d\n", __func__, new_opp_idx);
			}
		}
	}

#endif /* CONFIG_CPU_DVFS_DOWNGRADE_FREQ */

	/* search thermal limited freq */
	idx = _thermal_limited_verify(p, new_opp_idx);

	if (idx != -1) {
		new_opp_idx = idx;
		cpufreq_dbg("%s(): thermal limited freq, idx = %d\n", __func__, new_opp_idx);
	}

	/* for ptpod init */
	if (p->dvfs_disable_by_ptpod) {
		new_opp_idx = 0; // cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 0 : p->idx_normal_max_opp;
		cpufreq_dbg("%s(): for ptpod init, idx = %d\n", __func__, new_opp_idx);
	}

	/* for early suspend */
	if (p->limit_max_freq_early_suspend) {
		new_opp_idx = p->idx_normal_max_opp; // (new_opp_idx < p->idx_normal_max_opp) ? p->idx_normal_max_opp : new_opp_idx;
		cpufreq_dbg("%s(): for early suspend, idx = %d\n", __func__, new_opp_idx);
	}

	/* for suspend */
	if (p->cpufreq_pause)
		new_opp_idx = p->idx_normal_max_opp;

	/* for power throttling */
	if (p->pwr_thro_mode & (PWR_THRO_MODE_BAT_OC_806MHZ)) {
		if (new_opp_idx < CPU_DVFS_OPPIDX_806MHZ)
			cpufreq_dbg("%s(): for power throttling = %d\n", __func__, CPU_DVFS_OPPIDX_806MHZ);

		new_opp_idx = (new_opp_idx < CPU_DVFS_OPPIDX_806MHZ) ? CPU_DVFS_OPPIDX_806MHZ : new_opp_idx;
	} else if (p->pwr_thro_mode & (PWR_THRO_MODE_LBAT_1365MHZ | PWR_THRO_MODE_BAT_PER_1365MHZ)) {
		if (new_opp_idx < CPU_DVFS_OPPIDX_1365MHZ)
			cpufreq_dbg("%s(): for power throttling = %d\n", __func__, CPU_DVFS_OPPIDX_1365MHZ);

		new_opp_idx = (new_opp_idx < CPU_DVFS_OPPIDX_1365MHZ) ? CPU_DVFS_OPPIDX_1365MHZ : new_opp_idx;
	}

	if (   cpu_dvfs_is(p,MT_CPU_DVFS_LITTLE)
	    && hps_get_num_online_cpus(&online_cpu_little, &online_cpu_big) != -1
	    && ((online_cpu_little + num_online_cpus_little_delta_for_403) > 1)
	    && new_opp_idx == 7 
	    )
	{
		new_opp_idx--;
	}
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC

	if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) //Big|Little
		*p_cpu_oppidx = new_opp_idx | ((*p_cpu_oppidx) & 0xF0);
	else
		*p_cpu_oppidx = (new_opp_idx << 4) | ((*p_cpu_oppidx) & 0x0F);

#endif
	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static void bat_per_protection_powerlimit(BATTERY_PERCENT_LEVEL level)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		cpufreq_lock(flags);

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl_for_pwr_thro = p->idx_opp_tbl;

		switch (level) {
		case BATTERY_PERCENT_LEVEL_1:
			// Trigger CPU Limit to under CA7 x 4 + 1.36GHz
			p->pwr_thro_mode |= PWR_THRO_MODE_BAT_PER_1365MHZ;
			break;

		default:
			// unlimit cpu and gpu
			p->pwr_thro_mode &= ~PWR_THRO_MODE_BAT_PER_1365MHZ;
			break;
		}

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl = p->idx_opp_tbl_for_pwr_thro;

		cpufreq_unlock(flags);

		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, p->idx_opp_tbl);

		switch (level) {
		case BATTERY_PERCENT_LEVEL_1:
			// Trigger CPU Limit to under CA7 x 4 + 1.36GHz
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 0); // TODO: FIXME
			break;

		default:
			// unlimit cpu and gpu
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 4); // TODO: FIXME
			break;
		}
	}
}

static void bat_oc_protection_powerlimit(BATTERY_OC_LEVEL level)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p) || !cpu_dvfs_is(p, MT_CPU_DVFS_BIG)) /* just for big */
			continue;

		cpufreq_lock(flags);

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl_for_pwr_thro = p->idx_opp_tbl;

		switch (level) {
		case BATTERY_OC_LEVEL_1:
			// battery OC trigger CPU Limit to under CA17 x 4 + 0.8G
			p->pwr_thro_mode |= PWR_THRO_MODE_BAT_OC_806MHZ;
			break;

		default:
			// unlimit cpu and gpu
			p->pwr_thro_mode &= ~PWR_THRO_MODE_BAT_OC_806MHZ;
			break;
		}

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl = p->idx_opp_tbl_for_pwr_thro;

		cpufreq_unlock(flags);

		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, p->idx_opp_tbl);
	}
}

void Lbat_protection_powerlimit(LOW_BATTERY_LEVEL level)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		cpufreq_lock(flags);

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl_for_pwr_thro = p->idx_opp_tbl;

		switch (level) {
		case LOW_BATTERY_LEVEL_1:
			//1st LV trigger CPU Limit to under CA7 x 4
			p->pwr_thro_mode &= ~PWR_THRO_MODE_LBAT_1365MHZ;
			break;

		case LOW_BATTERY_LEVEL_2:
			//2nd LV trigger CPU Limit to under CA7 x 4 + 1.36G
			p->pwr_thro_mode |= PWR_THRO_MODE_LBAT_1365MHZ;
			break;

		default:
			//unlimit cpu and gpu
			p->pwr_thro_mode &= ~PWR_THRO_MODE_LBAT_1365MHZ;
			break;
		}

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl = p->idx_opp_tbl_for_pwr_thro;

		cpufreq_unlock(flags);

		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, p->idx_opp_tbl);

		switch (level) {
		case LOW_BATTERY_LEVEL_1:
			//1st LV trigger CPU Limit to under CA7 x 4
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 0); // TODO: FIXME
			break;

		case LOW_BATTERY_LEVEL_2:
			//2nd LV trigger CPU Limit to under CA7 x 4 + 1.36G
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 0); // TODO: FIXME
			break;

		default:
			//unlimit cpu and gpu
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 4); // TODO: FIXME
			break;
		}
	}
}

/*
 * cpufreq driver
 */
static int _mt_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct mt_cpu_dvfs *p;
	int ret = 0; /* cpufreq_frequency_table_verify() always return 0 */

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	BUG_ON(NULL == p);

	ret = cpufreq_frequency_table_verify(policy, p->freq_tbl_for_cpufreq);

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_target(struct cpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
	//unsigned int cpu;							// XXX: move to _cpufreq_set_locked()
	//struct cpufreq_freqs freqs;						// XXX: move to _cpufreq_set_locked()
	unsigned int new_opp_idx;

	enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);

	// unsigned long flags;							// XXX: move to _mt_cpufreq_set()
	int ret = 0; /* -EINVAL; */

	FUNC_ENTER(FUNC_LV_MODULE);

	if (policy->cpu >= num_possible_cpus() // TODO: FIXME
	    || cpufreq_frequency_table_target(policy, id_to_cpu_dvfs(id)->freq_tbl_for_cpufreq, target_freq, relation, &new_opp_idx)
	    || (id_to_cpu_dvfs(id) && id_to_cpu_dvfs(id)->is_fixed_freq)
	   )
		return -EINVAL;

	//freqs.old = policy->cur;						// XXX: move to _cpufreq_set_locked()
	//freqs.new = mt_cpufreq_max_frequency_by_DVS(id, new_opp_idx);		// XXX: move to _cpufreq_set_locked()
	//freqs.cpu = policy->cpu;						// XXX: move to _cpufreq_set_locked()

	//for_each_online_cpu(cpu) { // TODO: big LITTLE issue (id mapping)	// XXX: move to _cpufreq_set_locked()
	//	freqs.cpu = cpu;						// XXX: move to _cpufreq_set_locked()
	//	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);	// XXX: move to _cpufreq_set_locked()
	//}									// XXX: move to _cpufreq_set_locked()

	// cpufreq_lock(flags);							// XXX: move to _mt_cpufreq_set()
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC

	if (id == MT_CPU_DVFS_LITTLE)
		*p_dvfs_state |= (1 << CPU_DVFS_LITTLE_IS_DOING_DVFS);
	else
		*p_dvfs_state |= (1 << CPU_DVFS_BIG_IS_DOING_DVFS);

#endif

	_mt_cpufreq_set(id, new_opp_idx);

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC

	if (id == MT_CPU_DVFS_LITTLE)
		*p_dvfs_state &= ~(1 << CPU_DVFS_LITTLE_IS_DOING_DVFS);
	else
		*p_dvfs_state &= ~(1 << CPU_DVFS_BIG_IS_DOING_DVFS);

#endif

	// cpufreq_unlock(flags);						// XXX: move to _mt_cpufreq_set()

	//for_each_online_cpu(cpu) { // TODO: big LITTLE issue (id mapping)	// XXX: move to _cpufreq_set_locked()
	//	freqs.cpu = cpu;						// XXX: move to _cpufreq_set_locked()
	//	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);	// XXX: move to _cpufreq_set_locked()
	//}									// XXX: move to _cpufreq_set_locked()

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret = -EINVAL;

	FUNC_ENTER(FUNC_LV_MODULE);

	if (policy->cpu >= num_possible_cpus()) // TODO: FIXME
		return -EINVAL;

	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_setall(policy->cpus);

	/*******************************************************
	* 1 us, assumed, will be overwrited by min_sampling_rate
	********************************************************/
	policy->cpuinfo.transition_latency = 1000;

	/*********************************************
	* set default policy and cpuinfo, unit : Khz
	**********************************************/
	{
		enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);
		struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
		unsigned int lv = read_efuse_speed(); /* i.e. g_cpufreq_get_ptp_level */
		struct opp_tbl_info *opp_tbl_info = (MT_CPU_DVFS_BIG == id) ? &opp_tbls_big[chip_ver][CPU_LV_TO_OPP_IDX(lv)] : &opp_tbls_little[chip_ver][CPU_LV_TO_OPP_IDX(lv)];

		BUG_ON(NULL == p);
		BUG_ON(!(lv == CPU_LEVEL_0 || lv == CPU_LEVEL_1 || lv == CPU_LEVEL_2));

		ret = _mt_cpufreq_setup_freqs_table(policy,
						    opp_tbl_info->opp_tbl,
						    opp_tbl_info->size
						   );

		policy->cpuinfo.max_freq = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->cpuinfo.min_freq = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		policy->cur = _mt_cpufreq_get_cur_phy_freq(id); /* use cur phy freq is better */
		policy->max = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->min = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		if (_sync_opp_tbl_idx(p) >= 0) /* sync p->idx_opp_tbl first before _restore_default_volt() */
			p->idx_normal_max_opp = p->idx_opp_tbl;

		/* restore default volt, sync opp idx, set default limit */
		// _restore_default_volt(p);

		_set_no_limited(p);
#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)
		_init_downgrade(p, read_efuse_speed());
#endif

		if (0 == policy->cpu) {
			register_battery_percent_notify(&bat_per_protection_powerlimit, BATTERY_PERCENT_PRIO_CPU_L);
			register_battery_oc_notify(&bat_oc_protection_powerlimit, BATTERY_OC_PRIO_CPU_L);
			register_low_battery_notify(&Lbat_protection_powerlimit, LOW_BATTERY_PRIO_CPU_L);
		}
	}

	if (ret)
		cpufreq_err("failed to setup frequency table\n");

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static unsigned int _mt_cpufreq_get(unsigned int cpu)
{
	struct mt_cpu_dvfs *p;

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu));

	BUG_ON(NULL == p);

	FUNC_EXIT(FUNC_LV_MODULE);

	return cpu_dvfs_get_cur_freq(p);
}

/*
 * Early suspend
 */
static bool _allow_dpidle_ctrl_vproc = false;

bool mt_cpufreq_earlysuspend_status_get(void)
{
	return _allow_dpidle_ctrl_vproc;
}
EXPORT_SYMBOL(mt_cpufreq_earlysuspend_status_get);

static void _mt_cpufreq_early_suspend(struct early_suspend *h)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	// mt_cpufreq_state_set(0); // TODO: it is not necessary because of limit_max_freq_early_suspend

	// mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE); // TODO: move to deepidle driver

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		p->limit_max_freq_early_suspend = true;

		p->idx_opp_tbl_for_late_resume = p->idx_opp_tbl;

		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p), CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		}
	}

	_allow_dpidle_ctrl_vproc = true;

	FUNC_EXIT(FUNC_LV_MODULE);
}

static void _mt_cpufreq_late_resume(struct early_suspend *h)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	_allow_dpidle_ctrl_vproc = false;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		p->limit_max_freq_early_suspend = false;

		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_freq_by_idx(p, p->idx_opp_tbl_for_late_resume), CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		}
	}

	// mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); // TODO: move to deepidle driver

	// mt_cpufreq_state_set(1); // TODO: it is not necessary because of limit_max_freq_early_suspend

	FUNC_EXIT(FUNC_LV_MODULE);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend _mt_cpufreq_early_suspend_handler = {
	.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend  = _mt_cpufreq_early_suspend,
	.resume   = _mt_cpufreq_late_resume,
};
#endif /* CONFIG_HAS_EARLYSUSPEND */

static struct freq_attr *_mt_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver _mt_cpufreq_driver = {
	.verify = _mt_cpufreq_verify,
	.target = _mt_cpufreq_target,
	.init   = _mt_cpufreq_init,
	.get    = _mt_cpufreq_get,
	.name   = "mt-cpufreq",
	.attr   = _mt_cpufreq_attr,
	//	.have_governor_per_policy = true,
};

/*
 * Platform driver
 */
static int _mt_cpufreq_suspend(struct device *dev)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	// mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_SUSPEND); // TODO: move to suspend driver

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		p->cpufreq_pause = true;

#if 0 // XXX: cpufreq_driver_target doesn't work @ suspend
		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p), CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		}

#else
		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, p->idx_normal_max_opp); // XXX: useless, decided @ _calc_new_opp_idx()
#endif
	}

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_resume(struct device *dev)
{
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	// mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); // TODO: move to suspend driver

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		p->cpufreq_pause = false;
	}

	// TODO: set big/LITTLE voltage???

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pm_restore_early(struct device *dev) /* for IPO-H HW(freq) / SW(opp_tbl_idx) */ // TODO: DON'T CARE???
{
	FUNC_ENTER(FUNC_LV_MODULE);

	_mt_cpufreq_sync_opp_tbl_idx();

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret;

	FUNC_ENTER(FUNC_LV_MODULE);

	// TODO: check extBuck init with James

	/* init static power table */
	mt_spower_init();

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	_mt_cpufreq_aee_init();
#endif

	/* register early suspend */
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&_mt_cpufreq_early_suspend_handler);
#endif

	/* init PMIC_WRAP & volt */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#if 0 // TODO: FIXME
	/* restore default volt, sync opp idx, set default limit */
	{
		struct mt_cpu_dvfs *p;
		int i;

		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_availiable(p))
				continue;

			_restore_default_volt(p);

			if (_sync_opp_tbl_idx(p) >= 0)
				p->idx_normal_max_opp = p->idx_opp_tbl;

			_set_no_limited(p);

#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)
			_init_downgrade(p, read_efuse_speed());
#endif
		}
	}
#endif
	ret = cpufreq_register_driver(&_mt_cpufreq_driver);
	register_hotcpu_notifier(&turbo_mode_cpu_notifier); // <-XXX

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_remove(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	unregister_hotcpu_notifier(&turbo_mode_cpu_notifier); // <-XXX
	cpufreq_unregister_driver(&_mt_cpufreq_driver);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static const struct dev_pm_ops _mt_cpufreq_pm_ops = {
	.suspend	= _mt_cpufreq_suspend,
	.resume		= _mt_cpufreq_resume,
	.restore_early	= _mt_cpufreq_pm_restore_early,
	.freeze		= _mt_cpufreq_suspend,
	.thaw		= _mt_cpufreq_resume,
	.restore	= _mt_cpufreq_resume,
};

static struct platform_driver _mt_cpufreq_pdrv = {
	.probe      = _mt_cpufreq_pdrv_probe,
	.remove     = _mt_cpufreq_pdrv_remove,
	.driver     = {
		.name   = "mt-cpufreq",
		.pm     = &_mt_cpufreq_pm_ops,
		.owner  = THIS_MODULE,
	},
};

#ifndef __KERNEL__
/*
 * For CTP
 */
int mt_cpufreq_pdrv_probe(void)
{
	static struct cpufreq_policy policy_little;
	static struct cpufreq_policy policy_big;

	_mt_cpufreq_pdrv_probe(NULL);

	policy_little.cpu = cpu_dvfs[MT_CPU_DVFS_LITTLE].cpu_id;
	_mt_cpufreq_init(&policy_little);

	policy_big.cpu = cpu_dvfs[MT_CPU_DVFS_BIG].cpu_id;
	_mt_cpufreq_init(&policy_big);

	return 0;
}

int mt_cpufreq_set_opp_volt(enum mt_cpu_dvfs_id id, int idx)
{
	static struct opp_tbl_info *info;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	switch (id) {
	case MT_CPU_DVFS_LITTLE:
		info = &opp_tbls_little[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)];
		break;

	case MT_CPU_DVFS_BIG:
	default:
		info = &opp_tbls_big[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)];
		break;
	}

	if (idx >= info->size)
		return -1;


	return _set_cur_volt_locked(p, info->opp_tbl[idx].cpufreq_volt);
}

int mt_cpufreq_set_freq(enum mt_cpu_dvfs_id id, int idx)
{
	unsigned int cur_freq;
	unsigned int target_freq;
	int ret;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	cur_freq = p->ops->get_cur_phy_freq(p);
	target_freq = cpu_dvfs_get_freq_by_idx(p, idx);

	ret = _cpufreq_set_locked(p, cur_freq, target_freq);

	if (ret < 0)
		return ret;

	return target_freq;
}

#include "dvfs.h"

/* MCUSYS Register */

// APB Module ca15l_config
#define CA15L_CONFIG_BASE (0x10200200)

#define IR_ROSC_CTL             (MCUCFG_BASE + 0x030)
#define CA15L_MON_SEL           (CA15L_CONFIG_BASE + 0x01C)
#define pminit_write(addr, val) mt65xx_reg_sync_writel((val), ((void *)addr))

static unsigned int _mt_get_bigcpu_freq(void)
{
	int output = 0;
	unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1, clk26cali_1;
	unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl, ca15l_mon_sel;

	clk26cali_0 = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

	clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
	pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00); // select divider

	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	pminit_write(CLK_CFG_8, (46 << 8)); // select abist_cksw

	top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
	pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFF3) | (0x1 << 2));

	top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
	pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFC1F) | (0xb << 5));

	ca15l_mon_sel = DRV_Reg32(CA15L_MON_SEL);
	DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel | 0x00000500);

	ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x10000000);

	temp = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, temp | 0x1); // start fmeter

	/* wait frequency meter finish */
	while (DRV_Reg32(CLK26CALI_0) & 0x1) {
		printf("wait for frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
		//mdelay(10);
	}

	temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000) / 1024) * 4; // Khz

	pminit_write(CLK_CFG_8, clk_cfg_8);
	pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
	pminit_write(CLK26CALI_0, clk26cali_0);
	pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
	pminit_write(TOP_CKDIV1, top_ckdiv1);
	DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

	//print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output);

	return output;
}

static unsigned int _mt_get_smallcpu_freq(void)
{
	int output = 0;
	unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1, clk26cali_1;
	unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl;

	clk26cali_0 = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

	clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
	pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00); // select divider

	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	pminit_write(CLK_CFG_8, (46 << 8)); // select armpll_occ_mon

	top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
	pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFFC) | 0x1);

	top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
	pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFFE0) | 0xb);

	ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x08100000);

	temp = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, temp | 0x1); // start fmeter

	/* wait frequency meter finish */
	while (DRV_Reg32(CLK26CALI_0) & 0x1) {
		printf("wait for frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
		//mdelay(10);
	}

	temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000) / 1024) * 4; // Khz

	pminit_write(CLK_CFG_8, clk_cfg_8);
	pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
	pminit_write(CLK26CALI_0, clk26cali_0);
	pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
	pminit_write(TOP_CKDIV1, top_ckdiv1);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

	//print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output);

	return output;
}

unsigned int dvfs_get_cpu_freq(enum mt_cpu_dvfs_id id)
{
	return _mt_cpufreq_get_cur_phy_freq(id);
}

void dvfs_set_cpu_freq_FH(enum mt_cpu_dvfs_id id, int freq)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int idx;

	if (!p) {
		cpufreq_err("%s(%d, %d), id is wrong\n", __func__, id, freq);
		return;
	}

	idx = _search_available_freq_idx(p, freq, CPUFREQ_RELATION_H);

	if (-1 == idx) {
		cpufreq_err("%s(%d, %d), freq is wrong\n", __func__, id, freq);
		return;
	}

	mt_cpufreq_set_freq(id, idx);
}

unsigned int cpu_frequency_output_slt(enum mt_cpu_dvfs_id id)
{
	return (MT_CPU_DVFS_LITTLE == id) ? _mt_get_smallcpu_freq() : _mt_get_bigcpu_freq();
}

void dvfs_set_cpu_volt(enum mt_cpu_dvfs_id id, int mv)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	cpufreq_dbg("%s(%d, %d)\n", __func__, id, mv);

	if (!p) {
		cpufreq_err("%s(%d, %d), id is wrong\n", __func__, id, mv);
		return;
	}

	if (_set_cur_volt_locked(p, mv))
		cpufreq_err("%s(%d, %d), set volt fail\n", __func__, id, mv);
}

void dvfs_set_gpu_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VGPU, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VGPU);
}

void dvfs_set_vcore_ao_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_AO_NORMAL, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_DI_VCORE_AO_NORMAL);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
}

void dvfs_set_vcore_pdn_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_PDN_NORMAL, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_DI_VCORE_PDN_NORMAL);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
}

static unsigned int little_freq_backup;
static unsigned int big_freq_backup;
static unsigned int vgpu_backup;
static unsigned int vcore_ao_backup;
static unsigned int vcore_pdn_backup;

void dvfs_disable_by_ptpod(void)
{
	cpufreq_dbg("%s()\n", __func__); // <-XXX
	little_freq_backup = _mt_cpufreq_get_cur_phy_freq(MT_CPU_DVFS_LITTLE);
	big_freq_backup = _mt_cpufreq_get_cur_phy_freq(MT_CPU_DVFS_BIG);
	pmic_read_interface(PMIC_ADDR_VGPU_VOSEL_ON, &vgpu_backup, 0x7F, 0);
	pmic_read_interface(PMIC_ADDR_VCORE_AO_VOSEL_ON, &vcore_ao_backup, 0x7F, 0);
	pmic_read_interface(PMIC_ADDR_VCORE_PDN_VOSEL_ON, &vcore_pdn_backup, 0x7F, 0);

	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_LITTLE, 1140000);
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_BIG, 1140000);
	dvfs_set_gpu_volt(0x30);
	dvfs_set_vcore_ao_volt(0x38);
	dvfs_set_vcore_pdn_volt(0x30);
}

void dvfs_enable_by_ptpod(void)
{
	cpufreq_dbg("%s()\n", __func__); // <-XXX
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_LITTLE, little_freq_backup);
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_BIG, big_freq_backup);
	dvfs_set_gpu_volt(vgpu_backup);
	dvfs_set_vcore_ao_volt(vcore_ao_backup);
	dvfs_set_vcore_pdn_volt(vcore_pdn_backup);
}
#endif /* ! __KERNEL__ */

#ifdef CONFIG_PROC_FS
/*
 * PROC
 */

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

/* cpufreq_debug */
static int cpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cpufreq debug (log level) = %d\n"
		   "cpufreq debug (ptp level) = %d, 0x%08X, 0x%08X, 0x%08X\n",
		   func_lv_mask,
		   read_efuse_speed(),
		   get_devinfo_with_index(CPUFREQ_EFUSE_INDEX),
		   cpu_dvfs[MT_CPU_DVFS_LITTLE].pwr_thro_mode,
		   cpu_dvfs[MT_CPU_DVFS_BIG].pwr_thro_mode
		  );

	return 0;
}

static ssize_t cpufreq_debug_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	int dbg_lv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &dbg_lv) == 1)
		func_lv_mask = dbg_lv;
	else
		cpufreq_err("echo dbg_lv (dec) > /proc/cpufreq/cpufreq_debug\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_downgrade_freq_info */
static int cpufreq_downgrade_freq_info_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "downgrade_freq_counter_limit = %d\n"
		   "ptpod_temperature_limit_1 = %d\n"
		   "ptpod_temperature_limit_2 = %d\n"
		   "ptpod_temperature_time_1 = %d\n"
		   "ptpod_temperature_time_2 = %d\n"
		   "downgrade_freq_counter_return_limit 1 = %d\n"
		   "downgrade_freq_counter_return_limit 2 = %d\n"
		   "over_max_cpu = %d\n",
		   p->downgrade_freq_counter_limit,
		   p->ptpod_temperature_limit_1,
		   p->ptpod_temperature_limit_2,
		   p->ptpod_temperature_time_1,
		   p->ptpod_temperature_time_2,
		   p->ptpod_temperature_limit_1 * p->ptpod_temperature_time_1,
		   p->ptpod_temperature_limit_2 * p->ptpod_temperature_time_2,
		   p->over_max_cpu
		  );

	return 0;
}

/* cpufreq_downgrade_freq_counter_limit */
static int cpufreq_downgrade_freq_counter_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->downgrade_freq_counter_limit);

	return 0;
}

static ssize_t cpufreq_downgrade_freq_counter_limit_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int downgrade_freq_counter_limit;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &downgrade_freq_counter_limit) == 1)
		p->downgrade_freq_counter_limit = downgrade_freq_counter_limit;
	else
		cpufreq_err("echo downgrade_freq_counter_limit (dec) > /proc/cpufreq/%s/cpufreq_downgrade_freq_counter_limit\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_downgrade_freq_counter_return_limit */
static int cpufreq_downgrade_freq_counter_return_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->downgrade_freq_counter_return_limit);

	return 0;
}

static ssize_t cpufreq_downgrade_freq_counter_return_limit_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int downgrade_freq_counter_return_limit;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &downgrade_freq_counter_return_limit) == 1)
		p->downgrade_freq_counter_return_limit = downgrade_freq_counter_return_limit; // TODO: p->ptpod_temperature_limit_1 * p->ptpod_temperature_time_1 or p->ptpod_temperature_limit_2 * p->ptpod_temperature_time_2
	else
		cpufreq_err("echo downgrade_freq_counter_return_limit (dec) > /proc/cpufreq/%s/cpufreq_downgrade_freq_counter_return_limit\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_fftt_test */
#include <asm/sched_clock.h>

static unsigned long _delay_us;
static unsigned long _delay_us_buf;

static int cpufreq_fftt_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", _delay_us);

	if (_delay_us < _delay_us_buf)
		cpufreq_err("@%s(), %lu < %lu, loops_per_jiffy = %lu\n", __func__, _delay_us, _delay_us_buf, loops_per_jiffy);

	return 0;
}

static ssize_t cpufreq_fftt_test_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%lu", &_delay_us_buf) == 1) {
		unsigned long start;

		start = (unsigned long)sched_clock();
		udelay(_delay_us_buf);
		_delay_us = ((unsigned long)sched_clock() - start) / 1000;

		cpufreq_ver("@%s(%lu), _delay_us = %lu, loops_per_jiffy = %lu\n", __func__, _delay_us_buf, _delay_us, loops_per_jiffy);
	}

	free_page((unsigned int)buf);

	return count;
}

/* cpufreq_limited_by_hevc */
static int cpufreq_limited_by_hevc_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->limited_freq_by_hevc);

	return 0;
}

static ssize_t cpufreq_limited_by_hevc_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int limited_freq_by_hevc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &limited_freq_by_hevc) == 1) {

		p->limited_freq_by_hevc = limited_freq_by_hevc;

		if (cpu_dvfs_is_availiable(p) && (p->limited_freq_by_hevc > cpu_dvfs_get_cur_freq(p))) {
			struct cpufreq_policy *policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				cpufreq_driver_target(policy, p->limited_freq_by_hevc, CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy);
			}
		}
	} else
		cpufreq_err("echo limited_freq_by_hevc (dec) > /proc/cpufreq/%s/cpufreq_limited_by_hevc\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

static int cpufreq_limited_max_freq_by_user_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->limited_max_freq_by_user);

	return 0;
}

static ssize_t cpufreq_limited_max_freq_by_user_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int limited_max_freq;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &limited_max_freq) == 1) {

		p->limited_max_freq_by_user = limited_max_freq;

		if (cpu_dvfs_is_availiable(p) && (p->limited_max_freq_by_user < cpu_dvfs_get_cur_freq(p))) {
			struct cpufreq_policy *policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				cpufreq_driver_target(policy, p->limited_max_freq_by_user, CPUFREQ_RELATION_H);
				cpufreq_cpu_put(policy);
			}
		}
	} else
		cpufreq_err("echo limited_max_freq (dec) > /proc/cpufreq/%s/cpufreq_limited_max_freq_by_user\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_limited_power */
static int cpufreq_limited_power_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		seq_printf(m, "[%s/%d] %d\n"
			   "limited_max_freq = %d\n"
			   "limited_max_ncpu = %d\n",
			   p->name, i, p->thermal_protect_limited_power,
			   p->limited_max_freq,
			   p->limited_max_ncpu
			  );
	}

	return 0;
}

static ssize_t cpufreq_limited_power_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	int i, tlp, limited_power;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &limited_power) == 1) {
		if (0) { // (limited_power == -1) {
			tlp = 7; // TODO: FIXME, int hps_get_tlp(unsigned int * tlp_ptr), fix TLP = 8

			for (i = 0; i < cpu_tlp_power_tbl[tlp].nr_power_table; i++) {
				mt_cpufreq_thermal_protect(cpu_tlp_power_tbl[tlp].power_tbl[i].power);
				msleep(1000);
			}
		} else {
			mt_cpufreq_thermal_protect(limited_power); // TODO: specify limited_power by id???
		}
	} else
		cpufreq_err("echo limited_power (dec) > /proc/cpufreq/cpufreq_limited_power\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_over_max_cpu */
static int cpufreq_over_max_cpu_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->over_max_cpu);

	return 0;
}

static ssize_t cpufreq_over_max_cpu_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int over_max_cpu;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &over_max_cpu) == 1)
		p->over_max_cpu = over_max_cpu;
	else
		cpufreq_err("echo over_max_cpu (dec) > /proc/cpufreq/%s/cpufreq_over_max_cpu\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_power_dump */
static int cpufreq_power_dump_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i, j;

	if (chip_ver == CHIP_SW_VER_01) {
		for_each_cpu_dvfs(i, p) {
			seq_printf(m, "[%s/%d]\n", p->name, i);

			for (j = 0; j < p->nr_power_tbl; j++) {
				seq_printf(m, "[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d, },\n",
					   j,
					   p->power_tbl[j].cpufreq_khz,
					   p->power_tbl[j].cpufreq_ncpu,
					   p->power_tbl[j].cpufreq_power
					  );
			}
		}
	} else {
		for (i = 0; i < 8; i++) {
			seq_printf(m, "[TLP/%d]\n", i + 1);

			for (j = 0; j < cpu_tlp_power_tbl[i].nr_power_table; j++) {
				seq_printf(m, "[%d] = { .ncpu_big = %d,\t.khz_big = %d,\t.ncpu_little = %d,\t.khz_little = %d,\t.performance = %d,\t.power = %d, },\n",
					   j,
					   cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big,
					   cpu_tlp_power_tbl[i].power_tbl[j].khz_big,
					   cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little,
					   cpu_tlp_power_tbl[i].power_tbl[j].khz_little,
					   cpu_tlp_power_tbl[i].power_tbl[j].performance,
					   cpu_tlp_power_tbl[i].power_tbl[j].power
					  );
			}
		}
	}

	return 0;
}

/* cpufreq_ptpod_freq_volt */
static int cpufreq_ptpod_freq_volt_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int j;

	for (j = 0; j < p->nr_opp_tbl; j++) {
		seq_printf(m, "[%d] = { .cpufreq_khz = %d,\t.cpufreq_volt = %d,\t.cpufreq_volt_org = %d, },\n",
			   j,
			   p->opp_tbl[j].cpufreq_khz,
			   p->opp_tbl[j].cpufreq_volt,
			   p->opp_tbl[j].cpufreq_volt_org
			  );
	}

	return 0;
}

/* cpufreq_ptpod_temperature_limit */
static int cpufreq_ptpod_temperature_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "ptpod_temperature_limit_1 = %d\n"
		   "ptpod_temperature_limit_2 = %d\n",
		   p->ptpod_temperature_limit_1,
		   p->ptpod_temperature_limit_2
		  );

	return 0;
}

static ssize_t cpufreq_ptpod_temperature_limit_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int ptpod_temperature_limit_1;
	int ptpod_temperature_limit_2;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &ptpod_temperature_limit_1, &ptpod_temperature_limit_2) == 2) {
		p->ptpod_temperature_limit_1 = ptpod_temperature_limit_1;
		p->ptpod_temperature_limit_2 = ptpod_temperature_limit_2;
	} else
		cpufreq_err("echo ptpod_temperature_limit_1 (dec) ptpod_temperature_limit_2 (dec) > /proc/cpufreq/%s/cpufreq_ptpod_temperature_limit\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_ptpod_temperature_time */
static int cpufreq_ptpod_temperature_time_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "ptpod_temperature_time_1 = %d\n"
		   "ptpod_temperature_time_2 = %d\n",
		   p->ptpod_temperature_time_1,
		   p->ptpod_temperature_time_2
		  );

	return 0;
}

static ssize_t cpufreq_ptpod_temperature_time_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int ptpod_temperature_time_1;
	int ptpod_temperature_time_2;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &ptpod_temperature_time_1, &ptpod_temperature_time_2) == 2) {
		p->ptpod_temperature_time_1 = ptpod_temperature_time_1;
		p->ptpod_temperature_time_2 = ptpod_temperature_time_2;
	} else
		cpufreq_err("echo ptpod_temperature_time_1 (dec) ptpod_temperature_time_2 (dec) > /proc/cpufreq/%s/cpufreq_ptpod_temperature_time\n", p->name);

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_ptpod_test */
static int cpufreq_ptpod_test_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cpufreq_ptpod_test_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	return count;
}

/* cpufreq_state */
static int cpufreq_state_proc_show(struct seq_file *m, void *v) // TODO: keep this function???
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "cpufreq_pause = %d\n",
			   p->name, i,
			   p->cpufreq_pause
			  );
	}

	return 0;
}

static ssize_t cpufreq_state_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) // TODO: keep this function???
{
	int enable;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &enable) == 1)
		mt_cpufreq_state_set(enable);
	else
		cpufreq_err("echo 1/0 > /proc/cpufreq/cpufreq_state\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_oppidx */
static int cpufreq_oppidx_proc_show(struct seq_file *m, void *v) // <-XXX
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int j;

	seq_printf(m, "[%s/%d]\n"
		   "cpufreq_oppidx = %d\n",
		   p->name, p->cpu_id,
		   p->idx_opp_tbl
		  );

	for (j = 0; j < p->nr_opp_tbl; j++) {
		seq_printf(m, "\tOP(%d, %d),\n",
			   cpu_dvfs_get_freq_by_idx(p, j),
			   cpu_dvfs_get_volt_by_idx(p, j)
			  );
	}

	return 0;
}

static ssize_t cpufreq_oppidx_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) // <-XXX
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int oppidx;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);

	if (sscanf(buf, "%d", &oppidx) == 1
	    && 0 <= oppidx && oppidx < p->nr_opp_tbl
	   ) {
		p->is_fixed_freq = true;
		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, oppidx); // TOOD: FIXME, MT_CPU_DVFS_LITTLE, MT_CPU_DVFS_BIG
	} else {
		p->is_fixed_freq = false; // TODO: FIXME
		cpufreq_err("echo oppidx > /proc/cpufreq/%s/cpufreq_oppidx (0 <= %d < %d)\n", p->name, oppidx, p->nr_opp_tbl);
	}

	free_page((unsigned int)buf);

	return count;
}

/* cpufreq_freq */
static int cpufreq_freq_proc_show(struct seq_file *m, void *v) // <-XXX
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->ops->get_cur_phy_freq(p));

	return 0;
}

static ssize_t cpufreq_freq_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) // <-XXX
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	unsigned int cur_freq;
	int freq;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);

	if (sscanf(buf, "%d", &freq) == 1) {
		p->is_fixed_freq = true; // TODO: FIXME
		cpufreq_lock(flags);	// <-XXX
		cur_freq = p->ops->get_cur_phy_freq(p);
		p->ops->set_cur_freq(p, cur_freq, freq);
		cpufreq_unlock(flags);	// <-XXX
	} else {
		p->is_fixed_freq = false; // TODO: FIXME
		cpufreq_err("echo khz > /proc/cpufreq/%s/cpufreq_freq\n", p->name);
	}

	free_page((unsigned int)buf);

	return count;
}

/* cpufreq_volt */
static int cpufreq_volt_proc_show(struct seq_file *m, void *v) // <-XXX
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->ops->get_cur_volt(p));

	return 0;
}

static ssize_t cpufreq_volt_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) // <-XXX
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int mv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &mv) == 1) {
		p->is_fixed_freq = true; // TODO: FIXME
		cpufreq_lock(flags);
		_set_cur_volt_locked(p, mv);
		cpufreq_unlock(flags);
	} else {
		p->is_fixed_freq = false; // TODO: FIXME
		cpufreq_err("echo mv > /proc/cpufreq/%s/cpufreq_volt\n", p->name);
	}

	free_page((unsigned int)buf);

	return count;
}

/* cpufreq_turbo_mode */
static int cpufreq_turbo_mode_proc_show(struct seq_file *m, void *v) // <-XXX
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int i;

	seq_printf(m, "turbo_mode = %d\n", p->turbo_mode);

	for (i = 0; i < NR_TURBO_MODE; i++) {
		seq_printf(m, "[%d] = { .temp = %d, .freq_delta = %d, .volt_delta = %d }\n",
			   i,
			   turbo_mode_cfg[i].temp,
			   turbo_mode_cfg[i].freq_delta,
			   turbo_mode_cfg[i].volt_delta
			  );
	}

	return 0;
}

static ssize_t cpufreq_turbo_mode_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) // <-XXX
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	unsigned int turbo_mode;
	int temp;
	int freq_delta;
	int volt_delta;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if ((sscanf(buf, "%d %d %d %d", &turbo_mode, &temp, &freq_delta, &volt_delta) == 4) && turbo_mode < NR_TURBO_MODE) {
		turbo_mode_cfg[turbo_mode].temp = temp;
		turbo_mode_cfg[turbo_mode].freq_delta = freq_delta;
		turbo_mode_cfg[turbo_mode].volt_delta = volt_delta;
	} else if (sscanf(buf, "%d", &turbo_mode) == 1)
		p->turbo_mode = turbo_mode; // TODO: FIXME
	else {
		cpufreq_err("echo 0/1 > /proc/cpufreq/%s/cpufreq_turbo_mode\n", p->name);
		cpufreq_err("echo idx temp freq_delta volt_delta > /proc/cpufreq/%s/cpufreq_turbo_mode\n", p->name);
	}

	free_page((unsigned int)buf);

	return count;
}

#define PROC_FOPS_RW(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
	{									\
		return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
	}									\
	static const struct file_operations name ## _proc_fops = {		\
		.owner          = THIS_MODULE,					\
		.open           = name ## _proc_open,				\
		.read           = seq_read,					\
		.llseek         = seq_lseek,					\
		.release        = single_release,				\
		.write          = name ## _proc_write,				\
	}

#define PROC_FOPS_RO(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
	{									\
		return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
	}									\
	static const struct file_operations name ## _proc_fops = {		\
		.owner          = THIS_MODULE,					\
		.open           = name ## _proc_open,				\
		.read           = seq_read,					\
		.llseek         = seq_lseek,					\
		.release        = single_release,				\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(cpufreq_debug);
PROC_FOPS_RW(cpufreq_fftt_test);
PROC_FOPS_RW(cpufreq_limited_power);
PROC_FOPS_RO(cpufreq_power_dump);
PROC_FOPS_RW(cpufreq_ptpod_test);
PROC_FOPS_RW(cpufreq_state);

PROC_FOPS_RO(cpufreq_downgrade_freq_info);
PROC_FOPS_RW(cpufreq_downgrade_freq_counter_limit);
PROC_FOPS_RW(cpufreq_downgrade_freq_counter_return_limit);
PROC_FOPS_RW(cpufreq_limited_by_hevc);
PROC_FOPS_RW(cpufreq_limited_max_freq_by_user);
PROC_FOPS_RW(cpufreq_over_max_cpu);
PROC_FOPS_RO(cpufreq_ptpod_freq_volt);
PROC_FOPS_RW(cpufreq_ptpod_temperature_limit);
PROC_FOPS_RW(cpufreq_ptpod_temperature_time);
PROC_FOPS_RW(cpufreq_oppidx); // <-XXX
PROC_FOPS_RW(cpufreq_freq); // <-XXX
PROC_FOPS_RW(cpufreq_volt); // <-XXX
PROC_FOPS_RW(cpufreq_turbo_mode); // <-XXX

static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	struct proc_dir_entry *cpu_dir = NULL;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);
	int i, j;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(cpufreq_debug),
		PROC_ENTRY(cpufreq_fftt_test),
		PROC_ENTRY(cpufreq_limited_power),
		PROC_ENTRY(cpufreq_power_dump),
		PROC_ENTRY(cpufreq_ptpod_test),
		PROC_ENTRY(cpufreq_state),
	};

	const struct pentry cpu_entries[] = {
		PROC_ENTRY(cpufreq_downgrade_freq_info),
		PROC_ENTRY(cpufreq_downgrade_freq_counter_limit),
		PROC_ENTRY(cpufreq_downgrade_freq_counter_return_limit),
		PROC_ENTRY(cpufreq_limited_by_hevc),
		PROC_ENTRY(cpufreq_limited_max_freq_by_user),
		PROC_ENTRY(cpufreq_over_max_cpu),
		PROC_ENTRY(cpufreq_ptpod_freq_volt),
		PROC_ENTRY(cpufreq_ptpod_temperature_limit),
		PROC_ENTRY(cpufreq_ptpod_temperature_time),
		PROC_ENTRY(cpufreq_oppidx), // <-XXX
		PROC_ENTRY(cpufreq_freq), // <-XXX
		PROC_ENTRY(cpufreq_volt), // <-XXX
		PROC_ENTRY(cpufreq_turbo_mode), // <-XXX
	};

	dir = proc_mkdir("cpufreq", NULL);

	if (!dir) {
		cpufreq_err("fail to create /proc/cpufreq @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__, entries[i].name);
	}

	for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
		if (!proc_create_data(cpu_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, cpu_entries[i].fops, p))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__, entries[i].name);
	}

	for_each_cpu_dvfs(j, p) {
		cpu_dir = proc_mkdir(p->name, dir);

		if (!cpu_dir) {
			cpufreq_err("fail to create /proc/cpufreq/%s @ %s()\n", p->name, __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
			if (!proc_create_data(cpu_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, cpu_dir, cpu_entries[i].fops, p))
				cpufreq_err("%s(), create /proc/cpufreq/%s/%s failed\n", __func__, p->name, entries[i].name);
		}
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */

/*
 * Module driver
 */
static int __init _mt_cpufreq_pdrv_init(void)
{
	int ret = 0;

	FUNC_ENTER(FUNC_LV_MODULE);

	{
		int i;

		chip_ver = mt_get_chip_sw_ver();
		sched_get_big_little_cpus(&cpumask_big, &cpumask_little);

		switch (chip_ver) {
		case CHIP_SW_VER_01:
#ifdef MTK_FORCE_CLUSTER1
			sched_get_big_little_cpus(&cpumask_little, &cpumask_big);

			for (i = 0; i < pw.set[PMIC_WRAP_PHASE_DEEPIDLE].nr_idx; i++) {
				pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[i].cmd_addr = pw.set[PMIC_WRAP_PHASE_DEEPIDLE_BIG]._[i].cmd_addr;
				pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[i].cmd_wdata = pw.set[PMIC_WRAP_PHASE_DEEPIDLE_BIG]._[i].cmd_wdata;
			}

#endif
			break;

		case CHIP_SW_VER_02:
		default:
			break;
		}

		cpu_dvfs[MT_CPU_DVFS_BIG].cpu_id = cpumask_first(&cpumask_big);
		cpu_dvfs[MT_CPU_DVFS_LITTLE].cpu_id = cpumask_first(&cpumask_little);

		if (CPU_LEVEL_2 != read_efuse_speed()) cpu_dvfs[MT_CPU_DVFS_BIG].turbo_mode = 0;

		/*cpufreq_err("@%s():%d, chip_ver = %d, little.cpu_id = %d, cpumask_little = 0x%08X, big.cpu_id = %d, cpumask_big = 0x%08X\n",
			    __func__, __LINE__, chip_ver,
			    cpu_dvfs[MT_CPU_DVFS_LITTLE].cpu_id, cpumask_little,
			    cpu_dvfs[MT_CPU_DVFS_BIG].cpu_id, cpumask_big
			    );*/
	}

#ifdef CONFIG_PROC_FS

	/* init proc */
	if (_create_procfs())
		goto out;

#endif /* CONFIG_PROC_FS */

	/* register platform driver */
	ret = platform_driver_register(&_mt_cpufreq_pdrv);

	if (ret)
		cpufreq_err("fail to register cpufreq driver @ %s()\n", __func__);

out:
	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static void __exit _mt_cpufreq_pdrv_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	platform_driver_unregister(&_mt_cpufreq_pdrv);

	FUNC_EXIT(FUNC_LV_MODULE);
}

#if defined(CONFIG_CPU_DVFS_BRINGUP)
#else   /* CONFIG_CPU_DVFS_BRINGUP */
module_init(_mt_cpufreq_pdrv_init);
module_exit(_mt_cpufreq_pdrv_exit);
#endif  /* CONFIG_CPU_DVFS_BRINGUP */

MODULE_DESCRIPTION("MediaTek CPU DVFS Driver v0.3");
MODULE_LICENSE("GPL");
