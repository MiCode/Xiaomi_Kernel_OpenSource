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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/stddef.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/system_misc.h>
#include <asm/uaccess.h>
#include <mt-plat/sync_write.h>
#include "mt_dcm.h"
#include "mt_ptp.h"
#include <mach/mt_gpt.h>

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
#include <mach/mt_cpuxgpt.h>
#else
#include <mach/mt_clkmgr.h>
#endif

#include <mach/mt_spm_mtcmos_internal.h>
#include "hotplug.h"
#include "mt_cpufreq.h"
#include "mt_idle.h"
#include "mt_spm.h"
#include "mt_spm_idle.h"

#define IDLE_TAG     "[Power/swap]"
#define idle_warn(fmt, args...)		pr_warn(IDLE_TAG fmt, ##args)
#define idle_dbg(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)

#define idle_warn_log(fmt, args...) { \
	if (dpidle_dump_log == DEEPIDLE_LOG_FULL) \
		pr_warn(IDLE_TAG fmt, ##args); \
	}

#define idle_gpt GPT4

#define idle_readl(addr)			__raw_readl(addr)

#define idle_writel(addr, val)		mt65xx_reg_sync_writel(val, addr)

#define idle_setl(addr, val) \
	mt65xx_reg_sync_writel(idle_readl(addr) | (val), addr)

#define idle_clrl(addr, val) \
	mt65xx_reg_sync_writel(idle_readl(addr) & ~(val), addr)

enum mt_idle_mode {
	MT_DPIDLE = 0,
	MT_SOIDLE,
	MT_SLIDLE,
};

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
enum {
	CG_INFRA   = 0,
	CG_PERI    = 1,
	CG_DISP0   = 2,
	CG_DISP1   = 3,
	CG_IMAGE   = 4,
	CG_MFG     = 5,
	CG_AUDIO   = 6,
	CG_VDEC0   = 7,
	CG_VDEC1   = 8,
	CG_VENC    = 9,
	NR_GRPS    = 10,
};
#endif

#if defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)
static atomic_t is_in_hotplug = ATOMIC_INIT(0);
#endif


#ifdef CONFIG_FPGA_EARLY_PORTING
#undef EN_PTP_OD
#define EN_PTP_OD 0

bool __attribute__((weak)) clkmgr_idle_can_enter(unsigned int *condition_mask, unsigned int *block_mask)
{
	return false;
}
unsigned int __attribute__((weak)) clk_id_to_grp_id(unsigned int id)
{
	return 0;
}
unsigned int __attribute__((weak)) clk_id_to_mask(unsigned int id)
{
	return 0;
}
void __attribute__((weak)) clkmgr_faudintbus_pll2sq(void)
{
}
void __attribute__((weak)) clkmgr_faudintbus_sq2pll(void)
{
}
#endif

static unsigned long rgidle_cnt[NR_CPUS] = {0};
static bool mt_idle_chk_golden;
static bool mt_dpidle_chk_golden;

#define INVALID_GRP_ID(grp) (grp < 0 || grp >= NR_GRPS)

void __attribute__((weak)) bus_dcm_enable(void)
{

}
void __attribute__((weak)) bus_dcm_disable(void)
{

}
void __attribute__((weak)) mt_dcm_topckg_enable(void)
{

}
void __attribute__((weak)) mt_dcm_topckg_disable(void)
{

}
void __attribute__((weak)) tscpu_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) tscpu_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_bts_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_btsmdpa_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pmic_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_battery_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pa_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_wmt_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_bts_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_btsmdpa_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pmic_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_battery_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pa_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_wmt_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts1_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts2_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts3_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts4_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts5_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts1_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts2_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts3_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts4_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts5_timer(void)
{

}

wake_reason_t __attribute__((weak)) spm_go_to_dpidle(u32 spm_flags, u32 spm_data, u32 dump_log)
{
	return 0;
}

void __attribute__((weak)) spm_go_to_sodi(u32 spm_flags, u32 spm_data)
{

}

bool __attribute__((weak)) spm_get_sodi_en(void)
{
	return false;
}

unsigned long __attribute__((weak)) localtimer_get_counter(void)
{
	return 0;
}

int __attribute__((weak)) localtimer_set_next_event(unsigned long evt)
{
	return 0;
}

int __attribute__((weak)) hps_del_timer(void)
{
	return 0;
}

int __attribute__((weak)) hps_restart_timer(void)
{
	return 0;
}

void __attribute__((weak)) MMProfileEnable(int enable)
{

}

void __attribute__((weak)) MMProfileStart(int start)
{

}

void __attribute__((weak)) msdc_clk_status(int *status)
{
	*status = 0x1;
}


enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO,
	IDLE_TYPE_SL,
	IDLE_TYPE_RG,
	NR_TYPES
};

enum {
	BY_CPU = 0,
	BY_CLK,
	BY_TMR,
	BY_OTH,
	BY_VTG,
	NR_REASONS
};

#if defined(CONFIG_ARCH_MT6735)
/* Idle handler on/off */
static int idle_switch[NR_TYPES] = {
	1,  /* dpidle switch */
#if defined(CONFIG_MTK_DISABLE_SODI)
	0,  /* soidle switch */
#else
	1,  /* soidle switch */
#endif
	1,  /* slidle switch */
	1,  /* rgidle switch */
};

static unsigned int dpidle_condition_mask[NR_GRPS] = {
	0x0000008A, /* INFRA: */
	0x37FC1FFD, /* PERI0: */
	0x000FFFFF, /* DISP0: */
	0x0000003C, /* DISP1: */
	0x00000FE1, /* IMAGE: */
	0x00000001, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000001, /* VDEC0: */
	0x00000001, /* VDEC1: */
	0x00001111, /* VENC:  */
};

static unsigned int soidle_condition_mask[NR_GRPS] = {
	0x00000088, /* INFRA: */
	0x37FC0FFC, /* PERI0: */
	0x000033FC, /* DISP0: */
	0x00000030, /* DISP1: */
	0x00000FE1, /* IMAGE: */
	0x00000001, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000001, /* VDEC0: */
	0x00000001, /* VDEC1: */
	0x00001111, /* VENC:  */
};

static unsigned int slidle_condition_mask[NR_GRPS] = {
	0x00000000, /* INFRA: */
	0x07C01000, /* PERI0: */
	0x00000000, /* DISP0: */
	0x00000000, /* DISP1: */
	0x00000000, /* IMAGE: */
	0x00000000, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000000, /* VDEC0: */
	0x00000000, /* VDEC1: */
	0x00000000, /* VENC:  */
};
#elif defined(CONFIG_ARCH_MT6735M)
/* Idle handler on/off */
static int idle_switch[NR_TYPES] = {
	1,  /* dpidle switch */
#if defined(CONFIG_MTK_DISABLE_SODI)
	0,  /* soidle switch */
#else
	1,  /* soidle switch */
#endif
	1,  /* slidle switch */
	1,  /* rgidle switch */
};

static unsigned int dpidle_condition_mask[NR_GRPS] = {
	0x0000008A, /* INFRA: */
	0x37DA1FFD, /* PERI0: */
	0x000FFFFF, /* DISP0: */
	0x0000003F, /* DISP1: */
	0x00000FE1, /* IMAGE: */
	0x00000001, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000001, /* VDEC0: */
	0x00000001, /* VDEC1: */
	/* VENC: there is no venc */
};

static unsigned int soidle_condition_mask[NR_GRPS] = {
	0x00000088, /* INFRA: */
	0x37DC0FFC, /* PERI0: */
	0x000033FC, /* DISP0: */
	0x00000030, /* DISP1: */
	0x00000FE1, /* IMAGE: */
	0x00000001, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000001, /* VDEC0: */
	0x00000001, /* VDEC1: */
	/* VENC: there is no venc */
};

static unsigned int slidle_condition_mask[NR_GRPS] = {
	0x00000000, /* INFRA: */
	0x07C01000, /* PERI0: */
	0x00000000, /* DISP0: */
	0x00000000, /* DISP1: */
	0x00000000, /* IMAGE: */
	0x00000000, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000000, /* VDEC0: */
	0x00000000, /* VDEC1: */
	/* VENC: there is no venc */
};
#elif defined(CONFIG_ARCH_MT6753)
static int idle_switch[NR_TYPES] = {
	1,  /* dpidle switch */
#if defined(CONFIG_MTK_DISABLE_SODI)
	0,  /* soidle switch */
#else
	1,  /* soidle switch */
#endif
	1,  /* slidle switch */
	1,  /* rgidle switch */
};

static unsigned int dpidle_condition_mask[NR_GRPS] = {
	0x0000008A, /* INFRA: */
	0x77FC1FFD, /* PERI0: */
	0x002FFFFF, /* DISP0: */
	0x0000003C, /* DISP1: */
	0x00000FE1, /* IMAGE: */
	0x00000001, /* MFG:   */
	0x00000000, /* AUDIO: */
	0x00000001, /* VDEC0: */
	0x00000001, /* VDEC1: */
	0x00001111, /* VENC:  */
};

static unsigned int soidle_condition_mask[NR_GRPS] = {
	0x00000088, /* INFRA: */
	0x77FC0FFC, /* PERI0: */
	0x000063FC, /* DISP0: */
	0x00000030, /* DISP1: */
	0x00000FE1, /* IMAGE: */
	0x00000001, /* MFG: */
	0x00000000, /* AUDIO: */
	0x00000001, /* VDEC0: */
	0x00000001, /* VDEC1: */
	0x00001111, /* VENC: */
};

static unsigned int slidle_condition_mask[NR_GRPS] = {
	0x00000000, /* INFRA: */
	0x07C01000, /* PERI0: */
	0x00000000, /* DISP0: */
	0x00000000, /* DISP1: */
	0x00000000, /* IMAGE: */
	0x00000000, /* MFG: */
	0x00000000, /* AUDIO: */
	0x00000000, /* VDEC0: */
	0x00000000, /* VDEC1: */
	0x00000000, /* VENC: */
};
#elif defined(CONFIG_ARCH_MT6570)
/*Idle handler on/off*/
static int idle_switch[NR_TYPES] = {
	0,  /* dpidle switch */
	0,  /* soidle switch */
	0,  /* slidle switch */
	1,  /* rgidle switch */
};

static unsigned int dpidle_condition_mask[NR_GRPS] = {
	0x00000000, /* CG_MIXED: */
	0x00000000, /* CG_MPLL: */
	0x00000000, /* CG_UPLL: */
	0x00000037, /* CG_CTRL0: */
	0x8089B2FC, /* CG_CTRL1: */
	0x00003F16, /* CG_CTRL2: */
	0x0007EFFF, /* CG_MMSYS0: */
	0x0000000C, /* CG_MMSYS1: */
	0x000003E1, /* CG_IMGSYS: */
	0x00000001, /* CG_MFGSYS: */
	0x00000000, /* CG_AUDIO: */
	0x00000000, /* CG_INFRA_AO: */
	0x00000006, /* CG_CTRL3: */
};

static unsigned int soidle_condition_mask[NR_GRPS] = {
	0x00000000, /* CG_MIXED: */
	0x00000000, /* CG_MPLL: */
	0x00000000, /* CG_UPLL: */
	0x00000026, /* CG_CTRL0: */
	0x8089B2F8, /* CG_CTRL1: */
	0x00003F06, /* CG_CTRL2: */
	0x00000200, /* CG_MMSYS0: */
	0x00000000, /* CG_MMSYS1: */
	0x000003E1, /* CG_IMGSYS: */
	0x00000001, /* CG_MFGSYS: */
	0x00000000, /* CG_AUDIO: */
	0x00000000, /* CG_INFRA_AO: */
	0x00000000, /* CG_CTRL3: */
};

static unsigned int slidle_condition_mask[NR_GRPS] = {
	0xFFFFFFFF, /* CG_MIXED: */
	0xFFFFFFFF, /* CG_MPLL: */
	0xFFFFFFFF, /* CG_UPLL: */
	0xFFFFFFFF, /* CG_CTRL0: */
	0xFFFFFFFF, /* CG_CTRL1: */
	0xFFFFFFFF, /* CG_CTRL2: */
	0xFFFFFFFF, /* CG_MMSYS0: */
	0xFFFFFFFF, /* CG_MMSYS1: */
	0xFFFFFFFF, /* CG_IMGSYS: */
	0xFFFFFFFF, /* CG_MFGSYS: */
	0xFFFFFFFF, /* CG_AUDIO: */
	0xFFFFFFFF, /* CG_INFRA_AO: */
	0xFFFFFFFF, /* CG_CTRL3: */
};
#elif defined(CONFIG_ARCH_MT6580)
/*Idle handler on/off*/
static int idle_switch[NR_TYPES] = {
	1,  /* dpidle switch */
	1,  /* soidle switch */
	0,  /* slidle switch */
	1,  /* rgidle switch */
};

static unsigned int dpidle_condition_mask[NR_GRPS] = {
	0x00000000, /* CG_MIXED: */
	0x00000000, /* CG_MPLL: */
	0x00000000, /* CG_INFRA_AO: */
	0x00000037, /* CG_CTRL0: */
	0x8089B2FC, /* CG_CTRL1: */
	0x00003F16, /* CG_CTRL2: */
	0x0007EFFF, /* CG_MMSYS0: */
	0x0000000C, /* CG_MMSYS1: */
	0x000003E1, /* CG_IMGSYS: */
	0x00000001, /* CG_MFGSYS: */
	0x00000000, /* CG_AUDIO: */
};

static unsigned int soidle_condition_mask[NR_GRPS] = {
	0x00000000, /* CG_MIXED: */
	0x00000000, /* CG_MPLL: */
	0x00000000, /* CG_INFRA_AO: */
	0x00000026, /* CG_CTRL0: */
	0x8089B2F8, /* CG_CTRL1: */
	0x00003F06, /* CG_CTRL2: */
	0x00000200, /* CG_MMSYS0: */
	0x00000000, /* CG_MMSYS1: */
	0x000003E1, /* CG_IMGSYS: */
	0x00000001, /* CG_MFGSYS: */
	0x00000000, /* CG_AUDIO: */
};

static unsigned int slidle_condition_mask[NR_GRPS] = {
	0xFFFFFFFF, /* CG_MIXED: */
	0xFFFFFFFF, /* CG_MPLL: */
	0xFFFFFFFF, /* CG_INFRA_AO: */
	0xFFFFFFFF, /* CG_CTRL0: */
	0xFFFFFFFF, /* CG_CTRL1: */
	0xFFFFFFFF, /* CG_CTRL2: */
	0xFFFFFFFF, /* CG_MMSYS0: */
	0xFFFFFFFF, /* CG_MMSYS1: */
	0xFFFFFFFF, /* CG_IMGSYS: */
	0xFFFFFFFF, /* CG_MFGSYS: */
	0xFFFFFFFF, /* CG_AUDIO: */
};
#else
#error "Does not support!"
#endif

static const char *idle_name[NR_TYPES] = {
	"dpidle",
	"soidle",
	"slidle",
	"rgidle",
};

static const char *reason_name[NR_REASONS] = {
	"by_cpu",
	"by_clk",
	"by_tmr",
	"by_oth",
	"by_vtg",
};

char cg_group_name[][NR_GRPS] = {
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	"INFRA",
	"PERI",
	"DISP0",
	"DISP1",
	"IMAGE",
	"MFG",
	"AUDIO",
	"VDEC0",
	"VDEC1",
	"VENC",
#else
	"MIXED",
	"MPLL",
	"INFRA_AO",
	"CTRL0",
	"CTRL1",
	"CTRL2",
	"MMSYS0",
	"MMSYS1",
	"IMGSYS",
	"MFGSYS",
	"AUDIO",
#endif
};

static char log_buf[500];
static char log_buf_2[500];

static unsigned long long idle_block_log_prev_time;
static unsigned int idle_block_log_time_criteria = 5000;	/* 5 sec */
static unsigned long long idle_cnt_dump_prev_time;
static unsigned int idle_cnt_dump_criteria = 10000;			/* 10 sec */

static bool             idle_ratio_en;
static unsigned long long idle_ratio_profile_start_time;
static unsigned long long idle_ratio_profile_duration;
static unsigned long long idle_ratio_start_time[NR_TYPES];
static unsigned long long idle_ratio_value[NR_TYPES];

/* Slow Idle */
static unsigned int slidle_block_mask[NR_GRPS] = {0x0};
static unsigned long slidle_cnt[NR_CPUS] = {0};
static unsigned long slidle_block_cnt[NR_REASONS] = {0};

/* SODI */
static unsigned int     soidle_block_mask[NR_GRPS] = {0x0};
static unsigned int     soidle_timer_left;
static unsigned int     soidle_timer_left2;
#ifndef CONFIG_SMP
static unsigned int     soidle_timer_cmp;
#endif
static unsigned int     soidle_time_critera = 26000;
static unsigned int     soidle_block_time_critera = 30000; /* default 30sec */
static unsigned long    soidle_cnt[NR_CPUS] = {0};
static unsigned long    soidle_last_cnt[NR_CPUS] = {0};
static unsigned long    soidle_block_cnt[NR_CPUS][NR_REASONS] = { {0} };
static unsigned long long soidle_block_prev_time;
static bool             soidle_by_pass_cg;
static bool             soidle_by_pass_pg;

/* DeepIdle */
static unsigned int     dpidle_block_mask[NR_GRPS] = {0x0};
static unsigned int     dpidle_timer_left;
static unsigned int     dpidle_timer_left2;
#ifndef CONFIG_SMP
static unsigned int     dpidle_timer_cmp;
#endif
static unsigned int     dpidle_time_critera = 26000;
static unsigned int     dpidle_block_time_critera = 30000; /* default 30sec */
static unsigned long    dpidle_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_last_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_block_cnt[NR_REASONS] = {0};
static unsigned long long dpidle_block_prev_time;
static bool             dpidle_by_pass_cg;
static bool             dpidle_by_pass_pg;
static unsigned int     dpidle_dump_log = DEEPIDLE_LOG_REDUCED;

static unsigned int		idle_spm_lock;

#define clk_readl(addr)			__raw_readl(addr)
#define clk_writel(addr, val)	mt_reg_sync_writel(val, addr)

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
static void __iomem *infrasys_base;
static void __iomem *perisys_base;
static void __iomem *audiosys_base;
static void __iomem *mfgsys_base;
static void __iomem *mmsys_base;
static void __iomem *imgsys_base;
static void __iomem *vdecsys_base;
#if !defined(CONFIG_ARCH_MT6735M)
static void __iomem *vencsys_base;
#endif
static void __iomem *cksys_base;

#define INFRA_REG(ofs)      (infrasys_base + ofs)
#define PREI_REG(ofs)       (perisys_base + ofs)
#define AUDIO_REG(ofs)      (audiosys_base + ofs)
#define MFG_REG(ofs)        (mfgsys_base + ofs)
#define MM_REG(ofs)         (mmsys_base + ofs)
#define IMG_REG(ofs)        (imgsys_base + ofs)
#define VDEC_REG(ofs)       (vdecsys_base + ofs)
#define VENC_REG(ofs)       (vencsys_base + ofs)
#define CKSYS_REG(ofs)      (cksys_base + ofs)

#define INFRA_PDN_STA       INFRA_REG(0x0048)
#define PERI_PDN0_STA       PREI_REG(0x0018)
#define PERI_PDN1_STA       PREI_REG(0x001C)
#define AUDIO_TOP_CON0      AUDIO_REG(0x0000)
#define MFG_CG_CON          MFG_REG(0)
#define DISP_CG_CON0        MM_REG(0x100)
#define DISP_CG_CON1        MM_REG(0x110)
#define DISP_CG_DUMMY       MM_REG(0x894)
#define IMG_CG_CON          IMG_REG(0x0000)
#define VDEC_CKEN_SET       VDEC_REG(0x0000)
#define LARB_CKEN_SET       VDEC_REG(0x0008)
#define VENC_CG_CON         VENC_REG(0x0)

#define CLK_CFG_4               CKSYS_REG(0x080)

#define DIS_PWR_STA_MASK        BIT(3)
#define MFG_PWR_STA_MASK        BIT(4)
#define ISP_PWR_STA_MASK        BIT(5)
#define VDE_PWR_STA_MASK        BIT(7)
#define VEN_PWR_STA_MASK        BIT(8)

#define INFRA_AUDIO_PDN_STA_MASK	BIT(5)

enum subsys_id {
	SYS_VDE,
	SYS_MFG,
	SYS_VEN,
	SYS_ISP,
	SYS_DIS,
	NR_SYSS__,
};

static int sys_is_on(enum subsys_id id)
{
	u32 pwr_sta_mask[] = {
		VDE_PWR_STA_MASK,
		MFG_PWR_STA_MASK,
		VEN_PWR_STA_MASK,
		ISP_PWR_STA_MASK,
		DIS_PWR_STA_MASK,
	};

	u32 mask = pwr_sta_mask[id];
	u32 sta = idle_readl(SPM_PWR_STATUS);
	u32 sta_s = idle_readl(SPM_PWR_STATUS_2ND);

	return (sta & mask) && (sta_s & mask);
}

static void get_all_clock_state(u32 clks[NR_GRPS])
{
	int i;

	for (i = 0; i < NR_GRPS; i++)
		clks[i] = 0;

	clks[CG_INFRA] = ~idle_readl(INFRA_PDN_STA); /* INFRA */

	clks[CG_PERI] = ~idle_readl(PERI_PDN0_STA); /* PERI */

	if (sys_is_on(SYS_DIS)) {
#if defined(CONFIG_ARCH_MT6753)
		clks[CG_DISP0] = ~idle_readl(DISP_CG_DUMMY); /* DUMMY */
#else
		clks[CG_DISP0] = ~idle_readl(DISP_CG_CON0); /* DISP0 */
#endif
		clks[CG_DISP1] = ~idle_readl(DISP_CG_CON1); /* DISP1 */
	}

	if (sys_is_on(SYS_ISP))
		clks[CG_IMAGE] = ~idle_readl(IMG_CG_CON); /* IMAGE */

	if (sys_is_on(SYS_MFG))
		clks[CG_MFG] = ~idle_readl(MFG_CG_CON); /* MFG */

	if (clks[CG_INFRA] & INFRA_AUDIO_PDN_STA_MASK) /* check if infra_audio is on */
		clks[CG_AUDIO] = ~idle_readl(AUDIO_TOP_CON0); /* AUDIO */

	if (sys_is_on(SYS_VDE)) {
		clks[CG_VDEC0] = idle_readl(VDEC_CKEN_SET); /* VDEC0 */
		clks[CG_VDEC1] = idle_readl(LARB_CKEN_SET); /* VDEC1 */
	}
#if !defined(CONFIG_ARCH_MT6735M)
	if (sys_is_on(SYS_VEN))
		clks[CG_VENC] = idle_readl(VENC_CG_CON); /* VENC_JPEG */
#endif
}

bool cg_check_idle_can_enter(
	unsigned int *condition_mask, unsigned int *block_mask, enum mt_idle_mode mode)
{
	int i;
	unsigned int sd_mask = 0;
	u32 clks[NR_GRPS];
	u32 r = 0;
	unsigned int sta;

	/* SD status */
	msdc_clk_status(&sd_mask);
	if (sd_mask) {
		block_mask[CG_PERI] |= sd_mask;

		return false;
	}

	/* CG status */
	get_all_clock_state(clks);

	for (i = 0; i < NR_GRPS; i++) {
		block_mask[i] = condition_mask[i] & clks[i];
		r |= block_mask[i];
	}

	if (!(r == 0))
		return false;

	/* MTCMOS status */
	sta = idle_readl(SPM_PWR_STATUS);
	if (mode == MT_DPIDLE) {
		if (!dpidle_by_pass_pg) {
			if (sta & (MFG_PWR_STA_MASK |
						ISP_PWR_STA_MASK |
						VDE_PWR_STA_MASK |
						VEN_PWR_STA_MASK |
						DIS_PWR_STA_MASK))
				return false;
		}
	} else if (mode == MT_SOIDLE) {
		if (!soidle_by_pass_pg) {
			if (sta & (MFG_PWR_STA_MASK |
						ISP_PWR_STA_MASK |
						VDE_PWR_STA_MASK |
						VEN_PWR_STA_MASK))
				return false;
		}
	}

	return true;
}

static unsigned int clk_cfg_4;
void faudintbus_pll2sq(void)
{
	clk_cfg_4 = clk_readl(CLK_CFG_4);
	clk_writel(CLK_CFG_4, clk_cfg_4 & 0xFFFFFCFF);
}

void faudintbus_sq2pll(void)
{
	clk_writel(CLK_CFG_4, clk_cfg_4);
}

static int __init get_base_from_node(
				     const struct of_device_id *ids, void __iomem **pbase, int idx, const char *cmp)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, ids);
	if (!node) {
		idle_warn("node '%s' not found!\n", cmp);
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		BUG();
#endif
	}

	*pbase = of_iomap(node, idx);
	if (!(*pbase)) {
		idle_warn("node '%s' cannot iomap!\n", cmp);
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		BUG();
#endif
	}

	return 0;
}

static void __init iomap_init(void)
{
	static const struct of_device_id infra_ao_ids[] = {
		{.compatible = "mediatek,infracfg_ao"},
		{.compatible = "mediatek,mt6735-infracfg_ao"},
		{ /* sentinel */ }
	};
	static const struct of_device_id pericfg_ids[] = {
		{.compatible = "mediatek,pericfg"},
		{.compatible = "mediatek,mt6735-pericfg"},
		{ /* sentinel */ }
	};
	static const struct of_device_id audio_ids[] = {
		{.compatible = "mediatek,audio"},
		{.compatible = "mediatek,mt6735-audio"},
		{ /* sentinel */ }
	};
	static const struct of_device_id g3d_config_ids[] = {
		{.compatible = "mediatek,g3d_config"},
		{.compatible = "mediatek,mt6735-g3d_config"},
		{ /* sentinel */ }
	};
	static const struct of_device_id mmsys_config_ids[] = {
		{.compatible = "mediatek,mmsys_config"},
		{.compatible = "mediatek,mt6735-mmsys_config"},
		{ /* sentinel */ }
	};
	static const struct of_device_id imgsys_ids[] = {
		{.compatible = "mediatek,imgsys"},
		{.compatible = "mediatek,mt6735-imgsys"},
		{ /* sentinel */ }
	};
	static const struct of_device_id vdec_gcon_ids[] = {
		{.compatible = "mediatek,vdec_gcon"},
		{.compatible = "mediatek,mt6735-vdec_gcon"},
		{ /* sentinel */ }
	};
#if !defined(CONFIG_ARCH_MT6735M)
	static const struct of_device_id venc_gcon_ids[] = {
		{.compatible = "mediatek,venc_gcon"},
		{.compatible = "mediatek,mt6735-venc_gcon"},
		{ /* sentinel */ }
	};
#endif
	static const struct of_device_id cksys_ids[] = {
		{.compatible = "mediatek,cksys"},
		{.compatible = "mediatek,mt6735-cksys"},
		{ /* sentinel */ }
	};

	get_base_from_node(infra_ao_ids, &infrasys_base, 0, "infracfg_ao");
	get_base_from_node(pericfg_ids, &perisys_base, 0, "pericfg");
	get_base_from_node(audio_ids, &audiosys_base, 0, "audio");
	get_base_from_node(g3d_config_ids, &mfgsys_base, 0, "g3d_config");
	get_base_from_node(mmsys_config_ids, &mmsys_base, 0, "mmsys_config");
	get_base_from_node(imgsys_ids, &imgsys_base, 0, "imgsys");
	get_base_from_node(vdec_gcon_ids, &vdecsys_base, 0, "vdec_gcon");
#if !defined(CONFIG_ARCH_MT6735M)
	get_base_from_node(venc_gcon_ids, &vencsys_base, 0, "venc_gcon");
#endif
	get_base_from_node(cksys_ids, &cksys_base, 0, "cksys");
}
#endif /* !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580) */

const char *cg_grp_get_name(int id)
{
	BUG_ON(INVALID_GRP_ID(id));
	return cg_group_name[id];
}

/* Workaround of static analysis defect*/
int idle_gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cnt(id, val);
	*ptr = val[0];

	return ret;
}

int idle_gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cmp(id, val);
	*ptr = val[0];

	return ret;
}

static long int idle_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

static DEFINE_SPINLOCK(idle_spm_spin_lock);

void idle_lock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock |= (1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

void idle_unlock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock &= ~(1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

#if defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)
static bool mt_idle_cpu_criteria(void)
{
	return ((atomic_read(&is_in_hotplug) == 1) || (num_online_cpus() != 1)) ? false : true;
}
#else
static bool mt_idle_cpu_criteria(void)
{
	return (spm_get_cpu_pwr_status() == CA7_CPU0) ? true : false;
}
#endif

/*
 * SODI part
 */
static DEFINE_MUTEX(soidle_locked);
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
static void enable_soidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle_locked);
	soidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&soidle_locked);
}

static void disable_soidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle_locked);
	soidle_condition_mask[grp] |= mask;
	mutex_unlock(&soidle_locked);
}
#endif
void enable_soidle_by_bit(int id)
{
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_soidle_by_mask(grp, mask);
#else
	unsigned int grp = clk_id_to_grp_id(id);
	unsigned int mask = clk_id_to_mask(id);

	if ((grp == NR_GRPS) || (mask == NR_GRPS))
		idle_warn("[%s]wrong clock id\n", __func__);
	else
		soidle_condition_mask[grp] &= ~mask;
#endif
}
EXPORT_SYMBOL(enable_soidle_by_bit);

void disable_soidle_by_bit(int id)
{
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_soidle_by_mask(grp, mask);
#else
	unsigned int grp = clk_id_to_grp_id(id);
	unsigned int mask = clk_id_to_mask(id);

	if ((grp == NR_GRPS) || (mask == NR_GRPS))
		idle_warn("[%s]wrong clock id\n", __func__);
	else
		soidle_condition_mask[grp] |= mask;
#endif
}
EXPORT_SYMBOL(disable_soidle_by_bit);

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
void defeature_soidle_by_display(void)
{
    if (idle_switch[IDLE_TYPE_SO] != 0)
    {
        idle_switch[IDLE_TYPE_SO] = 0;
    }
}
EXPORT_SYMBOL(defeature_soidle_by_display);
#endif

static bool soidle_can_enter(int cpu)
{
	int reason = NR_REASONS;
	unsigned long long soidle_block_curr_time = 0;
	bool retval = false;
	char *p;

#ifdef CONFIG_SMP
	if (!mt_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto out;
	}
#endif

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto out;
	}

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	if (!is_teei_ready()) {
		reason = BY_OTH;
		goto out;
	}
#endif

	/* decide when to enable SODI by display driver */
	if (spm_get_sodi_en() == 0) {
		reason = BY_OTH;
		goto out;
	}

	if (soidle_by_pass_cg == 0) {
		memset(soidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		if (!cg_check_idle_can_enter(soidle_condition_mask, soidle_block_mask, MT_SOIDLE)) {
#else
	if (!clkmgr_idle_can_enter(soidle_condition_mask, soidle_block_mask)) {
#endif
			reason = BY_CLK;
			goto out;
		}
	}

#ifdef CONFIG_SMP
	soidle_timer_left = localtimer_get_counter();
	if ((int)soidle_timer_left < soidle_time_critera ||
			((int)soidle_timer_left) < 0) {
		reason = BY_TMR;
		goto out;
	}
#else
	gpt_get_cnt(GPT1, &soidle_timer_left);
	gpt_get_cmp(GPT1, &soidle_timer_cmp);
	if ((soidle_timer_cmp - soidle_timer_left) < soidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#endif

out:
	if (reason < NR_REASONS) {
		if (soidle_block_prev_time == 0)
			soidle_block_prev_time = idle_get_current_time_ms();

		soidle_block_curr_time = idle_get_current_time_ms();
		if (((soidle_block_curr_time - soidle_block_prev_time) > soidle_block_time_critera)
			&& ((soidle_block_curr_time - idle_block_log_prev_time) > idle_block_log_time_criteria)) {

			if ((smp_processor_id() == 0)) {
				int i = 0;

				/* soidle,rgidle count */
				p = log_buf;
				p += sprintf(p, "CNT(soidle,rgidle): ");
				for (i = 0; i < nr_cpu_ids; i++)
					p += sprintf(p, "[%d] = (%lu,%lu), ", i, soidle_cnt[i], rgidle_cnt[i]);
				idle_warn("%s\n", log_buf);

				/* block category */
				p = log_buf;
				p += sprintf(p, "soidle_block_cnt: ");
				for (i = 0; i < NR_REASONS; i++)
					p += sprintf(p, "[%s] = %lu, ", reason_name[i], soidle_block_cnt[0][i]);
				idle_warn("%s\n", log_buf);

				p = log_buf;
				p += sprintf(p, "soidle_block_mask: ");
				for (i = 0; i < NR_GRPS; i++)
					p += sprintf(p, "0x%08x, ", soidle_block_mask[i]);
				idle_warn("%s\n", log_buf);

				memset(soidle_block_cnt, 0, sizeof(soidle_block_cnt));
				soidle_block_prev_time = idle_get_current_time_ms();
				idle_block_log_prev_time = soidle_block_prev_time;
			}
		}

		soidle_block_cnt[cpu][reason]++;
		retval = false;
	} else {
		soidle_block_prev_time = idle_get_current_time_ms();
		retval = true;
	}

	return retval;
}

void soidle_before_wfi(int cpu)
{
#ifdef CONFIG_SMP
	soidle_timer_left2 = localtimer_get_counter();

	if ((int)soidle_timer_left2 <= 0) {
		/* Trigger idle_gpt Timerout imediately */
		gpt_set_cmp(idle_gpt, 1);
	} else
		gpt_set_cmp(idle_gpt, soidle_timer_left2);

	start_gpt(idle_gpt);
#else
	gpt_get_cnt(GPT1, &soidle_timer_left2);
#endif
}

void soidle_after_wfi(int cpu)
{
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(idle_gpt)) {
		localtimer_set_next_event(1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;

		idle_gpt_get_cnt(idle_gpt, &cnt);
		idle_gpt_get_cmp(idle_gpt, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_warn("[%s]GPT%d: counter = %10u, compare = %10u\n", __func__,
					idle_gpt + 1, cnt, cmp);
			BUG();
		}

		localtimer_set_next_event(cmp-cnt);
		stop_gpt(idle_gpt);
	}
#endif
	soidle_cnt[cpu]++;
}

/*
 * deep idle part
 */
static DEFINE_MUTEX(dpidle_locked);
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&dpidle_locked);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] |= mask;
	mutex_unlock(&dpidle_locked);
}
#endif
void enable_dpidle_by_bit(int id)
{
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_dpidle_by_mask(grp, mask);
#else
	unsigned int grp = clk_id_to_grp_id(id);
	unsigned int mask = clk_id_to_mask(id);

	if ((grp == NR_GRPS) || (mask == NR_GRPS))
		idle_warn("[%s]wrong clock id\n", __func__);
	else
		dpidle_condition_mask[grp] &= ~mask;
#endif
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_dpidle_by_mask(grp, mask);
#else
	unsigned int grp = clk_id_to_grp_id(id);
	unsigned int mask = clk_id_to_mask(id);

	if ((grp == NR_GRPS) || (mask == NR_GRPS))
		idle_warn("[%s]wrong clock id\n", __func__);
	else
		dpidle_condition_mask[grp] |= mask;
#endif
}
EXPORT_SYMBOL(disable_dpidle_by_bit);

static bool dpidle_can_enter(void)
{
	int reason = NR_REASONS;
	int i = 0;
	unsigned long long dpidle_block_curr_time = 0;
	bool retval = false;
	char *p;

#ifdef CONFIG_SMP
	if (!mt_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto out;
	}
#endif

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto out;
	}

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	if (!is_teei_ready()) {
		reason = BY_OTH;
		goto out;
	}
#endif

	if (dpidle_by_pass_cg == 0) {
		memset(dpidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		if (!cg_check_idle_can_enter(dpidle_condition_mask, dpidle_block_mask, MT_DPIDLE)) {
#else
		if (!clkmgr_idle_can_enter(dpidle_condition_mask, dpidle_block_mask)) {
#endif
			reason = BY_CLK;
			goto out;
		}
	}
#ifdef CONFIG_SMP
	dpidle_timer_left = localtimer_get_counter();
	if ((int)dpidle_timer_left < dpidle_time_critera ||
			((int)dpidle_timer_left) < 0) {
		reason = BY_TMR;
		goto out;
	}
#else
	gpt_get_cnt(GPT1, &dpidle_timer_left);
	gpt_get_cmp(GPT1, &dpidle_timer_cmp);
	if ((dpidle_timer_cmp - dpidle_timer_left) < dpidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#endif

out:
	if (reason < NR_REASONS) {
		if (dpidle_block_prev_time == 0)
			dpidle_block_prev_time = idle_get_current_time_ms();

		dpidle_block_curr_time = idle_get_current_time_ms();
		if (((dpidle_block_curr_time - dpidle_block_prev_time) > dpidle_block_time_critera)
			&& ((dpidle_block_curr_time - idle_block_log_prev_time) > idle_block_log_time_criteria)) {

			if ((smp_processor_id() == 0)) {
				/* dpidle,rgidle count */
				p = log_buf;
				p += sprintf(p, "CNT(dpidle,rgidle): ");
				for (i = 0; i < nr_cpu_ids; i++)
					p += sprintf(p, "[%d] = (%lu,%lu), ", i, dpidle_cnt[i], rgidle_cnt[i]);
				idle_warn("%s\n", log_buf);

				/* block category */
				p = log_buf;
				p += sprintf(p, "dpidle_block_cnt: ");
				for (i = 0; i < NR_REASONS; i++)
					p += sprintf(p, "[%s] = %lu, ", reason_name[i], dpidle_block_cnt[i]);
				idle_warn("%s\n", log_buf);

				p = log_buf;
				p += sprintf(p, "dpidle_block_mask: ");
				for (i = 0; i < NR_GRPS; i++)
					p += sprintf(p, "0x%08x, ", dpidle_block_mask[i]);
				idle_warn("%s\n", log_buf);

				memset(dpidle_block_cnt, 0, sizeof(dpidle_block_cnt));
				dpidle_block_prev_time = idle_get_current_time_ms();
				idle_block_log_prev_time = dpidle_block_prev_time;
			}
		}
		dpidle_block_cnt[reason]++;
		retval = false;
	} else {
		dpidle_block_prev_time = idle_get_current_time_ms();
		retval = true;
	}

	return retval;
}

void spm_dpidle_before_wfi(void)
{
	bus_dcm_enable();

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	faudintbus_pll2sq();
#else
	clkmgr_faudintbus_pll2sq();
#endif

#ifdef CONFIG_SMP
	dpidle_timer_left2 = localtimer_get_counter();

	if ((int)dpidle_timer_left2 <= 0) {
		/* Trigger GPT4 Timerout imediately */
		gpt_set_cmp(idle_gpt, 1);
	} else
		gpt_set_cmp(idle_gpt, dpidle_timer_left2);

	start_gpt(idle_gpt);
#else
	gpt_get_cnt(idle_gpt, &dpidle_timer_left2);
#endif
}

void spm_dpidle_after_wfi(void)
{
#ifdef CONFIG_SMP
	/* if (gpt_check_irq(GPT4)) { */
	if (gpt_check_and_ack_irq(idle_gpt)) {
		/* waked up by WAKEUP_GPT */
		localtimer_set_next_event(1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;

		idle_gpt_get_cnt(idle_gpt, &cnt);
		idle_gpt_get_cmp(idle_gpt, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_warn("[%s]GPT%d: counter = %10u, compare = %10u\n", __func__,
					idle_gpt + 1, cnt, cmp);
			BUG();
		}

		localtimer_set_next_event(cmp-cnt);
		stop_gpt(idle_gpt);
	}
#endif

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	faudintbus_sq2pll();
#else
	clkmgr_faudintbus_sq2pll();
#endif

	bus_dcm_disable();

	dpidle_cnt[0]++;
}

/*
 * slow idle part
 */

static DEFINE_MUTEX(slidle_locked);

static void enable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] |= mask;
	mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);

static bool slidle_can_enter(void)
{
	int reason = NR_REASONS;

	if (!mt_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto out;
	}

	memset(slidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	if (!cg_check_idle_can_enter(slidle_condition_mask, slidle_block_mask, MT_SLIDLE)) {
#else
	if (!clkmgr_idle_can_enter(slidle_condition_mask, slidle_block_mask)) {
#endif
		reason = BY_CLK;
		goto out;
	}

#if EN_PTP_OD
	if (ptp_data[0]) {
		reason = BY_OTH;
		goto out;
	}
#endif

out:
	if (reason < NR_REASONS) {
		slidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

static void slidle_before_wfi(int cpu)
{
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || defined(CONFIG_ARCH_MT6753)
	mt_dcm_topckg_enable();
#endif
}

static void slidle_after_wfi(int cpu)
{
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || defined(CONFIG_ARCH_MT6753)
	mt_dcm_topckg_disable();
	slidle_cnt[cpu]++;
#endif
}

static void go_to_slidle(int cpu)
{
	slidle_before_wfi(cpu);

	mb();
	__asm__ __volatile__("wfi" : : : "memory");

	slidle_after_wfi(cpu);
}

/*
 * regular idle part
 */
static void rgidle_before_wfi(int cpu)
{

}

static void rgidle_after_wfi(int cpu)
{
	rgidle_cnt[cpu]++;
}

static noinline void go_to_rgidle(int cpu)
{
	rgidle_before_wfi(cpu);
	isb();
	mb();
	__asm__ __volatile__("wfi" : : : "memory");

	rgidle_after_wfi(cpu);
}

/*
 * idle task flow part
 */
static inline void soidle_pre_handler(void)
{
	hps_del_timer();
#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	tscpu_cancel_thermal_timer();

	mtkts_bts_cancel_thermal_timer();
	mtkts_btsmdpa_cancel_thermal_timer();
	mtkts_pmic_cancel_thermal_timer();
	mtkts_battery_cancel_thermal_timer();
	mtkts_pa_cancel_thermal_timer();
	mtkts_wmt_cancel_thermal_timer();

	mtkts_allts_cancel_ts1_timer();
	mtkts_allts_cancel_ts2_timer();
	mtkts_allts_cancel_ts3_timer();
	mtkts_allts_cancel_ts4_timer();
	mtkts_allts_cancel_ts5_timer();
#endif
}

static inline void soidle_post_handler(void)
{
	hps_restart_timer();
#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	tscpu_start_thermal_timer();

	mtkts_bts_start_thermal_timer();
	mtkts_btsmdpa_start_thermal_timer();
	mtkts_pmic_start_thermal_timer();
	mtkts_battery_start_thermal_timer();
	mtkts_pa_start_thermal_timer();
	mtkts_wmt_start_thermal_timer();

	mtkts_allts_start_ts1_timer();
	mtkts_allts_start_ts2_timer();
	mtkts_allts_start_ts3_timer();
	mtkts_allts_start_ts4_timer();
	mtkts_allts_start_ts5_timer();
#endif
}

static u32 slp_spm_SODI_flags = {
	0
};

u32 slp_spm_deepidle_flags = {
	0
};

static inline void dpidle_pre_handler(void)
{
	hps_del_timer();
#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	tscpu_cancel_thermal_timer();
	mtkts_bts_cancel_thermal_timer();
	mtkts_btsmdpa_cancel_thermal_timer();
	mtkts_pmic_cancel_thermal_timer();
	mtkts_battery_cancel_thermal_timer();
	mtkts_pa_cancel_thermal_timer();
	mtkts_wmt_cancel_thermal_timer();

	mtkts_allts_cancel_ts1_timer();
	mtkts_allts_cancel_ts2_timer();
	mtkts_allts_cancel_ts3_timer();
	mtkts_allts_cancel_ts4_timer();
	mtkts_allts_cancel_ts5_timer();
#endif
}
static inline void dpidle_post_handler(void)
{
	hps_restart_timer();
#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	tscpu_start_thermal_timer();
	mtkts_bts_start_thermal_timer();
	mtkts_btsmdpa_start_thermal_timer();
	mtkts_pmic_start_thermal_timer();
	mtkts_battery_start_thermal_timer();
	mtkts_pa_start_thermal_timer();
	mtkts_wmt_start_thermal_timer();

	mtkts_allts_start_ts1_timer();
	mtkts_allts_start_ts2_timer();
	mtkts_allts_start_ts3_timer();
	mtkts_allts_start_ts4_timer();
	mtkts_allts_start_ts5_timer();
#endif
}
#ifdef SPM_DEEPIDLE_PROFILE_TIME
unsigned int dpidle_profile[4];
#endif

static inline int dpidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_DP]) {
		if (dpidle_can_enter())
			ret = 1;
	}

	return ret;
}

static inline int soidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_SO]) {
		if (soidle_can_enter(cpu))
			ret = 1;
	}

	return ret;
}

static inline int slidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_SL]) {
		if (slidle_can_enter())
			ret = 1;
	}

	return ret;
}

static inline int rgidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_RG])
		ret = 1;

	return ret;
}

static int (*idle_select_handlers[NR_TYPES])(int) = {
	dpidle_select_handler,
	soidle_select_handler,
	slidle_select_handler,
	rgidle_select_handler,
};

void dump_idle_cnt_in_interval(int cpu)
{
	int i = 0;
	char *p = log_buf;
	char *p2 = log_buf_2;
	unsigned long long idle_cnt_dump_curr_time = 0;
	bool have_dpidle = false;
	bool have_soidle = false;

	if (idle_cnt_dump_prev_time == 0)
		idle_cnt_dump_prev_time = idle_get_current_time_ms();

	idle_cnt_dump_curr_time = idle_get_current_time_ms();

	if (!(cpu == 0))
		return;

	if (!((idle_cnt_dump_curr_time - idle_cnt_dump_prev_time) > idle_cnt_dump_criteria))
		return;

	/* dump idle count */
	/* deepidle */
	p = log_buf;
	for (i = 0; i < nr_cpu_ids; i++) {
		if ((dpidle_cnt[i] - dpidle_last_cnt[i]) != 0) {
			p += sprintf(p, "[%d] = %lu, ", i, dpidle_cnt[i] - dpidle_last_cnt[i]);
			have_dpidle = true;
		}

		dpidle_last_cnt[i] = dpidle_cnt[i];
	}

	if (have_dpidle)
		p2 += sprintf(p2, "DP: %s --- ", log_buf);
	else
		p2 += sprintf(p2, "DP: No enter --- ");

	/* sodi */
	p = log_buf;
	for (i = 0; i < nr_cpu_ids; i++) {
		if ((soidle_cnt[i] - soidle_last_cnt[i]) != 0) {
			p += sprintf(p, "[%d] = %lu, ", i, soidle_cnt[i] - soidle_last_cnt[i]);
			have_soidle = true;
		}

		soidle_last_cnt[i] = soidle_cnt[i];
	}

	if (have_soidle)
		p2 += sprintf(p2, "SODI: %s --- ", log_buf);
	else
		p2 += sprintf(p2, "SODI: No enter --- ");

	/* dump log */
	idle_warn("%s\n", log_buf_2);

	/* dump idle ratio */
	if (idle_ratio_en) {
		idle_ratio_profile_duration = idle_get_current_time_ms() - idle_ratio_profile_start_time;
		idle_warn("--- CPU 0 idle: %llu, DP = %llu, SO = %llu, SL = %llu, RG = %llu --- (ms)\n",
				idle_ratio_profile_duration,
				idle_ratio_value[IDLE_TYPE_DP],
				idle_ratio_value[IDLE_TYPE_SO],
				idle_ratio_value[IDLE_TYPE_SL],
				idle_ratio_value[IDLE_TYPE_RG]);

		idle_ratio_profile_start_time = idle_get_current_time_ms();
		for (i = 0; i < NR_TYPES; i++)
			idle_ratio_value[i] = 0;
	}

	/* update time base */
	idle_cnt_dump_prev_time = idle_cnt_dump_curr_time;
}

inline void idle_ratio_calc_start(int type, int cpu)
{
	if (type >= 0 && type < NR_TYPES && cpu == 0)
		idle_ratio_start_time[type] = idle_get_current_time_ms();
}

inline void idle_ratio_calc_stop(int type, int cpu)
{
	if (type >= 0 && type < NR_TYPES && cpu == 0)
		idle_ratio_value[type] += (idle_get_current_time_ms() - idle_ratio_start_time[type]);
}

int mt_idle_select(int cpu)
{
	int i = NR_TYPES - 1;

	dump_idle_cnt_in_interval(cpu);

	for (i = 0; i < NR_TYPES; i++) {
		if (idle_select_handlers[i](cpu))
			break;
	}

	return i;
}
EXPORT_SYMBOL(mt_idle_select);

int dpidle_enter(int cpu)
{
	int ret = 1;

	idle_ratio_calc_start(IDLE_TYPE_DP, cpu);

	dpidle_pre_handler();
	spm_go_to_dpidle(slp_spm_deepidle_flags, 0, dpidle_dump_log);
	dpidle_post_handler();

	idle_ratio_calc_stop(IDLE_TYPE_DP, cpu);

#ifdef CONFIG_SMP
	idle_warn_log("DP:timer_left=%d, timer_left2=%d, delta=%d\n",
				dpidle_timer_left, dpidle_timer_left2, dpidle_timer_left-dpidle_timer_left2);
#else
	idle_warn_log("DP:timer_left=%d, timer_left2=%d, delta=%d, timeout val=%d\n",
				dpidle_timer_left,
				dipidle_timer_left2,
				dpidle_timer_left2 - dpidle_timer_left,
				dpidle_timer_cmp - dpidle_timer_left);
#endif
#ifdef SPM_DEEPIDLE_PROFILE_TIME
	gpt_get_cnt(SPM_PROFILE_APXGPT, &dpidle_profile[3]);
	idle_warn_log("1:%u, 2:%u, 3:%u, 4:%u\n",
				dpidle_profile[0], dpidle_profile[1], dpidle_profile[2], dpidle_profile[3]);
#endif

	return ret;
}
EXPORT_SYMBOL(dpidle_enter);

int soidle_enter(int cpu)
{
	int ret = 1;

	idle_ratio_calc_start(IDLE_TYPE_SO, cpu);

	soidle_pre_handler();
	spm_go_to_sodi(slp_spm_SODI_flags, 0);
	soidle_post_handler();

	idle_ratio_calc_stop(IDLE_TYPE_SO, cpu);

	return ret;
}
EXPORT_SYMBOL(soidle_enter);

int slidle_enter(int cpu)
{
	int ret = 1;

	idle_ratio_calc_start(IDLE_TYPE_SL, cpu);

	go_to_slidle(cpu);

	idle_ratio_calc_stop(IDLE_TYPE_SL, cpu);

	return ret;
}
EXPORT_SYMBOL(slidle_enter);

int rgidle_enter(int cpu)
{
	int ret = 1;

	idle_ratio_calc_start(IDLE_TYPE_RG, cpu);

	go_to_rgidle(cpu);

	idle_ratio_calc_stop(IDLE_TYPE_RG, cpu);

	return ret;
}
EXPORT_SYMBOL(rgidle_enter);

void mt_idle_init(void)
{

}

/*
 * debugfs
 */
static char dbg_buf[2048] = {0};
static char cmd_buf[512] = {0};

/*
 * idle_state
 */
static int _idle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int idle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _idle_state_open, inode->i_private);
}

static ssize_t idle_state_read(struct file *filp,
								char __user *userbuf,
								size_t count,
								loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "********** idle state dump **********\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		p += sprintf(p, "soidle_cnt[%d]=%lu, dpidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
				i, soidle_cnt[i], i, dpidle_cnt[i],
				i, slidle_cnt[i], i, rgidle_cnt[i]);
	}

	p += sprintf(p, "\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		p += sprintf(p, "%s_switch=%d, ", idle_name[i], idle_switch[i]);

	p += sprintf(p, "\n");
	p += sprintf(p, "idle_ratio_en = %u\n", idle_ratio_en);

	p += sprintf(p, "\n********** idle command help **********\n");
	p += sprintf(p, "status help:   cat /sys/kernel/debug/cpuidle/idle_state\n");
	p += sprintf(p, "switch on/off: echo switch mask > /sys/kernel/debug/cpuidle/idle_state\n");
	p += sprintf(p, "idle ratio profile: echo ratio 1/0 > /sys/kernel/debug/cpuidle/idle_state\n");

	p += sprintf(p, "soidle help:   cat /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "dpidle help:   cat /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "slidle help:   cat /sys/kernel/debug/cpuidle/slidle_state\n");
	p += sprintf(p, "rgidle help:   cat /sys/kernel/debug/cpuidle/rgidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t idle_state_write(struct file *filp,
								const char __user *userbuf,
								size_t count,
								loff_t *f_pos)
{
	char cmd[32];
	int idx;
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "switch")) {
			for (idx = 0; idx < NR_TYPES; idx++)
				idle_switch[idx] = (param & (1U << idx)) ? 1 : 0;
		} else if (!strcmp(cmd, "ratio")) {
			idle_ratio_en = param;

			if (idle_ratio_en) {
				idle_ratio_profile_start_time = idle_get_current_time_ms();
				for (idx = 0; idx < NR_TYPES; idx++)
					idle_ratio_value[idx] = 0;
			}
		}
		return count;
	}

	return -EINVAL;
}

static const struct file_operations idle_state_fops = {
	.open = idle_state_open,
	.read = idle_state_read,
	.write = idle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};
/*
 * dpidle_state
 */
static int _dpidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int dpidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _dpidle_state_open, inode->i_private);
}

static ssize_t dpidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "*********** deep idle state ************\n");
	p += sprintf(p, "dpidle_time_critera=%u\n", dpidle_time_critera);

	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]dpidle_block_cnt[%s]=%lu\n", i, reason_name[i],
				dpidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p, "[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\tdpidle_block_mask[%-8s]=0x%08x\n", i,
				cg_grp_get_name(i), dpidle_condition_mask[i],
				cg_grp_get_name(i), dpidle_block_mask[i]);
	}

	p += sprintf(p, "dpidle_bypass_cg=%u\n", dpidle_by_pass_cg);
	p += sprintf(p, "dpidle_by_pass_pg=%u\n", dpidle_by_pass_pg);
	p += sprintf(p, "dpidle_dump_log = %u\n", dpidle_dump_log);
	p += sprintf(p, "(0: None, 1: Reduced, 2: Full\n");

	p += sprintf(p, "\n*********** dpidle command help  ************\n");
	p += sprintf(p, "dpidle help:   cat /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "switch on/off: echo [dpidle] 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "cpupdn on/off: echo cpupdn 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/dpidle_state\n");
	p += sprintf(p, "bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t dpidle_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			idle_switch[IDLE_TYPE_DP] = param;
		else if (!strcmp(cmd, "enable"))
			enable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			dpidle_time_critera = param;
		else if (!strcmp(cmd, "bypass")) {
			dpidle_by_pass_cg = param;
			idle_warn("bypass = %d\n", dpidle_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_pg")) {
			dpidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", dpidle_by_pass_pg);
		} else if (!strcmp(cmd, "log"))
			dpidle_dump_log = param;
		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param)) {
		idle_switch[IDLE_TYPE_DP] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations dpidle_state_fops = {
	.open = dpidle_state_open,
	.read = dpidle_state_read,
	.write = dpidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * soidle_state
 */
static int _soidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle_state_open, inode->i_private);
}

static ssize_t soidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "*********** deep idle state ************\n");
	p += sprintf(p, "soidle_time_critera=%u\n", soidle_time_critera);

	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]soidle_block_cnt[%s]=%lu\n", i, reason_name[i],
						soidle_block_cnt[0][i]);
	}

	p += sprintf(p, "\n");

	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p, "[%02d]soidle_condition_mask[%-8s]=0x%08x\t\tsoidle_block_mask[%-8s]=0x%08x\n", i,
				cg_grp_get_name(i), soidle_condition_mask[i],
				cg_grp_get_name(i), soidle_block_mask[i]);
	}

	p += sprintf(p, "soidle_bypass_cg=%u\n", soidle_by_pass_cg);
	p += sprintf(p, "soidle_by_pass_pg=%u\n", soidle_by_pass_pg);

	p += sprintf(p, "\n*********** soidle command help  ************\n");
	p += sprintf(p, "soidle help:   cat /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "switch on/off: echo [soidle] 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "cpupdn on/off: echo cpupdn 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/soidle_state\n");
	p += sprintf(p, "bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t soidle_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle"))
			idle_switch[IDLE_TYPE_SO] = param;
		else if (!strcmp(cmd, "enable"))
			enable_soidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_soidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			soidle_time_critera = param;
		else if (!strcmp(cmd, "bypass")) {
			soidle_by_pass_cg = param;
			idle_warn("bypass = %d\n", soidle_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_pg")) {
			soidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", soidle_by_pass_pg);
		}
		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param)) {
		idle_switch[IDLE_TYPE_SO] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle_state_fops = {
	.open = soidle_state_open,
	.read = soidle_state_read,
	.write = soidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * slidle_state
 */
static int _slidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int slidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _slidle_state_open, inode->i_private);
}

static ssize_t slidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "*********** slow idle state ************\n");
	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]slidle_block_cnt[%s]=%lu\n",
				i, reason_name[i], slidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p, "[%02d]slidle_condition_mask[%-8s]=0x%08x\t\tslidle_block_mask[%-8s]=0x%08x\n", i,
				cg_grp_get_name(i), slidle_condition_mask[i],
				cg_grp_get_name(i), slidle_block_mask[i]);
	}

	p += sprintf(p, "\n********** slidle command help **********\n");
	p += sprintf(p, "slidle help:   cat /sys/kernel/debug/cpuidle/slidle_state\n");
	p += sprintf(p, "switch on/off: echo [slidle] 1/0 > /sys/kernel/debug/cpuidle/slidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t slidle_state_write(struct file *filp, const char __user *userbuf,
									size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(userbuf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "slidle"))
			idle_switch[IDLE_TYPE_SL] = param;
		else if (!strcmp(cmd, "enable"))
			enable_slidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_slidle_by_bit(param);

		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param)) {
		idle_switch[IDLE_TYPE_SL] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations slidle_state_fops = {
	.open = slidle_state_open,
	.read = slidle_state_read,
	.write = slidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *root_entry;

#if defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)
/* CPU hotplug notifier, for informing whether CPU hotplug is working */
static int mt_idle_cpu_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		atomic_inc(&is_in_hotplug);
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		atomic_dec(&is_in_hotplug);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mt_idle_cpu_notifier = {
	.notifier_call = mt_idle_cpu_callback,
	.priority   = INT_MAX,
};

static int mt_idle_hotplug_cb_init(void)
{
	register_cpu_notifier(&mt_idle_cpu_notifier);

	return 0;
}
#endif

static int mt_cpuidle_debugfs_init(void)
{
	/* Initialize debugfs */
	root_entry = debugfs_create_dir("cpuidle", NULL);
	if (!root_entry) {
		idle_warn("Can not create debugfs `dpidle_state`\n");
		return 1;
	}

	debugfs_create_file("idle_state", 0644, root_entry, NULL, &idle_state_fops);
	debugfs_create_file("dpidle_state", 0644, root_entry, NULL, &dpidle_state_fops);
	debugfs_create_file("soidle_state", 0644, root_entry, NULL, &soidle_state_fops);
	debugfs_create_file("slidle_state", 0644, root_entry, NULL,
			&slidle_state_fops);

	return 0;
}

void mt_cpuidle_framework_init(void)
{
	int err = 0;
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	int i = 0;
#endif

	idle_dbg("[%s]entry!!\n", __func__);

	err = request_gpt(idle_gpt, GPT_ONE_SHOT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
				0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_warn("[%s]fail to request GPT%d\n", __func__, idle_gpt + 1);

	err = 0;

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	for (i = 0; i < num_possible_cpus(); i++)
		err |= cpu_xgpt_register_timer(i, NULL);
#else
	/* TODO: cpu_xgpt_register_timer() has not been ported to mach/mt_cpuxgpt.h */
#endif

	if (err)
		idle_warn("[%s]fail to request cpuxgpt\n", __func__);

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	iomap_init();
#endif

#if defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)
	mt_idle_hotplug_cb_init();
#endif

	mt_cpuidle_debugfs_init();
}
EXPORT_SYMBOL(mt_cpuidle_framework_init);

module_param(mt_idle_chk_golden, bool, 0644);
module_param(mt_dpidle_chk_golden, bool, 0644);
