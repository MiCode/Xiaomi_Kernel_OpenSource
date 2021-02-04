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

#ifndef __MT_SPM_MCSODI_H_
#define __MT_SPM_MCSODI_H_

#include "mtk_cpuidle.h"
#include "mtk_spm_idle.h"
#include "mtk_spm_misc.h"
#include "mtk_spm_internal.h"
#include "mtk_spm_pmic_wrap.h"


#define MCSODI_TAG     "[MS] "
#define ms_err(fmt, args...)	pr_info(MCSODI_TAG fmt, ##args)
#define ms_warn(fmt, args...)	pr_info(MCSODI_TAG fmt, ##args)
#define ms_debug(fmt, args...)	pr_debug(MCSODI_TAG fmt, ##args)

#define WAKE_SRC_FOR_MCSODI \
	(WAKE_SRC_R12_PCM_TIMER | WAKE_SRC_R12_CPU_IRQ_B)

#define WAKE_SRC_FOR_MD32	0

#define SPM_BYPASS_SYSPWREQ	0

enum spm_mcsodi_step {
	SPM_MCSODI_ENTER,
	SPM_MCSODI_SPM_FLOW,
	SPM_MCSODI_B2,
	SPM_MCSODI_B3,
	SPM_MCSODI_B4,
	SPM_MCSODI_B5,
	SPM_MCSODI_B6,
	SPM_MCSODI_ENTER_WFI,
	SPM_MCSODI_LEAVE_WFI,
	SPM_MCSODI_LEAVE,
};


#if SPM_AEE_RR_REC
void __attribute__((weak)) aee_rr_rec_mcsodi_val(u32 val)
{
}

u32 __attribute__((weak)) aee_rr_curr_mcsodi_val(void)
{
	return 0;
}
#endif

static inline void spm_mcsodi_footprint(enum spm_mcsodi_step step)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_mcsodi_val(aee_rr_curr_mcsodi_val() | (1 << step));
#endif
}

static inline void spm_mcsodi_footprint_val(u32 val)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_mcsodi_val(aee_rr_curr_mcsodi_val() | val);
#endif
}

static inline void spm_mcsodi_aee_init(void)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_mcsodi_val(0);
#endif
}

#define spm_mcsodi_reset_footprint() spm_mcsodi_aee_init()


#endif /* __MT_SPM_MCSODI_H_ */

