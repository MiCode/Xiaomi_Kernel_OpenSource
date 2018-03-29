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

/*
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/upmu_common.h>
*/
#include "mt6337_upmu_hw.h"
#include "mt6337.h"

unsigned int mt6337_upmu_get_hwcid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_HWCID),
							(&val),
							(unsigned int)(MT6337_PMIC_HWCID_MASK),
							(unsigned int)(MT6337_PMIC_HWCID_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_swcid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_SWCID),
							(&val),
							(unsigned int)(MT6337_PMIC_SWCID_MASK),
							(unsigned int)(MT6337_PMIC_SWCID_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_anacid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ANACID),
							(&val),
							(unsigned int)(MT6337_PMIC_ANACID_MASK),
							(unsigned int)(MT6337_PMIC_ANACID_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_srclken_in0_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN0_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN0_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_vowen_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VOWEN_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VOWEN_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_srclken_in0_hw_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN0_HW_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN0_HW_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_vowen_hw_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VOWEN_HW_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_VOWEN_HW_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_hw_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_HW_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_HW_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_srclken_in_sync_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN_SYNC_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN_SYNC_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_en_auto_off(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_EN_AUTO_OFF_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_EN_AUTO_OFF_SHIFT)
							);

}

unsigned int mt6337_upmu_get_test_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TEST_OUT),
							(&val),
							(unsigned int)(MT6337_PMIC_TEST_OUT_MASK),
							(unsigned int)(MT6337_PMIC_TEST_OUT_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_mon_flag_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TEST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_MON_FLAG_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_MON_FLAG_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_mon_grp_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TEST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_MON_GRP_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_MON_GRP_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_nandtree_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TEST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_NANDTREE_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_NANDTREE_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_test_auxadc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TEST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_TEST_AUXADC_MASK),
							(unsigned int)(MT6337_PMIC_RG_TEST_AUXADC_SHIFT)
							);

}

void mt6337_upmu_set_rg_efuse_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TEST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_test_strup(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TEST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_TEST_STRUP_MASK),
							(unsigned int)(MT6337_PMIC_RG_TEST_STRUP_SHIFT)
							);

}

void mt6337_upmu_set_testmode_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TESTMODE_SW),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TESTMODE_SW_MASK),
							(unsigned int)(MT6337_PMIC_TESTMODE_SW_SHIFT)
							);

}

unsigned int mt6337_upmu_get_va18_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOPSTATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_VA18_PG_DEB_MASK),
							(unsigned int)(MT6337_PMIC_VA18_PG_DEB_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_va18_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOPSTATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_VA18_OC_STATUS_MASK),
							(unsigned int)(MT6337_PMIC_VA18_OC_STATUS_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_va25_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOPSTATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_VA25_OC_STATUS_MASK),
							(unsigned int)(MT6337_PMIC_VA25_OC_STATUS_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_pmu_thr_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOPSTATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_PMU_THR_DEB_MASK),
							(unsigned int)(MT6337_PMIC_PMU_THR_DEB_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_pmu_test_mode_scan(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOPSTATUS1),
							(&val),
							(unsigned int)(MT6337_PMIC_PMU_TEST_MODE_SCAN_MASK),
							(unsigned int)(MT6337_PMIC_PMU_TEST_MODE_SCAN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_pmu_tdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PMU_TDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_PMU_TDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_tdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_TDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_TDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_tdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_TDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_TDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_e32cal_tdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_E32CAL_TDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_E32CAL_TDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_pmu_rdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PMU_RDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_PMU_RDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_rdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_RDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_RDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_rdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_RDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_RDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_e32cal_rdsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RDSEL_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_E32CAL_RDSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_E32CAL_RDSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_wdtrstb_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_WDTRSTB_IN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_WDTRSTB_IN_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_srclken_in0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_SRCLKEN_IN0_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_SRCLKEN_IN0_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_spi_clk(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_CLK_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_CLK_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_spi_csn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_CSN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_CSN_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_spi_mosi(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_MOSI_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_MOSI_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_spi_miso(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_MISO_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_SPI_MISO_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_aud_clk_mosi(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_CLK_MOSI_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_CLK_MOSI_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_aud_dat_mosi1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MOSI1_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MOSI1_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_aud_dat_mosi2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MOSI2_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MOSI2_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_aud_dat_miso1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MISO1_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MISO1_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_aud_dat_miso2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MISO2_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_AUD_DAT_MISO2_SHIFT)
							);

}

void mt6337_upmu_set_rg_smt_vow_clk_miso(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_SMT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMT_VOW_CLK_MISO_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMT_VOW_CLK_MISO_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_srclken_in0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SRCLKEN_IN0_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SRCLKEN_IN0_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_spi_clk(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_CLK_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_CLK_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_spi_csn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_CSN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_CSN_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_spi_mosi(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_MOSI_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_MOSI_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_spi_miso(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_MISO_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_SPI_MISO_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_aud_clk_mosi(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_CLK_MOSI_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_CLK_MOSI_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_aud_dat_mosi1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI1_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI1_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_aud_dat_mosi2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI2_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI2_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_aud_dat_miso1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MISO1_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MISO1_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_aud_dat_miso2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MISO2_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_AUD_DAT_MISO2_SHIFT)
							);

}

void mt6337_upmu_set_rg_octl_vow_clk_miso(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DRV_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OCTL_VOW_CLK_MISO_MASK),
							(unsigned int)(MT6337_PMIC_RG_OCTL_VOW_CLK_MISO_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_spi_clk(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_CLK_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_CLK_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_spi_csn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_CSN_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_CSN_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_spi_mosi(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_MOSI_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_MOSI_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_spi_miso(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_MISO_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_SPI_MISO_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_aud_clk_mosi(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_CLK_MOSI_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_CLK_MOSI_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_aud_dat_mosi1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI1_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI1_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_aud_dat_mosi2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI2_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI2_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_aud_dat_miso1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MISO1_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MISO1_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_aud_dat_miso2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MISO2_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_AUD_DAT_MISO2_SHIFT)
							);

}

void mt6337_upmu_set_rg_filter_vow_clk_miso(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FILTER_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FILTER_VOW_CLK_MISO_MASK),
							(unsigned int)(MT6337_PMIC_RG_FILTER_VOW_CLK_MISO_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_mosi_dly(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DLY_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_MOSI_DLY_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_MOSI_DLY_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_mosi_oe_dly(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DLY_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_MOSI_OE_DLY_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_MOSI_OE_DLY_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_miso_dly(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DLY_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_MISO_DLY_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_MISO_DLY_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_miso_oe_dly(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DLY_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_MISO_OE_DLY_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_MISO_OE_DLY_SHIFT)
							);

}

void mt6337_upmu_set_top_status(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_STATUS),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_STATUS_MASK),
							(unsigned int)(MT6337_PMIC_TOP_STATUS_SHIFT)
							);

}

void mt6337_upmu_set_rg_srclken_in2_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_SPI_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN2_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN2_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_g_smps_pd_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_g_smps_aud_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_smps_ao_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMPS_AO_1M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMPS_AO_1M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_75K_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_75K_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_trim_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_TRIM_75K_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_TRIM_75K_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_vow32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VOW32K_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VOW32K_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_vow12m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VOW12M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VOW12M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_1M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_1M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_smps_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_rng_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audif_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIF_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIF_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_zcd13m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ZCD13M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_ZCD13M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_buck_9m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BUCK_9M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BUCK_9M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_buck_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BUCK_32K_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BUCK_32K_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_intrp_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INTRP_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_INTRP_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_intrp_pre_oc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INTRP_PRE_OC_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_INTRP_PRE_OC_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_efuse_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_fqmtr_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_32K_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_32K_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_fqmtr_26m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_26M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_26M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_fqmtr_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_ldo_cali_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LDO_CALI_75K_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_LDO_CALI_75K_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_stb_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STB_1M_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_STB_1M_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_smps_ck_div_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMPS_CK_DIV_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMPS_CK_DIV_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_accdet_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ACCDET_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_ACCDET_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_test_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_reg_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKPDN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_REG_CK_PDN_MASK),
							(unsigned int)(MT6337_PMIC_RG_REG_CK_PDN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audif_ck_cksel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIF_CK_CKSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIF_CK_CKSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_ck_cksel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_CK_CKSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_CK_CKSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_75k_ck_cksel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_75K_CK_CKSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_75K_CK_CKSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_test_ck_cksel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_CK_CKSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_CK_CKSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_fqmtr_ck_cksel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_CK_CKSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_CK_CKSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_hw_src_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_HW_SRC_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_HW_SRC_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_top_cksel_con0_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_TOP_CKSEL_CON0_RSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_TOP_CKSEL_CON0_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_smps_ck_divsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKDIVSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_SMPS_CK_DIVSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_SMPS_CK_DIVSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_reg_ck_divsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKDIVSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_REG_CK_DIVSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_REG_CK_DIVSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_buck_9m_ck_divsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKDIVSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BUCK_9M_CK_DIVSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_BUCK_9M_CK_DIVSEL_SHIFT)
							);

}

void mt6337_upmu_set_top_ckdivsel_con0_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKDIVSEL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_CKDIVSEL_CON0_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_CKDIVSEL_CON0_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_g_smps_pd_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_g_smps_aud_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_smps_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_efuse_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_stb_1m_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STB_1M_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_STB_1M_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_rng_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_reg_ck_pdn_hwen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_REG_CK_PDN_HWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_REG_CK_PDN_HWEN_SHIFT)
							);

}

void mt6337_upmu_set_top_ckhwen_con0_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_CKHWEN_CON0_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_CKHWEN_CON0_RSV_SHIFT)
							);

}

void mt6337_upmu_set_top_ckhwen_con1_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKHWEN_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_CKHWEN_CON1_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_CKHWEN_CON1_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_pmu75k_ck_tst_dis(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PMU75K_CK_TST_DIS_MASK),
							(unsigned int)(MT6337_PMIC_RG_PMU75K_CK_TST_DIS_SHIFT)
							);

}

void mt6337_upmu_set_rg_smps_ck_tst_dis(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMPS_CK_TST_DIS_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMPS_CK_TST_DIS_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud26m_ck_tst_dis(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD26M_CK_TST_DIS_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD26M_CK_TST_DIS_SHIFT)
							);

}

void mt6337_upmu_set_rg_clk32k_ck_tst_dis(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLK32K_CK_TST_DIS_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLK32K_CK_TST_DIS_SHIFT)
							);

}

void mt6337_upmu_set_rg_vow12m_ck_tst_dis(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VOW12M_CK_TST_DIS_MASK),
							(unsigned int)(MT6337_PMIC_RG_VOW12M_CK_TST_DIS_SHIFT)
							);

}

void mt6337_upmu_set_top_cktst_con0_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_CKTST_CON0_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_CKTST_CON0_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_vow12m_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VOW12M_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VOW12M_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud26m_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD26M_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD26M_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audif_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIF_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIF_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_fqmtr_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_pmu75k_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PMU75K_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_PMU75K_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_smps_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SMPS_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_SMPS_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_75k_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_75K_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_75K_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_clk32k_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLK32K_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLK32K_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_test_ck_tstsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CKTST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_CK_TSTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_CK_TSTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_en_aud(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUD_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUD_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_en_fqr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_FQR_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_FQR_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_en_aux_ap(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_en_aux_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUX_RSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUX_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_en_aux_ap_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_in_sel_va18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_in_sel_va18_swctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_top_clksq_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_CLKSQ_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_CLKSQ_RSV_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_clksq_en_va18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOP_CLKSQ),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_CLKSQ_EN_VA18_MASK),
							(unsigned int)(MT6337_PMIC_DA_CLKSQ_EN_VA18_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_top_clksq_rtc_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ_RTC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_CLKSQ_RTC_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_TOP_CLKSQ_RTC_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_rg_clksq_en_6336(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLKSQ_RTC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_6336_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLKSQ_EN_6336_SHIFT)
							);

}

void mt6337_upmu_set_osc_75k_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_TRIM),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_OSC_75K_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_OSC_75K_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_75k_trim_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_TRIM),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_75K_TRIM_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_75K_TRIM_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_75k_trim_rate(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_TRIM),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_75K_TRIM_RATE_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_75K_TRIM_RATE_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_osc_75k_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOP_CLK_TRIM),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_OSC_75K_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_DA_OSC_75K_TRIM_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_g_smps_ck_pdn_srclken0_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_g_smps_ck_pdn_srclken0_dly_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_DLY_EN_MASK),
					(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_DLY_EN_SHIFT)
					);

}

void mt6337_upmu_set_rg_g_smps_ck_pdn_srclken2_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN2_EN_MASK),
					(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN2_EN_SHIFT)
					);

}

void mt6337_upmu_set_rg_g_smps_ck_pdn_buck_osc_sel_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_BUCK_OSC_SEL_EN_MASK),
					(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_BUCK_OSC_SEL_EN_SHIFT)
					);

}

void mt6337_upmu_set_rg_g_smps_ck_pdn_vowen_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_VOWEN_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_G_SMPS_CK_PDN_VOWEN_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_srclken0_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_srclken0_dly_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_DLY_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_DLY_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_srclken2_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SRCLKEN2_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SRCLKEN2_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_buck_ldo_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_BUCK_LDO_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_BUCK_LDO_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_vowen_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_VOWEN_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_VOWEN_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_spi_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SPI_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SPI_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_clk_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_CLK_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLK_RSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLK_RSV_SHIFT)
							);

}

void mt6337_upmu_set_buck_ldo_ft_testmode_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_LDO_FT_TESTMODE_EN_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_LDO_FT_TESTMODE_EN_SHIFT)
							);

}

void mt6337_upmu_set_buck_all_con0_rsv1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_ALL_CON0_RSV1_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_ALL_CON0_RSV1_SHIFT)
							);

}

void mt6337_upmu_set_buck_all_con0_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_ALL_CON0_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_ALL_CON0_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_buck_buck_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_BUCK_RSV_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_BUCK_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SW_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_SW_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_ldo_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_LDO_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_LDO_MODE_SHIFT)
							);

}

void mt6337_upmu_set_buck_buck_vsleep_srclken_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_BUCK_VSLEEP_SRCLKEN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_BUCK_VSLEEP_SRCLKEN_SEL_SHIFT)
							);

}

void mt6337_upmu_set_buck_all_con2_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_ALL_CON2_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_ALL_CON2_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_buck_vsleep_src0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_VSLEEP_SRC0_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_VSLEEP_SRC0_SHIFT)
							);

}

void mt6337_upmu_set_buck_vsleep_src1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_VSLEEP_SRC1_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_VSLEEP_SRC1_SHIFT)
							);

}

void mt6337_upmu_set_buck_r2r_src0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_R2R_SRC0_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_R2R_SRC0_SHIFT)
							);

}

void mt6337_upmu_set_buck_r2r_src1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_R2R_SRC1_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_R2R_SRC1_SHIFT)
							);

}

void mt6337_upmu_set_rg_osc_sel_dly_max(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_DLY_MAX_MASK),
							(unsigned int)(MT6337_PMIC_RG_OSC_SEL_DLY_MAX_SHIFT)
							);

}

void mt6337_upmu_set_buck_srclken_dly_src1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_ALL_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_SRCLKEN_DLY_SRC1_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_SRCLKEN_DLY_SRC1_SHIFT)
							);

}

void mt6337_upmu_set_rg_efuse_man_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_MAN_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_EFUSE_MAN_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_reg_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_REG_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_REG_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_audio_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIO_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIO_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_accdet_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ACCDET_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_ACCDET_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_intctl_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INTCTL_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_INTCTL_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_buck_cali_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BUCK_CALI_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_BUCK_CALI_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_clk_trim_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CLK_TRIM_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_CLK_TRIM_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_fqmtr_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_FQMTR_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_ldo_cali_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LDO_CALI_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_LDO_CALI_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_zcd_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ZCD_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_ZCD_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_long_press_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_LONG_PRESS_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_LONG_PRESS_RST_SHIFT)
							);

}

void mt6337_upmu_set_top_rst_con1_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_RST_CON1_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_RST_CON1_RSV_SHIFT)
							);

}

void mt6337_upmu_set_top_rst_con2_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_RST_CON2_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_RST_CON2_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_wdtrstb_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_wdtrstb_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_MODE_SHIFT)
							);

}

unsigned int mt6337_upmu_get_wdtrstb_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(&val),
							(unsigned int)(MT6337_PMIC_WDTRSTB_STATUS_MASK),
							(unsigned int)(MT6337_PMIC_WDTRSTB_STATUS_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_wdtrstb_status_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_WDTRSTB_STATUS_CLR_MASK),
							(unsigned int)(MT6337_PMIC_WDTRSTB_STATUS_CLR_SHIFT)
							);

}

void mt6337_upmu_set_rg_wdtrstb_fb_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_FB_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_FB_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_wdtrstb_deb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_DEB_MASK),
							(unsigned int)(MT6337_PMIC_RG_WDTRSTB_DEB_SHIFT)
							);

}

void mt6337_upmu_set_top_rst_misc_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_MISC),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_RST_MISC_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_RST_MISC_RSV_SHIFT)
							);

}

void mt6337_upmu_set_vpwrin_rstb_status(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_STATUS),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_VPWRIN_RSTB_STATUS_MASK),
							(unsigned int)(MT6337_PMIC_VPWRIN_RSTB_STATUS_SHIFT)
							);

}

void mt6337_upmu_set_top_rst_status_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TOP_RST_STATUS),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_TOP_RST_STATUS_RSV_MASK),
							(unsigned int)(MT6337_PMIC_TOP_RST_STATUS_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_thr_h(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_THR_H_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_THR_H_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_thr_l(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_THR_L_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_THR_L_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_audio(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_AUDIO_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_AUDIO_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_mad(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_MAD_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_MAD_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_accdet(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_accdet_eint(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_EINT_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_EINT_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_accdet_eint1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_EINT1_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_EINT1_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_accdet_negv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_NEGV_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_ACCDET_NEGV_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_pmu_thr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_PMU_THR_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_PMU_THR_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_ldo_va18_oc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_LDO_VA18_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_LDO_VA18_OC_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_ldo_va25_oc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_LDO_VA25_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_LDO_VA25_OC_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_en_ldo_va18_pg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_LDO_VA18_PG_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_EN_LDO_VA18_PG_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_thr_h(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_THR_H_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_THR_H_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_thr_l(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_THR_L_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_THR_L_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_audio(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_AUDIO_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_AUDIO_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_mad(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_MAD_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_MAD_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_accdet(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_accdet_eint(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_accdet_eint1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT1_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT1_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_accdet_negv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_NEGV_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_ACCDET_NEGV_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_pmu_thr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_PMU_THR_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_PMU_THR_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_ldo_va18_oc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_OC_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_ldo_va25_oc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_LDO_VA25_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_LDO_VA25_OC_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_mask_b_ldo_va18_pg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_PG_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_PG_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_thr_h(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_THR_H_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_THR_H_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_thr_l(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_THR_L_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_THR_L_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_audio(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_AUDIO_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_AUDIO_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_mad(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_MAD_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_MAD_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_accdet(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_accdet_eint(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_EINT_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_EINT_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_accdet_eint1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_EINT1_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_EINT1_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_accdet_negv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_NEGV_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_ACCDET_NEGV_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_pmu_thr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_PMU_THR_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_PMU_THR_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_ldo_va18_oc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_LDO_VA18_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_LDO_VA18_OC_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_ldo_va25_oc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_LDO_VA25_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_LDO_VA25_OC_SHIFT)
							);

}

void mt6337_upmu_set_rg_int_sel_ldo_va18_pg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_LDO_VA18_PG_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_SEL_LDO_VA18_PG_SHIFT)
							);

}

void mt6337_upmu_set_polarity(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_MISC_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_POLARITY_MASK),
							(unsigned int)(MT6337_PMIC_POLARITY_SHIFT)
							);

}

void mt6337_upmu_set_rg_pchr_cm_vdec_polarity_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_MISC_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PCHR_CM_VDEC_POLARITY_RSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_PCHR_CM_VDEC_POLARITY_RSV_SHIFT)
							);

}

unsigned int mt6337_upmu_get_rg_int_status_thr_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_THR_H_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_THR_H_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_thr_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_THR_L_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_THR_L_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_audio(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_AUDIO_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_AUDIO_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_mad(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_MAD_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_MAD_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_accdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_accdet_eint(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_accdet_eint1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT1_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT1_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_accdet_negv(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_NEGV_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_ACCDET_NEGV_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_pmu_thr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_PMU_THR_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_PMU_THR_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_ldo_va18_oc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_LDO_VA18_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_LDO_VA18_OC_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_ldo_va25_oc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_LDO_VA25_OC_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_LDO_VA25_OC_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_int_status_ldo_va18_pg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STATUS0),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_LDO_VA18_PG_MASK),
							(unsigned int)(MT6337_PMIC_RG_INT_STATUS_LDO_VA18_PG_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_thr_det_dis(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_THR_DET_DIS_MASK),
							(unsigned int)(MT6337_PMIC_RG_THR_DET_DIS_SHIFT)
							);

}

void mt6337_upmu_set_rg_thr_test(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_THR_TEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_THR_TEST_SHIFT)
							);

}

void mt6337_upmu_set_thr_hwpdn_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_THR_HWPDN_EN_MASK),
							(unsigned int)(MT6337_PMIC_THR_HWPDN_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_thermal_deb_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_THERMAL_DEB_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_THERMAL_DEB_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_pg_deb_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_PG_DEB_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_PG_DEB_SEL_SHIFT)
							);

}

void mt6337_upmu_set_strup_con4_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_CON4_RSV_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_CON4_RSV_SHIFT)
							);

}

void mt6337_upmu_set_strup_osc_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_OSC_EN_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_OSC_EN_SHIFT)
							);

}

void mt6337_upmu_set_strup_osc_en_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_OSC_EN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_OSC_EN_SEL_SHIFT)
							);

}

void mt6337_upmu_set_strup_ft_ctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_FT_CTRL_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_FT_CTRL_SHIFT)
							);

}

void mt6337_upmu_set_strup_pwron_force(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_PWRON_FORCE_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_PWRON_FORCE_SHIFT)
							);

}

void mt6337_upmu_set_bias_gen_en_force(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BIAS_GEN_EN_FORCE_MASK),
							(unsigned int)(MT6337_PMIC_BIAS_GEN_EN_FORCE_SHIFT)
							);

}

void mt6337_upmu_set_strup_pwron(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_PWRON_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_PWRON_SHIFT)
							);

}

void mt6337_upmu_set_strup_pwron_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_PWRON_SEL_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_PWRON_SEL_SHIFT)
							);

}

void mt6337_upmu_set_bias_gen_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BIAS_GEN_EN_MASK),
							(unsigned int)(MT6337_PMIC_BIAS_GEN_EN_SHIFT)
							);

}

void mt6337_upmu_set_bias_gen_en_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BIAS_GEN_EN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_BIAS_GEN_EN_SEL_SHIFT)
							);

}

void mt6337_upmu_set_vbgsw_enb_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_VBGSW_ENB_EN_MASK),
							(unsigned int)(MT6337_PMIC_VBGSW_ENB_EN_SHIFT)
							);

}

void mt6337_upmu_set_vbgsw_enb_en_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_VBGSW_ENB_EN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_VBGSW_ENB_EN_SEL_SHIFT)
							);

}

void mt6337_upmu_set_vbgsw_enb_force(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_VBGSW_ENB_FORCE_MASK),
							(unsigned int)(MT6337_PMIC_VBGSW_ENB_FORCE_SHIFT)
							);

}

void mt6337_upmu_set_strup_dig_io_pg_force(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_STRUP_DIG_IO_PG_FORCE_MASK),
							(unsigned int)(MT6337_PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_va18_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_va18_en_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_EN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_EN_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_va18_stb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_STB_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_STB_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_va18_stb_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_STB_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_VA18_STB_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_pgst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_pgst_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_pgst_audenc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_AUDENC_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_AUDENC_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_pgst_audenc_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_AUDENC_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_PGST_AUDENC_SEL_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_osc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_STRUP_CON5),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_OSC_EN_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_OSC_EN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_bgr_unchop(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_UNCHOP_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_UNCHOP_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_unchop_ph(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_UNCHOP_PH_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_UNCHOP_PH_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_rsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_RSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_RSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_trim_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TRIM_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TRIM_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_test_rstb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_RSTB_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_RSTB_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_test_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_TEST_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_bypassmodesel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BYPASSMODESEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_BYPASSMODESEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_bypassmodeen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BYPASSMODEEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BYPASSMODEEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_bgr_osc_en_test(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BGR_OSC_EN_TEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_BGR_OSC_EN_TEST_SHIFT)
							);

}

void mt6337_upmu_set_rg_tsens_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_TSENS_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_TSENS_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_sparebgr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_TSBG_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPAREBGR_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPAREBGR_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_iref_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_IREF_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_IREF_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_rg_vref_bg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VREF_BG_MASK),
							(unsigned int)(MT6337_PMIC_RG_VREF_BG_SHIFT)
							);

}

void mt6337_upmu_set_rg_strup_thr_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_STRUP_THR_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_STRUP_THR_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_thr_tmode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_THR_TMODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_THR_TMODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_thrdet_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_THRDET_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_THRDET_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_vthr_pol(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VTHR_POL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VTHR_POL_SHIFT)
							);

}

void mt6337_upmu_set_rg_spareivgen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPAREIVGEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPAREIVGEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_ovt_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OVT_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OVT_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_ivgen_ext_test_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_IVGEN_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_IVGEN_EXT_TEST_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_IVGEN_EXT_TEST_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_topspareva18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_STRUP_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_TOPSPAREVA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_TOPSPAREVA18_SHIFT)
							);

}

unsigned int mt6337_upmu_get_rgs_ana_chip_id(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_STRUP_ANA_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_RGS_ANA_CHIP_ID_MASK),
							(unsigned int)(MT6337_PMIC_RGS_ANA_CHIP_ID_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_slp_rw_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RG_SPI_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SLP_RW_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SLP_RW_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RG_SPI_CON),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_RSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_RSV_SHIFT)
							);

}

void mt6337_upmu_set_dew_dio_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_DIO_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_DIO_EN_MASK),
							(unsigned int)(MT6337_PMIC_DEW_DIO_EN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_dew_read_test(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_DEW_READ_TEST),
							(&val),
							(unsigned int)(MT6337_PMIC_DEW_READ_TEST_MASK),
							(unsigned int)(MT6337_PMIC_DEW_READ_TEST_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_dew_write_test(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_WRITE_TEST),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_WRITE_TEST_MASK),
							(unsigned int)(MT6337_PMIC_DEW_WRITE_TEST_SHIFT)
							);

}

void mt6337_upmu_set_dew_crc_swrst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CRC_SWRST),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CRC_SWRST_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CRC_SWRST_SHIFT)
							);

}

void mt6337_upmu_set_dew_crc_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CRC_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CRC_EN_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CRC_EN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_dew_crc_val(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_DEW_CRC_VAL),
							(&val),
							(unsigned int)(MT6337_PMIC_DEW_CRC_VAL_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CRC_VAL_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_dew_dbg_mon_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_DBG_MON_SEL),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_DBG_MON_SEL_MASK),
							(unsigned int)(MT6337_PMIC_DEW_DBG_MON_SEL_SHIFT)
							);

}

void mt6337_upmu_set_dew_cipher_key_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CIPHER_KEY_SEL),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_KEY_SEL_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_KEY_SEL_SHIFT)
							);

}

void mt6337_upmu_set_dew_cipher_iv_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CIPHER_IV_SEL),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_IV_SEL_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_IV_SEL_SHIFT)
							);

}

void mt6337_upmu_set_dew_cipher_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CIPHER_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_EN_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_EN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_dew_cipher_rdy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_DEW_CIPHER_RDY),
							(&val),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_RDY_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_RDY_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_dew_cipher_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CIPHER_MODE),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_MODE_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_MODE_SHIFT)
							);

}

void mt6337_upmu_set_dew_cipher_swrst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_CIPHER_SWRST),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_SWRST_MASK),
							(unsigned int)(MT6337_PMIC_DEW_CIPHER_SWRST_SHIFT)
							);

}

void mt6337_upmu_set_dew_rddmy_no(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_DEW_RDDMY_NO),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_DEW_RDDMY_NO_MASK),
							(unsigned int)(MT6337_PMIC_DEW_RDDMY_NO_SHIFT)
							);

}

void mt6337_upmu_set_int_type_con0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_TYPE_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON0_MASK),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON0_SHIFT)
							);

}

void mt6337_upmu_set_int_type_con1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_TYPE_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON1_MASK),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON1_SHIFT)
							);

}

void mt6337_upmu_set_int_type_con2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_TYPE_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON2_MASK),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON2_SHIFT)
							);

}

void mt6337_upmu_set_int_type_con3(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_INT_TYPE_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON3_MASK),
							(unsigned int)(MT6337_PMIC_INT_TYPE_CON3_SHIFT)
							);

}

unsigned int mt6337_upmu_get_cpu_int_sta(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STA),
							(&val),
							(unsigned int)(MT6337_PMIC_CPU_INT_STA_MASK),
							(unsigned int)(MT6337_PMIC_CPU_INT_STA_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_md32_int_sta(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_INT_STA),
							(&val),
							(unsigned int)(MT6337_PMIC_MD32_INT_STA_MASK),
							(unsigned int)(MT6337_PMIC_MD32_INT_STA_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_srclken_in2_smps_clk_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RG_SPI_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN2_SMPS_CLK_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN2_SMPS_CLK_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_srclken_in2_en_smps_test(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RG_SPI_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN2_EN_SMPS_TEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_SRCLKEN_IN2_EN_SMPS_TEST_SHIFT)
							);

}

void mt6337_upmu_set_rg_spi_dly_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RG_SPI_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SPI_DLY_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_SPI_DLY_SEL_SHIFT)
							);

}

void mt6337_upmu_set_fqmtr_tcksel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FQMTR_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_FQMTR_TCKSEL_MASK),
							(unsigned int)(MT6337_PMIC_FQMTR_TCKSEL_SHIFT)
							);

}

unsigned int mt6337_upmu_get_fqmtr_busy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_FQMTR_CON0),
							(&val),
							(unsigned int)(MT6337_PMIC_FQMTR_BUSY_MASK),
							(unsigned int)(MT6337_PMIC_FQMTR_BUSY_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_fqmtr_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FQMTR_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_FQMTR_EN_MASK),
							(unsigned int)(MT6337_PMIC_FQMTR_EN_SHIFT)
							);

}

void mt6337_upmu_set_fqmtr_winset(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_FQMTR_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_FQMTR_WINSET_MASK),
							(unsigned int)(MT6337_PMIC_FQMTR_WINSET_SHIFT)
							);

}

unsigned int mt6337_upmu_get_fqmtr_data(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_FQMTR_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_FQMTR_DATA_MASK),
							(unsigned int)(MT6337_PMIC_FQMTR_DATA_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_buck_k_rst_done(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_RST_DONE_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_RST_DONE_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_map_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_MAP_SEL_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_MAP_SEL_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_once_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_ONCE_EN_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_ONCE_EN_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_once(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_ONCE_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_ONCE_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_start_manual(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_START_MANUAL_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_START_MANUAL_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_src_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_SRC_SEL_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_SRC_SEL_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_auto_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_AUTO_EN_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_AUTO_EN_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_inv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_INV_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_INV_SHIFT)
							);

}

void mt6337_upmu_set_buck_k_control_smps(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_CONTROL_SMPS_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_CONTROL_SMPS_SHIFT)
							);

}

unsigned int mt6337_upmu_get_k_result(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_BUCK_K_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_K_RESULT_MASK),
							(unsigned int)(MT6337_PMIC_K_RESULT_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_k_done(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_BUCK_K_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_K_DONE_MASK),
							(unsigned int)(MT6337_PMIC_K_DONE_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_k_control(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_BUCK_K_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_K_CONTROL_MASK),
							(unsigned int)(MT6337_PMIC_K_CONTROL_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_da_qi_smps_osc_cal(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_BUCK_K_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_SMPS_OSC_CAL_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_SMPS_OSC_CAL_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_buck_k_buck_ck_cnt(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_BUCK_K_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_BUCK_K_BUCK_CK_CNT_MASK),
							(unsigned int)(MT6337_PMIC_BUCK_K_BUCK_CK_CNT_SHIFT)
							);

}

void mt6337_upmu_set_rg_ldo_rsv1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_RSV_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LDO_RSV1_MASK),
							(unsigned int)(MT6337_PMIC_RG_LDO_RSV1_SHIFT)
							);

}

void mt6337_upmu_set_rg_ldo_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_RSV_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LDO_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_RG_LDO_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_rg_ldo_rsv2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_RSV_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LDO_RSV2_MASK),
							(unsigned int)(MT6337_PMIC_RG_LDO_RSV2_SHIFT)
							);

}

void mt6337_upmu_set_ldo_degtd_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_OCFB0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_LDO_DEGTD_SEL_MASK),
							(unsigned int)(MT6337_PMIC_LDO_DEGTD_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_sw_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_SW_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_SW_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_sw_lp(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_SW_LP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_SW_LP_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_sw_op_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_SW_OP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_SW_OP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_hw0_op_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW0_OP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW0_OP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_hw2_op_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW2_OP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW2_OP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_hw0_op_cfg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW0_OP_CFG_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW0_OP_CFG_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_hw2_op_cfg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW2_OP_CFG_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_HW2_OP_CFG_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_go_on_op(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_GO_ON_OP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_GO_ON_OP_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_go_lp_op(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_GO_LP_OP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_GO_LP_OP_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va18_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA18_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_MODE_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_MODE_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_va18_stbtd(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_STBTD_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_STBTD_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va18_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA18_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_STB_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_STB_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_da_qi_va18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA18_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_EN_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_EN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_va18_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_OCFB_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_OCFB_EN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va18_ocfb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA18_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_OCFB_EN_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_OCFB_EN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_va18_dummy_load(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_DUMMY_LOAD_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_DUMMY_LOAD_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va18_dummy_load(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA18_CON3),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_DUMMY_LOAD_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA18_DUMMY_LOAD_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_va25_sw_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_SW_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_SW_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_sw_lp(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_SW_LP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_SW_LP_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_sw_op_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_SW_OP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_SW_OP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_hw0_op_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW0_OP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW0_OP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_hw2_op_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_EN),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW2_OP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW2_OP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_hw0_op_cfg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW0_OP_CFG_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW0_OP_CFG_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_hw2_op_cfg(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW2_OP_CFG_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_HW2_OP_CFG_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_go_on_op(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_GO_ON_OP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_GO_ON_OP_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_go_lp_op(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_OP_CFG),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_GO_LP_OP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_GO_LP_OP_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va25_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA25_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_MODE_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_MODE_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_va25_stbtd(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_STBTD_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_STBTD_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va25_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA25_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_STB_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_STB_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_da_qi_va25_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA25_CON1),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_EN_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_EN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_va25_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_OCFB_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_OCFB_EN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_da_qi_va25_ocfb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_LDO_VA25_CON2),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_OCFB_EN_MASK),
							(unsigned int)(MT6337_PMIC_DA_QI_VA25_OCFB_EN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_dcm_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_DCM),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DCM_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_DCM_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_ck_sw_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CG0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_CK_SW_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_CK_SW_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_ck_sw_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA18_CG0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_CK_SW_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_CK_SW_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_ck_sw_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_CG0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_CK_SW_MODE_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_CK_SW_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_ck_sw_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_LDO_VA25_CG0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_CK_SW_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_CK_SW_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_cal(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA25_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_CAL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_CAL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_vosel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA25_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_VOSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_VOSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_ndis_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA25_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_NDIS_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_NDIS_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va25_fbsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA25_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA25_FBSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA25_FBSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_cal(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA18_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_CAL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_CAL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_vosel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA18_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_VOSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_VOSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_pg_status_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA18_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_PG_STATUS_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_PG_STATUS_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_ndis_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA18_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_NDIS_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_NDIS_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_va18_stb_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_VA18_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VA18_STB_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_VA18_STB_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_pa(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PA_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PA_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_pdin(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PDIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PDIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_ptm(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PTM_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PTM_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_pwe(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PWE_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PWE_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_pprog(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PPROG_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PPROG_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_pwe_src(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PWE_SRC_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PWE_SRC_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_prog_pkey(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_PROG_PKEY_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_PROG_PKEY_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_rd_pkey(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_PKEY_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_PKEY_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_rd_trig(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_TRIG_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_TRIG_SHIFT)
							);

}

void mt6337_upmu_set_rg_rd_rdy_bypass(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_RD_RDY_BYPASS_MASK),
							(unsigned int)(MT6337_PMIC_RG_RD_RDY_BYPASS_SHIFT)
							);

}

void mt6337_upmu_set_rg_skip_otp_out(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SKIP_OTP_OUT_MASK),
							(unsigned int)(MT6337_PMIC_RG_SKIP_OTP_OUT_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_rd_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_SW_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_SW_SHIFT)
							);

}

unsigned int mt6337_upmu_get_rg_otp_rd_busy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_CON13),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_BUSY_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_BUSY_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_rd_ack(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_CON13),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_ACK_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_RD_ACK_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_0_15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_0_15),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_0_15_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_0_15_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_16_31(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_16_31),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_16_31_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_16_31_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_32_47(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_32_47),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_32_47_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_32_47_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_48_63(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_48_63),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_48_63_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_48_63_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_64_79(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_64_79),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_64_79_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_64_79_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_80_95(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_80_95),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_80_95_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_80_95_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_96_111(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_96_111),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_96_111_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_96_111_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_112_127(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_112_127),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_112_127_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_112_127_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_128_143(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_128_143),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_128_143_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_128_143_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_144_159(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_144_159),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_144_159_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_144_159_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_160_175(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_160_175),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_160_175_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_160_175_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_176_191(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_176_191),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_176_191_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_176_191_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_192_207(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_192_207),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_192_207_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_192_207_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_208_223(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_208_223),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_208_223_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_208_223_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_224_239(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_224_239),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_224_239_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_224_239_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_240_255(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_240_255),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_240_255_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_240_255_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_256_271(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_256_271),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_256_271_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_256_271_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_272_287(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_272_287),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_272_287_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_272_287_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_288_303(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_288_303),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_288_303_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_288_303_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_304_319(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_304_319),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_304_319_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_304_319_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_320_335(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_320_335),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_320_335_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_320_335_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_336_351(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_336_351),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_336_351_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_336_351_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_352_367(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_352_367),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_352_367_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_352_367_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_368_383(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_368_383),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_368_383_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_368_383_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_384_399(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_384_399),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_384_399_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_384_399_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_400_415(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_400_415),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_400_415_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_400_415_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_416_431(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_416_431),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_416_431_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_416_431_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_432_447(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_432_447),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_432_447_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_432_447_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_448_463(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_448_463),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_448_463_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_448_463_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_464_479(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_464_479),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_464_479_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_464_479_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_480_495(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_480_495),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_480_495_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_480_495_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_otp_dout_496_511(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_OTP_DOUT_496_511),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_496_511_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_DOUT_496_511_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_otp_val_0_15(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_0_15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_0_15_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_0_15_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_16_31(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_16_31),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_16_31_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_16_31_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_32_47(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_32_47),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_32_47_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_32_47_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_48_63(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_48_63),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_48_63_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_48_63_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_64_79(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_64_79),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_64_79_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_64_79_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_80_90(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_80_95),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_80_90_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_80_90_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_222_223(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_208_223),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_222_223_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_222_223_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_224_239(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_224_239),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_224_239_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_224_239_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_240_252(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_240_255),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_240_252_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_240_252_SHIFT)
							);

}

void mt6337_upmu_set_rg_otp_val_511(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_OTP_VAL_496_511),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_511_MASK),
							(unsigned int)(MT6337_PMIC_RG_OTP_VAL_511_SHIFT)
							);

}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC0),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH0_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC0),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH0_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC1),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH1_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH1_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC1),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH1_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH1_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC2),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH2_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC2),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH2_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC3),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH3_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH3_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC3),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH3_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH3_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC4),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH4_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH4_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC4),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH4_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH4_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC5),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH5_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH5_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC5),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH5_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH5_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC6),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH6_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH6_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC6),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH6_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH6_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC7),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH7_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH7_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC7),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH7_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH7_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch8(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC8),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH8_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH8_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch8(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC8),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH8_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH8_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch9(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC9),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH9_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH9_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch9(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC9),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH9_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH9_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch10(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC10),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH10_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH10_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch10(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC10),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH10_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH10_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch11(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC11),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH11_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH11_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch11(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC11),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH11_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH11_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch12_15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC12),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH12_15_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH12_15_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch12_15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC12),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH12_15_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH12_15_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_thr_hw(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC13),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_THR_HW_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_THR_HW_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_thr_hw(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC13),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_THR_HW_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_THR_HW_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch4_by_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC14),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH4_BY_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH4_BY_MD_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch4_by_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC14),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH4_BY_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH4_BY_MD_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch0_by_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC15),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_MD_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch0_by_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC15),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_MD_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_ch0_by_ap(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC16),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_AP_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_AP_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_rdy_ch0_by_ap(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC16),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_AP_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_AP_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_out_raw(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_ADC17),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_RAW_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_OUT_RAW_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_busy_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_STA0),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_busy_in_share(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_STA1),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHARE_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHARE_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_busy_in_thr_hw(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_STA1),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_HW_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_HW_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_adc_busy_in_thr_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_STA1),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_MD_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_auxadc_rqst_ch0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH0_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH1_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH1_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH2_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch3(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH3_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH3_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch4(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH4_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH4_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch5(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH5_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH5_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch6(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH6_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH6_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch7(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH7_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH7_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch8(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH8_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH8_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch9(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH9_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH9_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch10(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH10_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH10_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch11(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH11_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH11_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch12(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH12_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH12_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch13(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH13_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH13_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch14(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH14_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH14_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch15(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH15_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH15_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch0_by_md(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH0_BY_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH0_BY_MD_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_ch4_by_md(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH4_BY_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_CH4_BY_MD_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rqst_rsv1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_RQST1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_RSV1_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RQST_RSV1_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_ck_on_extd(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_ON_EXTD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_ON_EXTD_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_srclken_src_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SRCLKEN_SRC_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SRCLKEN_SRC_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adc_pwdb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_PWDB_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_PWDB_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adc_pwdb_swctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_PWDB_SWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_PWDB_SWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_strup_ck_on_enb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_STRUP_CK_ON_ENB_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_STRUP_CK_ON_ENB_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adc_rdy_wakeup_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_WAKEUP_CLR_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_RDY_WAKEUP_CLR_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_srclken_ck_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SRCLKEN_CK_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SRCLKEN_CK_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_ck_aon_gps(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_AON_GPS_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_AON_GPS_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_ck_aon_md(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_AON_MD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_AON_MD_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_ck_aon(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_AON_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_CK_AON_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_avg_num_small(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_SMALL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_SMALL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_avg_num_large(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_LARGE_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_LARGE_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_spl_num(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_avg_num_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_avg_num_sel_share(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHARE_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHARE_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_spl_num_large(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_LARGE_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_LARGE_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_spl_num_sleep(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_spl_num_sleep_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_spl_num_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_spl_num_sel_share(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHARE_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHARE_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch0_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH0_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH0_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch1_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH1_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH1_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch2_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH2_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH2_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch3_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH3_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH3_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch4_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH4_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH4_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch5_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH5_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH5_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch6_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH6_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH6_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch7_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH7_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH7_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch8_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH8_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH8_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch9_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH9_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH9_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch10_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH10_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH10_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch11_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH11_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH11_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_trim_ch12_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH12_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TRIM_CH12_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adc_2s_comp_enb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_2S_COMP_ENB_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_2S_COMP_ENB_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adc_trim_comp(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_TRIM_COMP_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADC_TRIM_COMP_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_sw_gain_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SW_GAIN_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SW_GAIN_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_sw_offset_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SW_OFFSET_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SW_OFFSET_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_rng_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_RNG_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_RNG_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_data_reuse_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DATA_REUSE_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DATA_REUSE_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_test_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TEST_MODE_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TEST_MODE_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_bit_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_BIT_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_BIT_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_start_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SW_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SW_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_start_swctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_ts_vbe_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TS_VBE_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TS_VBE_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_ts_vbe_sel_swctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_TS_VBE_SEL_SWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_TS_VBE_SEL_SWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_vbuf_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_VBUF_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_VBUF_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_vbuf_en_swctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_VBUF_EN_SWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_VBUF_EN_SWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_out_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_OUT_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_OUT_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_da_dac(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DA_DAC_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DA_DAC_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_da_dac_swctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DA_DAC_SWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DA_DAC_SWCTRL_SHIFT)
							);

}

unsigned int mt6337_upmu_get_ad_auxadc_comp(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_CON11),
							(&val),
							(unsigned int)(MT6337_PMIC_AD_AUXADC_COMP_MASK),
							(unsigned int)(MT6337_PMIC_AD_AUXADC_COMP_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_auxadc_adcin_vsen_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VSEN_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VSEN_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adcin_vbat_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VBAT_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VBAT_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adcin_vsen_mux_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VSEN_MUX_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VSEN_MUX_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adcin_vsen_ext_baton_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VSEN_EXT_BATON_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_VSEN_EXT_BATON_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adcin_chr_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_CHR_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_CHR_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_adcin_baton_tdet_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_BATON_TDET_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ADCIN_BATON_TDET_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_accdet_anaswctrl_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_ANASWCTRL_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_ANASWCTRL_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dig0_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG0_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG0_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_chsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_CHSEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_CHSEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_swctrl_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SWCTRL_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SWCTRL_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_source_lbat_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SOURCE_LBAT_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SOURCE_LBAT_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_source_lbat2_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_SOURCE_LBAT2_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_SOURCE_LBAT2_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dig0_rsv2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG0_RSV2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG0_RSV2_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dig1_rsv2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG1_RSV2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG1_RSV2_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dac_extd(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DAC_EXTD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DAC_EXTD_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dac_extd_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DAC_EXTD_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DAC_EXTD_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_pmu_thr_pdn_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_PMU_THR_PDN_SW_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_PMU_THR_PDN_SW_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_pmu_thr_pdn_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_PMU_THR_PDN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_PMU_THR_PDN_SEL_SHIFT)
							);

}

unsigned int mt6337_upmu_get_auxadc_pmu_thr_pdn_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_CON14),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_PMU_THR_PDN_STATUS_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_PMU_THR_PDN_STATUS_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_auxadc_dig0_rsv1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG0_RSV1_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DIG0_RSV1_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_start_shade_num(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SHADE_NUM_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SHADE_NUM_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_start_shade_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SHADE_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SHADE_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_start_shade_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SHADE_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_START_SHADE_SEL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_autorpt_prd(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_AUTORPT0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_AUTORPT_PRD_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_AUTORPT_PRD_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_autorpt_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_AUTORPT0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_AUTORPT_EN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_AUTORPT_EN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_accdet_auto_spl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ACCDET),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_AUTO_SPL_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_AUTO_SPL_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_accdet_auto_rqst_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ACCDET),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_AUTO_RQST_CLR_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_AUTO_RQST_CLR_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_accdet_dig1_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ACCDET),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_DIG1_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_DIG1_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_accdet_dig0_rsv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ACCDET),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_DIG0_RSV0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_ACCDET_DIG0_RSV0_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_debt_max(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBT_MAX_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBT_MAX_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_debt_min(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBT_MIN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBT_MIN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_det_prd_15_0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DET_PRD_15_0_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DET_PRD_15_0_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_det_prd_19_16(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DET_PRD_19_16_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DET_PRD_19_16_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_volt_max(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_VOLT_MAX_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_VOLT_MAX_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_irq_en_max(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_IRQ_EN_MAX_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_IRQ_EN_MAX_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_en_max(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_EN_MAX_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_EN_MAX_SHIFT)
							);

}

unsigned int mt6337_upmu_get_auxadc_thr_max_irq_b(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_THR3),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_MAX_IRQ_B_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_MAX_IRQ_B_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_auxadc_thr_volt_min(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_VOLT_MIN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_VOLT_MIN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_irq_en_min(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_IRQ_EN_MIN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_IRQ_EN_MIN_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_thr_en_min(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_THR4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_EN_MIN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_EN_MIN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_auxadc_thr_min_irq_b(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_THR4),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_MIN_IRQ_B_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_MIN_IRQ_B_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_thr_debounce_count_max(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_THR5),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MAX_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MAX_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_auxadc_thr_debounce_count_min(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUXADC_THR6),
							(&val),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_efuse_gain_ch4_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_EFUSE0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_EFUSE_GAIN_CH4_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_EFUSE_GAIN_CH4_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_efuse_offset_ch4_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_EFUSE1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_EFUSE_OFFSET_CH4_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_EFUSE_OFFSET_CH4_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_efuse_gain_ch0_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_EFUSE2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_EFUSE_GAIN_CH0_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_EFUSE_GAIN_CH0_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_efuse_offset_ch0_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_EFUSE3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_EFUSE_OFFSET_CH0_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_EFUSE_OFFSET_CH0_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_efuse_gain_ch7_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_EFUSE4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_EFUSE_GAIN_CH7_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_EFUSE_GAIN_CH7_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_efuse_offset_ch7_trim(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_EFUSE5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_EFUSE_OFFSET_CH7_TRIM_MASK),
							(unsigned int)(MT6337_PMIC_EFUSE_OFFSET_CH7_TRIM_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dbg_dig0_rsv2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_DBG0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DBG_DIG0_RSV2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DBG_DIG0_RSV2_SHIFT)
							);

}

void mt6337_upmu_set_auxadc_dbg_dig1_rsv2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_DBG0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUXADC_DBG_DIG1_RSV2_MASK),
							(unsigned int)(MT6337_PMIC_AUXADC_DBG_DIG1_RSV2_SHIFT)
							);

}

void mt6337_upmu_set_rg_auxadc_cali(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_CALI_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUXADC_CALI_SHIFT)
							);

}

void mt6337_upmu_set_rg_aux_rsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUX_RSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUX_RSV_SHIFT)
							);

}

void mt6337_upmu_set_rg_vbuf_byp(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VBUF_BYP_MASK),
							(unsigned int)(MT6337_PMIC_RG_VBUF_BYP_SHIFT)
							);

}

void mt6337_upmu_set_rg_vbuf_calen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VBUF_CALEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VBUF_CALEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_vbuf_exten(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VBUF_EXTEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_VBUF_EXTEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_rng_mod(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_RNG_MOD_MASK),
							(unsigned int)(MT6337_PMIC_RG_RNG_MOD_SHIFT)
							);

}

void mt6337_upmu_set_rg_rng_ana_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_RNG_ANA_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_RNG_ANA_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_rng_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUXADC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_RNG_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_RNG_SEL_SHIFT)
							);

}

unsigned int mt6337_upmu_get_ad_audaccdetcmpoc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON0),
							(&val),
							(unsigned int)(MT6337_PMIC_AD_AUDACCDETCMPOC_MASK),
							(unsigned int)(MT6337_PMIC_AD_AUDACCDETCMPOC_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_audaccdetanaswctrlenb(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETANASWCTRLENB_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETANASWCTRLENB_SHIFT)
							);

}

void mt6337_upmu_set_rg_accdetsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ACCDETSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_ACCDETSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetswctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETSWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETSWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdettvdet(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETTVDET_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETTVDET_SHIFT)
							);

}

void mt6337_upmu_set_audaccdetauxadcswctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUDACCDETAUXADCSWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SHIFT)
							);

}

void mt6337_upmu_set_audaccdetauxadcswctrl_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SEL_MASK),
							(unsigned int)(MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetrsv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETRSV_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETRSV_SHIFT)
							);

}

void mt6337_upmu_set_accdet_sw_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_SW_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_SW_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_seq_init(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_SEQ_INIT_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_SEQ_INIT_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_seq_init(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_SEQ_INIT_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_SEQ_INIT_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_seq_init(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_SEQ_INIT_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_SEQ_INIT_SHIFT)
							);

}

void mt6337_upmu_set_accdet_negvdet_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGVDET_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGVDET_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_negvdet_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGVDET_EN_CTRL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGVDET_EN_CTRL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_anaswctrl_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_ANASWCTRL_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_ANASWCTRL_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp_pwm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_PWM_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_PWM_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_vth_pwm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_PWM_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_PWM_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_mbias_pwm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_PWM_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_PWM_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp1_pwm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_PWM_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_PWM_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_pwm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_pwm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_EN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_EN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_PWM_IDLE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_PWM_IDLE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_vth_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_PWM_IDLE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_PWM_IDLE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_mbias_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_PWM_IDLE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_PWM_IDLE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp1_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_PWM_IDLE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_PWM_IDLE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_IDLE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_IDLE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_IDLE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_IDLE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_pwm_width(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_WIDTH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_WIDTH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_pwm_thresh(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_THRESH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_THRESH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_rise_delay(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_RISE_DELAY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_RISE_DELAY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_fall_delay(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_FALL_DELAY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_FALL_DELAY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE0_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE0_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE1_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE1_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE2_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE2_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce3(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE3_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE3_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce4(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE4_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE4_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce5(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE5_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE5_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce6(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE6_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE6_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce7(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE7_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE7_SHIFT)
							);

}

void mt6337_upmu_set_accdet_debounce8(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE8_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_DEBOUNCE8_SHIFT)
							);

}

void mt6337_upmu_set_accdet_ival_cur_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_CUR_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_CUR_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_ival_sam_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_SAM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_SAM_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_ival_mem_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_MEM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_MEM_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_ival_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IVAL_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_ival_cur_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_CUR_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_CUR_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_ival_sam_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_SAM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_SAM_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_ival_mem_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_MEM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_MEM_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_ival_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IVAL_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_ival_cur_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_CUR_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_CUR_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_ival_sam_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_SAM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_SAM_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_ival_mem_in(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_MEM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_MEM_IN_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_ival_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IVAL_SEL_SHIFT)
							);

}

unsigned int mt6337_upmu_get_accdet_irq(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON17),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_IRQ_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IRQ_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_irq(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON17),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_IRQ_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_IRQ_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint_irq(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON17),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IRQ_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IRQ_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint1_irq(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON17),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IRQ_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IRQ_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_accdet_irq_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_IRQ_CLR_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IRQ_CLR_SHIFT)
							);

}

void mt6337_upmu_set_accdet_negv_irq_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_IRQ_CLR_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_IRQ_CLR_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_irq_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IRQ_CLR_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IRQ_CLR_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_irq_clr(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IRQ_CLR_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_irq_polarity(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IRQ_POLARITY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_IRQ_POLARITY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_irq_polarity(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IRQ_POLARITY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_IRQ_POLARITY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE0_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE0_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp_swsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_SWSEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_SWSEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_vth_swsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_SWSEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_SWSEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_mbias_swsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_SWSEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_SWSEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp1_swsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_SWSEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_SWSEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_pwm_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp1_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP1_EN_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_cmp_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_EN_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_vth_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_EN_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_mbias_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_EN_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_pwm_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_PWM_EN_SW_SHIFT)
							);

}

unsigned int mt6337_upmu_get_accdet_mbias_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON19),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_CLK_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_MBIAS_CLK_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_vth_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON19),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_CLK_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_VTH_CLK_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_cmp_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON19),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_CLK_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMP_CLK_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet2_cmp_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON19),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET2_CMP_CLK_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET2_CMP_CLK_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_da_ni_audaccdetauxadcswctrl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON19),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_NI_AUDACCDETAUXADCSWCTRL_MASK),
							(unsigned int)(MT6337_PMIC_DA_NI_AUDACCDETAUXADCSWCTRL_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_cmpc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON20),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_CMPC_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CMPC_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON20),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_cur_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON20),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_CUR_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CUR_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_sam_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON20),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_SAM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_SAM_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_mem_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON20),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_MEM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_MEM_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_state(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON20),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_STATE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_STATE_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_accdet_eint_deb_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON21),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_DEB_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_DEB_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_debounce(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON21),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_DEBOUNCE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_DEBOUNCE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_pwm_thresh(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON21),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_THRESH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_THRESH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_pwm_width(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON21),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_WIDTH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_WIDTH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_deb_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_DEB_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_DEB_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_debounce(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_DEBOUNCE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_DEBOUNCE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_pwm_thresh(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_THRESH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_THRESH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_pwm_width(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_WIDTH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_WIDTH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_negv_thresh(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_THRESH_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_THRESH_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_pwm_fall_delay(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_FALL_DELAY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_FALL_DELAY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_pwm_rise_delay(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_RISE_DELAY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_PWM_RISE_DELAY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_pwm_fall_delay(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_FALL_DELAY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_FALL_DELAY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_pwm_rise_delay(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_RISE_DELAY_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_PWM_RISE_DELAY_SHIFT)
							);

}

void mt6337_upmu_set_accdet_nvdetectout_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON25),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_NVDETECTOUT_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NVDETECTOUT_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_cmpout_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON25),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CMPOUT_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CMPOUT_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_cmpout_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON25),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CMPOUT_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CMPOUT_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_auxadc_ctrl_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON25),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_CTRL_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_CTRL_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_cmp_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON25),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CMP_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CMP_EN_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_cmp_en_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON25),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CMP_EN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CMP_EN_SW_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode13(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE13_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE13_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode12(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE12_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE12_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode11(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE11_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE11_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode10(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE10_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE10_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode9(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE9_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE9_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode8(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE8_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE8_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode7(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE7_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE7_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode6(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE6_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE6_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode5(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE5_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE5_SHIFT)
							);

}

void mt6337_upmu_set_accdet_test_mode4(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE4_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_TEST_MODE4_SHIFT)
							);

}

void mt6337_upmu_set_accdet_in_sw(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON26),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_IN_SW_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_IN_SW_SHIFT)
							);

}

unsigned int mt6337_upmu_get_accdet_eint_state(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_STATE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_STATE_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_auxadc_debounce_end(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_DEBOUNCE_END_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_DEBOUNCE_END_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_auxadc_connect_pre(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_CONNECT_PRE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_CONNECT_PRE_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint_cur_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CUR_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CUR_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint_sam_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_SAM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_SAM_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint_mem_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_MEM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_MEM_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_ad_nvdetectout(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_AD_NVDETECTOUT_MASK),
							(unsigned int)(MT6337_PMIC_AD_NVDETECTOUT_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_ad_eint1cmpout(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_AD_EINT1CMPOUT_MASK),
							(unsigned int)(MT6337_PMIC_AD_EINT1CMPOUT_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_da_ni_eint1cmpen(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON27),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_NI_EINT1CMPEN_MASK),
							(unsigned int)(MT6337_PMIC_DA_NI_EINT1CMPEN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint1_state(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON28),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_STATE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_STATE_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint1_cur_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON28),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CUR_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CUR_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint1_sam_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON28),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_SAM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_SAM_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint1_mem_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON28),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_MEM_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_MEM_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_ad_eint2cmpout(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON28),
							(&val),
							(unsigned int)(MT6337_PMIC_AD_EINT2CMPOUT_MASK),
							(unsigned int)(MT6337_PMIC_AD_EINT2CMPOUT_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_da_ni_eint2cmpen(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON28),
							(&val),
							(unsigned int)(MT6337_PMIC_DA_NI_EINT2CMPEN_MASK),
							(unsigned int)(MT6337_PMIC_DA_NI_EINT2CMPEN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_count_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON29),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_COUNT_IN_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_COUNT_IN_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_en_final(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON29),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_EN_FINAL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_EN_FINAL_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_count_end(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON29),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_COUNT_END_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_COUNT_END_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_minu(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON29),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_MINU_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_MINU_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_add(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON29),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_ADD_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_ADD_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_negv_cmp(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON29),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_CMP_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_NEGV_CMP_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_cur_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON30),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_CUR_DEB_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_CUR_DEB_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint_cur_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON31),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CUR_DEB_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_CUR_DEB_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_accdet_eint1_cur_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON32),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CUR_DEB_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_CUR_DEB_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_accdet_rsv_con0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON33),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_RSV_CON0_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_RSV_CON0_SHIFT)
							);

}

unsigned int mt6337_upmu_get_accdet_rsv_con1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ACCDET_CON34),
							(&val),
							(unsigned int)(MT6337_PMIC_ACCDET_RSV_CON1_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_RSV_CON1_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_accdet_auxadc_connect_time(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON35),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_CONNECT_TIME_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_AUXADC_CONNECT_TIME_SHIFT)
							);

}

void mt6337_upmu_set_accdet_hwen_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON36),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_HWEN_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_HWEN_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint_reverse(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON36),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_REVERSE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT_REVERSE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_eint1_reverse(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON36),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_REVERSE_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EINT1_REVERSE_SHIFT)
							);

}

void mt6337_upmu_set_accdet_hwmode_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON36),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_HWMODE_SEL_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_HWMODE_SEL_SHIFT)
							);

}

void mt6337_upmu_set_accdet_en_cmpc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ACCDET_CON36),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_ACCDET_EN_CMPC_MASK),
							(unsigned int)(MT6337_PMIC_ACCDET_EN_CMPC_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddaclpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDACLPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDACLPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddacrpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDACRPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDACRPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_dac_pwr_up_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_DAC_PWR_UP_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_DAC_PWR_UP_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_aud_dac_pwl_up_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUD_DAC_PWL_UP_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUD_DAC_PWL_UP_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplpwrup_ibias_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLPWRUP_IBIAS_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLPWRUP_IBIAS_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprpwrup_ibias_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRPWRUP_IBIAS_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRPWRUP_IBIAS_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplmuxinputsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprmuxinputsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplscdisable_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLSCDISABLE_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLSCDISABLE_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprscdisable_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRSCDISABLE_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRSCDISABLE_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplbsccurrent_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLBSCCURRENT_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLBSCCURRENT_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprbsccurrent_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRBSCCURRENT_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRBSCCURRENT_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhploutpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLOUTPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLOUTPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhproutpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPROUTPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPROUTPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhploutauxpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLOUTAUXPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLOUTAUXPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhproutauxpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPROUTAUXPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPROUTAUXPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hplauxfbrsw_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPLAUXFBRSW_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPLAUXFBRSW_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hprauxfbrsw_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPRAUXFBRSW_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPRAUXFBRSW_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hplshort2hplaux_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPLSHORT2HPLAUX_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPLSHORT2HPLAUX_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hprshort2hpraux_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPRSHORT2HPRAUX_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPRSHORT2HPRAUX_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hppoutstgctrl_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPPOUTSTGCTRL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPPOUTSTGCTRL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hpnoutstgctrl_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPNOUTSTGCTRL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPNOUTSTGCTRL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hppoutputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPPOUTPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPPOUTPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hpnoutputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPNOUTPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPNOUTPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpstartup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPSTARTUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPSTARTUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrefn_deres_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDREFN_DERES_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDREFN_DERES_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hppshort2vcm_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPPSHORT2VCM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPPSHORT2VCM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhptrim_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPTRIM_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPTRIM_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpltrim_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLTRIM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLTRIM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplfinetrim_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLFINETRIM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLFINETRIM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprtrim_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRTRIM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRTRIM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprfinetrim_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRFINETRIM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRFINETRIM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hpinputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPINPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPINPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hpinputreset0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPINPUTRESET0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPINPUTRESET0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hpoutputreset0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HPOUTPUTRESET0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HPOUTPUTRESET0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpcomp_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPCOMP_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPCOMP_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpdiffinpbiasadj_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPDIFFINPBIASADJ_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPDIFFINPBIASADJ_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplfcompressel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLFCOMPRESSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLFCOMPRESSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhphfcompressel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPHFCOMPRESSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPHFCOMPRESSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhphfcompbufgainsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON4),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_AUDHPHFCOMPBUFGAINSEL_VAUDP32_MASK),
					(unsigned int)(MT6337_PMIC_RG_AUDHPHFCOMPBUFGAINSEL_VAUDP32_SHIFT)
					);

}

void mt6337_upmu_set_rg_audhpdecmgainadj_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPDECMGAINADJ_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPDECMGAINADJ_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpdedmgainadj_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPDEDMGAINADJ_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPDEDMGAINADJ_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhspwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhspwrup_ibias_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSPWRUP_IBIAS_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSPWRUP_IBIAS_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhsmuxinputsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSMUXINPUTSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSMUXINPUTSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhsscdisable_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSSCDISABLE_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSSCDISABLE_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhsbsccurrent_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSBSCCURRENT_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSBSCCURRENT_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhsstartup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSSTARTUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSSTARTUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hsoutputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HSOUTPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HSOUTPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hsinputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HSINPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HSINPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hsinputreset0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HSINPUTRESET0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HSINPUTRESET0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hsoutputreset0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HSOUTPUTRESET0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HSOUTPUTRESET0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_hsout_shortvcm_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_HSOUT_SHORTVCM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_HSOUT_SHORTVCM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlolpwrup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLPWRUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLPWRUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlolpwrup_ibias_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLPWRUP_IBIAS_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLPWRUP_IBIAS_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlolmuxinputsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLMUXINPUTSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLMUXINPUTSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlolscdisable_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLSCDISABLE_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLSCDISABLE_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlolbsccurrent_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLBSCCURRENT_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLBSCCURRENT_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlostartup_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOSTARTUP_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOSTARTUP_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_loinputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LOINPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LOINPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_looutputstbenh_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LOOUTPUTSTBENH_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LOOUTPUTSTBENH_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_loinputreset0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LOINPUTRESET0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LOINPUTRESET0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_looutputreset0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LOOUTPUTRESET0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LOOUTPUTRESET0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_loout_shortvcm_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LOOUT_SHORTVCM_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LOOUT_SHORTVCM_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audtrimbuf_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDTRIMBUF_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDTRIMBUF_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audtrimbuf_inputmuxsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON8),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP32_MASK),
					(unsigned int)(MT6337_PMIC_RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP32_SHIFT)
					);

}

void mt6337_upmu_set_rg_audtrimbuf_gainsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDTRIMBUF_GAINSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDTRIMBUF_GAINSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpspkdet_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPSPKDET_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPSPKDET_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhpspkdet_inputmuxsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON8),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP32_MASK),
					(unsigned int)(MT6337_PMIC_RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP32_SHIFT)
					);

}

void mt6337_upmu_set_rg_audhpspkdet_outputmuxsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON8),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP32_MASK),
					(unsigned int)(MT6337_PMIC_RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP32_SHIFT)
					);

}

void mt6337_upmu_set_rg_abidec_rsvd0_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ABIDEC_RSVD0_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_ABIDEC_RSVD0_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_abidec_rsvd0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ABIDEC_RSVD0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_ABIDEC_RSVD0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdmuxsel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdclksel_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audbiasadj_0_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDBIASADJ_0_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDBIASADJ_0_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audbiasadj_1_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDBIASADJ_1_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDBIASADJ_1_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audibiaspwrdn_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIBIASPWRDN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIBIASPWRDN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_rstb_decoder_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_RSTB_DECODER_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_RSTB_DECODER_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_sel_decoder_96k_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SEL_DECODER_96K_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_SEL_DECODER_96K_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_sel_delay_vcore(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SEL_DELAY_VCORE_MASK),
							(unsigned int)(MT6337_PMIC_RG_SEL_DELAY_VCORE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglb_pwrdn_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_PWRDN_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_PWRDN_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglb_lp_vow_en_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_LP_VOW_EN_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_LP_VOW_EN_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglb_lp2_vow_en_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_LP2_VOW_EN_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_LP2_VOW_EN_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglb_nvreg_l_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_NVREG_L_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_NVREG_L_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglb_nvreg_r_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_NVREG_R_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLB_NVREG_R_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_lcldo_decl_en_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECL_EN_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECL_EN_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_lcldo_decl_pddis_en_va18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECL_PDDIS_EN_VA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECL_PDDIS_EN_VA18_SHIFT)
							);

}

void mt6337_upmu_set_rg_lcldo_decl_remote_sense_va18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_LCLDO_DECL_REMOTE_SENSE_VA18_MASK),
					(unsigned int)(MT6337_PMIC_RG_LCLDO_DECL_REMOTE_SENSE_VA18_SHIFT)
					);

}

void mt6337_upmu_set_rg_lcldo_decr_en_va32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECR_EN_VA32_MASK),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECR_EN_VA32_SHIFT)
							);

}

void mt6337_upmu_set_rg_lcldo_decr_pddis_en_va18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECR_PDDIS_EN_VA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_LCLDO_DECR_PDDIS_EN_VA18_SHIFT)
							);

}

void mt6337_upmu_set_rg_lcldo_decr_remote_sense_va18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
					(unsigned int)(val),
					(unsigned int)(MT6337_PMIC_RG_LCLDO_DECR_REMOTE_SENSE_VA18_MASK),
					(unsigned int)(MT6337_PMIC_RG_LCLDO_DECR_REMOTE_SENSE_VA18_SHIFT)
					);

}

void mt6337_upmu_set_rg_audpmu_rsvd_va18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPMU_RSVD_VA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPMU_RSVD_VA18_SHIFT)
							);

}

void mt6337_upmu_set_rg_nvregl_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_NVREGL_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_NVREGL_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_nvregr_en_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_NVREGR_EN_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_NVREGR_EN_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_nvregl_pull0v_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_NVREGL_PULL0V_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_NVREGL_PULL0V_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_nvregr_pull0v_vaudp32(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDDEC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_NVREGR_PULL0V_VAUDP32_MASK),
							(unsigned int)(MT6337_PMIC_RG_NVREGR_PULL0V_VAUDP32_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01on(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01ON_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01ON_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01dccen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01DCCEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01DCCEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01dcprecharge(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01DCPRECHARGE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01DCPRECHARGE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01hpmodeen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01HPMODEEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01HPMODEEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01vscale(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01VSCALE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01VSCALE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01gain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01GAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01GAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch0_01pgatest(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01PGATEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH0_01PGATEST_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch0_01pwrup(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH0_01PWRUP_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH0_01PWRUP_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch0_01inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH0_01INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH0_01INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23on(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23ON_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23ON_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23dccen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23DCCEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23DCCEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23dcprecharge(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23DCPRECHARGE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23DCPRECHARGE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23hpmodeen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23HPMODEEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23HPMODEEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23vscale(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23VSCALE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23VSCALE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23gain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23GAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23GAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch1_23pgatest(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23PGATEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH1_23PGATEST_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch1_23pwrup(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH1_23PWRUP_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH1_23PWRUP_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch1_23inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH1_23INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH1_23INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45on(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45ON_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45ON_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45dccen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45DCCEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45DCCEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45dcprecharge(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45DCPRECHARGE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45DCPRECHARGE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45hpmodeen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45HPMODEEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45HPMODEEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45vscale(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45VSCALE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45VSCALE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45gain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45GAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45GAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch2_45pgatest(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45PGATEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH2_45PGATEST_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch2_45pwrup(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH2_45PWRUP_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH2_45PWRUP_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch2_45inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH2_45INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH2_45INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6on(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6ON_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6ON_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6dccen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6DCCEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6DCCEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6dcprecharge(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6DCPRECHARGE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6DCPRECHARGE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6hpmodeen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6HPMODEEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6HPMODEEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6vscale(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6VSCALE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6VSCALE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6gain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6GAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6GAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampch3_6pgatest(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6PGATEST_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCH3_6PGATEST_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch3_6pwrup(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH3_6PWRUP_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH3_6PWRUP_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcch3_6inputsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH3_6INPUTSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCH3_6INPUTSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audulhalfbias_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDULHALFBIAS_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDULHALFBIAS_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglbvowlpwen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreamplpen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPLPEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPLPEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc1ststagelpen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc2ndstagelpen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcflashlpen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHLPEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHLPEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampiddtest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc1ststageiddtest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc2ndstageiddtest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcrefbufiddtest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcflashiddtest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audulhalfbias_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDULHALFBIAS_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDULHALFBIAS_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audglbvowlpwen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreamplpen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPLPEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPLPEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc1ststagelpen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc2ndstagelpen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcflashlpen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHLPEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHLPEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampiddtest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc1ststageiddtest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc2ndstageiddtest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcrefbufiddtest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcflashiddtest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON5),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdac0p25fs_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDAC0P25FS_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDAC0P25FS_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclksel_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSEL_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSEL_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclksource_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSOURCE_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSOURCE_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclkgenmode_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKGENMODE_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKGENMODE_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclkrstb_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKRSTB_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKRSTB_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampcascadeen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampfbcapen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampaafen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPAAFEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPAAFEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_dccvcmbuflpmodsel_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_dccvcmbuflpswen_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audsparepga_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON6),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREPGA_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREPGA_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdac0p25fs_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDAC0P25FS_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDAC0P25FS_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclksel_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSEL_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSEL_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclksource_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSOURCE_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKSOURCE_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclkgenmode_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKGENMODE_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKGENMODE_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcclkrstb_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKRSTB_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCCLKRSTB_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampcascadeen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampfbcapen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpreampaafen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPAAFEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPREAMPAAFEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_dccvcmbuflpmodsel_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_dccvcmbuflpswen_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audsparepga_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON7),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREPGA_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREPGA_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc1ststagesdenb_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc2ndstagereset_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc3rdstagereset_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcfsreset_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFSRESET_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFSRESET_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcwidecm_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCWIDECM_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCWIDECM_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcnopatest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNOPATEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNOPATEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcbypass_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCBYPASS_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCBYPASS_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcffbypass_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFFBYPASS_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFFBYPASS_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdacfbcurrent_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdaciddtest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACIDDTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACIDDTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdacnrz_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACNRZ_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACNRZ_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcnodem_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNODEM_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNODEM_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdactest_ch01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON8),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACTEST_CH01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACTEST_CH01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc1ststagesdenb_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc2ndstagereset_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadc3rdstagereset_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcfsreset_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFSRESET_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFSRESET_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcwidecm_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCWIDECM_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCWIDECM_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcnopatest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNOPATEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNOPATEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcbypass_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCBYPASS_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCBYPASS_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcffbypass_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFFBYPASS_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCFFBYPASS_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdacfbcurrent_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdaciddtest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACIDDTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACIDDTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdacnrz_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACNRZ_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACNRZ_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcnodem_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNODEM_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCNODEM_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audadcdactest_ch23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON9),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACTEST_CH23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDADCDACTEST_CH23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech0_01(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH0_01_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH0_01_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech0_01sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH0_01SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH0_01SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech1_23(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH1_23_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH1_23_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech1_23sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON10),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH1_23SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH1_23SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech2_45(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH2_45_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH2_45_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech2_45sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH2_45SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH2_45SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech3_6(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH3_6_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH3_6_SHIFT)
							);

}

void mt6337_upmu_set_rg_audrctunech3_6sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON11),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH3_6SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDRCTUNECH3_6SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audspareva30(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREVA30_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREVA30_SHIFT)
							);

}

void mt6337_upmu_set_rg_audspareva18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON12),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREVA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDSPAREVA18_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic0en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic0bias(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0BIAS_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0BIAS_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic0hpclken(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC0HPCLKEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC0HPCLKEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic0pduty(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0PDUTY_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0PDUTY_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic0nduty(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON13),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0NDUTY_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC0NDUTY_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic1en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic1bias(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1BIAS_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1BIAS_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic1hpclken(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC1HPCLKEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC1HPCLKEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic1pduty(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1PDUTY_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1PDUTY_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic1nduty(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1NDUTY_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC1NDUTY_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic1monen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC1MONEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC1MONEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic1monsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON14),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC1MONSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC1MONSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic2en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic2bias(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2BIAS_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2BIAS_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic2hpclken(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC2HPCLKEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC2HPCLKEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic2pduty(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2PDUTY_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2PDUTY_SHIFT)
							);

}

void mt6337_upmu_set_rg_auddigmic2nduty(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2NDUTY_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDDIGMIC2NDUTY_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic2monen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC2MONEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC2MONEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic2monsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC2MONSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC2MONSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_dmic2rdatasel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON15),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_DMIC2RDATASEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_DMIC2RDATASEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpwdbmicbias0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS0_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS0_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias0dcsw0p1en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0DCSW0P1EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0DCSW0P1EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias0dcsw0p2en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0DCSW0P2EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0DCSW0P2EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias0dcsw0nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0DCSW0NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0DCSW0NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias0vref(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0VREF_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0VREF_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias0lowpen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0LOWPEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS0LOWPEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpwdbmicbias2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS2_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS2_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2dcsw2p1en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW2P1EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW2P1EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2dcsw2p2en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW2P2EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW2P2EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2dcsw2nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW2NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW2NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2vref(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2VREF_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2VREF_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2lowpen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON16),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2LOWPEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2LOWPEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpwdbmicbias1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS1_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS1_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1dcsw1pen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW1PEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW1PEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1dcsw1nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW1NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW1NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1vref(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1VREF_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1VREF_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1lowpen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1LOWPEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1LOWPEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1dcsw3pen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW3PEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW3PEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1dcsw3nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW3NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1DCSW3NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2dcsw3pen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW3PEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW3PEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias2dcsw3nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW3NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS2DCSW3NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1hven(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1HVEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1HVEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias1hvvref(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1HVVREF_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS1HVVREF_SHIFT)
							);

}

void mt6337_upmu_set_rg_bandgapgen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON17),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_BANDGAPGEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_BANDGAPGEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audpwdbmicbias3(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS3_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDPWDBMICBIAS3_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw4p1en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW4P1EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW4P1EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw4p2en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW4P2EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW4P2EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw4nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW4NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW4NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3vref(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3VREF_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3VREF_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3lowpen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3LOWPEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3LOWPEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw5p1en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW5P1EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW5P1EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw5p2en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW5P2EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW5P2EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw5nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW5NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW5NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw6p1en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW6P1EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW6P1EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw6p2en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW6P2EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW6P2EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audmicbias3dcsw6nen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON18),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW6NEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDMICBIAS3DCSW6NEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetmicbias0pulllow(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS0PULLLOW_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS0PULLLOW_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetmicbias1pulllow(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS1PULLLOW_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS1PULLLOW_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetmicbias2pulllow(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS2PULLLOW_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS2PULLLOW_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetmicbias3pulllow(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS3PULLLOW_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETMICBIAS3PULLLOW_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetvin1pulllow(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETVIN1PULLLOW_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETVIN1PULLLOW_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetvthacal(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETVTHACAL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETVTHACAL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audaccdetvthbcal(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETVTHBCAL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDACCDETVTHBCAL_SHIFT)
							);

}

void mt6337_upmu_set_rg_accdet1sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ACCDET1SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_ACCDET1SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_accdet2sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ACCDET2SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_ACCDET2SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_swbufmodsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SWBUFMODSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_SWBUFMODSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_swbufswen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_SWBUFSWEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_SWBUFSWEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_eintcompvth(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EINTCOMPVTH_MASK),
							(unsigned int)(MT6337_PMIC_RG_EINTCOMPVTH_SHIFT)
							);

}

void mt6337_upmu_set_rg_eint1configaccdet1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EINT1CONFIGACCDET1_MASK),
							(unsigned int)(MT6337_PMIC_RG_EINT1CONFIGACCDET1_SHIFT)
							);

}

void mt6337_upmu_set_rg_eint2configaccdet2(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON19),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_EINT2CONFIGACCDET2_MASK),
							(unsigned int)(MT6337_PMIC_RG_EINT2CONFIGACCDET2_SHIFT)
							);

}

void mt6337_upmu_set_rg_accdetspare(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON20),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_ACCDETSPARE_MASK),
							(unsigned int)(MT6337_PMIC_RG_ACCDETSPARE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audencspareva30(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON21),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDENCSPAREVA30_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDENCSPAREVA30_SHIFT)
							);

}

void mt6337_upmu_set_rg_audencspareva18(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON21),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDENCSPAREVA18_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDENCSPAREVA18_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pllbs_rst(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLLBS_RST_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLLBS_RST_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_dcko_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_DCKO_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_DCKO_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_div1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_DIV1_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_DIV1_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_rlatch_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_RLATCH_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_RLATCH_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_pdiv1_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_PDIV1_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_PDIV1_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_pdiv1(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON22),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_PDIV1_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_PDIV1_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_bc(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_BC_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_BC_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_bp(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_BP_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_BP_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_br(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_BR_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_BR_SHIFT)
							);

}

void mt6337_upmu_set_rg_cko_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CKO_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_CKO_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_ibsel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_IBSEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_IBSEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_ckt_sel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_CKT_SEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_CKT_SEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_vct_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_VCT_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_VCT_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_ckt_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_CKT_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_CKT_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_hpm_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_HPM_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_HPM_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_dchp_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON23),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_DCHP_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_DCHP_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_cdiv(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_CDIV_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_CDIV_SHIFT)
							);

}

void mt6337_upmu_set_rg_vcoband(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_VCOBAND_MASK),
							(unsigned int)(MT6337_PMIC_RG_VCOBAND_SHIFT)
							);

}

void mt6337_upmu_set_rg_ckdrv_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_CKDRV_EN_MASK),
							(unsigned int)(MT6337_PMIC_RG_CKDRV_EN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_dchp_aen(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_DCHP_AEN_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_DCHP_AEN_SHIFT)
							);

}

void mt6337_upmu_set_rg_pll_rsva(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_AUDENC_ANA_CON24),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_PLL_RSVA_MASK),
							(unsigned int)(MT6337_PMIC_RG_PLL_RSVA_SHIFT)
							);

}

unsigned int mt6337_upmu_get_rgs_audrctunech0_01read(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUDENC_ANA_CON25),
							(&val),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH0_01READ_MASK),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH0_01READ_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rgs_audrctunech1_23read(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUDENC_ANA_CON25),
							(&val),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH1_23READ_MASK),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH1_23READ_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rgs_audrctunech2_45read(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUDENC_ANA_CON26),
							(&val),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH2_45READ_MASK),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH2_45READ_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rgs_audrctunech3_6read(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_AUDENC_ANA_CON26),
							(&val),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH3_6READ_MASK),
							(unsigned int)(MT6337_PMIC_RGS_AUDRCTUNECH3_6READ_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_rg_audzcdenable(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDENABLE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDENABLE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdgainsteptime(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDGAINSTEPTIME_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDGAINSTEPTIME_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdgainstepsize(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDGAINSTEPSIZE_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDGAINSTEPSIZE_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdtimeoutmodesel(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDTIMEOUTMODESEL_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDTIMEOUTMODESEL_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdclksel_vaudp15(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP15_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP15_SHIFT)
							);

}

void mt6337_upmu_set_rg_audzcdmuxsel_vaudp15(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP15_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP15_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlolgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLOLGAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audlorgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDLORGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDLORGAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhplgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPLGAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhprgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHPRGAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audhsgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON3),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDHSGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDHSGAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audivlgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIVLGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIVLGAIN_SHIFT)
							);

}

void mt6337_upmu_set_rg_audivrgain(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_ZCD_CON4),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_AUDIVRGAIN_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDIVRGAIN_SHIFT)
							);

}

unsigned int mt6337_upmu_get_rg_audintgain1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ZCD_CON5),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_AUDINTGAIN1_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDINTGAIN1_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_rg_audintgain2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_ZCD_CON5),
							(&val),
							(unsigned int)(MT6337_PMIC_RG_AUDINTGAIN2_MASK),
							(unsigned int)(MT6337_PMIC_RG_AUDINTGAIN2_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_gpio_dir0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_DIR0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO_DIR0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_DIR0_SHIFT)
							);

}

void mt6337_upmu_set_gpio_pullen0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_PULLEN0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO_PULLEN0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_PULLEN0_SHIFT)
							);

}

void mt6337_upmu_set_gpio_pullsel0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_PULLSEL0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO_PULLSEL0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_PULLSEL0_SHIFT)
							);

}

void mt6337_upmu_set_gpio_dinv0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_DINV0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO_DINV0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_DINV0_SHIFT)
							);

}

void mt6337_upmu_set_gpio_dout0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_DOUT0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO_DOUT0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_DOUT0_SHIFT)
							);

}

unsigned int mt6337_upmu_get_gpio_pi0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_GPIO_PI0),
							(&val),
							(unsigned int)(MT6337_PMIC_GPIO_PI0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_PI0_SHIFT)
						);


	return val;
}

unsigned int mt6337_upmu_get_gpio_poe0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;


	ret = mt6337_read_interface((unsigned int)(MT6337_GPIO_POE0),
							(&val),
							(unsigned int)(MT6337_PMIC_GPIO_POE0_MASK),
							(unsigned int)(MT6337_PMIC_GPIO_POE0_SHIFT)
						);


	return val;
}

void mt6337_upmu_set_gpio0_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO0_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO0_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio1_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO1_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO1_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio2_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO2_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO2_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio3_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO3_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO3_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio4_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO4_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO4_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio5_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO5_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO5_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio6_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO6_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO6_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio7_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO7_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO7_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio8_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO8_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO8_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio9_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE1),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO9_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO9_MODE_SHIFT)
							);

}

void mt6337_upmu_set_gpio10_mode(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_GPIO_MODE2),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_GPIO10_MODE_MASK),
							(unsigned int)(MT6337_PMIC_GPIO10_MODE_SHIFT)
							);

}

void mt6337_upmu_set_rg_rsv_con0(unsigned int val)
{
	unsigned int ret = 0;


	ret = mt6337_config_interface((unsigned int)(MT6337_RSV_CON0),
							(unsigned int)(val),
							(unsigned int)(MT6337_PMIC_RG_RSV_CON0_MASK),
							(unsigned int)(MT6337_PMIC_RG_RSV_CON0_SHIFT)
							);

}

