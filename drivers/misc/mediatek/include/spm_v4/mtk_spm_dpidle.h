/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SPM_DPIDLE_H__
#define __MTK_SPM_DPIDLE_H__

#include "mtk_spm.h"
#include "mtk_spm_internal.h"

extern void spm_dpidle_pre_process(
	unsigned int operation_cond, struct pwr_ctrl *pwrctrl);
extern void spm_dpidle_post_process(void);
extern void spm_deepidle_chip_init(void);
extern void spm_dpidle_notify_sspm_after_wfi_async_wait(void);
extern void spm_dpidle_pcm_setup_before_wfi(
	bool sleep_dpidle, u32 cpu, struct pcm_desc *pcmdesc,
	struct pwr_ctrl *pwrctrl, u32 operation_cond);

#endif /* __MTK_SPM_DPIDLE_H__ */

