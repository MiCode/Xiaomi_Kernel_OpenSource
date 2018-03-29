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

#ifndef __MT_IDLE_INTERNAL_H__
#define __MT_IDLE_INTERNAL_H__
#include <linux/io.h>

/*
 * Chip specific declaratinos
 */
#if defined(CONFIG_ARCH_MT6755)

#include "mt_idle_mt6755.h"

#elif defined(CONFIG_ARCH_MT6757)

#include "mt_idle_mt6757.h"

#elif defined(CONFIG_ARCH_MT6797)

#include "mt_idle_mt6797.h"

#endif

/*
 * Common declarations of MT6755/MT6797
 */
enum mt_idle_mode {
	MT_DPIDLE = 0,
	MT_SOIDLE,
	MT_MCIDLE,
	MT_SLIDLE
};

enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO3,
	IDLE_TYPE_SO,
	IDLE_TYPE_MC,
	IDLE_TYPE_SL,
	IDLE_TYPE_RG,
	NR_TYPES,
};

enum {
	BY_CPU = 0,
	BY_CLK,
	BY_TMR,
	BY_OTH,
	BY_VTG,
	BY_FRM,
	BY_PLL,
	BY_PWM,
#ifdef CONFIG_CPU_ISOLATION
	BY_ISO,
#endif
	BY_DVFSP,
	BY_CONN,
	NR_REASONS,
};

#define INVALID_GRP_ID(grp)     (grp < 0 || grp >= NR_GRPS)
#define idle_readl(addr)	    __raw_readl(addr)

extern unsigned int dpidle_blocking_stat[NR_GRPS][32];
extern int idle_switch[NR_TYPES];
extern unsigned int dpidle_condition_mask[NR_GRPS];
extern unsigned int soidle3_pll_condition_mask[NR_PLLS];
extern unsigned int soidle3_condition_mask[NR_GRPS];
extern unsigned int soidle_condition_mask[NR_GRPS];
extern unsigned int slidle_condition_mask[NR_GRPS];
extern const char *idle_name[NR_TYPES];
extern const char *reason_name[NR_REASONS];

/*
 * Function Declarations
 */
const char *cg_grp_get_name(int id);
bool cg_check_idle_can_enter(
	unsigned int *condition_mask, unsigned int *block_mask, enum mt_idle_mode mode);
bool cg_i2c_appm_check_idle_can_enter(unsigned int *block_mask);
bool pll_check_idle_can_enter(unsigned int *condition_mask, unsigned int *block_mask);
const char *pll_grp_get_name(int id);
void __init iomap_init(void);

bool is_disp_pwm_rosc(void);
bool is_auxadc_released(void);
bool vcore_dvfs_is_progressing(void);

#endif /* __MT_IDLE_INTERNAL_H__ */

