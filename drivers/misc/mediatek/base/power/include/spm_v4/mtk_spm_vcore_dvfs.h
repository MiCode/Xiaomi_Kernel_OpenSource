/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_SPM_VCORE_DVFS_H__
#define __MTK_SPM_VCORE_DVFS_H__

#include "mtk_spm.h"
#include <mtk_vcorefs_manager.h>

/* Feature will disable both of DVS/DFS are 0 */
#if defined(CONFIG_MACH_MT6739)
#define SPM_VCORE_DVS_EN       1
#define SPM_DDR_DFS_EN         1
#define SPM_MM_CLK_EN          0
#define VMODEM_VCORE_COBUCK    1
#elif defined(CONFIG_MACH_MT6771)
#define SPM_VCORE_DVS_EN       0 /* SB disabled */
#define SPM_DDR_DFS_EN         0 /* SB disabled */
#define SPM_MM_CLK_EN          0
#define VMODEM_VCORE_COBUCK    0 /* SB disabled */
#else
#define SPM_VCORE_DVS_EN       1
#define SPM_DDR_DFS_EN         1
#define SPM_MM_CLK_EN          0
#define VMODEM_VCORE_COBUCK    1
#endif

#if defined(CONFIG_MACH_MT6739)
#define SPM_DVFS_TIMEOUT       5000	/* 5ms */
#else
#define SPM_DVFS_TIMEOUT       1000	/* 1ms */
#endif

enum vcorefs_smc_cmd {
	VCOREFS_SMC_CMD_0,
	VCOREFS_SMC_CMD_1,
	VCOREFS_SMC_CMD_2,
	NUM_VCOREFS_SMC_CMD,
};

extern void spm_go_to_vcorefs(int spm_flags);
extern int spm_set_vcore_dvfs(struct kicker_config *krconf);
extern void spm_vcorefs_init(void);
extern int spm_dvfs_flag_init(void);
extern char *spm_vcorefs_dump_dvfs_regs(char *p);
extern u32 spm_vcorefs_get_MD_status(void);
extern int spm_vcorefs_pwarp_cmd(void);
extern int spm_vcorefs_get_opp(void);
extern void spm_request_dvfs_opp(int id, enum dvfs_opp opp);
extern u32 spm_vcorefs_get_md_srcclkena(void);
extern void dvfsrc_md_scenario_update(bool suspend);
void dvfsrc_mdsrclkena_control(bool on);
void dvfsrc_mdsrclkena_control_nolock(bool on);
void dvfsrc_msdc_autok_finish(void);

#endif /* __MTK_SPM_VCORE_DVFS_H__ */
