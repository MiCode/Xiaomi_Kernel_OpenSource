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

#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/mtk_secure_api.h>

#include <mtk_spm.h>
#include <mtk_spm_idle.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_vcore_dvfs.h>
#include <mtk_spm_resource_req_internal.h>

void spm_dpidle_pre_process(unsigned int operation_cond,
			    struct pwr_ctrl *pwrctrl)
{
	__spm_sync_pcm_flags(pwrctrl);

	/* dvfsrc_md_scenario_update(1); */
}

void spm_dpidle_post_process(void)
{
	/* dvfsrc_md_scenario_update(0); */
}

void spm_dpidle_pcm_setup_before_wfi(bool sleep_dpidle,
				     u32 cpu, struct pcm_desc *pcmdesc,
				     struct pwr_ctrl *pwrctrl,
				     u32 operation_cond)
{
	unsigned int resource_usage = 0;

	spm_dpidle_pre_process(operation_cond, pwrctrl);

	/* Get SPM resource request and update reg_spm_xxx_req */
	resource_usage = (!sleep_dpidle) ? spm_get_resource_usage() : 0;

	SMC_CALL(MTK_SIP_KERNEL_SPM_DPIDLE_ARGS,
			pwrctrl->pcm_flags,
			pwrctrl->pcm_flags1,
			resource_usage);

	if (sleep_dpidle)
		SMC_CALL(MTK_SIP_KERNEL_SPM_SLEEP_DPIDLE_ARGS,
			       pwrctrl->timer_val, pwrctrl->wake_src, 0);
}

void spm_deepidle_chip_init(void)
{
}

