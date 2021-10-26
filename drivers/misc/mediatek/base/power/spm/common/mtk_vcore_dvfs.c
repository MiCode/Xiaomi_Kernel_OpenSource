/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <mt-plat/mtk_secure_api.h>
#include <mtk_spm_internal.h>

static void spm_dvfsfw_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_0, 0, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_go_to_vcorefs(int spm_flags)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_1, spm_flags, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

void mtk_spmfw_init(int dvfsrc_en, int skip_check)
{
	if (is_spm_enabled() != 0x0 && !skip_check)
		return;

	spm_dvfsfw_init();

	spm_go_to_vcorefs(spm_dvfs_flag_init(dvfsrc_en));
}

void spm_dvfs_pwrap_cmd(int pwrap_cmd, int pwrap_vcore)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_3, pwrap_cmd, pwrap_vcore, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}
