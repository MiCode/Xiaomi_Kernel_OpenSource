/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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

#if 0				/* L318_Need_Related_File */
#include <linux/earlysuspend.h>
#endif				/* L318_Need_Related_File */
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#if 0				/* L318_Need_Related_File */
#include <linux/xlog.h>
#endif				/* L318_Need_Related_File */
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <asm/io.h>

#include <mt_chip.h>
#if 0				/* L318_Need_Related_File */
#include <linux/cpufreq.h>
#include <linux/clk.h>

#include <asm/system.h>

/* project includes */
#include "mach/mt_typedefs.h"

#include "mach/irqs.h"
#include "mach/mt_irq.h"
#include "mach/mt_thermal.h"
#include "mach/mt_spm_idle.h"
#include "mach/mt_pmic_wrap.h"
#endif				/* L318_Need_Related_File */
#include "mach/mt_freqhopping.h"
#include "mt_spm.h"
#if 0				/* L318_Need_Related_File */
#include "mach/mt_ptp.h"
#endif				/* L318_Need_Related_File */
#include "mt_static_power.h"
#if 0				/* L318_Need_Related_File */
#include "mach/upmu_sw.h"
#include "mach/mt_boot.h"

#include "mach/mtk_rtc_hal.h"
#include "mach/mt_rtc_hw.h"
#endif				/* L318_Need_Related_File */

/* local includes */
#include "mt_cpufreq.h"
#include "mt_hotplug_strategy.h"
#include "mt_hotplug_strategy_internal.h"
#if 0				/* L318_Need_Related_File */
#include "mach/mt_ptp2.h"
#endif				/* L318_Need_Related_File */

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include <linux/cpumask.h>
#include <asm/topology.h>
#include <mach/mt_thermal.h>

static void __iomem *apmixed_base;	/* 0x10209000 */
/* #define APMIXED_BASE     ((unsigned long)apmixed_base) */
#define APMIXED_BASE     apmixed_base

static void __iomem *infracfg_ao_base;	/* 0x10001000 */
/* #define INFRACFG_AO_BASE     ((unsigned long)infracfg_ao_base) */
#define INFRACFG_AO_BASE     infracfg_ao_base


#define TOP_CKMUXSEL            (INFRACFG_AO_BASE + 0x00)
#define TOP_CKDIV1              (INFRACFG_AO_BASE + 0x08)

struct regulator *reg_vpca53;	/* 0x0334 */
struct regulator *reg_ext_vpca57;	/* da9212 */
struct regulator *reg_vsramca57;	/* 0x024E */
struct regulator *reg_vcore_backup;	/* 0x027A */
struct regulator *reg_vgpu;	/* 0x??? */
struct clk *clk_pllca57;

int _mt_cpufreq_pdrv_probed = 0;


#endif

int	regulator_vpca53_voltage = 0;

#if 1	/* L318_Need_Related_File Dummy code */

/* mt_spm_idle.h */
void spm_mcdi_wakeup_all_cores(void)
{
}

/* mt_ptp2.h */
void turn_off_SPARK(char *message)
{
}

void turn_on_SPARK(void)
{
}
#endif	/* L318_Need_Related_File Dummy code */


/*=============================================================*/
/* Macro definition                                            */
/*=============================================================*/

/*
 * CONFIG
 */
#define FIXME 0

#define CONFIG_CPU_DVFS_SHOWLOG 1

/* #define CONFIG_CPU_DVFS_BRINGUP 1 *//* for bring up */
/* #define CONFIG_CPU_DVFS_RANDOM_TEST 1 *//* random test for UT/IT */
/* #define CONFIG_CPU_DVFS_FFTT_TEST 1 *//* FF TT SS volt test */
/* #define CONFIG_CPU_DVFS_TURBO 1 *//* turbo mode */
/* mt8173 not use cpufreq_hotplug policy, won't call into downgrade_freq */
/* #define CONFIG_CPU_DVFS_DOWNGRADE_FREQ 1 *//* downgrade freq */

#define MIN_DIFF_VSRAM_PROC     10
#define NORMAL_DIFF_VRSAM_VPROC 100
#define MAX_DIFF_VSRAM_VPROC    200

#define PMIC_SETTLE_TIME(old_mv, new_mv) \
	((((old_mv) > (new_mv)) ? ((old_mv) - (new_mv)) : ((new_mv) - (old_mv))) * 2 / 25 + 25 + 1)
#define PLL_SETTLE_TIME         (30)	/* us, PLL settle time, should not be changed */
#define RAMP_DOWN_TIMES         (2)	/* RAMP DOWN TIMES to postpone frequency degrade */
#define FHCTL_CHANGE_FREQ       (1000000)	/* if cross 1GHz when DFS, don't used FHCTL */

#define DEFAULT_VOLT_VGPU       (1125)
#define DEFAULT_VOLT_VCORE      (1125)

/* for DVFS OPP table *//*TODO: necessary or just specify in opp table directly??? */

/* SB combination */
#define DVFS_BIG_SB_F00 (2249000)	/* KHz */
#define DVFS_BIG_SB_F01 (2002000)	/* KHz */
#define DVFS_BIG_SB_F02 (1755000)	/* KHz */
#define DVFS_BIG_SB_F03 (1508000)	/* KHz */
#define DVFS_BIG_SB_F04 (1261000)	/* KHz */
#define DVFS_BIG_SB_F05 (1001000)	/* KHz */
#define DVFS_BIG_SB_F06 (702000)	/* KHz */
#define DVFS_BIG_SB_F07 (507000)	/* KHz */

#define DVFS_BIG_SB_F10 (2106000)	/* KHz */
#define DVFS_BIG_SB_F11 (1807000)	/* KHz */
#define DVFS_BIG_SB_F12 (1612000)	/* KHz */
#define DVFS_BIG_SB_F13 (1404000)	/* KHz */
#define DVFS_BIG_SB_F14 (1209000)	/* KHz */
#define DVFS_BIG_SB_F15 (1001000)	/* KHz */
#define DVFS_BIG_SB_F16 (702000)	/* KHz */
#define DVFS_BIG_SB_F17 (507000)	/* KHz */

#define DVFS_BIG_SB_V00 (1125)	/* mV */
#define DVFS_BIG_SB_V01 (1083)	/* mV */
#define DVFS_BIG_SB_V02 (1040)	/* mV */
#define DVFS_BIG_SB_V03 (998)	/* mV */
#define DVFS_BIG_SB_V04 (955)	/* mV */
#define DVFS_BIG_SB_V05 (911)	/* mV */
#define DVFS_BIG_SB_V06 (859)	/* mV */
#define DVFS_BIG_SB_V07 (826)	/* mV */

#define DVFS_BIG_SB_V10 (1125)	/* mV */
#define DVFS_BIG_SB_V11 (1088)	/* mV */
#define DVFS_BIG_SB_V12 (1049)	/* mV */
#define DVFS_BIG_SB_V13 (1007)	/* mV */
#define DVFS_BIG_SB_V14 (968)	/* mV */
#define DVFS_BIG_SB_V15 (927)	/* mV */
#define DVFS_BIG_SB_V16 (867)	/* mV */
#define DVFS_BIG_SB_V17 (828)	/* mV */

/* FY combination */
#define DVFS_BIG_F0 (1989000)	/* KHz */
#define DVFS_BIG_F01 (1911000)	/* KHz */
#define DVFS_BIG_F1 (1807000)	/* KHz */
#define DVFS_BIG_F2 (1651000)	/* KHz */
#define DVFS_BIG_F3 (1612000)	/* KHz */
#define DVFS_BIG_F4 (1508000)	/* KHz */
#define DVFS_BIG_F5 (1404000)	/* KHz */
#define DVFS_BIG_F6 (1261000)	/* KHz */
#define DVFS_BIG_F7 (1209000)	/* KHz */
#define DVFS_BIG_F8 (1001000)	/* KHz */
#define DVFS_BIG_F9 (910000)	/* KHz */
#define DVFS_BIG_F10 (702000)	/* KHz */
#define DVFS_BIG_F11 (507000)	/* KHz */

#define DVFS_BIG_V0 (1125)	/* mV */
#define DVFS_BIG_V01 (1109)	/* mV */
#define DVFS_BIG_V1 (1089)	/* mV */
#define DVFS_BIG_V2 (1056)	/* mV */
#define DVFS_BIG_V3 (1049)	/* mV */
#define DVFS_BIG_V4 (1028)	/* mV */
#define DVFS_BIG_V5 (1007)	/* mV */
#define DVFS_BIG_V6 (979)	/* mV */
#define DVFS_BIG_V7 (968)	/* mV */
#define DVFS_BIG_V8 (927)	/* mV */
#define DVFS_BIG_V9 (909)	/* mV */
#define DVFS_BIG_V10 (867)	/* mV */
#define DVFS_BIG_V11 (828)	/* mV */

/* SB combination */
#define DVFS_LITTLE_SB_F00 (1924000)	/* KHz */
#define DVFS_LITTLE_SB_F01 (1703000)	/* KHz */
#define DVFS_LITTLE_SB_F02 (1508000)	/* KHz */
#define DVFS_LITTLE_SB_F03 (1300000)	/* KHz */
#define DVFS_LITTLE_SB_F04 (1105000)	/* KHz */
#define DVFS_LITTLE_SB_F05 (1001000)	/* KHz */
#define DVFS_LITTLE_SB_F06 (702000)	/* KHz */
#define DVFS_LITTLE_SB_F07 (507000)	/* KHz */

#define DVFS_LITTLE_SB_F10 (1807000)	/* KHz */
#define DVFS_LITTLE_SB_F11 (1651000)	/* KHz */
#define DVFS_LITTLE_SB_F12 (1508000)	/* KHz */
#define DVFS_LITTLE_SB_F13 (1300000)	/* KHz */
#define DVFS_LITTLE_SB_F14 (1105000)	/* KHz */
#define DVFS_LITTLE_SB_F15 (1001000)	/* KHz */
#define DVFS_LITTLE_SB_F16 (702000)	/* KHz */
#define DVFS_LITTLE_SB_F17 (507000)	/* KHz */

#define DVFS_LITTLE_SB_F20 (1703000)	/* KHz */
#define DVFS_LITTLE_SB_F21 (1690000)	/* KHz */
#define DVFS_LITTLE_SB_F22 (1508000)	/* KHz */
#define DVFS_LITTLE_SB_F23 (1300000)	/* KHz */
#define DVFS_LITTLE_SB_F24 (1105000)	/* KHz */
#define DVFS_LITTLE_SB_F25 (1001000)	/* KHz */
#define DVFS_LITTLE_SB_F26 (702000)	/* KHz */
#define DVFS_LITTLE_SB_F27 (507000)	/* KHz */

#define DVFS_LITTLE_SB_V00 (1125)	/* mV */
#define DVFS_LITTLE_SB_V01 (1082)	/* mV */
#define DVFS_LITTLE_SB_V02 (1044)	/* mV */
#define DVFS_LITTLE_SB_V03 (1003)	/* mV */
#define DVFS_LITTLE_SB_V04 (965)	/* mV */
#define DVFS_LITTLE_SB_V05 (944)	/* mV */
#define DVFS_LITTLE_SB_V06 (886)	/* mV */
#define DVFS_LITTLE_SB_V07 (847)	/* mV */

#define DVFS_LITTLE_SB_V10 (1103)	/* mV */
#define DVFS_LITTLE_SB_V11 (1072)	/* mV */
#define DVFS_LITTLE_SB_V12 (1044)	/* mV */
#define DVFS_LITTLE_SB_V13 (1003)	/* mV */
#define DVFS_LITTLE_SB_V14 (965)	/* mV */
#define DVFS_LITTLE_SB_V15 (944)	/* mV */
#define DVFS_LITTLE_SB_V16 (886)	/* mV */
#define DVFS_LITTLE_SB_V17 (847)	/* mV */

#define DVFS_LITTLE_SB_V20 (1082)	/* mV */
#define DVFS_LITTLE_SB_V21 (1080)	/* mV */
#define DVFS_LITTLE_SB_V22 (1044)	/* mV */
#define DVFS_LITTLE_SB_V23 (1003)	/* mV */
#define DVFS_LITTLE_SB_V24 (965)	/* mV */
#define DVFS_LITTLE_SB_V25 (944)	/* mV */
#define DVFS_LITTLE_SB_V26 (886)	/* mV */
#define DVFS_LITTLE_SB_V27 (847)	/* mV */

/* FY combination */
#define DVFS_LITTLE_F0 (1573000)	/* KHz */
#define DVFS_LITTLE_F01 (1508000)	/* KHz */
#define DVFS_LITTLE_F1 (1456000)	/* KHz */
#define DVFS_LITTLE_F2 (1404000)	/* KHz */
#define DVFS_LITTLE_F3 (1300000)	/* KHz */
#define DVFS_LITTLE_F4 (1209000)	/* KHz */
#define DVFS_LITTLE_F5 (1105000)	/* KHz */
#define DVFS_LITTLE_F6 (1001000)	/* KHz */
#define DVFS_LITTLE_F7 (910000)	/* KHz */
#define DVFS_LITTLE_F8 (702000)	/* KHz */
#define DVFS_LITTLE_F9 (507000)	/* KHz */

#define DVFS_LITTLE_V0 (1125)	/* mV */
#define DVFS_LITTLE_V01 (1109)	/* mV */
#define DVFS_LITTLE_V1 (1096)	/* mV */
#define DVFS_LITTLE_V2 (1083)	/* mV */
#define DVFS_LITTLE_V3 (1057)	/* mV */
#define DVFS_LITTLE_V4 (1034)	/* mV */
#define DVFS_LITTLE_V5 (1009)	/* mV */
#define DVFS_LITTLE_V6 (983)	/* mV */
#define DVFS_LITTLE_V7 (960)	/* mV */
#define DVFS_LITTLE_V8 (908)	/* mV */
#define DVFS_LITTLE_V9 (859)	/* mV */

#define PWR_THRO_MODE_LBAT_1365MHZ	BIT(0)
#define PWR_THRO_MODE_BAT_OC_806MHZ	BIT(1)
#define PWR_THRO_MODE_BAT_PER_1365MHZ	BIT(2)

#define CPU_DVFS_OPPIDX_1365MHZ		2
#define CPU_DVFS_OPPIDX_806MHZ		6

/*
 * LOG
 */
/* #define USING_XLOG */

#define HEX_FMT "0x%08x"
#undef TAG

#define TAG     "[Power/cpufreq] "

#define cpufreq_err(fmt, args...)       \
	pr_err(TAG"[ERROR]"fmt, ##args)
#define cpufreq_warn(fmt, args...)      \
	pr_warn(TAG"[WARNING]"fmt, ##args)
#define cpufreq_info(fmt, args...)      \
	pr_warn(TAG""fmt, ##args)
#define cpufreq_dbg(fmt, args...)       \
	do {                                \
		if (func_lv_mask)           \
			cpufreq_info(fmt, ##args);     \
	} while (0)
#define cpufreq_ver(fmt, args...)       \
	do {                                \
		if (func_lv_mask)           \
			pr_debug(TAG""fmt, ##args);    \
	} while (0)

#define FUNC_LV_MODULE          BIT(0)	/* module, platform driver interface */
#define FUNC_LV_CPUFREQ         BIT(1)	/* cpufreq driver interface          */
#define FUNC_LV_API             BIT(2)	/* mt_cpufreq driver global function */
#define FUNC_LV_LOCAL           BIT(3)	/* mt_cpufreq driver lcaol function  */
#define FUNC_LV_HELP            BIT(4)	/* mt_cpufreq driver help function   */

unsigned int func_lv_mask = 0;	/* (FUNC_LV_MODULE | FUNC_LV_CPUFREQ | FUNC_LV_API | FUNC_LV_LOCAL | FUNC_LV_HELP); */

#if defined(CONFIG_CPU_DVFS_SHOWLOG)
#define FUNC_ENTER(lv)          do { if ((lv) & func_lv_mask) cpufreq_dbg(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv)    \
	do { if ((lv) & func_lv_mask) cpufreq_dbg("<< %s():%d\n", __func__, __LINE__); } while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif				/* CONFIG_CPU_DVFS_SHOWLOG */

/*
 * BIT Operation
 */
#define _BIT_(_bit_)                    (unsigned)(1 << (_bit_))
#define _BITS_(_bits_, _val_)   \
	((((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_)               (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * REG ACCESS
 */
#define cpufreq_read(addr)                  __raw_readl(addr)
#define cpufreq_write(addr, val)            mt_reg_sync_writel(val, addr)
#define cpufreq_write_mask(addr, mask, val) \
cpufreq_write(addr, (cpufreq_read(addr) & ~(_BITMASK_(mask))) | _BITS_(mask, val))

/*=============================================================*/
/* Local type definition                                       */
/*=============================================================*/


/*=============================================================*/
/* Local variable definition                                   */
/*=============================================================*/
enum chip_sw_ver chip_ver = CHIP_SW_VER_02;
struct mt_cpu_tlp_power_info *cpu_tlp_power_tbl = NULL;

/*=============================================================*/
/* Local function definition                                   */
/*=============================================================*/


/*=============================================================*/
/* Gobal function definition                                   */
/*=============================================================*/

/*
 * LOCK
 */
#if 0				/* spinlock */
/* TODO: FIXME, it would cause warning @ big because of i2c access with atomic operation */
static DEFINE_SPINLOCK(cpufreq_lock);
#define cpufreq_lock(flags) spin_lock_irqsave(&cpufreq_lock, flags)
#define cpufreq_unlock(flags) spin_unlock_irqrestore(&cpufreq_lock, flags)
#else				/* mutex */
DEFINE_MUTEX(cpufreq_mutex);
bool is_in_cpufreq = 0;
/* mt8173-TBD, mark spm_mcdi_wakeup_all_cores(), wait for it ready */
#define cpufreq_lock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_lock(&cpufreq_mutex); \
		is_in_cpufreq = 1;\
		spm_mcdi_wakeup_all_cores(); \
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
#define CPUFREQ_EFUSE_INDEX     (3)	/* TODO: confirm CPU efuse */

/* #define CPU_LEVEL_0             (0x0) */
#define CPU_LEVEL_1             (0x1)	/* 1.8G, 2.1G, SB */
#define CPU_LEVEL_2             (0x2)	/* 1.7G, 2.0G */
#define CPU_LEVEL_3             (0x3)	/* 1.6G, 1.9G */
#define CPU_LEVEL_4             (0x4)	/* 1.5G, 1.8G */
#define CPU_LEVEL_5             (0x5)	/* 1.4G,  */

#define CPU_LV_TO_OPP_IDX(lv)   ((lv)-1)	/* cpu_level to opp_idx */

static int system_boost;

static unsigned int read_efuse_speed(enum mt_cpu_dvfs_id id)
{				/* TODO: remove it latter */
	unsigned int efuse = 0;
	unsigned int lv = 0;

	if (id == MT_CPU_DVFS_BIG) {
		efuse = _GET_BITS_VAL_(2:0, get_devinfo_with_index(CPUFREQ_EFUSE_INDEX));
		switch (efuse) {
		case 0:
			lv = CPU_LEVEL_2;	/* default = sloane = [1.6G, 2.0G] */
#ifdef CONFIG_TB8173_P1
			lv = CPU_LEVEL_4;	/* p1 = [1.4G, 1.8G] */
#endif
#ifdef CONFIG_TB8173_P1_PLUS
			lv = CPU_LEVEL_2;	/* p1_plus = [1.6G, 2.0G], set to [1.8G, 2.1G] for SB IC */
#endif
#ifdef CONFIG_SND_SOC_MT8173_SLOANE
			lv = CPU_LEVEL_2;	/* sloane = [1.6G, 2.0G] */
#endif
#ifdef CONFIG_BX8173_P12
			lv = CPU_LEVEL_4;	/* bx8173p12 = [1.4G, 1.8G] */
#endif
			if (strcmp(CONFIG_ARCH_MTK_PROJECT, "tb8173p1_plus") == 0)
				lv = CPU_LEVEL_2;	/* p1_plus = [1.6G, 2.0G], set to [1.8G, 2.1G] for SB IC */

			break;
		case 1:
		case 2:
		case 3:
		case 4:
			lv = CPU_LEVEL_1;	/* 2.1G, SB */
			break;
		case 5:
			lv = CPU_LEVEL_2;	/* 2.0G */
			break;
		case 6:
			lv = CPU_LEVEL_4;	/* 1.8G */
			break;
		case 7:
		default:
			cpufreq_err
			    ("No suitable DVFS table, set to default CPU[%d] level! efuse=0x%x\n",
			     id, efuse);
			lv = CPU_LEVEL_4;
		}
		if (mt_get_chip_sw_ver() == CHIP_SW_VER_02)
			lv = CPU_LEVEL_5;	/* 1.6G */
	} else {
		efuse = _GET_BITS_VAL_(29:28, get_devinfo_with_index(CPUFREQ_EFUSE_INDEX));
		switch (efuse) {
		case 0:
			lv = CPU_LEVEL_3;	/* default = sloane = [1.6G, 2.0G] */
#ifdef CONFIG_TB8173_P1
			lv = CPU_LEVEL_5;	/* p1 = [1.4G, 1.8G] */
#endif
#ifdef CONFIG_TB8173_P1_PLUS
			lv = CPU_LEVEL_3;	/* p1_plus = [1.6G, 2.0G], set to [1.8G, 2.1G] for SB IC */
#endif
#ifdef CONFIG_SND_SOC_MT8173_SLOANE
			lv = CPU_LEVEL_3;	/* sloane = [1.6G, 2.0G] */
#endif
#ifdef CONFIG_BX8173_P12
			lv = CPU_LEVEL_5;	/* bx8173p12 = [1.4G, 1.8G] */
#endif
			if (strcmp(CONFIG_ARCH_MTK_PROJECT, "tb8173p1_plus") == 0)
				lv = CPU_LEVEL_3;	/* p1_plus = [1.6G, 2.0G], set to [1.8G, 2.1G] for SB IC */
			break;
		case 1:
			lv = CPU_LEVEL_2;	/* 1.7G */
			break;

		case 2:
			lv = CPU_LEVEL_3;	/* 1.6G */
			break;

		case 3:
			lv = CPU_LEVEL_5;	/* 1.4G */
			break;

		default:
			cpufreq_err
			    ("No suitable DVFS table, set to default CPU[%d] level! efuse=0x%x\n",
			     id, efuse);
			lv = CPU_LEVEL_5;
		}
	}

	return lv;
}

/*
 * PMIC_WRAP
 */
/* TODO: defined @ pmic head file??? */
#define VOLT_TO_PMIC_VAL(volt)  ((((volt) - 700) * 100 + 625 - 1) / 625)
#define PMIC_VAL_TO_VOLT(pmic)  (((pmic) * 625) / 100 + 700)	/* (((pmic) * 625 + 100 - 1) / 100 + 700) */

#define VOLT_TO_EXTBUCK_VAL(volt) (((((volt) - 300) + 9) / 10) & 0x7F)
#define EXTBUCK_VAL_TO_VOLT(val)  (300 + ((val) & 0x7F) * 10)

#define PMIC_WRAP_DVFS_ADR0     (PWRAP_BASE_ADDR + 0x0E8)
#define PMIC_WRAP_DVFS_WDATA0   (PWRAP_BASE_ADDR + 0x0EC)
#define PMIC_WRAP_DVFS_ADR1     (PWRAP_BASE_ADDR + 0x0F0)
#define PMIC_WRAP_DVFS_WDATA1   (PWRAP_BASE_ADDR + 0x0F4)
#define PMIC_WRAP_DVFS_ADR2     (PWRAP_BASE_ADDR + 0x0F8)
#define PMIC_WRAP_DVFS_WDATA2   (PWRAP_BASE_ADDR + 0x0FC)
#define PMIC_WRAP_DVFS_ADR3     (PWRAP_BASE_ADDR + 0x100)
#define PMIC_WRAP_DVFS_WDATA3   (PWRAP_BASE_ADDR + 0x104)
#define PMIC_WRAP_DVFS_ADR4     (PWRAP_BASE_ADDR + 0x108)
#define PMIC_WRAP_DVFS_WDATA4   (PWRAP_BASE_ADDR + 0x10C)
#define PMIC_WRAP_DVFS_ADR5     (PWRAP_BASE_ADDR + 0x110)
#define PMIC_WRAP_DVFS_WDATA5   (PWRAP_BASE_ADDR + 0x114)
#define PMIC_WRAP_DVFS_ADR6     (PWRAP_BASE_ADDR + 0x118)
#define PMIC_WRAP_DVFS_WDATA6   (PWRAP_BASE_ADDR + 0x11C)
#define PMIC_WRAP_DVFS_ADR7     (PWRAP_BASE_ADDR + 0x120)
#define PMIC_WRAP_DVFS_WDATA7   (PWRAP_BASE_ADDR + 0x124)

/* PMIC ADDR ...... TODO: include other head file */
/* mt6397 */
#define PMIC_ADDR_VPROC_CA53_VOSEL_ON      0x0228	/* [6:0]                     */
#define PMIC_ADDR_VPROC_CA53_VOSEL_SLEEP   0x022A	/* [6:0]                     */
#define PMIC_ADDR_VPROC_CA53_EN            0x0222	/* [0] (shared with others)  */
#define PMIC_ADDR_VSRAM_CA53_EN            0x0248	/* [0] (shared with others) */

#define PMIC_ADDR_VSRAM_CA57_VOSEL_ON      0x0360	/* [6:0]                     */
#define PMIC_ADDR_VSRAM_CA57_VOSEL_SLEEP   0x0362	/* [6:0]                     */
#define PMIC_ADDR_VSRAM_CA57_EN            0x035A	/* [0] (shared with others) */

#define PMIC_ADDR_VCORE_VOSEL_ON          0x027A	/* [6:0]                     */
#define PMIC_ADDR_VCORE_VOSEL_SLEEP       0x027C	/* [6:0]                     */

/* mt6595...need to remove TBD */
#define PMIC_ADDR_VGPU_VOSEL_ON           0x02B0	/* [6:0]                     */

#define NR_PMIC_WRAP_CMD 8	/* num of pmic wrap cmd (fixed value) */

/* TODO: replace predefined reg_base with address from device tree */
#define ARMCA15PLL_CON0		(APMIXED_BASE + 0x200)
#define ARMCA15PLL_CON1		(APMIXED_BASE + 0x204)
#define ARMCA15PLL_CON2		(APMIXED_BASE + 0x208)
#define ARMCA7PLL_CON0		(APMIXED_BASE + 0x210)
#define ARMCA7PLL_CON1		(APMIXED_BASE + 0x214)
#define ARMCA7PLL_CON2		(APMIXED_BASE + 0x218)

struct pmic_wrap_cmd {
	unsigned long cmd_addr;
	unsigned long cmd_wdata;
};

struct pmic_wrap_setting {
	enum pmic_wrap_phase_id phase;
	struct pmic_wrap_cmd addr[NR_PMIC_WRAP_CMD];
	struct {
		struct {
			unsigned long cmd_addr;
			unsigned long cmd_wdata;
		} _[NR_PMIC_WRAP_CMD];
		const int nr_idx;
	} set[NR_PMIC_WRAP_PHASE];
};

static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE,	/* invalid setting for init */
#if 0
	.addr = {
		 {PMIC_WRAP_DVFS_ADR0, PMIC_WRAP_DVFS_WDATA0,},
		 {PMIC_WRAP_DVFS_ADR1, PMIC_WRAP_DVFS_WDATA1,},
		 {PMIC_WRAP_DVFS_ADR2, PMIC_WRAP_DVFS_WDATA2,},
		 {PMIC_WRAP_DVFS_ADR3, PMIC_WRAP_DVFS_WDATA3,},
		 {PMIC_WRAP_DVFS_ADR4, PMIC_WRAP_DVFS_WDATA4,},
		 {PMIC_WRAP_DVFS_ADR5, PMIC_WRAP_DVFS_WDATA5,},
		 {PMIC_WRAP_DVFS_ADR6, PMIC_WRAP_DVFS_WDATA6,},
		 {PMIC_WRAP_DVFS_ADR7, PMIC_WRAP_DVFS_WDATA7,},
		 },
#else
	.addr = {{0, 0} },
#endif

	.set[PMIC_WRAP_PHASE_NORMAL] = {
					._[IDX_NM_VSRAM_CA15L] = {PMIC_ADDR_VSRAM_CA57_VOSEL_ON,
								  VOLT_TO_PMIC_VAL(DVFS_BIG_V0),},
					._[IDX_NM_VPROC_CA7] = {PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								VOLT_TO_PMIC_VAL(DVFS_LITTLE_V0),},
					._[IDX_NM_VGPU] = {PMIC_ADDR_VGPU_VOSEL_ON,
							   VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VGPU),},
					._[IDX_NM_VCORE] = {PMIC_ADDR_VCORE_VOSEL_ON,
							    VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VCORE),},
					.nr_idx = NR_IDX_NM,
					},

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
					  /* Barry Chang firmware won't work, index 2, 3, 6, 7 are dummy setting */
					  ._[IDX_DI_VPROC_CA7_NORMAL] = {
									 PMIC_ADDR_VPROC_CA53_VOSEL_ON,
									 VOLT_TO_PMIC_VAL(1000),},
					  ._[IDX_DI_VPROC_CA7_SLEEP] = {
									PMIC_ADDR_VPROC_CA53_VOSEL_ON,
									VOLT_TO_PMIC_VAL(800),},
					  ._[IDX_DI_VSRAM_CA7_FAST_TRSN_EN] = {
									       PMIC_ADDR_VCORE_VOSEL_ON,
									       VOLT_TO_PMIC_VAL
									       (1125),},
					  ._[IDX_DI_VSRAM_CA7_FAST_TRSN_DIS] = {
										PMIC_ADDR_VCORE_VOSEL_ON,
										VOLT_TO_PMIC_VAL
										(900),},
					  ._[IDX_DI_VCORE_NORMAL] = {
								     PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								     VOLT_TO_PMIC_VAL(1000),},
					  ._[IDX_DI_VCORE_SLEEP] = {
								    PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								    VOLT_TO_PMIC_VAL(800),},
					  ._[IDX_DI_VCORE_PDN_NORMAL] = {
									 PMIC_ADDR_VCORE_VOSEL_ON,
									 VOLT_TO_PMIC_VAL(1125),},
					  ._[IDX_DI_VCORE_PDN_SLEEP] = {
									PMIC_ADDR_VCORE_VOSEL_ON,
									VOLT_TO_PMIC_VAL(900),},
					  .nr_idx = NR_IDX_DI,
					  },

	.set[PMIC_WRAP_PHASE_SODI] = {
				      /* power off: index 7,3. power on: index 2,6 */
				      ._[IDX_SO_VSRAM_CA15L_NORMAL] = {
								       PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								       VOLT_TO_PMIC_VAL(1000),},
				      ._[IDX_SO_VSRAM_CA15L_SLEEP] = {
								      PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								      VOLT_TO_PMIC_VAL(800),},
				      ._[IDX_SO_VPROC_CA7_NORMAL] = {
								     PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								     VOLT_TO_PMIC_VAL(1125),},
				      ._[IDX_SO_VPROC_CA7_SLEEP] = {
								    PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								    VOLT_TO_PMIC_VAL(700),},
				      ._[IDX_SO_VCORE_NORMAL] = {
								 PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								 VOLT_TO_PMIC_VAL(1000),},
				      ._[IDX_SO_VCORE_SLEEP] = {
								PMIC_ADDR_VPROC_CA53_VOSEL_ON,
								VOLT_TO_PMIC_VAL(800),},
				      ._[IDX_SO_VSRAM_CA7_FAST_TRSN_EN] = {
									   PMIC_ADDR_VCORE_VOSEL_ON,
									   VOLT_TO_PMIC_VAL(1125),},
				      ._[IDX_SO_VSRAM_CA7_FAST_TRSN_DIS] = {
									    PMIC_ADDR_VCORE_VOSEL_ON,
									    VOLT_TO_PMIC_VAL
									    (1125),},

				      .nr_idx = NR_IDX_SO,
				      },
};

#if 1				/* spinlock */
static DEFINE_SPINLOCK(pmic_wrap_lock);
#define pmic_wrap_lock(flags) spin_lock_irqsave(&pmic_wrap_lock, flags)
#define pmic_wrap_unlock(flags) spin_unlock_irqrestore(&pmic_wrap_lock, flags)
#else				/* mutex */
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

/* (early-)suspend / (late-)resume */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void _mt_cpufreq_early_suspend(struct early_suspend *h);
static void _mt_cpufreq_late_resume(struct early_suspend *h);
#else
static int _mt_cpufreq_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif


static int _spm_dvfs_ctrl_volt(u32 value)
{
#define MAX_RETRY_COUNT (100)

	u32 ap_dvfs_con;
	int retry = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));	/* TODO: FIXME */

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

void _mt_cpufreq_pmic_table_init(void)
{
	struct pmic_wrap_cmd pwrap_cmd_default[NR_PMIC_WRAP_CMD] = {
		{PMIC_WRAP_DVFS_ADR0, PMIC_WRAP_DVFS_WDATA0,},
		{PMIC_WRAP_DVFS_ADR1, PMIC_WRAP_DVFS_WDATA1,},
		{PMIC_WRAP_DVFS_ADR2, PMIC_WRAP_DVFS_WDATA2,},
		{PMIC_WRAP_DVFS_ADR3, PMIC_WRAP_DVFS_WDATA3,},
		{PMIC_WRAP_DVFS_ADR4, PMIC_WRAP_DVFS_WDATA4,},
		{PMIC_WRAP_DVFS_ADR5, PMIC_WRAP_DVFS_WDATA5,},
		{PMIC_WRAP_DVFS_ADR6, PMIC_WRAP_DVFS_WDATA6,},
		{PMIC_WRAP_DVFS_ADR7, PMIC_WRAP_DVFS_WDATA7,},
	};

	FUNC_ENTER(FUNC_LV_HELP);

	memcpy(pw.addr, pwrap_cmd_default, sizeof(pwrap_cmd_default));

	FUNC_EXIT(FUNC_LV_HELP);
}

void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase)
{
	int i;
	unsigned long flags;
	uint32_t rdata = 0;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);

	if (pw.addr[0].cmd_addr == 0) {
		cpufreq_warn("pmic table not initialized\n");
		_mt_cpufreq_pmic_table_init();
	}
#if 0				/* TODO: FIXME, check IPO-H case */

	if (pw.phase == phase)
		return;

#endif

	pmic_wrap_lock(flags);

	pw.phase = phase;

	if (phase == PMIC_WRAP_PHASE_SODI) {
		rdata = regulator_get_voltage(reg_vpca53) / 1000;
		pw.set[phase]._[IDX_SO_VPROC_CA7_NORMAL].cmd_wdata = VOLT_TO_PMIC_VAL(rdata);
	}

	for (i = 0; i < pw.set[phase].nr_idx; i++) {
		cpufreq_write(pw.addr[i].cmd_addr, pw.set[phase]._[i].cmd_addr);
		cpufreq_write(pw.addr[i].cmd_wdata, pw.set[phase]._[i].cmd_wdata);
	}

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_set_pmic_phase);

void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata)
{				/* just set wdata value */
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

void mt_cpufreq_apply_pmic_cmd(int idx)
{				/* kick spm */
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
cpuVoltsampler_func g_pCpuVoltSampler = NULL;

void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB)
{
	g_pCpuVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_setvolt_registerCB);

/* SDIO */
unsigned int mt_get_cur_volt_vcore(void)
{
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_LOCAL);

	rdata = regulator_get_voltage(reg_vcore_backup) / 1000;

	rdata = PMIC_VAL_TO_VOLT(rdata);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata;		/* mv: vproc */
}

void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled)
{
	/* empty function */
}

void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt)
{				/* unit: mv x 100 */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE, VOLT_TO_PMIC_VAL(volt / 100));
}

unsigned int mt_vcore_dvfs_volt_get_by_sdio(void)
{
	return mt_get_cur_volt_vcore() * 1000;
}

/*
 * mt_cpufreq driver
 */
#define MAX_CPU_NUM 2		/* for limited_max_ncpu */

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
	/* set power limit by thermal *//* TODO: sync with mt_cpufreq_thermal_protect() */
	void (*protect)(struct mt_cpu_dvfs *p, unsigned int limited_power);
	unsigned int (*get_temp)(struct mt_cpu_dvfs *p);	/* return temperature */
	int (*setup_power_table)(struct mt_cpu_dvfs *p);

	/* for freq change (PLL/MUX) */
	unsigned int (*get_cur_phy_freq)(struct mt_cpu_dvfs *p);	/* return (physical) freq (KHz) */
	/* set freq  */
	void (*set_cur_freq)(struct mt_cpu_dvfs *p, unsigned int cur_khz,
			      unsigned int target_khz);

	/* for volt change (PMICWRAP/extBuck) */
	unsigned int (*get_cur_volt)(struct mt_cpu_dvfs *p);	/* return volt (mV) */
	int (*set_cur_volt)(struct mt_cpu_dvfs *p, unsigned int mv);	/* set volt, return 0 (success), -1 (fail) */
};

struct mt_cpu_dvfs {
	const char *name;
	unsigned int cpu_id;	/* for cpufreq */
	struct mt_cpu_dvfs_ops *ops;

	/* opp (freq) table */
	struct mt_cpu_freq_info *opp_tbl;	/* OPP table */
	int nr_opp_tbl;		/* size for OPP table */
	int idx_opp_tbl;	/* current OPP idx */
	int idx_normal_max_opp;	/* idx for normal max OPP */
	int idx_opp_tbl_for_late_resume;	/* keep the setting for late resume */
	int idx_opp_tbl_for_pwr_thro;	/* keep the setting for power throttling */

	struct cpufreq_frequency_table *freq_tbl_for_cpufreq;	/* freq table for cpufreq */

	/* power table */
	struct mt_cpu_power_info *power_tbl;
	unsigned int nr_power_tbl;

	/* enable/disable DVFS function */
	int dvfs_disable_count;
	bool cpufreq_pause;
	bool dvfs_disable_by_ptpod;
	/* TODO: rename it to dvfs_disable_by_early_suspend */
	/* bool limit_max_freq_early_suspend; */
	bool dvfs_disable_by_early_suspend;
	bool is_fixed_freq;	/* TODO: FIXME */

	/* limit for thermal */
	unsigned int limited_max_ncpu;
	unsigned int limited_max_freq;
	/* unsigned int limited_min_freq; ... TODO: remove it??? */

	unsigned int thermal_protect_limited_power;

	/* limit for HEVC (via. sysfs) */
	unsigned int limited_freq_by_hevc;

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

static struct mt_cpu_dvfs cpu_dvfs[] = {	/* TODO: FIXME, big/LITTLE exclusive, NR_MT_CPU_DVFS */
	[MT_CPU_DVFS_LITTLE] = {
				.name = __stringify(MT_CPU_DVFS_LITTLE),
				.cpu_id = MT_CPU_DVFS_LITTLE,	/* TODO: FIXME */
				.ops = &little_ops,

				.over_max_cpu = 4,
				.ptpod_temperature_limit_1 = 110000,
				.ptpod_temperature_limit_2 = 120000,
				.ptpod_temperature_time_1 = 1,
				.ptpod_temperature_time_2 = 4,
				.pre_online_cpu = 0,
				.pre_freq = 0,
				.downgrade_freq = 0,
				.downgrade_freq_counter = 0,
				.downgrade_freq_counter_return = 0,
				.downgrade_freq_counter_limit = 0,
				.downgrade_freq_counter_return_limit = 0,

				.ramp_down_count_const = RAMP_DOWN_TIMES,

				.turbo_mode = 1,

				.idx_opp_tbl_for_pwr_thro = -1,
				},

	[MT_CPU_DVFS_BIG] = {
			     .name = __stringify(MT_CPU_DVFS_BIG),
			     .cpu_id = MT_CPU_DVFS_BIG,	/* TODO: FIXME */
			     .ops = &big_ops,

			     .over_max_cpu = 4,
			     .ptpod_temperature_limit_1 = 110000,
			     .ptpod_temperature_limit_2 = 120000,
			     .ptpod_temperature_time_1 = 1,
			     .ptpod_temperature_time_2 = 4,
			     .pre_online_cpu = 0,
			     .pre_freq = 0,
			     .downgrade_freq = 0,
			     .downgrade_freq_counter = 0,
			     .downgrade_freq_counter_return = 0,
			     .downgrade_freq_counter_limit = 0,
			     .downgrade_freq_counter_return_limit = 0,

			     .ramp_down_count_const = RAMP_DOWN_TIMES,

			     .turbo_mode = 1,

			     .idx_opp_tbl_for_pwr_thro = -1,
			     },
};

#define for_each_cpu_dvfs(i, p)                 for (i = 0, p = cpu_dvfs; i < NR_MT_CPU_DVFS; i++, p = &cpu_dvfs[i])
#define cpu_dvfs_is(p, id)                      (p == &cpu_dvfs[id])
#define cpu_dvfs_is_available(p)               (p->opp_tbl)
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

#define NR_MAX_OPP_TBL  8	/* TODO: refere to PTP-OD */
#define NR_MAX_CPU      4	/* TODO: one cluster, any kernel define for this - CONFIG_NR_CPU??? */

			 /* LITTLE CPU LEVEL 0 *//* 1.8G, SB */
static struct mt_cpu_freq_info opp_tbl_little_e1_0[] = {
	OP(DVFS_LITTLE_SB_F10, DVFS_LITTLE_SB_V10),
	OP(DVFS_LITTLE_SB_F11, DVFS_LITTLE_SB_V11),
	OP(DVFS_LITTLE_SB_F12, DVFS_LITTLE_SB_V12),
	OP(DVFS_LITTLE_SB_F13, DVFS_LITTLE_SB_V13),
	OP(DVFS_LITTLE_SB_F14, DVFS_LITTLE_SB_V14),
	OP(DVFS_LITTLE_SB_F15, DVFS_LITTLE_SB_V15),
	OP(DVFS_LITTLE_SB_F16, DVFS_LITTLE_SB_V16),
	OP(DVFS_LITTLE_SB_F17, DVFS_LITTLE_SB_V17),
};

			 /* LITTLE CPU LEVEL 0 *//* 1.7G, SB */
static struct mt_cpu_freq_info opp_tbl_little_e1_1[] = {
	OP(DVFS_LITTLE_SB_F20, DVFS_LITTLE_SB_V20),
	OP(DVFS_LITTLE_SB_F21, DVFS_LITTLE_SB_V21),
	OP(DVFS_LITTLE_SB_F22, DVFS_LITTLE_SB_V22),
	OP(DVFS_LITTLE_SB_F23, DVFS_LITTLE_SB_V23),
	OP(DVFS_LITTLE_SB_F24, DVFS_LITTLE_SB_V24),
	OP(DVFS_LITTLE_SB_F25, DVFS_LITTLE_SB_V25),
	OP(DVFS_LITTLE_SB_F26, DVFS_LITTLE_SB_V26),
	OP(DVFS_LITTLE_SB_F27, DVFS_LITTLE_SB_V27),
};

			 /* LITTLE CPU LEVEL 0 *//* 1.6G */
static struct mt_cpu_freq_info opp_tbl_little_e1_2[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F8, DVFS_LITTLE_V8),
	OP(DVFS_LITTLE_F9, DVFS_LITTLE_V9),
};

			 /* LITTLE CPU LEVEL 1 *//* 1.5G */
static struct mt_cpu_freq_info opp_tbl_little_e1_3[] = {
	OP(DVFS_LITTLE_F01, DVFS_LITTLE_V01),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F8, DVFS_LITTLE_V8),
	OP(DVFS_LITTLE_F9, DVFS_LITTLE_V9),
};

			 /* LITTLE CPU LEVEL 1 *//* 1.40G */
static struct mt_cpu_freq_info opp_tbl_little_e1_4[] = {
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
	OP(DVFS_LITTLE_F8, DVFS_LITTLE_V8),
	OP(DVFS_LITTLE_F9, DVFS_LITTLE_V9),
};

		      /* big CPU LEVEL 0 *//* 2.1G, SB */
static struct mt_cpu_freq_info opp_tbl_big_e1_0[] = {
	OP(DVFS_BIG_SB_F10, DVFS_BIG_SB_V10),
	OP(DVFS_BIG_SB_F11, DVFS_BIG_SB_V11),
	OP(DVFS_BIG_SB_F12, DVFS_BIG_SB_V12),
	OP(DVFS_BIG_SB_F13, DVFS_BIG_SB_V13),
	OP(DVFS_BIG_SB_F14, DVFS_BIG_SB_V14),
	OP(DVFS_BIG_SB_F15, DVFS_BIG_SB_V15),
	OP(DVFS_BIG_SB_F16, DVFS_BIG_SB_V16),
	OP(DVFS_BIG_SB_F17, DVFS_BIG_SB_V17),
};

		      /* big CPU LEVEL 0 *//* 2G */
static struct mt_cpu_freq_info opp_tbl_big_e1_1[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
	OP(DVFS_BIG_F8, DVFS_BIG_V8),
	OP(DVFS_BIG_F10, DVFS_BIG_V10),
	OP(DVFS_BIG_F11, DVFS_BIG_V11),
};

		      /* big CPU LEVEL 1 *//* 1.9G */
static struct mt_cpu_freq_info opp_tbl_big_e1_2[] = {
	OP(DVFS_BIG_F01, DVFS_BIG_V01),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F8, DVFS_BIG_V8),
	OP(DVFS_BIG_F9, DVFS_BIG_V9),
	OP(DVFS_BIG_F10, DVFS_BIG_V10),
	OP(DVFS_BIG_F11, DVFS_BIG_V11),
};

		      /* big CPU LEVEL 1 *//* 1.8G */
static struct mt_cpu_freq_info opp_tbl_big_e1_3[] = {
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F8, DVFS_BIG_V8),
	OP(DVFS_BIG_F9, DVFS_BIG_V9),
	OP(DVFS_BIG_F10, DVFS_BIG_V10),
	OP(DVFS_BIG_F11, DVFS_BIG_V11),
};

		      /* big CPU LEVEL 1 *//* 1.6G */
static struct mt_cpu_freq_info opp_tbl_big_e1_4[] = {
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F8, DVFS_BIG_V8),
	OP(DVFS_BIG_F9, DVFS_BIG_V9),
	OP(DVFS_BIG_F10, DVFS_BIG_V10),
	OP(DVFS_BIG_F11, DVFS_BIG_V11),
};

struct opp_tbl_info {
	struct mt_cpu_freq_info *const opp_tbl;
	const int size;
};

static struct opp_tbl_info opp_tbls_little[] = {
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = {opp_tbl_little_e1_0, ARRAY_SIZE(opp_tbl_little_e1_0)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = {opp_tbl_little_e1_1, ARRAY_SIZE(opp_tbl_little_e1_1)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_3)] = {opp_tbl_little_e1_2, ARRAY_SIZE(opp_tbl_little_e1_2)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_4)] = {opp_tbl_little_e1_3, ARRAY_SIZE(opp_tbl_little_e1_3)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_5)] = {opp_tbl_little_e1_4, ARRAY_SIZE(opp_tbl_little_e1_4)},
};

static struct opp_tbl_info opp_tbls_big[] = {
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = {opp_tbl_big_e1_0, ARRAY_SIZE(opp_tbl_big_e1_0)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = {opp_tbl_big_e1_1, ARRAY_SIZE(opp_tbl_big_e1_1)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_3)] = {opp_tbl_big_e1_2, ARRAY_SIZE(opp_tbl_big_e1_2)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_4)] = {opp_tbl_big_e1_3, ARRAY_SIZE(opp_tbl_big_e1_3)},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_5)] = {opp_tbl_big_e1_4, ARRAY_SIZE(opp_tbl_big_e1_4)},
};

/* for freq change (PLL/MUX) */

#define PLL_FREQ_STEP		(13000)	/* KHz */

/* #define PLL_MAX_FREQ            (1989000) *//* KHz *//* TODO: check max freq */
#define PLL_MIN_FREQ            (130000)	/* KHz */
#define PLL_DIV1_FREQ           (1001000)	/* KHz */
#define PLL_DIV2_FREQ           (520000)	/* KHz */
#define PLL_DIV4_FREQ           (260000)	/* KHz */
#define PLL_DIV8_FREQ           (PLL_MIN_FREQ)	/* KHz */

#define DDS_DIV1_FREQ           (0x0009A000)	/* 1001MHz */
#define DDS_DIV2_FREQ           (0x010A0000)	/* 520MHz  */
#define DDS_DIV4_FREQ           (0x020A0000)	/* 260MHz  */
#define DDS_DIV8_FREQ           (0x030A0000)	/* 130MHz  */

/* turbo mode */

#define TURBO_MODE_BOUNDARY_CPU_NUM	2

/* idx sort by temp from low to high */
enum turbo_mode {
	TURBO_MODE_2,
	TURBO_MODE_1,
	TURBO_MODE_NONE,

	NR_TURBO_MODE,
};

/* idx sort by temp from low to high */
struct turbo_mode_cfg {
	int temp;		/* degree x 1000 */
	int freq_delta;		/* percentage    */
	int volt_delta;		/* mv            */
} turbo_mode_cfg[] = {
	[TURBO_MODE_2] = {
	.temp = 65000, .freq_delta = 10, .volt_delta = 40,}
	, [TURBO_MODE_1] = {
	.temp = 85000, .freq_delta = 5, .volt_delta = 20,}
	, [TURBO_MODE_NONE] = {
.temp = 125000, .freq_delta = 0, .volt_delta = 0,},};

#define TURBO_MODE_FREQ(mode, freq) (((freq * (100 + turbo_mode_cfg[mode].freq_delta)) \
	/ PLL_FREQ_STEP) / 100 * PLL_FREQ_STEP)
#define TURBO_MODE_VOLT(mode, volt) (volt + turbo_mode_cfg[mode].volt_delta)

static int num_online_cpus_little_delta;
static int num_online_cpus_big_delta;

static enum turbo_mode get_turbo_mode(struct mt_cpu_dvfs *p, unsigned int target_khz)
{
	enum turbo_mode mode = TURBO_MODE_NONE;
	int temp =
	    tscpu_get_bL_temp(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? THERMAL_BANK0 : THERMAL_BANK1);
	int num_online_cpus_little = 0, num_online_cpus_big = 0;
	int i;

	if (-1 == hps_get_num_online_cpus(&num_online_cpus_little, &num_online_cpus_big)) {
		num_online_cpus_little = 2;
		num_online_cpus_big = 2;
	} else {
		num_online_cpus_little += num_online_cpus_little_delta;
		num_online_cpus_big += num_online_cpus_big_delta;
	}

	if (p->turbo_mode && target_khz == cpu_dvfs_get_freq_by_idx(p, 0)
	    && ((cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? num_online_cpus_little : num_online_cpus_big))
	    <= TURBO_MODE_BOUNDARY_CPU_NUM) {
		for (i = 0; i < NR_TURBO_MODE; i++) {
			if (temp < turbo_mode_cfg[i].temp) {
				mode = i;
				break;
			}
		}
	}

	if (TURBO_MODE_NONE != mode) {
		cpufreq_ver("%s(), mode = %d, temp = %d, target_khz = %d (%d)\n"
			, __func__, mode, temp, target_khz, TURBO_MODE_FREQ(mode, target_khz));
		cpufreq_ver("num_online_cpus_little = %d, num_online_cpus_big = %d\n",
			num_online_cpus_little, num_online_cpus_big);
	}

	return mode;
}

/* for PTP-OD */

static int _set_cur_volt_locked(struct mt_cpu_dvfs *p, unsigned int mv)
{
	int ret = -1;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_available(p) || p->cpufreq_pause) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 0;
	}
#if 0				/* 95 designer ask, mt8173 DE not request */
	/* update for deep idle */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
				cpu_dvfs_is(p,
					    MT_CPU_DVFS_LITTLE) ? IDX_DI_VPROC_CA7_NORMAL :
				IDX_DI_VSRAM_CA15L_NORMAL,
				VOLT_TO_PMIC_VAL(cpu_dvfs_get_volt_by_idx(p, p->idx_normal_max_opp))
	    );

	/* update for suspend */
	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)
	    && pw.set[PMIC_WRAP_PHASE_SUSPEND]._[IDX_SP_VSRAM_CA15L_PWR_ON].cmd_addr ==
	    PMIC_ADDR_VSRAM_CA57_VOSEL_ON)
		mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_SUSPEND, IDX_SP_VSRAM_CA15L_PWR_ON,
					VOLT_TO_PMIC_VAL(mv + NORMAL_DIFF_VRSAM_VPROC)
		    );
#endif

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

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 0;
	}

	cpufreq_lock(flags);	/* TODO: is it necessary??? */

	/* restore to default volt */
	for (i = 0; i < p->nr_opp_tbl; i++)
		p->opp_tbl[i].cpufreq_volt = p->opp_tbl[i].cpufreq_volt_org;

	/* set volt */
	ret = _set_cur_volt_locked(p,
				   TURBO_MODE_VOLT(get_turbo_mode(p, cpu_dvfs_get_cur_freq(p)),
						   cpu_dvfs_get_cur_volt(p)
				   )
	    );

	cpufreq_unlock(flags);	/* TODO: is it necessary??? */

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

unsigned int mt_cpufreq_max_frequency_by_DVS(enum mt_cpu_dvfs_id id, int idx)
{				/* TODO: rename to mt_cpufreq_get_freq_by_idx() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_available(p) || idx >= p->nr_opp_tbl) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_freq_by_idx(p, idx);
}
EXPORT_SYMBOL(mt_cpufreq_max_frequency_by_DVS);

int mt_cpufreq_voltage_set_by_ptpod(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl)
{				/* TODO: rename to mt_cpufreq_update_volt() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned long flags;
	int i;
	int ret = -1;

	FUNC_ENTER(FUNC_LV_API);

#if 0				/* TODO: remove it latter */

	if (id != 0)
		return 0;	/* TODO: FIXME, just for E1 */

#endif				/* TODO: remove it latter */

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_available(p)) {
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

void mt_cpufreq_return_default_DVS_by_ptpod(enum mt_cpu_dvfs_id id)
{				/* TODO: rename to mt_cpufreq_restore_default_volt() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_available(p)) {
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

#if 0				/* method 1 */
	static const unsigned int pll_vcodivsel_map[2] = { 1, 2 };
	static const unsigned int pll_prediv_map[4] = { 1, 2, 4, 4 };
	static const unsigned int pll_posdiv_map[8] = { 1, 2, 4, 8, 16, 16, 16, 16 };
	static const unsigned int pll_fbksel_map[4] = { 1, 2, 4, 4 };
	static const unsigned int pll_n_info_map[14] = {	/* assume fin = 26MHz */
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

unsigned int posdiv = _GET_BITS_VAL_(26:24, con1);
	unsigned int vcodivsel = 0;	/* _GET_BITS_VAL_(19 : 19, con0); *//* XXX: always zero */
	unsigned int prediv = 0;	/* _GET_BITS_VAL_(5 : 4, con0);   *//* XXX: always zero */
unsigned int n_info_i = _GET_BITS_VAL_(20:14, con1);
unsigned int n_info_f = _GET_BITS_VAL_(13:0, con1);

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

			if (!(n_info_f & (mask - 1)))	/* could break early if remaining bits are 0 */
				break;
		}
	}

	vco_f = (vco_f + 1000000 / 2) / 1000000;	/* round up */

	freq = (vco_i + vco_f) * 1000 * vcodivsel / prediv / posdiv;	/* KHz */
#else				/* method 2 */
con1 &= _BITMASK_(26:0);

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
		/* BUG(); *//* TODO: FIXME */
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return freq;		/* TODO: adjust by ptp level??? */
}

static unsigned int get_cur_phy_freq(struct mt_cpu_dvfs *p)
{
	unsigned int con1;
	unsigned int ckdiv1;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	con1 = cpufreq_read(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_CON1 : ARMCA15PLL_CON1);

	ckdiv1 = cpufreq_read(TOP_CKDIV1);
ckdiv1 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? _GET_BITS_VAL_(4 : 0, ckdiv1) : _GET_BITS_VAL_(9:5, ckdiv1);

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

static unsigned int _cpu_dds_calc(unsigned int khz)
{				/* XXX: NOT OK FOR 1007.5MHz */
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
unsigned int mask = _BITMASK_(1:0);

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(sel >= NR_TOP_CKMUXSEL);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)) {
		sel <<= 2;
		mask <<= 2;	/* _BITMASK_(3 : 2) */
	}

	cpufreq_write(TOP_CKMUXSEL, (val & ~mask) | sel);

	FUNC_EXIT(FUNC_LV_HELP);
}

static enum top_ckmuxsel _get_cpu_clock_switch(struct mt_cpu_dvfs *p)
{
	unsigned int val = cpufreq_read(TOP_CKMUXSEL);
unsigned int mask = _BITMASK_(1:0);

	FUNC_ENTER(FUNC_LV_HELP);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG))
		val = (val & (mask << 2)) >> 2;	/* _BITMASK_(3 : 2) */
	else
		val &= mask;	/* _BITMASK_(1 : 0) */

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
	/* unsigned long addr_con1; */
	unsigned int dds;
	unsigned int is_fhctl_used;
	unsigned int cur_volt;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (cur_khz < PLL_DIV1_FREQ && PLL_DIV1_FREQ < target_khz) {
		set_cur_freq(p, cur_khz, PLL_DIV1_FREQ);
		cur_khz = PLL_DIV1_FREQ;
	} else if (target_khz < PLL_DIV1_FREQ && PLL_DIV1_FREQ < cur_khz) {
		set_cur_freq(p, cur_khz, PLL_DIV1_FREQ);
		cur_khz = PLL_DIV1_FREQ;
	}

	/* addr_con1 = (unsigned long)(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_CON1 : ARMCA15PLL_CON1); */

#if defined(CONFIG_CPU_DVFS_BRINGUP)
	is_fhctl_used = 0;
#else
	is_fhctl_used = (PLL_DIV1_FREQ < target_khz) ? 1 : 0;
#endif

	cpufreq_ver("@%s():%d, cur_khz = %d, target_khz = %d, is_fhctl_used = %d\n", __func__,
		    __LINE__, cur_khz, target_khz, is_fhctl_used);

	/* calc dds */
	dds = _cpu_dds_calc(target_khz);

	if (!is_fhctl_used) {
		/* enable_clock(MT_CG_MPLL_D2, "CPU_DVFS"); */

		cur_volt = p->ops->get_cur_volt(p);
		if (cur_volt < cpu_dvfs_get_volt_by_idx(p, 4))	/* MAINPLL_FREQ ~= DVFS_BIG_F4 */
			p->ops->set_cur_volt(p, cpu_dvfs_get_volt_by_idx(p, 4));
		else
			cur_volt = 0;

		_cpu_clock_switch(p, TOP_CKMUXSEL_MAINPLL);
	}

	/* set dds */
	if (!is_fhctl_used) {
		if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
			cpufreq_write(ARMCA7PLL_CON1, dds | _BIT_(31));	/* CHG */
		else
			cpufreq_write(ARMCA15PLL_CON1, dds | _BIT_(31));	/* CHG */
	} else {
BUG_ON(dds & _BITMASK_(26:24));
		/* should not use posdiv */
#ifndef __KERNEL__
		freqhopping_dvt_dvfs_enable(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_ID :
					    ARMCA15PLL_ID, target_khz);
#else				/* __KERNEL__ */
		mt_dfs_armpll(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? FH_ARMCA7_PLLID :
			      FH_ARMCA15_PLLID, dds);
#endif				/* ! __KERNEL__ */
	}

	udelay(PLL_SETTLE_TIME);

	if (!is_fhctl_used) {
		_cpu_clock_switch(p, TOP_CKMUXSEL_ARMPLL);

		if (cur_volt)
			p->ops->set_cur_volt(p, cur_volt);

		/* disable_clock(MT_CG_MPLL_D2, "CPU_DVFS"); */
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

/* for volt change (PMICWRAP/extBuck) */

static unsigned int get_cur_volt_little(struct mt_cpu_dvfs *p)
{
	unsigned int rdata;

#ifdef CONFIG_OF
	unsigned int isenabled = 0;
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

#ifdef CONFIG_OF
	isenabled = regulator_is_enabled(reg_vpca53);
	if (isenabled)
		rdata = regulator_get_voltage(reg_vpca53) / 1000;
	else
		rdata = 0;
#else
	pwrap_read(PMIC_ADDR_VPROC_CA53_EN, &rdata);

rdata &= _BITMASK_(0:0);	/* enable or disable (i.e. 0mv or not) */

	if (rdata) {		/* enabled i.e. not 0mv */
		pwrap_read(PMIC_ADDR_VPROC_CA53_VOSEL_ON, &rdata);

		rdata = PMIC_VAL_TO_VOLT(rdata);
	}
#endif

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata;		/* mv: vproc */
}

static unsigned int get_cur_vsram_big(struct mt_cpu_dvfs *p)
{
	unsigned int rdata;
#ifdef CONFIG_OF
	unsigned int isenabled = 0;
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

#ifdef CONFIG_OF
	isenabled = regulator_is_enabled(reg_vsramca57);
	if (isenabled)
		rdata = regulator_get_voltage(reg_vsramca57) / 1000;
	else
		rdata = 0;
#else
	unsigned int retry_cnt = 5;

	pwrap_read(PMIC_ADDR_VSRAM_CA57_EN, &rdata);

rdata &= _BITMASK_(0:0);	/* enable or disable (i.e. 0mv or not) */

	if (rdata) {		/* enabled i.e. not 0mv */
		do {
			pwrap_read(PMIC_ADDR_VSRAM_CA57_VOSEL_ON, &rdata);
} while (rdata == _BITMASK_(0:0) && retry_cnt--);

		rdata = PMIC_VAL_TO_VOLT(rdata);
	}
#endif

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata;		/* mv: vproc */
}

static unsigned int get_cur_volt_big(struct mt_cpu_dvfs *p)
{
	unsigned int ret_mv;
#ifdef CONFIG_OF
	unsigned int isenabled = 0;
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	isenabled = regulator_is_enabled(reg_ext_vpca57);
	if (isenabled)
		ret_mv = regulator_get_voltage(reg_ext_vpca57) / 1000;
	else
		ret_mv = 0;

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret_mv;
}

unsigned int mt_cpufreq_cur_vproc(enum mt_cpu_dvfs_id id)
{				/* TODO: rename it to mt_cpufreq_get_cur_volt() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);
	BUG_ON(NULL == p->ops);

	FUNC_EXIT(FUNC_LV_API);

	return p->ops->get_cur_volt(p);
}
EXPORT_SYMBOL(mt_cpufreq_cur_vproc);

static int set_cur_volt_little(struct mt_cpu_dvfs *p, unsigned int mv)
{				/* mv: vproc */
	unsigned int cur_mv = get_cur_volt_little(p);

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (p->dvfs_disable_by_ptpod) {
		cpufreq_err("@%s():%d, mv = %d\n", __func__, __LINE__, mv);
		mv = 1000;
	}
#ifdef CONFIG_OF
	regulator_set_voltage(reg_vpca53, mv * 1000, mv * 1000 + 6250 - 1);
	regulator_vpca53_voltage = mv;
#else
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VPROC_CA7, VOLT_TO_PMIC_VAL(mv));
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VPROC_CA7);
#endif

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

	cpufreq_err("[%s/%d]\n" "cpufreq_oppidx = %d\n", p->name, p->cpu_id, p->idx_opp_tbl);

	for (i = 0; i < p->nr_opp_tbl; i++) {
		cpufreq_err("\tOP(%d, %d),\n",
			    cpu_dvfs_get_freq_by_idx(p, i), cpu_dvfs_get_volt_by_idx(p, i)
		    );
	}
}

#define CA57_VSRAM_UPPER_LIMIT	1150
#define CA57_VSRAM_LOWER_LIMIT	930
#define MAX_DIFF_VSRAM_VPROC_TOLERANCE	10
static int set_cur_volt_big(struct mt_cpu_dvfs *p, unsigned int mv)
{				/* mv: vproc */
	unsigned int cur_vsram_mv = get_cur_vsram_big(p);
	unsigned int cur_vproc_mv = get_cur_volt_big(p);
	unsigned int check_vsram_mv = 0, check_vproc_mv = 0;
	int vsram_ceiling_flag = 0;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if ((cur_vproc_mv >= 1000) && (mv < 1000))
		turn_off_SPARK("Big voltage less then 1 V");

	if (cur_vproc_mv == 0) {
		cpufreq_dbg("%s, [cur_vsram_mv, cur_vproc_mv] = [%d, %d]\n", __func__, cur_vsram_mv, cur_vproc_mv);
		return -1;
	}

	if (p->dvfs_disable_by_ptpod) {
		cpufreq_err("@%s():%d, mv = %d\n", __func__, __LINE__, mv);
		mv = 1000;
	}
	if (!
	       ((cur_vsram_mv >= cur_vproc_mv) /* VSRAM with upper limit */
		&& (MAX_DIFF_VSRAM_VPROC + MAX_DIFF_VSRAM_VPROC_TOLERANCE >= (cur_vsram_mv - cur_vproc_mv)))) {
		cpufreq_err("%s, [cur_vsram_mv, cur_vproc_mv] = [%d, %d]\n", __func__, cur_vsram_mv, cur_vproc_mv);
		return -1;
	}
	BUG_ON(!
	       ((cur_vsram_mv >= cur_vproc_mv) /* VSRAM with upper limit */
		&& (MAX_DIFF_VSRAM_VPROC + MAX_DIFF_VSRAM_VPROC_TOLERANCE >= (cur_vsram_mv - cur_vproc_mv))));

	/* UP */
	if (mv > cur_vproc_mv) {
		unsigned int target_vsram_mv = min((mv + NORMAL_DIFF_VRSAM_VPROC)
						   , (unsigned int)(CA57_VSRAM_UPPER_LIMIT));
		unsigned int next_vsram_mv;

		do {
			next_vsram_mv = min(MAX_DIFF_VSRAM_VPROC + cur_vproc_mv, target_vsram_mv);

			/* update vsram */
			/* cur_vsram_mv = next_vsram_mv; */
			cur_vsram_mv = min(next_vsram_mv, (unsigned int)(CA57_VSRAM_UPPER_LIMIT));
			if (cur_vsram_mv == CA57_VSRAM_UPPER_LIMIT)
				vsram_ceiling_flag = 1;

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv)
				       && (MAX_DIFF_VSRAM_VPROC + MAX_DIFF_VSRAM_VPROC_TOLERANCE
					   >= (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n",
					    __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}
#ifdef CONFIG_OF
			regulator_set_voltage(reg_vsramca57, cur_vsram_mv * 1000,
					      cur_vsram_mv * 1000 + 6250 - 1);
			check_vsram_mv = regulator_get_voltage(reg_vsramca57);
			if (check_vsram_mv < cur_vsram_mv * 1000 || cur_vsram_mv * 1000 + 6250 - 1 < check_vsram_mv) {
				cpufreq_err("@%s():%d, vsram uv: set %d but get %d\n"
					, __func__, __LINE__, cur_vsram_mv, check_vsram_mv);
				BUG();
			}
#else
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VSRAM_CA15L,
						VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_apply_pmic_cmd(IDX_NM_VSRAM_CA15L);
#endif

			/* update vproc */
			if (vsram_ceiling_flag)
				cur_vproc_mv = min((cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC), mv);
			else
				cur_vproc_mv = cur_vsram_mv - NORMAL_DIFF_VRSAM_VPROC;

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv)
				       && (MAX_DIFF_VSRAM_VPROC + MAX_DIFF_VSRAM_VPROC_TOLERANCE
					   >= (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n",
					    __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}
#ifdef CONFIG_OF
			regulator_set_voltage(reg_ext_vpca57, cur_vproc_mv * 1000,
					      cur_vproc_mv * 1000 + 10000 - 1);
			check_vproc_mv = regulator_get_voltage(reg_ext_vpca57);
			if (check_vproc_mv < cur_vproc_mv * 1000 || cur_vproc_mv * 1000 + 10000 - 1 < check_vproc_mv) {
				cpufreq_err("@%s():%d, vproc uv: set %d but get %d\n"
					, __func__, __LINE__, cur_vproc_mv, check_vproc_mv);
				BUG();
			}
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
			if (!ext_buck_vosel(cur_vproc_mv * 1000)) {
				cpufreq_err("%s(), fail to set ext buck volt\n", __func__);
				ret = -1;
			}
#endif
#endif
			udelay(PMIC_SETTLE_TIME(cur_vproc_mv - MAX_DIFF_VSRAM_VPROC, cur_vproc_mv));
		} while (((target_vsram_mv != CA57_VSRAM_UPPER_LIMIT)
			  && (cur_vsram_mv < target_vsram_mv))
			 || (cur_vproc_mv < mv));
	}
	/* DOWN */
	else if (mv < cur_vproc_mv) {
		unsigned int next_vproc_mv;
		unsigned int next_vsram_mv = cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC;

		do {
			next_vproc_mv = max((next_vsram_mv - MAX_DIFF_VSRAM_VPROC), mv);

			/* update vproc */
			cur_vproc_mv = next_vproc_mv;

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv)
				       && (MAX_DIFF_VSRAM_VPROC + 10 >=
					   (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n",
					    __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}
#ifdef CONFIG_OF
			regulator_set_voltage(reg_ext_vpca57, cur_vproc_mv * 1000,
					      cur_vproc_mv * 1000 + 10000 - 1);
			check_vproc_mv = regulator_get_voltage(reg_ext_vpca57);
			if (check_vproc_mv < cur_vproc_mv * 1000 || cur_vproc_mv * 1000 + 10000 - 1 < check_vproc_mv) {
				cpufreq_err("@%s():%d, vproc uv: set %d but get %d\n"
					, __func__, __LINE__, cur_vproc_mv, check_vproc_mv);
				BUG();
			}
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
			if (!ext_buck_vosel(cur_vproc_mv * 1000)) {
				cpufreq_err("%s(), fail to set ext buck volt\n", __func__);
				ret = -1;
			}
#endif
#endif

			/* update vsram */
			next_vsram_mv = cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC;
			cur_vsram_mv = max(next_vsram_mv, (unsigned int)(CA57_VSRAM_LOWER_LIMIT));

			if (unlikely(!((cur_vsram_mv > cur_vproc_mv)
				       && (MAX_DIFF_VSRAM_VPROC + 10 >=
					   (cur_vsram_mv - cur_vproc_mv))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, cur_vsram_mv = %d, cur_vproc_mv = %d\n",
					    __func__, __LINE__, cur_vsram_mv, cur_vproc_mv);
				BUG();
			}
#ifdef CONFIG_OF
			regulator_set_voltage(reg_vsramca57, cur_vsram_mv * 1000,
					      cur_vsram_mv * 1000 + 6250 - 1);
			check_vsram_mv = regulator_get_voltage(reg_vsramca57);
			if (check_vsram_mv < cur_vsram_mv * 1000 || cur_vsram_mv * 1000 + 6250 - 1 < check_vsram_mv) {
				cpufreq_err("@%s():%d, vsram uv: set %d but get %d\n"
					, __func__, __LINE__, cur_vsram_mv, check_vsram_mv);
				BUG();
			}
#else
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VSRAM_CA15L,
						VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_apply_pmic_cmd(IDX_NM_VSRAM_CA15L);
#endif
			udelay(PMIC_SETTLE_TIME(cur_vproc_mv + MAX_DIFF_VSRAM_VPROC, cur_vproc_mv));

			/* VSRAM -VProc should between 100~200mV.
				When VSRAM reach lower limit 930mV,
				(it should be lower if there is no limit)
				(VSRAM -VProc) will be > 200mV on next round. */
			if (cur_vsram_mv == CA57_VSRAM_LOWER_LIMIT)
				break;
		} while (cur_vproc_mv > mv);
	}

	if (NULL != g_pCpuVoltSampler)
		g_pCpuVoltSampler(MT_CPU_DVFS_BIG, mv);

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

	BUG_ON(i < 0);		/* i.e. target_khz > p->opp_tbl[0].cpufreq_khz */

	FUNC_EXIT(FUNC_LV_HELP);

	return cpu_dvfs_get_volt_by_idx(p, i);
}

static int _cpufreq_set_locked(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz,
			       struct cpufreq_policy *policy)
{
	/* unsigned int cpu;                    *//* XXX: it causes deadlock */
	/* struct cpufreq_freqs freqs;          *//* XXX: it causes deadlock */
	/* struct cpufreq_policy *policy;       *//* XXX: it causes deadlock */

	unsigned int mv;
	int ret = 0;
	/* notify governor right after set_cur_freq() rather than all DVFS procedure finished */
	struct cpufreq_freqs freqs;
	unsigned int target_khz_orig = target_khz;

	enum turbo_mode mode = get_turbo_mode(p, target_khz);

	FUNC_ENTER(FUNC_LV_HELP);

	mv = _search_available_volt(p, target_khz);

	if (cur_khz != TURBO_MODE_FREQ(mode, target_khz))
		cpufreq_ver
		    ("@%s(), target_khz = %d (%d), mv = %d (%d), num_online_cpus = %d, cur_khz = %d\n",
		     __func__, target_khz, TURBO_MODE_FREQ(mode, target_khz), mv,
		     TURBO_MODE_VOLT(mode, mv), num_online_cpus(), cur_khz);

	mv = TURBO_MODE_VOLT(mode, mv);
	target_khz = TURBO_MODE_FREQ(mode, target_khz);

	if (cur_khz == target_khz)
		goto out;

	/* set volt (UP) */
	if (cur_khz < target_khz) {
		ret = p->ops->set_cur_volt(p, mv);

		if (ret)	/* set volt fail */
			goto out;
	}

	/* notify governor right after set_cur_freq() rather than all DVFS procedure finished */
	freqs.old = cur_khz;
	/* new freq without turbo */
	freqs.new = target_khz_orig;
	/* fix notify transition hang issue for Linux-3.18 */
#if 0
	if (policy) {
		for_each_online_cpu(cpu) {
			freqs.cpu = cpu;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
		}
	}
#else
	/* fix notify transition hang issue for Linux-3.18 */
	if (policy) {
		freqs.cpu = policy->cpu;
		cpufreq_freq_transition_begin(policy, &freqs);
	}
#endif

	/* set freq (UP/DOWN) */
	if (cur_khz != target_khz)
		p->ops->set_cur_freq(p, cur_khz, target_khz);

	/* notify governor right after set_cur_freq() rather than all DVFS procedure finished */
#if 0
	if (policy) {
		for_each_online_cpu(cpu) {
			freqs.cpu = cpu;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
		}
	}
#else
	/* fix notify transition hang issue for Linux-3.18 */
	if (policy)
		cpufreq_freq_transition_end(policy, &freqs, 0);
#endif

	/* set volt (DOWN) */
	if (cur_khz > target_khz) {
		ret = p->ops->set_cur_volt(p, mv);

		if (ret)	/* set volt fail */
			goto out;
	}

	FUNC_EXIT(FUNC_LV_HELP);
out:
	return ret;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx);

unsigned int stress_test = 0;
static void _mt_cpufreq_set(enum mt_cpu_dvfs_id id, int new_opp_idx)
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int cur_freq;
	unsigned int target_freq;
	/* notify governor right after set_cur_freq() rather than all DVFS procedure finished */
#if 0
	unsigned int cpu;
	struct cpufreq_freqs freqs;
#endif
	struct cpufreq_policy *policy;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);
	/* BUG_ON(new_opp_idx >= p->nr_opp_tbl); */

	policy = cpufreq_cpu_get(p->cpu_id);

	/* notify governor right after set_cur_freq() rather than all DVFS procedure finished */
#if 0
	if (policy) {
		freqs.old = policy->cur;
		freqs.new = mt_cpufreq_max_frequency_by_DVS(id, new_opp_idx);

		for_each_online_cpu(cpu) {	/* TODO: big LITTLE issue (id mapping) */
			freqs.cpu = cpu;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
		}
	}
#endif

	cpufreq_lock(flags);	/* <-XXX */

	if (new_opp_idx == -1)
		new_opp_idx = p->idx_opp_tbl;

#if defined(CONFIG_CPU_DVFS_BRINGUP)
	new_opp_idx = id_to_cpu_dvfs(id)->idx_normal_max_opp;
#elif defined(CONFIG_CPU_DVFS_RANDOM_TEST)
	new_opp_idx = jiffies & 0x7;	/* 0~7 */
#else
	new_opp_idx = _calc_new_opp_idx(id_to_cpu_dvfs(id), new_opp_idx);
	if (stress_test)
		new_opp_idx = jiffies & 0x7;	/* 0~7 */
#endif

	cur_freq = p->ops->get_cur_phy_freq(p);
	target_freq = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);

#if 0				/* not in mt8173 */
	/* enable FBB for CA15L before entering into first OPP */
	if (id == MT_CPU_DVFS_BIG && num_online_big_cpus() > 0 && 0 != p->idx_opp_tbl
	    && 0 == new_opp_idx)
		turn_on_FBB();
#endif

	/* disable SPARK for CA15L before leaving first OPP */
	if (id == MT_CPU_DVFS_BIG && 0 == p->idx_opp_tbl && 0 != new_opp_idx)
		turn_off_SPARK("Leave out oppidx_0");

	_cpufreq_set_locked(p, cur_freq, target_freq, policy);

#if 0				/* not in mt8173 */
	/* disable FBB for CA15L after leaving first OPP */
	if (id == MT_CPU_DVFS_BIG && 0 == p->idx_opp_tbl && 0 != new_opp_idx)
		turn_off_FBB();
#endif

	/* enable SPARK for CA15L at first OPP and voltage no less than 1000mv */
	if (id == MT_CPU_DVFS_BIG && num_online_big_cpus() > 0 && 0 != p->idx_opp_tbl
	    && 0 == new_opp_idx && p->ops->get_cur_volt(p) >= 1000) {
		turn_on_SPARK();
	}

	p->idx_opp_tbl = new_opp_idx;

	cpufreq_unlock(flags);	/* <-XXX */

	/* notify governor right after set_cur_freq() rather than all DVFS procedure finished */
	if (policy) {
#if 0
		freqs.new = p->ops->get_cur_phy_freq(p);
		for_each_online_cpu(cpu) {	/* TODO: big LITTLE issue (id mapping) */
			freqs.cpu = cpu;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
		}
#endif
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
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu));	/* TODO: FIXME, for E1 */
	int num_online_cpus_little = 0, num_online_cpus_big = 0;

	if (-1 == hps_get_num_online_cpus(&num_online_cpus_little, &num_online_cpus_big)) {
		num_online_cpus_little = 2;
		num_online_cpus_big = 2;
	}


	cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d, num_online_cpus = %d, %d\n",
		    __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus_little,
		    num_online_cpus_big);

	dev = get_cpu_device(cpu);

	if (dev) {
		if (TURBO_MODE_BOUNDARY_CPU_NUM ==
		    (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? num_online_cpus_little :
		     num_online_cpus_big)) {
			switch (action & 0xF) {
			case CPU_UP_PREPARE:
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
					num_online_cpus_little_delta = 1;
				else if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG))
					num_online_cpus_big_delta = 1;
				/* fall through */
			case CPU_DEAD:
				_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ?
					MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG, -1);
				cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d\n"
					, __func__,
					__LINE__, cpu, action, p->idx_opp_tbl);
				cpufreq_ver("num_online_cpus = %d, %d, %d, %d\n",
					num_online_cpus_little, num_online_cpus_big,
					num_online_cpus_little_delta,
					num_online_cpus_big_delta);
				break;
			}
		} else {
			switch (action & 0xF) {
			case CPU_ONLINE:
			case CPU_UP_CANCELED:
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
					num_online_cpus_little_delta = 0;
				else if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG))
					num_online_cpus_big_delta = 0;

				cpufreq_ver("@%s():%d, cpu = %d, action = %lu, oppidx = %d\n",
				     __func__, __LINE__, cpu, action, p->idx_opp_tbl);
				cpufreq_ver("num_online_cpus = %d, %d, %d, %d\n",
				     num_online_cpus_little, num_online_cpus_big,
				     num_online_cpus_little_delta, num_online_cpus_big_delta);
				break;
			}
		}
	}
#else				/* XXX: DON'T USE cpufreq_driver_target() for the case which cur_freq == target_freq */
	struct cpufreq_policy *policy;
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu));	/* TODO: FIXME, for E1 */

	cpufreq_ver("@%s():%d, cpu = %d, action = %d, oppidx = %d, num_online_cpus = %d\n",
		    __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus());

	dev = get_cpu_device(cpu);

	if (dev && 0 == p->idx_opp_tbl && TURBO_MODE_BOUNDARY_CPU_NUM == num_online_cpus()
	    ) {
		switch (action) {
		case CPU_UP_PREPARE:
		case CPU_DEAD:

			policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				cpufreq_driver_target(policy, cpu_dvfs_get_cur_freq(p),
						      CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy);
			}

			cpufreq_ver
			    ("@%s():%d, cpu = %d, action = %d, oppidx = %d, num_online_cpus = %d\n",
			     __func__, __LINE__, cpu, action, p->idx_opp_tbl, num_online_cpus());
			break;
		}
	}
#endif
	return NOTIFY_OK;
}

static struct notifier_block __refdata turbo_mode_cpu_notifier = {
	.notifier_call = turbo_mode_cpu_callback,
};

static int big_clk_buck_enable(void)
{
	int ret = 0;

	ret = regulator_enable(reg_vsramca57);
	if (ret) {
		pr_err("Failed to enable g_big_vsram\n");
		return ret;
	}
	ret = regulator_enable(reg_ext_vpca57);
	if (ret) {
		pr_err("Failed to enable g_big_vproc\n");
		return ret;
	}
	ret = clk_prepare_enable(clk_pllca57);
	if (ret) {
		pr_err("Failed to enable clk_pllca57\n");
		return ret;
	}


	return ret;
}


static int big_clk_buck_disable(void)
{
	int ret = 0;

	clk_disable_unprepare(clk_pllca57);

	ret = regulator_disable(reg_ext_vpca57);
	if (ret) {
		pr_err("Failed to disable g_big_vproc\n");
		return ret;
	}
	ret = regulator_disable(reg_vsramca57);
	if (ret) {
		pr_err("Failed to disable g_big_vsram\n");
		return ret;
	}

	return ret;
}


static int __cpuinit extbuck_cpu_callback(struct notifier_block *nfb,
					  unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	int num_online_cpus_little, num_online_cpus_big;

	dev = get_cpu_device(cpu);

	if (dev) {
		switch (action) {

		case CPU_DEAD:
		case CPU_DEAD_FROZEN:
			hps_get_num_online_cpus(&num_online_cpus_little, &num_online_cpus_big);

			if (num_online_cpus_big == 0 && hps_cpu_is_cpu_big(cpu)) {
				pr_debug("CPU_DEAD, big?%d\n", hps_cpu_is_cpu_big(cpu));
				big_clk_buck_disable();
			}

			break;
		case CPU_UP_PREPARE:
		case CPU_UP_PREPARE_FROZEN:
			hps_get_num_online_cpus(&num_online_cpus_little, &num_online_cpus_big);

			if (num_online_cpus_big == 0 && hps_cpu_is_cpu_big(cpu)) {
				pr_debug("CPU_UP_PREPARE\n");
				big_clk_buck_enable();
			}

			break;
		}
	}
	return NOTIFY_OK;
}


static struct notifier_block __refdata extbuck_cpu_notifier = {
	.notifier_call = extbuck_cpu_callback,
};





static void _set_no_limited(struct mt_cpu_dvfs *p)
{
	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	p->limited_max_freq = cpu_dvfs_get_max_freq(p);
	p->limited_max_ncpu = MAX_CPU_NUM;

	FUNC_EXIT(FUNC_LV_HELP);
}

#ifdef CONFIG_CPU_DVFS_DOWNGRADE_FREQ
static void _downgrade_freq_check(enum mt_cpu_dvfs_id id)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int temp = 0;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	/* if not CPU_LEVEL0 */
	if (cpu_dvfs_get_max_freq(p) < ((MT_CPU_DVFS_LITTLE == id) ? DVFS_LITTLE_F0 : DVFS_BIG_F0))
		goto out;

	/* get temp */
#if 0				/* TODO: FIXME */

	if (mt_ptp_status((MT_CPU_DVFS_LITTLE == id) ? PTP_DET_LITTLE : PTP_DET_BIG) == 1)
		temp = (((DRV_Reg32(PTP_TEMP) & 0xff)) + 25) * 1000;	/* TODO: mt_ptp.c provide mt_ptp_get_temp() */
	else
		temp = mtktscpu_get_Tj_temp();	/* TODO: FIXME, what is the difference for big & LITTLE */

#else

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	temp = tscpu_get_bL_temp((MT_CPU_DVFS_LITTLE == id) ? THERMAL_BANK0 : THERMAL_BANK1);
#endif
#endif

	if (temp < 0 || 125000 < temp) {
		/* cpufreq_dbg("%d (temp) < 0 || 12500 < %d (temp)\n", temp, temp); */
		goto out;
	}

	{
		static enum turbo_mode pre_mode = TURBO_MODE_NONE;
		enum turbo_mode cur_mode = get_turbo_mode(p, cpu_dvfs_get_cur_freq(p));

		if (pre_mode != cur_mode) {
			_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE :
					MT_CPU_DVFS_BIG, p->idx_opp_tbl);
			cpufreq_ver
			    ("@%s():%d, oppidx = %d, num_online_cpus = %d, pre_mode = %d, cur_mode = %d\n",
			     __func__, __LINE__, p->idx_opp_tbl, num_online_cpus(), pre_mode,
			     cur_mode);
			pre_mode = cur_mode;
		}
	}

	if (temp <= p->ptpod_temperature_limit_1) {
		p->downgrade_freq_for_ptpod = false;
		/* cpufreq_dbg("%d (temp) < %d (limit_1)\n", temp, p->ptpod_temperature_limit_1); */
		goto out;
	} else if ((temp > p->ptpod_temperature_limit_1) && (temp < p->ptpod_temperature_limit_2)) {
		p->downgrade_freq_counter_return_limit =
		    p->downgrade_freq_counter_limit * p->ptpod_temperature_time_1;
		/* cpufreq_dbg("%d (temp) > %d (limit_1)\n", temp, p->ptpod_temperature_limit_1); */
	} else {
		p->downgrade_freq_counter_return_limit =
		    p->downgrade_freq_counter_limit * p->ptpod_temperature_time_2;
		/* cpufreq_dbg("%d (temp) > %d (limit_2)\n", temp, p->ptpod_temperature_limit_2); */
	}

	if (p->downgrade_freq_for_ptpod == false) {
		if ((num_online_cpus() == p->pre_online_cpu)
		    && (cpu_dvfs_get_cur_freq(p) == p->pre_freq)) {
			if ((num_online_cpus() >= p->over_max_cpu) && (p->idx_opp_tbl == 0)) {
				p->downgrade_freq_counter++;
				/*
				   cpufreq_dbg("downgrade_freq_counter_limit = %d\n",
				   p->downgrade_freq_counter_limit);
				   cpufreq_dbg("downgrade_freq_counter = %d\n", p->downgrade_freq_counter);
				 */

				if (p->downgrade_freq_counter >= p->downgrade_freq_counter_limit) {
					p->downgrade_freq = cpu_dvfs_get_freq_by_idx(p, 1);

					p->downgrade_freq_for_ptpod = true;
					p->downgrade_freq_counter = 0;

					cpufreq_dbg("freq limit, downgrade_freq_for_ptpod = %d\n",
						    p->downgrade_freq_for_ptpod);

					policy = cpufreq_cpu_get(p->cpu_id);

					if (!policy)
						goto out;

					cpufreq_driver_target(policy, p->downgrade_freq,
							      CPUFREQ_RELATION_L);

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

		/* cpufreq_dbg("downgrade_freq_counter_return_limit = %d\n", p->downgrade_freq_counter_return_limit); */
		/* cpufreq_dbg("downgrade_freq_counter_return = %d\n", p->downgrade_freq_counter_return); */

		if (p->downgrade_freq_counter_return >= p->downgrade_freq_counter_return_limit) {
			p->downgrade_freq_for_ptpod = false;
			p->downgrade_freq_counter_return = 0;

			/*
			   cpufreq_dbg("Release freq limit, downgrade_freq_for_ptpod = %d\n"
			   , p->downgrade_freq_for_ptpod);
			 */
		}
	}

out:
	FUNC_EXIT(FUNC_LV_API);
}

static void _init_downgrade(struct mt_cpu_dvfs *p, unsigned int cpu_level)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (cpu_level) {
	case CPU_LEVEL_1:
	case CPU_LEVEL_2:
	case CPU_LEVEL_3:
	case CPU_LEVEL_4:
	case CPU_LEVEL_5:
	default:
		p->downgrade_freq_counter_limit = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 10 : 10;
		p->ptpod_temperature_time_1 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 2 : 1;
		p->ptpod_temperature_time_2 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 8 : 4;
		break;
	}

	/* install callback */
	cpufreq_freq_check = _downgrade_freq_check;

	FUNC_EXIT(FUNC_LV_HELP);
}
#endif

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

		/* TODO: apply correct voltage??? */

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
		if (cpu_dvfs_is_available(p))
			_sync_opp_tbl_idx(p);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static struct cpumask cpumask_big;
static struct cpumask cpumask_little;

static enum mt_cpu_dvfs_id _get_cpu_dvfs_id(unsigned int cpu_id)
{
#if 1				/* TODO: FIXME, just for E1 */
	return cpumask_test_cpu(cpu_id, &cpumask_little) ? MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG;
#else				/* TODO: FIXME, just for E1 */
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		if (p->cpu_id == cpu_id)
			break;
	}

	BUG_ON(i >= NR_MT_CPU_DVFS);

	return i;
#endif
}

int mt_cpufreq_state_set(int enabled)
{				/* TODO: state set by id??? */
	int ret = 0;

	FUNC_ENTER(FUNC_LV_API);
#if 0
	bool set_normal_max_opp = false;
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		cpufreq_lock(flags);

		if (enabled) {
			/* enable CPU DVFS */
			if (p->cpufreq_pause) {
				p->dvfs_disable_count--;
				cpufreq_dbg("enable %s DVFS: dvfs_disable_count = %d\n", p->name,
					    p->dvfs_disable_count);

				if (p->dvfs_disable_count <= 0)
					p->cpufreq_pause = false;
				else
					cpufreq_dbg
					    ("someone still disable %s DVFS and cant't enable it\n",
					     p->name);
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
				cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p),
						      CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy);
			} else {
				cpufreq_warn("can't get cpufreq policy to disable %s DVFS\n",
					     p->name);
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
#define P_MCU_L         (1243)	/* MCU Leakage Power          */
#define P_MCU_T         (2900)	/* MCU Total Power            */
#define P_CA7_L         (110)	/* CA7 Leakage Power          */
#define P_CA7_T         (305)	/* Single CA7 Core Power      */

#define P_MCL99_105C_L  (1243)	/* MCL99 Leakage Power @ 105C */
#define P_MCL99_25C_L   (93)	/* MCL99 Leakage Power @ 25C  */
#define P_MCL50_105C_L  (587)	/* MCL50 Leakage Power @ 105C */
#define P_MCL50_25C_L   (35)	/* MCL50 Leakage Power @ 25C  */

#define T_105           (105)	/* Temperature 105C           */
#define T_65            (65)	/* Temperature 65C            */
#define T_25            (25)	/* Temperature 25C            */

#define P_MCU_D ((P_MCU_T - P_MCU_L) - 8 * (P_CA7_T - P_CA7_L))	/* MCU dynamic power except of CA7 cores */

#define P_TOTAL_CORE_L ((P_MCL99_105C_L  * 27049) / 100000)	/* Total leakage at T_65 */
#define P_EACH_CORE_L  ((P_TOTAL_CORE_L * ((P_CA7_L * 1000) / P_MCU_L)) / 1000)	/* 1 core leakage at T_65 */

#define P_CA7_D_1_CORE ((P_CA7_T - P_CA7_L) * 1)	/* CA7 dynamic power for 1 cores turned on */
#define P_CA7_D_2_CORE ((P_CA7_T - P_CA7_L) * 2)	/* CA7 dynamic power for 2 cores turned on */
#define P_CA7_D_3_CORE ((P_CA7_T - P_CA7_L) * 3)	/* CA7 dynamic power for 3 cores turned on */
#define P_CA7_D_4_CORE ((P_CA7_T - P_CA7_L) * 4)	/* CA7 dynamic power for 4 cores turned on */

#define A_1_CORE (P_MCU_D + P_CA7_D_1_CORE)	/* MCU dynamic power for 1 cores turned on */
#define A_2_CORE (P_MCU_D + P_CA7_D_2_CORE)	/* MCU dynamic power for 2 cores turned on */
#define A_3_CORE (P_MCU_D + P_CA7_D_3_CORE)	/* MCU dynamic power for 3 cores turned on */
#define A_4_CORE (P_MCU_D + P_CA7_D_4_CORE)	/* MCU dynamic power for 4 cores turned on */

static void _power_calculation(struct mt_cpu_dvfs *p, int idx, int ncpu)
{
	int multi = 0, p_dynamic = 0, p_leakage = 0, freq_ratio = 0, volt_square_ratio = 0;
	int possible_cpu = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	possible_cpu = num_possible_cpus();	/* TODO: FIXME */

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

	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_ncpu = ncpu + 1;
	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_khz = p->opp_tbl[idx].cpufreq_khz;
	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_power = p_dynamic + p_leakage;

	cpufreq_dbg("p->power_tbl[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		    (idx * possible_cpu + ncpu),
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_ncpu,
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_khz,
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_power);

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
	cpufreq_dbg
	    ("P_CA7_D_1_CORE = %d, P_CA7_D_2_CORE = %d, P_CA7_D_3_CORE = %d, P_CA7_D_4_CORE = %d\n",
	     P_CA7_D_1_CORE, P_CA7_D_2_CORE, P_CA7_D_3_CORE, P_CA7_D_4_CORE);
	cpufreq_dbg("P_TOTAL_CORE_L = %d, P_EACH_CORE_L = %d\n", P_TOTAL_CORE_L, P_EACH_CORE_L);
	cpufreq_dbg("A_1_CORE = %d, A_2_CORE = %d, A_3_CORE = %d, A_4_CORE = %d\n", A_1_CORE,
		    A_2_CORE, A_3_CORE, A_4_CORE);

	possible_cpu = num_possible_cpus();	/* TODO: FIXME */

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl =
	    kzalloc(p->nr_opp_tbl * possible_cpu * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

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

				tmp.cpufreq_khz = p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu = p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power = p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz = p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu = p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power = p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz = tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu = tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power = tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		cpufreq_dbg("[%d] = { .khz = %d, .ncup = %d, .power = %d }\n",
			    p->power_tbl[i].cpufreq_khz,
			    p->power_tbl[i].cpufreq_ncpu, p->power_tbl[i].cpufreq_power);
	}

out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}
#else
static void _power_calculation(struct mt_cpu_dvfs *p, int oppidx, int ncpu)
{
#define CA7_REF_POWER	715	/* mW  */
#define CA7_REF_FREQ	1696000	/* KHz */
#define CA7_REF_VOLT	1000	/* mV  */
#define CA15L_REF_POWER	3910	/* mW  */
#define CA15L_REF_FREQ	2093000	/* KHz */
#define CA15L_REF_VOLT	1020	/* mV  */

	int p_dynamic = 0, ref_freq, ref_volt;
	int possible_cpu = num_possible_cpus();	/* TODO: FIXME */

	FUNC_ENTER(FUNC_LV_HELP);

	p_dynamic = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_POWER : CA15L_REF_POWER;
	ref_freq = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_FREQ : CA15L_REF_FREQ;
	ref_volt = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_VOLT : CA15L_REF_VOLT;

	p_dynamic = p_dynamic *
	    (p->opp_tbl[oppidx].cpufreq_khz / 1000) / (ref_freq / 1000) *
	    p->opp_tbl[oppidx].cpufreq_volt / ref_volt * p->opp_tbl[oppidx].cpufreq_volt / ref_volt;
	+mt_spower_get_leakage(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_SPOWER_CA7 : MT_SPOWER_CA17,
			       p->opp_tbl[oppidx].cpufreq_volt, 85);

	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_ncpu = ncpu + 1;
	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_khz =
	    p->opp_tbl[oppidx].cpufreq_khz;
	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_power =
	    p_dynamic * (ncpu + 1) / possible_cpu;

	FUNC_EXIT(FUNC_LV_HELP);
}

static int setup_power_table(struct mt_cpu_dvfs *p)
{
	static const unsigned int pwr_tbl_cgf[NR_MAX_CPU] = { 0, 0, 0, 0, };
	unsigned int pwr_eff_tbl[NR_MAX_OPP_TBL][NR_MAX_CPU];
	unsigned int pwr_eff_num;
	int possible_cpu = num_possible_cpus();	/* TODO: FIXME */
	int i, j;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	if (p->power_tbl)
		goto out;

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl =
	    kzalloc(p->nr_opp_tbl * possible_cpu * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

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

				tmp.cpufreq_khz = p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu = p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power = p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz = p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu = p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power = p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz = tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu = tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power = tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		cpufreq_dbg
		    ("[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d }\n", i,
		     p->power_tbl[i].cpufreq_khz, p->power_tbl[i].cpufreq_ncpu,
		     p->power_tbl[i].cpufreq_power);
	}

#if 0				/* def CONFIG_THERMAL */	/* TODO: FIXME */
	mtk_cpufreq_register(p->power_tbl, p->nr_power_tbl);
#endif

out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}
#endif

/* mt8173 power budge */
#define	CA53_VOL_BASE	1125
#define	CA53_FREQ_BASE	1700
#define CA53_DYNAMIC_POWER_BASE	276
#define CA53_N_DYNAMIC_POWER_BASE	27	/* 27 */
#define CA53_LEAKAGE_POWER_BASE	276
#define CA53_N_LEAKAGE_POWER_BASE	27	/* 27 */

#define	CA57_VOL_BASE	1125
#define	CA57_FREQ_BASE	2000
#define CA57_DYNAMIC_POWER_BASE	1340
#define CA57_N_DYNAMIC_POWER_BASE	160	/* 160 */
#define CA57_LEAKAGE_POWER_BASE	340
#define CA57_N_LEAKAGE_POWER_BASE	180	/* 180 */

			     /* #define MAX_TLP	4 *//* 8 */
			     /* #define MAX_TLP_CA57	2 *//* 4 */
			     /* #define MAX_TLP_CA53	2 *//* 4 */
#define FREQ_LEVEL_NUM	8
#define CA53_CA57_FREQ_COMBINATION	8

#define CA53_PERFORMANCE_COEFFICIENT	23
#define CA57_PERFORMANCE_COEFFICIENT	42

unsigned int get_ca53_dynamic_power(unsigned int num, unsigned freq, unsigned int vol,
				    unsigned freq_base, unsigned int vol_base)
{
	unsigned int total_power = 0;

	/* total_power =(CA53_DYNAMIC_POWER_BASE*num+CA53_N_DYNAMIC_POWER_BASE)
	 *(vol/vol_base)*(vol/vol_base)*(freq/freq_base); */
	total_power = (CA53_DYNAMIC_POWER_BASE * num + CA53_N_DYNAMIC_POWER_BASE * (num ? 1 : 0))
	    * vol / vol_base * vol / vol_base * freq / freq_base;
	return total_power;
}

unsigned int get_ca57_dynamic_power(unsigned int num, unsigned freq, unsigned int vol,
				    unsigned freq_base, unsigned int vol_base)
{
	unsigned int total_power = 0;

	/* total_power =(CA57_DYNAMIC_POWER_BASE*num+CA57_N_DYNAMIC_POWER_BASE)
	 *(vol/vol_base)*(vol/vol_base)*(freq/freq_base); */
	total_power = (CA57_DYNAMIC_POWER_BASE * num + CA57_N_DYNAMIC_POWER_BASE * (num ? 1 : 0))
	    * vol / vol_base * vol / vol_base * freq / freq_base;
	return total_power;
}

unsigned int get_ca53_leakage_power(unsigned int num, unsigned int vol, unsigned int vol_base)
{
	unsigned int total_power = 0;

	/* total_power = (CA53_LEAKAGE_POWER_BASE*num+CA53_N_LEAKAGE_POWER_BASE)
	 *(vol/vol_base)*(vol/vol_base)*(vol/vol_base); */
	total_power = (CA53_LEAKAGE_POWER_BASE * num + CA53_N_LEAKAGE_POWER_BASE * (num ? 1 : 0))
	    * vol / vol_base * vol / vol_base * vol / vol_base;
	return total_power;
}

unsigned int get_ca57_leakage_power(unsigned int num, unsigned int vol, unsigned int vol_base)
{
	unsigned int total_power = 0;

	/* total_power = (CA57_LEAKAGE_POWER_BASE*num+CA57_N_LEAKAGE_POWER_BASE)
	 *(vol/vol_base)*(vol/vol_base)*(vol/vol_base); */
	total_power = (CA57_LEAKAGE_POWER_BASE * num + CA57_N_LEAKAGE_POWER_BASE * (num ? 1 : 0))
	    * vol / vol_base * vol / vol_base * vol / vol_base;
	return total_power;
}

unsigned int get_ca53_performance(unsigned int num, unsigned freq)
{
	return num * freq * CA53_PERFORMANCE_COEFFICIENT;
}

unsigned int get_ca57_performance(unsigned int num, unsigned freq)
{
	return num * freq * CA57_PERFORMANCE_COEFFICIENT;
}

int power_budget_upper_bound = 0;
static void _mt_cpufreq_tlp_power_init(struct mt_cpu_dvfs *p)
{
#define CA7_REF_POWER	715	/* mW  */
#define CA7_REF_FREQ	1696000	/* KHz */
#define CA7_REF_VOLT	1000	/* mV  */
#define CA15L_REF_POWER	3910	/* mW  */
#define CA15L_REF_FREQ	2093000	/* KHz */
#define CA15L_REF_VOLT	1020	/* mV  */

#if 0
	static bool leakage_inited_little;
	static bool leakage_inited_big;
	unsigned int sindex;
	int p_dynamic = 0, ref_freq, ref_volt;
	int possible_cpu_little, possible_cpu_big;
#endif

	int i, j, k;
	int tlp, ca57_on_num, ca53_on_num;
	unsigned int ca53_curr_freq, ca53_curr_vol, ca57_curr_freq, ca57_curr_vol;
	unsigned int curr_performance;
	unsigned int curr_power;
	unsigned int max_tlp = num_possible_cpus();
	unsigned int max_tlp_ca53 = 2, max_tlp_ca57 = 2;

	hps_get_num_possible_cpus(&max_tlp_ca53, &max_tlp_ca57);

#if 1
	/* mt8173, no table, calculate by formula */
	/* if (cpu_tlp_power_tbl == NULL) { */
	if ((cpu_dvfs[MT_CPU_DVFS_LITTLE].opp_tbl != NULL)
	    && (cpu_dvfs[MT_CPU_DVFS_BIG].opp_tbl != NULL) && (cpu_tlp_power_tbl == NULL)) {
		/* get space */
		cpu_tlp_power_tbl = get_cpu_tlp_power_tbl();

		for (tlp = 1; tlp <= max_tlp; tlp++) {
			if (tlp >= max_tlp_ca57)
				ca57_on_num = max_tlp_ca57;
			else
				ca57_on_num = tlp;
			ca53_on_num = tlp - ca57_on_num;
			for (i = 0; i < CA53_CA57_FREQ_COMBINATION; i++) {
				for (j = 0; j < FREQ_LEVEL_NUM; j++) {
					cpu_tlp_power_tbl[tlp - 1].power_tbl[i * FREQ_LEVEL_NUM +
									     j].ncpu_big =
					    ca57_on_num;
					cpu_tlp_power_tbl[tlp - 1].power_tbl[i * FREQ_LEVEL_NUM +
									     j].khz_big =
					    ca57_on_num ? j : 7;
					cpu_tlp_power_tbl[tlp - 1].power_tbl[i * FREQ_LEVEL_NUM +
									     j].ncpu_little =
					    ca53_on_num;
					cpu_tlp_power_tbl[tlp - 1].power_tbl[i * FREQ_LEVEL_NUM +
									     j].khz_little =
					    ca57_on_num ? 0 : (ca53_on_num ? j : 0);

					if (cpu_dvfs[MT_CPU_DVFS_LITTLE].opp_tbl) {
						ca53_curr_freq
						    =
						    cpu_dvfs_get_freq_by_idx((&cpu_dvfs
									      [MT_CPU_DVFS_LITTLE])
									     ,
									     ca57_on_num ? 0
									     : (ca53_on_num ? j :
										0));
						ca53_curr_vol =
						    cpu_dvfs_get_volt_by_idx((&cpu_dvfs
									      [MT_CPU_DVFS_LITTLE])
									     ,
									     ca57_on_num ? 0
									     : (ca53_on_num ? j :
										0));
					} else {
						ca53_curr_freq = 0;
						ca53_curr_vol = 0;
					}
					ca53_curr_freq /= 1000;
					if (cpu_dvfs[MT_CPU_DVFS_BIG].opp_tbl) {
						ca57_curr_freq =
						    cpu_dvfs_get_freq_by_idx((&cpu_dvfs
									      [MT_CPU_DVFS_BIG])
									     , ca57_on_num ? j : 7);
						ca57_curr_vol =
						    cpu_dvfs_get_volt_by_idx((&cpu_dvfs
									      [MT_CPU_DVFS_BIG])
									     , ca57_on_num ? j : 7);
					} else {
						ca57_curr_freq = 0;
						ca57_curr_vol = 0;
					}
					ca57_curr_freq /= 1000;

					curr_power =
					    get_ca53_dynamic_power(ca53_on_num, ca53_curr_freq,
								   ca53_curr_vol, CA53_FREQ_BASE,
								   CA53_VOL_BASE)
					    + get_ca53_leakage_power(ca53_on_num, ca53_curr_vol,
								     CA53_VOL_BASE)
					    + get_ca57_dynamic_power(ca57_on_num, ca57_curr_freq,
								     ca57_curr_vol, CA57_FREQ_BASE,
								     CA57_VOL_BASE)
					    + get_ca57_leakage_power(ca57_on_num, ca57_curr_vol,
								     CA57_VOL_BASE);
					cpu_tlp_power_tbl[tlp - 1].power_tbl[i * FREQ_LEVEL_NUM +
									     j].power = curr_power;

					curr_performance =
					    get_ca53_performance(ca53_on_num, ca53_curr_freq)
					    + get_ca57_performance(ca57_on_num, ca57_curr_freq);
					cpu_tlp_power_tbl[tlp - 1].power_tbl[i * FREQ_LEVEL_NUM +
									     j].performance =
					    curr_performance / 1000;
				}

				/* next ca53/ca57 combination */
				if (ca57_on_num) {
					ca57_on_num--;
					if (ca53_on_num < max_tlp_ca53)
						ca53_on_num++;
				} else if (ca53_on_num) {
					ca53_on_num--;
				} else {
					ca53_on_num = 0;
					ca53_on_num = 0;
				}
			}
		}
	} else {
		return;
	}
#else
	if (cpu_tlp_power_tbl == NULL) {
		cpu_tlp_power_tbl = get_cpu_tlp_power_tbl();
		leakage_inited_little = false;
		leakage_inited_big = false;
	}

	/* if (-1 == hps_get_num_possible_cpus(&possible_cpu_little, &possible_cpu_big)) { */
	possible_cpu_little = 2;
	possible_cpu_big = 2;
	/* } */

	ref_freq = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_FREQ : CA15L_REF_FREQ;
	ref_volt = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_VOLT : CA15L_REF_VOLT;

	/* add static power */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < cpu_tlp_power_tbl[i].nr_power_table; j++) {

			if (leakage_inited_little == false && leakage_inited_big == false)
				cpu_tlp_power_tbl[i].power_tbl[j].power = 0;

			if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
				if (cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little > 0
				    && false == leakage_inited_little) {
					sindex = cpu_tlp_power_tbl[i].power_tbl[j].khz_little - 1;

					p_dynamic = CA7_REF_POWER *
					    (p->opp_tbl[sindex].cpufreq_khz / 1000) / (ref_freq /
										       1000) *
					    p->opp_tbl[sindex].cpufreq_volt / ref_volt *
					    p->opp_tbl[sindex].cpufreq_volt / ref_volt;

					cpu_tlp_power_tbl[i].power_tbl[j].power +=
					    (p_dynamic +
					     mt_spower_get_leakage(MT_SPOWER_CA7,
								   p->opp_tbl[sindex].cpufreq_volt,
								   85)) *
					    cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little /
					    possible_cpu_little;
				}
			} else {
				if (cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big > 0
				    && false == leakage_inited_big) {
					sindex = cpu_tlp_power_tbl[i].power_tbl[j].khz_big - 1;

					p_dynamic = CA15L_REF_POWER *
					    (p->opp_tbl[sindex].cpufreq_khz / 1000) / (ref_freq /
										       1000) *
					    p->opp_tbl[sindex].cpufreq_volt / ref_volt *
					    p->opp_tbl[sindex].cpufreq_volt / ref_volt;

					cpu_tlp_power_tbl[i].power_tbl[j].power +=
					    (p_dynamic +
					     mt_spower_get_leakage(MT_SPOWER_CA17,
								   p->opp_tbl[sindex].cpufreq_volt,
								   85)) *
					    cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big /
					    possible_cpu_big;
				}
			}
		}
	}

	if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
		leakage_inited_little = true;
	else
		leakage_inited_big = true;

#endif
	/* sort power table */
	for (i = 0; i < 8; i++) {
		for (j = (cpu_tlp_power_tbl[i].nr_power_table - 1); j > 0; j--) {
			for (k = 1; k <= j; k++) {
				if (cpu_tlp_power_tbl[i].power_tbl[k - 1].power <
				    cpu_tlp_power_tbl[i].power_tbl[k].power) {
					struct mt_cpu_power_tbl tmp;

					tmp.ncpu_big =
					    cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_big;
					tmp.khz_big = cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_big;
					tmp.ncpu_little =
					    cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_little;
					tmp.khz_little =
					    cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_little;
					tmp.performance =
					    cpu_tlp_power_tbl[i].power_tbl[k - 1].performance;
					tmp.power = cpu_tlp_power_tbl[i].power_tbl[k - 1].power;

					cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_big =
					    cpu_tlp_power_tbl[i].power_tbl[k].ncpu_big;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_big =
					    cpu_tlp_power_tbl[i].power_tbl[k].khz_big;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].ncpu_little =
					    cpu_tlp_power_tbl[i].power_tbl[k].ncpu_little;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].khz_little =
					    cpu_tlp_power_tbl[i].power_tbl[k].khz_little;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].performance =
					    cpu_tlp_power_tbl[i].power_tbl[k].performance;
					cpu_tlp_power_tbl[i].power_tbl[k - 1].power =
					    cpu_tlp_power_tbl[i].power_tbl[k].power;

					cpu_tlp_power_tbl[i].power_tbl[k].ncpu_big = tmp.ncpu_big;
					cpu_tlp_power_tbl[i].power_tbl[k].khz_big = tmp.khz_big;
					cpu_tlp_power_tbl[i].power_tbl[k].ncpu_little =
					    tmp.ncpu_little;
					cpu_tlp_power_tbl[i].power_tbl[k].khz_little =
					    tmp.khz_little;
					cpu_tlp_power_tbl[i].power_tbl[k].performance =
					    tmp.performance;
					cpu_tlp_power_tbl[i].power_tbl[k].power = tmp.power;
				}
			}
		}
	}

	/* dump power table */
	for (i = 0; i < 8; i++) {
		cpufreq_dbg("TLP = %d\n", i + 1);
		for (j = 0; j < cpu_tlp_power_tbl[i].nr_power_table; j++) {
			if (cpu_tlp_power_tbl[i].power_tbl[j].power > power_budget_upper_bound)
				power_budget_upper_bound = cpu_tlp_power_tbl[i].power_tbl[j].power;

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

static int _mt_cpufreq_setup_freqs_table(struct cpufreq_policy *policy,
					 struct mt_cpu_freq_info *freqs, int num)
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
			table[i].driver_data = i;
			table[i].frequency = freqs[i].cpufreq_khz;
		}

		table[num].driver_data = i;	/* TODO: FIXME, why need this??? */
		table[num].frequency = CPUFREQ_TABLE_END;

		p->opp_tbl = freqs;
		p->nr_opp_tbl = num;
		p->freq_tbl_for_cpufreq = table;
	}

	ret = cpufreq_frequency_table_cpuinfo(policy, p->freq_tbl_for_cpufreq);

#if 0				/* not available for kernel 3.18 */
	if (!ret)
		cpufreq_frequency_table_get_attr(p->freq_tbl_for_cpufreq, policy->cpu);
#else
	if (!ret)
		policy->freq_table = p->freq_tbl_for_cpufreq;
#endif

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

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return;
	}

	_mt_cpufreq_set(id, p->idx_opp_tbl_for_late_resume);
	if (id == MT_CPU_DVFS_LITTLE) {
		regulator_set_mode(reg_vpca53, REGULATOR_MODE_NORMAL);
		if (regulator_get_mode(reg_vpca53) != REGULATOR_MODE_NORMAL)
			cpufreq_err("Vpca53 should be REGULATOR_MODE_NORMAL(%d), but mode = %d\n",
							REGULATOR_MODE_NORMAL, regulator_get_mode(reg_vpca53));
	} else {
		regulator_set_mode(reg_ext_vpca57, REGULATOR_MODE_NORMAL);
		if (regulator_get_mode(reg_ext_vpca57) != REGULATOR_MODE_NORMAL)
			cpufreq_err("vpca57 should be REGULATOR_MODE_NORMAL(%d), but mode = %d\n",
							REGULATOR_MODE_NORMAL, regulator_get_mode(reg_ext_vpca57));
	}

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_enable_by_ptpod);

unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	p->dvfs_disable_by_ptpod = true;

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}
#if 0				/* XXX: BUG_ON(irqs_disabled()) @ __cpufreq_notify_transition() */
	{
		struct cpufreq_policy *policy;

		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p),
					      CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		} else
			cpufreq_warn("can't get cpufreq policy to disable %s DVFS\n", p->name);
	}
#else
	p->idx_opp_tbl_for_late_resume = p->idx_opp_tbl;
	_mt_cpufreq_set(id, p->idx_normal_max_opp);
	_set_cur_volt_locked(p, 1000);	/* from Henry, from HPT team */
	if (id == MT_CPU_DVFS_LITTLE) {
		regulator_set_mode(reg_vpca53, REGULATOR_MODE_FAST);
		if (regulator_get_mode(reg_vpca53) != REGULATOR_MODE_FAST)
			cpufreq_err("vpca53 should be REGULATOR_MODE_FAST(%d), but mode = %d\n",
							REGULATOR_MODE_FAST, regulator_get_mode(reg_vpca53));
	} else {
		regulator_set_mode(reg_ext_vpca57, REGULATOR_MODE_FAST);
		if (regulator_get_mode(reg_ext_vpca57) != REGULATOR_MODE_FAST)
			cpufreq_err("vpca57 should be REGULATOR_MODE_FAST(%d), but mode = %d\n",
							REGULATOR_MODE_FAST, regulator_get_mode(reg_ext_vpca57));
	}

#endif

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_cur_freq(p);
}
EXPORT_SYMBOL(mt_cpufreq_disable_by_ptpod);

#define MAX_LIMITOR_NUM	2
unsigned int all_limited_power[MAX_LIMITOR_NUM] = { 0 };

int mt_cpufreq_thermal_protect(unsigned int limited_power, unsigned int limitor_index)
{
	struct mt_cpu_dvfs *p;
	int possible_cpu = 0;
	int possible_cpu_big = 0;
	int ncpu;
	int found = 0;
	unsigned long flag;
	/* unsigned int max_ncpu_big, max_khz_big, max_ncpu_little, max_khz_little; */
	int i, j, tlp = 0;
	unsigned int max_tlp = num_possible_cpus();

	FUNC_ENTER(FUNC_LV_API);

	cpufreq_dbg("%s(): limited_power = %d, limitor_index = %d\n", __func__, limited_power, limitor_index);

	if (chip_ver == CHIP_SW_VER_02 && NULL == cpu_tlp_power_tbl) {
		FUNC_EXIT(FUNC_LV_API);
		return -1;
	}

	if (!_mt_cpufreq_pdrv_probed)
		return -1;

	if (limited_power > power_budget_upper_bound)
		limited_power = 0;

	all_limited_power[limitor_index] = limited_power;
	for (i = 0; i < MAX_LIMITOR_NUM; i++) {
		if (limited_power == 0)
			limited_power = all_limited_power[i];
		if (all_limited_power[i] && (all_limited_power[i] < limited_power))
			limited_power = all_limited_power[i];
	}

	cpufreq_lock(flag);	/* <- lock */

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;
		p->thermal_protect_limited_power = limited_power;
	}

	if (-1 == hps_get_num_possible_cpus(&possible_cpu, &possible_cpu_big)) {
		possible_cpu = 2;
		possible_cpu_big = 2;
	}

	/* no limited */
	if (0 == limited_power) {
		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
				continue;
			p->limited_max_ncpu =
			    cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? possible_cpu : possible_cpu_big;
			p->limited_max_freq = cpu_dvfs_get_max_freq(p);
		}
		/* limited */
	} else if (chip_ver == CHIP_SW_VER_01) {
		p = id_to_cpu_dvfs(_get_cpu_dvfs_id(0));

		if (cpu_dvfs_is_available(p)) {
			for (ncpu = possible_cpu; ncpu > 0; ncpu--) {
				for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
					if (p->power_tbl[i].cpufreq_power <= limited_power) {
						p->limited_max_ncpu = p->power_tbl[i].cpufreq_ncpu;
						p->limited_max_freq = p->power_tbl[i].cpufreq_khz;
						found = 1;
						ncpu = 0;	/* for break outer loop */
						break;
					}
				}
			}

			/* not found and use lowest power limit */
			if (!found) {
				p->limited_max_ncpu =
				    p->power_tbl[p->nr_power_tbl - 1].cpufreq_ncpu;
				p->limited_max_freq =
				    p->power_tbl[p->nr_power_tbl - 1].cpufreq_khz;
			}
		}
	} else {
		/* search index that power is less than or equal to limited power */
		hps_get_tlp(&tlp);
		tlp = tlp / 100 + ((tlp % 100) ? 1 : 0);
		if (tlp > max_tlp)
			tlp = max_tlp;
		else if (tlp < 1)
			tlp = 1;
		tlp--;
		for (i = 0; i < cpu_tlp_power_tbl[tlp].nr_power_table; i++) {
			if (cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_little == 0)
				continue;
			if (cpu_tlp_power_tbl[tlp].power_tbl[i].power <= limited_power)
				break;
		}

		if (i < cpu_tlp_power_tbl[tlp].nr_power_table) {

			for (j = i; j < cpu_tlp_power_tbl[tlp].nr_power_table; j++) {
				if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little == 0)
					continue;
				if (cpu_tlp_power_tbl[tlp].power_tbl[j].performance >
				    cpu_tlp_power_tbl[tlp].power_tbl[i].performance)
					i = j;
			}

			if (cpu_dvfs_is_available((&cpu_dvfs[MT_CPU_DVFS_BIG]))) {
				cpu_dvfs[MT_CPU_DVFS_BIG].limited_max_ncpu =
				    cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_big;
				cpu_dvfs[MT_CPU_DVFS_BIG].limited_max_freq =
				    cpu_dvfs_get_freq_by_idx((&cpu_dvfs[MT_CPU_DVFS_BIG]),
							     cpu_tlp_power_tbl[tlp].power_tbl[i].khz_big);
			}
			if (cpu_dvfs_is_available((&cpu_dvfs[MT_CPU_DVFS_LITTLE]))) {
				cpu_dvfs[MT_CPU_DVFS_LITTLE].limited_max_ncpu =
				    cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_little;
				cpu_dvfs[MT_CPU_DVFS_LITTLE].limited_max_freq =
				    cpu_dvfs_get_freq_by_idx((&cpu_dvfs[MT_CPU_DVFS_LITTLE]),
						cpu_tlp_power_tbl[tlp].power_tbl[i].khz_little);
			}
		}
	}

	cpufreq_unlock(flag);	/* <- unlock */

#if 1				/* TODO: FIXME, apply limit */
	{
		struct cpufreq_policy *policy;

		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
				continue;
#if 0				/* defined(CONFIG_CPU_FREQ_GOV_HOTPLUG) */	/* TODO (Chun-Wei) : FIXME */
			hp_limited_cpu_num(p->limited_max_ncpu);	/* notify hotplug governor */
#else
			hps_set_cpu_num_limit(LIMIT_THERMAL,
					      cpu_dvfs[MT_CPU_DVFS_LITTLE].limited_max_ncpu,
					      cpu_dvfs[MT_CPU_DVFS_BIG].limited_max_ncpu);
#endif
			policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				/* cpufreq_driver_target(policy, p->limited_max_freq, CPUFREQ_RELATION_L); */
				if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE))
					_mt_cpufreq_set(MT_CPU_DVFS_LITTLE, p->idx_opp_tbl);
				else
					_mt_cpufreq_set(MT_CPU_DVFS_BIG, p->idx_opp_tbl);
				cpufreq_cpu_put(policy);	/* <- policy put */
			}
		}
	}
#endif

	FUNC_EXIT(FUNC_LV_API);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_thermal_protect);

int mt_cpufreq_get_thermal_limited_power(void)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		return p->thermal_protect_limited_power;
	}

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_get_thermal_limited_power);

/* for ramp down */
void mt_cpufreq_set_ramp_down_count_const(enum mt_cpu_dvfs_id id, int count)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	BUG_ON(NULL == p);

	p->ramp_down_count_const = count;
}
EXPORT_SYMBOL(mt_cpufreq_set_ramp_down_count_const);

#if 0				/* mt8173-TBD, defined but not used */
static int _keep_max_freq(struct mt_cpu_dvfs *p, unsigned int freq_old, unsigned int freq_new)
{				/* TODO: inline @ mt_cpufreq_target() */
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
#endif

static int _search_available_freq_idx(struct mt_cpu_dvfs *p, unsigned int target_khz,
				      unsigned int relation)
{				/* return -1 (not found) */
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
	} else {		/* CPUFREQ_RELATION_H */
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
	int i, j, tlp = 0;
	unsigned int max_performance, max_performance_id = 0;
	unsigned int online_cpu_big = 0, online_cpu_little = 0;
	unsigned int max_tlp = num_possible_cpus();

	FUNC_ENTER(FUNC_LV_HELP);

	possible_cpu = num_possible_cpus();	/* TODO: FIXME */
	online_cpu = num_online_cpus();	/* TODO: FIXME */

#if defined(CONFIG_CPU_FREQ_GOV_HOTPLUG)
	{
		/* extern int g_cpus_sum_load_current; */
		cpufreq_dbg("%s(): begin, idx = %d, online_cpu = %d, xxx = %d\n", __func__,
			    new_opp_idx, online_cpu, p->limited_freq_by_hevc);
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
			    && p->power_tbl[i].cpufreq_khz == p->limited_max_freq)
				break;
		}

		cpufreq_dbg("%s(): idx = %d, limited_max_ncpu = %d, limited_max_freq = %d\n",
			    __func__, i, p->limited_max_ncpu, p->limited_max_freq);

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
			cpufreq_dbg
			    ("%s(): freq found, idx = %d, target_khz = %d, online_cpu = %d\n",
			     __func__, i, target_khz, online_cpu);
		} else {
			target_khz = p->limited_max_freq;
			cpufreq_dbg("%s(): freq not found, set to limited_max_freq = %d\n",
				    __func__, target_khz);
		}
	} else {
		hps_get_tlp(&tlp);

		tlp = tlp / 100 + ((tlp % 100) ? 1 : 0);
		if (tlp > max_tlp)
			tlp = max_tlp;
		else if (tlp < 1)
			tlp = 1;
		tlp--;

		for (i = 0; i < cpu_tlp_power_tbl[tlp].nr_power_table; i++) {
			if (cpu_tlp_power_tbl[tlp].power_tbl[i].ncpu_little == 0)
				continue;
			if (cpu_tlp_power_tbl[tlp].power_tbl[i].power <=
			    p->thermal_protect_limited_power)
				break;
		}

		if (hps_get_num_online_cpus(&online_cpu_little, &online_cpu_big) == -1) {
			online_cpu_big = 2;
			online_cpu_little = 2;
		}
		max_performance = 0;
		max_performance_id = 0;

		for (j = i; j < cpu_tlp_power_tbl[tlp].nr_power_table; j++) {
			if (cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_big == online_cpu_big
			    && cpu_tlp_power_tbl[tlp].power_tbl[j].ncpu_little ==
			    online_cpu_little) {
				found = 1;
				break;
			}
		}

		if (found) {
			if (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) {
				target_khz =
				    cpu_dvfs_get_freq_by_idx(p,
							     cpu_tlp_power_tbl[tlp].
							     power_tbl[j].khz_little);
			} else {
				target_khz =
				    cpu_dvfs_get_freq_by_idx(p,
							     cpu_tlp_power_tbl[tlp].
							     power_tbl[j].khz_big);
			}
			cpufreq_dbg("%s(): freq found, idx = %d, target_khz = %d, "
				, __func__, j, target_khz);
			cpufreq_dbg("online_cpu_little = %d, online_cpu_big = %d\n"
				, online_cpu_little, online_cpu_big);
		} else {
			target_khz = p->limited_max_freq;
			cpufreq_dbg("%s(): freq not found, set to limited_max_freq = %d\n",
				    __func__, target_khz);
		}
	}

	i = _search_available_freq_idx(p, target_khz, CPUFREQ_RELATION_H);

	FUNC_EXIT(FUNC_LV_HELP);

	return (i > new_opp_idx) ? i : new_opp_idx;
}

void interactive_boost_cpu(int boost)
{
	system_boost = boost;

	if (system_boost && _mt_cpufreq_pdrv_probed) {
		_mt_cpufreq_set(MT_CPU_DVFS_LITTLE, 0);
	}
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	int idx;
	unsigned int online_cpu_big = 0, online_cpu_little = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* for ramp down */
	if (0) {
		cpufreq_dbg("%s(): ramp down, idx = %d, freq_old = %d, freq_new = %d\n", __func__,
			    new_opp_idx, cpu_dvfs_get_cur_freq(p), cpu_dvfs_get_freq_by_idx(p,
											    new_opp_idx));
		new_opp_idx = p->idx_opp_tbl;
	}

	if (chip_ver == CHIP_SW_VER_02 && cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)
	    && hps_get_num_online_cpus(&online_cpu_little, &online_cpu_big) != -1
	    && online_cpu_big > 0 && new_opp_idx > 4)
		new_opp_idx = 4;

	/* HEVC */
	if (p->limited_freq_by_hevc) {
		idx = _search_available_freq_idx(p, p->limited_freq_by_hevc, CPUFREQ_RELATION_L);

		if (idx != -1 && new_opp_idx > idx) {
			new_opp_idx = idx;
			cpufreq_dbg("%s(): hevc limited freq, idx = %d\n", __func__, new_opp_idx);
		}
	}

	if (system_boost) {
		new_opp_idx = 0;
	}
#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)

	if (true == p->downgrade_freq_for_ptpod) {
		if (cpu_dvfs_get_freq_by_idx(p, new_opp_idx) > p->downgrade_freq) {
			idx = _search_available_freq_idx(p, p->downgrade_freq, CPUFREQ_RELATION_H);

			if (idx != -1) {
				new_opp_idx = idx;
				cpufreq_dbg("%s(): downgrade freq, idx = %d\n", __func__,
					    new_opp_idx);
			}
		}
	}
#endif				/* CONFIG_CPU_DVFS_DOWNGRADE_FREQ */

	/* search thermal limited freq */
	idx = _thermal_limited_verify(p, new_opp_idx);

	if (idx != -1) {
		new_opp_idx = idx;
		cpufreq_dbg("%s(): thermal limited freq, idx = %d\n", __func__, new_opp_idx);
	}

	/* for ptpod init */
	if (p->dvfs_disable_by_ptpod) {
		new_opp_idx = 0;	/* cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 0 : p->idx_normal_max_opp; */
		new_opp_idx = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 5 : 6;
		cpufreq_dbg("%s(): for ptpod init, idx = %d\n", __func__, new_opp_idx);
	}

	/* for early suspend */
	if (p->dvfs_disable_by_early_suspend) {
		new_opp_idx = p->idx_normal_max_opp;
		cpufreq_dbg("%s(): for early suspend, idx = %d\n", __func__, new_opp_idx);
	}

	/* for suspend */
	if (p->cpufreq_pause)
		new_opp_idx = p->idx_normal_max_opp;

	/* for power throttling */
	if (p->pwr_thro_mode & (PWR_THRO_MODE_BAT_OC_806MHZ)) {
		if (new_opp_idx < CPU_DVFS_OPPIDX_806MHZ)
			cpufreq_dbg("%s(): for power throttling = %d\n", __func__,
				    CPU_DVFS_OPPIDX_806MHZ);
		new_opp_idx =
		    (new_opp_idx < CPU_DVFS_OPPIDX_806MHZ) ? CPU_DVFS_OPPIDX_806MHZ : new_opp_idx;
	} else if (p->pwr_thro_mode & (PWR_THRO_MODE_LBAT_1365MHZ | PWR_THRO_MODE_BAT_PER_1365MHZ)) {
		if (new_opp_idx < CPU_DVFS_OPPIDX_1365MHZ)
			cpufreq_dbg("%s(): for power throttling = %d\n", __func__,
				    CPU_DVFS_OPPIDX_1365MHZ);
		new_opp_idx =
		    (new_opp_idx < CPU_DVFS_OPPIDX_1365MHZ) ? CPU_DVFS_OPPIDX_1365MHZ : new_opp_idx;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

#if 0				/* mt8173-TBD, defined by not used */
static void bat_per_protection_powerlimit(BATTERY_PERCENT_LEVEL level)
{
	/* struct cpufreq_policy *policy; */
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		cpufreq_lock(flags);

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl_for_pwr_thro = p->idx_opp_tbl;

		switch (level) {
		case BATTERY_PERCENT_LEVEL_1:
			/* Trigger CPU Limit to under CA7 x 4 + 1.36GHz */
			p->pwr_thro_mode |= PWR_THRO_MODE_BAT_PER_1365MHZ;
			break;

		default:
			/* unlimit cpu and gpu */
			p->pwr_thro_mode &= ~PWR_THRO_MODE_BAT_PER_1365MHZ;
			break;
		}

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl = p->idx_opp_tbl_for_pwr_thro;

		cpufreq_unlock(flags);

		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE :
				MT_CPU_DVFS_BIG, p->idx_opp_tbl);

		switch (level) {
		case BATTERY_PERCENT_LEVEL_1:
			/* Trigger CPU Limit to under CA7 x 4 + 1.36GHz */
			/* hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 0); *//* TODO: FIXME */
			break;

		default:
			/* unlimit cpu and gpu */
			/* hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 4); *//* TODO: FIXME */
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
		if (!cpu_dvfs_is_available(p) || !cpu_dvfs_is(p, MT_CPU_DVFS_BIG))	/* just for big */
			continue;

		cpufreq_lock(flags);

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl_for_pwr_thro = p->idx_opp_tbl;

		switch (level) {
		case BATTERY_OC_LEVEL_1:
			/* battery OC trigger CPU Limit to under CA17 x 4 + 0.8G */
			p->pwr_thro_mode |= PWR_THRO_MODE_BAT_OC_806MHZ;
			break;

		default:
			/* unlimit cpu and gpu */
			p->pwr_thro_mode &= ~PWR_THRO_MODE_BAT_OC_806MHZ;
			break;
		}

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl = p->idx_opp_tbl_for_pwr_thro;

		cpufreq_unlock(flags);

		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE :
				MT_CPU_DVFS_BIG, p->idx_opp_tbl);
	}
}

void Lbat_protection_powerlimit(LOW_BATTERY_LEVEL level)
{
	/* struct cpufreq_policy *policy; */
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		cpufreq_lock(flags);

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl_for_pwr_thro = p->idx_opp_tbl;

		switch (level) {
		case LOW_BATTERY_LEVEL_1:
			/* 1st LV trigger CPU Limit to under CA7 x 4 */
			p->pwr_thro_mode &= ~PWR_THRO_MODE_LBAT_1365MHZ;
			break;

		case LOW_BATTERY_LEVEL_2:
			/* 2nd LV trigger CPU Limit to under CA7 x 4 + 1.36G */
			p->pwr_thro_mode |= PWR_THRO_MODE_LBAT_1365MHZ;
			break;

		default:
			/* unlimit cpu and gpu */
			p->pwr_thro_mode &= ~PWR_THRO_MODE_LBAT_1365MHZ;
			break;
		}

		if (!p->pwr_thro_mode)
			p->idx_opp_tbl = p->idx_opp_tbl_for_pwr_thro;

		cpufreq_unlock(flags);

		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE :
				MT_CPU_DVFS_BIG, p->idx_opp_tbl);

		switch (level) {
		case LOW_BATTERY_LEVEL_1:
			/* 1st LV trigger CPU Limit to under CA7 x 4 */
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 0);
			break;

		case LOW_BATTERY_LEVEL_2:
			/* 2nd LV trigger CPU Limit to under CA7 x 4 + 1.36G */
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 0);
			break;

		default:
			/* unlimit cpu and gpu */
			hps_set_cpu_num_limit(LIMIT_LOW_BATTERY, 4, 4);
			break;
		}
	}
}
#endif

/*
 * cpufreq driver
 */
static int _mt_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct mt_cpu_dvfs *p;
	int ret = 0;		/* cpufreq_frequency_table_verify() always return 0 */

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	BUG_ON(NULL == p);

	ret = cpufreq_frequency_table_verify(policy, p->freq_tbl_for_cpufreq);

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_target(struct cpufreq_policy *policy, unsigned int target_freq,
			      unsigned int relation)
{
	/* move to _cpufreq_set_locked() */
	/*
	   unsigned int cpu;
	   struct cpufreq_freqs freqs;
	 */
	unsigned int new_opp_idx;

	enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);

	/* unsigned long flags;                                                 *//* XXX: move to _mt_cpufreq_set() */
	int ret = 0;		/* -EINVAL; */

	FUNC_ENTER(FUNC_LV_MODULE);

	if (policy->cpu >= num_possible_cpus()	/* TODO: FIXME */
		|| cpufreq_frequency_table_target(policy, id_to_cpu_dvfs(id)->freq_tbl_for_cpufreq,
			target_freq, relation, &new_opp_idx)
	    || (id_to_cpu_dvfs(id) && id_to_cpu_dvfs(id)->is_fixed_freq)
	    )
		return -EINVAL;

	/* move to _cpufreq_set_locked() */
	/*
	   freqs.old = policy->cur;
	   freqs.new = mt_cpufreq_max_frequency_by_DVS(id, new_opp_idx);
	   freqs.cpu = policy->cpu;

	   for_each_online_cpu(cpu) {
	   freqs.cpu = cpu;
	   cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
	   }
	 */

	/* move to _mt_cpufreq_set() */
	/* cpufreq_lock(flags); */

	_mt_cpufreq_set(id, new_opp_idx);

	/* move to _mt_cpufreq_set() */
	/* cpufreq_unlock(flags); */

	/* move to _cpufreq_set_locked() */
	/*
	   for_each_online_cpu(cpu) {
	   freqs.cpu = cpu;
	   cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
	   }
	 */

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret = -EINVAL;

	FUNC_ENTER(FUNC_LV_MODULE);

	if (policy->cpu >= num_possible_cpus())	/* TODO: FIXME */
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
		int lv = read_efuse_speed(id);	/* i.e. g_cpufreq_get_ptp_level */
		struct opp_tbl_info *opp_tbl_info =
		    (MT_CPU_DVFS_BIG ==
		     id) ? &opp_tbls_big[CPU_LV_TO_OPP_IDX(lv)] :
		    &opp_tbls_little[CPU_LV_TO_OPP_IDX(lv)];

		BUG_ON(NULL == p);
		BUG_ON(!(lv == CPU_LEVEL_1 || lv == CPU_LEVEL_2
			 || lv == CPU_LEVEL_3 || lv == CPU_LEVEL_4 || lv == CPU_LEVEL_5));

		ret = _mt_cpufreq_setup_freqs_table(policy,
						    opp_tbl_info->opp_tbl, opp_tbl_info->size);

		policy->cpuinfo.max_freq = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->cpuinfo.min_freq = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		policy->cur = _mt_cpufreq_get_cur_phy_freq(id);	/* use cur phy freq is better */
		policy->max = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->min = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		if (_sync_opp_tbl_idx(p) >= 0)	/* sync p->idx_opp_tbl first before _restore_default_volt() */
			p->idx_normal_max_opp = p->idx_opp_tbl;

		/* restore default volt, sync opp idx, set default limit */
		/* _restore_default_volt(p); */

		_set_no_limited(p);
#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)
		_init_downgrade(p, read_efuse_speed(id));
#endif
#if 0				/* mt8173-TBD, wait pmic_mt6397.c ready */
		if (0 == policy->cpu) {
			register_battery_percent_notify(&bat_per_protection_powerlimit,
							BATTERY_PERCENT_PRIO_CPU_L);
			register_battery_oc_notify(&bat_oc_protection_powerlimit,
						   BATTERY_OC_PRIO_CPU_L);
			register_low_battery_notify(&Lbat_protection_powerlimit,
						    LOW_BATTERY_PRIO_CPU_L);
		}
#endif
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

#define FIX_FREQ_WHEN_SCREEN_OFF	1
#if FIX_FREQ_WHEN_SCREEN_OFF
static bool _allow_dpidle_ctrl_vproc;
#else
/* return _allow_dpidle_ctrl_vproc; */
#define VPROC_THRESHOLD_TO_DEEPIDLE	990
#endif
bool mt_cpufreq_earlysuspend_status_get(void)
{
#if FIX_FREQ_WHEN_SCREEN_OFF
	return _allow_dpidle_ctrl_vproc;
#else
	int	ret = 0;

	if (regulator_vpca53_voltage && (regulator_vpca53_voltage < VPROC_THRESHOLD_TO_DEEPIDLE))
		ret = 1;

	return ret;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_earlysuspend_status_get);

static void _mt_cpufreq_lcm_status_switch(int onoff)
{
	struct mt_cpu_dvfs *p;
	int i;
	#ifdef CONFIG_CPU_FREQ
	struct cpufreq_policy *policy;
	#endif

	if (!_mt_cpufreq_pdrv_probed)
		return;
	cpufreq_info("@%s: LCM is %s\n", __func__, (onoff) ? "on" : "off");

	/* onoff = 0: LCM OFF */
	/* others: LCM ON */
	if (onoff) {
		_allow_dpidle_ctrl_vproc = false;

		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
				continue;

			p->dvfs_disable_by_early_suspend = false;


#ifdef CONFIG_CPU_FREQ
				policy = cpufreq_cpu_get(p->cpu_id);

				if (policy) {
					cpufreq_driver_target(
						policy,
						cpu_dvfs_get_freq_by_idx(
							p,
							p->idx_opp_tbl_for_late_resume
						),
						CPUFREQ_RELATION_L
					);
					cpufreq_cpu_put(policy);
				}
#endif
		}
	} else {
		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
				continue;

			p->dvfs_disable_by_early_suspend = true;

			p->idx_opp_tbl_for_late_resume = p->idx_opp_tbl;

#ifdef CONFIG_CPU_FREQ
				policy = cpufreq_cpu_get(p->cpu_id);

				if (policy) {
					cpufreq_driver_target(
						policy,
						cpu_dvfs_get_normal_max_freq(p), CPUFREQ_RELATION_L);
					cpufreq_cpu_put(policy);
				}
#endif
		}
		_allow_dpidle_ctrl_vproc = true;
	}
}

/* (early-)suspend / (late-)resume */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void _mt_cpufreq_early_suspend(struct early_suspend *h)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	_mt_cpufreq_lcm_status_switch(0);

	FUNC_EXIT(FUNC_LV_MODULE);
}

static void _mt_cpufreq_late_resume(struct early_suspend *h)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	_mt_cpufreq_lcm_status_switch(1);

	FUNC_EXIT(FUNC_LV_MODULE);
}
#else
static int _mt_cpufreq_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* skip if it's not a blank event */
	if (event != FB_EVENT_BLANK)
		return 0;

	if (evdata == NULL)
		return 0;
	if (evdata->data == NULL)
		return 0;

	blank = *(int *)evdata->data;

	cpufreq_ver("@%s: blank = %d, event = %lu\n", __func__, blank, event);

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		_mt_cpufreq_lcm_status_switch(1);
		break;
	/* LCM OFF */
	case FB_BLANK_POWERDOWN:
		_mt_cpufreq_lcm_status_switch(0);
		break;
	default:
		break;
	}

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

#endif				/* CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend _mt_cpufreq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend = _mt_cpufreq_early_suspend,
	.resume = _mt_cpufreq_late_resume,
};
#else
static struct notifier_block _mt_cpufreq_fb_notifier = {
	.notifier_call = _mt_cpufreq_fb_notifier_callback,
};
#endif				/* CONFIG_HAS_EARLYSUSPEND */

static struct freq_attr *_mt_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver _mt_cpufreq_driver = {
	.verify = _mt_cpufreq_verify,
	.target = _mt_cpufreq_target,
	.init = _mt_cpufreq_init,
	.get = _mt_cpufreq_get,
	.name = "mt-cpufreq",
	.attr = _mt_cpufreq_attr,
	.flags = CPUFREQ_HAVE_GOVERNOR_PER_POLICY|CPUFREQ_ASYNC_NOTIFICATION,
};

/*
 * Platform driver
 */
static int _mt_cpufreq_suspend(struct device *dev)
{
	/* struct cpufreq_policy *policy; */
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_SUSPEND); *//* TODO: move to suspend driver */

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		p->cpufreq_pause = true;

#if 0				/* XXX: cpufreq_driver_target doesn't work @ suspend */
		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p),
					      CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		}
#else
		/* XXX: useless, decided @ _calc_new_opp_idx() */
		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE :
				MT_CPU_DVFS_BIG, p->idx_normal_max_opp);
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

	/* mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); *//* TODO: move to suspend driver */

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		p->cpufreq_pause = false;
	}

	/* TODO: set big/LITTLE voltage??? */

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pm_restore_early(struct device *dev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	_mt_cpufreq_sync_opp_tbl_idx();

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtcpufreq_of_match[] = {
	{.compatible = "mediatek,mt8173-cpufreq",},
	{},
};

MODULE_DEVICE_TABLE(of, mtcpufreq_of_match);

void __iomem *mtcpufreq_base;

#endif

static int _mt_cpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_MODULE);

	if (pw.addr[0].cmd_addr == 0)
		_mt_cpufreq_pmic_table_init();

#ifdef CONFIG_OF
	reg_vpca53 = devm_regulator_get(&pdev->dev, "reg-vpca53");
	if (IS_ERR(reg_vpca53)) {
		ret = PTR_ERR(reg_vpca53);
		dev_err(&pdev->dev, "Failed to request reg-vpca53: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(reg_vpca53);

	reg_ext_vpca57 = devm_regulator_get(&pdev->dev, "reg-ext-vpca57");
	if (IS_ERR(reg_ext_vpca57)) {
		ret = PTR_ERR(reg_ext_vpca57);
		dev_err(&pdev->dev, "Failed to request reg-ext-vpca57: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(reg_ext_vpca57);

	reg_vsramca57 = devm_regulator_get(&pdev->dev, "reg-vsramca57");
	if (IS_ERR(reg_vsramca57)) {
		ret = PTR_ERR(reg_vsramca57);
		dev_err(&pdev->dev, "Failed to request reg-vsramca57: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(reg_vsramca57);

	reg_vcore_backup = devm_regulator_get(&pdev->dev, "reg-vcore");
	if (IS_ERR(reg_vcore_backup)) {
		ret = PTR_ERR(reg_vcore_backup);
		dev_err(&pdev->dev, "Failed to request reg-vcore: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(reg_vcore_backup);

	clk_pllca57 = devm_clk_get(&pdev->dev, "mtcpufreq_apmixed_ca15pll");
	if (IS_ERR(clk_pllca57)) {
		ret = PTR_ERR(clk_pllca57);
		dev_err(&pdev->dev, "Failed to request clk_pllca57: %d\n", ret);
		return ret;
	}
#ifdef CLK_PLLCA57_NOT_CRITICAL_CLK
	/* enable clk_pllca57 in probe func only if clk_pllca57 is not critical clk */
	ret = clk_prepare_enable(clk_pllca57);
#endif

#if 0				/* reserved */
	reg_vgpu = devm_regulator_get(&pdev->dev, "reg-vgpu");
	if (IS_ERR(reg_vgpu)) {
		ret = PTR_ERR(reg_vgpu);
		dev_err(&pdev->dev, "Failed to request reg-vgpu: %d\n", ret);
		return ret;
	}
#endif

	/* mt8173 backup VCORE voltage */
	if (regulator_is_enabled(reg_vcore_backup))
		rdata = regulator_get_voltage(reg_vcore_backup) / 1000;
	else
		rdata = 0;
	pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[IDX_DI_VSRAM_CA7_FAST_TRSN_EN].cmd_wdata =
	    VOLT_TO_PMIC_VAL(rdata);
	pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[IDX_DI_VCORE_PDN_NORMAL].cmd_wdata =
	    VOLT_TO_PMIC_VAL(rdata);
	pw.set[PMIC_WRAP_PHASE_SODI]._[IDX_SO_VSRAM_CA7_FAST_TRSN_DIS].cmd_wdata =
	    VOLT_TO_PMIC_VAL(rdata);
	pw.set[PMIC_WRAP_PHASE_SODI]._[IDX_SO_VSRAM_CA7_FAST_TRSN_EN].cmd_wdata =
	    VOLT_TO_PMIC_VAL(rdata);

#endif

	/* TODO: check extBuck init with James */
	/* init static power table */
	/* mt8173-TBD, calculate power budget by formula */
#if 0
	mt_spower_init();
#endif

	/* register early suspend */
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&_mt_cpufreq_early_suspend_handler);
#else
	if (fb_register_client(&_mt_cpufreq_fb_notifier)) {
		cpufreq_err("@%s: register FB client failed!\n", __func__);
		return 0;
	}
#endif

	/* init PMIC_WRAP & volt */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#if 0				/* TODO: FIXME */
	/* restore default volt, sync opp idx, set default limit */
	{
		struct mt_cpu_dvfs *p;
		int i;

		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
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
	register_hotcpu_notifier(&turbo_mode_cpu_notifier);	/* <-XXX */
	register_hotcpu_notifier(&extbuck_cpu_notifier);

	_mt_cpufreq_pdrv_probed = 1;

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_remove(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	unregister_hotcpu_notifier(&extbuck_cpu_notifier);
	unregister_hotcpu_notifier(&turbo_mode_cpu_notifier);	/* <-XXX */
	cpufreq_unregister_driver(&_mt_cpufreq_driver);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static const struct dev_pm_ops _mt_cpufreq_pm_ops = {
	.suspend = _mt_cpufreq_suspend,
	.resume = _mt_cpufreq_resume,
	.restore_early = _mt_cpufreq_pm_restore_early,
	.freeze = _mt_cpufreq_suspend,
	.thaw = _mt_cpufreq_resume,
	.restore = _mt_cpufreq_resume,
};

static struct platform_driver _mt_cpufreq_pdrv = {
	.probe = _mt_cpufreq_pdrv_probe,
	.remove = _mt_cpufreq_pdrv_remove,
	.driver = {
		   .name = "mt-cpufreq",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(mtcpufreq_of_match),
#endif
		   .pm = &_mt_cpufreq_pm_ops,
		   .owner = THIS_MODULE,
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

	/* CTP */
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
		info = &opp_tbls_little[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)];
		break;

	case MT_CPU_DVFS_BIG:
	default:
		info = &opp_tbls_big[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)];
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

/* APB Module ca15l_config */
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
	pminit_write(CLK26CALI_0, clk26cali_0 | 0x80);	/* enable fmeter_en */

	clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
	pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00);	/* select divider */

	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	pminit_write(CLK_CFG_8, (46 << 8));	/* select abist_cksw */

	top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
	pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFF3) | (0x1 << 2));

	top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
	pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFC1F) | (0xb << 5));

	ca15l_mon_sel = DRV_Reg32(CA15L_MON_SEL);
	DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel | 0x00000500);

	ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x10000000);

	temp = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, temp | 0x1);	/* start fmeter */

	/* wait frequency meter finish */
	while (DRV_Reg32(CLK26CALI_0) & 0x1) {
		printf("wait for frequency meter finish, CLK26CALI = 0x%x\n",
		       DRV_Reg32(CLK26CALI_0));
		/* mdelay(10); */
	}

	temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000) / 1024) * 4;	/* Khz */

	pminit_write(CLK_CFG_8, clk_cfg_8);
	pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
	pminit_write(CLK26CALI_0, clk26cali_0);
	pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
	pminit_write(TOP_CKDIV1, top_ckdiv1);
	DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

	/* print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output); */

	return output;
}

static unsigned int _mt_get_smallcpu_freq(void)
{
	int output = 0;
	unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1, clk26cali_1;
	unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl;

	clk26cali_0 = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, clk26cali_0 | 0x80);	/* enable fmeter_en */

	clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
	pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00);	/* select divider */

	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	pminit_write(CLK_CFG_8, (46 << 8));	/* select armpll_occ_mon */

	top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
	pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFFC) | 0x1);

	top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
	pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFFE0) | 0xb);

	ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x08100000);

	temp = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, temp | 0x1);	/* start fmeter */

	/* wait frequency meter finish */
	while (DRV_Reg32(CLK26CALI_0) & 0x1) {
		printf("wait for frequency meter finish, CLK26CALI = 0x%x\n",
		       DRV_Reg32(CLK26CALI_0));
		/* mdelay(10); */
	}

	temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000) / 1024) * 4;	/* Khz */

	pminit_write(CLK_CFG_8, clk_cfg_8);
	pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
	pminit_write(CLK26CALI_0, clk26cali_0);
	pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
	pminit_write(TOP_CKDIV1, top_ckdiv1);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

	/* print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output); */

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

void dvfs_set_vcore_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_NORMAL, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_DI_VCORE_NORMAL);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
}

static unsigned int little_freq_backup;
static unsigned int big_freq_backup;
static unsigned int vgpu_backup;
static unsigned int vcore_backup;

void dvfs_disable_by_ptpod(void)
{
	cpufreq_dbg("%s()\n", __func__);	/* <-XXX */
	little_freq_backup = _mt_cpufreq_get_cur_phy_freq(MT_CPU_DVFS_LITTLE);
	big_freq_backup = _mt_cpufreq_get_cur_phy_freq(MT_CPU_DVFS_BIG);
	pmic_read_interface(PMIC_ADDR_VGPU_VOSEL_ON, &vgpu_backup, 0x7F, 0);
	pmic_read_interface(PMIC_ADDR_VCORE_AO_VOSEL_ON, &vcore_backup, 0x7F, 0);

	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_LITTLE, 1140000);
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_BIG, 1140000);
	dvfs_set_gpu_volt(0x30);
	dvfs_set_vcore_volt(0x38);
}

void dvfs_enable_by_ptpod(void)
{
	cpufreq_dbg("%s()\n", __func__);	/* <-XXX */
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_LITTLE, little_freq_backup);
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_BIG, big_freq_backup);
	dvfs_set_gpu_volt(vgpu_backup);
	dvfs_set_vcore_volt(vcore_backup);
}
#endif				/* ! __KERNEL__ */

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

/* cpufreq_stress */
static int cpufreq_stress_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cpufreq stress (log level) = %d\n", stress_test);

	return 0;
}

static ssize_t cpufreq_stress_proc_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{
	int para1;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &para1))
		stress_test = para1;
	else
		cpufreq_err("echo stress_test > /proc/cpufreq/cpufreq_stress\n");

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_debug */
static int cpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cpufreq debug (log level) = %d\n"
		   "cpufreq debug (ptp level) = [%d, %d]\n", func_lv_mask,
		   read_efuse_speed(MT_CPU_DVFS_LITTLE), read_efuse_speed(MT_CPU_DVFS_BIG)
	    );

	return 0;
}

static ssize_t cpufreq_debug_proc_write(struct file *file, const char __user *buffer, size_t count,
					loff_t *pos)
{
	int dbg_lv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &dbg_lv))
		func_lv_mask = dbg_lv;
	else
		cpufreq_err("echo dbg_lv (dec) > /proc/cpufreq/cpufreq_debug\n");

	free_page((unsigned long)buf);
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
		   p->ptpod_temperature_limit_2 * p->ptpod_temperature_time_2, p->over_max_cpu);

	return 0;
}

/* cpufreq_downgrade_freq_counter_limit */
static int cpufreq_downgrade_freq_counter_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->downgrade_freq_counter_limit);

	return 0;
}

static ssize_t cpufreq_downgrade_freq_counter_limit_proc_write(struct file *file,
							       const char __user *buffer,
							       size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int downgrade_freq_counter_limit;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &downgrade_freq_counter_limit))
		p->downgrade_freq_counter_limit = downgrade_freq_counter_limit;
	else
		cpufreq_err("echo downgrade_freq_counter_limit (dec)\n");
	cpufreq_err("> /proc/cpufreq/%s/cpufreq_downgrade_freq_counter_limit\n", p->name);

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_downgrade_freq_counter_return_limit */
static int cpufreq_downgrade_freq_counter_return_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->downgrade_freq_counter_return_limit);

	return 0;
}

static ssize_t cpufreq_downgrade_freq_counter_return_limit_proc_write(struct file *file,
								      const char __user *buffer,
								      size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int downgrade_freq_counter_return_limit;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &downgrade_freq_counter_return_limit))
		/* TODO: p->ptpod_temperature_limit_1 * p->ptpod_temperature_time_1
		   or p->ptpod_temperature_limit_2 * p->ptpod_temperature_time_2 */
		p->downgrade_freq_counter_return_limit = downgrade_freq_counter_return_limit;
	else
		cpufreq_err("echo downgrade_freq_counter_return_limit (dec)\n");
	cpufreq_err(" > /proc/cpufreq/%s/cpufreq_downgrade_freq_counter_return_limit\n", p->name);

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_fftt_test */
#include <linux/sched_clock.h>

static unsigned long _delay_us;
static unsigned long long _delay_us_buf;

static int cpufreq_fftt_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", _delay_us);

	if (_delay_us < _delay_us_buf)
		cpufreq_err("@%s(), %lu < %llu, loops_per_jiffy = %lu\n", __func__, _delay_us,
			    _delay_us_buf, loops_per_jiffy);

	return 0;
}

static ssize_t cpufreq_fftt_test_proc_write(struct file *file, const char __user *buffer,
					    size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoull(buf, 10, &_delay_us_buf)) {
		unsigned long start;

		start = (unsigned long)sched_clock();
		udelay(_delay_us_buf);
		_delay_us = ((unsigned long)sched_clock() - start) / 1000;

		cpufreq_ver("@%s(%llu), _delay_us = %lu, loops_per_jiffy = %lu\n", __func__,
			    _delay_us_buf, _delay_us, loops_per_jiffy);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_limited_by_hevc */
static int cpufreq_limited_by_hevc_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->limited_freq_by_hevc);

	return 0;
}

static ssize_t cpufreq_limited_by_hevc_proc_write(struct file *file,
						  const char __user *buffer, size_t count,
						  loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int limited_freq_by_hevc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &limited_freq_by_hevc)) {
		p->limited_freq_by_hevc = limited_freq_by_hevc;

		if (cpu_dvfs_is_available(p)
		    && (p->limited_freq_by_hevc > cpu_dvfs_get_cur_freq(p))) {
			struct cpufreq_policy *policy = cpufreq_cpu_get(p->cpu_id);

			if (policy) {
				cpufreq_driver_target(policy, p->limited_freq_by_hevc,
						      CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy);
			}
		}
	} else
		cpufreq_err
		    ("echo limited_freq_by_hevc (dec) > /proc/cpufreq/%s/cpufreq_limited_by_hevc\n",
		     p->name);

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_limited_power */
static int cpufreq_limited_power_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		seq_printf(m, "[%s/%d] %d\n"
			   "limited_max_freq = %d\n"
			   "limited_max_ncpu = %d\n"
			   "all_limited_power[0]=%d\n"
			   "all_limited_power[1]=%d\n",
			   p->name, i, p->thermal_protect_limited_power,
			   p->limited_max_freq, p->limited_max_ncpu,
			   all_limited_power[0], all_limited_power[1]);
	}

	return 0;
}

static ssize_t cpufreq_limited_power_proc_write(struct file *file, const char __user *buffer,
						size_t count, loff_t *pos)
{
	int i, tlp, limited_power;
	unsigned int max_tlp = num_possible_cpus();
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &limited_power)) {
		if (0) {	/* (limited_power == -1) { */
			hps_get_tlp(&tlp);
			tlp = tlp / 100 + ((tlp % 100) ? 1 : 0);
			if (tlp > max_tlp)
				tlp = max_tlp;
			else if (tlp < 1)
				tlp = 1;
			tlp--;
			for (i = 0; i < cpu_tlp_power_tbl[tlp].nr_power_table; i++) {
				mt_cpufreq_thermal_protect(cpu_tlp_power_tbl[tlp].
							   power_tbl[i].power, 0);
				msleep(1000);
			}
		} else {
			mt_cpufreq_thermal_protect(limited_power, 0);	/* TODO: specify limited_power by id??? */
		}
	} else {
		cpufreq_err("echo limited_power (dec) > /proc/cpufreq/cpufreq_limited_power\n");
	}

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_over_max_cpu */
static int cpufreq_over_max_cpu_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->over_max_cpu);

	return 0;
}

static ssize_t cpufreq_over_max_cpu_proc_write(struct file *file, const char __user *buffer,
					       size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int over_max_cpu;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &over_max_cpu))
		p->over_max_cpu = over_max_cpu;
	else
		cpufreq_err("echo over_max_cpu (dec) > /proc/cpufreq/%s/cpufreq_over_max_cpu\n",
			    p->name);

	free_page((unsigned long)buf);
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
				seq_printf(m,
					   "[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d, },\n",
					   j, p->power_tbl[j].cpufreq_khz,
					   p->power_tbl[j].cpufreq_ncpu,
					   p->power_tbl[j].cpufreq_power);
			}
		}
	} else {
		for (i = 0; i < 8; i++) {
			seq_printf(m, "[TLP/%d]\n", i + 1);

			for (j = 0; j < cpu_tlp_power_tbl[i].nr_power_table; j++) {
				seq_printf(m,
					   "[%d] = { .ncpu_big = %d,\t.khz_big = %d,\t.ncpu_little = %d,\t.khz_little = %d,\t.performance = %d,\t.power = %d, chip_ver=%d},\n",
					   j, cpu_tlp_power_tbl[i].power_tbl[j].ncpu_big,
					   cpu_tlp_power_tbl[i].power_tbl[j].khz_big,
					   cpu_tlp_power_tbl[i].power_tbl[j].ncpu_little,
					   cpu_tlp_power_tbl[i].power_tbl[j].khz_little,
					   cpu_tlp_power_tbl[i].power_tbl[j].performance,
					   cpu_tlp_power_tbl[i].power_tbl[j].power, chip_ver);
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
		seq_printf(m,
			   "[%d] = { .cpufreq_khz = %d,\t.cpufreq_volt = %d,\t.cpufreq_volt_org = %d, },\n",
			   j, p->opp_tbl[j].cpufreq_khz, p->opp_tbl[j].cpufreq_volt,
			   p->opp_tbl[j].cpufreq_volt_org);
	}

	return 0;
}

/* cpufreq_ptpod_temperature_limit */
static int cpufreq_ptpod_temperature_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "ptpod_temperature_limit_1 = %d\n"
		   "ptpod_temperature_limit_2 = %d\n",
		   p->ptpod_temperature_limit_1, p->ptpod_temperature_limit_2);

	return 0;
}

static ssize_t cpufreq_ptpod_temperature_limit_proc_write(struct file *file,
							  const char __user *buffer, size_t count,
							  loff_t *pos)
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
		cpufreq_err
		    ("echo ptpod_temperature_limit_1 (dec) ptpod_temperature_limit_2 (dec)\n");
	cpufreq_err(" > /proc/cpufreq/%s/cpufreq_ptpod_temperature_limit\n", p->name);

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_ptpod_temperature_time */
static int cpufreq_ptpod_temperature_time_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "ptpod_temperature_time_1 = %d\n"
		   "ptpod_temperature_time_2 = %d\n",
		   p->ptpod_temperature_time_1, p->ptpod_temperature_time_2);

	return 0;
}

static ssize_t cpufreq_ptpod_temperature_time_proc_write(struct file *file,
							 const char __user *buffer, size_t count,
							 loff_t *pos)
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
	} else {
		cpufreq_err("echo ptpod_temperature_time_1 (dec) ptpod_temperature_time_2 (dec)\n");
		cpufreq_err(" > /proc/cpufreq/%s/cpufreq_ptpod_temperature_time\n", p->name);
	}

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_ptpod_test */
static int cpufreq_ptpod_test_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cpufreq_ptpod_test_proc_write(struct file *file, const char __user *buffer,
					     size_t count, loff_t *pos)
{
	return count;
}

/* cpufreq_state */
static int cpufreq_state_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n" "cpufreq_pause = %d\n", p->name, i, p->cpufreq_pause);
	}

	return 0;
}

static ssize_t cpufreq_state_proc_write(struct file *file, const char __user *buffer, size_t count,
					loff_t *pos)
{
	int enable;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &enable))
		mt_cpufreq_state_set(enable);
	else
		cpufreq_err("echo 1/0 > /proc/cpufreq/cpufreq_state\n");

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_oppidx */
static int cpufreq_oppidx_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int j;

	seq_printf(m, "[%s/%d]\n" "cpufreq_oppidx = %d\n", p->name, p->cpu_id, p->idx_opp_tbl);

	for (j = 0; j < p->nr_opp_tbl; j++) {
		seq_printf(m, "\tOP(%d, %d),\n",
			   cpu_dvfs_get_freq_by_idx(p, j), cpu_dvfs_get_volt_by_idx(p, j));
	}

	return 0;
}

static ssize_t cpufreq_oppidx_proc_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int oppidx;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);

	if (!kstrtoint(buf, 10, &oppidx) && 0 <= oppidx && oppidx < p->nr_opp_tbl) {
		p->is_fixed_freq = true;
		_mt_cpufreq_set(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? MT_CPU_DVFS_LITTLE :
				MT_CPU_DVFS_BIG, oppidx);
	} else {
		p->is_fixed_freq = false;	/* TODO: FIXME */
		cpufreq_err("echo oppidx > /proc/cpufreq/%s/cpufreq_oppidx (0 <= %d < %d)\n",
			    p->name, oppidx, p->nr_opp_tbl);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_freq */
static int cpufreq_freq_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->ops->get_cur_phy_freq(p));

	return 0;
}

static ssize_t cpufreq_freq_proc_write(struct file *file, const char __user *buffer, size_t count,
				       loff_t *pos)
{				/* <-XXX */
	unsigned long flags;
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	unsigned int cur_freq;
	int freq;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);

	if (!kstrtoint(buf, 10, &freq)) {
		p->is_fixed_freq = true;	/* TODO: FIXME */
		cpufreq_lock(flags);	/* <-XXX */
		cur_freq = p->ops->get_cur_phy_freq(p);
		p->ops->set_cur_freq(p, cur_freq, freq);
		cpufreq_unlock(flags);	/* <-XXX */
	} else {
		p->is_fixed_freq = false;	/* TODO: FIXME */
		cpufreq_err("echo khz > /proc/cpufreq/%s/cpufreq_freq\n", p->name);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_volt */
static int cpufreq_volt_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "%d\n", p->ops->get_cur_volt(p));

	return 0;
}

static ssize_t cpufreq_volt_proc_write(struct file *file, const char __user *buffer, size_t count,
				       loff_t *pos)
{				/* <-XXX */
	unsigned long flags;
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int mv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &mv)) {
		p->is_fixed_freq = true;	/* TODO: FIXME */
		cpufreq_lock(flags);
		_set_cur_volt_locked(p, mv);
		cpufreq_unlock(flags);
	} else {
		p->is_fixed_freq = false;	/* TODO: FIXME */
		cpufreq_err("echo mv > /proc/cpufreq/%s/cpufreq_volt\n", p->name);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_turbo_mode */
static int cpufreq_turbo_mode_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int i;

	seq_printf(m, "turbo_mode = %d\n", p->turbo_mode);

	for (i = 0; i < NR_TURBO_MODE; i++) {
		seq_printf(m, "[%d] = { .temp = %d, .freq_delta = %d, .volt_delta = %d }\n",
			   i,
			   turbo_mode_cfg[i].temp,
			   turbo_mode_cfg[i].freq_delta, turbo_mode_cfg[i].volt_delta);
	}

	return 0;
}

static ssize_t cpufreq_turbo_mode_proc_write(struct file *file, const char __user *buffer,
					     size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	unsigned int turbo_mode;
	int temp;
	int freq_delta;
	int volt_delta;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if ((sscanf(buf, "%d %d %d %d", &turbo_mode, &temp, &freq_delta, &volt_delta) == 4)
	    && turbo_mode < NR_TURBO_MODE) {
		turbo_mode_cfg[turbo_mode].temp = temp;
		turbo_mode_cfg[turbo_mode].freq_delta = freq_delta;
		turbo_mode_cfg[turbo_mode].volt_delta = volt_delta;
	} else if (!kstrtouint(buf, 10, &turbo_mode))
		p->turbo_mode = turbo_mode;	/* TODO: FIXME */
	else {
		cpufreq_err("echo 0/1 > /proc/cpufreq/%s/cpufreq_turbo_mode\n", p->name);
		cpufreq_err
		    ("echo idx temp freq_delta volt_delta > /proc/cpufreq/%s/cpufreq_turbo_mode\n",
		     p->name);
	}

	free_page((unsigned long)buf);

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

PROC_FOPS_RW(cpufreq_stress);
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
PROC_FOPS_RW(cpufreq_over_max_cpu);
PROC_FOPS_RO(cpufreq_ptpod_freq_volt);
PROC_FOPS_RW(cpufreq_ptpod_temperature_limit);
PROC_FOPS_RW(cpufreq_ptpod_temperature_time);
PROC_FOPS_RW(cpufreq_oppidx);	/* <-XXX */
PROC_FOPS_RW(cpufreq_freq);	/* <-XXX */
PROC_FOPS_RW(cpufreq_volt);	/* <-XXX */
PROC_FOPS_RW(cpufreq_turbo_mode);	/* <-XXX */

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
		PROC_ENTRY(cpufreq_stress),
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
		PROC_ENTRY(cpufreq_over_max_cpu),
		PROC_ENTRY(cpufreq_ptpod_freq_volt),
		PROC_ENTRY(cpufreq_ptpod_temperature_limit),
		PROC_ENTRY(cpufreq_ptpod_temperature_time),
		PROC_ENTRY(cpufreq_oppidx),	/* <-XXX */
		PROC_ENTRY(cpufreq_freq),	/* <-XXX */
		PROC_ENTRY(cpufreq_volt),	/* <-XXX */
		PROC_ENTRY(cpufreq_turbo_mode),	/* <-XXX */
	};

	dir = proc_mkdir("cpufreq", NULL);

	if (!dir) {
		cpufreq_err("fail to create /proc/cpufreq @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__,
				    entries[i].name);
	}

	for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
		if (!proc_create_data
		    (cpu_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, cpu_entries[i].fops, p))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__,
				    cpu_entries[i].name);
	}

	for_each_cpu_dvfs(j, p) {
		cpu_dir = proc_mkdir(p->name, dir);

		if (!cpu_dir) {
			cpufreq_err("fail to create /proc/cpufreq/%s @ %s()\n", p->name, __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
			if (!proc_create_data
			    (cpu_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, cpu_dir,
			     cpu_entries[i].fops, p))
				cpufreq_err("%s(), create /proc/cpufreq/%s/%s failed\n", __func__,
					    p->name, cpu_entries[i].name);
		}
	}

	return 0;
}
#endif				/* CONFIG_PROC_FS */

#ifdef CONFIG_OF
void add_pwrap_base(void)
{
	int i;

	for (i = 0; i < NR_PMIC_WRAP_CMD; i++) {
		pw.addr[i].cmd_addr += PWRAP_BASE_ADDR;
		pw.addr[i].cmd_wdata += PWRAP_BASE_ADDR;
	}
}

#endif

/*
 * Module driver
 */
static int __init _mt_cpufreq_pdrv_init(void)
{
	int ret = 0;
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	struct device_node *apmix_node = NULL;
	struct device_node *infra_ao_node = NULL;
#endif

	FUNC_ENTER(FUNC_LV_MODULE);

#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-cpufreq");
	if (node) {
		/* Setup IO addresses */
#if 0
		mtcpufreq_base = of_iomap(node, 0);
		if (!mtcpufreq_base) {
			cpufreq_err("mtcpufreq_base = 0x%x\n", mtcpufreq_base);
			return 0;
		}

		cpufreq_err("mtcpufreq_base = 0x%x\n", mtcpufreq_base);
#endif
	}

	apmix_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-apmixedsys");
	if (apmix_node) {
		/* Setup IO addresses */
		apmixed_base = of_iomap(apmix_node, 0);
		if (!apmixed_base) {
			cpufreq_err("apmixed_base = 0x%lx\n", (unsigned long)apmixed_base);
			return 0;
		}

		cpufreq_err("apmixed_base = 0x%lx\n", (unsigned long)apmixed_base);
	}

	infra_ao_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-infracfg");
	if (infra_ao_node) {
		/* Setup IO addresses */
		infracfg_ao_base = of_iomap(infra_ao_node, 0);
		if (!infracfg_ao_base) {
			cpufreq_err("infracfg_ao_base = 0x%lx\n", (unsigned long)infracfg_ao_base);
			return 0;
		}

		cpufreq_err("infracfg_ao_base = 0x%lx\n", (unsigned long)infracfg_ao_base);
	}
#endif

	{
		/* mt8173-TBD, wait mt_boot.c ready, sync with jamesjj mt_idle.c */
		/* chip_ver = mt_get_chip_sw_ver(); */
		cpumask_clear(&cpumask_little);
		cpumask_clear(&cpumask_big);
		arch_get_cluster_cpus(&cpumask_little, 0);
		arch_get_cluster_cpus(&cpumask_big, 1);

		switch (chip_ver) {
		case CHIP_SW_VER_01:
#ifdef MTK_FORCE_CLUSTER1
			int i;

			sched_get_big_little_cpus(&cpumask_little, &cpumask_big);
			for (i = 0; i < pw.set[PMIC_WRAP_PHASE_DEEPIDLE].nr_idx; i++) {
				pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[i].cmd_addr =
				    pw.set[PMIC_WRAP_PHASE_DEEPIDLE_BIG]._[i].cmd_addr;
				pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[i].cmd_wdata =
				    pw.set[PMIC_WRAP_PHASE_DEEPIDLE_BIG]._[i].cmd_wdata;
			}
#endif
			break;

		case CHIP_SW_VER_02:
		default:
			break;
		}

		cpu_dvfs[MT_CPU_DVFS_BIG].cpu_id = cpumask_first(&cpumask_big);
		cpu_dvfs[MT_CPU_DVFS_LITTLE].cpu_id = cpumask_first(&cpumask_little);

		/* mt8173-TBD, how to decide turbo mode ? */
		/* if (CPU_LEVEL_2 != read_efuse_speed()) */
		cpu_dvfs[MT_CPU_DVFS_LITTLE].turbo_mode = 0;
		cpu_dvfs[MT_CPU_DVFS_BIG].turbo_mode = 0;

		cpufreq_err
		    ("@%s():%d, chip_ver = %d, little.cpu_id = %d, big.cpu_id = %d\n",
		     __func__, __LINE__, chip_ver, cpu_dvfs[MT_CPU_DVFS_LITTLE].cpu_id,
		     cpu_dvfs[MT_CPU_DVFS_BIG].cpu_id);
	}

#ifdef CONFIG_PROC_FS

	/* init proc */
	if (_create_procfs())
		goto out;

#endif				/* CONFIG_PROC_FS */

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
#else                          /* CONFIG_CPU_DVFS_BRINGUP */
late_initcall(_mt_cpufreq_pdrv_init);
module_exit(_mt_cpufreq_pdrv_exit);
#endif
MODULE_DESCRIPTION("MediaTek CPU DVFS Driver v0.3");
MODULE_LICENSE("GPL");
