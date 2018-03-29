/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MT_SPM_SLEEP_
#define _MT_SPM_SLEEP_

#include <linux/kernel.h>
#include "mt_spm.h"
/*
 * for suspend
 */
extern int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
extern u32 spm_get_sleep_wakesrc(void);
extern wake_reason_t spm_go_to_sleep(u32 spm_flags, u32 spm_data);

extern bool spm_is_md_sleep(void);
extern bool spm_is_md1_sleep(void);
extern bool spm_is_md2_sleep(void);
extern bool spm_is_conn_sleep(void);
/* extern void spm_set_wakeup_src_check(void); */
extern void spm_ap_mdsrc_req(u8 set);
extern bool spm_set_suspned_pcm_init_flag(u32 *suspend_flags);

extern void spm_output_sleep_option(void);

/* record last wakesta */
extern u32 spm_get_last_wakeup_src(void);
extern u32 spm_get_last_wakeup_misc(void);
extern u32 spm_get_register(void __force __iomem *offset);
extern void spm_set_register(void __force __iomem *offset, u32 value);
#endif
