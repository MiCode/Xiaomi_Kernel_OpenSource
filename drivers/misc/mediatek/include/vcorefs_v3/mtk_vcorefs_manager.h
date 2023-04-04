/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_VCOREFS_MANAGER_H
#define _MTK_VCOREFS_MANAGER_H

#include "mtk_vcorefs_governor.h"

extern int is_vcorefs_can_work(void);
extern bool is_vcorefs_request(void);
extern int vcorefs_each_kicker_request(enum dvfs_kicker kicker);
extern int vcorefs_request_dvfs_opp(enum dvfs_kicker, enum dvfs_opp);
extern void vcorefs_drv_init(int plat_init_opp);
extern int init_vcorefs_sysfs(void);
extern u32 log_mask(void);

/* MET */
typedef void (*vcorefs_req_handler_t) (enum dvfs_kicker kicker,
		enum dvfs_opp opp);
extern void vcorefs_register_req_notify(vcorefs_req_handler_t handler);

/* AEE */
extern void aee_rr_rec_vcore_dvfs_opp(u32 val);
extern u32 aee_rr_curr_vcore_dvfs_opp(void);
extern void aee_rr_rec_vcore_dvfs_status(u32 val);
extern u32 aee_rr_curr_vcore_dvfs_status(void);

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
extern void ccci_set_svcorefs_request_dvfs_cb(int (*vcorefs_request_dvfs_opp)(enum dvfs_kicker, enum dvfs_opp));
#endif

extern int spm_msdc_dvfs_setting(int msdc, bool enable);
extern void __iomem *vcore_dvfsrc_base;
#define VCORE_DVFSRC_BASE (vcore_dvfsrc_base)
#define vcorefs_read(addr)			__raw_readl((void __force __iomem *)(addr))
#define vcorefs_write(addr, val)		mt_reg_sync_writel(val, addr)

#endif	/* _MTK_VCOREFS_MANAGER_H */
