/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _AUTOK_DVFS_H_
#define _AUTOK_DVFS_H_

#define VCOREFS_READY

#include "autok.h"

#ifdef VCOREFS_READY
#include <mtk_vcorefs_manager.h>
#include <mtk_spm_vcore_dvfs.h>

enum AUTOK_VCORE {
	AUTOK_VCORE_LEVEL0 = 0,
	AUTOK_VCORE_LEVEL1,
	AUTOK_VCORE_LEVEL2,
	AUTOK_VCORE_LEVEL3,
	AUTOK_VCORE_MERGE,
	AUTOK_VCORE_NUM = AUTOK_VCORE_MERGE
};

#else
enum dvfs_opp {
	OPP_UNREQ = -1,
	OPP_0 = 0,
	OPP_1,
	OPP_2,
	OPP_3,
	NUM_OPP,
};

/* define as any value is ok, just for build pass */
#define KIR_SDIO	1
#define KIR_AUTOK_EMMC	9
#define KIR_AUTOK_SDIO	10
#define KIR_AUTOK_SD	11

#define is_vcorefs_can_work() -1
#define vcorefs_request_dvfs_opp(a, b)	0
#define vcorefs_get_hw_opp() OPP_0
#define spm_msdc_dvfs_setting(a, b)

#define AUTOK_VCORE_LEVEL0	0
#define AUTOK_VCORE_LEVEL1	0
#define AUTOK_VCORE_LEVEL2	0
#define AUTOK_VCORE_LEVEL3	0
#define AUTOK_VCORE_MERGE       0
#define AUTOK_VCORE_NUM		1
#endif

#define MSDC_DVFS_TIMEOUT       (HZ/100 * 5)    /* 10ms x5 */

#define BACKUP_REG_COUNT_SDIO           14
#define BACKUP_REG_COUNT_EMMC_INTERNAL  5
#define BACKUP_REG_COUNT_EMMC_TOP       12
#define BACKUP_REG_COUNT_EMMC \
	(BACKUP_REG_COUNT_EMMC_INTERNAL + BACKUP_REG_COUNT_EMMC_TOP)

#define MSDC_DVFS_SET_SIZE      0x48
#define MSDC_TOP_SET_SIZE       0x30

#define SDIO_HW_DVFS_CONDITIONAL

/**********************************************************
 * Function Declaration                                    *
 **********************************************************/
extern int sdio_autok_res_exist(struct msdc_host *host);
extern int sdio_autok_res_apply(struct msdc_host *host, int vcore);
extern int sdio_autok_res_save(struct msdc_host *host, int vcore, u8 *res);
extern void sdio_autok_wait_dvfs_ready(void);
extern int emmc_execute_dvfs_autok(struct msdc_host *host, u32 opcode);
extern int sd_execute_dvfs_autok(struct msdc_host *host, u32 opcode);
extern void sdio_execute_dvfs_autok(struct msdc_host *host);

extern int autok_res_check(u8 *res_h, u8 *res_l);
extern void msdc_dvfs_reg_backup_init(struct msdc_host *host);
extern void msdc_dvfs_reg_restore(struct msdc_host *host);
extern void msdc_dump_autok(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
extern int msdc_vcorefs_get_hw_opp(struct msdc_host *host);

#endif /* _AUTOK_DVFS_H_ */

