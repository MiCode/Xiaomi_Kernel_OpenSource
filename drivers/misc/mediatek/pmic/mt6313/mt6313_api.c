/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "mt6313_upmu_hw.h"
#include "mt6313.h"

unsigned char mt6313_get_cid(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;

	ret = mt6313_read_interface((unsigned char)(MT6313_CID),
				    (&val),
				    (unsigned char)(MT6313_PMIC_CID_MASK),
				    (unsigned char)(MT6313_PMIC_CID_SHIFT)
	    );
	if (ret < 0) {
		pr_err("[mt6313_get_cid] ret=%d\n", ret);
		return ret;
	}

	return val;
}

unsigned char mt6313_get_swcid(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;

	ret = mt6313_read_interface((unsigned char)(MT6313_SWCID),
				    (&val),
				    (unsigned char)(MT6313_PMIC_SWCID_MASK),
				    (unsigned char)(MT6313_PMIC_SWCID_SHIFT)
	    );
	if (ret < 0) {
		pr_err("[mt6313_get_swcid] ret=%d\n", ret);
		return ret;
	}

	return val;
}

unsigned char mt6313_get_hwcid(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;

	ret = mt6313_read_interface((unsigned char)(MT6313_HWCID),
				    (&val),
				    (unsigned char)(MT6313_PMIC_HWCID_MASK),
				    (unsigned char)(MT6313_PMIC_HWCID_SHIFT)
	    );
	if (ret < 0) {
		pr_err("[mt6313_get_hwcid] ret=%d\n", ret);
		return ret;
	}

	return val;
}

void mt6313_set_gpio0_dir(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DIR),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO0_DIR_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO0_DIR_SHIFT)
	    );

}

void mt6313_set_gpio1_dir(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DIR),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO1_DIR_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO1_DIR_SHIFT)
	    );

}

void mt6313_set_gpio2_dir(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DIR),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO2_DIR_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO2_DIR_SHIFT)
	    );

}

void mt6313_set_gpio3_dir(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DIR),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO3_DIR_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO3_DIR_SHIFT)
	    );

}

void mt6313_set_gpio4_dir(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DIR),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO4_DIR_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO4_DIR_SHIFT)
	    );

}

void mt6313_set_gpio0_dinv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DINV),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO0_DINV_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO0_DINV_SHIFT)
	    );

}

void mt6313_set_gpio1_dinv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DINV),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO1_DINV_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO1_DINV_SHIFT)
	    );

}

void mt6313_set_gpio2_dinv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DINV),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO2_DINV_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO2_DINV_SHIFT)
	    );

}

void mt6313_set_gpio3_dinv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DINV),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO3_DINV_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO3_DINV_SHIFT)
	    );

}

void mt6313_set_gpio4_dinv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DINV),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO4_DINV_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO4_DINV_SHIFT)
	    );

}

void mt6313_set_gpio0_dout(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DOUT),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO0_DOUT_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO0_DOUT_SHIFT)
	    );

}

void mt6313_set_gpio1_dout(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DOUT),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO1_DOUT_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO1_DOUT_SHIFT)
	    );

}

void mt6313_set_gpio2_dout(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DOUT),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO2_DOUT_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO2_DOUT_SHIFT)
	    );

}

void mt6313_set_gpio3_dout(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DOUT),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO3_DOUT_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO3_DOUT_SHIFT)
	    );

}

void mt6313_set_gpio4_dout(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_DOUT),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO4_DOUT_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO4_DOUT_SHIFT)
	    );

}

unsigned char mt6313_get_gpio0_din(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_GPIO_DIN),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_GPIO0_DIN_MASK),
				    (unsigned char)(MT6313_PMIC_GPIO0_DIN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_gpio1_din(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_GPIO_DIN),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_GPIO1_DIN_MASK),
				    (unsigned char)(MT6313_PMIC_GPIO1_DIN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_gpio2_din(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_GPIO_DIN),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_GPIO2_DIN_MASK),
				    (unsigned char)(MT6313_PMIC_GPIO2_DIN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_gpio3_din(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_GPIO_DIN),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_GPIO3_DIN_MASK),
				    (unsigned char)(MT6313_PMIC_GPIO3_DIN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_gpio4_din(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_GPIO_DIN),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_GPIO4_DIN_MASK),
				    (unsigned char)(MT6313_PMIC_GPIO4_DIN_SHIFT)
	    );


	return val;
}

void mt6313_set_gpio0_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_MODE0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO0_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO0_MODE_SHIFT)
	    );

}

void mt6313_set_gpio1_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_MODE0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO1_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO1_MODE_SHIFT)
	    );

}

void mt6313_set_gpio2_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_MODE1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO2_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO2_MODE_SHIFT)
	    );

}

void mt6313_set_gpio3_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_MODE1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO3_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO3_MODE_SHIFT)
	    );

}

void mt6313_set_gpio4_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_GPIO_MODE2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_GPIO4_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_GPIO4_MODE_SHIFT)
	    );

}

unsigned char mt6313_get_test_out(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TEST_OUT),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_TEST_OUT_MASK),
				    (unsigned char)(MT6313_PMIC_TEST_OUT_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_mon_grp_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TEST_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_MON_GRP_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_MON_GRP_SEL_SHIFT)
	    );

}

void mt6313_set_rg_mon_flag_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TEST_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_MON_FLAG_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_MON_FLAG_SEL_SHIFT)
	    );

}

void mt6313_set_dig_testmode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TEST_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_DIG_TESTMODE_MASK),
				      (unsigned char)(MT6313_PMIC_DIG_TESTMODE_SHIFT)
	    );

}

void mt6313_set_pmu_testmode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TEST_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PMU_TESTMODE_MASK),
				      (unsigned char)(MT6313_PMIC_PMU_TESTMODE_SHIFT)
	    );

}

void mt6313_set_rg_srclken_in_hw_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SRCLKEN_IN_HW_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SRCLKEN_IN_HW_MODE_SHIFT)
	    );

}

void mt6313_set_rg_srclken_in_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SRCLKEN_IN_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SRCLKEN_IN_EN_SHIFT)
	    );

}

void mt6313_set_rg_buck_lp_hw_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_LP_HW_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_LP_HW_MODE_SHIFT)
	    );

}

void mt6313_set_rg_buck_lp_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_LP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_LP_EN_SHIFT)
	    );

}

void mt6313_set_rg_osc_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_OSC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_OSC_EN_SHIFT)
	    );

}

void mt6313_set_rg_osc_en_hw_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_OSC_EN_HW_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_OSC_EN_HW_MODE_SHIFT)
	    );

}

void mt6313_set_rg_srclken_in_sync_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SRCLKEN_IN_SYNC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SRCLKEN_IN_SYNC_EN_SHIFT)
	    );

}

void mt6313_set_rg_strup_rsv_hw_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_RSV_HW_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_RSV_HW_MODE_SHIFT)
	    );

}

void mt6313_set_rg_volten_in_hw_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VOLTEN_IN_HW_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VOLTEN_IN_HW_MODE_SHIFT)
	    );

}

void mt6313_set_rg_volten_in_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VOLTEN_IN_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VOLTEN_IN_EN_SHIFT)
	    );

}

void mt6313_set_rg_volten_in_sync_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VOLTEN_IN_SYNC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VOLTEN_IN_SYNC_EN_SHIFT)
	    );

}

void mt6313_set_rg_buck_ref_ck_tstsel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_REF_CK_TSTSEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_REF_CK_TSTSEL_SHIFT)
	    );

}

void mt6313_set_rg_fqmtr_ck_tstsel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_FQMTR_CK_TSTSEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_FQMTR_CK_TSTSEL_SHIFT)
	    );

}

void mt6313_set_rg_smps_ck_tstsel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SMPS_CK_TSTSEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SMPS_CK_TSTSEL_SHIFT)
	    );

}

void mt6313_set_rg_pmu75k_ck_tstsel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_PMU75K_CK_TSTSEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_PMU75K_CK_TSTSEL_SHIFT)
	    );

}

void mt6313_set_rg_smps_ck_tst_dis(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SMPS_CK_TST_DIS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SMPS_CK_TST_DIS_SHIFT)
	    );

}

void mt6313_set_rg_pmu75k_ck_tst_dis(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_PMU75K_CK_TST_DIS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_PMU75K_CK_TST_DIS_SHIFT)
	    );

}

void mt6313_set_rg_buck_ana_auto_off_dis(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKTST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_ANA_AUTO_OFF_DIS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_ANA_AUTO_OFF_DIS_SHIFT)
	    );

}

void mt6313_set_rg_buck_ref_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_REF_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_REF_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_buck_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_buck_1m_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_1M_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_1M_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_intrp_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_INTRP_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_INTRP_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_efuse_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_strup_75k_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_75K_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_75K_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_buck_ana_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_ANA_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_ANA_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_trim_75k_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_TRIM_75K_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_TRIM_75K_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_auxadc_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_AUXADC_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_AUXADC_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_auxadc_1m_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_AUXADC_1M_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_AUXADC_1M_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_stb_75k_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STB_75K_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STB_75K_CK_PDN_SHIFT)
	    );

}

void mt6313_set_rg_fqmtr_ck_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_FQMTR_CK_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_FQMTR_CK_PDN_SHIFT)
	    );

}

void mt6313_set_pad_scl_smt_level(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PAD_SCL_SMT_LEVEL_MASK),
				      (unsigned char)(MT6313_PMIC_PAD_SCL_SMT_LEVEL_SHIFT)
	    );

}

void mt6313_set_pad_scl_filter(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PAD_SCL_FILTER_MASK),
				      (unsigned char)(MT6313_PMIC_PAD_SCL_FILTER_SHIFT)
	    );

}

void mt6313_set_pad_smt_level(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PAD_SMT_LEVEL_MASK),
				      (unsigned char)(MT6313_PMIC_PAD_SMT_LEVEL_SHIFT)
	    );

}

void mt6313_set_pad_filter(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKPDN_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PAD_FILTER_MASK),
				      (unsigned char)(MT6313_PMIC_PAD_FILTER_SHIFT)
	    );

}

void mt6313_set_rg_buck_1m_ck_pdn_hwen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKHWEN_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_1M_CK_PDN_HWEN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BUCK_1M_CK_PDN_HWEN_SHIFT)
	    );

}

void mt6313_set_rg_efuse_ck_pdn_hwen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_CKHWEN_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_CK_PDN_HWEN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_CK_PDN_HWEN_SHIFT)
	    );

}

void mt6313_set_rg_auxadc_rst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_AUXADC_RST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_AUXADC_RST_SHIFT)
	    );

}

void mt6313_set_rg_fqmtr_rst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_FQMTR_RST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_FQMTR_RST_SHIFT)
	    );

}

void mt6313_set_rg_clk_trim_rst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_CLK_TRIM_RST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_CLK_TRIM_RST_SHIFT)
	    );

}

void mt6313_set_rg_efuse_man_rst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_MAN_RST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_MAN_RST_SHIFT)
	    );

}

void mt6313_set_rg_wdtrstb_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_WDTRSTB_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_WDTRSTB_MODE_SHIFT)
	    );

}

void mt6313_set_rg_wdtrstb_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_WDTRSTB_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_WDTRSTB_EN_SHIFT)
	    );

}

void mt6313_set_wdtrstb_status_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_RST_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_WDTRSTB_STATUS_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_WDTRSTB_STATUS_CLR_SHIFT)
	    );

}

unsigned char mt6313_get_wdtrstb_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_RST_CON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_WDTRSTB_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_WDTRSTB_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_int_pol(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_INT_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_INT_POL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_INT_POL_SHIFT)
	    );

}

void mt6313_set_rg_int_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_INT_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_INT_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_INT_EN_SHIFT)
	    );

}

void mt6313_set_i2c_config(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_INT_CON),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_I2C_CONFIG_MASK),
				      (unsigned char)(MT6313_PMIC_I2C_CONFIG_SHIFT)
	    );

}

unsigned char mt6313_get_rg_lbat_min_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_LBAT_MIN_INT_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_LBAT_MIN_INT_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_lbat_max_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_LBAT_MAX_INT_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_LBAT_MAX_INT_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_thr_l_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_THR_L_INT_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_THR_L_INT_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_thr_h_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_THR_H_INT_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_THR_H_INT_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_buck_oc_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_BUCK_OC_INT_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_BUCK_OC_INT_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_thr_h_110_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_THR_H_110_INT_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_THR_H_110_INT_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_da_sys_latch_en_tm(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_DA_SYS_LATCH_EN_TM_MASK),
				    (unsigned char)(MT6313_PMIC_DA_SYS_LATCH_EN_TM_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_da_sys_latch_en_tm_sel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_TOP_INT_MON),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_DA_SYS_LATCH_EN_TM_SEL_MASK),
				    (unsigned char)(MT6313_PMIC_DA_SYS_LATCH_EN_TM_SEL_SHIFT)
	    );


	return val;
}

void mt6313_set_testmode_set_key0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_ANATEST_MODE0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_TESTMODE_SET_KEY0_MASK),
				      (unsigned char)(MT6313_PMIC_TESTMODE_SET_KEY0_SHIFT)
	    );

}

void mt6313_set_testmode_set_key1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_TOP_ANATEST_MODE1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_TESTMODE_SET_KEY1_MASK),
				      (unsigned char)(MT6313_PMIC_TESTMODE_SET_KEY1_SHIFT)
	    );

}

void mt6313_set_rg_usbdl_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_USBDL_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_USBDL_EN_SHIFT)
	    );

}

void mt6313_set_rg_test_strup(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_TEST_STRUP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_TEST_STRUP_SHIFT)
	    );

}

void mt6313_set_rg_test_strup_thr_in(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_TEST_STRUP_THR_IN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_TEST_STRUP_THR_IN_SHIFT)
	    );

}

void mt6313_set_ni_vdvfs11_loading_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_NI_VDVFS11_LOADING_SW_MASK),
				      (unsigned char)(MT6313_PMIC_NI_VDVFS11_LOADING_SW_SHIFT)
	    );

}

void mt6313_set_ni_vdvfs11_loading_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_NI_VDVFS11_LOADING_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_NI_VDVFS11_LOADING_MODE_SHIFT)
	    );

}

void mt6313_set_thr_det_dis(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_THR_DET_DIS_MASK),
				      (unsigned char)(MT6313_PMIC_THR_DET_DIS_SHIFT)
	    );

}

void mt6313_set_thr_hwpdn_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_THR_HWPDN_EN_MASK),
				      (unsigned char)(MT6313_PMIC_THR_HWPDN_EN_SHIFT)
	    );

}

void mt6313_set_strup_dig0_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_DIG0_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_DIG0_RSV0_SHIFT)
	    );

}

void mt6313_set_thr_test(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_THR_TEST_MASK),
				      (unsigned char)(MT6313_PMIC_THR_TEST_SHIFT)
	    );

}

unsigned char mt6313_get_pmu_thr_deb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON1),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_PMU_THR_DEB_MASK),
				    (unsigned char)(MT6313_PMIC_PMU_THR_DEB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_pmu_thr_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON1),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_PMU_THR_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_PMU_THR_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_strup_pwron(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_PWRON_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_PWRON_SHIFT)
	    );

}

void mt6313_set_strup_pwron_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_PWRON_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_PWRON_SEL_SHIFT)
	    );

}

void mt6313_set_bias_gen_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BIAS_GEN_EN_MASK),
				      (unsigned char)(MT6313_PMIC_BIAS_GEN_EN_SHIFT)
	    );

}

void mt6313_set_bias_gen_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BIAS_GEN_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_BIAS_GEN_EN_SEL_SHIFT)
	    );

}

void mt6313_set_rtc_xosc32_enb_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RTC_XOSC32_ENB_SW_MASK),
				      (unsigned char)(MT6313_PMIC_RTC_XOSC32_ENB_SW_SHIFT)
	    );

}

void mt6313_set_rtc_xosc32_enb_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RTC_XOSC32_ENB_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RTC_XOSC32_ENB_SEL_SHIFT)
	    );

}

void mt6313_set_strup_dig_io_pg_force(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_DIG_IO_PG_FORCE_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT)
	    );

}

void mt6313_set_dduvlo_deb_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_DDUVLO_DEB_EN_MASK),
				      (unsigned char)(MT6313_PMIC_DDUVLO_DEB_EN_SHIFT)
	    );

}

void mt6313_set_pwrbb_deb_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PWRBB_DEB_EN_MASK),
				      (unsigned char)(MT6313_PMIC_PWRBB_DEB_EN_SHIFT)
	    );

}

void mt6313_set_strup_osc_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_OSC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_OSC_EN_SHIFT)
	    );

}

void mt6313_set_strup_osc_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_OSC_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_OSC_EN_SEL_SHIFT)
	    );

}

void mt6313_set_strup_ft_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_FT_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_FT_CTRL_SHIFT)
	    );

}

void mt6313_set_strup_pwron_force(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_PWRON_FORCE_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_PWRON_FORCE_SHIFT)
	    );

}

void mt6313_set_bias_gen_en_force(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BIAS_GEN_EN_FORCE_MASK),
				      (unsigned char)(MT6313_PMIC_BIAS_GEN_EN_FORCE_SHIFT)
	    );

}

void mt6313_set_vdvfs11_pg_h2l_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_PG_H2L_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_PG_H2L_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_pg_h2l_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_PG_H2L_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_PG_H2L_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_pg_h2l_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_PG_H2L_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_PG_H2L_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_pg_h2l_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_PG_H2L_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_PG_H2L_EN_SHIFT)
	    );

}

void mt6313_set_rg_strup_pmu_pwron_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PMU_PWRON_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PMU_PWRON_SEL_SHIFT)
	    );

}

void mt6313_set_rg_strup_pmu_pwron_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PMU_PWRON_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PMU_PWRON_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_pg_enb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_PG_ENB_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_PG_ENB_SHIFT)
	    );

}

void mt6313_set_vdvfs12_pg_enb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_PG_ENB_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_PG_ENB_SHIFT)
	    );

}

void mt6313_set_vdvfs13_pg_enb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_PG_ENB_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_PG_ENB_SHIFT)
	    );

}

void mt6313_set_vdvfs14_pg_enb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_PG_ENB_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_PG_ENB_SHIFT)
	    );

}

void mt6313_set_rg_pre_pwron_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_PRE_PWRON_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_PRE_PWRON_EN_SHIFT)
	    );

}

void mt6313_set_rg_pre_pwron_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_PRE_PWRON_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_PRE_PWRON_SWCTRL_SHIFT)
	    );

}

void mt6313_set_clr_just_rst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_CLR_JUST_RST_MASK),
				      (unsigned char)(MT6313_PMIC_CLR_JUST_RST_SHIFT)
	    );

}

void mt6313_set_uvlo_l2h_deb_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_UVLO_L2H_DEB_EN_MASK),
				      (unsigned char)(MT6313_PMIC_UVLO_L2H_DEB_EN_SHIFT)
	    );

}

void mt6313_set_rg_bgr_test_ckin_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TEST_CKIN_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TEST_CKIN_EN_SHIFT)
	    );

}

unsigned char mt6313_get_qi_osc_en(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON6),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_OSC_EN_MASK),
				    (unsigned char)(MT6313_PMIC_QI_OSC_EN_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_strup_pwron_reset_cnt(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_RESET_CNT_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_RESET_CNT_SHIFT)
	    );

}

void mt6313_set_rg_strup_pwron_rst_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_RST_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_RST_SWCTRL_SHIFT)
	    );

}

void mt6313_set_rg_strup_pwron_rst_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_RST_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_RST_EN_SHIFT)
	    );

}

void mt6313_set_strup_pwroff_preoff_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_PWROFF_PREOFF_EN_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_PWROFF_PREOFF_EN_SHIFT)
	    );

}

void mt6313_set_strup_pwroff_seq_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_PWROFF_SEQ_EN_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_PWROFF_SEQ_EN_SHIFT)
	    );

}

void mt6313_set_rg_sys_latch_en_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SYS_LATCH_EN_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SYS_LATCH_EN_SWCTRL_SHIFT)
	    );

}

void mt6313_set_rg_sys_latch_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SYS_LATCH_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SYS_LATCH_EN_SHIFT)
	    );

}

void mt6313_set_rg_onoff_en_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_ONOFF_EN_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_ONOFF_EN_SWCTRL_SHIFT)
	    );

}

void mt6313_set_rg_onoff_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_ONOFF_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_ONOFF_EN_SHIFT)
	    );

}

void mt6313_set_rg_strup_pwron_cond_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_COND_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_COND_SEL_SHIFT)
	    );

}

void mt6313_set_rg_strup_pwron_cond_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_COND_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_PWRON_COND_EN_SHIFT)
	    );

}

unsigned char mt6313_get_strup_pg_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_STRUP_PG_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_STRUP_PG_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_strup_pg_status_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_PG_STATUS_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_PG_STATUS_CLR_SHIFT)
	    );

}

void mt6313_set_rg_rsv_swreg(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_RSV_SWREG_MASK),
				      (unsigned char)(MT6313_PMIC_RG_RSV_SWREG_SHIFT)
	    );

}

unsigned char mt6313_get_vdvfs11_pg_deb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON11),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS11_PG_DEB_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS11_PG_DEB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs12_pg_deb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON11),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS12_PG_DEB_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS12_PG_DEB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs13_pg_deb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON11),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS13_PG_DEB_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS13_PG_DEB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs14_pg_deb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON11),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS14_PG_DEB_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS14_PG_DEB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_strup_ro_rsv0(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON11),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_STRUP_RO_RSV0_MASK),
				    (unsigned char)(MT6313_PMIC_STRUP_RO_RSV0_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_strup_thr_110_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_110_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_110_CLR_SHIFT)
	    );

}

void mt6313_set_rg_strup_thr_125_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_125_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_125_CLR_SHIFT)
	    );

}

void mt6313_set_rg_strup_thr_110_irq_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_110_IRQ_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_110_IRQ_EN_SHIFT)
	    );

}

void mt6313_set_rg_strup_thr_125_irq_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_125_IRQ_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_125_IRQ_EN_SHIFT)
	    );

}

unsigned char mt6313_get_rg_strup_thr_110_irq_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON12),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_STRUP_THR_110_IRQ_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_STRUP_THR_110_IRQ_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_strup_thr_125_irq_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON12),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_STRUP_THR_125_IRQ_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RG_STRUP_THR_125_IRQ_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_strup_con14_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_STRUP_CON14_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_STRUP_CON14_RSV0_SHIFT)
	    );

}

void mt6313_set_rg_strup_thr_over_110_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_OVER_110_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_OVER_110_CLR_SHIFT)
	    );

}

void mt6313_set_rg_strup_thr_over_110_irq_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON13),
				      (unsigned char)(val),
				      (unsigned
				       char)(MT6313_PMIC_RG_STRUP_THR_OVER_110_IRQ_EN_MASK),
				      (unsigned
				       char)(MT6313_PMIC_RG_STRUP_THR_OVER_110_IRQ_EN_SHIFT)
	    );

}

unsigned char mt6313_get_rg_strup_thr_over_110_irq_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON13),
				    (unsigned char *)(&val),
				    (unsigned
				     char)(MT6313_PMIC_RG_STRUP_THR_OVER_110_IRQ_STATUS_MASK),
				    (unsigned
				     char)(MT6313_PMIC_RG_STRUP_THR_OVER_110_IRQ_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_strup_con15_rsv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_CON15_RSV_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_CON15_RSV_SHIFT)
	    );

}

unsigned char mt6313_get_rg_strup_con16_ro(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_STRUP_CON14),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_STRUP_CON16_RO_MASK),
				    (unsigned char)(MT6313_PMIC_RG_STRUP_CON16_RO_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_thermal_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON15),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_THERMAL_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_THERMAL_EN_SHIFT)
	    );

}

void mt6313_set_rg_thermal_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON15),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_THERMAL_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_THERMAL_EN_SEL_SHIFT)
	    );

}

void mt6313_set_strup_vproc_off_td5_option_def(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON16),
				      (unsigned char)(val),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD5_OPTION_DEF_MASK),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD5_OPTION_DEF_SHIFT)
	    );

}

void mt6313_set_strup_vproc_off_td6_option_def(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON16),
				      (unsigned char)(val),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD6_OPTION_DEF_MASK),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD6_OPTION_DEF_SHIFT)
	    );

}

void mt6313_set_strup_vproc_off_td7_option_def(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON16),
				      (unsigned char)(val),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD7_OPTION_DEF_MASK),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD7_OPTION_DEF_SHIFT)
	    );

}

void mt6313_set_strup_vproc_off_td8_option_def(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_CON16),
				      (unsigned char)(val),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD8_OPTION_DEF_MASK),
				      (unsigned
				       char)(MT6313_PMIC_STRUP_VPROC_OFF_TD8_OPTION_DEF_SHIFT)
	    );

}

void mt6313_set_rg_efuse_addr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_ADDR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_ADDR_SHIFT)
	    );

}

void mt6313_set_rg_efuse_din(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DIN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DIN_SHIFT)
	    );

}

void mt6313_set_rg_efuse_dm(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DM_SHIFT)
	    );

}

void mt6313_set_rg_efuse_pgm(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_SHIFT)
	    );

}

void mt6313_set_rg_efuse_pgm_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_EN_SHIFT)
	    );

}

void mt6313_set_rg_efuse_prog_pkey(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PROG_PKEY_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PROG_PKEY_SHIFT)
	    );

}

void mt6313_set_rg_efuse_rd_pkey(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_PKEY_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_PKEY_SHIFT)
	    );

}

void mt6313_set_rg_efuse_pgm_src(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_SRC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_SRC_SHIFT)
	    );

}

void mt6313_set_rg_efuse_din_src(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DIN_SRC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DIN_SRC_SHIFT)
	    );

}

void mt6313_set_rg_efuse_rd_trig(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_TRIG_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_TRIG_SHIFT)
	    );

}

void mt6313_set_rg_rd_rdy_bypass(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_RD_RDY_BYPASS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_RD_RDY_BYPASS_SHIFT)
	    );

}

void mt6313_set_rg_skip_efuse_out(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_SKIP_EFUSE_OUT_MASK),
				      (unsigned char)(MT6313_PMIC_RG_SKIP_EFUSE_OUT_SHIFT)
	    );

}

void mt6313_set_rg_efuse_pgm_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_PGM_SEL_SHIFT)
	    );

}

unsigned char mt6313_get_rg_efuse_rd_ack(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_EFUSE_CON5),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_ACK_MASK),
				    (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_ACK_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rg_efuse_rd_busy(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_EFUSE_CON5),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_BUSY_MASK),
				    (unsigned char)(MT6313_PMIC_RG_EFUSE_RD_BUSY_SHIFT)
	    );


	return val;
}

void mt6313_set_rg_efuse_data_mon_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DATA_MON_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_DATA_MON_SEL_SHIFT)
	    );

}

void mt6313_set_rg_efuse_write_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_WRITE_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_WRITE_MODE_SHIFT)
	    );

}

void mt6313_set_rg_efuse_macro_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_MACRO_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_MACRO_SEL_SHIFT)
	    );

}

void mt6313_set_rg_efuse_macro_sel_w(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_MACRO_SEL_W_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_MACRO_SEL_W_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_0_7(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_0_7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_0_7_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_0_7_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_8_15(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_8_15),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_8_15_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_8_15_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_16_23(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_16_23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_16_23_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_16_23_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_24_31(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_24_31),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_24_31_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_24_31_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_32_39(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_32_39),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_32_39_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_32_39_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_40_47(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_40_47),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_40_47_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_40_47_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_48_55(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_48_55),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_48_55_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_48_55_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_56_63(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_56_63),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_56_63_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_56_63_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_64_71(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_64_71),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_64_71_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_64_71_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_72_79(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_72_79),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_72_79_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_72_79_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_80_87(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_80_87),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_80_87_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_80_87_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_88_95(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_88_95),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_88_95_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_88_95_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_96_103(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_96_103),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_96_103_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_96_103_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_104_111(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_104_111),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_104_111_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_104_111_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_112_119(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_112_119),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_112_119_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_112_119_SHIFT)
	    );

}

void mt6313_set_rg_efuse_val_120_127(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_EFUSE_VAL_120_127),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_120_127_MASK),
				      (unsigned char)(MT6313_PMIC_RG_EFUSE_VAL_120_127_SHIFT)
	    );

}

void mt6313_set_buck_dig0_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BUCK_DIG0_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_BUCK_DIG0_RSV0_SHIFT)
	    );

}

void mt6313_set_vsleep_src0_8(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VSLEEP_SRC0_8_MASK),
				      (unsigned char)(MT6313_PMIC_VSLEEP_SRC0_8_SHIFT)
	    );

}

void mt6313_set_vsleep_src1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VSLEEP_SRC1_MASK),
				      (unsigned char)(MT6313_PMIC_VSLEEP_SRC1_SHIFT)
	    );

}

void mt6313_set_vsleep_src0_7_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VSLEEP_SRC0_7_0_MASK),
				      (unsigned char)(MT6313_PMIC_VSLEEP_SRC0_7_0_SHIFT)
	    );

}

void mt6313_set_r2r_src0_8(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_R2R_SRC0_8_MASK),
				      (unsigned char)(MT6313_PMIC_R2R_SRC0_8_SHIFT)
	    );

}

void mt6313_set_r2r_src1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_R2R_SRC1_MASK),
				      (unsigned char)(MT6313_PMIC_R2R_SRC1_SHIFT)
	    );

}

void mt6313_set_r2r_src0_7_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_R2R_SRC0_7_0_MASK),
				      (unsigned char)(MT6313_PMIC_R2R_SRC0_7_0_SHIFT)
	    );

}

void mt6313_set_buck_osc_sel_src0_8(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BUCK_OSC_SEL_SRC0_8_MASK),
				      (unsigned char)(MT6313_PMIC_BUCK_OSC_SEL_SRC0_8_SHIFT)
	    );

}

void mt6313_set_srclken_dly_src1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_SRCLKEN_DLY_SRC1_MASK),
				      (unsigned char)(MT6313_PMIC_SRCLKEN_DLY_SRC1_SHIFT)
	    );

}

void mt6313_set_buck_osc_sel_src0_7_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BUCK_OSC_SEL_SRC0_7_0_MASK),
				      (unsigned char)(MT6313_PMIC_BUCK_OSC_SEL_SRC0_7_0_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_deg_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_DEG_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_DEG_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_wnd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_WND_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_WND_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_thd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_THD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_THD_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_deg_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_DEG_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_DEG_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_wnd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_WND_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_WND_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_thd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_THD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_THD_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_deg_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_DEG_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_DEG_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_wnd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_WND_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_WND_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_thd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_THD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_THD_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_deg_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_DEG_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_DEG_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_wnd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_WND_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_WND_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_thd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_THD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_THD_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_flag_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_FLAG_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_FLAG_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_flag_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_FLAG_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_FLAG_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_rg_status_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_RG_STATUS_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_RG_STATUS_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_rg_status_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_RG_STATUS_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_RG_STATUS_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_flag_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_FLAG_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_FLAG_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_flag_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_FLAG_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_FLAG_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_rg_status_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_RG_STATUS_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_RG_STATUS_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_rg_status_clr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_RG_STATUS_CLR_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_RG_STATUS_CLR_SHIFT)
	    );

}

void mt6313_set_vdvfs11_oc_flag_clr_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_FLAG_CLR_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_FLAG_CLR_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_flag_clr_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_FLAG_CLR_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_FLAG_CLR_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_flag_clr_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_FLAG_CLR_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_FLAG_CLR_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_flag_clr_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_FLAG_CLR_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_FLAG_CLR_SEL_SHIFT)
	    );

}

unsigned char mt6313_get_rgs_en_2a_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON19),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RGS_EN_2A_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RGS_EN_2A_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rgs_en_4a_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON19),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RGS_EN_4A_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RGS_EN_4A_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs11_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS11_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS11_OC_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs12_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS12_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS12_OC_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs13_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS13_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS13_OC_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs14_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS14_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS14_OC_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rgs_vdvfs11_enpwm_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS11_ENPWM_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS11_ENPWM_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rgs_vdvfs12_enpwm_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS12_ENPWM_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS12_ENPWM_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rgs_vdvfs13_enpwm_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS13_ENPWM_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS13_ENPWM_STATUS_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_rgs_vdvfs14_enpwm_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS14_ENPWM_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_RGS_VDVFS14_ENPWM_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs11_oc_int_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_INT_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_OC_INT_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_oc_int_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_INT_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_OC_INT_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_oc_int_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_INT_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_OC_INT_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_oc_int_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_INT_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_OC_INT_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_en_oc_sdn_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_OC_SDN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_OC_SDN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_en_oc_sdn_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_OC_SDN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_OC_SDN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_en_oc_sdn_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_OC_SDN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_OC_SDN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_en_oc_sdn_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_OC_SDN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_OC_SDN_SEL_SHIFT)
	    );

}

void mt6313_set_qi_vdvfs13_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS13_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS13_VSLEEP_SHIFT)
	    );

}

void mt6313_set_qi_vdvfs14_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS14_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS14_VSLEEP_SHIFT)
	    );

}

void mt6313_set_buck_dig1_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BUCK_DIG1_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_BUCK_DIG1_RSV0_SHIFT)
	    );

}

void mt6313_set_buck_test_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BUCK_TEST_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_BUCK_TEST_MODE_SHIFT)
	    );

}

void mt6313_set_qi_vdvfs11_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON24),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS11_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS11_VSLEEP_SHIFT)
	    );

}

void mt6313_set_qi_vdvfs12_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_ALL_CON24),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS12_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_QI_VDVFS12_VSLEEP_SHIFT)
	    );

}

unsigned char mt6313_get_qi_vdvfs11_dig_mon(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON26),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_DIG_MON_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_DIG_MON_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs12_dig_mon(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON27),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_DIG_MON_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_DIG_MON_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs13_dig_mon(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON28),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_DIG_MON_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_DIG_MON_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs14_dig_mon(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_ALL_CON29),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_DIG_MON_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_DIG_MON_SHIFT)
	    );


	return val;
}

void mt6313_set_buck_ana_dig0_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_ANA_RSV_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_BUCK_ANA_DIG0_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_BUCK_ANA_DIG0_RSV0_SHIFT)
	    );

}

void mt6313_set_rg_thrdet_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_THRDET_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_THRDET_SEL_SHIFT)
	    );

}

void mt6313_set_rg_strup_thr_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_THR_SEL_SHIFT)
	    );

}

void mt6313_set_rg_thr_tmode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_THR_TMODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_THR_TMODE_SHIFT)
	    );

}

void mt6313_set_rg_strup_iref_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_IREF_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_IREF_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_uvlo_vthl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_UVLO_VTHL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_UVLO_VTHL_SHIFT)
	    );

}

void mt6313_set_rg_uvlo_vthh(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_UVLO_VTHH_MASK),
				      (unsigned char)(MT6313_PMIC_RG_UVLO_VTHH_SHIFT)
	    );

}

void mt6313_set_rg_bgr_unchop(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_UNCHOP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_UNCHOP_SHIFT)
	    );

}

void mt6313_set_rg_bgr_unchop_ph(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_UNCHOP_PH_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_UNCHOP_PH_SHIFT)
	    );

}

void mt6313_set_rg_bgr_rsel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_RSEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_RSEL_SHIFT)
	    );

}

void mt6313_set_rg_bgr_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_bgr_test_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TEST_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TEST_EN_SHIFT)
	    );

}

void mt6313_set_rg_bgr_test_rstb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TEST_RSTB_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_TEST_RSTB_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_trimh(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TRIMH_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TRIMH_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_triml(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TRIML_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TRIML_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_trimh(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_TRIMH_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_TRIMH_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_triml(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_TRIML_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_TRIML_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_trimh(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_TRIMH_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_TRIMH_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_triml(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_TRIML_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_TRIML_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_trimh(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_TRIMH_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_TRIMH_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_triml(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_TRIML_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_TRIML_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_VSLEEP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_VSLEEP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_VSLEEP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_vsleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_VSLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_VSLEEP_SHIFT)
	    );

}

void mt6313_set_rg_bgr_osc_cal(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_BGR_OSC_CAL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_BGR_OSC_CAL_SHIFT)
	    );

}

void mt6313_set_rg_strup_rsv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON15),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_RSV_MASK),
				      (unsigned char)(MT6313_PMIC_RG_STRUP_RSV_SHIFT)
	    );

}

void mt6313_set_rg_vref_lp_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON16),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VREF_LP_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VREF_LP_MODE_SHIFT)
	    );

}

void mt6313_set_rg_testmode_swen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON16),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_TESTMODE_SWEN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_TESTMODE_SWEN_SHIFT)
	    );

}

void mt6313_set_rg_vdig18_vosel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON16),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDIG18_VOSEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDIG18_VOSEL_SHIFT)
	    );

}

void mt6313_set_rg_vdig18_cal(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDIG18_CAL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDIG18_CAL_SHIFT)
	    );

}

void mt6313_set_rg_osc_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_STRUP_ANA_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_OSC_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_OSC_SEL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_rc(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_RC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_RC_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_rc(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_RC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_RC_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_rc(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_RC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_RC_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_rc(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_RC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_RC_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_pfm_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PFM_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PFM_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_pfm_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_PFM_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_PFM_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_pfm_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_PFM_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_PFM_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_pfm_csr(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_PFM_CSR_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_PFM_CSR_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_slp(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_SLP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_SLP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_slp(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_SLP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_SLP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_slp(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_SLP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_SLP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_slp(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_SLP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_SLP_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_uvp_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_UVP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_UVP_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_uvp_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_UVP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_UVP_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_uvp_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_UVP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_UVP_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_uvp_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_UVP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_UVP_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_modeset(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_MODESET_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_MODESET_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_modeset(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_MODESET_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_MODESET_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_modeset(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_MODESET_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_MODESET_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_modeset(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_MODESET_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_MODESET_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_ndis_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_NDIS_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_NDIS_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_ndis_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_NDIS_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_NDIS_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_ndis_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_NDIS_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_NDIS_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_ndis_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_NDIS_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_NDIS_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_trans_bst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TRANS_BST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TRANS_BST_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_trans_bst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_TRANS_BST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_TRANS_BST_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_trans_bst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_TRANS_BST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_TRANS_BST_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_trans_bst(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_TRANS_BST_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_TRANS_BST_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_csm_n(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_CSM_N_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_CSM_N_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_csm_p(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_CSM_P_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_CSM_P_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_csm_n(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_CSM_N_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_CSM_N_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_csm_p(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_CSM_P_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_CSM_P_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_csm_n(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_CSM_N_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_CSM_N_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_csm_p(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_CSM_P_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_CSM_P_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_csm_n(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_CSM_N_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_CSM_N_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_csm_p(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_CSM_P_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_CSM_P_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_zxos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON15),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_ZXOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_ZXOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_zxos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON16),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_ZXOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_ZXOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_zxos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_ZXOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_ZXOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_oc_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_OC_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_OC_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_oc_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_OC_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_OC_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_oc_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_OC_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_OC_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_oc_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_OC_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_OC_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_phs_shed_2a_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PHS_SHED_2A_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PHS_SHED_2A_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_phs_shed_3a_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PHS_SHED_3A_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PHS_SHED_3A_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_phs_shed_4a_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PHS_SHED_4A_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PHS_SHED_4A_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_f2phs(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_F2PHS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_F2PHS_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_f3phs(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_F3PHS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_F3PHS_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_f4phs(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_F4PHS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_F4PHS_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_rs_force_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_RS_FORCE_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_RS_FORCE_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_rs_force_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_RS_FORCE_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_RS_FORCE_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_rs_force_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_RS_FORCE_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_RS_FORCE_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_rs_force_off(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_RS_FORCE_OFF_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_RS_FORCE_OFF_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_tm_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TM_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TM_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_tm_ugsns(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TM_UGSNS_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_TM_UGSNS_SHIFT)
	    );

}

void mt6313_set_rg_platform_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_PLATFORM_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_PLATFORM_SEL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_ics_cal(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_ICS_CAL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_ICS_CAL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_ics_cal(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_ICS_CAL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_ICS_CAL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_ics_cal(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_ICS_CAL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_ICS_CAL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_ics_cal(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON24),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_ICS_CAL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_ICS_CAL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_nlimos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON25),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_NLIMOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_NLIMOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_nlimos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_NLIMOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_NLIMOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_nlimos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON27),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_NLIMOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_NLIMOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_nlimos_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON28),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_NLIMOS_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_NLIMOS_TRIM_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_pfmvhvl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON29),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PFMVHVL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_PFMVHVL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_pfmvhvl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON29),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_PFMVHVL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_PFMVHVL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_pfmvhvl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON30),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_PFMVHVL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_PFMVHVL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_pfmvhvl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON30),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_PFMVHVL_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_PFMVHVL_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs11_load_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON31),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_LOAD_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS11_LOAD_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs12_load_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON31),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_LOAD_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS12_LOAD_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs13_load_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON31),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_LOAD_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS13_LOAD_EN_SHIFT)
	    );

}

void mt6313_set_rg_vdvfs14_load_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS1_ANA_CON31),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_LOAD_EN_MASK),
				      (unsigned char)(MT6313_PMIC_RG_VDVFS14_LOAD_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_dig0_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_DIG0_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_DIG0_RSV0_SHIFT)
	    );

}

void mt6313_set_vdvfs11_en_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel_dvfs_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_DVFS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_DVFS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_stbtd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_STBTD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_STBTD_SHIFT)
	    );

}

unsigned char mt6313_get_qi_vdvfs11_stb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_STB_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_STB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs11_en(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_EN_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_EN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs11_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS11_OC_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs11_sfchg_rrate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_RRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_RRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs11_sfchg_ren(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_REN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_REN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_sfchg_frate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_FRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_FRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs11_sfchg_fen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_FEN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_SFCHG_FEN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel_on(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_ON_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_ON_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel_sleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_SLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_SLEEP_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs11_vosel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON15),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_VOSEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_VOSEL_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs11_ro_rsv(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON16),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS11_RO_RSV_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS11_RO_RSV_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs11_con17_rsv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_CON17_RSV_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_CON17_RSV_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vsleep_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VSLEEP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VSLEEP_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_r2r_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_R2R_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_R2R_PDN_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vsleep_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VSLEEP_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VSLEEP_SEL_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs11_r2r_pdn(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_R2R_PDN_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_R2R_PDN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_ni_vdvfs11_vsleep_sel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_VSLEEP_SEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_VSLEEP_SEL_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs11_trans_td(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_TRANS_TD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_TRANS_TD_SHIFT)
	    );

}

void mt6313_set_vdvfs11_trans_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_TRANS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_TRANS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs11_trans_once(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_TRANS_ONCE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_TRANS_ONCE_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs11_vosel_trans(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS11_CON19),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_VOSEL_TRANS_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS11_VOSEL_TRANS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs11_vosel_dvfs0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_DVFS0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_DVFS0_SHIFT)
	    );

}

void mt6313_set_vdvfs11_vosel_dvfs1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS11_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_DVFS1_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS11_VOSEL_DVFS1_SHIFT)
	    );

}

void mt6313_set_vdvfs12_dig0_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_DIG0_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_DIG0_RSV0_SHIFT)
	    );

}

void mt6313_set_vdvfs12_en_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel_dvfs_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_DVFS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_DVFS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_stbtd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_STBTD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_STBTD_SHIFT)
	    );

}

unsigned char mt6313_get_qi_vdvfs12_stb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_STB_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_STB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs12_en(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_EN_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_EN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs12_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS12_OC_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs12_sfchg_rrate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_RRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_RRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs12_sfchg_ren(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_REN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_REN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_sfchg_frate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_FRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_FRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs12_sfchg_fen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_FEN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_SFCHG_FEN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel_on(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_ON_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_ON_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel_sleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_SLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_SLEEP_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs12_vosel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON15),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_VOSEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_VOSEL_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs12_ro_rsv(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON16),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS12_RO_RSV_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS12_RO_RSV_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs12_con17_rsv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_CON17_RSV_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_CON17_RSV_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vsleep_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VSLEEP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VSLEEP_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_r2r_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_R2R_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_R2R_PDN_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vsleep_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VSLEEP_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VSLEEP_SEL_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs12_r2r_pdn(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_R2R_PDN_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_R2R_PDN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_ni_vdvfs12_vsleep_sel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_VSLEEP_SEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_VSLEEP_SEL_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs12_trans_td(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_TRANS_TD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_TRANS_TD_SHIFT)
	    );

}

void mt6313_set_vdvfs12_trans_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_TRANS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_TRANS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs12_trans_once(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_TRANS_ONCE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_TRANS_ONCE_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs12_vosel_trans(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS12_CON19),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_VOSEL_TRANS_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS12_VOSEL_TRANS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs12_vosel_dvfs0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_DVFS0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_DVFS0_SHIFT)
	    );

}

void mt6313_set_vdvfs12_vosel_dvfs1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS12_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_DVFS1_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS12_VOSEL_DVFS1_SHIFT)
	    );

}

void mt6313_set_vdvfs13_dig0_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_DIG0_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_DIG0_RSV0_SHIFT)
	    );

}

void mt6313_set_vdvfs13_en_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel_dvfs_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_DVFS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_DVFS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_stbtd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_STBTD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_STBTD_SHIFT)
	    );

}

unsigned char mt6313_get_qi_vdvfs13_stb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_STB_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_STB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs13_en(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_EN_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_EN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs13_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS13_OC_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs13_sfchg_rrate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_RRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_RRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs13_sfchg_ren(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_REN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_REN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_sfchg_frate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_FRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_FRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs13_sfchg_fen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_FEN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_SFCHG_FEN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel_on(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_ON_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_ON_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel_sleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_SLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_SLEEP_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs13_vosel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON15),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_VOSEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_VOSEL_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs13_ro_rsv(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON16),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS13_RO_RSV_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS13_RO_RSV_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs13_con17_rsv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_CON17_RSV_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_CON17_RSV_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vsleep_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VSLEEP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VSLEEP_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_r2r_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_R2R_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_R2R_PDN_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vsleep_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VSLEEP_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VSLEEP_SEL_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs13_r2r_pdn(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_R2R_PDN_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_R2R_PDN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_ni_vdvfs13_vsleep_sel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_VSLEEP_SEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_VSLEEP_SEL_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs13_trans_td(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_TRANS_TD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_TRANS_TD_SHIFT)
	    );

}

void mt6313_set_vdvfs13_trans_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_TRANS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_TRANS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs13_trans_once(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_TRANS_ONCE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_TRANS_ONCE_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs13_vosel_trans(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS13_CON19),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_VOSEL_TRANS_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS13_VOSEL_TRANS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs13_vosel_dvfs0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_DVFS0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_DVFS0_SHIFT)
	    );

}

void mt6313_set_vdvfs13_vosel_dvfs1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS13_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_DVFS1_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS13_VOSEL_DVFS1_SHIFT)
	    );

}

void mt6313_set_pwron_key(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_PWRON_KEY_MASK),
				      (unsigned char)(MT6313_PMIC_PWRON_KEY_SHIFT)
	    );

}

void mt6313_set_vdvfs14_en_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel_dvfs_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_DVFS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_DVFS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_en_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_SEL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_stbtd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_STBTD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_STBTD_SHIFT)
	    );

}

unsigned char mt6313_get_qi_vdvfs14_stb(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_STB_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_STB_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs14_en(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_EN_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_EN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_qi_vdvfs14_oc_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_OC_STATUS_MASK),
				    (unsigned char)(MT6313_PMIC_QI_VDVFS14_OC_STATUS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs14_sfchg_rrate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_RRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_RRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs14_sfchg_ren(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_REN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_REN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_sfchg_frate(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_FRATE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_FRATE_SHIFT)
	    );

}

void mt6313_set_vdvfs14_sfchg_fen(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_FEN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_SFCHG_FEN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel_on(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_ON_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_ON_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel_sleep(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_SLEEP_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_SLEEP_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs14_vosel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON15),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_VOSEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_VOSEL_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_vdvfs14_ro_rsv(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON16),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_VDVFS14_RO_RSV_MASK),
				    (unsigned char)(MT6313_PMIC_VDVFS14_RO_RSV_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs14_con17_rsv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_CON17_RSV_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_CON17_RSV_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vsleep_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VSLEEP_EN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VSLEEP_EN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_r2r_pdn(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_R2R_PDN_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_R2R_PDN_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vsleep_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VSLEEP_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VSLEEP_SEL_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs14_r2r_pdn(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_R2R_PDN_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_R2R_PDN_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_ni_vdvfs14_vsleep_sel(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON18),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_VSLEEP_SEL_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_VSLEEP_SEL_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs14_trans_td(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_TRANS_TD_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_TRANS_TD_SHIFT)
	    );

}

void mt6313_set_vdvfs14_trans_ctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_TRANS_CTRL_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_TRANS_CTRL_SHIFT)
	    );

}

void mt6313_set_vdvfs14_trans_once(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_TRANS_ONCE_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_TRANS_ONCE_SHIFT)
	    );

}

unsigned char mt6313_get_ni_vdvfs14_vosel_trans(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_VDVFS14_CON19),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_VOSEL_TRANS_MASK),
				    (unsigned char)(MT6313_PMIC_NI_VDVFS14_VOSEL_TRANS_SHIFT)
	    );


	return val;
}

void mt6313_set_vdvfs14_vosel_dvfs0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_DVFS0_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_DVFS0_SHIFT)
	    );

}

void mt6313_set_vdvfs14_vosel_dvfs1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_VDVFS14_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_DVFS1_MASK),
				      (unsigned char)(MT6313_PMIC_VDVFS14_VOSEL_DVFS1_SHIFT)
	    );

}

void mt6313_set_k_rst_done(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_RST_DONE_MASK),
				      (unsigned char)(MT6313_PMIC_K_RST_DONE_SHIFT)
	    );

}

void mt6313_set_k_map_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_MAP_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_K_MAP_SEL_SHIFT)
	    );

}

void mt6313_set_k_once_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_ONCE_EN_MASK),
				      (unsigned char)(MT6313_PMIC_K_ONCE_EN_SHIFT)
	    );

}

void mt6313_set_k_once(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_ONCE_MASK),
				      (unsigned char)(MT6313_PMIC_K_ONCE_SHIFT)
	    );

}

void mt6313_set_k_start_manual(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_START_MANUAL_MASK),
				      (unsigned char)(MT6313_PMIC_K_START_MANUAL_SHIFT)
	    );

}

void mt6313_set_k_src_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_SRC_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_K_SRC_SEL_SHIFT)
	    );

}

void mt6313_set_k_auto_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_AUTO_EN_MASK),
				      (unsigned char)(MT6313_PMIC_K_AUTO_EN_SHIFT)
	    );

}

void mt6313_set_k_inv(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_INV_MASK),
				      (unsigned char)(MT6313_PMIC_K_INV_SHIFT)
	    );

}

void mt6313_set_k_control_smps(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_CONTROL_SMPS_MASK),
				      (unsigned char)(MT6313_PMIC_K_CONTROL_SMPS_SHIFT)
	    );

}

unsigned char mt6313_get_qi_smps_osc_cal(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_K_CON2),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_QI_SMPS_OSC_CAL_MASK),
				    (unsigned char)(MT6313_PMIC_QI_SMPS_OSC_CAL_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_k_result(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_K_CON3),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_K_RESULT_MASK),
				    (unsigned char)(MT6313_PMIC_K_RESULT_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_k_done(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_K_CON3),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_K_DONE_MASK),
				    (unsigned char)(MT6313_PMIC_K_DONE_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_k_control(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_BUCK_K_CON3),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_K_CONTROL_MASK),
				    (unsigned char)(MT6313_PMIC_K_CONTROL_SHIFT)
	    );


	return val;
}

void mt6313_set_k_buck_ck_cnt_8(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_BUCK_CK_CNT_8_MASK),
				      (unsigned char)(MT6313_PMIC_K_BUCK_CK_CNT_8_SHIFT)
	    );

}

void mt6313_set_k_buck_ck_cnt_7_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_BUCK_K_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_K_BUCK_CK_CNT_7_0_MASK),
				      (unsigned char)(MT6313_PMIC_K_BUCK_CK_CNT_7_0_SHIFT)
	    );

}

unsigned char mt6313_get_auxadc_adc_out_ch0(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC0),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH0_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH0_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_rdy_ch0(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC0),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH0_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH0_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_out_ch1(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC1),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH1_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH1_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_rdy_ch1(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC1),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH1_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH1_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_out_ch2(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC2),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH2_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH2_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_rdy_ch2(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC2),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH2_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH2_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_out_ch3(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC3),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH3_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CH3_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_rdy_ch3(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC3),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH3_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CH3_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_out_csm(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC4),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CSM_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_OUT_CSM_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_rdy_csm(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_ADC4),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CSM_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_RDY_CSM_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_adc_busy_in(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_STA0),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_BUSY_IN_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_ADC_BUSY_IN_SHIFT)
	    );


	return val;
}

void mt6313_set_auxadc_rqst_ch0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_RQST0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH0_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH0_SHIFT)
	    );

}

void mt6313_set_auxadc_rqst_ch1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_RQST0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH1_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH1_SHIFT)
	    );

}

void mt6313_set_auxadc_rqst_ch2(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_RQST0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH2_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH2_SHIFT)
	    );

}

void mt6313_set_auxadc_rqst_ch3(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_RQST0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH3_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_RQST_CH3_SHIFT)
	    );

}

void mt6313_set_auxadc_en_csm_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_EN_CSM_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_EN_CSM_SW_SHIFT)
	    );

}

void mt6313_set_auxadc_en_csm_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_EN_CSM_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_EN_CSM_SEL_SHIFT)
	    );

}

void mt6313_set_rg_test_auxadc(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_TEST_AUXADC_MASK),
				      (unsigned char)(MT6313_PMIC_RG_TEST_AUXADC_SHIFT)
	    );

}

void mt6313_set_auxadc_ck_aon_gps(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_AON_GPS_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_AON_GPS_SHIFT)
	    );

}

void mt6313_set_auxadc_ck_aon_md(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_AON_MD_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_AON_MD_SHIFT)
	    );

}

void mt6313_set_auxadc_ck_aon(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_AON_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_AON_SHIFT)
	    );

}

void mt6313_set_auxadc_ck_on_extd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_ON_EXTD_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_CK_ON_EXTD_SHIFT)
	    );

}

void mt6313_set_auxadc_spl_num(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_SPL_NUM_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_SPL_NUM_SHIFT)
	    );

}

void mt6313_set_auxadc_avg_num_small(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_AVG_NUM_SMALL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_AVG_NUM_SMALL_SHIFT)
	    );

}

void mt6313_set_auxadc_avg_num_large(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON3),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_AVG_NUM_LARGE_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_AVG_NUM_LARGE_SHIFT)
	    );

}

void mt6313_set_auxadc_avg_num_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON4),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_AVG_NUM_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_AVG_NUM_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_con5_rsv0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_CON5_RSV0_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_CON5_RSV0_SHIFT)
	    );

}

void mt6313_set_rg_adc_2s_comp_enb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_ADC_2S_COMP_ENB_MASK),
				      (unsigned char)(MT6313_PMIC_RG_ADC_2S_COMP_ENB_SHIFT)
	    );

}

void mt6313_set_rg_adc_trim_comp(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_RG_ADC_TRIM_COMP_MASK),
				      (unsigned char)(MT6313_PMIC_RG_ADC_TRIM_COMP_SHIFT)
	    );

}

void mt6313_set_auxadc_out_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_OUT_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_OUT_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_adc_pwdb_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ADC_PWDB_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ADC_PWDB_SWCTRL_SHIFT)
	    );

}

void mt6313_set_auxadc_qi_vdvfs1_csm_en_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS1_CSM_EN_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS1_CSM_EN_SW_SHIFT)
	    );

}

void mt6313_set_auxadc_qi_vdvfs11_csm_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS11_CSM_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS11_CSM_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_qi_vdvfs12_csm_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON5),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS12_CSM_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS12_CSM_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_sw_gain_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON6),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_SW_GAIN_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_SW_GAIN_TRIM_SHIFT)
	    );

}

void mt6313_set_auxadc_sw_offset_trim(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON7),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_SW_OFFSET_TRIM_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_SW_OFFSET_TRIM_SHIFT)
	    );

}

void mt6313_set_auxadc_rng_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_RNG_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_RNG_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_data_reuse_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DATA_REUSE_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DATA_REUSE_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_test_mode(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_TEST_MODE_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_TEST_MODE_SHIFT)
	    );

}

void mt6313_set_auxadc_bit_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_BIT_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_BIT_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_start_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_START_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_START_SW_SHIFT)
	    );

}

void mt6313_set_auxadc_start_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_START_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_START_SWCTRL_SHIFT)
	    );

}

void mt6313_set_auxadc_adc_pwdb(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON8),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ADC_PWDB_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ADC_PWDB_SHIFT)
	    );

}

unsigned char mt6313_get_ad_auxadc_comp(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON9),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AD_AUXADC_COMP_MASK),
				    (unsigned char)(MT6313_PMIC_AD_AUXADC_COMP_SHIFT)
	    );


	return val;
}

void mt6313_set_auxadc_da_dac_swctrl(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON9),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DA_DAC_SWCTRL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DA_DAC_SWCTRL_SHIFT)
	    );

}

void mt6313_set_auxadc_da_dac(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON10),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DA_DAC_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DA_DAC_SHIFT)
	    );

}

void mt6313_set_auxadc_swctrl_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_SWCTRL_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_SWCTRL_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_chsel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON11),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_CHSEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_CHSEL_SHIFT)
	    );

}

void mt6313_set_auxadc_dac_extd_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DAC_EXTD_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DAC_EXTD_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_dac_extd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DAC_EXTD_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DAC_EXTD_SHIFT)
	    );

}

void mt6313_set_auxadc_dig1_rsv1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DIG1_RSV1_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DIG1_RSV1_SHIFT)
	    );

}

void mt6313_set_auxadc_qi_vdvfs13_csm_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS13_CSM_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS13_CSM_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_qi_vdvfs14_csm_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON12),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS14_CSM_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_QI_VDVFS14_CSM_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_dig0_rsv1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_DIG0_RSV1_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_DIG0_RSV1_SHIFT)
	    );

}

unsigned char mt6313_get_auxadc_ro_rsv1(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON13),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_RO_RSV1_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_RO_RSV1_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_lbat_max_irq(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON13),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_LBAT_MAX_IRQ_MASK),
				    (unsigned char)(MT6313_PMIC_LBAT_MAX_IRQ_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_lbat_min_irq(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON13),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_LBAT_MIN_IRQ_MASK),
				    (unsigned char)(MT6313_PMIC_LBAT_MIN_IRQ_SHIFT)
	    );


	return val;
}

void mt6313_set_auxadc_autorpt_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON13),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_AUTORPT_EN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_AUTORPT_EN_SHIFT)
	    );

}

void mt6313_set_auxadc_autorpt_prd(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON14),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_AUTORPT_PRD_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_AUTORPT_PRD_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_debt_min(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON15),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DEBT_MIN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DEBT_MIN_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_debt_max(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON16),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DEBT_MAX_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DEBT_MAX_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_det_prd_7_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON17),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DET_PRD_7_0_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DET_PRD_7_0_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_det_prd_15_8(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON18),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DET_PRD_15_8_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DET_PRD_15_8_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_det_prd_19_16(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON19),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DET_PRD_19_16_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_DET_PRD_19_16_SHIFT)
	    );

}

unsigned char mt6313_get_auxadc_lbat_max_irq_b(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON20),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_LBAT_MAX_IRQ_B_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_LBAT_MAX_IRQ_B_SHIFT)
	    );


	return val;
}

void mt6313_set_auxadc_lbat_en_max(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_EN_MAX_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_EN_MAX_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_irq_en_max(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_IRQ_EN_MAX_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_IRQ_EN_MAX_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_volt_max_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON20),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MAX_0_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MAX_0_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_volt_max_1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON21),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MAX_1_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MAX_1_SHIFT)
	    );

}

unsigned char mt6313_get_auxadc_lbat_min_irq_b(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON22),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_AUXADC_LBAT_MIN_IRQ_B_MASK),
				    (unsigned char)(MT6313_PMIC_AUXADC_LBAT_MIN_IRQ_B_SHIFT)
	    );


	return val;
}

void mt6313_set_auxadc_lbat_en_min(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_EN_MIN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_EN_MIN_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_irq_en_min(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_IRQ_EN_MIN_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_IRQ_EN_MIN_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_volt_min_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON22),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MIN_0_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MIN_0_SHIFT)
	    );

}

void mt6313_set_auxadc_lbat_volt_min_1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON23),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MIN_1_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_LBAT_VOLT_MIN_1_SHIFT)
	    );

}

unsigned char mt6313_get_auxadc_lbat_debounce_count_max(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON24),
				    (unsigned char *)(&val),
				    (unsigned
				     char)(MT6313_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX_MASK),
				    (unsigned
				     char)(MT6313_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_auxadc_lbat_debounce_count_min(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_AUXADC_CON25),
				    (unsigned char *)(&val),
				    (unsigned
				     char)(MT6313_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN_MASK),
				    (unsigned
				     char)(MT6313_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN_SHIFT)
	    );


	return val;
}

void mt6313_set_auxadc_enpwm1_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM1_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM1_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm1_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM1_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM1_SW_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm2_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM2_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM2_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm2_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM2_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM2_SW_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm3_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM3_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM3_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm3_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM3_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM3_SW_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm4_sel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM4_SEL_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM4_SEL_SHIFT)
	    );

}

void mt6313_set_auxadc_enpwm4_sw(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_AUXADC_CON26),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM4_SW_MASK),
				      (unsigned char)(MT6313_PMIC_AUXADC_ENPWM4_SW_SHIFT)
	    );

}

void mt6313_set_fqmtr_tcksel(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_FQMTR_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_FQMTR_TCKSEL_MASK),
				      (unsigned char)(MT6313_PMIC_FQMTR_TCKSEL_SHIFT)
	    );

}

unsigned char mt6313_get_fqmtr_busy(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_FQMTR_CON0),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_FQMTR_BUSY_MASK),
				    (unsigned char)(MT6313_PMIC_FQMTR_BUSY_SHIFT)
	    );


	return val;
}

void mt6313_set_fqmtr_en(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_FQMTR_CON0),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_FQMTR_EN_MASK),
				      (unsigned char)(MT6313_PMIC_FQMTR_EN_SHIFT)
	    );

}

void mt6313_set_fqmtr_winset_1(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_FQMTR_CON1),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_FQMTR_WINSET_1_MASK),
				      (unsigned char)(MT6313_PMIC_FQMTR_WINSET_1_SHIFT)
	    );

}

void mt6313_set_fqmtr_winset_0(unsigned char val)
{
	unsigned char ret = 0;


	ret = mt6313_config_interface((unsigned char)(MT6313_FQMTR_CON2),
				      (unsigned char)(val),
				      (unsigned char)(MT6313_PMIC_FQMTR_WINSET_0_MASK),
				      (unsigned char)(MT6313_PMIC_FQMTR_WINSET_0_SHIFT)
	    );

}

unsigned char mt6313_get_fqmtr_data_1(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_FQMTR_CON3),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_FQMTR_DATA_1_MASK),
				    (unsigned char)(MT6313_PMIC_FQMTR_DATA_1_SHIFT)
	    );


	return val;
}

unsigned char mt6313_get_fqmtr_data_0(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;


	ret = mt6313_read_interface((unsigned char)(MT6313_FQMTR_CON4),
				    (unsigned char *)(&val),
				    (unsigned char)(MT6313_PMIC_FQMTR_DATA_0_MASK),
				    (unsigned char)(MT6313_PMIC_FQMTR_DATA_0_SHIFT)
	    );


	return val;
}
