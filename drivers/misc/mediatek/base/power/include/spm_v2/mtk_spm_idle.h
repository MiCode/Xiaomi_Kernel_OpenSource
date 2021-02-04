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

#ifndef _MTK_SPM_IDLE_
#define _MTK_SPM_IDLE_

#include <linux/kernel.h>
#include "mtk_spm.h"
#include "mtk_spm_sleep.h"


#define TAG     "SPM-Idle"

#define spm_idle_err(fmt, args...)		pr_debug(TAG fmt, ##args)
#define spm_idle_warn(fmt, args...)		pr_debug(TAG fmt, ##args)
#define spm_idle_dbg(fmt, args...)		pr_debug(TAG fmt, ##args)
#define spm_idle_info(fmt, args...)		pr_debug(TAG fmt, ##args)
#define spm_idle_ver(fmt, args...)		pr_debug(TAG fmt, ##args)	/* pr_debug show nothing */

/*
 * for SPM common part
 */
#define	CPU_0		(1U << 0)
#define	CPU_1		(1U << 1)
#define	CPU_2		(1U << 2)
#define	CPU_3		(1U << 3)
#define	CPU_4		(1U << 4)
#define	CPU_5		(1U << 5)
#define	CPU_6		(1U << 6)
#define	CPU_7		(1U << 7)
#define	CPU_8		(1U << 8)
#define	CPU_9		(1U << 9)

extern unsigned int spm_get_cpu_pwr_status(void);
extern long int spm_get_current_time_ms(void);

/*
 * for Deep Idle
 */
void spm_deepidle_init(void);
void spm_dpidle_before_wfi(int cpu);		 /* can be redefined */
void spm_dpidle_after_wfi(int cpu, u32 spm_debug_flag);		 /* can be redefined */
unsigned int spm_go_to_dpidle(u32 spm_flags, u32 spm_data, u32 dump_log);
unsigned int spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data);
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace);
bool spm_set_dpidle_pcm_init_flag(void);

#define DEEPIDLE_LOG_NONE      0
#define DEEPIDLE_LOG_REDUCED   1
#define DEEPIDLE_LOG_FULL      2

/*
 * for Screen On Deep Idle 3.0
 */
void spm_sodi3_init(void);
unsigned int spm_go_to_sodi3(u32 spm_flags, u32 spm_data, u32 sodi_flags);
void spm_enable_sodi3(bool);
bool spm_get_sodi3_en(void);

/*
 * for Screen On Deep Idle
 */
void spm_sodi_init(void);
unsigned int spm_go_to_sodi(u32 spm_flags, u32 spm_data, u32 sodi_flags);
void spm_enable_sodi(bool);
bool spm_get_sodi_en(void);

void spm_sodi_set_vdo_mode(bool vdo_mode);
bool spm_get_cmd_mode(void);
void spm_sodi_mempll_pwr_mode(bool pwr_mode);
bool spm_get_sodi_mempll(void);

enum mtk_sodi_flag {
	SODI_FLAG_3P0         = (1 << 0),
	SODI_FLAG_RESIDENCY   = (1 << 1),
	SODI_FLAG_REDUCE_LOG  = (1 << 2),
	SODI_FLAG_NO_LOG      = (1 << 3),
	SODI_FLAG_DUMP_REG    = (1 << 4),
	SODI_FLAG_VCORE_OPP   = (1 << 16),
};

/*
 * for Multi Core Deep Idle
 */
enum spm_mcdi_lock_id {
	SPM_MCDI_IDLE = 0,
	SPM_MCDI_VCORE_DVFS = 1,
	SPM_MCDI_EARLY_SUSPEND = 2,
};

void mcidle_before_wfi(int cpu);
void mcidle_after_wfi(int cpu);
void spm_mcdi_init(void);
void spm_mcdi_switch_on_off(enum spm_mcdi_lock_id id, int mcdi_en);
bool spm_mcdi_wfi(int core_id);
bool spm_mcdi_can_enter(void);
bool spm_is_cpu_irq_occur(int core_id);

bool go_to_mcidle(int cpu);

#endif
