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
#include <linux/of.h>
#include <linux/of_address.h>


static int get_vb_volt(int vcore_opp)
{
	int ret = 0;
	int ptpod64 = ((get_devinfo_with_index(64) >> 9) & 0x3);

	pr_info("%s: ptpod64: 0x%x\n", __func__, ptpod64);
	switch (vcore_opp) {
	case VCORE_OPP_0:
	case VCORE_OPP_1:
	case VCORE_OPP_3:
		break;
	case VCORE_OPP_2:
		if (ptpod64 != 0)
			ret = ptpod64 - 1;
		break;
	default:
		break;
	}
	return ret * 25000;
}

static int is_aging_test(void)
{
	int ret = 0;

#if defined(CONFIG_ARM64) && \
	defined(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)

	pr_info("[VcoreFS] flavor name: %s\n",
			CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);

	if ((strstr(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES,
				"k68v1_64_aging") != NULL)) {
		pr_info("[VcoreFS]: AGING flavor !!!\n");
		return 1;
	}
#endif

	return ret;
}


void dvfsrc_opp_level_mapping(void)
{
	int vcore_opp_0_uv, vcore_opp_1_uv, vcore_opp_2_uv, vcore_opp_3_uv;
	int is_vcore_ct = 0, is_mini_sqc = 0;
	int info2 = 0;
	struct device_node *dvfsrc_node = NULL;
	int dvfs_v_mode = 0;
	int is_vcore_aging = is_aging_test();

	if (!strncmp(CONFIG_ARCH_MTK_PROJECT,
				"k68v1_64_bsp_ctig", 17))
		is_vcore_ct = is_mini_sqc = 1;

	pr_info("flavor check: %s, is_vcore_ct: %d, is_mini_sqc: %d\n",
			CONFIG_ARCH_MTK_PROJECT,
			is_vcore_ct, is_mini_sqc);

	set_pwrap_cmd(VCORE_OPP_0, 0);
	set_pwrap_cmd(VCORE_OPP_1, 2);
	set_pwrap_cmd(VCORE_OPP_2, 3);
	set_pwrap_cmd(VCORE_OPP_3, 4);
	dvfsrc_node =
		of_find_compatible_node(NULL, NULL, "mediatek,dvfsrc");
	if (of_property_read_u32(dvfsrc_node, "dvfs_v_mode",
		(u32 *) &dvfs_v_mode) == 0)
		pr_info("%s: DOE DVFS_V_MODE = %d\n",
					__func__, dvfs_v_mode);

	vcore_opp_0_uv = 800000;
	vcore_opp_2_uv = 700000 - get_vb_volt(VCORE_OPP_2);
	vcore_opp_1_uv = 700000;
	vcore_opp_3_uv = 650000;

	if (dvfs_v_mode == 1) {  /* HV */
		vcore_opp_0_uv = 843750;
		vcore_opp_1_uv = 737500;
		vcore_opp_3_uv = 687500;
		/* apply MD VB */
		vcore_opp_2_uv = 737500;
	} else if (dvfs_v_mode == 3) {  /* LV */
		vcore_opp_0_uv = 756250;
		vcore_opp_1_uv = 662500;
		vcore_opp_3_uv = 612500;
		/* apply MD VB */
		vcore_opp_2_uv = 662500;
	} else if (is_vcore_aging == 1) {
		vcore_opp_0_uv -= 12500;
		vcore_opp_1_uv -= 12500;
		vcore_opp_2_uv -= 12500;
		vcore_opp_3_uv -= 12500;
	}

	pr_info("flavor check: %s, is_vcore_ct: %d, is_mini_sqc: %d dvfs_v_mode: %d\n",
			CONFIG_ARCH_MTK_PROJECT,
			is_vcore_ct, is_mini_sqc, dvfs_v_mode);
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
		/* fall through*/
	case SPMFW_LP4X_2CH_3600:
	case SPMFW_LP4_2CH_3200:
		set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_0);
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

		set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_0);
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
		set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_0);
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

		set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_0);
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

