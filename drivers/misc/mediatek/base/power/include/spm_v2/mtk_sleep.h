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

#ifndef _MT_SLEEP_
#define _MT_SLEEP_

#include <linux/kernel.h>
#include "mtk_spm.h"
#include "mtk_spm_sleep.h"

#define WAKE_SRC_CFG_KEY            (1U << 31)

extern int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on);

extern unsigned int slp_get_wake_reason(void);
extern bool slp_will_infra_pdn(void);
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
extern void slp_set_infra_on(bool infra_on);
#endif
extern void slp_pasr_en(bool en, u32 value);
extern void slp_dpd_en(bool en);

extern void slp_set_auto_suspend_wakelock(bool lock);
extern void slp_start_auto_suspend_resume_timer(u32 sec);
extern void slp_create_auto_suspend_resume_thread(void);

extern void slp_module_init(void);
#endif
