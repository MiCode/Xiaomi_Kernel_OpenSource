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

#include <linux/kernel.h>
#include <linux/of_address.h>
#include <mt-plat/aee.h>	/* aee_xxx */

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#include <mtk_spm_internal.h>

int spmfw_idx = -1;

#ifdef CONFIG_MTK_DRAMC
/***********************************************************
 * SPM Golden Seting API(MEMPLL Control, DRAMC)
 ***********************************************************/
static struct ddrphy_golden_cfg ddrphy_setting_lp4_2ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007}, //REG_DRAMC_PD_CTRL
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007}, //REG_DRAMC_PD_CTRL
	{PHY_AO_CHA, 0x0088, 0x880AEC00, 0x00000000}, //B0_DLL_ARPI2
	{PHY_AO_CHB, 0x0088, 0x880AEC00, 0x00000000}, //B0_DLL_ARPI2
	{PHY_AO_CHA, 0x008C, 0x000AE800, 0x0002E800}, //B0_DLL_ARPI3
	{PHY_AO_CHB, 0x008C, 0x000AE800, 0x0002E800}, //B0_DLL_ARPI3
	{PHY_AO_CHA, 0x0108, 0x880AEC00, 0x00000000}, //B1_DLL_ARPI2
	{PHY_AO_CHB, 0x0108, 0x880AEC00, 0x00000000}, //B1_DLL_ARPI2
	{PHY_AO_CHA, 0x010C, 0x000AE800, 0x0002E800}, //B1_DLL_ARPI3
	{PHY_AO_CHB, 0x010C, 0x000AE800, 0x0002E800}, //B1_DLL_ARPI3
	{PHY_AO_CHA, 0x0188, 0x880AAC00, 0x00000800}, //CA_DLL_ARPI2
	{PHY_AO_CHB, 0x0188, 0x880AAC00, 0x00000800}, //CA_DLL_ARPI2
	{PHY_AO_CHA, 0x018C, 0x000BA800, 0x000BA000}, //CA_DLL_ARPI3
	{PHY_AO_CHB, 0x018C, 0x000BA800, 0x0003A000}, //CA_DLL_ARPI3
	{PHY_AO_CHA, 0x0274, 0xFBFFEFFF, 0xFBFFEFFF}, //MISC_SPM_CTRL0
	{PHY_AO_CHB, 0x0274, 0xFBFFEFFF, 0xFBFFEFFF}, //MISC_SPM_CTRL0
	{PHY_AO_CHA, 0x027C, 0xFFFFFFFF, 0xFFFFFFEF}, //MISC_SPM_CTRL2
	{PHY_AO_CHB, 0x027C, 0xFFFFFFFF, 0x7FFFFFEF}, //MISC_SPM_CTRL2
	{PHY_AO_CHA, 0x0298, 0x00778000, 0x00770000}, //MISC_CG_CTRL5
	{PHY_AO_CHB, 0x0298, 0x00778000, 0x00770000}, //MISC_CG_CTRL5
	{PHY_AO_CHA, 0x0C20, 0xFFF80000, 0x00200000}, //SHU1_B0_DQ8
	{PHY_AO_CHB, 0x0C20, 0xFFF80000, 0x00200000}, //SHU1_B0_DQ8
	{PHY_AO_CHA, 0x0CA0, 0xFFF80000, 0x00200000}, //SHU1_B1_DQ8
	{PHY_AO_CHB, 0x0CA0, 0xFFF80000, 0x00200000}, //SHU1_B1_DQ8
	{PHY_AO_CHA, 0x0D20, 0xFFF80000, 0x00000000}, //SHU1_CA_CMD8
	{PHY_AO_CHB, 0x0D20, 0xFFF80000, 0x00000000}, //SHU1_CA_CMD8
	{PHY_AO_CHA, 0x1120, 0xFFF80000, 0x00200000}, //SHU2_B0_DQ8
	{PHY_AO_CHB, 0x1120, 0xFFF80000, 0x00200000}, //SHU2_B0_DQ8
	{PHY_AO_CHA, 0x1220, 0xFFF80000, 0x00000000}, //SHU2_CA_CMD8
	{PHY_AO_CHB, 0x1220, 0xFFF80000, 0x00000000}, //SHU2_CA_CMD8
	{PHY_AO_CHA, 0x1620, 0xFFF80000, 0x00200000}, //SHU3_B0_DQ8
	{PHY_AO_CHB, 0x1620, 0xFFF80000, 0x00200000}, //SHU3_B0_DQ8
	{PHY_AO_CHA, 0x1720, 0xFFF80000, 0x00000000}, //SHU3_CA_CMD8
	{PHY_AO_CHB, 0x1720, 0xFFF80000, 0x00000000}, //SHU3_CA_CMD8
};

static struct ddrphy_golden_cfg ddrphy_setting_lp3_1ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007}, //REG_DRAMC_PD_CTRL
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007}, //REG_DRAMC_PD_CTRL
	{PHY_AO_CHA, 0x0088, 0x880AEC00, 0x00000800}, //B0_DLL_ARPI2
	{PHY_AO_CHB, 0x0088, 0x880AEC00, 0x00000000}, //B0_DLL_ARPI2
	{PHY_AO_CHA, 0x008C, 0x000AE800, 0x0002E000}, //B0_DLL_ARPI3
	{PHY_AO_CHB, 0x008C, 0x000AE800, 0x0002E800}, //B0_DLL_ARPI3
	{PHY_AO_CHA, 0x0108, 0x880AEC00, 0x00000000}, //B1_DLL_ARPI2
	{PHY_AO_CHB, 0x0108, 0x880AEC00, 0x00000000}, //B1_DLL_ARPI2
	{PHY_AO_CHA, 0x010C, 0x000AE800, 0x0002E800}, //B1_DLL_ARPI3
	{PHY_AO_CHB, 0x010C, 0x000AE800, 0x0002E800}, //B1_DLL_ARPI3
	{PHY_AO_CHA, 0x0188, 0x880AAC00, 0x00000800}, //CA_DLL_ARPI2
	{PHY_AO_CHB, 0x0188, 0x880AAC00, 0x00000000}, //CA_DLL_ARPI2
	{PHY_AO_CHA, 0x018C, 0x000BA800, 0x000BA000}, //CA_DLL_ARPI3
	{PHY_AO_CHB, 0x018C, 0x000BA800, 0x0003A800}, //CA_DLL_ARPI3
	{PHY_AO_CHA, 0x0274, 0xFBFFEFFF, 0xFBFFEFFF}, //MISC_SPM_CTRL0
	{PHY_AO_CHB, 0x0274, 0xFBFFEFFF, 0xFBFFEFFF}, //MISC_SPM_CTRL0
	{PHY_AO_CHA, 0x027C, 0xFFFFFFFF, 0x7FFFFFEF}, //MISC_SPM_CTRL2
	{PHY_AO_CHB, 0x027C, 0xFFFFFFFF, 0x7FFFFFEF}, //MISC_SPM_CTRL2
	{PHY_AO_CHA, 0x0298, 0x00778000, 0x00570000}, //MISC_CG_CTRL5
	{PHY_AO_CHB, 0x0298, 0x00778000, 0x00570000}, //MISC_CG_CTRL5
	{PHY_AO_CHA, 0x0C20, 0xFFF80000, 0x00000000}, //SHU1_B0_DQ8
	{PHY_AO_CHB, 0x0C20, 0xFFF80000, 0x00200000}, //SHU1_B0_DQ8
	{PHY_AO_CHA, 0x0CA0, 0xFFF80000, 0x00200000}, //SHU1_B1_DQ8
	{PHY_AO_CHB, 0x0CA0, 0xFFF80000, 0x00200000}, //SHU1_B1_DQ8
	{PHY_AO_CHA, 0x0D20, 0xFFF80000, 0x00000000}, //SHU1_CA_CMD8
	{PHY_AO_CHB, 0x0D20, 0xFFF80000, 0x01200000}, //SHU1_CA_CMD8
	{PHY_AO_CHA, 0x1120, 0xFFF80000, 0x00000000}, //SHU2_B0_DQ8
	{PHY_AO_CHB, 0x1120, 0xFFF80000, 0x00200000}, //SHU2_B0_DQ8
	{PHY_AO_CHA, 0x1220, 0xFFF80000, 0x00000000}, //SHU2_CA_CMD8
	{PHY_AO_CHB, 0x1220, 0xFFF80000, 0x01200000}, //SHU2_CA_CMD8
	{PHY_AO_CHA, 0x1620, 0xFFF80000, 0x00000000}, //SHU3_B0_DQ8
	{PHY_AO_CHB, 0x1620, 0xFFF80000, 0x00200000}, //SHU3_B0_DQ8
	{PHY_AO_CHA, 0x1720, 0xFFF80000, 0x00000000}, //SHU3_CA_CMD8
	{PHY_AO_CHB, 0x1720, 0xFFF80000, 0x01200000}, //SHU3_CA_CMD8
};

static int spm_dram_golden_setting_cmp(bool en)
{
	int i, ddrphy_num, r = 0;
	struct ddrphy_golden_cfg *ddrphy_setting;

	if (!en)
		return r;

	switch (spm_get_spmfw_idx()) {
	case SPMFW_LP4_2CH_2400:
	case SPMFW_LP4_2CH_3200:
		ddrphy_setting = ddrphy_setting_lp4_2ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_2ch);
		break;
	case SPMFW_LP4X_2CH_3200:
	case SPMFW_LP4X_2CH_2400:
		ddrphy_setting = ddrphy_setting_lp4_2ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_2ch);
		break;
	case SPMFW_LP3_1CH_1866:
		ddrphy_setting = ddrphy_setting_lp3_1ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp3_1ch);
		break;
	default:
		return r;
	}

	/*Compare Dramc Goldeing Setting */
	for (i = 0; i < ddrphy_num; i++) {
		u32 value;

		value = lpDram_Register_Read(ddrphy_setting[i].base,
					ddrphy_setting[i].offset);
		if ((value & ddrphy_setting[i].mask) !=
				ddrphy_setting[i].value) {
			pr_info(
				"[SPM] NO dramc mismatch addr: 0x%.2x, offset: 0x%.3x, ",
				ddrphy_setting[i].base,
				ddrphy_setting[i].offset);
			pr_info(
				"mask: 0x%.8x, val: 0x%x, read: 0x%x\n",
				ddrphy_setting[i].mask,
				ddrphy_setting[i].value,
				value);
			aee_sram_printk(
				"NO dramc mismatch addr: 0x%.2x, offset: 0x%.3x, ",
				ddrphy_setting[i].base,
				ddrphy_setting[i].offset);
			aee_sram_printk(
				"mask: 0x%.8x, val: 0x%x, read: 0x%x\n",
				ddrphy_setting[i].mask,
				ddrphy_setting[i].value,
				value);

			r = -EPERM;
		}
	}

	return r;

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
	if (spm_dram_golden_setting_cmp(1) != 0)
		aee_kernel_warning("SPM Warning",
			"dram golden setting mismach");
#endif /* CONFIG_MTK_DRAMC && !CONFIG_FPGA_EARLY_PORTING */

	spm_phypll_mode_check();
}

