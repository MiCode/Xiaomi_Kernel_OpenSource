/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/of_address.h>
#include <mt-plat/aee.h>	/* aee_xxx */

#if defined(CONFIG_MTK_DRAMC)
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#include <mtk_spm_internal.h>

int spmfw_idx = -1;

#if defined(CONFIG_MTK_DRAMC)

static int spm_dram_golden_setting_cmp(bool en)
{
	return 0;

}

#endif /* CONFIG_MTK_DRAMC */

static void spm_phypll_mode_check(void)
{
#if defined(CONFIG_MTK_DRAMC)
	unsigned int val = spm_read(SPM_POWER_ON_VAL0);

	if (val)
		aee_kernel_warning(
			"SPM Warning",
			"Invalid SPM_POWER_ON_VAL0: 0x%08x\n",
			val);
#endif
}

int spm_get_spmfw_idx(void)
{
	if (spmfw_idx == -1)
		spmfw_idx++;

	return spmfw_idx;
}
EXPORT_SYMBOL(spm_get_spmfw_idx);

void spm_do_dram_config_check(void)
{
#if defined(CONFIG_MTK_DRAMC)
	if (spm_dram_golden_setting_cmp(0) != 0)
		aee_kernel_warning("SPM Warning",
			"dram golden setting mismach");
#endif /* CONFIG_MTK_DRAMC && !CONFIG_FPGA_EARLY_PORTING */

	spm_phypll_mode_check();
}

