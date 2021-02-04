/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_IDLE_INTERNAL_H__
#define __MTK_IDLE_INTERNAL_H__
#include "mtk_idle_mcdi.h"

/*
 * Chip specific declaratinos
 */
#if defined(CONFIG_MACH_MT6763)
#include "mtk_idle_mt6763.h"
#elif defined(CONFIG_MACH_MT6739)
#include "mtk_idle_mt6739.h"
#elif defined(CONFIG_MACH_MT6771)
#include "mtk_idle_mt6771.h"
#endif

/*
 * Common declarations
 */
enum mt_idle_mode {
	MT_DPIDLE = 0,
	MT_SOIDLE,
};

/* CPUIDLE_STATE is used to represent CPUidle C States */
enum {
	CPUIDLE_STATE_RG = 0,
	CPUIDLE_STATE_SO,
	CPUIDLE_STATE_DP,
	CPUIDLE_STATE_SO3,
	NR_CPUIDLE_STATE
};

enum {
	BY_FRM = 0,
	BY_CPU,		/* No single CPU  (not used in ACAO case)*/
	BY_SRR,		/* Blocked by SPM Resource Request */
	BY_UFS,		/* Blocked by UFS */
	BY_TEE,		/* TEEI not ready */
	BY_CLK,		/* CG check fail */
	BY_DCS,		/* Blocked by DCS */
	BY_DIS,		/* Display not ready (SODI) */
	BY_PWM,		/* Display pwm not ready (SODI3) */
	BY_PLL,		/* PLL not off (SODI3) */
	BY_BOOT,	/* Boot up time < 20/30 seconds (SODI/SODI3) */
	NR_REASONS,
};

#define INVALID_GRP_ID(grp)	(grp < 0 || grp >= NR_GRPS)
#define INVALID_IDLE_ID(id)	(id < 0 || id >= NR_TYPES)
#define INVALID_REASON_ID(id)	(id < 0 || id >= NR_REASONS)

#define idle_readl(addr)	__raw_readl(addr)

extern unsigned int dpidle_blocking_stat[NR_GRPS][32];
extern int idle_switch[NR_TYPES];

extern unsigned int idle_condition_mask[NR_TYPES][NR_GRPS];

extern unsigned int soidle3_pll_condition_mask[NR_PLLS];

/*
 * Function Declarations
 */
const char *mtk_get_idle_name(int id);
const char *mtk_get_reason_name(int id);
const char *mtk_get_cg_group_name(int id);
const char *mtk_get_pll_group_name(int id);

bool mtk_idle_check_cg(unsigned int block_mask[NR_TYPES][NF_CG_STA_RECORD]);
bool mtk_idle_check_secure_cg(
		unsigned int block_mask[NR_TYPES][NF_CG_STA_RECORD]);
bool mtk_idle_check_pll(unsigned int *condition_mask, unsigned int *block_mask);
bool mtk_idle_check_clkmux(int idle_type,
		unsigned int block_mask[NR_TYPES][NF_CLK_CFG]);
bool mtk_idle_check_vcore_cond(void);

void __init iomap_init(void);

bool mtk_idle_disp_is_pwm_rosc(void);

unsigned int soidle_pre_handler(void);
void soidle_post_handler(void);

u32 get_spm_idle_flags1(void);


#endif /* __MTK_IDLE_INTERNAL_H__ */

