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

#ifndef _AUTOK_DVFS_H_
#define _AUTOK_DVFS_H_
#include "msdc_cust.h"

#include "autok.h"

#define VCOREFS_READY
#if defined(VCOREFS_READY)
#include <linux/pm_qos.h>
#include "helio-dvfsrc-opp.h"
#endif

enum AUTOK_VCORE {
	AUTOK_VCORE_LEVEL0 = 0,
	AUTOK_VCORE_LEVEL1,
	AUTOK_VCORE_LEVEL2,
	AUTOK_VCORE_LEVEL3,
	AUTOK_VCORE_MERGE,
	AUTOK_VCORE_NUM = AUTOK_VCORE_MERGE
};

#define MSDC_DVFS_TIMEOUT       (HZ/100 * 5)     /* 10ms x5 */

#define MSDC_DVFS_SET_SIZE      0x48
#define MSDC_TOP_SET_SIZE       0x30
#define SDIO_DVFS_TIMEOUT       (HZ/100 * 5)    /* 10ms x5 */
/* Enable later@Peter */
/* #define SDIO_HW_DVFS_CONDITIONAL */

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
extern int sdio_autok_res_exist(struct msdc_host *host);
extern int sdio_autok_res_apply(struct msdc_host *host, int vcore);
extern int sdio_autok_res_save(struct msdc_host *host, int vcore, u8 *res);
extern void sdio_autok_wait_dvfs_ready(void);
extern int emmc_execute_dvfs_autok(struct msdc_host *host, u32 opcode);
extern int sd_execute_dvfs_autok(struct msdc_host *host, u32 opcode);
extern void sdio_execute_dvfs_autok(struct msdc_host *host);

extern int autok_res_check(u8 *res_h, u8 *res_l);
extern void sdio_set_hw_dvfs(int vcore, int done, struct msdc_host *host);
extern void sdio_dvfs_reg_restore(struct msdc_host *host);
extern void msdc_dump_autok(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
extern void msdc_dvfs_reg_backup_init(struct msdc_host *host);
extern void msdc_dvfs_reg_restore(struct msdc_host *host);
#endif /* _AUTOK_DVFS_H_ */

