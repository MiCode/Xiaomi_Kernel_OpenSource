/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SLEEP_H__
#define __MTK_SLEEP_H__

#include <linux/kernel.h>
#include "mtk_spm.h"
#include "mtk_spm_sleep.h"

#define WAKE_SRC_CFG_KEY            (1U << 31)

void spm_suspend_debugfs_init(void *entry);

extern int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on);

extern unsigned int slp_get_wake_reason(void);
extern void slp_set_infra_on(bool infra_on);

extern void slp_set_auto_suspend_wakelock(bool lock);
extern void slp_start_auto_suspend_resume_timer(u32 sec);
extern void slp_create_auto_suspend_resume_thread(void);

extern void slp_module_init(void);
extern void subsys_if_on(void);
extern void pll_if_on(void);
#endif
