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

#include "autok.h"

#define SDIO_DVFS_TIMEOUT       (HZ/100 * 5)    /* 10ms x5 */
#define SDIO_FIX_VCORE_CONDITIONAL

enum AUTOK_VCORE {
	AUTOK_VCORE_LOW = 0,
	AUTOK_VCORE_HIGH,
	AUTOK_VCORE_NUM
};

/**********************************************************
 * Variable Declaration                                   *
 **********************************************************/
extern u8 sdio_autok_res[2][TUNING_PARAM_COUNT];
extern u8 emmc_autok_res[2][TUNING_PARAM_COUNT];
extern u8 sd_autok_res[2][TUNING_PARAM_COUNT];

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
extern int sdio_autok_res_apply(struct msdc_host *host, int vcore);
extern int sdio_autok_res_save(struct msdc_host *host, int vcore, u8 *res);
extern void sdio_autok_wait_dvfs_ready(void);

extern int autok_res_check(u8 *res_h, u8 *res_l);
extern int sdio_version(struct msdc_host *host);
extern void sdio_unreq_vcore(struct work_struct *work);
extern void sdio_set_vcore_performance(struct msdc_host *host, u32 enable);
void sdio_set_vcorefs_sram(int vcore, int done, struct msdc_host *host);

#endif /* _AUTOK_DVFS_H_ */

