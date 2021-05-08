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

#if defined(CONFIG_MTK_DRAMC)
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#include <mtk_spm_internal.h>

int spmfw_idx = -1;

#if defined(CONFIG_MTK_DRAMC)
/***********************************************************
 * SPM Golden Seting API(MEMPLL Control, DRAMC)
 ***********************************************************/
static struct ddrphy_golden_cfg ddrphy_setting_lp4_2ch[] = {
	/* DRAMC */
	{DRAMC_AO_CHA, 0x038, 0xC0000027, 0xC0000007},
	{DRAMC_AO_CHB, 0x038, 0xC0000027, 0xC0000007},
	{DRAMC_AO_CHA, 0x03C, 0xFF99FFFF, 0x10000000},
	{DRAMC_AO_CHB, 0x03C, 0xFF99FFFF, 0x10000000},
	{DRAMC_AO_CHA, 0x048, 0x0000F000, 0x00000000},
	{DRAMC_AO_CHB, 0x048, 0x0000F000, 0x00000000},
	{DRAMC_AO_CHA, 0x050, 0x00000080, 0x00000000},
	{DRAMC_AO_CHB, 0x050, 0x00000080, 0x00000000},
	{DRAMC_AO_CHA, 0x0D0, 0x01000000, 0x01000000},
	{DRAMC_AO_CHB, 0x0D0, 0x01000000, 0x01000000},
	{DRAMC_AO_CHA, 0x0D4, 0x00000004, 0x00000000},
	{DRAMC_AO_CHB, 0x0D4, 0x00000004, 0x00000000},
	{DRAMC_AO_CHA, 0x218, 0x00020000, 0x00000000},
	{DRAMC_AO_CHB, 0x218, 0x00020000, 0x00000000},
	{DRAMC_AO_CHA, 0x6E4, 0x00080000, 0x00000000},
	{DRAMC_AO_CHB, 0x6E4, 0x00080000, 0x00000000},

	/* DDRPHY */
	{PHY_AO_CHA, 0x00BC, 0x00000010, 0x00000010},
	{PHY_AO_CHB, 0x00BC, 0x00000010, 0x00000010},
	{PHY_AO_CHA, 0x00D0, 0x000003FF, 0x00000000},
	{PHY_AO_CHB, 0x00D0, 0x000003FF, 0x00000000},
	{PHY_AO_CHA, 0x013C, 0x00000010, 0x00000010},
	{PHY_AO_CHB, 0x013C, 0x00000010, 0x00000010},
	{PHY_AO_CHA, 0x0150, 0x000003FF, 0x00000000},
	{PHY_AO_CHB, 0x0150, 0x000003FF, 0x00000000},
	{PHY_AO_CHA, 0x0184, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0184, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x01D0, 0x00001FFF, 0x000011C0},
	{PHY_AO_CHB, 0x01D0, 0x00001FFF, 0x000011C0},
	{PHY_AO_CHA, 0x0260, 0x600BFF00, 0x20000100},
	{PHY_AO_CHB, 0x0260, 0x600BFF00, 0x20000100},
	{PHY_AO_CHA, 0x028C, 0x04000040, 0x00000000},
	{PHY_AO_CHB, 0x028C, 0x04000040, 0x00000000},
	{PHY_AO_CHA, 0x0294, 0xFFFFFFFF, 0x11400000},
	{PHY_AO_CHB, 0x0294, 0xFFFFFFFF, 0x11400000},
	{PHY_AO_CHA, 0x0298, 0x00770000, 0x00770000},
	{PHY_AO_CHB, 0x0298, 0x00770000, 0x00770000},
	{PHY_AO_CHA, 0x029C, 0x08000000, 0x08000000},
	{PHY_AO_CHB, 0x029C, 0x08000000, 0x08000000},
	{PHY_AO_CHA, 0x02A0, 0x84FF7F7E, 0x8000300C},
	{PHY_AO_CHB, 0x02A0, 0x84FF7F7E, 0x8000300C},
	{PHY_AO_CHA, 0x02AC, 0x000001FF, 0x000001FF},
	{PHY_AO_CHB, 0x02AC, 0x000001FF, 0x000001FF},
	{PHY_AO_CHA, 0x02B0, 0xFFFFE3FF, 0xC20122A4},
	{PHY_AO_CHB, 0x02B0, 0xFFFFE3FF, 0xC20122A4},
	{PHY_AO_CHA, 0x0308, 0x00000003, 0x00000003},
	{PHY_AO_CHB, 0x0308, 0x00000003, 0x00000003},
	{PHY_AO_CHA, 0x0318, 0x000FFF11, 0x000BBB11},
	{PHY_AO_CHB, 0x0318, 0x000FFF11, 0x000BBB11},
	{PHY_AO_CHA, 0x0C1C, 0x000F0000, 0x000E0000},
	{PHY_AO_CHB, 0x0C1C, 0x000F0000, 0x000E0000},
	{PHY_AO_CHA, 0x0C20, 0xFFF80000, 0x00200000},
	{PHY_AO_CHB, 0x0C20, 0xFFF80000, 0x00200000},
	{PHY_AO_CHA, 0x0C34, 0x01000001, 0x01000001},
	{PHY_AO_CHB, 0x0C34, 0x01000001, 0x01000001},
	{PHY_AO_CHA, 0x0C9C, 0x000F0000, 0x000E0000},
	{PHY_AO_CHB, 0x0C9C, 0x000F0000, 0x000E0000},
	{PHY_AO_CHA, 0x0CA0, 0xFFF80000, 0x00200000},
	{PHY_AO_CHB, 0x0CA0, 0xFFF80000, 0x00200000},
	{PHY_AO_CHA, 0x0CB4, 0x01000001, 0x01000001},
	{PHY_AO_CHB, 0x0CB4, 0x01000001, 0x01000001},
	{PHY_AO_CHA, 0x0D1C, 0x000F0001, 0x000A0000},
	{PHY_AO_CHB, 0x0D1C, 0x000F0001, 0x000A0000},
	{PHY_AO_CHA, 0x0D20, 0xFC300000, 0x00000000},
	{PHY_AO_CHB, 0x0D20, 0xFC300000, 0x00000000},
	{PHY_AO_CHA, 0x0D34, 0x01000001, 0x01000001},
	{PHY_AO_CHB, 0x0D34, 0x01000001, 0x01000001},
	{PHY_AO_CHA, 0x0DD8, 0x00000001, 0x00000001},
	/* {PHY_AO_CHB, 0x0DD8, 0x00000001, 0x00000001}, */
};

static int spm_dram_golden_setting_cmp(bool en)
{
	int i, ddrphy_num, r = 0;
	struct ddrphy_golden_cfg *ddrphy_setting;

	if (!en)
		return r;

	ddrphy_setting = ddrphy_setting_lp4_2ch;
	ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_2ch);

	/*Compare Dramc Goldeing Setting */
	for (i = 0; i < ddrphy_num; i++) {
		u32 value;

		value = lpDram_Register_Read(ddrphy_setting[i].base,
					ddrphy_setting[i].offset);
		if ((value & ddrphy_setting[i].mask) !=
				ddrphy_setting[i].value) {
			printk_deferred(
				"[name:spm&][SPM] NO dramc mismatch addr: 0x%.2x, offset: 0x%.3x, ",
				ddrphy_setting[i].base,
				ddrphy_setting[i].offset);
			printk_deferred(
				"[name:spm&]mask: 0x%.8x, val: 0x%x, read: 0x%x\n",
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
	if (spm_dram_golden_setting_cmp(1) != 0)
		aee_kernel_warning("SPM Warning",
			"dram golden setting mismach");
#endif /* CONFIG_MTK_DRAMC && !CONFIG_FPGA_EARLY_PORTING */

	spm_phypll_mode_check();
}

