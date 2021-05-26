/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <mtk_spm.h>

/*
 * for suspend
 */
extern bool spm_is_md_sleep(void);
extern bool spm_is_md1_sleep(void);
extern bool spm_is_md2_sleep(void);
extern bool spm_is_conn_sleep(void);
extern void spm_ap_mdsrc_req(u8 set);
extern ssize_t get_spm_sleep_count(char *ToUserBuf
			, size_t sz, void *priv);
extern ssize_t get_spm_last_wakeup_src(char *ToUserBuf
			, size_t sz, void *priv);
extern ssize_t get_spm_last_debug_flag(char *ToUserBuf
			, size_t sz, void *priv);
extern void spm_adsp_mem_protect(void);
extern void spm_adsp_mem_unprotect(void);

/* record last wakesta */
extern u32 spm_get_last_wakeup_src(void);
extern u32 spm_get_last_wakeup_misc(void);
#endif /* __MTK_SPM_SLEEP_H__ */
