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

#ifndef __MTK_SPM_SLEEP_H__
#define __MTK_SPM_SLEEP_H__

#include <linux/kernel.h>
#include "mtk_spm.h"
#include "mtk_spm_internal.h"
/*
 * for suspend
 */
extern int spm_ap_mdsrc_req_cnt;

extern int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
extern u32 spm_get_sleep_wakesrc(void);
extern unsigned int spm_go_to_sleep(u32 spm_flags, u32 spm_data);

extern bool spm_is_md_sleep(void);
extern bool spm_is_md1_sleep(void);
extern bool spm_is_md2_sleep(void);
extern bool spm_is_conn_sleep(void);
/* extern void spm_set_wakeup_src_check(void); */
extern void spm_ap_mdsrc_req(u8 set);
extern ssize_t get_spm_sleep_count(char *ToUserBuf, size_t sz, void *priv);
extern ssize_t get_spm_last_wakeup_src(char *ToUserBuf, size_t sz, void *priv);
extern bool spm_set_suspned_pcm_init_flag(u32 *suspend_flags);

extern void spm_output_sleep_option(void);

/* record last wakesta */
extern u32 spm_get_last_wakeup_src(void);
extern u32 spm_get_last_wakeup_misc(void);

extern void spm_suspend_pre_process(struct pwr_ctrl *pwrctrl);
extern void spm_suspend_post_process(struct pwr_ctrl *pwrctrl);
extern void spm_set_sysclk_settle(void);
#endif
