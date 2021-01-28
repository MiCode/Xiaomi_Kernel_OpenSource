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

#include <linux/kernel.h>

#include <mt-plat/mtk_devinfo.h>
#include <mtk_spm_internal.h>

#include "helio-dvfsrc-opp.h"

static int get_vb_volt(int vcore_opp)
{
	int ret = 0;
	int ptpod0 = get_devinfo_with_index(50);
	int ptpod10 = get_devinfo_with_index(60);

	pr_info("%s: PTPOD10: 0x%x\n", __func__, ptpod10);
	switch (vcore_opp) {
	case VCORE_OPP_0:
		ret = min(((ptpod10 >> 2) & 0x3), ((ptpod10 >> 6) & 0x3));
		ret = min(ret, ((ptpod10 >> 12) & 0x3));

		/* GPU 730 Mhz 0.8V VB */
		ptpod10 = (ptpod10 >> 8) & 0x3;
		/* if GPU 730 Mhz needs 0.8V */
		if (ptpod10 == 1)
			ret = 0;
		if (ret > 1) {
			pr_info("failed PTPOD10: 0x%x\n", ptpod10);
			ret = 0;
		}
		break;
	case VCORE_OPP_1:
		ret = min(((ptpod10 >> 0) & 0x3), ((ptpod10 >> 4) & 0x3));
		if (ret > 1) {
			pr_info("failed PTPOD10: 0x%x\n", ptpod10);
			ret = 0;
		}
		break;
	case VCORE_OPP_2:
		ret = (ptpod10 >> 10) & 0x3;
		if (ret == 3)
			ret = 2;
		break;
	case VCORE_OPP_3:
		ret = (ptpod10 >> 4) & 0x3F;
		if (ptpod0 == 0x0000FF00 || ptpod0 == 0x0)
			ret = 0;
		else if (ret == 0)
			ret = 1;
		else
			ret = 0;
		break;
	default:
		break;
	}
	return ret * 25000;
}

void dvfsrc_opp_level_mapping(void)
{
	int vcore_opp_0_uv, vcore_opp_1_uv, vcore_opp_2_uv, vcore_opp_3_uv;
	int is_vcore_ct = 0, is_mini_sqc = 0;
	int info2 = get_devinfo_with_index(132);

	if (!strncmp(CONFIG_ARCH_MTK_PROJECT,
				"k65v1_64_bsp_ctig", 17) ||
			!strncmp(CONFIG_ARCH_MTK_PROJECT,
				"k65v1_64_bsp_ctqc", 17) ||
			!strncmp(CONFIG_ARCH_MTK_PROJECT,
				"k62v1_64_bsp_ctgc", 17))
		is_vcore_ct = is_mini_sqc = 1;

	pr_info("flavor check: %s, is_vcore_ct: %d, is_mini_sqc: %d\n",
			CONFIG_ARCH_MTK_PROJECT,
			is_vcore_ct, is_mini_sqc);

	set_pwrap_cmd(VCORE_OPP_0, 0);
	set_pwrap_cmd(VCORE_OPP_1, 2);
	set_pwrap_cmd(VCORE_OPP_2, 3);
	set_pwrap_cmd(VCORE_OPP_3, 4);

	if (is_vcore_ct) {
		vcore_opp_0_uv = 800000 - get_vb_volt(VCORE_OPP_0);
		vcore_opp_1_uv = 700000 - get_vb_volt(VCORE_OPP_1);
		vcore_opp_2_uv = 700000 - get_vb_volt(VCORE_OPP_2);
		vcore_opp_3_uv = 650000 + get_vb_volt(VCORE_OPP_3);
		pr_info("%s: EFUSE vcore_opp_uv: %d, %d, %d, %d\n", __func__,
				vcore_opp_0_uv,
				vcore_opp_1_uv,
				vcore_opp_2_uv,
				vcore_opp_3_uv);

		if (is_mini_sqc) {
			vcore_opp_0_uv -= 31250;
			vcore_opp_1_uv -= 31250;
			vcore_opp_2_uv -= 31250;
			vcore_opp_3_uv = min(vcore_opp_2_uv, vcore_opp_3_uv);
			pr_info("%s: MINI SQC vcore_opp_uv: %d, %d, %d, %d\n",
					__func__,
					vcore_opp_0_uv,
					vcore_opp_1_uv,
					vcore_opp_2_uv,
					vcore_opp_3_uv);
		}

		vcore_opp_2_uv = max(vcore_opp_2_uv, vcore_opp_3_uv);
		vcore_opp_1_uv = max(vcore_opp_0_uv - 100000, vcore_opp_1_uv);
		vcore_opp_1_uv = max(vcore_opp_1_uv, vcore_opp_2_uv);
		pr_info("%s: DRAM vcore_opp_uv: %d, %d, %d, %d\n", __func__,
				vcore_opp_0_uv,
				vcore_opp_1_uv,
				vcore_opp_2_uv,
				vcore_opp_3_uv);
	} else {
		vcore_opp_0_uv = 800000;
		vcore_opp_1_uv = 700000;
		vcore_opp_3_uv = 650000 + get_vb_volt(VCORE_OPP_3);
		/* apply MD VB */
		vcore_opp_2_uv = 700000 - get_vb_volt(VCORE_OPP_2);
		vcore_opp_2_uv = max(vcore_opp_2_uv, vcore_opp_3_uv);
		pr_info("%s: vcore_opp_2: %d uv (MD VB:%d)\n", __func__,
				vcore_opp_2_uv, get_vb_volt(VCORE_OPP_2));

	}

	pr_info("flavor check: %s, is_vcore_ct: %d, is_mini_sqc: %d\n",
			CONFIG_ARCH_MTK_PROJECT,
			is_vcore_ct, is_mini_sqc);
	pr_info("%s: FINAL vcore_opp_uv: %d, %d, %d, %d\n",
			__func__,
			vcore_opp_0_uv,
			vcore_opp_1_uv,
			vcore_opp_2_uv,
			vcore_opp_3_uv);

	set_vcore_uv_table(VCORE_OPP_0, vcore_opp_0_uv);
	set_vcore_uv_table(VCORE_OPP_1, vcore_opp_1_uv);

	if (info2 & 0x1) {
		set_vcore_uv_table(VCORE_OPP_2, vcore_opp_1_uv);
		set_vcore_uv_table(VCORE_OPP_3, vcore_opp_1_uv);
	} else {
		set_vcore_uv_table(VCORE_OPP_2, vcore_opp_2_uv);
		set_vcore_uv_table(VCORE_OPP_3, vcore_opp_3_uv);
	}

	switch (spm_get_spmfw_idx()) {
	case SPMFW_LP4_2CH_3200:
	case SPMFW_LP4_2CH_2400:
		/* fall through*/
	case SPMFW_LP4X_2CH_3200:
		set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_1, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_2, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_3, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_4, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_5, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_6, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_7, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_8, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_9, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_10, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_11, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_12, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_13, VCORE_OPP_2);
		set_vcore_opp(VCORE_DVFS_OPP_14, VCORE_OPP_2);
		set_vcore_opp(VCORE_DVFS_OPP_15, VCORE_OPP_3);

		set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_1, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_2, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_3, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_4, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_5, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_6, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_7, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_8, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_9, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_10, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_11, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_12, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_13, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_14, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_15, DDR_OPP_2);
		break;
	case SPMFW_LP3_1CH_1866:
		set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_1, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_2, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_3, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_4, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_5, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_6, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_7, VCORE_OPP_0);
		set_vcore_opp(VCORE_DVFS_OPP_8, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_9, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_10, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_11, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_12, VCORE_OPP_1);
		set_vcore_opp(VCORE_DVFS_OPP_13, VCORE_OPP_2);
		set_vcore_opp(VCORE_DVFS_OPP_14, VCORE_OPP_2);
		set_vcore_opp(VCORE_DVFS_OPP_15, VCORE_OPP_3);

		set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_1, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_2, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_3, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_4, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_5, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_6, DDR_OPP_0);
		set_ddr_opp(VCORE_DVFS_OPP_7, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_8, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_9, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_10, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_11, DDR_OPP_1);
		set_ddr_opp(VCORE_DVFS_OPP_12, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_13, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_14, DDR_OPP_2);
		set_ddr_opp(VCORE_DVFS_OPP_15, DDR_OPP_2);
		break;
	default:
		set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_1, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_2, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_3, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_4, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_5, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_6, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_7, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_8, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_9, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_10, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_11, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_12, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_13, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_14, VCORE_OPP_UNREQ);
		set_vcore_opp(VCORE_DVFS_OPP_15, VCORE_OPP_UNREQ);

		set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_1, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_2, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_3, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_4, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_5, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_6, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_7, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_8, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_9, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_10, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_11, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_12, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_13, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_14, DDR_OPP_UNREQ);
		set_ddr_opp(VCORE_DVFS_OPP_15, DDR_OPP_UNREQ);
		break;
	}
}

