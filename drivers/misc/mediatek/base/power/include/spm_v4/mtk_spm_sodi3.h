/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SPM_SODI3_H__
#define __MTK_SPM_SODI3_H__

#include <mtk_cpuidle.h>
#include "mtk_spm_idle.h"
#include "mtk_spm_misc.h"
#include "mtk_spm_pmic_wrap.h"
#include "mtk_spm_internal.h"
#include "mtk_spm_sodi.h"

enum spm_sodi3_step {
	SPM_SODI3_ENTER = 0,
	SPM_SODI3_ENTER_SSPM_ASYNC_IPI_BEFORE_WFI,
	SPM_SODI3_ENTER_SPM_FLOW,
	SPM_SODI3_ENTER_UART_SLEEP,
	SPM_SODI3_B4,
	SPM_SODI3_B5,
	SPM_SODI3_B6,
	SPM_SODI3_ENTER_WFI,
	SPM_SODI3_LEAVE_WFI,
	SPM_SODI3_ENTER_UART_AWAKE,
	SPM_SODI3_LEAVE_SSPM_ASYNC_IPI_AFTER_WFI,
	SPM_SODI3_LEAVE_SPM_FLOW,
	SPM_SODI3_LEAVE,
};

#define CPU_FOOTPRINT_SHIFT 24

#if SPM_AEE_RR_REC
void __attribute__((weak)) aee_rr_rec_sodi3_val(u32 val)
{
}

u32 __attribute__((weak)) aee_rr_curr_sodi3_val(void)
{
	return 0;
}
#endif

static inline void spm_sodi3_footprint(enum spm_sodi3_step step)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_sodi3_val(aee_rr_curr_sodi3_val() |
			     (1 << step) |
			     (smp_processor_id() << CPU_FOOTPRINT_SHIFT));
#endif
}

static inline void spm_sodi3_footprint_val(u32 val)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_sodi3_val(aee_rr_curr_sodi3_val() |
			     val |
			     (smp_processor_id() << CPU_FOOTPRINT_SHIFT));
#endif
}

static inline void spm_sodi3_aee_init(void)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_sodi3_val(0);
#endif
}

#define spm_sodi3_reset_footprint() spm_sodi3_aee_init()

extern void spm_sodi3_post_process(void);
extern void spm_sodi3_pre_process(struct pwr_ctrl *pwrctrl,
				  u32 operation_cond);
extern void spm_sodi3_pcm_setup_before_wfi(
		u32 cpu, struct pcm_desc *pcmdesc,
		struct pwr_ctrl *pwrctrl, u32 operation_cond);

#endif /* __MTK_SPM_SODI3_H__ */

