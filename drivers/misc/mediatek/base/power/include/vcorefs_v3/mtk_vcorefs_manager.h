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

#ifndef _MTK_VCOREFS_MANAGER_H
#define _MTK_VCOREFS_MANAGER_H

#include <mtk_vcorefs_governor.h>

extern int is_vcorefs_can_work(void);
extern bool is_vcorefs_request(void);
extern int vcorefs_each_kicker_request(enum dvfs_kicker kicker);
extern int vcorefs_request_dvfs_opp(enum dvfs_kicker, enum dvfs_opp);
extern void vcorefs_drv_init(int plat_init_opp);
extern int init_vcorefs_sysfs(void);
extern u32 log_mask(void);

/* MET */
extern void vcorefs_register_req_notify(void (*vcorefs_req_handler)
				(enum dvfs_kicker kicker, enum dvfs_opp opp));

/* AEE */
extern void aee_rr_rec_vcore_dvfs_opp(u32 val);
extern u32 aee_rr_curr_vcore_dvfs_opp(void);
extern void aee_rr_rec_vcore_dvfs_status(u32 val);
extern u32 aee_rr_curr_vcore_dvfs_status(void);

extern int spm_msdc_dvfs_setting(int msdc, bool enable);

#endif	/* _MTK_VCOREFS_MANAGER_H */
