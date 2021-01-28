// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of_address.h>
#include <mt-plat/aee.h>	/* aee_xxx */

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#include <mtk_spm_internal.h>

int spmfw_idx = -1;

#ifdef CONFIG_MTK_DRAMC
static int spm_dram_golden_setting_cmp(bool en)
{
	return 0;
}

static void spm_dram_type_check(void)
{
	int ddr_type = get_ddr_type();
	int ddr_hz = dram_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4 && ddr_hz == 2400)
		spmfw_idx = SPMFW_LP4_2CH_2400;
	else if (ddr_type == TYPE_LPDDR4 && ddr_hz == 3200)
		spmfw_idx = SPMFW_LP4_2CH_3200;
	else if (ddr_type == TYPE_LPDDR4X && ddr_hz == 3200)
		spmfw_idx = SPMFW_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR4X && ddr_hz == 2400)
		spmfw_idx = SPMFW_LP4X_2CH_2400;
	else if (ddr_type == TYPE_LPDDR3)
		spmfw_idx = SPMFW_LP3_1CH_1866;

	pr_info("#@# %s(%d) __spmfw_idx 0x%x, ddr=[%d][%d]\n",
		__func__, __LINE__, spmfw_idx, ddr_type, ddr_hz);
}
#endif /* CONFIG_MTK_DRAMC */

static void spm_phypll_mode_check(void)
{
	unsigned int val = spm_read(SPM_POWER_ON_VAL0);

	if ((val & (R0_SC_PHYPLL_MODE_SW_PCM | R0_SC_PHYPLL2_MODE_SW_PCM))
			!= R0_SC_PHYPLL_MODE_SW_PCM) {

		aee_kernel_warning(
			"SPM Warning",
			"Invalid SPM_POWER_ON_VAL0: 0x%08x\n",
			val);
	}
}

int spm_get_spmfw_idx(void)
{
	if (spmfw_idx == -1) {
		spmfw_idx++;
#ifdef CONFIG_MTK_DRAMC
		spm_dram_type_check();
#endif /* CONFIG_MTK_DRAMC */
	}

	return spmfw_idx;
}
EXPORT_SYMBOL(spm_get_spmfw_idx);

void spm_do_dram_config_check(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_MTK_DRAMC)
	if (spm_dram_golden_setting_cmp(0) != 0)
		aee_kernel_warning("SPM Warning",
			"dram golden setting mismach");
#endif /* CONFIG_MTK_DRAMC && !CONFIG_FPGA_EARLY_PORTING */

	spm_phypll_mode_check();
}

